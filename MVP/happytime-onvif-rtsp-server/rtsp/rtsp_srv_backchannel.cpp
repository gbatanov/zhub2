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
#include "rtsp_srv_backchannel.h"
#include "rtp.h"
#include "pcm_rtp_rx.h"
#include "aac_rtp_rx.h"

#ifdef RTSP_BACKCHANNEL

#if __WINDOWS_OS__
#include "audio_play_dsound.h"
#endif

#if __LINUX_OS__
#include "audio_play_linux.h"
#endif

#ifndef MEDIA_LIVE
void rtsp_bc_decoder_cb(AVFrame * frame, void * puser)
{
    RSUA * p_rua = (RSUA *) puser;

    if (p_rua->audio_player)
	{
		p_rua->audio_player->playAudio(frame->data[0], frame->nb_samples * frame->channels * av_get_bytes_per_sample((enum AVSampleFormat)frame->format));
	}
}
#endif

int rtsp_bc_data_cb(uint8 * p_data, int len, uint32 ts, uint32 seq, void * p_userdata)
{
    RSUA * p_rua = (RSUA *)p_userdata;

#ifdef MEDIA_LIVE
    // todo : add your handler ...
        
#else
    if (p_rua->ad_inited && p_rua->audio_decoder)
	{
		p_rua->audio_decoder->decode(p_data, len);
	}
#endif

    return 0;
}

BOOL rtsp_bc_init_player(RSUA * p_rua, int samplerate, int channels)
{
#ifdef MEDIA_LIVE
    // todo : add your handler ...
    
	return TRUE;
#else
	if (p_rua->audio_player == NULL)
	{
#if __WINDOWS_OS__	
		p_rua->audio_player = new CDSoundAudioPlay(samplerate, channels);
#endif
#if __LINUX_OS__
		p_rua->audio_player = new CLAudioPlay(samplerate, channels);
#endif
	}

	if (p_rua->audio_player == NULL)
	{
	    return FALSE;
	}

	p_rua->audio_player->setVolume(255);
	
	return p_rua->audio_player->startPlay();
#endif	
}

BOOL rtsp_bc_init_audio(RSUA * p_rua)
{
    BOOL ret = FALSE;

#ifdef MEDIA_LIVE
    // todo : add your handler ...
    
    ret = TRUE;
#else    
    p_rua->audio_decoder = new CAudioDecoder();
	if (p_rua->audio_decoder)
	{	
		ret = p_rua->audio_decoder->init(p_rua->bc_codec, p_rua->bc_samplerate, p_rua->bc_channels, NULL, 0);
	}

	if (ret)
	{
		p_rua->audio_decoder->setCallback(rtsp_bc_decoder_cb, p_rua);
		
		ret = rtsp_bc_init_player(p_rua, p_rua->bc_samplerate, p_rua->bc_channels);
	}
#endif

	if (ret)
	{
	    aac_rxi_init(&p_rua->aacrxi, rtsp_bc_data_cb, p_rua);

    	p_rua->aacrxi.size_length = 13;
    	p_rua->aacrxi.index_length = 3;
    	p_rua->aacrxi.index_delta_length = 3;
	
	    p_rua->ad_inited = 1;
	}
	else
	{
	    rtsp_bc_uninit_audio(p_rua);
	}

	return ret;
}

void rtsp_bc_uninit_audio(RSUA * p_rua)
{
	if (NULL == p_rua)
	{
		return;
	}

#ifdef MEDIA_LIVE
    // todo : add your handler ...
    
#else
	if (p_rua->audio_decoder)
	{
		delete p_rua->audio_decoder;
		p_rua->audio_decoder = NULL;
	}

	if (p_rua->audio_player)
	{
		delete p_rua->audio_player;
		p_rua->audio_player = NULL;
	}
#endif

	aac_rxi_deinit(&p_rua->aacrxi);

	p_rua->ad_inited = 0;
}

