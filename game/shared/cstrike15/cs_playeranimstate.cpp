//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "cs_playeranimstate.h"
#include "base_playeranimstate.h"
#include "tier0/vprof.h"
#include "animation.h"
#include "weapon_csbase.h"
#include "studio.h"
#include "apparent_velocity_helper.h"
#include "utldict.h"
#include "weapon_basecsgrenade.h"
#include "datacache/imdlcache.h"

#include "cs_shareddefs.h"

#ifdef CLIENT_DLL
	#include "c_cs_player.h"
	#include "bone_setup.h"
	#include "tier1/interpolatedvar.h"
	#include "c_cs_hostage.h"
#else
	#include "cs_player.h"
	#include "cs_simple_hostage.h"
	#include "cs_gamestats.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#define ANIM_TOPSPEED_WALK			100
#define ANIM_TOPSPEED_RUN			250
#define ANIM_TOPSPEED_RUN_CROUCH	85

#define DEFAULT_IDLE_NAME "idle_upper_"
#define DEFAULT_CROUCH_IDLE_NAME "crouch_idle_upper_"
#define DEFAULT_CROUCH_WALK_NAME "crouch_walk_upper_"
#define DEFAULT_WALK_NAME "walk_upper_"
#define DEFAULT_RUN_NAME "run_upper_"

#define DEFAULT_FIRE_IDLE_NAME "idle_shoot_"
#define DEFAULT_FIRE_CROUCH_NAME "crouch_idle_shoot_"
#define DEFAULT_FIRE_CROUCH_WALK_NAME "crouch_walk_shoot_"
#define DEFAULT_FIRE_WALK_NAME "walk_shoot_"
#define DEFAULT_FIRE_RUN_NAME "run_shoot_"

#define DEFAULT_SILENCER_ATTACH_NAME "silencer_attach_%s"
#define DEFAULT_SILENCER_DETACH_NAME "silencer_detach_%s"

#define ANIM_ACT_DURATION_BEFORE_RETURN_TO_IDLE 0.5f
#define ANIM_ACT_DURATION_BEFORE_RETURN_TO_WALK 0.7f

enum CSPlayerAnimStateLayer_t
{
	FIRESEQUENCE_LAYER = ( AIMSEQUENCE_LAYER + NUM_AIMSEQUENCE_LAYERS ),
	FIRESEQUENCE2_LAYER,
	DEPLOYSEQUENCE_LAYER,
	RELOADSEQUENCE_LAYER,
	SILENCERSEQUENCE_LAYER,
	GRENADESEQUENCE_LAYER,
	FLASHEDSEQUENCE_LAYER,
	FLINCHSEQUENCE_LAYER,
	TAUNTSEQUENCE_LAYER,
	FOOTPLANTSEQUENCE_LAYER,

	NUM_LAYERS_WANTED
};


ConVar post_jump_crouch( "post_jump_crouch", "0.2f", FCVAR_REPLICATED | FCVAR_CHEAT, "This determines how long the third person player character will crouch for after landing a jump.  This only affects the third person animation visuals." );

ConVar hostage_feetyawrate( "hostage_feetyawrate", "720", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "How many degrees per second that hostages can turn their feet or upper body." );

// ------------------------------------------------------------------------------------------------ //
// CCSPlayerAnimState declaration.
// ------------------------------------------------------------------------------------------------ //

class CCSPlayerAnimState : public CBasePlayerAnimState
{
public:
	DECLARE_CLASS( CCSPlayerAnimState, CBasePlayerAnimState );
	friend IPlayerAnimState* CreatePlayerAnimState( CBaseAnimatingOverlay *pEntity, ICSPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences );

	CCSPlayerAnimState();

	virtual void DoAnimationEvent( PlayerAnimEvent_t animEvent, int nData );
	virtual bool IsThrowingGrenade();
	virtual bool ShouldHideGrenadeDuringThrow();
	virtual int CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight, bool bForceIdle );
	virtual void ClearAnimationState();
	virtual bool CanThePlayerMove();
	virtual float GetCurrentMaxGroundSpeed();
	virtual Activity CalcMainActivity();
	virtual void DebugShowAnimState( int iStartLine );
	virtual void ComputeSequences( CStudioHdr *pStudioHdr );
	virtual void ClearAnimationLayers();
	virtual int SelectWeightedSequence( Activity activity );
	// Calculate the playback rate for movement layer
	virtual float CalcMovementPlaybackRate( bool *bIsMoving );
	
	virtual void Update( float eyeYaw, float eyePitch );

	virtual void ModifyTauntDuration( float flTimingChange );

	void InitCS( CBaseAnimatingOverlay *pPlayer, ICSPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences );
	
protected:

	int CalcFireLayerSequence(PlayerAnimEvent_t animEvent);
	void ComputeFireSequence( CStudioHdr *pStudioHdr );

	void ComputeDeploySequence( CStudioHdr *pStudioHdr );
	int CalcDeployLayerSequence( void );

	void ComputeReloadSequence( CStudioHdr *pStudioHdr );
	int CalcReloadLayerSequence( PlayerAnimEvent_t animEvent );

	void ComputeSilencerChangeSequence( CStudioHdr *pStudioHdr );
	int CalcSilencerChangeLayerSequence( PlayerAnimEvent_t animEvent );

	int CalcFlashedLayerSequence( CBaseCombatCharacter *pBaseCombatCharacter );
	void ComputeFlashedSequence( CStudioHdr *pStudioHdr );

	int CalcFlinchLayerSequence( CBaseCombatCharacter *pBaseCombatCharacter );
	void ComputeFlinchSequence( CStudioHdr *pStudioHdr );

	int CalcTauntLayerSequence( CBaseCombatCharacter *pBaseCombatCharacter );
	void ComputeTauntSequence( CStudioHdr *pStudioHdr );

	void ComputeFootPlantSequence(CStudioHdr *pStudioHdr);

	bool IsOuterGrenadePrimed();
	void ComputeGrenadeSequence( CStudioHdr *pStudioHdr );
	int CalcGrenadePrimeSequence();
#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
	int CalcGrenadeThrowSequence( bool bUnderhand = false );
#else
	int CalcGrenadeThrowSequence();
#endif
	int GetOuterGrenadeThrowCounter();

	const char* GetWeaponSuffix();
	bool HandleJumping();

	void UpdateLayerSequenceGeneric( CStudioHdr *pStudioHdr, int iLayer, bool &bEnabled, float &flCurCycle, int &iSequence, bool bWaitAtEnd, float flWeight = 1.0f );

	virtual int CalcSequenceIndex( PRINTF_FORMAT_STRING const char *pBaseName, ... );

	bool ActiveWeaponIsDeployed();

	float GetTimeSinceLastActChange();
	void UpdateTimeSinceLastActChange();

private:

	// Current state variables.
	bool m_bJumping;			// Set on a jump event.
	float m_flJumpStartTime;
	bool m_bFirstJumpFrame;
	
	float m_flPostLandCrouchEndTime;
	
	// Aim sequence plays reload while this is on.
	bool m_bReloading;
	float m_flReloadCycle;
	int m_iReloadSequence;
	float m_flReloadHoldEndTime;	// Intermediate shotgun reloads get held a fraction of a second

	// Sequence for silencer changing
	bool m_bSilencerChanging;
	float m_flSilencerChangeCycle;
	int m_iSilencerChangeSequence;

	// Sequence for flashbang reaction
	float m_flFlashedAmountDelayed;
	float m_flFlashedAmount;
	float m_flLastFlashDuration;
	int m_iFlashedSequence;

	// Sequence for flinches
	float m_flFlinchStartTime;
	float m_flFlinchLength;
	int m_nFlinchSequence;

	// Sequence for taunts
	float m_flTauntStartTime;
	float m_flTauntLength;
	int m_nTauntSequence;

	// Deploy sequence
	bool m_bDeploying;
	int m_iDeploySequence;
	float m_flDeployCycle;

	int m_iCurrentAimSequence;
	float m_flTargetMaxSpeed;
	float m_flCurrentMaxSpeed;

	int m_iIdleFireSequence;

	// This is set to true if ANY animation is being played in the fire layer.
	bool m_bFiring;						// If this is on, then it'll continue the fire animation in the fire layer
										// until it completes.
	int m_iFireSequence;				// (For any sequences in the fire layer, including grenade throw).
	float m_flFireCycle;
	PlayerAnimEvent_t m_delayedFire;	// if we fire while reloading, delay the fire by one frame so we can cancel the reload first
	PlayerAnimEvent_t m_activeFireEvent;// The current or last set fire event.

	// These control grenade animations.
	bool m_bThrowingGrenade;
	bool m_bPrimingGrenade;
	float m_flGrenadeCycle;
	int m_iGrenadeSequence;
	int m_iLastThrowGrenadeCounter;	// used to detect when the guy threw the grenade.

	bool m_bTryingToRunAfterJump;		// State information that allows us to skip the transition animations to running right after a jump.
	Activity m_CurrentActivity;

	CCSPlayer *m_pPlayer;

	// m_iDeployedWeaponID is the active weapon used for animations.  It does not necessarily line up with what the player has active
	// since the animation system uses events that have higher latency than networked variables.
	// Having our own weapon id will make sure we play animations with the correct weapon.
	CSWeaponID m_iDeployedWeaponID;
	float m_flWeaponSwitchTime;

	ICSPlayerAnimStateHelpers *m_pHelpers;

	void CheckCachedSequenceValidity( void );
	void ClearAnimationLayer( int layer );

	int m_sequenceCache[ ACT_CROUCHIDLE+1 ];	// Cache the first N sequences, since we don't have weights.
	int m_cachedModelIndex;						// Model index for which the sequence cache is valid.

	float m_flLastActChangeTime;
	Activity m_LastActivity;
};


IPlayerAnimState* CreatePlayerAnimState( CBaseAnimatingOverlay *pEntity, ICSPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences )
{
	CCSPlayerAnimState *pRet = new CCSPlayerAnimState;
	pRet->InitCS( pEntity, pHelpers, legAnimType, bUseAimSequences );
	return pRet;
}




