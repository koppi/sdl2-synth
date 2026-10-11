#include "params.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace sdl2synth {

const char* const kPitchBendKey = "pitch_bend";

namespace {

static_assert(sizeof(OscWaveform) == sizeof(int), "enums must be int sized");
static_assert(sizeof(LfoWaveform) == sizeof(int), "enums must be int sized");
static_assert(sizeof(LfoSyncMode) == sizeof(int), "enums must be int sized");
static_assert(sizeof(ArpMode) == sizeof(int), "enums must be int sized");
static_assert(sizeof(ArpRate) == sizeof(int), "enums must be int sized");
static_assert(sizeof(ChordType) == sizeof(int), "enums must be int sized");
static_assert(sizeof(FilterType) == sizeof(int), "enums must be int sized");

// Unit (group) indices; keep in sync with kUnitNames.
enum Unit {
  kUnitOsc1, kUnitOsc2, kUnitOsc3, kUnitOsc4,
  kUnitEnvelope,
  kUnitLfo1, kUnitLfo2, kUnitLfo3,
  kUnitMixer, kUnitCompressor, kUnitMastering,
  kUnitFilter, kUnitRingMod, kUnitReverb, kUnitFlanger, kUnitDelay, kUnitMultiTap,
  kUnitArp, kUnitPerformance,
};

const std::vector<std::string> kUnitNames = {
  "Oscillator 1", "Oscillator 2", "Oscillator 3", "Oscillator 4",
  "Envelope",
  "LFO 1 (Pitch)", "LFO 2 (Volume)", "LFO 3 (Filter)",
  "Mixer", "Compressor", "Mastering",
  "Filter", "Ring Modulator", "Reverb", "Flanger", "Delay", "Multi-Tap Delay",
  "Arpeggiator", "Performance",
};

// 32-bit FNV-1a, stable across builds and platforms. VST3 parameter IDs must
// stay within [0, 0x7FFFFFFF], so the top bit is dropped.
uint32_t hashKey(const std::string& key) {
  uint32_t h = 2166136261u;
  for (unsigned char c : key) {
    h ^= c;
    h *= 16777619u;
  }
  return h & 0x7FFFFFFFu;
}

class Builder {
public:
  std::vector<ParamDef> list;

  ParamDef& add(ParamKind kind, const std::string& key, const std::string& title,
                const std::string& shortTitle, int unit, size_t offset, bool floatField,
                float min, float max, float def) {
    list.emplace_back();
    ParamDef& p = list.back();
    p.key = key;
    p.id = hashKey(key);
    p.title = title;
    p.shortTitle = shortTitle;
    p.unit = unit;
    p.kind = kind;
    p.offset = offset;
    p.floatField = floatField;
    p.min = min;
    p.max = max;
    p.def = def;
    p.applyKey = key; // overridden below where the setter name differs
    return p;
  }

  ParamDef& flt(const std::string& key, const std::string& title, const std::string& shortTitle,
                int unit, size_t offset, float min, float max, float def,
                const char* units = "", int precision = 2, bool log = false) {
    ParamDef& p = add(ParamKind::Float, key, title, shortTitle, unit, offset, true, min, max, def);
    p.units = units;
    p.precision = precision;
    p.logarithmic = log;
    return p;
  }

  ParamDef& integer(const std::string& key, const std::string& title, const std::string& shortTitle,
                    int unit, size_t offset, int min, int max, int def) {
    ParamDef& p = add(ParamKind::Int, key, title, shortTitle, unit, offset, false,
                      (float)min, (float)max, (float)def);
    p.precision = 0;
    return p;
  }

  ParamDef& boolean(const std::string& key, const std::string& title, const std::string& shortTitle,
                    int unit, size_t offset, bool def) {
    ParamDef& p = add(ParamKind::Bool, key, title, shortTitle, unit, offset, false, 0.0f, 1.0f,
                      def ? 1.0f : 0.0f);
    p.precision = 0;
    return p;
  }

