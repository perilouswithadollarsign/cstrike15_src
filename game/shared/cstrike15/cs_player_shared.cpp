 //========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_csbase.h"
#include "decals.h"
#include "cs_gamerules.h"
#include "weapon_c4.h"
#include "in_buttons.h"
#include "datacache/imdlcache.h"
#include "GameStats.h"

#include "commonmacros.h"

#ifdef CLIENT_DLL
	#include "c_cs_player.h"
	#include "c_triggers.h"
	#include "c_plantedc4.h"
	#include "inputsystem/iinputsystem.h"
	#include "prediction.h"
	#include "c_cs_hostage.h"
	#include "c_cs_team.h"
	#include "gametypes.h"
	#include "c_cs_playerresource.h"

	#define CRecipientFilter C_RecipientFilter
	#define CCSPlayerResource C_CS_PlayerResource
#else
	#include "cs_player.h"
	#include "soundent.h"
	#include "bot/cs_bot.h"
	#include "keyvalues.h"
	#include "triggers.h"
	#include "cs_gamestats.h"
	#include "recipientfilter.h"
	#include "cs_simple_hostage.h"
	#include "predicted_viewmodel.h"
	#include "cs_team.h"
	#include "cs_player_resource.h"
#endif

#include "cs_playeranimstate.h"
#include "basecombatweapon_shared.h"
#include "util_shared.h"
#include "takedamageinfo.h"
#include "effect_dispatch_data.h"
#include "engine/ivdebugoverlay.h"
#include "obstacle_pushaway.h"
#include "props_shared.h"
#include "ammodef.h"
#include "platforminputdevice.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "obstacle_pushaway.h"
#include "gametypes.h"

#include "playerdecals_signature.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar sv_penetration_type( "sv_penetration_type", "1", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "What type of penertration to use. 0 = old CS, 1 = new penetration" );
ConVar sv_showimpacts_penetration( "sv_showimpacts_penetration", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Shows extra data when bullets penetrate. (use sv_showimpacts_time to increase time shown)" );

ConVar sv_showbullethits("sv_showbullethits", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "" );

ConVar sv_showimpacts("sv_showimpacts", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Shows client (red) and server (blue) bullet impact point (1=both, 2=client-only, 3=server-only)" );
ConVar sv_showimpacts_time("sv_showimpacts_time", "4", FCVAR_REPLICATED | FCVAR_RELEASE, "Duration bullet impact indicators remain before disappearing", true, 0.0f, true, 10.0f );
ConVar sv_showplayerhitboxes( "sv_showplayerhitboxes", "0", FCVAR_REPLICATED, "Show lag compensated hitboxes for the specified player index whenever a player fires." );

// friendly fire damage scalers
ConVar	ff_damage_reduction_grenade( "ff_damage_reduction_grenade", "0.25", FCVAR_REPLICATED | FCVAR_RELEASE, "How much to reduce damage done to teammates by a thrown grenade.  Range is from 0 - 1 (with 1 being damage equal to what is done to an enemy)" );
ConVar	ff_damage_reduction_grenade_self( "ff_damage_reduction_grenade_self", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "How much to damage a player does to himself with his own grenade.  Range is from 0 - 1 (with 1 being damage equal to what is done to an enemy)" );
ConVar	ff_damage_reduction_bullets( "ff_damage_reduction_bullets", "0.1", FCVAR_REPLICATED | FCVAR_RELEASE, "How much to reduce damage done to teammates when shot.  Range is from 0 - 1 (with 1 being damage equal to what is done to an enemy)" );
ConVar	ff_damage_reduction_other( "ff_damage_reduction_other", "0.25", FCVAR_REPLICATED | FCVAR_RELEASE, "How much to reduce damage done to teammates by things other than bullets and grenades.  Range is from 0 - 1 (with 1 being damage equal to what is done to an enemy)" );
ConVar  ff_damage_bullet_penetration( "ff_damage_bullet_penetration", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "If friendly fire is off, this will scale the penetration power and damage a bullet does when penetrating another friendly player", true, 0.0f, true, 1.0f );

ConVar sv_spec_use_tournament_content_standards( "sv_spec_use_tournament_content_standards", "0.0", FCVAR_REPLICATED | FCVAR_RELEASE );

#if defined( CLIENT_DLL )
ConVar cl_spec_use_tournament_content_standards( "cl_spec_use_tournament_content_standards", "0.0", FCVAR_RELEASE );
#endif

extern ConVar mp_teammates_are_enemies;
extern ConVar mp_use_respawn_waves;
extern ConVar mp_respawn_on_death_ct;
extern ConVar mp_respawn_on_death_t;
extern ConVar mp_buy_allow_grenades;
extern ConVar mp_buy_anywhere;
extern ConVar mp_buy_during_immunity;
extern ConVar mp_free_armor;

#define	CS_MASK_SHOOT (MASK_SOLID|CONTENTS_DEBRIS)
#define MAX_PENETRATION_DISTANCE 90 // this is 7.5 feet

#define CS_MAX_WALLBANG_TRAIL_LENGTH 800

void DispatchEffect( const char *pName, const CEffectData &data );

#if defined( CLIENT_DLL )

#define CPlantedC4 C_PlantedC4

extern void StartParticleEffect( const CEffectData &data, int nSplitScreenPlayerSlot = -1 );

#endif

#if defined( _DEBUG ) && !defined( CLIENT_DLL )

	// This is some extra code to collect weapon accuracy stats:

	struct bulletdata_s
	{
		float	timedelta;	// time delta since first shot of this round
		float	derivation;	// derivation for first shoot view angle
		int		count;
	};

	#define STATS_MAX_BULLETS	50

	static bulletdata_s s_bullet_stats[STATS_MAX_BULLETS];

	Vector	s_firstImpact = Vector(0,0,0);
	float	s_firstTime = 0;
	float	s_LastTime = 0;
	int		s_bulletCount = 0;

	void ResetBulletStats()
	{
		s_firstTime = 0;
		s_LastTime = 0;
		s_bulletCount = 0;
		s_firstImpact = Vector(0,0,0);
		Q_memset( s_bullet_stats, 0, sizeof(s_bullet_stats) );
	}

	void PrintBulletStats()
	{
		for (int i=0; i<STATS_MAX_BULLETS; i++ )
		{
			if (s_bullet_stats[i].count == 0)
				break;

			Msg("%3i;%3i;%.4f;%.4f\n", i, s_bullet_stats[i].count, 
				s_bullet_stats[i].timedelta, s_bullet_stats[i].derivation );
		}
	}

	void AddBulletStat( float time, float dist, Vector &impact )
	{
		if ( time > s_LastTime + 2.0f )
		{
			// time delta since last shoot is bigger than 2 seconds, start new row
			s_LastTime = s_firstTime = time;
			s_bulletCount = 0;
			s_firstImpact = impact;

		}
		else
		{
			s_LastTime = time;
			s_bulletCount++;
		}

		if ( s_bulletCount >= STATS_MAX_BULLETS )
			s_bulletCount = STATS_MAX_BULLETS -1;

		if ( dist < 1 )
			dist = 1;
		
		int i = s_bulletCount;

		float offset = VectorLength( s_firstImpact - impact );

		float timedelta = time - s_firstTime;
		float derivation = offset / dist;

		float weight = (float)s_bullet_stats[i].count/(float)(s_bullet_stats[i].count+1);

		s_bullet_stats[i].timedelta *= weight;
		s_bullet_stats[i].timedelta += (1.0f-weight) * timedelta;

		s_bullet_stats[i].derivation *= weight;
		s_bullet_stats[i].derivation += (1.0f-weight) * derivation;

		s_bullet_stats[i].count++;
	}

	CON_COMMAND( stats_bullets_reset, "Reset bullet stats")
	{
		ResetBulletStats();
	}

	CON_COMMAND( stats_bullets_print, "Print bullet stats")
	{
		PrintBulletStats();
	}

#endif

Vector CCSPlayer::Weapon_ShootPosition()
{
	Vector vecPos = BaseClass::Weapon_ShootPosition();

	// fail out to un-altered position
	if ( !m_bUseNewAnimstate || !m_PlayerAnimStateCSGO )
		return vecPos;

	// warning: the modify eye position call will query and set up bones
	// on the game server it is called when giving weapon items or firing bullets
	m_PlayerAnimStateCSGO->ModifyEyePosition( vecPos );

	return vecPos;
}

bool CCSPlayer::IsInBuyZone()
{
	if ( mp_buy_anywhere.GetInt() == 1 ||
		mp_buy_anywhere.GetInt() == GetTeamNumber() )
		return true;//m_bGunGameImmunity;

	return m_bInBuyZone;
}

bool CCSPlayer::IsInBuyPeriod()
{
	if ( mp_buy_during_immunity.GetInt() == 1 ||
		mp_buy_during_immunity.GetInt() == GetTeamNumber() )
	{
		return m_bGunGameImmunity;
	}
	else
	{

		return CSGameRules() ? !CSGameRules()->IsBuyTimeElapsed() : false;
	}
}

bool CCSPlayer::CanBuyDuringImmunity()
{
	return ( mp_buy_during_immunity.GetInt() == 1 ) || ( mp_buy_during_immunity.GetInt() == GetTeamNumber() ); 
}

bool CCSPlayer::IsAbleToInstantRespawn( void )
{
	if ( CSGameRules() )
	{
		switch( CSGameRules()->GetGamePhase() )
		{
		case GAMEPHASE_MATCH_ENDED:
		case GAMEPHASE_HALFTIME:
			return false;
		}

		if ( CSGameRules()->IsWarmupPeriod() )
			return true;
	}

	// if we use respawn waves AND the next respawn wave is past AND our team is able to respawn OR it is the warmup period
	return (	CSGameRules() && ( ( mp_respawn_on_death_ct.GetBool() && GetTeamNumber() == TEAM_CT ) || 
		( mp_respawn_on_death_t.GetBool() && GetTeamNumber() == TEAM_TERRORIST ) ) );
}

char const * CCSPlayer::IsAbleToApplySpray( trace_t *ptr, Vector *pvecForward, Vector *pvecRight )
{
	Vector forward, right;
	EyeVectors( &forward, &right );
	if ( pvecForward ) *pvecForward = forward;
	if ( pvecRight ) *pvecRight = right;

	if ( !IsAlive() || ( GetTeamNumber() != TEAM_CT && GetTeamNumber() != TEAM_TERRORIST ) )
		return "#SFUI_Notice_SprayPaint_NotAlive";

	trace_t	tr;

	Vector vecTraceOriginLocation = Weapon_ShootPosition();
	Vector vecTraceTestMaxDepth = vecTraceOriginLocation + forward * 96;
	UTIL_TraceLine( vecTraceOriginLocation,
		vecTraceTestMaxDepth,
		CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_WINDOW,
		this, COLLISION_GROUP_NONE, &tr );

	if ( tr.fraction == 1.0 )
		return "#SFUI_Notice_SprayPaint_OutOfRange";

	// Have we hit something TOOLS/???
	if ( tr.surface.name && StringHasPrefix( tr.surface.name, "tools/" ) )
		return "#SFUI_Notice_SprayPaint_BadSurface";

	// Do a second trace to see if anything weird blocks the trace?
	// an example could be a destructible door, so spray would go through onto the wall behind it
	// then we should ignore the first trace obtained
	trace_t tr2;
	UTIL_TraceLine( vecTraceOriginLocation,
		vecTraceTestMaxDepth,
		CONTENTS_OPAQUE | CONTENTS_MONSTER,
		this, COLLISION_GROUP_NONE, &tr2 );
	if ( tr2.fraction + 0.001 < tr.fraction )
		return "#SFUI_Notice_SprayPaint_Obstructed";

	//
	// Now that we have the location+normal of the trace hit do some validation checks for
	// whether that's a good way to apply a decal
	//
	Vector vecHitToEye = ( vecTraceOriginLocation - tr.endpos );
	if ( vecHitToEye.LengthSqr() < 0.5f )
		return "#SFUI_Notice_SprayPaint_TooClose";

	/*vec_t lenToHit = */ vecHitToEye.NormalizeInPlace();
	if ( tr.plane.normal.Dot( vecHitToEye ) < 0.1f )
		return "#SFUI_Notice_SprayPaint_GrazingAngle"; // angle too grazing (or backfacing normal), 0.1 = 85 degrees

	if ( ptr )
		*ptr = tr;

	return NULL; // all trace tests passed
}

float CCSPlayer::GetPlayerMaxSpeed()
{
	if ( GetMoveType() == MOVETYPE_NONE )
	{
		return CS_PLAYER_SPEED_STOPPED;
	}

	if ( IsObserver() )
	{
		// Player gets speed bonus in observer mode
		return CS_PLAYER_SPEED_OBSERVER;
	}

	bool bValidMoveState = ( State_Get() == STATE_ACTIVE || State_Get() == STATE_OBSERVER_MODE );
	if ( !bValidMoveState || m_bIsDefusing || m_bIsGrabbingHostage || (CSGameRules()->IsFreezePeriod() && !m_bCanMoveDuringFreezePeriod) )
	{
		// Player should not move during the freeze period
		return CS_PLAYER_SPEED_STOPPED;
	}

	float speed = BaseClass::GetPlayerMaxSpeed();

	speed = MIN( CS_PLAYER_SPEED_RUN, speed );
	
	if ( IsVIP() == true )  // VIP is slow due to the armour he's wearing
	{
		speed = CS_PLAYER_SPEED_VIP;
	}
	else if ( m_hCarriedHostage != NULL )
	{
		speed = CS_PLAYER_SPEED_HAS_HOSTAGE;
	}
	else
	{
		CWeaponCSBase *pWeapon = dynamic_cast<CWeaponCSBase*>( GetActiveWeapon() );

		if ( pWeapon )
		{
			if ( HasShield() && IsShieldDrawn() )
			{
				speed = MIN( CS_PLAYER_SPEED_SHIELD, speed );	
			}
			else
			{
				speed = MIN( pWeapon->GetMaxSpeed(), speed );
			}
		}
	}

//	if ( HasExosuit() )
//		speed *= CS_PLAYER_SPEED_EXOSUIT_RUN_BONUS;

	return speed;
}

void CCSPlayer::GiveCarriedHostage( EHANDLE hHostage )
{
	if ( !IsAlive() )
		return;

	m_hCarriedHostage = hHostage;

	RefreshCarriedHostage( true );
}

void CCSPlayer::RefreshCarriedHostage( bool bForceCreate )
{
#ifndef CLIENT_DLL 
	if ( m_hCarriedHostage == NULL )
		return;

	if ( m_hCarriedHostageProp == NULL )
	{
		CHostageCarriableProp *pHostageProp = dynamic_cast< CHostageCarriableProp* >( CreateEntityByName( "hostage_carriable_prop" ) );

		if ( pHostageProp )
		{
			pHostageProp->SetAbsOrigin( GetAbsOrigin() );
			pHostageProp->SetSolid( SOLID_NONE );
			pHostageProp->SetModel( "models/hostage/hostage_carry.mdl" );
			pHostageProp->SetModelName( MAKE_STRING( "models/hostage/hostage_carry.mdl" ) );
			pHostageProp->SetParent( this );
			pHostageProp->SetOwnerEntity( this );
			pHostageProp->FollowEntity( this );
			m_hCarriedHostageProp = pHostageProp;

			CRecipientFilter filter;
			filter.MakeReliable();
			filter.AddRecipient( this );
			UTIL_ClientPrintFilter( filter, HUD_PRINTCENTER, "#Cstrike_TitlesTXT_CarryingHostage" );
		}
	}

	if ( bForceCreate && GetViewModel( 1 ) )
	{
		CBaseViewModel *vm = GetViewModel( 1 );
		UTIL_Remove( vm );
		m_hViewModel.Set( 1, INVALID_EHANDLE );
	}

	CPredictedViewModel *vm = NULL;

	CBaseViewModel *pVM = GetViewModel( 1 );
	if ( pVM )
		vm = ( CPredictedViewModel * )pVM;
	else
	{
		vm = ( CPredictedViewModel * )CreateEntityByName( "predicted_viewmodel" );
		bForceCreate = true;
	}

	if ( vm )
	{
		vm->SetAbsOrigin( GetAbsOrigin() );
		vm->SetOwner( this );
		vm->SetIndex( 1 );
		int nAct = ACT_VM_IDLE;
		if ( bForceCreate )
		{
			nAct = ACT_VM_DRAW;
			DispatchSpawn( vm );
		}
		vm->FollowEntity( this, false );
		vm->SetModel( "models/hostage/v_hostage_arm.mdl" );

		int	idealSequence = vm->SelectWeightedSequence( ( Activity )nAct );
		if ( idealSequence >= 0 )
		{
			vm->SendViewModelMatchingSequence( idealSequence );
		}
		vm->SetShouldIgnoreOffsetAndAccuracy( true );

		m_hViewModel.Set( 1, vm );

		m_hHostageViewModel = vm;
	}

#endif
}

void CCSPlayer::RemoveCarriedHostage( void )
{
	m_hCarriedHostage = NULL;

#ifndef CLIENT_DLL 
	if ( m_hCarriedHostageProp )
	{
		CBaseAnimating *pHostageProp = dynamic_cast< CBaseAnimating* >( m_hCarriedHostageProp.Get() );
		if ( pHostageProp )
		{
			pHostageProp->FollowEntity( NULL );
			UTIL_Remove( pHostageProp );
		}

		m_hCarriedHostageProp = NULL;
	}

	if ( m_hHostageViewModel || dynamic_cast<CPredictedViewModel*>(GetViewModel( 1 )) )
	{
		CPredictedViewModel *pHostageVM = dynamic_cast< CPredictedViewModel* >( m_hHostageViewModel.Get() );
		if ( !pHostageVM )
			pHostageVM = dynamic_cast<CPredictedViewModel*>(GetViewModel( 1 ));

		if ( pHostageVM )
		{
			pHostageVM->FollowEntity( NULL );
			UTIL_Remove( pHostageVM );
		}

		m_hHostageViewModel = INVALID_EHANDLE;

		m_hViewModel.Set( 1, INVALID_EHANDLE );
	}
#endif
}

void CCSPlayer::GetBulletTypeParameters( 
	int iBulletType, 
	float &fPenetrationPower, 
	float &flPenetrationDistance )
{
	if ( sv_penetration_type.GetInt() == 1 )
	{
		fPenetrationPower = 35;
		flPenetrationDistance = 3000.0;
		return;
	}

	//MIKETODO: make ammo types come from a script file.
	if ( IsAmmoType( iBulletType, BULLET_PLAYER_50AE ) )
	{
		fPenetrationPower = 30;
		flPenetrationDistance = 1000.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_762MM ) )
	{
		fPenetrationPower = 39;
		flPenetrationDistance = 5000.0;
	}
	else if (	IsAmmoType( iBulletType, BULLET_PLAYER_556MM ) || 
				IsAmmoType( iBulletType, BULLET_PLAYER_556MM_SMALL ) ||
				IsAmmoType( iBulletType, BULLET_PLAYER_556MM_BOX ) )
	{
		fPenetrationPower = 35;
		flPenetrationDistance = 4000.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_338MAG ) )
	{
		fPenetrationPower = 45;
		flPenetrationDistance = 8000.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_9MM ) )
	{
		fPenetrationPower = 21;
		flPenetrationDistance = 800.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_BUCKSHOT ) )
	{
		fPenetrationPower = 0;
		flPenetrationDistance = 0.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_45ACP ) )
	{
		fPenetrationPower = 15;
		flPenetrationDistance = 500.0;
	}
	else if (	IsAmmoType( iBulletType, BULLET_PLAYER_357SIG ) ||
				IsAmmoType( iBulletType, BULLET_PLAYER_357SIG_SMALL ) ||
				IsAmmoType( iBulletType, BULLET_PLAYER_357SIG_P250 ) ||
				IsAmmoType( iBulletType, BULLET_PLAYER_357SIG_MIN ) )
	{
		fPenetrationPower = 25;
		flPenetrationDistance = 800.0;
	}
	else if ( IsAmmoType( iBulletType, BULLET_PLAYER_57MM ) )
	{
		fPenetrationPower = 30;
		flPenetrationDistance = 2000.0;
	}
	else if ( IsAmmoType( iBulletType, AMMO_TYPE_TASERCHARGE ) )
	{
		fPenetrationPower = 0;
		flPenetrationDistance = 0.0;
	}
	else
	{
		// What kind of ammo is this?
		Assert( false );
		fPenetrationPower = 0;
		flPenetrationDistance = 0.0;
	}
}

