/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_selector

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <zmk/keymap.h>
#include <zmk/matrix.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/hid.h>
#include <dt-bindings/zmk/selector.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_selector_data {
    unsigned int index;
};

struct behavior_selector_config {
    int tap_ms;
    bool loop;
    bool separate_pushed;
    size_t behavior_count;
    struct zmk_behavior_binding *behaviors;
};

static int on_selector_binding_pressed(struct zmk_behavior_binding *binding,
                                       struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    struct behavior_selector_data *data = dev->data;
    const struct behavior_selector_config *cfg = dev->config;

    if (cfg->behavior_count <= data->index) {
        data->index = 0;
    } else {
        if (cfg->separate_pushed) {
            if (binding->param1 == SEL_PREV) {
                if (data->index > 1) {
                    data->index -= 2;
                } else if (cfg->loop) {
                    data->index = cfg->behavior_count - 2;
                }
            } else if (binding->param1 == SEL_NEXT) {
                if (data->index < cfg->behavior_count - 2) {
                    data->index += 2;
                } else if (cfg->loop) {
                    data->index = 0;
                }
            } else if (binding->param1 == SEL_PUSH) {
                zmk_behavior_queue_add(&event, cfg->behaviors[data->index + 1], true, cfg->tap_ms);
                zmk_behavior_queue_add(&event, cfg->behaviors[data->index + 1], false, 0);
                return ZMK_BEHAVIOR_OPAQUE;
            } else if (0 <= binding->param1 && binding->param1 < cfg->behavior_count) {
                data->index = binding->param1 * 2;
            }
        } else {
            if (binding->param1 == SEL_PREV) {
                if (data->index > 0) {
                    data->index--;
                } else if (cfg->loop) {
                    data->index = cfg->behavior_count - 1;
                }
            } else if (binding->param1 == SEL_NEXT) {
                if (data->index < cfg->behavior_count - 1) {
                    data->index++;
                } else if (cfg->loop) {
                    data->index = 0;
                }
            } else if (0 <= binding->param1 && binding->param1 < cfg->behavior_count) {
                data->index = binding->param1;
            }
        }
    }

    zmk_behavior_queue_add(&event, cfg->behaviors[data->index], true, cfg->tap_ms);
    zmk_behavior_queue_add(&event, cfg->behaviors[data->index], false, 0);

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_selector_binding_released(struct zmk_behavior_binding *binding,
                                        struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_selector_driver_api = {
    .binding_pressed = on_selector_binding_pressed,
    .binding_released = on_selector_binding_released,
};

static int behavior_selector_init(const struct device *dev) { return 0; }

#define _TRANSFORM_ENTRY(idx, node) ZMK_KEYMAP_EXTRACT_BINDING(idx, node)

#define TRANSFORMED_BINDINGS(node)                                                                 \
    {LISTIFY(DT_INST_PROP_LEN(node, bindings), _TRANSFORM_ENTRY, (, ), DT_DRV_INST(node))}

#define SEL_INST(n)                                                                                \
    static struct zmk_behavior_binding                                                             \
        behavior_selector_config_##n##_bindings[DT_INST_PROP_LEN(n, bindings)] =                   \
            TRANSFORMED_BINDINGS(n);                                                               \
    static struct behavior_selector_config behavior_selector_config_##n = {                        \
        .tap_ms = DT_INST_PROP_OR(n, tap_ms, 5),                                                   \
        .loop = DT_INST_PROP_OR(n, loop, false),                                                   \
        .separate_pushed = DT_INST_PROP_OR(n, push, false),                                                   \
        .behaviors = behavior_selector_config_##n##_bindings,                                      \
        .behavior_count = DT_INST_PROP_LEN(n, bindings)};                                          \
    static struct behavior_selector_data behavior_selector_data_##n = {                            \
        .index = DT_INST_PROP_OR(n, selection_default, 0)};                                        \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_selector_init, NULL, &behavior_selector_data_##n,          \
                            &behavior_selector_config_##n, POST_KERNEL,                            \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_selector_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SEL_INST)

#endif