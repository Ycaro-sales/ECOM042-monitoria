# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(project)

file(GLOB SOURCES_APP src/*.c)

# button_sim.c usa o gpio_emul, que só existe no native_sim.
if(NOT CONFIG_BOARD_NATIVE_SIM)
  list(REMOVE_ITEM SOURCES_APP ${CMAKE_CURRENT_SOURCE_DIR}/src/button_sim.c)
endif()

target_sources(app PRIVATE ${SOURCES_APP})
