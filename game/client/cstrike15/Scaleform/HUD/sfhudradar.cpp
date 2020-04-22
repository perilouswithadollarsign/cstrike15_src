//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#include "cbase.h"
#include "hudelement.h"
#include "sfhudradar.h"
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
#include "sfhudfreezepanel.h"
#include "sfhudwinpanel.h"
#include "c_plantedc4.h"
#include "HUD/sfhud_teamcounter.h"
#include "gametypes.h"
#include "eventlist.h"
#include <game/client/iviewport.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static const float RADARRADIUS = 58.0f;
static const float DEAD_FADE_TIME = 4.0f;
static const float GHOST_FADE_TIME = 6.0f;
static const float BOMB_FADE_TIME = 8.0f;
static const float DEFUSER_FADE_TIME = 8.0f;
static const float TIMER_INIT = -1000.0f;
static const int	ABOVE_BELOW_HEIGHT = 94;
static const int	INVALID_INDEX = -1;

extern ConVar cl_draw_only_deathnotices;
extern ConVar cl_radar_square_with_scoreboard;

/*******************************************************************************
 * data definitions
 */

static const char* playerIconNames[] = {
	"PlayerNumber",
	"PlayerLetter",
	"PlayerIndicator",
	"SpeakingOnMap",
	"SpeakingOffMap",
	"AbovePlayer",
	"BelowPlayer",
	"HostageTransitOnMap",
	"HostageTransitOffMap",
	"CTOnMap",
	"CTOffMap",
	"CTDeath",
	"CTGhost",
	"TOnMap",
	"TOffMap",
	"TDeath",
	"TGhost",
	"EnemyOnMap",
	"EnemyOffMap",
	"EnemyDeath",
	"EnemyGhost",
	"HostageOnMap",
	"HostageOffMap",
	"HostageDeath",
	"HostageGhost",
	"DirectionalIndicator",
	"Defuser",
	"Selected",
	"ViewFrustrum",
	"EnemyOnMapSeeOther",
	NULL
};

static const char* hostageIconNames[] = {
	"HostageDead",
	"HostageRescued",
	"HostageAlive",
	"HostageTransit",
	NULL
};

static const char* radarIconNames[] = {
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
	BOT_TAKEOVER,

};

/***************************************************
 * callback and function declarations
 */

static bool maplessfunc( const char* const & s1, const char* const & s2 )
{
	return V_strcmp( s1, s2 ) < 0;
}

CUtlMap<const char*, int> SFHudRadar::m_messageMap( maplessfunc );

DECLARE_HUDELEMENT( SFHudRadar );
DECLARE_HUD_MESSAGE( SFHudRadar, ProcessSpottedEntityUpdate );

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( MapLoaded ),
SFUI_END_GAME_API_DEF( SFHudRadar, Radar );

CON_COMMAND( drawradar, "Draws HUD radar" )
{
	( GET_HUDELEMENT( SFHudRadar ) )->ShowRadar( true );
}

CON_COMMAND( hideradar, "Hides HUD radar" )
{
	( GET_HUDELEMENT( SFHudRadar ) )->ShowRadar( false );
}

CON_COMMAND( cl_reload_hud, "Reloads the hud scale and resets scale and borders" )
{
	( GET_HUDELEMENT( SFHudRadar ) )->ResizeHud();
}

ConVar cl_radar_rotate( "cl_radar_rotate", "1", FCVAR_RELEASE | FCVAR_ARCHIVE, "1" );
ConVar cl_radar_scale( "cl_radar_scale", "0.7", FCVAR_RELEASE | FCVAR_ARCHIVE, "Sets the radar scale. Valid values are 0.25 to 1.0.", true, 0.25f, true, 1.0f );
ConVar cl_radar_always_centered( "cl_radar_always_centered", "1", FCVAR_RELEASE | FCVAR_ARCHIVE, "If set to 0, the radar is maximally used. Otherwise the player is always centered, even at map extents." );
ConVar cl_radar_icon_scale_min( "cl_radar_icon_scale_min", "0.6", FCVAR_RELEASE | FCVAR_ARCHIVE, "Sets the minimum icon scale. Valid values are 0.4 to 1.0.", true, 0.4f, true, 1.0f );
ConVar cl_radar_fast_transforms( "cl_radar_fast_transforms", "1", FCVAR_DEVELOPMENTONLY, "Faster way of placing icons on the mini map." );

/**********************************************************************
 * SFHudRadarIconPackage code
 */

SFHudRadar::SFHudRadarIconPackage::SFHudRadarIconPackage()
{
	ClearAll();
}

SFHudRadar::SFHudRadarIconPackage::~SFHudRadarIconPackage()
{
}

void SFHudRadar::SFHudRadarIconPackage::ClearAll( void )
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

	m_bIsSpotted = false;
	m_bIsSpottedByFriendsOnly = false;
	m_fGhostTime = TIMER_INIT;

	m_fLastColorUpdate = TIMER_INIT;

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

	m_HudPosition = vec3_origin;
	m_HudRotation = 0.0f;
	m_HudScale = 1.0f;

	m_bIsDefuser = false;

	m_bHostageIsUsed = false;

}

void SFHudRadar::SFHudRadarIconPackage::Init( IScaleformUI* pui, SFVALUE iconPackage )
{
	m_pScaleformUI = pui;
	m_IconPackage = pui->CreateValue( iconPackage );
	m_IconPackageRotate = m_pScaleformUI->Value_GetMember( iconPackage, "RotatedLayer" );

	for ( int i = 0; i < PI_NUM_ICONS && playerIconNames[i] != NULL; i++ )
	{
		if ( i < PI_FIRST_ROTATED )
		{
			m_Icons[i] = pui->Value_GetMember( iconPackage, playerIconNames[i] );
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
			pui->Value_SetVisible( m_Icons[i], ( m_iCurrentVisibilityFlags & ( 1 << i ) ) != 0 );
		}
	}

	ScaleformDisplayInfo displayInfo;
	displayInfo.SetAlpha( m_fCurrentAlpha * 100.0f );
	displayInfo.SetVisibility( m_iCurrentVisibilityFlags != 0 );
	m_pScaleformUI->Value_SetDisplayInfo( m_IconPackage, &displayInfo );
}


void SFHudRadar::SFHudRadarIconPackage::NukeFromOrbit( SFHudRadar* pSFUI )
{
	if ( pSFUI->FlashAPIIsValid() )
	{
		pSFUI->SafeReleaseSFVALUE( m_IconPackage );
		pSFUI->SafeReleaseSFVALUE( m_IconPackageRotate );

		for ( int i = 0; i < PI_NUM_ICONS; i++ )
		{
			pSFUI->SafeReleaseSFVALUE( m_Icons[i] );
		}
	}

	ClearAll();
}

void SFHudRadar::SFHudRadarIconPackage::StartRound( void )
{
	m_Health = 100;

	m_bOffMap = false;
	m_bIsSpotted = false;
	m_bIsSpottedByFriendsOnly = false;
	m_fGhostTime = TIMER_INIT;

	m_fLastColorUpdate = TIMER_INIT;

	m_bIsDead = false;
	m_fDeadTime = TIMER_INIT;

	m_fGrenExpireTime = TIMER_INIT;

	m_bIsRescued = false;

	m_nAboveOrBelow = R_SAMELEVEL;

	m_fRoundStartTime = gpGlobals->curtime;

	m_Position = vec3_origin;
	m_Angle.Init();

	SetAlpha( 0 );
	SetVisibilityFlags( 0 );
}

bool SFHudRadar::SFHudRadarIconPackage::IsVisible( void )
{
	return ( m_fCurrentAlpha != 0 && m_iCurrentVisibilityFlags != 0 );
}


void SFHudRadar::SFHudRadarIconPackage::SetIsPlayer( bool value )
{
	m_bIsPlayer = value;
}

void SFHudRadar::SFHudRadarIconPackage::SetIsSelected( bool value )
{
	m_bIsSelected = value;
}

void SFHudRadar::SFHudRadarIconPackage::SetIsSpeaking ( bool value )
{
	m_bIsSpeaking = value && m_bIsOnLocalTeam;
}

void SFHudRadar::SFHudRadarIconPackage::SetIsOffMap( bool value )
{
	m_bOffMap = value;
}

void SFHudRadar::SFHudRadarIconPackage::SetIsAboveOrBelow( int value )
{
	m_nAboveOrBelow = value;
}

void SFHudRadar::SFHudRadarIconPackage::SetIsMovingHostage( bool value )
{
	m_bIsMovingHostage = value;
}

void SFHudRadar::SFHudRadarIconPackage::SetIsRescued( bool value )
{
	m_bIsRescued = value;
}

void SFHudRadar::SFHudRadarIconPackage::SetIsOnLocalTeam( bool value )
{
	m_bIsOnLocalTeam = value;
}

void SFHudRadar::SFHudRadarIconPackage::SetIsBot( bool value )
{
	m_bIsBot = value;
}

void SFHudRadar::SFHudRadarIconPackage::SetIsControlledBot( void )
{
	m_bIsDead = true;
	//m_bIsSpotted = false;
	m_bIsSpotted = true;
	m_fDeadTime = TIMER_INIT;
	m_fGhostTime = TIMER_INIT;
	m_fLastColorUpdate = TIMER_INIT;
}


void SFHudRadar::SFHudRadarIconPackage::SetIsDead( bool value )
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

void SFHudRadar::SFHudRadarIconPackage::SetGrenadeExpireTime( float value )
{
	if ( value )
	{
		m_fGrenExpireTime = value;
	}
}

void SFHudRadar::SFHudRadarIconPackage::SetIsSpotted( bool value )
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

void SFHudRadar::SFHudRadarIconPackage::SetIsSpottedByFriendsOnly( bool value )
{
	if ( gpGlobals->curtime - m_fRoundStartTime > 0.25f )
	{
		m_bIsSpottedByFriendsOnly = value;
	}
}

