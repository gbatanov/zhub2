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

#ifndef _LAYOUT_H_
#define _LAYOUT_H_

#include <QString>

#define LAYOUT_MODE_1       1
#define LAYOUT_MODE_2		2
#define LAYOUT_MODE_3		3
#define LAYOUT_MODE_4		4
#define LAYOUT_MODE_5		5
#define LAYOUT_MODE_6       6
#define LAYOUT_MODE_7       7
#define LAYOUT_MODE_8		8
#define LAYOUT_MODE_9		9
#define LAYOUT_MODE_10		10
#define LAYOUT_MODE_11		11
#define LAYOUT_MODE_13		13
#define LAYOUT_MODE_16		16
#define LAYOUT_MODE_19		19
#define LAYOUT_MODE_22		22
#define LAYOUT_MODE_25		25


typedef struct
{
 	int		left; 	
 	int		right;
 	int		top;
 	int		bottom;

 	QString url;
 	QString acct;
 	QString pass;
} LayoutZone;

typedef QList<LayoutZone> LayoutZoneList;

typedef struct 
{
    int rows;
    int cols;
    LayoutZoneList zones;
} Layout;


#endif


