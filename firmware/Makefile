#----------------------------------------------------------------------------
# On command line:
#
# make all = Make software.
#
# make clean = Clean out built project files.
#
# make coff = Convert ELF to AVR COFF.
#
# make extcoff = Convert ELF to AVR Extended COFF.
#
# make program = Download the hex file to the device.
#                Please customize your programmer settings(PROGRAM_CMD)
#
# make teensy = Download the hex file to the device, using teensy_loader_cli.
#               (must have teensy_loader_cli installed).
#
# make dfu = Download the hex file to the device, using dfu-programmer (must
#            have dfu-programmer installed).
#
# make flip = Download the hex file to the device, using Atmel FLIP (must
#             have Atmel FLIP installed).
#
# make dfu-ee = Download the eeprom file to the device, using dfu-programmer
#               (must have dfu-programmer installed).
#
# make flip-ee = Download the eeprom file to the device, using Atmel FLIP
#                (must have Atmel FLIP installed).
#
# make debug = Start either simulavr or avarice as specified for debugging, 
#              with avr-gdb or avr-insight as the front end for debugging.
#
# make filename.s = Just compile filename.c into the assembler code only.
#
# make filename.i = Create a preprocessed source file for use in submitting
#                   bug reports to the GCC project.
#
# To rebuild project do "make clean" then "make all".
#----------------------------------------------------------------------------

KEYMAP ?= test

# Target file name (without extension).
TARGET = nan-15_$(KEYMAP)_lufa

# Directory common source filess exist
TMK_DIR = tmk_keyboard/tmk_core

# Directory keyboard dependent files exist
TARGET_DIR = .

# project specific files
SRC =	matrix.c

SRC := nan-15_$(KEYMAP).c $(SRC)

CONFIG_H = config.h

PROGRAM_CMD = avrdude -p $(MCU) -c usbasp -U flash:w:$(TARGET).hex

# MCU name
#MCU = at90usb1287
MCU = atmega32u2

# Processor frequency.
#     This will define a symbol, F_CPU, in all source code files equal to the
#     processor frequency in Hz. You can then use this symbol in your source code to
#     calculate timings. Do NOT tack on a 'UL' at the end, this will be done
#     automatically to create a 32-bit value in your source code.
#
#     This will be an integer division of F_USB below, as it is sourced by
#     F_USB after it has run through any CPU prescalers. Note that this value
#     does not *change* the processor frequency - it should merely be updated to
#     reflect the processor speed set externally so that the code can use accurate
#     software delays.
F_CPU = 16000000


#
# LUFA specific
#
# Target architecture (see library "Board Types" documentation).
ARCH = AVR8

# Input clock frequency.
#     This will define a symbol, F_USB, in all source code files equal to the
#     input clock frequency (before any prescaling is performed) in Hz. This value may
#     differ from F_CPU if prescaling is used on the latter, and is required as the
#     raw input clock is fed directly to the PLL sections of the AVR for high speed
#     clock generation for the USB and other AVR subsections. Do NOT tack on a 'UL'
#     at the end, this will be done automatically to create a 32-bit value in your
#     source code.
#
#     If no clock division is performed on the input clock inside the AVR (via the
#     CPU clock adjust registers or the clock division fuses), this will be equal to F_CPU.
F_USB = $(F_CPU)

# Interrupt driven control endpoint task(+60)
OPT_DEFS += -DINTERRUPT_CONTROL_ENDPOINT


# Boot Section Size in *bytes*
#   Teensy halfKay   512
#   Teensy++ halfKay 1024
#   Atmel DFU loader 4096
#   LUFA bootloader  4096
#   USBaspLoader     2048
OPT_DEFS += -DBOOTLOADER_SIZE=4096


BOARD = NONE
FLASH_SIZE_KB = 32
BOOT_SECTION_SIZE_KB = 4
export ARCH F_USB F_CPU MCU BOARD FLASH_SIZE_KB BOOT_SECTION_SIZE_KB

