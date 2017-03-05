/*
Copyright 2013,2016 Jun Wako <wakojun@gmail.com>

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
#include "debug.h"
#include "host.h"
#include "led.h"
#include "timer.h"
#include <avr/eeprom.h>


#ifdef BOOTMAGIC_ENABLE
extern keymap_config_t keymap_config;
#endif
#define ACT_THUMB_CHORD ACT_MODS_TAP

typedef struct {
    uint16_t on;
    uint16_t off;
    uint16_t last;
    uint8_t cycles;
} blink_t;

static blink_t leds[12] = {{0}};

enum led_cmd {OFF = 0, ON = 1, TOGGLE, STATE};
#define LED_ON(port, bit) {(DDR##port) |= (1<<(bit)); (PORT##port) |= (1<<(bit));} while (0)
#define LED_OFF(port, bit) {(DDR##port) |= (1<<(bit)); (PORT##port) &= ~(1<<(bit));} while (0)
#define LED_TOGGLE(port, bit) {(DDR##port) |= (1<<(bit)); (PORT##port) ^= (1<<(bit));} while (0)
#define LED_STATE(port, bit) ((PORT##port) & (1<<(bit)))

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
        } break;
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
        } break;
    case TOGGLE:
        switch (led) {
        case 0: LED_TOGGLE(D, 0); break;
        case 1: LED_TOGGLE(D, 4); break;
        case 2: LED_TOGGLE(D, 5); break;
        case 3: LED_TOGGLE(D, 6); break;
        case 4: LED_TOGGLE(B, 0); break;
        case 5: LED_TOGGLE(B, 1); break;
        case 6: LED_TOGGLE(B, 4); break;
        case 7: LED_TOGGLE(B, 5); break;
        case 8: LED_TOGGLE(B, 6); break;
        case 9: LED_TOGGLE(B, 7); break;
        case 10: LED_TOGGLE(C, 5); break;
        case 11: LED_TOGGLE(C, 6); break;
        } break;
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
        } break;
    }
    return false;
}
    

void
led_set(uint8_t usb_led)
{
    led(5, usb_led & (1<<USB_LED_CAPS_LOCK) ? ON : OFF);
    led(6, usb_led & (1<<USB_LED_NUM_LOCK) ? ON : OFF);
    led(7, usb_led & (1<<USB_LED_SCROLL_LOCK) ? ON : OFF);
}

void
hook_matrix_change(keyevent_t event)
{
     if (event.pressed){
         led(8, TOGGLE);
         leds[0].last = timer_read();
    }
}

static void
blink(void)
{
    int8_t i;
    
    for (i = 0; i <= 11; i++) {
        if (led(i, STATE)) {
            /* do nothing if leds[i].on == 0 and leds[i].off == 0 */
            if (leds[i].on && timer_elapsed(leds[i].last) > leds[i].on) {
                led(i, OFF);
                leds[i].last = timer_read();
            }
        } else {
            if (leds[i].off && timer_elapsed(leds[i].last) > leds[i].off && leds[i].cycles > 0) {
                led(i, ON);
                leds[i].last = timer_read();
                leds[i].cycles--;
            }
        }
                
    }
            
}

void
hook_early_init(void)
{
    led(8, ON);
}

void
hook_late_init(void)
{
    led(8, OFF);
    leds[1].on = 1000;
    leds[1].off = 300;
    leds[1].cycles = 4;
    leds[2].on = 100;
    leds[2].off = 300;
    leds[2].cycles = 20;
    leds[3] = (blink_t){.on = 100, .off = 300, .cycles = 20,};
}

void
hook_keyboard_loop(void)
{
    blink();
}

/*  NaN-15 raw actionmap and LED definition
 *
 * ,-------------------------------------------.
 * |          |        LED5         |          |
 * |          |          |          |          |
 * |   1,3    |   1,2  LED6  1,1    |   1,0    |
 * |          |          |          |          |
 * |          |        LED7         |          |
 * |LED9------+----------+----------+----- LED4|
 * |          |          |          |          |
 * |          |          |          |          |
 * |   2,3    |   2,2    |   2,1    |   2,0    |
 * |          |          |          |          |
 * |          |          |          |          |
 * |LED12-----+--------LED8---------+------LED3|
 * |          |          |          |          |
 * |          |          |          |          |
 * |   3,3    |   3,2    |   3,1    |   3,0    |
 * |          |          |          |          |
 * |          |          |          |          |
 * |LED11-----+---------------------+------LED2|
 * |          |                     |          |
 * |   4,2    |         4,1         |   4,0    |
 * |          |                     |          |
 * |        LED0                  LED1         |
 * `-------------------------------------------'
 */

#define POSFUNC(row, col) ACTION_FUNCTION_OPT(col, row)
#define THUMB_ROW 4

const action_t actionmaps[][MATRIX_ROWS][MATRIX_COLS] PROGMEM = {
    [0] = {
        { POSFUNC(1, 3), POSFUNC(1, 2), POSFUNC(1, 1), POSFUNC(1, 0), },
        { POSFUNC(2, 3), POSFUNC(2, 2), POSFUNC(2, 1), POSFUNC(2, 0), },
        { POSFUNC(3, 3), POSFUNC(3, 2), POSFUNC(3, 1), POSFUNC(3, 0), },
        { POSFUNC(4, 2), ACTION_NO,     POSFUNC(4, 1), POSFUNC(4, 0), },
    }
};

action_t
action_for_key(uint8_t layer, keypos_t key)
{
    return (action_t)pgm_read_word(&actionmaps[0][key.row][key.col]);
}

/* 
 * Two keycode/modifier sets, 12 bits each.  The keyboard's shift
 * state (which is independent of the mods stored here) determines the
 * actual pair member to be reported.
 */
