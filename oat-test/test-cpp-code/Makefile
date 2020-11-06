TEST ?= test_combo
CC = aarch64-linux-gnu-gcc
NUC_PC ?= name@ip
objdump ?= aarch64-linux-gnu-objdump 
hikey ?= linaro@ip
TEEC_EXPORT ?= /path/to/work/tee_workspace/devel/optee_hikey_debian/optee_client/out/export
TA ?= /path/to/work/tee_workspace/devel/optee_hikey_debian/hello_world/ta

#Add/link other required libraries here
LDADD += -lteec -L$(TEEC_EXPORT)/lib

NOVA_PATH = ~/work/llvm-env/build/lib/
LLC_ARM = ~/work/llvm-env/build/bin/llc
LLVM_PATH = ~/work/llvm-env/build/bin/



send-combo-s:
	scp combo.s pc:~/tmp/hikey-relay/

asm-combo:
	$(LLC_ARM) -march=aarch64  -aarch64-enable-cfv=false test_combo.bc -o combo.s

opt-hello:
	opt -load $(NOVA_PATH)/LLVMNova.so -nova < hello.bc > combo.bc 2>/dev/null
	opt -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-ibranch-hints-pass -collect-icall-hints-pass -collect-loop-hints-pass < combo.bc > combo_hints.bc
	$(LLC_ARM) -march=aarch64  -aarch64-enable-cfv combo_hints.bc -o combo.s

local-test: vf-combo
	#$(LLVM_PATH)/opt -load $(NOVA_PATH)/LLVMNova.so -nova < test_combo.bc > combo.bc
	#$(LLVM_PATH)/opt -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-ibranch-hints-pass -collect-icall-hints-pass -collect-loop-hints-pass < vf_combo.bc > combo_hints.bc
	$(LLVM_PATH)/opt -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-cond-branch-hints-pass < vf_combo.bc > combo_hints.bc

dis-combo:
	llvm-dis < combo_hints.bc >combo_hints.dis


vf:
	$(LLVM_PATH)/clang++ -S -emit-llvm vf.cpp -o vf.bc

vf-combo: vf
	$(LLVM_PATH)/llvm-link vf.bc -o vf_combo.bc

opt-vf: vf-combo
	opt -load $(NOVA_PATH)/LLVMNova.so -nova < vf_combo.bc > combo.bc 
	opt -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-ibranch-hints-pass -collect-icall-hints-pass -collect-loop-hints-pass < combo.bc > combo_hints.bc
	$(LLC_ARM) -march=aarch64  -aarch64-enable-cfv combo_hints.bc -o combo.s
