//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "gameuidynamictextures.h"
#include "gamelayer.h"
#include "gamerect.h"
#include "tier1/utlstring.h"
#include "tier1/utlstringmap.h"
#include "tier1/utlbuffer.h"
#include "gameuisystemmgr.h"
#include "tier1/fmtstr.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"


#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "rendersystem/irenderdevice.h"
#include "rendersystem/irendercontext.h"
#include "tier1/keyvalues.h"
#include "materialsystem/IMaterialProxy.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "gameuisystemmgr.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

	 
#define GAMEUI_DYNAMIC_TEXTURE_SHEET_WIDTH 2048
#define GAMEUI_DYNAMIC_TEXTURE_SHEET_HEIGHT 2048

//-----------------------------------------------------------------------------
// A material proxy that resets the base texture to use the dynamic texture
//-----------------------------------------------------------------------------
class CGameControlsProxy : public IMaterialProxy
{
public:
	CGameControlsProxy();
	virtual ~CGameControlsProxy(){};
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pProxyData );
	virtual void Release( void ) 
	{ 
		delete this; 
	}
	virtual IMaterial *GetMaterial();

private:
	IMaterialVar* m_BaseTextureVar;
};

CGameControlsProxy::CGameControlsProxy(): m_BaseTextureVar( NULL )
{
}

bool CGameControlsProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	bool bFoundVar;
	m_BaseTextureVar = pMaterial->FindVar( "$basetexture", &bFoundVar, false );
	return bFoundVar;
}

void CGameControlsProxy::OnBind( void *pProxyData )
{
	const char *pBaseTextureName = ( const char * )pProxyData;
	ITexture *pTexture = g_pMaterialSystem->FindTexture( pBaseTextureName, TEXTURE_GROUP_OTHER, true );
	m_BaseTextureVar->SetTextureValue( pTexture );
}

IMaterial *CGameControlsProxy::GetMaterial()
{
	return m_BaseTextureVar->GetOwningMaterial();
}


//-----------------------------------------------------------------------------
// Factory to create dynamic material. ( Return in this case. )
//-----------------------------------------------------------------------------
class CMaterialProxyFactory : public IMaterialProxyFactory
{
public:
	IMaterialProxy *CreateProxy( const char *proxyName );
	void DeleteProxy( IMaterialProxy *pProxy );
	CreateInterfaceFn GetFactory();
};
static CMaterialProxyFactory s_DynamicMaterialProxyFactory;

IMaterialProxy *CMaterialProxyFactory::CreateProxy( const char *proxyName )
{
	if ( Q_strcmp( proxyName, "GameControlsProxy" ) == NULL )
	{	
		return static_cast< IMaterialProxy * >( new CGameControlsProxy );
	}
	return NULL;
}

void CMaterialProxyFactory::DeleteProxy( IMaterialProxy *pProxy )
{
}

CreateInterfaceFn CMaterialProxyFactory::GetFactory()
{
	return Sys_GetFactoryThis();
}


//-----------------------------------------------------------------------------
// Constructor / Destructor.
//-----------------------------------------------------------------------------
CGameUIDynamicTextures::CGameUIDynamicTextures() 
{
	m_pDynamicTexturePacker = NULL;
	m_RenderMaterial = NULL;
	m_bRegenerate = false;	
}

CGameUIDynamicTextures::~CGameUIDynamicTextures()
{	
	Shutdown();
}

