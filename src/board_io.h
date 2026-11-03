/*******************************************************************
 * @file board_io.h
 *
 * @brief LED e botão obtidos pelo devicetree (aliases led0 e sw0).
 *******************************************************************/

#pragma once

#include "command.h"

/* Configura led0 (saída, apagado) e sw0 (entrada). */
int board_io_init(void);

/* Implementação real das operações usadas pelos comandos. */
extern const struct command_hal board_hal;
