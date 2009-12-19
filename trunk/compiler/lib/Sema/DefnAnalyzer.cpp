/* ================================================================ *
    TART - A Sweet Programming Language.
 * ================================================================ */

#include "tart/CFG/CompositeType.h"
#include "tart/CFG/FunctionType.h"
#include "tart/CFG/FunctionDefn.h"
#include "tart/CFG/TypeDefn.h"
#include "tart/CFG/PropertyDefn.h"
#include "tart/CFG/PrimitiveType.h"
#include "tart/CFG/NativeType.h"
#include "tart/CFG/UnitType.h"
#include "tart/CFG/TupleType.h"
#include "tart/CFG/Module.h"
#include "tart/CFG/Template.h"
#include "tart/CFG/TemplateConditions.h"
#include "tart/Common/Diagnostics.h"
#include "tart/Common/InternedString.h"
#include "tart/Common/PackageMgr.h"
#include "tart/Sema/DefnAnalyzer.h"
#include "tart/Sema/ParameterAssignments.h"
#include "tart/Sema/TypeAnalyzer.h"
#include "tart/Sema/StmtAnalyzer.h"
#include "tart/Sema/ExprAnalyzer.h"
#include "tart/Sema/ScopeBuilder.h"
#include "tart/Sema/FindExternalRefsPass.h"
#include "tart/Sema/EvalPass.h"
#include "tart/Objects/Builtins.h"
#include "tart/Objects/Intrinsic.h"

namespace tart {

class TemplateParamAnalyzer : public TypeAnalyzer {
public:
  TemplateParamAnalyzer(Defn * de)
    : TypeAnalyzer(de->module(), de->definingScope())
    , tsig_(de->templateSignature())
  {}

