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

var (
	outFilename = flag.String("o", "chordmap.svg", "output SVG filename (\"-\" for stdout)")
	inFilename  = flag.String("i", "chordmap.txt", "input filename (\"-\" for stdin)")
	width       = flag.Int("w", 200, "width in mm")
	height      = flag.Int("h", 290, "height in mm")
)

const (
	keySize        = 450
	keySep         = 60
	keyradius      = 60
	padSize        = keySize*4 + keySep*3
	pageMargin     = 1200
	padSep         = 600
	frameThickness = 120
	frameSize      = padSize + frameThickness + 2*keySep
	colorRed       = "darkred"
	colorGreen     = "darkgreen"
	colorGrey      = "darkslategrey"
	frameStyle     = "fill:white;stroke-opacity:0.2"
	relStyle       = "stroke-width:50;fill:white;stroke-opacity:0.2"
	pressStyle     = "stroke-width:30;fill-opacity:0.2;stroke-opacity:0.4"
	legendStyle    = "text-anchor:middle;font-family:'DejaVu Sans';fill:black"
	qStyle         = "font-family:'DejaVu Sans';font-size:400px" +
		";dominant-baseline:middle;text-anchor:middle"
	headerStyle = "text-anchor:left;font-family:'DejaVu Sans'" +
		";dominant-baseline:bottom;fill:black"
	padHeaderStyle = "font-family:'DejaVu Sans';font-size:400px;font-weight:bold" +
		";dominant-baseline:bottom"
	intMax = int(^uint(0) >> 1)
)

