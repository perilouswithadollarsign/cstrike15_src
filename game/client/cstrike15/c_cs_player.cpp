//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "c_cs_player.h"
#include "c_user_message_register.h"
#include "view.h"
#include "iclientvehicle.h"
#include "ivieweffects.h"
#include "input.h"
#include "IEffects.h"
#include "fx.h"
#include "c_basetempentity.h"
#include "hud_macros.h"	//HOOK_COMMAND
#include "engine/ivdebugoverlay.h"
#include "smoke_fog_overlay.h"
#include "bone_setup.h"
#include "in_buttons.h"
#include "r_efx.h"
#include "dlight.h"
#include "shake.h"
#include "cl_animevent.h"
#include "c_physicsprop.h"
#include "props_shared.h"
#include "obstacle_pushaway.h"
#include "death_pose.h"
#include "voice_status.h"
#include "interpolatortypes.h"
#include "smokegrenade_projectile.h"

#include "effect_dispatch_data.h"	//for water ripple / splash effect
#include "c_te_effect_dispatch.h"	//ditto
#include "c_te_legacytempents.h"
#include "cs_gamerules.h"
#include "fx_cs_blood.h"
#include "c_cs_playerresource.h"
#include "c_team.h"
#include "flashlighteffect.h"
#include "c_cs_hostage.h"
#include "prediction.h"

#include "HUD/sfweaponselection.h"
#include "HUD/sfhudreticle.h"
#include "HUD/sfweaponselection.h"
#include "ragdoll_shared.h"
#include "collisionutils.h"
#include "engineinterface.h"
#include "econ_gcmessages.h"
#include "cstrike15_item_system.h"
#include "hltvcamera.h"

#include "steam/steam_api.h"

#include "vguicenterprint.h"
#include "ixboxsystem.h"
#include "xlast_csgo/csgo.spa.h"

#include "weapon_basecsgrenade.h"

#include <vgui/IInput.h>
#include "vgui_controls/Controls.h"

#include "cs_player_rank_mgr.h"
#include "platforminputdevice.h"
#include "cam_thirdperson.h"
#include "inputsystem/iinputsystem.h"
#include <localize/ilocalize.h>
#include "interfaces/interfaces.h"

#include "gametypes.h"
#include "GameStats.h"
#include "c_cs_team.h"

#include "Scaleform/HUD/sfhudinfopanel.h"

#include <engine/IEngineSound.h>

#include "cs_shareddefs.h"

#include "hltvreplaysystem.h"

#include "cs_custom_material_swap.h"
#include "materialsystem/icustommaterial.h"

// Comment this back in if you want the cl_minmodels convar to operate as normal.
#define CS_ALLOW_CL_MINMODELS 0



#if defined( CCSPlayer )
	#undef CCSPlayer
#endif

#include "materialsystem/imesh.h"		//for materials->FindMaterial
#include "materialsystem/imaterialvar.h"
#include "iviewrender.h"				//for view->
#include "view_shared.h"				//for CViewSetup

#include "iviewrender_beams.h"			// flashlight beam

#include "materialsystem/icustommaterialmanager.h"
#include "materialsystem/icompositetexturegenerator.h"
#include "cs_custom_clothing_visualsdata_processor.h"
#include "cs_custom_epidermis_visualsdata_processor.h"
#include "model_combiner.h"

#include "physpropclientside.h"			// for dropping physics mags

#include "cstrike15_gcmessages.pb.h"
#include "csgo_playeranimstate.h"

#include "c_props.h"
#include "model_types.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static Vector WALL_MIN(-WALL_OFFSET,-WALL_OFFSET,-WALL_OFFSET );
static Vector WALL_MAX(WALL_OFFSET,WALL_OFFSET,WALL_OFFSET );

extern ConVar econ_use_cosmetics;

extern ConVar	spec_freeze_time;
extern ConVar	spec_freeze_traveltime;
extern ConVar	spec_freeze_traveltime_long;
extern ConVar	spec_freeze_distance_min;
extern ConVar	spec_freeze_distance_max;
extern ConVar	spec_freeze_target_fov;
extern ConVar	spec_freeze_target_fov_long;
extern ConVar	spec_freeze_deathanim_time;
extern ConVar	cl_clanid;
extern ConVar	mp_teammates_are_enemies;
extern ConVar	mp_use_respawn_waves;
extern ConVar	mp_respawn_on_death_ct;
extern ConVar   mp_respawn_on_death_t;
extern ConVar	sv_disable_immunity_alpha;
extern ConVar	cl_spec_use_tournament_content_standards;
extern ConVar	sv_spec_use_tournament_content_standards;

ConVar cl_foot_contact_shadows( "cl_foot_contact_shadows", "1", FCVAR_CLIENTDLL | FCVAR_RELEASE, "" );

ConVar spec_freeze_cinematiclight_r( "spec_freeze_cinematiclight_r", "1.5", FCVAR_CHEAT );
ConVar spec_freeze_cinematiclight_g( "spec_freeze_cinematiclight_g", "1.2", FCVAR_CHEAT  );
ConVar spec_freeze_cinematiclight_b( "spec_freeze_cinematiclight_b", "1.0", FCVAR_CHEAT  );
ConVar spec_freeze_cinematiclight_scale( "spec_freeze_cinematiclight_scale", "2.0", FCVAR_CHEAT  );

ConVar spec_glow_silent_factor( "spec_glow_silent_factor", "0.6", FCVAR_CLIENTDLL | FCVAR_RELEASE, "Lurking player xray glow scaling.", true, 0.0f, true, 1.0f );
ConVar spec_glow_spike_factor( "spec_glow_spike_factor", "1.2", FCVAR_CLIENTDLL | FCVAR_RELEASE, "Noisy player xray glow scaling (pop when noise is made).  Make >1 to add a 'spike' to noise-making players", true, 1.0f, true, 3.0f );

ConVar spec_glow_full_time( "spec_glow_full_time", "1.0", FCVAR_CLIENTDLL | FCVAR_RELEASE, "Noisy players stay at full brightness for this long.", true, 0.0f, false, 0.0f );
ConVar spec_glow_decay_time( "spec_glow_decay_time", "2.0", FCVAR_CLIENTDLL | FCVAR_RELEASE, "Time to decay glow from 1.0 to spec_glow_silent_factor after spec_glow_full_time.", true, 0.0f, false, 0.0f );
ConVar spec_glow_spike_time( "spec_glow_spike_time", "0.0", FCVAR_CLIENTDLL | FCVAR_RELEASE, "Time for noisy player glow 'spike' to show that they made noise very recently.", true, 0.0f, false, 0.0f );

ConVar cl_crosshair_sniper_width( "cl_crosshair_sniper_width", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE | FCVAR_SS, "If >1 sniper scope cross lines gain extra width (1 for single-pixel hairline)" );

void BorrowMusicKitChangeCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
}

ConVar cl_borrow_music_from_player_index( "cl_borrow_music_from_player_index", "0", FCVAR_CLIENTDLL,
	"", false, false, false, false, BorrowMusicKitChangeCallback );


static void Spec_Show_Xray_Callback( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer )
		pLocalPlayer->UpdateGlowsForAllPlayers();
}
ConVar spec_show_xray("spec_show_xray", "0", FCVAR_ARCHIVE | FCVAR_RELEASE, "If set to 1, you can see player outlines and name IDs through walls - who you can see depends on your team and mode", Spec_Show_Xray_Callback );

ConVar cl_ragdoll_physics_enable( "cl_ragdoll_physics_enable", "1", 0, "Enable/disable ragdoll physics." );

ConVar cl_minmodels( "cl_minmodels", "0", 0, "Uses one player model for each team.  Set this value to -1 to allow unapproved / in progress player models to be used." );
ConVar cl_min_ct( "cl_min_ct", "1", 0, "Controls which CT model is used when cl_minmodels is set to 1." );
ConVar cl_min_t( "cl_min_t", "1", 0, "Controls which Terrorist model is used when cl_minmodels is set to 1." );

// [jason] Adjusts the safe extents of the camera placement for the freeze cam to prevent it from penetrating the killer's geometry
ConVar cl_freeze_cam_penetration_tolerance( "cl_freeze_cam_penetration_tolerance", "0", 0, "If the freeze cam gets closer to target than this distance, we snap to death cam instead (0 = use character bounds instead, -1 = disable this safety check" );

ConVar cl_ragdoll_crumple( "cl_ragdoll_crumple", "1" );

ConVar cl_teamid_overhead( "cl_teamid_overhead", "1", FCVAR_CHEAT | FCVAR_SS, "Shows teamID over player's heads.  0 = off, 1 = on" );
ConVar cl_teamid_overhead_maxdist( "cl_teamid_overhead_maxdist", "3000", FCVAR_CHEAT | FCVAR_SS, "max distance at which the overhead team id icons will show" );
ConVar cl_teamid_overhead_maxdist_spec( "cl_teamid_overhead_maxdist_spec", "2000", FCVAR_CHEAT | FCVAR_SS, "max distance at which the overhead team id icons will show when a spectator" );
ConVar cl_show_equipment_value( "cl_show_equipment_value", "0" );

ConVar cl_show_clan_in_death_notice("cl_show_clan_in_death_notice", "1", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_ARCHIVE, "Is set, the clan name will show next to player names in the death notices.");

//ConVar cl_violent_ragdolls( "cl_violent_ragdolls", "1", FCVAR_RELEASE, "Allows ragdolls to bleed out and react to gun shots.");
#define USE_VIOLENT_RAGDOLLS 0

ConVar cl_dm_buyrandomweapons( "cl_dm_buyrandomweapons", "1", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_ARCHIVE, "Player will automatically receive a random weapon on spawn in deathmatch if this is set to 1 (otherwise, they will receive the last weapon)" );

ConVar cl_teammate_colors_show( "cl_teammate_colors_show", "1", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_ARCHIVE, "In competitive, 1 = show teammates as separate colors in the radar, scoreboard, etc., 2 = show colors and letters" );
ConVar cl_hud_playercount_pos( "cl_hud_playercount_pos", "0", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_ARCHIVE, "0 = default (top), 1 = bottom" );
ConVar cl_hud_playercount_showcount( "cl_hud_playercount_showcount", "0", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_ARCHIVE, "0 = show player avatars (default), 1 = just show count number (no avatars)" );
ConVar cl_hud_color( "cl_hud_color", "0", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_ARCHIVE, "0 = default, 1 = light blue, 2 = orange, 3 = green, 4 = purple, 5 = white." );
static void Hud_Radar_Scale_Callback( IConVar *pConVar, const char *pOldString, float flOldValue )
{
	// refresh here
	ConVarRef m_hudscaling( "hud_scaling" );
	float flScale = m_hudscaling.GetFloat();
	m_hudscaling.SetValue( flScale + 1 );
	m_hudscaling.SetValue( flScale );
}

ConVar cl_hud_radar_scale( "cl_hud_radar_scale", "1", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_ARCHIVE, "", true, 0.8, true, 1.3, Hud_Radar_Scale_Callback );
ConVar cl_hud_bomb_under_radar( "cl_hud_bomb_under_radar", "1", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_ARCHIVE, "" );
ConVar cl_hud_background_alpha( "cl_hud_background_alpha", "0.5", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_ARCHIVE, "", true, 0.0f, true, 1.0f );
ConVar cl_hud_healthammo_style( "cl_hud_healthammo_style", "0", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_ARCHIVE, "" );

ConVar cl_spec_follow_grenade_key( "cl_spec_follow_grenade_key", "0", FCVAR_CLIENTDLL | FCVAR_RELEASE | FCVAR_ARCHIVE, "0 = LALT, 1 = LSHIFT, 2 = +reload" );


ConVar cl_freezecameffects_showholiday( "cl_freezecameffects_showholiday", "0", FCVAR_CLIENTDLL | FCVAR_RELEASE, "Happy holidays from the CS:GO team and Valve!" );

const float CycleLatchTolerance = 0.15;	// amount we can diverge from the server's cycle before we're corrected

extern ConVar mp_playerid_delay;
extern ConVar mp_playerid_hold;

ConVar cl_camera_height_restriction_debug( "cl_camera_height_restriction_debug", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "" );

//ConVar  *sv_cheats = NULL;

ConVar fov_cs_debug( "fov_cs_debug", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "Sets the view fov if cheats are on." );

#define FREEZECAM_LONGCAM_DIST	320  // over this amount, the camera will zoom close on target

#define sv_magazine_drop_physics 1
#define sv_magazine_drop_time 15
#define sv_magazine_drop_debug 0

//ConVar sv_magazine_drop_physics( "sv_magazine_drop_physics", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "Players drop physical weapon magazines when reloading." );
//ConVar sv_magazine_drop_time( "sv_magazine_drop_time", "15", FCVAR_REPLICATED | FCVAR_RELEASE, "Duration physical magazines stay in the world.", true, 2.0f, true, 20.0f );
//ConVar sv_magazine_drop_debug( "sv_magazine_drop_debug", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Show a debug marker at mag spawn position." );


class CAddonInfo
{
public:
	const char *m_pAttachmentName;
	const char *m_pWeaponClassName;	// The addon uses the w_ model from this weapon.
	const char *m_pModelName;		//If this is present, will use this model instead of looking up the weapon 
	const char *m_pHolsterName;
};


// These must follow the ADDON_ ordering.
CAddonInfo g_AddonInfo[] = 
{
	{ "grenade0",	"weapon_flashbang",		0, 0 },
	{ "grenade1",	"weapon_flashbang",		0, 0 },
	{ "grenade2",	"weapon_hegrenade",		0, 0 },
	{ "grenade3",	"weapon_smokegrenade",	0, 0 },
	{ "c4",			"weapon_c4",			0, 0 },
	{ "defusekit",	0,						"models/weapons/w_defuser.mdl", 0 },
	{ "primary",	0,						0, 0 },	// Primary addon model is looked up based on m_iPrimaryAddon
	{ "pistol",		0,						0, 0 },	// Pistol addon model is looked up based on m_iSecondaryAddon
	{ "eholster",	0,						"models/weapons/w_eq_eholster_elite.mdl", "models/weapons/w_eq_eholster.mdl" },
	{ "grenade4",	"weapon_decoy",			0, 0 },
	{ "knife",		"weapon_knife",			0, 0 },
	{ "facemask",	0,						"models/player/holiday/facemasks/facemask_skull.mdl", 0 },
	{ "grenade4",	"weapon_tagrenade",	0, 0 },
};

CAddonInfo g_ClientSideAddons[] =
{
	{ "forward",	0,						"models/player/holiday/santahat.mdl", 0 },
	{ "forward",	0,						"models/ghost/ghost.mdl", 0 },
	{ "facemask",	0,						"models/player/holiday/facemasks/facemask_battlemask.mdl", 0 },
};

#define SMOKEGRENADE_LIFETIME 17.5f

CUtlVector<EHANDLE> g_SmokeGrenadeHandles;
CUtlVector<clientSmokeGrenadeRecord_t> g_SmokeGrenades;

void AddSmokeGrenade( Vector location, int iEntityId )
{
	int nIndex = g_SmokeGrenades.AddToTail();
	g_SmokeGrenades[nIndex].m_flInceptionTime = gpGlobals->curtime;
	g_SmokeGrenades[nIndex].m_vecPosition = location;
	g_SmokeGrenades[nIndex].m_iEntityId = iEntityId;
}

void AddSmokeGrenadeHandle( EHANDLE hGrenade )
{
	for ( int i = 0; i < g_SmokeGrenadeHandles.Count(); i++ )
	{
		if ( g_SmokeGrenadeHandles[i] && g_SmokeGrenadeHandles[i] == hGrenade )
		{
			return;
		}
	}

	g_SmokeGrenadeHandles.AddToTail( hGrenade );
}

void RemoveSmokeGrenade( Vector location, int iEntityId )
{
	for ( int i = 0; i < g_SmokeGrenades.Count(); i++ )
	{
		if ( g_SmokeGrenades[i].m_iEntityId == iEntityId && g_SmokeGrenades[i].m_vecPosition.DistToSqr( location ) < 0.1f )
		{
			g_SmokeGrenades.FastRemove( i );
			break;
		}
	}
}

void RemoveSmokeGrenadeHandle( EHANDLE hGrenade )
{
	for ( int i = 0; i < g_SmokeGrenadeHandles.Count(); i++ )
	{
		if ( g_SmokeGrenadeHandles[i] && g_SmokeGrenadeHandles[i] == hGrenade )
		{
			g_SmokeGrenadeHandles.FastRemove( i );
			break;
		}
	}
}

bool LineGoesThroughSmoke( Vector from, Vector to, bool grenadeBloat )
{
	float totalSmokedLength = 0.0f;	// distance along line of sight covered by smoke

	// compute unit vector and length of line of sight segment
	//Vector sightDir = to - from;
	//float sightLength = sightDir.NormalizeInPlace();

	const float smokeRadiusSq = SmokeGrenadeRadius * SmokeGrenadeRadius * grenadeBloat * grenadeBloat;
	for( int it=0; it < g_SmokeGrenadeHandles.Count(); it++ )
	{
		float flLengthAdd = 0;
		if ( CSGameRules() )
		{
			C_SmokeGrenadeProjectile *pGrenade = static_cast<C_SmokeGrenadeProjectile*>(g_SmokeGrenadeHandles[it].Get());
			if ( pGrenade && pGrenade->m_bDidSmokeEffect == true )
			{
				flLengthAdd = CSGameRules()->CheckTotalSmokedLength( smokeRadiusSq, pGrenade->GetAbsOrigin(), from, to );
				// get the totalSmokedLength and check to see if the line starts or stops in smoke.  If it does this will return -1 and we should just bail early
				if ( flLengthAdd == -1 )
					return true;

				totalSmokedLength += flLengthAdd;
			}
		}
	}

	// define how much smoke a bot can see thru
	const float maxSmokedLength = 0.7f * SmokeGrenadeRadius;

	// return true if the total length of smoke-covered line-of-sight is too much
	return (totalSmokedLength > maxSmokedLength);
}

void RemoveAllSmokeGrenades( void )
{
	g_SmokeGrenadeHandles.RemoveAll();
}

void TruncatePlayerName( wchar_t *pCleanedHTMLName, int destLen, int nTruncateAt, bool bSkipHTML )
{
    // this code expects the input string to be clean HTML (call MakeStringSafe on it first)

    // This code is smart enough to find HTML style escapements in the strings.
    // anything between a & and a ; is treated as one character. &lt; for example.
    int charsLeft = nTruncateAt+1;

    int bufferPosition = 0;

    bool done = false;
    bool insideEscapement = false;

    while( !done )
    {
        if ( pCleanedHTMLName[bufferPosition] == 0 || charsLeft == 0 )
        {
            done = true;
        }
        else 
        {
			if ( !bSkipHTML )
			{
				if ( insideEscapement )
				{
					if ( pCleanedHTMLName[bufferPosition] == L';' )
					{
						insideEscapement = false;
						charsLeft--;
					}
				}
				else if ( pCleanedHTMLName[bufferPosition] == L'&' )
				{
					insideEscapement = true;
				}
				else
				{
					charsLeft--;
				}
			}

            bufferPosition++;
        }
    }

    if ( charsLeft == 0 )
    {
        if ( ( bufferPosition + 4 ) < destLen )
        {
            V_wcsncpy( pCleanedHTMLName + bufferPosition, L"...", 4 * sizeof( wchar_t ) );
        }
	    else
	    {
		    AssertMsg( false, "Destination buffer of insufficient size." );
	    }
    }

}

#if CS_ALLOW_CL_MINMODELS

// Return the first class id that has a legitimate model based on cl_minmodels.
static int FilterModelUsingCL_MinModels( int iClass )
{

	AssertMsg(false, "This needs to be updated to use the PlayerModelInfo class" );
	return 0;

/*
	int rModel = 0;
	int team = GetTeamFromClass( iClass );

	if ( cl_minmodels.GetInt() == 1 )
	{
		if ( team == TEAM_TERRORIST )
		{
			int index = cl_min_t.GetInt() - 1;
			if ( index < 0 || index >= TerroristPlayerModels.Count() )
			{
				index = 0;
			}
			rModel = modelinfo->GetModelIndex(TerroristPlayerModels[index] );
		}
		else
		{
			int index = cl_min_ct.GetInt() - 1;
			if ( index < 0 || index >= CTPlayerModels.Count() )
			{
				index = 0;
			}
			rModel = modelinfo->GetModelIndex( CTPlayerModels[index] );
		}
	}	
	else if ( cl_minmodels.GetInt() == 0 && !s_approvedModels[iClass] )
	{
		// Only pull players from the approved list.
		if ( team == TEAM_TERRORIST )
		{
			// Pick the defalut T model.
			int bestClass = FindFirstUsableModel( FIRST_T_CLASS, LAST_T_CLASS+1,  CS_CLASS_PROFESSIONAL_MALE );
			rModel = modelinfo->GetModelIndex( TerroristPlayerModels[ bestClass - FIRST_T_CLASS ] );
		}
		else
		{
			// Pick the defalut CT model.
			int bestClass = FindFirstUsableModel( FIRST_CT_CLASS, LAST_CT_CLASS+1,  CS_CLASS_ST6_MALE );
			rModel = modelinfo->GetModelIndex( CTPlayerModels[ bestClass - FIRST_CT_CLASS ] );
		}

	}
	else
	{
		// Make sure we restore the proper model based on class.
		if ( team == TEAM_TERRORIST )
		{
			rModel = modelinfo->GetModelIndex( TerroristPlayerModels[ iClass - FIRST_T_CLASS ] );
		}
		else
		{
			rModel = modelinfo->GetModelIndex( CTPlayerModels[ iClass - FIRST_CT_CLASS ] );
		}
	}
	return rModel;
*/
}

#endif // CS_ALLOW_CL_MINMODELS

// -------------------------------------------------------------------------------- //
// Player animation event. Sent to the client when a player fires, jumps, reloads, etc..
// -------------------------------------------------------------------------------- //

class C_TEPlayerAnimEvent : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlayerAnimEvent, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void PostDataUpdate( DataUpdateType_t updateType )
	{
		// Create the effect.
		C_CSPlayer *pPlayer = ToCSPlayer( m_hPlayer.Get() );
		if ( pPlayer && !pPlayer->IsDormant() )
		{
			pPlayer->DoAnimationEvent( (PlayerAnimEvent_t )m_iEvent.Get(), m_nData );
		}
	}

public:
	CNetworkHandle( CBasePlayer, m_hPlayer );
	CNetworkVar( int, m_iEvent );
	CNetworkVar( int, m_nData );
};

IMPLEMENT_CLIENTCLASS_EVENT( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent, CTEPlayerAnimEvent );

BEGIN_RECV_TABLE_NOBASE( C_TEPlayerAnimEvent, DT_TEPlayerAnimEvent )
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
	RecvPropInt( RECVINFO( m_iEvent ) ),
	RecvPropInt( RECVINFO( m_nData ) )
END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_CSPlayer )
#ifdef CS_SHIELD_ENABLED
	DEFINE_PRED_FIELD( m_bShieldDrawn, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
#endif
	DEFINE_PRED_FIELD_TOL( m_flStamina, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, 0.1f ),
	DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_PRIVATE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_iShotsFired, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),   
	DEFINE_PRED_FIELD( m_iDirection, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),   
	DEFINE_PRED_FIELD( m_bIsScoped, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bIsWalking, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bResumeZoom, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nNumFastDucks, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),   
	DEFINE_PRED_FIELD( m_bDuckOverride, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),   

END_PREDICTION_DATA()

vgui::IImage* GetDefaultAvatarImage( C_BasePlayer *pPlayer )
{
	vgui::IImage* result = NULL;

	switch ( pPlayer ? pPlayer->GetTeamNumber() : TEAM_MAXCOUNT )
	{
		case TEAM_TERRORIST: 
			result = vgui::scheme()->GetImage( CSTRIKE_DEFAULT_T_AVATAR, true );
			break;

		case TEAM_CT:		 
			result = vgui::scheme()->GetImage( CSTRIKE_DEFAULT_CT_AVATAR, true );
			break;

		default:
			result = vgui::scheme()->GetImage( CSTRIKE_DEFAULT_AVATAR, true );
			break;
	}

	return result;
}

// ----------------------------------------------------------------------------- //
// Client ragdoll entity.
// ----------------------------------------------------------------------------- //

float g_flDieTranslucentTime = 0.6;




IMPLEMENT_CLIENTCLASS_DT_NOBASE( C_CSRagdoll, DT_CSRagdoll, CCSRagdoll )
	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropVector( RECVINFO(m_vecRagdollOrigin ) ),
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
	RecvPropInt( RECVINFO( m_nModelIndex ) ),
	RecvPropInt( RECVINFO(m_nForceBone ) ),
	RecvPropVector( RECVINFO(m_vecForce ) ),
	RecvPropVector( RECVINFO( m_vecRagdollVelocity ) ),
	RecvPropInt( RECVINFO(m_iDeathPose ) ),
	RecvPropInt( RECVINFO(m_iDeathFrame ) ),
	RecvPropInt(RECVINFO(m_iTeamNum )),
	RecvPropInt( RECVINFO(m_bClientSideAnimation )),
	RecvPropFloat( RECVINFO(m_flDeathYaw) ),
	RecvPropFloat( RECVINFO(m_flAbsYaw) ),
END_RECV_TABLE()


C_CSRagdoll::C_CSRagdoll():
	m_nGlowObjectHandle(-1)
{
	m_bInitialized = false;
}

C_CSRagdoll::~C_CSRagdoll()
{
	DestroyGlowObject();

	PhysCleanupFrictionSounds( this );

	SetRagdollClientSideAddon( 0 );

	DestroyAttachedWearableGibs();
}


void C_CSRagdoll::DestroyGlowObject()
{
	if ( m_nGlowObjectHandle >= 0 )
	{
		GlowObjectManager().UnregisterGlowObject( m_nGlowObjectHandle );
		m_nGlowObjectHandle = -1;
	}
}

void C_CSRagdoll::AttachWearableGibsFromPlayer( C_CSPlayer *pParentPlayer )
{
	/* Removed for partner depot */
}

void C_CSRagdoll::DestroyAttachedWearableGibs( void )
{
	/* Removed for partner depot */
}

void C_CSRagdoll::GetRagdollInitBoneArrays( matrix3x4a_t *pDeltaBones0, matrix3x4a_t *pDeltaBones1, matrix3x4a_t *pCurrentBones, float boneDt )
{
	// otherwise use the death pose to set up the ragdoll
	ForceSetupBonesAtTime( pDeltaBones0, gpGlobals->curtime - boneDt );
	GetRagdollCurSequenceWithDeathPose( this, pDeltaBones1, gpGlobals->curtime, m_iDeathPose, m_iDeathFrame );
	SetupBones( pCurrentBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
}

void C_CSRagdoll::GetRagdollInitBoneArraysYawMode( matrix3x4a_t *pDeltaBones0, matrix3x4a_t *pDeltaBones1, matrix3x4a_t *pCurrentBones, float boneDt )
{
	// turn off interp so we can setup bones in multiple positions
	SetEffects( EF_NOINTERP );

	// populate bone arrays for current positions and starting velocity positions
	InvalidateBoneCache();
	SetupBones( pCurrentBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
	Plat_FastMemcpy( pDeltaBones0, pCurrentBones, sizeof( matrix3x4a_t ) * MAXSTUDIOBONES );

	// set death anim
	CBaseAnimatingOverlay *pRagdollOverlay = GetBaseAnimatingOverlay();
	int n = pRagdollOverlay->GetNumAnimOverlays();
	if ( n > 0 )
	{
		CAnimationLayer *pLastRagdollLayer = pRagdollOverlay->GetAnimOverlay(n-1);
		if ( pLastRagdollLayer )
		{
			pLastRagdollLayer->SetSequence( m_iDeathPose );
			pLastRagdollLayer->SetWeight( 1 );
		}
	}
	SetPoseParameter( LookupPoseParameter( "death_yaw" ), m_flDeathYaw );

	SetAbsOrigin( GetAbsOrigin() );

	// set up bones in velocity adding positions
	InvalidateBoneCache();
	SetupBones( pDeltaBones1, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, gpGlobals->curtime );

	//fallback
	Vector vecRagdollVelocityPush = m_vecRagdollVelocity;
	
	C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( m_hPlayer.Get() );
	if ( pPlayer )
	{
		vecRagdollVelocityPush = pPlayer->m_vecLastAliveLocalVelocity * boneDt;
	}

	if ( vecRagdollVelocityPush.Length() > CS_PLAYER_SPEED_RUN * 3 )
		vecRagdollVelocityPush = vecRagdollVelocityPush.Normalized() * CS_PLAYER_SPEED_RUN * 3;
	
	// apply global extra velocity manually instead of relying on prediction to do it. This means all bones get the same vel...
	for ( int i=0; i<MAXSTUDIOBONES; i++ )
	{
		pDeltaBones1[i].SetOrigin( pDeltaBones0[i].GetOrigin() + vecRagdollVelocityPush );

		//debugoverlay->AddBoxOverlay( pCurrentBones[i].GetOrigin(), -Vector(0.1, 0.1, 0.1), Vector(0.1, 0.1, 0.1), QAngle(0,0,0), 255,0,0,255, 5 );
		////debugoverlay->AddBoxOverlay( pDeltaBones1[i].GetOrigin(), -Vector(0.1, 0.1, 0.1), Vector(0.1, 0.1, 0.1), QAngle(0,0,0), 0,255,0,255, 5 );
		////debugoverlay->AddBoxOverlay( pDeltaBones0[i].GetOrigin(), -Vector(0.1, 0.1, 0.1), Vector(0.1, 0.1, 0.1), QAngle(0,0,0), 0,0,255,255, 5 );
		//debugoverlay->AddLineOverlay( pDeltaBones0[i].GetOrigin(), pDeltaBones1[i].GetOrigin(), 255,0,0, true, 5 );
	}
}

void C_CSRagdoll::Interp_Copy( C_BaseAnimatingOverlay *pSourceEntity )
{
	if ( !pSourceEntity )
		return;
	
	VarMapping_t *pSrc = pSourceEntity->GetVarMapping();
	VarMapping_t *pDest = GetVarMapping();
    	
	// Find all the VarMapEntry_t's that represent the same variable.
	for ( int i = 0; i < pDest->m_Entries.Count(); i++ )
	{
		VarMapEntry_t *pDestEntry = &pDest->m_Entries[i];
		for ( int j=0; j < pSrc->m_Entries.Count(); j++ )
		{
			VarMapEntry_t *pSrcEntry = &pSrc->m_Entries[j];
			if ( !Q_strcmp( pSrcEntry->watcher->GetDebugName(),
				pDestEntry->watcher->GetDebugName() ) )
			{
				pDestEntry->watcher->Copy( pSrcEntry->watcher );
				break;
			}
		}
	}
}

void C_CSRagdoll::ApplySemiRandomDirectionalForce( Vector vecDir, float flStrength )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();

	if( !pPhysicsObject )
		return;
	
	IRagdoll *pIRagdoll = GetIRagdoll();
	if ( pIRagdoll )
	{
		for ( int i=0; i<24; i++ )
		{
			IPhysicsObject *pPhysObj = pIRagdoll->GetElement( i );
			if ( pPhysObj != NULL )
			{
				pPhysObj->ApplyForceCenter( vecDir * flStrength );
			}
		}
	}

	m_pRagdoll->ResetRagdollSleepAfterTime();
}

ConVar cl_random_taser_bone_y( "cl_random_taser_bone_y", "-1.0", 0, "The Y position used for the random taser force." );
ConVar cl_random_taser_force_y( "cl_random_taser_force_y", "-1.0", 0, "The Y position used for the random taser force." );
ConVar cl_random_taser_power( "cl_random_taser_power", "4000.0", 0, "Power used when applying the taser effect." );

void C_CSRagdoll::ApplyRandomTaserForce( void )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();

	if( !pPhysicsObject )
		return;

	int boneID = LookupBone( RandomInt( 0, 1 ) ? "ValveBiped.Bip01_L_Hand" : "ValveBiped.Bip01_R_Hand" );
	if( boneID < 0 )
	{
		// error, couldn't find a bone matching this name, early out
		AssertMsg( false, "couldn't find a bone matching this name, early out" );
		return;
	}
	
	Vector bonePos;
	QAngle boneAngle;
	GetBonePosition( boneID, bonePos, boneAngle );

	bonePos.y += cl_random_taser_bone_y.GetFloat();
	Vector dir( random->RandomFloat( -1.0f, 1.0f ), random->RandomFloat( -1.0f, 1.0f ), cl_random_taser_force_y.GetFloat() );
	VectorNormalize( dir );

	dir *= cl_random_taser_power.GetFloat();  // adjust  strength

	// apply force where we hit it
	pPhysicsObject->ApplyForceOffset( dir, bonePos );	

	// make sure the ragdoll is "awake" to process our updates, at least for a bit
	m_pRagdoll->ResetRagdollSleepAfterTime();

}

void C_CSRagdoll::ImpactTrace( trace_t *pTrace, int iDamageType, char *pCustomImpactName )
{

#if USE_VIOLENT_RAGDOLLS

	static const float RAGDOLL_IMPACT_MAGNITUDE = 8000.0f;

	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();

	if( !pPhysicsObject )
		return;

	Vector dir = pTrace->endpos - pTrace->startpos;
	
	if ( iDamageType == DMG_BLAST )
	{
		VectorNormalize( dir );
		dir *= RAGDOLL_IMPACT_MAGNITUDE;  // adjust impact strenght
				
		// apply force at object mass center
		pPhysicsObject->ApplyForceCenter( dir );
	}
	else
	{
		Vector hitpos;  
	
		VectorMA( pTrace->startpos, pTrace->fraction, dir, hitpos );
		VectorNormalize( dir );

		// apply force where we hit it (shock/taser is handled with a special death type )
		if ( (iDamageType & DMG_SHOCK ) == 0 )
		{
			// Blood spray!
			float flDamage = 10.0f;
			// This does smaller splotches on the guy and splats blood on the world.
			TraceBleed( flDamage, dir, pTrace, iDamageType );
			FX_CS_BloodSpray( hitpos, dir, flDamage );

			dir *= RAGDOLL_IMPACT_MAGNITUDE;  // adjust impact strenght
			pPhysicsObject->ApplyForceOffset( dir, hitpos );
		}
	}

	m_pRagdoll->ResetRagdollSleepAfterTime();

#endif //USE_VIOLENT_RAGDOLLS

}

void C_CSRagdoll::ValidateModelIndex( void )
{

#if CS_ALLOW_CL_MINMODELS

	C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( m_hPlayer.Get() );
	if ( pPlayer )
	{
		int iClass = pPlayer->PlayerClass();
		m_nModelIndex = FilterModelUsingCL_MinModels(iClass );
	}

#endif

	BaseClass::ValidateModelIndex();
}


void C_CSRagdoll::CreateLowViolenceRagdoll( void )
{
	// Just play a death animation.
	// Find a death anim to play.
	int iMinDeathAnim = 9999, iMaxDeathAnim = -9999;
	for ( int iAnim=1; iAnim < 100; iAnim++ )
	{
		char str[512];
		Q_snprintf( str, sizeof( str ), "death%d", iAnim );
		if ( LookupSequence( str ) == -1 )
			break;
		
		iMinDeathAnim = MIN( iMinDeathAnim, iAnim );
		iMaxDeathAnim = MAX( iMaxDeathAnim, iAnim );
	}

	if ( iMinDeathAnim == 9999 )
	{
		CreateCSRagdoll();
		return;
	}

	SetNetworkOrigin( m_vecRagdollOrigin );
	SetAbsOrigin( m_vecRagdollOrigin );
	SetAbsVelocity( m_vecRagdollVelocity );

	C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( m_hPlayer.Get() );
	if ( pPlayer )
	{
		if ( !pPlayer->IsDormant() && !pPlayer->m_bUseNewAnimstate )
		{
			// move my current model instance to the ragdoll's so decals are preserved.
			pPlayer->SnatchModelInstance( this );
			// copy bodygroup state
			SetBody( pPlayer->GetBody() );
		}

		SetAbsAngles( pPlayer->GetRenderAngles() );
		SetNetworkAngles( pPlayer->GetRenderAngles() );

		if ( pPlayer->m_bUseNewAnimstate )
			AttachWearableGibsFromPlayer( pPlayer );
			//pPlayer->CreateBoneAttachmentsFromWearables( this );

		pPlayer->MoveBoneAttachments( this );
	}

	int iDeathAnim = RandomInt( iMinDeathAnim, iMaxDeathAnim );
	char str[512];
	Q_snprintf( str, sizeof( str ), "death%d", iDeathAnim );
	SetSequence( LookupSequence( str ) );
	ForceClientSideAnimationOn();

	Interp_Reset( GetVarMapping() );
}

ConVar cl_ragdoll_workaround_threshold( "cl_ragdoll_workaround_threshold", "4", FCVAR_RELEASE, "Mainly cosmetic, client-only effect: when client doesn't know the last position of another player that spawns a ragdoll, the ragdoll creation is simplified and ragdoll is created in the right place. If you increase this significantly, ragdoll positions on your client may be dramatically wrong, but it won't affect other clients" );
ConVar spec_replay_outline( "spec_replay_outline", "1", FCVAR_CLIENTDLL, "Enable outline selecting victim in hltv replay: 0 - none; 1 - ouline YOU; 2 - outline YOU, with red ragdoll outline; 3 - normal spectator outlines" );

void C_CSPlayer::SetSequence( int nSequence )
{
	if ( m_bUseNewAnimstate && m_PlayerAnimStateCSGO )
	{
		AssertMsg( nSequence == 0, "Warning: Player attempted to set non-zero default sequence.\n" );
		BaseClass::SetSequence( 0 );
	}
	else
	{
		BaseClass::SetSequence( nSequence );
	}	
}

void C_CSRagdoll::CreateCSRagdoll()
{
	// First, initialize all our data. If we have the player's entity on our client,
	// then we can make ourselves start out exactly where the player is.
	C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( m_hPlayer.Get() );

	//	DevMsg( "Ragdoll %d player %d (s:%d) %s\n", entindex(), m_hPlayer.GetEntryIndex(), m_hPlayer.GetSerialNumber(), pPlayer ? " ok" : " unresolved" ); // replay

	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( pPlayer );

	// mark this to prevent model changes from overwriting the death sequence with the server sequence
	SetReceivedSequence();

	// if we're playing a replay, only add a glow to ourselves
	DestroyGlowObject();

	if ( g_HltvReplaySystem.GetHltvReplayDelay() && pPlayer && engine && spec_replay_outline.GetInt() )
	{
		if ( pPlayer->entindex() == g_HltvReplaySystem.GetPrimaryVictimEntIndex() )
		{
			float gb = ( spec_replay_outline.GetInt() == 2 ) ? 0.375f : 1.0f;
			m_nGlowObjectHandle = GlowObjectManager().RegisterGlowObject( this, Vector( 1, gb, gb ), 0.8f, true, false, GLOW_FOR_ALL_SPLIT_SCREEN_SLOTS );
			SetRenderColor( 255, 0, 0 );
		}						
	}

	if ( pPlayer && !pPlayer->IsDormant() )
	{
		// move my current model instance to the ragdoll's so decals are preserved.
		pPlayer->SnatchModelInstance(this);
		// copy bodygroup state
		SetBody( pPlayer->GetBody() );
	
		VarMapping_t *varMap = GetVarMapping();

		// Copy all the interpolated vars from the player entity.
		// The entity uses the interpolated history to get bone velocity.
		bool bRemotePlayer = (pPlayer != C_BasePlayer::GetLocalPlayer() );			
		if ( bRemotePlayer )
		{
			Interp_Copy( pPlayer );

			SetAbsAngles( QAngle( 0, m_flAbsYaw, 0 ) );
			GetRotationInterpolator().Reset( gpGlobals->curtime );

			m_flAnimTime = pPlayer->m_flAnimTime;
			SetSequence( pPlayer->GetSequence() );
		}
		else
		{
			// This is the local player, so set them in a default
			// pose and slam their velocity, angles and origin
			SetAbsOrigin( m_vecRagdollOrigin );
			
			SetAbsAngles( QAngle( 0, m_flAbsYaw, 0 ) );
			
			SetAbsVelocity( m_vecRagdollVelocity );
		}
		
		// in addition to base cycle, duplicate overlay layers and pose params onto the ragdoll, 
		// so the starting pose is as accurate as possible.

		SetCycle( pPlayer->GetCycle() );

		for ( int i=0; i<MAXSTUDIOPOSEPARAM; i++ )
		{
			//Msg( "Setting pose param %i to %.2f\n", i, pPlayer->GetPoseParameter( i ) );
			SetPoseParameter( i, pPlayer->GetPoseParameter( i ) );
		}

		CBaseAnimatingOverlay *pPlayerOverlay = pPlayer->GetBaseAnimatingOverlay();
		CBaseAnimatingOverlay *pRagdollOverlay = GetBaseAnimatingOverlay();
		if ( pPlayerOverlay )
		{
			int layerCount = pPlayerOverlay->GetNumAnimOverlays();
			pRagdollOverlay->SetNumAnimOverlays(layerCount);
			for( int layerIndex = 0; layerIndex < layerCount; ++layerIndex )
			{
				CAnimationLayer *playerLayer = pPlayerOverlay->GetAnimOverlay(layerIndex);
				CAnimationLayer *ragdollLayer = pRagdollOverlay->GetAnimOverlay(layerIndex);
				if( playerLayer && ragdollLayer )
				{
					ragdollLayer->SetCycle( playerLayer->GetCycle() );
					ragdollLayer->SetOrder( playerLayer->GetOrder() );
					ragdollLayer->SetSequence( playerLayer->GetSequence() );
					ragdollLayer->SetWeight( playerLayer->GetWeight() );
				}
			}
		}

		m_flPlaybackRate = pPlayer->GetPlaybackRate();


		if ( !bRemotePlayer )
		{
			Interp_Reset( varMap );
		}

		CopySequenceTransitions( pPlayer );
		
		if ( pPlayer->m_bUseNewAnimstate )
			AttachWearableGibsFromPlayer( pPlayer );
			//pPlayer->CreateBoneAttachmentsFromWearables( this );

		pPlayer->MoveBoneAttachments( this );
	}
	else
	{
		// overwrite network origin so later interpolation will
		// use this position
		SetNetworkOrigin( m_vecRagdollOrigin );

		SetAbsOrigin( m_vecRagdollOrigin );
		SetAbsVelocity( m_vecRagdollVelocity );

		Interp_Reset( GetVarMapping() );
	}

	bool bDissolveEntity = true;
	// Turn it into a ragdoll.
	if ( cl_ragdoll_physics_enable.GetInt() )
	{
		bool bRemoveCachedBonesAfterUse = g_HltvReplaySystem.GetHltvReplayDelay() == 0; // when we are not replaying, just reuse the bone cache once
		CachedRagdollBones_t *pCachedRagdoll = g_HltvReplaySystem.GetCachedRagdollBones( entindex(), bRemoveCachedBonesAfterUse );
		if ( pPlayer )
		{
			bDissolveEntity = false;
			// Make us a ragdoll..
			m_bClientSideRagdoll = true;
			Vector vRagdollOrigin = GetAbsOrigin(), vPlayerOrigin = pPlayer->GetAbsOrigin();

			matrix3x4a_t currentBones[ MAXSTUDIOBONES ];
			const float boneDt = 0.05f;
		
			bool bleedOut = false;
#if USE_VIOLENT_RAGDOLLS
			bleedOut = ( pPlayer ? !pPlayer->m_bKilledByTaser : true );
#endif

			if ( pCachedRagdoll && pCachedRagdoll->nBones == GetModelPtr()->numbones() )
			{
				const matrix3x4a_t *pCachedBones = pCachedRagdoll->GetBones();
				//DevMsg( "Reusing bones of ragdoll %d\n", entindex() );
				InitAsClientRagdoll( pCachedBones, pCachedBones, pCachedBones, boneDt, bleedOut );
				if ( m_pRagdoll )
				{
					if ( m_pRagdoll->RagdollBoneCount() == pCachedRagdoll->nBodyParts )
					{
						matrix3x4a_t *pCachedTransforms = pCachedRagdoll->GetBodyParts();
						for ( int i = 0; i < pCachedRagdoll->nBodyParts; ++i )
						{
							IPhysicsObject *pObj = m_pRagdoll->RagdollPhysicsObject( i );
							const matrix3x4a_t &cachedMatrix = pCachedTransforms[ i ];
							pObj->SetPositionMatrix( cachedMatrix, true );
						}
					}
					if ( pCachedRagdoll->bAllAsleep )
						m_pRagdoll->PhysForceRagdollToSleep();
				}
			}
			else if ( ( vRagdollOrigin - vPlayerOrigin ).LengthSqr() > Sqr( cl_ragdoll_workaround_threshold.GetFloat() ) )  // ragdoll origin is set from the player's origin on server. If they aren't the same, it means we haven't seen the player in a while.
			{
				// The player isn't even visible right now, so we don't need to run the complicated and hacky logic to make ragdoll transition seamless. That logic would teleport the ragdoll to the last known position of the now-dormant player

				SetupBones( currentBones, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
				// Plat_FastMemcpy( boneDelta0, m_CachedBoneData.Base(), sizeof( matrix3x4a_t ) * m_CachedBoneData.Count() );
				InitAsClientRagdoll( currentBones, currentBones, currentBones, boneDt, bleedOut );
			}
			else
			{
				matrix3x4a_t boneDelta0[ MAXSTUDIOBONES ];
				matrix3x4a_t boneDelta1[ MAXSTUDIOBONES ];
				// use death pose and death frame differently for new animstate player
				if ( pPlayer->m_bUseNewAnimstate )
				{
					GetRagdollInitBoneArraysYawMode( boneDelta0, boneDelta1, currentBones, boneDt );
				}
				else
				{
					// We used to get these values from the local player object when he ragdolled, but he was some bad values when using prediction.
					// It ends up that just getting the bone array values for this ragdoll works best for both the local and remote players.
					ConVarRef cl_ragdoll_crumple( "cl_ragdoll_crumple" );
					if ( cl_ragdoll_crumple.GetBool() )
					{
						BaseClass::GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
					}
					else
					{
						GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
					}
				}

				//Vector vResultOrigin = GetAbsOrigin();

				//Msg( "C_CSRagdoll::CreateCSRagdoll at {%.1f,%.1f,%.1f}, player at {%.1f,%.1f,%.1f}, spawning at {%.1f,%.1f,%.1f}\n", vRagdollOrigin.x, vRagdollOrigin.y, vRagdollOrigin.z, vPlayerOrigin.x, vPlayerOrigin.y, vPlayerOrigin.z, vResultOrigin.x, vResultOrigin.y, vResultOrigin.z );

				InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt, bleedOut );
			}
		}
		else if ( pCachedRagdoll )
		{
			// there's no player entity anymore, but we're in replay or just after replay - and we have enough information to recreate this ragdoll, cached off
			bDissolveEntity = false;
			// Make us a ragdoll..
			m_bClientSideRagdoll = true;
			const matrix3x4a_t *pCachedBones = pCachedRagdoll->GetBones();
			InitAsClientRagdoll( pCachedBones, pCachedBones, pCachedBones, 0.05f, false ); // here, we can recreate the ragdoll... but we lost our wearables :(
			if ( m_pRagdoll && pCachedRagdoll->bAllAsleep )
				m_pRagdoll->PhysForceRagdollToSleep();
		}
		if ( bRemoveCachedBonesAfterUse )
			g_HltvReplaySystem.FreeCachedRagdollBones( pCachedRagdoll );
	}
	
	if ( bDissolveEntity )
	{
		SetRenderMode( kRenderTransTexture );
		SetRenderFX( kRenderFxFadeOut, gpGlobals->curtime, g_flDieTranslucentTime );
	}

	m_bInitialized = true;
}

void C_CSRagdoll::SetRagdollClientSideAddon( uint32 uiAddonMask )
{
	/* Removed for partner depot */
	if ( ( uiAddonMask & ADDON_CLIENTSIDE_ASSASSINATION_TARGET ) && !m_hAssassinationTargetAddon.Get() )
	{
		C_BreakableProp *pEnt = new C_BreakableProp;
		pEnt->InitializeAsClientEntity( g_ClientSideAddons[ 2 ].m_pModelName, false );
		C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( m_hPlayer.Get() );
		if ( pPlayer )
		{
			// Create the mask
			int nAttachIndex = LookupAttachment( "facemask" );
			pEnt->SetParent( this, nAttachIndex );
			pEnt->SetLocalOrigin( Vector( 0, 0, 0 ) );
			pEnt->SetLocalAngles( QAngle( 0, 0, 0 ) );
			pEnt->SetUseParentLightingOrigin( true );
			pEnt->SetSolid( SOLID_NONE );
			pEnt->RemoveEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );
			m_hAssassinationTargetAddon.Set( pEnt );
		}
	}

	if ( !( uiAddonMask & ADDON_CLIENTSIDE_ASSASSINATION_TARGET ) && m_hAssassinationTargetAddon.Get() )
	{
		m_hAssassinationTargetAddon->Release();
		m_hAssassinationTargetAddon.Term();
	}
}

void C_CSRagdoll::OnDataChanged( DataUpdateType_t type )
{
	if ( type == DATA_UPDATE_CREATED )
	{
		int unused;
		AddDataChangeEvent( this, DATA_UPDATE_POST_UPDATE, &unused );
		return;
	}
	
	DataUpdateType_t typeForBaseClass = ( type == DATA_UPDATE_POST_UPDATE ) ? DATA_UPDATE_CREATED : type;
	BaseClass::OnDataChanged( typeForBaseClass );

	if ( type == DATA_UPDATE_POST_UPDATE )
	{
		if ( g_RagdollLVManager.IsLowViolence() )
		{
			CreateLowViolenceRagdoll();
		}
		else
		{
			CreateCSRagdoll();
		}
	}
	else 
	{
		if ( !cl_ragdoll_physics_enable.GetInt() )
		{
			// Don't let it set us back to a ragdoll with data from the server.
			m_bClientSideRagdoll = false;
		}
	}
}

IRagdoll* C_CSRagdoll::GetIRagdoll() const
{
	return m_pRagdoll;
}

//-----------------------------------------------------------------------------
// Purpose: Called when the player toggles nightvision
// Input  : *pData - the int value of the nightvision state
//			*pStruct - the player
//			*pOut - 
//-----------------------------------------------------------------------------
void RecvProxy_NightVision( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_CSPlayer *pPlayerData = (C_CSPlayer * ) pStruct;

	bool bNightVisionOn = ( pData->m_Value.m_Int > 0 );
	
	if ( pPlayerData->m_bNightVisionOn != bNightVisionOn )
	{
		if ( bNightVisionOn )
			 pPlayerData->m_flNightVisionAlpha = 1;
	}

	pPlayerData->m_bNightVisionOn = bNightVisionOn;
}

void RecvProxy_FlashTime( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_CSPlayer *pPlayerData = (C_CSPlayer * ) pStruct;
	pPlayerData->m_bFlashBuildUp = false;

	float flNewFlashDuration = pData->m_Value.m_Float;
	if ( flNewFlashDuration == 0.0f )
	{
		// Disable flashbang effect
		pPlayerData->m_flFlashScreenshotAlpha = 0.0f;
		pPlayerData->m_flFlashOverlayAlpha = 0.0f;
		pPlayerData->m_bFlashBuildUp = false;
		pPlayerData->m_bFlashScreenshotHasBeenGrabbed = false;
		pPlayerData->m_flFlashDuration = 0.0f;
		pPlayerData->m_flFlashBangTime = 0.0f;
		pPlayerData->m_bFlashDspHasBeenCleared = false;
	
		C_CSPlayer *pLocalCSPlayer = C_CSPlayer::GetLocalCSPlayer();
		if (pLocalCSPlayer)
		{
			pLocalCSPlayer->m_bFlashDspHasBeenCleared = false;
		}
		return;
	}

	// If local player is spectating in mode other than first-person, reduce effect duration by half
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer && pLocalPlayer->GetObserverMode() != OBS_MODE_NONE && pLocalPlayer->GetObserverMode() != OBS_MODE_IN_EYE )
	{
		flNewFlashDuration *= 0.5f;
	}

	if ( pLocalPlayer && pLocalPlayer->GetObserverMode() != OBS_MODE_NONE && 
		 flNewFlashDuration > 0.0f && pPlayerData->m_flFlashDuration == flNewFlashDuration )
	{
		// Ignore this update. This is a resend from the server triggered by the spectator changing target.
		return;
	}

	if ( !pPlayerData->IsFlashBangActive() && flNewFlashDuration > 0.0f )
	{
		// reset flash alpha to start of effect build-up
		pPlayerData->m_flFlashScreenshotAlpha = 1.0f;
		pPlayerData->m_flFlashOverlayAlpha = 1.0f;
		pPlayerData->m_bFlashBuildUp = true;
		pPlayerData->m_bFlashScreenshotHasBeenGrabbed = false;
	}

	pPlayerData->m_flFlashDuration = flNewFlashDuration;
	pPlayerData->m_flFlashBangTime = gpGlobals->curtime + pPlayerData->m_flFlashDuration;
	pPlayerData->m_bFlashDspHasBeenCleared = false;
	
	C_CSPlayer *pLocalCSPlayer = C_CSPlayer::GetLocalCSPlayer();
	if (pLocalCSPlayer)
	{
		pLocalCSPlayer->m_bFlashDspHasBeenCleared = false;
	}
}

void C_CSPlayer::SetRenderAlpha( byte a )
{
	/* Removed for partner depot */
	BaseClass::SetRenderAlpha( a );
}

void C_CSPlayer::SetRenderMode( RenderMode_t nRenderMode, bool bForceUpdate )
{
	/* Removed for partner depot */
	BaseClass::SetRenderMode( nRenderMode, bForceUpdate );
}

void C_CSPlayer::UpdateFlashBangEffect( void )
{
	if ( ( m_flFlashBangTime < gpGlobals->curtime ) || ( m_flFlashMaxAlpha <= 0.0f ) )
	{
		// FlashBang is inactive
		m_flFlashScreenshotAlpha = 0.0f;
		m_flFlashOverlayAlpha = 0.0f;
		return;
	}

	static const float FLASH_BUILD_UP_PER_FRAME = 45.0f;
	static const float FLASH_BUILD_UP_DURATION = ( 255.0f / FLASH_BUILD_UP_PER_FRAME ) * ( 1.0f / 60.0f );

	float flFlashTimeElapsed = GetFlashTimeElapsed();

	if ( m_bFlashBuildUp )
	{
		// build up
		m_flFlashScreenshotAlpha = Clamp( ( flFlashTimeElapsed / FLASH_BUILD_UP_DURATION ) * m_flFlashMaxAlpha.Get(), 0.0f, m_flFlashMaxAlpha.Get() );
		m_flFlashOverlayAlpha = m_flFlashScreenshotAlpha;

		if ( flFlashTimeElapsed >= FLASH_BUILD_UP_DURATION )
		{
			m_bFlashBuildUp = false;
		}
	}
	else
	{
		// cool down
		float flFlashTimeLeft = m_flFlashBangTime - gpGlobals->curtime;
		m_flFlashScreenshotAlpha = ( m_flFlashMaxAlpha * flFlashTimeLeft ) / m_flFlashDuration;
		m_flFlashScreenshotAlpha = Clamp( m_flFlashScreenshotAlpha, 0.0f, m_flFlashMaxAlpha.Get() );

		float flAlphaPercentage = 1.0f;
		const float certainBlindnessTimeThresh = 3.0f; // yes this is a magic number, necessary to match CS/CZ flashbang effectiveness cause the rendering system is completely different.

		if (flFlashTimeLeft > certainBlindnessTimeThresh)
		{
			// if we still have enough time of blindness left, make sure the player can't see anything yet.
			flAlphaPercentage = 1.0f;
		}
		else
		{
			// blindness effects shorter than 'certainBlindness`TimeThresh' will start off at less than 255 alpha.
			flAlphaPercentage = flFlashTimeLeft / certainBlindnessTimeThresh;

			// reduce alpha level quicker with dx 8 support and higher to compensate
			// for having the burn-in effect.
			flAlphaPercentage *= flAlphaPercentage;
		}

		m_flFlashOverlayAlpha = flAlphaPercentage *= m_flFlashMaxAlpha; // scale a [0..1) value to a [0..MaxAlpha] value for the alpha.

		// make sure the alpha is in the range of [0..MaxAlpha]
		m_flFlashOverlayAlpha = Max( m_flFlashOverlayAlpha, 0.0f );
		m_flFlashOverlayAlpha = Min( m_flFlashOverlayAlpha, m_flFlashMaxAlpha.Get());
	}
}


void RecvProxy_HasDefuser( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_CSPlayer *pPlayerData = (C_CSPlayer * )pStruct;

	if (pPlayerData == NULL )
	{
		return;
	}

	bool drawIcon = false;

	if (pData->m_Value.m_Int == 0 )
	{
		pPlayerData->RemoveDefuser();
	}
	else
	{
		if (pPlayerData->HasDefuser() == false )
		{
			drawIcon = true;
		}
		pPlayerData->GiveDefuser();

		if ( drawIcon )
			pPlayerData->DisplayInventory( true );
	}
}

void C_CSPlayer::RecvProxy_CycleLatch( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// This receive proxy looks to see if the server's value is close enough to what we think it should
	// be.  We've been running the same code; this is an error correction for changes we didn't simulate
	// while they were out of PVS.
	C_CSPlayer *pPlayer = (C_CSPlayer * )pStruct;
	if( C_BasePlayer::IsLocalPlayer( pPlayer ) )
		return; // Don't need to fixup ourselves.  

	float incomingCycle = (float )(pData->m_Value.m_Int) / 16; // Came in as 4 bit fixed point
	float currentCycle = pPlayer->GetCycle();
	bool closeEnough = fabs(currentCycle - incomingCycle ) < CycleLatchTolerance;
	if( fabs(currentCycle - incomingCycle ) > (1 - CycleLatchTolerance) )
	{
		closeEnough = true;// Handle wrapping around 1->0
	}

	if( !closeEnough )
	{
		// Server disagrees too greatly.  Correct our value.
		if ( pPlayer && pPlayer->GetTeam() )
		{
			DevMsg( 2, "%s %s(%d): Cycle latch wants to correct %.2f in to %.2f.\n",
				pPlayer->GetTeam()->Get_Name(), pPlayer->GetPlayerName(), pPlayer->entindex(), currentCycle, incomingCycle );
		}
		pPlayer->SetServerIntendedCycle( incomingCycle );
	}
}

bool C_CSPlayer::ShouldRegenerateOriginFromCellBits() const
{
	return false;
}

bool __MsgFunc_ItemDrop( const CCSUsrMsg_ItemDrop &msg )
{
	return true;	
}
USER_MESSAGE_REGISTER( ItemDrop );

bool __MsgFunc_ReloadEffect( const CCSUsrMsg_ReloadEffect &msg )
{
	int iPlayer = msg.entidx();
	int iActAnimID = msg.has_actanim() ? msg.actanim() : ACT_VM_RELOAD;

	Vector origin;
	Vector *pOrigin = NULL;
	if ( msg.has_origin_x() )
	{
		origin.x = msg.origin_x();
		origin.y = msg.origin_y();
		origin.z = msg.origin_z();
		pOrigin = &origin;
	}

	C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( C_BaseEntity::Instance( iPlayer ) );
	if ( pPlayer )
		pPlayer->PlayReloadEffect( iActAnimID, pOrigin );

	return true;	
}
USER_MESSAGE_REGISTER( ReloadEffect );

BEGIN_RECV_TABLE_NOBASE( C_CSPlayer, DT_CSLocalPlayerExclusive )

	// DEPRECATED; redundant origin positions. Kept for backwards demo compatible.
	RecvPropVectorXY( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropFloat( RECVINFO_NAME( m_vecNetworkOrigin[2], m_vecOrigin[2] ) ),
	/////	
	
	RecvPropFloat( RECVINFO(m_flStamina ) ),
	RecvPropInt( RECVINFO( m_iDirection ) ),
	RecvPropInt( RECVINFO( m_iShotsFired ) ),
	RecvPropInt( RECVINFO( m_nNumFastDucks ) ),
	RecvPropBool( RECVINFO( m_bDuckOverride ) ),
	RecvPropFloat( RECVINFO( m_flVelocityModifier ) ),

	RecvPropArray3( RECVINFO_ARRAY( m_bPlayerDominated ), RecvPropBool( RECVINFO( m_bPlayerDominated[0] ) ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_bPlayerDominatingMe ), RecvPropBool( RECVINFO( m_bPlayerDominatingMe[0] ) ) ),

	RecvPropArray3( RECVINFO_ARRAY( m_iWeaponPurchasesThisRound ), RecvPropInt( RECVINFO( m_iWeaponPurchasesThisRound[0] ) ) ),

	RecvPropInt( RECVINFO( m_nQuestProgressReason ) ),
END_RECV_TABLE()


BEGIN_RECV_TABLE_NOBASE( C_CSPlayer, DT_CSNonLocalPlayerExclusive )
	// DEPRECATED; redundant origin positions. Kept for backwards demo compatible.
	RecvPropVectorXY( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropFloat( RECVINFO_NAME( m_vecNetworkOrigin[2], m_vecOrigin[2] ) ),
	//////
END_RECV_TABLE()


IMPLEMENT_CLIENTCLASS_DT( C_CSPlayer, DT_CSPlayer, CCSPlayer )
	// Data that only gets sent to the local player.
	RecvPropDataTable( "cslocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_CSLocalPlayerExclusive ) ),
	RecvPropDataTable( "csnonlocaldata", 0, 0, &REFERENCE_RECV_TABLE(DT_CSNonLocalPlayerExclusive ) ),
	
	RecvPropFloat( RECVINFO( m_angEyeAngles[0] ) ),
	RecvPropFloat( RECVINFO( m_angEyeAngles[1] ) ),

	RecvPropInt( RECVINFO( m_iAddonBits ) ),
	RecvPropInt( RECVINFO( m_iPrimaryAddon ) ),
	RecvPropInt( RECVINFO( m_iSecondaryAddon ) ),
	RecvPropInt( RECVINFO( m_iThrowGrenadeCounter ) ),
	RecvPropBool( RECVINFO( m_bWaitForNoAttack ) ),
	RecvPropBool( RECVINFO( m_bIsRespawningForDMBonus ) ),
	RecvPropInt( RECVINFO( m_iPlayerState ) ),
	RecvPropInt( RECVINFO( m_iAccount ) ),
	RecvPropInt( RECVINFO( m_iStartAccount ) ),
	RecvPropInt( RECVINFO( m_totalHitsOnServer ) ),
	RecvPropInt( RECVINFO( m_bInBombZone ) ),
	RecvPropInt( RECVINFO( m_bInBuyZone ) ),
	RecvPropInt( RECVINFO( m_bInNoDefuseArea ) ),
	RecvPropBool( RECVINFO( m_bKilledByTaser ) ),
	RecvPropInt( RECVINFO( m_iMoveState ) ),
	RecvPropInt( RECVINFO( m_iClass ) ),
	RecvPropInt( RECVINFO( m_ArmorValue ) ),
	RecvPropQAngles( RECVINFO( m_angEyeAngles ) ),
	RecvPropInt( RECVINFO( m_bHasDefuser ), 0, RecvProxy_HasDefuser ),
	RecvPropInt( RECVINFO( m_bNightVisionOn ), 0, RecvProxy_NightVision ),
	RecvPropBool( RECVINFO( m_bHasNightVision ) ),
	RecvPropBool( RECVINFO( m_bInHostageRescueZone ) ),
	RecvPropBool( RECVINFO( m_bIsDefusing ) ),
	RecvPropBool( RECVINFO( m_bIsGrabbingHostage ) ),
	RecvPropBool( RECVINFO( m_bIsScoped ) ),
	RecvPropBool( RECVINFO( m_bIsWalking ) ),
	RecvPropBool( RECVINFO( m_bResumeZoom ) ),
	RecvPropFloat( RECVINFO( m_fImmuneToGunGameDamageTime ) ),
	RecvPropBool( RECVINFO( m_bGunGameImmunity ) ),
	RecvPropBool( RECVINFO( m_bHasMovedSinceSpawn ) ),
	RecvPropBool( RECVINFO( m_bMadeFinalGunGameProgressiveKill ) ),
	RecvPropInt( RECVINFO( m_iGunGameProgressiveWeaponIndex ) ),
	RecvPropInt( RECVINFO( m_iNumGunGameTRKillPoints ) ),
	RecvPropInt( RECVINFO( m_iNumGunGameKillsWithCurrentWeapon ) ),
	RecvPropInt( RECVINFO( m_iNumRoundKills ) ),
	RecvPropFloat( RECVINFO( m_fMolotovUseTime ) ),
	RecvPropFloat( RECVINFO( m_fMolotovDamageTime ) ),
	RecvPropString( RECVINFO( m_szArmsModel ) ),
	RecvPropEHandle( RECVINFO(m_hCarriedHostage) ),
	RecvPropEHandle( RECVINFO(m_hCarriedHostageProp) ),
	RecvPropBool( RECVINFO( m_bIsRescuing ) ),
	RecvPropFloat( RECVINFO( m_flGroundAccelLinearFracLastTime ) ),
	RecvPropBool( RECVINFO( m_bCanMoveDuringFreezePeriod ) ),
	RecvPropBool( RECVINFO( m_isCurrentGunGameLeader ) ),
	RecvPropBool( RECVINFO( m_isCurrentGunGameTeamLeader ) ),	
	RecvPropFloat( RECVINFO( m_flGuardianTooFarDistFrac ) ),	
	RecvPropFloat( RECVINFO( m_flDetectedByEnemySensorTime ) ),
	
	RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_Kills ),			RecvPropInt( RECVINFO( m_iMatchStats_Kills[0] ))),
	RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_Damage ),			RecvPropInt( RECVINFO( m_iMatchStats_Damage[0] ))),
	RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_EquipmentValue ), RecvPropInt( RECVINFO( m_iMatchStats_EquipmentValue[0] ))),
	RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_MoneySaved ),		RecvPropInt( RECVINFO( m_iMatchStats_MoneySaved[0] ))),
	RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_KillReward ),		RecvPropInt( RECVINFO( m_iMatchStats_KillReward[0] ))),
	RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_LiveTime ),		RecvPropInt( RECVINFO( m_iMatchStats_LiveTime[0] ))),
	RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_Deaths ),		RecvPropInt( RECVINFO( m_iMatchStats_Deaths[0] ))),
	RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_Assists ),		RecvPropInt( RECVINFO( m_iMatchStats_Assists[0] ))),
	RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_HeadShotKills ),		RecvPropInt( RECVINFO( m_iMatchStats_HeadShotKills[0] ))),
	RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_Objective ),		RecvPropInt( RECVINFO( m_iMatchStats_Objective[0] ))),
	RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_CashEarned ),		RecvPropInt( RECVINFO( m_iMatchStats_CashEarned[0] ))),
	RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_UtilityDamage ), RecvPropInt( RECVINFO( m_iMatchStats_UtilityDamage[0] ))),
	RecvPropArray3( RECVINFO_ARRAY( m_iMatchStats_EnemiesFlashed ), RecvPropInt( RECVINFO( m_iMatchStats_EnemiesFlashed[0] ))),

	RecvPropArray3( RECVINFO_ARRAY(m_rank), RecvPropInt( RECVINFO(m_rank[0]))),

	RecvPropInt( RECVINFO( m_unMusicID ), 0 ),

#ifdef CS_SHIELD_ENABLED
	RecvPropBool( RECVINFO( m_bHasShield ) ),
	RecvPropBool( RECVINFO( m_bShieldDrawn ) ),
#endif

	RecvPropBool( RECVINFO( m_bHasHelmet ) ),
	RecvPropBool( RECVINFO( m_bHasHeavyArmor ) ),
	RecvPropFloat( RECVINFO( m_flFlashDuration ), 0, RecvProxy_FlashTime ),
	RecvPropFloat( RECVINFO( m_flFlashMaxAlpha )),
	RecvPropInt( RECVINFO( m_iProgressBarDuration ) ),
	RecvPropFloat( RECVINFO( m_flProgressBarStartTime ) ),
	RecvPropEHandle( RECVINFO( m_hRagdoll ) ),
	RecvPropInt( RECVINFO( m_cycleLatch ), 0, &C_CSPlayer::RecvProxy_CycleLatch ),
	RecvPropInt( RECVINFO( m_unCurrentEquipmentValue ) ),
	RecvPropInt( RECVINFO( m_unRoundStartEquipmentValue ) ),
	RecvPropInt( RECVINFO( m_unFreezetimeEndEquipmentValue ) ),

#if CS_CONTROLLABLE_BOTS_ENABLED
	RecvPropBool( RECVINFO( m_bIsControllingBot ) ),
	RecvPropBool( RECVINFO( m_bHasControlledBotThisRound ) ),
	RecvPropBool( RECVINFO( m_bCanControlObservedBot ) ),
	RecvPropInt( RECVINFO( m_iControlledBotEntIndex ) ),
#endif

	RecvPropBool( RECVINFO( m_bIsAssassinationTarget ) ),

	// data used to show and hide hud via scripts in the training map
	RecvPropBool( RECVINFO( m_bHud_MiniScoreHidden ) ),
	RecvPropBool( RECVINFO( m_bHud_RadarHidden ) ),

	RecvPropInt( RECVINFO( m_nLastKillerIndex ) ),
	// when a player dies, we send to the client the number of unbroken  times in a row the player has been killed by their last killer
	RecvPropInt( RECVINFO( m_nLastConcurrentKilled ) ),
	RecvPropInt( RECVINFO( m_nDeathCamMusic ) ),

	RecvPropBool( RECVINFO( m_bIsHoldingLookAtWeapon ) ),
	RecvPropBool( RECVINFO( m_bIsLookingAtWeapon ) ),
	RecvPropInt( RECVINFO( m_iNumRoundKillsHeadshots ) ),
#if defined( PLAYER_TAUNT_SHIPPING_FEATURE )
	RecvPropBool( RECVINFO( m_bIsTaunting ) ),
	RecvPropBool( RECVINFO( m_bIsThirdPersonTaunt ) ),
	RecvPropBool( RECVINFO( m_bIsHoldingTaunt ) ),
	RecvPropFloat( RECVINFO( m_flTauntYaw ) ),
#endif

#if defined( USE_PLAYER_ATTRIBUTE_MANAGER )
	RecvPropDataTable( RECVINFO_DT( m_AttributeManager ), 0, &REFERENCE_RECV_TABLE(DT_AttributeManager) ),
#endif

	RecvPropFloat( RECVINFO( m_flLowerBodyYawTarget ) ),
	RecvPropBool( RECVINFO( m_bStrafing ) ),

	RecvPropFloat( RECVINFO( m_flThirdpersonRecoil ) ),	

END_RECV_TABLE()

bool C_CSPlayer::s_bPlayingFreezeCamSound = false;


C_CSPlayer::C_CSPlayer() : 
	m_iv_angEyeAngles( "C_CSPlayer::m_iv_angEyeAngles" ),
	m_GlowObject( this, Vector( 1.0f, 1.0f, 1.0f ), 0.0f, false, false ),
	m_bIsSpecFollowingGrenade( false )
{

	m_bMaintainSequenceTransitions = false; // disabled for perf - animstate takes care of carefully blending only the layers that need it

	m_PlayerAnimState = CreatePlayerAnimState( this, this, LEGANIM_9WAY, true );
	
	//the new animstate needs to live side-by-side with the old animstate for a while so it can be hot swappable
	m_PlayerAnimStateCSGO = CreateCSGOPlayerAnimstate( this );

	m_bCanMoveDuringFreezePeriod = false;

	m_flThirdpersonRecoil = 0;

	m_angEyeAngles.Init();

	AddVar( &m_angEyeAngles, &m_iv_angEyeAngles, LATCH_SIMULATION_VAR );

	// Remove interpolation of variables we have excluded from send table
	// HACK: m_angRotation is private in C_BaseEntity but it's accessible via GetLocalAngles()
	RemoveVar( const_cast<QAngle*>(&GetLocalAngles()) );  	// == RemoveVar( &m_angRotation );

	m_bAddonModelsAreOutOfDate = false;
	m_iLastAddonBits = m_iAddonBits = 0;
	m_iLastPrimaryAddon = m_iLastSecondaryAddon = WEAPON_NONE;
	m_iProgressBarDuration = 0;
	m_flProgressBarStartTime = 0.0f;
	m_ArmorValue = 0;
	m_bHasHelmet = false;
	m_bHasHeavyArmor = false;
	m_iIDEntIndex = 0;
	m_delayTargetIDTimer.Reset();
	m_iOldIDEntIndex = 0;
	m_holdTargetIDTimer.Reset();
	m_iDirection = 0;
	m_bIsBuyMenuOpen = false;
	m_flNextGuardianTooFarWarning = 0;

	m_flLastFiredWeaponTime = -1;

	m_nQuestProgressReason = QuestProgress::QUEST_NONINITIALIZED;

	m_unCurrentEquipmentValue = 0;
	m_unRoundStartEquipmentValue = 0;
	m_unFreezetimeEndEquipmentValue = 0;

	m_hC4AddonLED = NULL;
	m_hC4WeaponLED = NULL;

	m_hOldGrenadeObserverTarget = NULL;

	m_Activity = ACT_IDLE;

	m_pFlashlightBeam = NULL;
	m_fNextThinkPushAway = 0.0f;
	m_fNextGlowCheckUpdate = 0.0f;
	m_fNextGlowCheckInterval = GLOWUPDATE_DEFAULT_THINK_INTERVAL;

	m_fGlowAlpha = 1.0f;
	m_fGlowAlphaTarget = 1.0f;
	m_fGlowAlphaUpdateTime = -1.0f;
	m_fGlowAlphaTargetTime = -1.0f;

	m_bFreezeCamFlashlightActive = false;

	m_serverIntendedCycle = -1.0f;

	view->SetScreenOverlayMaterial( NULL );

	m_iTargetedWeaponEntIndex = 0;

	m_vecFreezeFrameEnd = Vector( 0, 0, 0 );
	m_flFreezeFrameTilt = 0;
	m_vecFreezeFrameAnglesStart = QAngle( 0, 0, 0 );
	m_bFreezeFrameCloseOnKiller = false;
	m_nFreezeFrameShiftSideDist = 0;

	m_bOldIsScoped = false;

	m_fImmuneToGunGameDamageTimeLast = 0;

	m_previousPlayerState = STATE_DORMANT;

	m_duckUntilOnGround = false;

	// set all matchstats values to -1 on the client to identify new stats that don't
	// exist in demos
	memset( m_iMatchStats_Kills, -1, sizeof(m_iMatchStats_Kills) );		
	memset( m_iMatchStats_Damage, -1, sizeof(m_iMatchStats_Damage) );	 
	memset( m_iMatchStats_EquipmentValue, -1, sizeof(m_iMatchStats_EquipmentValue) );	 
	memset( m_iMatchStats_MoneySaved, -1, sizeof(m_iMatchStats_MoneySaved) );	
	memset( m_iMatchStats_KillReward, -1, sizeof(m_iMatchStats_KillReward) );	 	
	memset( m_iMatchStats_LiveTime, -1, sizeof(m_iMatchStats_LiveTime) );	 
	memset( m_iMatchStats_Deaths, -1, sizeof(m_iMatchStats_Deaths) );		
	memset( m_iMatchStats_Assists, -1, sizeof(m_iMatchStats_Assists) );		
	memset( m_iMatchStats_HeadShotKills, -1, sizeof(m_iMatchStats_HeadShotKills) );	
	memset( m_iMatchStats_Objective, -1, sizeof(m_iMatchStats_Objective) );		
	memset( m_iMatchStats_CashEarned, -1, sizeof(m_iMatchStats_CashEarned) );	
	memset( m_iMatchStats_UtilityDamage, -1, sizeof( m_iMatchStats_UtilityDamage ) );
	memset( m_iMatchStats_EnemiesFlashed, -1, sizeof( m_iMatchStats_EnemiesFlashed ) );

	//=============================================================================
	// HPE_BEGIN:
	// [dwenger] Added for auto-buy functionality
	//=============================================================================
	m_bShouldAutobuyNow = false;
	m_bShouldAutobuyDMWeapons = false;
	ListenForGameEvent( "round_freeze_end" );
	//=============================================================================
	// HPE_END
	//=============================================================================

	ListenForGameEvent( "ggtr_player_levelup" );
	ListenForGameEvent( "ggprogressive_player_levelup" );
	ListenForGameEvent( "gg_player_impending_upgrade" );
	ListenForGameEvent( "gg_killed_enemy" );
	ListenForGameEvent( "gg_final_weapon_achieved" );
	ListenForGameEvent( "gg_bonus_grenade_achieved" );
	ListenForGameEvent( "gg_reset_round_start_sounds" );
	ListenForGameEvent( "gg_leader" );
	ListenForGameEvent( "gg_team_leader" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "round_officially_ended" );
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_spawn" );
	ListenForGameEvent( "item_pickup" );
	ListenForGameEvent( "ammo_pickup" );
	ListenForGameEvent( "defuser_pickup" );
	ListenForGameEvent( "player_given_c4" );

	ListenForGameEvent( "tr_mark_complete" );
	ListenForGameEvent( "tr_mark_best_time" );

	ListenForGameEvent( "cs_pre_restart" );
	ListenForGameEvent( "weapon_fire" );
	
	
	ListenForGameEvent( "bot_takeover" );
	ListenForGameEvent( "spec_target_updated" );

	ListenForGameEvent( "add_bullet_hit_marker" );
	ListenForGameEvent( "assassination_target_killed" );


	//m_isCurrentGunGameLeader = false;
	//m_isCurrentGunGameTeamLeader = false;
	m_nextTaserShakeTime = 0.0f;
	m_firstTaserShakeTime= 0.0f;
	m_bKilledByTaser = false;

	m_iMoveState = MOVESTATE_IDLE;

	m_flFlashScreenshotAlpha = 0.0f;
	m_flFlashOverlayAlpha = 0.0f;
	m_bFlashBuildUp = false;
	m_bFlashScreenshotHasBeenGrabbed = false;
	m_bFlashDspHasBeenCleared = true;

	m_bPlayingHostageCarrySound = false;

	m_flLastSmokeOverlayAlpha = 0.0f;
	m_fMolotovUseTime = 0.0f;
	m_fMolotovDamageTime = 0.0f;

	m_vecLastMuzzleFlashPos = Vector(0,0,0);
	m_angLastMuzzleFlashAngle = QAngle(0,0,0);

	m_flHealthFadeValue = 100.0f;
	m_flHealthFadeAlpha = 0.0f;



	SetCurrentMusic( CSMUSIC_NONE );
	m_flMusicRoundStartTime = 0.0;
	m_vecObserverInterpolateOffset = vec3_origin;
	m_bObserverInterpolationNeedsDeferredSetup = false;
	m_flObsInterp_PathLength = 0.0f;
	m_obsInterpState = OBSERVER_INTERP_NONE;
	m_qObsInterp_OrientationStart = m_qObsInterp_OrientationTravelDir = Quaternion( 0, 0, 0, 0 );

	// This code allowed us to measure discrepency between client and server bullet hits.
	// It became obsolete when we started using a separate seed for client and server
	// to eliminate 'rage' hacks.
	//
	m_ui8ClientServerHitDifference = 0;

	m_flNextMagDropTime = 0;
	m_nLastMagDropAttachmentIndex = -1;

	m_boneSnapshots[BONESNAPSHOT_UPPER_BODY].Init( this );
	m_boneSnapshots[BONESNAPSHOT_UPPER_BODY].SetWeightListName( "snapshot_weights_upperbody" );
	
	m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].Init( this );
	m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].AddSubordinate( &m_boneSnapshots[BONESNAPSHOT_UPPER_BODY] );
	m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].SetWeightListName( "snapshot_weights_all" );

	m_nClipPlaneProximityLimitAttachmentIndex = -1;

	m_flLastSpawnTimeIndex = 0;

	m_vecLastAliveLocalVelocity.Init();

	m_fRenderingClipPlane[0] = 0.0f;
	m_fRenderingClipPlane[1] = 0.0f;
	m_fRenderingClipPlane[2] = 0.0f;
	m_fRenderingClipPlane[3] = 0.0f;
	m_nLastClipPlaneSetupFrame = 0;
	m_vecLastClipCameraPos.Init();
	m_vecLastClipCameraForward.Init();
	m_bClipHitStaticWorld = false;
	m_bCachedPlaneIsValid = false;
	m_pClippingWeaponWorldModel = NULL;


	m_vecLastContactShadowTraceOriginLeft = vec3_origin;
	m_vecLastContactShadowTraceOriginRight = vec3_origin;
	m_flLastContactShadowGroundHeightLeft = 0;
	m_flLastContactShadowGroundHeightRight = 0;

}

class C_PlayerFootContactShadow : public C_BreakableProp
{
public:

	EHANDLE m_hParentPlayer;

	float *	GetRenderClipPlane( void )
	{
		C_CSPlayer *pParentPlayer = ToCSPlayer( m_hParentPlayer.Get() );
		if ( pParentPlayer )
			return pParentPlayer->GetRenderClipPlane();

		return NULL;
	}

	virtual bool ShouldDraw()
	{

		SetAllowFastPath( false );

		if ( !m_hParentPlayer.IsValid() )
		{
			return true;
		}

		if ( GetAbsOrigin().LengthSqr() < 1 )
			return false;

		C_CSPlayer *pParentPlayer = ToCSPlayer( m_hParentPlayer.Get() );
		if ( pParentPlayer )
		{
			if ( pParentPlayer->IsDormant() )
				return false;

			if ( pParentPlayer->m_hContactShadowLeft.Get() == this || pParentPlayer->m_hContactShadowRight.Get() == this )
			{
				return BaseClass::ShouldDraw() && pParentPlayer->ShouldDraw();
			}
		}

		extern bool SetImmediateEntityRemovesAllowed( bool bAllowed );
		bool bTemp = SetImmediateEntityRemovesAllowed( false );
		UTIL_Remove( this );
		SetImmediateEntityRemovesAllowed( bTemp );

		return false;
	}

};

void C_CSPlayer::CreateClientEffectModels( void )
{
	if ( !m_hMuzzleFlashShape.Get() )
	{
		C_BaseAnimating *pMuzzleFlashShape = new C_BaseAnimating;
		if ( pMuzzleFlashShape->InitializeAsClientEntity( "models/weapons/w_muzzlefireshape.mdl", false ) )
		{
			pMuzzleFlashShape->AddEffects( EF_NODRAW );
			m_hMuzzleFlashShape.Set( pMuzzleFlashShape );
		}
	}

	if ( !m_hContactShadowLeft.Get() )
	{
		C_PlayerFootContactShadow *pContactShadowLeft = new C_PlayerFootContactShadow;
		if ( pContactShadowLeft->InitializeAsClientEntity( "models/player/contactshadow/contactshadow_leftfoot.mdl", false ) )
		{
			pContactShadowLeft->m_hParentPlayer = this;
			m_hContactShadowLeft.Set( pContactShadowLeft );
			pContactShadowLeft->AddEffects( EF_NODRAW );
			pContactShadowLeft->AddEffects( EF_NOSHADOW );
		}
		else
		{
			pContactShadowLeft->Release();
		}
	}

	if ( !m_hContactShadowRight.Get() )
	{
		C_PlayerFootContactShadow *pContactShadowRight = new C_PlayerFootContactShadow;
		if ( pContactShadowRight->InitializeAsClientEntity( "models/player/contactshadow/contactshadow_rightfoot.mdl", false ) )
		{
			pContactShadowRight->m_hParentPlayer = this;
			m_hContactShadowRight.Set( pContactShadowRight );
			pContactShadowRight->AddEffects( EF_NODRAW );
			pContactShadowRight->AddEffects( EF_NOSHADOW );
		}
		else
		{
			pContactShadowRight->Release();
		}
	}
}

void C_CSPlayer::RemoveClientEffectModels( void )
{
	if ( m_hMuzzleFlashShape.Get() )
		UTIL_Remove( m_hMuzzleFlashShape.Get() );
	
	if ( m_hContactShadowLeft.Get() )
		UTIL_Remove( m_hContactShadowLeft.Get() );

	if ( m_hContactShadowRight.Get() )
		UTIL_Remove( m_hContactShadowRight.Get() );
}

C_CSPlayer::~C_CSPlayer()
{
	MDLCACHE_CRITICAL_SECTION();

	RemoveAddonModels();

	for ( int hh = 0 ; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		ReleaseFlashlight();
	}

	if ( m_PlayerAnimState )
		m_PlayerAnimState->Release();
	if ( m_PlayerAnimStateCSGO )
		m_PlayerAnimStateCSGO->Release();

	m_freezeCamSpotLightTexture.Shutdown();

	RemoveC4Effect( false );

	UpdateRadioHeadIcon( false );
	RemoveClientEffectModels();
}

class CTraceFilterOmitPlayers : public CTraceFilterSimple
{
public:
	CTraceFilterOmitPlayers( const IHandleEntity *passentity = NULL, int collisionGroup = MASK_SHOT )
		: CTraceFilterSimple( passentity, collisionGroup )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pEntity = EntityFromEntityHandle( pHandleEntity );
		if ( !pEntity )
			return NULL;

		if ( pEntity->IsPlayer() )
			return false;

		// Honor BlockLOS - this lets us see through partially-broken doors, etc
		if ( !pEntity->BlocksLOS() )
			return false;

		return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
	}
};

bool UTIL_clipwalls_hitvalid( const trace_t &tr )
{
	return ( tr.DidHit() && !tr.m_pEnt->IsPlayer() && !(tr.surface.flags & SURF_TRANS) );
}

void UTIL_clipwalls_debugline( const trace_t &tr )
{
	if ( tr.DidHit() )
	{
		debugoverlay->AddLineOverlay( tr.startpos, tr.endpos, 255,0,0, false, 0 );
	}
	else
	{
		debugoverlay->AddLineOverlay( tr.startpos, tr.endpos, 0,255,0, false, 0 );
	}
}

ConVar cl_weapon_clip_thinwalls("cl_weapon_clip_thinwalls", "1", FCVAR_CHEAT | FCVAR_REPLICATED );
ConVar cl_weapon_clip_thinwalls_debug("cl_weapon_clip_thinwalls_debug", "0", FCVAR_CHEAT | FCVAR_REPLICATED );
ConVar cl_weapon_clip_thinwalls_lock( "cl_weapon_clip_thinwalls_lock", "0", FCVAR_CHEAT | FCVAR_REPLICATED );
float *	C_CSPlayer::GetRenderClipPlane( void )
{
	// Players that are on the other side of thin surfaces like doors poke through.
	// This experimental solution places a clipping plane at the extent of a player's 
	// weapon, to try and keep the renderable part of the player behind thin obstacles 
	// in worst-case situations.

	if ( IsAnimLODflagSet(ANIMLODFLAG_OUTSIDEVIEWFRUSTUM|ANIMLODFLAG_INVISIBLELOCALPLAYER) ||
		!m_bUseNewAnimstate || !cl_weapon_clip_thinwalls.GetBool() || IsDormant() || !ShouldDraw() || GetMoveType() == MOVETYPE_LADDER )
	{
		m_bCachedPlaneIsValid = false;
		return NULL;
	}

	if ( cl_weapon_clip_thinwalls_lock.GetBool() )
		return m_fRenderingClipPlane;

	if ( m_nLastClipPlaneSetupFrame == gpGlobals->framecount && m_bCachedPlaneIsValid ) // we already computed a plane this frame, it's still good
		return m_fRenderingClipPlane;

	CWeaponCSBase* pActiveWeapon = GetActiveCSWeapon();
	if ( pActiveWeapon )
	{
		CBaseWeaponWorldModel *pWeaponWorldModel = pActiveWeapon->GetWeaponWorldModel();
		if ( pWeaponWorldModel && !pWeaponWorldModel->HasDormantOwner() )
		{

			if ( !pWeaponWorldModel->IsEffectActive( EF_BONEMERGE ) )
			{
				m_bCachedPlaneIsValid = false;
				return NULL;
			}

			Vector vecEyePosition = EyePosition();

			Vector vecEyeForward;
			Vector vecEyeRight;
			Vector vecEyeUp;
			AngleVectors( EyeAngles(), &vecEyeForward, &vecEyeRight, &vecEyeUp );

			if ( m_bCachedPlaneIsValid )
			{
				// The player is allowed to move a tiny amount relative to the cached plane, so this additional
				// fallback check is to make sure that the cached plane is still never too close to the center of the player
				float flDistFromCachedPlane = DotProduct( vecEyePosition, Vector(m_fRenderingClipPlane[0], m_fRenderingClipPlane[1], m_fRenderingClipPlane[2]) ) - m_fRenderingClipPlane[3];

				// if too close for any reason, invalidate it
				if ( flDistFromCachedPlane < 15 )
					m_bCachedPlaneIsValid = false;
			}
			

			if ( m_bClipHitStaticWorld && // the clipping plane isn't going to change on us, since we built it when we hit the static world, not a door or some other dynamic object
				 m_bCachedPlaneIsValid && 
				 m_pClippingWeaponWorldModel && 
				 m_pClippingWeaponWorldModel == pWeaponWorldModel &&
				 vecEyePosition.DistToSqr( m_vecLastClipCameraPos ) < 2 && 
				 vecEyeForward.DistToSqr( m_vecLastClipCameraForward ) < 0.0005f )
			{
				return m_fRenderingClipPlane; // the position, angle, and weapon we used to recently compute the plane are still valid and close enough to re-use.
			}

			m_pClippingWeaponWorldModel = pWeaponWorldModel;
			m_vecLastClipCameraPos = vecEyePosition;
			m_vecLastClipCameraForward = vecEyeForward;


			CSWeaponType wepType = pActiveWeapon->GetWeaponType();

			QAngle angMuzzleAngle;
			Vector vecWeaponMuzzle;
			if ( pWeaponWorldModel->IsVisible() && wepType != WEAPONTYPE_KNIFE && wepType < WEAPONTYPE_C4 )
			{
				int iMuzzleBoneIndex = pWeaponWorldModel->GetMuzzleBoneIndex();
				if ( iMuzzleBoneIndex < 0 )
				{
					m_bCachedPlaneIsValid = false;
					return NULL;
				}

				CStudioHdr *pHdr = pWeaponWorldModel->GetModelPtr();
				if ( pHdr )
				{
					int nRightHandWepBoneIndex = LookupBone( "weapon_hand_R" );
					if ( nRightHandWepBoneIndex > 0 && isBoneAvailableForRead( nRightHandWepBoneIndex ) )
					{
						matrix3x4a_t matHand = GetBone( nRightHandWepBoneIndex );
						vecWeaponMuzzle = VectorTransform( pHdr->pBone(iMuzzleBoneIndex)->pos, matHand );
						VectorAngles( matHand.GetForward(), angMuzzleAngle );
					}
					else
					{
						m_bCachedPlaneIsValid = false;
						return NULL;
					}
				}
				else
				{
					m_bCachedPlaneIsValid = false;
					return NULL;
				}
			}
			else
			{
				vecWeaponMuzzle = vecEyePosition + (vecEyeForward * 32);
				VectorAngles( vecEyeForward, angMuzzleAngle );
			}
			
			// It's tempting to bail when the muzzle is simply embedded in solid. But this fails in a rather common case:
			// When a player behind a thin door points their weapon through the corner of the door in such a way that the 
			// muzzle would poke into the door frame after having poked through the door.
			//int nMuzzlePointContents = enginetrace->GetPointContents(vecWeaponMuzzle);
			//if ( nMuzzlePointContents & CONTENTS_SOLID )
			//{
			//	return NULL;
			//}
			
			Vector vecEyeToWorldMuzzle = vecWeaponMuzzle - vecEyePosition;
			float flEyeToWorldMuzzleLength = vecEyeToWorldMuzzle.Length();

			if ( flEyeToWorldMuzzleLength > 128 )
			{
				m_bCachedPlaneIsValid = false;
				return NULL;
			}

			// the weapon is pointing away from the flat eye direction, don't clip
			Vector vecEyeForwardFlat = Vector( vecEyeForward.x, vecEyeForward.y, 0 ).Normalized();
			float flDotEyeToMuzzleByEyeForward = DotProduct( Vector( vecEyeToWorldMuzzle.x, vecEyeToWorldMuzzle.y, 0 ).Normalized(), vecEyeForwardFlat );
			if ( flDotEyeToMuzzleByEyeForward < 0 )
			{
				m_bCachedPlaneIsValid = false;
				return NULL;
			}

			Vector vecWeaponForward;
			AngleVectors( angMuzzleAngle, &vecWeaponForward );
			Vector vecWeaponRear = vecWeaponMuzzle - vecWeaponForward * flEyeToWorldMuzzleLength;

			vecWeaponMuzzle += vecWeaponForward; // move the muzzle test point forward by 1 unit

			CTraceFilterOmitPlayers traceFilter;
			traceFilter.SetPassEntity( this );

			trace_t tr_StockToMuzzle;
			UTIL_TraceLine( vecWeaponRear, vecWeaponMuzzle, MASK_WEAPONCLIPPING, &traceFilter, &tr_StockToMuzzle );

			if ( UTIL_clipwalls_hitvalid(tr_StockToMuzzle) )
			{
				
				// Something solid, non-player, and non-translucent is in the way of the gun. At this point we check some
				// edge cases to prevent odd special cases, like preventing the weapon from clipping due to narrow objects
				// like signposts, railing, or ladder rungs.

				if ( tr_StockToMuzzle.m_pEnt )
					m_bClipHitStaticWorld = tr_StockToMuzzle.m_pEnt->IsWorld();

				bool bClipHitPropDoor = false;
				if ( !m_bClipHitStaticWorld )
				{
					C_BasePropDoor *pPropDoor = dynamic_cast<C_BasePropDoor*>(tr_StockToMuzzle.m_pEnt);
					if ( pPropDoor )
					{
						bClipHitPropDoor = true;
					}
				}

				// Can the player see the muzzle position from their eye position?
				trace_t tr_EyeToMuzzle;
				UTIL_TraceLine( vecEyePosition, vecWeaponMuzzle, MASK_WEAPONCLIPPING, &traceFilter, &tr_EyeToMuzzle );

				if ( !UTIL_clipwalls_hitvalid(tr_EyeToMuzzle) )
				{
					// The player CAN see the muzzle position from their eye position. Don't clip the weapon.
					if ( cl_weapon_clip_thinwalls_debug.GetBool()  )
					{
						UTIL_clipwalls_debugline( tr_StockToMuzzle );
						UTIL_clipwalls_debugline( tr_EyeToMuzzle );
					}
					m_bCachedPlaneIsValid = false;
					return NULL;
				}

				// if the weapon direction is wildly different from the eye direction, don't clip
				if ( DotProduct( vecEyeForward, vecWeaponForward ) < 0.3 )
				{
					m_bCachedPlaneIsValid = false;
					return NULL;
				}

				// Can the player see the muzzle position from a point to the left of their eye position?
				trace_t tr_LeftOfEyeToMuzzle;
				UTIL_TraceLine( vecEyePosition - (vecEyeRight * 16), vecWeaponMuzzle, MASK_WEAPONCLIPPING, &traceFilter, &tr_LeftOfEyeToMuzzle );

				if ( !UTIL_clipwalls_hitvalid(tr_LeftOfEyeToMuzzle) )
				{
					// Yes, don't clip the weapon.
					if ( cl_weapon_clip_thinwalls_debug.GetBool()  )
					{
						UTIL_clipwalls_debugline( tr_StockToMuzzle );
						UTIL_clipwalls_debugline( tr_EyeToMuzzle );
						UTIL_clipwalls_debugline( tr_LeftOfEyeToMuzzle );
					}
					m_bCachedPlaneIsValid = false;
					return NULL;
				}

				// Can a point to the left of the muzzle see the side of the player's head?
				trace_t tr_LeftOfMuzzleToEye;
				UTIL_TraceLine( vecWeaponMuzzle - (vecEyeRight * 16), vecEyePosition - (vecEyeRight * 4), MASK_WEAPONCLIPPING, &traceFilter, &tr_LeftOfMuzzleToEye );

				if ( !UTIL_clipwalls_hitvalid( tr_LeftOfMuzzleToEye ) )
				{
					// Yes, don't clip the weapon.
					if ( cl_weapon_clip_thinwalls_debug.GetBool() )
					{
						UTIL_clipwalls_debugline( tr_StockToMuzzle );
						UTIL_clipwalls_debugline( tr_EyeToMuzzle );
						UTIL_clipwalls_debugline( tr_LeftOfEyeToMuzzle );
						UTIL_clipwalls_debugline( tr_LeftOfMuzzleToEye );
					}
					m_bCachedPlaneIsValid = false;
					return NULL;
				}

				// We also want to avoid the situation where a player can see and fire around a corner, but their weapon doesn't appear.
				// If a point well ahead of the player in-line with where they're looking at can see the muzzle, we shouldn't clip.

				trace_t tr_EyeToTarget;
				UTIL_TraceLine( vecEyePosition, vecEyePosition + vecEyeForward * 80, MASK_WEAPONCLIPPING, &traceFilter, &tr_EyeToTarget );

				trace_t tr_TargetToMuzzle;
				UTIL_TraceLine( tr_EyeToTarget.endpos - vecEyeForward, vecWeaponMuzzle, MASK_WEAPONCLIPPING, &traceFilter, &tr_TargetToMuzzle );

				if ( cl_weapon_clip_thinwalls_debug.GetBool()  )
				{
					UTIL_clipwalls_debugline( tr_StockToMuzzle );
					UTIL_clipwalls_debugline( tr_EyeToMuzzle );
					UTIL_clipwalls_debugline( tr_LeftOfEyeToMuzzle );
					UTIL_clipwalls_debugline( tr_LeftOfMuzzleToEye );
					UTIL_clipwalls_debugline( tr_EyeToTarget );
					UTIL_clipwalls_debugline( tr_TargetToMuzzle );
				}

				if ( !UTIL_clipwalls_hitvalid(tr_TargetToMuzzle) )
				{
					// The spot the player is looking at can see the muzzle of the weapon. Don't clip the weapon.
					m_bCachedPlaneIsValid = false;
					return NULL;
				}
				

				// Should be reasonable to clip the weapon now.
				

				// can't clip in the fast path?
				pWeaponWorldModel->SetAllowFastPath( false );

				if ( m_nClipPlaneProximityLimitAttachmentIndex == -1 )
				{
					m_nClipPlaneProximityLimitAttachmentIndex = -2;
					m_nClipPlaneProximityLimitAttachmentIndex = LookupAttachment( "clip_limit" );
				}
				
				Vector vecEyePosFlat = Vector( vecEyePosition.x, vecEyePosition.y, 0 );
				Vector vecClipPosFlat = Vector( tr_TargetToMuzzle.endpos.x, tr_TargetToMuzzle.endpos.y, 0 );

				Vector vecClipLimit = vecClipPosFlat;
				if ( !bClipHitPropDoor && m_nClipPlaneProximityLimitAttachmentIndex >= 0 )
				{
					GetAttachment( m_nClipPlaneProximityLimitAttachmentIndex, vecClipLimit );

					if ( cl_weapon_clip_thinwalls_debug.GetBool()  )
						debugoverlay->AddBoxOverlay( vecClipLimit, Vector(-1,-1,-1), Vector(1,1,1), QAngle(0,0,0), 255,0,0, 0, 0 );

					vecClipLimit.z = 0;
					if ( vecClipLimit.DistToSqr( vecEyePosFlat ) > vecClipPosFlat.DistToSqr( vecEyePosFlat ) )
					{
						vecClipPosFlat = vecClipLimit;
					}
				}

				Vector vecSurfNormal = tr_StockToMuzzle.plane.normal;
				Vector vecFallBackNormal = -vecEyeForwardFlat;

				if ( abs(vecSurfNormal.z) >= 1 )
				{
					vecSurfNormal = vecFallBackNormal;
				}
				else
				{
					vecSurfNormal.z = 0;
					vecSurfNormal.NormalizeInPlaceSafe( vecFallBackNormal );
				}
				
				float flClipPosDistSqr = (vecClipPosFlat - vecEyePosFlat).LengthSqr();

				if ( flClipPosDistSqr < (16*16) )
				{
					vecClipPosFlat = vecEyePosFlat + (vecClipPosFlat - vecEyePosFlat).Normalized() * 16.0f;
				}

				// build a plane around world pos vecClipPosFlat with local normal vecSurfNormal

				matrix3x4_t matTemp;
				VectorMatrix( vecSurfNormal, matTemp );
				matTemp.SetOrigin( vecClipPosFlat );

				cplane_t planeClipLocal;
				planeClipLocal.normal = Vector(1,0,0);
				planeClipLocal.dist = 0;

				cplane_t planeClipWorld;
				MatrixTransformPlane( matTemp, planeClipLocal, planeClipWorld );

				float flDistFromPlane = DotProduct( vecEyePosFlat, planeClipWorld.normal ) - planeClipWorld.dist;

				// Something is catastrophically wrong for the plane to be this far away.
				if ( flDistFromPlane > 128 )
				{
					Assert( false );
					m_bCachedPlaneIsValid = false;
					return NULL;
				}

				if ( flDistFromPlane < 16.0f )
				{
					planeClipWorld.dist -= (16.0f - flDistFromPlane);
				}

				float flDistFromLimit = DotProduct( vecClipLimit, planeClipWorld.normal ) - planeClipWorld.dist;
				if ( flDistFromLimit < 0 )
				{
					planeClipWorld.dist += flDistFromLimit;
				}

				m_fRenderingClipPlane[0] = planeClipWorld.normal.x;
				m_fRenderingClipPlane[1] = planeClipWorld.normal.y;
				m_fRenderingClipPlane[2] = planeClipWorld.normal.z;
				m_fRenderingClipPlane[3] = planeClipWorld.dist;

				if ( cl_weapon_clip_thinwalls_debug.GetBool()  )
				{
					Vector vecCenter = VPlane( planeClipWorld.normal, planeClipWorld.dist ).SnapPointToPlane( vecEyePosition );

					// draw a grid of the local clip position, projected onto the final clipping plane
					for ( float nLine=0; nLine<1.05f; nLine += 0.1f )
					{
						Vector vecLineStart = Vector( 0, -30, 60 * nLine - 30 );
						Vector vecLineEnd = Vector( 0, 30, 60 * nLine - 30 );

						Vector vecLineStart2 = Vector( 0, 60 * nLine - 30, -30 );
						Vector vecLineEnd2 = Vector( 0, 60 * nLine - 30, 30 );

						QAngle angAngle;
						VectorAngles( planeClipWorld.normal.Normalized(), angAngle );

						VectorRotate( vecLineStart, angAngle, vecLineStart );
						VectorRotate( vecLineEnd, angAngle, vecLineEnd );

						VectorRotate( vecLineStart2, angAngle, vecLineStart2 );
						VectorRotate( vecLineEnd2, angAngle, vecLineEnd2 );

						vecLineStart += vecCenter;
						vecLineEnd += vecCenter;

						vecLineStart2 += vecCenter;
						vecLineEnd2 += vecCenter;

						debugoverlay->AddLineOverlay( vecLineStart, vecLineEnd, 255, 255, 0, false, 0 );
						debugoverlay->AddLineOverlay( vecLineStart2, vecLineEnd2, 255, 255, 0, false, 0 );
					}
				}

				m_nLastClipPlaneSetupFrame = gpGlobals->framecount;

				m_bCachedPlaneIsValid = true;
				return m_fRenderingClipPlane;

			}
			else
			{
				if ( cl_weapon_clip_thinwalls_debug.GetBool()  )
				{
					UTIL_clipwalls_debugline( tr_StockToMuzzle );
				}
				m_bCachedPlaneIsValid = false;
				return NULL;
			}
		}
	}
	m_bCachedPlaneIsValid = false;
	return NULL;
}

ConVar thirdperson_lockcamera("thirdperson_lockcamera", "0", FCVAR_CHEAT | FCVAR_REPLICATED );
Vector C_CSPlayer::GetThirdPersonViewPosition( void )
{
	if ( !thirdperson_lockcamera.GetBool() )
	{
		m_vecThirdPersonViewPositionOverride = BaseClass::GetThirdPersonViewPosition();
		return m_vecThirdPersonViewPositionOverride;
	}
	return m_vecThirdPersonViewPositionOverride;
}

//=============================================================================
// HPE_BEGIN:
// [dwenger] Added for auto-buy functionality
//=============================================================================
void C_CSPlayer::FireGameEvent( IGameEvent *event )
{
	const char *name = event->GetName();
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

	int EventUserID = event->GetInt( "userid", -1 );
	//int LocalPlayerID = ( pLocalPlayer != NULL ) ? pLocalPlayer->GetUserID() : -2;
	int PlayerUserID = GetUserID();

	if ( Q_strcmp( name, "round_freeze_end" ) == 0 )
	{
		m_bShouldAutobuyNow = false;
		m_bShouldAutobuyDMWeapons = false;

		if ( IsLocalPlayer() )
		{
			CHudElement *pElement = GetHud().FindElement( "SFHudInfoPanel" );
			C_CS_PlayerResource *pCSRes = GetCSResources();
			CEconQuestDefinition *pQuest = CSGameRules()->GetActiveAssassinationQuest();
			if ( pElement && pCSRes && pQuest )
			{
				wchar_t szBuf[ 512 ];
				const char *szAlertToken = NULL;
				if ( IsAssassinationTarget() )
					szAlertToken = "#quest_assassination_you_are_target";
				else if ( GetActiveQuestID() == pQuest->GetID() && (int)pQuest->GetTargetTeam() != GetTeamNumber() )
					szAlertToken = "#quest_assassination_target_on_server_has_quest";
				else
					szAlertToken = "#quest_assassination_target_on_server";

				g_pVGuiLocalize->ConstructString( szBuf, sizeof( szBuf ), g_pVGuiLocalize->Find( szAlertToken ), 1, g_pVGuiLocalize->Find( Helper_GetLocalPlayerAssassinationQuestLocToken( pQuest ) ) );

				( ( SFHudInfoPanel * ) pElement )->SetPriorityHintText( szBuf );
			}
		}
		
	}
	else if ( Q_strcmp( name, "cs_pre_restart" ) == 0 )
	{

		// this is a hack because GetHudPlayer seems to return the previously spectated player instead of the newly spawned 
// 			C_CSPlayer *pHudPlayer = this;
// 			if( this->GetTeamNumber() == TEAM_SPECTATOR )
// 			{
// 				pHudPlayer = GetHudPlayer();
// 			}
		CLocalPlayerFilter filter;
		if( CSGameRules()->IsPlayingGunGame() )
		{
			if ( IsLocalPlayer() )
			{
				PlayMusicSelection( filter, CSMUSIC_STARTGG );
			}
		}
		else if( CSGameRules()->IsPlayingCoopMission() )
		{
		}
		else
		{					
			if( ( this->GetTeamNumber() == TEAM_SPECTATOR ) || ( this->IsLocalPlayer() ) )
			{
				PlayMusicSelection(filter, CSMUSIC_START);
			}

			SetCurrentMusic( CSMUSIC_START );
		}
	}
	else if ( (Q_strcmp( "bot_takeover", name ) == 0 || Q_strcmp( "spec_target_updated", name ) == 0) && PlayerUserID == EventUserID )
	{
		UpdateGlowsForAllPlayers();

		C_CSPlayer *pLocalCSPlayer = C_CSPlayer::GetLocalCSPlayer();
		if (pLocalCSPlayer)
		{
			// FlashBang effect needs to update it's screenshot
			pLocalCSPlayer->m_bFlashScreenshotHasBeenGrabbed = false;

			//reset smoke alpha
			m_flLastSmokeOverlayAlpha = 0;
			
			if ( pLocalPlayer->GetObserverMode() != OBS_MODE_NONE && pLocalPlayer->GetUserID() == EventUserID )
			{
				C_CSPlayer *pFlashBangPlayer = GetHudPlayer();
				if ( pFlashBangPlayer )
				{
					pFlashBangPlayer->m_bFlashScreenshotHasBeenGrabbed = false;
				}
			}
		}
	}
	else if ( Q_strcmp( "gg_reset_round_start_sounds", name ) == 0 )
	{
		if ( PlayerUserID == EventUserID )
		{
			// Clear out the queue of round start sound events
			m_StartOfRoundSoundEvents.ClearSounds();
		}
	}
	else if ( Q_strcmp( "ggtr_player_levelup", name ) == 0 )
	{
		// commenting all of this out because the level up sound is associated with killing a player instead of actually leveling up
		// playing the ding when it's not associated with killing someone is causing a bunch of confusion and is percieved as a bug
		// in TR mode, we don't need any sound played when you've leveled up at all
		/*
		// Let the local player know he leveled up
		if ( PlayerUserID == EventUserID )
		{
			// Play level-up gun game sound
			C_RecipientFilter filter;
			filter.AddRecipient( this );
			//C_BaseEntity::EmitSound( filter, entindex(), "GunGameWeapon.LevelUp" );
			m_StartOfRoundSoundEvents.AddSound( this, "GunGameWeapon.LevelUp", 0.10f );

			// Play weapon name voiceover after a delay
//			const char* pWeaponName = event->GetString( "weaponname" );
// 			if ( pWeaponName )
// 			{
// 				char weaponSoundName[64];
// 				Q_snprintf( weaponSoundName, 64, "GunGameWeapon.%s", pWeaponName );
// 
// 				m_StartOfRoundSoundEvents.AddSound( this, weaponSoundName, 2.0f );
// 			}
		}
		*/
	}
	else if ( Q_strcmp( "ggprogressive_player_levelup", name ) == 0 )
	{
		// Let the local player know he leveled up
		if ( PlayerUserID == EventUserID )
		{
			// Play level-up gun game sound
			C_RecipientFilter filter;
			filter.AddRecipient( this );
			C_BaseEntity::EmitSound( filter, entindex(), "GunGameWeapon.LevelUp" );
			//C_BaseEntity::EmitSound( filter, entindex(), "UI.DeathMatchBonusKill" );		

			// Play weapon name voiceover immediately
//			const char* pWeaponName = event->GetString( "weaponname" );
// 			if ( pWeaponName )
// 			{
// 				char weaponSoundName[64];
// 				Q_snprintf( weaponSoundName, 64, "GunGameWeapon.%s", pWeaponName );
// 
// 				EmitSound( weaponSoundName );
// 			}
		}
	}
	else if ( Q_strcmp( "gg_player_impending_upgrade", name ) == 0 )
	{
		// Let the local player know a level-up is impending
		if ( PlayerUserID == EventUserID )
		{
			// Play level-up gun game sound
			C_RecipientFilter filter;
			filter.AddRecipient( this );
			C_BaseEntity::EmitSound( filter, entindex(), "GunGameWeapon.ImpendingLevelUp" );
		}
	}
	else if ( Q_strcmp( "gg_killed_enemy", name ) == 0 )
	{
		if ( CSGameRules()->IsPlayingGunGame() )
		{
			FirePerfStatsEvent( PERF_STATS_PLAYER );
			if ( pLocalPlayer && pLocalPlayer->GetUserID() == event->GetInt( "attackerid" ) )
			{
// 				if ( event->GetInt( "dominated" ) == 1 )
// 				{
// 					// Play gun game domination sound
// 					C_RecipientFilter filter;
// 					filter.AddRecipient( this );
// 					C_BaseEntity::EmitSound( filter, entindex(), "Music.GG_Dominating" );
//				}

				if ( event->GetInt( "revenge" ) == 1 )
				{
					// Play gun game revenge sound
					C_RecipientFilter filter;
					filter.AddRecipient( this );
					C_BaseEntity::EmitSound( filter, entindex(), "Music.GG_Revenge" );
					STEAMWORKS_TESTSECRETALWAYS_AMORTIZE( 7 );
				}
				if ( event->GetInt( "bonus" ) != 0 )
				{
					C_RecipientFilter filter;
					filter.AddRecipient( this );
					C_BaseEntity::EmitSound( filter, entindex(), "UI.DeathMatchBonusKill" );
				}
				else if ( CSGameRules()->IsPlayingGunGameDeathmatch() || CSGameRules()->IsPlayingGunGameProgressive() )
				{
					// Play level-up gun game sound because it's a better kill sound than the default one.
					C_RecipientFilter filter;
					filter.AddRecipient( this );
					C_BaseEntity::EmitSound( filter, entindex(), "GunGameWeapon.ImpendingLevelUp" );
				}
			}
// 			else if ( pLocalPlayer && pLocalPlayer->GetUserID() == event->GetInt( "victimid" ) )
// 			{
// 				if ( event->GetInt( "dominated" ) == 1 )
// 				{
// 					// Play gun game nemesis sound
// 					C_RecipientFilter filter;
// 					filter.AddRecipient( this );
// 					C_BaseEntity::EmitSound( filter, entindex(), "GunGameWeapon.Nemesis" );
// 				}
// 			}
		}
	}
	else if ( Q_strcmp( "gg_final_weapon_achieved", name ) == 0 )
	{
		int nGoldKnifeUserID = event->GetInt( "playerid", -1 );
		if ( CSGameRules()->IsPlayingGunGameProgressive() && nGoldKnifeUserID == pLocalPlayer->GetUserID() )
		{
			// Play an audio cue corresponding to getting the final weapon
			//EmitSound( "GunGameWeapon.AchievedFinalWeapon" );
			GetCenterPrint()->Print( "#SFUI_Notice_Knife_Level_You" );

			STEAMWORKS_TESTSECRETALWAYS_AMORTIZE( 13 );
		}
		else
		{
			C_BasePlayer *pGoldKnifeUser = static_cast<CBasePlayer *>( UTIL_PlayerByUserId( nGoldKnifeUserID ) );
			if ( pGoldKnifeUser )
			{
				wchar_t wszLocalized[100];
				wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
				g_pVGuiLocalize->ConvertANSIToUnicode( pGoldKnifeUser->GetPlayerName(), wszPlayerName, sizeof(wszPlayerName) );
				g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#SFUI_Notice_Knife_Level" ), 1, wszPlayerName );

				GetCenterPrint()->Print( wszLocalized );
				EmitSound("GunGame.PlayerReachedKnife");
			}
		}
	}
	else if ( Q_strcmp( "gg_bonus_grenade_achieved", name ) == 0 )
	{
		if ( CSGameRules()->IsPlayingGunGameTRBomb() )
		{
			if ( PlayerUserID == EventUserID )
			{
				// Play the bonus grenade voiceover after next round start
				m_StartOfRoundSoundEvents.AddSound( this, "GunGameWeapon.AchievedBonusGrenade", 2.0f );

				STEAMWORKS_TESTSECRETALWAYS_AMORTIZE( 23 );
			}
		}
	}
	else if ( Q_strcmp( "player_given_c4", name ) == 0 )
	{
		FirePerfStatsEvent( PERF_STATS_PLAYER );
		if ( PlayerUserID == EventUserID )
		{
			// Play the voiceover for receiving the c4
			STEAMWORKS_TESTSECRETALWAYS_AMORTIZE( 13 );
		}
	}
	else if ( Q_strcmp( "item_pickup", name ) == 0 )
	{
		FirePerfStatsEvent( PERF_STATS_PLAYER );
		if ( /*Q_strcmp( event->GetString( "item" ), "c4" ) == 0 &&*/
			( pLocalPlayer && pLocalPlayer->GetUserID() == EventUserID ) )
		{
			// if we aren't playing the sound on the server, play a "silent" version on the client
			if ( event->GetBool( "silent" ) )
				EmitSound( "Player.PickupWeaponSilent" );

			STEAMWORKS_TESTSECRET_AMORTIZE( 67 );
		}
	}
	else if ( Q_strcmp( "ammo_pickup", name ) == 0 )
	{
		FirePerfStatsEvent( PERF_STATS_PLAYER );
		// this is to catch the case where a grenade was just thrown and we picked up another grenade immediately before the one iun our inventory has a chance to remove itself
		// what happens here is that the one we just picked up adds to the "ammo" of the one that we have and we then remove the one that we picked up
		C_CSPlayer *pObservedPlayer = GetHudPlayer();

		// check if weapon was dropped by local player or the player we are observing
		if ( pLocalPlayer && pObservedPlayer->GetUserID() == EventUserID )
		{
			C_WeaponCSBase *pWeapon = dynamic_cast<C_WeaponCSBase*>( ClientEntityList().GetEnt( event->GetInt( "index" ) ) );
			if ( pWeapon && pWeapon->ShouldDrawPickup() )
			{
				CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
				if ( pHudSelection )
				{
					pHudSelection->OnWeaponPickup( pWeapon );
				}
			}
		}
	}
	else if ( Q_strcmp( "gg_leader", name ) == 0 )
	{
		if ( CSGameRules()->IsPlayingGunGameProgressive() || CSGameRules()->IsPlayingGunGameDeathmatch() )
		{	
			if ( GetUserID() == event->GetInt("playerid" ) )
			{		
				STEAMWORKS_TESTSECRETALWAYS_AMORTIZE( 11 );

				/*
				if ( !m_isCurrentGunGameLeader )
				{
					if ( this == C_BasePlayer::GetLocalPlayer() )
					{
						GetCenterPrint()->Print( "#SFUI_Notice_Gun_Game_Leader" );
					}

					//m_isCurrentGunGameLeader = true;
					//m_isCurrentGunGameTeamLeader = true;
				}
				*/
			}
			else
			{
				//m_isCurrentGunGameLeader = false;
				//m_isCurrentGunGameTeamLeader = false;
			}

			// update the glow so it's up-to-date
			m_fNextGlowCheckUpdate = gpGlobals->curtime;
			m_fNextGlowCheckInterval = 0.1f;
			UpdateGlows();
		}
	}
	else if ( Q_strcmp( "gg_team_leader", name ) == 0 )
	{
		if ( CSGameRules()->IsPlayingGunGameProgressive() )
		{

			// update the glow so it's up-to-date
			m_fNextGlowCheckUpdate = gpGlobals->curtime;
			m_fNextGlowCheckInterval = 0.1f;
			UpdateGlows();
		}
	}
	else if ( Q_strcmp( "round_start", name ) == 0 )
	{
		//m_isCurrentGunGameLeader = false;
		//m_isCurrentGunGameTeamLeader = false;
		if( IsLocalPlayer( ) && !IsBot( ) )
		{
			ConVarRef round_start_reset_duck( "round_start_reset_duck" );
			round_start_reset_duck.SetValue( true );
			ConVarRef round_start_reset_speed( "round_start_reset_speed" );
			round_start_reset_speed.SetValue( true );	
		}

		UpdateGlowsForAllPlayers();

		// Enable playback of the start of round sound events
		m_StartOfRoundSoundEvents.PlaySounds();

		if ( IsLocalPlayer() && CSGameRules() && CSGameRules()->IsPlayingGunGameDeathmatch() )
			m_bShouldAutobuyDMWeapons = true;

		m_totalHitsOnClient = 0;
		m_isCurrentGunGameTeamLeader = false;
	}
	else if ( Q_strcmp( "round_end", name ) == 0 )
	{
		if ( IsLocalPlayer() )
		{
				// This code allowed us to measure discrepency between client and server bullet hits.
				// It became obsolete when we started using a separate seed for client and server
				// to eliminate 'rage' hacks.
				//
			CompareClientServerBulletHits();

			if ( pLocalPlayer->IsAlive() )
				RecordAmmoForRound();
		}
	}
	else if ( Q_strcmp( "player_death", name ) == 0 )
	{
		FirePerfStatsEvent( PERF_STATS_PLAYER, 1 ); // don't look ahead: after the player is dead, framerate doesn't matter
		C_BasePlayer *pPlayer = UTIL_PlayerByUserId( event->GetInt( "userid" ) );
		C_CSPlayer* csPlayer = ToCSPlayer( pPlayer );

		if ( csPlayer && pPlayer == C_BasePlayer::GetLocalPlayer() )
		{
			ConVarRef round_start_reset_duck( "round_start_reset_duck" );
			round_start_reset_duck.SetValue( true );
			ConVarRef round_start_reset_speed( "round_start_reset_speed" );
			round_start_reset_speed.SetValue( true );

			//reset target ID 
			m_iIDEntIndex = 0;
			m_delayTargetIDTimer.Reset();
			m_iOldIDEntIndex = 0;
			m_holdTargetIDTimer.Reset();
			m_iTargetedWeaponEntIndex = 0;

			////  data collection for ammo remaining at death. OGS
			RecordAmmoForRound();

			if ( IsLocalPlayer() )
			{
				if ( CSGameRules() && CSGameRules()->GetActiveAssassinationQuest() && IsAssassinationTarget() )
				{
					CHudElement *pElement = GetHud().FindElement( "SFHudInfoPanel" );
					C_CS_PlayerResource *pCSRes = GetCSResources();
					if ( pElement && pCSRes )
					{
						wchar_t szBuf[ 512 ];
						wchar_t wszName[ MAX_DECORATED_PLAYER_NAME_LENGTH ] = { };
						pCSRes->GetDecoratedPlayerName( entindex(), wszName, sizeof( wszName ), k_EDecoratedPlayerNameFlag_Simple );
						g_pVGuiLocalize->ConstructString( szBuf, sizeof( szBuf ), g_pVGuiLocalize->Find( "#quest_assassination_no_longer_target" ), 1, wszName );

						( ( SFHudInfoPanel * )pElement )->SetPriorityHintText( szBuf );
					}
				}
			}
		}
		if( CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && !g_HltvReplaySystem.GetHltvReplayDelay() )
		{
			if (csPlayer && csPlayer->IsLocalPlayer())
			{
				C_RecipientFilter filter;
				filter.AddRecipient( this );
				PlayMusicSelection( filter, CSMUSIC_DEATHCAM );
			}
		}	
		UpdateGlowsForAllPlayers();
	}

	else if ( Q_strcmp( "player_spawn", name ) == 0 )
	{
		FirePerfStatsEvent( PERF_STATS_PLAYER, 16, 1 );// look ahead, but not back
		if ( PlayerUserID == EventUserID )
		{
			// we've just spawned, so reset our entity id stuff
			m_iIDEntIndex = 0;
			m_delayTargetIDTimer.Reset();
			m_iOldIDEntIndex = 0;
			m_holdTargetIDTimer.Reset();
			m_iTargetedWeaponEntIndex = 0;
			m_flLastFiredWeaponTime = 0;

			m_nLastKillerDamageTaken = 0;
			m_nLastKillerHitsTaken = 0;
			m_nLastKillerDamageGiven = 0;
			m_nLastKillerHitsGiven = 0;

			m_flHealthFadeValue = 100.0f;
			m_flHealthFadeAlpha = 0.0f;

			RemoveC4Effect( false );
			
			CancelFreezeCamFlashlightEffect();

			CreateClientEffectModels();

			if ( IsLocalPlayer() && CSGameRules() && CSGameRules()->IsPlayingGunGameDeathmatch() )
				m_bShouldAutobuyDMWeapons = true;
			
			// 
			//if ( IsLocalPlayer() && !IsBot() )
			//{
			//	UI_COMPONENT_BROADCAST_EVENT( GameState, PlayerSpawning )
			//}

			ClearAllBulletHitModels();

			// This code allowed us to measure discrepency between client and server bullet hits.
			// It became obsolete when we started using a separate seed for client and server
			// to eliminate 'rage' hacks.
			//
			m_vecBulletVerifyListClient.RemoveAll();
			m_vecBulletVerifyListServer.RemoveAll();
			m_ui8ClientServerHitDifference = 0;

			UpdateAddonModels( true );

			m_flLastSpawnTimeIndex = gpGlobals->curtime;

			m_pViewmodelArmConfig = NULL;

			if ( m_bUseNewAnimstate && m_PlayerAnimStateCSGO )
			{
				m_PlayerAnimStateCSGO->Reset();
			}

		}

		if ( IsLocalPlayer() )
		{
			UpdateGlowsForAllPlayers();

			if ( CSGameRules() && CSGameRules()->IsPlayingGunGameProgressive() )
			{
				m_isCurrentGunGameTeamLeader = entindex() == GetGlobalTeam( GetTeamNumber() )->GetGGLeader( GetTeamNumber() );
			}
		}

	}
	else if ( Q_strcmp( "weapon_fire", name ) == 0 )
	{
		if ( PlayerUserID == EventUserID )
		{
			// we just fired our weapon
			m_flLastFiredWeaponTime = gpGlobals->curtime;
		}
	}
	else if ( Q_strcmp( "assassination_target_killed", name ) == 0 )
	{
		if ( CSGameRules() && CSGameRules()->GetActiveAssassinationQuest() && IsLocalPlayer() )
		{
			CHudElement *pElement = GetHud().FindElement( "SFHudInfoPanel" );
			C_CS_PlayerResource *pCSRes = GetCSResources();
			CEconQuestDefinition *pQuest = CSGameRules()->GetActiveAssassinationQuest();
			wchar_t wszName[ MAX_DECORATED_PLAYER_NAME_LENGTH ] = {};
			if ( pElement && pCSRes && Helper_GetDecoratedAssassinationTargetName( pQuest, wszName, ARRAYSIZE( wszName ) ) )
			{
				wchar_t szBuf[ 512 ];
				g_pVGuiLocalize->ConstructString( szBuf, sizeof( szBuf ), g_pVGuiLocalize->Find( "#quest_assassination_target_killed" ), 1, wszName );
				( ( SFHudInfoPanel * ) pElement )->SetPriorityHintText( szBuf );
			}
		}
	}
	else if ( Q_strcmp( "add_bullet_hit_marker", name ) == 0 )
	{
		//FirePerfStatsEvent( PERF_STATS_BULLET );
		if ( PlayerUserID == EventUserID )
		{
			int nBoneIndex = event->GetInt( "bone", 0 );

			Vector hitPos = Vector( event->GetFloat("pos_x",0), event->GetFloat("pos_y",0), event->GetFloat("pos_z",0) );
			QAngle hitAng = QAngle( event->GetFloat("ang_x",0), event->GetFloat("ang_y",0), event->GetFloat("ang_z",0) );
			Vector vecStartPos = Vector( event->GetFloat("start_x",0), event->GetFloat("start_y",0), event->GetFloat("start_z",0) );

			if ( nBoneIndex >= 0 && hitPos.IsValid() && hitAng.IsValid() )
			{
				VMatrix matLocalOffset = SetupMatrixOrgAngles( hitPos, hitAng );
				C_BulletHitModel *pBulletHitModel = new C_BulletHitModel();
				pBulletHitModel->AttachToPlayer( this, nBoneIndex, matLocalOffset.As3x4(), event->GetBool( "hit", false ), vecStartPos );
				m_vecBulletHitModels.AddToTail( pBulletHitModel );
				if ( m_vecBulletHitModels.Count() > 25 )
				{
					m_vecBulletHitModels[0]->Release();
					m_vecBulletHitModels.Remove(0);
				}
			}
		}
	}
}

// This code allowed us to measure discrepency between client and server bullet hits.
// It became obsolete when we started using a separate seed for client and server
// to eliminate 'rage' hacks.
//
bool __MsgFunc_ReportHit( const CCSUsrMsg_ReportHit &msg )
{
	if ( msg.has_pos_x() && msg.has_pos_y() && msg.has_pos_z() && msg.has_timestamp() )
	{
		Vector vecServerHitPos = Vector( msg.pos_x(), msg.pos_y(), msg.pos_z() );
		float flServerHitTime = msg.timestamp();

		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pPlayer )
		{
			C_CSPlayer* pCSPlayer = ToCSPlayer( pPlayer );

			if ( pCSPlayer)
			{
				pCSPlayer->m_vecBulletVerifyListServer.AddToTail( clientHitVerify_t( vecServerHitPos, flServerHitTime, gpGlobals->curtime + 10 ) );
			}

		}
	}

	return true;
}
USER_MESSAGE_REGISTER( ReportHit );

void C_CSPlayer::CompareClientServerBulletHits( void )
{
	bool bAllowVisDebug = false;

	AccountID_t uiLocalAccountID = 0;
	if ( steamapicontext && steamapicontext->SteamUser() )
		uiLocalAccountID = steamapicontext->SteamUser()->GetSteamID().GetAccountID();

	switch( uiLocalAccountID )
	{
	case 8186565:	// mattw
	case 158213:	// ido
	case 24715681:	// vitaliy
	case 101804581:	// will
	case 11134320:	// brianlev

		bAllowVisDebug = true;
	}

	
	// remove server hits that have matching hits on client
	FOR_EACH_VEC_BACK( m_vecBulletVerifyListServer, s )
	{
		FOR_EACH_VEC_BACK( m_vecBulletVerifyListClient, c )
		{
			if ( CloseEnough( m_vecBulletVerifyListServer[ s ].vecPosition, m_vecBulletVerifyListClient[ c ].vecPosition, 1.0f ) )
			{
				m_vecBulletVerifyListServer.Remove( s );
				break;
			}
		}
	}

	// record the unmatched server hits.
	m_ui8ClientServerHitDifference = MIN( 255, m_vecBulletVerifyListServer.Count() );

	m_vecBulletVerifyListServer.RemoveAll();
	m_vecBulletVerifyListClient.RemoveAll();

}


//=============================================================================
// HPE_END
//=============================================================================

static void ClientBuyHelperForwardToServer( char const *szCommand, char const *szParam )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;

	if ( engine->IsHLTV() )
		return;

	if ( !szParam )
	{
		// just forward the command without parameters
		engine->ServerCmd( szCommand );
	}
	else
	{
		// forward the command with parameter
		char command[ 256 ] = {};
		Q_snprintf( command, sizeof( command ), "%s \"%s\"", szCommand, szParam );
		engine->ServerCmd( command );
	}
}

CON_COMMAND_F( autobuy, "Attempt to purchase items with the order listed in cl_autobuy", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	extern ConVar cl_autobuy;
	ClientBuyHelperForwardToServer( "autobuy", cl_autobuy.GetString() );
}
CON_COMMAND_F( rebuy, "Attempt to repurchase items with the order listed in cl_rebuy", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	extern ConVar cl_rebuy;
	ClientBuyHelperForwardToServer( "rebuy", cl_rebuy.GetString() );
}

CON_COMMAND_F( dm_togglerandomweapons, "Turns random weapons in deathmatch on/off", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	C_CSPlayer* pPlayer = ToCSPlayer(pLocalPlayer);
	if ( pPlayer )
		pPlayer->ToggleRandomWeapons();
}

void C_CSPlayer::ToggleRandomWeapons( void )
{
	ConVarRef cl_dm_buyrandomweapons( "cl_dm_buyrandomweapons" );
	float flTimeLeft = m_fImmuneToGunGameDamageTime - gpGlobals->curtime;
	if ( cl_dm_buyrandomweapons.GetBool() )
	{
		cl_dm_buyrandomweapons.SetValue(false);
		if ( flTimeLeft <= 0 )
		{
			GetCenterPrint()->Print( "#SFUI_Notice_DM_RandomOFF" );
		}
	}
	else
	{
		cl_dm_buyrandomweapons.SetValue(true);
		if ( flTimeLeft <= 0 )
		{
			GetCenterPrint()->Print( "#SFUI_Notice_DM_RandomON" );
		}
		engine->ClientCmd_Unrestricted( "buyrandom" );
	}

	CLocalPlayerFilter filter;
	EmitSound( filter, GetSoundSourceIndex(), "BuyPreset.Updated" );
}

bool C_CSPlayer::ShouldShowTeamPlayerColors( int nOtherTeamNum )
{
	ConVarRef cl_teammate_colors_show( "cl_teammate_colors_show" );
	if ( cl_teammate_colors_show.GetBool( ) == false || engine->IsHLTV( ) )
		return false;

	if ( nOtherTeamNum != TEAM_CT && nOtherTeamNum != TEAM_TERRORIST )
		return false;

	int nMaxPlayers = CCSGameRules::GetMaxPlayers();
	
	bool bShowColor = (nMaxPlayers <= 10) && CSGameRules() && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && (nOtherTeamNum == GetTeamNumber());

	return bShowColor;
}

bool C_CSPlayer::ShouldShowTeamPlayerColorLetters( void )
{
	ConVarRef cl_teammate_colors_show( "cl_teammate_colors_show" );
	if ( cl_teammate_colors_show.GetInt() == 2 && engine->IsHLTV() == false )
		return true;

	return false;
}

static void ForwardEndMatchVoteNextMapToServer( const CCommand &args )
{
	if ( engine->IsPlayingDemo() )
		return;

	if ( args.ArgC() == 1 )
	{
		// just forward the command without parameters
		engine->ServerCmd( args[ 0 ] );
	}
	else if ( args.ArgC() == 2 )
	{
		// forward the command with parameter
		char command[128];
		Q_snprintf( command, sizeof(command), "%s \"%s\"", args[ 0 ], args[ 1 ] );
		engine->ServerCmd( command );
	}
}

CON_COMMAND_F( endmatch_votenextmap, "Votes for the next map at the end of the match", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer || g_bEngineIsHLTV || pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		return;

	if ( args.ArgC() != 2 )
		return;

	ForwardEndMatchVoteNextMapToServer( args );
}

void C_CSPlayer::UpdateGlowsForAllPlayers( void )
{
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		C_CSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer )
		{
			pPlayer->UpdateGlows();
		}
	}
}

static bool GlowEffectHLTVReplay( C_CSPlayer* thisPlayer, C_CSPlayer* pLocalPlayer, GlowRenderStyle_t& glowStyle, Vector& glowColor, float& alphaStart, float& alpha, float& timeStart, float& timeTarget, bool& animate )
{
	if ( g_HltvReplaySystem.GetHltvReplayDelay() && spec_replay_outline.GetInt() != 3 )
	{
		if ( spec_replay_outline.GetInt() && ( thisPlayer->entindex() == g_HltvReplaySystem.GetPrimaryVictimEntIndex() ) )
		{
			glowColor.Init( 1, 1, 1 );
			alpha = 0.6f;
			return true;
		}
	}
	else if ( g_HltvReplaySystem.IsDemoPlayback() && g_HltvReplaySystem.IsDemoPlaybackLowLights() && spec_replay_outline.GetInt() )
	{
		if ( g_HltvReplaySystem.GetDemoPlaybackPlayer() == thisPlayer )
		{
			glowColor.Init( 1, 1, 1 );
			alpha = 0.8f;
			return true;
		}
	}

	return false;
}

static bool GlowEffectSensorGrenade( C_CSPlayer* thisPlayer, C_CSPlayer* pLocalPlayer, GlowRenderStyle_t& glowStyle, Vector& glowColor, float& alphaStart, float& alpha, float& timeStart, float& timeTarget, bool& animate )
{
	float detectionTime = thisPlayer->m_flDetectedByEnemySensorTime;

	// If we weren't detected by a sensor grenade, no glow
	if ( detectionTime <= 0.0f )
		return false;

	// If we are on the same team as the detected player, don't show
	if ( !pLocalPlayer )
		return false;
	if( pLocalPlayer->GetTeamNumber() == thisPlayer->GetTeamNumber() )
		return false;

	// If the detection was too long ago, no glow
	static const float kSensorGlowTime = 5.0f;
	if ( gpGlobals->curtime > ( detectionTime + kSensorGlowTime ) )
		return false;

	// glow red
	glowColor.x = ( 255.0f / 255.0f );
	glowColor.y = ( 78.0f / 255.0f );
	glowColor.z = ( 78.0f / 255.0f );

	// Animate from max to 0 over the sensor duration
	alphaStart = 0.6f;
	timeStart = detectionTime;
	alpha = 0.0f;
	timeTarget = detectionTime + kSensorGlowTime;
	animate = true;

	return true;
}

static bool GlowEffectGunGameLeader( C_CSPlayer* thisPlayer, C_CSPlayer* pLocalPlayer, GlowRenderStyle_t& glowStyle, Vector& glowColor, float& alphaStart, float& alpha, float& timeStart, float& timeTarget, bool& animate )
{
	if ( !pLocalPlayer )
		return false;

	if ( !CSGameRules()->IsPlayingGunGameProgressive() )
		return false;

	// Get current player's team
	int nTeam = -1;
	if ( thisPlayer->GetTeamNumber() == TEAM_CT )
		nTeam = TEAM_CT;
	else if ( thisPlayer->GetTeamNumber() == TEAM_TERRORIST )
		nTeam = TEAM_TERRORIST;

	// If current player isn't on a team, they won't glow
	if ( nTeam == -1 )
		return false;

	// if this player isn't winning on their team, they don't glow
	C_Team *team = GetGlobalTeam( nTeam );
	if ( !team )
		return false;
	if ( team->GetGGLeader( nTeam ) != thisPlayer->entindex() )
		return false;

#if 0
	// if this player hasn't fired for a while, they don't glow
	// (REMOVED)
	if ( thisPlayer->m_flLastFiredWeaponTime > 0 )
	{
		if ( gpGlobals->curTime > ( thisPlayer->m_flLastFiredWeaponTime + 4.0f ) )
			return false;
	}
#endif

	// if the player is at gold knife level, they don't glow
	int nMaxIndex = CSGameRules()->GetNumProgressiveGunGameWeapons( nTeam ) - 1;
	if ( thisPlayer->m_iGunGameProgressiveWeaponIndex >= nMaxIndex )
		return false;

	// If the local player is leading their team, they don't get to see the glow
	if ( pLocalPlayer->m_isCurrentGunGameTeamLeader
		|| pLocalPlayer->entindex() == GetGlobalTeam( pLocalPlayer->GetTeamNumber() )->GetGGLeader( pLocalPlayer->GetTeamNumber() ) )
		return false;

	// Otherwise they glow.  Pick a color based on the team.  Spectators always see team color glow, but players in-game just see red for the opposing leader.
	if ( pLocalPlayer->GetTeamNumber() != nTeam && ( pLocalPlayer->GetTeamNumber() == TEAM_CT || pLocalPlayer->GetTeamNumber() == TEAM_TERRORIST ) )
	{
		// "enemy", red
		glowColor.x = ( 255.0f / 255.0f );
		glowColor.y = ( 78.0f / 255.0f );
		glowColor.z = ( 78.0f / 255.0f );
	}
	else if ( nTeam == TEAM_CT )
	{
		// CT teammate blue
		glowColor.x = ( 114.0f / 255.0f );
		glowColor.y = ( 155.0f / 255.0f );
		glowColor.z = ( 221.0f / 255.0f );
	}
	else if ( nTeam == TEAM_TERRORIST )
	{
		// T teammate yellow
		glowColor.x = ( 224.0f / 255.0f );
		glowColor.y = ( 175.0f / 255.0f );
		glowColor.z = ( 86.0f / 255.0f );
	}

	// this is arms race glow, so use the edge highlight pulse effect style (doesn't show through walls)
	glowStyle = GLOWRENDERSTYLE_EDGE_HIGHLIGHT;

	// gun game alpha is the default 0.6f
	alpha = 0.6f;

	return true;
}

static bool GlowEffectSpectator( C_CSPlayer* thisPlayer, C_CSPlayer* pLocalPlayer, GlowRenderStyle_t& glowStyle, Vector& glowColor, float& alphaStart, float& alpha, float& timeStart, float& timeTarget, bool& animate )
{
	// Spectator rendering
	if ( !pLocalPlayer )
		return false;

	C_CS_PlayerResource *cs_PR = dynamic_cast< C_CS_PlayerResource * >( g_PR );
	if ( cs_PR )
	{
		if ( cs_PR->GetTeam( thisPlayer->entindex() ) != thisPlayer->GetTeamNumber() )
		{
			//Msg( "Player resource disagrees on player team: %s\n", GetPlayerName() );
			return false;
		}
	}

	bool bRenderForSpectator = ( CanSeeSpectatorOnlyTools() && spec_show_xray.GetInt() );

	if ( !bRenderForSpectator && ( pLocalPlayer->IsAlive() || ( pLocalPlayer->GetObserverMode() <= OBS_MODE_FREEZECAM ) ) )
		return false;

	bool bRender = false;
	if ( mp_teammates_are_enemies.GetBool() && thisPlayer->IsOtherEnemy( pLocalPlayer->entindex() ) )  // if everyone is an enemy
	{
		// red
		glowColor.x = ( 242.0f / 255.0f );
		glowColor.y = ( 117.0f / 255.0f );
		glowColor.z = ( 117.0f / 255.0f );
		bRender = true;
	}
	else if ( ( thisPlayer->GetTeamNumber() == TEAM_CT ) && ( bRenderForSpectator || ( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_CT ) ) )
	{
		// blue
		glowColor.x = ( 114.0f / 255.0f );
		glowColor.y = ( 155.0f / 255.0f );
		glowColor.z = ( 221.0f / 255.0f );
		bRender = true;
	}
	else if ( ( thisPlayer->GetTeamNumber() == TEAM_TERRORIST ) && ( bRenderForSpectator || ( pLocalPlayer->GetAssociatedTeamNumber() == TEAM_TERRORIST ) ) )
	{
		// yellow
		glowColor.x = ( 224.0f / 255.0f );
		glowColor.y = ( 175.0f / 255.0f );
		glowColor.z = ( 86.0f / 255.0f );
		bRender = true;
	}

	// Check for xray highlight of currently selected player
	int nTargetSpec = g_bEngineIsHLTV ? HLTVCamera()->GetCurrentOrLastTarget() : ( pLocalPlayer->GetObserverTarget() ? pLocalPlayer->GetObserverTarget()->entindex() : -1 );
	if ( nTargetSpec == thisPlayer->entindex() )
	{
		bool bShowSelected =
			// we must alraedy be eligible to xray this player to highlight them as selected
			bRender &&
			// $$$REI I think this is a proxy for "is this a competitive mode game"
			( CCSGameRules::GetMaxPlayers() <= 10 ) &&
			// no selection highlight in dm/armsrace ($$$REI Why?)
			!( CSGameRules()->IsPlayingGunGameDeathmatch() || CSGameRules()->IsPlayingGunGameProgressive() ) &&
			// must be in 'free' observer mode otherwise we are already tethered to the selected player
			( pLocalPlayer->GetObserverMode() == OBS_MODE_FIXED || pLocalPlayer->GetObserverMode() == OBS_MODE_ROAMING );

		// always highlight the selected player when we are interpolating to them
		if ( pLocalPlayer->IsInObserverInterpolation() )
			bShowSelected = true;

		if ( bShowSelected )
		{
			glowColor.x = ( 255.0f / 255.0f );
			glowColor.y = ( 255.0f / 255.0f );
			glowColor.z = ( 255.0f / 255.0f );
			bRender = true;
		}
	}

	if ( !bRender )
		return false;

	// turn down spectator xray if player is quiet
	float fSpecGlowSilentFactor = spec_glow_silent_factor.GetFloat();
	float fSpecGlowSpikeFactor = spec_glow_spike_factor.GetFloat();
	float fSpecGlowFullTime = spec_glow_full_time.GetFloat();
	float fSpecGlowDecayTime = spec_glow_decay_time.GetFloat();
	float fSpecGlowSpikeTime = spec_glow_spike_time.GetFloat();

	float lastNoiseTime = thisPlayer->m_flLastMadeNoiseTime;

	if ( gpGlobals->curtime >= ( lastNoiseTime + fSpecGlowFullTime + fSpecGlowDecayTime ) )
	{
		// quiet
		alpha = fSpecGlowSilentFactor;
	}
	else if( gpGlobals->curtime >= ( lastNoiseTime + fSpecGlowFullTime ) )
	{
		// animate to 'quiet' alpha over the decay time
		timeStart = lastNoiseTime + fSpecGlowFullTime;
		alphaStart = 1.0f;

		timeTarget = lastNoiseTime + fSpecGlowFullTime + fSpecGlowDecayTime;
		alpha = fSpecGlowSilentFactor;

		animate = true;
	}
	else if( gpGlobals->curtime >= ( lastNoiseTime + fSpecGlowSpikeTime * 2.0f ) )
	{
		// regular "recently noisy" level.  still use 'animation' code here so we get a notification when full time expires
		timeStart = lastNoiseTime + fSpecGlowSpikeTime * 2.0f;
		alphaStart = 1.0f;

		timeTarget = lastNoiseTime + fSpecGlowFullTime;
		alpha = 1.0f;

		animate = true;
	}
	else if( gpGlobals->curtime >= ( lastNoiseTime + fSpecGlowSpikeTime ) )
	{
		// animate from spiked value to 1.0f
		timeStart = lastNoiseTime + fSpecGlowSpikeTime;
		alphaStart = fSpecGlowSpikeFactor;

		timeTarget = lastNoiseTime + fSpecGlowSpikeTime * 2.0f;
		alpha = 1.0f;

		animate = true;
	}
	else if ( gpGlobals->curtime >= lastNoiseTime )
	{
		// animate from current alpha to spike
		// note that we use current time here, instead of lastnoisetime.  this is because we might not update right away when the noise happens
		// if we are off by too much, we might lose the spike-drop animation, but that's better than losing the spike start animation.
		// if we really care, we could store an extra variable saying we are in the spike animation, but i might cut this feature anyways. 
		timeStart = gpGlobals->curtime;
		alphaStart = alphaStart * (1.0f / 0.6f); // we are going to multiply by 0.6f below; we really want to leave it untouched

		timeTarget = gpGlobals->curtime + fSpecGlowSpikeTime;
		alpha = fSpecGlowSpikeFactor;

		animate = true;
	}
	else
	{
		// somehow we made noise in the future?  just treat it as normal
		alpha = 1.0f;
	}

	// drop values (remnant of legacy glow code that 1.0 really means 0.6)
	alpha *= 0.6f;
	alphaStart *= 0.6f;

	return true;
}


typedef bool( *tGetGlowFunc )( C_CSPlayer* thisPlayer, C_CSPlayer* localPlayer, GlowRenderStyle_t& glowStyle, Vector& glowColor, float& alphaStart, float& alpha, float& timeStart, float& timeTarget, bool& animate );

void C_CSPlayer::UpdateGlows( void )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	//////////////////////////////////////////////////////////////////////////
	// First handle screen effects
	// TODO: Doesn't really belong in UpdateGlow(), but right now they update
	//       at the same time as glow effects

	// If we are winning arms race so that we glow to other players, put a glow effect on our screen to warn us
	if ( CSGameRules()->IsPlayingGunGameProgressive()
		&& pLocalPlayer == this
		&& IsAlive()
		&& ( m_isCurrentGunGameTeamLeader || entindex() == GetGlobalTeam( GetTeamNumber() )->GetGGLeader( GetTeamNumber() ) )
		&& ( m_iGunGameProgressiveWeaponIndex < ( CSGameRules()->GetNumProgressiveGunGameWeapons( GetTeamNumber() ) - 1 ) )
		)
	{
		float flAlphaScaler = 1.0f; // was some commented out code here to fade it based on time since firing

		if ( !m_ARScreenGlowEffect.IsValid() )
		{
			Vector vecOffset = Vector( 0, 0, 64 );//GetEyeOffset();

			m_ARScreenGlowEffect = ParticleProp()->Create( "ar_screenglow_leader_red", PATTACH_CUSTOMORIGIN );

			if ( m_ARScreenGlowEffect.IsValid() )
			{
				//m_ARScreenGlowEffect->m_pDef->SetDrawThroughLeafSystem( false );

				ParticleProp()->AddControlPoint( m_ARScreenGlowEffect, 1, this, PATTACH_CUSTOMORIGIN );
				m_ARScreenGlowEffect->SetControlPointEntity( 0, this );
				m_ARScreenGlowEffect->SetControlPointEntity( 1, this );
				m_ARScreenGlowEffect->SetControlPoint( 0, GetAbsOrigin() );
				m_ARScreenGlowEffect->SetControlPoint( 1, Vector( flAlphaScaler*0.65, 0, 0 ) );
				m_ARScreenGlowEffect->StartEmission();
				m_ARScreenGlowEffect->SetDrawn( true );
			}
		}
		else // m_ARScreenGlowEffect.IsValid()
		{
			m_ARScreenGlowEffect->SetControlPoint( 1, Vector( flAlphaScaler*0.65, 0, 0 ) );
			if ( flAlphaScaler > 0 )
				m_fNextGlowCheckInterval = 0.05f;
		}
	}
	else if ( m_ARScreenGlowEffect.IsValid() )
	{
		m_ARScreenGlowEffect->StopEmission();
		m_ARScreenGlowEffect->SetRemoveFlag();
		m_ARScreenGlowEffect = NULL;
	}

	// in order of priority
	static const tGetGlowFunc kGlowFuncs[] =
	{
		GlowEffectHLTVReplay,
		GlowEffectSensorGrenade,
		GlowEffectGunGameLeader,
		GlowEffectSpectator,
		nullptr
	};

	m_fNextGlowCheckInterval = GLOWUPDATE_DEFAULT_THINK_INTERVAL;

	bool bRender = false;
	Vector glowColor = vec3_origin;
	GlowRenderStyle_t glowStyle = GLOWRENDERSTYLE_DEFAULT;
	//	bool bRenderInside = false;
	float flAlphaStart = m_fGlowAlpha;
	float flAlpha = 1.0f;
	float flAlphaStartTime = gpGlobals->curtime;
	float flAlphaTargetTime = gpGlobals->curtime;
	bool animate = false;

	for ( const tGetGlowFunc* pGlowFunc = kGlowFuncs; *pGlowFunc != nullptr; ++pGlowFunc )
	{
		if ( ( *pGlowFunc )( this, pLocalPlayer, glowStyle, glowColor, flAlphaStart, flAlpha, flAlphaStartTime, flAlphaTargetTime, animate ) )
		{
			bRender = true;
			break;
		}
	}

	m_GlowObject.SetRenderFlags( bRender, false );
	//m_GlowObject.SetRenderFlags( !bRenderInside, bRender, bRenderInside );
	m_GlowObject.SetRenderStyle( glowStyle );
	m_GlowObject.SetColor( glowColor );

	if( bRender && animate )
	{
		// set up animation
		if(flAlphaTargetTime > flAlphaStartTime && flAlphaTargetTime > gpGlobals->curtime )
		{
			m_fGlowAlpha = Lerp( ( gpGlobals->curtime - flAlphaStartTime ) / ( flAlphaTargetTime - flAlphaStartTime ), flAlphaStart, flAlpha );
			m_fGlowAlphaTarget = flAlpha;
			m_fGlowAlphaTargetTime = flAlphaTargetTime;
		}
		else
		{
			m_fGlowAlphaTargetTime = -1.0f;
			m_fGlowAlpha = m_fGlowAlphaTarget = flAlpha;
		}
	}
	else
	{
		m_fGlowAlphaTargetTime = -1.0f;
		m_fGlowAlpha = m_fGlowAlphaTarget = flAlpha;
	}
	m_fGlowAlphaUpdateTime = gpGlobals->curtime;

	m_GlowObject.SetAlpha( bRender ? m_fGlowAlpha : 0.0f );
}

void C_CSPlayer::AnimateGlows( void )
{
	// Lerp towards the target
	float totalTimeRemaining = m_fGlowAlphaTargetTime - m_fGlowAlphaUpdateTime;
	float timeUsed = gpGlobals->curtime - m_fGlowAlphaUpdateTime;
	float percent;
	
	if ( timeUsed < totalTimeRemaining && totalTimeRemaining > 0 )
	{
		percent = timeUsed / totalTimeRemaining;
	}
	else
	{
		percent = 1.0f;
	}

	if ( percent < 1.0f )
	{
		// animate
		float newAlpha = Lerp( percent, m_fGlowAlpha, m_fGlowAlphaTarget );
		m_fGlowAlpha = newAlpha;
		m_fGlowAlphaUpdateTime = gpGlobals->curtime;
		m_GlowObject.SetAlpha( newAlpha );
	}
	else
	{
		// otherwise we are done with the animation, finish it and check if there is a new state
		m_fGlowAlpha = m_fGlowAlphaTarget;
		m_fGlowAlphaUpdateTime = m_fGlowAlphaTargetTime;
		m_fGlowAlphaTargetTime = -1.0f;
		m_GlowObject.SetAlpha( m_fGlowAlphaTarget );
		UpdateGlows();
	}
}

void C_CSPlayer::Spawn( void )
{
	m_flLastSpawnTimeIndex = gpGlobals->curtime;

#if defined( USE_PLAYER_ATTRIBUTE_MANAGER )
	m_AttributeManager.SetPlayer( this );
	m_AttributeList.SetManager( &m_AttributeManager );
#endif
	BaseClass::Spawn();

	if ( m_bUseNewAnimstate && m_PlayerAnimStateCSGO )
	{
		m_PlayerAnimStateCSGO->Reset();
		m_PlayerAnimStateCSGO->Update( EyeAngles()[YAW], EyeAngles()[PITCH] );
	}

	m_roundEndAmmoCount = roundEndAmmoCount_t();	// clear data
}

////  data collection for ammo remaining at death. OGS
void C_CSPlayer::RecordAmmoForRound( void )
{
	CBaseCombatWeapon *pWeapon;

	if ( !IsControllingBot() && !IsHLTV() && !g_HltvReplaySystem.GetHltvReplayDelay() )
	{
		// PRIMARY WEAPON
		pWeapon = Weapon_GetSlot( WEAPON_SLOT_RIFLE );

		if ( pWeapon )
		{
			m_roundEndAmmoCount.nPrimaryWeaponDefIndex = pWeapon->GetEconItemView()->GetItemDefinition()->GetDefinitionIndex();
			m_roundEndAmmoCount.nPrimaryWeaponAmmoCount = pWeapon->Clip1() + pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );
		}

		// SECONDARY WEAPON
		pWeapon = Weapon_GetSlot( WEAPON_SLOT_PISTOL );
		if ( pWeapon )
		{
			m_roundEndAmmoCount.nSecondaryWeaponDefIndex = pWeapon->GetEconItemView()->GetItemDefinition()->GetDefinitionIndex();
			m_roundEndAmmoCount.nSecondaryWeaponAmmoCount = pWeapon->Clip1() + pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );
		}
	}
}

void C_CSPlayer::UpdateOnRemove( void )
{
	if ( m_ARScreenGlowEffect.IsValid() && m_ARScreenGlowEffect->IsValid() )
	{
		m_ARScreenGlowEffect->StopEmission();
		m_ARScreenGlowEffect->SetRemoveFlag();
		m_ARScreenGlowEffect = NULL;
	}


	CancelFreezeCamFlashlightEffect();

	BaseClass::UpdateOnRemove();
}

//--------------------------------------------------------------------------------------------------------
void C_CSPlayer::OnSetDormant( bool bDormant )
{

	if ( bDormant )
	{
		// Turn off effects if we're going dormant
		if ( m_ARScreenGlowEffect.IsValid() )
		{
			m_ARScreenGlowEffect->StopEmission();
			m_ARScreenGlowEffect->SetRemoveFlag();
			m_ARScreenGlowEffect = NULL;
		}

		//if ( m_AdrenalineScreenEffect.IsValid() )
		//{
		//	m_AdrenalineScreenEffect->StopEmission();
		//	m_AdrenalineScreenEffect->SetRemoveFlag();
		//	m_AdrenalineScreenEffect = NULL;
		//}

		if ( !IsAnimLODflagSet( ANIMLODFLAG_DORMANT ) )
		{
			m_nAnimLODflagsOld &= ~ANIMLODFLAG_DORMANT;
		}
		SetAnimLODflag( ANIMLODFLAG_DORMANT );
	}
	else
	{
		if ( IsAnimLODflagSet( ANIMLODFLAG_DORMANT ) )
		{
			m_nAnimLODflagsOld |= ANIMLODFLAG_DORMANT;
		}
		UnSetAnimLODflag( ANIMLODFLAG_DORMANT );
	}

	BaseClass::OnSetDormant( bDormant );
}

bool C_CSPlayer::HasDefuser() const
{
	return m_bHasDefuser;
}

void C_CSPlayer::GiveDefuser()
{
	m_bHasDefuser = true;
}

void C_CSPlayer::RemoveDefuser()
{
	m_bHasDefuser = false;
}

bool C_CSPlayer::HasNightVision() const
{
	return m_bHasNightVision;
}

bool C_CSPlayer::IsVIP() const
{
	C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource* )GameResources();

	if ( !pCSPR )
		return false;

	return pCSPR->IsVIP( entindex() );
}

C_CSPlayer* C_CSPlayer::GetLocalCSPlayer()
{
	return (C_CSPlayer* )C_BasePlayer::GetLocalPlayer();
}


CSPlayerState C_CSPlayer::State_Get() const
{
	return m_iPlayerState;
}


float C_CSPlayer::GetMinFOV() const
{
	// Min FOV for AWP.
	return 10;
}


int C_CSPlayer::GetAccount() const
{
	return m_iAccount;
}


int C_CSPlayer::PlayerClass() const
{
	return m_iClass;
}

bool C_CSPlayer::CanShowTeamMenu() const
{
	return ( CSGameRules() && !CSGameRules()->IsQueuedMatchmaking() && !CSGameRules()->IsPlayingCooperativeGametype() && !IsHLTV() );
}


int C_CSPlayer::ArmorValue() const
{
	return m_ArmorValue;
}

bool C_CSPlayer::HasHelmet() const
{
	return m_bHasHelmet;
}


bool C_CSPlayer::HasHeavyArmor() const
{
	return m_bHasHeavyArmor;
}

bool C_CSPlayer::RenderLocalScreenSpaceEffect( CStrikeScreenSpaceEffect effect, IMatRenderContext *pRenderContext, int x, int y, int w, int h )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer )
	{
		return pLocalPlayer->DrawScreenSpaceVomitParticles( pRenderContext );
	}

	return false;
}

int C_CSPlayer::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( IsAnimLODflagSet(ANIMLODFLAG_OUTSIDEVIEWFRUSTUM) || IsDormant() || !IsVisible() )
	{

		if ( !IsVisible() )
		{
			// fixme: players spectators fly towards return false to IsVisible? Special case for now:
			C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

			if ( pLocalPlayer && 
				 pLocalPlayer->GetObserverInterpState() == OBSERVER_INTERP_TRAVELING && 
				 pLocalPlayer->GetObserverTarget() == ToBasePlayer(this) )
			{
				return BaseClass::DrawModel( flags, instance );
			}
		}

		return 0;
	}

	return BaseClass::DrawModel( flags, instance );
}

// bool C_CSPlayer::RenderScreenSpaceEffect( PortalScreenSpaceEffect effect, IMatRenderContext *pRenderContext, int x, int y, int w, int h )
// {
// 	bool result = false;
// 	switch ( effect )
// 	{
// 		case PAINT_SCREEN_SPACE_EFFECT:
// 			result = RenderScreenSpacePaintEffect( pRenderContext );
// 			break;
// 	}
// 
// 	return result;
// }

static void SetRenderTargetAndViewPort( ITexture *rt )
{
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->SetRenderTarget( rt );
	if ( rt )
	{
		pRenderContext->Viewport( 0, 0, rt->GetActualWidth(), rt->GetActualHeight() );
	}
}

//--------------------------------------------------------------------------------------------------------
bool C_CSPlayer::AreScreenSpaceVomitParticlesActive( void ) const
{
	if ( !m_ARScreenGlowEffect.IsValid() )
	{
		return false;
	}
	if ( !m_ARScreenGlowEffect->IsValid() )
	{
		return false;
	}

	return true;
}

bool C_CSPlayer::DrawScreenSpaceVomitParticles( IMatRenderContext *pRenderContext )
{
	if ( AreScreenSpaceVomitParticlesActive() )
	{
//  		pRenderContext->PushRenderTargetAndViewport();
//  		ITexture* pDestRenderTarget = materials->FindTexture( "_rt_SmallFB1", TEXTURE_GROUP_RENDER_TARGET );
//  		SetRenderTargetAndViewPort( pDestRenderTarget );
//  		pRenderContext->ClearColor4ub( 128, 128, 0, 0 );
//  		pRenderContext->ClearBuffers( true, false, false );

 		RenderableInstance_t instance;
 		instance.m_nAlpha = 255;
 		m_ARScreenGlowEffect->DrawModel( 1, instance );

// 		if ( IsGameConsole() )
// 		{
// 			pRenderContext->CopyRenderTargetToTextureEx( pDestRenderTarget, 0, NULL, NULL );
// 		}
// 
 		//pRenderContext->PopRenderTargetAndViewport();

		return true;
	}

	return false;
}

// --------------------------------------------------------------------------------------------------------
// void C_CSPlayer::DrawScreenSpaceVomitParticles( void )
// {
// 	if ( !m_ARScreenGlowEffect.IsValid() )
// 	{
// 		return;
// 	}
// 	if ( !m_ARScreenGlowEffect->IsValid() )
// 	{
// 		return;
// 	}
// 
// 	RenderableInstance_t instance;
// 	instance.m_nAlpha = 255;
// 	m_ARScreenGlowEffect->DrawModel( 1, instance );
// }

void C_CSPlayer::AddDecal( const Vector& rayStart, const Vector& rayEnd, const Vector& decalCenter, int hitbox, int decalIndex, bool doTrace, trace_t& tr, int maxLODToDecal )
{
	/* Removed for partner depot */
	BaseClass::AddDecal( rayStart, rayEnd, decalCenter, hitbox, decalIndex, doTrace, tr, maxLODToDecal );
}

float g_flFattenAmt = 4;
void C_CSPlayer::GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType )
{
#if defined( _PS3 ) && defined( CSTRIKE15 )

	// eurogamer PS3 temp fix - prevents the shadow resolution yo-yo (due to render bound changes - comments below) and frees some perf up

	mins = CollisionProp()->OBBMins();
	maxs = CollisionProp()->OBBMaxs();
	// Thus, we give it some padding here.
	mins -= Vector( g_flFattenAmt, g_flFattenAmt, 0 );
	maxs += Vector( g_flFattenAmt, g_flFattenAmt, 0 );

	return;

#else



	if ( shadowType == SHADOWS_SIMPLE )
	{
		// Don't let the render bounds change when we're using blobby shadows, or else the shadow
		// will pop and stretch.
		mins = CollisionProp()->OBBMins();
		maxs = CollisionProp()->OBBMaxs();
	}
	else
	{
		GetRenderBounds( mins, maxs );

		// We do this because the normal bbox calculations don't take pose params into account, and 
		// the rotation of the guy's upper torso can place his gun a ways out of his bbox, and 
		// the shadow will get cut off as he rotates.
		//
		// Thus, we give it some padding here.
		mins -= Vector( g_flFattenAmt, g_flFattenAmt, 0 );
		maxs += Vector( g_flFattenAmt, g_flFattenAmt, 0 );
	}

#endif

}

void C_CSPlayer::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	// TODO POSTSHIP - this hack/fix goes hand-in-hand with a fix in CalcSequenceBoundingBoxes in utils/studiomdl/simplify.cpp.
	// When we enable the fix in CalcSequenceBoundingBoxes, we can get rid of this.
	//
	// What we're doing right here is making sure it only uses the bbox for our lower-body sequences since,
	// with the current animations and the bug in CalcSequenceBoundingBoxes, are WAY bigger than they need to be.
	C_BaseAnimating::GetRenderBounds( theMins, theMaxs );

	// If we're ducking, we should reduce the render height by the difference in standing and ducking heights.
	// This prevents shadows from drawing above ducking players etc.
	if ( GetFlags() & FL_DUCKING )
	{
		theMaxs.z -= 18.5f;
	}
}


bool C_CSPlayer::GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const
{ 
	if ( shadowType == SHADOWS_SIMPLE )
	{
		// Blobby shadows should sit directly underneath us.
		pDirection->Init( 0, 0, -1 );
		return true;
	}
	else
	{
		return BaseClass::GetShadowCastDirection( pDirection, shadowType );
	}
}


void C_CSPlayer::VPhysicsUpdate( IPhysicsObject *pPhysics )
{
	BaseClass::VPhysicsUpdate( pPhysics );
}


int C_CSPlayer::GetIDTarget() const
{
	if ( !m_delayTargetIDTimer.IsElapsed() )
		return 0;

	if ( m_iIDEntIndex )
	{
		return m_iIDEntIndex;
	}

	if ( m_iOldIDEntIndex && !m_holdTargetIDTimer.IsElapsed() )
	{
		return m_iOldIDEntIndex;
	}

	return 0;
}

int C_CSPlayer::GetTargetedWeapon( void ) const
{
	return m_iTargetedWeaponEntIndex;
}

void C_CSPlayer::UpdateRadioHeadIcon( bool bRadio )
{
	bRadio = ( bRadio && ShouldShowRadioHeadIcon() );

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( bRadio )
	{
		if ( !m_radioHeadIconParticleEffect.IsValid() && (pPlayer && pPlayer != this) )
		{
			MDLCACHE_CRITICAL_SECTION();
			m_radioHeadIconParticleEffect = GetRadioHeadParticleEffect();
		}
	}
	else
	{
		if ( m_radioHeadIconParticleEffect.IsValid() )
		{
			m_radioHeadIconParticleEffect->StopEmission();
			m_radioHeadIconParticleEffect->SetRemoveFlag();
			m_radioHeadIconParticleEffect = NULL;
		}
	}
}

bool C_CSPlayer::ShouldShowRadioHeadIcon() const
{
	return GameRules() && GameRules()->IsMultiplayer();
}

Vector C_CSPlayer::GetParticleHeadLabelOffset( void )
{
	Vector vecVoice;
	int iBIndex = LookupBone( "ValveBiped.Bip01_Head" );
	if ( iBIndex >= 0 )
	{
		Vector vecBone;
		QAngle angBone;
		GetBonePosition( iBIndex, vecBone, angBone );

		vecVoice = (vecBone - GetAbsOrigin()) + Vector( 0, 0, 12 );
	}
	else
	{
		vecVoice = (EyePosition() - GetAbsOrigin()) + Vector( 0.0f, 0.0f, GetClientVoiceMgr()->GetHeadLabelOffset() );
	}

	return vecVoice;
}

CNewParticleEffect *C_CSPlayer::GetRadioHeadParticleEffect( void )
{
	Vector vecVoice = GetParticleHeadLabelOffset();
	if ( cl_teamid_overhead.GetInt() > 0 )
	{
		vecVoice += Vector( 0, 0, 18 );
	}

	return ParticleProp()->Create( GetRadioHeadParticleEffectName(), PATTACH_ABSORIGIN_FOLLOW, -1, vecVoice );
}

CNewParticleEffect *C_CSPlayer::GetVOIPParticleEffect( void )
{
	return ParticleProp()->Create( GetVOIPParticleEffectName(), PATTACH_ABSORIGIN_FOLLOW, -1, GetParticleHeadLabelOffset() );
}


char *C_CSPlayer::GetHalloweenMaskModelAddon( C_CSPlayer *pPlayer ) 
{ 
	int nMaskIndex = 0;

	bool bShowColor = ( CCSGameRules::GetMaxPlayers() <= 10 ) && CSGameRules() && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset();
	C_CS_PlayerResource* pCSPR = ( C_CS_PlayerResource* )g_PR;
	int nMainArraySize = ARRAYSIZE( s_HalloweenMaskModels );
	if ( bShowColor && pCSPR )
	{
		int playerIndex = 0;
		for ( int i = 0; i <= MAX_PLAYERS; i++ )
		{
			CBasePlayer* pCheckPlayer = UTIL_PlayerByIndex( i );
			if ( pCheckPlayer && pCheckPlayer == pPlayer )
			{
				playerIndex = i;
				break;
			}
		}
		int nColor = pCSPR->GetCompTeammateColor( playerIndex );
		if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
			nColor += 5;

		if ( nColor < 0 )
			nMaskIndex = (pPlayer->entindex() + (CCSGameRules::GetMaxPlayers()-1));
		else
			nMaskIndex = nColor;

		return s_HalloweenMaskModelsCompetitive[nMaskIndex % ARRAYSIZE( s_HalloweenMaskModelsCompetitive )].model;
	}
	else
	{
		int nSeed = CSGameRules()->m_nHalloweenMaskListSeed;
		nMaskIndex = (pPlayer->entindex()+nSeed) % nMainArraySize;
	}

	if ( Q_strcmp( s_HalloweenMaskModels[nMaskIndex].model, "tf2" ) == 0 )
	{
		nMaskIndex += RandomInt( 0, 6 );
		nMaskIndex = nMaskIndex % ARRAYSIZE( s_HalloweenMaskModelsTF2 );
		return s_HalloweenMaskModelsTF2[nMaskIndex].model;
	}

	return s_HalloweenMaskModels[nMaskIndex].model;
}

class C_PlayerAddonModel : public C_BreakableProp
{
public:
	virtual const Vector& GetAbsOrigin( void ) const
	{
		// if the player carrying this addon is in lod state (meaning outside the camera frustum)
		// we don't need to set up all the player's attachment bones just to find out where exactly
		// the addon model wants to render. Just return the player's origin.

		CBaseEntity *pMoveParent = GetMoveParent();

		if ( pMoveParent && pMoveParent->IsPlayer() )
		{
			C_CSPlayer *pCSPlayer = static_cast<C_CSPlayer *>( pMoveParent );

			if ( pCSPlayer && ( pCSPlayer->IsAnimLODflagSet(ANIMLODFLAG_OUTSIDEVIEWFRUSTUM) || pCSPlayer->IsDormant() || !pCSPlayer->IsVisible() ) )
				return pCSPlayer->GetAbsOrigin();

		}

		return BaseClass::GetAbsOrigin();
	}

	virtual bool ShouldDraw()
	{
		CBaseEntity *pMoveParent = GetMoveParent();
		if ( pMoveParent && pMoveParent->IsPlayer() )
		{
			C_CSPlayer *pCSPlayer = static_cast<C_CSPlayer *>( pMoveParent );
			if ( pCSPlayer && ( pCSPlayer->IsAnimLODflagSet(ANIMLODFLAG_OUTSIDEVIEWFRUSTUM) || pCSPlayer->IsDormant() || !pCSPlayer->IsVisible() ) )
				return false;
		}

		return BaseClass::ShouldDraw();
	}

	virtual bool IsFollowingEntity()
	{
		// addon models are ALWAYS following players
		return true;
	}

};

void C_CSPlayer::CreateAddonModel( int i )
{
	/* Removed for partner depot */
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bThirdperson - 
//-----------------------------------------------------------------------------
void C_CSPlayer::ThirdPersonSwitch( bool bThirdperson )
{
	BaseClass::ThirdPersonSwitch( bThirdperson );

	if ( m_hCarriedHostageProp != NULL )
	{
		C_HostageCarriableProp *pHostageProp = static_cast< C_HostageCarriableProp* >( m_hCarriedHostageProp.Get() );
		if ( pHostageProp )
		{
			UpdateHostageCarryModels();
		}
	}
}

void C_CSPlayer::CalcView( Vector &eyeOrigin, QAngle &eyeAngles, float &zNear, float &zFar, float &fov )
{
	BaseClass::CalcView( eyeOrigin, eyeAngles, zNear, zFar, fov );

	//only modify the eye position for first-person players or observers
	if ( m_bUseNewAnimstate && m_PlayerAnimStateCSGO )
	{
		if ( IsLocalPlayer() && IsAlive() && ( !::input->CAM_IsThirdPerson() || cl_camera_height_restriction_debug.GetBool() ) )
		{
			m_PlayerAnimStateCSGO->ModifyEyePosition( eyeOrigin );
		}
		else if ( GetObserverMode() == OBS_MODE_IN_EYE )
		{
			C_CSPlayer *pTargetPlayer = ToCSPlayer( GetLocalCSPlayer()->GetObserverTarget() );
			if ( pTargetPlayer )
			{
				pTargetPlayer->m_PlayerAnimStateCSGO->ModifyEyePosition( eyeOrigin );
			}
		}
	}

#ifdef IRONSIGHT
	CWeaponCSBase *pWeapon = GetActiveCSWeapon();
	if (pWeapon)
	{
		CIronSightController* pIronSightController = pWeapon->GetIronSightController();
		if (pIronSightController)
		{
			//bias the local client FOV change so ironsight transitions are nicer
			fov = pIronSightController->GetIronSightFOV(GetDefaultFOV(), true);
		}
	}
#endif //IRONSIGHT}

	Assert( eyeAngles.IsValid() && eyeOrigin.IsValid() );
}

#define	MP_TAUNT_PITCH	0
#define MP_TAUNT_YAW	1
#define MP_TAUNT_DIST	2

#define MP_TAUNT_MAXYAW		135
#define MP_TAUNT_MINYAW		-135
#define MP_TAUNT_MAXPITCH	90
#define MP_TAUNT_MINPITCH	0
#define MP_TAUNT_IDEALLAG	4.0f

static Vector MP_TAUNTCAM_HULL_MIN( -9.0f, -9.0f, -9.0f );
static Vector MP_TAUNTCAM_HULL_MAX( 9.0f, 9.0f, 9.0f );

//ConVar sv_slomo_death_cam_speed( "sv_slomo_death_cam_speed", "0.3", FCVAR_CHEAT | FCVAR_REPLICATED );
//ConVar sv_slomo_death_cam_dist( "sv_slomo_death_cam_dist", "180", FCVAR_CHEAT | FCVAR_REPLICATED );

void C_CSPlayer::UpdateHostageCarryModels()
{
	if ( m_hCarriedHostage )
	{
		if ( m_hCarriedHostageProp != NULL )
		{
			C_HostageCarriableProp *pHostageProp = static_cast< C_HostageCarriableProp* >( m_hCarriedHostageProp.Get() );
			if ( pHostageProp )
			{
				pHostageProp->UpdateVisibility();
			}
		}

		C_BaseViewModel *pViewModel = assert_cast<C_BaseViewModel *>( GetViewModel( 1 ) );
		if ( pViewModel )
		{
			pViewModel->UpdateVisibility();
		}
	}

}

// ConVar debug_test_hats( "debug_test_hats", "1" );

void C_CSPlayer::UpdateAddonModels( bool bForce )
{
	COMPILE_TIME_ASSERT( NUM_ADDON_BITS + NUM_CLIENTSIDE_ADDON_BITS < 32 );
	int iCurAddonBits = m_iAddonBits;

	if ( m_hC4AddonLED && ( IsAnimLODflagSet(ANIMLODFLAG_OUTSIDEVIEWFRUSTUM) || IsDormant() ) )
	{
		RemoveC4Effect( false );
	}
	else if ( !m_hC4AddonLED && !IsAnimLODflagSet(ANIMLODFLAG_OUTSIDEVIEWFRUSTUM) && !IsDormant() && HasC4() )
	{
		int j,jNext;
		for ( j=m_AddonModels.Head(); j != m_AddonModels.InvalidIndex(); j = jNext )
		{
			jNext = m_AddonModels.Next( j );
			CAddonModel *pModel = &m_AddonModels[j];

			// if we haven't removed it
			int addonBit = 1<<pModel->m_iAddon;
			if ( pModel->m_hEnt.Get() && addonBit == ADDON_C4 )
			{
				// make sure the c4 has the light on it
				CreateC4Effect( pModel->m_hEnt.Get(), false );
			}
		}
	}

	// If a CS player has a holiday hat then make all hostages wear a holiday hat too
	C_CHostage::SetClientSideHoldayHatAddonForAllHostagesAndTheirRagdolls( !!( iCurAddonBits & ADDON_CLIENTSIDE_HOLIDAY_HAT ) );

	// If our player has a ragdoll then apply the holiday hat or other addons to the ragdoll
	if ( C_CSRagdoll *pRagdoll = (C_CSRagdoll* )m_hRagdoll.Get() )
	{
		pRagdoll->SetRagdollClientSideAddon( iCurAddonBits );

		if ( iCurAddonBits & ADDON_MASK )
			iCurAddonBits &= ~ADDON_MASK; // halloween

		if ( iCurAddonBits & ADDON_CLIENTSIDE_ASSASSINATION_TARGET)
			iCurAddonBits &= ~ADDON_CLIENTSIDE_ASSASSINATION_TARGET; // clean up assassination target mask if we're dead 
	}

	if ( iCurAddonBits & ADDON_CLIENTSIDE_GHOST )
		iCurAddonBits &= ~ADDON_CLIENTSIDE_GHOST; // halloween

	// Don't put addon models on the local player unless in third person.
	if ( IsLocalPlayer( this ) && !C_BasePlayer::ShouldDrawLocalPlayer() )
		iCurAddonBits = 0;

	// If a local player is observing this entity in first-person mode, get rid of its addons.
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pPlayer && pPlayer->GetObserverMode() == OBS_MODE_IN_EYE && pPlayer->GetObserverTarget() == this )
			iCurAddonBits = 0;
	}


	int n,nNext;
	for ( n=m_AddonModels.Head(); n != m_AddonModels.InvalidIndex(); n = nNext )
	{
		nNext = m_AddonModels.Next( n );
		CAddonModel *pModel = &m_AddonModels[n];
		int addonBit = 1<<pModel->m_iAddon;
	
		if ( !pModel || !pModel->m_hEnt || !pModel->m_hEnt->GetMoveParent() || pModel->m_hEnt->GetMoveParent()->IsDormant() )
		{
			if ( addonBit == ADDON_C4 )
			{
				RemoveC4Effect( false );
			}
	
			if ( pModel->m_hEnt.Get() )
			{
				pModel->m_hEnt->ClearCustomMaterials();
				pModel->m_hEnt->Release();
			}
			
			m_AddonModels.Remove( n );
			iCurAddonBits = 0;
		}
	}

	if ( bForce )
		iCurAddonBits = 0;

	// Any changes to the attachments we should have?
	if ( !bForce &&
		m_iLastAddonBits == iCurAddonBits &&
		m_iLastPrimaryAddon == m_iPrimaryAddon &&
		m_iLastSecondaryAddon == m_iSecondaryAddon )
	{
		return;
	}

	bool rebuildPistol2Addon = bForce;
	if ( m_iSecondaryAddon == WEAPON_ELITE && ((m_iLastAddonBits ^ iCurAddonBits ) & ADDON_PISTOL) != 0 )
	{
		rebuildPistol2Addon = true;
	}

	bool rebuildPrimaryAddon = (( m_iLastPrimaryAddon != m_iPrimaryAddon ) || bForce );

	m_iLastAddonBits = iCurAddonBits;
	m_iLastPrimaryAddon = m_iPrimaryAddon;
	m_iLastSecondaryAddon = m_iSecondaryAddon;

	// Get rid of any old models.
	int i,iNext;
	for ( i=m_AddonModels.Head(); i != m_AddonModels.InvalidIndex(); i = iNext )
	{
		iNext = m_AddonModels.Next( i );
		CAddonModel *pModel = &m_AddonModels[i];

		int addonBit = 1<<pModel->m_iAddon;
		if ( !( iCurAddonBits & addonBit ) || (rebuildPistol2Addon && addonBit == ADDON_PISTOL2 ) || ( rebuildPrimaryAddon && addonBit == ADDON_PRIMARY ) )
		{
			if ( addonBit == ADDON_C4 )
			{
				RemoveC4Effect( false );
			}

			if ( pModel->m_hEnt.Get() )
			{
				pModel->m_hEnt->ClearCustomMaterials();
				pModel->m_hEnt->Release();
			}

			m_AddonModels.Remove( i );
		}
	}

	// Figure out which models we have now.
	int curModelBits = 0;
	FOR_EACH_LL( m_AddonModels, j )
	{
		curModelBits |= (1<<m_AddonModels[j].m_iAddon );
	}

	// Add any new models.
	for ( i=0; i < NUM_ADDON_BITS + NUM_CLIENTSIDE_ADDON_BITS; i++ )
	{
		if ( (iCurAddonBits & (1<<i )) && !( curModelBits & (1<<i) ) )
		{
			// Ok, we're supposed to have this one.
			CreateAddonModel( i );
		}
	}

	// go through again and see if they need any effects
	int j,jNext;
	for ( j=m_AddonModels.Head(); j != m_AddonModels.InvalidIndex(); j = jNext )
	{
		jNext = m_AddonModels.Next( j );
		CAddonModel *pModel = &m_AddonModels[j];

		// if we haven't removed it
		int addonBit = 1<<pModel->m_iAddon;
		if ( pModel->m_hEnt.Get() && addonBit == ADDON_C4 )
		{
			// make sure the c4 has the light on it
			CreateC4Effect( pModel->m_hEnt.Get(), false );
		}
	}

	m_bAddonModelsAreOutOfDate = false;
}


void C_CSPlayer::RemoveAddonModels()
{
	m_iAddonBits = 0;
	
	if ( !m_AddonModels.Count() )
		return;

	//Don't test anything about the addon models, just remove them.
	int n, nNext;
	for ( n=m_AddonModels.Head(); n != m_AddonModels.InvalidIndex(); n = nNext )
	{
		nNext = m_AddonModels.Next( n );
		CAddonModel *pModel = &m_AddonModels[n];
		int addonBit = 1<<pModel->m_iAddon;

		if ( addonBit == ADDON_C4 )
		{
			RemoveC4Effect( false );
		}

		if ( pModel->m_hEnt.Get() )
		{
			pModel->m_hEnt->ClearCustomMaterials();
			pModel->m_hEnt->Release();
		}

		m_AddonModels.Remove( n );
	}
}

void C_CSPlayer::CreateC4Effect( CBaseEntity *pEnt, bool bIsWeaponModel )
{
	if ( !pEnt )
		return;

	if ( bIsWeaponModel )
	{
		if ( !m_hC4WeaponLED )
		{
			//RemoveC4Effect( bIsWeaponModel );
			
			m_hC4WeaponLED = pEnt->ParticleProp()->Create( "c4_timer_light_held", PATTACH_POINT_FOLLOW, "led" );
			if ( m_hC4WeaponLED )
			{
				ParticleProp()->AddControlPoint( m_hC4WeaponLED, 0, pEnt, PATTACH_POINT_FOLLOW, "led" );
			}

			/*
			int splitScreenRenderFlags = 0x0;
			FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
			{
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
				CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

				// We don't want to show third person muzzle flash effects for this player if the splitscreen viewer is looking at this player in first person mode.
				bool viewingInFirstPersonMode = ( pLocalPlayer == this ) ||
					( pLocalPlayer && pLocalPlayer->GetObserverTarget() == this && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE );

				if ( !viewingInFirstPersonMode )
				{
					splitScreenRenderFlags |= (0x1 << hh );
				}
			}

			int ledAttachmentIndex = pEnt->LookupAttachment("led" );
			DispatchParticleEffect( "c4_timer_light_held", PATTACH_POINT_FOLLOW, pEnt, ledAttachmentIndex, false, splitScreenRenderFlags );
			*/
		}
	}
	else
	{
		if ( m_hC4AddonLED )
		{
			RemoveC4Effect( bIsWeaponModel );
		}

		m_hC4AddonLED = ParticleProp()->Create( "c4_timer_light_held", PATTACH_ABSORIGIN_FOLLOW );

		if ( m_hC4AddonLED )
		{
			ParticleProp()->AddControlPoint( m_hC4AddonLED, 0, pEnt, PATTACH_POINT_FOLLOW, "led" );
		}
	}
}

void C_CSPlayer::RemoveC4Effect( bool bIsWeaponModel )
{
	if ( bIsWeaponModel )
	{
		if ( m_hC4WeaponLED )
		{
			ParticleProp()->StopEmission( m_hC4WeaponLED );
			m_hC4WeaponLED = NULL;
		}
	}
	else
	{
		if ( m_hC4AddonLED )
		{
			ParticleProp()->StopEmission( m_hC4AddonLED );
			m_hC4AddonLED = NULL;
		}
	}
}

void C_CSPlayer::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	// Remove all addon models if we go out of the PVS.
	if ( state == SHOULDTRANSMIT_END )
	{
		RemoveAddonModels();

		if( m_pFlashlightBeam != NULL )
		{
			FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
			{
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
				ReleaseFlashlight();
			}
		}
	}

	BaseClass::NotifyShouldTransmit( state );
}


void C_CSPlayer::UpdateSoundEvents()
{
	int iNext;
	for ( int i=m_SoundEvents.Head(); i != m_SoundEvents.InvalidIndex(); i = iNext )
	{
		iNext = m_SoundEvents.Next( i );

		CCSSoundEvent *pEvent = &m_SoundEvents[i];
		if ( gpGlobals->curtime >= pEvent->m_flEventTime )
		{
			const Vector *pOrigin = NULL;
			if ( pEvent->m_bHasSoundOrigin )
			{
				pOrigin = &pEvent->m_SoundOrigin;
			}
			CLocalPlayerFilter filter;
			EmitSound( filter, GetSoundSourceIndex(), STRING( pEvent->m_SoundName ), pOrigin );

			m_SoundEvents.Remove( i );
		}
	}
}

//-----------------------------------------------------------------------------
void C_CSPlayer::UpdateMinModels( void )
{
	int modelIndex = m_nModelIndex;

#if CS_ALLOW_CL_MINMODELS

	modelIndex = FilterModelUsingCL_MinModels( PlayerClass() );

#endif

	SetModelByIndex( modelIndex );
}
extern ConVar sv_showbullethits;
//-----------------------------------------------------------------------------
void C_CSPlayer::ClientThink()
{
	if ( IsLocalPlayer() && cl_show_equipment_value.GetBool() )
	{
		engine->Con_NPrintf( 33, "round start: %d", GetRoundStartEquipmentValue() );
		engine->Con_NPrintf( 34, "current: %d", GetCurrentEquipmentValue() );
	}

	if ( IsAlive() )
	{
		m_vecLastAliveLocalVelocity = (m_vecLastAliveLocalVelocity * 0.8) + (GetLocalVelocity() * 0.2);
		ReevauluateAnimLOD();
	}

	CreateClientEffectModels();

	if ( m_hContactShadowLeft.Get() && m_hContactShadowRight.Get() )
	{
		if ( cl_foot_contact_shadows.GetBool() && 
			IsAlive() && 
			(GetFlags() & FL_ONGROUND) &&
			!IsAnimLODflagSet( ANIMLODFLAG_DISTANT | ANIMLODFLAG_OUTSIDEVIEWFRUSTUM | ANIMLODFLAG_INVISIBLELOCALPLAYER | ANIMLODFLAG_DORMANT ) &&
			ShouldDraw() )
		{
			int nLeftBoneIndex = LookupBone( "ankle_L" );
			matrix3x4_t matBoneLeft;
			GetBoneTransform( nLeftBoneIndex, matBoneLeft );
			Vector vecFootPosLeft = matBoneLeft.GetOrigin();

			int nRightBoneIndex = LookupBone( "ankle_R" );
			matrix3x4_t matBoneRight;
			GetBoneTransform( nRightBoneIndex, matBoneRight );
			Vector vecFootPosRight = matBoneRight.GetOrigin();

			if ( vecFootPosLeft.DistToSqr( m_vecLastContactShadowTraceOriginLeft ) > (8*8) )
			{
				trace_t trLeft;
				UTIL_TraceLine( vecFootPosLeft, vecFootPosLeft + Vector( 0, 0, -12.0f ), MASK_FLOORTRACE, this, COLLISION_GROUP_NONE, &trLeft );
				m_vecLastContactShadowTraceOriginLeft = trLeft.startpos;
				m_flLastContactShadowGroundHeightLeft = trLeft.endpos.z;
				//debugoverlay->AddLineOverlay( trLeft.startpos, trLeft.endpos, 255, 0, 0, true, 1.0f );
			}

			if ( vecFootPosRight.DistToSqr( m_vecLastContactShadowTraceOriginRight ) > (8*8) )
			{
				trace_t trRight;
				UTIL_TraceLine( vecFootPosRight, vecFootPosRight + Vector( 0, 0, -12.0f ), MASK_FLOORTRACE, this, COLLISION_GROUP_NONE, &trRight );
				m_vecLastContactShadowTraceOriginRight = trRight.startpos;
				m_flLastContactShadowGroundHeightRight = trRight.endpos.z;
				//debugoverlay->AddLineOverlay( trRight.startpos, trRight.endpos, 255, 0, 0, true, 1.0f );
			}

			float flTraceLengthLeft = vecFootPosLeft.z - m_flLastContactShadowGroundHeightLeft;
			m_hContactShadowLeft->SetRenderMode( kRenderTransAlpha );
			m_hContactShadowLeft->SetRenderAlpha( MIN( GetRenderAlpha(), RemapValClamped( flTraceLengthLeft, 4.0f, 8.0f, 250.0f, 0.0f ) ) ); // don't be more opaque than the player
			m_hContactShadowLeft->RenderForceOpaquePass( true );

			float flTraceLengthRight = vecFootPosRight.z - m_flLastContactShadowGroundHeightRight;
			m_hContactShadowRight->SetRenderMode( kRenderTransAlpha );
			m_hContactShadowRight->SetRenderAlpha( MIN( GetRenderAlpha(), RemapValClamped( flTraceLengthRight, 4.0f, 8.0f, 250.0f, 0.0f ) ) ); // don't be more opaque than the player
			m_hContactShadowRight->RenderForceOpaquePass( true );

			QAngle angLFoot;
			Vector vecLFootForward = -matBoneLeft.GetForward();
			vecLFootForward.z = 0;
			VectorAngles( vecLFootForward, angLFoot );
			m_hContactShadowLeft->SetAbsAngles( angLFoot );
			m_hContactShadowLeft->SetAbsOrigin( Vector( vecFootPosLeft.x, vecFootPosLeft.y, GetAbsOrigin().z + 0.25f ) );
			m_hContactShadowLeft->RemoveEffects( EF_NODRAW );

			QAngle angRFoot;
			Vector vecRFootForward = matBoneRight.GetForward();
			vecRFootForward.z = 0;
			VectorAngles( vecRFootForward, angRFoot );
			m_hContactShadowRight->SetAbsAngles( angRFoot );
			m_hContactShadowRight->SetAbsOrigin( Vector( vecFootPosRight.x, vecFootPosRight.y, GetAbsOrigin().z + 0.25f ) );
			m_hContactShadowRight->RemoveEffects( EF_NODRAW );
		}
		else
		{
			m_hContactShadowLeft->AddEffects( EF_NODRAW );
			m_hContactShadowRight->AddEffects( EF_NODRAW );
		}
	}

	BaseClass::ClientThink();

	// Cheap cheat detection code to catch cheat-engine users
	/** Removed for partner depot **/

	// velocity music handling
	if( GetCurrentMusic() == CSMUSIC_START &&  GetMusicStartRoundElapsed() > 0.5 )
	{
		Vector vAbsVelocity = GetAbsVelocity();
		float flAbsVelocity = vAbsVelocity.Length2D();
		if( flAbsVelocity > 10 )
		{
			if( this == GetHudPlayer() )
			{
				CLocalPlayerFilter filter;
				PlayMusicSelection( filter, CSMUSIC_ACTION );
			}
			SetCurrentMusic( CSMUSIC_ACTION );
		}
	}
	
	//Make sure smoke grenades that have expired are removed.
	for ( int it = 0; it < g_SmokeGrenadeHandles.Count(); it++ )
	{
		CBaseCSGrenadeProjectile *pGrenade = static_cast< CBaseCSGrenadeProjectile* >( g_SmokeGrenadeHandles[it].Get() );
		if ( !pGrenade )
			g_SmokeGrenadeHandles.FastRemove( it-- );
	}

	UpdateSoundEvents();

	UpdateFlashBangEffect();

	UpdateHostageCarryModels();

	// Client controls this addon. Set it if we need it, but respect the below hiding rules (which set all addons to 0)
	C_CSPlayer *pLocalPlayer = GetLocalCSPlayer();
	if ( GetTeamNumber() == TEAM_TERRORIST && IsAssassinationTarget() && !IsControllingBot() && !m_bHasControlledBotThisRound )
	{
		m_iAddonBits |= ADDON_CLIENTSIDE_ASSASSINATION_TARGET;
	}

	UpdateAddonModels( m_bAddonModelsAreOutOfDate );

	
	// don't show IDs in chase spec mode
	bool inSpecMode = ( GetObserverMode() == OBS_MODE_CHASE || GetObserverMode() == OBS_MODE_DEATHCAM );

	if ( IsLocalPlayer( this ) && !inSpecMode && IsAlive() && ( mp_forcecamera.GetInt() != OBS_ALLOW_NONE ) )
	{
		UpdateIDTarget();
		UpdateTargetedWeapon();
	}

	if ( CSGameRules() && CSGameRules()->IsPlayingCoopGuardian() && IsLocalPlayer( this ) && !inSpecMode && IsAlive() )
	{
		if ( m_flGuardianTooFarDistFrac > 0.2 && m_flNextGuardianTooFarWarning <= gpGlobals->curtime )
		{
			CHudElement *pElement = GetHud().FindElement( "SFHudInfoPanel" );
			if ( pElement )
			{
				if ( CSGameRules()->IsPlayingCoopGuardian() )
				{
					EmitSound( "UI.Guardian.TooFarWarning" );
					( ( SFHudInfoPanel * )pElement )->SetPriorityHintText( g_pVGuiLocalize->Find( "#SFUI_Notice_GuardianModeTooFarFromBomb" ) );
				}
			}

			m_flNextGuardianTooFarWarning = gpGlobals->curtime + MAX( 0.25, ( 1 - m_flGuardianTooFarDistFrac ) * 2 );
		}
	}

	////////////////////////////////////
	// show player shot locations
	bool bRenderForSpectator = CanSeeSpectatorOnlyTools() && spec_show_xray.GetInt();
	if ( bRenderForSpectator && GetLocalPlayer() && /*(GetLocalPlayer()->m_nButtons & IN_RELOAD) &&*/ (GetLocalPlayer()->GetObserverMode() == OBS_MODE_FIXED || GetLocalPlayer()->GetObserverMode() == OBS_MODE_ROAMING) )
	{
		Vector aimDir;
		AngleVectors( GetFinalAimAngle(), &aimDir );
		Vector vecShootPos = EyePosition();
		CWeaponCSBase* pActiveWeapon = GetActiveCSWeapon();
		bool bShowLine = true;
		if ( pActiveWeapon )	
		{
			QAngle angTemp;
			GetAttachment( LookupAttachment("facemask"), vecShootPos, angTemp );
			vecShootPos += aimDir * 10;
			if ( pActiveWeapon->m_bInReload )
				bShowLine = false;
		}

		if ( bShowLine )
		{
			Vector vecCamPos = g_bEngineIsHLTV ? HLTVCamera()->GetCameraPosition() : (GetLocalPlayer() ? GetLocalPlayer()->EyePosition() : Vector(0,0,0));
			trace_t result;
			UTIL_TraceLine( EyePosition(), EyePosition() + 2000 * aimDir, MASK_SOLID|CONTENTS_DEBRIS|CONTENTS_HITBOX, this, COLLISION_GROUP_NONE, &result );

			float flMaxLength = 1024;
			float flLength = VectorLength( vecShootPos - vecCamPos );
			float flLengthEndPoint = VectorLength( result.endpos - vecCamPos );
			float flLengthLine = VectorLength( result.endpos - vecShootPos );
			float flAlpha = (1.0f - (clamp( flLength, 0, flMaxLength ) / flMaxLength)) * 255.0f;
			float flAlphaEndPoint = (1.0f - (clamp( flLengthEndPoint, 0, flMaxLength ) / flMaxLength)) * 180.0f;
			if ( flAlphaEndPoint > flAlpha )
				flAlpha = flAlphaEndPoint;

			// reduce alpha if the line is really short
			float flAlphaMult = clamp( flLengthLine, 0, 256.0f ) / 256.0f;
			flAlpha *= flAlphaMult;

			if ( IsAlive() && GetTeamNumber() == TEAM_CT )
			{
				debugoverlay->AddBoxOverlay( result.endpos, Vector(-1,-1,-1), Vector(1,1,1), QAngle(0,0,0), 0,120,240, (int)flAlpha, 0.01f );
				debugoverlay->AddLineOverlayAlpha( result.endpos, vecShootPos, 0,120,240, flAlpha, true, 0.01f );
			}
			else if ( IsAlive() && GetTeamNumber() == TEAM_TERRORIST )
			{
				debugoverlay->AddBoxOverlay( result.endpos, Vector(-1,-1,-1), Vector(1,1,1), QAngle(0,0,0), 230,128,0, (int)flAlpha, 0.01f );
				debugoverlay->AddLineOverlayAlpha( result.endpos, vecShootPos, 230,128,0, flAlpha, true, 0.01f );
			}
		}
	}

	if ( gpGlobals->curtime >= m_fNextThinkPushAway )
	{
		PerformObstaclePushaway( this );
		m_fNextThinkPushAway =  gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL;
	}

	// Do this every frame
	if( m_fGlowAlphaTargetTime != -1.0f )
		AnimateGlows();

	if ( IsLocalPlayer()
		&& (m_previousPlayerState != State_Get()
			|| gpGlobals->curtime >= m_fNextGlowCheckUpdate
			)
		)
	{
		if ( m_previousPlayerState != State_Get() )
			m_previousPlayerState = State_Get();

		UpdateGlowsForAllPlayers();
		m_fNextGlowCheckUpdate = gpGlobals->curtime + m_fNextGlowCheckInterval;
	}

	if ( IsLocalPlayer() )
	{
		if ( m_iObserverMode == OBS_MODE_FREEZECAM || g_HltvReplaySystem.GetHltvReplayDelay() )
		{
			static ConVarRef sv_disablefreezecam( "sv_disablefreezecam" );
			if ( !s_bPlayingFreezeCamSound && !cl_disablefreezecam.GetBool() && !sv_disablefreezecam.GetBool() && !g_HltvReplaySystem.IsDelayedReplayRequestPending() )
			{
				// Play sound
				s_bPlayingFreezeCamSound = true;

				C_RecipientFilter filter;
				filter.AddRecipient( this );

				// this lets us know at what "level" to play the death cam stinger
				int nConsecutiveKills = GetLastConcurrentKilled();
				// 0 == suicide or no killer
				// 1, 2, 3 == consecutive number of kills from a player
				// 4 + same, but on the 4th kill, domination kicks in

				if ( CSGameRules()->IsPlayingGunGame() )
				{
					if ( nConsecutiveKills > 3 )
					{
						C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Music.GG_Nemesis" );
					}
					else if ( nConsecutiveKills > 2 )
					{
						C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Music.GG_DeathCam_03" );
					}
					else if ( nConsecutiveKills > 1 )
					{
						C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Music.GG_DeathCam_02" );
					}
					else if ( nConsecutiveKills > 0 )
					{
						C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Music.GG_DeathCam_01" );
					}

				}
				// don't play this here with competetive mode, we play it when the player gets the player_death event for that mode instead so it happens immediately
				else if ( !CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() )
				{
					PlayMusicSelection( filter, CSMUSIC_DEATHCAM );
				}
			}
		}
		else
		{
			s_bPlayingFreezeCamSound = false;
			CancelFreezeCamFlashlightEffect();
		}
	}

	if ( sv_disable_immunity_alpha.GetBool() == false )
	{
		ConVarRef mp_respawn_immunitytime( "mp_respawn_immunitytime" );
		float flImmuneTime = mp_respawn_immunitytime.GetFloat();
		if ( flImmuneTime > 0 || CSGameRules()->IsWarmupPeriod() )
		{
			if ( m_bGunGameImmunity )
			{
				SetRenderMode( kRenderTransAlpha );
				SetRenderAlpha( 128 );
			}
			else
			{
				SetRenderMode( kRenderNormal, true );
				SetRenderAlpha( 255 );
			}
		}
		else
		{
			if ( GetRenderAlpha() < 255 )
			{
				SetRenderMode( kRenderNormal, true );
				SetRenderAlpha( 255 );
			}
		}
	}

	if ( CSGameRules()->IsPlayingCoopGuardian() && CSGameRules()->IsWarmupPeriod() == false && 
		 mp_use_respawn_waves.GetInt() == 2 && this == GetLocalPlayer() && IsAlive() && 
		 GetObserverMode() == OBS_MODE_NONE )
	{
		bool bCanBuy = false;
		if ( !IsBot() && CSGameRules()->m_flGuardianBuyUntilTime - 3.5f > gpGlobals->curtime && m_iAccount > 0 )
		{
			bCanBuy = true;
		}

		//float flTimeLeft = m_fImmuneToGunGameDamageTime - gpGlobals->curtime;
		if ( bCanBuy )
		{
			//wchar_t szNotice[64] = L"";
// 			wchar_t wzTime[8] = L"";
// 			int nMinLeft = ( int )flTimeLeft / 60;
// 			int nSecLeft = ( int )flTimeLeft - ( nMinLeft * 60 );
// 			int nMSecLeft = ( flTimeLeft - ( ( float )( nMinLeft * 60 ) + ( float )nSecLeft ) ) * 10;
// 			V_swprintf_safe( wzTime, L"%d.%d", nSecLeft, nMSecLeft );

			wchar_t wzBuyBind[32] = L"";
			UTIL_ReplaceKeyBindings( L"%buymenu%", 0, wzBuyBind, sizeof( wzBuyBind ) );

			//wchar_t wzAutoBuyBind[32] = L"";
			//UTIL_ReplaceKeyBindings( L"%autobuy%", 0, wzAutoBuyBind, sizeof( wzAutoBuyBind ) );

			wchar_t wszLocalized[256];
			//if ( flTimeLeft < 1.0f && m_bHasMovedSinceSpawn )
			g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#SFUI_Notice_Guardian_BuyMenuAvailable" ), 3, wzBuyBind/*, wzTime, wzAutoBuyBind*/ );

			CHudElement *pElement = GetHud().FindElement( "SFHudInfoPanel" );
			if ( pElement )
			{
				( ( SFHudInfoPanel * )pElement )->SetPriorityHintText( wszLocalized );
			}

			//GetCenterPrint()->Print( wszLocalized );
			//UTIL_HudHintText( GetOwner(), hint.Access() );

			//UTIL_ClientPrintAll( HUD_PRINTCENTER, "#SFUI_Notice_DM_OpenBuyMenu", wzFinal, wzTime );

			//m_fImmuneToGunGameDamageTimeLast = m_fImmuneToGunGameDamageTime;
		}
	}
	else if ( CSGameRules()->IsPlayingGunGameDeathmatch() && this == GetLocalPlayer() && IsAlive() && GetObserverMode() == OBS_MODE_NONE )
	{
		float flTimeLeft = m_fImmuneToGunGameDamageTime - gpGlobals->curtime;
		if ( m_fImmuneToGunGameDamageTimeLast != 0 || flTimeLeft >= 0 )
		{
			//wchar_t szNotice[64] = L"";
			wchar_t wzTime[8] = L"";
			int nMinLeft = (int)flTimeLeft / 60;
			int nSecLeft = (int)flTimeLeft - ( nMinLeft * 60 ); 
			int nMSecLeft = (flTimeLeft - ((float)(nMinLeft*60) + (float)nSecLeft)) * 10; 
			V_swprintf_safe( wzTime, L"%d.%d", nSecLeft, nMSecLeft );

			wchar_t wzBuyBind[32] = L"";
			UTIL_ReplaceKeyBindings( L"%buymenu%", 0, wzBuyBind, sizeof( wzBuyBind ) );

			wchar_t wzAutoBuyBind[32] = L"";
			UTIL_ReplaceKeyBindings( L"%autobuy%", 0, wzAutoBuyBind, sizeof( wzAutoBuyBind ) );

			wchar_t wszLocalized[256];
			if ( cl_dm_buyrandomweapons.GetBool() )
			{
				if ( flTimeLeft < 1.0f && m_bHasMovedSinceSpawn )
					g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#SFUI_Notice_DM_InvulnExpire_RandomON" ), 1, wzAutoBuyBind );
				else if ( flTimeLeft < 0.1 )
					g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#SFUI_Notice_DM_BuyMenuExpire_RandomON" ), 1, wzAutoBuyBind );
				else
					g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#SFUI_Notice_DM_BuyMenu_RandomON" ), 3, wzBuyBind, wzTime, wzAutoBuyBind );
			}	
			else
			{	
				if ( flTimeLeft < 1.0f && m_bHasMovedSinceSpawn )
					g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#SFUI_Notice_DM_InvulnExpire_RandomOFF" ), 1, wzAutoBuyBind );
				else if ( flTimeLeft < 0.1 )
					g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#SFUI_Notice_DM_BuyMenuExpire_RandomOFF" ), 1, wzAutoBuyBind );
				else	
					g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#SFUI_Notice_DM_BuyMenu_RandomOFF" ), 3, wzBuyBind, wzTime, wzAutoBuyBind );
			}

			CHudElement *pElement = GetHud().FindElement( "SFHudInfoPanel" );
			if ( pElement )														
			{																	
				((SFHudInfoPanel *)pElement)->SetPriorityHintText( wszLocalized );				
			}

			//GetCenterPrint()->Print( wszLocalized );
			//UTIL_HudHintText( GetOwner(), hint.Access() );

			//UTIL_ClientPrintAll( HUD_PRINTCENTER, "#SFUI_Notice_DM_OpenBuyMenu", wzFinal, wzTime );

			m_fImmuneToGunGameDamageTimeLast = m_fImmuneToGunGameDamageTime;
		}
	}
	else if ( IsAlive() && m_hCarriedHostage != NULL && this == GetLocalPlayer() && IsAlive() && GetObserverMode() == OBS_MODE_NONE )
	{
		wchar_t wszLocalized[256];	
		//g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#SFUI_Notice_DM_BuyMenu_RandomOFF" ), 3, wzBuyBind, wzTime, wzAutoBuyBind );
		g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#Cstrike_TitlesTXT_CarryingHostage" ), 0 );

		CHudElement *pElement = GetHud().FindElement( "SFHudInfoPanel" );
		if ( pElement )														
		{																	
			((SFHudInfoPanel *)pElement)->SetPriorityHintText( wszLocalized );				
		}
	}
	else if ( !IsAlive() && mp_use_respawn_waves.GetBool() && CSGameRules() && IsAbleToInstantRespawn() && this == GetLocalPlayer() && GetObserverMode() > OBS_MODE_FREEZECAM )
	{
		if ( CSGameRules()->IsWarmupPeriod() == false )
		{
			float flTimeLeft = CSGameRules()->GetNextRespawnWave( GetTeamNumber(), NULL ) - gpGlobals->curtime;
			if ( flTimeLeft > CSGameRules()->GetRespawnWaveMaxLength( GetTeamNumber() ) )
			{
				CHudElement *pElement = GetHud().FindElement( "SFHudInfoPanel" );
				if ( pElement )
				{
					( ( SFHudInfoPanel * )pElement )->SetPriorityHintText( g_pVGuiLocalize->Find( "#SFUI_Notice_WaitToRespawn" ) );
				}
			}
			else if ( flTimeLeft > 1.0f )
			{
				wchar_t wzTime[8] = L"";
				int nMinLeft = (int)flTimeLeft / 60;
				int nSecLeft = (int)flTimeLeft - ( nMinLeft * 60 ); 
				int nMSecLeft = (flTimeLeft - ((float)(nMinLeft*60) + (float)nSecLeft)) * 10; 
				V_swprintf_safe( wzTime, L"%d.%d", nSecLeft, nMSecLeft );

				wchar_t wszLocalized[256];
				g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#SFUI_Notice_WaveRespawnIn" ), 1, wzTime );

				CHudElement *pElement = GetHud().FindElement( "SFHudInfoPanel" );
				if ( pElement )														
				{																	
					((SFHudInfoPanel *)pElement)->SetPriorityHintText( wszLocalized );				
				}

				m_fImmuneToGunGameDamageTimeLast = m_fImmuneToGunGameDamageTime;
			}
			else if ( flTimeLeft > 0.75f )
			{
				CHudElement *pElement = GetHud().FindElement( "SFHudInfoPanel" );
				if ( pElement )														
				{																	
					((SFHudInfoPanel *)pElement)->SetPriorityHintText( g_pVGuiLocalize->Find( "#SFUI_Notice_WaveRespawning" ) );				
				}

				m_fImmuneToGunGameDamageTimeLast = m_fImmuneToGunGameDamageTime;
			}
		}
	}

	// Otherwise buy random or get previous round's gear, depending on cl_dm_buyrandomweapons.
	if ( m_bShouldAutobuyDMWeapons )
	{
		if ( this == GetLocalPlayer() && IsAlive() && (GetTeamNumber() == TEAM_CT || GetTeamNumber() == TEAM_TERRORIST) )
		{
			if ( cl_dm_buyrandomweapons.GetBool() )
			{
				engine->ClientCmd_Unrestricted( "buyrandom" );
			}
			else
			{
				engine->ClientCmd_Unrestricted( "rebuy" );
			}

			if ( m_bIsRespawningForDMBonus )
			{

				engine->ClientCmd_Unrestricted( "drop" ); // 'drop' is overloaded for DM to respawn with bonus weapon.
			}

			m_bShouldAutobuyDMWeapons = false;
		}
	}

	//=============================================================================
	// HPE_BEGIN:
	// [dwenger] Added for auto-buy functionality
	//=============================================================================
	if (m_bShouldAutobuyNow )
	{
		if (this == GetLocalPlayer() )
		{
			bool bDoAutoBuy = true;

			// Make sure the player only has the starting equipment (USP for CT, Glock for T )
			if ( Weapon_GetSlot( WEAPON_SLOT_RIFLE ) != NULL )
			{
				// Already has a primary weapon, so don't auto-buy
				bDoAutoBuy = false;
			}

			if (bDoAutoBuy )
			{
				engine->ClientCmd_Unrestricted("autobuy" );
			}
			else
			{
				// Test if armor should be bought
				if ( ArmorValue() < 100 )
				{
					engine->ClientCmd_Unrestricted("buy vesthelm" );
				}
			}
		}

		m_bShouldAutobuyNow = false;
	}

	//=============================================================================
	// HPE_END
	//=============================================================================


	if ( IsAlive() && ShouldDraw() )
	{
		// enable bone snapshots
		m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].Enable();
		m_boneSnapshots[BONESNAPSHOT_UPPER_BODY].Enable();
	}
	else
	{
		m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].Disable();
		m_boneSnapshots[BONESNAPSHOT_UPPER_BODY].Disable();
	}


	UpdateAllBulletHitModels();


	// code for following grenades
	if ( CanSeeSpectatorOnlyTools() && pLocalPlayer )
	{
		bool bHoldingGrenadeKey = IsHoldingSpecGrenadeKey();

		// if we don't have a target, but we have an old grenade target, the grenade exploded
		if ( pLocalPlayer->IsSpecFollowingGrenade() && pLocalPlayer->GetTeamNumber() == TEAM_SPECTATOR && pLocalPlayer->m_hOldGrenadeObserverTarget.Get() && 
			 (pLocalPlayer->GetObserverTarget() == NULL || dynamic_cast< CBaseCSGrenadeProjectile* >( pLocalPlayer->GetObserverTarget() ) ) )
		{
			CBaseCSGrenadeProjectile *pGrenade = dynamic_cast< CBaseCSGrenadeProjectile* >( pLocalPlayer->GetObserverTarget() );
			C_BaseEntity *pTarget = m_hOldGrenadeObserverTarget.Get();
			if ( pTarget )
			{
				Vector vecSpecPos = pLocalPlayer->EyePosition();
				QAngle angView;
				engine->GetViewAngles( angView );

				if ( !pGrenade )
				{
					pLocalPlayer->SetSpecWatchingGrenade( NULL, false );
					pLocalPlayer->m_iObserverMode = pLocalPlayer->GetObserverMode();
				}

				//
// 				if ( bHoldingGrenadeKey || (pGrenade && pGrenade->GetMoveType() == MOVETYPE_NONE ) )
// 				{
// 					char commandBuffer[128];
// 					char commandB[32] = "spec_lerpto";
// 					float flLerpTime = 0.5f;
// 
// 					//Vector vecSpecPos = g_bEngineIsHLTV ? HLTVCamera()->GetCameraPosition() : pLocalPlayer->GetAbsOrigin();
// 					V_snprintf( commandBuffer, sizeof( commandBuffer ), "%s %f %f %f %f %f %d %f", commandB, vecSpecPos[0], vecSpecPos[1], vecSpecPos[2], angView[0], angView[1], pTarget->entindex(), flLerpTime );
// 
// 					engine->ClientCmd( commandBuffer );
// 				}
			}
		}

		if ( (!pLocalPlayer->GetObserverTarget() && !IsHLTV()) )
			return;

		int nTargetSpec = -1;

		if ( IsHLTV() )
		{
			if ( HLTVCamera()->GetPrimaryTarget() )
				nTargetSpec = HLTVCamera()->GetPrimaryTarget()->entindex();
		}
		else
		{
			if ( pLocalPlayer->GetObserverTarget() )
				nTargetSpec = pLocalPlayer->GetObserverTarget()->entindex();
		}

		C_BasePlayer *pTarget = UTIL_PlayerByIndex( nTargetSpec );

		bool bIsFollowingGrenade = IsHLTV() ? HLTVCamera()->IsWatchingGrenade() : pLocalPlayer->m_bIsSpecFollowingGrenade;
		
		CBaseEntity *pGrenade = NULL;
		if ( pTarget && bHoldingGrenadeKey && bIsFollowingGrenade == false )
		{
			CBaseEntity *pEnt = NULL;
			
			float flNewest = 0;
			float flSpawnTime = 0;

			for ( CEntitySphereQuery sphere( pTarget->GetAbsOrigin(), 1024 ); ( pEnt = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
			{
				CBaseCSGrenadeProjectile* pGrenadeProjectile = dynamic_cast< CBaseCSGrenadeProjectile* >( pEnt );
				// filter out non-tracks
				if ( pGrenadeProjectile && pGrenadeProjectile->GetThrower() && pTarget->entindex() == pGrenadeProjectile->GetThrower()->entindex() )
				{
					flSpawnTime = pGrenadeProjectile->m_flSpawnTime;
					if ( flSpawnTime > flNewest )
					{
						flNewest = flSpawnTime;
						pGrenade = pEnt;
					}
				}
			}
		}

		if ( pGrenade && bHoldingGrenadeKey )
		{
			if ( IsHLTV() )
			{
				HLTVCamera()->SetWatchingGrenade( pGrenade, bHoldingGrenadeKey );
			}
			else
			{
				pLocalPlayer->SetSpecWatchingGrenade( pGrenade, bHoldingGrenadeKey );
			}
		}
		else
		{
			// if we are currently following a grenade, but our "follow greande"
			// key isn't down, stop following

			if ( bHoldingGrenadeKey == false )
			{
				if ( IsHLTV() && bIsFollowingGrenade )
				{
					HLTVCamera()->SetWatchingGrenade( pGrenade, false );
				}
				else if ( bIsFollowingGrenade )
				{
					pLocalPlayer->SetSpecWatchingGrenade( pGrenade, false );
				}
			}
		}
	}

	m_StartOfRoundSoundEvents.Update();
}



void C_CSPlayer::OnTimeJump()
{
	C_BasePlayer::OnTimeJump();
	m_fNextGlowCheckUpdate = 0;
	UpdateGlows();
	ClearAllBulletHitModels();
}


bool C_CSPlayer::IsHoldingSpecGrenadeKey( void )
{
	bool bHoldingGrenadeKey = false;
	int nGrenadeKey = cl_spec_follow_grenade_key.GetInt();
	if ( nGrenadeKey == 0 )
		bHoldingGrenadeKey = vgui::input()->IsKeyDown( KEY_LALT );
	else if ( nGrenadeKey == 1 )
		bHoldingGrenadeKey = vgui::input()->IsKeyDown( KEY_LSHIFT );
	else if ( nGrenadeKey == 2 )
		clientdll->IN_IsKeyDown( "in_reload", bHoldingGrenadeKey );

	int x = 1;
	if ( bHoldingGrenadeKey )
		x++;

	return bHoldingGrenadeKey;
}

void C_CSPlayer::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( type == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );

		m_freezeCamSpotLightTexture.Init( "effects/flashlight_freezecam", TEXTURE_GROUP_OTHER, true );
	}

	C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();

	if ( player == this && player->GetObserverMode() != player->m_iOldObserverMode )
	{
		bool bRespawning = false;
		bool bCompetitive = false;

		if ( CSGameRules() )
		{
			bRespawning = (State_Get() == STATE_GUNGAME_RESPAWN && IsAbleToInstantRespawn());
			if ( bRespawning )
				g_HltvReplaySystem.OnLocalPlayerRespawning();

			bCompetitive = CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset();
		}

//		Commented out because new item acknowledgement will only happen in the main menu.
// 		if ( !IsAlive() && ((m_iOldObserverMode < OBS_MODE_FIXED && GetObserverMode() >= OBS_MODE_FIXED) || bRespawning) )
// 		{
// 			// Show items we've picked up when we exit freezecam, or after deathcam on suicide
// 			if ( !bCompetitive )
// 			{
// 				// Don't do this in competitive modes, instead we'll show those at the end of the match
// 				InventoryManager()->ShowItemsPickedUp();
// 			}
// 		}
	}

	if ( m_bPlayingHostageCarrySound == false && m_hCarriedHostage )
	{
		m_bPlayingHostageCarrySound = true;
		EmitSound( "Hostage.Breath" );
	}
	else if ( m_bPlayingHostageCarrySound == true && !m_hCarriedHostage )
	{
		m_bPlayingHostageCarrySound = false;
		StopSound( "Hostage.Breath" );	
	}

	if ( m_bOldIsScoped != m_bIsScoped )
	{
		m_bOldIsScoped = m_bIsScoped;
		FogControllerChanged( true );
	}


	UpdateVisibility();
}


void C_CSPlayer::ValidateModelIndex( void )
{
	UpdateMinModels();
}

void C_CSPlayer::SetModelPointer( const model_t *pModel )
{
	bool bModelPointerIsChanged = ( pModel != GetModel() );
	
	BaseClass::SetModelPointer( pModel );

	if ( bModelPointerIsChanged )
	{
		m_bUseNewAnimstate = ( Q_stristr( modelinfo->GetModelName(GetModel()), "custom_player" ) != 0 );
		m_bAddonModelsAreOutOfDate = true; // next time we update addon models, do a complete refresh

		// apply BONE_ALWAYS_SETUP flag to certain hardcoded bone names, in case they're missing the flags in content
		CStudioHdr *pHdr = GetModelPtr();
		Assert( pHdr );
		if ( pHdr )
		{
			for ( int i=0; i<pHdr->numbones(); i++ )
			{
				if ( !V_stricmp( pHdr->pBone(i)->pszName(), "lh_ik_driver"	) ||
					 !V_stricmp( pHdr->pBone(i)->pszName(), "lean_root"		) ||
					 !V_stricmp( pHdr->pBone(i)->pszName(), "lfoot_lock"	) ||
					 !V_stricmp( pHdr->pBone(i)->pszName(), "rfoot_lock"	) ||
					 !V_stricmp( pHdr->pBone(i)->pszName(), "ball_l"		) ||
					 !V_stricmp( pHdr->pBone(i)->pszName(), "ball_r"		) ||
					 !V_stricmp( pHdr->pBone(i)->pszName(), "cam_driver"	) )
				{
					pHdr->setBoneFlags( i, BONE_ALWAYS_SETUP );
				}
			}
		}

	}
}

void C_CSPlayer::PostDataUpdate( DataUpdateType_t updateType )
{
	// C_BaseEntity assumes we're networking the entity's angles, so pretend that it
	// networked the same value we already have.
	SetNetworkAngles( GetLocalAngles() );

	BaseClass::PostDataUpdate( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		if ( m_bUseNewAnimstate && m_PlayerAnimStateCSGO )
		{
			m_PlayerAnimStateCSGO->Reset();
			//m_PlayerAnimStateCSGO->Update( EyeAngles()[YAW], EyeAngles()[PITCH] );
		}
	}

	/*if ( m_nLastObserverMode != GetObserverMode() )
	{
		DevMsg( "Player %d tick %d observer mode %d\n", this->index, gpGlobals->tickcount, GetObserverMode() );
		m_nLastObserverMode = GetObserverMode();
	}*/
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_CSPlayer::Interpolate( float currentTime )
{
	if ( !BaseClass::Interpolate( currentTime ) )
		return false;

	if ( CSGameRules()->IsFreezePeriod() )
	{
		// don't interpolate players position during freeze periode
		SetAbsOrigin( GetNetworkOrigin() );
	}

	return true;
}

void C_CSPlayer::PlayClientJumpSound( void )
{
	// during prediction play footstep sounds only once
	if ( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		return;

	CLocalPlayerFilter filter;
	EmitSound( filter, entindex(), "Default.WalkJump" );
}

int	C_CSPlayer::GetMaxHealth() const
{
	return 100;
}

bool C_CSPlayer::ShouldInterpolate()
{
	// [msmith] Do we need to check this for split screen as well?
	// If this is the player, (or being observed by the player ) then we want to interpolate it.
	if ( this == GetLocalOrInEyeCSPlayer() )
	{
		return true;
	}

	return BaseClass::ShouldInterpolate();
}

//-----------------------------------------------------------------------------
// Purpose: Return the local player, or the player being spectated in-eye
//-----------------------------------------------------------------------------
C_CSPlayer* GetLocalOrInEyeCSPlayer( void )
{
	C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();

	if( player && player->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		C_BaseEntity *target = player->GetObserverTarget();

		if( target && target->IsPlayer() )
		{
			return ToCSPlayer( target );
		}
	}
	return player;
}

//-----------------------------------------------------------------------------
// Purpose: Return the local player, or the player being spectated
//-----------------------------------------------------------------------------
C_CSPlayer* GetHudPlayer( void )
{
	C_CSPlayer *player = C_CSPlayer::GetLocalCSPlayer();

	if ( player && ( player->GetObserverMode() == OBS_MODE_IN_EYE || player->GetObserverMode() == OBS_MODE_CHASE ) )
	{
		C_BaseEntity *target = player->GetObserverTarget();

		if( target && target->IsPlayer() )
		{
			return ToCSPlayer( target );
		}
	}
	return player;
}


// FIXME(hpe ) sb: some hackery to get split screen up and running
// CON_COMMAND( joinsplitscreen, "join split screen" )
// {
// 	char splitScreenCommand[1024] = {0};
// 
// 	ACTIVE_SPLITSCREEN_PLAYER_GUARD( 1 );
// 	Q_snprintf(splitScreenCommand, sizeof( splitScreenCommand ), "jointeam 3\njoinclass\n" );
// 	//engine->ClientCmd_Unrestricted(splitScreenCommand );  // ClientCmd_Unrestricted uses in_forceuser so use ClientCmd which uses GET_ACTIVE_SPLITSCREEN_PLAYER
// 	engine->ClientCmd(splitScreenCommand );
// }


CON_COMMAND_F( rangefinder, "rangefinder", FCVAR_CHEAT )
{

	CBasePlayer *pPlayer = ToBasePlayer( C_CSPlayer::GetLocalCSPlayer() );

	if ( !pPlayer )
		return;

	// Rangefinder
	trace_t tr;

 	Vector vecForward;
 
 	AngleVectors( pPlayer->EyeAngles(), &vecForward );

	UTIL_TraceLine( pPlayer->EyePosition(), pPlayer->EyePosition() + vecForward * MAX_COORD_RANGE, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );

	float flDist_aim = ( tr.fraction != 1.0 ) ? ( tr.startpos - tr.endpos ).Length() : 0;
	
	if ( flDist_aim )
	{
		float flDist_aim = ( tr.startpos - tr.endpos ).Length();
		float flDist_aim2D = ( tr.startpos - tr.endpos ).Length2D();
		Msg( "\nStartPos: %.4f %.4f %.4f --- EndPos: %.4f %.4f %.4f\n", tr.startpos.x, tr.startpos.y, tr.startpos.z, tr.endpos.x, tr.endpos.y, tr.endpos.z );
		Msg( "3D Distance: %.4f units --- 2D Distance: %.4f units\n", flDist_aim, flDist_aim2D );
	}
}

#define MAX_FLASHBANG_OPACITY 75.0f

//-----------------------------------------------------------------------------
// Purpose: Update this client's targetid entity
//-----------------------------------------------------------------------------
ConVar clDrawTargetIDTrace( "clDrawTargetIDTrace", "0", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY, "visualizing line trace for target ID" );
void C_CSPlayer::UpdateIDTarget()
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( this );

	// Clear old target and find a new one
	m_iIDEntIndex = 0;

	// don't show IDs if mp_playerid == 2
	if ( mp_playerid.GetInt() == 2 )
		return;

	//Check how much of a screen fade we have.
	//if it's more than 75 then we can't see what's going on so we don't display the id.
	byte color[4];
	bool blend;
	GetViewEffects()->GetFadeParams( &color[0], &color[1], &color[2], &color[3], &blend );

	if ( color[3] > MAX_FLASHBANG_OPACITY && ( IsAlive() || GetObserverMode() == OBS_MODE_IN_EYE ) )
		 return;

	trace_t tr;
	Vector vecStart, vecEnd;

	Vector forward;
	QAngle viewAngles;
	float fov;
	Vector eyePos;

	CalcPlayerView( eyePos, viewAngles, fov );

	if ( PlatformInputDevice::IsInputDeviceAPointer( g_pInputSystem->GetCurrentInputDevice() ) )
	{
		forward = GetAimDirection();
	}
	else
	{
		AngleVectors( viewAngles, &forward );
	}

	// convert to vector
	VectorMA( eyePos , 2500, forward, vecEnd );
	VectorMA( eyePos , 10,   forward, vecStart );

	#define BLUE 0,0,255
	if( clDrawTargetIDTrace.GetBool() )
	{
		DebugDrawLine( vecStart, vecEnd, BLUE, true, 60.0f );
		clDrawTargetIDTrace.SetValue( 0 );
	}

	UTIL_TraceLine( vecStart, vecEnd, MASK_VISIBLE_AND_NPCS | MASK_SOLID, GetLocalOrInEyeCSPlayer(), COLLISION_GROUP_NONE, &tr );

	vecEnd = tr.endpos + (32 * forward);

	if ( !tr.startsolid && !tr.DidHitNonWorldEntity() )
	{
		CTraceFilterSimple filter( GetLocalOrInEyeCSPlayer(), COLLISION_GROUP_NONE );

		// Check for player hitboxes extending outside their collision bounds
		const float rayExtension = 40.0f;
		UTIL_ClipTraceToPlayers(vecStart, vecEnd + forward * rayExtension, MASK_SOLID|CONTENTS_HITBOX, &filter, &tr );
	}

	if ( !tr.startsolid && tr.DidHitNonWorldEntity() )
	{
		C_BaseEntity *pEntity = tr.m_pEnt;


		if ( pEntity && (pEntity != this ) )
		{
			if ( mp_playerid.GetInt() == 1 ) // only show team names
			{
				if ( pEntity->GetTeamNumber() != GetTeamNumber() )
				{
					return;
				}
			}

			if ( LineGoesThroughSmoke( vecStart, pEntity->WorldSpaceCenter(), 1.0f ) )
			{
				return;
			}

			// only test against hit boxes for enemy players
			if ( pEntity->GetTeamNumber() != GetTeamNumber() )
			{
				// trace again here to test hitboxes
				UTIL_TraceLine( vecStart, vecEnd, MASK_VISIBLE_AND_NPCS|CONTENTS_HITBOX, GetLocalOrInEyeCSPlayer(), COLLISION_GROUP_NONE, &tr );
				if ( (tr.surface.flags & SURF_HITBOX) == 0 )
					return;
			}

			if ( !GetIDTarget() && ( !m_iOldIDEntIndex || ( ( m_delayTargetIDTimer.GetRemainingRatio() == 0 ) && ( m_holdTargetIDTimer.GetRemainingRatio() == 0 ) ) ) )
			{
				// track when we first mouse over the target
				float flDelay = mp_playerid_delay.GetFloat();
				C_CSPlayer *pPlayer = ( C_CSPlayer* ) ToCSPlayer( pEntity );
				if ( pPlayer && pPlayer->IsAssassinationTarget() )
				{
					flDelay = 0; // Show assassination target names immediately
				}
				m_delayTargetIDTimer.Start( flDelay );
			}

			m_iIDEntIndex = pEntity->entindex();

			m_iOldIDEntIndex = m_iIDEntIndex;
			m_holdTargetIDTimer.Start( mp_playerid_hold.GetFloat() );
		}
	}
}

void C_CSPlayer::UpdateTargetedWeapon( void )
{	
	m_iTargetedWeaponEntIndex = 0;

	Vector aimDir;
	AngleVectors( GetFinalAimAngle(), &aimDir );

	// FIXME: if you drop a weapon at a teammates' feet, you won't get the HUD prompt text because the teammate id
	// trace (which uses the bounding box of the teammate) is prioritized in the hud over the prompt to pick up the weapon.
	// Pressing USE while looking at a weapon a teammate is standing on will still swap to it, since this trace is 
	// succeeding - but you don't get the on-screen prompt. This kinda sucks during buytime, ideally the hud should
	// support drawing the teammate name AND the weapon pickup promt at the same time.

	trace_t result;
	CTraceFilterOmitPlayers traceFilter; // don't hit players with this trace
	UTIL_TraceLine( EyePosition(), EyePosition() + MAX_WEAPON_NAME_POPUP_RANGE * aimDir, MASK_SHOT, &traceFilter, &result );

	if ( result.DidHitNonWorldEntity() && result.m_pEnt->IsBaseCombatWeapon() )
	{
		if ( LineGoesThroughSmoke( EyePosition(), result.m_pEnt->WorldSpaceCenter(), 1.0f ) )
			return;

		//now that we have a weapon, we check to see if we are also looking at a bomb
		// setting the weaponEntIndex to the bomb prevents the hint coming up to 
		// pick up a weapon if it occupies the same space as a bomb
		if ( GetUsableHighPriorityEntity() )
			return;
		
		// Set if to point at the weapon
		m_iTargetedWeaponEntIndex = result.m_pEnt->entindex();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Input handling
//-----------------------------------------------------------------------------
bool C_CSPlayer::CreateMove( float flInputSampleTime, CUserCmd *pCmd )
{
	// Bleh... we will wind up needing to access bones for attachments in here.
	C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, true );

	static QAngle angMoveAngle( 0.0f, 0.0f, 0.0f );

	bool bNoTaunt = true;
	bool bInTaunt = IsTaunting() && IsThirdPersonTaunt();
	if ( bInTaunt )
	{
		pCmd->forwardmove = 0.0f;
		pCmd->sidemove = 0.0f;
		pCmd->upmove = 0.0f;
		pCmd->weaponselect = 0;
		pCmd->buttons = 0;

		VectorCopy( angMoveAngle, pCmd->viewangles );
		bNoTaunt = false;
	}
	else
	{
		VectorCopy( pCmd->viewangles, angMoveAngle );
	}

	BaseClass::CreateMove( flInputSampleTime, pCmd );

	return bNoTaunt;
}

//-----------------------------------------------------------------------------
// Purpose: Flash this entity on the radar
//-----------------------------------------------------------------------------
bool C_CSPlayer::IsInHostageRescueZone()
{
	return 	m_bInHostageRescueZone;
}

void C_CSPlayer::SetBuyMenuOpen( bool bOpen ) 
{ 
	m_bIsBuyMenuOpen = bOpen;
	
	if ( bOpen == true )
		engine->ClientCmd( "open_buymenu" );
	else
		engine->ClientCmd( "close_buymenu" );
} 

CWeaponCSBase* C_CSPlayer::GetActiveCSWeapon() const
{
	return assert_cast< CWeaponCSBase* >( GetActiveWeapon() );
}

CWeaponCSBase* C_CSPlayer::GetCSWeapon( CSWeaponID id ) const
{
	for (int i=0;i<MAX_WEAPONS;i++ ) 
	{
		CBaseCombatWeapon *weapon = GetWeapon( i );
		if ( weapon )
		{
			CWeaponCSBase *csWeapon = assert_cast< CWeaponCSBase * >( weapon );
			if ( csWeapon )
			{
				if ( id == csWeapon->GetCSWeaponID() )
				{
					return csWeapon;
				}
			}
		}
	}

	return NULL;
}

//REMOVEME
/*
void C_CSPlayer::SetFireAnimation( PLAYER_ANIM playerAnim )
{
	Activity idealActivity = ACT_WALK;

	// Figure out stuff about the current state.
	float speed = GetAbsVelocity().Length2D();
	bool isMoving = ( speed != 0.0f ) ? true : false;
	bool isDucked = ( GetFlags() & FL_DUCKING ) ? true : false;
	bool isStillJumping = false; //!( GetFlags() & FL_ONGROUND );
	bool isRunning = false;

	if ( speed > ARBITRARY_RUN_SPEED )
	{
		isRunning = true;
	}

	// Now figure out what to do based on the current state and the new state.
	switch ( playerAnim )
	{
	default:
	case PLAYER_RELOAD:
	case PLAYER_ATTACK1:
	case PLAYER_IDLE:
	case PLAYER_WALK:
		// Are we still jumping?
		// If so, keep playing the jump animation.
		if ( !isStillJumping )
		{
			idealActivity = ACT_WALK;

			if ( isDucked )
			{
				idealActivity = !isMoving ? ACT_CROUCHIDLE : ACT_RUN_CROUCH;
			}
			else
			{
				if ( isRunning )
				{
					idealActivity = ACT_RUN;
				}
				else
				{
					idealActivity = isMoving ? ACT_WALK : ACT_IDLE;
				}
			}

			// Allow body yaw to override for standing and turning in place
			idealActivity = m_PlayerAnimState.BodyYawTranslateActivity( idealActivity );
		}
		break;

	case PLAYER_JUMP:
		idealActivity = ACT_HOP;
		break;

	case PLAYER_DIE:
		// Uses Ragdoll now???
		idealActivity = ACT_DIESIMPLE;
		break;

	// FIXME:  Use overlays for reload, start/leave aiming, attacking
	case PLAYER_START_AIMING:
	case PLAYER_LEAVE_AIMING:
		idealActivity = ACT_WALK;
		break;
	}
	
	CWeaponCSBase *pWeapon = GetActiveCSWeapon();
				
	if ( pWeapon )
	{
		Activity aWeaponActivity = idealActivity;

		if ( playerAnim == PLAYER_ATTACK1 )
		{
			switch ( idealActivity )
			{
				case ACT_WALK:
				default:
					aWeaponActivity = ACT_PLAYER_WALK_FIRE;
					break;
				case ACT_RUN:
					aWeaponActivity = ACT_PLAYER_RUN_FIRE;
					break;
				case ACT_IDLE:
					aWeaponActivity = ACT_PLAYER_IDLE_FIRE;
					break;
				case ACT_CROUCHIDLE:
					aWeaponActivity = ACT_PLAYER_CROUCH_FIRE;
					break;
				case ACT_RUN_CROUCH:
					aWeaponActivity = ACT_PLAYER_CROUCH_WALK_FIRE;
					break;
			}
		}

		m_PlayerAnimState.SetWeaponLayerSequence( pWeapon->GetPlayerAnimationExtension(), aWeaponActivity );
	}
}
*/

ShadowType_t C_CSPlayer::ShadowCastType( void ) 
{
	if ( !IsVisible() )
		 return SHADOWS_NONE;

	return SHADOWS_RENDER_TO_TEXTURE_DYNAMIC;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether or not we can switch to the given weapon.
// Input  : pWeapon - 
//-----------------------------------------------------------------------------
bool C_CSPlayer::Weapon_CanSwitchTo( CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon->CanDeploy() )
		return false;
	
	if ( GetActiveWeapon() )
	{
		if ( !GetActiveWeapon()->CanHolster() )
			return false;
	}

	return true;
}

ConVar clTaserShakeFreqMin( "clTaserShakeFreqMin", "0.2", 0, "how often the shake is applied (min time)" );
ConVar clTaserShakeFreqMax( "clTaserShakeFreqMax", "0.7", 0, "how often the shake is applied (max time)" );

ConVar clTaserShakeTimeTotal( "clTaserShakeTimeTotal", "7.0", 0, "time the taser shake is applied." );


void C_CSPlayer::HandleTaserAnimation()
{
	if ( m_bClientSideRagdoll && m_bKilledByTaser )
	{
		if ( m_nextTaserShakeTime < gpGlobals->curtime )
		{
			// we're ready to apply a taser force
			C_CSRagdoll *pRagdoll = (C_CSRagdoll* )m_hRagdoll.Get();
			if ( pRagdoll )
			{
				pRagdoll->ApplyRandomTaserForce();
			}

			if ( m_firstTaserShakeTime == 0.0f )
			{
				m_firstTaserShakeTime = gpGlobals->curtime;
				EmitSound("Player.DeathTaser" ); // play death audio here
			}

			if ( m_firstTaserShakeTime + clTaserShakeTimeTotal.GetFloat() < gpGlobals->curtime )
			{
				// we've waited more than clTaserShakeTimeTotal since our first shake so we're done with the taze effect...  AKA: "DON'T TAZE ME BRO"
				m_bKilledByTaser = false;
				m_firstTaserShakeTime = 0.0f;
			}
			else
			{
				// set the timer for our next shake
				m_nextTaserShakeTime = gpGlobals->curtime + RandomFloat( clTaserShakeFreqMin.GetFloat(), clTaserShakeFreqMax.GetFloat() );
			}
		}
	}
}


void C_CSPlayer::UpdateClientSideAnimation()
{
	if ( m_bUseNewAnimstate )
	{
		m_PlayerAnimStateCSGO->Update( EyeAngles()[YAW], EyeAngles()[PITCH] );
	}
	else
	{

		// We do this in a different order than the base class.
		// We need our cycle to be valid for when we call the playeranimstate update code, 
		// or else it'll synchronize the upper body anims with the wrong cycle.
		
		if ( GetSequence() != -1 )
		{
			// move frame forward
			FrameAdvance( 0.0f ); // 0 means to use the time we last advanced instead of a constant
		}

		if ( C_BasePlayer::IsLocalPlayer( this ) )
			m_PlayerAnimState->Update( EyeAngles()[YAW], EyeAngles()[PITCH] );
		else
			m_PlayerAnimState->Update( m_angEyeAngles[YAW], m_angEyeAngles[PITCH] );
	}
		
	if ( GetSequence() != -1 )
	{
		// latch old values
		OnLatchInterpolatedVariables( LATCH_ANIMATION_VAR );
	}

	if ( m_bKilledByTaser )
	{
		HandleTaserAnimation();
	}

	// We only update the view model for the local player.
	if ( IsLocalPlayer( this ) )
	{
		CWeaponCSBase *pWeapon = GetActiveCSWeapon();
		if ( pWeapon )
		{
			C_BaseViewModel *pViewModel = assert_cast<C_BaseViewModel *>( GetViewModel( pWeapon->m_nViewModelIndex ) );
			if ( pViewModel )
			{
				pViewModel->UpdateAllViewmodelAddons();
			}
		}
		else
		{
			//We have a null weapon so remove the add ons for all the view models for this player.
			for ( int i=0; i<MAX_VIEWMODELS; ++i )
			{
				C_BaseViewModel *pViewModel = assert_cast<C_BaseViewModel *>( GetViewModel( i ) );
				if ( pViewModel )
				{
					pViewModel->RemoveViewmodelArmModels();
					pViewModel->RemoveViewmodelLabel();
					pViewModel->RemoveViewmodelStatTrak();
					pViewModel->RemoveViewmodelStickers();
				}
			}
		}
	}
}


float g_flMuzzleFlashScale=1;

void C_CSPlayer::ProcessMuzzleFlashEvent()
{
	CWeaponCSBase *pWeapon = GetActiveCSWeapon();

	if ( !pWeapon )
		return;

	int splitScreenRenderFlags = 0x0;
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

		// We don't want to show third person muzzle flash effects for this player if the splitscreen viewer is looking at this player in first person mode.
		bool viewingInFirstPersonMode = ( pLocalPlayer == this && !pLocalPlayer->ShouldDraw() ) ||
										( pLocalPlayer && pLocalPlayer->GetObserverTarget() == this && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE && !pLocalPlayer->IsInObserverInterpolation() );

		if ( !viewingInFirstPersonMode )
		{
			splitScreenRenderFlags |= (0x1 << hh );
		}
	}

	// Don't draw the effects if this third person weapon is not drawing in any screen.
	if ( 0 == splitScreenRenderFlags )
	{
		return;
	}
	else if ( splitScreenRenderFlags >= 3 )
	{
		// Seen by both split screen players 1 and 2 so use -1 for visible to all split screen players.
		splitScreenRenderFlags = -1;
	}
	else
	{
		splitScreenRenderFlags -= 1;
	}


	// Muzzle Flash Effect.
	int iAttachmentIndex = pWeapon->GetMuzzleAttachmentIndex_3rdPerson();
	const char* pszEffect = pWeapon->GetMuzzleFlashEffectName_3rdPerson();
	CBaseWeaponWorldModel *pWeaponWorldModel = pWeapon->GetWeaponWorldModel();

	if ( !pWeaponWorldModel )
		return;

	if ( pszEffect && Q_strlen(pszEffect ) > 0 && iAttachmentIndex >= 0 && pWeaponWorldModel && pWeaponWorldModel->ShouldDraw() && pWeaponWorldModel->IsVisible() && !pWeaponWorldModel->HasDormantOwner() )
	{
		pWeaponWorldModel->GetAttachment( iAttachmentIndex, m_vecLastMuzzleFlashPos, m_angLastMuzzleFlashAngle );
		DispatchParticleEffect( pszEffect, PATTACH_POINT_FOLLOW, pWeaponWorldModel, iAttachmentIndex, false, splitScreenRenderFlags );
	}

	// Brass Eject Effect.
	iAttachmentIndex = pWeapon->GetEjectBrassAttachmentIndex_3rdPerson();
	pszEffect = pWeapon->GetEjectBrassEffectName();
	if ( pszEffect && Q_strlen(pszEffect ) > 0 && iAttachmentIndex >= 0 && pWeaponWorldModel && pWeaponWorldModel->ShouldDraw() && pWeaponWorldModel->IsVisible() && !pWeaponWorldModel->HasDormantOwner() )
	{
		DispatchParticleEffect( pszEffect, PATTACH_POINT_FOLLOW, pWeaponWorldModel, iAttachmentIndex, false, splitScreenRenderFlags );
	}
}

const QAngle& C_CSPlayer::EyeAngles()
{
	if ( IsLocalPlayer( this ) && !g_nKillCamMode && !g_bEngineIsHLTV )
	{
		return BaseClass::EyeAngles();
	}
	else
	{
		return m_angEyeAngles;
	}
}

bool C_CSPlayer::ShouldDraw( void )
{
	// If we're dead, our ragdoll will be drawn for us instead.
	if ( !IsAlive() )
		return false;

	if( GetTeamNumber() == TEAM_SPECTATOR )
		return false;

	if( IsLocalPlayer( this ) )
	{
		if ( IsRagdoll() )
			return true;

		return ShouldDrawLocalPlayer();
	}

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

	// keep drawing players we're observing with the interpolating spectator camera
	if ( pLocalPlayer && pLocalPlayer->GetObserverInterpState() == OBSERVER_INTERP_TRAVELING )
	{
		return true;
	}

	// don't draw players we're observing in first-person
	if ( pLocalPlayer && pLocalPlayer->GetObserverTarget() == ToBasePlayer(this) && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
	{
		return false;
	}

	return BaseClass::ShouldDraw();
}

#define APPROX_CENTER_PLAYER Vector(0,0,50)

bool C_CSPlayer::GetAttachment( int number, matrix3x4_t &matrix )
{
	if ( IsAnimLODflagSet(ANIMLODFLAG_OUTSIDEVIEWFRUSTUM) || IsDormant() )
	{
		MatrixCopy( EntityToWorldTransform(), matrix );
		matrix.SetOrigin( matrix.GetOrigin() + APPROX_CENTER_PLAYER );
		return true;
	}

	return BaseClass::GetAttachment( number, matrix );
}

bool C_CSPlayer::GetAttachment( int number, Vector &origin )
{
	if ( IsAnimLODflagSet(ANIMLODFLAG_OUTSIDEVIEWFRUSTUM) || IsDormant() )
	{
		origin = GetAbsOrigin() + APPROX_CENTER_PLAYER;
		return true;
	}
	return BaseClass::GetAttachment( number, origin );
}

bool C_CSPlayer::GetAttachment( int number, Vector &origin, QAngle &angles )
{
	if ( IsAnimLODflagSet(ANIMLODFLAG_OUTSIDEVIEWFRUSTUM) || IsDormant() )
	{
		origin = GetAbsOrigin() + APPROX_CENTER_PLAYER;
		angles = GetAbsAngles();
		return true;
	}
	return BaseClass::GetAttachment( number, origin, angles );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
#ifdef DEBUG
ConVar cl_animlod_dotproduct( "cl_animlod_dotproduct", "0.3" );
#define animlod_dotproduct cl_animlod_dotproduct.GetFloat()
#else
#define animlod_dotproduct 0.3
#endif
void C_CSPlayer::ReevauluateAnimLOD( int boneMask )
{
	
	if ( !engine->IsHLTV() && gpGlobals->framecount != m_nComputedLODframe )
	{
		m_nCustomBlendingRuleMask = -1;

		bool bFirstSetup = ( m_nComputedLODframe == 0 );

		m_nComputedLODframe = gpGlobals->framecount;

		// save off the old flags before we reset and recompute the new ones, so we have a one-step record of change
		m_nAnimLODflagsOld = m_nAnimLODflags;

		ClearAnimLODflags();

		if ( !bFirstSetup )
		{
			if ( !IsVisible() || IsDormant() || (IsLocalPlayer( this ) && !C_BasePlayer::ShouldDrawLocalPlayer()) || !ShouldDraw() )
			{
				// always do cheap bone setup for an invisible local player
				SetAnimLODflag( ANIMLODFLAG_INVISIBLELOCALPLAYER );
			}
			else
			{

				// is this player being interpolated towards by an observer camera?
				C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
				bool bTargetOfInterpolatingObsCam = ( pLocalPlayer && pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE && 
					pLocalPlayer->GetObserverTarget() == ToBasePlayer(this) && 
					pLocalPlayer->GetObserverInterpState() != OBSERVER_INTERP_NONE );

				if ( !bTargetOfInterpolatingObsCam )
				{
					// if this player is behind the camera and beyond a certain distance, perform only cheap and simple bone setup.
					Vector vecEyeToPlayer = EyePosition() - MainViewOrigin(GET_ACTIVE_SPLITSCREEN_SLOT());
					m_flDistanceFromCamera = vecEyeToPlayer.Length();

					if ( m_flDistanceFromCamera > 400.0f )
					{
						SetAnimLODflag( ANIMLODFLAG_DISTANT );

						Vector vecEyeDir = MainViewForward(GET_ACTIVE_SPLITSCREEN_SLOT());
						float flEyeDirToPlayerDirDot = DotProduct( vecEyeToPlayer.Normalized(), vecEyeDir.Normalized() );

						if ( flEyeDirToPlayerDirDot < animlod_dotproduct )
						{
							SetAnimLODflag( ANIMLODFLAG_OUTSIDEVIEWFRUSTUM );
						}
					}
				}
			}
		}

		// weapon world model mimics player's anim lod flags
		C_BaseCombatWeapon *pWeapon = GetActiveWeapon();
		if ( pWeapon )
		{
			CBaseWeaponWorldModel *pWeaponWorldModel = pWeapon->m_hWeaponWorldModel.Get();
			if ( pWeaponWorldModel )
			{
				pWeaponWorldModel->m_nAnimLODflags = m_nAnimLODflags;
				pWeaponWorldModel->m_nAnimLODflagsOld = m_nAnimLODflagsOld;
			}
		}

		bool bCrossedDistanceThreshold = ((m_nAnimLODflags & ANIMLODFLAG_DISTANT) != 0) != ((m_nAnimLODflagsOld & ANIMLODFLAG_DISTANT) != 0);

		// unless this is the first setup or the lod state is changing this frame, use a much more conservative bone mask for distant lod
		if ( !bFirstSetup && IsAnimLODflagSet( ANIMLODFLAG_DISTANT ) && !bCrossedDistanceThreshold )
		{
			m_nCustomBlendingRuleMask = (BONE_USED_BY_ATTACHMENT | BONE_USED_BY_HITBOX);
		}

		// if the player has just become awake (no longer dormant) then we should set up all the bones (treat it like a first setup)
		if ( bFirstSetup || (IsDormant() && !(m_nAnimLODflagsOld & ANIMLODFLAG_DORMANT)) )
		{
			m_nCustomBlendingRuleMask = -1;
		}

		//if ( bCrossedDistanceThreshold )
		//{
		//	if ( IsAnimLODflagSet( ANIMLODFLAG_DISTANT ) )
		//	{
		//		debugoverlay->AddTextOverlay( GetAbsOrigin(), 5, "Distant" );
		//	}
		//	else
		//	{
		//		debugoverlay->AddTextOverlay( GetAbsOrigin(), 5, "Closer" );
		//	}
		//}
		//
		//bool bEnteredFrustum = ((m_nAnimLODflags & ANIMLODFLAG_OUTSIDEVIEWFRUSTUM) != 0) != ((m_nAnimLODflagsOld & ANIMLODFLAG_OUTSIDEVIEWFRUSTUM) != 0);
		//
		//if ( bEnteredFrustum )
		//{
		//	if ( !IsAnimLODflagSet( ANIMLODFLAG_OUTSIDEVIEWFRUSTUM ) )
		//	{
		//		debugoverlay->AddTextOverlay( GetAbsOrigin(), 2, "Enter frustum" );
		//	}
		//}

	}

	//	// player models don't have explicit levels of vertex lod - yet. This is how vert lod would be masked:
	//	studiohwdata_t *pHardwareData = g_pMDLCache->GetHardwareData( modelinfo->GetCacheHandle( GetModel() ) );
	//	int nHighLod = MAX( pHardwareData->m_RootLOD, pHardwareData->m_NumLODs - 1 );
	//	m_nCustomBlendingRuleMask ... BONE_USED_BY_VERTEX_AT_LOD(nHighLod);
}

bool C_CSPlayer::SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime )
{
	ReevauluateAnimLOD( boneMask );

	return BaseClass::SetupBones( pBoneToWorldOut, nMaxBones, boneMask, currentTime );
}


void C_CSPlayer::AccumulateLayers( IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime )
{
	if ( !engine->IsHLTV() && IsAnimLODflagSet( ANIMLODFLAG_DORMANT | ANIMLODFLAG_OUTSIDEVIEWFRUSTUM ) )
		return;

	C_BaseCombatWeapon *pWeapon = GetActiveWeapon();
	CBaseWeaponWorldModel *pWeaponWorldModel = NULL;
	if ( pWeapon )
		pWeaponWorldModel = pWeapon->m_hWeaponWorldModel.Get();

	if ( pWeapon && pWeaponWorldModel && m_bUseNewAnimstate && m_PlayerAnimStateCSGO )
	{
		m_PlayerAnimStateCSGO->OnClientWeaponChange( GetActiveCSWeapon() );

		//pre-bone-setup snapshot capture to grab bones before a deleted weapon pops the pose

		int oldReadableBones = m_BoneAccessor.GetReadableBones();
		m_BoneAccessor.SetReadableBones( BONE_USED_BY_ANYTHING );

		m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].UpdateReadOnly();
		m_boneSnapshots[BONESNAPSHOT_UPPER_BODY].UpdateReadOnly();

		m_BoneAccessor.SetReadableBones( oldReadableBones );
		
		AccumulateInterleavedDispatchedLayers( pWeaponWorldModel, boneSetup, pos, q, currentTime, GetLocalOrInEyeCSPlayer() == this );
		return;
	}

	BaseClass::AccumulateLayers( boneSetup, pos, q, currentTime );
}


void C_CSPlayer::NotifyOnLayerChangeSequence( const CAnimationLayer* pLayer, const int nNewSequence )
{
	if ( pLayer && m_bUseNewAnimstate && m_PlayerAnimStateCSGO )
	{
		m_PlayerAnimStateCSGO->NotifyOnLayerChangeSequence( pLayer, nNewSequence );
	}
}

void C_CSPlayer::NotifyOnLayerChangeWeight( const CAnimationLayer* pLayer, const float flNewWeight )
{
	if ( pLayer && m_bUseNewAnimstate && m_PlayerAnimStateCSGO )
	{
		m_PlayerAnimStateCSGO->NotifyOnLayerChangeWeight( pLayer, flNewWeight );
	}
}

void C_CSPlayer::NotifyOnLayerChangeCycle( const CAnimationLayer* pLayer, const float flNewCycle )
{
	if ( pLayer && m_bUseNewAnimstate && m_PlayerAnimStateCSGO )
	{
		m_PlayerAnimStateCSGO->NotifyOnLayerChangeCycle( pLayer, flNewCycle );
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

void C_CSPlayer::DoAnimStateEvent( PlayerAnimEvent_t evt )
{
	m_PlayerAnimState->DoAnimationEvent( evt );
}

void CBoneSnapshot::Update( CBaseAnimating* pEnt, bool bReadOnly )
{
	if ( m_flWeight > 0 )
		m_flWeight = clamp( smoothstep_bounds( m_flDecayEndTime, m_flDecayStartTime, gpGlobals->curtime ), 0, 1 );

	// if the last known bonesetup occurred too long ago, it doesn't make sense to capture a snapshot of severely outdated or possibly uninitialized bones.
	if ( !IsBoneSetupTimeIndexRecent() || !pEnt )
		return;

	if ( pEnt != m_pEnt )
	{
		Init( pEnt );
		return;
	}

	if ( !m_bEnabled || m_pEnt->Teleported() || m_pEnt->IsEffectActive(EF_NOINTERP) )
	{
		AbandonAnyPending();
		return;
	}

	C_CSPlayer* pPlayer = ToCSPlayer( m_pEnt );
	if ( pPlayer && ( pPlayer->IsAnimLODflagSet(ANIMLODFLAG_INVISIBLELOCALPLAYER|ANIMLODFLAG_DORMANT|ANIMLODFLAG_OUTSIDEVIEWFRUSTUM) || (gpGlobals->curtime - pPlayer->m_flLastSpawnTimeIndex) <= 0.5f ) )
	{
		AbandonAnyPending();
		return;
	}

	if ( !m_bWeightlistInitialized )
		InitWeightList();

	if ( !bReadOnly && m_flWeight > 0 )
		PlaybackSnapshot();

	if ( IsCapturePending() )
		CaptureSnapshot();
}

void CBoneSnapshot::CaptureSnapshot( void )
{
	if ( !m_bCapturePending || !m_pEnt || !m_pEnt->IsVisible() )
		return;

	CStudioHdr *pHdr = m_pEnt->GetModelPtr();
	if ( !pHdr )
		return;

	const studiohdr_t *pRenderHdr = pHdr->GetRenderHdr();
	if ( pRenderHdr )
	{
		if ( m_nStudioRenderHdrId == -1 )
		{
			m_nStudioRenderHdrId = pRenderHdr->id;
		}
		else if ( m_nStudioRenderHdrId != pRenderHdr->id )
		{
			// render hdr id changed underneath us, likely a model swap
			Init( m_pEnt );
			return;
		}
	}

	matrix3x4_t matPlayer;
	AngleMatrix( m_pEnt->GetRenderAngles(), m_pEnt->GetRenderOrigin(), matPlayer );
	matrix3x4_t matPlayerInv = matPlayer.InverseTR();

	for (int i = 0; i < pHdr->numbones(); ++i)
	{
		if ( m_Weightlist[i] <= 0 )
			continue;

		ConcatTransforms( matPlayerInv, m_pEnt->GetBone(i), m_Cache[i] );
	}

	FOR_EACH_VEC( m_vecSubordinateSnapshots, i )
	{
		if ( m_vecSubordinateSnapshots[i] )
			m_vecSubordinateSnapshots[i]->AbandonAnyPending();
	}

	m_vecWorldCapturePos = matPlayer.GetOrigin();

	m_flWeight = 1;

	m_bCapturePending = false;
}

void CBoneSnapshot::PlaybackSnapshot( void )
{
	if ( !m_pEnt )
		return;

	CStudioHdr *pHdr = m_pEnt->GetModelPtr();
	if ( !pHdr || !m_pEnt->IsVisible() || m_flWeight <= 0 )
		return;

	matrix3x4_t matPlayer;
	AngleMatrix( m_pEnt->GetRenderAngles(), m_pEnt->GetRenderOrigin(), matPlayer );

	float flFailsafeDistance = matPlayer.GetOrigin().DistToSqr( m_vecWorldCapturePos );
	m_flWeight = MIN( m_flWeight, RemapValClamped( flFailsafeDistance, 484.0f, 1296.0f, 1.0f, 0.0f ) );

	if ( m_flWeight <= 0 )
		return;

	for (int i = 0; i < pHdr->numbones(); ++i) 
	{
		if ( m_Weightlist[i] <= 0 )
			continue;

		float flWeightedElement = m_flWeight * m_Weightlist[i];

		matrix3x4_t matCurrent = m_pEnt->GetBone(i);
		matrix3x4_t matCached = ConcatTransforms( matPlayer, m_Cache[i] );

		Vector posCurrent = matCurrent.GetOrigin();
		Vector posCached = matCached.GetOrigin();

		if ( posCurrent.DistToSqr( posCached ) > 5000 )
		{
//#ifdef DEBUG
//			AssertMsgOnce( false, "Warning: Bonesnapshot lerp distance is too large.\n" );
//#endif
			break;
		}

		Quaternion qCurrent;
		MatrixQuaternion( matCurrent, qCurrent );

		Quaternion qCached;
		MatrixQuaternion( matCached, qCached );

		Quaternion qLerpOutput;
		QuaternionSlerp( qCurrent, qCached, flWeightedElement, qLerpOutput );
		
		Vector posLerpOutput = Lerp( flWeightedElement, posCurrent, posCached );

		AngleMatrix( RadianEuler( qLerpOutput ), posLerpOutput, m_pEnt->GetBoneForWrite(i) );
	}
}

void CBoneSnapshot::InitWeightList( void )
{
	if ( !m_pEnt )
		return;

	const model_t *pModel = m_pEnt->GetModel();
	if ( !pModel )
		return;

	KeyValues *pModelKV = modelinfo->GetModelKeyValues( pModel );
	if ( !pModelKV )
		return;

	pModelKV = pModelKV->FindKey( m_szWeightlistName );
	if ( !pModelKV )
		return;

	for ( int i=0; i<MAXSTUDIOBONES; i++ )
		m_Weightlist[i] = 1;

	FOR_EACH_SUBKEY( pModelKV, pBoneWeightKV )
	{
		int nBoneIdx = m_pEnt->LookupBone( pBoneWeightKV->GetName() );
		if ( nBoneIdx >= 0 && nBoneIdx < MAXSTUDIOBONES )
		{
			m_Weightlist[ nBoneIdx ] = pBoneWeightKV->GetFloat();
			//DevMsg( "Populating weightlist bone: %s (index %i) -> [%f]\n", pBoneWeightKV->GetName(), nBoneIdx, m_Weightlist[ nBoneIdx ] );
		}
	}

	m_bWeightlistInitialized = true;
}

bool C_CSPlayer::IsAnyBoneSnapshotPending( void )
{
	return ( m_boneSnapshots[BONESNAPSHOT_UPPER_BODY].IsCapturePending() || m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].IsCapturePending() );
}

#define CS_ARM_HYPEREXTENSION_LIM 22.0f
#define CS_ARM_HYPEREXTENSION_LIM_SQR 484.0f
#define cl_player_toe_length 4.5
void C_CSPlayer::DoExtraBoneProcessing( CStudioHdr *pStudioHdr, BoneVector pos[], BoneQuaternion q[], matrix3x4a_t boneToWorld[], CBoneBitList &boneComputed, CIKContext *pIKContext )
{
	if ( !m_bUseNewAnimstate || !m_PlayerAnimStateCSGO || IsAnimLODflagSet(ANIMLODFLAG_DORMANT|ANIMLODFLAG_INVISIBLELOCALPLAYER|ANIMLODFLAG_OUTSIDEVIEWFRUSTUM) )
		return;
	
	if ( !IsVisible() || (IsLocalPlayer( this ) && !C_BasePlayer::ShouldDrawLocalPlayer()) || !ShouldDraw() )
		return;

	mstudioikchain_t *pLeftFootChain = NULL;
	mstudioikchain_t *pRightFootChain = NULL;
	mstudioikchain_t *pLeftArmChain = NULL;

	int nLeftFootBoneIndex = LookupBone( "ankle_L" );
	int nRightFootBoneIndex = LookupBone( "ankle_R" );
	int nLeftHandBoneIndex = LookupBone( "hand_L" );

	Assert( nLeftFootBoneIndex != -1 && nRightFootBoneIndex != -1 && nLeftHandBoneIndex != -1 );

	for( int i = 0; i < pStudioHdr->numikchains(); i++ )
	{
		mstudioikchain_t *pchain = pStudioHdr->pIKChain( i );
		if ( nLeftFootBoneIndex == pchain->pLink( 2 )->bone )
		{
			pLeftFootChain = pchain;
		}
		else if ( nRightFootBoneIndex == pchain->pLink( 2 )->bone )
		{
			pRightFootChain = pchain;
		}
		else if ( nLeftHandBoneIndex == pchain->pLink( 2 )->bone )
		{
			pLeftArmChain = pchain;
		}

		if ( pLeftFootChain && pRightFootChain && pLeftArmChain )
			break;
	}
	
	Assert( pLeftFootChain && pRightFootChain );
	
	Vector vecAnimatedLeftFootPos = boneToWorld[nLeftFootBoneIndex].GetOrigin();
	Vector vecAnimatedRightFootPos = boneToWorld[nRightFootBoneIndex].GetOrigin();

	m_PlayerAnimStateCSGO->DoProceduralFootPlant( boneToWorld, pLeftFootChain, pRightFootChain, pos );
	

	// hack - keep the toes above the ground
	if ( (GetFlags() & FL_ONGROUND) && (GetMoveType() == MOVETYPE_WALK) )
	{
		float flZMaxToe = GetAbsOrigin().z + 0.75f;

		int nLeftToeBoneIndex = LookupBone( "ball_L" );
		int nRightToeBoneIndex = LookupBone( "ball_R" );

		if ( nLeftToeBoneIndex > 0 )
		{
			// need to build an extended toe position
			Vector vecToeLeft = boneToWorld[nLeftFootBoneIndex].TransformVector( pos[nLeftToeBoneIndex] );
			Vector vecForward;
			MatrixGetColumn( boneToWorld[nLeftToeBoneIndex], 0, vecForward );
			vecToeLeft += vecForward * cl_player_toe_length;
			if ( vecToeLeft.z < flZMaxToe )
			{
				boneToWorld[nLeftFootBoneIndex][2][3] += (flZMaxToe - vecToeLeft.z);
			}
		}

		if ( nRightToeBoneIndex > 0 )
		{
			Vector vecToeRight = boneToWorld[nRightFootBoneIndex].TransformVector( pos[nRightToeBoneIndex] );
			Vector vecForward;
			MatrixGetColumn( boneToWorld[nRightToeBoneIndex], 0, vecForward );
			vecToeRight -= vecForward * cl_player_toe_length; // right toe bone is backwards...
			if ( vecToeRight.z < flZMaxToe )
			{
				boneToWorld[nRightFootBoneIndex][2][3] += (flZMaxToe - vecToeRight.z);
			}
		}
	}

	Vector vecLeftFootPos = boneToWorld[nLeftFootBoneIndex].GetOrigin();
	Vector vecRightFootPos = boneToWorld[nRightFootBoneIndex].GetOrigin();

	boneToWorld[nLeftFootBoneIndex].SetOrigin( vecAnimatedLeftFootPos );
	boneToWorld[nRightFootBoneIndex].SetOrigin( vecAnimatedRightFootPos );

	Studio_SolveIK( pLeftFootChain->pLink( 0 )->bone, pLeftFootChain->pLink( 1 )->bone, nLeftFootBoneIndex, vecLeftFootPos, boneToWorld );
	Studio_SolveIK( pRightFootChain->pLink( 0 )->bone, pRightFootChain->pLink( 1 )->bone, nRightFootBoneIndex, vecRightFootPos, boneToWorld );


	int nLeftHandIkBoneDriver = LookupBone( "lh_ik_driver" );
	if ( nLeftHandIkBoneDriver > 0 && pos[nLeftHandIkBoneDriver].x > 0 )
	{
		MDLCACHE_CRITICAL_SECTION();

		int nRightHandWepBoneIndex = LookupBone( "weapon_hand_R" );
		if ( nRightHandWepBoneIndex > 0 )
		{
			// early out if the bone isn't in the ikcontext mask
			CStudioHdr *pPlayerHdr = GetModelPtr();
			if ( !(pPlayerHdr->boneFlags( nRightHandWepBoneIndex ) & pIKContext->GetBoneMask()) )
				return;

			C_BaseCombatWeapon *pWeapon = GetActiveWeapon();
			if ( pWeapon )
			{
				CBaseWeaponWorldModel *pWeaponWorldModel = pWeapon->m_hWeaponWorldModel.Get();
				if ( pWeaponWorldModel && pWeaponWorldModel->IsVisible() && pWeaponWorldModel->GetLeftHandAttachBoneIndex() != -1 )
				{
					int nWepAttach = pWeaponWorldModel->GetLeftHandAttachBoneIndex();
					if ( nWepAttach > -1 )
					{
						// make sure the left hand attach bone is marked for setup
						CStudioHdr *pHdr = pWeaponWorldModel->GetModelPtr();
						if ( !( pHdr->boneFlags(nWepAttach) & BONE_ALWAYS_SETUP ) )
							pHdr->setBoneFlags( nWepAttach, BONE_ALWAYS_SETUP );

						if ( pHdr->boneParent( nWepAttach ) != -1 && pWeaponWorldModel->isBoneAvailableForRead(nWepAttach) )
						{
							pIKContext->BuildBoneChain( pos, q, nRightHandWepBoneIndex, boneToWorld, boneComputed );

							// Turns out the weapon hand attachment bone is sometimes expected to independently animate.
							// hack: derive the local position offset from cached bones, since otherwise the weapon (a child of the 
							// player) will try and set up the player before itself, then place itself in the wrong spot relative to
							// the player that's in the position we're setting up NOW
							Vector vecRelTarget;
							int nParent = pHdr->pBone(nWepAttach)->parent;
							if ( nParent != -1 )
							{
								matrix3x4_t matAttach;
								pWeaponWorldModel->GetCachedBoneMatrix( nWepAttach, matAttach );
								
								matrix3x4_t matAttachParent;
								pWeaponWorldModel->GetCachedBoneMatrix( nParent, matAttachParent );

								matrix3x4_t matRel = ConcatTransforms( matAttachParent.InverseTR(), matAttach );
								vecRelTarget = matRel.GetOrigin();
							}
							else
							{
								vecRelTarget = pHdr->pBone(nWepAttach)->pos;
							}

							Vector vecLHandAttach = boneToWorld[nRightHandWepBoneIndex].TransformVector( vecRelTarget );
							Vector vecTarget = Lerp( pos[nLeftHandIkBoneDriver].x, boneToWorld[nLeftHandBoneIndex].GetOrigin(), vecLHandAttach );

							// let the ik fail gracefully with an elastic-y pull instead of hyper-extension
							float flDist = vecTarget.DistToSqr( boneToWorld[pLeftArmChain->pLink( 0 )->bone].GetOrigin() );
							if ( flDist > CS_ARM_HYPEREXTENSION_LIM_SQR )
							{
								// HACK: force a valid elbow dir (down z)
								boneToWorld[pLeftArmChain->pLink( 1 )->bone][2][3] -= 0.5f;

								Vector vecShoulderToHand = (vecTarget - boneToWorld[pLeftArmChain->pLink( 0 )->bone].GetOrigin()).Normalized() * CS_ARM_HYPEREXTENSION_LIM;
								vecTarget = vecShoulderToHand + boneToWorld[pLeftArmChain->pLink( 0 )->bone].GetOrigin();							
							}

							//debugoverlay->AddBoxOverlay( vecTarget, Vector(-0.1,-0.1,-0.1), Vector(0.1,0.1,0.1), QAngle(0,0,0), 0,255,0,255, 0 );
							//debugoverlay->AddLineOverlay( boneToWorld[pLeftArmChain->pLink( 0 )->bone].GetOrigin(), boneToWorld[pLeftArmChain->pLink( 1 )->bone].GetOrigin(), 80,80,80,true,0);
							//debugoverlay->AddLineOverlay( boneToWorld[pLeftArmChain->pLink( 1 )->bone].GetOrigin(), boneToWorld[pLeftArmChain->pLink( 2 )->bone].GetOrigin(), 80,80,80,true,0);
							//debugoverlay->AddLineOverlay( boneToWorld[pLeftArmChain->pLink( 0 )->bone].GetOrigin(), boneToWorld[pLeftArmChain->pLink( 2 )->bone].GetOrigin(), 80,80,80,true,0);

							Studio_SolveIK( pLeftArmChain->pLink( 0 )->bone, pLeftArmChain->pLink( 1 )->bone, pLeftArmChain->pLink( 2 )->bone, vecTarget, boneToWorld );

							//debugoverlay->AddLineOverlay( boneToWorld[pLeftArmChain->pLink( 0 )->bone].GetOrigin(), boneToWorld[pLeftArmChain->pLink( 1 )->bone].GetOrigin(), 255,0,0,true,0);
							//debugoverlay->AddLineOverlay( boneToWorld[pLeftArmChain->pLink( 1 )->bone].GetOrigin(), boneToWorld[pLeftArmChain->pLink( 2 )->bone].GetOrigin(), 255,0,0,true,0);
							//debugoverlay->AddLineOverlay( boneToWorld[pLeftArmChain->pLink( 0 )->bone].GetOrigin(), boneToWorld[pLeftArmChain->pLink( 2 )->bone].GetOrigin(), 0,0,255,true,0);
						}
					}
				}
			}
		}
	}

}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void C_CSPlayer::BuildTransformations( CStudioHdr *pHdr, BoneVector *pos, BoneQuaternion q[], const matrix3x4_t& cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	// First, setup our model's transformations like normal.
	BaseClass::BuildTransformations( pHdr, pos, q, cameraTransform, boneMask, boneComputed );

	if ( !m_bUseNewAnimstate || !m_PlayerAnimStateCSGO )
		return;
	
	if ( !IsVisible() || IsDormant() || (IsLocalPlayer( this ) && !C_BasePlayer::ShouldDrawLocalPlayer()) || !ShouldDraw() )
		return;

	if ( boneMask == BONE_USED_BY_ATTACHMENT )
		return; // we're only building transformations to get attachment positions. No need to update bone snapshots now.

	// process bone snapshots
	{
		int oldWritableBones = m_BoneAccessor.GetReadableBones();
		m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );
		int oldReadableBones = m_BoneAccessor.GetReadableBones();
		m_BoneAccessor.SetReadableBones( BONE_USED_BY_ANYTHING );

		m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].Update( this );
		m_boneSnapshots[BONESNAPSHOT_UPPER_BODY].Update( this );

		m_boneSnapshots[BONESNAPSHOT_ENTIRE_BODY].SetLastBoneSetupTimeIndex();
		m_boneSnapshots[BONESNAPSHOT_UPPER_BODY].SetLastBoneSetupTimeIndex();		

		m_BoneAccessor.SetWritableBones( oldWritableBones );
		m_BoneAccessor.SetReadableBones( oldReadableBones );
	}

}


C_BaseAnimating * C_CSPlayer::BecomeRagdollOnClient()
{
	return NULL;
}


IRagdoll* C_CSPlayer::GetRepresentativeRagdoll() const
{
	if ( m_hRagdoll.Get() )
	{
		C_CSRagdoll *pRagdoll = (C_CSRagdoll* )m_hRagdoll.Get();

		return pRagdoll->GetIRagdoll();
	}
	else
	{
		return NULL;
	}
}

// Update the rich presence for this player when they change teams
void C_CSPlayer::TeamChange( int iNewTeam )
{
	C_BasePlayer::TeamChange( iNewTeam );

	// update the spectator glows
	UpdateGlowsForAllPlayers();

	// Apply the new HUD tint for our team
	if ( C_BasePlayer::IsLocalPlayer( this ) && !IsBot() )
	{
		static const int g_CT_Tint	= 1;
		static const int g_T_Tint	= 2;
		ConVarRef sf_ui_tint( "sf_ui_tint" ); 

		if ( iNewTeam == TEAM_TERRORIST )
			sf_ui_tint.SetValue( g_T_Tint );
		else 
			sf_ui_tint.SetValue( g_CT_Tint );

		if ( CSGameRules() && CSGameRules()->IsPlayingGunGameDeathmatch())
			m_bShouldAutobuyDMWeapons = true;
	}

#if defined( _X360 )
	if ( C_BasePlayer::IsLocalPlayer( this ) )
	{
		DWORD dwValue = CONTEXT_CSS_TEAM_SPECTATOR;
		if ( iNewTeam == TEAM_TERRORIST ) 
			dwValue = CONTEXT_CSS_TEAM_T;
		else if ( iNewTeam == TEAM_CT ) 
			dwValue = CONTEXT_CSS_TEAM_CT;

		DevMsg( "Setting rich presence for team to %d\n", dwValue );
		
		XUSER_CONTEXT xUserContext = { CONTEXT_CSS_TEAM, dwValue };
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
		xboxsystem->UserSetContext( XBX_GetActiveUserId(), xUserContext, true );
	}
#endif 

	SplitScreenConVarRef varOption( "cl_clanid" );
	const char *pClanID = varOption.GetString( 0 );
	varOption.SetValue( 0, pClanID );
}

void C_CSPlayer::PlayReloadEffect( int iActAnimID, const Vector *pOrigin )
{
	// Only play the effect for other players.
	if ( this == C_CSPlayer::GetLocalCSPlayer() )
	{
		Assert( g_HltvReplaySystem.GetHltvReplayDelay() ); // We shouldn't have been sent this message - except during replay
		return;
	}
	
	// Get the view model for our current gun.
	CWeaponCSBase *pWeapon = GetActiveCSWeapon();
	if ( !pWeapon )
		return;

	// The weapon needs two models, world and view, but can only cache one. Synthesize the other.
	const model_t *pModel = modelinfo->GetModel( modelinfo->GetModelIndex( pWeapon->GetViewModel() ) );
	if ( !pModel )
		return;
	CStudioHdr studioHdr( modelinfo->GetStudiomodel( pModel ), mdlcache );
	if ( !studioHdr.IsValid() )
		return;

	// Find the reload animation.
	for ( int iSeq=0; iSeq < studioHdr.GetNumSeq(); iSeq++ )
	{
		mstudioseqdesc_t *pSeq = &studioHdr.pSeqdesc( iSeq );
		
		if ( pSeq->activity == iActAnimID )
		{
			float poseParameters[MAXSTUDIOPOSEPARAM];
			memset( poseParameters, 0, sizeof( poseParameters ) );
			float cyclesPerSecond = Studio_CPS( &studioHdr, *pSeq, iSeq, poseParameters );

			// Now read out all the sound events with their timing
			for ( int iEvent=0; iEvent < pSeq->numevents; iEvent++ )
			{
				mstudioevent_t *pEvent = (mstudioevent_for_client_server_t* )pSeq->pEvent( iEvent );
				
				if ( pEvent->Event() == CL_EVENT_SOUND )
				{
					CCSSoundEvent event;
					event.m_SoundName = pEvent->options;
					event.m_flEventTime = gpGlobals->curtime + pEvent->cycle / cyclesPerSecond;

					if ( pOrigin != NULL )
					{
						event.m_SoundOrigin = *pOrigin;
						event.m_bHasSoundOrigin = true;
					}

					m_SoundEvents.AddToTail( event );
				}
			}

			break;
		}
	}	
}

void C_CSPlayer::DoAnimationEvent( PlayerAnimEvent_t event, int nData )
{
	if ( m_bUseNewAnimstate )
	{
		return;
	}

	if ( event == PLAYERANIMEVENT_THROW_GRENADE )
	{
		// Let the server handle this event. It will update m_iThrowGrenadeCounter and the client will
		// pick up the event in CCSPlayerAnimState.
	}
	else
	{
		m_PlayerAnimState->DoAnimationEvent( event, nData );
	}
}

void C_CSPlayer::DropPhysicsMag( const char *options )
{
	// create a client-side physical magazine model to drop in the world and clatter to the floor. Realism!
	
	if ( sv_magazine_drop_physics == 0 )
		return;

	CWeaponCSBase *pWeapon = GetActiveCSWeapon();
	if ( !pWeapon )
		return;

	CEconItemView *pItem = pWeapon->GetEconItemView();
	if ( !pItem || !pItem->GetMagazineModel() )
		return;
	
	Vector attachOrigin = GetAbsOrigin() + Vector(0,0,50);
	QAngle attachAngles = QAngle(0,0,0);

	// find the best attachment position to drop the mag from

	int iMagAttachIndex = -1;
	CBaseWeaponWorldModel *pWeaponWorldModel = pWeapon->m_hWeaponWorldModel.Get();

	if ( options && options[0] != 0 )
	{
		// if a custom attachment is specified, look for it on the weapon, then the player.
		if ( pWeaponWorldModel )
			iMagAttachIndex = pWeaponWorldModel->LookupAttachment( options );
		if ( iMagAttachIndex <= 0 )
			iMagAttachIndex = LookupAttachment( options );
	}

	if ( iMagAttachIndex <= 0 )
	{
		// we either didn't specify a custom attachment, or the one we did wasn't found. Find the default, 'mag_eject' on the weapon, then the player.
		if ( pWeaponWorldModel )
			iMagAttachIndex = pWeaponWorldModel->LookupAttachment( "mag_eject" );
		if ( iMagAttachIndex <= 0 )
			iMagAttachIndex = LookupAttachment( "mag_eject" );
	}

	if ( iMagAttachIndex <= 0 && pWeaponWorldModel )
	{
		// no luck looking for the custom attachment, or "mag_eject". How about "shell_eject"? Wrong, but better than nothing...
		iMagAttachIndex = pWeaponWorldModel->LookupAttachment( "shell_eject" );
	}
	

	// limit mag drops to one per second, in case animations accidentally overlap or events erroneously get fired too rapidly
	// let new attachment indices through though, for elites
	if ( m_flNextMagDropTime > gpGlobals->curtime && iMagAttachIndex == m_nLastMagDropAttachmentIndex )
		return;
	m_flNextMagDropTime = gpGlobals->curtime + 1;
	m_nLastMagDropAttachmentIndex = iMagAttachIndex;

	if ( iMagAttachIndex <= 0 )
	{
		return;
	}

	if ( !IsDormant() )
	{
		if ( pWeaponWorldModel )
		{
			pWeaponWorldModel->GetAttachment( iMagAttachIndex, attachOrigin, attachAngles );
		}
		else
		{
			GetAttachment( iMagAttachIndex, attachOrigin, attachAngles );
		}
	}

	//// hide the animation-driven w_model magazine
	//if ( pWeaponWorldModel )
	//{
	//	pWeaponWorldModel->SetBodygroupPreset( "hide_mag" );
	//}

	// The local first-person player can't drop mags in the correct world-space location, otherwise the mag would appear in mid-air.
	// Instead, first try to drop the mag slightly above the origin of the player.
	// However if the player is looking nearly straight down, they'll still see the mag appear. If this is the case,
	// drop the mags from 10 units behind their eyes. This means the mag ALWAYS drops in from "off-screen"
	if ( ( IsLocalPlayer() && !ShouldDraw() ) || GetSpectatorMode() == OBS_MODE_IN_EYE )
	{
		if ( EyeAngles().x < 42.0f ) //not looking extremely vertically downward
		{
			attachOrigin = GetAbsOrigin() + Vector(0,0,20);
		}
		else
		{
			attachOrigin = EyePosition() - ( Forward() * 10 );
		}
	}

	C_PhysPropClientside *pEntity = C_PhysPropClientside::CreateNew();
	if ( !pEntity )
		return;

	pEntity->SetModelName( pItem->GetMagazineModel() );
	pEntity->SetPhysicsMode( PHYSICS_MULTIPLAYER_CLIENTSIDE );
	pEntity->SetCollisionGroup( COLLISION_GROUP_DEBRIS );
	pEntity->SetDistanceFade( 500.0f, 550.0f );
	pEntity->SetLocalOrigin( attachOrigin );
	pEntity->SetLocalAngles( attachAngles );
	pEntity->m_iHealth = 0;
	pEntity->m_takedamage = DAMAGE_NO;

	if ( !pEntity->Initialize() )
	{
		pEntity->Release();
		return;
	}

	// apply custom material
	pEntity->ClearCustomMaterials();
	for ( int i = 0; i < pWeapon->GetCustomMaterialCount(); i++ )
		pEntity->SetCustomMaterial( pWeapon->GetCustomMaterial( i ), i );

	// fade out after set time
	pEntity->StartFadeOut( sv_magazine_drop_time );

	// apply starting velocity
	IPhysicsObject *pPhysicsObject = pEntity->VPhysicsGetObject();
	if( pPhysicsObject )
	{
		if ( !IsDormant() ) // don't apply velocity to mags dropped by dormant players
		{
			Vector vecMagVelocity; vecMagVelocity.Init();
			Quaternion quatMagAngular; quatMagAngular.Init();
			if ( pWeaponWorldModel )
			{
				pWeaponWorldModel->GetAttachmentVelocity( iMagAttachIndex, vecMagVelocity, quatMagAngular );
			}
			else
			{
				GetAttachmentVelocity( iMagAttachIndex, vecMagVelocity, quatMagAngular );
			}

			// if the local attachment is returning no motion, pull velocity from the player just to be sure
			if ( vecMagVelocity == vec3_origin )
				vecMagVelocity = GetLocalVelocity();

			if ( sv_magazine_drop_debug && debugoverlay )
			{
				debugoverlay->AddBoxOverlay( attachOrigin, Vector(-1,-1,-1), Vector(1,1,1), QAngle(0,0,0), 200,0,0, 255, 10.0f );
				debugoverlay->AddLineOverlayAlpha( attachOrigin, attachOrigin+vecMagVelocity, 0,200,0, 255, true, 10.0f );
			}

			QAngle angMagAngular; angMagAngular.Init();
			QuaternionAngles( quatMagAngular, angMagAngular );

			AngularImpulse angImpMagAngular; angImpMagAngular.Init();
			QAngleToAngularImpulse( angMagAngular, angImpMagAngular );

			// clamp to 300 max
			if ( vecMagVelocity.Length() > 300.0f )
				vecMagVelocity = vecMagVelocity.Normalized() * 300.0f;

			pPhysicsObject->SetVelocity( &vecMagVelocity, &angImpMagAngular );
		}
	}
	else
	{
		pEntity->Release();
		return;
	}
}

void C_CSPlayer::FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	if ( event == AE_WPN_HIDE )
	{
		CBaseCombatWeapon *pWeapon = GetActiveWeapon();
		if (pWeapon && options && options[0] != 0)
			pWeapon->m_flWeaponTauntHideTimeout = gpGlobals->curtime + atof(options);
		return;
	}
	else if ( event == AE_CL_EJECT_MAG_UNHIDE )
	{
		// mag is unhidden by the server
		return;
	}
	else if ( event == AE_CL_EJECT_MAG )
	{
		CAnimationLayer *pWeaponLayer = GetAnimOverlay( ANIMATION_LAYER_WEAPON_ACTION );
		if ( pWeaponLayer && pWeaponLayer->m_nDispatchedDst != ACT_INVALID )
		{
			// let the weapon drop the mag
		}
		else
		{
			DropPhysicsMag( options );

			// hack: in first-person, the player isn't playing their third-person gun animations, so the events on the third-person gun model don't fire.
			// This means the event is fired from the player, and the player only fires ONE mag drop event. So while the elite model drops two data-driven mags
			// in third person, we need to help it out a little bit in first-person. So here's some one-off code to drop another physics mag only for the elite,
			// and only in first-person.

			CWeaponCSBase *pWeapon = GetActiveCSWeapon();
			if ( pWeapon && pWeapon->IsA(WEAPON_ELITE) )
			{
				DropPhysicsMag( "mag_eject2" );
			}

		}
		return;
	}

	if (event != 7001 && event != 7002 )
	{
		BaseClass::FireEvent(origin, angles, event, options);
		return;
	}

	bool kneesInWater, feetInWater;
	switch( GetWaterLevel() )
	{
	case WL_NotInWater:	default:	feetInWater = kneesInWater = false; break;
	case WL_Feet:					feetInWater = true; kneesInWater = false; break;
	case WL_Waist: case WL_Eyes:	feetInWater = kneesInWater = true; break;			
	}

	// If we're not on the ground, don't draw splashes for footsteps
	if ( !(GetFlags() & FL_ONGROUND) )	
	{
		kneesInWater = feetInWater = 0;
	}

	if ( !feetInWater && !kneesInWater )
		return;

	if ( event == 7001 )
	{

		//Msg( "run event ( %d )\n", bInWater ? 1 : 0 );

		if( feetInWater )
		{
			//run splash
			CEffectData data;

			//trace up from foot position to the water surface
			trace_t tr;
			Vector vecTrace(0,0,1024 );
			UTIL_TraceLine( origin, origin + vecTrace, MASK_WATER, NULL, COLLISION_GROUP_NONE, &tr );
			if ( tr.fractionleftsolid )
			{
				data.m_vOrigin = origin + (vecTrace * tr.fractionleftsolid );
			}
			else
			{
				data.m_vOrigin = origin;
			}
			
			data.m_vNormal = Vector( 0,0,1 );
			data.m_flScale = random->RandomFloat( 4.0f, 5.0f );
			DispatchEffect( "watersplash", data );
		}		
	}
	else if( event == 7002 )
	{

		//Msg( "walk event ( %d )\n", bInWater ? 1 : 0 );
		
		if( feetInWater )
		{
			//walk ripple
			CEffectData data;

			//trace up from foot position to the water surface
			trace_t tr;
			Vector vecTrace(0,0,1024 );
			UTIL_TraceLine( origin, origin + vecTrace, MASK_WATER, NULL, COLLISION_GROUP_NONE, &tr );
			if ( tr.fractionleftsolid )
			{
				data.m_vOrigin = origin + (vecTrace * tr.fractionleftsolid );
			}
			else
			{
				data.m_vOrigin = origin;
			}
	
			data.m_vNormal = Vector( 0,0,1 );
			data.m_flScale = random->RandomFloat( 4.0f, 7.0f );
			DispatchEffect( "waterripple", data );
		}
	}
}


void C_CSPlayer::SetActivity( Activity eActivity )
{
	m_Activity = eActivity;
}


Activity C_CSPlayer::GetActivity() const
{
	return m_Activity;
}


const Vector& C_CSPlayer::GetRenderOrigin( void )
{
	if ( m_hRagdoll.Get() )
	{
		C_CSRagdoll *pRagdoll = (C_CSRagdoll* )m_hRagdoll.Get();
		if ( pRagdoll->IsInitialized() )
			return pRagdoll->GetRenderOrigin();
	}

	return BaseClass::GetRenderOrigin();
}


bool C_CSPlayer::Simulate( void )
{
	if ( !C_BasePlayer::IsLocalPlayer( this ) )
	{
		if ( IsEffectActive( EF_DIMLIGHT ) )
		{
			QAngle eyeAngles = EyeAngles();
			Vector vForward;
			AngleVectors( eyeAngles, &vForward );

			int iAttachment = LookupAttachment( "muzzle_flash" );

			if ( iAttachment < 0 )
				return false;

			Vector vecOrigin;
			QAngle dummy;
			GetAttachment( iAttachment, vecOrigin, dummy );
				
			trace_t tr;
			UTIL_TraceLine( vecOrigin, vecOrigin + (vForward * 200 ), MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

			if( !m_pFlashlightBeam )
			{
				BeamInfo_t beamInfo;
				beamInfo.m_nType = TE_BEAMPOINTS;
				beamInfo.m_vecStart = tr.startpos;
				beamInfo.m_vecEnd = tr.endpos;
				beamInfo.m_pszModelName = "sprites/glow01.vmt";
				beamInfo.m_pszHaloName = "sprites/glow01.vmt";
				beamInfo.m_flHaloScale = 3.0;
				beamInfo.m_flWidth = 8.0f;
				beamInfo.m_flEndWidth = 35.0f;
				beamInfo.m_flFadeLength = 300.0f;
				beamInfo.m_flAmplitude = 0;
				beamInfo.m_flBrightness = 60.0;
				beamInfo.m_flSpeed = 0.0f;
				beamInfo.m_nStartFrame = 0.0;
				beamInfo.m_flFrameRate = 0.0;
				beamInfo.m_flRed = 255.0;
				beamInfo.m_flGreen = 255.0;
				beamInfo.m_flBlue = 255.0;
				beamInfo.m_nSegments = 8;
				beamInfo.m_bRenderable = true;
				beamInfo.m_flLife = 0.5;
				beamInfo.m_nFlags = FBEAM_FOREVER | FBEAM_ONLYNOISEONCE | FBEAM_NOTILE | FBEAM_HALOBEAM;
				
				m_pFlashlightBeam = beams->CreateBeamPoints( beamInfo );
			}

			if( m_pFlashlightBeam )
			{
				BeamInfo_t beamInfo;
				beamInfo.m_vecStart = tr.startpos;
				beamInfo.m_vecEnd = tr.endpos;
				beamInfo.m_flRed = 255.0;
				beamInfo.m_flGreen = 255.0;
				beamInfo.m_flBlue = 255.0;

				beams->UpdateBeamInfo( m_pFlashlightBeam, beamInfo );

				dlight_t *el = effects->CL_AllocDlight( 0 );
				el->origin = tr.endpos;
				el->radius = 50; 
				el->color.r = 200;
				el->color.g = 200;
				el->color.b = 200;
				el->die = gpGlobals->curtime + 0.1;
			}
		}
		else if ( m_pFlashlightBeam )
		{
			ReleaseFlashlight();
		}
	}

	BaseClass::Simulate();
	return true;
}

void C_CSPlayer::ReleaseFlashlight( void )
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	if( m_pFlashlightBeam )
	{
		m_pFlashlightBeam->flags = 0;
		m_pFlashlightBeam->die = gpGlobals->curtime - 1;

		m_pFlashlightBeam = NULL;
	}
}

bool C_CSPlayer::HasC4( void )
{
	if( C_BasePlayer::IsLocalPlayer( this ) )
	{
		return Weapon_OwnsThisType( "weapon_c4" ) != NULL;
	}
	else
	{
		C_CS_PlayerResource *pCSPR = (C_CS_PlayerResource* )GameResources();

		return pCSPR->HasC4( entindex() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float C_CSPlayer::GetFOV( void ) const
{
	float flCurFOV = BaseClass::GetFOV();

	if ( flCurFOV == GetDefaultFOV() )
	{
		if ( !sv_cheats )
		{
			sv_cheats = cvar->FindVar( "sv_cheats" );
		}

		if ( sv_cheats->GetBool() && fov_cs_debug.GetInt() > 0 )
		{
			return fov_cs_debug.GetInt();
		}
	}

#ifdef IRONSIGHT
	CWeaponCSBase *pWeapon = GetActiveCSWeapon();
	if ( pWeapon )
	{
		CIronSightController* pIronSightController = pWeapon->GetIronSightController();
		if ( pIronSightController )
		{
			//bias the local client FOV change so ironsight transitions are nicer
			flCurFOV = pIronSightController->GetIronSightFOV( GetDefaultFOV(), true );
		}
	}
#endif //IRONSIGHT

	if ( GetObserverInterpState() == OBSERVER_INTERP_TRAVELING )
		flCurFOV = GetDefaultFOV();

	return flCurFOV;
}

void C_CSPlayer::SetSpecWatchingGrenade( C_BaseEntity *pGrenade, bool bWatching )
{
	if ( bWatching )
	{
		if ( pGrenade && IsSpecFollowingGrenade() == false )
		{
			m_hOldGrenadeObserverTarget = GetObserverTarget();
			m_bIsSpecFollowingGrenade = true;
			SetObserverTarget( pGrenade );
		}
	}
	else
	{
		// only change state if we get a false
//		if ( pGrenade == GetObserverTarget() )
		{
			if ( IsSpecFollowingGrenade() == true )
			{
				m_bIsSpecFollowingGrenade = false;

				C_BaseEntity *pLastTarget = m_hOldGrenadeObserverTarget.Get();
				if ( !pLastTarget || pLastTarget->IsDormant() || !pLastTarget->IsAlive() )
				{
					// find a player that is alive to go to
					for ( int i = 1; i <= MAX_PLAYERS; i++ )
					{
						CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
						if ( pPlayer && pPlayer->IsAlive() && ( pPlayer->GetTeamNumber() == TEAM_TERRORIST || pPlayer->GetTeamNumber() == TEAM_CT ) )
						{
							pLastTarget = pPlayer;
							break;
						}
					}
				}

				if ( pLastTarget )
				{
					SetObserverTarget( pLastTarget );
					m_hOldGrenadeObserverTarget = NULL;
				}
			}
		}
	}
}

CBaseEntity	*C_CSPlayer::GetObserverTarget() const	// returns players targer or NULL
{
	return BaseClass::GetObserverTarget();
}

int C_CSPlayer::GetObserverMode() const
{
	if ( IsHLTV() )
		return BaseClass::GetObserverMode();

	if ( dynamic_cast< C_BaseCSGrenadeProjectile* >( GetObserverTarget() ) )
	{
		//m_bIsSpecFollowingGrenade = true;
		return OBS_MODE_CHASE;
	}

	return BaseClass::GetObserverMode();
}

//-----------------------------------------------------------------------------
void C_CSPlayer::CalcObserverView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	CancelFreezeCamFlashlightEffect();

	/**
	 * TODO: Fix this!
	// CS:S standing eyeheight is above the collision volume, so we need to pull it
	// down when we go into close quarters.
	float maxEyeHeightAboveBounds = VEC_VIEW.z - VEC_HULL_MAX.z;
	if ( GetObserverMode() == OBS_MODE_IN_EYE &&
		maxEyeHeightAboveBounds > 0.0f &&
		GetObserverTarget() &&
		GetObserverTarget()->IsPlayer() )
	{
		const float eyeClearance = 12.0f; // eye pos must be this far below the ceiling

		C_CSPlayer *target = ToCSPlayer( GetObserverTarget() );

		Vector offset = eyeOrigin - GetAbsOrigin();

		Vector vHullMin = VEC_HULL_MIN;
		vHullMin.z = 0.0f;
		Vector vHullMax = VEC_HULL_MAX;

		Vector start = GetAbsOrigin();
		start.z += vHullMax.z;
		Vector end = start;
		end.z += eyeClearance + VEC_VIEW.z - vHullMax.z;

		vHullMax.z = 0.0f;

		Vector fudge( 1, 1, 0 );
		vHullMin += fudge;
		vHullMax -= fudge;

		trace_t trace;
		Ray_t ray;
		ray.Init( start, end, vHullMin, vHullMax );
		UTIL_TraceRay( ray, MASK_PLAYERSOLID, target, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

		if ( trace.fraction < 1.0f )
		{
			float est = start.z + trace.fraction * (end.z - start.z ) - GetAbsOrigin().z - eyeClearance;
			if ( ( target->GetFlags() & FL_DUCKING ) == 0 && !target->GetFallVelocity() && !target->IsDucked() )
			{
				offset.z = est;
			}
			else
			{
				offset.z = MIN( est, offset.z );
			}
			eyeOrigin.z = GetAbsOrigin().z + offset.z;
		}
	}
	*/
	if ( GetObserverTarget() )
	{
		CBaseCSGrenadeProjectile *pGrenade = dynamic_cast< CBaseCSGrenadeProjectile* >( GetObserverTarget() );
		if ( pGrenade )
		{
			m_bIsSpecFollowingGrenade = true;
		}
	}


	BaseClass::CalcObserverView( eyeOrigin, eyeAngles, fov );
}

ConVar cl_obs_interp_enable( "cl_obs_interp_enable", "1", FCVAR_ARCHIVE, "Enables interpolation between observer targets" );
ConVar cl_obs_interp_pos_rate( "cl_obs_interp_pos_rate", "0.27" );
ConVar cl_obs_interp_pos_halflife( "cl_obs_interp_pos_halflife", "0.26" );
ConVar cl_obs_interp_snap_dist( "cl_obs_interp_snap_dist", "1" );
ConVar cl_obs_interp_settle_dist( "cl_obs_interp_settle_dist", "16" );
ConVar cl_obs_interp_dist_to_turn_to_face( "cl_obs_interp_dist_to_turn_to_face", "500", 0, "Changing to a target further than this will cause the camera to face the direction of travel" );
ConVar cl_obs_interp_angle_progress_headstart( "cl_obs_interp_angle_progress_headstart", "0.025" );
ConVar cl_obs_interp_turn_to_face_start_frac( "cl_obs_interp_turn_to_face_start_frac", "0.1" );
ConVar cl_obs_interp_turn_to_face_end_frac( "cl_obs_interp_turn_to_face_end_frac", "0.65" );
ConVar cl_obs_interp_obstruction_behavior( "cl_obs_interp_obstruction_behavior", "2" );
extern ConVar sv_disable_observer_interpolation;

void C_CSPlayer::InterpolateObserverView( Vector& vOutOrigin, QAngle& vOutAngles )
{
	Assert( vOutAngles.IsValid() && vOutOrigin.IsValid() );
	// Interpolate the view between observer target changes
	if ( m_obsInterpState != OBSERVER_INTERP_NONE && ShouldInterpolateObserverChanges() )
	{
		// We flag observer interpolation on as soon as observer targets change in the recvproxy change callback,
		// but we can't get entity position that early in the frame, so some setup is deferred until this point. We
		// have to set observer interpolation earlier, before the view is set up for this frame, otherwise we'll get a flicker
		// as the first frame after an observer target change will draw at the final position.
		if ( m_bObserverInterpolationNeedsDeferredSetup )
		{
			
			// Initial setup
			m_vecObserverInterpolateOffset = vOutOrigin - m_vecObserverInterpStartPos;
			m_flObsInterp_PathLength = m_vecObserverInterpolateOffset.Length();
			Vector vRight = m_vecObserverInterpolateOffset.Cross( Vector( 0, 0, 1 ) );
			Vector vUp = vRight.Cross( m_vecObserverInterpolateOffset );
			BasisToQuaternion( m_vecObserverInterpolateOffset.Normalized(), vRight.Normalized(), vUp.Normalized(), m_qObsInterp_OrientationTravelDir );
			m_bObserverInterpolationNeedsDeferredSetup = false;
		}

		float flPosProgress = ExponentialDecay( cl_obs_interp_pos_halflife.GetFloat(), cl_obs_interp_pos_rate.GetFloat(), gpGlobals->frametime );

		// Decay the offset vector until we reach the new observer position
		m_vecObserverInterpolateOffset *= flPosProgress;
		vOutOrigin -= m_vecObserverInterpolateOffset;

		// Angle interpolation is a function of position progress so they stay in sync (adding a slight head start for aesthetic reasons)
		float flPathProgress, flObserverInterpolateOffset = m_vecObserverInterpolateOffset.Length();
		if ( m_flObsInterp_PathLength <= 0.0001f * flObserverInterpolateOffset )
		{
			flPathProgress = 1.0f;
		}
		else
		{
			flPathProgress = ( flObserverInterpolateOffset / m_flObsInterp_PathLength ) - cl_obs_interp_angle_progress_headstart.GetFloat();
			flPathProgress = 1.0f - Clamp( flPathProgress, 0.0f, 1.0f );
		}

		// Messy and in flux... still tuning the interpolation code below
		Quaternion q1, q2;
		Quaternion qFinal;
		AngleQuaternion( vOutAngles, qFinal );
		float t = 0;
		if ( m_flObsInterp_PathLength > cl_obs_interp_dist_to_turn_to_face.GetFloat() && GetObserverMode() == OBS_MODE_IN_EYE )
		{
			// at a far enough distance, turn to face direction of motion before ending at final angles
			//QuaternionSlerp( m_qObsInterp_OrientationStart, m_qObsInterp_OrientationTravelDir, flPathProgress, q1 );
			//QuaternionSlerp( m_qObsInterp_OrientationTravelDir, qFinal, flPathProgress, q2 );

			if ( flPathProgress < cl_obs_interp_turn_to_face_start_frac.GetFloat() )
			{
				//QuaternionSlerp( m_qObsInterp_OrientationStart, m_qObsInterp_OrientationTravelDir, flPathProgress, q1 );
				q1 = m_qObsInterp_OrientationStart;
				q2 = m_qObsInterp_OrientationTravelDir;
				t = RemapVal( flPathProgress, 0.0, cl_obs_interp_turn_to_face_start_frac.GetFloat(), 0.0, 1.0 );
			}
			else if ( flPathProgress < cl_obs_interp_turn_to_face_end_frac.GetFloat() )
			{
				q1 = q2 = m_qObsInterp_OrientationTravelDir;
			}
			else
			{
				q1 = m_qObsInterp_OrientationTravelDir;
				q2 = qFinal;
				t = SimpleSplineRemapValClamped( flPathProgress, cl_obs_interp_turn_to_face_end_frac.GetFloat(), 1.0, 0.0, 1.0 );
			}
		}
		else
		{
			// Otherwise, interpolate from start to end orientation
			q1 = m_qObsInterp_OrientationStart;
			q2 = qFinal;
			t = flPathProgress;
		}

		Quaternion qOut;
		//QuaternionSlerp( q1, q2, flPathProgress, qOut );
		QuaternionSlerp( q1, q2, t, qOut );
		QuaternionAngles( qOut, vOutAngles );

		Assert( cl_obs_interp_snap_dist.GetFloat() < cl_obs_interp_settle_dist.GetFloat() );
		// At a close enough dist snap to final and stop interpolating
		if ( m_vecObserverInterpolateOffset.LengthSqr() < cl_obs_interp_snap_dist.GetFloat()*cl_obs_interp_snap_dist.GetFloat() )
		{
			m_obsInterpState = OBSERVER_INTERP_NONE;
			UpdateObserverTargetVisibility();
		}
		else if ( m_vecObserverInterpolateOffset.LengthSqr() < cl_obs_interp_settle_dist.GetFloat() * cl_obs_interp_settle_dist.GetFloat() )
		{
			m_obsInterpState = OBSERVER_INTERP_SETTLING;
			UpdateObserverTargetVisibility();
		}
	}
	Assert( vOutAngles.IsValid() && vOutOrigin.IsValid() );
}


void C_CSPlayer::UpdateObserverTargetVisibility( void ) const
{
	C_CSPlayer *pObservedPlayer = dynamic_cast < C_CSPlayer* >( GetObserverTarget() );
	if ( pObservedPlayer )
	{
		extern void UpdateViewmodelVisibility( C_BasePlayer *player );
		UpdateViewmodelVisibility( pObservedPlayer );
		pObservedPlayer->UpdateVisibility();

		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pLocalPlayer )
			pLocalPlayer->UpdateGlowsForAllPlayers();
	}
}


bool C_CSPlayer::ShouldInterpolateObserverChanges() const
{
	if ( !cl_obs_interp_enable.GetBool() )
		return false;

	// server doesn't want this
	if ( sv_disable_observer_interpolation.GetBool() )
		return false;

	// Disallow when playing on a team in a competitive match
	bool bIsPlayingOnCompetitiveTeam = CSGameRules() && CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset() && ( GetAssociatedTeamNumber() == TEAM_CT || GetAssociatedTeamNumber() == TEAM_TERRORIST );
	if ( bIsPlayingOnCompetitiveTeam )
		return false;

	// supported modes
	if ( GetObserverMode() != OBS_MODE_IN_EYE && GetObserverMode() != OBS_MODE_CHASE && GetObserverMode() != OBS_MODE_ROAMING )
		return false;

	// In hltv, our camera man might be the player we're using for view. Only bother with the dynamic cast if needed.
	CBasePlayer *pCameraMan = NULL;
	if ( engine->IsHLTV() )
		pCameraMan = HLTVCamera()->GetCameraMan();

	// If we are in hltv and have a camera man, only run for that player. Otherwise, only run for the local player.
	CBasePlayer *pInterpPlayer = ( engine->IsHLTV() && pCameraMan ) ? pCameraMan : C_CSPlayer::GetLocalCSPlayer();

	if ( pInterpPlayer != this )
		return false;

	return true;
}

// Set up initial state for interpolating between observer positions
void C_CSPlayer::StartObserverInterpolation( const QAngle& startAngles )
{
	// Find obstructions in the path, skip past them if needed
	m_vecObserverInterpStartPos = MainViewOrigin( GetSplitScreenPlayerSlot() );
	C_CSPlayer* pObserverTarget = dynamic_cast< C_CSPlayer* > ( GetObserverTarget() );
	if ( cl_obs_interp_obstruction_behavior.GetInt() > 0 && pObserverTarget )
	{
		trace_t tr;
		Ray_t ray;
		// HACK: This is wrong for chase and roam modes, but this is too early in the frame to call
		// CalcObserverView functions without trigger the absqueries valid assert... Revisit if needed,
		// but this will probably be a good enough test to see if we would lerp through solid
		ray.Init( pObserverTarget->GetNetworkOrigin() + VEC_VIEW, m_vecObserverInterpStartPos );
		CTraceFilterWorldAndPropsOnly filter;
		enginetrace->TraceRay( ray, MASK_VISIBLE, &filter, &tr );

		if ( tr.DidHit() )
		{
			if ( cl_obs_interp_obstruction_behavior.GetInt() == 1 )
			{
				m_vecObserverInterpStartPos = tr.endpos;
			}
			else if ( cl_obs_interp_obstruction_behavior.GetInt() == 2 )
			{
				m_obsInterpState = OBSERVER_INTERP_NONE;
				UpdateObserverTargetVisibility();
				return;
			}
		}
	}

	AngleQuaternion( startAngles, m_qObsInterp_OrientationStart );
	m_obsInterpState = OBSERVER_INTERP_TRAVELING;
	m_bObserverInterpolationNeedsDeferredSetup = true;
	UpdateObserverTargetVisibility();
}


C_CSPlayer::eObserverInterpState C_CSPlayer::GetObserverInterpState( void ) const
{
	if ( !ShouldInterpolateObserverChanges() )
		return OBSERVER_INTERP_NONE;

	return m_obsInterpState;
}

void C_CSPlayer::SetObserverTarget( EHANDLE hTarget )
{
	EHANDLE prevTarget = m_hObserverTarget;
	BaseClass::SetObserverTarget( hTarget );
	if ( hTarget && prevTarget != hTarget && ShouldInterpolateObserverChanges() )
	{
		QAngle eyeAngles;
		if ( g_HltvReplaySystem.GetHltvReplayDelay() ) 
		{
			eyeAngles = view->GetViewSetup()->angles;
		}
		else
		{
			eyeAngles = EyeAngles();
		}
		StartObserverInterpolation( eyeAngles );
	}
}

// [tj] checks if this player has another given player on their Steam friends list.
bool C_CSPlayer::HasPlayerAsFriend( C_CSPlayer* player )
{
	if ( !steamapicontext || !steamapicontext->SteamFriends() || !steamapicontext->SteamUtils() || !player )
	{
		return false;
	}

	player_info_t pi;
	if ( !engine->GetPlayerInfo( player->entindex(), &pi ) )
	{
		return false;
	}

	if ( !pi.friendsID )
	{
		return false;
	}

	// check and see if they're on the local player's friends list
	CSteamID steamID( pi.friendsID, 1, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
	return steamapicontext->SteamFriends()->HasFriend( steamID, k_EFriendFlagImmediate );
}

// [menglish] Returns whether this player is dominating or is being dominated by the specified player
bool C_CSPlayer::IsPlayerDominated( int iPlayerIndex )
{
	if ( CSGameRules()->IsPlayingGunGame() )
		return m_bPlayerDominated.Get( iPlayerIndex );

	return false;
}

bool C_CSPlayer::IsPlayerDominatingMe( int iPlayerIndex )
{
	if ( CSGameRules()->IsPlayingGunGame() )
		return m_bPlayerDominatingMe.Get( iPlayerIndex );

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: helper interpolation functions
//-----------------------------------------------------------------------------
namespace Interpolators
{
	inline float Linear( float t ) { return t; }

	inline float SmoothStep( float t )
	{
		t = 3 * t * t - 2.0f * t * t * t;
		return t;
	}

	inline float SmoothStep2( float t )
	{
		return t * t * t * (t * (t * 6.0f - 15.0f ) + 10.0f);
	}

	inline float SmoothStepStart( float t )
	{
		t = 0.5f * t;
		t = 3 * t * t - 2.0f * t * t * t;
		t = t* 2.0f;
		return t;
	}

	inline float SmoothStepEnd( float t )
	{
		t = 0.5f * t + 0.5f;
		t = 3 * t * t - 2.0f * t * t * t;
		t = (t - 0.5f ) * 2.0f;
		return t;
	}
}

float C_CSPlayer::GetFreezeFrameInterpolant( void )
{

	float fCurTime = gpGlobals->curtime - m_flFreezeFrameStartTime;
	float fTravelTime = !m_bFreezeFrameCloseOnKiller ? spec_freeze_traveltime.GetFloat() : spec_freeze_traveltime_long.GetFloat();
	float fInterpolant = clamp( fCurTime / fTravelTime, 0.0f, 1.0f );
	
	return Interpolators::SmoothStepEnd( fInterpolant );
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the view for the player while he's in freeze frame observer mode
//-----------------------------------------------------------------------------
void C_CSPlayer::CalcFreezeCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	//SetFOV( CBaseEntity *pRequester, int FOV, float zoomRate = 0.0f, int iZoomStart = 0 );
	C_BaseEntity *pTarget = GetObserverTarget();

	Vector prevEyeOrigin = eyeOrigin;
	QAngle prevEyeAngles = eyeAngles;

	// if we auto-start replay, we don't want to layer camera motion on top of fade on top of replay scene cut
	float fInterpolant = g_HltvReplaySystem.IsDelayedReplayRequestPending() ? 0.0f : GetFreezeFrameInterpolant();

	static ConVarRef sv_disablefreezecam( "sv_disablefreezecam" );
	if ( m_bAbortedFreezeFrame || !pTarget || pTarget == this || cl_disablefreezecam.GetBool() || sv_disablefreezecam.GetBool() || g_HltvReplaySystem.IsDelayedReplayRequestPending() )
	{
		if ( !pTarget )
		{
			if ( !m_bStartedFreezeFrame )
			{
				// randomly pick left or right
				int nLeftOrRight = RandomInt( 0, 1 );
				// sup dutch
				m_flFreezeFrameTilt = ( nLeftOrRight > 0 ) ? RandomFloat( 4, 10 ) : RandomFloat( -4, -10 );
				m_bStartedFreezeFrame = true;
			}		
			eyeAngles.z = Lerp( fInterpolant, 0.0f, m_flFreezeFrameTilt );
		}

		return CalcDeathCamView( eyeOrigin, eyeAngles, fov );
	}

	Vector targetOrig = pTarget->GetRenderOrigin();
	float flDistToTarg = (GetAbsOrigin() - targetOrig ).Length2D();

	Vector vLookAt = pTarget->GetObserverCamOrigin();	// Returns ragdoll origin if they're ragdolled
	vLookAt += GetChaseCamViewOffset( pTarget );
	Vector vecCamTarget = vLookAt;
	if ( pTarget->IsAlive() )
	{
		// Look at their chest, not their head
		Vector maxs = GameRules()->GetViewVectors()->m_vHullMax;
		vecCamTarget.z -= (maxs.z * 0.25 );
	}
	else
	{
		vecCamTarget.z += VEC_DEAD_VIEWHEIGHT.z;	// look over ragdoll, not through
	}

	float flScaler = pTarget->IsAlive() ? 0.1 : 0.075;
	float flDistFromCurToTarg2D = vLookAt.AsVector2D().DistTo( eyeOrigin.AsVector2D() );
	vecCamTarget.z -= clamp( (flDistFromCurToTarg2D )*flScaler, 0, 34 );

	// Figure out a view position in front of the target
	Vector vecEyeOnPlane = eyeOrigin;
	vecEyeOnPlane.z = vecCamTarget.z;
	Vector vTargetPos = vecCamTarget;
	Vector vToTarget = vTargetPos - vecEyeOnPlane;
	VectorNormalize( vToTarget );

	// Stop a few units away from the target, and shift up to be at the same height
	vTargetPos = vecCamTarget - (vToTarget * m_flFreezeFrameDistance );

	float flEyePosZ = pTarget->EyePosition().z;
	vTargetPos.z = flEyePosZ + m_flFreezeZOffset;

	if ( vToTarget == Vector( 0, 0, 0 ) )
	{
		// Abort!
		m_bAbortedFreezeFrame = true;
		return;
	}

	// Now trace out from the target, so that we're put in front of any walls
	trace_t trace;
	Vector vecHMinWall(-16,-16,-16 );
	Vector vecHMaxWall(16,16,16 );
	C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceHull( vecCamTarget, vTargetPos, vecHMinWall, vecHMaxWall, MASK_SHOT, pTarget, COLLISION_GROUP_NONE, &trace );
	C_BaseEntity::PopEnableAbsRecomputations();
	if ( trace.fraction < 1.0 )
	{
		// The camera's going to be really close to the target. So we don't end up
		// looking at someone's chest, aim close freezecams at the target's eyes.
		vTargetPos = trace.endpos;
		vecCamTarget = vLookAt;

		// To stop all close in views looking up at character's chins, move the view up.
		vTargetPos.z += fabs(vecCamTarget.z - vTargetPos.z ) * 0.85;
		C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
		UTIL_TraceHull( vecCamTarget, vTargetPos, WALL_MIN, WALL_MAX, MASK_SHOT, pTarget, COLLISION_GROUP_NONE, &trace );
		C_BaseEntity::PopEnableAbsRecomputations();
		vTargetPos = trace.endpos;
	}

	// move the eye toward our killer
	if ( !m_bStartedFreezeFrame )
	{
		m_vecFreezeFrameStart = eyeOrigin;

		m_bFreezeFrameCloseOnKiller = false;
		m_vecFreezeFrameAnglesStart = eyeAngles;

		// randomly pick left or right
		int nLeftOrRight = RandomInt( 0, 1 );
		m_nFreezeFrameShiftSideDist = nLeftOrRight == 1 ? RandomInt( 12, 22 ) : RandomInt( -12, -22 );

		// sup dutch
		m_flFreezeFrameTilt = ( m_nFreezeFrameShiftSideDist > 0 ) ? RandomFloat( 6, 14 ) : RandomFloat( -6, -14 );

		if ( flDistToTarg >= FREEZECAM_LONGCAM_DIST )
		{
			m_bFreezeFrameCloseOnKiller = true;
		}
		else
		{
			// do a one time hull trace to the target and see if it's wide enough
			C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
			Vector vecHMin(-16,-16,-16 );
			Vector vecHMax(16,16,16 );
			trace_t trace3;
			UTIL_TraceHull( m_vecFreezeFrameStart, vTargetPos, vecHMin, vecHMax, MASK_SHOT, this, COLLISION_GROUP_DEBRIS, &trace3 );
			C_BaseEntity::PopEnableAbsRecomputations();
			if ( trace3.fraction < 1.0 )
			{
				//Abort!
				m_bFreezeFrameCloseOnKiller = true;
			}
		}

		m_bStartedFreezeFrame = true;
	}

	//Vector vCamEnd = EyePosition() - ( vToTarget * 90 );
	if ( !m_bFreezeFrameCloseOnKiller && flDistToTarg < FREEZECAM_LONGCAM_DIST )
	{
		Vector vCamEnd = m_vecFreezeFrameStart - ( vToTarget * 8 );
		vCamEnd.z += MAX( (m_vecFreezeFrameStart.z - vecCamTarget.z )*0.75, 8 );

		Vector forward, right, up;
		QAngle angTemp;
		VectorAngles( vToTarget, angTemp );
		AngleVectors (angTemp, &forward, &right, &up );

		Vector vLRStart = m_vecFreezeFrameStart;
		Vector vLREnd = vCamEnd - ( right * m_nFreezeFrameShiftSideDist );
		trace_t traceLR;
		UTIL_TraceHull(m_vecFreezeFrameStart, vLREnd, WALL_MIN, WALL_MAX, MASK_SOLID, this, COLLISION_GROUP_NONE, &traceLR );

		IRagdoll *pRagdoll = GetRepresentativeRagdoll();
		if ( pRagdoll )
		{
			vLRStart = pRagdoll->GetRagdollOrigin();
			vLRStart.z = trace.endpos.z;
		}
		float distToOffset = vLRStart.AsVector2D().DistTo( trace.endpos.AsVector2D() );
		if ( traceLR.fraction < 0.2 || distToOffset < 16 )
		{
			// Abort!
			m_bFreezeFrameCloseOnKiller = true;
		}

		m_vecFreezeFrameEnd = traceLR.endpos;

		//DebugDrawLine( m_vecFreezeFrameStart, m_vecFreezeFrameEnd, 255, 0, 0, false, 25 );
	}

	float flNewFOV = spec_freeze_target_fov.GetFloat();

	// dont frame both player, just zoom on killer
	if ( m_bFreezeFrameCloseOnKiller )
	{
		m_vecFreezeFrameEnd = vTargetPos;
		flNewFOV = spec_freeze_target_fov_long.GetFloat();
	}

	// Look directly at the target
	vToTarget = vecCamTarget - prevEyeOrigin;
	VectorNormalize( vToTarget );
	VectorAngles( vToTarget, eyeAngles );
	// apply the tilt
	eyeAngles.z = Lerp( fInterpolant, 0.0f, m_flFreezeFrameTilt );

	// set the fov to the convar, but have it hit the target slightly before the camera stops
	fov = clamp( Lerp( fInterpolant + 0.05f, (float )GetDefaultFOV(), flNewFOV ), flNewFOV, GetDefaultFOV() );
	Interpolator_CurveInterpolate( INTERPOLATE_SIMPLE_CUBIC, m_vecFreezeFrameStart - Vector( 0, 0, -12 ), m_vecFreezeFrameStart, m_vecFreezeFrameEnd, vTargetPos, fInterpolant, eyeOrigin );

	float fCurTime = gpGlobals->curtime - m_flFreezeFrameStartTime;
	float fTravelTime = !m_bFreezeFrameCloseOnKiller ? spec_freeze_traveltime.GetFloat() : spec_freeze_traveltime_long.GetFloat();

	// cancel the light shortly after the freeze frame was taken
	if ( m_bSentFreezeFrame && fCurTime >= (fTravelTime + 0.25f ) )
	{
		CancelFreezeCamFlashlightEffect();
	}
	else
	{
		UpdateFreezeCamFlashlightEffect( pTarget, fInterpolant );
	}
	
	// [jason] check that our target position does not fall within the render extents of the target we're looking at;
	//	this can happen if our killer is in a tight spot and the camera is trying to avoid clipping geometry
	const int kFreezeCamTolerance = cl_freeze_cam_penetration_tolerance.GetInt();
	if ( !m_bSentFreezeFrame && kFreezeCamTolerance >= 0 )
	{
		// at really long distances the camera moves long distances each frame
		// this means that thecamera could bypass the target spot by a bunch, so
		// let' check to see if the target will surpass the target pos next frame and just go ahead and stop
		bool bStopCamera = false;
		float distFromPrev = eyeOrigin.AsVector2D().DistTo( prevEyeOrigin.AsVector2D() );
		float distToTargetPos = m_vecFreezeFrameEnd.AsVector2D().DistTo( eyeOrigin.AsVector2D() );

		// either use the render extents of the target, or the value we specified in the convar
		float targetRadius = (float )kFreezeCamTolerance;
		if ( m_bFreezeFrameCloseOnKiller && targetRadius <= 0.0f )
		{
			Vector vecMins, vecMaxs;
			pTarget->GetRenderBounds( vecMins, vecMaxs );
			targetRadius = vecMins.AsVector2D().DistTo( vecMaxs.AsVector2D() ) * 0.3f;
		}

		// figure out ho much we moved last frame and keep from clipping too far into the killer's face
		if ( distToTargetPos - (distFromPrev - distToTargetPos) < (targetRadius*0.5f) )
		{
			bStopCamera = true;
		}

		// disregard height, treat the target as an infinite cylinder so we don't end up with extreme up/down view angles
		float distFromTarget = vLookAt.AsVector2D().DistTo( eyeOrigin.AsVector2D() );

		if ( (m_bFreezeFrameCloseOnKiller && distFromTarget < targetRadius ) )
		{
			DevMsg( "CS_PLAYER: Detected overlap: Extents of target = %3.2f, Dist from them = %3.2f \n", targetRadius, distFromTarget );

			bStopCamera = true;
		}

		if ( bStopCamera )
		{
			eyeOrigin = prevEyeOrigin;
			eyeAngles = prevEyeAngles;
			fTravelTime = fCurTime;

			m_flFreezeFrameTilt = eyeAngles.z;
		}
	}

	if ( fCurTime >= fTravelTime && !m_bSentFreezeFrame )
	{
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "freezecam_started" );
		if ( pEvent )
		{
			gameeventmanager->FireEventClientSide( pEvent );
		}

		m_bSentFreezeFrame = true;
		view->FreezeFrame( spec_freeze_time.GetFloat() );
	}
}

float C_CSPlayer::GetDeathCamInterpolationTime()
{
	static ConVarRef sv_disablefreezecam( "sv_disablefreezecam" );
	if ( cl_disablefreezecam.GetBool() || sv_disablefreezecam.GetBool() || !GetObserverTarget() || g_HltvReplaySystem.IsDelayedReplayRequestPending() )
		return spec_freeze_time.GetFloat();
	else
		return spec_freeze_deathanim_time.GetFloat();
}

void C_CSPlayer::UpdateFreezeCamFlashlightEffect( C_BaseEntity *pTarget, float flAmount )
{
	if ( !pTarget )
	{
		CancelFreezeCamFlashlightEffect();
		return;
	}

	Vector brightness( spec_freeze_cinematiclight_r.GetFloat(), spec_freeze_cinematiclight_g.GetFloat(), spec_freeze_cinematiclight_b.GetFloat() );
	Vector dimWhite( 0.3f, 0.3f, 0.3f );

	if ( !m_bFreezeCamFlashlightActive )
	{
		//m_fFlashlightEffectStartTonemapScale = GetCurrentTonemapScale();
		m_bFreezeCamFlashlightActive = true;
		//m_fFlashlightEffectStartTime = gpGlobals->curtime;
		//m_flashLightFadeTimer.Start( 3.0f );
	}

	Vector vecFlashlightOrigin;
	Vector vecFlashlightForward( 0.0f, 0.0f, -1.0f );
	Vector vecFlashlightRight( 1.0f, 0.0f, 0.0f );
	Vector vecFlashlightUp( 0.0f, 1.0f, 0.0f );
	float fFOV = 0.0f;

	float invScale = 1.0f;
	//if ( m_fFlashlightEffectStartTonemapScale != 0.0f )
	//{
	//	invScale = 1.0f / m_fFlashlightEffectStartTonemapScale;
	//}
	brightness = (brightness * invScale * spec_freeze_cinematiclight_scale.GetFloat() ) * flAmount;

	//if ( isDying )
	{
		Vector targetOrig = pTarget->GetRenderOrigin();
		targetOrig.z += 32;
		Vector vToTarget = targetOrig - EyePosition();
		VectorNormalize( vToTarget );
		Vector forward, right, up;
		QAngle angTemp;
		VectorAngles( vToTarget, angTemp );
		AngleVectors (angTemp, &forward, &right, &up );

		if ( m_nFreezeFrameShiftSideDist > 0 )
			vecFlashlightOrigin = targetOrig + ( right * 80 );
		else
			vecFlashlightOrigin = targetOrig - ( right * 80 );
		vecFlashlightOrigin -= ( forward * 50 );
		vecFlashlightOrigin.z += 100.f;

		float flFOVExtra = 0;

		trace_t trace;
		UTIL_TraceLine( targetOrig, vecFlashlightOrigin, MASK_OPAQUE, pTarget, COLLISION_GROUP_DEBRIS, &trace );
		if ( trace.fraction >= 0.8 )
		{
			vecFlashlightOrigin = trace.endpos;
		}
		else
		{
			// just go the other way
			if ( m_nFreezeFrameShiftSideDist > 0 )
				vecFlashlightOrigin = targetOrig - ( right * 60 );
			else
				vecFlashlightOrigin = targetOrig + ( right * 60 );
			vecFlashlightOrigin -= ( forward * 40 );
			vecFlashlightOrigin.z += 80.f;
			UTIL_TraceLine( targetOrig, vecFlashlightOrigin, MASK_OPAQUE, pTarget, COLLISION_GROUP_DEBRIS, &trace );
			vecFlashlightOrigin = trace.endpos;

			flFOVExtra = (1 - trace.fraction ) * 20; 
			targetOrig.z += flFOVExtra;
		}

		Vector vToTarget2 = targetOrig - vecFlashlightOrigin;
		VectorNormalize( vToTarget2 );
		QAngle angTemp2;
		VectorAngles( vToTarget2, angTemp2 );
		AngleVectors (angTemp2, &vecFlashlightForward, &vecFlashlightRight, &vecFlashlightUp );

		fFOV = 50.f + flFOVExtra;
	}

	MDLCACHE_CRITICAL_SECTION();
	FlashlightEffectManager().EnableFlashlightOverride( true );
	FlashlightEffectManager().UpdateFlashlightOverride( true, vecFlashlightOrigin, vecFlashlightForward, vecFlashlightRight,
		vecFlashlightUp, fFOV, true, m_freezeCamSpotLightTexture, brightness );

	// force tonemapping down
	//if ( m_bOverrideTonemapping )
	//{
	//	SetOverrideTonemapScale( true, fTonemapScale );
	//}
}

void C_CSPlayer::CancelFreezeCamFlashlightEffect()
{
	if( m_bFreezeCamFlashlightActive )
	{
		FlashlightEffectManager().EnableFlashlightOverride( false );
		m_bFreezeCamFlashlightActive = false;
	}
}

void C_CSPlayer::CalcDeathCamView( Vector& eyeOrigin, QAngle& eyeAngles, float& fov )
{
	CBaseEntity* pKiller = NULL;

	if ( mp_forcecamera.GetInt() == OBS_ALLOW_ALL )
	{
		// if mp_forcecamera is off let user see killer or look around
		pKiller = GetObserverTarget();
		eyeAngles = EyeAngles();
	}

	// NOTE: CS_DEATH_ANIMATION_TIME differs from base class implementation
	float interpolation = ( gpGlobals->curtime - m_flDeathTime ) / CS_DEATH_ANIMATION_TIME;

	interpolation = clamp( interpolation, 0.0f, 1.0f );

	m_flObserverChaseDistance += gpGlobals->frametime*48.0f;
	m_flObserverChaseDistance = clamp( m_flObserverChaseDistance, 16, CHASE_CAM_DISTANCE );

	QAngle aForward = eyeAngles;
	Vector origin = EyePosition();

	IRagdoll *pRagdoll = GetRepresentativeRagdoll();
	if ( pRagdoll )
	{
		origin = pRagdoll->GetRagdollOrigin();
		origin.z += VEC_DEAD_VIEWHEIGHT.z; // look over ragdoll, not through
	}

	if ( pKiller && pKiller->IsPlayer() && pKiller != this )
	{
		//Get the vector from the dead body EyePos to the killers EyePos
		Vector vKiller = pKiller->EyePosition() - origin;

		// Get the angles for that vector
		QAngle aKiller; VectorAngles( vKiller, aKiller );
		// Interpolate from the original eye angles to point at the killers EyePos
		InterpolateAngles( aForward, aKiller, eyeAngles, interpolation );
	}

	// Get the vector for our new view.  It should be looking at the killer
	Vector vForward; AngleVectors( eyeAngles, &vForward );

	VectorNormalize( vForward );
	// Add the two vectors with the negative chase distance as a scale
	VectorMA( origin, -m_flObserverChaseDistance, vForward, eyeOrigin );

	trace_t trace; // clip against world
	C_BaseEntity::PushEnableAbsRecomputations( false ); // HACK don't recompute positions while doing RayTrace
	UTIL_TraceHull( origin, eyeOrigin, WALL_MIN, WALL_MAX, MASK_SOLID, this, COLLISION_GROUP_NONE, &trace );
	C_BaseEntity::PopEnableAbsRecomputations();

	if ( trace.fraction < 1.0 )
	{
		eyeOrigin = trace.endpos;
		m_flObserverChaseDistance = VectorLength( origin - eyeOrigin );
	}

	fov = GetFOV();
}

bool C_CSPlayer::IsCursorOnAutoAimTarget()
{
	// don't allow autoaiming on PC at all
	if ( IsPC() )
		return false;

// disabled 6/29/15 -mtw
/*

	// [sbodenbender] $TODO: for now, we will still allow autoaim even when blinded
	// code lifted from hintupdate in server side player code
	//if ( IsBlind() || IsObserver() )
	//	return false;

	QAngle aimAngles = GetFinalAimAngle();

	Vector forward;
	AngleVectors( aimAngles, &forward );

	trace_t tr;
	// Search for objects in a sphere (tests for entities that are not solid, yet still useable )
	Vector searchStart = EyePosition();
	const float flMaxSearchDistance = 4098.0f; // Make sure this works for long shots like with sniper rifles.
	Vector searchEnd = searchStart + forward * flMaxSearchDistance;

	//int useableContents = MASK_NPCSOLID_BRUSHONLY | MASK_VISIBLE_AND_NPCS;
	int useableContents = MASK_SHOT; 

	UTIL_TraceLine( searchStart, searchEnd, useableContents, this, COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction != 1.0f )
	{
		if (tr.DidHitNonWorldEntity() && tr.m_pEnt )
		{
			CBaseEntity *pEntity = tr.m_pEnt;
			// Autoaim at enemy players or at any entity that specifically requests it.
			if ( pEntity && ( ( pEntity->IsPlayer() && !InSameTeam(pEntity ) ) || pEntity->IsAutoaimTarget() ) ) 
			{
				return true;
			}
		}
	}
*/
	return false;
}

bool C_CSPlayer::CanUseGrenade( CSWeaponID nID )
{
	if ( nID == WEAPON_MOLOTOV || nID == WEAPON_INCGRENADE )
	{
		if ( gpGlobals->curtime < m_fMolotovUseTime )
		{
			// Can't use molotov until timer elapses
			return false;
		}
	}

	return true;
}

void C_CSPlayer::DisplayInventory( bool showPistol )
{
	if ( !C_BasePlayer::GetLocalPlayer() || !engine->IsLocalPlayerResolvable() )
		return;

	SFWeaponSelection *pHudWS = GET_HUDELEMENT( SFWeaponSelection );
	if ( !pHudWS )
	{
		return;
	}

	for ( int i = 0; i < WeaponCount(); ++i )
	{		
		CWeaponCSBase *pWeapon = dynamic_cast< CWeaponCSBase* > ( GetWeapon( i ) );
		

		if ( pWeapon == NULL )
			continue;

		if ( pWeapon->IsKindOf( WEAPONTYPE_GRENADE )  && ( ( CBaseCSGrenade * ) pWeapon )->GetIsThrown() )
		{
			// don't include thrown grenades in inventory
			continue;
		}

		CSWeaponID weaponId = pWeapon->GetCSWeaponID();

		if ( showPistol || !IsSecondaryWeapon( weaponId )  )
		{
			// TODO: weapon selection does this job now
			//pItemHistory->AddToHistory( pWeapon );

			pHudWS->ShowAndUpdateSelection();
		}
	}

	/*
	if ( HasDefuser() )
	{
		pItemHistory->AddToHistory( "defuser", "#Cstrike_BMDefuser" );
	}
	*/
}

MedalRank_t C_CSPlayer::GetRank( MedalCategory_t category )
{
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_bAnonymousPlayerIdentity )
			return MEDAL_RANK_NONE;
	}

	return m_rank.Get( category );
}

uint32 C_CSPlayer::GetMusicID( )
{
	return m_unMusicID.Get( );
}

void C_CSPlayer::UpdateAllBulletHitModels( void )
{
	if ( sv_showbullethits.GetBool() )
	{
		FOR_EACH_VEC( m_vecBulletHitModels, n )
		{
			m_vecBulletHitModels[n]->UpdatePosition();
		}
	}
	else
	{
		ClearAllBulletHitModels();
	}
}

void C_CSPlayer::ClearAllBulletHitModels( void )
{
	if ( m_vecBulletHitModels.Count() )
	{
		FOR_EACH_VEC( m_vecBulletHitModels, n )
		{
			m_vecBulletHitModels[n]->Release();
		}
		m_vecBulletHitModels.RemoveAll();
	}
}

void C_BulletHitModel::AttachToPlayer( C_CSPlayer *pTargetPlayer, int nBoneIndex, matrix3x4_t matLocalOffset, bool bIsHit, Vector vecStartPos )
{
	InitializeAsClientEntity( "models/tools/bullet_hit_marker.mdl", false );

	m_hPlayerParent = pTargetPlayer;
	MatrixCopy( matLocalOffset, m_matLocal );
	m_iBoneIndex = nBoneIndex;
	m_flTimeCreated = gpGlobals->curtime;
	m_bIsHit = bIsHit;
	m_vecStartPos = vecStartPos;
	UpdatePosition();
	SetAllowFastPath( false );
}


C_CSPlayer *C_BulletHitModel::GetPlayerParent() { return m_hPlayerParent.IsValid() ? static_cast< C_CSPlayer* >( m_hPlayerParent.Get() ) : NULL; }

bool C_BulletHitModel::UpdatePosition( void )
{
	C_CSPlayer *pPlayer = GetPlayerParent();
	if ( !pPlayer )
		return false;

	float flDuration = gpGlobals->curtime - m_flTimeCreated;
	if ( flDuration > 5 && !m_bIsHit )
		return false;

	matrix3x4_t mat_boneToWorld;

	if ( pPlayer->IsAlive() )
	{
		pPlayer->GetBoneTransform( m_iBoneIndex, mat_boneToWorld );
	}
	else if ( pPlayer->m_hRagdoll.Get() )
	{
		C_CSRagdoll *pRagdoll = (C_CSRagdoll* )pPlayer->m_hRagdoll.Get();
		C_BaseAnimating *pBaseAnimating = pRagdoll->GetBaseAnimating();
		pBaseAnimating->GetBoneTransform( m_iBoneIndex, mat_boneToWorld );
	}
	else
	{
		return false;
	}

	MatrixMultiply( mat_boneToWorld, m_matLocal, mat_boneToWorld );
	
	Vector vecPos;
	QAngle angAngles;
	MatrixAngles( mat_boneToWorld, angAngles, vecPos );

	SetAbsAngles( angAngles );

	if ( flDuration <= 0.1 )
		vecPos = Lerp( clamp(flDuration*10,0,1), m_vecStartPos, vecPos );

	SetAbsOrigin( vecPos );

	if ( m_bIsHit )
	{
		SetRenderColor( 240, 0, 0 );
	}
	else
	{
		float flDurationToColor = RemapValClamped( flDuration, 0, 0.8, 255, 0 );
		if ( fmod( gpGlobals->curtime, 0.25 ) < 0.125 )
			flDurationToColor = 0;
		flDurationToColor = MAX( flDurationToColor, 100 );
		SetRenderColor( flDurationToColor, flDurationToColor, flDurationToColor );
	}
	
	return true;
}

int C_BulletHitModel::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if ( m_hPlayerParent.IsValid() && m_hPlayerParent.Get() != GetLocalOrInEyeCSPlayer() && UpdatePosition() )
	{
		SetAllowFastPath( false );
		return BaseClass::DrawModel(flags, instance);
	}
	return 0;
}

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

#if defined ENABLE_CLIENT_INVENTORIES_FOR_OTHER_PLAYERS
CCSPlayerInventory *C_CSPlayer::Inventory( void )
{
	static CCSPlayerInventory dummy;
	if ( IsLocalPlayer() )
		return dynamic_cast< CCSPlayerInventory* >( CSInventoryManager()->GetLocalInventory() );
	if ( !g_PR )
		return &dummy;
	
	static XUID s_myXuid = steamapicontext->SteamUser()->GetSteamID().ConvertToUint64();
	XUID thisXuid = g_PR->GetXuid( entindex() );
	if ( thisXuid == INVALID_XUID )
		return &dummy;
	if ( thisXuid == s_myXuid )
		return dynamic_cast< CCSPlayerInventory* >( CSInventoryManager()->GetLocalInventory() );

	return static_cast< C_CS_PlayerResource * >( g_PR )->GetInventory( entindex() );
}

#endif

#endif //!defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )


// Adds a sound event to be played at the next round restart
void CStartOfRoundAudioPlayback::AddSound( CBaseEntity* pEntityPlayingSound, char* pName, float fPlaybackDuration )
{
	// Ensure that the sound is not already in the playback list
	for ( int i = 0; i < m_SoundEvents.Count(); ++i )
	{
		if ( Q_strcmp( pName, m_SoundEvents[i].m_SoundName ) == 0 )
		{
			// Found a match, so skip adding this sound
			return;
		}
	}

	if ( pEntityPlayingSound && pName && ( Q_strlen ( pName ) > 0 ) && ( fPlaybackDuration > 0.0f ) )
	{
		RoundStartSoundPlaybackData	playbackData;

		playbackData.m_pEntityPlayingSound = pEntityPlayingSound;

		Q_snprintf( playbackData.m_SoundName, sizeof( playbackData.m_SoundName ), "%s", pName );
		playbackData.m_fPlaybackTime = m_NextAvailableTime;
		m_NextAvailableTime += fPlaybackDuration;

		m_SoundEvents.AddToTail( playbackData );
	}
}

// Play all the queued sounds
void CStartOfRoundAudioPlayback::PlaySounds( void )
{
	m_bPlaybackEnabled = true;

	// Loop through all the queued sound events and set their time to play
	float curtime = gpGlobals->curtime;
	for ( int i = 0; i < m_SoundEvents.Count(); ++i )
	{
		m_SoundEvents[i].m_fPlaybackTime += curtime;
	}
}

// Play all the queued sounds
void CStartOfRoundAudioPlayback::Update( void )
{
	if ( m_bPlaybackEnabled )
	{
		int nTotalSoundsPlayed = 0;

		// Loop through each sound event to determine if it is time to play it
		for ( int i = 0; i < m_SoundEvents.Count(); ++i )
		{
			if ( !m_SoundEvents[i].m_bHasBeenPlayed )
			{
				if ( ( m_SoundEvents[i].m_fPlaybackTime > 0.0f ) && ( m_SoundEvents[i].m_fPlaybackTime < gpGlobals->curtime ) )
				{
					// Play the bonus grenade voiceover sound
					if ( m_SoundEvents[i].m_pEntityPlayingSound )
					{
						C_CSPlayer* pPlayer = static_cast<C_CSPlayer*>( m_SoundEvents[i].m_pEntityPlayingSound );
						C_RecipientFilter filter;
						filter.AddRecipient( pPlayer );
						C_BaseEntity::EmitSound( filter, pPlayer->entindex(), m_SoundEvents[i].m_SoundName );

						//DevMsg( "Played %s for Entity %d\n", m_SoundEvents[i].m_SoundName, pPlayer->entindex() );
					}

					m_SoundEvents[i].m_bHasBeenPlayed = true;
				}
			}
			else
			{
				nTotalSoundsPlayed++;
			}
		}

		if ( nTotalSoundsPlayed == m_SoundEvents.Count() )
		{
			// All sounds have been played, so clear the list of pending sound events
			ClearSounds();
		}
	}
}

// Clear all the queued sounds
void CStartOfRoundAudioPlayback::ClearSounds( void )
{
	m_SoundEvents.RemoveAll();
	m_NextAvailableTime = 1.0f;
	m_bPlaybackEnabled = false;
}

void CStartOfRoundAudioPlayback::RemoveSoundByName( char* pName )
{
	FOR_EACH_VEC_BACK( m_SoundEvents, i )
	{
		if ( V_strcmp( m_SoundEvents[i].m_SoundName, pName ) == 0 )
		{
			m_SoundEvents.Remove( i );
		}
	}
}


static void CC_PlayerDecalTraceBloodDev( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		Vector vForward; AngleVectors( pPlayer->EyeAngles(), &vForward );
		Vector vecSrc	= pPlayer->Weapon_ShootPosition();
		Vector vecEnd	= vecSrc + vForward * 512;

		trace_t tr;
		UTIL_TraceLine( vecSrc, vecEnd, MASK_SOLID, pPlayer, COLLISION_GROUP_NONE, &tr );

		UTIL_BloodDecalTrace( &tr, BLOOD_COLOR_RED );
	}
}

static ConCommand cl_dev_decaltrace_blood( "cl_dev_decaltrace_blood", CC_PlayerDecalTraceBloodDev, "Shoot out a decal spray that shoots blood.", FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY );
