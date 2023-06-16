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
#include "screen_capture_win.h"
#include "media_format.h"
#include "lock.h"


void * screenCaptureThread(void * argv)
{
	CWScreenCapture *capture = (CWScreenCapture *)argv;

	capture->captureThread();

	return NULL;
}

CWScreenCapture::CWScreenCapture()  : CScreenCapture()
{
	m_dcDesktop = NULL;
	m_dcMemory = NULL;
	m_hBitmap = NULL;
	m_hDib = NULL;	
}

CWScreenCapture::~CWScreenCapture()
{
	stopCapture();
}

CScreenCapture * CWScreenCapture::getInstance(int devid)
{
	if (devid < 0 || devid >= MAX_MONITOR_NUMS)
	{
		return NULL;
	}
	
	if (NULL == m_pInstance[devid])
	{
		sys_os_mutex_enter(m_pInstMutex);

		if (NULL == m_pInstance[devid])
		{
			m_pInstance[devid] = (CScreenCapture *) new CWScreenCapture;
			if (m_pInstance[devid])
			{
				m_pInstance[devid]->m_nRefCnt++;
				m_pInstance[devid]->m_nDevIndex = devid;
			}
		}
		
		sys_os_mutex_leave(m_pInstMutex);
	}
	else
	{
		sys_os_mutex_enter(m_pInstMutex);
		m_pInstance[devid]->m_nRefCnt++;
		sys_os_mutex_leave(m_pInstMutex);
	}

	return m_pInstance[devid];
}

