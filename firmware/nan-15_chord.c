/*
Copyright 2013,2016 Jun Wako <wakojun@gmail.com>
Copyright 2017 Bert Burgemeister <trebbu@googlemail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "actionmap.h"
#include "action_layer.h"
#include "action_util.h"
#include "debug.h"
#include "host.h"
#include "led.h"
#include "timer.h"
#include "wait.h"
#include <avr/eeprom.h>
#include <stdio.h>


/*  NaN-15 raw actionmap and LED definition
 *
 *  ,-------------------------------------------.
 *  |          |        LED5         |          |
 *  |  finger  |  finger  |  finger  |  finger  |
 *  |   1,3    |   1,2  LED6  1,1    |   1,0    |
 *  |          |          |          |          |
 *  |          |        LED7         |          |
 *  |LED9------+----------+----------+------LED4|
 *  |          |          |          |          |
 *  |  finger  |  finger  |  finger  |  finger  |
 *  |   2,3    |   2,2    |   2,1    |   2,0    |
 *  |          |          |          |          |
 *  |          |          |          |          |
 *  |LED12-----+--------LED8---------+------LED3|
 *  |          |          |          |          |
 *  |  finger  |  finger  |  finger  |  finger  |
 *  |   3,3    |   3,2    |   3,1    |   3,0    |
 *  |          |          |          |          |
 *  |          |          |          |          |
 *  |LED11-----+---------------------+------LED2|
 *  |          |                     |          |
 *  |  thumb   |        thumb        |  thumb   |
 *  |   4,2    |         4,1         |   4,0    |
 *  |          |                     |          |
 *  |        LED0                  LED1         |
 *  `-------------------------------------------'
 */


/*************************************************************
 * Define individual keys
 *************************************************************/

typedef union {
    uint8_t raw;
    struct {
        uint8_t row :3;
        uint8_t col :5;
    } key;
} keycoord_t;

enum func_id {
    RESET,
    FNG_CHRD,
    THB_CHRD,
    CHG_LAYER,
    SWAP_CHRDS,
    MCR_RECORD,
    MCR_PLAY,
    PRINT,
};

/* action_function() dispatches on AF()'s and PF()'s func_id */
#define AF(param, func_id) ACTION_FUNCTION_OPT((param), (func_id))
#define PF(row, col, func_id) AF((row) | (col)<<3, (func_id))

enum layer {L_DFLT = 0, L_NUM, L_NAV, L_MSE, L_MCR,};

static const action_t actionmaps[][MATRIX_ROWS][MATRIX_COLS] PROGMEM = {
    [L_DFLT] = {                /* keychord keys */
        {PF(1, 3, FNG_CHRD), PF(1, 2, FNG_CHRD), PF(1, 1, FNG_CHRD), PF(1, 0, FNG_CHRD),},
        {PF(2, 3, FNG_CHRD), PF(2, 2, FNG_CHRD), PF(2, 1, FNG_CHRD), PF(2, 0, FNG_CHRD),},
        {PF(3, 3, FNG_CHRD), PF(3, 2, FNG_CHRD), PF(3, 1, FNG_CHRD), PF(3, 0, FNG_CHRD),},
        {PF(4, 2, THB_CHRD), AC_NO,              PF(4, 1, THB_CHRD), PF(4, 0, THB_CHRD),},
    },
    [L_NUM] = {                 /* numpad */
        {AC_P7,                 AC_P8, AC_P9, AC_BSPC,},
        {AC_P4,                 AC_P5, AC_P6, AC_PDOT,},
        {AC_P1,                 AC_P2, AC_P3, AC_SPC, },
        {AF(L_DFLT, CHG_LAYER), AC_NO, AC_P0, AC_ENT, },
    },
    [L_NAV] = {                 /* nav */
        {AC_HOME,               AC_UP,   AC_PGUP,   AC_BSPC,},
        {AC_LEFT,               AC_NO,   AC_RIGHT,  AC_INS, },
        {AC_END,                AC_DOWN, AC_PGDOWN, AC_DEL, },
        {AF(L_DFLT, CHG_LAYER), AC_NO,   AC_P0,     AC_ENT, },
    },
    [L_MSE] = {                 /* mouse */
        {AC_MS_WH_LEFT,         AC_MS_UP,   AC_MS_WH_RIGHT, AC_MS_WH_UP,  },
        {AC_MS_LEFT,            AC_MS_BTN1, AC_MS_RIGHT,    AC_MS_WH_DOWN,},
        {AC_MS_BTN2,            AC_MS_DOWN, AC_MS_BTN3,     AC_MS_BTN4,   },
        {AF(L_DFLT, CHG_LAYER), AC_NO,      AC_MS_BTN1,     AC_MS_BTN5,   },
    },
    [L_MCR] = {                 /* macro pad */
        {AF(KC_FN0, MCR_PLAY),  AF(KC_FN1, MCR_PLAY), AF(KC_FN2, MCR_PLAY), AF(KC_FN3, MCR_PLAY),},
        {AF(KC_FN4, MCR_PLAY),  AF(KC_FN5, MCR_PLAY), AF(KC_FN6, MCR_PLAY), AF(KC_FN7, MCR_PLAY),},
        {AF(L_NUM, CHG_LAYER),  AF(L_NAV, CHG_LAYER), AF(L_MSE, CHG_LAYER), AC_SPC,              },
        {AF(L_DFLT, CHG_LAYER), AC_NO,                AC_P0,                AC_ENT,              },
    },
};

#define MCR_LEN 6             /* chords per macro */
#define MCR_MAX 8             /* number of macros */

action_t
action_for_key(uint8_t layer, keypos_t key)
{
    return (action_t)pgm_read_word(&actionmaps[layer][key.row][key.col]);
}


/*************************************************************
 * Define keychords
 *************************************************************/

/*
 * Two keycode/modifier sets, 12 bits each.  The keyboard's
 * upper/lower-level state (which is independent of the mods stored
 * here) determines the actual pair member to be reported.  A struct {
 * struct ... struct ... } (containing 2 12-bit structs) would be a
 * better representation but doesn't seem to pack properly.
 */
typedef struct {
    uint8_t code_lo :8;
    uint8_t mods_lo :4;
    uint8_t code_up :8;
    uint8_t mods_up :4;
} keypair_t;

/* keypair_t mods:  3210
 *   bit 0          |||+- Left Control
 *   bit 1          ||+-- Left Shift
 *   bit 2          |+--- Left Alt
 *   bit 3          +---- Right Alt (not Left GUI!)
 */
enum keypair_mod {
    No = 0x00,                /* no mod */
    Co = 0x01,                /* Left Control */
    Sh = 0x02,                /* Left Shift */
    Al = 0x04,                /* Left Alt */
    Ag = 0x08,                /* AltGr/Right Alt */
};

/* AltGr in keypair mods becomes Alt in mods */
#define KEYPAIR_MODS_TO_MODS(KEYPAIR_MODS) \
    (((KEYPAIR_MODS) & ~Ag) | ((KEYPAIR_MODS) & Ag)<<3)

#define MODS_TO_KEYPAIR_MODS(MODS) \
    (((MODS) & ~(Ag<<4))>>4 | ((MODS) & Al<<4)>>3 | ((MODS) & 0x0f))

/*
 * Keychords comprising one key per column on the upper three rows
 */
