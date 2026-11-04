/*******************************************************************
 * @file led_ctrl.c
 *
 * @brief LEDs independentes multiplexados em um único k_timer.
 *
 * Cada LED guarda o seu próximo instante de troca (deadline absoluto,
 * em ticks). O timer é one-shot e sempre rearmado para o menor deadline;
 * como os deadlines são absolutos, rearmar por causa de um LED não muda
 * a fase nem o período dos outros. LED parado tem deadline LED_NEVER,
 * então callback e reagendamento só comparam deadlines.
 *******************************************************************/

#include "led_ctrl.h"

#include <errno.h>
#include <stdint.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(led_ctrl, LOG_LEVEL_INF);

#define LED_NEVER INT64_MAX

/* Uma entrada por alias ledN existente no devicetree. */
#define LED_SPEC(i, _)                                                                             \
	IF_ENABLED(DT_HAS_ALIAS(led##i), (GPIO_DT_SPEC_GET(DT_ALIAS(led##i), gpios), ))

static const struct gpio_dt_spec led_specs[] = {LISTIFY(CONFIG_APP_LED_MAX_ALIASES, LED_SPEC, ())};

#define LED_COUNT ARRAY_SIZE(led_specs)

BUILD_ASSERT(LED_COUNT > 0, "defina ao menos o alias led0 no devicetree");

/* Período (ciclo completo, ms) de cada padrão, indexado por enum led_pattern. */
static const uint32_t pattern_period_ms[LED_PATTERN_COUNT] = {
	[LED_PATTERN_SLOW] = CONFIG_APP_LED_PERIOD_SLOW_MS,
	[LED_PATTERN_NORMAL] = CONFIG_APP_LED_PERIOD_NORMAL_MS,
	[LED_PATTERN_FAST] = CONFIG_APP_LED_PERIOD_FAST_MS,
};

struct led_state {
	enum led_pattern pattern;
	k_ticks_t half_period; /* intervalo entre trocas de nível */
	k_ticks_t deadline;    /* próxima troca (uptime em ticks) ou LED_NEVER */
	bool blinking;
};

static struct led_state leds[LED_COUNT];
static struct k_timer led_timer;
static bool timer_initialized;
static struct k_spinlock lock;

/* Ação aplicada a um LED com o lock tomado. */
typedef int (*led_action_fn)(size_t idx, enum led_pattern pattern);

/* Arma o timer para o deadline mais próximo. Chamar com o lock tomado. */
static void reschedule(void)
{
	k_ticks_t next = LED_NEVER;

	for (size_t i = 0; i < LED_COUNT; i++) {
		next = MIN(next, leds[i].deadline);
	}

	if (next == LED_NEVER) {
		k_timer_stop(&led_timer);
		return;
	}
	k_timer_start(&led_timer, K_TIMEOUT_ABS_TICKS(next), K_NO_WAIT);
}

/* Contexto de ISR: só GPIO, deadlines e rearme. */
static void led_timer_expiry(struct k_timer *timer)
{
	k_spinlock_key_t key = k_spin_lock(&lock);
	k_ticks_t now = k_uptime_ticks();

	ARG_UNUSED(timer);

	for (size_t i = 0; i < LED_COUNT; i++) {
		if (leds[i].deadline <= now) {
			(void)gpio_pin_toggle_dt(&led_specs[i]);
			leds[i].deadline += leds[i].half_period;
		}
	}
	reschedule();

	k_spin_unlock(&lock, key);
}

static void stop_blink(size_t idx)
{
	leds[idx].blinking = false;
	leds[idx].deadline = LED_NEVER;
}

static int action_on(size_t idx, enum led_pattern pattern)
{
	ARG_UNUSED(pattern);
	stop_blink(idx);
	return gpio_pin_set_dt(&led_specs[idx], 1);
}

static int action_off(size_t idx, enum led_pattern pattern)
{
	ARG_UNUSED(pattern);
	stop_blink(idx);
	return gpio_pin_set_dt(&led_specs[idx], 0);
}

static int action_toggle(size_t idx, enum led_pattern pattern)
{
	ARG_UNUSED(pattern);
	stop_blink(idx);
	return gpio_pin_toggle_dt(&led_specs[idx]);
}

static int action_blink(size_t idx, enum led_pattern pattern)
{
	struct led_state *led = &leds[idx];
	int ret = gpio_pin_set_dt(&led_specs[idx], 1);

	if (ret != 0) {
		stop_blink(idx);
		return ret;
	}

	led->pattern = pattern;
	led->half_period = k_ms_to_ticks_ceil64(pattern_period_ms[pattern] / 2U);
	led->deadline = k_uptime_ticks() + led->half_period;
	led->blinking = true;
	return 0;
}

/* Único lugar que trata "um LED ou todos": resolve o intervalo e aplica a ação. */
static int led_apply(int led, led_action_fn action, enum led_pattern pattern)
{
	size_t first = 0;
	size_t last = LED_COUNT;
	k_spinlock_key_t key;
	int ret = 0;

	if (led != LED_CTRL_ALL) {
		if (led < 0 || (size_t)led >= LED_COUNT) {
			LOG_ERR("LED invalido: %d (existem %zu)", led, LED_COUNT);
			return -EINVAL;
		}
		first = (size_t)led;
		last = first + 1;
	}

	if (!timer_initialized) {
		LOG_ERR("led_ctrl_init() ainda nao foi chamada");
		return -EPERM;
	}

	key = k_spin_lock(&lock);
	for (size_t i = first; i < last && ret == 0; i++) {
		ret = action(i, pattern);
	}
	reschedule();
	k_spin_unlock(&lock, key);

	return ret;
}

int led_ctrl_init(void)
{
	k_spinlock_key_t key;

	for (size_t i = 0; i < LED_COUNT; i++) {
		if (!gpio_is_ready_dt(&led_specs[i])) {
			LOG_ERR("controlador do LED %zu (%s) nao esta pronto", i,
				led_specs[i].port->name);
			return -ENODEV;
		}
	}

	/* k_timer_init num timer ativo corromperia a fila de timeouts: só na 1a vez. */
	if (!timer_initialized) {
		k_timer_init(&led_timer, led_timer_expiry, NULL);
		timer_initialized = true;
	}

	key = k_spin_lock(&lock);
	k_timer_stop(&led_timer);
	for (size_t i = 0; i < LED_COUNT; i++) {
		stop_blink(i);
	}
	k_spin_unlock(&lock, key);

	for (size_t i = 0; i < LED_COUNT; i++) {
		int ret = gpio_pin_configure_dt(&led_specs[i], GPIO_OUTPUT_INACTIVE);

		if (ret != 0) {
			LOG_ERR("falha ao configurar LED %zu: %d", i, ret);
			return ret;
		}
	}

	return 0;
}

size_t led_ctrl_count(void)
{
	return LED_COUNT;
}

int led_ctrl_on(int led)
{
	return led_apply(led, action_on, LED_PATTERN_SLOW);
}

int led_ctrl_off(int led)
{
	return led_apply(led, action_off, LED_PATTERN_SLOW);
}

int led_ctrl_toggle(int led)
{
	return led_apply(led, action_toggle, LED_PATTERN_SLOW);
}

int led_ctrl_blink(int led, enum led_pattern pattern)
{
	if ((unsigned int)pattern >= LED_PATTERN_COUNT) {
		LOG_ERR("padrao invalido: %d", (int)pattern);
		return -EINVAL;
	}
	return led_apply(led, action_blink, pattern);
}
