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
#include "video_render_d3d.h"
#include "globallock.h"

/***************************************************************************************/

const DWORD g_YUV420ps30_main[] = 
{
    0xffff0300, 0x003ffffe, 0x42415443, 0x0000001c, 0x000000c7, 0xffff0300, 
    0x00000004, 0x0000001c, 0x20000102, 0x000000c0, 0x0000006c, 0x00010003, 
    0x00000001, 0x00000074, 0x00000000, 0x00000084, 0x00020003, 0x00000001, 
    0x00000074, 0x00000000, 0x0000008c, 0x00000003, 0x00000001, 0x00000074, 
    0x00000000, 0x00000094, 0x00000002, 0x00000001, 0x000000a0, 0x000000b0, 
    0x78655455, 0x00657574, 0x000c0004, 0x00010001, 0x00000001, 0x00000000, 
    0x78655456, 0x00657574, 0x78655459, 0x00657574, 0x6e617274, 0x72617073, 
    0x00746e65, 0x00030000, 0x00010001, 0x00000001, 0x00000000, 0x3f800000, 
    0x00000000, 0x00000000, 0x00000000, 0x335f7370, 0x4d00305f, 0x6f726369, 
    0x74666f73, 0x29522820, 0x534c4820, 0x6853204c, 0x72656461, 0x6d6f4320, 
    0x656c6970, 0x2e392072, 0x392e3432, 0x322e3934, 0x00373033, 0x05000051, 
    0xa00f0001, 0x3f950a81, 0x3fcc4a9d, 0xbf5fcbb4, 0x00000000, 0x05000051, 
    0xa00f0002, 0x3f950a81, 0xbec89507, 0xbf501eac, 0x3f081b65, 0x05000051, 
    0xa00f0003, 0x3f950a81, 0x40011a54, 0xbf8af5f5, 0x00000000, 0x05000051, 
    0xa00f0004, 0x3f800000, 0x00000000, 0x00000000, 0x00000000, 0x0200001f, 
    0x80000005, 0x90030000, 0x0200001f, 0x80010005, 0x90030001, 0x0200001f, 
    0x80020005, 0x90030002, 0x0200001f, 0x90000000, 0xa00f0800, 0x0200001f, 
    0x90000000, 0xa00f0801, 0x0200001f, 0x90000000, 0xa00f0802, 0x03000042, 
    0x800f0000, 0x90e40002, 0xa0e40802, 0x02000001, 0x80040000, 0x80000000, 
    0x03000042, 0x800f0001, 0x90e40000, 0xa0e40800, 0x04000004, 0x80090000, 
    0x80000001, 0xa0640004, 0xa0250004, 0x03000008, 0x80010800, 0xa0e40001, 
    0x80f80000, 0x03000042, 0x800f0001, 0x90e40001, 0xa0e40801, 0x02000001, 
    0x80020000, 0x80000001, 0x03000009, 0x80020800, 0xa0e40002, 0x80e40000, 
    0x03000008, 0x80040800, 0xa0e40003, 0x80f40000, 0x02000001, 0x80080800, 
    0xa0000000, 0x0000ffff
};

