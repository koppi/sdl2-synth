# SDL2 Modular Synthesizer

A polyphonic modular synthesizer written in C using SDL2, featuring:

- Sine and square wave oscillators (toggle with F1)
- Complex reverb, delay, and flanger effects
- Polyphony: up to 64 voices
- 48kHz sample rate, 512 sample buffer size
- QWERTZ keyboard-to-MIDI piano mapping (`qwertzuiop`, `asdfghjkl`), octave shift (arrow keys)
- GUI with visual keyboard (white/black keys), state shown in window title
- Polyphonic demo melody (press Space)
- Volume and effect parameter controls (arrow keys, F3/F4/F5/F6)

## Controls

| Key               | Action                                   |
|-------------------|------------------------------------------|
| QWERTZUIOP        | Play notes (top row, white keys)         |
| ASDFGHJKL         | Play notes (second row, white keys)      |
| Up/Down           | Change octave                            |
| Left/Right        | Change volume                            |
| F1                | Toggle oscillators (sine/square)         |
| F3/F4             | Change flanger parameters                |
| F5/F6             | Change delay parameters                  |
| Space             | Play demo melody                         |

## Build Instructions

Requires SDL2 and SDL2_ttf development libraries.

```
sudo apt-get install libsdl2-dev libsdl2-ttf-dev
make
./synth
```

## License

MIT License (see LICENSE).