//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "gametext.h"
#include "vgui/ilocalize.h"
#include "vgui/vgui.h"
#include <ctype.h>
#include "gameuisystemsurface.h"
#include "gameuisystemmgr.h"
#include "gameuischeme.h"
#include "graphicgroup.h"
#include "gameuidefinition.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Class factory for scripting.
class CGameTextClassFactory : IGameUIGraphicClassFactory
{
public:

	CGameTextClassFactory()
	{
		Assert( g_pGameUISystemMgrImpl );
		g_pGameUISystemMgrImpl->RegisterGraphicClassFactory( "text", this );
	}

	// Returns an instance of a graphic interface (keyvalues owned by caller)
	virtual CGameGraphic *CreateNewGraphicClass( KeyValues *kvRequest, CGameUIDefinition *pMenu )
	{
		Assert( pMenu );
		CGameText *pNewGraphic = NULL;

		const char *pName = kvRequest->GetString( "name", NULL );
		if ( pName )
		{
			pNewGraphic = new CGameText( pName );
			pMenu->AddGraphicToLayer( pNewGraphic, SUBLAYER_FONT );

			// Now set the attributes.
			for ( KeyValues *arg = kvRequest->GetFirstSubKey(); arg != NULL; arg = arg->GetNextKey() )
			{
				pNewGraphic->HandleScriptCommand( arg );	
			}
		}
		return pNewGraphic;	
	}
};
static CGameTextClassFactory g_CGameTextClassFactory;


BEGIN_DMXELEMENT_UNPACK ( CGameText ) 
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "name", "", m_pName ) 
	DMXELEMENT_UNPACK_FIELD( "center", "0 0", Vector2D, m_Geometry.m_Center ) 
	DMXELEMENT_UNPACK_FIELD( "scale", "1 1", Vector2D, m_Geometry.m_Scale ) 
	DMXELEMENT_UNPACK_FIELD( "rotation", "0", float, m_Geometry.m_Rotation )  	
	DMXELEMENT_UNPACK_FIELD( "maintainaspectratio", "0", bool, m_Geometry.m_bMaintainAspectRatio )
	DMXELEMENT_UNPACK_FIELD( "sublayertype", "0", int, m_Geometry.m_Sublayer )
	DMXELEMENT_UNPACK_FIELD( "visible", "1", bool, m_Geometry.m_bVisible )
	DMXELEMENT_UNPACK_FIELD( "initialstate", "-1", int, m_CurrentState )

	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "unlocalizedtext", "NONE", m_CharText )
	DMXELEMENT_UNPACK_FIELD( "allcaps", "0", bool, m_bAllCaps )
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "fontname", "Default", m_FontName )
	DMXELEMENT_UNPACK_FIELD( "justfication", "0", int, m_Justification )
	DMXELEMENT_UNPACK_FIELD( "color", "255 255 255 255", Color, m_Geometry.m_Color )
	DMXELEMENT_UNPACK_FIELD( "topcolor", "255 255 255 255", Color, m_Geometry.m_TopColor )
	DMXELEMENT_UNPACK_FIELD( "bottomcolor", "255 255 255 255", Color, m_Geometry.m_BottomColor )
	DMXELEMENT_UNPACK_FIELD( "horizgradient", "0", bool, m_Geometry.m_bHorizontalGradient )

	// color is gotten from log.
	
