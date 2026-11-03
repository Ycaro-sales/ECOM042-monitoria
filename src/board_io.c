/*******************************************************************
 * @file board_io.c
 *
 * @brief LED e botão obtidos pelo devicetree (aliases led0 e sw0).
 *******************************************************************/

#include "board_io.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

/* Ler de volta um pino de saída não é portátil: guarda o estado aqui. */
static bool led_state;

int board_io_init(void)
{
	int ret;

	if (!gpio_is_ready_dt(&led) || !gpio_is_ready_dt(&button)) {
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
	if (ret != 0) {
		return ret;
	}

	return gpio_pin_configure_dt(&button, GPIO_INPUT);
}

static int led_set(bool on)
{
	int ret = gpio_pin_set_dt(&led, on ? 1 : 0);

	if (ret == 0) {
		led_state = on;
	}
	return ret;
}

static bool led_get(void)
{
	return led_state;
}

static int button_get(void)
{
	return gpio_pin_get_dt(&button);
}

static int64_t uptime_ms(void)
{
	return k_uptime_get();
}

const struct command_hal board_hal = {
	.led_set = led_set,
	.led_get = led_get,
	.button_get = button_get,
	.uptime_ms = uptime_ms,
};
