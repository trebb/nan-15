export GOPATH = $(shell pwd)

chordmap: chordmap.go
	go get -d -x
	go build chordmap.go

default-chordmap.svg: chordmap Makefile
	./chordmap -o default-chordmap.svg -w 400 -h 650

clean:
	go clean
	rm -rf src
	rm -f *.svg