typedef struct {
    unsigned int code_u :8;
    unsigned int code_s :8;
    unsigned int mods_u :4;
    unsigned int mods_s :4;
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

/* 
 * Keychords comprising one key per column from the upper three rows
 */
#define CHORD(COL0, COL1, COL2, COL3) ((COL0)<<6 | (COL1)<<4 | (COL2)<<2 | (COL3))
#define KEYPAIR(UNSHIFTED_MODS, UNSHIFTED_CODE, SHIFTED_MODS, SHIFTED_CODE) \
    {.mods_u = (UNSHIFTED_MODS), .code_u = (KC_##UNSHIFTED_CODE), \
            .mods_s = (SHIFTED_MODS), .code_s = (KC_##SHIFTED_CODE)}

static keypair_t chordmap[256] EEMEM = {
    [CHORD(0, 0, 0, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(0, 0, 0, 1)] = KEYPAIR(No, A,              Sh, A             ),
    [CHORD(0, 0, 0, 2)] = KEYPAIR(No, B,              Sh, B             ),
    [CHORD(0, 0, 0, 3)] = KEYPAIR(No, C,              Sh, C             ),
    [CHORD(0, 0, 1, 0)] = KEYPAIR(No, D,              Sh, D             ),
    [CHORD(0, 0, 1, 1)] = KEYPAIR(No, E,              Sh, E             ),
    [CHORD(0, 0, 1, 2)] = KEYPAIR(No, F,              Sh, F             ),
    [CHORD(0, 0, 1, 3)] = KEYPAIR(No, G,              Sh, G             ),
    [CHORD(0, 0, 2, 0)] = KEYPAIR(No, H,              Sh, H             ),
    [CHORD(0, 0, 2, 1)] = KEYPAIR(No, I,              Sh, I             ),
    [CHORD(0, 0, 2, 2)] = KEYPAIR(No, J,              Sh, J             ),
    [CHORD(0, 0, 2, 3)] = KEYPAIR(No, K,              Sh, K             ),
    [CHORD(0, 0, 3, 0)] = KEYPAIR(No, L,              Sh, L             ),
    [CHORD(0, 0, 3, 1)] = KEYPAIR(No, M,              Sh, M             ),
    [CHORD(0, 0, 3, 2)] = KEYPAIR(No, N,              Sh, N             ),
    [CHORD(0, 0, 3, 3)] = KEYPAIR(No, O,              Sh, O             ),
    [CHORD(0, 1, 0, 0)] = KEYPAIR(No, P,              Sh, P             ),
    [CHORD(0, 1, 0, 1)] = KEYPAIR(No, Q,              Sh, Q             ),
    [CHORD(0, 1, 0, 2)] = KEYPAIR(No, R,              Sh, R             ),
    [CHORD(0, 1, 0, 3)] = KEYPAIR(No, S,              Sh, S             ),
    [CHORD(0, 1, 1, 0)] = KEYPAIR(No, T,              Sh, T             ),
    [CHORD(0, 1, 1, 1)] = KEYPAIR(No, U,              Sh, U             ),
    [CHORD(0, 1, 1, 2)] = KEYPAIR(No, V,              Sh, V             ),
    [CHORD(0, 1, 1, 3)] = KEYPAIR(No, W,              Sh, W             ),
    [CHORD(0, 1, 2, 0)] = KEYPAIR(No, X,              Sh, X             ),
    [CHORD(0, 1, 2, 1)] = KEYPAIR(No, Y,              Sh, Y             ),
    [CHORD(0, 1, 2, 2)] = KEYPAIR(No, Z,              Sh, Z             ),
    [CHORD(0, 1, 2, 3)] = KEYPAIR(No, 1,              Sh, 1             ),
    [CHORD(0, 1, 3, 0)] = KEYPAIR(No, 2,              Sh, 2             ),
    [CHORD(0, 1, 3, 1)] = KEYPAIR(No, 3,              Sh, 3             ),
    [CHORD(0, 1, 3, 2)] = KEYPAIR(No, 4,              Sh, 4             ),
    [CHORD(0, 1, 3, 3)] = KEYPAIR(No, 5,              Sh, 5             ),
    [CHORD(0, 2, 0, 0)] = KEYPAIR(No, 6,              Sh, 6             ),
    [CHORD(0, 2, 0, 1)] = KEYPAIR(No, 7,              Sh, 7             ),
    [CHORD(0, 2, 0, 2)] = KEYPAIR(No, 8,              Sh, 8             ),
    [CHORD(0, 2, 0, 3)] = KEYPAIR(No, 9,              Sh, 9             ),
    [CHORD(0, 2, 1, 0)] = KEYPAIR(No, 0,              Sh, 0             ),
    [CHORD(0, 2, 1, 1)] = KEYPAIR(No, ENTER,          Sh, ENTER         ),
    [CHORD(0, 2, 1, 2)] = KEYPAIR(No, ESCAPE,         Sh, ESCAPE        ),
    [CHORD(0, 2, 1, 3)] = KEYPAIR(No, BSPACE,         Sh, BSPACE        ),
    [CHORD(0, 2, 2, 0)] = KEYPAIR(No, TAB,            Sh, TAB           ),
    [CHORD(0, 2, 2, 1)] = KEYPAIR(No, SPACE,          Sh, SPACE         ),
    [CHORD(0, 2, 2, 2)] = KEYPAIR(No, MINUS,          Sh, MINUS         ),
    [CHORD(0, 2, 2, 3)] = KEYPAIR(No, EQUAL,          Sh, EQUAL         ),
    [CHORD(0, 2, 3, 0)] = KEYPAIR(No, LBRACKET,       Sh, LBRACKET      ),
    [CHORD(0, 2, 3, 1)] = KEYPAIR(No, RBRACKET,       Sh, RBRACKET      ),
    [CHORD(0, 2, 3, 2)] = KEYPAIR(No, BSLASH,         Sh, BSLASH        ),
    [CHORD(0, 2, 3, 3)] = KEYPAIR(No, NONUS_HASH,     Sh, NONUS_HASH    ),
    [CHORD(0, 3, 0, 0)] = KEYPAIR(No, SCOLON,         Sh, SCOLON        ),
    [CHORD(0, 3, 0, 1)] = KEYPAIR(No, QUOTE,          Sh, QUOTE         ),
    [CHORD(0, 3, 0, 2)] = KEYPAIR(No, GRAVE,          Sh, GRAVE         ),
    [CHORD(0, 3, 0, 3)] = KEYPAIR(No, COMMA,          Sh, COMMA         ),
    [CHORD(0, 3, 1, 0)] = KEYPAIR(No, DOT,            Sh, DOT           ),
    [CHORD(0, 3, 1, 1)] = KEYPAIR(No, SLASH,          Sh, SLASH         ),
    [CHORD(0, 3, 1, 2)] = KEYPAIR(No, CAPSLOCK,       Sh, CAPSLOCK      ),
    [CHORD(0, 3, 1, 3)] = KEYPAIR(No, F1,             Sh, F1            ),
    [CHORD(0, 3, 2, 0)] = KEYPAIR(No, F2,             Sh, F2            ),
    [CHORD(0, 3, 2, 1)] = KEYPAIR(No, F3,             Sh, F3            ),
    [CHORD(0, 3, 2, 2)] = KEYPAIR(No, F4,             Sh, F4            ),
    [CHORD(0, 3, 2, 3)] = KEYPAIR(No, F5,             Sh, F5            ),
    [CHORD(0, 3, 3, 0)] = KEYPAIR(No, F6,             Sh, F6            ),
    [CHORD(0, 3, 3, 1)] = KEYPAIR(No, F7,             Sh, F7            ),
    [CHORD(0, 3, 3, 2)] = KEYPAIR(No, F8,             Sh, F8            ),
    [CHORD(0, 3, 3, 3)] = KEYPAIR(No, F9,             Sh, F9            ),
    [CHORD(1, 0, 0, 0)] = KEYPAIR(No, F10,            Sh, F10           ),
    [CHORD(1, 0, 0, 1)] = KEYPAIR(No, F11,            Sh, F11           ),
    [CHORD(1, 0, 0, 2)] = KEYPAIR(No, F12,            Sh, F12           ),
    [CHORD(1, 0, 0, 3)] = KEYPAIR(No, PSCREEN,        Sh, PSCREEN       ),
    [CHORD(1, 0, 1, 0)] = KEYPAIR(No, SCROLLLOCK,     Sh, SCROLLLOCK    ),
    [CHORD(1, 0, 1, 1)] = KEYPAIR(No, PAUSE,          Sh, PAUSE         ),
    [CHORD(1, 0, 1, 2)] = KEYPAIR(No, INSERT,         Sh, INSERT        ),
    [CHORD(1, 0, 1, 3)] = KEYPAIR(No, HOME,           Sh, HOME          ),
    [CHORD(1, 0, 2, 0)] = KEYPAIR(No, PGUP,           Sh, PGUP          ),
    [CHORD(1, 0, 2, 1)] = KEYPAIR(No, DELETE,         Sh, DELETE        ),
    [CHORD(1, 0, 2, 2)] = KEYPAIR(No, END,            Sh, END           ),
    [CHORD(1, 0, 2, 3)] = KEYPAIR(No, PGDOWN,         Sh, PGDOWN        ),
    [CHORD(1, 0, 3, 0)] = KEYPAIR(No, RIGHT,          Sh, RIGHT         ),
    [CHORD(1, 0, 3, 1)] = KEYPAIR(No, LEFT,           Sh, LEFT          ),
    [CHORD(1, 0, 3, 2)] = KEYPAIR(No, DOWN,           Sh, DOWN          ),
    [CHORD(1, 0, 3, 3)] = KEYPAIR(No, UP,             Sh, UP            ),
    [CHORD(1, 1, 0, 0)] = KEYPAIR(No, NUMLOCK,        Sh, NUMLOCK       ),
    [CHORD(1, 1, 0, 1)] = KEYPAIR(No, KP_SLASH,       Sh, KP_SLASH      ),
    [CHORD(1, 1, 0, 2)] = KEYPAIR(No, KP_ASTERISK,    Sh, KP_ASTERISK   ),
    [CHORD(1, 1, 0, 3)] = KEYPAIR(No, KP_MINUS,       Sh, KP_MINUS      ),
    [CHORD(1, 1, 1, 0)] = KEYPAIR(No, KP_PLUS,        Sh, KP_PLUS       ),
    [CHORD(1, 1, 1, 1)] = KEYPAIR(No, KP_ENTER,       Sh, KP_ENTER      ),
    [CHORD(1, 1, 1, 2)] = KEYPAIR(No, KP_1,           Sh, KP_1          ),
    [CHORD(1, 1, 1, 3)] = KEYPAIR(No, KP_2,           Sh, KP_2          ),
    [CHORD(1, 1, 2, 0)] = KEYPAIR(No, KP_3,           Sh, KP_3          ),
    [CHORD(1, 1, 2, 1)] = KEYPAIR(No, KP_4,           Sh, KP_4          ),
    [CHORD(1, 1, 2, 2)] = KEYPAIR(No, KP_5,           Sh, KP_5          ),
    [CHORD(1, 1, 2, 3)] = KEYPAIR(No, KP_6,           Sh, KP_6          ),
    [CHORD(1, 1, 3, 0)] = KEYPAIR(No, KP_7,           Sh, KP_7          ),
    [CHORD(1, 1, 3, 1)] = KEYPAIR(No, KP_8,           Sh, KP_8          ),
    [CHORD(1, 1, 3, 2)] = KEYPAIR(No, KP_9,           Sh, KP_9          ),
    [CHORD(1, 1, 3, 3)] = KEYPAIR(No, KP_0,           Sh, KP_0          ),
    [CHORD(1, 2, 0, 0)] = KEYPAIR(No, KP_DOT,         Sh, KP_DOT        ),
    [CHORD(1, 2, 0, 1)] = KEYPAIR(No, NONUS_BSLASH,   Sh, NONUS_BSLASH  ),
    [CHORD(1, 2, 0, 2)] = KEYPAIR(No, APPLICATION,    Sh, APPLICATION   ),
    [CHORD(1, 2, 0, 3)] = KEYPAIR(No, POWER,          Sh, POWER         ),
    [CHORD(1, 2, 1, 0)] = KEYPAIR(No, KP_EQUAL,       Sh, KP_EQUAL      ),
    [CHORD(1, 2, 1, 1)] = KEYPAIR(No, F13,            Sh, F13           ),
    [CHORD(1, 2, 1, 2)] = KEYPAIR(No, F14,            Sh, F14           ),
    [CHORD(1, 2, 1, 3)] = KEYPAIR(No, F15,            Sh, F15           ),
    [CHORD(1, 2, 2, 0)] = KEYPAIR(No, F16,            Sh, F16           ),
    [CHORD(1, 2, 2, 1)] = KEYPAIR(No, F17,            Sh, F17           ),
    [CHORD(1, 2, 2, 2)] = KEYPAIR(No, F18,            Sh, F18           ),
    [CHORD(1, 2, 2, 3)] = KEYPAIR(No, F19,            Sh, F19           ),
    [CHORD(1, 2, 3, 0)] = KEYPAIR(No, F20,            Sh, F20           ),
    [CHORD(1, 2, 3, 1)] = KEYPAIR(No, F21,            Sh, F21           ),
    [CHORD(1, 2, 3, 2)] = KEYPAIR(No, F22,            Sh, F22           ),
    [CHORD(1, 2, 3, 3)] = KEYPAIR(No, F23,            Sh, F23           ),
    [CHORD(1, 3, 0, 0)] = KEYPAIR(No, F24,            Sh, F24           ),
    [CHORD(1, 3, 0, 1)] = KEYPAIR(No, EXECUTE,        Sh, EXECUTE       ),
    [CHORD(1, 3, 0, 2)] = KEYPAIR(No, HELP,           Sh, HELP          ),
    [CHORD(1, 3, 0, 3)] = KEYPAIR(No, MENU,           Sh, MENU          ),
    [CHORD(1, 3, 1, 0)] = KEYPAIR(No, SELECT,         Sh, SELECT        ),
    [CHORD(1, 3, 1, 1)] = KEYPAIR(No, STOP,           Sh, STOP          ),
    [CHORD(1, 3, 1, 2)] = KEYPAIR(No, AGAIN,          Sh, AGAIN         ),
    [CHORD(1, 3, 1, 3)] = KEYPAIR(No, UNDO,           Sh, UNDO          ),
    [CHORD(1, 3, 2, 0)] = KEYPAIR(No, CUT,            Sh, CUT           ),
    [CHORD(1, 3, 2, 1)] = KEYPAIR(No, COPY,           Sh, COPY          ),
    [CHORD(1, 3, 2, 2)] = KEYPAIR(No, PASTE,          Sh, PASTE         ),
    [CHORD(1, 3, 2, 3)] = KEYPAIR(No, FIND,           Sh, FIND          ),
    [CHORD(1, 3, 3, 0)] = KEYPAIR(No, _MUTE,          Sh, _MUTE         ),
    [CHORD(1, 3, 3, 1)] = KEYPAIR(No, _VOLUP,         Sh, _VOLUP        ),
    [CHORD(1, 3, 3, 2)] = KEYPAIR(No, _VOLDOWN,       Sh, _VOLDOWN      ),
    [CHORD(1, 3, 3, 3)] = KEYPAIR(No, LOCKING_CAPS,   Sh, LOCKING_CAPS  ),
    [CHORD(2, 0, 0, 0)] = KEYPAIR(No, LOCKING_NUM,    Sh, LOCKING_NUM   ),
    [CHORD(2, 0, 0, 1)] = KEYPAIR(No, LOCKING_SCROLL, Sh, LOCKING_SCROLL),
    [CHORD(2, 0, 0, 2)] = KEYPAIR(No, KP_COMMA,       Sh, KP_COMMA      ),
    [CHORD(2, 0, 0, 3)] = KEYPAIR(No, KP_EQUAL_AS400, Sh, KP_EQUAL_AS400),
    [CHORD(2, 0, 1, 0)] = KEYPAIR(No, INT1,           Sh, INT1          ),
    [CHORD(2, 0, 1, 1)] = KEYPAIR(No, INT2,           Sh, INT2          ),
    [CHORD(2, 0, 1, 2)] = KEYPAIR(No, INT3,           Sh, INT3          ),
    [CHORD(2, 0, 1, 3)] = KEYPAIR(No, INT4,           Sh, INT4          ),
    [CHORD(2, 0, 2, 0)] = KEYPAIR(No, INT5,           Sh, INT5          ),
    [CHORD(2, 0, 2, 1)] = KEYPAIR(No, INT6,           Sh, INT6          ),
    [CHORD(2, 0, 2, 2)] = KEYPAIR(No, INT7,           Sh, INT7          ),
    [CHORD(2, 0, 2, 3)] = KEYPAIR(No, INT8,           Sh, INT8          ),
    [CHORD(2, 0, 3, 0)] = KEYPAIR(No, INT9,           Sh, INT9          ),
    [CHORD(2, 0, 3, 1)] = KEYPAIR(No, LANG1,          Sh, LANG1         ),
    [CHORD(2, 0, 3, 2)] = KEYPAIR(No, LANG2,          Sh, LANG2         ),
    [CHORD(2, 0, 3, 3)] = KEYPAIR(No, LANG3,          Sh, LANG3         ),
    [CHORD(2, 1, 0, 0)] = KEYPAIR(No, LANG4,          Sh, LANG4         ),
    [CHORD(2, 1, 0, 1)] = KEYPAIR(No, LANG5,          Sh, LANG5         ),
    [CHORD(2, 1, 0, 2)] = KEYPAIR(No, LANG6,          Sh, LANG6         ),
    [CHORD(2, 1, 0, 3)] = KEYPAIR(No, LANG7,          Sh, LANG7         ),
    [CHORD(2, 1, 1, 0)] = KEYPAIR(No, LANG8,          Sh, LANG8         ),
    [CHORD(2, 1, 1, 1)] = KEYPAIR(No, LANG9,          Sh, LANG9         ),
    [CHORD(2, 1, 1, 2)] = KEYPAIR(No, ALT_ERASE,      Sh, ALT_ERASE     ),
    [CHORD(2, 1, 1, 3)] = KEYPAIR(No, SYSREQ,         Sh, SYSREQ        ),
    [CHORD(2, 1, 2, 0)] = KEYPAIR(No, CANCEL,         Sh, CANCEL        ),
    [CHORD(2, 1, 2, 1)] = KEYPAIR(No, CLEAR,          Sh, CLEAR         ),
    [CHORD(2, 1, 2, 2)] = KEYPAIR(No, PRIOR,          Sh, PRIOR         ),
    [CHORD(2, 1, 2, 3)] = KEYPAIR(No, RETURN,         Sh, RETURN        ),
    [CHORD(2, 1, 3, 0)] = KEYPAIR(No, SEPARATOR,      Sh, SEPARATOR     ),
    [CHORD(2, 1, 3, 1)] = KEYPAIR(No, OUT,            Sh, OUT           ),
    [CHORD(2, 1, 3, 2)] = KEYPAIR(No, OPER,           Sh, OPER          ),
    [CHORD(2, 1, 3, 3)] = KEYPAIR(No, CLEAR_AGAIN,    Sh, CLEAR_AGAIN   ),
    [CHORD(2, 2, 0, 0)] = KEYPAIR(No, CRSEL,          Sh, CRSEL         ),
    [CHORD(2, 2, 0, 1)] = KEYPAIR(No, EXSEL,          Sh, EXSEL         ),
    [CHORD(2, 2, 0, 2)] = KEYPAIR(Ag, A,           Ag|Sh, A             ),
    [CHORD(2, 2, 0, 3)] = KEYPAIR(Ag, B,           Ag|Sh, B             ),
    [CHORD(2, 2, 1, 0)] = KEYPAIR(Ag, C,           Ag|Sh, C             ),
    [CHORD(2, 2, 1, 1)] = KEYPAIR(Ag, D,           Ag|Sh, D             ),
    [CHORD(2, 2, 1, 2)] = KEYPAIR(Ag, E,           Ag|Sh, E             ),
    [CHORD(2, 2, 1, 3)] = KEYPAIR(Ag, F,           Ag|Sh, F             ),
    [CHORD(2, 2, 2, 0)] = KEYPAIR(Ag, G,           Ag|Sh, G             ),
    [CHORD(2, 2, 2, 1)] = KEYPAIR(Ag, H,           Ag|Sh, H             ),
    [CHORD(2, 2, 2, 2)] = KEYPAIR(Ag, I,           Ag|Sh, I             ),
    [CHORD(2, 2, 2, 3)] = KEYPAIR(Ag, J,           Ag|Sh, J             ),
    [CHORD(2, 2, 3, 0)] = KEYPAIR(Ag, K,           Ag|Sh, K             ),
    [CHORD(2, 2, 3, 1)] = KEYPAIR(Ag, L,           Ag|Sh, L             ),
    [CHORD(2, 2, 3, 2)] = KEYPAIR(Ag, M,           Ag|Sh, M             ),
    [CHORD(2, 2, 3, 3)] = KEYPAIR(Ag, N,           Ag|Sh, N             ),
    [CHORD(2, 3, 0, 0)] = KEYPAIR(Ag, O,           Ag|Sh, O             ),
    [CHORD(2, 3, 0, 1)] = KEYPAIR(Ag, P,           Ag|Sh, P             ),
    [CHORD(2, 3, 0, 2)] = KEYPAIR(Ag, Q,           Ag|Sh, Q             ),
    [CHORD(2, 3, 0, 3)] = KEYPAIR(Ag, R,           Ag|Sh, R             ),
    [CHORD(2, 3, 1, 0)] = KEYPAIR(Ag, S,           Ag|Sh, S             ),
    [CHORD(2, 3, 1, 1)] = KEYPAIR(Ag, T,           Ag|Sh, T             ),
    [CHORD(2, 3, 1, 2)] = KEYPAIR(Ag, U,           Ag|Sh, U             ),
    [CHORD(2, 3, 1, 3)] = KEYPAIR(Ag, V,           Ag|Sh, V             ),
    [CHORD(2, 3, 2, 0)] = KEYPAIR(Ag, W,           Ag|Sh, W             ),
    [CHORD(2, 3, 2, 1)] = KEYPAIR(Ag, X,           Ag|Sh, X             ),
    [CHORD(2, 3, 2, 2)] = KEYPAIR(Ag, Y,           Ag|Sh, Y             ),
    [CHORD(2, 3, 2, 3)] = KEYPAIR(Ag, Z,           Ag|Sh, Z             ),
    [CHORD(2, 3, 3, 0)] = KEYPAIR(Ag, 1,           Ag|Sh, 1             ),
    [CHORD(2, 3, 3, 1)] = KEYPAIR(Ag, 2,           Ag|Sh, 2             ),
    [CHORD(2, 3, 3, 2)] = KEYPAIR(Ag, 3,           Ag|Sh, 3             ),
    [CHORD(2, 3, 3, 3)] = KEYPAIR(Ag, 4,           Ag|Sh, 4             ),
    [CHORD(3, 0, 0, 0)] = KEYPAIR(Ag, 5,           Ag|Sh, 5             ),
    [CHORD(3, 0, 0, 1)] = KEYPAIR(Ag, 6,           Ag|Sh, 6             ),
    [CHORD(3, 0, 0, 2)] = KEYPAIR(Ag, 7,           Ag|Sh, 7             ),
    [CHORD(3, 0, 0, 3)] = KEYPAIR(Ag, 8,           Ag|Sh, 8             ),
    [CHORD(3, 0, 1, 0)] = KEYPAIR(Ag, 9,           Ag|Sh, 9             ),
    [CHORD(3, 0, 1, 1)] = KEYPAIR(Ag, 0,           Ag|Sh, 0             ),
    [CHORD(3, 0, 1, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 0, 1, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 0, 2, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 0, 2, 1)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 0, 2, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 0, 2, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 0, 3, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 0, 3, 1)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 0, 3, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 0, 3, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 0, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 0, 1)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 0, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 0, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 1, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 1, 1)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 1, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 1, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 2, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 2, 1)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 2, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 2, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 3, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 3, 1)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 3, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 1, 3, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 0, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 0, 1)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 0, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 0, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 1, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 1, 1)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 1, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 1, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 2, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 2, 1)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 2, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 2, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 3, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 3, 1)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 3, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 2, 3, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 0, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 0, 1)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 0, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 0, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 1, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 1, 1)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 1, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 1, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 2, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 2, 1)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 2, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 2, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 3, 0)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 3, 1)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 3, 2)] = KEYPAIR(No, NO,             Sh, NO            ),
    [CHORD(3, 3, 3, 3)] = KEYPAIR(No, NO,             Sh, NO            ),
};


