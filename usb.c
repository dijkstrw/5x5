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
 * USB interface
 *
 * Use the libopencm3 usb stack on top of a stm32f103 to provide usb
 * enumeration as a:
 * - boot usb keyboard
 * - boot usb mouse
 * - usb extrakey that can report <system|consumer> control/application events
 * - nkro usb keyboard
 * - a cdc/acm usb serial port
 */

#include <stdlib.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>
#include <libopencm3/stm32/st_usbfs.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/usb/hid.h>
#include <libopencm3/usb/usbd.h>

#include "descriptor.h"
#include "elog.h"
#include "extrakey.h"
#include "hid.h"
#include "keyboard.h"
#include "mouse.h"
#include "usb.h"
#include "usb_keycode.h"

static usbd_device *usbd_dev;
volatile uint32_t usb_ms;
volatile uint32_t usb_ifs_enumerated;
volatile uint8_t usb_ep_keyboard_idle;
volatile uint8_t usb_ep_mouse_idle;
volatile uint8_t usb_ep_nkro_idle;
volatile uint8_t usb_ep_extrakey_idle;
volatile uint8_t usb_ep_serial_idle;

/*
 * USB hid
 *
 * Interface, endpoint and hid report definitions for the USB human interface
 * devices we provide.
 */

/*
 * Keyboard boot report, taken from USB HID 1.11 Appendix B
 *
 * Input report (8 bytes):
 * | byte | description   |
 * |------+---------------|
 * |    0 | Modifier keys |
 * |    1 | Reserved      |
 * |    2 | Keycode 1     |
 * |    3 | Keycode 2     |
 * |    4 | Keycode 3     |
 * |    5 | Keycode 4     |
 * |    6 | Keycode 5     |
 * |    7 | Keycode 6     |
 *
 * Output report (1 byte):
 * | bit | description   |
 * |-----+---------------|
 * |   0 | Num Lock      |
 * |   1 | Caps Lock     |
 * |   2 | Scroll Lock   |
 * |   3 | Compose       |
 * |   4 | Kana          |
 * | 5-7 | CONSTANT      |
 */
const uint8_t keyboard_report_descriptor[] = {
    HID_RI_USAGE_PAGE(8, 0x01),                /* Generic Desktop */
    HID_RI_USAGE(8, 0x06),                     /* Keyboard */
    HID_RI_COLLECTION(8, 0x01),                /* Application */
        HID_RI_USAGE_PAGE(8, 0x07),            /* Key Codes */
        HID_RI_USAGE_MINIMUM(8, 0xE0),         /* Keyboard Left Control */
        HID_RI_USAGE_MAXIMUM(8, 0xE7),         /* Keyboard Right GUI */
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0x01),
        HID_RI_REPORT_COUNT(8, 0x08),
        HID_RI_REPORT_SIZE(8, 0x01),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),

        HID_RI_REPORT_COUNT(8, 0x01),
        HID_RI_REPORT_SIZE(8, 0x08),
        HID_RI_INPUT(8, HID_IOF_CONSTANT),     /* Reserved */

        HID_RI_USAGE_PAGE(8, 0x08),            /* LEDs */
        HID_RI_USAGE_MINIMUM(8, 0x01),         /* Num Lock */
        HID_RI_USAGE_MAXIMUM(8, 0x05),         /* Kana */
        HID_RI_REPORT_COUNT(8, 0x05),
        HID_RI_REPORT_SIZE(8, 0x01),
        HID_RI_OUTPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE | HID_IOF_NON_VOLATILE),
        HID_RI_REPORT_COUNT(8, 0x01),
        HID_RI_REPORT_SIZE(8, 0x03),
        HID_RI_OUTPUT(8, HID_IOF_CONSTANT),    /* Led padding */

        HID_RI_USAGE_PAGE(8, 0x07),            /* Keyboard */
        HID_RI_USAGE_MINIMUM(8, 0x00),         /* Reserved (no event indicated) */
        HID_RI_USAGE_MAXIMUM(8, 0xFF),         /* Keyboard Application */
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0xFF),
        HID_RI_REPORT_COUNT(8, 0x06),
        HID_RI_REPORT_SIZE(8, 0x08),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_ARRAY | HID_IOF_ABSOLUTE),
    HID_RI_END_COLLECTION(0),
};

