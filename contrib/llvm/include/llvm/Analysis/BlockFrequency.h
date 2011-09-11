//========-------- BlockFrequency.h - Block Frequency Analysis -------========//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Loops should be simplified before this analysis.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_BLOCKFREQUENCY_H
#define LLVM_ANALYSIS_BLOCKFREQUENCY_H

#include "llvm/Pass.h"
#include <climits>

namespace llvm {

class BranchProbabilityInfo;
template<class BlockT, class FunctionT, class BranchProbInfoT>
class BlockFrequencyImpl;

/// BlockFrequency pass uses BlockFrequencyImpl implementation to estimate
/// IR basic block frequencies.
class BlockFrequency : public FunctionPass {

  BlockFrequencyImpl<BasicBlock, Function, BranchProbabilityInfo> *BFI;

public:
  static char ID;

  BlockFrequency();

  ~BlockFrequency();

  void getAnalysisUsage(AnalysisUsage &AU) const;

  bool runOnFunction(Function &F);

  /// getblockFreq - Return block frequency. Never return 0, value must be
  /// positive. Please note that initial frequency is equal to 1024. It means
  /// that we should not rely on the value itself, but only on the comparison to
  /// the other block frequencies. We do this to avoid using of the floating
  /// points.
  uint32_t getBlockFreq(BasicBlock *BB);
};

}

#endif
