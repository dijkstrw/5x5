/*
 * Copyright (c) 2015-2016 by Willem Dijkstra <wpd@xs4all.nl>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the auhor nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * A keymap consists of <layers> * <rows> * <colums> of scanmap_t.
 *
 *
 * |87654321|87654321|87654321|87654321|
 * |--------+--------+--------+--------|
 * |type    |data1   |data2   |data3   |
 * |    0000|        |keymod  |scancode|
 * |    0001|        |extra key        |
 * |    0010|buttons |x       |y       |
 * |    0011|buttons |h       |v       |
 * |    0100|buttons |x       |y       |
 * |    0101|layer select              |
 * |--------+--------+--------+--------|
 */

#ifndef _KEYMAP_H
#define _KEYMAP_H
#include "config.h"

typedef struct {
    uint8_t type;
    union {
        uint8_t data[3];
        struct {
            uint8_t empty1;
            uint8_t mod;
            uint8_t code;
        } __attribute__ ((packed)) key;
        struct {
            uint8_t empty2;
            uint16_t code;
        } __attribute__ ((packed)) extra;
        struct {
            uint8_t button;
            int8_t x;
            int8_t y;
        } __attribute__ ((packed)) mouse;
        struct {
            uint8_t button;
            int8_t times;
            int8_t wiggle;
        } __attribute__ ((packed)) automouse;
        struct {
            uint8_t button;
            int8_t h;
            int8_t v;
        } __attribute__ ((packed)) wheel;
        struct {
            uint8_t empty3;
            uint8_t empty4;
            uint8_t number;
        } __attribute__ ((packed)) macro;
        struct {
            uint8_t empty5;
            uint8_t empty6;
            uint8_t number;
        } __attribute__ ((packed)) layer;
        struct {
            uint8_t num1;
            uint8_t num2;
            uint8_t num3;
        } __attribute__ ((packed)) args;
    };
} __attribute__ ((packed)) event_t;

extern event_t keymap[][ROWS_NUM][COLS_NUM];

enum {
    KMT_NONE = 0,

    KMT_AUTOMOUSE,
    KMT_CONSUMER,
    KMT_KEY,
    KMT_LAYER,
    KMT_MACRO,
    KMT_MOUSE,
    KMT_SYSTEM,
    KMT_WHEEL
};

#define _AM(Button,Times,Wiggle)  {.type = KMT_AUTOMOUSE, .automouse = {.button = Button, .times = Times, .wiggle = Wiggle }}
#define _B(Button)                {.type = KMT_MOUSE, .mouse = {.button = Button, .x = 0, .y = 0}}
#define _C(Key)                   {.type = KMT_CONSUMER, .extra = { .code = CONSUMER_##Key }}
#define _K(Key)                   {.type = KMT_KEY, .key = { .mod = 0, .code = KEY_##Key }}
#define _KC(Code)                 {.type = KMT_KEY, .key = { .mod = 0, .code = 0x##Code }}
/* Example CTRL-D: _KM(LCTRL, D) */
#define _KM(ModKey, Key)          {.type = KMT_KEY, .key = { .mod = MOD_##ModKey, .code = KEY_##Key }}
/* Example CTRL-ALT-DEL: _KMB(MOD_LCTRL|MOD_LALT, DELETE) */
#define _KMB(ModBits, Key)        {.type = KMT_KEY, .key = { .mod = ModBits, .code = KEY_##Key }}
#define _L(Layer)                 {.type = KMT_LAYER, .layer = { .number = Layer }}
#define _M(X,Y)                   {.type = KMT_MOUSE, .mouse = {.button = 0, .x = X, .y = Y }}
#define _MA(Number)               {.type = KMT_MACRO, .macro = { .number = Number }}
#define _S(Mod)                   {.type = KMT_KEY, .key = { .code = 0, .mod = Mod }}
#define _W(H,V)                   {.type = KMT_WHEEL, .wheel = {.button = 0, .h = H, .v = V }}
#define _Y(Key)                   {.type = KMT_SYSTEM, .extra = { .code = SYSTEM_##Key }}

extern uint8_t layer;

void keymap_dump(void);
void keymap_set(uint8_t layer, uint8_t row, uint8_t column, event_t *event);
void keymap_event(uint16_t row, uint16_t col, bool pressed);

#endif /* _KEYMAP_H */
