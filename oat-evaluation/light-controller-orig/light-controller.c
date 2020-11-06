/**
 * Light controller for controlling remote controllable switches.
 * Copyright (C) 2016  Jussi Judin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <telldus-core.h>
#include <time.h>
//#include "cfv_bellman.h"

typedef enum SwitchDevices {
    DEVICE_NONE = 0,
    DEVICE_MAKUUHUONE_KIRKAS = 1,
    DEVICE_OLOHUONE_KIRKAS = 2,
    DEVICE_MAKUUHUONE_HIMMEA = 3,
    DEVICE_OLOHUONE_HIMMEA = 4,
    DEVICE_KAIKKI_KIRKAS = 5,
    DEVICE_KAIKKI_HIMMEA = 6,
    DEVICE_KAJARIT = 7
} SwitchDevices;

typedef enum MethodReact {
    REACT_NONE = 0,
    REACT_TURNON = 1 << 0,
    REACT_TURNOFF = 1 << 1
} MethodReact;

typedef struct RoutedDevice {
    SwitchDevices targetDevice;
    MethodReact react;
    const char* switchPrefix;
} RoutedDevice;

typedef struct SwitchPattern {
    SwitchDevices targetDevice;
    MethodReact react;
    const char** switchPrefixes;
} SwitchPattern;

typedef struct SwitchMemoryItem {
    uint64_t timestamp;
    MethodReact method;
    const char* switchPrefix;
} SwitchMemoryItem;

#define REACT_BOTH (REACT_TURNON | REACT_TURNOFF)

static const char SWITCH_MAKUUHUONE_KIRKAS[] = "class:command;protocol:arctech;model:codeswitch;house:D;unit:1;";
static const char SWITCH_OLOHUONE_KIRKAS[] = "class:command;protocol:arctech;model:codeswitch;house:D;unit:2;";
static const char SWITCH_MAKUUHUONE_HIMMEA[] = "class:command;protocol:arctech;model:codeswitch;house:D;unit:3;";
static const char SWITCH_OLOHUONE_HIMMEA[] = "class:command;protocol:arctech;model:codeswitch;house:D;unit:4;";

static const char SWITCH_KAJARIT_DUMMY_1[] = "class:command;protocol:arctech;model:codeswitch;house:D;unit:5;";
static const char SWITCH_KAJARIT_DUMMY_2[] = "class:command;protocol:arctech;model:codeswitch;house:D;unit:6;";
static const char SWITCH_KAJARIT_DUMMY_3[] = "class:command;protocol:arctech;model:codeswitch;house:D;unit:7;";
static const char SWITCH_KAJARIT_DUMMY_4[] = "class:command;protocol:arctech;model:codeswitch;house:D;unit:8;";

static const char SWITCH_KAIKKI_KIRKAS[] = "class:command;protocol:arctech;model:selflearning;house:11799578;unit:12;group:0;";
static const char SWITCH_KAIKKI_HIMMEA[] = "class:command;protocol:arctech;model:selflearning;house:11799578;unit:11;group:0;";

const RoutedDevice DEVICE_ROUTINGS[] = {
     // makuuhuone kirkas
    {DEVICE_NONE, REACT_NONE, SWITCH_MAKUUHUONE_KIRKAS},
     // olohuone kirkas
    {DEVICE_NONE, REACT_NONE, SWITCH_OLOHUONE_KIRKAS},
     // makuuhuone himmeä
    {DEVICE_NONE, REACT_NONE, SWITCH_MAKUUHUONE_HIMMEA},
     // olohuone himmeä
    {DEVICE_NONE, REACT_NONE, SWITCH_OLOHUONE_HIMMEA},
    // kaikki kirkas pois -> kaikki himmeä pois
    {DEVICE_KAIKKI_HIMMEA, REACT_TURNOFF, SWITCH_KAIKKI_KIRKAS},
    // kaikki himmeä pois -> kaikki kirkas pois
    {DEVICE_KAIKKI_KIRKAS, REACT_TURNOFF, SWITCH_KAIKKI_HIMMEA},
    // Kytkin OH01
    {DEVICE_OLOHUONE_KIRKAS, REACT_BOTH, "class:command;protocol:arctech;model:selflearning;house:19437866;unit:12;group:0;"},
    {DEVICE_OLOHUONE_HIMMEA, REACT_BOTH, "class:command;protocol:arctech;model:selflearning;house:19437866;unit:11;group:0;"},
    // Kytkin OH02
    {DEVICE_OLOHUONE_KIRKAS, REACT_BOTH, "class:command;protocol:arctech;model:selflearning;house:19413362;unit:12;group:0;"},
    {DEVICE_OLOHUONE_HIMMEA, REACT_BOTH, "class:command;protocol:arctech;model:selflearning;house:19413362;unit:11;group:0;"},
    // Kytkin OH03
    {DEVICE_OLOHUONE_KIRKAS, REACT_BOTH, "class:command;protocol:arctech;model:selflearning;house:21953510;unit:12;group:0;"},
    {DEVICE_OLOHUONE_HIMMEA, REACT_BOTH, "class:command;protocol:arctech;model:selflearning;house:21953510;unit:11;group:0;"},
    // Kytkin MH01
    {DEVICE_MAKUUHUONE_KIRKAS, REACT_BOTH, "class:command;protocol:arctech;model:selflearning;house:20256766;unit:12;group:0;"},
    {DEVICE_MAKUUHUONE_HIMMEA, REACT_BOTH, "class:command;protocol:arctech;model:selflearning;house:20256766;unit:11;group:0;"},
    // Kajarit sammuu myös oven vieressä olevasta "kaikki pois"-painikkeista:
    {DEVICE_KAJARIT, REACT_TURNOFF, SWITCH_KAIKKI_KIRKAS},
    {DEVICE_KAJARIT, REACT_TURNOFF, SWITCH_KAIKKI_HIMMEA},
    // Lisäksi kajareille on oma alueensa tyhmillä Rele ja ratas Oy:n
    // kytkimillä (group II):
    {DEVICE_KAJARIT, REACT_BOTH, SWITCH_KAJARIT_DUMMY_1},
    {DEVICE_KAJARIT, REACT_BOTH, SWITCH_KAJARIT_DUMMY_2},
    {DEVICE_KAJARIT, REACT_BOTH, SWITCH_KAJARIT_DUMMY_3},
    {DEVICE_KAJARIT, REACT_BOTH, SWITCH_KAJARIT_DUMMY_4}
};

const size_t DEVICE_COUNT = sizeof(DEVICE_ROUTINGS) / sizeof(DEVICE_ROUTINGS[0]);

const char* PATTERN_KIRKAS_HIMMEA[] = {
    SWITCH_MAKUUHUONE_KIRKAS,
    SWITCH_OLOHUONE_KIRKAS,
    SWITCH_MAKUUHUONE_HIMMEA,
    SWITCH_OLOHUONE_HIMMEA,
    NULL};

const char* PATTERN_HIMMEA_KIRKAS[] = {
    SWITCH_OLOHUONE_HIMMEA,
    SWITCH_MAKUUHUONE_HIMMEA,
    SWITCH_OLOHUONE_KIRKAS,
    SWITCH_MAKUUHUONE_KIRKAS,
    NULL};

const SwitchPattern SWITCH_PATTERNS[] = {
    {DEVICE_KAJARIT, REACT_TURNOFF, PATTERN_KIRKAS_HIMMEA},
    {DEVICE_KAJARIT, REACT_TURNOFF, PATTERN_HIMMEA_KIRKAS},
    {DEVICE_KAIKKI_KIRKAS, REACT_TURNOFF, PATTERN_KIRKAS_HIMMEA},
    {DEVICE_KAIKKI_KIRKAS, REACT_TURNOFF, PATTERN_HIMMEA_KIRKAS},
    {DEVICE_KAIKKI_HIMMEA, REACT_TURNOFF, PATTERN_KIRKAS_HIMMEA},
    {DEVICE_KAIKKI_HIMMEA, REACT_TURNOFF, PATTERN_HIMMEA_KIRKAS}
};

const size_t PATTERN_COUNT = sizeof(SWITCH_PATTERNS) / sizeof(SWITCH_PATTERNS[0]);

static SwitchMemoryItem g_switch_memory[4] = {
    {0, REACT_NONE, NULL},
    {0, REACT_NONE, NULL},
    {0, REACT_NONE, NULL},
    {0, REACT_NONE, NULL}};

const size_t SWITCH_MEMORY_ITEMS = sizeof(g_switch_memory) / sizeof(g_switch_memory[0]);

const uint64_t PATTERN_TIMEOUT_MS = 4000;

const char METHOD_TURNON[] = "method:turnon;";
const char METHOD_TURNOFF[] = "method:turnoff;";

static void react_to_pattern(uint64_t now)
{
    uint64_t min_timestamp = now - PATTERN_TIMEOUT_MS;

    printf(" %s \n", __func__);

    for (size_t pattern_index = 0; pattern_index < PATTERN_COUNT; pattern_index++) {
        const SwitchPattern* pattern = &SWITCH_PATTERNS[pattern_index];
        bool has_match = true;
        for (size_t switch_memory_index = 0;
             switch_memory_index < SWITCH_MEMORY_ITEMS;
             switch_memory_index++) {
            const SwitchMemoryItem* memory_item = &g_switch_memory[switch_memory_index];
            if (memory_item->timestamp < min_timestamp) {
                has_match = false;
                break;
            }
            if (!(pattern->react & memory_item->method)) {
                has_match = false;
                break;
            }
            if (pattern->switchPrefixes[switch_memory_index] == NULL) {
                break;
            }
            if (pattern->switchPrefixes[switch_memory_index] != memory_item->switchPrefix) {
                has_match = false;
                break;
            }
        }
        if (has_match) {
            if (pattern->react & REACT_TURNON) {
                printf("PATTERN %zu Turn on %d\n", pattern_index, pattern->targetDevice);
                tdTurnOn(pattern->targetDevice);
            } else if (pattern->react & REACT_TURNOFF) {
                printf("PATTERN %zu Turn off %d\n", pattern_index, pattern->targetDevice);
                tdTurnOff(pattern->targetDevice);
            }
        }
    }
}


void listen_to_events(const char *data, int controllerId, int callbackId, void *context __attribute__((unused)))
{
    struct timespec now_ts;
    clock_gettime(CLOCK_MONOTONIC, &now_ts);
    uint64_t now = now_ts.tv_sec * 1000 + now_ts.tv_nsec / 1000000;
    bool __attribute__((annotate("sensitive"))) memory_added_pattern = false;

    printf(" %s \n", __func__);

    for (size_t device_id = 0; device_id < DEVICE_COUNT; device_id++) {
        const RoutedDevice* __attribute__((annotate("sensitive"))) device_routing = &DEVICE_ROUTINGS[device_id];
        const size_t prefix_length = strlen(device_routing->switchPrefix);
        if (strncmp(data, device_routing->switchPrefix, prefix_length) == 0) {
            const char* method_start = data + prefix_length;
            for (size_t i = 1; i < SWITCH_MEMORY_ITEMS ; i++) {
                g_switch_memory[i - 1] = g_switch_memory[i];
            }
            MethodReact __attribute__((annotate("sensitive"))) method = REACT_TURNON;
            if (strcmp(method_start, METHOD_TURNOFF) == 0) {
                method = REACT_TURNOFF;
            }
            SwitchMemoryItem new_item = {now, method, device_routing->switchPrefix};
            g_switch_memory[SWITCH_MEMORY_ITEMS - 1] = new_item;
            memory_added_pattern = true;
        }
        if (device_routing->targetDevice == DEVICE_NONE) {
	    printf(" device none\n");
            continue;
        }
        if (strncmp(data, device_routing->switchPrefix, prefix_length) == 0) {
            const char* __attribute__((annotate("sensitive"))) method_start = data + prefix_length;
            if (strcmp(method_start, METHOD_TURNON) == 0) {
                printf("Turn on %d\n", device_routing->targetDevice);
                if (device_routing->react & REACT_TURNON) {
                    tdTurnOn(device_routing->targetDevice);
                } else {
                    printf("IGNORED\n");
                }
            } else if (strcmp(method_start, METHOD_TURNOFF) == 0) {
                printf("Turn off %d\n", device_routing->targetDevice);
                if (device_routing->react & REACT_TURNOFF) {
                    tdTurnOff(device_routing->targetDevice);
                } else {
                    printf("IGNORED\n");
                }
            } else {
                printf("Unknown method %s\n", data);
            }
        }
    }

    if (!memory_added_pattern) {
        react_to_pattern(now);
    }

    printf("II %d %d %s\n", controllerId, callbackId, data);
}

int main(void)
{
    tdInit();
    int callbackId = tdRegisterRawDeviceEvent(listen_to_events, NULL);
    printf("%d %d\n", callbackId, tdGetNumberOfDevices());

    unsigned long start, end;
    int count = 0;
    char data[] = "class:command;protocol:arctech;model:selflearning;house:11799578;unit:12;group:0;method:turnon;";

    // static const char SWITCH_MAKUUHUONE_KIRKAS[] = "class:command;protocol:arctech;model:codeswitch;house:D;unit:1;";
    // static const char SWITCH_KAIKKI_KIRKAS[] = "class:command;protocol:arctech;model:selflearning;house:11799578;unit:12;group:0;";
    // const char METHOD_TURNON[] = "method:turnon;";
    // const char METHOD_TURNOFF[] = "method:turnoff;";
    
    start = usecs();
    //cfv_init(1024);
    for (count = 0 ; count < 10; count++)
        listen_to_events(data,0,0,NULL);
   // while (count++ < 100) {
   //    // struct timespec wait_time = {0, 0.05};
   //    // nanosleep(&wait_time, NULL);
   // 
   // }

    //cfv_quote();
    end = usecs();
    printf("round with attestation time usecs: %lu\n", end - start);

    tdClose();
    return 0;
}
