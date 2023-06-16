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
#include "http_srv.h"
#include "http_parse.h"

#ifdef HTTPD
#include "httpd.h"
#endif

/***************************************************************************************/

#ifdef HTTPS

#if __WINDOWS_OS__
#pragma comment(lib, "libcrypto.lib")
#pragma comment(lib, "libssl.lib")
#endif


/* initialize SSL server and create context */
SSL_CTX * http_init_ssl_ctx()
{
	const SSL_METHOD * method;
	SSL_CTX * ctx;

	SSLeay_add_ssl_algorithms();		/* load & register all cryptos, etc. */
	SSL_load_error_strings();			/* load all error messages */
	method = SSLv23_server_method();	/* create new server-method instance */
	ctx = SSL_CTX_new(method);			/* create new context from method */
	if (ctx == NULL)
	{
		log_print(HT_LOG_ERR, "%s, SSL_CTX_new failed\r\n", __FUNCTION__);
	}
	
	return ctx;
}

/* LoadCertificates - load from files */
void http_load_certificates(SSL_CTX * ctx, const char * cert_file, const char * key_file)
{
	/* set the local certificate from CertFile */
	if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0)
	{
		log_print(HT_LOG_ERR, "%s, SSL_CTX_use_certificate_file failed\r\n", __FUNCTION__);
		return;
	}
	
	/* set the private key from KeyFile (may be the same as CertFile) */
	if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0)
	{
		log_print(HT_LOG_ERR, "%s, SSL_CTX_use_PrivateKey_file failed\r\n", __FUNCTION__);
		return;
	}
	
	/* verify private key */
	if (!SSL_CTX_check_private_key(ctx))
	{
		log_print(HT_LOG_ERR, "%s, Private key does not match the public certificate\r\n", __FUNCTION__);
		return;
	}
}

#endif // end of HTTPS

/***************************************************************************************/

BOOL http_commit_rx_msg(HTTPCLN * p_user, HTTPMSG * rx_msg)
{
    BOOL ret = FALSE;
    HTTPSRV * p_srv = (HTTPSRV *)p_user->p_srv;
    
    sys_os_mutex_enter(p_srv->mutex_cb);
    
    if (p_srv->callback)
    {
        ret = p_srv->callback((void *)p_srv, p_user, rx_msg, p_srv->userdata);
    }
    
    sys_os_mutex_leave(p_srv->mutex_cb);

    return ret;
}

/*
 * The message previously received by the client may not have been processed yet and 
 *  needs to be sent to the main task queue for deletion
 */
void http_commit_free_cln(HTTPSRV * p_srv, HTTPCLN * p_cln)
{
    if (p_cln->cfd > 0)
	{
#ifdef EPOLL	
	    epoll_ctl(p_srv->ep_fd, EPOLL_CTL_DEL, p_cln->cfd, NULL);
#endif

		closesocket(p_cln->cfd);
		p_cln->cfd = 0;
	}
	
    sys_os_mutex_enter(p_srv->mutex_cb);
    
    if (p_srv->callback)
    {
        p_srv->callback(p_srv, p_cln, NULL, p_srv->userdata);
    }
    
    sys_os_mutex_leave(p_srv->mutex_cb);
}

void http_commit_rtsp_msg(HTTPCLN * p_user, char * buff, int buflen)
{
#ifdef RTSP_OVER_HTTP
    HTTPSRV * p_srv = (HTTPSRV *)p_user->p_srv;
    if (NULL == p_srv)
    {
        return;
    }
    
    sys_os_mutex_enter(p_srv->mutex_cb);
    
    if (p_srv->rtsp_cb)
    {
        p_srv->rtsp_cb(p_user->p_srv, p_user, buff, buflen, p_srv->rtsp_userdata);
    }
    
    sys_os_mutex_leave(p_srv->mutex_cb);   
#endif    
}

