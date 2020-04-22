//===== Copyright  Valve Corporation, All rights reserved. ======//
//
//  Radial, context-sensitive menu for co-op communication
//
//================================================================//

#include "cbase.h"
#include <string.h>
#include <stdio.h>
#include <vgui_controls/ImagePanel.h>
#include "voice_status.h"
#include "c_playerresource.h"
#include "cliententitylist.h"
#include "c_baseplayer.h"
#include "materialsystem/imesh.h"
#include "view.h"
#include "materialsystem/imaterial.h"
#include "tier0/dbg.h"
#include "cdll_int.h"
#include "menu.h" // for chudmenu defs
#include "keyvalues.h"
#include <filesystem.h>
#include "c_team.h"
#include "vgui/isurface.h"
#include "iclientmode.h"
#include "c_portal_player.h"
#include "hud_locator_target.h"
#include "c_user_message_register.h"
#include "portal_placement.h"
#include "glow_outline_effect.h"
#include "soundemittersystem/isoundemittersystembase.h"
#include "c_prop_portal.h"
#include "c_trigger_tractorbeam.h"
#include "c_projectedwallentity.h"
#include "portal_mp_gamerules.h"

#include "vgui/cursor.h"
#include "fmtstr.h"
#include "vgui_int.h"
#include "vgui/IVgui.h"
#include <game/client/iviewport.h>

#include "radialmenu.h"
#include "radialmenu_taunt.h"
#include "radialbutton.h"

#include "cegclientwrapper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern void AddLocator( C_BaseEntity *pTarget, const Vector &vPosition, const Vector &vNormal, int nPlayerIndex, const char *caption, float fDisplayTime );

ConVar cl_fastradial( "cl_fastradial", "1", FCVAR_DEVELOPMENTONLY, "If 1, releasing the button on a radial menu executes the highlighted button" );
ConVar RadialMenuDebug( "cl_rosette_debug", "0" );
ConVar cl_rosette_line_inner_radius( "cl_rosette_line_inner_radius", "25" );
ConVar cl_rosette_line_outer_radius( "cl_rosette_line_outer_radius", "45" );
ConVar cl_rosette_gamepad_lockin_time( "cl_rosette_gamepad_lockin_time", "0.2" );
ConVar cl_rosette_gamepad_expand_time( "cl_rosette_gamepad_expand_time", "0.5" );

#if defined( PORTAL2_PUZZLEMAKER )
extern ConVar sv_record_playtest;
#endif // PORTAL2_PUZZLEMAKER


#define PING_DELAY_BASE 0.05f
#define PING_DELAY_INCREMENT 0.05f

#define PING_SOUND_NAME "GameUI.UiCoopHudClick"
#define PING_SOUND_NAME_LOW "GameUI.UiCoopHudClickLow"
#define PING_SOUND_NAME_HIGH "GameUI.UiCoopHudClickHigh"

#define RADIAL_MENU_POINTER_TEXTURE "vgui/hud/icon_arrow_right"


void FlushClientMenus( void );
void CloseRadialMenuCommand( RadialMenuTypes_t menuType, bool bForceClose = false );


static ClientMenuManager TheClientMenuManager;
static ClientMenuManagerPlaytest TheClientMenuManagerPlaytest;


//--------------------------------------------------------------------------------------------------------
static char s_radialMenuName[ MAX_SPLITSCREEN_PLAYERS ][ 64 ];
static bool s_mouseMenuKeyHeld[ MAX_SPLITSCREEN_PLAYERS ];

// Precache our glow effects
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheRadialMenu )
	PRECACHE( MATERIAL, "dev/glow_blur_x" )
	PRECACHE( MATERIAL, "dev/glow_blur_y" )
	PRECACHE( MATERIAL, "dev/glow_color" )
	PRECACHE( MATERIAL, "dev/glow_downsample" )
	PRECACHE( MATERIAL, "dev/halo_add_to_screen" )
	PRECACHE( MATERIAL, RADIAL_MENU_POINTER_TEXTURE )
PRECACHE_REGISTER_END()

#define GLOW_OUTLINE_COLOR	Vector( 0.25f, 0.62f, 1.0f )
#define GLOW_OUTLINE_ALPHA	0.75f


struct SignifierInfo_t
{
	EHANDLE hTarget;
	Vector vPos;
	Vector vNormal;
	int nPlayerIndex;
	char szCaption[ 32 ];
	float fDisplayTime;
};

CUtlVector< SignifierInfo_t > g_SignifierQueue[ MAX_SPLITSCREEN_PLAYERS ];

CUtlVector< SignifierInfo_t >* GetSignifierQueue( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return &(g_SignifierQueue[ GET_ACTIVE_SPLITSCREEN_SLOT() ]);
}

void TeamPingColor( int nTeamNumber, Vector &vColor )
{
	Color color = Color( GLOW_OUTLINE_COLOR[0], GLOW_OUTLINE_COLOR[1], GLOW_OUTLINE_COLOR[2] );
	if ( nTeamNumber == TEAM_RED )
	{
		color = UTIL_Portal_Color( 2, 0 );  //orange
	}
	else
	{
		color = UTIL_Portal_Color( 1, 0 );  //blue
	}

	vColor.x = (float)color.r()/255.0f;
	vColor.y = (float)color.g()/255.0f;
	vColor.z = (float)color.b()/255.0f;
}

//--------------------------------------------------------------------------------------------------------
int AddGlowToObject( C_BaseEntity *pObject, int nTeamNumber )
{
	if ( pObject == NULL || pObject->GetRenderAlpha() <= 0 )
		return -1;

	// Determine if this entity uses the glow capability
	ISignifierTarget *pSignifier = dynamic_cast<ISignifierTarget *>(pObject);
	if ( pSignifier != NULL )
	{
		if ( pSignifier->UseSelectionGlow() == false )
			return -1;
	}

	Vector vColor;
	TeamPingColor( nTeamNumber, vColor );

	return g_GlowObjectManager.RegisterGlowObject( pObject, vColor, GLOW_OUTLINE_ALPHA, GET_ACTIVE_SPLITSCREEN_SLOT() );
}

void RadialMenuMouseCallback( uint8* pData, size_t iSize )
{
	// make sure we're getting as much data as we expect
	Assert( iSize == sizeof(int)*2 );
	// check for null pointer
	if ( pData == NULL )
		return;

	// capture the data
	int nCursorX = ((int*)pData)[0];
	int nCursorY = ((int*)pData)[1];
	// get the radial menu and give it the cursor position
	CRadialMenu *pRadialMenu = GET_HUDELEMENT( CRadialMenu );
	if ( pRadialMenu )
	{
		pRadialMenu->SetDemoCursorPos( nCursorX, nCursorY );
	}
}

void ClientMenuManager::AddMenuFile( const char *filename )
{
	if ( !m_menuKeys )
	{
		m_menuKeys = new KeyValues( "ClientMenu" );
	}

	KeyValues *data = new KeyValues( "ClientMenu" );
	if ( !data->LoadFromFile( filesystem, filename ) )
	{
		Warning( "Could not load client menu %s\n", filename );
		data->deleteThis();
		return;
	}

	KeyValues *menuKey = data->GetFirstTrueSubKey();
	while ( menuKey )
	{
		if ( m_menuKeys->FindKey( menuKey->GetName(), false ) == NULL )
		{
			data->RemoveSubKey( menuKey );
			m_menuKeys->AddSubKey( menuKey );
		}
		menuKey = data->GetFirstTrueSubKey();
	}
	data->deleteThis();
}

KeyValues *ClientMenuManager::FindMenu( const char *menuName )
{
	// do not show the menu if the player is dead or is an observer
	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( !player )
		return NULL;

	// Find our menu, honoring Alive/Dead/Team suffixes
	/*
	const char *teamSuffix = player->GetTeam()->Get_Name();
	const char *lifeSuffix = player->IsAlive() ? "Alive" : "Dead";

	CFmtStr str;
	const char *fullMenuName = str.sprintf( "%s,%s,%s", menuName, teamSuffix, lifeSuffix );
	*/

	CFmtStr str;
	const char *fullMenuName = str.sprintf( "%s,%s", "Command", menuName );
	KeyValues *menuKey = m_menuKeys->FindKey( fullMenuName, false );
	if ( !menuKey )
	{
		fullMenuName = menuName;
		menuKey = m_menuKeys->FindKey( fullMenuName, false );
	}

	return menuKey;
}

void ClientMenuManager::Flush( void )
{
	Reset();
	AddMenuFile( "scripts/RadialMenu.txt" );
}


//-------------------------------------------------------------------------------------------------------
KeyValues *ClientMenuManagerPlaytest::FindMenu( const char *menuName )
{
	return ClientMenuManager::FindMenu( menuName );
}


//-------------------------------------------------------------------------------------------------------
void ClientMenuManagerPlaytest::Flush( void )
{
	Reset();
	AddMenuFile( "scripts/RadialMenuPlaytest.txt" );
}


void ShowRadialMenuPanel( bool bShow )
{
	if ( IsPC() )
	{
		GetViewPortInterface()->ShowPanel( PANEL_RADIAL_MENU, bShow );
	}
}


CRadialMenuPanel::CRadialMenuPanel(IViewPort *pViewPort) : BaseClass(NULL, PANEL_RADIAL_MENU)
{
	m_pViewPort = pViewPort;

	// load the new scheme early!!
	SetScheme("ClientScheme");
	SetMoveable(false);
	SetSizeable(false);

	SetCursor( NULL );

	LoadControlSettings("Resource/UI/RadialMenuPanel.res");
	InvalidateLayout();

	SetPaintBackgroundEnabled( false );
}


void CRadialMenuPanel::ShowPanel( bool bShow )
{
	if ( BaseClass::IsVisible() == bShow )
		return;

	if ( bShow )
	{
		Activate();

		SetMouseInputEnabled( true );
	}
	else
	{
		SetVisible( false );
		SetMouseInputEnabled( false );
	}

	m_pViewPort->ShowBackGround( false );
}



float CRadialMenu::m_fLastPingTime[ MAX_SPLITSCREEN_PLAYERS ][ 2 ] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
int CRadialMenu::m_nNumPings[ MAX_SPLITSCREEN_PLAYERS ][ 2 ] = { { 0, 0 }, { 0, 0 } };

DECLARE_HUDELEMENT( CRadialMenu );

//--------------------------------------------------------------------------------------------------------------
CRadialMenu::CRadialMenu( const char *pElementName )
		: CHudElement( pElementName ), BaseClass( NULL, "RadialMenu" )
{
	SetParent( GetClientMode()->GetViewport() );
	SetHiddenBits( HIDEHUD_PLAYERDEAD );

	MEM_ALLOC_CREDIT();
	// initialize dialog

	m_resource = new KeyValues( "RadialMenu" );
	m_resource->LoadFromFile( filesystem, "resource/UI/RadialMenu.res" );
	m_menuData = NULL;
	FlushClientMenus();

	// load the new scheme early!!
	SetScheme("ClientScheme");
	SetProportional(true);

	HandleControlSettings();

	SetCursor( NULL );

	m_nArrowTexture = -1;

	m_minButtonX = 0;
	m_minButtonY = 0;
	m_maxButtonX = 0;
	m_maxButtonY = 0;

	m_demoCursorX = m_demoCursorY = 0;
	
	m_nEntityGlowIndex = -1;

	m_bEditing = false;
	m_bDragging = false;
	m_nDraggingTaunt = -1;

	m_fFadeInTime = 0.0f;
	m_fSelectionLockInTime = 0.0f;

	m_bEnabled = false;
	m_bQuickPingForceClose = false;

	vgui::ivgui()->AddTickSignal( GetVPanel() );
}


//--------------------------------------------------------------------------------------------------------
const char *CRadialMenu::ButtonNameFromDir( ButtonDir dir )
{
	switch( dir )
	{
	case CENTER:
		return "Center";
	case NORTH:
		return "North";
	case NORTH_EAST:
		return "NorthEast";
	case EAST:
		return "East";
	case SOUTH_EAST:
		return "SouthEast";
	case SOUTH:
		return "South";
	case SOUTH_WEST:
		return "SouthWest";
	case WEST:
		return "West";
	case NORTH_WEST:
		return "NorthWest";
	}

	return "None";
}


