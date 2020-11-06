/*
 * Copyright (c) 2016, Linaro Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define STR_TRACE_USER_TA "HELLO_WORLD"

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include <string.h>
#include <assert.h>
#include "hello_world_ta.h"
#include "cfa.h"

/* store buffer in encrypted file */
const char blob_cond_fname[] = "blob.cond.teedata.date";
const char blob_iaddr_fname[] = "blob.iaddr.teedata.date";
const char blob_rethash_fname[] = "blob.rethash.teedata.date";

/*
 * Called when the instance of the TA is created. This is the first call in
 * the TA.
 */
TEE_Result TA_CreateEntryPoint(void)
{
	DMSG("has been called");
	return TEE_SUCCESS;
}

/*
 * Called when the instance of the TA is destroyed if the TA has not
 * crashed or panicked. This is the last call in the TA.
 */
void TA_DestroyEntryPoint(void)
{
	DMSG("has been called");
}

/*
 * Called when a new session is opened to the TA. *sess_ctx can be updated
 * with a value to be able to identify this session in subsequent calls to the
 * TA. In this function you will normally do the global initialization for the
 * TA.
 */
TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
		TEE_Param __maybe_unused params[4],
		void __maybe_unused **sess_ctx)
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	/* Unused parameters */
	(void)&params;
	(void)&sess_ctx;

	/*
	 * The DMSG() macro is non-standard, TEE Internal API doesn't
	 * specify any means to logging from a TA.
	 */
	DMSG("Hello World!\n");

	/* If return value != TEE_SUCCESS the session will not be created. */
	return TEE_SUCCESS;
}

/*
 * Called when a session is closed, sess_ctx hold the value that was
 * assigned by TA_OpenSessionEntryPoint().
 */
void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx)
{
	(void)&sess_ctx; /* Unused parameter */
	DMSG("Goodbye!\n");
}

cfa_ctx_t cfa_ctx;
bool cfa_start = false;
bool cfa_inited = false;


static inline uint32_t tee_time_to_ms(TEE_Time t)
{
	return t.seconds * 1000 + t.millis;
}

static inline uint32_t get_delta_time_in_ms(TEE_Time start, TEE_Time stop)
{
	return tee_time_to_ms(stop) - tee_time_to_ms(start);
}


static TEE_Result cfa_init_wrapper(uint32_t param_types,
	TEE_Param params[4])
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(
						   TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	DMSG("has been called");
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	cfa_init(&cfa_ctx);

	return TEE_SUCCESS;
}

static void data_event(cfa_ctx_t *ctx, cfa_event_t *evt) {
    node_t *ptr;
    if (evt->etype == CFV_EVENT_DATA_DEF) {
        hashmap_update(&(ctx->sec_data_hashmap), evt->a, evt->b);
    } else if (evt->etype == CFV_EVENT_DATA_USE) {
        ptr = hashmap_lookup(&(ctx->sec_data_hashmap), evt->a);
        if(ptr == NULL || ptr->value != evt->b) {
            // record and update
            hashmap_update(&(ctx->sec_data_hashmap), evt->a, evt->b);

            /* use check fail! */
            if (ptr == NULL)
	            DMSG("def-use check fail at addr: 0x%llx, value: 0x%llx no define record\n", evt->a, evt->b);
            else
	            DMSG("def-use check fail at addr: 0x%llx, value: 0x%llx value not match with recorded 0x%llx\n", evt->a, evt->b, ptr->value);
        }
    }
}

static void control_event(cfa_ctx_t *ctx, cfa_event_t *evt) {
    blake2s_update(&(ctx->S), ((uint8_t *)evt) + 8, 16); // skip evt->etype, which occupy 8 bytes 
}

