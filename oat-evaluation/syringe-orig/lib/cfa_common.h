/*
 * Copyright (c) 2016 Aalto University
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef CFA_COMMON_H
#define CFA_COMMON_H

/* Length of the computed digests */
#define DIGEST_SIZE_BYTES       16

/* Length of instruction in bytes */
#define INSTRUCTION_LEN		4

/* CFA event types */
#define CFA_EVENT_INIT		0x00000000
#define CFA_EVENT_B		0x00000001
#define CFA_EVENT_BR_X1		0x00000002
#define CFA_EVENT_BR_X2		0x00000004
#define CFA_EVENT_BR_X17	0x00000008
#define CFA_EVENT_BL		0x00000010
#define CFA_EVENT_BLR_X1	0x00000020
#define CFA_EVENT_BLR_X3	0x00000040
#define CFA_EVENT_RET		0x00000080
#define CFA_EVENT_QUOTE		0x80000000
#define CFA_EVENT_ERROR		0x000000FF

/* Internal CFA return values */
#define CFA_SUCCESS              0x00000000
#define CFA_ERROR_GENERIC        0xFFFF0000
#define CFA_ERROR_BAD_PARAMETERS 0xFFFF0006
#define CFA_ERROR_OUT_OF_MEMORY  0xFFFF000C

#ifndef ASSEMBLY

#include <stdint.h>

/* Type alias for memory address */
typedef uint32_t cfa_addr_t;

#endif

#endif /* CFA_COMMON_H */
