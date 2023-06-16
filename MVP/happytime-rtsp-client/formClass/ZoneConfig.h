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

#ifndef _ZONE_CONFIG_H_
#define _ZONE_CONFIG_H_

#include <QWidget>
#include <QAction>
#include "Layout.h"

#define DEF_GRID_ROWS               9
#define DEF_GRID_COLS               16

#define MAX_GRID_ROWS               64
#define MAX_GRID_COLS               64

typedef LayoutZoneList ZoneList;

class ZoneConfig : public QWidget
{
    Q_OBJECT

public:
	ZoneConfig(QWidget *parent);
	~ZoneConfig();

    ZoneList    getZoneList();
    void        setZoneList(ZoneList & zones);
    void        setLayoutMode(int layoutMode);
    int         getGridRows() {return m_gridRows;}
    int         getGridCols() {return m_gridCols;}
    void        setGridSize(int rows, int cols);
    
protected:
    virtual void paintEvent(QPaintEvent * event);
	virtual void dropEvent(QDropEvent *event);
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual void mouseMoveEvent(QMouseEvent * event);
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseReleaseEvent(QMouseEvent * event);
    virtual void contextMenuEvent(QContextMenuEvent * event);

public slots:
    void        slotDelZone();
    void        slotClearZone();
    
private:
    void        initActions();    
    void        initZoneConfig();
    void        initGridFlag();
    void        drawGrid(QPainter & painter, int rows, int cols);
    void        drawDragingZone(QPainter & painter);
    void        addZone();
    void        addZone(ZoneList & zones, int l, int r, int t, int b);
    void        delZone(LayoutZone * zone);
    void        drawZone(QPainter & painter);
    QRect       getGridRect(int left, int right, int top, int bottom);
    bool        isValidPos(QPoint pos);
    void        getDragingGrids(QPoint pos1, QPoint pos2, int &left, int &right, int &top, int &bottom);
    void        setGridFlag(int left, int right, int top, int bottom, bool flag);
    bool        isValidGridRect(int left, int right, int top, int bottom);
    LayoutZone* getZone(QPoint pos);

private:
    QPoint      m_pressPos;
    QPoint      m_curPos;
    QPoint      m_ContextMenuPos;

    int         m_gridRows;
    int         m_gridCols;
    
    ZoneList    m_zones;
    bool        m_mouseMoving;

    bool        m_gridFlag[MAX_GRID_ROWS][MAX_GRID_COLS];

    QAction   * m_actionDelZone;
    QAction   * m_actionClearZone;
};


#endif // _ZONE_CONFIG_H_


