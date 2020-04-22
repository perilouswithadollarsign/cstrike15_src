//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//


#include "gamelayer.h"
#include "gamerect.h"
#include "tier1/keyvalues.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "tier1/utlbuffer.h"
#include "tier2/tier2.h"
#include "filesystem.h"
#include "rendersystem/irendercontext.h"
#include "tier2/fileutils.h"
#include "gameuisystemsurface.h"
#include "gameuidefinition.h"
#include "tier1/fmtstr.h"

#include "gamerect.h"
#include "gametext.h"
#include "hitarea.h"
#include "graphicgroup.h"
#include "dynamicrect.h"
#include "gameuiscript.h"
#include "gameuisystemmgr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DMXELEMENT_UNPACK ( CGameLayer ) 
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "name", "", m_pName ) 
END_DMXELEMENT_UNPACK( CGameLayer, s_GameLayerUnpack )

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CGameLayer::CGameLayer( SublayerTypes_t layerType )
{
	m_pName = "";
	m_LayerType = layerType;
	m_pTextureName = "";
	m_hTexture = RENDER_TEXTURE_HANDLE_INVALID;
	m_Material = NULL;
	m_Sheet = NULL;
	m_SheetSymbol = UTL_INVAL_SYMBOL;
	m_bSheetSymbolCached = false;
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CGameLayer::~CGameLayer()
{
	if ( m_Material.IsValid() )
	{
		m_Material.Shutdown( true );
	}	
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameLayer::Shutdown()
{
	int nGraphicsCount = m_LayerGraphics.Count();
	for ( int j = 0; j < nGraphicsCount; ++j )
	{
		Assert( !m_LayerGraphics[j]->IsGroup() );
		delete m_LayerGraphics[j];
		m_LayerGraphics[j] = NULL;
	}
	m_LayerGraphics.RemoveAll();
}

//-----------------------------------------------------------------------------
//	Load data from dmx file
//-----------------------------------------------------------------------------
bool CGameLayer::Unserialize( CDmxElement *pLayer, CUtlDict< CGameGraphic *, int > &unserializedGraphicMapping )
{
	pLayer->UnpackIntoStructure( this, s_GameLayerUnpack );

	// Static graphics
	if ( m_LayerType == SUBLAYER_STATIC )
	{
		CDmxAttribute *pGraphics = pLayer->GetAttribute( "staticGraphics" );
		const CUtlVector< CDmxElement * > &graphics = pGraphics->GetArray< CDmxElement * >( );
		int nCount = graphics.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			// What kind of graphic is this?
			const char *pType = graphics[i]->GetValueString( "classtype" );

			if ( !Q_stricmp( pType, "CDmeRectGeometry" ) )
			{
				CGameRect *pGraphic = new CGameRect( "" );
				if ( !pGraphic->Unserialize( graphics[i] ) )
				{
					delete pGraphic;
					return false;
				}
				m_LayerGraphics.AddToTail( pGraphic );
				char pBuf[255];
				UniqueIdToString( graphics[i]->GetId(), pBuf, 255 );
				unserializedGraphicMapping.Insert( pBuf, pGraphic );
			}
			else if ( !Q_stricmp( pType, "CDmeHitAreaGeometry" ) )
			{
				CHitArea *pGraphic = new CHitArea( "" );
				if ( !pGraphic->Unserialize( graphics[i] ) )
				{
					delete pGraphic;
					return false;
				}
				m_LayerGraphics.AddToTail( pGraphic );

				char pBuf[255];
				UniqueIdToString( graphics[i]->GetId(), pBuf, 255 );
				unserializedGraphicMapping.Insert( pBuf, pGraphic );
			}
			else
			{
				Warning( "CGameUIDefinition: Warning unknown static graphic type\n" );
			}
		}


		CDmxAttribute *pTextureName = pLayer->GetAttribute( "statictexture" );
		if ( !pTextureName || pTextureName->GetType() != AT_STRING )
		{
			return false;
		}

		char textureName[128];
		pTextureName->GetValueAsString( textureName, 128 );
		InitSheetTexture( textureName );
	}
	// Dynamic graphics
	else if ( m_LayerType == SUBLAYER_DYNAMIC )
	{
		CDmxAttribute *pGraphicsAttr = pLayer->GetAttribute( "dynamicGraphics" );
		const CUtlVector< CDmxElement * > &graphics = pGraphicsAttr->GetArray< CDmxElement * >( );
		for ( int i = 0; i < graphics.Count(); ++i )
		{
			// What kind of graphic is this?
			const char *pType = graphics[i]->GetValueString( "classtype" );

			if ( !Q_stricmp( pType, "CDmeDynamicRectGeometry" ) )
			{
				CDynamicRect *pGraphic = new CDynamicRect( "" );
				if ( !pGraphic->Unserialize( graphics[i] ) )
				{
					delete pGraphic;
					return false;
				}
				m_LayerGraphics.AddToTail( pGraphic );
				char pBuf[255];
				UniqueIdToString( graphics[i]->GetId(), pBuf, 255 );
				unserializedGraphicMapping.Insert( pBuf, pGraphic );	
			}
			else
			{
				Warning( "CGameUIDefinition: Warning unknown static graphic type\n" );
			}
		}
	}
	// Font graphics
	else if ( m_LayerType == SUBLAYER_FONT )
	{
		CDmxAttribute *pGraphics = pLayer->GetAttribute( "fontGraphics" );
		const CUtlVector< CDmxElement * > &graphics = pGraphics->GetArray< CDmxElement * >( );
		int nCount = graphics.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			// What kind of graphic is this?
			const char *pType = graphics[i]->GetValueString( "classtype" );

			if ( !Q_stricmp( pType, "CDmeTextGeometry" ) )
			{
				CGameText *pGraphic = new CGameText( "" );
				if ( !pGraphic->Unserialize( graphics[i] ) )
				{
					delete pGraphic;
					return false;
				}
				m_LayerGraphics.AddToTail( pGraphic );
				char pBuf[255];
				UniqueIdToString( graphics[i]->GetId(), pBuf, 255 );
				GameGraphicMap_t graphicMapEntry;
				graphicMapEntry.m_Id = graphics[i]->GetId();
				graphicMapEntry.pGraphic = pGraphic;
				unserializedGraphicMapping.Insert( pBuf, pGraphic );
			}
			else
			{
				Warning( "CGameUIDefinition: Warning unknown font graphic type in gui file.\n" );
			}
		}
	}
	else
	{
		Assert(0); // Unknown layer type!!
	}

	return true;	
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CGameLayer::InitSheetTexture()
{
	Assert( m_pTextureName );
	return InitSheetTexture( m_pTextureName );
}



//-----------------------------------------------------------------------------
// FIXME: Must go into rendersystem at some point
//-----------------------------------------------------------------------------
static HRenderTexture CreateTextureFromVTFFile( const char *pFileName, CSheet **ppSheet )
{
	static intp s_UniqueID = 0x4000000;

	*ppSheet = NULL;

	char pTemp[MAX_PATH];
	Q_ComposeFileName( "materials/", pFileName, pTemp, sizeof(pTemp) );
	Q_SetExtension( pTemp, "vtf", sizeof(pTemp) );
	pFileName = pTemp;

	char pTemp360[MAX_PATH];
	if ( PLATFORM_EXT[0] )
	{
		CreatePlatformFilename( pFileName, pTemp360, sizeof(pTemp360) );
		pFileName = pTemp360;
	}
	CUtlBuffer fBuf;
	if ( !g_pFullFileSystem->ReadFile( pFileName, "GAME", fBuf ) )
	{
		Warning( "Unable to read texture file %s\n", pFileName );
		return RENDER_TEXTURE_HANDLE_INVALID;
	}

	IVTFTexture *pVtfTexture = CreateVTFTexture();
#if !defined( _GAMECONSOLE )
	pVtfTexture->Unserialize( fBuf );
#else
	pVtfTexture->UnserializeFromBuffer( fBuf, true, false, false, 0 );
#endif

	// we are only going to load the high mip
	// pCurData is pointing at pixel data now
	TextureHeader_t spec;
	memset( &spec, 0, sizeof(TextureHeader_t) );
	spec.m_nWidth = pVtfTexture->Width();
	spec.m_nHeight = pVtfTexture->Height();
	spec.m_nNumMipLevels = 1; //pVtfTexture->MipCount();
	spec.m_nDepth = pVtfTexture->Depth();
	spec.m_nImageFormat = pVtfTexture->Format();
	//spec.m_vecAverageColor.Init( pVtfTexture->Reflectivity().x, pVtfTexture->Reflectivity().y, pVtfTexture->Reflectivity().z, 1 );

	char pResourceName[16];
	Q_snprintf( pResourceName, sizeof(pResourceName), "%d", s_UniqueID );
	HRenderTexture hRet = g_pRenderDevice->FindOrCreateTexture( "gamelayer", pResourceName, &spec );
	ResourceAddRef( hRet );
	s_UniqueID++;
	IRenderContext *pCtx = g_pRenderDevice->GetRenderContext( );
	pCtx->SetTextureData( hRet, &spec, pVtfTexture->ImageData( 0, 0, 0 ), pVtfTexture->ComputeTotalSize(), pVtfTexture->IsPreTiled() );
	pCtx->Submit( );
	g_pRenderDevice->ReleaseRenderContext( pCtx );
	ResourceRelease( hRet );

	size_t nSheetByteCount;
	const void *pSheetData = pVtfTexture->GetResourceData( VTF_RSRC_SHEET, &nSheetByteCount );
	if ( pSheetData )
	{
		// expand compact sheet into fatter runtime form
		CUtlBuffer bufLoad( pSheetData, nSheetByteCount, CUtlBuffer::READ_ONLY );
		*ppSheet = new CSheet( bufLoad );
	}

	DestroyVTFTexture( pVtfTexture );
	return hRet;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CGameLayer::InitSheetTexture( const char *pBaseTextureName )
{
	m_pTextureName = pBaseTextureName;

	if ( !g_pRenderDevice )
	{
		// Material names must be different for each menu.
		CFmtStr materialName;
		switch ( m_LayerType )
		{
		case SUBLAYER_STATIC:
			materialName.sprintf( "statictx_%s", pBaseTextureName );
			break;
		case SUBLAYER_DYNAMIC:
			materialName.sprintf( "dynamictx_%s", pBaseTextureName );
			break;
		case SUBLAYER_FONT:
			materialName.sprintf( "fonttx_%s", pBaseTextureName );
			break;
		default:
			Assert(0);
			break;
		};

		KeyValues *pVMTKeyValues = new KeyValues( "GameControls" );
		pVMTKeyValues->SetString( "$basetexture", pBaseTextureName );
		m_Material.Init( materialName, TEXTURE_GROUP_OTHER, pVMTKeyValues );
		m_Material->Refresh();
		m_Sheet.Set( LoadSheet( m_Material ) );
	}
	else
	{
		CUtlString materialName;
		switch ( m_LayerType )
		{
		case SUBLAYER_STATIC:
			CSheet *pSheet;
			m_hTexture = CreateTextureFromVTFFile( pBaseTextureName, &pSheet );
			m_Sheet.Set( pSheet );
			break;
		case SUBLAYER_DYNAMIC:
			break;
		case SUBLAYER_FONT:
			break;
		default:
			Assert(0);
			break;
		};
	}

	return true;
}


//-----------------------------------------------------------------------------
// UI sheets
//-----------------------------------------------------------------------------
CSheet *CGameLayer::LoadSheet( IMaterial *pMaterial )
{
	if ( !pMaterial )
		return NULL;

	bool bFoundVar = false;
	IMaterialVar *pVar = pMaterial->FindVar( "$basetexture", &bFoundVar, true );

	if ( bFoundVar && pVar && pVar->IsDefined() )
	{
		ITexture *pTex = pVar->GetTextureValue();
		if ( pTex && !pTex->IsError() )
		{
			return LoadSheet( pTex->GetName(), pTex );
		}
	}

	return NULL;
}

void CGameLayer::GetSheetTextureSize( int &nWidth, int &nHeight )
{
	if ( !m_Material )
		return;

	bool bFoundVar = false;
	IMaterialVar *pVar = m_Material->FindVar( "$basetexture", &bFoundVar, true );

	if ( bFoundVar && pVar && pVar->IsDefined() )
	{
		ITexture *pTex = pVar->GetTextureValue();
		if ( pTex && !pTex->IsError() )
		{
			nWidth = pTex->GetActualWidth();
			nHeight = pTex->GetActualHeight();
		}
	}
}

//--------------------------------------------------------------------------------
// UI sheets
//--------------------------------------------------------------------------------
CSheet *CGameLayer::LoadSheet( char const *pszFname, ITexture *pTexture )
{
	//if ( !m_bShouldLoadSheets )
	//	return NULL;

	//if ( m_SheetList.Defined( pszFname ) )
	//	return m_SheetList[ pszFname ];

	CSheet *pNewSheet = NULL;

	// get compact sheet representation held by texture
	size_t numBytes;
	void const *pSheet = pTexture->GetResourceData( VTF_RSRC_SHEET, &numBytes );
	if ( pSheet )
	{
		// expand compact sheet into fatter runtime form
		CUtlBuffer bufLoad( pSheet, numBytes, CUtlBuffer::READ_ONLY );
		pNewSheet = new CSheet( bufLoad );
	}

	//m_SheetList[ pszFname ] = pNewSheet;
	return pNewSheet;
}


//--------------------------------------------------------------------------------
// Add a graphic to the list.
//--------------------------------------------------------------------------------
int CGameLayer::AddGraphic( CGameGraphic *pGraphic )
{
	return m_LayerGraphics.AddToTail( pGraphic );
}

//--------------------------------------------------------------------------------
// Remove a graphic from the list.
// Returns true if it was found and removed.
//--------------------------------------------------------------------------------
bool CGameLayer::RemoveGraphic( CGameGraphic *pGraphic )
{
	return m_LayerGraphics.FindAndRemove( pGraphic );
}


//--------------------------------------------------------------------------------
// Clear out the graphics list
//--------------------------------------------------------------------------------
void CGameLayer::ClearGraphics()
{
	m_LayerGraphics.RemoveAll();
}


//--------------------------------------------------------------------------------
// Clear out the graphics list
//--------------------------------------------------------------------------------
bool CGameLayer::HasGraphic( CGameGraphic *pGraphic )
{
	int nCount = m_LayerGraphics.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pGraphic == m_LayerGraphics[i] )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
IMaterial *CGameLayer::GetMaterial()
{
	// FIXME
	// NOTE: This has to be this way to ensure we don't load every freaking material @ startup
	//Assert( IsPrecached() );
	//if ( !IsPrecached() )
	//	return NULL;
	return m_Material;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameLayer::InvalidateSheetSymbol()
{
	m_bSheetSymbolCached = false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameLayer::CacheSheetSymbol( CUtlSymbol sheetSymbol )
{
	m_SheetSymbol = sheetSymbol;
	m_bSheetSymbolCached = true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CUtlSymbol CGameLayer::GetSheetSymbol() const
{
	Assert( IsSheetSymbolCached() );
	return m_SheetSymbol;
}

//-----------------------------------------------------------------------------
// Start playing animations
//-----------------------------------------------------------------------------
void CGameLayer::StartPlaying()
{
	int nCount = m_LayerGraphics.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_LayerGraphics[i]->StartPlaying();
	}
}


//-----------------------------------------------------------------------------
// Stop playing animations
//-----------------------------------------------------------------------------
void CGameLayer::StopPlaying()
{
	int nCount = m_LayerGraphics.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_LayerGraphics[i]->StopPlaying();
	}
}


//-----------------------------------------------------------------------------
// Move to the next available animation state
//-----------------------------------------------------------------------------
void CGameLayer::AdvanceState()
{
	int nCount = m_LayerGraphics.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_LayerGraphics[i]->AdvanceState();
	}
}


//-----------------------------------------------------------------------------
// Set all graphics to "default" state.
//-----------------------------------------------------------------------------
void CGameLayer::InitAnims()
{
	int nCount = m_LayerGraphics.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		m_LayerGraphics[i]->SetState( "default" );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameLayer::UpdateGeometry()
{
	int nGraphicsCount = m_LayerGraphics.Count();
	if ( nGraphicsCount == 0 )
	{
		return;
	}
 
	for ( int i = 0; i < nGraphicsCount; ++i )
	{
		m_LayerGraphics[i]->UpdateGeometry();
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameLayer::UpdateRenderTransforms( const StageRenderInfo_t &stageRenderInfo )
{
	int nGraphicsCount = m_LayerGraphics.Count();
	if ( nGraphicsCount == 0 )
	{
		return;
	}

	for ( int i = 0; i < nGraphicsCount; ++i )
	{
		m_LayerGraphics[i]->UpdateRenderTransforms( stageRenderInfo );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameLayer::UpdateRenderData( CGameUIDefinition &gameUIDef, color32 parentColor, CUtlVector< LayerRenderLists_t > &renderLists )
{
	bool bDrawExtents = false;

	int nGraphicsCount = m_LayerGraphics.Count();
	if ( nGraphicsCount == 0 )
	{
		return;
	}
	
	// Basic algorithm is one renderList per material.
	// Static graphics are sorted so that they all use the same sheet texture.
	// Font graphics are sorted by render order. Their rendergeometry contains the font ID to use
	// Dynamic graphics could add renderlists, since the material can change.
	// Dynamic graphics will keep adding to the same renderlist until the material's don't match.
	// This adds draw calls but preserves render order.

	if ( m_LayerType == SUBLAYER_DYNAMIC )
	{
		int listIndex = -1;
		int layerIndex = -1;
		for ( int i = 0; i < nGraphicsCount; ++i )
		{
			// Groups should never be in lists of layer graphics.
			Assert( !m_LayerGraphics[i]->IsGroup() );
			CGraphicGroup *pGroup = m_LayerGraphics[i]->GetGroup();
			Assert( pGroup );
			// Only dynamic graphics are allowed in dynamic sublayers.
			Assert( m_LayerGraphics[i]->IsDynamic() );

			// Get the material this graphic uses.
			const char *pAlias = m_LayerGraphics[i]->GetMaterialAlias();
			IMaterial *pAliasMaterial = g_pGameUISystemMgrImpl->GetImageAliasMaterial( pAlias );
			// Is the material this graphic uses the same as the one in our current renderlist?
			if ( i == 0 || renderLists[layerIndex].m_pMaterial != pAliasMaterial )
			{
				// Our material has changed. Add a new renderlist for this material.
				layerIndex = renderLists.AddToTail();
				listIndex = renderLists[layerIndex].m_RenderGeometryLists.AddToTail();
				renderLists[layerIndex].m_LayerType = m_LayerType;
				renderLists[layerIndex].m_pMaterial = pAliasMaterial;
				renderLists[layerIndex].m_pSheet = NULL;
				renderLists[layerIndex].m_hTexture = m_hTexture;
			}
			
			CUtlVector< RenderGeometryList_t > &renderGeometryLists = renderLists[layerIndex].m_RenderGeometryLists;
			Assert( layerIndex != -1 );
			Assert( listIndex != -1 );
			m_LayerGraphics[i]->UpdateRenderData( pGroup->GetResultantColor(), renderGeometryLists, listIndex );
		}
	}
	else if ( m_LayerType == SUBLAYER_STATIC ) 
	{
		int listIndex = -1;
		int layerIndex = -1;

		layerIndex = renderLists.AddToTail();
		listIndex = renderLists[layerIndex].m_RenderGeometryLists.AddToTail();
		renderLists[layerIndex].m_LayerType = m_LayerType;
		renderLists[layerIndex].m_pMaterial = m_Material;
		renderLists[layerIndex].m_pSheet = m_Sheet;
		renderLists[layerIndex].m_hTexture = m_hTexture;

		CUtlVector< RenderGeometryList_t > &renderGeometryLists = renderLists[layerIndex].m_RenderGeometryLists;
		Assert( layerIndex != -1 );
		Assert( listIndex != -1 );
		
		for ( int i = 0; i < nGraphicsCount; ++i )
		{
			// Groups should never be in lists of layer graphics.
			Assert( !m_LayerGraphics[i]->IsGroup() );
			CGraphicGroup *pGroup = m_LayerGraphics[i]->GetGroup();
			Assert( pGroup );

			m_LayerGraphics[i]->UpdateRenderData( pGroup->GetResultantColor(), renderGeometryLists, listIndex );	
		}


		if ( bDrawExtents )
		{
			for ( int i = 0; i < nGraphicsCount; ++i )
			{
				if ( !m_LayerGraphics[i]->IsGroup() ) 
				{
					m_LayerGraphics[i]->DrawExtents( renderGeometryLists, listIndex );	
				}
			}
		}
	}
	else if ( m_LayerType == SUBLAYER_FONT )
	{
		int listIndex = -1;
		int layerIndex = -1;

		layerIndex = renderLists.AddToTail();
		listIndex = renderLists[layerIndex].m_RenderGeometryLists.AddToTail();
		renderLists[layerIndex].m_LayerType = m_LayerType;
		renderLists[layerIndex].m_pMaterial = m_Material;
		renderLists[layerIndex].m_pSheet = m_Sheet;
		renderLists[layerIndex].m_hTexture = m_hTexture;

		CUtlVector< RenderGeometryList_t > &renderGeometryLists = renderLists[layerIndex].m_RenderGeometryLists;
		Assert( layerIndex != -1 );
		Assert( listIndex != -1 );

		for ( int i = 0; i < nGraphicsCount; ++i )
		{
			// Groups should never be in lists of layer graphics.
			Assert( !m_LayerGraphics[i]->IsGroup() );
			CGraphicGroup *pGroup = m_LayerGraphics[i]->GetGroup();
			Assert( pGroup );

			m_LayerGraphics[i]->UpdateRenderData( pGroup->GetResultantColor(), renderGeometryLists, listIndex );	
		}

		if ( bDrawExtents )
		{
			// Find the static material and sheet
			for ( int i = 0; i < renderLists.Count(); ++i )
			{
				if ( renderLists[i].m_LayerType == SUBLAYER_STATIC )
				{
					// Make another layer for rendering extents
					// Only do this if there is a static texture around to use for rendering.
					layerIndex = renderLists.AddToTail();
					listIndex = renderLists[layerIndex].m_RenderGeometryLists.AddToTail();
					renderLists[layerIndex].m_LayerType = SUBLAYER_STATIC;   // must be static since these render as triangles.
					renderLists[layerIndex].m_pMaterial = renderLists[i].m_pMaterial;
					renderLists[layerIndex].m_pSheet = renderLists[i].m_pSheet;
					renderLists[layerIndex].m_hTexture = renderLists[i].m_hTexture;

					CUtlVector< RenderGeometryList_t > &renderGeometryLists2 = renderLists[layerIndex].m_RenderGeometryLists;
					Assert( layerIndex != -1 );
					Assert( listIndex != -1 );

					for ( int i = 0; i < nGraphicsCount; ++i )
					{
						CGameText *pText = dynamic_cast<CGameText *>( m_LayerGraphics[i] );
						Assert( pText );
						pText->DrawExtents( renderGeometryLists2, listIndex );	
					}
					break;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Given a position, return the front most graphic under it.
//-----------------------------------------------------------------------------
CGameGraphic *CGameLayer::GetGraphic( int x, int y )
{
	int nGraphicCount = m_LayerGraphics.Count();
	for ( int i = nGraphicCount-1; i >= 0; --i )
	{
		CGameGraphic *pGraphic = m_LayerGraphics[i];
		if ( pGraphic->HitTest( x, y ) )
			return pGraphic;
	}	

	return NULL;
}

//-----------------------------------------------------------------------------
// Given a position, return the front most graphic that can take input under it.
//-----------------------------------------------------------------------------
CGameGraphic *CGameLayer::GetMouseFocus( int x, int y )
{
	int nGraphicCount = m_LayerGraphics.Count();
	for ( int i = nGraphicCount-1; i >= 0; --i )
	{
		CGameGraphic *pGraphic = m_LayerGraphics[i];
		if ( pGraphic->CanAcceptInput() && pGraphic->HitTest( x, y ) )
			return pGraphic;
	}	

	return NULL;
}

//-----------------------------------------------------------------------------
// Given a position, return the front most graphic that can take input under it.
//-----------------------------------------------------------------------------
CGameGraphic *CGameLayer::GetNextFocus( bool &bGetNext, CGameGraphic *pCurrentGraphic )
{
	int nGraphicCount = m_LayerGraphics.Count();
	for ( int i = 0; i < nGraphicCount; ++i )
	{
		CGameGraphic *pGraphic = m_LayerGraphics[i];

		if ( bGetNext && pGraphic->CanAcceptInput() )
		{
			return pGraphic;
		}

		if ( pCurrentGraphic == pGraphic )
		{
			bGetNext = true;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Given a name of a graphic, find it in this layer
//-----------------------------------------------------------------------------
CGameGraphic *CGameLayer::FindGraphicByName( const char *pName )
{ 
	int nGraphicCount = m_LayerGraphics.Count();
	for ( int i = 0; i < nGraphicCount; ++i )
	{
		CGameGraphic *pGraphic = m_LayerGraphics[i]->FindGraphicByName( pName );
		if ( pGraphic )
		{
			// Match.
			return pGraphic;
		}
	}
	
	return NULL;
}






