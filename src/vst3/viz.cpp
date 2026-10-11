#include "viz.h"

#include <mutex>
#include <random>
#include <unordered_map>

namespace sdl2synth {

namespace {
std::mutex& registryMutex() {
  static std::mutex m;
  return m;
}
std::unordered_map<uint64_t, std::weak_ptr<VizData>>& registry() {
  static std::unordered_map<uint64_t, std::weak_ptr<VizData>> r;
  return r;
}
} // namespace

std::shared_ptr<VizData> createViz(uint64_t& token) {
  auto data = std::make_shared<VizData>();
  std::lock_guard<std::mutex> lock(registryMutex());
  std::random_device rd;
  do {
    token = ((uint64_t)rd() << 32) ^ (uint64_t)rd();
  } while (token == 0 || registry().count(token));
  registry()[token] = data;
  return data;
}

std::shared_ptr<VizData> findViz(uint64_t token) {
  std::lock_guard<std::mutex> lock(registryMutex());
  auto it = registry().find(token);
  return it == registry().end() ? nullptr : it->second.lock();
}

void releaseViz(uint64_t token) {
  std::lock_guard<std::mutex> lock(registryMutex());
  registry().erase(token);
}

} // namespace sdl2synth
