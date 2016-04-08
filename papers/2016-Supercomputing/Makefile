PAPER=paper
SECTIONS=motivation storage details hardware performance discussion
CLEAN_EXTENSIONS=pdf dvi aux bbl log blg

.PHONY: all TeX_clean clean

all: $(PAPER).pdf

$(PAPER).pdf: paper.tex $(addsuffix .tex, $(SECTIONS))
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
