#ifndef MIDI_H
#define MIDI_H
#include <SDL2/SDL.h>

// Maps QWERTZUIOP, ASDFGHJKL to MIDI notes for two rows of white keys
extern const int midi_qwertz_map_white[20];

// Convert SDL_Keycode and modifiers to MIDI note, -1 if not mapped
int midi_key_to_note(SDL_Keycode sym, SDL_Keymod mod);

// Get the MIDI note name (e.g. "C4") for a MIDI note number
void midi_note_name(int midi, char *buf);

// Returns the position in the white key array for a MIDI note, -1 if not a white key
int midi_white_pos(int midi);

// Returns offset for a black key if it exists, otherwise -1
int midi_black_offset(int n);

#endif