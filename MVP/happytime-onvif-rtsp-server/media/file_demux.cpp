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
#include "file_demux.h"
#include "audio_encoder.h"
#include "video_encoder.h"
#include "media_format.h"
#include "base64.h"
#include "media_util.h"
#include "h264.h"
#include "h265.h"
#include "avcodec_mutex.h"


/*********************************************************************************/

/**
 * audio data callback, if need re-codec audio data
 */
void AudioCallback(uint8 *data, int size, int nbsamples, void *pUserdata)
{
	CFileDemux * pDemux = (CFileDemux *)pUserdata;

	pDemux->dataCallback(data, size, DATA_TYPE_AUDIO, nbsamples, 1);
}

/**
 * video data callback, if need re-codec video data
 */
void VideoCallback(uint8 *data, int size, void *pUserdata)
{
	CFileDemux * pDemux = (CFileDemux *)pUserdata;

	pDemux->dataCallback(data, size, DATA_TYPE_VIDEO, 0, 1);
}


CFileDemux::CFileDemux(const char * filename, int loopnums)
{
	m_nAudioIndex = -1;
    m_nVideoIndex = -1;
    m_nDuration = 0;
    m_nCurPos = 0;

    m_nNalLength = 0;

    m_pFormatContext = NULL;
    m_pACodecCtx = NULL;    
    m_pVCodecCtx = NULL;
    
    m_pVideoEncoder = NULL;
	m_pAudioEncoder = NULL;

    m_pVideoFrame = NULL;
    m_pAudioFrame = NULL;
    
	m_bFirst = TRUE;
	m_pCallback = NULL;
	m_pUserdata = NULL;

    m_nLoopNums = 0;
    m_nMaxLoopNums = loopnums;
    
	init(filename);
}

CFileDemux::~CFileDemux()
{
	uninit();
}

/**
 * Init file demux
 * 
 * @param filename the file name
 * @return TRUE on success, FALSE on error
 */
BOOL CFileDemux::init(const char * filename)
{
	if (avformat_open_input(&m_pFormatContext, filename, NULL, NULL) != 0)
	{
		log_print(HT_LOG_ERR, "avformat_open_input failed, %s\r\n", filename);
		return FALSE;
	}

	avformat_find_stream_info(m_pFormatContext, NULL);

    if (m_pFormatContext->duration != AV_NOPTS_VALUE)
    {
        m_nDuration = m_pFormatContext->duration; 
    }
    
	// find audio & video stream index
	for (uint32 i=0; i < m_pFormatContext->nb_streams; i++)
	{
		if (m_pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			m_nVideoIndex = i;

			if (m_nDuration < m_pFormatContext->streams[i]->duration) 
            {
                m_nDuration = m_pFormatContext->streams[i]->duration;
            }
		}
		else if (m_pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			m_nAudioIndex = i;

			if (m_nDuration < m_pFormatContext->streams[i]->duration) 
            {
                m_nDuration = m_pFormatContext->streams[i]->duration;
            }
		}
	}

    m_nDuration /= 1000; // to millisecond
    
	// has video stream
	if (m_nVideoIndex != -1)
	{
	    AVCodecParameters * codecpar = m_pFormatContext->streams[m_nVideoIndex]->codecpar;
	    
		if (codecpar->codec_id == AV_CODEC_ID_H264)
    	{
    		if (codecpar->extradata && codecpar->extradata_size > 8)
    		{
    		    if (codecpar->extradata[0] == 1)
    		    {
    		        // Store right nal length size that will be used to parse all other nals
                    m_nNalLength = (codecpar->extradata[4] & 0x03) + 1;
    		    }
    		}
    	}
    	else if (codecpar->codec_id == AV_CODEC_ID_HEVC)
    	{
    	    if (codecpar->extradata && codecpar->extradata_size > 8)
    		{
    		    if (codecpar->extradata[0] || codecpar->extradata[1] || codecpar->extradata[2] > 1)
    		    {
                    m_nNalLength = 4;
    		    }
    		}
    	}
	}

	return TRUE;
}


