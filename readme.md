# Sunflower

**Sunflower** is a two-axis solar-tracker prototype built around the Silicon Labs **BGM220P Explorer Kit (BRD4314A)**. The current firmware controls two SG90 servos over **Bluetooth Low Energy (BLE)** using a phone controller. Automatic tracking from light sensors and harvested-power operation are future stages.

## Current status

- **Working milestone (previous bench test):** Both servos responded to Android Uni Controller joystick buttons over BLE.
- **Current repository:** The rebuilt `Sunflower` project compiles successfully. Reflash and retest the hardware after the project recreation.
- **Not implemented yet:** Automatic light tracking, solar energy harvesting, battery management, or autonomous mode switching.

The earlier servo-sweep experiment is preserved separately in [Sunflower_Test](https://github.com/jumazer/Sunflower_Test). This repository is for the newer BLE-based firmware.

## Hardware and wiring

| Function | BGM220P pin | Peripheral | Notes |
| --- | --- | --- | --- |
| Pan / horizontal servo | PB4 | TIMER0 CC0 | SG90 signal wire |
| Tilt / vertical servo | PB3 | TIMER0 CC1 | SG90 signal wire |
| Servo power | External regulated 5 V | — | Size for both motors, including startup/stall transients |
| Ground | Shared ground | — | Servo supply GND and BGM220P GND must be connected |

**Electrical caution:** Do not power both motors from a GPIO pin. Verify servo wiring and supply voltage before connecting power. Keep the mechanisms within their tested limits; a stalled servo may draw substantial current.

## PWM and movement

PWM is produced by TIMER0 at approximately **50 Hz** (20 ms period), using two independent compare channels. Values below are the **prototype's calibrated pulse limits**, not universal SG90 limits.

| Axis | Initial pulse | Allowed pulse range | Joystick step |
| --- | ---: | ---: | ---: |
| Pan (PB4) | 1500 µs | 1000–2000 µs | 20 µs |
| Tilt (PB3) | 750 µs | 600–900 µs | 10 µs |

Movement is updated every **50 ms**. If valid controller packets stop arriving for **300 ms**, the firmware clears held buttons and stops changing the servo commands; it does **not** cut servo power or automatically return to center. On disconnect it also clears held buttons.

The narrow tilt range protects the current mechanical arrangement. Recalibrate before changing the mechanism.

## Bluetooth interface

The GATT definitions are in **`config/btconf/gatt_configuration.btconf`**. The active application logic and BLE event handler are in **`app.c`**.

| GATT characteristic ID | Payload | Purpose |
| --- | --- | --- |
| `servo1_position` | One byte, 0–100 | Set pan position directly |
| `servo2_position` | One byte, 0–100 | Set tilt position directly |
| `controller_cmd` | Fixed 10-byte packet | Receive Uni Controller button state |

The controller characteristic accepts writes with or without response. The firmware checks the **FA FB** header, **FE** ending, and 10-byte length; byte **6** (zero-based) contains these button flags:

| Button | Mask |
| --- | --- |
| Up | `0x01` |
| Down | `0x02` |
| Left | `0x04` |
| Right | `0x08` |

Only the four low button bits are used. The firmware does not currently validate a checksum.

**Note:** The current GATT configuration still initializes the device name as **“Empty Example”**, inherited from the Silicon Labs starter project. The project itself is named Sunflower. Rename the GATT Device Name through Simplicity Studio later if desired, followed by regeneration and a new flash.

## Build and flash

1. Open **`Sunflower.slcp`** in **Simplicity Studio 6**.
2. Use **Simplicity SDK 2026.6.2** and the **BRD4314A / BGM220P** target.
3. Generate the project if prompted. Keep the GATT characteristic IDs above unchanged unless you also update `app.c`.
4. Build and flash to the Explorer Kit.
5. Connect from a BLE client. To use the directional controls, configure Uni Controller to write the specified 10-byte packets to `controller_cmd`.

If the SDK Power Manager settings change, regenerate the project and test the build again. Do not edit the copied Silicon Labs SDK files directly.

## Repository layout

```text
Sunflower/
├── Sunflower.slcp                # Simplicity Studio project
├── app.c                         # Active BLE + two-servo firmware
├── app.h
├── app_bm.c
├── main.c
├── Sunflower.pintool
├── config/
│   └── btconf/
│       └── gatt_configuration.btconf
├── cmake_gcc/                    # Generated project build configuration
├── future/                       # Inactive starter templates, not built
│   ├── README.md
│   ├── bluetooth_control.c
│   ├── free_roam.c
│   └── sun_tracking.c
├── image/                        # Images from the original SDK example
├── .gitignore
└── readme.md
```

The repository excludes local copies of `simplicity_sdk_*/`, `autogen/`, and `cmake_gcc/build/`. These are recreated locally and should not be manually copied into Git.

## Planned work

- Refactor active servo and BLE code into reusable modules if complexity grows.
- Add four light sensors and ADC sampling.
- Develop closed-loop sun tracking with a deadband, safe motion limits, and one-axis-at-a-time movement where appropriate.
- Test whether tracking energy gain exceeds movement energy cost.
- Integrate the battery, energy-harvesting PMIC, custom PCB, and power management.

See [future/README.md](future/README.md) for the original nonfunctional starter templates. **Do not add them to the Simplicity Studio build as-is.**

## Safety and reproducibility

Commit the **`.slcp` descriptor and GATT `.btconf` file** whenever the Bluetooth configuration changes. Back up the project before moving or renaming it outside Simplicity Studio, and retest both servo axes after any rebuild or wiring change.
