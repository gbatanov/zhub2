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
#include "rtsp_parse.h"
#include "util.h"
#include "rtsp_rsua.h"
#include "rtsp_srv.h"
#include "rtp.h"
#include "rtsp_stream.h"
#include "rtsp_media.h"
#include "media_format.h"
#include "rtsp_cfg.h"
#include "rfc_md5.h"
#include "http_auth.h"
#include "rtsp_util.h"
#include "rtsp_timer.h"
#include "rtsp_mc.h"

#ifdef RTSP_OVER_HTTP
#include "rtsp_http.h"
#endif
#ifdef RTSP_BACKCHANNEL
#include "rtsp_srv_backchannel.h"
#endif
#ifdef RTSP_RTCP
#include "rtcp.h"
#endif
#ifdef MEDIA_PUSHER
#include "mpeg4.h"
#endif
#ifdef RTSP_CRYPT
#include "rtsp_crypt.h"
#endif
#ifdef RTMP_PROXY
#include "log.h"
#endif
#if __WINDOWS_OS__ && defined(MEDIA_DEVICE)
#include "mfapi.h"
#endif

/***********************************************************************/
#define RTSP_MAJOR_VERSION	4
#define RTSP_MINOR_VERSION  8

/***********************************************************************/
RTSP_CLASS	hrtsp;

/***********************************************************************/
void rtsp_print_info()
{
    uint32 i;
	char rtspUrl[NET_IF_NUM][100] = {'\0'};

	for (i = 0; i < hrtsp.local_ip_num; i++)
	{
    	if (hrtsp.sport == 554)
    	{
    		sprintf(rtspUrl[i], "rtsp://%s", hrtsp.local_ipstr[i]);
    	}
    	else
    	{
    		sprintf(rtspUrl[i], "rtsp://%s:%d", hrtsp.local_ipstr[i], hrtsp.sport);
    	}
	}
	
	printf("Happytime rtsp server V%d.%d\r\n", RTSP_MAJOR_VERSION, RTSP_MINOR_VERSION);
	printf("Play streams from this server using the URL:\r\n");

#ifdef MEDIA_FILE
	for (i = 0; i < hrtsp.local_ip_num; i++)
	{
	    printf("\t%s/<filename>\r\n", rtspUrl[i]);
	}
	
	printf("where <filename> is a file present in the current directory.\r\n");
#endif

#ifdef MEDIA_DEVICE	
    for (i = 0; i < hrtsp.local_ip_num; i++)
    {
	    printf("\t%s/screenlive\r\n", rtspUrl[i]);
	}
	
	printf("stream from live screen.\r\n");

	for (i = 0; i < hrtsp.local_ip_num; i++)
	{
	    printf("\t%s/videodevice\r\n", rtspUrl[i]);
	}
	
	printf("stream from camera device.\r\n");

	for (i = 0; i < hrtsp.local_ip_num; i++)
	{
	    printf("\t%s/audiodevice\r\n", rtspUrl[i]);
	}
	
	printf("stream from audio device.\r\n");

	for (i = 0; i < hrtsp.local_ip_num; i++)
	{
	    printf("\t%s/screenlive+audiodevice\r\n", rtspUrl[i]);
	}

	printf("stream from live screen and audio device\r\n");

	for (i = 0; i < hrtsp.local_ip_num; i++)
	{
	    printf("\t%s/videodevice+audiodevice\r\n", rtspUrl[i]);
	}
	
	printf("stream from camera device and audio device.\r\n");
#endif

#ifdef MEDIA_PROXY    
    MEDIA_PRY * p_proxy = g_rtsp_cfg.proxy;
    if (p_proxy)
    {
        printf("\r\nplay proxy streams from this server using the URL:\r\n");
    }
    
    while (p_proxy)
    {
        for (i = 0; i < hrtsp.local_ip_num; i++)
        {
            printf("\t%s/%s\r\n", rtspUrl[i], p_proxy->cfg.suffix);
        }
        
        p_proxy = p_proxy->next;
    }
#endif

#ifdef MEDIA_PUSHER
	RTSP_PUSHER * p_pusher = g_rtsp_cfg.pusher;
    if (p_pusher)
    {
        printf("\r\nplay pusher streams from this server using the URL:\r\n");
    }

    while (p_pusher)
    {
        for (i = 0; i < hrtsp.local_ip_num; i++)
        {
            printf("\t%s/%s\r\n", rtspUrl[i], p_pusher->cfg.suffix);
        }
        
        p_pusher = p_pusher->next;
    }
#endif

#ifdef MEDIA_LIVE
	for (i = 0; i < hrtsp.local_ip_num; i++)
	{
	    printf("\t%s/live\r\n", rtspUrl[i]);
	}
	
	printf("where <filename> is a file present in the current directory.\r\n");
#endif

#ifdef RTSP_OVER_HTTP
    if (g_rtsp_cfg.rtsp_over_http)
    {
        // printf("\r\n(We use port %d for optional RTSP-over-HTTP tunneling, or for HTTP live streaming)\r\n", g_rtsp_cfg.http_port);
    }
#endif

	printf("\r\nSee the log file ipsee.txt for additional information.\r\n");
}

#ifdef RTMP_PROXY

void rtmp_log_callback(int level, const char *format, va_list vl)
{
	int lvl = HT_LOG_DBG;
	int offset = 0;
	char str[2048] = "";

	offset += vsnprintf(str, 2048-1, format, vl);

	offset += snprintf(str+offset, 2048-1-offset, "\r\n");
	
	if (level == RTMP_LOGCRIT)
	{
		lvl = HT_LOG_FATAL;
	}
	else if (level == RTMP_LOGERROR)
	{
		lvl = HT_LOG_ERR;
	}
	else if (level == RTMP_LOGWARNING)
	{
		lvl = HT_LOG_WARN;
	}
	else if (level == RTMP_LOGINFO)
	{
		lvl = HT_LOG_INFO;
	}
	else if (level == RTMP_LOGDEBUG || level == RTMP_LOGDEBUG2)
	{
		lvl = HT_LOG_DBG;
	}
	else if (level == RTMP_LOGALL)
	{
	    lvl = HT_LOG_TRC;
	}

	log_print(level, str);
}

void rtsp_set_rtmp_log()
{
    RTMP_LogLevel lvl;
    
    RTMP_LogSetCallback(rtmp_log_callback);

    switch (g_rtsp_cfg.log_level)
    {
    case HT_LOG_TRC:
        lvl = RTMP_LOGINFO; // RTMP_LOGALL;
        break;
        
    case HT_LOG_DBG:
        lvl = RTMP_LOGINFO; // RTMP_LOGDEBUG;
        break;

    case HT_LOG_INFO:
        lvl = RTMP_LOGINFO;
        break;

    case HT_LOG_WARN:
        lvl = RTMP_LOGWARNING;
        break;

    case HT_LOG_ERR:
        lvl = RTMP_LOGERROR;
        break;

    case HT_LOG_FATAL:
        lvl = RTMP_LOGCRIT;
        break;

    default:
        lvl = RTMP_LOGERROR;
        break;
    }

    RTMP_LogSetLevel(lvl);
}

#endif // RTMP_PROXY