void SFHudRadar::SFHudRadarIconPackage::SetPlayerTeam( int team )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if( pLocalPlayer == NULL )
	{
		return;
	}

	int newType;
	bool bSpec = ((pLocalPlayer && pLocalPlayer->IsSpectator() ) || engine->IsHLTV());

	switch( team )
	{
		case TEAM_UNASSIGNED:
			newType = PI_HOSTAGE;
			break;

		case TEAM_TERRORIST:
			newType = pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST || bSpec ? PI_T : PI_ENEMY;
			break;

		case TEAM_CT:
			newType = pLocalPlayer->GetAssociatedTeamNumber() == TEAM_CT || bSpec ? PI_CT : PI_ENEMY;
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

bool Helper_ShouldShowAssassinationTargetIcon( int iEntIndex )
{
	C_CSPlayer *pTarget= (C_CSPlayer*)UTIL_PlayerByIndex( iEntIndex );
	C_CS_PlayerResource *pCSPR = GetCSResources();
	if ( !pTarget || !pCSPR )
		return false;

	return ( !pCSPR->IsControllingBot( iEntIndex ) &&
		pTarget->IsAssassinationTarget() );
}


void SFHudRadar::SFHudRadarIconPackage::SetupIconsFromStates( void )
{
	int flags = 0;
	float alpha = 1;

	if ( m_iPlayerType != 0 )
	{
		flags = 1 << m_iPlayerType;

		if ( m_bIsPlayer )
		{
			flags |= 1 << PI_VIEWFRUSTRUM;
		}

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
						flags <<= 2;
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

				if ( m_bOffMap )
				{
					flags <<= 1;
				}
				else
				{
					flags <<= 3;
				}
			}
		}
		else
		{
			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
			bool bSpec = ( ( pLocalPlayer && pLocalPlayer->IsSpectator() ) || engine->IsHLTV());

			bool bShowLetters = ( !bSpec && m_iPlayerType != PI_ENEMY && !m_bOffMap && !IsHostageType( ) );
			if ( bSpec )
			{
				flags |= 1 << PI_PLAYER_NUMBER;
			}

			// this decides if the letter should be shown
			if ( bShowLetters || Helper_ShouldShowAssassinationTargetIcon( m_iEntityID ) )
			{
				flags |= 1 << PI_PLAYER_LETTER;
			}

			if ( m_bIsMovingHostage )
			{
				flags |= 1 << PI_HOSTAGE_MOVING;
			}

			if ( m_bIsSpeaking )
			{
				flags |= 1 << PI_SPEAKING;
			}
	
			if ( m_bIsSelected )
			{
				flags |= 1 << PI_SELECTED;
			}

			if ( bSpec == false && m_iPlayerType == PI_ENEMY && (m_bIsSpottedByFriendsOnly == false || pLocalPlayer->IsAlive() == false) )
			{
				flags |= 1 << PI_ENEMY_SEELOCAL;
			}

			// this will shift the MOVING and SPEAKING icons to their
			// offscreen versions as well ( look at the enum decaration )
			if ( m_bOffMap )
			{
				flags <<= 1;
			}
			else
			{
				if ( m_nAboveOrBelow == R_ABOVE )
				{
					flags |= 1 << PI_ABOVE;
				}
				else if ( m_nAboveOrBelow == R_BELOW )
				{
					flags |= 1 << PI_BELOW;
				}
				else
				{	
					if ( !bSpec && /*!bShowNumbers &&*/ m_iPlayerType != PI_ENEMY && !IsHostageType( ) )
						flags |= 1 << PI_DIRECTION_INDICATOR;
				}			
			}

//			if ( m_bIsPlayer )
//			{
//				flags |= 1 << PI_PLAYER_INDICATOR;
//			}
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

	SetAlpha( alpha );
	SetVisibilityFlags( flags );
	UpdateIconsPostion();
}


void SFHudRadar::SFHudRadarIconPackage::SetVisibilityFlags( int newFlags )
{
	newFlags &= ( ( 1 << PI_NUM_ICONS )-1 );

	int diffFlags = m_iCurrentVisibilityFlags ^ newFlags;

	if ( diffFlags )
	{
		if ( m_IconPackage )
		{
			bool wantVisible = ( newFlags != 0 );
			if ( ( m_iCurrentVisibilityFlags != 0 ) != wantVisible )
			{
				m_pScaleformUI->Value_SetVisible( m_IconPackage, wantVisible );
			}

			for ( int i = 0; i < PI_NUM_ICONS && ( diffFlags != 0 ); i++, diffFlags >>= 1 )
			{
				if ( (diffFlags & 1) && ( m_Icons[i] ) )
				{
					Assert( m_Icons[i] );
					if ( m_Icons[i] )
					{
						m_pScaleformUI->Value_SetVisible( m_Icons[i], ( newFlags & ( 1 << i ) ) != 0 );
					}
				}
			}
		}

		m_iCurrentVisibilityFlags = newFlags;
	}
}

void SFHudRadar::SFHudRadarIconPackage::UpdateIconsPostion( void )
{
	int visibilityFlags = m_iCurrentVisibilityFlags;

	if ( m_IconPackage )
	{
		for ( int i = 0; i < PI_NUM_ICONS && ( visibilityFlags != 0 ); i++, visibilityFlags >>= 1 )
		{
			if ( (visibilityFlags & 1) && ( m_Icons[i] ) )
			{
				Assert( m_Icons[i] );
				ScaleformDisplayInfo displayInfo;
				if ( cl_radar_fast_transforms.GetBool() )
				{
					displayInfo.SetX( m_HudPosition.x );
					displayInfo.SetY( m_HudPosition.y );
					displayInfo.SetXScale( m_HudScale );
					displayInfo.SetYScale( m_HudScale );
					if ( i >= PI_FIRST_ROTATED )
					{
						displayInfo.SetRotation( m_HudRotation );
					}
				}
				else
				{
					displayInfo.SetX( 0.0f );
					displayInfo.SetY( 0.0f );
					displayInfo.SetXScale( 100.0f );
					displayInfo.SetYScale( 100.0f );
					displayInfo.SetRotation( 0.0f );
				}				
				m_pScaleformUI->Value_SetDisplayInfo( m_Icons[i], &displayInfo );
			}
		}
	}
}

void SFHudRadar::SFHudRadarIconPackage::SetAlpha( float newAlpha )
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

void SFHudRadar::SFHudRadarIconPackage::SetIsDefuse( bool bValue )
{
	m_bIsDefuser = bValue;
}

/**********************************************************************
 **********************************************************************
 **********************************************************************
 **********************************************************************
 **********************************************************************
 * SFHudHostageIcon
 */

SFHudRadar::SFHudRadarHostageIcons::SFHudRadarHostageIcons()
{
	m_IconPackage = NULL;
	V_memset( m_Icons, 0,sizeof( m_Icons ) );
	m_iCurrentIcon = HI_UNUSED;
}

SFHudRadar::SFHudRadarHostageIcons::~SFHudRadarHostageIcons()
{
}

void SFHudRadar::SFHudRadarHostageIcons::Init( IScaleformUI* scaleformui, SFVALUE iconPackage )
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

void SFHudRadar::SFHudRadarHostageIcons::SetStatus( int status )
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


void SFHudRadar::SFHudRadarHostageIcons::ReleaseHandles( SFHudRadar* pradar )
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
 * SFHudRadar Code
 *
 */

SFHudRadar::SFHudRadar( const char *value ) : SFHudFlashInterface( value ),
	m_bVisible( false ),	
	m_fMapSize( 1.0f ),
	m_fRadarSize( 1.0f ),
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
	m_iLastDecoyIndex( INVALID_INDEX ),
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
	m_nBombHolderUserId( -1 ),
	m_bShowBombHighlight( false ),
	m_bShowRadar( true ),
	m_bShowAll( false ),
	m_bShowingDashboard( false ),
	m_iObserverMode( OBS_MODE_NONE ),
	m_bTrackDefusers( false ),
	m_bRound( true )
{
	SetHiddenBits( HIDEHUD_RADAR );
    m_bWantLateUpdate = true;
	
	m_cDesiredMapName[0] = 0;
	m_cLoadedMapName[0] = 0;
	m_wcLocationString[0] = 0;

	V_memset( m_BombZoneIcons, 0, sizeof( m_BombZoneIcons ) );
	V_memset( m_HostageZoneIcons, 0, sizeof( m_HostageZoneIcons ) );
	V_memset( m_GoalIcons, 0, sizeof( m_GoalIcons ) );
	V_memset( m_HostageStatusIcons, 0, sizeof( m_HostageStatusIcons ) );
	V_memset( m_Icons, 0, sizeof( m_Icons ) );		

	m_EntitySpotted.ClearAll();

	m_nCurrentRadarVerticalSection = -1;
	m_vecRadarVerticalSections.RemoveAll();
}


SFHudRadar::~SFHudRadar()
{
}

/**************************************
 * hud element functions
 */

void SFHudRadar::Init( void )
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

	HOOK_HUD_MESSAGE( SFHudRadar, ProcessSpottedEntityUpdate );
}

void SFHudRadar::LevelInit( void )
{
	if ( !m_bFlashReady && !m_bFlashLoading )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudRadar, this, Radar );
	}

}

void SFHudRadar::LevelShutdown( void )
{
	if ( m_bFlashReady || m_bFlashLoading )
	{
		RemoveFlashElement();
	}
}

ConVar cl_drawhud_force_radar( "cl_drawhud_force_radar", "0", FCVAR_RELEASE, "0: default; 1: draw radar even if hud disabled; -1: force no radar" );
bool SFHudRadar::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer( );
	if ( !pPlayer )
		return false;

	bool bSpec = pPlayer->IsSpectator( ) || engine->IsHLTV( );

	if ( bSpec == false )
	{	
// 		//Don't show the radar while the win panel is up (only look at slot 0 because that is the only "true" Win Panel)
// 		SFHudWinPanel * pWinPanel = ( SFHudWinPanel* ) ( GetHud( 0 ).FindElement( "SFHudWinPanel" ) );
// 		if ( pWinPanel && pWinPanel->IsVisible( ) )
// 		{
// 			return false;
// 		}

		if ( pPlayer->GetTeamNumber() == TEAM_UNASSIGNED ||
			 ( pPlayer->GetTeamNumber() == TEAM_UNASSIGNED && pPlayer->GetPendingTeamNumber() != TEAM_SPECTATOR ) )
			 return false;
	}

	return m_bShowRadar && ( cl_drawhud_force_radar.GetInt() >= 0 ) && (
		( cl_drawhud_force_radar.GetInt() > 0 ) ||
		( cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false )
		) && CHudElement::ShouldDraw();
}


void SFHudRadar::SetActive( bool bActive )
{
	if ( bActive != m_bVisible )
		Show( bActive );

	CHudElement::SetActive( bActive );
}

void SFHudRadar::Show( bool show )
{
	if ( !m_pScaleformUI )
		return;

	WITH_SLOT_LOCKED
	{
		if ( show )
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );

			for ( int i = 0; i <= MAX_PLAYERS; ++i )
			{
				SFHudRadarIconPackage* pPackage = GetRadarPlayer( i );
				if ( pPackage )
					pPackage->m_fLastColorUpdate = TIMER_INIT;
			}

			UpdateAllPlayerNumbers();

			m_bVisible = true;
		}
		else
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", NULL, 0 );
			m_bVisible = false;
		}
	}
}

/**********************************
 * initialization code goes here
 */