//----------------------------------------------------------------------------------------------
/**
 * Hostage animation mechanism
 */
class CCSHostageAnimState : public CCSPlayerAnimState
{
public:
	DECLARE_CLASS( CCSHostageAnimState, CCSPlayerAnimState );

	CCSHostageAnimState();

	virtual Activity CalcMainActivity();
	virtual float GetFeetYawRate( void )
	{
		return hostage_feetyawrate.GetFloat();
	}

private:
	Activity mCurrentActivity;
	// No need to cache sequences, and we *do* have multiple sequences per activity
	//virtual int SelectWeightedSequence( Activity activity ) { return GetOuter()->SelectWeightedSequence( activity ); }
};


//----------------------------------------------------------------------------------------------
IPlayerAnimState* CreateHostageAnimState( CBaseAnimatingOverlay *pEntity, ICSPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences )
{
	CCSHostageAnimState *anim = new CCSHostageAnimState;
	anim->InitCS( pEntity, pHelpers, legAnimType, bUseAimSequences );
	return anim;
}


//----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------
CCSHostageAnimState::CCSHostageAnimState()
{
	mCurrentActivity = ACT_IDLE;
}


//----------------------------------------------------------------------------------------------
/**
 * Set hostage animation state
 */
Activity CCSHostageAnimState::CalcMainActivity()
{
	float flOuterSpeed = GetOuterXYSpeed();

	if ( HandleJumping() )
	{
		return ACT_JUMP;
	}
	else
	{
		Assert( dynamic_cast<CHostage*>( m_pOuter ) );
		CHostage *me = (CHostage*)m_pOuter;
		
		// If we have no leader, hang out.
		Activity targetIdle = me->GetLeader() ? ACT_IDLE : ACT_BUSY_QUEUE;
		
		// [msmith] We don't want to jitter the activities based on subtle speed changes.
		// The code below only chagnes the activity if the speed has changed by at least SPEED_CHANGE_BUFFER amount.
		const float SPEED_CHANGE_BUFFER = (ARBITRARY_RUN_SPEED - MOVING_MINIMUM_SPEED) / 4.0f;

		if ( m_pOuter->GetFlags() & FL_DUCKING )
		{
			if ( flOuterSpeed > MOVING_MINIMUM_SPEED + SPEED_CHANGE_BUFFER )
			{
				mCurrentActivity = ACT_RUN_CROUCH;
			}
			else if ( flOuterSpeed < MOVING_MINIMUM_SPEED )
			{
				mCurrentActivity = ACT_COVER_LOW;
			}
		}
		else
		{
			if ( flOuterSpeed > ARBITRARY_RUN_SPEED )
			{
				mCurrentActivity = ACT_RUN;
			}
			else if ( flOuterSpeed < ARBITRARY_RUN_SPEED - SPEED_CHANGE_BUFFER && flOuterSpeed > MOVING_MINIMUM_SPEED + SPEED_CHANGE_BUFFER )
			{
				mCurrentActivity = ACT_WALK;
			}
			else if ( flOuterSpeed < MOVING_MINIMUM_SPEED )
			{
				mCurrentActivity = targetIdle;
			}
		}

		return mCurrentActivity;
	}
}


// ------------------------------------------------------------------------------------------------ //
// CCSPlayerAnimState implementation.
// ------------------------------------------------------------------------------------------------ //

CCSPlayerAnimState::CCSPlayerAnimState()
{
	m_pOuter = NULL;

	m_bJumping = false;
	m_flJumpStartTime = 0.0f;
	m_bFirstJumpFrame = false;

	m_flPostLandCrouchEndTime = 0.0f;

	m_bReloading = false;
	m_flReloadCycle = 0.0f;
	m_iReloadSequence = -1;
	m_flReloadHoldEndTime = 0.0f;

	m_bSilencerChanging = false;
	m_flSilencerChangeCycle = 0.0f;
	m_iSilencerChangeSequence = -1;

	m_flFlashedAmountDelayed = 0;
	m_flFlashedAmount = 0;
	m_flLastFlashDuration = 0;
	m_iFlashedSequence = 0;

	m_flFlinchStartTime = -1.0f;
	m_flFlinchLength = 0.0f;
	m_nFlinchSequence = 0;

	m_flTauntStartTime = -1.0f;
	m_flTauntLength = 0.0f;
	m_nTauntSequence = 0;

	m_bDeploying = false;
	m_iDeploySequence = -1;
	m_flDeployCycle = 0.0f;

	m_iCurrentAimSequence = 0;
	m_flTargetMaxSpeed = 0.0f;
	m_flCurrentMaxSpeed = 0.0f;

	m_bFiring = false;
	m_iFireSequence = -1;
	m_flFireCycle = 0.0f;
	m_delayedFire = PLAYERANIMEVENT_COUNT;
	m_activeFireEvent = PLAYERANIMEVENT_COUNT;
	m_iIdleFireSequence = -1;

	m_bThrowingGrenade = false;
	m_bPrimingGrenade = false;
	m_flGrenadeCycle = 0.0f;
	m_iGrenadeSequence = -1;
	m_iLastThrowGrenadeCounter = 0;
	m_cachedModelIndex = -1;

	m_bTryingToRunAfterJump = false;
	m_CurrentActivity = ACT_IDLE;

	m_pPlayer = NULL;
	m_iDeployedWeaponID = WEAPON_NONE;
	m_flWeaponSwitchTime = 0.0f;

	m_pHelpers = NULL;

	m_flLastActChangeTime = gpGlobals->curtime;
	m_LastActivity = ACT_IDLE;
}

void CCSPlayerAnimState::UpdateTimeSinceLastActChange()
{
	if ( m_CurrentActivity != m_LastActivity )
	{
		m_flLastActChangeTime = gpGlobals->curtime;
		m_LastActivity = m_CurrentActivity;
	}
}

float CCSPlayerAnimState::GetTimeSinceLastActChange()
{
	return ( gpGlobals->curtime - m_flLastActChangeTime );
}

void CCSPlayerAnimState::InitCS( CBaseAnimatingOverlay *pEntity, ICSPlayerAnimStateHelpers *pHelpers, LegAnimType_t legAnimType, bool bUseAimSequences )
{
	CModAnimConfig config;
	config.m_flMaxBodyYawDegrees = 60;
	
	//When correcting the player's yaw, m_flMaxBodyYawDegreesCorrectionAmount is applied instead of the max. 
	//Since this value is intended to be more than m_flMaxBodyYawDegrees, it will set the new yaw PAST the midpoint.
	//This looks a little more lifelike when the player is rapidly turning, and if the player halts, it makes the facefront that follows more obvious.
	config.m_flMaxBodyYawDegreesCorrectionAmount = 90;

	//Disable foot plant tunring if the feet are lagging behind by more than this angle
	config.m_flIdleFootPlantMaxYaw = 110;

	//Turning less than this amount? Don't lift the feet, just shuffle them over.
	config.m_flIdleFootPlantFootLiftDelta = 25;

	config.m_LegAnimType = legAnimType;
	config.m_bUseAimSequences = bUseAimSequences;

	m_pPlayer = ToCSPlayer( pEntity );

	m_pHelpers = pHelpers;

	BaseClass::Init( pEntity, config );
}

void CCSPlayerAnimState::ModifyTauntDuration( float flTimingChange )
{
	float flInterp = ( m_flTauntLength <= 0.0f ) ? ( -1.0f ) : ( ( gpGlobals->curtime - m_flTauntStartTime ) / m_flTauntLength );
	bool bTaunt = ( flInterp >= 0.0f && flInterp <= 1.0f );

	if ( bTaunt )
	{
		// Move to start time to account for how much the timing change
		m_flTauntStartTime -= flTimingChange;
	}
}


//--------------------------------------------------------------------------------------------------------------
void CCSPlayerAnimState::CheckCachedSequenceValidity( void )
{
	if ( m_cachedModelIndex != GetOuter()->GetModelIndex() )
	{
		m_cachedModelIndex = GetOuter()->GetModelIndex();
		for ( int i=0; i<=ACT_CROUCHIDLE; ++i )
		{
			m_sequenceCache[i] = -1;
		}

		// precache the sequences we'll be using for movement
		if ( m_cachedModelIndex > 0 )
		{
			m_sequenceCache[ACT_HOP - 1] = GetOuter()->SelectWeightedSequence( ACT_HOP );
			m_sequenceCache[ACT_LEAP - 1] = GetOuter()->SelectWeightedSequence( ACT_LEAP );
			m_sequenceCache[ACT_JUMP - 1] = GetOuter()->SelectWeightedSequence( ACT_JUMP );
			m_sequenceCache[ACT_IDLE - 1] = GetOuter()->SelectWeightedSequence( ACT_IDLE );
			m_sequenceCache[ACT_RUN_CROUCH - 1] = GetOuter()->SelectWeightedSequence( ACT_RUN_CROUCH );
			m_sequenceCache[ACT_CROUCHIDLE - 1] = GetOuter()->SelectWeightedSequence( ACT_CROUCHIDLE );
			m_sequenceCache[ACT_RUN - 1] = GetOuter()->SelectWeightedSequence( ACT_RUN );
			m_sequenceCache[ACT_WALK - 1] = GetOuter()->SelectWeightedSequence( ACT_WALK );
			m_sequenceCache[ACT_IDLE - 1] = GetOuter()->SelectWeightedSequence( ACT_IDLE );
		}
	}
}

