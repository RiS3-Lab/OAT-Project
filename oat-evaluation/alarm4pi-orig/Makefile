TEST ?= test_combo
#CC = aarch64-linux-gnu-gcc
CC = gcc
NUC_PC ?= your-deskop-ip
NUC_TARGET ?= ~/work/ra-project/evaluation/alarm4pi/
objdump ?= aarch64-linux-gnu-objdump 
hikey ?= your-hikey-ip 
TEEC_EXPORT ?= /home/linaro/work/tee/export/
TA ?= /home/linaro/work/tee/ta/
hikey_target ?= ~/work/alarm4pi/

NOVA_PATH = ~/work/llvm-env/build/lib/
LLC_ARM = ~/work/llvm-env/build/bin/llc

CFLAGS += -Wall -I$(TA)/include -I$(TEEC_EXPORT)/include -I./include
#Add/link other required libraries here
LDADD += -lteec -L$(TEEC_EXPORT)/lib -lresolv -pthread
LDADD_ORIG += -lresolv -pthread
OPT = ~/work/llvm-env/build/bin/opt
VM = your-server-ip:/work/ra-project/evaluation/alarm4pi-cb/

util:
	clang -D_GNU_SOURCE --target=aarch64 -S -emit-llvm util.c -o util.bc

bcm-gpio:
	clang -D_GNU_SOURCE --target=aarch64 -S -emit-llvm bcm_gpio.c -o bcm_gpio.bc

alarm4pi:
	clang -D_GNU_SOURCE --target=aarch64 -S -emit-llvm alarm4pi.c -o alarm4pi.bc

log-msgs:
	clang -D_GNU_SOURCE --target=aarch64 -S -emit-llvm log_msgs.c -o log_msgs.bc

proc-helper:
	clang -D_GNU_SOURCE --target=aarch64 -S -emit-llvm proc_helper.c -o proc_helper.bc

public-ip:
	clang -D_GNU_SOURCE --target=aarch64 -S -emit-llvm public_ip.c -o public_ip.bc

pushover:
	clang -D_GNU_SOURCE --target=aarch64 -S -emit-llvm pushover.c -o pushover.bc

gpio-polling:
	clang -D_GNU_SOURCE --target=aarch64 -S -emit-llvm gpio_polling.c -o gpio_polling.bc

gpio-polling-orig:
	clang -D_GNU_SOURCE --target=aarch64 -S -emit-llvm gpio_polling_orig.c -o gpio_polling_orig.bc

test-orig: util alarm4pi gpio-polling-orig pushover public-ip proc-helper log-msgs bcm-gpio
	$(CC) $(LDADD_ORIG) -g util.o alarm4pi.o gpio_polling_orig.o pushover.o public_ip.o proc_helper.o log_msgs.o bcm_gpio.o  -o test_orig
	
test-combo: util alarm4pi gpio-polling pushover public-ip proc-helper log-msgs bcm-gpio 
	llvm-link util.bc alarm4pi.bc gpio_polling.bc pushover.bc public_ip.bc proc_helper.bc log_msgs.bc bcm_gpio.bc -o test_combo.bc 

opt-combo:
	$(OPT) -load $(NOVA_PATH)/LLVMNova.so -nova < test_combo.bc > combo.bc 2>err.log
	$(OPT) -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-ibranch-hints-pass -collect-icall-hints-pass -collect-loop-hints-pass < combo.bc > combo_hints.bc 2>>err.log
	$(LLC_ARM) -march=aarch64  -aarch64-enable-cfv combo_hints.bc -o combo.s

send-combo-s:
	scp combo.s machine:~/tmp/hikey-relay/

send-combo-bc:
	scp -i vm test_combo.bc $(VM)

fetch-combo-s:
	scp $(VM)/combo.s . 

bin: test-combo
	clang combo.s -L../../data/runtime -lm -lrt  -lsoftboundcets_rt  -lnova -lteec -lpthread  -lc -lresolv

dis-combo:
	llvm-dis < test_combo.bc >test_combo.dis

send-combo:
	scp -i pi $(TEST) $(NUC_PC):~/work/ra-project/evaluation/hook-a64/

send-hikey:
	scp $(TEST) $(hikey):$(hikey_target)

sync-hikey:
	scp -r *.c *.h hooks_a64.S *.ld Makefile lib/ $(hikey):$(hikey_target)

sync-nuc:
	scp -i pi -r *.c *.h hooks_a64.S *.ld Makefile lib/ $(NUC_PC):$(NUC_TARGET)

clean:
	rm *.o
	rm test

obj-orig: test-combo
	llc -march=aarch64 test_combo.bc -o combo.s
	clang combo.s -lc -lpthread -lm -lresolv

dump:
	$(objdump) $(TEST) -D > $(TEST).dump

ph:
	readelf -S  $(TEST)

sh:
	readelf -l  $(TEST)

# Compile Note: cdefs.h not found can be solved by the following cmd
#time CPATH=/usr/include/aarch64-linux-gnu/ make test-combo
