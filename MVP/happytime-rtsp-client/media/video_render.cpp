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

#include "sys_inc.h"
#include "video_render.h"


CVideoRender::CVideoRender()
: m_bInited(FALSE)
, m_hWnd(NULL)
, m_nVideoWidth(0)
, m_nVideoHeight(0)
, m_nVideoFmt(VIDEO_FMT_YUV420P)
{
    
}

CVideoRender::~CVideoRender()
{
    unInit();
}

BOOL CVideoRender::init(HWND hWnd, int w, int h, int videofmt)
{
    m_hWnd = hWnd;
    m_nVideoWidth = w;
    m_nVideoHeight = h;
    m_nVideoFmt = videofmt;

    return TRUE;
}

void CVideoRender::unInit()
{
	m_bInited = FALSE;
}

BOOL CVideoRender::render(AVFrame * frame, int mode)
{
    return FALSE;
}



