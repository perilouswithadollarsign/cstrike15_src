//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//	LibGcm implementation of DX
//
//==================================================================================================

#ifndef SPU
#define CELL_GCM_MEMCPY memcpy						// PPU SNC has no such intrinsic
#endif

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
#include "dxabstract.h"
#include "materialsystem/imaterialsystem.h"

#include "ps3gcmmemory.h"
#include "gcmtexture.h"
#include "gcmstate.h"
#include "gcmdrawstate.h"

#include "vprof.h"

#include "cgutils.h"

#include "tier0/memdbgon.h"


extern IDirect3DDevice9 *m_pD3DDevice;

//--------------------------------------------------------------------------------------------------
//				Direct3DCreate9
//
// IDirect3D9:: GetAdapterCount
// 				GetAdapterIdentifier
// 				CheckDeviceFormat
// 				GetAdapterModeCount
// 				EnumAdapterModes
// 				CheckDeviceType
// 				GetAdapterDisplayMode
// 				CheckDepthStencilMatch
// 				CheckDeviceMultiSampleType
// 				CreateDevice
//				BeginZcullMeasurement
//				EndZcullMeasurement
//--------------------------------------------------------------------------------------------------

IDirect3D9 *Direct3DCreate9(UINT SDKVersion)
{
	return new IDirect3D9;
}

UINT IDirect3D9::GetAdapterCount()
{	
	return 1;		//FIXME revisit later when we know how many screems there are
}

HRESULT IDirect3D9::GetDeviceCaps(UINT Adapter,D3DDEVTYPE DeviceType,D3DCAPS9* pCaps)
{
	// Generally called from "CShaderDeviceMgrDx8::ComputeCapsFromD3D" in ShaderDeviceDX8.cpp
	
	//cheese: fill in the pCaps record for adapter... we zero most of it and just fill in the fields that we think the caller wants.
	Q_memset( pCaps, 0, sizeof(*pCaps) );
	

    /* Device Info */
	pCaps->DeviceType					=	D3DDEVTYPE_HAL;

    /* Caps from DX7 Draw */
    pCaps->Caps							=	0;									// does anyone look at this ?
	
    pCaps->Caps2						=	D3DCAPS2_DYNAMICTEXTURES;    
    /* Cursor Caps */
    pCaps->CursorCaps					=	0;									// nobody looks at this

    /* 3D Device Caps */
    pCaps->DevCaps						=	D3DDEVCAPS_HWTRANSFORMANDLIGHT;

    pCaps->TextureCaps					=	D3DPTEXTURECAPS_CUBEMAP | D3DPTEXTURECAPS_MIPCUBEMAP | D3DPTEXTURECAPS_NONPOW2CONDITIONAL | D3DPTEXTURECAPS_PROJECTED;
											// D3DPTEXTURECAPS_NOPROJECTEDBUMPENV ?
											// D3DPTEXTURECAPS_POW2 ? 
	// caller looks at POT support like this:
	//		pCaps->m_SupportsNonPow2Textures = 
	//			( !( caps.TextureCaps & D3DPTEXTURECAPS_POW2 ) || 
	//			( caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL ) );
	// so we should set D3DPTEXTURECAPS_NONPOW2CONDITIONAL bit ?


    pCaps->PrimitiveMiscCaps			=	0;									//only the HDR setup looks at this for D3DPMISCCAPS_SEPARATEALPHABLEND.
		// ? D3DPMISCCAPS_SEPARATEALPHABLEND 
		// ? D3DPMISCCAPS_BLENDOP
		// ? D3DPMISCCAPS_CLIPPLANESCALEDPOINTS
		// ? D3DPMISCCAPS_CLIPTLVERTS D3DPMISCCAPS_COLORWRITEENABLE D3DPMISCCAPS_MASKZ D3DPMISCCAPS_TSSARGTEMP
		
		
    pCaps->RasterCaps					=	D3DPRASTERCAPS_SCISSORTEST
										|	D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS	// ref'd in CShaderDeviceMgrDx8::ComputeCapsFromD3D
										|	D3DPRASTERCAPS_DEPTHBIAS			// ref'd in CShaderDeviceMgrDx8::ComputeCapsFromD3D
										;
		
    pCaps->TextureFilterCaps			=	D3DPTFILTERCAPS_MINFANISOTROPIC | D3DPTFILTERCAPS_MAGFANISOTROPIC;
    
    pCaps->MaxTextureWidth				=	4096;
	pCaps->MaxTextureHeight				=	4096;
    pCaps->MaxVolumeExtent				=	1024;	//guesses

    pCaps->MaxTextureAspectRatio		=	0;		// imply no limit on AR

    pCaps->MaxAnisotropy				=	8;		//guess
    
    pCaps->TextureOpCaps				=	D3DTEXOPCAPS_ADD | D3DTEXOPCAPS_MODULATE2X;	//guess
    DWORD   MaxTextureBlendStages;
    DWORD   MaxSimultaneousTextures;

	pCaps->VertexProcessingCaps			=	D3DVTXPCAPS_TEXGEN_SPHEREMAP;
	
    pCaps->MaxActiveLights				=	8;		// guess
    pCaps->MaxUserClipPlanes			=	0;		// guess until we know better
    pCaps->MaxVertexBlendMatrices		=	0;		// see if anyone cares
    pCaps->MaxVertexBlendMatrixIndex	=	0;		// see if anyone cares

    pCaps->MaxPrimitiveCount			=	32768;	// guess
    pCaps->MaxStreams					=	4;		// guess

    pCaps->VertexShaderVersion			=	0x200;	// model 2.0
    pCaps->MaxVertexShaderConst			=	256;	// number of vertex shader constant registers

    pCaps->PixelShaderVersion			=	0x200;	// model 2.0

    // Here are the DX9 specific ones
    pCaps->DevCaps2						=	0;		// D3DDEVCAPS2_STREAMOFFSET - leave it off
	
    pCaps->PS20Caps.NumInstructionSlots	=	512;	// guess
	// only examined once:
	// pCaps->m_SupportsPixelShaders_2_b = ( ( caps.PixelShaderVersion & 0xffff ) >= 0x0200) && (caps.PS20Caps.NumInstructionSlots >= 512);
	//pCaps->m_SupportsPixelShaders_2_b = 1;
	
	pCaps->NumSimultaneousRTs					=	1;         // Will be at least 1
    pCaps->MaxVertexShader30InstructionSlots	=	0; 
    pCaps->MaxPixelShader30InstructionSlots		=	0;

	return S_OK;
}

HRESULT IDirect3D9::GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier)
{
	// Generally called from "CShaderDeviceMgrDx8::ComputeCapsFromD3D" in ShaderDeviceDX8.cpp


	Assert( Adapter==0 );					// only one adapter now
	Assert( Flags == D3DENUM_WHQL_LEVEL );	// we're not handling any other queries than this yet
	
	Q_memset( pIdentifier, 0, sizeof(*pIdentifier) );

	// this came from the shaderapigl effort	
    Q_strncpy( pIdentifier->Driver, "Fake-Video-Card", MAX_DEVICE_IDENTIFIER_STRING );
    Q_strncpy( pIdentifier->Description, "Fake-Video-Card", MAX_DEVICE_IDENTIFIER_STRING );
    pIdentifier->VendorId				= 4318;
    pIdentifier->DeviceId				= 401;
    pIdentifier->SubSysId				= 3358668866;
    pIdentifier->Revision				= 162;

	return S_OK;
}

HRESULT IDirect3D9::CheckDeviceFormat(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,DWORD Usage,D3DRESOURCETYPE RType,D3DFORMAT CheckFormat)
{
	HRESULT result = -1;	// failure
	
	DWORD	knownUsageMask =	D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL | D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP
							|	D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_FILTER | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING
							|	D3DUSAGE_QUERY_VERTEXTEXTURE;
	
	Assert ((Usage & knownUsageMask) == Usage);
	
	DWORD	legalUsage = 0;
	switch( AdapterFormat )
	{
		case	D3DFMT_X8R8G8B8:
			switch( RType )
			{
				case	D3DRTYPE_TEXTURE:
					switch( CheckFormat )
					{
						case D3DFMT_DXT1:
						case D3DFMT_DXT3:
						case D3DFMT_DXT5:
													legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage	|=	D3DUSAGE_QUERY_SRGBREAD;
													
													//open question: is auto gen of mipmaps is allowed or attempted on any DXT textures.
						break;
						
						case D3DFMT_A8R8G8B8:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage |=	D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
						break;
						
						case D3DFMT_R32F:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage |=	D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
						break;
						
						case D3DFMT_A16B16G16R16:	legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP /* --- OSX specific? --- | D3DUSAGE_QUERY_FILTER --- */;
													legalUsage |=	IsPS3() ? D3DUSAGE_QUERY_FILTER : 0;
													legalUsage |=	D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
						break;
						
						case D3DFMT_A16B16G16R16F:	legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage |=	D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
						
						case D3DFMT_A32B32G32R32F:	legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage |=	D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
						break;
						
						case D3DFMT_R5G6B5:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						//-----------------------------------------------------------
						// these come in from TestTextureFormat in ColorFormatDX8.cpp which is being driven by InitializeColorInformation...
						// which is going to try all 8 combinations of (vertex texturable / render targetable / filterable ) on every image format it knows.
						 
						case D3DFMT_R8G8B8:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage	|=	D3DUSAGE_QUERY_SRGBREAD;
						break;
												
						case D3DFMT_X8R8G8B8:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage	|=	D3DUSAGE_QUERY_SRGBREAD;
						break;

							// one and two channel textures... we'll have to fake these as four channel tex if we want to support them
						case D3DFMT_L8:				legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						case D3DFMT_A8L8:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						case D3DFMT_A8:				legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;
						
							// going to need to go back and double check all of these..
						case D3DFMT_X1R5G5B5:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;
						
						case D3DFMT_A4R4G4B4:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						case D3DFMT_A1R5G5B5:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;
						
						case D3DFMT_V8U8:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						case D3DFMT_Q8W8V8U8:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
							// what the heck is QWVU8 ... ?
						break;

						case D3DFMT_X8L8V8U8:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
							// what the heck is XLVU8 ... ?
						break;

							// formats with depth...
							
						case	D3DFMT_D16:			legalUsage =	D3DUSAGE_DYNAMIC | D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL;
							// just a guess on the legal usages
						break;

						case	D3DFMT_D24S8:		legalUsage =	D3DUSAGE_DYNAMIC | D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL;
							// just a guess on the legal usages
						break;

							// vendor formats... try marking these all invalid for now
						case	D3DFMT_NV_INTZ:
						case	D3DFMT_NV_RAWZ:
						case	D3DFMT_NV_NULL:
						case	D3DFMT_ATI_D16:
						case	D3DFMT_ATI_D24S8:
						case	D3DFMT_ATI_2N:
						case	D3DFMT_ATI_1N:
							legalUsage = 0;
						break;

						//-----------------------------------------------------------
												
						default:
							Assert(!"Unknown check format");
							result = -1;
						break;
					}

					if ((Usage & legalUsage) == Usage)
					{
						//NC(( " --> OK!" ));
						result = S_OK;
					}
					else
					{
						DWORD unsatBits = Usage & (~legalUsage);	// clear the bits of the req that were legal, leaving the illegal ones
						//NC(( " --> NOT OK: flags %8x:%s", unsatBits,GLMDecodeMask( eD3D_USAGE, unsatBits ) ));
						result = -1;
					}
				break;				
				
				case	D3DRTYPE_SURFACE:
					switch( CheckFormat )
					{
					case D3DFMT_D16:
					case D3DFMT_D24S8:		
						legalUsage =	D3DUSAGE_DYNAMIC | D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL;
						if ((Usage & legalUsage) == Usage)
						{
							result = S_OK;
						}
						break;

						case	0x434f5441:
						case	0x41415353:
							result = -1;
						break;
						//** IDirect3D9::CheckDeviceFormat  adapter=0, DeviceType=   1:D3DDEVTYPE_HAL, AdapterFormat=       5:D3DFMT_X8R8G8B8, RType=   1:D3DRTYPE_SURFACE, CheckFormat=434f5441:UNKNOWN
						//** IDirect3D9::CheckDeviceFormat  adapter=0, DeviceType=   1:D3DDEVTYPE_HAL, AdapterFormat=       5:D3DFMT_X8R8G8B8, RType=   1:D3DRTYPE_SURFACE, CheckFormat=41415353:UNKNOWN
						//** IDirect3D9::CheckDeviceFormat  adapter=0, DeviceType=   1:D3DDEVTYPE_HAL, AdapterFormat=       5:D3DFMT_X8R8G8B8, RType=   1:D3DRTYPE_SURFACE, CheckFormat=434f5441:UNKNOWN
						//** IDirect3D9::CheckDeviceFormat  adapter=0, DeviceType=   1:D3DDEVTYPE_HAL, AdapterFormat=       5:D3DFMT_X8R8G8B8, RType=   1:D3DRTYPE_SURFACE, CheckFormat=41415353:UNKNOWN
					}
				break;
				
				default:
					Assert(!"Unknown resource type");
					result = -1;
				break;
			}
		break;
		
		default:
			Assert(!"Unknown adapter format");
			result = -1;
		break;
	}
	
	return result;
}

UINT IDirect3D9::GetAdapterModeCount(UINT Adapter,D3DFORMAT Format)
{
	return 1;
}

HRESULT IDirect3D9::EnumAdapterModes(UINT Adapter,D3DFORMAT Format,UINT Mode,D3DDISPLAYMODE* pMode)
{

	if ( IsPC() )
	{
		pMode->Width		= 1024;
		pMode->Height		= 768;
	}
	else
	{
		pMode->Width = 640;
		pMode->Height = 480;
	}
	pMode->RefreshRate	= 0;		// "adapter default"
	pMode->Format		= Format;

	return S_OK;
}

HRESULT IDirect3D9::CheckDeviceType(UINT Adapter,D3DDEVTYPE DevType,D3DFORMAT AdapterFormat,D3DFORMAT BackBufferFormat,BOOL bWindowed)
{
	return S_OK;
}

HRESULT IDirect3D9::GetAdapterDisplayMode(UINT Adapter,D3DDISPLAYMODE* pMode)
{

	pMode->Width		= 1024;
	pMode->Height		= 768;
	pMode->RefreshRate	= 0;		// "adapter default"
	pMode->Format		= D3DFMT_X8R8G8B8;

	return S_OK;
}

HRESULT IDirect3D9::CheckDepthStencilMatch(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,D3DFORMAT RenderTargetFormat,D3DFORMAT DepthStencilFormat)
{
	// one known request looks like this:
	// AdapterFormat=5:D3DFMT_X8R8G8B8 || RenderTargetFormat=3:D3DFMT_A8R8G8B8 || DepthStencilFormat=2:D3DFMT_D24S8
	
	// return S_OK for that one combo, Debugger() on anything else
	HRESULT result = -1;	// failure
	
	switch( AdapterFormat )
	{
		case	D3DFMT_X8R8G8B8:
		{
			if ( (RenderTargetFormat == D3DFMT_A8R8G8B8) && (DepthStencilFormat == D3DFMT_D24S8) )
			{
				result = S_OK;
			}
		}
		break;
	}
	
	Assert( result == S_OK );

	return result;
}

HRESULT IDirect3D9::CheckDeviceMultiSampleType(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT SurfaceFormat,BOOL Windowed,D3DMULTISAMPLE_TYPE MultiSampleType,DWORD* pQualityLevels)
{
	switch( MultiSampleType )
	{
		case D3DMULTISAMPLE_NONE:
			*pQualityLevels = 1;
			return S_OK;
		break;
		
		default:
			if(pQualityLevels)
			{
				*pQualityLevels = 0;
			}
			return D3DERR_NOTAVAILABLE;
		break;
	}
	return D3DERR_NOTAVAILABLE;
}

HRESULT IDirect3D9::CreateDevice(UINT Adapter,D3DDEVTYPE DeviceType,VD3DHWND hFocusWindow,DWORD BehaviorFlags,
								 D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DDevice9** ppReturnedDeviceInterface)
{
	// constrain these inputs for the time being
	// BackBufferFormat			-> A8R8G8B8
	// BackBufferCount			-> 1;
	// MultiSampleType			-> D3DMULTISAMPLE_NONE
	// AutoDepthStencilFormat	-> D3DFMT_D24S8
	
	// NULL out the return pointer so if we exit early it is not set
	*ppReturnedDeviceInterface = NULL;
	
	//extern void	UnpackD3DRSITable( void );
	//UnpackD3DRSITable();
	
	// assume success unless something is sour
	HRESULT result = S_OK;
	
	// relax this check for now
	//if (pPresentationParameters->BackBufferFormat != D3DFMT_A8R8G8B8)
	//{
	//	Debugger();
	//	result = -1;
	//}
	
	if (pPresentationParameters->BackBufferCount != 1)
	{
		Debugger();
		result = -1;
	}
	
	if (pPresentationParameters->MultiSampleType != D3DMULTISAMPLE_NONE)
	{
		Debugger();
		result = -1;
	}
	
	if (pPresentationParameters->AutoDepthStencilFormat != D3DFMT_D24S8)
	{
		Debugger();
		result = -1;
	}

	if (result == S_OK)
	{
		// create an IDirect3DDevice9
		// make a GLMContext and set up some drawables

		IDirect3DDevice9Params	devparams;
		memset( &devparams, 0, sizeof(devparams) );
		
		devparams.m_adapter				= Adapter;
		devparams.m_deviceType				= DeviceType;
		devparams.m_focusWindow			= hFocusWindow;
		devparams.m_behaviorFlags			= BehaviorFlags;
		devparams.m_presentationParameters	= *pPresentationParameters;

		IDirect3DDevice9 *dev = new IDirect3DDevice9;
		
		result = dev->Create( &devparams );
		
		if (result == S_OK)
		{
			*ppReturnedDeviceInterface = dev;
		}
	}
	
	return result;
}

