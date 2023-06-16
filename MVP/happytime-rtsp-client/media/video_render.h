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

#ifndef VIDEO_RENDER_H
#define VIDEO_RENDER_H

#include "media_format.h"

extern "C"
{
#include "libavcodec\avcodec.h"
};

/***************************************************************************************/

#define RENDER_MODE_KEEP	0
#define RENDER_MODE_FILL	1

/***************************************************************************************/

class CVideoRender 
{
public:
    CVideoRender();
    virtual ~CVideoRender();

    virtual BOOL init(HWND hWnd, int w, int h, int videofmt);
    virtual void unInit();

    virtual BOOL render(AVFrame * frame, int mode);
   
protected:
	BOOL    m_bInited;
	HWND    m_hWnd;
	int     m_nVideoWidth;
	int     m_nVideoHeight;
	int     m_nVideoFmt;
};


#endif // end of VIDEO_RENDER_H




