################OPTION###################
OUTPUT = onvifrtspserver
CCOMPILE = gcc
CPPCOMPILE = g++
COMPILEOPTION = -g -c -O3
COMPILEOPTION += -DEPOLL
COMPILEOPTION += -DHTTPD
COMPILEOPTION += -DMEDIA_SUPPORT
COMPILEOPTION += -DIMAGE_SUPPORT
COMPILEOPTION += -DDEVICEIO_SUPPORT
COMPILEOPTION += -DMEDIA_FILE
COMPILEOPTION += -DMEDIA_DEVICE
COMPILEOPTION += -DMEDIA_PROXY
COMPILEOPTION += -DMEDIA_PUSHER
COMPILEOPTION += -DRTSP_BACKCHANNEL
COMPILEOPTION += -DRTSP_OVER_HTTP
COMPILEOPTION += -DRTSP_RTCP
COMPILEOPTION += -DRTSP_METADATA
COMPILEOPTION += -DRTSP_REPLAY
LINK = g++
LINKOPTION += -o $(OUTPUT)
INCLUDEDIR += -I./bm
INCLUDEDIR += -I./onvif
INCLUDEDIR += -I./http
INCLUDEDIR += -I./rtp
INCLUDEDIR += -I./rtsp
INCLUDEDIR += -I./media
INCLUDEDIR += -I./rtmp
INCLUDEDIR += -I./librtmp
INCLUDEDIR += -I./openssl/include
INCLUDEDIR += -I./zlib/include
INCLUDEDIR += -I./ffmpeg/include
LIBDIRS += -L./openssl/lib/linux
LIBDIRS += -L./zlib/lib/linux
LIBDIRS += -L./ffmpeg/lib/linux
OBJS += bm/word_analyse.o
OBJS += bm/util.o
OBJS += bm/sys_os.o
OBJS += bm/sys_log.o
OBJS += bm/sys_buf.o
OBJS += bm/ppstack.o
OBJS += bm/base64.o
OBJS += bm/sha1.o
OBJS += bm/linked_list.o
OBJS += bm/hqueue.o
OBJS += bm/rfc_md5.o
OBJS += bm/xml_node.o
OBJS += bm/hxml.o
OBJS += http/http_srv.o
OBJS += http/http_auth.o
OBJS += http/http_parse.o
OBJS += http/http_cln.o
OBJS += onvif/soap.o
OBJS += onvif/onvif_probe.o
OBJS += onvif/onvif_pkt.o
OBJS += onvif/onvif.o
OBJS += onvif/soap_parser.o
OBJS += onvif/onvif_device.o
OBJS += onvif/onvif_timer.o
OBJS += onvif/onvif_event.o
OBJS += onvif/onvif_srv.o
OBJS += onvif/onvif_utils.o
OBJS += onvif/onvif_cm.o
OBJS += onvif/onvif_cfg.o
OBJS += rtp/rtp_tx.o
OBJS += rtp/mjpeg_tables.o
OBJS += rtp/bit_vector.o
OBJS += rtp/mjpeg_rtp_rx.o
OBJS += rtp/rtp_rx.o
OBJS += rtp/h264_rtp_rx.o
OBJS += rtp/h265_rtp_rx.o
OBJS += rtp/aac_rtp_rx.o
OBJS += rtp/pcm_rtp_rx.o
OBJS += rtp/mpeg4_rtp_rx.o
OBJS += rtp/mpeg4.o
OBJS += rtsp/rtsp_srv.o
OBJS += rtsp/rtsp_media.o
OBJS += rtsp/rtsp_parse.o
OBJS += rtsp/rtsp_stream.o
OBJS += rtsp/rtsp_rsua.o
OBJS += rtsp/rtsp_cfg.o
OBJS += rtsp/rtsp_util.o
OBJS += rtsp/rtsp_timer.o
OBJS += rtsp/rtsp_mc.o
OBJS += media/media_util.o
OBJS += http/http_mjpeg_cln.o
OBJS += main.o 

