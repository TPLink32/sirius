PAPER=paper
all: paper.tex
	rm -f ${PAPER}.pdf
	rm -f ${PAPER}.dvi
	rm -f ${PAPER}.aux
	rm -f ${PAPER}.bbl
	rm -f ${PAPER}.log
	rm -f ${PAPER}.blg
	pdflatex ${PAPER}.tex
	pdflatex ${PAPER}.tex
	bibtex ${PAPER}
	pdflatex ${PAPER}.tex
	pdflatex ${PAPER}.tex
	pdflatex ${PAPER}.tex

clean:
	rm -f ${PAPER}.aux ${PAPER}.bbl ${PAPER}.blg ${PAPER}.log
