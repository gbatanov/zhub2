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

#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include "sys_inc.h"
#include "video_decoder.h"
#include "audio_decoder.h"
#include "video_render.h"
#include "audio_play.h"
#include "avi_write.h"
#include <QObject>
#include <QWidget>


class CVideoPlayer : public QObject
{
    Q_OBJECT

public:
    CVideoPlayer(QObject * parent = 0);
    virtual ~CVideoPlayer();

    virtual BOOL    open(QString fileName, HWND hWnd);    
    virtual BOOL    play() = 0;
    virtual void    stop() = 0;
    virtual void    pause() = 0;
    virtual void    close();
    virtual void    setVolume(int volume);
    virtual void    snapshot(int videofmt);
    virtual BOOL    record(QString filepath);
    virtual void    stopRecord();
    virtual BOOL    isRecording() {return m_bRecording;}
    virtual BOOL    onRecord() = 0;
    
    virtual void    setAuthInfo(QString acct, QString pass) {m_acct=acct; m_pass=pass;}
	virtual void    setRenderMode(int mode) {m_nRenderMode=mode;}
	virtual void    setRtpMulticast(BOOL flag) {}
	virtual void    setRtpOverUdp(BOOL flag) {}

#ifdef OVER_HTTP
    virtual void    setRtspOverHttp(int flag, int port) {}
#endif

#ifdef BACKCHANNEL
    virtual int     getBCFlag() {return 0;}
    virtual void    setBCFlag(int flag) {}
    virtual int     getBCDataFlag() {return 0;}
    virtual void    setBCDataFlag(int flag) {}
#endif

#ifdef REPLAY
    virtual int     getReplayFlag() {return 0;}
    virtual void    setReplayFlag(int flag) {}
    virtual void    setScale(double scale) {}
    virtual void    setRateControlFlag(int flag) {}
    virtual void    setImmediateFlag(int flag) {}
    virtual void    setFramesFlag(int flag, int interval) {}
    virtual void    setReplayRange(time_t start, time_t end) {}
#endif

    BOOL            openVideo(int codec);
    void            closeVideo();
    BOOL            openAudio(int codec, int samplerate, int channels, uint8 * config, int configlen);
    void            closeAudio();

    void            playVideo(uint8 * data, int len, uint32 ts, uint16 seq);
    void            playAudio(uint8 * data, int len, uint32 ts, uint16 seq);

    void            recordVideo(uint8 * data, int len, uint32 ts, uint16 seq);
    void            recordAudio(uint8 * data, int len, uint32 ts, uint16 seq);

    void            onVideoFrame(AVFrame * frame);
    void            onAudioFrame(AVFrame * frame);

signals:
    void	        notify(int);
    void            snapshoted(AVFrame * frame);    

protected:
    BOOL            initSnapFrame(AVFrame * frame, int dstfmt);
    BOOL            initRenderFrame(AVFrame * frame, int dstfmt);
    BOOL            doSnapshot(AVFrame * srcframe);
    AVFrame *       convertFrame(AVFrame * srcframe, AVFrame * dstframe, BOOL updown);
    
protected:
    BOOL            m_bVideoInited;
    BOOL            m_bAudioInited;
    CVideoDecoder * m_pVideoDecoder;	
	CAudioDecoder * m_pAudioDecoder;
	CVideoRender  * m_pVideoRender;
	CAudioPlay    * m_pAudioPlay;

    QString         m_acct;
    QString         m_pass;
    QString         m_sFileName;
	HWND            m_nVideoWnd;
	int				m_nRenderMode;
	int             m_nDstVideoFmt;
	BOOL            m_bSnapshot;
	int             m_nSnapVideoFmt;

    AVFrame       * m_pSnapFrame;
	AVFrame       * m_pRenderFrame;

    BOOL            m_bRecording;
    AVICTX        * m_pAviCtx;
    void          * m_pRecordMutex;
};

#endif // end of VIDEO_PLAYER_H