ifneq ($(findstring HTTPD, $(COMPILEOPTION)),)
OBJS += http/httpd.o
endif

ifneq ($(findstring PROFILE_C_SUPPORT, $(COMPILEOPTION)),)
OBJS += onvif/onvif_doorcontrol.o
endif

ifneq ($(findstring PROFILE_G_SUPPORT, $(COMPILEOPTION)),)
OBJS += onvif/onvif_recording.o
endif

ifneq ($(findstring ACCESS_RULES, $(COMPILEOPTION)),)
OBJS += onvif/onvif_accessrules.o
endif

ifneq ($(findstring CREDENTIAL_SUPPORT, $(COMPILEOPTION)),)
OBJS += onvif/onvif_credential.o
endif

ifneq ($(findstring DEVICEIO_SUPPORT, $(COMPILEOPTION)),)
OBJS += onvif/onvif_deviceio.o
endif

ifneq ($(findstring MEDIA_SUPPORT, $(COMPILEOPTION)),)
OBJS += onvif/onvif_media.o
endif

ifneq ($(findstring IMAGE_SUPPORT, $(COMPILEOPTION)),)
OBJS += onvif/onvif_image.o
endif

ifneq ($(findstring SCHEDULE_SUPPORT, $(COMPILEOPTION)),)
OBJS += onvif/onvif_schedule.o
endif

ifneq ($(findstring MEDIA2_SUPPORT, $(COMPILEOPTION)),)
ifeq ($(findstring onvif_media, $(OBJS)),)
OBJS += onvif/onvif_media.o
endif
OBJS += onvif/onvif_media2.o
endif

ifneq ($(findstring PTZ_SUPPORT, $(COMPILEOPTION)),)
OBJS += onvif/onvif_ptz.o
endif

ifneq ($(findstring VIDEO_ANALYTICS, $(COMPILEOPTION)),)
OBJS += onvif/onvif_analytics.o
endif

ifneq ($(findstring THERMAL_SUPPORT, $(COMPILEOPTION)),)
OBJS += onvif/onvif_thermal.o
endif

ifneq ($(findstring RECEIVER_SUPPORT, $(COMPILEOPTION)),)
OBJS += onvif/onvif_receiver.o
endif

ifneq ($(findstring PROVISIONING_SUPPORT, $(COMPILEOPTION)),)
OBJS += onvif/onvif_provisioning.o
endif

ifneq ($(findstring MEDIA_FILE, $(COMPILEOPTION)),)
OBJS += media/file_demux.o
endif

ifneq ($(findstring MEDIA_DEVICE, $(COMPILEOPTION)),)
OBJS += media/v4l2_comm.o
OBJS += media/v4l2.o
OBJS += media/video_capture.o
OBJS += media/video_capture_linux.o
OBJS += media/audio_capture.o
OBJS += media/audio_capture_linux.o
OBJS += media/screen_capture.o
OBJS += media/screen_capture_linux.o
OBJS += media/alsa.o
endif

ffmpeg := 0

ifneq ($(findstring MEDIA_FILE, $(COMPILEOPTION)),)
ffmpeg := 1
endif

ifneq ($(findstring MEDIA_DEVICE, $(COMPILEOPTION)),)
ffmpeg := 1
endif

ifeq ($(ffmpeg), 1)
OBJS += media/audio_encoder.o
OBJS += media/audio_decoder.o
OBJS += media/video_encoder.o
OBJS += media/video_decoder.o
OBJS += media/avcodec_mutex.o
endif

ifneq ($(findstring MEDIA_PROXY, $(COMPILEOPTION)),)       
OBJS += rtsp/media_proxy.o
OBJS += rtsp/rtsp_cln.o
OBJS += rtsp/rtsp_rcua.o
OBJS += rtp/h264_util.o
OBJS += rtp/h265_util.o
endif

ifneq ($(findstring RTMP_PROXY, $(COMPILEOPTION)),)
OBJS += librtmp/amf.o
OBJS += librtmp/hashswf.o
OBJS += librtmp/log.o
OBJS += librtmp/parseurl.o
OBJS += librtmp/rtmp.o
OBJS += rtmp/rtmp_cln.o
endif

