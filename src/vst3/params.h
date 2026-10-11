#pragma once

// Parameter table of the VST3 plug-in.
//
// Every automatable value of the synth is described once here: its range,
// default, how it maps to the host's normalised 0..1 range, where it lives
// inside the Synth struct and how to apply it to a running engine. The
// processor, the controller (host-facing parameter list) and the editor all
// work from this table.

#include "synth.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sdl2synth {

enum class ParamKind {
  Float,  // continuous
  Int,    // integer steps from min to max
  Bool,   // on/off toggle
  Choice, // list of named values, index 0..choices.size()-1
};

struct ParamDef {
  uint32_t id = 0;       // VST3 ParamID: stable hash of `key`
  std::string key;       // stable identifier, e.g. "osc1.pitch"; never rename
  std::string title;     // e.g. "OSC1 Pitch"
  std::string shortTitle;
  std::string units;     // "Hz", "dB", ...
  int unit = 0;          // index into unitNames()
  ParamKind kind = ParamKind::Float;
  float min = 0.0f, max = 1.0f, def = 0.0f;
  bool logarithmic = false; // host-side mapping only (needs min > 0)
  std::vector<std::string> choices;
  int precision = 2;

  // Location inside Synth. The field is a float, or an int/enum for the
  // Int, Bool and Choice kinds.
  size_t offset = 0;
  bool floatField = true;

  // Name understood by synth_set_param(); when empty the field is written
  // directly.
  std::string applyKey;
};

const std::vector<ParamDef>& params();
const std::vector<std::string>& unitNames();

// Lookup by VST3 ParamID or by key. Return -1 when unknown.
int paramIndex(uint32_t id);
int paramIndexByKey(const std::string& key);

// Plain <-> normalised conversion (plain is in the units of the field).
double plainToNormalized(const ParamDef& p, double plain);
double normalizedToPlain(const ParamDef& p, double normalized);
// Number of discrete steps (0 = continuous), as VST3's stepCount.
int stepCount(const ParamDef& p);
float clampPlain(const ParamDef& p, float plain);

// Reads the current value of the parameter out of a Synth.
float readPlain(const Synth& s, const ParamDef& p);
// Writes the field only. Used for UI-side models that have no live engine.
void writePlain(Synth& s, const ParamDef& p, float plain);
// Writes the value and triggers whatever the engine needs to react to it
// (envelope copies into voices, compressor coefficients, ...).
void applyPlain(Synth& s, const ParamDef& p, float plain);

// Human readable value without units ("0.50", "SAW", "On").
std::string formatValue(const ParamDef& p, double plain);
// Parses text typed by the user; returns false when it is not understood.
bool parseValue(const ParamDef& p, const std::string& text, double& plain);

// MIDI controller -> parameter assignments exposed to the host (IMidiMapping).
struct CcAssignment {
  int cc;
  const char* key;
};
const std::vector<CcAssignment>& midiCcAssignments();

// Key of the parameter driven by the pitch wheel.
extern const char* const kPitchBendKey;

} // namespace sdl2synth
