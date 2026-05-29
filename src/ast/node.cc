#include "xcc/ast/node.h"
#include "xcc/codegen.h"
#include "xcc/util/log.h"
#include "xcc/exceptions.h"
#include <unordered_map>

using namespace xcc;
using namespace xcc::ast;

static auto& logger = xcc::log::Logger::get("AST_NODE");

#define DESC(__type, __name) {__type, {#__type, __name}}

struct NodeString {
  std::string type_name;
  std::string name;
};

static const std::unordered_map<NodeType, NodeString> s_type_map {
    DESC(AST_EXPR_ASSIGN,           "assignment"),
    DESC(AST_EXPR_BINARY,           "binary operation"),
    DESC(AST_BLOCK,                 "block"),
    DESC(AST_EXPR_CALL,             "call"),
    DESC(AST_EXPR_MACRO_CALL,       "macro call"),
    DESC(AST_EXPR_CAST,             "cast"),
    DESC(AST_FUNCTION_DECL,         "function declaration"),
    DESC(AST_FUNCTION_DEF,          "function definition"),
    DESC(AST_LAMBDA,                "lambda expression"),
    DESC(AST_TYPE_DECL,             "type declaration"),
    DESC(AST_FOR,                   "for statement"),
    DESC(AST_EXPR_IDENTIFIER,       "identifier"),
    DESC(AST_IF,                    "if statement"),
    DESC(AST_EXPR_MEMBER_ACCESS,    "member access"),
    DESC(AST_EXPR_NUMBER,           "number literal"),
    DESC(AST_RETURN,                "return statement"),
    DESC(AST_INIT,                  "initializer expression"),
    DESC(AST_EXPR_STRING,           "string literal"),
    DESC(AST_STRUCT,                "struct declaration"),
    DESC(AST_ENUM,                  "enum declaration"),
    DESC(AST_EXPR_SUBSCRIPT,        "subscript"),
    DESC(AST_EXPR_TYPE,             "type"),
    DESC(AST_EXPR_TYPED_IDENTIFIER, "typed identifier"),
    DESC(AST_EXPR_UNARY,            "unary operation"),
    DESC(AST_MOD,                   "module"),
    DESC(AST_MACRO,                 "macro declaration"),
    DESC(AST_MATCH,                 "match expression"),
    DESC(AST_VAR_DECL,              "variable declaration"),
    DESC(AST_CONST_DECL,            "constant declaration"),
    DESC(AST_DECOMPOSITION_DECL,    "value decomposition declaration"),
    DESC(AST_WHILE,                 "while statement"),
    DESC(AST_ASM,                   "inline assembly block"),
};

Node::Payload::Payload(NodeType type) : type(type) {}

static std::string formArgTypeList(const std::vector<NodeType>& arg_types) {
  std::string res;

  for (size_t i = 0; i < arg_types.size(); i++) {
    res += Node::typeToHumanReadableString(arg_types[i]);

    if (i + 1 < arg_types.size()) {
      res += ", ";
    }
  }

  return res;
}

void Node::Attribute::validateArgsStrict(const std::vector<NodeType>& arg_types) {
  assertRaise(args.size() == arg_types.size(),
    Error(ERROR_ATTR_ARG_COUNT_MISMATCH, span, "Attribute '{}' expected {} args, got {}", name, arg_types.size(), args.size()));

  for (size_t i = 0; i < arg_types.size(); ++i) {
    assertRaise(args[i]->is(arg_types[i]),
      Error(ERROR_ATTR_ARG_TYPE_MISMATCH, span, "Attribute '{}' expected {} as {} argument, got {}",
        name, typeToHumanReadableString(arg_types[i]), str::toStringWithOrdinalSuffix(i), typeToHumanReadableString(args[i]->type)));
  }
}

