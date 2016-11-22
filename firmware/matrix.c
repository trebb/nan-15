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

/*
 * scan matrix
 */
#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <util/delay.h>
#include "print.h"
#include "debug.h"
#include "util.h"
#include "matrix.h"


#ifndef DEBOUNCE
#   define DEBOUNCE	5
#endif
static uint8_t debouncing = DEBOUNCE;

/* matrix state(1:on, 0:off) */
static matrix_row_t matrix[MATRIX_ROWS];
static matrix_row_t matrix_debouncing[MATRIX_ROWS];

static uint8_t read_rows(void);
static void init_rows(void);
static void unselect_cols(void);
static void select_col(uint8_t row);


inline uint8_t
matrix_rows(void)
{
    return MATRIX_ROWS;
}

inline uint8_t
matrix_cols(void)
{
    return MATRIX_COLS;
}

void
matrix_init(void)
{
    // initialize row and col
    unselect_cols();
    init_rows();

    // initialize matrix state: all keys off
    for (uint8_t i=0; i < MATRIX_ROWS; i++) {
        matrix[i] = 0;
        matrix_debouncing[i] = 0;
    }
}

uint8_t
matrix_scan(void)
{
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        select_col(col);
        _delay_us(30);  // without this wait read unstable value.
        uint8_t rows = read_rows();
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            bool prev_bit = matrix_debouncing[row] & ((matrix_row_t)1<<col);
            bool curr_bit = rows & (1<<row);
            if (prev_bit != curr_bit) {
                matrix_debouncing[row] ^= ((matrix_row_t)1<<col);
                if (debouncing) {
                    debug("bounce!: "); debug_hex(debouncing); debug("\n");
                }
                debouncing = DEBOUNCE;
            }
            unselect_cols();
        }
    }

    if (debouncing) {
        if (--debouncing) {
            _delay_ms(1);
        } else {
            for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
                matrix[i] = matrix_debouncing[i];
            }
        }
    }

    return 1;
}

bool
matrix_is_modified(void)
{
    if (debouncing) return false;
    return true;
}

inline bool
matrix_is_on(uint8_t row, uint8_t col)
{
    return (matrix[row] & ((matrix_row_t)1<<col));
}

inline matrix_row_t
matrix_get_row(uint8_t row)
{
    return matrix[row];
}

void
matrix_print(void)
{
    print("\nr/c 0123456789ABCDEF\n");
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        phex(row); print(": ");
        pbin_reverse16(matrix_get_row(row));
        print("\n");
    }
}

uint8_t
matrix_key_count(void)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        count += bitpop16(matrix[i]);
    }
    return count;
}

/* Row pin configuration
 * row: 0  1  2  3
 * pin: B3 B2 D3 C2
 */
static void
init_rows(void)
{
    // Input with pull-up(DDR:0, PORT:1)
    DDRB  &= ~0b00001100;
    PORTB |=  0b00001100;
    DDRC  &= ~0b00000100;
    PORTC |=  0b00000100;
    DDRD  &= ~0b00001000;
    PORTD |=  0b00001000;
}

static uint8_t
read_rows(void)
{
    return
        (PINB&(1<<3) ? 0 : (1<<0)) |
        (PINB&(1<<2) ? 0 : (1<<1)) |
        (PIND&(1<<3) ? 0 : (1<<2)) |
        (PINC&(1<<2) ? 0 : (1<<3));
}


/* Column pin configuration
 * col: 0  1  2  3
 * pin: C7 C4 D2 D1
 */
static void
unselect_cols(void)
{
    // Hi-Z(DDR:0, PORT:0) to unselect
    DDRC  |= 0b10010000;
    PORTC |= 0b10010000;
    DDRD  |= 0b00000110;
    PORTD |= 0b00000110;
}

static void
select_col(uint8_t col)
{
    // Output low(DDR:1, PORT:0) to select
    switch (col) {
        case 0x0:
            DDRC  |=  (1<<7);
            PORTC &= ~(1<<7);
            break;
        case 0x1:
            DDRC  |=  (1<<4);
            PORTC &= ~(1<<4);
            break;
        case 0x2:
            DDRD  |=  (1<<2);
            PORTD &= ~(1<<2);
            break;
        case 0x3:
            DDRB  |=  (1<<1);
            PORTB &= ~(1<<1);
            break;
    }
}
