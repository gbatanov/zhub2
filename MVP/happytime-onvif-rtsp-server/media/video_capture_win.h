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

#ifndef _VIDIO_CAPTURE_WIN_H
#define _VIDIO_CAPTURE_WIN_H

#include <mfidl.h>
#include <mfreadwrite.h>
#include "video_capture.h"

/***************************************************************************************/

class CWVideoCapture : public CVideoCapture
{
public:
    // get video capture devices numbers
    static int getDeviceNums();

    // get the single instance
	static CVideoCapture * getInstance(int devid);

	BOOL    initCapture(int codec, int width, int height, int framerate, int bitrate);
	BOOL    startCapture();

    void    captureThread();
    
private:
    CWVideoCapture();
    CWVideoCapture(CWVideoCapture &obj);
	~CWVideoCapture();

    BOOL    capture();
	void    stopCapture();
    BOOL    checkMediaType();
    BOOL    setMediaFormat(int width, int height);
    BOOL    setFramerate(int framerate);
    
private:
    IMFMediaSource  * m_pSource;
    IMFSourceReader * m_pReader;
};

#endif // _VIDIO_CAPTURE_WIN_H


