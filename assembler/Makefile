BIN :=

ifeq ($(OS), Windows_NT)
	BIN += 6502asm.exe
else
	BIN += 6502asm
endif

CFLAGS := -Wall -Wextra -Wpedantic -std=c23 -g3 -D_DEFAULT_SOURCE -I../include

SRC := $(wildcard *.c)
INC := $(wildcard ../include/*.h)

.PHONY: all clean

all: $(BIN)

$(BIN): $(SRC) $(INC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

clean:
	rm -f $(BIN)
