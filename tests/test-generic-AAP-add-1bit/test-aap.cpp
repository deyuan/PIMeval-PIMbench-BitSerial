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
    PimObjId objANot = pimAllocAssociated(m_objA, PIM_INT32); assert(objANot != -1);
    PimObjId objBNot = pimAllocAssociated(m_objB, PIM_INT32); assert(objBNot != -1);
    PimObjId objCinNot = pimAllocAssociated(m_objCin, PIM_INT32); assert(objCinNot != -1);
    PimObjId objSumNot = pimAllocAssociated(m_objSum, PIM_INT32); assert(objSumNot != -1);
    PimObjId objCoutNot = pimAllocAssociated(m_objCout, PIM_INT32); assert(objCoutNot != -1);
    pimNot(m_objA, objANot);
    pimNot(m_objB, objBNot);
    pimNot(m_objCin, objCinNot);

    // Use m_objTmp rows as t0 (row 0) and t1 (row 1)

    // t0 = ROW_CLONE(a)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objA, 0}}, {{m_objTmp, 0}});
    // t1 = ROW_CLONE(b)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{m_objTmp, 1}});
    // t0 = AND2(t0, t1)    // ab
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    // cout = ROW_CLONE(t0)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{m_objCout, 0}});

    // t0 = ROW_CLONE(a)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objA, 0}}, {{m_objTmp, 0}});
    // t1 = ROW_CLONE(cin)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCin, 0}}, {{m_objTmp, 1}});
    // t0 = AND2(t0, t1)    // ac
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    // t1 = ROW_CLONE(cout)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCout, 0}}, {{m_objTmp, 1}});
    // t0 = OR2(t1, t0)     // ab + ac
    pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 1}, {m_objTmp, 0}});
    // cout = ROW_CLONE(t0)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{m_objCout, 0}});

    // t0 = ROW_CLONE(b)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{m_objTmp, 0}});
    // t1 = ROW_CLONE(cin)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCin, 0}}, {{m_objTmp, 1}});
    // t0 = AND2(t0, t1)    // bc
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    // t1 = ROW_CLONE(cout)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCout, 0}}, {{m_objTmp, 1}});
    // t0 = OR2(t1, t0)     // ab + ac + bc
    pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 1}, {m_objTmp, 0}});
    // cout = ROW_CLONE(t0)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{m_objCout, 0}});

    // wire P   = (~A & B) | (A & ~B);  // A xor B using NOT/AND/OR
    // wire nP  = ~P;

    // assign Sum  = (P & ~Cin) | (~P & Cin);
    // assign Cout = (A & B) | (A & Cin) | (B & Cin);

    // Sum = a⊕b⊕cin = Σ m(1,2,4,7)
    // term1: a b' c'
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objA, 0}}, {{m_objTmp, 0}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objBNot, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objCinNot, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{m_objSum, 0}});

    // term2: a' b c'
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objANot, 0}}, {{m_objTmp, 0}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objCinNot, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objSum, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 1}, {m_objTmp, 0}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{m_objSum, 0}});

    // term3: a' b' c
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objANot, 0}}, {{m_objTmp, 0}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objBNot, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCin, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objSum, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 1}, {m_objTmp, 0}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{m_objSum, 0}});

    // term4: a b c
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objA, 0}}, {{m_objTmp, 0}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCin, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objSum, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 1}, {m_objTmp, 0}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{m_objSum, 0}});


    // CoutNot = (a'+b')(a'+c')(b'+c')  (De Morgan)
    // t0 = ROW_CLONE(aNot)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objANot, 0}}, {{m_objTmp, 0}});
    // t1 = ROW_CLONE(bNot)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objBNot, 0}}, {{m_objTmp, 1}});
    // t0 = OR(t0, t1)    // a'+b'
    pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 0}, {m_objTmp, 1}});
    // coutNot = ROW_CLONE(t0)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{objCoutNot, 0}});

    // t1 = ROW_CLONE(aNot)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objANot, 0}}, {{m_objTmp, 1}});
    // t0 = ROW_CLONE(cinNot)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objCinNot, 0}}, {{m_objTmp, 0}});
    // t1 = OR(t1, t0)    // a'+c'
    pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 1}, {m_objTmp, 0}});
    // t0 = ROW_CLONE(coutNot)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objCoutNot, 0}}, {{m_objTmp, 0}});
    // t0 = AND(t0, t1)   // (a'+b')(a'+c')
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    // coutNot = ROW_CLONE(t0)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{objCoutNot, 0}});

    // t0 = ROW_CLONE(bNot)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objBNot, 0}}, {{m_objTmp, 0}});
    // t1 = ROW_CLONE(cinNot)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objCinNot, 0}}, {{m_objTmp, 1}});
    // t0 = OR(t0, t1)    // b'+c'
    pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 0}, {m_objTmp, 1}});
    // t1 = ROW_CLONE(coutNot)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objCoutNot, 0}}, {{m_objTmp, 1}});
    // t1 = AND(t1, t0)   // (a'+b')(a'+c')(b'+c')
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 1}, {m_objTmp, 0}});
    // coutNot = ROW_CLONE(t1)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 1}}, {{objCoutNot, 0}});

    // SumNot = a' b' c' + a b c' + a b' c + a' b c  (even parity)
    // term1: a' b' c'
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objANot, 0}}, {{m_objTmp, 0}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objBNot, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objCinNot, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{objSumNot, 0}});

    // term2: a b c'
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objA, 0}}, {{m_objTmp, 0}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objCinNot, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objSumNot, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 1}, {m_objTmp, 0}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{objSumNot, 0}});

    // term3: a b' c
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objA, 0}}, {{m_objTmp, 0}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objBNot, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCin, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objSumNot, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 1}, {m_objTmp, 0}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{objSumNot, 0}});

    // term4: a' b c
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objANot, 0}}, {{m_objTmp, 0}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCin, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 0}, {m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objSumNot, 0}}, {{m_objTmp, 1}});
    pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 1}, {m_objTmp, 0}});
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{objSumNot, 0}});

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
    // assign xab  = A ^ B;          // XOR
    // t0, t3 = ROW_CLONE(A)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objA, 0}}, {{m_objTmp, 0}, {m_objTmp, 3}});
    // t1, t4 = ROW_CLONE(B)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{m_objTmp, 1}, {m_objTmp, 4}});
    // t2, t5 = ROW_CLONE(cin) (could be ignored in real-world scenario)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCin, 0}}, {{m_objTmp, 2}, {m_objTmp, 5}});
    // t0, t1 = XOR2(t0, t1)   
    pimGenericAAP(PimAnalogOpEnum::XOR2, {{m_objTmp, 0}, {m_objTmp, 1}});

    // assign Sum  = xab ^ Cin;      // XOR
    // t0, t2 = XOR2(t0, t2)
    pimGenericAAP(PimAnalogOpEnum::XOR2, {{m_objTmp, 0}, {m_objTmp, 2}}); // t0 now holds the sum
    // sum = ROW_CLONE(t0)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{m_objSum, 0}});

    // assign Cout = (A & B) | (Cin & xab);  // 2 AND + OR
    // t1, t5 = AND(t1, t5)
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 1}, {m_objTmp, 5}});
    // t3, t4 = AND(t3, t4)
    pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 3}, {m_objTmp, 4}}); // t3 now holds ab
    // t1, t2, t3, t5 = OR(t1, t3)
    pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 1}, {m_objTmp, 3}} , {{m_objTmp, 2}, {m_objTmp, 5}});
    // cout = ROW_CLONE(t2) (could be ignored in real-world scenario)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 2}}, {{m_objCout, 0}});



    // // SUM path
    // // t0, t3 = ROW_CLONE(a)
    // pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objA, 0}}, {{m_objTmp, 0}, {m_objTmp, 3}});
    // // t1, t4 = ROW_CLONE(b)
    // pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{m_objTmp, 1}, {m_objTmp, 4}});
    // // t2, t5 = ROW_CLONE(cin)
    // pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCin, 0}}, {{m_objTmp, 2}, {m_objTmp, 5}});
    // // t0 = XOR2(t0, t1)    // a ⊕ b -> stored in tmp[0]
    // pimGenericAAP(PimAnalogOpEnum::XOR2, {{m_objTmp, 0}, {m_objTmp, 1}});
    // // t0 = XOR2(t0, t2)    // (a ⊕ b) ⊕ cin
    // pimGenericAAP(PimAnalogOpEnum::XOR2, {{m_objTmp, 0}, {m_objTmp, 2}});
    // // sum = ROW_CLONE(t0)
    // pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 0}}, {{m_objSum, 0}});

    // // CARRY path: cout = ab + a·cin + b·cin
    // // t3 = AND2(t3, t4)    // ab stored in tmp[3]
    // pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 3}, {m_objTmp, 4}});

    // // t6 = ROW_CLONE(a); t7 = ROW_CLONE(cin)
    // pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objA, 0}}, {{m_objTmp, 6}});
    // pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCin, 0}}, {{m_objTmp, 7}});
    // // t6 = AND2(t6, t7)    // a·cin in tmp[6]
    // pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 6}, {m_objTmp, 7}});
    // // t3 = OR2(t3, t6)     // ab + a·cin in tmp[3]
    // pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 3}, {m_objTmp, 6}});

    // // t6 = ROW_CLONE(b); t7 = ROW_CLONE(cin)
    // pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{m_objTmp, 6}});
    // pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCin, 0}}, {{m_objTmp, 7}});
    // // t6 = AND2(t6, t7)    // b·cin in tmp[6]
    // pimGenericAAP(PimAnalogOpEnum::AND2, {{m_objTmp, 6}, {m_objTmp, 7}});
    // // t3 = OR2(t3, t6)     // ab + a·cin + b·cin in tmp[3]
    // pimGenericAAP(PimAnalogOpEnum::OR2, {{m_objTmp, 3}, {m_objTmp, 6}});

    // // cout = ROW_CLONE(t3)
    // pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 3}}, {{m_objCout, 0}});

  }
};

