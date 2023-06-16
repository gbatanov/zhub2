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

#include "sys_inc.h"
#include "media_proxy.h"
#include "rtsp_cfg.h"
#include "media_format.h"
#include "h264_util.h"
#include "h265_util.h"
#include "bit_vector.h"
#include "media_util.h"
#include "mjpeg.h"
#include "base64.h"
#include <math.h>

#ifdef MEDIA_PROXY

int event_notify_cb(int evt, void * puser)
{
    CMediaProxy * p_proxy = (CMediaProxy *) puser;

    p_proxy->onNotify(evt);
    return 0;
}

int rtsp_audio_cb(uint8 * pdata, int len, uint32 ts, uint16 seq, void *puser)
{
    CMediaProxy * p_proxy = (CMediaProxy *) puser;

    p_proxy->onAudio(pdata, len, ts, seq);
    return 0;
}

int rtsp_video_cb(uint8 * pdata, int len, uint32 ts, uint16 seq, void *puser)
{
    CMediaProxy * p_proxy = (CMediaProxy *) puser;

    p_proxy->onVideo(pdata, len, ts, seq);
    return 0;
}

int jpeg_video_cb(uint8 * pdata, int len, void *puser)
{
    CMediaProxy * p_proxy = (CMediaProxy *) puser;

    p_proxy->onVideo(pdata, len, 0, 0);
    return 0;
}

#ifdef RTMP_PROXY

int rtmp_video_data_cb(uint8 * pdata, int len, uint32 ts, void *puser)
{
    CMediaProxy * p_proxy = (CMediaProxy *) puser;

    p_proxy->onVideo(pdata, len, ts, 0);
    return 0;
}

int rtmp_audio_data_cb(uint8 * pdata, int len, uint32 ts, void *puser)
{
    CMediaProxy * p_proxy = (CMediaProxy *) puser;

    p_proxy->onAudio(pdata, len, ts, 0);
    return 0;
}

#endif // RTMP_PROXY

void * notifyThread(void * argv)
{
    CMediaProxy * p_proxy = (CMediaProxy *) argv;

    p_proxy->notifyHandler();
    return NULL;
}

/*************************************************************/

MEDIA_PRY * rtsp_add_proxy(MEDIA_PRY ** p_proxy)
{
	MEDIA_PRY * p_tmp;
	MEDIA_PRY * p_new_proxy = (MEDIA_PRY *) malloc(sizeof(MEDIA_PRY));
	if (NULL == p_new_proxy)
	{
		return NULL;
	}

	memset(p_new_proxy, 0, sizeof(MEDIA_PRY));

	p_tmp = *p_proxy;
	if (NULL == p_tmp)
	{
		*p_proxy = p_new_proxy;
	}
	else
	{
		while (p_tmp && p_tmp->next)
		{
			p_tmp = p_tmp->next;
		}
		
		p_tmp->next = p_new_proxy;
	}

	return p_new_proxy;
}

void rtsp_free_proxies(MEDIA_PRY ** p_proxy)
{
	MEDIA_PRY * p_next;
	MEDIA_PRY * p_tmp = *p_proxy;

	while (p_tmp)
	{
		p_next = p_tmp->next;
		
		if (p_tmp->proxy)
		{
		    delete p_tmp->proxy;
		    p_tmp->proxy = NULL;
		}
		
		free(p_tmp);
		p_tmp = p_next;
	}

	*p_proxy = NULL;
}

BOOL rtsp_init_proxy(MEDIA_PRY * p_proxy)
{
    BOOL ret = FALSE;
    
    p_proxy->proxy = new CMediaProxy(&p_proxy->cfg);
    if (p_proxy->proxy)
    {
        ret = p_proxy->proxy->startConn(p_proxy->cfg.url, p_proxy->cfg.user, p_proxy->cfg.pass);
    }
    else
    {
        log_print(HT_LOG_ERR, "%s, new rtsp proxy failed\r\n", __FUNCTION__);
    }
    
    return ret;
}

void rtsp_init_proxies()
{
    MEDIA_PRY * p_proxy = g_rtsp_cfg.proxy;
    while (p_proxy)
    {
        rtsp_init_proxy(p_proxy);
        
        p_proxy = p_proxy->next;
    }
}

int rtsp_get_proxy_nums()
{
    int nums = 0;
    
    MEDIA_PRY * p_proxy = g_rtsp_cfg.proxy;
    while (p_proxy)
    {
        nums++;
        
        p_proxy = p_proxy->next;
    }

    return nums;
}

MEDIA_PRY * rtsp_proxy_match(const char * suffix)
{
    MEDIA_PRY * p_proxy = g_rtsp_cfg.proxy;
    while (p_proxy)
    {
        if (strcmp(suffix, p_proxy->cfg.suffix) == 0)
        {
            return p_proxy;
        }
        
        p_proxy = p_proxy->next;
    }
    
    return NULL;
}

#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)

void media_proxy_vdecoder_cb(AVFrame * frame, void *pUserdata)
{
    CMediaProxy * pProxy = (CMediaProxy *)pUserdata;

    pProxy->videoDataEncode(frame);
}

void media_proxy_vencoder_cb(uint8 *data, int size, void *pUserdata)
{
	CMediaProxy * pProxy = (CMediaProxy *)pUserdata;

	pProxy->dataCallback(data, size, DATA_TYPE_VIDEO);
}

void media_proxy_adecoder_cb(AVFrame * frame, void *pUserdata)
{
    CMediaProxy * pProxy = (CMediaProxy *)pUserdata;

    pProxy->audioDataEncode(frame);
}

void meida_proxy_aencoder_cb(uint8 *data, int size, int nbsamples, void *pUserdata)
{
	CMediaProxy * pProxy = (CMediaProxy *)pUserdata;

	pProxy->dataCallback(data, size, DATA_TYPE_AUDIO);
}

#endif

