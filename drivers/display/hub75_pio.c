/*
 *  * Copyright (c) 2024 DxPru. All Rights Reserved.
 *   *
 *    * SPDX-License-Identifier: Apache-2.0
 *     */

#include "zephyr/devicetree.h"
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/drivers/misc/pio_rpi_pico/pio_rpi_pico.h>
#include <zephyr/drivers/pinctrl.h>

#include <hardware/clocks.h>
#include <hardware/pio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display_hub75_pio, CONFIG_DISPLAY_LOG_LEVEL);

#define DT_DRV_COMPAT raspberrypi_hub75_pio

#define DATA_CNT_MAX 6
#define ADDR_CNT_MAX 5

struct hub75_pio_display_config {
  const struct device *piodev;
  const struct pinctrl_dev_config *pcfg;
  const uint16_t height;
  const uint16_t width;
  const struct gpio_dt_spec oe;
  const struct gpio_dt_spec latch;
  const struct gpio_dt_spec clk;
  //
  const struct gpio_dt_spec data[DATA_CNT_MAX];
  const uint8_t data_width;
  //
  const struct gpio_dt_spec addr[ADDR_CNT_MAX];
  const uint8_t addr_width;
};

struct hub75_pio_display_data {
  enum display_pixel_format pixel_format;
  size_t sm_data;
  size_t sm_ctrl;
};

//
// OUT: DATA
// SET: LAT
// SIDESET: CLK
//
RPI_PICO_PIO_DEFINE_PROGRAM(hub75_data, 0, 12,
                            //     .wrap_target
                            0xe02f, //  0: set    x, 15           side 0
                            0xc020, //  1: irq    wait 0          side 0
                            0x80a0, //  2: pull   block           side 0
                            0x6006, //  3: out    pins, 6         side 0
                            0x7162, //  4: out    null, 2         side 1 [1]
                            0x6006, //  5: out    pins, 6         side 0
                            0x7162, //  6: out    null, 2         side 1 [1]
                            0x6006, //  7: out    pins, 6         side 0
                            0x7162, //  8: out    null, 2         side 1 [1]
                            0x6006, //  9: out    pins, 6         side 0
                            0x1142, // 10: jmp    x--, 2          side 1 [1]
                            0xe101, // 11: set    pins, 1         side 0 [1]
                            0xc041, // 12: irq    clear 1         side 0
                                    //     .wrap
);

//
// OUT: ADDR
// SET: OE, LATCH
//
RPI_PICO_PIO_DEFINE_PROGRAM(hub75_ctrl, 0, 16,
                            //     .wrap_target
                            0x8000, //  0: push   noblock
                            0xe001, //  1: set    pins, 1
                            0xc040, //  2: irq    clear 0
                            0xc021, //  3: irq    wait 1
                            0xe001, //  4: set    pins, 1
                            0x80a0, //  5: pull   block
                            0x6005, //  6: out    pins, 5
                            0x40ed, //  7: in     osr, 13
                            0x606d, //  8: out    null, 13
                            0xa046, //  9: mov    y, isr
                            0x8000, // 10: push   noblock
                            0x40ed, // 11: in     osr, 13
                            0xa026, // 12: mov    x, isr
                            0xe000, // 13: set    pins, 0
                            0x01b0, // 14: jmp    x != y, 16             [1]
                            0xe001, // 15: set    pins, 1
                            0x008e, // 16: jmp    y--, 14
                                    //     .wrap
);

/*
 * @brief Initalize the data PIO
 * @param pio PIO to Initalize
 * @param sm StateMaschine on which the data program should run
 * @param display_config the Configuration of the Display
 * @param div clock_divider
 * @return 0 on Success & negativ on Error
 */
