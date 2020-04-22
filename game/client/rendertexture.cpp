//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
// Implements local hooks into named renderable textures.
// See matrendertexture.cpp in material system for list of available RT's
//
//=============================================================================//

#include "materialsystem/imesh.h"
#include "materialsystem/itexture.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "tier1/strtools.h"
#include "rendertexture.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void ReleaseRenderTargets( int nChangeFlags );

void AddReleaseFunc( void )
{
	static bool bAdded = false;
	if( !bAdded )
	{
		bAdded = true;
		materials->AddReleaseFunc( ReleaseRenderTargets );
	}
}

//=============================================================================
// Stereo Param Texture
//=============================================================================
static CTextureReference s_pStereoParamTexture;
ITexture *GetStereoParamTexture( void )
{
	if ( !s_pStereoParamTexture )
	{
		s_pStereoParamTexture.Init( materials->FindTexture( "stereoparam", TEXTURE_GROUP_OTHER ) );
		Assert( !IsErrorTexture( s_pStereoParamTexture ) );
		AddReleaseFunc();
	}

	return s_pStereoParamTexture;
}

//=============================================================================
// Power of Two Frame Buffer Texture
//=============================================================================
static CTextureReference s_pPowerOfTwoFrameBufferTexture;
ITexture *GetPowerOfTwoFrameBufferTexture( void )
{
	if ( IsGameConsole() )
	{
		return GetFullFrameFrameBufferTexture( 1 );
	}

	if ( !s_pPowerOfTwoFrameBufferTexture )
	{
		s_pPowerOfTwoFrameBufferTexture.Init( materials->FindTexture( "_rt_PowerOfTwoFB", TEXTURE_GROUP_RENDER_TARGET ) );
		Assert( !IsErrorTexture( s_pPowerOfTwoFrameBufferTexture ) );
		AddReleaseFunc();
	}
	
	return s_pPowerOfTwoFrameBufferTexture;
}

//=============================================================================
// Fullscreen Texture
//=============================================================================
static CTextureReference s_pFullscreenTexture;
ITexture *GetFullscreenTexture( void )
{
	if ( !s_pFullscreenTexture )
	{
		s_pFullscreenTexture.Init( materials->FindTexture( "_rt_Fullscreen", TEXTURE_GROUP_RENDER_TARGET ) );
		Assert( !IsErrorTexture( s_pFullscreenTexture ) );
		AddReleaseFunc();
	}

	return s_pFullscreenTexture;
}

//=============================================================================
// Camera Texture
//=============================================================================
static CTextureReference s_pCameraTexture;
ITexture *GetCameraTexture( void )
{
	if ( !s_pCameraTexture )
	{
		s_pCameraTexture.Init( materials->FindTexture( "_rt_Camera", TEXTURE_GROUP_RENDER_TARGET ) );
		Assert( !IsErrorTexture( s_pCameraTexture ) );
		AddReleaseFunc();
	}
	
	return s_pCameraTexture;
}

//=============================================================================
// Full Frame Depth Texture
//=============================================================================
static CTextureReference s_pFullFrameDepthTexture;
ITexture *GetFullFrameDepthTexture( void )
{
	if ( !s_pFullFrameDepthTexture )
	{
		s_pFullFrameDepthTexture.Init( materials->FindTexture( "_rt_FullFrameDepth", TEXTURE_GROUP_RENDER_TARGET ) );
		Assert( !IsErrorTexture( s_pFullFrameDepthTexture ) );
		AddReleaseFunc();
	}

	return s_pFullFrameDepthTexture;
}

//=============================================================================
// Full Frame Buffer Textures
//=============================================================================
static CTextureReference s_pFullFrameFrameBufferTexture[MAX_FB_TEXTURES];
ITexture *GetFullFrameFrameBufferTexture( int textureIndex )
{
	if ( !s_pFullFrameFrameBufferTexture[textureIndex] )
	{
		char name[256];
		if( textureIndex != 0 )
		{
			Q_snprintf( name, ARRAYSIZE( name ), "_rt_FullFrameFB%d", textureIndex );
		}
		else
		{
			Q_strcpy( name, "_rt_FullFrameFB" );
		}
		s_pFullFrameFrameBufferTexture[textureIndex].Init( materials->FindTexture( name, TEXTURE_GROUP_RENDER_TARGET ) );
		Assert( !IsErrorTexture( s_pFullFrameFrameBufferTexture[textureIndex] ) );
		AddReleaseFunc();
	}
	
	return s_pFullFrameFrameBufferTexture[textureIndex];
}


//=============================================================================
// Water reflection
//=============================================================================
static CTextureReference s_pWaterReflectionTexture;
ITexture *GetWaterReflectionTexture( void )
{
	if ( !s_pWaterReflectionTexture )
	{
		s_pWaterReflectionTexture.Init( materials->FindTexture( "_rt_WaterReflection", TEXTURE_GROUP_RENDER_TARGET ) );
		Assert( !IsErrorTexture( s_pWaterReflectionTexture ) );
		AddReleaseFunc();
	}
	
	return s_pWaterReflectionTexture;
}