/*************************************************************/
CMediaProxy::CMediaProxy(PROXY_CFG * pConfig)
: has_audio(0)
, has_video(0)
, inited(0)
, exiting(0)
, m_pConfig(NULL)
, v_codec(VIDEO_CODEC_NONE)
, v_width(0)
, v_height(0)
, a_codec(AUDIO_CODEC_NONE)
, a_samplerate(0)
, a_channels(0)
, m_reconnto(1)
, m_rtsp(NULL)
, m_mjpeg(NULL)
#ifdef RTMP_PROXY
, m_rtmp(NULL)
#endif
, m_notifyQueue(NULL)
, m_hNotify(0)
{
    memset(m_url, 0, sizeof(m_url));
    memset(m_user, 0, sizeof(m_user));
    memset(m_pass, 0, sizeof(m_pass));

    if (pConfig)
    {
        m_pConfig = (PROXY_CFG *) malloc(sizeof(PROXY_CFG));
        memcpy(m_pConfig, pConfig, sizeof(PROXY_CFG));
    }
    
	m_pCallbackMutex = sys_os_create_mutex();
    m_pCallbackList = h_list_create(FALSE);

    m_notifyQueue = hqCreate(10, sizeof(int), HQ_GET_WAIT | HQ_PUT_WAIT);
    m_hNotify = sys_os_create_thread((void *) notifyThread, this);

#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)
    m_nVideoRecodec = 0;
    m_nAudioRecodec = 0;
    m_pVideoDecoder = NULL;
    m_pAudioDecoder = NULL;
    m_pVideoEncoder = NULL;
	m_pAudioEncoder = NULL;
#endif    
}

CMediaProxy::~CMediaProxy(void)
{
    exiting = 1;

    freeConn();

    int evt = -1;

    hqBufPut(m_notifyQueue, (char *)&evt);

    while (m_hNotify)
    {
        usleep(20*1000);
    }

    hqDelete(m_notifyQueue);

#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)
    if (m_pVideoDecoder)
    {
        delete m_pVideoDecoder;
        m_pVideoDecoder = NULL;
    }

    if (m_pAudioDecoder)
    {
        delete m_pAudioDecoder;
        m_pAudioDecoder = NULL;
    }

    if (m_pVideoEncoder)
    {
        delete m_pVideoEncoder;
        m_pVideoEncoder = NULL;
    }

    if (m_pAudioEncoder)
    {
        delete m_pAudioEncoder;
        m_pAudioEncoder = NULL;
    }
#endif

    if (m_pConfig)
    {
        free(m_pConfig);
        m_pConfig = NULL;
    }
    
    h_list_free_container(m_pCallbackList);
    
    sys_os_destroy_sig_mutex(m_pCallbackMutex);
}

void CMediaProxy::freeConn()
{
    if (m_rtsp)
    {
        // stop rtsp connection
    	m_rtsp->rtsp_stop();
    	m_rtsp->rtsp_close();
	
        delete m_rtsp;
        m_rtsp = NULL;
    }

    if (m_mjpeg)
    {
        delete m_mjpeg;
        m_mjpeg = NULL;
    }

#ifdef RTMP_PROXY
    if (m_rtmp)
    {
        // stop rtmp connection
    	m_rtmp->rtmp_stop();
    	m_rtmp->rtmp_close();
	
        delete m_rtmp;
        m_rtmp = NULL;
    }
#endif
}

BOOL CMediaProxy::startConn(const char * url, char * user, char * pass)
{
    if (memcmp(url, "rtsp://", 7) == 0)
    {
        m_rtsp = new CRtspClient;
        if (NULL == m_rtsp)
        {
            log_print(HT_LOG_ERR, "%s, new rtsp failed\r\n", __FUNCTION__);
            return FALSE;
        }

        strncpy(m_url, url, sizeof(m_url)-1);
        strncpy(m_user, user, sizeof(m_user)-1);
        strncpy(m_pass, pass, sizeof(m_pass)-1);

        m_rtsp->set_notify_cb(event_notify_cb, this);
        m_rtsp->set_video_cb(rtsp_video_cb);
        m_rtsp->set_audio_cb(rtsp_audio_cb);
        
        return m_rtsp->rtsp_start(url, user, pass);
    }
    else if (memcmp(url, "http://", 7) == 0 || memcmp(url, "https://", 8) == 0)
    {        
        m_mjpeg = new CHttpMjpeg;
        if (NULL == m_mjpeg)
        {
            log_print(HT_LOG_ERR, "%s, new httpmjpeg failed\r\n", __FUNCTION__);
            return FALSE;
        }

        strncpy(m_url, url, sizeof(m_url)-1);
        strncpy(m_user, user, sizeof(m_user)-1);
        strncpy(m_pass, pass, sizeof(m_pass)-1);

        m_mjpeg->set_notify_cb(event_notify_cb, this);
        m_mjpeg->set_video_cb(jpeg_video_cb);
        
        return m_mjpeg->mjpeg_start(url, user, pass);
    }
#ifdef RTMP_PROXY    
    else if (memcmp(url, "rtmp://", 7) == 0 || memcmp(url, "rtmpt://", 8) == 0 ||
        memcmp(url, "rtmps://", 8) == 0 || memcmp(url, "rtmpe://", 8) == 0 ||
        memcmp(url, "rtmpfp://", 9) == 0 || memcmp(url, "rtmpte://", 9) == 0 ||
        memcmp(url, "rtmpts://", 9) == 0)
    {
        m_rtmp = new CRtmpClient;
        if (NULL == m_rtmp)
        {
            log_print(HT_LOG_ERR, "%s, new rtmp client failed\r\n", __FUNCTION__);
            return FALSE;
        }

        strncpy(m_url, url, sizeof(m_url)-1);
        strncpy(m_user, user, sizeof(m_user)-1);
        strncpy(m_pass, pass, sizeof(m_pass)-1);

        m_rtmp->set_notify_cb(event_notify_cb, this);
        m_rtmp->set_video_cb(rtmp_video_data_cb);
        m_rtmp->set_audio_cb(rtmp_audio_data_cb);
        
        return m_rtmp->rtmp_start(url, user, pass);
    }
#endif

	return FALSE;
}

