//===----------------------------------------------------------------------===//
//
// This file contains a pass that add control flow check for indirect call/jmp,
// ret instruction.
// This pass should be run at the late end of the compilation flow, just before
// Linker Optimization Hint(LOH) pass.
//
// For indirect call/jmp, ret instruction, it works as follows:
// 
// =*= indirect jmp =*=
// before insert check
//      L1: br xA
// after insert check
//          push x0 
//          mov x0, xA
//          bl __control_flow_check /* param0:target_addr, src_addr could be get from lr reg */
//          pop x0 
//      L1: br xA
//
// =*= indirect call =*=
// before insert check
//      L1: blr xA
// after insert check
//          push x0 
//          mov x0, xA
//          bl __control_flow_check /* param0:target_addr, src_addr could be get from lr reg */
//          pop x0 
//      L1: blr xA
//
// =*= ret =*=
// before insert check
//      L1: ret [xA]
// after insert check
//          push x0 
//          mov x0, [xA] /* xA default is lr */
//          bl __control_flow_check /* param0:target_addr, src_addr could be get from lr reg */
//          pop x0 
//      L1: ret [xA]
//
//===----------------------------------------------------------------------===//

#include "AArch64.h"
#include "AArch64InstrInfo.h"
#include "AArch64Subtarget.h"
#include "AArch64TargetMachine.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

#include "AArch64ControlFlowVerification.h"


using namespace llvm;

#define DEBUG_TYPE "aarch64-control-flow-verification"

char AArch64ControlFlowVerification::ID = 0;
        
INITIALIZE_PASS(AArch64ControlFlowVerification, DEBUG_TYPE, AARCH64_CONTROL_FLOW_VERIFICATION_NAME, false, false)

// verifiy indirect call inst, blr xR
bool AArch64ControlFlowVerification::instrumentIndirectCall (MachineBasicBlock &MBB,
                         MachineInstr &MI,
                         const DebugLoc &DL,
                         const TargetInstrInfo *TII,
                         const char *sym) {
    unsigned targetReg;

    DEBUG(dbgs() << __func__ << "\n");
    DEBUG(MI.print(dbgs()));

    // get target register xR
    targetReg = MI.getOperand(0).getReg();

    return handleControlTransfer(MBB, MI, DL, TII, sym, targetReg);
}

bool AArch64ControlFlowVerification::handleControlTransfer(MachineBasicBlock &MBB,
                         MachineInstr &MI,
                         const DebugLoc &DL,
                         const TargetInstrInfo *TII,
                         const char *sym,
                         unsigned targetReg) {

    MachineInstr *BMI;

    DEBUG(dbgs() << __func__ << "\n");

    // sub sp, sp, 16
    BMI = BuildMI(MBB, MI, DL, TII->get(AArch64::SUBXri))
        .addReg(AArch64::SP)
        .addReg(AArch64::SP)
        .addImm(16)
        .addImm(0); /*shift imm*/

    DEBUG(BMI->print(dbgs()));

    // stp r0,lr, [sp]
    BMI = BuildMI(MBB, MI, DL, TII->get(AArch64::STPXi))
        .addReg(AArch64::X0, RegState::Kill) //src reg
        .addReg(AArch64::LR) //src reg
        .addReg(AArch64::SP)
        .addImm(0); /*offset imm*/

    DEBUG(BMI->print(dbgs()));

    // original inst: mov xR, r0
    // replaced with : orr r0, xR, XZR
    BMI = BuildMI(MBB, MI, DL, TII->get(AArch64::ORRXrs))
        .addReg(AArch64::X0, RegState::Define)
        .addReg(targetReg)
        .addReg(AArch64::XZR)
        .addImm(0); /* shift imm */

    DEBUG(BMI->print(dbgs()));

    // bl sym
    BMI = BuildMI(MBB,MI,DL,TII->get(AArch64::BL)).addExternalSymbol(sym);

    DEBUG(BMI->print(dbgs()));

    // ldp x0,lr [sp]
    BMI = BuildMI(MBB, MI, DL, TII->get(AArch64::LDPXi))
        .addReg(AArch64::X0, RegState::Define) //src1 reg
        .addReg(AArch64::LR, RegState::Define) //src2 reg
        .addReg(AArch64::SP)
        .addImm(0); /* offset imm */

    DEBUG(BMI->print(dbgs()));

    // add sp, sp, 16
    BMI = BuildMI(MBB, MI, DL, TII->get(AArch64::ADDXri))
        .addReg(AArch64::SP)
        .addReg(AArch64::SP)
        .addImm(16)
        .addImm(0); /* shift imm */

    DEBUG(BMI->print(dbgs()));

    return true;
}

