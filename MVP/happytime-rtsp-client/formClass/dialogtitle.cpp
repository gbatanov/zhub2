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
#include "dialogtitle.h"
#include <QtGui/QPainter>
#include <QtWidgets/QStyle>
#include <QtGui/QKeyEvent>
#include <QCoreApplication>
#include <QtWidgets/QDialog>

#ifdef _MSC_VER
#include <windows.h>
#include <qtwinextras/qwinfunctions.h>
#endif

DialogTitleBar::DialogTitleBar(QWidget *parent, Qt::WindowFlags flags) : QDialog(parent, flags)
{
    m_bSysMenu = (flags & Qt::WindowSystemMenuHint) ? true : false;
    m_bCanMax  = (flags & Qt::WindowMaximizeButtonHint) ? true : false;
    m_bCanMin  = (flags & Qt::WindowMinimizeButtonHint) ? true : false;

    m_bHiClose = m_bHiMax = m_bHiMin = false;

	m_bkColor = QColor(42, 46, 51);
	m_textColor = QColor(255, 255, 255);
}

DialogTitleBar::~DialogTitleBar()
{
}

void DialogTitleBar::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
	{
	case Qt::Key_Escape:
		return;
	}

	QDialog::keyPressEvent(event);
}

#ifdef _MSC_VER

#define WM_NCDRAWCAPTION      0xae 
#define WM_NCDRAWFRAME        0xaf 


bool DialogTitleBar::winEvent(MSG* pMessage, long *result)
{    
	//if (pMessage->hwnd != winId())
	//{
	//	*result = 0;
	//	return false;
	//}
		

	if (pMessage->message == WM_MOVING)
	{
		//qDebug() << QString::fromLocal8Bit("LQFoundation nativeEventFilter message::WM_MOVING(oldPos:") << ",newPos:)";
		return false;
	}
    
	if (pMessage->message == WM_NCPAINT || pMessage->message == WM_NCDRAWCAPTION || pMessage->message == WM_NCDRAWFRAME)
	{
	    paintTitle();
	    *result = 0;
	    return true;
	}
	else if (pMessage->message == WM_NCACTIVATE)
	{
		*result = true;
		return true;
	}
	else if (pMessage->message == WM_NCHITTEST)
	{
	    onNcHitTest(pMessage, result);
	    return true;
	}
	else if (pMessage->message == WM_NCMOUSEMOVE)
	{
	    onNcMouseMove(pMessage);
	    *result = 0;
		return true;
	}
	else if (pMessage->message == WM_NCMOUSELEAVE)
	{
	    onNcMouseLeave(pMessage);
	    *result = 0;
		return true;
	}
	else if (pMessage->message == WM_NCLBUTTONDOWN)
	{
	    if (onNcLButtonDown(pMessage, result))
            return true; 
	}
	else if (pMessage->message == WM_NCLBUTTONUP)
	{	    
	    onNcLButtonUp(pMessage, result);
	    return true;
	}
	else if (pMessage->message == WM_NCLBUTTONDBLCLK)
	{
	    onNcLButtonDblClk(pMessage, result);
	    return true;
	}

	return QDialog::nativeEvent("", pMessage, result);
}


#define RIGHT_MARGIN        10
#define TOP_MARGIN          6
#define LEFT_MARGIN         6
#define BTN_SPACE           0
#define TEXT_SPACE          4

#define HL_OPACITY          0.6