int CWScreenCapture::getDeviceNums()
{	
	int i = 0;
	int count = 0;

	DISPLAY_DEVICE dev;
	memset(&dev, 0, sizeof(dev));

	dev.cb = sizeof(dev);
	
	while (EnumDisplayDevices(NULL, i, &dev, 0))
	{
		if (dev.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
		{
			count++;
		}
		
		i++;
		
		memset(&dev, 0, sizeof(dev));
		
		dev.cb = sizeof(dev);
	}
	
	return count;
}

BOOL CWScreenCapture::initCapture(int codec, int width, int height, int framerate, int bitrate)
{
	CLock lock(m_pMutex);
	
	if (m_bInited)
	{
		return TRUE;
	}

	int i = 0;
	int count = 0;

	DISPLAY_DEVICE dev;
	memset(&dev, 0, sizeof(dev));

	dev.cb = sizeof(dev);
	
	while (EnumDisplayDevices(NULL, i, &dev, 0))
	{
		if (dev.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
		{
			if (count++ == m_nDevIndex)
			{
				break;
			}
		}
		
		i++;
		
		memset(&dev, 0, sizeof(dev));
		
		dev.cb = sizeof(dev);
	}

	DEVMODE mode;
	memset(&mode, 0, sizeof(mode));

	mode.dmSize = sizeof(mode);
	
	if (!EnumDisplaySettings(dev.DeviceName, ENUM_CURRENT_SETTINGS, &mode))
	{
		log_print(HT_LOG_ERR, "%s, EnumDisplaySettings failed\r\n", __FUNCTION__);
		return FALSE;
	}

	m_nWidth = mode.dmPelsWidth;
	m_nHeight = mode.dmPelsHeight;

	m_nFramerate = framerate;
	m_nBitrate = bitrate;
	
	m_dcDesktop = CreateDC(dev.DeviceID, dev.DeviceName, NULL, NULL);
	if (NULL == m_dcDesktop)
	{
		return FALSE;
	}
	
	m_dcMemory = CreateCompatibleDC(m_dcDesktop);
	if (NULL == m_dcMemory)
	{
		return FALSE;
	}

	m_hBitmap = CreateCompatibleBitmap(m_dcDesktop, m_nWidth, m_nHeight);
	if (NULL == m_hBitmap)
	{
		return FALSE;
	}

	DWORD dwBmpSize = ((m_nWidth * 32 + 31) / 32) * 4 * m_nHeight;
	
	m_hDib = GlobalAlloc(GHND, dwBmpSize); 
	if (NULL == m_hDib)
	{
		return FALSE;
	}

	VideoEncoderParam params;
	memset(&params, 0, sizeof(params));
	
	params.SrcWidth = m_nWidth;
	params.SrcHeight = m_nHeight;
	params.SrcPixFmt = CVideoEncoder::toAVPixelFormat(VIDEO_FMT_BGRA);
	params.Updown = 1;
	params.DstCodec = codec;
	params.DstWidth = width ? width : m_nWidth;;
	params.DstHeight = height ? height : m_nHeight;
	params.DstFramerate = framerate;
	params.DstBitrate = bitrate;
	
	if (m_encoder.init(&params) == FALSE)
	{
		return FALSE;
	}

	m_bInited = TRUE;
	
	return TRUE;
}

BOOL CWScreenCapture::startCapture()
{
	CLock lock(m_pMutex);
	
	if (m_hCapture)
	{
		return TRUE;
	}

	m_bCapture = TRUE;
	m_hCapture = sys_os_create_thread((void *)screenCaptureThread, this);

	return (NULL != m_hCapture);
}

void CWScreenCapture::stopCapture()
{
	CLock lock(m_pMutex);
	
	m_bCapture = FALSE;
	
	while (m_hCapture)
	{
		usleep(10*1000);
	}
	
	if (m_dcDesktop)
	{
		DeleteDC(m_dcDesktop);
		m_dcDesktop = NULL;
	}

	if (m_dcMemory)
	{
		DeleteDC(m_dcMemory);
		m_dcMemory = NULL;
	}

	if (m_hBitmap)
	{
		DeleteObject(m_hBitmap);
		m_hBitmap = NULL;
	}

	if (m_hDib)
	{
		GlobalFree(m_hDib);
		m_hDib = NULL;
	}

	m_bInited = FALSE;
}

void CWScreenCapture::captureCursor()
{
	CURSORINFO ci = {0};
	
    ci.cbSize = sizeof(ci);

    if (GetCursorInfo(&ci))
    {
        HCURSOR icon = CopyCursor(ci.hCursor);
        ICONINFO info;
        POINT pos;
        int vertres = GetDeviceCaps(m_dcDesktop, VERTRES);
        int desktopvertres = GetDeviceCaps(m_dcDesktop, DESKTOPVERTRES);
        info.hbmMask = NULL;
        info.hbmColor = NULL;

        if (ci.flags != CURSOR_SHOWING)
            return;

        if (!icon) 
        {
            /* Use the standard arrow cursor as a fallback.
             * You'll probably only hit this in Wine, which can't fetch
             * the current system cursor. */
            icon = CopyCursor(LoadCursor(NULL, IDC_ARROW));
        }

        if (!GetIconInfo(icon, &info)) 
        {
            log_print(HT_LOG_ERR, "%s, could not get icon info\r\n", __FUNCTION__);
            goto icon_error;
        }

        GetCursorPos(&pos);
        
        pos.x = pos.x - info.xHotspot;
        pos.y = pos.y - info.yHotspot;

        // that would keep the correct location of mouse with hidpi screens
        pos.x = pos.x * desktopvertres / vertres;
        pos.y = pos.y * desktopvertres / vertres;

        if (!DrawIconEx(m_dcMemory, pos.x, pos.y, icon, 0, 0, 0, NULL, DI_NORMAL|DI_COMPAT|DI_DEFAULTSIZE))
        {
            log_print(HT_LOG_ERR, "%s, couldn't draw icon\r\n", __FUNCTION__);
        }

icon_error:

        if (info.hbmMask)
        {
            DeleteObject(info.hbmMask);
        }
        
        if (info.hbmColor)
        {
            DeleteObject(info.hbmColor);
        }
        
        if (icon)
        {
            DestroyCursor(icon);
        }    
    } 
    else 
    {
        log_print(HT_LOG_ERR, "%s, couldn't get cursor info\r\n", __FUNCTION__);
    }
}

BOOL CWScreenCapture::capture()
{
	if (!m_bInited)
	{
		return FALSE;
	}
	
	// Request that the system not power-down the system, or the display hardware.
	if (!SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED)) 
	{
		log_print(HT_LOG_WARN, "Failed to make system & display power assertion\r\n");
	}

	HGDIOBJ old_bitmap = SelectObject(m_dcMemory, m_hBitmap);
	if (old_bitmap != NULL) 
	{		
    	BitBlt(m_dcMemory, 0, 0, m_nWidth, m_nHeight, m_dcDesktop, 0, 0, SRCCOPY | CAPTUREBLT); 

		captureCursor();
		
    	SelectObject(m_dcMemory, old_bitmap);
  	}
  	else
  	{
  		return FALSE;
  	}
  	
	BITMAP bmpScreen;	
	GetObject(m_hBitmap, sizeof(BITMAP), &bmpScreen);

	if (bmpScreen.bmBitsPixel != 32)
	{
		return FALSE;
	}

	int w = bmpScreen.bmWidth;
	int h = bmpScreen.bmHeight;
	
	BITMAPINFOHEADER bi;
	 
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = w;    
	bi.biHeight = h;  
	bi.biPlanes = 1;	
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;	  
	bi.biSizeImage = 0;  
	bi.biXPelsPerMeter = 0;    
	bi.biYPelsPerMeter = 0;    
	bi.biClrUsed = 0;	 
	bi.biClrImportant = 0;

	BOOL ret = FALSE;
    uint8 *lpbitmap = (uint8 *)GlobalLock(m_hDib); 
    
	GetDIBits(m_dcDesktop, m_hBitmap, 0, (UINT)h, lpbitmap, (BITMAPINFO *)&bi, DIB_RGB_COLORS);

	ret = m_encoder.encode(lpbitmap, bmpScreen.bmWidthBytes * m_nHeight);

	GlobalUnlock(m_hDib);

	return ret;
}

void CWScreenCapture::captureThread()
{	
	uint32 curtime;
	uint32 lasttime = sys_os_get_ms();
	double timestep = 1000.0 / m_nFramerate;
	double timeout;
		
	while (m_bCapture)
	{
		if (capture())
		{
			curtime = sys_os_get_ms();
			timeout = timestep - (curtime - lasttime);
			if (timeout > 10)
			{				
				usleep(timestep * 1000);
			}
			else
			{
				usleep(10*1000);
			}

			lasttime = curtime;
		}
		else
		{
			usleep(10*1000);
		}
	}

	m_hCapture = 0;
}