void CCSPlayerAnimState::ClearAnimationLayer( int layer )
{
	// Client obeys Order of CBaseAnimatingOverlay::MAX_OVERLAYS (15), but server trusts only the ANIM_LAYER_ACTIVE flag.
	CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( layer );
	if ( pLayer )
	{
		pLayer->SetWeight( 0.0f );
		pLayer->SetOrder( CBaseAnimatingOverlay::MAX_OVERLAYS );
#ifndef CLIENT_DLL
		pLayer->m_fFlags = 0;
#endif
	}
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Cache the sequence numbers for the first ACT_HOP activities, since the CS player doesn't have multiple
 * sequences per activity.
 */
int CCSPlayerAnimState::SelectWeightedSequence( Activity activity )
{
	VPROF( "CCSPlayerAnimState::ComputeMainSequence" );

	if ( activity > ACT_CROUCHIDLE || activity < 1 )
	{
		return GetOuter()->SelectWeightedSequence( activity );
	}

	CheckCachedSequenceValidity();

	int sequence = m_sequenceCache[ activity - 1 ];
	if ( sequence < 0 )
	{
		// just in case, look up the sequence if we didn't precache it above
		sequence = m_sequenceCache[ activity - 1 ] = GetOuter()->SelectWeightedSequence( activity );
	}

#if defined(CLIENT_DLL) && defined(_DEBUG)
	int realSequence = GetOuter()->SelectWeightedSequence( activity );
	Assert( realSequence == sequence );
#endif

	return sequence;
}


//--------------------------------------------------------------------------------------------------------------
// sprintf version of LookupSequence() 
//--------------------------------------------------------------------------------------------------------------
int CCSPlayerAnimState::CalcSequenceIndex( PRINTF_FORMAT_STRING const char *pBaseName, ... )
{
	VPROF( "CCSPlayerAnimState::CalcSequenceIndex" );

	CheckCachedSequenceValidity();

	char szFullName[512];
	va_list marker;
	va_start( marker, pBaseName );
	Q_vsnprintf( szFullName, sizeof( szFullName ), pBaseName, marker );
	va_end( marker );

	// FIXME: this looks up on the player, not their weapon
	int iSequence = GetOuter()->LookupSequence( szFullName );
	
	// Show warnings if we can't find anything here.
	if ( iSequence == -1 )
	{
		static CUtlDict<int,int> dict;
		if ( dict.Find( szFullName ) == dict.InvalidIndex() )
		{
			dict.Insert( szFullName, 0 );
			Warning( "CalcSequenceIndex: can't find '%s'.\n", szFullName );
		}

		iSequence = 0;
	}

	return iSequence;
}


void CCSPlayerAnimState::ClearAnimationState()
{
	m_bJumping = false;
	m_bDeploying = false;
	m_bFiring = false;
	m_bReloading = false;
	m_flReloadHoldEndTime = 0.0f;
	m_bSilencerChanging = false;
	m_bThrowingGrenade = m_bPrimingGrenade = false;
	m_iLastThrowGrenadeCounter = GetOuterGrenadeThrowCounter();
	
	m_bTryingToRunAfterJump = false;

	BaseClass::ClearAnimationState();
}


void CCSPlayerAnimState::DoAnimationEvent( PlayerAnimEvent_t animEvent, int nData )
{
	Assert( animEvent != PLAYERANIMEVENT_THROW_GRENADE );

	CWeaponCSBase *pActiveWeapon = m_pHelpers->CSAnim_GetActiveWeapon();
	bool currentWeaponIsDeployedWeapon = ActiveWeaponIsDeployed();

	MDLCACHE_CRITICAL_SECTION();
	switch ( animEvent )
	{
	case PLAYERANIMEVENT_FIRE_GUN_PRIMARY:
	case PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT:
	case PLAYERANIMEVENT_FIRE_GUN_PRIMARY_SPECIAL1:
	case PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT_SPECIAL1:
	case PLAYERANIMEVENT_FIRE_GUN_SECONDARY:
	case PLAYERANIMEVENT_FIRE_GUN_SECONDARY_SPECIAL1:
		if ( currentWeaponIsDeployedWeapon )
		{
			// Regardless of what we're doing in the fire layer, restart it.
			m_activeFireEvent = animEvent;
			m_iFireSequence = CalcFireLayerSequence( animEvent );
			m_bFiring = m_iFireSequence != -1;
			m_flFireCycle = 0;
	
			// If we are interrupting a (shotgun) reload, cancel the reload, and fire next frame.
			if ( m_bFiring && m_bReloading )
			{
				m_bReloading = false;
				m_iReloadSequence = -1;
	
				m_delayedFire = animEvent;
				m_bFiring = false;
				m_iFireSequence = -1;
	
				ClearAnimationLayer( RELOADSEQUENCE_LAYER );
			}

#ifdef CLIENT_DLL
			if ( m_bFiring && !m_bReloading )
			{
				if ( m_pPlayer )
				{
					m_pPlayer->ProcessMuzzleFlashEvent();
				}
			}
#endif
		}
		break;

	case PLAYERANIMEVENT_JUMP:
		// Play the jump animation.
		m_bJumping = true;
		m_bFirstJumpFrame = true;
		m_flJumpStartTime = gpGlobals->curtime;
		break;

	case PLAYERANIMEVENT_RELOAD:
		{
			// ignore normal reload events for shotguns - they get sent to trigger sounds etc only
			// [mlowrance] Mag7 special case: it's classified as a Shotgun, but reloads like a conventional gun with a clip.
			if ( pActiveWeapon && ( pActiveWeapon->GetWeaponType() != WEAPONTYPE_SHOTGUN || pActiveWeapon->GetCSWeaponID() == WEAPON_MAG7 ) )
			{
				m_iReloadSequence = CalcReloadLayerSequence( animEvent );
				if ( m_iReloadSequence != -1 )
				{
					m_bReloading = true;
					m_flReloadCycle = 0;
				}
				else
				{
					m_bReloading = false;
				}
			}
		}
		break;

	case PLAYERANIMEVENT_SILENCER_ATTACH:
		{
			m_iSilencerChangeSequence = CalcSilencerChangeLayerSequence( animEvent );
			if ( m_iSilencerChangeSequence != -1 )
			{
				m_bSilencerChanging = true;
				m_flSilencerChangeCycle = 0;
			}
			else
			{
				m_bSilencerChanging = false;
			}
		}
		break;

	case PLAYERANIMEVENT_SILENCER_DETACH:
		{
			m_iSilencerChangeSequence = CalcSilencerChangeLayerSequence( animEvent );
			if ( m_iSilencerChangeSequence != -1 )
			{
				m_bSilencerChanging = true;
				m_flSilencerChangeCycle = 0;
			}
			else
			{
				m_bSilencerChanging = false;
			}
		}
		break;

	case PLAYERANIMEVENT_RELOAD_START:
	case PLAYERANIMEVENT_RELOAD_LOOP:
		// Set the hold time for _start and _loop anims, then fall through to the _end case
		m_flReloadHoldEndTime = gpGlobals->curtime + 0.75f;

	case PLAYERANIMEVENT_RELOAD_END:
		{
			// ignore shotgun reload events for non-shotguns
			// [mlowrance] Mag7 special case: it's classified as a Shotgun, but reloads like a conventional gun with a clip.
			if ( pActiveWeapon && ( pActiveWeapon->GetWeaponType() != WEAPONTYPE_SHOTGUN || pActiveWeapon->GetCSWeaponID() == WEAPON_MAG7 ) )
			{
				m_flReloadHoldEndTime = 0.0f;  // clear this out in case we set it in _START or _LOOP above
			}
			else
			{
				m_iReloadSequence = CalcReloadLayerSequence( animEvent );
				if ( m_iReloadSequence != -1 )
				{
					m_bReloading = true;
					m_flReloadCycle = 0;
				}
				else
				{
					m_bReloading = false;
				}
			}
		}
		break;

	case PLAYERANIMEVENT_CLEAR_FIRING:
		{
			m_iFireSequence = -1;
			m_flFireCycle = 0;
			m_bFiring = false;
			ClearAnimationLayer( FIRESEQUENCE_LAYER );
			ClearAnimationLayer( FIRESEQUENCE2_LAYER );
		}
		break;

	case PLAYERANIMEVENT_DEPLOY:
		{
			// Update the weapon to use for animations.
			if ( pActiveWeapon != NULL )
			{
				m_iDeployedWeaponID = pActiveWeapon->GetCSWeaponID();
			}
			else
			{
				m_iDeployedWeaponID = WEAPON_NONE;
			}

			// Start the Deploy animation if we have one.
			m_iDeploySequence = CalcDeployLayerSequence();
			if ( m_iDeploySequence != -1 )
			{
				m_bDeploying = true;
				m_flDeployCycle = 0.0f;

			}
			else
			{
				m_bDeploying = false;
				m_iDeploySequence = -1;
				ClearAnimationLayer( DEPLOYSEQUENCE_LAYER );
			}

		}
		break;

	case PLAYERANIMEVENT_GRENADE_PULL_PIN:
		break;

	//default:
	//	Assert( !"CCSPlayerAnimState::DoAnimationEvent" );
	}
}


float g_flThrowGrenadeFraction = 0.25;
bool CCSPlayerAnimState::IsThrowingGrenade()
{
	if ( m_bThrowingGrenade )
	{
		// An animation event would be more appropriate here.
		return m_flGrenadeCycle < g_flThrowGrenadeFraction;
	}
	else
	{
		bool bThrowPending = (m_iLastThrowGrenadeCounter != GetOuterGrenadeThrowCounter());
		return bThrowPending || IsOuterGrenadePrimed();
	}
}

bool CCSPlayerAnimState::ShouldHideGrenadeDuringThrow()
{
	return m_bThrowingGrenade && (m_flGrenadeCycle > 0.1f);
}

int CCSPlayerAnimState::CalcSilencerChangeLayerSequence( PlayerAnimEvent_t animEvent )
{
	if ( m_delayedFire != PLAYERANIMEVENT_COUNT )
		return ACTIVITY_NOT_AVAILABLE;

	const char *weaponSuffix = GetWeaponSuffix();
	if ( !weaponSuffix )
		return ACTIVITY_NOT_AVAILABLE;

	CWeaponCSBase *pWeapon = m_pHelpers->CSAnim_GetActiveWeapon();
	if ( !pWeapon )
		return ACTIVITY_NOT_AVAILABLE;

	char szName[512];
	int iSilencerChangeSequence = -1;

	if ( animEvent == PLAYERANIMEVENT_SILENCER_ATTACH )
	{
		Q_snprintf( szName, sizeof( szName ), DEFAULT_SILENCER_ATTACH_NAME, weaponSuffix );
	}
	else if ( animEvent == PLAYERANIMEVENT_SILENCER_DETACH )
	{
		Q_snprintf( szName, sizeof( szName ), DEFAULT_SILENCER_DETACH_NAME, weaponSuffix );
	}
	else
	{
		return ACTIVITY_NOT_AVAILABLE;
	}
	
	iSilencerChangeSequence = m_pOuter->LookupSequence( szName );
	if ( iSilencerChangeSequence != -1 )
		return iSilencerChangeSequence;

	// no current fallback
	return ACTIVITY_NOT_AVAILABLE;
}


int CCSPlayerAnimState::CalcReloadLayerSequence( PlayerAnimEvent_t animEvent )
{
	if ( m_delayedFire != PLAYERANIMEVENT_COUNT )
		return -1;

	const char *weaponSuffix = GetWeaponSuffix();
	if ( !weaponSuffix )
		return -1;

	CWeaponCSBase *pWeapon = m_pHelpers->CSAnim_GetActiveWeapon();
	if ( !pWeapon )
		return -1;

	const char *prefix = "";
	switch ( GetCurrentMainSequenceActivity() )
	{
		case ACT_PLAYER_RUN_FIRE:
		case ACT_RUN:
			prefix = "run";
			break;

		case ACT_PLAYER_WALK_FIRE:
		case ACT_WALK:
			prefix = "walk";
			break;

		case ACT_PLAYER_CROUCH_FIRE:
		case ACT_CROUCHIDLE:
			prefix = "crouch_idle";
			break;

		case ACT_PLAYER_CROUCH_WALK_FIRE:
		case ACT_RUN_CROUCH:
			prefix = "crouch_walk";
			break;

		default:
		case ACT_PLAYER_IDLE_FIRE:
			prefix = "idle";
			break;
	}

	const char *reloadSuffix = "";
	switch ( animEvent )
	{
	case PLAYERANIMEVENT_RELOAD_START:
		reloadSuffix = "_start";
		break;

	case PLAYERANIMEVENT_RELOAD_LOOP:
		reloadSuffix = "_loop";
		break;

	case PLAYERANIMEVENT_RELOAD_END:
		reloadSuffix = "_end";
		break;
	}

	// First, look for <prefix>_reload_<weapon name><_start|_loop|_end>.
	char szName[512];
	int iReloadSequence = -1;

// Avoid doing this look up since we don't need it for CSGO
#if !defined( CSTRIKE15 )

	Q_snprintf( szName, sizeof( szName ), "%s_reload_%s%s", prefix, weaponSuffix, reloadSuffix );
	iReloadSequence = m_pOuter->LookupSequence( szName );
	if ( iReloadSequence != -1 )
		return iReloadSequence;

#endif

	// Next, look for reload_<weapon name><_start|_loop|_end>.
	Q_snprintf( szName, sizeof( szName ), "reload_%s%s", weaponSuffix, reloadSuffix );
	iReloadSequence = m_pOuter->LookupSequence( szName );
	if ( iReloadSequence != -1 )
		return iReloadSequence;

	// Ok, look for generic categories.. pistol, shotgun, rifle, etc.
	if ( pWeapon->GetWeaponType() == WEAPONTYPE_PISTOL )
	{
		Q_snprintf( szName, sizeof( szName ), "reload_pistol" );
		iReloadSequence = m_pOuter->LookupSequence( szName );
		if ( iReloadSequence != -1 )
			return iReloadSequence;
	}
			
	// Fall back to reload_m4.
	iReloadSequence = CalcSequenceIndex( "reload_m4" );
	if ( iReloadSequence > 0 )
		return iReloadSequence;

	return -1;
}

void CCSPlayerAnimState::UpdateLayerSequenceGeneric( CStudioHdr *pStudioHdr, int iLayer, bool &bEnabled, float &flCurCycle, int &iSequence, bool bWaitAtEnd, float flWeight )
{
	if ( !bEnabled || iSequence < 0 )
		return;

	CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iLayer );
	pLayer->SetSequence( iSequence );

	// find cycle rate for the layer
	float flSequenceCycleRate = m_pPlayer->GetLayerSequenceCycleRate( pLayer, iSequence );

	// weapon vs self
	flCurCycle += flSequenceCycleRate * gpGlobals->frametime;

	if ( flCurCycle > 1 )
	{
		if ( bWaitAtEnd )
		{
			flCurCycle = 1;
		}
		else
		{
			// Not firing anymore.
			bEnabled = false;
			iSequence = 0;
			return;
		}
	}

	// Now dump the state into its animation layer.
	pLayer->SetCycle( flCurCycle );

	pLayer->SetPlaybackRate( 1.0f );
	pLayer->SetWeight( flWeight );
	pLayer->SetOrder( iLayer );
#ifndef CLIENT_DLL
	pLayer->m_fFlags |= ANIM_LAYER_ACTIVE; 
#endif
}

