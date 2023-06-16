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
#include "sys_buf.h"
#include "word_analyse.h"
#include "rtsp_parse.h"
#include "rtsp_rsua.h"
#include "rtsp_srv.h"
#include "rtsp_stream.h"
#include "rtsp_cfg.h"
#include "rtsp_util.h"

#ifdef RTSP_BACKCHANNEL
#include "rtsp_srv_backchannel.h"
#endif
#ifdef RTSP_OVER_HTTP
#include "http.h"
#endif

/***********************************************************************/

extern RTSP_CLASS	hrtsp;


/***********************************************************************/

BOOL rua_get_transport_info(RSUA * p_rua, char * transport_buf, int av_t)
{
	char * p_s = transport_buf;
	char * p_e = p_s;

	while (*p_e != ';' && *p_e != '\0')
	{
		p_e++;
	}
	
	if (*p_e == '\0')
	{
		return FALSE;
	}
	
	*p_e = '\0';
	
	if (strcasecmp(p_s, "RTP/AVP") == 0 || strcasecmp(p_s, "RTP/AVP/UDP") == 0)
	{
		p_rua->rtp_tcp = 0;
	}	
	else if (strcasecmp(p_s, "RTP/AVP/TCP") == 0)
	{
		p_rua->rtp_tcp = 1;
	}
	
	p_e++; 
	p_s = p_e;
	
	while (*p_e != ';' && *p_e != '\0')
	{
		p_e++;
	}
	
	if (*p_e == '\0')
	{
		return FALSE;
	}
	
	*p_e = '\0';
	
	if (strcasecmp(p_s, "unicast") == 0)
	{
		p_rua->rtp_unicast = 1;
	}
	else if (strcasecmp(p_s, "multicast") == 0)
	{
		p_rua->rtp_unicast = 0;
	}
	else
	{
		return FALSE;
	}
	
	p_e++; 
	p_s = p_e;
	
	while (*p_e != ';' && *p_e != '\0')
	{
		p_e++;
	}
	
	if (p_rua->rtp_tcp == 1)
	{
		*p_e = '\0';

		int il_len = strlen("interleaved=");
		if (memcmp(p_s, "interleaved=", il_len) != 0)
		{
			return FALSE;
		}
		
		p_s += il_len; 
		p_e = p_s;
		
		while (*p_e != '-' && *p_e != '\0')
		{
			p_e++;
		}
		
		if (*p_e == '\0')
		{
			return FALSE;
		}
		
		*p_e = '\0';

		if (!is_integer(p_s))
		{
			return FALSE;
		}
		
		int vv = atoi(p_s);
		if (vv > 255 || vv < 0)
		{
			return FALSE;
		}

		p_rua->channels[av_t].interleaved = vv;
	}
	else
	{
		char client_port[32];

		if (p_rua->rtp_unicast == 1)
		{
		    if (GetNameValuePair(p_s, strlen(p_s), "client_port", client_port, sizeof(client_port)-1) == FALSE)
		    {
			    return FALSE;
			}    
		}
		else
		{
		    if (GetNameValuePair(p_s, strlen(p_s), "port", client_port, sizeof(client_port)-1) == FALSE)
		    {
		    	// Transport: RTP/AVP;multicast, there is not port
			    return TRUE;
			}
		}
		
		int i = 0;
		
		while (client_port[i] != '-' && i < 31 && client_port[i] != '\0')
		{
			i++;
		}
		
		if (i >= 31)
		{
			return FALSE;
		}
		
		client_port[i] = '\0';
		
		int rport = atoi(client_port);
		if (rport >= 0xFFFF || rport < 0)
		{
			return FALSE;
		}

		p_rua->channels[av_t].r_port = rport;
	}

	return TRUE;
}

BOOL rua_get_play_range_info(RSUA * p_rua, char * range_buf)
{
    int timeType = 0; // Relative Time
    double rangeStart = 0, rangeEnd = 0;
    double start, end;
    int numCharsMatched1 = 0, numCharsMatched2 = 0, numCharsMatched3 = 0;
    
    if (sscanf(range_buf, "npt = %lf - %lf", &start, &end) == 2) 
    {
        rangeStart = start;
        rangeEnd = end;
    } 
    else if (sscanf(range_buf, "npt = %n%lf -", &numCharsMatched1, &start) == 1) 
    {
        if (range_buf[numCharsMatched1] == '-') 
        {
            // special case for "npt = -<endtime>", which matches here:
            rangeStart = 0.0;
            rangeEnd = -start;
        } 
        else 
        {
            rangeStart = start;
            rangeEnd = 0.0;
        }
    } 
    else if (sscanf(range_buf, "npt = now - %lf", &end) == 1) 
    {
        rangeStart = 0.0;
        rangeEnd = end;
    } 
    else if (sscanf(range_buf, "npt = now -%n", &numCharsMatched2) == 0 && numCharsMatched2 > 0) 
    {
        rangeStart = 0.0;
        rangeEnd = 0.0;
    }
    else if (sscanf(range_buf, "clock = %n", &numCharsMatched3) == 0 && numCharsMatched3 > 0) 
    {
        timeType = 1; // Absolute time
        rangeStart = rangeEnd = 0.0;

        char const* utcTimes = &range_buf[numCharsMatched3];
        size_t len = strlen(utcTimes) + 1;
        char* as = new char[len];
        char* ae = new char[len];
        int sscanfResult = sscanf(utcTimes, "%[^-]-%[^\r\n]", as, ae);
        if (sscanfResult == 2) 
        {
            time_t t;
        
            if (rtsp_parse_xsd_datetime(as, &t))
            {
                rangeStart = t;
            }

            if (rtsp_parse_xsd_datetime(ae, &t))
            {
                rangeEnd = t;
            }
        } 
        else if (sscanfResult == 1) 
        {
            time_t t;
        
            if (rtsp_parse_xsd_datetime(as, &t))
            {
                rangeStart = t;
            }
        } 
        else 
        {
            delete[] as; delete[] ae;
            return FALSE;
        }

        delete[] as; delete[] ae;
    } 
    else 
    {
        return FALSE; // The header is malformed
    }

    p_rua->play_range_type = timeType;
    p_rua->play_range_begin = (int64)rangeStart;
    p_rua->play_range_end = (int64)rangeEnd;

    return TRUE;	
}

