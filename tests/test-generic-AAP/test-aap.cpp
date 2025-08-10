// Test: Generic AAP
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
  unsigned m_numRows = 128;
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
  }
private:
  PimObjId m_negObj1 = -1;
};

class TestSIMDRAM : public TestPim {
public:
  TestSIMDRAM() : TestPim("SIMDRAM") {}
  virtual ~TestSIMDRAM() {}
  virtual void runCore() {
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
    TestComputeDRAM test1;
    test1.run();
  }
  {
    TestSIMDRAM test2;
    test2.run();
  }
  {
    TestReDRAM test3;
    test3.run();
  }
  return 0;
}

