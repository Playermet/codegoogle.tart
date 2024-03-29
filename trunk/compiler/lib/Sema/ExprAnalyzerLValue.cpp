/* ================================================================ *
 TART - A Sweet Programming Language.
 * ================================================================ */

#include "tart/Expr/Exprs.h"

#include "tart/Defn/Module.h"
#include "tart/Defn/FunctionDefn.h"
#include "tart/Defn/PropertyDefn.h"
#include "tart/Defn/VariableDefn.h"
#include "tart/Defn/Template.h"

#include "tart/Type/PrimitiveType.h"
#include "tart/Type/NativeType.h"
#include "tart/Type/TupleType.h"
#include "tart/Type/TypeConversion.h"
#include "tart/Type/TypeRelation.h"

#include "tart/Common/Diagnostics.h"

#include "tart/Sema/ExprAnalyzer.h"
#include "tart/Sema/TypeAnalyzer.h"
#include "tart/Sema/BindingEnv.h"
#include "tart/Sema/SpCandidate.h"

namespace tart {

Expr * ExprAnalyzer::reduceValueRef(const ASTNode * ast, bool store) {
  if (ast->nodeType() == ASTNode::GetElement) {
    return reduceElementRef(static_cast<const ASTOper *>(ast), store, false);
  } else {
    return reduceSymbolRef(ast, store);
  }
}

Expr * ExprAnalyzer::reduceLoadValue(const ASTNode * ast) {
  Expr * lvalue = reduceValueRef(ast, false);
  if (isErrorResult(lvalue)) {
    return &Expr::ErrorVal;
  }

  DASSERT_OBJ(lvalue->type(), lvalue);

  if (CallExpr * call = dyn_cast<CallExpr>(lvalue)) {
    if (LValueExpr * lval = dyn_cast<LValueExpr>(call->function())) {
      if (PropertyDefn * prop = dyn_cast<PropertyDefn>(lval->value())) {
        checkAccess(ast->location(), prop);
        return reduceGetParamPropertyValue(ast->location(), call);
      }
    }
  }

  return lvalue;
}

Expr * ExprAnalyzer::reduceStoreValue(const SourceLocation & loc, Expr * lhs, Expr * rhs) {
  if (LValueExpr * lval = dyn_cast<LValueExpr>(lhs)) {
    // If it's a property reference, convert it into a method call.
    if (VariableDefn * var = dyn_cast<VariableDefn>(lval->value())) {
      // The only time we may assign to a 'let' variable is in a constructor, and only
      // if the variable is an instance member of that class.
      if (var->defnType() == Defn::Let) {
        if (var->storageClass() == Storage_Instance) {
          if (FunctionDefn * subjectFn = dyn_cast<FunctionDefn>(subject_)) {
            if (subjectFn->isCtor() && subjectFn->parentDefn() == var->parentDefn()) {
              return new AssignmentExpr(Expr::Assign, loc, lhs, rhs);
            }
          }
        }

        diag.error(loc) << "Assignment to immutable value '" << var << "'";
      } else if (var->defnType() == Defn::MacroArg) {
        // Special case for macro arguments which may be lvalues but weren't evaluated that way.
        Expr * macroVal = var->initValue();
        switch (macroVal->exprType()) {
          case Expr::LValue:
            break;

          case Expr::FnCall: {
            // Transform a property get into a property set.
            FnCallExpr * fnCall = static_cast<FnCallExpr *>(macroVal);
            FunctionDefn * fn = fnCall->function();
            if (PropertyDefn * prop = dyn_cast<PropertyDefn>(fn->parentDefn())) {
              DASSERT(fn == prop->getter());
              if (prop->setter() != NULL) {
                FnCallExpr * setterCall = new FnCallExpr(
                    fnCall->exprType(),
                    fnCall->location(),
                    prop->setter(),
                    fnCall->selfArg());
                setterCall->setType(&VoidType::instance);
                setterCall->args().append(fnCall->args().begin(), fnCall->args().end());
                setterCall->args().push_back(rhs);
                return setterCall;
              }
            } else if (IndexerDefn * idx = dyn_cast<IndexerDefn>(fn->parentDefn())) {
              DASSERT(fn == idx->getter());
              if (idx->setter() != NULL) {
                FnCallExpr * setterCall = new FnCallExpr(
                    fnCall->exprType(),
                    fnCall->location(),
                    idx->setter(),
                    fnCall->selfArg());
                setterCall->setType(&VoidType::instance);
                setterCall->args().append(fnCall->args().begin(), fnCall->args().end());
                setterCall->args().push_back(rhs);
                return setterCall;
              }
            }

            diag.error(lval) << "Not an l-value: " << macroVal;
            break;
          }

          default:
            diag.error() << "Macro param expr type not handled: " <<
                exprTypeName(macroVal->exprType());
            DASSERT(false);
            break;
        }
      }
    }
  } else if (CallExpr * call = dyn_cast<CallExpr>(lhs)) {
    if (LValueExpr * lval = dyn_cast<LValueExpr>(call->function())) {
      if (isa<PropertyDefn>(lval->value())) {
        return reduceSetParamPropertyValue(loc, call, rhs);
      }
    }

    diag.error(lhs) << "lvalue expected on left side of assignment: " << lhs;
  }

  return new AssignmentExpr(Expr::Assign, loc, lhs, rhs);
}

Expr * ExprAnalyzer::reduceSymbolRef(const ASTNode * ast, bool store) {
  LookupResults values;
  lookupName(values, ast, LOOKUP_REQUIRED);

  if (values.size() == 0) {
    // Undefined symbol (error already reported).
    return &Expr::ErrorVal;
  } else if (values.size() > 1) {
    diag.error(ast) << "Multiply defined symbol " << ast;
    return &Expr::ErrorVal;
  } else {
    const LookupResult & sym = values.front();
    if (ValueDefn * val = dyn_cast_or_null<ValueDefn>(sym.defn())) {
      return getLValue(ast->location(), val, sym.expr(), store);
    } else if (TypeDefn * tdef = dyn_cast_or_null<TypeDefn>(sym.defn())) {
      return new TypeLiteralExpr(ast->location(), tdef->value());
    } else {
      diag.error(ast) << "Not an l-value " << ast;
      DFAIL("Not an LValue");
      return &Expr::ErrorVal;
    }
  }
}

Expr * ExprAnalyzer::reduceElementRef(const ASTOper * ast, bool store, bool allowOverloads) {
  // TODO: We might want to support more than 1 array index.
  DASSERT_OBJ(ast->count() >= 1, ast);
  if (ast->count() == 1) {
    TypeAnalyzer ta(module(), activeScope());
    Type * elemType = ta.typeFromAST(ast->arg(0));
    if (elemType == NULL) {
      return &Expr::ErrorVal;
    }
    return new TypeLiteralExpr(ast->location(), getArrayTypeForElement(elemType));
  }

  // If it's a name, see if it's a specializable name.
  const ASTNode * base = ast->arg(0);
  Expr * arrayExpr;
  if (base->nodeType() == ASTNode::Id || base->nodeType() == ASTNode::Member) {
    LookupResults values;
    lookupName(values, base, LOOKUP_REQUIRED);

    if (values.size() == 0) {
      return &Expr::ErrorVal;
    } else {
      // For type names and function names, brackets always mean specialize, not index.
      bool isTypeOrFunc = false;
      for (LookupResults::iterator it = values.begin(); it != values.end(); ++it) {
        if (isa<TypeDefn>(it->defn())) {
          isTypeOrFunc = true;
          break;
        } else if (isa<FunctionDefn>(it->defn())) {
          isTypeOrFunc = true;
          break;
        }
      }

      if (isTypeOrFunc) {
        ASTNodeList args;
        SourceLocation loc(ast->location());
        args.insert(args.begin(), ast->args().begin() + 1, ast->args().end());
        Expr * specResult = specialize(loc, values, args, true);
        if (specResult != NULL) {
          // TODO: This collapses the SpE into a single answer based only on the
          // template arguments. We could instead simply return the SpE and collapse
          // at the point of use, where we would have access to additional contextual
          // information.
          if (SpecializeExpr * spe = dyn_cast<SpecializeExpr>(specResult)) {
            if (allowOverloads) {
              return spe;
            }
            BindingEnv env;
            SourceContext specSite(loc, NULL, spe);
            const SpCandidateList & candidates = spe->candidates();
            SpCandidateList unifiedCandidates;
            for (SpCandidateList::const_iterator it = candidates.begin(); it != candidates.end();
                ++it) {
              SpCandidate * sp = *it;
              SourceContext candidateSite(sp->def()->location(), &specSite, sp->def(), Format_Type);
              sp->relabelTypeVars(env);
              // If unification fails, just skip over it - it's not an error.
              if (sp->unify(&candidateSite, env)) {
                unifiedCandidates.push_back(sp);
              }
            }

            if (unifiedCandidates.size() > 1) {
              diag.error(ast) << "Multiple definitions for '" << base << "':";
              for (SpCandidateList::const_iterator it = candidates.begin(); it != candidates.end();
                  ++it) {
                SpCandidate * sp = *it;
                diag.info(sp->def()) << sp->def();
              }
              return &Expr::ErrorVal;
            }

            SpCandidate * spFinal = unifiedCandidates.front();
            env.updateAssignments(loc, spFinal);
            QualifiedTypeVarMap vars;
            env.toTypeVarMap(vars, spFinal);
            if (TypeDefn * tdef = dyn_cast<TypeDefn>(spFinal->def())) {
              const Type * type = tdef->templateSignature()->instantiateType(loc, vars);
              if (type != NULL) {
                return new TypeLiteralExpr(loc, type);
              }
            } else {
              // It can only be a function
              Defn * defn = spFinal->def()->templateSignature()->instantiate(loc, vars);
              if (defn != NULL) {
                if (defn->storageClass() == Storage_Instance && spFinal->base() == NULL) {
                  diag.error(loc) << "Cannot access non-static method '" <<
                      defn->name() << "' from static method.";
                  return &Expr::ErrorVal;
                }

                analyzeDefn(defn, Task_PrepTypeComparison);
                return LValueExpr::get(loc, spFinal->base(), cast<ValueDefn>(defn));
              }
            }
            return &Expr::ErrorVal;
          }
          return specResult;
        } else {
          return &Expr::ErrorVal;
        }
      }

      if (values.size() > 1) {
        diag.error(base) << "Multiply defined symbol " << base;
        return &Expr::ErrorVal;
      }

      const LookupResult & sym = values.front();
      if (ValueDefn * val = dyn_cast_or_null<ValueDefn>(sym.defn())) {
        arrayExpr = getLValue(ast->location(), val, sym.expr(), store);
      } else if (sym.defn() == NULL && sym.expr() != NULL) {
        arrayExpr = sym.expr();
      } else {
        diag.error(base) << "Expression " << base << " is neither an array type nor a template";
        return &Expr::ErrorVal;
      }
    }
  } else {
    // TODO: We might want to support more than 1 array index.
    DASSERT_OBJ(ast->count() == 2, ast);
    arrayExpr = inferTypes(reduceExpr(base, NULL), NULL);
  }

  if (isErrorResult(arrayExpr)) {
    return &Expr::ErrorVal;
  }

  // What we want is to determine whether or not the expression is a template. Or even a type.

  QualifiedType arrayType = dealias(arrayExpr->type());
  DASSERT_OBJ(arrayType, arrayExpr);

  // Reduce all of the arguments
  ExprList args;
  for (size_t i = 1; i < ast->count(); ++i) {
    Expr * arg = reduceExpr(ast->arg(i), NULL);
    if (isErrorResult(arg)) {
      return &Expr::ErrorVal;
    }

    args.push_back(arg);
  }

  if (args.empty()) {
    diag.fatal(ast) << "Array element access with no arguments";
    return &Expr::ErrorVal;
  }

  // Memory address type
  if (Qualified<AddressType> maType = arrayType.dyn_cast<AddressType>()) {
    QualifiedType elemType = maType->typeParam(0);
    if (!AnalyzerBase::analyzeType(elemType, Task_PrepTypeComparison)) {
      return &Expr::ErrorVal;
    }

    DASSERT_OBJ(!elemType.isNull(), maType);

    if (args.size() != 1) {
      diag.fatal(ast) << "Incorrect number of array index dimensions";
    }

    // TODO: Attempt to cast arg to an integer type of known size.

    // First dereference the pointer and then get the element.
    return new BinaryExpr(Expr::ElementRef, ast->location(), elemType, arrayExpr, args[0]);
  }

  // Do automatic pointer dereferencing
  /*if (AddressType * npt = dyn_cast<AddressType>(arrayType)) {
   if (!AnalyzerBase::analyzeType(npt, Task_PrepTypeComparison)) {
   return &Expr::ErrorVal;
   }

   arrayType = npt->typeParam(0);
   arrayExpr = new UnaryExpr(Expr::PtrDeref, arrayExpr->location(), arrayType, arrayExpr);
   }*/

  // Handle native arrays.
  if (Qualified<NativeArrayType> naType = arrayType.dyn_cast<NativeArrayType>()) {
    if (!AnalyzerBase::analyzeType(naType.unqualified(), Task_PrepTypeComparison)) {
      return &Expr::ErrorVal;
    }

    QualifiedType elemType = naType->typeParam(0);
    DASSERT_OBJ(!elemType.isNull(), naType);

    if (args.size() != 1) {
      diag.fatal(ast) << "Incorrect number of array subscripts";
      return &Expr::ErrorVal;
    }

    Expr * indexExpr = args[0];
    if (TypeConversion::check(
        indexExpr, &Int32Type::instance, TypeConversion::COERCE) == Incompatible) {
      diag.fatal(ast) << "Native array subscript must be integer type, but is " <<
          indexExpr->type();
      return &Expr::ErrorVal;
    }

    // TODO: Attempt to cast arg to an integer type of known size.
    return new BinaryExpr(Expr::ElementRef, ast->location(), elemType, arrayExpr, indexExpr);
  }

  // Handle flexible arrays.
  if (Qualified<FlexibleArrayType> faType = arrayType.dyn_cast<FlexibleArrayType>()) {
    if (!AnalyzerBase::analyzeType(faType.unqualified(), Task_PrepTypeComparison)) {
      return &Expr::ErrorVal;
    }

    QualifiedType elemType = faType->typeParam(0);
    DASSERT_OBJ(!elemType.isNull(), faType);

    if (args.size() != 1) {
      diag.fatal(ast) << "Incorrect number of array subscripts";
      return &Expr::ErrorVal;
    }

    Expr * indexExpr = args[0];
    if (TypeConversion::check(
        indexExpr, &Int32Type::instance, TypeConversion::COERCE) == Incompatible) {
      diag.fatal(ast) << "Flexible array subscript must be integer type, but is " <<
          indexExpr->type();
      return &Expr::ErrorVal;
    }

    // TODO: Attempt to cast arg to an integer type of known size.
    return new BinaryExpr(Expr::ElementRef, ast->location(), elemType, arrayExpr, indexExpr);
  }

  // Handle tuples
  if (const TupleType * tt = dyn_cast<TupleType>(arrayType.type())) {
    if (args.size() != 1) {
      diag.fatal(ast) << "Incorrect number of array subscripts";
      return &Expr::ErrorVal;
    }

    DASSERT(arrayExpr->isSingular()) << "Non-singular tuple expression: " << arrayExpr;

    Expr * indexExpr = args[0];
    const Type * indexType = indexExpr->type().type();
    if (TypeConversion::check(
        indexExpr, &Int32Type::instance, TypeConversion::COERCE) == Incompatible) {
      diag.fatal(args[0]) << "Tuple subscript must be integer type, is " << indexType;
      return &Expr::ErrorVal;
    }

    if (ConstantInteger * cint = dyn_cast<ConstantInteger>(indexExpr)) {
      const llvm::APInt & indexVal = cint->intValue();
      if (indexVal.isNegative() || indexVal.getZExtValue() >= tt->numTypeParams()) {
        diag.fatal(args[0]) << "Tuple subscript out of range";
        return &Expr::ErrorVal;
      } else if (store) {
        diag.fatal(args[0]) << "Tuples are immutable";
        return &Expr::ErrorVal;
      }

      uint32_t index = uint32_t(indexVal.getZExtValue());
      return new BinaryExpr(Expr::ElementRef, ast->location(), tt->member(index), arrayExpr, cint);
    } else {
      diag.fatal(args[0]) << "Tuple subscript must be an integer constant";
      return &Expr::ErrorVal;
    }
  }

  // See if the type has any indexers defined.
  DefnList indexers;
  if (arrayType->typeDefn() != NULL) {
    if (!AnalyzerBase::analyzeType(arrayType, Task_PrepMemberLookup)) {
      return &Expr::ErrorVal;
    }
  }

  if (arrayType->memberScope() == NULL ||
      !arrayType->memberScope()->lookupMember("$index", indexers, true)) {
    if (!arrayType->isErrorType()) {
      diag.error(ast) << "Type " << arrayType << " does not support element reference operator";
    }
    return &Expr::ErrorVal;
  }

  DASSERT(!indexers.empty());

  // For now, we only allow a single indexer to be defined. Later we will allow
  // overloading both by argument type and return type.
  if (indexers.size() > 1) {
    diag.fatal(ast) << "Multiple indexer definitions are currently not supported";
    return &Expr::ErrorVal;
  }

  PropertyDefn * indexer = cast<PropertyDefn>(indexers.front());
  if (!analyzeProperty(indexer, Task_PrepTypeComparison)) {
    return &Expr::ErrorVal;
  }

  // CallExpr type used to hold the array reference and parameters.
  LValueExpr * callable = LValueExpr::get(ast->location(), arrayExpr, indexer);
  CallExpr * call = new CallExpr(Expr::Call, ast->location(), callable);
  call->args().append(args.begin(), args.end());
  call->setType(indexer->type());
  return call;
}

Expr * ExprAnalyzer::reduceGetParamPropertyValue(const SourceLocation & loc, CallExpr * call) {

  LValueExpr * lval = cast<LValueExpr>(call->function());
  PropertyDefn * prop = cast<PropertyDefn>(lval->value());
  Expr * basePtr = lvalueBase(lval);
  //const ASTNodeList & args = call->args();

  if (!analyzeProperty(prop, Task_PrepTypeComparison)) {
    return &Expr::ErrorVal;
  }

  DASSERT_OBJ(prop->isSingular(), prop);
  DASSERT_OBJ(prop->getter()->isSingular(), prop);

  FunctionDefn * getter = prop->getter();
  if (getter == NULL) {
    diag.fatal(prop) << "No getter for property '" << prop->name() << "'";
    return &Expr::ErrorVal;
  }

  if (!analyzeFunction(getter, Task_PrepTypeComparison)) {
    return &Expr::ErrorVal;
  }

  DASSERT_OBJ(TypeRelation::isEqual(getter->returnType(), prop->type()), getter);
  DASSERT_OBJ(!getter->returnType()->isVoidType(), getter);

#if 0

  CallExpr * call = new CallExpr(Expr::Call, loc, NULL);
  call->setExpectedReturnType(prop->type());
  if (!addOverload(call, basePtr, getter, args)) {
    return &Expr::ErrorVal;
  }

  if (!reduceArgList(args, call)) {
    return &Expr::ErrorVal;
  }

  if (call->candidates().empty()) {
    // Generate the calling signature in a buffer.
    std::stringstream callsig;
    FormatStream fs(callsig);
    fs << Format_Dealias << basePtr << "[";
    formatExprTypeList(fs, call->args());
    fs << "]";
    fs << " -> " << prop->type();

    diag.error(loc) << "No matching method for call to " << callsig.str() << ", candidates are:";
    /*for (ExprList::iterator it = results.begin(); it != results.end(); ++it) {
     Expr * resultMethod = *it;
     if (LValueExpr * lval = dyn_cast<LValueExpr>(*it)) {
     diag.info(lval->value()) << Format_Type << lval->value();
     } else {
     diag.info(*it) << *it;
     }
     }*/

    return &Expr::ErrorVal;
  }

  call->setType(reduceReturnType(call));
  return call;

#else

  // TODO: Type check args against function signature.

  ExprList castArgs;
  size_t argCount = call->args().size();
  for (size_t i = 0; i < argCount; ++i) {
    const Type * paramType = getter->functionType()->param(i)->type().type();
    Expr * arg = call->arg(i);
    arg = inferTypes(subject(), arg, paramType);
    Expr * castArg = paramType->implicitCast(arg->location(), arg);
    if (isErrorResult(castArg)) {
      return &Expr::ErrorVal;
    }

    castArgs.push_back(castArg);
  }

  Expr::ExprType callType = Expr::FnCall;
  if (basePtr->type()->typeClass() == Type::Interface ||
      (basePtr->type()->typeClass() == Type::Class && !getter->isFinal())) {
    callType = Expr::VTableCall;
  }

  FnCallExpr * getterCall = new FnCallExpr(callType, loc, getter, basePtr);
  getterCall->setType(prop->type());
  getterCall->args().append(castArgs.begin(), castArgs.end());
  module()->addSymbol(getter);
  return getterCall;
#endif
}

/** Given a symbol lookup result, attempt to return an LValue reference for that symbol. */
Expr * ExprAnalyzer::getLValue(SLC & loc, ValueDefn * val, Expr * base, bool store) {
  DASSERT(val != NULL);
  analyzeDefn(val, Task_PrepTypeComparison);
  DASSERT(val->type());
  if (store && val->isReadOnly()) {
    diag.error(loc) << "Attempt to write to read-only field '" << val << "'";
  }

  checkAccess(loc, val);
  switch (val->storageClass()) {
    case Storage_Global:
    case Storage_Static:
      base = NULL;

      // If it's a let-variable, then make sure that the initialization value
      // gets evaluated.
      if (val->defnType() == Defn::Let &&
          val->module() != module() &&
          val->module() != NULL) {
        analyzeDefn(val, Task_PrepConstruction);
      }
      break;

    case Storage_Local: {
      base = NULL;
      break;
    }

    case Storage_Instance: {
      if (base == NULL) {
        diag.error(loc) << "Cannot access non-static member '" << val->name() <<
            "' from static method.";
        return &Expr::ErrorVal;
      }
      if (store && !base->type().isWritable() && !val->isMutable()) {
        diag.error(loc) << "Attempt to write to read-only field '" << val << "'";
      }
      break;
    }

    case Storage_Class:
    default:
      DFAIL("Invalid storage class");
      break;
  }

  // Addresses are implicitly dereferenced.
  if (base != NULL && base->type()->typeClass() == Type::NAddress) {
    base = new UnaryExpr(Expr::PtrDeref, base->location(), base->type()->typeParam(0), base);
  }

  LValueExpr * result = LValueExpr::get(loc, base, val);
  if (ParameterDefn * param = dyn_cast<ParameterDefn>(val)) {
    DASSERT(param->passes().isFinished(ParameterDefn::TypeModifierPass));
    result->setType(param->internalType());
  }
  return result;
}

/** Given an LValue, return the base expression. */
Expr * ExprAnalyzer::lvalueBase(LValueExpr * lval) {
  return lval->base();
}

}
