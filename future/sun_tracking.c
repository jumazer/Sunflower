/*
 * Future feature: Automatic sun tracking.
 * TEMPLATE ONLY -- not included in the current Simplicity Studio build.
 *
 * Sensor selection, placement and ADC channels are still TBD.
 */
#include <stdint.h>

typedef struct {
    uint16_t top_left;
    uint16_t top_right;
    uint16_t bottom_left;
    uint16_t bottom_right;
} sunlight_readings_t;

void sun_tracking_update(const sunlight_readings_t *light)
{
    if (light == 0) {
        return;
    }

    // TODO: Compare (left vs right) and (top vs bottom) light levels.
    // TODO: Apply a deadband so small differences do not cause jitter.
    // TODO: Move one axis at a time, within calibrated servo limits.
    // TODO: Test whether energy gained exceeds tracking energy consumed.
}
