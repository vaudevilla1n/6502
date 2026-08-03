CFLAGS := -Wall -Wextra -Wpedantic -std=c23 -g3 -D_DEFAULT_SOURCE

.PHONY: all emulator assembler clean

all: emulator assembler

emulator: 6502

6502: 6502.c

assembler: 6502asm

6502asm: 6502asm.c

clean:
	rm -f ./6502
	rm -f ./6502asm
