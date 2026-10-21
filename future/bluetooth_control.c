/*
 * Future feature: Bluetooth Low Energy (BLE) control.
 * TEMPLATE ONLY -- not included in the current Simplicity Studio build.
 *
 * The BGM220P has an integrated BLE radio. Phone joystick control can
 * use a writable GATT characteristic; no external USART is required.
 */
#include <stddef.h>
#include <stdint.h>

void bluetooth_control_init(void)
{
    // TODO: Add Silicon Labs Bluetooth stack components in the .slcp.
    // TODO: Define a GATT service and writable control characteristic.
    // TODO: Initialize advertising and handle connection events.
}

void bluetooth_control_on_write(const uint8_t *value, size_t length)
{
    // TODO: Validate packet length, format, and requested mode.
    // TODO: Decode desired pan/tilt targets for free-roam mode.
    // TODO: Clamp commands to safe limits before updating servos.
    (void)value;
    (void)length;
}
