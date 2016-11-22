/*
Copyright 2012,2013 Jun Wako <wakojun@gmail.com>

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
#ifndef KEYMAP_COMMON_H
#define KEYMAP_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <avr/pgmspace.h>
#include "action_layer.h"
#include "action_util.h"


extern const uint8_t keymaps[][MATRIX_ROWS][MATRIX_COLS];
extern const uint16_t fn_actions[];


/* NaN-15 keymap definition macro
 * Layout: 15key
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
    K30,      K32, K33, \
    ) { \
    { KC_##K00, KC_##K01, KC_##K02, KC_##K03, }, \
    { KC_##K10, KC_##K11, KC_##K12, KC_##K13, }, \
    { KC_##K20, KC_##K21, KC_##K22, KC_##K23, }, \
    { KC_##K30,           KC_##K32, KC_##K33, } \
}

#endif
