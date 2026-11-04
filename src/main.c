/*******************************************************************
 * @file main.c
 *
 * @brief Main file.
 * @author João Matheus Nascimento Dias (jmnd@ic.ufal.br)
 * @version 0.1
 * @date 19/10/2026
 *******************************************************************/
#include "led_ctrl.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* Padrão de cada LED na demonstração (LED i usa o padrão i % LED_PATTERN_COUNT). */
static const enum led_pattern demo_pattern[LED_PATTERN_COUNT] = {
	LED_PATTERN_SLOW,
	LED_PATTERN_FAST,
	LED_PATTERN_NORMAL,
};

int main(void)
{
	int ret;

	LOG_INF("Atividade 05: LEDs independentes com um k_timer (%s)", CONFIG_BOARD);

	ret = led_ctrl_init();
	if (ret != 0) {
		LOG_ERR("led_ctrl_init falhou: %d", ret);
		return ret;
	}

	for (size_t i = 0; i < led_ctrl_count(); i++) {
		enum led_pattern pattern = demo_pattern[i % LED_PATTERN_COUNT];

		ret = led_ctrl_blink((int)i, pattern);
		if (ret != 0) {
			LOG_ERR("led_ctrl_blink(%zu) falhou: %d", i, ret);
			return ret;
		}
		LOG_INF("LED %zu piscando no padrao %d", i, (int)pattern);
	}

	/* O pisca segue pelo timer; main pode terminar. */
	return 0;
}
