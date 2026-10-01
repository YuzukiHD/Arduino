# SPDX-License-Identifier: Apache-2.0
#
# The prebuilt core of the Arduino core for the F101: Zephyr, the core (src/core) and the libraries
# (libsrc/<name>) in one image. Built by tools/build-core:
#
#   -DBOARD=yuzukineko/sun252i_f101/evb
#   -DARDUINO_LIBRARIES="Wire;SPI;..."   libraries (libraries/<name>: headers, f101.conf, f101.overlay)
#   -DARDUINO_CORE_ID=<n>                identifies this build to the sketches linked against it
#   -DARDUINO_EXPORTS=<file>             response file with -u <symbol> lines: symbols kept for sketches
#
# A sketch is not part of this build: it is linked by plain gcc against core.elf and runs from a fixed RAM
# window that the core starts (cores/f101/sketch_abi.h, platform.txt).

cmake_minimum_required(VERSION 3.20.0)

set(ARDUINO_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(ARDUINO_VARIANT "evb")
set(ARDUINO_LIBRARIES "" CACHE STRING "Libraries to build in")
set(ARDUINO_EXPORTS "" CACHE STRING "Response file with -u <symbol> lines")
set(ARDUINO_CORE_ID "1" CACHE STRING "Id of the core build")
set(ARDUINO_VARIANT_DIR ${ARDUINO_ROOT}/variants/${ARDUINO_VARIANT})

list(APPEND EXTRA_CONF_FILE ${ARDUINO_VARIANT_DIR}/${ARDUINO_VARIANT}.conf)
list(APPEND EXTRA_DTC_OVERLAY_FILE ${ARDUINO_VARIANT_DIR}/${ARDUINO_VARIANT}.overlay)
# RAM above the core is the sketch window, not heap
list(APPEND EXTRA_DTC_OVERLAY_FILE ${ARDUINO_VARIANT_DIR}/prebuilt.overlay)

# A library may bring a Kconfig fragment (f101.conf) and a devicetree overlay (f101.overlay): they
# have to be known before Zephyr is configured.
foreach(lib ${ARDUINO_LIBRARIES})
  set(libdir ${ARDUINO_ROOT}/libraries/${lib})
  if(NOT EXISTS ${libdir})
    message(FATAL_ERROR "Arduino library '${lib}' not found")
  endif()
  if(EXISTS ${libdir}/f101.conf)
    list(APPEND EXTRA_CONF_FILE ${libdir}/f101.conf)
  endif()
  if(EXISTS ${libdir}/f101.overlay)
    list(APPEND EXTRA_DTC_OVERLAY_FILE ${libdir}/f101.overlay)
  endif()
endforeach()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(arduino_f101 LANGUAGES C CXX ASM)

file(GLOB CORE_SOURCES ${ARDUINO_ROOT}/src/core/*.cpp ${ARDUINO_ROOT}/src/core/*.c)
target_sources(app PRIVATE ${CORE_SOURCES})
target_include_directories(app PRIVATE ${ARDUINO_ROOT}/cores/f101 ${ARDUINO_VARIANT_DIR})
target_compile_definitions(app PRIVATE ARDUINO_ARCH_F101 ARDUINO_ARCH_ZEPHYR)
target_compile_options(app PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter -fpermissive>)

# the implementation of each library is kept out of libraries/<n>/src (the Arduino IDE compiles
# everything in src/ with the sketch, but it is part of this core)
foreach(lib ${ARDUINO_LIBRARIES})
  file(GLOB lib_sources ${ARDUINO_ROOT}/libsrc/${lib}/*.c ${ARDUINO_ROOT}/libsrc/${lib}/*.cpp)
  target_sources(app PRIVATE ${lib_sources})
  target_include_directories(app PRIVATE ${ARDUINO_ROOT}/libraries/${lib}/src ${ARDUINO_ROOT}/libsrc/${lib})
endforeach()

# identifies this core to the sketches linked against it; keeps the symbols a sketch may call
zephyr_ld_options(-Wl,--defsym=f101_core_id_value=${ARDUINO_CORE_ID})
if(ARDUINO_EXPORTS)
  zephyr_ld_options(-Wl,@${ARDUINO_EXPORTS})
endif()

# the toolchain libm provides sin/cos/pow/...
target_link_libraries(app PRIVATE m)
