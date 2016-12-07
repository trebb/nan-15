EESchema Schematic File Version 2
LIBS:power
LIBS:device
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:microcontrollers
LIBS:dsp
LIBS:microchip
LIBS:analog_switches
LIBS:motorola
LIBS:texas
LIBS:intel
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:display
LIBS:cypress
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:bottom-plate-cache
EELAYER 25 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "NaN-15"
Date ""
Rev "0.2"
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L GND #PWR01
U 1 1 582C2D63
P 4850 2700
F 0 "#PWR01" H 4850 2450 50  0001 C CNN
F 1 "GND" H 4855 2527 50  0000 C CNN
F 2 "" H 4850 2700 50  0000 C CNN
F 3 "" H 4850 2700 50  0000 C CNN
	1    4850 2700
	1    0    0    -1  
$EndComp
$Comp
L TEST_1P W1
U 1 1 582C2FD5
P 4850 2500
F 0 "W1" H 4908 2574 50  0000 L CNN
F 1 "TEST_1P" H 4908 2529 50  0001 L CNN
F 2 "Mounting_Holes:MountingHole_2.2mm_M2_Pad" H 5050 2500 50  0001 C CNN
F 3 "" H 5050 2500 50  0000 C CNN
	1    4850 2500
	1    0    0    -1  
$EndComp
Wire Wire Line
	4850 2500 4850 2700
$Comp
L GND #PWR02
U 1 1 582C30A9
P 5200 2700
F 0 "#PWR02" H 5200 2450 50  0001 C CNN
F 1 "GND" H 5205 2527 50  0000 C CNN
F 2 "" H 5200 2700 50  0000 C CNN
F 3 "" H 5200 2700 50  0000 C CNN
	1    5200 2700
	1    0    0    -1  
$EndComp
$Comp
L TEST_1P W2
U 1 1 582C30AF
P 5200 2500
F 0 "W2" H 5258 2574 50  0000 L CNN
F 1 "TEST_1P" H 5258 2529 50  0001 L CNN
F 2 "Mounting_Holes:MountingHole_2.2mm_M2_Pad" H 5400 2500 50  0001 C CNN
F 3 "" H 5400 2500 50  0000 C CNN
	1    5200 2500
	1    0    0    -1  
$EndComp
Wire Wire Line
	5200 2500 5200 2700
$Comp
L GND #PWR03
U 1 1 582C30D4
P 5500 2700
F 0 "#PWR03" H 5500 2450 50  0001 C CNN
F 1 "GND" H 5505 2527 50  0000 C CNN
F 2 "" H 5500 2700 50  0000 C CNN
F 3 "" H 5500 2700 50  0000 C CNN
	1    5500 2700
	1    0    0    -1  
$EndComp
$Comp
L TEST_1P W3
U 1 1 582C30DA
P 5500 2500
F 0 "W3" H 5558 2574 50  0000 L CNN
F 1 "TEST_1P" H 5558 2529 50  0001 L CNN
F 2 "Mounting_Holes:MountingHole_2.2mm_M2_Pad" H 5700 2500 50  0001 C CNN
F 3 "" H 5700 2500 50  0000 C CNN
	1    5500 2500
	1    0    0    -1  
$EndComp
Wire Wire Line
	5500 2500 5500 2700
$Comp
L PWR_FLAG #FLG04
U 1 1 582C30FC
P 4450 2600
F 0 "#FLG04" H 4450 2695 50  0001 C CNN
F 1 "PWR_FLAG" H 4450 2824 50  0000 C CNN
F 2 "" H 4450 2600 50  0000 C CNN
F 3 "" H 4450 2600 50  0000 C CNN
	1    4450 2600
	1    0    0    -1  
$EndComp
Wire Wire Line
	4450 2600 4850 2600
Connection ~ 4850 2600
$EndSCHEMATC
