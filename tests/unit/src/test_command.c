/*******************************************************************
 * @file test_command.c
 *
 * @brief Testes do parser/dispatcher com um hardware fake.
 *******************************************************************/

#include <string.h>
#include <zephyr/ztest.h>

#include "command.h"

static bool fake_led;
static int fake_led_set_calls;
static int fake_button;

static int fake_led_set(bool on)
{
	fake_led = on;
	fake_led_set_calls++;
	return 0;
}

static bool fake_led_get(void)
{
	return fake_led;
}

static int fake_button_get(void)
{
	return fake_button;
}

static int64_t fake_uptime_ms(void)
{
	return 1234;
}

static const struct command_hal fake_hal = {
	.led_set = fake_led_set,
	.led_get = fake_led_get,
	.button_get = fake_button_get,
	.uptime_ms = fake_uptime_ms,
};

static char out[256];

static uint16_t run(const char *line)
{
	return dispatch(&fake_hal, line, out, sizeof(out));
}

static void reset_fakes(void *fixture)
{
	ARG_UNUSED(fixture);
	fake_led = false;
	fake_led_set_calls = 0;
	fake_button = 0;
	memset(out, 0, sizeof(out));
}

ZTEST_SUITE(command, NULL, NULL, reset_fakes, NULL, NULL);

ZTEST(command, test_parse_command)
{
	zassert_equal(parse_command("ON"), 1);
	zassert_equal(parse_command("OFF"), 0);
	zassert_equal(parse_command("on"), 1);
	zassert_equal(parse_command("oFf"), 0);
	zassert_equal(parse_command("FOO"), -1);
	zassert_equal(parse_command(""), -1);
	zassert_equal(parse_command(NULL), -1);
}

ZTEST(command, test_help_lists_one_line_per_command)
{
	const char *names[] = {"HELP", "LED", "BTN", "UPTIME", "ECHO"};
	const char *p = out;
	int lines = 0;

	zassert_equal(run("HELP"), 0);
	for (size_t i = 0; i < ARRAY_SIZE(names); i++) {
		zassert_not_null(strstr(out, names[i]), "faltou %s", names[i]);
	}
	while ((p = strstr(p, "\r\n")) != NULL) {
		lines++;
		p += 2;
	}
	zassert_equal(lines, ARRAY_SIZE(names));
	zassert_equal(strcmp(out + strlen(out) - 2, "\r\n"), 0);
}

ZTEST(command, test_led_on_off)
{
	zassert_equal(run("LED ON"), 0);
	zassert_str_equal(out, "OK\r\n");
	zassert_true(fake_led);

	zassert_equal(run("LED OFF"), 0);
	zassert_str_equal(out, "OK\r\n");
	zassert_false(fake_led);
	zassert_equal(fake_led_set_calls, 2);
}

ZTEST(command, test_case_insensitive)
{
	zassert_equal(run("led on"), 0);
	zassert_str_equal(out, "OK\r\n");
	zassert_true(fake_led);

	zassert_equal(run("Led"), 0);
	zassert_str_equal(out, "LED: ON\r\n");
}

ZTEST(command, test_led_state)
{
	zassert_equal(run("LED"), 0);
	zassert_str_equal(out, "LED: OFF\r\n");

	fake_led = true;
	zassert_equal(run("LED"), 0);
	zassert_str_equal(out, "LED: ON\r\n");
}

ZTEST(command, test_led_invalid_arg)
{
	zassert_equal(run("LED FOO"), ENOCMD);
	zassert_str_equal(out, "ERR: unknown command\r\n");
	zassert_equal(fake_led_set_calls, 0);
}

ZTEST(command, test_btn)
{
	zassert_equal(run("BTN"), 0);
	zassert_str_equal(out, "BTN: 0\r\n");

	fake_button = 1;
	zassert_equal(run("btn"), 0);
	zassert_str_equal(out, "BTN: 1\r\n");
}

ZTEST(command, test_uptime)
{
	zassert_equal(run("UPTIME"), 0);
	zassert_str_equal(out, "UPTIME: 1234 ms\r\n");
}

ZTEST(command, test_echo_preserves_text)
{
	zassert_equal(run("ECHO Hello  World!"), 0);
	zassert_str_equal(out, "Hello  World!\r\n");

	zassert_equal(run("echo MiXeD"), 0);
	zassert_str_equal(out, "MiXeD\r\n");
}

ZTEST(command, test_unknown_command)
{
	zassert_equal(run("FOO"), ENOCMD);
	zassert_str_equal(out, "ERR: unknown command\r\n");

	/* Prefixo de um comando válido não é o comando. */
	zassert_equal(run("LE"), ENOCMD);
	zassert_equal(run("LEDX"), ENOCMD);
	zassert_equal(run("UPTIME 5"), ENOCMD);
}

ZTEST(command, test_empty_line_has_no_response)
{
	zassert_equal(run(""), 0);
	zassert_str_equal(out, "");
	zassert_equal(run("   "), 0);
	zassert_str_equal(out, "");
}

ZTEST(command, test_small_output_buffer_is_truncated)
{
	char small[8];

	dispatch(&fake_hal, "ECHO 0123456789", small, sizeof(small));
	zassert_equal(strlen(small), sizeof(small) - 1);
	zassert_str_equal(small, "0123456");
}
