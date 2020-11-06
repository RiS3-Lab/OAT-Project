TEE_PATH ?= /home/yourusername/work/tee-workspace/devel/optee-hikey-debian/
TOOLCHAIN ?= $(TEE_PATH)/toolchains/aarch64/bin/
TA ?= $(TEE_PATH)/hello_world/ta
TEEC_EXPORT ?= $(TEE_PATH)/optee_client/out/export
CFLAGS += -Wall -I$(TA)/include -I$(TEEC_EXPORT)/include -I./include
LDADD += -lteec -L$(TEEC_EXPORT)/lib
CC = clang
TARGET = -target aarch64-unknown-linux
CCC = $(TOOLCHAIN)/aarch64-linux-gnu-gcc
SOFT_CFLAGS=-Wall -pedantic  -O3 -D__SOFTBOUNDCETS_SPATIAL
ARFLAGS=-rcs 
 
nova:
	#$(CC) $(TARGET) -fPIC -g -c -Wall nova.c -o nova.o
	$(CCC) -fPIC -g -c -Wall nova.c -o nova.o

cfv-bellman:
	#$(CC) $(TARGET) -fPIC -g -c $(CFLAGS) -Wall cfv_bellman.c -o cfv_bellman.o
	$(CCC) -fPIC -g -c $(CFLAGS) -Wall cfv_bellman.c -o cfv_bellman.o

trampoline:
	#$(CC) $(TARGET) -fPIC -g -c -Wall trampoline.S -o trampoline.o
	$(CCC) -fPIC -g -c -Wall trampoline.S -o trampoline.o

lib-a64: nova cfv-bellman trampoline
	$(CCC) $(LDADD) -shared -Wl,-soname,libnova.so.1 -o libnova.so.1.0.1 nova.o trampoline.o cfv_bellman.o -lc

lib-aarch64:
	clang  -fPIC -g -c -Wall nova.c
	clang  -fPIC -g -c -Wall cfv_bellman.c
	clang  -fPIC -g -c -Wall trampoline.S
	clang -shared -Wl,-soname,libnova.so.1 -o libnova.so.1.0.1 nova.o trampoline.o cfv_bellman.o -lc
	sudo cp libnova.so.1.0.1 /usr/lib/
	sudo ln -fs libnova.so.1.0.1 /usr/lib/libnova.so.1
	sudo ln -fs /usr/lib/libnova.so.1 /usr/lib/libnova.so

softboundcets_rt: softboundcets.h softboundcets-checks.c softboundcets.c softboundcets-wrappers.c
	clang $(SOFT_CFLAGS) -c softboundcets-checks.c -o softboundcets-checks.o
	clang $(SOFT_CFLAGS) -c softboundcets.c -o softboundcets.o
	clang $(SOFT_CFLAGS) -c softboundcets-wrappers.c -o softboundcets-wrappers.o
	ar $(ARFLAGS) libsoftboundcets_rt.a softboundcets.o softboundcets-checks.o softboundcets-wrappers.o

lib-x64:
	clang  -fPIC -g -c -Wall nova.c
	clang -shared -Wl,-soname,libnova.so.1 -o libnova.so.1.0.1 nova.o -lc
	sudo cp libnova.so.1.0.1 /usr/lib/
	sudo ln -fs libnova.so.1.0.1 /usr/lib/libnova.so.1
	sudo ln -fs /usr/lib/libnova.so.1 /usr/lib/libnova.so

sync:
	cp Makefile ~/work/ra-project/data/runtime/
	cp nova.c ~/work/ra-project/data/runtime/
	cp trampoline.S ~/work/ra-project/data/runtime/

send:
	scp libnova.so.1.0.1  your-pc:~/tmp/hikey-relay/

bin-microbench:
	clang microbench.c -lnova -lteec -o microbench
