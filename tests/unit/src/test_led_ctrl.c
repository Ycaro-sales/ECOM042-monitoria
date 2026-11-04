/*******************************************************************
 * @file test_led_ctrl.c
 *
 * @brief Testes do módulo led_ctrl no native_sim.
 *
 * Os LEDs ficam num gpio-emul próprio (app.overlay) e o nível dos
 * pinos é lido com gpio_emul_output_get. O tempo do native_sim é
 * simulado, então a amostragem a cada 1 ms é determinística.
 *******************************************************************/

#include "led_ctrl.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

static const struct gpio_dt_spec specs[] = {
	GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios),
	GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios),
};

#define NUM_LEDS ARRAY_SIZE(specs)

static const struct device *const test_gpio = DEVICE_DT_GET(DT_NODELABEL(test_gpio));

#define SAMPLE_MS    1
#define MAX_EDGES    32
#define PHASE_TOL_MS 2

struct observation {
	uint32_t edges;
	bool seen_on;
	bool seen_off;
	int last;
	int64_t edge_ms[MAX_EDGES];
};

static int level(size_t i)
{
	return gpio_emul_output_get_dt(&specs[i]);
}

/* Amostra todos os LEDs a cada SAMPLE_MS durante duration_ms. */
static void observe(struct observation obs[NUM_LEDS], uint32_t duration_ms)
{
	int64_t end;

	memset(obs, 0, NUM_LEDS * sizeof(obs[0]));
	for (size_t i = 0; i < NUM_LEDS; i++) {
		obs[i].last = level(i);
	}

	end = k_uptime_get() + duration_ms;
	while (k_uptime_get() < end) {
		k_msleep(SAMPLE_MS);
		for (size_t i = 0; i < NUM_LEDS; i++) {
			int lvl = level(i);

			obs[i].seen_on |= (lvl == 1);
			obs[i].seen_off |= (lvl == 0);
			if (lvl != obs[i].last) {
				if (obs[i].edges < MAX_EDGES) {
					obs[i].edge_ms[obs[i].edges] = k_uptime_get();
				}
				obs[i].edges++;
				obs[i].last = lvl;
			}
		}
	}
}

/* ---------------------------------------------------------------- */
/* Suite 1: controlador com zephyr,deferred-init ainda não pronto.  */
/* ---------------------------------------------------------------- */

ZTEST(led_ctrl_deferred, test_init_fails_until_controller_ready)
{
	zassert_false(device_is_ready(test_gpio), "controlador deveria estar adiado");
	zassert_equal(led_ctrl_init(), -ENODEV);

	zassert_ok(device_init(test_gpio));
	zassert_ok(led_ctrl_init());
}

/* Só roda enquanto o controlador ainda não foi inicializado. */
static bool controller_not_ready(const void *state)
{
	ARG_UNUSED(state);
	return !device_is_ready(test_gpio);
}

ZTEST_SUITE(led_ctrl_deferred, controller_not_ready, NULL, NULL, NULL, NULL);

/* ---------------------------------------------------------------- */
/* Suite 2: comportamento com o controlador pronto.                 */
/* ---------------------------------------------------------------- */

static void led_ctrl_before(void *fixture)
{
	ARG_UNUSED(fixture);
	zassert_ok(led_ctrl_init());
}

static void led_ctrl_after(void *fixture)
{
	ARG_UNUSED(fixture);
	(void)led_ctrl_off(LED_CTRL_ALL);
}

ZTEST(led_ctrl, test_init_succeeds_with_all_leds_off)
{
	zassert_ok(led_ctrl_on(LED_CTRL_ALL));
	zassert_ok(led_ctrl_init());

	zassert_equal(led_ctrl_count(), NUM_LEDS);
	for (size_t i = 0; i < NUM_LEDS; i++) {
		zassert_equal(level(i), 0, "LED %zu deveria comecar desligado", i);
	}
}

