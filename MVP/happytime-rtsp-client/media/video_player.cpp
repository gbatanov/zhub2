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

#include "video_player.h"

extern "C" 
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/intreadwrite.h>
#include <libavutil/avstring.h>
#include <libavutil/base64.h>
#include <libavutil/imgutils.h>
}

#if __WINDOWS_OS__
#include "video_render_d3d.h"
#include "video_render_gdi.h"
#include "audio_play_dsound.h"
#elif __LINUX_OS__
#include "audio_play_linux.h"
#endif

void VideoDecoderCallback(AVFrame * frame, void * userdata)
{
    CVideoPlayer * pPlayer = (CVideoPlayer *) userdata;

    pPlayer->onVideoFrame(frame);
}

void AudioDecoderCallback(AVFrame * frame, void * userdata)
{
    CVideoPlayer * pPlayer = (CVideoPlayer *) userdata;

    pPlayer->onAudioFrame(frame);
}


CVideoPlayer::CVideoPlayer(QObject * parent)
: QObject(parent)
, m_bVideoInited(FALSE)
, m_bAudioInited(FALSE)
, m_pVideoDecoder(NULL)
, m_pAudioDecoder(NULL)
, m_pVideoRender(NULL)
, m_pAudioPlay(NULL)
, m_nVideoWnd(NULL)
, m_nRenderMode(RENDER_MODE_KEEP)
, m_nDstVideoFmt(VIDEO_FMT_YUV420P)
, m_bSnapshot(FALSE)
, m_nSnapVideoFmt(VIDEO_FMT_BGR24)
, m_pSnapFrame(NULL)
, m_pRenderFrame(NULL)
, m_bRecording(FALSE)
, m_pAviCtx(NULL)
{
    m_pRecordMutex = sys_os_create_mutex();
}

CVideoPlayer::~CVideoPlayer()
{
    close();
}

BOOL CVideoPlayer::open(QString fileName, HWND hWnd)
{
    m_sFileName = fileName;
    m_nVideoWnd = hWnd;

    return TRUE;
}

void CVideoPlayer::close()
{
    closeVideo();
    closeAudio();

    if (m_pSnapFrame)
    {
        av_frame_free(&m_pSnapFrame);
    }
    
    if (m_pRenderFrame)
    {
        av_frame_free(&m_pRenderFrame);
    }

    stopRecord();

    sys_os_destroy_sig_mutex(m_pRecordMutex);
    m_pRecordMutex = NULL;
}

void CVideoPlayer::setVolume(int volume)
{
    if (m_pAudioPlay)
    {
        m_pAudioPlay->setVolume(volume);
    }
}

void CVideoPlayer::snapshot(int videofmt)
{
    m_bSnapshot = TRUE;
    m_nSnapVideoFmt = videofmt;
}

BOOL CVideoPlayer::record(QString filepath)
{
    if (m_bRecording)
    {
        return TRUE;
    }
    
    m_pAviCtx = avi_write_open(filepath.toStdString().c_str());
    if (NULL == m_pAviCtx)
    {
        log_print(HT_LOG_ERR, "%s, avi_write_open failed. %s\r\n", __FUNCTION__, filepath.toStdString().c_str());
        return FALSE;
    }
	
	if (!onRecord())
	{
	    avi_write_close(m_pAviCtx);
	    m_pAviCtx = NULL;
	    
	    return FALSE;
	}

	m_bRecording = TRUE;
	
	return m_bRecording;
}

void CVideoPlayer::stopRecord()
{
    sys_os_mutex_enter(m_pRecordMutex);
    
    m_bRecording = FALSE;

    if (m_pAviCtx)
    {
        avi_write_close(m_pAviCtx);
        m_pAviCtx = NULL;
    }

    sys_os_mutex_leave(m_pRecordMutex);
}

