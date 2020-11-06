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

#ifndef LLVM_LIB_TARGET_AARCH64_AARCH64ControlFlowVerification_H
#define LLVM_LIB_TARGET_AARCH64_AARCH64ControlFlowVerification_H

#include "llvm/CodeGen/MachineFunctionPass.h"

#define AARCH64_CONTROL_FLOW_VERIFICATION_NAME "AArch64 Control Flow Verification pass"

namespace llvm {
class AArch64ControlFlowVerification : public MachineFunctionPass {
  
  // verifiy indirect call inst, blr xR
  bool instrumentIndirectCall (MachineBasicBlock &MBB,
                           MachineInstr &MI,
                           const DebugLoc &DL,
                           const TargetInstrInfo *TII,
                           const char *sym);

  // verifiy indirect jmp inst, br xR
  bool instrumentIndirectJump (MachineBasicBlock &MBB,
                           MachineInstr &MI,
                           const DebugLoc &DL,
                           const TargetInstrInfo *TII,
                           const char *sym);

  // verifiy ret inst, ret [xR], xR default is lr
  bool instrumentRet (MachineBasicBlock &MBB,
                           MachineInstr &MI,
                           const DebugLoc &DL,
                           const TargetInstrInfo *TII,
                           const char *sym);

  // verifiy ret inst, ret, lr as target register
  bool instrumentRetLR (MachineBasicBlock &MBB,
                           MachineInstr &MI,
                           const DebugLoc &DL,
                           const TargetInstrInfo *TII,
                           const char *sym);

  // called by other instrumentXXX functions, do the real job!
  bool handleControlTransfer(MachineBasicBlock &MBB,
                           MachineInstr &MI,
                           const DebugLoc &DL,
                           const TargetInstrInfo *TII,
                           const char *sym,
                           unsigned targetReg);

public:
  static char ID;
  AArch64ControlFlowVerification() : MachineFunctionPass(ID) { }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return AARCH64_CONTROL_FLOW_VERIFICATION_NAME;
  }

};

} // End llvm namespace

#endif