/*
 * Keychords comprising a THUMB_ACTION(n)-mapped thumb chord FN and
 * zero to four keys from one particular ROW of the upper three rows.
 */
#define FN_CHORD(FN, ROW, COL_PATTERN) (((FN)<<6) | ((ROW)<<4) | (COL_PATTERN))

static action_t fn_chordmap[128] EEMEM = {
    [FN_CHORD(0, 0, 0b0000)] = AC_KP_MINUS,
    /* big hole */
    [FN_CHORD(0, 1, 0b0001)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_NONE | MOD_NONE | MOD_LALT),
    [FN_CHORD(0, 1, 0b0010)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_NONE | MOD_LSFT | MOD_NONE),
    [FN_CHORD(0, 1, 0b0011)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_NONE | MOD_LSFT | MOD_LALT),
    [FN_CHORD(0, 1, 0b0100)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_LGUI | MOD_NONE | MOD_NONE),
    [FN_CHORD(0, 1, 0b0101)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_LGUI | MOD_NONE | MOD_LALT),
    [FN_CHORD(0, 1, 0b0110)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_LGUI | MOD_LSFT | MOD_NONE),
    [FN_CHORD(0, 1, 0b0111)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_LGUI | MOD_LSFT | MOD_LALT),
    [FN_CHORD(0, 1, 0b1000)] = ACTION_MODS_TAP_TOGGLE(MOD_LCTL | MOD_NONE | MOD_NONE | MOD_NONE),
    [FN_CHORD(0, 1, 0b1001)] = ACTION_MODS_TAP_TOGGLE(MOD_LCTL | MOD_NONE | MOD_NONE | MOD_LALT),
    [FN_CHORD(0, 1, 0b1010)] = ACTION_MODS_TAP_TOGGLE(MOD_LCTL | MOD_NONE | MOD_LSFT | MOD_NONE),
    [FN_CHORD(0, 1, 0b1011)] = ACTION_MODS_TAP_TOGGLE(MOD_LCTL | MOD_NONE | MOD_LSFT | MOD_LALT),
    [FN_CHORD(0, 1, 0b1100)] = ACTION_MODS_TAP_TOGGLE(MOD_LCTL | MOD_LGUI | MOD_NONE | MOD_NONE),
    [FN_CHORD(0, 1, 0b1101)] = ACTION_MODS_TAP_TOGGLE(MOD_LCTL | MOD_LGUI | MOD_NONE | MOD_LALT),
    [FN_CHORD(0, 1, 0b1110)] = ACTION_MODS_TAP_TOGGLE(MOD_LCTL | MOD_LGUI | MOD_LSFT | MOD_NONE),
    [FN_CHORD(0, 1, 0b1111)] = ACTION_MODS_TAP_TOGGLE(MOD_LCTL | MOD_LGUI | MOD_LSFT | MOD_LALT),
    /* hole */
    [FN_CHORD(0, 2, 0b0001)] = ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_NONE | MOD_LALT),
    [FN_CHORD(0, 2, 0b0010)] = ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_LSFT | MOD_NONE),
    [FN_CHORD(0, 2, 0b0011)] = ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_LSFT | MOD_LALT),
    [FN_CHORD(0, 2, 0b0100)] = ACTION_MODS_ONESHOT(MOD_NONE | MOD_LGUI | MOD_NONE | MOD_NONE),
    [FN_CHORD(0, 2, 0b0101)] = ACTION_MODS_ONESHOT(MOD_NONE | MOD_LGUI | MOD_NONE | MOD_LALT),
    [FN_CHORD(0, 2, 0b0110)] = ACTION_MODS_ONESHOT(MOD_NONE | MOD_LGUI | MOD_LSFT | MOD_NONE),
    [FN_CHORD(0, 2, 0b0111)] = ACTION_MODS_ONESHOT(MOD_NONE | MOD_LGUI | MOD_LSFT | MOD_LALT),
    [FN_CHORD(0, 2, 0b1000)] = ACTION_MODS_ONESHOT(MOD_LCTL | MOD_NONE | MOD_NONE | MOD_NONE),
    [FN_CHORD(0, 2, 0b1001)] = ACTION_MODS_ONESHOT(MOD_LCTL | MOD_NONE | MOD_NONE | MOD_LALT),
    [FN_CHORD(0, 2, 0b1010)] = ACTION_MODS_ONESHOT(MOD_LCTL | MOD_NONE | MOD_LSFT | MOD_NONE),
    [FN_CHORD(0, 2, 0b1011)] = ACTION_MODS_ONESHOT(MOD_LCTL | MOD_NONE | MOD_LSFT | MOD_LALT),
    [FN_CHORD(0, 2, 0b1100)] = ACTION_MODS_ONESHOT(MOD_LCTL | MOD_LGUI | MOD_NONE | MOD_NONE),
    [FN_CHORD(0, 2, 0b1101)] = ACTION_MODS_ONESHOT(MOD_LCTL | MOD_LGUI | MOD_NONE | MOD_LALT),
    [FN_CHORD(0, 2, 0b1110)] = ACTION_MODS_ONESHOT(MOD_LCTL | MOD_LGUI | MOD_LSFT | MOD_NONE),
    [FN_CHORD(0, 2, 0b1111)] = ACTION_MODS_ONESHOT(MOD_LCTL | MOD_LGUI | MOD_LSFT | MOD_LALT),
    /* hole */
    [FN_CHORD(0, 3, 0b0001)] = AC_6,
    [FN_CHORD(0, 3, 0b0010)] = AC_7,
    [FN_CHORD(0, 3, 0b0011)] = AC_8,
    [FN_CHORD(0, 3, 0b0100)] = AC_9,
    [FN_CHORD(0, 3, 0b0101)] = AC_0,
    [FN_CHORD(0, 3, 0b0110)] = AC_A,
    [FN_CHORD(0, 3, 0b0111)] = AC_B,
    [FN_CHORD(0, 3, 0b1000)] = AC_C,
    [FN_CHORD(0, 3, 0b1001)] = AC_D,
    [FN_CHORD(0, 3, 0b1010)] = AC_E,
    [FN_CHORD(0, 3, 0b1011)] = AC_F,
    [FN_CHORD(0, 3, 0b1100)] = AC_G,
    [FN_CHORD(0, 3, 0b1101)] = AC_H,
    [FN_CHORD(0, 3, 0b1110)] = AC_I,
    [FN_CHORD(0, 3, 0b1111)] = AC_J,
    [FN_CHORD(1, 0, 0b0000)] = AC_KP_PLUS,
    /* big hole */
    [FN_CHORD(1, 1, 0b0001)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_NONE | MOD_NONE | MOD_LALT),
    [FN_CHORD(1, 1, 0b0010)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_NONE | MOD_RSFT | MOD_NONE),
    [FN_CHORD(1, 1, 0b0011)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_NONE | MOD_RSFT | MOD_LALT),
    [FN_CHORD(1, 1, 0b0100)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_RGUI | MOD_NONE | MOD_NONE),
    [FN_CHORD(1, 1, 0b0101)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_RGUI | MOD_NONE | MOD_LALT),
    [FN_CHORD(1, 1, 0b0110)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_RGUI | MOD_RSFT | MOD_NONE),
    [FN_CHORD(1, 1, 0b0111)] = ACTION_MODS_TAP_TOGGLE(MOD_NONE | MOD_RGUI | MOD_RSFT | MOD_LALT),
    [FN_CHORD(1, 1, 0b1000)] = ACTION_MODS_TAP_TOGGLE(MOD_RCTL | MOD_NONE | MOD_NONE | MOD_NONE),
    [FN_CHORD(1, 1, 0b1001)] = ACTION_MODS_TAP_TOGGLE(MOD_RCTL | MOD_NONE | MOD_NONE | MOD_LALT),
    [FN_CHORD(1, 1, 0b1010)] = ACTION_MODS_TAP_TOGGLE(MOD_RCTL | MOD_NONE | MOD_RSFT | MOD_NONE),
    [FN_CHORD(1, 1, 0b1011)] = ACTION_MODS_TAP_TOGGLE(MOD_RCTL | MOD_NONE | MOD_RSFT | MOD_LALT),
    [FN_CHORD(1, 1, 0b1100)] = ACTION_MODS_TAP_TOGGLE(MOD_RCTL | MOD_RGUI | MOD_NONE | MOD_NONE),
    [FN_CHORD(1, 1, 0b1101)] = ACTION_MODS_TAP_TOGGLE(MOD_RCTL | MOD_RGUI | MOD_NONE | MOD_LALT),
    [FN_CHORD(1, 1, 0b1110)] = ACTION_MODS_TAP_TOGGLE(MOD_RCTL | MOD_RGUI | MOD_RSFT | MOD_NONE),
    [FN_CHORD(1, 1, 0b1111)] = ACTION_MODS_TAP_TOGGLE(MOD_RCTL | MOD_RGUI | MOD_RSFT | MOD_LALT),
    /* hole */
    [FN_CHORD(1, 2, 0b0001)] = ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_NONE | MOD_LALT),
    [FN_CHORD(1, 2, 0b0010)] = ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_RSFT | MOD_NONE),
    [FN_CHORD(1, 2, 0b0011)] = ACTION_MODS_ONESHOT(MOD_NONE | MOD_NONE | MOD_RSFT | MOD_LALT),
    [FN_CHORD(1, 2, 0b0100)] = ACTION_MODS_ONESHOT(MOD_NONE | MOD_RGUI | MOD_NONE | MOD_NONE),
    [FN_CHORD(1, 2, 0b0101)] = ACTION_MODS_ONESHOT(MOD_NONE | MOD_RGUI | MOD_NONE | MOD_LALT),
    [FN_CHORD(1, 2, 0b0110)] = ACTION_MODS_ONESHOT(MOD_NONE | MOD_RGUI | MOD_RSFT | MOD_NONE),
    [FN_CHORD(1, 2, 0b0111)] = ACTION_MODS_ONESHOT(MOD_NONE | MOD_RGUI | MOD_RSFT | MOD_LALT),
    [FN_CHORD(1, 2, 0b1000)] = ACTION_MODS_ONESHOT(MOD_RCTL | MOD_NONE | MOD_NONE | MOD_NONE),
    [FN_CHORD(1, 2, 0b1001)] = ACTION_MODS_ONESHOT(MOD_RCTL | MOD_NONE | MOD_NONE | MOD_LALT),
    [FN_CHORD(1, 2, 0b1010)] = ACTION_MODS_ONESHOT(MOD_RCTL | MOD_NONE | MOD_RSFT | MOD_NONE),
    [FN_CHORD(1, 2, 0b1011)] = ACTION_MODS_ONESHOT(MOD_RCTL | MOD_NONE | MOD_RSFT | MOD_LALT),
    [FN_CHORD(1, 2, 0b1100)] = ACTION_MODS_ONESHOT(MOD_RCTL | MOD_RGUI | MOD_NONE | MOD_NONE),
    [FN_CHORD(1, 2, 0b1101)] = ACTION_MODS_ONESHOT(MOD_RCTL | MOD_RGUI | MOD_NONE | MOD_LALT),
    [FN_CHORD(1, 2, 0b1110)] = ACTION_MODS_ONESHOT(MOD_RCTL | MOD_RGUI | MOD_RSFT | MOD_NONE),
    [FN_CHORD(1, 2, 0b1111)] = ACTION_MODS_ONESHOT(MOD_RCTL | MOD_RGUI | MOD_RSFT | MOD_LALT),
    /* hole */
    [FN_CHORD(1, 3, 0b0001)] = AC_F,
    [FN_CHORD(1, 3, 0b0010)] = AC_G,
    [FN_CHORD(1, 3, 0b0011)] = AC_H,
    [FN_CHORD(1, 3, 0b0100)] = AC_I,
    [FN_CHORD(1, 3, 0b0101)] = AC_J,
    [FN_CHORD(1, 3, 0b0110)] = AC_K,
    [FN_CHORD(1, 3, 0b0111)] = AC_L,
    [FN_CHORD(1, 3, 0b1000)] = AC_M,
    [FN_CHORD(1, 3, 0b1001)] = AC_N,
    [FN_CHORD(1, 3, 0b1010)] = AC_O,
    [FN_CHORD(1, 3, 0b1011)] = AC_P,
    [FN_CHORD(1, 3, 0b1100)] = AC_Q,
    [FN_CHORD(1, 3, 0b1101)] = AC_R,
    [FN_CHORD(1, 3, 0b1110)] = AC_S,
    [FN_CHORD(1, 3, 0b1111)] = AC_T,
};

