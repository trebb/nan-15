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
)

var (
	outFilename = flag.String("o", "leds.svg", "output SVG filename (\"-\" for stdout)")
	width       = flag.Int("w", 200, "width in mm")
	height      = flag.Int("h", 290, "height in mm")
	ledState    = [...]led{
		0:  led{x: padSize/2 - (keySize + keySep), y: padSize, color: redColor, on: false},
		1:  led{x: padSize/2 + (keySize + keySep), y: padSize, color: redColor, on: false},
		2:  led{x: padSize, y: padSize/2 + (keySize + keySep), color: greenColor, on: false},
		3:  led{x: padSize, y: padSize / 2, color: redColor, on: false},
		4:  led{x: padSize, y: padSize/2 - (keySize + keySep), color: greenColor, on: false},
		5:  led{x: padSize / 2, y: keySize / 6 * 1, color: greenColor, on: false},
		6:  led{x: padSize / 2, y: keySize / 6 * 3, color: redColor, on: false},
		7:  led{x: padSize / 2, y: keySize / 6 * 5, color: greenColor, on: false},
		8:  led{x: padSize / 2, y: padSize / 2, color: redColor, on: false},
		9:  led{x: 0, y: padSize/2 - (keySize + keySep), color: greenColor, on: false},
		10: led{x: 0, y: padSize / 2, color: redColor, on: false},
		11: led{x: 0, y: padSize/2 + (keySize + keySep), color: greenColor, on: false},
	}
)

const (
	pageTitle             = "NaN-15 LED signals"
	keySize               = 450
	keySep                = 60
	keyradius             = 60
	padSize               = keySize*4 + keySep*3
	blinkPatternHeight    = 400
	blinkPatternThickness = 100
	pageMargin            = padSize / 2
	padSep                = 1000
	frameThickness        = 120
	frameSize             = padSize + frameThickness + 2*keySep
	frameStyle            = "stroke:lightgrey;fill:white"
	keyStyle              = "stroke:lightgrey;stroke-width:30;fill:white"
	bsUnit                = 3
	redColor              = "crimson"
	greenColor            = "limeGreen"
	offColor              = "whitesmoke"
	legendColor           = "black"
	legendHeight          = 400
	legendStyle           = "text-anchor:middle;font-family:'DejaVu Sans';font-stretch:semi-condensed" +
		";dominant-baseline:bottom;font-size:300px;fill:" + legendColor
	headerStyle = "text-anchor:left;font-family:'DejaVu Sans';font-weight:bold;font-stretch:normal" +
		";dominant-baseline:bottom;font-size:400px;fill:" + legendColor
)

type (
	bp  struct{ on, off, cycles int }
	ls  struct{ leds, bp string }
	led struct {
		x, y  int
		color string
		on    bool
	}
	ledSets       map[string][]int
	blinkPatterns map[string]bp
	ledSignals    map[string]ls
	blinkOnLine   struct{ x0, x1 int }
	ledSignalMap  struct {
		outFile       *os.File
		canvas        *svg.SVG
		xPads         int
		nPad          int
		yPageOffset   int
		ledSets       ledSets
		blinkPatterns blinkPatterns
		ledSignals    ledSignals
	}
)

func (b bp) blinkSequence() (s []blinkOnLine, ok bool) {
	var bol blinkOnLine
	if b.cycles == 0 {
		ok = false
		return
	}
	ok = true
	for x0, x1, c := 0, b.on*bsUnit, b.cycles; x0 < padSize && x1 < padSize && c != 0; x0, x1, c = x1+b.off*bsUnit, x1+(b.off+b.on)*bsUnit, c-1 {
		bol.x0 = x0
		bol.x1 = x1
		s = append(s, bol)
	}
	return
}

func main() {
	var (
		lsm     ledSignalMap
		inFile  *os.File
		legends []string
	)
	flag.Parse()
	ledSetRx := regexp.MustCompile(` *\[LEDS_([A-Z_]+)\] *= \{\.len.+\{([0-9, ]+)\}\},`)
	ledSetLedsRx := regexp.MustCompile(`([0-9]+)[, ]*`)
	blinkPatternRx := regexp.MustCompile(`^#define BLINK_([A-Z_]+) ([0-9]+), ([0-9]+), ([0-9A-Z]+)`)
	ledSignalRx := regexp.MustCompile(`^#define [A-Z_]+_ON LEDS_([A-Z_]+), BLINK_([A-Z_]+) .*/\* (.*) \*/`)
	lsm.xPads = (100**width - 2*pageMargin + padSep) / (padSize + padSep)
	lsm.ledSets = make(ledSets)
	lsm.blinkPatterns = make(blinkPatterns)
	lsm.ledSignals = make(ledSignals)
	inFilename := "../nan-15_chord.c"
	inFile, err := os.Open(inFilename)
	if err != nil {
		log.Fatal(err)
	}
	defer inFile.Close()
	scanner := bufio.NewScanner(inFile)
	for scanner.Scan() {
		r := scanner.Text()
		ledSet := ledSetRx.FindStringSubmatch(r)
		blinkPattern := blinkPatternRx.FindStringSubmatch(r)
		ledSignal := ledSignalRx.FindStringSubmatch(r)
		if len(ledSet) > 0 {
			var leds []int
			for _, l := range ledSetLedsRx.FindAllStringSubmatch(ledSet[2], -1) {
				ledNumber, _ := strconv.Atoi(l[1])
				leds = append(leds, ledNumber)
			}
			lsm.ledSets[ledSet[1]] = leds
		}
		if len(blinkPattern) > 0 {
			var p bp
			p.on, _ = strconv.Atoi(blinkPattern[2])
			p.off, _ = strconv.Atoi(blinkPattern[3])
			p.cycles, err = strconv.Atoi(blinkPattern[4])
			if err != nil {
				p.cycles = -1
			}
			lsm.blinkPatterns[blinkPattern[1]] = p
		}
		if len(ledSignal) > 0 {
			lsm.ledSignals[ledSignal[3]] = ls{leds: ledSignal[1], bp: ledSignal[2]}
		}

	}
	if *outFilename == "-" {
		lsm.outFile = os.Stdout
	} else {
		lsm.outFile, err = os.Create(*outFilename)
		if err != nil {
			log.Fatal(err)
		}
		defer lsm.outFile.Close()
	}
	lsm.newSVG()
	for legend, _ := range lsm.ledSignals {
		legends = append(legends, legend)
	}
	sort.Strings(legends)
	for _, legend := range legends {
		lsm.putLs(legend, lsm.ledSignals[legend])
	}
	lsm.canvas.End()
}