bool CCSPlayerAnimState::IsOuterGrenadePrimed()
{
	CBaseCombatCharacter *pChar = m_pOuter->MyCombatCharacterPointer();
	if ( pChar )
	{
		CBaseCSGrenade *pGren = dynamic_cast<CBaseCSGrenade*>( pChar->GetActiveWeapon() );
		return pGren && pGren->IsPinPulled();
	}
	else
	{
		return NULL;
	}
}


void CCSPlayerAnimState::ComputeGrenadeSequence( CStudioHdr *pStudioHdr )
{
	VPROF( "CCSPlayerAnimState::ComputeGrenadeSequence" );

	if ( m_bThrowingGrenade )
	{
		UpdateLayerSequenceGeneric( pStudioHdr, GRENADESEQUENCE_LAYER, m_bThrowingGrenade, m_flGrenadeCycle, m_iGrenadeSequence, false );
	}
	else
	{

#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
		bool bThrowingUnderhand = false;
#endif

		if ( m_pPlayer )
		{
			CBaseCombatWeapon *pWeapon = m_pPlayer->GetActiveWeapon();
			CBaseCSGrenade *pGren = dynamic_cast<CBaseCSGrenade*>( pWeapon );
			if ( !pGren )
			{
				// The player no longer has a grenade equipped. Bail.
				m_iLastThrowGrenadeCounter = GetOuterGrenadeThrowCounter();
				return;
			}
#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
			else
			{
				bThrowingUnderhand = pGren->IsThrownUnderhand();
			}
#endif
		}

		// Priming the grenade isn't an event.. we just watch the player for it.
		// Also play the prime animation first if he wants to throw the grenade.
		bool bThrowPending = (m_iLastThrowGrenadeCounter != GetOuterGrenadeThrowCounter());
		if ( IsOuterGrenadePrimed() || bThrowPending )
		{
			if ( !m_bPrimingGrenade )
			{
				// If this guy just popped into our PVS, and he's got his grenade primed, then
				// let's assume that it's all the way primed rather than playing the prime
				// animation from the start.
				if ( TimeSinceLastAnimationStateClear() < 0.4f )
				{
					m_flGrenadeCycle = 1;
				}
				else
				{
					m_flGrenadeCycle = 0;
				}
					
				m_bPrimingGrenade = true;
			}

			// [MLowrance] Moved this from the IF statement above.  This allows the sequences to switch while maintaining the last frame of the prime anim.
			// So priming the grenade at Idle, then going to a Run while holding the last frame doesn't break the character.
			m_iGrenadeSequence = CalcGrenadePrimeSequence();

			UpdateLayerSequenceGeneric( pStudioHdr, GRENADESEQUENCE_LAYER, m_bPrimingGrenade, m_flGrenadeCycle, m_iGrenadeSequence, true );
			
			// If we're waiting to throw and we're done playing the prime animation...
			// [mlowrance] due to timing and gameplay issues, if we're pending to throw... don't wait, do it
			if ( bThrowPending ) //&& m_flGrenadeCycle == 1 )
			{
				m_iLastThrowGrenadeCounter = GetOuterGrenadeThrowCounter();

				// Now play the throw animation.
#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
				m_iGrenadeSequence = CalcGrenadeThrowSequence( bThrowingUnderhand );
#else
				m_iGrenadeSequence = CalcGrenadeThrowSequence();
#endif				
				if ( m_iGrenadeSequence != -1 )
				{
					// Configure to start playing 
					m_bThrowingGrenade = true;
					m_bPrimingGrenade = false;
					m_flGrenadeCycle = 0;
				}
			}
		}
		else
		{
			m_bPrimingGrenade = false;
		}
	}
}


int CCSPlayerAnimState::CalcGrenadePrimeSequence()
{
	const char *prefix = "";
	switch ( GetCurrentMainSequenceActivity() )
	{
		case ACT_PLAYER_RUN_FIRE:
		case ACT_RUN:
			prefix = "run";
			break;

		case ACT_PLAYER_WALK_FIRE:
		case ACT_WALK:
			prefix = "walk";
			break;

		case ACT_PLAYER_CROUCH_FIRE:
		case ACT_CROUCHIDLE:
			prefix = "crouch_idle";
			break;

		case ACT_PLAYER_CROUCH_WALK_FIRE:
		case ACT_RUN_CROUCH:
			prefix = "crouch_walk";
			break;

		default:
		case ACT_PLAYER_IDLE_FIRE:
			prefix = "idle";
			break;
	}

	return CalcSequenceIndex( "%s_shoot_gren1", prefix );
}

