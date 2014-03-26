//===-- LegISelDAGToDAG.cpp - A dag to dag inst selector for Leg ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the Leg target.
//
//===----------------------------------------------------------------------===//

#include "Leg.h"
#include "LegTargetMachine.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include "LegInstrInfo.h"

using namespace llvm;

/// LegDAGToDAGISel - Leg specific code to select Leg machine
/// instructions for SelectionDAG operations.
///
namespace {
class LegDAGToDAGISel : public SelectionDAGISel {
  const LegSubtarget &Subtarget;

public:
  explicit LegDAGToDAGISel(LegTargetMachine &TM, CodeGenOpt::Level OptLevel)
      : SelectionDAGISel(TM, OptLevel), Subtarget(*TM.getSubtargetImpl()) {}

  SDNode *Select(SDNode *N);

  bool SelectAddr(SDValue Addr, SDValue &Base, SDValue &Offset);

  virtual const char *getPassName() const {
    return "Leg DAG->DAG Pattern Instruction Selection";
  }

private:
  SDNode *SelectMoveImmediate(SDNode *N);

// Include the pieces autogenerated from the target description.
#include "LegGenDAGISel.inc"
};
} // end anonymous namespace

bool LegDAGToDAGISel::SelectAddr(SDValue Addr, SDValue &Base, SDValue &Offset) {
  if (FrameIndexSDNode *FIN = dyn_cast<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(),
                                       getTargetLowering()->getPointerTy());
    Offset = CurDAG->getTargetConstant(0, MVT::i32);
    return true;
  }
  if (Addr.getOpcode() == ISD::TargetExternalSymbol ||
      Addr.getOpcode() == ISD::TargetGlobalAddress ||
      Addr.getOpcode() == ISD::TargetGlobalTLSAddress)
    return false; // direct calls.

  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, MVT::i32);
  return true;
}

SDNode *LegDAGToDAGISel::SelectMoveImmediate(SDNode *N) {
  // Make sure the immediate size is supported.
  ConstantSDNode *ConstVal = cast<ConstantSDNode>(N);
  uint64_t ImmVal = ConstVal->getZExtValue();
  uint64_t SupportedMask = 0xfffffffff;
  if ((ImmVal & SupportedMask) != ImmVal)
    return SelectCode(N);

  // Select the low part of the immediate move.
  uint64_t LoMask = 0xffff;
  uint64_t HiMask = 0xffff0000;
  uint64_t ImmLo = (ImmVal & LoMask);
  uint64_t ImmHi = (ImmVal & HiMask);
  SDValue ConstLo = CurDAG->getTargetConstant(ImmLo, MVT::i32);
  MachineSDNode *Move = CurDAG->getMachineNode(Leg::MOVWi16, N, MVT::i32,
                                               ConstLo);

  // Select the low part of the immediate move, if needed.
  if (ImmHi) {
    SDValue ConstHi = CurDAG->getTargetConstant(ImmHi >> 16, MVT::i32);
    Move = CurDAG->getMachineNode(Leg::MOVTi16, N, MVT::i32, SDValue(Move, 0),
                                  ConstHi);
  }

  return Move;
}

SDNode *LegDAGToDAGISel::Select(SDNode *N) {
  switch (N->getOpcode()) {
  case ISD::Constant:
    return SelectMoveImmediate(N);
  }

  return SelectCode(N);
}

/// createLegISelDag - This pass converts a legalized DAG into a
/// Leg-specific DAG, ready for instruction scheduling.
///
FunctionPass *llvm::createLegISelDag(LegTargetMachine &TM,
                                     CodeGenOpt::Level OptLevel) {
  return new LegDAGToDAGISel(TM, OptLevel);
}

