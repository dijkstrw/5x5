Mapping keys
============

Physical keys are mapped to usb scan codes. Usb scancodes are mapped to linux
scancodes. Scancodes are mapped to keycodes, which in turn are mapped to
keysyms. A keycode corresponds to a function, a keysym may change if a keyboard
modifier (i.e. shift is active)

Usb scancodes
-------------

Keyboards that are attached via usb hid can emit 255 different
scancodes. Inside the linux kernel, only about half of these usb scancodes are
mapped to linux scancodes (see linux/drivers/hid/usbhid/usbkbd.c). The unmapped
half are declared unknown, but can be enabled by first looking at their real
value using evtest:

      evtest
      Properties:
      Testing ... (interrupt to exit)
      Event: time 1451422547.619719, type 4 (EV_MSC), code 4 (MSC_SCAN), value 7008b
      Event: time 1451422547.619719, type 1 (EV_KEY), code 471 (KEY_FN_F6), value 1
      Event: time 1451422547.619719, -------------- SYN_REPORT ------------
      Event: time 1451422547.675713, type 4 (EV_MSC), code 4 (MSC_SCAN), value 7008b
      Event: time 1451422547.675713, type 1 (EV_KEY), code 471 (KEY_FN_F6), value 0
      Event: time 1451422547.675713, -------------- SYN_REPORT ------------

And then adjusting the udev hwdb /etc/udev/hwdb.d/70-keyboard.hwdb:

    evdev:input:b0003vDEADpBEEF*
     KEYBOARD_KEY_70086=fn_f1
     KEYBOARD_KEY_70087=fn_f2
     KEYBOARD_KEY_70088=fn_f3
     KEYBOARD_KEY_70089=fn_f4
     KEYBOARD_KEY_7008a=fn_f5
     KEYBOARD_KEY_7008b=fn_f6
     KEYBOARD_KEY_7008c=fn_f7
     KEYBOARD_KEY_7008d=fn_f8
     KEYBOARD_KEY_7008e=fn_f9
     KEYBOARD_KEY_7008f=fn_f10

Update the hwdb, and trigger a reload

    udevadm hwdb --update
    udevadm trigger /dev/input/event*

and after that the linux kernel will map the usb scancodes to linux scancodes.

Keycodes
--------

Linux scancode to keycode mapping for X happens through the mappings provided
by xkb. "xmodmap is a decade out of date" -- man that made me feel old. From
what I can find, these are still only max 255 keycodes possible, so mapping to
unused codes <255 seems "key".

This made me rethink the whole mapping approach described above. I would have
liked to define my own partitioned off set of keys, and not interfere with
other keys present. The last mapping step precludes this, so why go to the
trouble in the first place. I reset my initial keyboard layer to just use the
media keys, F13-F20 and the numeric keypad. None of these are present on my
normal tkl-keyboard, and all are present in the default usb-hid -> linux
scancodes -> X keycode -> X keysym mappings.
