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

#include "SetRtspInfo.h"
#include <QMessageBox>

SetRtspInfo::SetRtspInfo(QString &url, QString &user, QString &pass, QWidget *parent, Qt::WindowFlags flags)
: DialogTitleBar(parent, flags)
, m_url(url)
, m_user(user)
, m_pass(pass)
{
	ui.setupUi(this);

	ui.editUrl->setText(m_url);
	ui.editUser->setText(m_user);
	ui.editPass->setText(m_pass);

	connect(ui.btnCancel, SIGNAL(clicked()), this, SLOT(close()));
	connect(ui.btnConfirm, SIGNAL(clicked()), this, SLOT(slotConfirm()));
}


SetRtspInfo::~SetRtspInfo()
{
}

void SetRtspInfo::slotConfirm()
{
	m_url = ui.editUrl->text();
	m_user = ui.editUser->text();
	m_pass = ui.editPass->text();

	if (m_url.isEmpty())
	{
		QMessageBox::information(NULL, tr("Tips"), tr("The rtsp URL can not be empty!"));
		ui.editUrl->setFocus();
		return;
	}
#ifdef OVER_HTTP
    else if (m_url.startsWith("http://"))
    {
    }
#endif
	else if (!m_url.startsWith("rtsp://"))
	{
		QMessageBox::information(NULL, tr("Tips"), tr("Invalid RTSP URL format!"));
		ui.editUrl->setFocus();
		return;
	}

	accept();
}




