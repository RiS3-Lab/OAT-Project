//==- CollectLoopHints.cpp - Collect Loop Hints for CFV ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This pass is in cooperation with the Control-Flow Verification pass for
// AArch64 backend. In order to provide hints info for Verifier to quickly
// verify the control-flow HASH value, we need to collect hints info for
// those control-flow event that might cause path explosion. Among them, loop
// is the chief culprit of the path explosion. Indirect calls are also a
// uncertain part for building CFG. We will handle indirect calls in other
// passes.
//
// TODO : Theoretically, we only need to instrument those loops that has a
// indirect jump or indirect call inside its body. However, function calls
// and jump instructions inside loop may make it difficult to make the choice,
// so this initial version will instrument all loops.
//
// For every loop inside a function will be assigned a label, which is a 
// triple <function-name, loop-count, loop-level>.
//   function-name: we may collect function name info and assign an id to it.
//   loop-count: for loops at the same level, they are assigned a count.
//   loop-level: loop might be nested, so we need a level number to distinguish
//               loops at different level.
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include <string>

#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Mangler.h" 
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "collect-loop-hints"

STATISTIC(NumOfAffectedLoops, "Number of entry safepoints inserted");

using namespace llvm;

typedef std::map<std::string, int> FunctionMap;

// Make it configurable whether or not to instrument the loop.
static cl::opt<bool> CollectLoopHintsInfo("collect-loop-hints", cl::Hidden,
                                  cl::init(true));

namespace {
struct  CollectLoopHints : public FunctionPass {
  static char ID;
  static FunctionMap *fMap;
  static int FID;

  bool Modified = false;
  LoopInfo *LI = nullptr;

  CollectLoopHints() : FunctionPass(ID) {};

  bool runOnLoop(Loop *, int, int, int);
  bool runOnLoopAndSubLoops(Loop *L, int fid, int level, int count) {
    bool modified = false;
    int localCount = 0;

    // Visit all the subloops
    for (Loop *I : *L) {
      modified |= runOnLoopAndSubLoops(I, fid, level + 1, localCount);
      localCount++;
    }
    modified |= runOnLoop(L, fid, level, count);

    return modified;
  }

  bool runOnFunction(Function &F) override {
    LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    std::string name = F.getName().str();
    int count = 0;
    int fid;

    // check whether we have visit this function before
    if (fMap->find(name) != fMap->end()) {
	(*fMap)[name] = ++FID; 
    }

    fid = (*fMap)[name];

    for (Loop *I : *LI) {
      Modified |= runOnLoopAndSubLoops(I, fid, /*initial level*/0, count);
      count++;
    }
    return Modified;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LoopInfoWrapperPass>();
    // We need to modify IR, so we can't indicate AU to preserve analysis results. 
    // AU.setPreservesAll();
  }

  bool instrumentLoopHeader(Instruction *I, int fid, int level, int count);

};
} // end of namespace

bool CollectLoopHints::runOnLoop(Loop *L, int fid, int level, int count) {
  // TODO: do instrumentation later
  BasicBlock *Header = L->getHeader();
  Instruction *I = &(*(Header->begin()));
  bool modified;

  NumOfAffectedLoops++;

  modified = instrumentLoopHeader(I, fid, level, count);

  errs() << __func__ << "function id: " << fid << " level: " << level << " count: " << count << "\n";
  errs() << "header : " << Header->getName() << "\n";

  return modified;
}

bool CollectLoopHints::instrumentLoopHeader(Instruction *I, int fid, int level, int count) {
    IRBuilder<> B(I);
    Module *M = B.GetInsertBlock()->getModule();
    Type *VoidTy = B.getVoidTy();
    Type *I32Ty = B.getInt32Ty();
    ConstantInt *cfid, *clevel, *ccount;

    errs() << __func__ << " : "<< *I<< "\n";

    Constant *ConstCollectLoopHints= M->getOrInsertFunction("__collect_loop_hints", VoidTy,
                                                                     I32Ty,
                                                                     I32Ty, 
                                                                     I32Ty, 
                                                                     nullptr);

    Function *FuncCollectLoopHints= cast<Function>(ConstCollectLoopHints);
    cfid = ConstantInt::get((IntegerType*)I32Ty, fid);
    clevel = ConstantInt::get((IntegerType*)I32Ty, level);
    ccount = ConstantInt::get((IntegerType*)I32Ty, count);

    B.CreateCall(FuncCollectLoopHints, {cfid, clevel, ccount});

    return true;
}

char CollectLoopHints::ID = 0;
int CollectLoopHints::FID = 0;
FunctionMap *CollectLoopHints::fMap= new FunctionMap();
static RegisterPass<CollectLoopHints> X("collect-loop-hints-pass", "Collect Loop Hints Info", false, false);
