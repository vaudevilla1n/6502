CFLAGS := -Wall -Wextra -Wpedantic -std=c23 -g3 -D_DEFAULT_SOURCE -I../include

.PHONY: all clean

all: 6502asm

6502asm: 6502asm.c lexer.c

clean:
	rm -f ./6502asm