#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
int CCSPlayerAnimState::CalcGrenadeThrowSequence( bool bUnderhand )
#else
int CCSPlayerAnimState::CalcGrenadeThrowSequence( )
#endif
{
	const char *prefix = "";
	switch ( GetCurrentMainSequenceActivity() )
	{
		case ACT_PLAYER_RUN_FIRE:
		case ACT_RUN:
			prefix = "run";
			break;

		case ACT_PLAYER_WALK_FIRE:
		case ACT_WALK:
			prefix = "walk";
			break;

		case ACT_PLAYER_CROUCH_FIRE:
		case ACT_CROUCHIDLE:
			prefix = "crouch_idle";
			break;

		case ACT_PLAYER_CROUCH_WALK_FIRE:
		case ACT_RUN_CROUCH:
			prefix = "crouch_walk";
			break;

		default:
		case ACT_PLAYER_IDLE_FIRE:
			prefix = "idle";
			break;
	}
#ifdef GRENADE_UNDERHAND_FEATURE_ENABLED
	if ( bUnderhand )
	{
		return CalcSequenceIndex( "%s_shoot_gren3", prefix );
	}
	else
#endif
	{
		return CalcSequenceIndex( "%s_shoot_gren2", prefix );
	}
}

int CCSPlayerAnimState::GetOuterGrenadeThrowCounter()
{
	if ( m_pPlayer )
		return m_pPlayer->m_iThrowGrenadeCounter;
	else
		return 0;
}

void CCSPlayerAnimState::ComputeSilencerChangeSequence( CStudioHdr *pStudioHdr )
{
	VPROF( "CCSPlayerAnimState::ComputeSilencerChangeSequence" );
	UpdateLayerSequenceGeneric( pStudioHdr, SILENCERSEQUENCE_LAYER, m_bSilencerChanging, m_flSilencerChangeCycle, m_iSilencerChangeSequence, false );
}

void CCSPlayerAnimState::ComputeReloadSequence( CStudioHdr *pStudioHdr )
{
	VPROF( "CCSPlayerAnimState::ComputeReloadSequence" );
	bool hold = m_flReloadHoldEndTime > gpGlobals->curtime;
	UpdateLayerSequenceGeneric( pStudioHdr, RELOADSEQUENCE_LAYER, m_bReloading, m_flReloadCycle, m_iReloadSequence, hold );
	if ( !m_bReloading )
	{
		m_flReloadHoldEndTime = 0.0f;
	}
}

int CCSPlayerAnimState::CalcFlinchLayerSequence( CBaseCombatCharacter *pBaseCombatCharacter )
{
	if ( !pBaseCombatCharacter )
		return ACTIVITY_NOT_AVAILABLE;

	int nSequence = ACTIVITY_NOT_AVAILABLE;

	float flTimeSinceLastInjury = pBaseCombatCharacter->GetTimeSinceLastInjury();
	float flTimeSinceEndOfLastFlinch = gpGlobals->curtime - ( m_flFlinchStartTime + m_flFlinchLength );

	// If there were hurt after the end of the last flinch, time to flinch again!
	bool bFlinch = pBaseCombatCharacter->HasEverBeenInjured() && ( flTimeSinceLastInjury <= flTimeSinceEndOfLastFlinch );

	if ( bFlinch )
	{
		Activity flinchActivity;

		if ( m_pOuter->GetFlags() & FL_DUCKING )
		{
			// Crouching flinches
			if ( pBaseCombatCharacter->LastHitGroup() != HITGROUP_HEAD )
			{
				switch ( pBaseCombatCharacter->GetLastInjuryRelativeDirection() )
				{
				case DAMAGED_DIR_BACK:
					flinchActivity = ACT_FLINCH_CROUCH_FRONT;
					break;

				case DAMAGED_DIR_LEFT:
					flinchActivity = ACT_FLINCH_CROUCH_LEFT;
					break;

				case DAMAGED_DIR_RIGHT:
					flinchActivity = ACT_FLINCH_CROUCH_RIGHT;
					break;

				case DAMAGED_DIR_FRONT:
				case DAMAGED_DIR_NONE:
				default:
					flinchActivity = ACT_FLINCH_CROUCH_FRONT;
					break;
				}
			}
			else
			{
				switch ( pBaseCombatCharacter->GetLastInjuryRelativeDirection() )
				{
				case DAMAGED_DIR_BACK:
					flinchActivity = ACT_FLINCH_HEAD_BACK;
					break;

				case DAMAGED_DIR_LEFT:
					flinchActivity = ACT_FLINCH_HEAD_LEFT;
					break;

				case DAMAGED_DIR_RIGHT:
					flinchActivity = ACT_FLINCH_HEAD_RIGHT;
					break;

				case DAMAGED_DIR_FRONT:
				case DAMAGED_DIR_NONE:
				default:
					flinchActivity = ACT_FLINCH_HEAD;
					break;
				}
			}

			// do we have a sequence for the ideal activity?
			if ( SelectWeightedSequence( flinchActivity ) == ACTIVITY_NOT_AVAILABLE )
			{
				flinchActivity = ACT_FLINCH_CROUCH_FRONT;
			}
		}
		else
		{
			// Standing flinches
			int nLastHitGroup = pBaseCombatCharacter->LastHitGroup();

			switch ( nLastHitGroup )
			{
				// pick a region-specific flinch
			case HITGROUP_HEAD:
				{
					switch ( pBaseCombatCharacter->GetLastInjuryRelativeDirection() )
					{
					case DAMAGED_DIR_BACK:
						flinchActivity = ACT_FLINCH_HEAD_BACK;
						break;

					case DAMAGED_DIR_LEFT:
						flinchActivity = ACT_FLINCH_HEAD_LEFT;
						break;

					case DAMAGED_DIR_RIGHT:
						flinchActivity = ACT_FLINCH_HEAD_RIGHT;
						break;

					case DAMAGED_DIR_FRONT:
					case DAMAGED_DIR_NONE:
					default:
						flinchActivity = ACT_FLINCH_HEAD;
						break;
					}			 
				}
				break;
			case HITGROUP_STOMACH:
				flinchActivity = ( pBaseCombatCharacter->GetLastInjuryRelativeDirection() == DAMAGED_DIR_BACK ) ? ACT_FLINCH_STOMACH_BACK : ACT_FLINCH_STOMACH;
				break;
			case HITGROUP_LEFTARM:
				flinchActivity = ACT_FLINCH_LEFTARM;
				break;
			case HITGROUP_RIGHTARM:
				flinchActivity = ACT_FLINCH_RIGHTARM;
				break;
			case HITGROUP_LEFTLEG:
				flinchActivity = ACT_FLINCH_LEFTLEG;
				break;
			case HITGROUP_RIGHTLEG:
				flinchActivity = ACT_FLINCH_RIGHTLEG;
				break;
			case HITGROUP_CHEST:
				flinchActivity = ( pBaseCombatCharacter->GetLastInjuryRelativeDirection() == DAMAGED_DIR_BACK ) ? ACT_FLINCH_CHEST_BACK : ACT_FLINCH_CHEST;
				break;
			case HITGROUP_GEAR:
			case HITGROUP_GENERIC:
			default:
				// just get a generic flinch.
				flinchActivity = flinchActivity = ( pBaseCombatCharacter->GetLastInjuryRelativeDirection() == DAMAGED_DIR_BACK ) ? ACT_FLINCH_STOMACH_BACK : ACT_FLINCH_PHYSICS;
				break;
			}
		}

		// Get the sequence for this activity
		nSequence = SelectWeightedSequence( flinchActivity );

		// Do we have a sequence for the ideal activity?
		if ( nSequence == ACTIVITY_NOT_AVAILABLE )
		{
			// Fall back to a basic physics flinch
			nSequence = SelectWeightedSequence( ACT_FLINCH_PHYSICS );
		}
	}

	return nSequence;
}

void CCSPlayerAnimState::ComputeFootPlantSequence( CStudioHdr *pStudioHdr )
{
	if ( !m_bInFootPlantIdleTurn )
		return;

	VPROF("CCSPlayerAnimState::ComputeFootPlantSequence");

	CBaseCombatCharacter *pBaseCombatCharacter = dynamic_cast<CBaseCombatCharacter*>(m_pOuter);
	if (!pBaseCombatCharacter)
		return;

	int nSequence = SelectWeightedSequence( m_bFootPlantIdleNeedToLiftFeet ? ACT_TURN : ACT_STEP_FORE );
	UpdateLayerSequenceGeneric( pStudioHdr, FOOTPLANTSEQUENCE_LAYER, m_bInFootPlantIdleTurn, m_flFootPlantIdleTurnCycle, nSequence, false );
}

void CCSPlayerAnimState::ComputeFlinchSequence( CStudioHdr *pStudioHdr )
{
	VPROF( "CCSPlayerAnimState::ComputeFlinchSequence" );

	CBaseCombatCharacter *pBaseCombatCharacter = dynamic_cast< CBaseCombatCharacter* >( m_pOuter );
	if ( !pBaseCombatCharacter )
		return;

	float flInterp = ( m_flFlinchLength <= 0.0f ) ? ( -1.0f ) : ( ( gpGlobals->curtime - m_flFlinchStartTime ) / m_flFlinchLength );
	bool bFlinch = ( flInterp >= 0.0f && flInterp <= 1.0f );

	if ( bFlinch && 
		 ( pBaseCombatCharacter->LastHitGroup() == HITGROUP_HEAD ) && 
		 m_nFlinchSequence != SelectWeightedSequence( ACT_FLINCH_HEAD ) &&
		 m_nFlinchSequence != SelectWeightedSequence( ACT_FLINCH_HEAD_BACK ) &&
		 m_nFlinchSequence != SelectWeightedSequence( ACT_FLINCH_HEAD_LEFT ) &&
		 m_nFlinchSequence != SelectWeightedSequence( ACT_FLINCH_HEAD_RIGHT ) )
	{
		// They were hit in the head while doing a non-head flinch
		// Override it and force a new head flinch
		bFlinch = false;
		m_flFlinchStartTime = -1.0f;
		m_flFlinchLength = 0.0f;
	}

	if ( !bFlinch )
	{
		// Calculate a new flinch if we have one
		int nSequence = CalcFlinchLayerSequence( pBaseCombatCharacter );
		if ( nSequence != ACTIVITY_NOT_AVAILABLE )
		{
			flInterp = 0.0f;
			bFlinch = true;
			m_nFlinchSequence = nSequence;
			m_flFlinchStartTime = gpGlobals->curtime;
			m_flFlinchLength = m_pOuter->SequenceDuration( nSequence );
		}
	}

	// Update with the current flinch state
	UpdateLayerSequenceGeneric( pStudioHdr, FLINCHSEQUENCE_LAYER, bFlinch, flInterp, m_nFlinchSequence, false );
}

