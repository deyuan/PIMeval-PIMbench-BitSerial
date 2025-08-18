// Test: Generic AAP SEL
// Copyright (c) 2024 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include "libpimeval.h"
#include <iostream>
#include <vector>
#include <cassert>

// Test SEL operation on various PIM architectures
class TestPim {
public:
  TestPim(const std::string& name)
  {
    m_name = name;
    pimCreateDevice(PIM_DEVICE_BITSIMD_V, 1, 1, m_numSubarrayPerBank, m_numRows, m_numCols);
  }
  ~TestPim()
  {
    pimFree(m_obj1);
    pimFree(m_obj2);
    pimFree(m_objSel);
    pimFree(m_objDest);
    pimFree(m_objTmp);
    pimFree(m_objZero);
    pimFree(m_objOne);
    pimDeleteDevice();
  }

  void initTest()
  {
    m_obj1 = pimAlloc(PIM_ALLOC_V1, m_numElements, PIM_INT32); assert(m_obj1 != -1);
    m_obj2 = pimAllocAssociated(m_obj1, PIM_INT32); assert(m_obj2 != -1);
    m_objSel = pimAllocAssociated(m_obj1, PIM_INT32); assert(m_objSel != -1);
    m_objDest = pimAllocAssociated(m_obj1, PIM_INT32); assert(m_objDest != -1);
    m_objTmp = pimAllocAssociated(m_obj1, PIM_INT32); assert(m_objTmp != -1);
    m_objZero = pimAllocAssociated(m_obj1, PIM_INT32); assert(m_objZero != -1);
    m_objOne = pimAllocAssociated(m_obj1, PIM_INT32); assert(m_objOne != -1);
    pimCopyHostToDevice((void*)m_src1.data(), m_obj1);
    pimCopyHostToDevice((void*)m_src2.data(), m_obj2);
    pimCopyHostToDevice((void*)m_srcSel.data(), m_objSel);
    pimCopyHostToDevice((void*)m_zero.data(), m_objZero);
    pimCopyHostToDevice((void*)m_one.data(), m_objOne);
    pimCopyHostToDevice((void*)m_zero.data(), m_objDest);
    pimCopyHostToDevice((void*)m_zero.data(), m_objTmp);
  }

  bool checkResults()
  {
    bool ok = true;
    std::vector<int> dest(m_numElements, 0);
    pimCopyDeviceToHost(m_objDest, (void*)dest.data());
    for (unsigned i = 0; i < m_numElements; ++i) {
      if (dest[i] != (m_srcSel[i] ? m_src1[i] : m_src2[i])) {
        ok = false;
        std::cout << "Result: " << m_srcSel[i] << " " << m_src1[i] << " " << m_src2[i] << " -> " << dest[i] << " error" << std::endl;
      } else {
        std::cout << "Result: " << m_srcSel[i] << " " << m_src1[i] << " " << m_src2[i] << " -> " << dest[i] << " ok" << std::endl;
      }
    }
    if (ok) {
      std::cout << "Test " << m_name << " PASSED!" << std::endl;
      pimShowStats();
    } else {
      std::cout << "Test " << m_name << " FAILED!" << std::endl;
    }
    return ok;
  }

  bool run() {
    initTest();
    runCore();
    bool ok = checkResults();
    return ok;
  }

  virtual void runCore() = 0;

protected:
  std::string m_name;
  unsigned m_numSubarrayPerBank = 8;
  unsigned m_numRows = 256;
  unsigned m_numCols = 256;
  unsigned m_numElements = 8;
  const std::vector<int> m_src1   = {0, 0, 0, 0, 1, 1, 1, 1};
  const std::vector<int> m_src2   = {0, 0, 1, 1, 0, 0, 1, 1};
  const std::vector<int> m_srcSel = {0, 1, 0, 1, 0, 1, 0, 1};
  const std::vector<int> m_zero   = {0, 0, 0, 0, 0, 0, 0, 0};
  const std::vector<int> m_one    = {1, 1, 1, 1, 1, 1, 1, 1};
  PimObjId m_obj1 = -1;
  PimObjId m_obj2 = -1;
  PimObjId m_objSel = -1;
  PimObjId m_objDest = -1;
  PimObjId m_objTmp = -1;
  PimObjId m_objZero = -1;
  PimObjId m_objOne = -1;
};

