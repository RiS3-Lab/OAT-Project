#include "cfv_bellman.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <fcntl.h>

#include <sys/time.h>
#include <sys/mman.h>
#include <time.h>

/* OP-TEE TEE client API (built by optee_client) */
#include "tee_client_api.h"

/* To the the UUID (found the the TA's h-file(s)) */
#include "hello_world_ta.h"

/* Normal world API */
TEEC_Context ctx;
TEEC_Session sess;
TEEC_Operation op;

bool cfv_start = false;

/* hints related */
char hints_file[64] = "hints.txt";
FILE *hfp = NULL;

/**
 * for control event:(noly return event)
 *    a = src
 *    b = dest
 * for data event
 *    a = addr
 *    b = value 
 * for icall hint
 *    a = src
 *    b = target
 * for ijmp hint
 *    a = src
 *    b = target
 * for cond branch hint
 *    a = true/false
 *
 */
//typedef struct cfv_event {
//	uint64_t etype;
//	uint64_t a;
//	uint64_t b;
//} cfv_event_t;

void errx(const char *msg, TEEC_Result res);
void errx(const char *msg, TEEC_Result res)
{
	fprintf(stderr, "%s: 0x%08x", msg, res);
	exit (1);
}

void check_res(TEEC_Result res, const char *errmsg);
void check_res(TEEC_Result res, const char *errmsg)
{
	if (res != TEEC_SUCCESS)
		errx(errmsg, res);
}

void open_ta(void);
void open_ta(void)
{
	TEEC_Result res;
	TEEC_UUID uuid = TA_HELLO_WORLD_UUID;
	uint32_t err_origin;

	res = TEEC_InitializeContext(NULL, &ctx);
	check_res(res,"TEEC_InitializeContext");

	res = TEEC_OpenSession(&ctx, &sess, &uuid, TEEC_LOGIN_PUBLIC, NULL,
			       NULL, &err_origin);
	check_res(res,"TEEC_OpenSession");
}

void close_ta(void);
void close_ta(void) {
	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);
}

unsigned long usecs() {
        struct timeval start;
        gettimeofday(&start, NULL);
        return start.tv_sec * 1000 * 1000 + start.tv_usec;
}


unsigned long start_glob;

uint32_t cfv_init()
{
	TEEC_Result res;
	uint32_t ret_origin;

	unsigned long start, end;
	start = usecs();
	start_glob = start;

	open_ta();

	end = usecs();

	printf("open_ta time: %lu\n", end - start);

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE);

	start = usecs();
	printf("memset op time: %lu\n", start - end);

	/* open hints file */
	hfp = fopen (hints_file, "w+");

	/* indicate cfv_start now! TODO: sensitive value? */
	cfv_start = true;

	res = TEEC_InvokeCommand(&sess, TA_CMD_CFA_INIT, &op,
				 &ret_origin);
	check_res(res, "TEEC_InvokeCommand");
	end = usecs();
	printf("invoke cmd time: %lu\n",  end - start);

	printf("cfv_init time: %lu\n", usecs() - start_glob);
	return 0;
}

uint32_t cfv_quote()
{
	TEEC_Result res;
	uint32_t ret_origin;

	unsigned long start = usecs();

	memset(&op, 0, sizeof(op));

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT,
					 TEEC_VALUE_INOUT,
					 TEEC_NONE,
					 TEEC_NONE);

	res = TEEC_InvokeCommand(&sess, TA_CMD_CFA_QUOTE, &op,
				 &ret_origin);
	check_res(res, "TEEC_InvokeCommand");

    printf("hint count:%u, data event count:%u, ctrl event count:%u, total event count:%u\n", op.params[0].value.a,
             op.params[0].value.b, op.params[1].value.a, op.params[1].value.b);

	cfv_start = false;

	close_ta();

	printf("cfv_quote time: %lu\n", usecs() - start);

	printf("time during attestation: %lu\n", usecs() - start_glob);	

	/* close hints file */
   	fclose(hfp);

	return 0;
}


/**
 * TODO: we should implement handle_event in assembly code
 * to prevent leak sensitive info.
 */
uint32_t handle_event(uint64_t etype, uint64_t a, uint64_t b) {
	TEEC_Result res;
	uint32_t ret_origin;

	/* ta is opened by cfv_init, before that we skip events */
	if (cfv_start == false)
		return 0;
	memset(&op, 0, sizeof(op));

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT,
					 TEEC_VALUE_INOUT,
					 TEEC_VALUE_INOUT,
					 TEEC_NONE);
    op.params[0].value.a = etype;
    op.params[1].value.a = a & 0xffffffff;
    op.params[1].value.b = (a>>32) & 0xffffffff;
    op.params[2].value.a = b & 0xffffffff;
    op.params[2].value.b = (b>>32) & 0xffffffff;


	res = TEEC_InvokeCommand(&sess, TA_CMD_CFA_VERIFY_EVENTS, &op,
				 &ret_origin);
	check_res(res, "TEEC_InvokeCommand");

	return 0;
}

void enable_pmc() {
	// program the performance-counter control-register:
	asm volatile("msr pmcr_el0, %0" : : "r" (17));
	//enable all counters
	asm volatile("msr PMCNTENSET_EL0, %0" : : "r" (0x8000000f));
	//clear the overflow 
	asm volatile("msr PMOVSCLR_EL0, %0" : : "r" (0x8000000f));
}

unsigned int readticks()
{
	unsigned int cc;
	//read the coutner value
	asm volatile("mrs %0, PMCCNTR_EL0" : "=r" (cc));
	return cc;
}

void test_world_switch() {
	TEEC_Result res;
	uint32_t ret_origin;
	unsigned long start, end;
	unsigned cc_start, cc_end;

	//enable_pmc();

	memset(&op, 0, sizeof(op));

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE,
					 TEEC_NONE);

 	start = usecs();
	cc_start = readticks();
	res = TEEC_InvokeCommand(&sess, TA_CMD_CFA_QUOTE + 5, &op,
				 &ret_origin);
	res = TEEC_InvokeCommand(&sess, TA_CMD_CFA_QUOTE + 5, &op,
				 &ret_origin);
	res = TEEC_InvokeCommand(&sess, TA_CMD_CFA_QUOTE + 5, &op,
				 &ret_origin);
	res = TEEC_InvokeCommand(&sess, TA_CMD_CFA_QUOTE + 5, &op,
				 &ret_origin);
	res = TEEC_InvokeCommand(&sess, TA_CMD_CFA_QUOTE + 5, &op,
				 &ret_origin);
 	cc_end = readticks();
	end = usecs();
	printf("5 world switch cycles : %u\n", cc_end - cc_start);
	printf("5 world switch time: %lu\n", end - start);

	return;
}
