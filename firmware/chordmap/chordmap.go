package main

import (
	"bufio"
	"flag"
	"fmt"
	"github.com/ajstarks/svgo"
	"log"
	"os"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"unicode"
)

var outFilename = flag.String("o", "chordmap.svg", "output SVG filename (\"-\" for stdout)")
var inFilename = flag.String("i", "chordmap.txt", "input filename (\"-\" for stdin)")
var width = flag.Int("w", 200, "width in mm")
var height = flag.Int("h", 290, "height in mm")

const (
	keySize        = 450
	keySep         = 60
	keyradius      = 60
	padSize        = keySize*4 + keySep*3
	pageMargin     = padSize / 2
	padSep         = 600
	frameThickness = 120
	frameSize      = padSize + frameThickness + 2*keySep
	frameStyle     = "stroke:lightgray;fill:white"
	pressStyle     = "stroke:gray;stroke-width:30;fill:gainsboro"
	relStyle       = "stroke:lightgray;stroke-width:50;fill:white"
	legendStyle    = "text-anchor:middle;font-family:'DejaVu Sans'"
)

type chord [5][4]bool

type section int

const (
	thumb = iota
	finger
	fn
)

type chordPad struct {
	chord            chord
	section          section
	legend           string
	legendIsChar     bool
	modifiers        []string
	modifierDuration string
}

type formatting struct{}

type op int

const (
	newSection = iota
	newPage
)

type chordMap struct {
	outFilename          string
	outFile              *os.File
	canvas               *svg.SVG
	width, height, xPads int
	nPage, nPad          int
	chordPads            chan interface{}
	done                 chan struct{}
	yPageOffset          int
}

func nextFilename(old string) string {
	fnRx := regexp.MustCompile(`(.*?)([0-9]*)\.([a-z]*)`)
	parts := fnRx.FindStringSubmatch(old)
	numLen := len(parts[2])
	num, _ := strconv.Atoi(parts[2])
	return fmt.Sprintf("%s%0*d.%s", parts[1], numLen, num+1, parts[3])
}

func (cm *chordMap) newSVG() {
	cm.nPad = 0
	cm.yPageOffset = 0
	cm.canvas = svg.New(cm.outFile)
	cm.canvas.StartviewUnit(cm.width, cm.height, "mm", 0, 0, cm.width*100, cm.height*100)
	cm.canvas.Title(fmt.Sprintf("NaN-15 chordmap (p. %d)", cm.nPage))
}

func (cm *chordMap) switchOutFile() {
	var err error
	if cm.outFile == os.Stdout {
		return
	}
	cm.canvas.End()
	cm.outFile.Close()
	cm.nPage++
	cm.outFilename = nextFilename(cm.outFilename)
	cm.outFile, err = os.Create(cm.outFilename)
	if err != nil {
		log.Fatal(err)
	}
	cm.newSVG()
}

func newChordMap() (cm chordMap) {
	cm.width, cm.height = *width, *height
	cm.outFilename = *outFilename
	cm.xPads = (100*cm.width - 2*pageMargin + padSep) / (padSize + padSep)
	cm.done = make(chan struct{})
	cm.chordPads = make(chan interface{})
	go func() {
		var err error
		if cm.outFilename == "-" {
			cm.outFile = os.Stdout
		} else {
			cm.outFile, err = os.Create(cm.outFilename)
			if err != nil {
				log.Fatal(err)
			}
			defer cm.outFile.Close()
		}
		cm.newSVG()
		for cp := range cm.chordPads {
			switch x := cp.(type) {
			case chordPad:
				if cm.chordPad(x) {
				} else {
					cm.switchOutFile()
					cm.chordPad(x)
				}
			case op:
				switch x {
				case newSection:
					cm.nPad += (cm.xPads - cm.nPad%cm.xPads) % cm.xPads
					cm.yPageOffset += padSep
				case newPage:
					cm.switchOutFile()
				}
			}
		}
		cm.canvas.End()
		cm.done <- struct{}{}
	}()
	return
}

func (cm *chordMap) put(cp interface{}) {
	cm.chordPads <- cp
}

