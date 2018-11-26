CC     = gcc
INC    = -I.
CFLAGS = $(INC) -Wall -Wextra -Werror -O3 -DNO_PRINT_TITLES
CFILES = $(wildcard *.c)
OBJS   = $(patsubst %.c, %.o,$(CFILES))
BIN    = test

%.o:%.c
	$(info Compiling $<)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BIN):$(OBJS)
	$(CC) -o $@ $^

exec:$(BIN)
	./test

clean:
	@rm -f $(BIN) $(OBJS) *~