BOOL CMediaProxy::restartConn()
{
    freeConn();

    if (memcmp(m_url, "rtsp://", 7) == 0)
    {
        m_rtsp = new CRtspClient;
        if (NULL == m_rtsp)
        {
            log_print(HT_LOG_ERR, "%s, new rtsp failed\r\n", __FUNCTION__);
            return FALSE;
        }

        m_rtsp->set_notify_cb(event_notify_cb, this);
        m_rtsp->set_video_cb(rtsp_video_cb);
        m_rtsp->set_audio_cb(rtsp_audio_cb);
        
        return m_rtsp->rtsp_start(m_url, m_user, m_pass);
    }
    else if (memcmp(m_url, "http://", 7) == 0 || memcmp(m_url, "https://", 8) == 0)
    {        
        m_mjpeg = new CHttpMjpeg;
        if (NULL == m_mjpeg)
        {
            log_print(HT_LOG_ERR, "%s, new httpmjpeg failed\r\n", __FUNCTION__);
            return FALSE;
        }

        m_mjpeg->set_notify_cb(event_notify_cb, this);
        m_mjpeg->set_video_cb(jpeg_video_cb);
        
        return m_mjpeg->mjpeg_start(m_url, m_user, m_pass);
    }
#ifdef RTMP_PROXY
    else if (memcmp(m_url, "rtmp://", 7) == 0 || memcmp(m_url, "rtmpt://", 8) == 0 ||
        memcmp(m_url, "rtmps://", 8) == 0 || memcmp(m_url, "rtmpe://", 8) == 0 ||
        memcmp(m_url, "rtmpfp://", 9) == 0 || memcmp(m_url, "rtmpte://", 9) == 0 ||
        memcmp(m_url, "rtmpts://", 9) == 0)
    {
        m_rtmp = new CRtmpClient;
        if (NULL == m_rtmp)
        {
            log_print(HT_LOG_ERR, "%s, new rtmp client failed\r\n", __FUNCTION__);
            return FALSE;
        }

        m_rtmp->set_notify_cb(event_notify_cb, this);
        m_rtmp->set_video_cb(rtmp_video_data_cb);
        m_rtmp->set_audio_cb(rtmp_audio_data_cb);
        
        return m_rtmp->rtmp_start(m_url, m_user, m_pass);
    }
#endif

	return FALSE;
}

void CMediaProxy::onNotify(int evt)
{
	if (evt == RTSP_EVE_CONNSUCC)
	{
        v_codec = m_rtsp->video_codec();
        a_codec = m_rtsp->audio_codec();

        if (v_codec != VIDEO_CODEC_NONE)
        {
            has_video = 1;
        }

        if (a_codec != AUDIO_CODEC_NONE)
        {
            has_audio = 1;
            a_samplerate = m_rtsp->get_audio_samplerate();
            a_channels = m_rtsp->get_audio_channels();
        }
        
        inited = 1;
	}
	else if (evt == MJPEG_EVE_CONNSUCC)
	{
	    v_codec = VIDEO_CODEC_JPEG;
	    has_video = 1;

	    inited = 1;
	}
#ifdef RTMP_PROXY
    else if (evt == RTMP_EVE_VIDEOREADY)
	{
        v_codec = m_rtmp->video_codec();
        if (v_codec != VIDEO_CODEC_NONE)
        {
            has_video = 1;
        }
        
        inited = 1;
	}
	else if (evt == RTMP_EVE_AUDIOREADY)
	{
        a_codec = m_rtmp->audio_codec();
        if (a_codec != AUDIO_CODEC_NONE)
        {
            has_audio = 1;
            a_samplerate = m_rtmp->get_audio_samplerate();
            a_channels = m_rtmp->get_audio_channels();
        }
        
        inited = 1;
	}
#endif
	
	hqBufPut(m_notifyQueue, (char *)&evt);
}

void CMediaProxy::onVideo(uint8 * pdata, int len, uint32 ts, uint16 seq)
{
    if (v_width == 0 || v_height == 0)
    {
        parseVideoSize(pdata, len);
    }

#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)
    int recodec = needVideoRecodec(pdata, len);
    if (1 == recodec) // need recodec
    {
        videoDataDecode(pdata, len);
    }
    else if (-1 == recodec) // not need recodec
    {
        dataCallback(pdata, len, DATA_TYPE_VIDEO);;
    }
    else if (0 == recodec)  // still not sure
    {
    }
#else
    dataCallback(pdata, len, DATA_TYPE_VIDEO);
#endif	
}

void CMediaProxy::onAudio(uint8 * pdata, int len, uint32 ts, uint16 seq)
{    
#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)
    int recodec = needAudioRecodec();
    if (1 == recodec) // need recodec
    {
        audioDataDecode(pdata, len);
    }
    else if (-1 == recodec) // not need recodec
    {
        dataCallback(pdata, len, DATA_TYPE_AUDIO);
    }
    else if (0 == recodec)  // still not sure
    {
    }
#else
    dataCallback(pdata, len, DATA_TYPE_AUDIO);
#endif
}

void CMediaProxy::clearNotify()
{
	int evt;
	
	while (!hqBufIsEmpty(m_notifyQueue))
	{
		hqBufGet(m_notifyQueue, (char *)&evt);
	}
}

