//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "gameuidefinition.h"
#include "gamelayer.h"
#include "gamerect.h"
#include "gametext.h"
#include "hitarea.h"
#include "graphicgroup.h"
#include "tier1/utlstringmap.h"
#include "tier1/utlbuffer.h"
#include "gameuisystem.h"
#include "gameuiscript.h"
#include "gameuisystemmgr.h"
#include "tier1/fmtstr.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"


#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "rendersystem/irenderdevice.h"
#include "rendersystem/irendercontext.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static unsigned int s_nBaseTextureVarCache = 0;


BEGIN_DMXELEMENT_UNPACK ( CGameUIDefinition ) 
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "name", "", m_pName ) 
END_DMXELEMENT_UNPACK( CGameUIDefinition, s_GameUIDefinitionUnpack )


//-----------------------------------------------------------------------------
// Constructor / Destructor.
//-----------------------------------------------------------------------------
CGameUIDefinition::CGameUIDefinition( IGameUISystem *pGameUISystem /* = NULL */ ) :
	m_pGameUISystem( pGameUISystem )
{
	m_pName = "";
	m_bVisible = true;
	m_bCanAcceptInput = false;
	m_hScheme = 0;
	m_pGameStage = NULL;

	// All menus currently default so that
	// if mouse focus changes then keyboard focus will match it.
	// When keyboard focus changes in response to mouse focus we do not play any anims or send any 
	// script commands.
	// This makes it so one hitarea in the menu has input focus at a time.
	// If a menu needs them to be separate we can add a script command to turn this off.
	bMouseFocusEqualsKeyboardFocus = true;
}