static bool TraceToExit( Vector start, Vector dir, Vector &end, trace_t &trEnter, trace_t &trExit, float flStepSize, float flMaxDistance )
{
	float flDistance = 0;
	Vector last = start;
	int nStartContents = 0;

	while ( flDistance <= flMaxDistance )
	{
		flDistance += flStepSize;

		end = start + ( flDistance * dir );

		Vector vecTrEnd = end - ( flStepSize * dir );

		if ( nStartContents == 0 )
			nStartContents = UTIL_PointContents( end, CS_MASK_SHOOT|CONTENTS_HITBOX );

		int nCurrentContents = UTIL_PointContents( end, CS_MASK_SHOOT|CONTENTS_HITBOX );

		if ( (nCurrentContents & CS_MASK_SHOOT) == 0 || ((nCurrentContents & CONTENTS_HITBOX) && nStartContents != nCurrentContents) )
		{
			// this gets a bit more complicated and expensive when we have to deal with displacements
			UTIL_TraceLine( end, vecTrEnd, CS_MASK_SHOOT|CONTENTS_HITBOX, NULL, &trExit );

			// we exited the wall into a player's hitbox
			if ( trExit.startsolid == true && (trExit.surface.flags & SURF_HITBOX)/*( nStartContents & CONTENTS_HITBOX ) == 0 && (nCurrentContents & CONTENTS_HITBOX)*/ )
			{
				// do another trace, but skip the player to get the actual exit surface 
				UTIL_TraceLine( end, start, CS_MASK_SHOOT, trExit.m_pEnt, COLLISION_GROUP_NONE, &trExit );
				if ( trExit.DidHit() && trExit.startsolid == false )
				{
					end = trExit.endpos;
					return true;
				}
			}
			else if ( trExit.DidHit() && trExit.startsolid == false )
			{
				bool bStartIsNodraw = !!( trEnter.surface.flags & (SURF_NODRAW) );
				bool bExitIsNodraw = !!( trExit.surface.flags & (SURF_NODRAW) );
				if ( bExitIsNodraw && IsBreakableEntity( trExit.m_pEnt ) && IsBreakableEntity( trEnter.m_pEnt ) )
				{
					// we have a case where we have a breakable object, but the mapper put a nodraw on the backside
					end = trExit.endpos;
					return true;
				}
				else if ( bExitIsNodraw == false || (bStartIsNodraw && bExitIsNodraw) ) // exit nodraw is only valid if our entrace is also nodraw
				{
					Vector vecNormal = trExit.plane.normal;
					float flDot = dir.Dot( vecNormal );
					if ( flDot <= 1.0f )
					{
						// get the real end pos
						end = end - ( (flStepSize * trExit.fraction) * dir );
						return true;
					}
				}
			}
			else if ( trEnter.DidHitNonWorldEntity() && IsBreakableEntity( trEnter.m_pEnt ) )
			{
				// if we hit a breakable, make the assumption that we broke it if we can't find an exit (hopefully..)
				// fake the end pos
				trExit = trEnter;
				trExit.endpos = start + ( 1.0f * dir );
				return true;
			}
		}
	}

	return false;
}

inline void UTIL_TraceLineIgnoreTwoEntities( const Vector& vecAbsStart, const Vector& vecAbsEnd, unsigned int mask, 
					 const IHandleEntity *ignore, const IHandleEntity *ignore2, int collisionGroup, trace_t *ptr )
{
	Ray_t ray;
	ray.Init( vecAbsStart, vecAbsEnd );
	CTraceFilterSkipTwoEntities traceFilter( ignore, ignore2, collisionGroup );
	enginetrace->TraceRay( ray, mask, &traceFilter, ptr );
	if( r_visualizetraces.GetBool() )
	{
		DebugDrawLine( ptr->startpos, ptr->endpos, 255, 0, 0, true, -1.0f );
	}
}

#ifndef CLIENT_DLL
void CCSPlayer::CheckForWeaponFiredAchievement()
{
	// Removed the Smorgasbord achievement... for now
}
#endif

bool CCSPlayer::IsPrimaryOrSecondaryWeapon( CSWeaponType nType )
{
	if ( nType == WEAPONTYPE_PISTOL || nType == WEAPONTYPE_SUBMACHINEGUN || nType == WEAPONTYPE_RIFLE ||  
		nType == WEAPONTYPE_SHOTGUN || nType == WEAPONTYPE_SNIPER_RIFLE || nType == WEAPONTYPE_MACHINEGUN )
	{
		return true;
	}

	return false;
}


bool CCSPlayer::IsOtherSameTeam( int nTeam )
{
	return GetTeamNumber() == nTeam;
}

bool CCSPlayer::IsOtherEnemy( CCSPlayer *pPlayer )
{
	if ( !pPlayer )
		return false;

	// we are never an enemy of ourselves
	if ( entindex() == pPlayer->entindex() )
		return false;
	
	int nOtherTeam = pPlayer->GetAssociatedTeamNumber();

	int nTeam = GetAssociatedTeamNumber();


	if ( mp_teammates_are_enemies.GetBool() && nTeam == nOtherTeam )
	{
		return true;
	}

	return nTeam != nOtherTeam;
}

uint32 CCSPlayer::GetActiveQuestID( void ) const
{
	uint32 unQuestID = 
#if defined ( CLIENT_DLL )
	 CSInventoryManager()->GetLocalCSInventory() ? CSInventoryManager()->GetLocalCSInventory()->GetActiveQuestID() : 0;
#else
	 m_Inventory.GetActiveQuestID(); 
#endif

	return unQuestID;
}

#if defined( CLIENT_DLL )
static bool ClientThinksQuestIsOk(CCSGameRules* pGameRules, const CCSPlayer* pPlayer)
{
	if ( !pGameRules || !pPlayer )
		return false;

	// Should also be handled by QuestProgressReason set to QUEST_NONOFFICIAL_SERVER, but just to be safe, handle it here too.
	if ( !pGameRules->IsQuestEligible() )
		return false;

	uint32 unQuestID = pPlayer->GetActiveQuestID();
	if ( !unQuestID )
		return false;

	CEconQuestDefinition *pQuestDef = GetItemSchema()->GetQuestDefinition( unQuestID );
	if ( !pQuestDef )
		return false;

	// This is a clone of the server logic in Helper_InitializeQuestDataFromInventory() in cs_player.cpp

	//
	// Check the game mode
	//
	char const *szRequireGameType = NULL;
	if ( !g_pGameTypes->GetGameTypeFromMode( pQuestDef->GetGameMode(), szRequireGameType ) )
		return false;

	int nRequireGameType = -1;
	int nRequireGameMode = -1;
	if ( !g_pGameTypes->GetGameModeAndTypeIntsFromStrings( szRequireGameType, pQuestDef->GetGameMode(), nRequireGameType, nRequireGameMode ) )
		return false;

	if ( ( g_pGameTypes->GetCurrentGameType() != nRequireGameType ) || ( g_pGameTypes->GetCurrentGameMode() != nRequireGameMode ) )
		return false;

	//
	// check the map group
	//
	const char* szCurMapGroup = engine->GetMapGroupName();
	bool bHasMapGroupReq = pQuestDef->GetMapGroup() && pQuestDef->GetMapGroup()[0];
	if ( bHasMapGroupReq && stricmp( szCurMapGroup, pQuestDef->GetMapGroup() ) != 0 )
		return false;

	// check the map
	const char *szCurMapName = engine->GetLevelNameShort();
	bool bHasMapReq = pQuestDef->GetMap() && pQuestDef->GetMap()[0];
	if ( bHasMapReq && stricmp( szCurMapName, pQuestDef->GetMap() ) != 0 )
		return false;

	//
	// mapgroup, gamemode, and/or map are valid.

	// Note: We can't check the server quest here; co-op missions require the server to have been
	// started exactly for that mission.  Assume the server is wrong here, we don't want to display
	// a de-sync message.
	if ( CSGameRules()->IsPlayingCooperativeGametype() )
		return false;

	// TODO: There are some longer-term things not checked here, but in quest rules, such as
	//       cond_team_terrorist.  Ideally we'd like to put those in your quest progression
	//       reason too, but I'm leaving that for a future update.  We should probably put
	//       those outside of the quest evaluation expression into a separate set of state
	//       that is easier to message failure reasons for than arbitrary conditional expressions.

	// Alright, we think we are allowed to make progress!
	return true;
}
#endif

QuestProgress::Reason CCSPlayer::GetQuestProgressReason( void ) const
{
#if defined( CLIENT_DLL )
	// On the client, we want to compare our expected quest state with the server

	// Non-official server, don't bother
	if ( m_nQuestProgressReason == QuestProgress::QUEST_NONOFFICIAL_SERVER )
		return m_nQuestProgressReason;

	// If we are OK, skip the more expensive test
	if ( m_nQuestProgressReason == QuestProgress::QUEST_OK || m_nQuestProgressReason == QuestProgress::QUEST_NOT_ENOUGH_PLAYERS )
		return m_nQuestProgressReason;

	// If we get here, the server thinks we don't have a valid quest.  Check if we believe it by looking at the state in our inventory.
	if ( CSGameRules() && ClientThinksQuestIsOk( CSGameRules(), this ) )
	{
		// The server doesn't initialize quests until after warmup, so during warmup we will trust that the client's state is valid.
		if ( CSGameRules()->IsWarmupPeriod() )
			return QuestProgress::QUEST_WARMUP;

		// Otherwise, the client thinks the quest should be able to make progress but the server doesn't.  Notify the de-sync'd state.
		return QuestProgress::QUEST_NOT_SYNCED_WITH_SERVER;
	}
#endif

	return m_nQuestProgressReason;
}


bool CCSPlayer::IsAssassinationTarget( void ) const
{
	CCSPlayerResource* pCSPR =
#if defined  ( CLIENT_DLL )
		GetCSResources();
#else
		CSPlayerResource();
#endif

	if ( !pCSPR )
		return false;
	
	return pCSPR->IsAssassinationTarget( entindex() );
}



bool CCSPlayer::IsOtherEnemy( int nEntIndex )
{
	CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( nEntIndex );
	if ( !pPlayer )
	{
		// client doesn't have a pointer to enemy players outside our PVS
#if defined ( CLIENT_DLL )	

		// we are never an enemy of ourselves
		if ( entindex() == nEntIndex )
			return false;

		C_CS_PlayerResource *pCSPR = GetCSResources();
		if ( pCSPR )
		{
			int nOtherTeam = pCSPR->GetTeam( nEntIndex );
			int nTeam = GetAssociatedTeamNumber();

			if ( mp_teammates_are_enemies.GetBool() && nTeam == nOtherTeam )
			{
				return true;
			}

			return nTeam != nOtherTeam;
		}
#endif

		return false;
	}

	return IsOtherEnemy( pPlayer );
}

bool CCSPlayer::IsOtherEnemyAndPlaying( int nEntIndex )
{
	int nOtherTeam = 0;

	CCSPlayer *pPlayer = (CCSPlayer*) UTIL_PlayerByIndex( nEntIndex );
	if ( !pPlayer )
	{
#if defined ( CLIENT_DLL )	
		C_CS_PlayerResource *pCSPR = GetCSResources();
		if ( !pCSPR )
			return false;

		nOtherTeam = pCSPR->GetTeam( nEntIndex );
#else if
		return false;
#endif
	}

	if ( ( GetTeamNumber() == TEAM_CT && nOtherTeam == TEAM_TERRORIST ) ||
		( GetTeamNumber() == TEAM_TERRORIST && nOtherTeam == TEAM_CT ) )
		return true;

	return false;
}