func main() {
	flag.Parse()
	var inFile *os.File
	var err error
	if *inFilename == "-" {
		inFile = os.Stdin
	} else {
		inFile, err = os.Open(*inFilename)
		if err != nil {
			log.Fatal(err)
		}
		defer inFile.Close()
	}
	specialKeys := map[string]bool{
		"appl":      true,
		"capslock":  true,
		"change lr": true,
		"delete":    true,
		"end":       true,
		"enter":     true,
		"escape":    true,
		"f1":        true,
		"f10":       true,
		"f11":       true,
		"f12":       true,
		"f2":        true,
		"f3":        true,
		"f4":        true,
		"f5":        true,
		"f6":        true,
		"f7":        true,
		"f8":        true,
		"f9":        true,
		"home":      true,
		"insert":    true,
		"macro 0":   true,
		"macro 1":   true,
		"macro 2":   true,
		"macro 3":   true,
		"macro 4":   true,
		"macro 5":   true,
		"macro 6":   true,
		"macro 7":   true,
		"numlock":   true,
		"pgdown":    true,
		"pgup":      true,
		"prnt chds": true,
		"rec macro": true,
		"reset kbd": true,
		"scrolllck": true,
		"swap chds": true,
		"tab":       true,
	}
	translatableNames := map[string]string{
		"bspace": "⌫",
		"down":   "▽",
		"left":   "◁",
		"right":  "▷",
		"space":  "␣",
		"up":     "△",
	}
	var chordPads []chordPad
	scanner := bufio.NewScanner(inFile)
	for scanner.Scan() {
		r := []rune(scanner.Text())
		if len(r) > 37 && r[0] == '*' {
			var cpLo, cpUp chordPad
			cpLo.section = finger
			cpUp.section = finger
			for col, digit := range r[2:6] {
				row, _ := strconv.Atoi(string(digit))
				cpLo.chord[row][col] = true
				cpUp.chord[row][col] = true
			}
			if r[16] != ' ' {
				cpLo.legend = string(r[16])
				cpLo.legendIsChar = true
			} else {
				cpLo.legend = strings.TrimRight(string(r[18:27]), " ")
			}
			chordPads = append(chordPads, cpLo)
			cpUp.chord[4][1] = true
			if r[37] != ' ' {
				cpUp.legend = string(r[37])
				cpUp.legendIsChar = true
			} else {
				cpUp.legend = strings.TrimRight(string(r[39:]), " ")
			}
			chordPads = append(chordPads, cpUp)
		} else if len(r) > 23 && r[0] == '*' && r[2] == ' ' {
			var cp chordPad
			cp.section = thumb
			for col, digit := range r[3:6] {
				row, _ := strconv.Atoi(string(digit))
				cp.chord[row][col] = true
			}
			cp.legend = strings.TrimRight(string(r[24:]), " ")
			if len(cp.legend) == 0 {
				cp.legend = "DUMMY"
			}
			chordPads = append(chordPads, cp)

		} else if len(r) > 25 && r[0] == '*' {
			var cp chordPad
			cp.section = fn
			for col, digit := range r[4:8] {
				row, _ := strconv.Atoi(string(digit))
				cp.chord[row][col] = true
			}
			switch fn, _ := strconv.Atoi(string(r[2])); fn {
			case 0:
				cp.chord[4][0] = true
			case 1:
				cp.chord[4][2] = true
			}
			cp.legend = strings.TrimRight(string(r[26:]), " ")
			if strings.Contains(string(r[9:13]), "a") {
				cp.modifiers = append(cp.modifiers, "L Alt")
			}
			if strings.Contains(string(r[9:13]), "s") {
				cp.modifiers = append(cp.modifiers, "L Shift")
			}
			if strings.Contains(string(r[9:13]), "g") {
				cp.modifiers = append(cp.modifiers, "L GUI")
			}
			if strings.Contains(string(r[9:13]), "c") {
				cp.modifiers = append(cp.modifiers, "L Ctrl")
			}
			if strings.Contains(string(r[14:18]), "a") {
				cp.modifiers = append(cp.modifiers, "R Alt")
			}
			if strings.Contains(string(r[14:18]), "s") {
				cp.modifiers = append(cp.modifiers, "R Shift")
			}
			if strings.Contains(string(r[14:18]), "g") {
				cp.modifiers = append(cp.modifiers, "R GUI")
			}
			if strings.Contains(string(r[14:18]), "c") {
				cp.modifiers = append(cp.modifiers, "R Ctrl")
			}
			if r[19] == '1' {
				cp.modifierDuration = "(sticky)"
			}
			if r[19] == 't' {
				cp.modifierDuration = "(toggle)"
			}
			chordPads = append(chordPads, cp)
		}

	}
	for i, _ := range chordPads {
		if r, ok := translatableNames[chordPads[i].legend]; ok {
			chordPads[i].legend = r
			chordPads[i].legendIsChar = true
		}
	}
	cm := newChordMap()
	sort.Stable(byAlphabet(chordPads))
	sort.Stable(byCategory(chordPads))
	sort.Stable(byLen(chordPads))

	for _, cp := range chordPads {
		_, ok := specialKeys[cp.legend]
		if ok || cp.legendIsChar {
			cm.put(cp)
		}
	}
	cm.put(op(newSection))
	for _, cp := range chordPads {
		if cp.legend == "modifiers" {
			cm.put(cp)
		}
	}
	cm.put(op(newPage))
	for _, cp := range chordPads {
		if cp.legend == "no" {
			cp.legend = "[empty]"
			cm.put(cp)
		}
	}
	if err := scanner.Err(); err != nil {
		log.Fatal(err)
	}
	close(cm.chordPads)
	<-cm.done
}

type byAlphabet []chordPad
type byCategory []chordPad
type byLen []chordPad

