/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <zephyr/drivers/sensor.h>

#include "battery_common.h"

int battery_channel_get(const struct battery_value *value, enum sensor_channel chan,
                        struct sensor_value *val_out) {
    switch (chan) {
    case SENSOR_CHAN_GAUGE_VOLTAGE:
        val_out->val1 = value->millivolts / 1000;
        val_out->val2 = (value->millivolts % 1000) * 1000U;
        break;

    case SENSOR_CHAN_GAUGE_STATE_OF_CHARGE:
        val_out->val1 = value->state_of_charge;
        val_out->val2 = 0;
        break;

    default:
        return -ENOTSUP;
    }

    return 0;
}

uint8_t lithium_ion_mv_to_pct(uint16_t bat_mv) {
    // Simple linear approximation of a battery based off adafruit's discharge graph:
    // https://learn.adafruit.com/li-ion-and-lipoly-batteries/voltages

    if (bat_mv >= 4200) {
        return 100;
    } else if (bat_mv <= 3450) {
        return 0;
    }

    return bat_mv * 2 / 15 - 459;
}

uint8_t nimh_x2_mv_to_pct(uint16_t bat_mv) {
    // For 2 series NiMH batteries
    // Can also be used with the following battery types with a cut-off voltage of 2V
    // - 2x Alkaline 1.5V(LR)
    // - 2x Lithium 1.5V(FR)
    // - Lithium 3V(CR,BR)
    // - LiFePO4

    if (bat_mv >= 2800) {
        return 100;
    } else if (bat_mv <= 2050) {
        return 0;
    }

    return bat_mv * 17 / 128 - 272;
}