bootloaders-help:
	$(MAKE) -C tmk_keyboard/tmk_core/protocol/lufa/LUFA-git/Bootloaders help

bootloaders:
	$(MAKE) -e -C tmk_keyboard/tmk_core/protocol/lufa/LUFA-git/Bootloaders


bootloader-dfu: bootloaders
	avrdude -c usbasp -pm32u2 -U flash:w:tmk_keyboard/tmk_core/protocol/lufa/LUFA-git/Bootloaders/DFU/BootloaderDFU.hex

bootloader-mass-storage: bootloaders
	avrdude -c usbasp -pm32u2 -U flash:w:tmk_keyboard/tmk_core/protocol/lufa/LUFA-git/Bootloaders/MassStorage/BootloaderMassStorage.hex


bootloaders-clean:
	$(MAKE) -C tmk_keyboard/tmk_core/protocol/lufa/LUFA-git/Bootloaders clean

fuses:
	avrdude -c usbasp -pm32u2 -U lfuse:w:0xff:m -U hfuse:w:0xd9:m -U efuse:w:0xf4:m 

# Build Options
#   comment out to disable the options.
#
#BOOTMAGIC_ENABLE = yes	# Virtual DIP switch configuration(+1000)
MOUSEKEY_ENABLE = yes	# Mouse keys(+4700)
# EXTRAKEY_ENABLE = yes	# Audio control and System control(+450)
#CONSOLE_ENABLE = yes	# Console for debug(+400)
#COMMAND_ENABLE = yes    # Commands for debug and configuration
#SLEEP_LED_ENABLE = yes  # Breathing sleep LED during USB suspend
#NKRO_ENABLE = yes	# USB Nkey Rollover - not yet supported in LUFA


# Optimize size but this may cause error "relocation truncated to fit"
#EXTRALDFLAGS = -Wl,--relax

# Search Path
VPATH += $(TARGET_DIR)
VPATH += $(TMK_DIR)

include $(TMK_DIR)/protocol/lufa.mk
include $(TMK_DIR)/common.mk
include $(TMK_DIR)/rules.mk

cflow: cflow-$(KEYMAP).out

cflow-$(KEYMAP).out: *.c *.h Makefile
	ln -s $(TMK_DIR)/common common
	ln -s $(TMK_DIR)/protocol protocol
	cflow -Tnbm keyboard_task \
		$(SRC) \
		--cpp='avr-gcc -E -I. -I./common -I./protocol' \
		$(OPT_DEFS) \
		--symbol __inline:=inline \
		--symbol __inline__:=inline \
		--symbol __const__:=const \
		--symbol __const:=const \
		--symbol __restrict:=restrict \
		--symbol __extension__:qualifier \
		--symbol __attribute__:wrapper \
		--symbol __asm__:wrapper \
		--symbol __nonnull:wrapper \
		--symbol __wur:wrapper | \
	fold -s | \
	awk 'BEGIN{FS="-"} \
		/^ *[0-9]+ .*-/ { \
			prefix=$$1; \
			print $$0; \
		} \
		!/^[\| 0-9]+ [\\\\+]?-/ { \
			gsub("[0-9\\\\]", " ", prefix); \
			sub("\\+", "|", prefix); \
			print prefix, "> ", $$0 \
		}' | \
	fold -s | \
	awk 'BEGIN{FS=">"} \
		/^[\| ]+*> / { \
			prefix = $$1; \
			print $$0; \
		} \
		/^ *[0-9]+[\| ]+[\\\\+]?-/ {\
			print $$0; \
		} \
		!/^[\| 0-9]+ [\\\\+]?-|        / { \
			print prefix "> ", $$0; \
		}' > $@
	rm common protocol

DEVICE_VER != awk \
	'/\#define DEVICE_VER/{printf "%02x.%02x", ($$3 - $$3%256)/256, $$3%256}' \
	config.h

git-tag:
	git tag $(DEVICE_VER)

clean:
	rm -rf cflow-$(KEYMAP).out
	test ! -d common || rm common
	test ! -d protocol || rm protocol



