# Copyright (c) 2024 DxPru. All Rights Reserved.

cmake_minimum_required(VERSION 3.20)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

add_subdirectory(drivers)

project(app LANGUAGES C)

target_sources(app PRIVATE src/main.c)