const DWORD g_YUV420ps20_main[] =
{
    0xffff0200, 0x003ffffe, 0x42415443, 0x0000001c, 0x000000c7, 0xffff0200, 
    0x00000004, 0x0000001c, 0x20000102, 0x000000c0, 0x0000006c, 0x00010003, 
    0x00000001, 0x00000074, 0x00000000, 0x00000084, 0x00020003, 0x00000001, 
    0x00000074, 0x00000000, 0x0000008c, 0x00000003, 0x00000001, 0x00000074, 
    0x00000000, 0x00000094, 0x00000002, 0x00000001, 0x000000a0, 0x000000b0, 
    0x78655455, 0x00657574, 0x000c0004, 0x00010001, 0x00000001, 0x00000000, 
    0x78655456, 0x00657574, 0x78655459, 0x00657574, 0x6e617274, 0x72617073, 
    0x00746e65, 0x00030000, 0x00010001, 0x00000001, 0x00000000, 0x3f800000, 
    0x00000000, 0x00000000, 0x00000000, 0x325f7370, 0x4d00305f, 0x6f726369, 
    0x74666f73, 0x29522820, 0x534c4820, 0x6853204c, 0x72656461, 0x6d6f4320, 
    0x656c6970, 0x2e392072, 0x392e3432, 0x322e3934, 0x00373033, 0x05000051, 
    0xa00f0001, 0x3f950a81, 0x00000000, 0x3fcc4a9d, 0xbf5fcbb4, 0x05000051, 
    0xa00f0002, 0x3f950a81, 0xbec89507, 0xbf501eac, 0x3f081b65, 0x05000051, 
    0xa00f0003, 0x3f950a81, 0x40011a54, 0x00000000, 0xbf8af5f5, 0x05000051, 
    0xa00f0004, 0x3f800000, 0x00000000, 0x00000000, 0x00000000, 0x0200001f, 
    0x80000000, 0xb0030000, 0x0200001f, 0x80000000, 0xb0030001, 0x0200001f, 
    0x80000000, 0xb0030002, 0x0200001f, 0x90000000, 0xa00f0800, 0x0200001f, 
    0x90000000, 0xa00f0801, 0x0200001f, 0x90000000, 0xa00f0802, 0x03000042, 
    0x800f0000, 0xb0e40000, 0xa0e40800, 0x03000042, 0x800f0001, 0xb0e40001, 
    0xa0e40801, 0x03000042, 0x800f0002, 0xb0e40002, 0xa0e40802, 0x02000001, 
    0x80080003, 0xa0000000, 0x02000001, 0x80020000, 0x80000001, 0x02000001, 
    0x80040000, 0x80000002, 0x02000001, 0x80080000, 0xa0000004, 0x03000009, 
    0x80010003, 0xa0e40001, 0x80e40000, 0x03000009, 0x80020003, 0xa0e40002, 
    0x80e40000, 0x03000009, 0x80040003, 0xa0e40003, 0x80e40000, 0x02000001, 
    0x800f0800, 0x80e40003, 0x0000ffff
};

/***************************************************************************************/

CD3DVideoRender::CD3DVideoRender()
: CVideoRender()
, m_pD3D9(NULL)
, m_pD3DD9(NULL)
, m_pRenderSurface(NULL)
, m_pVertices(NULL)
, m_pPixelShader(NULL)
, m_pPixelConstTable(NULL)
, m_hTransparent(NULL)
, m_nFlip(GS_NONE)
, m_dTransparent(0.0f)
, m_dwColor(0)
, m_bReset(FALSE)
{
    m_pTexture[0] = NULL;
    m_pTexture[1] = NULL;
    m_pTexture[2] = NULL;

    m_pTexTemp[0] = NULL;
    m_pTexTemp[1] = NULL;
    m_pTexTemp[2] = NULL;

    m_hYUVTexture[0] = NULL;
    m_hYUVTexture[1] = NULL;
    m_hYUVTexture[2] = NULL;

    memset(&m_dstRect, 0, sizeof(RECT));
    memset(&m_srcRect, 0, sizeof(RECT));
}

CD3DVideoRender::~CD3DVideoRender()
{
    unInit();
}

BOOL CD3DVideoRender::init(HWND hWnd, int w, int h, int videofmt)
{
    CGlobalLock lock;
    
    m_pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (NULL == m_pD3D9)
    {
        log_print(HT_LOG_ERR, "%s, Direct3DCreate9 failed\r\n", __FUNCTION__);
        return FALSE;
    }

    HRESULT hr;
	D3DPRESENT_PARAMETERS D3DPP;
    
    initD3DPP(&D3DPP, hWnd);

    if (!initDevice(&D3DPP, hWnd))
    {
        log_print(HT_LOG_ERR, "%s, initDevice failed\r\n", __FUNCTION__);
        return FALSE;
    }
	
	hr = m_pD3DD9->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX3);
    if (FAILED(hr))
    {
        log_print(HT_LOG_ERR, "%s, SetFVF failed\r\n", __FUNCTION__);
        return FALSE;
	}
	
	if (initShader(g_YUV420ps30_main, g_YUV420ps20_main) == FALSE)
	{
	    log_print(HT_LOG_ERR, "%s, initShader failed\r\n", __FUNCTION__);
		return FALSE;
	}
    
    hr = m_pD3DD9->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    if (FAILED(hr))
    {
        log_print(HT_LOG_ERR, "%s, Clear failed\r\n", __FUNCTION__);
        return FALSE;    
    }

    CVideoRender::init(hWnd, w, h, videofmt);

    m_bInited = initImageBuffer(w, h, videofmt);

    return m_bInited;
}

