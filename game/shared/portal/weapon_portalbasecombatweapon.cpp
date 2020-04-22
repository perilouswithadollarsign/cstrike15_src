//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "weapon_portalbasecombatweapon.h"
#include "in_buttons.h"

#include "portal_player_shared.h"

#if defined( CLIENT_DLL )
	#include "c_portal_player.h"
#else
	#include "vphysics/constraints.h"
	#include "gameweaponmanager.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_NETWORKCLASS_ALIASED( BasePortalCombatWeapon , DT_BasePortalCombatWeapon )
LINK_ENTITY_TO_CLASS_ALIASED( baseportalcombatweapon, BasePortalCombatWeapon );

BEGIN_NETWORK_TABLE( CBasePortalCombatWeapon , DT_BasePortalCombatWeapon )
#if !defined( CLIENT_DLL )
//	SendPropInt( SENDINFO( m_bReflectViewModelAnimations ), 1, SPROP_UNSIGNED ),
#else
//	RecvPropInt( RECVINFO( m_bReflectViewModelAnimations ) ),
#endif
END_NETWORK_TABLE()


#if !defined( CLIENT_DLL )

#include "globalstate.h"

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CBasePortalCombatWeapon )

	DEFINE_FIELD( m_bLowered,			FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flRaiseTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flHolsterTime,		FIELD_TIME ),
	DEFINE_FIELD( m_flNextRepeatPrimaryAttack,		FIELD_TIME ),
	DEFINE_FIELD( m_flNextRepeatSecondaryAttack,	FIELD_TIME ),
END_DATADESC()

#else

BEGIN_PREDICTION_DATA( CBasePortalCombatWeapon )
	DEFINE_FIELD( m_flNextRepeatPrimaryAttack, FIELD_TIME ),
	DEFINE_FIELD( m_flNextRepeatSecondaryAttack, FIELD_TIME ),
END_PREDICTION_DATA()

#endif

extern ConVar sk_auto_reload_time;

