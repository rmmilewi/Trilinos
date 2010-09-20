// @HEADER
// ***********************************************************************
// 
//                    Teuchos: Common Tools Package
//                 Copyright (2004) Sandia Corporation
// 
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
// 
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//  
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//  
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ***********************************************************************
// @HEADER

#include "Teuchos_VerboseObject.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"
#include "Teuchos_StandardDependencies.hpp"
#include "Teuchos_DependencySheet.hpp"
#include "Teuchos_StandardConditions.hpp"
#include "Teuchos_StandardDependencies.hpp"
#include "Teuchos_UnitTestHarness.hpp"
#include "Teuchos_DependencyXMLConverterDB.hpp"
#include "Teuchos_StandardDependencyXMLConverters.hpp"
#include "Teuchos_ParameterList.cpp"


namespace Teuchos{


typedef unsigned short int ushort;
typedef unsigned int uint;
typedef unsigned long int ulong;
typedef std::string myString_t;
#ifdef HAVE_TEUCHOS_LONG_LONG_INT
typedef long long int llint;
typedef unsigned long long int ullint;
#endif


TEUCHOS_UNIT_TEST(Teuchos_Dependencies, StringVisualDepSerialization){
  std::string dependee1 = "string param";
  std::string dependee2 = "string param2";
  std::string dependent1 = "dependent param1";
  std::string dependent2 = "dependent param2";
  ParameterList myDepList("String Visual Dep List");
  RCP<DependencySheet> myDepSheet = rcp(new DependencySheet);
  myDepList.set(dependee1, "val1");
  myDepList.set(dependee2, "val2");
  myDepList.set(dependent1, 1.0);
  myDepList.set(dependent2, 1.0);

  StringVisualDependency::ValueList valList1 = tuple<std::string>("val1");

  RCP<StringVisualDependency> basicStringVisDep = rcp(
    new StringVisualDependency(
      myDepList.getEntryRCP(dependee1),
      myDepList.getEntryRCP(dependent1),
      valList1));

  Dependency::ParameterEntryList dependentList;
  dependentList.insert(myDepList.getEntryRCP(dependent1));
  dependentList.insert(myDepList.getEntryRCP(dependent2));
  StringVisualDependency::ValueList valList2 = 
    tuple<std::string>("val1", "val2");

  RCP<StringVisualDependency> complexStringVisDep = rcp(
    new StringVisualDependency(
      myDepList.getEntryRCP(dependee2),
      dependentList,
      valList2,
      false));

  myDepSheet->addDependency(basicStringVisDep);
  myDepSheet->addDependency(complexStringVisDep);

  RCP<DependencySheet> readInDepSheet = rcp(new DependencySheet);

  XMLParameterListWriter plWriter;
  XMLObject xmlOut = plWriter.toXML(myDepList, myDepSheet);
  out << xmlOut.toString();

  RCP<ParameterList> readInList = 
    writeThenReadPL(myDepList, myDepSheet, readInDepSheet); 

  RCP<ParameterEntry> readinDependee1 = readInList->getEntryRCP(dependee1);
  RCP<ParameterEntry> readinDependent1 = readInList->getEntryRCP(dependent1);
  RCP<ParameterEntry> readinDependee2 = readInList->getEntryRCP(dependee2);
  RCP<ParameterEntry> readinDependent2 = readInList->getEntryRCP(dependent2);
  
  RCP<Dependency> readinDep1 =
    *(readInDepSheet->getDependenciesForParameter(readinDependee1)->begin());

  RCP<Dependency> readinDep2 =
    *(readInDepSheet->getDependenciesForParameter(readinDependee2)->begin());

  std::string stringVisXMLTag = 
    DummyObjectGetter<StringVisualDependency>::getDummyObject()->getTypeAttributeValue();

  TEST_ASSERT(readinDep1->getTypeAttributeValue() == stringVisXMLTag);
  TEST_ASSERT(readinDep2->getTypeAttributeValue() == stringVisXMLTag);

  TEST_ASSERT(readinDep1->getFirstDependee().get() == readinDependee1.get());
  TEST_ASSERT(readinDep1->getDependents().size() == 1);
  TEST_ASSERT((*readinDep1->getDependents().begin()).get() == readinDependent1.get());

  TEST_ASSERT(readinDep2->getFirstDependee().get() == readinDependee2.get());
  TEST_ASSERT(readinDep2->getDependents().size() == 2);
  TEST_ASSERT(
    readinDep2->getDependents().find(readinDependent1) 
    !=
    readinDep2->getDependents().end()
  );
  TEST_ASSERT(
    readinDep2->getDependents().find(readinDependent2)
    !=
    readinDep2->getDependents().end()
  );
    
  RCP<StringVisualDependency> castedDep1 =
    rcp_dynamic_cast<StringVisualDependency>(readinDep1, true);
  RCP<StringVisualDependency> castedDep2 =
    rcp_dynamic_cast<StringVisualDependency>(readinDep2, true);

  TEST_COMPARE_ARRAYS(
    castedDep1->getValues(), basicStringVisDep->getValues());
  TEST_COMPARE_ARRAYS(
    castedDep2->getValues(), complexStringVisDep->getValues());

  TEST_EQUALITY(castedDep1->getShowIf(), basicStringVisDep->getShowIf());
  TEST_EQUALITY(castedDep2->getShowIf(), complexStringVisDep->getShowIf());
}

TEUCHOS_UNIT_TEST(Teuchos_Dependencies, BoolVisualDepSerialization){
  std::string dependee1 = "bool param";
  std::string dependee2 = "bool param2";
  std::string dependent1 = "dependent param1";
  std::string dependent2 = "dependent param2";
  ParameterList myDepList("Bool Visual Dep List");
  RCP<DependencySheet> myDepSheet = rcp(new DependencySheet);
  myDepList.set(dependee1, true);
  myDepList.set(dependee2, true);
  myDepList.set(dependent1, 1.0);
  myDepList.set(dependent2, 1.0);

  RCP<BoolVisualDependency> trueBoolVisDep = rcp(
    new BoolVisualDependency(
      myDepList.getEntryRCP(dependee1),
      myDepList.getEntryRCP(dependent1)));

  Dependency::ParameterEntryList dependentList;
  dependentList.insert(myDepList.getEntryRCP(dependent1));
  dependentList.insert(myDepList.getEntryRCP(dependent2));

  RCP<BoolVisualDependency> falseBoolVisDep = rcp(
    new BoolVisualDependency(
      myDepList.getEntryRCP(dependee2),
      dependentList,
      false));

  myDepSheet->addDependency(trueBoolVisDep);
  myDepSheet->addDependency(falseBoolVisDep);

  RCP<DependencySheet> readInDepSheet = rcp(new DependencySheet);

  XMLParameterListWriter plWriter;
  XMLObject xmlOut = plWriter.toXML(myDepList, myDepSheet);
  out << xmlOut.toString();

  RCP<ParameterList> readInList = 
    writeThenReadPL(myDepList, myDepSheet, readInDepSheet); 

  RCP<ParameterEntry> readinDependee1 = readInList->getEntryRCP(dependee1);
  RCP<ParameterEntry> readinDependent1 = readInList->getEntryRCP(dependent1);
  RCP<ParameterEntry> readinDependee2 = readInList->getEntryRCP(dependee2);
  RCP<ParameterEntry> readinDependent2 = readInList->getEntryRCP(dependent2);
  
  RCP<Dependency> readinDep1 =
    *(readInDepSheet->getDependenciesForParameter(readinDependee1)->begin());

  RCP<Dependency> readinDep2 =
    *(readInDepSheet->getDependenciesForParameter(readinDependee2)->begin());

  std::string boolVisXMLTag = 
    DummyObjectGetter<BoolVisualDependency>::getDummyObject()->getTypeAttributeValue();

  TEST_ASSERT(readinDep1->getTypeAttributeValue() == boolVisXMLTag);
  TEST_ASSERT(readinDep2->getTypeAttributeValue() == boolVisXMLTag);

  TEST_ASSERT(readinDep1->getFirstDependee().get() == readinDependee1.get());
  TEST_ASSERT(readinDep1->getDependents().size() == 1);
  TEST_ASSERT((*readinDep1->getDependents().begin()).get() == readinDependent1.get());

  TEST_ASSERT(readinDep2->getFirstDependee().get() == readinDependee2.get());
  TEST_ASSERT(readinDep2->getDependents().size() == 2);
  TEST_ASSERT(
    readinDep2->getDependents().find(readinDependent1) 
    !=
    readinDep2->getDependents().end()
  );
  TEST_ASSERT(
    readinDep2->getDependents().find(readinDependent2)
    !=
    readinDep2->getDependents().end()
  );
    
  RCP<BoolVisualDependency> castedDep1 =
    rcp_dynamic_cast<BoolVisualDependency>(readinDep1, true);
  RCP<BoolVisualDependency> castedDep2 =
    rcp_dynamic_cast<BoolVisualDependency>(readinDep2, true);

  TEST_EQUALITY(castedDep1->getShowIf(), trueBoolVisDep->getShowIf());
  TEST_EQUALITY(castedDep2->getShowIf(), falseBoolVisDep->getShowIf());
}

TEUCHOS_UNIT_TEST_TEMPLATE_1_DECL(
  Teuchos_Dependencies, 
  NumberVisualDepSerialization, 
  T)
{
  std::string dependee1 = "num param";
  std::string dependee2 = "num param2";
  std::string dependent1 = "dependent param1";
  std::string dependent2 = "dependent param2";
  ParameterList myDepList("Number Visual Dep List");
  RCP<DependencySheet> myDepSheet = rcp(new DependencySheet);
  myDepList.set(dependee1, ScalarTraits<T>::one());
  myDepList.set(dependee2, ScalarTraits<T>::one());
  myDepList.set(dependent1, true);
  myDepList.set(dependent2, "vale");

  RCP<NumberVisualDependency<T> > simpleNumVisDep = rcp(
    new NumberVisualDependency<T>(
      myDepList.getEntryRCP(dependee1),
      myDepList.getEntryRCP(dependent1)));

  Dependency::ParameterEntryList dependentList;
  dependentList.insert(myDepList.getEntryRCP(dependent1));
  dependentList.insert(myDepList.getEntryRCP(dependent2));

  RCP<NumberVisualDependency<T> > complexNumVisDep = rcp(
    new NumberVisualDependency<T>(
      myDepList.getEntryRCP(dependee2),
      dependentList));

  myDepSheet->addDependency(simpleNumVisDep);
  myDepSheet->addDependency(complexNumVisDep);

  RCP<DependencySheet> readInDepSheet = rcp(new DependencySheet);

  XMLParameterListWriter plWriter;
  XMLObject xmlOut = plWriter.toXML(myDepList, myDepSheet);
  out << xmlOut.toString();

  RCP<ParameterList> readInList = 
    writeThenReadPL(myDepList, myDepSheet, readInDepSheet); 

  RCP<ParameterEntry> readinDependee1 = readInList->getEntryRCP(dependee1);
  RCP<ParameterEntry> readinDependent1 = readInList->getEntryRCP(dependent1);
  RCP<ParameterEntry> readinDependee2 = readInList->getEntryRCP(dependee2);
  RCP<ParameterEntry> readinDependent2 = readInList->getEntryRCP(dependent2);
  
  RCP<Dependency> readinDep1 =
    *(readInDepSheet->getDependenciesForParameter(readinDependee1)->begin());

  RCP<Dependency> readinDep2 =
    *(readInDepSheet->getDependenciesForParameter(readinDependee2)->begin());

  std::string numVisXMLTag = 
    DummyObjectGetter<NumberVisualDependency<T> >::getDummyObject()->getTypeAttributeValue();

  TEST_ASSERT(readinDep1->getTypeAttributeValue() == numVisXMLTag);
  TEST_ASSERT(readinDep2->getTypeAttributeValue() == numVisXMLTag);

  TEST_ASSERT(readinDep1->getFirstDependee().get() == readinDependee1.get());
  TEST_ASSERT(readinDep1->getDependents().size() == 1);
  TEST_ASSERT((*readinDep1->getDependents().begin()).get() == readinDependent1.get());

  TEST_ASSERT(readinDep2->getFirstDependee().get() == readinDependee2.get());
  TEST_ASSERT(readinDep2->getDependents().size() == 2);
  TEST_ASSERT(
    readinDep2->getDependents().find(readinDependent1) 
    !=
    readinDep2->getDependents().end()
  );
  TEST_ASSERT(
    readinDep2->getDependents().find(readinDependent2)
    !=
    readinDep2->getDependents().end()
  );
    
  RCP<NumberVisualDependency<T> > castedDep1 =
    rcp_dynamic_cast<NumberVisualDependency<T> >(readinDep1, true);
  RCP<NumberVisualDependency<T> > castedDep2 =
    rcp_dynamic_cast<NumberVisualDependency<T> >(readinDep2, true);

  TEST_EQUALITY(castedDep1->getShowIf(), simpleNumVisDep->getShowIf());
  TEST_EQUALITY(castedDep2->getShowIf(), complexNumVisDep->getShowIf());
}

#define NUMBER_VIS_TEST(T) \
TEUCHOS_UNIT_TEST_TEMPLATE_1_INSTANT( \
  Teuchos_Dependencies, NumberVisualDepSerialization, T)

NUMBER_VIS_TEST(int)
NUMBER_VIS_TEST(uint)
NUMBER_VIS_TEST(short)
NUMBER_VIS_TEST(ushort)
NUMBER_VIS_TEST(long)
NUMBER_VIS_TEST(ulong)
NUMBER_VIS_TEST(float)
NUMBER_VIS_TEST(double)
#ifdef HAVE_TEUCHOS_LONG_LONG_INT
NUMBER_VIS_TEST(llint)
NUMBER_VIS_TEST(ullint)
#endif

TEUCHOS_UNIT_TEST(Teuchos_Dependencies, ConditionVisualDepSerialization){
  std::string dependee1 = "string param";
  std::string dependee2 = "bool param";
  std::string dependee3 = "int param";
  std::string dependent1 = "dependent param1";
  std::string dependent2 = "dependent param2";
  std::string dependent3 = "dependent param3";
  ParameterList myDepList("Condition Visual Dep List");
  RCP<DependencySheet> myDepSheet = rcp(new DependencySheet);
  myDepList.set(dependee1, "val1");
  myDepList.set(dependee2, true);
  myDepList.set(dependee3, 1);
  myDepList.set(dependent1, 1.0);
  myDepList.set(dependent2, 1.0);
  myDepList.set(dependent3, (float)1.0);

  StringCondition::ValueList conditionVal1 = 
    tuple<std::string>("steve", "blah", "your face");
  RCP<StringCondition> stringCon = 
    rcp(new StringCondition(
      myDepList.getEntryRCP(dependee1), conditionVal1, false));

  RCP<BoolCondition> boolCon = 
    rcp(new BoolCondition(myDepList.getEntryRCP(dependee2)));

  RCP<NumberCondition<int> > numberCon = 
    rcp(new NumberCondition<int>(myDepList.getEntryRCP(dependee3)));

  Condition::ConstConditionList conList = 
    tuple<RCP<const Condition> >(boolCon, numberCon);

  RCP<AndCondition> andCon = rcp(new AndCondition(conList));

  RCP<ConditionVisualDependency> simpleConVisDep = rcp(
    new ConditionVisualDependency(
      stringCon,
      myDepList.getEntryRCP(dependent1)));

  Dependency::ParameterEntryList dependentList;
  dependentList.insert(myDepList.getEntryRCP(dependent2));
  dependentList.insert(myDepList.getEntryRCP(dependent3));

  RCP<ConditionVisualDependency> complexConVisDep = rcp(
    new ConditionVisualDependency(
      andCon,
      dependentList,
      false));

  myDepSheet->addDependency(simpleConVisDep);
  myDepSheet->addDependency(complexConVisDep);

  RCP<DependencySheet> readInDepSheet = rcp(new DependencySheet);

  XMLParameterListWriter plWriter;
  XMLObject xmlOut = plWriter.toXML(myDepList, myDepSheet);
  out << xmlOut.toString();

  RCP<ParameterList> readInList = 
    writeThenReadPL(myDepList, myDepSheet, readInDepSheet); 

  RCP<ParameterEntry> readinDependee1 = readInList->getEntryRCP(dependee1);
  RCP<ParameterEntry> readinDependent1 = readInList->getEntryRCP(dependent1);
  RCP<ParameterEntry> readinDependee2 = readInList->getEntryRCP(dependee2);
  RCP<ParameterEntry> readinDependent2 = readInList->getEntryRCP(dependent2);
  RCP<ParameterEntry> readinDependee3 = readInList->getEntryRCP(dependee3);
  RCP<ParameterEntry> readinDependent3 = readInList->getEntryRCP(dependent3);
  
  RCP<Dependency> readinDep1 =
    *(readInDepSheet->getDependenciesForParameter(readinDependee1)->begin());

  RCP<Dependency> readinDep2 =
    *(readInDepSheet->getDependenciesForParameter(readinDependee2)->begin());

  RCP<Dependency> readinDep3 =
    *(readInDepSheet->getDependenciesForParameter(readinDependee3)->begin());

  std::string conVisXMLTag = 
    DummyObjectGetter<ConditionVisualDependency>::getDummyObject()->getTypeAttributeValue();

  TEST_ASSERT(readinDep1->getTypeAttributeValue() == conVisXMLTag);
  TEST_ASSERT(readinDep2->getTypeAttributeValue() == conVisXMLTag);
  TEST_ASSERT(readinDep3->getTypeAttributeValue() == conVisXMLTag);

  TEST_ASSERT(readinDep1->getFirstDependee().get() == readinDependee1.get());
  TEST_ASSERT(readinDep1->getDependents().size() == 1);
  TEST_ASSERT((*readinDep1->getDependents().begin()).get() 
    == readinDependent1.get());

  TEST_ASSERT(readinDep2.get() == readinDep3.get());
  TEST_ASSERT(readinDep2->getDependees().size() == 2);
  TEST_ASSERT(
    readinDep2->getDependees().find(readinDependee2) 
    !=
    readinDep2->getDependees().end());
  TEST_ASSERT(
    readinDep2->getDependees().find(readinDependee3) 
    !=
    readinDep2->getDependees().end());

  TEST_ASSERT(readinDep2->getDependents().size() == 2);
  TEST_ASSERT(
    readinDep2->getDependents().find(readinDependent2) 
    !=
    readinDep2->getDependents().end()
  );
  TEST_ASSERT(
    readinDep2->getDependents().find(readinDependent3)
    !=
    readinDep2->getDependents().end()
  );
    
  RCP<ConditionVisualDependency> castedDep1 =
    rcp_dynamic_cast<ConditionVisualDependency>(readinDep1, true);
  RCP<ConditionVisualDependency> castedDep2 =
    rcp_dynamic_cast<ConditionVisualDependency>(readinDep2, true);

  TEST_EQUALITY(castedDep1->getShowIf(), simpleConVisDep->getShowIf());
  TEST_EQUALITY(castedDep2->getShowIf(), complexConVisDep->getShowIf());

  TEST_EQUALITY(castedDep1->getCondition()->getTypeAttributeValue(),
    simpleConVisDep->getCondition()->getTypeAttributeValue());
  TEST_EQUALITY(castedDep2->getCondition()->getTypeAttributeValue(), 
    complexConVisDep->getCondition()->getTypeAttributeValue());
}

TEUCHOS_UNIT_TEST_TEMPLATE_2_DECL(
  Teuchos_Dependencies, 
  NumberArrayLengthDepSerialization, 
  DependeeType,
  DependentType)
{
  std::string dependee1 = "dependee param";
  std::string dependee2 = "dependee param2";
  std::string dependent1 = "dependent param1";
  std::string dependent2 = "dependent param2";
  ParameterList myDepList("Number Array LenthDep List");
  RCP<DependencySheet> myDepSheet = rcp(new DependencySheet);
  myDepList.set(dependee1, ScalarTraits<DependeeType>::one());
  myDepList.set(dependee2, ScalarTraits<DependeeType>::one());
  myDepList.set(dependent1, Array<DependentType>(8));
  myDepList.set(dependent2, Array<DependentType>(5));


  RCP<NumberArrayLengthDependency<DependeeType, DependentType> > basicArrayDep =
    rcp(new NumberArrayLengthDependency<DependeeType, DependentType>(
      myDepList.getEntryRCP(dependee1),
      myDepList.getEntryRCP(dependent1)));

  myDepSheet->addDependency(basicArrayDep);

  RCP<DependencySheet> readInDepSheet = rcp(new DependencySheet);

  XMLParameterListWriter plWriter;
  XMLObject xmlOut = plWriter.toXML(myDepList, myDepSheet);
  out << xmlOut.toString();

  RCP<ParameterList> readInList = 
    writeThenReadPL(myDepList, myDepSheet, readInDepSheet); 

  RCP<ParameterEntry> readinDependee1 = readInList->getEntryRCP(dependee1);
  RCP<ParameterEntry> readinDependent1 = readInList->getEntryRCP(dependent1);
  RCP<ParameterEntry> readinDependee2 = readInList->getEntryRCP(dependee2);
  RCP<ParameterEntry> readinDependent2 = readInList->getEntryRCP(dependent2);
  
  RCP<Dependency> readinDep1 =
    *(readInDepSheet->getDependenciesForParameter(readinDependee1)->begin());

  std::string arrayLengthXMLTag = 
    DummyObjectGetter<NumberArrayLengthDependency<DependeeType, DependentType> >::getDummyObject()->getTypeAttributeValue();

  TEST_ASSERT(readinDep1->getTypeAttributeValue() == arrayLengthXMLTag);

  TEST_ASSERT(readinDep1->getFirstDependee().get() == readinDependee1.get());
  TEST_ASSERT(readinDep1->getDependents().size() == 1);
  TEST_ASSERT((*readinDep1->getDependents().begin()).get() == readinDependent1.get());

  RCP<NumberArrayLengthDependency<DependeeType, DependentType> > castedDep1 =
    rcp_dynamic_cast<NumberArrayLengthDependency<DependeeType, DependentType> >(
      readinDep1, true);
}

#define NUM_ARRAY_LENGTH_TEST(DependeeType, DependentType) \
TEUCHOS_UNIT_TEST_TEMPLATE_2_INSTANT( \
  Teuchos_Dependencies, \
  NumberArrayLengthDepSerialization, \
  DependeeType, \
  DependentType) \

// Need to fix array serialization so we can test this with
// a dependent type of strings. Right now an array of emptyr strings does not
// seralize correctly
// KLN 09.17/2010
#ifdef HAVE_TEUCHOS_LONG_LONG_INT
#define NUM_ARRAY_LENGTH_TEST_GROUP(DependeeType) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, int) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, short) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, uint) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, ushort) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, long) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, ulong) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, double) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, float) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, llint) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, ullint)
#else
#define NUMBER_VIS_TEST_GROUP(DependeeType) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, int) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, short) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, uint) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, ushort) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, long) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, ulong) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, double) \
  NUM_ARRAY_LENGTH_TEST(DependeeType, float)
#endif

NUM_ARRAY_LENGTH_TEST_GROUP(int)
NUM_ARRAY_LENGTH_TEST_GROUP(short)
NUM_ARRAY_LENGTH_TEST_GROUP(uint)
NUM_ARRAY_LENGTH_TEST_GROUP(ushort)
NUM_ARRAY_LENGTH_TEST_GROUP(long)
NUM_ARRAY_LENGTH_TEST_GROUP(ulong)
NUM_ARRAY_LENGTH_TEST_GROUP(double)
NUM_ARRAY_LENGTH_TEST_GROUP(float)
#ifdef HAVE_TEUCHOS_LONG_LONG_INT
NUM_ARRAY_LENGTH_TEST_GROUP(llint)
NUM_ARRAY_LENGTH_TEST_GROUP(ullint)
#endif

TEUCHOS_UNIT_TEST(Teuchos_Dependencies, StringValidatorDepSerialization){
  std::string dependee1 = "string param";
  std::string dependee2 = "string param2";
  std::string dependent1 = "dependent param1";
  std::string dependent2 = "dependent param2";
  ParameterList myDepList("String Vali Dep List");
  RCP<DependencySheet> myDepSheet = rcp(new DependencySheet);
  myDepList.set(dependee1, "val1");
  myDepList.set(dependee2, "val2");
  myDepList.set(dependent1, 2.0);
  myDepList.set(dependent2, 3.0);

	RCP<EnhancedNumberValidator<double> > double1Vali =
    rcp(new EnhancedNumberValidator<double>(0,10));

	RCP<EnhancedNumberValidator<double> > double2Vali =
    rcp(new EnhancedNumberValidator<double>(0,30));

	RCP<EnhancedNumberValidator<double> > defaultVali =
    rcp(new EnhancedNumberValidator<double>(4,90));

  StringValidatorDependency::ValueToValidatorMap valuesAndValidators;
  valuesAndValidators["val1"] = double1Vali;
  valuesAndValidators["val2"] = double2Vali;

  RCP<StringValidatorDependency> basicStringValiDep = rcp(
    new StringValidatorDependency(
      myDepList.getEntryRCP(dependee1),
      myDepList.getEntryRCP(dependent1),
      valuesAndValidators));

  Dependency::ParameterEntryList dependentList;
  dependentList.insert(myDepList.getEntryRCP(dependent1));
  dependentList.insert(myDepList.getEntryRCP(dependent2));

  RCP<StringValidatorDependency> complexStringValiDep = rcp(
    new StringValidatorDependency(
      myDepList.getEntryRCP(dependee2),
      dependentList,
      valuesAndValidators,
      defaultVali));

  myDepSheet->addDependency(basicStringValiDep);
  myDepSheet->addDependency(complexStringValiDep);

  RCP<DependencySheet> readInDepSheet = rcp(new DependencySheet);

  XMLParameterListWriter plWriter;
  XMLObject xmlOut = plWriter.toXML(myDepList, myDepSheet);
  out << xmlOut.toString();

  RCP<ParameterList> readInList = 
    writeThenReadPL(myDepList, myDepSheet, readInDepSheet); 

  RCP<ParameterEntry> readinDependee1 = readInList->getEntryRCP(dependee1);
  RCP<ParameterEntry> readinDependent1 = readInList->getEntryRCP(dependent1);
  RCP<ParameterEntry> readinDependee2 = readInList->getEntryRCP(dependee2);
  RCP<ParameterEntry> readinDependent2 = readInList->getEntryRCP(dependent2);
  
  RCP<Dependency> readinDep1 =
    *(readInDepSheet->getDependenciesForParameter(readinDependee1)->begin());

  RCP<Dependency> readinDep2 =
    *(readInDepSheet->getDependenciesForParameter(readinDependee2)->begin());

  std::string stringValiXMLTag = 
    DummyObjectGetter<StringValidatorDependency>::getDummyObject()->getTypeAttributeValue();

  TEST_ASSERT(readinDep1->getTypeAttributeValue() == stringValiXMLTag);
  TEST_ASSERT(readinDep2->getTypeAttributeValue() == stringValiXMLTag);

  TEST_ASSERT(readinDep1->getFirstDependee().get() == readinDependee1.get());
  TEST_ASSERT(readinDep1->getDependents().size() == 1);
  TEST_ASSERT((*readinDep1->getDependents().begin()).get() 
    == readinDependent1.get());

  TEST_ASSERT(readinDep2->getFirstDependee().get() == readinDependee2.get());
  TEST_ASSERT(readinDep2->getDependents().size() == 2);
  TEST_ASSERT(
    readinDep2->getDependents().find(readinDependent1) 
    !=
    readinDep2->getDependents().end()
  );
  TEST_ASSERT(
    readinDep2->getDependents().find(readinDependent2)
    !=
    readinDep2->getDependents().end()
  );
    
  RCP<StringValidatorDependency> castedDep1 =
    rcp_dynamic_cast<StringValidatorDependency>(readinDep1, true);
  RCP<StringValidatorDependency> castedDep2 =
    rcp_dynamic_cast<StringValidatorDependency>(readinDep2, true);

  TEST_ASSERT(castedDep1->getValuesAndValidators().size() == 2);
  TEST_ASSERT(castedDep2->getValuesAndValidators().size() == 2);
  TEST_ASSERT(castedDep1->getDefaultValidator().is_null());
  TEST_ASSERT(nonnull(castedDep2->getDefaultValidator()));

}




} //namespace Teuchos