void CD3DVideoRender::unInit()
{
    CGlobalLock lock;

    freeImageBuffer();

	if (m_pPixelShader)
	{ 
	    m_pPixelShader->Release(); 
	    m_pPixelShader = NULL; 
	}

	if (m_pPixelConstTable) 
	{ 
	    m_pPixelConstTable->Release(); 
	    m_pPixelConstTable = NULL; 
	}

	if (m_pD3DD9) 
	{
    	m_pD3DD9->Release(); 
    	m_pD3DD9 = NULL; 
	}
	
	if (m_pD3D9) 
	{ 
    	m_pD3D9->Release(); 
    	m_pD3D9 = NULL; 
	}

	m_bInited = FALSE;
}

BOOL CD3DVideoRender::initD3DPP(D3DPRESENT_PARAMETERS * pD3DPP, HWND hWnd)
{
    memset(pD3DPP, 0, sizeof(D3DPRESENT_PARAMETERS));
    
    pD3DPP->Windowed = TRUE;
    pD3DPP->hDeviceWindow = hWnd;

    pD3DPP->Flags = D3DPRESENTFLAG_VIDEO;
    pD3DPP->FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    pD3DPP->PresentationInterval = D3DPRESENT_INTERVAL_ONE; // note that this setting leads to an implicit timeBeginPeriod call
    pD3DPP->BackBufferCount = 1;
    pD3DPP->BackBufferFormat = D3DFMT_UNKNOWN;

    pD3DPP->BackBufferWidth  = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    pD3DPP->BackBufferHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    //
    // Mark the back buffer lockable if software DXVA2 could be used.
    // This is because software DXVA2 device requires a lockable render target
    // for the optimal performance.
    //
    pD3DPP->Flags |= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

    pD3DPP->SwapEffect = D3DSWAPEFFECT_DISCARD;

    return TRUE;
}

