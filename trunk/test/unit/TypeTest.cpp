/* ================================================================ *
    TART - A Sweet Programming Language.
 * ================================================================ */

#include <gtest/gtest.h>
#include "tart/Defn/TypeDefn.h"
#include "tart/Defn/Module.h"

#include "tart/Type/Type.h"
#include "tart/Type/TypeConversion.h"
#include "tart/Type/TypeRelation.h"
#include "tart/Type/PrimitiveType.h"
#include "tart/Type/TupleType.h"
#include "tart/Type/CompositeType.h"

#include "tart/Common/Diagnostics.h"

#include "FakeSourceFile.h"
#include "TestHelpers.h"

using namespace tart;

class TupleTest : public testing::Test {
public:
  Module * testModule;
  TypeDefn * testTypeDefn;
  Type * testClass;

  QualifiedType intTypes[4];
  QualifiedType shortTypes[4];
  QualifiedType testClassTypes[1];
  QualifiedType intAndTestClass[4];

  TupleTest() {
    // Set up test class.
    testModule = new Module(new SourceFile("TypeTest"), "test");
    testTypeDefn = new TypeDefn(testModule, "test");
    testClass = new CompositeType(Type::Class, testTypeDefn, testModule);
    testTypeDefn->addTrait(Defn::Singular);
    testTypeDefn->setValue(testClass);

    // Set up arrays of types
    intTypes[0] = intTypes[1] = intTypes[2] = intTypes[3] = &Int32Type::instance;
    shortTypes[0] = shortTypes[1] = shortTypes[2] = shortTypes[3] = &Int16Type::instance;
    intAndTestClass[0] = intAndTestClass[2] = &Int32Type::instance;
    intAndTestClass[1] = intAndTestClass[3] = testClassTypes[0] = testClass;
  }
};

TEST_F(TupleTest, FindCommonType) {
  EXPECT_EQ(&Int32Type::instance, findCommonType(&Int32Type::instance, &Int32Type::instance));
  EXPECT_EQ(&Int32Type::instance, findCommonType(&Int32Type::instance, &Int16Type::instance));
}

TEST_F(TupleTest, TypeAttributes) {
}

#if 0
TEST(TypeTest, TypePairTest) {
  typedef TypeTupleKeyInfo TRIPKI;

  TypeTupleKey p0(&Int32Type::instance, &Int32Type::instance + 1);
  TypeTupleKey p1(&Int32Type::instance, &Int32Type::instance + 1);
  TypeTupleKey p2(&Int16Type::instance, &Int16Type::instance + 1);

  ASSERT_TRUE(TRIPKI::getHashValue(p0) == TRIPKI::getHashValue(p1));
  ASSERT_FALSE(TRIPKI::getHashValue(p0) == TRIPKI::getHashValue(p2));
  ASSERT_TRUE(TRIPKI::isEqual(p0, p1));
  ASSERT_FALSE(TRIPKI::isEqual(p0, p2));
}
#endif

TEST_F(TupleTest, TupleTypeTest) {
  //typedef TypeRefIterPairKeyInfo TRIPKI;

  TupleType * t0 = TupleType::get(QualifiedType(&Int32Type::instance));
  TupleType * t1 = TupleType::get(QualifiedType(&Int32Type::instance));
  TupleType * t2 = TupleType::get(QualifiedType(&Int16Type::instance));

  //ASSERT_TRUE(TRIPKI::getHashValue(t0->iterPair()) == TRIPKI::getHashValue(t1->iterPair()));
  //ASSERT_FALSE(TRIPKI::getHashValue(t0->iterPair()) == TRIPKI::getHashValue(t2->iterPair()));
  //ASSERT_TRUE(TRIPKI::isEqual(t0->iterPair(), t1->iterPair()));
  //ASSERT_FALSE(TRIPKI::isEqual(t0->iterPair(), t2->iterPair()));

  ASSERT_TRUE(TupleType::KeyInfo::getHashValue(t0) == TupleType::KeyInfo::getHashValue(t1));
  ASSERT_FALSE(TupleType::KeyInfo::getHashValue(t0) == TupleType::KeyInfo::getHashValue(t2));
  ASSERT_TRUE(TupleType::KeyInfo::isEqual(t0, t1));
  ASSERT_FALSE(TupleType::KeyInfo::isEqual(t0, t2));

  ASSERT_TRUE((void *)t0 == (void *)t1);
  ASSERT_TRUE(t0 == t1);
  ASSERT_FALSE(t0 != t1);
  ASSERT_FALSE((void *)t0 == (void *)t2);
  ASSERT_FALSE(t0 == t2);
  ASSERT_TRUE(t0 != t2);

  ASSERT_EQ(1u, t0->size());
  ASSERT_TRUE(TypeRelation::isEqual((*t0->begin()), &Int32Type::instance));
  ASSERT_TRUE(TypeRelation::isEqual((*t0)[0], &Int32Type::instance));

  ASSERT_TRUE(t0->isSingular());
}

TEST_F(TupleTest, TupleTypeTest2) {
  TupleType * t0 = TupleType::get(&testClassTypes[0], &testClassTypes[1]);
  TupleType * t1 = TupleType::get(&testClassTypes[0], &testClassTypes[1]);

  ASSERT_TRUE(TupleType::KeyInfo::getHashValue(t0) == TupleType::KeyInfo::getHashValue(t1));
  ASSERT_TRUE(TupleType::KeyInfo::isEqual(t0, t1));

  ASSERT_TRUE((void *)t0 == (void *)t1);
  ASSERT_TRUE(t0 == t1);
  ASSERT_FALSE(t0 != t1);
  ASSERT_TRUE((void *)t0 == (void *)t1);

  ASSERT_EQ(1u, t0->size());
  ASSERT_TRUE(TypeRelation::isEqual(t0->member(0), testClass));
  ASSERT_TRUE(t0->isSingular());
}

/*TEST_F(TupleTest, LargeValue) {
  TupleType * t0 = TupleType::get(&testClassTypes[0], &testClassTypes[1]);
  TupleType * t1 = TupleType::get(&testClassTypes[0], &testClassTypes[2]);
  TupleType * t2 = TupleType::get(&testClassTypes[0], &testClassTypes[3]);
  TupleType * t3 = TupleType::get(&testClassTypes[0], &testClassTypes[4]);

  ASSERT_FALSE(t0->isLargeValueType());
  ASSERT_FALSE(t1->isLargeValueType());
  ASSERT_TRUE(t2->isLargeValueType());
  ASSERT_TRUE(t3->isLargeValueType());
}*/

TEST_F(TupleTest, TupleConversion) {
  TupleType * t0 = TupleType::get(&testClassTypes[0], &testClassTypes[1]);
  TupleType * t1 = TupleType::get(&testClassTypes[0], &testClassTypes[1]);
  TupleType * t2 = TupleType::get(&intTypes[0], &intTypes[2]);
  TupleType * t3 = TupleType::get(&shortTypes[0], &shortTypes[2]);

  ASSERT_EQ(IdenticalTypes, TypeConversion::check(t1, t0));
  ASSERT_EQ(IdenticalTypes, TypeConversion::check(t0, t1));
  ASSERT_EQ(ExactConversion, TypeConversion::check(t3, t2));
  ASSERT_EQ(Truncation, TypeConversion::check(t2, t3));
  ASSERT_EQ(Incompatible, TypeConversion::check(t3, t0));
  ASSERT_EQ(Incompatible, TypeConversion::check(t0, t3));
}
