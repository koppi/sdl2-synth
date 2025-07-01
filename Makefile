CC = gcc
CXX = g++
CFLAGS = -Wall -O2 -g `sdl2-config --cflags` -I.
CXXFLAGS = -Wall -O2 -g `sdl2-config --cflags` -I.
LDFLAGS = `sdl2-config --libs` -lSDL2_ttf -lm -lpthread -lasound

SRC_C = main.c synth.c osc.c voice.c effects.c gui.c midi.c
SRC_CPP = midi_in.cpp RtMidi.cpp
OBJ_C = $(SRC_C:.c=.o)
OBJ_CPP = $(SRC_CPP:.cpp=.o)
BIN = synth

all: $(BIN)

$(BIN): $(OBJ_C) $(OBJ_CPP)
	$(CXX) $(OBJ_C) $(OBJ_CPP) -o $@ $(LDFLAGS)

%.o: %.c *.h
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp *.h
	$(CXX) $(CXXFLAGS) -D__LINUX_ALSA__ -c $< -o $@

clean:
	rm -f *.o $(BIN)

.PHONY: all clean