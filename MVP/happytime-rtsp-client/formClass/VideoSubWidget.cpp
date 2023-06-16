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
#include "VideoSubWidget.h"
#include <QPainter>
#include <QMimeData>
#include <QDragEnterEvent>


VideoSubWidget::VideoSubWidget(QWidget *parent, Qt::WindowFlags flags)
	: QWidget(parent, flags)
{
	setAcceptDrops(true);
	setAutoFillBackground(false);
	setContextMenuPolicy(Qt::CustomContextMenu);
}


VideoSubWidget::~VideoSubWidget()
{
	
}

void VideoSubWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat("text/plain"))
	{
		event->acceptProposedAction();
	}
	else
	{
		event->ignore();
	}
}

void VideoSubWidget::dropEvent(QDropEvent *event)
{
	QString mimeData = QString::fromLocal8Bit(event->mimeData()->data("text/plain"));

	if (!mimeData.startsWith("url="))
	{
		return;
	}

	int acctPos = mimeData.indexOf(";acct=");
	int passPos = mimeData.indexOf(";pass=");
	int ipPos = mimeData.indexOf(";ip=");
	int portPos = mimeData.indexOf(";port=");
	int bcflagPos = mimeData.indexOf(";bcflag=");

	if (acctPos == -1 || passPos == -1 || ipPos == -1 || portPos == -1)
	{
		return;
	}

    QString url = mimeData.mid(4, acctPos-4);
    QString acct = mimeData.mid(acctPos+6, passPos-acctPos-6);
    QString pass = mimeData.mid(passPos+6, ipPos-passPos-6);	
    QString ip = mimeData.mid(ipPos+4, portPos-ipPos-4);    

	int port = mimeData.mid(portPos+6, bcflagPos-portPos-6).toInt();
	int bcFlag = mimeData.mid(bcflagPos+8).toInt();

	event->acceptProposedAction();

	emit dragDroped(url, acct, pass, ip, port, bcFlag);
}


void VideoSubWidget::paintEvent(QPaintEvent * event)
{
	QPainter painter(this);

	painter.setClipRegion(event->rect());

	painter.fillRect(rect(), Qt::SolidPattern);

	if (!m_tips.isEmpty())
	{
		QPen pen(Qt::red, 5);
		QRect rect = this->rect();
		
	    painter.setPen(pen);		
		painter.drawText(QRect(rect.left(), rect.top()+rect.height()/2-10, rect.width(), 20), Qt::AlignCenter, m_tips);
	}
}

void VideoSubWidget::mouseDoubleClickEvent(QMouseEvent * event)
{
	emit doubleClicked();
}

void VideoSubWidget::mousePressEvent(QMouseEvent *event)
{
	emit widgetSelecting();

	if (event->button() == Qt::RightButton)
	{
		emit contextMenu(event->globalPos());
	}
}

void VideoSubWidget::setTips(QString tips)
{
	m_tips = tips;

	update();
}