func (lsm *ledSignalMap) putLs(legend string, l ls) {
	var ledColor1, ledColor2 string
	var xStart, xEnd int
	xPad := lsm.nPad % lsm.xPads
	yPad := lsm.nPad / lsm.xPads
	usedLedColors := make(map[string]bool)
	x := pageMargin + xPad*(padSize+padSep)
	y := pageMargin + yPad*(legendHeight+padSize+blinkPatternHeight+padSep) + lsm.yPageOffset
	xFrame := x - frameThickness/2 - keySep
	yFrame := y - frameThickness/2 - keySep
	yUpper := y + padSize + frameThickness + blinkPatternHeight/2
	yLower := yUpper + blinkPatternThickness
	yEllipsis := (yUpper + yLower) / 2
	lsm.canvas.Roundrect(xFrame, yFrame, frameSize, frameSize, keyradius, keyradius,
		frameStyle+fmt.Sprintf(";stroke-width:%d", frameThickness))
	for row := 0; row < 4; row++ {
		for col, keycol := 0, 0; keycol < 4; col, keycol = col+1, keycol+1 {
			if row == 3 && col == 1 {
				lsm.canvas.Roundrect(x+keycol*(keySize+keySep), y+row*(keySize+keySep),
					2*keySize+keySep, keySize, keyradius, keyradius, keyStyle)
				keycol++
			} else {
				lsm.canvas.Roundrect(x+keycol*(keySize+keySep), y+row*(keySize+keySep),
					keySize, keySize, keyradius, keyradius, keyStyle)
			}
		}
	}
	for _, l := range ledState {
		lsm.canvas.Circle(x+l.x, y+l.y,
			keySize/6, fmt.Sprintf("fill:%s;stroke:%s;stroke-width:7", offColor, legendColor))
	}
	for _, ledNr := range lsm.ledSets[l.leds] {
		lsm.canvas.Circle(x+ledState[ledNr].x, y+ledState[ledNr].y,
			keySize/6, fmt.Sprintf("fill:%s", ledState[ledNr].color))
		usedLedColors[ledState[ledNr].color] = true
	}
	if usedLedColors[greenColor] {
		ledColor1 = greenColor
	} else {
		ledColor1 = redColor
	}
	if usedLedColors[redColor] {
		ledColor2 = redColor
	} else {
		ledColor2 = greenColor
	}
	bs, ok := lsm.blinkPatterns[l.bp].blinkSequence()
	lsm.canvas.Text(x+padSize/2, y-frameThickness-(legendHeight/2), legend,
		legendStyle)
	if ok {
		for _, bol := range bs {
			xStart = xFrame + bol.x0
			xEnd = xFrame + bol.x1
			lsm.canvas.Line(xStart, yUpper, xEnd, yUpper,
				fmt.Sprintf("stroke:%s;stroke-width:%d", ledColor1, blinkPatternThickness))
			lsm.canvas.Line(xStart, yLower, xEnd, yLower,
				fmt.Sprintf("stroke:%s;stroke-width:%d", ledColor2, blinkPatternThickness))
		}
	}
	if lsm.blinkPatterns[l.bp].cycles == -1 {
		lsm.canvas.Line(xEnd+50, yEllipsis, xFrame+frameSize, yEllipsis,
			fmt.Sprintf("stroke:%s;stroke-width:%d;stroke-dasharray:10 50",
				legendColor, 2*blinkPatternThickness))
	}
	lsm.nPad++
}

func (lsm *ledSignalMap) newSVG() {
	lsm.nPad = 0
	lsm.yPageOffset = 2 * pageMargin
	lsm.canvas = svg.New(lsm.outFile)
	lsm.canvas.StartviewUnit(*width, *height, "mm", 0, 0, *width*100, *height*100)
	lsm.canvas.Title(pageTitle)
	lsm.canvas.Text(pageMargin, pageMargin+500, pageTitle, headerStyle)
}