BOOL CD3DVideoRender::initDevice(D3DPRESENT_PARAMETERS * pD3DPP, HWND hWnd)
{
    HRESULT hr;
    UINT AdapterToUse = D3DADAPTER_DEFAULT;
    D3DDEVTYPE DeviceType = D3DDEVTYPE_HAL;

    DWORD thread_modes[] = { D3DCREATE_MULTITHREADED, 0 };
    DWORD vertex_modes[] = { D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE,
                             D3DCREATE_HARDWARE_VERTEXPROCESSING,
                             D3DCREATE_MIXED_VERTEXPROCESSING,
                             D3DCREATE_SOFTWARE_VERTEXPROCESSING };

    for (size_t t = 0; t < ARRAY_SIZE(thread_modes); t++)
    {
        for (size_t v = 0; v < ARRAY_SIZE(vertex_modes); v++)
        {
            DWORD creationFlags = thread_modes[t] | vertex_modes[v];
            
            hr = m_pD3D9->CreateDevice(AdapterToUse,
                                       DeviceType, hWnd,
                                       creationFlags,
                                       pD3DPP, &m_pD3DD9);
            if (SUCCEEDED(hr))
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

BOOL CD3DVideoRender::initImageBuffer(int w, int h, int videofmt)
{
    RECT rect;
    GetClientRect(m_hWnd, &rect);
		
    m_srcRect.left = m_srcRect.top = 0;
	m_srcRect.right = w;
	m_srcRect.bottom = h;

	m_dstRect.left = rect.left;
	m_dstRect.top = rect.top;
	m_dstRect.right = rect.right;
	m_dstRect.bottom = rect.bottom;

	m_verPoint[0] = D3DXVECTOR4(m_dstRect.left-0.5f,  m_dstRect.top-0.5f,    0.0f, 1.0f);
	m_verPoint[1] = D3DXVECTOR4(m_dstRect.right-0.5f, m_dstRect.top-0.5f,    0.0f, 1.0f);
	m_verPoint[2] = D3DXVECTOR4(m_dstRect.right-0.5f, m_dstRect.bottom-0.5f, 0.0f, 1.0f);
	m_verPoint[3] = D3DXVECTOR4(m_dstRect.left-0.5f,  m_dstRect.bottom-0.5f, 0.0f, 1.0f);

    float dx = 1.0f / w;
	float dy = 1.0f / h;
	
	m_texPoint[0] = D3DXVECTOR2(dx * m_srcRect.left,  dy * m_srcRect.top);
	m_texPoint[1] = D3DXVECTOR2(dx * m_srcRect.right, dy * m_srcRect.top);
	m_texPoint[2] = D3DXVECTOR2(dx * m_srcRect.right, dy * m_srcRect.bottom);
	m_texPoint[3] = D3DXVECTOR2(dx * m_srcRect.left,  dy * m_srcRect.bottom);
		
	HRESULT hr = m_pD3DD9->CreateVertexBuffer(4 * sizeof(YUV_VERTEX), 
					D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, 
	                D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX3, D3DPOOL_DEFAULT, &m_pVertices, NULL);
	if (FAILED(hr))
	{
	    log_print(HT_LOG_ERR, "%s, CreateVertexBuffer failed\r\n", __FUNCTION__);
	    goto done; 
	}

    int i;
	YUV_VERTEX * pVB;

	hr = m_pVertices->Lock(0, 0, (void**)&pVB, 0);
	if (FAILED(hr)) 
	{
	    goto done; 
	}

	for (i = 0; i < 4; i++)
	{
		pVB[i].pos = m_verPoint[i];
		pVB[i].color = m_dwColor;
		pVB[i].tex1 = pVB[i].tex2 = pVB[i].tex3 = m_texPoint[i];
	}
	
	hr = m_pVertices->Unlock();
	if (FAILED(hr))
	{
	    goto done; 
	}
	
	for (i = 0; i < 3; i++)
	{
		int Width = w;
		int Height = h;
		
		if (i != 0)
		{
			Width  /= 2;
			Height /= 2;
		}

		hr = m_pD3DD9->CreateTexture(Width, Height, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &m_pTexture[i], NULL);
		if (FAILED(hr))
		{
		    log_print(HT_LOG_ERR, "%s, CreateTexture failed\r\n", __FUNCTION__);
		    goto done; 
		}
	}

	m_pTexTemp[Y_TEX] = m_pTexture[Y_TEX];
	
	if (VIDEO_FMT_YV12 == videofmt)
	{
		m_pTexTemp[U_TEX] = m_pTexture[V_TEX];
		m_pTexTemp[V_TEX] = m_pTexture[U_TEX];
	}
	else if (VIDEO_FMT_YUV420P == videofmt)
	{
		m_pTexTemp[U_TEX] = m_pTexture[U_TEX];
		m_pTexTemp[V_TEX] = m_pTexture[V_TEX];
	}

	return TRUE;
	
done:

	return FALSE;
}

void CD3DVideoRender::freeImageBuffer()
{
    if (m_pVertices) 
	{ 
    	m_pVertices->Release(); 
    	m_pVertices = NULL; 
	}

	for (int i = 0 ; i < COUNT_YUV_TEX; i++)
	{
		if (m_pTexture[i]) 
		{ 
		    m_pTexture[i]->Release(); 
		    m_pTexture[i] = NULL; 
		}
	}
}

BOOL CD3DVideoRender::initShader(const DWORD * pCode3, const DWORD * pCode2)
{
	D3DCAPS9 caps;
	HRESULT hr = S_OK;

	hr = m_pD3DD9->GetDeviceCaps(&caps);
	if (FAILED(hr))
	{
	    goto done; 
	}

	const DWORD * pCode;
	
	if (caps.PixelShaderVersion >= D3DPS_VERSION(3, 0))
	{
		pCode = pCode3;
	}
	else
	{
		pCode = pCode2;
	}

	hr = m_pD3DD9->CreatePixelShader(pCode, &m_pPixelShader);
	if (FAILED(hr))
	{
	    goto done; 
	}

	hr = D3DXGetShaderConstantTable(pCode, &m_pPixelConstTable);
	if (FAILED(hr))
	{
	    goto done; 
	}

	m_hYUVTexture[Y_TEX] = m_pPixelConstTable->GetConstantByName(NULL, "YTextue");
	if (m_hYUVTexture[Y_TEX] == NULL)
	{
		goto done;
	}

	m_hYUVTexture[U_TEX] = m_pPixelConstTable->GetConstantByName(NULL, "UTextue");
	if (m_hYUVTexture[U_TEX] == NULL)
	{
		goto done;
	}

	m_hYUVTexture[V_TEX] = m_pPixelConstTable->GetConstantByName(NULL, "VTextue");
	if (m_hYUVTexture[V_TEX] == NULL)
	{
		goto done;
	}

	UINT count;
	
	hr = m_pPixelConstTable->GetConstantDesc(m_hYUVTexture[Y_TEX], &m_dYUVTexture[Y_TEX], &count);
	if (FAILED(hr))
	{
	    goto done; 
	}
	
	hr = m_pPixelConstTable->GetConstantDesc(m_hYUVTexture[U_TEX], &m_dYUVTexture[U_TEX], &count);
	if (FAILED(hr))
	{
	    goto done; 
	}
	
	hr = m_pPixelConstTable->GetConstantDesc(m_hYUVTexture[V_TEX], &m_dYUVTexture[V_TEX], &count);
	if (FAILED(hr))
	{
	    goto done; 
	}

	m_hTransparent = m_pPixelConstTable->GetConstantByName(NULL, "transparent");
	if (m_hTransparent == NULL)
	{
		goto done;
	}

	hr = m_pPixelConstTable->SetDefaults(m_pD3DD9);

done:

    return SUCCEEDED(hr) ? TRUE : FALSE;
}

BOOL CD3DVideoRender::resetDevice()
{
    freeImageBuffer();

	if (m_pPixelShader)
	{ 
	    m_pPixelShader->Release(); 
	    m_pPixelShader = NULL; 
	}

	if (m_pPixelConstTable) 
	{ 
	    m_pPixelConstTable->Release(); 
	    m_pPixelConstTable = NULL; 
	}

	HRESULT hr;
	D3DPRESENT_PARAMETERS D3DPP;
    
    initD3DPP(&D3DPP, m_hWnd);

	hr = m_pD3DD9->Reset(&D3DPP);
    if (FAILED(hr))
    {
        return FALSE;    
    }
    
	hr = m_pD3DD9->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX3);
    if (FAILED(hr))
    {
        log_print(HT_LOG_ERR, "%s, SetFVF failed\r\n", __FUNCTION__);
        return FALSE;
	}
	
	if (initShader(g_YUV420ps30_main, g_YUV420ps20_main) == FALSE)
	{
	    log_print(HT_LOG_ERR, "%s, initShader failed\r\n", __FUNCTION__);
		return FALSE;
	}
    
    hr = m_pD3DD9->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    if (FAILED(hr))
    {
        log_print(HT_LOG_ERR, "%s, Clear failed\r\n", __FUNCTION__);
        return FALSE;    
    }

    return initImageBuffer(m_nVideoWidth, m_nVideoHeight, m_nVideoFmt);
}

BOOL CD3DVideoRender::setImageArgs(float Transparent, GEOMETRICTRANSFORMATION flip, RECT * pDstRect, RECT * pSrcRect)
{
	BOOL bSet = FALSE;	
	
	if (m_dTransparent != Transparent)
	{
		m_dTransparent = Transparent;
		m_dwColor = ((DWORD)( 255 * Transparent ) << 24) | 0x00ffffff;
		bSet = TRUE;
	}

	if (memcmp(&m_dstRect, pDstRect, sizeof(RECT)) != 0)
	{
		memcpy(&m_dstRect, pDstRect, sizeof(RECT));
		
		m_verPoint[0] = D3DXVECTOR4(m_dstRect.left-0.5f,  m_dstRect.top-0.5f,	0.0f, 1.0f);
		m_verPoint[1] = D3DXVECTOR4(m_dstRect.right-0.5f, m_dstRect.top-0.5f,	0.0f, 1.0f);
		m_verPoint[2] = D3DXVECTOR4(m_dstRect.right-0.5f, m_dstRect.bottom-0.5f, 0.0f, 1.0f);
		m_verPoint[3] = D3DXVECTOR4(m_dstRect.left-0.5f,  m_dstRect.bottom-0.5f, 0.0f, 1.0f);
		bSet = TRUE;
	}

	if (memcmp(&m_srcRect, pSrcRect, sizeof(RECT)) != 0 || flip != m_nFlip)
	{
	    float dx = 1.0f / m_nVideoWidth;
	    float dy = 1.0f / m_nVideoHeight;
	
		memcpy(&m_srcRect, pSrcRect, sizeof(RECT));
		
		m_texPoint[0] = D3DXVECTOR2(dx * m_srcRect.left, dy * m_srcRect.top);
		m_texPoint[1] = D3DXVECTOR2(dx * m_srcRect.right, dy * m_srcRect.top);
		m_texPoint[2] = D3DXVECTOR2(dx * m_srcRect.right, dy * m_srcRect.bottom);
		m_texPoint[3] = D3DXVECTOR2(dx * m_srcRect.left, dy * m_srcRect.bottom);

		if (flip & GS_UPPER_DOWN_FLIP)
		{
			D3DXVECTOR2 temp = m_texPoint[0];
			m_texPoint[0] = m_texPoint[3];
			m_texPoint[3] = temp;

			temp = m_texPoint[1];
			m_texPoint[1] = m_texPoint[2];
			m_texPoint[2] = temp;
		}

		if (flip & GS_LEFT_RIGHT_FLIP)
		{
			D3DXVECTOR2 temp = m_texPoint[0];
			m_texPoint[0] = m_texPoint[1];
			m_texPoint[1] = temp;

			temp = m_texPoint[2];
			m_texPoint[2] = m_texPoint[3];
			m_texPoint[3] = temp;
		}
	
		m_nFlip = flip;
		bSet = TRUE;
	}
	
	return bSet;
}

BOOL CD3DVideoRender::setImage(GEOMETRICTRANSFORMATION flip, RECT * pDstRect, RECT * pSrcRect)
{
	if (!setImageArgs(1.0f, flip, pDstRect, pSrcRect))
	{
		return TRUE;
	}

	YUV_VERTEX * pVB;
	HRESULT hr = m_pVertices->Lock(0, 0, (void**)&pVB, D3DLOCK_DISCARD);
	if (FAILED(hr))
	{
		return FALSE;
	}

	for (int i = 0 ; i < 4 ; i++)
	{
		pVB[i].pos = m_verPoint[i];
		pVB[i].color = m_dwColor;
		pVB[i].tex1 = pVB[i].tex2 = pVB[i].tex3 = m_texPoint[i];
	}

	hr = m_pVertices->Unlock();

	if (FAILED(hr))
	{
		return FALSE;
	}

	return TRUE;
}

BOOL CD3DVideoRender::render(AVFrame * frame, int mode)
{
    HRESULT hr = S_OK;
    
    if (m_bReset)
    {
        if (resetDevice())
        {
            m_bReset = FALSE;
        }
    }
    
    if (!m_bInited || m_bReset || NULL == frame)
    {
        return FALSE;
    }

	if (m_pD3DD9)
	{
		hr = m_pD3DD9->TestCooperativeLevel();    
	}
	else
	{
		return FALSE; 
	}

    if (FAILED(hr))
    {
        log_print(HT_LOG_ERR, "%s, TestCooperativeLevel failed. (hr=0x%0lx)\r\n", __FUNCTION__, hr);
        goto done;
    }

    if (m_nVideoWidth != frame->width || m_nVideoHeight != frame->height)
	{
	    freeImageBuffer();
	    
	    if (!initImageBuffer(frame->width, frame->height, m_nVideoFmt))
	    {
	        log_print(HT_LOG_ERR, "%s, initImageBuffer failed\r\n", __FUNCTION__);
	        return FALSE;
	    }

	    m_nVideoWidth = frame->width;
	    m_nVideoHeight = frame->height;
	}

	RECT rect;
    GetClientRect(m_hWnd, &rect);

    RECT targetRect;
    RECT sourceRect;
		
	sourceRect.left = 0;
	sourceRect.top = 0;
	sourceRect.right = frame->width;
	sourceRect.bottom = frame->height;
		
	if (mode == RENDER_MODE_KEEP)
	{
	    int rectw = rect.right - rect.left;
	    int recth = rect.bottom - rect.top;
	    
		double dw = rectw / (double) m_nVideoWidth;
		double dh = recth / (double) m_nVideoHeight;
		double rate = (dw > dh)? dh : dw;

		int rw = (int)(m_nVideoWidth * rate);
		int rh = (int)(m_nVideoHeight * rate);

		targetRect.left = (rectw - rw) / 2;
		targetRect.top  = (recth - rh) / 2;
		targetRect.right = targetRect.left + rw;
		targetRect.bottom = targetRect.top + rh;
	}
	else
	{
		targetRect = rect;
	}

	for (int i = 0 ; i < 3 ; i++)
	{
	    int w = m_nVideoWidth;
	    int h = m_nVideoHeight;

	    if (i != 0)
	    {
	        w /= 2;
	        h /= 2;
	    }
	    
		BYTE * pSrc = frame->data[i];
		D3DLOCKED_RECT lrect;

		hr = m_pTexture[i]->LockRect(0, &lrect, NULL, D3DLOCK_DISCARD);
		if (SUCCEEDED(hr))
		{
			BYTE * pDest = (BYTE*) lrect.pBits;

			for (int j = 0 ; j < h; j++)
			{
				memcpy(pDest, pSrc, w);
				
				pDest += lrect.Pitch;
				pSrc += frame->linesize[i];
			}

			hr = m_pTexture[i]->UnlockRect(0);
			if (FAILED(hr)) 
			{
			    goto done; 
			}
		}
		else
		{
			if (FAILED(hr)) 
			{
			    log_print(HT_LOG_ERR, "%s, LockRect failed\r\n", __FUNCTION__);
			    goto done; 
			}
		}
	}

	// begin draw
	hr = m_pD3DD9->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_pRenderSurface);
	if (SUCCEEDED(hr))
	{
		m_pD3DD9->ColorFill(m_pRenderSurface, &rect, D3DCOLOR_ARGB(0xff,0x0,0x0,0x0));
		m_pD3DD9->SetRenderTarget(0, m_pRenderSurface);
		m_pRenderSurface->Release();
	}
	
	hr = m_pD3DD9->BeginScene();
	if (FAILED(hr))
	{
	    log_print(HT_LOG_ERR, "%s, BeginScene failed\r\n", __FUNCTION__);
	    goto done; 
	}

	hr = m_pD3DD9->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX3);
	if (FAILED(hr)) 
	{
	    log_print(HT_LOG_ERR, "%s, SetFVF failed\r\n", __FUNCTION__);
	    goto done; 
	}
	
	hr = m_pD3DD9->SetPixelShader(m_pPixelShader);
	if (FAILED(hr)) 
	{
	    log_print(HT_LOG_ERR, "%s, SetPixelShader failed\r\n", __FUNCTION__);
	    goto done; 
	}

	hr = m_pPixelConstTable->SetFloat(m_pD3DD9, m_hTransparent, 1.0);
	if (FAILED(hr)) 
	{
	    log_print(HT_LOG_ERR, "%s, SetFloat failed\r\n", __FUNCTION__);
	    goto done; 
	} 

	setImage(GS_NONE, &targetRect, &sourceRect);

	hr = m_pD3DD9->SetTexture(m_dYUVTexture[Y_TEX].RegisterIndex, m_pTexTemp[Y_TEX]);
	if (FAILED(hr)) 
	{
	    log_print(HT_LOG_ERR, "%s, SetTexture failed\r\n", __FUNCTION__);
	    goto done; 
	}

	hr = m_pD3DD9->SetTexture(m_dYUVTexture[U_TEX].RegisterIndex, m_pTexTemp[U_TEX]);
	if (FAILED(hr)) 
	{
	    log_print(HT_LOG_ERR, "%s, SetTexture failed\r\n", __FUNCTION__);
	    goto done; 
	}

	hr = m_pD3DD9->SetTexture(m_dYUVTexture[V_TEX].RegisterIndex, m_pTexTemp[V_TEX]);
	if (FAILED(hr)) 
	{
	    log_print(HT_LOG_ERR, "%s, SetTexture failed\r\n", __FUNCTION__);
	    goto done; 
	}

	hr = m_pD3DD9->SetStreamSource(0, m_pVertices, 0, sizeof(YUV_VERTEX));
	if (FAILED(hr)) 
	{
	    log_print(HT_LOG_ERR, "%s, SetStreamSource failed\r\n", __FUNCTION__);
	    goto done; 
	}

	hr = m_pD3DD9->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
	if (FAILED(hr)) 
	{
	    log_print(HT_LOG_ERR, "%s, DrawPrimitive failed\r\n", __FUNCTION__);
	    goto done; 
	}

	hr = m_pD3DD9->EndScene();
	if (FAILED(hr)) 
	{
	    log_print(HT_LOG_ERR, "%s, EndScene failed\r\n", __FUNCTION__);
	    goto done; 
	}
	
    hr = m_pD3DD9->Present(&rect, &rect, NULL, NULL);

done:
    
    if (hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET)
	{
	    log_print(HT_LOG_DBG, "%s, device lost!\r\n", __FUNCTION__);
	    
	    m_bReset = TRUE;
		return FALSE;
	}
	
    return SUCCEEDED(hr) ? TRUE : FALSE;
}



