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
#include "video_capture_win.h"
#include "media_format.h"
#include "lock.h"
#include <mfidl.h>
#include <mfapi.h>


/***************************************************************************************/

static void * videoCaptureThread(void * argv)
{
	CWVideoCapture *capture = (CWVideoCapture *)argv;

	capture->captureThread();

	return NULL;
}

CWVideoCapture::CWVideoCapture(void) : CVideoCapture()
{
	m_pSource = NULL;
    m_pReader = NULL;
}

CWVideoCapture::~CWVideoCapture(void)
{
    stopCapture();
}

CVideoCapture * CWVideoCapture::getInstance(int devid)
{
	if (devid < 0 || devid >= MAX_VIDEO_DEV_NUMS)
	{
		return NULL;
	}
	
	if (NULL == m_pInstance[devid])
	{
		sys_os_mutex_enter(m_pInstMutex);

		if (NULL == m_pInstance[devid])
		{
			m_pInstance[devid] = (CVideoCapture *)new CWVideoCapture;
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

int CWVideoCapture::getDeviceNums()
{
    int i, count = 0;
    IMFAttributes * pAttributes = NULL;
    IMFActivate ** ppDevices = NULL;
    
	// Create an attribute store to specify the enumeration parameters.
    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr))
    {
        goto done;
    }

    // Source type: video capture devices
    hr = pAttributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr))
    {
        goto done;
    }

    // Enumerate devices
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, (UINT*)&count);
    if (FAILED(hr))
    {
        goto done;
    }

done:

    if (pAttributes)
    {
        pAttributes->Release();
    }

    for (i = 0; i < count; i++)
    {
        ppDevices[i]->Release();
    }
    
    CoTaskMemFree(ppDevices);
	
	return count;
}

BOOL CWVideoCapture::checkMediaType()
{
    BOOL ret = FALSE;
    GUID guid;
    IMFMediaType *pType = NULL;
    
    HRESULT hr = m_pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pType);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = pType->GetGUID(MF_MT_SUBTYPE, &guid);
    if (FAILED(hr))
    {
        goto done;
    }

    if (guid == MFVideoFormat_IYUV)
	{
		m_nVideoFormat = VIDEO_FMT_YUV420P;
	}
	else if (guid == MFVideoFormat_YUY2)
	{
		m_nVideoFormat = VIDEO_FMT_YUYV422;
	}
	else if (guid == MFVideoFormat_YVYU)
	{
		m_nVideoFormat = VIDEO_FMT_YVYU422;
	}
	else if (guid == MFVideoFormat_UYVY)
	{
		m_nVideoFormat = VIDEO_FMT_UYVY422;
	}
	else if (guid == MFVideoFormat_YV12)
	{
		m_nVideoFormat = VIDEO_FMT_YUV420P;
	}
	else if (guid == MFVideoFormat_NV12)
	{
		m_nVideoFormat = VIDEO_FMT_NV12;
	}
	else if (guid == MFVideoFormat_RGB24)
	{
		m_nVideoFormat = VIDEO_FMT_RGB24;
	}
	else if (guid == MFVideoFormat_RGB32)
	{
		m_nVideoFormat = VIDEO_FMT_RGB32;
	}
	else if (guid == MFVideoFormat_ARGB32)
	{
		m_nVideoFormat = VIDEO_FMT_ARGB;
	}
	else
	{
		goto done;
	}

	hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, (UINT*)&m_nWidth, (UINT*)&m_nHeight);
    if (FAILED(hr))
    {
        goto done;
    }

    UINT num, den;
    
    hr = MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &num, &den);
    if (FAILED(hr))
    {
        goto done;
    }
    
    m_nFramerate = num/den;

    ret = TRUE;

done:

    if (pType)
    {
        pType->Release();
    }

    return ret;
}

BOOL CWVideoCapture::setMediaFormat(int width, int height)
{
    BOOL set = FALSE;
    int dw = 0, dh = 0, w = 0, h = 0;
    DWORD index = 0;
    HRESULT hr;
    IMFMediaType *pType = NULL;
	IMFMediaType *pDType = NULL;

    hr = m_pReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, index, &pType);
    while (SUCCEEDED(hr) && pType)
    {
        GUID guid;
        
        hr = pType->GetGUID(MF_MT_SUBTYPE, &guid);
        if (FAILED(hr))
        {
            goto retry;
        }

        if (guid != MFVideoFormat_IYUV && guid != MFVideoFormat_YUY2 &&
            guid != MFVideoFormat_YVYU && guid != MFVideoFormat_UYVY && 
            guid != MFVideoFormat_YV12 && guid != MFVideoFormat_NV12 && 
            guid != MFVideoFormat_RGB24 && guid != MFVideoFormat_RGB32 && 
            guid != MFVideoFormat_ARGB32)
    	{
    	    goto retry;
    	}

    	hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, (UINT*)&w, (UINT*)&h);
        if (FAILED(hr))
        {
            goto retry;
        }

        if (w == width && h == height)
        {
            hr = m_pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType);
            if (SUCCEEDED(hr))
            {
                set = TRUE;
                pType->Release();
                break;
            }
        }
        else if (dw * dh < w * h)
        {
            dw = w;
            dh = h;

            if (pDType)
            {
                pDType->Release();
            }
            pDType = pType;
            
            index++;
            hr = m_pReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, index, &pType);
            continue;
        }

retry:

        pType->Release();
        
        index++;
        hr = m_pReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, index, &pType);
    }

    if (!set && pDType)
    {
        hr = m_pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pDType);
        if (SUCCEEDED(hr))
        {
            set = TRUE;
        }
    }

    if (pDType)
    {
        pDType->Release();
    }
        
    return set;
}