  ParamDef& choice(const std::string& key, const std::string& title, const std::string& shortTitle,
                   int unit, size_t offset, std::vector<std::string> choices, int def) {
    ParamDef& p = add(ParamKind::Choice, key, title, shortTitle, unit, offset, false, 0.0f,
                      (float)(choices.size() - 1), (float)def);
    p.choices = std::move(choices);
    p.precision = 0;
    return p;
  }
};

std::vector<ParamDef> buildParams() {
  Builder b;
  const std::string S = "";

  // --- Oscillators -----------------------------------------------------------
  struct OscDefault {
    int waveform;
    float pitch, detune, gain, pulse_width;
  };
  const OscDefault osc_defaults[4] = {
    {OSC_SAW,    0.0f,   0.0f, 0.25f, 0.5f},
    {OSC_SAW,    0.0f,   0.1f, 0.25f, 0.5f},
    {OSC_SQUARE, -12.0f, 0.0f, 0.12f, 0.5f},
    {OSC_SINE,   -12.0f, 0.0f, 0.20f, 0.5f},
  };
  for (int i = 0; i < 4; ++i) {
    const std::string k = "osc" + std::to_string(i + 1) + ".";
    const std::string t = "OSC" + std::to_string(i + 1) + " ";
    const std::string st = "O" + std::to_string(i + 1) + " ";
    const size_t base = offsetof(Synth, osc) + i * sizeof(Oscillator);
    const OscDefault& d = osc_defaults[i];
    int u = kUnitOsc1 + i;

    b.choice(k + "waveform", t + "Waveform", st + "Wave", u, base + offsetof(Oscillator, waveform),
             {"SINE", "SAW", "SQUARE", "TRI", "NOISE"}, d.waveform);
    b.flt(k + "pitch", t + "Pitch", st + "Pitch", u, base + offsetof(Oscillator, pitch),
          -24.0f, 24.0f, d.pitch, "st", 2);
    b.flt(k + "detune", t + "Detune", st + "Detune", u, base + offsetof(Oscillator, detune),
          -1.0f, 1.0f, d.detune, "st", 2);
    // The GUI's Gain knob edits the oscillator's own gain (not the mixer gain
    // that "oscN.gain" addresses in synth_set_param), so write it directly.
    b.flt(k + "gain", t + "Gain", st + "Gain", u, base + offsetof(Oscillator, gain),
          0.0f, 1.0f, d.gain).applyKey.clear();
    b.flt(k + "pan", t + "Pan", st + "Pan", u, base + offsetof(Oscillator, pan),
          -1.0f, 1.0f, 0.0f);
    b.flt(k + "pulse_width", t + "Pulse Width", st + "PW", u, base + offsetof(Oscillator, pulse_width),
          0.0f, 1.0f, d.pulse_width);
    b.integer(k + "unison_voices", t + "Unison Voices", st + "Unison", u,
              base + offsetof(Oscillator, unison_voices), 1, 8, 1);
    b.flt(k + "unison_detune", t + "Unison Detune", st + "U.Det", u,
          base + offsetof(Oscillator, unison_detune), 0.0f, 1.0f, 0.1f);
  }

  // --- Envelope ----------------------------------------------------------------
  {
    const size_t base = offsetof(Synth, adsr);
    b.flt("adsr.attack", "Attack", "Atk", kUnitEnvelope, base + offsetof(AdsrEnvelope, attack),
          0.001f, 5.0f, 0.01f, "s", 3, true);
    b.flt("adsr.decay", "Decay", "Dec", kUnitEnvelope, base + offsetof(AdsrEnvelope, decay),
          0.001f, 5.0f, 0.3f, "s", 3, true);
    b.flt("adsr.sustain", "Sustain", "Sus", kUnitEnvelope, base + offsetof(AdsrEnvelope, sustain),
          0.0f, 1.0f, 0.7f);
    b.flt("adsr.release", "Release", "Rel", kUnitEnvelope, base + offsetof(AdsrEnvelope, release),
          0.001f, 10.0f, 0.4f, "s", 3, true);
  }

  // --- LFOs ----------------------------------------------------------------------
  const char* lfo_names[3] = {"Pitch", "Volume", "Filter"};
  for (int i = 0; i < 3; ++i) {
    const std::string k = "lfo" + std::to_string(i + 1) + ".";
    const std::string t = "LFO" + std::to_string(i + 1) + " ";
    const std::string st = "L" + std::to_string(i + 1) + " ";
    const size_t base = offsetof(Synth, lfos) + i * sizeof(LFO);
    int u = kUnitLfo1 + i;
    (void)lfo_names;

    b.boolean(k + "enabled", t + "On", st + "On", u, base + offsetof(LFO, enabled), false);
    b.choice(k + "waveform", t + "Waveform", st + "Wave", u, base + offsetof(LFO, waveform),
             {"SINE", "TRIANGLE", "SQUARE", "SAW", "RANDOM"}, LFO_SINE);
    b.choice(k + "sync", t + "Sync", st + "Sync", u, base + offsetof(LFO, sync),
             {"Free", "Retrigger", "Keyfollow"}, LFO_SYNC_RETRIGGER);
    b.flt(k + "frequency", t + "Rate", st + "Rate", u, base + offsetof(LFO, frequency),
          0.1f, 20.0f, i == 0 ? 2.0f : 1.0f, "Hz", 2, true);
    b.flt(k + "depth", t + "Depth", st + "Depth", u, base + offsetof(LFO, depth),
          0.0f, 1.0f, i == 0 ? 0.1f : 0.5f);
    b.flt(k + "phase", t + "Phase", st + "Phase", u, base + offsetof(LFO, phase),
          0.0f, 1.0f, 0.0f);
  }

  // --- Mixer ---------------------------------------------------------------------
  {
    const size_t base = offsetof(Synth, mixer);
    b.flt("mixer.master", "Master Volume", "Master", kUnitMixer, base + offsetof(Mixer, master),
          0.0f, 2.0f, 0.5f);
    b.flt("mixer.master.pan", "Master Pan", "Pan", kUnitMixer, base + offsetof(Mixer, master_pan),
          -1.0f, 1.0f, 0.0f);
    b.flt("mixer.master.width", "Master Width", "Width", kUnitMixer, base + offsetof(Mixer, master_width),
          0.0f, 2.0f, 1.0f);

    b.boolean("mixer.comp.enabled", "Compressor On", "Comp", kUnitCompressor,
              base + offsetof(Mixer, comp_enabled), false);
    b.flt("mixer.comp.threshold", "Comp Threshold", "Thresh", kUnitCompressor,
          base + offsetof(Mixer, comp_threshold), -24.0f, 0.0f, -12.0f, "dB", 1);
    b.flt("mixer.comp.ratio", "Comp Ratio", "Ratio", kUnitCompressor,
          base + offsetof(Mixer, comp_ratio), 1.0f, 10.0f, 3.0f, ":1", 1);
    b.flt("mixer.comp.attack", "Comp Attack", "Atk", kUnitCompressor,
          base + offsetof(Mixer, comp_attack), 1.0f, 100.0f, 10.0f, "ms", 1, true);
    b.flt("mixer.comp.release", "Comp Release", "Rel", kUnitCompressor,
          base + offsetof(Mixer, comp_release), 10.0f, 1000.0f, 100.0f, "ms", 0, true);
    b.flt("mixer.comp.makeup", "Comp Makeup", "Makeup", kUnitCompressor,
          base + offsetof(Mixer, comp_makeup_gain), 0.0f, 12.0f, 6.0f, "dB", 1);

    b.boolean("mixer.dc.filter.enabled", "DC Filter On", "DC", kUnitMastering,
              base + offsetof(Mixer, dc_filter_enabled), false);
    b.flt("mixer.dc.filter.freq", "DC Filter Cutoff", "DC Freq", kUnitMastering,
          base + offsetof(Mixer, dc_filter_freq), 1.0f, 100.0f, 20.0f, "Hz", 1);
    b.boolean("mixer.soft.clip.enabled", "Soft Clip On", "Clip", kUnitMastering,
              base + offsetof(Mixer, soft_clip_enabled), false);
    b.flt("mixer.soft.clip.threshold", "Soft Clip Threshold", "Clip Th", kUnitMastering,
          base + offsetof(Mixer, soft_clip_threshold), -20.0f, 0.0f, -3.0f, "dB", 1);
    b.flt("mixer.soft.clip.ratio", "Soft Clip Knee", "Knee", kUnitMastering,
          base + offsetof(Mixer, soft_clip_ratio), 1.0f, 10.0f, 2.0f, "", 1);
    b.boolean("mixer.auto.gain.enabled", "Auto Gain On", "AutoG", kUnitMastering,
              base + offsetof(Mixer, auto_gain_enabled), false);
    b.flt("mixer.auto.gain.target", "Auto Gain Target", "AutoG Tgt", kUnitMastering,
          base + offsetof(Mixer, auto_gain_target), -20.0f, 0.0f, -6.0f, "dB", 1);
  }

  // --- Filter ----------------------------------------------------------------------
  {
    const size_t base = offsetof(Synth, fx);
    b.boolean("fx.filter.enabled", "Filter On", "Filter", kUnitFilter,
              base + offsetof(FX, filter_enabled), false);
    b.choice("fx.filter.type", "Filter Type", "Type", kUnitFilter,
             base + offsetof(FX, filter) + offsetof(AnalogFilter, type),
             {"Lowpass", "Highpass", "Bandpass", "Notch"}, FILTER_LOWPASS);
    b.flt("fx.filter.cutoff", "Filter Cutoff", "Cutoff", kUnitFilter,
          base + offsetof(FX, filter_cutoff), 20.0f, 20000.0f, 1000.0f, "Hz", 0, true);
    b.flt("fx.filter.resonance", "Filter Resonance", "Res", kUnitFilter,
          base + offsetof(FX, filter_resonance), 0.1f, 10.0f, 1.0f, "", 2, true);
    b.flt("fx.filter.drive", "Filter Drive", "Drive", kUnitFilter,
          base + offsetof(FX, filter_drive), 1.0f, 10.0f, 1.0f);
    b.flt("fx.filter.mix", "Filter Mix", "Mix", kUnitFilter,
          base + offsetof(FX, filter_mix), 0.0f, 1.0f, 1.0f);
    b.choice("fx.filter.oversampling", "Filter Oversampling", "OvrSmp", kUnitFilter,
             base + offsetof(FX, filter_oversampling), {"1x", "2x", "4x", "8x"}, 1);

    b.boolean("ring_mod.enabled", "Ring Mod On", "Ring", kUnitRingMod,
              offsetof(Synth, ring_mod) + offsetof(RingModulator, enabled), false);
    b.flt("ring_mod.frequency", "Ring Mod Frequency", "RM Freq", kUnitRingMod,
          offsetof(Synth, ring_mod) + offsetof(RingModulator, frequency), 0.1f, 5000.0f, 20.0f,
          "Hz", 1, true);
    b.flt("ring_mod.mix", "Ring Mod Mix", "RM Mix", kUnitRingMod,
          offsetof(Synth, ring_mod) + offsetof(RingModulator, mix), 0.0f, 1.0f, 0.0f);

    b.flt("fx.reverb.size", "Reverb Size", "Rv Size", kUnitReverb,
          base + offsetof(FX, reverb_size), 0.0f, 1.0f, 0.5f);
    b.flt("fx.reverb.damping", "Reverb Damping", "Rv Damp", kUnitReverb,
          base + offsetof(FX, reverb_damping), 0.0f, 1.0f, 0.3f);
    b.flt("fx.reverb.mix", "Reverb Mix", "Rv Mix", kUnitReverb,
          base + offsetof(FX, reverb_mix), 0.0f, 1.0f, 0.12f);

    b.flt("fx.flanger.depth", "Flanger Depth", "Fl Depth", kUnitFlanger,
          base + offsetof(FX, flanger_depth), 0.0f, 1.0f, 0.0f);
    b.flt("fx.flanger.rate", "Flanger Rate", "Fl Rate", kUnitFlanger,
          base + offsetof(FX, flanger_rate), 0.0f, 5.0f, 0.25f, "Hz", 2);
    b.flt("fx.flanger.feedback", "Flanger Feedback", "Fl Fdbk", kUnitFlanger,
          base + offsetof(FX, flanger_feedback), 0.0f, 1.0f, 0.5f);

    b.flt("fx.delay.time", "Delay Time", "Dl Time", kUnitDelay,
          base + offsetof(FX, delay_time), 0.0f, 1.0f, 0.3f, "s", 3);
    b.flt("fx.delay.feedback", "Delay Feedback", "Dl Fdbk", kUnitDelay,
          base + offsetof(FX, delay_feedback), 0.0f, 1.0f, 0.3f);
    b.flt("fx.delay.mix", "Delay Mix", "Dl Mix", kUnitDelay,
          base + offsetof(FX, delay_mix), 0.0f, 1.0f, 0.0f);

    b.boolean("fx.multitap.enabled", "Multi-Tap On", "Multi", kUnitMultiTap,
              base + offsetof(FX, multitap_enabled), false);
    const float tap_beats[4] = {1.0f, 0.75f, 0.5f, 0.333f};
    const float tap_levels[4] = {0.7f, 0.5f, 0.4f, 0.3f};
    for (int i = 0; i < 4; ++i) {
      const std::string n = std::to_string(i + 1);
      const std::string idx = std::to_string(i);
      b.flt("fx.multitap.tap" + idx, "Tap " + n + " Beat", "T" + n + " Beat", kUnitMultiTap,
            base + offsetof(FX, multitap_taps) + i * sizeof(float), 0.125f, 2.0f, tap_beats[i],
            "beats", 3);
      b.flt("fx.multitap.tap" + idx + "_level", "Tap " + n + " Level", "T" + n + " Lvl", kUnitMultiTap,
            base + offsetof(FX, multitap_levels) + i * sizeof(float), 0.0f, 1.0f, tap_levels[i]);
    }
  }

  // --- Arpeggiator -----------------------------------------------------------------
  {
    const size_t base = offsetof(Synth, arp);
    b.boolean("arp.enabled", "Arp On", "Arp", kUnitArp, base + offsetof(Arpeggiator, enabled), false);
    b.choice("arp.mode", "Arp Mode", "Mode", kUnitArp, base + offsetof(Arpeggiator, mode),
             {"OFF", "CHORD", "UP", "DOWN", "UP/DOWN", "PENDULUM", "CONVERGE", "DIVERGE", "LEAPFROG",
              "THUMB-UP", "THUMB-DOWN", "PINKY-UP", "PINKY-DOWN", "REPEAT", "RANDOM", "RANDOM WALK",
              "SHUFFLE", "ORDER"},
             ARP_UP);
    b.choice("arp.rate", "Arp Rate", "Rate", kUnitArp, base + offsetof(Arpeggiator, rate),
             {"1/4", "1/8", "1/16", "1/32"}, RATE_EIGHTH);
    b.flt("arp.tempo", "Arp Tempo", "Tempo", kUnitArp, base + offsetof(Arpeggiator, tempo),
          30.0f, 240.0f, 120.0f, "BPM", 0);
    b.integer("arp.octave", "Arp Octave", "Octave", kUnitArp, base + offsetof(Arpeggiator, octave), 0, 4, 0);
    b.integer("arp.octaves", "Arp Octaves", "Octaves", kUnitArp, base + offsetof(Arpeggiator, octaves), 1, 6, 3);
    b.flt("arp.gate_length", "Arp Gate", "Gate", kUnitArp, base + offsetof(Arpeggiator, gate_length),
          0.1f, 1.0f, 0.8f);
    b.boolean("arp.polyphonic", "Arp Poly", "Poly", kUnitArp, base + offsetof(Arpeggiator, polyphonic), true);
    b.boolean("arp.hold", "Arp Hold", "Hold", kUnitArp, base + offsetof(Arpeggiator, hold), false);
    b.choice("arp.chord_type", "Arp Chord", "Chord", kUnitArp, base + offsetof(Arpeggiator, chord_type),
             {"MAJOR", "MINOR", "SUS", "DIM"}, CHORD_MAJOR);
    b.boolean("arp.add_6", "Arp 6th", "6th", kUnitArp, base + offsetof(Arpeggiator, add_6), false);
    b.boolean("arp.add_m7", "Arp m7", "m7", kUnitArp, base + offsetof(Arpeggiator, add_m7), false);
    b.boolean("arp.add_M7", "Arp Maj7", "Maj7", kUnitArp, base + offsetof(Arpeggiator, add_M7), false);
    b.boolean("arp.add_9", "Arp 9th", "9th", kUnitArp, base + offsetof(Arpeggiator, add_9), false);
    b.integer("arp.voicing", "Arp Voicing", "Voicing", kUnitArp, base + offsetof(Arpeggiator, voicing), 0, 16, 0);

    // Plug-in only: follow the host tempo. Not a synth_set_param() key.
    b.boolean("host_sync", "Host Tempo Sync", "Sync", kUnitArp, offsetof(Synth, host_sync), true)
        .applyKey.clear();
  }

  // --- Performance --------------------------------------------------------------------
  b.flt(kPitchBendKey, "Pitch Bend", "Bend", kUnitPerformance, offsetof(Synth, pitch_bend),
        -2.0f, 2.0f, 0.0f, "st", 2).applyKey.clear();

  return std::move(b.list);
}

struct Table {
  std::vector<ParamDef> list = buildParams();
  std::unordered_map<uint32_t, int> byId;
  Table() {
    for (size_t i = 0; i < list.size(); ++i) {
      if (!byId.emplace(list[i].id, (int)i).second) {
        // Two keys hashed to the same ID. Pick a different key for one of them.
        std::fprintf(stderr, "sdl2-synth: duplicate parameter id for '%s'\n", list[i].key.c_str());
        std::abort();
      }
    }
  }
};

const Table& table() {
  static const Table t;
  return t;
}

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

} // namespace