bool CCSPlayer::CanPlayerBuy( bool display )
{
	if ( !CSGameRules() )
		return false;

	// is the player alive?
	if ( m_lifeState != LIFE_ALIVE )
	{
		return false;
	}

	// is the player in a buy zone?
	if ( !IsInBuyZone() )
	{
		return false;
	}

	// Don't allow buying in the last few seconds of warmup because everybody should be freezed, but sometimes people aren't
	// also fixes buy on the very moment that round starts which might cause the bought weapon to spawn, but touched by the
	// player in the actual match time next frame and have a powerful gun for the first pistol round.
	if ( CSGameRules()->IsWarmupPeriod() && ( CSGameRules()->GetWarmupPeriodEndTime() - 3 < gpGlobals->curtime ) )
		return false;

	// Even if buytime has run out, if buy time is tied to immunity then we let the player buy if the menu is still open.
	if ( !IsInBuyPeriod() && CanBuyDuringImmunity() )
			return IsBuyMenuOpen();

	CCSGameRules* mp = CSGameRules();

	if ( mp->m_bCTCantBuy && ( GetTeamNumber() == TEAM_CT ) )
	{
		if ( display == true )
			ClientPrint( this, HUD_PRINTCENTER, "#CT_cant_buy" );

		return false;
	}

	if ( mp->m_bTCantBuy && ( GetTeamNumber() == TEAM_TERRORIST ) )
	{
		if ( display == true )
			ClientPrint( this, HUD_PRINTCENTER, "#Terrorist_cant_buy" );

		return false;
	}

	if ( CSGameRules()->IsPlayingCoopGuardian() )
	{
		if ( CSGameRules()->IsWarmupPeriod() == false &&
			CSGameRules()->m_flGuardianBuyUntilTime < gpGlobals->curtime )
		{
			int nTeam = CSGameRules()->IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT;
			if ( GetTeamNumber() == nTeam )
			{
				if ( display == true )
				{
#ifdef CLIENT_DLL
					static ConVarRef sv_buy_status_override_ref( "sv_buy_status_override" );
					int iBuyStatus = sv_buy_status_override_ref.GetInt();
#else
					extern ConVar sv_buy_status_override;
					int iBuyStatus = sv_buy_status_override.GetInt();
#endif
					if ( iBuyStatus > 0 && ( ( nTeam == TEAM_CT && iBuyStatus != 1 ) || ( nTeam == TEAM_TERRORIST && iBuyStatus != 2 ) ) )
						ClientPrint( this, HUD_PRINTCENTER, "#SFUI_BuyMenu_CantBuy" );
					else
						ClientPrint( this, HUD_PRINTCENTER, "#SFUI_BuyMenu_CantBuyTilNextWave" );
				}
				return false;
			}
		}
	}

	int buyTime = mp_buytime.GetInt();

	if ( mp->IsBuyTimeElapsed() && !CanBuyDuringImmunity() )
	{
		if ( display == true )
		{
			char strBuyTime[16];
			Q_snprintf( strBuyTime, sizeof( strBuyTime ), "%d", buyTime );
			ClientPrint( this, HUD_PRINTCENTER, "#Cant_buy", strBuyTime );
		}

		return false;
	}

	if ( mp->IsWarmupPeriod() && mp->IsPlayingCooperativeGametype() )
		return true;

	if ( mp->IsPlayingCoopMission() )
		return true;

#ifdef GAME_DLL 

	AssertMsg( !m_bIsVIP, " There's no VIP in CSGO. Yet. If implementing, fix this GAME_DLL only code."  );

	if ( m_bIsVIP )
	{
		if ( display == true )
			ClientPrint( this, HUD_PRINTCENTER, "#VIP_cant_buy" );

		return false;
	}
#endif

	return true;
}

bool CCSPlayer::GetUseConfigurationForHighPriorityUseEntity( CBaseEntity *pEntity, CConfigurationForHighPriorityUseEntity_t &cfg )
{
	if ( dynamic_cast<CPlantedC4*>( pEntity ) )
	{
		if ( CSGameRules() && CSGameRules()->IsBombDefuseMap() &&
			( this->GetTeamNumber() == TEAM_CT ) )
		{
			cfg.m_pEntity = pEntity;
		}
		else
		{
			// it's a high-priority entity, but not used by the player team
			cfg.m_pEntity = NULL;
		}
		cfg.m_ePriority = cfg.k_EPriority_Bomb;
		cfg.m_eDistanceCheckType = cfg.k_EDistanceCheckType_2D;
		cfg.m_pos = pEntity->GetAbsOrigin() + Vector( 0, 0, 3 );
		cfg.m_flMaxUseDistance = 62;		// Cannot use if > 62 units away
		cfg.m_flLosCheckDistance = 36;		// Check LOS if > 36 units away (2D)
		cfg.m_flDotCheckAngle = -0.7;		// 0.7 taken from Goldsrc, +/- ~45 degrees
		cfg.m_flDotCheckAngleMax = -0.5;	// 0.3 for it going outside the range during continuous use (120-degree cone)
		return true;
	}
	else if ( dynamic_cast<CHostage*>( pEntity ) )
	{
		cfg.m_pEntity = pEntity;
		cfg.m_ePriority = cfg.k_EPriority_Hostage;
		cfg.m_eDistanceCheckType = cfg.k_EDistanceCheckType_3D;
		cfg.m_pos = pEntity->EyePosition();
		cfg.m_flMaxUseDistance = 62;		// Cannot use if > 62 units away
		cfg.m_flLosCheckDistance = 32;		// Check LOS if > 32 units away (2D)
		cfg.m_flDotCheckAngle = -0.7;		// 0.7 taken from Goldsrc, +/- ~45 degrees
		cfg.m_flDotCheckAngleMax = -0.5;	// 0.5 for it going outside the range during continuous use (120-degree cone)
		return true;
	}
	return false;
}

CBaseEntity *CCSPlayer::GetUsableHighPriorityEntity( void )
{
	// This is done separately since there might be something blocking our LOS to it
	// but we might want to use it anyway if it's close enough.  This should eliminate
	// the vast majority of bomb placement exploits (places where the bomb can be planted
	// but can't be "used".  This also mimics goldsrc cstrike behavior.

	CBaseEntity *pEntsNearPlayer[64];
	// 64 is the distance in Goldsrc.  However since Goldsrc did distance from the player's origin and we're doing distance from the player's eye, make the radius a bit bigger.
	int iEntsNearPlayer = UTIL_EntitiesInSphere( pEntsNearPlayer, 64, EyePosition(), 72, FL_OBJECT );
	if( iEntsNearPlayer != 0 )
	{
		CConfigurationForHighPriorityUseEntity_t cfgBestHighPriorityEntity;
		cfgBestHighPriorityEntity.m_pEntity = NULL;
		cfgBestHighPriorityEntity.m_ePriority = cfgBestHighPriorityEntity.k_EPriority_Default;

		for( int i = 0; i != iEntsNearPlayer; ++i )
		{
			CBaseEntity *pEntity = pEntsNearPlayer[i];
			Assert( pEntity != NULL );
			CConfigurationForHighPriorityUseEntity_t cfgUseSettings;
			if ( !GetUseConfigurationForHighPriorityUseEntity( pEntity, cfgUseSettings ) )
				continue; // not a high-priority entity
			if ( !cfgUseSettings.m_pEntity )
				continue; // not used by the player
			if ( cfgUseSettings.m_ePriority < cfgBestHighPriorityEntity.m_ePriority )
				continue; // we already have a higher priority entity
			
			if ( !cfgUseSettings.UseByPlayerNow( this, cfgUseSettings.k_EPlayerUseType_Start ) )
				continue; // cannot start use by the player right now
				
			// This high-priority entity passes the checks, remember it as best
			if ( cfgUseSettings.IsBetterForUseThan( cfgBestHighPriorityEntity ) )
				cfgBestHighPriorityEntity = cfgUseSettings;
		}

		return cfgBestHighPriorityEntity.m_pEntity;
	}

	return NULL;
}

bool CConfigurationForHighPriorityUseEntity_t::IsBetterForUseThan( CConfigurationForHighPriorityUseEntity_t const &other ) const
{
	if ( !m_pEntity )
		return false;
	if ( !other.m_pEntity )
		return true;
	if ( m_ePriority < other.m_ePriority )
		return false;
	if ( m_ePriority > other.m_ePriority )
		return true;
	if ( m_flDotCheckAngleMax < other.m_flDotCheckAngleMax ) // We are looking at it with a better angle
		return true;
	if ( m_flMaxUseDistance < other.m_flMaxUseDistance ) // This entity is closer to user
		return true;
	return false;
}

bool CConfigurationForHighPriorityUseEntity_t::UseByPlayerNow( CCSPlayer *pPlayer, EPlayerUseType_t ePlayerUseType )
{
	if ( !pPlayer )
		return false;

	// entity is close enough, now make sure the player is facing the bomb.
	float flDistTo = FLT_MAX;
	switch ( m_eDistanceCheckType )
	{
	case k_EDistanceCheckType_2D:
		flDistTo = pPlayer->EyePosition().AsVector2D().DistTo( m_pos.AsVector2D() );
		break;
	case k_EDistanceCheckType_3D:
		flDistTo = pPlayer->EyePosition().DistTo( m_pos );
		break;
	default:
		Assert( false );
	}
	// UTIL_EntitiesInSphere gives strange results where I can find it when my eyes are at an angle, but not when I'm right on top of it
	// because of that, make sure it's in our radius, but check the 2d los and make sure we are as close or closer than we need to be in 1.6
	if ( flDistTo > m_flMaxUseDistance )
		return false;

	// if it's more than 36 units away (2d), we should check LOS
	if ( flDistTo > m_flLosCheckDistance )
	{
		trace_t tr;
		UTIL_TraceLine( pPlayer->EyePosition(), m_pos, (MASK_VISIBLE|CONTENTS_WATER|CONTENTS_SLIME), pPlayer, COLLISION_GROUP_DEBRIS, &tr );
		// if we can't trace to the bomb at this distance, then we fail
		if ( tr.fraction < 0.98 )
			return false;
	}

	Vector vecLOS = pPlayer->EyePosition() - m_pos;
	Vector forward;
	AngleVectors( pPlayer->EyeAngles(), &forward, NULL, NULL );

	vecLOS.NormalizeInPlace();

	float flDot = DotProduct(forward, vecLOS);
	float flCheckAngle = ( ePlayerUseType == k_EPlayerUseType_Start ) ? m_flDotCheckAngle : m_flDotCheckAngleMax;
	if ( flDot >= flCheckAngle )
		return false;

	// Remember the actual settings of this entity
	m_flDotCheckAngle = m_flDotCheckAngleMax = flDot;
	m_flLosCheckDistance = m_flMaxUseDistance = flDistTo;
	return true;
}
ConVar sv_server_verify_blood_on_player( "sv_server_verify_blood_on_player", "1", FCVAR_CHEAT | FCVAR_REPLICATED );

#ifndef CLIENT_DLL
static const int kMaxNumPenetrationsSupported = 4;
struct DelayedDamageInfoData_t
{
	CTakeDamageInfo m_info;
	trace_t m_tr;

	typedef CUtlVectorFixedGrowable< DelayedDamageInfoData_t, kMaxNumPenetrationsSupported > Array;
};
#endif

