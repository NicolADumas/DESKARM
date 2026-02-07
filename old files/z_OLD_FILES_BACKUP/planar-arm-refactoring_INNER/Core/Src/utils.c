#include "utils.h"

float calculate_slope(ringbuffer_t *buffer, uint8_t num_points, float dt) {
    if (buffer->length < num_points) {
        return 0.0f; // Not enough data yet
    }

    // Formulas for linear regression slope (m) for y = mx + c
    // m = (N * Σ(xy) - Σx * Σy) / (N * Σ(x^2) - (Σx)^2)
    // Since our time steps (x) are uniform (0, 1, 2...), we can pre-calculate Σx and Σ(x^2)
    // and simplify the formula.
    // Let x be the sample index (0, 1, ..., N-1)
    
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f;
    float y;

    for (uint8_t i = 0; i < num_points; i++) {
        // rbgetoffset gets the i-th most recent element (0 is the newest)
        rbgetoffset(buffer, i, &y); 
        
        float x = (float)i;
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }

    float N = (float)num_points;
    float denominator = N * sum_x2 - sum_x * sum_x;

    if (fabs(denominator) < 1e-6) {
        return 0.0f; // Avoid division by zero
    }

    // The slope is calculated in (radians / sample_index). We convert it to (radians / second).
    // Note: The slope will be negative because we are looking back in time.
    float slope_rad_per_sample = (N * sum_xy - sum_x * sum_y) / denominator;
    
    // Convert from (rad/sample) to (rad/s) and invert the sign
    return -slope_rad_per_sample / dt;
}