// these overload the ScaleformFlashInterfaceMixin class
void SFHudRadar::FlashLoaded( void )
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
			return;

		m_IconTranslation = m_pScaleformUI->Value_GetMember( m_IconRotation, "IconTranslation" );

		if ( !m_IconTranslation )
			return;

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
	m_Icons[RI_DASHBOARD] = dashboard;

	if ( !dashboard )
		return;

	//SFVALUE C4andHostages = m_pScaleformUI->Value_GetMember( m_RadarModule, "C4" );
	//if ( !C4andHostages )
	//	return;

	SFVALUE hostageIcons = m_pScaleformUI->Value_GetMember( m_RadarModule, "Hostages" );

	char hostageIconName[20] = {"HostageStatusX"};

	int index = 0;
	SFVALUE hostageIcon;

	do
	{
		hostageIconName[13] = index + '1';
		hostageIcon = m_pScaleformUI->Value_GetMember( hostageIcons, hostageIconName );
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

	for ( int i = 0; radarIconNames[i] != NULL; i++ )
	{
		m_Icons[i] = m_pScaleformUI->Value_GetMember( hostageIcons, radarIconNames[i] );
	}

	m_LocationText = m_pScaleformUI->TextObject_MakeTextObjectFromMember( dashboard, "Location" );
	if ( m_LocationText )
	{
		m_LocationText->SetText( m_wcLocationString );
	}

	SafeReleaseSFVALUE( hostageIcons );
}


void SFHudRadar::FlashReady( void )
{
	// these aren't ready until after the first frame, so they have to get connected here instead of in
	// FlashLoaded()

	m_Icons[RI_BOMB_ICON_PACKAGE] = m_pScaleformUI->Value_Invoke( m_IconTranslation, "createBombPack", NULL, 0 );
	if ( m_Icons[RI_BOMB_ICON_PACKAGE] )
	{
		m_Icons[RI_BOMB_ICON_DROPPED] = m_pScaleformUI->Value_GetMember( m_Icons[RI_BOMB_ICON_PACKAGE], "DroppedBomb" );
		m_Icons[RI_BOMB_ICON_PLANTED] = m_pScaleformUI->Value_GetMember( m_Icons[RI_BOMB_ICON_PACKAGE], "PlantedBomb" );
		m_Icons[RI_BOMB_ICON_BOMB_CT] = m_pScaleformUI->Value_GetMember( m_Icons[RI_BOMB_ICON_PACKAGE], "BombCT" );
		m_Icons[RI_BOMB_ICON_BOMB_T] = m_pScaleformUI->Value_GetMember( m_Icons[RI_BOMB_ICON_PACKAGE], "BombT" );

		m_Icons[RI_BOMB_ICON_BOMB_ABOVE] = m_pScaleformUI->Value_GetMember( m_Icons[RI_BOMB_ICON_PACKAGE], "BombAbove" );
		m_Icons[RI_BOMB_ICON_BOMB_BELOW] = m_pScaleformUI->Value_GetMember( m_Icons[RI_BOMB_ICON_PACKAGE], "BombBelow" );
	}

	for ( int i = 0; i < RI_NUM_ICONS; i++ )
	{
		if ( m_Icons[i] )
		{
			m_pScaleformUI->Value_SetVisible( m_Icons[i], false );
		}
	}

	m_bFlashLoading = false;
	m_bFlashReady = true;
	ResetForNewMap();

	// there will be a map waiting for us to load

	FlashLoadMap( NULL );
// 
// 	if ( m_bActive )
// 	{
// 		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );
// 		m_bVisible = true;
// 	}
// 	else
// 	{
// 		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", NULL, 0 );
// 		m_bVisible = false;
// 	}
}

void SFHudRadar::LazyCreateGoalIcons( void )
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



bool SFHudRadar::PreUnloadFlash( void )
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

	RemoveAllDecoys();

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
		SafeReleaseSFVALUE( m_Icons[i] );
	}

	m_bFlashReady = false;
	m_cDesiredMapName[0] = 0;
	m_cLoadedMapName[0] = 0;

	m_bActive = false;
	m_bVisible = false;

	return true;
}

/*********************************************************
 * set up the background map
 */

void SFHudRadar::SetMap( const char* pMapName )
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

	KeyValues* pSections = MapKeyValues->FindKey( "verticalsections" );
	if ( pSections )
	{
		int nIndex = 0;
		for ( KeyValues *kSection = pSections->GetFirstSubKey(); kSection != NULL; kSection = kSection->GetNextKey() )
		{
			float flAltMin = kSection->GetFloat( "AltitudeMin", 0.0f );
			float flAltMax = kSection->GetFloat( "AltitudeMax", 0.0f );

			if ( flAltMin < flAltMax )
			{
				HudRadarLevelVerticalSection_t *pNewSection = &m_vecRadarVerticalSections[ m_vecRadarVerticalSections.AddToTail() ];
				
				if ( !V_strcmp( kSection->GetName(), "default" ) )
				{
					V_strcpy_safe( pNewSection->m_szSectionName, pMapName );
				}
				else
				{
					V_sprintf_safe( pNewSection->m_szSectionName, "%s_%s", pMapName, kSection->GetName() );
				}

				pNewSection->m_nSectionIndex = nIndex;
				pNewSection->m_flSectionAltitudeFloor = flAltMin;
				pNewSection->m_flSectionAltitudeCeiling = flAltMax;
				nIndex++;
			}
			else
			{
				DevWarning( "Radar vertical section is invalid!\n" );
			}
		}

		if ( m_vecRadarVerticalSections.Count() )
		{
			DevMsg( "Loaded radar vertical section keyvalues.\n" );
		}
	}
	else
	{
		// map doesn't have sections, clear any sections from previous map
		// TODO: Perhaps initialize a single giant vertical section that covers the whole map,
		//       to reduce variation in handling radar in downstream code?
		m_vecRadarVerticalSections.Purge();
	}

	// TODO release old texture ?

	m_MapOrigin.x	= MapKeyValues->GetInt( "pos_x" );
	m_MapOrigin.y	= MapKeyValues->GetInt( "pos_y" );
	m_MapOrigin.z   = 0;
	m_fWorldToPixelScale = 1.0f / MapKeyValues->GetFloat( "scale", 1.0f );

	m_fWorldToRadarScale = m_fWorldToPixelScale * m_fPixelToRadarScale;

	MapKeyValues->deleteThis();

}


void SFHudRadar::FlashLoadMap( const char* pMapName )
{
	const char* pMapToLoad = pMapName ? pMapName : m_cDesiredMapName;

	if ( V_strcmp( pMapToLoad, m_cLoadedMapName ) )
	{
		V_strcpy_safe( m_cDesiredMapName, pMapToLoad );

		if ( m_bFlashReady )
		{
			if (!m_vecRadarVerticalSections.Count())
			{
				// Single map
				WITH_SFVALUEARRAY_SLOT_LOCKED(args, 1)
				{
					m_pScaleformUI->ValueArray_SetElement(args, 0, pMapToLoad);
					m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "loadMap", args, 1);
				}
			}
			else
			{
				// Multiple map levels
				int numArgs = m_vecRadarVerticalSections.Count() + 1;
				WITH_SFVALUEARRAY_SLOT_LOCKED(args, numArgs)
				{
					m_pScaleformUI->ValueArray_SetElement(args, 0, pMapToLoad);
					FOR_EACH_VEC(m_vecRadarVerticalSections, i)
					{
						m_pScaleformUI->ValueArray_SetElement(args, i + 1, m_vecRadarVerticalSections[i].m_szSectionName);
					}
					m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "loadMap2", args, numArgs);
				}
			}
		}

		// Force current map layer to update
		m_nCurrentRadarVerticalSection = -1;
	}
}

void SFHudRadar::FlashUpdateMapLayer( int layerIdx )
{
	if (m_bFlashReady)
	{
		WITH_SFVALUEARRAY_SLOT_LOCKED(args, 1)
		{
			m_pScaleformUI->ValueArray_SetElement(args, 0, layerIdx);
			m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "setVisibleLayer", args, 1);
		}
	}
}

void SFHudRadar::MapLoaded( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// flash passes in the scaledsize / image size
	m_fMapSize = 320; //radar_mapsize.GetInt();//( float )pui->Params_GetArgAsNumber( obj, 0 );

	m_fPixelToRadarScale = ( float )pui->Params_GetArgAsNumber( obj, 0 );

	m_fRadarSize = ( float )pui->Params_GetArgAsNumber( obj, 1 );


	m_fWorldToRadarScale = m_fWorldToPixelScale * m_fPixelToRadarScale;

	V_strcpy_safe( m_cLoadedMapName, m_cDesiredMapName );
}


/**********************************************
 * reset functions
 */


void SFHudRadar::ResetForNewMap( void )
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
			m_HostageStatusIcons[index++].SetStatus( SFHudRadarHostageIcons::HI_UNUSED );
		}

		RemoveAllDecoys();
		RemoveAllDefusers();
	}

	m_iNumGoalIcons = 0;
	m_bGotGoalIcons = false;

	for ( int i = 0; i <= m_iLastPlayerIndex; i++ )
	{
		ResetPlayer( i );
	}

	ResetRoundVariables();

}

void SFHudRadar::ResetRoundVariables( bool bResetGlobalStates )
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

	ConVarRef mp_defuser_allocation( "mp_defuser_allocation" );
	m_bTrackDefusers = ( mp_defuser_allocation.GetInt() == DefuserAllocation::Random );

	if ( bResetGlobalStates )
	{
		m_nBombEntIndex = -1;
		m_nBombHolderUserId = -1;
		m_BombPosition = vec3_origin;
		m_DefuserPosition = vec3_origin;
		m_bBombIsSpotted = false;
		m_bBombPlanted = false;
		m_bBombDropped = false;
		m_bBombExploded = false;
		m_bBombDefused = false;
	}

	m_EntitySpotted.ClearAll();
}

void SFHudRadar::ResetRadar( bool bResetGlobalStates )
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
			m_HostageStatusIcons[index++].SetStatus( SFHudRadarHostageIcons::HI_UNUSED );
		}
	}

	if ( bResetGlobalStates )
	{
		RemoveAllDecoys();
		RemoveAllDefusers();
		RemoveAllHostages();
	}

	ResetRoundVariables( bResetGlobalStates );

	Show( true );
}

void SFHudRadar::ResetRound( void )
{
	ResetRadar( true );
}


/*****************************************************
 * player icon loading / creating
 */