static const struct {
    struct usb_hid_descriptor hid_descriptor;
    struct {
        uint8_t bReportDescriptorType;
        uint16_t wDescriptorLength;
    } __attribute__((packed)) hid_report;
} __attribute__((packed)) keyboard_function = {
    .hid_descriptor = {
        .bLength = sizeof(keyboard_function),
        .bDescriptorType = USB_DT_HID,
        .bcdHID = 0x0111,
        .bCountryCode = 0,
        .bNumDescriptors = 1,
    },
    .hid_report = {
        .bReportDescriptorType = USB_DT_REPORT,
        .wDescriptorLength = sizeof(keyboard_report_descriptor),
    }
};

const struct usb_endpoint_descriptor keyboard_endpoint = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_ENDPOINT_ADDR_IN(EP_KEYBOARD),
    .bmAttributes = (USB_ENDPOINT_ATTR_INTERRUPT |
                     USB_ENDPOINT_ATTR_NOSYNC |
                     USB_ENDPOINT_ATTR_DATA),
    .wMaxPacketSize = EP_SIZE_KEYBOARD,
    .bInterval = 0x0A,
};

const struct usb_interface_descriptor keyboard_iface = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = IF_KEYBOARD,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_HID,
    .bInterfaceSubClass = ID_IS_BOOT,
    .bInterfaceProtocol = ID_IP_KEYBOARD,
    .iInterface = STRI_KEYBOARD,

    .endpoint = &keyboard_endpoint,

    .extra = &keyboard_function,
    .extralen = sizeof(keyboard_function),
};

/*
 * Mouse boot report, taken from USB HID 1.11 Appendix B
 *
 * Input report (5 bytes):
 * | byte | description |
 * |------+-------------|
 * |    0 | Buttons     |
 * |    1 | x           |
 * |    2 | y           |
 * |    3 | w           |
 * |    4 | h           |
 */
const uint8_t mouse_report_descriptor[] = {
    HID_RI_USAGE_PAGE(8, 0x01),                /* Generic Desktop */
    HID_RI_USAGE(8, 0x02),                     /* Mouse */
    HID_RI_COLLECTION(8, 0x01),                /* Application */
        HID_RI_USAGE(8, 0x01),                 /* Pointer */
        HID_RI_COLLECTION(8, 0x00),            /* Physical */

            HID_RI_USAGE_PAGE(8, 0x09),        /* Button */
            HID_RI_USAGE_MINIMUM(8, 0x01),     /* Button 1 */
            HID_RI_USAGE_MAXIMUM(8, 0x05),     /* Button 5 */
            HID_RI_LOGICAL_MINIMUM(8, 0x00),
            HID_RI_LOGICAL_MAXIMUM(8, 0x01),
            HID_RI_REPORT_COUNT(8, 0x05),
            HID_RI_REPORT_SIZE(8, 0x01),
            HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),
            HID_RI_REPORT_COUNT(8, 0x01),
            HID_RI_REPORT_SIZE(8, 0x03),
            HID_RI_INPUT(8, HID_IOF_CONSTANT),

            HID_RI_USAGE_PAGE(8, 0x01),        /* Generic Desktop */
            HID_RI_USAGE(8, 0x30),             /* Usage X */
            HID_RI_USAGE(8, 0x31),             /* Usage Y */
            HID_RI_LOGICAL_MINIMUM(8, -127),
            HID_RI_LOGICAL_MAXIMUM(8, 127),
            HID_RI_REPORT_COUNT(8, 0x02),
            HID_RI_REPORT_SIZE(8, 0x08),
            HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_RELATIVE),

            HID_RI_USAGE(8, 0x38),             /* Wheel */
            HID_RI_LOGICAL_MINIMUM(8, -127),
            HID_RI_LOGICAL_MAXIMUM(8, 127),
            HID_RI_REPORT_COUNT(8, 0x01),
            HID_RI_REPORT_SIZE(8, 0x08),
            HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_RELATIVE),

            HID_RI_USAGE_PAGE(8, 0x0C),        /* Consumer */
            HID_RI_USAGE(16, 0x0238),          /* AC Pan (Horizontal wheel) */
            HID_RI_LOGICAL_MINIMUM(8, -127),
            HID_RI_LOGICAL_MAXIMUM(8, 127),
            HID_RI_REPORT_COUNT(8, 0x01),
            HID_RI_REPORT_SIZE(8, 0x08),
            HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_RELATIVE),

        HID_RI_END_COLLECTION(0),
    HID_RI_END_COLLECTION(0),
};

