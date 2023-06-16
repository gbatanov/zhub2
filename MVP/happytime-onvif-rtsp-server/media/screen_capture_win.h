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

#ifndef _SCREEN_CAPTURE_WIN_H
#define _SCREEN_CAPTURE_WIN_H

#include "screen_capture.h"


class CWScreenCapture : public CScreenCapture
{
public:
    // get monitor numbers
    static int getDeviceNums();
    
    static CScreenCapture * getInstance(int devid);
	
	BOOL    initCapture(int codec, int width, int height, int framerate, int bitrate);
	BOOL    startCapture();
    
    void    captureThread();
    
private:
    CWScreenCapture();
    CWScreenCapture(CWScreenCapture &obj);
    ~CWScreenCapture();

    void    captureCursor();
	BOOL    capture();
	void    stopCapture();
	
private:	
	HDC         m_dcDesktop;
    HDC         m_dcMemory;
    HBITMAP     m_hBitmap;    
    HANDLE      m_hDib;
};

#endif