class TestComputeDRAM : public TestPim {
public:
  TestComputeDRAM() : TestPim("ComputeDRAM") {}
  virtual ~TestComputeDRAM() {}
  virtual void runCore() {
    PimObjId objSelNot = pimAllocAssociated(m_objSel, PIM_INT32); assert(objSelNot != -1);
    PimObjId obj1Not = pimAllocAssociated(m_obj1, PIM_INT32); assert(obj1Not != -1);
    PimObjId obj2Not = pimAllocAssociated(m_obj2, PIM_INT32); assert(obj2Not != -1);
    PimObjId objDestNot = pimAllocAssociated(m_objDest, PIM_INT32); assert(objDestNot != -1);
    pimNot(m_objSel, objSelNot);
    pimNot(m_obj1, obj1Not);
    pimNot(m_obj2, obj2Not);

    // t0 = ROW_CLONE RS1        // seed RS1
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_obj1, 0}}, {{m_objTmp, 0}});
    // t1 = ROW_CLONE SEL        // seed SEL
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objSel, 0}}, {{m_objTmp, 1}});
    // t1 = AND t1, t0           // A = SEL & RS1      (t1,t0 overwritten→A)
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 1}, {m_objTmp, 0}});
    // t2 = ROW_CLONE t1        // spill A
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 1}}, {{m_objTmp, 2}});
    // t0 = ROW_CLONE RS2        // seed RS2
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_obj2, 0}}, {{m_objTmp, 0}});
    // t1 = ROW_CLONE ~SEL       // seed ~SEL 
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objSelNot, 0}}, {{m_objTmp, 1}});
    // t1 = AND t1, t0           // B = ~SEL & RS2      (t1,t0 overwritten→B)
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 1}, {m_objTmp, 0}});
    // t0 = ROW_CLONE t2        //  load A
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 2}}, {{m_objTmp, 0}});
    // t0 = OR  t0, t1          // RD = A | B
    pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 0}, {m_objTmp, 1}});
    // RD = ROW_CLONE t0       //  store result
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{m_objDest, 0}});
    

    // t0 = ROW_CLONE ~RS1        // seed ~RS1
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{obj1Not, 0}}, {{m_objTmp, 0}});
    // t1 = ROW_CLONE SEL        // seed SEL
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objSel, 0}}, {{m_objTmp, 1}});
    // t1 = AND t1, t0           // A = SEL & ~RS1      (t1,t0 overwritten→A)
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 1}, {m_objTmp, 0}});
    // t2 = ROW_CLONE t1        // spill A
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 1}}, {{m_objTmp, 2}});
    // t0 = ROW_CLONE ~RS2        // seed ~RS2
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{obj2Not, 0}}, {{m_objTmp, 0}});
    // t1 = ROW_CLONE ~SEL       // seed ~SEL 
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objSelNot, 0}}, {{m_objTmp, 1}});
    // t1 = AND t1, t0           // B = ~SEL & ~RS2      (t1,t0 overwritten→B)
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 1}, {m_objTmp, 0}});
    // t0 = ROW_CLONE t2        //  load A
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 2}}, {{m_objTmp, 0}});
    // t0 = OR  t0, t1          // RD = A | B
    pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 0}, {m_objTmp, 1}});
    // ~RD = ROW_CLONE t0       //  store result
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{objDestNot, 0}});

  }
private:
  PimObjId m_negObj1 = -1;
};

class TestSIMDRAM : public TestPim {
public:
  TestSIMDRAM() : TestPim("SIMDRAM") {}
  virtual ~TestSIMDRAM() {}
  virtual void runCore() {
    PimObjId objTmpNot = pimCreateDualContactRef(m_objTmp);
    // t1 = AAP(RS2)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_obj2, 0}}, {{m_objTmp, 1}});
    // t2 = AAP(ZERO)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objZero, 0}}, {{m_objTmp, 2}});
    // t3 = AAP(~SEL)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objSel, 0}}, {{m_objTmp, 3}});
    // t4 = AAP(t1, t2, t3, MAJ3) // AND
    pimGenericAAP(PimAnalogOpEnum::MAJ3, {{m_objTmp, 1}, {m_objTmp, 2}, {objTmpNot, 3}}, {{m_objTmp, 4}});
    // t1 = AAP(SEL)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objSel, 0}}, {{m_objTmp, 1}});
    // t2 = AAP(RS1)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_obj1, 0}}, {{m_objTmp, 2}});
    // t3 = AAP(ZERO)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objZero, 0}}, {{m_objTmp, 3}});
    // t5 = AAP(t1, t2, t3, MAJ3) // AND 
    pimGenericAAP(PimAnalogOpEnum::MAJ3, {{m_objTmp, 1}, {m_objTmp, 2}, {m_objTmp, 3}}, {{m_objTmp, 5}});
    // t2 = AAP(ONE)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objOne, 0}}, {{m_objTmp, 2}});
    // RD = AAP(t5, t2, t4, MAJ3) // OR 
    pimGenericAAP(PimAnalogOpEnum::MAJ3, {{m_objTmp, 5}, {m_objTmp, 2}, {m_objTmp, 4}}, {{m_objDest, 0}});
  }
};

