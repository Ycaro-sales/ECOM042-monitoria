/*******************************************************************
 * @file uart_transport.h
 *
 * @brief Transporte da mini-shell por UART (RX por interrupção).
 *******************************************************************/

#pragma once

#include <zephyr/device.h>

#include "command.h"

/*
 * Liga a interrupção de RX da UART e inicia a thread que monta as
 * linhas e despacha os comandos. Retorna 0 ou erro negativo.
 */
int uart_transport_start(const struct device *uart, const struct command_hal *hal);
