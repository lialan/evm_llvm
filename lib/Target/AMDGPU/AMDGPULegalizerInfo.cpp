//===- AMDGPULegalizerInfo.cpp -----------------------------------*- C++ -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file implements the targeting of the Machinelegalizer class for
/// AMDGPU.
/// \todo This should be generated by TableGen.
//===----------------------------------------------------------------------===//

#include "AMDGPU.h"
#include "AMDGPULegalizerInfo.h"
#include "AMDGPUTargetMachine.h"
#include "SIMachineFunctionInfo.h"

#include "llvm/CodeGen/GlobalISel/MachineIRBuilder.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"

using namespace llvm;
using namespace LegalizeActions;
using namespace LegalizeMutations;
using namespace LegalityPredicates;


static LegalityPredicate isMultiple32(unsigned TypeIdx,
                                      unsigned MaxSize = 512) {
  return [=](const LegalityQuery &Query) {
    const LLT Ty = Query.Types[TypeIdx];
    const LLT EltTy = Ty.getScalarType();
    return Ty.getSizeInBits() <= MaxSize && EltTy.getSizeInBits() % 32 == 0;
  };
}

static LegalityPredicate isSmallOddVector(unsigned TypeIdx) {
  return [=](const LegalityQuery &Query) {
    const LLT Ty = Query.Types[TypeIdx];
    return Ty.isVector() &&
           Ty.getNumElements() % 2 != 0 &&
           Ty.getElementType().getSizeInBits() < 32;
  };
}

static LegalizeMutation oneMoreElement(unsigned TypeIdx) {
  return [=](const LegalityQuery &Query) {
    const LLT Ty = Query.Types[TypeIdx];
    const LLT EltTy = Ty.getElementType();
    return std::make_pair(TypeIdx, LLT::vector(Ty.getNumElements() + 1, EltTy));
  };
}

static LegalizeMutation fewerEltsToSize64Vector(unsigned TypeIdx) {
  return [=](const LegalityQuery &Query) {
    const LLT Ty = Query.Types[TypeIdx];
    const LLT EltTy = Ty.getElementType();
    unsigned Size = Ty.getSizeInBits();
    unsigned Pieces = (Size + 63) / 64;
    unsigned NewNumElts = (Ty.getNumElements() + 1) / Pieces;
    return std::make_pair(TypeIdx, LLT::scalarOrVector(NewNumElts, EltTy));
  };
}

static LegalityPredicate vectorWiderThan(unsigned TypeIdx, unsigned Size) {
  return [=](const LegalityQuery &Query) {
    const LLT QueryTy = Query.Types[TypeIdx];
    return QueryTy.isVector() && QueryTy.getSizeInBits() > Size;
  };
}

static LegalityPredicate numElementsNotEven(unsigned TypeIdx) {
  return [=](const LegalityQuery &Query) {
    const LLT QueryTy = Query.Types[TypeIdx];
    return QueryTy.isVector() && QueryTy.getNumElements() % 2 != 0;
  };
}

