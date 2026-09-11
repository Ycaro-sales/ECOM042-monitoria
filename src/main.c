/*******************************************************************
 * @file main.c
 *
 * @brief Main file.
 * @author João Matheus Nascimento Dias (jmnd@ic.ufal.br)
 * @version 0.1
 * @date 19/10/2026
 *******************************************************************/

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, CONFIG_MAIN_LOG_LEVEL);

#include "command.h"

int main(void)
{
	LOG_INF("Hello World! %s", CONFIG_BOARD);
	printk("Hello World! %s\n", CONFIG_BOARD_TARGET);
	printk("parse_command(ON) = %d\n", parse_command("ON"));
	printk("parse_command(OFF) = %d\n", parse_command("OFF"));
	printk("parse_command(FOO) = %d\n", parse_command("FOO"));

	return 0;
}
