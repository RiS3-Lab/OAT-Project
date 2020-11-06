#ifndef CFV_BELLMAN_H_
#define CFV_BELLMAN_H_

#include <stddef.h>
#include <stdint.h>

/* CFV event types */
#define CFV_EVENT_CTRL		0x00000010
#define CFV_EVENT_DATA_DEF	0x00000020
#define CFV_EVENT_DATA_USE	0x00000040
#define CFV_EVENT_HINT_CONDBR	0x00000080
#define CFV_EVENT_HINT_ICALL	0x00000100
#define CFV_EVENT_HINT_IBR  	0x00000200

/* Normal world API */

uint32_t cfv_init(void);
uint32_t cfv_quote(void);
uint32_t handle_event(uint64_t event_type, uint64_t a, uint64_t b);


#endif /* CFV_BELLMAN_H*/