#ifdef MEDIA_PUSHER

BOOL rsua_get_sdp_video_desc(RSUA * p_rua, const char * key, int * pt, char * p_sdp, int max_len)
{
    int payload_type = 0, i;
	int rtpmap_len = (int)strlen("a=rtpmap:");

	for (i=0; i<MAX_AVN; i++)
	{
		char * ptr = p_rua->channels[AV_VIDEO_CH].cap_desc[i];
		if (memcmp(ptr, "a=rtpmap:", rtpmap_len) == 0)
		{
			char pt_buf[16];
			char code_buf[64];
			int next_offset = 0;
			ptr += rtpmap_len;

			if (GetLineWord(ptr, 0, (int)strlen(ptr), pt_buf, sizeof(pt_buf), &next_offset, WORD_TYPE_NUM) == FALSE)
			{
				return FALSE;
			}
			
			GetLineWord(ptr, next_offset, (int)strlen(ptr)-next_offset, code_buf, sizeof(code_buf),  &next_offset, WORD_TYPE_STRING);
			
			if (memcmp(code_buf, key, strlen(key)) == 0)
			{
				payload_type = atoi(pt_buf);
				if (payload_type <= 0)
				{
					return FALSE;
				}
				
				break;
			}
		}
	}

	if (payload_type == 0)
	{
		return FALSE;
	}
	
    if (pt)
    {
	    *pt = payload_type;
	}

	if (p_sdp == NULL)	// not need sps, pps parameter
	{
		return TRUE;
	}
	
	p_sdp[0] = '\0';

	char fmtp_buf[32];
	int fmtp_len = sprintf(fmtp_buf, "a=fmtp:%u", payload_type);

	for (i=0; i<MAX_AVN; i++)
	{
		char * ptr = p_rua->channels[AV_VIDEO_CH].cap_desc[i];
		if (memcmp(ptr, fmtp_buf, fmtp_len) == 0)
		{
			ptr += rtpmap_len+1;
			strncpy(p_sdp, ptr, max_len);
			break;
		}
	}

	return TRUE;
}

BOOL rsua_get_sdp_audio_desc(RSUA * p_rua, const char * key, int * pt, char * p_sdp, int max_len)
{
    int payload_type = 0, i;
	int rtpmap_len = (int)strlen("a=rtpmap:");

	for (i=0; i<MAX_AVN; i++)
	{
		char * ptr = p_rua->channels[AV_AUDIO_CH].cap_desc[i];
		if (memcmp(ptr, "a=rtpmap:", rtpmap_len) == 0)
		{
			char pt_buf[16];
			char code_buf[64];
			int next_offset = 0;
			ptr += rtpmap_len;

			if (GetLineWord(ptr, 0, (int)strlen(ptr), pt_buf, sizeof(pt_buf), &next_offset, WORD_TYPE_NUM) == FALSE)
			{
				return FALSE;
			}
			
			GetLineWord(ptr, next_offset, (int)strlen(ptr)-next_offset, code_buf, sizeof(code_buf),  &next_offset, WORD_TYPE_STRING);
			
			if (memcmp(code_buf, key, strlen(key)) == 0)
			{
				payload_type = atoi(pt_buf);				
				if (payload_type <= 0)
				{
					return FALSE;
				}
				
				break;
			}
		}
	}

	if (payload_type == 0)
	{
		return FALSE;
	}
	
    if (pt)
    {
	    *pt = payload_type;
	}

	if (p_sdp == NULL)	// not need sps, pps parameter
	{
		return TRUE;
	}
	
	p_sdp[0] = '\0';

	char fmtp_buf[32];
	int fmtp_len = sprintf(fmtp_buf, "a=fmtp:%u", payload_type);

	for (i=0; i<MAX_AVN; i++)
	{
		char * ptr = p_rua->channels[AV_AUDIO_CH].cap_desc[i];
		if (memcmp(ptr, fmtp_buf, fmtp_len) == 0)
		{
			ptr += rtpmap_len+1;
			strncpy(p_sdp, ptr, max_len);
			break;
		}
	}

	return TRUE;
}

BOOL rsua_get_sdp_h264_desc(RSUA * p_rua, int * pt, char * p_sdp, int max_len)
{
    return rsua_get_sdp_video_desc(p_rua, "H264/90000", pt, p_sdp, max_len);
}

BOOL rsua_get_sdp_h265_desc(RSUA * p_rua, int * pt, char * p_sdp, int max_len)
{
    return rsua_get_sdp_video_desc(p_rua, "H265/90000", pt, p_sdp, max_len);
}

BOOL rsua_get_sdp_mp4_desc(RSUA * p_rua, int * pt, char * p_sdp, int max_len)
{
    return rsua_get_sdp_video_desc(p_rua, "MP4V-ES/90000", pt, p_sdp, max_len);
}

