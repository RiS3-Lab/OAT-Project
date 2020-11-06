#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include <include/cfa.h>
#include <include/blake2.h>

uint32_t cfa_init(cfa_ctx_t *ctx) {
    int i;

    ctx->p = 10000001; /* some prime number near 2^64 */
    ctx->a = 7; /* a should be a prime root of p */
    ctx->aa = 7; /* a should be a prime root of p */
    ctx->HASH = 0; /* initial hash value H0 */
    ctx->p32 = 10000001; /* some prime number near 2^64 */
    ctx->a32 = 7; /* a should be a prime root of p */
    ctx->aa32 = 7; /* a should be a prime root of p */
    ctx->HASH32 = 0; /* initial hash value H0 */
    ctx->sec_data_hashmap.p = HASHMAP_SIZE; /* .p should be a prime number*/

    blake2s_init(&(ctx->S), BLAKE2S_OUTBYTES);

    /* initialize hashmap */
    for (i = 0; i < HASHMAP_SIZE; i++)
       ctx->sec_data_hashmap.bucket[i] = NULL;

    /* initialize conditional branch condition buffer */
    ctx->cond_buf = TEE_Malloc(MAX_COND_EVENTS*sizeof(char), TEE_MALLOC_FILL_ZERO);
    ctx->cond_buf_idx = 0;

    /* initialize indirect branch address buffer */
    ctx->iaddr_buf = TEE_Malloc(MAX_IBRANCH_EVENTS*sizeof(uint64_t), TEE_MALLOC_FILL_ZERO);
    ctx->iaddr_buf_idx = 0;

    ctx->initialized = true;

    return 0;
}

uint32_t cfa_quote(cfa_ctx_t *ctx) {
    /* TODO report the final hash value */
    blake2s_final(&(ctx->S), ctx->digest, BLAKE2S_OUTBYTES);
    ctx->initialized = false;


    return 0;
}

node_t* hashmap_lookup(hashmap_t *hmap, uint64_t key) {
    node_t *ptr;

	DMSG("hash lookup key : 0x%llx\n", key);

    ptr = hmap->bucket[key % hmap->p];

    while (ptr != NULL) {
        if (ptr->key != key)
            ptr = ptr->next;
        else
            break;
    }

	DMSG("hash lookup key : 0x%llx, %s\n", key, ptr == NULL ? "Failed":"Succeed");

    return ptr;
}

void hashmap_update(hashmap_t *hmap, uint64_t key, uint64_t value) {
    node_t *ptr, *new_node;

	DMSG("hash updata key : 0x%llx, value: 0x%llx\n", key, value);

    new_node = hashmap_lookup(hmap, key);
    if(new_node != NULL) { /* modify existing node */
        new_node->value = value;
    } else { /* insert new node */
        new_node = TEE_Malloc(sizeof(node_t), TEE_MALLOC_FILL_ZERO);
        new_node->key = key;
        new_node->value = value;

        ptr = hmap->bucket[key % hmap->p];
        new_node->next = ptr;
        ptr = new_node;
    }

    return;
}
