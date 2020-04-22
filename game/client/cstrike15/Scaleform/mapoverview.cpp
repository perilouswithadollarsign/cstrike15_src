// ========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
// 
// Purpose:
// 
// =============================================================================//
#include "cbase.h"
#include "hudelement.h"
#include "mapoverview.h"
#include "hudelement.h"
#include "hud_macros.h"
#include <filesystem.h>
#include "cs_gamerules.h"
#include "c_team.h"
#include "c_cs_playerresource.h"
#include "c_plantedc4.h"
#include "c_cs_hostage.h"
#include "vtf/vtf.h"
#include "clientmode.h"
#include "voice_status.h"
#include "vgui/ILocalize.h"
#include <vstdlib/vstrtools.h>
#include "view.h"
#include "gameui_util.h"
#include "HUD/sfhudfreezepanel.h"
#include "HUD/sfhudwinpanel.h"
#include "HUD/sfhud_teamcounter.h"
#include "c_plantedc4.h"
#include "c_world.h"
#include "gametypes.h"
#include "hltvreplaysystem.h"
#include "clientmode_shared.h"

//memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const float NOTSPOTTEDRADIUS = 1400.0f;
static const float DEAD_FADE_TIME = 4.0f;
static const float GHOST_FADE_TIME = 6.0f;
static const float BOMB_FADE_TIME = 8.0f;
static const float DEFUSER_FADE_TIME = 8.0f;
static const float TIMER_INIT = -1000.0f;
static const int   INVALID_INDEX = -1;

ConVar mapoverview_allow_client_draw( "mapoverview_allow_client_draw", "0", FCVAR_RELEASE, "Allow a client to draw on the map overview" );
ConVar mapoverview_allow_grid_usage( "mapoverview_allow_grid_usage", "0", FCVAR_RELEASE, "When set to 1, allows turning on the (experimental) grid for drawing." );
ConVar mapoverview_icon_scale( "mapoverview_icon_scale", "1.0", FCVAR_RELEASE | FCVAR_ARCHIVE, "Sets the icon scale multiplier for the overview map. Valid values are 0.5 to 3.0.", true, 0.5f, true, 3.0f );

/*******************************************************************************
 * data definitions
 */

static const char* playerIconNames[] = {
	"Greande_HE",
	"Greande_FLASH",
	"Greande_SMOKE",
	"Greande_MOLOTOV",
	"Greande_DECOY",

	"Defuser",
	"AbovePlayer",
	"BelowPlayer",
	"Flashed",
	"PlayerNumber",
	"PlayerNameCT",
	"PlayerNameT",
	// below this, the icons are rotated
	"PlayerIndicator",
	"SpeakingOnMap",
	"HostageTransitOnMap",
	"CTOnMap",
	"CTDeath",
	"CTGhost",
	"TOnMap",
	"TDeath",
	"TGhost",
	"EnemyOnMap",
	"EnemyDeath",
	"EnemyGhost",
	"HostageOnMap",
	"HostageDeath",
	"HostageGhost",
	"DirectionalIndicator",
	"MuzzleFlash",
	"LowHealth",
	"Selected",
	NULL
};

static const char* hostageIconNames[] = {
	"HostageDead",
	"HostageRescued",
	"HostageAlive",
	"HostageTransit",
	NULL
};

static const char* overviewIconNames[] = {
	"BombPlantedIcon",
	"BombPlantedIconMedium",
	"BombPlantedIconFast",
	"HostageZoneIcon",
	NULL
};

static const char *desiredMessageNames[] = {
	"game_newmap",
	"round_start",
	"player_connect",
	"player_info",
	"player_team",
	"player_spawn",
	"player_death",
	"player_disconnect",
	"hostage_killed",
	"hostage_rescued",
	"bomb_defused",
	"bomb_exploded",
	"bomb_planted",
	"bomb_pickup",
	"bomb_dropped",
	"defuser_pickup",
	"defuser_dropped",
	"decoy_started",
	"decoy_detonate",
	"hegrenade_detonate",
	"flashbang_detonate",
	"smokegrenade_detonate",
	"smokegrenade_expired",
	"inferno_startburn",
	"inferno_expire",
	"bot_takeover",
	NULL
};

enum DESIRED_MESSAGE_INDICES
{
	GAME_NEWMAP,
	ROUND_POST_START,
	PLAYER_CONNECT,
	PLAYER_INFO,
	PLAYER_TEAM,
	PLAYER_SPAWN,
	PLAYER_DEATH,
	PLAYER_DISCONNECT,
	HOSTAGE_KILLED,
	HOSTAGE_RESCUED,
	BOMB_DEFUSED,
	BOMB_EXPLODED,
	BOMB_PLANTED,
	BOMB_PICKUP,
	BOMB_DROPPED,
	DEFUSER_PICKUP,
	DEFUSER_DROPPED,
	DECOY_STARTED,
	DECOY_DETONATE,
	HE_DETONATE,
	FLASH_DETONATE,
	SMOKE_DETONATE,
	SMOKE_EXPIRE,
	MOLOTOV_DETONATE,
	MOLOTOV_EXPIRE,
	BOT_TAKEOVER,

};

/***************************************************
 * callback and function declarations
 */

static bool maplessfunc( const char* const & s1, const char* const & s2 )
{
	return V_strcmp( s1, s2 ) < 0;
}

CUtlMap<const char*, int> SFMapOverview::m_messageMap( maplessfunc );

DECLARE_HUDELEMENT( SFMapOverview );
DECLARE_HUD_MESSAGE( SFMapOverview, ProcessSpottedEntityUpdate );

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( MapLoaded ),
	SFUI_DECL_METHOD( ToggleOverviewMap ),
	SFUI_DECL_METHOD( AllowMapDrawing ),
	SFUI_DECL_METHOD( GetWorldDistance ),
	//SFUI_DECL_METHOD( GetNavPath ),
SFUI_END_GAME_API_DEF( SFMapOverview, MapOverview );

CON_COMMAND( drawoverviewmap, "Draws the overview map" )
{
	( GET_HUDELEMENT( SFMapOverview ) )->ShowMapOverview( true );
}

CON_COMMAND( hideoverviewmap, "Hides the overview map" )
{
	( GET_HUDELEMENT( SFMapOverview ) )->ShowMapOverview( false );
}



/**********************************************************************
 * SFMapOverviewIconPackage code
 */

SFMapOverview::SFMapOverviewIconPackage::SFMapOverviewIconPackage()
{
	ClearAll();
}

SFMapOverview::SFMapOverviewIconPackage::~SFMapOverviewIconPackage()
{
}

void SFMapOverview::SFMapOverviewIconPackage::ClearAll( void )
{
	m_IconPackage = NULL;
	m_IconPackageRotate = NULL;
	V_memset( m_Icons, 0, sizeof( m_Icons ) );

	m_pScaleformUI = NULL;

	m_iCurrentVisibilityFlags = 0;
	m_fCurrentAlpha = 1.0f;

	m_Health = 0;
	m_iPlayerType = 0;
	m_iIndex = INVALID_INDEX;
	m_iEntityID = 0;

	m_bIsActive = false;
	m_bOffMap = true;

	m_bIsLowHealth = false;
	m_bIsSelected = false;
	m_bIsFlashed = false;
	m_bIsFiring = false;

	m_bIsSpotted = false;
	m_fGhostTime = TIMER_INIT;

	m_bIsDead = false;
	m_fDeadTime = TIMER_INIT;
	m_fGrenExpireTime = TIMER_INIT;

	m_IconPackType = ICON_PACK_PLAYER;
	m_bIsRescued = false;

	m_bIsOnLocalTeam = false;

	m_fRoundStartTime = TIMER_INIT;

	m_wcName[0] = 0;

	m_Position = vec3_origin;
	m_Angle.Init();

	m_nGrenadeType = -1;

	m_bIsDefuser = false;
}

void SFMapOverview::SFMapOverviewIconPackage::Init( IScaleformUI* pui, SFVALUE iconPackage )
{
	m_pScaleformUI = pui;
	m_IconPackage = pui->CreateValue( iconPackage );
	m_IconPackageRotate = m_pScaleformUI->Value_GetMember( iconPackage, "RotatedLayer" );

	for ( uint64 i = 0; i < PI_NUM_ICONS && playerIconNames[i] != NULL; i++ )
	{
		if ( i < PI_FIRST_ROTATED )
		{
			m_Icons[i] = pui->Value_GetMember( m_IconPackage, playerIconNames[i] );
		}
		else
		{
			if ( m_IconPackageRotate )
			{
				m_Icons[i] = pui->Value_GetMember( m_IconPackageRotate, playerIconNames[i] );
			}
		}

		if ( m_Icons[i] )
		{
			pui->Value_SetVisible( m_Icons[i], ( m_iCurrentVisibilityFlags & ( 1ll << i ) ) != 0 );
		}
	}

	ScaleformDisplayInfo displayInfo;
	displayInfo.SetAlpha( m_fCurrentAlpha * 100.0f );
	displayInfo.SetVisibility( m_iCurrentVisibilityFlags != 0 );
	m_pScaleformUI->Value_SetDisplayInfo( m_IconPackage, &displayInfo );
}


void SFMapOverview::SFMapOverviewIconPackage::NukeFromOrbit( SFMapOverview* pSFUI )
{
	if ( pSFUI->FlashAPIIsValid() )
	{
		pSFUI->SafeReleaseSFVALUE( m_IconPackage );
		pSFUI->SafeReleaseSFVALUE( m_IconPackageRotate );
		
		for ( uint64 i = 0; i < PI_NUM_ICONS; i++ )
		{
			pSFUI->SafeReleaseSFVALUE( m_Icons[i] );
		}
	}

	ClearAll();
}

void SFMapOverview::SFMapOverviewIconPackage::StartRound( void )
{
	m_Health = 100;

	m_bOffMap = false;
	m_bIsLowHealth = false;
	m_bIsSelected = false;
	m_bIsFlashed = false;
	m_bIsFiring = false;
	m_bIsSpotted = false;
	m_fGhostTime = TIMER_INIT;

	m_bIsDead = false;
	m_fDeadTime = TIMER_INIT;
	m_fGrenExpireTime = TIMER_INIT;

	m_bIsRescued = false;

	m_nAboveOrBelow = R_SAMELEVEL;

	m_fRoundStartTime = gpGlobals->curtime;

	m_nGrenadeType = -1;

	m_Position = vec3_origin;
	m_Angle.Init();

	SetAlpha( 0 );
	SetVisibilityFlags( 0 );
}

bool SFMapOverview::SFMapOverviewIconPackage::IsVisible( void )
{
	return ( m_fCurrentAlpha != 0 && m_iCurrentVisibilityFlags != 0 );
}


void SFMapOverview::SFMapOverviewIconPackage::SetIsPlayer( bool value )
{
	m_bIsPlayer = value;
}

void SFMapOverview::SFMapOverviewIconPackage::SetIsSpeaking ( bool value )
{
	m_bIsSpeaking = value && m_bIsOnLocalTeam;
}

void SFMapOverview::SFMapOverviewIconPackage::SetIsOffMap( bool value )
{
	m_bOffMap = value;
}

void SFMapOverview::SFMapOverviewIconPackage::SetIsLowHealth( bool value )
{
	m_bIsLowHealth = value;
}

void SFMapOverview::SFMapOverviewIconPackage::SetIsSelected( bool value )
{
	m_bIsSelected = value;
}

void SFMapOverview::SFMapOverviewIconPackage::SetIsFlashed( bool value )
{
	m_bIsFlashed = value;
}

void SFMapOverview::SFMapOverviewIconPackage::SetIsFiring( bool value )
{
	m_bIsFiring = value;
}

void SFMapOverview::SFMapOverviewIconPackage::SetIsAboveOrBelow( int value )
{
	m_nAboveOrBelow = value;
}

void SFMapOverview::SFMapOverviewIconPackage::SetIsMovingHostage( bool value )
{
	m_bIsMovingHostage = value;
}

void SFMapOverview::SFMapOverviewIconPackage::SetIsRescued( bool value )
{
	m_bIsRescued = value;
}

void SFMapOverview::SFMapOverviewIconPackage::SetIsOnLocalTeam( bool value )
{
	m_bIsOnLocalTeam = value;
}

void SFMapOverview::SFMapOverviewIconPackage::SetIsControlledBot( void )
{
	m_bIsDead = true;
	m_bIsSpotted = true;
	m_fDeadTime = TIMER_INIT;
	m_fGhostTime = TIMER_INIT;
}


void SFMapOverview::SFMapOverviewIconPackage::SetIsDead( bool value )
{
	if ( value != m_bIsDead )
	{
		if ( value )
		{
// 			SetIsSpotted( true );
			m_fDeadTime = gpGlobals->curtime;
		}
		else
		{
			m_fDeadTime = TIMER_INIT;
		}

		m_bIsDead = value;
	}

}

void SFMapOverview::SFMapOverviewIconPackage::SetGrenadeExpireTime( float value )
{
	if ( value )
	{
		m_fGrenExpireTime = value;
	}
}

void SFMapOverview::SFMapOverviewIconPackage::SetIsSpotted( bool value )
{
	if ( gpGlobals->curtime - m_fRoundStartTime > 0.25f )
	{
		if ( value != m_bIsSpotted )
		{
			if ( !value )
			{
				m_fGhostTime = gpGlobals->curtime;
			}
			else
			{
				m_fGhostTime = TIMER_INIT;
			}

			m_bIsSpotted = value;
		}
	}
}