void CVideoPlayer::recordVideo(uint8 * data, int len, uint32 ts, uint16 seq)
{
    AVICTX * p_avictx = m_pAviCtx;
    
	if (p_avictx->v_fps == 0)
    {
        avi_calc_fps(p_avictx, data, len, ts);
    }

    if (p_avictx->v_width == 0 || p_avictx->v_height == 0)
    {
        avi_parse_video_size(p_avictx, data, len);
        
        if (p_avictx->v_width && p_avictx->v_height)
        {
            avi_update_header(p_avictx);
        }
    }

    int key = 0;

    if (memcmp(p_avictx->v_fcc, "H264", 4) == 0)
    {
        uint8 nalu_t = (data[4] & 0x1F);
        key = (nalu_t == 5);
    }
    else if (memcmp(p_avictx->v_fcc, "H265", 4) == 0)
    {
        uint8 nalu_t = (data[4] >> 1) & 0x3F;
        key = (nalu_t >= 16 && nalu_t <= 21);
    }  
    else if (memcmp(p_avictx->v_fcc, "JPEG", 4) == 0)
    {
        key = 1;
    }
    
    avi_write_video(p_avictx, data, len, key);
}

void CVideoPlayer::recordAudio(uint8 * data, int len, uint32 ts, uint16 seq)
{
	AVICTX * p_avictx = m_pAviCtx;

    if (p_avictx->a_fmt == 0xFF) // AAC
    {
        int chns = p_avictx->a_chns;
        int rate_idx = 0;
        uint16 frame_len = len + 7;
        uint8 adts[7];

        adts[0] = 0xff;
        adts[1] = 0xf9;
        adts[2] = (2 - 1) << 6; // profile, AAC-LC

        switch (p_avictx->a_rate)
        {
        case 96000:
            rate_idx = 0;
            break;

        case 88200:
            rate_idx = 1;
            break;
            
        case 64000:
            rate_idx = 2;
            break;
            
        case 48000:
            rate_idx = 3;
            break;
            
        case 44100:
            rate_idx = 4;
            break;
            
        case 32000:
            rate_idx = 5;
            break;
            
        case 24000:
            rate_idx = 6;
            break;

        case 22050:
            rate_idx = 7;
            break;

        case 16000:
            rate_idx = 8;
            break;     

        case 12000:
            rate_idx = 9;
            break;

        case 11025:
            rate_idx = 10;
            break;

        case 8000:
            rate_idx = 11;
            break;

        case 7350:
            rate_idx = 12;
            break;
            
        default:
            rate_idx = 4;
            break;
        }

        adts[2] |= (rate_idx << 2);
        adts[2] |= (chns & 0x4) >> 2;
        adts[3] =  (chns & 0x3) << 6;
        adts[3] |= (frame_len & 0x1800) >> 11;
        adts[4] =  (frame_len & 0x1FF8) >> 3;
        adts[5] =  (frame_len & 0x7) << 5; 

        /* buffer fullness (0x7FF for VBR) over 5 last bits*/
        
        adts[5] |= 0x1F;
        
        adts[6] = 0xFC;
        adts[6] |= (len / 1024) & 0x03; // set raw data blocks. 
    
        memcpy(data - 7, adts, 7); // this will destroy rtp header
        
        avi_write_audio(p_avictx, data - 7, len + 7);
    }
    else
    {
        avi_write_audio(p_avictx, data, len);
    }
}

BOOL CVideoPlayer::openVideo(int codec)
{
    m_pVideoDecoder = new CVideoDecoder();
    if (m_pVideoDecoder)
    {
		m_bVideoInited = m_pVideoDecoder->init(codec);
    }

    if (m_bVideoInited)
    {
        m_pVideoDecoder->setCallback(VideoDecoderCallback, this);
    }

    return m_bVideoInited;
}

void CVideoPlayer::closeVideo()
{	
    if (m_pVideoDecoder)
    {
		delete m_pVideoDecoder;
	    m_pVideoDecoder = NULL;
	}

	if (m_pVideoRender)
	{
	    delete m_pVideoRender;
	    m_pVideoRender = NULL;
	}

	m_bVideoInited = FALSE;
}

