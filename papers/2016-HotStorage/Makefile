PAPER=paper
all: paper.tex
	rm -f ${PAPER}.pdf
	rm -f ${PAPER}.dvi
	rm -f ${PAPER}.aux
	rm -f ${PAPER}.bbl
	rm -f ${PAPER}.log
	rm -f ${PAPER}.blg
	latex ${PAPER}.tex
	latex ${PAPER}.tex
	bibtex ${PAPER}
	latex ${PAPER}.tex
	latex ${PAPER}.tex
	latex ${PAPER}.tex
	dvips -t letter -D 600 -G0 -f ${PAPER}.dvi > ${PAPER}.ps
	ps2pdf14 -dPDFSETTINGS=/prepress -dEmbedAllFonts=true ${PAPER}.ps

clean:
	rm -f ${PAPER}.aux ${PAPER}.bbl ${PAPER}.blg ${PAPER}.log ${PAPER}.dvi ${PAPER}.ps
#	dvips -Ppdf -Pcmz -Pamz -t letter -D 600 -G0 -o ${PAPER}.ps ${PAPER}.dvi
#	ps2pdf14 -dPDFSETTINGS=/prepress -dEmbedAllFonts=true ${PAPER}
