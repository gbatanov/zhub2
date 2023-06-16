cmake_minimum_required(VERSION 3.13)
project(RtspClientLibrary
		VERSION 1.0.0 
		LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(OVER_HTTP "Enable RTP? over HTTP" ON)
option(BACKCHANNEL "Enable backchannel" OFF)

add_compile_options(-DMETADATA -DREPLAY)

option(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(SRC
    bm/base64.cpp
    bm/hqueue.cpp
    bm/ppstack.cpp
    bm/rfc_md5.cpp
    bm/sys_buf.cpp
    bm/sys_log.cpp
    bm/sys_os.cpp
    bm/util.cpp
    bm/word_analyse.cpp

    http/http_cln.cpp
    http/http_parse.cpp

    rtp/aac_rtp_rx.cpp
    rtp/bit_vector.cpp
    rtp/h264_rtp_rx.cpp
    rtp/h265_rtp_rx.cpp
    rtp/mjpeg_rtp_rx.cpp
    rtp/mjpeg_tables.cpp
    rtp/mpeg4.cpp
    rtp/mpeg4_rtp_rx.cpp
    rtp/pcm_rtp_rx.cpp
    rtp/rtp_rx.cpp

    rtsp/rtsp_cln.cpp
    rtsp/rtsp_parse.cpp
    rtsp/rtsp_rcua.cpp
    rtsp/rtsp_util.cpp
    )

if(OVER_HTTP)
    set(SRC ${SRC} http/http_cln.cpp http/http_parse.cpp)
endif()

if(BACKCHANNEL)
    set (SRC ${PROJECT_NAME} rtsp/rtsp_backchannel.cpp)
endif()

add_library(${PROJECT_NAME} SHARED ${SRC})

target_include_directories(${PROJECT_NAME} PRIVATE  ${PROJECT_SOURCE_DIR}/
                                                    ${PROJECT_SOURCE_DIR}/bm
                                                    ${PROJECT_SOURCE_DIR}/http
                                                    ${PROJECT_SOURCE_DIR}/rtp
                                                    ${PROJECT_SOURCE_DIR}/rtsp
                                                    ${PROJECT_SOURCE_DIR}/media)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

install(TARGETS ${PROJECT_NAME} DESTINATION /usr/local/lib)

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Kraftway Inc.")

include(CPack)

