/***************************************************************************//**
 * @file app.c
 * @brief Sunflower - Bluetooth Servo Control
 ******************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "sl_bt_api.h"
#include "sl_main_init.h"
#include "app_assert.h"
#include "app.h"
#include "gatt_db.h"

#include "em_cmu.h"
#include "em_gpio.h"
#include "em_timer.h"

#include "sl_component_catalog.h"
#include "sl_sleeptimer.h"

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

// Servo limits (microseconds)
#define SERVO1_MIN_US  1000U
#define SERVO1_MAX_US  2000U

#define SERVO2_MIN_US  600U
#define SERVO2_MAX_US  900U

// Movement settings
#define SERVO1_STEP_US  20U
#define SERVO2_STEP_US  10U
#define SERVO_TICK_MS   50U

// Stop movement if BLE packets stop arriving
#define COMMAND_TIMEOUT_MS 300U

// Uni Controller button bitmasks
#define BUTTON_UP     0x01
#define BUTTON_DOWN   0x02
#define BUTTON_LEFT   0x04
#define BUTTON_RIGHT  0x08

// Hardware timer frequency
static uint32_t timerFreq;

// Advertising handle
static uint8_t advertising_set_handle = 0xff;

// Servo positions
static uint16_t servo1Pulse = 1500U;
static uint16_t servo2Pulse = 750U;

// Last received button state
static uint8_t buttonsHeld = 0;
static uint16_t commandAgeMs = COMMAND_TIMEOUT_MS;

// Movement timer
static sl_sleeptimer_timer_handle_t movementTimer;
static volatile bool movementDue = false;


/***************************************************************************//**
 * Convert microseconds to TIMER0 ticks.
 ******************************************************************************/
static uint32_t pulseToTicks(uint16_t pulseUs)
{
    return (uint32_t)(((uint64_t)timerFreq * pulseUs)
                      / 1000000ULL);
}


/***************************************************************************//**
 * Convert BLE position 0-100 into pulse width.
 ******************************************************************************/
static uint16_t positionToPulse(uint8_t position,
                                uint16_t minUs,
                                uint16_t maxUs)
{
    return minUs +
        (uint16_t)(((uint32_t)(maxUs - minUs) * position)
                   / 100U);
}


/***************************************************************************//**
 * Move toward a lower pulse width safely.
 ******************************************************************************/
static uint16_t moveDown(uint16_t pulse,
                         uint16_t minPulse,
                         uint16_t step)
{
    if (pulse <= minPulse + step)
    {
        return minPulse;
    }

    return pulse - step;
}


/***************************************************************************//**
 * Move toward a higher pulse width safely.
 ******************************************************************************/
static uint16_t moveUp(uint16_t pulse,
                       uint16_t maxPulse,
                       uint16_t step)
{
    if (pulse >= maxPulse - step)
    {
        return maxPulse;
    }

    return pulse + step;
}


/***************************************************************************//**
 * Update PWM outputs.
 ******************************************************************************/
static void updateServos(void)
{
    TIMER_CompareBufSet(
        TIMER0, 0, pulseToTicks(servo1Pulse));

    TIMER_CompareBufSet(
        TIMER0, 1, pulseToTicks(servo2Pulse));
}


/***************************************************************************//**
 * Called periodically to request a movement update.
 ******************************************************************************/
static void movementCallback(sl_sleeptimer_timer_handle_t *handle,
                             void *data)
{
    (void)handle;
    (void)data;

    movementDue = true;
}


/***************************************************************************//**
 * Initialize servo hardware PWM.
 ******************************************************************************/
static void initServoPwm(void)
{
    CMU_ClockEnable(cmuClock_GPIO, true);
    CMU_ClockEnable(cmuClock_TIMER0, true);

    // Servo 1 - PB4
    GPIO_PinModeSet(gpioPortB, 4, gpioModePushPull, 0);

    // Servo 2 - PB3
    GPIO_PinModeSet(gpioPortB, 3, gpioModePushPull, 0);

    TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
    TIMER_InitCC_TypeDef ccInit = TIMER_INITCC_DEFAULT;

    timerInit.enable = false;
    timerInit.prescale = timerPrescale32;

    ccInit.mode = timerCCModePWM;

    TIMER_Init(TIMER0, &timerInit);

    TIMER_InitCC(TIMER0, 0, &ccInit);
    TIMER_InitCC(TIMER0, 1, &ccInit);

    // TIMER0 CC0 -> PB4
    GPIO->TIMERROUTE[0].CC0ROUTE =
        (gpioPortB << _GPIO_TIMER_CC0ROUTE_PORT_SHIFT) |
        (4 << _GPIO_TIMER_CC0ROUTE_PIN_SHIFT);

    // TIMER0 CC1 -> PB3
    GPIO->TIMERROUTE[0].CC1ROUTE =
        (gpioPortB << _GPIO_TIMER_CC1ROUTE_PORT_SHIFT) |
        (3 << _GPIO_TIMER_CC1ROUTE_PIN_SHIFT);

    // Enable both channels
    GPIO->TIMERROUTE[0].ROUTEEN =
        GPIO_TIMER_ROUTEEN_CC0PEN |
        GPIO_TIMER_ROUTEEN_CC1PEN;

    // Generate 50 Hz PWM
    timerFreq = CMU_ClockFreqGet(cmuClock_TIMER0) / 32U;

    TIMER_TopSet(TIMER0, (timerFreq / 50U) - 1U);

    // Initialize positions
    TIMER_CompareSet(TIMER0, 0, pulseToTicks(servo1Pulse));
    TIMER_CompareSet(TIMER0, 1, pulseToTicks(servo2Pulse));

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
#endif

    TIMER_Enable(TIMER0, true);
}