bool SFHudRadar::LazyUpdateIconArray( SFHudRadarIconPackage* pArray, int lastIndex )
{
	bool result = true;
	int i;
	SFHudRadarIconPackage* pwalk;

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

void SFHudRadar::LazyCreatePlayerIcons( void )
{
	if ( !m_bFlashReady || m_bGotPlayerIcons )
		return;

	// the following code looks odd, but it insures that all three of the LazyUpdate calls are made
	// even if m_bGotPlayerIcons becomes false early.

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Players, m_iLastPlayerIndex );

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Hostages, m_iLastHostageIndex ) && m_bGotPlayerIcons;

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Decoys, m_iLastDecoyIndex ) && m_bGotPlayerIcons;

	m_bGotPlayerIcons = LazyUpdateIconArray( m_Defusers, m_iLastDefuserIndex ) && m_bGotPlayerIcons;

}

bool SFHudRadar::LazyCreateIconPackage( SFHudRadarIconPackage* pIconPack )
{
	if ( m_bFlashReady && pIconPack->m_bIsActive && !pIconPack->m_IconPackage )
	{
		SFVALUE result = NULL;

		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 2 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, pIconPack->m_iIndex );
			m_pScaleformUI->ValueArray_SetElement( args, 1, pIconPack->m_IconPackType );
			result = m_pScaleformUI->Value_Invoke( m_IconTranslation, "createIconPack", args, 2 );
		
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

void SFHudRadar::InitIconPackage( SFHudRadarIconPackage* pPlayer, int iAbsoluteIndex, ICON_PACK_TYPE iconType )
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

void SFHudRadar::RemoveIconPackage( SFHudRadarIconPackage* pPackage )
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


/**********************************************
 * player and hostage initialization and creation
 */

void SFHudRadar::UpdateAllPlayerNumbers( void )
{
	for ( int i = 0; i <= MAX_PLAYERS; ++i )
	{
		SFHudRadarIconPackage* pPackage = GetRadarPlayer( i );
		if ( pPackage )
			UpdatePlayerNumber( pPackage );	
	}
}

void SFHudRadar::UpdatePlayerNumber( SFHudRadarIconPackage* pPackage )
{
	int nMaxPlayers = CCSGameRules::GetMaxPlayers();
	
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pLocalPlayer )
		return;

	bool bSpec = pLocalPlayer->IsSpectator() || engine->IsHLTV();

	// update the player number
	SFHudTeamCounter* pTeamCounter = GET_HUDELEMENT( SFHudTeamCounter );
	if ( pPackage && pPackage->m_Icons[PI_PLAYER_NUMBER] )
	{
		if ( pTeamCounter )
		{	
			int pidx = pTeamCounter->GetPlayerSlotIndex(pPackage->m_iEntityID);
			if ( pidx != -1 )
			{
				if (bSpec)
				{
					pidx += 1;
					if (pidx == 10)
						pidx = 0;

					ISFTextObject* textPanel = g_pScaleformUI->TextObject_MakeTextObjectFromMember(pPackage->m_Icons[PI_PLAYER_NUMBER], "Text");
					if (textPanel)
					{
						WITH_SLOT_LOCKED
						{
						if (nMaxPlayers <= 10)
						{
							textPanel->SetText(pidx);
						}
						else
						{
							textPanel->SetText("");
						}
					}
						SafeReleaseSFTextObject(textPanel);
					}
				}
				else
				{
					bool bShowLetter = false;

					if ((pPackage->m_fLastColorUpdate + 0.5f) < gpGlobals->curtime)
					{
						int nOtherTeamNum = 0;
						if (pPackage->m_iPlayerType == PI_CT)
							nOtherTeamNum = TEAM_CT;
						else if (pPackage->m_iPlayerType == PI_T)
							nOtherTeamNum = TEAM_TERRORIST;

						int nColorID = -1;
						if (pLocalPlayer->ShouldShowTeamPlayerColors(nOtherTeamNum))
						{
							C_CS_PlayerResource* pCSPR = ( C_CS_PlayerResource* )g_PR;
							if ( pCSPR )
								nColorID = pCSPR->GetCompTeammateColor( pPackage->m_iEntityID );

							bShowLetter = pLocalPlayer->ShouldShowTeamPlayerColorLetters( );
						}

						WITH_SLOT_LOCKED
						{
							if (pPackage->m_iPlayerType == PI_CT && pLocalPlayer->GetAssociatedTeamNumber() == TEAM_CT)
							{
								// 							ScaleformDisplayInfo displayInfo;
								// 							displayInfo.SetAlpha(100.0f);
								// 							displayInfo.SetVisibility(1);
								// 							m_pScaleformUI->Value_SetDisplayInfo(pPackage->m_IconPackage, &displayInfo);

								SFVALUE doton = m_pScaleformUI->Value_GetMember(pPackage->m_Icons[PI_CT], "Dot");
								SFVALUE dotoff = m_pScaleformUI->Value_GetMember(pPackage->m_Icons[PI_CT_OFFMAP], "Arrow");
								WITH_SFVALUEARRAY_SLOT_LOCKED(args, 2)
								{
									m_pScaleformUI->ValueArray_SetElement(args, 0, doton);
									m_pScaleformUI->ValueArray_SetElement(args, 1, nColorID);
									m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "SetPlayerObjectColor", args, 2);
								}
								WITH_SFVALUEARRAY_SLOT_LOCKED(args, 2)
								{
									m_pScaleformUI->ValueArray_SetElement(args, 0, dotoff);
									m_pScaleformUI->ValueArray_SetElement(args, 1, nColorID);
									m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "SetPlayerObjectColor", args, 2);
								}
								WITH_SFVALUEARRAY_SLOT_LOCKED(args, 2)
								{
									m_pScaleformUI->ValueArray_SetElement(args, 0, pPackage->m_Icons[PI_CT_DEAD]);
									m_pScaleformUI->ValueArray_SetElement(args, 1, nColorID);
									m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "SetPlayerObjectColor", args, 2);
								}
								SafeReleaseSFVALUE(doton);
								SafeReleaseSFVALUE(dotoff);
							}
							else if (pPackage->m_iPlayerType == PI_T && pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST)
							{
								if (pPackage->m_bIsDead == false && pPackage->m_Health > 0)
								{
									ScaleformDisplayInfo displayInfo;
									displayInfo.SetAlpha(100.0f);
									displayInfo.SetVisibility(1);
									m_pScaleformUI->Value_SetDisplayInfo(pPackage->m_IconPackageRotate, &displayInfo);
									//m_pScaleformUI->Value_SetDisplayInfo(pPackage->m_IconPackage, &displayInfo);

									SFVALUE doton = m_pScaleformUI->Value_GetMember(pPackage->m_Icons[PI_T], "Dot");
									SFVALUE dotoff = m_pScaleformUI->Value_GetMember(pPackage->m_Icons[PI_T_OFFMAP], "Arrow");

									WITH_SFVALUEARRAY_SLOT_LOCKED(args, 2)
									{
										//m_pScaleformUI->Value_SetDisplayInfo(pPackage->m_Icons[PI_T], &displayInfo);

										m_pScaleformUI->ValueArray_SetElement(args, 0, doton);
										m_pScaleformUI->ValueArray_SetElement(args, 1, nColorID);
										m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "SetPlayerObjectColor", args, 2);
									}

									WITH_SFVALUEARRAY_SLOT_LOCKED(args, 2)
									{
										m_pScaleformUI->ValueArray_SetElement(args, 0, dotoff);
										m_pScaleformUI->ValueArray_SetElement(args, 1, nColorID);
										m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "SetPlayerObjectColor", args, 2);
									}

									WITH_SFVALUEARRAY_SLOT_LOCKED(args, 2)
									{
										m_pScaleformUI->ValueArray_SetElement(args, 0, pPackage->m_Icons[PI_T_DEAD]);
										m_pScaleformUI->ValueArray_SetElement(args, 1, nColorID);
										m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "SetPlayerObjectColor", args, 2);
									}
									SafeReleaseSFVALUE(doton);
									SafeReleaseSFVALUE(dotoff);
								}
							}

							WITH_SFVALUEARRAY_SLOT_LOCKED( args, 2 )
							{
								m_pScaleformUI->ValueArray_SetElement( args, 0, pPackage->m_Icons[PI_PLAYER_LETTER] );
								m_pScaleformUI->ValueArray_SetElement( args, 1, bShowLetter ? nColorID : -1 );
								m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetPlayerColorLetter", args, 2 );
							}

							// decide if the target shows the symbol
							// make sure they're no the other team
							if ( pPackage->m_iPlayerType == PI_ENEMY && Helper_ShouldShowAssassinationTargetIcon( pPackage->m_iEntityID ) )
							{
								WITH_SFVALUEARRAY_SLOT_LOCKED( args, 2 )
								{
									// unique number for targets for missions
									int nSpecialTargetID = 10;
									m_pScaleformUI->ValueArray_SetElement( args, 0, pPackage->m_Icons[PI_PLAYER_LETTER] );
									m_pScaleformUI->ValueArray_SetElement( args, 1, nSpecialTargetID );
									m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetPlayerColorLetter", args, 2 );
								}
							}
						}

						pPackage->m_fLastColorUpdate = gpGlobals->curtime;
					}
				}
			}
		}
	}
}


SFHudRadar::SFHudRadarIconPackage* SFHudRadar::CreatePlayer( int index )
{
	SFHudRadarIconPackage* pPackage = GetRadarPlayer( index );
	InitIconPackage( pPackage, index, ICON_PACK_PLAYER );

	m_iLastPlayerIndex = MAX( index, m_iLastPlayerIndex );

	return pPackage;
}


void SFHudRadar::ResetPlayer( int index )
{
	SFHudRadarIconPackage* pPackage = GetRadarPlayer( index );
	if ( pPackage->m_bIsActive )
	{
		pPackage->StartRound();

		UpdatePlayerNumber( pPackage );
	}
}

void SFHudRadar::RemovePlayer( int index )
{
	RemoveIconPackage( GetRadarPlayer( index ) );

	if ( index == m_iLastPlayerIndex )
	{
		while( m_iLastPlayerIndex >= 0 && !m_Players[m_iLastPlayerIndex].m_bIsActive )
		{
			m_iLastPlayerIndex--;
		}
	}

	UpdateAllPlayerNumbers();
}

SFHudRadar::SFHudRadarIconPackage* SFHudRadar::CreateHostage( int index )
{
	SFHudRadarIconPackage* pPackage = GetRadarHostage( index );
	InitIconPackage( pPackage, index, ICON_PACK_HOSTAGE );

	m_iLastHostageIndex = MAX( index, m_iLastHostageIndex );

	return pPackage;
}

void SFHudRadar::ResetHostage( int index )
{
	SFHudRadarIconPackage* pPackage = GetRadarHostage( index );
	if ( pPackage->m_bIsActive )
	{
		pPackage->StartRound();
	}
}

void SFHudRadar::RemoveAllHostages( void )
{
	for ( int index = 0; index < MAX_HOSTAGES; index++ )
	{
		RemoveHostage( index );
	}
}

