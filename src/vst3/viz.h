#pragma once

// Visualisation data that flows from the audio processor to the editor.
//
// The editor lives in the controller, which VST3 allows to run apart from the
// processor. In practice both objects live in the same process, so the
// processor publishes its data under a random token and tells the controller
// the token through an IMessage. The controller looks the token up in a
// process-wide registry; if that fails (separate processes) the editor simply
// shows no scope/meter. Only a weak reference is stored in the registry, so
// neither side can outlive the data by accident.

#include "scope.h"

#include <atomic>
#include <cstdint>
#include <memory>

namespace sdl2synth {

struct VizData {
  ScopeBuffer scope;
  std::atomic<float> peakDb{-120.0f};
  std::atomic<float> rmsDb{-120.0f};
  std::atomic<uint64_t> blocks{0}; // incremented by every processed audio block

  VizData() { scope_init(&scope); }
};

// Creates the data and returns it together with the token it is registered as.
std::shared_ptr<VizData> createViz(uint64_t& token);
// Returns the data registered under `token`, or null if it no longer exists.
std::shared_ptr<VizData> findViz(uint64_t token);
// Removes the registration (called when the processor goes away).
void releaseViz(uint64_t token);

} // namespace sdl2synth
