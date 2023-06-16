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
#include "audio_play_dsound.h"

/**************************************************************************************/

#pragma comment(lib, "dsound.lib")

#define HTAUDIOBUF_MAX  200 * 1024


/**************************************************************************************/

CDSoundAudioPlay::CDSoundAudioPlay(int samplerate, int channels)
: CAudioPlay(samplerate, channels)
, m_pDSound8(NULL)
, m_pDSoundBuffer(NULL)
, m_dwOffset(0)
{
    
}

CDSoundAudioPlay::~CDSoundAudioPlay(void)
{
    stopPlay();
}

BOOL CDSoundAudioPlay::startPlay()
{
	HRESULT ret = DirectSoundCreate8(NULL, &m_pDSound8, NULL);
	if (FAILED(ret))
	{
	    log_print(HT_LOG_ERR, "%s, DirectSoundCreate8 failed\r\n", __FUNCTION__);
	    return FALSE;
	}

	ret = m_pDSound8->SetCooperativeLevel(GetDesktopWindow(), DSSCL_NORMAL);
	if (FAILED(ret))
	{
	    stopPlay();
	    log_print(HT_LOG_ERR, "%s, SetCooperativeLevel failed\r\n", __FUNCTION__);
	    return FALSE;
	}

    WAVEFORMATEX format;
    memset(&format, 0, sizeof(WAVEFORMATEX));

    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = m_nChannels;
    format.wBitsPerSample = 16;
    format.nSamplesPerSec = m_nSamplerate;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nBlockAlign * format.nSamplesPerSec;   
    format.cbSize = 0;
    
    DSBUFFERDESC buf_desc;
    memset(&buf_desc, 0, sizeof(DSBUFFERDESC));
    
    buf_desc.dwSize = sizeof(buf_desc);
    buf_desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME;
    buf_desc.dwBufferBytes = HTAUDIOBUF_MAX;
    buf_desc.dwReserved = 0;
    buf_desc.lpwfxFormat = &format;
    
	ret = m_pDSound8->CreateSoundBuffer(&buf_desc, &m_pDSoundBuffer, NULL);
    if (FAILED(ret))
	{
	    stopPlay();
	    log_print(HT_LOG_ERR, "%s, CreateSoundBuffer failed\r\n", __FUNCTION__);
	    return FALSE;
	}

    m_pDSoundBuffer->SetFormat(&format);

    void * buf1 = NULL;
    DWORD  len1 = 0;
    void * buf2 = NULL;
    DWORD  len2 = 0;
    
    /* Silence the initial audio buffer */
    ret = m_pDSoundBuffer->Lock(0, buf_desc.dwBufferBytes, &buf1, &len1, &buf2, &len2, DSBLOCK_ENTIREBUFFER);
    if (ret == DS_OK) 
    {
        memset(buf1, 0, len1);

        m_pDSoundBuffer->Unlock(buf1, len1, buf2, len2);
    }
    
    m_bInited = TRUE;
    
	return TRUE;
}

void CDSoundAudioPlay::stopPlay(void)
{
	m_bInited = FALSE;

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
}

int CDSoundAudioPlay::setVolume(int volume)
{
    if (m_pDSoundBuffer)
    {
        double db = (double) volume / (HTVOLUME_MAX - HTVOLUME_MIN);
        if (db < 0)
        {
            db = -db;
        }
            
        int nv = (DSBVOLUME_MAX - DSBVOLUME_MIN) * db + DSBVOLUME_MIN;
            
        HRESULT ret = m_pDSoundBuffer->SetVolume(nv);
        if (SUCCEEDED(ret))
        {
            return TRUE;
        }
    }

    return FALSE;
}

int CDSoundAudioPlay::getVolume()
{
    if (m_pDSoundBuffer)
    {
        long volume = 0;
        
        HRESULT ret = m_pDSoundBuffer->GetVolume(&volume);
        if (SUCCEEDED(ret))
        {
            double db = (double) volume / (DSBVOLUME_MAX - DSBVOLUME_MIN);
            if (db < 0)
            {
                db = -db;
            }
            
            int nv = (HTVOLUME_MAX - HTVOLUME_MIN) * db + HTVOLUME_MIN;
            
            return nv;
        }
    }

    return 0;
}

void CDSoundAudioPlay::playAudio(uint8 * data, int size) 
{
    if (!m_bInited)
    {
        return;   
    }
    
    void * buf1 = NULL;
    DWORD  len1 = 0;
    void * buf2 = NULL;
    DWORD  len2 = 0;

	HRESULT ret = m_pDSoundBuffer->Lock(m_dwOffset, size, &buf1, &len1, &buf2, &len2, 0);
	if (ret == DSERR_BUFFERLOST) 
	{
        m_pDSoundBuffer->Restore();
        ret = m_pDSoundBuffer->Lock(m_dwOffset, size, &buf1, &len1, &buf2, &len2, 0);
    }

    if (ret != DS_OK) 
    {
        log_print(HT_LOG_ERR, "%s, direct sound lock failed. ret=%d\r\n", ret);
        return;
    }
    
	if (buf1 && len1 > 0)
	{
	    memcpy(buf1, data, len1);
	}

	if (buf2 && len2 > 0)
	{
	    memcpy(buf2, data+len1, len2);
	}

	m_pDSoundBuffer->Unlock(buf1, len1, buf2, len2);

    m_dwOffset += len1 + len2;
    m_dwOffset %= HTAUDIOBUF_MAX;
    
	m_pDSoundBuffer->Play(0, 0, 0);
} 