AMDGPULegalizerInfo::AMDGPULegalizerInfo(const GCNSubtarget &ST,
                                         const GCNTargetMachine &TM) {
  using namespace TargetOpcode;

  auto GetAddrSpacePtr = [&TM](unsigned AS) {
    return LLT::pointer(AS, TM.getPointerSizeInBits(AS));
  };

  const LLT S1 = LLT::scalar(1);
  const LLT S8 = LLT::scalar(8);
  const LLT S16 = LLT::scalar(16);
  const LLT S32 = LLT::scalar(32);
  const LLT S64 = LLT::scalar(64);
  const LLT S128 = LLT::scalar(128);
  const LLT S256 = LLT::scalar(256);
  const LLT S512 = LLT::scalar(512);

  const LLT V2S16 = LLT::vector(2, 16);
  const LLT V4S16 = LLT::vector(4, 16);
  const LLT V8S16 = LLT::vector(8, 16);

  const LLT V2S32 = LLT::vector(2, 32);
  const LLT V3S32 = LLT::vector(3, 32);
  const LLT V4S32 = LLT::vector(4, 32);
  const LLT V5S32 = LLT::vector(5, 32);
  const LLT V6S32 = LLT::vector(6, 32);
  const LLT V7S32 = LLT::vector(7, 32);
  const LLT V8S32 = LLT::vector(8, 32);
  const LLT V9S32 = LLT::vector(9, 32);
  const LLT V10S32 = LLT::vector(10, 32);
  const LLT V11S32 = LLT::vector(11, 32);
  const LLT V12S32 = LLT::vector(12, 32);
  const LLT V13S32 = LLT::vector(13, 32);
  const LLT V14S32 = LLT::vector(14, 32);
  const LLT V15S32 = LLT::vector(15, 32);
  const LLT V16S32 = LLT::vector(16, 32);

  const LLT V2S64 = LLT::vector(2, 64);
  const LLT V3S64 = LLT::vector(3, 64);
  const LLT V4S64 = LLT::vector(4, 64);
  const LLT V5S64 = LLT::vector(5, 64);
  const LLT V6S64 = LLT::vector(6, 64);
  const LLT V7S64 = LLT::vector(7, 64);
  const LLT V8S64 = LLT::vector(8, 64);

  std::initializer_list<LLT> AllS32Vectors =
    {V2S32, V3S32, V4S32, V5S32, V6S32, V7S32, V8S32,
     V9S32, V10S32, V11S32, V12S32, V13S32, V14S32, V15S32, V16S32};
  std::initializer_list<LLT> AllS64Vectors =
    {V2S64, V3S64, V4S64, V5S64, V6S64, V7S64, V8S64};

  const LLT GlobalPtr = GetAddrSpacePtr(AMDGPUAS::GLOBAL_ADDRESS);
  const LLT ConstantPtr = GetAddrSpacePtr(AMDGPUAS::CONSTANT_ADDRESS);
  const LLT LocalPtr = GetAddrSpacePtr(AMDGPUAS::LOCAL_ADDRESS);
  const LLT FlatPtr = GetAddrSpacePtr(AMDGPUAS::FLAT_ADDRESS);
  const LLT PrivatePtr = GetAddrSpacePtr(AMDGPUAS::PRIVATE_ADDRESS);

  const LLT CodePtr = FlatPtr;

  const std::initializer_list<LLT> AddrSpaces64 = {
    GlobalPtr, ConstantPtr, FlatPtr
  };

  const std::initializer_list<LLT> AddrSpaces32 = {
    LocalPtr, PrivatePtr
  };

  setAction({G_BRCOND, S1}, Legal);

  getActionDefinitionsBuilder({G_ADD, G_SUB, G_MUL, G_UMULH, G_SMULH})
    .legalFor({S32})
    .clampScalar(0, S32, S32)
    .scalarize(0);

  // Report legal for any types we can handle anywhere. For the cases only legal
  // on the SALU, RegBankSelect will be able to re-legalize.
  getActionDefinitionsBuilder({G_AND, G_OR, G_XOR})
    .legalFor({S32, S1, S64, V2S32, V2S16, V4S16})
    .clampScalar(0, S32, S64)
    .moreElementsIf(isSmallOddVector(0), oneMoreElement(0))
    .fewerElementsIf(vectorWiderThan(0, 32), fewerEltsToSize64Vector(0))
    .scalarize(0);

  getActionDefinitionsBuilder({G_UADDO, G_SADDO, G_USUBO, G_SSUBO,
                               G_UADDE, G_SADDE, G_USUBE, G_SSUBE})
    .legalFor({{S32, S1}})
    .clampScalar(0, S32, S32);

  getActionDefinitionsBuilder(G_BITCAST)
    .legalForCartesianProduct({S32, V2S16})
    .legalForCartesianProduct({S64, V2S32, V4S16})
    .legalForCartesianProduct({V2S64, V4S32})
    // Don't worry about the size constraint.
    .legalIf(all(isPointer(0), isPointer(1)));

  if (ST.has16BitInsts()) {
    getActionDefinitionsBuilder(G_FCONSTANT)
      .legalFor({S32, S64, S16})
      .clampScalar(0, S16, S64);
  } else {
    getActionDefinitionsBuilder(G_FCONSTANT)
      .legalFor({S32, S64})
      .clampScalar(0, S32, S64);
  }

  getActionDefinitionsBuilder(G_IMPLICIT_DEF)
    .legalFor({S1, S32, S64, V2S32, V4S32, V2S16, V4S16, GlobalPtr,
               ConstantPtr, LocalPtr, FlatPtr, PrivatePtr})
    .moreElementsIf(isSmallOddVector(0), oneMoreElement(0))
    .clampScalarOrElt(0, S32, S512)
    .legalIf(isMultiple32(0))
    .widenScalarToNextPow2(0, 32);


  // FIXME: i1 operands to intrinsics should always be legal, but other i1
  // values may not be legal.  We need to figure out how to distinguish
  // between these two scenarios.
  getActionDefinitionsBuilder(G_CONSTANT)
    .legalFor({S1, S32, S64, GlobalPtr,
               LocalPtr, ConstantPtr, PrivatePtr, FlatPtr })
    .clampScalar(0, S32, S64)
    .widenScalarToNextPow2(0)
    .legalIf(isPointer(0));

  setAction({G_FRAME_INDEX, PrivatePtr}, Legal);

  auto &FPOpActions = getActionDefinitionsBuilder(
    { G_FADD, G_FMUL, G_FNEG, G_FABS, G_FMA, G_FCANONICALIZE})
    .legalFor({S32, S64});

  if (ST.has16BitInsts()) {
    if (ST.hasVOP3PInsts())
      FPOpActions.legalFor({S16, V2S16});
    else
      FPOpActions.legalFor({S16});
  }

  if (ST.hasVOP3PInsts())
    FPOpActions.clampMaxNumElements(0, S16, 2);
  FPOpActions
    .scalarize(0)
    .clampScalar(0, ST.has16BitInsts() ? S16 : S32, S64);

  if (ST.has16BitInsts()) {
    getActionDefinitionsBuilder(G_FSQRT)
      .legalFor({S32, S64, S16})
      .scalarize(0)
      .clampScalar(0, S16, S64);
  } else {
    getActionDefinitionsBuilder(G_FSQRT)
      .legalFor({S32, S64})
      .scalarize(0)
      .clampScalar(0, S32, S64);
  }

  getActionDefinitionsBuilder(G_FPTRUNC)
    .legalFor({{S32, S64}, {S16, S32}})
    .scalarize(0);

  getActionDefinitionsBuilder(G_FPEXT)
    .legalFor({{S64, S32}, {S32, S16}})
    .lowerFor({{S64, S16}}) // FIXME: Implement
    .scalarize(0);

  getActionDefinitionsBuilder(G_FSUB)
      // Use actual fsub instruction
      .legalFor({S32})
      // Must use fadd + fneg
      .lowerFor({S64, S16, V2S16})
      .scalarize(0)
      .clampScalar(0, S32, S64);

  getActionDefinitionsBuilder({G_SEXT, G_ZEXT, G_ANYEXT})
    .legalFor({{S64, S32}, {S32, S16}, {S64, S16},
               {S32, S1}, {S64, S1}, {S16, S1},
               // FIXME: Hack
               {S32, S8}, {S128, S32}, {S128, S64}, {S32, LLT::scalar(24)}})
    .scalarize(0);

  getActionDefinitionsBuilder({G_SITOFP, G_UITOFP})
    .legalFor({{S32, S32}, {S64, S32}})
    .scalarize(0);

  getActionDefinitionsBuilder({G_FPTOSI, G_FPTOUI})
    .legalFor({{S32, S32}, {S32, S64}})
    .scalarize(0);

  getActionDefinitionsBuilder({G_INTRINSIC_TRUNC, G_INTRINSIC_ROUND})
    .legalFor({S32, S64})
    .scalarize(0);


  getActionDefinitionsBuilder(G_GEP)
    .legalForCartesianProduct(AddrSpaces64, {S64})
    .legalForCartesianProduct(AddrSpaces32, {S32})
    .scalarize(0);

  setAction({G_BLOCK_ADDR, CodePtr}, Legal);

  getActionDefinitionsBuilder(G_ICMP)
    .legalForCartesianProduct(
      {S1}, {S32, S64, GlobalPtr, LocalPtr, ConstantPtr, PrivatePtr, FlatPtr})
    .legalFor({{S1, S32}, {S1, S64}})
    .widenScalarToNextPow2(1)
    .clampScalar(1, S32, S64)
    .scalarize(0)
    .legalIf(all(typeIs(0, S1), isPointer(1)));

  getActionDefinitionsBuilder(G_FCMP)
    .legalFor({{S1, S32}, {S1, S64}})
    .widenScalarToNextPow2(1)
    .clampScalar(1, S32, S64)
    .scalarize(0);

  // FIXME: fexp, flog2, flog10 needs to be custom lowered.
  getActionDefinitionsBuilder({G_FPOW, G_FEXP, G_FEXP2,
                               G_FLOG, G_FLOG2, G_FLOG10})
    .legalFor({S32})
    .scalarize(0);

  // The 64-bit versions produce 32-bit results, but only on the SALU.
  getActionDefinitionsBuilder({G_CTLZ, G_CTLZ_ZERO_UNDEF,
                               G_CTTZ, G_CTTZ_ZERO_UNDEF,
                               G_CTPOP})
    .legalFor({{S32, S32}, {S32, S64}})
    .clampScalar(0, S32, S32)
    .clampScalar(1, S32, S64)
    .scalarize(0)
    .widenScalarToNextPow2(0, 32)
    .widenScalarToNextPow2(1, 32);

  // TODO: Expand for > s32
  getActionDefinitionsBuilder(G_BSWAP)
    .legalFor({S32})
    .clampScalar(0, S32, S32)
    .scalarize(0);


  auto smallerThan = [](unsigned TypeIdx0, unsigned TypeIdx1) {
    return [=](const LegalityQuery &Query) {
      return Query.Types[TypeIdx0].getSizeInBits() <
             Query.Types[TypeIdx1].getSizeInBits();
    };
  };

  auto greaterThan = [](unsigned TypeIdx0, unsigned TypeIdx1) {
    return [=](const LegalityQuery &Query) {
      return Query.Types[TypeIdx0].getSizeInBits() >
             Query.Types[TypeIdx1].getSizeInBits();
    };
  };

  getActionDefinitionsBuilder(G_INTTOPTR)
    // List the common cases
    .legalForCartesianProduct(AddrSpaces64, {S64})
    .legalForCartesianProduct(AddrSpaces32, {S32})
    .scalarize(0)
    // Accept any address space as long as the size matches
    .legalIf(sameSize(0, 1))
    .widenScalarIf(smallerThan(1, 0),
      [](const LegalityQuery &Query) {
        return std::make_pair(1, LLT::scalar(Query.Types[0].getSizeInBits()));
      })
    .narrowScalarIf(greaterThan(1, 0),
      [](const LegalityQuery &Query) {
        return std::make_pair(1, LLT::scalar(Query.Types[0].getSizeInBits()));
      });

  getActionDefinitionsBuilder(G_PTRTOINT)
    // List the common cases
    .legalForCartesianProduct(AddrSpaces64, {S64})
    .legalForCartesianProduct(AddrSpaces32, {S32})
    .scalarize(0)
    // Accept any address space as long as the size matches
    .legalIf(sameSize(0, 1))
    .widenScalarIf(smallerThan(0, 1),
      [](const LegalityQuery &Query) {
        return std::make_pair(0, LLT::scalar(Query.Types[1].getSizeInBits()));
      })
    .narrowScalarIf(
      greaterThan(0, 1),
      [](const LegalityQuery &Query) {
        return std::make_pair(0, LLT::scalar(Query.Types[1].getSizeInBits()));
      });

  if (ST.hasFlatAddressSpace()) {
    getActionDefinitionsBuilder(G_ADDRSPACE_CAST)
      .scalarize(0)
      .custom();
  }

  getActionDefinitionsBuilder({G_LOAD, G_STORE})
    .narrowScalarIf([](const LegalityQuery &Query) {
        unsigned Size = Query.Types[0].getSizeInBits();
        unsigned MemSize = Query.MMODescrs[0].SizeInBits;
        return (Size > 32 && MemSize < Size);
      },
      [](const LegalityQuery &Query) {
        return std::make_pair(0, LLT::scalar(32));
      })
    .fewerElementsIf([=, &ST](const LegalityQuery &Query) {
        unsigned MemSize = Query.MMODescrs[0].SizeInBits;
        return (MemSize == 96) &&
               Query.Types[0].isVector() &&
               ST.getGeneration() < AMDGPUSubtarget::SEA_ISLANDS;
      },
      [=](const LegalityQuery &Query) {
        return std::make_pair(0, V2S32);
      })
    .legalIf([=, &ST](const LegalityQuery &Query) {
        const LLT &Ty0 = Query.Types[0];

        unsigned Size = Ty0.getSizeInBits();
        unsigned MemSize = Query.MMODescrs[0].SizeInBits;
        if (Size < 32 || (Size > 32 && MemSize < Size))
          return false;

        if (Ty0.isVector() && Size != MemSize)
          return false;

        // TODO: Decompose private loads into 4-byte components.
        // TODO: Illegal flat loads on SI
        switch (MemSize) {
        case 8:
        case 16:
          return Size == 32;
        case 32:
        case 64:
        case 128:
          return true;

        case 96:
          // XXX hasLoadX3
          return (ST.getGeneration() >= AMDGPUSubtarget::SEA_ISLANDS);

        case 256:
        case 512:
          // TODO: constant loads
        default:
          return false;
        }
      })
    .clampScalar(0, S32, S64);


  // FIXME: Handle alignment requirements.
  auto &ExtLoads = getActionDefinitionsBuilder({G_SEXTLOAD, G_ZEXTLOAD})
    .legalForTypesWithMemDesc({
        {S32, GlobalPtr, 8, 8},
        {S32, GlobalPtr, 16, 8},
        {S32, LocalPtr, 8, 8},
        {S32, LocalPtr, 16, 8},
        {S32, PrivatePtr, 8, 8},
        {S32, PrivatePtr, 16, 8}});
  if (ST.hasFlatAddressSpace()) {
    ExtLoads.legalForTypesWithMemDesc({{S32, FlatPtr, 8, 8},
                                       {S32, FlatPtr, 16, 8}});
  }

  ExtLoads.clampScalar(0, S32, S32)
          .widenScalarToNextPow2(0)
          .unsupportedIfMemSizeNotPow2()
          .lower();

  auto &Atomics = getActionDefinitionsBuilder(
    {G_ATOMICRMW_XCHG, G_ATOMICRMW_ADD, G_ATOMICRMW_SUB,
     G_ATOMICRMW_AND, G_ATOMICRMW_OR, G_ATOMICRMW_XOR,
     G_ATOMICRMW_MAX, G_ATOMICRMW_MIN, G_ATOMICRMW_UMAX,
     G_ATOMICRMW_UMIN, G_ATOMIC_CMPXCHG})
    .legalFor({{S32, GlobalPtr}, {S32, LocalPtr},
               {S64, GlobalPtr}, {S64, LocalPtr}});
  if (ST.hasFlatAddressSpace()) {
    Atomics.legalFor({{S32, FlatPtr}, {S64, FlatPtr}});
  }

  // TODO: Pointer types, any 32-bit or 64-bit vector
  getActionDefinitionsBuilder(G_SELECT)
    .legalForCartesianProduct({S32, S64, V2S32, V2S16, V4S16,
          GlobalPtr, LocalPtr, FlatPtr, PrivatePtr,
          LLT::vector(2, LocalPtr), LLT::vector(2, PrivatePtr)}, {S1})
    .clampScalar(0, S32, S64)
    .moreElementsIf(isSmallOddVector(0), oneMoreElement(0))
    .fewerElementsIf(numElementsNotEven(0), scalarize(0))
    .scalarize(1)
    .clampMaxNumElements(0, S32, 2)
    .clampMaxNumElements(0, LocalPtr, 2)
    .clampMaxNumElements(0, PrivatePtr, 2)
    .scalarize(0)
    .legalIf(all(isPointer(0), typeIs(1, S1)));

  // TODO: Only the low 4/5/6 bits of the shift amount are observed, so we can
  // be more flexible with the shift amount type.
  auto &Shifts = getActionDefinitionsBuilder({G_SHL, G_LSHR, G_ASHR})
    .legalFor({{S32, S32}, {S64, S32}});
  if (ST.has16BitInsts()) {
    if (ST.hasVOP3PInsts()) {
      Shifts.legalFor({{S16, S32}, {S16, S16}, {V2S16, V2S16}})
            .clampMaxNumElements(0, S16, 2);
    } else
      Shifts.legalFor({{S16, S32}, {S16, S16}});

    Shifts.clampScalar(1, S16, S32);
    Shifts.clampScalar(0, S16, S64);
    Shifts.widenScalarToNextPow2(0, 16);
  } else {
    // Make sure we legalize the shift amount type first, as the general
    // expansion for the shifted type will produce much worse code if it hasn't
    // been truncated already.
    Shifts.clampScalar(1, S32, S32);
    Shifts.clampScalar(0, S32, S64);
    Shifts.widenScalarToNextPow2(0, 32);
  }
  Shifts.scalarize(0);

  for (unsigned Op : {G_EXTRACT_VECTOR_ELT, G_INSERT_VECTOR_ELT}) {
    unsigned VecTypeIdx = Op == G_EXTRACT_VECTOR_ELT ? 1 : 0;
    unsigned EltTypeIdx = Op == G_EXTRACT_VECTOR_ELT ? 0 : 1;
    unsigned IdxTypeIdx = 2;

    getActionDefinitionsBuilder(Op)
      .legalIf([=](const LegalityQuery &Query) {
          const LLT &VecTy = Query.Types[VecTypeIdx];
          const LLT &IdxTy = Query.Types[IdxTypeIdx];
          return VecTy.getSizeInBits() % 32 == 0 &&
            VecTy.getSizeInBits() <= 512 &&
            IdxTy.getSizeInBits() == 32;
        })
      .clampScalar(EltTypeIdx, S32, S64)
      .clampScalar(VecTypeIdx, S32, S64)
      .clampScalar(IdxTypeIdx, S32, S32);
  }

  getActionDefinitionsBuilder(G_EXTRACT_VECTOR_ELT)
    .unsupportedIf([=](const LegalityQuery &Query) {
        const LLT &EltTy = Query.Types[1].getElementType();
        return Query.Types[0] != EltTy;
      });

  for (unsigned Op : {G_EXTRACT, G_INSERT}) {
    unsigned BigTyIdx = Op == G_EXTRACT ? 1 : 0;
    unsigned LitTyIdx = Op == G_EXTRACT ? 0 : 1;

    // FIXME: Doesn't handle extract of illegal sizes.
    getActionDefinitionsBuilder(Op)
      .legalIf([=](const LegalityQuery &Query) {
          const LLT BigTy = Query.Types[BigTyIdx];
          const LLT LitTy = Query.Types[LitTyIdx];
          return (BigTy.getSizeInBits() % 32 == 0) &&
                 (LitTy.getSizeInBits() % 16 == 0);
        })
      .widenScalarIf(
        [=](const LegalityQuery &Query) {
          const LLT BigTy = Query.Types[BigTyIdx];
          return (BigTy.getScalarSizeInBits() < 16);
        },
        LegalizeMutations::widenScalarOrEltToNextPow2(BigTyIdx, 16))
      .widenScalarIf(
        [=](const LegalityQuery &Query) {
          const LLT LitTy = Query.Types[LitTyIdx];
          return (LitTy.getScalarSizeInBits() < 16);
        },
        LegalizeMutations::widenScalarOrEltToNextPow2(LitTyIdx, 16))
      .moreElementsIf(isSmallOddVector(BigTyIdx), oneMoreElement(BigTyIdx));
  }

  // TODO: vectors of pointers
  getActionDefinitionsBuilder(G_BUILD_VECTOR)
      .legalForCartesianProduct(AllS32Vectors, {S32})
      .legalForCartesianProduct(AllS64Vectors, {S64})
      .clampNumElements(0, V16S32, V16S32)
      .clampNumElements(0, V2S64, V8S64)
      .minScalarSameAs(1, 0)
      // FIXME: Sort of a hack to make progress on other legalizations.
      .legalIf([=](const LegalityQuery &Query) {
        return Query.Types[0].getScalarSizeInBits() <= 32 ||
               Query.Types[0].getScalarSizeInBits() == 64;
      });

  // TODO: Support any combination of v2s32
  getActionDefinitionsBuilder(G_CONCAT_VECTORS)
    .legalFor({{V4S32, V2S32},
               {V8S32, V2S32},
               {V8S32, V4S32},
               {V4S64, V2S64},
               {V4S16, V2S16},
               {V8S16, V2S16},
               {V8S16, V4S16},
               {LLT::vector(4, LocalPtr), LLT::vector(2, LocalPtr)},
               {LLT::vector(4, PrivatePtr), LLT::vector(2, PrivatePtr)}});

  // Merge/Unmerge
  for (unsigned Op : {G_MERGE_VALUES, G_UNMERGE_VALUES}) {
    unsigned BigTyIdx = Op == G_MERGE_VALUES ? 0 : 1;
    unsigned LitTyIdx = Op == G_MERGE_VALUES ? 1 : 0;

    auto notValidElt = [=](const LegalityQuery &Query, unsigned TypeIdx) {
      const LLT &Ty = Query.Types[TypeIdx];
      if (Ty.isVector()) {
        const LLT &EltTy = Ty.getElementType();
        if (EltTy.getSizeInBits() < 8 || EltTy.getSizeInBits() > 64)
          return true;
        if (!isPowerOf2_32(EltTy.getSizeInBits()))
          return true;
      }
      return false;
    };

    getActionDefinitionsBuilder(Op)
      .widenScalarToNextPow2(LitTyIdx, /*Min*/ 16)
      // Clamp the little scalar to s8-s256 and make it a power of 2. It's not
      // worth considering the multiples of 64 since 2*192 and 2*384 are not
      // valid.
      .clampScalar(LitTyIdx, S16, S256)
      .widenScalarToNextPow2(LitTyIdx, /*Min*/ 32)

      // Break up vectors with weird elements into scalars
      .fewerElementsIf(
        [=](const LegalityQuery &Query) { return notValidElt(Query, 0); },
        scalarize(0))
      .fewerElementsIf(
        [=](const LegalityQuery &Query) { return notValidElt(Query, 1); },
        scalarize(1))
      .clampScalar(BigTyIdx, S32, S512)
      .widenScalarIf(
        [=](const LegalityQuery &Query) {
          const LLT &Ty = Query.Types[BigTyIdx];
          return !isPowerOf2_32(Ty.getSizeInBits()) &&
                 Ty.getSizeInBits() % 16 != 0;
        },
        [=](const LegalityQuery &Query) {
          // Pick the next power of 2, or a multiple of 64 over 128.
          // Whichever is smaller.
          const LLT &Ty = Query.Types[BigTyIdx];
          unsigned NewSizeInBits = 1 << Log2_32_Ceil(Ty.getSizeInBits() + 1);
          if (NewSizeInBits >= 256) {
            unsigned RoundedTo = alignTo<64>(Ty.getSizeInBits() + 1);
            if (RoundedTo < NewSizeInBits)
              NewSizeInBits = RoundedTo;
          }
          return std::make_pair(BigTyIdx, LLT::scalar(NewSizeInBits));
        })
      .legalIf([=](const LegalityQuery &Query) {
          const LLT &BigTy = Query.Types[BigTyIdx];
          const LLT &LitTy = Query.Types[LitTyIdx];

          if (BigTy.isVector() && BigTy.getSizeInBits() < 32)
            return false;
          if (LitTy.isVector() && LitTy.getSizeInBits() < 32)
            return false;

          return BigTy.getSizeInBits() % 16 == 0 &&
                 LitTy.getSizeInBits() % 16 == 0 &&
                 BigTy.getSizeInBits() <= 512;
        })
      // Any vectors left are the wrong size. Scalarize them.
      .scalarize(0)
      .scalarize(1);
  }

  computeTables();
  verify(*ST.getInstrInfo());
}

