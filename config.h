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

#ifndef _CONFIG_H
#define _CONFIG_H

#include "usb.h"

#define SERIAL_BUF_SIZEIN (EP_SIZE_SERIALDATAOUT)
#define SERIAL_BUF_SIZEOUT (EP_SIZE_SERIALDATAIN)

/*
 * Matrix pinout definition:
 *
 * PA0-A4 = row driver
 * PB0-B2, B6-B7 = column reader
 *
 * COLS_DECODE is used after column GPIO reading to get a consecutive bitfield
 */

#define ROWS_NUM        5
#define ROWS_GPIO       GPIOA
#define ROWS_RCC        RCC_GPIOA
#define ROWS_BV         0b11111

#define COLS_NUM        5
#define COLS_GPIO       GPIOB
#define COLS_RCC        RCC_GPIOB
#define COLS_BV         0b11000111
#define COLS_DECODE(x)  (((x >> 3) & 0b11000) | (x & 0b111))

#define DEBOUNCE_MS     10

#define LEDS_GPIO       GPIOC
#define LEDS_RCC        RCC_GPIOC
#define LEDS_BV         (GPIO13 | GPIO14 | GPIO15)
#define LED1IO          GPIO13
#define LED2IO          GPIO14
#define LED3IO          GPIO15

#endif
