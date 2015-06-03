all: pdf

ps: inc2.tex
	latex inc2.tex
	#bibtex inc2
	latex inc2.tex
	dvips -q inc2.dvi -o inc2.ps
	ps2pdf inc2.ps draft.pdf

pdf: inc2.tex
	pdflatex inc2.tex
	#bibtex inc2
	pdflatex inc2.tex
	open inc2.pdf
	#dvips -h draft.pro inc2.dvi -o draftinc2.ps


clean:
	-rm -rf *~ *.bak inc2.ps inc2.pdf inc2.out
	-rm -rf inc2.aux inc2.blg inc2.bbl inc2.dvi inc2.log  	
	-find . -name "*.bak" | xargs rm -rf