int h75p_init_pio_data(PIO pio, uint32_t sm,
                       const struct hub75_pio_display_config *display_config,
                       float div) {
  uint32_t offset;
  pio_sm_config sm_config;

  if (!pio_can_add_program(pio, RPI_PICO_PIO_GET_PROGRAM(hub75_data))) {
    return -EBUSY;
  }

  offset = pio_add_program(pio, RPI_PICO_PIO_GET_PROGRAM(hub75_data));
  sm_config = pio_get_default_sm_config();

  sm_config_set_wrap(&sm_config,
                     offset + RPI_PICO_PIO_GET_WRAP_TARGET(hub75_data),
                     offset + RPI_PICO_PIO_GET_WRAP(hub75_data));

  // OUT Pins
  for (int i = 0; i < display_config->data_width; i++) {
    pio_gpio_init(pio, display_config->data[i].pin);
  }
  pio_sm_set_consecutive_pindirs(pio, sm, display_config->data[0].pin,
                                 display_config->data_width, true);
  sm_config_set_out_pins(&sm_config, display_config->data[0].pin,
                         display_config->data_width);

  // SET Pin
  pio_gpio_init(pio, display_config->latch.pin);
  pio_sm_set_consecutive_pindirs(pio, sm, display_config->latch.pin, 1, true);
  sm_config_set_set_pins(&sm_config, display_config->latch.pin, 1);

  // SIDESET Pin
  pio_gpio_init(pio, display_config->clk.pin);
  pio_sm_set_consecutive_pindirs(pio, sm, display_config->clk.pin, 1, true);
  sm_config_set_sideset_pins(&sm_config, display_config->clk.pin);
  sm_config_set_sideset(&sm_config, 1, false, false);

  sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_TX);

  sm_config_set_clkdiv(&sm_config, div);

  sm_config_set_out_shift(&sm_config, true, false, 32);
  sm_config_set_in_shift(&sm_config, false, false, 32);

  pio_sm_init(pio, sm, offset, &sm_config);
  pio_sm_exec(pio, sm, offset);
  pio_sm_set_enabled(pio, sm, true);

  return 0;
}

/*
 * @brief Initalize the ctrl PIO
 * @param pio PIO to Initalize
 * @param sm StateMaschine on which the ctrl program should run
 * @param display_config the Configuration of the Display
 * @param div clock_divider
 * @return 0 on Success & negativ on Error
 */
int h75p_init_pio_ctrl(PIO pio, uint32_t sm,
                       const struct hub75_pio_display_config *display_config,
                       float div) {
  uint32_t offset;
  pio_sm_config sm_config;

  if (!pio_can_add_program(pio, RPI_PICO_PIO_GET_PROGRAM(hub75_ctrl))) {
    return -EBUSY;
  }

  offset = pio_add_program(pio, RPI_PICO_PIO_GET_PROGRAM(hub75_ctrl));
  sm_config = pio_get_default_sm_config();

  sm_config_set_wrap(&sm_config,
                     offset + RPI_PICO_PIO_GET_WRAP_TARGET(hub75_ctrl),
                     offset + RPI_PICO_PIO_GET_WRAP(hub75_ctrl));

  // OUT Pins
  pio_sm_set_consecutive_pindirs(pio, sm, display_config->addr[0].pin,
                                 display_config->addr_width, true);
  for (int i = 0; i < display_config->addr_width; i++) {
    pio_gpio_init(pio, display_config->addr[i].pin);
  }
  sm_config_set_out_pins(&sm_config, display_config->addr[0].pin,
                         display_config->addr_width);

  // SET Pins
  __ASSERT(display_config->latch.pin == display_config->oe.pin + 1,
           "Latch Pin doesn't come after OE");
  pio_sm_set_consecutive_pindirs(pio, sm, display_config->oe.pin, 2, true);
  pio_gpio_init(pio, display_config->oe.pin);
  pio_gpio_init(pio, display_config->latch.pin);
  sm_config_set_set_pins(&sm_config, display_config->oe.pin, 2);

  sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_TX);

  sm_config_set_clkdiv(&sm_config, div);

  sm_config_set_out_shift(&sm_config, true, false, 32);
  sm_config_set_in_shift(&sm_config, false, false, 32);

  pio_sm_init(pio, sm, offset, &sm_config);
  pio_sm_exec(pio, sm, offset);
  pio_sm_set_enabled(pio, sm, true);

  return 0;
}

static int h75p_init(const struct device *dev) {
  const struct hub75_pio_display_config *config = dev->config;
  struct hub75_pio_display_data *data = dev->data;

  int retval;

  data->pixel_format = PIXEL_FORMAT_ARGB_8888;

  // Dispatch PIO's
  PIO pio = pio_rpi_pico_get_pio(config->piodev);
  float div = 2;

  // Allocat the StateMaschines
  size_t sm_data;
  retval = pio_rpi_pico_allocate_sm(config->piodev, &sm_data);
  if (retval < 0) {
    return retval;
  }

  size_t sm_ctrl;
  retval = pio_rpi_pico_allocate_sm(config->piodev, &sm_ctrl);
  if (retval < 0) {
    return retval;
  }

  data->sm_data = sm_data;
  data->sm_ctrl = sm_ctrl;

  // Initalize the StateMaschines
  retval = h75p_init_pio_ctrl(pio, sm_ctrl, config, div);
  if (retval < 0) {
    return retval;
  }

  retval = h75p_init_pio_data(pio, sm_data, config, div);
  if (retval < 0) {
    return retval;
  }

  return pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
}

