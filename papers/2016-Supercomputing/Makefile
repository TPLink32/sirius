PAPER=paper
SECTIONS=motivation storage details hardware performance discussion
CLEAN_EXTENSIONS=pdf dvi aux bbl log blg

.PHONY: all plots TeX_clean clean

all: $(PAPER).pdf

plots:
	$(MAKE) --directory images all

$(PAPER).pdf: paper.tex $(addsuffix .tex, $(SECTIONS)) plots
	make TeX_clean
	pdflatex ${PAPER}.tex
	pdflatex ${PAPER}.tex
	bibtex ${PAPER}
	pdflatex ${PAPER}.tex
	pdflatex ${PAPER}.tex
	pdflatex ${PAPER}.tex

TeX_clean:
	rm -f $(addprefix $(PAPER)., $(CLEAN_EXTENSIONS))

clean:
	make TeX_clean
	$(MAKE) --directory images clean
