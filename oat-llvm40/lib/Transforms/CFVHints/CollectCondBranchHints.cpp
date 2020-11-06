//==- CollectCondBranchHints.cpp - Collect Conditional Branch Hints for CFV ------------------===//
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
// every conditional branch, using 1 bit to record basically the taken or
// not taken info.
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include <string>
#include <vector>
#include <fstream>

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

#define DEBUG_TYPE "collect-cond-branch-hints"

// The MACROs defined below only for evaluation, controlling whether we instrument all function or not
#define INSTRUMENT_ALL

STATISTIC(NumOfCondBranches, "Number of conditional branch inst");

using namespace llvm;

typedef std::map<std::string, int> FunctionMap;

// Make it configurable whether or not to instrument the conditional branch.
static cl::opt<bool> CollectCondBranchHintsInfo("collect-cond-branch-hints", cl::Hidden,
                                  cl::init(true));

static cl::opt<std::string> InstrumentFunctionNameListFile("funclist", cl::Hidden, cl::desc("Specify input file that list the function names that need to be instrumented"),
                                  cl::init("funclist.txt"));

namespace {
struct  CollectCondBranchHints : public FunctionPass {
  static char ID;
  static FunctionMap *fMap;
  static int FID;
  static std::vector<std::string> *list;

  bool Modified = false;

  CollectCondBranchHints() : FunctionPass(ID) {
    list->clear();

    std::ifstream infile(InstrumentFunctionNameListFile);
    std::string word;
    while(infile>>word) {
        list->push_back(word);
    }
    if( list->empty()) {
        errs() << "Note: funclist is empty! use -funclist filename to designate funclist file" << "\n";
    }
  };

  bool runOnFunction(Function &F) override {
    std::string name = F.getName().str();

#ifdef INSTRUMENT_ALL // instrument all
#else // instrument selected functions
    errs() << __func__ << " : "<< name << "\n";
    if (std::find(std::begin(*list), std::end(*list), name) == std::end(*list)) {
        errs() << __func__ << ": skip function "<< name << ", for it is not in list"<<"\n";
        return false;
    }
#endif

    // check whether we have visit this function before
    if (fMap->find(name) != fMap->end()) {
	(*fMap)[name] = ++FID; 
    }

    for (BasicBlock &BB : F) {
        for  (Instruction &I : BB) {
            switch (I.getOpcode()) {
                case Instruction::Br: {
                    BranchInst *bi = cast<BranchInst>(&I);
                    if (bi->isUnconditional()) {
                        //errs() << *bi << "\n";
                    } else {
                        //errs() <<"CondBranch: " << *bi << "\n";
                        Modified |= instrumentCondBranch(&I, bi->getCondition());
                        NumOfCondBranches++;
                    }
                    break;
                }

                default:
                   //errs() << I << "\n";
                   break;
            }
        }	
    }

    return Modified;
  }

  bool instrumentCondBranch(Instruction *I, Value *cond);

};
} // end of namespace

bool CollectCondBranchHints::instrumentCondBranch(Instruction *I, Value *cond) {
    IRBuilder<> B(I);
    Module *M = B.GetInsertBlock()->getModule();
    Type *VoidTy = B.getVoidTy();
    Type *I1Ty = B.getInt1Ty();

    //errs() << __func__ << " : "<< *I<< "\n";

    Constant *ConstCollectCondBranchHints= M->getOrInsertFunction("__collect_cond_branch_hints", VoidTy,
                                                                     I1Ty, 
                                                                     nullptr);

    Function *FuncCollectCondBranchHints= cast<Function>(ConstCollectCondBranchHints);

    B.CreateCall(FuncCollectCondBranchHints, {cond});

    return true;

}

char CollectCondBranchHints::ID = 0;
int CollectCondBranchHints::FID = 0;
FunctionMap *CollectCondBranchHints::fMap= new FunctionMap();
std::vector<std::string> *CollectCondBranchHints::list = new std::vector<std::string>();
static RegisterPass<CollectCondBranchHints> X("collect-cond-branch-hints-pass", "Collect Conditional Branch Hints Info", false, false);