void CFileDemux::uninit()
{
	flushAudio();
	flushVideo();
	
	if (m_pACodecCtx)
	{
		avcodec_free_context(&m_pACodecCtx);
	}
	
	if (m_pVCodecCtx)
	{
		avcodec_free_context(&m_pVCodecCtx);
	}

	if (m_pVideoFrame)
	{
	    av_frame_free(&m_pVideoFrame);
	}

	if (m_pAudioFrame)
	{
	    av_frame_free(&m_pAudioFrame);
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

	if (m_pFormatContext)	
	{
		avformat_close_input(&m_pFormatContext);
	}
}

int CFileDemux::getWidth()
{
	if (m_nVideoIndex != -1)
	{
		return m_pFormatContext->streams[m_nVideoIndex]->codecpar->width;
	}

	return 0;
}

int CFileDemux::getHeight()
{
	if (m_nVideoIndex != -1)
	{
		return m_pFormatContext->streams[m_nVideoIndex]->codecpar->height;
	}

	return 0;
}

int CFileDemux::getFramerate() 
{
	int framerate = 25;
	
	if (m_nVideoIndex != -1)
	{
		if (m_pFormatContext->streams[m_nVideoIndex]->r_frame_rate.den > 0)
		{
			framerate = m_pFormatContext->streams[m_nVideoIndex]->r_frame_rate.num / 
						(double)m_pFormatContext->streams[m_nVideoIndex]->r_frame_rate.den;
		}
	}

	return framerate;
}

int CFileDemux::getSamplerate()
{
	if (m_nAudioIndex != -1)
	{
		return m_pFormatContext->streams[m_nAudioIndex]->codecpar->sample_rate;
	}

	return 8000;
}

int CFileDemux::getChannels()
{
	if (m_nAudioIndex != -1)
	{
		return m_pFormatContext->streams[m_nAudioIndex]->codecpar->channels;
	}

	return 2;
}

void CFileDemux::flushVideo()
{
	if (NULL == m_pVCodecCtx || 
		NULL == m_pVCodecCtx->codec || 
		!(m_pVCodecCtx->codec->capabilities | AV_CODEC_CAP_DELAY))
	{
		return;
	}

    videoRecodec(NULL);
}

void CFileDemux::flushAudio()
{
	if (NULL == m_pACodecCtx || 
		NULL == m_pACodecCtx->codec || 
		!(m_pACodecCtx->codec->capabilities | AV_CODEC_CAP_DELAY))
	{
		return;
	}
	
	audioRecodec(NULL);
}

/**
 * set audio output parameters
 *
 * @param codec audio output codec
 * @param samplerate audio output sample rate
 * @param channels audio output channels
 * @return TRUE on success, FALSE on error
 */
BOOL CFileDemux::setAudioFormat(int codec, int samplerate, int channels, int bitrate)
{
	if (m_nAudioIndex == -1)
	{
		return FALSE;
	}

    AVCodecParameters * codecpar = m_pFormatContext->streams[m_nAudioIndex]->codecpar;
    
#if 0				
	if (codecpar->codec_id != CAudioEncoder::toAVCodecID(codec) ||
		codecpar->channels != channels ||
		codecpar->sample_rate != samplerate ||
		codecpar->sample_fmt != AV_SAMPLE_FMT_S16)
#else
	if (1)
#endif		
	{
		AVCodec * pCodec = avcodec_find_decoder(codecpar->codec_id);
		if (pCodec == NULL)
		{
			log_print(HT_LOG_ERR, "%s, avcodec_find_decoder failed, %d\r\n", __FUNCTION__, codecpar->codec_id);
			return FALSE;
		}

		m_pACodecCtx = avcodec_alloc_context3(pCodec);
		if (NULL == m_pACodecCtx)
		{
		    log_print(HT_LOG_ERR, "%s, avcodec_alloc_context3 failed\r\n", __FUNCTION__);
			return FALSE;
		}

		// Copy codec parameters from input stream to output codec context
        if (avcodec_parameters_to_context(m_pACodecCtx, codecpar) < 0) 
        {
            log_print(HT_LOG_ERR, "%s, failed to copy codec parameters to decoder context\r\n", __FUNCTION__);
            return FALSE;
        }

		if (avcodec_thread_open(m_pACodecCtx, pCodec, NULL) != 0)
		{
			log_print(HT_LOG_ERR, "%s, avcodec_thread_open failed\r\n", __FUNCTION__);
			return FALSE;
		}

		m_pAudioFrame = av_frame_alloc();
    	if (NULL == m_pAudioFrame)
    	{	
    	    log_print(HT_LOG_ERR, "%s, av_frame_alloc failed\r\n", __FUNCTION__);
    		return FALSE;
    	}

		m_pAudioEncoder = new CAudioEncoder;
		if (NULL == m_pAudioEncoder)
		{
			return FALSE;
		}

		AudioEncoderParam params;
		memset(&params, 0, sizeof(params));
		
		params.SrcChannels = codecpar->channels;
		params.SrcSamplerate = codecpar->sample_rate;
		params.SrcSamplefmt = (AVSampleFormat)codecpar->format;
		params.DstChannels = channels;
		params.DstSamplefmt = AV_SAMPLE_FMT_S16;
		params.DstSamplerate = samplerate;
		params.DstBitrate = bitrate;
		params.DstCodec = codec;
		
		if (FALSE == m_pAudioEncoder->init(&params))
		{
			return FALSE;
		}

		m_pAudioEncoder->addCallback(AudioCallback, this);
	}

	return TRUE;
}

BOOL CFileDemux::setVideoFormat(int codec, int width, int height, int framerate, int bitrate)
{
	if (m_nVideoIndex == -1)
	{
		return FALSE;
	}

    AVCodecParameters * codecpar = m_pFormatContext->streams[m_nVideoIndex]->codecpar;
    
	if (codecpar->codec_id != CVideoEncoder::toAVCodecID(codec) || 
		codecpar->width != width ||
		codecpar->height != height)
	{
		AVCodec * pCodec = avcodec_find_decoder(codecpar->codec_id);
		if (pCodec == NULL)
		{
			log_print(HT_LOG_ERR, "avcodec_find_decoder failed, %d\r\n", codecpar->codec_id);
			return FALSE;
		}

		m_pVCodecCtx = avcodec_alloc_context3(pCodec);
		if (NULL == m_pVCodecCtx)
		{
		    log_print(HT_LOG_ERR, "avcodec_alloc_context3 failed\r\n");
			return FALSE;
		}

		// Copy codec parameters from input stream to output codec context
        if (avcodec_parameters_to_context(m_pVCodecCtx, codecpar) < 0) 
        {
            log_print(HT_LOG_ERR, "failed to copy codec parameters to decoder context\r\n");
            return FALSE;
        }

		if (avcodec_thread_open(m_pVCodecCtx, pCodec, NULL) != 0)
		{
			log_print(HT_LOG_ERR, "avcodec_thread_open failed, video decoder\r\n");
			return FALSE;
		}

		m_pVideoFrame = av_frame_alloc();
    	if (NULL == m_pVideoFrame)
    	{	
    	    log_print(HT_LOG_ERR, "%s, av_frame_alloc failed\r\n", __FUNCTION__);
    		return FALSE;
    	}

		m_pVideoEncoder = new CVideoEncoder;		
		if (NULL == m_pVideoEncoder)
		{
			return FALSE;
		}

		VideoEncoderParam params;
		memset(&params, 0, sizeof(params));
		
		params.SrcWidth = codecpar->width;
		params.SrcHeight = codecpar->height;
		params.SrcPixFmt = (AVPixelFormat)codecpar->format;
		params.DstCodec = codec;
		params.DstWidth = width;
		params.DstHeight = height;
		params.DstFramerate = framerate;
		params.DstBitrate = bitrate;
		
		if (FALSE == m_pVideoEncoder->init(&params))
		{
			return FALSE;
		}

		m_pVideoEncoder->addCallback(VideoCallback, this);
	}
	else if (codecpar->codec_id == AV_CODEC_ID_H264)
	{
		const AVBitStreamFilter * bsfc = av_bsf_get_by_name("h264_mp4toannexb");
		if (bsfc) 
		{
			int ret;
			
			AVBSFContext *bsf;
			av_bsf_alloc(bsfc, &bsf);

			ret = avcodec_parameters_copy(bsf->par_in, codecpar);
	        if (ret < 0)
	        {
	           return FALSE;
			}
			
	        ret = av_bsf_init(bsf);
	        if (ret < 0)
	        {
	            return FALSE;
			}
			
	        ret = avcodec_parameters_copy(codecpar, bsf->par_out);
	        if (ret < 0)
	        {
	            return FALSE;
			}
			
			av_bsf_free(&bsf);	            
		}
	}	
	else if (codecpar->codec_id == AV_CODEC_ID_HEVC)
	{
		const AVBitStreamFilter * bsfc = av_bsf_get_by_name("hevc_mp4toannexb");
		if (bsfc) 
		{
			int ret;

			AVBSFContext *bsf;
			av_bsf_alloc(bsfc, &bsf);

			ret = avcodec_parameters_copy(bsf->par_in, codecpar);
	        if (ret < 0)
	        {
	           return FALSE;
			}
			
	        ret = av_bsf_init(bsf);
	        if (ret < 0)
	        {
	            return FALSE;
			}
			
	        ret = avcodec_parameters_copy(codecpar, bsf->par_out);
	        if (ret < 0)
	        {
	            return FALSE;
			}
			
			av_bsf_free(&bsf);	            
		}
	}

	return TRUE;
}

void CFileDemux::videoRecodec(AVPacket * pkt)
{
    int ret = avcodec_send_packet(m_pVCodecCtx, pkt);
	if (ret < 0) 
	{       
		log_print(HT_LOG_ERR, "%s, error sending a packet for decoding\r\n", __FUNCTION__); 
		return;   
	}  
	
	while (ret >= 0)
	{        
		ret = avcodec_receive_frame(m_pVCodecCtx, m_pVideoFrame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			return; 
		}		
		else if (ret < 0)
		{     
			log_print(HT_LOG_ERR, "%s, error during decoding\r\n", __FUNCTION__);
			return;     
		}

		m_pVideoEncoder->encode(m_pVideoFrame);	
	}
}

void CFileDemux::audioRecodec(AVPacket * pkt)
{
    int ret = avcodec_send_packet(m_pACodecCtx, pkt);
	if (ret < 0) 
	{       
		log_print(HT_LOG_ERR, "%s, error sending a packet for decoding\r\n", __FUNCTION__); 
		return;   
	}  
	
	while (ret >= 0)
	{        
		ret = avcodec_receive_frame(m_pACodecCtx, m_pAudioFrame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			return; 
		}		
		else if (ret < 0)
		{     
			log_print(HT_LOG_ERR, "%s, error during decoding\r\n", __FUNCTION__);
			return;     
		}

		m_pAudioEncoder->encode(m_pAudioFrame);	
	}
}

BOOL CFileDemux::readFrame()
{
    int rret = 0;
	AVPacket pkt;
	
	av_init_packet(&pkt);
	pkt.data = 0;
	pkt.size = 0;

	rret = av_read_frame(m_pFormatContext, &pkt);
    if (AVERROR_EOF == rret)
    {
        if (++m_nLoopNums >= m_nMaxLoopNums)
        {
            return FALSE;
        }
        
		rret = av_seek_frame(m_pFormatContext, 0, m_pFormatContext->streams[0]->start_time, 0);
        if (rret < 0)
        {
            rret = av_seek_frame(m_pFormatContext, 0, 0, AVSEEK_FLAG_BYTE | AVSEEK_FLAG_BACKWARD);
            if (rret < 0)
            {
                return FALSE;
            }
        }

        // avcodec_flush_buffers();
        
        if (av_read_frame(m_pFormatContext, &pkt) != 0)
        {
            return FALSE;
        }
    }	
	else if (0 != rret)
	{
		return FALSE;
	}
	
	if (pkt.stream_index == m_nVideoIndex)
	{
	    AVRational q = {1, AV_TIME_BASE};
        
        if (pkt.pts != AV_NOPTS_VALUE)
		{
			m_nCurPos = av_rescale_q (pkt.pts, m_pFormatContext->streams[m_nVideoIndex]->time_base, q);

			if (m_pFormatContext->start_time != AV_NOPTS_VALUE)
            {
                m_nCurPos -= m_pFormatContext->start_time;
            }

            m_nCurPos /= 1000;
		}
		else if (pkt.dts != AV_NOPTS_VALUE)
		{
			m_nCurPos = av_rescale_q (pkt.dts, m_pFormatContext->streams[m_nVideoIndex]->time_base, q);

			if (m_pFormatContext->start_time != AV_NOPTS_VALUE)
            {
                m_nCurPos -= m_pFormatContext->start_time;
            }

            m_nCurPos /= 1000;
		}
    		
		if (NULL == m_pVideoEncoder)
		{
		    AVCodecParameters * codecpar = m_pFormatContext->streams[m_nVideoIndex]->codecpar;
		    
			if (codecpar->codec_id == AV_CODEC_ID_H264 || codecpar->codec_id == AV_CODEC_ID_HEVC)
			{
			    if (m_bFirst)
			    {
			    	m_bFirst = FALSE;
			    	
			        if (codecpar->extradata_size > 8)
			        {
			            dataCallback(codecpar->extradata, codecpar->extradata_size, DATA_TYPE_VIDEO, 0, 0);
			        }
			    }
			    
                if (m_nNalLength)
                {
                    while (pkt.size >= m_nNalLength)
					{
						int len = 0;
						int nal_length = m_nNalLength;
						uint8 * data = pkt.data;
						
						while (nal_length--)
						{
                            len = (len << 8) | *data++;
        				}
        				
						if (len > pkt.size - m_nNalLength || len <= 0)
						{
						    log_print(HT_LOG_DBG, "len=%d, pkt.size=%d\r\n", len, pkt.size);
							break;
						}

						nal_length = m_nNalLength;
						pkt.data[nal_length-1] = 1;
						nal_length--;
						while (nal_length--)
						{
						    pkt.data[nal_length] = 0;
						}
						
						dataCallback(pkt.data, len+m_nNalLength, DATA_TYPE_VIDEO, 0, pkt.size - len - m_nNalLength >= m_nNalLength ? 0 : 1);

						pkt.data += len + m_nNalLength;
						pkt.size -= len + m_nNalLength;
					}	
                }                
				else if (pkt.data[0] == 0 && pkt.data[1] == 0 && pkt.data[2] == 0 && pkt.data[3] == 1)
				{
					dataCallback(pkt.data, pkt.size, DATA_TYPE_VIDEO, 0, 1);
				}
				else if (pkt.data[0] == 0 && pkt.data[1] == 0 && pkt.data[2] == 1)
				{
					dataCallback(pkt.data, pkt.size, DATA_TYPE_VIDEO, 0, 1);
				}
				else 
				{
					log_print(HT_LOG_ERR, "%s, unknown format\r\n", __FUNCTION__);
				}
			}
			else
			{
				dataCallback(pkt.data, pkt.size, DATA_TYPE_VIDEO, 0, 1);				
			}
		}
		else 
		{
			videoRecodec(&pkt);
		}
	}
	else if (pkt.stream_index == m_nAudioIndex)
	{
	    AVRational q = {1, AV_TIME_BASE};
        
        if (pkt.pts != AV_NOPTS_VALUE)
		{
			m_nCurPos = av_rescale_q (pkt.pts, m_pFormatContext->streams[m_nAudioIndex]->time_base, q);

			if (m_pFormatContext->start_time != AV_NOPTS_VALUE)
            {
                m_nCurPos -= m_pFormatContext->start_time;
            }

            m_nCurPos /= 1000;
		}
		else if (pkt.dts != AV_NOPTS_VALUE)
		{
			m_nCurPos = av_rescale_q (pkt.dts, m_pFormatContext->streams[m_nAudioIndex]->time_base, q);

			if (m_pFormatContext->start_time != AV_NOPTS_VALUE)
            {
                m_nCurPos -= m_pFormatContext->start_time;
            }

            m_nCurPos /= 1000;
		}
		
		if (NULL == m_pAudioEncoder)
		{
			dataCallback(pkt.data, pkt.size, DATA_TYPE_AUDIO, 0, 1);
		}
		else
		{
			audioRecodec(&pkt);
		}
	}

	av_packet_unref(&pkt);

	return TRUE;
}

char * CFileDemux::getH264AuxSDPLine(int rtp_pt)
{
	uint8 * sps = NULL; uint32 spsSize = 0;
	uint8 * pps = NULL; uint32 ppsSize = 0;

    AVCodecParameters * codecpar = m_pFormatContext->streams[m_nVideoIndex]->codecpar;
    
	if (codecpar->extradata_size <= 8)
	{
		return NULL;
	}

	uint8 *r, *end = codecpar->extradata + codecpar->extradata_size;
	r = avc_find_startcode(codecpar->extradata, end);

	while (r < end) 
	{
        uint8 *r1;
        
        while (!*(r++));
        r1 = avc_find_startcode(r, end);

        int nal_type = (r[0] & 0x1F);
        
        if (H264_NAL_PPS == nal_type)
        {
            pps = r;
            ppsSize = r1 - r;
        }
        else if (H264_NAL_SPS == nal_type)
        {
            sps = r;
            spsSize = r1 - r;
        }
		
        r = r1;
    }

	if (NULL == sps || spsSize == 0 ||
		NULL == pps || ppsSize == 0)
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

char * CFileDemux::getH265AuxSDPLine(int rtp_pt)
{
	uint8* vps = NULL; uint32 vpsSize = 0;
	uint8* sps = NULL; uint32 spsSize = 0;
	uint8* pps = NULL; uint32 ppsSize = 0;

    AVCodecParameters * codecpar = m_pFormatContext->streams[m_nVideoIndex]->codecpar;
    
	if (codecpar->extradata_size < 23)
	{
		return NULL;
	}

	uint8 *r, *end = codecpar->extradata + codecpar->extradata_size;
	r = avc_find_startcode(codecpar->extradata, end);

	while (r < end) 
	{
        uint8 *r1;
        
        while (!*(r++));
        r1 = avc_find_startcode(r, end);

        int nal_type = (r[0] >> 1) & 0x3F;
        
        if (HEVC_NAL_VPS == nal_type)
        {
            vps = r;
            vpsSize = r1 - r;
        }
        else if (HEVC_NAL_PPS == nal_type)
        {
            pps = r;
            ppsSize = r1 - r;
        }
        else if (HEVC_NAL_SPS == nal_type)
        {
            sps = r;
            spsSize = r1 - r;
        }
		
        r = r1;
    }
    
	if (NULL == vps || vpsSize == 0 ||
		NULL == sps || spsSize == 0 ||
		NULL == pps || ppsSize == 0)
	{
		return NULL;
	}

	// Set up the "a=fmtp:" SDP line for this stream.
	uint8* vpsWEB = new uint8[vpsSize]; // "WEB" means "Without Emulation Bytes"
	uint32 vpsWEBSize = remove_emulation_bytes(vpsWEB, vpsSize, vps, vpsSize);
	if (vpsWEBSize < 6/*'profile_tier_level' offset*/ + 12/*num 'profile_tier_level' bytes*/)
	{
		// Bad VPS size => assume our source isn't ready
		delete[] vpsWEB;
		return NULL;
	}
	
	uint8 const* profileTierLevelHeaderBytes = &vpsWEB[6];
	uint32 profileSpace  = profileTierLevelHeaderBytes[0]>>6; // general_profile_space
	uint32 profileId = profileTierLevelHeaderBytes[0]&0x1F; 	// general_profile_idc
	uint32 tierFlag = (profileTierLevelHeaderBytes[0]>>5)&0x1;// general_tier_flag
	uint32 levelId = profileTierLevelHeaderBytes[11]; 		// general_level_idc
	uint8 const* interop_constraints = &profileTierLevelHeaderBytes[5];
	char interopConstraintsStr[100];
	sprintf(interopConstraintsStr, "%02X%02X%02X%02X%02X%02X", 
	interop_constraints[0], interop_constraints[1], interop_constraints[2],
	interop_constraints[3], interop_constraints[4], interop_constraints[5]);
	delete[] vpsWEB;

	char* sprop_vps = new char[vpsSize*2+1];
  	char* sprop_sps = new char[spsSize*2+1];
  	char* sprop_pps = new char[ppsSize*2+1];

	base64_encode(vps, vpsSize, sprop_vps, vpsSize*2+1);
  	base64_encode(sps, spsSize, sprop_sps, spsSize*2+1);
	base64_encode(pps, ppsSize, sprop_pps, ppsSize*2+1);

	char const* fmtpFmt =
		"a=fmtp:%d profile-space=%u"
		";profile-id=%u"
		";tier-flag=%u"
		";level-id=%u"
		";interop-constraints=%s"
		";sprop-vps=%s"
		";sprop-sps=%s"
		";sprop-pps=%s";
		
	uint32 fmtpFmtSize = strlen(fmtpFmt)
		+ 3 /* max num chars: rtpPayloadType */ + 20 /* max num chars: profile_space */
		+ 20 /* max num chars: profile_id */
		+ 20 /* max num chars: tier_flag */
		+ 20 /* max num chars: level_id */
		+ strlen(interopConstraintsStr)
		+ strlen(sprop_vps)
		+ strlen(sprop_sps)
		+ strlen(sprop_pps);
		
	char* fmtp = new char[fmtpFmtSize+1];
	memset(fmtp, 0, fmtpFmtSize+1);
	
	sprintf(fmtp, fmtpFmt,
	  	rtp_pt, profileSpace,
		profileId,
		tierFlag,
		levelId,
		interopConstraintsStr,
		sprop_vps,
		sprop_sps,
		sprop_pps);

	delete[] sprop_vps;
	delete[] sprop_sps;
	delete[] sprop_pps;

	return fmtp;
}

char * CFileDemux::getMP4AuxSDPLine(int rtp_pt)
{
    AVCodecParameters * codecpar = m_pFormatContext->streams[m_nVideoIndex]->codecpar;
    
	if (codecpar->extradata_size == 0)
	{
		return NULL;
	}
	
	char const* fmtpFmt =
		"a=fmtp:%d "
		"profile-level-id=%d;"
		"config=";
	uint32 fmtpFmtSize = strlen(fmtpFmt)
		+ 3 /* max char len */
		+ 3 /* max char len */
		+ 2*codecpar->extradata_size; /* 2*, because each byte prints as 2 chars */

	char* fmtp = new char[fmtpFmtSize+1];
	memset(fmtp, 0, fmtpFmtSize+1);
	
	sprintf(fmtp, fmtpFmt, rtp_pt, 1);
	char* endPtr = &fmtp[strlen(fmtp)];
	for (int i = 0; i < codecpar->extradata_size; ++i) 
	{
		sprintf(endPtr, "%02X", codecpar->extradata[i]);
		endPtr += 2;
	}

	return fmtp;
}

char * CFileDemux::getVideoAuxSDPLine(int rtp_pt)
{
	if (m_pVideoEncoder)
	{
		return m_pVideoEncoder->getAuxSDPLine(rtp_pt);
	}

	if (m_nVideoIndex < 0)
	{
	    return NULL;
	}

	AVCodecParameters * codecpar = m_pFormatContext->streams[m_nVideoIndex]->codecpar;
	
	if (codecpar->codec_id == AV_CODEC_ID_H264)
	{
		return getH264AuxSDPLine(rtp_pt);		
	}
	else if (codecpar->codec_id == AV_CODEC_ID_MPEG4)
	{
		return getMP4AuxSDPLine(rtp_pt);
	}
	else if (codecpar->codec_id == AV_CODEC_ID_HEVC)
	{
		return getH265AuxSDPLine(rtp_pt);
	}

	return NULL;  	
}

char * CFileDemux::getAACAuxSDPLine(int rtp_pt)
{
    AVCodecParameters * codecpar = m_pFormatContext->streams[m_nAudioIndex]->codecpar;
    
	if (codecpar->extradata_size == 0)
	{
		return NULL;
	}
	
	char const* fmtpFmt =
        "a=fmtp:%d "
    	"streamtype=5;profile-level-id=1;"
    	"mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;"
    	"config="; /* streamtype, 4 : video, 5 : audio */
	uint32 fmtpFmtSize = strlen(fmtpFmt)
	    + 3 /* max char len */
	    + 2*codecpar->extradata_size; /* 2*, because each byte prints as 2 chars */

	char* fmtp = new char[fmtpFmtSize+1];
	memset(fmtp, 0, fmtpFmtSize+1);
	
	sprintf(fmtp, fmtpFmt, rtp_pt);
	char* endPtr = &fmtp[strlen(fmtp)];
	for (int i = 0; i < codecpar->extradata_size; ++i) 
	{
		sprintf(endPtr, "%02X", codecpar->extradata[i]);
		endPtr += 2;
	}

	return fmtp;
}

char * CFileDemux::getAudioAuxSDPLine(int rtp_pt)
{
	if (m_pAudioEncoder)
	{
		return m_pAudioEncoder->getAuxSDPLine(rtp_pt);
	}

	if (m_nAudioIndex < 0)
	{
	    return NULL;
	}

	AVCodecParameters * codecpar = m_pFormatContext->streams[m_nAudioIndex]->codecpar;
	
	if (codecpar->codec_id == AV_CODEC_ID_AAC)
	{
		return getAACAuxSDPLine(rtp_pt);		
	}

	return NULL;  	
}

BOOL CFileDemux::seekStream(double pos)
{
    if (pos < 0)
    {
        return FALSE;
    }

    if (pos == m_nCurPos)
    {
    	return TRUE;
    }

    int stream = -1;
    int64 seekpos = pos * 1000000;
    
    if (m_nVideoIndex >= 0)
    {
        stream = m_nVideoIndex;
    }    
    else if (m_nAudioIndex >= 0)
    {
        stream = m_nAudioIndex;
    }

    if (m_pFormatContext->start_time != AV_NOPTS_VALUE)
    {
        seekpos += m_pFormatContext->start_time;
    }

    if (stream >= 0)
    {
        AVRational q = {1, AV_TIME_BASE}; 
        
        seekpos = av_rescale_q(seekpos, q, m_pFormatContext->streams[stream]->time_base);
    }

    if (av_seek_frame(m_pFormatContext, stream, pos, AVSEEK_FLAG_BACKWARD) < 0)
    { 
        return FALSE;
    }

    // Accurate seek to the specified position
    AVPacket pkt;
	
	av_init_packet(&pkt);
	pkt.data = 0;
	pkt.size = 0;
	
    while (av_read_frame(m_pFormatContext, &pkt) == 0)
    {
		if (pkt.stream_index != stream)
		{
			av_packet_unref(&pkt);
			continue;
		}
		
        if (pkt.pts != AV_NOPTS_VALUE)
		{
			if (pkt.pts < seekpos)
			{
				av_packet_unref(&pkt);
				continue;
			}
			else
			{
			    break;
			}
		}
		else if (pkt.dts != AV_NOPTS_VALUE)
		{
			if (pkt.dts < seekpos)
			{
				av_packet_unref(&pkt);
				continue;
			}
			else
			{
			    break;
			}
		}
		else
		{
			break;
		}
        
        av_packet_unref(&pkt);
    }

	av_packet_unref(&pkt);
	
    m_nCurPos = pos * 1000;

    return TRUE;
}

void CFileDemux::dataCallback(uint8 * data, int size, int type, int nbsamples, BOOL waitnext)
{
	if (m_pCallback)
	{
		m_pCallback(data, size, type, nbsamples, waitnext, m_pUserdata);
	}
}

void CFileDemux::setCallback(DemuxCallback pCallback, void * pUserdata)
{
	m_pCallback = pCallback;
	m_pUserdata = pUserdata;
}