var (
	translatableNames = map[string]string{
		"bspace": "⌫",
		"down":   "▽",
		"left":   "◁",
		"right":  "▷",
		"space":  "␣",
		"up":     "△",
	}
	specialKeys = map[string]string{
		"Left Shift": "Left Shift", // synthetic
		"again":      "Again",
		"appl":       "Appl",
		"capslock":   "Caps Lock",
		"copy":       "Copy",
		"copz":       "Copy",
		"cut":        "Cut",
		"delete":     "Delete",
		"end":        "End",
		"enter":      "Enter",
		"escape":     "Escape",
		"f1":         "F1",
		"f10":        "F10",
		"f11":        "F11",
		"f12":        "F12",
		"f13":        "F13",
		"f14":        "F14",
		"f15":        "F15",
		"f16":        "F16",
		"f17":        "F17",
		"f18":        "F18",
		"f19":        "F19",
		"f2":         "F2",
		"f20":        "F20",
		"f21":        "F21",
		"f22":        "F22",
		"f23":        "F23",
		"f24":        "F24",
		"f3":         "F3",
		"f4":         "F4",
		"f5":         "F5",
		"f6":         "F6",
		"f7":         "F7",
		"f8":         "F8",
		"f9":         "F9",
		"find":       "Find",
		"help":       "Help",
		"home":       "Home",
		"insert":     "Insert",
		"int1":       "Intl 1",
		"int2":       "Intl 2",
		"int3":       "Intl 3",
		"int4":       "Intl 4",
		"int5":       "Intl 5",
		"int6":       "Intl 6",
		"int7":       "Intl 7",
		"int8":       "Intl 8",
		"int9":       "Intl 9",
		"kp 0":       "Keypad 0",
		"kp 1":       "Keypad 1",
		"kp 2":       "Keypad 2",
		"kp 3":       "Keypad 3",
		"kp 4":       "Keypad 4",
		"kp 5":       "Keypad 5",
		"kp 6":       "Keypad 6",
		"kp 7":       "Keypad 7",
		"kp 8":       "Keypad 8",
		"kp 9":       "Keypad 9",
		"kp aster":   "Keypad *",
		"kp comma":   "Keypad Comma",
		"kp dot":     "Keypad Dot",
		"kp enter":   "Keypad Enter",
		"kp equal":   "Keypad =",
		"kp minus":   "Keypad -",
		"kp plus":    "Keypad +",
		"kp slash":   "Keypad /",
		"lang1":      "Lang 1",
		"lang2":      "Lang 2",
		"lang3":      "Lang 3",
		"lang4":      "Lang 4",
		"lang5":      "Lang 5",
		"lang6":      "Lang 6",
		"lang7":      "Lang 7",
		"lang8":      "Lang 8",
		"lang9":      "Lang 9",
		"macro 0":    "store/play macro 0",
		"macro 1":    "store/play macro 1",
		"macro 2":    "store/play macro 2",
		"macro 3":    "store/play macro 3",
		"macro 4":    "store/play macro 4",
		"macro 5":    "store/play macro 5",
		"macro 6":    "store/play macro 6",
		"macro 7":    "store/play macro 7",
		"macro lr":   "macro layer",
		"mouse lr":   "mouse layer",
		"mute":       "Mute",
		"nav lr":     "navigation layer",
		"numlock":    "Num Lock",
		"numpad lr":  "number pad layer",
		"paste":      "Paste",
		"pause":      "Pause",
		"pgdown":     "Page Down",
		"pgup":       "Page Up",
		"power":      "Power",
		"prnt chds":  "type chordmap",
		"pscreen":    "Print Screen",
		"rec macro":  "start macro record",
		"reset kbd":  "keyboard reset",
		"scrolllck":  "Scroll Lock",
		"stop":       "Stop",
		"swap chds":  "start chord swap",
		"sysreq":     "SysReq",
		"szsreq":     "SysReq",
		"tab":        "Tab",
		"voldown":    "Volume Down",
		"volup":      "Volume Up",
	}
	mapLegend = []chordPad{
		{
			section:   finger,
			legend:    "red 1",
			suppressQ: true,
		}, {
			section:     finger,
			legend:      "red 2",
			suppressQ:   true,
			header:      "swappable",
			headerStyle: padHeaderStyle + ";text-anchor:middle;fill:" + colorRed,
		}, {
			section:   fn,
			legend:    "green 1",
			suppressQ: true,
		}, {
			section:     fn,
			legend:      "green 2",
			suppressQ:   true,
			header:      "swappable",
			headerStyle: padHeaderStyle + ";text-anchor:middle;fill:" + colorGreen,
		}, {
			section:   finger,
			legend:    "red 1",
			suppressQ: true,
		}, {
			section:     fn,
			legend:      "green 2",
			suppressQ:   true,
			header:      "unswappable",
			headerStyle: padHeaderStyle + ";text-anchor:middle",
		}, {
			section:     thumb,
			legend:      "grey",
			suppressQ:   true,
			header:      "\u00A0immutable",
			headerStyle: padHeaderStyle + ";text-anchor:left;fill:" + colorGrey,
		}, {
			section: finger,
			chord: chord{
				{},
				{},
				{false, true},
			},
			legend:      "excellent",
			suppressQ:   false,
			header:      "\u00A0\u00A0\u00A0ease",
			headerStyle: padHeaderStyle + ";text-anchor:left",
		}, {
			section: finger,
			chord: chord{
				{},
				{},
				{true},
				{false, true, true, true},
			},
			legend:      "fair",
			suppressQ:   false,
			header:      "\u00A0\u00A0\u00A0ease",
			headerStyle: padHeaderStyle + ";text-anchor:left",
		}, {
			section: finger,
			chord: chord{
				{},
				{false, false, true},
				{true},
				{false, true, false, true},
			},
			legend:      "poor",
			suppressQ:   false,
			header:      "\u00A0\u00A0\u00A0ease",
			headerStyle: padHeaderStyle + ";text-anchor:left",
		},
	}
)

const (
	thumb = iota
	finger
	fn
)

