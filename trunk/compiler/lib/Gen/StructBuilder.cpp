/* ================================================================ *
    TART - A Sweet Programming Language.
 * ================================================================ */

#include "tart/Gen/StructBuilder.h"
#include "tart/Gen/CodeGenerator.h"

#include "tart/CFG/TypeDefn.h"
#include "tart/CFG/VariableDefn.h"
#include "tart/CFG/CompositeType.h"

#include "tart/Objects/Builtins.h"

#include "tart/Common/Diagnostics.h"

namespace tart {

using namespace llvm;

StructBuilder::StructBuilder(CodeGenerator & gen) : gen_(gen) {}

StructBuilder & StructBuilder::createObjectHeader(const Type * type) {
  ConstantList objMembers;
  objMembers.push_back(gen_.getTypeInfoBlockPtr(cast<CompositeType>(type)));
  members_.push_back(ConstantStruct::get(gen_.context(), objMembers, false));
  return *this;
}

StructBuilder & StructBuilder::addField(llvm::Constant * value) {
  members_.push_back(value);
  return *this;
}

StructBuilder & StructBuilder::addNullField(const Type * type) {
  const llvm::PointerType * irType = cast<llvm::PointerType>(type->irEmbeddedType());
  return addField(ConstantPointerNull::get(irType));
}

StructBuilder & StructBuilder::addIntegerField(const Type * type, int32_t value) {
  members_.push_back(ConstantInt::get(cast<IntegerType>(type->irType()), value, true));
  return *this;
}

StructBuilder & StructBuilder::addIntegerField(VariableDefn * var, int32_t value) {
  return addIntegerField(var->type(), value);
}

StructBuilder & StructBuilder::addStringField(const std::string & strval) {
  return addField(gen_.genStringLiteral(strval));
}

StructBuilder & StructBuilder::addArrayField(
    const Type * elementType, const ConstantList & values) {
  const ArrayType * arrayType = ArrayType::get(elementType->irEmbeddedType(), values.size());
  return addField(ConstantArray::get(arrayType, values));
}

StructBuilder & StructBuilder::addArrayField(
    const VariableDefn * arrayVar, const ConstantList & values) {
  if (const CompositeType * arrayType = dyn_cast<CompositeType>(arrayVar->type())) {
    addArrayField(arrayType->typeParam(0), values);
  } else {
    DFAIL("Not an array type");
  }
}

StructBuilder & StructBuilder::combine() {
  Constant * c = ConstantStruct::get(gen_.context(), members_, false);
  members_.clear();
  members_.push_back(c);
  return *this;
}

llvm::Constant * StructBuilder::build() const {
  return ConstantStruct::get(gen_.context(), members_, false);
}

llvm::Constant * StructBuilder::build(const llvm::Type * expectedType) const {
  llvm::Constant * result = ConstantStruct::get(gen_.context(), members_, false);
  if (result->getType() != expectedType) {
    diag.error() << "Expected type does not match actual type:";
    expectedType->dump(gen_.irModule());
    result->getType()->dump(gen_.irModule());
    DFAIL("abort");
  }
  return result;
}

} // namespace tart