//--------------------------------------------------------------------------------------------------------
CRadialMenu::ButtonDir CRadialMenu::DirFromButtonName( const char * name )
{
	if ( FStrEq( name, "Center" ) )
		return CENTER;
	if ( FStrEq( name, "North" ) )
		return NORTH;
	if ( FStrEq( name, "NorthEast" ) )
		return NORTH_EAST;
	if ( FStrEq( name, "East" ) )
		return EAST;
	if ( FStrEq( name, "SouthEast" ) )
		return SOUTH_EAST;
	if ( FStrEq( name, "South" ) )
		return SOUTH;
	if ( FStrEq( name, "SouthWest" ) )
		return SOUTH_WEST;
	if ( FStrEq( name, "West" ) )
		return WEST;
	if ( FStrEq( name, "NorthWest" ) )
		return NORTH_WEST;

	return CENTER;
}


//--------------------------------------------------------------------------------------------------------
/**
 * Created controls from the resource file.  We know how to make a special PolygonButton :)
 */
vgui::Panel *CRadialMenu::CreateControlByName( const char *controlName )
{
	if( !Q_stricmp( "PolygonButton", controlName ) )
	{
		vgui::Button *newButton = new CRadialButton( this, NULL );
		return newButton;
	}
	else
	{
		return BaseClass::CreateControlByName( controlName );
	}
}


//--------------------------------------------------------------------------------------------------------
CRadialMenu::~CRadialMenu()
{
	m_resource->deleteThis();
	if ( m_menuData )
	{
		m_menuData->deleteThis();
	}

	if ( vgui::surface() && m_nArrowTexture != -1 )
	{
		vgui::surface()->DestroyTextureID( m_nArrowTexture );
		m_nArrowTexture = -1;
	}
}

void CRadialMenu::HandleControlSettings()
{
	LoadControlSettings("Resource/UI/RadialMenu.res");

	for ( int i=0; i<NUM_BUTTON_DIRS; ++i )
	{
		ButtonDir dir = (ButtonDir)i;
		const char *buttonName = ButtonNameFromDir( dir );
		m_buttons[i] = dynamic_cast<CRadialButton *>(FindChildByName( buttonName ));
		if ( m_buttons[i] )
		{
			m_buttons[i]->SetMouseInputEnabled( true );
		}
	}

	m_armedButtonDir = CENTER;
}

void CRadialMenu::StartDrag( void )
{
	if ( m_nDraggingTaunt == -1 )
	{
		return;
	}

	m_bDragging = true;

	for ( int i=0; i<NUM_BUTTON_DIRS; ++i )
	{
		ButtonDir dir = (ButtonDir)i;
		const char *buttonName = ButtonNameFromDir( dir );
		m_buttons[i] = dynamic_cast<CRadialButton *>(FindChildByName( buttonName ));
		if ( m_buttons[i] )
		{
			m_buttons[i]->SetMouseInputEnabled( false );
		}
	}

	SetArmedButtonDir( CENTER );
}

void CRadialMenu::EndDrag( void )
{
	if ( !m_bDragging || m_nDraggingTaunt == -1 )
	{
		return;
	}

	int nSwap = -1;

	for ( int i=0; i<NUM_BUTTON_DIRS; ++i )
	{
		ButtonDir dir = (ButtonDir)i;
		const char *buttonName = ButtonNameFromDir( dir );
		m_buttons[i] = dynamic_cast<CRadialButton *>(FindChildByName( buttonName ));
		if ( m_buttons[i] )
		{
			m_buttons[i]->SetMouseInputEnabled( true );

			int mouseX, mouseY;
			vgui::surface()->SurfaceGetCursorPos( mouseX, mouseY );
			if ( nSwap == -1 && m_buttons[i]->IsVisible() && m_buttons[i]->IsEnabled() && m_buttons[i]->IsWithinTraverse( mouseX, mouseY, false ) )
			{
				nSwap = i;
			}
		}
	}

	if ( nSwap != -1 && nSwap != CENTER )
	{
		CUtlVector< TauntStatusData > *pTauntData = GetClientMenuManagerTaunt().GetTauntData();
		TauntStatusData *pData = &((*pTauntData)[ m_nDraggingTaunt ]);

		const char *pDir = "empty";

		switch ( nSwap )
		{
		case NORTH:
			pDir = "North";
			break;

		case SOUTH:
			pDir = "South";
			break;

		case WEST:
			pDir = "West";
			break;

		case EAST:
			pDir = "East";
			break;

		case NORTH_WEST:
			pDir = "NorthWest";
			break;

		case NORTH_EAST:
			pDir = "NorthEast";
			break;

		case SOUTH_WEST:
			pDir = "SouthWest";
			break;

		case SOUTH_EAST:
			pDir = "SouthEast";
			break;
		}

		GetClientMenuManagerTaunt().SetTauntPosition( pData->szName, pDir );
		//GetClientMenuManagerTaunt().UpdateStorageChange( pData, GetClientMenuManagerTaunt().UPDATE_STORAGE_EQUIPSLOT );

		KeyValues *menuKey = GetClientMenuManagerTaunt().FindMenu( "Default" );
		if ( menuKey )
		{
			SetData( menuKey );
		}
	}

	m_bDragging = false;
	m_nDraggingTaunt = -1;
}


//--------------------------------------------------------------------------------------------------------
/**
 * The radial menu should cover the entire screen to capture mouse input, so we should have a blank
 * background.
 */
void CRadialMenu::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	HandleControlSettings();

	BaseClass::ApplySchemeSettings( pScheme );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	m_lineColor = pScheme->GetColor( "rosette.LineColor", Color( 192, 192, 192, 128 ) );

	if ( RadialMenuDebug.GetBool() )
	{
		SetCursor( vgui::dc_crosshair );
	}
	else
	{
		SetCursor( NULL );
	}

	// Restore button names/commands
	if ( m_menuData )
	{
		SetData( m_menuData );
	}

	SetAlpha( 0 );
}


void CRadialMenu::PerformLayout( void )
{
	SetRadialMenuEnabled( false );

	BaseClass::PerformLayout();
}


//--------------------------------------------------------------------------------------------------------
void CRadialMenu::ShowPanel( bool show )
{
	if ( IsVisible() == show )
		return;

	if ( show )
	{
		for ( int i=0; i<NUM_BUTTON_DIRS; ++i )
		{
			if ( !m_buttons[i] )
				continue;

			m_buttons[i]->SetArmed( false );
			m_buttons[i]->SetFakeArmed( false );
			m_buttons[i]->SetChosen( false );
		}

		SetMouseInputEnabled( true );
		m_cursorX = m_cursorY = 0;
		m_demoCursorX = m_demoCursorY = 0;
		m_bFirstCentering = true;
	}
	else
	{
		SetRadialMenuEnabled( false );
		SetMouseInputEnabled( false );
	}
}

void CRadialMenu::PaintBackground( void )
{
	int nSlot = vgui::ipanel()->GetMessageContextId( GetVPanel() );
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSlot );

	if ( m_armedButtonDir != CENTER || !m_buttons[ CENTER ]->IsVisible() )
	{
		int x, y, wide, tall;
		GetBounds( x, y, wide, tall );

		float fCenterX = x + wide/2;
		float fCenterY = y + tall/2;

		int nCursorX, nCursorY;
		if ( !input->ControllerModeActive() )
		{
			nCursorX = m_cursorX;
			nCursorY = m_cursorY;
		}
		else
		{
			float fJoyForward, fJoySide, fJoyPitch, fJoyYaw = 0.0f;
			input->Joystick_Querry( fJoyForward, fJoySide, fJoyPitch, fJoyYaw );

			// Replace if the other stick was pushed further
			// We need to use both sticks because they might have southpaw or legacy set
			if ( fabsf( fJoyForward ) > fabsf( fJoyPitch ) )
			{
				fJoyPitch = fJoyForward;
			}
			else
			{
				// Reflip it if the y was inverted because this shouldn't be inverted
				static SplitScreenConVarRef s_joy_inverty( "joy_inverty" );
				if ( s_joy_inverty.IsValid() && s_joy_inverty.GetBool( nSlot ) )
				{
					fJoyPitch *= -1.0f;
				}
			}

			// Replace if the other stick was pushed further
			// We need to use both sticks because they might have southpaw or legacy set
			if ( fabsf( fJoySide ) > fabsf( fJoyYaw ) )
			{
				fJoyYaw = fJoySide;
			}

			if ( fJoyPitch > 0.1f || fJoyYaw > 0.1f )
			{
				nCursorX = fCenterX + fJoyYaw * 100.0f;
				nCursorY = fCenterY + fJoyPitch * 100.0f;
			}
			else
			{
				int buttonX, buttonY;
				m_buttons[ m_armedButtonDir ]->GetIcon()->GetPos( nCursorX, nCursorY );
				m_buttons[ m_armedButtonDir ]->GetPos( buttonX, buttonY );
				nCursorX += buttonX + m_buttons[ m_armedButtonDir ]->GetIcon()->GetWide() / 2;
				nCursorY += buttonY + m_buttons[ m_armedButtonDir ]->GetIcon()->GetTall() / 2;
			}
		}

		Vector2D vNormal( nCursorX - fCenterX, nCursorY - fCenterY );
		if ( vNormal.IsZero() )
		{
			vNormal = Vector2D( 0.0f, -1.0f );
		}
		Vector2DNormalize( vNormal );
		Vector2D vDown( vNormal.y, -vNormal.x );

		float innerRadius = cl_rosette_line_inner_radius.GetFloat();
		float outerRadius = cl_rosette_line_outer_radius.GetFloat();
		innerRadius = YRES( innerRadius ) * 0.75f;
		outerRadius = YRES( outerRadius );

		float fSize = ( outerRadius - innerRadius ) * 0.5f;
		float fOffset = innerRadius + fSize;

		fCenterX += vNormal.x * fOffset;
		fCenterY += vNormal.y * fOffset;

		vgui::Vertex_t points[4] =
		{
			vgui::Vertex_t( Vector2D( fCenterX, fCenterY ) - vDown * fSize - vNormal * fSize,	Vector2D(0,0) ),	// Top Left
			vgui::Vertex_t( Vector2D( fCenterX, fCenterY ) + vDown * fSize - vNormal * fSize,	Vector2D(0,1) ),	// Bottom Left
			vgui::Vertex_t( Vector2D( fCenterX, fCenterY ) + vDown * fSize + vNormal * fSize,	Vector2D(1,1) ),	// Bottom Right
			vgui::Vertex_t( Vector2D( fCenterX, fCenterY ) - vDown * fSize + vNormal * fSize,	Vector2D(1,0) )		// Top Right
		};

		vgui::surface()->DrawSetColor( 255, 255, 255, 255 );
		vgui::surface()->DrawSetTexture( m_nArrowTexture );
		vgui::surface()->DrawTexturedPolygon( 4, points );
	}

	BaseClass::PaintBackground();
}

