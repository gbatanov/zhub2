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

#include "rtsp_player.h"
#include "utils.h"

/***************************************************************************************/

int rtsp_notify_cb(int evt, void * puser)
{
    CRtspPlayer * pPlayer = (CRtspPlayer *) puser;
    
    pPlayer->onNotify(evt);
    return 0;
}

int rtsp_audio_cb(uint8 * pdata, int len, uint32 ts, uint16 seq, void *puser)
{
    CRtspPlayer * pPlayer = (CRtspPlayer *) puser;

    pPlayer->onAudio(pdata, len, ts, seq);
    return 0;
}

int rtsp_video_cb(uint8 * pdata, int len, uint32 ts, uint16 seq, void *puser)
{
    CRtspPlayer * pPlayer = (CRtspPlayer *) puser;

    pPlayer->onVideo(pdata, len, ts, seq);
    return 0;
}

int rtsp_metadata_cb(uint8 * pdata, int len, uint32 ts, uint16 seq, void *puser)
{
	return 0;
}


/***************************************************************************************/

CRtspPlayer::CRtspPlayer(QObject * parent)
: CVideoPlayer(parent)
, m_bPlaying(false)
, m_bPaused(false)
, m_port(554)
{
    memset(m_ip, 0, sizeof(m_ip));
}

CRtspPlayer::~CRtspPlayer()
{
    close();
}

BOOL CRtspPlayer::open(QString fileName, HWND hWnd)
{
    close();

    CVideoPlayer::open(fileName, hWnd);

    char* username = NULL;
	char* password = NULL;
	char* address = NULL;
	int   urlPortNum = 554;
	
    if (!CRtspClient::parse_url(fileName.toStdString().c_str(), username, password, address, urlPortNum, NULL))
    {
        return FALSE;
    }

    if (username)
	{
		m_acct = username;
		delete[] username;
	}
	
	if (password)
	{
		m_pass = password;
		delete[] password;
	}

	strncpy(m_ip, address, sizeof(m_ip) - 1);
	delete[] address; 

	m_port = urlPortNum;
	
	m_rtsp.set_notify_cb(rtsp_notify_cb, this);
    m_rtsp.set_audio_cb(rtsp_audio_cb);
    m_rtsp.set_video_cb(rtsp_video_cb);
#ifdef METADATA
    m_rtsp.set_metadata_cb(rtsp_metadata_cb);
#endif
	 
	return TRUE;
}

void CRtspPlayer::close()
{
	// stop rtsp connection
	m_rtsp.rtsp_stop();
	m_rtsp.rtsp_close();

    m_bPlaying = FALSE;
    m_bPaused = FALSE;
    m_port = 554;

    memset(m_ip, 0, sizeof(m_ip));

    CVideoPlayer::close();
}

BOOL CRtspPlayer::play()
{
    char url[512];
    char user[64];
    char pass[64];
	strcpy(url, m_sFileName.toStdString().c_str());
	strcpy(user, m_acct.toStdString().c_str());
	strcpy(pass, m_pass.toStdString().c_str());
    
	if (m_rtsp.rtsp_start(url, user, pass))
    {
        m_bPlaying = TRUE;
    }

    return m_bPlaying;
}

void CRtspPlayer::stop()
{
    if (m_bPlaying || m_bPaused)
    {
        m_rtsp.rtsp_stop();
    }

    m_bPlaying = FALSE;
    m_bPaused = FALSE;
}

void CRtspPlayer::pause()
{
    if (m_bPlaying && m_rtsp.rtsp_pause())
    {
        m_bPaused = TRUE;
        m_bPlaying = FALSE;
    }
}

void CRtspPlayer::setRtpMulticast(BOOL flag)
{
	m_rtsp.set_rtp_multicast(flag);
}

void CRtspPlayer::setRtpOverUdp(BOOL flag)
{
    m_rtsp.set_rtp_over_udp(flag);
}

#ifdef BACKCHANNEL

int CRtspPlayer::getBCFlag()
{
	return m_rtsp.get_bc_flag();
}

void CRtspPlayer::setBCFlag(int flag)
{
	m_rtsp.set_bc_flag(flag);
}

int CRtspPlayer::getBCDataFlag()
{
	return m_rtsp.get_bc_data_flag();
}

void CRtspPlayer::setBCDataFlag(int flag)
{
	if (m_rtsp.get_bc_data_flag())
    {
        m_rtsp.set_bc_data_flag(0);
    }
    else
    {
    	m_rtsp.set_bc_data_flag(1);
    }
}

#endif // end of BACKCHANNEL

#ifdef REPLAY

int CRtspPlayer::getReplayFlag()
{
    return m_rtsp.get_replay_flag();
}

void CRtspPlayer::setReplayFlag(int flag)
{
    m_rtsp.set_replay_flag(flag);
}

void CRtspPlayer::setScale(double scale)
{
    m_rtsp.set_scale(scale);
}

void CRtspPlayer::setRateControlFlag(int flag)
{
    m_rtsp.set_rate_control_flag(flag);
}

void CRtspPlayer::setImmediateFlag(int flag)
{
    m_rtsp.set_immediate_flag(flag);
}

void CRtspPlayer::setFramesFlag(int flag, int interval)
{
    m_rtsp.set_frames_flag(flag, interval);
}

void CRtspPlayer::setReplayRange(time_t start, time_t end)
{
	m_rtsp.set_replay_range(start, end);
}

#endif // end of REPLAY

#ifdef OVER_HTTP