// verifiy indirect jmp inst, br xR
bool AArch64ControlFlowVerification::instrumentIndirectJump (MachineBasicBlock &MBB,
                         MachineInstr &MI,
                         const DebugLoc &DL,
                         const TargetInstrInfo *TII,
                         const char *sym) {

    unsigned targetReg;

    DEBUG(dbgs() << __func__ << "\n");
    DEBUG(MI.print(dbgs()));

    // get target register xR
    targetReg = MI.getOperand(0).getReg();

    return handleControlTransfer(MBB, MI, DL, TII, sym, targetReg);
}

// verifiy ret inst, ret [xR], xR default is lr
bool AArch64ControlFlowVerification::instrumentRet (MachineBasicBlock &MBB,
                         MachineInstr &MI,
                         const DebugLoc &DL,
                         const TargetInstrInfo *TII,
                         const char *sym) {
    unsigned targetReg;

    DEBUG(dbgs() << __func__ << "\n");
    DEBUG(MI.print(dbgs()));

    // get target register xR
    targetReg = MI.getOperand(0).getReg();

    return handleControlTransfer(MBB, MI, DL, TII, sym, targetReg);
}

// verifiy ret inst, ret, lr as target register
bool AArch64ControlFlowVerification::instrumentRetLR (MachineBasicBlock &MBB,
                         MachineInstr &MI,
                         const DebugLoc &DL,
                         const TargetInstrInfo *TII,
                         const char *sym) {
    unsigned targetReg;

    DEBUG(dbgs() << __func__ << "\n");
    DEBUG(MI.print(dbgs()));

    // get target register LR 
    targetReg = AArch64::LR;

    return handleControlTransfer(MBB, MI, DL, TII, sym, targetReg);
}

bool AArch64ControlFlowVerification::runOnMachineFunction(MachineFunction &MF) {
  bool MadeChange = false;
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  const char* symICall = "__cfv_icall";
  const char* symIJmp = "__cfv_ijmp";
  const char* symRet = "__cfv_ret";

  DEBUG(dbgs() << "***** AArch64ControlFlowVerification *****\n");

  for (MachineFunction::iterator FI = MF.begin(); FI != MF.end(); ++FI) {
    MachineBasicBlock& MBB = *FI;

    // traverse all the instructions inside the machine basic block
    for (MachineBasicBlock::iterator I = MBB.begin(); I!=MBB.end(); ++I) {
      MachineInstr &MI = *I;
      if (MI.getDesc().isCall() ||
          MI.getDesc().isIndirectBranch() ||
          MI.getDesc().isReturn()) {
        switch(MI.getOpcode()) {
          case AArch64::BLR:
            MadeChange |= instrumentIndirectCall(MBB,MI,MI.getDebugLoc(),TII,symICall);
            break;
          case AArch64::BR:
            MadeChange |= instrumentIndirectJump(MBB,MI,MI.getDebugLoc(),TII,symIJmp);
            break;
          case AArch64::RET:
            MadeChange |= instrumentRet(MBB,MI,MI.getDebugLoc(),TII,symRet);
            break;
          case AArch64::RET_ReallyLR:
            MadeChange |= instrumentRetLR(MBB,MI,MI.getDebugLoc(),TII,symRet);
            break;
          default:
	    /* Skip direct call(BL) instructions! */
            break;
        }
      }
    }
  }

  return MadeChange;
}

FunctionPass *llvm::createAArch64ControlFlowVerificationPass() {
  return new AArch64ControlFlowVerification();
}