//--------------------------------------------------------------------------------------------------------
void CRadialMenu::Paint( void )
{
	int nBaseAlpha = 0;
	float fFade = gpGlobals->curtime - m_fFadeInTime;
	if ( fFade > 0.0f )
	{
		fFade = MIN( 1.0f, fFade * 10.0f );
		nBaseAlpha = fFade * 255.0f;
	}

	const float fadeDuration = 0.1f;
	if ( m_fading )
	{
		float fadeTimeRemaining = m_fadeStart + fadeDuration - gpGlobals->curtime;
		int alpha = nBaseAlpha * fadeTimeRemaining / fadeDuration;
		SetAlpha( alpha );
		if ( alpha <= 0 )
		{
			ShowRadialMenuPanel( false );
			SetRadialMenuEnabled( false );
			return;
		}
	}
	else
	{
		SetAlpha( nBaseAlpha );
	}

	int x, y, wide, tall;
	GetBounds( x, y, wide, tall );
	vgui::surface()->DrawSetColor( m_lineColor );

	int centerX = x + wide/2;
	int centerY = y + tall/2;
	float innerRadius = cl_rosette_line_inner_radius.GetFloat();
	float outerRadius = cl_rosette_line_outer_radius.GetFloat();
	innerRadius = YRES( innerRadius );
	outerRadius = YRES( outerRadius );

	// Draw horizontal and vertical lines
	if ( m_armedButtonDir != EAST && m_buttons[EAST]->IsVisible() && m_buttons[EAST]->IsEnabled() )
	{
		vgui::surface()->DrawLine( centerX + innerRadius, centerY, centerX + outerRadius, centerY );
	}
	
	if ( m_armedButtonDir != WEST && m_buttons[WEST]->IsVisible() && m_buttons[WEST]->IsEnabled() )
	{
		vgui::surface()->DrawLine( centerX - innerRadius, centerY, centerX - outerRadius, centerY );
	}
	
	if ( m_armedButtonDir != SOUTH && m_buttons[SOUTH]->IsVisible() && m_buttons[SOUTH]->IsEnabled() )
	{
		vgui::surface()->DrawLine( centerX, centerY + innerRadius, centerX, centerY + outerRadius );
	}
	
	if ( m_armedButtonDir != NORTH && m_buttons[NORTH]->IsVisible() && m_buttons[NORTH]->IsEnabled() )
	{
		vgui::surface()->DrawLine( centerX, centerY - innerRadius, centerX, centerY - outerRadius );
	}

	// Draw diagonal lines
	const float scale = 0.707f; // sqrt(2) / 2

	if ( m_armedButtonDir != SOUTH_EAST && m_buttons[SOUTH_EAST]->IsVisible() && m_buttons[SOUTH_EAST]->IsEnabled() )
	{
		vgui::surface()->DrawLine( centerX + innerRadius * scale, centerY + innerRadius * scale, centerX + outerRadius * scale, centerY + outerRadius * scale );
	}

	if ( m_armedButtonDir != NORTH_WEST && m_buttons[NORTH_WEST]->IsVisible() && m_buttons[NORTH_WEST]->IsEnabled() )
	{
		vgui::surface()->DrawLine( centerX - innerRadius * scale, centerY - innerRadius * scale, centerX - outerRadius * scale, centerY - outerRadius * scale );
	}

	if ( m_armedButtonDir != NORTH_EAST && m_buttons[NORTH_EAST]->IsVisible() && m_buttons[NORTH_EAST]->IsEnabled() )
	{
		vgui::surface()->DrawLine( centerX + innerRadius * scale, centerY - innerRadius * scale, centerX + outerRadius * scale, centerY - outerRadius * scale );
	}

	if ( m_armedButtonDir != SOUTH_WEST && m_buttons[SOUTH_WEST]->IsVisible() && m_buttons[SOUTH_WEST]->IsEnabled() )
	{
		vgui::surface()->DrawLine( centerX - innerRadius * scale, centerY + innerRadius * scale, centerX - outerRadius * scale, centerY + outerRadius * scale );
	}

	if ( RadialMenuDebug.GetBool() )
	{
		vgui::surface()->DrawSetColor( m_lineColor );
		vgui::surface()->DrawOutlinedRect( x + m_minButtonX, y + m_minButtonY, x + m_maxButtonX, y + m_maxButtonY );

		// draw the cursor position
		vgui::surface()->DrawLine( m_cursorX -10, m_cursorY, m_cursorX + 10, m_cursorY );
		vgui::surface()->DrawLine( m_cursorX , m_cursorY - 10 , m_cursorX, m_cursorY + 10 );
	}

	BaseClass::Paint();
}


void PlayDeactivateSound()
{
	C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
	// If we've got the player and they selected a different button
	if ( localPlayer )
	{
		localPlayer->EmitSound( "GameUI.UiCoopHudDeactivate" );
	}
}

//--------------------------------------------------------------------------------------------------------
void CRadialMenu::OnMousePressed( vgui::MouseCode code )
{
	if ( m_bEditing )
	{
		if ( code == MOUSE_LEFT )
		{
			StartDrag();
		}
	}

	if ( code == MOUSE_RIGHT )
	{
		StartFade();
		PlayDeactivateSound();
	}
	
	BaseClass::OnMousePressed( code );
}

void CRadialMenu::OnMouseReleased( vgui::MouseCode code )
{
	if ( m_bEditing )
	{
		if ( code == MOUSE_LEFT )
		{
			EndDrag();
		}
	}

	BaseClass::OnMouseReleased( code );
}

//--------------------------------------------------------------------------------------------------------
void CRadialMenu::OnCommand( const char *command )
{
	if ( RadialMenuDebug.GetBool() )
	{
		Msg( "%f: Clicked on button with command '%s'\n", gpGlobals->curtime, command );
	}

	if ( !Q_strcmp(command, "done") )
	{
		C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
		if ( localPlayer )
		{
			localPlayer->EmitSound("MouseMenu.abort");
		}
		StartFade();
	}
	else
	{
		// Undone... people hate clicking to select, so don't let them train themselves to do it
		//StartFade();
		//SendCommand( command );
	}

	BaseClass::OnCommand( command );
}


//--------------------------------------------------------------------------------------------------------
void CRadialMenu::SetArmedButtonDir( ButtonDir dir )
{
	if ( dir != NUM_BUTTON_DIRS )
	{
		CRadialButton *armedButton = m_buttons[dir];
		if ( m_buttons[dir]->GetPassthru() )
		{
			armedButton = m_buttons[dir]->GetPassthru();
			for ( int i=0; i<NUM_BUTTON_DIRS; ++i )
			{
				if ( m_buttons[i] == armedButton )
				{
					dir = (ButtonDir)i;
					break;
				}
			}
		}
	}

	if ( m_buttons[ dir ]->IsEnabled() )
	{
		C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
		// If we've got the player and they selected a different button
		if ( localPlayer && dir != m_armedButtonDir )
		{
			if( dir != CENTER )
			{
				localPlayer->EmitSound("GameUI.UiCoopHudFocus");
			}
			else
			{
				localPlayer->EmitSound("GameUI.UiCoopHudUnfocus");
			}
		}
	}

	m_armedButtonDir = dir;

	for ( int i=0; i<NUM_BUTTON_DIRS; ++i )
	{
		if ( !m_buttons[i] )
			continue;

		m_buttons[i]->SetFakeArmed( false );
		m_buttons[i]->SetArmed( false );
		if ( i != m_armedButtonDir )
		{
			m_buttons[i]->SetChosen( false );
		}
	}

	if ( m_armedButtonDir != NUM_BUTTON_DIRS )
	{
		if ( m_buttons[m_armedButtonDir] )
		{
#ifdef OSX
			if ( !m_buttons[m_armedButtonDir]->IsFakeArmed() )
			{
				m_buttons[m_armedButtonDir]->SetArmed( true );	
				OnCursorEnteredButton( m_cursorX, m_cursorY, m_buttons[m_armedButtonDir] );
			}
#endif
			m_buttons[m_armedButtonDir]->SetFakeArmed( true );
		}
	}

	if ( m_armedButtonDir != CENTER )
	{
		SetFadeInTime( gpGlobals->curtime - 1.0f );
	}
}


//--------------------------------------------------------------------------------------------------------
static const char *ButtonDirString( CRadialMenu::ButtonDir dir )
{
	switch ( dir )
	{
	case CRadialMenu::CENTER:
		return "CENTER";
	case CRadialMenu::NORTH:
		return "NORTH";
	case CRadialMenu::NORTH_EAST:
		return "NORTH_EAST";
	case CRadialMenu::EAST:
		return "EAST";
	case CRadialMenu::SOUTH_EAST:
		return "SOUTH_EAST";
	case CRadialMenu::SOUTH:
		return "SOUTH";
	case CRadialMenu::SOUTH_WEST:
		return "SOUTH_WEST";
	case CRadialMenu::WEST:
		return "WEST";
	case CRadialMenu::NORTH_WEST:
		return "NORTH_WEST";
	default:
		return "UNKNOWN";
	}
}


//--------------------------------------------------------------------------------------------------------
bool IsCurrentMenuTypeDisabled( C_Portal_Player *pPlayer, RadialMenuTypes_t menuType )
{
	// if the player isn't valid or is in the middle of taunting
	if ( !pPlayer || pPlayer->IsTaunting() )
	{
		return true;
	}

	// check the specific type of radial menu
	switch ( menuType )
	{
	case MENU_PING:
		if ( pPlayer->IsPingDisabled() )
		{
			return true;
		}
		break;
	case MENU_TAUNT:
		if ( pPlayer->IsTauntDisabled() )
		{
			return true;
		}
		break;
	case MENU_PLAYTEST:
		// don't allow playtest pings where normal pings aren't allowed
		if ( pPlayer->IsPingDisabled() 
#if defined( PORTAL2_PUZZLEMAKER )
			|| ( !sv_record_playtest.GetBool() && !engine->IsPlayingDemo() )
#endif // PORTAL2_PUZZLEMAKER
			)
		{
			return true;
		}
		break;
	}

	return false;
}


//--------------------------------------------------------------------------------------------------------
void CRadialMenu::OnCursorEnteredButton( int x, int y, CRadialButton *button )
{
	ButtonDir nNewDir = NUM_BUTTON_DIRS;

	for ( int i=0; i<NUM_BUTTON_DIRS; ++i )
	{
		if ( m_buttons[i] == button )
		{
			m_cursorX = x;
			m_cursorY = y;
			// If we've got the player and they selected a different button
			if ( (ButtonDir)i != m_armedButtonDir )
			{
				nNewDir = (ButtonDir)i;
				break;
			}

			if ( RadialMenuDebug.GetBool() )
			{
				Msg( "%f: rosette cursor entered %s at %d,%d\n", gpGlobals->curtime, ButtonDirString( m_armedButtonDir ), x, y );
				engine->Con_NPrintf( 20, "%d %d %s", x, y, ButtonDirString( m_armedButtonDir ) );
			}
		}
	}

	if ( nNewDir != NUM_BUTTON_DIRS && !input->ControllerModeActive() )
	{
		SetArmedButtonDir( nNewDir );
	}
}


//--------------------------------------------------------------------------------------------------------
void CRadialMenu::UpdateButtonBounds( void )
{
	// Save off the extents of our child buttons so we can clip the cursor to that later

	int wide, tall;
	GetSize( wide, tall );
	m_minButtonX = wide;
	m_minButtonY = tall;
	m_maxButtonX = 0;
	m_maxButtonY = 0;

	for ( int i=0; i<NUM_BUTTON_DIRS; ++i )
	{
		if ( !m_buttons[i] )
			continue;

		int hotspotMinX = 0;
		int hotspotMinY = 0;
		int hotspotMaxX = 0;
		int hotspotMaxY = 0;
		m_buttons[i]->GetHotspotBounds( &hotspotMinX, &hotspotMinY, &hotspotMaxX, &hotspotMaxY );

		int buttonX, buttonY;
		m_buttons[i]->GetPos( buttonX, buttonY );

		m_minButtonX = MIN( m_minButtonX, hotspotMinX + buttonX );
		m_minButtonY = MIN( m_minButtonY, hotspotMinY + buttonY );
		m_maxButtonX = MAX( m_maxButtonX, hotspotMaxX + buttonX );
		m_maxButtonY = MAX( m_maxButtonY, hotspotMaxY + buttonY );
	}

	// First frame, we won't have any hotspots set up, so we get inverted bounds from our initial setup.
	// Reverse these, so our button bounds covers our bounds.
	if ( m_minButtonX > m_maxButtonX )
	{
		V_swap( m_minButtonX, m_maxButtonX );
	}

	if ( m_minButtonY > m_maxButtonY )
	{
		V_swap( m_minButtonY, m_maxButtonY );
	}
}


