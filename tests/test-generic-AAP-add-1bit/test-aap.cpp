// Test: Generic AAP Addition
// Copyright (c) 2024 University of Virginia
// This file is licensed under the MIT License.
// See the LICENSE file in the root of this repository for more details.

#include "libpimeval.h"
#include <iostream>
#include <vector>
#include <cassert>

// Test 1-bit adder operation on various PIM architectures
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
    pimFree(m_objCin);
    pimFree(m_objSum);
    pimFree(m_objCout);
    pimFree(m_objTmp);
    pimFree(m_objZero);
    pimFree(m_objOne);
    pimDeleteDevice();
  }

  void initTest()
  {
    m_objA    = pimAlloc(PIM_ALLOC_V1, m_numElements, PIM_INT32); assert(m_objA != -1);
    m_objB    = pimAllocAssociated(m_objA, PIM_INT32);       assert(m_objB != -1);
    m_objCin  = pimAllocAssociated(m_objA, PIM_INT32);       assert(m_objCin != -1);
    m_objSum  = pimAllocAssociated(m_objA, PIM_INT32);       assert(m_objSum != -1);
    m_objCout = pimAllocAssociated(m_objA, PIM_INT32);       assert(m_objCout != -1);
    m_objTmp  = pimAllocAssociated(m_objA, PIM_INT32);       assert(m_objTmp != -1);
    m_objZero = pimAllocAssociated(m_objA, PIM_INT32);       assert(m_objZero != -1);
    m_objOne  = pimAllocAssociated(m_objA, PIM_INT32);       assert(m_objOne != -1);

    pimCopyHostToDevice((void*)m_a.data(), m_objA);
    pimCopyHostToDevice((void*)m_b.data(), m_objB);
    pimCopyHostToDevice((void*)m_cin.data(), m_objCin);
    pimCopyHostToDevice((void*)m_zero.data(), m_objZero);
    pimCopyHostToDevice((void*)m_one.data(), m_objOne);
    pimCopyHostToDevice((void*)m_zero.data(), m_objSum);
    pimCopyHostToDevice((void*)m_zero.data(), m_objCout);
    pimCopyHostToDevice((void*)m_zero.data(), m_objTmp);
  }

  bool checkResults()
  {
    bool ok = true;
    std::vector<int> sumRes(m_numElements, 0);
    std::vector<int> coutRes(m_numElements, 0);
    pimCopyDeviceToHost(m_objSum, (void*)sumRes.data());
    pimCopyDeviceToHost(m_objCout, (void*)coutRes.data());
    for (unsigned i = 0; i < m_numElements; ++i) {
      int a = m_a[i];
      int b = m_b[i];
      int cin = m_cin[i];
      int expectSum = (a ^ b ^ cin) & 1;
      int expectCout = ((a & b) | (a & cin) | (b & cin)) & 1;
      if (sumRes[i] != expectSum || coutRes[i] != expectCout) {
        ok = false;
        std::cout << "Input: " << a << " " << b << " " << cin
                  << " -> got(sum,cout): " << sumRes[i] << "," << coutRes[i]
                  << " expected: " << expectSum << "," << expectCout << " ERROR" << std::endl;
      } else {
        std::cout << "Input: " << a << " " << b << " " << cin
                  << " -> got(sum,cout): " << sumRes[i] << "," << coutRes[i]
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

  // Inputs: a, b, cin
  const std::vector<int> m_a   = {0, 0, 0, 0, 1, 1, 1, 1};
  const std::vector<int> m_b   = {0, 0, 1, 1, 0, 0, 1, 1};
  const std::vector<int> m_cin = {0, 1, 0, 1, 0, 1, 0, 1};

  // Utility vectors
  const std::vector<int> m_zero   = {0, 0, 0, 0, 0, 0, 0, 0};
  const std::vector<int> m_one    = {1, 1, 1, 1, 1, 1, 1, 1};

  // Device object IDs
  PimObjId m_objA    = -1;
  PimObjId m_objB    = -1;
  PimObjId m_objCin  = -1;
  PimObjId m_objSum  = -1;
  PimObjId m_objCout = -1;
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

    // pimOpAAP(1, 1, cin, 0, DCC, 1);
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCin, 0}}, {{objDCC, 1}});

    // pimOpAAP(1, 3, B_DCC1, B_T0_T1_T2);
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objDCC, 1}}, {{m_objTmp, 0}, {m_objTmp, 1}, {m_objTmp, 2}});

    // pimOpAAP(1, 2, src1, 0, B_T2_T3);
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objA, 0}}, {{m_objTmp, 2}, {m_objTmp, 3}});

    // pimOpAAP(1, 1, src2, 0, B_DCC1);
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{objDCC, 1}});

    // pimOpAP(3, B_DCC1_T0_T3);
    pimGenericAAP(PimAnalogOpEnum::MAJ3, {{objDCC, 1}, {m_objTmp, 0}, {m_objTmp, 3}});

    // pimOpAAP(1, 2, B_DCC1N, B_T0_T3);
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objDCCN, 1}}, {{m_objTmp, 0}, {m_objTmp, 3}});

    // pimOpAP(3, B_T0_T1_T2);
    pimGenericAAP(PimAnalogOpEnum::MAJ3, {{m_objTmp, 0}, {m_objTmp, 1}, { m_objTmp, 2}});

    // pimOpAAP(1, 1, src2, 0, B_T1);
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{m_objTmp, 1}});

    // pimOpAAP(3, 1, B_T1_T2_T3, dest, 0);
    pimGenericAAP(PimAnalogOpEnum::MAJ3, {{m_objTmp, 1}, {m_objTmp, 2}, {m_objTmp, 3}}, {{m_objSum, 0}});

    // pimOpAAP(1, 1, DCC, 1, cout, 0);
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objDCC, 1}}, {{m_objCout, 0}});
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