void CCSPlayer::FireBullet( 
	Vector vecSrc,	// shooting postion
	const QAngle &shootAngles,  //shooting angle
	float flDistance, // max distance 
	float flPenetration, // the power of the penetration
	int nPenetrationCount,
	int iBulletType, // ammo type
	int iDamage, // base damage
	float flRangeModifier, // damage range modifier
	CBaseEntity *pevAttacker, // shooter
	bool bDoEffects,
	float xSpread, float ySpread
	)
{
	float fCurrentDamage = iDamage;   // damage of the bullet at it's current trajectory
	float flCurrentDistance = 0.0;  //distance that the bullet has traveled so far
		
	Vector vecDirShooting, vecRight, vecUp;
	AngleVectors( shootAngles, &vecDirShooting, &vecRight, &vecUp );
	
	// MIKETODO: put all the ammo parameters into a script file and allow for CS-specific params.
	float flPenetrationPower = 0;		// thickness of a wall that this bullet can penetrate
	float flPenetrationDistance = 0;	// distance at which the bullet is capable of penetrating a wall
	float flDamageModifier = 0.5f;		// default modification of bullets power after they go through a wall.
	float flPenetrationModifier = 1.0f;

	GetBulletTypeParameters( iBulletType, flPenetrationPower, flPenetrationDistance );

	// we use the max penetrations on this gun to figure out how much penetration it's capable of
	if ( sv_penetration_type.GetInt() == 1 )
		flPenetrationPower = flPenetration; 

	if ( !pevAttacker )
		pevAttacker = this;  // the default attacker is ourselves

	// add the spray 
	Vector vecDir = vecDirShooting + xSpread * vecRight + ySpread * vecUp;

	VectorNormalize( vecDir );


	//Adrian: visualize server/client player positions
	//This is used to show where the lag compesator thinks the player should be at.
#if 0 
	for ( int k = 1; k <= gpGlobals->maxClients; k++ )
	{
		CBasePlayer *clientClass = (CBasePlayer *)CBaseEntity::Instance( k );

		if ( clientClass == NULL ) 
			 continue;

		if ( k == entindex() ) 
			 continue;

#ifdef CLIENT_DLL
		debugoverlay->AddBoxOverlay( clientClass->GetAbsOrigin(), clientClass->WorldAlignMins(), clientClass->WorldAlignMaxs(), QAngle( 0, 0, 0), 255,0,0,127, 4 );
#else
		NDebugOverlay::Box( clientClass->GetAbsOrigin(), clientClass->WorldAlignMins(), clientClass->WorldAlignMaxs(), 0,0,255,127, 4 );
#endif

	}

#endif

#ifndef CLIENT_DLL
	// [pfreese] Track number player entities killed with this bullet
	int iPenetrationKills = 0;
	int numPlayersHit = 0;

	// [menglish] Increment the shots fired for this player
	CCS_GameStats.Event_ShotFired( this, GetActiveWeapon() );
	m_bulletsFiredSinceLastSpawn++;
	CheckForWeaponFiredAchievement();
#endif

	bool bFirstHit = true;

	const CBaseCombatCharacter *lastPlayerHit = NULL;	// this includes players, bots, and hostages

#ifdef CLIENT_DLL
	Vector vecWallBangHitStart, vecWallBangHitEnd;
	vecWallBangHitStart.Init();
	vecWallBangHitEnd.Init();
	bool bWallBangStarted = false;
	bool bWallBangEnded = false;
	bool bWallBangHeavyVersion = false;
#endif

	bool bBulletHitPlayer = false;

	MDLCACHE_CRITICAL_SECTION();

#ifndef CLIENT_DLL
	DelayedDamageInfoData_t::Array arrPendingDamage;
#endif

	bool bShotHitTeammate = false;

	float flDist_aim = 0;
	Vector vHitLocation = Vector( 0,0,0 );

	while ( fCurrentDamage > 0 )
	{
		Vector vecEnd = vecSrc + vecDir * (flDistance-flCurrentDistance);

		trace_t tr; // main enter bullet trace

		UTIL_TraceLineIgnoreTwoEntities( vecSrc, vecEnd, CS_MASK_SHOOT|CONTENTS_HITBOX, this, lastPlayerHit, COLLISION_GROUP_NONE, &tr );
		{
			CTraceFilterSkipTwoEntities filter( this, lastPlayerHit, COLLISION_GROUP_NONE );

			// Check for player hitboxes extending outside their collision bounds
			const float rayExtension = 40.0f;
			UTIL_ClipTraceToPlayers( vecSrc, vecEnd + vecDir * rayExtension, CS_MASK_SHOOT|CONTENTS_HITBOX, &filter, &tr );
		}

		if ( !flDist_aim )
		{
			flDist_aim = ( tr.fraction != 1.0 ) ? ( tr.startpos - tr.endpos ).Length() : 0;
		}

		if ( flDist_aim )
		{
			vHitLocation = tr.endpos;
		}

		lastPlayerHit = dynamic_cast<const CBaseCombatCharacter *>(tr.m_pEnt);

#ifndef CLIENT_DLL
		if ( sv_showbullethits.GetInt() == 1 && !lastPlayerHit )
		{
			trace_t tr_bulletmiss;
			UTIL_TraceLine( vecSrc, vecEnd, CS_MASK_SHOOT, this, COLLISION_GROUP_NONE, &tr_bulletmiss );

			CCSPlayer *playerMissed = ToCSPlayer( tr_bulletmiss.m_pEnt );
			if ( tr_bulletmiss.DidHit() && !tr_bulletmiss.startsolid && playerMissed )
			{
				Vector vecPelvisPos;
				QAngle angTemp;
				playerMissed->GetBonePosition( 0, vecPelvisPos, angTemp );

				Vector vecMissPos;
				CalcClosestPointOnLine( vecPelvisPos, vecSrc, vecEnd, vecMissPos );

				CStudioHdr *pStudioHdr = playerMissed->GetModelPtr();
				mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( playerMissed->m_nHitboxSet );

				float flClosestHitboxDistance = FLT_MAX;
				Vector vecClosestHitboxPos = vecMissPos;
				int nClosestHitboxIndex = -1;

				Vector vecPos;
				QAngle angAng;
				for ( int n = 0; n < set->numhitboxes; n++ )
				{
					mstudiobbox_t *pbox = set->pHitbox( n );
					playerMissed->GetHitboxBonePosition( pbox->bone, vecPos, angAng, pbox->angOffsetOrientation );

					float flDist = vecPos.DistToSqr( tr_bulletmiss.endpos );

					if ( flDist < flClosestHitboxDistance )
					{
						flClosestHitboxDistance = flDist;
						vecClosestHitboxPos = vecPos;
						nClosestHitboxIndex = n;
					}
				}

				if ( nClosestHitboxIndex > -1 )
				{
					CalcClosestPointOnLine( vecClosestHitboxPos, vecSrc, vecEnd, vecMissPos );

					Vector vecMissDir = (vecEnd - vecSrc).Normalized();
					vecMissPos += vecMissDir * 15;

					int nBoneIndex = playerMissed->GetHitboxBone( nClosestHitboxIndex );					

					// build a matrix from the trace hit start and end position
					matrix3x4_t matWorldSpaceBulletHit;
					VectorMatrix( vecSrc - vecEnd, matWorldSpaceBulletHit );
					PositionMatrix( vecMissPos, matWorldSpaceBulletHit );
				
					// get the transform of the bone that owns the hitbox
					matrix3x4_t matBoneToWorldTransform;
					playerMissed->GetBoneTransform( nBoneIndex, matBoneToWorldTransform );

					// get the local transform of the hit transform relative to the bone transform
					matrix3x4_t matHitLocal;
					MatrixInvert( matBoneToWorldTransform, matHitLocal );
					MatrixMultiply( matHitLocal, matWorldSpaceBulletHit, matHitLocal );

					Vector vecPosTemp;
					QAngle angAngTemp;
					MatrixAngles( matHitLocal, angAngTemp, vecPosTemp );

					IGameEvent * bullet_hit_marker_event = gameeventmanager->CreateEvent( "add_bullet_hit_marker" );
					if ( bullet_hit_marker_event )
					{
						Vector vecBHitStart = vecSrc - (vecUp * 4) + (vecRight * 5);
						bullet_hit_marker_event->SetInt( "userid", playerMissed->GetUserID() );
						bullet_hit_marker_event->SetInt( "bone", nBoneIndex );
						bullet_hit_marker_event->SetFloat( "pos_x", vecPosTemp.x );
						bullet_hit_marker_event->SetFloat( "pos_y", vecPosTemp.y );
						bullet_hit_marker_event->SetFloat( "pos_z", vecPosTemp.z );
						bullet_hit_marker_event->SetFloat( "ang_x", angAngTemp.x );
						bullet_hit_marker_event->SetFloat( "ang_y", angAngTemp.y );
						bullet_hit_marker_event->SetFloat( "ang_z", angAngTemp.z );
						bullet_hit_marker_event->SetFloat( "start_x", vecBHitStart.x );
						bullet_hit_marker_event->SetFloat( "start_y", vecBHitStart.y );
						bullet_hit_marker_event->SetFloat( "start_z", vecBHitStart.z );
						bullet_hit_marker_event->SetBool( "hit", false );
						gameeventmanager->FireEvent( bullet_hit_marker_event );
					}
				}
			}
		}
#endif

		if ( lastPlayerHit )
		{
			if ( lastPlayerHit->GetTeamNumber() == GetTeamNumber() )
			{
				bShotHitTeammate = true;
			}

			bBulletHitPlayer = true;
		}

		if ( tr.fraction == 1.0f )
			break; // we didn't hit anything, stop tracing shoot

#ifdef CLIENT_DLL
		if ( !bWallBangStarted && !bBulletHitPlayer )
		{
			vecWallBangHitStart = tr.endpos;
			vecWallBangHitEnd = tr.endpos;
			bWallBangStarted = true;

			if ( fCurrentDamage > 20 )
				bWallBangHeavyVersion = true;
		}
		else if ( !bWallBangEnded )
		{
			vecWallBangHitEnd = tr.endpos;

			if ( bBulletHitPlayer )
				bWallBangEnded = true;
		}
#endif


#if defined( _DEBUG ) && !defined( CLIENT_DLL )	
		if ( bFirstHit )
			AddBulletStat( gpGlobals->realtime, VectorLength( vecSrc-tr.endpos), tr.endpos );
#endif


#ifdef CLIENT_DLL

		////  ACCURACY DEBUG INFO
		//
		if ( bFirstHit )
		{
			extern ConVar cl_weapon_debug_print_accuracy;
			extern ConVar cl_weapon_debug_show_accuracy;
			extern ConVar cl_weapon_debug_show_accuracy_duration;

			if ( ( cl_weapon_debug_print_accuracy.GetBool( ) || cl_weapon_debug_show_accuracy.GetBool( ) ) && this->IsLocalPlayer( ) )
			{
				CWeaponCSBase *weapon = dynamic_cast< CWeaponCSBase * >( GetActiveWeapon( ) );
				//		const CCSWeaponInfo& weaponInfo = weapon->GetCSWpnData();

				//Vector vecDirShooting, vecRight, vecUp;
				//		AngleVectors( GetFinalAimAngle(), &vecDirShooting, &vecRight, &vecUp );

				// 		float flInaccuracyMove = weaponInfo.GetInaccuracyMove( weapon->m_weaponMode, weapon->GetEconItemView() );
				// 		Vector vecDirInaccMove = vecDirShooting + flInaccuracyMove * vecRight + flInaccuracyMove * vecUp;
				// 		VectorNormalize( vecDirInaccMove );
				// 		float flDotInaccMove = DotProduct( vecDirShooting.Normalized(), vecDirInaccMove.Normalized() );
				// 		float flAngleInaccMove = flDotInaccMove < 0.0f ? -acos( flDotInaccMove ) : acos( flDotInaccMove );
				// 		Msg( "Movement Inaccuracy	: %.2f deg.\n",	RAD2DEG( flAngleInaccMove ) );
				// 
				// 		float flInaccuracyStand = weaponInfo.GetInaccuracyStand( weapon->m_weaponMode, weapon->GetEconItemView() );
				// 		Vector vecDirInaccStand = vecDirShooting + flInaccuracyStand * vecRight + flInaccuracyStand * vecUp;
				// 		VectorNormalize( vecDirInaccStand );
				// 		float flDotInaccStand = DotProduct( vecDirShooting.Normalized(), vecDirInaccStand.Normalized() );
				// 		float flAngleInaccStand = flDotInaccStand < 0.0f ? -acos( flDotInaccStand ) : acos( flDotInaccStand );
				// 		Msg( "Standing Inaccuracy	: %.2f deg.\n",	RAD2DEG( flAngleInaccStand ) );
				// 
				// 		float flInaccuracyLadder = weaponInfo.GetInaccuracyLadder( weapon->m_weaponMode, weapon->GetEconItemView() );
				// 		Vector vecDirInaccLadder = vecDirShooting + flInaccuracyLadder * vecRight + flInaccuracyLadder * vecUp;
				// 		VectorNormalize( vecDirInaccLadder );
				// 		float flDotInaccLadder = DotProduct( vecDirShooting.Normalized(), vecDirInaccLadder.Normalized() );
				// 		float flAngleInaccLadder = flDotInaccLadder < 0.0f ? -acos( flDotInaccLadder ) : acos( flDotInaccLadder );
				// 		Msg( "Ladder   Inaccuracy	: %.2f deg.\n",	RAD2DEG( flAngleInaccLadder ) );
				// 
				// 		float flInaccuracyFire = weaponInfo.GetInaccuracyFire( weapon->m_weaponMode, weapon->GetEconItemView() );
				// 		Vector vecDirInaccFire = vecDirShooting + flInaccuracyFire * vecRight + flInaccuracyFire * vecUp;
				// 		VectorNormalize( vecDirInaccFire );
				// 		float flDotInaccFire = DotProduct( vecDirShooting.Normalized(), vecDirInaccFire.Normalized() );
				// 		float flAngleInaccFire = flDotInaccFire < 0.0f ? -acos( flDotInaccFire ) : acos( flDotInaccFire );
				// 		Msg( "Firing   Inaccuracy	: %.2f deg.\n",	RAD2DEG( flAngleInaccFire ) );
				// 
				float fInaccuracy = weapon->GetInaccuracy( );
				// 		Vector vecDirInaccuracy = vecDirShooting + fInaccuracy * vecRight + fInaccuracy * vecUp;
				// 		VectorNormalize( vecDirInaccuracy );
				// 		float flDotInaccuracy = DotProduct( vecDirShooting.Normalized(), vecDirInaccuracy.Normalized() );
				// 		float flAngleInaccuracy = flDotInaccuracy < 0.0f ? -acos( flDotInaccuracy ) : acos( flDotInaccuracy );
				// 
				float fSpread = weapon->GetSpread( );
				// 		Vector vecDirSpread = vecDirShooting + fSpread * vecRight + fSpread * vecUp;
				// 		VectorNormalize( vecDirSpread );
				// 		float flDotSpread = DotProduct( vecDirShooting.Normalized(), vecDirSpread.Normalized() );
				// 		float flAngleSpread = flDotSpread < 0.0f ? -acos( flDotSpread ) : acos( flDotSpread );
				// 		Msg( "Spread				: %.2f deg.\n",	RAD2DEG( flAngleSpread ) );

				const float kAccurateRadius = 0.5f * 12; // 12 inch dinner plate

				float fFinalInaccuracy = fInaccuracy + fSpread;

				// Calculate effective range:              
				//                                   ----| -
				//                              -----    | ^
				//                         -----         | |
				//                    -----              | accurateRadius
				//               -----                   | |   
				//          ----|                        | |    vecUp * (accurateradius / inaccuracy)
				//     -----    | vecUp * inaccuracy     | v          * inaccuracy
				//  ------------|------------------------| -    = vecUp * accurateRadius
				//     vecDirShooting
				//  |<--- 1 --->|
				//  |<-------- effective range --------->| =  accurateradius / inaccuracy
				float flEffectiveRange = fFinalInaccuracy > 0.00001f ? (kAccurateRadius / fFinalInaccuracy) : 1000000.0f;

				// float flAngleInaccFinal = atanf(fFinalInaccuracy);
				//Msg( "Inaccuracy			: %.2f deg.\n", RAD2DEG( flAngleInaccFinal ) );

				if ( cl_weapon_debug_show_accuracy.GetInt( ) == 1 )	// head sized circle at effective range
				{
					NDebugOverlay::Line( vecSrc, vecSrc + ( flEffectiveRange * vecDirShooting.Normalized( ) ), 100, 100, 100, false, cl_weapon_debug_show_accuracy_duration.GetFloat( ) );

					// shift the color deeper into the recoil sequence.

					int nColorFromRecoilIndex = Max( 255 - ( (int)weapon->m_flRecoilIndex * 20 ), 0 );

					NDebugOverlay::Circle( vecSrc + ( flEffectiveRange * vecDirShooting.Normalized( ) ), kAccurateRadius/*inches radius*/, 255 /*r*/, nColorFromRecoilIndex /*g*/, nColorFromRecoilIndex /*b*/, 255 /*a*/, false /*no depth test*/, cl_weapon_debug_show_accuracy_duration.GetFloat( ) /*duration*/ );
				}
				else if ( cl_weapon_debug_show_accuracy.GetInt( ) == 2 ) // size of inaccuracy on surface
				{
					int nColorFromRecoilIndex = Max( 255 - ( ( int )weapon->m_flRecoilIndex * 20 ), 0 );

					// trace the shot as intended without inaccuracy

					Vector vecEnd = vecSrc + vecDirShooting.Normalized( ) * flDistance;

					trace_t tr_intended;
					UTIL_TraceLine( vecSrc, vecEnd, CS_MASK_SHOOT|CONTENTS_HITBOX, this, COLLISION_GROUP_NONE, &tr_intended );

					NDebugOverlay::Line( vecSrc, tr_intended.endpos, 100, 100, 100, false, cl_weapon_debug_show_accuracy_duration.GetFloat( ) );

					if ( tr_intended.fraction != 1.0 )
					{
						float flRadius = fFinalInaccuracy * vecSrc.DistTo( tr_intended.endpos );

						NDebugOverlay::Circle( tr_intended.endpos, flRadius /*inches radius*/, 255 /*r*/, nColorFromRecoilIndex /*g*/, nColorFromRecoilIndex /*b*/, 255 /*a*/, true /*no depth test*/, cl_weapon_debug_show_accuracy_duration.GetFloat( ) /*duration*/ );
					}
				}

				// 		// trace actual shot
				// 		Vector vecEndShot = vecSrc + vecDir * flDistance;
				// 		trace_t tr_actual;
				// 		UTIL_TraceLineIgnoreTwoEntities( vecSrc, vecEndShot, CS_MASK_SHOOT | CONTENTS_HITBOX, this, NULL, COLLISION_GROUP_NONE, &tr_actual );
				// 		{
				// 			CTraceFilterSkipTwoEntities filter( this, lastPlayerHit, COLLISION_GROUP_NONE );
				// 
				// 			// Check for player hitboxes extending outside their collision bounds
				// 			const float rayExtension = 40.0f;
				// 			UTIL_ClipTraceToPlayers( vecSrc, vecEndShot + vecDir * rayExtension, CS_MASK_SHOOT | CONTENTS_HITBOX, &filter, &tr_actual );
				// 		}
				//		float flDist_actual = ( tr_aim.fraction != 1.0 ) ? ( tr_actual.startpos - tr_actual.endpos ).Length() : 0;

				//		float flMissedDist = ( tr_actual.fraction != 1.0 && tr_aim.fraction != 1.0 ) ? ( tr_aim.endpos - tr_actual.endpos ).Length() : 0;

				//		float flDotShot = DotProduct( vecDirShooting.Normalized(), vecDir.Normalized() );
				//		float flAngleShot = flDotShot < 0.0f ? -acos( flDotShot ) : acos( flDotShot );

				//		Msg(						"-- Shot info --\n" );

				if ( cl_weapon_debug_print_accuracy.GetInt( ) == 1 )
				{
					Msg( "\nAccurate range:		%.2fm", flEffectiveRange * 0.0254 );
					if ( flDist_aim ) Msg( "					Distance to target:		%.2fm\n", flDist_aim * 0.0254 );
				}
				else if ( cl_weapon_debug_print_accuracy.GetInt( ) == 2 )
				{
					if ( weapon->m_flRecoilIndex == 0 )
					{
						Msg( "\ntime	bullet	range	recovery	inaccuracy\n" );
					}

					Msg( "%.5f	%d	%.5f	%.10f	%.10f\n", gpGlobals->curtime, (int)weapon->m_flRecoilIndex, flEffectiveRange * 0.0254, weapon->GetRecoveryTime( ), fFinalInaccuracy );
				}

				//		if ( flDist_actual ) Msg(	"Actual shot distance:  %.0f\"  (%.2f\')\n", flDist_actual, flDist_actual / 12.0 );
				//		if ( flMissedDist ) Msg(	"Missed by:             %.0f\"  (%.2f\')\n", flMissedDist, flMissedDist / 12.0 );

				//		Msg( "\n" );
				//		Msg( "Nominal inaccuracy:       %.2f degrees\n", RAD2DEG( flAngleInaccFinal ) );
				//		Msg( "Actual shot inaccuracy:   %.2f degrees\n", RAD2DEG( flAngleShot ) );

			}
		}
#endif // CLIENT_DLL



		bFirstHit = false;
#ifndef CLIENT_DLL
		//
		// Propogate a bullet impact event
		// @todo Add this for shotgun pellets (which dont go thru here)
		//
		IGameEvent * event = gameeventmanager->CreateEvent( "bullet_impact" );
		if ( event )
		{
			event->SetInt( "userid", GetUserID() );
			event->SetFloat( "x", tr.endpos.x );
			event->SetFloat( "y", tr.endpos.y );
			event->SetFloat( "z", tr.endpos.z );
			gameeventmanager->FireEvent( event );
		}
#endif
		FirePerfStatsEvent( PERF_STATS_BULLET ); // client-only event

		/************* MATERIAL DETECTION ***********/
		surfacedata_t *pSurfaceData = physprops->GetSurfaceData( tr.surface.surfaceProps );
		int iEnterMaterial = pSurfaceData->game.material;

		flPenetrationModifier = pSurfaceData->game.penetrationModifier;
		flDamageModifier = pSurfaceData->game.damageModifier;

		bool hitGrate = ( tr.contents & CONTENTS_GRATE ) != 0;


#ifdef CLIENT_DLL
		if ( sv_showimpacts.GetInt() == 1 || sv_showimpacts.GetInt() == 2 )
		{
			// draw red client impact markers
			debugoverlay->AddBoxOverlay( tr.endpos, Vector(-2,-2,-2), Vector(2,2,2), QAngle( 0, 0, 0), 255,0,0,127, sv_showimpacts_time.GetFloat() );
		}

		// bullet registration recording ( client )
		// This code allowed us to measure discrepency between client and server bullet hits.
		// It became obsolete when we started using a separate seed for client and server
		// to eliminate 'rage' hacks.
		//
		if ( this == C_CSPlayer::GetLocalCSPlayer())
		{
			m_vecBulletVerifyListClient.AddToTail( clientHitVerify_t( tr.endpos, gpGlobals->realtime, gpGlobals->curtime + 10.0f ) );
		}
		

#else

		// bullet registration recording ( server )
		// This code allowed us to measure discrepency between client and server bullet hits.
		// It became obsolete when we started using a separate seed for client and server
		// to eliminate 'rage' hacks.
		//
		CSingleUserRecipientFilter user( this );

		CCSUsrMsg_ReportHit msg;
		msg.set_pos_x( tr.endpos.x );
		msg.set_pos_y( tr.endpos.y );
		msg.set_pos_z( tr.endpos.z );
		msg.set_timestamp( gpGlobals->realtime );

		// only compare shots that were server hits
		if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() )
		{
			SendUserMessage( user, CS_UM_ReportHit, msg );
		}
		
		// end bullet registration recording

		if ( sv_showbullethits.GetInt() == 1 && tr.m_pEnt && tr.m_pEnt->IsPlayer() )
		{
			CCSPlayer *pPlayer = ToCSPlayer( tr.m_pEnt );
			IGameEvent * bullet_hit_marker_event = gameeventmanager->CreateEvent( "add_bullet_hit_marker" );
			if ( bullet_hit_marker_event )
			{
				Vector vecHitPos;
				QAngle angHitAng;
				int nBoneIndex;

				pPlayer->GetBulletHitLocalBoneOffset( tr, nBoneIndex, vecHitPos, angHitAng );

				Vector vecBHitStart = vecSrc - (vecUp * 4) + (vecRight * 5);
				bullet_hit_marker_event->SetInt( "userid", pPlayer->GetUserID() );
				bullet_hit_marker_event->SetInt( "bone", nBoneIndex );
				bullet_hit_marker_event->SetFloat( "pos_x", vecHitPos.x );
				bullet_hit_marker_event->SetFloat( "pos_y", vecHitPos.y );
				bullet_hit_marker_event->SetFloat( "pos_z", vecHitPos.z );
				bullet_hit_marker_event->SetFloat( "ang_x", angHitAng.x );
				bullet_hit_marker_event->SetFloat( "ang_y", angHitAng.y );
				bullet_hit_marker_event->SetFloat( "ang_z", angHitAng.z );
				bullet_hit_marker_event->SetFloat( "start_x", vecBHitStart.x );
				bullet_hit_marker_event->SetFloat( "start_y", vecBHitStart.y );
				bullet_hit_marker_event->SetFloat( "start_z", vecBHitStart.z );
				bullet_hit_marker_event->SetBool( "hit", true );

				gameeventmanager->FireEvent( bullet_hit_marker_event );
			}
		}

		if ( sv_showimpacts.GetInt() == 1 || sv_showimpacts.GetInt() == 3 )
		{
			// draw blue server impact markers
			NDebugOverlay::Box( tr.endpos, Vector(-2,-2,-2), Vector(2,2,2), 0,0,255,127, sv_showimpacts_time.GetFloat() );
		}
#endif

		// client-server hit comparison.
		if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() && !IsControllingBot()  )
		{
#ifndef CLIENT_DLL
			if ( m_totalHitsOnServer < 255 ) // clamp at 8 bits
				m_totalHitsOnServer++;
#else
			if ( m_totalHitsOnClient < 255 ) // clamp at 8 bits
				m_totalHitsOnClient++;
#endif
		}

		// draw green boxes where the shot originated from
		//NDebugOverlay::Box( vecSrc, Vector(-1,-1,-1), Vector(1,1,1), 0,255,90,90, 10 );

		//calculate the damage based on the distance the bullet travelled.
		flCurrentDistance += tr.fraction * (flDistance-flCurrentDistance);
		fCurrentDamage *= pow (flRangeModifier, (flCurrentDistance / 500));

