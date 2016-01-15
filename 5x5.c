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
 * 5x5
 *
 * Entrypoint and mainloop for the 5x5 keyboard.
 * - Setup usb for enumeration after reset
 * - Once enumeration is complete, contineously call:
 *   - usb_poll to recieve serial input and leds states
 *   - matrix_process to detect and process matrix events
 *   - serial_out to write output
 */
#include <libopencm3/stm32/rcc.h>

#include "clock.h"
#include "usb.h"
#include "serial.h"
#include "matrix.h"
#include "keyboard.h"
#include "serial.h"
#include "led.h"
#include "elog.h"

static void
mcu_init(void)
{
    rcc_clock_setup_in_hse_16mhz_out_72mhz();
}

int
main(void)
{
    mcu_init();
    clock_init();
    serial_init();
    led_init();
    matrix_init();
    usb_init();

    elog("initialized\n");

    while (1) {
        usb_poll();

        if (serial_active) {
            serial_out();
        }

        if (keyboard_active) {
            matrix_process();
        }
    }
}
