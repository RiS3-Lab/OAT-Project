TEST ?= test_combo
CC = aarch64-linux-gnu-gcc
NUC_PC ?= your-desktop-ip 
objdump ?= aarch64-linux-gnu-objdump 
hikey ?= hikey-ip
TEEC_EXPORT ?= /your/path/to/work/tee_workspace/devel/optee_hikey_debian/optee_client/out/export
TA ?= /your/path/to/work/tee_workspace/devel/optee_hikey_debian/hello_world/ta

CFLAGS += -Wall -I$(TA)/include -I$(TEEC_EXPORT)/include -I./include
#Add/link other required libraries here
LDADD += -lteec -L$(TEEC_EXPORT)/lib

NOVA_PATH = ~/work/llvm-env/build/lib/
LLC_ARM = ~/work/llvm-env/build/bin/llc
OPT = ~/work/llvm-env/build/bin/opt

VM =  your-server-ip:~/work/ra-project/evaluation/syringe-cb/

util:
	clang -S -emit-llvm util.c -o util.bc

lcd:
	clang -S -emit-llvm LiquidCrystal.c -o LiquidCrystal.bc

led:
	clang -S -emit-llvm led.c -o led.bc

syringe:
	clang -S -emit-llvm syringePump_orig.c -o syringePump.bc

test-combo: syringe led lcd util
	llvm-link  syringePump.bc util.bc led.bc LiquidCrystal.bc -o test_combo.bc

opt-combo:
	#$(opt) -load $(NOVA_PATH)/LLVMNova.so -nova < test_combo.bc > combo.bc 2>/dev/null
	$(OPT) -load $(NOVA_PATH)/LLVMNova.so -nova < test_combo.bc > combo.bc 2>clog.txt
	$(OPT) -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-ibranch-hints-pass -collect-icall-hints-pass -collect-loop-hints-pass < combo.bc > combo_hints.bc
	$(LLC_ARM) -march=aarch64  -aarch64-enable-cfv combo_hints.bc -o combo.s

obj-orig: test-combo
	llc -march=aarch64  test_combo.bc -o combo.s
	clang combo.s 

send-combo-bc:
	scp -i vm test_combo.bc $(VM)

send-combo-s:
	scp combo.s your-machine:~/tmp/hikey-relay/

asm-combo:
	$(LLC_ARM) -march=aarch64  -aarch64-enable-cfv=false test_combo.bc -o combo.s

opt-hello:
	$(opt) -load $(NOVA_PATH)/LLVMNova.so -nova < hello.bc > combo.bc 2>/dev/null
	$(opt) -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-ibranch-hints-pass -collect-icall-hints-pass -collect-loop-hints-pass < combo.bc > combo_hints.bc
	$(LLC_ARM) -march=aarch64  -aarch64-enable-cfv combo_hints.bc -o combo.s

bin:
	clang combo.s -L../../data/runtime -lm -lrt  -lsoftboundcets_rt  -lnova -lteec
