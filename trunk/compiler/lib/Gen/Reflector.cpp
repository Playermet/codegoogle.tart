/* ================================================================ *
    TART - A Sweet Programming Language.
 * ================================================================ */

#include "tart/Gen/CodeGenerator.h"
#include "tart/Gen/StructBuilder.h"

#include "tart/Meta/VarInt.h"

#include "tart/Defn/Module.h"
#include "tart/Defn/Defn.h"
#include "tart/Defn/FunctionDefn.h"
#include "tart/Defn/NamespaceDefn.h"
#include "tart/Defn/PropertyDefn.h"
#include "tart/Defn/Template.h"
#include "tart/Defn/TypeDefn.h"

#include "tart/Type/CompositeType.h"
#include "tart/Type/PrimitiveType.h"
#include "tart/Type/FunctionType.h"
#include "tart/Type/EnumType.h"
#include "tart/Type/TupleType.h"
#include "tart/Type/NativeType.h"
#include "tart/Type/TypeLiteral.h"
#include "tart/Type/UnionType.h"

#include "tart/Objects/Builtins.h"
#include "tart/Objects/SystemDefs.h"

#include "tart/Common/Diagnostics.h"
#include "tart/Common/SourceFile.h"

#include "llvm/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/StringExtras.h"

#define DEBUG_VERBOSE 0

namespace tart {

using namespace llvm;

// -------------------------------------------------------------------

namespace reflect {
  // Members of tart.reflect.Module.
  namespace Module {
    SystemClassMember<VariableDefn> names(Builtins::typeModule, "_names");
    SystemClassMember<VariableDefn> nameIndex(Builtins::typeModule, "_nameIndex");
    SystemClassMember<VariableDefn> strmData(Builtins::typeModule, "_strmData");
    SystemClassMember<VariableDefn> typeRefs(Builtins::typeModule, "_typeRefs");
    SystemClassMember<VariableDefn> globalRefs(Builtins::typeModule, "_globalRefs");
    SystemClassMember<VariableDefn> methodRefs(Builtins::typeModule, "_methodRefs");
    SystemClassMember<VariableDefn> memberTypes(Builtins::typeModule, "_memberTypes");
    SystemClassMember<VariableDefn> methods(Builtins::typeModule, "_methods");
  }

  // Members of tart.reflect.Type
  namespace Type {
    SystemClassMember<VariableDefn> typeKind(Builtins::typeType, "_typeKind");
  }

  // Members of tart.reflect.CompositeType
  namespace CompositeType {
    SystemClassMember<VariableDefn> names(Builtins::typeCompositeType, "_names");
    SystemClassMember<VariableDefn> nameIndex(Builtins::typeCompositeType, "_nameIndex");
    SystemClassMember<VariableDefn> size(Builtins::typeCompositeType, "_size");
    SystemClassMember<VariableDefn> typeInfo(Builtins::typeCompositeType, "_typeInfo");
    SystemClassMember<VariableDefn> strmData(Builtins::typeCompositeType, "_strmData");
    SystemClassMember<VariableDefn> typeRefs(Builtins::typeCompositeType, "_typeRefs");
    SystemClassMember<VariableDefn> globalRefs(Builtins::typeCompositeType, "_globalRefs");
    SystemClassMember<VariableDefn> methodRefs(Builtins::typeCompositeType, "_methodRefs");
    SystemClassMember<VariableDefn> supertype(Builtins::typeCompositeType, "_supertype");
    SystemClassMember<VariableDefn> interfaces(Builtins::typeCompositeType, "_interfaces");
    SystemClassMember<VariableDefn> typeParams(Builtins::typeCompositeType, "_typeParams");
    SystemClassMember<VariableDefn> attributes(Builtins::typeCompositeType, "_attributes");
    SystemClassMember<VariableDefn> fields(Builtins::typeCompositeType, "_fields");
    SystemClassMember<VariableDefn> properties(Builtins::typeCompositeType, "_properties");
    SystemClassMember<VariableDefn> constructors(Builtins::typeCompositeType, "_constructors");
    SystemClassMember<VariableDefn> methods(Builtins::typeCompositeType, "_methods");
    SystemClassMember<VariableDefn> memberTypes(Builtins::typeCompositeType, "_memberTypes");
    SystemClassMember<VariableDefn> alloc(Builtins::typeCompositeType, "_alloc");
    SystemClassMember<VariableDefn> noArgCtor(Builtins::typeCompositeType, "_noArgCtor");
  }

  // Members of tart.reflect.EnumType
  namespace EnumType {
    SystemClassMember<VariableDefn> names(Builtins::typeEnumType, "_names");
    SystemClassMember<VariableDefn> nameIndex(Builtins::typeEnumType, "_nameIndex");
    SystemClassMember<VariableDefn> supertype(Builtins::typeEnumType, "_supertype");
    SystemClassMember<VariableDefn> values(Builtins::typeEnumType, "_values");
  }

  // Members of tart.reflect.DerivedType
  namespace DerivedType {
    SystemClassMember<VariableDefn> typeParams(Builtins::typeDerivedType, "_typeParams");
  }

  // Members of tart.reflect.NameTable.
  namespace NameTable {
    SystemClassMember<VariableDefn> nameStrmSimple(Builtins::typeNameTable, "_nameStrmSimple");
    SystemClassMember<VariableDefn> simpleNames(Builtins::typeNameTable, "_simpleNames");
    SystemClassMember<VariableDefn> compoundNames(Builtins::typeNameTable, "_compoundNames");
    SystemClassMember<VariableDefn> compoundNameStrings(
        Builtins::typeNameTable, "_compoundNameStrings");
  }

  // Members of tart.reflect.FunctionType
  namespace FunctionType {
    SystemClassMember<TypeDefn> CallAdapterFnType(Builtins::typeFunctionType, "CallAdapterFn");
    SystemClassMember<VariableDefn> returnType(Builtins::typeFunctionType, "_returnType");
    SystemClassMember<VariableDefn> selfType(Builtins::typeFunctionType, "_selfType");
    SystemClassMember<VariableDefn> paramTypes(Builtins::typeFunctionType, "_paramTypes");
    SystemClassMember<VariableDefn> invoke(Builtins::typeFunctionType, "_invoke");
  }

