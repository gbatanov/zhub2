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

#ifndef VIDEO_RENDER_D3D_H
#define VIDEO_RENDER_D3D_H

#include "video_render.h"
#include <d3d9.h>
#include <D3DX9Shader.h>

/***************************************************************************************/

typedef struct _YUV_VERTEX
{
	D3DXVECTOR4 pos;
	DWORD       color;
	D3DXVECTOR2 tex1;
	D3DXVECTOR2 tex2;
	D3DXVECTOR2 tex3;
} YUV_VERTEX;

enum GEOMETRICTRANSFORMATION
{
	GS_NONE = 0,
	GS_UPPER_DOWN_FLIP = 1,
	GS_LEFT_RIGHT_FLIP = 2 
};

#define Y_TEX               0 
#define U_TEX               1
#define V_TEX               2 
#define COUNT_YUV_TEX       3

/***************************************************************************************/

class CD3DVideoRender : public CVideoRender
{
public:
    CD3DVideoRender();
    ~CD3DVideoRender();

    BOOL init(HWND hWnd, int w, int h, int videofmt);
    void unInit();

    BOOL render(AVFrame * frame, int mode);

private:
    BOOL initDevice(D3DPRESENT_PARAMETERS * pD3DPP, HWND hWnd);
    BOOL initD3DPP(D3DPRESENT_PARAMETERS * pD3DPP, HWND hWnd);
    BOOL initShader(const DWORD * pCode3, const DWORD * pCode2);
    BOOL initImageBuffer(int w, int h, int videofmt);
    void freeImageBuffer();
    BOOL resetDevice();
    BOOL setImage(GEOMETRICTRANSFORMATION flip, RECT * pDstRect, RECT * pSrcRect);
    BOOL setImageArgs(float Transparent, GEOMETRICTRANSFORMATION flip, RECT * pDstRect, RECT * pSrcRect);
    
private:
    IDirect3D9            * m_pD3D9;
    IDirect3DDevice9      * m_pD3DD9;
	IDirect3DSurface9     * m_pRenderSurface; 
	IDirect3DTexture9     * m_pTexture[3];	    
	IDirect3DTexture9     * m_pTexTemp[3];
    D3DXVECTOR2             m_texPoint[4];
	D3DXVECTOR4             m_verPoint[4];
    LPDIRECT3DVERTEXBUFFER9 m_pVertices;
	LPDIRECT3DPIXELSHADER9  m_pPixelShader;
	LPD3DXCONSTANTTABLE     m_pPixelConstTable;
	D3DXHANDLE              m_hYUVTexture[3];
	D3DXCONSTANT_DESC       m_dYUVTexture[3];
	D3DXHANDLE              m_hTransparent;	
	GEOMETRICTRANSFORMATION m_nFlip;
	float                   m_dTransparent;
	DWORD                   m_dwColor;

	BOOL                    m_bReset;
	RECT                    m_dstRect;
	RECT                    m_srcRect;
};


#endif // end of VIDEO_RENDER_D3D_H




