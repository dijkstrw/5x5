Interesting
===========

While working on a project I collect my own questions and they answers I
found + add in interesting gotchas encountered along the way.

* There are a lot of parts to an usb descriptor. How are they tied together?

    Device Desc -> Configuration Desc -> Interface Desc -> Endpoint Desc / HID Desc
    if HID Desc -> Report Desc / Physical Desc

    (p 16, USB HID 1.11)

* Wat is the purpose of SOF?

    Start of Frame, issued every millisecond by the host. The SOF interrupt can
    be used to drive routines that need to work timeouts.

* How many different usb devices can the stm32f1 pretend to be?

    One usb device, but with 8 endpoints. I'm using 7 so far for keyboard,
    mouse, nkro, extra keys, cdc comm, cdc in and cdc out.

* What is needed in a usb boot keyboard descriptor?

    HID Report parsing is large, so for bios boot support a special HID
    subclass is supported that identifies a predefined protocol for
    mouse/keyboards. The boot protocol can be extended to include additional
    data not recognized by the BIOS, or the device can support a second
    preferred protocol for use by the HID driver.

    bInterfaceSubClass = 0 /* No Subclass */
    bInterfaceSubClass = 1 /* Boot interface subclass */

* STM32F1 errata: Some Cortex-M3 have a silicon error, although mine is reputed
  to not have it, enable -mfix-cortex-m3-ldrd

* STM32F1 errata: USB operation may have a bug if APB1 clock is lower than
  13MHz

* Never send a different amount of bytes than the amount promised in your
  descriptor. Deviation = linux driver error, that prompts the stm to resend,
  ad nauseam.

Debugging USB
-------------

Being able to debug usb turned out to be essential. wireshark can capture usb
packets and show their disassembly. Faulty packets are not always displayed
(for instance if in the case of physical packet errors), but still provide a
smoking gun for fault analysis.

        modprobe usbmon
        wireshark

If the initial usb negotiation fails, the kernel will ignore the attached
device. Reconnecting the usb plug will restart the negotiation, but is not
always convenient when you have a debugger on the usb keyboard waiting for an
interesting breakpoint. There is a utility in util/usbreset that will reset the
usb bus and restart negotiation without having to replug the connector.
