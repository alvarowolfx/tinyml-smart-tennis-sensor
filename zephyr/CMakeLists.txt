cmake_minimum_required(VERSION 3.13.1)

set(DTC_OVERLAY_FILE ./nrf52840dongle_nrf52840.overlay)

include($ENV{ZEPHYR_BASE}/cmake/app/boilerplate.cmake NO_POLICY_SCOPE)
project(tinyml-tennis-sensor)

FILE(GLOB app_sources ../src/*.c*)
target_sources(app PRIVATE ${app_sources})
