# Copyright (c) 2024 DxPru. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

zephyr_syscall_include_directories(include)

zephyr_include_directories(include)

add_subdirectory(drivers)

project(app LANGUAGES C)

target_sources(app PRIVATE src/main.c)