#ifndef CLIENT_DLL
		// the value of iPenetration when the round reached its max penetration distance
		int nPenetrationAtMaxDistance = 0;
		// save off how many penetrations this bullet had in case we reached max distance and stomp the value later
		int const numPenetrationsInitiallyAllowedForThisBullet = nPenetrationCount;
#endif

		// check if we reach penetration distance, no more penetrations after that
		// or if our modifyer is super low, just stop the bullet
		if ( (flCurrentDistance > flPenetrationDistance && flPenetration > 0 ) ||
			flPenetrationModifier < 0.1 )
		{
#ifndef CLIENT_DLL
			nPenetrationAtMaxDistance = 0;
#endif
			// Setting nPenetrationCount to zero prevents the bullet from penetrating object at max distance
			// and will no longer trace beyond the exit point, however "numPenetrationsInitiallyAllowedForThisBullet"
			// is saved off to allow correct determination whether the hit on the object at max distance had
			// *previously* penetrated anything or not. In case of a direct hit over 3000 units the saved off
			// value would be max penetrations value and will determine a direct hit and not a penetration hit.
			// However it is important that all tracing further stops past this point (as the code does at
			// the time of writing) because otherwise next trace will think that 4 penetrations have already
			// occurred.
			nPenetrationCount = 0;
		}

#ifndef CLIENT_DLL
		// This just keeps track of sounds for AIs (it doesn't play anything).
		CSoundEnt::InsertSound( SOUND_BULLET_IMPACT, tr.endpos, 400, 0.2f, this );
#endif

		int iDamageType = DMG_BULLET | DMG_NEVERGIB;
		CWeaponCSBase* pActiveWeapon = GetActiveCSWeapon();
		if ( pActiveWeapon && pActiveWeapon->IsA( WEAPON_TASER ) )
		{
			iDamageType = DMG_SHOCK | DMG_NEVERGIB;
		}

		if( bDoEffects )
		{
			// See if the bullet ended up underwater + started out of the water
			if ( enginetrace->GetPointContents( tr.endpos, MASK_WATER ) & (CONTENTS_WATER|CONTENTS_SLIME) )
			{	
				trace_t waterTrace;
				UTIL_TraceLine( vecSrc, tr.endpos, (MASK_SHOT|CONTENTS_WATER|CONTENTS_SLIME), this, COLLISION_GROUP_NONE, &waterTrace );
				
				if( waterTrace.allsolid != 1 )
				{
					CEffectData	data;
 					data.m_vOrigin = waterTrace.endpos;
					data.m_vNormal = waterTrace.plane.normal;
					data.m_flScale = random->RandomFloat( 8, 12 );

					if ( waterTrace.contents & CONTENTS_SLIME )
					{
						data.m_fFlags |= FX_WATER_IN_SLIME;
					}

					DispatchEffect( "gunshotsplash", data );
				}
			}
			else
			{
				//Do Regular hit effects

				// Don't decal nodraw surfaces
				if ( !( tr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) ) )
				{
					//CBaseEntity *pEntity = tr.m_pEnt;
					UTIL_ImpactTrace( &tr, iDamageType );
				}
			}
		}

#ifndef CLIENT_DLL
		// decal players on the server to eliminate the disparity between where the client thinks the decal went and where it actually went
		// we want to eliminate the case where a player sees a blood decal on someone, but they are at 100 health
		if ( sv_server_verify_blood_on_player.GetBool() && tr.DidHit() && tr.m_pEnt && tr.m_pEnt->IsPlayer() )
		{
			UTIL_ImpactTrace( &tr, iDamageType );
		}
#endif

#ifdef CLIENT_DLL
		// create the tracer
		CreateWeaponTracer( vecSrc, tr.endpos );
#endif

		// add damage to entity that we hit
		
#ifndef CLIENT_DLL
		CBaseEntity *pEntity = tr.m_pEnt;	

	//
	// DAMAGE MUST BE DEFERRED TILL LATER IF WE DECIDE TO SHIP IT
	//
// 		if ( sv_shoot_dropped_grenades.GetBool() )
// 		{
// 			CBaseCSGrenade* pWeapon = dynamic_cast<CBaseCSGrenade*>( pEntity );
			//Only detonate shot grenades if they have been dropped in the world longer than the grace period.
			//This prevents shooting at players and they miraculously explode - because you shot their grenade the instant they died
// 			if ( pWeapon && gpGlobals->curtime > (pWeapon->m_flDroppedAtTime + sv_shoot_dropped_grenades_grace_time.GetFloat()) )
// 			{
// 				pWeapon->ShotDetonate( this, pWeapon->GetCSWpnData() );
// 				pWeapon->AddSolidFlags( FSOLID_NOT_SOLID );
// 				pWeapon->AddEffects( EF_NODRAW );
// 				UTIL_Remove( pWeapon );
// 			}
// 		}
		// [pfreese] Check if enemy players were killed by this bullet, and if so,
		// add them to the iPenetrationKills count

		DelayedDamageInfoData_t &delayedDamage = arrPendingDamage.Element( arrPendingDamage.AddToTail() );
		delayedDamage.m_tr = tr;

		int nObjectsPenetrated = kMaxNumPenetrationsSupported - ( numPenetrationsInitiallyAllowedForThisBullet + nPenetrationAtMaxDistance );
		CTakeDamageInfo &info = delayedDamage.m_info;
		info.Set( pevAttacker, pevAttacker, GetActiveWeapon(), fCurrentDamage, iDamageType, 0, nObjectsPenetrated );

		// [dkorus] note:  This is the number of players hit up to this point, not the total number this bullet WILL hit.
		info.SetDamagedOtherPlayers( numPlayersHit );

		// Set the bullet ID so that we can later track all the enemies that are damage by the same bullet
		info.SetBulletID( GetBulletGroup(), pActiveWeapon ? (int)pActiveWeapon->m_flRecoilIndex : 0 );
		info.SetAmmoType( iBulletType );
		CalculateBulletDamageForce( &info, iBulletType, vecDir, tr.endpos );

		bool bWasAlive = pEntity->IsAlive();

		// === Damage applied later ===

		if ( bWasAlive && pEntity->IsPlayer() && IsOtherEnemy( pEntity->entindex() ) )
		{
			numPlayersHit++;
		}

		if ( sv_showimpacts_penetration.GetInt() > 0 )
		{
			char text[4];
			Q_snprintf( text, sizeof( text ), "^" );
			char text2[32];
			Q_snprintf( text2, sizeof( text2 ), "%s%d", ( sv_showimpacts_penetration.GetInt() == 2 ) ? "" : "DAMAGE APPLIED:  ", (int)ceil(fCurrentDamage) );
			char text3[32];
			// convert to meters
			//(100%% of shots will fall within a 30cm circle.)
			float flDistMeters = ( flCurrentDistance*0.0254 );
			if ( flDistMeters >= 1.0 )
				Q_snprintf( text3, sizeof( text3 ), "%s%0.1fm", ( sv_showimpacts_penetration.GetInt() == 2 ) ? "" : "TOTAL DISTANCE:  ", flDistMeters );
			else
				Q_snprintf( text3, sizeof( text3 ), "%s%0.1fcm", ( sv_showimpacts_penetration.GetInt() == 2 ) ? "" : "TOTAL DISTANCE:  ", flDistMeters/0.01 );

			Vector textPos = tr.endpos;

			NDebugOverlay::EntityTextAtPosition( textPos, 1, text, sv_showimpacts_time.GetFloat(), 225, 128, 64, 255 );
			NDebugOverlay::EntityTextAtPosition( textPos, 2, text2, sv_showimpacts_time.GetFloat(), 255, 64, 0, 255 );
			NDebugOverlay::EntityTextAtPosition( textPos, 3, text3, sv_showimpacts_time.GetFloat(), 255, 128, 64, 255 );

			NDebugOverlay::Box( tr.endpos, Vector( -0.8, -0.8, -0.8 ), Vector( 0.8, 0.8, 0.8 ), 255, 100, 50, 64, sv_showimpacts_time.GetFloat() );
		}
#endif

		// [dkorus] note: values are changed inside of HandleBulletPenetration
		bool bulletStopped = HandleBulletPenetration( flPenetration, iEnterMaterial, hitGrate, tr, vecDir, pSurfaceData, flPenetrationModifier,
			flDamageModifier, bDoEffects, iDamageType, flPenetrationPower, nPenetrationCount, vecSrc, flDistance,
			flCurrentDistance, fCurrentDamage );

		// [dkorus] bulletStopped is true if the bullet can no longer continue penetrating materials
		if ( bulletStopped )
			break;



	}

#ifndef CLIENT_DLL
	if ( bBulletHitPlayer && !bShotHitTeammate )
	{	// Guarantee that the bullet that hit an enemy trumps the player viewangles
		// that are locked in for the duration of the server simulation ticks
		m_iLockViewanglesTickNumber = gpGlobals->tickcount;
		m_qangLockViewangles = pl.v_angle;
	}
#endif

#ifndef CLIENT_DLL
	FOR_EACH_VEC( arrPendingDamage, idxDamage )
	{
		ClearMultiDamage();

		CTakeDamageInfo &info = arrPendingDamage[idxDamage].m_info;
		trace_t &tr = arrPendingDamage[idxDamage].m_tr;

		CBaseEntity *pEntity = tr.m_pEnt;
		bool bWasAlive = pEntity->IsAlive();

		pEntity->DispatchTraceAttack( info, vecDir, &tr );
		TraceAttackToTriggers( info, tr.startpos, tr.endpos, vecDir );

		ApplyMultiDamage();

		if ( bWasAlive && !pEntity->IsAlive() && pEntity->IsPlayer() && IsOtherEnemy( pEntity->entindex() ) )
		{
			++iPenetrationKills;
		}
	}
#endif

#ifdef CLIENT_DLL
	if ( bWallBangStarted )
	{
		float flWallBangLength = (vecWallBangHitEnd - vecWallBangHitStart).Length();
		if ( flWallBangLength > 0 && flWallBangLength < CS_MAX_WALLBANG_TRAIL_LENGTH )
		{
			QAngle temp;
			VectorAngles( vecWallBangHitEnd - vecWallBangHitStart, temp );

			CEffectData	data;
 			data.m_vOrigin = vecWallBangHitStart;
			data.m_vStart = vecWallBangHitEnd;
			data.m_vAngles = temp;
			//data.m_vNormal = vecWallBangHitStart - vecWallBangHitEnd;
			data.m_flScale = 1.0f;
			
			//why is particle system index stored on m_nHitBox?
			if ( bWallBangHeavyVersion )
			{
				data.m_nHitBox = GetParticleSystemIndex( "impact_wallbang_heavy" );
			}
			else
			{
				data.m_nHitBox = GetParticleSystemIndex( "impact_wallbang_light" );
			}

			StartParticleEffect( data );

			//debugoverlay->AddLineOverlay( vecWallBangHitStart, vecWallBangHitEnd, 0, 255, 0, false, 3 );
		}
	}
