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

#ifndef _USB_H
#define _USB_H

#include <libopencm3/usb/usbd.h>

/*
 * USB Configuration:
 *
 * - 4 interfaces that carry hid endpoints
 *   - 1 endpoint boot keyboard
 *   - 1 endpoint boot mouse
 *   - 1 endpoint extra keys (system control/application keys)
 *   - 1 endpoint nkro keyboard
 * - 3 interfaces for cdc acm definition
 *   - 1 endpoint for communication interrupts
 *   - 1 endpoint for bulk data send
 *   - 1 endpoint for bulk data receive
 *
 * The usb interfaces, endpoints and string indexes are used in the
 * configuration. Internal consistency is easy to distort + magic numbers in
 * code are a horror. Here they are apriori:
 */

#define IF_KEYBOARD                             0
#define IF_MOUSE                                1
#define IF_EXTRAKEY                             2
#define IF_NKRO                                 3
#define IF_SERIALCOMM                           4
#define IF_SERIALDATA                           5
#define IF_MAX                                  6

#define EP_KEYBOARD                             1
#define EP_MOUSE                                2
#define EP_EXTRAKEY                             3
#define EP_NKRO                                 4
#define EP_SERIALCOMM                           5
#define EP_SERIALDATAIN                         6
#define EP_SERIALDATAOUT                        7

#define EP_SIZE_KEYBOARD                        8
#define EP_SIZE_MOUSE                           5
#define EP_SIZE_EXTRAKEY                        3
#define EP_SIZE_NKRO                            29

#define EP_SIZE_SERIALCOMM                      16
#define EP_SIZE_SERIALDATAIN                    64
#define EP_SIZE_SERIALDATAOUT                   32

#define STRI_MANUFACTURER                       1
#define STRI_PRODUCT                            2
#define STRI_SERIAL                             3
#define STRI_KEYBOARD                           4
#define STRI_MOUSE                              5
#define STRI_EXTRAKEY                           6
#define STRI_NKRO                               7
#define STRI_COMMAND                            8
#define STRI_MAX                                8

#define REPORTID_SYSTEM                         1
#define REPORTID_CONSUMER                       2

#define CDC_CONTROL_LINE_STATE_DTR              1
#define CDC_CONTROL_LINE_STATE_RTS              2

#define SEND_RETRIES                           10

/*
 * STM32F1 requires data buffers to be at an 8 byte boundary. Ensure that
 * EP_SIZEs are aligned that way using this macro
 */
#define EP_SIZE_ALIGN(x) ((x + 0b111) & ~0b111)

typedef union {
    uint8_t raw[EP_SIZE_KEYBOARD];
    struct {
        uint8_t mods;
        uint8_t reserved;
        uint8_t keys[EP_SIZE_KEYBOARD - 2];
    };
} __attribute__ ((packed)) report_keyboard_t;

typedef union {
    uint8_t raw[EP_SIZE_MOUSE];
    struct {
        uint8_t buttons;
        int8_t x;
        int8_t y;
        int8_t v;
        int8_t h;
    };
} __attribute__ ((packed)) report_mouse_t;

typedef union {
    uint8_t raw[EP_SIZE_EXTRAKEY];
    struct {
        uint8_t id;
        uint8_t codel;
        uint8_t codeh;
    };
} __attribute__ ((packed)) report_extrakey_t;

typedef union {
    uint8_t raw[EP_SIZE_NKRO];
    struct {
        uint8_t mods;
        uint8_t bits[EP_SIZE_NKRO - 1];
    };
} __attribute__ ((packed)) report_nkro_t;

extern const char *usb_strings[];

extern volatile uint32_t usb_ms;
extern volatile uint32_t usb_ifs_enumerated;
extern volatile uint8_t usb_ep_keyboard_idle;
extern volatile uint8_t usb_ep_mouse_idle;
extern volatile uint8_t usb_ep_nkro_idle;
extern volatile uint8_t usb_ep_extrakey_idle;
extern volatile uint8_t usb_ep_serial_idle;

void usb_init(void);
void usb_prevent_enumeration(void);
uint32_t usb_now(void);

void usb_enumeration_complete(void);
void usb_reset(void);
void usb_resume(void);
void usb_suspend(void);

void usb_update_keyboard(report_keyboard_t *);
void usb_update_mouse(report_mouse_t *);
void usb_update_extrakey(report_extrakey_t *);
void usb_update_nkro(report_nkro_t *);

void usb_endpoint_idle(usbd_device *dev, uint8_t ep);

void cdcacm_data_rx_cb(usbd_device *dev, uint8_t ep);
void cdcacm_data_wx(uint8_t *buf, uint16_t len);

#endif /* _USB_H */
