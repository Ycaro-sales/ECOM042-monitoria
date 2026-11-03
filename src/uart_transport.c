/*******************************************************************
 * @file uart_transport.c
 *
 * @brief Transporte da mini-shell por UART.
 *
 * ISR: só copia os bytes recebidos para uma fila.
 * Thread: consome a fila, ecoa, monta linhas e despacha comandos.
 * TX: uart_poll_out.
 *******************************************************************/

#include "uart_transport.h"

#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#include "line_editor.h"

#define RX_QUEUE_LEN      128
#define RESPONSE_SIZE     256
#define SHELL_STACK_SIZE  2048
#define SHELL_THREAD_PRIO 5

K_MSGQ_DEFINE(rx_queue, sizeof(uint8_t), RX_QUEUE_LEN, 1);
K_THREAD_STACK_DEFINE(shell_stack, SHELL_STACK_SIZE);

static struct k_thread shell_thread;
static const struct device *shell_uart;
static const struct command_hal *shell_hal;

static void uart_isr(const struct device *dev, void *user_data)
{
	uint8_t buf[16];
	int n;

	ARG_UNUSED(user_data);

	uart_irq_update(dev);

	while (uart_irq_rx_ready(dev) > 0) {
		n = uart_fifo_read(dev, buf, sizeof(buf));
		if (n <= 0) {
			break;
		}
		for (int i = 0; i < n; i++) {
			/* Fila cheia: o byte é perdido (não bloqueia na ISR). */
			(void)k_msgq_put(&rx_queue, &buf[i], K_NO_WAIT);
		}
	}
}

static void send_str(const char *s)
{
	while (*s != '\0') {
		uart_poll_out(shell_uart, *s++);
	}
}

static void shell_thread_fn(void *p1, void *p2, void *p3)
{
	static char line[CONFIG_APP_CMD_LINE_SIZE];
	static char response[RESPONSE_SIZE];
	struct line_editor le;
	uint8_t c;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	line_editor_init(&le, line, sizeof(line));
	send_str("> ");

	while (true) {
		k_msgq_get(&rx_queue, &c, K_FOREVER);

		switch (line_editor_feed(&le, (char)c)) {
		case LINE_CHAR:
			uart_poll_out(shell_uart, c);
			break;
		case LINE_ERASED:
			send_str("\b \b");
			break;
		case LINE_DONE:
			send_str("\r\n");
			dispatch(shell_hal, line, response, sizeof(response));
			send_str(response);
			send_str("> ");
			break;
		case LINE_TOO_LONG:
			send_str("\r\nERR: line too long\r\n> ");
			break;
		case LINE_IGNORED:
		default:
			break;
		}
	}
}

int uart_transport_start(const struct device *uart, const struct command_hal *hal)
{
	int ret;

	shell_uart = uart;
	shell_hal = hal;

	ret = uart_irq_callback_user_data_set(uart, uart_isr, NULL);
	if (ret != 0) {
		return ret;
	}

	k_thread_create(&shell_thread, shell_stack, K_THREAD_STACK_SIZEOF(shell_stack),
			shell_thread_fn, NULL, NULL, NULL, SHELL_THREAD_PRIO, 0, K_NO_WAIT);
	k_thread_name_set(&shell_thread, "shell");

	uart_irq_rx_enable(uart);
	return 0;
}
