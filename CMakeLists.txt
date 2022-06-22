#
# Copyright (c) 2022 Lars Øvergård
#
# Copyright (c) 2020 Nordic Semiconductor
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(water_qa)

# NORDIC SDK APP START
target_sources(app PRIVATE src/main.c)
zephyr_include_directories(src)
add_subdirectory(src/watchdog)
# NORDIC SDK APP END