ifneq ($(findstring RTSP_BACKCHANNEL, $(COMPILEOPTION)),)
OBJS += rtsp/rtsp_srv_backchannel.o
OBJS += media/audio_play.o
OBJS += media/audio_play_linux.o
endif

ifneq ($(findstring RTSP_OVER_HTTP, $(COMPILEOPTION)),)
OBJS += rtsp/rtsp_http.o   
endif

ifneq ($(findstring MEDIA_PUSHER, $(COMPILEOPTION)),)
OBJS += rtsp/rtsp_pusher.o 
endif

ifneq ($(findstring RTSP_RTCP, $(COMPILEOPTION)),)
OBJS += rtp/rtcp.o 
endif

ifneq ($(findstring RTSP_CRYPT, $(COMPILEOPTION)),)
OBJS += rtsp/rtsp_crypt.o
endif

openssl := 0

ifneq ($(findstring HTTPS, $(COMPILEOPTION)),)
openssl := 1
endif

ifneq ($(findstring RTMP_PROXY, $(COMPILEOPTION)),)
openssl := 1
endif

ifeq ($(openssl), 1)
SHAREDLIB += -lssl
SHAREDLIB += -lcrypto
SHAREDLIB += -lz
endif

ifeq ($(findstring IOS, $(COMPILEOPTION)),)
SHAREDLIB += -lrt
endif

ifeq ($(ffmpeg), 1)
SHAREDLIB += -lavformat
SHAREDLIB += -lavcodec
SHAREDLIB += -lswresample
SHAREDLIB += -lavutil
SHAREDLIB += -lswscale
SHAREDLIB += -lx264
SHAREDLIB += -lx265
SHAREDLIB += -lopus
endif

ifneq ($(findstring MEDIA_DEVICE, $(COMPILEOPTION)),)
SHAREDLIB += -lasound
endif

SHAREDLIB += -lpthread
SHAREDLIB += -lm
SHAREDLIB += -ldl

APPENDLIB = 
PROC_OPTION = DEFINE=_PROC_ MODE=ORACLE LINES=true CODE=CPP
ESQL_OPTION = -g
################OPTION END################
ESQL = esql
PROC = proc
$(OUTPUT):$(OBJS) $(APPENDLIB)
	./mklinks.sh
	$(LINK) $(LINKOPTION) $(LIBDIRS)   $(OBJS) $(SHAREDLIB) $(APPENDLIB) 

clean: 
	rm -f $(OBJS)
	rm -f $(OUTPUT)
all: clean $(OUTPUT)
.PRECIOUS:%.cpp %.c %.C
.SUFFIXES:
.SUFFIXES:  .c .o .cpp .ecpp .pc .ec .C .cc .cxx

.cpp.o:
	$(CPPCOMPILE) -c -o $*.o $(COMPILEOPTION) $(INCLUDEDIR)  $*.cpp
	
.cc.o:
	$(CCOMPILE) -c -o $*.o $(COMPILEOPTION) $(INCLUDEDIR)  $*.cpp

.cxx.o:
	$(CPPCOMPILE) -c -o $*.o $(COMPILEOPTION) $(INCLUDEDIR)  $*.cpp

.c.o:
	$(CCOMPILE) -c -o $*.o $(COMPILEOPTION) $(INCLUDEDIR) $*.c

.C.o:
	$(CPPCOMPILE) -c -o $*.o $(COMPILEOPTION) $(INCLUDEDIR) $*.C	

.ecpp.C:
	$(ESQL) -e $(ESQL_OPTION) $(INCLUDEDIR) $*.ecpp 
	
.ec.c:
	$(ESQL) -e $(ESQL_OPTION) $(INCLUDEDIR) $*.ec
	
.pc.cpp:
	$(PROC)  CPP_SUFFIX=cpp $(PROC_OPTION)  $*.pc