//--------------------------------------------------------------------------------------------------
// IDirect3DDevice9::	Create
// 						Release
// 						Reset
// 						SetViewport
// 						BeginScene
// 						EndScene
// 						Present
// 						Ps3Helper_ResetSurfaceToKnownDefaultState
//						ReloadZcullMemory
//--------------------------------------------------------------------------------------------------

HRESULT	IDirect3DDevice9::Create( IDirect3DDevice9Params *params )
{
	HRESULT result = S_OK;

  	// create an IDirect3DDevice9
 	// make a GLMContext and set up some drawables
 	m_params		=	*params;
 
 	V_memset( m_rtSurfaces, 0, sizeof(m_rtSurfaces) );
 	m_dsSurface = NULL;
 
 	m_defaultColorSurface = NULL;
 	m_defaultDepthStencilSurface = NULL;
 
 	// Init GCM
 	int nGcmInitError = g_ps3gcmGlobalState.Init();
 	if( nGcmInitError < CELL_OK )
 	{
 		Debugger();	// bad news
 		Warning( "IDirect3DDevice9::Create error 0x%X", nGcmInitError );
 		return (HRESULT) -1;
 	}
 
	gpGcmDrawState->Init(params);

 	// we create two IDirect3DSurface9's.  These will be known as the internal render target 0 and the depthstencil.
 
 	// color surface
 	result = this->CreateRenderTarget( 
 		m_params.m_presentationParameters.BackBufferWidth,	// width
 		m_params.m_presentationParameters.BackBufferHeight,	// height
 		m_params.m_presentationParameters.BackBufferFormat, // format
 		D3DMULTISAMPLE_NONE,								// FIXME
 		0,													// MSAA quality
 		true,												// lockable ????
 		&m_defaultColorSurface,								// ppSurface
 		(VD3DHANDLE*)this									// shared handle, signal that it is internal RT
 		);

	if (result != S_OK)
	{
		Debugger();
		return result;
	}
	else
	{
		m_defaultColorSurface->AddRef();
	}
 
	if (!m_params.m_presentationParameters.EnableAutoDepthStencil)
	{
		Debugger();	// bad news
	}
 
	result = CreateDepthStencilSurface(
		m_params.m_presentationParameters.BackBufferWidth,	// width
		m_params.m_presentationParameters.BackBufferHeight,	// height
		m_params.m_presentationParameters.AutoDepthStencilFormat,	// format
		D3DMULTISAMPLE_NONE,								// FIXME
		0,													// MSAA quality
		TRUE,												// enable z-buffer discard ????
		&m_defaultDepthStencilSurface,						// ppSurface
		(VD3DHANDLE*)this									// shared handle, signal that it is internal RT
		);
	if (result != S_OK)
	{
		Debugger();
		return result;
	}
	else
	{
		m_defaultDepthStencilSurface->AddRef();
	}
 
	// Set the default surfaces
	(m_rtSurfaces[0] = m_defaultColorSurface)->AddRef();
	(m_dsSurface = m_defaultDepthStencilSurface)->AddRef();
	Ps3Helper_UpdateSurface( 0 );	// submit the change to GCM
 
	// Create internal textures

 	void Ps3gcmInitializeArtificialTexture( int iHandle, IDirect3DBaseTexture9 *pPtr );
 	Ps3gcmInitializeArtificialTexture( PS3GCM_ARTIFICIAL_TEXTURE_HANDLE_INDEX_BACKBUFFER, 
									   ( IDirect3DBaseTexture9 * ) m_defaultColorSurface );
 	Ps3gcmInitializeArtificialTexture( PS3GCM_ARTIFICIAL_TEXTURE_HANDLE_INDEX_DEPTHBUFFER, 
										( IDirect3DBaseTexture9 * ) m_defaultDepthStencilSurface );

	D3DVIEWPORT9 viewport = { 0, 0,
	 		m_params.m_presentationParameters.BackBufferWidth,
	 		m_params.m_presentationParameters.BackBufferHeight,
	 		0.1f, 1000.0f
	 	};
	
	SetViewport( &viewport );

 	Ps3Helper_ResetSurfaceToKnownDefaultState();

	return result;
}

ULONG __stdcall IDirect3DDevice9::Release()
{
 	g_ps3gcmGlobalState.Shutdown();
	return 0;
}

HRESULT IDirect3DDevice9::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	// nothing interesting here
	return S_OK;
}

HRESULT IDirect3DDevice9::SetViewport(CONST D3DVIEWPORT9* pViewport)
{
	return gpGcmDrawState->SetViewport(pViewport);
}

HRESULT IDirect3DDevice9::BeginScene()
{
	// 	m_isZPass = false;			// z pres pass off at this time
 	
	g_ps3gcmGlobalState.BeginScene();

	//m_nAntiAliasingStatus = AA_STATUS_NORMAL;			// AA FXAA
 
	return S_OK;
}

HRESULT IDirect3DDevice9::EndScene()
{
	g_ps3gcmGlobalState.EndScene();

	return S_OK;
}


HRESULT IDirect3DDevice9::Present(CONST RECT* pSourceRect,CONST RECT* pDestRect,VD3DHWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion)
{
 	m_nAntiAliasingStatus = AA_STATUS_NORMAL;// we aren't drawing into previous frame, we just set the current frame and we're about to set the surface for drawing

	// Flip frames and Advance display counters

	g_ps3gcmGlobalState.Flip();

 
 	// Set our new default color buffer offset
 	m_defaultColorSurface->m_tex->m_lmBlock.Assign( g_ps3gcmGlobalState.m_display.surfaceColor[g_ps3gcmGlobalState.m_display.surfaceFlipIdx] );
 
 	//
 	// Starting next frame
 	//
 
 	// Bind our default surfaces for the first time this frame here:
 	if ( m_rtSurfaces[0] != m_defaultColorSurface )
 	{
 		m_defaultColorSurface->AddRef();
 		if ( m_rtSurfaces[0] ) m_rtSurfaces[0]->Release();
 		m_rtSurfaces[0] = m_defaultColorSurface;
 	}
 	if ( m_dsSurface != m_defaultDepthStencilSurface )
 	{
 		m_defaultDepthStencilSurface->AddRef();
 		if ( m_dsSurface ) m_dsSurface->Release();
 		m_dsSurface = m_defaultDepthStencilSurface;
 	}
 	Ps3Helper_UpdateSurface( 0 );
 
 	Ps3Helper_ResetSurfaceToKnownDefaultState();

	return S_OK;
}

void IDirect3DDevice9::Ps3Helper_ResetSurfaceToKnownDefaultState()
{
	gpGcmDrawState->ResetSurfaceToKnownDefaultState();
}

//--------------------------------------------------------------------------------------------------
// IDirect3DGcmBufferBase - Lock, unlock, mem mgmt via CPs3gcmBuffer
//--------------------------------------------------------------------------------------------------

HRESULT IDirect3DGcmBufferBase::Lock(UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags)
{
	// FIXME would be good to have "can't lock twice" logic

	Assert( !(Flags & D3DLOCK_READONLY) );	// not impl'd
	//	Assert( !(Flags & D3DLOCK_NOSYSLOCK) );	// not impl'd - it triggers though

	if ( Flags & D3DLOCK_DISCARD )
	{
		// When the buffer is being discarded we need to allocate a new
		// instance of the buffer in case the current instance has been
		// in use:
		m_pBuffer->m_lmBlock.FreeAndAllocNew();
		gpGcmDrawState->UpdateVtxBufferOffset(( IDirect3DVertexBuffer9 * )this, m_pBuffer->Offset());
	}
	// We assume that we are always locking in NOOVERWRITE mode otherwise!

	// Return the buffer data pointer at requested offset
	*ppbData = m_pBuffer->m_lmBlock.DataInAnyMemory() + OffsetToLock;

	return S_OK;
}

HRESULT IDirect3DGcmBufferBase::Unlock()
{
	// if this buffer is dirty, we need to invalidate vertex cache when we bind it
	gpGcmDrawState->m_dirtyCachesMask |= CGcmDrawState::kDirtyVxCache;
	return S_OK;
}

//--------------------------------------------------------------------------------------------------
// TEXTURE
// -------
// PreparePs3TextureKey
// IDirect3DDevice9::	CreateTexture
//						CreateCubeTexture
//						CreateVolumeTexture
//						AllocateTextureStorage
//						SetTexture
//						GetTexture
//						FlushTextureCache
//
//--------------------------------------------------------------------------------------------------

static void PreparePs3TextureKey( CPs3gcmTextureLayout::Key_t &key, D3DFORMAT Format, UINT Levels, DWORD Usage )
{
	memset( &key, 0, sizeof(key) );
	key.m_texFormat		= Format;
	if ( Levels > 1 )
	{
		key.m_texFlags |= CPs3gcmTextureLayout::kfMip;
	}
	key.m_nActualMipCount = Levels;

	// http://msdn.microsoft.com/en-us/library/bb172625(VS.85).aspx

	// complain if any usage bits come down that I don't know.
	uint knownUsageBits = (D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_RENDERTARGET | D3DUSAGE_DYNAMIC | D3DUSAGE_TEXTURE_SRGB | D3DUSAGE_DEPTHSTENCIL | D3DUSAGE_TEXTURE_NOD3DMEMORY);
	if ( (Usage & knownUsageBits) != Usage )
	{
		Debugger();
	}

	if (Usage & D3DUSAGE_AUTOGENMIPMAP)
	{
		key.m_texFlags |= CPs3gcmTextureLayout::kfMip | CPs3gcmTextureLayout::kfMipAuto;
	}

	if ( Usage & D3DUSAGE_DEPTHSTENCIL )
	{
		key.m_texFlags |= CPs3gcmTextureLayout::kfTypeRenderable;
		key.m_texFlags |= CPs3gcmTextureLayout::kfTypeDepthStencil;
	}
	else if (Usage & D3DUSAGE_RENDERTARGET)
	{
		key.m_texFlags |= CPs3gcmTextureLayout::kfTypeRenderable;
		key.m_texFlags |= CPs3gcmTextureLayout::kfSrgbEnabled;			// this catches callers of CreateTexture who set the "renderable" option - they get an SRGB tex
	}

	if (Usage & D3DUSAGE_DYNAMIC)
	{
		key.m_texFlags |= CPs3gcmTextureLayout::kfDynamicNoSwizzle;
	}

	if (Usage & D3DUSAGE_TEXTURE_SRGB)
	{
		key.m_texFlags |= CPs3gcmTextureLayout::kfSrgbEnabled;
	}

	if (Usage & D3DUSAGE_TEXTURE_NOD3DMEMORY)
	{
		key.m_texFlags |= CPs3gcmTextureLayout::kfNoD3DMemory;
	}
}

HRESULT IDirect3DDevice9::CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture9** ppTexture,VD3DHANDLE* pSharedHandle)
{
	IDirect3DTexture9	*dxtex = new IDirect3DTexture9;
	dxtex->m_restype = D3DRTYPE_TEXTURE;

	dxtex->m_device		= this;

	dxtex->m_descZero.Format	= Format;
	dxtex->m_descZero.Type		= D3DRTYPE_TEXTURE;
	dxtex->m_descZero.Usage		= Usage;
	dxtex->m_descZero.Pool		= Pool;

	dxtex->m_descZero.MultiSampleType		= D3DMULTISAMPLE_NONE;
	dxtex->m_descZero.MultiSampleQuality	= 0;
	dxtex->m_descZero.Width		= Width;
	dxtex->m_descZero.Height	= Height;

	CPs3gcmTextureLayout::Key_t key;
	PreparePs3TextureKey( key, Format, Levels, Usage );
	key.m_size[0] = Width;
	key.m_size[1] = Height;
	key.m_size[2] = 1;
	dxtex->m_tex = CPs3gcmTexture::New( key );

	//
	// Create surface
	//
	dxtex->m_surfZero = new IDirect3DSurface9;

	dxtex->m_surfZero->m_desc	=	dxtex->m_descZero;
	dxtex->m_surfZero->m_tex	=	dxtex->m_tex;
	dxtex->m_surfZero->m_bOwnsTexture = false;
	dxtex->m_surfZero->m_face	=	0;
	dxtex->m_surfZero->m_mip	=	0;

	*ppTexture = dxtex;

	return S_OK;
}

HRESULT IDirect3DDevice9::CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture9** ppCubeTexture,VD3DHANDLE* pSharedHandle)
{

	IDirect3DCubeTexture9	*dxtex = new IDirect3DCubeTexture9;
	dxtex->m_restype = D3DRTYPE_CUBETEXTURE;

	dxtex->m_device			= this;

	dxtex->m_descZero.Format	= Format;
	dxtex->m_descZero.Type		= D3DRTYPE_CUBETEXTURE;
	dxtex->m_descZero.Usage		= Usage;
	dxtex->m_descZero.Pool		= Pool;

	dxtex->m_descZero.MultiSampleType		= D3DMULTISAMPLE_NONE;
	dxtex->m_descZero.MultiSampleQuality	= 0;
	dxtex->m_descZero.Width		= EdgeLength;
	dxtex->m_descZero.Height	= EdgeLength;

	CPs3gcmTextureLayout::Key_t key;
	PreparePs3TextureKey( key, Format, Levels, Usage );
	key.m_texFlags |= CPs3gcmTextureLayout::kfTypeCubeMap;
	key.m_size[0] = EdgeLength;
	key.m_size[1] = EdgeLength;
	key.m_size[2] = 1;
	dxtex->m_tex = CPs3gcmTexture::New( key );

	//
	// Create surfaces
	//
	for( int face = 0; face < 6; face ++)
	{
		dxtex->m_surfZero[face] = new IDirect3DSurface9;

		dxtex->m_surfZero[face]->m_desc	=	dxtex->m_descZero;
		dxtex->m_surfZero[face]->m_tex	=	dxtex->m_tex;
		dxtex->m_surfZero[face]->m_bOwnsTexture = false;
		dxtex->m_surfZero[face]->m_face	=	face;
		dxtex->m_surfZero[face]->m_mip	=	0;
	}

	*ppCubeTexture = dxtex;


	return S_OK;
}

HRESULT IDirect3DDevice9::CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture9** ppVolumeTexture,VD3DHANDLE* pSharedHandle)
{
	// set dxtex->m_restype to D3DRTYPE_VOLUMETEXTURE...

	IDirect3DVolumeTexture9	*dxtex = new IDirect3DVolumeTexture9;
	dxtex->m_restype = D3DRTYPE_VOLUMETEXTURE;

	dxtex->m_device			= this;

	dxtex->m_descZero.Format	= Format;
	dxtex->m_descZero.Type		= D3DRTYPE_VOLUMETEXTURE;
	dxtex->m_descZero.Usage		= Usage;
	dxtex->m_descZero.Pool		= Pool;

	dxtex->m_descZero.MultiSampleType		= D3DMULTISAMPLE_NONE;
	dxtex->m_descZero.MultiSampleQuality	= 0;
	dxtex->m_descZero.Width		= Width;
	dxtex->m_descZero.Height	= Height;

	// also a volume specific desc
	dxtex->m_volDescZero.Format		= Format;
	dxtex->m_volDescZero.Type		= D3DRTYPE_VOLUMETEXTURE;
	dxtex->m_volDescZero.Usage		= Usage;
	dxtex->m_volDescZero.Pool		= Pool;

	dxtex->m_volDescZero.Width		= Width;
	dxtex->m_volDescZero.Height		= Height;
	dxtex->m_volDescZero.Depth		= Depth;

	CPs3gcmTextureLayout::Key_t key;
	PreparePs3TextureKey( key, Format, Levels, Usage );
	key.m_size[0] = Width;
	key.m_size[1] = Height;
	key.m_size[2] = Depth;
	dxtex->m_tex = CPs3gcmTexture::New( key );

	//
	// Create surface
	//
	dxtex->m_surfZero = new IDirect3DSurface9;

	dxtex->m_surfZero->m_desc	=	dxtex->m_descZero;
	dxtex->m_surfZero->m_tex	=	dxtex->m_tex;
	dxtex->m_surfZero->m_bOwnsTexture = false;
	dxtex->m_surfZero->m_face	=	0;
	dxtex->m_surfZero->m_mip	=	0;

	*ppVolumeTexture = dxtex;

	return S_OK;
}

bool IDirect3DDevice9::AllocateTextureStorage( IDirect3DBaseTexture9 *pTexture )
{
	// Allocate storage for a texture's bits (if D3DUSAGE_TEXTURE_NOD3DMEMORY was used to defer allocation on creation)
	return pTexture->m_tex->Allocate();
}

HRESULT IDirect3DDevice9::SetTexture( DWORD Stage, IDirect3DBaseTexture9* pTexture )
{
	if( pTexture /*&& pTexture->m_tex->m_lmBlock.Size()*/ )
	{
		gpGcmDrawState->SetTexture(Stage, pTexture->m_tex);

	}
	else
	{
		gpGcmDrawState->ResetTexture(Stage);
	}	
	return S_OK;
}

