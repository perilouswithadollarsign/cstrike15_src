//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include <stdlib.h>
#ifndef _PS3
#include <malloc.h>
#endif
#include "materialsystem_global.h"
#include "string.h"
#include "shaderapi/ishaderapi.h"
#include "materialsystem/materialsystem_config.h"
#include "IHardwareConfigInternal.h"
#include "texturemanager.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/IColorCorrection.h"
#include "tier1/strtools.h"
#include "utlvector.h"
#include "utldict.h"
#include "itextureinternal.h"
#include "vtf/vtf.h"
#include "pixelwriter.h"
#include "basetypes.h"
#include "utlbuffer.h"
#include "filesystem.h"
#include "materialsystem/imesh.h"
#include "materialsystem/ishaderapi.h"
#include "vstdlib/random.h"
#include "imorphinternal.h"
#include "isubdinternal.h"
#include "tier1/utlrbtree.h"
#include "ctype.h"
#include "tier0/icommandline.h"
#include "filesystem/IQueuedLoader.h"

// Need lightmaps access here
#ifndef _PS3
#define MATSYS_INTERNAL
#endif
#include "cmatlightmaps.h"
#include "cmaterialsystem.h"
#ifndef _PS3
#undef MATSYS_INTERNAL
#endif

#include "tier0/memdbgon.h"

#define ERROR_TEXTURE_SIZE				32
#define WHITE_TEXTURE_SIZE				1
#define BLACK_TEXTURE_SIZE				1
#define GREY_TEXTURE_SIZE				1
#define NORMALIZATION_CUBEMAP_SIZE		32
#define SSAO_NOISE_TEXTURE_SIZE			32
#define ERROR_TEXTURE_IS_SOLID

//-----------------------------------------------------------------------------
//
// Various procedural texture regeneration classes
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Creates a checkerboard texture
//-----------------------------------------------------------------------------
class CCheckerboardTexture : public ITextureRegenerator
{
public:
	CCheckerboardTexture( int nCheckerSize, color32 color1, color32 color2 ) :
		m_nCheckerSize( nCheckerSize ), m_Color1(color1), m_Color2(color2)
	{
	}

	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
	{
		for (int iFrame = 0; iFrame < pVTFTexture->FrameCount(); ++iFrame )
		{
			for (int iFace = 0; iFace < pVTFTexture->FaceCount(); ++iFace )
			{
				int nWidth = pVTFTexture->Width();
				int nHeight = pVTFTexture->Height();
				int nDepth = pVTFTexture->Depth();
				for (int z = 0; z < nDepth; ++z)
				{
					// Fill mip 0 with a checkerboard
					CPixelWriter pixelWriter;
					pixelWriter.SetPixelMemory( pVTFTexture->Format(), 
						pVTFTexture->ImageData( iFrame, iFace, 0, 0, 0, z ), pVTFTexture->RowSizeInBytes( 0 ) );
					
					for (int y = 0; y < nHeight; ++y)
					{
						pixelWriter.Seek( 0, y );
						for (int x = 0; x < nWidth; ++x)
						{
							if ( ((x & m_nCheckerSize) ^ (y & m_nCheckerSize)) ^ (z & m_nCheckerSize) )
							{
								pixelWriter.WritePixel( m_Color1.r, m_Color1.g, m_Color1.b, m_Color1.a );
							}
							else
							{
								pixelWriter.WritePixel( m_Color2.r, m_Color2.g, m_Color2.b, m_Color2.a );
							}
						}
					}
				}
			}
		}
	}

	virtual void Release()
	{
		delete this;
	}

private:
	int		m_nCheckerSize;
	color32 m_Color1;
	color32 m_Color2;
};

static void CreateCheckerboardTexture( ITextureInternal *pTexture, int nCheckerSize, color32 color1, color32 color2 )
{
	ITextureRegenerator *pRegen = new CCheckerboardTexture( nCheckerSize, color1, color2 );
	pTexture->SetTextureRegenerator( pRegen );
}

//-----------------------------------------------------------------------------
// Creates a solid texture
//-----------------------------------------------------------------------------
class CSolidTexture : public ITextureRegenerator
{
public:
	CSolidTexture( color32 color ) : m_Color(color)
	{
	}

	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
	{
		int nMipCount = pTexture->IsMipmapped() ? pVTFTexture->MipCount() : 1;
		for (int iFrame = 0; iFrame < pVTFTexture->FrameCount(); ++iFrame )
		{
			for (int iFace = 0; iFace < pVTFTexture->FaceCount(); ++iFace )
			{
				for (int iMip = 0; iMip < nMipCount; ++iMip )
				{
					int nWidth, nHeight, nDepth;
					pVTFTexture->ComputeMipLevelDimensions( iMip, &nWidth, &nHeight, &nDepth );
					for (int z = 0; z < nDepth; ++z)
					{
						CPixelWriter pixelWriter;
						pixelWriter.SetPixelMemory( pVTFTexture->Format(), 
							pVTFTexture->ImageData( iFrame, iFace, iMip, 0, 0, z ), pVTFTexture->RowSizeInBytes( iMip ) );
					
						for (int y = 0; y < nHeight; ++y)
						{
							pixelWriter.Seek( 0, y );
							for (int x = 0; x < nWidth; ++x)
							{
								pixelWriter.WritePixel( m_Color.r, m_Color.g, m_Color.b, m_Color.a );
							}
						}
					}
				}
			}
		}
	}

	virtual void Release()
	{
		delete this;
	}

private:
	color32 m_Color;
};

static void CreateSolidTexture( ITextureInternal *pTexture, color32 color )
{
	ITextureRegenerator *pRegen = new CSolidTexture( color );
	pTexture->SetTextureRegenerator( pRegen );
}

//-----------------------------------------------------------------------------
// Creates a normalization cubemap texture
//-----------------------------------------------------------------------------
class CNormalizationCubemap : public ITextureRegenerator
{
public:
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
	{
		// Normalization cubemap doesn't make sense on low-end hardware
		// So we won't construct a spheremap out of this
		CPixelWriter pixelWriter;

		Vector direction;
		for (int iFace = 0; iFace < 6; ++iFace)
		{
			pixelWriter.SetPixelMemory( pVTFTexture->Format(), 
				pVTFTexture->ImageData( 0, iFace, 0 ), pVTFTexture->RowSizeInBytes( 0 ) );
			
			int nWidth = pVTFTexture->Width();
			int nHeight = pVTFTexture->Height();

			float flInvWidth = 2.0f / (float)(nWidth-1);
			float flInvHeight = 2.0f / (float)(nHeight-1);

			for (int y = 0; y < nHeight; ++y)
			{
				float v = y * flInvHeight - 1.0f;

				pixelWriter.Seek( 0, y );
				for (int x = 0; x < nWidth; ++x)
				{
					float u = x * flInvWidth - 1.0f;
					float oow = 1.0f / sqrt( 1.0f + u*u + v*v );

					int ix = (int)(255.0f * 0.5f * (u*oow + 1.0f) + 0.5f);
					ix = clamp( ix, 0, 255 );
					int iy = (int)(255.0f * 0.5f * (v*oow + 1.0f) + 0.5f);
					iy = clamp( iy, 0, 255 );
					int iz = (int)(255.0f * 0.5f * (oow + 1.0f) + 0.5f);
					iz = clamp( iz, 0, 255 );

					switch (iFace)
					{
					case CUBEMAP_FACE_RIGHT:
						pixelWriter.WritePixel( iz, 255 - iy, 255 - ix, 255 );
						break;
					case CUBEMAP_FACE_LEFT:
						pixelWriter.WritePixel( 255 - iz, 255 - iy, ix, 255 );
						break;
					case CUBEMAP_FACE_BACK:	
						pixelWriter.WritePixel( ix, iz, iy, 255 );
						break;
					case CUBEMAP_FACE_FRONT:
						pixelWriter.WritePixel( ix, 255 - iz, 255 - iy, 255 );
						break;
					case CUBEMAP_FACE_UP:
						pixelWriter.WritePixel( ix, 255 - iy, iz, 255 );
						break;
					case CUBEMAP_FACE_DOWN:
						pixelWriter.WritePixel( 255 - ix, 255 - iy, 255 - iz, 255 );
						break;
					default:
						break;
					}
				}
			}
		}
	}

	// NOTE: The normalization cubemap regenerator is stateless
	// so there's no need to allocate + deallocate them
	virtual void Release() {}
};


//-----------------------------------------------------------------------------
// Creates a normalization cubemap texture
//-----------------------------------------------------------------------------
class CSignedNormalizationCubemap : public ITextureRegenerator
{
public:
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pSubRect )
	{
		// Normalization cubemap doesn't make sense on low-end hardware
		// So we won't construct a spheremap out of this
		CPixelWriter pixelWriter;

		Vector direction;
		for (int iFace = 0; iFace < 6; ++iFace)
		{
			pixelWriter.SetPixelMemory( pVTFTexture->Format(), 
				pVTFTexture->ImageData( 0, iFace, 0 ), pVTFTexture->RowSizeInBytes( 0 ) );
			
			int nWidth = pVTFTexture->Width();
			int nHeight = pVTFTexture->Height();

			float flInvWidth = 2.0f / (float)(nWidth-1);
			float flInvHeight = 2.0f / (float)(nHeight-1);

			for (int y = 0; y < nHeight; ++y)
			{
				float v = y * flInvHeight - 1.0f;

				pixelWriter.Seek( 0, y );
				for (int x = 0; x < nWidth; ++x)
				{
					float u = x * flInvWidth - 1.0f;
					float oow = 1.0f / sqrt( 1.0f + u*u + v*v );

#if defined( DX_TO_GL_ABSTRACTION ) && !defined( _PS3 )
					float flX = (255.0f * 0.5 * (u*oow + 1.0f) + 0.5f);
					float flY = (255.0f * 0.5 * (v*oow + 1.0f) + 0.5f);
					float flZ = (255.0f * 0.5 * (oow + 1.0f) + 0.5f);

					switch (iFace)
					{
						case CUBEMAP_FACE_RIGHT:
							flX = 255.0f - flX;
							flY = 255.0f - flY;
							break;
						case CUBEMAP_FACE_LEFT:
							flY = 255.0f - flY;
							flZ = 255.0f - flZ;
							break;
						case CUBEMAP_FACE_BACK:	
							break;
						case CUBEMAP_FACE_FRONT:
							flY = 255.0f - flY;
							flZ = 255.0f - flZ;
							break;
						case CUBEMAP_FACE_UP:
							flY = 255.0f - flY;
							break;
						case CUBEMAP_FACE_DOWN:
							flX = 255.0f - flX;
							flY = 255.0f - flY;
							flZ = 255.0f - flZ;
							break;
						default:
							break;
					}

					flX -= 128.0f;
					flY -= 128.0f;
					flZ -= 128.0f;

					flX /= 128.0f;
					flY /= 128.0f;
					flZ /= 128.0f;

					switch ( iFace )
					{
						case CUBEMAP_FACE_RIGHT:
							pixelWriter.WritePixelF( flZ, flY, flX, 0.0f );
							break;
						case CUBEMAP_FACE_LEFT:
							pixelWriter.WritePixelF( flZ, flY, flX, 0.0f );
							break;
						case CUBEMAP_FACE_BACK:	
							pixelWriter.WritePixelF( flX,  flZ,  flY, 0.0f );
							break;
						case CUBEMAP_FACE_FRONT:
							pixelWriter.WritePixelF( flX,  flZ,  flY, 0.0f );
							break;
						case CUBEMAP_FACE_UP:
							pixelWriter.WritePixelF( flX, flY,  flZ, 0.0f );
							break;
						case CUBEMAP_FACE_DOWN:
							pixelWriter.WritePixelF( flX, flY, flZ, 0.0f );
							break;
						default:
							break;
					}
#else
					int ix = (int)(255 * 0.5 * (u*oow + 1.0f) + 0.5f);
					ix = clamp( ix, 0, 255 );
					int iy = (int)(255 * 0.5 * (v*oow + 1.0f) + 0.5f);
					iy = clamp( iy, 0, 255 );
					int iz = (int)(255 * 0.5 * (oow + 1.0f) + 0.5f);
					iz = clamp( iz, 0, 255 );

					switch (iFace)
					{
					case CUBEMAP_FACE_RIGHT:
						ix = 255 - ix;
						iy = 255 - iy;
						break;
					case CUBEMAP_FACE_LEFT:
						iy = 255 - iy;
						iz = 255 - iz;
						break;
					case CUBEMAP_FACE_BACK:	
						break;
					case CUBEMAP_FACE_FRONT:
						iy = 255 - iy;
						iz = 255 - iz;
						break;
					case CUBEMAP_FACE_UP:
						iy = 255 - iy;
						break;
					case CUBEMAP_FACE_DOWN:
						ix = 255 - ix;
						iy = 255 - iy;
						iz = 255 - iz;
						break;
					default:
						break;
					}

					ix -= 128;
					iy -= 128;
					iz -= 128;

					Assert( ix >= -128 && ix <= 127 );
					Assert( iy >= -128 && iy <= 127 );
					Assert( iz >= -128 && iz <= 127 );

					switch (iFace)
					{
					case CUBEMAP_FACE_RIGHT:
						// correct
//						pixelWriter.WritePixelSigned( -128, -128, -128, 0 );
						pixelWriter.WritePixelSigned( iz, iy, ix, 0 );
						break;
					case CUBEMAP_FACE_LEFT:
						// correct
//						pixelWriter.WritePixelSigned( -128, -128, -128, 0 );
						pixelWriter.WritePixelSigned( iz, iy, ix, 0 );
						break;
					case CUBEMAP_FACE_BACK:	
						// wrong
//						pixelWriter.WritePixelSigned( -128, -128, -128, 0 );
						pixelWriter.WritePixelSigned( ix, iz, iy, 0 );
//						pixelWriter.WritePixelSigned( -127, -127, 127, 0 );
						break;
					case CUBEMAP_FACE_FRONT:
						// wrong
//						pixelWriter.WritePixelSigned( -128, -128, -128, 0 );
						pixelWriter.WritePixelSigned( ix, iz, iy, 0 );
						break;
					case CUBEMAP_FACE_UP:
						// correct
//						pixelWriter.WritePixelSigned( -128, -128, -128, 0 );
						pixelWriter.WritePixelSigned( ix, iy, iz, 0 );
						break;
					case CUBEMAP_FACE_DOWN:
						// correct
//						pixelWriter.WritePixelSigned( -128, -128, -128, 0 );
						pixelWriter.WritePixelSigned( ix, iy, iz, 0 );
						break;
					default:
						break;
					}
#endif
				} // x
			} // y
		} // iFace
	}

	// NOTE: The normalization cubemap regenerator is stateless
	// so there's no need to allocate + deallocate them
	virtual void Release() {}
};

static void CreateNormalizationCubemap( ITextureInternal *pTexture )
{
	// NOTE: The normalization cubemap regenerator is stateless
	// so there's no need to allocate + deallocate them
	static CNormalizationCubemap s_NormalizationCubemap;
	pTexture->SetTextureRegenerator( &s_NormalizationCubemap );
}

static void CreateSignedNormalizationCubemap( ITextureInternal *pTexture )
{
	// NOTE: The normalization cubemap regenerator is stateless
	// so there's no need to allocate + deallocate them
	static CSignedNormalizationCubemap s_SignedNormalizationCubemap;
	pTexture->SetTextureRegenerator( &s_SignedNormalizationCubemap );
}

/*
static void CreateSSAONoiseTexture( ITextureInternal *pTexture )
{
	// NOTE: This texture regenerator is stateless so there's no need to allocate + deallocate
	static CSSAONoiseMap s_SSAONoiseMap;
	pTexture->SetTextureRegenerator( &s_SSAONoiseMap );
}
*/


//-----------------------------------------------------------------------------
// Implementation of the texture manager
//-----------------------------------------------------------------------------
class CTextureManager : public ITextureManager
{
public:
	CTextureManager( void );

	// Initialization + shutdown
	virtual void Init( int nFlags );
	virtual void Shutdown();

	virtual void AllocateStandardRenderTargets( );
	virtual void FreeStandardRenderTargets();

	virtual void CacheExternalStandardRenderTargets();

	virtual ITextureInternal *CreateProceduralTexture( const char *pTextureName, const char *pTextureGroupName, int w, int h, int d, ImageFormat fmt, int nFlags );
	virtual ITextureInternal *FindOrLoadTexture( const char *textureName, const char *pTextureGroupName, int nAdditionalCreationFlags = 0 );
 	virtual bool IsTextureLoaded( const char *pTextureName );
	virtual bool GetTextureInformation( char const *szTextureName, MaterialTextureInfo_t &info );

	virtual void AddTextureAlias( const char *pAlias, const char *pRealName );
	virtual void RemoveTextureAlias( const char *pAlias );

	virtual void SetExcludedTextures( const char *pScriptName, bool bUsingWeaponModelCache );
	virtual void UpdateExcludedTextures();
	virtual void ClearForceExcludes();

	virtual void ResetTextureFilteringState();
	void ReloadTextures( void );

	// These are used when we lose our video memory due to a mode switch etc
	void ReleaseTextures( bool bReleaseManaged = true );
	void RestoreNonRenderTargetTextures( void );
	void RestoreRenderTargets( void );

	// delete any texture that has a refcount <= 0
	void RemoveUnusedTextures( void );
	void DebugPrintUsedTextures( void );

	// Request a texture ID
	virtual int	RequestNextTextureID();

	// Get at a couple standard textures
	virtual ITextureInternal *ErrorTexture();
	virtual ITextureInternal *NormalizationCubemap();
	virtual ITextureInternal *SignedNormalizationCubemap();
	virtual ITextureInternal *ShadowNoise2D();
	virtual ITextureInternal *SSAONoise2D();	
	virtual ITextureInternal *IdentityLightWarp();
	virtual ITextureInternal *ColorCorrectionTexture( int i );
	virtual ITextureInternal *FullFrameDepthTexture();
	virtual ITextureInternal *StereoParamTexture();

	// Generates an error texture pattern
	virtual void GenerateErrorTexture( ITexture *pTexture, IVTFTexture *pVTFTexture );

	// Updates the color correction state
	virtual void SetColorCorrectionTexture( int i, ITextureInternal *pTexture );

	virtual void ForceAllTexturesIntoHardware( void );

	virtual ITextureInternal *CreateRenderTargetTexture( 
		const char *pRTName,	// NULL for auto-generated name
		int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode, 
		ImageFormat fmt, 
		RenderTargetType_t type, 
		unsigned int textureFlags, 
		unsigned int renderTargetFlags,
		bool bMultipleTargets );

	virtual void RemoveTexture( ITextureInternal *pTexture );
	virtual void ReloadFilesInList( IFileList *pFilesToReload );

	// start with -1, list terminates with -1
	virtual int FindNext( int iIndex, ITextureInternal **ppTexture );

	virtual void ReleaseTempRenderTargetBits( void );

protected:
	ITextureInternal *FindTexture( const char *textureName );
	ITextureInternal *LoadTexture( const char *textureName, const char *pTextureGroupName, int nAdditionalCreationFlags = 0 );

	// Restores a single texture
	void RestoreTexture( ITextureInternal* pTex );

	CUtlDict< ITextureInternal *, unsigned short > m_TextureList;
	CUtlDict< const char *, unsigned short > m_TextureAliases;
	CUtlDict< int, unsigned short > m_TextureExcludes;	

	bool m_bUsingWeaponModelCache;

	int m_iNextTexID;
	int m_nFlags;

	ITextureInternal *m_pErrorTexture;
	ITextureInternal *m_pBlackTexture;
	ITextureInternal *m_pWhiteTexture;
	ITextureInternal *m_pGreyTexture;
	ITextureInternal *m_pGreyAlphaZeroTexture;
	ITextureInternal *m_pNormalizationCubemap;
	ITextureInternal *m_pFullScreenTexture;
	ITextureInternal *m_pSignedNormalizationCubemap;
	ITextureInternal *m_pShadowNoise2D;
	ITextureInternal *m_pSSAONoise2D;
	ITextureInternal *m_pIdentityLightWarp;
	ITextureInternal *m_pColorCorrectionTextures[ COLOR_CORRECTION_MAX_TEXTURES ];
	ITextureInternal *m_pFullScreenDepthTexture;
	ITextureInternal *m_pStereoParamTexture;

