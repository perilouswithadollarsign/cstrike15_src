//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: Provide custom texture generation (compositing) 
//
// $NoKeywords: $
//=============================================================================//

#include "composite_texture.h"
#include "vstdlib/jobthread.h"
#include "materialsystem/base_visuals_data_processor.h"
#include "materialsystem_global.h"
#include "tier0/vprof.h"
#include "keyvalues.h"
#include "texturemanager.h"

//#define WRITE_OUT_VTF_PRE_COMPRESS
#ifdef WRITE_OUT_VTF_PRE_COMPRESS
#include "filesystem.h"
#endif

ConVar mat_verbose_texture_gen( "mat_verbose_texture_gen", "0" );

#define TEX_GEN_LOG( msg, ... ) \
	if ( mat_verbose_texture_gen.GetBool() ) \
	{ \
		Msg( "TextureGeneration: " msg, ##__VA_ARGS__ ); \
	} \
	
// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

int CCompositeTexture::m_nTextureCount = 0;

int s_nCompositeMaterialIndex = 0;

static ConVar *s_mat_picmip = NULL;

int GetMatPicMip()
{
	if ( !s_mat_picmip )
	{
		s_mat_picmip = g_pCVar->FindVar( "mat_picmip" );
	}

	return ( s_mat_picmip && s_mat_picmip->GetInt() > 0 ) ? s_mat_picmip->GetInt() : 0;
}

// this should match the CompositeTextureRTSizes_t enum in composite_texture.h
static SCompositeTextureRTData_t s_compositeTextureRTData[COMPOSITE_TEXTURE_RT_COUNT] =
{
	// these should be sorted in descending size order
#if !defined( PLATFORM_OSX )
	{ "_rt_CustomMaterial2048", 2048, true,  false, NULL },
#endif
	{ "_rt_CustomMaterial1024", 1024, true,  false, NULL },
	{ "_rt_CustomMaterial512",   512, true,  false, NULL },
	{ "_rt_CustomMaterial256",   256, false, false, NULL },
	{ "_rt_CustomMaterial128",   128, false, false, NULL }
};

int GetRTIndex( int nSize )
{
	int nClosestBigger = 0;
	for (int i = 0; i < COMPOSITE_TEXTURE_RT_COUNT; i++)
	{
		if ( s_compositeTextureRTData[i].nSize == nSize && s_compositeTextureRTData[i].bAvailable == true )
		{
			return i;
		}

		if ( s_compositeTextureRTData[i].nSize > nSize && s_compositeTextureRTData[i].bAvailable == true )
		{
			nClosestBigger = i;
		}
	}

	return nClosestBigger;
}

//-----------------------------------------------------------------------------
// Inherited from ITextureRegenerator
//-----------------------------------------------------------------------------
// If generation is complete then this will copy the results over into the texture
// pRect is ignored, this always does the whole texture
void CCompositeTextureResult::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
{
	TM_ZONE_FILTERED( TELEMETRY_LEVEL1, 200, TMZF_NONE, "%s", __FUNCTION__ );

	if ( m_pOwner && m_pOwner->GenerationComplete() )
	{
		TEX_GEN_LOG( "Finished generating texture %s... ", m_pOwner->GetName() );
		IVTFTexture *pResultVTF = m_pOwner->GetResultVTF();

		// we have already generated once, if we have results of an appropriate size, then copy them over, else signal regeneration
		if ( pResultVTF != NULL && ( pVTFTexture->Width() == ( m_pOwner->ActualSize() ) ) && ( pVTFTexture->Width() <= pResultVTF->Width() ) )
		{
			// handle the case where the destination texture is smaller than our result (can happen with mat_picmip change to values lower than we have RTs for)
			int nMipOffset = 0;
			if ( pVTFTexture->Width() < pResultVTF->Width() )
			{
				int nSourceSize = pResultVTF->Width();
				while ( nSourceSize > pVTFTexture->Width() )
				{
					nMipOffset++;
					nSourceSize >>= 1;
				}
			}

			// copy each mip over
			for (int nMip = 0; nMip < pResultVTF->MipCount(); nMip++)
			{
				unsigned char *pBuffer = pResultVTF->ImageData( 0, 0, nMip + nMipOffset );
				unsigned char *pImageData = pVTFTexture->ImageData( 0, 0, nMip );
				// this amounts to a memcpy, but deals with properly calculating the size for compressed textures
				ImageLoader::ConvertImageFormat( pBuffer, pResultVTF->Format(), pImageData, pVTFTexture->Format(), pResultVTF->Width() >> ( nMip + nMipOffset ), pResultVTF->Height() >> ( nMip + nMipOffset ) );
			}

			TEX_GEN_LOG( "SUCCESS.\n" );
		}
		else
		{
			TEX_GEN_LOG( "FAILURE: resulting vtf size missmatch, forcing regenerate.\n" );
			m_pOwner->ForceRegenerate();
		}
	}
}

void CCompositeTextureResult::Release()
{
	if ( m_pTexture != NULL )
	{
		m_pTexture->SetTextureRegenerator( NULL, false );
		m_pTexture->DecrementReferenceCount();
		m_pTexture->DeleteIfUnreferenced();
		m_pTexture = NULL;
	}
	m_pOwner = NULL;
}

//
// Composite Texture - used to make a custom texture using a Shader that can be used with custom materials
//

CCompositeTexture::CCompositeTexture( const CUtlBuffer &compareBlob, KeyValues *pCompositingMaterialKeyValues, CompositeTextureSize_t size, CompositeTextureFormat_t format, int nMaterialParamNameId, bool bSRGB, bool bIgnorePicMip )
	: m_size( size )
	, m_format( format )
	, m_nMaterialParamNameId( nMaterialParamNameId )
	, m_bSRGB( bSRGB )
	, m_nRegenerateStage( COMPOSITE_TEXTURE_STATE_NOT_STARTED )
	, m_pResultVTF( NULL )
	, m_ResultTexture( this )
	, m_bNeedsRegenerate( false )
	, m_bNeedsFinalize( true )
	, m_pScratchVTF( NULL )
	, m_pCustomMaterialRT( NULL )
	, m_pCompositingMaterial( NULL )
	, m_nLastFrameCount( 0 )
	, m_pPixelsReadEvent( NULL )
	, m_bIgnorePicMip( bIgnorePicMip )
{
	for ( int i = 0; i < NUM_PRELOAD_TEXTURES; i++ )
	{
		m_pPreLoadTextures[ i ] = NULL;
	}

	m_compareBlob.CopyBuffer( compareBlob );

	// we need to copy this, because the passed in one was allocated outside materialsystem.dll
	m_pCompositingMaterialKeyValues = pCompositingMaterialKeyValues->MakeCopy();

	m_nActualSize = ( 1 << Size() ) >> ( m_bIgnorePicMip ? 0 : GetMatPicMip() );
	V_sprintf_safe( m_szTextureName, "composite_texture_%d_%d_%d_%d", m_nMaterialParamNameId, m_nActualSize, m_format, m_nTextureCount++ );

}

bool CCompositeTexture::Init()
{
	int nRT = GetRTIndex( m_nActualSize );

	m_pCustomMaterialRT = materials->FindTexture( s_compositeTextureRTData[nRT].pName, TEXTURE_GROUP_RENDER_TARGET );
 
	if ( !m_pCustomMaterialRT->IsError() )
	{
		m_pCustomMaterialRT->AddRef();
		m_pScratchVTF = s_compositeTextureRTData[nRT].pScratch;
	}
	else
	{
		// release the error texture
		m_pCustomMaterialRT->Release();
		m_pCustomMaterialRT = NULL;

		Warning( "Error creating composite tex8ture! Could not find render target texture. \n" );
		m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_COMPLETE;
		return false;
	}

	// it's possible for the result vtf to exist already in the case of regeneration, so we clean up the old one before making a new one
	if ( m_pResultVTF != NULL )
	{
		DestroyVTFTexture( m_pResultVTF );
		m_pResultVTF = NULL;
	}

	{
		TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "CreateResultVTF" );
		// create the final result VTF that we'll keep around. It's compressed and used to copy over to the actual texture whenever the texture needs downloading
		m_pResultVTF = CreateVTFTexture();
		m_pResultVTF->Init( m_pScratchVTF->Width(), m_pScratchVTF->Height(), m_pScratchVTF->Depth(), ( Format() == COMPOSITE_TEXTURE_FORMAT_DXT1 ) ? IMAGE_FORMAT_DXT1_RUNTIME : IMAGE_FORMAT_DXT5_RUNTIME, TEXTUREFLAGS_ALL_MIPS, 1, m_pScratchVTF->MipCount() );
	}

	return true;
}

