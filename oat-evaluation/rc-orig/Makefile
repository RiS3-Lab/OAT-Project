# I guess that requiring people to instal Scons is silly, so I'll add this
# Makefile.

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

LDLIBS := -L../../data/runtime -lm -ljansson -lnova -lteec

#pi_pcm: pi_pcm.o mailbox.o
pi_pcm: 
	clang $(CFLAGS) $(LDLIBS) pi_pcm.c -o server

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