//--------------------------------------------------------------------------------------------------------
void CRadialMenu::OnThink( void )
{
	int nSlot = vgui::ipanel()->GetMessageContextId( GetVPanel() );
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSlot );

	if ( m_bQuickPingForceClose )
	{
		SetArmedButtonDir( CENTER );
	}

	C_Portal_Player *pPlayer = ToPortalPlayer( C_BasePlayer::GetLocalPlayer( nSlot ) );
	if ( IsCurrentMenuTypeDisabled( pPlayer, m_menuType ) )
	{
		CloseRadialMenuCommand( m_menuType );
	}

	if ( !IsMouseInputEnabled() )
		return;

	// See if our target entity has vanished
	if ( (m_menuType != MENU_TAUNT) && m_hTargetEntity == NULL && m_vPosition == vec3_invalid )
	{
		StartFade();
		return;
	}

	// See if we need to create our glow index
	if ( m_nEntityGlowIndex == -1 )
	{
		// dont glow players
		C_Portal_Player *pPlayer = dynamic_cast<C_Portal_Player *>(m_hTargetEntity.Get());
		if ( !pPlayer && m_menuType != MENU_TAUNT && ( m_hTargetEntity.Get() && m_hTargetEntity.Get()->GetRenderAlpha() > 0) )
		{
			C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
			if ( !localPlayer )
				return;

			m_nEntityGlowIndex = AddGlowToObject( m_hTargetEntity, localPlayer->GetTeamNumber() );
		}
	}
	else if ( m_menuType == MENU_TAUNT )
	{
		ClearGlowEntity();
	}

#if defined(OSX)
	int dx, dy;
	engine->GetMouseDelta( dx, dy );
	m_cursorX += dx;
	m_cursorY += dy;
#else
	vgui::surface()->SurfaceGetCursorPos( m_cursorX, m_cursorY );
	ScreenToLocal( m_cursorX, m_cursorY );
#endif

	if ( engine->IsRecordingDemo() )
	{
		int nMousePos[2] = { m_cursorX, m_cursorY };
		engine->RecordDemoCustomData( RadialMenuMouseCallback, &nMousePos, sizeof(int) * 2 );
	}
	else if ( engine->IsPlayingDemo() )
	{
		// use saved coords from the callback
		m_cursorX = m_demoCursorX;
		m_cursorY = m_demoCursorY;
	}
	
	int wide, tall;
	GetSize( wide, tall );

	int centerx = wide / 2;
	int centery = tall / 2;

	UpdateButtonBounds();

	if ( m_bFirstCentering )
	{
#ifndef OSX
		LocalToScreen( centerx, centery );
		vgui::surface()->SurfaceSetCursorPos( centerx, centery );
#endif
		m_cursorX = centerx;
		m_cursorY = centery;
		SetArmedButtonDir( CENTER );

		m_bFirstCentering = false;
	}
	else
	{

		float cursorDistX = ( m_cursorX - centerx );
		float cursorDistY = ( m_cursorY - centery );
		float buttonRadius = MAX( m_maxButtonX - centerx, m_maxButtonY - centery ) * 0.3f;

		if ( cursorDistX != 0.0f && cursorDistY != 0.0f )
		{
			float cursorDist = sqrt( Sqr(cursorDistX) + Sqr(cursorDistY) );

			// keeping mouse cursor within button radius
			if ( cursorDist > buttonRadius )
			{
				cursorDistX *= ( buttonRadius / cursorDist );
				cursorDistY *= ( buttonRadius / cursorDist );

				m_cursorX = centerx + cursorDistX;
				m_cursorY = centery + cursorDistY;
			}

#ifndef OSX
			LocalToScreen( m_cursorX, m_cursorY );
			if ( RadialMenuDebug.GetBool() )
			{
				Msg( "%f: rosette warping cursor to %d %d\n", gpGlobals->curtime, m_cursorX, m_cursorY );
			}
			
			vgui::surface()->SurfaceSetCursorPos( m_cursorX, m_cursorY );
#endif
		}

		float fJoyForward, fJoySide, fJoyPitch, fJoyYaw = 0.0f;

		if ( input->ControllerModeActive() )
		{
			input->Joystick_Querry( fJoyForward, fJoySide, fJoyPitch, fJoyYaw );

			// Replace if the other stick was pushed further
			// We need to use both sticks because they might have southpaw or legacy set
			if ( fabsf( fJoyForward ) > fabsf( fJoyPitch ) )
			{
				fJoyPitch = fJoyForward;
			}
			else
			{
				// Reflip it if the y was inverted because this shouldn't be inverted
				static SplitScreenConVarRef s_joy_inverty( "joy_inverty" );
				if ( s_joy_inverty.IsValid() && s_joy_inverty.GetBool( nSlot ) )
				{
					fJoyPitch *= -1.0f;
				}
			}

			// Replace if the other stick was pushed further
			// We need to use both sticks because they might have southpaw or legacy set
			if ( fabsf( fJoySide ) > fabsf( fJoyYaw ) )
			{
				fJoyYaw = fJoySide;
			}

		}
		else
		{
			fJoyPitch = cursorDistY / buttonRadius;
			fJoyYaw = cursorDistX / buttonRadius;
		}

		ButtonDir dir = CENTER;
		// Right stick can select
		if ( fabsf( fJoyPitch ) > 0.3f || fabsf( fJoyYaw ) > 0.3f )
		{
			// Right stick is past the dead zone
			float fStickAngle = RAD2DEG( atan2( fJoyYaw, fJoyPitch ) );

			if ( fStickAngle > 157.5f )
			{
				dir = NORTH;
			}
			else if ( fStickAngle > 112.5f )
			{
				dir = NORTH_EAST;
			}
			else if ( fStickAngle > 67.5f )
			{
				dir = EAST;
			}
			else if ( fStickAngle > 22.5f )
			{
				dir = SOUTH_EAST;
			}
			else if ( fStickAngle > -22.5f )
			{
				dir = SOUTH;
			}
			else if ( fStickAngle > -67.5f )
			{
				dir = SOUTH_WEST;
			}
			else if ( fStickAngle > -112.5f )
			{
				dir = WEST;
			}
			else if ( fStickAngle > -157.5f )
			{
				dir = NORTH_WEST;
			}
			else
			{
				dir = NORTH;
			}
		}

		CRadialButton *pButton = m_buttons[dir];
		if ( pButton && pButton->IsVisible() && pButton->IsEnabled() && !pButton->IsArmed() && m_armedButtonDir != dir )
		{
			// Only allow going back to center if a tiny amount of time has passed
			if ( !input->ControllerModeActive() || m_fSelectionLockInTime == 0.0f || gpGlobals->curtime < m_fSelectionLockInTime + cl_rosette_gamepad_lockin_time.GetFloat() || dir != CENTER )
			{
				m_fSelectionLockInTime = gpGlobals->curtime;
				pButton->SetMaxScale( dir == CENTER ? 1.0f : 0.75f );
				SetArmedButtonDir( dir );
			}
		}

		if ( m_armedButtonDir != CENTER && m_armedButtonDir != NUM_BUTTON_DIRS )
		{
			float fInterp = RemapValClamped( ( gpGlobals->curtime - m_fSelectionLockInTime ), 0.0f, cl_rosette_gamepad_expand_time.GetFloat(), 0.0f, 1.0f );

			CRadialButton *pSelectedButton = m_buttons[ m_armedButtonDir ];
			pSelectedButton->SetMaxScale( 0.75f + 0.25f * fInterp );

			if ( input->ControllerModeActive() && fInterp >= 1.0f )
			{
				// take action as soon as the player select something on the menu
				CloseRadialMenuCommand( m_menuType );
			}
		}
	}
}


void CRadialMenu::OnTick( void )
{
	BaseClass::OnTick();

	for ( int nPlayer = 0; nPlayer < MAX_SPLITSCREEN_PLAYERS; ++nPlayer )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( nPlayer );
		CUtlVector< SignifierInfo_t > *pSignifierQueue = GetSignifierQueue();
		for ( int i = pSignifierQueue->Count() - 1; i >= 0; --i )
		{
			if ( (*pSignifierQueue)[ i ].fDisplayTime <= gpGlobals->curtime )
			{
				AddLocator( (*pSignifierQueue)[ i ].hTarget, (*pSignifierQueue)[ i ].vPos, (*pSignifierQueue)[ i ].vNormal, (*pSignifierQueue)[ i ].nPlayerIndex, (*pSignifierQueue)[ i ].szCaption, 0.0f );
				pSignifierQueue->Remove( i );
			}
		}
	}
}


static const char *s_pszRadialMenuIgnoreActions[] =
{
	"forward",
	"back",
	"moveleft",
	"moveright",
	"jump",
	"duck",
	"attack",
	"attack2"
};

int	CRadialMenu::KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	static bool bViewLocked = false;
	if ( !IsVisible() )
		return 1;

	if ( !down )
		return 1;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	int numIgnore = ARRAYSIZE( s_pszRadialMenuIgnoreActions );
	for ( int i=0; i<numIgnore; ++i )
	{
		int count = 0;
		ButtonCode_t key;
		do 
		{
			key = (ButtonCode_t)engine->Key_CodeForBinding( s_pszRadialMenuIgnoreActions[i], nSlot, count, -1 );
			if ( IsJoystickCode( key ) )
			{
				key = GetBaseButtonCode( key );
			}

			if ( keynum == key )
			{
				return 0;
			}
			++count;
		} while ( key != BUTTON_CODE_INVALID );
	}

	return 1;
}


//--------------------------------------------------------------------------------------------------------
void CRadialMenu::SendCommand( const char *commandStr )
{
	if ( V_strcmp( commandStr, "done" ) == 0 )
	{
		return;
	}

	// Setup the basic command
	char szClientCmd[512];

	// Tack on our target entity index
	int nEntityIndex = ( m_hTargetEntity ) ? m_hTargetEntity->entindex() : -1;

	bool bDelayed = false;
	bool bIsTaunt = false;

	if ( StringHasPrefix( commandStr, "taunt" ) )
	{
		bIsTaunt = true;

		V_strcpy( szClientCmd, commandStr );

		const char *pchTaunt = commandStr + V_strlen( "taunt" );
		if ( pchTaunt[ 0 ] == ' ' && pchTaunt[ 1 ] != '\0' )
		{
			pchTaunt++;

			GetClientMenuManagerTaunt().IsTauntTeam( pchTaunt );
			GetClientMenuManagerTaunt().SetTauntUsed( pchTaunt );
		}
	}
	else
	{
		if ( V_strcmp( commandStr, "countdown" ) == 0 )
		{
			bDelayed = true;

			if ( GetSignifierQueue()->Count() )
			{
				// Don't start a new count till the old one finished!
				return;
			}
		}

		// Build a super string (ick)
		V_snprintf( szClientCmd, sizeof( szClientCmd ), "signify %s %d %.2f %.2f %.2f %.2f %.2f %.2f %.2f", 
			commandStr,
			nEntityIndex,
			( bDelayed ? 0.3f : 0.0f ),
			m_vPosition.x, m_vPosition.y, m_vPosition.z,
			m_vNormal.x, m_vNormal.y, m_vNormal.z );
	}
	
	// Send it on its way
	engine->ClientCmd( szClientCmd );

	if ( !bDelayed && !bIsTaunt )
	{
		// Now send to ourself first
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pPlayer )
		{
			AddLocator( m_hTargetEntity, m_vPosition, m_vNormal, pPlayer->entindex(), commandStr, 0.0f );
		}
	}
}