void SFHudRadar::RemoveStaleHostages( void )
{
	// Remove hostages that are no longer tracked by the player resource (hostages that have died)

	C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* )GameResources();
	if ( !pCSPR )
	{
		return;
	}

	for ( int index = 0; index < MAX_HOSTAGES; index++ )
	{
		SFHudRadarIconPackage* pPackage = GetRadarHostage( index );
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

void SFHudRadar::RemoveHostage( int index )
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


SFHudRadar::SFHudRadarIconPackage* SFHudRadar::CreateDecoy( int entityID )
{
	SFHudRadarIconPackage* pPackage = NULL;
	int index = m_iLastDecoyIndex+1;

	for ( int i = 0; i <= m_iLastDecoyIndex; i++ )
	{
		if ( !m_Decoys[i].m_bIsActive )
		{
			index = i;
			break;
		}
	}

	if ( index < MAX_DECOYS )
	{
		pPackage = GetRadarDecoy( index );

		InitIconPackage( pPackage, index, ICON_PACK_DECOY );

		pPackage->m_iEntityID = entityID;
		pPackage->m_fRoundStartTime = TIMER_INIT;

		m_iLastDecoyIndex = MAX( index, m_iLastDecoyIndex );
	}

	return pPackage;
}

void SFHudRadar::RemoveAllDecoys( void )
{
	for ( int i = 0; i <= m_iLastDecoyIndex; i++ )
	{
		if ( m_Decoys[i].m_bIsActive )
		{
			RemoveDecoy( i );
		}
	}
}


void SFHudRadar::RemoveDecoy( int index )
{
	RemoveIconPackage( GetRadarDecoy( index ) );

	if ( index == m_iLastDecoyIndex )
	{
		while( m_iLastDecoyIndex >= 0 && !m_Decoys[m_iLastDecoyIndex].m_bIsActive )
		{
			m_iLastDecoyIndex--;
		}
	}
}

SFHudRadar::SFHudRadarIconPackage * SFHudRadar::GetDefuser( int nEntityID, bool bCreateIfNotFound )
{
	SFHudRadarIconPackage * pResult = NULL;

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



void SFHudRadar::SetDefuserPos( int nEntityID, int x, int y, int z, int a )
{
	if ( !m_bTrackDefusers )
	{
		return;
	}

	SFHudRadarIconPackage * pDefuser = GetDefuser( nEntityID, true );

	AssertMsg( pDefuser != NULL, "Defuser not found. Update failed." );

	if ( pDefuser )
	{
		pDefuser->m_Position.Init( x, y, z ) ;
		pDefuser->SetAlpha( 1.0f );

		SetIconPackagePosition( pDefuser );
	}
}

// defusers attached to players will update during the PlacePlayer phase
SFHudRadar::SFHudRadarIconPackage* SFHudRadar::CreateDefuser( int nEntityID )
{
	if ( !m_bTrackDefusers )
	{
		return NULL;
	}

	SFHudRadarIconPackage* pPackage = NULL;
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

void SFHudRadar::RemoveAllDefusers( void )
{
	for ( int i = 0; i <= m_iLastDefuserIndex; i++ )
	{
		if ( m_Defusers[i].m_bIsActive )
		{
			RemoveDefuser( i );
		}
	}
}

void SFHudRadar::RemoveDefuser( int index )
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



void SFHudRadar::SetPlayerTeam( int index, int team )
{
	SFHudRadarIconPackage* pPlayer = GetRadarPlayer( index );
	pPlayer->SetPlayerTeam( team );
}

int SFHudRadar::GetPlayerIndexFromUserID( int userID )
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

int SFHudRadar::GetHostageIndexFromHostageEntityID( int entityID )
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

int SFHudRadar::GetDecoyIndexFromEntityID( int entityID )
{
	for ( int i = 0; i <= m_iLastDecoyIndex; i++ )
	{
		if ( m_Decoys[i].m_bIsActive && m_Decoys[i].m_iEntityID == entityID )
		{
			return i;
		}
	}

	return INVALID_INDEX;
}

int SFHudRadar::GetDefuseIndexFromEntityID( int nEntityID )
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



SFHudRadar::SFHudRadarIconPackage* SFHudRadar::GetRadarPlayer( int index )
{
	return &m_Players[index];
}

SFHudRadar::SFHudRadarIconPackage* SFHudRadar::GetRadarHostage( int index )
{
	return &m_Hostages[index];
}

SFHudRadar::SFHudRadarIconPackage* SFHudRadar::GetRadarDecoy( int index )
{
	return &m_Decoys[index];
}

SFHudRadar::SFHudRadarIconPackage* SFHudRadar::GetRadarDefuser( int index )
{
	return &m_Defusers[index];
}

/*************************************************
 * We don't get updates from the server from players
 * that are not in our PVS, so there are separate
 * messages specifically to update the radar
 */
bool SFHudRadar::MsgFunc_ProcessSpottedEntityUpdate( const CCSUsrMsg_ProcessSpottedEntityUpdate &msg )
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

		// non-class specific data
		// read position and angle
		int x = update.origin_x() * 4;
		int y = update.origin_y() * 4;
		int z = update.origin_z() * 4;
		int a = update.angle_y();

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
		
		// Clients are unaware of the defuser class type, so we need to flag defuse entities manually
		if ( update.defuser() )
		{
			SetDefuserPos( nEntityID, x, y, z, a );
		}
		else if ( V_strcmp( "CCSPlayer", szEntityClass ) == 0 )
		{
			SFHudRadarIconPackage* pPlayerIcon = NULL;

			pPlayerIcon = GetRadarPlayer( nEntityID - 1 );

			if ( pPlayerIcon->m_bIsDead )
				continue;

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
				SFHudRadarIconPackage* pPackage = GetRadarHostage( hostageIndex );

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


// handles all game event messages ( first looks them up in map so there's no long list of strcmps )
void SFHudRadar::FireGameEvent( IGameEvent *event )
{
	const char* eventName = event->GetName();
	int elementIndex = m_messageMap.Find( eventName );
	if ( elementIndex == m_messageMap.InvalidIndex() )
	{
		return;
	}

	SF_SPLITSCREEN_PLAYER_GUARD();


	DESIRED_MESSAGE_INDICES messageTypeIndex = ( DESIRED_MESSAGE_INDICES )m_messageMap.Element( elementIndex );

	switch( messageTypeIndex )
	{
		case GAME_NEWMAP:
			ResetForNewMap();
			SetMap( event->GetString( "mapname" ) );
		break;

		case ROUND_POST_START:
		{
			ResetRound();
			UpdateAllPlayerNumbers();
		}			
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

			SFHudRadarIconPackage* pPackage = CreatePlayer( index );
			if ( messageTypeIndex == PLAYER_CONNECT )
			{
				pPackage->SetPlayerTeam( TEAM_SPECTATOR );
			}

			pPackage->m_iEntityID = userID;

			const char* name = event->GetString( "name","unknown" );

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

			SFHudRadarIconPackage* pPackage = GetRadarPlayer( playerIndex );
			pPackage->SetPlayerTeam( event->GetInt( "team" ) );
			UpdatePlayerNumber( pPackage );

			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
			if ( pLocalPlayer )
			{
				int localID = pLocalPlayer->GetUserID();
				if ( localID == playerIndex )
				{
					ResizeHud();
				}
			}
		}
		break;

		case PLAYER_DEATH:
		{
			int playerIndex = GetPlayerIndexFromUserID( event->GetInt( "userid" ) );

			if ( playerIndex == INVALID_INDEX )
			{
				return;
			}

			SFHudRadarIconPackage* pPackage = GetRadarPlayer( playerIndex );

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

			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
			if ( pLocalPlayer )
			{
				int localID = pLocalPlayer->GetUserID();
				if ( localID == playerIndex )
				{
					ResizeHud();
				}
			}		
		}
		break;

		case PLAYER_SPAWN:
		{
			int playerIndex = GetPlayerIndexFromUserID( event->GetInt( "userid" ) );

			if ( playerIndex != INVALID_INDEX )
			{
				SFHudRadarIconPackage* pPackage = GetRadarPlayer( playerIndex );
				pPackage->m_Health = 100;

				UpdatePlayerNumber( pPackage );
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

			SFHudRadarIconPackage* pPackage = GetRadarHostage( hostageIndex );

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

			SFHudRadarIconPackage* pPackage = GetRadarHostage( hostageIndex );

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
			m_nBombHolderUserId = -1;
		}
		break;

		case BOMB_PICKUP:
		{
			m_bBombDropped = false;
			m_nBombEntIndex = -1;
			m_nBombHolderUserId = event->GetInt( "userid", -1 );
		}
		break;

		case BOMB_DROPPED:
		{
			m_nBombEntIndex = event->GetInt( "entindex", -1 );
			m_bBombDropped = true;
			m_nBombHolderUserId = -1;
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
			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
			if ( pLocalPlayer )
			{
				int userID = event->GetInt( "userid" );
				CBasePlayer* player =  UTIL_PlayerByUserId( userID );

				if ( player )
				{
					// the radar only shows the enemy decoys
					if ( pLocalPlayer->IsOtherEnemy( player->entindex() ) )
					{
						int entityID = event->GetInt( "entityid" );
						int teamNumber = player->GetAssociatedTeamNumber();

						SFHudRadarIconPackage* pPackage = CreateDecoy( entityID );

						if ( pPackage )
						{
							pPackage->SetPlayerTeam( teamNumber );
							pPackage->SetIsSpotted( true );
							// this sets the exact moment when the grenade should no longer be visible incase we miss the event
							pPackage->SetGrenadeExpireTime( gpGlobals->curtime + 24 );

							pPackage->m_Position.Init( ( float )event->GetInt( "x" ), ( float )event->GetInt( "y" ), 0.0f );
							pPackage->m_Angle.Init( 0.0f, RandomFloat( 0.0f, 360.0f ), 0.0f );
							SetIconPackagePosition( pPackage );
						}
					}
				}
			}

		}
		break;

		case DECOY_DETONATE:
		{
			int entityID = event->GetInt( "entityid" );
			int i = GetDecoyIndexFromEntityID( entityID );
			if ( i != INVALID_INDEX )
			{
				SFHudRadarIconPackage* pPackage = GetRadarDecoy( i );
				pPackage->SetIsSpotted( false );
				SetIconPackagePosition( pPackage );
			}

			STEAMWORKS_TESTSECRET_AMORTIZE( 31 );
		}
		break;

		case BOT_TAKEOVER:
		{
			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
			if ( pLocalPlayer )
			{
				int userID = event->GetInt( "userid" );
				//int playerIndex = GetPlayerIndexFromUserID(event->GetInt("userid"));
				int localID = pLocalPlayer->GetUserID();

				if (localID == userID )
				{
					ResizeHud();
					ResetRadar( false );

					int playerIndex = GetPlayerIndexFromUserID(userID);
					if (playerIndex != INVALID_INDEX)
					{
						SFHudRadarIconPackage* pPackage = GetRadarPlayer(playerIndex);
						UpdatePlayerNumber(pPackage);
					}
				}
			}

		}
		break;

		default:
			break;
	}
}


/*****************************************************
 * these set lazy update the icons and text based
 * on the values in the state variables
 */

void SFHudRadar::SetupIconsFromStates( void )
{
	int newFlags = 0;	
	
	if ( CSGameRules()->IsBombDefuseMap() )
	{
		if ( m_fBombAlpha > 0.0f )
		{
			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

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

				C_CSPlayer *pLocalOrObserver = NULL;
				if ( pLocalPlayer )
				{
					C_BaseEntity *target = pLocalPlayer->GetObserverTarget();
					if( target && target->IsPlayer() )
						pLocalOrObserver = ToCSPlayer( target );
					else
						pLocalOrObserver = pLocalPlayer;
				}
				
				if ( pLocalOrObserver )
				{
					if ( m_BombPosition.z > pLocalOrObserver->GetAbsOrigin().z + ABOVE_BELOW_HEIGHT )
						newFlags |= 1 << RI_BOMB_ICON_BOMB_ABOVE;
					else if ( m_BombPosition.z < pLocalOrObserver->GetAbsOrigin().z - ABOVE_BELOW_HEIGHT )
						newFlags |= 1 << RI_BOMB_ICON_BOMB_BELOW;
				}
			}
	
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
			float dist = newMapPosition.LengthSqr();
			if ( dist >= RADARRADIUS*RADARRADIUS && m_bRound )
			{
				newMapPosition *= sqrt( RADARRADIUS*RADARRADIUS/dist );
				mapPosition = m_RadarViewpointMap;
				mapPosition += newMapPosition;
			}

			ScaleformDisplayInfo displayInfo;
			displayInfo.SetAlpha( m_fBombAlpha * 100.0f );
			if ( cl_radar_fast_transforms.GetBool() )
			{
				Vector hudPos;
				RadarToHud( mapPosition, hudPos );
				displayInfo.SetX( hudPos.x );
				displayInfo.SetY( hudPos.y );
				displayInfo.SetRotation( 0 );
			}
			else
			{
				displayInfo.SetX( mapPosition.x );
				displayInfo.SetY( mapPosition.y );

				bool bRotate = m_bRound ? cl_radar_rotate.GetBool() : false;
				displayInfo.SetRotation( bRotate ? -m_RadarRotation : 0 );
			}

			assert( m_Icons[RI_BOMB_ICON_PACKAGE] );
			m_pScaleformUI->Value_SetDisplayInfo( m_Icons[RI_BOMB_ICON_PACKAGE], &displayInfo );

			SFHudTeamCounter* pTeamCounter = GET_HUDELEMENT( SFHudTeamCounter );
			if (pLocalPlayer && pTeamCounter && m_Icons[RI_BOMB_ICON_PACKAGE])
			{
				int playerIndex = GetPlayerIndexFromUserID( m_nBombHolderUserId ) + 1;
				int pidx = pTeamCounter->GetPlayerSlotIndex( playerIndex );

				int nColorID = 4;
				if (pLocalPlayer->ShouldShowTeamPlayerColors(pLocalPlayer->GetAssociatedTeamNumber()))
				{
					C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* )GameResources();

					if ( m_bBombDropped == false && ( pidx > -1 ) && pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST )
					{
						if ( pCSPR )
							nColorID = pCSPR->GetCompTeammateColor( playerIndex );
					}
					else
						nColorID = -1;
				}

				SFVALUE bomb = m_pScaleformUI->Value_GetMember(m_Icons[RI_BOMB_ICON_PACKAGE], "BombT");
				if (bomb)
				{
					SFVALUE bombinner = m_pScaleformUI->Value_GetMember(bomb, "bomb_inner");
					WITH_SFVALUEARRAY_SLOT_LOCKED(args, 2)
					{
						m_pScaleformUI->ValueArray_SetElement(args, 0, bombinner);
						m_pScaleformUI->ValueArray_SetElement(args, 1, nColorID);
						m_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "SetPlayerObjectColor", args, 2);
					}
					SafeReleaseSFVALUE(bombinner);
				}
				SafeReleaseSFVALUE(bomb);

			}
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

void SFHudRadar::SetVisibilityFlags( int newFlags )
{
	newFlags &= ( ( 1 << RI_NUM_ICONS )-1 );

	int diffFlags = m_iCurrentVisibilityFlags ^ newFlags;

	if ( diffFlags )
	{
		for ( int i = 0; i < RI_NUM_ICONS && ( diffFlags != 0 ); i++, diffFlags >>= 1 )
		{
			if ( diffFlags & 1 )
			{
				if ( m_Icons[i] )
					m_pScaleformUI->Value_SetVisible( m_Icons[i], ( newFlags & ( 1 << i ) ) != 0 );
			}
		}

		m_iCurrentVisibilityFlags = newFlags;
	}
}

void SFHudRadar::SetLocationText( wchar_t *newText )
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
			{
				m_LocationText->SetText( m_wcLocationString );
			}
		}
	}
}


/********************************************
 * "drawing" code.  Actually just sets position / alpha
 * of the flash objects
 */

void SFHudRadar::WorldToRadar( const Vector& ptin, Vector& ptout )
{
	float fWorldScale = m_fWorldToRadarScale;

	float flMapScaler = 0;
	if ( m_bRound && CSGameRules()->IsPlayingCoopMission() && cl_radar_always_centered.GetBool() )
		flMapScaler = MAX( 0 , (0.75f - fWorldScale) ) * 2.5;

	float fScale = m_bRound ? (cl_radar_scale.GetFloat() + flMapScaler) : ( m_fRadarSize / m_fMapSize ) ;

	fWorldScale *= fScale; 

	ptout.x = ( ptin.x - m_MapOrigin.x ) * fWorldScale;
	ptout.y = ( m_MapOrigin.y - ptin.y ) * fWorldScale;
	ptout.z = 0;
}

void SFHudRadar::RadarToHud( const Vector& ptin, Vector& ptout )
{
	VMatrix rot;
	MatrixBuildRotateZ( rot, m_RadarRotation );
	Vector3DMultiply( rot, ptin - m_RadarViewpointMap, ptout );
}

//ConVar radar_test( "radar_test", "0" );

void SFHudRadar::PositionRadarViewpoint( void )
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

	bool bRotate;
	float fScale;
	Vector vRadarOffset;
	float fFrustrumRotation;

	Vector vMapCenter = Vector( m_fMapSize / 2, m_fMapSize / 2, 0 );

	if ( m_bRound )
	{
		// [jbright] Using GetLocalOrigin addresses an issue which
		// causes the center player indicator to not stay centered
		// on the radar. This solution causes the radar to go jump
		// around while in observer mode, so in those cases rely on
		// MainViewOrigin.
		if ( pPlayer && m_iObserverMode == OBS_MODE_NONE )
		{
			m_RadarViewpointWorld = pPlayer->GetLocalOrigin();
		}
		else 
		{
			m_RadarViewpointWorld = MainViewOrigin( nSlot );
		}

		m_RadarViewpointWorld.z = 0;

		WorldToRadar( m_RadarViewpointWorld, m_RadarViewpointMap );

		float flMapScaler = 0;
		if ( m_bRound && CSGameRules()->IsPlayingCoopMission() && cl_radar_always_centered.GetBool() )
			flMapScaler = MAX( 0, ( 0.75f - m_fWorldToRadarScale ) ) * 2.5;

		fScale = cl_radar_scale.GetFloat() + flMapScaler;

		if ( !cl_radar_always_centered.GetBool() )
		{
			vRadarOffset = ( 1 - fScale ) * ( m_RadarViewpointMap - fScale * ( vMapCenter ) );

			m_RadarViewpointMap -=  vRadarOffset;
		}

		bRotate = cl_radar_rotate.GetBool();


	}
	else
	{
		bRotate = false;

		fScale = m_fRadarSize / m_fMapSize;

		m_RadarViewpointMap = fScale * vMapCenter;

		vRadarOffset = Vector( 0, 0, 0 );
	}

	if ( bRotate )
	{
		m_RadarRotation = MainViewAngles( nSlot )[YAW] - 90;
		fFrustrumRotation = 0;
	}
	else
	{
		m_RadarRotation = 0;
		fFrustrumRotation = MainViewAngles( nSlot )[YAW] - 90;
	}

	ScaleformDisplayInfo displayInfo;

	if ( m_MapRotation )
	{
		displayInfo.SetRotation( m_RadarRotation );

		m_pScaleformUI->Value_SetDisplayInfo( m_MapRotation, &displayInfo );

		if ( m_MapTranslation )
		{
			displayInfo.Clear();

			displayInfo.SetX( -m_RadarViewpointMap.x );
			displayInfo.SetY( -m_RadarViewpointMap.y );	

			double scale = 100.0f;

			scale *= fScale;

			displayInfo.SetXScale( scale );
			displayInfo.SetYScale( scale );			

			m_pScaleformUI->Value_SetDisplayInfo( m_MapTranslation, &displayInfo );
		}
	}

	static bool s_bResetTransform = false;
	if ( m_IconRotation )
	{
		displayInfo.Clear();

		if ( !cl_radar_fast_transforms.GetBool() )
		{
			displayInfo.SetRotation( m_RadarRotation );
			s_bResetTransform = true;
		}
		else if ( s_bResetTransform )
		{
			// Icons are updated individually (do not update parent)
			displayInfo.SetRotation( 0.0f );
		}

		m_pScaleformUI->Value_SetDisplayInfo( m_IconRotation, &displayInfo );

		if ( m_IconTranslation )
		{
			displayInfo.Clear();
					
			if ( !cl_radar_fast_transforms.GetBool() )
			{
				displayInfo.SetX( -m_RadarViewpointMap.x );
				displayInfo.SetY( -m_RadarViewpointMap.y );
				s_bResetTransform = true;
			}
			else if ( s_bResetTransform )
			{
				// Icons are updated individually (do not update parent)
				displayInfo.SetX( -0.4f );
				displayInfo.SetY( -0.4f );
			}

			m_pScaleformUI->Value_SetDisplayInfo( m_IconTranslation, &displayInfo );
		}

		if ( cl_radar_fast_transforms.GetBool() )
		{
			s_bResetTransform = false;
		}
	}

	if ( m_vecRadarVerticalSections.Count() )
	{
		float flPlayerZ = 0;
		if ( pPlayer && m_iObserverMode == OBS_MODE_NONE )
		{
			flPlayerZ = pPlayer->GetLocalOrigin().z;
		}
		else 
		{
			flPlayerZ = MainViewOrigin( nSlot ).z;
		}

		FOR_EACH_VEC( m_vecRadarVerticalSections, i )
		{
			HudRadarLevelVerticalSection_t *pRadarSection = &m_vecRadarVerticalSections[i];

			if ( flPlayerZ >= pRadarSection->m_flSectionAltitudeFloor && 
				 flPlayerZ < pRadarSection->m_flSectionAltitudeCeiling && 
				m_nCurrentRadarVerticalSection != pRadarSection->m_nSectionIndex )
			{
				m_nCurrentRadarVerticalSection = pRadarSection->m_nSectionIndex;

				FlashUpdateMapLayer(i);
				break;
			}
		}
	}

}

