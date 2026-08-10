CFLAGS := -Wall -Wextra -Wpedantic -std=c23 -g3 -D_DEFAULT_SOURCE -I../include

SRC := $(wildcard *.c)
INC := $(wildcard ../include/*.h)

.PHONY: all clean

all: 6502asm

6502asm: $(SRC) $(INC)
	$(CC) $(CFLAGS) $(SRC) -o 6502asm

clean:
	rm -f ./6502asm