#define CHRD(COL0, COL1, COL2, COL3) ((COL0)<<6 | (COL1)<<4 | (COL2)<<2 | (COL3))
#define KEYPAIR(LOWER_MODS, LOWER_CODE, UPPER_MODS, UPPER_CODE) \
    {.mods_lo = (LOWER_MODS), .code_lo = (KC_##LOWER_CODE), \
            .mods_up = (UPPER_MODS), .code_up = (KC_##UPPER_CODE)}

static keypair_t chrdmap[256] EEMEM = {
    [CHRD(0, 0, 0, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHRD(0, 0, 0, 1)] = KEYPAIR(No, A,              Sh, A             ),
    [CHRD(0, 0, 0, 2)] = KEYPAIR(No, B,              Sh, B             ),
    [CHRD(0, 0, 0, 3)] = KEYPAIR(No, C,              Sh, C             ),
    [CHRD(0, 0, 1, 0)] = KEYPAIR(No, D,              Sh, D             ),
    [CHRD(0, 0, 1, 1)] = KEYPAIR(No, E,              Sh, E             ),
    [CHRD(0, 0, 1, 2)] = KEYPAIR(No, F,              Sh, F             ),
    [CHRD(0, 0, 1, 3)] = KEYPAIR(No, G,              Sh, G             ),
    [CHRD(0, 0, 2, 0)] = KEYPAIR(No, H,              Sh, H             ),
    [CHRD(0, 0, 2, 1)] = KEYPAIR(No, I,              Sh, I             ),
    [CHRD(0, 0, 2, 2)] = KEYPAIR(No, J,              Sh, J             ),
    [CHRD(0, 0, 2, 3)] = KEYPAIR(No, K,              Sh, K             ),
    [CHRD(0, 0, 3, 0)] = KEYPAIR(No, L,              Sh, L             ),
    [CHRD(0, 0, 3, 1)] = KEYPAIR(No, M,              Sh, M             ),
    [CHRD(0, 0, 3, 2)] = KEYPAIR(No, N,              Sh, N             ),
    [CHRD(0, 0, 3, 3)] = KEYPAIR(No, O,              Sh, O             ),
    [CHRD(0, 1, 0, 0)] = KEYPAIR(No, P,              Sh, P             ),
    [CHRD(0, 1, 0, 1)] = KEYPAIR(No, Q,              Sh, Q             ),
    [CHRD(0, 1, 0, 2)] = KEYPAIR(No, R,              Sh, R             ),
    [CHRD(0, 1, 0, 3)] = KEYPAIR(No, S,              Sh, S             ),
    [CHRD(0, 1, 1, 0)] = KEYPAIR(No, T,              Sh, T             ),
    [CHRD(0, 1, 1, 1)] = KEYPAIR(No, U,              Sh, U             ),
    [CHRD(0, 1, 1, 2)] = KEYPAIR(No, V,              Sh, V             ),
    [CHRD(0, 1, 1, 3)] = KEYPAIR(No, W,              Sh, W             ),
    [CHRD(0, 1, 2, 0)] = KEYPAIR(No, X,              Sh, X             ),
    [CHRD(0, 1, 2, 1)] = KEYPAIR(No, Y,              Sh, Y             ),
    [CHRD(0, 1, 2, 2)] = KEYPAIR(No, Z,              Sh, Z             ),
    [CHRD(0, 1, 2, 3)] = KEYPAIR(No, 1,              Sh, 1             ),
    [CHRD(0, 1, 3, 0)] = KEYPAIR(No, 2,              Sh, 2             ),
    [CHRD(0, 1, 3, 1)] = KEYPAIR(No, 3,              Sh, 3             ),
    [CHRD(0, 1, 3, 2)] = KEYPAIR(No, 4,              Sh, 4             ),
    [CHRD(0, 1, 3, 3)] = KEYPAIR(No, 5,              Sh, 5             ),
    [CHRD(0, 2, 0, 0)] = KEYPAIR(No, 6,              Sh, 6             ),
    [CHRD(0, 2, 0, 1)] = KEYPAIR(No, 7,              Sh, 7             ),
    [CHRD(0, 2, 0, 2)] = KEYPAIR(No, 8,              Sh, 8             ),
    [CHRD(0, 2, 0, 3)] = KEYPAIR(No, 9,              Sh, 9             ),
    [CHRD(0, 2, 1, 0)] = KEYPAIR(No, 0,              Sh, 0             ),
    [CHRD(0, 2, 1, 1)] = KEYPAIR(No, ENTER,          Sh, ENTER         ),
    [CHRD(0, 2, 1, 2)] = KEYPAIR(No, ESCAPE,         Sh, ESCAPE        ),
    [CHRD(0, 2, 1, 3)] = KEYPAIR(No, BSPACE,         Sh, BSPACE        ),
    [CHRD(0, 2, 2, 0)] = KEYPAIR(No, TAB,            Sh, TAB           ),
    [CHRD(0, 2, 2, 1)] = KEYPAIR(No, SPACE,          Sh, SPACE         ),
    [CHRD(0, 2, 2, 2)] = KEYPAIR(No, MINUS,          Sh, MINUS         ),
    [CHRD(0, 2, 2, 3)] = KEYPAIR(No, EQUAL,          Sh, EQUAL         ),
    [CHRD(0, 2, 3, 0)] = KEYPAIR(No, LBRACKET,       Sh, LBRACKET      ),
    [CHRD(0, 2, 3, 1)] = KEYPAIR(No, RBRACKET,       Sh, RBRACKET      ),
    [CHRD(0, 2, 3, 2)] = KEYPAIR(No, BSLASH,         Sh, BSLASH        ),
    [CHRD(0, 2, 3, 3)] = KEYPAIR(No, NONUS_HASH,     Sh, NONUS_HASH    ),
    [CHRD(0, 3, 0, 0)] = KEYPAIR(No, SCOLON,         Sh, SCOLON        ),
    [CHRD(0, 3, 0, 1)] = KEYPAIR(No, QUOTE,          Sh, QUOTE         ),
    [CHRD(0, 3, 0, 2)] = KEYPAIR(No, GRAVE,          Sh, GRAVE         ),
    [CHRD(0, 3, 0, 3)] = KEYPAIR(No, COMMA,          Sh, COMMA         ),
    [CHRD(0, 3, 1, 0)] = KEYPAIR(No, DOT,            Sh, DOT           ),
    [CHRD(0, 3, 1, 1)] = KEYPAIR(No, SLASH,          Sh, SLASH         ),
    [CHRD(0, 3, 1, 2)] = KEYPAIR(No, CAPSLOCK,       Sh, CAPSLOCK      ),
    [CHRD(0, 3, 1, 3)] = KEYPAIR(No, F1,             Sh, F1            ),
    [CHRD(0, 3, 2, 0)] = KEYPAIR(No, F2,             Sh, F2            ),
    [CHRD(0, 3, 2, 1)] = KEYPAIR(No, F3,             Sh, F3            ),
    [CHRD(0, 3, 2, 2)] = KEYPAIR(No, F4,             Sh, F4            ),
    [CHRD(0, 3, 2, 3)] = KEYPAIR(No, F5,             Sh, F5            ),
    [CHRD(0, 3, 3, 0)] = KEYPAIR(No, F6,             Sh, F6            ),
    [CHRD(0, 3, 3, 1)] = KEYPAIR(No, F7,             Sh, F7            ),
    [CHRD(0, 3, 3, 2)] = KEYPAIR(No, F8,             Sh, F8            ),
    [CHRD(0, 3, 3, 3)] = KEYPAIR(No, F9,             Sh, F9            ),
    [CHRD(1, 0, 0, 0)] = KEYPAIR(No, F10,            Sh, F10           ),
    [CHRD(1, 0, 0, 1)] = KEYPAIR(No, F11,            Sh, F11           ),
    [CHRD(1, 0, 0, 2)] = KEYPAIR(No, F12,            Sh, F12           ),
    [CHRD(1, 0, 0, 3)] = KEYPAIR(No, PSCREEN,        Sh, PSCREEN       ),
    [CHRD(1, 0, 1, 0)] = KEYPAIR(No, SCROLLLOCK,     Sh, SCROLLLOCK    ),
    [CHRD(1, 0, 1, 1)] = KEYPAIR(No, PAUSE,          Sh, PAUSE         ),
    [CHRD(1, 0, 1, 2)] = KEYPAIR(No, INSERT,         Sh, INSERT        ),
    [CHRD(1, 0, 1, 3)] = KEYPAIR(No, HOME,           Sh, HOME          ),
    [CHRD(1, 0, 2, 0)] = KEYPAIR(No, PGUP,           Sh, PGUP          ),
    [CHRD(1, 0, 2, 1)] = KEYPAIR(No, DELETE,         Sh, DELETE        ),
    [CHRD(1, 0, 2, 2)] = KEYPAIR(No, END,            Sh, END           ),
    [CHRD(1, 0, 2, 3)] = KEYPAIR(No, PGDOWN,         Sh, PGDOWN        ),
    [CHRD(1, 0, 3, 0)] = KEYPAIR(No, RIGHT,          Sh, RIGHT         ),
    [CHRD(1, 0, 3, 1)] = KEYPAIR(No, LEFT,           Sh, LEFT          ),
    [CHRD(1, 0, 3, 2)] = KEYPAIR(No, DOWN,           Sh, DOWN          ),
    [CHRD(1, 0, 3, 3)] = KEYPAIR(No, UP,             Sh, UP            ),
    [CHRD(1, 1, 0, 0)] = KEYPAIR(No, NUMLOCK,        Sh, NUMLOCK       ),
    [CHRD(1, 1, 0, 1)] = KEYPAIR(No, KP_SLASH,       Sh, KP_SLASH      ),
    [CHRD(1, 1, 0, 2)] = KEYPAIR(No, KP_ASTERISK,    Sh, KP_ASTERISK   ),
    [CHRD(1, 1, 0, 3)] = KEYPAIR(No, KP_MINUS,       Sh, KP_MINUS      ),
    [CHRD(1, 1, 1, 0)] = KEYPAIR(No, KP_PLUS,        Sh, KP_PLUS       ),
    [CHRD(1, 1, 1, 1)] = KEYPAIR(No, KP_ENTER,       Sh, KP_ENTER      ),
    [CHRD(1, 1, 1, 2)] = KEYPAIR(No, KP_1,           Sh, KP_1          ),
    [CHRD(1, 1, 1, 3)] = KEYPAIR(No, KP_2,           Sh, KP_2          ),
    [CHRD(1, 1, 2, 0)] = KEYPAIR(No, KP_3,           Sh, KP_3          ),
    [CHRD(1, 1, 2, 1)] = KEYPAIR(No, KP_4,           Sh, KP_4          ),
    [CHRD(1, 1, 2, 2)] = KEYPAIR(No, KP_5,           Sh, KP_5          ),
    [CHRD(1, 1, 2, 3)] = KEYPAIR(No, KP_6,           Sh, KP_6          ),
    [CHRD(1, 1, 3, 0)] = KEYPAIR(No, KP_7,           Sh, KP_7          ),
    [CHRD(1, 1, 3, 1)] = KEYPAIR(No, KP_8,           Sh, KP_8          ),
    [CHRD(1, 1, 3, 2)] = KEYPAIR(No, KP_9,           Sh, KP_9          ),
    [CHRD(1, 1, 3, 3)] = KEYPAIR(No, KP_0,           Sh, KP_0          ),
    [CHRD(1, 2, 0, 0)] = KEYPAIR(No, KP_DOT,         Sh, KP_DOT        ),
    [CHRD(1, 2, 0, 1)] = KEYPAIR(No, NONUS_BSLASH,   Sh, NONUS_BSLASH  ),
    [CHRD(1, 2, 0, 2)] = KEYPAIR(No, APPLICATION,    Sh, APPLICATION   ),
    [CHRD(1, 2, 0, 3)] = KEYPAIR(No, POWER,          Sh, POWER         ),
    [CHRD(1, 2, 1, 0)] = KEYPAIR(No, KP_EQUAL,       Sh, KP_EQUAL      ),
    [CHRD(1, 2, 1, 1)] = KEYPAIR(No, F13,            Sh, F13           ),
    [CHRD(1, 2, 1, 2)] = KEYPAIR(No, F14,            Sh, F14           ),
    [CHRD(1, 2, 1, 3)] = KEYPAIR(No, F15,            Sh, F15           ),
    [CHRD(1, 2, 2, 0)] = KEYPAIR(No, F16,            Sh, F16           ),
    [CHRD(1, 2, 2, 1)] = KEYPAIR(No, F17,            Sh, F17           ),
    [CHRD(1, 2, 2, 2)] = KEYPAIR(No, F18,            Sh, F18           ),
    [CHRD(1, 2, 2, 3)] = KEYPAIR(No, F19,            Sh, F19           ),
    [CHRD(1, 2, 3, 0)] = KEYPAIR(No, F20,            Sh, F20           ),
    [CHRD(1, 2, 3, 1)] = KEYPAIR(No, F21,            Sh, F21           ),
    [CHRD(1, 2, 3, 2)] = KEYPAIR(No, F22,            Sh, F22           ),
    [CHRD(1, 2, 3, 3)] = KEYPAIR(No, F23,            Sh, F23           ),
    [CHRD(1, 3, 0, 0)] = KEYPAIR(No, F24,            Sh, F24           ),
    [CHRD(1, 3, 0, 1)] = KEYPAIR(No, EXECUTE,        Sh, EXECUTE       ),
    [CHRD(1, 3, 0, 2)] = KEYPAIR(No, HELP,           Sh, HELP          ),
    [CHRD(1, 3, 0, 3)] = KEYPAIR(No, MENU,           Sh, MENU          ),
    [CHRD(1, 3, 1, 0)] = KEYPAIR(No, SELECT,         Sh, SELECT        ),
    [CHRD(1, 3, 1, 1)] = KEYPAIR(No, STOP,           Sh, STOP          ),
    [CHRD(1, 3, 1, 2)] = KEYPAIR(No, AGAIN,          Sh, AGAIN         ),
    [CHRD(1, 3, 1, 3)] = KEYPAIR(No, UNDO,           Sh, UNDO          ),
    [CHRD(1, 3, 2, 0)] = KEYPAIR(No, CUT,            Sh, CUT           ),
    [CHRD(1, 3, 2, 1)] = KEYPAIR(No, COPY,           Sh, COPY          ),
    [CHRD(1, 3, 2, 2)] = KEYPAIR(No, PASTE,          Sh, PASTE         ),
    [CHRD(1, 3, 2, 3)] = KEYPAIR(No, FIND,           Sh, FIND          ),
    [CHRD(1, 3, 3, 0)] = KEYPAIR(No, _MUTE,          Sh, _MUTE         ),
    [CHRD(1, 3, 3, 1)] = KEYPAIR(No, _VOLUP,         Sh, _VOLUP        ),
    [CHRD(1, 3, 3, 2)] = KEYPAIR(No, _VOLDOWN,       Sh, _VOLDOWN      ),
    [CHRD(1, 3, 3, 3)] = KEYPAIR(No, LOCKING_CAPS,   Sh, LOCKING_CAPS  ),
    [CHRD(2, 0, 0, 0)] = KEYPAIR(No, LOCKING_NUM,    Sh, LOCKING_NUM   ),
    [CHRD(2, 0, 0, 1)] = KEYPAIR(No, LOCKING_SCROLL, Sh, LOCKING_SCROLL),
    [CHRD(2, 0, 0, 2)] = KEYPAIR(No, KP_COMMA,       Sh, KP_COMMA      ),
    [CHRD(2, 0, 0, 3)] = KEYPAIR(No, KP_EQUAL_AS400, Sh, KP_EQUAL_AS400),
    [CHRD(2, 0, 1, 0)] = KEYPAIR(No, INT1,           Sh, INT1          ),
    [CHRD(2, 0, 1, 1)] = KEYPAIR(No, INT2,           Sh, INT2          ),
    [CHRD(2, 0, 1, 2)] = KEYPAIR(No, INT3,           Sh, INT3          ),
    [CHRD(2, 0, 1, 3)] = KEYPAIR(No, INT4,           Sh, INT4          ),
    [CHRD(2, 0, 2, 0)] = KEYPAIR(No, INT5,           Sh, INT5          ),
    [CHRD(2, 0, 2, 1)] = KEYPAIR(No, INT6,           Sh, INT6          ),
    [CHRD(2, 0, 2, 2)] = KEYPAIR(No, INT7,           Sh, INT7          ),
    [CHRD(2, 0, 2, 3)] = KEYPAIR(No, INT8,           Sh, INT8          ),
    [CHRD(2, 0, 3, 0)] = KEYPAIR(No, INT9,           Sh, INT9          ),
    [CHRD(2, 0, 3, 1)] = KEYPAIR(No, LANG1,          Sh, LANG1         ),
    [CHRD(2, 0, 3, 2)] = KEYPAIR(No, LANG2,          Sh, LANG2         ),
    [CHRD(2, 0, 3, 3)] = KEYPAIR(No, LANG3,          Sh, LANG3         ),
    [CHRD(2, 1, 0, 0)] = KEYPAIR(No, LANG4,          Sh, LANG4         ),
    [CHRD(2, 1, 0, 1)] = KEYPAIR(No, LANG5,          Sh, LANG5         ),
    [CHRD(2, 1, 0, 2)] = KEYPAIR(No, LANG6,          Sh, LANG6         ),
    [CHRD(2, 1, 0, 3)] = KEYPAIR(No, LANG7,          Sh, LANG7         ),
    [CHRD(2, 1, 1, 0)] = KEYPAIR(No, LANG8,          Sh, LANG8         ),
    [CHRD(2, 1, 1, 1)] = KEYPAIR(No, LANG9,          Sh, LANG9         ),
    [CHRD(2, 1, 1, 2)] = KEYPAIR(No, ALT_ERASE,      Sh, ALT_ERASE     ),
    [CHRD(2, 1, 1, 3)] = KEYPAIR(No, SYSREQ,         Sh, SYSREQ        ),
    [CHRD(2, 1, 2, 0)] = KEYPAIR(No, CANCEL,         Sh, CANCEL        ),
    [CHRD(2, 1, 2, 1)] = KEYPAIR(No, CLEAR,          Sh, CLEAR         ),
    [CHRD(2, 1, 2, 2)] = KEYPAIR(No, PRIOR,          Sh, PRIOR         ),
    [CHRD(2, 1, 2, 3)] = KEYPAIR(No, RETURN,         Sh, RETURN        ),
    [CHRD(2, 1, 3, 0)] = KEYPAIR(No, SEPARATOR,      Sh, SEPARATOR     ),
    [CHRD(2, 1, 3, 1)] = KEYPAIR(No, OUT,            Sh, OUT           ),
    [CHRD(2, 1, 3, 2)] = KEYPAIR(No, OPER,           Sh, OPER          ),
    [CHRD(2, 1, 3, 3)] = KEYPAIR(No, CLEAR_AGAIN,    Sh, CLEAR_AGAIN   ),
    [CHRD(2, 2, 0, 0)] = KEYPAIR(No, CRSEL,          Sh, CRSEL         ),
    [CHRD(2, 2, 0, 1)] = KEYPAIR(No, EXSEL,          Sh, EXSEL         ),
    [CHRD(2, 2, 0, 2)] = KEYPAIR(Ag, A,           Ag|Sh, A             ),
    [CHRD(2, 2, 0, 3)] = KEYPAIR(Ag, B,           Ag|Sh, B             ),
    [CHRD(2, 2, 1, 0)] = KEYPAIR(Ag, C,           Ag|Sh, C             ),
    [CHRD(2, 2, 1, 1)] = KEYPAIR(Ag, D,           Ag|Sh, D             ),
    [CHRD(2, 2, 1, 2)] = KEYPAIR(Ag, E,           Ag|Sh, E             ),
    [CHRD(2, 2, 1, 3)] = KEYPAIR(Ag, F,           Ag|Sh, F             ),
    [CHRD(2, 2, 2, 0)] = KEYPAIR(Ag, G,           Ag|Sh, G             ),
    [CHRD(2, 2, 2, 1)] = KEYPAIR(Ag, H,           Ag|Sh, H             ),
    [CHRD(2, 2, 2, 2)] = KEYPAIR(Ag, I,           Ag|Sh, I             ),
    [CHRD(2, 2, 2, 3)] = KEYPAIR(Ag, J,           Ag|Sh, J             ),
    [CHRD(2, 2, 3, 0)] = KEYPAIR(Ag, K,           Ag|Sh, K             ),
    [CHRD(2, 2, 3, 1)] = KEYPAIR(Ag, L,           Ag|Sh, L             ),
    [CHRD(2, 2, 3, 2)] = KEYPAIR(Ag, M,           Ag|Sh, M             ),
    [CHRD(2, 2, 3, 3)] = KEYPAIR(Ag, N,           Ag|Sh, N             ),
    [CHRD(2, 3, 0, 0)] = KEYPAIR(Ag, O,           Ag|Sh, O             ),
    [CHRD(2, 3, 0, 1)] = KEYPAIR(Ag, P,           Ag|Sh, P             ),
    [CHRD(2, 3, 0, 2)] = KEYPAIR(Ag, Q,           Ag|Sh, Q             ),
    [CHRD(2, 3, 0, 3)] = KEYPAIR(Ag, R,           Ag|Sh, R             ),
    [CHRD(2, 3, 1, 0)] = KEYPAIR(Ag, S,           Ag|Sh, S             ),
    [CHRD(2, 3, 1, 1)] = KEYPAIR(Ag, T,           Ag|Sh, T             ),
    [CHRD(2, 3, 1, 2)] = KEYPAIR(Ag, U,           Ag|Sh, U             ),
    [CHRD(2, 3, 1, 3)] = KEYPAIR(Ag, V,           Ag|Sh, V             ),
    [CHRD(2, 3, 2, 0)] = KEYPAIR(Ag, W,           Ag|Sh, W             ),
    [CHRD(2, 3, 2, 1)] = KEYPAIR(Ag, X,           Ag|Sh, X             ),
    [CHRD(2, 3, 2, 2)] = KEYPAIR(Ag, Y,           Ag|Sh, Y             ),
    [CHRD(2, 3, 2, 3)] = KEYPAIR(Ag, Z,           Ag|Sh, Z             ),
    [CHRD(2, 3, 3, 0)] = KEYPAIR(Ag, 1,           Ag|Sh, 1             ),
    [CHRD(2, 3, 3, 1)] = KEYPAIR(Ag, 2,           Ag|Sh, 2             ),
    [CHRD(2, 3, 3, 2)] = KEYPAIR(Ag, 3,           Ag|Sh, 3             ),
    [CHRD(2, 3, 3, 3)] = KEYPAIR(Ag, 4,           Ag|Sh, 4             ),
    [CHRD(3, 0, 0, 0)] = KEYPAIR(Ag, 5,           Ag|Sh, 5             ),
    [CHRD(3, 0, 0, 1)] = KEYPAIR(Ag, 6,           Ag|Sh, 6             ),
    [CHRD(3, 0, 0, 2)] = KEYPAIR(Ag, 7,           Ag|Sh, 7             ),
    [CHRD(3, 0, 0, 3)] = KEYPAIR(Ag, 8,           Ag|Sh, 8             ),
    [CHRD(3, 0, 1, 0)] = KEYPAIR(Ag, 9,           Ag|Sh, 9             ),
    [CHRD(3, 0, 1, 1)] = KEYPAIR(Ag, 0,           Ag|Sh, 0             ),
    [CHRD(3, 0, 1, 2)] = KEYPAIR(Ag, ENTER,       Ag|Sh, ENTER         ),
    [CHRD(3, 0, 1, 3)] = KEYPAIR(Ag, ESCAPE,      Ag|Sh, ESCAPE        ),
    [CHRD(3, 0, 2, 0)] = KEYPAIR(Ag, BSPACE,      Ag|Sh, BSPACE        ),
    [CHRD(3, 0, 2, 1)] = KEYPAIR(Ag, TAB,         Ag|Sh, TAB           ),
    [CHRD(3, 0, 2, 2)] = KEYPAIR(Ag, SPACE,       Ag|Sh, SPACE         ),
    [CHRD(3, 0, 2, 3)] = KEYPAIR(Ag, MINUS,       Ag|Sh, MINUS         ),
    [CHRD(3, 0, 3, 0)] = KEYPAIR(Ag, EQUAL,       Ag|Sh, EQUAL         ),
    [CHRD(3, 0, 3, 1)] = KEYPAIR(Ag, LBRACKET,    Ag|Sh, LBRACKET      ),
    [CHRD(3, 0, 3, 2)] = KEYPAIR(Ag, RBRACKET,    Ag|Sh, RBRACKET      ),
    [CHRD(3, 0, 3, 3)] = KEYPAIR(Ag, BSLASH,      Ag|Sh, BSLASH        ),
    [CHRD(3, 1, 0, 0)] = KEYPAIR(Ag, NONUS_HASH,  Ag|Sh, NONUS_HASH    ),
    [CHRD(3, 1, 0, 1)] = KEYPAIR(Ag, SCOLON,      Ag|Sh, SCOLON        ),
    [CHRD(3, 1, 0, 2)] = KEYPAIR(Ag, QUOTE,       Ag|Sh, QUOTE         ),
    [CHRD(3, 1, 0, 3)] = KEYPAIR(Ag, GRAVE,       Ag|Sh, GRAVE         ),
    [CHRD(3, 1, 1, 0)] = KEYPAIR(Ag, COMMA,       Ag|Sh, COMMA         ),
    [CHRD(3, 1, 1, 1)] = KEYPAIR(Ag, DOT,         Ag|Sh, DOT           ),
    [CHRD(3, 1, 1, 2)] = KEYPAIR(Ag, SLASH,       Ag|Sh, SLASH         ),
    [CHRD(3, 1, 1, 3)] = KEYPAIR(Ag, NONUS_BSLASH,Ag|Sh, NONUS_BSLASH  ),
    [CHRD(3, 1, 2, 0)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 1, 2, 1)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 1, 2, 2)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 1, 2, 3)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 1, 3, 0)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 1, 3, 1)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 1, 3, 2)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 1, 3, 3)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 0, 0)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 0, 1)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 0, 2)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 0, 3)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 1, 0)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 1, 1)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 1, 2)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 1, 3)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 2, 0)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 2, 1)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 2, 2)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 2, 3)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 3, 0)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 3, 1)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 3, 2)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 2, 3, 3)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 3, 0, 0)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 3, 0, 1)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 3, 0, 2)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 3, 0, 3)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 3, 1, 0)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 3, 1, 1)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 3, 1, 2)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 3, 1, 3)] = KEYPAIR(No, NO,             No, NO            ),
    [CHRD(3, 3, 2, 0)] = KEYPAIR(No, FN0,            No, NO            ),
    [CHRD(3, 3, 2, 1)] = KEYPAIR(No, FN1,            No, NO            ),
    [CHRD(3, 3, 2, 2)] = KEYPAIR(No, FN2,            No, NO            ),
    [CHRD(3, 3, 2, 3)] = KEYPAIR(No, FN3,            No, NO            ),
    [CHRD(3, 3, 3, 0)] = KEYPAIR(No, FN4,            No, NO            ),
    [CHRD(3, 3, 3, 1)] = KEYPAIR(No, FN5,            No, NO            ),
    [CHRD(3, 3, 3, 2)] = KEYPAIR(No, FN6,            No, NO            ),
    [CHRD(3, 3, 3, 3)] = KEYPAIR(No, FN7,            No, NO            ),
};