void DialogTitleBar::paintTitle()
{
    RECT rcWindow, rcClient;
    
	::GetWindowRect((HWND)winId(), &rcWindow);
	::GetClientRect((HWND)winId(), &rcClient);
    
	HDC  hDC = ::GetWindowDC((HWND)winId());
	LONG lStyle = ::GetWindowLong((HWND)winId(), GWL_STYLE);

    int hframe  = GetSystemMetrics(SM_CXFRAME);
    int vframe  = GetSystemMetrics(SM_CYFRAME);
    int caption = GetSystemMetrics(SM_CYCAPTION);
	int cxsize = GetSystemMetrics(SM_CXSIZE);
	int cysize = GetSystemMetrics(SM_CYSIZE);
	int cxicon = GetSystemMetrics(SM_CXSMICON);
	int cyicon = GetSystemMetrics(SM_CYSMICON);
	int cycaption = GetSystemMetrics(SM_CYCAPTION);

    int width   = rcWindow.right - rcWindow.left;
    
    POINT ptTopLeft     = {rcClient.left, rcClient.top};
    POINT ptBottomRight = {rcClient.right, rcClient.bottom};
 
	::ClientToScreen((HWND)winId(), &ptTopLeft);
	::ClientToScreen((HWND)winId(), &ptBottomRight);
    
    rcClient.left   = ptTopLeft.x - rcWindow.left;
    rcClient.top    = ptTopLeft.y - rcWindow.top;
    rcClient.right  = ptBottomRight.x - rcWindow.left;
    rcClient.bottom = ptBottomRight.y - rcWindow.top;
 
    rcWindow.right  = rcWindow.right - rcWindow.left;
    rcWindow.bottom = rcWindow.bottom - rcWindow.top;
    rcWindow.top    = 0;
    rcWindow.left   = 0;
 
    HRGN hRgnOuter   = ::CreateRectRgn(rcWindow.left, rcWindow.top, rcWindow.right, rcWindow.bottom);
    HRGN hRgnInner   = ::CreateRectRgn(rcClient.left, rcClient.top, rcClient.right, rcClient.bottom);
    HRGN hRgnCombine = ::CreateRectRgn(rcWindow.left, rcWindow.top, rcWindow.right, rcWindow.bottom);
 
    ::CombineRgn(hRgnCombine, hRgnOuter, hRgnInner, RGN_DIFF);
    ::SelectClipRgn(hDC, hRgnCombine);
 
    QPixmap  back(rcWindow.right, rcWindow.bottom);
    QPainter paint(&back);
    QRect    rc(0, 0, rcWindow.right, rcWindow.bottom);
	    
	paint.fillRect(rc, m_bkColor);

    QRect rcClose(width - RIGHT_MARGIN - cxsize, TOP_MARGIN, cxsize, cysize); 

    if (m_bHiClose)
    {        
        paint.drawPixmap(rcClose, QPixmap(":/RtspClient/Resources/btn_close.png"));        
    }
    else
    {
		paint.setOpacity(HL_OPACITY);
        paint.drawPixmap(rcClose, QPixmap(":/RtspClient/Resources/btn_close.png"));
		paint.setOpacity(1);
    }    
    
    if (m_bCanMax)
    {
        QRect rcMax;

        rcMax.setLeft(width - RIGHT_MARGIN - 2*cxsize - BTN_SPACE);
        rcMax.setTop(TOP_MARGIN);
        rcMax.setWidth(cxsize);
        rcMax.setHeight(cysize);

        if (m_bHiMax)
        {            
			paint.drawPixmap(rcMax, !IsZoomed((HWND)winId()) ?
    	                     QPixmap(":/RtspClient/Resources/btn_max.png") :
    	                     QPixmap(":/RtspClient/Resources/btn_max_hot.png"));            	     	                     
        }
        else
        {
			paint.setOpacity(HL_OPACITY);
			paint.drawPixmap(rcMax, !IsZoomed((HWND)winId()) ?
    	                     QPixmap(":/RtspClient/Resources/btn_max.png") :
    	                     QPixmap(":/RtspClient/Resources/btn_max_hot.png"));
			paint.setOpacity(1);
        }                            
    }
    
    if (m_bCanMin)
    {
        QRect rcMin;

        rcMin.setLeft(width - RIGHT_MARGIN - 3*cxsize - 2*BTN_SPACE);
        rcMin.setTop(TOP_MARGIN);
        rcMin.setWidth(cxsize);
        rcMin.setHeight(cysize);

        if (m_bHiMin)
        {            
            paint.drawPixmap(rcMin, QPixmap(":/RtspClient/Resources/btn_min.png"));            
        }
        else
        {
			paint.setOpacity(HL_OPACITY);
            paint.drawPixmap(rcMin, QPixmap(":/RtspClient/Resources/btn_min.png"));
			paint.setOpacity(1);
        }
    }
    
    if (m_bSysMenu)
    {
		QRect rcSysIcon(hframe + LEFT_MARGIN, (cycaption - cyicon) / 2 + vframe, cxicon, cyicon);

	    paint.drawPixmap(rcSysIcon, QPixmap(":/RtspClient/Resources/main.png"));
    }

    if (!windowTitle().isEmpty())
    {
        QRect rcText;

        rcText.setLeft(hframe + LEFT_MARGIN + TEXT_SPACE + (m_bSysMenu ? cxicon : 0)); 
        rcText.setTop(vframe);
        rcText.setWidth(width - 4 * cxsize - hframe - 2 * TEXT_SPACE - RIGHT_MARGIN - 2 * BTN_SPACE);
        rcText.setHeight(cycaption);

        paint.setPen(m_textColor);
        paint.drawText(rcText, Qt::AlignLeft | Qt::AlignVCenter, windowTitle());
    }
    
	HBITMAP hBmp = QtWin::toHBITMAP(back, QtWin::HBitmapNoAlpha);;
    HDC hMemDC   = ::CreateCompatibleDC(hDC);
    
    ::SelectObject(hMemDC, hBmp);
    ::BitBlt(hDC, rcWindow.left, rcWindow.top, rcWindow.right, rcWindow.bottom, hMemDC, 0, 0, SRCCOPY);
    ::DeleteDC(hMemDC);
    ::DeleteObject(hBmp);
 
    ::DeleteObject(hRgnOuter);
    ::DeleteObject(hRgnInner);
    ::DeleteObject(hRgnCombine);
    
	::ReleaseDC((HWND)winId(), hDC);
}

