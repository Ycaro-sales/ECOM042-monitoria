/*******************************************************************
 * @file main.c
 *
 * @brief Mini-shell via UART1: inicializa e conecta os módulos.
 * @author João Matheus Nascimento Dias (jmnd@ic.ufal.br)
 * @version 0.1
 * @date 19/10/2026
 *******************************************************************/

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include "board_io.h"
#include "uart_transport.h"

static const struct device *const cmd_uart = DEVICE_DT_GET(DT_NODELABEL(uart1));

int main(void)
{
	int ret;

	ret = board_io_init();
	if (ret != 0) {
		printk("board_io_init failed: %d\n", ret);
		return ret;
	}

	if (!device_is_ready(cmd_uart)) {
		printk("uart1 not ready\n");
		return -ENODEV;
	}

	ret = uart_transport_start(cmd_uart, &board_hal);
	if (ret != 0) {
		printk("uart_transport_start failed: %d\n", ret);
		return ret;
	}

	printk("Mini-shell pronta em %s\n", cmd_uart->name);
	return 0;
}
