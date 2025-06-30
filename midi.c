#include "midi.h"
#include <SDL2/SDL.h>
#include <string.h>

const int midi_qwertz_map_white[20] = {
    60, 62, 64, 65, 67, 69, 71, 72, 74, 76, // QWERTZUIOP
    61, 63, 66, 68, 70, 73, 75, 77, 78, 79  // ASDFGHJKL
};

// Mapping: QWERTZUIOP, ASDFGHJKL to MIDI notes
int midi_key_to_note(SDL_Keycode sym, SDL_Keymod mod) {
    int base = 0;
    switch (sym) {
        case SDLK_q: base = 0; break;
        case SDLK_w: base = 1; break;
        case SDLK_e: base = 2; break;
        case SDLK_r: base = 3; break;
        case SDLK_t: base = 4; break;
        case SDLK_z: base = 5; break;
        case SDLK_u: base = 6; break;
        case SDLK_i: base = 7; break;
        case SDLK_o: base = 8; break;
        case SDLK_p: base = 9; break;
        case SDLK_a: base = 10; break;
        case SDLK_s: base = 11; break;
        case SDLK_d: base = 12; break;
        case SDLK_f: base = 13; break;
        case SDLK_g: base = 14; break;
        case SDLK_h: base = 15; break;
        case SDLK_j: base = 16; break;
        case SDLK_k: base = 17; break;
        case SDLK_l: base = 18; break;
        default: return -1;
    }
    return midi_qwertz_map_white[base];
}

void midi_note_name(int midi, char *buf) {
    static const char *names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    int oct = midi/12 - 1;
    int n = midi%12;
    sprintf(buf, "%s%d", names[n], oct);
}

// Returns position in white key array, -1 if not white key
int midi_white_pos(int midi) {
    int n = midi%12;
    const int white[] = {0,2,4,5,7,9,11};
    for (int i=0; i<7; ++i) if (white[i]==n) return i;
    return -1;
}

// Returns offset for black key if it exists, else -1
int midi_black_offset(int n) {
    switch (n) {
        case 0: case 2: case 5: case 7: case 9: return n;
        default: return -1;
    }
}