HRESULT IDirect3DDevice9::GetTexture(DWORD Stage,IDirect3DBaseTexture9** ppTexture)
{
	// if implemented, should it increase the ref count ??
	Debugger();
	return S_OK;
}

void IDirect3DDevice9::FlushTextureCache()
{
	gpGcmDrawState->SetInvalidateTextureCache();
}


//--------------------------------------------------------------------------------------------------
// IDirect3DBaseTexture9::	GetType
// 							GetLevelCount
// 							GetLevelDesc
// 							LockRect
// 							UnlockRect
// IDirect3DCubeTexture9::	GetCubeMapSurface
// 							GetLevelDesc
// IDirect3DVolumeTexture9::LockBox
// 							UnlockBox
// 							GetLevelDesc
//--------------------------------------------------------------------------------------------------

D3DRESOURCETYPE IDirect3DBaseTexture9::GetType()
{
	return m_restype;	//D3DRTYPE_TEXTURE;
}

DWORD IDirect3DBaseTexture9::GetLevelCount()
{
	return m_tex->m_layout->m_mipCount;
}

HRESULT IDirect3DBaseTexture9::GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc)
{
	Assert (Level < m_tex->m_layout->m_mipCount);

	D3DSURFACE_DESC result = m_descZero;
	// then mutate it for the level of interest
	
	CPs3gcmTextureLayout const &layout = *m_tex->m_layout;
	int iSlice = layout.SliceIndex( 0, Level );
	CPs3gcmTextureLayout::Slice_t const &slice = layout.m_slices[iSlice];

	result.Width = slice.m_size[0];
	result.Height = slice.m_size[1];
	
	*pDesc = result;

	return S_OK;
}

HRESULT IDirect3DTexture9::LockRect(UINT Level,D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
{
	Debugger();
	return S_OK;
}

HRESULT IDirect3DTexture9::UnlockRect(UINT Level)
{
	Debugger();
	return S_OK;
}

HRESULT IDirect3DTexture9::GetSurfaceLevel(UINT Level,IDirect3DSurface9** ppSurfaceLevel)
{
	// we create and pass back a surface, and the client is on the hook to release it.  tidy.

	IDirect3DSurface9 *surf = new IDirect3DSurface9;

	CPs3gcmTextureLayout const &layout = *m_tex->m_layout;
	int iSlice = layout.SliceIndex( 0, Level );
	CPs3gcmTextureLayout::Slice_t const &slice = layout.m_slices[iSlice];

	surf->m_desc = m_descZero;
	surf->m_desc.Width = slice.m_size[0];
	surf->m_desc.Height = slice.m_size[1];

	surf->m_tex	= m_tex;
	surf->m_bOwnsTexture = false;
	surf->m_face = 0;
	surf->m_mip = Level;

	*ppSurfaceLevel = surf;

	return S_OK;
}


HRESULT IDirect3DCubeTexture9::GetCubeMapSurface(D3DCUBEMAP_FACES FaceType,UINT Level,IDirect3DSurface9** ppCubeMapSurface)
{
	// we create and pass back a surface, and the client is on the hook to release it...

	IDirect3DSurface9 *surf = new IDirect3DSurface9;

	CPs3gcmTextureLayout const &layout = *m_tex->m_layout;
	int iSlice = layout.SliceIndex( FaceType, Level );
	CPs3gcmTextureLayout::Slice_t const &slice = layout.m_slices[iSlice];

	surf->m_desc = m_descZero;
		surf->m_desc.Width = slice.m_size[0];
		surf->m_desc.Height = slice.m_size[1];
		
	surf->m_tex	= m_tex;
	surf->m_bOwnsTexture = false;
	surf->m_face = FaceType;
	surf->m_mip = Level;

	*ppCubeMapSurface = surf;

	return S_OK;
}

HRESULT IDirect3DCubeTexture9::GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc)
{
	Assert (Level < m_tex->m_layout->m_mipCount);

	D3DSURFACE_DESC result = m_descZero;
	// then mutate it for the level of interest
	
	CPs3gcmTextureLayout const &layout = *m_tex->m_layout;
	int iSlice = layout.SliceIndex( 0, Level );
	CPs3gcmTextureLayout::Slice_t const &slice = layout.m_slices[iSlice];

	result.Width = slice.m_size[0];
	result.Height = slice.m_size[1];

	*pDesc = result;

	return S_OK;
}

HRESULT IDirect3DVolumeTexture9::LockBox(UINT Level,D3DLOCKED_BOX* pLockedVolume,CONST D3DBOX* pBox,DWORD Flags)
{
	if ( !m_tex->m_lmBlock.Size() )
	{
		Assert( 0 );
		Warning( "\n\nERROR: (IDirect3DSurface9::LockBox) cannot lock this texture until AllocateTextureStorage is called!!\n\n\n" );
		return S_FALSE;
	}

	CPs3gcmTextureLayout const &layout = *m_tex->m_layout;
	int iSlice = layout.SliceIndex( 0, Level );
	CPs3gcmTextureLayout::Slice_t const &slice = layout.m_slices[iSlice];

	int iOffsetInDataSlab = slice.m_storageOffset;
	int iPitch = layout.SlicePitch( iSlice );
	int iDepthPitch = iPitch * slice.m_size[1];
	Assert( !layout.IsTiledMemory() );

	// Account for locking request on a subrect:
	if ( pBox )
	{
		// Assert that locking the box can yield a pointer to a legitimate byte:
		Assert( !pBox->Left || !layout.IsTiledMemory() );
		Assert( 0 == ( ( pBox->Left * layout.GetFormatPtr()->m_gcmPitchPer4X ) % 4 ) );
		iOffsetInDataSlab += pBox->Left * layout.GetFormatPtr()->m_gcmPitchPer4X / 4;
		iOffsetInDataSlab += pBox->Top * iPitch;
		iOffsetInDataSlab += pBox->Front * iDepthPitch;
	}

	// Set the locked rect data:
	pLockedVolume->pBits = m_tex->Data() + iOffsetInDataSlab;
	pLockedVolume->RowPitch = iPitch;
	pLockedVolume->SlicePitch = iDepthPitch;

	return S_OK;
}

HRESULT IDirect3DVolumeTexture9::UnlockBox(UINT Level)
{
	// Since the texture has just been modified, and this same texture bits may have been used in one of the previous draw calls
	// and may still linger in the texture cache (even if this is a new texture, it may theoretically share bits with some old texture,
	// which was just destroyed, and if we didn't have a lot of texture traffic in the last frame, those bits in texture cache may conceivably 
	// survive until the next draw call)
	gpGcmDrawState->m_dirtyCachesMask |= CGcmDrawState::kDirtyTxCache;
	return S_OK;
}

HRESULT IDirect3DVolumeTexture9::GetLevelDesc( UINT Level, D3DVOLUME_DESC *pDesc )
{
	if (Level > m_tex->m_layout->m_mipCount)
	{
		Debugger();
	}

	D3DVOLUME_DESC result = m_volDescZero;
	// then mutate it for the level of interest

	CPs3gcmTextureLayout const &layout = *m_tex->m_layout;
	int iSlice = layout.SliceIndex( 0, Level );
	CPs3gcmTextureLayout::Slice_t const &slice = layout.m_slices[iSlice];

	result.Width = slice.m_size[0];
	result.Height = slice.m_size[1];
	result.Depth = slice.m_size[2];

	*pDesc = result;

	return S_OK;
}

//--------------------------------------------------------------------------------------------------
// RENDER TARGETS and SURFACES
// IDirect3DDevice9::	CreateRenderTarget
// 						SetRenderTarget
// 						GetRenderTarget
// 						CreateOffscreenPlainSurface
// 						CreateDepthStencilSurface
// 						Ps3Helper_UpdateSurface
// 						DxDeviceForceUpdateRenderTarget
// 						SetDepthStencilSurface
// 						GetDepthStencilSurface
// 						GetRenderTargetData
// 						GetFrontBufferData
// 						StretchRect
//						SetScissorRect
//						SetClipPlane
//--------------------------------------------------------------------------------------------------

HRESULT IDirect3DDevice9::CreateRenderTarget(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable,IDirect3DSurface9** ppSurface,VD3DHANDLE* pSharedHandle)
{
	HRESULT result = S_OK;

	IDirect3DSurface9 *surf = new IDirect3DSurface9;
	surf->m_restype = D3DRTYPE_SURFACE;

	surf->m_device		= this;				// always set device on creations!

	CPs3gcmTextureLayout::Key_t key;
	PreparePs3TextureKey( key, Format, 0, 0 );
	key.m_size[0] = Width;
	key.m_size[1] = Height;
	key.m_size[2] = 1;

	key.m_texFlags = CPs3gcmTextureLayout::kfTypeRenderable;
	key.m_texFlags |= CPs3gcmTextureLayout::kfSrgbEnabled;	// all render target tex are SRGB mode

	if ( pSharedHandle == ( VD3DHANDLE* ) this )
	{
		// internal RT, reuse the color buffer
		surf->m_tex = new CPs3gcmTexture;
		surf->m_tex->m_layout = CPs3gcmTextureLayout::New( key );
		surf->m_tex->m_lmBlock.Assign( g_ps3gcmGlobalState.m_display.surfaceColor[ g_ps3gcmGlobalState.m_display.surfaceFlipIdx ] );
	}
	else
	{
		surf->m_tex			= CPs3gcmTexture::New( key );
	}
	surf->m_bOwnsTexture = true;
	surf->m_face		= 0;
	surf->m_mip			= 0;

	//desc
	surf->m_desc.Format				=	Format;
	surf->m_desc.Type				=	D3DRTYPE_SURFACE;
	surf->m_desc.Usage				=	0;					//FIXME ???????????
	surf->m_desc.Pool				=	D3DPOOL_DEFAULT;	//FIXME ???????????
	surf->m_desc.MultiSampleType	=	MultiSample;
	surf->m_desc.MultiSampleQuality	=	MultisampleQuality;
	surf->m_desc.Width				=	Width;
	surf->m_desc.Height				=	Height;

	*ppSurface = (result==S_OK) ? surf : NULL;

	return result;
}

HRESULT IDirect3DDevice9::SetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9* pRenderTarget)
{
	if ( pRenderTarget != m_rtSurfaces[RenderTargetIndex] )
	{
		if (pRenderTarget)
			pRenderTarget->AddRef();

		if (m_rtSurfaces[RenderTargetIndex])
			m_rtSurfaces[RenderTargetIndex]->Release();

		m_rtSurfaces[RenderTargetIndex] = pRenderTarget;

		Ps3Helper_UpdateSurface( RenderTargetIndex );
	}

	return S_OK;
}

HRESULT IDirect3DDevice9::GetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9** ppRenderTarget)
{
	m_rtSurfaces[ RenderTargetIndex ]->AddRef();	// per http://msdn.microsoft.com/en-us/library/bb174404(VS.85).aspx

	*ppRenderTarget = m_rtSurfaces[ RenderTargetIndex ];

	return S_OK;
}

HRESULT IDirect3DDevice9::CreateOffscreenPlainSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DPOOL Pool,IDirect3DSurface9** ppSurface,VD3DHANDLE* pSharedHandle)
{
	// set surf->m_restype to D3DRTYPE_SURFACE...

	// this is almost identical to CreateRenderTarget..

	HRESULT result = S_OK;

	IDirect3DSurface9 *surf = new IDirect3DSurface9;
	surf->m_restype = D3DRTYPE_SURFACE;

	surf->m_device		= this;				// always set device on creations!

	CPs3gcmTextureLayout::Key_t key;
	PreparePs3TextureKey( key, Format, 0, 0 );
	key.m_size[0] = Width;
	key.m_size[1] = Height;
	key.m_size[2] = 1;

	key.m_texFlags = CPs3gcmTextureLayout::kfTypeRenderable;

	surf->m_tex			= CPs3gcmTexture::New( key );
	surf->m_bOwnsTexture = true;

	surf->m_face		=	0;
	surf->m_mip			=	0;

	//desc
	surf->m_desc.Format				=	Format;
	surf->m_desc.Type				=	D3DRTYPE_SURFACE;
	surf->m_desc.Usage				=	0;
	surf->m_desc.Pool				=	D3DPOOL_DEFAULT;
	surf->m_desc.MultiSampleType	=	D3DMULTISAMPLE_NONE;
	surf->m_desc.MultiSampleQuality	=	0;
	surf->m_desc.Width				=	Width;
	surf->m_desc.Height				=	Height;

	*ppSurface = (result==S_OK) ? surf : NULL;

	return result;
}

HRESULT IDirect3DDevice9::CreateDepthStencilSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,
													BOOL Discard,IDirect3DSurface9** ppSurface,VD3DHANDLE* pSharedHandle)
{
	HRESULT result = S_OK;

	IDirect3DSurface9 *surf = new IDirect3DSurface9;
	surf->m_restype = D3DRTYPE_SURFACE;

	surf->m_device		= this;				// always set device on creations!

	CPs3gcmTextureLayout::Key_t key;
	PreparePs3TextureKey( key, Format, 0, 0 );
	key.m_size[0] = Width;
	key.m_size[1] = Height;
	key.m_size[2] = 1;

	key.m_texFlags = CPs3gcmTextureLayout::kfTypeRenderable | CPs3gcmTextureLayout::kfTypeDepthStencil;

	if ( pSharedHandle == ( VD3DHANDLE* ) this )
	{
		// internal RT, reuse the depth buffer
		surf->m_tex = new CPs3gcmTexture;
		surf->m_tex->m_layout = CPs3gcmTextureLayout::New( key );
		surf->m_tex->m_lmBlock.Assign( g_ps3gcmGlobalState.m_display.surfaceDepth );
	}
	else
	{
		surf->m_tex				= CPs3gcmTexture::New( key );
	}
	surf->m_bOwnsTexture = true;
	surf->m_face			= 0;
	surf->m_mip				= 0;

	//desc

	surf->m_desc.Format				=	Format;
	surf->m_desc.Type				=	D3DRTYPE_SURFACE;
	surf->m_desc.Usage				=	0;					//FIXME ???????????
	surf->m_desc.Pool				=	D3DPOOL_DEFAULT;	//FIXME ???????????
	surf->m_desc.MultiSampleType	=	MultiSample;
	surf->m_desc.MultiSampleQuality	=	MultisampleQuality;
	surf->m_desc.Width				=	Width;
	surf->m_desc.Height				=	Height;

	*ppSurface = (result==S_OK) ? surf : NULL;

	return result;
}


uint s_nUpdateSurfaceCounter = 0, s_nUpdateSurfaceDebug = -1;

void IDirect3DDevice9::Ps3Helper_UpdateSurface( int idx )
{
	CPs3gcmTexture *texC = m_rtSurfaces[idx] ? m_rtSurfaces[idx]->m_tex : NULL;

	// If color buffer is 8x8 or less, we assume this is a dummy color buffer, and Z only the surface

	if (texC)
	{
		if(texC->m_layout->m_key.m_size[0] < 9)
		{
			texC = NULL;
		}
	}

	CPs3gcmTexture *texZ = ( m_dsSurface && !idx ) ? m_dsSurface->m_tex : NULL;
	CPs3gcmTexture *texCZ = texC ? texC : texZ;

	UpdateSurface_t surface, *pSurfaceUpdate=&surface;

	pSurfaceUpdate->m_texC.Assign( texC );
	pSurfaceUpdate->m_texZ.Assign( texZ );
	gpGcmDrawState->Ps3Helper_UpdateSurface(pSurfaceUpdate);
}

void DxDeviceForceUpdateRenderTarget( )
{
	extern IDirect3DDevice9 *m_pD3DDevice;
	m_pD3DDevice->Ps3Helper_UpdateSurface( 0 );
}

HRESULT IDirect3DDevice9::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil)
{
	if ( pNewZStencil != m_dsSurface )
	{
		if (pNewZStencil)
			pNewZStencil->AddRef();

		if (m_dsSurface)
			m_dsSurface->Release();

		m_dsSurface = pNewZStencil;
		Ps3Helper_UpdateSurface( 0 );
	}

	return S_OK;
}

HRESULT IDirect3DDevice9::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface)
{
	m_dsSurface->AddRef();			// per http://msdn.microsoft.com/en-us/library/bb174384(VS.85).aspx

	*ppZStencilSurface = m_dsSurface;

	return S_OK;
}

HRESULT IDirect3DDevice9::GetRenderTargetData(IDirect3DSurface9* pRenderTarget,IDirect3DSurface9* pDestSurface)
{
	// is it just a blit ?

	this->StretchRect( pRenderTarget, NULL, pDestSurface, NULL, D3DTEXF_NONE ); // is this good enough ???

	return S_OK;
}

HRESULT IDirect3DDevice9::GetFrontBufferData(UINT iSwapChain,IDirect3DSurface9* pDestSurface)
{
	Debugger();
	return S_OK;
}

