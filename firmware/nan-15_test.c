#include "action_layer.h"
#include "led.h"
#include <avr/pgmspace.h>


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

/*
 * LED pin configuration
 * LED: NumLock           ScrollLock CapsLock
 * pin: C6 C5 D6 D5 D4 D0            B7 B6 B5 B4 B1 B0
 */

void
led_set(uint8_t usb_led)
{
    if (usb_led & (1<<USB_LED_NUM_LOCK)) {
        DDRC |=  (1<<6);
        PORTC |= (1<<6);
        DDRC |=  (1<<5);
        PORTC |= (1<<5);
        DDRD |=  (1<<6);
        PORTD |= (1<<6);
        DDRD |=  (1<<5);
        PORTD |= (1<<5);
        DDRD |=  (1<<4);
        PORTD |= (1<<4);
        DDRD |=  (1<<0);
        PORTD |= (1<<0);
    } else {
        DDRC |=  (1<<6);
        PORTC &= ~(1<<6);
        DDRC |=  (1<<5);
        PORTC &= ~(1<<5);
        DDRD |=  (1<<6);
        PORTD &= ~(1<<6);
        DDRD |=  (1<<5);
        PORTD &= ~(1<<5);
        DDRD |=  (1<<4);
        PORTD &= ~(1<<4);
        DDRD |=  (1<<0);
        PORTD &= ~(1<<0);
    }
    if (usb_led & (1<<USB_LED_SCROLL_LOCK)) {
    } else {
    }
    if (usb_led & (1<<USB_LED_CAPS_LOCK)) {
        DDRB |=  (1<<7);
        PORTB |= (1<<7);
        DDRB |=  (1<<6);
        PORTB |= (1<<6);
        DDRB |=  (1<<5);
        PORTB |= (1<<5);
        DDRB |=  (1<<4);
        PORTB |= (1<<4);
        DDRB |=  (1<<1);
        PORTB |= (1<<1);
        DDRB |=  (1<<0);
        PORTB |= (1<<0);
    } else {
        DDRB |=  (1<<7);
        PORTB &= ~(1<<7);
        DDRB |=  (1<<6);
        PORTB &= ~(1<<6);
        DDRB |=  (1<<5);
        PORTB &= ~(1<<5);
        DDRB |=  (1<<4);
        PORTB &= ~(1<<4);
        DDRB |=  (1<<1);
        PORTB &= ~(1<<1);
        DDRB |=  (1<<0);
        PORTB &= ~(1<<0);
    }
}