type (
	chord    [5][4]bool
	section  int
	chordPad struct {
		chord            chord
		section          section
		legend           string
		legendIsChar     bool
		modifiers        []string
		modifierDuration string
		header           string
		headerStyle      string
		suppressQ        bool
	}
	sectionHeader struct{ header string }
	newPage       struct{}
	chordMap      struct {
		outFilename          string
		outFile              *os.File
		canvas               *svg.SVG
		width, height, xPads int
		x, y                 int
		nPage, nPad          int
		chordPads            chan interface{}
		done                 chan struct{}
		yPageOffset          int
	}
	step struct {
		step    int
		colSpan int
		valid   bool
	}
)

func nextFilename(old string) string {
	fnRx := regexp.MustCompile(`(.*?)([0-9]*)\.([a-z]*)`)
	parts := fnRx.FindStringSubmatch(old)
	numLen := len(parts[2])
	num, _ := strconv.Atoi(parts[2])
	return fmt.Sprintf("%s%0*d.%s", parts[1], numLen, num+1, parts[3])
}

func (cm *chordMap) newSVG() {
	cm.nPad = 0
	cm.yPageOffset = pageMargin
	cm.canvas = svg.New(cm.outFile)
	cm.canvas.StartviewUnit(cm.width, cm.height, "mm", 0, 0, cm.width*100, cm.height*100)
	cm.canvas.Title(fmt.Sprintf("NaN-15 chordmap (p. %d)", cm.nPage+1))
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
	cm.outFilename = nextFilename(*outFilename)
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
			switch t := cp.(type) {
			case chordPad:
				if cm.chordPad(t) {
				} else {
					cm.switchOutFile()
					cm.chordPad(t)
				}
			case sectionHeader:
				if cm.sectionHeader(t) {
				} else {
					cm.switchOutFile()
					cm.sectionHeader(t)
				}
			case newPage:
				cm.y = 0
				cm.switchOutFile()
			}
		}
		cm.canvas.End()
		cm.done <- struct{}{}
	}()
	return
}

func (cm *chordMap) sectionHeader(h sectionHeader) bool {
	if cm.y+2*(padSize+pageMargin) > cm.height*100 {
		cm.y = 0
		return false
	}

	cm.yPageOffset += padSep
	remainder := (cm.xPads - cm.nPad%cm.xPads) % cm.xPads
	cm.nPad += remainder
	if remainder > 0 {
		cm.yPageOffset += padSize + padSep
	}
	cm.canvas.Text(pageMargin, cm.y+cm.yPageOffset, h.header,
		headerStyle+fmt.Sprintf(";font-size:%dpx", keySize))
	return true
}

