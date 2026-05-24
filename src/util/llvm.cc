#include "xcc/util/llvm.h"
#include "xcc/util/log.h"

#include <llvm/Target/TargetOptions.h>

static auto& logger = xcc::log::Logger::get("LLVM");

using namespace xcc::util;
using namespace xcc::meta;

Target::Target(const std::string& target_name, const std::string& machine_name) : target_triple(target_name) {
  std::string error;
  target = llvm::TargetRegistry::lookupTarget(target_name, error);

  if (!target) {
    logger.error("Failed to find target '{}': {}", target_name, error);
    throw std::runtime_error("Failed to find requested target");
  }

  llvm::TargetOptions opt;

  // TODO: Configurable PIC?
  machine = target->createTargetMachine(target_name, machine_name, "", opt, llvm::Reloc::PIC_);
}

GenericValueContainer::GenericValueContainer(Tag tag) : tag(tag) {}

GenericValueContainer::GenericValueContainer(Tag tag, Value value) : tag(tag), value(value) {}

GenericValueContainer GenericValueContainer::signedInt(int64_t value) {
  return GenericValueContainer(Tag::SIGNED_INTEGER, {.signed_integer = value});
}

GenericValueContainer GenericValueContainer::unsignedInt(uint64_t value) {
  return GenericValueContainer(Tag::UNSIGNED_INTEGER, {.unsigned_integer = value});
}

GenericValueContainer GenericValueContainer::floating(double value) {
  return GenericValueContainer(Tag::FLOATING, {.floating = value});
}

GenericValueContainer xcc::util::call(std::shared_ptr<xcc::meta::Type> type, llvm::orc::ExecutorSymbolDef symbol) {
  switch (type->getTag()) {
    case TypeTag::U8:   return GenericValueContainer::unsignedInt(call<uint8_t>(symbol));
    case TypeTag::U16:  return GenericValueContainer::unsignedInt(call<uint16_t>(symbol));
    case TypeTag::U32:  return GenericValueContainer::unsignedInt(call<uint32_t>(symbol));
    case TypeTag::U64:  return GenericValueContainer::unsignedInt(call<uint64_t>(symbol));
    case TypeTag::I8:   return GenericValueContainer::signedInt(call<int8_t>(symbol));
    case TypeTag::I16:  return GenericValueContainer::signedInt(call<int16_t>(symbol));
    case TypeTag::I32:  return GenericValueContainer::signedInt(call<int32_t>(symbol));
    case TypeTag::I64:  return GenericValueContainer::signedInt(call<int64_t>(symbol));
    case TypeTag::F32:  return GenericValueContainer::floating(call<float>(symbol));
    case TypeTag::F64:  return GenericValueContainer::floating(call<double>(symbol));
    case TypeTag::VOID:
    default:
      call<void>(symbol);
      return GenericValueContainer();
  }
}