  namespace Method {
    SystemClassMember<VariableDefn> typeParams(Builtins::typeMethod, "_typeParams");
    SystemClassMember<VariableDefn> params(Builtins::typeMethod, "_params");
    SystemClassMember<VariableDefn> methodPointer(Builtins::typeMethod, "_methodPointer");
    SystemClassMember<VariableDefn> isConstructor(Builtins::typeMethod, "_isConstructor");
  }

  namespace Field {
    SystemClassMember<VariableDefn> offset(Builtins::typeField, "_offset");
    SystemClassMember<VariableDefn> addr(Builtins::typeField, "_addr");
  }

  namespace DataMember {
    SystemClassMember<VariableDefn> selfType(Builtins::typeDataMember, "_selfType");
  }

  // Members of tart.reflect.Member
  namespace Member {
    SystemClassMember<VariableDefn> names(Builtins::typeMember, "_names");
    SystemClassMember<VariableDefn> nameIndex(Builtins::typeMember, "_nameIndex");
    SystemClassMember<VariableDefn> kind(Builtins::typeMember, "_kind");
    SystemClassMember<VariableDefn> visibility(Builtins::typeMember, "_visibility");
    SystemClassMember<VariableDefn> traits(Builtins::typeMember, "_traits");
    SystemClassMember<VariableDefn> type(Builtins::typeMember, "_type");
    SystemClassMember<VariableDefn> attributes(Builtins::typeMember, "_attributes");

    SystemEnum VisbilityEnum(Builtins::typeMember, "Visibility");
    SystemEnum TraitsEnum(Builtins::typeMember, "Traits");

    namespace Visibility {
      SystemEnumConstant PUBLIC(VisbilityEnum, "PUBLIC");
      SystemEnumConstant PROTECTED(VisbilityEnum, "PUBLIC");
      SystemEnumConstant PRIVATE(VisbilityEnum, "PUBLIC");
    }

    namespace Traits {
      SystemEnumConstant FINAL(TraitsEnum, "FINAL");
      SystemEnumConstant ABSTRACT(TraitsEnum, "ABSTRACT");
      SystemEnumConstant STATIC(TraitsEnum, "STATIC");
      SystemEnumConstant UNSAFE(TraitsEnum, "UNSAFE");
      SystemEnumConstant OVERRIDE(TraitsEnum, "OVERRIDE");
    }
  }

  // Members of tart.reflect.StaticList
  namespace StaticList {
    SystemClassMember<VariableDefn> size(Builtins::typeStaticList, "_size");
  }

