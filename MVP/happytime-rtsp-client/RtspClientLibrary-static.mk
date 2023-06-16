################OPTION###################
OUTPUT = libRtspClientLibrary.a
CCOMPILE = gcc
CPPCOMPILE = g++
COMPILEOPTION += -c -O3 -fPIC
COMPILEOPTION += -DMETADATA
COMPILEOPTION += -DREPLAY
COMPILEOPTION += -DOVER_HTTP
LINK = ar
LINKOPTION = cr $(OUTPUT)
INCLUDEDIR += -I.
INCLUDEDIR += -I./bm
INCLUDEDIR += -I./http
INCLUDEDIR += -I./rtp
INCLUDEDIR += -I./rtsp
INCLUDEDIR += -I./media
LIBDIRS = 
OBJS += bm/word_analyse.o
OBJS += bm/util.o
OBJS += bm/sys_os.o
OBJS += bm/sys_log.o
OBJS += bm/sys_buf.o
OBJS += bm/ppstack.o
OBJS += bm/base64.o
OBJS += bm/rfc_md5.o
OBJS += bm/hqueue.o
OBJS += rtp/aac_rtp_rx.o
OBJS += rtp/bit_vector.o
OBJS += rtp/h264_rtp_rx.o
OBJS += rtp/h265_rtp_rx.o
OBJS += rtp/mjpeg_rtp_rx.o
OBJS += rtp/mjpeg_tables.o
OBJS += rtp/mpeg4.o
OBJS += rtp/mpeg4_rtp_rx.o
OBJS += rtp/pcm_rtp_rx.o
OBJS += rtp/rtp_rx.o
OBJS += rtsp/rtsp_cln.o
OBJS += rtsp/rtsp_parse.o
OBJS += rtsp/rtsp_rcua.o
OBJS += rtsp/rtsp_util.o

ifneq ($(findstring OVER_HTTP, $(COMPILEOPTION)),)
OBJS += http/http_cln.o
OBJS += http/http_parse.o
endif

ifneq ($(findstring BACKCHANNEL, $(COMPILEOPTION)),)
OBJS += rtsp/rtsp_backchannel.o
endif

SHAREDLIB =
APPENDLIB = 
PROC_OPTION = DEFINE=_PROC_ MODE=ORACLE LINES=true CODE=CPP
ESQL_OPTION = -g
################OPTION END################
ESQL = esql
PROC = proc
$(OUTPUT):$(OBJS) $(APPENDLIB)
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
