CC      = cc
CFLAGS  = -std=c2x -O3 -Wall -Wextra -Iinclude $(shell pkg-config --cflags sdl2 SDL2_ttf rtmidi)
LDFLAGS = $(shell pkg-config --libs sdl2 SDL2_ttf rtmidi) -lm
SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:.c=.o)
BIN     = synth

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJ) $(BIN)
