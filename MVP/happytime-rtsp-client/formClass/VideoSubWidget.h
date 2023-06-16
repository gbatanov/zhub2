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

#ifndef VIDEO_SUB_WIDGET_H
#define VIDEO_SUB_WIDGET_H

#include <QWidget>


class VideoSubWidget : public QWidget
{
	Q_OBJECT

public:
	VideoSubWidget(QWidget *parent = 0, Qt::WindowFlags flags = 0);
	~VideoSubWidget();

    void setTips(QString tips);
    
signals:
	void widgetSelecting();
	void doubleClicked();
	void dragDroped(QString,QString,QString,QString,int,int);
	void contextMenu(QPoint);

protected:
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual void dropEvent(QDropEvent *event);
	virtual void paintEvent(QPaintEvent * event);
	virtual void mouseDoubleClickEvent(QMouseEvent * event);
	virtual void mousePressEvent(QMouseEvent *event);

private:
    QString m_tips;
};

#endif