CCompositeTexture::~CCompositeTexture()
{
	ReleasePreloadedTextures();
	if ( m_pCompositingMaterial != NULL )
	{
		m_pCompositingMaterial->DecrementReferenceCount();
		m_pCompositingMaterial->DeleteIfUnreferenced();
		m_pCompositingMaterial = NULL;
	}

	if ( m_pCompositingMaterialKeyValues != NULL )
	{
		m_pCompositingMaterialKeyValues->deleteThis();
		m_pCompositingMaterialKeyValues = NULL;
	}

	m_ResultTexture.Release();

	if ( m_pResultVTF != NULL )
	{
		DestroyVTFTexture(m_pResultVTF);
		m_pResultVTF = NULL;
	}

	m_pScratchVTF = NULL;

	if ( m_pCustomMaterialRT != NULL )
	{
		m_pCustomMaterialRT->Release();
		m_pCustomMaterialRT = NULL;
	}

	if ( m_pPixelsReadEvent != NULL )
	{
		delete m_pPixelsReadEvent;
		m_pPixelsReadEvent = NULL;
	}
}

void CCompositeTexture::ReleasePreloadedTextures()
{
	for ( int i = 0; i < NUM_PRELOAD_TEXTURES; i++ )
	{
		if ( m_pPreLoadTextures[ i ] != NULL )
		{
			m_pPreLoadTextures[ i ]->DecrementReferenceCount();
			m_pPreLoadTextures[ i ]->DeleteIfUnreferenced();
			m_pPreLoadTextures[ i ] = NULL;
		}
	}
}