void CMediaProxy::notifyHandler()
{
    int evt;
    
    while (!exiting && hqBufGet(m_notifyQueue, (char *)&evt))
    {
        log_print(HT_LOG_DBG, "%s, evt = %d\r\n", __FUNCTION__, evt);
        
        if (evt < 0)
        {
            break;
        }

        if (   evt == RTSP_EVE_CONNFAIL || evt == RTSP_EVE_NOSIGNAL
            || evt == RTSP_EVE_NODATA || evt == RTSP_EVE_STOPPED 
            || evt == MJPEG_EVE_CONNFAIL || evt == MJPEG_EVE_NOSIGNAL
            || evt == MJPEG_EVE_NODATA || evt == MJPEG_EVE_STOPPED
#ifdef RTMP_PROXY            
            || evt == RTMP_EVE_CONNFAIL || evt == RTMP_EVE_NOSIGNAL
            || evt == RTMP_EVE_NODATA || evt == RTMP_EVE_STOPPED
#endif            
            )
    	{
    	    if (m_reconnto < 300)
    	    {
    	        m_reconnto *= 2;
    	    }
    	    
    	    sleep(m_reconnto);

    	    clearNotify();
    	    
    	    restartConn();
    	}
    }
    
    m_hNotify = 0;
}

BOOL CMediaProxy::isCallbackExist(ProxyDataCB pCallback, void * pUserdata)
{
	BOOL exist = FALSE;
	ProxyCB * p_cb = NULL;
	LINKED_NODE * p_node = NULL;
	
	sys_os_mutex_enter(m_pCallbackMutex);

	p_node = h_list_lookup_start(m_pCallbackList);
	while (p_node)
	{
		p_cb = (ProxyCB *) p_node->p_data;
		if (p_cb->pCallback == pCallback && p_cb->pUserdata == pUserdata)
		{
			exist = TRUE;
			break;
		}
		
		p_node = h_list_lookup_next(m_pCallbackList, p_node);
	}
	h_list_lookup_end(m_pCallbackList);
	
	sys_os_mutex_leave(m_pCallbackMutex);

	return exist;
}

void CMediaProxy::addCallback(ProxyDataCB pCallback, void * pUserdata)
{
	if (isCallbackExist(pCallback, pUserdata))
	{
		return;
	}
	
	ProxyCB * p_cb = (ProxyCB *) malloc(sizeof(ProxyCB));

	p_cb->pCallback = pCallback;
	p_cb->pUserdata = pUserdata;
	p_cb->bVFirst = TRUE;
	p_cb->bAFirst = TRUE;

	sys_os_mutex_enter(m_pCallbackMutex);
	h_list_add_at_back(m_pCallbackList, p_cb);	
	sys_os_mutex_leave(m_pCallbackMutex);
}

void CMediaProxy::delCallback(ProxyDataCB pCallback, void * pUserdata)
{
	ProxyCB * p_cb = NULL;
	LINKED_NODE * p_node = NULL;
	
	sys_os_mutex_enter(m_pCallbackMutex);

	p_node = h_list_lookup_start(m_pCallbackList);
	while (p_node)
	{
		p_cb = (ProxyCB *) p_node->p_data;
		if (p_cb->pCallback == pCallback && p_cb->pUserdata == pUserdata)
		{		
			free(p_cb);
			
			h_list_remove(m_pCallbackList, p_node);
			break;
		}
		
		p_node = h_list_lookup_next(m_pCallbackList, p_node);
	}
	h_list_lookup_end(m_pCallbackList);

	sys_os_mutex_leave(m_pCallbackMutex);
}

void CMediaProxy::dataCallback(uint8 * data, int size, int type)
{
	ProxyCB * p_cb = NULL;
	LINKED_NODE * p_node = NULL;
	
	sys_os_mutex_enter(m_pCallbackMutex);

	p_node = h_list_lookup_start(m_pCallbackList);
	while (p_node)
	{
		p_cb = (ProxyCB *) p_node->p_data;
		if (p_cb->pCallback != NULL)
		{
			if (p_cb->bVFirst && type == DATA_TYPE_VIDEO)
			{
				p_cb->bVFirst = FALSE;

#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)			
                if (NULL == m_pVideoEncoder)
#endif
                {
    				if (v_codec == VIDEO_CODEC_H264)
    				{
    					uint8 sps[1024], pps[1024];
    					int sps_len = 0, pps_len = 0;
    				
    					if (m_rtsp && m_rtsp->get_h264_params(sps, &sps_len, pps, &pps_len))
    					{
    					    if (sps_len > 0)
    					    {
    						    p_cb->pCallback(sps, sps_len, type, p_cb->pUserdata);
    						}
    						
    						if (pps_len > 0)
    						{
    						    p_cb->pCallback(pps, pps_len, type, p_cb->pUserdata);
    						}
    					}
#ifdef RTMP_PROXY    					
    					else if (m_rtmp && m_rtmp->get_h264_params(sps, &sps_len, pps, &pps_len))
    					{
    					    if (sps_len > 0)
    					    {
    					        p_cb->pCallback(sps, sps_len, type, p_cb->pUserdata);
    					    }

    					    if (pps_len > 0)
    					    {
    						    p_cb->pCallback(pps, pps_len, type, p_cb->pUserdata);
    						}
    					}
#endif    					
    				}
    				else if (v_codec == VIDEO_CODEC_H265)
    				{
    					uint8 vps[1024], sps[1024], pps[1024];
    					int vps_len, sps_len = 0, pps_len = 0;
    				
    					if (m_rtsp->get_h265_params(sps, &sps_len, pps, &pps_len, vps, &vps_len))
    					{
    						p_cb->pCallback(vps, vps_len, type, p_cb->pUserdata);
    						p_cb->pCallback(sps, sps_len, type, p_cb->pUserdata);
    						p_cb->pCallback(pps, pps_len, type, p_cb->pUserdata);
    					}
    				}
				}
#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)			
                else
                {
                    int extralen;
                    uint8 * extradata;
                    
                    if (m_pVideoEncoder->getExtraData(&extradata, &extralen))
                    {
                        p_cb->pCallback(extradata, extralen, type, p_cb->pUserdata);
                    }
                }
#endif				
			}
			else if (p_cb->bAFirst && type == DATA_TYPE_AUDIO)
			{
			    p_cb->bAFirst = FALSE;

#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)			
                if (NULL == m_pAudioEncoder)
#endif
                {
                    int len = 0;
                    uint8 * p_spec = NULL;
                    
                    if (m_rtsp)
                    {
        				len = m_rtsp->get_audio_config_len();
        				p_spec = m_rtsp->get_audio_config();
    				}
#ifdef RTMP_PROXY
                    else if (m_rtmp)
                    {
                        len = m_rtmp->get_audio_config_len();
        				p_spec = m_rtmp->get_audio_config();
                    }
#endif
                    if (p_spec && len > 0)
    				{
    					p_cb->pCallback(p_spec, len, type, p_cb->pUserdata);
    				}
				}
#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)			
                else
                {
                    int extralen;
                    uint8 * extradata;
                    
                    if (m_pAudioEncoder->getExtraData(&extradata, &extralen))
                    {
                        p_cb->pCallback(extradata, extralen, type, p_cb->pUserdata);
                    }
                }
#endif
			}

			p_cb->pCallback(data, size, type, p_cb->pUserdata);
		}
		
		p_node = h_list_lookup_next(m_pCallbackList, p_node);
	}
	h_list_lookup_end(m_pCallbackList);

	sys_os_mutex_leave(m_pCallbackMutex);
}

