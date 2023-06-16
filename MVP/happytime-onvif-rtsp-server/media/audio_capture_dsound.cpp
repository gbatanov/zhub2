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
#include "audio_capture_dsound.h"
#include "lock.h"

/***************************************************************************************/
#define HTAUDIOC_CHNUMS     2
#define HTAUDIOC_SAMPLE     1024
#define HTAUDIOC_BPS        2
#define HTAUDIOC_CHUNKS     8

#define HTAUDIOCBUF_MAX     (HTAUDIOC_CHUNKS * HTAUDIOC_CHNUMS * HTAUDIOC_BPS * HTAUDIOC_SAMPLE)

int    CDSoundAudioCapture::m_nDevNums = 0;
void * CDSoundAudioCapture::m_pDevNumsMutes = sys_os_create_mutex();
GUID   CDSoundAudioCapture::m_devGuid[MAX_AUDIO_DEV_NUMS];

BOOL CALLBACK DevEnumCallback(LPGUID guid, LPCTSTR desc, LPCTSTR module, LPVOID data)
{
    if (guid != NULL)   // skip default device
    {  
        memcpy(&CDSoundAudioCapture::m_devGuid[CDSoundAudioCapture::m_nDevNums], guid, sizeof(GUID));

        CDSoundAudioCapture::m_nDevNums++;
        if (CDSoundAudioCapture::m_nDevNums >= MAX_AUDIO_DEV_NUMS)
        {
            return FALSE;
        }
    }

    return TRUE;
}

/***************************************************************************************/
static void * audioCaptureThread(void * argv)
{
	CDSoundAudioCapture *capture = (CDSoundAudioCapture *)argv;

	capture->captureThread();

	return NULL;
}


/***************************************************************************************/
CDSoundAudioCapture::CDSoundAudioCapture() 
: CAudioCapture()
, m_nLastChunk(0)
, m_pDSound8(NULL)
, m_pDSoundBuffer(NULL)
{
    
}

CDSoundAudioCapture::~CDSoundAudioCapture()
{
    stopCapture();
}