/***********************************************************************
 *
 * rtsp server start
 *
************************************************************************/
BOOL rtsp_start(const char * config)
{
    int i;
    int bufs;
	
	rtsp_read_config(config);

#if 0
	if (g_rtsp_cfg.log_enable)
	{
		log_init("ipsee.txt");
		log_set_level(g_rtsp_cfg.log_level);
	}
	else
	{
		log_close();
	}
#endif

	memset(&hrtsp, 0, sizeof(hrtsp));

	if (g_rtsp_cfg.serverip_nums == 0)
	{
	    struct in_addr in;
	    
	    g_rtsp_cfg.serverip_nums++;
	    
	    in.s_addr = get_default_if_ip();
	    strcpy(g_rtsp_cfg.serverip[0], inet_ntoa(in));
	}

	if (g_rtsp_cfg.serverport <= 0 || g_rtsp_cfg.serverport > 65535)
	{
		g_rtsp_cfg.serverport = 554;
	}

#ifdef RTSP_OVER_HTTP
	if (g_rtsp_cfg.http_port <= 0 || g_rtsp_cfg.http_port > 65535)
	{
		g_rtsp_cfg.http_port = 80;
	}
#endif	

	sprintf(hrtsp.srv_ver, "happytime rtsp server %d.%d", RTSP_MAJOR_VERSION, RTSP_MINOR_VERSION);

	rua_proxy_init();

	rmc_proxy_init();

	hrtsp.msg_queue = hqCreate(MAX_NUM_RUA * 4, sizeof(RIMSG), HQ_GET_WAIT);
	if (hrtsp.msg_queue == NULL)
	{
		log_print(HT_LOG_ERR, "rtsp_start::create rtsp task queue failed!!!\r\n");
		return FALSE;
	}

#ifdef EPOLL
    hrtsp.ep_event_num = MAX_NUM_RUA + NET_IF_NUM + 8;
    
    hrtsp.ep_fd = epoll_create(hrtsp.ep_event_num);
    if (hrtsp.ep_fd < 0)
    {
        log_print(HT_LOG_ERR, "%s, epoll_create failed\r\n", __FUNCTION__);
        return FALSE;
    }

    hrtsp.ep_events = (struct epoll_event *)malloc(sizeof(struct epoll_event) * hrtsp.ep_event_num);
    if (hrtsp.ep_events == NULL)
    {
        log_print(HT_LOG_ERR, "%s, malloc failed\r\n", __FUNCTION__);
        return FALSE;
    }
#endif

#if __WINDOWS_OS__ && defined(MEDIA_DEVICE)
    MFStartup(MF_VERSION);
#endif

    for (i = 0; i < g_rtsp_cfg.serverip_nums; i++)
    {
        int idx = hrtsp.local_ip_num;
        
        hrtsp.local_ip[idx] = get_address_by_name(g_rtsp_cfg.serverip[i]);
		strcpy(hrtsp.local_ipstr[idx], g_rtsp_cfg.serverip[i]);

		if (rtsp_net_listen_init(hrtsp.local_ip[idx], g_rtsp_cfg.serverport, idx) < 0)
    	{
    		struct in_addr in;
    		in.s_addr = hrtsp.local_ip[idx];
    		
    		log_print(HT_LOG_ERR, "rtsp_start::rtsp_net_listen_init failed!!!\r\n");
    		printf("Bind %s:%d failed\r\n", inet_ntoa(in), g_rtsp_cfg.serverport);
    	}
    	else
    	{
    	    hrtsp.local_ip_num++;
    	}
    }

	bufs = MAX_NUM_RUA * 2;

#ifdef MEDIA_PROXY
	bufs += rtsp_get_proxy_nums() * 2;
#endif

#ifdef MEDIA_PUSHER
	bufs += rtsp_get_pusher_nums() * 2;
#endif

	rtsp_parse_buf_init(bufs);

#ifdef RTSP_CRYPT	
	rtsp_crypt_init();
#endif	

#ifdef MEDIA_PROXY
	rtsp_init_proxies();
#endif

#ifdef MEDIA_PUSHER
	rtsp_init_pushers();
#endif

#ifdef RTSP_OVER_HTTP
    if (g_rtsp_cfg.rtsp_over_http)
    {
        rtsp_http_init();
    }
#endif

#ifdef RTMP_PROXY
    rtsp_set_rtmp_log();
#endif

	rtsp_print_info();

    srand((uint32)time(NULL));

	hrtsp.r_flag = 1;
	hrtsp.tid_pkt_rx = sys_os_create_thread((void *)rtsp_rx_thread, NULL);
	hrtsp.tid_main = sys_os_create_thread((void *)rtsp_task, NULL);

	hrtsp.session_timeout = 60;
	
	rtsp_timer_init();

	hrtsp.sys_init_flag = 1;
	
	return TRUE;
}

void rtsp_stop()
{
    uint32 i;

	rtsp_timer_deinit();
    
    hrtsp.r_flag = 0;
    while (hrtsp.tid_pkt_rx)
    {
        usleep(10*1000);
    }

    RIMSG stm;
    memset(&stm, 0, sizeof(stm));

    stm.msg_src = RTSP_EXIT;

    hqBufPut(hrtsp.msg_queue, (char *)&stm);

    while (hrtsp.tid_main)
    {
        usleep(10*1000);
    }

#ifdef RTSP_CRYPT	
	rtsp_crypt_uninit();
#endif

    for (i = 0; i < hrtsp.local_ip_num; i++)
    {
        if (hrtsp.sfd[i] > 0)
        {
#ifdef EPOLL
            epoll_ctl(hrtsp.ep_fd, EPOLL_CTL_DEL, hrtsp.sfd[i], NULL);
#endif

            closesocket(hrtsp.sfd[i]);
            hrtsp.sfd[i] = 0;
        }
    }    

    hqDelete(hrtsp.msg_queue);

	for (i = 0; i < MAX_NUM_RUA; i++)
	{
		RSUA * p_rua = rua_get_by_index(i);

		if (p_rua->used_flag)
		{
			rtsp_close_rua(p_rua);
		}
	}
	
    rua_proxy_deinit();

    rmc_proxy_deinit();

    rtsp_free_outputs(&g_rtsp_cfg.output);

#ifdef MEDIA_PROXY
    rtsp_free_proxies(&g_rtsp_cfg.proxy);
#endif

#ifdef MEDIA_PUSHER
    rtsp_free_pushers(&g_rtsp_cfg.pusher);
#endif

#ifdef RTSP_OVER_HTTP
    if (g_rtsp_cfg.rtsp_over_http)
    {
        rtsp_http_deinit();
    }
#endif

#ifdef EPOLL
    if (hrtsp.ep_fd)
    {
        close(hrtsp.ep_fd);
        hrtsp.ep_fd = 0;        
    }

    if (hrtsp.ep_events)
    {
        free(hrtsp.ep_events);
        hrtsp.ep_events = NULL;
    }
#endif

#if __WINDOWS_OS__ && defined(MEDIA_DEVICE)
    MFShutdown();
#endif

    rtsp_parse_buf_deinit();
}

int rtsp_net_listen_init(uint32 addr, uint16 port, int idx)
{
	struct sockaddr_in saddr;

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
	{
		return -1;
	}
	
	memset(&saddr, 0, sizeof(struct sockaddr_in));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
	saddr.sin_addr.s_addr = addr;

    if (bind(fd, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in)) < 0)
	{
		log_print(HT_LOG_ERR, "rtsp_net_listen_init::bind errno=%d!!!\r\n", errno);
		closesocket(fd);
		return -1;
	}

    if (listen(fd, 5) < 0)
	{
		log_print(HT_LOG_ERR, "rtsp_net_listen_init::listen errno=%d!!!\r\n", errno);
		closesocket(fd);
		return -1;
	}

	hrtsp.sfd[idx] = fd;
	hrtsp.sport = port;

#ifdef EPOLL
    uint64_t e_dat = hrtsp.sfd[idx];
    e_dat |= ((uint64_t)1 << 63);

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.u64 = e_dat;
    epoll_ctl(hrtsp.ep_fd, EPOLL_CTL_ADD, hrtsp.sfd[idx], &event);
#endif

	return fd;
}

void rtsp_listen_rx(int sfd)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	socklen_t size = sizeof(struct sockaddr_in);

	int cfd = accept(sfd, (struct sockaddr *)&addr, &size);
	if (cfd < 0)
	{
		log_print(HT_LOG_ERR, "rtsp_listen_rx::accept ret error[%d]!!!\r\n", errno);
		return;
	}

	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
		
	setsockopt(cfd, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));
	
	int len = 1024 * 1024;
	if (setsockopt(cfd, SOL_SOCKET, SO_SNDBUF, (char*)&len, sizeof(int)))
	{
		log_print(HT_LOG_WARN, "%s, setsockopt SO_SNDBUF error!!!\r\n", __FUNCTION__);
	}
	if (setsockopt(cfd, SOL_SOCKET, SO_RCVBUF, (char*)&len, sizeof(int)))
	{
		log_print(HT_LOG_WARN, "%s, setsockopt SO_SNDBUF error!!!\r\n", __FUNCTION__);
	}

	RSUA * p_rua = rua_get_idle_rua();
	if (p_rua == NULL)
	{
		log_print(HT_LOG_ERR, "rtsp_listen_rx::rua_get_idle_rua return NULL, close cfd(%d)!!!\r\n", cfd);
		closesocket(cfd);
		return;
	}
	
	p_rua->fd = cfd;	
	p_rua->fd_mutex = sys_os_create_mutex();

	p_rua->user_real_ip = addr.sin_addr.s_addr;
	p_rua->user_real_port = ntohs(addr.sin_port);

	p_rua->lats_rx_time = time(NULL);
	
#ifdef EPOLL
    uint64_t e_dat = rua_get_index(p_rua);
    e_dat = e_dat << 32;
    e_dat = e_dat | cfd;

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.u64 = e_dat;
    epoll_ctl(hrtsp.ep_fd, EPOLL_CTL_ADD, cfd, &event);
#endif

	log_print(HT_LOG_INFO, "new user over tcp from[%s,%u]\r\n", inet_ntoa(addr.sin_addr), p_rua->user_real_port);
	
	rua_set_online_rua(p_rua);
}

