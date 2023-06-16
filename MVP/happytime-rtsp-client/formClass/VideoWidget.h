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

#ifndef _VIDEO_WIDGET_H_
#define _VIDEO_WIDGET_H_

#include <QWidget>
#include "ui_VideoWidget.h"
#include "FloatWidget.h"
#include "video_player.h"
#include <QTimer>


/******************************************************************/
class VideoWidget : public QWidget
{
    Q_OBJECT

public:
	VideoWidget(QWidget *parent = 0, Qt::WindowFlags flags = 0);
    ~VideoWidget();

	bool	isSelected() {return m_bSelected;}
    void	setSelected(bool flag);

	QString getUrl() { return m_url; }
	QString	getAcct() { return m_acct; }
	QString	getPass() { return m_pass; }

	void	setDevInfo(QString url, QString acct, QString pass, int bcflag = 0);
	void	setRenderMode(int mode);

#ifdef BACKCHANNEL
	void	setBackChannelFlag(int flag) { m_nBackChannelFlag = flag; }
#endif

public slots:
    void	slotCloseVideo();
    void	slotResumeNormal();
    void	slotFillWhole();
	void	slotKeepAspectRatio();
	void	slotWidgetSelecting();
	void	slotFullscreen();
	void    slotDragDroped(QString url, QString acct, QString pass, int bcflag);
	void    slotPlayerNotify(int event);
	void    slotReconn();
	void    slotFloatWidget();
	void	slotPlay();
	void	slotPause();
	void	slotStop();
	void	slotMicphone();
	void	slotSnapshot();
	void	slotRecord();
	void	slotVolume();
	void	slotVolumeChanged(int);
	void	slotContextMenu(QPoint);
	void    slotSnapshoted(AVFrame * frame);

signals:
    void	widgetSelecting(QWidget * pWidget);
        
protected:
	void	paintEvent(QPaintEvent * event);
	void	resizeEvent(QResizeEvent * event);
    void	keyPressEvent(QKeyEvent * event); 
	void	mousePressEvent(QMouseEvent *event);
	bool	event(QEvent * event);

    void	fullScreen();
    void	resumeWidget();
	void	closePlayer();
    void	closeVideo();
	void	makeCall();
	void	showFloatWidget(bool show);
    
private:
    void	initVideoWidget();
	void	initActions();	
	bool	canMaximized();
	void	showParent(bool show);
	void	startRecord();
	void	stopRecord();
	void    setMicphone(int flag);
	QString getUrlIp(QString &url);
	
private:
	Ui::VideoWidget ui;
	bool			m_bSelected;
	bool			m_bFullScreen;

	QString			m_url;
	QString			m_acct;
	QString			m_pass;
	QString         m_ip;

	CVideoPlayer  * m_pPlayer;
	FloatWidget   * m_pFloatWidget;
	int				m_bConnSucc;
	int				m_nRenderMode;

	QTimer			m_timerReconn;
	QTimer          m_timerFloatWidget;

	QAction       * m_actionCloseVideo;
	QAction       * m_actionFullScreen;
	QAction       * m_actionResumeNormal;
	QAction	      * m_actionFillWhole;
	QAction	      * m_actionKeepAspectRatio;

#ifdef BACKCHANNEL
	int				m_nBackChannelFlag;
#endif
};

#endif


