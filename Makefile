# Basic Makefile for SDL2 Synth Project with GUI, Virtual Keyboard, Effects, and MIDI
# Adjust paths for your system as needed

CC = gcc
CFLAGS = -Wall -O2 -g `sdl2-config --cflags` -I.
LDFLAGS = `sdl2-config --libs` -lSDL2_ttf -lSDL2_gfx -lm

# If you want MIDI via RtMidi, uncomment the following lines and add rtmidi_in.o to OBJS
# RTMIDI_SRC = RtMidi.cpp
# RTMIDI_OBJ = RtMidi.o
# MIDI_OBJS = midi_in.o $(RTMIDI_OBJ)
# MIDI_FLAGS = -I/path/to/rtmidi

OBJS = main.o synth.o voice.o osc.o gui.o
# Add effects.o if you implement effects processing

TARGET = sdl2synth

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean