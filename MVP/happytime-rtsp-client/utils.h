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

#ifndef _UTILS_H_
#define _UTILS_H_

#include <QString>

QString getSnapshotPath();
QString getRecordPath();
BOOL 	getEnableLogFlag();
int 	getVideoRenderMode();
BOOL    getRtpMulticast();
BOOL    getRtpOverUdp();
BOOL    getRtspOverHttp();
int     getRtspOverHttpPort();
int 	getLogLevel();
int 	getSysLang();

void 	saveSnapshotPath(QString path);
void 	saveRecordPath(QString path);
void 	saveEnableLogFlag(BOOL flag);
void 	saveVideoRenderMode(int mode);
void    saveRtpMulticast(BOOL flag);
void    saveRtpOverUdp(BOOL flag);
void    saveRtspOverHttp(BOOL flag);
void    saveRtspOverHttpPort(int port);
void    saveLogLevel(int level);
void    saveSysLang(int lang);

QString getTempFile(QString prefix, QString extName);

#endif