//-----------------------------------------------------------------------------
// Init any render targets needed by the UI.
//-----------------------------------------------------------------------------
void CGameUIDynamicTextures::InitRenderTargets()
{
	if ( !m_TexturePage.IsValid() )
	{
		m_TexturePage.InitRenderTarget( GAMEUI_DYNAMIC_TEXTURE_SHEET_WIDTH, GAMEUI_DYNAMIC_TEXTURE_SHEET_HEIGHT, 
			RT_SIZE_NO_CHANGE, IMAGE_FORMAT_ARGB8888, 
			MATERIAL_RT_DEPTH_NONE, false, "_rt_DynamicUI" );
	}

	if ( m_TexturePage.IsValid() )
	{
		int nSheetWidth = m_TexturePage->GetActualWidth();
		int nSheetHeight = m_TexturePage->GetActualHeight();

		m_pDynamicTexturePacker = new CTexturePacker( nSheetWidth, nSheetHeight, IsGameConsole() ? 0 : 1 );

		KeyValues *pVMTKeyValues = new KeyValues( "GameControls" );
		pVMTKeyValues->SetString( "$basetexture", "_rt_DynamicUI" );
		CMaterialReference material;
		// Material names must be different
		CFmtStr materialName;
		materialName.sprintf( "dynamictx_%s", "_rt_DynamicUI" );
		material.Init( materialName, TEXTURE_GROUP_OTHER, pVMTKeyValues );
		material->Refresh();

		{
			ImageAliasData_t imageData;
			imageData.m_Width = nSheetWidth;
			imageData.m_Height = nSheetHeight;
			imageData.m_Material = material;
			m_ImageAliasMap[ "_rt_DynamicUI" ] = imageData;
		}

		{
			CTextureReference pErrorTexture;
			pErrorTexture.Init( "error", TEXTURE_GROUP_OTHER, true );

			ImageAliasData_t imageData;
			imageData.m_Width = pErrorTexture->GetActualWidth();
			imageData.m_Height = pErrorTexture->GetActualHeight();
			imageData.m_szBaseTextureName = "error";
			imageData.m_Material = NULL;
			m_ImageAliasMap[ "errorImageAlias" ] = imageData;
		}

		pVMTKeyValues = new KeyValues( "GameControls" );
		pVMTKeyValues->SetInt( "$ignorez", 1 );
		KeyValues *pProxies = new KeyValues( "Proxies" );
		pProxies->AddSubKey( new KeyValues( "GameControlsProxy" ) );
		pVMTKeyValues->AddSubKey( pProxies );
		m_RenderMaterial.Init( "dynamic_render_texture", TEXTURE_GROUP_OTHER, pVMTKeyValues );
		m_RenderMaterial->Refresh();

		g_pGameUISystemMgrImpl->InitImageAlias( "defaultImageAlias" );
		g_pGameUISystemMgrImpl->LoadImageAliasTexture( "defaultImageAlias", "vguiedit/pixel"  );
	}
}

