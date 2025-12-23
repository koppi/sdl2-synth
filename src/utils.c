#include "utils.h"
#include <math.h>

// Fast sine approximation using polynomial approximation
// Based on Taylor series with optimizations for audio range
float fastsin(float x) {
    // Normalize to [0, 2π]
    const float PI = 3.141592653589793f;
    const float TWO_PI = 2.0f * PI;
    
    // Wrap input to [0, 2π] range
    x = x - TWO_PI * floorf(x / TWO_PI);
    
    // Map to [0, π] for symmetric calculation
    float sign = 1.0f;
    if (x > PI) {
        x = TWO_PI - x;
        sign = -1.0f;
    }
    
    // Map to [0, π/2] for more accurate approximation
    if (x > PI * 0.5f) {
        x = PI - x;
    }
    
    // Polynomial approximation for sin(x) on [0, π/2]
    // sin(x) ≈ x * (1 - x² * (1/6 - x² * (1/120)))
    float x2 = x * x;
    float x4 = x2 * x2;
    float x6 = x4 * x2;
    
    float result = x * (1.0f - x2 * (0.1666666667f - x4 * 0.0083333333f));
    
    return sign * result;
}

// Fast cosine using sin with phase shift
float fastcos(float x) {
    return fastsin(x + 1.570796326794897f); // π/2
}