cmake_minimum_required(VERSION 3.20)
project(zhub LANGUAGES CXX VERSION ${PROJECT_VERSION})

set(PROJECT_DEFAULT_BUILD_TYPE ${PROJECT_DEFAULT_BUILD_TYPE})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -I. -Wno-unused-result -Wno-unused-variable")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -pthread")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDebug=1  -g  -pthread")
    set(DEBUG_STRING "#define DEBUG 1")
endif()

message(STATUS "${PROJECT_NAME} build type: ${CMAKE_BUILD_TYPE}")

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  set(PREFIX "/opt/local")
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-rpath,/opt/local/lib")
else()
  set(PREFIX "/usr/local")
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-rpath,/usr/local/lib")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

configure_file(../version.h.in ../../src/version.h)

list( APPEND SOURCES
  main.cpp
  zigbee/attribute.cpp
  zigbee/controller.cpp
  zigbee/zhub.cpp
  zigbee/end_device.cpp
  zigbee/uart.cpp
  zigbee/zdo.cpp
  zigbee/cluster/basic.cpp
  zigbee/cluster/power_configuration.cpp
  zigbee/cluster/onoff.cpp
  zigbee/cluster/device_temperature_configuration.cpp
  zigbee/cluster/analog_input.cpp
  zigbee/cluster/multistate_input.cpp
  zigbee/cluster/xiaomi.cpp
  zigbee/cluster/simple_metering.cpp
  zigbee/cluster/electrical_measurements.cpp
  zigbee/cluster/tuya.cpp
  zigbee/cluster/other.cpp
  comport/unix.cpp
  comport/serial.cpp
)

if(TELEGA)
list( APPEND SOURCES 
   telega32.cpp
  )
endif()

if(SIM800)
  list( APPEND SOURCES 
    modem.cpp
  )
endif()

if(HTTP)
  list( APPEND SOURCES 
    http.cpp
    httpserver.cpp
  )
endif()

if (GPIO)
  list( APPEND SOURCES 
    pi4-gpio.cpp
  )
endif()

if(PROMETHEUS)
list( APPEND SOURCES 
exposer.cpp
)
endif()

set(DEPENDENTS "libcurl")
include(FindPkgConfig)
pkg_check_modules(PKGS REQUIRED ${DEPENDENTS})

foreach(flag ${PKGS_CFLAGS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
endforeach(flag)
foreach(flag ${PKGS_LDFLAGS})
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${flag}")
endforeach(flag)

string(STRIP ${CMAKE_EXE_LINKER_FLAGS} CMAKE_EXE_LINKER_FLAGS)
message(STATUS ${CMAKE_EXE_LINKER_FLAGS})

add_executable (${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PUBLIC gsbutils)

if(TELEGA)
  target_link_libraries(${PROJECT_NAME} PUBLIC telebot32)
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  find_library(IOKIT_LIBRARY IOKit)
  find_library(FOUNDATION_LIBRARY Foundation)
  target_link_libraries(${PROJECT_NAME} PUBLIC ${FOUNDATION_LIBRARY} ${IOKIT_LIBRARY})
else()
target_link_libraries(${PROJECT_NAME} PUBLIC curl)  
endif()

if(GPIO)
  target_link_libraries(${PROJECT_NAME} PUBLIC gpiod)
endif()

set_property(TARGET ${PROJECT_NAME} PROPERTY BUILD_WITH_INSTALL_RPATH TRUE)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION /usr/local/bin)

install(FILES gsb_style.css DESTINATION /usr/local/etc/zhub2/css/)

if(TELEGA)
  message(STATUS "Bot name ${BOT_NAME}")
endif()

if(TEST)
  install(FILES ../map_addr_test.cfg DESTINATION /usr/local/etc/zhub2/)
#  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/.token DESTINATION /usr/local/etc/telebot32/.token${BOT_NAME})
else()
install(FILES ../map_addr.cfg DESTINATION /usr/local/etc/zhub2/)
#  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/.token DESTINATION /usr/local/etc/telebot32/.token${BOT_NAME})
endif()