void SFMapOverview::SFMapOverviewIconPackage::SetPlayerTeam( int team )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if( pLocalPlayer == NULL )
	{
		return;
	}

	int newType;

	switch( team )
	{
		case TEAM_UNASSIGNED:
			newType = PI_HOSTAGE;
			break;

		case TEAM_TERRORIST:
			newType = pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST || pLocalPlayer->IsObserver() ? PI_T : PI_ENEMY;
			break;

		case TEAM_CT:
			newType =pLocalPlayer->GetAssociatedTeamNumber() == TEAM_CT || pLocalPlayer->IsObserver() ? PI_CT : PI_ENEMY;
			break;

		case TEAM_SPECTATOR:
		default:
			newType = 0;
			break;
	}

	if ( m_iPlayerType != newType )
	{
		m_iPlayerType = newType;
		SetVisibilityFlags( 0 );
	}

}


void SFMapOverview::SFMapOverviewIconPackage::SetupIconsFromStates( void )
{
	uint64 flags = 0;
	float alpha = 1;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if( pLocalPlayer == NULL )
		return;

	bool bShowAll = false;
	if ( pLocalPlayer->IsSpectator() || g_bEngineIsHLTV )
		bShowAll = true;

	if ( m_iPlayerType != 0 )
	{
		flags = 1ll << m_iPlayerType;

		if ( m_bIsDead )
		{
			if ( m_Position.x == 0 && m_Position.y == 0 || !m_bIsSpotted )
			{
				flags = 0;
			}
			else
			{
				float deadElapsedTime = gpGlobals->curtime - m_fDeadTime;
				if ( deadElapsedTime > DEAD_FADE_TIME )
				{
					m_fDeadTime = TIMER_INIT;
					flags = 0;
				}
				else
				{
					alpha = clamp( 1.0f - deadElapsedTime / DEAD_FADE_TIME, 0.0f, 1.0f );

					// the shift below changes the icon to be the dead icon ( the X )
					// rescued hostages are marked as dead, but we don't want to show
					// the X in that case.  So if this guy is a hostage and has been resuced
					// we want to skip the code that changes the icon to the dead icon.

					bool isRescuedNotDead = IsHostageType() && m_bIsRescued;

					if ( !isRescuedNotDead )
					{
						flags <<= 1;
					}
				}
			}
		}
		else if ( !m_bIsSpotted )
		{
			float ghostElapsedTime = gpGlobals->curtime - m_fGhostTime;
			if ( ghostElapsedTime > GHOST_FADE_TIME )
			{
				m_fGhostTime = TIMER_INIT;
				flags = 0;
			}
			else
			{
				alpha = clamp( 1.0f - ghostElapsedTime / GHOST_FADE_TIME, 0.0f, 1.0f );
				// shifting to show the "ghost" icon
				flags <<= 2;
			}
		}
		else
		{
			if ( m_bIsMovingHostage )
			{
				flags |= 1 << PI_HOSTAGE_MOVING;
			}

			if ( m_bIsSpeaking )
			{
				flags |= 1 << PI_SPEAKING;
			}
			
			if ( m_bIsLowHealth )
			{
				flags |= 1 << PI_LOWHEALTH;
			}

			if ( m_bIsSelected )
			{
				flags |= 1 << PI_SELECTED;
			}

			if ( m_bIsFlashed )
			{
				flags |= 1 << PI_FLASHED;
			}

			if ( m_bIsFiring )
			{
				flags |= 1 << PI_MUZZLE_FLASH;
			}	

			bool bSameTeamOrSpec = false;
			if ( m_iPlayerType == PI_CT && (pLocalPlayer->GetAssociatedTeamNumber() == TEAM_CT || bShowAll) )
			{
				flags |= 1 << PI_PLAYER_NAME_CT;
				bSameTeamOrSpec = true;
			}

			if ( m_iPlayerType == PI_T && (pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST || bShowAll) )
			{
				flags |= 1 << PI_PLAYER_NAME_T;
				bSameTeamOrSpec = true;
			}

			if ( bSameTeamOrSpec )
				flags |= 1 << PI_PLAYER_NUMBER;
			
			
			flags |= 1 << PI_DIRECTION_INDICATOR;		

			if ( m_bIsPlayer )
			{
				flags |= 1 << PI_PLAYER_INDICATOR;
			}

			if ( m_fGrenExpireTime != TIMER_INIT )
			{
				if ( m_fGrenExpireTime < gpGlobals->curtime )
				{
					m_fGrenExpireTime = TIMER_INIT;
					m_bIsSpotted = false;
				}
			}
		}
	}
	else if ( m_bIsDefuser )
	{
		// only render after we have received a position update
		if ( m_Position != vec3_origin )
		{
			flags |= 1 << PI_DEFUSER;

			if ( m_bIsSpotted ) 
			{
				m_fGhostTime = TIMER_INIT;
				alpha = 1.0f;
			}
			else
			{
				float ghostElapsedTime = gpGlobals->curtime - m_fGhostTime;
				if ( ghostElapsedTime > GHOST_FADE_TIME )
				{
					m_fGhostTime = TIMER_INIT;
					flags = 0;
				}
				else
				{
					alpha = clamp( 1.0f - ghostElapsedTime / GHOST_FADE_TIME, 0.0f, 1.0f );
				}
			}
		}
	}
	else if ( m_nGrenadeType > -1 )
	{
		flags = 1ll << m_nGrenadeType;

		float flFadeTime = 1.0f;

		float deadElapsedTime = (m_fGrenExpireTime <= 0 ) ? 0 : gpGlobals->curtime - m_fGrenExpireTime;
		if ( deadElapsedTime > flFadeTime )
		{
			m_fGrenExpireTime = TIMER_INIT;
			flags = 0;
		}
		else
		{
			alpha = clamp( 1.0f - deadElapsedTime / flFadeTime, 0.0f, 1.0f );
		}
	}

	//GetPlayerSlotIndex( m_iEntityID/*engine->GetPlayerForUserID( m_iEntityID )*/ );

	SetAlpha( alpha );
	SetVisibilityFlags( flags );
}


void SFMapOverview::SFMapOverviewIconPackage::SetVisibilityFlags( uint64 newFlags )
{
	newFlags &= ( ( 1ll << PI_NUM_ICONS )-1 );

	uint64 diffFlags = m_iCurrentVisibilityFlags ^ newFlags;

	if ( diffFlags )
	{
		if ( m_IconPackage )
		{
			bool wantVisible = ( newFlags != 0 );
			if ( ( m_iCurrentVisibilityFlags != 0 ) != wantVisible )
			{
				m_pScaleformUI->Value_SetVisible( m_IconPackage, wantVisible );
			}

			for ( uint64 i = 0; i < PI_NUM_ICONS && ( diffFlags != 0 ); i++, diffFlags >>= 1 )
			{
				if ( (diffFlags & 1) && ( m_Icons[i] ) )
				{
					Assert( m_Icons[i] );
					if ( m_Icons[i] )
					{
						m_pScaleformUI->Value_SetVisible( m_Icons[i], ( newFlags & ( 1ll << i ) ) != 0 );
					}
				}
			}
		}

		m_iCurrentVisibilityFlags = newFlags;
	}
}
 
void SFMapOverview::SFMapOverviewIconPackage::SetAlpha( float newAlpha )
{
	if ( newAlpha != m_fCurrentAlpha )
	{
		m_fCurrentAlpha = newAlpha;

		if ( m_IconPackage )
		{
			ScaleformDisplayInfo displayInfo;
			displayInfo.SetAlpha( m_fCurrentAlpha * 100.0f );
			m_pScaleformUI->Value_SetDisplayInfo( m_IconPackage, &displayInfo );
		}
	}
}

void SFMapOverview::SFMapOverviewIconPackage::SetIsDefuse( bool bValue )
{
	m_bIsDefuser = bValue;
}

void SFMapOverview::SFMapOverviewIconPackage::SetGrenadeType( int nValue )
{
	m_nGrenadeType = nValue;
}

/**********************************************************************
 **********************************************************************
 **********************************************************************
 **********************************************************************
 **********************************************************************
 * SFHudHostageIcon
 */

SFMapOverview::SFMapOverviewHostageIcons::SFMapOverviewHostageIcons()
{
	m_IconPackage = NULL;
	V_memset( m_Icons, 0,sizeof( m_Icons ) );
	m_iCurrentIcon = HI_UNUSED;
}

SFMapOverview::SFMapOverviewHostageIcons::~SFMapOverviewHostageIcons()
{
}
 
void SFMapOverview::SFMapOverviewHostageIcons::Init( IScaleformUI* scaleformui, SFVALUE iconPackage )
{
	m_pScaleformUI = scaleformui;
	m_IconPackage = scaleformui->CreateValue( iconPackage );

	for ( int i = 0; i < HI_NUM_ICONS && hostageIconNames[i] != NULL; i++ )
	{
		m_Icons[i] = m_pScaleformUI->Value_GetMember( m_IconPackage, hostageIconNames[i] );
		if ( m_Icons[i] )
		{
			m_pScaleformUI->Value_SetVisible( m_Icons[i], m_iCurrentIcon == i );
		}
	}

	m_pScaleformUI->Value_SetVisible( m_IconPackage, m_iCurrentIcon != HI_UNUSED );
}

void SFMapOverview::SFMapOverviewHostageIcons::SetStatus( int status )
{
	if ( status != m_iCurrentIcon )
	{
		if ( m_IconPackage )
		{
			if ( m_iCurrentIcon != HI_UNUSED )
			{
				assert(m_Icons[m_iCurrentIcon]);
				m_pScaleformUI->Value_SetVisible( m_Icons[m_iCurrentIcon], false );
			}
			else
			{
				m_pScaleformUI->Value_SetVisible( m_IconPackage, true );
			}

			m_iCurrentIcon = status;

			if ( m_iCurrentIcon != HI_UNUSED )
			{
				assert(m_Icons[m_iCurrentIcon]);
				m_pScaleformUI->Value_SetVisible( m_Icons[m_iCurrentIcon], true );
			}
			else
			{
				m_pScaleformUI->Value_SetVisible( m_IconPackage, false );
			}
		}
	}
}


void SFMapOverview::SFMapOverviewHostageIcons::ReleaseHandles( SFMapOverview* pradar )
{
	if ( m_IconPackage && pradar->FlashAPIIsValid() )
	{
		pradar->SafeReleaseSFVALUE( m_IconPackage );

		for ( int i = 0; i < HI_NUM_ICONS; i++ )
		{
			pradar->SafeReleaseSFVALUE( m_Icons[i] );
		}
	}
}


/**********************************************************************
 **********************************************************************
 **********************************************************************
 **********************************************************************
 **********************************************************************
 * SFMapOverview Code
 *
 */

SFMapOverview::SFMapOverview( const char *value ) : SFHudFlashInterface( value ),
	m_fMapSize( 1.0f ),
	m_fMapScale( 1.0f ),
	m_fPixelToRadarScale( 1.0f ),
	m_fWorldToPixelScale( 1.0f ),
	m_fWorldToRadarScale( 1.0f ),
	m_RadarModule( NULL ),
	m_Radar( NULL ),
	m_IconTranslation( NULL ),
	m_IconRotation( NULL ),
	m_MapRotation( NULL ),
	m_MapTranslation( NULL ),
	m_iNumGoalIcons( 0 ),
	m_iLastPlayerIndex( INVALID_INDEX ),
	m_iLastHostageIndex( INVALID_INDEX ),
	m_iLastGrenadeIndex( INVALID_INDEX ),
	m_iLastDefuserIndex( INVALID_INDEX ),
	m_RadarViewpointWorld( vec3_origin ),
	m_RadarViewpointMap( vec3_origin ),
	m_RadarRotation( 0 ),
	m_BombPosition( vec3_origin ),
	m_DefuserPosition( vec3_origin ),
	m_iCurrentVisibilityFlags( 0 ),
	m_fBombSeenTime( TIMER_INIT ),
	m_fBombAlpha( 0 ),
	m_fDefuserSeenTime( TIMER_INIT ),
	m_fDefuserAlpha( 0.0f ),
	m_LocationText( NULL ),
	m_bFlashLoading( false ),
	m_bFlashReady( false ),
	m_bGotPlayerIcons( false ),	
	m_bShowingHostageZone( false ),
	m_bBombPlanted( false ),
	m_bBombDropped( false ),
	m_nBombEntIndex( -1 ),
	m_bShowBombHighlight( false ),
	m_bShowMapOverview( false ),
	m_bShowAll( false ),
	m_bShowingDashboard( false ),
	m_iObserverMode( OBS_MODE_NONE ),
	m_bTrackDefusers( false ),
	m_bVisible( false )
{
	//SetHiddenBits( HIDEHUD_RADAR );
    m_bWantLateUpdate = true;
	
	m_cDesiredMapName[0] = 0;
	m_cLoadedMapName[0] = 0;
	m_wcLocationString[0] = 0;

	V_memset( m_BombZoneIcons, 0, sizeof( m_BombZoneIcons ) );
	V_memset( m_HostageZoneIcons, 0, sizeof( m_HostageZoneIcons ) );
	V_memset( m_GoalIcons, 0, sizeof( m_GoalIcons ) );
	V_memset( m_HostageStatusIcons, 0, sizeof( m_HostageStatusIcons ) );
	V_memset( m_AllIcons, 0, sizeof( m_AllIcons ) );		

	m_EntitySpotted.ClearAll();
}


SFMapOverview::~SFMapOverview()
{
}

/**************************************
 * hud element functions
 */

void SFMapOverview::Init( void )
{
	// register for events as client listener

	const char** pwalk = &desiredMessageNames[0];

	while( *pwalk != NULL )
	{
		ListenForGameEvent( *pwalk );
		pwalk++;
	}

	if ( ! m_messageMap.Count() )
	{
		const char** pwalk = &desiredMessageNames[0];
		int i = 0;

		while( *pwalk != NULL )
		{
			m_messageMap.Insert( *pwalk++, i++ );
		}
	}

	HOOK_HUD_MESSAGE( SFMapOverview, ProcessSpottedEntityUpdate );
}

void SFMapOverview::LevelInit( void )
{
	if ( !m_bFlashReady && !m_bFlashLoading )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFMapOverview, this, MapOverview );
	}

}

