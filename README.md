NaN-15 -- a 15-key chorded computer keyboard
============================================

![Finished product](http://i.imgur.com/UGswMU8.jpg)

Firmware
--------

The firmware is based on [TMK](https://github.com/tmk/tmk_keyboard);
see ./firmware/.

In addition to its main (chorded) input mode, a few __special layers__
are available:

- a number pad,
- a set of navigation keys,
- a mouse emulation layer,
- a macro pad. (Macro recording is done in chorded mode.)

On these layers, keycodes are registered in the usual way, i.e. once
a key is pressed.

In __chorded mode__,

1. one or more keys (the "chord") are pressed down in no particular
   order;
2. once the first key of the chord is released, the keycode mapped to
   the chord is being registered;
3. any further activity is ignored until all remaining keys are
   released.

The chorded-mode keyboard is divided into sections, and there are a
few restrictions as to the possible chords.  These restrictions and
the sectioning make it possible to fit the chordmaps into EEPROM where
they are on-the-fly editable.

The __principal section__ comprises the top three rows where a chord can
consist of up to one key from each of the four columns. This section
is switchable from its lower to an upper level, providing room for 511
different keycodes.  Each keycode is stored with its own set of the
four modfiers Right Alt, Left Alt, Left Shift, and Left Control.

The __Fn section__ occupies the same 12 keys as the principal section.
It has two levels that are bound to the function chords Fn0 and Fn1.
A chord in the Fn section can consist of one to four keys of one
particular row, providing room for 92 different keycodes, each with
its own full set of eight modifiers.  The Fn section is used for two
flavours of modifier chords, one-shot and toggled, and a few auxiliary
chords for customization, layer switching, and chordmap printing.

The three keys on the __bottom row__ form their own little section.  The
seven chords located here select the upper level of the principal
section or one of the Fn section levels, and perform a keyboard reset.

The special layers and the bottom row are immutable, but both
principal section and Fn section can be __customized__ at any time by
swapping chords.

There is storage for eight __chord macros__.  Each can record up to six
chords (including modifiers).  Recorded macros are played back through
dedicated macro chords in the principal section, or by keys of the
macro pad.

Both chordmap customizations and macro definitions __persist through
power cycles__.


Self-Documentation
------------------

The keyboard is able to __print__ its current chordmap tables by
"typing" them to the host computer.  Depending on the host computer's
keymap, the output of this operation may appear a bit mangled
([Example, affected by QUERTZ layout
setting](https://trebb.github.io/nan-15/chordmap.txt)).

See ./firmware/cheatsheet-generator/ for a tool that converts the
textual chordmap into a graphical representation (Example: [page
1](https://trebb.github.io/nan-15/default-chordmap1.svg), [page
2](https://trebb.github.io/nan-15/default-chordmap2.svg)), and for a
similar tool that extracts documentation of the [keyboard
LEDs](https://trebb.github.io/nan-15/leds.svg) from the firmware
source code.


Hardware
--------

See ./hardware/.

The main parts are three PCBs (top plate, PCB proper, and bottom
plate) and a 3D-printed frame.

It uses Cherry MX-style switches and an ATmega32U2-MU MCU.  There are
12 LEDs signalling keyboard state and errors.

![Parts](http://i.imgur.com/Hco3Ytr.jpg)

The finished product draws about 18 mA.
