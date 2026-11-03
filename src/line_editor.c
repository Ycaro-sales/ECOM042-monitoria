/*******************************************************************
 * @file line_editor.c
 *
 * @brief Montagem de linhas com backspace e descarte de linha longa.
 *******************************************************************/

#include "line_editor.h"

#define KEY_BACKSPACE 0x08
#define KEY_DELETE    0x7F

void line_editor_init(struct line_editor *le, char *buf, size_t size)
{
	le->buf = buf;
	le->size = size;
	le->len = 0;
	le->overflow = false;
	le->last_cr = false;
	if (size > 0) {
		buf[0] = '\0';
	}
}

static enum line_event finish_line(struct line_editor *le)
{
	bool overflow = le->overflow;

	le->buf[le->len] = '\0';
	le->len = 0;
	le->overflow = false;

	return overflow ? LINE_TOO_LONG : LINE_DONE;
}

enum line_event line_editor_feed(struct line_editor *le, char c)
{
	bool after_cr = le->last_cr;

	le->last_cr = (c == '\r');

	if (c == '\r' || c == '\n') {
		/* "\r\n" conta como um único terminador. */
		if (c == '\n' && after_cr) {
			return LINE_IGNORED;
		}
		return finish_line(le);
	}

	if (c == KEY_BACKSPACE || c == KEY_DELETE) {
		/* Linha já condenada: nada a apagar até o terminador. */
		if (le->overflow || le->len == 0) {
			return LINE_IGNORED;
		}
		le->len--;
		return LINE_ERASED;
	}

	if ((unsigned char)c < 0x20) {
		return LINE_IGNORED;
	}

	/* Sem espaço (inclusive após o overflow): descarta até o terminador. */
	if (le->len + 1 >= le->size) {
		le->overflow = true;
		return LINE_CHAR;
	}

	le->buf[le->len++] = c;
	return LINE_CHAR;
}