IMaterialProxy *CGameUIDynamicTextures::CreateProxy( const char *proxyName )
{
	return s_DynamicMaterialProxyFactory.CreateProxy( proxyName );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameUIDynamicTextures::Shutdown()
{
	m_RenderMaterial.Shutdown();
	m_TexturePage.Shutdown();

	if ( m_pDynamicTexturePacker )
	{
		delete m_pDynamicTexturePacker;
	}

	m_pDynamicTexturePacker = NULL;
	m_RenderMaterial = NULL;
	m_bRegenerate = false;
}

void CGameUIDynamicTextures::SetImageEntry( const char *pEntryName, ImageAliasData_t &imageData )
{
	m_ImageAliasMap[ pEntryName ] = imageData;
}


//-----------------------------------------------------------------------------
// Associate this image alias name with a .vtf texture.
//-----------------------------------------------------------------------------
void CGameUIDynamicTextures::LoadImageAlias( const char *pAlias, const char *pBaseTextureName )
{
	if ( !pAlias || !*pAlias )
		return;

	if ( !pBaseTextureName || !*pBaseTextureName )
		return;


	CFmtStr texturePath;
	texturePath.sprintf( "materials\\%s.vtf", pBaseTextureName );
	if ( !g_pFullFileSystem->FileExists( texturePath.Access(), IsGameConsole() ? "MOD" : "GAME" ) )
	{
		Warning( "Unable to find game ui dynamic texture \"%s\"\n", texturePath.Access() );
	}

	if ( Q_strcmp( pBaseTextureName, "" ) == 0 )
	{
		ImageAliasData_t *pImageData = GetImageAliasData( "errorImageAlias" );
		ImageAliasData_t imageData;
		imageData.Init();
		imageData.m_XPos = pImageData->m_XPos;
		imageData.m_YPos = pImageData->m_YPos;
		imageData.m_Width = pImageData->m_Width;
		imageData.m_Height = pImageData->m_Height;
		imageData.m_szBaseTextureName = pImageData->m_szBaseTextureName;
		imageData.m_Material = pImageData->m_Material;
		imageData.m_bIsInSheet = pImageData->m_bIsInSheet;
		imageData.m_nNodeIndex = pImageData->m_nNodeIndex;
		SetImageEntry( pAlias, imageData );
		return;
	}


	Msg( "Loading Alias %s Texture %s\n", pAlias, pBaseTextureName );


	CTextureReference pTexture;
	pTexture.Init( pBaseTextureName, TEXTURE_GROUP_OTHER, true );

	ImageAliasData_t *pImageData = GetImageAliasData( pAlias );
	int nodeIndex = -1;

	if ( m_pDynamicTexturePacker )
	{
		if ( !IsErrorImageAliasData( pImageData ) )
		{
			if ( pImageData->m_nNodeIndex != -1 )
			{
				// We already had something packed in there for this alias.
				// Remove the old alias texture
				m_pDynamicTexturePacker->RemoveRect( pImageData->m_nNodeIndex );
			}

			// Set up new imagedata values
			pImageData->m_Width = pTexture->GetActualWidth();
			pImageData->m_Height = pTexture->GetActualHeight();
			pImageData->m_szBaseTextureName = pBaseTextureName;
		}
		else
		{
			ImageAliasData_t imageData;
			imageData.m_Width = pTexture->GetActualWidth();
			imageData.m_Height = pTexture->GetActualHeight();
			imageData.m_szBaseTextureName = pBaseTextureName;
			imageData.m_nRefCount = pImageData->m_nRefCount;
			SetImageEntry( pAlias, imageData );
			pImageData = GetImageAliasData( pAlias );
			Assert( !IsErrorImageAliasData( pImageData ) );
		}

		int nSheetWidth = 0;
		int nSheetHeight = 0;
		GetDynamicSheetSize( nSheetWidth, nSheetHeight );
		Assert( nSheetWidth != 0 );
		Assert( nSheetHeight != 0 );
		// If our texture is huge in some way, don't bother putting it in the packed texture.
		if ( ( pTexture->GetActualWidth() << 1 ) < nSheetWidth || ( pTexture->GetActualHeight() << 1 ) < nSheetHeight ) 
		{
			// Put this texture into the packed render target texture.
			Rect_t texRect;
			texRect.x = 0;
			texRect.y = 0;
			texRect.width = pTexture->GetActualWidth();
			texRect.height = pTexture->GetActualHeight();
			nodeIndex = m_pDynamicTexturePacker->InsertRect( texRect );	
		}
	}

	// For debugging this will make no packing happen. Each texture is one draw call
	//nodeIndex = -1;
	pImageData->m_nNodeIndex = nodeIndex;

	LoadImageAliasTexture( pAlias, pBaseTextureName );
}

//-----------------------------------------------------------------------------
// Release the entry for this alias in the packer.
//-----------------------------------------------------------------------------
void CGameUIDynamicTextures::ReleaseImageAlias( const char *pAlias )
{
	if ( Q_strcmp( pAlias, "_rt_DynamicUI" ) == 0 )
		return;
	if ( Q_strcmp( pAlias, "errorImageAlias" ) == 0 )
		return;
	if ( Q_strcmp( pAlias, "defaultImageAlias" ) == 0 )
		return;

	ImageAliasData_t *pImageData = GetImageAliasData( pAlias );	
	if ( pImageData )
	{
		Assert( pImageData->m_nRefCount > 0 );
		pImageData->m_nRefCount--;
		if ( pImageData->m_nRefCount == 0 )
		{
			if ( pImageData->m_bIsInSheet )
			{
				Msg( "Releasing Alias %s\n", pAlias );
				m_pDynamicTexturePacker->RemoveRect( pImageData->m_nNodeIndex );
			}
		
			pImageData->Init();
		}
	}
}

//-----------------------------------------------------------------------------
// Associate this image alias name with a .vtf texture.
// This fxn loads the texture into the GameControls shader and makes a material 
// for you
//-----------------------------------------------------------------------------
void CGameUIDynamicTextures::LoadImageAliasTexture( const char *pAlias, const char *pBaseTextureName )
{
	if ( !pBaseTextureName || !*pBaseTextureName )
		return;

	CTextureReference pTexture;
	pTexture.Init( pBaseTextureName, TEXTURE_GROUP_OTHER, true );

	ImageAliasData_t *pImageData = GetImageAliasData( pAlias );
	
	// Now set up the material for the new imagedata.
	if ( pImageData->m_nNodeIndex == -1 )// it won't fit in the map, give it its own material, it is its own draw call.
	{
		// Material names must be different
		CUtlVector<char *> words;
		V_SplitString( pImageData->m_szBaseTextureName, "/", words );

		CFmtStr materialName;
		materialName.sprintf( "dynamictx_%s", words[words.Count()-1] );

		KeyValues *pVMTKeyValues = new KeyValues( "GameControls" );
		pVMTKeyValues->SetString( "$basetexture", pImageData->m_szBaseTextureName );
		CMaterialReference material;
		material.Init( materialName, TEXTURE_GROUP_OTHER, pVMTKeyValues );
		material->Refresh();

		pImageData->m_Material = material;
		pImageData->m_bIsInSheet = false;
	}
	else // Do this if it fit in the packed texture.
	{
		if ( !m_RenderMaterial )
			return;

		pImageData->m_Material.Init( GetImageAliasMaterial( "_rt_DynamicUI" ) );
		Assert( pImageData->m_Material );
		pImageData->m_bIsInSheet = true;
		
		const CTexturePacker::TreeEntry_t &newEntry = m_pDynamicTexturePacker->GetEntry( pImageData->m_nNodeIndex );
		Assert( newEntry.rc.width == pImageData->m_Width );
		Assert( newEntry.rc.height == pImageData->m_Height );

		pImageData->m_XPos = newEntry.rc.x;
		pImageData->m_YPos = newEntry.rc.y;

		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		pRenderContext->PushRenderTargetAndViewport( m_TexturePage, NULL, pImageData->m_XPos, pImageData->m_YPos, pImageData->m_Width, pImageData->m_Height );	
		pRenderContext->MatrixMode( MATERIAL_PROJECTION );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();
		pRenderContext->Scale( 1, -1, 1 );
		float flPixelOffsetX = 0.5f;
		float flPixelOffsetY = 0.5f;
		pRenderContext->Ortho( flPixelOffsetX, flPixelOffsetY, pImageData->m_Width + flPixelOffsetX, pImageData->m_Height + flPixelOffsetY, -1.0f, 1.0f );
		//pRenderContext->Ortho( 0, 0, pImageData->m_Width, pImageData->m_Height, -1.0f, 1.0f );

		// Clear to random color. Useful for testing.
		//pRenderContext->ClearColor3ub( rand()%256, rand()%256, rand()%256 );
		pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
		pRenderContext->ClearBuffers( true, false );
	

		// make sure there is no translation and rotation laying around
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();

		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->PushMatrix();
		pRenderContext->LoadIdentity();
		
		pRenderContext->Bind( m_RenderMaterial, (void *)pImageData->m_szBaseTextureName.Get() );

		int x = 0;
		int y = 0;
		int wide = pImageData->m_Width;
		int tall = pImageData->m_Height;


		unsigned char drawcolor[4];
		drawcolor[0] = 255;
		drawcolor[1] = 255;
		drawcolor[2] = 255;
		drawcolor[3] = 255;

		IMesh *pMesh = pRenderContext->GetDynamicMesh( true );
		if ( pMesh )
		{
			CMeshBuilder meshBuilder;
			meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

			meshBuilder.Position3f( x, y, 0 );
			meshBuilder.Color4ubv( drawcolor );
			meshBuilder.TexCoord2f( 0, 0, 0 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

			meshBuilder.Position3f( wide, y, 0 );
			meshBuilder.Color4ubv( drawcolor );
			meshBuilder.TexCoord2f( 0, 1, 0 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

			meshBuilder.Position3f( wide, tall, 0 );
			meshBuilder.Color4ubv( drawcolor );
			meshBuilder.TexCoord2f( 0, 1, 1 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

			meshBuilder.Position3f( x, tall, 0 );
			meshBuilder.Color4ubv( drawcolor );
			meshBuilder.TexCoord2f( 0, 0, 1 );
			meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

			meshBuilder.End();
			pMesh->Draw();
		}


		// Restore the matrices
		pRenderContext->MatrixMode( MATERIAL_PROJECTION );
		pRenderContext->PopMatrix();

		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->PopMatrix();

		pRenderContext->MatrixMode( MATERIAL_VIEW );
		pRenderContext->PopMatrix();
		

		pRenderContext->PopRenderTargetAndViewport();
	}		
}

//-----------------------------------------------------------------------------
// Return the material bound to this image alias.
// If the texture was not found you will see the purple and black checkerboard
// error texture.
//-----------------------------------------------------------------------------
IMaterial *CGameUIDynamicTextures::GetImageAliasMaterial( const char *pAlias ) 
{ 
	if ( m_ImageAliasMap.Defined( pAlias ) )
	{
		return m_ImageAliasMap[pAlias].m_Material;
	}
	return m_ImageAliasMap[ "errorImageAlias" ].m_Material;			
}


//-----------------------------------------------------------------------------
// Return the data bound to this image alias.
// If the alias was not found you will see the purple and black checkerboard
// error texture.
//-----------------------------------------------------------------------------
ImageAliasData_t *CGameUIDynamicTextures::GetImageAliasData( const char *pAlias )
{ 
	if ( m_ImageAliasMap.Defined( pAlias ) )
	{
		return &m_ImageAliasMap[pAlias];
	}
	
	return &m_ImageAliasMap[ "errorImageAlias" ];
}


//-----------------------------------------------------------------------------
// Return true if this data is the error alias
//-----------------------------------------------------------------------------
bool CGameUIDynamicTextures::IsErrorImageAliasData( ImageAliasData_t *pData ) 
{
	return ( &m_ImageAliasMap[ "errorImageAlias" ] ) == pData;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUIDynamicTextures::GetDynamicSheetSize( int &nWidth, int &nHeight )
{
	nWidth = m_TexturePage->GetActualWidth();
	nHeight = m_TexturePage->GetActualHeight();
}


//-----------------------------------------------------------------------------
// Draw the dynamic texture associated with this alias at x, y
// It is drawn full size. This fxn is modeled off of CGameUISystemSurface::DrawFontTexture()
//-----------------------------------------------------------------------------
void CGameUIDynamicTextures::DrawDynamicTexture( const char *pAlias, int x, int y )
{
	if ( !pAlias || !*pAlias )
		return;


	ImageAliasData_t *pImageData = GetImageAliasData( pAlias );

	int wide = 0;
	int tall = 0;

	if ( !pImageData->m_bIsInSheet )
	{
		wide = pImageData->m_Width;
		tall = pImageData->m_Height;
	}
	else
	{	
		GetDynamicSheetSize( wide, tall );
	}
	
	color32 drawcolor;
	drawcolor.r = 255;
	drawcolor.g = 255;
	drawcolor.b = 255;
	drawcolor.a = 255;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pImageData->m_Material );
	if ( !pMesh )
		return;

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Position3f( x, y, 0 );
	meshBuilder.Color4ub( drawcolor.r, drawcolor.g, drawcolor.b, drawcolor.a );
	meshBuilder.TexCoord3f( 0, 0, 0, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( x + wide, y, 0 );
	meshBuilder.Color4ub( drawcolor.r, drawcolor.g, drawcolor.b, drawcolor.a );
	meshBuilder.TexCoord3f( 0, 1, 0, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( x + wide, y + tall, 0 );
	meshBuilder.Color4ub( drawcolor.r, drawcolor.g, drawcolor.b, drawcolor.a );
	meshBuilder.TexCoord3f( 0, 1, 1, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( x, y + tall, 0 );
	meshBuilder.Color4ub( drawcolor.r, drawcolor.g, drawcolor.b, drawcolor.a );
	meshBuilder.TexCoord3f( 0, 0, 1, 0 );
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();			
}

//-----------------------------------------------------------------------------
// Reload all textures into the rendertarget.
//-----------------------------------------------------------------------------
void CGameUIDynamicTextures::OnRestore( int nChangeFlags )
{
	m_bRegenerate = true;
}

//-----------------------------------------------------------------------------
// Reload all textures into the rendertarget.
//-----------------------------------------------------------------------------
void CGameUIDynamicTextures::RegenerateTexture( int nChangeFlags )
{
	if ( !m_bRegenerate )
		return;

	int numEntries = m_ImageAliasMap.GetNumStrings();
	for ( int i = 0; i < numEntries; ++i )
	{
		const char *pAlias = m_ImageAliasMap.String( i );
		ImageAliasData_t *pImageData = GetImageAliasData( pAlias );
		if ( IsErrorImageAliasData( pImageData )  )
			continue;
		if ( Q_strcmp( pAlias, "_rt_DynamicUI") == 0 )
			continue;

		if ( !pImageData->m_szBaseTextureName || !*pImageData->m_szBaseTextureName )
			continue;

		ITexture *pTexture = g_pMaterialSystem->FindTexture( pImageData->m_szBaseTextureName.Get(), TEXTURE_GROUP_OTHER, true );
		Assert( pTexture );
		pTexture->Download();

		LoadImageAliasTexture( pAlias, pImageData->m_szBaseTextureName );
	}
}



