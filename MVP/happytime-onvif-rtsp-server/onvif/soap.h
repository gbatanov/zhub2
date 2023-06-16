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

#ifndef SOAP_H
#define SOAP_H

#include "sys_inc.h"
#include "http.h"

typedef int (*soap_build_xml)(char * p_buf, int mlen, const char * argv);

#ifdef __cplusplus
extern "C" {
#endif

ONVIF_RET soap_build_send_rly(HTTPCLN * p_user, HTTPMSG * rx_msg, soap_build_xml build_xml, const char * argv, const char * action, XMLN * p_header);

void soap_process_request(HTTPCLN * p_user, HTTPMSG * rx_msg);
void soap_FirmwareUpgrade(HTTPCLN * p_user, HTTPMSG * rx_msg);
void soap_GetSnapshot(HTTPCLN * p_user, HTTPMSG * rx_msg);
void soap_GetHttpSystemLog(HTTPCLN * p_user, HTTPMSG * rx_msg);
void soap_GetHttpAccessLog(HTTPCLN * p_user, HTTPMSG * rx_msg);
void soap_GetSupportInfo(HTTPCLN * p_user, HTTPMSG * rx_msg);
void soap_GetSystemBackup(HTTPCLN * p_user, HTTPMSG * rx_msg);
void soap_SystemRestore(HTTPCLN * p_user, HTTPMSG * rx_msg);

#ifdef __cplusplus
}
#endif

#endif