CBasePortalCombatWeapon::CBasePortalCombatWeapon( void )
{

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBasePortalCombatWeapon::ItemHolsterFrame( void )
{
	BaseClass::ItemHolsterFrame();

	// Must be player held
	if ( GetOwner() && GetOwner()->IsPlayer() == false )
		return;

	// We can't be active
	if ( GetOwner()->GetActiveWeapon() == this )
		return;

	// If it's been longer than three seconds, reload
	if ( ( gpGlobals->curtime - m_flHolsterTime ) > sk_auto_reload_time.GetFloat() )
	{
		// Just load the clip with no animations
		FinishReload();
		m_flHolsterTime = gpGlobals->curtime;
	}
}

bool CBasePortalCombatWeapon::CanLower()
{
	if ( SelectWeightedSequence( ACT_VM_IDLE_LOWERED ) == ACTIVITY_NOT_AVAILABLE )
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Drops the weapon into a lowered pose
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBasePortalCombatWeapon::Lower( void )
{
	//Don't bother if we don't have the animation
	if ( SelectWeightedSequence( ACT_VM_IDLE_LOWERED ) == ACTIVITY_NOT_AVAILABLE )
		return false;

	m_bLowered = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Brings the weapon up to the ready position
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBasePortalCombatWeapon::Ready( void )
{
	//Don't bother if we don't have the animation
	if ( SelectWeightedSequence( ACT_VM_LOWERED_TO_IDLE ) == ACTIVITY_NOT_AVAILABLE )
		return false;

	m_bLowered = false;	
	m_flRaiseTime = gpGlobals->curtime + 0.5f;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBasePortalCombatWeapon::Deploy( void )
{
	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBasePortalCombatWeapon::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( BaseClass::Holster( pSwitchingTo ) )
	{
		m_flHolsterTime = gpGlobals->curtime;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBasePortalCombatWeapon::WeaponShouldBeLowered( void )
{
	// Can't be in the middle of another animation
  	if ( GetIdealActivity() != ACT_VM_IDLE_LOWERED && GetIdealActivity() != ACT_VM_IDLE &&
		 GetIdealActivity() != ACT_VM_IDLE_TO_LOWERED && GetIdealActivity() != ACT_VM_LOWERED_TO_IDLE )
  		return false;

	if ( m_bLowered )
		return true;
	
#if !defined( CLIENT_DLL )

	if ( GlobalEntity_GetState( "friendly_encounter" ) == GLOBAL_ON )
		return true;

#endif

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Allows the weapon to choose proper weapon idle animation
//-----------------------------------------------------------------------------
void CBasePortalCombatWeapon::WeaponIdle( void )
{
}

#if defined( CLIENT_DLL )

extern float	g_lateralBob;
extern float	g_verticalBob;

#define	HL2_BOB_CYCLE_MIN	1.0f
#define	HL2_BOB_CYCLE_MAX	0.45f
#define	HL2_BOB			0.002f
#define	HL2_BOB_UP		0.5f

#if !defined( PORTAL2 )
static ConVar	cl_bobcycle( "cl_bobcycle","0.8" );
static ConVar	cl_bob( "cl_bob","0.002" );
static ConVar	cl_bobup( "cl_bobup","0.5" );

// Register these cvars if needed for easy tweaking
static ConVar	v_iyaw_cycle( "v_iyaw_cycle", "2", FCVAR_CHEAT );
static ConVar	v_iroll_cycle( "v_iroll_cycle", "0.5", FCVAR_CHEAT );
static ConVar	v_ipitch_cycle( "v_ipitch_cycle", "1", FCVAR_CHEAT );
static ConVar	v_iyaw_level( "v_iyaw_level", "0.3", FCVAR_CHEAT );
static ConVar	v_iroll_level( "v_iroll_level", "0.1", FCVAR_CHEAT );
static ConVar	v_ipitch_level( "v_ipitch_level", "0.3", FCVAR_CHEAT );
#endif

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float CBasePortalCombatWeapon::CalcViewmodelBob( void )
{
	static	float bobtime;
	static	float lastbobtime;
	float	cycle;
	
	CBasePlayer *player = ToBasePlayer( GetOwner() );
	//Assert( player );

	//NOTENOTE: For now, let this cycle continue when in the air, because it snaps badly without it

	if ( ( !gpGlobals->frametime ) || ( player == NULL ) )
	{
		//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
		return 0.0f;// just use old value
	}

	// Note: we use paint code for this so when player move on speed paint, gun bob faster (Bank)
	//Find the speed of the player
	float speed = player->GetLocalVelocity().Length();

	speed = clamp( speed, -player->MaxSpeed(), player->MaxSpeed() );

	float bob_offset = RemapVal( speed, 0, player->MaxSpeed(), 0.0f, 1.0f );

	////Find the speed of the player
	//float speed = player->GetLocalVelocity().Length2D();

	////FIXME: This maximum speed value must come from the server.
	////		 MaxSpeed() is not sufficient for dealing with sprinting - jdw

	//speed = clamp( speed, -320, 320 );

	//float bob_offset = RemapVal( speed, 0, 320, 0.0f, 1.0f );
	
	bobtime += ( gpGlobals->curtime - lastbobtime ) * bob_offset;
	lastbobtime = gpGlobals->curtime;

	//Calculate the vertical bob
	cycle = bobtime - (int)(bobtime/HL2_BOB_CYCLE_MAX)*HL2_BOB_CYCLE_MAX;
	cycle /= HL2_BOB_CYCLE_MAX;

	if ( cycle < HL2_BOB_UP )
	{
		cycle = M_PI * cycle / HL2_BOB_UP;
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-HL2_BOB_UP)/(1.0 - HL2_BOB_UP);
	}
	
	g_verticalBob = speed*0.005f;
	g_verticalBob = g_verticalBob*0.3 + g_verticalBob*0.7*sin(cycle);

	g_verticalBob = clamp( g_verticalBob, -7.0f, 4.0f );

	//Calculate the lateral bob
	cycle = bobtime - (int)(bobtime/HL2_BOB_CYCLE_MAX*2)*HL2_BOB_CYCLE_MAX*2;
	cycle /= HL2_BOB_CYCLE_MAX*2;

	if ( cycle < HL2_BOB_UP )
	{
		cycle = M_PI * cycle / HL2_BOB_UP;
	}
	else
	{
		cycle = M_PI + M_PI*(cycle-HL2_BOB_UP)/(1.0 - HL2_BOB_UP);
	}

	g_lateralBob = speed*0.005f;
	g_lateralBob = g_lateralBob*0.3 + g_lateralBob*0.7*sin(cycle);
	g_lateralBob = clamp( g_lateralBob, -7.0f, 4.0f );
	
	//NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			viewmodelindex - 
//-----------------------------------------------------------------------------
void CBasePortalCombatWeapon::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
	Vector	forward, right, up;
	AngleVectors( angles, &forward, &right, &up );

	CalcViewmodelBob();


	// Note: we need to use paint code for gun bob so the gun bobs correctly when player sticks on walls (Bank)
	C_Portal_Player *pPortalPlayer = ToPortalPlayer( GetOwner() );
	if ( !pPortalPlayer )
		return;

	// Apply bob, but scaled down to 40%
	VectorMA( origin, g_verticalBob * 0.1f, forward, origin );

	// Z bob a bit more
	origin += g_verticalBob * 0.1f * pPortalPlayer->GetPortalPlayerLocalData().m_Up;

	//move left and right
	VectorMA( origin, g_lateralBob * 0.8f, right, origin );

	//roll, pitch, yaw
	float rollAngle = g_verticalBob * 0.5f;
	VMatrix rotMatrix;
	Vector rotAxis = CrossProduct( right, up ).Normalized();

	MatrixBuildRotationAboutAxis( rotMatrix, rotAxis, rollAngle );
	up = rotMatrix * up;
	forward = rotMatrix * forward;
	right = rotMatrix * right;

	float pitchAngle = -g_verticalBob * 0.4f;
	rotAxis = right;
	MatrixBuildRotationAboutAxis( rotMatrix, rotAxis, pitchAngle );
	up = rotMatrix * up;
	forward = rotMatrix * forward;

	float yawAngle = -g_lateralBob * 0.3f;
	rotAxis = up;
	MatrixBuildRotationAboutAxis( rotMatrix, rotAxis, yawAngle );
	forward = rotMatrix * forward;

	VectorAngles( forward, up, angles );

	//// Apply bob, but scaled down to 40%
	//VectorMA( origin, g_verticalBob * 0.1f, forward, origin );
	//
	//// Z bob a bit more
	//origin[2] += g_verticalBob * 0.1f;
	//
	//// bob the angles
	//angles[ ROLL ]	+= g_verticalBob * 0.5f;
	//angles[ PITCH ]	-= g_verticalBob * 0.4f;

	//angles[ YAW ]	-= g_lateralBob  * 0.3f;

	//VectorMA( origin, g_lateralBob * 0.8f, right, origin );
}

//-----------------------------------------------------------------------------
Vector CBasePortalCombatWeapon::GetBulletSpread( WeaponProficiency_t proficiency )
{
	return BaseClass::GetBulletSpread( proficiency );
}

//-----------------------------------------------------------------------------
float CBasePortalCombatWeapon::GetSpreadBias( WeaponProficiency_t proficiency )
{
	return BaseClass::GetSpreadBias( proficiency );
}
//-----------------------------------------------------------------------------

const WeaponProficiencyInfo_t *CBasePortalCombatWeapon::GetProficiencyValues()
{
	return NULL;
}

#else

// Server stubs
float CBasePortalCombatWeapon::CalcViewmodelBob( void )
{
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			viewmodelindex - 
//-----------------------------------------------------------------------------
void CBasePortalCombatWeapon::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
}


//-----------------------------------------------------------------------------
Vector CBasePortalCombatWeapon::GetBulletSpread( WeaponProficiency_t proficiency )
{
	Vector baseSpread = BaseClass::GetBulletSpread( proficiency );

	const WeaponProficiencyInfo_t *pProficiencyValues = GetProficiencyValues();
	float flModifier = (pProficiencyValues)[ proficiency ].spreadscale;
	return ( baseSpread * flModifier );
}

//-----------------------------------------------------------------------------
float CBasePortalCombatWeapon::GetSpreadBias( WeaponProficiency_t proficiency )
{
	const WeaponProficiencyInfo_t *pProficiencyValues = GetProficiencyValues();
	return (pProficiencyValues)[ proficiency ].bias;
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CBasePortalCombatWeapon::GetProficiencyValues()
{
	return GetDefaultProficiencyValues();
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CBasePortalCombatWeapon::GetDefaultProficiencyValues()
{
	// Weapon proficiency table. Keep this in sync with WeaponProficiency_t enum in the header!!
	static WeaponProficiencyInfo_t g_BaseWeaponProficiencyTable[] =
	{
		{ 2.50, 1.0	},
		{ 2.00, 1.0	},
		{ 1.50, 1.0	},
		{ 1.25, 1.0 },
		{ 1.00, 1.0	},
	};

	COMPILE_TIME_ASSERT( ARRAYSIZE(g_BaseWeaponProficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return g_BaseWeaponProficiencyTable;
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Handle firing
//-----------------------------------------------------------------------------
void CBasePortalCombatWeapon::ItemPostFrame( void )
{
	CPortal_Player *pOwner = ToPortalPlayer( GetOwner() );
	if ( pOwner == NULL )
		return;

	// Primary attack
	if ( pOwner->m_nButtons & IN_ATTACK && ( m_flNextPrimaryAttack <= gpGlobals->curtime ) )
	{
		if ( pOwner->GetWaterLevel() == 3 && m_bFiresUnderwater == false )
		{
			// This weapon doesn't fire underwater
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + 0.2;
			return;
		}

		// If they're still holding down the button, wait for the next fire time
		if ( ( pOwner->m_afButtonLast & IN_ATTACK ) && ( m_flNextRepeatPrimaryAttack > gpGlobals->curtime ) )
			return;

		PrimaryAttack();
		return;
	}

	// Secondary attack
	if ( pOwner->m_nButtons & IN_ATTACK2 && ( m_flNextSecondaryAttack <= gpGlobals->curtime ) )
	{
		if ( pOwner->GetWaterLevel() == 3 )
		{
			// This weapon doesn't fire underwater
			WeaponSound( EMPTY );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + 0.2;
			return;
		}

		// If they're still holding down the button, wait for the next fire time
		if ( ( pOwner->m_afButtonLast & IN_ATTACK2 ) && ( m_flNextRepeatSecondaryAttack > gpGlobals->curtime ) )
			return;

		// Attack!
		SecondaryAttack();
		return;
	}

	WeaponIdle();
}


ConVar sv_weapon_pickup_time_delay("sv_weapon_pickup_time_delay", "0.2f", FCVAR_REPLICATED | FCVAR_CHEAT);

bool CBasePortalCombatWeapon::EnoughTimeSinceThrown()
{
	return gpGlobals->curtime - m_flThrowTime > sv_weapon_pickup_time_delay.GetFloat();
}

float CBasePortalCombatWeapon::GetThrowTime()
{
	return m_flThrowTime;
}

void CBasePortalCombatWeapon::Drop( const Vector& vecVelocity )
{
	// Store time when we threw the gun so we dont go and pick up the gun too soon
	m_flThrowTime = gpGlobals->curtime;
	m_pLastOwner = GetOwner();

	BaseClass::Drop( vecVelocity );
}

CBaseEntity* CBasePortalCombatWeapon::GetLastOwner()
{
	return m_pLastOwner;
}