void DialogTitleBar::onNcHitTest(MSG* pMessage, long *result)
{
    RECT rcWindow;
    
	::GetWindowRect((HWND)winId(), &rcWindow);
    
    int hframe  = GetSystemMetrics(SM_CXFRAME);
    int vframe  = GetSystemMetrics(SM_CYFRAME);
    int caption = GetSystemMetrics(SM_CYCAPTION);
	int cxsize = GetSystemMetrics(SM_CXSIZE);
	int cysize = GetSystemMetrics(SM_CYSIZE);
	int cxicon = GetSystemMetrics(SM_CXSMICON);
	int cyicon = GetSystemMetrics(SM_CYSMICON);
	
	QRect rcCaption(rcWindow.left, rcWindow.top, rcWindow.right-rcWindow.left, caption);
    QRect rcClose(rcWindow.right - RIGHT_MARGIN - cxsize, rcWindow.top + TOP_MARGIN, cxsize, cysize);    
    QRect rcMax(rcWindow.right - RIGHT_MARGIN - 2*cxsize - BTN_SPACE, rcWindow.top + TOP_MARGIN, cxsize, cysize);
    QRect rcMin(rcWindow.right - RIGHT_MARGIN - 3*cxsize - 2*BTN_SPACE, rcWindow.top + TOP_MARGIN, cxsize, cysize);
    QRect rcSysIcon(rcWindow.left + LEFT_MARGIN, rcWindow.top + TOP_MARGIN, cxicon, cyicon);
    QRect rcClient(rcWindow.left+hframe, rcWindow.top+vframe+caption, 
                   rcWindow.right-rcWindow.left-2*hframe, rcWindow.bottom-rcWindow.top-2*vframe-caption);

    QPoint pt(LOWORD(pMessage->lParam), HIWORD(pMessage->lParam));

    if (rcClose.contains(pt))
    {
        *result = HTCLOSE;
    }
    else if (m_bCanMax && rcMax.contains(pt))
    {
        *result = HTMAXBUTTON;
    }
    else if (m_bCanMin && rcMin.contains(pt))
    {
        *result = HTMINBUTTON;
    }
    else if (m_bSysMenu && rcSysIcon.contains(pt))
    {
        *result = HTSYSMENU;
    }
    else if (m_bCanMax)
    {    
        QRect rcLeftBorder(rcWindow.left, rcWindow.top+vframe, hframe, rcWindow.bottom-2*vframe);
        QRect rcRightBorder(rcWindow.right-hframe, rcWindow.top+vframe, hframe, rcWindow.bottom-2*vframe);
        QRect rcLeftBottom(rcWindow.left, rcWindow.bottom-vframe, hframe, vframe);
        QRect rcLeftTop(rcWindow.left, rcWindow.top, hframe, vframe);
        QRect rcRightTop(rcWindow.right-hframe, rcWindow.top, hframe, vframe);
        QRect rcRightBottom(rcWindow.right-hframe, rcWindow.bottom-vframe, hframe, vframe);        
        QRect rcBottomBorder(rcWindow.left+hframe, rcWindow.bottom-vframe, rcWindow.right-rcWindow.left-2*hframe, vframe);
        QRect rcTopBorder(rcWindow.left+hframe, rcWindow.top, rcWindow.right-rcWindow.left-2*hframe, vframe);
    
        if (rcLeftBottom.contains(pt))
        {
            *result = HTBOTTOMLEFT;
        }
        else if (rcLeftTop.contains(pt))
        {
            *result = HTTOPLEFT;
        }
        else if (rcLeftBorder.contains(pt))
        {
            *result = HTLEFT;
        }
        else if (rcRightTop.contains(pt))
        {
            *result = HTTOPRIGHT;
        }
        else if (rcRightBottom.contains(pt))
        {
            *result = HTBOTTOMRIGHT;
        }
        else if (rcRightBorder.contains(pt))
        {
            *result = HTRIGHT;
        }
        else if (rcBottomBorder.contains(pt))
        {
            *result = HTBOTTOM;
        }
        else if (rcTopBorder.contains(pt))
        {
            *result = HTTOP;
        }
        else if (rcCaption.contains(pt))
        {
            *result = HTCAPTION;
        }
        else if (rcClient.contains(pt))
        {
            *result = HTCLIENT;
        }
        else
        {
            *result = 0;
        }
    }
    else if (rcCaption.contains(pt))
    {
        *result = HTCAPTION;
    }
    else if (rcClient.contains(pt))
    {
        *result = HTCLIENT;
    }
    else
    {
        *result = 0;
    }    
}


