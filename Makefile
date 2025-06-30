CC = gcc
CFLAGS = -Wall -O2 -g `sdl2-config --cflags` -I.
LDFLAGS = `sdl2-config --libs` -lSDL2_ttf -lm

SRC = main.c synth.c osc.c voice.c effects.c gui.c midi.c
OBJ = $(SRC:.c=.o)
BIN = synth

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c *.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(BIN)

.PHONY: all clean