void SFMapOverview::LevelShutdown( void )
{
	if ( m_bFlashReady || m_bFlashLoading )
	{
		RemoveFlashElement();
	}
}

bool SFMapOverview::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() || !m_bShowMapOverview )
		return false;

	return cl_drawhud.GetBool();// && CHudElement::ShouldDraw();
}

bool SFMapOverview::CanShowOverview( void )
{
	CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer && (pLocalPlayer->GetObserverMode() == OBS_MODE_NONE ||
		pLocalPlayer->GetObserverMode() == OBS_MODE_DEATHCAM || pLocalPlayer->GetObserverMode() == OBS_MODE_FREEZECAM ) )
	{
		return false;
	}
	if ( g_HltvReplaySystem.GetHltvReplayDelay() )
		return false;

	return true;
}


void SFMapOverview::SetActive( bool bActive )
{
	if ( bActive == true && !CanShowOverview() )
		return;

	Show( bActive );
	CHudElement::SetActive( bActive );
}

void SFMapOverview::Show( bool show )
{
	if ( show == true && !CanShowOverview() )
		return;

	if ( m_bFlashReady && show != m_bActive )
	{
		WITH_SLOT_LOCKED
		{
			if ( show )
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );
				m_bVisible = true;
			}
			else
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", NULL, 0 );
				m_bVisible = false;
			}
		}

		UpdateAllPlayerNamesAndNumbers();
	}

}

void SFMapOverview::ShowPanel( bool bShow )
{
	if ( bShow != m_bVisible )
	{
		if ( bShow )
		{
			Show( true );
		}
		else
		{
			Show( false );
		}
	}
}

// 
// /**********************************
//  * initialization code goes here
//  */
// 
// 
// these overload the ScaleformFlashInterfaceMixin class
void SFMapOverview::FlashLoaded( void )
{
	m_RadarModule = m_pScaleformUI->Value_GetMember( m_FlashAPI, "RadarModule" );

	if ( !m_RadarModule )
	{
		return;
	}

	m_Radar = m_pScaleformUI->Value_GetMember( m_RadarModule, "Radar" );

	if ( m_Radar )
	{

		m_MapRotation = m_pScaleformUI->Value_GetMember( m_Radar, "MapRotation" );

		if ( m_MapRotation )
		{
			m_MapTranslation =m_pScaleformUI->Value_GetMember( m_MapRotation, "MapTranslation" );
		}


		m_IconRotation = m_pScaleformUI->Value_GetMember( m_Radar, "IconRotation" );

		if ( !m_IconRotation )
		{
			return;
		}

		m_IconTranslation = m_pScaleformUI->Value_GetMember( m_IconRotation, "IconTranslation" );

		if ( !m_IconTranslation )
		{
			return;
		}

		char cHostageZoneIconName[20] = {"HZone0"};

		for ( int i = 0; i < MAX_HOSTAGE_RESCUES; i++ )
		{
			cHostageZoneIconName[5] = '0'+i;
			m_HostageZoneIcons[i] = m_pScaleformUI->Value_GetMember( m_IconTranslation, cHostageZoneIconName );
		}

		m_BombZoneIcons[0] = m_pScaleformUI->Value_GetMember( m_IconTranslation, "BombZoneA" );
		m_BombZoneIcons[1] = m_pScaleformUI->Value_GetMember( m_IconTranslation, "BombZoneB" );

	}

	SFVALUE dashboard = m_pScaleformUI->Value_GetMember( m_RadarModule, "Dashboard" );
	m_AllIcons[RI_DASHBOARD] = dashboard;

	if ( !dashboard )
	{
		return;
	}

	char hostageIconName[20] = {"HostageStatusX"};

	int index = 0;
	SFVALUE hostageIcon;

	do
	{
		hostageIconName[13] = index + '1';
		hostageIcon = m_pScaleformUI->Value_GetMember( dashboard, hostageIconName );
		if ( hostageIcon == NULL )
		{
			break;
		}
		else
		{
			m_HostageStatusIcons[index].Init( m_pScaleformUI, hostageIcon );
			SafeReleaseSFVALUE( hostageIcon );
		}

	} while( ++index < MAX_HOSTAGES );

	for ( int i = 0; overviewIconNames[i] != NULL; i++ )
	{
		m_AllIcons[i] = m_pScaleformUI->Value_GetMember( dashboard, overviewIconNames[i] );
	}

// 	m_LocationText = m_pScaleformUI->TextObject_MakeTextObjectFromMember( dashboard, "Location" );
// 	if ( m_LocationText )
// 	{
// 		m_LocationText->SetText( m_wcLocationString );
// 	}	
}


void SFMapOverview::FlashReady( void )
{
	// these aren't ready until after the first frame, so they have to get connected here instead of in
	// FlashLoaded()

	m_AllIcons[RI_BOMB_ICON_PACKAGE] = m_pScaleformUI->Value_Invoke( m_IconTranslation, "createBombPack", NULL, 0 );
	if ( m_AllIcons[RI_BOMB_ICON_PACKAGE] )
	{
		m_AllIcons[RI_BOMB_ICON_DROPPED] = m_pScaleformUI->Value_GetMember( m_AllIcons[RI_BOMB_ICON_PACKAGE], "DroppedBomb" );
		m_AllIcons[RI_BOMB_ICON_PLANTED] = m_pScaleformUI->Value_GetMember( m_AllIcons[RI_BOMB_ICON_PACKAGE], "PlantedBomb" );
		m_AllIcons[RI_BOMB_ICON_BOMB_CT] = m_pScaleformUI->Value_GetMember( m_AllIcons[RI_BOMB_ICON_PACKAGE], "BombCT" );
		m_AllIcons[RI_BOMB_ICON_BOMB_T] = m_pScaleformUI->Value_GetMember( m_AllIcons[RI_BOMB_ICON_PACKAGE], "BombT" );

		m_AllIcons[RI_BOMB_ICON_BOMB_ABOVE] = m_pScaleformUI->Value_GetMember( m_AllIcons[RI_BOMB_ICON_PACKAGE], "BombAbove" );
		m_AllIcons[RI_BOMB_ICON_BOMB_BELOW] = m_pScaleformUI->Value_GetMember( m_AllIcons[RI_BOMB_ICON_PACKAGE], "BombBelow" );
	}

	for ( int i = 0; i < RI_NUM_ICONS; i++ )
	{
		if ( m_AllIcons[i] )
		{
			m_pScaleformUI->Value_SetVisible( m_AllIcons[i], false );
		}
	}

	m_bFlashLoading = false;
	m_bFlashReady = true;
	ResetForNewMap();

	// there will be a map waiting for us to load

	FlashLoadMap( NULL );

	if ( m_bActive )
	{
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );
	}
	else
	{
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", NULL, 0 );
	}
 }

void SFMapOverview::LazyCreateGoalIcons( void )
{
	if ( m_bGotGoalIcons || !m_bFlashReady )
	{
		return;
	}

	// The goal entities don't exist on the client, so we have to get them from the CS Resource.
	C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* )GameResources();
	if ( !pCSPR )
	{
		return;
	}

	m_iNumGoalIcons = 0;

	if ( CSGameRules()->IsBombDefuseMap() )
	{

		Vector bombA = pCSPR->GetBombsiteAPosition();
		if( bombA != vec3_origin )
		{
			m_GoalIcons[m_iNumGoalIcons].m_Position = bombA;
			m_GoalIcons[m_iNumGoalIcons].m_Icon = m_BombZoneIcons[0];
			m_pScaleformUI->Value_SetVisible( m_GoalIcons[m_iNumGoalIcons].m_Icon, true );
			m_iNumGoalIcons++;
		}

		Vector bombB = pCSPR->GetBombsiteBPosition();
		if( bombB != vec3_origin )
		{
			m_GoalIcons[m_iNumGoalIcons].m_Position = bombB;
			m_GoalIcons[m_iNumGoalIcons].m_Icon = m_BombZoneIcons[1];
			m_pScaleformUI->Value_SetVisible( m_GoalIcons[m_iNumGoalIcons].m_Icon, true );
			m_iNumGoalIcons++;
		}
	}
	else if ( CSGameRules()->IsHostageRescueMap() )
	{
		for( int rescueIndex = 0; rescueIndex < MAX_HOSTAGE_RESCUES; rescueIndex++ )
		{
			int hIndex = 0;
			Vector hostageI = pCSPR->GetHostageRescuePosition( rescueIndex );
			if( hostageI != vec3_origin )
			{
				m_GoalIcons[m_iNumGoalIcons].m_Position = hostageI;
				m_GoalIcons[m_iNumGoalIcons].m_Icon = m_HostageZoneIcons[hIndex++];
				m_pScaleformUI->Value_SetVisible( m_GoalIcons[m_iNumGoalIcons].m_Icon, true );
				m_iNumGoalIcons++;
			}
		}
	}

	m_bGotGoalIcons = true;

}



bool SFMapOverview::PreUnloadFlash( void )
{
	if ( m_IconTranslation )
	{
		m_pScaleformUI->Value_InvokeWithoutReturn( m_IconTranslation, "removeBombPack", NULL, 0 );
	}

	for ( int i = 0; i < MAX_BOMB_ZONES; i++ )
	{
		SafeReleaseSFVALUE( m_BombZoneIcons[i] );
	}

	for ( int i = 0; i < MAX_HOSTAGE_RESCUES; i++ )
	{
		SafeReleaseSFVALUE( m_HostageZoneIcons[i] );
	}

	while( m_iLastPlayerIndex >= 0 )
	{
		RemovePlayer( m_iLastPlayerIndex );
	}

	while( m_iLastHostageIndex >= 0 )
	{
		RemoveHostage( m_iLastHostageIndex );
	}

	RemoveAllDefusers();

	RemoveAllGrenades();

	SafeReleaseSFVALUE( m_MapRotation );
	SafeReleaseSFVALUE( m_MapTranslation );
	SafeReleaseSFVALUE( m_RadarModule );
	SafeReleaseSFVALUE( m_Radar );
	SafeReleaseSFVALUE( m_IconTranslation );
	SafeReleaseSFVALUE( m_IconRotation );		

	SafeReleaseSFTextObject( m_LocationText );

	int index = 0;
	while( m_HostageStatusIcons[index].m_IconPackage )
	{
		m_HostageStatusIcons[index++].ReleaseHandles( this );
	}

	for ( int i =0 ; i < RI_NUM_ICONS; i++ )
	{
		SafeReleaseSFVALUE( m_AllIcons[i] );
	}

	m_bFlashReady = false;
	m_cDesiredMapName[0] = 0;
	m_cLoadedMapName[0] = 0;

	m_bActive = false;

	return true;
}

// /*********************************************************
//  * set up the background map
//  */

void SFMapOverview::SetMap( const char* pMapName )
{
	FlashLoadMap( pMapName );

	KeyValues* MapKeyValues = new KeyValues( pMapName );

	char tempfile[MAX_PATH];
	Q_snprintf( tempfile, sizeof( tempfile ), "resource/overviews/%s.txt", pMapName );

	if ( !MapKeyValues->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
	{
		DevMsg( 1, "Error! CMapOverview::SetMap: couldn't load file %s.\n", tempfile );
		m_MapOrigin.x = 0;
		m_MapOrigin.y = 0;
		return;
	}

	// TODO release old texture ?

	m_MapOrigin.x	= MapKeyValues->GetInt( "pos_x" );
	m_MapOrigin.y	= MapKeyValues->GetInt( "pos_y" );
	m_MapOrigin.z   = 0;
	m_fMapScale = MapKeyValues->GetFloat( "scale", 1.0f );
	m_fWorldToPixelScale = 1.0f / m_fMapScale;

	m_fWorldToRadarScale = m_fWorldToPixelScale * m_fPixelToRadarScale;

	MapKeyValues->deleteThis();

}


void SFMapOverview::FlashLoadMap( const char* pMapName )
{
	const char* pMapToLoad = pMapName ? pMapName : m_cDesiredMapName;

	if ( V_strcmp( pMapToLoad, m_cLoadedMapName ) )
	{
		if ( m_bFlashReady )
		{
			if ( pMapToLoad != m_cDesiredMapName )
			{
				V_strncpy( m_cDesiredMapName, pMapToLoad, sizeof( m_cDesiredMapName )+1 );
			}

			WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, pMapToLoad );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "loadMap", args, 1 );
			}
		}
		else
		{
			if ( pMapToLoad != m_cDesiredMapName )
			{
				V_strncpy( m_cDesiredMapName, pMapToLoad, sizeof( m_cDesiredMapName )+1 );
			}
		}
	}
}

void SFMapOverview::MapLoaded( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// flash passes in the scaledsize / image size
	m_fMapSize = ( float )pui->Params_GetArgAsNumber( obj, 0 );
	m_fPixelToRadarScale = ( float )pui->Params_GetArgAsNumber( obj, 1 );

// 	m_MapOrigin.x	= -(m_fMapSize/2);
// 	m_MapOrigin.y	= -(m_fMapSize/2);

	m_fWorldToRadarScale = m_fWorldToPixelScale * m_fPixelToRadarScale;

	V_strncpy( m_cLoadedMapName, m_cDesiredMapName, sizeof( m_cLoadedMapName )+1 );

	//Get the world's extent
	C_World *world = GetClientWorldEntity();
	if ( !world )
		return;

	Vector worldCenter = ( world->m_WorldMins + world->m_WorldMaxs ) * 0.5f;
}

void SFMapOverview::ToggleOverviewMap( SCALEFORM_CALLBACK_ARGS_DECL )
{
	ShowMapOverview( !IsMapOverviewShown() );
}