/*
 * Keychords comprising a THB_ACTION(n)-mapped thumb chord FN and
 * zero to four keys from one particular ROW of the upper three rows.
 */
#define FN_CHRD(FN, ROW, ROW_PATTERN) (((FN)<<6) | ((ROW)<<4) | (ROW_PATTERN))
/* fn_chrdfunc() dispatches on id of AF(opt, id) used here */
static action_t fn_chrdmap[128] EEMEM = {
    [FN_CHRD(0, 0, 0b0000)] = AC_NO,
    /* hole: 0x01-0x10 */
    [FN_CHRD(0, 1, 0b0001)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_NONE | MOD_NONE | MOD_LCTL),
    [FN_CHRD(0, 1, 0b0010)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_NONE | MOD_LGUI | MOD_NONE),
    [FN_CHRD(0, 1, 0b0011)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_NONE | MOD_LGUI | MOD_LCTL),
    [FN_CHRD(0, 1, 0b0100)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_LSFT | MOD_NONE | MOD_NONE),
    [FN_CHRD(0, 1, 0b0101)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_LSFT | MOD_NONE | MOD_LCTL),
    [FN_CHRD(0, 1, 0b0110)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_LSFT | MOD_LGUI | MOD_NONE),
    [FN_CHRD(0, 1, 0b0111)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_LSFT | MOD_LGUI | MOD_LCTL),
    [FN_CHRD(0, 1, 0b1000)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_NONE | MOD_NONE | MOD_NONE),
    [FN_CHRD(0, 1, 0b1001)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_NONE | MOD_NONE | MOD_LCTL),
    [FN_CHRD(0, 1, 0b1010)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_NONE | MOD_LGUI | MOD_NONE),
    [FN_CHRD(0, 1, 0b1011)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_NONE | MOD_LGUI | MOD_LCTL),
    [FN_CHRD(0, 1, 0b1100)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_LSFT | MOD_NONE | MOD_NONE),
    [FN_CHRD(0, 1, 0b1101)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_LSFT | MOD_NONE | MOD_LCTL),
    [FN_CHRD(0, 1, 0b1110)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_LSFT | MOD_LGUI | MOD_NONE),
    [FN_CHRD(0, 1, 0b1111)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_LSFT | MOD_LGUI | MOD_LCTL),
    /* hole: 0x20 */
    [FN_CHRD(0, 2, 0b0001)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_NONE | MOD_LCTL),
    [FN_CHRD(0, 2, 0b0010)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_LGUI | MOD_NONE),
    [FN_CHRD(0, 2, 0b0011)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_LGUI | MOD_LCTL),
    [FN_CHRD(0, 2, 0b0100)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_LSFT | MOD_NONE | MOD_NONE),
    [FN_CHRD(0, 2, 0b0101)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_LSFT | MOD_NONE | MOD_LCTL),
    [FN_CHRD(0, 2, 0b0110)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_LSFT | MOD_LGUI | MOD_NONE),
    [FN_CHRD(0, 2, 0b0111)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_LSFT | MOD_LGUI | MOD_LCTL),
    [FN_CHRD(0, 2, 0b1000)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_NONE | MOD_NONE | MOD_NONE),
    [FN_CHRD(0, 2, 0b1001)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_NONE | MOD_NONE | MOD_LCTL),
    [FN_CHRD(0, 2, 0b1010)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_NONE | MOD_LGUI | MOD_NONE),
    [FN_CHRD(0, 2, 0b1011)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_NONE | MOD_LGUI | MOD_LCTL),
    [FN_CHRD(0, 2, 0b1100)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_LSFT | MOD_NONE | MOD_NONE),
    [FN_CHRD(0, 2, 0b1101)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_LSFT | MOD_NONE | MOD_LCTL),
    [FN_CHRD(0, 2, 0b1110)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_LSFT | MOD_LGUI | MOD_NONE),
    [FN_CHRD(0, 2, 0b1111)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_LSFT | MOD_LGUI | MOD_LCTL),
    /* hole: 0x30 */
    [FN_CHRD(0, 3, 0b0001)] = AC_NO,
    [FN_CHRD(0, 3, 0b0010)] = AC_NO,
    [FN_CHRD(0, 3, 0b0011)] = AF(L_NUM, CHG_LAYER),
    [FN_CHRD(0, 3, 0b0100)] = AC_NO,
    [FN_CHRD(0, 3, 0b0101)] = AC_NO,
    [FN_CHRD(0, 3, 0b0110)] = AF(0, SWAP_CHRDS),
    [FN_CHRD(0, 3, 0b0111)] = AF(0, PRINT),
    [FN_CHRD(0, 3, 0b1000)] = AC_NO,
    [FN_CHRD(0, 3, 0b1001)] = AF(L_MSE, CHG_LAYER),
    [FN_CHRD(0, 3, 0b1010)] = AC_NO,
    [FN_CHRD(0, 3, 0b1011)] = AC_NO,
    [FN_CHRD(0, 3, 0b1100)] = AF(L_NAV, CHG_LAYER),
    [FN_CHRD(0, 3, 0b1101)] = AC_NO,
    [FN_CHRD(0, 3, 0b1110)] = AF(L_MCR, CHG_LAYER),
    [FN_CHRD(0, 3, 0b1111)] = AF(0, MCR_RECORD),
    [FN_CHRD(1, 0, 0b0000)] = AC_NO,
    /* hole: 0x41-0x50*/
    [FN_CHRD(1, 1, 0b0001)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_NONE | MOD_NONE | MOD_RCTL),
    [FN_CHRD(1, 1, 0b0010)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_NONE | MOD_RGUI | MOD_NONE),
    [FN_CHRD(1, 1, 0b0011)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_NONE | MOD_RGUI | MOD_RCTL),
    [FN_CHRD(1, 1, 0b0100)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_RSFT | MOD_NONE | MOD_NONE),
    [FN_CHRD(1, 1, 0b0101)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_RSFT | MOD_NONE | MOD_RCTL),
    [FN_CHRD(1, 1, 0b0110)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_RSFT | MOD_RGUI | MOD_NONE),
    [FN_CHRD(1, 1, 0b0111)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_RSFT | MOD_RGUI | MOD_RCTL),
    [FN_CHRD(1, 1, 0b1000)] = ACTION_MODS_TAP_TOGGLE(MOD_RALT | MOD_NONE | MOD_NONE | MOD_NONE),
    [FN_CHRD(1, 1, 0b1001)] = ACTION_MODS_TAP_TOGGLE(MOD_RALT | MOD_NONE | MOD_NONE | MOD_RCTL),
    [FN_CHRD(1, 1, 0b1010)] = ACTION_MODS_TAP_TOGGLE(MOD_RALT | MOD_NONE | MOD_RGUI | MOD_NONE),
    [FN_CHRD(1, 1, 0b1011)] = ACTION_MODS_TAP_TOGGLE(MOD_RALT | MOD_NONE | MOD_RGUI | MOD_RCTL),
    [FN_CHRD(1, 1, 0b1100)] = ACTION_MODS_TAP_TOGGLE(MOD_RALT | MOD_RSFT | MOD_NONE | MOD_NONE),
    [FN_CHRD(1, 1, 0b1101)] = ACTION_MODS_TAP_TOGGLE(MOD_RALT | MOD_RSFT | MOD_NONE | MOD_RCTL),
    [FN_CHRD(1, 1, 0b1110)] = ACTION_MODS_TAP_TOGGLE(MOD_RALT | MOD_RSFT | MOD_RGUI | MOD_NONE),
    [FN_CHRD(1, 1, 0b1111)] = ACTION_MODS_TAP_TOGGLE(MOD_RALT | MOD_RSFT | MOD_RGUI | MOD_RCTL),
    /* hole: 0x60 */
    [FN_CHRD(1, 2, 0b0001)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_NONE | MOD_RCTL),
    [FN_CHRD(1, 2, 0b0010)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_RGUI | MOD_NONE),
    [FN_CHRD(1, 2, 0b0011)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_RGUI | MOD_RCTL),
    [FN_CHRD(1, 2, 0b0100)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_RSFT | MOD_NONE | MOD_NONE),
    [FN_CHRD(1, 2, 0b0101)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_RSFT | MOD_NONE | MOD_RCTL),
    [FN_CHRD(1, 2, 0b0110)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_RSFT | MOD_RGUI | MOD_NONE),
    [FN_CHRD(1, 2, 0b0111)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_RSFT | MOD_RGUI | MOD_RCTL),
    [FN_CHRD(1, 2, 0b1000)] =    ACTION_MODS_ONESHOT(MOD_RALT | MOD_NONE | MOD_NONE | MOD_NONE),
    [FN_CHRD(1, 2, 0b1001)] =    ACTION_MODS_ONESHOT(MOD_RALT | MOD_NONE | MOD_NONE | MOD_RCTL),
    [FN_CHRD(1, 2, 0b1010)] =    ACTION_MODS_ONESHOT(MOD_RALT | MOD_NONE | MOD_RGUI | MOD_NONE),
    [FN_CHRD(1, 2, 0b1011)] =    ACTION_MODS_ONESHOT(MOD_RALT | MOD_NONE | MOD_RGUI | MOD_RCTL),
    [FN_CHRD(1, 2, 0b1100)] =    ACTION_MODS_ONESHOT(MOD_RALT | MOD_RSFT | MOD_NONE | MOD_NONE),
    [FN_CHRD(1, 2, 0b1101)] =    ACTION_MODS_ONESHOT(MOD_RALT | MOD_RSFT | MOD_NONE | MOD_RCTL),
    [FN_CHRD(1, 2, 0b1110)] =    ACTION_MODS_ONESHOT(MOD_RALT | MOD_RSFT | MOD_RGUI | MOD_NONE),
    [FN_CHRD(1, 2, 0b1111)] =    ACTION_MODS_ONESHOT(MOD_RALT | MOD_RSFT | MOD_RGUI | MOD_RCTL),
    /* hole: 0x70 */
    [FN_CHRD(1, 3, 0b0001)] = AC_NO,
    [FN_CHRD(1, 3, 0b0010)] = AC_NO,
    [FN_CHRD(1, 3, 0b0011)] = AC_NO,
    [FN_CHRD(1, 3, 0b0100)] = AC_NO,
    [FN_CHRD(1, 3, 0b0101)] = AC_NO,
    [FN_CHRD(1, 3, 0b0110)] = AC_NO,
    [FN_CHRD(1, 3, 0b0111)] = AC_NO,
    [FN_CHRD(1, 3, 0b1000)] = AC_NO,
    [FN_CHRD(1, 3, 0b1001)] = AC_NO,
    [FN_CHRD(1, 3, 0b1010)] = AC_NO,
    [FN_CHRD(1, 3, 0b1011)] = AC_NO,
    [FN_CHRD(1, 3, 0b1100)] = AC_NO,
    [FN_CHRD(1, 3, 0b1101)] = AC_NO,
    [FN_CHRD(1, 3, 0b1110)] = AC_NO,
    [FN_CHRD(1, 3, 0b1111)] = AC_NO,
};

