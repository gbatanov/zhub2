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
#include "CustomLayoutWidget.h"


CustomLayoutWidget::CustomLayoutWidget(QWidget *parent, Qt::WindowFlags flags)
: DialogTitleBar(parent, flags)
{
    ui.setupUi(this);

    initCustomLayoutWidget();

    connSignalSlot();
}


CustomLayoutWidget::~CustomLayoutWidget()
{
}


void CustomLayoutWidget::initCustomLayoutWidget()
{
    int i;
    
    for (i = 1; i <= MAX_GRID_ROWS; i++)
    {
        ui.cmbGridRows->addItem(QString("%1").arg(i), i);
    }

    for (i = 1; i <= MAX_GRID_COLS; i++)
    {
        ui.cmbGridCols->addItem(QString("%1").arg(i), i);
    }

    ui.cmbGridRows->setCurrentIndex(DEF_GRID_ROWS-1);
    ui.cmbGridCols->setCurrentIndex(DEF_GRID_COLS-1);
}


void CustomLayoutWidget::connSignalSlot()
{
    connect(ui.btnCancel, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui.btnConfirm, SIGNAL(clicked()), this, SLOT(slotConfirm()));

    connect(ui.cmbGridRows, SIGNAL(currentIndexChanged(int)), this, SLOT(slotGridRowsChanged(int)));
    connect(ui.cmbGridCols, SIGNAL(currentIndexChanged(int)), this, SLOT(slotGridColsChanged(int)));
}


Layout CustomLayoutWidget::getLayout()
{
    Layout layout;

    layout.rows = ui.cmbGridRows->itemData(ui.cmbGridRows->currentIndex()).toInt();
    layout.cols = ui.cmbGridCols->itemData(ui.cmbGridCols->currentIndex()).toInt();
    layout.zones = ui.zoneWidget->getZoneList();

    return layout;
}

void CustomLayoutWidget::slotConfirm()
{
    accept();
}


void CustomLayoutWidget::slotGridRowsChanged(int index)
{
    int rows = ui.cmbGridRows->itemData(index).toInt();
    int cols = ui.cmbGridCols->itemData(ui.cmbGridCols->currentIndex()).toInt();

    ui.zoneWidget->setGridSize(rows, cols);
}


void CustomLayoutWidget::slotGridColsChanged(int index)
{
    int rows = ui.cmbGridRows->itemData(ui.cmbGridRows->currentIndex()).toInt();
    int cols = ui.cmbGridCols->itemData(index).toInt();

    ui.zoneWidget->setGridSize(rows, cols);
}