// /**********************************************
//  * reset functions
//  */
// 
// 
void SFMapOverview::ResetForNewMap( void )
{
	if ( m_bFlashReady )
	{
		for ( int i = 0; i < 2; i++ )
		{
			m_pScaleformUI->Value_SetVisible( m_BombZoneIcons[i], false );
		}

		for ( int i = 0; i < MAX_HOSTAGE_RESCUES; i++ )
		{
			m_pScaleformUI->Value_SetVisible( m_HostageZoneIcons[i], false );
		}

		while( m_iLastHostageIndex >= 0 )
		{
			RemoveHostage( m_iLastHostageIndex );
		}

		int index = 0;
		while( m_HostageStatusIcons[index].m_IconPackage && ( index < MAX_HOSTAGES ) )
		{
			m_HostageStatusIcons[index++].SetStatus( SFMapOverviewHostageIcons::HI_UNUSED );
		}

		RemoveAllGrenades();
		RemoveAllDefusers();
	}

	m_iNumGoalIcons = 0;
	m_bGotGoalIcons = false;

	ResetRoundVariables();

}

void SFMapOverview::ResetRoundVariables( bool bResetGlobalStates )
{
	SetVisibilityFlags( 0 );
	SetLocationText( NULL );

	m_RadarViewpointWorld = vec3_origin;
	m_RadarViewpointMap = vec3_origin;
	m_RadarRotation = 0;
	m_fBombSeenTime = TIMER_INIT;
	m_fBombAlpha = 0.0f;
	m_fDefuserSeenTime = TIMER_INIT;
	m_fDefuserAlpha = 0.0f;	
	m_bShowingHostageZone = false;

	m_bShowBombHighlight = false;
	m_bShowingDashboard = false;
	m_bShowAll = false;
	m_iObserverMode = OBS_MODE_NONE;
	ApplySpectatorModes(); // reapply here to make sure it stays when it's supposed to

	ConVarRef mp_defuser_allocation( "mp_defuser_allocation" );
	m_bTrackDefusers = ( mp_defuser_allocation.GetInt() == DefuserAllocation::Random );

	if ( bResetGlobalStates )
	{
		m_nBombEntIndex = -1;
		m_BombPosition = vec3_origin;
		m_DefuserPosition = vec3_origin;
		m_bBombIsSpotted = false;
		m_bBombPlanted = false;
		m_bBombDropped = false;
		m_bBombExploded = false;
		m_bBombDefused = false;
	}

	m_EntitySpotted.ClearAll();

	CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer && (pLocalPlayer->GetObserverMode() == OBS_MODE_NONE ) )
		ShowMapOverview( false );
}

void SFMapOverview::ShowMapOverview( bool bValue )
{
	if ( bValue == true && !CanShowOverview() )
		return;

	ClientModeShared *mode = ( ClientModeShared * )GetClientModeNormal();
	if ( mode )
	{
		mode->ReloadScheme();
	}
	
	m_bShowMapOverview = bValue;
}
 

void SFMapOverview::ResetRadar( bool bResetGlobalStates )
{
	for ( int i = 0; i <= m_iLastPlayerIndex; i++ )
	{
		ResetPlayer( i );
	}

	if ( CSGameRules()->IsHostageRescueMap() )
	{
		RemoveStaleHostages();

		for ( int i = 0; i <= m_iLastHostageIndex; i++ )
		{
			ResetHostage( i );
		}

		int index = 0;
		while( m_HostageStatusIcons[index].m_IconPackage && ( index < MAX_HOSTAGES ) )
		{
			m_HostageStatusIcons[index++].SetStatus( SFMapOverviewHostageIcons::HI_UNUSED );
		}
	}

	if ( bResetGlobalStates )
	{
		RemoveAllGrenades();
		RemoveAllDefusers();
	}

	ResetRoundVariables( bResetGlobalStates );
}

void SFMapOverview::ResetRound( void )
{
	ResetRadar( true );

	RefreshGraphs();
}

void SFMapOverview::RefreshGraphs( void )
{
	g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onRefresh", 0, NULL );
}

// /*****************************************************
//  * player icon loading / creating
//  */
// 
bool SFMapOverview::LazyUpdateIconArray( SFMapOverviewIconPackage* pArray, int lastIndex )
{
	bool result = true;
	int i;
	SFMapOverviewIconPackage* pwalk;

	for ( pwalk = pArray, i = 0; i <= lastIndex; i++, pwalk++ )
	{
		if ( pwalk->m_bIsActive && !pwalk->m_IconPackage )
		{
			if ( !LazyCreateIconPackage( pwalk ) )
			{
				// keep going, but remember that at least one wasn't loaded correctly
				result = false;
			}
		}
	}

	return result;
}

void SFMapOverview::LazyCreatePlayerIcons( void )
{
	if ( !m_bFlashReady || m_bGotPlayerIcons )
		return;

	// the following code looks odd, but it insures that all three of the LazyUpdate calls are made
	// even if m_bGotPlayerIcons becomes false early.

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Players, m_iLastPlayerIndex );

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Hostages, m_iLastHostageIndex ) && m_bGotPlayerIcons;

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Grenades, m_iLastGrenadeIndex ) && m_bGotPlayerIcons;

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Defusers, m_iLastDefuserIndex ) && m_bGotPlayerIcons;

}

bool SFMapOverview::LazyCreateIconPackage( SFMapOverviewIconPackage* pIconPack )
{
	if ( m_bFlashReady && pIconPack->m_bIsActive && !pIconPack->m_IconPackage )
	{
		SFVALUE result = NULL;

		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 2 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, pIconPack->m_iIndex );
			m_pScaleformUI->ValueArray_SetElement( args, 1, pIconPack->m_IconPackType );
			result = m_pScaleformUI->Value_Invoke( m_IconTranslation, "overviewCreateIconPack", args, 2 );
		
			pIconPack->Init( m_pScaleformUI, result );

			if ( result )
			{
				m_pScaleformUI->ReleaseValue( result );
			}		
		}

		return true;
	}
	else
	{
		return false;
	}
}

void SFMapOverview::InitIconPackage( SFMapOverviewIconPackage* pPlayer, int iAbsoluteIndex, ICON_PACK_TYPE iconType )
{
	if ( pPlayer->m_bIsActive && m_pScaleformUI )
	{
		RemoveIconPackage( pPlayer );
	}

	pPlayer->m_IconPackType = iconType;
	pPlayer->m_iIndex = iAbsoluteIndex;
	pPlayer->m_bIsActive = true;

	pPlayer->StartRound();

	if ( !pPlayer->m_IconPackage && !LazyCreateIconPackage( pPlayer ) )
	{
		m_bGotPlayerIcons = false;
	}

}

void SFMapOverview::RemoveIconPackage( SFMapOverviewIconPackage* pPackage )
{
	if ( pPackage->m_bIsActive && m_pScaleformUI )
	{
		if ( m_bFlashReady && pPackage->m_IconPackage )
		{
			WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, pPackage->m_IconPackage );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_IconTranslation, "removeIconPack", args, 1 );
			}
		}

		WITH_SLOT_LOCKED
		{
			pPackage->NukeFromOrbit( this );
		}
	}
}

 
// /**********************************************
//  * player and hostage initialization and creation
//  */


SFMapOverview::SFMapOverviewIconPackage* SFMapOverview::CreatePlayer( int index )
{
	SFMapOverviewIconPackage* pPackage = GetRadarPlayer( index );
	InitIconPackage( pPackage, index, ICON_PACK_PLAYER );

	m_iLastPlayerIndex = MAX( index, m_iLastPlayerIndex );

	return pPackage;
}


void SFMapOverview::ResetPlayer( int index )
{
	SFMapOverviewIconPackage* pPackage = GetRadarPlayer( index );
	if ( pPackage->m_bIsActive )
	{
		pPackage->StartRound();
	}

	UpdatePlayerNameAndNumber( pPackage );
}

void SFMapOverview::RemovePlayer( int index )
{
	RemoveIconPackage( GetRadarPlayer( index ) );

	if ( index == m_iLastPlayerIndex )
	{
		while( m_iLastPlayerIndex >= 0 && !m_Players[m_iLastPlayerIndex].m_bIsActive )
		{
			m_iLastPlayerIndex--;
		}
	}

	UpdateAllPlayerNamesAndNumbers();
}

SFMapOverview::SFMapOverviewIconPackage* SFMapOverview::CreateHostage( int index )
{
	SFMapOverviewIconPackage* pPackage = GetRadarHostage( index );
	InitIconPackage( pPackage, index, ICON_PACK_HOSTAGE );

	m_iLastHostageIndex = MAX( index, m_iLastHostageIndex );

	return pPackage;
}

void SFMapOverview::ResetHostage( int index )
{
	SFMapOverviewIconPackage* pPackage = GetRadarHostage( index );
	if ( pPackage->m_bIsActive )
	{
		pPackage->StartRound();
	}
}

void SFMapOverview::RemoveStaleHostages( void )
{
	// Remove hostages that are no longer tracked by the player resource (hostages that have died)

	C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* )GameResources();
	if ( !pCSPR )
	{
		return;
	}

	for ( int index = 0; index < MAX_HOSTAGES; index++ )
	{
		SFMapOverviewIconPackage* pPackage = GetRadarHostage( index );
		if ( pPackage->m_bIsActive )
		{
			bool bRemove = true;

			for( int i = 0; i < MAX_HOSTAGES; i++ )
			{
				if ( pCSPR->GetHostageEntityID( i ) == pPackage->m_iEntityID )
				{
					bRemove = false;
					break;
				}
			}

			if ( bRemove )
			{
				RemoveHostage( index );
			}
		}
	}
}

void SFMapOverview::RemoveHostage( int index )
{
	RemoveIconPackage( GetRadarHostage( index ) );

	if ( index == m_iLastHostageIndex )
	{
		while( m_iLastHostageIndex >= 0 && !m_Hostages[m_iLastHostageIndex].m_bIsActive )
		{
			m_iLastHostageIndex--;
		}
	}
}


SFMapOverview::SFMapOverviewIconPackage* SFMapOverview::CreateGrenade( int entityID, int nGrenadeType )
{
	SFMapOverviewIconPackage* pPackage = NULL;
	int index = m_iLastGrenadeIndex+1;

	for ( int i = 0; i <= m_iLastGrenadeIndex; i++ )
	{
		if ( !m_Grenades[i].m_bIsActive )
		{
			index = i;
			break;
		}
	}

	if ( index < MAX_GRENADES )
	{
		pPackage = GetRadarGrenade( index );

		InitIconPackage( pPackage, index, ICON_PACK_GRENADES );

		pPackage->m_iEntityID = entityID;
		pPackage->m_fRoundStartTime = TIMER_INIT;

		m_iLastGrenadeIndex = MAX( index, m_iLastGrenadeIndex );
	}

	return pPackage;
}

void SFMapOverview::RemoveAllGrenades( void )
{
	for ( int i = 0; i <= m_iLastGrenadeIndex; i++ )
	{
		if ( m_Grenades[i].m_bIsActive )
		{
			RemoveGrenade( i );
		}
	}
}


void SFMapOverview::RemoveGrenade( int index )
{
	RemoveIconPackage( GetRadarGrenade( index ) );

	if ( index == m_iLastGrenadeIndex )
	{
		while( m_iLastGrenadeIndex >= 0 && !m_Grenades[m_iLastGrenadeIndex].m_bIsActive )
		{
			m_iLastGrenadeIndex--;
		}
	}
}

SFMapOverview::SFMapOverviewIconPackage * SFMapOverview::GetDefuser( int nEntityID, bool bCreateIfNotFound )
{
	SFMapOverviewIconPackage * pResult = NULL;

	int nIndex = GetDefuseIndexFromEntityID( nEntityID );

	if ( nIndex != INVALID_INDEX )
	{
		pResult = GetRadarDefuser( nIndex );
	}

	if ( pResult == NULL && bCreateIfNotFound )
	{
		pResult = CreateDefuser( nEntityID );
	}

	return pResult;
}



void SFMapOverview::SetDefuserPos( int nEntityID, int x, int y, int z, int a )
{
	if ( !m_bTrackDefusers )
	{
		return;
	}

	SFMapOverviewIconPackage * pDefuser = GetDefuser( nEntityID, true );

	AssertMsg( pDefuser != NULL, "Defuser not found. Update failed." );

	if ( pDefuser )
	{
		pDefuser->m_Position.Init( x, y, z ) ;
		pDefuser->SetAlpha( 1.0f );

		SetIconPackagePosition( pDefuser );
	}
}

// defusers attached to players will update during the PlacePlayer phase
SFMapOverview::SFMapOverviewIconPackage* SFMapOverview::CreateDefuser( int nEntityID )
{
	if ( !m_bTrackDefusers )
	{
		return NULL;
	}

	SFMapOverviewIconPackage* pPackage = NULL;
	int index = m_iLastDefuserIndex+1;

	AssertMsg( GetDefuseIndexFromEntityID( nEntityID ) == INVALID_INDEX, "Defuser entity ID already exists." );

	for ( int i = 0; i <= m_iLastDefuserIndex; i++ )
	{
		if ( !m_Defusers[i].m_bIsActive )
		{
			index = i;
			break;
		}
	}

	if ( index < MAX_PLAYERS )
	{
		pPackage = GetRadarDefuser( index );

		InitIconPackage( pPackage, index, ICON_PACK_DEFUSER );

		pPackage->m_iEntityID = nEntityID;
		pPackage->m_fRoundStartTime = TIMER_INIT;
		pPackage->SetIsSpotted( false );
		pPackage->SetIsDefuse( true );
		pPackage->m_Position = vec3_origin;

		m_iLastDefuserIndex = MAX( index, m_iLastDefuserIndex );
	}
	else
	{
		AssertMsg( false, "m_Defusers array is full" );
	}

	return pPackage;
}

void SFMapOverview::RemoveAllDefusers( void )
{
	for ( int i = 0; i <= m_iLastDefuserIndex; i++ )
	{
		if ( m_Defusers[i].m_bIsActive )
		{
			RemoveDefuser( i );
		}
	}
}

