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

#ifndef RTSP_PLAYER_H
#define RTSP_PLAYER_H

#include "video_player.h"
#include "rtsp_cln.h"


class CRtspPlayer : public CVideoPlayer
{
    Q_OBJECT

public:
    CRtspPlayer(QObject * parent = 0);
    virtual ~CRtspPlayer();

    BOOL    open(QString fileName, HWND hWnd);
    void    close();
    BOOL    play();
    void    stop();
    void    pause();

    void    setRtpMulticast(BOOL flag);
    void    setRtpOverUdp(BOOL flag);

#ifdef OVER_HTTP
    void    setRtspOverHttp(int flag, int port);
#endif

#ifdef BACKCHANNEL
    int     getBCFlag();
    void    setBCFlag(int flag);
    int     getBCDataFlag();
    void    setBCDataFlag(int flag);
#endif

#ifdef REPLAY
    int     getReplayFlag();
    void    setReplayFlag(int flag);
    void    setScale(double scale);
    void    setRateControlFlag(int flag);
    void    setImmediateFlag(int flag);
    void    setFramesFlag(int flag, int interval);
    void    setReplayRange(time_t start, time_t end);
#endif

    void    onNotify(int evt);
    void    onAudio(uint8 * pdata, int len, uint32 ts, uint16 seq);
    void    onVideo(uint8 * pdata, int len, uint32 ts, uint16 seq);
    BOOL    onRecord();
    
private:
    char		m_ip[32];
    int			m_port;

    CRtspClient	m_rtsp;
    BOOL		m_bPlaying;
    BOOL		m_bPaused;
};

#endif // end of RTSP_PLAYER_H