CGameUIDefinition::~CGameUIDefinition()
{
	Shutdown();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameUIDefinition::Shutdown()
{
	int nGroups = m_Groups.Count();
	for ( int i = 0; i < nGroups; ++i )
	{	
		if ( m_Groups[i] ) 
		{
			delete m_Groups[i];
			m_Groups[i] = NULL;
		}
	}
	m_Groups.RemoveAll();

	int nLayers = m_Layers.Count();
	for ( int i = 0; i < nLayers; ++i )
	{	
		m_Layers[i]->Shutdown();
		delete m_Layers[i];
		m_Layers[i] = NULL;
	}
	m_Layers.RemoveAll();

	int nScripts = m_Scripts.Count();
	for ( int i = 0; i < nScripts; ++i )
	{	
		m_Scripts[i]->Shutdown();
		delete m_Scripts[i];
		m_Scripts[i] = NULL;
	}
	m_Scripts.RemoveAll();
}


//-----------------------------------------------------------------------------
// Creates an empty GameUI, just has one layer in it. No graphics.
//-----------------------------------------------------------------------------
bool CGameUIDefinition::CreateDefault( const char *pName )
{
	m_pName = pName;

	const char *pSchemeName = "resource\\ClientScheme.res";
	m_hScheme = g_pGameUISchemeManager->LoadSchemeFromFile( pSchemeName, "nouiloadedscheme" );


	// Static graphics
	CGameLayer *pGameLayer = new CGameLayer;
	pGameLayer->SetLayerType( SUBLAYER_STATIC );
	m_Layers.AddToTail( pGameLayer );

	// Dynamic graphics.
	pGameLayer = new CGameLayer;
	pGameLayer->SetLayerType( SUBLAYER_DYNAMIC );
	m_Layers.AddToTail( pGameLayer );

	// Font graphics
	pGameLayer = new CGameLayer;
	pGameLayer->SetLayerType( SUBLAYER_FONT);
	m_Layers.AddToTail( pGameLayer );


	// Create a default stage
	m_pGameStage = new CGameStage;
	m_Groups.InsertBefore( 0, m_pGameStage );

	m_bCanAcceptInput = false;

	return true;
}


//-----------------------------------------------------------------------------
// Load in data from file
//-----------------------------------------------------------------------------
bool CGameUIDefinition::Unserialize( CDmxElement *pElement )
{
	if ( Q_stricmp( pElement->GetTypeString(), "VguiCompiledDoc" ) )
	{
		return false;
	}

	pElement->UnpackIntoStructure( this, s_GameUIDefinitionUnpack );

	const char *pSchemeName = pElement->GetValueString( "scheme" );
	IGameUIScheme *pDefaultScheme = g_pGameUISchemeManager->GetDefaultScheme();
	m_hScheme = g_pGameUISchemeManager->GetScheme( pSchemeName );
	if ( m_hScheme == pDefaultScheme )
	{
		// It fell back to the default so didn't find it.
		m_hScheme = g_pGameUISchemeManager->LoadSchemeFromFile( pSchemeName, pSchemeName );
		Assert( m_hScheme );
	}
	
	g_pGameUISystemMgrImpl->SetScheme( m_hScheme );

	CDmxAttribute *pLayers = pElement->GetAttribute( "layers" );
	if ( !pLayers || pLayers->GetType() != AT_ELEMENT_ARRAY )
	{
		return false;
	}

	// Create dynamic image mapping.
	CDmxAttribute *pImageListAttr = pElement->GetAttribute( "dynamicimagelist" );
	if ( pImageListAttr )
	{
		const CUtlVector< CUtlString > &imageList = pImageListAttr->GetArray< CUtlString >( );
		for ( int i = 0; i < imageList.Count(); ++i )
		{
			const char *pAlias = imageList[i].Get();
			g_pGameUISystemMgrImpl->LoadImageAliasTexture( pAlias, "" );	
		}
	}

	// Map graphics to their DMX Element.
	CUtlDict< CGameGraphic *, int > unserializedGraphicMapping;

	const CUtlVector< CDmxElement * > &layers = pLayers->GetArray< CDmxElement * >( );
	int nCount = layers.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( layers[i]->GetTypeString(), "DmeCompiledSubLayer" ) )
		{
			if ( !UnserializeLayer( layers[i], unserializedGraphicMapping ) )
			{
				return false;
			}
		}
	}

	// Add default layers, these layer will contain anything created from scripting.

	// Static graphics
	CGameLayer *pGameLayer = new CGameLayer;
	pGameLayer->SetLayerType( SUBLAYER_STATIC );
	m_Layers.AddToTail( pGameLayer );

	// Dynamic graphics.
	pGameLayer = new CGameLayer;
	pGameLayer->SetLayerType( SUBLAYER_DYNAMIC );
	m_Layers.AddToTail( pGameLayer );

	// Font graphics
	pGameLayer = new CGameLayer;
	pGameLayer->SetLayerType( SUBLAYER_FONT);
	m_Layers.AddToTail( pGameLayer );


	// Groups
	CDmxAttribute *pGroups = pElement->GetAttribute( "groups" );
	if ( !pGroups )
	{
		return true;
	}

	if ( pGroups->GetType() != AT_ELEMENT_ARRAY )
	{
		return false;
	}

	const CUtlVector< CDmxElement * > &groups = pGroups->GetArray< CDmxElement * >( );
	nCount = groups.Count();
	if ( nCount == 0 )
	{
		Msg( "Error: No stage found" );
		return false; // no stage would be fail.
	}

	

	bool bStageFound = false;
	for ( int i = nCount-1; i >= 0; --i )
	{
		if ( !Q_stricmp( groups[i]->GetTypeString(), "DmeCompiledStage" ) )
		{
			bStageFound = true;
			break;
		}
	}

	if ( !bStageFound )
	{
		return false; // no stage would be fail.
	}


	// Add groups to the graphic mapping so groups can contain other groups.
	// This means the groups get created and put in the list before they get unserialized.
	// Groups should always be in order parents before children.
	// This makes sure parents update before children in update loops
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( groups[i]->GetTypeString(), "DmeCompiledGroup" ) )
		{
			CGraphicGroup *pGraphicGroup = new CGraphicGroup;
			char pBuf[255];
			UniqueIdToString( groups[i]->GetId(), pBuf, 255 );
			unserializedGraphicMapping.Insert( pBuf, pGraphicGroup );
			m_Groups.AddToTail( pGraphicGroup );
		}
		else if ( !Q_stricmp( groups[i]->GetTypeString(), "DmeCompiledStage" ) )
		{
			Assert( i == 0 );
			CGameStage *pGraphicGroup = new CGameStage;
			m_Groups.InsertBefore( 0, pGraphicGroup );
			m_pGameStage = pGraphicGroup;
		}
	}

	// Now unserialize the groups
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( groups[i]->GetTypeString(), "DmeCompiledGroup" ) )
		{
			if ( !m_Groups[i]->Unserialize( groups[i], unserializedGraphicMapping ) )
			{
				return false;
			}
		}
		else if ( !Q_stricmp( groups[i]->GetTypeString(), "DmeCompiledStage" ) )
		{
			Assert( i == 0 );
			CGameStage *pStage = (CGameStage *)m_Groups[i];
			if ( !pStage->Unserialize( groups[i], unserializedGraphicMapping ) )
			{
				return false;
			}
		}
		else
		{
			Assert(0); // something is in the group list that isn't a group!
		}
	}

	return true;	
}

