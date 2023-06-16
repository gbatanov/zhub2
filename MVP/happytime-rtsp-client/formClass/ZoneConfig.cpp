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
#include "ZoneConfig.h"
#include <QPainter>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMenu>
#include <QMimeData>


#define GRID_LINE_WIDTH     1
#define ZONE_COLOR          QColor(0, 0, 32)
#define GRID_LINE_COLOR     QColor(64, 64, 64)


ZoneConfig::ZoneConfig(QWidget *parent)
: QWidget(parent)
, m_mouseMoving(false)
, m_gridRows(DEF_GRID_ROWS)
, m_gridCols(DEF_GRID_COLS)
{
    initZoneConfig();
    
    initActions(); 
}


ZoneConfig::~ZoneConfig()
{    
}


void ZoneConfig::initZoneConfig()
{
    setMouseTracking(true);
    setAcceptDrops(true);

    setContextMenuPolicy(Qt::DefaultContextMenu);
    
    initGridFlag();
}


void ZoneConfig::initGridFlag()
{
    for (int i = 0; i < m_gridCols; i++)
    {
        for (int j = 0; j < m_gridRows; j++)
        {
            m_gridFlag[i][j] = false;
        }
    }
}

void ZoneConfig::initActions()
{
    m_actionDelZone = new QAction(this);
	m_actionDelZone->setText(QString::fromLocal8Bit("Del Zone"));

    m_actionClearZone = new QAction(this);
	m_actionClearZone->setText(QString::fromLocal8Bit("Clear Zones"));
	
    connect(m_actionDelZone, SIGNAL(triggered()), this, SLOT(slotDelZone()));
    connect(m_actionClearZone, SIGNAL(triggered()), this, SLOT(slotClearZone()));
}


void ZoneConfig::paintEvent(QPaintEvent * event)
{
    QPainter painter(this);

	painter.fillRect(rect(), Qt::SolidPattern);
    
	// draw grid
    drawGrid(painter, m_gridRows, m_gridCols);
    
    drawZone(painter);

    if (m_mouseMoving)
    {
        drawDragingZone(painter);
    }
}


void ZoneConfig::drawGrid(QPainter & painter, int rows, int cols)
{
    int x, y, x1, y1, i;
    int w = width();
    int h = height();

    float gridWidth = width() / (float) cols;
    float gridHeight = height() / (float) rows;
    
    painter.setPen(QPen(GRID_LINE_COLOR, GRID_LINE_WIDTH, Qt::DotLine));
    
    for (i = 0; i < cols; i++)
    {
        x = x1 = i * gridWidth;
        y = 0;
        y1 = h;

        painter.drawLine(x, y, x1, y1);
    }

    painter.drawLine(w-1, 0, w-1, h);
    
    for (i = 0; i < rows; i++)
    {
        x = 0;
        y = y1 = i * gridHeight;
        x1 = w;
        
        painter.drawLine(x, y, x1, y1);
    }

    painter.drawLine(0, h-1, w, h-1);
}


void ZoneConfig::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat("text/plain"))
	{
		event->acceptProposedAction();
	}	
}

void ZoneConfig::dropEvent(QDropEvent *event)
{
    event->acceptProposedAction();
    
    LayoutZone * zone = getZone(event->pos());   
    if (zone)
    {
	    QString mimeData = event->mimeData()->data("text/plain");

	    zone->url = mimeData;

	    update();
    }
}


void ZoneConfig::contextMenuEvent(QContextMenuEvent * event)
{    
    QMenu * pMenu = 0;
    
    if (getZone(event->pos()))
    {
        pMenu = new QMenu(this);

        pMenu->addAction(m_actionDelZone);
        pMenu->addAction(m_actionClearZone);
    }
    else if (m_zones.size() > 0)
    {
        pMenu = new QMenu(this);

        pMenu->addAction(m_actionClearZone);
    }

    m_ContextMenuPos = event->pos();
    
    if (pMenu)
    {
        pMenu->exec(event->globalPos());
	    delete pMenu;
    }
}


void ZoneConfig::mouseMoveEvent(QMouseEvent * event)
{    
    if (!(event->buttons() & Qt::LeftButton))
    {
        return;
    }
    
    if (m_pressPos.isNull() || !isValidPos(m_pressPos) || !isValidPos(event->pos()))
    {
        return;
    }

    int left, right, top, bottom;
    
    getDragingGrids(m_pressPos, event->pos(), left, right, top, bottom);

    if (!isValidGridRect(left, right, top, bottom))
    {
        return;
    }
        
    m_curPos = event->pos();
    m_mouseMoving = true;

    update();
}


