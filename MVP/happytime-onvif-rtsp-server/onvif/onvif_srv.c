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
#include "hxml.h"
#include "xml_node.h"
#include "onvif_probe.h"
#include "http.h"
#include "http_srv.h"
#include "http_parse.h"
#include "onvif_device.h"
#include "onvif.h"
#include "onvif_timer.h"
#include "onvif_srv.h"
#include "onvif_event.h"
#include "soap.h"
#ifdef PROFILE_G_SUPPORT
#include "onvif_recording.h"
#endif
#ifdef HTTPD
#include "httpd.h"
#endif
#ifdef RTSP_OVER_HTTP
#include "rtsp_http.h"
#endif


/***************************************************************************************/
static HTTPSRV http_srv[MAX_SERVERS];
extern ONVIF_CLS g_onvif_cls;
extern ONVIF_CFG g_onvif_cfg;

#define ONVIF_MAJOR_VERSION 8
#define ONVIF_MINOR_VERSION 2

/***************************************************************************************/

BOOL onvif_http_cb(void * p_srv, HTTPCLN * p_cln, HTTPMSG * p_msg, void * p_userdata)
{
    if (p_msg)
    {
#ifdef RTSP_OVER_HTTP
        char * p = http_get_headline(p_msg, "x-sessioncookie");
        if (p)
        {
            rtsp_http_process(p_cln, p_msg);
    
            if (p_msg)
            {
                http_free_msg(p_msg);
            }
        }
        else
#endif
        {
            OIMSG msg;
    		memset(&msg, 0, sizeof(OIMSG));
    		
    		msg.msg_src = ONVIF_MSG_SRC;
    		msg.msg_dua = (char *)p_cln;
    		msg.msg_buf = (char *)p_msg;
    		
    		if (hqBufPut(g_onvif_cls.msg_queue, (char *)&msg) == FALSE)
    		{
    			log_print(HT_LOG_ERR, "%s, send rx msg to main task failed!!!\r\n", __FUNCTION__);
    			return  FALSE;
    		}
    	}	
    }
    else if (p_cln)
    {
        OIMSG msg;
		memset(&msg, 0, sizeof(OIMSG));
		
		msg.msg_src = ONVIF_DEL_UA_SRC;
		msg.msg_dua = (char *)p_cln;
		
		if (hqBufPut(g_onvif_cls.msg_queue, (char *)&msg) == FALSE)
		{
			log_print(HT_LOG_ERR, "%s, send rx msg to main task failed!!!\r\n", __FUNCTION__);
			return  FALSE;
		}
    }
    
    return TRUE;
}

void onvif_http_rtsp_cb(void * p_srv, HTTPCLN * p_cln, char * buff, int buflen, void * p_userdata)
{
#ifdef RTSP_OVER_HTTP
    if (!rtsp_http_msg_process(p_cln, buff, buflen))
    {
        OIMSG msg;
		memset(&msg, 0, sizeof(OIMSG));
		
		msg.msg_src = ONVIF_DEL_UA_SRC;
		msg.msg_dua = (char *)p_cln;
		
		if (hqBufPut(g_onvif_cls.msg_queue, (char *)&msg) == FALSE)
		{
			log_print(HT_LOG_ERR, "%s, send rx msg to main task failed!!!\r\n", __FUNCTION__);
		}
    }
#endif
}

void onvif_http_msg_handler(HTTPCLN * p_cln, HTTPMSG * p_msg)
{
    char * post = p_msg->first_line.value_string;

    if (strstr(post, "FirmwareUpgrade"))	// must be the same with onvif_StartFirmwareUpgrade
	{
		soap_FirmwareUpgrade(p_cln, p_msg);
	}
	else if (strstr(post, "SystemRestore"))	// must be the same with onvif_StartSystemRestore
	{
		soap_SystemRestore(p_cln, p_msg);
	}
	else if (strstr(post, "snapshot"))	        
	{
		soap_GetSnapshot(p_cln, p_msg);
	}
	else if (strstr(post, "SystemLog"))
	{
	    soap_GetHttpSystemLog(p_cln, p_msg);
	}
	else if (strstr(post, "AccessLog"))
	{
	    soap_GetHttpAccessLog(p_cln, p_msg);
	}
	else if (strstr(post, "SupportInfo"))
	{
	    soap_GetSupportInfo(p_cln, p_msg);
	}
	else if (strstr(post, "SystemBackup"))
	{
	    soap_GetSystemBackup(p_cln, p_msg);
	}
	else if (p_msg->ctt_type == CTT_XML)
	{
	    soap_process_request(p_cln, p_msg);
	}
#ifdef HTTPD	
	else
	{
	    http_process_request(p_cln, p_msg);
	}
#endif
}

