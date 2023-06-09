cmake_minimum_required(VERSION 3.20)
project(zhub2 LANGUAGES CXX VERSION 2.26.599)

message(STATUS "${PROJECT_NAME} version ${PROJECT_VERSION}")

if(TEST)
  message(STATUS "Test")
  set(TEST_MODE "#define TEST 1")
endif()

if(DEBUG)
  set(PROJECT_DEFAULT_BUILD_TYPE "Debug")
else()
  set(PROJECT_DEFAULT_BUILD_TYPE "Release")
endif()

if(SIM800)
   set(SIM_800 "#define WITH_SIM800 1")
endif()

if(TELEGA)
  set(TELEGRAM "#define WITH_TELEGA 1 ")
endif()

if(GPIO)
  set(WITH_PI "#define IS_PI 1")
endif()

if(HTTP)
  set(HAS_HTTP "#define WITH_HTTP 1")
endif()

if(PROMETHEUS)
  set(HAS_PROMETHEUS "#define WITH_PROMETHEUS 1")
endif()

add_subdirectory(src)
add_subdirectory(gsb_utils)
if(TELEGA)
add_subdirectory(telebot32)
endif()

# Лкфае8472пыи
# sudo pmset standby 0
# sudo pmset autopoweroff 0
# sudo pmset hibernatemode 0
# sudo systemsetup -setcomputersleep Never