/*
 *  * Copyright (c) 2024 DxPru. All Rights Reserved.
 *   *
 *    * SPDX-License-Identifier: Apache-2.0
 *     */

#define DT_DRV_COMPAT raspberrypi_hub75_pio

#include <errno.h>
#include <string.h>

#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_hub75_pio, CONFIG_DISPLAY_LOG_LEVEL);

struct hub75_pio_display_config {
        uint16_t height;
            uint16_t width;
                struct gpio_dt_spec ctrl;
                    struct gpio_dt_spec data;
                        struct gpio_dt_spec addr;
};

struct hub75_pio_display_data {
        enum display_pixel_format pixel_format;
};

static int h75p_init(const struct device *dev)
{
        struct hub75_pio_display_data *disp_data = dev->data;

            disp_data->pixel_format = PIXEL_FORMAT_ARGB_8888;

                return 0;
}

static int h75p_write(const struct device *dev, const uint16_t x, const uint16_t y,
                      const struct display_buffer_descriptor *desc, const void *buf)
{
        const struct hub75_pio_display_config *config = dev->config;

            __ASSERT(desc->width <= desc->pitch, "Pitch is smaller then width");
                __ASSERT(desc->pitch <= config->width, "Pitch in descriptor is larger than screen size");
                    __ASSERT(desc->height <= config->height, "Height in descriptor is larger than screen size");
                        __ASSERT(x + desc->pitch <= config->width,
                                         "Writing outside screen boundaries in horizontal direction");
                            __ASSERT(y + desc->height <= config->height,
                                             "Writing outside screen boundaries in vertical direction");

                                if (desc->width > desc->pitch || x + desc->pitch > config->width ||
                                                y + desc->height > config->height) {
                                            return -EINVAL;
                                                }

                                    return 0;
}

static int h75p_blanking_off(const struct device *dev)
{
        return 0;
}

static int h75p_blanking_on(const struct device *dev)
{
        return 0;
}

static int h75p_set_brightness(const struct device *dev, const uint8_t brightness)
{
        return 0;
}

static int h75p_set_contrast(const struct device *dev, const uint8_t contrast)
{
        return 0;
}

static void h75p_get_capabilities(const struct device *dev,
                          struct display_capabilities *capabilities)
{
        const struct hub75_pio_display_config *config = dev->config;
            struct hub75_pio_display_data *disp_data = dev->data;

                memset(capabilities, 0, sizeof(struct display_capabilities));
                    capabilities->x_resolution = config->width;
                        capabilities->y_resolution = config->height;
                            capabilities->supported_pixel_formats = PIXEL_FORMAT_ARGB_8888 | PIXEL_FORMAT_RGB_888 |
                                                        PIXEL_FORMAT_MONO01 | PIXEL_FORMAT_MONO10;
                                capabilities->current_pixel_format = disp_data->pixel_format;
                                    capabilities->screen_info = SCREEN_INFO_MONO_VTILED | SCREEN_INFO_MONO_MSB_FIRST;
}

static int h75p_set_pixel_format(const struct device *dev,
                         const enum display_pixel_format pixel_format)
{
        struct hub75_pio_display_data *disp_data = dev->data;

            disp_data->pixel_format = pixel_format;
                return 0;
}

static const struct display_driver_api h75p_api = {
        .blanking_on = h75p_blanking_on,
            .blanking_off = h75p_blanking_off,
                .write = h75p_write,
                    .set_brightness = h75p_set_brightness,
                        .set_contrast = h75p_set_contrast,
                            .get_capabilities = h75p_get_capabilities,
                                .set_pixel_format = h75p_set_pixel_format,
};

#define HUB75_PIO_DEFINE(n)                                                                        \
        static const struct hub75_pio_display_config h75p_config_##n = {                           \
                    .height = DT_INST_PROP(n, height),                                                 \
                    .width = DT_INST_PROP(n, width),                                                   \
                    .ctrl = GPIO_DT_SPEC_GET_OR(n, ctrl_gpios, {0}),                                   \
                    .data = GPIO_DT_SPEC_GET_OR(n, data_gpios, {0}),                                   \
                    .addr = GPIO_DT_SPEC_GET_OR(n, addr_gpios, {0}),                                   \
                };                                                                                         \
                                                                                                                   \
                                                                                                                    static struct hub75_pio_display_data h75p_data_##n;                                        \
                                                                                                                                                                                                                       \
                                                                                                                                                                                                                        DEVICE_DT_INST_DEFINE(n, &h75p_init, NULL, &h75p_data_##n, &h75p_config_##n, POST_KERNEL,  \
                                                                                                                                                                                                                                                  CONFIG_DISPLAY_INIT_PRIORITY, &h75p_api);

DT_INST_FOREACH_STATUS_OKAY(HUB75_PIO_DEFINE)
