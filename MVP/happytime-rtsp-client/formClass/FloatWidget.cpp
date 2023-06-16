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
#include "FloatWidget.h"
#include <QResizeEvent>
#include <QMessageBox>


FloatWidget::FloatWidget(QWidget *parent, Qt::WindowFlags flags)
: QWidget(parent, flags | Qt::FramelessWindowHint | Qt::Tool)
{
	ui.setupUi(this);

	initWidget();
}

FloatWidget::~FloatWidget()
{

}

void FloatWidget::initWidget()
{
	setAttribute(Qt::WA_TranslucentBackground);
	setAttribute(Qt::WA_TransparentForMouseEvents);

	QObject::connect(ui.btnPlay, SIGNAL(clicked()), this, SIGNAL(play()));
	QObject::connect(ui.btnStop, SIGNAL(clicked()), this, SIGNAL(stop()));
	QObject::connect(ui.btnMic, SIGNAL(clicked()), this, SIGNAL(micphone()));
	QObject::connect(ui.btnSnapshot, SIGNAL(clicked()), this, SIGNAL(snapshot()));
	QObject::connect(ui.btnRecord, SIGNAL(clicked()), this, SIGNAL(record()));
	QObject::connect(ui.btnVolume, SIGNAL(clicked()), this, SIGNAL(volume()));
	QObject::connect(ui.sliderVolume, SIGNAL(actionTriggered(int)), this, SIGNAL(volumeChanged(int)));
	
}







