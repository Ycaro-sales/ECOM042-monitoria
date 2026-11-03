/*******************************************************************
 * @file command.c
 *
 * @brief Implementação do parser/dispatcher de comandos (Atividade-04).
 *
 * Baseado na tabela de function pointers da Atividade-02, agora
 * indexada pelo nome do comando em vez de um enum.
 *******************************************************************/

#include "command.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

struct command {
	const char *name;
	const char *help;
	command_handler_t handler;
};

static int handle_help(const struct command_hal *hal, const char *arg, struct cmd_out *out);
static int handle_led(const struct command_hal *hal, const char *arg, struct cmd_out *out);
static int handle_btn(const struct command_hal *hal, const char *arg, struct cmd_out *out);
static int handle_uptime(const struct command_hal *hal, const char *arg, struct cmd_out *out);
static int handle_echo(const struct command_hal *hal, const char *arg, struct cmd_out *out);

static const struct command command_table[] = {
	{"HELP", "HELP: lista os comandos", handle_help},
	{"LED", "LED [ON|OFF]: liga/desliga o led0 ou mostra seu estado", handle_led},
	{"BTN", "BTN: le o botao sw0", handle_btn},
	{"UPTIME", "UPTIME: tempo desde o boot em ms", handle_uptime},
	{"ECHO", "ECHO <texto>: devolve o texto", handle_echo},
};

#define COMMAND_TABLE_SIZE (sizeof(command_table) / sizeof(command_table[0]))

/* Acrescenta texto formatado ao buffer, truncando sem estourar. */
static void out_printf(struct cmd_out *out, const char *fmt, ...)
{
	va_list args;
	int n;

	if (out->len >= out->size) {
		return;
	}

	va_start(args, fmt);
	n = vsnprintf(out->buf + out->len, out->size - out->len, fmt, args);
	va_end(args);

	if (n < 0) {
		return;
	}

	out->len += (size_t)n;
	if (out->len >= out->size) {
		out->len = out->size - 1;
	}
}

static bool arg_is_empty(const char *arg)
{
	while (isspace((unsigned char)*arg)) {
		arg++;
	}
	return *arg == '\0';
}

int parse_command(const char *cmd)
{
	if (cmd == NULL) {
		return -1;
	}
	if (strcasecmp(cmd, "ON") == 0) {
		return 1;
	}
	if (strcasecmp(cmd, "OFF") == 0) {
		return 0;
	}
	return -1;
}

static int handle_help(const struct command_hal *hal, const char *arg, struct cmd_out *out)
{
	(void)hal;

	if (!arg_is_empty(arg)) {
		return -ENOCMD;
	}

	for (size_t i = 0; i < COMMAND_TABLE_SIZE; i++) {
		out_printf(out, "%s\r\n", command_table[i].help);
	}
	return 0;
}

static int handle_led(const struct command_hal *hal, const char *arg, struct cmd_out *out)
{
	char word[8];
	size_t len;
	int on;

	if (arg_is_empty(arg)) {
		out_printf(out, "LED: %s\r\n", hal->led_get() ? "ON" : "OFF");
		return 0;
	}

	/* Isola o argumento sem espaços nas pontas para o parse_command. */
	while (isspace((unsigned char)*arg)) {
		arg++;
	}
	len = strlen(arg);
	while (len > 0 && isspace((unsigned char)arg[len - 1])) {
		len--;
	}
	if (len >= sizeof(word)) {
		return -ENOCMD;
	}
	memcpy(word, arg, len);
	word[len] = '\0';

	on = parse_command(word);
	if (on < 0) {
		return -ENOCMD;
	}

	if (hal->led_set(on == 1) != 0) {
		out_printf(out, "ERR: led failure\r\n");
		return 0;
	}
	out_printf(out, "OK\r\n");
	return 0;
}

static int handle_btn(const struct command_hal *hal, const char *arg, struct cmd_out *out)
{
	int value;

	if (!arg_is_empty(arg)) {
		return -ENOCMD;
	}

	value = hal->button_get();
	if (value < 0) {
		out_printf(out, "ERR: button failure\r\n");
		return 0;
	}
	out_printf(out, "BTN: %d\r\n", value ? 1 : 0);
	return 0;
}

static int handle_uptime(const struct command_hal *hal, const char *arg, struct cmd_out *out)
{
	if (!arg_is_empty(arg)) {
		return -ENOCMD;
	}

	out_printf(out, "UPTIME: %lld ms\r\n", (long long)hal->uptime_ms());
	return 0;
}

static int handle_echo(const struct command_hal *hal, const char *arg, struct cmd_out *out)
{
	(void)hal;

	out_printf(out, "%s\r\n", arg);
	return 0;
}

uint16_t dispatch(const struct command_hal *hal, const char *line, char *out, size_t out_size)
{
	struct cmd_out o = {.buf = out, .size = out_size, .len = 0};
	const char *arg;
	size_t name_len;

	if (out_size == 0) {
		return ENOCMD;
	}
	out[0] = '\0';

	while (*line == ' ') {
		line++;
	}
	if (*line == '\0') {
		return 0;
	}

	/* Nome = primeira palavra; argumento = tudo após o primeiro espaço. */
	arg = strchr(line, ' ');
	if (arg != NULL) {
		name_len = (size_t)(arg - line);
		arg++;
	} else {
		name_len = strlen(line);
		arg = "";
	}

	for (size_t i = 0; i < COMMAND_TABLE_SIZE; i++) {
		const struct command *cmd = &command_table[i];

		if (strlen(cmd->name) == name_len && strncasecmp(cmd->name, line, name_len) == 0) {
			if (cmd->handler(hal, arg, &o) == 0) {
				return 0;
			}
			break;
		}
	}

	o.len = 0;
	out[0] = '\0';
	out_printf(&o, "ERR: unknown command\r\n");
	return ENOCMD;
}
