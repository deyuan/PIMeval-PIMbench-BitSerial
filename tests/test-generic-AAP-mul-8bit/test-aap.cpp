// Test: Generic AAP Addition
// Copyright (c) 2024 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include "libpimeval.h"
#include <iostream>
#include <vector>
#include <cassert>
// Test 8-bit multiplier operation on various PIM architectures
class TestPim {
public:
  TestPim(const std::string& name)
  {
    m_name = name;
    pimCreateDevice(PIM_DEVICE_BITSIMD_V, 1, 1, m_numSubarrayPerBank, m_numRows, m_numCols);
  }
  ~TestPim()
  {
    pimFree(m_objA);
    pimFree(m_objB);
    pimFree(m_objRes);
    pimFree(m_objTmp);
    pimFree(m_objZero);
    pimFree(m_objOne);
    pimDeleteDevice();
  }

  void initTest()
  {
    m_objA    = pimAlloc(PIM_ALLOC_V1, m_numElements, PIM_UINT8); assert(m_objA != -1);
    m_objB    = pimAllocAssociated(m_objA, PIM_UINT8);       assert(m_objB != -1);
    // result is a 16-bit product for 8x8 multiplier
    m_objRes  = pimAllocAssociated(m_objA, PIM_UINT16);      assert(m_objRes != -1);
    m_objTmp  = pimAllocAssociated(m_objA, PIM_UINT32);      assert(m_objTmp != -1);
    m_objZero = pimAllocAssociated(m_objA, PIM_UINT8);       assert(m_objZero != -1);
    m_objOne  = pimAllocAssociated(m_objA, PIM_UINT8);       assert(m_objOne != -1);

    pimCopyHostToDevice((void*)m_a.data(), m_objA);
    pimCopyHostToDevice((void*)m_b.data(), m_objB);
    pimCopyHostToDevice((void*)m_zero.data(), m_objZero);
    pimCopyHostToDevice((void*)m_one.data(), m_objOne);

    // initialize result and tmp to zero on device
    std::vector<uint16_t> zero16(m_numElements, 0);
    pimCopyHostToDevice((void*)zero16.data(), m_objRes);
    std::vector<uint32_t> zero32(m_numElements, 0);
    pimCopyHostToDevice((void*)zero32.data(), m_objTmp);
  }