/***********************************************************************
 *
 * rtsp packet receive thread
 *
************************************************************************/
void * rtsp_rx_thread(void * argv)
{
    uint32 i;
    RSUA * p_cln;

	log_print(HT_LOG_DBG, "%s, start\r\n", __FUNCTION__);

	while (hrtsp.r_flag == 1)
	{
#ifdef EPOLL

		int fd, nfds;
		
        nfds = epoll_wait(hrtsp.ep_fd, hrtsp.ep_events, hrtsp.ep_event_num, 1000);

		for (i=0; i<nfds; i++)
		{
			if (hrtsp.ep_events[i].events & EPOLLIN)
			{
				fd = (int)(hrtsp.ep_events[i].data.u64);
				if ((hrtsp.ep_events[i].data.u64 & ((uint64_t)1 << 63)) != 0)
				{
				    rtsp_listen_rx(fd);
				}
				else
				{
					uint32 u_index = hrtsp.ep_events[i].data.u64 >> 32;
					p_cln = rua_get_by_index(u_index);
					if (p_cln->fd > 0 && p_cln->fd == fd)
					{
						if (!rtsp_tcp_rx(p_cln))
						{
							rtsp_stop_rua(p_cln);
						}
						else
						{
							p_cln->lats_rx_time = time(NULL);
						}
					}
					else
					{
						//log_print(HT_LOG_WARN, "%s, event fd[%d] not match user fd[%d]!!!\r\n", __FUNCTION__, fd, p_cln->fd);
					}
				}
			}
		}
		
#else

		fd_set fdr;
    	int max_fd = 0;
    
		FD_ZERO(&fdr);

        for (i = 0; i < hrtsp.local_ip_num; i++)
        {
            max_fd = ((int)hrtsp.sfd[i] > max_fd)? hrtsp.sfd[i] : max_fd;
		    FD_SET(hrtsp.sfd[i], &fdr);
		}

		for (i = 0; i < MAX_NUM_RUA; i++)
		{
			p_cln = rua_get_by_index(i);

			if (p_cln->fd > 0)
			{
				FD_SET(p_cln->fd, &fdr);
				max_fd = ((int)p_cln->fd > max_fd)? p_cln->fd : max_fd;
			}
		}

		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		
		int sret = select(max_fd+1, &fdr, NULL, NULL, &tv);
		if (sret == 0)
		{
			continue;
		}
		else if (sret < 0)
		{
			log_print(HT_LOG_ERR, "%s, select err[%s], sret[%d]!!!\r\n", __FUNCTION__, sys_os_get_socket_error(), sret);

			usleep(10*1000);
			continue;
		}

        for (i = 0; i < hrtsp.local_ip_num; i++)
        {
    		if (FD_ISSET(hrtsp.sfd[i], &fdr))
    		{
    			rtsp_listen_rx(hrtsp.sfd[i]);
    		}
		}

		for (i = 0; i < MAX_NUM_RUA; i++)
		{
			p_cln = rua_get_by_index(i);

			if (p_cln->fd > 0 && FD_ISSET(p_cln->fd, &fdr))
			{
				if (!rtsp_tcp_rx(p_cln))
				{
					rtsp_stop_rua(p_cln);
				}
				else
				{
					p_cln->lats_rx_time = time(NULL);
				}
			}
		}

#endif
	}

    hrtsp.tid_pkt_rx = 0;
    
	log_print(HT_LOG_DBG, "%s, exit\r\n", __FUNCTION__);

	return NULL;
}

/***********************************************************************
 *
 * rtsp main task
 *
***********************************************************************/
void * rtsp_task(void * argv)
{
	RIMSG stm;

	while (1)
	{
		if (hqBufGet(hrtsp.msg_queue, (char *)&stm))
		{
			RSUA * p_rua = rua_get_by_index(stm.msg_dua);
			switch(stm.msg_src)
			{
			case RTSP_MSG_SRC:
				rtsp_rx_msg(p_rua, (HRTSP_MSG *)stm.msg_buf);
				if (stm.msg_buf) rtsp_free_msg((HRTSP_MSG *)stm.msg_buf);
				break;

			case RTSP_DEL_UA_SRC:
				rtsp_close_rua(p_rua);
				break;

			case RTSP_TIMER_SRC:
				rtsp_timer();
				break;

			case RTSP_EXIT:
			    goto EXIT;
			}
		}
	}

EXIT:

    hrtsp.tid_main = 0;
    
	return NULL;
}

int rtsp_rx_msg(RSUA * p_rua, HRTSP_MSG * rx_msg)
{
	if (p_rua == NULL || rx_msg == NULL)
	{
		return -1;
	}
	
	if (p_rua->used_flag == 0)
	{
		return -1;
	}
	
	return rtsp_server_state(p_rua, rx_msg);
}

void rtsp_stop_rua(RSUA * p_rua)
{	
    if (p_rua->fd > 0)
    {
#ifdef EPOLL    
        epoll_ctl(hrtsp.ep_fd, EPOLL_CTL_DEL, p_rua->fd, NULL);        
#endif

        closesocket(p_rua->fd);
        p_rua->fd = 0;
    }
    else
    {
        return;
    }

    RIMSG msg;
	memset(&msg,0,sizeof(RIMSG));
	msg.msg_src = RTSP_DEL_UA_SRC;
	msg.msg_dua = rua_get_index(p_rua);
	msg.msg_buf = NULL;
	
	if (hqBufPut(hrtsp.msg_queue, (char *)&msg) == FALSE)
	{
		log_print(HT_LOG_ERR, "%s, send msg[NULL] to main task failed!!!\r\n", __FUNCTION__);
	}
}

int rtsp_msg_parser(RSUA * p_rua)
{
    int rtsp_pkt_len = rtsp_pkt_find_end(p_rua->rcv_buf);
	if (rtsp_pkt_len == 0)
	{
		return RTSP_PARSE_MOREDATE;
	}
	
	HRTSP_MSG * rx_msg = rtsp_get_msg_buf();
	if (rx_msg == NULL)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_get_msg_buf return null!!!\r\n", __FUNCTION__);
		return RTSP_PARSE_FAIL;
	}

	memcpy(rx_msg->msg_buf, p_rua->rcv_buf, rtsp_pkt_len);
	rx_msg->msg_buf[rtsp_pkt_len] = '\0';

	log_print(HT_LOG_DBG, "%s\r\n", rx_msg->msg_buf);

	int parse_len = rtsp_msg_parse_part1(rx_msg->msg_buf, rtsp_pkt_len, rx_msg);
	if (parse_len != rtsp_pkt_len)
	{
		log_print(HT_LOG_ERR, "%s, rtsp_msg_parse_part1=%d, rtsp_pkt_len=%d!!!\r\n", __FUNCTION__, parse_len, rtsp_pkt_len);
		rtsp_free_msg(rx_msg);
		return RTSP_PARSE_FAIL;
	}
	
	if (rx_msg->ctx_len > 0)
	{
		if (p_rua->rcv_dlen < (parse_len + rx_msg->ctx_len))
		{
			rtsp_free_msg(rx_msg);
			return RTSP_PARSE_MOREDATE;
		}

		memcpy(rx_msg->msg_buf+rtsp_pkt_len, p_rua->rcv_buf+rtsp_pkt_len, rx_msg->ctx_len);
		rx_msg->msg_buf[rtsp_pkt_len + rx_msg->ctx_len] = '\0';

		log_print(HT_LOG_DBG, "%s\r\n\r\n", rx_msg->msg_buf+rtsp_pkt_len);

		int sdp_parse_len = rtsp_msg_parse_part2(rx_msg->msg_buf+parse_len, rx_msg->ctx_len, rx_msg);
		if (sdp_parse_len != rx_msg->ctx_len)
		{
			log_print(HT_LOG_ERR, "rtsp_tcp_rx::rtsp_msg_parse_part2 = %d, sdp_pkt_len = %d!!!\r\n", sdp_parse_len, rx_msg->ctx_len);
			rtsp_free_msg(rx_msg);
			return RTSP_PARSE_FAIL;
		}
		parse_len += sdp_parse_len;
	}

	if (parse_len < p_rua->rcv_dlen)
	{
		while (p_rua->rcv_buf[parse_len] == ' ' || p_rua->rcv_buf[parse_len] == '\r' || p_rua->rcv_buf[parse_len] == '\n')
		{
			parse_len++;
		}
		
		memmove(p_rua->rcv_buf, p_rua->rcv_buf + parse_len, p_rua->rcv_dlen - parse_len);
        p_rua->rcv_dlen -= parse_len;
		p_rua->rcv_buf[p_rua->rcv_dlen] = '\0';
	}
	else
	{
        p_rua->rcv_dlen = 0;
    } 

    RIMSG msg;
	memset(&msg,0,sizeof(RIMSG));
	msg.msg_src = RTSP_MSG_SRC;
	msg.msg_dua = rua_get_index(p_rua);
	msg.msg_buf = (char *)rx_msg;
	
	if (hqBufPut(hrtsp.msg_queue, (char *)&msg) == FALSE)
	{
		rtsp_free_msg(rx_msg);
		log_print(HT_LOG_ERR, "%s, send rx msg to main task failed!!!\r\n", __FUNCTION__);
	}

    return RTSP_PARSE_SUCC;
}

