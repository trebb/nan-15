export GOPATH = $(shell pwd)

all: leds.svg default-chordmap

chordmap: chordmap.go
	go get -d -x
	go build chordmap.go

leds: leds.go
	go get -d -x
	go build leds.go

default-chordmap: chordmap Makefile
	./chordmap -o default-chordmap.svg -w 460 -h 660

leds.svg: leds ../nan-15_chord.c
	./leds -o $@

%.pdf: %.svg
	inkscape --export-pdf $@ $<

clean:
	go clean
	rm -rf src
	rm -f *.svg
