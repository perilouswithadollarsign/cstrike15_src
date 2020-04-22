//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose: XBox win32 stubs
//
// $NoKeywords: $
//=============================================================================
#pragma once

typedef void*   				IDirect3D9;
typedef void*                   IDirect3DDevice9;
typedef void*					IDirect3DVertexShader9;
typedef void*					IDirect3DPixelShader9;
typedef void*					IDirect3DVertexDeclaration9;
typedef void*                   IDirect3DTexture9;
typedef void*                   IDirect3DBaseTexture9;
typedef void*               	IDirect3DCubeTexture9;
typedef void*           		IDirect3DSurface9;
typedef void*               	IDirect3DIndexBuffer9;
typedef void*               	IDirect3DVertexBuffer9;
typedef void*               	IDirect3DVolumeTexture9;
typedef void					ID3DXFont;
typedef void*               	D3DADAPTER_IDENTIFIER9;
typedef void*   				D3DCAPS9;
typedef void*       			D3DMATERIAL9;
typedef void*       			D3DVIEWPORT9;
typedef void*   				D3DLIGHT9;
typedef void*					D3DVERTEXELEMENT9;

// not sure what behavior to emulate yet
#define D3DLOCK_NOSYSLOCK		0
#define D3DLOCK_DISCARD			0

#define D3DENUM_WHQL_LEVEL		0

// collapse sampler state back into texture state
typedef enum 
{
	D3DSAMP_ADDRESSU      = D3DTSS_ADDRESSU,
	D3DSAMP_ADDRESSV      = D3DTSS_ADDRESSV,
	D3DSAMP_ADDRESSW      = D3DTSS_ADDRESSW,
	D3DSAMP_MINFILTER     = D3DTSS_MINFILTER,
	D3DSAMP_MAGFILTER     = D3DTSS_MAGFILTER,
	D3DSAMP_MIPFILTER     = D3DTSS_MIPFILTER,
	D3DSAMP_MAXANISOTROPY = D3DTSS_MAXANISOTROPY,
} D3DSAMPLERSTATETYPE;
