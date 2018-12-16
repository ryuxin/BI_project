include Makefile.inc

$(info ************* Building libs *************)

.PHONY: clean all

all:
	make $(MAKEFLAGS) -C rpc
	make $(MAKEFLAGS) -C mem
	make $(MAKEFLAGS) -C bi
	make $(MAKEFLAGS) -C util

clean:
	make $(MAKEFLAGS) -C rpc clean
	make $(MAKEFLAGS) -C mem clean
	make $(MAKEFLAGS) -C bi clean
	make $(MAKEFLAGS) -C util clean
	rm -rf *.a
