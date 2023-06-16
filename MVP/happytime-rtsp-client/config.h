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

#ifndef CONFIG_H
#define CONFIG_H

#include "sys_inc.h"
#include <QString>

#define MAIN_VERSION                2
#define MAJOR_VERSION               4

#define LANG_SYS                    0       // system language
#define LANG_ENG                    1       // english
#define LANG_ZH                     2       // chinese

typedef struct
{
	QString     snapshotPath;               // snapshot path
	QString     recordPath;                 // video recording path
	BOOL	    enableLog;                  // enable log
    BOOL        rtpMulticast;               // whether enable rtp multicast via rtsp
    BOOL        rtpOverUdp;                 // prefer to use RTP OVER UDP
    BOOL        rtspOverHttp;               // rtsp over http
    
	int		    videoRenderMode;		    // 0 - Keep the original aspect ratio, 1 - Filling the whole window
	int         logLevel;                   // log level, reference sys_log.h
	int         rtspOverHttpPort;           // rtsp over http port
	int         sysLang;                    // system language
} SysConfig;

#endif



