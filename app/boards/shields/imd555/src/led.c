#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/keymap.h>
#include <zmk/split/bluetooth/peripheral.h>

#include <math.h>

// Blink                LONG    SHORT
// Caps ON              1       10
// Caps OFF             1       1

// BLE connected        2       [profile]
// BLE deleted          3       [profile]
// BLE disconnected     4       [profile]

// low battery(5%)      5       5
// battery SOC on boot  6       [SOC/10]

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define LED_GPIO_NODE_ID DT_COMPAT_GET_ANY_STATUS_OKAY(gpio_leds)

BUILD_ASSERT(DT_NODE_EXISTS(DT_ALIAS(indicator_led)),
             "An alias for the indicator LED is not found for LED_INDICATOR");

static const struct device *led_dev = DEVICE_DT_GET(LED_GPIO_NODE_ID);
static const uint8_t led_idx = DT_NODE_CHILD_IDX(DT_ALIAS(indicator_led));

static bool initialized = false;

struct blink_item {
    uint16_t duration_ms;
    uint16_t interval_ms;
    uint8_t count;
};

K_MSGQ_DEFINE(led_msgq, sizeof(struct blink_item), 16, 1);

#if IS_ENABLED(CONFIG_ZMK_BLE)
static void output_blink(void) {
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    uint8_t profile_index = zmk_ble_active_profile_index() + 1;
    if (zmk_ble_active_profile_is_connected()) {
        LOG_INF("Profile %d connected, blinking once", profile_index);
        struct blink_item bl = {.duration_ms = CONFIG_LED_INDICATOR_LONG_ON_MS, .count = 2};
        k_msgq_put(&led_msgq, &bl, K_NO_WAIT);
    } else if (zmk_ble_active_profile_is_open()) {
        LOG_INF("Profile %d open, slow blinking", profile_index);
        struct blink_item bl = {.duration_ms = CONFIG_LED_INDICATOR_LONG_ON_MS, .count = 3};
        k_msgq_put(&led_msgq, &bl, K_NO_WAIT);
    } else {
        LOG_INF("Profile %d not connected, fast blinking", profile_index);
        struct blink_item bl = {.duration_ms = CONFIG_LED_INDICATOR_LONG_ON_MS, .count = 4};
        k_msgq_put(&led_msgq, &bl, K_NO_WAIT);
    }
    struct blink_item bs = {.duration_ms = CONFIG_LED_INDICATOR_SHORT_ON_MS,
                            .count = profile_index};
    k_msgq_put(&led_msgq, &bs, K_NO_WAIT);
#else
    if (zmk_split_bt_peripheral_is_connected()) {
        LOG_INF("Peripheral connected, blinking once");
    } else {
        LOG_INF("Peripheral not connected, fast blinking");
    }
#endif
}

static int led_output_listener_cb(const zmk_event_t *eh) {
    if (initialized) {
        output_blink();
    }
    return 0;
}

ZMK_LISTENER(led_output_listener, led_output_listener_cb);
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
ZMK_SUBSCRIPTION(led_output_listener, zmk_ble_active_profile_changed);
#else
ZMK_SUBSCRIPTION(led_output_listener, zmk_split_peripheral_status_changed);
#endif
#endif // IS_ENABLED(CONFIG_ZMK_BLE)

#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
static int led_battery_listener_cb(const zmk_event_t *eh) {
    if (!initialized) {
        return 0;
    }

    uint8_t battery_level = as_zmk_battery_state_changed(eh)->state_of_charge;

    if (battery_level <= CONFIG_LED_INDICATOR_BATTERY_LEVEL_CRITICAL) {
        LOG_INF("Battery level %d, blinking for critical", battery_level);

        struct blink_item bl = {.duration_ms = CONFIG_LED_INDICATOR_LONG_ON_MS, .count = 5};
        k_msgq_put(&led_msgq, &bl, K_NO_WAIT);
        struct blink_item bs = {.duration_ms = CONFIG_LED_INDICATOR_SHORT_ON_MS, .count = 5};
        k_msgq_put(&led_msgq, &bs, K_NO_WAIT);
    }
    return 0;
}

