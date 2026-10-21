# Planned firmware modules — reference templates

These files were copied from the earlier [Sunflower_Test](https://github.com/jumazer/Sunflower_Test) project for future organization. **None are part of the active `Sunflower` build.** They are deliberately incomplete and should not be added to `Sunflower.slcp` without implementing and testing them.

| Template | Intended future use | Current implementation |
| --- | --- | --- |
| `free_roam.c` | Separate manual pan/tilt movement API | Manual control already works through BLE in `../app.c`; this file is only a stub |
| `sun_tracking.c` | Four-light-sensor closed-loop aiming | Not implemented |
| `bluetooth_control.c` | Modular Bluetooth protocol handler | BLE advertising and command handling already work in `../app.c`; this file is only a stub |

## Suggested integration order

1. Preserve the working BLE firmware as the baseline.
2. Create a shared servo driver and move only the verified PWM/range logic from `app.c`.
3. Implement and test automatic sun tracking with real light sensors and a movement deadband.
4. Refactor Bluetooth/manual control into separate files when doing so offers a clear benefit.
5. Add any completed module through the Simplicity Studio `.slcp` project, regenerate, and build.

**Calibration:** The current BLE firmware uses **1000–2000 µs** on PB4 (pan) and **600–900 µs** on PB3 (tilt). The old sweep project's wider calibration does not apply automatically to this mechanism.

**Electrical caution:** Supply the servos appropriately, connect grounds, and avoid commanding a mechanical stall.