  Type * reduceTypeVariable(const ASTPatternVar * ast);

private:
  llvm::StringMap<TypeVariable *> vars_;
  TemplateSignature * tsig_;
};

Type * TemplateParamAnalyzer::reduceTypeVariable(const ASTPatternVar * ast) {
  llvm::StringMap<TypeVariable *>::iterator it = vars_.find(ast->name());
  TypeVariable * tvar = NULL;
  if (it != vars_.end()) {
    tvar = it->second;
  }

  // TODO: Actually we should make this a proper scope so that we can refer to the
  // same pattern variable without the % in the template parameter list.
  if (tvar == NULL) {
    tvar = new TypeVariable(ast->location(), ast->name(), NULL);
    vars_[ast->name()] = tvar;
  }

  // See if the type variable has constraints
  Type * type = NULL;
  if (ast->type() != NULL) {
    Type * type = typeFromAST(ast->type());
    if (type != NULL) {
      if (ast->constraint() == ASTPatternVar::IS_SUBTYPE) {
        // Add a subclass test
        TemplateCondition * condition = new IsSubtypeCondition(tvar, type);
        tsig_->conditions().push_back(condition);
      } else if (ast->constraint() == ASTPatternVar::IS_SUPERTYPE) {
        // Add a subclass test - reversed.
        TemplateCondition * condition = new IsSubtypeCondition(type, tvar);
        tsig_->conditions().push_back(condition);
      } else {
        if (tvar->valueType() == NULL) {
          tvar->setValueType(type);
        } else if (!tvar->valueType()->isEqual(type)) {
          diag.error(ast) << "Conflicting type declaration for pattern variable '" <<
              ast->name() << "'";
        }
      }
    }
  }

  return tvar;
}

bool DefnAnalyzer::analyzeModule() {
  bool success = true;

  if (!createMembersFromAST(module)) {
    return false;
  }

  if (module->firstMember() == NULL) {
    diag.fatal() << "Module should have at least one definition";
    return false;
  }

  // Analyze all exported definitions.
  for (Defn * de = module->firstMember(); de != NULL; de = de->nextInScope()) {
    if (de->isTemplate()) {
      analyzeTemplateSignature(de);
    }

    if (!de->hasUnboundTypeParams()) {
      if (analyzeDefn(de, Task_PrepCodeGeneration)) {
        module->addSymbol(de);
      } else {
        success = false;
        diag.recovered();
      }
    }

    if (de->isSingular()) {
      addReflectionInfo(de);
    }
  }

  analyzeType(Builtins::typeTypeInfoBlock.get(), Task_PrepCodeGeneration);
  analyzeDefn(Builtins::funcTypecastError, Task_PrepTypeGeneration);

  // Now deal with the xrefs. Synthetic xrefs need to be analyzed all the
  // way down; Non-synthetic xrefs only need to be analyzed deep enough to
  // be externally referenced.

  // Analyze all external references.
  while (Defn * de = module->nextDefToAnalyze()) {
    if (analyzeDefn(de, Task_PrepCodeGeneration)) {
      if (module->isReflectionEnabled() && module->exportDefs().count(de) > 0) {
        analyzeDefn(de, Task_PrepReflection);
      }

      FindExternalRefsPass::run(module, de);
    } else {
      success = false;
      diag.recovered();
    }
  }

  /*for (DefnSet::iterator it = module->exportDefs().begin(); it != module->exportDefs().end(); ++it) {
    diag.debug() << "Export " << Format_Verbose << *it;
  }*/

  // Prevent further symbols from being added.
  module->finishPass(Pass_ResolveModuleMembers);
  return success;
}

bool DefnAnalyzer::analyze(Defn * in, DefnPasses & passes) {

  // Create members of this scope.
  if (passes.contains(Pass_CreateMembers)) {
    createMembersFromAST(in);
  }

  // Resolve attributes
  if (passes.contains(Pass_ResolveAttributes) && !resolveAttributes(in)) {
    return false;
  }

  return true;
}

bool DefnAnalyzer::createMembersFromAST(Defn * in) {
  // Create members of this scope.
  if (in->beginPass(Pass_CreateMembers)) {
    if (in->ast() != NULL) {
      ScopeBuilder::createScopeMembers(in);
    }

    in->finishPass(Pass_CreateMembers);
  }

  return true;
}

bool DefnAnalyzer::resolveAttributes(Defn * in) {
  if (in->beginPass(Pass_ResolveAttributes)) {
    if (in == Builtins::typeAttribute->typeDefn()) {
      // Don't evaluate attributes for class Attribute - it creates a circular dependency.
      CompositeType * attrType = Builtins::typeAttribute.get();
      attrType->setClassFlag(CompositeType::Attribute, true);
      attrType->attributeInfo().setTarget(AttributeInfo::CLASS);
      attrType->attributeInfo().setRetained(false);
      attrType->typeDefn()->addTrait(Defn::Nonreflective);
      in->finishPass(Pass_ResolveAttributes);
      return true;
    }

    if (in->ast() != NULL) {
      ExprAnalyzer ea(module, activeScope, subject(), NULL);
      const ASTNodeList & attrs = in->ast()->attributes();
      for (ASTNodeList::const_iterator it = attrs.begin(); it != attrs.end(); ++it) {
        Expr * attrExpr = ea.reduceAttribute(*it);
        if (attrExpr != NULL) {
          //diag.info(in) << attrExpr;
          in->attrs().push_back(attrExpr);
        }
      }
    }

    applyAttributes(in);

    // Also propagate attributes from the parent defn.
    Defn * parent = in->parentDefn();
    if (parent != NULL) {
      for (ExprList::const_iterator it = parent->attrs().begin(); it != parent->attrs().end(); ++it) {
        propagateMemberAttributes(parent, in);
      }
    }

    in->finishPass(Pass_ResolveAttributes);
  }

  return true;
}

bool DefnAnalyzer::propagateSubtypeAttributes(Defn * baseDefn, Defn * target) {
  const ExprList & baseAttributes = baseDefn->attrs();
  for (ExprList::const_iterator it = baseAttributes.begin(); it != baseAttributes.end(); ++it) {
    if (const CompositeType * attrType = dyn_cast<CompositeType>((*it)->type())) {
      DASSERT(attrType->isAttribute());
      if (attrType->attributeInfo().propagation() & AttributeInfo::SUBTYPES) {
        if (!propagateAttribute(target, *it)) {
          return false;
        }
      }
    }
  }

  return true;
}

bool DefnAnalyzer::propagateMemberAttributes(Defn * scopeDefn, Defn * target) {
  const ExprList & scopeAttributes = scopeDefn->attrs();
  for (ExprList::const_iterator it = scopeAttributes.begin(); it != scopeAttributes.end(); ++it) {
    if (const CompositeType * attrType = dyn_cast<CompositeType>((*it)->type())) {
      DASSERT(attrType->isAttribute());
      if (attrType->attributeInfo().propagation() & AttributeInfo::MEMBERS) {
        if (!propagateAttribute(target, *it)) {
          return false;
        }
      }
    }
  }

  return true;
}

bool DefnAnalyzer::propagateAttribute(Defn * in, Expr * attr) {
  const CompositeType * attrType = cast<CompositeType>(attr->type());
  DASSERT(attrType->isAttribute());
  for (ExprList::const_iterator it = in->attrs().begin(); it != in->attrs().end(); ++it) {
    Expr * existingAttr = *it;
    const CompositeType * existingAttrType = cast<CompositeType>(existingAttr->type());
    if (existingAttrType->isEqual(attrType)) {
      return true;
    }
  }

  if (attrType->attributeInfo().canAttachTo(in)) {
    in->attrs().push_back(attr);
    if (attr->exprType() == Expr::ConstObjRef) {
      ConstantObjectRef * attrObj = static_cast<ConstantObjectRef *>(attr);
      FunctionDefn * applyMethod = dyn_cast_or_null<FunctionDefn>(
          attrType->lookupSingleMember("apply", true));
      if (applyMethod != NULL) {
        applyAttribute(in, attrObj, applyMethod);
      } else {
        diag.info(in) << "Unhandled attribute " << attr;
      }
    }
  }

  return true;
}

void DefnAnalyzer::applyAttributes(Defn * in) {
  ExprList & attrs = in->attrs();
  for (ExprList::iterator it = attrs.begin(); it != attrs.end(); ++it) {
    Expr * attrExpr = ExprAnalyzer::inferTypes(subject(), *it, NULL);
    if (isErrorResult(attrExpr)) {
      continue;
    }

    *it = attrExpr;

    // Handle @Intrinsic as a special case.
    const Type * attrType = attrExpr->type();
    if (attrType == Builtins::typeIntrinsicAttribute) {
      handleIntrinsicAttribute(in, attrExpr);
      continue;
    }

    if (attrExpr->exprType() == Expr::CtorCall) {
      const CompositeType * attrClass = cast<CompositeType>(attrType);
      if (!attrClass->isAttribute()) {
        diag.error(*it) << "Invalid attribute expression @" << *it;
        continue;
      }

      if (!attrClass->attributeInfo().canAttachTo(in)) {
        diag.error(*it) << "Attribute '" << attrClass << "' cannot apply to target '" << in << "'";
        continue;
      }

      // Evaluate the attribute. If it returns NULL, it simply means that it could not
      // be evaluated at compile-time.
      Expr * attrVal = EvalPass::eval(attrExpr, true);
      if (isErrorResult(attrVal)) {
        continue;
      }

      // Replace the attribute with its compiled representation.
      *it = attrVal;

      if (attrVal->exprType() == Expr::ConstObjRef) {
        ConstantObjectRef * attrObj = static_cast<ConstantObjectRef *>(attrVal);
        DASSERT_OBJ(attrVal->type()->isEqual(attrClass), attrVal->type());

        // Special case for @Attribute
        if (attrClass == Builtins::typeAttribute) {
          handleAttributeAttribute(in, attrObj);
          continue;
        }

        FunctionDefn * applyMethod = dyn_cast_or_null<FunctionDefn>(
            attrClass->lookupSingleMember("apply", true));
        if (applyMethod != NULL) {
          applyAttribute(in, attrObj, applyMethod);
        } else {
          diag.info(in) << "Unhandled attribute " << *it;
        }
      }
    }
  }
}

void DefnAnalyzer::applyAttribute(Defn * de, ConstantObjectRef * attrObj,
    FunctionDefn * applyMethod) {
  if (analyzeDefn(applyMethod, Task_PrepEvaluation)) {
    ExprList args;
    if (TypeDefn * tdef = dyn_cast<TypeDefn>(de)) {
      args.push_back(tdef->asExpr());
    } else if (ValueDefn * vdef = dyn_cast<ValueDefn>(de)) {
      args.push_back(LValueExpr::get(attrObj->location(), NULL, vdef));
    } else {
      DFAIL("Implement");
    }

    applyMethod->eval(de->location(), attrObj, args);
  }
}

void DefnAnalyzer::handleIntrinsicAttribute(Defn * de, Expr * attrExpr) {
  if (FunctionDefn * func = dyn_cast<FunctionDefn>(de)) {
    func->setIntrinsic(Intrinsic::get(func->qualifiedName().c_str()));
  } else {
    diag.fatal(attrExpr) << "Only functions can be Intrinsics";
  }
}

void DefnAnalyzer::handleAttributeAttribute(Defn * de, ConstantObjectRef * attrObj) {
  int target = attrObj->memberValueAsInt("target");
  int retention = attrObj->memberValueAsInt("retention");
  int propagation = attrObj->memberValueAsInt("propagation");

  TypeDefn * targetTypeDefn = cast<TypeDefn>(de);
  CompositeType * targetType = cast<CompositeType>(targetTypeDefn->typeValue());
  targetType->setClassFlag(CompositeType::Attribute, true);
  targetType->attributeInfo().setTarget(target);
  targetType->attributeInfo().setRetained(retention);
  targetType->attributeInfo().setPropagation(propagation);
  if (!retention) {
    // If a type is not retained, then don't generate reflection information for it.
    targetTypeDefn->addTrait(Defn::Nonreflective);
  }
}

void DefnAnalyzer::importIntoScope(const ASTImport * import, Scope * targetScope) {
  if (import->unpack()) {
    DFAIL("Implement import namespace");
  }

  ExprList importDefs;
  Scope * saveScope = setActiveScope(&Builtins::module); // Inhibit unqualified search.
  if (lookupName(importDefs, import->path(), true)) {
    targetScope->addMember(new ExplicitImportDefn(module, import->asName(), importDefs));
  } else if (lookupName(importDefs, import->path(), false)) {
    targetScope->addMember(new ExplicitImportDefn(module, import->asName(), importDefs));
  } else {
    diag.fatal(import) << "Not found '" << import << "'";
  }

  setActiveScope(saveScope);
}

void DefnAnalyzer::analyzeTemplateSignature(Defn * de) {
  TemplateSignature * tsig = de->templateSignature();
  DASSERT_OBJ(tsig != NULL, de);

  if (tsig->ast() != NULL) {
    DASSERT_OBJ(de->definingScope() != NULL, de);
    const ASTNodeList & paramsAst = tsig->ast()->params();
    TypeList params;

    for (ASTNodeList::const_iterator it = paramsAst.begin(); it != paramsAst.end(); ++it) {
      Type * param = TemplateParamAnalyzer(de).typeFromAST(*it);
      if (param != NULL) {
        params.push_back(param);
      }
    }

    tsig->setTypeParams(TupleType::get(params));

    if (!de->hasUnboundTypeParams()) {
      de->addTrait(Defn::Singular);
    }

    // Remove the AST so that we don't re-analyze this template.
    tsig->setAST(NULL);
  }
}

void DefnAnalyzer::addPass(Defn * de, DefnPasses & toRun, const DefnPass requested) {
  toRun.add(requested);
  toRun.removeAll(de->finished());
}

void DefnAnalyzer::addPasses(Defn * de, DefnPasses & toRun, const DefnPasses & requested) {
  toRun.addAll(requested);
  toRun.removeAll(de->finished());
}

void DefnAnalyzer::addReflectionInfo(Defn * in) {
  analyzeDefn(in, Task_PrepTypeComparison);
  bool enableReflection = module->isReflectionEnabled() &&
        (in->isSynthetic() || in->module() == module);
  bool enableReflectionDetail = enableReflection && !in->hasTrait(Defn::Nonreflective);

  if (TypeDefn * tdef = dyn_cast<TypeDefn>(in)) {
    switch (tdef->typeValue()->typeClass()) {
      case Type::Primitive:
        if (enableReflectionDetail) {
          module->reflectedDefs().insert(in);
          importSystemType(Builtins::typeSimpleType);
        }
        break;

      case Type::Class:
      case Type::Interface: {
        CompositeType * ctype = static_cast<CompositeType *>(tdef->typeValue());

        // If reflection enabled for this type then load the reflection classes.
        if (enableReflectionDetail) {
          if (module->reflectedDefs().insert(tdef)) {
            module->addSymbol(tdef);
            reflectTypeMembers(ctype);
          }

          if (importSystemType(Builtins::typeComplexType)) {
            importSystemType(Builtins::typeModule);
          }
        } else if (enableReflection) {
          module->reflectedDefs().insert(tdef);
          importSystemType(Builtins::typeSimpleType);
        }

        break;
      }

      case Type::Struct:
        if (enableReflectionDetail) {
          importSystemType(Builtins::typeComplexType);
        }

        break;

      case Type::Protocol:
        break;

      case Type::Enum:
        if (enableReflectionDetail) {
          module->reflectedDefs().insert(in);
          importSystemType(Builtins::typeEnumType);
        }
        break;

      default:
        break;
    }
  } else if (FunctionDefn * fn = dyn_cast<FunctionDefn>(in)) {
    if (!fn->isIntrinsic() && !fn->isExtern()) {
      if (enableReflectionDetail) {
        if (module->reflectedDefs().insert(fn)) {
          module->addSymbol(fn);
          importSystemType(Builtins::typeMethod);
          importSystemType(Builtins::typeFunctionType);
          importSystemType(Builtins::typeDerivedType);
          importSystemType(Builtins::typeModule);
        }

        for (ParameterList::iterator it = fn->params().begin(); it != fn->params().end(); ++it) {
          const Type * paramType = (*it)->internalType();
          reflectType(paramType);
          if (paramType->isBoxableType()) {
            // Cache the unbox function for this type.
            ExprAnalyzer(module, activeScope, fn, fn).getUnboxFn(SourceLocation(), paramType);
          }
        }

        reflectType(fn->returnType());
        if (fn->returnType()->isBoxableType()) {
          // Cache the boxing function for this type.
          ExprAnalyzer(module, activeScope, subject_, fn).coerceToObjectFn(fn->returnType());
        }
      }
    }
  } else if (PropertyDefn * prop = dyn_cast<PropertyDefn>(in)) {
    if (enableReflectionDetail) {
      module->reflectedDefs().insert(in);
      reflectType(prop->type());
      importSystemType(Builtins::typeProperty);
      importSystemType(Builtins::typeModule);
    }
  } else if (VariableDefn * var = dyn_cast<VariableDefn>(in)) {
    if (enableReflectionDetail) {
      //diag.info() << Format_Verbose << "Member " << in;
      reflectType(var->type());
    }
  }
}

bool DefnAnalyzer::reflectType(const Type * type) {
  TypeDefn * tdef = type->typeDefn();
  if (tdef != NULL) {
    if (tdef->isSynthetic()) {
      addReflectionInfo(tdef);
    } else if (module->addSymbol(tdef)) {
      if (type->typeClass() == Type::Primitive) {
        importSystemType(Builtins::typeSimpleType);
      } else if (type->typeClass() == Type::Enum) {
        importSystemType(Builtins::typeEnumType);
      }
      //diag.info() << Format_Verbose << "Reflecting type: " << type;
    }

    return true;
  }

  return false;
}

void DefnAnalyzer::reflectTypeMembers(CompositeType * type) {
  analyzeDefn(type->typeDefn(), Task_PrepCodeGeneration);
  //diag.info() << Format_Verbose << "Adding reflect info for " << type;

  // If we're doing detailed reflection, then reflect the members of this type.
  for (Defn * de = type->firstMember(); de != NULL; de = de->nextInScope()) {
    if (de->isSingular()) {
      switch (de->defnType()) {
        case Defn::Typedef:
        case Defn::Namespace:
        case Defn::Var:
        case Defn::Let:
        case Defn::Property:
        case Defn::Indexer:
        case Defn::Function:
          addReflectionInfo(de);
          break;

        default:
          break;
      }
    }
  }

  ClassList & bases = type->bases();
  for (ClassList::iterator it = bases.begin(); it != bases.end(); ++it) {
    reflectType(*it);
  }
}

bool DefnAnalyzer::importSystemType(const SystemClass & sclass) {
  if (module->addSymbol(sclass.typeDefn())) {
    AnalyzerBase::analyzeType(sclass, Task_PrepCodeGeneration);
    sclass->addFieldTypesToModule(module);
    return true;
  }

  return false;
}

Module * DefnAnalyzer::moduleForDefn(const Defn * def) {
  if (def->isTemplateInstance()) {
    return def->templateInstance()->templateDefn()->module();
  } else {
    return def->module();
  }
}

}
