//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMEUIDYNAMICTEXTURES_H
#define GAMEUIDYNAMICTEXTURES_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlobjectreference.h"
#include "tier1/utldict.h"
#include "tier1/utlstring.h"
#include "tier1/utlstringmap.h"
#include "materialsystem/materialsystemutil.h"
#include "materialsystem/imaterialproxy.h"
#include "bitmap/texturepacker.h"


//----------------------------------------------------------------------------
// Helper class for entries in the packer
//----------------------------------------------------------------------------
class ImageAliasData_t
{
public:
	ImageAliasData_t()
	{
		Init();
	}
	void Init()
	{
		m_XPos = 0;
		m_YPos = 0;
		m_Width = 0;
		m_Height = 0;
		m_szBaseTextureName = "";
		m_Material = NULL;
		m_bIsInSheet = false;
		m_nNodeIndex = -1;
		m_nRefCount = 0;
	}
	int m_XPos;
	int m_YPos;
	int m_Width;
	int m_Height;
	CUtlString m_szBaseTextureName;
	CMaterialReference m_Material;
	bool m_bIsInSheet;
	int m_nNodeIndex;
	int m_nRefCount;
private:
	ImageAliasData_t( ImageAliasData_t &ref ) { }
};

//-----------------------------------------------------------------------------
// A template describing how a gameUI will function
// It loads CVguiCompiledDoc 
//-----------------------------------------------------------------------------
class CGameUIDynamicTextures
{
	
public:

	CGameUIDynamicTextures();
	~CGameUIDynamicTextures();

	void InitRenderTargets();
	IMaterialProxy *CreateProxy( const char *proxyName );

	void Shutdown();

	void SetImageEntry( const char *pEntryName, ImageAliasData_t &imageData );

	void LoadImageAlias( const char *pAlias, const char *pBaseTextureName );
	void ReleaseImageAlias( const char *pAlias );

	IMaterial *GetImageAliasMaterial( const char *pAlias );
	
	ImageAliasData_t *GetImageAliasData( const char *pAlias );
	bool IsErrorImageAliasData( ImageAliasData_t *pData );

	void GetDynamicSheetSize( int &nWidth, int &nHeight );

	void DrawDynamicTexture( const char *pAlias, int x, int y );

	void OnRestore( int nChangeFlags );
	void RegenerateTexture( int nChangeFlags );

private:
	void LoadImageAliasTexture( const char *pAlias, const char *pBaseTextureName );

	// Stores the actual texture we're writing into
	CTextureReference m_TexturePage;
	CTexturePacker *m_pDynamicTexturePacker;

	CUtlStringMap< ImageAliasData_t > m_ImageAliasMap;
	CMaterialReference m_RenderMaterial; // used to render dynamic textures into the render target.	

	bool m_bRegenerate;
};



#endif // GAMEUIDYNAMICTEXTURES_H