class TestFlexiDRAM: public TestPim {
public:
  TestFlexiDRAM() : TestPim("FlexiDRAM") {}
  virtual ~TestFlexiDRAM() {}
  virtual void runCore() {

    // t0, t1 = ROW_CLONE(a)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objA, 0}}, {{m_objTmp, 0}, {m_objTmp, 1}});
    // t2 = ROW_CLONE(b)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{m_objTmp, 2}});
    // t3, t4 = ROW_CLONE(cin)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCin, 0}}, {{m_objTmp, 3}, {m_objTmp, 4}});

    // t0, t2, t3 = XOR3(t0, t2, t3) //
    pimGenericAAP(PimAnalogOpEnum::XOR3, {{m_objTmp, 0}, {m_objTmp, 2}, {m_objTmp, 3}});
    // sum = ROW_CLONE(t3)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 3}}, {{m_objSum, 0}});

    // t3 = ROW_CLONE(b)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{m_objTmp, 3}});
    // t1, t3, t4 = MAJ3(t1, t3, t4)
    pimGenericAAP(PimAnalogOpEnum::MAJ3, {{m_objTmp, 1}, {m_objTmp, 3}, {m_objTmp, 4}});
    // cout = ROW_CLONE(t1)
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objTmp, 1}}, {{m_objCout, 0}});

  }
};