class TestReDRAM : public TestPim {
public:
  TestReDRAM() : TestPim("ReDRAM") {}
  virtual ~TestReDRAM() {}
  virtual void runCore() {
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_obj1, 0}},                  {{m_objTmp, 0}}               ); // t0 = src1
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_obj2, 0}},                  {{m_objTmp, 1}, {m_objTmp, 2}}); // t1, t2 = src2
    pimGenericAAP(PimAnalogOpEnum::XOR2,      {{m_objTmp, 0}, {m_objTmp, 1}}                                ); // t0, t1 = xor(t0, t1)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objSel, 0}},                {{m_objTmp, 0}}               ); // t0 = sel
    pimGenericAAP(PimAnalogOpEnum::AND2,      {{m_objTmp, 0}, {m_objTmp, 1}}                                ); // t0, t1 = and(t0, t1)
    pimGenericAAP(PimAnalogOpEnum::XOR2,      {{m_objTmp, 0}, {m_objTmp, 2}}, {{m_objDest, 0}}              ); // dest, t0, t2 = xor(t0, t2)
  }
};

class TestFlexiDRAM: public TestPim {
public:
  TestFlexiDRAM() : TestPim("FlexiDRAM") {}
  virtual ~TestFlexiDRAM() {}
  virtual void runCore() {
    // t1 = ROW_CLONE(RS1)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_obj1, 0}}, {{m_objTmp, 1}});
    // t2 = ROW_CLONE(RS2)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_obj2, 0}}, {{m_objTmp, 2}});
    // t4 = ROW_CLONE(ZERO)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objZero, 0}}, {{m_objTmp, 4}});
    // t3 = XOR3(t1, t2, t4) // t3 = RS1 ⊕ RS2
    pimGenericAAP(PimAnalogOpEnum::XOR3, {{m_objTmp, 1}, {m_objTmp, 2}, {m_objTmp, 4}}, {{m_objTmp, 3}});
    // t0 = ROW_CLONE(SEL)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objSel, 0}}, {{m_objTmp, 0}});
    // t4 = ROW_CLONE(ZERO)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objZero, 0}}, {{m_objTmp, 4}});
    // t1 = MAJ3(t0, t3, t4) // t1 = SEL & (RS1 ⊕ RS2)
    pimGenericAAP(PimAnalogOpEnum::MAJ3, {{m_objTmp, 0}, {m_objTmp, 3}, {m_objTmp, 4}}, {{m_objTmp, 1}});
    // t2 = ROW_CLONE(RS2)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_obj2, 0}}, {{m_objTmp, 2}});
    // t4 = ROW_CLONE(ZERO)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objZero, 0}}, {{m_objTmp, 4}});
    // RD = XOR3(t2, t1, t4) // RD = RS2 ⊕ (SEL & (RS1 ⊕ RS2))
    pimGenericAAP(PimAnalogOpEnum::XOR3, {{m_objTmp, 2}, {m_objTmp, 1}, {m_objTmp, 4}}, {{m_objDest, 0}});
  }
};


