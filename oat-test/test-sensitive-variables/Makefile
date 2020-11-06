NOVA_PATH = ~/work/llvm-env/build/lib/
LLC_ARM = ~/work/llvm-env/build/bin/llc

bc-hello: hello.c
	clang -O0 -emit-llvm hello.c -c -o hello.bc

dag-hello: hello.c
	clang --target=x86_64 -S -emit-llvm hello.c
	llc -mcpu=corei7 -march=x86 -view-sched-dags hello.ll
	#llc -mcpu=corei7 -march=x86 -view-sched-dags hello.ll
	#llc -mcpu=corei7 -march=x86 -view-dag-combine1-dags hello.ll


arm-hello: hello.c
	clang --target=aarch64 -S -emit-llvm hello.c
	opt -load ../../llvm-env/llvm-v1/build/lib/LLVMNova.so -nova < hello.ll > hello_new.ll
	$(LLC_ARM) -mcpu=a57 -march=aarch64  -aarch64-enable-cfv hello_new.ll
	scp hello_new.s ip:~/work/ra-project/data/test/

asm-hello:
	clang --target=aarch64 -S -emit-llvm hello.c
	opt -load $(NOVA_PATH)/LLVMNova.so -nova < hello.ll > hello_nova.ll
	opt -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-ibranch-hints-pass -collect-icall-hints-pass -collect-loop-hints-pass < hello_nova.ll > hello_hints.ll
	$(LLC_ARM) -mcpu=a57 -march=aarch64  -aarch64-enable-cfv hello_hints.ll
	scp hello_hints.s ip:~/tmp/hikey-relay/

bin-hello: hello_hints.s
	clang  -c -g hello_hints.s 
	clang  hello_hints.o -lnova  -o hello

bc-oj: oj.c
	clang -O0 -emit-llvm oj.c -c -o oj.bc

dis-hello: bc-hello
	llvm-dis < hello.bc >hello.dis

dis-oj: bc-oj
	llvm-dis < oj.bc >oj.dis

opt-hello: bc-hello
	opt -load $(NOVA_PATH)/LLVMNova.so -nova < hello.bc > hello_new.bc
	llvm-dis < hello_new.bc >hello_new.dis

opt-hints-hello: bc-hello
	opt -load $(NOVA_PATH)/LLVMCollectCFVHints.so -collect-ibranch-hints-pass < hello.bc > hello_hints.bc
	clang  hello_hints.bc -lnova -o hello_hints 
	llvm-dis < hello_hints.bc >hello_hints.dis

nova-hello:
	clang -O0 -emit-llvm hello.c -c -o hello.bc
	opt -load $(NOVA_PATH)/LLVMNova.so -nova < hello.bc > hello_new.bc
	clang  hello_new.bc -lnova -o hello 

opt-oj: dis-oj
	opt -load $(NOVA_PATH)/LLVMNova.so -nova < oj.bc > /dev/null

clean:
	rm *.bc *.dis

sync:
	cp Makefile hello.c oj.c ~/work/ra-project/data/test/