void SFHudRadar::PlaceGoalIcons( void )
{
	C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* )GameResources();
	if ( !pCSPR )
	{
		return;
	}

	SFHudRadarGoalIcon* pwalk = m_GoalIcons;
	Vector mapPosition;
	ScaleformDisplayInfo displayInfo;

	bool bRotate = m_bRound ? cl_radar_rotate.GetBool() : false;

	for ( int i = 0; i < m_iNumGoalIcons; i++ )
	{
		WorldToRadar( pwalk->m_Position, mapPosition );

		if ( pwalk->m_Icon )
		{
			Vector newMapPosition = mapPosition;
			newMapPosition -= m_RadarViewpointMap;
			float dist = newMapPosition.LengthSqr();
			if ( dist > RADARRADIUS*RADARRADIUS && m_bRound )
			{
				newMapPosition *= sqrt( RADARRADIUS*RADARRADIUS/dist );
				mapPosition = m_RadarViewpointMap;
				mapPosition += newMapPosition;
			}

			if ( cl_radar_fast_transforms.GetBool() )
			{
				Vector hudPos;
				RadarToHud(mapPosition, hudPos);

				displayInfo.SetX( hudPos.x );
				displayInfo.SetY( hudPos.y );

				displayInfo.SetRotation( 0.0f );
			}
			else
			{
				displayInfo.SetX( mapPosition.x );
				displayInfo.SetY( mapPosition.y );

				displayInfo.SetRotation( bRotate ? -m_RadarRotation : 0 );
			}
			

			m_pScaleformUI->Value_SetDisplayInfo( pwalk->m_Icon, &displayInfo );
		}
		pwalk++;
	}

}