BOOL rsua_get_sdp_mp4_params(RSUA * p_rua, int * pt, char * p_cfg, int max_len)
{
    BOOL ret = FALSE;
    char sdp[1024] = {'\0'};
    
    if (rsua_get_sdp_mp4_desc(p_rua, pt, sdp, sizeof(sdp)) == FALSE)
    {
        return FALSE;
    }

    char * p_substr = strstr(sdp, "config=");
    if (p_substr != NULL)
    {
    	p_substr += strlen("config=");
    	char * p_tmp = p_substr;

    	while(*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0')
    	{
    		p_tmp++;
    	}
    	
    	int cfg_len = (int)(p_tmp - p_substr);
    	if(cfg_len < max_len)
    	{
    		memcpy(p_cfg, p_substr, cfg_len);
    		p_cfg[cfg_len] = '\0';
    		ret = TRUE;
    	}
    	else
    	{
    		ret = FALSE;
    	}
    }

    return ret;
}

BOOL rsua_get_sdp_aac_desc(RSUA * p_rua, int * pt, char * p_sdp, int max_len)
{
    return rsua_get_sdp_audio_desc(p_rua, "MPEG4-GENERIC", pt, p_sdp, max_len);
}

BOOL rsua_get_sdp_aac_params(RSUA * p_rua, int *pt, int *sizelength, int *indexlength, int *indexdeltalength, char * p_cfg, int max_len)
{
    BOOL ret = FALSE;
    char sdp[1024] = {'\0'};
    
    if (rsua_get_sdp_aac_desc(p_rua, pt, sdp, sizeof(sdp)) == FALSE)
    {
        return FALSE;
    }

    char * p_substr = strstr(sdp, "config=");
	if (p_substr != NULL)
	{
		p_substr += strlen("config=");
		char * p_tmp = p_substr;

		while (*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0')
		{
			p_tmp++;
		}
		
		int len = (int)(p_tmp - p_substr);
		if (len < max_len)
		{
			memcpy(p_cfg, p_substr, len);
			p_cfg[len] = '\0';
		}
		else
		{
		    return FALSE;
		}
	}

	p_substr = strstr(sdp, "sizelength=");
	if (p_substr != NULL)
	{
		p_substr += strlen("sizelength=");
		char * p_tmp = p_substr;

		while (*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0')
		{
			p_tmp++;
		}
		
		int len = (int)(p_tmp - p_substr);
		if (len > 0)
		{
			*sizelength = atoi(p_substr);
		}
	}

	p_substr = strstr(sdp, "indexlength=");
	if (p_substr != NULL)
	{
		p_substr += strlen("indexlength=");
		char * p_tmp = p_substr;

		while (*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0')
		{
			p_tmp++;
		}
		
		int len = (int)(p_tmp - p_substr);
		if (len > 0)
		{
			*indexlength = atoi(p_substr);
		}
	}

	p_substr = strstr(sdp, "indexdeltalength=");
	if (p_substr != NULL)
	{
		p_substr += strlen("indexdeltalength=");
		char * p_tmp = p_substr;

		while (*p_tmp != ' ' && *p_tmp != ';' && *p_tmp != '\0')
		{
			p_tmp++;
		}
		
		int len = (int)(p_tmp - p_substr);
		if (len > 0)
		{
			*indexdeltalength = atoi(p_substr);
		}
	}

    return TRUE;
}

#endif

/***********************************************************************/

HRTSP_MSG * rua_build_security_response(RSUA * p_rua)
{
    HRTSP_MSG * tx_msg = rtsp_get_msg_buf();
	if (tx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_get_msg_buf return NULL!!!\r\n", __FUNCTION__);
		return NULL;
	}

	tx_msg->msg_type = 1;
	tx_msg->msg_sub_type = 401;

	rtsp_add_tx_msg_fline(tx_msg, "RTSP/1.0", "401 Unauthorized");
	rtsp_add_tx_msg_line(tx_msg, "Server", "%s", hrtsp.srv_ver);
	rtsp_add_tx_msg_line(tx_msg, "CSeq", "%u", p_rua->cseq);
	rtsp_add_tx_msg_line(tx_msg, "Date", "%s", rtsp_get_utc_time());	

    sprintf(p_rua->auth_info.auth_nonce, "%08X%08X", rand(), rand());
	strcpy(p_rua->auth_info.auth_realm, "happytimesoft");
	
    rtsp_add_tx_msg_line(tx_msg, "WWW-Authenticate", "Digest realm=\"%s\", nonce=\"%s\"", 
		p_rua->auth_info.auth_realm, p_rua->auth_info.auth_nonce);
            
	tx_msg->remote_ip = p_rua->user_real_ip;
	tx_msg->remote_port = p_rua->user_real_port;

	return tx_msg;
}

HRTSP_MSG * rua_build_options_response(RSUA * p_rua)
{
	HRTSP_MSG * tx_msg = rtsp_get_msg_buf();
	if (tx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_get_msg_buf return NULL!!!\r\n", __FUNCTION__);
		return NULL;
	}

	tx_msg->msg_type = 1;
	tx_msg->msg_sub_type = 200;

	rtsp_add_tx_msg_fline(tx_msg, "RTSP/1.0", "200 OK");
	rtsp_add_tx_msg_line(tx_msg, "Server", "%s", hrtsp.srv_ver);
	rtsp_add_tx_msg_line(tx_msg, "CSeq", "%u", p_rua->cseq);
	rtsp_add_tx_msg_line(tx_msg, "Date", "%s", rtsp_get_utc_time());
#ifdef MEDIA_PUSHER
	rtsp_add_tx_msg_line(tx_msg, "Public", 
		"DESCRIBE, SETUP, PLAY, PAUSE, OPTIONS, TEARDOWN, GET_PARAMETER, SET_PARAMETER, ANNOUNCE, RECORD");
#else
	rtsp_add_tx_msg_line(tx_msg, "Public", 
		"DESCRIBE, SETUP, PLAY, PAUSE, OPTIONS, TEARDOWN, GET_PARAMETER, SET_PARAMETER");
#endif

	tx_msg->remote_ip = p_rua->user_real_ip;
	tx_msg->remote_port = p_rua->user_real_port;

	return tx_msg;
}

HRTSP_MSG * rua_build_descibe_response(RSUA * p_rua)
{
	HRTSP_MSG * tx_msg = rtsp_get_msg_buf();
	if (tx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_get_msg_buf return NULL!!!\r\n", __FUNCTION__);
		return NULL;
	}

	tx_msg->msg_type = 1;
	tx_msg->msg_sub_type = 200;

	rtsp_add_tx_msg_fline(tx_msg, "RTSP/1.0", "200 OK");
	rtsp_add_tx_msg_line(tx_msg, "Server", "%s", hrtsp.srv_ver);
	rtsp_add_tx_msg_line(tx_msg, "CSeq", "%u", p_rua->cseq);
	rtsp_add_tx_msg_line(tx_msg, "Date", "%s", rtsp_get_utc_time());

	if (p_rua->sid[0] != '\0')
	{
		rtsp_add_tx_msg_line(tx_msg, "Session", "%s", p_rua->sid);
	}
	
	rtsp_add_tx_msg_line(tx_msg, "Content-Base", "%s", p_rua->cbase);
	rtsp_add_tx_msg_line(tx_msg, "Content-type", "application/sdp");

	rua_build_sdp_msg(p_rua,tx_msg);
	
	int sdp_len = rtsp_cacl_sdp_length(tx_msg);
	rtsp_add_tx_msg_line(tx_msg, "Content-Length", "%d", sdp_len);

	tx_msg->remote_ip = p_rua->user_real_ip;
	tx_msg->remote_port = p_rua->user_real_port;

	return tx_msg;
}

HRTSP_MSG * rua_build_setup_response(RSUA * p_rua, int av_t)
{
	HRTSP_MSG * tx_msg = rtsp_get_msg_buf();
	if (tx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_get_msg_buf return NULL!!!\r\n", __FUNCTION__);
		return NULL;
	}

	tx_msg->msg_type = 1;
	tx_msg->msg_sub_type = 200;

	rtsp_add_tx_msg_fline(tx_msg, "RTSP/1.0", "200 OK");
	rtsp_add_tx_msg_line(tx_msg, "Server", "%s", hrtsp.srv_ver);
	rtsp_add_tx_msg_line(tx_msg, "CSeq", "%u", p_rua->cseq);
	rtsp_add_tx_msg_line(tx_msg, "Date", "%s", rtsp_get_utc_time());
	
	if (p_rua->sid[0] == '\0')
	{
		sprintf(p_rua->sid, "%u", rand());
	}
	
	rtsp_add_tx_msg_line(tx_msg, "Session", "%s", p_rua->sid);

    if (p_rua->rtp_tcp) // tcp
	{
		rtsp_add_tx_msg_line(tx_msg, "Transport", "RTP/AVP/TCP;unicast;interleaved=%u-%u",
			p_rua->channels[av_t].interleaved, p_rua->channels[av_t].interleaved+1);
	}		
	else if (1 == p_rua->rtp_unicast) // udp unicast
	{
		rtsp_add_tx_msg_line(tx_msg, "Transport", "RTP/AVP/UDP;unicast;client_port=%u-%u;server_port=%u-%u",
			p_rua->channels[av_t].r_port, p_rua->channels[av_t].r_port+1,
			p_rua->channels[av_t].l_port, p_rua->channels[av_t].l_port+1);
	}
	else // udp multicast
	{
	    rtsp_add_tx_msg_line(tx_msg, "Transport", "RTP/AVP;multicast;destination=%s;source=%s;port=%u-%u;ttl=255",
			p_rua->channels[av_t].destination, hrtsp.local_ipstr[0], 
			p_rua->channels[av_t].r_port, p_rua->channels[av_t].r_port+1);
	}

	tx_msg->remote_ip = p_rua->user_real_ip;
	tx_msg->remote_port = p_rua->user_real_port;

	return tx_msg;
}

HRTSP_MSG * rua_build_play_response(RSUA * p_rua)
{
	HRTSP_MSG * tx_msg = rtsp_get_msg_buf();
	if (tx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_get_msg_buf return NULL!!!\r\n", __FUNCTION__);
		return NULL;
	}

	tx_msg->msg_type = 1;
	tx_msg->msg_sub_type = 200;

	rtsp_add_tx_msg_fline(tx_msg, "RTSP/1.0", "200 OK");
	rtsp_add_tx_msg_line(tx_msg, "Server", "%s", hrtsp.srv_ver);
	rtsp_add_tx_msg_line(tx_msg, "CSeq", "%u", p_rua->cseq);
	rtsp_add_tx_msg_line(tx_msg, "Date", "%s", rtsp_get_utc_time());

#ifdef RTSP_REPLAY
	if (p_rua->replay)
	{
	    if (p_rua->play_range && p_rua->play_range_begin != 0)
	    {
    		char range[256];
    		char start[32], end[32];
    		time_t st = p_rua->play_range_begin;
    		time_t et = p_rua->play_range_end;
    	    struct tm *t1;	

    		t1 = gmtime(&st);

    		sprintf(start, "%04d%02d%02dT%02d%02d%02dZ",
    			t1->tm_year+1900, t1->tm_mon+1, t1->tm_mday,
    			t1->tm_hour, t1->tm_min, t1->tm_sec);

            if (et > 0)
            {
                t1 = gmtime(&et);

    			sprintf(end, "%04d%02d%02dT%02d%02d%02dZ",
    				t1->tm_year+1900, t1->tm_mon+1, t1->tm_mday,
    				t1->tm_hour, t1->tm_min, t1->tm_sec);

    			sprintf(range, "clock=%s-%s", start, end);	
            }
            else
            {
                sprintf(range, "clock=%s-", start);
            }

            rtsp_add_tx_msg_line(tx_msg, "Range", range);
        }
	}
	else
#endif

	if (p_rua->play_range)
	{
	    if (p_rua->play_range_end > 0)
	    {
	        rtsp_add_tx_msg_line(tx_msg, "Range", "npt=%.3f-%.3f", p_rua->media_info.curpos/1000.0, p_rua->play_range_end);
	    }
	    else
	    {
	        rtsp_add_tx_msg_line(tx_msg, "Range", "npt=%.3f-", p_rua->media_info.curpos/1000.0);
	    }
	}
	else
	{
	    rtsp_add_tx_msg_line(tx_msg, "Range", "npt=%.3f-", p_rua->media_info.curpos/1000.0);
	}

    rtsp_add_tx_msg_line(tx_msg, "Session", "%s;timeout=%d", p_rua->sid, hrtsp.session_timeout);

    if (p_rua->media_info.media_file)
    {
        char v_rtpinfo[1024] = {'\0'};
        char a_rtpinfo[1024] = {'\0'};

        if (p_rua->media_info.has_video && p_rua->channels[AV_VIDEO_CH].setup)
        {
            sprintf(v_rtpinfo, "url=%s/%s;seq=%d;rtptime=%u", 
                p_rua->cbase, p_rua->channels[AV_VIDEO_CH].ctl, 
                ++p_rua->channels[AV_VIDEO_CH].rtp_info.rtp_cnt, 
                rtsp_get_timestamp(90000)); 
        }

        if (p_rua->media_info.has_audio && p_rua->channels[AV_AUDIO_CH].setup)
        {
            sprintf(a_rtpinfo, "url=%s/%s;seq=%d;rtptime=%u", 
                p_rua->cbase, p_rua->channels[AV_AUDIO_CH].ctl, 
                ++p_rua->channels[AV_AUDIO_CH].rtp_info.rtp_cnt, 
                rtsp_get_timestamp(p_rua->media_info.a_samplerate));
        }

        if ((p_rua->media_info.has_video && p_rua->channels[AV_VIDEO_CH].setup) && 
        	(p_rua->media_info.has_audio && p_rua->channels[AV_AUDIO_CH].setup))
        {
            rtsp_add_tx_msg_line(tx_msg, "RTP-Info", "%s,%s", v_rtpinfo, a_rtpinfo);
        }
        else if (p_rua->media_info.has_video && p_rua->channels[AV_VIDEO_CH].setup)
        {
            rtsp_add_tx_msg_line(tx_msg, "RTP-Info", "%s", v_rtpinfo);
        }
        else if (p_rua->media_info.has_audio && p_rua->channels[AV_AUDIO_CH].setup)
        {
            rtsp_add_tx_msg_line(tx_msg, "RTP-Info", "%s", a_rtpinfo);
        }
    }

	tx_msg->remote_ip = p_rua->user_real_ip;
	tx_msg->remote_port = p_rua->user_real_port;

	return tx_msg;
}

HRTSP_MSG * rua_build_response(RSUA * p_rua, const char * resp_str)
{
	HRTSP_MSG * tx_msg = rtsp_get_msg_buf();
	if (tx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_get_msg_buf return NULL!!!\r\n", __FUNCTION__);
		return NULL;
	}

	tx_msg->msg_type = 1;
	tx_msg->msg_sub_type = 200;

	rtsp_add_tx_msg_fline(tx_msg, "RTSP/1.0", "%s", resp_str);
	rtsp_add_tx_msg_line(tx_msg, "Server", "%s", hrtsp.srv_ver);
	rtsp_add_tx_msg_line(tx_msg, "CSeq", "%u", p_rua->cseq);
	rtsp_add_tx_msg_line(tx_msg, "Date", "%s", rtsp_get_utc_time());

	if (p_rua->sid[0] != '\0')
	{
		rtsp_add_tx_msg_line(tx_msg, "Session", "%s", p_rua->sid);
	}
	
	tx_msg->remote_ip = p_rua->user_real_ip;
	tx_msg->remote_port = p_rua->user_real_port;

	return tx_msg;
}

BOOL rua_build_sdp_msg(RSUA * p_rua, HRTSP_MSG * tx_msg)
{
    int i, j;
    
	if (tx_msg == NULL)
	{
		return FALSE;
	}
	
	rtsp_add_tx_msg_sdp_line(tx_msg, "v", "0");
	rtsp_add_tx_msg_sdp_line(tx_msg, "o", "- 0 0 IN IP4 %s", hrtsp.local_ipstr[0]);
	rtsp_add_tx_msg_sdp_line(tx_msg, "s", "session");
	rtsp_add_tx_msg_sdp_line(tx_msg, "c", "IN IP4 %s", hrtsp.local_ipstr[0]);
	rtsp_add_tx_msg_sdp_line(tx_msg, "t", "0 0");
	rtsp_add_tx_msg_sdp_line(tx_msg, "a", "control:*");

	if (!p_rua->media_info.media_file || p_rua->media_info.duration == 0)
	{
	    rtsp_add_tx_msg_sdp_line(tx_msg, "a", "range:npt=0-");
	}
	else
	{
	    rtsp_add_tx_msg_sdp_line(tx_msg, "a", "range:npt=0-%0.3f", p_rua->media_info.duration / 1000.0);
	}

    if (g_rtsp_cfg.multicast && 0 == p_rua->rtp_unicast)
    {
        rtsp_add_tx_msg_sdp_line(tx_msg, "a", "type:broadcast");
    }

    for (i = 0; i < AV_MAX_CHS; i++)
    {
        if (p_rua->channels[i].cap_count == 0)
        {
            continue;
        }
        
		int  offset = 0;
		char cap_buf[128];
		char media[20];

		for (j = 0; j < p_rua->channels[i].cap_count; j++)
		{
			offset += sprintf(cap_buf+offset, "%u ", p_rua->channels[i].cap[j]);
		}
		
		if (offset > 0)
		{
			cap_buf[offset-1] = '\0';
		}

		if (AV_VIDEO_CH == i)
		{
		    strcpy(media, "video");
		}
		else if (AV_AUDIO_CH == i || AV_BACK_CH == i)
		{
		    strcpy(media, "audio");
		}
		else if (AV_METADATA_CH == i)
		{
		    strcpy(media, "application");
		}
		
		rtsp_add_tx_msg_sdp_line(tx_msg, "m", "%s %u RTP/AVP %s", media, p_rua->channels[i].l_port, cap_buf);

		if (g_rtsp_cfg.multicast && 0 == p_rua->rtp_unicast)
        {
            rtsp_add_tx_msg_sdp_line(tx_msg, "c", "IN IP4 %s/255", p_rua->channels[i].destination);
        }

		for (j = 0; j < MAX_AVN; j++)
		{
			char * mstr = p_rua->channels[i].cap_desc[j];
			if (mstr[0] != '\0')
			{
	            rtsp_add_tx_msg_sdp_line(tx_msg, "", "%s", mstr);
			}
		}
		
		rtsp_add_tx_msg_sdp_line(tx_msg, "a", "control:%s", p_rua->channels[i].ctl);
    }

	return TRUE;
}

int rtsp_cacl_sdp_length(HRTSP_MSG * tx_msg)
{
	if (tx_msg == NULL)
	{
		return 0;
	}
	
	int offset = 0;

	HDRV * pHdrV = (HDRV *)pps_lookup_start(&(tx_msg->sdp_ctx));
	while (pHdrV != NULL) 
	{
		if (pHdrV->header[0] != '\0')
		{
			offset += strlen(pHdrV->header) + strlen(pHdrV->value_string) + 3;
		}	
		else
		{
			offset += strlen(pHdrV->value_string) + 2;
		}
		
		pHdrV = (HDRV *)pps_lookup_next(&(tx_msg->sdp_ctx), pHdrV);
	}
	pps_lookup_end(&(tx_msg->sdp_ctx));

	return offset;
}

void rsua_send_rtsp_msg(RSUA * p_rua, HRTSP_MSG * tx_msg)
{
    int slen;
    int offset=0;
	char * tx_buf;	
	char rtsp_tx_buffer[2048];

	if (tx_msg == NULL)
	{
		return;
	}
	
	tx_buf = rtsp_tx_buffer;

	offset += sprintf(tx_buf+offset, "%s %s\r\n", tx_msg->first_line.header, tx_msg->first_line.value_string);
	
	HDRV * pHdrV = (HDRV *)pps_lookup_start(&(tx_msg->rtsp_ctx));
	while (pHdrV != NULL)
	{
		offset += sprintf(tx_buf+offset, "%s: %s\r\n", pHdrV->header, pHdrV->value_string);
		pHdrV = (HDRV *)pps_lookup_next(&(tx_msg->rtsp_ctx), pHdrV);
	}
	pps_lookup_end(&(tx_msg->rtsp_ctx));

	offset += sprintf(tx_buf+offset, "\r\n");

	if (tx_msg->sdp_ctx.node_num != 0)
	{
		pHdrV = (HDRV *)pps_lookup_start(&(tx_msg->sdp_ctx));
		while (pHdrV != NULL)
		{
			if ((strcmp(pHdrV->header, "pidf") == 0) || (strcmp(pHdrV->header, "text/plain") == 0))
			{
				offset += sprintf(tx_buf+offset, "%s\r\n", pHdrV->value_string);
			}	
			else
			{
				if (pHdrV->header[0] != '\0')
				{
					offset += sprintf(tx_buf+offset, "%s=%s\r\n", pHdrV->header, pHdrV->value_string);
				}	
				else
				{
					offset += sprintf(tx_buf+offset, "%s\r\n", pHdrV->value_string);
				}	
			}

			pHdrV = (HDRV *)pps_lookup_next(&(tx_msg->sdp_ctx), pHdrV);
		}
		pps_lookup_end(&(tx_msg->sdp_ctx));
	}

	log_print(HT_LOG_DBG, "%s\r\n", tx_buf);

#ifdef RTSP_OVER_HTTP
    if (p_rua->rtsp_send)
    {
        HTTPCLN * p_user = (HTTPCLN *) p_rua->rtsp_send;

        slen = send(p_user->cfd, tx_buf, offset, 0);
    	if (slen <= 0)
    	{
    		log_print(HT_LOG_ERR, "%s, send message failed!!!\r\n", __FUNCTION__);
    	}

    	return;
    }
#endif  

	sys_os_mutex_enter(p_rua->fd_mutex);
	
	slen = send(p_rua->fd, tx_buf, offset, 0);
	if (slen <= 0)
	{
		log_print(HT_LOG_ERR, "%s, send message failed!!!\r\n", __FUNCTION__);
	}

	sys_os_mutex_leave(p_rua->fd_mutex);
}

/***********************************************************************/
void rua_proxy_init()
{
	hrtsp.rua_fl = pps_ctx_fl_init(MAX_NUM_RUA, sizeof(RSUA), TRUE);
	hrtsp.rua_ul = pps_ctx_ul_init(hrtsp.rua_fl, TRUE);
}

void rua_proxy_deinit()
{
	if (hrtsp.rua_ul)
	{
	    pps_ul_free(hrtsp.rua_ul);
	    hrtsp.rua_ul = NULL;
	}

	if (hrtsp.rua_fl)
	{
	    pps_fl_free(hrtsp.rua_fl);
	    hrtsp.rua_fl = NULL;
	}
}

RSUA * rua_get_idle_rua()
{
	RSUA * p_rua = (RSUA *)pps_fl_pop(hrtsp.rua_fl);
	if (p_rua)
	{
		memset(p_rua, 0, sizeof(RSUA));
	}
	else
	{
		log_print(HT_LOG_ERR, "%s, don't have idle rtsp rua!!!\r\n", __FUNCTION__);
	}

	return p_rua;
}

void rua_set_online_rua(RSUA * p_rua)
{
	pps_ctx_ul_add(hrtsp.rua_ul, p_rua);
	p_rua->used_flag = 1;
}

void rua_set_idle_rua(RSUA * p_rua)
{
    int i;
    
	pps_ctx_ul_del(hrtsp.rua_ul, p_rua);

	if (p_rua->fd > 0)
	{
#ifdef EPOLL	
	    epoll_ctl(hrtsp.ep_fd, EPOLL_CTL_DEL, p_rua->fd, NULL);
#endif

		closesocket(p_rua->fd);
		p_rua->fd = 0;
	}	

	if (p_rua->fd_mutex)
	{
		sys_os_destroy_sig_mutex(p_rua->fd_mutex);
		p_rua->fd_mutex = NULL;
	}
	
#ifdef MEDIA_PUSHER
	if (p_rua->media_info.rtsp_pusher && p_rua->media_info.pusher)
	{
		p_rua->media_info.pusher->stopUdpRx();
		p_rua->media_info.pusher->setRua(NULL);
	}
#endif

    for (i = 0; i < AV_MAX_CHS; i++)
    {
        if (p_rua->channels[i].udp_fd > 0)
    	{
    		closesocket(p_rua->channels[i].udp_fd);
    		p_rua->channels[i].udp_fd = 0;
    	}

#ifdef RTSP_RTCP
    	if (p_rua->channels[i].rtcp_fd > 0)
    	{
    		closesocket(p_rua->channels[i].rtcp_fd);
    		p_rua->channels[i].rtcp_fd = 0;
    	}
#endif
    }

#ifdef RTSP_BACKCHANNEL
    p_rua->rtp_rx = 0;
    
	while (p_rua->tid_udp_rx)
	{
		usleep(10*1000);
	}

    rtsp_bc_uninit_audio(p_rua);
#endif

#ifdef RTSP_RTCP
    p_rua->rtcp_rx = 0;

    while (p_rua->tid_rtcp_rx)
    {
        usleep(10*1000);
    }
#endif

#ifdef MEDIA_FILE
	if (p_rua->media_info.file_demuxer)
	{
		delete p_rua->media_info.file_demuxer;
		p_rua->media_info.file_demuxer = NULL;
	}
#endif

#ifdef RTSP_OVER_HTTP
    if (p_rua->rtsp_recv)
    {
        ((HTTPCLN *)p_rua->rtsp_recv)->p_rua = NULL;
    }

    if (p_rua->rtsp_send)
    {
        ((HTTPCLN *)p_rua->rtsp_send)->p_rua = NULL;
    }
#endif

	if (p_rua->rtp_rcv_buf)
	{
		free(p_rua->rtp_rcv_buf);
	}
	
	memset(p_rua, 0, sizeof(RSUA));
	
	pps_fl_push(hrtsp.rua_fl, p_rua);
}

RSUA * rua_lookup_start()
{
	return (RSUA *)pps_lookup_start(hrtsp.rua_ul);
}

RSUA * rua_lookup_next(RSUA * p_rua)
{
	return (RSUA *)pps_lookup_next(hrtsp.rua_ul, p_rua);
}

void rua_lookup_stop()
{
	pps_lookup_end(hrtsp.rua_ul);
}

uint32 rua_get_index(RSUA * p_rua)
{
	return pps_get_index(hrtsp.rua_fl, p_rua);
}

RSUA * rua_get_by_index(uint32 index)
{
	return (RSUA *)pps_get_node_by_index(hrtsp.rua_fl, index);
}

#ifdef RTSP_OVER_HTTP
RSUA * rua_get_by_sessioncookie(char * sessioncookie)
{
    RSUA * p_rua = (RSUA *)pps_lookup_start(hrtsp.rua_ul);
    while (p_rua)
    {
        if (strcmp(p_rua->sessioncookie, sessioncookie) == 0)
        {
            break;
        }
        
        p_rua = (RSUA *)pps_lookup_next(hrtsp.rua_ul, p_rua);
    }
    pps_lookup_end(hrtsp.rua_ul);

    return p_rua;
}
#endif // end of RTSP_OVER_HTTP

BOOL rsua_init_udp_connection(RSUA * p_rua, int av_t, uint32 lip)
{
    uint16 port;
    int try_cnt = 0;
	int addr_len;
	struct sockaddr_in addr;
	SOCKET fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd <= 0)
	{
		log_print(HT_LOG_ERR, "%s, socket SOCK_DGRAM error!\n", __FUNCTION__);
		return FALSE;
	}

	addr_len = sizeof(addr);
	port = rtsp_get_udp_port();

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = lip;
	addr.sin_port = htons(port);

	int len = 1024 * 1024;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&len, sizeof(int)))
	{
		log_print(HT_LOG_WARN, "%s, setsockopt SO_SNDBUF error!!!\r\n", __FUNCTION__);
	}
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&len, sizeof(int)))
	{
		log_print(HT_LOG_WARN, "%s, setsockopt SO_SNDBUF error!!!\r\n", __FUNCTION__);
	}
	
	while (try_cnt < 3)
	{
    	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    	{
    		log_print(HT_LOG_ERR, "%s, Bind udp socket fail,error = %s\r\n", __FUNCTION__, sys_os_get_socket_error());

    		try_cnt++;
    		port = rtsp_get_udp_port();
    		addr.sin_port = htons(port);
    	}
    	else
    	{
    	    break;
    	}
	}

	if (try_cnt == 3)
	{
	    closesocket(fd);
		return FALSE;
	}

    p_rua->channels[av_t].udp_fd = fd;
    p_rua->channels[av_t].l_port = port;

	return TRUE;
}

