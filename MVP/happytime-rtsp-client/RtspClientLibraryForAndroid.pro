#-------------------------------------------------
#
# Project created by QtCreator 2014-02-17T15:51:28
#
#-------------------------------------------------

QT -= core gui
TARGET = RtspClientLibrary
TEMPLATE = lib

#If compiling the dynamic library, please comment out this line
CONFIG += staticlib

DEFINES += ANDROID
DEFINES += METADATA
DEFINES += REPLAY
DEFINES += OVER_HTTP

INCLUDEPATH += ./bm
INCLUDEPATH += ./http
INCLUDEPATH += ./
INCLUDEPATH += ./rtp
INCLUDEPATH += ./rtsp
INCLUDEPATH +=  ./media

SOURCES	+= \
    bm/word_analyse.cpp \
    bm/util.cpp \
    bm/sys_os.cpp \
    bm/sys_log.cpp \
    bm/sys_buf.cpp \
    bm/ppstack.cpp \
    bm/base64.cpp \
    bm/rfc_md5.cpp \
    bm/hqueue.cpp \
    http/http_cln.cpp \
    http/http_parse.cpp \
    rtp/aac_rtp_rx.cpp \
    rtp/bit_vector.cpp \
    rtp/h264_rtp_rx.cpp \
    rtp/h265_rtp_rx.cpp \
    rtp/mjpeg_rtp_rx.cpp \
    rtp/mjpeg_tables.cpp \
    rtp/mpeg4.cpp \
    rtp/mpeg4_rtp_rx.cpp \
    rtp/pcm_rtp_rx.cpp \
    rtp/rtp_rx.cpp \
    rtsp/rtsp_cln.cpp \
    rtsp/rtsp_parse.cpp \
    rtsp/rtsp_rcua.cpp \
    rtsp/rtsp_util.cpp

HEADERS += \
    bm/sys_inc.h \
    bm/word_analyse.h \
    bm/util.h \
    bm/sys_os.h \
    bm/sys_log.h \
    bm/sys_buf.h \
    bm/ppstack.h \
    bm/base64.h \
    bm/rfc_md5.h \
    bm/hqueue.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}