ZMK_LISTENER(led_battery_listener, led_battery_listener_cb);
ZMK_SUBSCRIPTION(led_battery_listener, zmk_battery_state_changed);
#endif // IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
static int led_hid_indicator_listener_cb(const zmk_event_t *eh) {
    if (!initialized) {
        return 0;
    }

    zmk_hid_indicators_t indicators = as_zmk_hid_indicators_changed(eh)->indicators;

    if (indicators & HID_USAGE_LED_CAPS_LOCK) {
        LOG_INF("Caps ON");

        struct blink_item bl = {.duration_ms = CONFIG_LED_INDICATOR_LONG_ON_MS, .count = 1};
        k_msgq_put(&led_msgq, &bl, K_NO_WAIT);
        struct blink_item bs = {.duration_ms = CONFIG_LED_INDICATOR_SHORT_ON_MS, .count = 5};
        k_msgq_put(&led_msgq, &bs, K_NO_WAIT);
    } else {
        LOG_INF("Caps OFF");

        struct blink_item bl = {.duration_ms = CONFIG_LED_INDICATOR_LONG_ON_MS, .count = 1};
        k_msgq_put(&led_msgq, &bl, K_NO_WAIT);
        struct blink_item bs = {.duration_ms = CONFIG_LED_INDICATOR_SHORT_ON_MS, .count = 1};
        k_msgq_put(&led_msgq, &bs, K_NO_WAIT);
    }
    return 0;
}

ZMK_LISTENER(led_hid_indicator_listener, led_hid_indicator_listener_cb);
ZMK_SUBSCRIPTION(led_hid_indicator_listener, zmk_hid_indicators_changed);
#endif // IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)

extern void led_process_thread(void *d0, void *d1, void *d2) {
    ARG_UNUSED(d0);
    ARG_UNUSED(d1);
    ARG_UNUSED(d2);

    while (true) {
        struct blink_item blink;
        k_msgq_get(&led_msgq, &blink, K_FOREVER);
        LOG_DBG("Got a blink item from msgq, duration %d", blink.duration_ms);

        for (int i = 0; i < blink.count; i++) {
            led_on(led_dev, led_idx);

            k_sleep(K_MSEC(blink.duration_ms));

            led_off(led_dev, led_idx);

            if (blink.interval_ms > 0) {
                k_sleep(K_MSEC(blink.interval_ms));
            } else {
                k_sleep(K_MSEC(CONFIG_LED_INDICATOR_INTERVAL_MS));
            }
        }
    }
}

// define led_process_thread with stack size 1024, start running it 100 ms after boot
K_THREAD_DEFINE(led_process_tid, 1024, led_process_thread, NULL, NULL, NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO, 0, 100);

extern void led_init_thread(void *d0, void *d1, void *d2) {
    ARG_UNUSED(d0);
    ARG_UNUSED(d1);
    ARG_UNUSED(d2);

#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
    LOG_INF("Indicating initial battery status");

    // struct blink_item blink = {0};

    uint8_t battery_level = zmk_battery_state_of_charge();
    while (battery_level == 0) {
        k_sleep(K_MSEC(100));
        battery_level = zmk_battery_state_of_charge();
    };

    uint8_t count = round(battery_level / 10);

    LOG_INF("Battery level %d, blinking %d time(s)", battery_level, count);

    struct blink_item bl = {.duration_ms = CONFIG_LED_INDICATOR_LONG_ON_MS, .count = 6};
    k_msgq_put(&led_msgq, &bl, K_NO_WAIT);
    struct blink_item bs = {.duration_ms = CONFIG_LED_INDICATOR_SHORT_ON_MS, .count = count};
    k_msgq_put(&led_msgq, &bs, K_NO_WAIT);

    k_sleep(K_MSEC(
        (CONFIG_LED_INDICATOR_LONG_ON_MS + CONFIG_LED_INDICATOR_INTERVAL_MS) * (bl.count + 1) +
        (CONFIG_LED_INDICATOR_SHORT_ON_MS + CONFIG_LED_INDICATOR_INTERVAL_MS) * (bs.count + 1)));
#endif // IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)

#if IS_ENABLED(CONFIG_ZMK_BLE)
    LOG_INF("Indicating initial connectivity status");
    output_blink();
#endif // IS_ENABLED(CONFIG_ZMK_BLE)

    initialized = true;
    LOG_INF("Finished initializing LED widget");
}

K_THREAD_DEFINE(led_init_tid, 1024, led_init_thread, NULL, NULL, NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO, 0, 200);