class TestDRISA1T1CNor : public TestPim {
public:
  TestDRISA1T1CNor() : TestPim("DRISA-1T1C-nor") {}
  virtual ~TestDRISA1T1CNor() {}
  virtual void runCore() {

    PimObjId objDCC = pimAllocAssociated(m_objTmp, PIM_INT32); assert(objDCC != -1);
    PimObjId objDCCN = pimAllocAssociated(m_objTmp, PIM_INT32); assert(objDCC != -1);
    // pimOpAAP(1, 1, cin, 0, DCC, 1); #RowRead = 2
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objCin, 0}}, {{objDCC, 1}});

    // pimOpAAP(1, 3, B_DCC1, B_T0_T1_T2); #RowRead = 4
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objDCC, 1}}, {{m_objTmp, 0}, {m_objTmp, 1}, {m_objTmp, 2}});

    // pimOpAAP(1, 2, src1, 0, B_T2_T3); #RowRead = 3
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objA, 0}}, {{m_objTmp, 2}, {m_objTmp, 3}});

    // pimOpAAP(1, 1, src2, 0, B_DCC1); #RowRead = 2
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{objDCC, 1}});

    // pimOpAP(3, B_DCC1_T0_T3); #RowRead = 1
    pimGenericAAP(PimAnalogOpEnum::MAJ3, {{objDCC, 1}, {m_objTmp, 0}, {m_objTmp, 3}});

    // objDCCN[1] = ~objDCC[1] # RowRead = 2
    pimOpReadRowToSa(objDCC, 1);
    pimOpMove(m_objA, PIM_RREG_SA, PIM_RREG_R1);
    pimOpNor(m_objA, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA);
    pimOpWriteSaToRow(objDCCN, 1);  


    // pimOpAAP(1, 2, B_DCC1N, B_T0_T3); #RowRead = 3
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objDCCN, 1}}, {{m_objTmp, 0}, {m_objTmp, 3}});

    // pimOpAP(3, B_T0_T1_T2); #RowRead = 1
    pimGenericAAP(PimAnalogOpEnum::MAJ3, {{m_objTmp, 0}, {m_objTmp, 1}, { m_objTmp, 2}});

    // pimOpAAP(1, 1, src2, 0, B_T1); #RowRead = 2
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{m_objB, 0}}, {{m_objTmp, 1}});

    // pimOpAAP(3, 1, B_T1_T2_T3, dest, 0); #RowRead = 3
    pimGenericAAP(PimAnalogOpEnum::MAJ3, {{m_objTmp, 1}, {m_objTmp, 2}, {m_objTmp, 3}}, {{m_objSum, 0}});

    // pimOpAAP(1, 1, DCC, 1, cout, 0); #RowRead = 2
    pimGenericAAP(PimAnalogOpEnum::ROW_CLONE, {{objDCC, 1}}, {{m_objCout, 0}});
  }
};

