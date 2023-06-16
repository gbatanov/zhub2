cmake_minimum_required(VERSION 3.13)
project(onvifrtspserver LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

<<<<<<< HEAD
<<<<<<< HEAD
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

=======
>>>>>>> 2ffa271... Add happytime client and server
=======
<<<<<<< HEAD
>>>>>>> 2ffa271... Add happytime client and server
=======
>>>>>>> 2ffa271ef0e81a6d56c1447b17f4868d5e53f7f6
>>>>>>> VIDEO-CORE-T-1
add_compile_options(
    -DEPOLL
    -DHTTPD
    -DIMAGE_SUPPORT
    -DMEDIA_SUPPORT
    -DDEVICEIO_SUPPORT
    -DPROFILE_G_SUPPORT
    -DPROFILE_C_SUPPORT
    -DCREDENTIAL_SUPPORT
    -DACCESS_RULES
    -DSCHEDULE_SUPPORT
    -DAUDIO_SUPPORT
    -DMEDIA2_SUPPORT
    -DPTZ_SUPPORT
    -DVIDEO_ANALYTICS
    -DTHERMAL_SUPPORT
    -DRECEIVER_SUPPORT
    -DIPFILTER_SUPPORT
    -DPROVISIONING_SUPPORT
    #-DHTTPS
    #-DPROFILE_Q_SUPPORT
    -DMEDIA_FILE
    -DMEDIA_DEVICE
    -DMEDIA_PROXY
    -DRTMP_PROXY
    -DMEDIA_PUSHER
    -DRTSP_BACKCHANNEL
    -DRTSP_OVER_HTTP
    -DRTSP_RTCP
    -DRTSP_METADATA
    -DRTSP_REPLAY
    )

option(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)
find_package(ALSA REQUIRED)

<<<<<<< HEAD
<<<<<<< HEAD
=======
=======
<<<<<<< HEAD
>>>>>>> 2ffa271... Add happytime client and server
=======
>>>>>>> 2ffa271ef0e81a6d56c1447b17f4868d5e53f7f6
>>>>>>> VIDEO-CORE-T-1
find_library(AVCODEC_LIBRARY avcodec)
find_library(AVFORMAT_LIBRARY avformat)
find_library(AVUTIL_LIBRARY avutil)
find_library(AVDEVICE_LIBRARY avdevice)
find_library(SWRESAMPLE_LIBRARY  swresample)
find_library(SWSCALE_LIBRARY  swscale)
find_library(RTMP_LIBRARY  rtmp)

<<<<<<< HEAD
<<<<<<< HEAD
>>>>>>> 2ffa271... Add happytime client and server
=======
=======
>>>>>>> VIDEO-CORE-T-1
>>>>>>> 2ffa271... Add happytime client and server
=======
>>>>>>> 2ffa271ef0e81a6d56c1447b17f4868d5e53f7f6
set(SRC
    bm/base64.c
    bm/hqueue.c
    bm/hxml.c
    bm/linked_list.c
    bm/ppstack.c
    bm/rfc_md5.c
    bm/sys_buf.c
    bm/sys_log.c
    bm/sys_os.c
    bm/util.c
    bm/word_analyse.c
    bm/xml_node.c
    bm/sha1.c

    http/http_auth.c
    http/http_cln.c
    http/httpd.c
    http/http_mjpeg_cln.cpp
    http/http_parse.c
    http/http_srv.cpp

    onvif/onvif_accessrules.c
    onvif/onvif_analytics.c
    onvif/onvif.c
    onvif/onvif_cfg.c
    onvif/onvif_cm.c
    onvif/onvif_credential.c
    onvif/onvif_device.c
    onvif/onvif_deviceio.c
    onvif/onvif_doorcontrol.c
    onvif/onvif_event.c
    onvif/onvif_image.c
    onvif/onvif_media2.c
    onvif/onvif_media.c
    onvif/onvif_pkt.c
    onvif/onvif_probe.c
    onvif/onvif_provisioning.c
    onvif/onvif_ptz.c
    onvif/onvif_receiver.c
    onvif/onvif_recording.c
    onvif/onvif_schedule.c
    onvif/onvif_srv.c
    onvif/onvif_thermal.c
    onvif/onvif_timer.c
    onvif/onvif_utils.c
    onvif/soap.c
    onvif/soap_parser.c

    rtp/aac_rtp_rx.cpp
    rtp/bit_vector.cpp
    rtp/h264_rtp_rx.cpp
    rtp/h264_util.cpp
    rtp/h265_rtp_rx.cpp
    rtp/h265_util.cpp
    rtp/mjpeg_rtp_rx.cpp
    rtp/mjpeg_tables.cpp
    rtp/mpeg4.cpp
    rtp/mpeg4_rtp_rx.cpp
    rtp/pcm_rtp_rx.cpp
    rtp/rtcp.cpp
    rtp/rtp_rx.cpp
    rtp/rtp_tx.cpp


    rtsp/media_proxy.cpp
    rtsp/rtsp_backchannel.cpp
    rtsp/rtsp_cfg.cpp
    rtsp/rtsp_cln.cpp
    rtsp/rtsp_crypt.cpp
    rtsp/rtsp_http.cpp
    rtsp/rtsp_mc.cpp
    rtsp/rtsp_media.cpp
    rtsp/rtsp_parse.cpp
    rtsp/rtsp_pusher.cpp
    rtsp/rtsp_rcua.cpp
    rtsp/rtsp_rsua.cpp
    rtsp/rtsp_srv_backchannel.cpp
    rtsp/rtsp_srv.cpp
    rtsp/rtsp_stream.cpp
    rtsp/rtsp_timer.cpp
    rtsp/rtsp_util.cpp

    rtmp/rtmp_cln.cpp

    media/alsa.cpp
    media/audio_capture.cpp
    media/audio_capture_linux.cpp
    media/audio_decoder.cpp
    media/audio_encoder.cpp
    media/audio_play.cpp
    media/audio_play_linux.cpp
    media/avcodec_mutex.cpp
    media/file_demux.cpp
    media/live_audio.cpp
    media/live_video.cpp
    media/media_util.cpp
    media/screen_capture.cpp
    media/screen_capture_linux.cpp
    media/v4l2_comm.cpp
    media/v4l2.cpp
    media/video_capture.cpp
    media/video_capture_linux.cpp
    media/video_decoder.cpp
    media/video_encoder.cpp

    main.cpp
    )

<<<<<<< HEAD
<<<<<<< HEAD
add_subdirectory(${PROJECT_SOURCE_DIR}/librtmp)

add_executable(${PROJECT_NAME} ${SRC})

target_include_directories(${PROJECT_NAME} PRIVATE  ${PROJECT_SOURCE_DIR}/
    ${PROJECT_SOURCE_DIR}/librtmp
=======
add_executable(${PROJECT_NAME} ${SRC})

target_include_directories(${PROJECT_NAME} PRIVATE  ${PROJECT_SOURCE_DIR}/
>>>>>>> 2ffa271... Add happytime client and server
=======
add_executable(${PROJECT_NAME} ${SRC})

target_include_directories(${PROJECT_NAME} PRIVATE  ${PROJECT_SOURCE_DIR}/
<<<<<<< HEAD
>>>>>>> 2ffa271... Add happytime client and server
=======
>>>>>>> 2ffa271ef0e81a6d56c1447b17f4868d5e53f7f6
>>>>>>> VIDEO-CORE-T-1
    ${PROJECT_SOURCE_DIR}/bm
    ${PROJECT_SOURCE_DIR}/onvif
    ${PROJECT_SOURCE_DIR}/http
    ${PROJECT_SOURCE_DIR}/rtp
    ${PROJECT_SOURCE_DIR}/rtsp
    ${PROJECT_SOURCE_DIR}/media
    ${PROJECT_SOURCE_DIR}/rtmp
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
=======
    ${PROJECT_SOURCE_DIR}/librtmp
>>>>>>> 2ffa271... Add happytime client and server
=======
>>>>>>> VIDEO-CORE-T-1
=======
    ${PROJECT_SOURCE_DIR}/librtmp
>>>>>>> 2ffa271... Add happytime client and server
=======
    ${PROJECT_SOURCE_DIR}/librtmp
>>>>>>> 2ffa271ef0e81a6d56c1447b17f4868d5e53f7f6
    ${PROJECT_SOURCE_DIR}/openssl/include
    ${PROJECT_SOURCE_DIR}/zlib/include
    ${PROJECT_SOURCE_DIR}/ffmpeg/include

    ${ALSA_INCLUDE_DIRS}
    )

target_link_libraries(${PROJECT_NAME} PRIVATE
<<<<<<< HEAD
<<<<<<< HEAD
    rtmp

    Threads::Threads
    ${PROJECT_SOURCE_DIR}/zlib/lib/linux/libz.so.1.2.11
    ${PROJECT_SOURCE_DIR}/ffmpeg/lib/linux/libavcodec.so.58.18.100
    ${PROJECT_SOURCE_DIR}/ffmpeg/lib/linux/libavdevice.so.58.3.100
    ${PROJECT_SOURCE_DIR}/ffmpeg/lib/linux/libavfilter.so.7.16.100
    ${PROJECT_SOURCE_DIR}/ffmpeg/lib/linux/libavformat.so.58.12.100
    ${PROJECT_SOURCE_DIR}/ffmpeg/lib/linux/libavutil.so.56.14.100
    ${PROJECT_SOURCE_DIR}/ffmpeg/lib/linux/libopus.so.0.6.0
    ${PROJECT_SOURCE_DIR}/ffmpeg/lib/linux/libpostproc.so.55.1.100
    ${PROJECT_SOURCE_DIR}/ffmpeg/lib/linux/libswresample.so.3.1.100
    ${PROJECT_SOURCE_DIR}/ffmpeg/lib/linux/libswscale.so.5.1.100
    ${PROJECT_SOURCE_DIR}/ffmpeg/lib/linux/libx264.so.157
    ${PROJECT_SOURCE_DIR}/ffmpeg/lib/linux/libx265.so.116

=======
=======
<<<<<<< HEAD
>>>>>>> 2ffa271... Add happytime client and server
=======
>>>>>>> 2ffa271ef0e81a6d56c1447b17f4868d5e53f7f6
>>>>>>> VIDEO-CORE-T-1
    Threads::Threads
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${AVDEVICE_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
    ${SWSCALE_LIBRARY}
    ${RTMP_LIBRARY}
<<<<<<< HEAD
<<<<<<< HEAD
>>>>>>> 2ffa271... Add happytime client and server
=======
=======
>>>>>>> VIDEO-CORE-T-1
>>>>>>> 2ffa271... Add happytime client and server
=======
>>>>>>> 2ffa271ef0e81a6d56c1447b17f4868d5e53f7f6
    ${ALSA_LIBRARIES}
    )