/* check_file_exists & prepare_file borrowed from c-flat */
bool check_file_exists(const char *filename);
bool check_file_exists(const char *filename)
{
	bool exists = false;
	TEE_Result res = TEE_SUCCESS;
	TEE_ObjectHandle object;

	res = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE,
			(void *)filename, strlen(filename),
			TEE_DATA_FLAG_ACCESS_READ |
			TEE_DATA_FLAG_ACCESS_WRITE |
			TEE_DATA_FLAG_ACCESS_WRITE_META |
			TEE_DATA_FLAG_OVERWRITE,
			&object);
	if (res != TEE_SUCCESS) {
		EMSG("Failed to open persistent object, res=0x%08x",
				res);
		exists = false;
		goto exit;
	} else {
		exists = true;
		goto exit_close_object;
	}

exit_close_object:
	TEE_CloseObject(object);
exit:
	return exists;
}

static TEE_Result prepare_file(const char *filename, uint8_t *chunk_buf,
				size_t chunk_size);
static TEE_Result prepare_file(const char *filename, uint8_t *chunk_buf,
				size_t chunk_size)
{
	TEE_Result res = TEE_SUCCESS;
	TEE_ObjectHandle object;

	res = TEE_CreatePersistentObject(TEE_STORAGE_PRIVATE,
			(void *)filename, strlen(filename),
			TEE_DATA_FLAG_ACCESS_READ |
			TEE_DATA_FLAG_ACCESS_WRITE | 
			TEE_DATA_FLAG_ACCESS_WRITE_META |
			TEE_DATA_FLAG_OVERWRITE,
			NULL, NULL, 0, &object);
	if (res != TEE_SUCCESS) {
		EMSG("Failed to create persistent object, res=0x%08x",
				res);
		goto exit;
	}

      res = TEE_WriteObjectData(object, chunk_buf, chunk_size);
      if (res != TEE_SUCCESS) {
      	EMSG("Failed to write data, res=0x%08x", res);
      	goto exit_close_object;
      }

exit_close_object:
	TEE_CloseObject(object);
exit:
	return res;
}

static TEE_Result append_file(const char *filename, uint8_t *chunk_buf,
				size_t chunk_size);
static TEE_Result append_file(const char *filename, uint8_t *chunk_buf,
				size_t chunk_size)
{
	TEE_Result res = TEE_SUCCESS;
	TEE_ObjectHandle object;

	res = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE,
			(void *)filename, strlen(filename),
			TEE_DATA_FLAG_ACCESS_READ |
			TEE_DATA_FLAG_ACCESS_WRITE |
			TEE_DATA_FLAG_ACCESS_WRITE_META |
			TEE_DATA_FLAG_OVERWRITE,
			&object);
	if (res != TEE_SUCCESS) {
		EMSG("Failed to open persistent object, res=0x%08x",
				res);
		goto exit;
	}

    res = TEE_SeekObjectData(object, 0, TEE_DATA_SEEK_END);
    if (res != TEE_SUCCESS) {
    	EMSG("Failed to seek data, res=0x%08x", res);
    	goto exit_close_object;
    }

    res = TEE_WriteObjectData(object, chunk_buf, chunk_size);
    if (res != TEE_SUCCESS) {
    	EMSG("Failed to write data, res=0x%08x", res);
    	goto exit_close_object;
    }

exit_close_object:
	TEE_CloseObject(object);
exit:
	return res;
}


// save data
static TEE_Result save_data(const char *fname, uint8_t *buf, size_t size) {
    bool exists;
    TEE_Result res;

    exists = check_file_exists(fname);
    if (exists == false) {
        res = prepare_file(fname, buf, size);
    } else {
        res = append_file(fname, buf, size);
    }

    return res;
}

static void trace_addr_event(cfa_ctx_t *ctx, cfa_event_t *evt) {
    if (ctx->iaddr_buf_idx == MAX_IBRANCH_EVENTS) {
        // buffer full, store it.
        save_data(blob_iaddr_fname, ctx->iaddr_buf, MAX_IBRANCH_EVENTS*sizeof(uint64_t));
        ctx->iaddr_buf_idx = 0;
    }

    ctx->iaddr_buf[ctx->iaddr_buf_idx++] = evt->b;//record return inst target address
}