ZTEST(led_ctrl, test_init_again_stops_blinking_and_timer_still_works)
{
	struct observation obs[NUM_LEDS];

	zassert_ok(led_ctrl_blink(LED_CTRL_ALL, LED_PATTERN_FAST));
	k_msleep(CONFIG_APP_LED_PERIOD_FAST_MS);
	zassert_ok(led_ctrl_init());

	observe(obs, 2 * CONFIG_APP_LED_PERIOD_FAST_MS);
	for (size_t i = 0; i < NUM_LEDS; i++) {
		zassert_equal(obs[i].edges, 0, "LED %zu ainda pisca apos init", i);
		zassert_equal(level(i), 0);
	}

	zassert_ok(led_ctrl_blink(0, LED_PATTERN_FAST));
	observe(obs, 2 * CONFIG_APP_LED_PERIOD_FAST_MS);
	zassert_true(obs[0].edges > 0, "timer deixou de funcionar apos reinit");
}

ZTEST(led_ctrl, test_on_off_toggle_change_pin)
{
	zassert_ok(led_ctrl_on(0));
	zassert_equal(level(0), 1);
	zassert_equal(level(1), 0, "outro LED nao deveria mudar");

	zassert_ok(led_ctrl_off(0));
	zassert_equal(level(0), 0);

	zassert_ok(led_ctrl_toggle(0));
	zassert_equal(level(0), 1);
	zassert_ok(led_ctrl_toggle(0));
	zassert_equal(level(0), 0);
}

ZTEST(led_ctrl, test_blink_is_seen_on_and_off)
{
	struct observation obs[NUM_LEDS];

	zassert_ok(led_ctrl_blink(0, LED_PATTERN_NORMAL));
	observe(obs, 2 * CONFIG_APP_LED_PERIOD_NORMAL_MS);

	zassert_true(obs[0].seen_on);
	zassert_true(obs[0].seen_off);
	zassert_equal(obs[1].edges, 0, "LED parado nao deveria trocar");
}

ZTEST(led_ctrl, test_faster_led_toggles_more)
{
	struct observation obs[NUM_LEDS];

	zassert_ok(led_ctrl_blink(0, LED_PATTERN_SLOW));
	zassert_ok(led_ctrl_blink(1, LED_PATTERN_FAST));
	observe(obs, 2 * CONFIG_APP_LED_PERIOD_SLOW_MS);

	zassert_true(obs[1].edges > obs[0].edges, "rapido=%u lento=%u", obs[1].edges, obs[0].edges);
}

ZTEST(led_ctrl, test_other_led_does_not_shift_phase)
{
	const int64_t half = CONFIG_APP_LED_PERIOD_NORMAL_MS / 2;
	struct observation obs[NUM_LEDS];
	int64_t t0 = k_uptime_get();

	zassert_ok(led_ctrl_blink(0, LED_PATTERN_NORMAL));

	/* Mexe no LED 1 fora das bordas do LED 0. */
	k_msleep(half + half / 3);
	zassert_ok(led_ctrl_blink(1, LED_PATTERN_FAST));
	k_msleep(half / 2);
	zassert_ok(led_ctrl_off(1));

	observe(obs, 4 * half);
	zassert_true(obs[0].edges >= 3, "LED 0 trocou %u vezes", obs[0].edges);
	for (uint32_t k = 0; k < MIN(obs[0].edges, MAX_EDGES); k++) {
		int64_t offset = (obs[0].edge_ms[k] - t0) % half;

		zassert_true(offset <= PHASE_TOL_MS || offset >= half - PHASE_TOL_MS,
			     "borda %u fora de fase: %lld ms", k, (long long)offset);
	}
}