/*
 * Cells in fn_chrdmap that are unreachable because they are mapped to
 * impossible chords.  We'll be using them for macro storage.
 */
static const uint8_t fn_chrdmap_holes[36] PROGMEM = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x20, 0x30,
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x60, 0x70,
};

#if MCR_LEN * MCR_MAX > 48
#    error "Macro space exceeds sizeof(fn_chrdmap_holes) * 16bit / 12bit"
#endif

/*
 * Keychords on the bottom row.  Not stored in EEPROM; immutable.
 */
#define ACT_THB_CHRD ACT_MODS_TAP
#define THB_CHRD(FN1, UPPER, FN2) ((FN1) | ((UPPER)<<1) | ((FN2)<<2))
#define THB_ACTION(FN) ACTION(ACT_THB_CHRD, (FN))
#define THB_UP (ACT_THB_CHRD<<12 | MOD_LSFT<<8)

static const action_t thb_chrdmap[8] PROGMEM = {
    [THB_CHRD(0, 0, 0)] = AC_NO, /* unreachable */
    [THB_CHRD(0, 0, 1)] = THB_ACTION(0),
    [THB_CHRD(0, 1, 0)] = {.code = THB_UP},
    [THB_CHRD(0, 1, 1)] = AC_NO,
    [THB_CHRD(1, 0, 0)] = THB_ACTION(1),
    [THB_CHRD(1, 0, 1)] = AC_ESCAPE,
    [THB_CHRD(1, 1, 0)] = AC_NO,
    [THB_CHRD(1, 1, 1)] = AF(0, RESET),
  };


/*************************************************************
 * Human-readable names of the keycodes
 *************************************************************/