void rtsp_bc_tcp_data_rx(RSUA * p_rua, uint8 * lpData, int rlen)
{
	RILF * p_rilf = (RILF *)lpData;
	uint8 * p_rtp = (uint8 *)p_rilf + 4;
	uint32	rtp_len = rlen - 4;

	if (p_rilf->channel != p_rua->channels[AV_BACK_CH].interleaved)
	{
	    return;
	}
	
	if (AUDIO_CODEC_G726  == p_rua->bc_codec || 
	    AUDIO_CODEC_G711A == p_rua->bc_codec || 
	    AUDIO_CODEC_G711U == p_rua->bc_codec || 
	    AUDIO_CODEC_G722  == p_rua->bc_codec || 
	    AUDIO_CODEC_OPUS  == p_rua->bc_codec)
	{
	    pcm_rtp_rx(&p_rua->pcmrxi, p_rtp, rtp_len);
	}
	else if (AUDIO_CODEC_AAC == p_rua->bc_codec)
	{
	    aac_rtp_rx(&p_rua->aacrxi, p_rtp, rtp_len);
	}
	else
	{
		log_print(HT_LOG_ERR, "%s, unsupport audio codec\r\n", __FUNCTION__, p_rua->bc_codec);
	}
}

void rtsp_bc_udp_data_rx(RSUA * p_rua, uint8 * lpData, int rlen)
{
	if (AUDIO_CODEC_G726  == p_rua->bc_codec || 
	    AUDIO_CODEC_G711A == p_rua->bc_codec || 
	    AUDIO_CODEC_G711U == p_rua->bc_codec || 
	    AUDIO_CODEC_G722  == p_rua->bc_codec || 
	    AUDIO_CODEC_OPUS  == p_rua->bc_codec)
	{
	    pcm_rtp_rx(&p_rua->pcmrxi, lpData, rlen);
	}
	else if (AUDIO_CODEC_AAC == p_rua->bc_codec)
	{
	    aac_rtp_rx(&p_rua->aacrxi, lpData, rlen);
	}
	else
	{
		log_print(HT_LOG_ERR, "%s, unsupport audio codec\r\n", __FUNCTION__, p_rua->bc_codec);
	}
}

void * rtsp_bc_udp_rx_thread(void * argv)
{
	fd_set fdr;
	RSUA * p_rua = (RSUA *)argv;
	int fd = p_rua->channels[AV_BACK_CH].udp_fd;
	
	while (p_rua->rtp_rx)
	{
		FD_ZERO(&fdr);
		FD_SET(fd, &fdr);

		struct timeval tv = {1, 0};

		int sret = select(fd+1, &fdr, NULL, NULL, &tv);
		if (sret == 0)
		{
			continue;
	    }
	    else if (sret < 0)
	    {
	    	usleep(10*1000);
	    	continue;
	    }

	    if (!FD_ISSET(fd, &fdr))
	    {
	    	continue;
	    }

	    int addr_len;
	    char buf[2048];
	    struct sockaddr_in addr;
	    
		memset(&addr, 0, sizeof(addr));
		addr_len = sizeof(struct sockaddr_in);

		int rlen = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&addr, (socklen_t*)&addr_len);
    	if (rlen <= 12)
    	{
    		log_print(HT_LOG_ERR, "%s, recvfrom return %d,err[%s]!!!\r\n", __FUNCTION__, rlen, sys_os_get_socket_error());
    	}
    	else
    	{
    	    rtsp_bc_udp_data_rx(p_rua, (uint8*)buf, rlen);
    	}
	}

	p_rua->tid_udp_rx = 0;

    log_print(HT_LOG_DBG, "%s, exit!!!\r\n", __FUNCTION__);
	
	return NULL;
}

BOOL rtsp_bc_parse_url_parameters(RSUA * p_rua)
{
	char value[32] = {'\0'};
	char * p = strchr(p_rua->media_info.filename, '&');	
	if (NULL == p)
	{
		return FALSE;
	}

	p++; // skip '&' char

	if (GetNameValuePair(p, strlen(p), "bce", value, sizeof(value)))
	{
		if (strcasecmp(value, "G711") == 0 || strcasecmp(value, "PCMU") == 0)
		{
			p_rua->bc_codec = AUDIO_CODEC_G711U;
		}
		else if (strcasecmp(value, "G726") == 0)
		{
			p_rua->bc_codec = AUDIO_CODEC_G726;
		}
		else if (strcasecmp(value, "AAC") == 0 || strcasecmp(value, "MP4A-LATM") == 0)
		{
			p_rua->bc_codec = AUDIO_CODEC_AAC;
		}
	}

	return TRUE;
}

#endif // end of RTSP_BACKCHANNEL