int CCSPlayerAnimState::CalcFlashedLayerSequence( CBaseCombatCharacter *pBaseCombatCharacter )
{
	if ( !pBaseCombatCharacter )
		return ACTIVITY_NOT_AVAILABLE;

	return SelectWeightedSequence( ACT_GESTURE_BIG_FLINCH );
}

void CCSPlayerAnimState::ComputeFlashedSequence( CStudioHdr *pStudioHdr )
{
	VPROF( "CCSPlayerAnimState::ComputeFlashedSequence" );

	CBaseCombatCharacter *pBaseCombatCharacter = dynamic_cast< CBaseCombatCharacter* >( m_pOuter );
	if ( !pBaseCombatCharacter )
		return;

	CCSPlayer *pPlayer = ToCSPlayer( pBaseCombatCharacter );
	if ( !pPlayer )
		return;
	
	float flCurrentFlashDuration = pPlayer->m_flFlashDuration.Get();
	if ( flCurrentFlashDuration != m_flLastFlashDuration )
		m_flFlashedAmount = MAX( 0.0f, flCurrentFlashDuration - 1.25f );

	bool bFlashed = m_flFlashedAmount > 0;

	if ( bFlashed )
	{
		int nSequence = CalcFlashedLayerSequence( pBaseCombatCharacter );
		if ( nSequence != ACTIVITY_NOT_AVAILABLE )
		{
			m_iFlashedSequence = nSequence;

			m_flFlashedAmountDelayed = Approach( m_flFlashedAmount, m_flFlashedAmountDelayed, gpGlobals->frametime * 5.0f );

			CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( FLASHEDSEQUENCE_LAYER );
			pLayer->SetSequence( m_iFlashedSequence );
			pLayer->SetCycle( clamp( m_flFlashedAmountDelayed, 0, 1 ) );
			pLayer->SetPlaybackRate( 1.0f );
			pLayer->SetWeight( 1.0f );
			pLayer->SetOrder( FLASHEDSEQUENCE_LAYER );
#ifndef CLIENT_DLL
			pLayer->m_fFlags |= ANIM_LAYER_ACTIVE; 
#endif

		}
	}

	if ( bFlashed )
	{
		m_flFlashedAmount -= gpGlobals->frametime;
	}
	else
	{
		m_flFlashedAmount = 0.0f;
	}
	m_flLastFlashDuration = pPlayer->m_flFlashDuration.Get();

}

int CCSPlayerAnimState::CalcTauntLayerSequence( CBaseCombatCharacter *pBaseCombatCharacter )
{
	return ACTIVITY_NOT_AVAILABLE;
	/*
	if ( !pBaseCombatCharacter )
		return ACTIVITY_NOT_AVAILABLE;

	if ( !m_pPlayer || !m_pPlayer->IsTaunting() )
		return ACTIVITY_NOT_AVAILABLE;

	CWeaponCSBase *pActiveWeapon = m_pPlayer->GetActiveCSWeapon();
	if ( !pActiveWeapon )
		return ACTIVITY_NOT_AVAILABLE;

	CEconItemView *pItemView = pActiveWeapon->GetAttributeContainer()->GetItem();
	if ( !pItemView->IsValid() )
		return ACTIVITY_NOT_AVAILABLE;

	const CEconTauntDefinition *pTauntDef = GetItemSchema()->GetTauntDefinition( pItemView->GetTauntID() );

	const char *pchTauntSequenceName = "";

	if ( pTauntDef )
	{
		pchTauntSequenceName = pTauntDef->GetSequenceName();
	}

	return m_pOuter->LookupSequence( pchTauntSequenceName );
	*/
}

void CCSPlayerAnimState::ComputeTauntSequence( CStudioHdr *pStudioHdr )
{
	VPROF( "CCSPlayerAnimState::ComputeFlinchSequence" );

	CBaseCombatCharacter *pBaseCombatCharacter = dynamic_cast< CBaseCombatCharacter* >( m_pOuter );
	if ( !pBaseCombatCharacter )
		return;

	float flInterp = ( m_flTauntLength <= 0.0f ) ? ( -1.0f ) : ( ( gpGlobals->curtime - m_flTauntStartTime ) / m_flTauntLength );
	bool bTaunt = ( flInterp >= 0.0f && flInterp <= 1.0f );

	if ( !bTaunt )
	{
		// Calculate a new taunt if we have one
		int nSequence = CalcTauntLayerSequence( pBaseCombatCharacter );
		if ( nSequence != ACTIVITY_NOT_AVAILABLE )
		{
			flInterp = 0.0f;
			bTaunt = true;
			m_nTauntSequence = nSequence;
			m_flTauntStartTime = gpGlobals->curtime;
			m_flTauntLength = m_pOuter->SequenceDuration( nSequence );
		}
	}

	// Update with the current taunt state
	UpdateLayerSequenceGeneric( pStudioHdr, TAUNTSEQUENCE_LAYER, bTaunt, flInterp, m_nTauntSequence, false );
}


int CCSPlayerAnimState::CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight, bool bForceIdle )
{
	VPROF( "CCSPlayerAnimState::CalcAimLayerSequence" );

	const char *pSuffix = GetWeaponSuffix();
	if ( !pSuffix )
		return 0;

	Activity activity = GetCurrentMainSequenceActivity();

	if ( bForceIdle )
	{
		switch ( activity )
		{
			case ACT_CROUCHIDLE:
			case ACT_RUN_CROUCH:
				return CalcSequenceIndex( "%s%s", DEFAULT_CROUCH_IDLE_NAME, pSuffix );

			default:
				return CalcSequenceIndex( "%s%s", DEFAULT_IDLE_NAME, pSuffix );
		}
	}
	else
	{
		switch ( activity )
		{
			case ACT_RUN:
			case ACT_LEAP:
				m_iCurrentAimSequence = CalcSequenceIndex( "%s%s", DEFAULT_RUN_NAME, pSuffix );
				break;

			case ACT_WALK:
			case ACT_RUNTOIDLE:
			case ACT_IDLETORUN:
			case ACT_JUMP:
				m_iCurrentAimSequence = CalcSequenceIndex( "%s%s", DEFAULT_WALK_NAME, pSuffix );
				break;

			case ACT_RUN_CROUCH:
				m_iCurrentAimSequence = CalcSequenceIndex( "%s%s", DEFAULT_CROUCH_WALK_NAME, pSuffix );
				break;
				
			// Since we blend between two transitioners, we always have the lower one be idle (hence the forceIdle flag above).
			// Because the lower on is always idle, we never want to have an idle sequence in the upper transitioner.
			case ACT_CROUCHIDLE:
			case ACT_HOP:
			case ACT_IDLE:
			default:
				break;
		}
		return m_iCurrentAimSequence;
	}
}


const char* CCSPlayerAnimState::GetWeaponSuffix()
{
	VPROF( "CCSPlayerAnimState::GetWeaponSuffix" );

	// Figure out the weapon suffix.
	CWeaponCSBase *pWeapon = m_pHelpers->CSAnim_GetActiveWeapon();
	if ( !pWeapon )
		return NULL;

	const char *pSuffix = pWeapon->GetPlayerAnimationExtension();

#ifdef CS_SHIELD_ENABLED
	if ( m_pOuter->HasShield() == true )
	{
		if ( m_pOuter->IsShieldDrawn() == true )
			pSuffix = "shield";
		else 
			pSuffix = "shield_undeployed";
	}
#endif

	return pSuffix;
}


int CCSPlayerAnimState::CalcFireLayerSequence(PlayerAnimEvent_t animEvent)
{
	// Figure out the weapon suffix.
	CWeaponCSBase *pWeapon = m_pHelpers->CSAnim_GetActiveWeapon();
	if ( !pWeapon )
		return -1;

	const char *pSuffix = GetWeaponSuffix();
	if ( !pSuffix )
		return -1;

	char tempsuffix[256];
	if ( pWeapon->GetCSWeaponID() == WEAPON_ELITE )
	{
		bool bPrimary = (animEvent == PLAYERANIMEVENT_FIRE_GUN_PRIMARY);
		Q_snprintf( tempsuffix, sizeof(tempsuffix), "%s_%c", pSuffix, bPrimary?'r':'l' );
		pSuffix = tempsuffix;
	}
	else if ( pWeapon->GetCSWeaponID() == WEAPON_KNIFE || pWeapon->GetCSWeaponID() == WEAPON_KNIFE_GG )
	{
		const char* newSuffix = tempsuffix;

		if ( animEvent == PLAYERANIMEVENT_FIRE_GUN_PRIMARY )
		{
			Q_snprintf( tempsuffix, sizeof(tempsuffix), "%s_light_r", pSuffix );
		}
		else if ( animEvent == PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT )
		{
			Q_snprintf( tempsuffix, sizeof(tempsuffix), "%s_light_l", pSuffix );
		}
		else if ( animEvent == PLAYERANIMEVENT_FIRE_GUN_PRIMARY_SPECIAL1 )
		{
			Q_snprintf( tempsuffix, sizeof(tempsuffix), "%s_light_r_bs", pSuffix );
		}
		else if ( animEvent == PLAYERANIMEVENT_FIRE_GUN_PRIMARY_OPT_SPECIAL1 )
		{
			Q_snprintf( tempsuffix, sizeof(tempsuffix), "%s_light_l_bs", pSuffix );
		}
		else if ( animEvent == PLAYERANIMEVENT_FIRE_GUN_SECONDARY )
		{
			Q_snprintf( tempsuffix, sizeof(tempsuffix), "%s_heavy", pSuffix );
		}
		else if ( animEvent == PLAYERANIMEVENT_FIRE_GUN_SECONDARY_SPECIAL1 )
		{
			Q_snprintf( tempsuffix, sizeof(tempsuffix), "%s_heavy_bs", pSuffix );
		}
		else
		{
			newSuffix = pSuffix;
		}

		pSuffix = newSuffix;
	}

	// Grenades handle their fire events separately
	if ( animEvent == PLAYERANIMEVENT_THROW_GRENADE || pWeapon->IsKindOf(WEAPONTYPE_GRENADE) )
	{
		return -1;
	}

	m_iIdleFireSequence = CalcSequenceIndex( "%s%s", DEFAULT_FIRE_IDLE_NAME, pSuffix );

	switch ( GetCurrentMainSequenceActivity() )
	{
		case ACT_PLAYER_RUN_FIRE:
		case ACT_RUN:
			return CalcSequenceIndex( "%s%s", DEFAULT_FIRE_RUN_NAME, pSuffix );

		case ACT_PLAYER_WALK_FIRE:
		case ACT_WALK:
			return CalcSequenceIndex( "%s%s", DEFAULT_FIRE_WALK_NAME, pSuffix );

		case ACT_PLAYER_CROUCH_FIRE:
		case ACT_CROUCHIDLE:
			return CalcSequenceIndex( "%s%s", DEFAULT_FIRE_CROUCH_NAME, pSuffix );

		case ACT_PLAYER_CROUCH_WALK_FIRE:
		case ACT_RUN_CROUCH:
			return CalcSequenceIndex( "%s%s", DEFAULT_FIRE_CROUCH_WALK_NAME, pSuffix );

		default:
		case ACT_PLAYER_IDLE_FIRE:
			return m_iIdleFireSequence;			
	}
}


