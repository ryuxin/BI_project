CC = gcc
LD = $(CC)
program_NAME := server
program_C_SRCS := $(wildcard *.c)
program_OBJS := ${program_C_SRCS:.c=.o}
program_INCLUDE_DIRS :=
program_LIBRARY_DIRS :=
program_LIBRARIES :=

CFLAGS = -O3
LDFLAGS += $(foreach librarydir,$(program_LIBRARY_DIRS),-L$(librarydir))
LDFLAGS += $(foreach library,$(program_LIBRARIES),-l$(library))
LDFLAGS += -pthread -lrt

.PHONY: all clean distclean

all: $(program_NAME)

$(program_NAME): $(program_OBJS)
	$(LD) $(program_OBJS) -o $@ $(LDFLAGS)

clean:
	@- $(RM) $(program_NAME)
	@- $(RM) $(program_OBJS)

distclean: clean

#program_CXX_SRCS := $(wildcard *.cpp)
#program_CXX_OBJS := ${program_CXX_SRCS:.cpp=.o}
#program_OBJS := $(program_C_OBJS) $(program_CXX_OBJS)
