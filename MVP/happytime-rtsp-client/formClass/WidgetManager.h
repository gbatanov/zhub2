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

#ifndef _VIDEO_WIDGET_MANAGER_H_
#define _VIDEO_WIDGET_MANAGER_H_


#include <QWidget>
#include "VideoWidget.h"
#include "Layout.h"


typedef struct
{
    LayoutZone    zone;
    VideoWidget * widget;
} WidgetZone;

typedef QList<WidgetZone> WidgetZoneList;

typedef struct 
{
    int rows;
    int cols;
    WidgetZoneList zones;
} WidgetLayout;


typedef QList<VideoWidget *> VideoWidgetList;


class WidgetManager : public QWidget
{
    Q_OBJECT

public:
	WidgetManager(QWidget *parent = 0, Qt::WindowFlags flags = 0);
    ~WidgetManager();

    void    setLayoutMode(int layoutMode);
    void    setLayout(int rows, int cols, LayoutZoneList & zones, bool syncFlag = false);
    void	getLayout(int & rows, int & cols, LayoutZoneList & zones);
	int		getWidgetNums();
	void	closeAll();
	
    VideoWidget * getSelectedWidget();
    VideoWidget * getIdleWidget();   
    VideoWidget * getWidget(int index);

public slots:
    void    slotWidgetSelecting(QWidget * pWidget);
    
protected:
    void    paintEvent(QPaintEvent * event);
    void 	resizeEvent(QResizeEvent * event);
	void	closeEvent(QCloseEvent *event);

private:
    void    drawGrid(QPainter & painter, int rows, int cols);
    void    initWidgetManager();
    void    addZone(LayoutZoneList & zones, int l, int r, int t, int b);
    QRect   getGridRect(int w, int h, int left, int right, int top, int bottom);
    QRect   getZoneRect(int w, int h, LayoutZone & zone);
    void    setVideoWidgetRect(WidgetZone & widgetZone);
    void    resizeVideoWidgets(int w, int h);
    void    initVideoWidget(VideoWidget * widget, WidgetZone & widgetZone);
    
private:
    WidgetLayout    m_WidgetLayout;
    VideoWidgetList m_VideoWidgets;
};

#endif 