// this is called from the GenerationStep which is called from the generate thread
void CCompositeTexture::GenerateComposite( void )
{
	TM_ZONE_FILTERED( TELEMETRY_LEVEL1, 100, TMZF_NONE, "GenerateComposite" );

	if ( m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_NOT_STARTED )
	{
		TEX_GEN_LOG( "Loading texture for %s\n", GetName() );
		m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_ASYNC_TEXTURE_LOAD;
	}
	else if ( m_pScratchVTF != NULL )
	{
		if ( m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_COPY_TO_VTF_COMPLETE )
		{
			// no longer need the RT
			if ( m_pCustomMaterialRT != NULL )
			{
				m_pCustomMaterialRT->Release();
				m_pCustomMaterialRT = NULL;
			}
			
			{
				TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "MipGen" );
				m_pScratchVTF->GenerateMipmaps();
			}

#ifdef WRITE_OUT_VTF_PRE_COMPRESS
			CUtlBuffer buf;
			m_pScratchVTF->Serialize(buf);

			char szTextureName[MAX_PATH];
			V_snprintf(szTextureName, MAX_PATH, "d:/temp/%s.vtf", m_szTextureName );
			
			FileHandle_t f = g_pFullFileSystem->Open(szTextureName, "wb", NULL);

			if ( f != FILESYSTEM_INVALID_HANDLE )
			{
				g_pFullFileSystem->Write( buf.Base(), buf.TellMaxPut(), f );
				g_pFullFileSystem->Close(f);
			}
#endif

			{
				TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "DXT Compress" );
				// compress and copy the Scratch VTF mip maps to the Result VTF
				for (int nMip = 0; nMip < m_pScratchVTF->MipCount(); nMip++)
				{
					unsigned char *pResultImageData = m_pResultVTF->ImageData( 0, 0, nMip );
					unsigned char *pScratchImageData = m_pScratchVTF->ImageData( 0, 0, nMip );
					ImageLoader::ConvertImageFormat( pScratchImageData, IMAGE_FORMAT_RGBA8888, pResultImageData, ( Format() == COMPOSITE_TEXTURE_FORMAT_DXT1 ) ? IMAGE_FORMAT_DXT1_RUNTIME : IMAGE_FORMAT_DXT5_RUNTIME, m_pResultVTF->Width() >> nMip, m_pResultVTF->Height() >> nMip );
				}
			}

			// done with the scratch VTF
			m_pScratchVTF = NULL;

			m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_WAITING_FOR_MATERIAL_CLEANUP;
		}
	}
}


// This does one generation step. It's called from the generate thread
void CCompositeTexture::GenerationStep( void )
{
	if ( m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_REQUESTED_READ )
	{
		TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "ReadPixelsEventWait" );
		// wait on ReadPixelsAsync() to signal up that it's completed
		if ( m_pPixelsReadEvent->Wait( 20 ) )
		{
			m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_WAITING_FOR_GETRESULT;
		}
	}
	if ( m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_REQUESTED_GETRESULT )
	{
		TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "ReadPixelsGetResultEventWait" );
		// wait on ReadPixelsAsyncGetResult() to signal up that it's completed
		if ( m_pPixelsReadEvent->Wait( 20 ) )
		{
			delete m_pPixelsReadEvent;
			m_pPixelsReadEvent = NULL;

			m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_COPY_TO_VTF_COMPLETE;
		}
	}
	if ( m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_WAITING_FOR_ASYNC_TEXTURE_LOAD_FINISH )
	{
		TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "WaitForAsyncTextureLoads" );
		bool bDone = true;
		for ( int i = 0; i < NUM_PRELOAD_TEXTURES; i++ )
		{
			if ( ( m_pPreLoadTextures[ i ] != NULL ) && !m_pPreLoadTextures[ i ]->IsAsyncDone() )
			{
				bDone = false;
				break;
			}
		}
		if ( bDone )
		{
			m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_NEEDS_INIT;
		}
	}

	GenerateComposite();
}

void CCompositeTexture::ForceRegenerate( void )
{
	// we need to recalculate m_nActualSize
	m_nActualSize = ( 1 << Size() ) >> ( m_bIgnorePicMip ? 0 : GetMatPicMip() );

	m_bNeedsRegenerate = true;
}

struct materialTextureParams
{
	const char *m_pParamName;
	const char *m_pDefaultTexture;
};