bool DialogTitleBar::onNcLButtonDown(MSG* pMessage, long *result)
{
    if (pMessage->wParam == HTSYSMENU)
    {
        if (m_bSysMenu)
        {
			SendMessage((HWND)winId(), WM_SYSCOMMAND, SC_MOUSEMENU, MAKELPARAM(pMessage->pt.x, pMessage->pt.y));
        }
        
        *result = 0;
        return true;
    }
    else if (pMessage->wParam == HTCLOSE || pMessage->wParam == HTMAXBUTTON || pMessage->wParam == HTMINBUTTON)
    {
        *result = 0;
        return true;
    }
    
	return false;	
}



void DialogTitleBar::onNcLButtonUp(MSG* pMessage, long *result)
{
    if (pMessage->wParam == HTCLOSE)
    {
		SendMessage((HWND)winId(), WM_CLOSE, 0, 0);
    }
    else if (pMessage->wParam == HTMAXBUTTON && m_bCanMax)
    {
        m_bHiMax = false;
		SendMessage((HWND)winId(), WM_SYSCOMMAND, IsZoomed((HWND)winId()) ? SC_RESTORE : SC_MAXIMIZE, MAKELPARAM(pMessage->pt.x, pMessage->pt.y));
    }
    else if (pMessage->wParam == HTMINBUTTON && m_bCanMin)
    {
        m_bHiMin = false;
		SendMessage((HWND)winId(), WM_SYSCOMMAND, SC_MINIMIZE, MAKELPARAM(pMessage->pt.x, pMessage->pt.y));
    }
    
    *result = 0;
}


void DialogTitleBar::onNcLButtonDblClk(MSG* pMessage, long *result)
{
    if (pMessage->wParam == HTCAPTION && m_bCanMax)
    {
		SendMessage((HWND)winId(), WM_SYSCOMMAND, IsZoomed((HWND)winId()) ? SC_RESTORE : SC_MAXIMIZE, MAKELPARAM(pMessage->pt.x, pMessage->pt.y));
    }

    *result = 0;
}

void DialogTitleBar::onNcMouseMove(MSG* pMessage)
{
    bool bRepaint = false;
    
    if (pMessage->wParam == HTCLOSE)
    {
        if (!m_bHiClose)
        {
            bRepaint = true;
            m_bHiClose = true;
        }
    }
    else if (m_bHiClose)
    {
        bRepaint = true;
        m_bHiClose = false;
    }
    
    if (pMessage->wParam == HTMAXBUTTON)
    {
        if (!m_bHiMax)
        {
            bRepaint = true;
            m_bHiMax = true;
        }
    }
    else if (m_bHiMax)
    {
        bRepaint = true;
        m_bHiMax = false;
    }
    
    if (pMessage->wParam == HTMINBUTTON)
    {
        if (!m_bHiMin)
        {
            bRepaint = true;
            m_bHiMin = true;
        }
    }
    else if (m_bHiMin)
    {
        bRepaint = true;
        m_bHiMin = false;
    }

    if (bRepaint)
    {
        paintTitle();
    }
}

void DialogTitleBar::onNcMouseLeave(MSG* pMessage)
{
    bool bRepaint = false;

    if (m_bHiClose)
    {
        bRepaint = true;
        m_bHiClose = false;
    }

    if (m_bHiMax)
    {
        bRepaint = true;
        m_bHiMax = false;
    }

    if (m_bHiMin)
    {
        bRepaint = true;
        m_bHiMin = false;
    }

    if (bRepaint)
    {
        paintTitle();
    }
}

#endif