BOOL http_srv_rx(HTTPCLN * p_user)
{
#ifdef HTTPS
    int pending;
#endif
    int rlen;
    HTTPMSG * rx_msg = NULL;
    
	if (p_user->p_rbuf == NULL)
	{
		p_user->p_rbuf = p_user->rcv_buf;
		p_user->mlen = sizeof(p_user->rcv_buf)-1;
		p_user->rcv_dlen = 0;
		p_user->ctt_len = 0;
		p_user->hdr_len = 0;
	}

#ifndef HTTPS
    rlen = recv(p_user->cfd, p_user->p_rbuf+p_user->rcv_dlen, p_user->mlen-p_user->rcv_dlen, 0);
#else
    
READ:

    if (p_user->ssl)
    {
        rlen = SSL_read(p_user->ssl, p_user->p_rbuf+p_user->rcv_dlen, p_user->mlen-p_user->rcv_dlen);
    }
    else
    {
        rlen = recv(p_user->cfd, p_user->p_rbuf+p_user->rcv_dlen, p_user->mlen-p_user->rcv_dlen, 0);
    }
#endif
	if (rlen <= 0)
	{
		log_print(HT_LOG_WARN, "%s, recv return = %d, dlen[%d], mlen[%d]\r\n", 
		    __FUNCTION__, rlen, p_user->rcv_dlen, p_user->mlen);
		return FALSE;
	}

	p_user->rcv_dlen += rlen;
	p_user->p_rbuf[p_user->rcv_dlen] = '\0';

#ifdef RTSP_OVER_HTTP
    if (p_user->rtsp_over_http)
    {
        http_commit_rtsp_msg(p_user, p_user->p_rbuf, p_user->rcv_dlen);
        
        p_user->hdr_len = 0;
		p_user->ctt_len = 0;
		p_user->p_rbuf = 0;
		p_user->rcv_dlen = 0;

        return TRUE;
    }
#endif    
        
rx_analyse_point:

	if (p_user->rcv_dlen < 16)
	{
		return TRUE;
    }
    
	if (http_is_http_msg(p_user->p_rbuf) == FALSE)
	{
		return FALSE;
    }
    
	rx_msg = NULL;

	if (p_user->hdr_len == 0)
	{
	    int parse_len;
		int http_pkt_len;
		
		http_pkt_len = http_pkt_find_end(p_user->p_rbuf);
		if (http_pkt_len == 0)
		{
			return TRUE;
		}
		p_user->hdr_len = http_pkt_len;

		rx_msg = http_get_msg_buf();
		if (rx_msg == NULL)
		{
			log_print(HT_LOG_ERR, "%s, get_msg_buf ret null!!!\r\n", __FUNCTION__);
			return FALSE;
		}

		memcpy(rx_msg->msg_buf, p_user->p_rbuf, http_pkt_len);
		rx_msg->msg_buf[http_pkt_len] = '\0';
		
		log_print(HT_LOG_DBG, "RX << %s\r\n", rx_msg->msg_buf);

		parse_len = http_msg_parse_part1(rx_msg->msg_buf, http_pkt_len, rx_msg);
		if (parse_len != http_pkt_len)
		{
			log_print(HT_LOG_ERR, "%s, http_msg_parse_part1=%d, http_pkt_len=%d!!!\r\n", 
			    __FUNCTION__, parse_len, http_pkt_len);
			http_free_msg(rx_msg);
			return FALSE;
		}
		
		p_user->ctt_len = rx_msg->ctt_len;
		p_user->ctt_type = rx_msg->ctt_type;

#ifdef RTSP_OVER_HTTP
		if (CTT_RTSP_TUNNELLED == p_user->ctt_type)
        {
            http_commit_rx_msg(p_user, rx_msg);

            if (p_user->rcv_dlen - http_pkt_len > 0)
            {
                http_commit_rtsp_msg(p_user, p_user->p_rbuf+p_user->hdr_len, p_user->rcv_dlen-http_pkt_len);
            }
            
            p_user->hdr_len = 0;
			p_user->ctt_len = 0;
			p_user->p_rbuf = 0;
			p_user->rcv_dlen = 0;

			p_user->rtsp_over_http = TRUE;
			
            return TRUE;
        }
#endif        
	}

	if ((p_user->ctt_len + p_user->hdr_len) > p_user->mlen)
	{
		if (p_user->dyn_recv_buf)
		{
			log_print(HT_LOG_INFO, "%s, dyn_recv_buf=%p, mlen=%d!!!\r\n", 
			    __FUNCTION__, p_user->dyn_recv_buf, p_user->mlen);
			free(p_user->dyn_recv_buf);
		}

		p_user->dyn_recv_buf = (char *)malloc(p_user->ctt_len + p_user->hdr_len + 1);
		if (NULL == p_user->dyn_recv_buf)
		{
		    if (rx_msg)
    		{
    		    http_free_msg(rx_msg);
            }
        
		    log_print(HT_LOG_INFO, "%s, malloc failed\r\n", __FUNCTION__);
		    return FALSE;
		}

		memcpy(p_user->dyn_recv_buf, p_user->rcv_buf, p_user->rcv_dlen);

		p_user->p_rbuf = p_user->dyn_recv_buf;
		p_user->mlen = p_user->ctt_len + p_user->hdr_len;

		if (rx_msg)
		{
		    http_free_msg(rx_msg);
        }
        
#ifdef HTTPS 
        if (p_user->ssl)
        {
    		pending = SSL_pending(p_user->ssl);
            if (pending > 0)
            {
            	goto READ;
            }
        }
#endif

		return TRUE;
	}

	if (p_user->rcv_dlen >= (p_user->ctt_len + p_user->hdr_len))
	{
		if (rx_msg == NULL)
		{
		    int parse_len;
			int nlen;

			nlen = p_user->ctt_len + p_user->hdr_len;
			if (nlen >= 2048)
			{
				rx_msg = http_get_msg_large_buf(nlen+1);
				if (rx_msg == NULL)
				{
				    log_print(HT_LOG_ERR, "%s, http_get_msg_large_buf failed\r\n", __FUNCTION__);
					return FALSE;
				}	
			}
			else
			{
				rx_msg = http_get_msg_buf();
				if (rx_msg == NULL)
				{
				    log_print(HT_LOG_ERR, "%s, http_get_msg_buf failed\r\n", __FUNCTION__);
					return FALSE;
				}	
			}

			memcpy(rx_msg->msg_buf, p_user->p_rbuf, p_user->hdr_len);
			rx_msg->msg_buf[p_user->hdr_len] = '\0';
			
			log_print(HT_LOG_DBG, "RX << %s\r\n\r\n", rx_msg->msg_buf);
			
			parse_len = http_msg_parse_part1(rx_msg->msg_buf, p_user->hdr_len, rx_msg);
			if (parse_len != p_user->hdr_len)
			{
				log_print(HT_LOG_ERR, "%s, http_msg_parse_part1=%d, sip_pkt_len=%d!!!\r\n", 
				    __FUNCTION__, parse_len, p_user->hdr_len);
				http_free_msg(rx_msg);
				return FALSE;
			}
		}

		if (p_user->ctt_len > 0)
		{
			int parse_len;
			
			memcpy(rx_msg->msg_buf+p_user->hdr_len, p_user->p_rbuf+p_user->hdr_len, p_user->ctt_len);
			rx_msg->msg_buf[p_user->hdr_len + p_user->ctt_len] = '\0';

			if (ctt_is_string(rx_msg->ctt_type))
			{
				log_print(HT_LOG_DBG, "%s\r\n\r\n", rx_msg->msg_buf+p_user->hdr_len);
            }
            
			parse_len = http_msg_parse_part2(rx_msg->msg_buf+p_user->hdr_len, p_user->ctt_len, rx_msg);
			if (parse_len != p_user->ctt_len)
			{
				log_print(HT_LOG_WARN, "%s, http_msg_parse_part2=%d, sdp_pkt_len=%d!!!\r\n", 
				    __FUNCTION__, parse_len, p_user->ctt_len);
			}
		}

		if (http_commit_rx_msg(p_user, rx_msg) == FALSE)
		{
		    http_free_msg(rx_msg);
		    return FALSE;
		}
		
		p_user->rcv_dlen -= p_user->hdr_len + p_user->ctt_len;

		if (p_user->dyn_recv_buf == NULL)
		{
			if (p_user->rcv_dlen > 0)
			{
				memmove(p_user->rcv_buf, p_user->rcv_buf+p_user->hdr_len + p_user->ctt_len, p_user->rcv_dlen);
				p_user->rcv_buf[p_user->rcv_dlen] = '\0';
			}
			
			p_user->p_rbuf = p_user->rcv_buf;
			p_user->mlen = sizeof(p_user->rcv_buf)-1;
			p_user->hdr_len = 0;
			p_user->ctt_len = 0;

			if (p_user->rcv_dlen > 16)
			{
				goto rx_analyse_point;
			}	
		}
		else
		{
			free(p_user->dyn_recv_buf);
			p_user->dyn_recv_buf = NULL;
			p_user->hdr_len = 0;
			p_user->ctt_len = 0;
			p_user->p_rbuf = 0;
			p_user->rcv_dlen = 0;
		}
	}
	else
	{
	    if (rx_msg)
	    {
		    http_free_msg(rx_msg);
		} 
	}
    
    return TRUE;
}

