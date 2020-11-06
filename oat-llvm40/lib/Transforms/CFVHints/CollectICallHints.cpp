//===- CollectICallHints.cpp - Collect Indirect Call hints ----------------===//
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
// uncertain part for building CFG. We will handle indirect calls in this 
// pass.
//
// TODO : Theoretically, we only need to instrument those indirect calls
// that has it has many or uncertain amount of possible target functions.
// However, that needs more analysis to assist our selection. As a initial
// version, we provide a framework for indirect call hints collection, where
// we treat all indirect call uniformly.
//
// For every indirect call, we record the following info
// triple <mother-function-id, possible-target-id>.
//   mother-function-id: the function that holds the indirect function call
//   icall-cite-count: the label of this indirect call instruction in mother func.
//   target-function-addr: the actual target function address at runtime. 
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include <string>

#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/IndirectCallSiteVisitor.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "collect-icall-hints"

using namespace llvm;

typedef std::map<std::string, int> FunctionMap;

namespace {
struct  CollectICallHints : public ModulePass {
  static char ID;
  static FunctionMap *fMap;
  static int FID;

  CollectICallHints() : ModulePass(ID) {}
  bool runOnModule(Module &M) override;
  bool processFunction(Function &);
  bool instrumentICall(Instruction *I, int fid, int count);
};
} // end of namespace

bool CollectICallHints::runOnModule(Module &M) {
  bool modified = false;
  for (auto &F : M) {
    //errs() << "runOnModule function name: " << F.getName() << "\n";
    if (F.isDeclaration())
      continue;
    if (F.hasFnAttribute(Attribute::OptimizeNone)) /* TODO:need further check */
      continue;
    modified |= CollectICallHints::processFunction(F);
  }

  return modified;
}

bool CollectICallHints::processFunction(Function &F) {
  bool modified = false;
  std::string name = F.getName().str();
  int fid;
  int count = 0;

  // check whether we have visit this function before
  if (fMap->find(name) != fMap->end()) {
      (*fMap)[name] = ++FID; 
  }

  fid = (*fMap)[name];

  errs() << "process function: " << F.getName() << "\n";

  for (auto &I : findIndirectCallSites(F)) {
    modified |= instrumentICall(I, fid, count);

    /* label the indirect call cites in this function */
    count++;
  }

  return modified;
}

bool CollectICallHints::instrumentICall(Instruction *I, int fid, int count) {
    IRBuilder<> B(I);
    Module *M = B.GetInsertBlock()->getModule();
    Type *VoidTy = B.getVoidTy();
    Type *I64Ty = B.getInt64Ty();
    ConstantInt *constFid, *constCount;
    Value *targetFunc;
    Value *castVal;

    errs() << __func__ << " : "<< *I<< "\n";
    errs() << __func__ << " fid : "<< fid << " count: " << count << "\n";

    if (auto *callInst = dyn_cast<CallInst>(I)) {
      targetFunc = callInst->getCalledValue();
    } else if (auto *invokeInst = dyn_cast<InvokeInst>(I)) {
      targetFunc = invokeInst->getCalledValue();
    } else {
      errs() << __func__ << "ERROR: unknown call inst: "<< *I<< "\n";
      return false;
    }

    assert(targetFunc != nullptr);

    Constant *ConstCollectICallHints= M->getOrInsertFunction("__collect_icall_hints", VoidTy,
                                                                     I64Ty,
                                                                     I64Ty, 
                                                                     I64Ty, 
                                                                     nullptr);

    Function *FuncCollectICallHints= cast<Function>(ConstCollectICallHints);
    constFid = ConstantInt::get((IntegerType*)I64Ty, fid);
    constCount = ConstantInt::get((IntegerType*)I64Ty, count);
    castVal = CastInst::Create(Instruction::PtrToInt, targetFunc, I64Ty, "ptrtoint", I);

    B.CreateCall(FuncCollectICallHints, {constFid, constCount, castVal});

    return true;
}

char CollectICallHints::ID = 0;
int CollectICallHints::FID = 0;
FunctionMap *CollectICallHints::fMap= new FunctionMap();
static RegisterPass<CollectICallHints> X("collect-icall-hints-pass", "Collect Indirect Call Hints Info", false, false);