END_DMXELEMENT_UNPACK( CGameText, s_GameTextUnpack )


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CGameText::CGameText( const char *pName )
{
	m_UnicodeText = NULL;
	m_TextBufferLen = 0;
	m_UnlocalizedTextSymbol = vgui::INVALID_STRING_INDEX;
	m_Font = vgui::INVALID_FONT;
	m_bCanAcceptInput = false;

	// DME default values.
	m_pName = pName;
	m_Geometry.m_Center.x = 0;
	m_Geometry.m_Center.y = 0;
	m_Geometry.m_Scale.x = 1;
	m_Geometry.m_Scale.y = 1;
	m_Geometry.m_Rotation = 0;
	m_Geometry.m_bMaintainAspectRatio = 1;
	m_Geometry.m_Sublayer = 0;
	m_Geometry.m_bVisible = true;
	m_CurrentState = -1;
	m_CharText = "NONE";
	m_bAllCaps = false;
	m_FontName = "Default";
	m_Justification = JUSTIFICATION_LEFT;
	m_Geometry.m_Color.r = 255;
	m_Geometry.m_Color.g = 255;
	m_Geometry.m_Color.b = 255;
	m_Geometry.m_Color.a = 255;
	m_Geometry.m_TopColor.r = 255;
	m_Geometry.m_TopColor.g = 255;
	m_Geometry.m_TopColor.b = 255;
	m_Geometry.m_TopColor.a = 255;
	m_Geometry.m_BottomColor.r = 255;
	m_Geometry.m_BottomColor.g = 255;
	m_Geometry.m_BottomColor.b = 255;
	m_Geometry.m_BottomColor.a = 255;
	m_Geometry.m_bHorizontalGradient = false;

	SetText( m_CharText );
	SetFont( m_FontName );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CGameText::~CGameText()
{
	if ( m_UnicodeText )
	{
		delete[] m_UnicodeText;
		m_UnicodeText = NULL;
	}
};


//-----------------------------------------------------------------------------
// Create game text using dme elements
//-----------------------------------------------------------------------------
bool CGameText::Unserialize( CDmxElement *pGraphic )
{
	pGraphic->UnpackIntoStructure( this, s_GameTextUnpack );

	// GEOMETRY
	// Geometry for text is generated. This is because you don't know it until you know the localized text.

	// ANIMSTATES
	CDmxAttribute *pImageAnims = pGraphic->GetAttribute( "imageanims" );
	if ( !pImageAnims || pImageAnims->GetType() != AT_ELEMENT_ARRAY )
    {
		return false;
    }
	const CUtlVector< CDmxElement * > &imageanims = pImageAnims->GetArray< CDmxElement * >( );
	int nCount = imageanims.Count();
    for ( int i = 0; i < nCount; ++i )
    {
		CAnimData *pAnimData = new CAnimData;
		if ( !pAnimData->Unserialize( imageanims[i] ) )
		{
			delete pAnimData;
			return false;
		}
		m_Anims.AddToTail( pAnimData );
		
    }

	SetText( m_CharText );
	SetFont( m_FontName );

	SetState( "default" );

	return true;
}

//-----------------------------------------------------------------------------
// The attributes that can be modified by scripting are a subset of the DME ones.
//-----------------------------------------------------------------------------
KeyValues *CGameText::HandleScriptCommand( KeyValues *args )
{
	char const *szCommand = args->GetName();

	if ( !Q_stricmp( "SetText", szCommand ) )
	{
		const char *text = args->GetString( "text" );
		SetText( text );
		return NULL;
	}
	else if ( !Q_stricmp( "SetAllCaps", szCommand ) )
	{
		m_bAllCaps = args->GetBool( "allcaps", false );
		return NULL;
	}
	else if ( !Q_stricmp( "SetFont", szCommand ) )
	{
		SetFont( args->GetString( "fontname" ) );
		return NULL;
	}
	else if ( !Q_stricmp( "SetJustification", szCommand ) )
	{
		// FIXME
		m_Justification = args->GetInt( "justfication", 0 );
		return NULL;
	}
	else if ( !Q_stricmp( "SetTopColor", szCommand ) )
	{
		Color c = args->GetColor( "color", Color( 255, 255, 255, 255 ) );
		m_Geometry.m_TopColor.r = c[0];
		m_Geometry.m_TopColor.g = c[1];
		m_Geometry.m_TopColor.b = c[2];
		m_Geometry.m_TopColor.a = c[3];
		return NULL;
	}
	else if ( !Q_stricmp( "SetBottomColor", szCommand ) )
	{
		Color c = args->GetColor( "color", Color( 255, 255, 255, 255 ) );
		m_Geometry.m_BottomColor.r = c[0];
		m_Geometry.m_BottomColor.g = c[1];
		m_Geometry.m_BottomColor.b = c[2];
		m_Geometry.m_BottomColor.a = c[3];
		return NULL;
	}
	
	
	else if ( !Q_stricmp( "GetFont", szCommand ) )
	{
		return new KeyValues( "", "font", m_FontName.Get() );
	}

	return CGameGraphic::HandleScriptCommand( args );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameText::SetFont( const char *pFontName )
{
	m_FontName = pFontName;
	if ( m_FontName.Length() )
	{
		m_Font = g_pGameUISystemMgrImpl->GetCurrentScheme()->GetFont( m_FontName, true );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameText::SetFont( FontHandle_t font )
{
	m_Font = font;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
FontHandle_t CGameText::GetFont()
{
	return m_Font;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameText::SetJustification( Justification_e justify )
{
	m_Justification = justify;
}

//-----------------------------------------------------------------------------
// Purpose: takes the string and looks it up in the localization file to convert it to unicode
//-----------------------------------------------------------------------------
void CGameText::SetText( const char *text )
{
	if ( !text )
	{
		text = "";
	}

	// check for localization
	if ( *text == '#' )
	{
		// try lookup in localization tables
		m_UnlocalizedTextSymbol = g_pVGuiLocalize->FindIndex( text + 1 );
		
		if ( m_UnlocalizedTextSymbol != vgui::INVALID_STRING_INDEX )
		{
			wchar_t *unicode = g_pVGuiLocalize->GetValueByIndex( m_UnlocalizedTextSymbol );
			SetText(unicode);
			return;
		}
	}
	

	// convert the ansi string to unicode and use that
	wchar_t unicode[1024];
	g_pVGuiLocalize->ConvertANSIToUnicode( text, unicode, sizeof(unicode) );
	SetText( unicode );
}

//-----------------------------------------------------------------------------
// Purpose: sets unicode text directly
//-----------------------------------------------------------------------------
void CGameText::SetText( const wchar_t *unicode, bool bClearUnlocalizedSymbol )
{
	if ( bClearUnlocalizedSymbol )
	{
		// Clear out unlocalized text symbol so that changing dialog variables
		// doesn't stomp over the custom unicode string we're being set to.
		m_UnlocalizedTextSymbol = vgui::INVALID_STRING_INDEX;
	}

	if (!unicode)
	{
		unicode = L"";
	}

	// reallocate the buffer if necessary
	short textLen = (short)wcslen( unicode );
	if ( textLen >= m_TextBufferLen )
	{
		if ( m_UnicodeText )
		{
			delete [] m_UnicodeText;
			m_UnicodeText = NULL;
		}
		m_TextBufferLen = (short)( textLen + 1 );
		m_UnicodeText = new wchar_t[ m_TextBufferLen ];
	}

	// store the text as unicode
	wcscpy( m_UnicodeText, unicode );
	SetupVertexColors();
}

//-----------------------------------------------------------------------------
//			  
//-----------------------------------------------------------------------------
void CGameText::UpdateGeometry()
{
	if ( m_CurrentState == -1 )
		return;

	Assert( m_CurrentState < m_Anims.Count() );
	
	DmeTime_t flAnimTime = GetAnimationTimePassed();

	// Update color
	m_Anims[ m_CurrentState ]->m_ColorAnim.GetValue( flAnimTime, &m_Geometry.m_Color );
		
	// Update center location
	m_Anims[ m_CurrentState ]->m_CenterPosAnim.GetValue( flAnimTime, &m_Geometry.m_Center );

	// Update scale
	m_Anims[ m_CurrentState ]->m_ScaleAnim.GetValue( flAnimTime, &m_Geometry.m_Scale );

	// Update rotation
	m_Anims[ m_CurrentState ]->m_RotationAnim.GetValue( flAnimTime, &m_Geometry.m_Rotation );

	// Update rotation
	m_Anims[ m_CurrentState ]->m_FontAnim.GetValue( flAnimTime, &m_FontName );
	SetFont( m_FontName );
}


//-----------------------------------------------------------------------------
// Rendering helper
// For text rendering the starting position is top left
// Center text according to justification.
//-----------------------------------------------------------------------------
void CGameText::GetStartingTextPosition( int &x, int &y )
{
	x = 0;
	y = -GetTextRenderHeight()/2;

	switch ( m_Justification )
	{
	case JUSTIFICATION_LEFT:
		break;
	case JUSTIFICATION_CENTER:
		x = -GetTextRenderWidth()/2;
		break;
	case JUSTIFICATION_RIGHT:
		x = -GetTextRenderWidth();
		break;
	default:
		Assert(0);
		break;
	}
}

//-----------------------------------------------------------------------------
// Rendering helper
// Determine what list to put this quad into, and make an entry slot for it.
//-----------------------------------------------------------------------------
CRenderGeometry *CGameText::GetGeometryEntry( CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex, int fontTextureID )
{	
	CRenderGeometry *pRenderGeometry = NULL;
	if ( renderGeometryLists[firstListIndex].Count() != 0 )
	{ 
		for ( int j = firstListIndex; j < renderGeometryLists.Count(); ++j )
		{
			if ( fontTextureID == renderGeometryLists[j][0].m_FontTextureID )
			{
				int index = renderGeometryLists[j].AddToTail();
				pRenderGeometry = &renderGeometryLists[j][index];
				break;
			}
		}
	}

	// Didn't find a match for this textureID, time to make a new list of quads for this texture.
	if ( pRenderGeometry == NULL )
	{   
		int newListIndex;
		if ( renderGeometryLists[firstListIndex].Count() == 0 )	// This is the first quad we are adding to this font layer.
		{
			newListIndex = firstListIndex;
		}
		else
		{
			newListIndex = renderGeometryLists.AddToTail();
		}
		int index = renderGeometryLists[newListIndex].AddToTail();
		pRenderGeometry = &renderGeometryLists[newListIndex][index];
	}
	
	return pRenderGeometry;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameText::SetupVertexColors()
{
	// There is no text to generate if these are not set.
	if ( !m_UnicodeText )
		return;

	m_Geometry.m_VertexColors.RemoveAll();

	color32 c;
	c.r = 255;
	c.g = 255;
	c.b = 255;
	c.a = 255;

	for ( wchar_t *wsz = m_UnicodeText; *wsz != 0; wsz++ )
	{
		// Create 4 vertex colors per letter.
		m_Geometry.m_VertexColors.AddToTail( c );
		m_Geometry.m_VertexColors.AddToTail( c );
		m_Geometry.m_VertexColors.AddToTail( c );
		m_Geometry.m_VertexColors.AddToTail( c );
	}

}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameText::UpdateRenderData( color32 parentColor, CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex )
{
	if ( !m_Geometry.m_bVisible )
		return;

	// There is no text to generate if these are not set.
	if ( !m_UnicodeText )
		return;

	if ( m_Font == vgui::INVALID_FONT)
		return;

	m_Geometry.SetResultantColor( parentColor );

	int x, y;
	GetStartingTextPosition( x, y );

	FontCharRenderInfo info;
	info.currentFont = m_Font;
	info.drawType = FONT_DRAW_DEFAULT;

	int letterIndex = 0;
	m_Geometry.m_RelativePositions.RemoveAll();
	for ( wchar_t *wsz = m_UnicodeText; *wsz != 0; wsz++, letterIndex += 4 )
	{
		Assert( letterIndex < m_Geometry.m_VertexColors.Count() );
		// Update FontCharRenderInfo
		info.x = x;
		info.y = y;
		info.ch = wsz[0];
		if ( m_bAllCaps )
		{
			info.ch = towupper( info.ch );
		}

		Vector2D relPositions[4];
		g_pGameUISystemSurface->GetUnicodeCharRenderPositions( info, relPositions );

		// get the character texture from the cache and the char's texture coords.
		float *texCoords = NULL;  // note this returns the static from the fonttexturecache... FIXME?
		g_pGameUISystemSurface->GetTextureForChar( info, &texCoords );

		x += g_pGameUISystemSurface->GetCharacterWidth( m_Font, info.ch );

		// Get a geometry from the correct texture list.
		CRenderGeometry *pRenderGeometry = GetGeometryEntry( renderGeometryLists, firstListIndex, info.textureId );
		Assert( pRenderGeometry != NULL );

		// Populate the new entry.
		pRenderGeometry->m_FontTextureID = info.textureId;
		Vector screenPosition;
		Vector relativePosition;

		// Top left
		relativePosition.Init( relPositions[0].x, relPositions[0].y, 0 );
		m_Geometry.m_RelativePositions.AddToTail( Vector2D( relPositions[0].x, relPositions[0].y ) );
		VectorTransform( relativePosition, m_Geometry.m_RenderToScreen, screenPosition );
		pRenderGeometry->m_Positions.AddToTail( Vector2D( floor( screenPosition.x ), floor( screenPosition.y ) ) );
		pRenderGeometry->m_VertexColors.AddToTail( m_Geometry.m_VertexColors[letterIndex] );
		pRenderGeometry->m_TextureCoords.AddToTail( Vector2D( texCoords[0], texCoords[1] ) );

		// Top right
		relativePosition.Init( relPositions[1].x, relPositions[1].y, 0 );
		m_Geometry.m_RelativePositions.AddToTail( Vector2D( relPositions[1].x, relPositions[1].y ) );
		VectorTransform( relativePosition, m_Geometry.m_RenderToScreen, screenPosition );
		pRenderGeometry->m_Positions.AddToTail( Vector2D( floor( screenPosition.x ), floor( screenPosition.y ) ) );
		pRenderGeometry->m_VertexColors.AddToTail( m_Geometry.m_VertexColors[letterIndex + 1] );
		pRenderGeometry->m_TextureCoords.AddToTail( Vector2D( texCoords[2], texCoords[1] ) );

		// Bottom right
		relativePosition.Init( relPositions[2].x, relPositions[2].y, 0 );
		m_Geometry.m_RelativePositions.AddToTail( Vector2D( relPositions[2].x, relPositions[2].y ) );
		VectorTransform( relativePosition, m_Geometry.m_RenderToScreen, screenPosition );
		pRenderGeometry->m_Positions.AddToTail( Vector2D( floor( screenPosition.x ), floor( screenPosition.y ) ) );
		pRenderGeometry->m_VertexColors.AddToTail( m_Geometry.m_VertexColors[letterIndex + 2] );
		pRenderGeometry->m_TextureCoords.AddToTail( Vector2D( texCoords[2], texCoords[3] ) );

		// Bottom left
		relativePosition.Init( relPositions[3].x, relPositions[3].y, 0 );
		m_Geometry.m_RelativePositions.AddToTail( Vector2D( relPositions[3].x, relPositions[3].y ) );
		VectorTransform( relativePosition, m_Geometry.m_RenderToScreen, screenPosition );
		pRenderGeometry->m_Positions.AddToTail( Vector2D( floor( screenPosition.x ), floor( screenPosition.y ) ) );
		pRenderGeometry->m_VertexColors.AddToTail( m_Geometry.m_VertexColors[letterIndex + 3] );
		pRenderGeometry->m_TextureCoords.AddToTail( Vector2D( texCoords[0], texCoords[3] ) );

		pRenderGeometry->m_AnimationRate = m_Geometry.m_AnimationRate;
		pRenderGeometry->m_AnimStartTime = m_Geometry.m_AnimStartTime;
		pRenderGeometry->m_bAnimate = m_Geometry.m_bAnimate;
		pRenderGeometry->m_pImageAlias = NULL;
	}


	m_Geometry.CalculateExtents();

	
}

//-----------------------------------------------------------------------------
// Have to do this separately because extents are drawn as rects.
//-----------------------------------------------------------------------------
void CGameText::DrawExtents( CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex )
{
	color32 extentLineColor = { 0, 25, 255, 255 };	
	m_Geometry.DrawExtents( renderGeometryLists, firstListIndex, extentLineColor );
}


//-----------------------------------------------------------------------------
// Purpose: Get the size of a text string in pixels
//-----------------------------------------------------------------------------
void CGameText::GetTextSize( int &wide, int &tall )
{
	wide = 0;
	tall = 0;

	if ( m_Font == vgui::INVALID_FONT )
		return;

	// For height, use the remapped font
	tall = GetTextRenderHeight();
	wide = GetTextRenderWidth();
}

//-----------------------------------------------------------------------------
// Return height of text in pixels
//-----------------------------------------------------------------------------
int CGameText::GetTextRenderWidth()
{
	int wordWidth = 0;
	int textLen = wcslen( m_UnicodeText );
	for ( int i = 0; i < textLen; i++ )
	{
		wchar_t ch = m_UnicodeText[ i ];
		
		if ( m_bAllCaps )
		{
			ch = towupper( ch );
		}

		// handle stupid special characters, these should be removed
		if ( ch == '&' && m_UnicodeText[ i + 1 ] != 0 )
		{
			continue;
		}

		wordWidth += g_pGameUISystemSurface->GetCharacterWidth( m_Font, ch );
	}

	return wordWidth;
}

//-----------------------------------------------------------------------------
// Return width of text in pixels
//-----------------------------------------------------------------------------
int CGameText::GetTextRenderHeight()
{
	return g_pGameUISystemSurface->GetFontTall( m_Font );
}


//-----------------------------------------------------------------------------
// Note corner colors will be stomped if the base graphic's color changes.
//-----------------------------------------------------------------------------
void CGameText::SetColor( color32 c )
{
	m_Geometry.m_TopColor = c;
	m_Geometry.m_BottomColor = c;
	// Remove first to force the new colors in.
	m_Geometry.m_VertexColors.RemoveAll();
	SetupVertexColors();
}


//-----------------------------------------------------------------------------
//	Determine if x,y is inside the graphic.
//-----------------------------------------------------------------------------
bool CGameText::HitTest( int x, int y )
{
	if ( !m_Geometry.m_bVisible ) 
		return false;

	// Just using extents for now, note extents don't take into account rotation.
	Vector2D point0( m_Geometry.m_Extents.m_TopLeft.x, m_Geometry.m_Extents.m_TopLeft.y );
	Vector2D point1( m_Geometry.m_Extents.m_BottomRight.x, m_Geometry.m_Extents.m_TopLeft.y );
	Vector2D point2( m_Geometry.m_Extents.m_BottomRight.x, m_Geometry.m_Extents.m_BottomRight.y );
	Vector2D point3( m_Geometry.m_Extents.m_TopLeft.x, m_Geometry.m_Extents.m_BottomRight.y );
	if ( PointTriangleHitTest( point0, point1, point2, Vector2D( x, y ) ) )
	{
		//Msg( "%d, %d hit\n", x, y );
		return true;
	}
	if ( PointTriangleHitTest( point0, point2, point3, Vector2D( x, y ) ) )
	{
		//Msg( "%d, %d hit\n", x, y );
		return true;
	}
	

	return false;
}




