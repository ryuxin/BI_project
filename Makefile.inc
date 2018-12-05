CINC     += -I. -I$(CODE_DIR)/include
CC        = gcc
LD        = $(CC)
MAKE      = make

COPT      = -O3
CWARN     = 
CFLAGS    = -MD -MP -std=c11 -Wall -Wextra $(CWARN) $(COPT) $(CINC) -Wno-unused-function
LDFLAGS   = -lrpc -lmem -lutil -L$(CODE_DIR)/libs
MAKEFLAGS = --no-print-directory --section-alignment 0x1000 -I$(PWD)