bool CCSPlayerAnimState::CanThePlayerMove()
{
	return m_pHelpers->CSAnim_CanMove();
}


float CCSPlayerAnimState::GetCurrentMaxGroundSpeed()
{
	Activity currentActivity = 	m_pOuter->GetSequenceActivity( m_pOuter->GetSequence() );

	if ( currentActivity == ACT_WALK || currentActivity == ACT_JUMP || currentActivity == ACT_IDLE || currentActivity == ACT_HOP || currentActivity == ACT_BUSY_QUEUE || currentActivity == ACT_TURN || currentActivity == ACT_STEP_FORE )
	{
		m_flTargetMaxSpeed = ANIM_TOPSPEED_WALK;
	}
	else if ( currentActivity == ACT_RUN || currentActivity == ACT_LEAP )
	{
		m_flTargetMaxSpeed = ANIM_TOPSPEED_RUN;
		if ( m_pPlayer )
		{
			CBaseCombatWeapon *activeWeapon = m_pPlayer->GetActiveWeapon();
			if ( activeWeapon )
			{
				CWeaponCSBase *csWeapon = dynamic_cast< CWeaponCSBase * >( activeWeapon );
				if ( csWeapon )
				{
					m_flTargetMaxSpeed = csWeapon->GetMaxSpeed();
				}
			}
		}
	}
	else if ( currentActivity == ACT_RUN_CROUCH || currentActivity == ACT_CROUCHIDLE )
	{
		m_flTargetMaxSpeed = ANIM_TOPSPEED_RUN_CROUCH;
	}
	else
	{
		AssertMsg1( false, "Need to handle the activity %d", currentActivity);
		DevMsg("Need to handle the activity %d\n", (int)currentActivity);
		m_flTargetMaxSpeed = 0.0f;
	}

	// The current max speed smoothly moves toward the TargetMax speed to avoid pops in animation.
	return m_flCurrentMaxSpeed;
}


bool CCSPlayerAnimState::HandleJumping()
{

	if ( m_bJumping )
	{
		if ( m_bFirstJumpFrame )
		{

#if !defined(CLIENT_DLL)
			// [dwenger] Needed for fun-fact implementation
			CCS_GameStats.IncrementStat( m_pPlayer, CSSTAT_TOTAL_JUMPS, 1 );
#endif

			m_bFirstJumpFrame = false;
			RestartMainSequence();	// Reset the animation.
		}

		// Don't check if he's on the ground for a sec.. sometimes the client still has the
		// on-ground flag set right when the message comes in.
		const float GROUND_CHECK_DELAY = 0.2f;
		if ( gpGlobals->curtime - m_flJumpStartTime > GROUND_CHECK_DELAY )
		{
			if ( m_pOuter->GetFlags() & FL_ONGROUND )
			{
				m_flPostLandCrouchEndTime = gpGlobals->curtime + post_jump_crouch.GetFloat();
				m_bJumping = false;
				RestartMainSequence();	// Reset the animation.
			}
		}
	}

	// Are we still jumping? If so, keep playing the jump animation.
	return m_bJumping;
}


Activity CCSPlayerAnimState::CalcMainActivity()
{
	float flOuterSpeed = GetOuterXYSpeed();

	if ( HandleJumping() )
	{
		if ( flOuterSpeed > MOVING_MINIMUM_SPEED )
		{
			if ( flOuterSpeed > ARBITRARY_RUN_SPEED )
				return ACT_LEAP;
			else
				return ACT_JUMP;
		}
		return ACT_HOP;
	}
	else
	{
		int moveState = m_pPlayer->m_iMoveState;
		bool inPostJump = false;

#if defined( CLIENT_DLL )

		inPostJump = ( gpGlobals->curtime < m_flPostLandCrouchEndTime );

#endif

		if ( m_pOuter->GetFlags() & FL_ANIMDUCKING || inPostJump )
		{
			// If the player is trying to run after jumping, we transition directly to the run animation instead of going through the
			// chain of animations based soley on the player's speed.
			if ( inPostJump && ( moveState == MOVESTATE_RUN ) )
			{
				m_bTryingToRunAfterJump = true;
			}

			if ( flOuterSpeed > MOVING_MINIMUM_SPEED )
			{
				return ACT_RUN_CROUCH;
			}

			return ACT_CROUCHIDLE;
		}
		else
		{

			

			if ( m_CurrentActivity == ACT_IDLE || m_CurrentActivity == ACT_TURN || m_CurrentActivity == ACT_STEP_FORE )
			{
				//we aren't showing any movement animation at the moment.

				//but we want to walk or run
				if ( moveState != MOVESTATE_IDLE )
				{
					//we can walk right away, pose params will blend us in nicely
					m_CurrentActivity = ACT_WALK;

					//but we need to be going fast enough to go all the way up to a run
					if ( flOuterSpeed > ARBITRARY_RUN_SPEED )
					{
						m_CurrentActivity = ACT_RUN;
					}
				}
			}
			else if ( m_CurrentActivity == ACT_WALK )
			{
				//we're showing a little movement animation

				//but we want to stop
				if ( moveState == MOVESTATE_IDLE )
				{
					//only stop if we're slow enough
					if ( flOuterSpeed < MOVING_MINIMUM_SPEED && GetTimeSinceLastActChange() > ANIM_ACT_DURATION_BEFORE_RETURN_TO_IDLE )
					{
						m_CurrentActivity = ACT_IDLE;
					}
				}

				//but we want to RUN we need to be moving fast enough
				else if ( moveState == MOVESTATE_RUN && flOuterSpeed > ARBITRARY_RUN_SPEED )
				{
					m_CurrentActivity = ACT_RUN;
				}

			}
			else if ( m_CurrentActivity == ACT_RUN )
			{
				//we're playing the run animation

				//if we want to walk, don't do it until we drop into walk speed
				if ( moveState == MOVESTATE_WALK && flOuterSpeed < ARBITRARY_RUN_SPEED && GetTimeSinceLastActChange() > ANIM_ACT_DURATION_BEFORE_RETURN_TO_WALK )
				{
					m_CurrentActivity = ACT_WALK;
				}
				//and if we want to stop, don't do it until we drop to near-stop speed
				else if ( moveState == MOVESTATE_IDLE && flOuterSpeed < MOVING_MINIMUM_SPEED && GetTimeSinceLastActChange() > ANIM_ACT_DURATION_BEFORE_RETURN_TO_IDLE )
				{
					m_CurrentActivity = ACT_IDLE;
				}

			}


			// As soon as they lift up on trying to run, we no longer force the post run animation after a jump.
			if ( moveState != MOVESTATE_RUN )
			{
				m_bTryingToRunAfterJump = false;
			}

			if ( m_bTryingToRunAfterJump )
			{
				m_CurrentActivity = ACT_RUN;
			}

			UpdateTimeSinceLastActChange();

			return m_CurrentActivity;
		}

	}
}


void CCSPlayerAnimState::DebugShowAnimState( int iStartLine )
{
	engine->Con_NPrintf( iStartLine++, "fire  : %s, cycle: %.2f\n", m_bFiring ? GetSequenceName( m_pOuter->GetModelPtr(), m_iFireSequence ) : "[not firing]", m_flFireCycle );
	engine->Con_NPrintf( iStartLine++, "deploy  : %s, cycle: %.2f\n", m_bDeploying ? GetSequenceName( m_pOuter->GetModelPtr(), m_iDeploySequence ) : "[not deploying]", m_flDeployCycle );
	engine->Con_NPrintf( iStartLine++, "reload: %s, cycle: %.2f\n", m_bReloading ? GetSequenceName( m_pOuter->GetModelPtr(), m_iReloadSequence ) : "[not reloading]", m_flReloadCycle );
	BaseClass::DebugShowAnimState( iStartLine );
}


void CCSPlayerAnimState::ComputeSequences( CStudioHdr *pStudioHdr )
{
	BaseClass::ComputeSequences( pStudioHdr );

	VPROF( "CCSPlayerAnimState::ComputeSequences" );
	
	// dispatched through weapon
	ComputeDeploySequence( pStudioHdr );
	ComputeFireSequence( pStudioHdr );
	ComputeReloadSequence( pStudioHdr );
	ComputeSilencerChangeSequence( pStudioHdr );
	ComputeGrenadeSequence( pStudioHdr );
	// not dispatched through weapon (normally)
	ComputeFlashedSequence( pStudioHdr );
	ComputeFlinchSequence( pStudioHdr );
	ComputeTauntSequence( pStudioHdr );
	ComputeFootPlantSequence(pStudioHdr);
}