bool AMDGPULegalizerInfo::legalizeCustom(MachineInstr &MI,
                                         MachineRegisterInfo &MRI,
                                         MachineIRBuilder &MIRBuilder,
                                         GISelChangeObserver &Observer) const {
  switch (MI.getOpcode()) {
  case TargetOpcode::G_ADDRSPACE_CAST:
    return legalizeAddrSpaceCast(MI, MRI, MIRBuilder);
  default:
    return false;
  }

  llvm_unreachable("expected switch to return");
}

unsigned AMDGPULegalizerInfo::getSegmentAperture(
  unsigned AS,
  MachineRegisterInfo &MRI,
  MachineIRBuilder &MIRBuilder) const {
  MachineFunction &MF = MIRBuilder.getMF();
  const GCNSubtarget &ST = MF.getSubtarget<GCNSubtarget>();
  const LLT S32 = LLT::scalar(32);

  if (ST.hasApertureRegs()) {
    // FIXME: Use inline constants (src_{shared, private}_base) instead of
    // getreg.
    unsigned Offset = AS == AMDGPUAS::LOCAL_ADDRESS ?
        AMDGPU::Hwreg::OFFSET_SRC_SHARED_BASE :
        AMDGPU::Hwreg::OFFSET_SRC_PRIVATE_BASE;
    unsigned WidthM1 = AS == AMDGPUAS::LOCAL_ADDRESS ?
        AMDGPU::Hwreg::WIDTH_M1_SRC_SHARED_BASE :
        AMDGPU::Hwreg::WIDTH_M1_SRC_PRIVATE_BASE;
    unsigned Encoding =
        AMDGPU::Hwreg::ID_MEM_BASES << AMDGPU::Hwreg::ID_SHIFT_ |
        Offset << AMDGPU::Hwreg::OFFSET_SHIFT_ |
        WidthM1 << AMDGPU::Hwreg::WIDTH_M1_SHIFT_;

    unsigned ShiftAmt = MRI.createGenericVirtualRegister(S32);
    unsigned ApertureReg = MRI.createGenericVirtualRegister(S32);
    unsigned GetReg = MRI.createVirtualRegister(&AMDGPU::SReg_32RegClass);

    MIRBuilder.buildInstr(AMDGPU::S_GETREG_B32)
      .addDef(GetReg)
      .addImm(Encoding);
    MRI.setType(GetReg, S32);

    MIRBuilder.buildConstant(ShiftAmt, WidthM1 + 1);
    MIRBuilder.buildInstr(TargetOpcode::G_SHL)
      .addDef(ApertureReg)
      .addUse(GetReg)
      .addUse(ShiftAmt);

    return ApertureReg;
  }

  unsigned QueuePtr = MRI.createGenericVirtualRegister(
    LLT::pointer(AMDGPUAS::CONSTANT_ADDRESS, 64));

  // FIXME: Placeholder until we can track the input registers.
  MIRBuilder.buildConstant(QueuePtr, 0xdeadbeef);

  // Offset into amd_queue_t for group_segment_aperture_base_hi /
  // private_segment_aperture_base_hi.
  uint32_t StructOffset = (AS == AMDGPUAS::LOCAL_ADDRESS) ? 0x40 : 0x44;

  // FIXME: Don't use undef
  Value *V = UndefValue::get(PointerType::get(
                               Type::getInt8Ty(MF.getFunction().getContext()),
                               AMDGPUAS::CONSTANT_ADDRESS));

  MachinePointerInfo PtrInfo(V, StructOffset);
  MachineMemOperand *MMO = MF.getMachineMemOperand(
    PtrInfo,
    MachineMemOperand::MOLoad |
    MachineMemOperand::MODereferenceable |
    MachineMemOperand::MOInvariant,
    4,
    MinAlign(64, StructOffset));

  unsigned LoadResult = MRI.createGenericVirtualRegister(S32);
  unsigned LoadAddr = AMDGPU::NoRegister;

  MIRBuilder.materializeGEP(LoadAddr, QueuePtr, LLT::scalar(64), StructOffset);
  MIRBuilder.buildLoad(LoadResult, LoadAddr, *MMO);
  return LoadResult;
}

