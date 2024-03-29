/* ================================================================ *
    TART - A Sweet Programming Language.
 * ================================================================ */

#ifndef TART_DEFN_SCOPE_H
#define TART_DEFN_SCOPE_H

#ifndef TART_DEFN_SYMBOLTABLE_H
#include "tart/Defn/SymbolTable.h"
#endif

#ifndef TART_COMMON_GC_H
#include "tart/Common/GC.h"
#endif

#ifndef LLVM_ADT_STRINGMAP_H
#include "llvm/ADT/StringMap.h"
#endif

#ifndef LLVM_ADT_SMALLVECTOR_H
#include "llvm/ADT/SmallVector.h"
#endif

#ifndef LLVM_ADT_SETVECTOR_H
#include "llvm/ADT/SetVector.h"
#endif

namespace tart {

class ProgramSource;
class Defn;
class Expr;
class LocalScope;

using llvm::StringRef;

/// -------------------------------------------------------------------
/// Scope interface
class Scope {
protected:
  virtual ~Scope() {}

public:
  /** Return the next outer scope. */
  virtual Scope * parentScope() const = 0;

  /** Add a new declaration to this scope. */
  virtual void addMember(Defn * d) = 0;

  /** Find a declaration by name */
  virtual bool lookupMember(StringRef ident, DefnList & defs, bool inherit = false) const = 0;

  /** Convenience function used to look up a member with no overloads. */
  Defn * lookupSingleMember(StringRef ident, bool inherit = false) const;

  /** Return true if this scope allows overloading. Local scopes and parameter scopes do not. */
  virtual bool allowOverloads() { return false; }

  /** Get the base pointer needed to access members found in this scope.  */
  virtual Expr * baseExpr() { return NULL; }

  /** Debugging function to dump the current hierarchy. */
  virtual void dumpHierarchy(bool full = true) const = 0;

  /** Debugging function to dump the scope hierarchy. */
  virtual void dump() const;

  /** Ugly hack - for now */
  virtual LocalScope * asLocalScope() { return NULL; }
};

typedef llvm::SetVector<Scope *> ScopeSet;

/// -------------------------------------------------------------------
/// An implementation of a scope.
class IterableScope : public Scope {
public:
  IterableScope()
      : parentScope_(NULL)
  {}

  IterableScope(Scope * parent)
      : parentScope_(parent)
  {}

  /** Get the scope which encloses this one. */
  Scope * parentScope() const;

  /** Set the scope which encloses this one. */
  void setParentScope(Scope * parent);

  /** Return the first symbol in this scope. */
  Defn * firstMember() const { return members_.first(); }

  /** Return the symbol table entry for the specified symbol name. */
  const SymbolTable::Entry * findSymbol(const char * key) const {
    return members_.findSymbol(key);
  }

  /** Auxiliary scopes associated with this one. */
  const ScopeSet & auxScopes() const { return auxScopes_; }
  ScopeSet & auxScopes() { return auxScopes_; }

  /** Return a reference to the symbol table. */
  const SymbolTable & members() const { return members_; }
  SymbolTable & members() { return members_; }

  // Overrides

  void addMember(Defn * d);
  bool lookupMember(llvm::StringRef ident, DefnList & defs, bool inherit) const;
  bool allowOverloads() { return true; }
  size_t count() { return members_.count(); }
  void clear() { members_.clear(); }
  void trace() const;
  void setScopeName(StringRef name) {
#ifndef NDEBUG
    scopeName_ = name;
#endif
  }
  void dumpHierarchy(bool full) const;

private:
  OrderedSymbolTable members_;
  Scope * parentScope_;
  ScopeSet auxScopes_;

#ifndef NDEBUG
  // For debugging
  llvm::SmallString<32> scopeName_;
#endif
};

/// -------------------------------------------------------------------
/// A block scope
class LocalScope : public GC, public IterableScope {
public:
  LocalScope(Scope * parent) : IterableScope(parent) {
    assert(parent != NULL);
  }

  void addMember(Defn * d);
  void trace() const;
  LocalScope * asLocalScope() { return this; }

private:
};

/// -------------------------------------------------------------------
/// A scope which delegates all methods to another scope. Allows us to
/// non-destructively modify the current scope.
class DelegatingScope : public Scope {
public:
  DelegatingScope(Scope * s, Scope * p) : delegate_(s), parent_(p) {}

  void setDelegate(Scope * scope) { delegate_ = scope; }
  void setParentScope(Scope * scope) { parent_ = scope; }

  Scope * parentScope() const { return parent_; }
  void addMember(Defn * d) { delegate_->addMember(d); }
  bool lookupMember(llvm::StringRef ident, DefnList & defs, bool inherit) const {
    return delegate_->lookupMember(ident, defs, inherit);
  }

  bool allowOverloads() { return delegate_->allowOverloads(); }
  Expr * baseExpr() { return delegate_->baseExpr(); }
  void dumpHierarchy(bool full = true) const { delegate_->dumpHierarchy(); }

private:
  Scope * delegate_;
  Scope * parent_;
};

typedef llvm::SmallVector<LocalScope *, 4> LocalScopeList;

} // namespace tart

#endif
