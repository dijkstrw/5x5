/*
 * Copyright (c) 2021 by Willem Dijkstra <wpd@xs4all.nl>.
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
 * Map from ascii characters 0x20 - 0x7e to events
 */
#include <stddef.h>

#include "keymap.h"
#include "map_ascii.h"
#include "usb_keycode.h"

event_t translate_ascii[] =
{
    _K(SPACE),                  /* 0x20   */
    _KM(LSHIFT, 1),             /* 0x21 ! */
    _KM(LSHIFT, QUOTE),         /* 0x22 " */
    _KM(LSHIFT, 3),             /* 0x23 # */
    _KM(LSHIFT, 4),             /* 0x24 $ */
    _KM(LSHIFT, 5),             /* 0x25 % */
    _KM(LSHIFT, 7),             /* 0x26 & */
    _K(QUOTE),                  /* 0x27 ' */
    _KM(LSHIFT, 9),             /* 0x28 ( */
    _KM(LSHIFT, 0),             /* 0x29 ) */
    _KM(LSHIFT, 8),             /* 0x2a * */
    _KM(LSHIFT, EQUAL),         /* 0x2b + */
    _K(COMMA),                  /* 0x2c , */
    _K(MINUS),                  /* 0x2d - */
    _K(PERIOD),                 /* 0x2e . */
    _K(SLASH),                  /* 0x2f / */

    _K(0),                      /* 0x30 0 */
    _K(1),                      /* 0x32 1 */
    _K(2),                      /* 0x32 2 */
    _K(3),                      /* 0x33 3 */
    _K(4),                      /* 0x34 4 */
    _K(5),                      /* 0x35 5 */
    _K(6),                      /* 0x36 6 */
    _K(7),                      /* 0x37 7 */
    _K(8),                      /* 0x38 8 */
    _K(9),                      /* 0x39 9 */

    _KM(LSHIFT, SEMICOLON),     /* 0x3a : */
    _K(SEMICOLON),              /* 0x3b ; */
    _KM(LSHIFT, COMMA),         /* 0x3c < */
    _K(EQUAL),                  /* 0x3d = */
    _KM(LSHIFT, PERIOD),        /* 0x3e > */
    _KM(LSHIFT, SLASH),         /* 0x3f ? */
    _KM(LSHIFT, 2),             /* 0x40 @ */

    _KM(LSHIFT, A),             /* 0x41 A */
    _KM(LSHIFT, B),             /* 0x42 B */
    _KM(LSHIFT, C),             /* 0x43 C */
    _KM(LSHIFT, D),             /* 0x44 D */
    _KM(LSHIFT, E),             /* 0x45 E */
    _KM(LSHIFT, F),             /* 0x46 F */
    _KM(LSHIFT, G),             /* 0x47 G */
    _KM(LSHIFT, H),             /* 0x48 H */
    _KM(LSHIFT, I),             /* 0x49 I */
    _KM(LSHIFT, J),             /* 0x4a J */
    _KM(LSHIFT, K),             /* 0x4b K */
    _KM(LSHIFT, L),             /* 0x4c L */
    _KM(LSHIFT, M),             /* 0x4d M */
    _KM(LSHIFT, N),             /* 0x4e N */
    _KM(LSHIFT, O),             /* 0x4f O */
    _KM(LSHIFT, P),             /* 0x50 P */
    _KM(LSHIFT, Q),             /* 0x51 Q */
    _KM(LSHIFT, R),             /* 0x52 R */
    _KM(LSHIFT, S),             /* 0x53 S */
    _KM(LSHIFT, T),             /* 0x54 T */
    _KM(LSHIFT, U),             /* 0x55 U */
    _KM(LSHIFT, V),             /* 0x56 V */
    _KM(LSHIFT, W),             /* 0x57 W */
    _KM(LSHIFT, X),             /* 0x58 X */
    _KM(LSHIFT, Y),             /* 0x59 Y */
    _KM(LSHIFT, Z),             /* 0x5a Z */

    _K(LEFTBRACE),              /* 0x5b [ */
    _K(BACKSLASH),              /* 0x5c \ */
    _K(RIGHTBRACE),             /* 0x5d ] */
    _KM(LSHIFT, 6),             /* 0x5e ^ */
    _KM(LSHIFT, MINUS),         /* 0x5f _ */
    _K(BACKTICK),               /* 0x60 ` */

    _K(A),                      /* 0x61 a */
    _K(B),                      /* 0x62 b */
    _K(C),                      /* 0x63 c */
    _K(D),                      /* 0x64 d */
    _K(E),                      /* 0x65 e */
    _K(F),                      /* 0x66 f */
    _K(G),                      /* 0x67 g */
    _K(H),                      /* 0x68 h */
    _K(I),                      /* 0x69 i */
    _K(J),                      /* 0x6a j */
    _K(K),                      /* 0x6b k */
    _K(L),                      /* 0x6c l */
    _K(M),                      /* 0x6d m */
    _K(N),                      /* 0x6e n */
    _K(O),                      /* 0x6f o */
    _K(P),                      /* 0x70 p */
    _K(Q),                      /* 0x71 q */
    _K(R),                      /* 0x72 r */
    _K(S),                      /* 0x73 s */
    _K(T),                      /* 0x74 t */
    _K(U),                      /* 0x75 u */
    _K(V),                      /* 0x76 v */
    _K(W),                      /* 0x77 w */
    _K(X),                      /* 0x78 x */
    _K(Y),                      /* 0x79 y */
    _K(Z),                      /* 0x7a z */

    _KM(LSHIFT, LEFTBRACE),     /* 0x7b { */
    _KM(LSHIFT, BACKSLASH),     /* 0x7c | */
    _KM(LSHIFT, RIGHTBRACE),    /* 0x7d } */
    _KM(LSHIFT, BACKTICK),      /* 0x7e ~ */

};

event_t
*map_ascii_to_event(uint8_t c)
{
    uint8_t offset = c - 0x20;

    if ((c >= 0x20) &&
        (c <= 0x7e)) {
        return &translate_ascii[offset];
    }

    return NULL;
}