static const struct {
    struct usb_hid_descriptor hid_descriptor;
    struct {
        uint8_t bReportDescriptorType;
        uint16_t wDescriptorLength;
    } __attribute__((packed)) hid_report;
} __attribute__((packed)) mouse_function = {
    .hid_descriptor = {
        .bLength = sizeof(mouse_function),
        .bDescriptorType = USB_DT_HID,
        .bcdHID = 0x0111,
        .bCountryCode = 0,
        .bNumDescriptors = 1,
    },
    .hid_report = {
        .bReportDescriptorType = USB_DT_REPORT,
        .wDescriptorLength = sizeof(mouse_report_descriptor),
    }
};

const struct usb_endpoint_descriptor mouse_endpoint = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_ENDPOINT_ADDR_IN(EP_MOUSE),
    .bmAttributes = (USB_ENDPOINT_ATTR_INTERRUPT |
                     USB_ENDPOINT_ATTR_NOSYNC |
                     USB_ENDPOINT_ATTR_DATA),
    .wMaxPacketSize = EP_SIZE_MOUSE,
    .bInterval = 0x0A,
};

const struct usb_interface_descriptor mouse_iface = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = IF_MOUSE,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_HID,
    .bInterfaceSubClass = ID_IS_BOOT,
    .bInterfaceProtocol = ID_IP_MOUSE,
    .iInterface = STRI_MOUSE,

    .endpoint = &mouse_endpoint,

    .extra = &mouse_function,
    .extralen = sizeof(mouse_function),
};

/*
 * Extra key report
 *
 * Input report (3 bytes):
 * | byte | description   |
 * |------+---------------|
 * |    0 | report id     |
 * |  1-2 | keycode       |
 */
static const uint8_t extrakey_report_descriptor[] = {
    HID_RI_USAGE_PAGE(8, 0x01),                /* Generic Desktop */
    HID_RI_USAGE(8, 0x80),                     /* System Control */
    HID_RI_COLLECTION(8, 0x01),                /* Application */
        HID_RI_REPORT_ID(8, REPORTID_SYSTEM),
        HID_RI_USAGE_MINIMUM(16, SYSTEM_START),
        HID_RI_USAGE_MAXIMUM(16, SYSTEM_DPADLEFT),
        HID_RI_LOGICAL_MINIMUM(16, SYSTEM_START),
        HID_RI_LOGICAL_MAXIMUM(16, SYSTEM_DPADLEFT),
        HID_RI_REPORT_SIZE(8, 16),
        HID_RI_REPORT_COUNT(8, 1),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_ARRAY | HID_IOF_ABSOLUTE),
    HID_RI_END_COLLECTION(0),

    HID_RI_USAGE_PAGE(8, 0x0C),                /* Consumer */
    HID_RI_USAGE(8, 0x01),                     /* Consumer Control */
    HID_RI_COLLECTION(8, 0x01),                /* Application */
    HID_RI_REPORT_ID(8, REPORTID_CONSUMER),
        HID_RI_USAGE_MINIMUM(16, CONSUMER_POWER),
        HID_RI_USAGE_MAXIMUM(16, CONSUMER_AC_SEND),      /* AC Distribute Vertically */
        HID_RI_LOGICAL_MINIMUM(16, CONSUMER_POWER),
        HID_RI_LOGICAL_MAXIMUM(16, CONSUMER_AC_SEND),
        HID_RI_REPORT_SIZE(8, 16),
        HID_RI_REPORT_COUNT(8, 1),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_ARRAY | HID_IOF_ABSOLUTE),
    HID_RI_END_COLLECTION(0),
};

static const struct {
    struct usb_hid_descriptor hid_descriptor;
    struct {
        uint8_t bReportDescriptorType;
        uint16_t wDescriptorLength;
    } __attribute__((packed)) hid_report;
} __attribute__((packed)) extrakey_function = {
    .hid_descriptor = {
        .bLength = sizeof(extrakey_function),
        .bDescriptorType = USB_DT_HID,
        .bcdHID = 0x0111,
        .bCountryCode = 0,
        .bNumDescriptors = 1,
    },
    .hid_report = {
        .bReportDescriptorType = USB_DT_REPORT,
        .wDescriptorLength = sizeof(extrakey_report_descriptor),
    }
};

const struct usb_endpoint_descriptor extrakey_endpoint = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_ENDPOINT_ADDR_IN(EP_EXTRAKEY),
    .bmAttributes = (USB_ENDPOINT_ATTR_INTERRUPT |
                     USB_ENDPOINT_ATTR_NOSYNC |
                     USB_ENDPOINT_ATTR_DATA),
    .wMaxPacketSize = EP_SIZE_EXTRAKEY,
    .bInterval = 0x0A,
};