	// Used to generate various error texture patterns when necessary
#ifdef ERROR_TEXTURE_IS_SOLID
	CSolidTexture *m_pErrorRegen;
#else
	CCheckerboardTexture *m_pErrorRegen;
#endif

private:
	bool ParseTextureExcludeScript( const char *pScriptName );
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CTextureManager s_TextureManager;
ITextureManager *g_pTextureManager = &s_TextureManager;


//-----------------------------------------------------------------------------
// Texture manager
//-----------------------------------------------------------------------------
CTextureManager::CTextureManager( void ) : m_TextureList( true ), m_TextureAliases( true ), m_TextureExcludes( true )
{
	m_pErrorTexture = NULL;
	m_pBlackTexture = NULL;
	m_pWhiteTexture = NULL;
	m_pGreyTexture  = NULL;
	m_pGreyAlphaZeroTexture  = NULL;
	m_pNormalizationCubemap = NULL;
	m_pErrorRegen = NULL;
	m_pFullScreenTexture = NULL;
	m_pSignedNormalizationCubemap = NULL;
	m_pShadowNoise2D = NULL;
	m_pSSAONoise2D = NULL;
	m_pIdentityLightWarp = NULL;
	m_pFullScreenDepthTexture = NULL;
	m_pStereoParamTexture = NULL;
	m_bUsingWeaponModelCache = false;
}


//-----------------------------------------------------------------------------
// Initialization + shutdown
//-----------------------------------------------------------------------------
void CTextureManager::Init( int nFlags )
{
	m_nFlags = nFlags;
	color32 color, color2;
	m_iNextTexID = 4096;

	// setup the checkerboard generator for failed texture loading
	color.r = color.g = color.b = 0; color.a = 128;
	color2.r = color2.b = color2.a = 255; color2.g = 0;
	
#ifdef ERROR_TEXTURE_IS_SOLID
	color32 color_black; color_black.r = color_black.g = color_black.b = 0; color_black.a = 255;
	m_pErrorRegen = new CSolidTexture( color_black );
#else
	m_pErrorRegen = new CCheckerboardTexture( 4, color, color2 );
#endif

	// Create an error texture
	m_pErrorTexture = CreateProceduralTexture( "error", TEXTURE_GROUP_OTHER,
		ERROR_TEXTURE_SIZE, ERROR_TEXTURE_SIZE, 1, IMAGE_FORMAT_BGRA8888, TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_SRGB );
#ifdef ERROR_TEXTURE_IS_SOLID
	CreateSolidTexture( m_pErrorTexture, color_black );
#else
	CreateCheckerboardTexture( m_pErrorTexture, 4, color, color2 );
#endif

	// Create a white texture
	m_pWhiteTexture = CreateProceduralTexture( "white", TEXTURE_GROUP_OTHER,
		WHITE_TEXTURE_SIZE, WHITE_TEXTURE_SIZE, 1, IMAGE_FORMAT_BGRX8888, TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_SRGB );
	color.r = color.g = color.b = color.a = 255;
	CreateSolidTexture( m_pWhiteTexture, color );

	// Create a black texture
	m_pBlackTexture = CreateProceduralTexture( "black", TEXTURE_GROUP_OTHER,
		BLACK_TEXTURE_SIZE, BLACK_TEXTURE_SIZE, 1, IMAGE_FORMAT_BGRX8888, TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_SRGB );
	color.r = color.g = color.b = 0;
	CreateSolidTexture( m_pBlackTexture, color );

	// Create a grey texture
	m_pGreyTexture = CreateProceduralTexture( "grey", TEXTURE_GROUP_OTHER,
		GREY_TEXTURE_SIZE, GREY_TEXTURE_SIZE, 1, IMAGE_FORMAT_BGRA8888, TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_SRGB );
	color.r = color.g = color.b = 128;
	color.a = 255;
	CreateSolidTexture( m_pGreyTexture, color );

	// Create a grey texture
	m_pGreyAlphaZeroTexture = CreateProceduralTexture( "greyalphazero", TEXTURE_GROUP_OTHER,
		GREY_TEXTURE_SIZE, GREY_TEXTURE_SIZE, 1, IMAGE_FORMAT_BGRA8888, TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_SRGB );
	color.r = color.g = color.b = 128;
	color.a = 0;
	CreateSolidTexture( m_pGreyAlphaZeroTexture, color );

#ifdef IS_WINDOWS_PC
	if ( g_pShaderAPI->IsStereoSupported() )
	{
		// TODO: Call CreateStereoTexture, which should make a similar call onto the ShaderAPI
		int stereoWidth = 8;
		int stereoHeight = 1;
		m_pStereoParamTexture = CreateProceduralTexture( "stereoparam", TEXTURE_GROUP_OTHER,
			stereoWidth, stereoHeight, 1, IMAGE_FORMAT_R32F, TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_POINTSAMPLE | TEXTUREFLAGS_DEFAULT_POOL);
	}
#endif

	if ( HardwareConfig()->GetMaxDXSupportLevel() >= 80 )
	{
		// Create a normalization cubemap
		m_pNormalizationCubemap = CreateProceduralTexture( "normalize", TEXTURE_GROUP_CUBE_MAP,
			NORMALIZATION_CUBEMAP_SIZE, NORMALIZATION_CUBEMAP_SIZE, 1, IMAGE_FORMAT_BGRX8888,
			TEXTUREFLAGS_ENVMAP | TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_SINGLECOPY |
			TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_CLAMPU );
		CreateNormalizationCubemap( m_pNormalizationCubemap );
	}

	if ( HardwareConfig()->GetMaxDXSupportLevel() >= 90 )
	{
		// On MacOS, we have poor format support, so we ask for signed float
		ImageFormat fmt = IsOpenGL() ? IMAGE_FORMAT_RGBA16161616F : IMAGE_FORMAT_UVWQ8888;
		
		int nFlags = TEXTUREFLAGS_ENVMAP | TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_CLAMPU;
		nFlags |= IsOSXOpenGL() ? TEXTUREFLAGS_POINTSAMPLE : 0; // JasonM - ridiculous hack around R500 lameness...we never use this texture on MacOS anyways (right?)
		
		// Create a normalization cubemap
		m_pSignedNormalizationCubemap = CreateProceduralTexture( "normalizesigned", TEXTURE_GROUP_CUBE_MAP,
																NORMALIZATION_CUBEMAP_SIZE, NORMALIZATION_CUBEMAP_SIZE, 1, fmt, nFlags );
		CreateSignedNormalizationCubemap( m_pSignedNormalizationCubemap );
		
		m_pIdentityLightWarp = FindOrLoadTexture( "dev/IdentityLightWarp", TEXTURE_GROUP_OTHER );
		m_pIdentityLightWarp->IncrementReferenceCount();
	}

	// For safety, always load the shadow noise 2D texture even on 9.0 hardware. (It's not needed in Portal2's flashlight shaders, but I'm leaving it in
	// because it's referenced all over the place and so the older L4D-style flashlight shadows can be easily re-enabled if needed.)
	m_pShadowNoise2D = FindOrLoadTexture( "engine/NormalizedRandomDirections2D", TEXTURE_GROUP_OTHER );
	m_pShadowNoise2D->IncrementReferenceCount();

	if ( HardwareConfig()->GetMaxDXSupportLevel() >= 92 )
	{
		m_pSSAONoise2D = FindOrLoadTexture( "engine/SSAOReflectionVectors", TEXTURE_GROUP_OTHER );
		m_pSSAONoise2D->IncrementReferenceCount();
	}
}

void CTextureManager::Shutdown()
{
	FreeStandardRenderTargets();

	// These checks added because it's possible for shutdown to be called before the material system is 
	// fully initialized.
	if ( m_pWhiteTexture )
	{
		m_pWhiteTexture->DecrementReferenceCount();
		m_pWhiteTexture = NULL;
	}

	if ( m_pBlackTexture )
	{
		m_pBlackTexture->DecrementReferenceCount();
		m_pBlackTexture = NULL;
	}

	if ( m_pGreyTexture )
	{
		m_pGreyTexture->DecrementReferenceCount();
		m_pGreyTexture = NULL;
	}

	if ( m_pGreyAlphaZeroTexture )
	{
		m_pGreyAlphaZeroTexture->DecrementReferenceCount();
		m_pGreyAlphaZeroTexture = NULL;
	}

	if ( m_pNormalizationCubemap )
	{
		m_pNormalizationCubemap->DecrementReferenceCount();
		m_pNormalizationCubemap = NULL;
	}

	if ( m_pSignedNormalizationCubemap )
	{
		m_pSignedNormalizationCubemap->DecrementReferenceCount();
		m_pSignedNormalizationCubemap = NULL;
	}

	if ( m_pShadowNoise2D )
	{
		m_pShadowNoise2D->DecrementReferenceCount();
		m_pShadowNoise2D = NULL;
	}

	if ( m_pSSAONoise2D )
	{
		m_pSSAONoise2D->DecrementReferenceCount();
		m_pSSAONoise2D = NULL;
	}

	if ( m_pIdentityLightWarp )
	{
		m_pIdentityLightWarp->DecrementReferenceCount();
		m_pIdentityLightWarp = NULL;
	}

	if ( m_pErrorTexture )
	{
		m_pErrorTexture->DecrementReferenceCount();
		m_pErrorTexture = NULL;
	}

	if ( m_pStereoParamTexture )
	{
		m_pStereoParamTexture->DecrementReferenceCount();
		m_pStereoParamTexture = NULL;
	}

	ReleaseTextures();

	if ( m_pErrorRegen )
	{
		m_pErrorRegen->Release();
		m_pErrorRegen = NULL;
	}

	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		ITextureInternal::Destroy( m_TextureList[i] );
	}
	m_TextureList.RemoveAll();

