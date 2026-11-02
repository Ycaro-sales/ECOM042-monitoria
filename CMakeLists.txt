# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(hello_world)

target_sources(app PRIVATE src/main.c src/sensor.c)

# Simulação do botão pelo teclado: só existe no native_sim.
if(CONFIG_BOARD_NATIVE_SIM)
  target_sources(app PRIVATE src/button_sim.c)
endif()