void Node::Attribute::validateArgs(const std::vector<std::vector<NodeType>>& arg_types) {
  assertRaise(args.size() == arg_types.size(),
    Error(ERROR_ATTR_ARG_COUNT_MISMATCH, span, "Attribute '{}' expected {} args, got {}", name, args.size(), arg_types.size()));

  for (size_t i = 0; i < arg_types.size(); ++i) {
    bool has = false;

    for (size_t j = 0; j < arg_types[i].size(); ++j) {
      if (args[i]->is(arg_types[i][j])) {
        has = true;
      }
    }

    assertRaise(has,
      Error(ERROR_ATTR_ARG_TYPE_MISMATCH, span, "Attribute '{}' expected any of {} as {} argument, got {}",
        name, formArgTypeList(arg_types[i]), str::toStringWithOrdinalSuffix(i), typeToHumanReadableString(args[i]->type)));
  }
}

Node::Node(NodeType type, SourceSpan span, LexicalScope scope) : type(type), span(span), scope(scope) {}

bool Node::isAnyOf(std::vector<NodeType> expected) const {
  for (auto& typ : expected) {
    if (is(typ)) {
      return true;
    }
  }

  return false;
}

Node::PayloadList Node::selectPayload(const PayloadList& payload) {
  return selectPayloadFor(payload, type);
}

std::shared_ptr<Node::Payload> Node::selectPayloadFirst(const PayloadList& payload) {
  return selectPayloadForFirst(payload, type);
}

Node::PayloadList Node::selectPayloadFor(const PayloadList& payload, NodeType type) {
  PayloadList result;

  for (const auto& element : payload) {
    if (element->type == type) {
      result.push_back(element);
    }
  }

  return result;
}

std::shared_ptr<Node::Payload> Node::selectPayloadForFirst(const PayloadList& payload, NodeType type) {
  for (const auto& element : payload) {
    if (element->type == type) {
      return element;
    }
  }

  return {};
}

Node::PayloadList Node::extendPayload(PayloadList list, std::shared_ptr<Payload> payload) {
  list.push_back(std::move(payload));
  return list;
}

Node::PayloadList Node::excludePayload(PayloadList list, NodeType type) {
  PayloadList result;

  for (auto& payload : list) {
    if (payload->type != type) {
      result.push_back(payload);
    }
  }

  return result;
}

void Node::addAttribute(const Attribute& attr) {
  attributes.push_back(attr);
}

bool Node::hasAttribute(const std::string& name) const {
  for (auto& attr : attributes) {
    if (attr.name == name) {
      return true;
    }
  }

  return false;
}

Node::Attribute& Node::getAttribute(const std::string& name) {
  for (auto& attr : attributes) {
    if (attr.name == name) {
      return attr;
    }
  }

  logger.error("Attribute '{}' is missing from {}", name, typeToString(type));
  throw std::runtime_error("Missing attribute");
}

const Node::Attribute& Node::getAttribute(const std::string& name) const {
  for (const auto& attr : attributes) {
    if (attr.name == name) {
      return attr;
    }
  }

  logger.error("Attribute '{}' is missing from {}", name, typeToString(type));
  throw std::runtime_error("Missing attribute");
}

std::vector<std::reference_wrapper<Node::Attribute>> Node::getAttributes(const std::string& name) {
  std::vector<std::reference_wrapper<Attribute>> attrs;

  for (auto& attr : attributes) {
    if (attr.name == name) {
      attrs.emplace_back(attr);
    }
  }

  return attrs;
}

std::vector<std::reference_wrapper<const Node::Attribute>> Node::getAttributes(const std::string& name) const {
  std::vector<std::reference_wrapper<const Attribute>> attrs;

  for (const auto& attr : attributes) {
    if (attr.name == name) {
      attrs.emplace_back(attr);
    }
  }

  return attrs;
}

llvm::Function * Node::generateFunction(codegen::ModuleContext& ctx, PayloadList payload) {
  logger.warn("Warning: Default Node::generateFunction is called on node with type '{}' ({})", Node::typeToString(type).c_str(), int(type));
  return nullptr;
}

llvm::Value * Node::generateValue(codegen::ModuleContext& ctx, PayloadList payload) {
  logger.warn("Warning: Default Node::generateValue is called on node with type '{}' ({})", Node::typeToString(type).c_str(), int(type));
  return nullptr;
}