	for( int i = m_TextureAliases.First(); i != m_TextureAliases.InvalidIndex(); i = m_TextureAliases.Next( i ) )
	{
		delete []m_TextureAliases[i];
	}
	m_TextureAliases.RemoveAll();

	m_TextureExcludes.RemoveAll();
}


//-----------------------------------------------------------------------------
// Allocate, free standard render target textures
//-----------------------------------------------------------------------------
void CTextureManager::AllocateStandardRenderTargets( )
{
	bool bAllocateFullscreenTexture = ( m_nFlags & MATERIAL_INIT_ALLOCATE_FULLSCREEN_TEXTURE ) != 0;
	bool bAllocateMorphAccumTexture = g_pMorphMgr->ShouldAllocateScratchTextures();

	if ( IsPC() && ( bAllocateFullscreenTexture || bAllocateMorphAccumTexture ) )
	{
		MaterialSystem()->BeginRenderTargetAllocation();

		// A offscreen render target which is the size + format of the back buffer (*not* HDR format!)
		if ( bAllocateFullscreenTexture )
		{
			// Ensure the _rt_FullScreen RT is given its own depth-stencil surface (RENDER_TARGET_WITH_DEPTH vs. RENDER_TARGET) on the PC/Mac to work around store rendering glitches between the bot panel and the rest of the store UI.
			m_pFullScreenTexture = CreateRenderTargetTexture( "_rt_FullScreen", 1, 1, RT_SIZE_FULL_FRAME_BUFFER_ROUNDED_UP, MaterialSystem()->GetBackBufferFormat(), RENDER_TARGET_WITH_DEPTH, TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT, 0, false );
						
			m_pFullScreenTexture->IncrementReferenceCount();
		}

		// This texture is the one we accumulate morph deltas into
		if ( bAllocateMorphAccumTexture )
		{
			g_pMorphMgr->AllocateScratchTextures();
			g_pMorphMgr->AllocateMaterials();
		}

		MaterialSystem()->EndRenderTargetAllocation();
	}
}


void CTextureManager::FreeStandardRenderTargets()
{
	if ( m_pFullScreenTexture )
	{
		m_pFullScreenTexture->DecrementReferenceCount();
		m_pFullScreenTexture = NULL;
	}

	g_pMorphMgr->FreeMaterials();
	g_pMorphMgr->FreeScratchTextures();

#if defined( FEATURE_SUBD_SUPPORT )
	g_pSubDMgr->FreeTextures();
#endif
}


void CTextureManager::CacheExternalStandardRenderTargets()
{
	m_pFullScreenDepthTexture = FindTexture( "_rt_FullFrameDepth" ); //created/destroyed in engine/matsys_interface.cpp to properly track hdr changes
}


//-----------------------------------------------------------------------------
// Generates an error texture pattern
//-----------------------------------------------------------------------------
void CTextureManager::GenerateErrorTexture( ITexture *pTexture, IVTFTexture *pVTFTexture )
{
	m_pErrorRegen->RegenerateTextureBits( pTexture, pVTFTexture, NULL );
}

//-----------------------------------------------------------------------------
// Updates the color correction state
//-----------------------------------------------------------------------------
ITextureInternal *CTextureManager::ColorCorrectionTexture( int i )
{
	Assert( i<COLOR_CORRECTION_MAX_TEXTURES );
	return m_pColorCorrectionTextures[ i ];
}

void CTextureManager::SetColorCorrectionTexture( int i, ITextureInternal *pTexture )
{
	Assert( i<COLOR_CORRECTION_MAX_TEXTURES );

	if( m_pColorCorrectionTextures[i] )
	{
		m_pColorCorrectionTextures[i]->DecrementReferenceCount();
	}

	m_pColorCorrectionTextures[i] = pTexture;
	if( pTexture )
		pTexture->IncrementReferenceCount();
}


//-----------------------------------------------------------------------------
// Releases all textures (cause we've lost video memory)
//-----------------------------------------------------------------------------
void CTextureManager::ReleaseTextures( bool bReleaseManaged /*= true*/ )
{
	g_pShaderAPI->SetFullScreenTextureHandle( INVALID_SHADERAPI_TEXTURE_HANDLE );

	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		if ( bReleaseManaged || m_TextureList[i]->IsRenderTarget() || m_TextureList[i]->IsDefaultPool() )
		{
			// Release the texture...
			m_TextureList[i]->Release();
		}
	}
}


//-----------------------------------------------------------------------------
// Request a texture ID
//-----------------------------------------------------------------------------
int CTextureManager::RequestNextTextureID()
{
	// FIXME: Deal better with texture ids
	// The range between 19000 and 21000 are used for standard textures + lightmaps
	if (m_iNextTexID == 19000)
	{
		m_iNextTexID = 21000;
	}

	return m_iNextTexID++;
}


//-----------------------------------------------------------------------------
// Restores a single texture
//-----------------------------------------------------------------------------
void CTextureManager::RestoreTexture( ITextureInternal* pTexture )
{
	// Put the texture back onto the board
	pTexture->OnRestore();	// Give render targets a chance to reinitialize themselves if necessary (due to AA changes).
	pTexture->Download();
}


//-----------------------------------------------------------------------------
// Restore all textures (cause we've got video memory again)
//-----------------------------------------------------------------------------
void CTextureManager::RestoreNonRenderTargetTextures()
{
	// 360 should not have gotten here
	Assert( !IsX360() );

	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		if ( !m_TextureList[i]->IsRenderTarget() )
		{
			RestoreTexture( m_TextureList[i] );
		}
	}
}

