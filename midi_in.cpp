#include "midi_in.h"
#include "RtMidi.h"
#include <vector>
#include <functional>
#include <memory>

static std::function<void(int, int, int)> g_note_cb;

extern "C" void midi_in_set_note_callback(void (*cb)(int, int, int)) {
    g_note_cb = cb;
}

// Hold RtMidiIn object somewhere with static duration
static std::unique_ptr<RtMidiIn> midiin;

extern "C" void midi_in_start() {
    if (!midiin) {
        midiin = std::make_unique<RtMidiIn>();
        // Open first available port
        if (midiin->getPortCount() > 0)
            midiin->openPort(0);
        midiin->ignoreTypes(false, false, false);
        midiin->setCallback([](double /*stamp*/, std::vector<unsigned char> *msg, void * /*userData*/) {
            if (msg->size() >= 3 && g_note_cb) {
                int status = (*msg)[0] & 0xF0;
                int note = (*msg)[1];
                int vel = (*msg)[2];
                if (status == 0x90 && vel > 0)
                    g_note_cb(1, note, vel);
                else if ((status == 0x80) || (status == 0x90 && vel == 0))
                    g_note_cb(0, note, vel);
            }
        });
    }
}