const struct usb_interface_descriptor extrakey_iface = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = IF_EXTRAKEY,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_HID,
    .bInterfaceSubClass = ID_IS_NONE,
    .bInterfaceProtocol = ID_IP_NONE,
    .iInterface = STRI_EXTRAKEY,

    .endpoint = &extrakey_endpoint,

    .extra = &extrakey_function,
    .extralen = sizeof(extrakey_function),
};

/*
 * NKRO Report
 *
 * Input report (8 bytes):
 * | byte | description   |
 * |------+---------------|
 * |    0 | Modifier keys |
 * |    1+| Key bitfield  |
 *
 * The key bitfield starts at keycode for 'a' and continues EP_SIZE_NKRO
 * bytes. Each bitfield byte holds the status for 8 additional keys.
 *
 * | EP_SIZE_NKRO | total keys |
 * |--------------+------------|
 * |           29 |        224 |
 * |           19 |        144 |
 *
 * Output report (1 byte):
 * | bit | description   |
 * |-----+---------------|
 * |   0 | Num Lock      |
 * |   1 | Caps Lock     |
 * |   2 | Scroll Lock   |
 * |   3 | Compose       |
 * |   4 | Kana          |
 * | 5-7 | CONSTANT      |
 */
const uint8_t nkro_report_descriptor[] =
{
    HID_RI_USAGE_PAGE(8, 0x01),                /* Generic Desktop */
    HID_RI_USAGE(8, 0x06),                     /* Keyboard */
    HID_RI_COLLECTION(8, 0x01),                /* Application */
        HID_RI_USAGE_PAGE(8, 0x07),            /* Key Codes */
        HID_RI_USAGE_MINIMUM(8, 0xE0),         /* Keyboard Left Control */
        HID_RI_USAGE_MAXIMUM(8, 0xE7),         /* Keyboard Right GUI */
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0x01),
        HID_RI_REPORT_COUNT(8, 0x08),
        HID_RI_REPORT_SIZE(8, 0x01),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),

        HID_RI_USAGE_PAGE(8, 0x08),            /* LEDs */
        HID_RI_USAGE_MINIMUM(8, 0x01),         /* Num Lock */
        HID_RI_USAGE_MAXIMUM(8, 0x05),         /* Kana */
        HID_RI_REPORT_COUNT(8, 0x05),
        HID_RI_REPORT_SIZE(8, 0x01),
        HID_RI_OUTPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE | HID_IOF_NON_VOLATILE),
        HID_RI_REPORT_COUNT(8, 0x01),
        HID_RI_REPORT_SIZE(8, 0x03),
        HID_RI_OUTPUT(8, HID_IOF_CONSTANT),    /* Led padding */

        HID_RI_USAGE_PAGE(8, 0x07),            /* Keyboard */
        HID_RI_USAGE_MINIMUM(8, KEY_A),
        HID_RI_USAGE_MAXIMUM(8, ((EP_SIZE_NKRO -1 ) * 8) + KEY_A - 1),
        HID_RI_LOGICAL_MINIMUM(8, 0x00),
        HID_RI_LOGICAL_MAXIMUM(8, 0x01),
        HID_RI_REPORT_COUNT(8, (EP_SIZE_NKRO - 1) * 8),
        HID_RI_REPORT_SIZE(8, 0x01),
        HID_RI_INPUT(8, HID_IOF_DATA | HID_IOF_VARIABLE | HID_IOF_ABSOLUTE),
    HID_RI_END_COLLECTION(0),
};

static const struct {
    struct usb_hid_descriptor hid_descriptor;
    struct {
        uint8_t bReportDescriptorType;
        uint16_t wDescriptorLength;
    } __attribute__((packed)) hid_report;
} __attribute__((packed)) nkro_function = {
    .hid_descriptor = {
        .bLength = sizeof(nkro_function),
        .bDescriptorType = USB_DT_HID,
        .bcdHID = 0x0111,
        .bCountryCode = 0,
        .bNumDescriptors = 1,
    },
    .hid_report = {
        .bReportDescriptorType = USB_DT_REPORT,
        .wDescriptorLength = sizeof(nkro_report_descriptor),
    }
};