/* 
 * Keychords on the bottom row
 */
#define THUMB_CHORD(FN1, SHIFT, FN2) ((FN1) | ((SHIFT) << 1) | ((FN2) << 2))
#define THUMB_ACTION(FN) ACTION(ACT_THUMB_CHORD, (FN))
#define THUMB_SHIFT (ACT_THUMB_CHORD << 12 | MOD_LSFT << 8)

static const action_t thumb_chordmap[8] PROGMEM = {
    [THUMB_CHORD(0, 0, 0)] = AC_NO,
    [THUMB_CHORD(0, 0, 1)] = THUMB_ACTION(0),
    [THUMB_CHORD(0, 1, 0)] = {.code = THUMB_SHIFT},
    [THUMB_CHORD(0, 1, 1)] = AC_NO,
    [THUMB_CHORD(1, 0, 0)] = THUMB_ACTION(1),
    [THUMB_CHORD(1, 0, 1)] = AC_ESCAPE,
    [THUMB_CHORD(1, 1, 0)] = AC_NO,
    [THUMB_CHORD(1, 1, 1)] = AC_NO,
};

/* 
 * Change bit patterns (representing keys pressed on columns) from two
 * bit per colum like d0c0b0a0, 0d0c0b0a, or ddccbbaa into one bit per
 * column like 00rrdcba.  The two rr bits represent the (single) row
 * the keys belonged to.
 */
