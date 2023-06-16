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
#include "utils.h"
#include "WidgetManager.h"
#include <QPainter>
#include <QPaintEvent>


WidgetManager::WidgetManager(QWidget *parent, Qt::WindowFlags flags)
: QWidget(parent, flags)
{
    initWidgetManager();

    setAutoFillBackground(false);
}


WidgetManager::~WidgetManager()
{
}


void WidgetManager::initWidgetManager()
{    
    setLayoutMode(LAYOUT_MODE_4);
}


void WidgetManager::paintEvent(QPaintEvent * event)
{
    QPainter painter(this);
}

void WidgetManager::resizeEvent(QResizeEvent * event)
{
    resizeVideoWidgets(event->size().width(), event->size().height());
}

void WidgetManager::closeEvent(QCloseEvent *event)
{
	QWidget::closeEvent(event);
}

void WidgetManager::resizeVideoWidgets(int w, int h)
{
    QRect rect;
    
    for (int i = 0; i < m_WidgetLayout.zones.size(); ++i)
    {
        WidgetZone & widgetZone = m_WidgetLayout.zones[i];
        
        rect = getZoneRect(w, h, widgetZone.zone);

        widgetZone.widget->setGeometry(rect);
    }
}

void WidgetManager::setLayoutMode(int layoutMode)
{
    LayoutZoneList zones;
    
    switch (layoutMode)
    {
    case LAYOUT_MODE_1:
        addZone(zones, 0, 0, 0, 0);
        
        setLayout(1, 1, zones);
        break;

    case LAYOUT_MODE_4:
        addZone(zones, 0, 0, 0, 0);
        addZone(zones, 1, 1, 0, 0);
        addZone(zones, 0, 0, 1, 1);
        addZone(zones, 1, 1, 1, 1);
        
        setLayout(2, 2, zones);
        break;

    case LAYOUT_MODE_6:
        addZone(zones, 0, 1, 0, 1);
        addZone(zones, 2, 2, 0, 0);
        addZone(zones, 2, 2, 1, 1);
        addZone(zones, 0, 0, 2, 2);
        addZone(zones, 1, 1, 2, 2);
        addZone(zones, 2, 2, 2, 2);
        
        setLayout(3, 3, zones);
        break;

    case LAYOUT_MODE_8:
        addZone(zones, 0, 2, 0, 2);
        addZone(zones, 3, 3, 0, 0);
        addZone(zones, 3, 3, 1, 1);
        addZone(zones, 3, 3, 2, 2);
        addZone(zones, 0, 0, 3, 3);
        addZone(zones, 1, 1, 3, 3);
        addZone(zones, 2, 2, 3, 3);
        addZone(zones, 3, 3, 3, 3);
        
        setLayout(4, 4, zones);
        break;    

    case LAYOUT_MODE_9:
        addZone(zones, 0, 0, 0, 0);
        addZone(zones, 1, 1, 0, 0);
        addZone(zones, 2, 2, 0, 0);
        addZone(zones, 0, 0, 1, 1);
        addZone(zones, 1, 1, 1, 1);
        addZone(zones, 2, 2, 1, 1);
        addZone(zones, 0, 0, 2, 2);
        addZone(zones, 1, 1, 2, 2);
        addZone(zones, 2, 2, 2, 2);
        
        setLayout(3, 3, zones);
        break; 

    case LAYOUT_MODE_16:
        addZone(zones, 0, 0, 0, 0);
        addZone(zones, 1, 1, 0, 0);
        addZone(zones, 2, 2, 0, 0);
        addZone(zones, 3, 3, 0, 0);
        addZone(zones, 0, 0, 1, 1);
        addZone(zones, 1, 1, 1, 1);
        addZone(zones, 2, 2, 1, 1);
        addZone(zones, 3, 3, 1, 1);
        addZone(zones, 0, 0, 2, 2);
        addZone(zones, 1, 1, 2, 2);
        addZone(zones, 2, 2, 2, 2);
        addZone(zones, 3, 3, 2, 2);
        addZone(zones, 0, 0, 3, 3);
        addZone(zones, 1, 1, 3, 3);
        addZone(zones, 2, 2, 3, 3);
        addZone(zones, 3, 3, 3, 3);
        
        setLayout(4, 4, zones);
        break;

	case LAYOUT_MODE_25:
		addZone(zones, 0, 0, 0, 0);
		addZone(zones, 1, 1, 0, 0);
		addZone(zones, 2, 2, 0, 0);
		addZone(zones, 3, 3, 0, 0);
		addZone(zones, 4, 4, 0, 0);
		addZone(zones, 0, 0, 1, 1);
		addZone(zones, 1, 1, 1, 1);
		addZone(zones, 2, 2, 1, 1);
		addZone(zones, 3, 3, 1, 1);
		addZone(zones, 4, 4, 1, 1);
		addZone(zones, 0, 0, 2, 2);
		addZone(zones, 1, 1, 2, 2);
		addZone(zones, 2, 2, 2, 2);
		addZone(zones, 3, 3, 2, 2);
		addZone(zones, 4, 4, 2, 2);
		addZone(zones, 0, 0, 3, 3);
		addZone(zones, 1, 1, 3, 3);
		addZone(zones, 2, 2, 3, 3);
		addZone(zones, 3, 3, 3, 3);
		addZone(zones, 4, 4, 3, 3);
		addZone(zones, 0, 0, 4, 4);
		addZone(zones, 1, 1, 4, 4);
		addZone(zones, 2, 2, 4, 4);
		addZone(zones, 3, 3, 4, 4);
		addZone(zones, 4, 4, 4, 4);

		setLayout(5, 5, zones);
		break;
    }
}