void SFHudRadar::SetIconPackagePosition( SFHudRadarIconPackage* pPackage )
{
	if ( !m_bFlashReady )
		return;

	Vector mapPosition;
	float mapAngle;

	WorldToRadar( pPackage->m_Position, mapPosition );

	Vector newMapPosition = mapPosition;
	newMapPosition -= m_RadarViewpointMap;

	float dist = newMapPosition.LengthSqr();
	if ( dist >= RADARRADIUS*RADARRADIUS && m_bRound )
	{
		pPackage->SetIsOffMap( true );
		newMapPosition *= sqrt( RADARRADIUS*RADARRADIUS/dist );
		mapPosition = m_RadarViewpointMap;
		mapPosition += newMapPosition;
		mapAngle = 180.0f*atan2( newMapPosition.y, newMapPosition.x )/3.141592f + 90;
	}
	else
	{
		pPackage->SetIsOffMap( false );
		if ( pPackage->m_bIsDead || !pPackage->m_bIsSpotted || (pPackage->m_nAboveOrBelow != R_SAMELEVEL) || (pPackage->m_iPlayerType == PI_ENEMY && !pPackage->m_bIsSelected) )
		{
			mapAngle = -m_RadarRotation;
		}
		else
		{
			mapAngle = -pPackage->m_Angle[YAW]+90;
		}

	}

	double scale = 100;

	float fIconScale = cl_radar_scale.GetFloat();

	fIconScale = RemapValClamped( fIconScale, 
						0,	 1, 
						cl_radar_icon_scale_min.GetFloat() , 1 );

	scale *= fIconScale;

	static bool s_bResetTransform = false;
	if ( cl_radar_fast_transforms.GetBool() )
	{
		RadarToHud( mapPosition, pPackage->m_HudPosition );
		if ( pPackage->m_IconPackageRotate )
		{
			bool bRotate = m_bRound ? cl_radar_rotate.GetBool() : false;
			pPackage->m_HudRotation = bRotate ? ( m_RadarRotation + mapAngle ) : mapAngle;
		}
		else
		{
			pPackage->m_HudRotation = 0.0f;
		}
		pPackage->m_HudScale = scale;

		if ( s_bResetTransform )
		{
			ScaleformDisplayInfo displayInfo;
			displayInfo.SetXScale( 100.0f );
			displayInfo.SetYScale( 100.0f );
			displayInfo.SetX( 0.0f );
			displayInfo.SetY( 0.0f );

			m_pScaleformUI->Value_SetDisplayInfo( pPackage->m_IconPackage, &displayInfo );

			if ( pPackage->m_IconPackageRotate )
			{
				ScaleformDisplayInfo rotatedDisplayInfo;
				rotatedDisplayInfo.SetRotation( 0.0f );
				m_pScaleformUI->Value_SetDisplayInfo( pPackage->m_IconPackageRotate, &rotatedDisplayInfo );
			}

			s_bResetTransform = false;
		}
	}
	else
	{
		ScaleformDisplayInfo displayInfo;
		
		displayInfo.SetXScale( scale );
		displayInfo.SetYScale( scale );

		displayInfo.SetX( mapPosition.x );
		displayInfo.SetY( mapPosition.y );

		m_pScaleformUI->Value_SetDisplayInfo( pPackage->m_IconPackage, &displayInfo );

		if ( pPackage->m_IconPackageRotate )
		{
			ScaleformDisplayInfo rotatedDisplayInfo;
			rotatedDisplayInfo.SetRotation( mapAngle );
			m_pScaleformUI->Value_SetDisplayInfo( pPackage->m_IconPackageRotate, &rotatedDisplayInfo );
		}
		s_bResetTransform = true;
	}

	pPackage->SetupIconsFromStates();
}

void SFHudRadar::PlaceHostages( void )
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


	/* 	// Commented out because we want to show hostages on radar to Ts
	bool bIsTerrorist = ( pLocalPlayer->GetTeamNumber() == TEAM_TERRORIST );

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
*/	

	
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
			SFHudRadarIconPackage* pHostage =  NULL;

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

						if ( !pHostage->m_bHostageIsUsed )
							pHostage->m_bHostageIsUsed = true;
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
		SFHudRadarIconPackage* pHostage = GetRadarHostage( i );

		if ( pHostage->m_bIsActive )
		{
			C_BaseEntity * pEntity = ClientEntityList().GetBaseEntity( pHostage->m_iEntityID );

			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

			// note: !pHostage->m_bHostageIsUsed : T's can always see Hostages until they get +used by CTs
			bool bSpotted = (	!pHostage->m_bHostageIsUsed || 
								pLocalPlayer->GetAssociatedTeamNumber() == TEAM_CT ||
								( pEntity && pEntity->IsSpotted() ) ||
								m_EntitySpotted.Get( pHostage->m_iEntityID ) ||
								m_bShowAll );

			pHostage->SetIsSpotted( bSpotted );	

			int nIsAboveOrBelow = R_SAMELEVEL;
			C_CSPlayer *pLocalOrObserver = NULL;
			if ( pLocalPlayer )
			{
				C_BaseEntity *target = pLocalPlayer->GetObserverTarget();
				if( target && target->IsPlayer() )
					pLocalOrObserver = ToCSPlayer( target );
				else
					pLocalOrObserver = pLocalPlayer;
			}

			if ( pLocalOrObserver )
			{
				if ( pHostage->m_Position.z > pLocalOrObserver->GetAbsOrigin().z + ABOVE_BELOW_HEIGHT )
					nIsAboveOrBelow = R_ABOVE;
				else if ( pHostage->m_Position.z < pLocalOrObserver->GetAbsOrigin().z - ABOVE_BELOW_HEIGHT )
					nIsAboveOrBelow = R_BELOW;
			}

			pHostage->SetIsAboveOrBelow( nIsAboveOrBelow );

			SetIconPackagePosition( pHostage );

			bool bRotate = m_bRound ? cl_radar_rotate.GetBool() : false;
			// hostage always faces the same way
			if ( !bRotate )
				pHostage->m_Angle = QAngle( 0, 90, 0 );
			else
			{
				int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
				pHostage->m_Angle = MainViewAngles( nSlot );
			}

			if ( pEntity && !pEntity->IsDormant() && bSpotted )
			{
				Assert( dynamic_cast<CHostage*>( pEntity ) );
				pHostage->m_Position = pEntity->GetNetworkOrigin();
			}
		}
	}

	int hostageIndex = 0;

	for ( int i = 0; i < iNumDeadHostages; i++ )
	{
		m_HostageStatusIcons[hostageIndex++].SetStatus( SFHudRadarHostageIcons::HI_DEAD );
	}

	for ( int i = 0; i < iNumRescuedHostages; i++ )
	{
		m_HostageStatusIcons[hostageIndex++].SetStatus( SFHudRadarHostageIcons::HI_RESCUED );
	}

	for ( int i = 0; i < iNumMovingHostages; i++ )
	{
		m_HostageStatusIcons[hostageIndex++].SetStatus( SFHudRadarHostageIcons::HI_TRANSIT );
	}

	for ( int i = 0; i < iNumLiveHostages; i++ )
	{
		m_HostageStatusIcons[hostageIndex++].SetStatus( SFHudRadarHostageIcons::HI_ALIVE );
	}

	for ( ; hostageIndex < MAX_HOSTAGES && m_HostageStatusIcons[hostageIndex].m_IconPackage != NULL; hostageIndex++ )
	{
		m_HostageStatusIcons[hostageIndex].SetStatus( SFHudRadarHostageIcons::HI_UNUSED );
	}
}

void SFHudRadar::UpdateAllDefusers( void )
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
		SFHudRadarIconPackage * pPackage = GetRadarDefuser( i );

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