HRESULT IDirect3DDevice9::StretchRect(IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestSurface,CONST RECT* pDestRect,D3DTEXTUREFILTERTYPE Filter)
{
	// find relevant slices in GLM tex

	int nRenderTargetIndex = ( int ) pSourceSurface;
	Assert( nRenderTargetIndex >= -1 && nRenderTargetIndex < 4 );

	//
	// Source texture
	//
	CPs3gcmTexture	*srcTex = NULL;
	if ( nRenderTargetIndex == -1 )
		srcTex = m_dsSurface ? m_dsSurface->m_tex : NULL;
	else
		srcTex = m_rtSurfaces[nRenderTargetIndex] ? m_rtSurfaces[nRenderTargetIndex]->m_tex : NULL;
	if ( !srcTex )
		return S_FALSE;

	CPs3gcmTextureLayout::Slice_t const srcSlice = srcTex->m_layout->m_slices[ 0 ];

	//
	// Destination texture
	//
	CPs3gcmTexture	*dstTex = pDestSurface->m_tex;
	// we're assuming that we always transfer into the main mip, because it was the case so far. not gonna change it now.
	int dstSliceIndex = 0;//dstTex->m_layout->SliceIndex( pDestSurface->m_face, pDestSurface->m_mip );
	CPs3gcmTextureLayout::Slice_t const dstSlice = dstTex->m_layout->m_slices[ dstSliceIndex ];

	//
	// Perform RSX data transfer
	//

	RECT srcSizeRect = {0,0,srcSlice.m_size[0],srcSlice.m_size[1]};
	RECT dstSizeRect = {0,0,dstSlice.m_size[0],dstSlice.m_size[1]};

	if( !pDestRect )
		pDestRect = &dstSizeRect;
	if( !pSourceRect )
		pSourceRect = &srcSizeRect;

	// explicit signed int, so that width/height intermediate values may be negative
	signed int nWidth = pSourceRect->right - pSourceRect->left, nHeight = pSourceRect->bottom-pSourceRect->top;
	if( pDestRect->left + nWidth > dstSizeRect.right )
	{
		nWidth = dstSizeRect.right - pDestRect->left;
	}

	if( pDestRect->top + nHeight > dstSizeRect.bottom )
	{
		nHeight = dstSizeRect.bottom - pDestRect->top;
	}

	if( pSourceRect->left + nWidth > srcSizeRect.right )
	{
		nWidth = srcSizeRect.right - pSourceRect->left;
	}

	if( pSourceRect->top + nHeight > srcSizeRect.bottom )
	{
		nHeight = srcSizeRect.bottom - pSourceRect->top;
	}

	if( nWidth > 0 && nHeight > 0 )
	{
		gpGcmDrawState->SetTransferImage(
			CELL_GCM_TRANSFER_LOCAL_TO_LOCAL,

			dstTex->Offset(),
			dstTex->m_layout->DefaultPitch(),
			pDestRect->left, pDestRect->top,

			srcTex->Offset(),
			srcTex->m_layout->DefaultPitch(),
			pSourceRect->left, pSourceRect->top,
			nWidth, nHeight,

			// The only supported formats are R5G6B5 for a 2-byte transfer and A8R8G8B8 for a 4-byte transfer.
			!srcTex->m_layout->IsTiledMemory() ? srcTex->m_layout->GetFormatPtr()->m_gcmPitchPer4X/4 : 4
			);
	}

	return S_OK;
}

HRESULT IDirect3DDevice9::SetScissorRect(CONST RECT* pRect)
{
// 	SpuDrawScissor_t * pScissor = g_spuGcm.GetDrawQueue()->AllocWithHeader<SpuDrawScissor_t>( SPUDRAWQUEUE_SETSCISSORRECT_METHOD );
// 	V_memcpy( &m_gcmStatePpu.m_scissor[0], &pScissor->x, 4 * sizeof( uint16 ) );

	DrawScissor_t scissor, *pScissor = &scissor;
	// clamping the values to the allowed by RSX. Values outside of this range (e.g. negative width/height) will crash RSX
	// this came up w.r.t. scissor stack optimization, when used with r_portalscissor 1
	// it would be better to do this on SPU, but it would make scissor state on PPU different from SPU
	// we could "& 4095", but it's late in the project, so it seems safer to logically clamp the values
	pScissor->x = clamp( pRect->left, 0, 4095 );
	pScissor->y = clamp( pRect->top, 0, 4095 );
	pScissor->w = clamp( pRect->right  - pRect->left, 0, 4096 );
	pScissor->h = clamp( pRect->bottom - pRect->top, 0, 4096 );

	gpGcmDrawState->SetScissorRect(pScissor);

	return S_OK;
}

HRESULT IDirect3DDevice9::SetClipPlane(DWORD Index,CONST float* pPlane)
{
	Assert(Index<2);
	this->SetVertexShaderConstantF( 253 + Index, pPlane, 1 );	// stash the clip plane values into shader param - translator knows where to look
	return S_OK;
}

//--------------------------------------------------------------------------------------------------
// IDirect3DSurface9::LockRect
//				      UnlockRect
//					  GetDesc
//--------------------------------------------------------------------------------------------------

HRESULT IDirect3DSurface9::LockRect(D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
{
	if ( !m_tex->m_lmBlock.Size() )
	{
		Assert( 0 );
		Warning( "\n\nERROR: (IDirect3DSurface9::LockRect) cannot lock this texture until AllocateTextureStorage is called!!\n\n\n" );
		return S_FALSE;
	}

	CPs3gcmTextureLayout const &layout = *m_tex->m_layout;
	int iSlice = layout.SliceIndex( this->m_face, this->m_mip );
	CPs3gcmTextureLayout::Slice_t const &slice = layout.m_slices[iSlice];

	int iOffsetInDataSlab = slice.m_storageOffset;
	int iPitch = layout.SlicePitch( iSlice );

	// Account for locking request on a subrect:
	if ( pRect )
	{
		// Assert that locking the rect can yield a pointer to a legitimate byte:
		Assert( !pRect->left || !layout.IsTiledMemory() );
		Assert( 0 == ( ( pRect->left * layout.GetFormatPtr()->m_gcmPitchPer4X ) % 4 ) );
		iOffsetInDataSlab += pRect->left * layout.GetFormatPtr()->m_gcmPitchPer4X / 4;
		iOffsetInDataSlab += pRect->top * iPitch;
	}

	// Set the locked rect data:
	pLockedRect->pBits = m_tex->Data() + iOffsetInDataSlab;
	pLockedRect->Pitch = iPitch;
	
	return S_OK;
}

HRESULT IDirect3DSurface9::UnlockRect()
{
	// Since the texture has just been modified, and this same texture bits may have been used in one of the previous draw calls
	// and may still linger in the texture cache (even if this is a new texture, it may theoretically share bits with some old texture,
	// which was just destroyed, and if we didn't have a lot of texture traffic in the last frame, those bits in texture cache may conceivably 
	// survive until the next draw call)
	gpGcmDrawState->m_dirtyCachesMask |= CGcmDrawState::kDirtyTxCache;
	return S_OK;
}

HRESULT IDirect3DSurface9::GetDesc(D3DSURFACE_DESC *pDesc)
{
	*pDesc = m_desc;
	return S_OK;
}


//--------------------------------------------------------------------------------------------------
// PIXEL SHADERS
//
// IDirect3DDevice9::CreatePixelShader
// IDirect3DPixelShader9::IDirect3DPixelShader9
//						  ~IDirect3DPixelShader9
// IDirect3DDevice9::SetPixelShader
//					 SetPixelShaderConstantF
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Util funcs for Cg etc..
//--------------------------------------------------------------------------------------------------

struct DatatypeRec_t
{
	CGtype type;
	CGparameterclass parameterClass;
};


static DatatypeRec_t s_datatypeClassname[] = {
#define CG_DATATYPE_MACRO(name, compiler_name, enum_name, base_enum, nrows, ncols,classname) \
	{ enum_name, classname },
#include <Cg/cg_datatypes.h>
#undef CG_DATATYPE_MACRO
};


CGparameterclass vcgGetTypeClass( CGtype type )
{
	if( type <= CG_TYPE_START_ENUM || type > CG_TYPE_START_ENUM + sizeof( s_datatypeClassname ) / sizeof( s_datatypeClassname[0] ) )
	{	
		return CG_PARAMETERCLASS_UNKNOWN;
	}
	else
	{
		DatatypeRec_t & rec = s_datatypeClassname[type - CG_TYPE_START_ENUM - 1];
		Assert( rec.type == type );
		return rec.parameterClass;
	}
}


static uint fspatchGetLength( CGtype nType )
{
	uint32_t length = 0;
	switch ( nType )
	{
	case CG_FLOAT:
	case CG_BOOL:
	case CG_FLOAT1:
	case CG_BOOL1:
		length = 1;
		break;
	case CG_FLOAT2:
	case CG_BOOL2:
		length = 2;
		break;
	case CG_FLOAT3:
	case CG_BOOL3:
		length = 3;
		break;
	case CG_FLOAT4:
	case CG_BOOL4:
		length = 4;
		break;
	case CG_FLOAT3x3:
	case CG_BOOL3x3:
	case CG_FLOAT3x4:
	case CG_BOOL3x4:
		length = 3;
		break;
	case CG_FLOAT4x4:
	case CG_BOOL4x4:
	case CG_FLOAT4x3:
	case CG_BOOL4x3:
		length = 4;
		break;
	default:
		//DebuggerBreak();
		length = 0;
	}
	return length;
}

// recursive set bit count
uint CountBits32( uint32 a )
{
	uint a1 = ( a & 0x55555555 ) + ( ( a >> 1 ) & 0x55555555 );
	uint a2 = ( a1 & 0x33333333 ) + ( ( a1 >> 2 ) & 0x33333333 );
	uint a3 = ( a2 & 0x0F0F0F0F ) + ( ( a2 >> 4 ) & 0x0F0F0F0F );
	uint a4 = ( a3 & 0x00FF00FF ) + ( ( a3 >> 8 ) & 0x00FF00FF );
	uint a5 =  ( a4 & 0xFFFF ) + ( a4 >> 16 );
	
	#ifdef DBGFLAG_ASSERT
	uint nCheckCount = 0;
	for( uint i  = 0; i < 32; ++i )
		nCheckCount += ( a >> i ) & 1;
	Assert( nCheckCount == a5 );
	#endif
	return a5;
}


//--------------------------------------------------------------------------------------------------
// IDirect3D pixel shader code
//--------------------------------------------------------------------------------------------------


HRESULT IDirect3DDevice9::CreatePixelShader(CONST DWORD* pFunction,IDirect3DPixelShader9** ppShader, const char *pShaderName, char *debugLabel)
{
	CgBinaryProgram *pProg = (CgBinaryProgram *)pFunction;

//	Msg(">>>Pixel Shader : %s at 0x%08x sz 0x%04d no.param 0x%04d \n", pShaderName, pFunction, pProg->ucodeSize, pProg->parameterCount);

	IDirect3DPixelShader9 *newprog = new IDirect3DPixelShader9( ( CgBinaryProgram * ) ( char* ) pFunction );

	Assert( !( 0xF & uint( &newprog->m_data ) ) );

	*ppShader = newprog;

	return S_OK;
}

uint32 g_nPixelShaderTotalSize = 0;
uint32 g_nPixelShaderTotalUcode = 0;

IDirect3DPixelShader9::IDirect3DPixelShader9( CgBinaryProgram* prog )
{
	g_nPixelShaderTotalSize += prog->totalSize;
	g_nPixelShaderTotalUcode += prog->ucodeSize;
	uint nPatchCount = 0;
 	
 	//--------------------------------------------------------------------------------------------------
	// Get Attribute input mask, register count and check revision
	//--------------------------------------------------------------------------------------------------
	
 	CgBinaryFragmentProgram *pCgFragmentProgram = (  CgBinaryFragmentProgram * )( uintp( prog ) + prog->program );
 	m_data.m_attributeInputMask = pCgFragmentProgram->attributeInputMask;

 	// check binary format revision -- offline recompile necessary
 	// -- enforce the correct ucode for nv40/nv47/rsx
 	Assert( prog->binaryFormatRevision == CG_BINARY_FORMAT_REVISION );

	uint registerCount = pCgFragmentProgram->registerCount;
	// NOTE: actual register count can be modified by specifying an artificial e.g. PS3REGCOUNT48 static combo to force it to 48
	Assert( registerCount <= 48 );
	if (registerCount < 2)
	{
		// register count must be [2, 48]
		registerCount = 2;
	}

	//--------------------------------------------------------------------------------------------------
	// Build shader control0 and get tex control info, including number of tex controls
	//--------------------------------------------------------------------------------------------------


	uint8_t controlTxp = CELL_GCM_FALSE;
	uint32 shCtrl0 = ( CELL_GCM_COMMAND_CAST( controlTxp ) << CELL_GCM_SHIFT_SET_SHADER_CONTROL_CONTROL_TXP ) 
		& CELL_GCM_MASK_SET_SHADER_CONTROL_CONTROL_TXP;
	shCtrl0 |= ( 1<<10 ) | ( registerCount << 24 );
	shCtrl0 |= pCgFragmentProgram->depthReplace ? 0xE : 0x0;
	shCtrl0 |= pCgFragmentProgram->outputFromH0 ? 0x00 : 0x40;
	shCtrl0 |= pCgFragmentProgram->pixelKill ? 0x80 : 0x00;

	uint texMask = pCgFragmentProgram->texCoordsInputMask;
	uint texMask2D = pCgFragmentProgram->texCoords2D;
	uint texMaskCentroid = pCgFragmentProgram->texCoordsCentroid;
	uint nTexControls = CountBits32( texMask );
	if( !IsCert() && nTexControls > 16 )
		Error( "Corrupted pixel shader with %d tex controls is requested.\n", nTexControls );

	//--------------------------------------------------------------------------------------------------
	// Walk params, count number of embedded constant patches and build sampler input mask 
	//--------------------------------------------------------------------------------------------------


	m_data.m_samplerInputMask = 0;
	CgBinaryParameter * pParameters = ( CgBinaryParameter * )( uintp( prog ) + prog->parameterArray ) ;
	
	for ( uint nPar = 0; nPar < prog->parameterCount; ++nPar )
	{
		CgBinaryParameter * pPar = pParameters + nPar;
		if( pPar->isReferenced )
		{
			if( vcgGetTypeClass( pPar->type ) == CG_PARAMETERCLASS_SAMPLER )
			{
				Assert( pPar->var == CG_UNIFORM ); // if there are varying textures, I'm not sure what they mean, exactly
				Assert( pPar->direction == CG_IN ); // fragment shaders don't generally output samplers. They take them as parameters.
				Assert( pPar->res >= CG_TEXUNIT0 && pPar->res <= CG_TEXUNIT15 );
				m_data.m_samplerInputMask |= 1 << ( pPar->res - CG_TEXUNIT0 );
			}
			else if ( pPar->embeddedConst )
			{
				const CgBinaryEmbeddedConstant * pEmbedded = ( const CgBinaryEmbeddedConstant* )( uintp( prog ) + pPar->embeddedConst );
				nPatchCount += pEmbedded->ucodeCount;
			}
		}
		else
		{
			Assert( !pPar->embeddedConst );
		}
	}

	//--------------------------------------------------------------------------------------------------
	// Allocate memory layout as : 
	// FpHeader_t
	// uCode
	// Patches
	// Texcontrols
	//--------------------------------------------------------------------------------------------------

	uint nUcodeSize = AlignValue( prog->ucodeSize, 16 );
	uint nTotalSize = AlignValue( sizeof( FpHeader_t ) + nUcodeSize + (sizeof( uint32 ) * nPatchCount) 
									+ (2 * sizeof( uint32 ) * nTexControls) , 16);
	m_data.m_nTotalSize = nTotalSize;
	m_data.m_eaFp = ( FpHeader_t* )MemAlloc_AllocAligned( nTotalSize, 16 );

	//--------------------------------------------------------------------------------------------------
	// header and mictocode
	//--------------------------------------------------------------------------------------------------

	m_data.m_eaFp->m_nUcodeSize = nUcodeSize;
	m_data.m_eaFp->m_nPatchCount = nPatchCount;
	m_data.m_eaFp->m_nShaderControl0 = shCtrl0;
	m_data.m_eaFp->m_nTexControls = nTexControls;
	V_memcpy( m_data.m_eaFp + 1, (void*)( uintp( prog ) + prog->ucode) , prog->ucodeSize );


	//--------------------------------------------------------------------------------------------------
	// Patches : Each patch is : Bits 31&30 hold the patch len - 1. Bits 16-24 constant number , bits 0-16 qw index to patch
	//--------------------------------------------------------------------------------------------------


	uint32 *pPatches = ( uint32* ) ( uintp( m_data.m_eaFp + 1 ) + nUcodeSize ), *pPatchesEnd = pPatches + nPatchCount;
	for ( uint nPar = 0; nPar < prog->parameterCount; ++nPar )
	{
		CgBinaryParameter * pPar = pParameters + nPar;

		if ( pPar->embeddedConst )
		{
			uint nLength = fspatchGetLength( pPar->type );
			uint32 nPatch = ( ( pPar->resIndex ) << 16 ) | ( ( nLength - 1 ) << 30 );
			if( pPar->resIndex > 0xFF )
			{
				Error( "Fragment Program Patch table refers to non-existing virtual register %d\n", pPar->resIndex );
			}

			if( nLength == 0 )
				Error(" Unsupported fragment program parameter type %d\n", pPar->type ); // only 4-element types are supported by the patcher so far
			const CgBinaryEmbeddedConstant * pEmbedded = ( const CgBinaryEmbeddedConstant* )( uintp( prog ) + pPar->embeddedConst );
			for ( uint nEm = 0; nEm < pEmbedded->ucodeCount; ++ nEm )
			{
				uint ucodeOffset = pEmbedded->ucodeOffset[nEm]; // is this the offset from prog structure start?
				if( !IsCert() && ( ucodeOffset & 0xF ) )
				{
					const char * pParname = (const char * )( uintp( prog ) + pPar->name );
					Error( "Patch table too big: offset 0x%X, resIndex %d, parameter %d '%s'\n", ucodeOffset, pPar->resIndex, nPar, pParname );
				}

				Assert( pPatches < pPatchesEnd );
				*( pPatches ++ ) = nPatch | ( ucodeOffset >> 4 );
			}
		}
	}
	Assert( pPatches == pPatchesEnd );

	//--------------------------------------------------------------------------------------------------
	// Tex Controls
	//--------------------------------------------------------------------------------------------------
	
	uint32 * pTexControls = (uint32*)( uintp( m_data.m_eaFp ) + sizeof( FpHeader_t ) + nUcodeSize + (sizeof( uint32 ) * nPatchCount) );
	uint32 * pTexControlsEnd = pTexControls + nTexControls * 2;
	for( uint i = 0; texMask; i++)
	{
		// keep the cached variable in sync
		if (texMask&1) {
			uint32_t hwTexCtrl = ( texMask2D & 1) | ( ( texMaskCentroid & 1 ) << 4 );
			CELL_GCM_METHOD_SET_TEX_COORD_CONTROL( pTexControls, i, hwTexCtrl );
		}

		texMask >>= 1;
		texMask2D >>= 1;
		texMaskCentroid >>= 1;
	}
	Assert( pTexControls == pTexControlsEnd );		// The CELL_GCM macro bumps pTexControls along..
	
}


IDirect3DPixelShader9::~IDirect3DPixelShader9()
{
	MemAlloc_FreeAligned( m_data.m_eaFp );
}


HRESULT IDirect3DDevice9::SetPixelShader( IDirect3DPixelShader9* pShader )
{
	m_pixelShader = pShader;
	
	gpGcmDrawState->m_dirtyCachesMask |= CGcmDrawState::kDirtyPxShader;
	gpGcmDrawState->m_pPixelShaderData = m_pixelShader ? &pShader->m_data : 0;

	return S_OK;
}

HRESULT IDirect3DDevice9::SetPixelShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount)
{
	gpGcmDrawState->SetPixelShaderConstantF(StartRegister, (float*)pConstantData, Vector4fCount);
	return S_OK;
}

