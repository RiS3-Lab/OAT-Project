# I guess that requiring people to instal Scons is silly, so I'll add this
# Makefile.

OPT = ~/work/llvm-env/build/bin/opt
VM = your-server-for-llvm:~/work/ra-project/evaluation/rc-cb/

NOVA_PATH = ~/work/llvm-env/build/lib/
LLC_ARM = ~/work/llvm-env/build/bin/llc

UNAME := $(shell uname -m)
WARNING_FLAGS := -Wall -Wextra -Wshadow -Wswitch-enum \
    -Wswitch-default
C_WARNING_FLAGS := -Wmissing-prototypes -Wmissing-declarations \
    -Wstrict-prototypes

ifeq ($(UNAME), armv6l)
	CFLAGS := ${WARNING_FLAGS} ${C_WARNING_FLAGS} -DRASPI=1
else ifeq ($(UNAME), armv7l)
	CFLAGS := ${WARNING_FLAGS} ${C_WARNING_FLAGS} -DRASPI=2
else
	CFLAGS := ${WARNING_FLAGS} ${C_WARNING_FLAGS}
endif

LDLIBS := -lm -ljansson

#pi_pcm: pi_pcm.o mailbox.o
pi_pcm: 
	clang $(CFLAGS) $(LDLIBS) pi_pcm.c -o server

pi-pcm-bc:
	#clang -D_GNU_SOURCE --target=aarch64 -S -emit-llvm pi_pcm.c -o pi_pcm.bc
	clang -S -emit-llvm  pi_pcm.c -o pi_pcm.bc

test-combo: pi-pcm-bc
	llvm-link pi_pcm.bc -o test_combo.bc


opt-combo:
	$(OPT) -load $(NOVA_PATH)/LLVMNova.so -nova < test_combo.bc > combo.bc 2>err.log
	$(OPT) -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-ibranch-hints-pass -collect-icall-hints-pass -collect-cond-branch-hints-pass < combo.bc > combo_hints.bc 2>>err.log
	$(LLC_ARM) -march=aarch64  -aarch64-enable-cfv combo_hints.bc -o combo.s

send-combo-bc:
	scp -i vm test_combo.bc $(VM)

fetch-combo-s:
	scp $(VM)/combo.s . 

bin: test-combo
	clang combo.s -L../../data/runtime -lm -ljansson -lrt  -lsoftboundcets_rt  -lnova -lteec -lc 

run-client: 
	./interactive_control.py example.json 
	
run-server: pi_pcm
	./server

sync:
	cp pi_pcm.c Makefile interactive_control.py ~/tee_workspace/ra-project/evaluation/rc-cb/ 

.PHONY:	clean
clean:
	rm -f *.o
	rm -f *.pyc
	rm -f pi_pcm