void SFHudRadar::PlacePlayers( void )
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
		return;

	int localTeamNumber = pLocalPlayer->GetAssociatedTeamNumber();

	SFHudRadarIconPackage* pwalk = m_Players;
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

			if ( pPlayer && pCSPR->IsConnected( playerIndex ) )
			{
				pwalk->SetIsBot( pCSPR->IsFakePlayer( playerIndex ) );

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
					bool bIsEnemy = pPlayer->IsOtherEnemy( pLocalPlayer ) && localPlayerIndex != playerIndex;

					bool bHasValidPosition = playerIsActive || ( pwalk->m_Position.x != 0 || pwalk->m_Position.y != 0 );

					bool bPlayerSpotted = ( m_EntitySpotted.Get( playerIndex ) || ( playerIsActive && pPlayer->IsSpotted() ) );
					bool bSpottedByFriends = !bIsEnemy;

					// verify here on the client that we do in fact see the entity how the player would expect to see the entity
					// if the local player is in smoke or if they are looking through smoke, we double check if they can see them on radar
					// use the networked position and not the stored position because the stored one is stale
					Vector vecOtherPos( pPlayer->GetNetworkOrigin( ).x, pPlayer->GetNetworkOrigin( ).y, ( pPlayer->GetNetworkOrigin( ).z + 56 ) );
					if ( bIsEnemy && bPlayerSpotted )
					{
						bSpottedByFriends = pPlayer->IsSpottedByFriends( localPlayerIndex );

						float flDist = ( pLocalPlayer->EyePosition( ) - vecOtherPos ).Length2D( );
						if ( flDist < 1600 && bSpottedByFriends == false &&
							 LineGoesThroughSmoke( pLocalPlayer->EyePosition(), vecOtherPos, 1.0f ) )
						{
							bPlayerSpotted = false;
						}				
					}					

					if ( bHasValidPosition && ( (bSameTeam && !bIsEnemy)  || m_bShowAll || bPlayerSpotted ) )
					{
						bool bSpottedByLocalPlayer = bSameTeam ? true : pPlayer->IsSpottedBy( localPlayerIndex );
						pwalk->SetIsSpotted( true );
						pwalk->SetIsSpottedByFriendsOnly( (bSpottedByFriends && !bSpottedByLocalPlayer) );

						if ( playerIsActive && pPlayer->HasDefuser() )
						{
							// attach a defuser icon to this player if one does not already exist
							if ( GetDefuseIndexFromEntityID( playerIndex ) == INVALID_INDEX ) 
							{
								CreateDefuser( playerIndex );
							}
						}

						if ( playerIsActive && pPlayer->HasC4() )
						{
							m_bBombIsSpotted = true;
							m_BombPosition = pwalk->m_Position;
							m_nBombHolderUserId = pPlayer->GetUserID();
						}

						// set is selected
						C_CSPlayer *pLocalOrObserver = ToCSPlayer( pLocalPlayer->GetObserverTarget() );
						pwalk->SetIsSelected( pLocalOrObserver && pPlayer == pLocalOrObserver );
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

				UpdatePlayerNumber( pwalk );

				C_CSPlayer *pLocalOrObserver = NULL;
				if ( pLocalPlayer )
				{
					C_BaseEntity *target = pLocalPlayer->GetObserverTarget();
					if( target && target->IsPlayer() )
						pLocalOrObserver = ToCSPlayer( target );
					else
						pLocalOrObserver = pLocalPlayer;
				}

				if ( pLocalOrObserver )
				{
					if ( pwalk->m_Position.z > pLocalOrObserver->GetAbsOrigin().z + ABOVE_BELOW_HEIGHT )
						nIsAboveOrBelow = R_ABOVE;
					else if ( pwalk->m_Position.z < pLocalOrObserver->GetAbsOrigin().z - ABOVE_BELOW_HEIGHT )
						nIsAboveOrBelow = R_BELOW;
				}

				pwalk->SetIsAboveOrBelow( nIsAboveOrBelow );

			}
			else
			{
				pwalk->SetIsSpotted( false );
				pwalk->SetIsSpeaking( false );
			}

			if ( playerIsActive && pwalk->m_bIsSpotted )
			{
				// when the local player dies the system reports the local players position
				// as the position of the spectator camera.  If we let that happen, the player's
				// X migrates from the point they died to follow the camera.  Can't have that,
				// so avoid setting the position if we're dead and in observer mode
				
				if ( m_iObserverMode == OBS_MODE_NONE || i != localPlayerIndex )
				{
					 // dkorus and pfreese:  Changed from local origin and local angles to last networked version to address jitter issues for pax demo 2011
					if ( !pwalk->m_bIsDead )
						pwalk->m_Position = pPlayer->GetNetworkOrigin(); 

					// only set the angle of the icon if the player isn't above or below us
					if ( nIsAboveOrBelow != R_SAMELEVEL || pwalk->IsHostageType( ) || ( pwalk->m_iPlayerType == PI_ENEMY && !pwalk->m_bIsSelected ) )
					{
						int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
						pwalk->m_Angle = MainViewAngles( nSlot );
					}
					else
						pwalk->m_Angle = pPlayer->EyeAngles( );

					if ( pPlayer->HasC4() )
					{
						m_BombPosition = pwalk->m_Position;
						m_nBombHolderUserId = pPlayer->GetUserID();
					}
				}
			}

			SetIconPackagePosition( pwalk );

		}
	}



}

void SFHudRadar::UpdateMiscIcons( void )
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

	// show the bomb under the radar if the player has this set
	static ConVarRef cl_hud_bomb_under_radar( "cl_hud_bomb_under_radar" );
	bool bShowBombUnderRadar = ( cl_hud_bomb_under_radar.GetInt( ) == 1 && pLocalPlayer->HasC4() );
	WITH_SFVALUEARRAY( args, 1 )
	{
		g_pScaleformUI->ValueArray_SetElement( args, 0, bShowBombUnderRadar );
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowC4", args, 1 );
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
					bool bLocalPlayerCanSeeBomb =( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST || m_bShowAll || ( pC4 && !pC4->IsDormant() && pC4->IsSpotted() ) );
					if ( bLocalPlayerCanSeeBomb && pC4 && !pC4->IsDormant() )
					{
						m_bBombIsSpotted = true;
						m_BombPosition = pC4->GetNetworkOrigin();
						m_nBombHolderUserId = -1;
					}
				}
			}
		}
		if ( m_nBombEntIndex != -1 && ( !m_bBombIsSpotted || m_bBombDropped ) )
		{
			C_BaseEntity * pEntity = ClientEntityList().GetBaseEntity( m_nBombEntIndex );

			bool bLocalPlayerCanSeeBomb =( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST || m_bShowAll || ( pEntity && !pEntity->IsDormant() && pEntity->IsSpotted() ) );
			if ( bLocalPlayerCanSeeBomb && pEntity && !pEntity->IsDormant() )
			{
				Assert( pEntity && FClassnameIs( pEntity, "weapon_c4" ) );
				m_bBombIsSpotted = true;
				m_BombPosition = pEntity->GetNetworkOrigin();
				m_nBombHolderUserId = -1;
			}
		}

		float now = gpGlobals->curtime;
		bool drawBomb = false;

		bool bBombPositionIsValid = ( m_BombPosition.x != 0 || m_BombPosition.y != 0 );

		if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() )
		{
			drawBomb = false;
		}
		else if ( bBombPositionIsValid && !m_bBombExploded && m_bBombIsSpotted )
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

		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowHostages", NULL, 0 );
		}
	}

}

void SFHudRadar::ApplySpectatorModes( void )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pLocalPlayer )
		return;
	
	m_bShowAll = false;
	m_iObserverMode = pLocalPlayer->GetObserverMode();

	if ( engine->IsHLTV() || pLocalPlayer->IsSpectator() )
	{
		m_bShowAll = true;
	}
	else if ( m_iObserverMode != OBS_MODE_NONE ) // is not a live player
	{
		// respect mp_forcecamera
		static ConVarRef mp_forcecamera( "mp_forcecamera" );
		m_bShowAll = ( mp_forcecamera.GetInt() == OBS_ALLOW_ALL );
	}

	// mp_radar_showall trumps all
	static ConVarRef mp_radar_showall( "mp_radar_showall" );
	if ( mp_radar_showall.GetInt() && (
		( pLocalPlayer->GetAssociatedTeamNumber() == mp_radar_showall.GetInt() ) ||
		( mp_radar_showall.GetInt() == 1 )
		) )
	{
		m_bShowAll = true;
	}

	// Radar type
	bool bRoundRadar = true;
	IViewPortPanel* panel = GetViewPortInterface()->FindPanelByName( PANEL_SCOREBOARD );
	if ( engine->IsHLTV() || pLocalPlayer->IsSpectator() || (panel->IsVisible() && cl_radar_square_with_scoreboard.GetBool()) )
		bRoundRadar = false;
	else if ( pLocalPlayer->IsAlive() == false && 
		(m_iObserverMode == OBS_MODE_FIXED ||
		m_iObserverMode == OBS_MODE_CHASE ||
		m_iObserverMode == OBS_MODE_ROAMING ||
		m_iObserverMode == OBS_MODE_IN_EYE) )
	{
		bRoundRadar = false;
	}
	SwitchRadarToRound( bRoundRadar );
}

void SFHudRadar::UpdateDecoys( void )
{
	for ( int i = 0; i <= m_iLastDecoyIndex; i++ )
	{
		SFHudRadarIconPackage* pPackage = GetRadarDecoy( i );

		if ( pPackage && pPackage->m_bIsActive )
		{
			SetIconPackagePosition( pPackage );
			if ( !pPackage->IsVisible() )
			{
				RemoveDecoy( i );
			}
		}
	}
}

void SFHudRadar::ResizeHud( void )
{
	if ( !m_pScaleformUI )
		return;

	WITH_SLOT_LOCKED
	{
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ForceResizeHud", NULL, 0 );
	}
}

void SFHudRadar::ProcessInput( void )
{
	LazyCreateGoalIcons();

	LazyCreatePlayerIcons();

	ApplySpectatorModes();
	
	PositionRadarViewpoint();

	PlaceGoalIcons();
	
	PlacePlayers();

	PlaceHostages();

	UpdateDecoys();

	UpdateAllDefusers();

	UpdateMiscIcons();

	SetupIconsFromStates();
}

void SFHudRadar::SwitchRadarToRound( bool toRound )
{
	if ( !m_bActive || !m_bFlashReady )
		return;

	WITH_SLOT_LOCKED
	{
		if ( m_MapRotation )
		{
			SafeReleaseSFVALUE( m_MapRotation );
		}
		if ( m_MapTranslation )
		{
			SafeReleaseSFVALUE( m_MapTranslation );
		}
		
		if ( toRound )
		{
			m_bRound = true;

			m_MapRotation = m_pScaleformUI->Value_GetMember( m_Radar, "MapRotation" );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SwitchRadarToRound", NULL, 0 );
		}
		else
		{
			m_bRound = false;

			m_MapRotation = m_pScaleformUI->Value_GetMember( m_Radar, "MapRotationSq" );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SwitchRadarToSquare", NULL, 0 );
		}

		if ( m_MapRotation )
		{
			m_MapTranslation = m_pScaleformUI->Value_GetMember( m_MapRotation, "MapTranslation" );
		}
	}
}

