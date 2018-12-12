include Makefile.inc

.PHONY: lib test clean all

all: lib test

lib:
	make $(MAKEFLAGS) -C libs

test:
	make $(MAKEFLAGS) -C tests
	make $(MAKEFLAGS) -C cache_op

config:
	@echo "CODE_DIR=`pwd`" >> Makefile.inc

clean:
	make $(MAKEFLAGS) -C libs clean
	make $(MAKEFLAGS) -C cache_op clean
	make $(MAKEFLAGS) -C tests clean