const struct usb_endpoint_descriptor nkro_endpoint = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_ENDPOINT_ADDR_IN(EP_NKRO),
    .bmAttributes = (USB_ENDPOINT_ATTR_INTERRUPT |
                     USB_ENDPOINT_ATTR_NOSYNC |
                     USB_ENDPOINT_ATTR_DATA),
    .wMaxPacketSize = EP_SIZE_NKRO,
    .bInterval = 0x01,
};

const struct usb_interface_descriptor nkro_iface = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = IF_NKRO,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_HID,
    .bInterfaceSubClass = ID_IS_NONE,
    .bInterfaceProtocol = ID_IP_NONE,
    .iInterface = STRI_NKRO,

    .endpoint = &nkro_endpoint,

    .extra = &nkro_function,
    .extralen = sizeof(nkro_function),
};

/*
 * USB cdc acm
 *
 * Interfaces and endpoints for the USB "virtual serial port" we provide.
 */

static const struct usb_endpoint_descriptor comm_endpoint[] = {{
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_ENDPOINT_ADDR_IN(EP_SERIALCOMM),
    .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
    .wMaxPacketSize = EP_SIZE_SERIALCOMM,
    .bInterval = 0xff,
    }};

static const struct usb_endpoint_descriptor data_endpoint[] = {{
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_ENDPOINT_ADDR_OUT(EP_SERIALDATAOUT),
    .bmAttributes = USB_ENDPOINT_ATTR_BULK,
    .wMaxPacketSize = EP_SIZE_SERIALDATAOUT,
    .bInterval = 0x0a,
    }, {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_ENDPOINT_ADDR_IN(EP_SERIALDATAIN),
    .bmAttributes = USB_ENDPOINT_ATTR_BULK,
    .wMaxPacketSize = EP_SIZE_SERIALDATAIN,
    .bInterval = 0x0a,
    }};

static const struct {
    struct usb_cdc_header_descriptor header;
    struct usb_cdc_call_management_descriptor call_mgmt;
    struct usb_cdc_acm_descriptor acm;
    struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
    .header = {
        .bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_HEADER,
        .bcdCDC = 0x0110,
    },
    .call_mgmt = {
        .bFunctionLength =
        sizeof(struct usb_cdc_call_management_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
        .bmCapabilities = 0,
        .bDataInterface = IF_SERIALDATA,
    },
    .acm = {
        .bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_ACM,
        .bmCapabilities = 0,
    },
    .cdc_union = {
        .bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
        .bDescriptorType = CS_INTERFACE,
        .bDescriptorSubtype = USB_CDC_TYPE_UNION,
        .bControlInterface = IF_SERIALCOMM,
        .bSubordinateInterface0 = IF_SERIALDATA,
    },
};

static const struct usb_interface_descriptor cdc_comm_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = IF_SERIALCOMM,
    .bAlternateSetting = 0,
    .bNumEndpoints = 1,
    .bInterfaceClass = USB_CLASS_CDC,
    .bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
    .bInterfaceProtocol = USB_CDC_PROTOCOL_NONE,
    .iInterface = STRI_COMMAND,

    .endpoint = comm_endpoint,

    .extra = &cdcacm_functional_descriptors,
    .extralen = sizeof(cdcacm_functional_descriptors),
    }};

static const struct usb_interface_descriptor cdc_data_iface[] = {{
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = IF_SERIALDATA,
    .bAlternateSetting = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_DATA,
    .bInterfaceSubClass = 0,
    .bInterfaceProtocol = 0,
    .iInterface = STRI_COMMAND,

    .endpoint = data_endpoint,
    }};

const struct usb_interface ifaces[] = {{
    .num_altsetting = 1,
    .altsetting = &keyboard_iface,
    }, {
    .num_altsetting = 1,
    .altsetting = &mouse_iface,
    }, {
    .num_altsetting = 1,
    .altsetting = &extrakey_iface,
    }, {
    .num_altsetting = 1,
    .altsetting = &nkro_iface,
    }, {
        .num_altsetting = 1,
    .altsetting = cdc_comm_iface,
    }, {
    .num_altsetting = 1,
    .altsetting = cdc_data_iface,
    }};

const struct usb_device_descriptor dev_descriptor = {
    .bLength = USB_DT_DEVICE_SIZE,
    .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0110,
    .bDeviceClass = 0,                     /* Each interface will specify its own class code */
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = 64,
    .idVendor = 0xDEAD,
    .idProduct = 0xBEEF,
    .bcdDevice = 0x010,
    .iManufacturer = STRI_MANUFACTURER,
    .iProduct = STRI_PRODUCT,
    .iSerialNumber = STRI_SERIAL,
    .bNumConfigurations = 1,
};