char * CMediaProxy::getH264AuxSDPLine(int rtp_pt)
{
	char sdp[1024] = {'\0'};

    if (NULL == m_rtsp)
	{
		return NULL;
	}

	if (m_rtsp->get_h264_sdp_desc(sdp, sizeof(sdp)) == FALSE)
	{
	    log_print(HT_LOG_ERR, "%s, get_h264_sdp_desc failed\r\n", __FUNCTION__);
	    return NULL;
	}
	
  	char const* fmtpFmt = "a=fmtp:%d %s";
    	
  	uint32 fmtpFmtSize = strlen(fmtpFmt)
    	+ 3 /* max char len */
    	+ strlen(sdp);
    	
	char* fmtp = new char[fmtpFmtSize+1];
	memset(fmtp, 0, fmtpFmtSize+1);

  	sprintf(fmtp, fmtpFmt, rtp_pt, sdp);

  	return fmtp;
}

char * CMediaProxy::getH265AuxSDPLine(int rtp_pt)
{
	char sdp[1024] = {'\0'};

    if (NULL == m_rtsp)
	{
		return NULL;
	}

	if (m_rtsp->get_h265_sdp_desc(sdp, sizeof(sdp)) == FALSE)
	{
	    log_print(HT_LOG_ERR, "%s, get_h265_sdp_desc failed\r\n", __FUNCTION__);
	    return NULL;
	}

	char const* fmtpFmt = "a=fmtp:%d %s";
    	
  	uint32 fmtpFmtSize = strlen(fmtpFmt)
    	+ 3 /* max char len */
    	+ strlen(sdp);
    	
	char* fmtp = new char[fmtpFmtSize+1];
	memset(fmtp, 0, fmtpFmtSize+1);

  	sprintf(fmtp, fmtpFmt, rtp_pt, sdp);

  	return fmtp;
}

char * CMediaProxy::getMP4AuxSDPLine(int rtp_pt)
{
	char sdp[1024] = {'\0'};

    if (NULL == m_rtsp)
	{
		return NULL;
	}

	if (m_rtsp->get_mp4_sdp_desc(sdp, sizeof(sdp)) == FALSE)
	{
	    log_print(HT_LOG_ERR, "%s, get_mp4_sdp_desc failed\r\n", __FUNCTION__);
	    return NULL;
	}

	char const* fmtpFmt = "a=fmtp:%d %s";
    	
  	uint32 fmtpFmtSize = strlen(fmtpFmt)
    	+ 3 /* max char len */
    	+ strlen(sdp);
    	
	char* fmtp = new char[fmtpFmtSize+1];
	memset(fmtp, 0, fmtpFmtSize+1);

  	sprintf(fmtp, fmtpFmt, rtp_pt, sdp);

  	return fmtp;
}

char * CMediaProxy::getAACAuxSDPLine(int rtp_pt)
{
	char sdp[1024] = {'\0'};

    if (NULL == m_rtsp)
	{
		return NULL;
	}

	if (m_rtsp->get_aac_sdp_desc(sdp, sizeof(sdp)) == FALSE)
	{
	    log_print(HT_LOG_ERR, "%s, get_aac_sdp_desc failed\r\n", __FUNCTION__);
	    return NULL;
	}

	char const* fmtpFmt = "a=fmtp:%d %s";
    	
  	uint32 fmtpFmtSize = strlen(fmtpFmt)
    	+ 3 /* max char len */
    	+ strlen(sdp);
    	
	char* fmtp = new char[fmtpFmtSize+1];
	memset(fmtp, 0, fmtpFmtSize+1);

  	sprintf(fmtp, fmtpFmt, rtp_pt, sdp);

  	return fmtp;
}

#ifdef RTMP_PROXY

