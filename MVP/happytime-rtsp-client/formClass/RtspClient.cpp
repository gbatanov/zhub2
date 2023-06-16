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

#include "RtspClient.h"
#include "CustomLayoutWidget.h"
#include "SystemSetting.h"
#include "utils.h"
#include <QDesktopServices>
#include <QXmlStreamReader>
#include <QTranslator>

/***************************************************************************************/

QTranslator   g_translator;

/***************************************************************************************/

RtspClient::RtspClient(QWidget *parent, Qt::WindowFlags flags)
: DialogTitleBar(parent, flags)
{
    loadSystemConfig();

    setLanguage(m_syscfg.sysLang);
    
	ui.setupUi(this);

	initDialog();

	connSignalSlot();

    if (m_syscfg.enableLog)
	{
		log_time_init("ipsee");
		log_set_level(m_syscfg.logLevel);
	}
	
#ifdef DEMO
	QDesktopServices::openUrl(QUrl("http://happytimesoft.com", QUrl::TolerantMode));
#endif	
}

void RtspClient::initDialog()
{
    setWindowTitle(QString("Happytime rtsp client V%1.%2").arg(MAIN_VERSION).arg(MAJOR_VERSION));
    
    loadVideoWindowLayout();
}

void RtspClient::setLanguage(int newLang)
{
    if (newLang == LANG_SYS)
    {
        if (QLocale::system().language() == QLocale::Chinese)
        {
            g_translator.load("rtspclient_zh.qm");
	        QCoreApplication::installTranslator(&g_translator);
        }
    }
    else if (newLang == LANG_ZH)
    {
        g_translator.load("rtspclient_zh.qm");
        QCoreApplication::installTranslator(&g_translator);
    }
}

void RtspClient::connSignalSlot()
{
	connect(ui.btnLayoutOne, SIGNAL(clicked()), this, SLOT(slotLayoutOne()));
    connect(ui.btnLayoutFour, SIGNAL(clicked()), this, SLOT(slotLayoutFour()));
    connect(ui.btnLayoutSix, SIGNAL(clicked()), this, SLOT(slotLayoutSix()));
    connect(ui.btnLayoutNine, SIGNAL(clicked()), this, SLOT(slotLayoutNine()));
    connect(ui.btnLayoutSixteen, SIGNAL(clicked()), this, SLOT(slotLayoutSixteen()));
    connect(ui.btnLayoutCustom, SIGNAL(clicked()), this, SLOT(slotLayoutCustom()));    
    connect(ui.btnFullScreen, SIGNAL(clicked()), this, SLOT(slotFullScreen()));
	connect(ui.btnStopAll, SIGNAL(clicked()), this, SLOT(slotStopAll()));
	connect(ui.btnSystemSetting, SIGNAL(clicked()), this, SLOT(slotSystemSetting()));
}

/* one window layout */
void RtspClient::slotLayoutOne()
{
    ui.widgetManager->setLayoutMode(LAYOUT_MODE_1);
}

/* four window layout */
void RtspClient::slotLayoutFour()
{
    ui.widgetManager->setLayoutMode(LAYOUT_MODE_4);
}

/* six window layout */
void RtspClient::slotLayoutSix()
{
    ui.widgetManager->setLayoutMode(LAYOUT_MODE_6);
}

/* nine window layout */
void RtspClient::slotLayoutNine()
{
    ui.widgetManager->setLayoutMode(LAYOUT_MODE_9);
}

/* sixteen window layout */
void RtspClient::slotLayoutSixteen()
{
    ui.widgetManager->setLayoutMode(LAYOUT_MODE_16);
}

/* custom window layout */
void RtspClient::slotLayoutCustom()
{
    CustomLayoutWidget dlg(this);

    if (QDialog::Accepted == dlg.exec())
    {
        Layout layout = dlg.getLayout();

        ui.widgetManager->setLayout(layout.rows, layout.cols, layout.zones);
    }
}

void RtspClient::slotFullScreen()
{
    ui.widgetManager->setWindowFlags(Qt::FramelessWindowHint);
    ui.widgetManager->setParent(0);
    ui.widgetManager->showMaximized();

    this->hide();
    this->grabKeyboard();
}

void RtspClient::slotStopAll()
{
	ui.widgetManager->closeAll();
}

void RtspClient::slotSystemSetting()
{
    SystemSetting dlg(m_syscfg, this);

	if (QDialog::Accepted == dlg.exec())
	{
		SysConfig config = dlg.getSysConfig();		

		/* apply system config parameter */

		if (m_syscfg.enableLog != config.enableLog)
		{
			if (m_syscfg.enableLog)
			{
				log_close();
			}
			else
			{
				log_time_init("ipsee");
			}
		}

		log_set_level(config.logLevel);

		/* update system config parameter */
		m_syscfg.enableLog = config.enableLog;
		m_syscfg.recordPath = config.recordPath;
		m_syscfg.snapshotPath = config.snapshotPath;
		m_syscfg.videoRenderMode = config.videoRenderMode;
		m_syscfg.rtpMulticast = config.rtpMulticast;
		m_syscfg.rtpOverUdp = config.rtpOverUdp;
		m_syscfg.rtspOverHttp = config.rtspOverHttp;
		m_syscfg.rtspOverHttpPort = config.rtspOverHttpPort;
		m_syscfg.logLevel = config.logLevel;
		m_syscfg.sysLang = config.sysLang;
	}
}

