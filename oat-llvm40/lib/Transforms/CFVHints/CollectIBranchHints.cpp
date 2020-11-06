//===- CollectIBranchHints.cpp - Collect Indirect Branch Hints ------------===//
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
// is the chief culprit of the path explosion. Indirect calls/branches are
// also a uncertain part for building CFG. We will handle indirect branches in
// this pass.
//
// TODO : Theoretically, we only need to instrument those indirect branches 
// that has it has many or uncertain amount of possible target addresses.
// However, that needs more analysis to assist our selection. As a initial
// version, we provide a framework for indirect branches hints collection,
// where we treat all indirect branches uniformly.
//
// For every indirect branches, we record the following info
// triple <mother-function-id, possible-target-id>.
//   mother-function-id: the function that holds the indirect function call
//   ibranch-cite-count:
//   target-address:
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include <string>

#include "llvm/Analysis/CFG.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "collect-ibranch-hints"

using namespace llvm;

typedef std::map<std::string, int> FunctionMap;

namespace {
struct  CollectIBranchHints : public FunctionPass {
  static char ID;
  static FunctionMap *fMap;
  static int FID;

  CollectIBranchHints() : FunctionPass(ID) {}
  bool runOnFunction(Function &F) override;
  bool instrumentIBranch(Instruction *I, int fid, int count);
};
} // end of namespace

bool CollectIBranchHints::runOnFunction(Function &F) {
  bool modified = false;
  std::string name = F.getName().str();
  int fid;
  int count = 0;

  // check whether we have visit this function before
  if (fMap->find(name) != fMap->end()) {
      (*fMap)[name] = ++FID; 
  }

  fid = (*fMap)[name];

  for (auto &BB : F) {
    //errs() << "runOnFunction basicblock name: " << BB.getName() << "\n";

    if (IndirectBrInst *IBI = dyn_cast<IndirectBrInst>(BB.getTerminator())) {  
      modified |= instrumentIBranch(IBI, fid, count);

      /* label the indirect branch cites in this function */
      count++;
    }
  }

  return modified;
}

bool CollectIBranchHints::instrumentIBranch(Instruction *I, int fid, int count) {
    IRBuilder<> B(I);
    Module *M = B.GetInsertBlock()->getModule();
    Type *VoidTy = B.getVoidTy();
    Type *I64Ty = B.getInt64Ty();
    ConstantInt *constFid, *constCount;
    Value *target;
    Value *castVal;

    errs() << __func__ << " : "<< *I<< "\n";
    errs() << __func__ << " fid : "<< fid << " count: " << count << "\n";

    if (auto *indirectBranchInst = dyn_cast<IndirectBrInst>(I)) {
      target = indirectBranchInst->getAddress();
    } else {
      errs() << __func__ << "ERROR: unknown indirect branch inst: "<< *I<< "\n";
      return false;
    }

    assert(target != nullptr);

    Constant *ConstCollectIBranchHints= M->getOrInsertFunction("__collect_ibranch_hints", VoidTy,
                                                                     I64Ty,
                                                                     I64Ty, 
                                                                     I64Ty, 
                                                                     nullptr);

    Function *FuncCollectIBranchHints= cast<Function>(ConstCollectIBranchHints);
    constFid = ConstantInt::get((IntegerType*)I64Ty, fid);
    constCount = ConstantInt::get((IntegerType*)I64Ty, count);
    castVal = CastInst::Create(Instruction::PtrToInt, target, I64Ty, "ptrtoint", I);

    B.CreateCall(FuncCollectIBranchHints, {constFid, constCount, castVal});

    return true;
}

char CollectIBranchHints::ID = 0;
int CollectIBranchHints::FID = 0;
FunctionMap *CollectIBranchHints::fMap= new FunctionMap();
static RegisterPass<CollectIBranchHints> X("collect-ibranch-hints-pass", "Collect Indirect Branch Hints Info", false, false);
