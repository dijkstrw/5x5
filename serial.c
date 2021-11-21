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
 * Virtual serial port
 *
 * The CDC ACM endpoints of the usb driver are used to communicate with the
 * outside world. Internally, this virtual serial port has a set of ringbuffers
 * for incoming and outgoing communication. A minimal printf function is tied
 * to the output_buffer ring.
 *
 * This software uses a slighly modified version of the mini-printf library by
 * Michal Ludvig:
 * === mini-printf ===
 * https://github.com/mludvig/mini-printf, The Minimal snprintf()
 * implementation
 * Copyright (c) 2013,2014 Michal Ludvig michal@logix.cz All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the auhor nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without specific
 *     prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * === mini-printf ===
 */

#include <string.h>
#include <stdarg.h>
#include "config.h"
#include "ring.h"
#include "serial.h"
#include "command.h"

static struct ring output_ring;
static struct ring input_ring;
static uint8_t input_buffer[SERIAL_BUF_SIZEIN];
static uint8_t output_buffer[SERIAL_BUF_SIZEOUT];
bool serial_active;

void
serial_init()
{
    ring_init(&output_ring, output_buffer, SERIAL_BUF_SIZEOUT);
    ring_init(&input_ring, input_buffer, SERIAL_BUF_SIZEIN);
    serial_active = false;
}

void
serial_in(uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint8_t eol = 0;
    uint8_t c;

    for (i = 0; i < len; i++) {
        c = *(buf + i);
        ring_write_ch(&input_ring, c);
        ring_write_ch(&output_ring, c);

        /* emit additional linefeed for terminals that give us carriage returns */
        if (c == '\r') {
            ring_write_ch(&output_ring, '\n');
        }

        /* have we seen an end of line */
        eol |= ((c == '\n') | (c == '\r'));
    }

    if (eol) {
        /* line received, decode command */
        command_process(&input_ring);
    }
}

void
serial_out()
{
    uint8_t *buf;
    int32_t len;

    if (usb_ep_serial_idle) {
        if (RING_EMPTY(&output_ring))
            return;

        len = ring_read_contineous(&output_ring, &buf, EP_SIZE_SERIALDATAOUT);
        cdcacm_data_wx(buf, len);
    }
}

static uint32_t
itoa(int32_t value, uint32_t radix, uint32_t uppercase, uint32_t unsig,
     char *buffer, uint32_t zero_pad)
{
    char *pbuffer = buffer;
    int32_t negative = 0;
    uint32_t i, len;

    /* No support for unusual radixes. */
    if (radix > 16)
        return 0;

    if (value < 0 && !unsig) {
        negative = 1;
        value = -value;
    }

    /* This builds the string back to front ... */
    do {
        int digit = value % radix;
        *(pbuffer++) = (digit < 10 ? '0' + digit : (uppercase ? 'A' : 'a') + digit - 10);
        value /= radix;
    } while (value > 0);

    for (i = (pbuffer - buffer); i < zero_pad; i++)
        *(pbuffer++) = '0';

    if (negative)
        *(pbuffer++) = '-';

    *(pbuffer) = '\0';

    /* ... now we reverse it (could do it recursively but will
     * conserve the stack space) */
    len = (pbuffer - buffer);
    for (i = 0; i < len / 2; i++) {
        char j = buffer[i];
        buffer[i] = buffer[len-i-1];
        buffer[len-i-1] = j;
    }

    return len;
}

static int
vrprintf(struct ring *ring, const char *fmt, va_list va)
{
    uint32_t mark = ring_mark(ring);
    char bf[24];
    char ch;

    while ((ch = *(fmt++))) {
        if (ch == '\n') {
            ring_write_ch(ring, '\n');
            ring_write_ch(ring, '\r');
        } else if (ch != '%') {
            ring_write_ch(ring, ch);
        } else {
            char zero_pad = 0;
            char *ptr;
            uint32_t len;

            ch = *(fmt++);

            /* Zero padding requested */
            if (ch == '0') {
                ch = *(fmt++);
                if (ch == '\0')
                    goto end;
                if (ch >= '0' && ch <= '9')
                    zero_pad = ch - '0';
                ch = *(fmt++);
            }

            switch (ch) {
            case 0:
                goto end;

            case 'u':
            case 'd':
                len = itoa(va_arg(va, uint32_t), 10, 0, (ch == 'u'), bf, zero_pad);
                ring_write(ring, (uint8_t *)bf, len);
                break;

            case 'x':
            case 'X':
                len = itoa(va_arg(va, uint32_t), 16, (ch == 'X'), 1, bf, zero_pad);
                ring_write(ring, (uint8_t *)bf, len);
                break;

            case 'c' :
                ring_write_ch(ring, (char)(va_arg(va, int)));
                break;

            case 's' :
                ptr = va_arg(va, char*);
                ring_write(ring, (uint8_t *)ptr, strlen(ptr));
                break;

            default:
                ring_write_ch(ring, ch);
                break;
            }
        }
    }
end:
    return ring_marklen(ring, mark);
}


int
printf(const char *fmt, ...)
{
    int ret;
    va_list va;
    va_start(va, fmt);
    ret = vrprintf(&output_ring, fmt, va);
    va_end(va);

    return ret;
}

int
printfnl(const char *fmt, ...)
{
    int ret;
    va_list va;
    va_start(va, fmt);
    ret = vrprintf(&output_ring, fmt, va);

    ring_write_ch(&output_ring, '\n');
    ring_write_ch(&output_ring, '\r');

    va_end(va);

    return ret;
}

int
puts(const char *s)
{
    return ring_write(&output_ring, (uint8_t *)s, strlen(s));
}
