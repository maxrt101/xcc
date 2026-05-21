#include "xcc/ast/match.h"
#include "xcc/codegen.h"

using namespace xcc;
using namespace xcc::ast;

Match::Match(
  SourceSpan            span,
  std::shared_ptr<Node> value,
  std::vector<Arm>      arms
) : Node(AST_MATCH, span),
    value(std::move(value)),
    arms(std::move(arms)) {}

std::shared_ptr<Match> Match::create(
  SourceSpan            span,
  std::shared_ptr<Node> value,
  std::vector<Arm>      arms
) {
  return std::make_shared<Match>(span, std::move(value), std::move(arms));
}

std::shared_ptr<Node> Match::clone() {
  std::vector<Arm> arms_copy;

  for (auto& arm : arms) {
    Pattern p = {};

    for (auto& node : arm.pattern.nodes) {
      p.nodes.push_back(node->clone());
    }

    arms_copy.push_back({p, arm.then->clone()});
  }

  return withAttrs(create(span, value->clone(), arms_copy));
}

void Match::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {
  callVisitor(globalContext, value, visitor, ignoreSubtree);

  for (auto& arm : arms) {
    for (auto& node : arm.pattern.nodes) {
      callVisitor(globalContext, node, visitor, ignoreSubtree);
    }
    callVisitor(globalContext, arm.then, visitor, ignoreSubtree);
  }
}

std::string Match::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  std::string res = "match (" + value->toString(parent, this, 0, false) + ") {";

  if (newline) {
    res += "\n";
  }

  for (auto& arm : arms) {
    res += newline ? getIndent(indent + 1) : " ";

    for (size_t i = 0; i < arm.pattern.nodes.size(); ++i) {
      auto& node = arm.pattern.nodes[i];
      res += node->toString(parent, this, 0, false);
      if (i + 1 < arm.pattern.nodes.size()) {
        res += " | ";
      }
    }

    res += " -> " + arm.then->toString(parent, this, indent + 1, newline);

    if (!arm.then->is(AST_BLOCK)) {
      res += ",";
    }

    if (newline) {
      res += "\n";
    }
  }

  res += newline ? getIndent(indent) : " ";

  res += "}";

  if (newline) {
    res += "\n";
  }

  return res;
}

llvm::Value * Match::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  auto fn = ctx.ir_builder->GetInsertBlock()->getParent();

  size_t cases = 0;
  bool has_wildcard = false;

  for (auto& arm : arms) {
    // Count cases to hint LLVM in CreateSwitch
    cases += arm.pattern.nodes.size();

    // Check if only one wildcard is present
    for (auto& node : arm.pattern.nodes) {
      if (node->is(AST_EXPR_IDENTIFIER) && node->as<Identifier>()->value == "_") {
        if (has_wildcard) {
          Error(ERROR_MATCH_MULTIPLE_WILDCARDS, node->span)
            .note(findOrCreateDefault()->span, "Previous wildcard here:")
            .raiseFromNode(this);
        }

        has_wildcard = true;
      }
    }
  }

  // Create new lexical scope
  ctx.pushScope(span);

  // Create merge block for match result PHI
  auto * merge_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "match_merge", fn);

  // Generate common match type (common type for all arms)
  auto match_meta_type = generateType(ctx, payload);
  auto * match_llvm_type = match_meta_type->getLLVMType(ctx);

  // Accumulate resulting value of `arm.then` for all arms
  std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> phi_incoming;

  auto default_node = findOrCreateDefault();
  auto default_block = llvm::BasicBlock::Create(*ctx.llvm.ctx, "default_match_arm", fn);

  auto * switch_expr = ctx.ir_builder->CreateSwitch(
    value->generateValue(ctx, payload),
    default_block,
    cases
  );

  for (auto& arm : arms) {
    // If this arm's pattern contains a wildcard ('_')
    // arm.then is already generated in default_block
    auto block = arm.then == default_node
      ? default_block
      : llvm::BasicBlock::Create(*ctx.llvm.ctx, "match_arm", fn);;

    ctx.ir_builder->SetInsertPoint(block);

    auto * val = castIfNotSame(ctx, arm.then->generateValue(ctx, payload), match_llvm_type, arm.then->span);

    // Create terminator that jumps to common merge block,
    // which uses PHI to select result value
    if (!ctx.ir_builder->GetInsertBlock()->getTerminator()) {
      ctx.ir_builder->CreateBr(merge_block);
      phi_incoming.emplace_back(val, ctx.ir_builder->GetInsertBlock());
    }

    for (auto& node : arm.pattern.nodes) {
      if (node->is(AST_EXPR_IDENTIFIER) && node->as<Identifier>()->value == "_") {
        continue;
      }

      // Generate arm condition and attach it to switch_expr
      auto cond = llvm::dyn_cast<llvm::ConstantInt>(node->generateConstant(ctx, payload));
      assertRaiseFromNode(cond, Error(ERROR_MATCH_COND_NON_CONST_INT, node->span), this);
      switch_expr->addCase(cond, block);
    }
  }

  // If there is no '_' implicitly terminate default arm with default value for common type
  if (!default_block->getTerminator()) {
    ctx.ir_builder->SetInsertPoint(default_block);
    ctx.ir_builder->CreateBr(merge_block);
    phi_incoming.emplace_back(match_meta_type->getDefault(ctx), default_block);
  }

  // Converge on merge block
  ctx.ir_builder->SetInsertPoint(merge_block);
  ctx.popScope();

  // Use PHI node to decide result value from match arms
  auto * phi = ctx.ir_builder->CreatePHI(match_llvm_type, phi_incoming.size(), "match_result");

  for (auto& incoming : phi_incoming) {
    phi->addIncoming(incoming.first, incoming.second);
  }

  return phi;
}

std::shared_ptr<meta::Type> Match::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  std::shared_ptr<meta::Type> common_type;

  for (auto& arm : arms) {
    auto t = arm.then->generateType(ctx, payload);

    if (!common_type) {
      common_type = t;
    }

    if (*common_type != *t) {
      // TODO: Should at least try to align t & common_type?
      Error(ERROR_MATCH_BAD_ARM_TYPE, arm.then->span, "type='{}' common='{}'", t->toString(), common_type->toString())
        .note("All arms in match must evaluate to the same type")
        .raiseFromNode(this);
    }
  }

  return common_type;
}

std::shared_ptr<Node> Match::findOrCreateDefault() {
  for (auto& arm : arms) {
    for (auto& node : arm.pattern.nodes) {
      if (node->is(AST_EXPR_IDENTIFIER) && node->as<Identifier>()->value == "_") {
        return arm.then;
      }
    }
  }

  return Block::create(span, {});
}