void SFMapOverview::RemoveDefuser( int index )
{
	RemoveIconPackage( GetRadarDefuser( index ) );

	if ( index == m_iLastDefuserIndex )
	{
		while( m_iLastDefuserIndex >= 0 && !m_Defusers[m_iLastDefuserIndex].m_bIsActive )
		{
			m_iLastDefuserIndex--;
		}
	}
}



void SFMapOverview::SetPlayerTeam( int index, int team )
{
	SFMapOverviewIconPackage* pPlayer = GetRadarPlayer( index );
	pPlayer->SetPlayerTeam( team );
}

int SFMapOverview::GetPlayerIndexFromUserID( int userID )
{
	int nEntityID = engine->GetPlayerForUserID( userID );
	for ( int i = 0; i <= m_iLastPlayerIndex; i++ )
	{
		if ( m_Players[i].m_bIsActive && m_Players[i].m_iEntityID == nEntityID )
		{
			return i;
		}
	}

	return INVALID_INDEX;
}

int SFMapOverview::GetHostageIndexFromHostageEntityID( int entityID )
{
	for ( int i = 0; i <= m_iLastHostageIndex; i++ )
	{
		if ( m_Hostages[i].m_bIsActive && m_Hostages[i].m_iEntityID == entityID )
		{
			return i;
		}
	}

	return INVALID_INDEX;
}

int SFMapOverview::GetGrenadeIndexFromEntityID( int entityID )
{
	for ( int i = 0; i <= m_iLastGrenadeIndex; i++ )
	{
		if ( m_Grenades[i].m_bIsActive && m_Grenades[i].m_iEntityID == entityID )
		{
			return i;
		}
	}

	return INVALID_INDEX;
}

int SFMapOverview::GetDefuseIndexFromEntityID( int nEntityID )
{
	for ( int i = 0; i <= m_iLastDefuserIndex; i++ )
	{
		if ( m_Defusers[i].m_bIsActive && m_Defusers[i].m_iEntityID == nEntityID )
		{
			return i;
		}
	}

	return INVALID_INDEX;
}



SFMapOverview::SFMapOverviewIconPackage* SFMapOverview::GetRadarPlayer( int index )
{
	return &m_Players[index];
}

SFMapOverview::SFMapOverviewIconPackage* SFMapOverview::GetRadarHostage( int index )
{
	return &m_Hostages[index];
}

SFMapOverview::SFMapOverviewIconPackage* SFMapOverview::GetRadarGrenade( int index )
{
	return &m_Grenades[index];
}

SFMapOverview::SFMapOverviewIconPackage* SFMapOverview::GetRadarDefuser( int index )
{
	return &m_Defusers[index];
}

// /*************************************************
//  * We don't get updates from the server from players
//  * that are not in our PVS, so there are separate
//  * messages specifically to update the radar
//  */
bool SFMapOverview::MsgFunc_ProcessSpottedEntityUpdate( const CCSUsrMsg_ProcessSpottedEntityUpdate &msg )
{
	if ( msg.new_update() )
	{
		// new spotting frame. clean up.
		m_bBombIsSpotted = false;
		m_EntitySpotted.ClearAll();
	}


	for ( int i = 0; i < msg.entity_updates_size(); i ++ )
	{
		const CCSUsrMsg_ProcessSpottedEntityUpdate::SpottedEntityUpdate &update = msg.entity_updates(i);

		int nEntityID = update.entity_idx();
		if ( nEntityID < 0 || nEntityID >= MAX_EDICTS )
			continue; // GeekPwn2016 range check

		m_EntitySpotted.Set( nEntityID, true );

		const char * szEntityClass = NULL;

		int nClassID =  update.class_id();
		for ( ClientClass *pCur = g_pClientClassHead; pCur; pCur=pCur->m_pNext )
		{
			if (  pCur->m_ClassID == nClassID )
			{
				szEntityClass = pCur->GetName();
				break;
			}
		}

		if ( szEntityClass == NULL )
		{
			Warning( "Unknown entity class received in ProcessSpottedEntityUpdate.\n" );
		}

		// non-class specific data
		// read position and angle
		int x = update.origin_x() * 4;
		int y = update.origin_y() * 4;
		int z = update.origin_z() * 4;
		int a = update.angle_y();


		// Clients are unaware of the defuser class type, so we need to flag defuse entities manually
		if ( update.defuser() )
		{
			SetDefuserPos( nEntityID, x, y, z, a );
		}
		else if ( V_strcmp( "CCSPlayer", szEntityClass ) == 0 )
		{
			SFMapOverviewIconPackage* pPlayerIcon = NULL;

			pPlayerIcon = GetRadarPlayer( nEntityID - 1 );

			pPlayerIcon->m_Position.Init( x, y, z );
			pPlayerIcon->m_Angle.Init( 0, a, 0 );

			// has defuser?
			if (  update.player_has_defuser() )
			{
				SetDefuserPos( nEntityID, x, y, z, a );
			}

			// has C4?
			if (  update.player_has_c4() )
			{
				m_bBombIsSpotted = true;
				m_BombPosition.Init( x, y, 0 );
			}

			// The following code should no longer be necessary with the new spotting system
			//C_CSPlayer *pPlayer =	ToCSPlayer( UTIL_PlayerByIndex( nEntityID ) );

			//// Only update players that are outside of PVS
			//if ( pPlayer && pPlayer->IsDormant() )
			//{
			//	// update origin and angle for players out of my PVS
			//	Vector origin = pPlayer->GetAbsOrigin();
			//	QAngle angles = pPlayer->GetAbsAngles();

			//	origin.x = x;
			//	origin.y = y;
			//	angles.y = a;

			//	pPlayer->SetAbsOrigin( origin );
			//	pPlayer->SetAbsAngles( angles );
			//}
		}
		else if ( V_strcmp( "CC4", szEntityClass ) == 0 || V_strcmp( "CPlantedC4", szEntityClass ) == 0 )
		{
			m_bBombIsSpotted = true;
			m_BombPosition.Init( x, y, 0 );
		}
		else if ( V_strcmp( "CHostage", szEntityClass ) == 0 )
		{
			int hostageIndex = GetHostageIndexFromHostageEntityID( nEntityID );

			if ( hostageIndex == INVALID_INDEX )
			{
				for ( int i = 0; i <= m_iLastHostageIndex; i++ )
				{
					RemoveHostage( i );
				}
			}
			else
			{
				SFMapOverviewIconPackage* pPackage = GetRadarHostage( hostageIndex );

				if ( pPackage )
				{
					pPackage->m_Position.Init( x, y, z );
				}
			}
		}
		else
		{
			Warning( "Unknown entity update received by ProcessSpottedEntityUpdate: %s.\n", szEntityClass );
		}

	}

	return true;
}

void SFMapOverview::UpdateAllPlayerNamesAndNumbers( void )
{
	//int nMaxPlayers = CCSGameRules::GetMaxPlayers();

	for ( int i = 0; i <= MAX_PLAYERS; ++i )
	{
		SFMapOverviewIconPackage* pPackage = GetRadarPlayer( i );
		if ( pPackage )
			UpdatePlayerNameAndNumber( pPackage );
	}
}

void SFMapOverview::UpdatePlayerNameAndNumber( SFMapOverviewIconPackage* pPackage )
{
	int nMaxPlayers = CCSGameRules::GetMaxPlayers();

	// update the player number
	SFHudTeamCounter* pTeamCounter = GET_HUDELEMENT( SFHudTeamCounter );
	if ( pPackage && pPackage->m_Icons[PI_PLAYER_NUMBER] && pPackage->m_Icons[PI_PLAYER_NAME_CT] && pPackage->m_Icons[PI_PLAYER_NAME_T] )
	{
		if ( pTeamCounter )
		{
			int pidx = pTeamCounter->GetPlayerSlotIndex( pPackage->m_iEntityID  );
			if ( pidx != -1 )
			{
				pidx += 1;
				if ( pidx == 10 )
					pidx = 0;

				ISFTextObject* textPanel =	g_pScaleformUI->TextObject_MakeTextObjectFromMember( pPackage->m_Icons[PI_PLAYER_NUMBER], "Text" );	
				if ( textPanel )
				{
					CBasePlayer * pPlayer = CBasePlayer::GetLocalPlayer();
					bool bIsSpec = (pPlayer && (m_bShowAll || pPlayer->GetObserverMode() != OBS_MODE_NONE ));
					WITH_SLOT_LOCKED
					{			
						if ( nMaxPlayers <= 10 && bIsSpec )
							textPanel->SetText( pidx );
						else
							textPanel->SetText( "" );
					}
					SafeReleaseSFTextObject( textPanel );
				}
			}
		}

		int nTeamSlot = PI_PLAYER_NAME_CT;
		if ( pPackage->m_iPlayerType == PI_T )
			nTeamSlot = PI_PLAYER_NAME_T;

		ISFTextObject* textPanelName =	g_pScaleformUI->TextObject_MakeTextObjectFromMember( pPackage->m_Icons[nTeamSlot], "Text" );	
		if ( textPanelName )
		{
			WITH_SLOT_LOCKED
			{
				textPanelName->SetText( pPackage->m_wcName );
			}
			SafeReleaseSFTextObject( textPanelName );
		}
	}
}

