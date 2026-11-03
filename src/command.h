/*******************************************************************
 * @file command.h
 *
 * @brief Parser e dispatcher de comandos em texto (Atividade-04).
 *
 * Camada pura: não chama UART, GPIO nem printk. Recebe uma linha,
 * consulta a tabela de comandos (function pointers) e escreve a
 * resposta num buffer. O acesso ao hardware é injetado via
 * struct command_hal, o que permite testar com fakes.
 *******************************************************************/

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ENOCMD 1 /* Codigo de erro comando não existe */

/* Operações de hardware usadas pelos comandos. */
struct command_hal {
	int (*led_set)(bool on);
	bool (*led_get)(void);
	int (*button_get)(void);
	int64_t (*uptime_ms)(void);
};

/* Cursor de escrita no buffer de resposta. */
struct cmd_out {
	char *buf;
	size_t size;
	size_t len;
};

/* Retorna 0 em sucesso ou -ENOCMD se o argumento não for aceito. */
typedef int (*command_handler_t)(const struct command_hal *hal, const char *arg,
				 struct cmd_out *out);

/* "ON" -> 1, "OFF" -> 0 (sem diferenciar maiúsculas), qualquer outro -> -1. */
int parse_command(const char *cmd);

/*
 * Executa a linha e escreve a resposta em out (cada linha terminada em
 * "\r\n", string sempre terminada em NUL). Linha vazia gera resposta vazia.
 * Retorna 0 ou ENOCMD (resposta "ERR: unknown command").
 */
uint16_t dispatch(const struct command_hal *hal, const char *line, char *out, size_t out_size);