func (p byAlphabet) Len() int { return len(p) }
func (p byAlphabet) Less(i, j int) bool {
	return strings.ToLower(p[i].legend) < strings.ToLower(p[j].legend)
}
func (p byAlphabet) Swap(i, j int) { p[i], p[j] = p[j], p[i] }
func (p byLen) Len() int           { return len(p) }
func (p byLen) Less(i, j int) bool { return p[i].legendIsChar && !p[j].legendIsChar }
func (p byLen) Swap(i, j int)      { p[i], p[j] = p[j], p[i] }
func (p byCategory) Len() int      { return len(p) }
func (p byCategory) Less(i, j int) bool {
	ri := []rune(p[i].legend)[0]
	rj := []rune(p[j].legend)[0]
	switch {
	case unicode.IsDigit(ri):
		return !unicode.IsDigit(rj)
	case unicode.IsLetter(ri):
		return !(unicode.IsLetter(rj) || unicode.IsDigit(rj))
	case unicode.IsPunct(ri):
		return !(unicode.IsPunct(rj) || unicode.IsLetter(rj) || unicode.IsDigit(rj))
	case unicode.IsSymbol(ri):
		return !(unicode.IsSymbol(rj) || unicode.IsPunct(rj) || unicode.IsLetter(rj) || unicode.IsDigit(rj))
	default:
		return false
	}
}
func (p byCategory) Swap(i, j int) { p[i], p[j] = p[j], p[i] }

func flatChord(c chord) (fc [4]int, tc [3]int) {
	for row := 0; row < 3; row++ {
		for col := 0; col < 4; col++ {
			if c[row+1][col] {
				fc[col] = row + 1
			}
		}
	}
	for col := 0; col < 4; col++ {
		if c[4][col] {
			tc[col] = 4
		}
	}
	return
}

func (cm *chordMap) chordPad(cp chordPad) bool {
	xPad := cm.nPad % cm.xPads
	yPad := cm.nPad / cm.xPads

	var keyState, legendColor string
	x := pageMargin + xPad*(padSize+padSep)
	y := pageMargin + yPad*(padSize+padSep) + cm.yPageOffset
	if y+padSize+pageMargin > cm.height*100 {
		return false
	}
	xFrame := x - frameThickness/2 - keySep
	yFrame := y - frameThickness/2 - keySep
	switch cp.section {
	case finger:
		legendColor = "darkred"
	case fn:
		legendColor = "olivedrab"
	case thumb:
		legendColor = "dimgray"
	}
	fc, tc := flatChord(cp.chord)
	cm.canvas.Group(fmt.Sprintf("title=\"%d%d%d%d %d%d%d\"",
		fc[0], fc[1], fc[2], fc[3], tc[0], tc[1], tc[2]))
	cm.canvas.Roundrect(xFrame, yFrame, frameSize, frameSize, keyradius, keyradius,
		frameStyle+fmt.Sprintf(";stroke-width:%d", frameThickness))
	for row := 0; row < 4; row++ {
		for col, keycol := 0, 0; keycol < 4; col, keycol = col+1, keycol+1 {
			if cp.chord[row+1][col] {
				keyState = pressStyle
			} else {
				keyState = relStyle
			}
			if row == 3 && col == 1 {
				cm.canvas.Roundrect(x+keycol*(keySize+keySep), y+row*(keySize+keySep),
					2*keySize+keySep, keySize, keyradius, keyradius, keyState)
				keycol++
			} else {
				cm.canvas.Roundrect(x+keycol*(keySize+keySep), y+row*(keySize+keySep),
					keySize, keySize, keyradius, keyradius, keyState)
			}
		}
	}
	nMods := len(cp.modifiers)
	if cp.legendIsChar {
		cm.canvas.Text(x+padSize/2, y+padSize*3/4, cp.legend,
			legendStyle+fmt.Sprintf(
				";font-size:%dpx;font-weight:bold;fill-opacity:0.1;fill:%s;stroke:%s;stroke-width:%dpx",
				padSize-keySize, legendColor, legendColor, keySep/3))
	} else if nMods == 0 {
		cm.canvas.Text(x+padSize/2, y+padSize/2, strings.Title(cp.legend),
			legendStyle+fmt.Sprintf(";dominant-baseline:middle;font-size:%dpx;fill:%s",
				keySize, legendColor))
	} else if cp.modifierDuration != "" {
		var (
			i   int
			mod string
		)
		for i, mod = range cp.modifiers {
			yOffset := keySize / 2 * (2*i - nMods)
			cm.canvas.Text(x+padSize/2, y+padSize/2+yOffset, strings.Title(mod),
				legendStyle+fmt.Sprintf(";dominant-baseline:middle;font-size:%dpx;fill:%s",
					keySize, legendColor))
		}
		yOffset := keySize / 2 * (2*(i+1) - nMods)
		cm.canvas.Text(x+padSize/2, y+padSize/2+yOffset, cp.modifierDuration,
			legendStyle+fmt.Sprintf(";dominant-baseline:middle;font-size:%dpx;fill:%s",
				keySize, legendColor))
	}
	cm.canvas.Gend()
	cm.nPad++
	return true
}