bool AMDGPULegalizerInfo::legalizeAddrSpaceCast(
  MachineInstr &MI, MachineRegisterInfo &MRI,
  MachineIRBuilder &MIRBuilder) const {
  MachineFunction &MF = MIRBuilder.getMF();

  MIRBuilder.setInstr(MI);

  unsigned Dst = MI.getOperand(0).getReg();
  unsigned Src = MI.getOperand(1).getReg();

  LLT DstTy = MRI.getType(Dst);
  LLT SrcTy = MRI.getType(Src);
  unsigned DestAS = DstTy.getAddressSpace();
  unsigned SrcAS = SrcTy.getAddressSpace();

  // TODO: Avoid reloading from the queue ptr for each cast, or at least each
  // vector element.
  assert(!DstTy.isVector());

  const AMDGPUTargetMachine &TM
    = static_cast<const AMDGPUTargetMachine &>(MF.getTarget());

  const GCNSubtarget &ST = MF.getSubtarget<GCNSubtarget>();
  if (ST.getTargetLowering()->isNoopAddrSpaceCast(SrcAS, DestAS)) {
    MI.setDesc(MIRBuilder.getTII().get(TargetOpcode::G_BITCAST));
    return true;
  }

  if (SrcAS == AMDGPUAS::FLAT_ADDRESS) {
    assert(DestAS == AMDGPUAS::LOCAL_ADDRESS ||
           DestAS == AMDGPUAS::PRIVATE_ADDRESS);
    unsigned NullVal = TM.getNullPointerValue(DestAS);

    unsigned SegmentNullReg = MRI.createGenericVirtualRegister(DstTy);
    unsigned FlatNullReg = MRI.createGenericVirtualRegister(SrcTy);

    MIRBuilder.buildConstant(SegmentNullReg, NullVal);
    MIRBuilder.buildConstant(FlatNullReg, 0);

    unsigned PtrLo32 = MRI.createGenericVirtualRegister(DstTy);

    // Extract low 32-bits of the pointer.
    MIRBuilder.buildExtract(PtrLo32, Src, 0);

    unsigned CmpRes = MRI.createGenericVirtualRegister(LLT::scalar(1));
    MIRBuilder.buildICmp(CmpInst::ICMP_NE, CmpRes, Src, FlatNullReg);
    MIRBuilder.buildSelect(Dst, CmpRes, PtrLo32, SegmentNullReg);

    MI.eraseFromParent();
    return true;
  }

  assert(SrcAS == AMDGPUAS::LOCAL_ADDRESS ||
         SrcAS == AMDGPUAS::PRIVATE_ADDRESS);

  unsigned FlatNullReg = MRI.createGenericVirtualRegister(DstTy);
  unsigned SegmentNullReg = MRI.createGenericVirtualRegister(SrcTy);
  MIRBuilder.buildConstant(SegmentNullReg, TM.getNullPointerValue(SrcAS));
  MIRBuilder.buildConstant(FlatNullReg, TM.getNullPointerValue(DestAS));

  unsigned ApertureReg = getSegmentAperture(DestAS, MRI, MIRBuilder);

  unsigned CmpRes = MRI.createGenericVirtualRegister(LLT::scalar(1));
  MIRBuilder.buildICmp(CmpInst::ICMP_NE, CmpRes, Src, SegmentNullReg);

  unsigned BuildPtr = MRI.createGenericVirtualRegister(DstTy);

  // Coerce the type of the low half of the result so we can use merge_values.
  unsigned SrcAsInt = MRI.createGenericVirtualRegister(LLT::scalar(32));
  MIRBuilder.buildInstr(TargetOpcode::G_PTRTOINT)
    .addDef(SrcAsInt)
    .addUse(Src);

  // TODO: Should we allow mismatched types but matching sizes in merges to
  // avoid the ptrtoint?
  MIRBuilder.buildMerge(BuildPtr, {SrcAsInt, ApertureReg});
  MIRBuilder.buildSelect(Dst, CmpRes, BuildPtr, FlatNullReg);

  MI.eraseFromParent();
  return true;
}