//handles all game event messages ( first looks them up in map so there's no long list of strcmps )
void SFMapOverview::FireGameEvent( IGameEvent *event )
{
	const char* eventName = event->GetName();
	int elementIndex = m_messageMap.Find( eventName );
	if ( elementIndex == m_messageMap.InvalidIndex() )
	{
		return;
	}

	SF_SPLITSCREEN_PLAYER_GUARD();

	CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	int EventUserID = event->GetInt( "userid", -1 );
	int LocalPlayerID = ( pLocalPlayer != NULL ) ? pLocalPlayer->GetUserID() : -2;

	DESIRED_MESSAGE_INDICES messageTypeIndex = ( DESIRED_MESSAGE_INDICES )m_messageMap.Element( elementIndex );

	switch( messageTypeIndex )
	{
		case GAME_NEWMAP:
			ResetForNewMap();
			SetMap( event->GetString( "mapname" ) );
		break;

		case ROUND_POST_START:
			ResetRound();
		break;


		case PLAYER_CONNECT:
		case PLAYER_INFO:
		{
			int index = event->GetInt( "index" ); // = entity index - 1
			int userID = index + 1;

			if ( index < 0 || index >= MAX_PLAYERS )
			{
				return;
			}

			SFMapOverviewIconPackage* pPackage = CreatePlayer( index );
			if ( messageTypeIndex == PLAYER_CONNECT )
			{
				pPackage->SetPlayerTeam( TEAM_SPECTATOR );
			}

			pPackage->m_iEntityID = userID;

			const char* name = event->GetString( "name","unknown" );
			if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
			{
				if ( pParameters->m_bAnonymousPlayerIdentity )
					name = ""; // suppress names for anonymous playback
			}

			V_UTF8ToUnicode( name, pPackage->m_wcName, sizeof( pPackage->m_wcName ) );
		}
		break;

		case PLAYER_TEAM:
		{
			int playerIndex = GetPlayerIndexFromUserID( event->GetInt( "userid" ) );

			if ( playerIndex == INVALID_INDEX )
			{
				return;
			}

			SFMapOverviewIconPackage* pPackage = GetRadarPlayer( playerIndex );
			pPackage->SetPlayerTeam( event->GetInt( "team" ) );

		}
		break;

		case PLAYER_DEATH:
		{
			int playerIndex = GetPlayerIndexFromUserID( event->GetInt( "userid" ) );

			if ( playerIndex == INVALID_INDEX )
			{
				return;
			}

			SFMapOverviewIconPackage* pPackage = GetRadarPlayer( playerIndex );

			if ( pPackage->m_bIsActive )
			{
				pPackage->SetIsDead( true );
				pPackage->m_Health = 0;
			}

			int nDefuseIndex = GetDefuseIndexFromEntityID( engine->GetPlayerForUserID( event->GetInt( "userid" ) ) );

			if ( nDefuseIndex != INVALID_INDEX )
			{
				RemoveDefuser( nDefuseIndex );
			}
		}
		break;

		case PLAYER_SPAWN:
		{
			if ( EventUserID == LocalPlayerID )
			{
				m_bShowMapOverview = false;
			}

			int playerIndex = GetPlayerIndexFromUserID( event->GetInt( "userid" ) );

			if ( playerIndex != INVALID_INDEX )
			{
				SFMapOverviewIconPackage* pPackage = GetRadarPlayer( playerIndex );
				pPackage->m_Health = 100;

				UpdatePlayerNameAndNumber( pPackage );
			}
		}
		break;

		case PLAYER_DISCONNECT:
		{

			int playerIndex = GetPlayerIndexFromUserID( event->GetInt( "userid" ) );

			if ( playerIndex != INVALID_INDEX )
			{
				RemovePlayer( playerIndex );
			}
		}
		break;

		case HOSTAGE_KILLED:
		{
			int hostageIndex = GetHostageIndexFromHostageEntityID( event->GetInt( "hostage" )  );

			if ( hostageIndex == INVALID_INDEX )
			{
				return;
			}

			SFMapOverviewIconPackage* pPackage = GetRadarHostage( hostageIndex );

			if ( pPackage->m_bIsActive )
			{
				pPackage->SetIsDead( true );
			}

		}
		break;

		case HOSTAGE_RESCUED:
		{
			int hostageIndex = GetHostageIndexFromHostageEntityID( event->GetInt( "hostage" )  );

			if ( hostageIndex == INVALID_INDEX )
			{
				return;
			}

			SFMapOverviewIconPackage* pPackage = GetRadarHostage( hostageIndex );

			if ( pPackage->m_bIsActive )
			{
				pPackage->SetIsRescued( true );
			}

		}
		break;

		case BOMB_DEFUSED:
		{
			m_bBombDefused = true;
		}
		break;

		case BOMB_EXPLODED:
		{
			m_bBombExploded = true;
		}
		break;

		case BOMB_PLANTED:
		{
			m_nBombEntIndex = -1;
			m_bBombPlanted = true;
			m_BombPosition.z = 0;
		}
		break;

		case BOMB_PICKUP:
		{
			m_bBombDropped = false;
			m_nBombEntIndex = -1;
		}
		break;

		case BOMB_DROPPED:
		{
			m_nBombEntIndex = event->GetInt( "entindex", -1 );
			m_bBombDropped = true;
		}
		break;

		case DEFUSER_PICKUP:
		{
			int nDefuseIndex = GetDefuseIndexFromEntityID( event->GetInt( "entityid" ) );

			if ( nDefuseIndex != INVALID_INDEX )
			{
				RemoveDefuser( nDefuseIndex );
			}
		}
		break;

		case DEFUSER_DROPPED:
		{
			int nEntityID = event->GetInt( "entityid" );

			CreateDefuser( nEntityID );
		}
		break;

		case DECOY_STARTED:
		{
			if ( pLocalPlayer )
			{
				int userID = event->GetInt( "userid" );
				CBasePlayer* player =  UTIL_PlayerByUserId( userID );

				if ( player )
				{
					bool bShowGrenade = false;

					// if it was placed by a player on our team or if we are a spectator, show the grenade as a grenade icon
					// otherwise, we're going to show it as if it were a fake player
					if ( player->GetAssociatedTeamNumber() == pLocalPlayer->GetAssociatedTeamNumber() || m_bShowAll )
						bShowGrenade = true;
				
					int entityID = event->GetInt( "entityid" );
					int teamNumber = player->GetAssociatedTeamNumber();

					SFMapOverviewIconPackage* pPackage = CreateGrenade( entityID, PI_GRENADE_DECOY );

					if ( pPackage )
					{
						if ( bShowGrenade )
							pPackage->SetGrenadeType( PI_GRENADE_DECOY );
						else
						{
							pPackage->SetPlayerTeam( teamNumber );
							pPackage->SetIsSpotted( true );
						}

						pPackage->m_Position.Init( ( float )event->GetInt( "x" ), ( float )event->GetInt( "y" ), 0.0f );
						pPackage->m_Angle.Init( 0.0f, RandomFloat( 0.0f, 360.0f ), 0.0f );
						SetIconPackagePosition( pPackage );
					}
				}
			}

		}
		break;

		case DECOY_DETONATE:
		{
			int entityID = event->GetInt( "entityid" );
			int i = GetGrenadeIndexFromEntityID( entityID );
			if ( i != INVALID_INDEX )
			{
				SFMapOverviewIconPackage* pPackage = GetRadarGrenade( i );
				pPackage->SetGrenadeExpireTime( gpGlobals->curtime );
				SetIconPackagePosition( pPackage );
			}

			STEAMWORKS_TESTSECRET_AMORTIZE( 23 );
		}
		break;

		case HE_DETONATE:
			{
				int entityID = event->GetInt( "entityid" );
				int userID = event->GetInt( "userid" );
				CBasePlayer* player =  UTIL_PlayerByUserId( userID );
				if ( pLocalPlayer && player )
				{
					// if it was placed by a player on our team or if we are a spectator, show the grenade as usually
					// otherwise, we don't show it at all
					Vector vecGrenade = Vector( ( float )event->GetInt( "x" ), ( float )event->GetInt( "y" ), 0 );
					if ( (player->GetAssociatedTeamNumber() == pLocalPlayer->GetAssociatedTeamNumber() && IsEnemyCloseEnoughToShow( vecGrenade )) || m_bShowAll )
					{
						SFMapOverviewIconPackage* pPackage = CreateGrenade( entityID, PI_GRENADE_HE );
						if ( pPackage )
						{
							pPackage->SetGrenadeType( PI_GRENADE_HE );
							pPackage->SetGrenadeExpireTime( gpGlobals->curtime );		
							pPackage->m_Position.Init( vecGrenade.x, vecGrenade.y, 0.0f );
							pPackage->m_Angle.Init( 0.0f, RandomFloat( 0.0f, 360.0f ), 0.0f );
							SetIconPackagePosition( pPackage );
						}
					}
				}

				STEAMWORKS_TESTSECRET_AMORTIZE( 19 );
			}
			break;

		case FLASH_DETONATE:
			{
				int entityID = event->GetInt( "entityid" );
				int userID = event->GetInt( "userid" );
				CBasePlayer* player =  UTIL_PlayerByUserId( userID );
				if ( pLocalPlayer && player )
				{
					// if it was placed by a player on our team or if we are a spectator, show the grenade as usually
					// otherwise, we don't show it at all
					Vector vecGrenade = Vector( ( float )event->GetInt( "x" ), ( float )event->GetInt( "y" ), 0 );
					if ( (player->GetAssociatedTeamNumber() == pLocalPlayer->GetAssociatedTeamNumber() && IsEnemyCloseEnoughToShow( vecGrenade )) || m_bShowAll )
					{
						SFMapOverviewIconPackage* pPackage = CreateGrenade( entityID, PI_GRENADE_FLASH );
						if ( pPackage )
						{
							pPackage->SetGrenadeType( PI_GRENADE_FLASH );
							pPackage->SetGrenadeExpireTime( gpGlobals->curtime );		
							pPackage->m_Position.Init( vecGrenade.x, vecGrenade.y, 0.0f );
							pPackage->m_Angle.Init( 0.0f, RandomFloat( 0.0f, 360.0f ), 0.0f );
							SetIconPackagePosition( pPackage );
						}
					}
				}

				STEAMWORKS_TESTSECRET_AMORTIZE( 17 );
			}
			break;

		case SMOKE_DETONATE:
			{
				int entityID = event->GetInt( "entityid" );
				int userID = event->GetInt( "userid" );
				CBasePlayer* player =  UTIL_PlayerByUserId( userID );
				if ( pLocalPlayer && player )
				{
					// if it was placed by a player on our team or if we are a spectator, show the grenade as usually
					// otherwise, we don't show it at all
					Vector vecGrenade = Vector( ( float )event->GetInt( "x" ), ( float )event->GetInt( "y" ), 0 );
					if ( (player->GetAssociatedTeamNumber() == pLocalPlayer->GetAssociatedTeamNumber() && IsEnemyCloseEnoughToShow( vecGrenade )) || m_bShowAll )
					{
						SFMapOverviewIconPackage* pPackage = CreateGrenade( entityID, PI_GRENADE_SMOKE );

						if ( pPackage )
						{
							pPackage->SetGrenadeType( PI_GRENADE_SMOKE );
							pPackage->m_Position.Init( vecGrenade.x, vecGrenade.y, 0.0f );
							pPackage->m_Angle.Init( 0.0f, RandomFloat( 0.0f, 360.0f ), 0.0f );
							SetIconPackagePosition( pPackage );
						}
					}
				}
			}
			break;
			
		case SMOKE_EXPIRE:
			{
				int entityID = event->GetInt( "entityid" );
				int i = GetGrenadeIndexFromEntityID( entityID );
				if ( i != INVALID_INDEX )
				{
					SFMapOverviewIconPackage* pPackage = GetRadarGrenade( i );
					pPackage->SetGrenadeExpireTime( gpGlobals->curtime );
					SetIconPackagePosition( pPackage );
				}

				STEAMWORKS_TESTSECRET_AMORTIZE( 11 );
			}
			break;

		case MOLOTOV_DETONATE:
			{
				int entityID = event->GetInt( "entityid" );
				int userID = event->GetInt( "userid" );
				CBasePlayer* player =  UTIL_PlayerByUserId( userID );
				if ( pLocalPlayer && player )
				{
					// if it was placed by a player on our team or if we are a spectator, show the grenade as usually
					// otherwise, we don't show it at all
					Vector vecGrenade = Vector( ( float )event->GetInt( "x" ), ( float )event->GetInt( "y" ), 0 );
					if ( (player->GetAssociatedTeamNumber() == pLocalPlayer->GetAssociatedTeamNumber() && IsEnemyCloseEnoughToShow( vecGrenade )) || m_bShowAll )
					{
						SFMapOverviewIconPackage* pPackage = CreateGrenade( entityID, PI_GRENADE_MOLOTOV );

						if ( pPackage )
						{
							pPackage->SetGrenadeType( PI_GRENADE_MOLOTOV );
							pPackage->m_Position.Init( vecGrenade.x, vecGrenade.y, 0.0f );
							pPackage->m_Angle.Init( 0.0f, RandomFloat( 0.0f, 360.0f ), 0.0f );
							SetIconPackagePosition( pPackage );
						}
					}
				}
			}
			break;

		case MOLOTOV_EXPIRE:
			{
				int entityID = event->GetInt( "entityid" );
				int i = GetGrenadeIndexFromEntityID( entityID );
				if ( i != INVALID_INDEX )
				{
					SFMapOverviewIconPackage* pPackage = GetRadarGrenade( i );
					pPackage->SetGrenadeExpireTime( gpGlobals->curtime );
					SetIconPackagePosition( pPackage );
				}

				STEAMWORKS_TESTSECRET_AMORTIZE( 19 );
			}
			break;

		case BOT_TAKEOVER:
		{
			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
			if ( pLocalPlayer )
			{
				int userID = event->GetInt( "userid" );
				int localID = pLocalPlayer->GetUserID();

				if (localID == userID )
				{
					ResetRadar( false );
				}
			}

		}
		break;

		default:
			break;
	}
}
// 
// 
// /*****************************************************
//  * these set lazy update the icons and text based
//  * on the values in the state variables
//  */
// 

void SFMapOverview::SetupIconsFromStates( void )
{
	uint64 newFlags = 0;	
	
	if ( CSGameRules()->IsBombDefuseMap() )
	{
		// check to see if the bomb exists before assuming the first exists and trying to get data from it
		if ( !m_bBombDefused && m_bBombPlanted && g_PlantedC4s.Count() > 0 )
		{
			float fPercent = ( g_PlantedC4s[0]->GetDetonationProgress() * 100.0f );

			if ( fPercent > 60.0f )
			{
				newFlags |= 1 << RI_BOMB_IS_PLANTED;
			}
			else if ( fPercent > 30.0f )
			{
				newFlags |= 1 << RI_BOMB_IS_PLANTED_MEDIUM;
			}
			else if ( fPercent > 0.0f )
			{
				newFlags |= 1 << RI_BOMB_IS_PLANTED_FAST;
			}			
		}

		if ( m_fBombAlpha > 0.0f )
		{
			newFlags |= 1 << RI_BOMB_ICON_PACKAGE;
			if ( m_bShowBombHighlight )
			{
				if ( m_bBombPlanted )
				{
					newFlags |= 1 << RI_BOMB_ICON_PLANTED;
				}
				else
				{
					newFlags |= 1 << RI_BOMB_ICON_DROPPED;
				}

				C_CSPlayer *pLocalOrObserver = GetLocalOrInEyeCSPlayer();
				if ( pLocalOrObserver )
				{
					if ( m_BombPosition.z > pLocalOrObserver->GetAbsOrigin().z + 180 )
						newFlags |= 1 << RI_BOMB_ICON_BOMB_ABOVE;
					else if ( m_BombPosition.z < pLocalOrObserver->GetAbsOrigin().z - 180 )
						newFlags |= 1 << RI_BOMB_ICON_BOMB_BELOW;
				}
			}
	
			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
			if( pLocalPlayer != NULL )
			{
				if ( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST )
				{
					newFlags |= 1 << RI_BOMB_ICON_BOMB_T;
				}
				else
				{
					newFlags |= 1 << RI_BOMB_ICON_BOMB_CT;
				}
			}

			Vector mapPosition;

			WorldToRadar( m_BombPosition, mapPosition );

			Vector newMapPosition = mapPosition;
			newMapPosition -= m_RadarViewpointMap;

			ScaleformDisplayInfo displayInfo;
			displayInfo.SetAlpha( m_fBombAlpha * 100.0f );
			int nBombCarryOffset = 0;
			if ( ((newFlags & (1 << RI_BOMB_ICON_BOMB_T)) || (newFlags & (1 << RI_BOMB_ICON_BOMB_CT))) && !(newFlags & (1 << RI_BOMB_ICON_PLANTED)) )
			{
				nBombCarryOffset = 6;
			}

			displayInfo.SetX( mapPosition.x );
			displayInfo.SetY( mapPosition.y + nBombCarryOffset );
			displayInfo.SetRotation( -m_RadarRotation );
			
			assert( m_AllIcons[RI_BOMB_ICON_PACKAGE] );
			m_pScaleformUI->Value_SetDisplayInfo( m_AllIcons[RI_BOMB_ICON_PACKAGE], &displayInfo );
		}
	}
	else if ( CSGameRules()->IsHostageRescueMap() )
	{
		if ( m_bShowingHostageZone )
		{
			newFlags |= 1 << RI_IN_HOSTAGE_ZONE;
		}
	}

	if ( m_bShowingDashboard )
		newFlags |= 1 << RI_DASHBOARD;

	SetVisibilityFlags( newFlags );	
}

void SFMapOverview::SetVisibilityFlags( uint64 newFlags )
{
	newFlags &= ( ( 1 << RI_NUM_ICONS )-1 );

	uint64 diffFlags = m_iCurrentVisibilityFlags ^ newFlags;

	if ( diffFlags )
	{
		for ( int i = 0; i < RI_NUM_ICONS && ( diffFlags != 0 ); i++, diffFlags >>= 1 )
		{
			if ( diffFlags & 1 )
			{
				if ( m_AllIcons[i] )
					m_pScaleformUI->Value_SetVisible( m_AllIcons[i], ( newFlags & ( 1ll << i ) ) != 0 );
			}
		}

		m_iCurrentVisibilityFlags = newFlags;
	}
}

