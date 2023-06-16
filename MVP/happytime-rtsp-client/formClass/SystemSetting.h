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

#ifndef _SYSTEM_SETTING_H_
#define _SYSTEM_SETTING_H_

#include "sys_inc.h"
#include <QDialog>
#include "ui_SystemSetting.h"
#include "config.h"
#include "dialogtitle.h"

class SystemSetting : public DialogTitleBar
{
    Q_OBJECT

public:
	SystemSetting(SysConfig &config, QWidget *parent = 0, Qt::WindowFlags flags = 0);
    ~SystemSetting();

	SysConfig getSysConfig(){return m_syscfg;}
	
private slots:
	void slotConfirm();	
	void slotOpenSnapshotPath();
	void slotOpenRecordPath();
	void slotBrowseSnapshotPath();
	void slotBrowseRecordPath();

private:
	void initDialog();
	void connSignalSlot();
	
private:
    Ui::SystemSetting ui;  

	SysConfig m_syscfg;
};

#endif




