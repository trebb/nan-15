#include <stdint.h>
#include <stdbool.h>
#include <avr/pgmspace.h>
#include "action_layer.h"


/* NaN-15 hardware test keymap definition
 *
 * ,---------------.
 * | 7 | 8 | 9 | T |
 * |---------------|
 * | 4 | 5 | 6 | M |
 * |---------------|
 * | 1 | 2 | 3 | B |
 * |---------------|
 * | L |   0   | R |
 * `---------------'
 */
#define KEYMAP(         \
    K00, K01, K02, K03, \
    K10, K11, K12, K13, \
    K20, K21, K22, K23, \
    K30,      K32, K33 \
    ) { \
    { KC_##K00, KC_##K01, KC_##K02, KC_##K03, }, \
    { KC_##K10, KC_##K11, KC_##K12, KC_##K13, }, \
    { KC_##K20, KC_##K21, KC_##K22, KC_##K23, }, \
    { KC_##K30, KC_NO,    KC_##K32, KC_##K33 }  \
}

const uint8_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = KEYMAP(
                 7,   8,   9,   T,
                 4,   5,   6,   M,
                 1,   2,   3,   B,
                 L,        0,   R),
    [1] = KEYMAP(
                 TRNS,TRNS,TRNS,TRNS,
                 TRNS,TRNS,TRNS,TRNS,
                 TRNS,TRNS,TRNS,TRNS,
                 TRNS,     TRNS,TRNS),
};

const uint16_t PROGMEM fn_actions[] = {
};