HRESULT IDirect3DDevice9::SetPixelShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
{
	// Not implemented on PS3!
	return S_OK;
}

HRESULT IDirect3DDevice9::SetPixelShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
{
	// Not implemented on PS3!
	return S_OK;
}


//--------------------------------------------------------------------------------------------------
// VERTEX SHADERS
//
// IDirect3DDevice9::	CreateVertexShader
// IDirect3DVertexShader9::~IDirect3DVertexShader9
// IDirect3DDevice9::	SetVertexShader
//						SetVertexShaderConstantF
//						SetVertexShaderConstantB
//--------------------------------------------------------------------------------------------------

HRESULT IDirect3DDevice9::CreateVertexShader(CONST DWORD* pFunction,IDirect3DVertexShader9** ppShader, char *debugLabel)
{
	IDirect3DVertexShader9 *newprog = new IDirect3DVertexShader9;
	newprog->m_pProgram = NULL; // don't copy (base member unused)

	//--------------------------------------------------------------------------------------------------
	// get program and in/out attr
	//--------------------------------------------------------------------------------------------------

	const CgBinaryProgram *prog = ( const CgBinaryProgram * )pFunction;
	CgBinaryVertexProgram * binaryVertexProgram = ( CgBinaryVertexProgram* ) ( ( char* )prog + prog->program );

	newprog->m_data.m_attributeInputMask = binaryVertexProgram->attributeInputMask;
	newprog->m_data.m_attributeOutputMask = binaryVertexProgram->attributeOutputMask;
	
	//--------------------------------------------------------------------------------------------------
	// Determine size of VP, allocate command buffer to set VP
	//--------------------------------------------------------------------------------------------------


	uint nReserveWords = AlignValue( cellGcmSetVertexProgramMeasureSizeInline( 0, ( const CGprogram )prog, ( ( uint8* )prog ) + prog->ucode ), 4 );
	if( nReserveWords > 4 * 1024 )
	{
		Error( "Vertex shader too big (%d words): won't fit into a single DMA, tell Sergiy to perform Large DMA transfer everywhere vertex shader command subbuffer is used\n", nReserveWords );
	}
	newprog->m_data.m_nVertexShaderCmdBufferWords = nReserveWords;
	newprog->m_data.m_pVertexShaderCmdBuffer = ( uint32* ) MemAlloc_AllocAligned( nReserveWords * sizeof( uint32 ), 16 );
	

	//--------------------------------------------------------------------------------------------------
	// Use GCM to output SetVertexProgram commands
	//--------------------------------------------------------------------------------------------------

	CellGcmContextData tempCtx;

	tempCtx.current = tempCtx.begin = newprog->m_data.m_pVertexShaderCmdBuffer; 
	tempCtx.end = tempCtx.begin + nReserveWords;
	tempCtx.callback = NULL;
	
	cellGcmSetVertexProgramUnsafeInline( &tempCtx, ( const CGprogram )prog, ( ( uint8* )prog ) + prog->ucode );
	
	Assert( tempCtx.current <= tempCtx.end && tempCtx.end - tempCtx.current < 4 );
	while( tempCtx.current < tempCtx.end )
	{
		*( tempCtx.current++ ) = CELL_GCM_METHOD_NOP; // make it 16-byte aligned
	}
	
	*ppShader = newprog;

	return S_OK;
}

IDirect3DVertexShader9::~IDirect3DVertexShader9()
{
	MemAlloc_FreeAligned( m_data.m_pVertexShaderCmdBuffer );
}

HRESULT IDirect3DDevice9::SetVertexShader( IDirect3DVertexShader9* pVertexShader )
{
	m_vertexShader = pVertexShader;

	gpGcmDrawState->m_dirtyCachesMask |= CGcmDrawState::kDirtyVxShader;
	gpGcmDrawState->m_pVertexShaderData = &pVertexShader->m_data;

	return S_OK;
}

HRESULT IDirect3DDevice9::SetVertexShaderConstantF( UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount )
{
	gpGcmDrawState->SetVertexShaderConstantF(StartRegister, (void*)pConstantData, Vector4fCount);
	return S_OK;
}


HRESULT IDirect3DDevice9::SetVertexShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
{
	gpGcmDrawState->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
	return S_OK;
}

HRESULT IDirect3DDevice9::SetVertexShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
{
	// Not implemented on PS3!
	return S_OK;
}

//--------------------------------------------------------------------------------------------------
// RENDERSTATES
//IDirect3DDevice9::SetRenderState
//					SetSamplerState
//					SetSamplerStatePart1
//--------------------------------------------------------------------------------------------------

struct	D3D_RSINFO
{
	int					m_class;
	D3DRENDERSTATETYPE	m_state;
	DWORD				m_defval;
	// m_class runs 0-3.
	// 3 = must implement - fully general - "obey"
	// 2 = implement setup to the default value (it has a GL effect but does not change later) "obey once"
	// 1 = "fake implement" setup to the default value no GL effect, debug break if anything but default value comes through - "ignore"
	// 0 = game never ever sets this one, break if someone even tries. "complain"
};

bool		g_D3DRS_INFO_unpacked_ready = false;	// set to true after unpack
D3D_RSINFO	g_D3DRS_INFO_unpacked[ D3DRS_VALUE_LIMIT+1 ];

#ifdef D3D_RSI
#error macro collision... rename this
#else
#define D3D_RSI(nclass,nstate,ndefval)	{ nclass, nstate, ndefval }
#endif

// FP conversions to hex courtesy of http://babbage.cs.qc.cuny.edu/IEEE-754/Decimal.html
#define	CONST_DZERO		0x00000000
#define	CONST_DONE		0x3F800000
#define	CONST_D64		0x42800000
#define	DONT_KNOW_YET	0x31415926


// see http://www.toymaker.info/Games/html/render_states.html

D3D_RSINFO	g_D3DRS_INFO_packed[] = 
{
	// these do not have to be in any particular order.  they get unpacked into the empty array above for direct indexing.

	D3D_RSI(	3,	D3DRS_ZENABLE,						DONT_KNOW_YET			),	// enable Z test (or W buffering)
	D3D_RSI(	3,	D3DRS_ZWRITEENABLE,					DONT_KNOW_YET			),	// enable Z write
	D3D_RSI(	3,	D3DRS_ZFUNC,						DONT_KNOW_YET			),	// select Z func

	D3D_RSI(	3,	D3DRS_COLORWRITEENABLE,				TRUE					),	// see transitiontable.cpp "APPLY_RENDER_STATE_FUNC( D3DRS_COLORWRITEENABLE, ColorWriteEnable )"

	D3D_RSI(	3,	D3DRS_CULLMODE,						D3DCULL_CCW				),	// backface cull control

	D3D_RSI(	3,	D3DRS_ALPHABLENDENABLE,				DONT_KNOW_YET			),	// ->CTransitionTable::ApplySeparateAlphaBlend and ApplyAlphaBlend
	D3D_RSI(	3,	D3DRS_BLENDOP,						D3DBLENDOP_ADD			),
	D3D_RSI(	3,	D3DRS_SRCBLEND,						DONT_KNOW_YET			),
	D3D_RSI(	3,	D3DRS_DESTBLEND,					DONT_KNOW_YET			),

	D3D_RSI(	1,	D3DRS_SEPARATEALPHABLENDENABLE,		FALSE					),	// hit in CTransitionTable::ApplySeparateAlphaBlend
	D3D_RSI(	1,	D3DRS_SRCBLENDALPHA,				D3DBLEND_ONE			),	// going to demote these to class 1 until I figure out if they are implementable
	D3D_RSI(	1,	D3DRS_DESTBLENDALPHA,				D3DBLEND_ZERO			),
	D3D_RSI(	1,	D3DRS_BLENDOPALPHA,					D3DBLENDOP_ADD			),

	// what is the deal with alpha test... looks like it is inited to off.
	D3D_RSI(	3,	D3DRS_ALPHATESTENABLE,				0						),
	D3D_RSI(	3,	D3DRS_ALPHAREF,						0						),
	D3D_RSI(	3,	D3DRS_ALPHAFUNC,					D3DCMP_GREATEREQUAL		),

	D3D_RSI(	3,	D3DRS_STENCILENABLE,				FALSE					),
	D3D_RSI(	3,	D3DRS_STENCILFAIL,					D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_STENCILZFAIL,					D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_STENCILPASS,					D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_STENCILFUNC,					D3DCMP_ALWAYS			),
	D3D_RSI(	3,	D3DRS_STENCILREF,					0						),
	D3D_RSI(	3,	D3DRS_STENCILMASK,					0xFFFFFFFF				),
	D3D_RSI(	3,	D3DRS_STENCILWRITEMASK,				0xFFFFFFFF				),

	D3D_RSI(	3,	D3DRS_TWOSIDEDSTENCILMODE,			FALSE					),
	D3D_RSI(	3,	D3DRS_CCW_STENCILFAIL,				D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_CCW_STENCILZFAIL,				D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_CCW_STENCILPASS,				D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_CCW_STENCILFUNC,				D3DCMP_ALWAYS 			),

	D3D_RSI(	3,	D3DRS_FOGENABLE,					FALSE					),	// see CShaderAPIDx8::FogMode and friends - be ready to do the ARB fog linear option madness
	D3D_RSI(	3,	D3DRS_FOGCOLOR,						0						),
	D3D_RSI(	3,	D3DRS_FOGTABLEMODE,					D3DFOG_NONE				),
	D3D_RSI(	3,	D3DRS_FOGSTART,						CONST_DZERO				),
	D3D_RSI(	3,	D3DRS_FOGEND,						CONST_DONE				),
	D3D_RSI(	3,	D3DRS_FOGDENSITY,					CONST_DZERO				),
	D3D_RSI(	3,	D3DRS_RANGEFOGENABLE,				FALSE					),
	D3D_RSI(	3,	D3DRS_FOGVERTEXMODE,				D3DFOG_NONE				),	// watch out for CShaderAPIDx8::CommitPerPassFogMode....

	D3D_RSI(	3,	D3DRS_MULTISAMPLEANTIALIAS,			TRUE					),
	D3D_RSI(	3,	D3DRS_MULTISAMPLEMASK,				0xFFFFFFFF				),

	D3D_RSI(	3,	D3DRS_SCISSORTESTENABLE,			FALSE					),	// heed IDirect3DDevice9::SetScissorRect

	D3D_RSI(	3,	D3DRS_DEPTHBIAS,					CONST_DZERO				),
	D3D_RSI(	3,	D3DRS_SLOPESCALEDEPTHBIAS,			CONST_DZERO				),

	D3D_RSI(	3,	D3DRS_COLORWRITEENABLE1,			0x0000000f				),
	D3D_RSI(	3,	D3DRS_COLORWRITEENABLE2,			0x0000000f				),
	D3D_RSI(	3,	D3DRS_COLORWRITEENABLE3,			0x0000000f				),

	D3D_RSI(	3,	D3DRS_SRGBWRITEENABLE,				0						),	// heeded but ignored..

	D3D_RSI(	2,	D3DRS_CLIPPING,						TRUE					),	// um, yeah, clipping is enabled (?)
	D3D_RSI(	3,	D3DRS_CLIPPLANEENABLE,				0						),	// mask 1<<n of active user clip planes.

	D3D_RSI(	0,	D3DRS_LIGHTING,						0						),	// strange, someone turns it on then off again. move to class 0 and just ignore it (lie)?

	D3D_RSI(	3,	D3DRS_FILLMODE,						D3DFILL_SOLID			),

	D3D_RSI(	1,	D3DRS_SHADEMODE,					D3DSHADE_GOURAUD		),
	D3D_RSI(	1,	D3DRS_LASTPIXEL,					TRUE					),
	D3D_RSI(	1,	D3DRS_DITHERENABLE,					0						),	//set to false by game, no one sets it to true
	D3D_RSI(	1,	D3DRS_SPECULARENABLE,				FALSE					),
	D3D_RSI(	1,	D3DRS_TEXTUREFACTOR,				0xFFFFFFFF				),	// watch out for CShaderAPIDx8::Color3f et al.
	D3D_RSI(	1,	D3DRS_WRAP0,						0						),
	D3D_RSI(	1,	D3DRS_WRAP1,						0						),
	D3D_RSI(	1,	D3DRS_WRAP2,						0						),
	D3D_RSI(	1,	D3DRS_WRAP3,						0						),
	D3D_RSI(	1,	D3DRS_WRAP4,						0						),
	D3D_RSI(	1,	D3DRS_WRAP5,						0						),
	D3D_RSI(	1,	D3DRS_WRAP6,						0						),
	D3D_RSI(	1,	D3DRS_WRAP7,						0						),
	D3D_RSI(	1,	D3DRS_AMBIENT,						0						),	// FF lighting, no
	D3D_RSI(	1,	D3DRS_COLORVERTEX,					TRUE					),	// FF lighing again
	D3D_RSI(	1,	D3DRS_LOCALVIEWER,					TRUE					),	// FF lighting
	D3D_RSI(	1,	D3DRS_NORMALIZENORMALS,				FALSE					),	// FF mode I think.  CShaderAPIDx8::SetVertexBlendState says it might switch this on when skinning is in play
	D3D_RSI(	1,	D3DRS_DIFFUSEMATERIALSOURCE,		D3DMCS_MATERIAL			),	// hit only in CShaderAPIDx8::ResetRenderState
	D3D_RSI(	1,	D3DRS_SPECULARMATERIALSOURCE,		D3DMCS_COLOR2			),
	D3D_RSI(	1,	D3DRS_AMBIENTMATERIALSOURCE,		D3DMCS_MATERIAL			),
	D3D_RSI(	1,	D3DRS_EMISSIVEMATERIALSOURCE,		D3DMCS_MATERIAL			),
	D3D_RSI(	1,	D3DRS_VERTEXBLEND,					D3DVBF_DISABLE			),	// also being set by CShaderAPIDx8::SetVertexBlendState, so might be FF
	D3D_RSI(	1,	D3DRS_POINTSIZE,					CONST_DONE				),
	D3D_RSI(	1,	D3DRS_POINTSIZE_MIN,				CONST_DONE				),
	D3D_RSI(	1,	D3DRS_POINTSPRITEENABLE,			FALSE					),
	D3D_RSI(	1,	D3DRS_POINTSCALEENABLE,				FALSE					),
	D3D_RSI(	1,	D3DRS_POINTSCALE_A,					CONST_DONE				),
	D3D_RSI(	1,	D3DRS_POINTSCALE_B,					CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_POINTSCALE_C,					CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_PATCHEDGESTYLE,				D3DPATCHEDGE_DISCRETE	),
	D3D_RSI(	1,	D3DRS_DEBUGMONITORTOKEN,			D3DDMT_ENABLE			),
	D3D_RSI(	1,	D3DRS_POINTSIZE_MAX,				CONST_D64				),
	D3D_RSI(	1,	D3DRS_INDEXEDVERTEXBLENDENABLE,		FALSE					),
	D3D_RSI(	1,	D3DRS_TWEENFACTOR,					CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_POSITIONDEGREE,				D3DDEGREE_CUBIC			),
	D3D_RSI(	1,	D3DRS_NORMALDEGREE,					D3DDEGREE_LINEAR		),
	D3D_RSI(	1,	D3DRS_ANTIALIASEDLINEENABLE,		FALSE					),	// just ignore it
	D3D_RSI(	1,	D3DRS_MINTESSELLATIONLEVEL,			CONST_DONE				),
	D3D_RSI(	1,	D3DRS_MAXTESSELLATIONLEVEL,			CONST_DONE				),
	D3D_RSI(	1,	D3DRS_ADAPTIVETESS_X,				CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_ADAPTIVETESS_Y,				CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_ADAPTIVETESS_Z,				CONST_DONE				),
	D3D_RSI(	1,	D3DRS_ADAPTIVETESS_W,				CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_ENABLEADAPTIVETESSELLATION,	FALSE					),
	D3D_RSI(	1,	D3DRS_BLENDFACTOR,					0xffffffff				),
	D3D_RSI(	1,	D3DRS_WRAP8,						0						),
	D3D_RSI(	1,	D3DRS_WRAP9,						0						),
	D3D_RSI(	1,	D3DRS_WRAP10,						0						),
	D3D_RSI(	1,	D3DRS_WRAP11,						0						),
	D3D_RSI(	1,	D3DRS_WRAP12,						0						),
	D3D_RSI(	1,	D3DRS_WRAP13,						0						),
	D3D_RSI(	1,	D3DRS_WRAP14,						0						),
	D3D_RSI(	1,	D3DRS_WRAP15,						0						),
	D3D_RSI(	-1,	(D3DRENDERSTATETYPE)0,				0						)	// terminator
};