func (cm *chordMap) chordPad(cp chordPad) bool {
	xPad := cm.nPad % cm.xPads
	yPad := cm.nPad / cm.xPads

	var keyState, keypadColor string
	cm.x = pageMargin + xPad*(padSize+padSep)
	cm.y = pageMargin + yPad*(padSize+padSep) + cm.yPageOffset
	if cm.y+padSize+pageMargin > cm.height*100 {
		return false
	}
	xFrame := cm.x - frameThickness/2 - keySep
	yFrame := cm.y - frameThickness/2 - keySep
	switch cp.section {
	case finger:
		keypadColor = colorRed
	case fn:
		keypadColor = colorGreen
	case thumb:
		keypadColor = colorGrey
	}
	fc, tc := cp.chord.flat()
	cm.canvas.Group(fmt.Sprintf("title=\"%d%d%d%d %d%d%d\"",
		fc[0], fc[1], fc[2], fc[3], tc[0], tc[1], tc[2]))
	cm.canvas.Roundrect(xFrame, yFrame, frameSize, frameSize, keyradius, keyradius,
		frameStyle+fmt.Sprintf(";stroke:%s;stroke-width:%d", keypadColor, frameThickness))
	for row := 0; row < 4; row++ {
		for col, keycol := 0, 0; keycol < 4; col, keycol = col+1, keycol+1 {
			if cp.chord[row+1][col] {
				keyState = pressStyle + fmt.Sprintf(";stroke:%s;fill:%s",
					keypadColor, keypadColor)
			} else {
				keyState = relStyle + fmt.Sprintf(";stroke:%s", keypadColor)
			}
			if row == 3 && col == 1 {
				cm.canvas.Roundrect(cm.x+keycol*(keySize+keySep), cm.y+row*(keySize+keySep),
					2*keySize+keySep, keySize, keyradius, keyradius, keyState)
				keycol++
			} else {
				cm.canvas.Roundrect(cm.x+keycol*(keySize+keySep), cm.y+row*(keySize+keySep),
					keySize, keySize, keyradius, keyradius, keyState)
			}
		}
	}
	if !cp.suppressQ {
		cm.canvas.Text(cm.x+padSize+keySep, cm.y+padSize+keySep,
			fmt.Sprintf("%d", cp.chord.quality()), qStyle+
				fmt.Sprintf(";fill:%s", keypadColor))
	}
	nMods := len(cp.modifiers)
	if cp.legendIsChar {
		cm.canvas.Text(cm.x+padSize/2, cm.y+padSize*3/4, cp.legend,
			legendStyle+
				fmt.Sprintf(";font-size:%dpx;font-weight:bold"+
					";fill-opacity:0;stroke-opacity:0.5;stroke:black;stroke-width:%dpx",
					padSize-keySize, keySep/2))
	} else if nMods == 0 {
		s := strings.SplitN(cp.legend, " ", 3)
		nParts := len(s)
		for i, part := range s {
			yOffset := keySize / 2 * (2*i - nParts + 1)
			cm.canvas.Text(cm.x+padSize/2, cm.y+padSize/2+yOffset, part,
				legendStyle+
					fmt.Sprintf(";fill-opacity:0.5;dominant-baseline:middle;font-size:%dpx",
						keySize))
		}
	} else if cp.modifierDuration != "" {
		var (
			i   int
			mod string
		)
		for i, mod = range cp.modifiers {
			yOffset := keySize / 2 * (2*i - nMods)
			cm.canvas.Text(cm.x+padSize/2, cm.y+padSize/2+yOffset, strings.Title(mod),
				legendStyle+
					fmt.Sprintf(";fill-opacity:0.5;dominant-baseline:middle;font-size:%dpx",
						keySize))
		}
		yOffset := keySize / 2 * (2*(i+1) - nMods)
		cm.canvas.Text(cm.x+padSize/2, cm.y+padSize/2+yOffset, cp.modifierDuration,
			legendStyle+fmt.Sprintf(";fill-opacity:0.5;dominant-baseline:middle;font-size:%dpx",
				keySize))
	}
	if cp.header != "" {
		cm.canvas.Text(cm.x-padSep/2, cm.y+padSize, cp.header, cp.headerStyle)
	}

	cm.canvas.Gend()
	cm.nPad++
	return true
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
				if cpLo.legend == "no" && strings.Contains(string(r[7:11]), "s") {
					cpLo.legend = "Left Shift"
				}
			}
			chordPads = append(chordPads, cpLo)
			cpUp.chord[4][1] = true
			if r[37] != ' ' {
				cpUp.legend = string(r[37])
				cpUp.legendIsChar = true
			} else {
				cpUp.legend = strings.TrimRight(string(r[39:]), " ")
				if cpUp.legend == "no" && strings.Contains(string(r[28:32]), "s") {
					cpUp.legend = "Left Shift"
				}
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
	cm.put(sectionHeader{header: "Simple Chords"})
	for _, cp := range chordPads {
		l, ok := specialKeys[cp.legend]
		if ok {
			cp.legend = l
			cm.put(cp)
		} else if cp.legendIsChar {
			cm.put(cp)
		}
	}
	cm.put(sectionHeader{header: "Modifiers"})
	for _, cp := range chordPads {
		if cp.legend == "modifiers" {
			cm.put(cp)
		}
	}
	cm.put(newPage{})
	cm.put(sectionHeader{header: "Unused Chords"})
	for _, cp := range chordPads {
		if cp.legend == "no" {
			cp.legend = "[empty]"
			fc, tc := cp.chord.flat()
			zerofc := [...]int{0, 0, 0, 0}
			zerotc := [...]int{0, 0, 0}
			if !(fc == zerofc && tc == zerotc) {
				cm.put(cp)
			}
		}
	}
	cm.put(sectionHeader{header: "Customization"})
	for _, cp := range mapLegend {
		cm.put(cp)
	}
	if err := scanner.Err(); err != nil {
		log.Fatal(err)
	}
	close(cm.chordPads)
	<-cm.done
}

