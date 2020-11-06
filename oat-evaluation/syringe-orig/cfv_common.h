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
#ifndef CFV_COMMON_H
#define CFV_COMMON_H

/* Length of the computed digests */
#define DIGEST_SIZE_BYTES       16

/* Length of instruction in bytes */
#define INSTRUCTION_LEN		4

/* CFV event types */
#define CFV_EVENT_INIT		0x00000000
#define CFV_EVENT_CTRL		0x00000010
#define CFV_EVENT_DATA_DEF	0x00000020
#define CFV_EVENT_DATA_USE	0x00000040
#define CFV_EVENT_QUOTE		0x80000000
#define CFV_EVENT_ERROR		0x000000FF

/* Internal CFV return values */
#define CFV_SUCCESS              0x00000000
#define CFV_ERROR_GENERIC        0xFFFF0000
#define CFV_ERROR_BAD_PARAMETERS 0xFFFF0006
#define CFV_ERROR_OUT_OF_MEMORY  0xFFFF000C

#ifndef ASSEMBLY

#include <stdint.h>

/* Type alias for memory address */
typedef uint32_t cfv_addr_t;

#endif

#endif /* CFV_COMMON_H */
