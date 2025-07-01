# SDL2 Synth

A polyphonic synthesizer demo written in C using SDL2. Features a graphical user interface for real-time control of oscillators, effects, and MIDI input.

## Features

- **4 independent oscillators per voice**  
  - Each oscillator can be set to one of 6 waveforms:
    - Saw, Square, Triangle, Sine, Pulse, Noise
  - Adjustable detune and phase (per oscillator) via GUI knobs

- **Polyphonic playback**  
  - 32-voice polyphony by default

- **Graphical user interface**  
  - Real-time visualization and control using SDL2
  - Virtual keyboard for mouse/touch play
  - Visual effect and oscillator controls (knobs and dropdowns)
  - Live demo pattern display and control

- **Built-in demo music**  
  - Toggle demo playback with <kbd>F5</kbd> or <kbd>Space</kbd>
  - Chord progression visualized in GUI

- **Effects**  
  - Flanger
  - Delay
  - Reverb
  - Master volume

- **MIDI input support**  
  - Plug in a MIDI keyboard or use the QWERTY keyboard for note input

## Controls

- **QWERTY Keyboard:**  
  - Play notes using rows (`zsxdcvgbhnjmq2w3er5t6y7ui9o0p`), mapped to white and black keys starting at C3.
- **Mouse:**  
  - Click/drag knobs to adjust parameters (detune, phase, effects, volume)
  - Click waveform name to select oscillator waveform
  - Click on virtual keyboard to play notes

- **Demo Pattern:**  
  - <kbd>F5</kbd> or <kbd>Space</kbd>: Toggle demo music playback

- **Exit:**  
  - <kbd>Esc</kbd> or window close

## Building

### Dependencies

- [SDL2](https://www.libsdl.org/)
- [SDL2_ttf](https://www.libsdl.org/projects/SDL_ttf/)
- [SDL2_gfx](http://www.ferzkopp.net/joomla/content/view/19/14/)
- (Optional) [RtMidi](https://github.com/thestk/rtmidi) for MIDI input

On Debian/Ubuntu:

```sh
sudo apt-get install libsdl2-dev libsdl2-ttf-dev libsdl2-gfx-dev
```

### Build

```
gcc -o sdl2synth \
    main.c synth.c gui.c voice.c effects.c osc.c midi.c \
    midi_in.cpp rtmidi_c.cpp RtMidi.cpp \
    -lSDL2 -lSDL2_ttf -lSDL2_gfx -lstdc++ -lpthread -lasound
```

- You may need to adapt includes/links for your platform (Windows, macOS, etc.).
- For MIDI support: ensure `RtMidi` is present and compiled with ALSA/JACK/CoreMIDI as appropriate.

## File Structure

- `main.c` - Application entry point, event loop and main logic
- `synth.c/.h` - Synthesizer core, audio generation, demo pattern
- `voice.c/.h` - Voice allocation and per-note synthesis
- `gui.c/.h` - User interface, knob rendering, dropdowns, and keyboard
- `effects.c/.h` - Flanger, delay, and reverb effects
- `osc.c/.h` - Oscillator waveform generation
- `midi.c/.h` - Keyboard-to-MIDI mapping helpers
- `midi_in.cpp/.h` - MIDI input via RtMidi
- `rtmidi_c.cpp/.h`, `RtMidi.cpp/.h` - RtMidi C and C++ MIDI backend

## License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Credits

- [RtMidi](https://github.com/thestk/rtmidi) by Gary P. Scavone
- SDL2 and related libraries

---
Enjoy making music!