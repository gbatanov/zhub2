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

#ifndef COMM_CFG_H
#define COMM_CFG_H

typedef struct
{
    int     codec;                  // video codec, refer media_format.h
    int     width;                  // video width
    int     height;                 // video height
    int     framerate;              // frame rate
    int     bitrate;                // bitrate
} RTSP_V_INFO;

typedef struct
{
    int     codec;                  // audio codec, refer media_format.h
    int     samplerate;             // sample rate
    int     channels;               // channels
    int     bitrate;                // bitrate
} RTSP_A_INFO;

typedef struct _RTSP_OUTPUT
{
    struct _RTSP_OUTPUT * next;
    
    char            url[256];       // the match url path
    RTSP_V_INFO     v_info;         // video output information
    RTSP_A_INFO     a_info;         // audio output information
} RTSP_OUTPUT;

#endif


