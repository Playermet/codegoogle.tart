/* ================================================================ *
    TART - A Sweet Programming Language.
 * ================================================================ */

#include <gtest/gtest.h>

#include "tart/Type/CompositeType.h"
#include "tart/Defn/Module.h"
#include "tart/Type/PrimitiveType.h"
#include "tart/Defn/TypeDefn.h"
#include "tart/Type/TupleType.h"
#include "tart/Type/UnionType.h"

#include "FakeSourceFile.h"
#include "TestHelpers.h"

namespace {
using namespace tart;

class UnionTest : public testing::Test {
protected:
  SourceFile testSource;
  Module testModule;
  Type * testClass;

  UnionTest() : testSource(""), testModule(&testSource, "test") {
    // Set up test class.
    TypeDefn * testClassDef = new TypeDefn(NULL, "Test");
    testClass = new CompositeType(Type::Class, testClassDef, &testModule);
    testClassDef->addTrait(Defn::Singular);
    testClassDef->setValue(testClass);
  }
};

TEST_F(UnionTest, CreateUnion) {
  QualifiedTypeList memberTypes;
  memberTypes.push_back(&Int32Type::instance);
  memberTypes.push_back(&FloatType::instance);
  UnionType * utype = UnionType::get(memberTypes);

  ASSERT_EQ(2u, utype->members().size());
  ASSERT_EQ(2u, utype->typeArgs()->size());
  ASSERT_EQ(2u, utype->numTypeParams());
  ASSERT_EQ(QualifiedType(&Int32Type::instance), utype->typeParam(0));
  ASSERT_EQ(QualifiedType(&FloatType::instance), utype->typeParam(1));
  ASSERT_FALSE(utype->hasVoidType());
  ASSERT_FALSE(utype->hasNullType());
  ASSERT_FALSE(utype->isSingleOptionalType());
  ASSERT_FALSE(utype->hasRefTypesOnly());
  ASSERT_EQ(&Int32Type::instance, utype->getFirstNonVoidType());
  ASSERT_TRUE(utype->isSingular());
}

TEST_F(UnionTest, GetTypeIndex) {
  QualifiedTypeList memberTypes;
  memberTypes.push_back(&Int32Type::instance);
  memberTypes.push_back(&FloatType::instance);
  UnionType * utype = UnionType::get(memberTypes);
  ASSERT_EQ(0, utype->getTypeIndex(&Int32Type::instance));
  ASSERT_EQ(1, utype->getTypeIndex(&FloatType::instance));
  ASSERT_EQ(&Int32Type::instance, utype->getFirstNonVoidType());
}

TEST_F(UnionTest, GetTypeSorting) {
  QualifiedTypeList memberTypes;
  memberTypes.push_back(&FloatType::instance);
  memberTypes.push_back(&Int32Type::instance);
  UnionType * utype = UnionType::get(memberTypes);
  ASSERT_EQ(0, utype->getTypeIndex(&Int32Type::instance));
  ASSERT_EQ(1, utype->getTypeIndex(&FloatType::instance));
  ASSERT_EQ(&Int32Type::instance, utype->getFirstNonVoidType());
}

TEST_F(UnionTest, FloatOrVoidUnion) {
  QualifiedTypeList memberTypes;
  memberTypes.push_back(&FloatType::instance);
  memberTypes.push_back(&VoidType::instance);
  UnionType * utype = UnionType::get(memberTypes);
  ASSERT_TRUE(utype->hasVoidType());
  ASSERT_FALSE(utype->hasNullType());
  ASSERT_TRUE(utype->isSingleOptionalType());
  ASSERT_FALSE(utype->hasRefTypesOnly());
  ASSERT_EQ(&FloatType::instance, utype->getFirstNonVoidType());
  ASSERT_TRUE(utype->isSingular());
}

TEST_F(UnionTest, IntOrRefTypeUnion) {
  QualifiedTypeList memberTypes;
  memberTypes.push_back(&Int32Type::instance);
  memberTypes.push_back(testClass);
  UnionType * utype = UnionType::get(memberTypes);
  ASSERT_FALSE(utype->hasVoidType());
  ASSERT_FALSE(utype->hasNullType());
  ASSERT_FALSE(utype->isSingleOptionalType());
  ASSERT_FALSE(utype->hasRefTypesOnly());
  ASSERT_EQ(testClass, utype->getFirstNonVoidType());
  ASSERT_TRUE(utype->isSingular());
}

TEST_F(UnionTest, NullOrRefTypeUnion) {
  QualifiedTypeList memberTypes;
  memberTypes.push_back(&NullType::instance);
  memberTypes.push_back(testClass);
  UnionType * utype = UnionType::get(memberTypes);
  ASSERT_FALSE(utype->hasVoidType());
  ASSERT_TRUE(utype->hasNullType());
  ASSERT_TRUE(utype->isSingleOptionalType());
  ASSERT_TRUE(utype->hasRefTypesOnly());
  ASSERT_EQ(testClass, utype->getFirstNonVoidType());
  ASSERT_TRUE(utype->isSingular());
}

TEST_F(UnionTest, Coalesce) {
  QualifiedTypeList memberTypes;
  memberTypes.push_back(&Int32Type::instance);
  memberTypes.push_back(&FloatType::instance);
  UnionType * utype1 = UnionType::get(memberTypes);
  UnionType * utype2 = UnionType::get(memberTypes);
  ASSERT_TRUE(utype1 == utype2);
}

}
