# Modular Synth

A modular synthesizer application written in C, using SDL2 for graphics and audio, and supporting MIDI input via libremidi. The GUI is resizable and features oscilloscope and spectrum displays, keyboard input, and real-time control of oscillators, effects, mixer, and arpeggiator.

## Screenshot

![Modular Synth Screenshot](screenshot.png)

## Features

- **4-oscillator synth**: Each with independent waveform, pitch, detune, gain, phase, pulse width, and unison controls.
- **Comprehensive Parameter Control**: All oscillator parameters accessible via intuitive combo boxes:
  - **Waveform Selection**: SINE, SAW, SQUARE, TRI, NOISE
  - **Pitch Presets**: Common semitone offsets (-24 to +24)
  - **Detune Presets**: Standard cent values (-100¢ to +100¢)
  - **Gain Presets**: Common levels (0%, 12%, 25%, 50%, 75%, 87%, 100%)
  - **Phase Presets**: Musical phase relationships (0°, 45°, 90°, 135°, 180°, etc.)
  - **Pulse Width Presets**: Standard duty cycles (10%, 25%, 50%, 75%, 90%)
  - **Unison Voices**: 1 to 8 voice layering
  - **Unison Spread**: Preset detuning amounts (Off, Tight, Medium, Wide, Extra Wide)
- **Modern microui Interface**: Clean, responsive GUI using the microui library
- **Mixer**: Control the mix and master volume of each oscillator with bus compression.
- **Effects**: Flanger, delay, and reverb with real-time controls.
- **Arpeggiator**: Multiple modes, adjustable tempo, and enable/disable.
- **Oscilloscope & Spectrum**: Visualize output waveform and frequency spectrum.
- **MIDI input**: Map MIDI CC to synth parameters for external control.
- **Interactive Keyboard**: On-screen piano keyboard with visual feedback.

## Dependencies

- [SDL2](https://www.libsdl.org/) (graphics, audio, events)
- [SDL2_ttf](https://github.com/libsdl-org/SDL_ttf) (font rendering)
- [microui](https://github.com/rxi/microui) (GUI library)
- [libremidi](https://github.com/celtera/libremidi) (MIDI input)
- [FFTW3](http://www.fftw.org/) (Fast Fourier Transform)
- [math.h], [string.h], [stdlib.h], [stdio.h]
- A TrueType font (e.g., DejaVuSans.ttf; see below)

## Build Instructions

### Linux

Install dependencies (example for Debian/Ubuntu):

```sh
sudo apt-get install libsdl2-dev libsdl2-ttf-dev libfftw3-dev
```

Clone and build:

```sh
git clone https://github.com/koppi/sdl2-synth
cd sdl2-synth
mkdir build && cd build && cmake .. && make
./synth
```

### macOS

Install dependencies (with Homebrew):

```sh
brew install sdl2 sdl2_ttf fftw
```

Then build as above.

### Windows

- Install SDL2, SDL2_ttf, fftw and libremidi development libraries.
- Use MinGW or Visual Studio to build the sources (adjust includes/libs as needed).

## Running

```sh
./synth
```

The application will open a window with the synthesizer GUI.

## Notes

- The application expects to find a TrueType font (such as `DejaVuSans.ttf`) in `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf` or the current directory.
- MIDI input is mapped to numerous parameters (see `src/midi.c` for mapping).
- All GUI controls respond to mouse drag and mouse wheel.
- The oscilloscope is fed directly from the audio callback thread for real-time visualization.

## File Overview

- `src/`: Source code for synth, GUI, effects, MIDI, etc.
- `src/gui.c`, `src/gui.h`: GUI rendering and event handling.
- `src/oscilloscope.c`, `src/oscilloscope.h`: Oscilloscope/spectrum visualization.
- `src/synth.c`, `src/synth.h`: Synth core and audio callback.
- `src/osc.c`, `src/osc.h`: Oscillator code.
- `src/fx.c`, `src/fx.h`: Effects (flanger, delay, reverb).
- `src/midi.c`, `src/midi.h`: MIDI input and mapping.
- `src/voice.c`, `src/voice.h`: Voice management.
- `src/mixer.c`, `src/mixer.h`: Mixer logic.
- `src/arpeggiator.c`, `src/arpeggiator.h`: Arpeggiator logic.
- `src/app.c`, `src/app.h`, `src/main.c`: Application entry point, SDL setup, main loop.

## License

MIT License (see [LICENSE](LICENSE))

## Credits

- [SDL2](https://www.libsdl.org/)
- [SDL2_ttf](https://www.libsdl.org/projects/SDL_ttf/)
- [libremidi](https://github.com/celtera/libremidi)
- [FFTW3](http://www.fftw.org/)
- [microui](https://github.com/rxi/microui)
- [DejaVu Fonts](https://dejavu-fonts.github.io/)