class TestDRISA1T1CNor : public TestPim {
public:
  TestDRISA1T1CNor() : TestPim("DRISA-1T1C-nor") {}
  virtual ~TestDRISA1T1CNor() {}
  virtual void runCore() {
    // t0 = NOR(SEL, SEL) -> t0 = NOT(SEL)
    pimOpReadRowToSa(m_objSel, 0);
    pimOpMove(m_objSel, PIM_RREG_SA, PIM_RREG_R1);
    pimOpNor(m_objSel, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA);
    pimOpWriteSaToRow(m_objTmp, 0);

    // t1 = NOR(RS2, RS2) -> t1 = NOT(RS2)
    pimOpReadRowToSa(m_obj2, 0);
    pimOpMove(m_obj2, PIM_RREG_SA, PIM_RREG_R1);
    pimOpNor(m_obj2, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA);
    pimOpWriteSaToRow(m_objTmp, 1);

    // t2 = NOR(t1, SEL) -> t2 = RS2 & ~SEL
    pimOpReadRowToSa(m_objTmp, 1);
    pimOpMove(m_objTmp, PIM_RREG_SA, PIM_RREG_R1);
    pimOpReadRowToSa(m_objSel, 0);
    pimOpNor(m_objTmp, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA);
    pimOpWriteSaToRow(m_objTmp, 2);

    // t3 = NOR(RS1, RS1) -> t3 = NOT(RS1)
    pimOpReadRowToSa(m_obj1, 0);
    pimOpMove(m_obj1, PIM_RREG_SA, PIM_RREG_R1);
    pimOpNor(m_obj1, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA);
    // pimOpWriteSaToRow(m_objTmp, 3);

    // t4 = NOR(t3, t0) -> t4 = RS1 & SEL
    // pimOpReadRowToSa(m_objTmp, 3);
    pimOpMove(m_objTmp, PIM_RREG_SA, PIM_RREG_R1);
    pimOpReadRowToSa(m_objTmp, 0);
    pimOpNor(m_objTmp, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA);
    // pimOpWriteSaToRow(m_objTmp, 4);

    // t1 = NOR(t2, t4) -> t1 = NOT( (RS2&~SEL) OR (RS1&SEL) )
    // pimOpReadRowToSa(m_objTmp, 4);
    pimOpMove(m_objTmp, PIM_RREG_SA, PIM_RREG_R1);
    pimOpReadRowToSa(m_objTmp, 2);
    pimOpNor(m_objTmp, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA);
    // pimOpWriteSaToRow(m_objTmp, 1);

    // RD = NOR(t1, t1) -> RD = (RS2&~SEL) OR (RS1&SEL)
    // pimOpReadRowToSa(m_objTmp, 1);
    pimOpMove(m_objTmp, PIM_RREG_SA, PIM_RREG_R1);
    pimOpNor(m_objTmp, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA);
    pimOpWriteSaToRow(m_objDest, 0);
  }
};

class TestDRISA1T1CMixed : public TestPim {
public:
  TestDRISA1T1CMixed() : TestPim("DRISA-1T1C-mixed") {}
  virtual ~TestDRISA1T1CMixed() {}
  virtual void runCore() {
    // t0 = NOT(SEL)
    pimOpReadRowToSa(m_objSel, 0);
    pimOpNot(m_objSel, PIM_RREG_SA, PIM_RREG_SA);
    // pimOpWriteSaToRow(m_objTmp, 0);

    // t2 = NAND(RS2, t0) // t2 = ¬(RS2 ∧ ¬SEL)
    // pimOpReadRowToSa(m_objTmp, 0);
    pimOpMove(m_obj2, PIM_RREG_SA, PIM_RREG_R1);
    pimOpReadRowToSa(m_obj2, 0);
    pimOpNand(m_obj2, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA);
    pimOpWriteSaToRow(m_objTmp, 2);
    
    // t1 = NAND(RS1, SEL) // t1 = ¬(RS1 ∧ SEL)
    pimOpReadRowToSa(m_obj1, 0);
    pimOpMove(m_obj1, PIM_RREG_SA, PIM_RREG_R1);
    pimOpReadRowToSa(m_objSel, 0);
    pimOpNand(m_obj1, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA);
    // pimOpWriteSaToRow(m_objTmp, 1);

    // RD = NAND(t1, t2) // RD = (RS1 ∧ SEL) ∨ (RS2 ∧ ¬SEL)
    // pimOpReadRowToSa(m_objTmp, 1);
    pimOpMove(m_objTmp, PIM_RREG_SA, PIM_RREG_R1);
    pimOpReadRowToSa(m_objTmp, 2);
    pimOpNand(m_objTmp, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA);
    pimOpWriteSaToRow(m_objDest, 0);
  }
};

int main()
{
  std::cout << "PIM test: Generic AAP" << std::endl;
  // Add scope for ctor/dtor sequence
  {
    TestComputeDRAM testComputeDRAM;
    testComputeDRAM.run();
  }
  {
    TestSIMDRAM testSIMDRAM;
    testSIMDRAM.run();
  }
  {
    TestReDRAM testReDRAM;
    testReDRAM.run();
  }
  {
    TestFlexiDRAM testFlexiDRAM;
    testFlexiDRAM.run();
  }
  {
    TestDRISA1T1CNor testDRISA1T1CNor;
    testDRISA1T1CNor.run();
  }
  {
    TestDRISA1T1CMixed testDRISA1T1CMixed;
    testDRISA1T1CMixed.run();
  }
  return 0;
}

