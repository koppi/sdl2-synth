A modular synthesizer application written in C, using SDL2 for graphics and audio, and supporting MIDI input via libremidi.

Live demo at: https://koppi.github.io/koppi/sdl2-synth

## Features

- **4-oscillator synth**: Each with independent waveform, pitch, detune, gain, phase, pulse width, and unison controls
- **Mixer**: Control the mix and master volume of each oscillator with bus compression
- **Effects**: Flanger, delay, reverb, and analog filter with real-time controls
- **Arpeggiator**: Multiple modes, adjustable tempo, octave control, and multi-octave chord arpeggiation
- **Oscilloscope & Spectrum**: Visualize output waveform and frequency spectrum
- **MIDI input**: Map MIDI CC to synth parameters for external control
- **Interactive Keyboard**: On-screen piano keyboard with visual feedback

## Build Dependencies

- SDL2 (graphics, audio, events)
- SDL2_ttf (font rendering)
- Dear ImGui (GUI library)
- libremidi (MIDI input)

## Build Instructions

### Linux

```sh
sudo apt-get install git cmake ninja libsdl2-dev libsdl2-ttf-dev
git clone https://github.com/koppi/sdl2-synth && cd sdl2-synth
cmake --preset release
cmake --build --preset release && build/release/synth
```

### macOS

```sh
brew install git ninja sdl2 sdl2_ttf
# Then build as above
```

### Windows

Install SDL2, SDL2_ttf and libremidi development libraries, then build with MinGW or Visual Studio.

## Running

```sh
build/release/synth
```

## MIDI Control

MIDI input is mapped to numerous parameters (see `src/midi.c` for mapping). All GUI controls respond to mouse drag and mouse wheel. The oscilloscope is fed directly from the audio callback thread for real-time visualization.