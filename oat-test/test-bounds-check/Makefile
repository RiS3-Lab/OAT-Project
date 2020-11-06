CLANG = ~/work/llvm-env/build/bin/clang
OPT = ~/work/llvm-env/build/bin/opt
TEST ?= test_combo
CC = aarch64-linux-gnu-gcc
NUC_PC ?= name@ip
objdump ?= aarch64-linux-gnu-objdump 
hikey ?= linaro@ip
TEEC_EXPORT ?= /path/to/work/tee_workspace/devel/optee_hikey_debian/optee_client/out/export
TA ?= /path/to/work/tee_workspace/devel/optee_hikey_debian/hello_world/ta

CFLAGS += -Wall -I$(TA)/include -I$(TEEC_EXPORT)/include -I./include
#Add/link other required libraries here
LDADD += -lteec -L$(TEEC_EXPORT)/lib

NOVA_PATH = ~/work/llvm-env/build/lib/
LLC_ARM = ~/work/llvm-env/build/bin/llc
DIS = ~/work/llvm-env/build/bin/llvm-dis
RT = /path/to/work/ra-project/data/

test:
	$(CLANG) -S -emit-llvm test.c -o test.bc

test-combo: test 
	llvm-link test.bc -o test_combo.bc

opt-test: test-combo
	$(OPT) -load $(NOVA_PATH)/LLVMNova.so -nova < test_combo.bc > combo.bc 
	$(OPT) -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-ibranch-hints-pass -collect-icall-hints-pass -collect-loop-hints-pass < combo.bc > combo_hints.bc
	$(LLC_ARM) -march=aarch64  -aarch64-enable-cfv combo_hints.bc -o combo.s
	$(DIS) combo.bc -o combo.ll
	$(CLANG) combo.bc -L$(RT)/runtime -lm -lrt -lnova -lsoftboundcets_rt
