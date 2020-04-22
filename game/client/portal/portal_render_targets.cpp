//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Portal mod render targets are specified by and accessable through this singleton
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "portal_render_targets.h"
#include "materialsystem/imaterialsystem.h"
#include "rendertexture.h"

//-----------------------------------------------------------------------------
// Purpose: Called in CClientRenderTargets::InitClientRenderTargets, used to set
//			the CTextureReference member
// Input  : pMaterialSystem - material system passed in from the engine
// Output : ITexture* - the created texture
//-----------------------------------------------------------------------------
ITexture* CPortalRenderTargets::InitPortal1Texture( IMaterialSystem* pMaterialSystem )
{
	if ( IsGameConsole() )
	{
		// shouldn't be using
		Assert( 0 );
		return NULL;
	}

	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_Portal1",
		1, 1, RT_SIZE_FULL_FRAME_BUFFER,
		pMaterialSystem->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED, 
		0,
		CREATERENDERTARGETFLAGS_HDR );
}

ITexture* CPortalRenderTargets::GetPortal1Texture()
{
	return m_Portal1Texture;
}


//-----------------------------------------------------------------------------
// Purpose: Called in CClientRenderTargets::InitClientRenderTargets, used to set
//			the CTextureReference member
// Input  : pMaterialSystem - material system passed in from the engine
// Output : ITexture* - the created texture
//-----------------------------------------------------------------------------
ITexture* CPortalRenderTargets::InitPortal2Texture( IMaterialSystem* pMaterialSystem )
{
	if ( IsGameConsole() )
	{
		// shouldn't be using
		Assert( 0 );
		return NULL;
	}

	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_Portal2",
		1, 1, RT_SIZE_FULL_FRAME_BUFFER,
		pMaterialSystem->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED, 
		0,
		CREATERENDERTARGETFLAGS_HDR );
}

ITexture* CPortalRenderTargets::GetPortal2Texture()
{
	return m_Portal2Texture;
} 



//-----------------------------------------------------------------------------
// Purpose: Called in CClientRenderTargets::InitClientRenderTargets, used to set
//			the CTextureReference member
// Input  : pMaterialSystem - material system passed in from the engine
// Output : ITexture* - the created texture
//-----------------------------------------------------------------------------
ITexture* CPortalRenderTargets::InitDepthDoublerTexture( IMaterialSystem* pMaterialSystem )
{
	return pMaterialSystem->CreateNamedRenderTargetTextureEx2(
		"_rt_DepthDoubler",
		1, 1, RT_SIZE_FULL_FRAME_BUFFER,
		pMaterialSystem->GetBackBufferFormat(),
		MATERIAL_RT_DEPTH_SHARED, 
		0,
		CREATERENDERTARGETFLAGS_HDR | (IsX360() ? CREATERENDERTARGETFLAGS_NOEDRAM : 0) );
}

ITexture* CPortalRenderTargets::GetDepthDoublerTexture()
{
	return m_DepthDoublerTexture;
}


void CPortalRenderTargets::InitPortalWaterTextures( IMaterialSystem* pMaterialSystem )
{
// This code does not work on ATI or newer nVidia chips (probably an issue with mismatched depth buffer configuration?) -- disabling it for now
// since we probably don't need it to ship either (first 2 levels use full res buffer, 3rd level uses cheap water, low end PC will just 
// use cheap water everywhere)
#if 0
	if( !IsGameConsole() )
	{
		//Reflections
		GetWaterReflectionTexture()->AddDownsizedSubTarget( GetSubTargetNameForPortalRecursionLevel( 1 ), 2, MATERIAL_RT_DEPTH_SEPARATE );
		GetWaterReflectionTexture()->AddDownsizedSubTarget( GetSubTargetNameForPortalRecursionLevel( 2 ), 4, MATERIAL_RT_DEPTH_SEPARATE );

		//Refractions
		GetWaterRefractionTexture()->AddDownsizedSubTarget( GetSubTargetNameForPortalRecursionLevel( 1 ), 2, MATERIAL_RT_DEPTH_SEPARATE );
		GetWaterRefractionTexture()->AddDownsizedSubTarget( GetSubTargetNameForPortalRecursionLevel( 2 ), 4, MATERIAL_RT_DEPTH_SEPARATE );
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: InitClientRenderTargets, interface called by the engine at material system init in the engine
// Input  : pMaterialSystem - the interface to the material system from the engine (our singleton hasn't been set up yet)
//			pHardwareConfig - the user's hardware config, useful for conditional render targets setup
//-----------------------------------------------------------------------------
void CPortalRenderTargets::InitClientRenderTargets( IMaterialSystem* pMaterialSystem, IMaterialSystemHardwareConfig* pHardwareConfig )
{
	static ConVarRef gpu_level( "gpu_level" );
	int nWaterRenderTargetResolution = 512;
	
	// If we're at a decent GPU level, check back buffer dimensions and increase water texture resolution accordingly
 	if ( !IsGameConsole() && ( gpu_level.GetInt() > 1 ) )
	{
		int nWidth, nHeight;
		pMaterialSystem->GetBackBufferDimensions( nWidth, nHeight );

		if ( nHeight >= 1024 )
		{
			nWaterRenderTargetResolution = 1024;
		}
	}

	// Water effects & camera from the base class (standard HL2 targets)
	BaseClass::SetupClientRenderTargets( pMaterialSystem, pHardwareConfig, nWaterRenderTargetResolution, 256 );

	// If they don't support stencils, allocate render targets for drawing portals.
	// TODO: When stencils are default, do the below check before bothering to allocate the RTs
	//		and make sure that switching from Stencil<->RT mode reinits the material system.
//	if ( materials->StencilBufferBits() == 0 )
	if ( IsPC() || !IsGameConsole() )
	{
		m_Portal1Texture.Init( InitPortal1Texture( pMaterialSystem ) );
		m_Portal2Texture.Init( InitPortal2Texture( pMaterialSystem ) );
	}

	m_DepthDoublerTexture.Init( InitDepthDoublerTexture( pMaterialSystem ) );

	//if ( IsPC() || !IsGameConsole() )
	{
		InitPortalWaterTextures( pMaterialSystem );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Shutdown client render targets. This gets called during shutdown in the engine
// Input  :  - 
//-----------------------------------------------------------------------------
void CPortalRenderTargets::ShutdownClientRenderTargets()
{
	m_Portal1Texture.Shutdown();
	m_Portal2Texture.Shutdown();
	m_DepthDoublerTexture.Shutdown();


	// Clean up standard HL2 RTs (camera and water)
	BaseClass::ShutdownClientRenderTargets();
}

const char *GetSubTargetNameForPortalRecursionLevel( int iRecursionLevel )
{
	switch( iRecursionLevel )
	{
	default:
	case 0:
		return NULL;
	case 1:
		return "Depth_1";
	case 2:
		return "Depth_2";
	}
}

static CPortalRenderTargets g_PortalRenderTargets;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CPortalRenderTargets, IClientRenderTargets, CLIENTRENDERTARGETS_INTERFACE_VERSION, g_PortalRenderTargets );
CPortalRenderTargets* portalrendertargets = &g_PortalRenderTargets;
