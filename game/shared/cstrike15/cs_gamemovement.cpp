//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "gamemovement.h"
#include "cs_gamerules.h"
#include "in_buttons.h"
#include "movevars_shared.h"
#include "weapon_csbase.h"


#ifdef CLIENT_DLL
	#include "c_cs_player.h"
	#include "vguicenterprint.h"
#else
	#include "cs_player.h"
	#include "keyvalues.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#define STAMINA_RANGE				100.0
ConVar sv_staminajumpcost( "sv_staminajumpcost", ".080", FCVAR_RELEASE | FCVAR_REPLICATED, "Stamina penalty for jumping", true, 0.0, false, 0.0 );
ConVar sv_staminalandcost( "sv_staminalandcost", ".050", FCVAR_RELEASE | FCVAR_REPLICATED, "Stamina penalty for landing", true, 0.0, false, 0.0 );
ConVar sv_staminarecoveryrate( "sv_staminarecoveryrate", "60", FCVAR_RELEASE | FCVAR_REPLICATED, "Rate at which stamina recovers (units/sec)", true, 0.0, false, 0.0 );
ConVar sv_staminamax( "sv_staminamax", "80", FCVAR_RELEASE | FCVAR_REPLICATED, "Maximum stamina penalty", true, 0.0, true, 100.0 );

#define get_sv_crouch_spam_penalty 2.0f

// Prevent super-fast duck-spam while in air
ConVar sv_timebetweenducks( "sv_timebetweenducks", "0.4", FCVAR_REPLICATED | FCVAR_RELEASE, "Minimum time before recognizing consecutive duck key", true, 0.0, true, 2.0 );

extern bool g_bMovementOptimizations;
extern ConVar sv_accelerate_use_weapon_speed;
extern ConVar sv_accelerate_debug_speed;

#define SV_ACCELERATE_EXPONENT_TIME 0
#define SV_ACCELERATE_EXPONENT 1

// Not ready to ship yet... previous value was 2.0
ConVar sv_extreme_strafe_accuracy_fishtail( "sv_extreme_strafe_accuracy_fishtail", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY, "Number of degrees of aim 'fishtail' when making an extreme strafe direction change", true, -5.0, true, 5.0 );

// sqrt(2 * 800 * 57) = sqrt(2 * gravity * height)
ConVar sv_jump_impulse( "sv_jump_impulse", "301.993377", FCVAR_REPLICATED | FCVAR_RELEASE, "Initial upward velocity for player jumps; sqrt(2*gravity*height).", true, 0.0f, false, 0.0f );

class CCSGameMovement : public CGameMovement
{
public:
	DECLARE_CLASS( CCSGameMovement, CGameMovement );

	CCSGameMovement();

	virtual void ProcessMovement( CBasePlayer *pPlayer, CMoveData *pMove );
	virtual bool CanAccelerate();
	virtual bool CheckJumpButton( void );
	virtual void PreventBunnyJumping( void );
	virtual void ReduceTimers( void );
	virtual void WalkMove( void );
	virtual void AirMove( void );
	virtual bool LadderMove( void );
	virtual void DecayAimPunchAngle( void );
	virtual void CheckParameters( void );

	// allow overridden versions to respond to jumping
	virtual void	OnJump( float fImpulse );
	virtual void	OnLand( float fVelocity );

	// Ducking
	virtual void Duck( void );
	virtual void FinishUnDuck( void );
	virtual void FinishDuck( void );
	virtual bool CanUnduck();

	virtual void Accelerate( Vector& wishdir, float wishspeed, float accel);

	virtual bool OnLadder( trace_t &trace );
	virtual float LadderDistance( void ) const
	{
		if ( player->GetMoveType() == MOVETYPE_LADDER )
			return 10.0f;
		return 2.0f;
	}

	virtual unsigned int LadderMask( void ) const
	{
		return MASK_PLAYERSOLID & ( ~CONTENTS_PLAYERCLIP );
	}

	virtual float ClimbSpeed( void ) const;
	virtual float LadderLateralMultiplier( void ) const;

protected:
	virtual void PlayerMove();

	virtual unsigned int PlayerSolidMask( bool brushOnly = false, CBasePlayer *testPlayer = NULL ) const;	///< returns the solid mask for the given player, so bots can have a more-restrictive set

public:
	CCSPlayer *m_pCSPlayer;

private:
	void HandleDuckingSpeedCrop( float duckFraction );
	inline float GetDuckSpeedModifier( float duckFraction ) const { return CS_PLAYER_SPEED_DUCK_MODIFIER * duckFraction + 1.0f - duckFraction; }
	bool DuckingEnabled();
};


// Expose our interface.
static CCSGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameMovement, IGameMovement,INTERFACENAME_GAMEMOVEMENT, g_GameMovement );


// ---------------------------------------------------------------------------------------- //
// CCSGameMovement.
// ---------------------------------------------------------------------------------------- //

CCSGameMovement::CCSGameMovement()
{
}

//-----------------------------------------------------------------------------
// Purpose: Allow bots etc to use slightly different solid masks
//-----------------------------------------------------------------------------
unsigned int CCSGameMovement::PlayerSolidMask( bool brushOnly, CBasePlayer *testPlayer ) const
{
	bool isBot = !player || player->IsBot();
	unsigned int uMask = MASK_PLAYERSOLID_BRUSHONLY;

	if ( !brushOnly )
	{
		uMask = player->PhysicsSolidMaskForEntity();
	}

	if ( isBot )
	{
		uMask |= CONTENTS_MONSTERCLIP;
	}

	return uMask;
}