const struct usb_config_descriptor config = {
    .bLength = USB_DT_CONFIGURATION_SIZE,
    .bDescriptorType = USB_DT_CONFIGURATION,
    .wTotalLength = 0,
    .bNumInterfaces = IF_MAX,
    .bConfigurationValue = 1,
    .iConfiguration = 0,
    .bmAttributes = (CD_A_RESERVED | CD_A_REMOTEWAKEUP),
    .bMaxPower = CD_MP_100MA,

    .interface = ifaces,
};

const char *usb_strings[] = {
    "dijkstra.xyz",
    "Gojira",
    GOJIRA_VERSION,
    "Boot keyboard",
    "Boot mouse",
    "Control keyboard",
    "NKRO keyboard",
    "Command channel"
};

static enum usbd_request_return_codes
usb_control_request(usbd_device *dev, struct usb_setup_data *req, uint8_t **buf,
                    uint16_t *len, void (**complete)(usbd_device *dev, struct usb_setup_data *req))
{
    (void)complete;
    (void)buf;
    (void)dev;

    if (req->bRequest == USB_REQ_GET_DESCRIPTOR) {
        uint8_t dtype = (req->wValue >> 8);

        if (dtype == USB_DT_REPORT) {
            switch (req->wIndex) {
            case IF_KEYBOARD:
                *buf = (uint8_t *) &keyboard_report_descriptor;
                *len = sizeof(keyboard_report_descriptor);
                usb_ifs_enumerated |= (1 << IF_KEYBOARD);
                return USBD_REQ_HANDLED;
                break;

            case IF_MOUSE:
                *buf = (uint8_t *) &mouse_report_descriptor;
                *len = sizeof(mouse_report_descriptor);
                usb_ifs_enumerated |= (1 << IF_MOUSE);
                return USBD_REQ_HANDLED;
                break;

            case IF_EXTRAKEY:
                *buf = (uint8_t *) &extrakey_report_descriptor;
                *len = sizeof(extrakey_report_descriptor);
                usb_ifs_enumerated |= (1 << IF_EXTRAKEY);
                return USBD_REQ_HANDLED;
                break;

            case IF_NKRO:
                *buf = (uint8_t *) &nkro_report_descriptor;
                *len = sizeof(nkro_report_descriptor);
                usb_ifs_enumerated |= (1 << IF_NKRO);
                return USBD_REQ_HANDLED;
                break;
            }
        }
    } else if (req->bRequest == USBHID_REQ_GET_REPORT) {
        switch (req->wIndex) {
        case IF_KEYBOARD:
            *buf = (uint8_t *) keyboard_report();
            *len = sizeof(report_keyboard_t);
            return USBD_REQ_HANDLED;
            break;

        case IF_MOUSE:
            *buf = (uint8_t *) mouse_report();
            *len = sizeof(report_mouse_t);
            return USBD_REQ_HANDLED;
            break;

        case IF_EXTRAKEY:
            *buf = (uint8_t *) extrakey_report();
            *len = sizeof(report_extrakey_t);
            return USBD_REQ_HANDLED;
            break;

        case IF_NKRO:
            *buf = (uint8_t *) nkro_report();
            *len = sizeof(report_nkro_t);
            return USBD_REQ_HANDLED;
            break;
        }
    } else if (req->bRequest == USBHID_REQ_SET_REPORT) {
        switch (req->wIndex) {
        case IF_KEYBOARD:
        case IF_NKRO:
            if (len && *len && buf && *buf)
                keyboard_set_leds(**buf);
            return USBD_REQ_HANDLED;
            break;
        }
    } else if (req->bRequest == USBHID_REQ_GET_IDLE) {
        switch (req->wIndex) {
        case IF_KEYBOARD:
            *buf = &keyboard_idle;
            *len = sizeof(keyboard_idle);
            return USBD_REQ_HANDLED;
            break;

        case IF_MOUSE:
            *buf = &mouse_idle;
            *len = sizeof(mouse_idle);
            return USBD_REQ_HANDLED;
            break;

        case IF_EXTRAKEY:
            *buf = &extrakey_idle;
            *len = sizeof(extrakey_idle);
            return USBD_REQ_HANDLED;
            break;

        case IF_NKRO:
            *buf = &nkro_idle;
            *len = sizeof(nkro_idle);
            return USBD_REQ_HANDLED;
            break;
        }
    } else if (req->bRequest == USBHID_REQ_SET_IDLE) {
        uint8_t idlerate = (req->wValue >> 8);

        switch (req->wIndex) {
        case IF_KEYBOARD:
            keyboard_idle = idlerate;
            return USBD_REQ_HANDLED;
            break;

        case IF_MOUSE:
            mouse_idle = idlerate;
            return USBD_REQ_HANDLED;
            break;

        case IF_EXTRAKEY:
            extrakey_idle = idlerate;
            return USBD_REQ_HANDLED;
            break;

        case IF_NKRO:
            nkro_idle = idlerate;
            return USBD_REQ_HANDLED;
            break;
        }
    } else if (req->bRequest == USBHID_REQ_GET_PROTOCOL) {
        switch (req->wIndex) {
        case IF_KEYBOARD:
        case IF_NKRO:
            *buf = keyboard_get_protocol();
            *len = 1;
            return USBD_REQ_HANDLED;
        }
    } else if (req->bRequest == USBHID_REQ_SET_PROTOCOL) {
        switch (req->wIndex) {
        case IF_KEYBOARD:
        case IF_NKRO:
            keyboard_set_protocol(req->wValue);
            return USBD_REQ_HANDLED;
        }
    } else if (req->bRequest == USB_CDC_REQ_SET_LINE_CODING) {
        usb_ifs_enumerated |= (1 << IF_SERIALCOMM);
        return USBD_REQ_HANDLED;
    } else if (req->bRequest == USB_CDC_REQ_SET_CONTROL_LINE_STATE) {
        if (req->wValue & (CDC_CONTROL_LINE_STATE_DTR |
                           CDC_CONTROL_LINE_STATE_RTS)) {
            usb_ep_serial_idle = 1;
            elog("serial attached");
        } else {
            /* serial detached */
            usb_ep_serial_idle = 0;
        }
        return USBD_REQ_HANDLED;
    }
    return USBD_REQ_NEXT_CALLBACK;
}