//-----------------------------------------------------------------------------
// Restore just the render targets (cause we've got video memory again)
//-----------------------------------------------------------------------------
void CTextureManager::RestoreRenderTargets()
{
	// 360 should not have gotten here
	Assert( !IsX360() );

	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		if ( m_TextureList[i]->IsRenderTarget() )
		{
			RestoreTexture( m_TextureList[i] );
		}
	}

	if ( m_pFullScreenTexture )
	{
		g_pShaderAPI->SetFullScreenTextureHandle( m_pFullScreenTexture->GetTextureHandle( 0 ) );
	}

	CacheExternalStandardRenderTargets();
}


//-----------------------------------------------------------------------------
// Reloads all textures
//-----------------------------------------------------------------------------
void CTextureManager::ReloadTextures()
{
	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		// Put the texture back onto the board
		m_TextureList[i]->Download();
	}
}

static void ForceTextureIntoHardware( ITexture *pTexture, IMaterial *pMaterial, IMaterialVar *pBaseTextureVar )
{
	if ( IsGameConsole() )
		return;

	pBaseTextureVar->SetTextureValue( pTexture );

	CMatRenderContextPtr pRenderContext( MaterialSystem()->GetRenderContext() );
	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_TRIANGLES, 1 );

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
	meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
	meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
	meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
	meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
	meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
	meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
	meshBuilder.TexCoord2f( 0, 0.0f, 0.0f );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

//-----------------------------------------------------------------------------
// Reloads all textures
//-----------------------------------------------------------------------------
void CTextureManager::ForceAllTexturesIntoHardware( void )
{
	if ( IsGameConsole() )
		return;

	IMaterial *pMaterial = MaterialSystem()->FindMaterial( "engine/preloadtexture", "texture preload" );
	pMaterial = ((IMaterialInternal *)pMaterial)->GetRealTimeVersion(); //always work with the realtime material internally
	bool bFound;
	IMaterialVar *pBaseTextureVar = pMaterial->FindVar( "$basetexture", &bFound );
	if( !bFound )
	{
		return;
	}

	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		// Put the texture back onto the board
		ForceTextureIntoHardware( m_TextureList[i], pMaterial, pBaseTextureVar );
	}
}

//-----------------------------------------------------------------------------
// Get at a couple standard textures
//-----------------------------------------------------------------------------
ITextureInternal *CTextureManager::ErrorTexture()
{
	return m_pErrorTexture;
}

ITextureInternal *CTextureManager::NormalizationCubemap()
{
	return m_pNormalizationCubemap; 
}

ITextureInternal *CTextureManager::SignedNormalizationCubemap()
{
	return m_pSignedNormalizationCubemap; 
}

ITextureInternal *CTextureManager::ShadowNoise2D()
{
	return m_pShadowNoise2D; 
}

ITextureInternal *CTextureManager::SSAONoise2D()
{
	return m_pSSAONoise2D; 
}

ITextureInternal *CTextureManager::IdentityLightWarp()
{
	return m_pIdentityLightWarp; 
}

ITextureInternal *CTextureManager::FullFrameDepthTexture()
{
	return m_pFullScreenDepthTexture;
}

ITextureInternal *CTextureManager::StereoParamTexture()
{
	return m_pStereoParamTexture ? 	m_pStereoParamTexture : m_pErrorTexture;
}

//-----------------------------------------------------------------------------
// Creates a procedural texture
//-----------------------------------------------------------------------------
ITextureInternal *CTextureManager::CreateProceduralTexture( 
	const char			*pTextureName, 
	const char			*pTextureGroupName, 
	int					w, 
	int					h, 
	int					d, 
	ImageFormat			fmt, 
	int					nFlags )
{
	ITextureInternal *pNewTexture = ITextureInternal::CreateProceduralTexture( pTextureName, pTextureGroupName, w, h, d, fmt, nFlags );
	if ( !pNewTexture )
		return NULL;

	// Add it to the list of textures so it can be restored, etc.
	m_TextureList.Insert( pNewTexture->GetName(), pNewTexture );

	if ( ( nFlags & TEXTUREFLAGS_SKIP_INITIAL_DOWNLOAD ) != TEXTUREFLAGS_SKIP_INITIAL_DOWNLOAD )
	{
		// NOTE: This will download the texture only if the shader api is ready
		pNewTexture->Download();
	}

	return pNewTexture;
}

//-----------------------------------------------------------------------------
// FIXME: Need some better understanding of when textures should be added to
// the texture dictionary here. Is it only for files, for example?
// Texture dictionary...
//-----------------------------------------------------------------------------
ITextureInternal *CTextureManager::LoadTexture( const char *pTextureName, const char *pTextureGroupName, int nAdditionalCreationFlags /* = 0 */  )
{
	ITextureInternal *pNewTexture = ITextureInternal::CreateFileTexture( pTextureName, pTextureGroupName );
	if ( pNewTexture )
	{
		int iIndex = m_TextureExcludes.Find( pNewTexture->GetName() );
		if ( m_TextureExcludes.IsValidIndex( iIndex ) )
		{
			// mark the new texture as excluded
			int nDimensionsLimit = m_TextureExcludes[iIndex];
			pNewTexture->MarkAsExcluded( ( nDimensionsLimit == 0 ), nDimensionsLimit );
		}
		else if ( m_bUsingWeaponModelCache && g_pQueuedLoader->IsMapLoading() )
		{
			// Unfortunate, but the weapon textures get automatically subverted
			// to avoid ensuring that scripts do not need to be maintained as new weapons occur.
			// When a weapon texture in not explicitly excluded (which trumps), ensure the exclusion.
			if ( V_stristr( pNewTexture->GetName(), "weapons/v_models" ) || 
				V_stristr( pNewTexture->GetName(), "weapons/w_models" ) || 
				V_stristr( pNewTexture->GetName(), "weapons/shared" ) )
			{
				// ALL weapon textures (subject to temp exclusion) are getting pre-excluded down to 16, which matches the weapon model cache
				// exclusion expectation during loading.
				//
				// This is necessary to avoid a horrible memory load pattern where the QL would otherwise load the texture at full-res and then
				// the weapon model cache would then evict causing a reload as it evicts down to 16.
				//
				// This hack is because the QL is blasting these in BEFORE the weapon model cache has any chance to know what are the actual dependent
				// weapon materials that are subject to initial eviction.
				//
				// Instead this gets in front of the QL which will bring in ALL weapon based textures in at the desired reduced state with a single load/free.
				// Then, there is a fixup by weapon model cache that has then discovered which texture are the REAL dependents and restores the ones
				// that got broadly classified here (i.e. shared textures that can't be subject to temp evictions). Temp Exclusion abilities cannot
				// be determined this early, thus the broad classification, and the unfortunate minor fixup
				pNewTexture->MarkAsExcluded( false, 16, true );
			}
		}

		// Stick the texture onto the board
		pNewTexture->Download( NULL, nAdditionalCreationFlags );

		// FIXME: If there's been an error loading, we don't also want this error...
	}

	return pNewTexture;
}

