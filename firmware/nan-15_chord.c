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
    /* keep those used in chrdfunc_name[] first */
    SWAP_CHRDS,
    MCR_RECORD,
    PRINT,
    RESET,
    CHG_LAYER,
    MCR_PLAY,
    FNG_CHRD,
    THB_CHRD,
    LAYER_MOMENTARY,
};

/* action_function() dispatches on AF()'s and PF()'s func_id */
#define AF(param, func_id) ACTION_FUNCTION_OPT((param), (func_id))
#define PF(row, col, func_id) AF((row) | (col)<<3, (func_id))

enum layer {
    L_DFLT = 0,
    /* keep those used in layer_name[] immdiately below L_DFLT */
    L_NUM,
    L_NAV,
    L_MSE,
    L_MCR,
    /* sublayers, only accessible from non-chord layers */
    L_NUM_FN,
};

static const action_t actionmaps[][MATRIX_ROWS][MATRIX_COLS] PROGMEM = {
    [L_DFLT] = {                /* keychord keys */
        {PF(1, 3, FNG_CHRD), PF(1, 2, FNG_CHRD), PF(1, 1, FNG_CHRD), PF(1, 0, FNG_CHRD),},
        {PF(2, 3, FNG_CHRD), PF(2, 2, FNG_CHRD), PF(2, 1, FNG_CHRD), PF(2, 0, FNG_CHRD),},
        {PF(3, 3, FNG_CHRD), PF(3, 2, FNG_CHRD), PF(3, 1, FNG_CHRD), PF(3, 0, FNG_CHRD),},
        {PF(4, 2, THB_CHRD), AC_NO,              PF(4, 1, THB_CHRD), PF(4, 0, THB_CHRD),},
    },
    [L_NUM] = {                 /* numpad */
        {AC_7,                  AC_8,  AC_9, AC_PMNS,                     },
        {AC_4,                  AC_5,  AC_6, AC_PDOT,                     },
        {AC_1,                  AC_2,  AC_3, AC_PENT,                     },
        {AF(L_DFLT, CHG_LAYER), AC_NO, AC_0, AF(L_NUM_FN, LAYER_MOMENTARY)},
    },
    [L_NAV] = {                 /* nav */
        {AC_HOME,               AC_UP,   AC_PGUP,   AC_BSPC,},
        {AC_LEFT,               AC_NO,   AC_RIGHT,  AC_INS, },
        {AC_END,                AC_DOWN, AC_PGDOWN, AC_DEL, },
        {AF(L_DFLT, CHG_LAYER), AC_NO,   AC_ESCAPE, AC_ENT, },
    },
    [L_MSE] = {                 /* mouse */
        {AC_MS_WH_LEFT,         AC_MS_UP,   AC_MS_WH_RIGHT, AC_MS_WH_UP,  },
        {AC_MS_LEFT,            AC_LSFT,    AC_MS_RIGHT,    AC_LCTL,      },
        {AC_MS_BTN3,            AC_MS_DOWN, AC_MS_BTN2,     AC_MS_WH_DOWN,},
        {AF(L_DFLT, CHG_LAYER), AC_NO,      AC_MS_BTN1,     AC_MS_BTN5,   },
    },
    [L_MCR] = {                 /* macro pad */
        {AF(KC_FN0, MCR_PLAY),  AF(KC_FN1, MCR_PLAY), AF(KC_FN2, MCR_PLAY), AF(KC_FN3, MCR_PLAY),},
        {AF(KC_FN4, MCR_PLAY),  AF(KC_FN5, MCR_PLAY), AF(KC_FN6, MCR_PLAY), AF(KC_FN7, MCR_PLAY),},
        {AF(L_NUM, CHG_LAYER),  AF(L_NAV, CHG_LAYER), AF(L_MSE, CHG_LAYER), AC_SPC,              },
        {AF(L_DFLT, CHG_LAYER), AC_NO,                AC_ESCAPE,            AC_ENT,              },
    },
    [L_NUM_FN] = {                 /* numpad sublayer */
        {AC_TRNS, AC_PSLS, AC_PAST, AC_PPLS,},
        {AC_TRNS, AC_TRNS, AC_TRNS, AC_SPC, },
        {AC_TRNS, AC_TRNS, AC_TRNS, AC_BSPC,},
        {AC_TRNS, AC_NO,   AC_P0,   AC_TRNS,},
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

/* chrdmap[0].mods_low and chrdmap[0].code_lo are unaccessible */
static keypair_t chrdmap[256] EEMEM = {
    [CHRD(0, 0, 0, 0)] = KEYPAIR(   No, NO,             Sh, NO            ),
    [CHRD(0, 0, 0, 1)] = KEYPAIR(   No, 1,              No, F1            ),
    [CHRD(0, 0, 0, 2)] = KEYPAIR(   No, 0,              No, F10           ),
    [CHRD(0, 0, 0, 3)] = KEYPAIR(   No, DOT,            Sh, DOT           ),
    [CHRD(0, 0, 1, 0)] = KEYPAIR(   No, 2,              No, F2            ),
    [CHRD(0, 0, 1, 1)] = KEYPAIR(   No, 3,              No, F3            ),
    [CHRD(0, 0, 1, 2)] = KEYPAIR(   No, Q,              Sh, Q             ),
    [CHRD(0, 0, 1, 3)] = KEYPAIR(   No, FN4,            No, NO            ),
    [CHRD(0, 0, 2, 0)] = KEYPAIR(   No, I,              Sh, I             ),
    [CHRD(0, 0, 2, 1)] = KEYPAIR(   No, Z,              Sh, Z             ),
    [CHRD(0, 0, 2, 2)] = KEYPAIR(   No, T,              Sh, T             ),
    [CHRD(0, 0, 2, 3)] = KEYPAIR(   Ag, EQUAL,       Ag|Sh, EQUAL         ),
    [CHRD(0, 0, 3, 0)] = KEYPAIR(   No, COMMA,          Sh, COMMA         ),
    [CHRD(0, 0, 3, 1)] = KEYPAIR(   No, FN5,            No, NO            ),
    [CHRD(0, 0, 3, 2)] = KEYPAIR(   Ag, 7,              Ag, 0             ),
    [CHRD(0, 0, 3, 3)] = KEYPAIR(   Sh, MINUS,          Sh, 1             ),
    [CHRD(0, 1, 0, 0)] = KEYPAIR(   No, 4,              No, F4            ),
    [CHRD(0, 1, 0, 1)] = KEYPAIR(   No, 5,              No, F5            ),
    [CHRD(0, 1, 0, 2)] = KEYPAIR(   No, UNDO,           No, HELP          ),
    [CHRD(0, 1, 0, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(0, 1, 1, 0)] = KEYPAIR(   No, 6,              No, F6            ),
    [CHRD(0, 1, 1, 1)] = KEYPAIR(   No, 7,              No, F7            ),
    [CHRD(0, 1, 1, 2)] = KEYPAIR(   No, LANG4,          No, INT4          ),
    [CHRD(0, 1, 1, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(0, 1, 2, 0)] = KEYPAIR(   No, X,              Sh, X             ),
    [CHRD(0, 1, 2, 1)] = KEYPAIR(   No, KP_ENTER,       No, NO            ),
    [CHRD(0, 1, 2, 2)] = KEYPAIR(   Ag, LBRACKET,    Ag|Sh, SCOLON        ),
    [CHRD(0, 1, 2, 3)] = KEYPAIR(   No, KP_6,           No, NO            ),
    [CHRD(0, 1, 3, 0)] = KEYPAIR(   No, FN2,            No, NO            ),
    [CHRD(0, 1, 3, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(0, 1, 3, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(0, 1, 3, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(0, 2, 0, 0)] = KEYPAIR(   No, E,              Sh, E             ),
    [CHRD(0, 2, 0, 1)] = KEYPAIR(   No, GRAVE,          Sh, GRAVE         ),
    [CHRD(0, 2, 0, 2)] = KEYPAIR(   No, R,              Sh, R             ),
    [CHRD(0, 2, 0, 3)] = KEYPAIR(   No, QUOTE,          Sh, QUOTE         ),
    [CHRD(0, 2, 1, 0)] = KEYPAIR(   No, K,              Sh, K             ),
    [CHRD(0, 2, 1, 1)] = KEYPAIR(   Ag|Sh, 9,        Ag|Sh, 8             ),
    [CHRD(0, 2, 1, 2)] = KEYPAIR(   No, ESCAPE,         No, PASTE         ),
    [CHRD(0, 2, 1, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(0, 2, 2, 0)] = KEYPAIR(   No, S,              Sh, S             ),
    [CHRD(0, 2, 2, 1)] = KEYPAIR(   Ag, 2,           Ag|Sh, 2             ),
    [CHRD(0, 2, 2, 2)] = KEYPAIR(   No, G,              Sh, G             ),
    [CHRD(0, 2, 2, 3)] = KEYPAIR(   No, LEFT,           No, RIGHT         ),
    [CHRD(0, 2, 3, 0)] = KEYPAIR(   Ag, D,           Ag|Sh, D             ),
    [CHRD(0, 2, 3, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(0, 2, 3, 2)] = KEYPAIR(   No, BSLASH,         Sh, BSLASH        ),
    [CHRD(0, 2, 3, 3)] = KEYPAIR(   Ag, 5,           Ag|Sh, 5             ),
    [CHRD(0, 3, 0, 0)] = KEYPAIR(   Sh, 4,              Sh, 3             ),
    [CHRD(0, 3, 0, 1)] = KEYPAIR(   No, KP_EQUAL,       No, NO            ),
    [CHRD(0, 3, 0, 2)] = KEYPAIR(   No, KP_DOT,         No, NO            ),
    [CHRD(0, 3, 0, 3)] = KEYPAIR(   No, SLASH,          Sh, SLASH         ),
    [CHRD(0, 3, 1, 0)] = KEYPAIR(   No, FN3,            No, NO            ),
    [CHRD(0, 3, 1, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(0, 3, 1, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(0, 3, 1, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(0, 3, 2, 0)] = KEYPAIR(   Ag, 8,              Ag, 9             ),
    [CHRD(0, 3, 2, 1)] = KEYPAIR(   No, KP_9,           No, NO            ),
    [CHRD(0, 3, 2, 2)] = KEYPAIR(   Ag|Sh, COMMA,    Ag|Sh, DOT           ),
    [CHRD(0, 3, 2, 3)] = KEYPAIR(   No, UP,             No, DOWN          ),
    [CHRD(0, 3, 3, 0)] = KEYPAIR(   No, SPACE,          No, NO            ),
    [CHRD(0, 3, 3, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(0, 3, 3, 2)] = KEYPAIR(   Ag, T,           Ag|Sh, T             ),
    [CHRD(0, 3, 3, 3)] = KEYPAIR(   Sh, 7,              Ag, MINUS         ),
    [CHRD(1, 0, 0, 0)] = KEYPAIR(   No, 8,              No, F8            ),
    [CHRD(1, 0, 0, 1)] = KEYPAIR(   No, 9,              No, F9            ),
    [CHRD(1, 0, 0, 2)] = KEYPAIR(   No, F11,            No, F12           ),
    [CHRD(1, 0, 0, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 0, 1, 0)] = KEYPAIR(   No, P,              Sh, P             ),
    [CHRD(1, 0, 1, 1)] = KEYPAIR(   No, D,              Sh, D             ),
    [CHRD(1, 0, 1, 2)] = KEYPAIR(   Ag, X,              Ag, Z             ),
    [CHRD(1, 0, 1, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 0, 2, 0)] = KEYPAIR(   No, MINUS,          No, NO            ),
    [CHRD(1, 0, 2, 1)] = KEYPAIR(   Ag, SLASH,       Ag|Sh, SLASH         ),
    [CHRD(1, 0, 2, 2)] = KEYPAIR(   Ag, U,           Ag|Sh, U             ),
    [CHRD(1, 0, 2, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 0, 3, 0)] = KEYPAIR(   No, FN6,            No, NO            ),
    [CHRD(1, 0, 3, 1)] = KEYPAIR(   Ag, BSLASH,         No, NO            ),
    [CHRD(1, 0, 3, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 0, 3, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 1, 0, 0)] = KEYPAIR(   No, C,              Sh, C             ),
    [CHRD(1, 1, 0, 1)] = KEYPAIR(   No, B,              Sh, B             ),
    [CHRD(1, 1, 0, 2)] = KEYPAIR(   No, KP_ASTERISK,    No, NO            ),
    [CHRD(1, 1, 0, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 1, 1, 0)] = KEYPAIR(   No, W,              Sh, W             ),
    [CHRD(1, 1, 1, 1)] = KEYPAIR(   No, Y,              Sh, Y             ),
    [CHRD(1, 1, 1, 2)] = KEYPAIR(   No, KP_1,           No, NO            ),
    [CHRD(1, 1, 1, 3)] = KEYPAIR(   No, NO,          Ag|Sh, E             ),
    [CHRD(1, 1, 2, 0)] = KEYPAIR(   No, KP_3,           No, NO            ),
    [CHRD(1, 1, 2, 1)] = KEYPAIR(   No, KP_4,           No, NO            ),
    [CHRD(1, 1, 2, 2)] = KEYPAIR(   No, KP_5,           No, NO            ),
    [CHRD(1, 1, 2, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 1, 3, 0)] = KEYPAIR(   No, KP_7,           No, NO            ),
    [CHRD(1, 1, 3, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 1, 3, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 1, 3, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 2, 0, 0)] = KEYPAIR(   No, V,              Sh, V             ),
    [CHRD(1, 2, 0, 1)] = KEYPAIR(   Ag, NONUS_HASH,  Ag|Sh, NONUS_HASH    ),
    [CHRD(1, 2, 0, 2)] = KEYPAIR(   No, APPLICATION,    No, NO            ),
    [CHRD(1, 2, 0, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 2, 1, 0)] = KEYPAIR(Ag|Sh, X,           Ag|Sh, Z             ),
    [CHRD(1, 2, 1, 1)] = KEYPAIR(   No, F13,            No, F14           ),
    [CHRD(1, 2, 1, 2)] = KEYPAIR(   No, F15,            No, F16           ),
    [CHRD(1, 2, 1, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 2, 2, 0)] = KEYPAIR(   Ag, Y,              Ag, I             ),
    [CHRD(1, 2, 2, 1)] = KEYPAIR(   No, F17,            No, F18           ),
    [CHRD(1, 2, 2, 2)] = KEYPAIR(   No, F19,            No, F20           ),
    [CHRD(1, 2, 2, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 2, 3, 0)] = KEYPAIR(   No, F21,            No, F22           ),
    [CHRD(1, 2, 3, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 2, 3, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 2, 3, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 0, 0)] = KEYPAIR(   No, FN0,            No, NO            ),
    [CHRD(1, 3, 0, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 0, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 0, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 1, 0)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 1, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 1, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 1, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 2, 0)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 2, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 2, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 2, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 3, 0)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 3, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 3, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(1, 3, 3, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 0, 0, 0)] = KEYPAIR(   No, A,              Sh, A             ),
    [CHRD(2, 0, 0, 1)] = KEYPAIR(   Ag, W,           Ag|Sh, W             ),
    [CHRD(2, 0, 0, 2)] = KEYPAIR(   No, N,              Sh, N             ),
    [CHRD(2, 0, 0, 3)] = KEYPAIR(   Ag, 4,           Ag|Sh, 4             ),
    [CHRD(2, 0, 1, 0)] = KEYPAIR(   No, SCOLON,         Sh, SCOLON        ),
    [CHRD(2, 0, 1, 1)] = KEYPAIR(   Ag, C,           Ag|Sh, C             ),
    [CHRD(2, 0, 1, 2)] = KEYPAIR(   Ag, GRAVE,       Ag|Sh, GRAVE         ),
    [CHRD(2, 0, 1, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 0, 2, 0)] = KEYPAIR(   No, O,              Sh, O             ),
    [CHRD(2, 0, 2, 1)] = KEYPAIR(   Ag, 6,           Ag|Sh, 6             ),
    [CHRD(2, 0, 2, 2)] = KEYPAIR(   No, U,              Sh, U             ),
    [CHRD(2, 0, 2, 3)] = KEYPAIR(   Ag, G,           Ag|Sh, G             ),
    [CHRD(2, 0, 3, 0)] = KEYPAIR(   Ag, P,           Ag|Sh, P             ),
    [CHRD(2, 0, 3, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 0, 3, 2)] = KEYPAIR(   No, LANG2,          No, INT2          ),
    [CHRD(2, 0, 3, 3)] = KEYPAIR(   No, LANG3,          No, INT3          ),
    [CHRD(2, 1, 0, 0)] = KEYPAIR(   No, J,              Sh, J             ),
    [CHRD(2, 1, 0, 1)] = KEYPAIR(   No, LANG5,          No, INT5          ),
    [CHRD(2, 1, 0, 2)] = KEYPAIR(   No, LANG6,          No, INT6          ),
    [CHRD(2, 1, 0, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 1, 1, 0)] = KEYPAIR(   No, LANG8,          No, INT8          ),
    [CHRD(2, 1, 1, 1)] = KEYPAIR(   No, LANG9,          No, INT9          ),
    [CHRD(2, 1, 1, 2)] = KEYPAIR(   No, _MUTE,          No, PAUSE         ),
    [CHRD(2, 1, 1, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 1, 2, 0)] = KEYPAIR(   No, PSCREEN,        No, SYSREQ        ),
    [CHRD(2, 1, 2, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 1, 2, 2)] = KEYPAIR(   No, LANG7,          No, INT7          ),
    [CHRD(2, 1, 2, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 1, 3, 0)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 1, 3, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 1, 3, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 1, 3, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 2, 0, 0)] = KEYPAIR(   No, L,              Sh, L             ),
    [CHRD(2, 2, 0, 1)] = KEYPAIR(   Ag, K,           Ag|Sh, K             ),
    [CHRD(2, 2, 0, 2)] = KEYPAIR(   No, M,              Sh, M             ),
    [CHRD(2, 2, 0, 3)] = KEYPAIR(   No, STOP,           Ag, E             ),
    [CHRD(2, 2, 1, 0)] = KEYPAIR(   No, KP_0,           No, NO            ),
    [CHRD(2, 2, 1, 1)] = KEYPAIR(   No, KP_8,           No, POWER         ),
    [CHRD(2, 2, 1, 2)] = KEYPAIR(   No, KP_2,           No, NO            ),
    [CHRD(2, 2, 1, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 2, 2, 0)] = KEYPAIR(   No, F,              Sh, F             ),
    [CHRD(2, 2, 2, 1)] = KEYPAIR(   No, KP_SLASH,       No, NO            ),
    [CHRD(2, 2, 2, 2)] = KEYPAIR(   No, H,              Sh, H             ),
    [CHRD(2, 2, 2, 3)] = KEYPAIR(   Ag, S,           Ag|Sh, S             ),
    [CHRD(2, 2, 3, 0)] = KEYPAIR(   Ag, H,           Ag|Sh, H             ),
    [CHRD(2, 2, 3, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 2, 3, 2)] = KEYPAIR(   Ag, R,           Ag|Sh, R             ),
    [CHRD(2, 2, 3, 3)] = KEYPAIR(   Ag, Q,           Ag|Sh, Q             ),
    [CHRD(2, 3, 0, 0)] = KEYPAIR(   No, NONUS_BSLASH,   Sh, NONUS_BSLASH  ),
    [CHRD(2, 3, 0, 1)] = KEYPAIR(   Ag, Y,           Ag|Sh, Y             ),
    [CHRD(2, 3, 0, 2)] = KEYPAIR(   Ag, 3,           Ag|Sh, 3             ),
    [CHRD(2, 3, 0, 3)] = KEYPAIR(   Ag, DOT,            Ag, COMMA         ),
    [CHRD(2, 3, 1, 0)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 3, 1, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 3, 1, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 3, 1, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 3, 2, 0)] = KEYPAIR(   No, _VOLDOWN,       No, _VOLUP        ),
    [CHRD(2, 3, 2, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 3, 2, 2)] = KEYPAIR(   No, INSERT,      Ag|Sh, BSLASH        ),
    [CHRD(2, 3, 2, 3)] = KEYPAIR(   Ag, O,           Ag|Sh, O             ),
    [CHRD(2, 3, 3, 0)] = KEYPAIR(   Ag, 1,           Ag|Sh, 1             ),
    [CHRD(2, 3, 3, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(2, 3, 3, 2)] = KEYPAIR(   Ag, N,           Ag|Sh, N             ),
    [CHRD(2, 3, 3, 3)] = KEYPAIR(   Ag, M,           Ag|Sh, M             ),
    [CHRD(3, 0, 0, 0)] = KEYPAIR(   No, NONUS_HASH,     Sh, NONUS_HASH    ),
    [CHRD(3, 0, 0, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 0, 0, 2)] = KEYPAIR(   No, KP_COMMA,    Ag|Sh, 7             ),
    [CHRD(3, 0, 0, 3)] = KEYPAIR(   No, ENTER,          No, NO            ),
    [CHRD(3, 0, 1, 0)] = KEYPAIR(   No, FN7,            No, NO            ),
    [CHRD(3, 0, 1, 1)] = KEYPAIR(   No, NO,          Ag|Sh, 0             ),
    [CHRD(3, 0, 1, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 0, 1, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 0, 2, 0)] = KEYPAIR(   No, LBRACKET,       Sh, LBRACKET      ),
    [CHRD(3, 0, 2, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 0, 2, 2)] = KEYPAIR(   Ag, A,           Ag|Sh, A             ),
    [CHRD(3, 0, 2, 3)] = KEYPAIR(Ag|Sh, NONUS_BSLASH,Ag|Sh, MINUS         ),
    [CHRD(3, 0, 3, 0)] = KEYPAIR(   No, TAB,            Sh, TAB           ),
    [CHRD(3, 0, 3, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 0, 3, 2)] = KEYPAIR(   No, AGAIN,       Ag|Sh, RBRACKET      ),
    [CHRD(3, 0, 3, 3)] = KEYPAIR(   No, EQUAL,          Sh, EQUAL         ),
    [CHRD(3, 1, 0, 0)] = KEYPAIR(   No, FN1,            No, NO            ),
    [CHRD(3, 1, 0, 1)] = KEYPAIR(   Ag, SCOLON,      Ag|Sh, LBRACKET      ),
    [CHRD(3, 1, 0, 2)] = KEYPAIR(   Ag, QUOTE,       Ag|Sh, QUOTE         ),
    [CHRD(3, 1, 0, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 1, 1, 0)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 1, 1, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 1, 1, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 1, 1, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 1, 2, 0)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 1, 2, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 1, 2, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 1, 2, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 1, 3, 0)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 1, 3, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 1, 3, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 1, 3, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 2, 0, 0)] = KEYPAIR(   Sh, 8,              Sh, 9             ),
    [CHRD(3, 2, 0, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 2, 0, 2)] = KEYPAIR(   Ag, V,           Ag|Sh, V             ),
    [CHRD(3, 2, 0, 3)] = KEYPAIR(   Sh, 5,              Sh, 6             ),
    [CHRD(3, 2, 1, 0)] = KEYPAIR(   No, KP_MINUS,       No, KP_PLUS       ),
    [CHRD(3, 2, 1, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 2, 1, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 2, 1, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 2, 2, 0)] = KEYPAIR(   No, HOME,           No, END           ),
    [CHRD(3, 2, 2, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 2, 2, 2)] = KEYPAIR(   Ag, L,           Ag|Sh, L             ),
    [CHRD(3, 2, 2, 3)] = KEYPAIR(   No, PGUP,           No, PGDOWN        ),
    [CHRD(3, 2, 3, 0)] = KEYPAIR(   Ag, B,           Ag|Sh, B             ),
    [CHRD(3, 2, 3, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 2, 3, 2)] = KEYPAIR(   Ag, J,           Ag|Sh, J             ),
    [CHRD(3, 2, 3, 3)] = KEYPAIR(Ag|Sh, Y,           Ag|Sh, I             ),
    [CHRD(3, 3, 0, 0)] = KEYPAIR(   No, RBRACKET,       Sh, RBRACKET      ),
    [CHRD(3, 3, 0, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 3, 0, 2)] = KEYPAIR(   Ag, F,           Ag|Sh, F             ),
    [CHRD(3, 3, 0, 3)] = KEYPAIR(   Sh, 0,              Sh, 2             ),
    [CHRD(3, 3, 1, 0)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 3, 1, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 3, 1, 2)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 3, 1, 3)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 3, 2, 0)] = KEYPAIR(   No, F23,            No, F24           ),
    [CHRD(3, 3, 2, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 3, 2, 2)] = KEYPAIR(   No, FIND,           No, NO            ),
    [CHRD(3, 3, 2, 3)] = KEYPAIR(   No, COPY,           No, CUT           ),
    [CHRD(3, 3, 3, 0)] = KEYPAIR(   Ag, NONUS_BSLASH,   Ag, RBRACKET      ),
    [CHRD(3, 3, 3, 1)] = KEYPAIR(   No, NO,             No, NO            ),
    [CHRD(3, 3, 3, 2)] = KEYPAIR(   No, LANG1,          No, INT1          ),
    [CHRD(3, 3, 3, 3)] = KEYPAIR(   No, BSPACE,         No, DELETE        ),
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
    [FN_CHRD(0, 3, 0b0001)] = AF(L_MCR, CHG_LAYER),
    [FN_CHRD(0, 3, 0b0010)] = AF(L_MSE, CHG_LAYER),
    [FN_CHRD(0, 3, 0b0011)] = AC_NO,
    [FN_CHRD(0, 3, 0b0100)] = AF(L_NAV, CHG_LAYER),
    [FN_CHRD(0, 3, 0b0101)] = AF(0, PRINT),
    [FN_CHRD(0, 3, 0b0110)] = AF(0, SWAP_CHRDS),
    [FN_CHRD(0, 3, 0b0111)] = AC_CAPSLOCK,
    [FN_CHRD(0, 3, 0b1000)] = AF(L_NUM, CHG_LAYER),
    [FN_CHRD(0, 3, 0b1001)] = AC_NO,
    [FN_CHRD(0, 3, 0b1010)] = AC_NO,
    [FN_CHRD(0, 3, 0b1011)] = AC_SCROLLLOCK,
    [FN_CHRD(0, 3, 0b1100)] = AC_NO,
    [FN_CHRD(0, 3, 0b1101)] = AC_NO,
    [FN_CHRD(0, 3, 0b1110)] = AC_NUMLOCK,
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
    [FN_CHRD(1, 1, 0b1000)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_NONE | MOD_NONE | MOD_NONE),
    [FN_CHRD(1, 1, 0b1001)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_NONE | MOD_NONE | MOD_LCTL),
    [FN_CHRD(1, 1, 0b1010)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_NONE | MOD_LGUI | MOD_NONE),
    [FN_CHRD(1, 1, 0b1011)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_NONE | MOD_LGUI | MOD_LCTL),
    [FN_CHRD(1, 1, 0b1100)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_LSFT | MOD_NONE | MOD_NONE),
    [FN_CHRD(1, 1, 0b1101)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_LSFT | MOD_NONE | MOD_LCTL),
    [FN_CHRD(1, 1, 0b1110)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_LSFT | MOD_LGUI | MOD_NONE),
    [FN_CHRD(1, 1, 0b1111)] = ACTION_MODS_TAP_TOGGLE(MOD_LALT | MOD_LSFT | MOD_LGUI | MOD_LCTL),
    /* hole: 0x60 */
    [FN_CHRD(1, 2, 0b0001)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_NONE | MOD_RCTL),
    [FN_CHRD(1, 2, 0b0010)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_RGUI | MOD_NONE),
    [FN_CHRD(1, 2, 0b0011)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_RGUI | MOD_RCTL),
    [FN_CHRD(1, 2, 0b0100)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_RSFT | MOD_NONE | MOD_NONE),
    [FN_CHRD(1, 2, 0b0101)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_RSFT | MOD_NONE | MOD_RCTL),
    [FN_CHRD(1, 2, 0b0110)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_RSFT | MOD_RGUI | MOD_NONE),
    [FN_CHRD(1, 2, 0b0111)] =    ACTION_MODS_ONESHOT(MOD_NONE | MOD_RSFT | MOD_RGUI | MOD_RCTL),
    [FN_CHRD(1, 2, 0b1000)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_NONE | MOD_NONE | MOD_NONE),
    [FN_CHRD(1, 2, 0b1001)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_NONE | MOD_NONE | MOD_LCTL),
    [FN_CHRD(1, 2, 0b1010)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_NONE | MOD_LGUI | MOD_NONE),
    [FN_CHRD(1, 2, 0b1011)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_NONE | MOD_LGUI | MOD_LCTL),
    [FN_CHRD(1, 2, 0b1100)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_LSFT | MOD_NONE | MOD_NONE),
    [FN_CHRD(1, 2, 0b1101)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_LSFT | MOD_NONE | MOD_LCTL),
    [FN_CHRD(1, 2, 0b1110)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_LSFT | MOD_LGUI | MOD_NONE),
    [FN_CHRD(1, 2, 0b1111)] =    ACTION_MODS_ONESHOT(MOD_LALT | MOD_LSFT | MOD_LGUI | MOD_LCTL),
    /* hole: 0x70 */
    [FN_CHRD(1, 3, 0b0001)] = AF(L_MCR, CHG_LAYER),
    [FN_CHRD(1, 3, 0b0010)] = AF(L_MSE, CHG_LAYER),
    [FN_CHRD(1, 3, 0b0011)] = AC_NO,
    [FN_CHRD(1, 3, 0b0100)] = AF(L_NAV, CHG_LAYER),
    [FN_CHRD(1, 3, 0b0101)] = AF(0, PRINT),
    [FN_CHRD(1, 3, 0b0110)] = AF(0, SWAP_CHRDS),
    [FN_CHRD(1, 3, 0b0111)] = AC_CAPSLOCK,
    [FN_CHRD(1, 3, 0b1000)] = AF(L_NUM, CHG_LAYER),
    [FN_CHRD(1, 3, 0b1001)] = AC_NO,
    [FN_CHRD(1, 3, 0b1010)] = AC_NO,
    [FN_CHRD(1, 3, 0b1011)] = AC_SCROLLLOCK,
    [FN_CHRD(1, 3, 0b1100)] = AC_NO,
    [FN_CHRD(1, 3, 0b1101)] = AC_NO,
    [FN_CHRD(1, 3, 0b1110)] = AC_NUMLOCK,
    [FN_CHRD(1, 3, 0b1111)] = AF(0, MCR_RECORD),
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
    [SWAP_CHRDS] = "swap chds",
    [MCR_RECORD] = "rec macro",
    [PRINT]      = "prnt chds",
    [RESET]      = "reset kbd",
};

static const char layer_name[][CODE_NAME_LEN + 1] PROGMEM = {
    [L_NUM] = "numpad lr",
    [L_NAV] = "nav lr",
    [L_MSE] = "mouse lr",
    [L_MCR] = "macro lr",
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

/* Subsets of the LEDs */
/* These are parsed by the cheatsheet generator. */
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
    [LEDS_ALL_MODS]    = {.len = 7,  .leds = {2, 3, 4, 5, 9, 10, 11}},
    [LEDS_ALT]         = {.len = 2,  .leds = {3, 10}},
    [LEDS_CHG_LAYER]   = {.len = 12, .leds = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    [LEDS_CTL]         = {.len = 2,  .leds = {4, 9}},
    [LEDS_GUI]         = {.len = 2,  .leds = {2, 11}},
    [LEDS_NO_KEYCODE]  = {.len = 3,  .leds = {0, 1, 8}},
    [LEDS_NUM_LOCK]    = {.len = 1,  .leds = {6}},
    [LEDS_PRINT]       = {.len = 12, .leds = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    [LEDS_RECORD_MCR]  = {.len = 3,  .leds = {0, 1, 8}},
    [LEDS_RESET]       = {.len = 12, .leds = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    [LEDS_SCROLL_LOCK] = {.len = 1,  .leds = {7}},
    [LEDS_SFT]         = {.len = 1,  .leds = {5}},
    [LEDS_SWAP_FIRST]  = {.len = 2,  .leds = {4, 9}},
    [LEDS_SWAP_SECOND] = {.len = 2,  .leds = {2, 11}},
    /* not used here; for documentation:
       [LEDS_INIT]     = {.len = 1,  .leds = {8}},
    */
};

/* blink patterns: on time, off time, cycles (0-255) */
/* These are parsed by the cheatsheet generator. */
#define BLINK_CHG_LAYER 250, 0, 1
#define BLINK_ERROR 10, 40, 10
#define BLINK_MCR_WARNING 10, 40, FOREVER
#define BLINK_OK 200, 0, 2
#define BLINK_ONESHOT_MODS 200, 20, FOREVER
#define BLINK_RESET 10, 0, 1
#define BLINK_REVERSE_ONESHOT_MODS 20, 200, FOREVER
#define BLINK_STEADY 250, 0, FOREVER
#define BLINK_STOP 0, 0, 0
#define BLINK_TOGGLED_MODS 250, 0, FOREVER
#define BLINK_WAITING 50, 50, FOREVER
#define BLINK_WARNING 10, 40, 3

/* LED signalling: LED set, blink pattern  */
/* The trailing comments are extracted by the cheatsheet generator. */
#define CHG_LAYER_ON LEDS_CHG_LAYER, BLINK_CHG_LAYER /* Switching layer */
#define NO_KEYCODE_ON LEDS_NO_KEYCODE, BLINK_WARNING /* Unmapped chord */
#define NUM_LOCK_ON LEDS_NUM_LOCK, BLINK_STEADY      /* Num Lock */
#define ONESHOT_ALT_ON LEDS_ALT, BLINK_ONESHOT_MODS  /* Mod: ALT, sticky */
#define ONESHOT_CTL_ON LEDS_CTL, BLINK_ONESHOT_MODS  /* Mod: CTRL, sticky */
#define ONESHOT_GUI_ON LEDS_GUI, BLINK_ONESHOT_MODS  /* Mod: GUI, sticky */
#define ONESHOT_SFT_ON LEDS_SFT, BLINK_ONESHOT_MODS  /* Mod: SHIFT, sticky */
#define ONESHOT_SFT_REVERSE_ON LEDS_SFT, BLINK_REVERSE_ONESHOT_MODS /* Mod: unSHIFT */
#define PRINT_ON LEDS_PRINT, BLINK_STEADY /* Typing chordmap */
#define RECORD_MCR_OK_ON LEDS_RECORD_MCR, BLINK_OK /* Macro: done */
#define RECORD_MCR_ON LEDS_RECORD_MCR, BLINK_WAITING /* Macro: recording */
#define RECORD_MCR_WARNING_ON LEDS_RECORD_MCR, BLINK_WARNING /* Macro: too long */
#define RESET_ON LEDS_RESET, BLINK_RESET                     /* Keyboard reset */
#define SCROLL_LOCK_ON LEDS_SCROLL_LOCK, BLINK_STEADY        /* Scroll Lock */
#define SWAP_FIRST_ON LEDS_SWAP_FIRST, BLINK_WAITING         /* Swap: chord A? */
#define SWAP_SECOND_ERROR_ON LEDS_SWAP_SECOND, BLINK_ERROR   /* Swap: rejected */
#define SWAP_SECOND_OK_ON LEDS_SWAP_SECOND, BLINK_OK         /* Swap: done */
#define SWAP_SECOND_ON LEDS_SWAP_SECOND, BLINK_WAITING       /* Swap: chord B? */
#define TOGGLED_ALT_ON LEDS_ALT, BLINK_TOGGLED_MODS          /* Mod: ALT */
#define TOGGLED_CTL_ON LEDS_CTL, BLINK_TOGGLED_MODS          /* Mod: CTRL */
#define TOGGLED_GUI_ON LEDS_GUI, BLINK_TOGGLED_MODS          /* Mod: GUI */
#define TOGGLED_SFT_ON LEDS_SFT, BLINK_TOGGLED_MODS          /* Mod: SHIFT */
#define OFF(LEDSET) (LEDS_##LEDSET), BLINK_STOP
/* not used here; for documentation: */
#define INIT_KBD_ON LEDS_INIT, BLINK_STEADY /* Keyboard start up */

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
        blink(TOGGLED_ALT_ON);
    else
        blink(OFF(ALT));
    if (wm & alt)
        blink(ONESHOT_ALT_ON);
    if (m & gui)
        blink(TOGGLED_GUI_ON);
    else
        blink(OFF(GUI));
    if (wm & gui)
        blink(ONESHOT_GUI_ON);
    if (m & ctl)
        blink(TOGGLED_GUI_ON);
    else
        blink(OFF(CTL));
    if (wm & ctl)
        blink(ONESHOT_CTL_ON);
    if (((m & sft) && cpslck) || (!(m & sft) && !cpslck)) {
        if (wm & sft)
            blink(ONESHOT_SFT_ON);
        else
            blink(OFF(SFT));
    } else if ((m & sft) && !cpslck) {
        blink(TOGGLED_SFT_ON);
    } else if (!(m & sft) && cpslck) {
        if (wm & sft)
            blink(ONESHOT_SFT_REVERSE_ON);
        else
            blink(TOGGLED_SFT_ON);
    }
    if (hkbl & (1<<USB_LED_NUM_LOCK))
        blink(NUM_LOCK_ON);
    else
        blink(OFF(NUM_LOCK));
    if (hkbl & (1<<USB_LED_SCROLL_LOCK))
        blink(SCROLL_LOCK_ON);
    else
        blink(OFF(SCROLL_LOCK));
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
    [KEYPAIR_HDR] = {"  ************ finger chords **************\n\n",
                     "       -----lower-----      -----upper-----\n",
                     "  rows mod code c name      mod code c name\n"
                     "\n",},
    [FN_ACT_HDR]  = {"\n"
                     "  ******* fn finger chords *******\n\n",
                     "  *fn    modifiers *oneshot-toggle\n",
                     "  * rows left rght * code name\n"
                     "\n",},
    [THB_ACT_HDR] = {"\n"
                     "  ****** bottom row chords ******\n\n",
                     "       modifiers *fn-upper-lower\n",
                     "  rows left rght * code name\n"
                     "\n",},
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
        uint8_t func_id = a.func.opt, layer_id = a.func.id;

        if (func_id == CHG_LAYER)
            strcpy_P(name, (char *)(layer_name + layer_id));
        else
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
        bufpos = 0;
        clear_keyboard();
        printing = FMT_KEYPAIR_HDR;
        break;
    case PRINT_CANCEL:
        printing = DONE;
        break;
    case PRINT_NEXT:
        switch (printing) {
        case PRINTING_LN:
            blink(PRINT_ON);
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
            blink(OFF(PRINT));
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
        blink(SWAP_FIRST_ON);
        break;
    case EXPECT_FNG_CHRD:
    case EXPECT_FN_CHRD:
        blink(OFF(SWAP_FIRST));
        blink(SWAP_SECOND_ON);
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
        blink(SWAP_SECOND_OK_ON);
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
        blink(SWAP_SECOND_OK_ON);
        break;
    }
    case CANCEL:
    default:
        swap.state = IDLE;
        blink(OFF(SWAP_FIRST));
        blink(SWAP_SECOND_ERROR_ON);
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
        blink(RECORD_MCR_ON);
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
                blink(RECORD_MCR_WARNING_ON);
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
            blink(RECORD_MCR_OK_ON);
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
        blink(RESET_ON);
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
            blink(NO_KEYCODE_ON);
        send_keyboard_report();
    }
    clear_keyboard_but_mods();
    blink(OFF(ALL_MODS));
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
                mods_tap_only = true;
                break;
            case MODS_TAP_TOGGLE:
                set_mods(get_mods() ^ m);
                break;
            }
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
            case LAYER_MOMENTARY: /* non-chord sublayer */
            {
                uint8_t layer = id;

                layer_on(layer);
                keys_down = 0;
            }
            }
        }
    } else {
        if (func_id == LAYER_MOMENTARY) { /* non-chord sublayer */
            uint8_t layer = id;

            layer_off(layer);
            keys_down = 0;
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
                   non-chord mode */
                keys_down = 0;
                ready = true;
                fng_chrd = 0;
                thb_chrd = 0;
                if (layer_pending) {         /* leave chord mode */
                    blink(CHG_LAYER_ON);
                    layer_move(layer);
                    layer_pending = false;
                }
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
    blink(RESET_ON);
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