void CCompositeTexture::DoAsyncTextureLoad( void )
{
	TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "DoAsyncTextureLoad" );

	materialTextureParams materialParams[ NUM_PRELOAD_TEXTURES ] =
	{
		// used by weapons
		{ "$basetexture", nullptr },
		{ "$exptexture", nullptr },
		{ "$painttexture", nullptr },
		{ "$maskstexture", nullptr },
		{ "$aotexture", nullptr },
		{ "$postexture", nullptr },
		{ "$surfacetexture", nullptr },

		// used by gloves (character / custom_character shaders)
		{ "$bumpmap", nullptr },
		{ "$masks1", nullptr },
		{ "$phongwarptexture", nullptr },
		{ "$fresnelrangestexture", nullptr },
		{ "$masks2", nullptr },
		{ "$envmap", nullptr },
		{ "$materialmask", nullptr },
		{ "$ao", nullptr },
		{ "$grunge", nullptr },
		{ "$detail", nullptr },
		{ "$detailnormal", nullptr },
		{ "$pattern", nullptr },
		{ "$noise", "models/weapons/customization/materials/noise" } // default set in custom character shader
	};

	//KeyValuesDumpAsDevMsg( m_pCompositingMaterialKeyValues, 1, 1 );

	// initiate async load of the textures needed for the compositing material
	for ( int i = 0; i < NUM_PRELOAD_TEXTURES; i++ )
	{
		const char *pszTexture = m_pCompositingMaterialKeyValues->GetString( materialParams[ i ].m_pParamName );
		if ( !( pszTexture && pszTexture[0] != 0 ) && materialParams[i].m_pDefaultTexture )
		{
			pszTexture = materialParams[i].m_pDefaultTexture;
		}

		if ( pszTexture && pszTexture[0] != 0 )
		{
			m_pPreLoadTextures[ i ] = TextureManager()->FindOrLoadTexture( pszTexture, TEXTURE_GROUP_COMPOSITE, TEXTUREFLAGS_ASYNC_DOWNLOAD );
			if ( !m_pPreLoadTextures[ i ]->IsError() )
			{
				m_pPreLoadTextures[ i ]->AddRef();
			}
			else
			{
				m_pPreLoadTextures[ i ] = NULL;
				DevMsg( "DoAsyncTextureLoad: Failed to preload texture: %s (%s)\n", pszTexture, materialParams[ i ].m_pParamName );
			}
		}
	}

	m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_WAITING_FOR_ASYNC_TEXTURE_LOAD_FINISH;
}

void CCompositeTexture::CreateCompositingMaterial( void )
{
	{
		TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "CreateCompositingMat" );

		char szCompositeMaterialName[32];
		V_sprintf_safe( szCompositeMaterialName, "compositing_material_%d", s_nCompositeMaterialIndex++ );
		m_pCompositingMaterial = materials->CreateMaterial( szCompositeMaterialName, m_pCompositingMaterialKeyValues->MakeCopy() );

		if ( m_pCompositingMaterial && m_pCompositingMaterial->IsErrorMaterial() )
		{
			// release the error material
			m_pCompositingMaterial->Release();

			// we do not call DeleteIfUnreferenced here because it's the error material
			m_pCompositingMaterial = NULL;

			Warning( "Error creating compositing material! Marking composite complete and not rendering to RT...\n" );
			m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_COMPLETE;
			return;
		}
	}

	{
		TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "RefreshCompositingMat" );
		// this causes a precache of the material
		m_pCompositingMaterial->Refresh();
	}

	m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_WAITING_FOR_RENDER_TO_RT;
}

void CCompositeTexture::RenderToRT( void )
{
	if ( m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_WAITING_FOR_RENDER_TO_RT )
	{
		if ( materials->CanDownloadTextures() )
		{
			int resolutionX = m_pCustomMaterialRT->GetActualWidth();
			int resolutionY = m_pCustomMaterialRT->GetActualHeight();

			TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "RenderToRT %d", resolutionX );

			//init render context and set the the custom weapon RT as the current target
			CMatRenderContextPtr pRenderContext( materials );
	
			pRenderContext->PushRenderTargetAndViewport( m_pCustomMaterialRT, 0, 0, resolutionX, resolutionY );

			//render a quad using the composite material to the custom weapon RT
			pRenderContext->DrawScreenSpaceRectangle( m_pCompositingMaterial, 0, 0, resolutionX, resolutionY,	0, 0, resolutionX - 1, resolutionY - 1, resolutionX, resolutionY );

			pRenderContext->PopRenderTargetAndViewport();

			m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_RENDERED_TO_RT;
		}
	}
}

// this is just to have two frames between rendering and attempting to read back.
void CCompositeTexture::AdvanceToReadRT( void )
{
	if ( m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_RENDERED_TO_RT )
	{
		m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_WAITING_FOR_READ_RT;
	}
}