#endif

#ifndef CLIENT_DLL
	// [pfreese] If we killed at least two enemies with a single bullet, award the
	// TWO_WITH_ONE_SHOT achievement

	if ( iPenetrationKills >= 2 )
	{
		AwardAchievement( CSKillTwoWithOneShot );
	}
#endif


}



// [dkorus] helper for FireBullet
//			changes iPenetration to updated value
//			returns TRUE if we should stop processing more hits after this one
//			returns FALSE if we can continue processing
bool CCSPlayer::HandleBulletPenetration( float &flPenetration,
										 int &iEnterMaterial,
										 bool &hitGrate,
										 trace_t &tr,
										 Vector &vecDir,
										 surfacedata_t *pSurfaceData,
										 float flPenetrationModifier,
										 float flDamageModifier,
										 bool bDoEffects,
										 int iDamageType,
										 float flPenetrationPower,
										 int &nPenetrationCount,
										 Vector &vecSrc,
										 float flDistance,
										 float flCurrentDistance,
										 float &fCurrentDamage)
{
	bool bIsNodraw = !!( tr.surface.flags & (SURF_NODRAW) );

	bool bFailedPenetrate = false;

	// check if bullet can penetrarte another entity
	if ( nPenetrationCount == 0 && !hitGrate && !bIsNodraw 
		 && iEnterMaterial != CHAR_TEX_GLASS && iEnterMaterial != CHAR_TEX_GRATE )
		bFailedPenetrate = true; // no, stop

	// If we hit a grate with iPenetration == 0, stop on the next thing we hit
	if ( flPenetration <= 0 || nPenetrationCount <= 0 )
		bFailedPenetrate = true;

	Vector penetrationEnd;

	// find exact penetration exit
	trace_t exitTr;
	if ( !TraceToExit( tr.endpos, vecDir, penetrationEnd, tr, exitTr, 4, MAX_PENETRATION_DISTANCE ) )
	{
		// ended in solid
		if ( (UTIL_PointContents ( tr.endpos, CS_MASK_SHOOT ) & CS_MASK_SHOOT) == 0 )
		{
			bFailedPenetrate = true;
		}
	}
	
	if ( bFailedPenetrate == true )
	{
		float flTraceDistance = VectorLength( penetrationEnd - tr.endpos );

		// this is copy pasted from below, it should probably be its own function
		float flPenMod = MAX( 0, ( 1 / flPenetrationModifier ) );
		float flPercentDamageChunk = fCurrentDamage * 0.15;
		float flDamageLostImpact = flPercentDamageChunk + MAX( 0, ( 3/ flPenetrationPower ) * 1.18 ) * (flPenMod * 2.8);

		float flLostDamageObject = ( ( flPenMod * ( flTraceDistance*flTraceDistance ) ) / 24 );
		float flTotalLostDamage = flDamageLostImpact + flLostDamageObject;
		DisplayPenetrationDebug( tr.endpos, penetrationEnd, flTraceDistance, fCurrentDamage, flDamageLostImpact, flTotalLostDamage, tr.surface.surfaceProps, -100 );
		return true;
	}

	//debugoverlay->AddBoxOverlay( exitTr.endpos, Vector(-1,-1,-1), Vector(1,1,1), QAngle( 0, 0, 0), 255,255,0,127, 400 );

	// get material at exit point
	surfacedata_t *pExitSurfaceData = physprops->GetSurfaceData( exitTr.surface.surfaceProps );
	int iExitMaterial = pExitSurfaceData->game.material;

	// new penetration method
	if ( sv_penetration_type.GetInt() == 1 )
	{
		// percent of total damage lost automatically on impacting a surface
		float flDamLostPercent = 0.16;

		// since some railings in de_inferno are CONTENTS_GRATE but CHAR_TEX_CONCRETE, we'll trust the
		// CONTENTS_GRATE and use a high damage modifier.
		if ( hitGrate || bIsNodraw || iEnterMaterial == CHAR_TEX_GLASS || iEnterMaterial == CHAR_TEX_GRATE )
		{
			// If we're a concrete grate (TOOLS/TOOLSINVISIBLE texture) allow more penetrating power.
			if ( iEnterMaterial == CHAR_TEX_GLASS || iEnterMaterial == CHAR_TEX_GRATE )
			{
				flPenetrationModifier = 3.0f;
				flDamLostPercent = 0.05;
			}
			else
				flPenetrationModifier = 1.0f;

			flDamageModifier = 0.99f;
		}
		else if ( iEnterMaterial == CHAR_TEX_FLESH && ff_damage_reduction_bullets.GetFloat() == 0 
				  && tr.m_pEnt && tr.m_pEnt->IsPlayer() && tr.m_pEnt->GetTeamNumber() == GetTeamNumber() )
		{
			if ( ff_damage_bullet_penetration.GetFloat() == 0 )
			{
				// don't allow penetrating players when FF is off
				flPenetrationModifier = 0;
				return true;
			}

			flPenetrationModifier = ff_damage_bullet_penetration.GetFloat();
			flDamageModifier = ff_damage_bullet_penetration.GetFloat();
		}
		else
		{
			// check the exit material and average the exit and entrace values
			float flExitPenetrationModifier = pExitSurfaceData->game.penetrationModifier;
			float flExitDamageModifier = pExitSurfaceData->game.damageModifier;
			flPenetrationModifier = (flPenetrationModifier + flExitPenetrationModifier)/2;
			flDamageModifier = (flDamageModifier + flExitDamageModifier)/2;
		}

		// if enter & exit point is wood we assume this is 
		// a hollow crate and give a penetration bonus
		if ( iEnterMaterial == iExitMaterial )
		{
			if( iExitMaterial == CHAR_TEX_WOOD || iExitMaterial == CHAR_TEX_CARDBOARD )
			{
				flPenetrationModifier = 3;
			}
			else if ( iExitMaterial == CHAR_TEX_PLASTIC )
			{
				flPenetrationModifier = 2;
			}
		}

		float flTraceDistance = VectorLength( exitTr.endpos - tr.endpos );

		float flPenMod = MAX( 0, ( 1 / flPenetrationModifier ));

		float flPercentDamageChunk = fCurrentDamage * flDamLostPercent;
		float flPenWepMod = flPercentDamageChunk + MAX( 0, ( 3/ flPenetrationPower ) * 1.25 ) * (flPenMod * 3.0);

		float flLostDamageObject = ((flPenMod * (flTraceDistance*flTraceDistance)) / 24);
		float flTotalLostDamage = flPenWepMod + flLostDamageObject;

		if ( sv_showimpacts_penetration.GetInt() > 0 )
		{
			Vector vecStart = tr.endpos;
			Vector vecEnd = penetrationEnd;
			float flTotalTraceDistance = VectorLength( penetrationEnd - tr.endpos );

			DisplayPenetrationDebug( vecStart, vecEnd, flTotalTraceDistance, fCurrentDamage, flPenWepMod, flTotalLostDamage, tr.surface.surfaceProps, exitTr.surface.surfaceProps );
		}
		
		// reduce damage power each time we hit something other than a grate
		fCurrentDamage -= MAX( 0, flTotalLostDamage );

		if ( fCurrentDamage < 1 )
			return true;

		// penetration was successful

		// bullet did penetrate object, exit Decal
		if ( bDoEffects )
		{
			UTIL_ImpactTrace( &exitTr, iDamageType );
		}

	#ifndef CLIENT_DLL
		// decal players on the server to eliminate the disparity between where the client thinks the decal went and where it actually went
		// we want to eliminate the case where a player sees a blood decal on someone, but they are at 100 health
		if ( sv_server_verify_blood_on_player.GetBool() && tr.DidHit() && tr.m_pEnt && tr.m_pEnt->IsPlayer() )
		{
			UTIL_ImpactTrace( &tr, iDamageType );
		}
	#endif

		//setup new start end parameters for successive trace

		//flPenetrationPower -= (flTraceDistance/2) / flPenMod;
		flCurrentDistance += flTraceDistance;

		// NDebugOverlay::Box( exitTr.endpos, Vector(-2,-2,-2), Vector(2,2,2), 0,255,0,127, 8 );

		vecSrc = exitTr.endpos;
		flDistance = (flDistance - flCurrentDistance) * 0.5;

		nPenetrationCount--;
		return false;
	}
	else
	{
		// since some railings in de_inferno are CONTENTS_GRATE but CHAR_TEX_CONCRETE, we'll trust the
		// CONTENTS_GRATE and use a high damage modifier.
		if ( hitGrate || bIsNodraw )
		{
			// If we're a concrete grate (TOOLS/TOOLSINVISIBLE texture) allow more penetrating power.
			flPenetrationModifier = 1.0f;
			flDamageModifier = 0.99f;
		}
		else
		{
			// Check the exit material to see if it is has less penetration than the entrance material.
			float flExitPenetrationModifier = pExitSurfaceData->game.penetrationModifier;
			float flExitDamageModifier = pExitSurfaceData->game.damageModifier;
			if ( flExitPenetrationModifier < flPenetrationModifier )
			{
				flPenetrationModifier = flExitPenetrationModifier;
			}
			if ( flExitDamageModifier < flDamageModifier )
			{
				flDamageModifier = flExitDamageModifier;
			}
		}

		// if enter & exit point is wood or metal we assume this is 
		// a hollow crate or barrel and give a penetration bonus
		if ( iEnterMaterial == iExitMaterial )
		{
			if ( iExitMaterial == CHAR_TEX_WOOD ||
				 iExitMaterial == CHAR_TEX_METAL )
			{
				flPenetrationModifier *= 2;
			}
		}

		float flTraceDistance = VectorLength( exitTr.endpos - tr.endpos );

		// check if bullet has enough power to penetrate this distance for this material
		if ( flTraceDistance > ( flPenetrationPower * flPenetrationModifier ) )
			return true; // bullet hasn't enough power to penetrate this distance

		// reduce damage power each time we hit something other than a grate
		fCurrentDamage *= flDamageModifier;

		// penetration was successful

		// bullet did penetrate object, exit Decal
		if ( bDoEffects )
		{
			UTIL_ImpactTrace( &exitTr, iDamageType );
		}

	#ifndef CLIENT_DLL
		// decal players on the server to eliminate the disparity between where the client thinks the decal went and where it actually went
		// we want to eliminate the case where a player sees a blood decal on someone, but they are at 100 health
		if ( sv_server_verify_blood_on_player.GetBool() && tr.DidHit() && tr.m_pEnt && tr.m_pEnt->IsPlayer() )
		{
			UTIL_ImpactTrace( &tr, iDamageType );
		}
	#endif

		//setup new start end parameters for successive trace

		flPenetrationPower -= flTraceDistance / flPenetrationModifier;
		flCurrentDistance += flTraceDistance;

		// NDebugOverlay::Box( exitTr.endpos, Vector(-2,-2,-2), Vector(2,2,2), 0,255,0,127, 8 );

		vecSrc = exitTr.endpos;
		flDistance = ( flDistance - flCurrentDistance ) * 0.5;

		// reduce penetration counter
		nPenetrationCount--;
		return false;
	}
}

void CCSPlayer::DisplayPenetrationDebug( Vector vecEnter, Vector vecExit, float flDistance, float flInitialDamage, float flDamageLostImpact, float flTotalLostDamage, short nEnterSurf, short nExitSurf )
{
#ifndef CLIENT_DLL
	if ( sv_showimpacts_penetration.GetInt() > 0 )
	{
		Vector vecStart = vecEnter;
		Vector vecEnd = vecExit;
		float flTotalTraceDistance = VectorLength( vecExit - vecEnter );
		//float flEnd = flTotalTraceDistance;
		//short nExitSurf = exitTr.surface.surfaceProps;
		if ( flTotalLostDamage >= flInitialDamage )
		{
			nExitSurf = -100;

			float flLostDamageObject = (flTotalLostDamage - flDamageLostImpact) ;
			//float flLostLinear = sqrt(flDistance * (flLostDamageObject)) * 24;
			float flFrac = MAX( 0, ( flInitialDamage - flDamageLostImpact ) / flLostDamageObject );
			vecEnd = ( vecEnd - vecStart );
			VectorNormalize( vecEnd );
			vecEnd = vecStart + ( vecEnd*flTotalTraceDistance*flFrac );

			if ( flDamageLostImpact >= flInitialDamage )
			{
				flDistance = 0;
				vecStart = vecEnd;
			}

			flTotalLostDamage = ( int )ceil( flInitialDamage );
		}

		Vector textPos = vecEnd;
		
		char text[64];
		if ( flTotalLostDamage < flInitialDamage )
		{
			
			float flDistMeters = ( flDistance*0.0254 );
			if ( flDistMeters >= 1.0 )
				Q_snprintf( text, sizeof( text ), "%s%0.1fm", ( sv_showimpacts_penetration.GetInt() == 2 ) ? "" : "THICKNESS:		", flDistMeters );
			else
				Q_snprintf( text, sizeof( text ), "%s%0.1fcm", ( sv_showimpacts_penetration.GetInt() == 2 ) ? "" : "THICKNESS:		", flDistMeters / 0.01 );

		}
		else
		{
			Q_snprintf( text, sizeof( text ), "%s", "STOPPED!" );
		}
		
		NDebugOverlay::EntityTextAtPosition( textPos, -3, text, sv_showimpacts_time.GetFloat(), 220, 128, 128, 255 );

		char text3[64];
		Q_snprintf( text3, sizeof( text3 ), "%s%0.1f", ( sv_showimpacts_penetration.GetInt() == 2 ) ? "-" : "LOST DAMAGE:		", flTotalLostDamage );

		NDebugOverlay::EntityTextAtPosition( textPos, -2, text3, sv_showimpacts_time.GetFloat(), 90, 22, 0, 160 );

		char textmat1[64];
		Q_snprintf( textmat1, sizeof( textmat1 ), "%s", physprops->GetPropName( nEnterSurf ) );
		NDebugOverlay::EntityTextAtPosition( vecStart, -1, textmat1, sv_showimpacts_time.GetFloat(), 0, 255, 0, 128 );

		if ( nExitSurf != -100 )
		{
			NDebugOverlay::Box( vecStart, Vector( -0.4, -0.4, -0.4 ), Vector( 0.4, 0.4, 0.4 ), 0, 255, 0, 128, sv_showimpacts_time.GetFloat() );		

			char textmat2[64];
			Q_snprintf( textmat2, sizeof( textmat2 ), "%s", ( nExitSurf == -1 ) ? "" : physprops->GetPropName( nExitSurf ) );
			NDebugOverlay::Box( vecEnd, Vector( -0.4, -0.4, -0.4 ), Vector( 0.4, 0.4, 0.4 ), 0, 128, 255, 128, sv_showimpacts_time.GetFloat() );
			NDebugOverlay::EntityTextAtPosition( vecEnd, -1, textmat2, sv_showimpacts_time.GetFloat(), 0, 128, 255, 128 );

			if ( flDistance > 0 && vecStart != vecEnd )
				NDebugOverlay::Line( vecStart, vecEnd, 0, 190, 190, true, sv_showimpacts_time.GetFloat() );
		}
		else
		{
			// different color
			NDebugOverlay::Box( vecStart, Vector( -0.4, -0.4, -0.4 ), Vector( 0.4, 0.4, 0.4 ), 160, 255, 0, 128, sv_showimpacts_time.GetFloat() );
			NDebugOverlay::Line( vecStart, vecEnd, 190, 190, 0, true, sv_showimpacts_time.GetFloat() );
		}	
	}
#endif
}