void RtspClient::keyPressEvent(QKeyEvent * event)
{
    if (event->key() == Qt::Key_Escape)
    {
        if (ui.widgetManager->isMaximized())
        {
            resumeVideoWidget();
        }
    }
}

void RtspClient::closeEvent(QCloseEvent * event)
{
	saveVideoWindowLayout();

    ui.widgetManager->closeAll();
	
	event->accept();
}

void RtspClient::resumeVideoWidget()
{
    ui.widgetManager->showNormal();
    ui.widgetManager->setParent(this);
    this->layout()->removeItem(ui.layoutLayout);
    this->layout()->addWidget(ui.widgetManager);
    this->layout()->addItem(ui.layoutLayout);

    this->show();
    releaseKeyboard();
}

/* load video window layout mode */
void RtspClient::loadVideoWindowLayout()
{
	QXmlStreamReader xml;    

    QFile file(QCoreApplication::applicationDirPath() + "/layout.xml");
    if (!file.open(QFile::ReadOnly | QFile::Text))
    {        
        return;
    }
    
    xml.setDevice(&file);

    if (!xml.readNextStartElement())
    {
        return;
    }

	if (xml.name() != "Layout")
    {
        return;
    }

    int rows = xml.attributes().value("rows").toString().toInt();
    int cols = xml.attributes().value("cols").toString().toInt();
	LayoutZone zone;
    LayoutZoneList zones;

    if (rows < 0 || rows >= 64 || cols < 0 || cols >= 64)
    {
    	return;
    }

	while (xml.readNextStartElement())
	{
		if (xml.name() != "zone")
		{
			xml.skipCurrentElement();
			continue;
		}

		zone.left = xml.attributes().value("left").toString().toInt();
		zone.right = xml.attributes().value("right").toString().toInt();
		zone.top = xml.attributes().value("top").toString().toInt();
		zone.bottom = xml.attributes().value("bottom").toString().toInt();

		if (zone.left < 0 || zone.left >= cols || zone.right < 0 || zone.right >= cols || zone.left > zone.right || 
			zone.top < 0 || zone.top >= rows || zone.bottom < 0 || zone.bottom >= rows || zone.top > zone.bottom)
		{
			xml.skipCurrentElement();
			continue;
		}

		zone.url = xml.attributes().value("url").toString();
		zone.acct = xml.attributes().value("acct").toString();
		zone.pass = xml.attributes().value("pass").toString();
        
        zones.push_back(zone);

        xml.skipCurrentElement();
	}

	if (zones.size() > 0)
	{
		ui.widgetManager->setLayout(rows, cols, zones);
	}
}

/* save video window layout mode */
void RtspClient::saveVideoWindowLayout()
{
	QFile file(QCoreApplication::applicationDirPath() + "/layout.xml");
    if (!file.open(QFile::WriteOnly | QFile::Text))
    {        
        return;
    }

	int rows, cols;
	LayoutZoneList zones;
	QXmlStreamWriter xml;
	
	ui.widgetManager->getLayout(rows, cols, zones);

    xml.setAutoFormatting(true);
    xml.setDevice(&file);

	xml.writeStartDocument();

    xml.writeStartElement("Layout");
    xml.writeAttribute("rows", QString("%1").arg(rows));
    xml.writeAttribute("cols", QString("%1").arg(cols));

    for (int i = 0; i < zones.size(); i++)
    {		
        xml.writeStartElement("zone");

		xml.writeAttribute("left", QString("%1").arg(zones[i].left));
		xml.writeAttribute("right", QString("%1").arg(zones[i].right));
		xml.writeAttribute("top", QString("%1").arg(zones[i].top));
		xml.writeAttribute("bottom", QString("%1").arg(zones[i].bottom));

		QString url;
		QString acct;
		QString pass;
		
		VideoWidget * pVideoWidget = ui.widgetManager->getWidget(i);
		if (pVideoWidget)
		{
			url = pVideoWidget->getUrl();
			acct = pVideoWidget->getAcct();
			pass = pVideoWidget->getPass();
		}
		
		xml.writeAttribute("url", QString("%1").arg(url));
		xml.writeAttribute("acct", QString("%1").arg(acct));
		xml.writeAttribute("pass", QString("%1").arg(pass));
		
		xml.writeEndElement();
    }

	xml.writeEndElement();
    
    xml.writeEndDocument();
}

/* load system config information */
void RtspClient::loadSystemConfig()
{
	m_syscfg.snapshotPath = getSnapshotPath();
	m_syscfg.recordPath = getRecordPath();
	m_syscfg.enableLog = getEnableLogFlag();
	m_syscfg.videoRenderMode = getVideoRenderMode();
	m_syscfg.rtpMulticast = getRtpMulticast();
	m_syscfg.rtpOverUdp = getRtpOverUdp();
	m_syscfg.rtspOverHttp = getRtspOverHttp();
	m_syscfg.rtspOverHttpPort = getRtspOverHttpPort();
	m_syscfg.logLevel = getLogLevel();
	m_syscfg.sysLang = getSysLang();
}


