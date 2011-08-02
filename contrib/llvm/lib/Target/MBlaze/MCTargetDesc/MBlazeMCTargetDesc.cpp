//===-- MBlazeMCTargetDesc.cpp - MBlaze Target Descriptions -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides MBlaze specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "MBlazeMCTargetDesc.h"
#include "MBlazeMCAsmInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Target/TargetRegistry.h"

#define GET_INSTRINFO_MC_DESC
#include "MBlazeGenInstrInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "MBlazeGenSubtargetInfo.inc"

#define GET_REGINFO_MC_DESC
#include "MBlazeGenRegisterInfo.inc"

using namespace llvm;


static MCInstrInfo *createMBlazeMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitMBlazeMCInstrInfo(X);
  return X;
}

extern "C" void LLVMInitializeMBlazeMCInstrInfo() {
  TargetRegistry::RegisterMCInstrInfo(TheMBlazeTarget, createMBlazeMCInstrInfo);
}

static MCSubtargetInfo *createMBlazeMCSubtargetInfo(StringRef TT, StringRef CPU,
                                                    StringRef FS) {
  MCSubtargetInfo *X = new MCSubtargetInfo();
  InitMBlazeMCSubtargetInfo(X, TT, CPU, FS);
  return X;
}

extern "C" void LLVMInitializeMBlazeMCSubtargetInfo() {
  TargetRegistry::RegisterMCSubtargetInfo(TheMBlazeTarget,
                                          createMBlazeMCSubtargetInfo);
}

static MCAsmInfo *createMCAsmInfo(const Target &T, StringRef TT) {
  Triple TheTriple(TT);
  switch (TheTriple.getOS()) {
  default:
    return new MBlazeMCAsmInfo();
  }
}

extern "C" void LLVMInitializeMBlazeMCAsmInfo() {
  RegisterMCAsmInfoFn X(TheMBlazeTarget, createMCAsmInfo);
}
