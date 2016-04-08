SOURCES=illustration_of_procedure delta_sizes compression_ratios
CLEAN_EXTS=pdf aux log

.PHONY: all clean

all: $(addsuffix .pdf, $(SOURCES))

$(addsuffix .pdf, $(SOURCES)): %.pdf: %.tex
	pdflatex $<

clean:
	rm -f $(foreach ext, $(CLEAN_EXTS), $(addsuffix .$(ext), $(SOURCES)))