void ZoneConfig::mousePressEvent(QMouseEvent * event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        m_pressPos = event->pos();
    }
    else
    {
        m_pressPos.setX(0);
        m_pressPos.setY(0);
    }
}

void ZoneConfig::mouseReleaseEvent(QMouseEvent * event)
{
    if (m_mouseMoving)
    {
        addZone();
        m_mouseMoving = false;	        
    }
}

void ZoneConfig::getDragingGrids(QPoint pos1, QPoint pos2, int &left, int &right, int &top, int &bottom)
{
    float gridWidth = width() / (float) m_gridCols;
    float gridHeight = height() / (float) m_gridRows;

    if (pos1.x() < pos2.x())
    {
        left = pos1.x() / gridWidth;
        right = pos2.x() / gridWidth;
    }
    else
    {
        left = pos2.x() / gridWidth;
        right = pos1.x() / gridWidth;
    }

    if (pos1.y() < pos2.y())
    {
        top = pos1.y() / gridHeight;
        bottom = pos2.y() / gridHeight;
    }
    else
    {
        top = pos2.y() / gridHeight;
        bottom = pos1.y() / gridHeight;
    }
}


QRect ZoneConfig::getGridRect(int left, int right, int top, int bottom)
{
    float gridWidth = width() / (float) m_gridCols;
    float gridHeight = height() / (float) m_gridRows;
    
    int x1 = left * gridWidth + GRID_LINE_WIDTH;
    int y1 = top * gridHeight + GRID_LINE_WIDTH;

    int x2 = (right + 1) * gridWidth - GRID_LINE_WIDTH;
    int y2 = (bottom + 1) * gridHeight - GRID_LINE_WIDTH;

    return QRect(x1, y1, x2 - x1, y2 - y1);
}


void ZoneConfig::drawDragingZone(QPainter & painter)
{
    int left, right, top, bottom;
    
    getDragingGrids(m_pressPos, m_curPos, left, right, top, bottom);
    
    painter.fillRect(getGridRect(left, right, top, bottom), ZONE_COLOR);
}

void ZoneConfig::setGridFlag(int left, int right, int top, int bottom, bool flag)
{
    for (int i = left; i <= right; i++)
    {
        for (int j = top; j <= bottom; j++)
        {
            m_gridFlag[i][j] = flag;
        }        
    }
}


void ZoneConfig::addZone()
{
    LayoutZone zone;
    
    getDragingGrids(m_pressPos, m_curPos, zone.left, zone.right, zone.top, zone.bottom);

    setGridFlag(zone.left, zone.right, zone.top, zone.bottom, true);
    
    m_zones.push_back(zone);
}


void ZoneConfig::drawZone(QPainter & painter)
{
	QRect rect;

    for (int i = 0; i < m_zones.size(); i++)
    {
        const LayoutZone & zone = m_zones.at(i);
        
		rect = getGridRect(zone.left, zone.right, zone.top, zone.bottom);

        painter.fillRect(rect, ZONE_COLOR);

        if (!zone.url.isEmpty())
        {
            painter.drawText(rect, Qt::AlignCenter, zone.url);
        }
    }
}


bool ZoneConfig::isValidPos(QPoint pos)
{
	QRect rect;

    for (int i = 0; i < m_zones.size(); i++)
    {
        const LayoutZone & zone = m_zones.at(i);

		rect = getGridRect(zone.left, zone.right, zone.top, zone.bottom);

        if (rect.adjusted(-GRID_LINE_WIDTH, -GRID_LINE_WIDTH, GRID_LINE_WIDTH, GRID_LINE_WIDTH).contains(pos))
        {
            return false;
        }
    }

    return true;
}

bool ZoneConfig::isValidGridRect(int left, int right, int top, int bottom)
{
    for (int i = left; i <= right; i++)
    {
        for (int j = top; j <= bottom; j++)
        {
            if (m_gridFlag[i][j])
            {
                return false;
            }
        }        
    } 

    return true;
}

void ZoneConfig::delZone(LayoutZone * zone)
{
    setGridFlag(zone->left, zone->right, zone->top, zone->bottom, false);

    for (int i = 0; i < m_zones.size(); i++)
    {
        LayoutZone & layoutZone = m_zones[i];

        if (zone == &layoutZone)
        {
            m_zones.removeAt(i);
            break;
        }
    }
}