#define CODE_NAME_LEN 9
static const char code_name[][CODE_NAME_LEN + 1] PROGMEM = {
    [KC_NO]                  = "no",
    [KC_ROLL_OVER]           = "roll_over",
    [KC_POST_FAIL]           = "post_fail",
    [KC_UNDEFINED]           = "undefined",
    [KC_A]                   = "a",
    [KC_B]                   = "b",
    [KC_C]                   = "c",
    [KC_D]                   = "d",
    [KC_E]                   = "e",
    [KC_F]                   = "f",
    [KC_G]                   = "g",
    [KC_H]                   = "h",
    [KC_I]                   = "i",
    [KC_J]                   = "j",
    [KC_K]                   = "k",
    [KC_L]                   = "l",
    [KC_M]                   = "m",
    [KC_N]                   = "n",
    [KC_O]                   = "o",
    [KC_P]                   = "p",
    [KC_Q]                   = "q",
    [KC_R]                   = "r",
    [KC_S]                   = "s",
    [KC_T]                   = "t",
    [KC_U]                   = "u",
    [KC_V]                   = "v",
    [KC_W]                   = "w",
    [KC_X]                   = "x",
    [KC_Y]                   = "y",
    [KC_Z]                   = "z",
    [KC_1]                   = "1",
    [KC_2]                   = "2",
    [KC_3]                   = "3",
    [KC_4]                   = "4",
    [KC_5]                   = "5",
    [KC_6]                   = "6",
    [KC_7]                   = "7",
    [KC_8]                   = "8",
    [KC_9]                   = "9",
    [KC_0]                   = "0",
    [KC_ENTER]               = "enter",
    [KC_ESCAPE]              = "escape",
    [KC_BSPACE]              = "bspace",
    [KC_TAB]                 = "tab",
    [KC_SPACE]               = "space",
    [KC_MINUS]               = "minus",
    [KC_EQUAL]               = "equal",
    [KC_LBRACKET]            = "lbracket",
    [KC_RBRACKET]            = "rbracket",
    [KC_BSLASH]              = "bslash",
    [KC_NONUS_HASH]          = "nus_hash",
    [KC_SCOLON]              = "scolon",
    [KC_QUOTE]               = "quote",
    [KC_GRAVE]               = "grave",
    [KC_COMMA]               = "comma",
    [KC_DOT]                 = "dot",
    [KC_SLASH]               = "slash",
    [KC_CAPSLOCK]            = "capslock",
    [KC_F1]                  = "f1",
    [KC_F2]                  = "f2",
    [KC_F3]                  = "f3",
    [KC_F4]                  = "f4",
    [KC_F5]                  = "f5",
    [KC_F6]                  = "f6",
    [KC_F7]                  = "f7",
    [KC_F8]                  = "f8",
    [KC_F9]                  = "f9",
    [KC_F10]                 = "f10",
    [KC_F11]                 = "f11",
    [KC_F12]                 = "f12",
    [KC_PSCREEN]             = "pscreen",
    [KC_SCROLLLOCK]          = "scrolllck",
    [KC_PAUSE]               = "pause",
    [KC_INSERT]              = "insert",
    [KC_HOME]                = "home",
    [KC_PGUP]                = "pgup",
    [KC_DELETE]              = "delete",
    [KC_END]                 = "end",
    [KC_PGDOWN]              = "pgdown",
    [KC_RIGHT]               = "right",
    [KC_LEFT]                = "left",
    [KC_DOWN]                = "down",
    [KC_UP]                  = "up",
    [KC_NUMLOCK]             = "numlock",
    [KC_KP_SLASH]            = "kp_slash",
    [KC_KP_ASTERISK]         = "kp_aster",
    [KC_KP_MINUS]            = "kp_minus",
    [KC_KP_PLUS]             = "kp_plus",
    [KC_KP_ENTER]            = "kp_enter",
    [KC_KP_1]                = "kp_1",
    [KC_KP_2]                = "kp_2",
    [KC_KP_3]                = "kp_3",
    [KC_KP_4]                = "kp_4",
    [KC_KP_5]                = "kp_5",
    [KC_KP_6]                = "kp_6",
    [KC_KP_7]                = "kp_7",
    [KC_KP_8]                = "kp_8",
    [KC_KP_9]                = "kp_9",
    [KC_KP_0]                = "kp_0",
    [KC_KP_DOT]              = "kp_dot",
    [KC_NONUS_BSLASH]        = "nus_bslsh",
    [KC_APPLICATION]         = "appl",
    [KC_POWER]               = "power",
    [KC_KP_EQUAL]            = "kp_equal",
    [KC_F13]                 = "f13",
    [KC_F14]                 = "f14",
    [KC_F15]                 = "f15",
    [KC_F16]                 = "f16",
    [KC_F17]                 = "f17",
    [KC_F18]                 = "f18",
    [KC_F19]                 = "f19",
    [KC_F20]                 = "f20",
    [KC_F21]                 = "f21",
    [KC_F22]                 = "f22",
    [KC_F23]                 = "f23",
    [KC_F24]                 = "f24",
    [KC_EXECUTE]             = "execute",
    [KC_HELP]                = "help",
    [KC_MENU]                = "menu",
    [KC_SELECT]              = "select",
    [KC_STOP]                = "stop",
    [KC_AGAIN]               = "again",
    [KC_UNDO]                = "undo",
    [KC_CUT]                 = "cut",
    [KC_COPY]                = "copy",
    [KC_PASTE]               = "paste",
    [KC_FIND]                = "find",
    [KC__MUTE]               = "mute",
    [KC__VOLUP]              = "volup",
    [KC__VOLDOWN]            = "voldown",
    [KC_LOCKING_CAPS]        = "lck_caps",
    [KC_LOCKING_NUM]         = "lck_num",
    [KC_LOCKING_SCROLL]      = "lck_scrll",
    [KC_KP_COMMA]            = "kp_comma",
    [KC_KP_EQUAL_AS400]      = "kp_eq_as4",
    [KC_INT1]                = "int1",
    [KC_INT2]                = "int2",
    [KC_INT3]                = "int3",
    [KC_INT4]                = "int4",
    [KC_INT5]                = "int5",
    [KC_INT6]                = "int6",
    [KC_INT7]                = "int7",
    [KC_INT8]                = "int8",
    [KC_INT9]                = "int9",
    [KC_LANG1]               = "lang1",
    [KC_LANG2]               = "lang2",
    [KC_LANG3]               = "lang3",
    [KC_LANG4]               = "lang4",
    [KC_LANG5]               = "lang5",
    [KC_LANG6]               = "lang6",
    [KC_LANG7]               = "lang7",
    [KC_LANG8]               = "lang8",
    [KC_LANG9]               = "lang9",
    [KC_ALT_ERASE]           = "alt_erase",
    [KC_SYSREQ]              = "sysreq",
    [KC_CANCEL]              = "cancel",
    [KC_CLEAR]               = "clear",
    [KC_PRIOR]               = "prior",
    [KC_RETURN]              = "return",
    [KC_SEPARATOR]           = "separator",
    [KC_OUT]                 = "out",
    [KC_OPER]                = "oper",
    [KC_CLEAR_AGAIN]         = "clr_again",
    [KC_CRSEL]               = "crsel",
    [KC_EXSEL]               = "exsel",
    [KC_KP_00]               = "kp_00",
    [KC_KP_000]              = "kp_000",
    [KC_THOUSANDS_SEPARATOR] = "1000s_sep",
    [KC_DECIMAL_SEPARATOR]   = "dec_sep",
    [KC_CURRENCY_UNIT]       = "cur_unit",
    [KC_CURRENCY_SUB_UNIT]   = "c_subunit",
    [KC_KP_LPAREN]           = "kp_lparen",
    [KC_KP_RPAREN]           = "kp_rparen",
    [KC_KP_LCBRACKET]        = "kp_lcbrck",
    [KC_KP_RCBRACKET]        = "kp_rcbrck",
    [KC_KP_TAB]              = "kp_tab",
    [KC_KP_BSPACE]           = "kp_bspace",
    [KC_KP_A]                = "kp_a",
    [KC_KP_B]                = "kp_b",
    [KC_KP_C]                = "kp_c",
    [KC_KP_D]                = "kp_d",
    [KC_KP_E]                = "kp_e",
    [KC_KP_F]                = "kp_f",
    [KC_KP_XOR]              = "kp_xor",
    [KC_KP_HAT]              = "kp_hat",
    [KC_KP_PERC]             = "kp_perc",
    [KC_KP_LT]               = "kp_lt",
    [KC_KP_GT]               = "kp_gt",
    [KC_KP_AND]              = "kp_and",
    [KC_KP_LAZYAND]          = "kp_lzyand",
    [KC_KP_OR]               = "kp_or",
    [KC_KP_LAZYOR]           = "kp_lazyor",
    [KC_KP_COLON]            = "kp_colon",
    [KC_KP_HASH]             = "kp_hash",
    [KC_KP_SPACE]            = "kp_space",
    [KC_KP_ATMARK]           = "kp_atmark",
    [KC_KP_EXCLAMATION]      = "kp_exclam",
    [KC_KP_MEM_STORE]        = "kp_m_sto",
    [KC_KP_MEM_RECALL]       = "kp_m_rcl",
    [KC_KP_MEM_CLEAR]        = "kp_m_clr",
    [KC_KP_MEM_ADD]          = "kp_m_add",
    [KC_KP_MEM_SUB]          = "kp_m_sub",
    [KC_KP_MEM_MUL]          = "kp_m_mul",
    [KC_KP_MEM_DIV]          = "kp_m_div",
    [KC_KP_PLUS_MINUS]       = "kp_plu_mi",
    [KC_KP_CLEAR]            = "kp_clr",
    [KC_KP_CLEAR_ENTRY]      = "kp_clr_en",
    [KC_KP_BINARY]           = "kp_binary",
    [KC_KP_OCTAL]            = "kp_octal",
    [KC_KP_DECIMAL]          = "kp_dec",
    [KC_KP_HEXADECIMAL]      = "kp_hex",
    [KC_LCTRL]               = "lctrl",
    [KC_LSHIFT]              = "lshift",
    [KC_LALT]                = "lalt",
    [KC_LGUI]                = "lgui",
    [KC_RCTRL]               = "rctrl",
    [KC_RSHIFT]              = "rshift",
    [KC_RALT]                = "ralt",
    [KC_RGUI]                = "rgui",
    [KC_FN0]                 = "macro_0",
    [KC_FN1]                 = "macro_1",
    [KC_FN2]                 = "macro_2",
    [KC_FN3]                 = "macro_3",
    [KC_FN4]                 = "macro_4",
    [KC_FN5]                 = "macro_5",
    [KC_FN6]                 = "macro_6",
    [KC_FN7]                 = "macro_7",
};

static const char chrdfunc_name[][CODE_NAME_LEN + 1] PROGMEM = {
    [CHG_LAYER]  = "chg layer",
    [SWAP_CHRDS] = "swap chds",
    [MCR_RECORD] = "rec macro",
    [PRINT]      = "prnt chds",
    [RESET]      = "reset kbd",
};

    
/*************************************************************
 * Illumination
 *************************************************************/

enum led_cmd {OFF = 0, ON = 1, STATE};
#define FOREVER UINT8_MAX
#define LED_ON(port, bit) {(PORT##port) |= (1<<(bit));} while (0)
#define LED_OFF(port, bit) {(PORT##port) &= ~(1<<(bit));} while (0)
#define LED_STATE(port, bit) ((PORT##port) & (1<<(bit)))
#define LED_INIT(port, bit) {(DDR##port) |= (1<<(bit));} while (0)

/*
 * Set or return state of an LED
 */
static bool
led(uint8_t led, uint8_t cmd)
{
    switch (cmd) {
    case ON:
        switch (led) {
        case 0: LED_ON(D, 0); break;
        case 1: LED_ON(D, 4); break;
        case 2: LED_ON(D, 5); break;
        case 3: LED_ON(D, 6); break;
        case 4: LED_ON(B, 0); break;
        case 5: LED_ON(B, 1); break;
        case 6: LED_ON(B, 4); break;
        case 7: LED_ON(B, 5); break;
        case 8: LED_ON(B, 6); break;
        case 9: LED_ON(B, 7); break;
        case 10: LED_ON(C, 5); break;
        case 11: LED_ON(C, 6); break;
        }
        break;
    case OFF:
        switch (led) {
        case 0: LED_OFF(D, 0); break;
        case 1: LED_OFF(D, 4); break;
        case 2: LED_OFF(D, 5); break;
        case 3: LED_OFF(D, 6); break;
        case 4: LED_OFF(B, 0); break;
        case 5: LED_OFF(B, 1); break;
        case 6: LED_OFF(B, 4); break;
        case 7: LED_OFF(B, 5); break;
        case 8: LED_OFF(B, 6); break;
        case 9: LED_OFF(B, 7); break;
        case 10: LED_OFF(C, 5); break;
        case 11: LED_OFF(C, 6); break;
        }
        break;
    case STATE:
        switch (led) {
        case 0: return LED_STATE(D, 0); break;
        case 1: return LED_STATE(D, 4); break;
        case 2: return LED_STATE(D, 5); break;
        case 3: return LED_STATE(D, 6); break;
        case 4: return LED_STATE(B, 0); break;
        case 5: return LED_STATE(B, 1); break;
        case 6: return LED_STATE(B, 4); break;
        case 7: return LED_STATE(B, 5); break;
        case 8: return LED_STATE(B, 6); break;
        case 9: return LED_STATE(B, 7); break;
        case 10: return LED_STATE(C, 5); break;
        case 11: return LED_STATE(C, 6); break;
        }
        break;
    }
    return false;
}