void * onvif_task(void * argv)
{
    OIMSG stm;

	while (1)
	{
		if (hqBufGet(g_onvif_cls.msg_queue, (char *)&stm))
		{
			HTTPCLN * p_cln = (HTTPCLN *)stm.msg_dua;
			
			switch (stm.msg_src)
			{
			case ONVIF_MSG_SRC:
				onvif_http_msg_handler(p_cln, (HTTPMSG *)stm.msg_buf);

				if (stm.msg_buf)
				{
				    http_free_msg((HTTPMSG *)stm.msg_buf);
				}
				
				if (p_cln) 
				{
#ifdef RTSP_OVER_HTTP				
				    rtsp_http_free_cln(p_cln);
#endif				    
				    http_free_used_cln((HTTPSRV *)p_cln->p_srv, p_cln);
				}
				break;

			case ONVIF_DEL_UA_SRC:
#ifdef RTSP_OVER_HTTP				
                rtsp_http_free_cln(p_cln);
#endif			
			    http_free_used_cln((HTTPSRV *)p_cln->p_srv, p_cln);
				break;

			case ONVIF_TIMER_SRC:
				onvif_timer();
				break;

			case ONVIF_EXIT:
			    goto EXIT;
			}
		}
	}

EXIT:

    g_onvif_cls.tid_main = 0;
    
	return NULL;
}

void onvif_start()
{
	int i;

	onvif_init();

    if (g_onvif_cfg.log_enable)
	{
	    log_init("ipsee.txt");
	    log_set_level(g_onvif_cfg.log_level);
	}
	
	printf("Happytime onvif server version %d.%d\r\n", ONVIF_MAJOR_VERSION, ONVIF_MINOR_VERSION);

    g_onvif_cls.msg_queue = hqCreate(100, sizeof(OIMSG), HQ_GET_WAIT);
	if (g_onvif_cls.msg_queue == NULL)
	{
		log_print(HT_LOG_ERR, "%s, create task queue failed!!!\r\n", __FUNCTION__);
		return;
	}
	
    g_onvif_cls.tid_main = sys_os_create_thread((void *)onvif_task, NULL);
    
    for (i = 0; i < g_onvif_cfg.servs_num; i++)
    {
        if (http_srv_init(&http_srv[i], g_onvif_cfg.servs[i].serv_ip, g_onvif_cfg.servs[i].serv_port, 
            g_onvif_cfg.http_max_users, g_onvif_cfg.https_enable, "ssl.ca", "ssl.key") < 0)
    	{
    		printf("http server listen on %s:%d failed\r\n", g_onvif_cfg.servs[i].serv_ip, g_onvif_cfg.servs[i].serv_port);
    	}
    	else
    	{
    	    http_srv_set_callback(&http_srv[i], onvif_http_cb, NULL);
    	    http_srv_set_rtsp_callback(&http_srv[i], onvif_http_rtsp_cb, NULL);
    	    
    	    printf("Onvif server running at %s:%d\r\n", g_onvif_cfg.servs[i].serv_ip, g_onvif_cfg.servs[i].serv_port);
    	}
    }

	onvif_timer_init();

	onvif_start_discovery();
}