int http_listen_rx(HTTPSRV * p_srv)
{
#ifdef HTTPS
	int ret;
	SSL * ssl = NULL;
#endif
    SOCKET cfd;
	struct sockaddr_in caddr;
	socklen_t size;
	HTTPCLN * p_cln;
	struct timeval tv;
	int len = 1024 * 1024;

    size = sizeof(struct sockaddr_in);
	cfd = accept(p_srv->sfd, (struct sockaddr *)&caddr, &size);
	if (cfd <= 0)
	{
		log_print(HT_LOG_ERR, "%s, accept, cfd(%d), %s\r\n", __FUNCTION__, cfd, sys_os_get_socket_error());
		return -1;
	}

#ifdef HTTPS
    if (p_srv->https)
    {
        ssl = SSL_new(p_srv->ssl_ctx);
        if (NULL == ssl)
        {
            closesocket(cfd);
            log_print(HT_LOG_ERR, "%s, SSL_new failed\r\n", __FUNCTION__);
            return -1;
        }
        
        SSL_set_fd(ssl, (int)cfd);
        
        ret = SSL_accept(ssl);        
        if (-1 == ret)
        {
            SSL_free(ssl);
            closesocket(cfd);
            log_print(HT_LOG_ERR, "%s, SSL_accept failed\r\n", __FUNCTION__);
            return -1;
        }
    }
#endif

	p_cln = http_get_idle_cln(p_srv);
	if (p_cln == NULL)
	{
		log_print(HT_LOG_ERR, "%s, http_get_idle_cln::ret null!!!\r\n", __FUNCTION__);
		closesocket(cfd);
		return -1;
	}

	tv.tv_sec = 5;
	tv.tv_usec = 0;
		
	setsockopt(cfd, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(tv));
	
	if (setsockopt(cfd, SOL_SOCKET, SO_SNDBUF, (char*)&len, sizeof(int)))
	{
		log_print(HT_LOG_WARN, "%s, setsockopt SO_SNDBUF error!!!\r\n", __FUNCTION__);
	}
	
	if (setsockopt(cfd, SOL_SOCKET, SO_RCVBUF, (char*)&len, sizeof(int)))
	{
		log_print(HT_LOG_WARN, "%s, setsockopt SO_SNDBUF error!!!\r\n", __FUNCTION__);
	}
	
	p_cln->cfd = cfd;
	p_cln->rip = caddr.sin_addr.s_addr;
	p_cln->rport = ntohs(caddr.sin_port);

    p_cln->lport = p_srv->sport;
	strcpy(p_cln->lip, p_srv->host);

    p_cln->p_srv = p_srv;
    
#ifdef HTTPS
    if (p_srv->https)
    {
	    p_cln->ssl = ssl;
	}
#endif

	pps_ctx_ul_add(p_srv->cln_ul, p_cln);

	log_print(HT_LOG_INFO, "http user over tcp from[0x%08x,%u]\r\n", p_cln->rip, p_cln->rport);

#ifdef EPOLL
    uint64_t e_dat = http_cln_index(p_srv, p_cln);
    e_dat = e_dat << 32;
    e_dat = e_dat | cfd;

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.u64 = e_dat;
    epoll_ctl(p_srv->ep_fd, EPOLL_CTL_ADD, cfd, &event);
#endif

	return 0;
}