uint FindOrInsert( CUtlVector<uint32> &arrDefValues, uint32 nDefValue )
{
	// the def value array is supposed to be VERY short, so linear search is faster than binary
	Assert( arrDefValues.Count() < 16 );
	for( uint i = 0; i < arrDefValues.Count(); ++i )
	{
		if( arrDefValues[i] == nDefValue )
			return i;
	}
	arrDefValues.AddToTail( nDefValue );
	return arrDefValues.Count() - 1 ;
}



void	UnpackD3DRSITable( void )
{
	V_memset (g_D3DRS_INFO_unpacked, 0, sizeof(g_D3DRS_INFO_unpacked) );

	for( D3D_RSINFO *packed = g_D3DRS_INFO_packed; packed->m_class >= 0; packed++ )
	{
		if ( (packed->m_state <0) || (packed->m_state >= D3DRS_VALUE_LIMIT) )
		{
			// bad
			Debugger();
		}
		else
		{
			// dispatch it to the unpacked array
			g_D3DRS_INFO_unpacked[ packed->m_state ] = *packed;
		}
	}

}

HRESULT IDirect3DDevice9::SetRenderState(D3DRENDERSTATETYPE State,DWORD Value)
{
	gpGcmDrawState->SetRenderState(State, Value);
	return S_OK;
}


HRESULT IDirect3DDevice9::SetSamplerState(DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD Value)
{
	gpGcmDrawState->SetSamplerState(Sampler, Type, Value);
	return S_OK;
}

//--------------------------------------------------------------------------------------------------
// VERTEX DECLS, STREAMS, BUFFERS, INDICES
// IDirect3DDevice9::	CreateVertexDeclaration
// 						SetVertexDeclaration
// 						CreateVertexBuffer
// 						SetVertexStreamSource
// 						SetStreamSource
//						FlushVertexCache
// 						SetRawHardwareDataStreams
// IDirect3DIndexBuffer9::GetDesc
// IDirect3DDevice9::	CreateIndexBuffer
//						SetIndices
//						ValidateDrawPrimitiveStreams
//--------------------------------------------------------------------------------------------------

// Lookup table used by CreateVertexDeclaration

unsigned char g_D3DDeclFromPSGL_UsageMappingTable[] =
{
	/*0x00*/	0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0x10*/	1, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0x20*/	7, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0x30*/	2, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0x40*/	6, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0x50*/	8, 9, 0xA, 0xB,		0xC, 0xD, 0xE, 0xF,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0x60*/	~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0x70*/	~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0x80*/	5, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0x90*/	~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0xA0*/	3, 4, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0xB0*/	~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0xC0*/	~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0xD0*/	~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0xE0*/	~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
	/*0xF0*/	~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,		~0, ~0, ~0, ~0,
};


HRESULT IDirect3DDevice9::CreateVertexDeclaration( CONST D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl)
{
	*ppDecl = NULL;
	
	// the goal here is to arrive at something which lets us quickly generate GLMVertexSetups.

	// the information we don't have, that must be inferred from the decls, is:
	// -> how many unique streams (buffers) are used - pure curiosity
	// -> what the stride and offset is for each decl.  Size you can figure out on the spot, stride requires surveying all the components in each stream first.
	//	so init an array of per-stream offsets to 0.
	//	each one is a cursor that gets bumped by decls.
	uint	streamOffsets[ D3D_MAX_STREAMS ];
	
	memset( streamOffsets, 0, sizeof( streamOffsets ) );

	IDirect3DVertexDeclaration9 *decl9 = new IDirect3DVertexDeclaration9;
	Assert( !( uintp( decl9 ) & 0xF ) );
	
	decl9->m_elemCount = 0;
			
	for (const D3DVERTEXELEMENT9 *src = pVertexElements; (src->Stream != 0xFF); src++)
	{
		// element
		D3DVERTEXELEMENT9_GCM *elem = &decl9->m_elements[ decl9->m_elemCount++ ];

		// copy the D3D decl wholesale.
		elem->m_dxdecl = *src;

		// On PS3:
		// TEXCOORD4 == POSITION1	(this semantic doesn't exist in Cg, see #define in common_vs_fxc.h)
		// TEXCOORD5 == NORMAL1 	(this semantic doesn't exist in Cg, see #define in common_vs_fxc.h)
		// TEXCOORD6 == TANGENT		(Cg remaps this automatically)
		// TEXCOORD7 == BINORMAL	(Cg remaps this automatically)

		if ( elem->m_dxdecl.Usage == D3DDECLUSAGE_TANGENT )
		{
			Assert( elem->m_dxdecl.UsageIndex == 0 );
			elem->m_dxdecl.Usage = D3DDECLUSAGE_TEXCOORD;
			elem->m_dxdecl.UsageIndex = 6;
		}
		else if ( elem->m_dxdecl.Usage == D3DDECLUSAGE_BINORMAL )
		{
			Assert( elem->m_dxdecl.UsageIndex == 0 );
			elem->m_dxdecl.Usage = D3DDECLUSAGE_TEXCOORD;
			elem->m_dxdecl.UsageIndex = 7;
		}
		else if ( elem->m_dxdecl.Usage == D3DDECLUSAGE_POSITION && elem->m_dxdecl.UsageIndex >= 1 )
		{
			Assert( elem->m_dxdecl.UsageIndex == 1 );
			elem->m_dxdecl.Usage = D3DDECLUSAGE_TEXCOORD;
			elem->m_dxdecl.UsageIndex = 4;
		}
		else if ( elem->m_dxdecl.Usage == D3DDECLUSAGE_NORMAL && elem->m_dxdecl.UsageIndex >= 1 )
		{
			Assert( elem->m_dxdecl.UsageIndex == 1 );
			elem->m_dxdecl.Usage = D3DDECLUSAGE_TEXCOORD;
			elem->m_dxdecl.UsageIndex = 5;
		}
				
		// latch current offset in this stream.
		elem->m_gcmdecl.m_offset = streamOffsets[ elem->m_dxdecl.Stream ];
		
		// figure out size of this attr and move the cursor
		// if cursor was on zero, bump the active stream count
		
		int bytes = 0;
		switch( elem->m_dxdecl.Type )
		{
			case D3DDECLTYPE_FLOAT1:	elem->m_gcmdecl.m_datasize = 1; elem->m_gcmdecl.m_datatype = CELL_GCM_VERTEX_F; bytes = 4; break;
			case D3DDECLTYPE_FLOAT2:	elem->m_gcmdecl.m_datasize = 2; elem->m_gcmdecl.m_datatype = CELL_GCM_VERTEX_F; bytes = 8; break;
			//case D3DVSDT_FLOAT3:
			case D3DDECLTYPE_FLOAT3:	elem->m_gcmdecl.m_datasize = 3; elem->m_gcmdecl.m_datatype = CELL_GCM_VERTEX_F; bytes = 12; break;
			//case D3DVSDT_FLOAT4:
			case D3DDECLTYPE_FLOAT4:	elem->m_gcmdecl.m_datasize = 4; elem->m_gcmdecl.m_datatype = CELL_GCM_VERTEX_F; bytes = 16; break;

			case D3DDECLTYPE_SHORT2:	elem->m_gcmdecl.m_datasize = 2; elem->m_gcmdecl.m_datatype = CELL_GCM_VERTEX_S32K; bytes = 4; break;
			case D3DDECLTYPE_UBYTE4:	elem->m_gcmdecl.m_datasize = 4; elem->m_gcmdecl.m_datatype = CELL_GCM_VERTEX_UB256; bytes = 4; break;
			case D3DDECLTYPE_UBYTE4N:	elem->m_gcmdecl.m_datasize = 4; elem->m_gcmdecl.m_datatype = CELL_GCM_VERTEX_UB; bytes = 4; break;

			// case D3DVSDT_UBYTE4:		
			case D3DDECLTYPE_D3DCOLOR:
				// pass 4 UB's but we know this is out of order compared to D3DCOLOR data
				elem->m_gcmdecl.m_datasize = 4; elem->m_gcmdecl.m_datatype = CELL_GCM_VERTEX_UB;
				bytes = 4;
			break;

			default:	Debugger(); return (HRESULT)-1; break;

		}
		
		// write the offset and move the cursor
		streamOffsets[ elem->m_dxdecl.Stream ] += bytes;
		
		// elem count was already bumped.
	}

	// the loop is done, we now know how many active streams there are, how many atribs are active in the declaration,
	// and how big each one is in terms of stride.

	// PS3 has fixed semantics of 16 attributes, to avoid searches later when
	// binding to slots perform the search now once:
	memset( decl9->m_cgAttrSlots, 0, sizeof( decl9->m_cgAttrSlots ) );
	for ( int j = 0; j < decl9->m_elemCount; ++ j )
	{
		D3DVERTEXELEMENT9_GCM	*elem = &decl9->m_elements[ j ];
		unsigned char uchType = ( ( elem->m_dxdecl.Usage & 0xF ) << 4 ) | ( elem->m_dxdecl.UsageIndex & 0xF );
		unsigned char chType = g_D3DDeclFromPSGL_UsageMappingTable[ uchType ];
		if ( chType < ARRAYSIZE( decl9->m_cgAttrSlots ) )
		{
			if ( !decl9->m_cgAttrSlots[chType] )
			{
				decl9->m_cgAttrSlots[chType] = j + 1;
			}
			else
			{
				// An input element has already been mapped to this slot. 
				// This can happen when the vertex decl uses POSITION1/NORMAL1 (flex deltas), which we map to TEXCOORD4/5, and the decl already uses TEXCOORD4/5. 
				// For now we ignore these elements, which it turns out are always unused when the vertex shader actually uses TEXCOORD4/5.
			}
		}
		else
		{
			Assert( false );
		}
	}
	
	*ppDecl = decl9;

	return S_OK;
}

HRESULT IDirect3DDevice9::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl)
{
	// we just latch it.  At draw time we combine the current vertex decl with the current stream set and generate a vertex setup for GLM.
	// GLM can see what the differences are and act accordingly to adjust vert attrib bindings.

	m_vertDecl = pDecl;
	
	return S_OK;
}

HRESULT IDirect3DDevice9::SetFVF(DWORD FVF)
{
	Debugger();

	return S_OK;
}

HRESULT IDirect3DDevice9::GetFVF(DWORD* pFVF)
{
	Debugger();

	return S_OK;
}

HRESULT IDirect3DDevice9::CreateVertexBuffer(UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer9** ppVertexBuffer,VD3DHANDLE* pSharedHandle)
{
	IDirect3DVertexBuffer9 *pNewVertexBuffer = new IDirect3DVertexBuffer9;
	//pNewVertexBuffer->m_restype = D3DRTYPE_VERTEXBUFFER;		hmmmmmmm why are we not derived from d3dresource..
	
	CPs3gcmAllocationType_t eAllocType = kAllocPs3GcmVertexBuffer;
	if ( Usage & D3DUSAGE_EDGE_DMA_INPUT )
		eAllocType = kAllocPs3GcmVertexBufferDma;
	else if ( Usage & D3DUSAGE_DYNAMIC )
		eAllocType = kAllocPs3GcmVertexBufferDynamic;
	pNewVertexBuffer->m_pBuffer = CPs3gcmBuffer::New( Length, eAllocType );
	
	pNewVertexBuffer->m_vtxDesc.Type		= D3DRTYPE_VERTEXBUFFER;
	pNewVertexBuffer->m_vtxDesc.Usage	= Usage;
	pNewVertexBuffer->m_vtxDesc.Pool		= Pool;
	pNewVertexBuffer->m_vtxDesc.Size		= Length;

	*ppVertexBuffer = pNewVertexBuffer;
	
	return S_OK;
}

inline void IDirect3DDevice9::SetVertexStreamSource( uint nStreamIndex, IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride )
{
	gpGcmDrawState->SetVertexStreamSource(nStreamIndex, pStreamData, OffsetInBytes, Stride);
}


HRESULT IDirect3DDevice9::SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride)
{
	// perfectly legal to see a vertex buffer of NULL get passed in here.
	// so we need an array to track these.
	// OK, we are being given the stride, we don't need to calc it..
	
	SetVertexStreamSource( StreamNumber, pStreamData, OffsetInBytes, Stride );
	
	return S_OK;
}


HRESULT IDirect3DDevice9::SetRawHardwareDataStreams( IDirect3DVertexBuffer9** ppRawHardwareDataStreams )
{
// Unused on PS3
// 	if ( ppRawHardwareDataStreams )
// 	{
// 		V_memcpy( gpGcmDrawState->m_arrRawHardwareDataStreams, ppRawHardwareDataStreams, sizeof( gpGcmDrawState->m_arrRawHardwareDataStreams ) );
// 	}
// 	else
// 	{
// 		V_memset( gpGcmDrawState->m_arrRawHardwareDataStreams, 0, sizeof( gpGcmDrawState->m_arrRawHardwareDataStreams ) );
// 	}
	return S_OK;
}

void IDirect3DDevice9::FlushVertexCache()
{
	gpGcmDrawState->SetInvalidateVertexCache();
}

HRESULT IDirect3DIndexBuffer9::GetDesc(D3DINDEXBUFFER_DESC *pDesc)
{
	*pDesc = m_idxDesc;
	return S_OK;
}

// index buffers
HRESULT IDirect3DDevice9::CreateIndexBuffer(UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer9** ppIndexBuffer,VD3DHANDLE* pSharedHandle)
{
	// it is important to save all the create info, since GetDesc could get called later to query it
	
	IDirect3DIndexBuffer9 *pNewIndexBuffer = new IDirect3DIndexBuffer9;
	
	CPs3gcmAllocationType_t eAllocType = kAllocPs3GcmIndexBuffer;
	if ( Usage & D3DUSAGE_EDGE_DMA_INPUT )
		eAllocType = kAllocPs3GcmIndexBufferDma;
	else if ( Usage & D3DUSAGE_DYNAMIC )
		eAllocType = kAllocPs3GcmIndexBufferDynamic;
	pNewIndexBuffer->m_pBuffer = CPs3gcmBuffer::New( Length, eAllocType );
	
	pNewIndexBuffer->m_idxDesc.Format	= Format;
	pNewIndexBuffer->m_idxDesc.Type		= D3DRTYPE_INDEXBUFFER;
	pNewIndexBuffer->m_idxDesc.Usage	= Usage;
	pNewIndexBuffer->m_idxDesc.Pool		= Pool;
	pNewIndexBuffer->m_idxDesc.Size		= Length;

	*ppIndexBuffer = pNewIndexBuffer;
	return S_OK;
}

HRESULT IDirect3DDevice9::SetIndices( IDirect3DIndexBuffer9* pIndexData )
{
	// just latch it.
	m_indices.m_idxBuffer = pIndexData;

	return S_OK;
}

ConVar r_ps3_validatestreams( "r_ps3_validatestreams", "0", FCVAR_DEVELOPMENTONLY );

HRESULT	IDirect3DDevice9::ValidateDrawPrimitiveStreams( D3DPRIMITIVETYPE Type, UINT baseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount )
{
	return S_OK;
}

//--------------------------------------------------------------------------------------------------
// DRAW
//
// IDirect3DDevice9::	DrawPrimitive
// 						DrawPrimitiveUP
// 						DrawIndexedPrimitive
// 						Clear
//--------------------------------------------------------------------------------------------------

HRESULT IDirect3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
{
	Debugger();
	return S_OK;
}

void IDirect3DDevice9::DrawPrimitiveUP( D3DPRIMITIVETYPE nPrimitiveType,UINT nPrimitiveCount,
									    CONST void *pVertexStreamZeroData, UINT nVertexStreamZeroStride )
{
	gpGcmDrawState->DrawPrimitiveUP(m_vertDecl, nPrimitiveType, nPrimitiveCount, pVertexStreamZeroData, nVertexStreamZeroStride);
}

HRESULT IDirect3DDevice9::DrawIndexedPrimitive( D3DPRIMITIVETYPE Type,INT BaseVertexIndex,UINT MinVertexIndex,
											    UINT NumVertices,UINT startIndex,UINT nDrawPrimCount )
{
	uint32 offset = m_indices.m_idxBuffer->m_pBuffer->Offset();

	gpGcmDrawState->DrawIndexedPrimitive(offset, m_vertDecl, Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, nDrawPrimCount );

	return S_OK;
}


