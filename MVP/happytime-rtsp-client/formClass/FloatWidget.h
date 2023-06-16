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

#pragma once

#include <QWidget>
#include "ui_FloatWidget.h"


class FloatWidget : public QWidget
{
	Q_OBJECT

public:
	FloatWidget(QWidget *parent = NULL, Qt::WindowFlags flags = 0);
	~FloatWidget();
    
signals:
	void play();
	void pause();
	void stop();
	void micphone();
	void snapshot();
	void record();
	void volume();
	void volumeChanged(int);

private:
	void initWidget();

public:
	Ui::FloatWidget ui;
	QString         m_ip;
	int             m_port;
};