void CCSPlayer::ImpactTrace( trace_t *pTrace, int iDamageType, char *pCustomImpactName )
{

#ifdef CLIENT_DLL
	if ( sv_server_verify_blood_on_player.GetBool() )
		return;
#endif

	static ConVar *violence_hblood = cvar->FindVar( "violence_hblood" );
	if ( violence_hblood && !violence_hblood->GetBool() )
		return;

	VPROF( "CCSPlayer::ImpactTrace" );
	Assert( pTrace->m_pEnt );

	CBaseEntity *pEntity = pTrace->m_pEnt;

	// Build the impact data
	CEffectData data;
	data.m_vOrigin = pTrace->endpos;
	data.m_vStart = pTrace->startpos;
	data.m_nSurfaceProp = pTrace->surface.surfaceProps;
	if ( data.m_nSurfaceProp < 0 )
	{
		data.m_nSurfaceProp = 0;
	}
	data.m_nDamageType = iDamageType;
	data.m_nHitBox = pTrace->hitbox;
#ifdef CLIENT_DLL
	data.m_hEntity = ClientEntityList().EntIndexToHandle( pEntity->entindex() );
#else
	data.m_nEntIndex = pEntity->entindex();
	data.m_fFlags |= EFFECTDATA_SERVER_IGNOREPREDICTIONCULL;

	if ( sv_server_verify_blood_on_player.GetBool() )
	{
		data.m_vOrigin -= GetAbsOrigin();
		data.m_vStart -= GetAbsOrigin();
		data.m_bPositionsAreRelativeToEntity = true;
	}

#endif

	// Send it on its way
	if ( !pCustomImpactName )
	{
		DispatchEffect( "Impact", data );
	}
	else
	{
		DispatchEffect( pCustomImpactName, data );
	}
}

#ifdef CLIENT_DLL

void TE_DynamicLight( IRecipientFilter& filter, float delay,
	const Vector* org, int r, int g, int b, int exponent, float radius, float time, float decay, int nLightIndex = LIGHT_INDEX_TE_DYNAMIC );

void CCSPlayer::CreateWeaponTracer( Vector vecStart, Vector vecEnd )
{
	int iTracerFreq = 1;
	C_WeaponCSBase *pWeapon = GetActiveCSWeapon();

	if ( pWeapon )
	{
		// if this is a local player, start at attachment on view model
		// else start on attachment on weapon model
		int iEntIndex = entindex();
		int iUseAttachment = TRACER_DONT_USE_ATTACHMENT;
		int iAttachment = 1;

		C_CSPlayer *pLocalPlayer = NULL;
		bool bUseObserverTarget = false;

		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
			if ( !pLocalPlayer )
				continue;

			if ( pLocalPlayer->GetObserverTarget() == this &&
				pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE &&
				!pLocalPlayer->IsInObserverInterpolation() )
			{
				bUseObserverTarget = true;
				break;
			}
		}
		
		C_BaseCombatWeapon *pActiveWeapon = GetActiveWeapon();
		C_BaseViewModel *pViewModel = GetViewModel(0);

		CBaseWeaponWorldModel *pWeaponWorldModel = NULL;
		if ( pActiveWeapon && ( !pViewModel || this->ShouldDraw() ) )
			pWeaponWorldModel = pActiveWeapon->GetWeaponWorldModel();

		if ( pWeaponWorldModel && pWeaponWorldModel->HasDormantOwner() )
		{
			// This is likely a player firing from around a corner, where this client can't see them.
			// Don't modify the tracer start position, since our local world weapon model position is not reliable.
		}
		else if (pWeaponWorldModel)
		{
			iAttachment = pWeaponWorldModel->LookupAttachment( "muzzle_flash" );
			if ( iAttachment > 0 )
				pWeaponWorldModel->GetAttachment( iAttachment, vecStart );
		}
		else if ( pViewModel )
		{
			iAttachment = pViewModel->LookupAttachment( "1" );
			pViewModel->GetAttachment( iAttachment, vecStart );
		}

		// bail if we're at the origin
		if ( vecStart.LengthSqr() <= 0 )
			return;

		// muzzle flash dynamic light
		m_GlowObject.SetGlowAlphaPulseOverdrive( 0.3f );
		CPVSFilter filter( vecStart );
		TE_DynamicLight( filter, 0.0, &vecStart, 255, 192, 64, 5, 70, 0.05, 768 );

		int	nBulletNumber = (pWeapon->GetMaxClip1() - pWeapon->Clip1()) + 1;
		iTracerFreq = pWeapon->GetCSWpnData().GetTracerFrequency( pWeapon->GetEconItemView(), pWeapon->m_weaponMode );
		if ( ( iTracerFreq != 0 ) && ( nBulletNumber % iTracerFreq ) == 0 )
		{
			const char *pszTracerEffect = GetTracerType();
			if ( pszTracerEffect && pszTracerEffect[0] )
			{
				UTIL_ParticleTracer( pszTracerEffect, vecStart, vecEnd, iEntIndex, iUseAttachment, true );
			}
		}
		else
		{
			// just do the whiz sound
			FX_TracerSound( vecStart, vecEnd, TRACER_TYPE_DEFAULT );
		}
		
	}
}
#endif

void CCSPlayer::UpdateStepSound( surfacedata_t *psurface, const Vector &vecOrigin, const Vector &vecVelocity  )
{
	if ( IsBot() && IsDormant() )
		return;

	if (!IsAlive())
		return;

	float speedSqr = vecVelocity.LengthSqr();

	float flWalkSpeed = (CS_PLAYER_SPEED_RUN * CS_PLAYER_SPEED_WALK_MODIFIER);

	if ( ( speedSqr < flWalkSpeed * flWalkSpeed ) || m_bIsWalking )
	{
		if ( speedSqr < 10.0 )
		{
			// If we stop, reset the step sound tracking.
			// This makes step sounds play a consistent time after
			// we start running making it easier to co-ordinate suit and
			// step sounds.
			SetStepSoundTime( STEPSOUNDTIME_NORMAL, false );
		}

		return; // player is not running, no footsteps
	}

	BaseClass::UpdateStepSound( psurface, vecOrigin, vecVelocity  );
}

ConVar weapon_recoil_view_punch_extra( "weapon_recoil_view_punch_extra", "0.055", FCVAR_RELEASE | FCVAR_CHEAT | FCVAR_REPLICATED, "Additional (non-aim) punch added to view from recoil" );

void CCSPlayer::KickBack( float fAngle, float fMagnitude )
{
	QAngle angleVelocity(0,0,0);
	angleVelocity[YAW] = -sinf(DEG2RAD(fAngle)) * fMagnitude;
	angleVelocity[PITCH] = -cosf(DEG2RAD(fAngle)) * fMagnitude;
	angleVelocity += m_Local.m_aimPunchAngleVel.Get();
	SetAimPunchAngleVelocity( angleVelocity );

	// this bit gives additional punch to the view (screen shake) to make the kick back a bit more visceral
	QAngle viewPunch = GetViewPunchAngle();
	float fViewPunchMagnitude = fMagnitude * weapon_recoil_view_punch_extra.GetFloat();
	viewPunch[YAW] -= sinf(DEG2RAD(fAngle)) * fViewPunchMagnitude;
	viewPunch[PITCH] -= cosf(DEG2RAD(fAngle)) * fViewPunchMagnitude;
	SetViewPunchAngle(viewPunch);
}


QAngle CCSPlayer::GetAimPunchAngle()
{

#ifdef CLIENT_DLL
	if (  PlatformInputDevice::IsInputDeviceAPointer( g_pInputSystem->GetCurrentInputDevice() ) )
#else
	if ( PlatformInputDevice::IsInputDeviceAPointer( GetPlayerInputDevice() ) )
#endif
	{
		return m_Local.m_aimPunchAngle.Get() * weapon_recoil_scale_motion_controller.GetFloat();
	}
	else
	{
		return m_Local.m_aimPunchAngle.Get() * weapon_recoil_scale.GetFloat();
	}
}