HRESULT IDirect3DDevice9::Clear( DWORD Count, CONST D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil )
{
	uint32 depth = ( ( m_dsSurface ) && ( m_dsSurface->m_desc.Format == D3DFMT_D16 ) ) ? 16 : 32;
	
	gpGcmDrawState->ClearSurface(Flags, Color, Z, Stencil, depth );

	return S_OK;
}


HRESULT IDirect3DDevice9::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
	Debugger();
	return S_OK;
}

//--------------------------------------------------------------------------------------------------
// Zpass
//--------------------------------------------------------------------------------------------------

void IDirect3DDevice9::BeginZPass( DWORD Flags )
{
// 	Assert( Flags == 0 );
// 	if( !m_isZPass )
// 	{
// 		m_isZPass = g_spuGcm.BeginZPass();
// 	}
}

void IDirect3DDevice9::SetPredication( DWORD PredicationMask )
{
// 	Assert( PredicationMask == 0 || PredicationMask == D3DPRED_ALL_RENDER || PredicationMask == D3DPRED_ALL_Z || PredicationMask == ( D3DPRED_ALL_RENDER | D3DPRED_ALL_Z ) );
// 	g_spuGcm.SetPredication( PredicationMask );
}

HRESULT IDirect3DDevice9::EndZPass()
{
// 	if( m_isZPass )
// 	{
// 		g_spuGcm.EndZPass( true ); // ZPass may have ended prematurely, we still pop the marker
// 		m_isZPass = false;
// 	}
	return S_OK;
}

//--------------------------------------------------------------------------------------------------
// Anti-Aliasing
//--------------------------------------------------------------------------------------------------

ConVar r_mlaa_hints("r_mlaa_hints", "1");

void IDirect3DDevice9::StartRenderingIntoPreviousFramebuffer()
{
// 	g_flipHandler.QmsAdviceBeforeDrawPrevFramebuffer();
// 	m_defaultColorSurface->m_tex->m_lmBlock.Assign( g_ps3gcmGlobalState.m_display.surfaceColor[ g_ps3gcmGlobalState.m_display.PrevSurfaceIndex( 1 ) ] );
// 	if( m_defaultColorSurface == m_rtSurfaces[ 0 ] )
// 	{
// 		Ps3Helper_UpdateSurface( 0 );
// 	}
}

void IDirect3DDevice9::AntiAliasingHint( int nHint )
{
// 	if( 0 && !IsCert() && IsDebug() )
// 	{
// 		const char * pAaHintReadableNames[] = {
// 			"AA_HINT_MESHES",
// 			"AA_HINT_TEXT",
// 			"AA_HINT_DEBUG_TEXT",
// 			"AA_HINT_HEAVY_UI_OVERLAY",
// 			"AA_HINT_ALIASING_PUSH",
// 			"AA_HINT_ALIASING_POP",
// 			"AA_HINT_POSTPROCESS",
// 			"AA_HINT_MOVIE", 
// 			"AA_HINT_MENU"
// 		};
// 
// 		Msg( "AntiAliasingHint( %s )\n", nHint >= ARRAYSIZE( pAaHintReadableNames ) ? "Out Of Range" : pAaHintReadableNames[ nHint ] );
// 	}
// 
// 	switch( nHint )
// 	{
// 		case AA_HINT_HEAVY_UI_OVERLAY:
// 			if( r_mlaa_hints.GetInt() & 2 )
// 			{
// 				g_spuGcm.DrawQueueNormal();
// 				
// 				StartRenderingIntoPreviousFramebuffer();
// 
// 				// from now and until the end of frame, render into previous surface (or in this surface, but deferred to next frame)
// 				// this means we'll have a potential without VGUI rendering and proper post-processing 
// 				m_nAntiAliasingStatus = AA_STATUS_PREV_FRAME;
// 			}
// 			g_spuGcm.DisableMlaa();
// 			//g_spuGcm.DisableMlaaForTwoFrames();
// 			break;
// 			
// 		case AA_HINT_ALIASING_PUSH:
// 			g_spuGcm.DisableMlaaPermanently();
// 			break;
// 
// 		case AA_HINT_ALIASING_POP:
// 			g_flipHandler.EnableMlaaPermannetly();
// 			break;
// 			
// 		case AA_HINT_MOVIE:
// 		case AA_HINT_MENU:
// 			//g_spuGcm.DrawQueueNormal();
// 			g_spuGcm.DisableMlaa();
// 			// if drawing into previous frame, do we need to continue that? both ways will probably work
// 		break;
// 			
// 		case AA_HINT_MESHES:
// 			if( g_flipHandler.IsMlaaEnabled() && m_nAntiAliasingStatus != AA_STATUS_NORMAL )
// 			{
// 				// switch back to default surface now...
// 				if( g_spuGcm.m_bUseDeferredDrawQueue )
// 				{
// 					g_spuGcm.DrawQueueNormal();
// 					if( m_nAntiAliasingStatus == AA_STATUS_PREV_FRAME )
// 					{
// 						// the first time this frame we need to set deferred queue surface
// 						Ps3Helper_UpdateSurface( 0 );
// 					}
// 				}
// 				else
// 				{
// 					m_defaultColorSurface->m_tex->m_lmBlock.Assign( g_ps3gcmGlobalState.m_display.surfaceColor[ g_ps3gcmGlobalState.m_display.surfaceFlipIdx ] );
// 					if( m_defaultColorSurface == m_rtSurfaces[ 0 ] )
// 					{
// 						Ps3Helper_UpdateSurface( 0 );
// 					}
// 					// from now and until the end of frame, render into previous surface
// 					// this means we'll have a potential without VGUI rendering and proper post-processing when we 
// 				}
// 				m_nAntiAliasingStatus = AA_STATUS_NORMAL;
// 				GCM_PERF_MARKER( "AntiAliasing_ON" );
// 			}
// 		break;
// 
// 		case AA_HINT_DEBUG_TEXT:
// 			if( g_flipHandler.IsMlaaEnabled() && m_nAntiAliasingStatus != AA_STATUS_PREV_FRAME && r_mlaa_hints.GetBool() )
// 			{
// 				// switch not normal ( non-deferred ) drawing in case we peruse deferred drawing
// 				if( g_spuGcm.m_bUseDeferredDrawQueue )
// 				{
// 					g_spuGcm.DrawQueueNormal();
// 				}
// 
// 				StartRenderingIntoPreviousFramebuffer();
// 				
// 				m_nAntiAliasingStatus = AA_STATUS_PREV_FRAME;
// 			}
// 			break;
// 		
// 		case AA_HINT_TEXT:
// 		case AA_HINT_POSTPROCESS:
// 			if( g_flipHandler.IsMlaaEnabled() && m_nAntiAliasingStatus != AA_STATUS_DEFERRED && r_mlaa_hints.GetBool() )
// 			{
// 				GCM_PERF_MARKER("AntiAliasing_OFF");
// 				// switch back to default surface now...
// 				if( g_spuGcm.m_bUseDeferredDrawQueue )
// 				{
// 					if( g_spuGcm.DrawQueueDeferred().isFirstInFrame ) // this doesn't do actual rendering, so we don't need to switch render surface to previous frame
// 					{
// 						// this is the first time this frame, so record switching the surface
// 						// even if the surface is not framebuffer, we still need to record it
// 						g_spuGcm.OpenDeferredChunk( SPUDRAWQUEUE_DEFERRED_GCMFLUSH_DRAW_METHOD );
// 						Ps3Helper_UpdateSurface( 0 );
// 						g_spuGcm.OpenDeferredChunk( );
// 					}
// 					if( g_spuGcm.IsDeferredDrawQueue() ) 
// 					{
// 						// we should've succeeded; if we didn't it means we ran out of memory or something. TODO: test the failure code path
// 						m_nAntiAliasingStatus = AA_STATUS_DEFERRED;
// 					}
// 				}
// 				else
// 				{
// 					StartRenderingIntoPreviousFramebuffer();
// 
// 					// from now and until the end of frame, render into previous surface (or in this surface, but deferred to next frame)
// 					// this means we'll have a potential without VGUI rendering and proper post-processing 
// 					m_nAntiAliasingStatus = AA_STATUS_PREV_FRAME;
// 				}
// 			}
// 		break;
// 		
// 	}
}

//--------------------------------------------------------------------------------------------------
// IDirect3DDevice9::	CreateQuery
// 						QueryGlobalStateFence_t::PrepareForQuery
// 						QueryGlobalStateOcclusion_t::PrepareForQuery
// IDirect3DQuery9::Issue
//					GetData
//--------------------------------------------------------------------------------------------------

IDirect3DQuery9::QueryGlobalStateOcclusion_t IDirect3DQuery9::s_GlobalStateOcclusion;

uint32 IDirect3DQuery9::QueryGlobalStateOcclusion_t::PrepareForQuery()
{
	uint32 uiQuery = (m_queryIdx ++) % kMaxQueries;
	if ( !m_Values[uiQuery] )
		m_Values[uiQuery] = cellGcmGetReportDataAddress( uiQuery + QueryGlobalStateOcclusion_t::kGcmQueryBase );
	m_Values[ uiQuery ]->zero = ~0;
	return uiQuery;
}

IDirect3DQuery9::QueryGlobalStateFence_t IDirect3DQuery9::s_GlobalStateFence;

uint32 IDirect3DQuery9::QueryGlobalStateFence_t::PrepareForQuery()
{
	uint32 uiQuery = (m_queryIdx ++) % kMaxQueries;
	if ( !m_Values[uiQuery] )
		m_Values[uiQuery] = cellGcmGetLabelAddress( uiQuery + QueryGlobalStateFence_t::kGcmLabelBase );
	(*m_Values[uiQuery]) = ~0;
	return uiQuery;
}

HRESULT IDirect3DDevice9::CreateQuery(D3DQUERYTYPE Type,IDirect3DQuery9** ppQuery)
{
	IDirect3DQuery9	*newquery = new IDirect3DQuery9;
	newquery->m_type = Type;
	newquery->m_queryIdx = ~0;

	switch ( Type )
	{
	case	D3DQUERYTYPE_OCCLUSION:				/* D3DISSUE_BEGIN, D3DISSUE_END */
		// newquery->m_query = newquery->s_GlobalStateOcclusion.PrepareForQuery();
		break;

	case	D3DQUERYTYPE_EVENT:					/* D3DISSUE_END */
		// newquery->m_query = newquery->s_GlobalStateFence.PrepareForQuery();
		break;

	case	D3DQUERYTYPE_RESOURCEMANAGER:		/* D3DISSUE_END */
	case	D3DQUERYTYPE_TIMESTAMP:				/* D3DISSUE_END */
	case	D3DQUERYTYPE_TIMESTAMPFREQ:			/* D3DISSUE_END */
	case	D3DQUERYTYPE_INTERFACETIMINGS:		/* D3DISSUE_BEGIN, D3DISSUE_END */
	case	D3DQUERYTYPE_PIXELTIMINGS:			/* D3DISSUE_BEGIN, D3DISSUE_END */
	case	D3DQUERYTYPE_CACHEUTILIZATION:		/* D3DISSUE_BEGIN, D3DISSUE_END */
		Assert( !"Un-implemented query type" );
		break;

	default:
		Assert( !"Unknown query type" );
		break;
	}

	*ppQuery = newquery;

	return S_OK;
}

HRESULT IDirect3DQuery9::Issue(DWORD dwIssueFlags)
{
	// Flags field for Issue
	//	#define D3DISSUE_END (1 << 0) // Tells the runtime to issue the end of a query, changing it's state to "non-signaled".
	//	#define D3DISSUE_BEGIN (1 << 1) // Tells the runtime to issue the beginng of a query.

	if (dwIssueFlags & D3DISSUE_BEGIN)
	{
		switch( m_type )
		{
		case	D3DQUERYTYPE_OCCLUSION:
			if ( !( m_queryIdx & kQueryFinished ) )
			{
				// Query is still pending!
				Assert( 0 );
				return S_OK;
			}
			m_queryIdx = s_GlobalStateOcclusion.PrepareForQuery();

			gpGcmDrawState->SetZpassPixelCountEnable( CELL_GCM_TRUE ); 
			gpGcmDrawState->SetClearReport( CELL_GCM_ZPASS_PIXEL_CNT );
			break;

		default:
			Assert(!"Can't use D3DISSUE_BEGIN on this query");
			break;
		}
	}

	if (dwIssueFlags & D3DISSUE_END)
	{
		switch( m_type )
		{
		case	D3DQUERYTYPE_OCCLUSION:
			if ( !!( m_queryIdx & kQueryFinished ) )
			{
				// Query has finished earlier!
				Assert( 0 );
				return S_OK;
			}
			gpGcmDrawState->SetReport ( CELL_GCM_ZPASS_PIXEL_CNT, m_queryIdx + QueryGlobalStateOcclusion_t::kGcmQueryBase );
			gpGcmDrawState->SetZpassPixelCountEnable ( CELL_GCM_FALSE );
			m_queryIdx |= kQueryFinished;	// mark the query as finished
			break;

		case	D3DQUERYTYPE_EVENT:
			// End is very weird with respect to Events (fences).
			// DX9 docs say to use End to put the fence in the stream.  So we map End to GLM's Start.
			// http://msdn.microsoft.com/en-us/library/ee422167(VS.85).aspx
			m_queryIdx = s_GlobalStateFence.PrepareForQuery();
			gpGcmDrawState->SetWriteBackEndLabel ( m_queryIdx + QueryGlobalStateFence_t::kGcmLabelBase, 0 );	// drop "set fence" into stream
			m_queryIdx |= kQueryFinished;
			break;
		}
	}
	return S_OK;
}

HRESULT IDirect3DQuery9::GetData(void* pData,DWORD dwSize,DWORD dwGetDataFlags)
{
 	HRESULT	result = -1;

	// GetData is not always called with the flush bit.

	// if an answer is not yet available - return S_FALSE.
	// if an answer is available - return S_OK and write the answer into *pData.
	bool flush = (dwGetDataFlags & D3DGETDATA_FLUSH) != 0;	// aka spin until done

	// hmmm both of these paths are the same, maybe we could fold them up
	if ( ( m_queryIdx == kQueryUninitialized ) || !( m_queryIdx & kQueryFinished ) )
	{
		Assert(!"Can't GetData before start-stop");
		if ( pData ) { *(int32*)(pData) = 0; }
		result = -1;
	}
	else
	{
		switch( m_type )
		{
		case	D3DQUERYTYPE_OCCLUSION: {
			// expectation - caller already did an issue begin (start) and an issue end (stop).
			// we can probe using IsDone.
			union RptData {
				CellGcmReportData data;
				vector int vFetch;
			};
			RptData volatile const *rpt = reinterpret_cast< RptData volatile const * >( s_GlobalStateOcclusion.m_Values[ m_queryIdx & kQueryValueMask ] );
			RptData rptValue;
			rptValue.vFetch = rpt->vFetch;
			if ( rptValue.data.zero && flush )
			{
//
// Disabled out the wait for (flush) of occlusion query
// c_pixel_vis, seems to flush queries every couple of seconds, and it seems pointless on PS3
// Flushing the GPU and waiting for the report to write it's value seems a bad situation on PS3
// We can literally stall the CPU for 10ms
// Need to test this on levels with many coronas etc.. But in that case we need a bigger query list
// and to look closer at the higher level code
//
				// Flush GPU right up to current point - Endframe call does this...
// 				gpGcmDrawState->EndFrame();
// 				gpGcmDrawState->CmdBufferFlush();
// 
//  				while ( ( ( rptValue.vFetch = rpt->vFetch ), rptValue.data.zero )
//  					&& ( ThreadSleep(1), 1 ) ) // yield CPU when spin-waiting
//  					continue;

				rptValue.data.zero = 0;
			}
			if ( !rptValue.data.zero )
			{
				if (pData)
				{
					*(int32*)pData = rptValue.data.value;
				}
				result = S_OK;
			}
			else
			{
				result = S_FALSE;
			}
										} break;

		case	D3DQUERYTYPE_EVENT: {
			// expectation - caller already did an issue end (for fence => start) but has not done anything that would call Stop.
			// that's ok because Stop is a no-op for fences.
			uint32 volatile const& lbl = *s_GlobalStateFence.m_Values[ m_queryIdx & kQueryValueMask ];
			uint32 lblValue = lbl;
			if ( lblValue && flush )
			{
				// Flush GPU right up to current point - Endframe call does this...
				gpGcmDrawState->EndFrame();
				gpGcmDrawState->CmdBufferFlush();

 				while ( ( (lblValue = lbl) != 0 )
 					&& ( ThreadSleep(1), 1 ) ) // yield CPU when spin-waiting
 					continue;
			}
			if ( !lblValue )
			{
				*(uint*)pData = 0;
				result = S_OK;
			}
			else
			{
				result = S_FALSE;
			}
									} break;
		}
	}

	return result;
}



//--------------------------------------------------------------------------------------------------
// MISC
//--------------------------------------------------------------------------------------------------

BOOL IDirect3DDevice9::ShowCursor(BOOL bShow)
{
	// FIXME NOP
	//Debugger();
	return TRUE;
}

HRESULT IDirect3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix)
{
	Debugger();
	return S_OK;
}

HRESULT IDirect3DDevice9::SetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value)
{
	Debugger();
	return S_OK;
}

HRESULT IDirect3DDevice9::ValidateDevice(DWORD* pNumPasses)
{
	Debugger();
	return S_OK;
}

HRESULT IDirect3DDevice9::SetMaterial(CONST D3DMATERIAL9* pMaterial)
{
	return S_OK;
}


