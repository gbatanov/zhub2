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
#include "http.h"
#include "http_parse.h"
#include "http_srv.h"
#include "rtsp_http.h"
#include "rtsp_srv.h"
#include "rtsp_cfg.h"
#include "rtsp_rsua.h"
#include "rtp.h"
#include "base64.h"
#ifdef RTSP_BACKCHANNEL
#include "rtsp_srv_backchannel.h"
#endif

#ifdef RTSP_OVER_HTTP

/***********************************************************************/
HTTPSRV             hsrv;
extern RTSP_CLASS	hrtsp;
extern RTSP_CFG     g_rtsp_cfg;

/***********************************************************************/

BOOL rtsp_http_cb(void * p_srv, HTTPCLN * p_cln, HTTPMSG * p_msg, void * userdata)
{
    if (p_msg)
    {
        char * p = http_get_headline(p_msg, "x-sessioncookie");
        if (p)
        {
            rtsp_http_process(p_cln, p_msg);
        }

        http_free_msg(p_msg);
    }
    else if (p_cln)
    {
        rtsp_http_free_cln(p_cln);
        http_free_used_cln(&hsrv, p_cln);
    }
    
    return TRUE;
}

void rtsp_http_rtsp_cb(void * p_srv, HTTPCLN * p_cln, char * buff, int buflen, void * userdata)
{
    if (!rtsp_http_msg_process(p_cln, buff, buflen))
    {
        rtsp_http_free_cln(p_cln);
        http_free_used_cln(&hsrv, p_cln);
    }
}

BOOL rtsp_http_init()
{
#if 0
	if (g_rtsp_cfg.http_port <= 0 || g_rtsp_cfg.http_port > 65535)
	{
		g_rtsp_cfg.http_port = 80;
	}
    	
    if (http_srv_init(&hsrv, NULL, g_rtsp_cfg.http_port, MAX_NUM_RUA, 0, NULL, NULL) == 0)
    {
        http_srv_set_callback(&hsrv, rtsp_http_cb, NULL);
        http_srv_set_rtsp_callback(&hsrv, rtsp_http_rtsp_cb, NULL);
    }
#endif

    return TRUE;
}

void rtsp_http_deinit()
{
#if 0
	http_srv_deinit(&hsrv);
#endif	
}

int rtsp_http_rly(HTTPCLN * p_user, HTTPMSG * rx_msg, const char * p_xml, int len)
{
    int tlen;
	char * p_bufs;

	p_bufs = (char *)malloc(len + 1024);
	if (NULL == p_bufs)
	{
		return -1;
	}
	
	tlen = sprintf(p_bufs,	"HTTP/1.1 200 OK\r\n"
							"Server: hsoap/2.8\r\n"
							"Content-Type: %s\r\n"
							"Content-Length: %d\r\n\r\n",
							"application/x-rtsp-tunnelled", len);

	if (p_xml && len > 0)
	{
		memcpy(p_bufs+tlen, p_xml, len);
		tlen += len;
	}

	p_bufs[tlen] = '\0';
	log_print(HT_LOG_DBG, "TX >> %s\r\n\r\n", p_bufs);

	tlen = send(p_user->cfd, p_bufs, tlen, 0);

	free(p_bufs);
	
	return tlen;
}

BOOL rtsp_http_process(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
    char * p = http_get_headline(rx_msg, "x-sessioncookie");
    if (NULL == p)
    {
        return FALSE;
    }

    if (p_user->p_rua)
    {
        log_print(HT_LOG_ERR, "%s, rua already exist\r\n", __FUNCTION__);
        return FALSE;
    }
    
    if (strstr(rx_msg->first_line.header, "GET"))
    {
        RSUA * p_rua = rua_get_idle_rua();
        if (NULL == p_rua)
        {
            log_print(HT_LOG_ERR, "%s, rua_get_idle_rua failed\r\n", __FUNCTION__);
            return FALSE;
        }

        p_rua->rtp_tcp = 1;
        p_rua->rtsp_send = p_user;
        p_rua->lats_rx_time = time(NULL);
        strncpy(p_rua->sessioncookie, p, sizeof(p_rua->sessioncookie)-1);
        
        rua_set_online_rua(p_rua);
        
        p_user->p_rua = p_rua;
        p_user->rtsp_over_http = TRUE;

        rtsp_http_rly(p_user, rx_msg, NULL, 0);
    }
    else if (strstr(rx_msg->first_line.header, "POST"))
    {
        RSUA * p_rua = rua_get_by_sessioncookie(p);        
        if (p_rua)
        {
        	p_rua->lats_rx_time = time(NULL);
            p_rua->rtsp_recv = p_user;
            p_user->p_rua = p_rua;
            p_user->rtsp_over_http = TRUE;
        }
    }

    return TRUE;
}

