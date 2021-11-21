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
 * Matrix
 *
 * Tasks:
 * - scan the hardware matrix
 * - determine if there is a keydown or keyup event
 * - if so, trigger keyboard, mouse, extrakey events
 */

#include <stdlib.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "clock.h"
#include "config.h"
#include "serial.h"
#include "matrix.h"
#include "keymap.h"


static uint32_t debounce;
static bool matrix_update;
matrix_t matrix;
static matrix_t matrix_debounce;
static matrix_t matrix_previous;
uint8_t show_matrix = 0;

/*
 * row_select
 *
 * Pull a row high
 */
static void
row_select(uint8_t r)
{
    GPIO_BSRR(ROWS_GPIO) = (1 << r);
}

/*
 * row_clear
 *
 * Pull a row low
 */
static void
row_clear(void)
{
    GPIO_BSRR(ROWS_GPIO) = (ROWS_BV << 16);
}

/*
 * col_read
 *
 * Read a column bits and return them in ascending order. Note that this
 * function hides the physical column wiring.
 */
static uint16_t
col_read(void)
{
    uint16_t c = (uint16_t)(GPIO_IDR(COLS_GPIO) & COLS_BV);

    return COLS_DECODE(c);
}

/*
 * matrix_init
 *
 * Initialize the matrix module. Must be called before any other function in
 * this module is called. Initializes the hardware and module local variables
 * we depend on.
 */
void
matrix_init(void)
{
    uint8_t i;

    rcc_periph_clock_enable(ROWS_RCC);
    rcc_periph_clock_enable(COLS_RCC);
    gpio_set_mode(ROWS_GPIO, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, ROWS_BV);
    gpio_set_mode(COLS_GPIO, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, COLS_BV);

    row_clear();

    for (i = 0; i < ROWS_NUM; i++) {
        matrix.row[i] = 0;
        matrix_debounce.row[i] = 0;
        matrix_previous.row[i] = 0;
    }
}

/*
 * matrix_scan
 *
 * Read the matrix; select each row, and read column value. Debounce by quickly
 * noting the current value, and only taking that value after it has been
 * stable for DEBOUNCE ms.
 */
void
matrix_scan()
{
    uint8_t r;
    uint16_t col;

    for (r = 0; r < ROWS_NUM; r++) {
        row_select(r);
        col = col_read();
        if (matrix_debounce.row[r] != col) {
            matrix_debounce.row[r] = col;
            matrix_update = true;
            debounce = timer_set(MS_DEBOUNCE);
        }
        row_clear();
    }

    if (matrix_update && timer_passed(debounce)) {
        matrix_update = false;
        for (r = 0; r < ROWS_NUM; r++) {
            matrix.row[r] = matrix_debounce.row[r];
        }
    }
}

/*
 * matrix_process
 *
 * Generate key up/down events depending on the current and previous scan
 * state.
 */
void
matrix_process()
{
    uint8_t r, c;
    uint16_t col, colbit;

    /* Make sure that we pick up new scan events */
    matrix_scan();

    /*
     * Check for scan events, even if the previous matrix scan returned
     * nothing. We might still have some unprocessed events in our previous
     * matrix.
     */

    for (r = 0; r < ROWS_NUM; r++) {
        col = matrix.row[r] ^ matrix_previous.row[r];
        if (col) {
            for (c = 0; c < COLS_NUM; c++) {
                colbit = (1 << c);
                if (col & colbit) {
                    if (show_matrix) matrix_debug();
                    keymap_event(r, c, matrix.row[r] & colbit);
                    matrix_previous.row[r] ^= colbit;
                }
            }
        }
    }
}

/*
 * matrix_debug
 *
 * Debugging routine that will emit the current module local state
 */
void
matrix_debug(void)
{
    uint8_t i;

    for (i = 0; i < ROWS_NUM; i++) {
        printf("%02x :%02x\n", i, (unsigned int) matrix.row[i]);
    }
}
