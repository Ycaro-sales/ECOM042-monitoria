/*******************************************************************
 * @file test_line_editor.c
 *
 * @brief Testes da montagem de linhas (backspace, terminadores,
 *        descarte de linha longa).
 *******************************************************************/

#include <string.h>
#include <zephyr/ztest.h>

#include "line_editor.h"

#define LINE_SIZE 64

static char buf[LINE_SIZE];
static struct line_editor le;

/* Alimenta todos os bytes de s e devolve o evento do último. */
static enum line_event feed(const char *s, size_t n)
{
	enum line_event ev = LINE_IGNORED;

	for (size_t i = 0; i < n; i++) {
		ev = line_editor_feed(&le, s[i]);
	}
	return ev;
}

#define FEED(s) feed((s), sizeof(s) - 1)

static void reset_editor(void *fixture)
{
	ARG_UNUSED(fixture);
	line_editor_init(&le, buf, sizeof(buf));
}

ZTEST_SUITE(line_editor, NULL, NULL, reset_editor, NULL, NULL);

ZTEST(line_editor, test_char_events)
{
	zassert_equal(line_editor_feed(&le, 'a'), LINE_CHAR);
	zassert_equal(line_editor_feed(&le, '\x01'), LINE_IGNORED);
}

ZTEST(line_editor, test_terminators)
{
	zassert_equal(FEED("HELP\r"), LINE_DONE);
	zassert_str_equal(buf, "HELP");

	zassert_equal(FEED("BTN\n"), LINE_DONE);
	zassert_str_equal(buf, "BTN");
}

ZTEST(line_editor, test_crlf_is_single_terminator)
{
	zassert_equal(FEED("LED\r"), LINE_DONE);
	zassert_equal(line_editor_feed(&le, '\n'), LINE_IGNORED);
	zassert_equal(FEED("BTN\r"), LINE_DONE);
	zassert_str_equal(buf, "BTN");
}

ZTEST(line_editor, test_empty_line)
{
	zassert_equal(line_editor_feed(&le, '\r'), LINE_DONE);
	zassert_str_equal(buf, "");
}

ZTEST(line_editor, test_backspace_and_delete)
{
	zassert_equal(FEED("LEX"), LINE_CHAR);
	zassert_equal(line_editor_feed(&le, 0x08), LINE_ERASED);
	zassert_equal(FEED("D OM"), LINE_CHAR);
	zassert_equal(line_editor_feed(&le, 0x7F), LINE_ERASED);
	zassert_equal(FEED("N\r"), LINE_DONE);
	zassert_str_equal(buf, "LED ON");
}

ZTEST(line_editor, test_backspace_on_empty_line)
{
	zassert_equal(line_editor_feed(&le, 0x08), LINE_IGNORED);
	zassert_equal(line_editor_feed(&le, 0x7F), LINE_IGNORED);
	zassert_equal(FEED("A\r"), LINE_DONE);
	zassert_str_equal(buf, "A");
}

ZTEST(line_editor, test_max_length_line_fits)
{
	char line[LINE_SIZE];

	memset(line, 'A', LINE_SIZE - 1);
	line[LINE_SIZE - 1] = '\r';
	zassert_equal(feed(line, LINE_SIZE), LINE_DONE);
	zassert_equal(strlen(buf), LINE_SIZE - 1);
}

ZTEST(line_editor, test_too_long_line_is_discarded)
{
	char line[LINE_SIZE + 1];

	memset(line, 'A', LINE_SIZE);
	line[LINE_SIZE] = '\r';
	zassert_equal(feed(line, LINE_SIZE), LINE_CHAR);
	/* Backspace não "salva" uma linha já descartada. */
	zassert_equal(line_editor_feed(&le, 0x08), LINE_IGNORED);
	zassert_equal(line_editor_feed(&le, '\r'), LINE_TOO_LONG);

	/* A linha seguinte volta a funcionar normalmente. */
	zassert_equal(FEED("LED\r"), LINE_DONE);
	zassert_str_equal(buf, "LED");
}