//-----------------------------------------------------------------------------
// Load in data from file for a layer
//-----------------------------------------------------------------------------
bool CGameUIDefinition::UnserializeLayer( CDmxElement *pLayer, 
										 CUtlDict< CGameGraphic *, int > &unserializedGraphicMapping )
{
	// Static graphics
	CDmxAttribute *pGraphics = pLayer->GetAttribute( "staticGraphics" );

	// The layer may have no static graphics ( it could be only fonts! )
	if ( pGraphics && pGraphics->GetType() == AT_ELEMENT_ARRAY )
	{
		CDmxAttribute *pGraphics = pLayer->GetAttribute( "staticGraphics" );
		const CUtlVector< CDmxElement * > &graphics = pGraphics->GetArray< CDmxElement * >( );
		if ( graphics.Count() != 0 )
		{
			CGameLayer *pGameLayer = new CGameLayer;
			pGameLayer->SetLayerType( SUBLAYER_STATIC );
			pGameLayer->Unserialize( pLayer, unserializedGraphicMapping );
			m_Layers.AddToTail( pGameLayer );
		}	
	}


	// repeat above for dynamic graphics.
	// The layer may have no dynamic graphics 
	if ( pGraphics && pGraphics->GetType() == AT_ELEMENT_ARRAY )
	{
		CDmxAttribute *pGraphics = pLayer->GetAttribute( "dynamicGraphics" );
		const CUtlVector< CDmxElement * > &graphics = pGraphics->GetArray< CDmxElement * >( );
		if ( graphics.Count() != 0 )
		{
			CGameLayer *pGameLayer = new CGameLayer;
			pGameLayer->SetLayerType( SUBLAYER_DYNAMIC );
			pGameLayer->Unserialize( pLayer, unserializedGraphicMapping );
			m_Layers.AddToTail( pGameLayer );
		}	
	}


	// Font graphics
	pGraphics = pLayer->GetAttribute( "fontGraphics" );

	// The layer may have no font graphics 
	if ( pGraphics && pGraphics->GetType() == AT_ELEMENT_ARRAY )
	{
		CDmxAttribute *pGraphics = pLayer->GetAttribute( "fontGraphics" );
		const CUtlVector< CDmxElement * > &graphics = pGraphics->GetArray< CDmxElement * >( );
		if ( graphics.Count() != 0 )
		{
			CGameLayer *pGameLayer = new CGameLayer;
			pGameLayer->SetLayerType( SUBLAYER_FONT );
			pGameLayer->Unserialize( pLayer, unserializedGraphicMapping );
			m_Layers.AddToTail( pGameLayer );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Path is always vguiedit\menuname.lua
//-----------------------------------------------------------------------------
void CGameUIDefinition::InitializeScripts()
{
	// First time?
	if ( m_Scripts.Count() == 0 )
	{
		CFmtStr path[2];
		path[0].sprintf( "vguiedit\\%s.lua", m_pName.Get() );
		path[1].sprintf( "vguiedit\\%s\\%s.lua", m_pName.Get(), m_pName.Get() );

		for ( int k = 0; k < ARRAYSIZE( path ); ++ k )
		{
			CFmtStr scriptPath;
			scriptPath.sprintf( "scripts\\%s", path[k].Access() );

			// Does this menu have a script? 
			if ( g_pFullFileSystem->FileExists( scriptPath, "MOD" ) )
			{
				CGameUIScript *pScript = new CGameUIScript();
				bool bResult = pScript->SetScript( path[k], this );
				if ( bResult )
				{
					m_Scripts.AddToTail( pScript );
					DevMsg( "Loaded script %s for %s\n", scriptPath.Access(), m_pName.Get() );
					break;	// break as soon as the first script is loaded successfully
				}
				else
				{
					Warning( "Invalid script %s for %s\n", scriptPath.Access(), m_pName.Get() );	
					delete pScript;
				}
			}
			else
			{
				DevMsg( "No default script %s found for %s\n", scriptPath.Access(), m_pName.Get() );
			}
		}
	}

	SetAcceptInput( false );
}


//-----------------------------------------------------------------------------
// Execute a script function
// The function to call is denoted by executionType
//-----------------------------------------------------------------------------
bool CGameUIDefinition::ExecuteScript( KeyValues *args, KeyValues **ppResult )
{
	Assert( !ppResult || !*ppResult );	// storing return value, might overwrite caller's keyvalues

	//////////////////////////////////////////////////////////////////////////
	// TEMP: special events for unit-tests state control
	char const *szEvent = args->GetName();

	if ( !Q_stricmp( "AdvanceState", szEvent ) )
	{
		AdvanceState();
		return true;
	}
	
	if ( !Q_stricmp( "StartPlaying", szEvent ) )
	{
		StartPlaying();
		return true;
	}

	if ( !Q_stricmp( "StopPlaying", szEvent ) )
	{
		StopPlaying();
		return true;
	}
	if ( !Q_stricmp( "ShowCursorCoords", szEvent ) )
	{
		g_pGameUISystemMgrImpl->ShowCursorCoords();
		return true;
	}
	if ( !Q_stricmp( "ShowGraphicName", szEvent ) )
	{
		g_pGameUISystemMgrImpl->ShowGraphicName();
		return true;
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Graphic access
	if ( !Q_stricmp( "FindGraphic", szEvent ) )
	{
		CGameGraphic *pGraphic = FindGraphicByName( args->GetString( "graphic" ) );
		if ( pGraphic && ppResult )
		{
			*ppResult = new KeyValues( "" );
			(*ppResult)->SetInt( "graphichandle", pGraphic->GetScriptHandle() );
		}
		return true;
	}

	if ( !Q_stricmp( "SetInput", szEvent ) )
	{
		SetAcceptInput( args->GetBool( "input", true ) );
		return true;
	}

	if ( !Q_stricmp( "InitAnims", szEvent ) )
	{
		InitAnims();
		return true;
	}

	if ( !Q_stricmp( "Sound", szEvent ) )
	{
		if ( args->GetBool( "play", true ) )
			g_pGameUISystemMgrImpl->PlayMenuSound( args->GetString( "sound" ) );
		else
			g_pGameUISystemMgrImpl->StopMenuSound( args->GetString( "sound" ) );
	}
	//////////////////////////////////////////////////////////////////////////
	if ( !Q_stricmp( "setdynamictexture", szEvent ) )
	{
		g_pGameUISystemMgrImpl->LoadImageAliasTexture( args->GetString( "aliasname", "" ), args->GetString( "texturename", "" ) );
	}


	bool bExecuted = false;
	if ( m_Scripts.Count() != 0 &&  m_Scripts[ 0 ] )
	{
		m_Scripts[ 0 ]->SetActive( true );	
		bExecuted = m_Scripts[ 0 ]->Execute( args, ppResult );
		m_Scripts[ 0 ]->SetActive( false );
	}

	if ( !bExecuted	)
	{
		// If the menu doesn't handle its exit just hide it.
		if ( !Q_stricmp( szEvent, "OnExit" ) )
		{
			SetVisible( false );
			return true;
		}
	}

	return bExecuted;
}

//-----------------------------------------------------------------------------
// Return the number of layers in this menu
//-----------------------------------------------------------------------------
int CGameUIDefinition::GetLayerCount()
{
	return m_Layers.Count();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameUIDefinition::InvalidateSheetSymbols()
{
	int nCount = m_Layers.Count();
	for ( int j = 0; j < nCount; ++j )
	{
		m_Layers[j]->InvalidateSheetSymbol();
	}
}


//-----------------------------------------------------------------------------
//	Update from front to back so input consequences go right in.
//-----------------------------------------------------------------------------
void CGameUIDefinition::UpdateGeometry()
{
	m_pGameStage->UpdateGeometry();
}


//-----------------------------------------------------------------------------
// Update the render to screen matrices of all graphics
// Children use thier parent's viewport/matrix and build off that.
//-----------------------------------------------------------------------------
void CGameUIDefinition::UpdateRenderTransforms( const Rect_t &viewport )
{
	m_pGameStage->UpdateRenderTransforms( viewport );
}

//-----------------------------------------------------------------------------
// Get lists of rendering data needed to draw the ui.
// Each layer has a different type, static, dynamic, and font
// a new render list is created whenever the layer type changes
// and when the texture needed to render changes.
// Layers live in sets of 3's, static, dynamic and font.
// This preserves render order from the editor.
//-----------------------------------------------------------------------------
void CGameUIDefinition::GetRenderData( CUtlVector< LayerRenderLists_t > &renderLists )
{
	int nLayers = m_Layers.Count();
	for ( int i = 0; i < nLayers; ++i )
	{ 
		color32 stageColor = m_pGameStage->GetStageColor();
		m_Layers[i]->UpdateRenderData( *this, stageColor, renderLists );
	}
}


//-----------------------------------------------------------------------------
// Given a position, return the front most graphic under it.
//-----------------------------------------------------------------------------
CGameGraphic *CGameUIDefinition::GetGraphic( int x, int y )
{
	int nLayers = m_Layers.Count();
	for ( int i = nLayers-1; i >= 0; --i )
	{ 
		CGameGraphic *pGraphic = ( CGameGraphic * )m_Layers[i]->GetGraphic( x, y );
		if ( pGraphic )
		{
			return pGraphic;
		}		
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Given a position, return the front most graphic that can take input under it.
//-----------------------------------------------------------------------------
CHitArea *CGameUIDefinition::GetMouseFocus( int x, int y )
{
	if ( CanAcceptInput() )
	{
		int nLayers = m_Layers.Count();
		for ( int i = nLayers-1; i >= 0; --i )
		{ 
			CHitArea *pGraphic = ( CHitArea * )m_Layers[i]->GetMouseFocus( x, y );
			if ( pGraphic )
			{
				return pGraphic;
			}		
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Given a graphic, return the next graphic that can receive focus.
// Note the default focus order is just the render order.
//-----------------------------------------------------------------------------
CHitArea *CGameUIDefinition::GetNextFocus( CHitArea *pCurrentGraphic )
{	
	if ( !CanAcceptInput() )
		return NULL; 

	bool bGetNext = false;

	if ( pCurrentGraphic == NULL )
	{
		bGetNext = true;
	}

	int nLayers = m_Layers.Count();
	for ( int i = 0; i < nLayers; ++i )
	{ 
		CHitArea *pGraphic = ( CHitArea * )m_Layers[i]->GetNextFocus( bGetNext, pCurrentGraphic );
		if ( pGraphic )
		{
			return pGraphic;
		}
	}

	// We found no next one, we must be at the end then, restart from the back.
	bGetNext = true;
	for ( int i = 0; i < nLayers; ++i )
	{
		CHitArea *pGraphic = ( CHitArea * )m_Layers[i]->GetNextFocus( bGetNext, pCurrentGraphic );
		if ( pGraphic )
		{
			return pGraphic;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
//	Start playing animations
//-----------------------------------------------------------------------------
void CGameUIDefinition::StartPlaying()
{
	m_pGameStage->StartPlaying();

	int nCount = m_Layers.Count();
	for ( int j = 0; j < nCount; ++j )
	{
		m_Layers[j]->StartPlaying();
	}
}


//-----------------------------------------------------------------------------
// Stop playing animations
//-----------------------------------------------------------------------------
void CGameUIDefinition::StopPlaying()
{
	m_pGameStage->StopPlaying();

	int nCount = m_Layers.Count();
	for ( int j = 0; j < nCount; ++j )
	{
		m_Layers[j]->StopPlaying();
	}
}

//-----------------------------------------------------------------------------
// Move to the next known animation state
//-----------------------------------------------------------------------------
void CGameUIDefinition::AdvanceState()
{
	m_pGameStage->AdvanceState();

	int nCount = m_Layers.Count();
	for ( int j = 0; j < nCount; ++j )
	{
		m_Layers[j]->AdvanceState();
	}
}

//-----------------------------------------------------------------------------
// Set all graphics to "default" state.
//-----------------------------------------------------------------------------
void CGameUIDefinition::InitAnims()
{
	m_pGameStage->SetState( "default" );

	int nCount = m_Layers.Count();
	for ( int j = 0; j < nCount; ++j )
	{
		m_Layers[j]->InitAnims();
	}
}

//-----------------------------------------------------------------------------
// Given a graphic, build its scoped name.
//-----------------------------------------------------------------------------
void CGameUIDefinition::BuildScopedGraphicName( CUtlString &name, CGameGraphic *pGraphic )
{ 
	if ( !pGraphic->GetGroup()->IsStageGroup() )
	{
		BuildScopedGraphicName( name, pGraphic->GetGroup() );
	}
	name += pGraphic->GetName();
	if ( pGraphic->IsGroup() )
	{
		name += ":";
	}
}


//-----------------------------------------------------------------------------
// Given a scoped name of a graphic, return it if it exists in this menu
//-----------------------------------------------------------------------------
CGameGraphic *CGameUIDefinition::GraphicExists( const char *pName ) const
{
	if ( m_pGameStage->IsGraphicNamed( pName ) )
		return m_pGameStage;

	CSplitString nameparts( pName, ":");

	CGameGraphic *parent = m_pGameStage;
	CGameGraphic *pGraphic = NULL;
	for ( int i = 0; i < nameparts.Count(); ++i )
	{
		pGraphic = parent->FindGraphicByName( nameparts[i] );
		if ( pGraphic )
		{
			parent = pGraphic;
		}
		else
		{
			return NULL; // couldn't find it.
		}		
	}

	return pGraphic;
}

//-----------------------------------------------------------------------------
// Given a scoped name of a graphic, find it in this menu.
// This function expects to find the graphic
//-----------------------------------------------------------------------------
CGameGraphic *CGameUIDefinition::FindGraphicByName( const char *pName ) const
{ 
	CGameGraphic *pGraphic = GraphicExists( pName );
	if ( pGraphic )
	{
		return pGraphic;
	}
	else
	{
		Warning( "FindGraphicByName: Unable to find graphic named %s\n", pName );
		return NULL; // couldn't find it.
	}	
}


//-----------------------------------------------------------------------------
// Add a text graphic to the front most layer of a given type
//-----------------------------------------------------------------------------
bool CGameUIDefinition::AddGraphicToLayer( CGameGraphic *pGraphic, int nLayerType )
{
	pGraphic->SetGroup( m_pGameStage );
	m_pGameStage->AddToGroup( pGraphic );

	// Find the frontmost player of this type.
	int nFrontLayer = -1;
	for ( int i = m_Layers.Count()-1; i >= 0; --i )
	{
		if ( m_Layers[i]->GetLayerType() == nLayerType )
		{
			nFrontLayer = i;
			break;
		}
	}

	if ( nFrontLayer != -1 )
	{
		AddGraphic( pGraphic, nFrontLayer );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Add a graphic to the given layer 
//-----------------------------------------------------------------------------
void CGameUIDefinition::AddGraphic( CGameGraphic *pGraphic, int layerIndex )
{
	Assert( layerIndex >= 0 );
	Assert (layerIndex < m_Layers.Count() );
	
	m_Layers[layerIndex]->AddGraphic( pGraphic );
}

//-----------------------------------------------------------------------------
//   Remove this graphic from the UI
//-----------------------------------------------------------------------------
bool CGameUIDefinition::RemoveGraphic( CGameGraphic *pGraphic )
{
	for ( int i = 0; i < m_Layers.Count(); ++i )
	{
		if ( m_Layers[i]->RemoveGraphic( pGraphic )	)
		{
			return true;	
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Return true if this graphic is in this menu
//-----------------------------------------------------------------------------
bool CGameUIDefinition::HasGraphic( CGameGraphic *pGraphic )
{
	for ( int i = 0; i < m_Layers.Count(); ++i )
	{
		if ( m_Layers[i]->HasGraphic( pGraphic )	)
		{
			return true;	
		}
	}

	return false;


}


//-----------------------------------------------------------------------------
// Set visibility of the gameui.
//-----------------------------------------------------------------------------
void CGameUIDefinition::SetVisible( bool bVisible )
{
	m_bVisible = bVisible;
	// Visibility can cause a focus change to be needed!
	g_pGameUISystemMgrImpl->ForceFocusUpdate();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUIDefinition::UpdateAspectRatio( const Rect_t &viewport )
{ 
	m_pGameStage->UpdateAspectRatio( viewport ); 
}

//-----------------------------------------------------------------------------
// Change the size of the stage.	
//-----------------------------------------------------------------------------
void CGameUIDefinition::SetStageSize( int nWide, int nTall )
{
	m_pGameStage->SetStageSize( nWide, nTall );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUIDefinition::GetStageSize( Vector2D &stageSize ) 
{ 
	m_pGameStage->GetStageSize( stageSize ); 
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUIDefinition::GetMaintainAspectRatioStageSize( Vector2D &stageSize )
{ 
	m_pGameStage->GetMaintainAspectRatioStageSize( stageSize ); 
}