void * http_rx_thread(void * argv)
{
    uint32 i;
    
#ifdef EPOLL
    int fd, nfds;
#else
    fd_set fdr;
#endif    
	
	HTTPSRV * p_srv = (HTTPSRV *)argv;
	if (p_srv == NULL)
	{
		return NULL;
    }
    
	while (p_srv->r_flag == 1)
	{
#ifdef EPOLL

        nfds = epoll_wait(p_srv->ep_fd, p_srv->ep_events, p_srv->ep_event_num, 500);

		for (i=0; i<nfds; i++)
		{
			if (p_srv->ep_events[i].events & EPOLLIN)
			{
				fd = (int)(p_srv->ep_events[i].data.u64);
				if ((p_srv->ep_events[i].data.u64 & ((uint64_t)1 << 63)) != 0)
				{
				    http_listen_rx(p_srv);
				}
				else
				{
					uint32 u_index = p_srv->ep_events[i].data.u64 >> 32;
					HTTPCLN * p_cln = http_get_cln_by_index(p_srv, u_index);

					if (p_cln->cfd > 0 && p_cln->cfd == fd)
					{
						if (http_srv_rx(p_cln) == FALSE)
						{            			    
						    http_commit_free_cln(p_srv, p_cln);
						}
					}
					else
					{
						log_print(HT_LOG_WARN, "%s, event fd[%d] not match user fd[%d]!!!\r\n", __FUNCTION__, fd, p_cln->cfd);
					}
				}
			}
		}
		
#else

	    int sret;
	    int max_fd;
	    HTTPCLN * p_cln;
	    struct timeval tv;
	    
		FD_ZERO(&fdr);		
		FD_SET(p_srv->sfd, &fdr);
		max_fd = (int)p_srv->sfd;

		for (i = 0; i < p_srv->max_cln_nums; i++)
		{
		    p_cln = http_get_cln_by_index(p_srv, i);
    		if (p_cln->cfd > 0)
    		{
    			FD_SET(p_cln->cfd, &fdr);
				max_fd = (int)(((int)p_cln->cfd > max_fd)? p_cln->cfd : max_fd);
    		}
		}
		
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		
		sret = select(max_fd+1, &fdr, NULL, NULL, &tv);
		if (sret == 0)
		{
			continue;
		}
		else if (sret < 0)
		{
			log_print(HT_LOG_ERR, "%s, select err[%s], max fd[%d], sret[%d]!!!\r\n", 
			    __FUNCTION__, sys_os_get_socket_error(), max_fd, sret);
			continue;
		}

		if (FD_ISSET(p_srv->sfd, &fdr))
		{
			http_listen_rx(p_srv);
		}

		for (i = 0; i < p_srv->max_cln_nums; i++)
		{
		    p_cln = http_get_cln_by_index(p_srv, i);

			if (p_cln->cfd > 0 && FD_ISSET(p_cln->cfd, &fdr))
    		{
    		    if (http_srv_rx(p_cln) == FALSE)
				{
                    http_commit_free_cln(p_srv, p_cln);
				}
    		}
		}

#endif

        usleep(10*1000);
	}

	p_srv->rx_tid = 0;
	
	log_print(HT_LOG_DBG, "%s, exit\r\n", __FUNCTION__);

	return NULL;
}