void CCompositeTexture::ReadRT( void )
{
	if ( m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_WAITING_FOR_READ_RT )
	{
		TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "ReadRT" );
		if ( materials->CanDownloadTextures() )
		{
			int resolutionX = m_pCustomMaterialRT->GetActualWidth();
			int resolutionY = m_pCustomMaterialRT->GetActualHeight();
			unsigned char *pDestImage = m_pScratchVTF->ImageData( 0, 0, 0 );

			// queue up read pixels
			CMatRenderContextPtr pRenderContext( materials );
			m_pPixelsReadEvent = new CThreadEvent();
			pRenderContext->ReadPixelsAsync( 0, 0, resolutionX, resolutionY, pDestImage, IMAGE_FORMAT_RGBA8888, m_pCustomMaterialRT, m_pPixelsReadEvent );

			m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_REQUESTED_READ;
		}
	}
}

void CCompositeTexture::GetReadRTResult( void )
{
	if ( m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_WAITING_FOR_GETRESULT )
	{
		TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "GetReadRTResult" );
		if ( materials->CanDownloadTextures() )
		{
			int resolutionX = m_pCustomMaterialRT->GetActualWidth();
			int resolutionY = m_pCustomMaterialRT->GetActualHeight();
			unsigned char *pDestImage = m_pScratchVTF->ImageData( 0, 0, 0 );

			// queue up read pixels
			CMatRenderContextPtr pRenderContext( materials );
			m_pPixelsReadEvent->Reset();
			pRenderContext->ReadPixelsAsyncGetResult( 0, 0, resolutionX, resolutionY, pDestImage, IMAGE_FORMAT_RGBA8888, m_pPixelsReadEvent );

			m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_REQUESTED_GETRESULT;
		}
	}
}

void CCompositeTexture::CleanupCompositingMaterial( void )
{
	if ( m_nRegenerateStage == COMPOSITE_TEXTURE_STATE_WAITING_FOR_MATERIAL_CLEANUP )
	{
		if ( m_pCompositingMaterial != NULL )
		{
			TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "CleanupCompositingMat" );
			m_pCompositingMaterial->DecrementReferenceCount();
			m_pCompositingMaterial->DeleteIfUnreferenced();
			m_pCompositingMaterial = NULL;
		}
		ReleasePreloadedTextures(); // no longer need these

		m_nRegenerateStage = COMPOSITE_TEXTURE_STATE_COMPLETE;
	}
}

void CCompositeTexture::Usage( int &nTextures, int &nBackingTextures )
{
	nTextures += ( m_ResultTexture.m_pTexture != NULL ) ? m_ResultTexture.m_pTexture->GetApproximateVidMemBytes() : 0;
	nBackingTextures += ( m_pResultVTF != NULL ) ? m_pResultVTF->ComputeTotalSize() : 0;
}

void CCompositeTexture::Finalize()
{
	TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "Finalize" );

	if ( m_ResultTexture.m_pTexture == NULL )
	{
		m_ResultTexture.m_pTexture = materials->CreateProceduralTexture( m_szTextureName, TEXTURE_GROUP_COMPOSITE, ( 1 << Size() ), ( 1 << Size() ), 
																		 ( Format() == COMPOSITE_TEXTURE_FORMAT_DXT5 ) ? IMAGE_FORMAT_DXT5_RUNTIME : IMAGE_FORMAT_DXT1_RUNTIME, 
																		 TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_ANISOTROPIC | ( ( m_bSRGB ) ? TEXTUREFLAGS_SRGB : 0 ) | TEXTUREFLAGS_SKIP_INITIAL_DOWNLOAD );
		m_ResultTexture.m_pTexture->SetTextureRegenerator( &m_ResultTexture );
	}

	m_ResultTexture.m_pTexture->Download();
	m_bNeedsFinalize = false;

#if defined( DX_TO_GL_ABSTRACTION )
	// Free VTF, significant mem saving - after Downloand not used for anything other than CWeaponCSBase::SaveCustomMaterialsTextures()
	// forceregenerate ensures this is safe on loss of focus - see CCompositeTextureResult::RegenerateTextureBits()
	if ( m_pResultVTF != NULL )
	{
		DestroyVTFTexture( m_pResultVTF );
		m_pResultVTF = NULL;
	}
#endif
}

bool CCompositeTexture::Compare( const SCompositeTextureInfo &textureInfo )
{
	if (Size() == textureInfo.m_size &&
		Format() == textureInfo.m_format &&
		GetMaterialParamNameId() == textureInfo.m_nMaterialParamID &&
		IsSRGB() == textureInfo.m_bSRGB &&
		textureInfo.m_pVisualsDataProcessor->GetCompareObject()->Compare( GetVisualsDataCompareBlob() ) )
	{
		return true;
	}

	return false;
}


//
// global weapon material generator
//  the game uses this to make/get a custom material for a weapon
//

