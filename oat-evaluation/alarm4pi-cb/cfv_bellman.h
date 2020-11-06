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

#include <stdint.h>

/* Normal world API */

/*!
 * \brief cfv_init
 * Initialize the CFA subsystem.
 * \param max_ecount max event count for event buffer
 */
uint32_t cfv_init(uint32_t max_ecount);

/*!
 * \brief cfv_quote
 * Quote the current digest value.
 */
uint32_t cfv_quote(void);

#endif /* CFV_BELLMAN_H*/

