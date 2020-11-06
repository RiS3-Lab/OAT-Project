#include "cfv_bellman.h"
#include "nova.h"
void test_world_switch(); 
void enable_pmc();
unsigned long usecs();
unsigned int readticks();

int main() {
	int i, j;
	int *p;
	unsigned long start, end;
	unsigned cc_start;
	unsigned cc_end;

	enable_pmc();

 	start = usecs();
	cc_start = readticks();
	cfv_init();
 	cc_end = readticks();
	end = usecs();
	printf("cfv_init cycles : %u\n", cc_end - cc_start);
	printf("cfv_init time: %lu\n", end - start);

 	start = usecs();
	cc_start = readticks();
	test_world_switch();
 	cc_end = readticks();
	end = usecs();
	printf("world switch cycles : %u\n", cc_end - cc_start);
	printf("world switch time: %lu\n", end - start);
	
	/* test control event handling */
 	start = usecs();
	cc_start = readticks();
	for (i = 0; i < 1000; i++) {
		cfv_ret(0x400100, 0x400110);
	}
 	cc_end = readticks();
	end = usecs();
	printf("1000 ret cycles : %u\n", cc_end - cc_start);
	printf("1000 ret time: %lu\n", end - start);

 	start = usecs();
	cc_start = readticks();
	for (i = 0; i < 500; i++) {
        __collect_cond_branch_hints(true);
        __collect_cond_branch_hints(false);
	}
 	cc_end = readticks();
	end = usecs();
	printf("1000 cond branch cycles : %u\n", cc_end - cc_start);
	printf("1000 cond branch time: %lu\n", end - start);

 	start = usecs();
	cc_start = readticks();
	for (i = 0; i < 100; i++) {
		cfv_icall(0x400100, 0x400110);
		cfv_ijmp(0x400100, 0x400110);
	}
 	cc_end = readticks();
	end = usecs();
	printf("200 icall/ijmp cycles : %u\n", cc_end - cc_start);
	printf("200 icall/ijmp time: %lu\n", end - start);

 	start = usecs();
	cc_start = readticks();
	/* test data event handling  */
	for (i = 0; i < 200; i++) {
		__record_defevt(0x400100 + i, 3);
		__check_useevt(0x400100 + i, 3);
	}
 	cc_end = readticks();
	end = usecs();
	printf("400 def/use cycles : %u\n", cc_end - cc_start);
	printf("400 def/use time: %lu\n", end - start);
	
 	start = usecs();
	cc_start = readticks();
  	printf("hello world\n");
  	printf("hello world\n");
  	printf("hello world\n");
  	printf("hello world\n");
  	printf("hello world\n");
 	cc_end = readticks();
	end = usecs();
	printf("printf 5 hello world cycles : %u\n", cc_end - cc_start);
	printf("printf 5 hello world time: %lu\n", end - start);

	p = &j ;
 	start = usecs();
	cc_start = readticks();
	for (i = 0; i < 1000;i++)
		*p = i;
 	cc_end = readticks();
	end = usecs();
	printf("sum(1 -- 1000): cycles : %u\n", cc_end - cc_start);
	printf("sum(1 -- 1000): time : %lu\n", end - start);
	
 	start = usecs();
	cc_start = readticks();
	cfv_quote();
 	cc_end = readticks();
	end = usecs();
	printf("cfv_quote cycles : %u\n", cc_end - cc_start);
	printf("cfv_quote time: %lu\n", end - start);

	return 0;
}