const std::vector<ParamDef>& params() { return table().list; }
const std::vector<std::string>& unitNames() { return kUnitNames; }

int paramIndex(uint32_t id) {
  const Table& t = table();
  auto it = t.byId.find(id);
  return it == t.byId.end() ? -1 : it->second;
}

int paramIndexByKey(const std::string& key) {
  const std::vector<ParamDef>& list = table().list;
  for (size_t i = 0; i < list.size(); ++i) {
    if (list[i].key == key)
      return (int)i;
  }
  return -1;
}

int stepCount(const ParamDef& p) {
  switch (p.kind) {
    case ParamKind::Float: return 0;
    case ParamKind::Bool: return 1;
    case ParamKind::Int:
    case ParamKind::Choice: return (int)std::lround(p.max - p.min);
  }
  return 0;
}

float clampPlain(const ParamDef& p, float plain) {
  plain = clampf(plain, p.min, p.max);
  if (p.kind != ParamKind::Float)
    plain = std::floor(plain + 0.5f);
  return plain;
}

double plainToNormalized(const ParamDef& p, double plain) {
  const double lo = p.min, hi = p.max;
  plain = std::min(std::max(plain, lo), hi);
  if (hi <= lo)
    return 0.0;
  if (p.logarithmic && lo > 0.0)
    return std::log(plain / lo) / std::log(hi / lo);
  return (plain - lo) / (hi - lo);
}

