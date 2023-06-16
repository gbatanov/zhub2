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
#include "video_render_gdi.h"


/***************************************************************************************/
#pragma comment(lib, "vfw32")

/***************************************************************************************/

CGDIVideoRender::CGDIVideoRender()
: CVideoRender()
, m_hDC(NULL)
, m_hCompatDC(NULL)
, m_hCompatBitmap(NULL)
, m_hDib(NULL)
, m_bReinit(FALSE)
{
    memset(&m_bmpHdr, 0, sizeof(m_bmpHdr));
    memset(&m_dstRect, 0, sizeof(RECT));
}

CGDIVideoRender::~CGDIVideoRender()
{
    unInit();
}

BOOL CGDIVideoRender::init(HWND hWnd, int w, int h, int videofmt)
{
    m_hDC = ::GetDC(hWnd);
    if (NULL == m_hDC)
    {
        return FALSE;
    }
    
    m_hCompatDC = ::CreateCompatibleDC(m_hDC);
    if (NULL == m_hCompatDC)
    {
        return FALSE;
    }
    
    ::SetStretchBltMode(m_hDC, HALFTONE);
    ::SetStretchBltMode(m_hCompatDC, HALFTONE);

    RECT rect;    
    ::GetClientRect(m_hWnd, &rect);
    
    m_hCompatBitmap = ::CreateCompatibleBitmap(m_hDC, rect.right - rect.left, rect.bottom - rect.top);

    ::SelectObject(m_hCompatDC, m_hCompatBitmap);

    memset(&m_bmpHdr, 0, sizeof(BITMAPINFOHEADER));
			
	m_bmpHdr.biSize = 40;
	m_bmpHdr.biWidth = w;
	m_bmpHdr.biHeight = h;
	m_bmpHdr.biBitCount = 24;
	m_bmpHdr.biPlanes = 1;
	m_bmpHdr.biSizeImage = w * h * m_bmpHdr.biBitCount / 8;

    m_hDib = ::DrawDibOpen();
    if (NULL == m_hDib)
    {
        log_print(HT_LOG_ERR, "%s, DrawDibOpen failed\r\n", __FUNCTION__);
        return FALSE;
    }
	
    int rectw = rect.right - rect.left;
    int recth = rect.bottom - rect.top;
    
	double dw = rectw / (double) w;
	double dh = recth / (double) h;
	double rate = (dw > dh)? dh : dw;

	int rw = (int)(w * rate);
	int rh = (int)(h * rate);

    m_dstRect.left = (rectw - rw) / 2;
	m_dstRect.top  = (recth - rh) / 2;
	m_dstRect.right = m_dstRect.left + rw;
	m_dstRect.bottom = m_dstRect.top + rh;

    CVideoRender::init(hWnd, w, h, videofmt);
    
    m_bInited = ::DrawDibBegin(m_hDib, m_hCompatDC, rw, rh, &m_bmpHdr, w, h, 0);
    
    return m_bInited;
}

void CGDIVideoRender::unInit()
{
	if (m_hCompatDC)
	{
	    ::DeleteDC(m_hCompatDC);
	    m_hCompatDC = NULL;
	}

	if (m_hDC)
	{
		::ReleaseDC(m_hWnd, m_hDC);
		m_hDC = NULL;
	}

	if (m_hCompatBitmap)
	{
	    ::DeleteObject(m_hCompatBitmap);
		m_hCompatBitmap = NULL;
	}

	if (m_hDib)
	{
		::DrawDibEnd(m_hDib);
		::DrawDibClose(m_hDib);		
		m_hDib = NULL;
	}	

	m_bInited = FALSE;
}

BOOL CGDIVideoRender::render(AVFrame * frame, int mode)
{
    if (m_bReinit)
    {
        unInit();

        if (init(m_hWnd, frame->width, frame->height, m_nVideoFmt))
        {
            m_bReinit = FALSE;
        }
    }
    
    if (!m_bInited || NULL == frame)
    {
        return FALSE;
    }

    BOOL ret = FALSE;
    RECT rect;
    GetClientRect(m_hWnd, &rect);

    RECT targetRect;
    RECT sourceRect;
		
	sourceRect.left = 0;
	sourceRect.top = 0;
	sourceRect.right = frame->width;
	sourceRect.bottom = frame->height;
		
	if (mode == RENDER_MODE_KEEP)
	{
	    int rectw = rect.right - rect.left;
	    int recth = rect.bottom - rect.top;
	    
		double dw = rectw / (double) frame->width;
		double dh = recth / (double) frame->height;
		double rate = (dw > dh)? dh : dw;

		int rw = (int)(frame->width * rate);
		int rh = (int)(frame->height * rate);

		targetRect.left = (rectw - rw) / 2;
		targetRect.top  = (recth - rh) / 2;
		targetRect.right = targetRect.left + rw;
		targetRect.bottom = targetRect.top + rh;
	}
	else
	{
		targetRect = rect;
	}

	if (memcmp(&m_dstRect, &targetRect, sizeof(RECT)) != 0)
	{
	    m_bReinit = TRUE;
	    memcpy(&m_dstRect, &targetRect, sizeof(RECT));
	}
	
	if (m_nVideoWidth != frame->width || m_nVideoHeight != frame->height)
	{
	    m_bReinit = TRUE;
	}

	if (m_bReinit)
	{
	    unInit();
	    
	    if (!init(m_hWnd, frame->width, frame->height, m_nVideoFmt))
	    {
	        log_print(HT_LOG_ERR, "%s, init failed\r\n", __FUNCTION__);
	        return FALSE;
	    }

        m_bReinit = FALSE;
	}
	
	::FillRect(m_hCompatDC, &rect, (HBRUSH) ::GetStockObject(BLACK_BRUSH));

	if (RENDER_MODE_KEEP == mode)
	{
		ret = ::DrawDibDraw(m_hDib, m_hCompatDC, targetRect.left, targetRect.top, 
		    targetRect.right - targetRect.left, targetRect.bottom - targetRect.top, 
		    &m_bmpHdr, frame->data[0],
			sourceRect.left, sourceRect.top, sourceRect.right - sourceRect.left, 
			sourceRect.bottom - sourceRect.top, DDF_BACKGROUNDPAL);
	}
	else
	{
		ret = ::DrawDibDraw(m_hDib, m_hCompatDC, rect.left, rect.top, 
		    rect.right - rect.left, rect.bottom - rect.top, 
		    &m_bmpHdr, frame->data[0],
			sourceRect.left, sourceRect.top, sourceRect.right - sourceRect.left, 
			sourceRect.bottom - sourceRect.top, DDF_BACKGROUNDPAL);
	}

    if (ret)
    {
	    ret = ::BitBlt(m_hDC, 0, 0, rect.right - rect.left, rect.bottom - rect.top , m_hCompatDC, 0, 0, SRCCOPY);
	}
	
    return ret;
}