llvm::Constant * Node::generateConstant(codegen::ModuleContext& ctx, PayloadList payload) {
  Error(ERROR_NOT_CONSTANT, span).raiseFromNode(this);
}

llvm::Value * Node::generateValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateValue(ctx, std::move(payload));
}

std::shared_ptr<meta::Type> Node::generateType(codegen::ModuleContext& ctx, PayloadList payload) {
  logger.warn("Warning: Default Node::generateType is called on node with type '{}' ({})", Node::typeToString(type).c_str(), int(type));
  return nullptr;
}

std::shared_ptr<meta::Type> Node::generateTypeForValueWithoutLoad(codegen::ModuleContext& ctx, PayloadList payload) {
  return generateType(ctx, std::move(payload));
}

std::string Node::typeToString(NodeType type) {
  if (s_type_map.find(type) != s_type_map.end()) {
    return s_type_map.at(type).type_name;
  }

  return "?";
}

std::string Node::typeToHumanReadableString(NodeType type) {
  if (s_type_map.find(type) != s_type_map.end()) {
    return s_type_map.at(type).name;
  }

  return "?";
}

std::string Node::getIndent(int indent) {
  std::string res;

  for (int i = 0; i < indent; ++i) {
    res += "  ";
  }

  return res;
}

std::string Node::attributesToString(int indent, bool newline) {
  if (attributes.empty()) {
    return "";
  }

  if (std::all_of(attributes.begin(), attributes.end(),
      [](auto& a) { return a.name == "__xcc_macro_expanded_from"; })
  ) {
    return "";
  }

  std::string res = "[";

  for (size_t i = 0; i < attributes.size(); ++i) {
    auto& attr = attributes[i];

    if (attr.name == "__xcc_macro_expanded_from") {
      continue;
    }

    res += attr.name;

    if (!attr.args.empty()) {
      res += "(";
      for (size_t j = 0; j < attr.args.size(); ++j) {
        res += attr.args[j]->toString(nullptr, this, indent, newline);
        if (j + 1 < attr.args.size()) {
          res += ", ";
        }
      }
      res += ")";
    }

    if (i + 1 < attributes.size()) {
      res += ", ";
    }
  }

  res += "]";

  if (newline) {
    res += "\n" + getIndent(indent);
  } else {
    res += " ";
  }

  return res;
}

std::string Node::getMangledName(const std::string& base_name) const {
  std::string mangled;

  for (const auto& s : scope) {
    mangled += s + "_";
  }

  return mangled + base_name;
}

std::string Node::resolveSymbolName(codegen::ModuleContext& ctx, const std::string& target_name) const {
  auto current_scope = scope;

  // Traverse scope list outward
  while (true) {
    std::string current_prefix;

    // Build prefix from scope
    for (const auto& s : current_scope) {
      current_prefix += s + "_";
    }

    auto search_name = current_prefix + target_name;

    search_name = ctx.globalContext.aliased(search_name);

    // Type/value exists - return current search_name
    if (meta::Type::hasCustomType(search_name) ||
       ctx.globalContext.getMetaFunction(search_name) ||
       ctx.globalContext.hasGlobal(search_name) ||
       ctx.globalContext.getConst(search_name) ||
       codegen::GenericsCache::has(search_name)
    ) {
      return search_name;
    }

    if (current_scope.empty()) break;

    // Otherwise - keep peeling innermost scope and trying again
    current_scope.pop_back();
  }

  // If the symbol can't be found, or scope is empty - return raw (with alias check)
  return ctx.globalContext.aliased(target_name);
}

std::string Node::defaultToString() {
  return toString(nullptr, nullptr, 0, false);
}

Empty::Empty() : Node(AST_EMPTY, {}, {}) {}

std::shared_ptr<Empty> Empty::create() {
  return std::make_shared<Empty>();
}

std::shared_ptr<Node> Empty::clone() {
  return withAttrs(create());
}

void Empty::visit(codegen::GlobalContext& globalContext, Visitor visitor, std::vector<NodeType> ignoreSubtree) {}

std::string Empty::toString(Node * grandparent, Node * parent, int indent, bool newline) {
  return "";
}