QAngle CCSPlayer::GetRawAimPunchAngle() const
{
	return m_Local.m_aimPunchAngle.Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CCSPlayer::GetDefaultCrouchedFOV( void ) const
{
#ifdef _GAMECONSOLE
	return GetDefaultFOV() - 5;
#else
	return GetDefaultFOV();
#endif
}

bool CCSPlayer::CanMove() const
{
	// When we're in intro camera mode, it's important to return false here 
	// so our physics object doesn't fall out of the world.
	if ( GetMoveType() == MOVETYPE_NONE )
		return false;

	if ( IsObserver() )
		return true; // observers can move all the time

	bool bValidMoveState = (State_Get() == STATE_ACTIVE || State_Get() == STATE_OBSERVER_MODE);
			
	if ( m_bIsDefusing || m_bIsGrabbingHostage || !bValidMoveState || (CSGameRules()->IsFreezePeriod() && !m_bCanMoveDuringFreezePeriod) )
	{
		return false;
	}
	else
	{
		// Can't move while planting C4.
		CC4 *pC4 = dynamic_cast< CC4* >( GetActiveWeapon() );
		if ( pC4 && pC4->m_bStartedArming )
			return false;

		return true;
	}
}

unsigned int CCSPlayer::PhysicsSolidMaskForEntity( void ) const
{
	if ( !CSGameRules()->IsTeammateSolid() )
	{
		switch ( GetTeamNumber() )
		{
			case TEAM_UNASSIGNED:
				return MASK_PLAYERSOLID;
			case LAST_SHARED_TEAM:
				return MASK_PLAYERSOLID;
			case TEAM_TERRORIST:
				return MASK_PLAYERSOLID | CONTENTS_TEAM1;
			case TEAM_CT:
				return MASK_PLAYERSOLID | CONTENTS_TEAM2;
		}
	}

	return MASK_PLAYERSOLID;
}

void CCSPlayer::OnJump( float fImpulse )
{
	CWeaponCSBase* pActiveWeapon = GetActiveCSWeapon();
	if ( pActiveWeapon != NULL )
		pActiveWeapon->OnJump(fImpulse);
}


void CCSPlayer::OnLand( float fVelocity )
{

	CWeaponCSBase* pActiveWeapon = GetActiveCSWeapon();
	if (pActiveWeapon != NULL)
		pActiveWeapon->OnLand(fVelocity);

	if ( fVelocity > 270 )
	{
		CRecipientFilter filter;

#if defined( CLIENT_DLL )
		filter.AddRecipient( this );

		if ( prediction->InPrediction() )
		{
			// Only use these rules when in prediction.
			filter.UsePredictionRules();
		}
#else
		filter.AddAllPlayers();
		// the client plays it's own sound
		filter.RemoveRecipient( this );
#endif
		
			EmitSound(filter, entindex(), "Default.Land");

			if (!m_pSurfaceData)
				return;

			unsigned short stepSoundName = m_pSurfaceData->sounds.runStepLeft;
			if (!stepSoundName)
				return;

			IPhysicsSurfaceProps *physprops = MoveHelper()->GetSurfaceProps();

			const char *pRawSoundName = physprops->GetString(stepSoundName);

			char szStep[512];

			if (GetTeamNumber() == TEAM_TERRORIST)
			{
				Q_snprintf(szStep, sizeof(szStep), "t_%s", pRawSoundName);
			}
			else
			{
				Q_snprintf(szStep, sizeof(szStep), "ct_%s", pRawSoundName);
			}

		
			EmitSound(filter, entindex(), szStep);
			
	}

	
}


//-------------------------------------------------------------------------------------------------------------------------------
/**
* Track the last time we were on a ladder, along with the ladder's normal and where we
* were grabbing it, so we don't reach behind us and grab it again as we are trying to
* dismount.
*/
void CCSPlayer::SurpressLadderChecks( const Vector& pos, const Vector& normal )
{
	m_ladderSurpressionTimer.Start( 1.0f );
	m_lastLadderPos = pos;
	m_lastLadderNormal = normal;
}


//-------------------------------------------------------------------------------------------------------------------------------
/**
* Prevent us from re-grabbing the same ladder we were just on:
*  - if the timer is elapsed, let us grab again
*  - if the normal is different, let us grab
*  - if the 2D pos is very different, let us grab, since it's probably a different ladder
*/
bool CCSPlayer::CanGrabLadder( const Vector& pos, const Vector& normal )
{
	if ( m_ladderSurpressionTimer.GetRemainingTime() <= 0.0f )
	{
		return true;
	}

	const float MaxDist = 64.0f;
	if ( pos.AsVector2D().DistToSqr( m_lastLadderPos.AsVector2D() ) < MaxDist * MaxDist )
	{
		return false;
	}

	if ( normal != m_lastLadderNormal )
	{
		return true;
	}

	return false;
}


void CCSPlayer::SetAnimation( PLAYER_ANIM playerAnim )
{
	// In CS, its CPlayerAnimState object manages ALL the animation state.
	return;
}


CWeaponCSBase* CCSPlayer::CSAnim_GetActiveWeapon()
{
	return GetActiveCSWeapon();
}


bool CCSPlayer::CSAnim_CanMove()
{
	return CanMove();
}

int CCSPlayer::GetCarryLimit( CSWeaponID weaponId )
{
	const CCSWeaponInfo *pWeaponInfo = GetWeaponInfo( weaponId );
	if ( pWeaponInfo == NULL )
		return 0;

	if ( pWeaponInfo->GetWeaponType() == WEAPONTYPE_GRENADE )
	{
		return GetAmmoDef()->MaxCarry( pWeaponInfo->GetPrimaryAmmoType( ), this );	// We still use player-stored ammo for grenades.
	}

	return 1;
}

AcquireResult::Type CCSPlayer::CanAcquire( CSWeaponID weaponId, AcquireMethod::Type acquireMethod, CEconItemView *pItem )
{
	const CCSWeaponInfo *pWeaponInfo = NULL;
	if ( weaponId == WEAPON_NONE && (pItem == NULL || !pItem->IsValid()) )
		return AcquireResult::InvalidItem;

	if ( pItem && pItem->IsValid() )
	{
		weaponId = WeaponIdFromString( pItem->GetStaticData()->GetItemClass() );
		if ( weaponId == WEAPON_NONE )
			return AcquireResult::InvalidItem;

		pWeaponInfo = GetWeaponInfo( weaponId );
	}
	else
		pWeaponInfo = GetWeaponInfo( weaponId );

	if ( pWeaponInfo == NULL )
		return AcquireResult::InvalidItem;

	AcquireResult::Type nGamerulesResult = CSGameRules()->IsWeaponAllowed( pWeaponInfo, GetTeamNumber(), pItem );
	if ( nGamerulesResult != AcquireResult::Allowed )
	{
		return nGamerulesResult;
	}

	int nType = pWeaponInfo->GetWeaponType( pItem );

// 	if ( acquireMethod == AcquireMethod::Buy )
// 	{
// 		bool bFoundInLoadout = false;
// 		int nStartSearchPos = LOADOUT_POSITION_INVALID;
// 
// 		if ( nType == WEAPONTYPE_PISTOL )
// 			nStartSearchPos = LOADOUT_POSITION_SECONDARY0;
// 		else if ( nType == WEAPONTYPE_SHOTGUN || nType == WEAPONTYPE_MACHINEGUN )
// 			nStartSearchPos = LOADOUT_POSITION_HEAVY0;
// 		else if ( nType == WEAPONTYPE_SUBMACHINEGUN )
// 			nStartSearchPos = LOADOUT_POSITION_SMG0;
// 		else if ( nType == WEAPONTYPE_RIFLE || nType == WEAPONTYPE_SNIPER_RIFLE )
// 			nStartSearchPos = LOADOUT_POSITION_RIFLE0;
// 
// 		// make sure that we have this item equipped in our inventory loadout
// 		for ( int i = nStartSearchPos; i < (nStartSearchPos+6); ++i )
// 		{
// 			CEconItemView *pItemView = Inventory()->GetItemInLoadout( GetTeamNumber(), i );
// 			if ( pItemView && pItemView->GetStaticData() )
// 			{
// 				if ( pItemView == pItem || ( V_stricmp( pItemView->GetStaticData()->GetDefinitionName(), WeaponIdAsString( weaponId ) ) == 0 ) )
// 				{
// 					bFoundInLoadout = true;
// 					break;
// 				}
// 			}
// 		}
// 		if ( nStartSearchPos != LOADOUT_POSITION_INVALID && !bFoundInLoadout )
// 			return AcquireResult::NotAllowedForPurchase;
// 	}

	if ( nType == WEAPONTYPE_GRENADE )
	{
		if ( mp_buy_allow_grenades.GetBool() == false )
		{
			if ( acquireMethod == AcquireMethod::Buy )
				return AcquireResult::NotAllowedForPurchase;
		}

		// make sure we aren't exceeding the ammo max for this grenade type
		int carryLimitThisGrenade = GetCarryLimit( weaponId );

		int carryLimitAllGrenades = ammo_grenade_limit_total.GetInt();

		CBaseCombatWeapon* pGrenadeWeapon = ( pItem && pItem->IsValid() ) ? CSWeapon_OwnsThisType( pItem ) : Weapon_OwnsThisType( WeaponIdAsString( weaponId ) );
		if ( pGrenadeWeapon != NULL )
		{
			int nAmmoType = pGrenadeWeapon->GetPrimaryAmmoType();

			if( nAmmoType != -1 )
			{
				int thisGrenadeCarried = GetAmmoCount(nAmmoType );
				if ( thisGrenadeCarried >= carryLimitThisGrenade )
				{
					return AcquireResult::ReachedGrenadeTypeLimit;
				}
			}
		}

		// count how many grenades of any type the player is currently carrying
		int allGrenadesCarried = 0;
		for ( int i = 0; i < MAX_WEAPONS; ++i )
		{
			CWeaponCSBase* pWeapon = dynamic_cast<CWeaponCSBase*>( GetWeapon( i) );
			if ( pWeapon != NULL && pWeapon->IsKindOf( WEAPONTYPE_GRENADE ) )
			{
				int nAmmoType = pWeapon->GetPrimaryAmmoType();
				if( nAmmoType != -1 )
				{
					allGrenadesCarried += GetAmmoCount( nAmmoType );
				}
			}
		}

		if ( allGrenadesCarried >= carryLimitAllGrenades )
		{
			return AcquireResult::ReachedGrenadeTotalLimit;
		}

		// don't allow players with an inferno spawning weapon to pick up another inferno spawning weapon
		if ( weaponId == WEAPON_INCGRENADE )
		{
			 if ( Weapon_OwnsThisType( "weapon_molotov" ) )
				 return AcquireResult::AlreadyOwned;
		}
		else if ( weaponId == WEAPON_MOLOTOV )
		{
			if ( Weapon_OwnsThisType( "weapon_incgrenade" ) )
				return AcquireResult::AlreadyOwned;
		}
	}
	else if ( nType == WEAPONTYPE_STACKABLEITEM )
	{
		int carryLimit = GetAmmoDef()->MaxCarry( pWeaponInfo->GetPrimaryAmmoType(), this );

		CBaseCombatWeapon* pItemWeapon = ( pItem && pItem->IsValid() ) ? CSWeapon_OwnsThisType( pItem ) : Weapon_OwnsThisType( WeaponIdAsString( weaponId ) );
		if ( pItemWeapon != NULL )
		{
			int nAmmoType = pItemWeapon->GetPrimaryAmmoType();

			if ( nAmmoType != -1 )
			{
				int thisCarried = GetAmmoCount( nAmmoType );
				if ( thisCarried >= carryLimit )
				{
					return AcquireResult::ReachedGrenadeTypeLimit;
				}
			}
		}
	}
	else if ( weaponId == ITEM_KEVLAR )
	{
		if ( mp_free_armor.GetBool() )
		{
			if ( acquireMethod == AcquireMethod::Buy )
				return AcquireResult::NotAllowedForPurchase;
		}

		if ( ArmorValue() >= 100 )
		{
			return AcquireResult::AlreadyOwned;
		}
	}
	else if ( weaponId == ITEM_ASSAULTSUIT )
	{
		if ( mp_free_armor.GetBool() )
		{
			if ( acquireMethod == AcquireMethod::Buy )
				return AcquireResult::NotAllowedForPurchase;
		}

		if ( m_bHasHelmet && !m_bHasHeavyArmor /*&& ArmorValue() >= 100*/ )
		{
			return AcquireResult::AlreadyOwned;
		}
	}
	else if ( weaponId == ITEM_HEAVYASSAULTSUIT )
	{
		if ( m_bHasHeavyArmor && ArmorValue() >= 200 )
		{
			return AcquireResult::AlreadyOwned;
		}
	}
	else if ( weaponId == ITEM_DEFUSER || weaponId == ITEM_CUTTERS )
	{
		if ( CSGameRules() && (CSGameRules()->IsPlayingGunGameDeathmatch() || CSGameRules()->IsPlayingCoopGuardian()) )
		{
			if ( acquireMethod == AcquireMethod::Buy )
				return AcquireResult::NotAllowedForPurchase;
		}

		if ( m_bHasDefuser )
			return AcquireResult::AlreadyOwned;
	}
	else if ( weaponId == WEAPON_C4 )
	{
		// TODO[pmf]: Data drive this from the scripts
		if ( acquireMethod == AcquireMethod::Buy )
			return AcquireResult::NotAllowedForPurchase;
	}
	else if ( CSWeapon_OwnsThisType( pItem ) )	
	{
		return AcquireResult::AlreadyOwned;
	}

	extern ConVar mp_weapons_allow_zeus;
	extern ConVar mp_weapons_allow_typecount;
	// special case for limiting taser to classic casual; data drive this if it becomes more complex
	if ( weaponId == WEAPON_TASER )
	{
		if ( !mp_weapons_allow_zeus.GetBool() )
			return AcquireResult::NotAllowedForPurchase;
		else if ( ( mp_weapons_allow_zeus.GetInt() > 0 ) && ( m_iWeaponPurchasesThisRound[ weaponId ] >= mp_weapons_allow_zeus.GetInt() ) )
			return AcquireResult::AlreadyPurchased;
		else
			return AcquireResult::Allowed;
	}

	// additional constraints for purchasing weapons
	if ( acquireMethod == AcquireMethod::Buy )
	{
		if ( pWeaponInfo->GetUsedByTeam( pItem ) != TEAM_UNASSIGNED && GetTeamNumber() != pWeaponInfo->GetUsedByTeam( pItem ) )
		{
			return AcquireResult::NotAllowedByTeam;
		}

		// special case for flashbangs - no limit
		if ( weaponId == WEAPON_FLASHBANG )
		{
			return AcquireResult::Allowed;
		}

		// don't allow purchasing multiple grenades of a given type per round (even if the player throws the purchased one)
		if ( pWeaponInfo->GetWeaponType() == WEAPONTYPE_GRENADE && !CSGameRules()->IsPlayingCooperativeGametype() )
		{
			// limit the number of purchases to one more than the number we are allowed to carry
			int carryLimitThisGrenade = GetAmmoDef()->MaxCarry( pWeaponInfo->GetPrimaryAmmoType(), this ); // We still use player-stored ammo for grenades.

			// for smoke grenade, we are only allow to buy exactly the amount we are allowed to carry, with other weapons, we can purchase one more than what we can carry per round
			if ( weaponId == WEAPON_SMOKEGRENADE && carryLimitThisGrenade > 0 )
				carryLimitThisGrenade--;

			if ( m_iWeaponPurchasesThisRound[weaponId] > carryLimitThisGrenade )
				return AcquireResult::AlreadyPurchased;
		}

		if ( weaponId == WEAPON_KNIFE || weaponId == WEAPON_KNIFE_GG )
		{ 
			return AcquireResult::NotAllowedForPurchase; 
		}

		// Validate that each player can buy only so many instances of same weapon type
		if ( mp_weapons_allow_typecount.GetInt() == 0 )
			return AcquireResult::NotAllowedForPurchase;
		else if ( ( mp_weapons_allow_typecount.GetInt() > 0 ) && ( m_iWeaponPurchasesThisRound[weaponId] >= mp_weapons_allow_typecount.GetInt() ) )
			return AcquireResult::AlreadyPurchased;
	}

	return AcquireResult::Allowed;
}

//-----------------------------------------------------------------------------
// Purpose: Returns weapon if already owns a weapon of this class
//-----------------------------------------------------------------------------
CWeaponCSBase* CCSPlayer::CSWeapon_OwnsThisType( CEconItemView *pItem ) const
{
	/** Removed for partner depot **/
	return NULL;
}

//************************************
// Determine the current cash cost of a weapon for this particular player
// Parameter: CSWeaponID weaponId
//************************************
int CCSPlayer::GetWeaponPrice( CSWeaponID weaponId, const CEconItemView *pWepView ) const
{
	Assert( pWepView || ( weaponId != WEAPON_NONE ) );

	if ( !pWepView && ( weaponId == WEAPON_NONE ) )
		return -1;
		
	bool bHasFullArmor = (ArmorValue() >= 100);

	// special case handling for reduced cost of Kevlar + helmet
	if ( weaponId == ITEM_ASSAULTSUIT )
	{
		int iCost = ITEM_PRICE_ASSAULTSUIT;
		if ( bHasFullArmor && !m_bHasHelmet )
			iCost -= ITEM_PRICE_KEVLAR;
		//else if ( m_bHasHelmet )
		//	iCost = ITEM_PRICE_KEVLAR;

		return iCost;
	}

	// special case handling for reduced cost of Kevlar
	if ( weaponId == ITEM_KEVLAR )
	{
		int iCost = ITEM_PRICE_KEVLAR;

		return iCost;
	}
	// special case handling for reduced cost of Kevlar
	if ( weaponId == ITEM_HEAVYASSAULTSUIT )
	{
		int iCost = ITEM_PRICE_HEAVYASSAULTSUIT;

		return iCost;
	}

//	if ( weaponId == ITEM_EXOSUIT )
//	{
//		int iCost = ITEM_PRICE_EXOSUIT;
//
//		return iCost;
//	}

	const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( weaponId );
	return ( pWeaponInfo ) ? pWeaponInfo->GetWeaponPrice( pWepView ) : 0;
}


bool CCSPlayer::HasWeaponOfType( int nWeaponID ) const
{
	for ( int i = 0; i < WeaponCount(); ++i )
	{		
		CWeaponCSBase *pWeapon = dynamic_cast< CWeaponCSBase* > ( GetWeapon( i ) );

		if ( pWeapon && pWeapon->GetCSWeaponID() == nWeaponID )
		{
			return true;
		}
	}

	return false;
}

#if defined ( GAME_DLL ) || defined ( ENABLE_CLIENT_INVENTORIES_FOR_OTHER_PLAYERS )
CEconItemView *CCSPlayer::GetEquippedItemInLoadoutSlotOrBaseItem( int iLoadoutSlot )
{
	CEconItemView *pBaseItem = CSInventoryManager()->GetBaseItemForTeam( GetTeamNumber(), iLoadoutSlot );

	bool bRandomCosmetics = false;

	// Bots and controlled bots always return the base item unless we're randomizing
	if ( !bRandomCosmetics && ( IsBot() || IsControllingBot() ) )
		return pBaseItem;

	CEconItemView *pResult = Inventory()->GetInventoryItemByItemID( m_EquippedLoadoutItemIndices[iLoadoutSlot] );
	if ( !pResult || !pResult->IsValid() )
	{
		pResult = pBaseItem;
	}

	return pResult;
}
#endif



bool CCSPlayer::UpdateDispatchLayer( CAnimationLayer *pLayer, CStudioHdr *pWeaponStudioHdr, int iSequence )
{
	if ( !pWeaponStudioHdr || !pLayer )
	{
		if ( pLayer )
			pLayer->m_nDispatchedDst = ACT_INVALID;
		return false;
	}	

	if ( pLayer->m_pDispatchedStudioHdr != pWeaponStudioHdr || pLayer->m_nDispatchedSrc != iSequence || pLayer->m_nDispatchedDst >= pWeaponStudioHdr->GetNumSeq() )
	{
		pLayer->m_pDispatchedStudioHdr = pWeaponStudioHdr;
		pLayer->m_nDispatchedSrc = iSequence;
		if ( pWeaponStudioHdr )
		{
			const char *pszSeqName = GetSequenceName( iSequence );
			
#ifdef DEBUG
			if ( V_stristr( pszSeqName, "default" ) )
			{
				AssertMsg( false, "Warning: weapon is attempting to play its default sequence as a dispatched anim.\n" );
			}
#endif

			// check if the weapon has a CT or T specific version of this sequence (denoted by a _t or ct suffix)
			if ( GetTeamNumber() == TEAM_TERRORIST )
			{
				char pszLayerNameT[128];
				V_sprintf_safe( pszLayerNameT, "%s_t", pszSeqName );
				int nTeamSpecificSequenceIndex = pWeaponStudioHdr->LookupSequence( pszLayerNameT );
				if ( nTeamSpecificSequenceIndex > 0 )
				{
					pLayer->m_nDispatchedDst = nTeamSpecificSequenceIndex;
					return true;
				}
			}

			pLayer->m_nDispatchedDst = pWeaponStudioHdr->LookupSequence( pszSeqName );
		}
		else
		{
			pLayer->m_nDispatchedDst = ACT_INVALID;
		}
	}
	return (pLayer->m_nDispatchedDst > 0 );
}

bool CCSPlayer::UpdateLayerWeaponDispatch( CAnimationLayer *pLayer, int iSequence )
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( pWeapon )
	{
		CBaseWeaponWorldModel *pWeaponWorldModel = pWeapon->GetWeaponWorldModel();
		if ( pWeaponWorldModel )
		{
			return UpdateDispatchLayer( pLayer, pWeaponWorldModel->GetModelPtr(), iSequence );
		}
	}
	return UpdateDispatchLayer( pLayer, NULL, iSequence );
}

float CCSPlayer::GetLayerSequenceCycleRate( CAnimationLayer *pLayer, int iSequence ) 
{ 
	UpdateLayerWeaponDispatch( pLayer, iSequence );
	if ( pLayer->m_nDispatchedDst != ACT_INVALID )
	{
		// weapon world model overrides rate
		return GetSequenceCycleRate( pLayer->m_pDispatchedStudioHdr, pLayer->m_nDispatchedDst );
	}
	return BaseClass::GetLayerSequenceCycleRate( pLayer, iSequence );
}



//--------------------------------------------------------------------------------------------------------------

#define MATERIAL_NAME_LENGTH 16

#ifdef GAME_DLL

class CFootstepControl : public CBaseTrigger
{
public:
	DECLARE_CLASS( CFootstepControl, CBaseTrigger );
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	virtual int UpdateTransmitState( void );
	virtual void Spawn( void );

	CNetworkVar( string_t, m_source );
	CNetworkVar( string_t, m_destination );
};

LINK_ENTITY_TO_CLASS( func_footstep_control, CFootstepControl );


BEGIN_DATADESC( CFootstepControl )
	DEFINE_KEYFIELD( m_source, FIELD_STRING, "Source" ),
	DEFINE_KEYFIELD( m_destination, FIELD_STRING, "Destination" ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CFootstepControl, DT_FootstepControl )
	SendPropStringT( SENDINFO(m_source) ),
	SendPropStringT( SENDINFO(m_destination) ),
END_SEND_TABLE()

int CFootstepControl::UpdateTransmitState( void )
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

void CFootstepControl::Spawn( void )
{
	InitTrigger();
}

#else

//--------------------------------------------------------------------------------------------------------------

class C_FootstepControl : public C_BaseTrigger
{
public:
	DECLARE_CLASS( C_FootstepControl, C_BaseTrigger );
	DECLARE_CLIENTCLASS();

	C_FootstepControl( void );
	~C_FootstepControl();

	char m_source[MATERIAL_NAME_LENGTH];
	char m_destination[MATERIAL_NAME_LENGTH];
};

IMPLEMENT_CLIENTCLASS_DT(C_FootstepControl, DT_FootstepControl, CFootstepControl)
	RecvPropString( RECVINFO(m_source) ),
	RecvPropString( RECVINFO(m_destination) ),
END_RECV_TABLE()

CUtlVector< C_FootstepControl * > s_footstepControllers;

C_FootstepControl::C_FootstepControl( void )
{
	s_footstepControllers.AddToTail( this );
}

C_FootstepControl::~C_FootstepControl()
{
	s_footstepControllers.FindAndRemove( this );
}

surfacedata_t * CCSPlayer::GetFootstepSurface( const Vector &origin, const char *surfaceName )
{
	for ( int i=0; i<s_footstepControllers.Count(); ++i )
	{
		C_FootstepControl *control = s_footstepControllers[i];

		if ( FStrEq( control->m_source, surfaceName ) )
		{
			if ( control->CollisionProp()->IsPointInBounds( origin ) )
			{
				return physprops->GetSurfaceData( physprops->GetSurfaceIndex( control->m_destination ) );
			}
		}
	}

	return physprops->GetSurfaceData( physprops->GetSurfaceIndex( surfaceName ) );
}

#endif