void rtsp_data_rx(RSUA * p_rua, uint8 * lpData, int rlen)
{
    RILF  * p_rilf = (RILF *)lpData;
	uint8 * p_rtp = (uint8 *)p_rilf + 4;
	uint32	rtp_len = rlen - 4;
	
    if (rtp_len >= 2 && RTP_PT_IS_RTCP(p_rtp[1]))
	{
		return;
	}
	
#ifdef RTSP_BACKCHANNEL
	if (p_rua->backchannel)
	{
    	rtsp_bc_tcp_data_rx(p_rua, lpData, rlen);
    }
#endif

#ifdef MEDIA_PUSHER
	if (p_rua->media_info.is_pusher)
	{
		rtsp_tcp_data_rx(p_rua, lpData, rlen);
	}
#endif

    return;
}

BOOL rtsp_tcp_rx(RSUA * p_rua)
{
	if (p_rua->fd <= 0)
	{
		return FALSE;
	}

	if (p_rua->rtp_rcv_buf == NULL || p_rua->rtp_t_len == 0)
	{
		int rlen = recv(p_rua->fd, p_rua->rcv_buf+p_rua->rcv_dlen, 2048-p_rua->rcv_dlen, 0);
		if (rlen <= 0)
		{
			log_print(HT_LOG_ERR, "%s, recv ret = %d, rcv_dlen = %d\r\n", __FUNCTION__, rlen, p_rua->rcv_dlen);
		    return FALSE;
		}

		p_rua->rcv_dlen += rlen;

		if (p_rua->rcv_dlen < 16)
		{
			return TRUE;
		}
	}
	else
	{
		int rlen = recv(p_rua->fd, p_rua->rtp_rcv_buf+p_rua->rtp_rcv_len, p_rua->rtp_t_len-p_rua->rtp_rcv_len, 0);
		if (rlen <= 0)
		{
			log_print(HT_LOG_WARN, "%s, ret=%d, err=%s\r\n", __FUNCTION__, rlen, sys_os_get_socket_error());	//recv error, connection maybe disconn?
			return FALSE;
		}

		p_rua->rtp_rcv_len += rlen;
		
		if (p_rua->rtp_rcv_len == p_rua->rtp_t_len)
		{
			rtsp_data_rx(p_rua, (uint8*)p_rua->rtp_rcv_buf, p_rua->rtp_rcv_len);
			
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
		if (ret == RTSP_PARSE_FAIL)
		{
		    return FALSE;
		}
		else if (ret == RTSP_PARSE_MOREDATE)
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
			
		rtsp_data_rx(p_rua, (uint8*)p_rilf, rtp_len+4);

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


BOOL rtsp_options_req(RSUA * p_rua, HRTSP_MSG * rx_msg)
{
	HRTSP_MSG * tx_msg = rua_build_options_response(p_rua);
	if (tx_msg == NULL) 
	{
		return FALSE;
	}
	
	rsua_send_free_rtsp_msg(p_rua,tx_msg);

	return TRUE;
}

void rtsp_add_video_cap(RSUA * p_rua, uint8 pt, const char * desc)
{
	p_rua->channels[AV_VIDEO_CH].cap[0] = pt;
	strcpy(p_rua->channels[AV_VIDEO_CH].cap_desc[0], desc);

	char * sdp = rtsp_media_get_video_sdp_line(p_rua);
	if (sdp)
	{
		strncpy(p_rua->channels[AV_VIDEO_CH].cap_desc[1], sdp, sizeof(p_rua->channels[AV_VIDEO_CH].cap_desc[1])-1);
		delete [] sdp;
	}
}

void rtsp_setup_video(RSUA * p_rua)
{
    p_rua->channels[AV_VIDEO_CH].cap_count = 1;

	if (p_rua->media_info.v_codec == VIDEO_CODEC_H264)
	{
		rtsp_add_video_cap(p_rua, 96, "a=rtpmap:96 H264/90000");
	}
	else if (p_rua->media_info.v_codec == VIDEO_CODEC_H265)
	{
		rtsp_add_video_cap(p_rua, 96, "a=rtpmap:96 H265/90000");
	}
	else if (p_rua->media_info.v_codec == VIDEO_CODEC_MP4)
	{
		rtsp_add_video_cap(p_rua, 96, "a=rtpmap:96 MP4V-ES/90000");
	}
	else if (p_rua->media_info.v_codec == VIDEO_CODEC_JPEG)
	{
		rtsp_add_video_cap(p_rua, 26, "a=rtpmap:26 JPEG/90000");
	}
	
	p_rua->channels[AV_VIDEO_CH].rtp_info.rtp_ts = 0;
	p_rua->channels[AV_VIDEO_CH].rtp_info.rtp_pt = p_rua->channels[AV_VIDEO_CH].cap[0];
	p_rua->channels[AV_VIDEO_CH].rtp_info.rtp_ssrc = rand();
	p_rua->channels[AV_VIDEO_CH].rtp_info.rtp_cnt = 0;
}

void rtsp_setup_audio(RSUA * p_rua)
{
    int desc_idx = 0;
		
	p_rua->channels[AV_AUDIO_CH].cap_count = 1;

	if (p_rua->media_info.a_codec == AUDIO_CODEC_G711A)
	{
		p_rua->channels[AV_AUDIO_CH].cap[0] = 8;
		
		sprintf(p_rua->channels[AV_AUDIO_CH].cap_desc[desc_idx++], 
		    "a=rtpmap:%d PCMA/%d/%d", 
		    p_rua->channels[AV_AUDIO_CH].cap[0],
		    p_rua->media_info.a_samplerate, 
		    p_rua->media_info.a_channels);
	}	
	else if (p_rua->media_info.a_codec == AUDIO_CODEC_G711U)
	{
		p_rua->channels[AV_AUDIO_CH].cap[0] = 0;
		
		sprintf(p_rua->channels[AV_AUDIO_CH].cap_desc[desc_idx++], 
		    "a=rtpmap:%d PCMU/%d/%d", 
		    p_rua->channels[AV_AUDIO_CH].cap[0],
		    p_rua->media_info.a_samplerate, 
		    p_rua->media_info.a_channels);
	}
	else if (p_rua->media_info.a_codec == AUDIO_CODEC_G726)
	{
		p_rua->channels[AV_AUDIO_CH].cap[0] = 97;	

		// G726 8000 1 32kbit/s
		sprintf(p_rua->channels[AV_AUDIO_CH].cap_desc[desc_idx++], 
		    "a=rtpmap:%d G726-32/%d/1",
		    p_rua->channels[AV_AUDIO_CH].cap[0],
		    p_rua->media_info.a_samplerate);
	}
	else if (p_rua->media_info.a_codec == AUDIO_CODEC_AAC)
	{
		p_rua->channels[AV_AUDIO_CH].cap[0] = 97;
		
		sprintf(p_rua->channels[AV_AUDIO_CH].cap_desc[desc_idx++], 
		    "a=rtpmap:%d MPEG4-GENERIC/%d/%d", 
		    p_rua->channels[AV_AUDIO_CH].cap[0],
		    p_rua->media_info.a_samplerate, 
		    p_rua->media_info.a_channels);

		char * sdp = rtsp_media_get_audio_sdp_line(p_rua);
		if (sdp)
		{
			strcpy(p_rua->channels[AV_AUDIO_CH].cap_desc[desc_idx++], sdp);
			delete [] sdp;
		}
	}
	else if (p_rua->media_info.a_codec == AUDIO_CODEC_G722)
	{
	    p_rua->channels[AV_AUDIO_CH].cap[0] = 9;
	    
		sprintf(p_rua->channels[AV_AUDIO_CH].cap_desc[desc_idx++], 
		    "a=rtpmap:%d G722/8000",
		    p_rua->channels[AV_AUDIO_CH].cap[0]);
	}
	else if (p_rua->media_info.a_codec == AUDIO_CODEC_OPUS)
	{
	    p_rua->channels[AV_AUDIO_CH].cap[0] = 97;
	    
		sprintf(p_rua->channels[AV_AUDIO_CH].cap_desc[desc_idx++], 
		    "a=rtpmap:%d opus/48000/2",
		    p_rua->channels[AV_AUDIO_CH].cap[0] = 97);
	}

	sprintf(p_rua->channels[AV_AUDIO_CH].cap_desc[desc_idx++], "a=recvonly");
	
	p_rua->channels[AV_AUDIO_CH].rtp_info.rtp_ts = 0;
	p_rua->channels[AV_AUDIO_CH].rtp_info.rtp_pt = p_rua->channels[AV_AUDIO_CH].cap[0];
	p_rua->channels[AV_AUDIO_CH].rtp_info.rtp_ssrc = rand();
	p_rua->channels[AV_AUDIO_CH].rtp_info.rtp_cnt = 0;
}

#ifdef RTSP_METADATA

void rtsp_setup_metadata(RSUA * p_rua)
{
    p_rua->media_info.metadata = 1;
		
	strcpy(p_rua->channels[AV_METADATA_CH].ctl, "metadata");
	p_rua->channels[AV_METADATA_CH].interleaved = 6;

	p_rua->channels[AV_METADATA_CH].cap_count = 1;
	p_rua->channels[AV_METADATA_CH].cap[0] = 98;
	sprintf(p_rua->channels[AV_METADATA_CH].cap_desc[0], "a=rtpmap:98 vnd.onvif.metadata/90000");
	
	p_rua->channels[AV_METADATA_CH].rtp_info.rtp_ts = 0;
	p_rua->channels[AV_METADATA_CH].rtp_info.rtp_pt = p_rua->channels[AV_METADATA_CH].cap[0];
	p_rua->channels[AV_METADATA_CH].rtp_info.rtp_ssrc = rand();
	p_rua->channels[AV_METADATA_CH].rtp_info.rtp_cnt = 0;
}

#endif // #ifdef RTSP_METADATA

#ifdef RTSP_BACKCHANNEL

void rtsp_setup_backchannel(RSUA * p_rua)
{
    strcpy(p_rua->channels[AV_BACK_CH].ctl, "audioback");
    p_rua->channels[AV_BACK_CH].interleaved = 8;

    rtsp_cfg_get_backchannel_info(&p_rua->bc_codec, &p_rua->bc_samplerate, &p_rua->bc_channels);

    if (p_rua->bc_codec == AUDIO_CODEC_NONE)
    {
        p_rua->bc_codec = AUDIO_CODEC_G711U;
    }

    if (p_rua->bc_samplerate == 0)
    {
        p_rua->bc_samplerate = 8000;
    }

    if (p_rua->bc_channels == 0)
    {
        p_rua->bc_channels = 1;
    }

    rtsp_bc_parse_url_parameters(p_rua);
    
	p_rua->channels[AV_BACK_CH].cap_count = 1;
	
	if (p_rua->bc_codec == AUDIO_CODEC_G711A)
	{
		p_rua->channels[AV_BACK_CH].cap[0] = 8;
		
		sprintf(p_rua->channels[AV_BACK_CH].cap_desc[0], 
		    "a=rtpmap:%d PCMA/%d/%d", 
		    p_rua->channels[AV_BACK_CH].cap[0], 
		    p_rua->bc_samplerate, 
		    p_rua->bc_channels);
	}
	else if (p_rua->bc_codec == AUDIO_CODEC_G711U)
	{
		p_rua->channels[AV_BACK_CH].cap[0] = 0;
		
		sprintf(p_rua->channels[AV_BACK_CH].cap_desc[0], 
		    "a=rtpmap:%d PCMU/%d/%d", 
		    p_rua->channels[AV_BACK_CH].cap[0] = 0, 
		    p_rua->bc_samplerate, 
		    p_rua->bc_channels);
	}
	else if (p_rua->bc_codec == AUDIO_CODEC_G726)
	{
		p_rua->bc_channels = 1; // G726 only support mono
		p_rua->bc_samplerate = 8000;
		p_rua->channels[AV_BACK_CH].cap[0] = 99;
		
		sprintf(p_rua->channels[AV_BACK_CH].cap_desc[0], 
		    "a=rtpmap:%d G726-32/%d/%d", 
		    p_rua->channels[AV_BACK_CH].cap[0], 
		    p_rua->bc_samplerate, 
		    p_rua->bc_channels);
	}
	else if (p_rua->bc_codec == AUDIO_CODEC_G722)
	{
	    p_rua->bc_channels = 1; // G722 only support mono
		p_rua->bc_samplerate = 16000;
	    p_rua->channels[AV_BACK_CH].cap[0] = 9;
	    
		sprintf(p_rua->channels[AV_BACK_CH].cap_desc[0], 
		    "a=rtpmap:%d G722/8000",
		    p_rua->channels[AV_BACK_CH].cap[0]);
	}
	else if (p_rua->bc_codec == AUDIO_CODEC_OPUS)
	{
	    p_rua->bc_channels = 2;
		p_rua->bc_samplerate = 48000;
	    p_rua->channels[AV_BACK_CH].cap[0] = 99;
	    
		sprintf(p_rua->channels[AV_BACK_CH].cap_desc[0], 
		    "a=rtpmap:%d opus/48000/2",
		    p_rua->channels[AV_BACK_CH].cap[0]);
	}
	else if (p_rua->bc_codec == AUDIO_CODEC_AAC)
	{
		p_rua->channels[AV_BACK_CH].cap[0] = 99;
		
		sprintf(p_rua->channels[AV_BACK_CH].cap_desc[0], 
		    "a=rtpmap:%d MPEG4-GENERIC/%d/%d", 
		    p_rua->channels[AV_BACK_CH].cap[0],
		    p_rua->bc_samplerate, 
		    p_rua->bc_channels);
	}
	
	sprintf(p_rua->channels[AV_BACK_CH].cap_desc[1], "a=sendonly");

	p_rua->channels[AV_BACK_CH].rtp_info.rtp_ts = 0;
	p_rua->channels[AV_BACK_CH].rtp_info.rtp_pt = p_rua->channels[AV_BACK_CH].cap[0];
	p_rua->channels[AV_BACK_CH].rtp_info.rtp_ssrc = rand();
	p_rua->channels[AV_BACK_CH].rtp_info.rtp_cnt = 0;
}

#endif // #ifdef RTSP_BACKCHANNEL

void rtsp_setup_multicast(RSUA * p_rua)
{
    RMCUA * p_mcua = rtsp_mc_add_ref(p_rua);
    if (p_mcua)
    {
        RSUA * pmc = rua_get_by_index(p_mcua->mcuaidx);

        p_rua->mc_ref = 1;
        
        p_rua->channels[AV_VIDEO_CH].rtp_info.rtp_ssrc = pmc->channels[AV_VIDEO_CH].rtp_info.rtp_ssrc;
        p_rua->channels[AV_AUDIO_CH].rtp_info.rtp_ssrc = pmc->channels[AV_AUDIO_CH].rtp_info.rtp_ssrc;
        
        p_rua->channels[AV_VIDEO_CH].l_port = pmc->channels[AV_VIDEO_CH].l_port;
        p_rua->channels[AV_AUDIO_CH].l_port = pmc->channels[AV_AUDIO_CH].l_port;
        strcpy(p_rua->channels[AV_VIDEO_CH].destination, pmc->channels[AV_VIDEO_CH].destination);
        strcpy(p_rua->channels[AV_AUDIO_CH].destination, pmc->channels[AV_AUDIO_CH].destination);

#ifdef RTSP_METADATA
        p_rua->channels[AV_METADATA_CH].rtp_info.rtp_ssrc = pmc->channels[AV_METADATA_CH].rtp_info.rtp_ssrc;
		p_rua->channels[AV_METADATA_CH].l_port = pmc->channels[AV_METADATA_CH].l_port;
        strcpy(p_rua->channels[AV_METADATA_CH].destination, pmc->channels[AV_METADATA_CH].destination);
#endif

#ifdef RTSP_BACKCHANNEL
        p_rua->channels[AV_BACK_CH].rtp_info.rtp_ssrc = pmc->channels[AV_BACK_CH].rtp_info.rtp_ssrc;
		p_rua->channels[AV_BACK_CH].l_port = pmc->channels[AV_BACK_CH].l_port;
		strcpy(p_rua->channels[AV_BACK_CH].destination, pmc->channels[AV_BACK_CH].destination);
#endif
    }
    else
    {
        p_rua->channels[AV_VIDEO_CH].l_port = rtsp_get_udp_port();
        p_rua->channels[AV_AUDIO_CH].l_port = rtsp_get_udp_port();
        sprintf(p_rua->channels[AV_VIDEO_CH].destination, "232.%u.%u.%u", rand()%256, rand()%256, rand()%256);
        sprintf(p_rua->channels[AV_AUDIO_CH].destination, "232.%u.%u.%u", rand()%256, rand()%256, rand()%256);

#ifdef RTSP_METADATA
		p_rua->channels[AV_METADATA_CH].l_port = rtsp_get_udp_port();
        sprintf(p_rua->channels[AV_METADATA_CH].destination, "232.%u.%u.%u", rand()%256, rand()%256, rand()%256);
#endif

#ifdef RTSP_BACKCHANNEL
		p_rua->channels[AV_BACK_CH].l_port = rtsp_get_udp_port();
		sprintf(p_rua->channels[AV_BACK_CH].destination, "232.%u.%u.%u", rand()%256, rand()%256, rand()%256);
#endif
    }
}

BOOL rtsp_describe_req(RSUA * p_rua, HRTSP_MSG * rx_msg)
{
	char accept_buf[32];
	char require_buf[256];

	if (rtsp_get_headline_string(rx_msg, "Accept", accept_buf, sizeof(accept_buf)) == FALSE)
	{
		return FALSE;
	}
	
	if (strcasecmp(accept_buf, "application/sdp") != 0)
	{
		return FALSE;
	}
	
	if (rtsp_media_init(p_rua) == FALSE)
	{
		return FALSE;
	}

    if (rtsp_get_headline_string(rx_msg, "Require", require_buf, sizeof(require_buf)) == TRUE)
    {
        if (strcasecmp(require_buf, "www.onvif.org/ver20/backchannel") == 0)
        {
#ifdef RTSP_BACKCHANNEL        
            p_rua->backchannel = 1;
#else
			HRTSP_MSG * tx_msg = rua_build_response(p_rua, "551 Option not supported");
			if (tx_msg == NULL)
			{
				return FALSE;
			}
			
			rsua_send_free_rtsp_msg(p_rua,tx_msg);
			return TRUE;
#endif
        }
    }

    sprintf(p_rua->sid, "%u", rand());
	sprintf(p_rua->cbase, "%s", p_rua->uri);
	
	strcpy(p_rua->channels[AV_VIDEO_CH].ctl, "realvideo");
	strcpy(p_rua->channels[AV_AUDIO_CH].ctl, "realaudio");
	
	p_rua->channels[AV_VIDEO_CH].interleaved = 2;
	p_rua->channels[AV_AUDIO_CH].interleaved = 4;
	
	// setup SDP
	if (p_rua->media_info.has_video)
	{
	    rtsp_setup_video(p_rua);
	}

	if (p_rua->media_info.has_audio)
	{
	    rtsp_setup_audio(p_rua);
	}

#ifdef RTSP_METADATA
	if (g_rtsp_cfg.metadata)
	{
	    rtsp_setup_metadata(p_rua);
	}
#endif

#ifdef RTSP_BACKCHANNEL
    if (p_rua->backchannel)
	{
	    rtsp_setup_backchannel(p_rua);
	}
#endif

    if (g_rtsp_cfg.multicast && 0 == p_rua->rtp_unicast)
    {
        rtsp_setup_multicast(p_rua);
    }
	
	HRTSP_MSG * tx_msg = rua_build_descibe_response(p_rua);
	if (tx_msg == NULL) 
	{
		return FALSE;
	}
	
	rsua_send_free_rtsp_msg(p_rua, tx_msg);

	p_rua->state = RSS_DESCRIBE;

	return TRUE;
}

BOOL rtsp_setup_req(RSUA * p_rua, HRTSP_MSG * rx_msg)
{
	int i, av_t = -1;
	char uri[512];
	char require_buf[256];
    char transport_buf[256];
    
    for (i = 0; i < AV_MAX_CHS; i++)
    {
        sprintf(uri, "%s/%s", p_rua->cbase, p_rua->channels[i].ctl);

        if (strcasecmp(uri, p_rua->uri) == 0)
    	{
    		av_t = i;
    		p_rua->channels[i].setup = 1;

    		break;
    	}
    }
	
	if (av_t < 0) 
	{
		return FALSE;
	}
	
	if (rtsp_get_headline_string(rx_msg, "Transport", transport_buf, sizeof(transport_buf)) == FALSE)
	{
		return FALSE;
	}
	
	if (rua_get_transport_info(p_rua, transport_buf, av_t) == FALSE)
	{
		return FALSE;
	}

    if (rtsp_get_headline_string(rx_msg, "Require", require_buf, sizeof(require_buf)) == TRUE)
    {
        if (strcasecmp(require_buf, "onvif-replay") == 0)
        {

#ifdef RTSP_REPLAY        
            p_rua->replay = 1;
#else
			HRTSP_MSG * tx_msg = rua_build_response(p_rua, "551 Option not supported");
			if (tx_msg == NULL)
			{
				return FALSE;
			}
			
			rsua_send_free_rtsp_msg(p_rua,tx_msg);
			return TRUE;
#endif
        }
    }
    else
    {
        p_rua->replay = 0;
    }
    
	if (p_rua->rtp_tcp == 0)
	{
	    BOOL ret = FALSE;
	    
	    if (1 == p_rua->rtp_unicast)
	    {
	        ret = rsua_init_udp_connection(p_rua, av_t, hrtsp.local_ip[0]);
#ifdef RTSP_RTCP
			if (ret)
			{
				rsua_init_rtcp_udp_connection(p_rua, av_t, hrtsp.local_ip[0]);
			}
#endif
	    }
	    else
	    {
	        if (p_rua->channels[av_t].r_port == 0)
	        {
	            p_rua->channels[av_t].r_port = p_rua->channels[av_t].l_port;
	        }
	        else if (p_rua->channels[av_t].r_port != p_rua->channels[av_t].l_port)
	        {
	            p_rua->channels[av_t].l_port = p_rua->channels[av_t].r_port;
	        }
	        
	        ret = rsua_init_mc_connection(p_rua, av_t, hrtsp.local_ip[0]);
#ifdef RTSP_RTCP
			if (ret)
			{
				rsua_init_rtcp_mc_connection(p_rua, av_t, hrtsp.local_ip[0]);
			}
#endif	        
	    }
	    
		if (FALSE == ret)
		{
			HRTSP_MSG * tx_msg = rua_build_response(p_rua, "461 Unsupported Transport");
			if (tx_msg == NULL)
			{
				return FALSE;
			}
			
			rsua_send_free_rtsp_msg(p_rua,tx_msg);
			return TRUE;
		}
	}
	
	HRTSP_MSG * tx_msg = rua_build_setup_response(p_rua, av_t);
	if (tx_msg == NULL)
	{
		return FALSE;
	}	
	
	rsua_send_free_rtsp_msg(p_rua, tx_msg);

	p_rua->state = RSS_INIT_V;

	return TRUE;
}

BOOL rtsp_play_req(RSUA * p_rua, HRTSP_MSG * rx_msg)
{
    char range[256];
    char require_buf[256];

    if (rtsp_get_headline_string(rx_msg, "Require", require_buf, sizeof(require_buf)) == TRUE)
    {
        if (strcasecmp(require_buf, "onvif-replay") == 0)
        {
#ifdef RTSP_REPLAY        
            p_rua->replay = 1;
#else
			HRTSP_MSG * tx_msg = rua_build_response(p_rua, "551 Option not supported");
			if (tx_msg == NULL)
			{
				return FALSE;
			}
			
			rsua_send_free_rtsp_msg(p_rua,tx_msg);
			return TRUE;
#endif
        }
        else
        {
            p_rua->replay = 0;
        }
    }
    else
    {
        p_rua->replay = 0;
    }

#ifdef RTSP_REPLAY
    rtsp_get_scale_info(rx_msg, &p_rua->scale);
    rtsp_get_rate_control(rx_msg, &p_rua->rate_control);
    rtsp_get_immediate(rx_msg, &p_rua->immediate);
    rtsp_get_frame_info(rx_msg, &p_rua->frame, &p_rua->frame_interval);

    // If start a new play command immediately, update the cseq
    p_rua->channels[AV_VIDEO_CH].rep_hdr.seq = (uint8) (p_rua->cseq & 0xFF);
    p_rua->channels[AV_AUDIO_CH].rep_hdr.seq = (uint8) (p_rua->cseq & 0xFF);
#ifdef RTSP_METADATA    
    p_rua->channels[AV_METADATA_CH].rep_hdr.seq = (uint8) (p_rua->cseq & 0xFF);
#endif

    if (p_rua->immediate)
    {
        p_rua->channels[AV_VIDEO_CH].rep_hdr.d = 1;
        p_rua->channels[AV_AUDIO_CH].rep_hdr.d = 1;
#ifdef RTSP_METADATA        
        p_rua->channels[AV_METADATA_CH].rep_hdr.d = 1;
#endif        
    }
#endif

	if (rtsp_get_headline_string(rx_msg, "Range", range, sizeof(range)) == TRUE)
	{
		p_rua->play_range = rua_get_play_range_info(p_rua, range);
	}
	else
	{
	    p_rua->play_range = 0;
	}

#ifdef MEDIA_FILE
    if (p_rua->play_range)
    {
        CFileDemux * pDemux = p_rua->media_info.file_demuxer;
        if (pDemux)
        {
            if (pDemux->seekStream(p_rua->play_range_begin))
            {
                p_rua->media_info.curpos = pDemux->getCurPos();
            }
        }
    }
#endif	

#ifdef RTSP_BACKCHANNEL
    if (p_rua->backchannel && !p_rua->ad_inited)
    {
        if (!rtsp_bc_init_audio(p_rua))
        {
            log_print(HT_LOG_ERR, "%s, rtsp_bc_init_audio failed\r\n", __FUNCTION__);
        }

        if (!p_rua->rtp_tcp)
		{
		    p_rua->rtp_rx = 1;
	    	p_rua->tid_udp_rx = sys_os_create_thread((void *)rtsp_bc_udp_rx_thread, p_rua);
	    }
    }
#endif

#ifdef RTSP_RTCP
    if (!p_rua->rtp_tcp)
    {
        p_rua->rtcp_rx = 1;
        p_rua->tid_rtcp_rx = sys_os_create_thread((void *)rtsp_rtcp_rx_thread, p_rua);
    }
#endif

	HRTSP_MSG * tx_msg = rua_build_play_response(p_rua);
	if (tx_msg == NULL)
	{
		return FALSE;
	}
	
	rsua_send_free_rtsp_msg(p_rua, tx_msg);

    if (p_rua->mc_ref)
    {
        if (p_rua->rtp_tcp || p_rua->rtp_unicast) 
        {
            // non multicast, remove the multicast reference
            
            rtsp_mc_del_ref(p_rua);
            p_rua->mc_ref = 0;
        }
    }
    else
    {
        if (0 == p_rua->mc_src && 0 == p_rua->rtp_tcp && 0 == p_rua->rtp_unicast)
        {
            // multicast source, add to rtsp multicast ua list
            
            rtsp_mc_add(p_rua);
            p_rua->mc_src = 1;
        }
    }
    
    if (p_rua->state == RSS_PAUSE)
    {
        rtsp_restart_stream_tx(p_rua);

	    p_rua->state = RSS_PLAYING;
    }
	else if (p_rua->state != RSS_PLAYING)
	{
	    if (0 == p_rua->mc_ref)
	    {
	        rtsp_start_stream_tx(p_rua);
	    }
	    else
	    {
	        // reference the other multicast source stream
	    }

	    p_rua->state = RSS_PLAYING;
	}

	return TRUE;
}

BOOL rtsp_pause_req(RSUA * p_rua, HRTSP_MSG * rx_msg)
{
    if (p_rua->state == RSS_PLAYING)
	{
	    HRTSP_MSG * tx_msg = rua_build_response(p_rua, "200 OK");
    	if (tx_msg == NULL)
    	{
    		return FALSE;
    	}

	    rsua_send_free_rtsp_msg(p_rua, tx_msg);
	
	    if (rtsp_pause_stream_tx(p_rua))
        {
	        p_rua->state = RSS_PAUSE;
	    }
	}

	return TRUE;
}

BOOL rtsp_teardown_req(RSUA * p_rua, HRTSP_MSG * rx_msg)
{
	HRTSP_MSG * tx_msg = rua_build_response(p_rua, "200 OK");
	if (tx_msg == NULL) 
	{
		return FALSE;
	}
	
	rsua_send_free_rtsp_msg(p_rua,tx_msg);

	rtsp_stop_stream_tx(p_rua);
	
	p_rua->state = RSS_NULL;

	return TRUE;
}

BOOL rtsp_get_parameter_req(RSUA * p_rua, HRTSP_MSG * rx_msg)
{
    HRTSP_MSG * tx_msg = rua_build_response(p_rua, "200 OK");
	if (tx_msg == NULL) 
	{
		return FALSE;
	}
	
	rsua_send_free_rtsp_msg(p_rua,tx_msg);

	return TRUE;
}

BOOL rtsp_set_parameter_req(RSUA * p_rua, HRTSP_MSG * rx_msg)
{
	HRTSP_MSG * tx_msg = rua_build_response(p_rua, "200 OK");
	if (tx_msg == NULL) 
	{
		return FALSE;
	}
	
	rsua_send_free_rtsp_msg(p_rua,tx_msg);

	return TRUE;
}

BOOL rtsp_auth_process(RSUA * p_rua, const char * methond, HD_AUTH_INFO * p_auth)
{
    HASHHEX HA1;
	HASHHEX HA2 = "";
	const char * auth_pass;

	char calc_response[36];

    auth_pass = rtsp_get_user_pass(p_auth->auth_name);
	if (NULL == auth_pass)	// user not exist
	{
		return FALSE;
	}
	
	DigestCalcHA1("md5", p_auth->auth_name, p_rua->auth_info.auth_realm, auth_pass, p_auth->auth_nonce, p_auth->auth_cnonce, HA1);
    
	DigestCalcResponse(HA1, p_rua->auth_info.auth_nonce, p_auth->auth_ncstr, p_auth->auth_cnonce,
		p_auth->auth_qop, methond, p_auth->auth_uri, HA2, calc_response);
		
	if (strcmp(calc_response, p_auth->auth_response) == 0)
	{
		return TRUE;
	}
	
	return FALSE;
}

BOOL rtsp_security_rly(RSUA * p_rua, HRTSP_MSG * rx_msg)
{
    HRTSP_MSG * tx_msg = rua_build_security_response(p_rua);
	if (tx_msg == NULL) 
	{
		return FALSE;
	}
	
	rsua_send_free_rtsp_msg(p_rua, tx_msg);

	return TRUE;
}

#ifdef MEDIA_PUSHER

BOOL rtsp_get_media_info(RSUA * p_rua, HRTSP_MSG * rx_msg)
{
	if (rx_msg == NULL || p_rua == NULL)
	{
		return FALSE;
	}
	
	if (rtsp_msg_with_sdp(rx_msg))
	{
		rtsp_get_remote_cap(rx_msg, "video", &(p_rua->channels[AV_VIDEO_CH].cap_count), 
		    p_rua->channels[AV_VIDEO_CH].cap, &p_rua->channels[AV_VIDEO_CH].r_port);
		rtsp_get_remote_cap_desc(rx_msg, "video", p_rua->channels[AV_VIDEO_CH].cap_desc);

		rtsp_get_remote_cap(rx_msg, "audio", &(p_rua->channels[AV_AUDIO_CH].cap_count), 
		    p_rua->channels[AV_AUDIO_CH].cap, &p_rua->channels[AV_AUDIO_CH].r_port);
		rtsp_get_remote_cap_desc(rx_msg, "audio", p_rua->channels[AV_AUDIO_CH].cap_desc);
		
		return TRUE;
	}
	
	return FALSE;
}

BOOL rtsp_get_video_media_info(RSUA * p_rua)
{
	if (p_rua->channels[AV_VIDEO_CH].cap_count == 0)
	{
		return FALSE;
	}

	if (p_rua->channels[AV_VIDEO_CH].cap[0] == 26)
	{
		p_rua->media_info.v_codec = VIDEO_CODEC_JPEG;
	}

	int i;
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

			if (strcasecmp(code_buf, "H264/90000") == 0)
			{
				p_rua->media_info.v_codec = VIDEO_CODEC_H264;
			}
			else if (strcasecmp(code_buf, "JPEG/90000") == 0)
			{
				p_rua->media_info.v_codec = VIDEO_CODEC_JPEG;
			}
			else if (strcasecmp(code_buf, "MP4V-ES/90000") == 0)
			{
				p_rua->media_info.v_codec = VIDEO_CODEC_MP4;
			}
			else if (strcasecmp(code_buf, "H265/90000") == 0)
			{
				p_rua->media_info.v_codec = VIDEO_CODEC_H265;
			}

			break;
		}
	}

	return TRUE;
}

BOOL rtsp_get_audio_media_info(RSUA * p_rua)
{
	if (p_rua->channels[AV_AUDIO_CH].cap_count == 0)
	{
		return FALSE;
	}

	if (p_rua->channels[AV_AUDIO_CH].cap[0] == 0)
	{
		p_rua->media_info.a_codec = AUDIO_CODEC_G711U;
	}
	else if (p_rua->channels[AV_AUDIO_CH].cap[0] == 8)
	{
		p_rua->media_info.a_codec = AUDIO_CODEC_G711A;
	}
	else if (p_rua->channels[AV_AUDIO_CH].cap[0] == 9)
	{
		p_rua->media_info.a_codec = AUDIO_CODEC_G722;
	}

	int i;
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

			uppercase(code_buf);

			if (strstr(code_buf, "G726-32"))
			{
				p_rua->media_info.a_codec = AUDIO_CODEC_G726;
			}
			else if (strstr(code_buf, "G722"))
			{
				p_rua->media_info.a_codec = AUDIO_CODEC_G722;
			}
			else if (strstr(code_buf, "PCMU"))
			{
				p_rua->media_info.a_codec = AUDIO_CODEC_G711U;
			}
			else if (strstr(code_buf, "PCMA"))
			{
				p_rua->media_info.a_codec = AUDIO_CODEC_G711A;
			}
			else if (strstr(code_buf, "MPEG4-GENERIC"))
			{
				p_rua->media_info.a_codec = AUDIO_CODEC_AAC;
			}
			else if (strstr(code_buf, "OPUS"))
			{
				p_rua->media_info.a_codec = AUDIO_CODEC_OPUS;
			}

			char * p = strchr(code_buf, '/');
			if (p)
			{
				p++;
				
				char * p1 = strchr(p, '/');
				if (p1)
				{
					*p1 = '\0';
					p_rua->media_info.a_samplerate = atoi(p);

					p1++;
					if (NULL != p1 && *p1 != '\0')
					{
						p_rua->media_info.a_channels = atoi(p1);
					}
					else
					{
						p_rua->media_info.a_channels = 1;
					}
				}
				else
				{
					p_rua->media_info.a_samplerate = atoi(p);
					p_rua->media_info.a_channels = 1;
				}
			}

			break;
		}
	}

    if (p_rua->media_info.a_codec == AUDIO_CODEC_G722)
    {
        p_rua->media_info.a_samplerate = 16000;
        p_rua->media_info.a_channels = 1;
    }
    
	return TRUE;
}