ITextureInternal *CTextureManager::FindTexture( const char *pTextureName )
{
	if ( !pTextureName || pTextureName[0] == 0 )
		return NULL;
	
	char szCleanName[MAX_PATH];
	NormalizeTextureName( pTextureName, szCleanName, sizeof( szCleanName ) );

	int i = m_TextureList.Find( szCleanName );
	if ( i != m_TextureList.InvalidIndex() )
	{
		return m_TextureList[i];
	}

	i = m_TextureAliases.Find( szCleanName );
	if ( i != m_TextureAliases.InvalidIndex() )
	{
		return FindTexture( m_TextureAliases[i] );
	}

	// Special handling: lightmaps
	if ( char const *szLightMapNum = StringAfterPrefix( szCleanName, "[lightmap" ) )
	{
		int iLightMapNum = atoi( szLightMapNum );
		extern CMaterialSystem g_MaterialSystem;
		CMatLightmaps *plm = g_MaterialSystem.GetLightmaps();
		if ( iLightMapNum >= 0 &&
			 iLightMapNum < plm->GetNumLightmapPages() )
		{
			ShaderAPITextureHandle_t hTex = plm->GetLightmapPageTextureHandle( iLightMapNum );
			if ( hTex != INVALID_SHADERAPI_TEXTURE_HANDLE )
			{
				// Establish the lookup linking in the dictionary
				ITextureInternal *pTxInt = ITextureInternal::CreateReferenceTextureFromHandle( pTextureName, TEXTURE_GROUP_LIGHTMAP, hTex );
				m_TextureList.Insert( pTextureName, pTxInt );
				return pTxInt;
			}
		}
	}

	// scaleform textures bypass the texture manager
	if ( !V_strncmp( szCleanName, "scaleform", 9 ) )
	{
		ShaderAPITextureHandle_t hTex = g_pShaderAPI->FindTexture( szCleanName );
		if ( hTex != INVALID_SHADERAPI_TEXTURE_HANDLE )
		{
			// Establish the lookup linking in the dictionary
			ITextureInternal *pTxInt = ITextureInternal::CreateReferenceTextureFromHandle( szCleanName, TEXTURE_GROUP_SCALEFORM, hTex );
			m_TextureList.Insert( szCleanName, pTxInt );
			return pTxInt;
		}
	}

	return NULL;
}

void CTextureManager::AddTextureAlias( const char *pAlias, const char *pRealName )
{
	if	( (pAlias == NULL) || (pRealName == NULL) )
		return; //invalid alias

	char szCleanName[MAX_PATH];
	int index = m_TextureAliases.Find( NormalizeTextureName( pAlias, szCleanName, sizeof( szCleanName ) ) );

	if	( index != m_TextureAliases.InvalidIndex() )
	{
		AssertMsg( Q_stricmp( pRealName, m_TextureAliases[index] ) == 0, "Trying to use one name to alias two different textures." );
		RemoveTextureAlias( pAlias ); //remove the old alias to make room for the new one.
	}

	size_t iRealNameLength = strlen( pRealName ) + 1;
	char *pRealNameCopy = new char [iRealNameLength];
	memcpy( pRealNameCopy, pRealName, iRealNameLength );

	m_TextureAliases.Insert( szCleanName, pRealNameCopy );
}

void CTextureManager::RemoveTextureAlias( const char *pAlias )
{
	if ( pAlias == NULL )
		return;

	char szCleanName[MAX_PATH];
	int index = m_TextureAliases.Find( NormalizeTextureName( pAlias, szCleanName, sizeof( szCleanName ) ) );
	if ( index == m_TextureAliases.InvalidIndex() )
		return; //not found

	delete []m_TextureAliases[index];
	m_TextureAliases.RemoveAt( index );
}

bool CTextureManager::ParseTextureExcludeScript( const char *pScriptName )
{
	// get optional script
	if ( !pScriptName || !pScriptName[0] )
		return false;

	CUtlBuffer excludeBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( pScriptName, NULL, excludeBuffer ) )
		return false;

	char szToken[MAX_PATH];
	while ( 1 )
	{
		// must support spaces in names without quotes
		// have to brute force parse up to a valid line
		while ( 1 )
		{
			excludeBuffer.EatWhiteSpace();
			if ( !excludeBuffer.EatCPPComment() )
			{
				// not a comment
				break;
			}
		}
		excludeBuffer.GetLine( szToken, sizeof( szToken ) );
		int tokenLength = strlen( szToken );
		if ( !tokenLength )
		{
			// end of list
			break;
		}

		// remove all trailing whitespace
		while ( tokenLength > 0 )
		{
			tokenLength--;
			if ( V_isgraph( szToken[tokenLength] ) )
			{
				break;
			}
			szToken[tokenLength] = '\0';
		}

		// first optional token may be a dimension limit hint
		int nDimensionsLimit = 0;
		char *pTextureName = szToken;
		if ( pTextureName[0] != 0 && V_isdigit( pTextureName[0] ) )
		{
			nDimensionsLimit = atoi( pTextureName );
			
			// skip forward to name
			for ( ;; )
			{
				char ch = *pTextureName;
				if ( !ch || ( !V_isdigit( ch ) && !V_isspace( ch ) ) )
				{
					break;
				}
				pTextureName++;
			}
		}

		char szCleanName[MAX_PATH];
		NormalizeTextureName( pTextureName, szCleanName, sizeof( szCleanName ) );

		int iIndex = m_TextureExcludes.Find( szCleanName );
		if ( m_TextureExcludes.IsValidIndex( iIndex ) )
		{
			// do not duplicate, override existing entry
			m_TextureExcludes[iIndex] = nDimensionsLimit;
		}
		else
		{
			m_TextureExcludes.Insert( szCleanName, nDimensionsLimit );
		}
	}

	return true;
}

void CTextureManager::SetExcludedTextures( const char *pScriptName, bool bUsingWeaponModelCache )
{
	MEM_ALLOC_CREDIT();

	m_bUsingWeaponModelCache = IsGameConsole() && bUsingWeaponModelCache;

	// clear all existing texture's exclusion
	for ( int i = m_TextureExcludes.First(); i != m_TextureExcludes.InvalidIndex(); i = m_TextureExcludes.Next( i ) )
	{
		ITextureInternal *pTexture = FindTexture( m_TextureExcludes.GetElementName( i ) );
		if ( pTexture )
		{
			pTexture->MarkAsExcluded( false, 0 );
		}
	}
	m_TextureExcludes.RemoveAll();

	// run through exclusions, build final aggregate list
	// optional global script first
	ParseTextureExcludeScript( "//MOD/maps/_exclude.lst" );
	// optional spec'd script further refines
	ParseTextureExcludeScript( pScriptName );

	// perform exclusions
	for ( int i = m_TextureExcludes.First(); i != m_TextureExcludes.InvalidIndex(); i = m_TextureExcludes.Next( i ) )
	{
		// set any existing texture's exclusion
		// textures that don't exist yet will get caught during their creation path
		ITextureInternal *pTexture = FindTexture( m_TextureExcludes.GetElementName( i ) );
		if ( pTexture )
		{
			int nDimensionsLimit = m_TextureExcludes[i];
			pTexture->MarkAsExcluded( ( nDimensionsLimit == 0 ), nDimensionsLimit );
		}
	}
}

void CTextureManager::UpdateExcludedTextures( void )
{
	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		m_TextureList[i]->UpdateExcludedState();
	}
}

void CTextureManager::ClearForceExcludes( void )
{
	if ( !m_bUsingWeaponModelCache )
	{
		// forced excludes are a temp state promoted by the weapon model cache
		return;
	}

	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		if ( m_TextureList[i]->IsForceExcluded() )
		{
			m_TextureList[i]->ClearForceExclusion();
		}
	}
}

