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

#ifndef VIDEO_RENDER_GDI_H
#define VIDEO_RENDER_GDI_H

#include "video_render.h"


/***************************************************************************************/


/***************************************************************************************/

class CGDIVideoRender : public CVideoRender
{
public:
    CGDIVideoRender();
    ~CGDIVideoRender();

    BOOL init(HWND hWnd, int w, int h, int videofmt);
    void unInit();

    BOOL render(AVFrame * frame, int mode);

private:
    
private:
    HDC                 m_hDC;
	HDC                 m_hCompatDC;
	HBITMAP             m_hCompatBitmap;
	HDRAWDIB            m_hDib;
	BITMAPINFOHEADER    m_bmpHdr;

	BOOL                m_bReinit;
	RECT                m_dstRect;
};


#endif // end of VIDEO_RENDER_GDI_H