CCompositeTextureGenerator::CCompositeTextureGenerator( void )
	: m_bGenerateThreadExit( false )
	, m_hGenerateThread( NULL )
	, m_pGunGrimeTexture( NULL )
	, m_pPaintWearTexture( NULL )
{
#ifndef DEDICATED
	m_pCompositeTextures.EnsureCapacity( 256 );
	m_pPendingCompositeTextures.EnsureCapacity( 256 );
#endif
}

CCompositeTextureGenerator::~CCompositeTextureGenerator()
{	
}

// this is called at the end of each frame
bool ProcessCompositeTextureGenerator( void )
{
	return MaterialSystem()->GetCompositeTextureGenerator()->Process();
}

// this handles doing rendering and material refreshes for the regenerators, downloads textures and flags materials ready so they will draw,
// triggers regenerations as needed, and handles swapping materials that are pending swap and ready, and cleans up materials that are no longer used
bool CCompositeTextureGenerator::Process( void )
{
	TM_ZONE( TELEMETRY_LEVEL1, TMZF_NONE, "%s", __FUNCTION__ );

	bool bDidWork = false;

	for ( int i = m_pPendingCompositeTextures.Count() - 1; i >= 0; i-- )
	{
		CCompositeTexture *pTexture = m_pPendingCompositeTextures[ i ];
		if ( pTexture && !pTexture->IsReady() )
		{
			if ( pTexture->GenerationComplete() && pTexture->NeedsFinalize() )
			{
				pTexture->Finalize();
				// move from pending to final list
				m_pCompositeTextures.AddToTail( pTexture );
				m_pPendingCompositeTextures.Remove( i );
				break;
			}
			else if ( pTexture->NeedsAsyncTextureLoad() )
			{
				pTexture->DoAsyncTextureLoad();
				break;
			}
			else if ( pTexture->NeedsCompositingMaterial() )
			{
				pTexture->CreateCompositingMaterial();
				bDidWork = true;
				break;
			}
			else if ( pTexture->NeedsMaterialCleanup() )
			{
				pTexture->CleanupCompositingMaterial();
				break;
			}
			else if ( pTexture->NeedsRender() )
			{
				pTexture->RenderToRT();
				bDidWork = true;
				break;
			}
			else if ( pTexture->HasRendered() )
			{
				pTexture->AdvanceToReadRT();
				break;
			}
			else if ( pTexture->NeedsReadRT() )
			{
				pTexture->ReadRT();
				break;
			}
			else if ( pTexture->NeedsGetResult() )
			{
				pTexture->GetReadRTResult();
				bDidWork = true;
				break;
			}
		}
	}

	for ( int i = m_pCompositeTextures.Count() - 1; i >= 0; i-- )
	{
		CCompositeTexture *pTexture = m_pCompositeTextures[ i ];
		if ( pTexture )
		{
			// see if a material needs regeneration (it must have completed generation)
			if ( pTexture->GenerationComplete() && pTexture->NeedsRegenerate() )
			{
				// signal the regeneration, and set the material to not draw
				pTexture->Refresh();
				pTexture->Init();

				// move back to the pending list
				m_pPendingCompositeTextures.AddToTail( pTexture );
				m_pCompositeTextures.Remove( i );
				
				// add material to generation queue
				m_GenerateQueueMutex.Lock();
				m_pGenerateQueue.AddToTail( pTexture );
				m_GenerateQueueMutex.Unlock();
			}
			// clean up materials that are no longer used (we are the only reference)
			else if ( pTexture->ShouldRelease() )
			{
				pTexture->Release();
				m_pCompositeTextures.Remove( i );
			}
		}
	}

	return bDidWork;
}

bool CCompositeTextureGenerator::Init( void )
{
#ifndef DEDICATED
	for (int i = 0; i < COMPOSITE_TEXTURE_RT_COUNT; i++)
	{
		ITexture *pRT = MaterialSystem()->CreateNamedRenderTargetTextureEx2( s_compositeTextureRTData[i].pName, s_compositeTextureRTData[i].nSize, s_compositeTextureRTData[i].nSize, RT_SIZE_NO_CHANGE, 
																			IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_NONE, TEXTUREFLAGS_SINGLECOPY | ( ( s_compositeTextureRTData[i].bSRGB ) ? TEXTUREFLAGS_SRGB : 0 ) );
		if ( pRT->IsError() )
		{
			pRT->Release();
			pRT = NULL;
		}

		if ( pRT == NULL )
		{
			Warning( "Failed to create %d x %d render target for composite texturing!\n", s_compositeTextureRTData[i].nSize, s_compositeTextureRTData[i].nSize );
		}
		else
		{
			m_CompositeTextureManagerRTs[i].Init( pRT );
			s_compositeTextureRTData[i].bAvailable = true;

			s_compositeTextureRTData[i].pScratch = CreateVTFTexture();
			s_compositeTextureRTData[i].pScratch->Init( s_compositeTextureRTData[i].nSize, s_compositeTextureRTData[i].nSize, 1, IMAGE_FORMAT_RGBA8888, TEXTUREFLAGS_ALL_MIPS, 1 );
		}
	}

	// precache scratch and grime textures
	m_pGunGrimeTexture = TextureManager()->FindOrLoadTexture( "models/weapons/customization/shared/gun_grunge.vtf", TEXTURE_GROUP_COMPOSITE );
	if ( !m_pGunGrimeTexture->IsError() )
	{
		m_pGunGrimeTexture->AddRef();
	}
	else
	{
		m_pGunGrimeTexture = NULL;
	}
	m_pPaintWearTexture = TextureManager()->FindOrLoadTexture( "models/weapons/customization/shared/paint_wear.vtf", TEXTURE_GROUP_COMPOSITE );
	if ( !m_pPaintWearTexture->IsError() )
	{
		m_pPaintWearTexture->AddRef();
	}
	else
	{
		m_pPaintWearTexture = NULL;
	}

	MaterialSystem()->AddEndFramePriorToNextContextFunc( ::ProcessCompositeTextureGenerator );
	CreateGenerateThread();
#endif
	return true;
}