void onvif_free_device()
{
    onvif_free_NetworkInterfaces(&g_onvif_cfg.network.interfaces);
    
    onvif_free_VideoSources(&g_onvif_cfg.v_src);

    onvif_free_VideoSourceConfigurations(&g_onvif_cfg.v_src_cfg);

    onvif_free_VideoEncoder2Configurations(&g_onvif_cfg.v_enc_cfg);

#ifdef AUDIO_SUPPORT
    onvif_free_AudioSources(&g_onvif_cfg.a_src);

    onvif_free_AudioSourceConfigurations(&g_onvif_cfg.a_src_cfg);

    onvif_free_AudioEncoder2Configurations(&g_onvif_cfg.a_enc_cfg);

    onvif_free_AudioDecoderConfigurations(&g_onvif_cfg.a_dec_cfg);
#endif 

    onvif_free_profiles(&g_onvif_cfg.profiles);

    onvif_free_OSDConfigurations(&g_onvif_cfg.OSDs);

    onvif_free_MetadataConfigurations(&g_onvif_cfg.metadata_cfg);

#ifdef MEDIA2_SUPPORT
    onvif_free_Masks(&g_onvif_cfg.mask);
#endif 

#ifdef PTZ_SUPPORT
    onvif_free_PTZNodes(&g_onvif_cfg.ptz_node);

    onvif_free_PTZConfigurations(&g_onvif_cfg.ptz_cfg);
#endif

#ifdef VIDEO_ANALYTICS
    onvif_free_VideoAnalyticsConfigurations(&g_onvif_cfg.va_cfg);
#endif

#ifdef PROFILE_G_SUPPORT
    onvif_free_Recordings(&g_onvif_cfg.recordings);

    onvif_free_RecordingJobs(&g_onvif_cfg.recording_jobs);
#endif

#ifdef PROFILE_C_SUPPORT
    onvif_free_AccessPoints(&g_onvif_cfg.access_points);
    onvif_free_Doors(&g_onvif_cfg.doors);
    onvif_free_AreaInfos(&g_onvif_cfg.area_info);
#endif

#ifdef DEVICEIO_SUPPORT
    onvif_free_VideoOutputs(&g_onvif_cfg.v_output);
    onvif_free_VideoOutputConfigurations(&g_onvif_cfg.v_output_cfg);
    onvif_free_AudioOutputs(&g_onvif_cfg.a_output);
    onvif_free_AudioOutputConfigurations(&g_onvif_cfg.a_output_cfg);
    onvif_free_RelayOutputs(&g_onvif_cfg.relay_output);
    onvif_free_DigitalInputs(&g_onvif_cfg.digit_input);
    onvif_free_SerialPorts(&g_onvif_cfg.serial_port);
#endif

    onvif_free_VideoEncoder2ConfigurationOptions(&g_onvif_cfg.v_enc_cfg_opt);

#ifdef AUDIO_SUPPORT
    onvif_free_AudioEncoder2ConfigurationOptions(&g_onvif_cfg.a_enc_cfg_opt);
#endif

#ifdef CREDENTIAL_SUPPORT
    onvif_free_Credentials(&g_onvif_cfg.credential);
#endif

#ifdef ACCESS_RULES
    onvif_free_AccessProfiles(&g_onvif_cfg.access_rules);
#endif

#ifdef SCHEDULE_SUPPORT
    onvif_free_Schedules(&g_onvif_cfg.schedule);
    onvif_free_SpecialDayGroups(&g_onvif_cfg.specialdaygroup);
#endif

#ifdef RECEIVER_SUPPORT
    onvif_free_Receivers(&g_onvif_cfg.receiver);
#endif
}

void onvif_stop()
{
    int i;

    OIMSG stm;
    memset(&stm, 0, sizeof(stm));

    stm.msg_src = ONVIF_EXIT;

    hqBufPut(g_onvif_cls.msg_queue, (char *)&stm);

    while (g_onvif_cls.tid_main)
    {
        usleep(10*1000);
    }

    onvif_bye();
	onvif_stop_discovery();

	onvif_timer_deinit();

    for (i = 0; i < g_onvif_cfg.servs_num; i++)
    {
	    http_srv_deinit(&http_srv[i]);
	}

    hqDelete(g_onvif_cls.msg_queue);
    
    onvif_eua_deinit();

#ifdef PROFILE_G_SUPPORT
    onvif_FreeSearchs();
#endif

    onvif_free_device();
	
    log_close();
}