char * CMediaProxy::getRTMPH264AuxSDPLine(int rtp_pt)
{
    uint8 sps[512]; int spsSize = 0;
	uint8 pps[512]; int ppsSize = 0;

	if (!m_rtmp->get_h264_params(sps, &spsSize, pps, &ppsSize))
	{
	    return NULL;
	}

	if (spsSize == 0 || ppsSize == 0)
	{
		return NULL;
	}

	// Set up the "a=fmtp:" SDP line for this stream:
	uint8* spsWEB = new uint8[spsSize]; // "WEB" means "Without Emulation Bytes"
  	uint32 spsWEBSize = remove_emulation_bytes(spsWEB, spsSize, sps, spsSize);
  	if (spsWEBSize < 4) 
  	{
  		// Bad SPS size => assume our source isn't ready
    	delete[] spsWEB;
    	return NULL;
  	}
  	uint8 profileLevelId = (spsWEB[1]<<16) | (spsWEB[2]<<8) | spsWEB[3];
  	delete[] spsWEB;

  	char* sps_base64 = new char[spsSize*2+1];
  	char* pps_base64 = new char[ppsSize*2+1];

	base64_encode(sps, spsSize, sps_base64, spsSize*2+1);
	base64_encode(pps, ppsSize, pps_base64, ppsSize*2+1);
	
  	char const* fmtpFmt =
    	"a=fmtp:%d packetization-mode=1"
    	";profile-level-id=%06X"
    	";sprop-parameter-sets=%s,%s";
    	
  	uint32 fmtpFmtSize = strlen(fmtpFmt)
    	+ 3 /* max char len */
    	+ 6 /* 3 bytes in hex */
    	+ strlen(sps_base64) + strlen(pps_base64);
    	
  	char* fmtp = new char[fmtpFmtSize+1];
  	memset(fmtp, 0, fmtpFmtSize+1);
  	
  	sprintf(fmtp, fmtpFmt, rtp_pt, profileLevelId, sps_base64, pps_base64);

  	delete[] sps_base64;
  	delete[] pps_base64;

  	return fmtp;
}

char * CMediaProxy::getRTMPAACAuxSDPLine(int rtp_pt)
{
    uint8 * ac = m_rtmp->get_audio_config();
    int aclen = m_rtmp->get_audio_config_len();

    if (NULL == ac || 0 == aclen)
    {
        return NULL;
    }
	
	char const* fmtpFmt =
        "a=fmtp:%d "
    	"streamtype=5;profile-level-id=1;"
    	"mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;"
    	"config=";
	uint32 fmtpFmtSize = strlen(fmtpFmt)
	    + 3 /* max char len */
	    + 2 * aclen; /* 2*, because each byte prints as 2 chars */

	char* fmtp = new char[fmtpFmtSize+1];
	memset(fmtp, 0, fmtpFmtSize+1);
	
	sprintf(fmtp, fmtpFmt, rtp_pt);
	char* endPtr = &fmtp[strlen(fmtp)];
	for (int i = 0; i < aclen; ++i) 
	{
		sprintf(endPtr, "%02X", ac[i]);
		endPtr += 2;
	}

	return fmtp;
}

#endif // RTMP_PROXY

char * CMediaProxy::getVideoAuxSDPLine(int rtp_pt)
{
#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)
    if (m_pVideoEncoder)
    {
        return m_pVideoEncoder->getAuxSDPLine(rtp_pt);
    }
    else
#endif

	if (v_codec == VIDEO_CODEC_H264)
	{
	    if (m_rtsp)
	    {
		    return getH264AuxSDPLine(rtp_pt);
		}
#ifdef RTMP_PROXY
        else if (m_rtmp)
        {
            return getRTMPH264AuxSDPLine(rtp_pt);
        }
#endif
	}
	else if (v_codec == VIDEO_CODEC_H265)
	{
		return getH265AuxSDPLine(rtp_pt);
	}
	else if (v_codec == VIDEO_CODEC_MP4)
	{
		return getMP4AuxSDPLine(rtp_pt);
	}

	return NULL;  	
}

char * CMediaProxy::getAudioAuxSDPLine(int rtp_pt)
{
#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)
    if (m_pAudioEncoder)
    {
        return m_pAudioEncoder->getAuxSDPLine(rtp_pt);
    }
    else
#endif

	if (a_codec == AUDIO_CODEC_AAC)
	{
	    if (m_rtsp)
	    {
		    return getAACAuxSDPLine(rtp_pt);
		}
#ifdef RTMP_PROXY
        else if (m_rtmp)
        {
            return getRTMPAACAuxSDPLine(rtp_pt);
        }
#endif		
	}

	return NULL;  	
}