BOOL rsua_init_mc_connection(RSUA * p_rua, int av_t, uint32 lip)
{
    uint16 port = 0;
    char destination[32];
    struct sockaddr_in addr;
    struct ip_mreq mcast;

    strcpy(destination, p_rua->channels[av_t].destination);
    
#ifdef RTSP_BACKCHANNEL
	if (AV_TYPE_BACKCHANNEL == av_t) // backchannel
    {
        port = p_rua->channels[av_t].r_port;
    }
#endif
    
	SOCKET fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd <= 0)
	{
		log_print(HT_LOG_ERR, "%s, socket SOCK_DGRAM error!\n", __FUNCTION__);
		return FALSE;
	}

    addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = lip;
	addr.sin_port = htons(port);

	/* reuse socket addr */
	int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt))) 
    {  
        log_print(HT_LOG_WARN, "%s, setsockopt SO_REUSEADDR error!\r\n", __FUNCTION__);
    }

    int len = 1024 * 1024;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&len, sizeof(int)))
	{
		log_print(HT_LOG_WARN, "%s, setsockopt SO_SNDBUF error!!!\r\n", __FUNCTION__);
	}
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&len, sizeof(int)))
	{
		log_print(HT_LOG_WARN, "%s, setsockopt SO_SNDBUF error!!!\r\n", __FUNCTION__);
	}
	
	if (bind(fd,(struct sockaddr *)&addr,sizeof(addr)) == -1)
	{
		log_print(HT_LOG_ERR, "%s, Bind udp socket fail,error = %s\r\n", __FUNCTION__, sys_os_get_socket_error());
		closesocket(fd);
		return FALSE;
	}

    int ttl = 255;
    setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl));
       
	mcast.imr_multiaddr.s_addr = inet_addr(destination);
	mcast.imr_interface.s_addr = lip;

	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mcast, sizeof(mcast)) < 0)
	{
		log_print(HT_LOG_ERR, "%s, setsockopt IP_ADD_MEMBERSHIP error!%s\r\n", __FUNCTION__, sys_os_get_socket_error());
		closesocket(fd);
		return FALSE;
	}

    p_rua->channels[av_t].udp_fd = fd;

	return TRUE;
}