//--------------------------------------------------------------------------------------------------------
void CRadialMenu::ChooseArmedButton()
{
	StartFade();

	CRadialButton *button = NULL;
	for ( int i=NUM_BUTTON_DIRS-1; i>=0; --i )
	{
		if ( !m_buttons[i] )
			continue;

		if ( m_buttons[i]->IsVisible() && m_buttons[i]->IsEnabled() && m_buttons[i]->IsArmed() && !m_buttons[i]->GetPassthru() )
		{
			if ( RadialMenuDebug.GetBool() )
			{
				Msg( "%f: Choosing armed button %s\n", gpGlobals->curtime, ButtonDirString( (ButtonDir)i ) );
			}
			button = m_buttons[i];
			break;
		}
	}

	if ( !button && m_armedButtonDir != NUM_BUTTON_DIRS )
	{
		if ( m_buttons[m_armedButtonDir] && m_buttons[m_armedButtonDir]->IsVisible() && m_buttons[m_armedButtonDir]->IsEnabled() )
		{
			if ( RadialMenuDebug.GetBool() )
			{
				Msg( "%f: Choosing saved armed button %s\n", gpGlobals->curtime, ButtonDirString( m_armedButtonDir ) );
			}
			button = m_buttons[m_armedButtonDir];
		}
	}

	if ( button )
	{
		KeyValues *command = button->GetCommand();
		if ( command )
		{
			const char *commandStr = command->GetString( "command", NULL );
			if ( commandStr )
			{
				button->SetChosen( true );
				SendCommand( commandStr );

				if ( button->GetGLaDOSResponse() > 0 )
				{
					C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
					if ( localPlayer )
					{
						char szCmd[ 32 ];
						V_snprintf( szCmd, sizeof(szCmd), "CoopPingTool(%i,%i)", ( localPlayer->GetTeamNumber() == TEAM_RED ? 1 : 2 ), button->GetGLaDOSResponse() );
						engine->ClientCmd( szCmd );
					}
				}
			}
		}

		for ( int i=0; i<NUM_BUTTON_DIRS; ++i )
		{
			if ( !m_buttons[i] )
				continue;

			if ( m_buttons[i] == button )
			{
				if( m_menuType == MENU_TAUNT && i == CENTER )
				{
					PlayDeactivateSound();
				}
				continue;
			}

			m_buttons[i]->SetFakeArmed( false );
			m_buttons[i]->SetChosen( false );
		}
	}
}


//--------------------------------------------------------------------------------------------------------
void CRadialMenu::StartFade( void )
{
	m_fading = true;
	m_fadeStart = gpGlobals->curtime;
	SetMouseInputEnabled( false );

	ClearGlowEntity();	
}


//--------------------------------------------------------------------------------------------------------
void CRadialMenu::SetData( KeyValues *data )
{
	if ( !data )
		return;

	if ( RadialMenuDebug.GetBool() )
	{
		m_resource->deleteThis();
		m_resource = new KeyValues( "RadialMenu" );
		m_resource->LoadFromFile( filesystem, "resource/UI/RadialMenu.res" );
	}

	if ( m_menuData != data )
	{
		if ( m_menuData )
		{
			m_menuData->deleteThis();
		}

		m_menuData = data->MakeCopy();
	}

	bool bDisabledButtons[ NUM_BUTTON_DIRS ];
	memset( bDisabledButtons, 0, sizeof( bDisabledButtons ) );

	for ( int i=0; i<NUM_BUTTON_DIRS; ++i )
	{
		if ( !m_buttons[i] )
			continue;

		ButtonDir dir = (ButtonDir)i;
		const char *buttonName = ButtonNameFromDir( dir );
		KeyValues *buttonData = data->FindKey( buttonName, false );
		if ( !buttonData )
		{
			m_buttons[i]->SetVisible( false );
			continue;
		}

		KeyValues *resourceControl = m_resource->FindKey( buttonName, false );
		if ( resourceControl )
		{
			m_buttons[i]->UpdateHotspots( resourceControl );
			m_buttons[i]->InvalidateLayout();
		}

		m_buttons[i]->SetVisible( true );
		m_buttons[i]->SetChosen( false );

		const char *text = buttonData->GetString( "text" );
		m_buttons[i]->SetText( text );

		const char *command = buttonData->GetString( "command" );
		m_buttons[i]->SetCommand( command );

		const char *image = buttonData->GetString( "icon" );
		if ( image && image[0] != '\0' )
		{
			const char *image2 = buttonData->GetString( "icon2" );
			if ( image2 && image2[0] != '\0' )
			{
				// Lets decide which image to use based on team
				C_BasePlayer *localPlayer = C_BasePlayer::GetLocalPlayer();
				if ( localPlayer && localPlayer->GetTeamNumber() == TEAM_RED )
				{
					m_buttons[i]->SetImage( image2 );
				}
				else
				{
					m_buttons[i]->SetImage( image );
				}
			}
			else
			{
				// Only one image possible
				m_buttons[i]->SetImage( image );
			}
		}

		if ( !command || !*command )
		{
			bDisabledButtons[ i ] = true;
			m_buttons[i]->SetEnabled( false );
			m_buttons[i]->SetPulse( false );
		}
		else
		{
			m_buttons[i]->SetEnabled( true );
			m_buttons[i]->SetPulse( false );
			//m_buttons[i]->SetPulse( buttonData->GetBool( "new" ) );
		}

		m_buttons[i]->SetGLaDOSResponse( buttonData->GetInt( "glados", 0 ) );

		m_buttons[i]->ShowSubmenuIndicator( Q_strncasecmp( "radialmenu ", command, Q_strlen( "radialmenu " ) ) == 0 );

		const char *owner = buttonData->GetString( "owner", NULL );
		if ( owner )
		{
			ButtonDir dir = DirFromButtonName( owner );
			m_buttons[i]->SetPassthru( m_buttons[dir] );
		}
		else
		{
			m_buttons[i]->SetPassthru( NULL );
		}

		m_buttons[i]->SetArmed( false );
	}
}

//--------------------------------------------------------------------------------------------------------
void CRadialMenu::ClearGlowEntity( void )
{
	// Stop glowing if we're done
	if ( m_nEntityGlowIndex != -1 )
	{
		g_GlowObjectManager.UnregisterGlowObject( m_nEntityGlowIndex );
		m_nEntityGlowIndex = -1;
	}
}

//--------------------------------------------------------------------------------------------------------
void CRadialMenu::OnRadialMenuOpen( void )
{
	if ( m_nArrowTexture == -1 )
	{
		m_nArrowTexture = vgui::surface()->CreateNewTextureID();
		vgui::surface()->DrawSetTextureFile( m_nArrowTexture, RADIAL_MENU_POINTER_TEXTURE, true, false );
	}

	m_fading = false;
	m_fadeStart = 0.0f;

	vgui::Button *firstButton = NULL;
	for ( int i=0; i<NUM_BUTTON_DIRS; ++i )
	{
		if ( !m_buttons[i] || !m_buttons[i]->IsVisible() || !m_buttons[i]->IsEnabled() )
			continue;

		if ( firstButton )
		{
			// already found another valid button.  since we have at least 2, we can show the menu.
			SetMouseInputEnabled( true );
			return;
		}

		firstButton = m_buttons[i];
	}
}


//--------------------------------------------------------------------------------------------------------
void FlushClientMenus( void )
{
	TheClientMenuManager.Flush();

	TheClientMenuManagerPlaytest.Flush();

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		GetClientMenuManagerTaunt( i ).Flush();
	}
}


//--------------------------------------------------------------------------------------------------------
void OpenRadialMenu( const char *lpszTargetClassification, EHANDLE hTargetEntity, const Vector &vPosition, const Vector &vNormal, RadialMenuTypes_t menuType )
{
	// FIXME: Need the equivalent here...
	// if ( GetTerrorClientMode() && GetTerrorClientMode()->IsInTransition() )
	//	return;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	C_Portal_Player *localPlayer = ToPortalPlayer( C_BasePlayer::GetLocalPlayer( nSlot ) );
	if ( IsCurrentMenuTypeDisabled( localPlayer, menuType ) )
	{
		return;
	}

	CRadialMenu *pRadialMenu = GET_HUDELEMENT( CRadialMenu );
	if ( !pRadialMenu )
		return;

	pRadialMenu->SetQuickPingForceClose( false );

	const char *pchTarget = ( ( lpszTargetClassification && lpszTargetClassification[ 0 ] != '\0' ) ? ( lpszTargetClassification ) : ( "Default" ) );

	if ( FStrEq( s_radialMenuName[ nSlot ], pchTarget ) )
	{
		bool wasOpen = pRadialMenu->IsVisible() && !pRadialMenu->IsFading();
		if ( wasOpen )
		{
			pRadialMenu->SetRadialType( menuType );

			return;
		}
	}

	if ( RadialMenuDebug.GetBool() )
	{
		FlushClientMenus(); // for now, reload every time
	}

	// Msg("Hit: %s\n", pchTarget );

	ClientMenuManager *pMM;
	if ( menuType == MENU_TAUNT )
	{
		pMM = &GetClientMenuManagerTaunt();
	}
	else if ( menuType == MENU_PING )
	{
		pMM = &TheClientMenuManager;
	}
	else
	{
		pMM = &TheClientMenuManagerPlaytest;
	}

	KeyValues *menuKey = pMM->FindMenu( pchTarget );
	if ( menuKey == NULL )
	{
		menuKey = pMM->FindMenu( "Default" );
		if ( menuKey == NULL )
		{
			DevMsg( "No client menu currently matches %s\n", pchTarget );

			ShowRadialMenuPanel( false );
			pRadialMenu->SetRadialMenuEnabled( false );
			return;
		}
	}

	V_snprintf( s_radialMenuName[ nSlot ], sizeof( s_radialMenuName[ nSlot ] ), pchTarget );
	pRadialMenu->SetData( menuKey );

	pRadialMenu->SetRadialType( menuType );

	if ( menuType == MENU_TAUNT || input->ControllerModeActive() )
	{
		pRadialMenu->SetFadeInTime( gpGlobals->curtime - 1.0f );
	}
	else
	{
		pRadialMenu->SetFadeInTime( gpGlobals->curtime + 0.5f );
	}

	pRadialMenu->ClearLockInTime();

	pRadialMenu->m_bFirstCentering = true;

	// Send up the specific data we need passed in
	pRadialMenu->SetTargetEntity( hTargetEntity );
	pRadialMenu->SetTraceData( vPosition, vNormal );

	if ( localPlayer )
	{
		localPlayer->EmitSound( "GameUI.UiCoopHudActivate" );
	}

	ShowRadialMenuPanel( true );
	pRadialMenu->SetRadialMenuEnabled( true );
	pRadialMenu->OnRadialMenuOpen();
}