double normalizedToPlain(const ParamDef& p, double normalized) {
  normalized = std::min(std::max(normalized, 0.0), 1.0);
  const double lo = p.min, hi = p.max;
  double plain;
  if (p.logarithmic && lo > 0.0)
    plain = lo * std::pow(hi / lo, normalized);
  else
    plain = lo + normalized * (hi - lo);
  if (p.kind != ParamKind::Float)
    plain = std::floor(plain + 0.5);
  return plain;
}

float readPlain(const Synth& s, const ParamDef& p) {
  const char* base = reinterpret_cast<const char*>(&s) + p.offset;
  if (p.floatField) {
    float v;
    std::memcpy(&v, base, sizeof(v));
    return v;
  }
  int v;
  std::memcpy(&v, base, sizeof(v));
  return (float)v;
}

void writePlain(Synth& s, const ParamDef& p, float plain) {
  char* base = reinterpret_cast<char*>(&s) + p.offset;
  plain = clampPlain(p, plain);
  if (p.floatField) {
    std::memcpy(base, &plain, sizeof(plain));
  } else {
    int v = (int)std::lround(plain);
    std::memcpy(base, &v, sizeof(v));
  }
}

void applyPlain(Synth& s, const ParamDef& p, float plain) {
  plain = clampPlain(p, plain);
  if (p.applyKey.empty())
    writePlain(s, p, plain);
  else
    synth_set_param(&s, p.applyKey.c_str(), plain);
}

