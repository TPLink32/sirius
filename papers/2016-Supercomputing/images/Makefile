SUBDIRS=interpolation

.PHONY: all $(SUBDIRS) clean

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) --directory $@ all

clean:
	for directory in $(SUBDIRS) ; do \
		$(MAKE) --directory $$directory clean ; \
	done
