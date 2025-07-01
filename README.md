# SDL2 Modular Synthesizer

A polyphonic modular synthesizer written in C using SDL2, featuring:

- Sine, square, white noise, and **sawtooth** oscillators (toggle with F1)
- Complex reverb, delay, and flanger effects
- Polyphony: up to 64 voices
- 48kHz sample rate, 512 sample buffer size
- QWERTZ keyboard-to-MIDI piano mapping (`qwertzuiop`, `asdfghjkl`), octave shift (arrow keys)
- GUI with visual keyboard (white/black keys), state shown in window title
- Polyphonic demo melody (press Space)
- Volume and effect parameter controls (arrow keys, F3/F4/F5/F6)
- **External MIDI input via RtMidi** (play from your MIDI keyboard/controller)

## Controls

| Key               | Action                                   |
|-------------------|------------------------------------------|
| QWERTZUIOP        | Play notes (top row, white keys)         |
| ASDFGHJKL         | Play notes (second row, white keys)      |
| Up/Down           | Change octave                            |
| Left/Right        | Change volume                            |
| F1                | Toggle oscillators (sine/square/noise/saw)|
| F3/F4             | Change flanger parameters                |
| F5/F6             | Change delay parameters                  |
| Space             | Play demo melody                         |
| **MIDI Keyboard** | Play notes, triggers synth voices        |

## MIDI Input Support

- This synthesizer supports MIDI input using the [RtMidi](https://github.com/thestk/rtmidi) library.
- The first available system MIDI input port is opened automatically.
- You can play notes and control the synth from an external MIDI keyboard or device.

### Linux dependencies

- Install SDL2, SDL2_ttf, and ALSA development packages:
  ```sh
  sudo apt-get install libsdl2-dev libsdl2-ttf-dev libasound2-dev
  ```
- RtMidi will use ALSA for MIDI input on Linux.

### macOS dependencies

- Install SDL2 and SDL2_ttf (e.g. via Homebrew).
- RtMidi will use CoreMIDI.

### Windows dependencies

- Install SDL2 and SDL2_ttf development libraries.
- RtMidi will use WinMM.

## Build Instructions

Requires SDL2, SDL2_ttf, and RtMidi (with appropriate MIDI backend support).

1. **Download or clone RtMidi**  
   Place `RtMidi.cpp` and `RtMidi.h` in your project directory.

2. **Build:**
    ```sh
    make
    ./synth
    ```

   If you encounter MIDI errors, ensure you have the necessary system MIDI development libraries (see above).

## License

MIT License (see LICENSE).