//=============================================================================
// Water refraction
//=============================================================================
static CTextureReference s_pWaterRefractionTexture;
ITexture *GetWaterRefractionTexture( void )
{
	if ( !s_pWaterRefractionTexture )
	{
		s_pWaterRefractionTexture.Init( materials->FindTexture( "_rt_WaterRefraction", TEXTURE_GROUP_RENDER_TARGET ) );
		Assert( !IsErrorTexture( s_pWaterRefractionTexture ) );
		AddReleaseFunc();
	}
	
	return s_pWaterRefractionTexture;
}

//=============================================================================
// Custom Weapon RT
//=============================================================================
static CTextureReference s_pCustomWeaponTexture;
ITexture *GetCustomWeaponTexture( void )
{ 
	if ( !s_pCustomWeaponTexture )
	{
		s_pCustomWeaponTexture.Init( materials->FindTexture( "_rt_CustomWeapon", TEXTURE_GROUP_RENDER_TARGET ) );
		Assert( !IsErrorTexture( s_pCustomWeaponTexture ) );
		AddReleaseFunc();
	}

	return s_pCustomWeaponTexture;
}

//=============================================================================
// Custom Weapon RT for exponent
//=============================================================================
static CTextureReference s_pCustomWeaponTextureExp;
ITexture *GetCustomWeaponTextureExp( void )
{ 
	if ( !s_pCustomWeaponTextureExp )
	{
		s_pCustomWeaponTextureExp.Init( materials->FindTexture( "_rt_CustomWeaponExp", TEXTURE_GROUP_RENDER_TARGET ) );
		Assert( !IsErrorTexture( s_pCustomWeaponTextureExp ) );
		AddReleaseFunc();
	}

	return s_pCustomWeaponTextureExp;
}

//=============================================================================
// Small Buffer HDR0
//=============================================================================
static CTextureReference s_pSmallBufferHDR0;
ITexture *GetSmallBufferHDR0( void )
{
	if ( !s_pSmallBufferHDR0 )
	{
		s_pSmallBufferHDR0.Init( materials->FindTexture( "_rt_SmallHDR0", TEXTURE_GROUP_RENDER_TARGET ) );
		Assert( !IsErrorTexture( s_pSmallBufferHDR0 ) );
		AddReleaseFunc();
	}
	
	return s_pSmallBufferHDR0;
}

//=============================================================================
// Small Buffer HDR1
//=============================================================================
static CTextureReference s_pSmallBufferHDR1;
ITexture *GetSmallBufferHDR1( void )
{
	if ( !s_pSmallBufferHDR1 )
	{
		s_pSmallBufferHDR1.Init( materials->FindTexture( "_rt_SmallHDR1", TEXTURE_GROUP_RENDER_TARGET ) );
		Assert( !IsErrorTexture( s_pSmallBufferHDR1 ) );
		AddReleaseFunc();
	}
	
	return s_pSmallBufferHDR1;
}

//=============================================================================
// Quarter Sized FB0
//=============================================================================
static CTextureReference s_pQuarterSizedFB0;
ITexture *GetSmallBuffer0( void )
{
	if ( !s_pQuarterSizedFB0 )
	{
		s_pQuarterSizedFB0.Init( materials->FindTexture( "_rt_SmallFB0", TEXTURE_GROUP_RENDER_TARGET ) );
		Assert( !IsErrorTexture( s_pQuarterSizedFB0 ) );
		AddReleaseFunc();
	}
	
	return s_pQuarterSizedFB0;
}

//=============================================================================
// Quarter Sized FB1
//=============================================================================
static CTextureReference s_pQuarterSizedFB1;
ITexture *GetSmallBuffer1( void )
{
	if ( !s_pQuarterSizedFB1 )
	{
		s_pQuarterSizedFB1.Init( materials->FindTexture( "_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET ) );
		Assert( !IsErrorTexture( s_pQuarterSizedFB1 ) );
		AddReleaseFunc();
	}
	
	return s_pQuarterSizedFB1;
}

//=============================================================================
// Teeny Textures
//=============================================================================
static CTextureReference s_TeenyTextures[MAX_TEENY_TEXTURES];
ITexture *GetTeenyTexture( int which )
{
	if ( IsGameConsole() )
	{
		Assert( 0 );
		return NULL;
	}

	Assert( which < MAX_TEENY_TEXTURES );

	if ( !s_TeenyTextures[which] )
	{
		char nbuf[20];
		Q_snprintf( nbuf, ARRAYSIZE( nbuf ), "_rt_TeenyFB%d", which );
		s_TeenyTextures[which].Init( materials->FindTexture( nbuf, TEXTURE_GROUP_RENDER_TARGET ) );
		Assert( !IsErrorTexture( s_TeenyTextures[which] ) );
		AddReleaseFunc();
	}
	return s_TeenyTextures[which];
}

void ReleaseRenderTargets( int nChangeFlags )
{
	if ( nChangeFlags & MATERIAL_RESTORE_VERTEX_FORMAT_CHANGED )
		return;

	s_pPowerOfTwoFrameBufferTexture.Shutdown();
	s_pCameraTexture.Shutdown();
	s_pWaterReflectionTexture.Shutdown();
	s_pWaterRefractionTexture.Shutdown();
	s_pQuarterSizedFB0.Shutdown();
	s_pQuarterSizedFB1.Shutdown();
	s_pFullFrameDepthTexture.Shutdown();
	s_pCustomWeaponTexture.Shutdown();

	for (int i=0; i<MAX_FB_TEXTURES; ++i)
	{
		s_pFullFrameFrameBufferTexture[i].Shutdown();
	}
}