BOOL CVideoPlayer::openAudio(int codec, int samplerate, int channels, uint8 * config, int configlen)
{
	m_pAudioDecoder = new CAudioDecoder();
	if (m_pAudioDecoder)
	{	
		m_bAudioInited = m_pAudioDecoder->init(codec, samplerate, channels, config, configlen);
	}

	if (m_bAudioInited)
	{
		m_pAudioDecoder->setCallback(AudioDecoderCallback, this);

		m_pAudioPlay = new CDSoundAudioPlay(samplerate, channels);
		if (m_pAudioPlay)
		{
		    m_pAudioPlay->startPlay();
		}
	}

	return m_bAudioInited;
}

void CVideoPlayer::closeAudio()
{
	if (m_pAudioDecoder)
	{
		delete m_pAudioDecoder;
		m_pAudioDecoder = NULL;
	}

	if (m_pAudioPlay)
	{
	    delete m_pAudioPlay;
		m_pAudioPlay = NULL;
	}

	m_bAudioInited = FALSE;
}

void CVideoPlayer::playVideo(uint8 * data, int len, uint32 ts, uint16 seq)
{
    if (m_bRecording)
    {
        sys_os_mutex_enter(m_pRecordMutex);
        recordVideo(data, len, ts, seq);
        sys_os_mutex_leave(m_pRecordMutex);
    }
    
    if (m_bVideoInited)
    {
		m_pVideoDecoder->decode(data, len);
    }
}

void CVideoPlayer::playAudio(uint8 * data, int len, uint32 ts, uint16 seq)
{
    if (m_bRecording)
    {
        sys_os_mutex_enter(m_pRecordMutex);
        recordAudio(data, len, ts, seq);
        sys_os_mutex_leave(m_pRecordMutex);
    }
    
    if (m_bAudioInited)
    {
		m_pAudioDecoder->decode(data, len);
    }
}

BOOL CVideoPlayer::initSnapFrame(AVFrame * frame, int dstfmt)
{
    if (m_pSnapFrame)
    {
	    av_frame_free(&m_pSnapFrame);
	}
	
    m_pSnapFrame = av_frame_alloc();	
	if (NULL == m_pSnapFrame)
	{
		return FALSE;
	}

	enum AVPixelFormat pixfmt;

    if (VIDEO_FMT_BGR24 == dstfmt)
    {
        pixfmt = AV_PIX_FMT_BGR24;
    }
    else if (VIDEO_FMT_RGB24 == dstfmt)
    {
        pixfmt = AV_PIX_FMT_RGB24;
    }
    else if (VIDEO_FMT_RGB32 == dstfmt)
    {
        pixfmt = AV_PIX_FMT_RGB32;
    }
    else
    {
        av_frame_free(&m_pSnapFrame);
        return FALSE;
    }

	m_pSnapFrame->format = pixfmt;
	m_pSnapFrame->width  = frame->width; 
	m_pSnapFrame->height = frame->height;

	if (0 != av_frame_get_buffer(m_pSnapFrame, 1))
	{
	    av_frame_free(&m_pSnapFrame);
	    return FALSE;
	}

    av_frame_make_writable(m_pSnapFrame);

    return TRUE;
}

BOOL CVideoPlayer::initRenderFrame(AVFrame * frame, int dstfmt)
{
    if (NULL == m_pRenderFrame || m_pRenderFrame->width != frame->width || m_pRenderFrame->height != frame->height)
    {
        if (m_pRenderFrame)
        {
    	    av_frame_free(&m_pRenderFrame);
    	}
    	
    	m_pRenderFrame = av_frame_alloc();    	
    	if (NULL == m_pRenderFrame)
    	{
    		return FALSE;
    	}

    	enum AVPixelFormat pixfmt;

        if (VIDEO_FMT_BGR24 == dstfmt)
        {
            pixfmt = AV_PIX_FMT_BGR24;
        }
        else if (VIDEO_FMT_RGB24 == dstfmt)
        {
            pixfmt = AV_PIX_FMT_RGB24;
        }
        else if (VIDEO_FMT_RGB32 == dstfmt)
        {
            pixfmt = AV_PIX_FMT_RGB32;
        }
        else
        {
            av_frame_free(&m_pRenderFrame);
            return FALSE;
        }

    	m_pRenderFrame->format = pixfmt;
    	m_pRenderFrame->width  = frame->width; 
    	m_pRenderFrame->height = frame->height;

    	if (0 != av_frame_get_buffer(m_pRenderFrame, 1))
    	{
    	    av_frame_free(&m_pRenderFrame);
    	    return FALSE;
    	}

        av_frame_make_writable(m_pRenderFrame);

        return TRUE;
	}

	return TRUE;
}