void SFMapOverview::SetLocationText( wchar_t *newText )
{
	if ( newText == NULL )
	{
		newText = L"";
	}

	if ( V_wcscmp( newText, m_wcLocationString ) )
	{
		V_wcsncpy( m_wcLocationString, newText, sizeof( wchar_t )*MAX_LOCATION_TEXT_LENGTH );

		if ( m_LocationText )
		{
			WITH_SLOT_LOCKED
				m_LocationText->SetText( m_wcLocationString );
		}
	}
}

// 
// /********************************************
//  * "drawing" code.  Actually just sets position / alpha
//  * of the flash objects
//  */
// 
void SFMapOverview::WorldToRadar( const Vector& ptin, Vector& ptout )
{
	ptout.x = ( ptin.x - m_MapOrigin.x ) * m_fWorldToRadarScale;
	ptout.y = ( m_MapOrigin.y - ptin.y ) * m_fWorldToRadarScale;
	ptout.z = 0;
}

void SFMapOverview::RadarToWorld( const Vector& ptin, Vector& ptout )
{
	ptout.x = ( ptin.x + m_MapOrigin.x ) / m_fWorldToRadarScale;
	ptout.y = ( m_MapOrigin.y + ptin.y ) / m_fWorldToRadarScale;
	ptout.z = 0;
}


void SFMapOverview::PositionRadarViewpoint( void )
{
	if ( !m_bFlashReady )
	{
		return;
	}

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	CBasePlayer * pPlayer = CBasePlayer::GetLocalPlayer();

	if( pPlayer == NULL )
	{
		pPlayer = GetSplitScreenViewPlayer( nSlot );
	}

	// [jbright] Using GetLocalOrigin addresses an issue which
	// causes the center player indicator to not stay centered
	// on the radar. This solution causes the radar to go jump
	// around while in observer mode, so in those cases rely on
	// MainViewOrigin.
// 	if ( pPlayer && m_iObserverMode == OBS_MODE_NONE )
// 	{
// 		m_RadarViewpointWorld = pPlayer->GetLocalOrigin();
// 	}
// 	else
// 	{
// 		m_RadarViewpointWorld = MainViewOrigin( nSlot );
// 	}

	// Get the world's extent
// 	C_World *world = GetClientWorldEntity();
// 	if ( !world )
// 		return;
// 
// 	Vector worldCenter = ( world->m_WorldMins + world->m_WorldMaxs ) * 0.5f;

	m_RadarViewpointWorld = Vector( m_MapOrigin.x, m_MapOrigin.y, 0 );//worldCenter;
	//m_RadarViewpointWorld.z = 0;

	WorldToRadar( m_RadarViewpointWorld, m_RadarViewpointMap );

	m_RadarRotation = 0;

	ScaleformDisplayInfo displayInfo;

	if ( m_MapRotation )
	{
		displayInfo.SetRotation( m_RadarRotation );
		m_pScaleformUI->Value_SetDisplayInfo( m_MapRotation, &displayInfo );

		if ( m_MapTranslation )
		{
			displayInfo.Clear();
			displayInfo.SetX( -(m_fMapSize/2) );
			displayInfo.SetY( -(m_fMapSize/2) );

			m_pScaleformUI->Value_SetDisplayInfo( m_MapTranslation, &displayInfo );
		}
	}

	if ( m_IconRotation )
	{
		displayInfo.Clear();
		displayInfo.SetRotation( m_RadarRotation );
		m_pScaleformUI->Value_SetDisplayInfo( m_IconRotation, &displayInfo );

		if ( m_IconTranslation )
		{
			displayInfo.Clear();
			displayInfo.SetX( -(m_fMapSize/2) - 1 );
			displayInfo.SetY( -(m_fMapSize/2) - 1 );

			m_pScaleformUI->Value_SetDisplayInfo( m_IconTranslation, &displayInfo );
		}
	}

}

void SFMapOverview::PlaceGoalIcons( void )
{
	C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* )GameResources();
	if ( !pCSPR )
	{
		return;
	}

	SFMapOverviewGoalIcon* pwalk = m_GoalIcons;
	Vector mapPosition;
	ScaleformDisplayInfo displayInfo;

	for ( int i = 0; i < m_iNumGoalIcons; i++ )
	{
		WorldToRadar( pwalk->m_Position, mapPosition );

		if ( pwalk->m_Icon )
		{
			Vector newMapPosition = mapPosition;
			newMapPosition -= m_RadarViewpointMap;

			displayInfo.SetX( mapPosition.x );
			displayInfo.SetY( mapPosition.y );
			displayInfo.SetRotation( -m_RadarRotation );

			m_pScaleformUI->Value_SetDisplayInfo( pwalk->m_Icon, &displayInfo );
		}
		pwalk++;
	}

}

void SFMapOverview::SetIconPackagePosition( SFMapOverviewIconPackage* pPackage )
{
	if ( !m_bFlashReady )
		return;

	Vector mapPosition;
	ScaleformDisplayInfo displayInfo;
	float mapAngle;

	WorldToRadar( pPackage->m_Position, mapPosition );

	Vector newMapPosition = mapPosition;
	newMapPosition -= m_RadarViewpointMap;
	pPackage->SetIsOffMap( false );
	if ( pPackage->m_bIsDead || !pPackage->m_bIsSpotted )
	{
		mapAngle = -m_RadarRotation;
	}
	else
	{
		mapAngle = -pPackage->m_Angle[YAW]+90;
	}

	displayInfo.SetX( mapPosition.x );
	displayInfo.SetY( mapPosition.y );
	float flScale = mapoverview_icon_scale.GetFloat();
	displayInfo.SetXScale( 120 * flScale );
	displayInfo.SetYScale( 120 * flScale );
	m_pScaleformUI->Value_SetDisplayInfo( pPackage->m_IconPackage, &displayInfo );

	// only rotated the sublayer, the stuff on top doesn't want to be rotated
	//SFVALUE rotatedIcon;
	//rotatedIcon = m_pScaleformUI->Value_GetMember( pPackage->m_IconPackage, "RotatedLayer" );
	if ( pPackage->m_IconPackageRotate )
	{
		ScaleformDisplayInfo rotatedDisplayInfo;
		rotatedDisplayInfo.SetRotation( mapAngle );
		m_pScaleformUI->Value_SetDisplayInfo( pPackage->m_IconPackageRotate, &rotatedDisplayInfo );
	}

	pPackage->SetupIconsFromStates();
}

void SFMapOverview::PlaceHostages( void )
{
	if ( !m_bFlashReady )
	{
		return;
	}

	if ( !CSGameRules()->IsHostageRescueMap() )
	{
		return;
	}


	C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* )GameResources();
	if ( !pCSPR )
	{
		return;
	}

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if( pLocalPlayer == NULL )
	{
		return;
	}

	int iNumLiveHostages = 0;
	int iNumDeadHostages = 0;
	int iNumMovingHostages = 0;
	int iNumRescuedHostages = 0;

	bool bIsTerrorist = ( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST );

	if ( bIsTerrorist )
	{
		// Taking over a bot after switching to the terrorist team results in
		// the hostage indicators remaining visible, so we need to remove them.
		for ( int i = 0; i <= m_iLastHostageIndex; i++ )
		{
			if ( GetRadarHostage( i )->m_bIsActive )
				RemoveHostage( i );
		}

		// we also want to show the number of hostages that are still alive
		// and unrescued
		for( int i=0; i < MAX_HOSTAGES; i++ )
		{
			if( pCSPR->IsHostageAlive( i ) )
				iNumLiveHostages++;
		}

	}
	else
	{
	
		// T's can now always see basic hostage info (alive, dead) but not moving or rescued status
		/*
		bool bCanShowHostages = m_bShowAll || ( pLocalPlayer->GetTeamNumber() == TEAM_CT );

		if ( !bCanShowHostages )
		{
			return;
		}
		*/

		// Update hostage status
		for( int i=0; i < MAX_HOSTAGES; i++ )
		{
			int nEntityID = pCSPR->GetHostageEntityID( i );

			if ( nEntityID > 0 )
			{
				SFMapOverviewIconPackage* pHostage =  NULL;

				int nHostageIndex = GetHostageIndexFromHostageEntityID( nEntityID );

				
				if ( nHostageIndex == INVALID_INDEX )
				{
					// Create the hostage in next available slot
					for ( nHostageIndex = 0; nHostageIndex < MAX_HOSTAGES; nHostageIndex++ )
					{
						if ( !m_Hostages[nHostageIndex].m_bIsActive )
						{
							pHostage = CreateHostage( nHostageIndex );
							pHostage->SetPlayerTeam( TEAM_UNASSIGNED );
							pHostage->m_iEntityID =  nEntityID;
							break;
						}
					}
				}
				else
				{
					pHostage = GetRadarHostage( nHostageIndex );
				}


				if ( pHostage )
				{
					if( pCSPR->IsHostageAlive( i ) )
					{
						if ( !pHostage->m_bIsActive )
						{
							
						}

						pHostage->SetIsDead( false );

						if ( pCSPR->IsHostageFollowingSomeone( i ) )
						{
							iNumMovingHostages++;
							pHostage->SetIsMovingHostage( true );
						}
						else
						{
							iNumLiveHostages++;
							pHostage->SetIsMovingHostage( false );
						}
					}
					else if ( i <= m_iLastHostageIndex && pHostage->m_bIsActive )
					{
						if ( pHostage->m_bIsRescued )
						{
							iNumRescuedHostages++;
						}
						else
						{
							iNumDeadHostages++;
						}

						pHostage->SetIsDead( true );
						pHostage->SetIsMovingHostage( false );
					}
				}
			}
		}

		// Update hostage positions
		for( int i = 0; i < MAX_HOSTAGES; i++ )
		{
			SFMapOverviewIconPackage* pHostage = GetRadarHostage( i );

			if ( pHostage->m_bIsActive )
			{
				C_BaseEntity * pEntity = ClientEntityList().GetBaseEntity( pHostage->m_iEntityID );

				if ( pEntity && !pEntity->IsDormant() )
				{
					Assert( dynamic_cast<CHostage*>( pEntity ) );

					pHostage->m_Position = pEntity->GetNetworkOrigin();
				}

				pHostage->SetIsSpotted( true );
				SetIconPackagePosition( pHostage );
			}
		}
	}

	int hostageIndex = 0;

	for ( int i = 0; i < iNumDeadHostages; i++ )
	{
		m_HostageStatusIcons[hostageIndex++].SetStatus( SFMapOverviewHostageIcons::HI_DEAD );
	}

	for ( int i = 0; i < iNumRescuedHostages; i++ )
	{
		m_HostageStatusIcons[hostageIndex++].SetStatus( SFMapOverviewHostageIcons::HI_RESCUED );
	}

	for ( int i = 0; i < iNumMovingHostages; i++ )
	{
		m_HostageStatusIcons[hostageIndex++].SetStatus( SFMapOverviewHostageIcons::HI_TRANSIT );
	}

	for ( int i = 0; i < iNumLiveHostages; i++ )
	{
		m_HostageStatusIcons[hostageIndex++].SetStatus( SFMapOverviewHostageIcons::HI_ALIVE );
	}

	for ( ; hostageIndex < MAX_HOSTAGES && m_HostageStatusIcons[hostageIndex].m_IconPackage != NULL; hostageIndex++ )
	{
		m_HostageStatusIcons[hostageIndex].SetStatus( SFMapOverviewHostageIcons::HI_UNUSED );
	}
}

void SFMapOverview::UpdateAllDefusers( void )
{
	if ( !m_bTrackDefusers )
	{
		return;
	}

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if( pLocalPlayer == NULL )
	{
		return;
	}

	for ( int i = 0; i <= m_iLastDefuserIndex; i++ )
	{
		SFMapOverviewIconPackage * pPackage = GetRadarDefuser( i );

		if ( pPackage && pPackage->m_bIsActive )
		{
			C_BaseEntity * pEntity = ClientEntityList().GetBaseEntity( pPackage->m_iEntityID );

			bool bSpotted = ( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_CT || ( pEntity && pEntity->IsSpotted() ) || m_EntitySpotted.Get( pPackage->m_iEntityID ) );

			pPackage->SetIsSpotted( bSpotted );

			if ( bSpotted && pEntity && !pEntity->IsDormant() )
			{
				SetDefuserPos( pPackage->m_iEntityID,
							   pEntity->GetNetworkOrigin().x,
							   pEntity->GetNetworkOrigin().y,
							   pEntity->GetNetworkOrigin().z,
							   pEntity->GetNetworkAngles().y );
			}
		}

		// update radar position and visibility
		if ( pPackage && pPackage->m_bIsActive )
		{
			SetIconPackagePosition( pPackage );
		}
	}
}





