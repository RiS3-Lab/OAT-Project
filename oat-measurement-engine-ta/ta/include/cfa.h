#ifndef CFA_H
#define CFA_H

#include <stdint.h>
#include <stdbool.h>
#include "blake2.h"

/* CFV event types */
#define CFV_EVENT_CTRL		0x00000010
#define CFV_EVENT_DATA_DEF	0x00000020
#define CFV_EVENT_DATA_USE	0x00000040
#define CFV_EVENT_HINT_CONDBR	0x00000080
#define CFV_EVENT_HINT_ICALL	0x00000100
#define CFV_EVENT_HINT_IBR  	0x00000200

/* max trace events */
#define MAX_COND_EVENTS 10*1000 // it depends on how much memory is available for recording trace
#define MAX_IBRANCH_EVENTS 1000 // it depends on how much memory is available for recording trace

typedef struct cfa_event {
	uint64_t etype;
    uint64_t a;
    uint64_t b;
} cfa_event_t;

typedef struct node {
    uint64_t key;
    uint64_t value;
    struct node *next;
} node_t;

/* this size should be at least as large as the prime number p */
#define HASHMAP_SIZE  0x1001
typedef struct hashmap {
    node_t *bucket[HASHMAP_SIZE];
    uint64_t p;
} hashmap_t;

/* Context for CFA operations */
typedef struct cfa_ctx {
    uint64_t p;
    uint64_t a;
    uint64_t aa;
    uint64_t HASH;
    uint32_t p32;
    uint32_t a32;
    uint32_t aa32;
    uint32_t HASH32;
    blake2s_state S;
    uint8_t digest[BLAKE2S_BLOCKBYTES];

    /* trace cond buffer */
    char *cond_buf;
    uint32_t cond_buf_idx;

    /* trace indirect branch address buffer */
    uint64_t *iaddr_buf;
    uint32_t iaddr_buf_idx;


    hashmap_t sec_data_hashmap;
	bool initialized;
} cfa_ctx_t;

uint32_t cfa_init(cfa_ctx_t *ctx);
uint32_t cfa_quote(cfa_ctx_t *ctx);

/*!
 * \brief hashmap_loopup
 * Look up a data_use event in the current hashmap;
 */
node_t* hashmap_lookup(hashmap_t *hmap, uint64_t key);

/*!
 * \brief hashmap_update
 * insert/modify a data_def event in the current hashmap;
 */
void hashmap_update(hashmap_t *hmap, uint64_t key, uint64_t value); 

#endif /* CFA_H */
