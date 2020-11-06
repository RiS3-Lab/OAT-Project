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
#ifndef CFV_BELLMAN_H_
#define CFV_BELLMAN_H_

#include <stddef.h>
#include <stdint.h>

#include "cfv_common.h"

/* Normal world API */

/*!
 * \brief cfv_init
 * Initialize the CFA subsystem.
 * \param main_start Start address of main function
 * \param main_end End address of main function
 */
uint32_t cfv_init(uint32_t max_ecount);

uint32_t handle_event(uint64_t event_type, uint64_t a, uint64_t b);
void commit_events(void);

/*!
 * \brief cfv_quote
 * Quote the current digest value.
 * \param out Output buffer for storing the digest quote
 * \param outlen Length of the output buffer for storing the digest quote
 */
uint32_t cfv_quote(void);

#endif /* CFV_BELLMAN_H*/

