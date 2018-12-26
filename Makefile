include Makefile.inc

.PHONY: lib test aobj rbtree clean all

all: lib test aobj rbtree

lib:
	make $(MAKEFLAGS) -C libs

test:
	make $(MAKEFLAGS) -C tests
	make $(MAKEFLAGS) -C cache_op

aobj:
	make $(MAKEFLAGS) -C atomic_obj

rbtree:
	make $(MAKEFLAGS) -C rb_tree

config:
	@echo "CODE_DIR=`pwd`" >> Makefile.inc

clean:
	make $(MAKEFLAGS) -C libs clean
	make $(MAKEFLAGS) -C cache_op clean
	make $(MAKEFLAGS) -C tests clean
	make $(MAKEFLAGS) -C atomic_obj clean
	make $(MAKEFLAGS) -C rb_tree clean