static int h75p_write(const struct device *dev, const uint16_t x,
                      const uint16_t y,
                      const struct display_buffer_descriptor *desc,
                      const void *buf) {
  const struct hub75_pio_display_config *config = dev->config;

  __ASSERT(desc->width <= desc->pitch, "Pitch is smaller then width");
  __ASSERT(desc->pitch <= config->width,
           "Pitch in descriptor is larger than screen size");
  __ASSERT(desc->height <= config->height,
           "Height in descriptor is larger than screen size");
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

static int h75p_blanking_off(const struct device *dev) { return 0; }

static int h75p_blanking_on(const struct device *dev) { return 0; }

static int h75p_set_brightness(const struct device *dev,
                               const uint8_t brightness) {
  return 0;
}

static int h75p_set_contrast(const struct device *dev, const uint8_t contrast) {
  return 0;
}

static void h75p_get_capabilities(const struct device *dev,
                                  struct display_capabilities *capabilities) {
  const struct hub75_pio_display_config *config = dev->config;
  struct hub75_pio_display_data *disp_data = dev->data;

  memset(capabilities, 0, sizeof(struct display_capabilities));
  capabilities->x_resolution = config->width;
  capabilities->y_resolution = config->height;
  capabilities->supported_pixel_formats =
      PIXEL_FORMAT_ARGB_8888 | PIXEL_FORMAT_RGB_888 | PIXEL_FORMAT_MONO01 |
      PIXEL_FORMAT_MONO10;
  capabilities->current_pixel_format = disp_data->pixel_format;
  capabilities->screen_info =
      SCREEN_INFO_MONO_VTILED | SCREEN_INFO_MONO_MSB_FIRST;
}

static int h75p_set_pixel_format(const struct device *dev,
                                 const enum display_pixel_format pixel_format) {
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

#define HUB75_PIO_DEFINE(n)                                                    \
  PINCTRL_DT_INST_DEFINE(n);                                                   \
  static const struct hub75_pio_display_config h75p_config_##n = {             \
      .piodev = DEVICE_DT_GET(DT_INST_PARENT(n)),                              \
      .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                               \
      .height = DT_INST_PROP(n, height),                                       \
      .width = DT_INST_PROP(n, width),                                         \
      .oe = GPIO_DT_SPEC_GET_OR(n, oe_gpios, {0}),                             \
      .latch = GPIO_DT_SPEC_GET_OR(n, latch_gpios, {0}),                       \
      .clk = GPIO_DT_SPEC_GET_OR(n, clk_gpios, {0}),                           \
      .data = {GPIO_DT_SPEC_GET_BY_IDX_OR(n, data_gpios, 0, {0}),              \
               GPIO_DT_SPEC_GET_BY_IDX_OR(n, data_gpios, 1, {0}),              \
               GPIO_DT_SPEC_GET_BY_IDX_OR(n, data_gpios, 2, {0}),              \
               GPIO_DT_SPEC_GET_BY_IDX_OR(n, data_gpios, 3, {0}),              \
               GPIO_DT_SPEC_GET_BY_IDX_OR(n, data_gpios, 4, {0}),              \
               GPIO_DT_SPEC_GET_BY_IDX_OR(n, data_gpios, 5, {0})},             \
      .data_width = DT_INST_PROP_LEN(n, data_gpios),                           \
      .addr = {GPIO_DT_SPEC_INST_GET_BY_IDX_OR(n, addr_gpios, 0, {0}),         \
               GPIO_DT_SPEC_INST_GET_BY_IDX_OR(n, addr_gpios, 1, {0}),         \
               GPIO_DT_SPEC_INST_GET_BY_IDX_OR(n, addr_gpios, 2, {0}),         \
               GPIO_DT_SPEC_INST_GET_BY_IDX_OR(n, addr_gpios, 3, {0}),         \
               GPIO_DT_SPEC_INST_GET_BY_IDX_OR(n, addr_gpios, 4, {0})},        \
      .addr_width = DT_INST_PROP_LEN(n, addr_gpios),                           \
  };                                                                           \
                                                                               \
  static struct hub75_pio_display_data h75p_data_##n;                          \
                                                                               \
  DEVICE_DT_INST_DEFINE(n, &h75p_init, NULL, &h75p_data_##n, &h75p_config_##n, \
                        POST_KERNEL, CONFIG_DISPLAY_INIT_PRIORITY, &h75p_api);

DT_INST_FOREACH_STATUS_OKAY(HUB75_PIO_DEFINE)