BOOL CVideoPlayer::doSnapshot(AVFrame * frame)
{
    if (!initSnapFrame(frame, m_nSnapVideoFmt))
    {
        return FALSE;
    }

    if (NULL == convertFrame(frame, m_pSnapFrame, FALSE))
    {
        return FALSE;
    }
    
    emit snapshoted(m_pSnapFrame);

    return TRUE;
}

AVFrame * CVideoPlayer::convertFrame(AVFrame * srcframe, AVFrame * dstframe, BOOL updown)
{
	SwsContext * swsctx = sws_getContext(srcframe->width, srcframe->height, (enum AVPixelFormat)srcframe->format,
		srcframe->width, srcframe->height, (enum AVPixelFormat)dstframe->format, SWS_BICUBIC, NULL, NULL, NULL);
	if (swsctx)
	{
	    if (updown)
	    {
    	    srcframe->data[0] += srcframe->linesize[0] * (srcframe->height - 1);
    		srcframe->linesize[0] *= -1;
    		srcframe->data[1] += srcframe->linesize[1] * (srcframe->height / 2 - 1);
    		srcframe->linesize[1] *= -1;
    		srcframe->data[2] += srcframe->linesize[2] * (srcframe->height / 2 - 1);
    		srcframe->linesize[2] *= -1;
		}
				
		int ret = sws_scale(swsctx, srcframe->data, srcframe->linesize, 0, srcframe->height, dstframe->data, dstframe->linesize);
		if (ret > 0)
		{
			sws_freeContext(swsctx);
			return dstframe;
		}
		else
		{
		    log_print(HT_LOG_ERR, "%s, sws_scale failed\r\n", __FUNCTION__);
		    
    	    sws_freeContext(swsctx);
    		return NULL;
		}
	}

	return NULL;
}

void CVideoPlayer::onVideoFrame(AVFrame * frame)
{
    if (m_bSnapshot)
    {
        if (doSnapshot(frame))
        {
            m_bSnapshot = FALSE;
        }
    }
    
    if (NULL == m_pVideoRender)
    {
#ifdef __WINDOWS_OS__

        BOOL init = FALSE;
        
        m_pVideoRender = new CD3DVideoRender();
        if (m_pVideoRender)
        {
            init = m_pVideoRender->init(m_nVideoWnd, frame->width, frame->height, VIDEO_FMT_YUV420P);
        }

        if (!init)
        {
            if (m_pVideoRender)
            {
                delete m_pVideoRender;
            }

            m_pVideoRender = new CGDIVideoRender();
            if (m_pVideoRender)
            {
                if (m_pVideoRender->init(m_nVideoWnd, frame->width, frame->height, VIDEO_FMT_BGR24))
                {
                    m_nDstVideoFmt = VIDEO_FMT_BGR24;
                    log_print(HT_LOG_INFO, "%s, use GDI to render video\r\n", __FUNCTION__);
                }
                else
                {
                    delete m_pVideoRender;
                    m_pVideoRender = NULL;
                }
            }
        }
        else
        {
            log_print(HT_LOG_INFO, "%s, use D3D to render video\r\n", __FUNCTION__);
        }
#endif        
    }

    if (m_pVideoRender)
    {
        if (VIDEO_FMT_YUV420P != m_nDstVideoFmt)
        {
            if (initRenderFrame(frame, m_nDstVideoFmt))
            {
                frame = convertFrame(frame, m_pRenderFrame, TRUE);
            }
            else
            {
                return;
            }
        }

        if (frame)
        {
            m_pVideoRender->render(frame, m_nRenderMode);
        }
    }
}

void CVideoPlayer::onAudioFrame(AVFrame * frame)
{
    if (m_pAudioPlay)
    {
        m_pAudioPlay->playAudio(frame->data[0], frame->nb_samples * frame->channels * 2);
    }
}