static uint16_t
usb_write_packet(usbd_device *dev, uint8_t addr, const void* buf, uint16_t len)
{
    int tries = 0;
    uint16_t wlen;

    for (wlen = usbd_ep_write_packet(dev, addr, buf, len);
         (wlen == 0) && (tries < SEND_RETRIES);
         tries++);

    if (wlen == 0) {
        elog("could not send packet to %x", addr);
    }
    return wlen;
}

void
usb_update_keyboard(report_keyboard_t *report)
{
    usb_ep_keyboard_idle = 0;
    usb_write_packet(usbd_dev, EP_KEYBOARD, report->raw, EP_SIZE_KEYBOARD);
}

void
usb_update_mouse(report_mouse_t *report)
{
    usb_ep_mouse_idle = 0;
    usb_write_packet(usbd_dev, EP_MOUSE, &report->raw, EP_SIZE_MOUSE);
}

void
usb_update_extrakey(report_extrakey_t *report)
{
    usb_ep_extrakey_idle = 0;
    usb_write_packet(usbd_dev, EP_EXTRAKEY, &report->raw, EP_SIZE_EXTRAKEY);
}

void
usb_update_nkro(report_nkro_t *report)
{
    usb_ep_nkro_idle = 0;
    usb_write_packet(usbd_dev, EP_NKRO, &report->raw, EP_SIZE_NKRO);
}

