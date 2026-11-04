/*******************************************************************
 * @file led_ctrl.h
 *
 * @brief Driver de LEDs independentes multiplexados em um único k_timer.
 *
 * O resto do código pede "LED 1 piscando devagar" e não sabe nada de
 * timer. Os LEDs vêm do devicetree (aliases led0, led1, ...); o índice
 * de cada LED é a sua posição nessa lista.
 *
 * Toda função de ação aceita LED_CTRL_ALL para agir em todos os LEDs.
 * LED ou padrão fora do intervalo retorna -EINVAL.
 *******************************************************************/

#pragma once

#include <stddef.h>

/* Índice especial: aplica a ação a todos os LEDs. */
#define LED_CTRL_ALL (-1)

/* Padrões de pisca; períodos definidos por Kconfig (APP_LED_PERIOD_*). */
enum led_pattern {
	LED_PATTERN_SLOW,
	LED_PATTERN_NORMAL,
	LED_PATTERN_FAST,
	LED_PATTERN_COUNT,
};

/*
 * Verifica os controladores GPIO e configura todos os LEDs como saída,
 * desligados, cancelando qualquer pisca. Pode ser chamada de novo.
 * Retorna 0, -ENODEV se algum controlador não estiver pronto, ou o erro
 * da configuração do GPIO.
 */
int led_ctrl_init(void);

/* Quantidade de LEDs encontrados no devicetree. */
size_t led_ctrl_count(void);

/* Ligar, desligar e alternar cancelam o pisca em andamento do LED. */
int led_ctrl_on(int led);
int led_ctrl_off(int led);
int led_ctrl_toggle(int led);

/* Começa a piscar o LED (aceso primeiro) no padrão escolhido. */
int led_ctrl_blink(int led, enum led_pattern pattern);