static void
led_init(void)
{
    LED_INIT(D, 0);
    LED_INIT(D, 4);
    LED_INIT(D, 5);
    LED_INIT(D, 6);
    LED_INIT(B, 0);
    LED_INIT(B, 1);
    LED_INIT(B, 4);
    LED_INIT(B, 5);
    LED_INIT(B, 6);
    LED_INIT(B, 7);
    LED_INIT(C, 5);
    LED_INIT(C, 6);
}

static struct {
    uint8_t on;
    uint8_t off;
    uint16_t last;
    uint8_t cycles;
} leds[12] = {{0}};

static void
update_leds(void)
{
    int8_t i;

    for (i = 0; i < 12; i++) {
        if (led(i, STATE)) {
            if (timer_elapsed(leds[i].last) > leds[i].on) {
                led(i, OFF);
                leds[i].last = timer_read();
            }
        } else {
            if (timer_elapsed(leds[i].last) > leds[i].off && leds[i].cycles > 0) {
                led(i, ON);
                leds[i].last = timer_read();
                if (leds[i].cycles != FOREVER)
                    leds[i].cycles--;
            }
        }
    }
}

enum ledset_id {
    LEDS_NO_KEYCODE,
    LEDS_NUM_LOCK,
    LEDS_SCROLL_LOCK,
    LEDS_SFT,
    LEDS_CTL,
    LEDS_ALT,
    LEDS_GUI,
    LEDS_ALL_MODS,
    LEDS_CHG_LAYER,
    LEDS_SWAP_FIRST,
    LEDS_SWAP_SECOND,
    LEDS_RECORD_MCR,
    LEDS_PRINT,
    LEDS_RESET,
};

