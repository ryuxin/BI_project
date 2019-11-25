CINC     += -I. -I$(CODE_DIR)/include
CC        = gcc
LD        = $(CC)
MAKE      = make

COPT      = -O3
CWARN     = 
CFLAGS    = -MD -MP -Wall -Wextra $(CWARN) $(COPT) $(CINC) -Wno-unused-function
LDFLAGS   = -lbi -lrpc -lmem -lutil -lbi -lrpc -lmem -lutil -lck -lpthread -L$(CODE_DIR)/libs
MAKEFLAGS = --no-print-directory --section-alignment 0x1000 -I$(PWD)
CODE_DIR=/home/ryx/research/BI_project
