/*******************************************************************
 * @file line_editor.h
 *
 * @brief Montagem de linhas a partir de bytes recebidos (Atividade-04).
 *
 * Camada pura: não faz I/O. Cada byte alimentado retorna um evento
 * que diz ao transporte o que ecoar ou se a linha está pronta.
 *******************************************************************/

#pragma once

#include <stdbool.h>
#include <stddef.h>

enum line_event {
	LINE_IGNORED,  /* nada a fazer */
	LINE_CHAR,     /* caractere aceito: ecoar o byte */
	LINE_ERASED,   /* último caractere apagado: ecoar "\b \b" */
	LINE_DONE,     /* linha completa em buf (terminada em NUL) */
	LINE_TOO_LONG, /* linha excedeu o buffer e foi descartada */
};

struct line_editor {
	char *buf;
	size_t size; /* inclui o NUL: aceita até size - 1 caracteres */
	size_t len;
	bool overflow;
	bool last_cr;
};

void line_editor_init(struct line_editor *le, char *buf, size_t size);

/*
 * Processa um byte. Após LINE_DONE a linha fica em le->buf até o
 * próximo byte alimentado, que começa uma linha nova.
 */
enum line_event line_editor_feed(struct line_editor *le, char c);