CAudioCapture * CDSoundAudioCapture::getInstance(int devid)
{
	if (devid < 0 || devid >= MAX_AUDIO_DEV_NUMS)
	{
		return NULL;
	}
	
	if (NULL == m_pInstance[devid])
	{
		sys_os_mutex_enter(m_pInstMutex);

		if (NULL == m_pInstance[devid])
		{
			m_pInstance[devid] = new CDSoundAudioCapture;
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

int CDSoundAudioCapture::getDeviceNums()
{
    CLock lock(m_pDevNumsMutes);

    m_nDevNums = 0;
    
    DirectSoundCaptureEnumerate(DevEnumCallback, NULL);
    
	return m_nDevNums;
}

BOOL CDSoundAudioCapture::initCapture(int codec, int samplerate, int channels, int bitrate)
{
	CLock lock(m_pMutex);
	
	if (m_bInited)
	{
		return TRUE;
	}

	if (m_nDevIndex >= m_nDevNums)
	{
	    return FALSE;
	}

    HRESULT ret = DirectSoundCaptureCreate8(&m_devGuid[m_nDevIndex], &m_pDSound8, NULL);
	if (FAILED(ret))
	{
	    log_print(HT_LOG_ERR, "%s, audio captrue create failed\r\n", __FUNCTION__);
	    return FALSE;
	}

	WAVEFORMATEX format;
    memset(&format, 0, sizeof(WAVEFORMATEX));

    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.wBitsPerSample = 16;
    format.nSamplesPerSec = samplerate;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nBlockAlign * format.nSamplesPerSec;   
    format.cbSize = 0;
    
    DSCBUFFERDESC buf_desc;
    memset(&buf_desc, 0, sizeof(buf_desc));
    
    buf_desc.dwSize = sizeof(buf_desc);
    buf_desc.dwFlags = DSCBCAPS_WAVEMAPPED;
    buf_desc.dwBufferBytes = HTAUDIOCBUF_MAX;
    buf_desc.dwReserved = 0;
    buf_desc.lpwfxFormat = &format;

	ret = m_pDSound8->CreateCaptureBuffer(&buf_desc, &m_pDSoundBuffer, NULL);
    if (ret != DS_OK) 
    {
        log_print(HT_LOG_ERR, "%s, audio captrue create buffer failed\r\n", __FUNCTION__);
        
        stopCapture();
        
        return FALSE;
    }
	
	AudioEncoderParam params;
	params.SrcChannels = 2;
	params.SrcSamplefmt = AV_SAMPLE_FMT_S16;
	params.SrcSamplerate = samplerate;
	params.DstChannels = channels;
	params.DstSamplefmt = AV_SAMPLE_FMT_S16;
	params.DstSamplerate = samplerate;
	params.DstBitrate = bitrate;
	params.DstCodec = codec;

	if (m_encoder.init(&params) == FALSE)
	{
		return FALSE;
	}

    m_nChannels = channels;
	m_nSampleRate = samplerate;
	m_nBitrate = bitrate;
	m_bInited = TRUE;
	
	return TRUE;
}

BOOL CDSoundAudioCapture::startCapture()
{
    CLock lock(m_pMutex);

	if (!m_bInited)
	{
		return FALSE;
	}
	
	if (m_bCapture)
	{
		return TRUE;
	}

	HRESULT ret = m_pDSoundBuffer->Start(DSCBSTART_LOOPING);
    if (ret != DS_OK) 
    {
        log_print(HT_LOG_ERR, "%s, audio captrue start failed\r\n", __FUNCTION__);
        return FALSE;
    }

    m_bCapture = TRUE;
	m_hCapture = sys_os_create_thread((void *)audioCaptureThread, this);

	return (m_hCapture ? TRUE : FALSE);
}

void CDSoundAudioCapture::stopCapture(void)
{
	CLock lock(m_pMutex);
	
	if (m_bCapture)
	{
		m_bCapture = FALSE;		
	}

	while (m_hCapture)
	{
		usleep(10*1000);
	}

	if (m_pDSoundBuffer)
    {
        m_pDSoundBuffer->Stop();
	    m_pDSoundBuffer->Release();
	    m_pDSoundBuffer = NULL;
	}

	if (m_pDSound8)
    {
	    m_pDSound8->Release();
	    m_pDSound8 = NULL;
	}

	m_bInited = FALSE;
}

BOOL CDSoundAudioCapture::capture()
{
    int specsize = HTAUDIOC_CHNUMS * HTAUDIOC_BPS * HTAUDIOC_SAMPLE;
    DWORD junk, cursor, ptr1len, ptr2len;
    void *ptr1, *ptr2;
    
    while (m_bCapture) 
    {
        if (m_pDSoundBuffer->GetCurrentPosition(&junk, &cursor) != DS_OK) 
        {
            return FALSE;
        }
        
        if ((cursor / specsize) == m_nLastChunk) 
        {
            usleep(10*1000);
        } 
        else 
        {
            break;
        }
    }

    if (m_pDSoundBuffer->Lock(m_nLastChunk * specsize, specsize, &ptr1, &ptr1len, &ptr2, &ptr2len, 0) != DS_OK) 
    {
        return FALSE;
    }

    assert(ptr1len == specsize);
    assert(ptr2 == NULL);
    assert(ptr2len == 0);

    m_encoder.encode((uint8*)ptr1, ptr1len);

    if (m_pDSoundBuffer->Unlock(ptr1, ptr1len, ptr2, ptr2len) != DS_OK) 
    {
        return FALSE;
    }

    m_nLastChunk = (m_nLastChunk + 1) % HTAUDIOC_CHUNKS;
    
    return TRUE;
}

void CDSoundAudioCapture::captureThread()
{
	while (m_bCapture)
	{
		if (capture())
		{
			usleep(1000000 * HTAUDIOC_SAMPLE / m_nSampleRate);
		}
		else
		{
			usleep(10*1000);
		}
	}

	m_hCapture = 0;
}

	



