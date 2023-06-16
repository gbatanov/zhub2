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
#include "utils.h"
#include <QFile>
#include <QCoreApplication>
#include <QSettings>
#include <QDir>
#include <QDateTime>
#include "config.h"


QString getSnapshotPath()
{
	QSettings setting;

    QString path = setting.value("snapshotPath").toString();
    if (path.isEmpty() || !QFile::exists(path))
    {
        path = QCoreApplication::applicationDirPath() + "/snapshot";
        if (!QFile::exists(path))
    	{
    	    QDir temppath(path);
    		temppath.mkdir(path);
    	}
    }

    return path;
}

QString getRecordPath()
{
	QSettings setting;

    QString path = setting.value("recordPath").toString();
    if (path.isEmpty() || !QFile::exists(path))
    {
        path = QCoreApplication::applicationDirPath() + "/record";
        if (!QFile::exists(path))
    	{
    	    QDir temppath(path);
    		temppath.mkdir(path);
    	}
    }

    return path;
}

BOOL getEnableLogFlag()
{
	QSettings setting;

    return setting.value("enableLog", 1).toInt();
}

int getVideoRenderMode()
{
	QSettings setting;

    return setting.value("videoRenderMode", 0).toInt();
}

BOOL getRtpMulticast()
{
	QSettings setting;

    return setting.value("rtpMulticast", 0).toInt();
}

BOOL getRtpOverUdp()
{
    QSettings setting;

    return setting.value("rtpOverUdp", 0).toInt();
}

BOOL getRtspOverHttp()
{
    QSettings setting;

    return setting.value("rtspOverHttp", 0).toInt();
}

int getRtspOverHttpPort()
{
    QSettings setting;

    return setting.value("rtspOverHttpPort", 80).toInt();
}

int getLogLevel()
{
    QSettings setting;

    return setting.value("logLevel", 2).toInt();
}

int getSysLang()
{
    QSettings setting;

    return setting.value("sysLang", 0).toInt();
}

QString getTempFile(QString prefix, QString extName)
{
	QDateTime tm = QDateTime::currentDateTime();
	QString time;
	QString fileName;

	time.sprintf("%04d%02d%02d_%02d%02d%02d",  
		tm.date().year(), tm.date().month(), tm.date().day(),
		tm.time().hour(), tm.time().minute(), tm.time().second()); 
		
	fileName = prefix + "_" + time + extName; 

	return fileName;
}

void saveSnapshotPath(QString path)
{
    QSettings setting;

    setting.setValue("snapshotPath", path);
}

void saveRecordPath(QString path)
{
    QSettings setting;

    setting.setValue("recordPath", path);
}

void saveEnableLogFlag(BOOL flag)
{
    QSettings setting;

    setting.setValue("enableLog", flag);
}

void saveVideoRenderMode(int mode)
{
	QSettings setting;

    setting.setValue("videoRenderMode", mode);
}

void saveRtpMulticast(BOOL flag)
{
	QSettings setting;

    setting.setValue("rtpMulticast", flag);
}

void saveRtpOverUdp(BOOL flag)
{
    QSettings setting;

    setting.setValue("rtpOverUdp", flag);
}

void saveRtspOverHttp(BOOL flag)
{
    QSettings setting;

    setting.setValue("rtspOverHttp", flag);
}

void saveRtspOverHttpPort(int port)
{
    QSettings setting;

    setting.setValue("rtspOverHttpPort", port);
}

void saveLogLevel(int level)
{
	QSettings setting;

    setting.setValue("logLevel", level);
}

void saveSysLang(int lang)
{
    QSettings setting;

    setting.setValue("sysLang", lang);
}