/***************************************************************************//**
 * Application initialization.
 ******************************************************************************/
void app_init(void)
{
    initServoPwm();

    // Update movement every 50 ms
    sl_status_t sc = sl_sleeptimer_start_periodic_timer_ms(
        &movementTimer,
        SERVO_TICK_MS,
        movementCallback,
        NULL,
        0,
        0
    );

    app_assert_status(sc);
}


/***************************************************************************//**
 * Application main loop.
 ******************************************************************************/
void app_process_action(void)
{
    if (!movementDue)
    {
        return;
    }

    movementDue = false;

    // Stop moving if Bluetooth commands stop arriving
    if (commandAgeMs < COMMAND_TIMEOUT_MS)
    {
        commandAgeMs += SERVO_TICK_MS;
    }

    if (commandAgeMs >= COMMAND_TIMEOUT_MS)
    {
        buttonsHeld = 0;
    }

    // Servo 1: horizontal rotation
    if ((buttonsHeld & BUTTON_LEFT) &&
        !(buttonsHeld & BUTTON_RIGHT))
    {
        servo1Pulse = moveDown(
            servo1Pulse,
            SERVO1_MIN_US,
            SERVO1_STEP_US);
    }
    else if ((buttonsHeld & BUTTON_RIGHT) &&
             !(buttonsHeld & BUTTON_LEFT))
    {
        servo1Pulse = moveUp(
            servo1Pulse,
            SERVO1_MAX_US,
            SERVO1_STEP_US);
    }

    // Servo 2: vertical tilt
    if ((buttonsHeld & BUTTON_UP) &&
        !(buttonsHeld & BUTTON_DOWN))
    {
        servo2Pulse = moveUp(
            servo2Pulse,
            SERVO2_MAX_US,
            SERVO2_STEP_US);
    }
    else if ((buttonsHeld & BUTTON_DOWN) &&
             !(buttonsHeld & BUTTON_UP))
    {
        servo2Pulse = moveDown(
            servo2Pulse,
            SERVO2_MIN_US,
            SERVO2_STEP_US);
    }

    updateServos();
}


/***************************************************************************//**
 * Bluetooth event handler.
 ******************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
    sl_status_t sc;

    switch (SL_BT_MSG_ID(evt->header))
    {
        case sl_bt_evt_system_boot_id:

            sc = sl_bt_advertiser_create_set(
                &advertising_set_handle);
            app_assert_status(sc);

            sc = sl_bt_legacy_advertiser_generate_data(
                advertising_set_handle,
                sl_bt_advertiser_general_discoverable);
            app_assert_status(sc);

            sc = sl_bt_advertiser_set_timing(
                advertising_set_handle,
                160,
                160,
                0,
                0);
            app_assert_status(sc);

            sc = sl_bt_legacy_advertiser_start(
                advertising_set_handle,
                sl_bt_legacy_advertiser_connectable);
            app_assert_status(sc);
            break;


        case sl_bt_evt_connection_opened_id:
            buttonsHeld = 0;
            commandAgeMs = COMMAND_TIMEOUT_MS;
            break;


        case sl_bt_evt_connection_closed_id:

            // Stop all movement on disconnect
            buttonsHeld = 0;
            commandAgeMs = COMMAND_TIMEOUT_MS;

            sc = sl_bt_legacy_advertiser_generate_data(
                advertising_set_handle,
                sl_bt_advertiser_general_discoverable);
            app_assert_status(sc);

            sc = sl_bt_legacy_advertiser_start(
                advertising_set_handle,
                sl_bt_legacy_advertiser_connectable);
            app_assert_status(sc);
            break;


        case sl_bt_evt_gatt_server_attribute_value_id:
        {
            const sl_bt_evt_gatt_server_attribute_value_t *write =
                &evt->data.evt_gatt_server_attribute_value;

            if (write->offset != 0U)
            {
                break;
            }

            // Uni Controller 10-byte command packet
            if (write->attribute ==
                gattdb_controller_cmd)
            {
                if (write->value.len != 10U)
                {
                    break;
                }

                const uint8_t *packet = write->value.data;

                // Validate header and footer
                if (packet[0] != 0xFA ||
                    packet[1] != 0xFB ||
                    packet[9] != 0xFE)
                {
                    break;
                }

                // Byte 7 contains button bitmask
                buttonsHeld = packet[6] & 0x0F;

                // Reset communication timeout
                commandAgeMs = 0U;
            }

            // Preserve original Servo 1 position control
            else if (write->attribute ==
                     gattdb_servo1_position)
            {
                if (write->value.len == 1U &&
                    write->value.data[0] <= 100U)
                {
                    buttonsHeld = 0;

                    servo1Pulse = positionToPulse(
                        write->value.data[0],
                        SERVO1_MIN_US,
                        SERVO1_MAX_US);

                    updateServos();
                }
            }

            // Preserve original Servo 2 position control
            else if (write->attribute ==
                     gattdb_servo2_position)
            {
                if (write->value.len == 1U &&
                    write->value.data[0] <= 100U)
                {
                    buttonsHeld = 0;

                    servo2Pulse = positionToPulse(
                        write->value.data[0],
                        SERVO2_MIN_US,
                        SERVO2_MAX_US);

                    updateServos();
                }
            }

            break;
        }

        default:
            break;
    }
}