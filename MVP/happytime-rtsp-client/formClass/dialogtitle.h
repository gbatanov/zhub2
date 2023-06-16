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

#ifndef _DialogTitleBarTITLEBAR_H_
#define _DialogTitleBarTITLEBAR_H_

#include<QtWidgets/qdialog.h>

class DialogTitleBar : public QDialog
{
	Q_OBJECT

public:
	DialogTitleBar(QWidget *parent = 0, Qt::WindowFlags flags = 0);
	~DialogTitleBar();

	void setCaptionBkColor(QColor color) { m_bkColor = color; }
	void setCaptionTextColor(QColor color) { m_textColor = color; }

protected:
    void keyPressEvent(QKeyEvent *event);
    
#ifdef _MSC_VER    
	bool winEvent(MSG* pMessage, long *result);
	bool nativeEvent(const QByteArray &eventType, void *message, long *result)
	{
		Q_UNUSED(eventType);
		if (strcmp(eventType.data(), "windows_generic_MSG") != 0)
		{
			return false;
		}
		MSG* msg = reinterpret_cast<MSG*>(message);
		return winEvent(msg, result);
	}
#endif

private:

#ifdef _MSC_VER    
	void paintTitle();
    void onNcHitTest(MSG* pMessage, long *result);
    bool onNcLButtonDown(MSG* pMessage, long *result);
    void onNcLButtonUp(MSG* pMessage, long *result);
    void onNcLButtonDblClk(MSG* pMessage, long *result);
    void onNcMouseMove(MSG* pMessage);
    void onNcMouseLeave(MSG* pMessage);
#endif

private:
    bool m_bSysMenu;
    bool m_bCanMax;
    bool m_bCanMin;

    bool m_bHiClose;
    bool m_bHiMax;
    bool m_bHiMin;

	QColor m_bkColor;
	QColor m_textColor;
};

#endif // _DialogTitleBarTITLEBAR_H_