static const struct {
    uint8_t len;
    uint8_t leds[12];
} ledsets[] PROGMEM = {
    [LEDS_NO_KEYCODE]  = {.len = 3,  .leds = {0, 1, 8}},
    [LEDS_NUM_LOCK]    = {.len = 1,  .leds = {6}},
    [LEDS_SCROLL_LOCK] = {.len = 1,  .leds = {7}},
    [LEDS_SFT]         = {.len = 1,  .leds = {5}},
    [LEDS_CTL]         = {.len = 2,  .leds = {4, 9}},
    [LEDS_ALT]         = {.len = 2,  .leds = {3, 10}},
    [LEDS_GUI]         = {.len = 2,  .leds = {2, 11}},
    [LEDS_ALL_MODS]    = {.len = 7,  .leds = {2, 3, 4, 5, 9, 10, 11}},
    [LEDS_CHG_LAYER]   = {.len = 12, .leds = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    [LEDS_SWAP_FIRST]  = {.len = 2,  .leds = {4, 9}},
    [LEDS_SWAP_SECOND] = {.len = 2,  .leds = {2, 11}},
    [LEDS_RECORD_MCR]  = {.len = 3,  .leds = {0, 1, 8}},
    [LEDS_PRINT]       = {.len = 12, .leds = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    [LEDS_RESET]       = {.len = 12, .leds = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
};

/* blink patterns: on time, off time, cycles (0-255) */
#define BLINK_WAITING 50, 50, FOREVER
#define BLINK_STOP 0, 0, 0
#define BLINK_WARNING 10, 40, 3
#define BLINK_MCR_WARNING 10, 40, FOREVER
#define BLINK_ERROR 10, 40, 10
#define BLINK_OK 200, 0, 2
#define BLINK_RESET 10, 0, 1
#define BLINK_STEADY 250, 0, FOREVER
#define BLINK_ONESHOT_MODS 200, 20, FOREVER
#define BLINK_REVERSE_ONESHOT_MODS 20, 200, FOREVER
#define BLINK_TOGGLED_MODS BLINK_STEADY
#define BLINK_CHG_LAYER 250, 0, 1

static void
blink(uint8_t id, uint8_t on, uint8_t off, uint8_t cycles)
{
    uint8_t i, len;

    len = pgm_read_byte(ledsets + id);
    for (i = 0; i < len; i++) {
        uint8_t led_num;

        led_num = pgm_read_byte((uint8_t *)(ledsets + id) + 1 + i);
        leds[led_num].on = on;
        leds[led_num].off = off;
        leds[led_num].cycles = cycles;
    }
}

static void
blink_mods(void)
{
    uint8_t m, wm;
    uint8_t hkbl = host_keyboard_leds();
    uint8_t alt = MOD_LALT, sft = MOD_LSFT, gui = MOD_LGUI, ctl = MOD_LCTL;
    uint8_t cpslck = hkbl & 1<<USB_LED_CAPS_LOCK;

    keyboard_set_leds(hkbl);
    m = get_mods();
    wm = get_weak_mods() & ~m;
    m = m>>4 | (m & 0xf);
    wm = wm>>4 | (wm & 0xf);
    if (m & alt)
        blink(LEDS_ALT, BLINK_TOGGLED_MODS);
    else
        blink(LEDS_ALT, BLINK_STOP);
    if (wm & alt)
        blink(LEDS_ALT, BLINK_ONESHOT_MODS);
    if (m & gui)
        blink(LEDS_GUI, BLINK_TOGGLED_MODS);
    else
        blink(LEDS_GUI, BLINK_STOP);
    if (wm & gui)
        blink(LEDS_GUI, BLINK_ONESHOT_MODS);
    if (m & ctl)
        blink(LEDS_CTL, BLINK_TOGGLED_MODS);
    else
        blink(LEDS_CTL, BLINK_STOP);
    if (wm & ctl)
        blink(LEDS_CTL, BLINK_ONESHOT_MODS);
    if (((m & sft) && cpslck) || (!(m & sft) && !cpslck)) {
        if (wm & sft)
            blink(LEDS_SFT, BLINK_ONESHOT_MODS);
        else
            blink(LEDS_SFT, BLINK_STOP);
    } else if ((m & sft) && !cpslck) {
        blink(LEDS_SFT, BLINK_TOGGLED_MODS);
    } else if (!(m & sft) && cpslck) {
        if (wm & sft)
            blink(LEDS_SFT, BLINK_REVERSE_ONESHOT_MODS);
        else
            blink(LEDS_SFT, BLINK_TOGGLED_MODS);
    }
    if (hkbl & (1<<USB_LED_NUM_LOCK))
        blink(LEDS_NUM_LOCK, BLINK_STEADY);
    else
        blink(LEDS_NUM_LOCK, BLINK_STOP);
    if (hkbl & (1<<USB_LED_SCROLL_LOCK))
        blink(LEDS_SCROLL_LOCK, BLINK_STEADY);
    else
        blink(LEDS_SCROLL_LOCK, BLINK_STOP);
}


/*************************************************************
 * Printing
 *************************************************************/

#define LINEBUFLEN 50
#define HDRHEIGHT 3

enum header {KEYPAIR_HDR, FN_ACT_HDR, THB_ACT_HDR,};
enum print {PRINT_CANCEL, PRINT_START, PRINT_NEXT,};

static uint8_t
strtocodes (char *buf)
{
    char c;
    uint8_t i = 0, code;

    while ((c = buf[i])) {
        if (c >= 'a' && c <= 'z')
            code = c - 'a' + KC_A;
        else if (c >= '1' && c <= '9')
            code = c - '1' + KC_1;
        else if (c == '0')
            code = KC_0;
        else if (c == '*')
            code = KC_KP_ASTERISK;
        else if (c == '-')
            code = KC_KP_MINUS;
        else if (c == '\n')
            code = KC_ENTER;
        else
            code = KC_SPACE;
        buf[i] = code;
        i++;
    }
    return i;
}

static const char print_header[][HDRHEIGHT][LINEBUFLEN] PROGMEM = {
    [KEYPAIR_HDR] = {"* ************ finger chords **************\n",
                     "* chrd  ---lower---  *****  ---upper---  **\n",
                     "* rows mod code c name      mod code c name\n",},
    [FN_ACT_HDR]  = {"* ***** fn finger chords *****\n",
                     "*   chrd modifiers  **********\n",
                     "* l rows left rght d code name\n",},
    [THB_ACT_HDR] = {"* **** bottom row chords ****\n",
                     "* chrd modifiers  ***********\n",
                     "* rows left rght l code name\n",},
};

static void
fmt_hdr(uint8_t hdr_kind, uint8_t hdr_line, char *linebuf, uint8_t *len)
{
    strcpy_P(linebuf, print_header[hdr_kind][hdr_line]);
    *len = strtocodes(linebuf);
}

static void
fmt_keypair(uint8_t chrd, char *linebuf, char *modsbuf, uint8_t *len)
{
    char name_lo[CODE_NAME_LEN + 1] = "", name_up[CODE_NAME_LEN + 1] = "";
    keypair_t kp;

    eeprom_read_block(&kp, chrdmap + chrd, sizeof(keypair_t));
    strcpy_P(name_lo, (char *)(code_name + kp.code_lo));
    strcpy_P(name_up, (char *)(code_name + kp.code_up));
    snprintf(linebuf, LINEBUFLEN,
             "* %x%x%x%x %c%c%c%c%#04x   %-9s %c%c%c%c%#04x   %-s\n",
             (chrd & 3<<6)>>6,
             (chrd & 3<<4)>>4,
             (chrd & 3<<2)>>2,
             (chrd & 3<<0)>>0,
             kp.mods_lo & Ag ? 'g' : '-',
             kp.mods_lo & Sh ? 's' : '-',
             kp.mods_lo & Al ? 'a' : '-',
             kp.mods_lo & Co ? 'c' : '-',
             kp.code_lo,
             name_lo,
             kp.mods_up & Ag ? 'g' : '-',
             kp.mods_up & Sh ? 's' : '-',
             kp.mods_up & Al ? 'a' : '-',
             kp.mods_up & Co ? 'c' : '-',
             kp.code_up,
             name_up);
    *len = strtocodes(linebuf);
    if ((kp.code_lo >= KC_A && kp.code_lo <= KC_0) ||
        (kp.code_lo >= KC_SPACE && kp.code_lo <= KC_SLASH) ||
        kp.code_lo == KC_NONUS_BSLASH) {
        linebuf[16] = kp.code_lo;
        modsbuf[16] = KEYPAIR_MODS_TO_MODS(kp.mods_lo & (Ag | Sh));
    }
    if ((kp.code_up >= KC_A && kp.code_up <= KC_0) ||
        (kp.code_up >= KC_SPACE && kp.code_up <= KC_SLASH) ||
        kp.code_up == KC_NONUS_BSLASH) {
        linebuf[37] = kp.code_up;
        modsbuf[37] = KEYPAIR_MODS_TO_MODS(kp.mods_up & (Ag | Sh));
    }
}

static bool
fmt_fn_action(uint8_t chrd, char *linebuf, uint8_t *len)
{
    char mods_strength = ' ', name[CODE_NAME_LEN + 1] = "modifiers";
    uint8_t mods = 0, row, keycode = 0;
    action_t a;

    a.code = eeprom_read_word((uint16_t *)fn_chrdmap + chrd);
    if ((chrd >= 0x1 && chrd <= 0x10) || chrd == 0x20 || chrd == 0x30 ||
        (chrd >= 0x41 && chrd <= 0x50) || chrd == 0x60 || chrd == 0x70)
        /* unreachable chords */
        return false;
    switch (a.kind.id) {
    case ACT_LMODS_TAP:
    case ACT_RMODS_TAP:
    {               /* modifier chords from fn_chrdmap */
        mods = a.key.mods;
        if (a.kind.id == ACT_RMODS_TAP)
            mods <<= 4;
        switch (a.key.code) {
        case MODS_ONESHOT: mods_strength = '1'; break;
        case MODS_TAP_TOGGLE: mods_strength = 't'; break;
        }
        break;
    }
    case ACT_FUNCTION:
    {
        uint8_t func_id = a.func.opt;

        strcpy_P(name, (char *)(chrdfunc_name + func_id));
        break;
    }
    default:             /* plain finger chord from fn_chrdmap */
        keycode = a.key.code;
        mods = a.key.mods;
        mods_strength = '-';
        strcpy_P(name, (char *)(code_name + keycode));
        break;
    }
    row = (chrd & 3<<4)>>4;
    snprintf(linebuf, LINEBUFLEN,
             "* %c %x%x%x%x %c%c%c%c %c%c%c%c %c %#04x %-s\n",
             chrd & 1<<6 ? '1' : '0',
             chrd & 1<<3 ? row : 0,
             chrd & 1<<2 ? row : 0,
             chrd & 1<<1 ? row : 0,
             chrd & 1<<0 ? row : 0,
             mods & MOD_LALT ? 'a' : '-',
             mods & MOD_LSFT ? 's' : '-',
             mods & MOD_LGUI ? 'g' : '-',
             mods & MOD_LCTL ? 'c' : '-',
             mods & MOD_LALT<<4 ? 'a' : '-',
             mods & MOD_LSFT<<4 ? 's' : '-',
             mods & MOD_LGUI<<4 ? 'g' : '-',
             mods & MOD_LCTL<<4 ? 'c' : '-',
             mods_strength,
             keycode,
             name);
    *len = strtocodes(linebuf);
    return true;
}

static void
fmt_thb_action(uint8_t chrd, char *linebuf, uint8_t *len)
{
    char level = ' ', name[CODE_NAME_LEN + 1] = " ";
    uint8_t mods = 0, keycode = 0;
    action_t a;

    a.code = pgm_read_word((uint16_t *)thb_chrdmap + chrd);
    if (a.code == KC_NO) { /* plain finger chrd from chrdmap */
        level = 'l';
    } else if (a.code == THB_UP) { /* upper-level finger chord from chrdmap */
        level = 'u';
    } else if (a.key.kind == ACT_MODS) { /* plain thumb chord from thb_chrdmap */
        keycode = a.key.code;
        mods = a.key.mods;
        level = '-';
        strcpy_P(name, (char *)(code_name + keycode));
    } else if (a.kind.id == ACT_FUNCTION) {
        uint8_t func_id = a.func.opt;

        strcpy_P(name, (char *)(chrdfunc_name + func_id));
    } else {
        level = a.kind.param ? '0' : '1';
    }
    snprintf(linebuf, LINEBUFLEN, "*  %c%c%c %c%c%c%c %c%c%c%c %c %#04x %-s\n",
             chrd & 1<<2 ? '4' : '0',
             chrd & 1<<1 ? '4' : '0',
             chrd & 1<<0 ? '4' : '0',
             mods & MOD_LALT ? 'a' : '-',
             mods & MOD_LSFT ? 's' : '-',
             mods & MOD_LGUI ? 'g' : '-',
             mods & MOD_LCTL ? 'c' : '-',
             mods & MOD_LALT<<4 ? 'a' : '-',
             mods & MOD_LSFT<<4 ? 's' : '-',
             mods & MOD_LGUI<<4 ? 'g' : '-',
             mods & MOD_LCTL<<4 ? 'c' : '-',
             level,
             keycode,
             name);
    *len = strtocodes(linebuf);
}

static void
print_chrdmaps(uint8_t cmd)
{
    enum {FMT_KEYPAIR_HDR, FMT_KEYPAIR, FMT_FN_ACT_HDR, FMT_FN_ACT,
          FMT_THB_ACT_HDR, FMT_THB_ACT, PRINTING_LN, DONE, IDLE,};
    static uint8_t printing = IDLE, scheduled_printing = IDLE;
    static uint16_t fng_chrd = 0;
    static uint8_t fn_chrd, thb_chrd = 0;
    static uint8_t fng_hdr = 0,fn_hdr = 0, thb_hdr = 0;
    static char linebuf[LINEBUFLEN] = "", modsbuf[LINEBUFLEN] = "";
    static uint8_t i, buflen, bufpos = 0;

    switch (cmd) {
    case PRINT_START:
        fng_hdr = fn_hdr = thb_hdr = 0;
        fng_chrd = 0;
        fn_chrd = 0;
        thb_chrd = 0;
        clear_keyboard();
        printing = FMT_KEYPAIR_HDR;
        break;
    case PRINT_CANCEL:
        printing = DONE;
        break;
    case PRINT_NEXT:
        switch (printing) {
        case PRINTING_LN:
            blink(LEDS_PRINT, BLINK_STEADY);
            if (bufpos <= buflen) {
                add_key(linebuf[bufpos]);
                add_mods(modsbuf[bufpos]);
                send_keyboard_report();
                clear_keyboard();
                bufpos++;
            } else {
                for (i = 0; i < LINEBUFLEN; modsbuf[i++] = 0);
                printing = scheduled_printing;
                bufpos = 0;
            }
            break;
        case FMT_KEYPAIR_HDR:
            if (fng_hdr < HDRHEIGHT) {
                fmt_hdr(KEYPAIR_HDR, fng_hdr, linebuf, &buflen);
                fng_hdr++;
                printing = PRINTING_LN;
                scheduled_printing = FMT_KEYPAIR_HDR;
            } else {
                printing = FMT_KEYPAIR;
            }
            break;
        case FMT_KEYPAIR:
            if (fng_chrd < 256) {
                fmt_keypair(fng_chrd, linebuf, modsbuf, &buflen);
                fng_chrd++;
                printing = PRINTING_LN;
                scheduled_printing = FMT_KEYPAIR;
            } else {
                printing = FMT_FN_ACT_HDR;
            }
            break;
        case FMT_FN_ACT_HDR:
            if (fn_hdr < HDRHEIGHT) {
                fmt_hdr(FN_ACT_HDR, fn_hdr, linebuf, &buflen);
                fn_hdr++;
                printing = PRINTING_LN;
                scheduled_printing = FMT_FN_ACT_HDR;
            } else {
                printing = FMT_FN_ACT;
            }
            break;
        case FMT_FN_ACT:
            if (fn_chrd < 128) {
                if (fmt_fn_action(fn_chrd, linebuf, &buflen))
                    printing = PRINTING_LN;
                else
                    printing = FMT_FN_ACT;
                fn_chrd++;
                scheduled_printing = FMT_FN_ACT;
            } else {
                printing = FMT_THB_ACT_HDR;
            }
            break;
        case FMT_THB_ACT_HDR:
            if (thb_hdr < HDRHEIGHT) {
                fmt_hdr(THB_ACT_HDR, thb_hdr, linebuf, &buflen);
                thb_hdr++;
                printing = PRINTING_LN;
                scheduled_printing = FMT_THB_ACT_HDR;
            } else {
                printing = FMT_THB_ACT;
            }
            break;
        case FMT_THB_ACT:
            if (thb_chrd < 8) {
                fmt_thb_action(thb_chrd, linebuf, &buflen);
                thb_chrd++;
                scheduled_printing = FMT_THB_ACT;
                printing = PRINTING_LN;
            } else {
                printing = DONE;
            }
            break;
        case DONE:
            blink(LEDS_PRINT, BLINK_STOP);
            printing = IDLE;
            break;
        case IDLE:
            break;
        }
        break;
    }
}


/*************************************************************
 * Customization support: swap chord mappings
 *************************************************************/

enum swap_state {
    IDLE = false,
    EXPECT_FIRST_CHRD,
    EXPECT_FNG_CHRD,
    EXPECT_FN_CHRD,
    HAVE_FNG_CHRDS,
    HAVE_FN_CHRDS,
    CANCEL,
};

static struct {
    uint8_t chrd1;
    uint8_t chrd2;
    uint8_t state;
    bool level1 :1;
    bool level2 :1;
} swap = {.state = IDLE};

static void
swap_chrds(void)
{
    switch (swap.state) {
    case IDLE:
        swap.state = EXPECT_FIRST_CHRD;
        blink(LEDS_SWAP_FIRST, BLINK_WAITING);
        break;
    case EXPECT_FNG_CHRD:
    case EXPECT_FN_CHRD:
        blink(LEDS_SWAP_FIRST, BLINK_STOP);
        blink(LEDS_SWAP_SECOND, BLINK_WAITING);
        break;
    case HAVE_FNG_CHRDS:
    {
        keypair_t kp1, kp2, kpa, kpb;

        eeprom_read_block(&kp1, chrdmap + swap.chrd1, sizeof(keypair_t));
        eeprom_read_block(&kp2, chrdmap + swap.chrd2, sizeof(keypair_t));
        kpa = kp1;
        kpb = kp2;
        if (swap.level1 == swap.level2) {
            kpa.code_up = kp2.code_up;
            kpa.mods_up = kp2.mods_up;
            kpb.code_up = kp1.code_up;
            kpb.mods_up = kp1.mods_up;
            if (!swap.level1 && !swap.level2) {
                kp1 = kpa;
                kpa = kpb;
                kpb = kp1;
            }
        } else if (swap.chrd1 == swap.chrd2) {
            kpa.code_up = kp1.code_lo;
            kpa.mods_up = kp1.mods_lo;
            kpa.code_lo = kp1.code_up;
            kpa.mods_lo = kp1.mods_up;
            kpb = kpa;
        } else if (swap.level1) {
            kpa.code_up = kp2.code_lo;
            kpa.mods_up = kp2.mods_lo;
            kpb.code_lo = kp1.code_up;
            kpb.mods_lo = kp1.mods_up;
        } else if (swap.level2) {
            kpa.code_lo = kp2.code_up;
            kpa.mods_lo = kp2.mods_up;
            kpb.code_up = kp1.code_lo;
            kpb.mods_up = kp1.mods_lo;
        } else {                /* shouldn't happen */
            kpa = kp1;
            kpb = kp2;
        }
        eeprom_update_block(&kpa, chrdmap + swap.chrd1, sizeof(keypair_t));
        eeprom_update_block(&kpb, chrdmap + swap.chrd2, sizeof(keypair_t));
        swap.state = IDLE;
        blink(LEDS_SWAP_SECOND, BLINK_OK);
        break;
    }
    case HAVE_FN_CHRDS:
    {
        action_t a1, a2;

        a1.code = eeprom_read_word((uint16_t *)fn_chrdmap + swap.chrd1);
        a2.code = eeprom_read_word((uint16_t *)fn_chrdmap + swap.chrd2);
        eeprom_update_word((uint16_t *)fn_chrdmap + swap.chrd1, a2.code);
        eeprom_update_word((uint16_t *)fn_chrdmap + swap.chrd2, a1.code);
        swap.state = IDLE;
        blink(LEDS_SWAP_SECOND, BLINK_OK);
        break;
    }
    case CANCEL:
    default:
        swap.state = IDLE;
        blink(LEDS_SWAP_FIRST, BLINK_STOP);
        blink(LEDS_SWAP_SECOND, BLINK_ERROR);
        break;
    }
}


/*************************************************************
 * Customization support: record and play macros
 *************************************************************/

static void
emit_keycode(uint8_t weak_mods, uint8_t keycode, bool success_elsewhere);

enum mcr_cmd {START_REC, COLLECT, EXEC, CANCEL_MCR,};
enum mcr_chrd_direction {GET, PUT,};

static void
mcr_chrd(uint8_t direction, uint8_t mcr, uint8_t c,
           uint8_t *mods, uint8_t *keycode)
{
    uint16_t mods0_word_idx, modsn_word_idx, modsn_word, modsn_nibble;
    uint16_t keyn_word_idx, keyn_word, keyn_byte;
    uint8_t mods0_nibble_idx, modsn_nibble_idx, keyn_byte_idx;

    mods0_word_idx = MCR_LEN * MCR_MAX / 2;
    mods0_nibble_idx = (MCR_LEN * MCR_MAX / 2) % 4;
    modsn_word_idx = (mods0_word_idx * 4 +
                      mods0_nibble_idx + (mcr * MCR_LEN) + c) / 4;
    modsn_nibble_idx = (mods0_nibble_idx + (mcr * MCR_LEN) + c) % 4;
    modsn_word = eeprom_read_word((uint16_t *)fn_chrdmap +
                                  pgm_read_byte(fn_chrdmap_holes + modsn_word_idx));
    keyn_word_idx = (mcr * MCR_LEN + c) / 2;
    keyn_byte_idx = (mcr * MCR_LEN + c) % 2;
    keyn_word = eeprom_read_word((uint16_t *)fn_chrdmap +
                                 pgm_read_byte(fn_chrdmap_holes + keyn_word_idx));
    switch (direction) {
    case GET:
        modsn_nibble = modsn_word & (0x0f<<modsn_nibble_idx * 4);
        *mods = modsn_nibble>>modsn_nibble_idx * 4;
        keyn_byte = keyn_word & (0xff<<keyn_byte_idx * 8);
        *keycode = keyn_byte>>keyn_byte_idx * 8;
        break;
    case PUT:
        modsn_word &= ~(0x0f<<modsn_nibble_idx * 4);
        modsn_word |= *mods<<modsn_nibble_idx * 4;
        eeprom_update_word((uint16_t *)fn_chrdmap +
                           pgm_read_byte(fn_chrdmap_holes + modsn_word_idx),
                           modsn_word);
        keyn_word &= ~(0xff<<keyn_byte_idx * 8);
        keyn_word |= *keycode<<keyn_byte_idx * 8;
        eeprom_update_word((uint16_t *)fn_chrdmap +
                           pgm_read_byte(fn_chrdmap_holes + keyn_word_idx),
                           keyn_word);
        break;
    }
}

static bool
mcr(uint8_t cmd, uint8_t keycode)
{
    static enum {RECORDING, IDLE,} state = IDLE;
    static uint8_t k[MCR_LEN] = {0};
    static uint8_t m[MCR_LEN] = {0};
    static uint8_t idx = 0;
    uint8_t i, fn_n = keycode - KC_FN0, mods = 0, kc;

    switch (cmd) {
    case START_REC:
        state = RECORDING;
        for (i = 0; i < MCR_LEN; i++) {
            k[i] = 0;
            m[i] = 0;
        }
        idx = 0;
        blink(LEDS_RECORD_MCR, BLINK_WAITING);
        return true;
        break;
    case COLLECT:
        switch (state) {
        case RECORDING:         /* add mods, keycode to new macro */
            if (idx < MCR_LEN) {
                mods = get_mods() | get_weak_mods();
                if ((mods || keycode)) {
                    m[idx] = MODS_TO_KEYPAIR_MODS(mods);
                    k[idx] = keycode;
                    idx++;
                }
            } else {
                blink(LEDS_RECORD_MCR, BLINK_MCR_WARNING);
            }
            return true;
            break;
        case IDLE:
            break;
        }
        break;
    case EXEC:
        switch (state) {
        case IDLE:              /* play macro */
            for (i = 0; i < MCR_LEN; i++) {
                mcr_chrd(GET, fn_n, i, &mods, &kc);
                if (kc || mods)
                    emit_keycode(mods, kc, false);
                else
                    break;      /* for (i = 0; ... */
            }
            break;
        case RECORDING:         /* store collected macro */
            for (i = 0; i < MCR_LEN; i++)
                mcr_chrd(PUT, fn_n, i, &m[i], &k[i]);
            state = IDLE;
            blink(LEDS_RECORD_MCR, BLINK_OK);
            break;
        }
        break;
    case CANCEL_MCR:
        state = IDLE;
        break;
    }
    return false;
}


/*************************************************************
 * Collect and handle key chords
 *************************************************************/

/*
 * Change bit patterns (representing keys pressed on columns) from two
 * bit per colum like d0c0b0a0, 0d0c0b0a, or ddccbbaa into one bit per
 * column like 00rrdcba.  The two rr bits represent the (single) row
 * the keys belonged to.
 */
static uint8_t
squeeze_chrd (uint8_t c)
{
    uint8_t even_bits, odd_bits, row;

    even_bits = (c & 1) | (c & 1<<2)>>1 | (c & 1<<4)>>2 | (c & 1<<6)>>3;
    odd_bits = (c & 1<<1)>>1 | (c & 1<<3)>>2 | (c & 1<<5)>>3 | (c & 1<<7)>>4;
    row = (c>>6 | c>>4 | c>>2 | c) & 3;
    if (even_bits && odd_bits && even_bits != odd_bits)
        return 0;               /* no multi-row chords allowed here */
    return even_bits | odd_bits | row<<4;
}

static uint8_t
fn_chrdfunc(action_t a)
{
    uint8_t func_id = a.func.opt, opt = a.func.id;

    switch (func_id) {
    case CHG_LAYER:
        return opt;            /* leave chord mode */
        break;
    case SWAP_CHRDS:
        swap_chrds();
        break;
    case MCR_RECORD:
        mcr(START_REC, 0);
        break;
    case PRINT:
        print_chrdmaps(PRINT_START);
        break;
    case RESET:
        print_chrdmaps(PRINT_CANCEL);
        mcr(CANCEL_MCR, 0);
        clear_keyboard();
        blink(LEDS_RESET, BLINK_RESET);
        update_leds();          /* preempt LED usage */
        wait_ms(10);
        break;
    }
    return false;
}

static void
emit_keycode(uint8_t weak_mods, uint8_t keycode, bool success_elsewhere)
{
    bool collecting_mcr = false;

    if (keycode >= KC_FN0 && keycode < KC_FN0 + MCR_MAX) {
        mcr(EXEC, keycode);
    } else {
        add_weak_mods(weak_mods);
        collecting_mcr = mcr(COLLECT, keycode);
        add_key(keycode);
        if (!(keycode | success_elsewhere |
              get_weak_mods() | get_mods() | collecting_mcr))
            blink(LEDS_NO_KEYCODE, BLINK_WARNING);
        send_keyboard_report();
    }
    clear_keyboard_but_mods();
    blink(LEDS_ALL_MODS, BLINK_STOP);
}

static uint8_t
emit_chrd(uint8_t thb_chrd, uint8_t fng_chrd)
{
    keypair_t keypair = {0};
    action_t thb_state = {0};
    uint8_t weak_mods = 0, keycode = 0, fn_chrd = 0, predicted_swap_state = IDLE;
    bool mods_tap_only = false, thb_func = false;

    thb_state.code = pgm_read_word((uint16_t *)thb_chrdmap + thb_chrd);
    eeprom_read_block(&keypair, chrdmap + fng_chrd, sizeof(keypair_t));
    if (thb_state.code == KC_NO) {
        /* plain finger chord from chrdmap */
        weak_mods = KEYPAIR_MODS_TO_MODS(keypair.mods_lo);
        keycode = keypair.code_lo;
        predicted_swap_state = EXPECT_FNG_CHRD;
    } else if (thb_state.code == THB_UP) {
        /* upper-level finger chord from chrdmap */
        weak_mods = KEYPAIR_MODS_TO_MODS(keypair.mods_up);
        keycode = keypair.code_up;
        predicted_swap_state = EXPECT_FNG_CHRD;
    } else if (thb_state.key.kind == ACT_MODS) {
        /* plain thumb chord from thb_chrdmap */
        weak_mods = thb_state.key.mods;
        keycode = thb_state.key.code;
        predicted_swap_state = IDLE;
    } else if (thb_state.kind.id == ACT_FUNCTION) {
        fn_chrdfunc(thb_state);
        thb_func = true;
        predicted_swap_state = IDLE;
    } else {                    /* fn_chrdmap */
        action_t fn_act;

        fn_chrd = squeeze_chrd(fng_chrd) | ((thb_state.key.code & 1)<<6);
        fn_act.code = eeprom_read_word((uint16_t *)fn_chrdmap + fn_chrd);
        switch (fn_act.kind.id) {
        case ACT_LMODS_TAP:
        case ACT_RMODS_TAP:
        {               /* modifier chords from fn_chrdmap */
            uint8_t m = fn_act.key.mods;

            if (fn_act.kind.id == ACT_RMODS_TAP)
                m <<= 4;
            switch (fn_act.key.code) {
            case MODS_ONESHOT:
                set_weak_mods(m);
                break;
            case MODS_TAP_TOGGLE:
                set_mods(get_mods() ^ m);
                break;
            }
            mods_tap_only = true;
            predicted_swap_state = IDLE;
            break;
        }
        case ACT_FUNCTION:
        {
            uint8_t layer;

            if ((layer = fn_chrdfunc(fn_act))) {
                return layer;   /* leave chord mode */
            }
            predicted_swap_state = IDLE;
            break;
        }
        default:             /* plain finger chord from fn_chrdmap */
            weak_mods = fn_act.key.mods;
            keycode = fn_act.key.code;
            predicted_swap_state = EXPECT_FN_CHRD;
            break;
        }
    }
    switch (swap.state) {
    case IDLE:
        if (!mods_tap_only)
            emit_keycode(weak_mods, keycode, thb_func);
        blink_mods();
        break;
    case EXPECT_FIRST_CHRD:
        switch (predicted_swap_state) {
        case EXPECT_FNG_CHRD:
            swap.state = EXPECT_FNG_CHRD;
            swap.chrd1 = fng_chrd;
            swap.level1 = thb_state.code;
            swap_chrds();
            break;
        case EXPECT_FN_CHRD:
            swap.state = EXPECT_FN_CHRD;
            swap.chrd1 = fn_chrd;
            swap_chrds();
            break;
        }
        break;
    case EXPECT_FNG_CHRD:
        switch (predicted_swap_state) {
        case EXPECT_FNG_CHRD:
            swap.state = HAVE_FNG_CHRDS;
            swap.chrd2 = fng_chrd;
            swap.level2 = thb_state.code;
            break;
        default:
            swap.state = CANCEL;
            break;
        }
        swap_chrds();
        break;
    case EXPECT_FN_CHRD:
        switch (predicted_swap_state) {
        case EXPECT_FN_CHRD:
            swap.state = HAVE_FN_CHRDS;
            swap.chrd2 = fn_chrd;
            break;
        default:
            swap.state = CANCEL;
            break;
        }
        swap_chrds();
        break;
    }
    return 0;
}

void
action_function(keyrecord_t *record, uint8_t id, uint8_t opt)
{
    static uint8_t fng_chrd = 0, thb_chrd = 0;
    static int8_t keys_down = 0, layer = 0;
    static bool ready = true, layer_pending = false;
    keyevent_t e = record->event;
    uint8_t func_id = opt, row, col;
    keycoord_t keycoords;

    keycoords.raw = id;
    row = keycoords.key.row;
    col = keycoords.key.col;
    if (e.pressed) {
        keys_down++;
        if (ready) { /* all remaining keys from previous chord released */
            switch (func_id) {
            case THB_CHRD:    /* collect bottom row keys seperately */
                thb_chrd |= 1<<col;
                break;
            case FNG_CHRD:      /* finger keys: top three rows */
            {
                uint8_t byte_pos = col * 2;

                fng_chrd &= ~(3<<byte_pos);
                fng_chrd |= row<<byte_pos;
                break;
            }
            case MCR_PLAY:      /* non-chord macro pad key */
                mcr(EXEC, id);
                break;
            }
        }
    } else {
        if (ready) {
            if (func_id == MCR_PLAY) {
                /* ignored on key release */
            } else if (func_id == CHG_LAYER) {
                /* leave or keep out of chord mode */
                layer = id;
                layer_pending = true;
            } else if ((layer = emit_chrd(thb_chrd, fng_chrd))) {
                /* any layer but L_DFLT */
                layer_pending = true;
            }
            ready = false;
        }
        if (--keys_down <= 0) {
            /* keys_down < 0 if there are pressed keys while leaving
               non_chord mode */
            keys_down = 0;
            ready = true;
            fng_chrd = 0;
            thb_chrd = 0;
            if (layer_pending) {         /* leave chord mode */
                blink(LEDS_CHG_LAYER, BLINK_CHG_LAYER);
                layer_move(layer);
                layer_pending = false;
            }
        }
    }
}


/*************************************************************
 * TMK hook and initialization functions
 *************************************************************/
void
hook_early_init(void)
{
    led_init();
    led(8, ON);
}

void
hook_late_init(void)
{
    led(8, OFF);
    blink(LEDS_RESET, BLINK_RESET);
}

void
hook_keyboard_loop(void)
{
    update_leds();
    print_chrdmaps(PRINT_NEXT);
}

void
hook_matrix_change(keyevent_t event)
{
}

void
hook_keyboard_leds_change(uint8_t led_status)
{
    blink_mods();
}

void
led_set(uint8_t usb_led)
{
}
