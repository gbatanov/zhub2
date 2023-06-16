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

#include "SystemSetting.h"
#include <QFile>
#include <QMessageBox>
#include <QDesktopServices>
#include <QFileDialog>
#include "utils.h"


SystemSetting::SystemSetting(SysConfig &config, QWidget *parent, Qt::WindowFlags flags)
: DialogTitleBar(parent, flags)
, m_syscfg(config)
{
    ui.setupUi(this);

    initDialog();
    connSignalSlot();
}


SystemSetting::~SystemSetting()
{
}

void SystemSetting::initDialog()
{
    ui.cmbLogLevel->addItem(tr("TRACE"));
    ui.cmbLogLevel->addItem(tr("DEBUG"));
    ui.cmbLogLevel->addItem(tr("INFO"));
    ui.cmbLogLevel->addItem(tr("WARNING"));
    ui.cmbLogLevel->addItem(tr("ERROR"));
    ui.cmbLogLevel->addItem(tr("FATAL"));

    ui.cmbLanguage->addItem(tr("System"));
    ui.cmbLanguage->addItem(tr("English"));
    ui.cmbLanguage->addItem(tr("Chinese"));
    
	ui.cmbVideoRenderMode->addItem(tr("Keep the original aspect ratio"));
	ui.cmbVideoRenderMode->addItem(tr("Fill the whole window"));
	
	ui.editSnapshotPath->setText(m_syscfg.snapshotPath);
	ui.editRecordPath->setText(m_syscfg.recordPath);
	ui.chkEnableLog->setChecked(m_syscfg.enableLog);
	ui.chkRtpMulticast->setChecked(m_syscfg.rtpMulticast);
	ui.chkRtpOverUdp->setChecked(m_syscfg.rtpOverUdp);
	ui.chkRtspOverHttp->setChecked(m_syscfg.rtspOverHttp);
	ui.spinHttpPort->setValue(m_syscfg.rtspOverHttpPort);
	ui.cmbVideoRenderMode->setCurrentIndex(m_syscfg.videoRenderMode);
	ui.cmbLogLevel->setCurrentIndex(m_syscfg.logLevel);
	ui.cmbLanguage->setCurrentIndex(m_syscfg.sysLang);
}

void SystemSetting::connSignalSlot()
{
	connect(ui.btnCancel, SIGNAL(clicked()), this, SLOT(close()));    
    connect(ui.btnConfirm, SIGNAL(clicked()), this, SLOT(slotConfirm()));
	connect(ui.btnOpenSnapshotPath, SIGNAL(clicked()), this, SLOT(slotOpenSnapshotPath()));    
    connect(ui.btnOpenRecordPath, SIGNAL(clicked()), this, SLOT(slotOpenRecordPath()));
    connect(ui.btnBrowseSnapshotPath, SIGNAL(clicked()), this, SLOT(slotBrowseSnapshotPath()));    
    connect(ui.btnBrowseRecordPath, SIGNAL(clicked()), this, SLOT(slotBrowseRecordPath()));
}

void SystemSetting::slotOpenSnapshotPath()
{
    QString path = ui.editSnapshotPath->text();
    if (path.isEmpty())
    {
        return;
    }

    if (!QFile::exists(path))
    {
        QMessageBox::information(this, tr("Tips"), tr("Snapshot path not exist"));
        return;
    }
    
    QDesktopServices::openUrl(QUrl("file:///" + path, QUrl::TolerantMode));
}

void SystemSetting::slotOpenRecordPath()
{
    QString path = ui.editRecordPath->text();
    if (path.isEmpty())
    {
        return;
    }

    if (!QFile::exists(path))
    {
        QMessageBox::information(this, tr("Tips"), tr("Record path not exist"));
        return;
    }
    
    QDesktopServices::openUrl(QUrl("file:///" + path, QUrl::TolerantMode));
}

void SystemSetting::slotBrowseSnapshotPath()
{
    QFileDialog::Options options = QFileDialog::DontResolveSymlinks | QFileDialog::ShowDirsOnly;
    QString path = ui.editSnapshotPath->text();
    
	path = QFileDialog::getExistingDirectory(this, tr("Select snapshot save path"), path, options);
	if (!path.isEmpty())
	{
	    ui.editSnapshotPath->setText(path);
	}
}

void SystemSetting::slotBrowseRecordPath()
{
    QFileDialog::Options options = QFileDialog::DontResolveSymlinks | QFileDialog::ShowDirsOnly;
    QString path = ui.editRecordPath->text();
    
	path = QFileDialog::getExistingDirectory(this, tr("Select record save path"), path, options);
	if (!path.isEmpty())
	{
	    ui.editRecordPath->setText(path);
	}
}


void SystemSetting::slotConfirm()
{
	m_syscfg.recordPath = ui.editRecordPath->text();
	m_syscfg.snapshotPath = ui.editSnapshotPath->text();
	m_syscfg.enableLog = ui.chkEnableLog->isChecked();
	m_syscfg.videoRenderMode = ui.cmbVideoRenderMode->currentIndex();
	m_syscfg.rtpMulticast = ui.chkRtpMulticast->isChecked();
	m_syscfg.rtpOverUdp = ui.chkRtpOverUdp->isChecked();
	m_syscfg.rtspOverHttp = ui.chkRtspOverHttp->isChecked();
	m_syscfg.rtspOverHttpPort = ui.spinHttpPort->value();
	m_syscfg.logLevel = ui.cmbLogLevel->currentIndex();
	m_syscfg.sysLang = ui.cmbLanguage->currentIndex();
	
	if (!QFile::exists(m_syscfg.snapshotPath))
    {
        QMessageBox::information(this, tr("Tips"), tr("Please specify the correct snapshot path"));
        ui.editSnapshotPath->setFocus();
        return;
    }

    if (!QFile::exists(m_syscfg.recordPath))
    {
        QMessageBox::information(this, tr("Tips"), tr("Please specify the correct video recording path"));
        ui.editRecordPath->setFocus();
        return;
    }

	saveSnapshotPath(m_syscfg.snapshotPath);
	saveRecordPath(m_syscfg.recordPath);
	saveEnableLogFlag(m_syscfg.enableLog);
	saveVideoRenderMode(m_syscfg.videoRenderMode);
	saveRtpMulticast(m_syscfg.rtpMulticast);
	saveRtpOverUdp(m_syscfg.rtpOverUdp);
	saveRtspOverHttp(m_syscfg.rtspOverHttp);
	saveRtspOverHttpPort(m_syscfg.rtspOverHttpPort);
	saveLogLevel(m_syscfg.logLevel);
	saveSysLang(m_syscfg.sysLang);
	
	accept();
}