HRESULT IDirect3DDevice9::LightEnable(DWORD Index,BOOL Enable)
{
	Debugger();
	return S_OK;
}


HRESULT IDirect3DDevice9::GetDeviceCaps(D3DCAPS9* pCaps)
{
	Debugger();
	return S_OK;
}


HRESULT IDirect3DDevice9::TestCooperativeLevel()
{
	// game calls this to see if device was lost.
	// last I checked the device was still attached to the computer.
	// so, return OK.

	return S_OK;
}


HRESULT IDirect3DDevice9::EvictManagedResources()
{
	return S_OK;
}

HRESULT IDirect3DDevice9::SetLight(DWORD Index,CONST D3DLIGHT9*)
{
	Debugger();
	return S_OK;
}

void IDirect3DDevice9::SetGammaRamp(UINT iSwapChain,DWORD Flags,CONST D3DGAMMARAMP* pRamp)
{
}

void D3DPERF_SetOptions( DWORD dwOptions )
{
}

HRESULT D3DXCompileShader(
						  LPCSTR                          pSrcData,
						  UINT                            SrcDataLen,
						  CONST D3DXMACRO*                pDefines,
						  LPD3DXINCLUDE                   pInclude,
						  LPCSTR                          pFunctionName,
						  LPCSTR                          pProfile,
						  DWORD                           Flags,
						  LPD3DXBUFFER*                   ppShader,
						  LPD3DXBUFFER*                   ppErrorMsgs,
						  LPD3DXCONSTANTTABLE*            ppConstantTable)
{
	return S_OK;
}

//--------------------------------------------------------------------------------------------------
// D3DX funcs
//--------------------------------------------------------------------------------------------------

void* ID3DXBuffer::GetBufferPointer()
{
	Debugger();
	return NULL;
}

DWORD ID3DXBuffer::GetBufferSize()
{
	Debugger();
	return 0;
}

// matrix stack...

HRESULT D3DXCreateMatrixStack( DWORD Flags, LPD3DXMATRIXSTACK* ppStack)
{
	
	*ppStack = new ID3DXMatrixStack;
	
	(*ppStack)->Create();
	
	return S_OK;
}

HRESULT	ID3DXMatrixStack::Create()
{
	m_stack.EnsureCapacity( 16 );	// 1KB ish
	m_stack.AddToTail();
	m_stackTop = 0;				// top of stack is at index 0 currently
	
	LoadIdentity();
	
	return S_OK;
}

D3DXMATRIX* ID3DXMatrixStack::GetTop()
{
	return (D3DXMATRIX*)&m_stack[ m_stackTop ];
}

void ID3DXMatrixStack::Push()
{
	D3DMATRIX temp = m_stack[ m_stackTop ];
	m_stack.AddToTail( temp );
	m_stackTop ++;
}

void ID3DXMatrixStack::Pop()
{
	int elem = m_stackTop--;
	m_stack.Remove( elem );
}

void ID3DXMatrixStack::LoadIdentity()
{
	D3DXMATRIX *mat = GetTop();

	D3DXMatrixIdentity( mat );
}

void ID3DXMatrixStack::LoadMatrix( const D3DXMATRIX *pMat )
{
	*(GetTop()) = *pMat;
}


void ID3DXMatrixStack::MultMatrix( const D3DXMATRIX *pMat )
{

	// http://msdn.microsoft.com/en-us/library/bb174057(VS.85).aspx
	//	This method right-multiplies the given matrix to the current matrix
	//	(transformation is about the current world origin).
	//		m_pstack[m_currentPos] = m_pstack[m_currentPos] * (*pMat);
	//	This method does not add an item to the stack, it replaces the current
	//  matrix with the product of the current matrix and the given matrix.


	Debugger();
}

void ID3DXMatrixStack::MultMatrixLocal( const D3DXMATRIX *pMat )
{
	//	http://msdn.microsoft.com/en-us/library/bb174058(VS.85).aspx
	//	This method left-multiplies the given matrix to the current matrix
	//	(transformation is about the local origin of the object).
	//		m_pstack[m_currentPos] = (*pMat) * m_pstack[m_currentPos];
	//	This method does not add an item to the stack, it replaces the current
	//	matrix with the product of the given matrix and the current matrix.


	Debugger();
}

HRESULT ID3DXMatrixStack::ScaleLocal(FLOAT x, FLOAT y, FLOAT z)
{
	//	http://msdn.microsoft.com/en-us/library/bb174066(VS.85).aspx
	//	Scale the current matrix about the object origin.
	//	This method left-multiplies the current matrix with the computed
	//	scale matrix. The transformation is about the local origin of the object.
	//
	//	D3DXMATRIX tmp;
	//	D3DXMatrixScaling(&tmp, x, y, z);
	//	m_stack[m_currentPos] = tmp * m_stack[m_currentPos];

	Debugger();

	return S_OK;
}


HRESULT ID3DXMatrixStack::RotateAxisLocal(CONST D3DXVECTOR3* pV, FLOAT Angle)
{
	//	http://msdn.microsoft.com/en-us/library/bb174062(VS.85).aspx
	//	Left multiply the current matrix with the computed rotation
	//	matrix, counterclockwise about the given axis with the given angle.
	//	(rotation is about the local origin of the object)

	//	D3DXMATRIX tmp;
	//	D3DXMatrixRotationAxis( &tmp, pV, angle );
	//	m_stack[m_currentPos] = tmp * m_stack[m_currentPos];
	//	Because the rotation is left-multiplied to the matrix stack, the rotation
	//	is relative to the object's local coordinate space.
	
	Debugger();

	return S_OK;
}

HRESULT ID3DXMatrixStack::TranslateLocal(FLOAT x, FLOAT y, FLOAT z)
{
	//	http://msdn.microsoft.com/en-us/library/bb174068(VS.85).aspx
	//	Left multiply the current matrix with the computed translation
	//	matrix. (transformation is about the local origin of the object)

	//	D3DXMATRIX tmp;
	//	D3DXMatrixTranslation( &tmp, x, y, z );
	//	m_stack[m_currentPos] = tmp * m_stack[m_currentPos];

	Debugger();

	return S_OK;
}


const char* D3DXGetPixelShaderProfile( IDirect3DDevice9 *pDevice )
{
	Debugger();
	return "";
}


D3DXMATRIX* D3DXMatrixMultiply( D3DXMATRIX *pOut, CONST D3DXMATRIX *pM1, CONST D3DXMATRIX *pM2 )
{
	D3DXMATRIX temp;
	
	for( int i=0; i<4; i++)
	{
		for( int j=0; j<4; j++)
		{
			temp.m[i][j]	=	(pM1->m[ i ][ 0 ] * pM2->m[ 0 ][ j ])
							+	(pM1->m[ i ][ 1 ] * pM2->m[ 1 ][ j ])
							+	(pM1->m[ i ][ 2 ] * pM2->m[ 2 ][ j ])
							+	(pM1->m[ i ][ 3 ] * pM2->m[ 3 ][ j ]);
		}
	}
	*pOut = temp;
	return pOut;
}

D3DXVECTOR3* D3DXVec3TransformCoord( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV, CONST D3DXMATRIX *pM )		// http://msdn.microsoft.com/en-us/library/ee417622(VS.85).aspx
{
	// this one is tricky because
	// "Transforms a 3D vector by a given matrix, projecting the result back into w = 1".
	// but the vector has no W attached to it coming in, so we have to go through the motions of figuring out what w' would be
	// assuming the input vector had a W of 1.
	
	// dot product of [a b c 1] against w column
	float wp = (pM->m[3][0] * pV->x) + (pM->m[3][1] * pV->y) + (pM->m[3][2] * pV->z) + (pM->m[3][3]);
	
	if (wp == 0.0f )
	{
		// do something to avoid dividing by zero..
		Debugger();
	}
	else
	{
		// unclear on whether I should include the fake W in the sum (last term) before dividing by wp... hmmmm
		// leave it out for now and see how well it works
		pOut->x = ((pM->m[0][0] * pV->x) + (pM->m[0][1] * pV->y) + (pM->m[0][2] * pV->z) /* + (pM->m[0][3]) */ ) / wp;
		pOut->y = ((pM->m[1][0] * pV->x) + (pM->m[1][1] * pV->y) + (pM->m[1][2] * pV->z) /* + (pM->m[1][3]) */ ) / wp;
		pOut->z = ((pM->m[2][0] * pV->x) + (pM->m[2][1] * pV->y) + (pM->m[2][2] * pV->z) /* + (pM->m[2][3]) */ ) / wp;
	}

	return pOut;
}


void D3DXMatrixIdentity( D3DXMATRIX *mat )
{
	for( int i=0; i<4; i++)
	{
		for( int j=0; j<4; j++)
		{
			mat->m[i][j] = (i==j) ? 1.0f : 0.0f;	// 1's on the diagonal.
		}
	}
}

D3DXMATRIX* D3DXMatrixTranslation( D3DXMATRIX *pOut, FLOAT x, FLOAT y, FLOAT z )
{
	D3DXMatrixIdentity( pOut );
	pOut->m[3][0] = x;
	pOut->m[3][1] = y;
	pOut->m[3][2] = z;
	return pOut;
}

D3DXMATRIX* D3DXMatrixInverse( D3DXMATRIX *pOut, FLOAT *pDeterminant, CONST D3DXMATRIX *pM )
{
	Assert( sizeof( D3DXMATRIX ) == (16 * sizeof(float) ) );
	Assert( sizeof( VMatrix ) == (16 * sizeof(float) ) );
	Assert( pDeterminant == NULL );	// homey don't play that
	
	VMatrix *origM = (VMatrix*)pM;
	VMatrix *destM = (VMatrix*)pOut;
	
	bool success = MatrixInverseGeneral( *origM, *destM );
	Assert( success );
	
	return pOut;
}


D3DXMATRIX* D3DXMatrixTranspose( D3DXMATRIX *pOut, CONST D3DXMATRIX *pM )
{
	if (pOut != pM)
	{
		for( int i=0; i<4; i++)
		{
			for( int j=0; j<4; j++)
			{
				pOut->m[i][j] = pM->m[j][i];
			}
		}
	}
	else
	{
		D3DXMATRIX temp = *pM;
		D3DXMatrixTranspose( pOut, &temp );
	}

	return NULL;
}


D3DXPLANE* D3DXPlaneNormalize( D3DXPLANE *pOut, CONST D3DXPLANE *pP)
{
	// not very different from normalizing a vector.
	// figure out the square root of the sum-of-squares of the x,y,z components
	// make sure that's non zero
	// then divide all four components by that value
	// or return some dummy plane like 0,0,1,0 if it fails
	
	float	len = sqrt( (pP->a * pP->a) + (pP->b * pP->b) + (pP->c * pP->c) );
	if (len > 1e-10)	//FIXME need a real epsilon here ?
	{
		pOut->a = pP->a / len;		pOut->b = pP->b / len;		pOut->c = pP->c / len;		pOut->d = pP->d / len;
	}
	else
	{
		pOut->a = 0.0f;				pOut->b = 0.0f;				pOut->c = 1.0f;				pOut->d = 0.0f;
	}
	return pOut;
}


D3DXVECTOR4* D3DXVec4Transform( D3DXVECTOR4 *pOut, CONST D3DXVECTOR4 *pV, CONST D3DXMATRIX *pM )
{
	VMatrix *mat = (VMatrix*)pM;
	Vector4D *vIn = (Vector4D*)pV;
	Vector4D *vOut = (Vector4D*)pOut;

	Vector4DMultiply( *mat, *vIn, *vOut );

	return pOut;
}



D3DXVECTOR4* D3DXVec4Normalize( D3DXVECTOR4 *pOut, CONST D3DXVECTOR4 *pV )
{
	Vector4D *vIn = (Vector4D*) pV;
	Vector4D *vOut = (Vector4D*) pOut;

	*vOut = *vIn;
	Vector4DNormalize( *vOut );
	
	return pOut;
}

D3DXMATRIX* D3DXMatrixOrthoOffCenterRH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn,FLOAT zf )
{
	Debugger();
	return NULL;
}


D3DXMATRIX* D3DXMatrixPerspectiveRH( D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf )
{
	Debugger();
	return NULL;
}


D3DXMATRIX* D3DXMatrixPerspectiveOffCenterRH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf )
{
	Debugger();
	return NULL;
}


D3DXPLANE* D3DXPlaneTransform( D3DXPLANE *pOut, CONST D3DXPLANE *pP, CONST D3DXMATRIX *pM )
{
	Debugger();
	return NULL;
}


//--------------------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------------------

D3DXMATRIX D3DXMATRIX::operator*( const D3DXMATRIX &o ) const
{
	D3DXMATRIX result;

	D3DXMatrixMultiply( &result, this, &o );	// this = lhs    o = rhs    result = this * o

	return result;
}

D3DXMATRIX::operator FLOAT* ()
{
	return (float*)this;
}

float& D3DXMATRIX::operator()( int row, int column )
{
	return m[row][column];
}

const float& D3DXMATRIX::operator()( int row, int column ) const
{
	return m[row][column];
}

// ------------------------------------------------------------------------------------------------------------------------------ //


float& D3DXPLANE::operator[]( int i )
{
	return ((float*)this)[i];
}

bool D3DXPLANE::operator==( const D3DXPLANE &o )
{
	return a == o.a && b == o.b && c == o.c && d == o.d;
}

bool D3DXPLANE::operator!=( const D3DXPLANE &o )
{
	return !( *this == o );
}

D3DXPLANE::operator float*()
{
	return (float*)this;
}

D3DXPLANE::operator const float*() const
{
	return (const float*)this;
}

// ------------------------------------------------------------------------------------------------------------------------------ //


D3DXVECTOR2::operator FLOAT* ()
{
	return (float*)this;
}

D3DXVECTOR2::operator CONST FLOAT* () const
{
	return (const float*)this;
}

// ------------------------------------------------------------------------------------------------------------------------------ //


D3DXVECTOR3::D3DXVECTOR3( float a, float b, float c )
{
	x = a;
	y = b;
	z = c;
}

D3DXVECTOR3::operator FLOAT* ()
{
	return (float*)this;
}

D3DXVECTOR3::operator CONST FLOAT* () const
{
	return (const float*)this;
}

// ------------------------------------------------------------------------------------------------------------------------------ //



D3DXVECTOR4::D3DXVECTOR4( float a, float b, float c, float d )
{
	x = a;
	y = b;
	z = c;
	w = d;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

DWORD IDirect3DResource9::SetPriority(DWORD PriorityNew)
{
	//	Debugger();
	return 0;
}

//--------------------------------------------------------------------------------------------------
// Screen shot for VX console
//--------------------------------------------------------------------------------------------------

// returns a pointer to the screen shot frame buffer and some associated header info.
// returns NULL on failure (which it can't right now)

char *GetScreenShotInfoForVX( IDirect3DDevice9 *pDevice, uint32 *uWidth, uint32 *uHeight, uint32 *uPitch, uint32 *colour )
{
	const CPs3gcmTextureLayout & layout = *pDevice->m_defaultColorSurface->m_tex->m_layout;
	*uWidth = layout.m_key.m_size[0];
	*uHeight = layout.m_key.m_size[1];
	*uPitch = g_ps3gcmGlobalState.m_nSurfaceRenderPitch;  //  layout.DefaultPitch();
	switch ( layout.GetFormatPtr()->m_gcmFormat )
	{
	case CELL_GCM_TEXTURE_A8R8G8B8 :
	case CELL_GCM_TEXTURE_D8R8G8B8:
	case CELL_GCM_SURFACE_A8R8G8B8:
		*colour =  IMaterialSystem::kX8R8G8B8;
		break;
	case CELL_GCM_SURFACE_A8B8G8R8:
		*colour =  IMaterialSystem::kX8B8G8R8;
		break;
	case CELL_GCM_TEXTURE_W16_Z16_Y16_X16_FLOAT:
		*colour =  IMaterialSystem::kR16G16B16X16;
		break;
	default:
		*colour = (IMaterialSystem::VRAMScreenShotInfoColorFormat_t) 0;
	}
	// send oldest buffer
	return g_ps3gcmGlobalState.m_display.surfaceColor[ (g_ps3gcmGlobalState.m_display.surfaceFlipIdx + 1) & 1 ].DataInAnyMemory();
}

//--------------------------------------------------------------------------------------------------
// Windows Stubs
//--------------------------------------------------------------------------------------------------

void GlobalMemoryStatus( MEMORYSTATUS *pOut )
{
	pOut->dwTotalPhys = (1<<31);
}

void Sleep( unsigned int ms )
{
	Debugger();
	ThreadSleep( ms );
}

bool IsIconic( VD3DHWND hWnd )
{
	return false;
}

void GetClientRect( VD3DHWND hWnd, RECT *destRect )
{
	destRect->left = 0;
	destRect->top = 0;
	destRect->right = g_ps3gcmGlobalState.m_nRenderSize[0];
	destRect->bottom = g_ps3gcmGlobalState.m_nRenderSize[1];
}

BOOL ClientToScreen( VD3DHWND hWnd, LPPOINT pPoint )
{
	Debugger();
	return true;
}

void* GetCurrentThread()
{
	Debugger();
	return 0;
}

void SetThreadAffinityMask( void *hThread, int nMask )
{
	Debugger();
}

bool operator==( const struct _GUID &lhs, const struct _GUID &rhs )
{
	Debugger();
	return memcmp( &lhs, &rhs, sizeof( GUID ) ) == 0;
}

