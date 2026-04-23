cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(central)

target_sources(app PRIVATE
  src/main.c
  src/central.c
  src/parse.c
  src/kalman.c
  lib/least_squares.c
)

zephyr_library_include_directories(${ZEPHYR_BASE}/samples/bluetooth)

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/lib
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
