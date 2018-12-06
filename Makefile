include Makefile.inc

.PHONY: lib clean all

all: lib

lib:
	make $(MAKEFLAGS) -C libs

config:
	@echo "CODE_DIR=`pwd`" >> Makefile.inc

clean:
	make $(MAKEFLAGS) -C libs clean