int rtsp_split_base64(char * p_buff, int len)
{
    for (int i = 0; i < len; i++)
    {
        char c = p_buff[i];
        
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
            (c >= '0' && c <= '9') || 
            c == '+' || c == '/' || c == '=' || c == '\r' || c == '\n') 
        {
            continue;
        } 
        else 
        {
            return -1; // not base64 encoded data
        }
    }
    
    char * ch = strchr(p_buff, '=');
    if (ch)
    {
        while (*ch == '=')
        {
            ch++;
        }
    }
    else
    {
        return len;
    }

    return ch - p_buff;
}

BOOL rtsp_http_msg_process1(RSUA * p_rua, char * p_buff, int len)
{
    uint8 buff[2048];
    int bufflen;

    bufflen = base64_decode(p_buff, len, buff, sizeof(buff));
    if (-1 == bufflen)
    {
        log_print(HT_LOG_ERR, "%s, base64_decode failed\r\n", __FUNCTION__);
        return FALSE;
    }
    
    if (NULL == p_rua->rtp_rcv_buf || 0 == p_rua->rtp_t_len)
    {
        memcpy(p_rua->rcv_buf + p_rua->rcv_dlen, buff, bufflen);
        p_rua->rcv_dlen += bufflen;

        if (p_rua->rcv_dlen < 16)
		{
			return TRUE;
		}
    }
    else
    {
        if (bufflen > p_rua->rtp_t_len - p_rua->rtp_rcv_len)
        {
            log_print(HT_LOG_ERR, "%s, bufflen=%d, rtp_t_len=%d, rtp_rcv_len=%d\r\n", 
                __FUNCTION__, bufflen, p_rua->rtp_t_len, p_rua->rtp_rcv_len);
            return FALSE;
        }

        memcpy(p_rua->rtp_rcv_buf + p_rua->rtp_rcv_len, buff, bufflen);
        p_rua->rtp_rcv_len += bufflen;

        if (p_rua->rtp_rcv_len == p_rua->rtp_t_len)
		{
#ifdef RTSP_BACKCHANNEL
        	if (p_rua->backchannel)
        	{
            	rtsp_bc_tcp_data_rx(p_rua, (uint8*)p_rua->rtp_rcv_buf, p_rua->rtp_rcv_len);
            }
#endif
			
			free(p_rua->rtp_rcv_buf);
			p_rua->rtp_rcv_buf = NULL;
			p_rua->rtp_rcv_len = 0;
			p_rua->rtp_t_len = 0;
		}
		
		return TRUE;
    }

rx_point:

    if (rtsp_is_rtsp_msg(p_rua->rcv_buf))
	{
		int ret = rtsp_msg_parser(p_rua);
		if (RTSP_PARSE_FAIL == ret)
		{
		    log_print(HT_LOG_ERR, "%s, rtsp_msg_parser failed\r\n", __FUNCTION__);
		    return FALSE;
		}
		else if (RTSP_PARSE_MOREDATE == ret)
		{
			return TRUE;
		}

		if (p_rua->rcv_dlen >= 16)
		{
			goto rx_point;
		}
	}
    else
    {
        RILF * p_rilf = (RILF *)(p_rua->rcv_buf);
		if (p_rilf->magic != 0x24)
		{		
			log_print(HT_LOG_WARN, "%s, p_rilf->magic[0x%02X]!!!\r\n", __FUNCTION__, p_rilf->magic);

			// Try to recover from wrong data
            
			for (int i = 1; i <= p_rua->rcv_dlen - 4; i++)
			{
				if (p_rua->rcv_buf[i] == 0x24 &&
					(p_rua->rcv_buf[i+1] == p_rua->channels[AV_VIDEO_CH].interleaved ||
					 p_rua->rcv_buf[i+1] == p_rua->channels[AV_AUDIO_CH].interleaved))
				{
					memmove(p_rua->rcv_buf, p_rua->rcv_buf+i, p_rua->rcv_dlen - i);
					p_rua->rcv_dlen -= i;
					goto rx_point;
				}
			}

			p_rua->rcv_dlen = 0;
			return TRUE;
		}
		
		uint16 rtp_len = ntohs(p_rilf->rtp_len);
		if (rtp_len > (p_rua->rcv_dlen - 4))
		{
			if (p_rua->rtp_rcv_buf)
			{
				free(p_rua->rtp_rcv_buf);
			}
			
			p_rua->rtp_rcv_buf = (char *)malloc(rtp_len+4);
			if (p_rua->rtp_rcv_buf == NULL) 
			{
			    return FALSE;
			}
			
			memcpy(p_rua->rtp_rcv_buf, p_rua->rcv_buf, p_rua->rcv_dlen);
			p_rua->rtp_rcv_len = p_rua->rcv_dlen;
			p_rua->rtp_t_len = rtp_len+4;

			p_rua->rcv_dlen = 0;

			return TRUE;
		}

#ifdef RTSP_BACKCHANNEL
    	if (p_rua->backchannel)
    	{
        	rtsp_bc_tcp_data_rx(p_rua, (uint8*)p_rilf, rtp_len+4);
        }
#endif

		p_rua->rcv_dlen -= rtp_len+4;
		
		if (p_rua->rcv_dlen > 0)
		{
			memmove(p_rua->rcv_buf, p_rua->rcv_buf+rtp_len+4, p_rua->rcv_dlen);
		}

		if (p_rua->rcv_dlen >= 16)
		{
			goto rx_point;
		}
    }
	
	return TRUE;
}

