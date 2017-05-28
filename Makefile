# Push the www directory to Github Pages

gh-pages:
	rm -rf gh-pages
	mkdir gh-pages
	cp hardware/bottom-label-de.svg gh-pages/
	(cd hardware; $(MAKE) frame.stl)
	cp hardware/frame.stl gh-pages/
	cp hardware/top-plate/plot_files/top-plate.zip gh-pages/
	cp hardware/top-plate/plot_files/top-plate.pdf gh-pages/
	cp hardware/top-plate/plot_files/top-plate-brd.svg gh-pages/
	cp hardware/pcb/plot_files/pcb.zip gh-pages/
	cp hardware/pcb/plot_files/pcb.pdf gh-pages/
	cp hardware/pcb/plot_files/pcb-brd.svg gh-pages/
	cp hardware/bottom-plate/plot_files/bottom-plate.zip gh-pages/
	cp hardware/bottom-plate/plot_files/bottom-plate.pdf gh-pages/
	cp hardware/bottom-plate/plot_files/bottom-plate-brd.svg gh-pages/
	(cd firmware/cheatsheet-generator; $(MAKE))
	cp firmware/cheatsheet-generator/chordmap.txt gh-pages/
	cp firmware/cheatsheet-generator/default-chordmap1.svg gh-pages/
	cp firmware/cheatsheet-generator/default-chordmap2.svg gh-pages/
	cp firmware/cheatsheet-generator/leds.svg gh-pages/
	echo '<h3>Generated files of <a href="https://github.com/trebb/nan-15">NaN-15</a></h3>' > gh-pages/index.html
	ls gh-pages | \
		grep -v index.html | \
		sed -e 's/^\(.*\)$$/<a href="\1">\1<\/a><br>/' >> gh-pages/index.html

publish: gh-pages
	(cd gh-pages; \
	git init; \
	git add ./; \
	git commit -a -m "gh-pages pseudo commit"; \
	git push git@github.com:trebb/nan-15.git +master:gh-pages)

clean:
	rm -rf gh-pages