static void
usb_set_config(usbd_device *dev, uint16_t wValue)
{
    (void)wValue;

    usbd_ep_setup(dev,
                  USB_ENDPOINT_ADDR_IN(EP_KEYBOARD),
                  USB_ENDPOINT_ATTR_INTERRUPT,
                  EP_SIZE_ALIGN(EP_SIZE_KEYBOARD),
                  usb_endpoint_idle);

    usbd_ep_setup(dev,
                  USB_ENDPOINT_ADDR_IN(EP_MOUSE),
                  USB_ENDPOINT_ATTR_INTERRUPT,
                  EP_SIZE_ALIGN(EP_SIZE_MOUSE),
                  usb_endpoint_idle);

    usbd_ep_setup(dev,
                  USB_ENDPOINT_ADDR_IN(EP_EXTRAKEY),
                  USB_ENDPOINT_ATTR_INTERRUPT,
                  EP_SIZE_ALIGN(EP_SIZE_EXTRAKEY),
                  usb_endpoint_idle);

    usbd_ep_setup(dev,
                  USB_ENDPOINT_ADDR_IN(EP_NKRO),
                  USB_ENDPOINT_ATTR_INTERRUPT,
                  EP_SIZE_ALIGN(EP_SIZE_NKRO),
                  usb_endpoint_idle);

    usbd_ep_setup(dev,
                  USB_ENDPOINT_ADDR_OUT(EP_SERIALDATAOUT),
                  USB_ENDPOINT_ATTR_BULK,
                  EP_SIZE_ALIGN(EP_SIZE_SERIALDATAOUT),
                  cdcacm_data_rx_cb);
    usbd_ep_setup(dev,
                  USB_ENDPOINT_ADDR_IN(EP_SERIALDATAIN),
                  USB_ENDPOINT_ATTR_BULK,
                  EP_SIZE_ALIGN(EP_SIZE_SERIALDATAIN),
                  usb_endpoint_idle);
    usbd_ep_setup(dev,
                  USB_ENDPOINT_ADDR_IN(EP_SERIALCOMM),
                  USB_ENDPOINT_ATTR_INTERRUPT,
                  EP_SIZE_ALIGN(EP_SIZE_SERIALCOMM),
                  NULL);

    usbd_register_control_callback(dev,
                                   USB_REQ_TYPE_INTERFACE,
                                   USB_REQ_TYPE_RECIPIENT,
                                   usb_control_request);
}

static void
usb_sof(void)
{
    usb_ms++;
}

uint32_t
usb_now(void)
{
    return usb_ms;
}

/* Buffer used for control requests. */
uint8_t usbd_control_buffer[256] __attribute__((aligned));

void
usb_init(void)
{
    usb_ms = 0;
    usb_ifs_enumerated = 0;
    usb_ep_serial_idle = 0;
    usb_ep_extrakey_idle = 1;
    usb_ep_keyboard_idle = 1;
    usb_ep_mouse_idle = 1;
    usb_ep_nkro_idle = 1;

    usbd_dev = usbd_init(&st_usbfs_v1_usb_driver,
                         &dev_descriptor,
                         &config,
                         usb_strings, STRI_MAX,
                         usbd_control_buffer, sizeof(usbd_control_buffer));

    usbd_register_set_config_callback(usbd_dev, usb_set_config);
    usbd_register_reset_callback(usbd_dev, usb_reset);
    usbd_register_resume_callback(usbd_dev, usb_resume);
    usbd_register_sof_callback(usbd_dev, usb_sof);
    usbd_register_suspend_callback(usbd_dev, usb_suspend);

    nvic_enable_irq(NVIC_USB_LP_CAN_RX0_IRQ);
    nvic_enable_irq(NVIC_USB_WAKEUP_IRQ);
}

void
usb_lp_can_rx0_isr(void)
{
    usbd_poll(usbd_dev);
}

void
usb_wakeup_isr(void)
{
    usbd_poll(usbd_dev);
}

void
usb_endpoint_idle(usbd_device *dev, uint8_t ep)
{
    (void)dev;

    switch (ep) {
        case EP_KEYBOARD:
            usb_ep_keyboard_idle = 1;
            break;

        case EP_MOUSE:
            usb_ep_mouse_idle = 1;
            break;

        case EP_NKRO:
            usb_ep_nkro_idle = 1;
            break;

        case EP_EXTRAKEY:
            usb_ep_extrakey_idle = 1;
            break;

        case EP_SERIALDATAIN:
            usb_ep_serial_idle = 1;
            break;
    }
    USB_CLR_EP_RX_CTR(ep);
}

void
cdcacm_data_rx_cb(usbd_device *dev, uint8_t ep)
{
    (void)ep;

    uint16_t len;
    char serialbuf[EP_SIZE_SERIALDATAOUT] __attribute__((aligned));
    uint16_t istr = *USB_ISTR_REG;

    /* Handle trouble like remote going away */
    if (istr & USB_ISTR_ERR) {
        USB_CLR_ISTR_ERR();
        return;
    }

    len = usbd_ep_read_packet(dev,
                              USB_ENDPOINT_ADDR_OUT(EP_SERIALDATAOUT),
                              serialbuf, EP_SIZE_SERIALDATAOUT);

    serial_in((uint8_t *)serialbuf, len);
}

void
cdcacm_data_wx(uint8_t *buf, uint16_t len)
{
    usb_ep_serial_idle = 0;
    usbd_ep_write_packet(usbd_dev, EP_SERIALDATAIN, buf, len);
}
