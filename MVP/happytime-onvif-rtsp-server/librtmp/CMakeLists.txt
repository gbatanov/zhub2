cmake_minimum_required(VERSION 3.13)
project(rtmp LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED)

add_library(${PROJECT_NAME} STATIC
    amf.c
    amf.h
    bytes.h
    CMakeLists.txt
    dhgroups.h
    dh.h
    handshake.h
    hashswf.c
    http.h
    log.c
    log.h
    parseurl.c
    rtmp.c
    rtmp.h
    rtmp_sys.h
    )
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/../openssl/include)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/../openssl/lib/linux/libcrypto.so.1.1
                                              ${PROJECT_SOURCE_DIR}/../openssl/lib/linux/libssl.so.1.1)