BOOL rtsp_announce_req(RSUA * p_rua, HRTSP_MSG * rx_msg)
{
	sprintf(p_rua->cbase, "%s", p_rua->uri);

	if (rtsp_media_init(p_rua) == FALSE)
	{
		return FALSE;
	}
	
	if (!p_rua->media_info.is_pusher || !p_rua->media_info.pusher)
	{
		return FALSE;
	}
	
	p_rua->media_info.rtsp_pusher = 1;
	
	if (p_rua->media_info.pusher->getRua())
	{
		// Prevent multiple clients from pushing at the same time 
		return FALSE;
	}

	p_rua->media_info.pusher->setRua(p_rua);
	
	if (rtsp_find_sdp_control(rx_msg, p_rua->channels[AV_VIDEO_CH].ctl, "video", sizeof(p_rua->channels[AV_VIDEO_CH].ctl)-1))
	{
		p_rua->media_info.has_video = 1;
	}

	if (rtsp_find_sdp_control(rx_msg, p_rua->channels[AV_AUDIO_CH].ctl, "audio", sizeof(p_rua->channels[AV_AUDIO_CH].ctl)-1))
	{
		p_rua->media_info.has_audio = 1;
	}

	if (rtsp_get_media_info(p_rua, rx_msg))
	{
		if (rtsp_get_video_media_info(p_rua))
		{
			p_rua->media_info.pusher->setVideoInfo(p_rua->media_info.has_video, p_rua->media_info.v_codec);
		}
		
		if (rtsp_get_audio_media_info(p_rua))
		{
			p_rua->media_info.pusher->setAudioInfo(p_rua->media_info.has_audio, p_rua->media_info.a_codec, 
				p_rua->media_info.a_samplerate, p_rua->media_info.a_channels);
		}
	}

	if (p_rua->media_info.a_codec == AUDIO_CODEC_AAC)
	{
		int sizelength = 13;
		int indexlength = 3;
		int indexdeltalength = 3;
		char config[128];
		
		rsua_get_sdp_aac_params(p_rua, NULL, &sizelength, &indexlength, &indexdeltalength, config, sizeof(config));

		p_rua->media_info.pusher->setAACConfig(sizelength, indexlength, indexdeltalength);
	}

	if (p_rua->media_info.v_codec == VIDEO_CODEC_MP4)
	{
		char config[1000];
		
		if (rsua_get_sdp_mp4_params(p_rua, NULL, config, sizeof(config)-1))
		{
			uint32  configLen;
			uint8 * configData = mpeg4_parse_config(config, configLen);
		    if (configData)
		    {
		    	p_rua->media_info.pusher->setMpeg4Config(configData, configLen);

		        delete[] configData;   
		    }
		}
	}
	else if (p_rua->media_info.v_codec == VIDEO_CODEC_H264)
	{
	    char sdp[1024];
	    
	    if (rsua_get_sdp_h264_desc(p_rua, NULL, sdp, sizeof(sdp)))
	    {
	        p_rua->media_info.pusher->setH264Params(sdp);
	    }
	}
	else if (p_rua->media_info.v_codec == VIDEO_CODEC_H265)
	{
	    char sdp[1024];
	    
	    if (rsua_get_sdp_h265_desc(p_rua, NULL, sdp, sizeof(sdp)))
	    {
	        p_rua->media_info.pusher->setH265Params(sdp);
	    }
	}
	
	HRTSP_MSG * tx_msg = rua_build_response(p_rua, "200 OK");
	if (tx_msg == NULL) 
	{
		return FALSE;
	}
	
	rsua_send_free_rtsp_msg(p_rua,tx_msg);

	p_rua->state = RSS_ANNOUNCE;
	
	return TRUE;
}

