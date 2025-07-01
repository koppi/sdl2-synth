#ifndef MIDI_IN_H
#define MIDI_IN_H
#ifdef __cplusplus
extern "C" {
#endif

// Callback: on=1 for note-on, on=0 for note-off, note=MIDI note number, vel=velocity
void midi_in_set_note_callback(void (*cb)(int on, int note, int vel));
void midi_in_start();

#ifdef __cplusplus
}
#endif
#endif