#pragma once

// Flushes denormal floats to zero for the lifetime of the object. Reverb and
// delay feedback paths decay into denormals, which are extremely slow on some
// CPUs. The previous floating point state is restored on destruction so the
// host is not affected.

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#endif
#include <cstdint>

namespace sdl2synth {

class ScopedFlushDenormals {
public:
  ScopedFlushDenormals() {
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
    saved_ = _mm_getcsr();
    _mm_setcsr(saved_ | 0x8040); // FTZ | DAZ
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    uint64_t fpcr;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
    saved_ = fpcr;
    __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr | (1ull << 24))); // FZ
#endif
  }
  ~ScopedFlushDenormals() {
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
    _mm_setcsr((unsigned int)saved_);
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("msr fpcr, %0" : : "r"((uint64_t)saved_));
#endif
  }
  ScopedFlushDenormals(const ScopedFlushDenormals&) = delete;
  ScopedFlushDenormals& operator=(const ScopedFlushDenormals&) = delete;

private:
  uint64_t saved_ = 0;
};

} // namespace sdl2synth
