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

#include "stdafx.h"
#include "sys_inc.h"
#include "VideoWidget.h"
#include "rtsp_player.h"
#include "utils.h"
#include <QMenu>
#include <QPainter>
#include <QMimeData>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QDebug>
#include <QtGlobal>
#include "SetRtspInfo.h"

/***********************************************************************/

VideoWidget::VideoWidget(QWidget *parent, Qt::WindowFlags flags)
: QWidget(parent, flags)
, m_bSelected(false)
, m_bFullScreen(false)
, m_pPlayer(NULL)
, m_pFloatWidget(NULL)
, m_bConnSucc(0)
, m_nRenderMode(RENDER_MODE_KEEP)
#ifdef BACKCHANNEL
, m_nBackChannelFlag(0)
#endif
{
	ui.setupUi(this);

    initVideoWidget();
	initActions();
}


VideoWidget::~VideoWidget()
{
    m_timerFloatWidget.stop();
    
	if (m_pFloatWidget)
	{
		delete m_pFloatWidget;
		m_pFloatWidget = NULL;
	}

	closeVideo();
}

void VideoWidget::initVideoWidget()
{
	setMouseTracking(true);
	setAcceptDrops(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(false);

	m_timerReconn.setSingleShot(true);

    m_timerFloatWidget.setParent(this);
	m_timerFloatWidget.start(1000);
	
	connect(&m_timerReconn, SIGNAL(timeout()), this, SLOT(slotReconn()));
	connect(&m_timerFloatWidget, SIGNAL(timeout()), this, SLOT(slotFloatWidget()));

	QObject::connect(ui.widgetVideo, SIGNAL(widgetSelecting()), this, SLOT(slotWidgetSelecting()));
	QObject::connect(ui.widgetVideo, SIGNAL(doubleClicked()), this, SLOT(slotFullscreen()));
	QObject::connect(ui.widgetVideo, SIGNAL(dragDroped(QString,QString,QString,QString,int,int)), this, SLOT(slotDragDroped(QString,QString,QString,QString,int,int)));
	QObject::connect(ui.widgetVideo, SIGNAL(contextMenu(QPoint)), this, SLOT(slotContextMenu(QPoint)));
}

void VideoWidget::initActions()
{
	m_actionCloseVideo = new QAction(this);
	m_actionCloseVideo->setText(tr("Close Video"));
	m_actionCloseVideo->setStatusTip(tr("Close Video"));

	m_actionFullScreen = new QAction(this);
	m_actionFullScreen->setText(tr("Full Screen"));
	m_actionFullScreen->setStatusTip(tr("Full Screen"));

	m_actionResumeNormal = new QAction(this);
	m_actionResumeNormal->setText(tr("Exit Full Screen"));
	m_actionResumeNormal->setStatusTip(tr("Exit Full Screen"));

	m_actionFillWhole = new QAction(this);
	m_actionFillWhole->setText(tr("Fill the whole window"));
	m_actionFillWhole->setStatusTip(tr("Fill the whole window"));

	m_actionKeepAspectRatio = new QAction(this);
	m_actionKeepAspectRatio->setText(tr("Keep the original aspect ratio"));
	m_actionKeepAspectRatio->setStatusTip(tr("Keep the original aspect ratio"));

	connect(m_actionCloseVideo, SIGNAL(triggered()), this, SLOT(slotCloseVideo()));
	connect(m_actionFullScreen, SIGNAL(triggered()), this, SLOT(slotFullscreen()));
	connect(m_actionResumeNormal, SIGNAL(triggered()), this, SLOT(slotResumeNormal()));
	connect(m_actionFillWhole, SIGNAL(triggered()), this, SLOT(slotFillWhole()));
	connect(m_actionKeepAspectRatio, SIGNAL(triggered()), this, SLOT(slotKeepAspectRatio()));
}

void VideoWidget::paintEvent(QPaintEvent * event)
{
	// qDebug() << "VideoWidget::paintEvent : " << m_bSelected << " , "<< m_bFullScreen;

	QPainter painter(this);

	if (m_bSelected && !m_bFullScreen)
	{
		painter.fillRect(rect(), QBrush(QColor(170, 85, 255)));
	}
	else
	{
		painter.fillRect(rect(), QBrush(QColor(20, 20, 20)));
	}
}

void VideoWidget::resizeEvent(QResizeEvent * event)
{
	if (m_pFloatWidget)
	{
		QRect rect = ui.widgetVideo->geometry();
		QPoint pt = mapToGlobal(rect.topLeft());

		m_pFloatWidget->setGeometry(pt.x(), pt.y(), ui.widgetVideo->width(), ui.widgetVideo->height());
	}
}

void VideoWidget::mousePressEvent(QMouseEvent *event)
{
	qDebug() << "VideoWidget::mousePressEvent";

	slotWidgetSelecting();
}

void VideoWidget::keyPressEvent(QKeyEvent * event)
{
	if (event->key() == Qt::Key_Escape)
	{
		if (m_bFullScreen)
		{
			resumeWidget();
		}
	}
}

bool VideoWidget::canMaximized()
{
	// The parent window is full screen, it cannot be full screen
	QWidget * pParentWidget;
	QObject * pParent = parent();
	while (pParent)
	{
		if (pParent->isWidgetType())
		{
			pParentWidget = (QWidget *)pParent;
			if (pParentWidget->inherits("RtspClient"))
			{
				break;
			}

			if (pParentWidget->isMaximized())
			{
				return false;
			}
		}

		pParent = pParent->parent();
	}

	return true;
}

void VideoWidget::showParent(bool show)
{
	// show parent widget
	QWidget * pParentWidget;
	QObject * pParent = parent();
	while (pParent)
	{
		if (pParent->isWidgetType())
		{
			pParentWidget = (QWidget *)pParent;

			if (show)
				pParentWidget->show();
			else
				pParentWidget->hide();
		}

		pParent = pParent->parent();
	}
}

void VideoWidget::fullScreen()
{
	if (!canMaximized())
	{
		return;
	}

	ui.verticalLayout->removeWidget(ui.widgetVideo);
	ui.widgetVideo->setParent(NULL);
	ui.widgetVideo->setWindowFlags(Qt::FramelessWindowHint);
	ui.widgetVideo->showMaximized();

	if (m_pFloatWidget)
	{
		m_pFloatWidget->hide();
	}

	// hide parent widget
	showParent(false);

	grabKeyboard();

	m_bFullScreen = true;
}

void VideoWidget::resumeWidget()
{
	// show parent widget
	showParent(true);

	ui.widgetVideo->setParent(this);
	ui.widgetVideo->showNormal();

	ui.verticalLayout->addWidget(ui.widgetVideo);

	if (m_pFloatWidget)
	{
		m_pFloatWidget->hide();
	}

	releaseKeyboard();

	m_bFullScreen = false;
}

void VideoWidget::closePlayer()
{
	m_bConnSucc = 0;

	m_timerReconn.stop();

	if (m_pPlayer)
	{
	    QObject::disconnect(m_pPlayer, SIGNAL(notify(int)), this, SLOT(slotPlayerNotify(int)));
        QObject::disconnect(m_pPlayer, SIGNAL(snapshoted(AVFrame*)), this, SLOT(slotSnapshoted(AVFrame*)));
    
		stopRecord();

		setMicphone(0);

		delete m_pPlayer;
		m_pPlayer = NULL;
	}
}

void VideoWidget::closeVideo()
{
	closePlayer();

	m_url = "";
	m_acct = "";
	m_pass = "";

	ui.widgetVideo->setTips("");
}

void VideoWidget::slotCloseVideo()
{
	closeVideo();
}

void VideoWidget::slotResumeNormal()
{
	resumeWidget();
}

void VideoWidget::slotFillWhole()
{
	m_nRenderMode = RENDER_MODE_FILL;

	if (m_pPlayer)
	{
		m_pPlayer->setRenderMode(m_nRenderMode);
	}
}

void VideoWidget::slotKeepAspectRatio()
{
	m_nRenderMode = RENDER_MODE_KEEP;

	if (m_pPlayer)
	{
		m_pPlayer->setRenderMode(m_nRenderMode);
	}
}

void VideoWidget::setSelected(bool flag)
{
	qDebug() << "VideoWidget::setSelected : " << flag;

	m_bSelected = flag;

	update();
}

void VideoWidget::setDevInfo(QString url, QString acct, QString pass, int bcflag)
{
	if (m_url == url && m_acct == acct && m_pass == pass)
	{
		return;
	}

	closeVideo();

	m_url = url;
	m_acct = acct;
	m_pass = pass;
    m_ip = getUrlIp(url);
    
#ifdef BACKCHANNEL
	m_nBackChannelFlag = bcflag;
#endif

	makeCall();
}

void VideoWidget::setRenderMode(int mode)
{
	m_nRenderMode = mode;
}

void VideoWidget::slotWidgetSelecting()
{
	qDebug() << "VideoWidget::slotWidgetSelecting : " << m_bSelected;

	if (!m_bSelected)
	{
		emit widgetSelecting(this);
	}
}

void VideoWidget::slotFullscreen()
{
	if (m_bFullScreen)
	{
		resumeWidget();
	}
	else
	{
		fullScreen();
	}
}

void VideoWidget::slotDragDroped(QString url, QString acct, QString pass, int bcflag)
{
	setDevInfo(url, acct, pass, bcflag);
}

void VideoWidget::makeCall()
{
    m_pPlayer = new CRtspPlayer(this);
	if (NULL == m_pPlayer)
	{
		return;    	
    }

    connect(m_pPlayer, SIGNAL(notify(int)), this, SLOT(slotPlayerNotify(int)), Qt::QueuedConnection);
    connect(m_pPlayer, SIGNAL(snapshoted(AVFrame*)), this, SLOT(slotSnapshoted(AVFrame*)), Qt::QueuedConnection);

    if (m_pPlayer->open(m_url, (HWND)ui.widgetVideo->winId()) >= 0)
    {
		m_pPlayer->setAuthInfo(m_acct, m_pass);
		m_pPlayer->setRenderMode(m_nRenderMode);
		m_pPlayer->setRtpOverUdp(getRtpOverUdp());
		m_pPlayer->setRtpMulticast(getRtpMulticast());

#ifdef OVER_HTTP
        m_pPlayer->setRtspOverHttp(getRtspOverHttp(), getRtspOverHttpPort());
#endif
#ifdef BACKCHANNEL
		m_pPlayer->setBCFlag(m_nBackChannelFlag);
#endif

        m_pPlayer->play();
    }
    else
    {
    	closePlayer();

    	ui.widgetVideo->setTips("Invalid rtsp url");
    }
}

void VideoWidget::showFloatWidget(bool show)
{
	if (show)
	{
		if (NULL == m_pFloatWidget)
		{
			m_pFloatWidget = new FloatWidget(this);

			QObject::connect(m_pFloatWidget, SIGNAL(play()), this, SLOT(slotPlay()));
			QObject::connect(m_pFloatWidget, SIGNAL(pause()), this, SLOT(slotPause()));
			QObject::connect(m_pFloatWidget, SIGNAL(stop()), this, SLOT(slotStop()));
			QObject::connect(m_pFloatWidget, SIGNAL(micphone()), this, SLOT(slotMicphone()));
			QObject::connect(m_pFloatWidget, SIGNAL(snapshot()), this, SLOT(slotSnapshot()));
			QObject::connect(m_pFloatWidget, SIGNAL(record()), this, SLOT(slotRecord()));
			QObject::connect(m_pFloatWidget, SIGNAL(volume()), this, SLOT(slotVolume()));
			QObject::connect(m_pFloatWidget, SIGNAL(volumeChanged(int)), this, SLOT(slotVolumeChanged(int)));
		}

		QRect rect = ui.widgetVideo->geometry();
		QPoint pt = mapToGlobal(rect.topLeft());

		m_pFloatWidget->setGeometry(pt.x(), pt.y(), ui.widgetVideo->width(), ui.widgetVideo->height());
		m_pFloatWidget->show();
	}
	else if (m_pFloatWidget)
	{
		QRect rect = m_pFloatWidget->geometry();

		if (!rect.contains(QCursor::pos()))
		{
			m_pFloatWidget->hide();
		}
	}
}

void VideoWidget::slotPlayerNotify(int event)
{
	QString tips;
	
	m_bConnSucc = 0;
	
	if (event == RTSP_EVE_CONNECTING)
	{
		tips = tr("Connecting");
	}
	else if (event == RTSP_EVE_CONNFAIL)
	{
		tips = tr("Connect failed");

		m_timerReconn.start(5 * 1000);
	}
	else if (event == RTSP_EVE_CONNSUCC)
	{
		tips = "";
		m_bConnSucc = 1;
	}
	else if (event == RTSP_EVE_NOSIGNAL)
	{
		tips = tr("NO Signal");

		m_timerReconn.start(5 * 1000);
	}
	else if (event == RTSP_EVE_NODATA)
	{
		tips = tr("NO Data");

		m_timerReconn.start(5 * 1000);
	}
	else if (event == RTSP_EVE_RESUME)
	{
		tips = "";
		m_bConnSucc = 1;

		m_timerReconn.stop();
	}
	else if (event == RTSP_EVE_STOPPED)
	{
		// tips = tr("Disconnect");
		
		m_timerReconn.start(5 * 1000);

		return;
	}
	else if (event == RTSP_EVE_AUTHFAILED)
	{
		tips = tr("Authenticate failed");
	}

	ui.widgetVideo->setTips(tips);
}

void VideoWidget::slotReconn()
{
	closePlayer();

	makeCall(); 
}

void VideoWidget::slotPlay()
{
	SetRtspInfo dlg(m_url, m_acct, m_pass, this, Qt::WindowCloseButtonHint);

	if (QDialog::Accepted == dlg.exec())
	{
		setDevInfo(dlg.getUrl(), dlg.getUser(), dlg.getPass());
	}
}

void VideoWidget::slotPause()
{
	if (m_pPlayer)
	{
		m_pPlayer->pause();
	}
}

void VideoWidget::slotStop()
{
	closeVideo();
}


void VideoWidget::slotMicphone()
{
#ifdef BACKCHANNEL
	closePlayer();

	ui.widgetVideo->setTips("");

	if (m_nBackChannelFlag)
	{
		m_nBackChannelFlag = 0;
	}
	else
	{
		m_nBackChannelFlag = 1;
	}

	makeCall();
	
	if (m_pPlayer)
	{
		setMicphone(!m_pPlayer->getBCDataFlag());
	}
#endif
}


void VideoWidget::slotVolume()
{
	if (m_pFloatWidget->ui.btnVolume->property("mute").toBool())
	{
		m_pFloatWidget->ui.btnVolume->setProperty("mute", false);
		m_pFloatWidget->ui.btnVolume->setIcon(QIcon(QString::fromUtf8(":/RtspClient/Resources/volume.png")));

		if (m_pPlayer)
		{
			m_pPlayer->setVolume(m_pFloatWidget->ui.sliderVolume->value());
		}
	}
	else
	{
		m_pFloatWidget->ui.btnVolume->setProperty("mute", true);
		m_pFloatWidget->ui.btnVolume->setIcon(QIcon(QString::fromUtf8(":/RtspClient/Resources/mute.png")));

		if (m_pPlayer)
		{
			m_pPlayer->setVolume(0);
		}
	}
}

void VideoWidget::slotVolumeChanged(int action)
{
	int pos = m_pFloatWidget->ui.sliderVolume->sliderPosition();

	// not mute
	if (!m_pFloatWidget->ui.btnVolume->property("mute").toBool())
	{
		if (m_pPlayer)
		{
			m_pPlayer->setVolume(pos);
		}
	}
}

void VideoWidget::slotSnapshot()
{
	if (m_pPlayer)
	{
		m_pPlayer->snapshot(VIDEO_FMT_RGB24);
	}
}

void VideoWidget::slotSnapshoted(AVFrame * frame)
{
    QImage image = QImage(frame->data[0], frame->width, frame->height, frame->linesize[0], QImage::Format_RGB888);
	QString path = getSnapshotPath();
	QString file = getTempFile(m_ip, ".jpg");
	
	if (!image.save(path + "/" + file, "JPG"))
	{
		QMessageBox::information(NULL, tr("Tips"), tr("Write file failed to check the file path, save path:\n") + path);
	}
	else
	{
		QMessageBox::information(NULL, tr("Tips"), tr("Snapshot successful. save path:\n") + path);
	}
}

void VideoWidget::slotRecord()
{
	if (NULL == m_pPlayer)
	{
		return;
	}

	if (m_pPlayer->isRecording())
	{
		stopRecord();
	}
	else
	{
		startRecord();
	}
}

void VideoWidget::startRecord()
{
	if (NULL == m_pPlayer)
	{
		return;
	}

    if (m_pPlayer->isRecording())
    {
        return;
    }

    QString path = getRecordPath();
	QString file = path + "/" + getTempFile(m_ip, ".avi");
	
	if (m_pPlayer->record(file))
	{
		QIcon icon;
		icon.addFile(QString::fromUtf8(":/RtspClient/Resources/stop_record.png"));
		m_pFloatWidget->ui.btnRecord->setIcon(icon);
		m_pFloatWidget->ui.btnRecord->setToolTip(tr("Stop video record"));
	}
}

void VideoWidget::stopRecord()
{
	if (NULL == m_pPlayer)
	{
		return;
	}

	if (m_pPlayer->isRecording())
	{
		m_pPlayer->stopRecord();

		if (m_pFloatWidget)
		{
			QIcon icon;
			icon.addFile(QString::fromUtf8(":/RtspClient/Resources/video_record.png"));
			m_pFloatWidget->ui.btnRecord->setIcon(icon);
			m_pFloatWidget->ui.btnRecord->setToolTip(tr("Video record"));
		}
	}
}

void VideoWidget::setMicphone(int flag)
{
#ifdef BACKCHANNEL
	if (m_pPlayer)
	{
		m_pPlayer->setBCDataFlag(flag);
	}

	if (m_pFloatWidget)
	{
		if (flag)
		{
			m_pFloatWidget->ui.btnMic->setIcon(QIcon(QString::fromUtf8(":/RtspClient/Resources/stopmic.png")));
		}
		else
		{	
			m_pFloatWidget->ui.btnMic->setIcon(QIcon(QString::fromUtf8(":/RtspClient/Resources/mic.png")));
		}
	}
#endif
}

void VideoWidget::slotContextMenu(QPoint pt)
{
	if (m_url.isEmpty())
	{
		return;
	}

	QMenu * popMenu = new QMenu();

	popMenu->addAction(m_actionCloseVideo);

	if (m_nRenderMode == RENDER_MODE_KEEP)
	{
		popMenu->addAction(m_actionFillWhole);
	}
	else
	{
		popMenu->addAction(m_actionKeepAspectRatio);
	}

	if (m_bFullScreen)
	{
		popMenu->addAction(m_actionResumeNormal);
	}
	else if (canMaximized())
	{
		popMenu->addAction(m_actionFullScreen);
	}

	popMenu->exec(pt);

	delete popMenu;
}

void VideoWidget::slotFloatWidget()
{
    if (NULL == m_pFloatWidget)
    {
        return;
    }
    
    QRect rect = m_pFloatWidget->geometry();

	if (!rect.contains(QCursor::pos()))
	{
		m_pFloatWidget->hide();
	}
}

bool VideoWidget::event(QEvent * event)
{
	QEvent::Type tp = event->type();

	switch (event->type())
	{
	case QEvent::Enter:
		showFloatWidget(true);
		break;

	case QEvent::Leave:
		showFloatWidget(false);
		break;

	case QEvent::Hide:
		showFloatWidget(false);
		break;

	case QEvent::Show:
		//showFloatWidget(true);
		break;
	}

	return QWidget::event(event);
}

QString VideoWidget::getUrlIp(QString &url)
{
    char* username = NULL;
	char* password = NULL;
	char* address = NULL;
	int   urlPortNum = 554;
	
    if (!CRtspClient::parse_url(url.toStdString().c_str(), username, password, address, urlPortNum, NULL))
    {
        return QString();
    }

    if (username)
	{
		delete[] username;
	}
	
	if (password)
	{
		delete[] password;
	}

	QString ip = address; 
	
	delete[] address; 

	return ip;
}