BOOL rtsp_http_msg_process(HTTPCLN * p_user, char * p_buff, int len)
{
    if (NULL == p_user->p_rua)
    {
        log_print(HT_LOG_ERR, "%s, rua is null\r\n", __FUNCTION__);
        return FALSE;
    }

    RSUA * p_rua = (RSUA *) p_user->p_rua;
    
    p_rua->lats_rx_time = time(NULL);

    int base64_len = rtsp_split_base64(p_buff, len);
    
    while (base64_len > 0)
    {
        if (!rtsp_http_msg_process1(p_rua, p_buff, base64_len))
        {
        }

        len -= base64_len;
        p_buff += base64_len;

		if (len > 0)
		{
			base64_len = rtsp_split_base64(p_buff, len);
		}
		else 
		{
			break;
		}
    }

    return TRUE;
}

void rtsp_http_free_cln(HTTPCLN * p_cln)
{
	if (p_cln->p_rua)
	{
	    RSUA * p_rua = (RSUA *)p_cln->p_rua;

        if (p_rua->rtsp_send == p_cln)
	    {
	        ((HTTPCLN *)p_rua->rtsp_send)->p_rua = NULL;
	        return;
	    }
	    
	    if (p_rua->rtsp_recv == p_cln)
	    {
	        ((HTTPCLN *)p_rua->rtsp_recv)->p_rua = NULL;
	    }
	    
	    RIMSG msg;
    	memset(&msg,0,sizeof(RIMSG));
    	msg.msg_src = RTSP_DEL_UA_SRC;
    	msg.msg_dua = rua_get_index(p_rua);
    	msg.msg_buf = NULL;
    	
    	if (hqBufPut(hrtsp.msg_queue, (char *)&msg) == FALSE)
    	{
    		log_print(HT_LOG_ERR, "rtsp_stop_rua::send msg[NULL] to main task failed!!!\r\n");
    	}
	}
}

#endif // end of RTSP_OVER_HTTP



