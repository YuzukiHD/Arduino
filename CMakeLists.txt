# SPDX-License-Identifier: Apache-2.0
#
# Arduino core for the F101 on Zephyr: this is the Zephyr application that
# links the core, the used libraries and the sketch. Normally it is driven by
# tools/f101-arduino:
#
#   -DBOARD=yuzukineko/sun252i_f101/evb
#   -DARDUINO_SKETCH=<sketch.cpp>          (the .ino already turned into C++)
#   -DARDUINO_LIBRARIES="Wire;SPI"         (library names)
#   -DARDUINO_LIBRARY_PATHS="dir;dir"      (extra places that hold libraries)
#   -DARDUINO_VARIANT=evb

cmake_minimum_required(VERSION 3.20.0)

set(ARDUINO_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(ARDUINO_VARIANT "evb" CACHE STRING "Arduino variant directory under variants/")
set(ARDUINO_SKETCH "" CACHE STRING "C++ sources of the sketch")
set(ARDUINO_LIBRARIES "" CACHE STRING "Libraries to link")
set(ARDUINO_LIBRARY_PATHS "" CACHE STRING "Extra library search paths")

set(ARDUINO_VARIANT_DIR ${ARDUINO_ROOT}/variants/${ARDUINO_VARIANT})
if(NOT EXISTS ${ARDUINO_VARIANT_DIR})
  message(FATAL_ERROR "unknown Arduino variant '${ARDUINO_VARIANT}'")
endif()

list(APPEND EXTRA_CONF_FILE ${ARDUINO_VARIANT_DIR}/${ARDUINO_VARIANT}.conf)
list(APPEND EXTRA_DTC_OVERLAY_FILE ${ARDUINO_VARIANT_DIR}/${ARDUINO_VARIANT}.overlay)

# Resolve the libraries first: a library may bring a Kconfig fragment (f101.conf)
# and a devicetree overlay (f101.overlay) for the hardware it needs, they have
# to be known before Zephyr is configured.
# Library layouts: <path>/<name>/src (1.5) or <path>/<name> (legacy, plus utility/).
set(ARDUINO_LIB_DIRS "")
foreach(lib ${ARDUINO_LIBRARIES})
  set(found FALSE)
  foreach(base ${ARDUINO_LIBRARY_PATHS} ${ARDUINO_ROOT}/libraries)
    if(EXISTS ${base}/${lib})
      list(APPEND ARDUINO_LIB_DIRS ${base}/${lib})
      if(EXISTS ${base}/${lib}/f101.conf)
        list(APPEND EXTRA_CONF_FILE ${base}/${lib}/f101.conf)
      endif()
      if(EXISTS ${base}/${lib}/f101.overlay)
        list(APPEND EXTRA_DTC_OVERLAY_FILE ${base}/${lib}/f101.overlay)
      endif()
      set(found TRUE)
      break()
    endif()
  endforeach()
  if(NOT found)
    message(WARNING "Arduino library '${lib}' not found")
  endif()
endforeach()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(arduino_f101 LANGUAGES C CXX ASM)

if(NOT ARDUINO_SKETCH)
  message(FATAL_ERROR "ARDUINO_SKETCH is not set (use tools/f101-arduino build)")
endif()

file(GLOB CORE_SOURCES ${ARDUINO_ROOT}/cores/f101/*.cpp ${ARDUINO_ROOT}/cores/f101/*.c)
target_sources(app PRIVATE ${CORE_SOURCES} ${ARDUINO_SKETCH})
target_include_directories(app PRIVATE ${ARDUINO_ROOT}/cores/f101 ${ARDUINO_VARIANT_DIR})
if(ARDUINO_SKETCH_DIR)
  target_include_directories(app PRIVATE ${ARDUINO_SKETCH_DIR})
endif()
target_compile_definitions(app PRIVATE ARDUINO_ARCH_F101 ARDUINO_ARCH_ZEPHYR)
# the sketch is C++ with Arduino semantics
target_compile_options(app PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-parameter -fpermissive>)

# sources and include directories of the libraries found above
foreach(libdir ${ARDUINO_LIB_DIRS})
  if(EXISTS ${libdir}/src)
    set(inc ${libdir}/src)
    file(GLOB lib_sources ${inc}/*.c ${inc}/*.cpp ${inc}/*.S ${inc}/*/*.c ${inc}/*/*.cpp)
  else()
    set(inc ${libdir})
    file(GLOB lib_sources ${inc}/*.c ${inc}/*.cpp ${inc}/utility/*.c ${inc}/utility/*.cpp)
  endif()
  target_sources(app PRIVATE ${lib_sources})
  target_include_directories(app PRIVATE ${inc})
endforeach()

# the toolchain libm provides sin/cos/pow/...
target_link_libraries(app PRIVATE m)