static uint8_t
squeeze_chord (uint8_t c)
{
    uint8_t even_bits, odd_bits, row;

    even_bits = (c & 1) | (c & 1<<2) >> 1 | (c & 1<<4) >> 2 | (c & 1<<6) >> 3;
    odd_bits = (c & 1<<1) >> 1 | (c & 1<<3) >> 2 | (c & 1<<5) >> 3 | (c & 1<<7) >> 4;
    row = (c>>6 | c>>4 | c>>2 | c) & 3;
    if (even_bits && odd_bits && even_bits != odd_bits)
        return 0;               /* no multi-row chords allowed here */
    return even_bits | odd_bits | row<<4;
}

static void
emit_key(uint8_t mods, uint8_t keycode)
{
    register_mods(mods);
    register_code(keycode);
    unregister_code(keycode);
    unregister_mods(mods);
}

/* /\* translates chord to action *\/ */
/* static action_t chord_to_action(uint8_t chord) */
/* { */
/*     switch (chord) { */
/*         case KC_A ... KC_EXSEL: */
/*         case KC_LCTRL ... KC_RGUI: */
/*             return (action_t)ACTION_KEY(keycode); */
/*             break; */
/*         case KC_SYSTEM_POWER ... KC_SYSTEM_WAKE: */
/*             return (action_t)ACTION_USAGE_SYSTEM(KEYCODE2SYSTEM(keycode)); */
/*             break; */
/*         case KC_AUDIO_MUTE ... KC_WWW_FAVORITES: */
/*             return (action_t)ACTION_USAGE_CONSUMER(KEYCODE2CONSUMER(keycode)); */
/*             break; */
/*         case KC_MS_UP ... KC_MS_ACCEL2: */
/*             return (action_t)ACTION_MOUSEKEY(keycode); */
/*             break; */
/*         case KC_TRNS: */
/*             return (action_t)ACTION_TRANSPARENT; */
/*             break; */
/*         case KC_BOOTLOADER: */
/*             clear_keyboard(); */
/*             wait_ms(50); */
/*             bootloader_jump(); // not return */
/*             break; */
/*         default: */
/*             return (action_t)ACTION_NO; */
/*             break; */
/*     } */
/*     return (action_t)ACTION_NO; */
/* } */
void
action_function(keyrecord_t *record, uint8_t id, uint8_t opt)
{
    static uint8_t fng_chord = 0, thb_chord = 0;
    static int8_t keys_down = 0;
    static bool ready = true;
    keyevent_t e = record->event;
    uint8_t row = opt, col = id;

    if (e.pressed) {
        keys_down++;
        if (ready) {
            if (row == THUMB_ROW) { /* collect bottom row keys seperately */
                thb_chord |= 1 << col;
            } else {            /* finger keys; top three rows */
                uint8_t byte_pos = col * 2;

                fng_chord &= ~(3 << byte_pos);
                fng_chord |= row << byte_pos;
            }
            dprintf("   COLLECTING THUMBCH:%X FINGERCH:%X ROW:%d COL:%d\n", thb_chord, fng_chord, row, col);
        }
    } else {
        if (ready) {
            keypair_t keypair = {0};
            action_t thb_state = {0};
            static uint8_t onesh_mods = 0, tgl_mods = 0;
            
            led(8, TOGGLE);
            thb_state.code = pgm_read_word((uint16_t *) thumb_chordmap + thb_chord);
            eeprom_read_block(&keypair, chordmap + fng_chord, sizeof(keypair));
            if (thb_state.code == KC_NO) {
                emit_key((keypair.mods_u & ~Ag) | (keypair.mods_u & Ag) << 3 |
                         onesh_mods | tgl_mods, keypair.code_u);
                onesh_mods = 0;
            } else if (thb_state.code == THUMB_SHIFT) {
                emit_key((keypair.mods_s & ~Ag) | (keypair.mods_s & Ag) << 3 |
                         onesh_mods | tgl_mods, keypair.code_s);
                onesh_mods = 0;
            } else if (thb_state.key.kind == ACT_MODS) {
                emit_key(thb_state.key.mods | onesh_mods | tgl_mods,
                         thb_state.key.code);
                onesh_mods = 0;
            } else if (thb_state.kind.id == ACT_THUMB_CHORD) {
                action_t fn_act;
                uint8_t fn_chord = squeeze_chord(fng_chord) |
                    ((thb_state.key.code & 1) << 6);

                fn_act.code = eeprom_read_word((uint16_t *) fn_chordmap + fn_chord);
                dprintf("   FN_ACTION:: squeezedchord:%X code:%d mods%d RAW-CHMP@%d:%X\n",
                        fn_chord, fn_act.key.code, fn_act.key.mods | onesh_mods, fn_chord, fn_act.code);
                if (fn_act.kind.id == ACT_LMODS_TAP ||
                    fn_act.kind.id == ACT_RMODS_TAP) {
                    uint8_t mods = fn_act.key.mods;

                    if (fn_act.kind.id == ACT_RMODS_TAP)
                        mods <<= 4;
                    switch (fn_act.key.code) {
                    case MODS_ONESHOT:
                        onesh_mods = mods;
                        break;
                    case MODS_TAP_TOGGLE:
                        tgl_mods ^= mods;
                        break;
                    }
                } else {
                    emit_key(fn_act.key.mods | onesh_mods | tgl_mods,
                             fn_act.key.code);
                    onesh_mods = 0;
                }
            }
            dprintf("   done THUMBCODE:%X FINGERCH:%X ROW:%d COL: %d keypaircode=%d/%d keypairmods=%d/%d\n",
                    thb_state.code, fng_chord, row, col, keypair.code_u, keypair.code_s, keypair.mods_u, keypair.mods_s);
            ready = false;
        }
        if (--keys_down == 0) {
            ready = true;
            fng_chord = 0;
            thb_chord = 0;
        }
    }
}