void SFMapOverview::PlacePlayers( void )
{
	if ( !m_bFlashReady )
	{
		return;
	}

	C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* )GameResources();
	if ( !pCSPR )
	{
		return;
	}

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if( pLocalPlayer == NULL )
	{
		return;
	}

	int localPlayerIndex = pLocalPlayer->entindex()-1;

	if ( localPlayerIndex == INVALID_INDEX )
	{
		return;
	}

	int localTeamNumber = pLocalPlayer->GetAssociatedTeamNumber();

	SFMapOverviewIconPackage* pwalk = m_Players;
	int playerIndex;

	for ( int i = 0; i <= m_iLastPlayerIndex; i++, pwalk++ )
	{
		if ( pwalk->m_IconPackage && pwalk->m_bIsActive )
		{
			playerIndex = i+1;

			C_CSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( playerIndex ) );

			bool playerIsActive = pPlayer != NULL && !pPlayer->IsDormant();
			int nIsAboveOrBelow = R_SAMELEVEL;

			// we do all the pCSPR stuff because the player may not have been given
			// to us by the server yet.  If that's the case, we can't use the player
			// pointer to find anything out, we have to use the PCSPR which keeps some
			// of this data ( like team numbers, spotted, hasC4 etc.  The dormant means
			// that it's not in our view

			if ( pCSPR->IsConnected( playerIndex ) )
			{
				pwalk->m_Health = pCSPR->GetHealth( playerIndex );

				if ( !pCSPR->IsAlive( playerIndex ) )
				{
					pwalk->m_Health = 0;
					if( pCSPR->GetControlledByPlayer( playerIndex ) )
					{
						pwalk->SetIsControlledBot();
					}
					else
					{
						pwalk->SetIsDead( true );
					}
				}
				else
				{
					pwalk->SetIsDead( false );

					//it's hard to figure out if this player has been spotted, and if spotted has enough info to be
					//shown.

					bool bSameTeam = ( localTeamNumber == pCSPR->GetTeam( playerIndex ) );
					bool bObservingTarget = pLocalPlayer->GetObserverTarget() && (pLocalPlayer->GetObserverTarget() == pPlayer);

					bool bHasValidPosition = playerIsActive || ( pwalk->m_Position.x != 0 || pwalk->m_Position.y != 0 );

					Vector vecEnemy = pwalk->m_Position;
					bool bPlayerSpotted = bObservingTarget || (( m_EntitySpotted.Get( playerIndex ) || ( playerIsActive && pPlayer->IsSpotted() ) ) 
						&& (m_bShowAll || (!bSameTeam && IsEnemyCloseEnoughToShow( vecEnemy ))));

					if ( pPlayer && bHasValidPosition && ( bSameTeam  || m_bShowAll || bPlayerSpotted ) )
					{
						pwalk->SetIsSpotted( true );

						if ( playerIsActive && pPlayer->HasDefuser() )
						{
							// attach a defuser icon to this player if one does not already exist
							if ( GetDefuseIndexFromEntityID( playerIndex ) == INVALID_INDEX ) 
							{
								CreateDefuser( playerIndex );
							}
						}

						if ( playerIsActive && pPlayer->HasC4() && ( bPlayerSpotted || m_bShowAll ) )
						{
							m_bBombIsSpotted = true;
							m_BombPosition = pwalk->m_Position;
						}

						// set is low health
						pwalk->SetIsLowHealth( (playerIsActive && pPlayer->GetHealth() < 40) );

						// set is selected
						C_CSPlayer *pLocalOrObserver = ToCSPlayer( pLocalPlayer->GetObserverTarget() );
						pwalk->SetIsSelected( pLocalOrObserver && pPlayer == pLocalOrObserver );
						
						// set is flashed
						pwalk->SetIsFlashed( pPlayer->IsBlinded() );	

						// set is firing
						pwalk->SetIsFiring( (pPlayer->GetLastFiredWeaponTime() + 0.2) >= gpGlobals->curtime );					
					}
					else
					{
						pwalk->SetIsSpotted( false );
					}
				}

				pwalk->SetPlayerTeam( pCSPR->GetTeam( playerIndex ) );
				pwalk->SetIsOnLocalTeam( localTeamNumber == pCSPR->GetTeam( playerIndex ) );

				pwalk->SetIsPlayer( i == localPlayerIndex );
				pwalk->SetIsSpeaking( GetClientVoiceMgr()->IsPlayerSpeaking( playerIndex ) && GetClientVoiceMgr()->IsPlayerAudible( playerIndex ) );

				UpdatePlayerNameAndNumber( pwalk );
			}
			else
			{
				pwalk->SetIsSpotted( false );
				pwalk->SetIsSpeaking( false );
			}

			// when a player dies the system reports the player's position as the position of
			// the spectator camera.  If we let that happen, the dead player's X migrates from
			// the point they died to follow another player's position.
			//
			// so, never update the icon position for a dead player
			if ( playerIsActive && pwalk->m_bIsSpotted && !pwalk->m_bIsDead )
			{
				// dkorus and pfreese:  Changed from local origin and local angles to last networked version to address jitter issues for pax demo 2011
				pwalk->m_Position = pPlayer->GetNetworkOrigin();

				// only set the angle of the icon if the player isn't above or below us
				if ( nIsAboveOrBelow == R_SAMELEVEL )
					pwalk->m_Angle = pPlayer->GetLocalAngles();
				else
				{
					int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
					pwalk->m_Angle = MainViewAngles( nSlot );
				}

				if ( pPlayer->HasC4() )
				{
					m_BombPosition = pwalk->m_Position;
				}
			}

			SetIconPackagePosition( pwalk );
		}
	}
}

bool SFMapOverview::IsEnemyCloseEnoughToShow( Vector vecEnemyPos )
{
	//double check if spotted based on distance
	// we're going to check the distance of this guy from all of the people on our team
	// if they are outside the radius (more or less) of what the radar would allow us to see, consider them not spotted
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if( !pLocalPlayer )
		return false;

	C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* )GameResources();
	if ( !pCSPR )
		return false;

	int localTeamNumber = pLocalPlayer->GetAssociatedTeamNumber();

	bool bTooFar = true;
	SFMapOverviewIconPackage* pTeammate = m_Players;
	for ( int j = 0; j <= (m_iLastPlayerIndex); j++, pTeammate++ )
	{
		if ( pCSPR->IsConnected( (j+1) ) && localTeamNumber == pCSPR->GetTeam( (j+1) ) )
		{
			Vector toEnemy = pTeammate->m_Position - vecEnemyPos;
			float dist = toEnemy.LengthSqr();
			if ( dist <= NOTSPOTTEDRADIUS*NOTSPOTTEDRADIUS )
			{
				bTooFar = false;;
			}
		}
	}

	return !bTooFar;
}

void SFMapOverview::UpdateMiscIcons( void )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
	{
		return;
	}

	// We show the entire dashboard in spectator mode now, so you can see bomb/hostage status, etc.
	m_bShowingDashboard = true;

	// Uncomment this line to disable the radar dashboard in spectator mode:
	// m_bShowingDashboard = ( pLocalPlayer->GetObserverMode() == OBS_MODE_NONE );

	const char *pszLocation = pLocalPlayer->GetLastKnownPlaceName();

	if ( pszLocation )
	{
		SetLocationText( g_pVGuiLocalize->Find( pszLocation ) );
	}
	else
	{
		SetLocationText( NULL );
	}


	C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* )GameResources();
	if ( !pCSPR )
	{
		return;
	}

	m_fBombAlpha = 0.0f;
	m_fDefuserAlpha = 0.0f;

	if ( CSGameRules()->IsBombDefuseMap() )
	{
		if ( m_bBombPlanted )
		{
			FOR_EACH_VEC( g_PlantedC4s, iBomb )
			{
				C_PlantedC4 * pC4 = g_PlantedC4s[iBomb];
				if ( pC4 && pC4->IsBombActive() )
				{
					bool bLocalPlayerCanSeeBomb =( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST || m_bShowAll || ( pC4 && !pC4->IsDormant() && pC4->IsSpotted() ) && IsEnemyCloseEnoughToShow( pC4->GetNetworkOrigin() ) );
					if ( bLocalPlayerCanSeeBomb && pC4 && !pC4->IsDormant() )
					{
						m_bBombIsSpotted = true;
						m_BombPosition = pC4->GetNetworkOrigin();
					}
				}
			}
		}
		if ( m_nBombEntIndex != -1 && ( !m_bBombIsSpotted || m_bBombDropped ) )
		{
			C_BaseEntity * pEntity = ClientEntityList().GetBaseEntity( m_nBombEntIndex );

			bool bLocalPlayerCanSeeBomb = pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST || m_bShowAll || ( pEntity && !pEntity->IsDormant() && pEntity->IsSpotted() && IsEnemyCloseEnoughToShow( pEntity->GetNetworkOrigin() ) );
			if ( bLocalPlayerCanSeeBomb && pEntity && !pEntity->IsDormant() )
			{
				Assert( pEntity && FClassnameIs( pEntity, "weapon_c4" ) );
				m_bBombIsSpotted = true;
				m_BombPosition = pEntity->GetNetworkOrigin();
			}
		}

		float now = gpGlobals->curtime;
		bool drawBomb = false;

		bool bBombPositionIsValid = ( m_BombPosition.x != 0 || m_BombPosition.y != 0 );

		if ( bBombPositionIsValid && !m_bBombExploded && m_bBombIsSpotted )
		{
			drawBomb = true;
			m_fBombSeenTime = now;
			m_fBombAlpha = 1.0f;
		}
		else if ( m_bBombExploded )
		{
			m_fBombAlpha = 0;
			m_fBombSeenTime = TIMER_INIT;
		}
		else
		{
			m_fBombAlpha = clamp( 1.0f - ( now - m_fBombSeenTime )/BOMB_FADE_TIME, 0.0f, 1.0f );
			if ( m_fBombAlpha > 0 )
			{
				drawBomb = true;
			}
			else
			{
				m_fBombSeenTime = TIMER_INIT;
			}
		}

		if ( drawBomb )
		{
			m_bShowBombHighlight = !m_bBombDefused && ( m_bBombPlanted || m_bBombDropped );
		}
	}
	else if ( CSGameRules()->IsHostageRescueMap() )
	{
		m_bShowingHostageZone = pLocalPlayer->IsInHostageRescueZone();
	}

}

void SFMapOverview::ApplySpectatorModes( void )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	m_bShowAll = false;
	m_iObserverMode = OBS_MODE_NONE;

	if ( pLocalPlayer )
	{
		m_iObserverMode = pLocalPlayer->GetObserverMode();
		if ( pLocalPlayer->IsSpectator()	|| pLocalPlayer->IsHLTV() )
		{
			m_bShowAll = true;
			return;
		}

	}

// 	if ( m_iObserverMode != OBS_MODE_NONE )
// 	{
// 		// if we're observing in casual, then show everyone everywhere
// 		ConVarRef mp_forcecamera( "mp_forcecamera" );
// 		m_bShowAll = mp_forcecamera.GetInt() == OBS_ALLOW_ALL;
// 	}

}

void SFMapOverview::UpdateGrenades( void )
{
	for ( int i = 0; i <= m_iLastGrenadeIndex; i++ )
	{
		SFMapOverviewIconPackage* pPackage = GetRadarGrenade( i );

		if ( pPackage && pPackage->m_bIsActive )
		{
			SetIconPackagePosition( pPackage );
			if ( !pPackage->IsVisible() )
			{
				RemoveGrenade( i );
			}
		}
	}
}



void SFMapOverview::ProcessInput( void )
{
	LazyCreateGoalIcons();

	LazyCreatePlayerIcons();

	ApplySpectatorModes();
	
	PositionRadarViewpoint();

	PlaceGoalIcons();
	
	PlacePlayers();

	PlaceHostages();

	UpdateGrenades();

	UpdateAllDefusers();

	UpdateMiscIcons();

	SetupIconsFromStates();
}

static inline bool Helper_AllowMapDrawing()
{
	if ( mapoverview_allow_client_draw.GetBool() )
		return true;

	if ( sv_competitive_official_5v5.GetBool() )
		return true;

	if ( C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer() )
	{
		if ( pPlayer->IsHLTV() )
			return true;

		if ( pPlayer->IsSpectator() )
		{
			if ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() )
				return true;
		}
	}

	return false;
}

void SFMapOverview::AllowMapDrawing( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool bAllow = Helper_AllowMapDrawing();
	m_pScaleformUI->Params_SetResult( obj, bAllow );
}

void SFMapOverview::GetWorldDistance( SCALEFORM_CALLBACK_ARGS_DECL )
{
	int x1 = ( float )pui->Params_GetArgAsNumber( obj, 0 );
	int y1 = ( float )pui->Params_GetArgAsNumber( obj, 1 );
	int x2 = ( float )pui->Params_GetArgAsNumber( obj, 2 );
	int y2 = ( float )pui->Params_GetArgAsNumber( obj, 3 );

	Vector vecStart;
	Vector vecEnd;
	RadarToWorld( Vector( x1, y1, 0 ), vecStart );
	RadarToWorld( Vector( x2, y2, 0 ), vecEnd );

	float flDist = vecStart.AsVector2D().DistTo( vecEnd.AsVector2D() );

	m_pScaleformUI->Params_SetResult( obj, flDist );
}

/*
void SFMapOverview::GetNavPath( SCALEFORM_CALLBACK_ARGS_DECL )
{
	int x1 = ( float )pui->Params_GetArgAsNumber( obj, 0 );
	int y1 = ( float )pui->Params_GetArgAsNumber( obj, 1 );
	int x2 = ( float )pui->Params_GetArgAsNumber( obj, 2 );
	int y2 = ( float )pui->Params_GetArgAsNumber( obj, 3 );

	Vector vecStart;
	Vector vecEnd;
	RadarToWorld( Vector( x1, y1, 0 ), vecStart );
	RadarToWorld( Vector( x2, y2, 0 ), vecEnd );

	CNavArea *navStartArea = TheNavMesh->GetNearestNavArea( vecStart );
	CNavArea *navEndArea = TheNavMesh->GetNearestNavArea( vecEnd );

	CNavArea *goalArea = NULL;
	NavAreaBuildPath( tArea, ctArea, NULL, cost, &goalArea );

	ShortestPathCost cost = ShortestPathCost();
	float dist_left = NavAreaTravelDistance( TheNavMesh->GetNearestNavArea( ( *left )->GetAbsOrigin() ),
											 TheNavMesh->GetNearestNavArea( CSGameRules()->m_vecMainCTSpawnPos ),
											 cost );

	float dist_right = NavAreaTravelDistance( TheNavMesh->GetNearestNavArea( ( *right )->GetAbsOrigin() ),
											  TheNavMesh->GetNearestNavArea( CSGameRules()->m_vecMainCTSpawnPos ),
											  cost );

	//bool bAllow = Helper_AllowMapDrawing();
	//m_pScaleformUI->Params_SetResult( obj, bAllow );
}
*/