bool CCSGameMovement::DuckingEnabled()
{
	// Anti-duck-spam: If mashing duck key too much, treat as unpressed to avoid being
	// able to "camp" in half-ducked positions by pressing/releasing near a target height.
	if ( m_pCSPlayer->m_flDuckSpeed < 1.5f )
		return false;

	// After completing a duck/unduck, we can't re-duck for a minimum amount of time.
	// This is to minimize ugly camera shaking due to immediate duck/un-duck in the air.
	// before the above anti-spam code kicks in.
	if ( ( player->GetFlags() & FL_DUCKING ) == 0
		 && gpGlobals->curtime < player->m_Local.m_flLastDuckTime + sv_timebetweenducks.GetFloat() )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCSGameMovement::CheckParameters( void )
{
	// HACK: IN_BULLRUSH isn't used anywhere.  We use it to store the "raw" value of whether
	//       IN_DUCK was held, before it is modified by code.  This way when m_nButtons is
	//       copied to m_nOldButtons we can tell if duck was pressed or released on the next
	//       frame.
	#define IN_RAWDUCK IN_BULLRUSH

	QAngle	v_angle;

	// Save off state of IN_DUCK before we set/unset it from gameplay effects
	if ( ( mv->m_nButtons & IN_DUCK ) != 0 )
		mv->m_nButtons |= IN_RAWDUCK;

	// Add duck-spam speed penalty when we press or release the duck key.
	// (this is the only place that IN_RAWDUCK is checked)
	if ( ( mv->m_nButtons & IN_RAWDUCK ) != ( mv->m_nOldButtons & IN_RAWDUCK ) )
	{
		player->m_flDuckSpeed = MAX( 0, player->m_flDuckSpeed - get_sv_crouch_spam_penalty );
	}

	// Cancel auto-duck if player hits +duck, so cancel our auto duck
	// Note: only bots use m_duckUntilOnGround
	if ( m_pCSPlayer->m_duckUntilOnGround && ( mv->m_nButtons & IN_DUCK ) )
	{
		m_pCSPlayer->m_duckUntilOnGround = false;
	}

	// Anti-duck-spam: If mashing duck key too much, treat as unpressed to avoid being
	// able to "camp" in half-ducked positions by pressing/releasing near a target height.
	if ( !DuckingEnabled() )
	{
		mv->m_nButtons &= ~IN_DUCK;
	}

	// maintaining auto-duck during jumps
	// Note: only bots use m_duckUntilOnGround
	if ( m_pCSPlayer->m_duckUntilOnGround && !player->GetGroundEntity() && player->GetMoveType() != MOVETYPE_LADDER )
	{
		mv->m_nButtons |= IN_DUCK;
	}

	// Always duck during bomb-plant animation
	if ( m_pCSPlayer->m_bDuckOverride )
	{
		mv->m_nButtons |= IN_DUCK;
	}

	// it would be nice to put this into the player->GetPlayerMaxSpeed() method, but
	// this flag is only stored in the move!

	bool walkButtonIsDown = ( mv->m_nButtons & ( /*IN_WALK | */ IN_SPEED ) ) != 0;
	bool runButtonIsDown = ( mv->m_nButtons & ( IN_FORWARD | IN_BACK | IN_MOVERIGHT | IN_MOVELEFT | IN_RUN) ) != 0;
	bool moveForward = ( mv->m_nButtons & ( IN_FORWARD ) ) != 0;
	bool moveBackward = ( mv->m_nButtons & ( IN_BACK ) ) != 0;
	bool moveRight = ( mv->m_nButtons & ( IN_MOVERIGHT ) ) != 0;
	bool moveLeft = ( mv->m_nButtons & ( IN_MOVELEFT ) ) != 0;
	bool opposingForwardBack = ( moveForward && moveBackward );
	bool opposingRightLeft = ( moveRight && moveLeft );

	// NOTE[pmf] ignore the walk button when we're ducking; this test matches that in CCSGameMovement::HandleDuckingSpeedCrop()
	// we can't simply test mv->m_iSpeedCropped == SPEED_CROPPED_DUCK, because that is set AFTER this code executes
	if ( ( mv->m_nButtons & IN_DUCK ) || ( player->m_Local.m_bDucking ) || ( player->GetFlags() & FL_DUCKING ) )
	{
		walkButtonIsDown = false;
	}

	if ( walkButtonIsDown )
	{
		// we don't cap walk immediately, let the player decelerate and only cap when the speed is really close
		float currentspeed = m_pCSPlayer->GetLocalVelocity( ).Length( );
		if ( currentspeed < ( ( mv->m_flMaxSpeed * CS_PLAYER_SPEED_WALK_MODIFIER ) + 25 ) )
		{
			mv->m_flMaxSpeed *= CS_PLAYER_SPEED_WALK_MODIFIER;
			m_pCSPlayer->m_bIsWalking = true;
		}
	}
	else
	{
		m_pCSPlayer->m_bIsWalking = false;
	}

	float speed_squared = 0.0f;

	if ( player->GetMoveType() != MOVETYPE_ISOMETRIC &&
		 player->GetMoveType() != MOVETYPE_NOCLIP &&
		 player->GetMoveType() != MOVETYPE_OBSERVER	)
	{
		speed_squared = ( mv->m_flForwardMove * mv->m_flForwardMove ) +
			  ( mv->m_flSideMove * mv->m_flSideMove ) +
			  ( mv->m_flUpMove * mv->m_flUpMove );


		// Slow down by the speed factor
		float flSpeedFactor = 1.0f;
		if (player->m_pSurfaceData)
		{
			flSpeedFactor = player->m_pSurfaceData->game.maxSpeedFactor;
		}

		// If we have a constraint, slow down because of that too.
		float flConstraintSpeedFactor = ComputeConstraintSpeedFactor();
		if (flConstraintSpeedFactor < flSpeedFactor)
			flSpeedFactor = flConstraintSpeedFactor;

		// Take the player's velocity modifier into account
		if ( FBitSet( m_pCSPlayer->GetFlags(), FL_ONGROUND ) )
		{
			flSpeedFactor *= m_pCSPlayer->m_flVelocityModifier;
		}

		mv->m_flMaxSpeed *= flSpeedFactor;

		// stamina slowing factor
		if ( m_pCSPlayer->m_flStamina > 0 )
		{
			float fSpeedScale = clamp(1.0f - m_pCSPlayer->m_flStamina / STAMINA_RANGE, 0.f, 1.f);
			fSpeedScale *= fSpeedScale;	// square the scale factor so that it correlates more closely with the jump penalty (i.e. a 50% stamina jumps .25 * normal height)
			mv->m_flMaxSpeed *= fSpeedScale;
		}

		if ( g_bMovementOptimizations )
		{
			// Same thing but only do the sqrt if we have to.
			if ( ( speed_squared != 0.0 ) && ( speed_squared > mv->m_flMaxSpeed*mv->m_flMaxSpeed ) )
			{
				float fRatio = mv->m_flMaxSpeed / sqrt( speed_squared );
				mv->m_flForwardMove *= fRatio;
				mv->m_flSideMove    *= fRatio;
				mv->m_flUpMove      *= fRatio;
			}
		}
		else
		{
			float spd = sqrt( speed_squared );
			if ( ( spd != 0.0 ) && ( spd > mv->m_flMaxSpeed ) )
			{
				float fRatio = mv->m_flMaxSpeed / spd;
				mv->m_flForwardMove *= fRatio;
				mv->m_flSideMove    *= fRatio;
				mv->m_flUpMove      *= fRatio;
			}
		}
	}
	
	if ( player->GetFlags() & FL_FROZEN ||
		 player->GetFlags() & FL_ONTRAIN || 
		 IsDead() )
	{
		if ( player->GetObserverMode() != OBS_MODE_ROAMING )
		{
			mv->m_flForwardMove = 0;
			mv->m_flSideMove    = 0;
			mv->m_flUpMove      = 0;
		}
	}

	DecayViewPunchAngle();
	DecayAimPunchAngle();

	// Take angles from command.
	if ( !IsDead() )
	{
		v_angle = mv->m_vecAngles;
		v_angle = v_angle + player->m_Local.m_viewPunchAngle;

		// Now adjust roll angle
		if ( player->GetMoveType() != MOVETYPE_ISOMETRIC  &&
			 player->GetMoveType() != MOVETYPE_NOCLIP )
		{
			mv->m_vecAngles[ROLL]  = CalcRoll( v_angle, mv->m_vecVelocity, sv_rollangle.GetFloat(), sv_rollspeed.GetFloat() );
		}
		else
		{
			mv->m_vecAngles[ROLL] = 0.0; // v_angle[ ROLL ];
		}
		mv->m_vecAngles[PITCH] = v_angle[PITCH];
		mv->m_vecAngles[YAW]   = v_angle[YAW];
	}
	else
	{
		mv->m_vecAngles = mv->m_vecOldAngles;
	}

	// Set dead player view_offset
	if ( IsDead() && 
		 player->GetObserverMode() == OBS_MODE_DEATHCAM )
	{
		player->SetViewOffset( VEC_DEAD_VIEWHEIGHT );
	}

	// Adjust client view angles to match values used on server.
	mv->m_vecAngles[YAW] = AngleNormalize( mv->m_vecAngles[YAW] );

	// If we're standing on a player, then force them off.
	if ( !player->IsObserver()  && ( player->GetMoveType() != MOVETYPE_LADDER ) )
	{
		int nLevels = 0;
		CBaseEntity *pCurGround = player->GetGroundEntity();
		while ( pCurGround && pCurGround->IsPlayer() && nLevels < 1000 )
		{
			pCurGround = pCurGround->GetGroundEntity();
			++nLevels;
		}
		if ( nLevels == 1000 )
			Warning( "BUG: CCSGameMovement::CheckParameters - too many stacking levels.\n" );

		// If they're stacked too many levels deep, slide them off.
		if ( nLevels > 1 )
		{
			mv->m_flForwardMove = mv->m_flMaxSpeed * 3;
			mv->m_flSideMove = 0;
			mv->m_nButtons = 0;
			mv->m_nImpulseCommand = 0;
		}
	}


	// Determine if the player is trying to run / move at full speed.
	m_pCSPlayer->m_iMoveState = MOVESTATE_IDLE; // idle, not driving the character
	if ( runButtonIsDown )
	{
		if ( opposingForwardBack && opposingRightLeft )
		{
			m_pCSPlayer->m_iMoveState = MOVESTATE_IDLE; // Idle, don't move if we are holding all 4 directions down.
		}
		else if ( opposingForwardBack || opposingRightLeft )
		{
			if ( ( opposingForwardBack && ( moveRight || moveLeft ) ) || ( opposingRightLeft && ( moveForward || moveBackward ) ) )
			{
				m_pCSPlayer->m_iMoveState = MOVESTATE_RUN; // Run, move if we are holding 3 buttons down, 2 are opposing directions.
			}
			else
			{
				m_pCSPlayer->m_iMoveState = MOVESTATE_IDLE; // Idle, don't move if we are holding just 2 opposing directions down.
			}

		}
		else
		{
			m_pCSPlayer->m_iMoveState = MOVESTATE_RUN; // Run
		}
	}

	if ( ( m_pCSPlayer->m_iMoveState == MOVESTATE_RUN ) && walkButtonIsDown )
	{
		m_pCSPlayer->m_iMoveState = MOVESTATE_WALK; // Walk
	}

}


void CCSGameMovement::ProcessMovement( CBasePlayer *pBasePlayer, CMoveData *pMove )
{
	m_pCSPlayer = static_cast<CCSPlayer *>( pBasePlayer );
	
	if ( m_pCSPlayer->IsBot() && m_pCSPlayer->IsDormant() )
		return;

	BaseClass::ProcessMovement( pBasePlayer, pMove );
}


bool CCSGameMovement::CanAccelerate()
{
	// Only allow the player to accelerate when in certain states.
	CSPlayerState curState = m_pCSPlayer->State_Get();
	if ( curState == STATE_ACTIVE )
	{
		return player->GetWaterJumpTime() == 0;
	}
	else if ( player->IsObserver() )
	{
		return true;
	}
	else
	{	
		return false;
	}
}


void CCSGameMovement::PlayerMove()
{
	if ( !m_pCSPlayer->CanMove() )
	{
		mv->m_flForwardMove = 0;
		mv->m_flSideMove = 0;
		mv->m_flUpMove = 0;
		mv->m_nButtons &= ~(IN_JUMP | IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT);
	}

	BaseClass::PlayerMove();

	if ( FBitSet( m_pCSPlayer->GetFlags(), FL_ONGROUND ) )
	{
		const float kVelocityRecoveryRate = 1.f / 2.5f;
		if ( m_pCSPlayer->m_flVelocityModifier < 1.0 )
		{
			m_pCSPlayer->m_flVelocityModifier = clamp(m_pCSPlayer->m_flVelocityModifier + gpGlobals->frametime * kVelocityRecoveryRate, 0.0f, 1.0f);
		}
	}

#if !defined(CLIENT_DLL)
	if( player && 
		player->GetTeamNumber() == TEAM_SPECTATOR &&
		player->GetObserverMode() == OBS_MODE_FIXED )
	{
		// [dkorus] if we're using a fixed view mode when spectating, we need to make sure we bump the camera up off the ground
		player->SetViewOffset( VEC_VIEW );
	}

	if ( m_pCSPlayer->IsAlive() )
	{
		if ( ( player->GetFlags() & FL_DUCKING ) == 0 && !player->m_Local.m_bDucking && !player->m_Local.m_bDucked )
		{
			player->SetViewOffset( VEC_VIEW );
		}
		else if ( m_pCSPlayer->m_duckUntilOnGround )
		{
			// Duck Hull, but we're in the air.  Calculate where the view would be.
			Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
			Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;

			// We've got the duck hull, pulled up to the top of where the player should be
			Vector lowerClearance = hullSizeNormal - hullSizeCrouch;
			Vector duckEyeHeight = GetPlayerViewOffset( false ) - lowerClearance;
			player->SetViewOffset( duckEyeHeight );
		}
		else if( player->m_Local.m_bDucked && !player->m_Local.m_bDucking )
		{
			player->SetViewOffset( VEC_DUCK_VIEW );
		}
	}

	if ( !m_pCSPlayer->m_bHasMovedSinceSpawn && Vector2DLength( mv->m_vecVelocity.AsVector2D() ) != 0  )
		m_pCSPlayer->m_bHasMovedSinceSpawn = true;

#endif	

}


void CCSGameMovement::WalkMove( void )
{
	BaseClass::WalkMove();
}


//-------------------------------------------------------------------------------------------------------------------------------
void CCSGameMovement::AirMove( void )
{
	BaseClass::AirMove();
}


//-------------------------------------------------------------------------------------------------------------------------------
bool CCSGameMovement::OnLadder( trace_t &trace )
{
	if ( trace.plane.normal.z == 1.0f )
		return false;

	return BaseClass::OnLadder( trace );
}


//-------------------------------------------------------------------------------------------------------------------------------
bool CCSGameMovement::LadderMove( void )
{
	bool isOnLadder = BaseClass::LadderMove();
	if ( isOnLadder && m_pCSPlayer )
	{
		m_pCSPlayer->SurpressLadderChecks( mv->GetAbsOrigin(), m_pCSPlayer->m_vecLadderNormal );
	}

	return isOnLadder;
}


//-------------------------------------------------------------------------------------------------------------------------------
/**
 * In CS, crouching or walking up ladders goes slowly and shouldn't make a sound.
 */
float CCSGameMovement::ClimbSpeed( void ) const
{
	if ( ( mv->m_nButtons & IN_DUCK ) || ( mv->m_nButtons & IN_SPEED ) )
	{
		return BaseClass::ClimbSpeed() * CS_PLAYER_SPEED_CLIMB_MODIFIER;
	}
	else
	{
		return BaseClass::ClimbSpeed();
	}
}


//-------------------------------------------------------------------------------------------------------------------------------
/**
* In CS, strafing on ladders goes slowly.
*/
float CCSGameMovement::LadderLateralMultiplier( void ) const
{
	if ( mv->m_nButtons & IN_DUCK )
	{
		return 1.0f;
	}
	else
	{
		return 0.5f;
	}
}

void CCSGameMovement::ReduceTimers( void )
{
	if ( m_pCSPlayer->m_flStamina > 0 )
	{
		m_pCSPlayer->m_flStamina -= gpGlobals->frametime * sv_staminarecoveryrate.GetFloat();

		if ( m_pCSPlayer->m_flStamina < 0 )
		{
			m_pCSPlayer->m_flStamina = 0;
		}
	}

	BaseClass::ReduceTimers();
}

ConVar sv_enablebunnyhopping( "sv_enablebunnyhopping", "0", FCVAR_RELEASE | FCVAR_REPLICATED, "Allow player speed to exceed maximum running speed" );
ConVar sv_autobunnyhopping( "sv_autobunnyhopping", "0", FCVAR_RELEASE | FCVAR_REPLICATED, "Players automatically re-jump while holding jump button" );

// Only allow bunny jumping up to 1.1x server / player maxspeed setting
#define BUNNYJUMP_MAX_SPEED_FACTOR 1.1f

// taken from TF2 but changed BUNNYJUMP_MAX_SPEED_FACTOR from 1.1 to 1.0
void CCSGameMovement::PreventBunnyJumping()
{
	// Speed at which bunny jumping is limited
	float maxscaledspeed = BUNNYJUMP_MAX_SPEED_FACTOR * player->m_flMaxspeed;
	if ( maxscaledspeed <= 0.0f )
		return;

	// Current player speed
	float spd = mv->m_vecVelocity.Length();

	if ( spd <= maxscaledspeed )
		return;

	// Apply this cropping fraction to velocity
	float fraction = ( maxscaledspeed / spd );

	mv->m_vecVelocity *= fraction;


}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCSGameMovement::CheckJumpButton( void )
{
	if (m_pCSPlayer->pl.deadflag)
	{
		mv->m_nOldButtons |= IN_JUMP ;	// don't jump again until released
		return false;
	}

	// See if we are waterjumping.  If so, decrement count and return.
	if (m_pCSPlayer->m_flWaterJumpTime)
	{
		m_pCSPlayer->m_flWaterJumpTime -= gpGlobals->frametime;
		if (m_pCSPlayer->m_flWaterJumpTime < 0)
			m_pCSPlayer->m_flWaterJumpTime = 0;
		
		return false;
	}

	// Can't jump while in a thirdperson taunt
	if ( m_pCSPlayer->IsTaunting() && m_pCSPlayer->IsThirdPersonTaunt() )
		return false;

//#if !defined(CLIENT_DLL)
//	if ( m_pCSPlayer->IsTaunting() )
//	{
//		// They jumped so break out of the taunt!
//		m_pCSPlayer->StopTaunting();
//	}
//#endif

	// If we are in the water most of the way...
	if ( m_pCSPlayer->GetWaterLevel() >= WL_Waist )
	{	
		// swimming, not jumping
		SetGroundEntity( NULL );

		if(m_pCSPlayer->GetWaterType() == CONTENTS_WATER)    // We move up a certain amount
			mv->m_vecVelocity[2] = 100;
		else if (m_pCSPlayer->GetWaterType() == CONTENTS_SLIME)
			mv->m_vecVelocity[2] = 80;
		
		// play swiming sound
		if ( m_pCSPlayer->m_flSwimSoundTime <= 0 )
		{
			// Don't play sound again for 1 second
			m_pCSPlayer->m_flSwimSoundTime = 1000;
			PlaySwimSound();
		}

		return false;
	}

	// are we jumping on another player's head?
	bool bStandingOnOtherPlayer = false;
	bool bStandingOnFallingPlayer = false;
	CBaseEntity *groundEntity = m_pCSPlayer->GetGroundEntity();
	if ( groundEntity && !groundEntity->IsWorld() && groundEntity->IsPlayer() )
	{
		bStandingOnOtherPlayer = true;
		if ( groundEntity->GetGroundEntity() == NULL )
		{
			bStandingOnFallingPlayer = true;
		}
	}

	// the player jumped so this bool will remain false until the player next walks
	player->m_bHasWalkMovedSinceLastJump = false;

	// No more effect
 	if (m_pCSPlayer->GetGroundEntity() == NULL)
	{
		mv->m_nOldButtons |= IN_JUMP;
		return false;		// in air, so no effect
	}

	if ( ( mv->m_nOldButtons & IN_JUMP ) != 0 && !sv_autobunnyhopping.GetBool() )
		return false;		// don't pogo stick

	if ( !sv_enablebunnyhopping.GetBool() )
	{
		PreventBunnyJumping();
	}

	// In the air now.
	SetGroundEntity( NULL );

	// if we're walking or standing still, play only a local sounds
	if ( mv->m_vecVelocity.Length() > 126 )
	{
		m_pCSPlayer->PlayStepSound( (Vector &)mv->GetAbsOrigin(), player->m_pSurfaceData, 1.0, true );
	}

#ifdef CLIENT_DLL
	m_pCSPlayer->PlayClientJumpSound();
#endif

	//MoveHelper()->PlayerSetAnimation( PLAYER_JUMP );
	m_pCSPlayer->DoAnimationEvent( PLAYERANIMEVENT_JUMP );

	float flGroundFactor = 1.0f;
	if (player->m_pSurfaceData)
	{
		flGroundFactor = player->m_pSurfaceData->game.jumpFactor; 
	}

	// if we weren't ducking, bots and hostages do a crouchjump programatically
	if ( (!player || player->IsBot()) && !(mv->m_nButtons & IN_DUCK) )
	{
		m_pCSPlayer->m_duckUntilOnGround = true;
		FinishDuck();
	}

	// Acclerate upward
	// If we are ducking...
	float startz = mv->m_vecVelocity[2];
	if ( bStandingOnFallingPlayer )
	{
		mv->m_vecVelocity[2] = 0.0f;
	}
	else if ( m_pCSPlayer->m_duckUntilOnGround || (  m_pCSPlayer->m_Local.m_bDucking ) || (  m_pCSPlayer->GetFlags() & FL_DUCKING ) || bStandingOnOtherPlayer )
	{
		// d = 0.5 * g * t^2		- distance traveled with linear accel
		// t = sqrt(2.0 * 45 / g)	- how long to fall 45 units
		// v = g * t				- velocity at the end (just invert it to jump up that high)
		// v = g * sqrt(2.0 * 45 / g )
		// v^2 = g * g * 2.0 * 45 / g
		// v = sqrt( g * 2.0 * 45 )
		
		mv->m_vecVelocity[2] = flGroundFactor * sv_jump_impulse.GetFloat();  // 2 * gravity * height
	}
	else
	{
		mv->m_vecVelocity[2] += flGroundFactor * sv_jump_impulse.GetFloat();  // 2 * gravity * height
	}

	if ( m_pCSPlayer->m_flStamina > 0 )
	{
		mv->m_vecVelocity[2] *= clamp(1.0f - m_pCSPlayer->m_flStamina / STAMINA_RANGE, 0.f, 1.f);
	}

	FinishGravity();

	mv->m_outWishVel.z += mv->m_vecVelocity[2] - startz;
	mv->m_outStepHeight += 0.1f;

	OnJump(mv->m_outWishVel.z);

#ifndef CLIENT_DLL
	// allow bots to react
	IGameEvent * event = gameeventmanager->CreateEvent( "player_jump" );
	if ( event )
	{
		event->SetInt( "userid", m_pCSPlayer->GetUserID() );
		gameeventmanager->FireEvent( event );
	}
#endif

	// Flag that we jumped.
	mv->m_nOldButtons |= IN_JUMP;	// don't jump again until released
	return true;
}


void HybridDecay( QAngle& v, float fExp, float fLin, float dT )
{
	fExp *= dT;
	fLin *= dT;

	v *= expf(-fExp);

	float fMag = v.Length();
	if ( fMag > fLin )
	{
		v *= (1.0f - fLin / fMag);
	}
	else
	{
		v.Init(0.0f, 0.0f, 0.0f);
	}
}

void CCSGameMovement::DecayAimPunchAngle( void )
{
	QAngle punchAngle = m_pCSPlayer->m_Local.m_aimPunchAngle;
	QAngle punchAngleVel = m_pCSPlayer->m_Local.m_aimPunchAngleVel;

	// decay the punch angle
	HybridDecay(punchAngle, weapon_recoil_decay2_exp.GetFloat(), weapon_recoil_decay2_lin.GetFloat(), TICK_INTERVAL);

	// add in the velocity
	punchAngle += punchAngleVel * TICK_INTERVAL * 0.5f;

	// decay the punch angle velocity
	punchAngleVel *= expf(TICK_INTERVAL * -weapon_recoil_vel_decay.GetFloat());

	punchAngle += punchAngleVel * TICK_INTERVAL * 0.5f;

	// save off the new values
	m_pCSPlayer->m_Local.m_aimPunchAngle = punchAngle;
	m_pCSPlayer->m_Local.m_aimPunchAngleVel = punchAngleVel;
}

void CCSGameMovement::HandleDuckingSpeedCrop( float duckFraction )
{
	// [Forrest] Movement speed in free look camera mode is unaffected by ducking state.
	if ( player->GetObserverMode() == OBS_MODE_ROAMING )
		return;

	if ( !( m_iSpeedCropped & SPEED_CROPPED_DUCK ) )
	{
		if ( ( mv->m_nButtons & IN_DUCK ) || ( player->m_Local.m_bDucking ) || ( player->GetFlags() & FL_DUCKING ) )
		{
			float duckSpeedModifier = GetDuckSpeedModifier(duckFraction);
			//DevMsg( "duckSpeedModifier = %f\n", duckSpeedModifier );
			mv->m_flForwardMove	*= duckSpeedModifier;
			mv->m_flSideMove	*= duckSpeedModifier;
			mv->m_flUpMove		*= duckSpeedModifier;
			mv->m_flMaxSpeed	*= duckSpeedModifier;
			m_iSpeedCropped		|= SPEED_CROPPED_DUCK;
		}
	}
}

bool CCSGameMovement::CanUnduck()
{
	// Can't unduck if we are planting the bomb.
	if ( m_pCSPlayer->m_bDuckOverride )
		return false;

	// Can always unduck if we are no-clipping
	if ( player->GetMoveType() == MOVETYPE_NOCLIP )
		return true;
	
	// Check to see if we would collide on anything if we unducked.
	trace_t trace;
	Vector newOrigin;

	VectorCopy( mv->GetAbsOrigin(), newOrigin );

	if ( player->GetGroundEntity() != NULL )
	{
		newOrigin += VEC_DUCK_HULL_MIN - VEC_HULL_MIN;
	}
	else
	{
		// If in air an letting go of crouch, make sure we can offset origin to make
		//  up for uncrouching
 		Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
		
		newOrigin += -0.5f * ( hullSizeNormal - hullSizeCrouch );
	}

	UTIL_TraceHull( mv->GetAbsOrigin(), newOrigin, VEC_HULL_MIN, VEC_HULL_MAX, PlayerSolidMask(), player, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

	if ( trace.startsolid || ( trace.fraction != 1.0f ) )
		return false;	

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Stop ducking
//-----------------------------------------------------------------------------
void CCSGameMovement::FinishUnDuck( void )
{
	Vector newOrigin = mv->GetAbsOrigin();
	if ( player->GetGroundEntity() != NULL || player->GetMoveType() == MOVETYPE_LADDER )
	{
		Vector hullMinDelta = VEC_DUCK_HULL_MIN - VEC_HULL_MIN;
		newOrigin += hullMinDelta;
	}
	else
	{
		// If in air an letting go of croush, make sure we can offset origin to make
		//  up for uncrouching
		Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
		Vector viewDelta = -0.5f * ( hullSizeNormal - hullSizeCrouch );
		newOrigin += viewDelta;
	}

	mv->SetAbsOrigin( newOrigin );

	player->RemoveFlag( FL_DUCKING | FL_ANIMDUCKING );
	player->m_Local.m_bDucked = false;
	player->m_Local.m_bDucking = false;
	player->m_Local.m_nDuckTimeMsecs = 0; // legacy
	player->SetViewOffset( GetPlayerViewOffset( false ) );

	// Recategorize position since ducking can change origin
	CategorizePosition();

	player->m_flDuckAmount = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Finish ducking
//-----------------------------------------------------------------------------
void CCSGameMovement::FinishDuck( void )
{
	Assert( !player->m_Local.m_bDucked );

	Vector newOrigin = mv->GetAbsOrigin();

	if ( player->GetGroundEntity() != NULL || player->GetMoveType() == MOVETYPE_LADDER )
	{
		Vector hullMinDelta = VEC_DUCK_HULL_MIN - VEC_HULL_MIN;
		newOrigin -= hullMinDelta;
	}
	else
	{
		Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
		Vector viewDelta = -0.5f * ( hullSizeNormal - hullSizeCrouch );
		newOrigin -= viewDelta;
	}
	mv->SetAbsOrigin( newOrigin );

	player->SetViewOffset( GetPlayerViewOffset( true ) );
	player->m_Local.m_bDucking = false;
	player->m_Local.m_bDucked = true;
	player->m_Local.m_flLastDuckTime = gpGlobals->curtime;
	player->AddFlag( FL_ANIMDUCKING | FL_DUCKING );

	// See if we are stuck?
	FixPlayerCrouchStuck( true );

	// Recategorize position since ducking can change origin
	CategorizePosition();

	player->m_flDuckAmount = 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: See if duck button is pressed and do the appropriate things
//-----------------------------------------------------------------------------
void CCSGameMovement::Duck( void )
{
	const bool playerTouchingGround = player->GetGroundEntity() != NULL;

	// Check to see if we are in the air.
	const bool bInAir = !playerTouchingGround && player->GetMoveType() != MOVETYPE_LADDER;

	if ( mv->m_nButtons & IN_DUCK )
	{
		mv->m_nOldButtons |= IN_DUCK;
	}
	else
	{
		mv->m_nOldButtons &= ~IN_DUCK;
	}

	// Dead players don't duck.
	if ( IsDead() && !player->IsObserver() )
	{
		// They also don't plant the bomb.
		m_pCSPlayer->m_bDuckOverride = false;

		if ( player->GetFlags() & FL_DUCKING )
		{
			FinishUnDuck();
		}

		return;
	}

	if ( m_pCSPlayer->m_duckUntilOnGround )
	{
		// This code handles the case where a bot is jumping; they
		// automatically crouch jump, and we want to decide if they are
		// ready to un-duck here.

		// TODO: Should we move this code into the bot movement logic
		//       instead?

		// $$$REI There still seems to be a way to end up in this state if the bot
		//        was crouch-jumping at the end of the round; I haven't found where
		//        his flags are getting reset.  Will fix later, and just reset the
		//        inconsistent state here.  Next tick the bot will behave normally.
		// Assert( player->GetFlags() & FL_DUCKING );
		if ( ( player->GetFlags() & FL_DUCKING ) == 0 )
		{
			m_pCSPlayer->m_duckUntilOnGround = false;
			return;
		}

		// If we have landed, we are done with 'duck until on ground'.
		if ( !bInAir )
		{
			m_pCSPlayer->m_duckUntilOnGround = false;

			// Stop crouching if possible
			if ( CanUnduck() )
			{
				FinishUnDuck();
			}

			return;
		}

		// Otherwise we are still in the air.
		// Try to un-duck just as we land, for better animation and movement.

		// If we're still going up, we aren't about to land.  Early-out.
		if ( mv->m_vecVelocity.z > 0.0f )
			return;

		// Check if we are close enough to the ground and that there is room to un-duck.
		trace_t trace;
		Vector newOrigin;
		Vector groundCheck;

		VectorCopy( mv->GetAbsOrigin(), newOrigin );
		Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
		Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;
		newOrigin -= ( hullSizeNormal - hullSizeCrouch );
		groundCheck = newOrigin;
		groundCheck.z -= player->GetStepSize();

		UTIL_TraceHull( newOrigin, groundCheck, VEC_HULL_MIN, VEC_HULL_MAX, PlayerSolidMask(), player, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );
		if ( trace.startsolid			// No room to unduck.
			|| trace.fraction == 1.0f	// We are still in the air
			)
			return;

		// Success!  We can un-duck.  Remove "un-duck when possible" flag.
		m_pCSPlayer->m_duckUntilOnGround = false;

		// Theoretically CanUnduck() should always succeed here since we just did a hull trace.
		// REI: But the hulltrace in CanUnduck() looks slightly different than this one; it uses
		//         newOrigin = mv->GetAbsOrigin() + -0.5f * ( hullSizeNormal - hullSizeCrouch )
		//      and traces from mv->GetAbsOrigin() to newOrigin instead of from newOrigin to a
		//      step away.
		if ( CanUnduck() )
		{
			FinishUnDuck();
		}

		return;
	}

	// Reduce duck-spam penalty over time
	player->m_flDuckSpeed = Approach( CS_PLAYER_DUCK_SPEED_IDEAL, player->m_flDuckSpeed, gpGlobals->frametime * 3.0f );

	// Use the last-known position of full crouch speed to restore crouch speed as a function of physical player position.
	// The goal is that moving a sufficient distance should reset crouch speed in an intuitive manner.
	if ( player->m_flDuckSpeed >= CS_PLAYER_DUCK_SPEED_IDEAL )
	{
		player->m_vecLastPositionAtFullCrouchSpeed = player->GetAbsOrigin().AsVector2D();
	}
	else if ( player->m_flDuckAmount <= 0 || player->m_flDuckAmount >= 1 )
	{
		//debugoverlay->AddLineOverlay( player->m_vecLastPositionAtFullCrouchSpeed, player->GetAbsOrigin(), 255,0,0, true, 0.1f );
		//debugoverlay->AddTextOverlay( player->GetAbsOrigin(), 0.1f, "%f", player->m_flDuckSpeed );

		float flDistToLastPositionAtFullCrouchSpeed = player->m_vecLastPositionAtFullCrouchSpeed.DistToSqr( player->GetAbsOrigin().AsVector2D() );

		// if we're sufficiently far from the last full crouch speed location, we can safely restore crouch speed faster.
		if ( flDistToLastPositionAtFullCrouchSpeed > (64*64) )
		{
			player->m_flDuckSpeed = Approach( CS_PLAYER_DUCK_SPEED_IDEAL, player->m_flDuckSpeed, gpGlobals->frametime * 6.0f );
		}
	}

	bool duckButtonHeld = ( mv->m_nButtons & IN_DUCK ) != 0;

	if ( !duckButtonHeld && player->m_flDuckAmount > 0 )
	{
		// Not sure if this is the appropriate use of this flag. It seems odd to have a dedicated variable that effectively means crouch-is-not-zero-or-one.

		// When the round restarts with the player in the ducked state, they can get stuck crouched.
		// To prevent this, I'm setting the "duck-in-progress" bool (m_bDucking) to true if
		// the player is ever in the state of NOT holding the duck button but is still ducked.
		player->m_Local.m_bDucking = true;
	}
	else if ( duckButtonHeld && player->m_flDuckAmount < 1 )
	{
		// or if the player IS holding the duck button but isn't yet fully ducked.
		player->m_Local.m_bDucking = true;
	}

	// Handle animating into the ducking pose.
	if ( duckButtonHeld && player->m_Local.m_bDucking )
	{
		Assert( !player->m_Local.m_bDucked );

		// ducking is always a little slower than unducking
		float duckSpeed = player->m_flDuckSpeed * 0.8f;

		// Reduce crouch/uncrouch speed significantly while defusing
		if ( m_pCSPlayer->m_bIsDefusing )
			duckSpeed *= 0.4f;

		player->m_flDuckAmount = Approach( 1.0f, player->m_flDuckAmount, gpGlobals->frametime * duckSpeed );

		// Finish ducking immediately if duck time is over or not on ground
		if ( player->m_flDuckAmount >= 1.0f || !playerTouchingGround )
		{
			FinishDuck();
		}
		else
		{
			SetDuckedEyeOffset( player->m_flDuckAmount );
		}

		// REI: For some reason we don't set this flag immediately, but wait until you have ducked a little bit.  Investigate?
		if ( player->m_flDuckAmount >= 0.1f && !( player->GetFlags() & FL_ANIMDUCKING ) )
		{
			player->AddFlag( FL_ANIMDUCKING );
		}
	}

	// Handle animating out of ducking pose.
	if ( !duckButtonHeld && player->m_Local.m_bDucking
		// Try to unduck unless automovement is not allowed
		// NOTE: When not onground, you can always unduck
		// REI: Cloned behavior from old code, not sure when m_bAllowAutomovement is used?
		&& ( player->m_Local.m_bAllowAutoMovement || !playerTouchingGround ) )
	{
		if ( CanUnduck() )
		{
			// Always unduck at at least 1.5 to prevent advantageous semi-ducked positions
			float duckSpeed = MAX( 1.5f, player->m_flDuckSpeed );

			// Reduce crouch/uncrouch speed significantly while defusing
			if ( m_pCSPlayer->m_bIsDefusing )
				duckSpeed *= 0.4f;

			player->m_flDuckAmount = Approach( 0.0f, player->m_flDuckAmount, gpGlobals->frametime * duckSpeed );
			player->m_Local.m_bDucked = false;

			if ( player->m_flDuckAmount <= 0.0f || !playerTouchingGround )
			{
				FinishUnDuck();
			}
			else
			{
				SetDuckedEyeOffset( player->m_flDuckAmount );
			}

			// Remove the ducked flags if we're not fully ducked anymore.
			// REI: This is inconsistent with the documentation for these flags, but I'm not sure why the code
			//      is doing this.  It does mean you lose your ducking accuracy bonus very early in the un-duck,
			//      which is certainly important.
			if ( player->m_flDuckAmount <= 0.75f && player->GetFlags() & ( FL_ANIMDUCKING | FL_DUCKING ) )
			{
				player->RemoveFlag( FL_ANIMDUCKING | FL_DUCKING );
			}
		}
		else
		{
			// Reset to fully-ducked as we went under something we can't un-duck from.
			// We'll try again once the player has moved out of the obstructing obstacle.
			player->m_flDuckAmount = 1.0f;
			player->m_Local.m_bDucked = true;
			player->m_Local.m_bDucking = false;
			player->AddFlag( FL_ANIMDUCKING | FL_DUCKING );

			SetDuckedEyeOffset( player->m_flDuckAmount );
		}
	}

#ifdef AUTHOR_RYANI
	// REI: Consistency checks for fully ducked/unducked states
	if ( player->m_flDuckAmount >= 1.0f )
	{
		AssertMsg1( ( player->GetFlags() & ( FL_DUCKING | FL_ANIMDUCKING ) ) == ( FL_DUCKING | FL_ANIMDUCKING ),
			"ryan: inconsistent state in new duck code for %s.", player->GetPlayerName() );
	}
	else if ( player->m_flDuckAmount <= 0.0f )
	{
		AssertMsg1( ( player->GetFlags() & ( FL_DUCKING | FL_ANIMDUCKING ) ) == 0,
			"ryan: inconsistent state in new duck code for %s.", player->GetPlayerName() );
	}
#endif

	// REI: I think I've fixed all cases of this happening.  Leaving it in for now.  $$$REI remove this after testing shows it never happening again.
	if ( player->m_flDuckAmount <= 0 && (player->GetFlags() & FL_ANIMDUCKING) )
	{
		AssertMsg1( false, "Clearing FL_ANIMDUCKING flag on player %s to prevent crab-walk.  Please let Ryan know if you hit this.", player->GetPlayerName() );
		player->RemoveFlag( FL_ANIMDUCKING );
	}

#ifdef CLIENT_DLL
	if ( IsPreCrouchUpdateDemo() )
	{
		// compatibility for old demos using the old crouch values
		if ( player->m_Local.m_nDuckTimeMsecs )
		{
			player->AddFlag( FL_ANIMDUCKING | FL_DUCKING );
		}
		else
		{
			player->RemoveFlag( FL_ANIMDUCKING | FL_DUCKING );
		}
		const float CS_DUCK_TIME_MSECS = 150.0f;
		int millisecondsDucked = MAX( 0, CS_DUCK_TIME_MSECS - player->m_Local.m_nDuckTimeMsecs );
		player->m_flDuckAmount = (float)millisecondsDucked / (float)CS_DUCK_TIME_MSECS;
		SetDuckedEyeOffset( player->m_flDuckAmount );
	}
#endif

	HandleDuckingSpeedCrop( player->m_flDuckAmount );
}


void CCSGameMovement::OnJump( float fImpulse )
{
	float flStamCost = sv_staminajumpcost.GetFloat( );

	m_pCSPlayer->m_flStamina = clamp( m_pCSPlayer->m_flStamina + flStamCost * fImpulse, 0.0f, sv_staminamax.GetFloat( ) );

	m_pCSPlayer->OnJump( fImpulse );
}	

void CCSGameMovement::OnLand( float fVelocity )
{
	m_pCSPlayer->m_flStamina = clamp( m_pCSPlayer->m_flStamina + sv_staminalandcost.GetFloat() * fVelocity, 0.0f, sv_staminamax.GetFloat());

	m_pCSPlayer->OnLand( fVelocity );
}

// override the default behavior in order to change acceleration based on movement modifiers
void CCSGameMovement::Accelerate( Vector& wishdir, float wishspeed, float accel )
{
	if ( !CanAccelerate() )
		return;

	float flStoredAccel = accel;
	// See if we are changing direction a bit
	float currentspeed = mv->m_vecVelocity.Dot(wishdir);
	float flUnmodifiedSpeed = currentspeed;

	// Reduce wishspeed by the amount of veer.
	float addspeed = wishspeed - currentspeed;

	// If not going to add any speed, done.
	if (addspeed <= 0)
		return;

	if ( currentspeed < 0 )
		currentspeed = 0;

	bool bIsDucking = ( mv->m_nButtons & IN_DUCK ) || ( player->m_Local.m_bDucking ) || ( player->GetFlags() & FL_DUCKING );
	bool bIsWalking = ( mv->m_nButtons & ( /*IN_WALK | */ IN_SPEED ) ) != 0 && !bIsDucking;

	float flMaxSpeed = 250.0f;
	float fAccelerationScale = MAX(flMaxSpeed, wishspeed);
	float flGoalSpeed = fAccelerationScale;

	// if this convar is set, we ignore the passed in accel value (sv_acceleration) and accelerate based on the one below instead
	float flZeroToMaxSpeedTime = SV_ACCELERATE_EXPONENT_TIME;

	CWeaponCSBase *csWeapon = dynamic_cast< CWeaponCSBase * >( player->GetActiveWeapon() );
/*		flMaxScaleSpeed = mv->m_flMaxSpeed;*/
	
	bool bIsSlowSniperScoped = false;

	flGoalSpeed = fAccelerationScale;
	if ( sv_accelerate_use_weapon_speed.GetBool( ) && csWeapon )
	{
		bIsSlowSniperScoped = (csWeapon->GetCSZoomLevel() > 0 && csWeapon->GetZoomLevels() > 1 
								&& (csWeapon->GetMaxSpeed( ) * CS_PLAYER_SPEED_WALK_MODIFIER) < 110.0);

		flGoalSpeed *= MIN( 1.0f, ( csWeapon->GetMaxSpeed( ) / flMaxSpeed ) );
		
		if ( (!bIsDucking && !bIsWalking) || (( bIsWalking || bIsDucking) && bIsSlowSniperScoped) )
			fAccelerationScale *= MIN( 1.0f, (csWeapon->GetMaxSpeed() / flMaxSpeed));
	}

	// TODO: make this number not a magic number
	if ( bIsDucking )
	{
		if ( !bIsSlowSniperScoped )
			fAccelerationScale *= CS_PLAYER_SPEED_DUCK_MODIFIER;

		flGoalSpeed *= CS_PLAYER_SPEED_DUCK_MODIFIER;
	}

	if ( bIsWalking )
	{
		if ( !bIsSlowSniperScoped )
			fAccelerationScale *= CS_PLAYER_SPEED_WALK_MODIFIER;

		flGoalSpeed *= CS_PLAYER_SPEED_WALK_MODIFIER;
	}

	/*
	bool bShouldStutter = (wishspeed > 200.0f);
	if ( bShouldStutter == false && bIsDucking == false && bIsWalking == false )
	{
		flStoredAccel *= 0.75;//clamp( 1.0f - ( MAX( 0.0f, currentspeed - ( flGoalSpeed - 5 ) ) / MAX( 0.0f, flGoalSpeed - ( flGoalSpeed - 5 ) ) ), 0.0f, 1.0f );
	}
	// we no longer just clamp player's max speed when they hit the walk key
	else if (bIsWalking && currentspeed > (flGoalSpeed-5) )*/
	if (bIsWalking && currentspeed > (flGoalSpeed-5) )
	{	
		// we now only clamp it when it's within a certain range of walking, otherwise we stop adding speed to let them decelerate naturally
		flStoredAccel *= clamp( 1.0f - ( MAX( 0.0f, currentspeed - ( flGoalSpeed - 5 ) ) / MAX( 0.0f, flGoalSpeed - ( flGoalSpeed - 5 ) ) ), 0.0f, 1.0f );
	}
	
	// Determine amount of acceleration.
	float accelspeed = 0;

	float flCounterSpeed = 0;
	float flMaxAccelspeed = 0;

	if ( flZeroToMaxSpeedTime > 0 )
	{	
		currentspeed = MAX( 5, currentspeed );

		float flRawAccelExponent = MAX(1, SV_ACCELERATE_EXPONENT);
		float flAccelExponentTop = (flRawAccelExponent-1);
		flAccelExponentTop *= MAX( 0.0001, 1-(MAX( 0, (fAccelerationScale-220) )/(flGoalSpeed-220) ) );

		if ( bIsDucking || bIsWalking )
			flAccelExponentTop *= 0.05;

		float flAccelExponent = flAccelExponentTop + 1;

		float flMoveFracActual = pow( currentspeed/flGoalSpeed, 1/flAccelExponent );

		float flLastTimeDelta = gpGlobals->curtime - m_pCSPlayer->m_flGroundAccelLinearFracLastTime;
		if ( currentspeed <= 1 || flLastTimeDelta > 0.1 )
			flLastTimeDelta = 0.015;

		// get friction because we're going to counter it a bit on accelerate
		float flFriction = (sv_friction.GetFloat() * player->m_surfaceFriction);
		float flControl = (currentspeed < sv_stopspeed.GetFloat()) ? sv_stopspeed.GetFloat() : currentspeed;
		flCounterSpeed = flControl*flFriction*gpGlobals->frametime;

		float flTimeSinceStart = (flZeroToMaxSpeedTime * flMoveFracActual);
		float flNewTime = (flTimeSinceStart + flLastTimeDelta);
		float flNewFrac = MIN( 1, (flNewTime / flZeroToMaxSpeedTime) );
// 		if ( flNewFrac > 1 )
// 		{
// 			int x = 1;
// 			x++;
// 		}
		float flNewSpeed = pow( flNewFrac, flAccelExponent ) * flGoalSpeed;
		flStoredAccel = flNewSpeed-currentspeed;

		// we're stutter stepping, counter the current speed here
		if ( flUnmodifiedSpeed < -(flMaxSpeed*CS_PLAYER_SPEED_WALK_MODIFIER) )
			flCounterSpeed += -(flUnmodifiedSpeed/3);

		flMaxAccelspeed = accel * gpGlobals->frametime * fAccelerationScale * player->m_surfaceFriction;

		// apply the speed
		accelspeed = MIN( flMaxAccelspeed, (flStoredAccel * gpGlobals->frametime * fAccelerationScale)+flCounterSpeed );
	}
	else
	{
		// apply the speed
		accelspeed = flStoredAccel * gpGlobals->frametime * fAccelerationScale * player->m_surfaceFriction;
	}

// 	if ( accelspeed < 16 && accelspeed > 11 )
// 	{
// 		Msg( "BLAH!!!" );
// 	}

	// Cap at addspeed
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	mv->m_vecVelocity += (accelspeed * wishdir);

	// store the last time we updated the speed
	m_pCSPlayer->m_flGroundAccelLinearFracLastTime = gpGlobals->curtime;

#if !defined(CLIENT_DLL)
	if ( sv_accelerate_debug_speed.GetBool() )
		DevMsg( "------- accelspeed = %f, flGoalSpeed = %f, flStoredAccel = %f\n", accelspeed, flGoalSpeed, flStoredAccel );
#endif

	//DevMsg( "TRAILING MOVESPEED %f!\n", mv->m_vecTrailingVelocity.AsVector2D().Length() );

	if ( mv->m_vecTrailingVelocity.IsZero() || ( gpGlobals->curtime - mv->m_flTrailingVelocityTime ) > 0.35f )
	{
		// Do a full update
		mv->m_vecTrailingVelocity = mv->m_vecVelocity;
		mv->m_flTrailingVelocityTime = gpGlobals->curtime;
	}
	else
	{
		Vector2D vNormalizedCurrent = mv->m_vecVelocity.AsVector2D();
		Vector2DNormalize( vNormalizedCurrent );

		Vector2D vNormalizedPrev = mv->m_vecTrailingVelocity.AsVector2D();
		Vector2DNormalize( vNormalizedPrev );

		// Check if they're pointed roughly the same direction
		float flDot = vNormalizedCurrent.Dot( vNormalizedPrev );
		if ( flDot > 0.8f )
		{
			// Check if the current has a larger magnitude
			if ( mv->m_vecTrailingVelocity.AsVector2D().LengthSqr() < mv->m_vecVelocity.AsVector2D().LengthSqr() )
			{
				mv->m_vecTrailingVelocity = mv->m_vecVelocity;
				mv->m_flTrailingVelocityTime = gpGlobals->curtime;
			}
		}
		// Check if they're going the opposite direction
		else if ( flDot < -0.8f )
		{
			// Check if the velocity difference is extreme
			if ( mv->m_vecTrailingVelocity.AsVector2D().Length() < 225.0f && mv->m_vecTrailingVelocity.AsVector2D().Length() > 115.0f && mv->m_vecVelocity.AsVector2D().Length() > 115.0f )
			{
				// Check if the player is moving perpendicular
				Vector vEyeForward;
				m_pCSPlayer->EyeVectors( &vEyeForward );

				float flEyeDot = vEyeForward.AsVector2D().Dot( vNormalizedCurrent );
				if ( flEyeDot > -0.3f && flEyeDot < 0.3f )
				{
					CWeaponCSBase *pWeapon = m_pCSPlayer->GetActiveCSWeapon();
					if ( pWeapon )
					{
#if defined( WEAPON_FIRE_BULLETS_ACCURACY_FISHTAIL_FEATURE )
						//DevMsg( "FISHTAIL %s!\n", flEyeDot > 0.0f ? "left" : "right" );

						/*if ( sv_extreme_strafe_aim_punch.GetBool() )
						{
							m_pCSPlayer->KickBack( flEyeDot > 0.0f ? 90.0f : 270.0f, sv_extreme_strafe_accuracy_fishtail.GetFloat() * 15.0f );
						}
						else*/
						{
							pWeapon->SetAccuracyFishtail( flEyeDot > 0.0f ? -sv_extreme_strafe_accuracy_fishtail.GetFloat() : sv_extreme_strafe_accuracy_fishtail.GetFloat() );
						}
#endif
						mv->m_vecTrailingVelocity = mv->m_vecVelocity;
						mv->m_flTrailingVelocityTime = gpGlobals->curtime;
					}
				}
			}
		}
	}
}

