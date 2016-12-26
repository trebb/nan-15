#include "keymap_common.h"

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

/* enum function_id { */
/*     ALTGR, */
/* }; */

const uint16_t PROGMEM fn_actions[] = {
    /* [0] = ACTION_LAYER_MOMENTARY(1), */
    /* [1] = ACTION_LAYER_MOMENTARY(2), */
};

/* void action_function(keyrecord_t *record, uint8_t id, uint8_t opt) */
/* { */
/*     switch (id) { */
/*     case ALTGR: */
/*         if (record->event.pressed) { */
/*             add_mods(MOD_BIT(KC_RALT)); */
/*             layer_on(4); */
/*         } else { */
/*             del_mods(MOD_BIT(KC_RALT)); */
/*             layer_off(4); */
/*         } */
/*         break; */
/*     } */
/* } */