BOOL rtsp_record_req(RSUA * p_rua, HRTSP_MSG * rx_msg)
{
	if (p_rua->state != RSS_RECORDING)
	{
		if (!p_rua->rtp_tcp)
		{
			CRtspPusher * p_pusher = p_rua->media_info.pusher;
			
			p_pusher->startUdpRx(p_rua->channels[AV_VIDEO_CH].udp_fd, p_rua->channels[AV_AUDIO_CH].udp_fd);
		}
	}

	p_rua->rtp_tx = 1;
	
	HRTSP_MSG * tx_msg = rua_build_response(p_rua, "200 OK");
	if (tx_msg == NULL) 
	{
		return FALSE;
	}
	
	rsua_send_free_rtsp_msg(p_rua,tx_msg);

	p_rua->state = RSS_RECORDING;
	
	return TRUE;
}

void rtsp_tcp_data_rx(RSUA * p_rua, uint8 * lpData, int rlen)
{
	RILF  * p_rilf = (RILF *)lpData;
	uint8 * p_rtp = (uint8 *)p_rilf + 4;
	uint32	rtp_len = rlen - 4;

	if (!p_rua->media_info.is_pusher || NULL == p_rua->media_info.pusher || !p_rua->rtp_tx)
	{
		return;
	}

	if (p_rilf->channel == p_rua->channels[AV_VIDEO_CH].interleaved)
	{
		p_rua->media_info.pusher->videoRtpRx(p_rtp, rtp_len);
	}
	else if (p_rilf->channel == p_rua->channels[AV_AUDIO_CH].interleaved)
	{
		p_rua->media_info.pusher->audioRtpRx(p_rtp, rtp_len);
	}
}

