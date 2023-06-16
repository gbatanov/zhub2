cmake_minimum_required(VERSION 3.13)
project(mvp LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(PkgConfig)

pkg_check_modules(GST REQUIRED
    glib-2.0
    gstreamer-1.0
    gstreamer-base-1.0
)

option(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

add_subdirectory(${PROJECT_SOURCE_DIR}/happytime-rtsp-client/ RtspClientLibrary)

set(SRC
    main.cpp
    cdispatcher.cpp
    videocorehtconnector.c
    hqueue.c
    )

set(HDR
    cdispatcher.h
    videocorehtconnector.h
    hqueue.h
    packet.h
    )

add_executable(${PROJECT_NAME} ${SRC} ${HDR})


target_include_directories(${PROJECT_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/tools/happytime-rtsp-client/
        ${PROJECT_SOURCE_DIR}/happytime-rtsp-client/bm
        ${PROJECT_SOURCE_DIR}/happytime-rtsp-client/http
        ${PROJECT_SOURCE_DIR}/happytime-rtsp-client/rtp
        ${PROJECT_SOURCE_DIR}/happytime-rtsp-client/rtsp
        ${PROJECT_SOURCE_DIR}/happytime-rtsp-client/media
        ${PROJECT_SOURCE_DIR}/happytime-onvif-rtsp-server/rtsp
        ${PROJECT_SOURCE_DIR}/happytime-onvif-rtsp-server/ffmpeg/include
        ${GST_INCLUDE_DIRS}
        )

target_compile_options(${PROJECT_NAME} PRIVATE ${GST_CFLAGS})

target_link_libraries(${PROJECT_NAME} PRIVATE
        Threads::Threads

        RtspClientLibrary
        ${PROJECT_SOURCE_DIR}/happytime-onvif-rtsp-server/ffmpeg/lib/linux/libavcodec.so.58.18.100
        ${PROJECT_SOURCE_DIR}/happytime-onvif-rtsp-server/ffmpeg/lib/linux/libavdevice.so.58.3.100
        ${PROJECT_SOURCE_DIR}/happytime-onvif-rtsp-server/ffmpeg/lib/linux/libavfilter.so.7.16.100
        ${PROJECT_SOURCE_DIR}/happytime-onvif-rtsp-server/ffmpeg/lib/linux/libavformat.so.58.12.100
        ${PROJECT_SOURCE_DIR}/happytime-onvif-rtsp-server/ffmpeg/lib/linux/libavutil.so.56.14.100
        ${PROJECT_SOURCE_DIR}/happytime-onvif-rtsp-server/ffmpeg/lib/linux/libopus.so.0.6.0
        ${PROJECT_SOURCE_DIR}/happytime-onvif-rtsp-server/ffmpeg/lib/linux/libswresample.so.3.1.100
        ${PROJECT_SOURCE_DIR}/happytime-onvif-rtsp-server/ffmpeg/lib/linux/libswscale.so.5.1.100
        ${PROJECT_SOURCE_DIR}/happytime-onvif-rtsp-server/ffmpeg/lib/linux/libx264.so.157
        ${PROJECT_SOURCE_DIR}/happytime-onvif-rtsp-server/ffmpeg/lib/linux/libx265.so.116
        ${GST_LIBRARIES}
        )