void WidgetManager::addZone(LayoutZoneList & zones, int l, int r, int t, int b)
{
    LayoutZone zone;
    
    zone.left = l;
    zone.right = r;
    zone.top = t;
    zone.bottom = b;

    zones.push_back(zone);
}


void WidgetManager::setLayout(int rows, int cols, LayoutZoneList & zones, bool syncFlag)
{
    m_WidgetLayout.rows = rows;
    m_WidgetLayout.cols = cols;

    int i, j, index = 0;
    WidgetZone widgetZone;
    VideoWidget * widget = 0;
        
    for (i = 0; i < m_WidgetLayout.zones.size(); ++i)
    {
        widgetZone = m_WidgetLayout.zones[i];
            
        if (widgetZone.widget->getUrl().isEmpty())
        {
            delete widgetZone.widget;
        }
        else
        {
            widgetZone.widget->hide();
            m_VideoWidgets.insert(index++, widgetZone.widget);
        }
    }
    
    m_WidgetLayout.zones.clear();
    
    for (i = 0; i < zones.size(); ++i)
    {
        widget = NULL;
        
        widgetZone.zone.left = zones[i].left;
        widgetZone.zone.right = zones[i].right;
        widgetZone.zone.top = zones[i].top;
        widgetZone.zone.bottom = zones[i].bottom;
		widgetZone.zone.url = zones[i].url;
        widgetZone.zone.acct = zones[i].acct;
        widgetZone.zone.pass = zones[i].pass;

        if (!zones[i].url.isEmpty())
        {
            for (j = 0; j < m_VideoWidgets.size(); ++j)
            {
                if (m_VideoWidgets[j]->getUrl() == zones[i].url)
                {
                    widget = m_VideoWidgets.takeAt(j);
                    break;
                }
            }
        }

        if (!syncFlag && !widget && !m_VideoWidgets.isEmpty())
        {
            widget = m_VideoWidgets.takeAt(0);
        }
        
        initVideoWidget(widget, widgetZone);
        
        m_WidgetLayout.zones.push_back(widgetZone);
    }

    if (syncFlag)
    {
        for (i = 0; i < m_VideoWidgets.size(); ++i)
        {
            widget = m_VideoWidgets[i];
            delete widget;
        }

        m_VideoWidgets.clear();
    }
    
    update();
}