#endif // end of MEDIA_PUSHER

/***********************************************************************
 *
 * rtsp server state machine
 *
***********************************************************************/
int rtsp_server_state(RSUA * p_rua, HRTSP_MSG * rx_msg)
{
	BOOL ret = FALSE;
	char cseq_buf[32];

	if (rx_msg->msg_type != 0)
	{
		goto err_del_rua;
	}
	
	if (rtsp_get_msg_cseq(rx_msg, cseq_buf, sizeof(cseq_buf)) == FALSE)
	{
		goto err_del_rua;
	}
	
	p_rua->cseq = atoi(cseq_buf);

    if (g_rtsp_cfg.need_auth)
	{
	    BOOL auth = FALSE;
	    HD_AUTH_INFO auth_info;

	    // check rtsp digest auth information
	    if (rtsp_get_auth_digest_info(rx_msg, &auth_info))
	    {
	        auth = rtsp_auth_process(p_rua, rx_msg->first_line.header, &auth_info);
	    }

	    if (auth == FALSE)
	    {
    		rtsp_security_rly(p_rua, rx_msg);
    		return 0;
		}
	}
	
	if (rtsp_get_headline_uri(rx_msg, p_rua->uri, sizeof(p_rua->uri)) == FALSE)
	{
		goto err_del_rua;
	}
	
	switch (rx_msg->msg_sub_type)
	{
	case RTSP_MT_OPTIONS:
		if (rtsp_options_req(p_rua, rx_msg) == FALSE)
		{
			goto err_del_rua;
		}
		break;

	case RTSP_MT_DESCRIBE:
		if (rtsp_describe_req(p_rua, rx_msg) == FALSE)
		{
			goto err_del_rua;
		}
		break;

	case RTSP_MT_SETUP:
		if (rtsp_setup_req(p_rua, rx_msg) == FALSE)
		{
			goto err_del_rua;
		}
		break;

	case RTSP_MT_PLAY:
		if (rtsp_play_req(p_rua, rx_msg) == FALSE)
		{
			goto err_del_rua;
		}
		break;

    case RTSP_MT_PAUSE:
        rtsp_pause_req(p_rua, rx_msg);
        break;
        
	case RTSP_MT_TEARDOWN:
		rtsp_teardown_req(p_rua, rx_msg);
		goto err_del_rua;
		break;

	case RTSP_MT_GET_PARAMETER:
	    rtsp_get_parameter_req(p_rua, rx_msg);
		break;

	case RTSP_MT_SET_PARAMETER:
	    rtsp_set_parameter_req(p_rua, rx_msg);
		break;

#ifdef MEDIA_PUSHER		
	case RTSP_MT_ANNOUNCE:
		if (rtsp_announce_req(p_rua, rx_msg) == FALSE)
		{
			goto err_del_rua;
		}
		break;
		
	case RTSP_MT_RECORD:
		if (rtsp_record_req(p_rua, rx_msg) == FALSE)
		{
			goto err_del_rua;
		}
		break;
#endif

	case RTSP_MT_REDIRECT:
		break;
	}

	return 0;

err_del_rua:

	rtsp_close_rua(p_rua);
	return -1;
}

void rtsp_close_rua(RSUA * p_rua)
{
	log_print(HT_LOG_DBG, "%s, p_rua = %p\r\n", __FUNCTION__, p_rua);

	if (p_rua == NULL)
	{
		return;
	}
	
	if (rtsp_stop_stream_tx(p_rua))
	{
	    rua_set_idle_rua(p_rua);
	}
}