type (
	byAlphabet []chordPad
	byCategory []chordPad
	byLen      []chordPad
)

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

func (c chord) flat() (fc [4]int, tc [3]int) {
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

func abs(x int) int {
	if x < 0 {
		return -x
	}
	return x
}

func (c chord) quality() int {
	var q int
	min, max := c.stepBounds()
	_, sumAbs := c.stepSums()
	minRow, maxRow, nCols := c.rowBounds()
	switch {
	case max.step == 0: // single row
		q = nCols
		q += rowQuality(minRow)
	case sumAbs == 1: // two adjacent rows, no zigzag
		q = 6
		q += nCols
		q += rowQuality(minRow)
	case maxRow-minRow <= 1 && abs(max.step) == 1: // two adjacent rows, zigzag
		q = 8
		q += nCols
		q += rowQuality(minRow)
	case sumAbs == 2 && abs(min.step) == 1 && abs(max.step) == 1: // straight diagonal
		q = 15
	case c.maxSlope() <= 1: // three rows, no steep steps
		q = 14
		q += nCols
		if c.hasBottomCorners() {
			q += 2
		}
	case c.maxSlope() > 1: // awkward row steps between adjacent columns
		q = 16
		q += nCols
		if c.hasBottomCorners() {
			q += 2
		}
	}
	return q * 9 / 22
}

func rowQuality(row int) int {
	switch row {
	case 1:
		return 1
	case 3:
		return 2
	}
	return 0
}

func (c chord) steps() (steps [3]step) {
	fc, _ := c.flat()
	for i := 1; i < len(fc); i++ {
		if fc[i-1] != 0 {
			for j := i; j < len(fc); j++ {
				if fc[j] != 0 {
					steps[i-1].step = fc[j] - fc[i-1]
					steps[i-1].valid = true
					steps[i-1].colSpan = j - (i - 1)
					break
				}
			}
		}
	}
	return
}

func (c chord) stepBounds() (min, max step) {
	min.step = intMax
	for _, s := range c.steps() {
		if s.valid && abs(s.step) > abs(max.step) {
			max = s
		}
		if s.valid && abs(s.step) < abs(min.step) {
			min = s
		}
	}
	return
}

func (c chord) stepSums() (sum, sumAbs int) {
	for _, s := range c.steps() {
		if s.valid {
			sum += s.step
			sumAbs += abs(s.step)
		}
	}
	return
}

func (c chord) maxSlope() (maxSlope int) {
	for _, s := range c.steps() {
		if s.valid {
			slope := abs(s.step) / s.colSpan
			if slope > maxSlope {
				maxSlope = slope
			}
		}
	}
	return
}

func (c chord) rowBounds() (min, max, nCols int) {
	fc, _ := c.flat()
	min = intMax
	for _, col := range fc {
		if col != 0 {
			if col < min {
				min = col
			}
			if col > max {
				max = col
			}
			nCols++
		}
	}
	return
}

func (c chord) hasBottomCorners() bool {
	fc, _ := c.flat()
	return fc[0] == 3 || fc[3] == 3
}