  // Members of tart.reflect.Package.
  namespace Package {
    SystemClassMember<VariableDefn> name(Builtins::typePackage, "_name");
    SystemClassMember<VariableDefn> modules(Builtins::typePackage, "_modules");
    SystemClassMember<VariableDefn> subpackages(Builtins::typePackage, "_subpackages");
  }
}

/// -------------------------------------------------------------------
/// Reflector

Reflector::Reflector(CodeGenerator & cg)
  : cg_(cg)
  , context_(cg.context())
  , builder_(cg.builder())
  , irModule_(cg.irModule())
  , nameTableVar_(NULL)
  , outputPhase_(false)
{
}

Reflector::~Reflector() {
  // TODO: Delete reflection metadata pointers.
}

llvm::Constant * Reflector::internSymbol(const StringRef &Key) {
  return cg_.genStringLiteral(Key, Key);
}

GlobalVariable * Reflector::getModulePtr(Module * module) {
  DASSERT(enabled_);

  Twine moduleSymbol(".module.", module->linkageName());
  if (GlobalVariable * var = findGlobal(moduleSymbol)) {
    return var;
  }

  return new GlobalVariable(*irModule_, Builtins::typeModule->irTypeComplete(), true,
      GlobalValue::ExternalLinkage, NULL, moduleSymbol);
}

GlobalVariable * Reflector::getNameTablePtr(Module * module) {
  DASSERT(enabled_);

  if (nameTableVar_ == NULL) {
    Twine nameTableSymbol(".names.", module->linkageName());
    nameTableVar_ = new GlobalVariable(*irModule_, Builtins::typeNameTable->irTypeComplete(),
        false, GlobalValue::ExternalLinkage, NULL, nameTableSymbol);
  }

  return nameTableVar_;
}

llvm::GlobalVariable * Reflector::getPackagePtr(Module * module) {
  Twine packageSymbol(".package.", module->packageName());
  if (GlobalVariable * var = findGlobal(packageSymbol)) {
    return var;
  }

  StructBuilder sb(cg_);
  sb.createObjectHeader(Builtins::typePackage);
  sb.addField(internSymbol(module->packageName()));
  sb.addField(emitArray("tart.reflect.Package.", reflect::Package::modules.get(), ConstantList()));
  sb.addField(emitArray("tart.reflect.Package.", reflect::Package::subpackages.get(),
      ConstantList()));

  return new GlobalVariable(*irModule_, Builtins::typePackage->irTypeComplete(), true,
      GlobalValue::LinkOnceAnyLinkage, sb.build(Builtins::typePackage), packageSymbol);
}

llvm::GlobalVariable * Reflector::getMethodPtr(const FunctionDefn * fn) {
  Twine symbol(".method.", fn->linkageName());
  if (GlobalVariable * var = findGlobal(symbol)) {
    return var;
  }

  if (outputPhase_) {
    diag.fatal() << "Attempting to add an export during output phase: " << fn;
  }

  getTypePtr(fn->type().unqualified());

  DASSERT_OBJ(module()->exportDefs().count(const_cast<FunctionDefn *>(fn)) > 0, fn);

  defnExports_.append(fn);
  return new GlobalVariable(*irModule_, Builtins::typeMethod->irTypeComplete(), true,
      fn->isSynthetic() ? GlobalValue::LinkOnceODRLinkage : GlobalValue::ExternalLinkage,
      NULL, symbol);
}

llvm::GlobalVariable * Reflector::getPropertyPtr(const PropertyDefn * prop) {
  Twine symbol(".property.", prop->linkageName());
  if (GlobalVariable * var = findGlobal(symbol)) {
    return var;
  }

  if (outputPhase_) {
    diag.fatal() << "Attempting to add an export during output phase: " << prop;
  }

  getTypePtr(prop->type().unqualified());

  defnExports_.append(prop);
  return new GlobalVariable(*irModule_, Builtins::typeProperty->irTypeComplete(), true,
      prop->isSynthetic() ? GlobalValue::LinkOnceODRLinkage : GlobalValue::ExternalLinkage,
      NULL, symbol);
}

llvm::GlobalVariable * Reflector::getFieldPtr(const VariableDefn * field) {
  Twine symbol(".field.", field->linkageName());
  if (GlobalVariable * var = findGlobal(symbol)) {
    return var;
  }

  if (outputPhase_) {
    diag.fatal() << "Attempting to add an export during output phase: " << field;
  }

  getTypePtr(field->type().unqualified());

  defnExports_.append(field);
  return new GlobalVariable(*irModule_, Builtins::typeField->irTypeComplete(), true,
      field->isSynthetic() ? GlobalValue::LinkOnceODRLinkage : GlobalValue::ExternalLinkage,
      NULL, symbol);
}

llvm::Constant * Reflector::getTypePtr(const Type * type) {
  switch (type->typeClass()) {
    case Type::Primitive:
      return cg_.getPrimitiveTypeObjectPtr(static_cast<const PrimitiveType *>(type));

    case Type::Class:
    case Type::Struct:
    case Type::Interface:
    case Type::Protocol:
      return getCompositeTypePtr(static_cast<const CompositeType *>(type));

    case Type::Enum:
      return getEnumTypePtr(static_cast<const EnumType *>(type));

    case Type::Tuple:
    case Type::Union:
    case Type::NAddress:
      return getDerivedTypePtr(type);

    case Type::Function:
      return getFunctionTypePtr(static_cast<const FunctionType *>(type));

    case Type::NArray:
      diag.fatal() << "Attempt to get type pointer for unimplemented type: " << type;
      DFAIL("Implement");
      break;

    default:
      diag.fatal() << "Attempt to get type pointer for non-supported type: " << type;
      break;
  }

  return NULL;
}

llvm::GlobalVariable * Reflector::getCompositeTypePtr(const CompositeType * type) {
  Twine typeSymbol(".compositeType.", type->typeDefn()->linkageName());
  if (GlobalVariable * var = findGlobal(typeSymbol)) {
    return var;
  }

  if (!isExport(type->typeDefn())) {
    return new GlobalVariable(*irModule_, Builtins::typeCompositeType->irTypeComplete(), true,
        GlobalValue::ExternalLinkage, NULL, typeSymbol);
  }

  if (outputPhase_) {
    diag.fatal() << "Attempting to add a type export during output phase: " << type;
  }
  typeExports_.append(type);
  return new GlobalVariable(*irModule_, Builtins::typeCompositeType->irTypeComplete(), true,
      type->typeDefn()->isSynthetic()
          ? GlobalValue::LinkOnceAnyLinkage
          : GlobalValue::ExternalLinkage,
      NULL, typeSymbol);
}

llvm::GlobalVariable * Reflector::getEnumTypePtr(const EnumType * type) {
  Twine typeSymbol(".enumType.", type->typeDefn()->linkageName());
  if (GlobalVariable * var = findGlobal(typeSymbol)) {
    return var;
  }

  if (!isExport(type->typeDefn())) {
    return new GlobalVariable(*irModule_, Builtins::typeEnumType->irTypeComplete(), true,
        GlobalValue::ExternalLinkage, NULL, typeSymbol);
  }

  if (outputPhase_) {
    diag.fatal() << "Attempting to add a type export during output phase: " << type;
  }
  typeExports_.append(type);
  return new GlobalVariable(*irModule_, Builtins::typeEnumType->irTypeComplete(), true,
      type->typeDefn()->isSynthetic()
          ? GlobalValue::LinkOnceAnyLinkage
          : GlobalValue::ExternalLinkage,
      NULL, typeSymbol);
}

llvm::GlobalVariable * Reflector::getDerivedTypePtr(const Type * type) {
  llvm::SmallString<64> typeSymbol(".type.");
  typeLinkageName(typeSymbol, type);
  GlobalVariable * var = irModule_->getGlobalVariable(typeSymbol, true);
  if (var != NULL) {
    return var;
  }

  if (outputPhase_) {
    diag.fatal() << "Attempting to add a type export during output phase: " << type;
  }
  typeExports_.append(type);

  for (size_t i = 0; i < type->numTypeParams(); ++i) {
    getTypePtr(type->typeParam(i).type());
  }

  return new GlobalVariable(*irModule_, Builtins::typeDerivedType->irTypeComplete(), true,
      GlobalValue::LinkOnceODRLinkage,
      NULL, Twine(typeSymbol));
}

llvm::GlobalVariable * Reflector::getFunctionTypePtr(const FunctionType * type) {
  llvm::SmallString<64> typeSymbol(".type.");
  typeLinkageName(typeSymbol, type);
  GlobalVariable * var = irModule_->getGlobalVariable(typeSymbol, true);
  if (var != NULL) {
    return var;
  }

  if (outputPhase_) {
    diag.fatal() << "Attempting to add a type export during output phase: " << type;
  }

  getTypePtr(type->returnType().unqualified());
  for (ParameterList::const_iterator it = type->params().begin();
      it != type->params().end(); ++it) {
    getTypePtr((*it)->type().unqualified());
  }

  typeExports_.append(type);
  return new GlobalVariable(*irModule_, Builtins::typeFunctionType->irTypeComplete(), true,
      GlobalValue::LinkOnceODRLinkage,
      NULL, Twine(typeSymbol));
}

void Reflector::emitModule(Module * module) {
  DASSERT(enabled_);

  NameTable & nameTable = cg_.nameTable();
  const DefnSet & reflectedDefs = module->reflectedDefs();

  Builtins::typeModule->createIRTypeFields();
  Builtins::typeEnumType->createIRTypeFields();

  // See if there are any reflected defns.
  bool hasReflectedDefns = false;
  if (!reflectedDefs.empty()) {
    hasReflectedDefns = true;
  }

  if (hasReflectedDefns) {
    GlobalVariable * modulePtr = getModulePtr(module);
    if (!modulePtr->hasInitializer()) {
      llvm::SmallString<128> moduleName = module->qualifiedName();
      moduleName += "#module";

      // First visit members which are explicitly declared in this module.
      for (DefnSet::const_iterator it = reflectedDefs.begin(); it != reflectedDefs.end(); ++it) {
        getRefs(*it);
      }

      NameTable::Name * qualifiedName = addQualifiedName(module->qualifiedName());
      DASSERT(qualifiedName != NULL);

      nameTable.assignIndices();

      // Generate the Module struct
      StructBuilder sb(cg_);
      sb.createObjectHeader(Builtins::typeModule);

      // Module._nameTable. Module._nameIndex
      GlobalVariable * nameTableVar = getNameTablePtr(cg_.module());
      sb.addField(nameTableVar);
      sb.addIntegerField(reflect::Module::nameIndex, qualifiedName->encodedIndex());

      // Module._memberTypes, Module._methods
      sb.addPointerField(reflect::Module::memberTypes, emitMemberTypes(module));
      sb.addPointerField(reflect::Module::methods, emitMethodList(module, false, moduleName));

      modulePtr->setInitializer(sb.build(Builtins::typeModule));
    }
  } else {
    nameTable.assignIndices();
  }

  outputPhase_ = true;

  emitNameTable(module);

  while (!typeExports_.empty() || !defnExports_.empty()) {
    while (!typeExports_.empty()) {
      emitType(typeExports_.next());
    }

    while (!defnExports_.empty()) {
      emitDefn(defnExports_.next());
    }
  }
}

void Reflector::emitNameTable(Module * module) {
  GlobalVariable * nameTablePtr = getNameTablePtr(module);
  if (!nameTablePtr->hasInitializer()) {
    NameTable & nameTable = cg_.nameTable();

    // Generate the table of encoded strings.
    std::string encodedStringData;
    raw_string_ostream encodedStringStream(encodedStringData);
    nameTable.writeStringTable(encodedStringStream);

    // Generate the table of encoded compound names.
    std::string encodedNamesData;
    raw_string_ostream encodedNamesStream(encodedNamesData);
    nameTable.writeCompoundNameTable(encodedNamesStream);

    // Generate the module constants structure
    StructBuilder sb(cg_);
    sb.createObjectHeader(Builtins::typeNameTable);

    // Write out encoded string data stream
    if (!encodedStringData.empty()) {
      Twine encodedStringsSymbol(".names_simple.", module->linkageName());
      Constant * encodedStrings = ConstantArray::get(context_, encodedStringData, false);
      GlobalVariable * encodedStringsVar = new GlobalVariable(*irModule_, encodedStrings->getType(),
          true, GlobalValue::InternalLinkage, encodedStrings, encodedStringsSymbol);
      sb.addField(llvm::ConstantExpr::getPointerCast(encodedStringsVar, builder_.getInt8PtrTy()));
    } else {
      sb.addNullField(reflect::NameTable::nameStrmSimple);
    }

    // Write out encoded compound name stream
    if (!encodedNamesData.empty()) {
      if (encodedNamesData.size() == 1 && encodedNamesData[0] == 0) {
        StringRef encodedNamesSymbol(".names_compound.$empty");
        Constant * encodedNames = ConstantArray::get(context_, encodedNamesData, false);
        GlobalVariable * encodedNamesVar = new GlobalVariable(*irModule_, encodedNames->getType(),
            true, GlobalValue::LinkOnceODRLinkage, encodedNames, encodedNamesSymbol);
        sb.addField(llvm::ConstantExpr::getPointerCast(encodedNamesVar, builder_.getInt8PtrTy()));
      } else {
        Twine encodedNamesSymbol(".names_compound.", module->linkageName());
        Constant * encodedNames = ConstantArray::get(context_, encodedNamesData, false);
        GlobalVariable * encodedNamesVar = new GlobalVariable(*irModule_, encodedNames->getType(),
            true, GlobalValue::InternalLinkage, encodedNames, encodedNamesSymbol);

        sb.addField(llvm::ConstantExpr::getPointerCast(encodedNamesVar, builder_.getInt8PtrTy()));
      }
    } else {
      sb.addNullField(reflect::NameTable::nameStrmSimple);
    }

    sb.addNullField(reflect::NameTable::simpleNames);
    sb.addNullField(reflect::NameTable::compoundNames);
    sb.addNullField(reflect::NameTable::compoundNameStrings);

    nameTablePtr->setInitializer(sb.build(Builtins::typeNameTable));
    cg_.addStaticRoot(nameTablePtr, Builtins::typeNameTable);
  }
}

void Reflector::getRefs(const Defn * def) {
  DASSERT(isExport(def));

#if 0
  diag.debug() << "Adding metadata for def " << def;
#endif

  addQualifiedName(def->qualifiedName())->use();

  // Add all of the members of this definition
  switch (def->defnType()) {
    case Defn::Typedef: {
      const TypeDefn * td = static_cast<const TypeDefn *>(def);
      const Type * type = td->value().type();
      getTypePtr(type);
      if (const CompositeType * ctype = dyn_cast<CompositeType>(type)) {
        for (ClassList::const_iterator it = ctype->bases().begin();
            it != ctype->bases().end(); ++it) {
          getTypePtr(*it);
        }

        if (td->isReflected()) {
          for (size_t i = 0; i < ctype->numTypeParams(); ++i) {
            getTypePtr(ctype->typeParam(i).unqualified());
          }
        }

        // Get refs for instance fields
        for (const Defn * m = ctype->memberScope()->firstMember(); m != NULL;
            m = m->nextInScope()) {
          if (m->isReflected() && m->storageClass() == Storage_Instance) {
            if (const VariableDefn * var = dyn_cast<VariableDefn>(m)) {
              getRefs(var);
            }
          }
        }

        //getRefsForMembers(ctype->memberScope());
      }

      switch (type->typeClass()) {
        case Type::Enum: {
          getTypePtr(type);
          //const EnumType * etype = static_cast<const EnumType *>(type);
#if DEBUG_VERBOSE
          diag.debug() << "Emitting metadata for enum " << etype;
          diag.indent();
#endif
#if DEBUG_VERBOSE
          diag.unindent();
#endif
          break;
        }

        case Type::Alias: {
          DASSERT_OBJ(false, def);
          break;
        }

        default: {
          break;
        }
      }
      break;
    }

    case Defn::Namespace: {
      //const NamespaceDefn * ns = static_cast<const NamespaceDefn *>(def);
#if DEBUG_VERBOSE
      diag.debug() << "Emitting metadata for namespace " << def;
      diag.indent();
#endif
      //getRefsForMembers(&ns->memberScope());
#if DEBUG_VERBOSE
      diag.unindent();
#endif
      break;
    }

    case Defn::Var:
    case Defn::Let: {
      const VariableDefn * v = static_cast<const VariableDefn *>(def);
      if (v->type()->typeClass() == Type::NArray ||
          v->type()->typeClass() == Type::FlexibleArray) {
        break;
      }
      addName(v->name())->use();
      getFieldPtr(v);
#if DEBUG_VERBOSE
      diag.debug() << "Emitting metadata for var " << def;
#endif
      break;
    }

    case Defn::Property:
    case Defn::Indexer: {
      const PropertyDefn * prop = static_cast<const PropertyDefn *>(def);
      addName(prop->name())->use();
      getPropertyPtr(prop);
#if DEBUG_VERBOSE
      diag.debug() << "Emitting metadata for property " << def;
#endif
      break;
    }

    case Defn::Function: {
      // Note: Macros are not reflected.
      const FunctionDefn * fn = static_cast<const FunctionDefn *>(def);
      const FunctionType * fnType = fn->functionType();
      if (!fn->isIntrinsic() && fn->isReflected() &&
          fn->isSingular() && fn->functionType() != NULL) {
        addName(fn->name())->use();
        getMethodPtr(fn);
        for (ParameterList::const_iterator it = fnType->params().begin();
            it != fnType->params().end(); ++it) {
          addName((*it)->name())->use();
        }

#if DEBUG_VERBOSE
        diag.debug() << "Emitting metadata for method " << def;
#endif
      }
      break;
    }

    default: {
      break;
    }
  }
}

void Reflector::emitDefn(const Defn * def) {
  if (const FunctionDefn * fn = dyn_cast<FunctionDefn>(def)) {
    emitMethod(fn);
  } else if (const VariableDefn * var = dyn_cast<VariableDefn>(def)) {
    emitField(var);
  } else if (const PropertyDefn * prop = dyn_cast<PropertyDefn>(def)) {
    emitProperty(prop);
  }
}

void Reflector::emitMethod(const FunctionDefn * fn) {
  GlobalVariable * var = getMethodPtr(fn);
  if (var->hasInitializer()) {
    return;
  }

  // Generate the Member instance
  StructBuilder sb(cg_);
  sb.addField(emitMember(fn, Builtins::typeMethod, fn->qualifiedName()));

  // Method._typeParams
  if (fn->isReflected() && fn->isTemplateInstance()) {
    const TupleType * typeArgs = fn->templateInstance()->typeArgs();
    sb.addPointerField(reflect::Method::typeParams, emitTypeList(typeArgs->members()));
  } else {
    sb.addPointerField(reflect::Method::typeParams, emitTypeList(QualifiedTypeList()));
  }

  // Method._params
  // TODO - implement
  sb.addNullField(reflect::Method::params);

  // Method._methodPointer
  sb.addPointerField(reflect::Method::methodPointer, cg_.genCallableDefn(fn));

  // Method._isConstructor
  sb.addIntegerField(reflect::Method::isConstructor, fn->isCtor() ? 1 : 0);

  var->setInitializer(sb.build(Builtins::typeMethod));
}

void Reflector::emitProperty(const PropertyDefn * prop) {

}

void Reflector::emitField(const VariableDefn * field) {
  GlobalVariable * var = getFieldPtr(field);
  if (var->hasInitializer()) {
    return;
  }

  // Generate the Member instance
  StructBuilder sb(cg_);
  sb.addField(emitMember(field, Builtins::typeField, field->qualifiedName()));

  // DataMember._selfType
  sb.addNullField(reflect::DataMember::selfType);
  sb.combine(Builtins::typeDataMember);

  // Field._offset, Field._addr
  if (field->storageClass() == Storage_Instance) {
    const CompositeType * cls = field->definingClass();
    StructType * clsType = cast<StructType>(cls->irTypeComplete());
    sb.addField(
        llvm::ConstantExpr::getIntegerCast(
            llvm::ConstantExpr::getOffsetOf(clsType, field->memberIndex()),
            reflect::Field::offset.type()->irType(),
            false));
    sb.addNullField(reflect::Field::addr);
  } else {
    sb.addIntegerField(reflect::Field::offset, 0);
    sb.addPointerField(reflect::Field::addr, cg_.genGlobalVar(field));
  }

  var->setInitializer(sb.build(Builtins::typeField));
}

llvm::Constant * Reflector::emitMember(const ValueDefn * member,
    const CompositeType * memberType, StringRef name) {
  GlobalVariable * nameTableVar = getNameTablePtr(cg_.module());

  // Generate the Member instance
  StructBuilder sb(cg_);
  sb.createObjectHeader(memberType);

  // Member._names, Member._nameIndex
  sb.addField(nameTableVar);
  NameTable::Name * nameRef = cg_.nameTable().getName(member->name());
  sb.addIntegerField(reflect::Member::nameIndex, nameRef ? nameRef->encodedIndex() : 0);

  // Member._visibility
  int visibility = 0;
  switch (member->visibility()) {
    case PUBLIC: visibility = reflect::Member::Visibility::PUBLIC.asInt(); break;
    case PRIVATE: visibility = reflect::Member::Visibility::PRIVATE.asInt(); break;
    case PROTECTED: visibility = reflect::Member::Visibility::PROTECTED.asInt(); break;
    default:
      break;
  }
  sb.addIntegerField(reflect::Member::visibility, visibility);

  // Member._traits
  int traits = 0;
  sb.addIntegerField(reflect::Member::traits, traits);

  // Member._type
  sb.addPointerField(reflect::Member::type, getTypePtr(member->type().type()));

  // Member._attributes
  sb.addPointerField(reflect::Member::attributes, emitAttributeList(member->attrs(), name));

  return sb.build(Builtins::typeMember);
}

void Reflector::emitType(const Type * type) {
  switch (type->typeClass()) {
    case Type::Primitive:
      diag.fatal() << "Attempt to emit type pointer for primtitive type: " << type;
      break;

    case Type::Class:
    case Type::Struct:
    case Type::Interface:
    case Type::Protocol:
      emitCompositeType(static_cast<const CompositeType *>(type));
      break;

    case Type::Enum:
      emitEnumType(static_cast<const EnumType *>(type));
      break;

    case Type::Tuple:
    case Type::Union:
    case Type::NAddress:
      emitDerivedType(type);
      break;

    case Type::Function:
      emitFunctionType(static_cast<const FunctionType *>(type));
      break;

    case Type::NArray:
      DFAIL("Implement");
      break;

    default:
      diag.fatal() << "Attempt to emit type pointer for unsupported type: " << type;
      break;
  }
}

void Reflector::emitCompositeType(const CompositeType * type) {
  GlobalVariable * var = getCompositeTypePtr(type);
  if (var->hasInitializer()) {
    return;
  }

  TypeDefn * td = type->typeDefn();
  GlobalVariable * nameTableVar = getNameTablePtr(cg_.module());

  // Generate the CompositeType instance
  StructBuilder sb(cg_);
  sb.createObjectHeader(Builtins::typeCompositeType);
  sb.addIntegerField(reflect::Type::typeKind.type(), typeKind(type->typeClass()));
  sb.combine(Builtins::typeType);

  // CompositeType._names
  sb.addField(nameTableVar);
  NameTable::Name * qname = cg_.nameTable().getQualifiedName(td->qualifiedName());
  sb.addIntegerField(reflect::CompositeType::nameIndex, qname ? qname->encodedIndex() : 0);

  // CompositeType._size
  if (type->isSingular()) {
    sb.addField(
        llvm::ConstantExpr::getIntegerCast(
            llvm::ConstantExpr::getSizeOf(type->irTypeComplete()),
            reflect::CompositeType::size.type()->irType(),
            false));
  } else {
    sb.addIntegerField(reflect::CompositeType::size.type(), 0);
  }

  // CompositeType._typeInfo
  sb.addField(cg_.getTypeInfoBlockPtr(type));

  // CompositeType._supertype
  if (type->super() != NULL) {
    sb.addField(getCompositeTypePtr(type->super()));
  } else {
    sb.addNullField(reflect::CompositeType::supertype.type());
  }

  // CompositeType._interfaces
  QualifiedTypeList interfaceList;
  for (ClassList::const_iterator it = type->bases().begin(); it != type->bases().end(); ++it) {
    if ((*it)->typeClass() == Type::Interface) {
      interfaceList.push_back(*it);
    }
  }
  sb.addPointerField(reflect::CompositeType::interfaces, emitTypeList(interfaceList));

  // CompositeType._typeParams
  QualifiedTypeList typeParamList;
  if (td->isReflected()) {
    type->getTypeParams(typeParamList);
  }
  sb.addPointerField(reflect::CompositeType::typeParams, emitTypeList(typeParamList));

  // CompositeType._attributes
  sb.addPointerField(reflect::CompositeType::attributes, emitAttributeList(td->attrs(),
      td->qualifiedName()));

  // CompositeType._fields, _properties, _constructors, _methods
  sb.addPointerField(reflect::CompositeType::fields, emitFieldList(type->memberScope(),
      td->qualifiedName()));
  sb.addPointerField(reflect::CompositeType::properties, emitPropertList(type->memberScope(),
      td->qualifiedName()));
  sb.addPointerField(reflect::CompositeType::constructors, emitMethodList(type->memberScope(),
      true, td->qualifiedName()));
  sb.addPointerField(reflect::CompositeType::methods, emitMethodList(type->memberScope(),
      false, td->qualifiedName()));

  // CompositeType._memberTypes
  if (td->isReflected()) {
    sb.addPointerField(reflect::CompositeType::memberTypes, emitMemberTypes(type->memberScope()));
  } else {
    sb.addNullField(reflect::CompositeType::memberTypes.type());
  }

  // CompositeType._noArgCtor
  FunctionDefn * noArgCtor = NULL;
  if (type->typeClass() == Type::Class ||
      (type->typeClass() == Type::Struct && td->isReflected())) {
    noArgCtor = type->noArgConstructor();
  }
  if (noArgCtor != NULL && noArgCtor->hasBody() && !noArgCtor->isUndefined()) {
    sb.addPointerField(reflect::CompositeType::noArgCtor, cg_.genFunctionValue(noArgCtor));
  } else {
    sb.addNullField(reflect::CompositeType::noArgCtor.type());
  }

  var->setInitializer(sb.build(Builtins::typeCompositeType));
}

void Reflector::emitEnumType(const EnumType * type) {
  GlobalVariable * var = getEnumTypePtr(type);
  if (var->hasInitializer()) {
    return;
  }

  //diag.debug() << "Emitting composite type: " << Format_Verbose << type;

  TypeDefn * td = type->typeDefn();
  GlobalVariable * nameTableVar = getNameTablePtr(cg_.module());

  // Generate the EnumType instance
  StructBuilder sb(cg_);
  sb.createObjectHeader(Builtins::typeEnumType);
  sb.addIntegerField(reflect::Type::typeKind.type(), ENUM);
  sb.combine(Builtins::typeType);

  // EnumType._names
  sb.addField(nameTableVar);
  NameTable::Name * qname = cg_.nameTable().getQualifiedName(td->qualifiedName());
  sb.addIntegerField(reflect::EnumType::nameIndex, qname ? qname->encodedIndex() : 0);

  // EnumType._supertype
  sb.addNullField(reflect::EnumType::supertype.type());
  sb.addNullField(reflect::EnumType::values.type());

  var->setInitializer(sb.build(Builtins::typeEnumType));
}

void Reflector::emitDerivedType(const Type * type) {
  GlobalVariable * var = getDerivedTypePtr(type);
  if (var->hasInitializer()) {
    return;
  }

  // Generate the DerivedType instance
  StructBuilder sb(cg_);
  sb.createObjectHeader(Builtins::typeDerivedType);
  sb.addIntegerField(reflect::Type::typeKind.type(), typeKind(type->typeClass()));
  sb.combine(Builtins::typeType);

  QualifiedTypeList typeParamList;
  type->getTypeParams(typeParamList);
  sb.addPointerField(reflect::DerivedType::typeParams, emitTypeList(typeParamList));

  var->setInitializer(sb.build(Builtins::typeDerivedType));
}

void Reflector::emitFunctionType(const FunctionType * type) {
  GlobalVariable * var = getFunctionTypePtr(type);
  if (var->hasInitializer()) {
    return;
  }

  // Union return values temporarily disabled for the moment.
  bool canInvoke = false;
  const Type * selfType = type->selfParam() != NULL ? type->selfParam()->type().type() : NULL;
  const Type * returnType = type->returnType().type();
  if (type->isInvocable() &&
      returnType->typeClass() != Type::Union &&
      (selfType == NULL ||
          selfType->typeClass() == Type::Class ||
          selfType->typeClass() == Type::Interface)) {
    canInvoke = true;
  }

  // Generate the FunctionType instance
  StructBuilder sb(cg_);
  sb.createObjectHeader(Builtins::typeFunctionType);
  sb.addIntegerField(reflect::Type::typeKind.type(), typeKind(type->typeClass()));
  sb.combine(Builtins::typeType);

  // FunctionType._returnType
  sb.addPointerField(reflect::FunctionType::returnType, getTypePtr(type->returnType().type()));

  // FunctionType._selfType
  if (type->selfParam() != NULL) {
    sb.addPointerField(reflect::FunctionType::selfType, getTypePtr(type->selfParam()->type().type()));
  } else {
    sb.addNullField(reflect::FunctionType::selfType.type());
  }

  // FunctionType._paramTypes
  sb.addPointerField(reflect::FunctionType::paramTypes,
      emitTypeList(type->paramTypes()->members()));

  // FunctionType._invoke
  if (canInvoke) {
    sb.addField(cg_.genCallAdapterFn(type));
  } else {
    sb.addNullField(reflect::FunctionType::invoke.type());
  }

  var->setInitializer(sb.build(Builtins::typeFunctionType));
}

llvm::Constant * Reflector::emitMemberTypes(const IterableScope * scope) {
  QualifiedTypeList memberTypes;

  for (Defn * de = scope->firstMember(); de != NULL; de = de->nextInScope()) {
    if (de->isReflected() && isExport(de) && de->isSingular() && de->defnType() == Defn::Typedef) {
      memberTypes.push_back(static_cast<const TypeDefn *>(de)->value().type());
    }
  }

  return emitTypeList(memberTypes);
}

llvm::Constant * Reflector::emitAttributeList(const ExprList & attrs, StringRef name) {
  llvm::Type * objType = Builtins::typeObject->irEmbeddedType();
  ConstantList retainedAttrs;
  for (ExprList::const_iterator it = attrs.begin(); it != attrs.end(); ++it) {
    llvm::Constant * retainedAttr = getRetainedAttr(*it, name);
    if (retainedAttr != NULL) {
      retainedAttrs.push_back(
          llvm::ConstantExpr::getPointerCast(retainedAttr, objType));
    }
  }

  return emitStaticList(retainedAttrs, ".attrs.", name,
      Builtins::typeAttributeList, Builtins::typeObject);
}

llvm::Constant * Reflector::emitMethodList(const IterableScope * scope, bool ctors,
    StringRef name) {
  ConstantList methods;

  for (Defn * de = scope->firstMember(); de != NULL; de = de->nextInScope()) {
    if (de->isReflected() && isExport(de) && de->isSingular() && de->defnType() == Defn::Function) {
      const FunctionDefn * fn = static_cast<const FunctionDefn *>(de);
      if (fn->isCtor() == ctors) {
        methods.push_back(getMethodPtr(fn));
      }
    }
  }

  return emitStaticList(methods, ctors ? ".constructors." : ".methods.", name,
      Builtins::typeMethodList, Builtins::typeMethod);
}

llvm::Constant * Reflector::emitPropertList(const IterableScope * scope, StringRef name) {
  ConstantList properties;

  for (Defn * de = scope->firstMember(); de != NULL; de = de->nextInScope()) {
    if (de->isReflected() && isExport(de) && de->isSingular()) {
      if (const PropertyDefn * prop = dyn_cast<PropertyDefn>(de)) {
        properties.push_back(getPropertyPtr(prop));
      }
    }
  }

  return emitStaticList(properties, ".properties.", name,
      Builtins::typePropertyList, Builtins::typeProperty);
}

llvm::Constant * Reflector::emitFieldList(const IterableScope * scope, StringRef name) {
  ConstantList fields;

  for (Defn * de = scope->firstMember(); de != NULL; de = de->nextInScope()) {
    if (de->isReflected() && isExport(de) && de->isSingular()) {
      if (const VariableDefn * var = dyn_cast<VariableDefn>(de)) {
        // TODO: Handle instance Lets, constants, etc.
        if (var->defnType() == Defn::Var) {
          fields.push_back(getFieldPtr(var));
        }
      }
    }
  }

  return emitStaticList(fields, ".fields.", name,
      Builtins::typeFieldList, Builtins::typeField);
}

llvm::Constant * Reflector::emitTypeList(const QualifiedTypeList & types) {
  // Generate the symbolic name of the type list.
  llvm::SmallString<64> sym(".typelist.(");
  for (TupleType::const_iterator it = types.begin(); it != types.end(); ++it) {
    if (it != types.begin()) {
      sym += ",";
    }
    typeLinkageName(sym, *it);
  }
  sym += ")";

  GlobalVariable * var = irModule_->getGlobalVariable(sym, true);
  if (var != NULL) {
    return var;
  }

  llvm::Type * typePtrType = Builtins::typeType->irEmbeddedType();
  ConstantList typePtrList;
  for (TupleType::const_iterator it = types.begin(); it != types.end(); ++it) {
    llvm::Constant * ptr = getTypePtr(it->type());
    typePtrList.push_back(llvm::ConstantExpr::getPointerCast(ptr, typePtrType));
  }

  Constant * typePtrArray = ConstantArray::get(
      ArrayType::get(typePtrType, typePtrList.size()),
      typePtrList);

  StructBuilder sb(cg_);
  sb.createObjectHeader(Builtins::typeTypeList);
  sb.addIntegerField(reflect::StaticList::size, typePtrList.size());
  sb.addField(typePtrArray);
  Constant * typeListStruct = sb.buildAnon();

  var = new GlobalVariable(*irModule_, typeListStruct->getType(), true,
      GlobalValue::LinkOnceAnyLinkage, typeListStruct, Twine(sym));
  return var;
}

llvm::Constant * Reflector::emitStaticList(const ConstantList & elements,
    StringRef namePrefix, StringRef nameSuffix,
    const CompositeType * listType, const CompositeType * elementType) {
  // If it's an empty list, then create a shared static singleton empty list.
  std::string name(namePrefix);
  GlobalValue::LinkageTypes linkage;
  if (elements.empty()) {
    name += "$empty";
    linkage = GlobalValue::LinkOnceAnyLinkage;
  } else {
    name += nameSuffix;
    linkage = GlobalValue::ExternalLinkage;
  }

  GlobalVariable * var = irModule_->getGlobalVariable(name, true);
  if (var != NULL) {
    return var;
  }

  Constant * data = ConstantArray::get(
      ArrayType::get(elementType->irEmbeddedType(), elements.size()),
      elements);

  StructBuilder sb(cg_);
  sb.createObjectHeader(listType);
  sb.addIntegerField(reflect::StaticList::size, elements.size());
  sb.addField(data);
  Constant * listStruct = sb.buildAnon();

  return new GlobalVariable(*irModule_, listStruct->getType(), true, linkage, listStruct, name);
}

// TODO - remove this and replace all arrays with immutable lists.
llvm::Constant * Reflector::emitArray(
    const std::string & baseName, const VariableDefn * var, const ConstantList & values)
{
  const Type * varType = var->type().unqualified();
  if (const UnionType * utype = dyn_cast<UnionType>(varType)) {
    DASSERT_OBJ(utype->isSingleOptionalType(), utype);
    varType = utype->getFirstNonVoidType();
  }
  const CompositeType * arrayType = cast<CompositeType>(varType);
  QualifiedType elementType = arrayType->typeParam(0);
  DASSERT_OBJ(arrayType->passes().isFinished(CompositeType::RecursiveFieldTypePass), var);

  if (values.empty()) {
    return cg_.genConstantEmptyArray(arrayType);
  }

  StructBuilder sb(cg_);
  sb.createObjectHeader(arrayType);
  sb.addField(cg_.getIntVal(values.size()));
  sb.addArrayField(elementType.unqualified(), values);

  llvm::Constant * arrayStruct = sb.build(arrayType);
  GlobalVariable * array = new GlobalVariable(*irModule_,
      arrayStruct->getType(), true, GlobalValue::InternalLinkage, arrayStruct,
      ".data." + baseName + var->name());
  return llvm::ConstantExpr::getPointerCast(array, arrayType->irEmbeddedType());
}

Module * Reflector::module() { return cg_.module(); }

llvm::Constant * Reflector::getRetainedAttr(const Expr * attrExpr, StringRef baseName) {
  const CompositeType * ctype = cast<CompositeType>(attrExpr->type().unqualified());
  if (ctype->attributeInfo().isRetained()) {
    if (isa<ConstantObjectRef>(attrExpr)) {
      // Construct a unique name for this attribute instance.
      int index = 0;
      StringRef attrInstanceName;
      llvm::SmallVector<char, 64> attrInstanceNameBuffer;
      for (;; ++index) {
        llvm::Twine attrInstanceNameBuilder =
            llvm::Twine(".attr.") +
            llvm::Twine::utohexstr(index) +
            llvm::Twine(".") + baseName;
        attrInstanceName = attrInstanceNameBuilder.toStringRef(attrInstanceNameBuffer);
        if (cg_.irModule()->getGlobalVariable(attrInstanceName) == NULL) {
          break;
        }
      }

      llvm::Constant * attr = cg_.genConstRef(attrExpr, attrInstanceName, false);
      return llvm::ConstantExpr::getPointerCast(attr, Builtins::typeObject->irEmbeddedType());
    } else {
      diag.error(attrExpr) << "Non-constant attribute (not implemented).";
      return NULL;
    }
  } else {
    return NULL;
  }
}

bool Reflector::isExport(const Defn * de) {
  return de->module() == module() || de->isSynthetic();
}

int Reflector::typeKind(Type::TypeClass cls) {
  switch (cls) {
    case Type::Primitive: return PRIMITIVE;
    case Type::Class: return CLASS;
    case Type::Struct: return STRUCT;
    case Type::Interface: return INTERFACE;
    case Type::Protocol: return PROTOCOL;
    case Type::Enum: return ENUM;
    case Type::Function: return FUNCTION;
    case Type::Tuple: return TUPLE;
    case Type::Union: return UNION;
    case Type::NAddress: return ADDRESS;
    case Type::NArray: return NATIVE_ARRAY;
    default:
      return OPAQUE;
  }
}

NameTable::Name * Reflector::addQualifiedName(StringRef name) {
  DASSERT(nameTableVar_ == NULL);
  return cg_.nameTable().addQualifiedName(name);
}

NameTable::Name * Reflector::addName(const StringRef name) {
  DASSERT(nameTableVar_ == NULL);
  return cg_.nameTable().addName(name);
}

GlobalVariable * Reflector::findGlobal(const llvm::Twine & name) {
  llvm::SmallString<128> str;
  return irModule_->getGlobalVariable(name.toStringRef(str), true);
}

} // namespace tart
