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
#include "onvif_srv.h"
#include "rtsp_srv.h"
#include "http_parse.h"
#include "onvif.h"
#include "rtsp_cfg.h"

/*****************************************************************/
extern "C" ONVIF_CLS    g_onvif_cls;
extern RTSP_CFG         g_rtsp_cfg;

/*****************************************************************/

void set_onvif_rtsp_port()
{
    g_onvif_cls.rtsp_port = g_rtsp_cfg.serverport;
}

#if __LINUX_OS__

void sig_handler(int sig)
{
    log_print(HT_LOG_DBG, "%s, sig=%d\r\n", __FUNCTION__, sig);

    rtsp_stop();
    onvif_stop();

    exit(0);
}

#elif __WINDOWS_OS__

BOOL WINAPI sig_handler(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:      // Ctrl+C
        log_print(HT_LOG_DBG, "%s, CTRL+C\r\n", __FUNCTION__);
        break;
        
    case CTRL_BREAK_EVENT: // Ctrl+Break
        log_print(HT_LOG_DBG, "%s, CTRL+Break\r\n", __FUNCTION__);
        break;
        
    case CTRL_CLOSE_EVENT: // Closing the consolewindow
        log_print(HT_LOG_DBG, "%s, Closing\r\n", __FUNCTION__);
        break;
    }

    rtsp_stop();
    onvif_stop();
    
    // Return TRUE if handled this message,further handler functions won't be called.
    // Return FALSE to pass this message to further handlers until default handler calls ExitProcess().
    
     return FALSE;
}

#endif

#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)

static void av_log_callback(void* ptr, int level, const char* fmt, va_list vl)   
{
    int htlv = HT_LOG_INFO;
	char buff[4096];

	if (AV_LOG_QUIET == level)
	{
	    return;
	}
	
	vsnprintf(buff, sizeof(buff), fmt, vl);

    if (AV_LOG_TRACE == level || AV_LOG_VERBOSE == level)
    {
        htlv = HT_LOG_TRC;
    }
	else if (AV_LOG_DEBUG == level)
	{
	    htlv = HT_LOG_DBG;
	}
	else if (AV_LOG_INFO == level)
    {
        htlv = HT_LOG_INFO;
    }
    else if (AV_LOG_WARNING == level)
    {
        htlv = HT_LOG_WARN;
    }
    else if (AV_LOG_ERROR == level)
    {
        htlv = HT_LOG_ERR;
    }
    else if (AV_LOG_FATAL == level || AV_LOG_PANIC == level)
    {
        htlv = HT_LOG_FATAL;
    }
	
	log_print(htlv, "%s", buff);
}

#endif

int main(int argc, char * argv[])
{
    network_init();

#if __LINUX_OS__

#if !defined(DEBUG) && !defined(_DEBUG)
    daemon_init();
#endif

    // Ignore broken pipes
    signal(SIGPIPE, SIG_IGN);
    
#endif

#if __LINUX_OS__
    signal(SIGINT, sig_handler);
    signal(SIGKILL, sig_handler);
    signal(SIGTERM, sig_handler);    
#elif __WINDOWS_OS__
    SetConsoleCtrlHandler(sig_handler, TRUE);
#endif

#if defined(MEDIA_FILE) || defined(MEDIA_DEVICE)
#if defined(DEBUG) || defined(_DEBUG)
    //av_log_set_callback(av_log_callback);
#endif
    av_log_set_level(AV_LOG_QUIET);
#endif

	sys_buf_init(MAX_NUM_RUA * 2 + 32);
	http_msg_buf_init(MAX_NUM_RUA * 2 + 16);
	
	onvif_start(); 

	rtsp_start("rtsp.cfg");

	set_onvif_rtsp_port();
	
	for (;;)
	{
#if defined(DEBUG) || defined(_DEBUG)		
	    if (getchar() == 'q')
	    {
	    	rtsp_stop();
	        onvif_stop();
	        break;
	    }
#endif

		sleep(5);
	}

	sys_buf_deinit();
	http_msg_buf_deinit();
	
	return 0;
}



