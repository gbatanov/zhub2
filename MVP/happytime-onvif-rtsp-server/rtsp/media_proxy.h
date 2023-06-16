/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2020, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#ifndef MEDIA_PROXY_H
#define MEDIA_PROXY_H

#include "rtsp_cln.h"
#include "http_mjpeg_cln.h"
#include "linked_list.h"
#include "hqueue.h"
#include "rtsp_comm_cfg.h"

#ifdef RTMP_PROXY
#include "rtmp_cln.h"
#endif

#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)
#include "video_decoder.h"
#include "audio_decoder.h"
#include "video_encoder.h"
#include "audio_encoder.h"
#endif

class CMediaProxy;

typedef struct
{
    uint32  has_output : 1;
    uint32  reserved   : 31;
    
    char    suffix[100];
    char    url[256];
    char    user[32];
    char    pass[32];

    RTSP_OUTPUT output;
} PROXY_CFG;

typedef struct _MEDIA_PRY
{
    struct _MEDIA_PRY * next;
    
    PROXY_CFG    cfg;
    
    CMediaProxy * proxy;
} MEDIA_PRY;

typedef void (*ProxyDataCB)(uint8 * data, int size, int type, void * pUserdata);

typedef struct
{
	ProxyDataCB pCallback;
	void *      pUserdata;
	BOOL        bVFirst;
	BOOL        bAFirst;
} ProxyCB;


#ifdef __cplusplus
extern "C" {
#endif

MEDIA_PRY  * rtsp_add_proxy(MEDIA_PRY ** p_proxy);
void         rtsp_free_proxies(MEDIA_PRY ** p_proxy);
BOOL         rtsp_init_proxy(MEDIA_PRY * p_proxy);
void         rtsp_init_proxies();
int          rtsp_get_proxy_nums();
MEDIA_PRY  * rtsp_proxy_match(const char * suffix);

#ifdef __cplusplus
}
#endif

class CMediaProxy 
{
public:
    CMediaProxy(PROXY_CFG * pConfig);
    ~CMediaProxy(void);

public:
    void    onNotify(int evt);
    void    onAudio(uint8 * pdata, int len, uint32 ts, uint16 seq);
    void    onVideo(uint8 * pdata, int len, uint32 ts, uint16 seq);
    void    notifyHandler();

    BOOL    startConn(const char * url, char * user, char * pass);
    
    char *  getVideoAuxSDPLine(int rtp_pt);
    char *  getAudioAuxSDPLine(int rtp_pt);

    void    addCallback(ProxyDataCB pCallback, void * pUserdata);
    void    delCallback(ProxyDataCB pCallback, void * pUserdata);
    void    dataCallback(uint8 * data, int size, int type);
    
#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)    
    int     getVideoRecodec() {return m_nVideoRecodec;}
    int     getAudioRecodec() {return m_nAudioRecodec;}
    int     needVideoRecodec(uint8 * p_data, int len);
    int     needAudioRecodec();
    void    videoDataDecode(uint8 * p_data, int len);
    void    videoDataEncode(AVFrame * frame);
    void    audioDataDecode(uint8 * p_data, int len);
    void    audioDataEncode(AVFrame * frame);
#endif

private:
    void    freeConn();
    BOOL    restartConn();
    void    clearNotify();    
    BOOL    isCallbackExist(ProxyDataCB pCallback, void * pUserdata);
    char *  getH264AuxSDPLine(int rtp_pt);
    char *  getH265AuxSDPLine(int rtp_pt);
    char *  getMP4AuxSDPLine(int rtp_pt);
    char *  getAACAuxSDPLine(int rtp_pt);
    int     parseVideoSize(uint8 * pdata, int len);

#ifdef RTMP_PROXY
    char *  getRTMPH264AuxSDPLine(int rtp_pt);
    char *  getRTMPAACAuxSDPLine(int rtp_pt);
#endif
    
public:
    uint32	        has_audio	: 1;
	uint32	        has_video	: 1;
	uint32	        inited      : 1;
	uint32          exiting     : 1;
	uint32	        reserved	: 28;

	PROXY_CFG     * m_pConfig;
	
    int             v_codec;
    int             v_width;
    int             v_height;
    
	int	            a_codec;
	int             a_samplerate;
	int             a_channels;

    char            m_url[500];
    char            m_user[32];
    char            m_pass[32];
    int             m_reconnto;
    
	CRtspClient   * m_rtsp;
	CHttpMjpeg    * m_mjpeg;
#ifdef RTMP_PROXY
    CRtmpClient   * m_rtmp;
#endif
	    
    HQUEUE        * m_notifyQueue;
    pthread_t       m_hNotify;
    
	void          * m_pCallbackMutex;
	LINKED_LIST   * m_pCallbackList;

#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)
    int             m_nVideoRecodec;    // 1  - need; 0  - Still not sure; -1 - not need
    int             m_nAudioRecodec;    // 1  - need; 0  - Still not sure; -1 - not need
    
    CVideoDecoder * m_pVideoDecoder;    // video decoder
    CAudioDecoder * m_pAudioDecoder;    // audio decoder
	CVideoEncoder * m_pVideoEncoder;    // video encoder  
	CAudioEncoder * m_pAudioEncoder;    // audio encoder
#endif	
};

#endif


