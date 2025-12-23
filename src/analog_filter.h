#pragma once

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// Filter types
typedef enum {
    FILTER_LOWPASS = 0,
    FILTER_HIGHPASS,
    FILTER_BANDPASS,
    FILTER_NOTCH
} FilterType;

// Oversampling rates
typedef enum {
    OVERSAMPLING_1X = 1,
    OVERSAMPLING_2X = 2,
    OVERSAMPLING_4X = 4,
    OVERSAMPLING_8X = 8
} OversamplingRate;

typedef struct {
    // Filter parameters
    FilterType type;
    float cutoff;        // 20Hz - 20kHz
    float resonance;     // Q factor (0.1 - 10.0)
    float drive;          // Drive amount (0.0 - 10.0)
    float mix;            // Wet/Dry mix (0.0 - 1.0)
    
    // Smoothing parameters (to prevent zipper noise)
    float cutoff_smooth;
    float resonance_smooth;
    float drive_smooth;
    float smoothing_coeff; // Inertial smoothing coefficient
    
    // Oversampling
    OversamplingRate oversampling;
    float *oversample_buffer;
    int oversample_buffer_size;
    
    // Filter state (biquad coefficients)
    float x1, x2, y1, y2;  // Delay lines for each channel
    float a0, a1, a2, b1, b2;  // Filter coefficients
    
    // Soft saturation state
    float sat_state;
    
    // Audio processing parameters
    float sample_rate;
    int initialized;
} AnalogFilter;

// Initialize the analog filter
void analog_filter_init(AnalogFilter *filter, float sample_rate);
void analog_filter_set_param(AnalogFilter *filter, const char *param, float value);
void analog_filter_process(AnalogFilter *filter, float *input, float *output, int frames);
float analog_filter_process_sample(AnalogFilter *filter, float input);
void analog_filter_set_type(AnalogFilter *filter, FilterType type);
void analog_filter_update_coefficients(AnalogFilter *filter);
float analog_filter_soft_saturation(float input, float drive);
void analog_filter_cleanup(AnalogFilter *filter);

#ifdef __cplusplus
}
#endif