class TestDRISA1T1CMixed : public TestPim {
public:
  TestDRISA1T1CMixed() : TestPim("DRISA-1T1C-mixed") {}
  virtual ~TestDRISA1T1CMixed() {}
  virtual void runCore() {
    // s1=XNOR(A,B)
    // Sum=XNOR(s1,Cin)
    // t1=NOR(A,B)
    // s1‾=NOT(s1) this instruction needs 1 row access.
    // t2 = NOR(Cin,s1‾) this instuction needs 1 row access.
    // Cout=NOR(t1, t2) this instruction needs 2 row accesses.

    // t0 = XNOR(A, B)           // intermediate for Sum
    pimOpReadRowToSa(m_objA, 0);
    pimOpMove(m_objA, PIM_RREG_SA, PIM_RREG_R1);           // t0 = a
    pimOpReadRowToSa(m_objB, 0);                           // sa = b
    pimOpXnor(m_objB, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA); // sa = XNOR(b,a)
    pimOpWriteSaToRow(m_objTmp, 0);                        // tmp[0] = sa

    // Sum = XNOR(s1, Cin)       // correct full-adder Sum
    pimOpMove(m_objA, PIM_RREG_SA, PIM_RREG_R1);           // t0 = a
    pimOpReadRowToSa(m_objCin, 0);
    pimOpXnor(m_objB, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA); // sa = XNOR(sa,cin)
    pimOpWriteSaToRow(m_objSum, 0);                        // sum = sa

    // t1 = NOR(A, B)            // part of Cout logic
    pimOpReadRowToSa(m_objA, 0);
    pimOpMove(m_objA, PIM_RREG_SA, PIM_RREG_R1);           // t0 = a
    pimOpReadRowToSa(m_objB, 0);                           // sa = b
    pimOpNor(m_objB, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA); // sa = NOR(b,a)
    pimOpWriteSaToRow(m_objTmp, 1);                        // tmp[1] = sa

    // t2 = NOR(Cin, t0)         // second part of Cout logic
    pimOpReadRowToSa(m_objCin, 0);
    pimOpMove(m_objA, PIM_RREG_SA, PIM_RREG_R1);           // t0 = a
    pimOpReadRowToSa(m_objTmp, 0);                           // sa = tmp[0]
    pimOpNor(m_objB, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_R1); // r1 = NOR(Cin,s1)

    // Cout = NOR(t1, t2)        // final Cout
    pimOpReadRowToSa(m_objTmp, 1);
    pimOpNor(m_objTmp, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA); // sa = NOR(t1,sa)
    pimOpWriteSaToRow(m_objCout, 0);                       // cout

    // // s1 = a XOR b  → tmp[0]
    // pimOpReadRowToSa(m_objA, 0);
    // pimOpMove(m_objA, PIM_RREG_SA, PIM_RREG_R1);           // t0 = a
    // pimOpReadRowToSa(m_objB, 0);                           // sa = b
    // pimOpXnor(m_objB, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA); // sa = XNOR(b,a)
    // pimOpNot(m_objB, PIM_RREG_SA, PIM_RREG_SA);            // sa = a XOR b
    // pimOpWriteSaToRow(m_objTmp, 0);                        // tmp[0] = s1

    // // ab = a AND b  → tmp[1]
    // pimOpReadRowToSa(m_objA, 0);  pimOpMove(m_objA, PIM_RREG_SA, PIM_RREG_R1); // t0 = a
    // pimOpReadRowToSa(m_objB, 0);  pimOpNand(m_objB, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA); // sa = ~(a&b)
    // pimOpNot(m_objB, PIM_RREG_SA, PIM_RREG_SA);            // sa = a&b
    // pimOpWriteSaToRow(m_objTmp, 1);                        // tmp[1] = ab

    // // c1 = cin AND s1  → tmp[2]
    // pimOpReadRowToSa(m_objCin, 0); pimOpMove(m_objCin, PIM_RREG_SA, PIM_RREG_R1); // t0 = cin
    // pimOpReadRowToSa(m_objTmp, 0); pimOpNand(m_objTmp, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA); // sa = ~(cin&s1)
    // pimOpNot(m_objTmp, PIM_RREG_SA, PIM_RREG_SA);          // sa = cin&s1
    // pimOpWriteSaToRow(m_objTmp, 2);                        // tmp[2] = c1

    // // cout = ab OR c1 = NAND(~ab, ~c1)
    // pimOpReadRowToSa(m_objTmp, 1); pimOpNot(m_objTmp, PIM_RREG_SA, PIM_RREG_SA); // sa = ~ab
    // pimOpMove(m_objTmp, PIM_RREG_SA, PIM_RREG_R1);         // t0 = ~ab
    // pimOpReadRowToSa(m_objTmp, 2); pimOpNot(m_objTmp, PIM_RREG_SA, PIM_RREG_SA); // sa = ~c1
    // pimOpNand(m_objTmp, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA);                   // sa = (~c1 NAND ~ab) = ab|c1
    // pimOpWriteSaToRow(m_objCout, 0);                       // cout

    // // sum = s1 XOR cin
    // pimOpReadRowToSa(m_objTmp, 0);                         // sa = s1
    // pimOpMove(m_objTmp, PIM_RREG_SA, PIM_RREG_R1);         // t0 = s1
    // pimOpReadRowToSa(m_objCin, 0);                         // sa = cin
    // pimOpXnor(m_objCin, PIM_RREG_SA, PIM_RREG_R1, PIM_RREG_SA); // sa = XNOR(cin,s1)
    // pimOpNot(m_objCin, PIM_RREG_SA, PIM_RREG_SA);          // sa = XOR(cin,s1)
    // pimOpWriteSaToRow(m_objSum, 0);                        // sum

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