void CRtspPlayer::setRtspOverHttp(int flag, int port)
{
    m_rtsp.set_rtsp_over_http(flag, port);
}

#endif // OVER_HTTP

void CRtspPlayer::onNotify(int evt)
{
	if (evt == RTSP_EVE_CONNSUCC)
	{
		int videoCodec = m_rtsp.video_codec();
		int audioCodec = m_rtsp.audio_codec();

		if (AUDIO_CODEC_NONE != audioCodec)
		{
			openAudio(audioCodec, m_rtsp.get_audio_samplerate(), m_rtsp.get_audio_channels(),
				m_rtsp.get_audio_config(), m_rtsp.get_audio_config_len());
		}

		if (VIDEO_CODEC_NONE != videoCodec)
		{
			openVideo(videoCodec);
		}
	}
	
	emit notify(evt);
}

void CRtspPlayer::onAudio(uint8 * pdata, int len, uint32 ts, uint16 seq)
{
    playAudio(pdata, len, ts, seq);
}

void CRtspPlayer::onVideo(uint8 * pdata, int len, uint32 ts, uint16 seq)
{
    playVideo(pdata, len, ts, seq);
}

BOOL CRtspPlayer::onRecord()
{
	CRtspClient * p_rtsp = &m_rtsp;
    AVICTX * p_avictx = m_pAviCtx;
    
    int vcodec = p_rtsp->video_codec();	
    int v_extra_len = 0;
    uint8 v_extra[1024];
    
    if (VIDEO_CODEC_H264 == vcodec)
    {
        avi_set_video_info(p_avictx, 0, 0, 0, "H264");

        uint8 sps[512], pps[512];
		int sps_len = 0, pps_len = 0;
        
        if (p_rtsp->get_h264_params(sps, &sps_len, pps, &pps_len))
        {
            memcpy(v_extra, sps, sps_len);
            memcpy(v_extra + sps_len, pps, pps_len);

            v_extra_len = sps_len + pps_len;
        }

        if (v_extra_len > 0)
        {
			avi_set_video_extra_info(p_avictx, v_extra, v_extra_len);
        }
    }
    else if (VIDEO_CODEC_H265 == vcodec)
    {
        avi_set_video_info(p_avictx, 0, 0, 0, "H265");

        uint8 vps[512], sps[512], pps[512];
        int vps_len = 0, sps_len = 0, pps_len = 0;
            
        if (p_rtsp->get_h265_params(sps, &sps_len, pps, &pps_len, vps, &vps_len))
        {
            memcpy(v_extra, vps, vps_len);
            memcpy(v_extra + vps_len, sps, sps_len);
            memcpy(v_extra + vps_len + sps_len, pps, pps_len);

            v_extra_len = vps_len + sps_len + pps_len;
        }

        if (v_extra_len > 0)
        {
			avi_set_video_extra_info(p_avictx, v_extra, v_extra_len);
        }
    }
    else if (VIDEO_CODEC_JPEG == vcodec)
    {
        avi_set_video_info(p_avictx, 0, 0, 0, "JPEG");
    }
    else if (VIDEO_CODEC_MP4 == vcodec)
    {
        avi_set_video_info(p_avictx, 0, 0, 0, "MP4V");
    }

    int acodec = p_rtsp->audio_codec();
    int sr = p_rtsp->get_audio_samplerate();
    int ch = p_rtsp->get_audio_channels();
	    
    if (AUDIO_CODEC_G711A == acodec)
    {
        avi_set_audio_info(p_avictx, ch, sr, AUDIO_FORMAT_ALAW);
    }
    else if (AUDIO_CODEC_G711U == acodec)
    {
        avi_set_audio_info(p_avictx, ch, sr, AUDIO_FORMAT_MULAW);
    }
    else if (AUDIO_CODEC_G726 == acodec)
    {
        avi_set_audio_info(p_avictx, ch, sr, AUDIO_FORMAT_G726);
    }
    else if (AUDIO_CODEC_G722 == acodec)
    {
        avi_set_audio_info(p_avictx, ch, sr, AUDIO_FORMAT_G722);
    }
    else if (AUDIO_CODEC_AAC == acodec)
    {
        avi_set_audio_info(p_avictx, ch, sr, AUDIO_FORMAT_AAC);
        avi_set_audio_extra_info(p_avictx, p_rtsp->get_audio_config(), p_rtsp->get_audio_config_len());
    }

    avi_update_header(p_avictx);

    if (p_avictx->ctxf_video)
    {
        if (memcmp(p_avictx->v_fcc, "H264", 4) == 0)
        {
            uint8 sps[512];
            uint8 pps[512];
            int sps_len = sizeof(sps);
            int pps_len = sizeof(pps);

            if (p_rtsp->get_h264_params(sps, &sps_len, pps, &pps_len))
            {
                avi_write_video(p_avictx, sps, sps_len, 0);
                avi_write_video(p_avictx, pps, pps_len, 0);
            }
        }
        else if (memcmp(p_avictx->v_fcc, "H265", 4) == 0)
        {
            uint8 sps[512];
            uint8 pps[512];
            uint8 vps[512];
            int sps_len = sizeof(sps);
            int pps_len = sizeof(pps);
            int vps_len = sizeof(vps);

            if (p_rtsp->get_h265_params(sps, &sps_len, pps, &pps_len, vps, &vps_len))
            {
                avi_write_video(p_avictx, vps, vps_len, 0);
                avi_write_video(p_avictx, sps, sps_len, 0);
                avi_write_video(p_avictx, pps, pps_len, 0);
            }
        }
    }
    
	return TRUE;
}






