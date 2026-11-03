# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(unit)

set(APP_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

target_sources(app PRIVATE
  src/test_command.c
  src/test_line_editor.c
  ${APP_SRC}/command.c
  ${APP_SRC}/line_editor.c
)
target_include_directories(app PRIVATE ${APP_SRC})
