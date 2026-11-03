/*******************************************************************
 * @file button_sim.c
 *
 * @brief Simula o botão sw0 no native_sim: cada espaço digitado no
 *        terminal (via UART0 em stdin) gera um aperto no pino emulado.
 *******************************************************************/

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#define PRESS_TIME_MS 50
#define POLL_TIME_MS  10

static const struct device *const uart = DEVICE_DT_GET(DT_NODELABEL(uart0));
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);

static void button_sim_thread(void)
{
	unsigned char c;

	while (true) {
		if (uart_poll_in(uart, &c) == 0 && c == ' ') {
			/* Nível físico: ativo = 1 (GPIO_ACTIVE_HIGH no overlay). */
			gpio_emul_input_set(button.port, button.pin, 1);
			k_msleep(PRESS_TIME_MS);
			gpio_emul_input_set(button.port, button.pin, 0);
		}
		k_msleep(POLL_TIME_MS);
	}
}

K_THREAD_DEFINE(button_sim, 1024, button_sim_thread, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);
