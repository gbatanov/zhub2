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

#ifndef SET_RTSP_INFO_H
#define SET_RTSP_INFO_H

#include <QDialog>
#include "ui_SetRtspInfo.h"
#include "dialogtitle.h"

class SetRtspInfo : public DialogTitleBar
{
    Q_OBJECT

public:
	SetRtspInfo(QString &url, QString &user, QString &pass, QWidget *parent = 0, Qt::WindowFlags flags = 0);
    ~SetRtspInfo();

    QString getUrl() { return m_url; }
    QString getUser() { return m_user; }
    QString getPass() { return m_pass; }

public slots:
    void slotConfirm();
    
private:
    Ui::SetRtspInfo ui;
    QString m_url;
    QString m_user;
    QString m_pass;
};

#endif