static void trace_cond_event(cfa_ctx_t *ctx, cfa_event_t *evt) {
    if (ctx->cond_buf_idx == MAX_COND_EVENTS) {
        // buffer full, store it.
        save_data(blob_cond_fname, ctx->cond_buf, MAX_COND_EVENTS*sizeof(char));
        ctx->cond_buf_idx = 0;
    }
    //record condition taken / not takeninfo, we use 'y' or 'n', which takes one-byte
    // it should be further optimized to use only one bit.
    ctx->cond_buf[ctx->cond_buf_idx++] = 'y' ? evt->a == 1 : 'n';
}

static void handle_event(cfa_ctx_t *ctx, cfa_event_t *evt) {
    if (evt->etype == CFV_EVENT_CTRL)
        control_event(ctx, evt);
    else (evt->etype == CFV_EVENT_HINT_ICALL || evt-etype == CFV_EVENT_HINT_IBR)
        trace_addr_event(ctx, evt);
    else (evt->etype == CFV_EVENT_CONDBR)
        trace_cond_event(ctx, evt);
    else
        data_event(ctx, evt);
}

static TEE_Result verify(uint32_t param_types,
	TEE_Param params[4])
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INOUT,
						   TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_VALUE_INOUT);

	cfa_event_t evt;

	DMSG("has been called");
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

    evt.etype = params[0].a;
    evt.a = params[1].b;
    evt.a = (evt.a << 32) |params[1].a;
    evt.b = params[2].b;
    evt.b = (evt.b << 32) |params[2].a;

	/* cfa event dispatcher */
	handle_event(&cfa_ctx, &evt);

	return TEE_SUCCESS;
}

static TEE_Result cfa_quote_wrapper(uint32_t param_types,
	TEE_Param params[4])
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	DMSG("has been called");
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	/* Unused parameters */
	(void)&params;

	cfa_quote(&cfa_ctx);

    
    // in real system, we should sign them and send the blob and the signature out to verifier
    // for our prototype, we just save them as secure objects.
    save_data(blob_iaddr_fname, cfa_ctx.iaddr_buf, cfa_ctx.iaddr_buf_idx*sizeof(uint64_t));
    save_data(blob_cond_fname, cfa_ctx.cond_buf, cfa_ctx.cond_buf_idx*sizeof(char));
    save_data(blob_rethash_fname, cfa_ctx.digest, BLAKE2S_OUTBYTES);

	return TEE_SUCCESS;

}

static TEE_Result inc_value(uint32_t param_types,
	TEE_Param params[4])
{
	uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE,
						   TEE_PARAM_TYPE_NONE);

	DMSG("has been called");
	if (param_types != exp_param_types)
		return TEE_ERROR_BAD_PARAMETERS;

	DMSG("Got value: %u from NW", params[0].value.a);
	params[0].value.a++;
	DMSG("Increase value to: %u", params[0].value.a);
	return TEE_SUCCESS;
}

/*
 * Called when a TA is invoked. sess_ctx hold that value that was
 * assigned by TA_OpenSessionEntryPoint(). The rest of the paramters
 * comes from normal world.
 */
TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
			uint32_t cmd_id,
			uint32_t param_types, TEE_Param params[4])
{
	(void)&sess_ctx; /* Unused parameter */

	switch (cmd_id) {
	case TA_HELLO_WORLD_CMD_INC_VALUE:
		return inc_value(param_types, params);
	case TA_CMD_CFA_VERIFY_EVENTS:
		return verify(param_types, params);
	case TA_CMD_CFA_INIT:
		return cfa_init_wrapper(param_types, params);
	case TA_CMD_CFA_QUOTE:
		return cfa_quote_wrapper(param_types, params);
	default:
		return TEE_ERROR_BAD_PARAMETERS;
	}
}
