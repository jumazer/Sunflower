/*
 * Future feature: Free-roam / manual control.
 * TEMPLATE ONLY -- not included in the current Simplicity Studio build.
 *
 * A BLE joystick or other user interface will set the desired pan/tilt
 * positions. The servo driver will translate these to PWM compare values.
 */
#include <stdint.h>

void free_roam_set_target_us(uint16_t pan_us, uint16_t tilt_us)
{
    // TODO: Clamp each target to its calibrated mechanical limits.
    // TODO: Pass the target positions to the shared two-servo controller.
    // TODO: Slew gradually rather than jumping to the commanded position.
    (void)pan_us;
    (void)tilt_us;
}
