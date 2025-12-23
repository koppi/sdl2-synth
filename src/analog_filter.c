#include "analog_filter.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

// Helper function for soft clipping
static float tanh_approx(float x) {
    // Fast tanh approximation
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

void analog_filter_init(AnalogFilter *filter, float sample_rate) {
    filter->sample_rate = sample_rate;
    filter->type = FILTER_LOWPASS;
    filter->cutoff = 1000.0f;        // 1kHz default
    filter->resonance = 1.0f;        // Q = 1.0 default
    filter->drive = 1.0f;             // No drive default
    filter->mix = 1.0f;               // 100% wet default
    
    // Initialize smoothing parameters
    filter->cutoff_smooth = filter->cutoff;
    filter->resonance_smooth = filter->resonance;
    filter->drive_smooth = filter->drive;
    filter->smoothing_coeff = 0.999f; // High smoothing for zipper noise prevention
    
    // Initialize oversampling
    filter->oversampling = OVERSAMPLING_2X;
    filter->oversample_buffer_size = 4096; // Large enough for 8x oversampling
    filter->oversample_buffer = malloc(filter->oversample_buffer_size * sizeof(float));
    
    // Initialize filter state
    filter->x1 = filter->x2 = filter->y1 = filter->y2 = 0.0f;
    filter->sat_state = 0.0f;
    
    // Calculate initial coefficients
    analog_filter_update_coefficients(filter);
    
    filter->initialized = 1;
}

void analog_filter_set_param(AnalogFilter *filter, const char *param, float value) {
    if (!strcmp(param, "type")) {
        filter->type = (FilterType)((int)value);
        analog_filter_set_type(filter, filter->type);
    }
    else if (!strcmp(param, "cutoff")) {
        // Clamp cutoff to 20Hz - 20kHz range
        filter->cutoff = fmaxf(20.0f, fminf(20000.0f, value));
    }
    else if (!strcmp(param, "resonance")) {
        // Clamp resonance to 0.1 - 10.0 range
        filter->resonance = fmaxf(0.1f, fminf(10.0f, value));
    }
    else if (!strcmp(param, "drive")) {
        // Clamp drive to 0.0 - 10.0 range
        filter->drive = fmaxf(0.0f, fminf(10.0f, value));
    }
    else if (!strcmp(param, "mix")) {
        // Clamp mix to 0.0 - 1.0 range
        filter->mix = fmaxf(0.0f, fminf(1.0f, value));
    }
    else if (!strcmp(param, "oversampling")) {
        filter->oversampling = (OversamplingRate)((int)value);
    }
    else if (!strcmp(param, "smoothing")) {
        filter->smoothing_coeff = fmaxf(0.9f, fminf(0.9999f, value));
    }
}

void analog_filter_set_type(AnalogFilter *filter, FilterType type) {
    filter->type = type;
    analog_filter_update_coefficients(filter);
}

void analog_filter_update_coefficients(AnalogFilter *filter) {
    // Use smoothed parameters for coefficient calculation
    float cutoff = filter->cutoff_smooth;
    float resonance = filter->resonance_smooth;
    
    // Prevent cutoff from going too close to Nyquist
    cutoff = fminf(cutoff, filter->sample_rate * 0.45f);
    
    // Normalize frequency
    float omega = 2.0f * 3.14159265359f * cutoff / filter->sample_rate;
    float sin_omega = sinf(omega);
    float cos_omega = cosf(omega);
    float alpha = sin_omega / (2.0f * resonance);
    
    // Calculate biquad coefficients based on filter type
    switch (filter->type) {
        case FILTER_LOWPASS:
            filter->a0 = (1.0f - cos_omega) / 2.0f;
            filter->a1 = 1.0f - cos_omega;
            filter->a2 = (1.0f - cos_omega) / 2.0f;
            break;
            
        case FILTER_HIGHPASS:
            filter->a0 = (1.0f + cos_omega) / 2.0f;
            filter->a1 = -(1.0f + cos_omega);
            filter->a2 = (1.0f + cos_omega) / 2.0f;
            break;
            
        case FILTER_BANDPASS:
            filter->a0 = alpha;
            filter->a1 = 0.0f;
            filter->a2 = -alpha;
            break;
            
        case FILTER_NOTCH:
            filter->a0 = 1.0f;
            filter->a1 = -2.0f * cos_omega;
            filter->a2 = 1.0f;
            break;
            
        default:
            filter->a0 = 1.0f;
            filter->a1 = 0.0f;
            filter->a2 = 0.0f;
            break;
    }
    
    // Common denominator coefficients
    float b0 = 1.0f + alpha;
    filter->b1 = -2.0f * cos_omega;
    filter->b2 = 1.0f - alpha;
    
    // Normalize coefficients
    filter->a0 /= b0;
    filter->a1 /= b0;
    filter->a2 /= b0;
    filter->b1 /= b0;
    filter->b2 /= b0;
}

float analog_filter_soft_saturation(float input, float drive) {
    if (drive <= 1.0f) {
        return input; // No saturation needed
    }
    
    // Apply drive gain
    float driven = input * drive;
    
    // Soft saturation using tanh approximation
    float saturated = tanh_approx(driven * 0.5f) * 2.0f;
    
    // Mix original and saturated signal for warmth
    float mix = fminf(1.0f, (drive - 1.0f) / 4.0f); // Gradual saturation mix
    return input * (1.0f - mix) + saturated * mix;
}

float analog_filter_process_sample(AnalogFilter *filter, float input) {
    if (!filter->initialized) {
        return input;
    }
    
    // Apply smoothing to parameters (inertial smoothing)
    filter->cutoff_smooth += (filter->cutoff - filter->cutoff_smooth) * (1.0f - filter->smoothing_coeff);
    filter->resonance_smooth += (filter->resonance - filter->resonance_smooth) * (1.0f - filter->smoothing_coeff);
    filter->drive_smooth += (filter->drive - filter->drive_smooth) * (1.0f - filter->smoothing_coeff);
    
    // Update coefficients if parameters changed significantly
    if (fabsf(filter->cutoff - filter->cutoff_smooth) > 0.1f ||
        fabsf(filter->resonance - filter->resonance_smooth) > 0.01f) {
        analog_filter_update_coefficients(filter);
    }
    
    // Apply soft saturation
    float saturated_input = analog_filter_soft_saturation(input, filter->drive_smooth);
    
    // Biquad difference equation
    float output = filter->a0 * saturated_input + 
                   filter->a1 * filter->x1 + 
                   filter->a2 * filter->x2 - 
                   filter->b1 * filter->y1 - 
                   filter->b2 * filter->y2;
    
    // Update delay lines
    filter->x2 = filter->x1;
    filter->x1 = saturated_input;
    filter->y2 = filter->y1;
    filter->y1 = output;
    
    // Apply wet/dry mix
    return input * (1.0f - filter->mix) + output * filter->mix;
}

void analog_filter_process(AnalogFilter *filter, float *input, float *output, int frames) {
    if (!filter->initialized || filter->oversampling == OVERSAMPLING_1X) {
        // No oversampling - process directly
        for (int i = 0; i < frames; i++) {
            output[i] = analog_filter_process_sample(filter, input[i]);
        }
        return;
    }
    
    int oversample_factor = (int)filter->oversampling;
    int upsampled_frames = frames * oversample_factor;
    
    // Ensure oversample buffer is large enough
    if (upsampled_frames > filter->oversample_buffer_size) {
        filter->oversample_buffer = realloc(filter->oversample_buffer, 
                                           upsampled_frames * sizeof(float));
        filter->oversample_buffer_size = upsampled_frames;
    }
    
    // Upsample using zero-order hold (simple but effective)
    for (int i = 0; i < frames; i++) {
        for (int j = 0; j < oversample_factor; j++) {
            filter->oversample_buffer[i * oversample_factor + j] = input[i];
        }
    }
    
    // Process at higher sample rate
    float original_sample_rate = filter->sample_rate;
    filter->sample_rate *= oversample_factor;
    analog_filter_update_coefficients(filter);
    
    for (int i = 0; i < upsampled_frames; i++) {
        filter->oversample_buffer[i] = analog_filter_process_sample(filter, 
                                                                   filter->oversample_buffer[i]);
    }
    
    // Downsample using simple averaging
    for (int i = 0; i < frames; i++) {
        float sum = 0.0f;
        for (int j = 0; j < oversample_factor; j++) {
            sum += filter->oversample_buffer[i * oversample_factor + j];
        }
        output[i] = sum / oversample_factor;
    }
    
    // Restore original sample rate
    filter->sample_rate = original_sample_rate;
    analog_filter_update_coefficients(filter);
}

void analog_filter_cleanup(AnalogFilter *filter) {
    if (filter->oversample_buffer) {
        free(filter->oversample_buffer);
        filter->oversample_buffer = NULL;
    }
    filter->initialized = 0;
}