  bool checkResults()
  {
    bool ok = true;
    std::vector<uint16_t> resRes(m_numElements, 0);
    pimCopyDeviceToHost(m_objRes, (void*)resRes.data());
    for (unsigned i = 0; i < m_numElements; ++i) {
      uint8_t a = m_a[i];
      uint8_t b = m_b[i];
      uint16_t expect = static_cast<uint16_t>(uint16_t(a) * uint16_t(b)); // full 16-bit product
      if (resRes[i] != expect) {
        ok = false;
        std::cout << "Input: " << int(a) << " " << int(b)
                  << " -> got(res): " << int(resRes[i])
                  << " expected: " << int(expect) << " ERROR" << std::endl;
      } else {
        std::cout << "Input: " << int(a) << " " << int(b)
                  << " -> got(res): " << int(resRes[i])
                  << " OK" << std::endl;
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
  unsigned m_numBits = 8; 

  // Inputs: a, b (8-bit values per element)
  const std::vector<uint8_t> m_a = {0x00, 0x01, 0x7F, 0x80, 0xFF, 0x12, 0x34, 0xAA};
  const std::vector<uint8_t> m_b = {0x00, 0x02, 0x01, 0x80, 0x01, 0x22, 0x10, 0x55};

  // Utility vectors
  const std::vector<uint8_t> m_zero = {0,0,0,0,0,0,0,0};
  const std::vector<uint8_t> m_one  = {1,1,1,1,1,1,1,1};

  // Device object IDs
  PimObjId m_objA    = -1;
  PimObjId m_objB    = -1;
  PimObjId m_objRes  = -1; // renamed from m_objSum, 16-bit result
  PimObjId m_objTmp  = -1;
  PimObjId m_objZero = -1;
  PimObjId m_objOne  = -1;
};

class TestComputeDRAM : public TestPim {
public:
  TestComputeDRAM() : TestPim("ComputeDRAM") {}
  virtual ~TestComputeDRAM() {}
  virtual void runCore() {
  }
private:
};

class TestSIMDRAM : public TestPim {
public:
  TestSIMDRAM() : TestPim("SIMDRAM") {}
  virtual ~TestSIMDRAM() {}
  virtual void runCore() {
    PimObjId objDCC = pimAllocAssociated(m_objTmp, PIM_INT32); assert(objDCC != -1);
    PimObjId objDCCN = pimCreateDualContactRef(objDCC);
    // Ref: MIMDRAM/microworkloads/11_multu-plus.c
    
    /*unsigned *v1 = VECTOR(vals1[0]);*/
    for (unsigned k = 0; k < m_numBits; ++k) {
      /*unsigned *v2 = VECTOR(vals2[k]);*/
      /*unsigned *out = VECTOR(output[k]);*/
      /*AAP_VECTORS (B_T0, v1        )*/
      pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objA, 0}}, {{m_objTmp, 0}});

      /*AAP_VECTORS (B_T1, v2        )*/
      pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, k}}, {{m_objTmp, 1}});

      /*AAP_VECTORS (B_T2, C_0       )*/
      pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objZero, 0}}, {{m_objTmp, 2}});

      /*AAP_VECTORS (out , B_T0_T1_T2)*/
      pimGenericAAP(PimAnalogOpEnum::MAJ3, {{m_objTmp, 0}, {m_objTmp, 1}, {m_objTmp, 2}}, {{m_objRes, k}});
    }

    /*AAP_VECTORS (VECTOR(output[col_length]), C_0);*/
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objZero, 0}}, {{m_objRes, m_numBits}});

    for (unsigned j = 1; j < m_numBits; ++j) {
      /*unsigned *v1 = VECTOR(vals1[j]);*/

      /*AAP_VECTORS (B_DCC1, C_0)*/
      pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objZero, 0}}, {{objDCC, 1}});

      for (unsigned k = 0; k < m_numBits; ++k) {
        /*unsigned *v2 = VECTOR(vals2[k]);*/
        /*unsigned *out = VECTOR(output[j + k]);*/
        /*AAP_VECTORS (B_T0        , v1        )*/
        pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objA, j}}, {{m_objTmp, 0}});

        /*AAP_VECTORS (B_T1        , v2        )*/
        pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, k}}, {{m_objTmp, 1}});

        /*AAP_VECTORS (B_T2        , C_0       )*/
        pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objZero, 0}}, {{m_objTmp, 2}});

        /*AP_VECTOR   (B_T0_T1_T2              )*/
        pimGenericAAP(PimAnalogOpEnum::MAJ3, {{m_objTmp, 0}, {m_objTmp, 1}, {m_objTmp, 2}});

        /*AAP_VECTORS (B_T2_T3     , B_DCC1    )*/
        pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objDCC, 1}}, {{m_objTmp, 2}, {m_objTmp, 3}});

        /*AAP_VECTORS (B_DCC1      , out       )*/
        pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objRes, j + k}}, {{objDCC, 1}});

        /*AP_VECTOR   (B_DCC1_T0_T3            )*/
        pimGenericAAP(PimAnalogOpEnum::MAJ3, {{objDCC, 1}, {m_objTmp, 0}, {m_objTmp, 3}});

        /*AAP_VECTORS (B_T0_T3     , B_DCC1N   )*/
        pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objDCCN, 1}}, {{m_objTmp, 0}, {m_objTmp, 3}});

        /*AP_VECTOR   (B_T0_T1_T2              )*/
        pimGenericAAP(PimAnalogOpEnum::MAJ3, {{m_objTmp, 0}, {m_objTmp, 1}, {m_objTmp, 2}});

        /*AAP_VECTORS (B_T1        , out       )*/
        pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objRes, j + k}}, {{m_objTmp, 1}});

        /*AAP_VECTORS (out         , B_T1_T2_T3)*/
        pimGenericAAP(PimAnalogOpEnum::MAJ3, {{m_objTmp, 1}, {m_objTmp, 2}, {m_objTmp, 3}}, {{m_objRes, j + k}});
      }
      /*AAP_VECTORS (VECTOR(output[j + col_length]), B_DCC1)*/
      pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objDCC, 1}}, {{m_objRes, j + m_numBits}});
    }
  }
};

class TestReDRAM : public TestPim {
public:
  TestReDRAM() : TestPim("ReDRAM") {}
  virtual ~TestReDRAM() {}
  virtual void runCore() {
  }
};

class TestFlexiDRAM: public TestPim {
public:
  TestFlexiDRAM() : TestPim("FlexiDRAM") {}
  virtual ~TestFlexiDRAM() {}
  virtual void runCore() { 
  }
};


class TestDRISA1T1CNor : public TestPim {
public:
  TestDRISA1T1CNor() : TestPim("DRISA-1T1C-nor") {}
  virtual ~TestDRISA1T1CNor() {}
  virtual void runCore() {
  }
};

class TestDRISA1T1CMixed : public TestPim {
public:
  TestDRISA1T1CMixed() : TestPim("DRISA-1T1C-mixed") {}
  virtual ~TestDRISA1T1CMixed() {}
  virtual void runCore() {
   
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