void CCompositeTextureGenerator::Shutdown( void )
{
#ifndef DEDICATED
	DestroyGenerateThread();
	MaterialSystem()->RemoveEndFramePriorToNextContextFunc( ::ProcessCompositeTextureGenerator );
	DestroyTextures();
	for (int i = 0; i < COMPOSITE_TEXTURE_RT_COUNT; i++)
	{
		if ( s_compositeTextureRTData[i].bAvailable )
		{
			s_compositeTextureRTData[i].bAvailable = false;
			if ( s_compositeTextureRTData[i].pScratch != NULL )
			{
				DestroyVTFTexture( s_compositeTextureRTData[i].pScratch );
				s_compositeTextureRTData[i].pScratch = NULL;
			}
			m_CompositeTextureManagerRTs[i].Shutdown();
		}
	}

	if ( m_pPaintWearTexture )
	{
		m_pPaintWearTexture->DecrementReferenceCount();
		m_pPaintWearTexture->DeleteIfUnreferenced();
		m_pPaintWearTexture = NULL;
	}
	if ( m_pGunGrimeTexture )
	{
		m_pGunGrimeTexture->DecrementReferenceCount();
		m_pGunGrimeTexture->DeleteIfUnreferenced();
		m_pGunGrimeTexture = NULL;
	}
#endif
}

ICompositeTexture *CCompositeTextureGenerator::GetCompositeTexture( const SCompositeTextureInfo &textureInfo, bool bIgnorePicMip, bool bAllowCreate )
{
	return GetCompositeTexture( textureInfo.m_pVisualsDataProcessor, textureInfo.m_nMaterialParamID, textureInfo.m_size, textureInfo.m_format, textureInfo.m_bSRGB, bIgnorePicMip, bAllowCreate );
}

ICompositeTexture *CCompositeTextureGenerator::GetCompositeTexture( IVisualsDataProcessor *pVisualsDataProcessor, int nMaterialParamNameId, CompositeTextureSize_t size, CompositeTextureFormat_t format, bool bSRGB, bool bIgnorePicMip, bool bAllowCreate )
{
#if defined( DEDICATED ) || defined( DISABLE_CUSTOM_MATERIAL_GENERATION )
	return NULL;
#endif

	if ( !pVisualsDataProcessor->HasCustomMaterial() )
	{
		return NULL;
	}

	CCompositeTexture *pTexture = NULL;
	
	// Look for an existing material match
	for ( int i = 0; i < m_pCompositeTextures.Count(); ++i )
	{
		CCompositeTexture *pCompareTexture = m_pCompositeTextures[ i ];
		if ( pCompareTexture->Format() == format && 
			 pCompareTexture->Size() == size &&
			 pCompareTexture->GetMaterialParamNameId() == nMaterialParamNameId &&
			 pCompareTexture->IsSRGB() == bSRGB &&
			 pVisualsDataProcessor->GetCompareObject()->Compare( pCompareTexture->GetVisualsDataCompareBlob() ) )
		{
			pTexture = m_pCompositeTextures[ i ];
			break;
		}
	}
	for ( int i = 0; i < m_pPendingCompositeTextures.Count(); ++i )
	{
		CCompositeTexture *pCompareTexture = m_pPendingCompositeTextures[ i ];
		if ( pCompareTexture->Format() == format && 
			 pCompareTexture->Size() == size &&
			 pCompareTexture->GetMaterialParamNameId() == nMaterialParamNameId &&
			 pCompareTexture->IsSRGB() == bSRGB &&
			 pVisualsDataProcessor->GetCompareObject()->Compare( pCompareTexture->GetVisualsDataCompareBlob() ) )
		{
			pTexture = m_pPendingCompositeTextures[ i ];
			break;
		}
	}

	if ( !pTexture && bAllowCreate )
	{
		KeyValues *pCompositeMaterialKeyValues = pVisualsDataProcessor->GenerateCompositeMaterialKeyValues( nMaterialParamNameId );
		if ( !pCompositeMaterialKeyValues )
		{
			return NULL;
		}
		pTexture = new CCompositeTexture( pVisualsDataProcessor->GetCompareObject()->GetCompareBlob(), pCompositeMaterialKeyValues, size, format, nMaterialParamNameId, bSRGB, bIgnorePicMip );
		pCompositeMaterialKeyValues->deleteThis(); // copied inside CCompositeTexture(), no longer needed

		if ( pTexture->Init() )
		{
			m_pPendingCompositeTextures.AddToTail( pTexture );

			// add texture to generation queue (handled in the generation thread)
			m_GenerateQueueMutex.Lock();
			m_pGenerateQueue.AddToHead( pTexture );
			m_GenerateQueueMutex.Unlock();
		}
		else
		{
			pTexture->Release();
			pTexture = NULL;
		}
	}

	// The texture may not be complete yet, but it will be completed over the next few frames via Process()
	return pTexture;
}

