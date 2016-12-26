/*
Copyright 2012 Jun Wako <wakojun@gmail.com>

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

#include <avr/io.h>
#include "stdint.h"
#include "led.h"

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