static void assert_cancels_blink(int (*cmd)(int led), int expected)
{
	struct observation obs[NUM_LEDS];

	zassert_ok(led_ctrl_blink(0, LED_PATTERN_FAST));
	k_msleep(CONFIG_APP_LED_PERIOD_FAST_MS / 4);
	zassert_ok(cmd(0));

	observe(obs, 3 * CONFIG_APP_LED_PERIOD_FAST_MS);
	zassert_equal(obs[0].edges, 0, "pisca nao foi cancelado");
	zassert_equal(level(0), expected);
}

ZTEST(led_ctrl, test_on_cancels_blink)
{
	assert_cancels_blink(led_ctrl_on, 1);
}

ZTEST(led_ctrl, test_off_cancels_blink)
{
	assert_cancels_blink(led_ctrl_off, 0);
}

ZTEST(led_ctrl, test_toggle_cancels_blink)
{
	/* blink acende o LED; 1/4 de período depois ainda está aceso, toggle apaga. */
	assert_cancels_blink(led_ctrl_toggle, 0);
}

ZTEST(led_ctrl, test_all_affects_every_led)
{
	struct observation obs[NUM_LEDS];

	zassert_ok(led_ctrl_on(LED_CTRL_ALL));
	for (size_t i = 0; i < NUM_LEDS; i++) {
		zassert_equal(level(i), 1, "LED %zu", i);
	}

	zassert_ok(led_ctrl_toggle(LED_CTRL_ALL));
	for (size_t i = 0; i < NUM_LEDS; i++) {
		zassert_equal(level(i), 0, "LED %zu", i);
	}

	zassert_ok(led_ctrl_blink(LED_CTRL_ALL, LED_PATTERN_FAST));
	observe(obs, 2 * CONFIG_APP_LED_PERIOD_FAST_MS);
	for (size_t i = 0; i < NUM_LEDS; i++) {
		zassert_true(obs[i].seen_on && obs[i].seen_off, "LED %zu nao piscou", i);
	}

	zassert_ok(led_ctrl_off(LED_CTRL_ALL));
	observe(obs, 2 * CONFIG_APP_LED_PERIOD_FAST_MS);
	for (size_t i = 0; i < NUM_LEDS; i++) {
		zassert_equal(obs[i].edges, 0, "LED %zu", i);
		zassert_equal(level(i), 0, "LED %zu", i);
	}
}

ZTEST(led_ctrl, test_invalid_arguments)
{
	const int past_end = (int)NUM_LEDS;

	zassert_equal(led_ctrl_on(past_end), -EINVAL);
	zassert_equal(led_ctrl_off(past_end), -EINVAL);
	zassert_equal(led_ctrl_toggle(past_end), -EINVAL);
	zassert_equal(led_ctrl_blink(past_end, LED_PATTERN_SLOW), -EINVAL);
	zassert_equal(led_ctrl_on(-2), -EINVAL);

	zassert_equal(led_ctrl_blink(0, LED_PATTERN_COUNT), -EINVAL);
	zassert_equal(led_ctrl_blink(0, (enum led_pattern) - 1), -EINVAL);
	zassert_equal(led_ctrl_blink(LED_CTRL_ALL, LED_PATTERN_COUNT), -EINVAL);

	for (size_t i = 0; i < NUM_LEDS; i++) {
		zassert_equal(level(i), 0, "argumento invalido mexeu no LED %zu", i);
	}
}

static bool controller_ready(const void *state)
{
	ARG_UNUSED(state);
	return device_is_ready(test_gpio);
}

ZTEST_SUITE(led_ctrl, controller_ready, NULL, led_ctrl_before, led_ctrl_after, NULL);

/*
 * O teste de deferred-init precisa rodar antes de qualquer device_init().
 * 1a passada: só led_ctrl_deferred (controlador adiado), que o inicializa;
 * 2a passada: só led_ctrl (controlador pronto).
 */
void test_main(void)
{
	ztest_run_all(NULL, false, 1, 1);
	ztest_run_all(NULL, false, 1, 1);
	ztest_verify_all_test_suites_ran();
}