ITextureInternal *CTextureManager::FindOrLoadTexture( const char *pTextureName, const char *pTextureGroupName, int nAdditionalCreationFlags /* = 0 */ )
{
	ITextureInternal *pTexture = FindTexture( pTextureName );
	if ( !pTexture )
	{
#if defined( DEVELOPMENT_ONLY ) || defined( ALLOW_TEXT_MODE )
		static bool s_bTextMode = CommandLine()->HasParm( "-textmode" );
		if ( s_bTextMode )
		{
			return m_pErrorTexture;
		}
#endif
			
		pTexture = LoadTexture( pTextureName, pTextureGroupName, nAdditionalCreationFlags );
		if ( pTexture )
		{
			// insert into the dictionary using the processed texture name
			m_TextureList.Insert( pTexture->GetName(), pTexture );
		}
	}

	return pTexture;
}

bool CTextureManager::IsTextureLoaded( const char *pTextureName )
{
	ITextureInternal *pTexture = FindTexture( pTextureName );
	return ( pTexture != NULL );
}

bool CTextureManager::GetTextureInformation( char const *szTextureName, MaterialTextureInfo_t &info )
{
	extern bool CTextureImpl_GetTextureInformation( char const *szTextureName, MaterialTextureInfo_t &info );
	return CTextureImpl_GetTextureInformation( szTextureName, info );
}


//-----------------------------------------------------------------------------
// Creates a texture that's a render target
//-----------------------------------------------------------------------------
ITextureInternal *CTextureManager::CreateRenderTargetTexture( 
	const char *pRTName,	// NULL for auto-generated name
	int w, 
	int h, 
	RenderTargetSizeMode_t sizeMode, 
	ImageFormat fmt, 
	RenderTargetType_t type, 
	unsigned int textureFlags, 
	unsigned int renderTargetFlags,
	bool bMultipleTargets )
{
	MEM_ALLOC_CREDIT_( __FILE__ ": Render target" );

	ITextureInternal *pTexture;

	if ( pRTName )
	{
		// caller is re-initing or changing
		pTexture = FindTexture( pRTName );

		if ( pTexture )
		{

			// Changing the underlying render target, but leaving the pointer and refcount
			// alone fixes callers that have exisiting references to this object.
			ITextureInternal::ChangeRenderTarget( pTexture, w, h, sizeMode, fmt, type, 
					textureFlags, renderTargetFlags );

#ifdef _PS3
			if ( pRTName[0] == '^' )
			{
				// Alias raw buffer
				pTexture->Ps3gcmRawBufferAlias( pRTName );
				return pTexture;
			}
#endif


			// download if ready
			pTexture->Download();
			return pTexture;
		}
	}
 
	pTexture = ITextureInternal::CreateRenderTarget( pRTName, w, h, sizeMode, fmt, type, 
											  textureFlags, renderTargetFlags, bMultipleTargets );
	if ( !pTexture )
		return NULL;

	// Add the render target to the list of textures
	// that way it'll get cleaned up correctly in case of a task switch
	m_TextureList.Insert( pTexture->GetName(), pTexture );

	// NOTE: This will download the texture only if the shader api is ready
#ifdef _PS3
	if ( pRTName && pRTName[0] == '^' )
	{
		// Alias raw buffer
		pTexture->Ps3gcmRawBufferAlias( pRTName );
	}
	else
#endif
	{
		pTexture->Download();
	}

	return pTexture;
}

void CTextureManager::ResetTextureFilteringState( )
{
	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		m_TextureList[i]->SetFilteringAndClampingMode();
	}
}


void CTextureManager::RemoveUnusedTextures( void )
{
	int iNext;
	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = iNext )
	{
		iNext = m_TextureList.Next( i );

#ifdef _DEBUG
		if ( m_TextureList[i]->GetReferenceCount() < 0 )
		{
			Warning( "RemoveUnusedTextures: pTexture->m_referenceCount < 0 for %s\n", m_TextureList[i]->GetName() );
		}
#endif
		if ( m_TextureList[i]->GetReferenceCount() <= 0 )
		{
			ITextureInternal::Destroy( m_TextureList[i] );
			m_TextureList.RemoveAt( i );
		}
	}
}

void CTextureManager::RemoveTexture( ITextureInternal *pTexture )
{
	Assert( pTexture->GetReferenceCount() <= 0 );

	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		// search by object
		if ( m_TextureList[i] == pTexture )
		{
			ITextureInternal::Destroy( m_TextureList[i] );
			m_TextureList.RemoveAt( i );
			break;
		}
	}
}

void CTextureManager::ReloadFilesInList( IFileList *pFilesToReload )
{
	if ( IsPC() )
	{
		for ( int i=m_TextureList.First(); i != m_TextureList.InvalidIndex(); i=m_TextureList.Next( i ) )
		{
			ITextureInternal *pTex = m_TextureList[i];

			pTex->ReloadFilesInList( pFilesToReload );
		}
	}
}

void CTextureManager::ReleaseTempRenderTargetBits( void )
{
	if( IsX360() ) //only sane on 360
	{
		int iNext;
		for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = iNext )
		{
			iNext = m_TextureList.Next( i );

			if ( m_TextureList[i]->IsTempRenderTarget() )
			{
				m_TextureList[i]->Release();
			}
		}
	}
}

void CTextureManager::DebugPrintUsedTextures( void )
{
	for ( int i = m_TextureList.First(); i != m_TextureList.InvalidIndex(); i = m_TextureList.Next( i ) )
	{
		ITextureInternal *pTexture;
		pTexture = m_TextureList[i];
		Msg( "Texture: '%s' RefCount: %d\n", pTexture->GetName(), pTexture->GetReferenceCount() );
	}

	if ( m_TextureExcludes.Count() )
	{
		Msg( "\nExcluded Textures: (%d)\n", m_TextureExcludes.Count() );
		for ( int i = m_TextureExcludes.First(); i != m_TextureExcludes.InvalidIndex(); i = m_TextureExcludes.Next( i ) )
		{
			char buff[256];
			const char *pName = m_TextureExcludes.GetElementName( i );
			V_snprintf( buff, sizeof( buff ), "Excluded: %d '%s' \n", m_TextureExcludes[i], pName );
	
			// an excluded texture is valid, but forced tiny
			if ( IsTextureLoaded( pName ) )
			{
				Msg( "%s\n", buff );
			}
			else
			{
				// warn as unknown, could be a spelling error
				Warning( "%s", buff );
			}
		}
	}
}

int CTextureManager::FindNext( int iIndex, ITextureInternal **pTexInternal )
{
	if ( iIndex == -1 && m_TextureList.Count() )
	{
		iIndex = m_TextureList.First();
	}
	else if ( !m_TextureList.Count() || !m_TextureList.IsValidIndex( iIndex ) )
	{
		*pTexInternal = NULL;
		return -1;
	}

	*pTexInternal = m_TextureList[iIndex];

	iIndex = m_TextureList.Next( iIndex );
	if ( iIndex == m_TextureList.InvalidIndex() )
	{
		// end of list
		iIndex = -1;
	}

	return iIndex;
}