//-----------------------------------------------------------------------------
// Purpose: Find and categorize our intended command target and let the client know
//-----------------------------------------------------------------------------
bool LaunchRadialMenu( int nPlayerSlot, RadialMenuTypes_t menuType )
{
	char szTarget[ MAX_PATH ] = { 0 };
	CBaseEntity *pTargetEntity = NULL;

	// Get our local target
	C_Portal_Player *pPlayer = ToPortalPlayer( C_BasePlayer::GetLocalPlayer( nPlayerSlot ) );
	if ( IsCurrentMenuTypeDisabled( pPlayer, menuType ) )
	{
		return false;
	}

	int nTeamSlot = ( pPlayer->GetTeamNumber() == TEAM_BLUE ? 0 : 1 );

	float *pfLastPingTime = &( CRadialMenu::m_fLastPingTime[ GET_ACTIVE_SPLITSCREEN_SLOT() ][ nTeamSlot ] );
	int *pfNumPings = &( CRadialMenu::m_nNumPings[ GET_ACTIVE_SPLITSCREEN_SLOT() ][ nTeamSlot ] );

	if ( menuType == MENU_TAUNT )
	{
		if ( pPlayer->PredictedAirTimeEnd() > 0.85f )
		{
			engine->ClientCmd( "taunt" );

			IGameEvent *event = gameeventmanager->CreateEvent( "player_gesture" );
			if ( event )
			{
				event->SetInt( "userid", pPlayer->GetUserID() );
				event->SetBool( "air", true );
				gameeventmanager->FireEventClientSide( event );
			}
			return false;
		}
	}
	else
	{
		float fNextPingTime = *pfLastPingTime + PING_DELAY_BASE + PING_DELAY_INCREMENT * *pfNumPings;
		float fPastNextPingTime = gpGlobals->curtime - fNextPingTime;

		if ( gpGlobals->curtime > *pfLastPingTime && fPastNextPingTime < 0.0f )
		{
			// They've been spamming pings, don't let them place another for now
			return false;
		}
	}

	// Clear this out for safety
	Vector vPosition = vec3_invalid;
	Vector vNormal = vec3_invalid;

	if ( menuType != MENU_TAUNT )
	{
		// Find what's under the cursor
		Vector vForward;
		pPlayer->EyeVectors( &vForward );
		Vector vEyeTrace = pPlayer->EyePosition() + ( vForward * MAX_TRACE_LENGTH );

		Ray_t ray;
		ray.Init( pPlayer->EyePosition(), vEyeTrace );

		float flTheNumberOne = 1.0f;
		C_Portal_Base2D *pHitPortal = UTIL_Portal_FirstAlongRay( ray, flTheNumberOne );
		C_Prop_Portal *pPropPoral = dynamic_cast<C_Prop_Portal *>(pHitPortal);

		trace_t tr;
		// Do a trace that respects portals (allows for portal-linked doors)
		CTraceFilterNoPlayers filter1;
		CTraceFilterSkipTwoEntities filter2( GetPlayerHeldEntity( pPlayer ), pPlayer->GetAttachedObject() );
		CTraceFilterChain filter( &filter1, &filter2 );
		UTIL_Portal_TraceRay( ray, (MASK_OPAQUE_AND_NPCS|CONTENTS_SLIME), &filter, &tr );

		pPlayer->CreatePingPointer( tr.endpos );

		// Did we hit a portal?
		if ( pHitPortal && pHitPortal->IsActivedAndLinked() && pPropPoral )
		{
			V_snprintf( szTarget, sizeof(szTarget), "Portal.%s", pHitPortal->m_bIsPortal2 ? "Orange" : "Blue" );
			pTargetEntity = pHitPortal;
		}
		else
		{
			// See if we passed through a tractor bream
			Ray_t ray;

			if ( !( ( tr.DidHitWorld() || ( tr.m_pEnt && tr.m_pEnt->IsBrushModel() ) ) && !( tr.contents & CONTENTS_SLIME ) && !IsNoPortalMaterial( tr ) ) )
			{
				// It's not a portal surface, so maybe they wanted to hit the bridge or tbeam
				for ( int i = 0; i < ITriggerTractorBeamAutoList::AutoList().Count(); ++i )
				{
					C_Trigger_TractorBeam *pTractorBeam = static_cast< C_Trigger_TractorBeam* >( ITriggerTractorBeamAutoList::AutoList()[ i ] );

					ray.Init( tr.startpos, tr.endpos );

					trace_t trTemp;
					enginetrace->ClipRayToEntity( ray, MASK_SHOT, pTractorBeam, &trTemp );

					if ( !trTemp.startsolid && ( trTemp.fraction < 1.0f || trTemp.m_pEnt == pTractorBeam ) )
					{
						tr = trTemp;
						tr.m_pEnt = ClientEntityList().GetBaseEntity( 0 );
						tr.surface.flags |= SURF_NOPORTAL;

						// Fix up the surface normal and ping position
						Vector vPointOnPath;
						CalcClosestPointOnLineSegment( tr.endpos, pTractorBeam->GetStartPoint(), pTractorBeam->GetEndPoint(), vPointOnPath, NULL );

						tr.plane.normal = tr.endpos - vPointOnPath;
						VectorNormalize( tr.plane.normal );

						tr.endpos = vPointOnPath + tr.plane.normal * pTractorBeam->GetBeamRadius();
					}
				}

				// See if we passed through a light bridge
				for ( int i = 0; i < IProjectedWallEntityAutoList::AutoList().Count(); ++i )
				{
					C_ProjectedWallEntity *pLightBridge = static_cast< C_ProjectedWallEntity* >( IProjectedWallEntityAutoList::AutoList()[ i ] );
					Vector vBridgeUp = pLightBridge->Up();
					if ( vBridgeUp.z > -0.4f && vBridgeUp.z < 0.4f )
					{
						// Don't hit wall bridges
						continue;
					}

					ray.Init( tr.startpos, tr.endpos );

					trace_t trTemp;
					enginetrace->ClipRayToEntity( ray, MASK_SHOT, pLightBridge, &trTemp );

					if ( trTemp.fraction < 1.0f || trTemp.m_pEnt == pLightBridge )
					{
						tr = trTemp;
						tr.m_pEnt = ClientEntityList().GetBaseEntity( 0 );
						tr.surface.flags |= SURF_NOPORTAL;

						if ( tr.plane.normal.z < -0.9f )
						{
							// Point down at the bridge from above so we can tell players to stand on it even when pointing from below
							tr.plane.normal.z = -tr.plane.normal.z;
							tr.endpos.z += 2.0f;
						}
					}
				}
			}

			// If it's an entity, just return that
			if ( tr.m_pEnt && tr.DidHitNonWorldEntity() && !tr.m_pEnt->IsBrushModel() )
			{
				// Fill out the details
				const char *lpszSignifier = tr.m_pEnt->GetSignifierName();

				Assert( lpszSignifier != NULL );
				V_snprintf( szTarget, sizeof(szTarget), "Entity.%s", lpszSignifier );

				// Initially, use this as the target entity
				pTargetEntity = tr.m_pEnt;

				if ( pTargetEntity && V_strstr( szTarget, "button" ) )
				{
					Vector vecBoundsMax, vecBoundsMin;
					pTargetEntity->GetRenderBounds( vecBoundsMin, vecBoundsMax );
					vPosition = pTargetEntity->WorldSpaceCenter();
					vPosition.z += (vecBoundsMax.z - 6);
					vNormal = Vector( 0, 0, 1 );
				}

				// Access the entity interface methods to get extra data
				ISignifierTarget *pSignifier = dynamic_cast<ISignifierTarget *>(tr.m_pEnt);
				if ( pSignifier != NULL )
				{
					// See if we're overriding our hit position
					if ( pSignifier->OverrideSignifierPosition() )
					{
						pTargetEntity = NULL;
						pSignifier->GetSignifierPosition( tr.endpos, vPosition, vNormal );
					}
				}
			}
			else if ( tr.DidHitWorld() || (tr.m_pEnt && tr.m_pEnt->IsBrushModel()) )
			{
				// We're going to use this position explicitly
				vPosition = tr.endpos;
				vNormal = tr.plane.normal;

				if ( tr.contents & CONTENTS_SLIME )
				{
					V_strncpy( szTarget, "World.Slime", sizeof(szTarget) );
				}
				else
				{
					if ( IsNoPortalMaterial( tr ) )
					{
						V_strncpy( szTarget, "World.NoPortal_", sizeof(szTarget) );
					}
					else
					{
						V_strncpy( szTarget, "World.", sizeof(szTarget) );
					}

					// If it's the world, then classify it
					if ( tr.plane.normal[2] > 0.75f )
					{
						V_strncat( szTarget, "Floor", sizeof(szTarget) );
					}
					else if ( tr.plane.normal[2] < -0.75f )
					{
						V_strncat( szTarget, "Ceiling", sizeof(szTarget) );
					}
					else
					{
						V_strncat( szTarget, "Wall", sizeof(szTarget) );
					}
				}
			}
			else
			{
				// Unknown entity type!
				Assert( 0 );
				return false;
			}
		}
	}

	// Launch the real menu
	OpenRadialMenu( szTarget, pTargetEntity, vPosition, vNormal, menuType );

	return true;
}