BOOL CWVideoCapture::setFramerate(int framerate)
{
    if (framerate <= 0)
    {
        return TRUE;
    }

    int  min, max, cur;
    UINT num, den;
    BOOL ret = FALSE;
    IMFMediaType *pType = NULL;
    
    HRESULT hr = m_pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pType);
    if (FAILED(hr))
    {
        goto done;
    }
    
    hr = MFGetAttributeRatio(pType, MF_MT_FRAME_RATE_RANGE_MIN, &num, &den);
    if (FAILED(hr))
    {
        goto done;
    }

    min = num / den;

    hr = MFGetAttributeRatio(pType, MF_MT_FRAME_RATE_RANGE_MAX, &num, &den);
    if (FAILED(hr))
    {
        goto done;
    }

    max = num / den;

    hr = MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &num, &den);
    if (FAILED(hr))
    {
        goto done;
    }

    cur = num /den;

    if (framerate < min)
    {
        framerate = min;
    }
    else if (framerate > max)
    {
        framerate = max;
    }

    if (framerate == cur)
    {
        ret = TRUE;
        goto done;
    }

    num = framerate;
    den = 1;

    hr = MFSetAttributeRatio(pType, MF_MT_FRAME_RATE, num, den);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = m_pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, pType);
    if (FAILED(hr))
    {
        goto done;
    }

    ret = TRUE;

done:

    if (pType)
    {
        pType->Release();
    }

    return ret;
}

BOOL CWVideoCapture::initCapture(int codec, int width, int height, int framerate, int bitrate)
{
	CLock lock(m_pMutex);
	
	if (m_bInited)
	{
		return TRUE;
	}
	
    int i, count = 0;
    IMFAttributes * pAttributes = NULL;
    IMFActivate ** ppDevices = NULL;
    
	// Create an attribute store to specify the enumeration parameters.
    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr))
    {
        goto done;
    }

    // Source type: video capture devices
    hr = pAttributes->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr))
    {
        goto done;
    }

    // Enumerate devices
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, (UINT*)&count);
    if (FAILED(hr))
    {
        goto done;
    }

    if (count <= m_nDevIndex)
    {
        goto done;
    }
    
    hr = ppDevices[m_nDevIndex]->ActivateObject(__uuidof(IMFMediaSource), (void**)&m_pSource);
    if (FAILED(hr))
    {
        goto done;
    }

    hr = MFCreateSourceReaderFromMediaSource(m_pSource, NULL, &m_pReader);
    if (FAILED(hr))
    {
        goto done;
    }

    setMediaFormat(width, height);

    setFramerate(framerate);

    if (!checkMediaType())
    {
        goto done;
    }

    VideoEncoderParam params;
	memset(&params, 0, sizeof(params));
	
	params.SrcWidth = m_nWidth;
	params.SrcHeight = m_nHeight;
	params.SrcPixFmt = CVideoEncoder::toAVPixelFormat(m_nVideoFormat);
	params.DstCodec = codec;
	params.DstWidth = width ? width : m_nWidth;
	params.DstHeight = height ? height : m_nHeight;
	params.DstFramerate = m_nFramerate;
	params.DstBitrate = bitrate;
		
	if (m_encoder.init(&params) == FALSE)
	{
		return FALSE;
	}

	m_nBitrate = bitrate;
   	m_bInited = TRUE;
   	
done:

    if (pAttributes)
    {
        pAttributes->Release();
    }

    for (i = 0; i < count; i++)
    {
        ppDevices[i]->Release();
    }
    
    CoTaskMemFree(ppDevices);
   	
	return m_bInited;
}

BOOL CWVideoCapture::capture()
{
    CLock lock(m_pMutex);
    
    if (!m_bInited)
	{
		return FALSE;
	}

    DWORD dwFlags = 0;
	IMFSample *pSample = NULL;
	
    HRESULT hr = m_pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, NULL, &dwFlags, NULL, &pSample);
    if (FAILED(hr) || NULL == pSample)
    {
        return FALSE;
    }

    IMFMediaBuffer *pBuffer = NULL;

    hr = pSample->ConvertToContiguousBuffer(&pBuffer);
    if (FAILED(hr))
    {
        pSample->Release();
        return FALSE;
    }

    BYTE * buff = NULL;
    DWORD  size = 0;
  
    hr = pBuffer->Lock(&buff, NULL, &size);
    if (FAILED(hr))
    {
        pBuffer->Release();
        pSample->Release();
        return FALSE;
    }

    BOOL ret = m_encoder.encode(buff, size);

    pBuffer->Unlock();

    pBuffer->Release();
    pSample->Release();

    return ret;	
}

BOOL CWVideoCapture::startCapture()
{
	CLock lock(m_pMutex);
	
	if (m_hCapture)
	{
		return TRUE;
	}

	m_bCapture = TRUE;
	m_hCapture = sys_os_create_thread((void *)videoCaptureThread, this);

	return (NULL != m_hCapture);
}

void CWVideoCapture::stopCapture()
{
	CLock lock(m_pMutex);
	
	// wiat for capture thread exit
	m_bCapture = FALSE;
	
	while (m_hCapture)
	{
		usleep(10*1000);
	}

	if (m_pReader)
	{
	    m_pReader->Release();
	    m_pReader = NULL;
	}

	if (m_pSource)
	{
	    m_pSource->Release();
	    m_pSource = NULL;
	}
	
    m_bInited = FALSE;
}

void CWVideoCapture::captureThread()
{	
	uint32 curtime;
	uint32 lasttime = sys_os_get_ms();
	double timestep = 1000.0 / m_nFramerate;
		
	while (m_bCapture)
	{
		if (capture())
		{
			curtime = sys_os_get_ms();
			if (curtime - lasttime < timestep)
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