bool CCompositeTextureGenerator::ForceRegenerate( ICompositeTexture *pTextureInterface )
{
	CCompositeTexture *pTexture = dynamic_cast< CCompositeTexture * >( pTextureInterface );
	if ( !pTexture )
	{
		return false;
	}

	int nTexture = m_pCompositeTextures.Find( pTexture );
	if ( nTexture != -1 )
	{
		// move back to the pending list
		m_pPendingCompositeTextures.AddToTail( pTexture );
		m_pCompositeTextures.Remove( nTexture );
	}
	else if ( m_pPendingCompositeTextures.Find( pTexture ) == -1 )
	{
		// texture isn't in either list, so we can't regenerate it
		return false;
	}

	pTexture->Refresh();

	// add texture to generation queue (handled in the generation thread)
	m_GenerateQueueMutex.Lock();
	m_pGenerateQueue.AddToTail( pTexture );
	m_GenerateQueueMutex.Unlock();

	return true;
}

void CCompositeTextureGenerator::DestroyTextures( void )
{
	for ( int i = 0; i < m_pCompositeTextures.Count(); ++i )
	{
		CCompositeTexture *pTexture = m_pCompositeTextures[ i ];
		if ( pTexture )
		{
			pTexture->ReleaseResult();
			pTexture->Release();
		}
	}
	m_pCompositeTextures.RemoveAll();

	for ( int i = 0; i < m_pPendingCompositeTextures.Count(); ++i )
	{
		CCompositeTexture *pTexture = m_pPendingCompositeTextures[ i ];
		if ( pTexture )
		{
			pTexture->ReleaseResult();
			pTexture->Release();
		}
	}
	m_pPendingCompositeTextures.RemoveAll();
}

void CCompositeTextureGenerator::CreateGenerateThread()
{
	// kick off a thread to do custom texture generation
	m_bGenerateThreadExit = false;
	m_hGenerateThread = ThreadExecuteSolo( "CompositeTextureGenerationThread", this, &CCompositeTextureGenerator::GenerateThread );
}

void CCompositeTextureGenerator::DestroyGenerateThread()
{
	if ( m_hGenerateThread )
	{
		// remove all the queued materials from the generate queue
		m_GenerateQueueMutex.Lock();
		m_pGenerateQueue.RemoveAll();
		m_GenerateQueueMutex.Unlock();

		m_bGenerateThreadExit = true;
		ThreadJoin( m_hGenerateThread );
		ReleaseThreadHandle( m_hGenerateThread );
		m_hGenerateThread = NULL;
	}
}

void CCompositeTextureGenerator::GenerateThread()
{
	while ( !m_bGenerateThreadExit )
	{
		CCompositeTexture *pTexture = NULL;

		// pull a material that needs generation off the queue (if any exist)
		m_GenerateQueueMutex.Lock();
		if ( m_pGenerateQueue.Count() > 0 )
		{
			pTexture = m_pGenerateQueue.Element( m_pGenerateQueue.Head() );
			m_pGenerateQueue.Remove( m_pGenerateQueue.Head() );
		}
		m_GenerateQueueMutex.Unlock();

		if ( pTexture )
		{
			while ( !m_bGenerateThreadExit && !pTexture->GenerationComplete() )
			{
				pTexture->GenerationStep();

				ThreadSleep( 1 );
			}

			// wait for the current texture to finalize (in the main thread) before going to the next one
			while ( !m_bGenerateThreadExit && pTexture->NeedsFinalize() && pTexture->GenerationComplete() )
			{
				ThreadSleep( 1 );
			}
		}
		else
		{
			ThreadSleep( 1 );
		}
	}

	m_GenerateQueueMutex.Lock();
	m_pGenerateQueue.RemoveAll();
	m_GenerateQueueMutex.Unlock();
}