//--------------------------------------------------------------------------------------------------------
bool IsRadialMenuOpen( void )
{
	// Determine whether this window is visible or not
	CRadialMenu *pRadialMenu = GET_HUDELEMENT( CRadialMenu );
	if ( pRadialMenu )
	{
		bool isOpen = pRadialMenu->IsVisible() && !pRadialMenu->IsFading();
		return isOpen;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------
bool OpenRadialMenuCommand( RadialMenuTypes_t menuType )
{
	if ( !g_pGameRules )
		return false;

	if ( menuType != MENU_PLAYTEST && !g_pGameRules->IsMultiplayer() )
		return false;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	if ( s_mouseMenuKeyHeld[ nSlot ] )
		return true;

	bool bSuccess = LaunchRadialMenu( nSlot, menuType );

	if ( bSuccess )
	{
		s_mouseMenuKeyHeld[ nSlot ] = true;
	}

	return bSuccess;
}

void openradialmenu( const CCommand &args )
{
	OpenRadialMenuCommand( MENU_PING );
}
static ConCommand mouse_menu_open( "+mouse_menu", openradialmenu, "Opens a menu while held" );

void openradialmenutaunt( const CCommand &args )
{
	OpenRadialMenuCommand( MENU_TAUNT );
}
static ConCommand mouse_menu_taunt_open( "+mouse_menu_taunt", openradialmenutaunt, "Opens a menu while held" );

void openradialmenuplaytest( const CCommand &args )
{
	OpenRadialMenuCommand( MENU_PLAYTEST );
}
static ConCommand mouse_menu_playtest_open( "+mouse_menu_playtest", openradialmenuplaytest, "Opens a menu while held" );

//--------------------------------------------------------------------------------------------------------
void CloseRadialMenuCommand( RadialMenuTypes_t menuType, bool bForceClose /*= false*/ )
{
	if ( menuType != MENU_PLAYTEST && ( !g_pGameRules || !g_pGameRules->IsMultiplayer() ) )
		return;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	CRadialMenu *pRadialMenu = GET_HUDELEMENT( CRadialMenu );
	if ( !pRadialMenu )
		return;

	// Get our local target
	C_Portal_Player *pPlayer = ToPortalPlayer( C_BasePlayer::GetLocalPlayer( nSlot ) );
	if ( pPlayer )
		pPlayer->DestroyPingPointer();

	s_mouseMenuKeyHeld[ nSlot ] = false;

	if ( !cl_fastradial.GetBool() )
	{
		return;
	}

	bool wasOpen = pRadialMenu->IsVisible() && !pRadialMenu->IsFading();

	if ( wasOpen || bForceClose )
	{
		pRadialMenu->ChooseArmedButton();

		int wide, tall;
		pRadialMenu->GetSize( wide, tall );
		wide /= 2;
		tall /= 2;
		pRadialMenu->LocalToScreen( wide, tall );
		vgui::surface()->SurfaceSetCursorPos( wide, tall );

		if ( bForceClose )
		{
			ShowRadialMenuPanel( false );
			pRadialMenu->SetRadialMenuEnabled( false );
		}

		if ( !bForceClose )
		{
			input->Joystick_ForceRecentering( 0 );
			input->Joystick_ForceRecentering( 1 );
		}
	}
	else if ( !pRadialMenu->IsVisible() )
	{
		ShowRadialMenuPanel( false );
	}

	s_radialMenuName[ nSlot ][0] = 0;
}

void closeradialmenu( const CCommand &args )
{
	CloseRadialMenuCommand( MENU_PING );
}
static ConCommand mouse_menu_close( "-mouse_menu", closeradialmenu, "Executes the highlighted button on the radial menu (if cl_fastradial is 1)" );

void closeradialmenutaunt( const CCommand &args )
{
	CloseRadialMenuCommand( MENU_TAUNT );
}
static ConCommand mouse_menu_taunt_close( "-mouse_menu_taunt", closeradialmenutaunt, "Executes the highlighted button on the radial menu (if cl_fastradial is 1)" );

void closeradialmenuplaytest( const CCommand &args )
{
	CloseRadialMenuCommand( MENU_PLAYTEST );
}
static ConCommand mouse_menu_playtest_close( "-mouse_menu_playtest", closeradialmenuplaytest, "Executes the highlighted button on the radial menu (if cl_fastradial is 1)" );

extern bool UTIL_EntityBoundsToSizes( C_BaseEntity *pTarget, int *pMinX, int *pMinY, int *pMaxX, int *pMaxY );
extern bool UTIL_WorldSpaceToScreensSpaceBounds( const Vector &vecCenter, const Vector &mins, const Vector &maxs, Vector2D *pMins, Vector2D *pMaxs );

void cc_quickping( const CCommand &args )
{
	if ( !g_pGameRules || !g_pGameRules->IsMultiplayer() )
		return;

	if ( !OpenRadialMenuCommand( MENU_PING ) )
	{
		return;
	}

	CRadialMenu *pRadialMenu = GET_HUDELEMENT( CRadialMenu );
	if ( pRadialMenu )
	{
		pRadialMenu->SetArmedButtonDir( CRadialMenu::CENTER );
		pRadialMenu->SetQuickPingForceClose( true );
	}
	else
	{
		CloseRadialMenuCommand( MENU_PING, true );
	}
}
static ConCommand quick_ping("+quick_ping", cc_quickping, "Ping the center option from the ping menu.");

void closequickping( const CCommand &args )
{
}
static ConCommand quick_ping_close( "-quick_ping", closequickping, "Quick ping is unpressed... nothing to do here." );

//--------------------------------------------------------------------------------------------------------
class CSignifierSystem : public CAutoGameSystemPerFrame
{
	struct SignifierData_t
	{
		SignifierData_t( int index, int glowIndex, int playerIndex, int splitscreenID, EHANDLE hEntity, const Vector &vOrigin, float lifetime ) : 
			m_nLocatorIndex( index ), 
			m_nGlowIndex( glowIndex ),
			m_nPlayerIndex( playerIndex ),
			m_nSplitscreenID( splitscreenID ),
			m_hTargetEntity( hEntity), 
			m_vOrigin( vOrigin ),
			m_flLifetime( lifetime )
		{
		}

		int		m_nLocatorIndex;
		int		m_nGlowIndex;
		EHANDLE	m_hTargetEntity;
		Vector	m_vOrigin;
		float	m_flLifetime;
		int		m_nPlayerIndex;
		int		m_nSplitscreenID;
	};

public:
	CSignifierSystem() : CAutoGameSystemPerFrame( "CSignifierSystem" ) {}
	
	int GetObjectScreenHeight( C_BaseEntity *pObject )
	{
		if ( pObject == NULL )
			return 0;

		int minY = 0;
		int maxY = 0;
		UTIL_EntityBoundsToSizes( pObject, NULL, &minY, NULL, &maxY );

		return abs( maxY - minY );
	}

	// Pre-render
	virtual void PreRender( void ) 
	{
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		bool bDead = pPlayer && !pPlayer->IsAlive();	

		// Work through all our indicators and keep them up to date
		FOR_EACH_VEC_BACK( m_Signifiers, itr )
		{
			CLocatorTarget *pLocator = Locator_GetTargetFromHandle( m_Signifiers[itr].m_nLocatorIndex );
			if ( pLocator )
			{
				const int HEIGHT_PAD = 8;
				int nPad = ( pLocator->GetIconHeight() / 2 ) + HEIGHT_PAD;

				// Update our position
				if ( m_Signifiers[itr].m_hTargetEntity != NULL )
				{
					pLocator->m_vecOrigin = m_Signifiers[itr].m_hTargetEntity->WorldSpaceCenter();

					if ( V_strstr( m_Signifiers[itr].m_hTargetEntity->GetSignifierName(), "button" ) )
					{
						Vector vecBoundsMax, vecBoundsMin;
						m_Signifiers[itr].m_hTargetEntity.Get()->GetRenderBounds( vecBoundsMin, vecBoundsMax );
						pLocator->m_vecOrigin.z += vecBoundsMax.z + 42;
					}
					else
					{
						// We'd actually like to find the size of this entity on the screen and make the indicator float above the target!
						int nObjectHeight = GetObjectScreenHeight( m_Signifiers[itr].m_hTargetEntity.Get() );
						int nHeightOffset = ( nObjectHeight / 2 );
						pLocator->m_offsetY = -(nHeightOffset+nPad);
					}
				}
				else
				{
					// don't offset anymore
					/*
					Vector2D mins, maxs;
					UTIL_WorldSpaceToScreensSpaceBounds( m_Signifiers[itr].m_vOrigin, -Vector(8,8,8), Vector(8,8,8), &mins, &maxs );

					int nHeightOffset = abs(maxs.y - mins.y);
					pLocator->m_offsetY = -(nHeightOffset+nPad);
					*/
				}

				pLocator->Update();
				
				// Fade away
				if ( bDead || m_Signifiers[itr].m_flLifetime < gpGlobals->curtime || ( m_Signifiers[itr].m_hTargetEntity == NULL && m_Signifiers[itr].m_vOrigin == vec3_invalid ) )
				{
					// Remove the glow
					if ( m_Signifiers[itr].m_nGlowIndex != -1 )
					{
						g_GlowObjectManager.UnregisterGlowObject( m_Signifiers[itr].m_nGlowIndex );
					}

					Locator_RemoveTarget( m_Signifiers[itr].m_nLocatorIndex );
					m_Signifiers.FastRemove( itr );
				}
			}
		}
	}

	virtual void LevelShutdownPostEntity()
	{
		// Shut down all glows
		FOR_EACH_VEC( m_Signifiers, itr )
		{
			if ( m_Signifiers[itr].m_nGlowIndex != -1 )
			{
				g_GlowObjectManager.UnregisterGlowObject( m_Signifiers[itr].m_nGlowIndex );
			}
		}

		// Clear the list
		m_Signifiers.Purge();
	}

	// Add an indicator to the world
	void AddLocator( const char *lpszIconName, int nPlayerIndex, C_BaseEntity *pTarget, const Vector &vPosition, float flLifetime, Color rgbaColor, bool bScaleByDistance )
	{
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pLocalPlayer && !pLocalPlayer->IsAlive() )
		{
			return;
		}

		int nRecycledGlowIndex = -1;

		// First, remove any duplicated icons
		FOR_EACH_VEC_BACK( m_Signifiers, itr )
		{
			float flAlpha = 255;
			if ( pTarget )
				flAlpha = pTarget->GetRenderAlpha();

			// We only want one glow handle per entity, so "steal" the glow control away from older locators
			if ( m_Signifiers[itr].m_hTargetEntity && m_Signifiers[itr].m_hTargetEntity == pTarget && m_Signifiers[itr].m_nSplitscreenID == GET_ACTIVE_SPLITSCREEN_SLOT() && flAlpha > 0 )
			{
				nRecycledGlowIndex = m_Signifiers[itr].m_nGlowIndex;
				m_Signifiers[itr].m_nGlowIndex = -1;

				// Kill it
				Locator_RemoveTarget( m_Signifiers[itr].m_nLocatorIndex );
				m_Signifiers.FastRemove( itr );
			}
			else
			{
				CLocatorTarget *pLocator = Locator_GetTargetFromHandle( m_Signifiers[itr].m_nLocatorIndex );
				if ( pLocator )
				{
					if ( m_Signifiers[itr].m_nPlayerIndex == nPlayerIndex && FStrEq( pLocator->GetOnscreenIconTextureName(), lpszIconName ) )
					{
						// Remove the glow
						if ( m_Signifiers[itr].m_nGlowIndex != -1 )
						{
							g_GlowObjectManager.UnregisterGlowObject( m_Signifiers[itr].m_nGlowIndex );
						}

						// Kill it
						Locator_RemoveTarget( m_Signifiers[itr].m_nLocatorIndex );
						m_Signifiers.FastRemove( itr );
					}
				}
			}
		}

		// Now add a fresh one
		int nIndex = Locator_AddTarget();
		CLocatorTarget *pLocatorTarget = Locator_GetTargetFromHandle( nIndex );
		if ( pLocatorTarget )
		{
			pLocatorTarget->m_vecOrigin = ( pTarget != NULL ) ? pTarget->WorldSpaceCenter() : vPosition;
			pLocatorTarget->SetOnscreenIconTextureName( lpszIconName );
			pLocatorTarget->SetOffscreenIconTextureName( lpszIconName );
			pLocatorTarget->AddIconEffects( LOCATOR_ICON_FX_FORCE_CAPTION );	// Draw even when occluded

			if ( bScaleByDistance )
			{
				pLocatorTarget->AddIconEffects( LOCATOR_ICON_FX_SCALE_BY_DIST );
			}
			else
			{
				pLocatorTarget->AddIconEffects( LOCATOR_ICON_FX_SCALE_LARGE );
			}

			pLocatorTarget->SetIconColor( rgbaColor );

			pLocatorTarget->Update();
		}

		// dont glow players
		bool bTargetIsPlayer = ToPortalPlayer( pTarget ) != NULL;

		// Now, add a glow to this entity (unless we're recycling an old one)
		C_BasePlayer *pPingPlayer = UTIL_PlayerByIndex( nPlayerIndex );

		bool bRecycled = ( nRecycledGlowIndex != -1 && !bTargetIsPlayer );

		int nGlowIndex = bRecycled ? nRecycledGlowIndex : AddGlowToObject( pTarget, pPingPlayer ? pPingPlayer->GetTeamNumber() : 0 );

		if ( bRecycled )
		{
			Vector vColor;
			TeamPingColor( pPingPlayer ? pPingPlayer->GetTeamNumber() : 0, vColor );

			g_GlowObjectManager.SetColor( nGlowIndex, vColor );
		}

		// Hold it
		m_Signifiers.AddToTail( SignifierData_t( nIndex, nGlowIndex, nPlayerIndex, GET_ACTIVE_SPLITSCREEN_SLOT(), pTarget, vPosition, flLifetime ) );
	}
	
	CUtlVector<SignifierData_t> m_Signifiers;
};
CSignifierSystem g_SignifierSystem;

//--------------------------------------------------------------------------------------------------------
inline bool IsPortalOnFloor( CBaseEntity *pEntity )
{
	C_Portal_Base2D *pHitPortal = dynamic_cast<C_Portal_Base2D *>(pEntity);
	if ( pHitPortal )
	{
		Vector vForward;
		pHitPortal->GetVectors( &vForward, NULL, NULL );
		return ( vForward[2] > 0.75f );
	}

	return false;
}



//--------------------------------------------------------------------------------------------------------
CEG_NOINLINE void PlaceCommandTargetDecal( const Vector &vPosition, const Vector &vNormal, int iTeam, bool bJustArrows )
{
	// Recreate the trace that got us here
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	if ( !pPlayer )
		return;

	QAngle angNormal;
	VectorAngles( vNormal, angNormal );
	angNormal.x += 90.0f;

	Color color( 255, 255, 255 );
	if ( iTeam == TEAM_RED )
		color = UTIL_Portal_Color( 2, 0 );  //orange
	else
		color = UTIL_Portal_Color( 1, 0 );  //blue

	Vector vColor;
	vColor.x = color.r();
	vColor.y = color.g();
	vColor.z = color.b();

	if ( bJustArrows )
	{
		DispatchParticleEffect( "command_target_ping_just_arrows", vPosition, vColor, angNormal );
	}
	else
	{
		DispatchParticleEffect( "command_target_ping", vPosition, vColor, angNormal );
	}
}

CEG_PROTECT_FUNCTION( PlaceCommandTargetDecal );

//--------------------------------------------------------------------------------------------------------
void AddLocator( C_BaseEntity *pTarget, const Vector &vPosition, const Vector &vNormal, int nPlayerIndex, const char *caption, float fDisplayTime )
{	
	int nSplitscreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	bool bCountdown = StringHasPrefix( caption, "countdown" );

	if ( fDisplayTime > gpGlobals->curtime )
	{
		if ( bCountdown )
		{
			IGameEvent *event = gameeventmanager->CreateEvent( "player_countdown" );
			if ( event )
			{
				C_BasePlayer *pPlayer = UTIL_PlayerByIndex( nPlayerIndex );
				event->SetInt( "userid", pPlayer ? pPlayer->GetUserID() : 0 );
				gameeventmanager->FireEventClientSide( event );
			}

			// Count down animates
			CUtlVector< SignifierInfo_t > *pSignifierQueue = GetSignifierQueue();

			int nNew = pSignifierQueue->AddToTail();
			SignifierInfo_t *pNewSignifier = &((*pSignifierQueue)[ nNew ]);
			pNewSignifier->hTarget = pTarget;
			pNewSignifier->vPos = vPosition;
			pNewSignifier->vNormal = vNormal;
			pNewSignifier->nPlayerIndex = nPlayerIndex;
			V_strcpy( pNewSignifier->szCaption, "countdown_3" );
			pNewSignifier->fDisplayTime = fDisplayTime;

			nNew = pSignifierQueue->AddToTail();
			pNewSignifier = &((*pSignifierQueue)[ nNew ]);
			pNewSignifier->hTarget = pTarget;
			pNewSignifier->vPos = vPosition;
			pNewSignifier->vNormal = vNormal;
			pNewSignifier->nPlayerIndex = nPlayerIndex;
			V_strcpy( pNewSignifier->szCaption, "countdown_2" );
			pNewSignifier->fDisplayTime = fDisplayTime + 1.0f;

			nNew = pSignifierQueue->AddToTail();
			pNewSignifier = &((*pSignifierQueue)[ nNew ]);
			pNewSignifier->hTarget = pTarget;
			pNewSignifier->vPos = vPosition;
			pNewSignifier->vNormal = vNormal;
			pNewSignifier->nPlayerIndex = nPlayerIndex;
			V_strcpy( pNewSignifier->szCaption, "countdown_1" );
			pNewSignifier->fDisplayTime = fDisplayTime + 2.0f;

			nNew = pSignifierQueue->AddToTail();
			pNewSignifier = &((*pSignifierQueue)[ nNew ]);
			pNewSignifier->hTarget = pTarget;
			pNewSignifier->vPos = vPosition;
			pNewSignifier->vNormal = vNormal;
			pNewSignifier->nPlayerIndex = nPlayerIndex;
			V_strcpy( pNewSignifier->szCaption, "countdown_go" );
			pNewSignifier->fDisplayTime = fDisplayTime + 3.0f;
		}
		else
		{
			// Put it in the queue for later display
			CUtlVector< SignifierInfo_t > *pSignifierQueue = GetSignifierQueue();

			int nNew = pSignifierQueue->AddToTail();
			SignifierInfo_t *pNewSignifier = &((*pSignifierQueue)[ nNew ]);
			pNewSignifier->hTarget = pTarget;
			pNewSignifier->vPos = vPosition;
			pNewSignifier->vNormal = vNormal;
			pNewSignifier->nPlayerIndex = nPlayerIndex;
			V_strcpy( pNewSignifier->szCaption, caption );
			pNewSignifier->fDisplayTime = fDisplayTime;
		}
		return;
	}

	C_BasePlayer *pPlayer = UTIL_PlayerByIndex( nPlayerIndex );
	int nTeamSlot = ( ( pPlayer && pPlayer->GetTeamNumber() == TEAM_BLUE ) ? ( 0 ) : ( 1 ) );

	float *pfLastPingTime = &( CRadialMenu::m_fLastPingTime[ nSplitscreenSlot ][ nTeamSlot ] );
	int *pfNumPings = &( CRadialMenu::m_nNumPings[ nSplitscreenSlot ][ nTeamSlot ] );

	if ( !bCountdown )
	{
		// Reduce the current ping count for ones that have faded by now
		float fNextPingTime = *pfLastPingTime + PING_DELAY_BASE + PING_DELAY_INCREMENT * *pfNumPings;
		float fPastNextPingTime = gpGlobals->curtime - fNextPingTime;

		if ( gpGlobals->curtime > *pfLastPingTime && fPastNextPingTime < 0.0f )
		{
			// They've been spamming pings, don't let them place another for now
			return;
		}
		else if ( *pfNumPings > 0 )
		{
			// Pop off old pings that last about 2.5 seconds
			float fOldestPingFadeTime = *pfLastPingTime + 2.5f;

			for ( int i = 1; i <= *pfNumPings; ++i )
			{
				fOldestPingFadeTime -= i * PING_DELAY_INCREMENT;
			}

			float fTimePastOldestFaded = gpGlobals->curtime - fOldestPingFadeTime;

			while ( *pfNumPings > 0 && fTimePastOldestFaded > 0.0f )
			{
				fTimePastOldestFaded -= *pfNumPings * PING_DELAY_INCREMENT;
				(*pfNumPings)--;
			}
		}
	}

	*pfLastPingTime = gpGlobals->curtime;
	(*pfNumPings)++;

	const char *lpszCommand = caption;
	char szIconName[MAX_PATH];
	bool bAddDecal = false;
	bool bColor = true;
	bool bScaleByDistance = true;

	char szSound[ 32 ];
	V_strncpy( szSound, PING_SOUND_NAME, sizeof( szSound ) );

	if ( FStrEq( lpszCommand, "look" ) )
	{
		V_strncpy( szIconName, "icon_look", sizeof(szIconName) );
		bAddDecal = true;
	}
	else if ( FStrEq( lpszCommand, "death_blue" ) )
	{
		V_strncpy( szIconName, "icon_death_blue", sizeof(szIconName) );
		bColor = false;
		bScaleByDistance = false;
		szSound[ 0 ] = '\0';
	}
	else if ( FStrEq( lpszCommand, "death_orange" ) )
	{
		V_strncpy( szIconName, "icon_death_orange", sizeof(szIconName) );
		bColor = false;
		bScaleByDistance = false;
		szSound[ 0 ] = '\0';
	}
	else if ( FStrEq( lpszCommand, "countdown_3" ) )
	{
		V_strncpy( szIconName, "icon_countdown_3", sizeof(szIconName) );
		bScaleByDistance = false;
		V_strncpy( szSound, PING_SOUND_NAME_LOW, sizeof( szSound ) );
	}
	else if ( FStrEq( lpszCommand, "countdown_2" ) )
	{
		V_strncpy( szIconName, "icon_countdown_2", sizeof(szIconName) );
		bScaleByDistance = false;
		V_strncpy( szSound, PING_SOUND_NAME_LOW, sizeof( szSound ) );
	}
	else if ( FStrEq( lpszCommand, "countdown_1" ) )
	{
		V_strncpy( szIconName, "icon_countdown_1", sizeof(szIconName) );
		bScaleByDistance = false;
		V_strncpy( szSound, PING_SOUND_NAME_LOW, sizeof( szSound ) );
	}
	else if ( FStrEq( lpszCommand, "countdown_go" ) )
	{
		V_strncpy( szIconName, "icon_countdown_go", sizeof(szIconName) );
		bScaleByDistance = false;
		V_strncpy( szSound, PING_SOUND_NAME_HIGH, sizeof( szSound ) );
	}
	else if ( FStrEq( lpszCommand, "move_here" ) )
	{
		V_strncpy( szIconName, "icon_move_here", sizeof(szIconName) );
		bAddDecal = true;
	}
	else if ( FStrEq( lpszCommand, "portal_place_floor" ) )
	{
		V_strncpy( szIconName, "icon_portal_place_floor", sizeof(szIconName) );
		bAddDecal = true;
	}
	else if ( FStrEq( lpszCommand, "portal_place_wall" ) )
	{
		V_strncpy( szIconName, "icon_portal_place_wall", sizeof(szIconName) );
		bAddDecal = true;
	}
	else if ( FStrEq( lpszCommand, "portal_move" ) )
	{
		V_strncpy( szIconName, "icon_portal_move", sizeof(szIconName) );
	}
	else if ( FStrEq( lpszCommand, "portal_enter" ) )
	{
		if ( IsPortalOnFloor( pTarget ) )
		{
			V_strncpy( szIconName, "icon_portal_enter_floor", sizeof(szIconName) );
		}
		else
		{
			V_strncpy( szIconName, "icon_portal_enter_wall", sizeof(szIconName) );
		}
	}
	else if ( FStrEq( lpszCommand, "portal_exit" ) )
	{
		if ( IsPortalOnFloor( pTarget ) )
		{
			V_strncpy( szIconName, "icon_portal_exit_floor", sizeof(szIconName) );
		}
		else
		{
			V_strncpy( szIconName, "icon_portal_exit_wall", sizeof(szIconName) );
		}
	}
	else if ( FStrEq( lpszCommand, "slime_no_drink" ) )
	{
		V_strncpy( szIconName, "icon_slime_no_drink", sizeof(szIconName) );
	}
	else if ( FStrEq( lpszCommand, "box_pickup" ) )
	{
		V_strncpy( szIconName, "icon_box_pickup", sizeof(szIconName) );
	}
	else if ( FStrEq( lpszCommand, "box_putdown" ) )
	{
		V_strncpy( szIconName, "icon_box_drop", sizeof(szIconName) );
		bAddDecal = true;
	}
	else if ( FStrEq( lpszCommand, "button_press" ) )
	{
		V_strncpy( szIconName, "icon_button_press", sizeof(szIconName) );
	}
	else if ( FStrEq( lpszCommand, "button_tall_press" ) )
	{
		V_strncpy( szIconName, "icon_button_tall_press", sizeof(szIconName) );
	}
	else if ( FStrEq( lpszCommand, "turret_warning" ) )
	{
		V_strncpy( szIconName, "icon_turret_warning", sizeof(szIconName) );
	}
	else if ( FStrEq( lpszCommand, "love_it" ) )
	{
		V_strncpy( szIconName, "icon_love_it", sizeof( szIconName ) );
		bAddDecal = true;
	}
	else if ( FStrEq( lpszCommand, "stuck" ) )
	{
		V_strncpy( szIconName, "icon_stuck", sizeof( szIconName ) );
		bAddDecal = true;
	}
	else if ( FStrEq( lpszCommand, "hate_it" ) )
	{
		V_strncpy( szIconName, "icon_hate_it", sizeof( szIconName ) );
		bAddDecal = true;
	}
	else if ( FStrEq( lpszCommand, "confused" ) )
	{
		V_strncpy( szIconName, "icon_confused", sizeof( szIconName ) );
		bAddDecal = true;
	}
	else if ( FStrEq( lpszCommand, "done" ) )
	{
		return;
	}
	else
	{
		// Found an unknown command!
		Assert(0);
		return;
	}

	C_Team *pTeam = pPlayer ? pPlayer->GetTeam() : NULL;

	// Add a decal down where we pointed
	if ( bAddDecal )
	{
		bool bJustArrows = false;
		if ( pTarget && V_strstr( pTarget->GetSignifierName(), "button" ) )
		{
			bJustArrows = true;
		}

		int iTeam = pTeam ? pTeam->GetTeamNumber() : 0;
		PlaceCommandTargetDecal( vPosition, vNormal, iTeam, bJustArrows );
	}

	Color color( 255, 255, 255, 255 );

	if ( pPlayer )
	{
		if ( szSound[ 0 ] != '\0' )
		{
			pPlayer->EmitSound( szSound );
		}

		if ( bColor )
		{
			if ( pTeam && pTeam->GetTeamNumber() == TEAM_RED )
			{
				color = UTIL_Portal_Color( 2, 0 );  //orange
			}
			else
			{
				color = UTIL_Portal_Color( 1, 0 );  //blue
			}
		}
	}

	// if single player playtest, our alpha will come back 0, push it up
	if ( color.a() == 0 )
	{
		color.SetColor( color.r(), color.g(), color.b(), 255 );
	}

	float flLifetime = gpGlobals->curtime + ( bCountdown ? 1.0f : 3.0f );
	Vector vecSigPos = vPosition;
	vecSigPos += vNormal*64;
	g_SignifierSystem.AddLocator( szIconName, nPlayerIndex, pTarget, vecSigPos, flLifetime, color, bScaleByDistance );
}

//--------------------------------------------------------------------------------------------------------
static void __MsgFunc_AddLocator( bf_read &msg )
{
	// Find the index of the sending player
	int nPlayerIndex = msg.ReadShort();

	// Find the entity in question
	C_BaseEntity *pTarget = UTIL_EntityFromUserMessageEHandle( msg.ReadLong() );

	float fDisplayTime = msg.ReadFloat();
	
	Vector vPosition = vec3_invalid;
	Vector vNormal = vec3_invalid;
	msg.ReadBitVec3Coord( vPosition );
	msg.ReadBitVec3Normal( vNormal );

	// Find the name of the icon to show
	char iconName[2048]; 
	msg.ReadString( iconName, sizeof(iconName) );
	AddLocator( pTarget, vPosition, vNormal, nPlayerIndex, iconName, fDisplayTime );
}

USER_MESSAGE_REGISTER( AddLocator );
