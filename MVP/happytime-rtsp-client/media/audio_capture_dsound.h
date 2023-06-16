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

#ifndef AUDIO_CAPTURE_DSOUND_H
#define AUDIO_CAPTURE_DSOUND_H

/***************************************************************************************/
#include "media_format.h"
#include "audio_capture.h"
#include <dsound.h>

/***************************************************************************************/

class CDSoundAudioCapture : public CAudioCapture
{
public:
    
    // get audio capture devcie nubmers     
    static int getDeviceNums();

    // get single instance
	static CAudioCapture * getInstance(int devid);

    BOOL    initCapture(int codec, int sampleRate, int channels, int bitrate);
	BOOL    startCapture();

    void    captureThread();
    
private:
    CDSoundAudioCapture();
    CDSoundAudioCapture(CDSoundAudioCapture &obj);
	~CDSoundAudioCapture();	

    void    stopCapture();
    BOOL    capture();

public:
    static void               * m_pDevNumsMutes;
    static int                  m_nDevNums;
    static GUID                 m_devGuid[MAX_AUDIO_DEV_NUMS];
    
private:
    int                         m_nLastChunk;
    LPDIRECTSOUNDCAPTURE8       m_pDSound8;
    LPDIRECTSOUNDCAPTUREBUFFER  m_pDSoundBuffer;
};


#endif // AUDIO_CAPTURE_DSOUND_H