int CMediaProxy::parseVideoSize(uint8 * p_data, int len)
{
    uint8 nalu_t;

	if (p_data == NULL || len < 5)
	{
		return -1;
    }
    
	// Need to parse width X height

	if (VIDEO_CODEC_H264 == v_codec)
	{        
		int s_len = 0, n_len = 0, parse_len = len;
		uint8 * p_cur = p_data;

		while (p_cur)
		{
			uint8 * p_next = avc_split_nalu(p_cur, parse_len, &s_len, &n_len);
			if (n_len < 5)
			{
				return 0;
            }
            
			nalu_t = (p_cur[s_len] & 0x1F);
			
			int b_start;
			nal_t nal;
			
			nal.i_payload = n_len-s_len-1;
			nal.p_payload = p_cur+s_len+1;
			nal.i_type = nalu_t;

			if (nalu_t == H264_NAL_SPS)	
			{
				h264_t parse;
				h264_parser_init(&parse);

				h264_parser_parse(&parse, &nal, &b_start);
				log_print(HT_LOG_INFO, "%s, H264 width[%d],height[%d]\r\n", __FUNCTION__, parse.i_width, parse.i_height);
				v_width = parse.i_width;
				v_height = parse.i_height;
				return 0;
			}
			
			parse_len -= n_len;
			p_cur = p_next;
		}
	}
	else if (VIDEO_CODEC_H265 == v_codec)
	{
		int s_len, n_len = 0, parse_len = len;
		uint8 * p_cur = p_data;

		while (p_cur)
		{
			uint8 * p_next = avc_split_nalu(p_cur, parse_len, &s_len, &n_len);
			if (n_len < 5)
			{
				return 0;
            }
            
			nalu_t = (p_cur[s_len] >> 1) & 0x3F;
			
			if (nalu_t == HEVC_NAL_SPS)	
			{
				h265_t parse;
				h265_parser_init(&parse);

				if (h265_parser_parse(&parse, p_cur+4, n_len-s_len) == 0)
				{
					log_print(HT_LOG_INFO, "%s, H265 width[%d],height[%d]\r\n", __FUNCTION__, parse.pic_width_in_luma_samples, parse.pic_height_in_luma_samples);
					v_width = parse.pic_width_in_luma_samples;
					v_height = parse.pic_height_in_luma_samples;
					return 0;
				}
			}
			
			parse_len -= n_len;
			p_cur = p_next;
		}
	}
    else if (VIDEO_CODEC_JPEG == v_codec)
    {
        int offset = 0;
        int size_chunk = 0;
        
        while (offset < len - 8 && p_data[offset] == 0xFF)
        {
            if (p_data[offset+1] == MARKER_SOF0)
            {
                int h = ((p_data[offset+5] << 8) | p_data[offset+6]);
                int w = ((p_data[offset+7] << 8) | p_data[offset+8]);
                log_print(HT_LOG_INFO, "%s, MJPEG width[%d],height[%d]\r\n", __FUNCTION__, w, h);
                v_width = w;
			    v_height = h;
                break;
            }
            else if (p_data[offset+1] == MARKER_SOI)
            {
                offset += 2;
            }
            else
            {
                size_chunk = ((p_data[offset+2] << 8) | p_data[offset+3]);
                offset += 2 + size_chunk;
            }
        }
    }
    else if (VIDEO_CODEC_MP4 == v_codec)
    {
        int pos = 0;
        int vol_f = 0;
        int vol_pos = 0;
        int vol_len = 0;

        while (pos < len - 4)
        {
            if (p_data[pos] == 0 && p_data[pos+1] == 0 && p_data[pos+2] == 1)
            {
                if (p_data[pos+3] >= 0x20 && p_data[pos+3] <= 0x2F)
                {
                    vol_f = 1;
                    vol_pos = pos+4;
                }
                else if (vol_f)
                {
                    vol_len = pos - vol_pos;
                    break;
                }
            }

            pos++;
        }

        if (!vol_f)
        {
            return 0;
        }
        else if (vol_len <= 0)
        {
            vol_len = len - vol_pos;
        }

        int vo_ver_id;
                    
        BitVector bv(&p_data[vol_pos], 0, vol_len*8);

        bv.skipBits(1);     /* random access */
        bv.skipBits(8);     /* vo_type */

        if (bv.get1Bit())   /* is_ol_id */
        {
            vo_ver_id = bv.getBits(4); /* vo_ver_id */
            bv.skipBits(3); /* vo_priority */
        }

        if (bv.getBits(4) == 15) // aspect_ratio_info
        {
            bv.skipBits(8);     // par_width
            bv.skipBits(8);     // par_height
        }

        if (bv.get1Bit()) /* vol control parameter */
        {
            int chroma_format = bv.getBits(2);

            bv.skipBits(1);     /* low_delay */

            if (bv.get1Bit()) 
            {    
                /* vbv parameters */
                bv.getBits(15);   /* first_half_bitrate */
                bv.skipBits(1);
                bv.getBits(15);   /* latter_half_bitrate */
                bv.skipBits(1);
                bv.getBits(15);   /* first_half_vbv_buffer_size */
                bv.skipBits(1);
                bv.getBits(3);    /* latter_half_vbv_buffer_size */
                bv.getBits(11);   /* first_half_vbv_occupancy */
                bv.skipBits(1);
                bv.getBits(15);   /* latter_half_vbv_occupancy */
                bv.skipBits(1);
            }                        
        }

        int shape = bv.getBits(2); /* vol shape */
        
        if (shape == 3 && vo_ver_id != 1) 
        {
            bv.skipBits(4);  /* video_object_layer_shape_extension */
        }

        bv.skipBits(1); 
        
        int framerate = bv.getBits(16);

        int time_increment_bits = (int) (log(framerate - 1.0) * 1.44269504088896340736 + 1); // log2(framerate - 1) + 1
        if (time_increment_bits < 1)
        {
            time_increment_bits = 1;
        }
        
        bv.skipBits(1);
        
        if (bv.get1Bit() != 0)     /* fixed_vop_rate  */
        {
            bv.skipBits(time_increment_bits);    
        }
        
        if (shape != 2)
        {
            if (shape == 0) 
            {
                bv.skipBits(1);
                int w = bv.getBits(13);
                bv.skipBits(1);
                int h = bv.getBits(13);

                log_print(HT_LOG_INFO, "%s, MPEG4 width[%d],height[%d]\r\n", __FUNCTION__, w, h);
                v_width = w;
    		    v_height = h;
            }
        }        
    }
    
	return 0;
}

#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)

/**
 * 1  - need
 * 0  - Still not sure
 * -1 - not need
 */