void WidgetManager::getLayout(int & rows, int & cols, LayoutZoneList & zones)
{
	rows = m_WidgetLayout.rows;
	cols = m_WidgetLayout.cols;

	zones.clear();
	
	for (int i = 0; i < m_WidgetLayout.zones.size(); i++)
	{
		zones.push_back(m_WidgetLayout.zones[i].zone);
	}
}

void WidgetManager::initVideoWidget(VideoWidget * widget, WidgetZone & widgetZone)
{
    if (widget)
    {
        widgetZone.widget = widget;
    }
    else
    {
        widgetZone.widget = new VideoWidget(this);

        widgetZone.widget->setRenderMode(getVideoRenderMode());	
        widgetZone.widget->setDevInfo(widgetZone.zone.url, 
			widgetZone.zone.acct, widgetZone.zone.pass);

        connect(widgetZone.widget, SIGNAL(widgetSelecting(QWidget*)), this, SLOT(slotWidgetSelecting(QWidget*)));
    }
    
    setVideoWidgetRect(widgetZone);

    widgetZone.widget->show();
}

void WidgetManager::setVideoWidgetRect(WidgetZone & widgetZone)
{
    QRect rect = getZoneRect(width(), height(), widgetZone.zone);

    widgetZone.widget->setGeometry(rect);
}


QRect WidgetManager::getZoneRect(int w, int h, LayoutZone & zone)
{
    return getGridRect(w, h, zone.left, zone.right, zone.top, zone.bottom);
}

QRect WidgetManager::getGridRect(int w, int h, int left, int right, int top, int bottom)
{
    float gridWidth = w / (float) m_WidgetLayout.cols;
    float gridHeight = h / (float) m_WidgetLayout.rows;
    
    int x1 = left * gridWidth;
    int y1 = top * gridHeight;

    int x2 = (right + 1) * gridWidth;
    int y2 = (bottom + 1) * gridHeight;

    return QRect(x1, y1, x2 - x1, y2 - y1);
}

VideoWidget * WidgetManager::getSelectedWidget()
{
    VideoWidget * pSelectedWidget = NULL;
    
    for (int i = 0; i < m_WidgetLayout.zones.size(); ++i)
    {
        WidgetZone & widgetZone = m_WidgetLayout.zones[i];
        
        if (widgetZone.widget->isSelected())
        {
            pSelectedWidget = widgetZone.widget;
            break;
        }
    }

    return pSelectedWidget;
}

VideoWidget * WidgetManager::getIdleWidget()
{
    VideoWidget * pIdleWidget = NULL;
    
    for (int i = 0; i < m_WidgetLayout.zones.size(); ++i)
    {
        WidgetZone & widgetZone = m_WidgetLayout.zones[i];
        
        if (widgetZone.widget->getUrl().isEmpty())
        {
            pIdleWidget = widgetZone.widget;
            break;
        }
    }

    return pIdleWidget;
}

VideoWidget * WidgetManager::getWidget(int index)
{
    if (index < 0 || index >= m_WidgetLayout.zones.size())
    {
    	return NULL;
    }

    return m_WidgetLayout.zones[index].widget;
}

int	WidgetManager::getWidgetNums()
{
	return m_WidgetLayout.zones.size();
}

void WidgetManager::slotWidgetSelecting(QWidget * pWidget)
{
    VideoWidget * pSelectedWidget = getSelectedWidget();
    if (pSelectedWidget != pWidget)
    {
        if (pSelectedWidget)
        {
            pSelectedWidget->setSelected(false);
        }

        pSelectedWidget = (VideoWidget *) pWidget;
        pSelectedWidget->setSelected(true);
    }
}

void WidgetManager::closeAll()
{
	for (int i = 0; i < m_WidgetLayout.zones.size(); ++i)
	{
		WidgetZone & widgetZone = m_WidgetLayout.zones[i];

		if (!widgetZone.widget->getUrl().isEmpty())
		{
			widgetZone.widget->slotCloseVideo();
		}
	}
}






