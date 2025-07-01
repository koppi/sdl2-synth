CC      = cc

PKGS    = sdl2 SDL2_ttf SDL2_gfx rtmidi

CFLAGS  = -std=c2x -O3 -Wall -Wextra -Iinclude $(shell pkg-config --cflags $(PKGS))
LDFLAGS = $(shell pkg-config --libs $(PKGS)) -lm
SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:.c=.o)
BIN     = synth

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJ) $(BIN)