int CMediaProxy::needVideoRecodec(uint8 * p_data, int len)
{
    if (0 != m_nVideoRecodec)
    {
        return m_nVideoRecodec;    
    }
    
    if (!m_pConfig->has_output)
    {
        m_nVideoRecodec = -1;
        return -1;
    }

    if (v_width == 0 || v_height == 0)
    {
        return 0;
    }

    if (VIDEO_CODEC_NONE == m_pConfig->output.v_info.codec)
    {
        m_pConfig->output.v_info.codec = v_codec;
    }

    if (0 == m_pConfig->output.v_info.width)
    {
        m_pConfig->output.v_info.width = v_width;
    }

    if (0 == m_pConfig->output.v_info.height)
    {
        m_pConfig->output.v_info.height = v_height;
    }

    if (0 == m_pConfig->output.v_info.framerate)
    {
        m_pConfig->output.v_info.framerate = 25;
    }
            
    if (m_pConfig->output.v_info.codec != v_codec || 
        m_pConfig->output.v_info.width != v_width ||
        m_pConfig->output.v_info.height != v_height)
    {
        m_pVideoDecoder = new CVideoDecoder;
        if (NULL == m_pVideoDecoder)
		{
		    m_nVideoRecodec = -1;
		    log_print(HT_LOG_ERR, "%s, new video decoder failed\r\n", __FUNCTION__);
			return -1;
		}

		if (!m_pVideoDecoder->init(v_codec))
		{
		    m_nVideoRecodec = -1;
		    log_print(HT_LOG_ERR, "%s, video decoder init failed\r\n", __FUNCTION__);
		    return -1;
		}

		m_pVideoDecoder->setCallback(media_proxy_vdecoder_cb, this);

		m_pVideoEncoder = new CVideoEncoder;		
		if (NULL == m_pVideoEncoder)
		{
		    m_nVideoRecodec = -1;
		    log_print(HT_LOG_ERR, "%s, new video encoder failed\r\n", __FUNCTION__);
			return -1;
		}

		VideoEncoderParam params;
		memset(&params, 0, sizeof(params));
		
		params.SrcWidth = v_width;
		params.SrcHeight = v_height;
		params.SrcPixFmt = AV_PIX_FMT_YUV420P;
		params.DstCodec = m_pConfig->output.v_info.codec;
		params.DstWidth = m_pConfig->output.v_info.width;
		params.DstHeight = m_pConfig->output.v_info.height;
		params.DstFramerate = m_pConfig->output.v_info.framerate;
		params.DstBitrate = m_pConfig->output.v_info.bitrate;
		
		if (FALSE == m_pVideoEncoder->init(&params))
		{
		    m_nVideoRecodec = -1;
		    log_print(HT_LOG_ERR, "%s, video encoder init failed\r\n", __FUNCTION__);
			return -1;
		}

		m_pVideoEncoder->addCallback(media_proxy_vencoder_cb, this);

		m_nVideoRecodec = 1;
    }
    else
    {
        m_nVideoRecodec = -1;
    }

    return m_nVideoRecodec;
}

/**
 * 1  - need
 * 0  - Still not sure
 * -1 - not need
 */
int CMediaProxy::needAudioRecodec()
{
    if (0 != m_nAudioRecodec)
    {
        return m_nAudioRecodec;    
    }
    
    if (!m_pConfig->has_output)
    {
        m_nAudioRecodec = -1;
        return -1;
    }

    if (AUDIO_CODEC_NONE == m_pConfig->output.a_info.codec)
    {
        m_pConfig->output.a_info.codec = a_codec;
    }

    if (0 == m_pConfig->output.a_info.samplerate)
    {
        m_pConfig->output.a_info.samplerate = a_samplerate;
    }

    if (0 == m_pConfig->output.a_info.channels)
    {
        m_pConfig->output.a_info.channels = a_channels;
    }
            
    if (m_pConfig->output.a_info.codec != a_codec || 
        m_pConfig->output.a_info.samplerate != a_samplerate ||
        m_pConfig->output.a_info.channels != a_channels)
    {
        m_pAudioDecoder = new CAudioDecoder;
        if (NULL == m_pAudioDecoder)
		{
		    m_nAudioRecodec = -1;
		    log_print(HT_LOG_ERR, "%s, new audio decoder failed\r\n", __FUNCTION__);
			return -1;
		}

		if (!m_pAudioDecoder->init(a_codec, a_samplerate, a_channels, NULL, 0))
		{
		    m_nAudioRecodec = -1;
		    log_print(HT_LOG_ERR, "%s, audio decoder init failed\r\n", __FUNCTION__);
		    return -1;
		}

		m_pAudioDecoder->setCallback(media_proxy_adecoder_cb, this);

		m_pAudioEncoder = new CAudioEncoder;		
		if (NULL == m_pAudioEncoder)
		{
		    m_nAudioRecodec = -1;
		    log_print(HT_LOG_ERR, "%s, new audio encoder failed\r\n", __FUNCTION__);
			return -1;
		}

		AudioEncoderParam params;
		memset(&params, 0, sizeof(params));
		
		params.SrcSamplerate = a_samplerate;
		params.SrcChannels = a_channels;
		params.SrcSamplefmt = AV_SAMPLE_FMT_S16;
		params.DstCodec = m_pConfig->output.a_info.codec;
		params.DstSamplerate = m_pConfig->output.a_info.samplerate;
		params.DstChannels = m_pConfig->output.a_info.channels;
		params.DstSamplefmt = AV_SAMPLE_FMT_S16;
		params.DstBitrate = m_pConfig->output.a_info.bitrate;
		
		if (FALSE == m_pAudioEncoder->init(&params))
		{
		    m_nAudioRecodec = -1;
		    log_print(HT_LOG_ERR, "%s, audio encoder init failed\r\n", __FUNCTION__);
			return -1;
		}

		m_pAudioEncoder->addCallback(meida_proxy_aencoder_cb, this);

		m_nAudioRecodec = 1;
    }
    else
    {
        m_nAudioRecodec = -1;
    }

    return m_nAudioRecodec;
}

void CMediaProxy::videoDataDecode(uint8 * p_data, int len)
{
    if (m_pVideoDecoder)
    {
        m_pVideoDecoder->decode(p_data, len);
    }
}

void CMediaProxy::videoDataEncode(AVFrame * frame)
{
    if (m_pVideoEncoder)
    {
        m_pVideoEncoder->encode(frame);
    }
}

void CMediaProxy::audioDataDecode(uint8 * p_data, int len)
{
    if (m_pAudioDecoder)
    {
        m_pAudioDecoder->decode(p_data, len);
    }
}

void CMediaProxy::audioDataEncode(AVFrame * frame)
{
    if (m_pAudioEncoder)
    {
        m_pAudioEncoder->encode(frame);
    }
}

#endif // end of defined(MEDIA_FILE) || defined(MEDIA_DEVICE)

#endif // end of MEDIA_PROXY