void CCSPlayerAnimState::ClearAnimationLayers()
{
	if ( !m_pOuter )
		return;

	m_pOuter->SetNumAnimOverlays( NUM_LAYERS_WANTED );
	for ( int i=0; i < m_pOuter->GetNumAnimOverlays(); i++ )
	{
		ClearAnimationLayer( i );
	}
}


void CCSPlayerAnimState::ComputeFireSequence( CStudioHdr *pStudioHdr )
{
	VPROF( "CCSPlayerAnimState::ComputeFireSequence" );

	if ( m_delayedFire != PLAYERANIMEVENT_COUNT )
	{
		DoAnimationEvent( m_delayedFire, 0 );
		m_delayedFire = PLAYERANIMEVENT_COUNT;
	}

	// firing anims are blended in case the player starts or stops moving during long firing animations

	bool bCrouched = ( m_pOuter->GetFlags() & FL_DUCKING ) ? true : false;

	if (bCrouched)
	{
		UpdateLayerSequenceGeneric( pStudioHdr, FIRESEQUENCE_LAYER, m_bFiring, m_flFireCycle, m_iFireSequence, false );
	}
	else
	{

		float flMaxSpeed = GetCurrentMaxGroundSpeed();
		float flPortionOfMaxSpeed = 0.0f;

		if ( flMaxSpeed > 0 )
		{
			Vector vel;
			GetOuterAbsVelocity( vel );
			flPortionOfMaxSpeed = MIN( vel.Length2D() / flMaxSpeed, 1.0f );
		}	

		if ( flPortionOfMaxSpeed < 1.0f && m_iIdleFireSequence != -1 )
		{
			//if we're at all under top speed, lay down a base layer of the idle (non-moving) firing animation, weighted by that portion of max speed
			UpdateLayerSequenceGeneric( pStudioHdr, FIRESEQUENCE_LAYER, m_bFiring, m_flFireCycle, m_iIdleFireSequence, false, 1.0 - flPortionOfMaxSpeed );
		}

		if ( flPortionOfMaxSpeed > 0 )
		{
			//we are moving and firing, but playing the idle firing animation. Re-evaluate the sequence so we can swap to the walk/crouchwalk/run version.
			if ( m_bFiring && m_iFireSequence == m_iIdleFireSequence )
			{
				m_iFireSequence = CalcFireLayerSequence( m_activeFireEvent );
			}

			// blend in the moving version of this firing animation
			UpdateLayerSequenceGeneric( pStudioHdr, FIRESEQUENCE2_LAYER, m_bFiring, m_flFireCycle, m_iFireSequence, false, flPortionOfMaxSpeed );
		}
	
	}
}

int CCSPlayerAnimState::CalcDeployLayerSequence( void )
{
	// Figure out the weapon suffix.
	const char *pSuffix = GetWeaponSuffix();
	if ( !pSuffix )
		return -1;
	
	//[msmith] NOTE: Once we get all the deploy animations added in, use the line below (CalcSequenceIndex) instead of querying LookupSequence directly.
	//return CalcSequenceIndex( "deploy_%s", pSuffix );
	
	char szName[512];
	Q_snprintf( szName, sizeof( szName ), "deploy_%s", pSuffix );
	return m_pOuter->LookupSequence( szName );
}

void CCSPlayerAnimState::ComputeDeploySequence( CStudioHdr *pStudioHdr )
{
	// We need to base the deploy animation on weapon changes instead of an animation event since animation
	// events are delayed and can cause base animations to be out of sync with the new weapons.	

	// There is a tricky bit here since the weapon gets set before the deploy event.
	// To fix this, while the actively deployed weapon differs from the active weapon,
	// clear out all the other layers. 
	if ( !ActiveWeaponIsDeployed() )
	{
		// Clear out the transition history so that we pop to the new weapon instead of blending to it.
		m_LowAimSequenceTransitioner.RemoveAll();
		m_HighAimSequenceTransitioner.RemoveAll();

		// Resets and turns off any Fire anim that might be playing
		m_bFiring = false;
		m_iFireSequence = -1;
		ClearAnimationLayer( FIRESEQUENCE_LAYER );
				
		// Resets and turns off any Reload anim that might be playing
		m_bReloading = false;
		m_iReloadSequence = -1;
		ClearAnimationLayer( RELOADSEQUENCE_LAYER );

		// Resets and turns off any Silencer attach/detach anim that might be playing
		m_bSilencerChanging = false;
		m_iSilencerChangeSequence = -1;
		ClearAnimationLayer( SILENCERSEQUENCE_LAYER );

		// Resets and turns off any Grenade animations that might be playing.
		m_bThrowingGrenade = false;
		m_bPrimingGrenade = false;
		m_iGrenadeSequence = -1;
		ClearAnimationLayer( GRENADESEQUENCE_LAYER );

	}

	if ( m_bDeploying )
	{
		VPROF( "CCSPlayerAnimState::ComputeDeploySequence" );
		UpdateLayerSequenceGeneric( pStudioHdr, DEPLOYSEQUENCE_LAYER, m_bDeploying, m_flDeployCycle, m_iDeploySequence, false );
		
		// Not deploying anymore so clear out the layer.
		if ( !m_bDeploying )
		{
			ClearAnimationLayer( DEPLOYSEQUENCE_LAYER );
		}
	}
}

float CCSPlayerAnimState::CalcMovementPlaybackRate( bool *bIsMoving )
{
	// Determine ideal playback rate
	Vector vel;
	GetOuterAbsVelocity( vel );

	float speed = vel.Length2D();
	bool isMoving = ( speed > MOVING_MINIMUM_SPEED );

	*bIsMoving = false;
	float flReturnValue = 1;

	if ( isMoving && CanThePlayerMove() )
	{
		if ( m_flCurrentMaxSpeed < 0.001f )
		{
			flReturnValue = 0.01;
		}
		else
		{
			// [msmith]
			// The playback rate is used to blend between idle and moving.
			// If we're fully moving, we want it to be 1.0.
			flReturnValue = speed / m_flCurrentMaxSpeed;

			// We cap this to one so we are never more than fully moving.
			flReturnValue = clamp( flReturnValue, 0.0f, 1.0f );
		}
		*bIsMoving = true;
	}
	
	return flReturnValue;
}

void CCSPlayerAnimState::Update( float eyeYaw, float eyePitch )
{

	// Adjust the maxSpeed toward the targetmax speed.  Doesn't need to be perfect here.
	// We just need to avoid some pops from when the speed suddenly changes.
	const float SPEED_BLEND_VALUE = 0.15f;
	m_flCurrentMaxSpeed += ( m_flTargetMaxSpeed - m_flCurrentMaxSpeed ) * SPEED_BLEND_VALUE;

	if ( m_pPlayer && m_pPlayer->IsTaunting() )
	{
		// Get the studio header for the player.
		CStudioHdr *pStudioHdr = m_pPlayer->GetModelPtr();
		if ( !pStudioHdr )
			return;

		Vector vPositionToFace = vec3_origin; // FIXME: orientation lock target goes here!
		bool bInTaunt = m_pPlayer->IsTaunting();
		bool bIsImmobilized = bInTaunt;
		bool bForceAdjust = bInTaunt; // FIXME: Maybe want this at some point
		bool bIsOrientationLockedOnEntity = false;

		if ( !bIsImmobilized )
		{
			// Pose parameter - what direction are the player's legs running in.
			ComputePoseParam_MoveYaw( pStudioHdr );
		}

		if ( bIsOrientationLockedOnEntity )
		{
			// snap body to align with eye angles
			m_bForceAimYaw = true;

			Vector toOther = vPositionToFace - m_pPlayer->GetAbsOrigin();
			QAngle alignedAngles;
			VectorAngles( toOther, alignedAngles );

			m_flEyeYaw = alignedAngles.y;
			m_flEyePitch = alignedAngles.x;
		}
	//	else if ( bForceAdjust )
	//	{
	//		m_bForceAimYaw = true;
	//		m_flEyeYaw = m_pPlayer->GetTauntYaw();
	//	}
		
		if ( bIsOrientationLockedOnEntity || !bIsImmobilized || bForceAdjust )
		{
			eyePitch = m_flEyePitch;
			eyeYaw = m_flEyeYaw;
		}
	}

	BaseClass::Update(eyeYaw, eyePitch);
}

bool CCSPlayerAnimState::ActiveWeaponIsDeployed()
{
	CWeaponCSBase *pActiveWeapon = m_pHelpers->CSAnim_GetActiveWeapon();
	bool currentWeaponIsDeployedWeapon = true;
	if ( pActiveWeapon != NULL )
	{
		currentWeaponIsDeployedWeapon = ( pActiveWeapon->GetCSWeaponID() == m_iDeployedWeaponID );
		if ( !currentWeaponIsDeployedWeapon )
		{
			// If the player is out of view we don't get animation events about deploy etc.
			// Because of this we use a time out to update the active weapon.
			const float MAX_DEPLOY_DELAY = 1.0f;

			if ( m_flWeaponSwitchTime == 0.0f )
			{
				m_flWeaponSwitchTime = gpGlobals->curtime;
			}
			else if ( m_flWeaponSwitchTime + MAX_DEPLOY_DELAY < gpGlobals->curtime )
			{
				// It has been MAX_DEPLOY_DELAY since the player switched weapons.
				// Go ahead and set the active weapon as the deployed weapon.
				m_iDeployedWeaponID = pActiveWeapon->GetCSWeaponID();
				currentWeaponIsDeployedWeapon = true;
			}
		}
		else
		{
			m_flWeaponSwitchTime = 0.0f;
		}
	}
	return currentWeaponIsDeployedWeapon;	
}