#ifdef RTSP_RTCP

BOOL rsua_init_rtcp_udp_connection(RSUA * p_rua, int av_t, uint32 lip)
{
    uint16 port;
    int try_cnt = 0;
	int addr_len;
	struct sockaddr_in addr;
	SOCKET fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd <= 0)
	{
		log_print(HT_LOG_ERR, "%s, socket SOCK_DGRAM error!\n", __FUNCTION__);
		return FALSE;
	}

	addr_len = sizeof(addr);
    port = p_rua->channels[av_t].l_port+1;
	
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = lip;
	addr.sin_port = htons(port);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		log_print(HT_LOG_ERR, "%s, Bind udp socket fail,error = %s\r\n", __FUNCTION__, sys_os_get_socket_error());

		closesocket(fd);
		return FALSE;
	}

    p_rua->channels[av_t].rtcp_fd = fd;

	return TRUE;
}

BOOL rsua_init_rtcp_mc_connection(RSUA * p_rua, int av_t, uint32 lip)
{
	uint16 port = 0;
    char destination[32];
    struct sockaddr_in addr;
    struct ip_mreq mcast;

    strcpy(destination, p_rua->channels[av_t].destination);
    
#ifdef RTSP_BACKCHANNEL
	if (AV_TYPE_BACKCHANNEL == av_t) // backchannel
    {
		port = p_rua->channels[av_t].r_port + 1;        
    }
#endif
    
	SOCKET fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd <= 0)
	{
		log_print(HT_LOG_ERR, "%s, socket SOCK_DGRAM error!\n", __FUNCTION__);
		return FALSE;
	}

    addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = lip;
	addr.sin_port = htons(port);

	/* reuse socket addr */
	int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt))) 
    {  
        log_print(HT_LOG_WARN, "%s, setsockopt SO_REUSEADDR error!\r\n", __FUNCTION__);
    }
    
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		log_print(HT_LOG_ERR, "%s, Bind udp socket fail,error = %s\r\n", __FUNCTION__, sys_os_get_socket_error());
		closesocket(fd);
		return FALSE;
	}

    int ttl = 255;
    setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl));
       
	mcast.imr_multiaddr.s_addr = inet_addr(destination);
	mcast.imr_interface.s_addr = lip;

	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mcast, sizeof(mcast)) < 0)
	{
		log_print(HT_LOG_ERR, "%s, setsockopt IP_ADD_MEMBERSHIP error!%s\r\n", __FUNCTION__, sys_os_get_socket_error());
		closesocket(fd);
		return FALSE;
	}

    p_rua->channels[av_t].rtcp_fd = fd;

	return TRUE;
}


#endif // end of RTSP_RTCP