void ZoneConfig::slotDelZone()
{
    LayoutZone * zone = getZone(m_ContextMenuPos);
    if (zone)
    {
        delZone(zone);

        update();
    }
}

void ZoneConfig::slotClearZone()
{
    m_zones.clear();

    initGridFlag();
    
    update();
}


LayoutZone * ZoneConfig::getZone(QPoint pos)
{
    QRect rect;
    
    for (int i = 0; i < m_zones.size(); i++)
    {
        LayoutZone & zone = m_zones[i];

        rect = getGridRect(zone.left, zone.right, zone.top, zone.bottom);
        if (rect.adjusted(-GRID_LINE_WIDTH, -GRID_LINE_WIDTH, GRID_LINE_WIDTH, GRID_LINE_WIDTH).contains(pos))
        {            
            return &zone;
        }
    }

    return 0;
}

ZoneList ZoneConfig::getZoneList()
{
    return m_zones;
}

void ZoneConfig::setZoneList(ZoneList & zones)
{
    m_zones = zones;

    initGridFlag();

    for (int i = 0; i < m_zones.size(); i++)
    {
        setGridFlag(m_zones[i].left, m_zones[i].right, m_zones[i].top, m_zones[i].bottom, true);
    }

    update();
}


void ZoneConfig::addZone(ZoneList & zones, int l, int r, int t, int b)
{
    LayoutZone zone;
    
    zone.left = l;
    zone.right = r;
    zone.top = t;
    zone.bottom = b;

    zones.push_back(zone);
}

void ZoneConfig::setLayoutMode(int layoutMode)
{    
    ZoneList zones;
    
    switch (layoutMode)
    {
    case LAYOUT_MODE_1:
        setGridSize(1, 1);

        addZone(zones, 0, 0, 0, 0);        
        
        setZoneList(zones);
        break;

    case LAYOUT_MODE_4:
        setGridSize(2, 2);

        addZone(zones, 0, 0, 0, 0);
        addZone(zones, 1, 1, 0, 0);
        addZone(zones, 0, 0, 1, 1);
        addZone(zones, 1, 1, 1, 1);
        
        setZoneList(zones);
        break;

    case LAYOUT_MODE_6:
        setGridSize(3, 3);

        addZone(zones, 0, 1, 0, 1);
        addZone(zones, 2, 2, 0, 0);
        addZone(zones, 2, 2, 1, 1);
        addZone(zones, 0, 0, 2, 2);
        addZone(zones, 1, 1, 2, 2);
        addZone(zones, 2, 2, 2, 2);
        
        setZoneList(zones);
        break;

    case LAYOUT_MODE_8:
        setGridSize(4, 4);

        addZone(zones, 0, 2, 0, 2);
        addZone(zones, 3, 3, 0, 0);
        addZone(zones, 3, 3, 1, 1);
        addZone(zones, 3, 3, 2, 2);
        addZone(zones, 0, 0, 3, 3);
        addZone(zones, 1, 1, 3, 3);
        addZone(zones, 2, 2, 3, 3);
        addZone(zones, 3, 3, 3, 3);
        
        setZoneList(zones);
        break;    

    case LAYOUT_MODE_9:
        setGridSize(3, 3);

        addZone(zones, 0, 0, 0, 0);
        addZone(zones, 1, 1, 0, 0);
        addZone(zones, 2, 2, 0, 0);
        addZone(zones, 0, 0, 1, 1);
        addZone(zones, 1, 1, 1, 1);
        addZone(zones, 2, 2, 1, 1);
        addZone(zones, 0, 0, 2, 2);
        addZone(zones, 1, 1, 2, 2);
        addZone(zones, 2, 2, 2, 2);
        
        setZoneList(zones);
        break; 

    case LAYOUT_MODE_16:
        setGridSize(4, 4);

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
        
        setZoneList(zones);
        break;
    }
}

void ZoneConfig::setGridSize(int rows, int cols)
{
    if (rows < 0 || cols < 0)
    {
        return;
    }
    else if (rows == m_gridRows && cols == m_gridCols)
    {
        return;
    }

    m_gridRows = rows;
    m_gridCols = cols;
    
    initGridFlag();

    m_zones.clear();

    update();
}