std::string formatValue(const ParamDef& p, double plain) {
  char buf[64];
  switch (p.kind) {
    case ParamKind::Bool:
      return plain >= 0.5 ? "On" : "Off";
    case ParamKind::Choice: {
      int i = (int)std::lround(plain);
      i = std::min(std::max(i, 0), (int)p.choices.size() - 1);
      return p.choices[(size_t)i];
    }
    case ParamKind::Int:
      std::snprintf(buf, sizeof(buf), "%d", (int)std::lround(plain));
      return buf;
    case ParamKind::Float:
      std::snprintf(buf, sizeof(buf), "%.*f", p.precision, plain);
      return buf;
  }
  return std::string();
}

bool parseValue(const ParamDef& p, const std::string& text, double& plain) {
  if (p.kind == ParamKind::Choice) {
    for (size_t i = 0; i < p.choices.size(); ++i) {
      if (text.size() == p.choices[i].size() &&
          std::equal(text.begin(), text.end(), p.choices[i].begin(), [](char a, char b) {
            return std::tolower((unsigned char)a) == std::tolower((unsigned char)b);
          })) {
        plain = (double)i;
        return true;
      }
    }
  }
  if (p.kind == ParamKind::Bool) {
    std::string t;
    for (char c : text)
      t.push_back((char)std::tolower((unsigned char)c));
    if (t == "on" || t == "true" || t == "yes") { plain = 1.0; return true; }
    if (t == "off" || t == "false" || t == "no") { plain = 0.0; return true; }
  }
  char* end = nullptr;
  double v = std::strtod(text.c_str(), &end);
  if (end == text.c_str())
    return false;
  plain = std::min(std::max(v, (double)p.min), (double)p.max);
  return true;
}

const std::vector<CcAssignment>& midiCcAssignments() {
  static const std::vector<CcAssignment> list = {
    {1, "lfo1.depth"},            // Mod wheel -> vibrato depth
    {7, "mixer.master"},          // Channel volume
    {10, "mixer.master.pan"},     // Pan
    {71, "fx.filter.resonance"},  // Sound controller 2: resonance
    {72, "adsr.release"},         // Sound controller 3: release time
    {73, "adsr.attack"},          // Sound controller 4: attack time
    {74, "fx.filter.cutoff"},     // Sound controller 5: brightness
    {75, "adsr.decay"},           // Sound controller 6: decay time
    {91, "fx.reverb.mix"},        // Effects 1 depth: reverb
    {93, "fx.flanger.depth"},     // Effects 3 depth: chorus
    {94, "fx.delay.mix"},         // Effects 4 depth: celeste/detune
  };
  return list;
}

} // namespace sdl2synth
