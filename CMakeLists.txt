cmake_minimum_required(VERSION 3.20)
project(zhub2 LANGUAGES CXX VERSION 2.27.616)

message(STATUS "${PROJECT_NAME} version ${PROJECT_VERSION}")

if(DEBUG)
  set(PROJECT_DEFAULT_BUILD_TYPE "Debug")
else()
  set(PROJECT_DEFAULT_BUILD_TYPE "Release")
endif()

add_subdirectory(src)
add_subdirectory(gsb_utils)
add_subdirectory(telebot32)

# sudo pmset standby 0
# sudo pmset autopoweroff 0
# sudo pmset hibernatemode 0
# sudo systemsetup -setcomputersleep Never