int http_srv_net_init(HTTPSRV * p_srv)
{
    int val = 1;
    int reuse_ret;
    struct sockaddr_in addr;
    
	p_srv->sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (p_srv->sfd <= 0)
	{
		log_print(HT_LOG_ERR, "%s, socket err[%s]!!!\r\n", __FUNCTION__, sys_os_get_socket_error());
		return -1;
	}
	
	reuse_ret = setsockopt(p_srv->sfd, SOL_SOCKET, SO_REUSEADDR, (char *)&val, 4);

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = p_srv->saddr;
	addr.sin_port = htons(p_srv->sport);
	
	if (bind(p_srv->sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
	{
		log_print(HT_LOG_ERR, "%s, bind tcp socket fail,err[%s]!!!\n", __FUNCTION__, sys_os_get_socket_error());
		closesocket(p_srv->sfd);
		p_srv->sfd = 0;
		return -1;
	}

	if (listen(p_srv->sfd, 10) < 0)
	{
		log_print(HT_LOG_ERR, "%s, listen tcp socket fail,err[%s]!!!\r\n", __FUNCTION__, sys_os_get_socket_error());
		closesocket(p_srv->sfd);
		return -1;
	}
	
#ifdef EPOLL
    uint64_t e_dat = p_srv->sfd;
	e_dat |= ((uint64_t)1 << 63);

	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.u64 = e_dat;
	epoll_ctl(p_srv->ep_fd, EPOLL_CTL_ADD, p_srv->sfd, &event);
#endif
	
	return 0;
}

HT_API int http_srv_init(HTTPSRV * p_srv, const char * saddr, uint16 sport, int cln_num, BOOL https, const char * cert_file, const char * key_file)
{
	memset(p_srv, 0, sizeof(HTTPSRV));

    if (saddr && saddr[0] != '\0')
    {
        strncpy(p_srv->host, saddr, sizeof(p_srv->host)-1);
	    p_srv->saddr = get_address_by_name(saddr);
	}
	else
	{
	    strcpy(p_srv->host, get_local_ip());
	    p_srv->saddr = 0;   // listen on all interfaces
	}
	
	p_srv->sport = sport;
	p_srv->https = https;
	p_srv->max_cln_nums = cln_num;

    p_srv->cln_fl = pps_ctx_fl_init(cln_num, sizeof(HTTPCLN), TRUE);
	if (p_srv->cln_fl == NULL)
	{
		return -1;
    }
    
	p_srv->cln_ul = pps_ctx_ul_init(p_srv->cln_fl, TRUE);
	if (p_srv->cln_ul == NULL)
	{
		goto FAILED;
    }
    
#ifdef EPOLL
    p_srv->ep_event_num = cln_num + 16;
    
    p_srv->ep_fd = epoll_create(p_srv->ep_event_num);
	if (p_srv->ep_fd < 0)
	{
		log_print(HT_LOG_ERR, "%s, epoll_create failed\r\n", __FUNCTION__);
		goto FAILED;
	}

	p_srv->ep_events = (struct epoll_event *)malloc(sizeof(struct epoll_event) * p_srv->ep_event_num);
	if (p_srv->ep_events == NULL)
	{
		log_print(HT_LOG_ERR, "%s, malloc failed\r\n", __FUNCTION__);
		goto FAILED;
	}
#endif
    
#ifdef HTTPS
    if (p_srv->https)
    {
        if (cert_file)
        {
            strncpy(p_srv->cert_file, cert_file, sizeof(p_srv->cert_file)-1);
        }

        if (key_file)
        {
	        strncpy(p_srv->key_file, key_file, sizeof(p_srv->key_file)-1);
	    }
	
        p_srv->ssl_ctx = http_init_ssl_ctx();   /* initialize SSL */
        if (NULL == p_srv->ssl_ctx)
        {
            log_print(HT_LOG_ERR, "%s, http_init_ssl_ctx failed\r\n", __FUNCTION__);
            goto FAILED;
        }
        
        http_load_certificates(p_srv->ssl_ctx, p_srv->cert_file, p_srv->key_file);  /* load certs */
    }
#endif

	if (http_srv_net_init(p_srv) != 0)
	{
		goto FAILED;
    }

    p_srv->mutex_cb = sys_os_create_mutex();
    
	p_srv->r_flag = 1;
	p_srv->rx_tid = sys_os_create_thread((void *)http_rx_thread, p_srv);

	return 0;

FAILED:

    http_srv_deinit(p_srv);
    
	return -1;
}

HT_API void http_srv_deinit(HTTPSRV * p_srv)
{
    p_srv->r_flag = 0;

	while (p_srv->rx_tid != 0)
	{
		usleep(10*1000);
	}

	if (p_srv->cln_ul)
	{
		pps_ul_free(p_srv->cln_ul);
		p_srv->cln_ul = NULL;
	}

	if (p_srv->cln_fl)
	{
		pps_fl_free(p_srv->cln_fl);
		p_srv->cln_fl = NULL;
	}

    if (p_srv->mutex_cb)
    {
        sys_os_destroy_sig_mutex(p_srv->mutex_cb);
        p_srv->mutex_cb = NULL;
    }
    
#ifdef HTTPS
	if (p_srv->ssl_ctx)
	{
		SSL_CTX_free(p_srv->ssl_ctx);
		p_srv->ssl_ctx = NULL;
	}
#endif

	if (p_srv->sfd > 0)
	{
#ifdef EPOLL
        epoll_ctl(p_srv->ep_fd, EPOLL_CTL_DEL, p_srv->sfd, NULL);
#endif
		closesocket(p_srv->sfd);
		p_srv->sfd = 0;
	}	

#ifdef EPOLL
    if (p_srv->ep_fd)
    {
        close(p_srv->ep_fd);
        p_srv->ep_fd = 0;        
    }

    if (p_srv->ep_events)
    {
        free(p_srv->ep_events);
        p_srv->ep_events = NULL;
    }
#endif
}

HT_API void http_srv_set_callback(HTTPSRV * p_srv, http_srv_callback cb, void * p_userdata)
{
    sys_os_mutex_enter(p_srv->mutex_cb);
    p_srv->callback = cb;
    p_srv->userdata = p_userdata;
    sys_os_mutex_leave(p_srv->mutex_cb);
}

HT_API void http_srv_set_rtsp_callback(HTTPSRV * p_srv, http_rtsp_callback cb, void * p_userdata)
{
#ifdef RTSP_OVER_HTTP
    sys_os_mutex_enter(p_srv->mutex_cb);
    p_srv->rtsp_cb = cb;
    p_srv->rtsp_userdata = p_userdata;
    sys_os_mutex_leave(p_srv->mutex_cb);
#endif    
}

/***************************************************************************************/

HT_API uint32 http_cln_index(HTTPSRV * p_srv, HTTPCLN * p_cln)
{
	return pps_get_index(p_srv->cln_fl, p_cln);
}

HT_API HTTPCLN * http_get_cln_by_index(HTTPSRV * p_srv, unsigned long index)
{
	return (HTTPCLN *)pps_get_node_by_index(p_srv->cln_fl, index);
}

HT_API HTTPCLN * http_get_idle_cln(HTTPSRV * p_srv)
{	
	HTTPCLN * p_cln = (HTTPCLN *)pps_fl_pop(p_srv->cln_fl);
	if (p_cln)
	{
		memset(p_cln, 0, sizeof(HTTPCLN));

		p_cln->use_count = 1;
	}

	return p_cln;
}

HT_API void http_free_used_cln(HTTPSRV * p_srv, HTTPCLN * p_cln)
{
    p_cln->use_count--;
    if (p_cln->use_count > 0)
    {
        return;
    }
    
    if (p_cln->dyn_recv_buf)
	{
		free(p_cln->dyn_recv_buf);
		p_cln->dyn_recv_buf = NULL;
	}

#ifdef HTTPS
	if (p_cln->ssl)
	{
		SSL_free(p_cln->ssl);
		p_cln->ssl = NULL;
	}
#endif

	if (p_cln->cfd > 0)
	{
#ifdef EPOLL	
	    epoll_ctl(p_srv->ep_fd, EPOLL_CTL_DEL, p_cln->cfd, NULL);
#endif

		closesocket(p_cln->cfd);
		p_cln->cfd = 0;
	}

	pps_ctx_ul_del(p_srv->cln_ul, p_cln);
	
	pps_fl_push_tail(p_srv->cln_fl, p_cln);
}




