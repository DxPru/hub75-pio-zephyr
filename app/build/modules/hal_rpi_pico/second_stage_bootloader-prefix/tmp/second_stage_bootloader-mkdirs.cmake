# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/dxpru/Dev/Zephyr/zephyr/modules/hal_rpi_pico/bootloader"
  "/home/dxpru/Dev/Zephyr/app/build/bootloader"
  "/home/dxpru/Dev/Zephyr/app/build/modules/hal_rpi_pico/second_stage_bootloader-prefix"
  "/home/dxpru/Dev/Zephyr/app/build/modules/hal_rpi_pico/second_stage_bootloader-prefix/tmp"
  "/home/dxpru/Dev/Zephyr/app/build/modules/hal_rpi_pico/second_stage_bootloader-prefix/src/second_stage_bootloader-stamp"
  "/home/dxpru/Dev/Zephyr/app/build/modules/hal_rpi_pico/second_stage_bootloader-prefix/src"
  "/home/dxpru/Dev/Zephyr/app/build/modules/hal_rpi_pico/second_stage_bootloader-prefix/src/second_stage_bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/dxpru/Dev/Zephyr/app/build/modules/hal_rpi_pico/second_stage_bootloader-prefix/src/second_stage_bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/dxpru/Dev/Zephyr/app/build/modules/hal_rpi_pico/second_stage_bootloader-prefix/src/second_stage_bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
