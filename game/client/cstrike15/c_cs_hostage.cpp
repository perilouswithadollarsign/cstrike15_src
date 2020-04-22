//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side C_CHostage class 
//
// $NoKeywords: $ 
//=============================================================================//
#include "cbase.h"
#include "c_cs_hostage.h"
#include <bitbuf.h>
#include "ragdoll_shared.h"
#include "c_breakableprop.h"
#include "cs_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#undef CHostage

//-----------------------------------------------------------------------------

static float HOSTAGE_HEAD_TURN_RATE = 130;

CUtlVector< C_CHostage* > g_Hostages;
CUtlVector< CHandle<C_BaseAnimating> > g_HostageRagdolls;

extern ConVar g_ragdoll_fadespeed;
ConVar cl_use_hostage_ik("cl_use_hostage_ik", "1");
//-----------------------------------------------------------------------------
const int NumInterestingPoseParameters = 6;
static const char* InterestingPoseParameters[NumInterestingPoseParameters] =
{
	"body_yaw",
	"spine_yaw",
	"neck_trans",
	"head_pitch",
	"head_yaw",
	"head_roll"
};

// Op Bloodhound hostage characters
// cbble: Lord William
// lake: journalist
// bank: Hostage
// office: 
struct CustomHostageNames
{
	const char* mapname;
	const char* hostageName;
};
static const CustomHostageNames s_HostageCharactersNames[] =
{
	{ "default", "#Cstrike_TitlesTXT_Hostage" },
	{ "cs_office", "#CSGO_StoryHostageName_cs_office" },
	{ "gd_lake", "#CSGO_StoryHostageName_gd_lake" },
	{ "gd_cbble", "#CSGO_StoryHostageName_gd_cbble" },
	{ "gd_bank", "#CSGO_StoryHostageName_gd_bank" },
	{ "coop_cementplant", "#CSGO_StoryHostageName_gd_lake" },
	{ "", "" } // this needs to be last
};

//-----------------------------------------------------------------------------
C_HostageCarriableProp::C_HostageCarriableProp()
{
	m_bCreatedViewmodel = false;
	m_flFadeInStartTime = 0.0f;
}

//-----------------------------------------------------------------------------
void C_HostageCarriableProp::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );

		m_flFadeInStartTime = gpGlobals->curtime;
	}

	// if we don't have an owner for some reason, remove ourselves
	C_BaseEntity *pOwner = GetFollowedEntity();
	if ( !pOwner )
		UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Should this object cast shadows?
//-----------------------------------------------------------------------------
ShadowType_t C_HostageCarriableProp::ShadowCastType()
{
	if ( IsEffectActive( /*EF_NODRAW |*/ EF_NOSHADOW ) )
		return SHADOWS_NONE;

	if ( ShouldDraw() )
		return SHADOWS_RENDER_TO_TEXTURE;

	return SHADOWS_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether object should render.
//-----------------------------------------------------------------------------
bool C_HostageCarriableProp::ShouldDraw( void )
{
	C_BaseEntity *pOwner = GetFollowedEntity();

	// weapon has no owner, don't draw it - will get removed soon
	if ( !pOwner )
		return false;

	C_BasePlayer *pViewedPlayer = GetLocalOrInEyeCSPlayer();
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	// carried by local player?
	if ( pOwner == pViewedPlayer )
	{
		if ( pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE )
			return false;

		// 3rd person mode
		return pViewedPlayer->ShouldDrawLocalPlayer();
	}

	return BaseClass::ShouldDraw();
}

void C_HostageCarriableProp::ClientThink()
{
	BaseClass::ClientThink();

	int a = GetRenderAlpha();

	if ( m_flFadeInStartTime + 1.0 > gpGlobals->curtime )
	{
		SetRenderMode( kRenderGlow );
		a = ((gpGlobals->curtime - m_flFadeInStartTime) /  1.0f) * 255;
	}
	else
	{
		SetRenderMode( kRenderNormal );
		a = 255;
	}

	a = MAX( 0, a );
	SetRenderAlpha( a );
}

IMPLEMENT_CLIENTCLASS_DT(C_HostageCarriableProp, DT_HostageCarriableProp, CHostageCarriableProp)
END_RECV_TABLE()

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
class C_LowViolenceHostageDeathModel : public C_BaseAnimating
{
public:
	DECLARE_CLASS( C_LowViolenceHostageDeathModel, C_BaseAnimating );
	
	C_LowViolenceHostageDeathModel();
	~C_LowViolenceHostageDeathModel();

	bool SetupLowViolenceModel( C_CHostage *pHostage );

	// fading out
	void ClientThink( void );

private:

	void Interp_Copy( VarMapping_t *pDest, CBaseEntity *pSourceEntity, VarMapping_t *pSrc );

	float m_flFadeOutStart;
};

//-----------------------------------------------------------------------------
C_LowViolenceHostageDeathModel::C_LowViolenceHostageDeathModel()
{
}

//-----------------------------------------------------------------------------
C_LowViolenceHostageDeathModel::~C_LowViolenceHostageDeathModel()
{
}

//-----------------------------------------------------------------------------
void C_LowViolenceHostageDeathModel::Interp_Copy( VarMapping_t *pDest, CBaseEntity *pSourceEntity, VarMapping_t *pSrc )
{
	if ( !pDest || !pSrc )
		return;

	if ( pDest->m_Entries.Count() != pSrc->m_Entries.Count() )
	{
		Assert( false );
		return;
	}

	int c = pDest->m_Entries.Count();
	for ( int i = 0; i < c; i++ )
	{
		pDest->m_Entries[ i ].watcher->Copy( pSrc->m_Entries[i].watcher );
	}
}

//-----------------------------------------------------------------------------
bool C_LowViolenceHostageDeathModel::SetupLowViolenceModel( C_CHostage *pHostage )
{
	const model_t *model = pHostage->GetModel();
	const char *pModelName = modelinfo->GetModelName( model );
	if ( InitializeAsClientEntity( pModelName, false ) == false )
	{
		Release();
		return false;
	}

	// Play the low-violence death anim
	if ( LookupSequence( "death1" ) == -1 )
	{
		Release();
		return false;
	}

	m_flFadeOutStart = gpGlobals->curtime + 5.0f;
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	SetSequence( LookupSequence( "death1" ) );
	ForceClientSideAnimationOn();

	if ( pHostage && !pHostage->IsDormant() )
	{
		SetNetworkOrigin( pHostage->GetAbsOrigin() );
		SetAbsOrigin( pHostage->GetAbsOrigin() );
		SetAbsVelocity( pHostage->GetAbsVelocity() );

		// move my current model instance to the ragdoll's so decals are preserved.
		pHostage->SnatchModelInstance( this );

		SetAbsAngles( pHostage->GetRenderAngles() );
		SetNetworkAngles( pHostage->GetRenderAngles() );

		CStudioHdr *pStudioHdr = GetModelPtr();

		// update pose parameters
		float poseParameter[MAXSTUDIOPOSEPARAM];
		GetPoseParameters( pStudioHdr, poseParameter );
		for ( int i=0; i<NumInterestingPoseParameters; ++i )
		{
			int poseParameterIndex = LookupPoseParameter( pStudioHdr, InterestingPoseParameters[i] );
			SetPoseParameter( pStudioHdr, poseParameterIndex, poseParameter[poseParameterIndex] );
		}
	}

	Interp_Reset( GetVarMapping() );
	return true;
}

//-----------------------------------------------------------------------------
void C_LowViolenceHostageDeathModel::ClientThink( void )
{
	if ( m_flFadeOutStart > gpGlobals->curtime )
	{
		 return;
	}

	int iAlpha = GetRenderAlpha();

	iAlpha = MAX( iAlpha - ( g_ragdoll_fadespeed.GetInt() * gpGlobals->frametime ), 0 );

	SetRenderMode( kRenderTransAlpha );
	SetRenderAlpha( iAlpha );

	if ( iAlpha == 0 )
	{
		Release();
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_CHostage::RecvProxy_Rescued( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_CHostage *pHostage= (C_CHostage *) pStruct;
	
	bool isRescued = pData->m_Value.m_Int != 0;

	if ( isRescued && !pHostage->m_isRescued )
	{
		// hostage was rescued
		pHostage->m_flDeadOrRescuedTime = gpGlobals->curtime + 2;
		//pHostage->SetRenderMode( kRenderGlow );
		//pHostage->SetNextClientThink( gpGlobals->curtime );
	}

	pHostage->m_isRescued = isRescued;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void C_CHostage::RecvProxy_Jumped( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_CHostage *pHostage= (C_CHostage *) pStruct;
	
	bool jumped = pData->m_Value.m_Int != 0;

	if ( jumped )
	{
		// hostage jumped
		pHostage->m_PlayerAnimState->DoAnimationEvent( PLAYERANIMEVENT_JUMP );
		pHostage->SetNextClientThink( gpGlobals->curtime );
	}

	pHostage->m_jumpedThisFrame = jumped;
}

//-----------------------------------------------------------------------------
IMPLEMENT_CLIENTCLASS_DT(C_CHostage, DT_CHostage, CHostage)
	
	RecvPropInt( RECVINFO( m_isRescued ), 0, C_CHostage::RecvProxy_Rescued ),
	RecvPropInt( RECVINFO( m_jumpedThisFrame ), 0, C_CHostage::RecvProxy_Jumped ),
	
	RecvPropInt( RECVINFO( m_iHealth ) ),
	RecvPropInt( RECVINFO( m_iMaxHealth ) ),
	RecvPropInt( RECVINFO( m_lifeState ) ),
	RecvPropInt( RECVINFO( m_fFlags ) ),// Needed for on ground detection for hostage jumping.

	RecvPropInt( RECVINFO( m_nHostageState ) ),

	RecvPropFloat( RECVINFO( m_flRescueStartTime ) ),
	RecvPropFloat( RECVINFO( m_flGrabSuccessTime ) ),
	RecvPropFloat( RECVINFO( m_flDropStartTime ) ),

	RecvPropVector( RECVINFO( m_vel ) ),
	RecvPropEHandle( RECVINFO( m_leader ) ),

END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_CHostage::C_CHostage()
{
	g_Hostages.AddToTail( this );

	m_flDeadOrRescuedTime = 0.0;
	m_flLastBodyYaw = 0;
	m_createdLowViolenceRagdoll = false;
	
	// TODO: Get IK working on the steep slopes CS has, then enable it on characters.
	// [msmith] We're starting to use IK on hostages so that we can optionally have their hands bound.
	if ( !cl_use_hostage_ik.GetBool() )
	{
		m_EntClientFlags |= ENTCLIENTFLAG_DONTUSEIK;
	}

	// set the model so the PlayerAnimState uses the Hostage activities/sequences
	SetModelName( HOSTAGE_ANIM_MODEL );

	m_PlayerAnimState = CreateHostageAnimState( this, this, LEGANIM_9WAY, false );
	
	m_leader = INVALID_EHANDLE;
	m_blinkTimer.Invalidate();

	m_flCurrentHeadPitch = 0;
	m_flCurrentHeadYaw = 0;

	m_eyeAttachment = -1;
	m_chestAttachment = -1;
	m_headYawPoseParam = -1;
	m_headPitchPoseParam = -1;
	m_lookAt = Vector( 0, 0, 0 );
	m_isInit = false;
	m_lookAroundTimer.Invalidate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_CHostage::~C_CHostage()
{
	g_Hostages.FindAndRemove( this );
	m_PlayerAnimState->Release();

	SetClientSideHolidayHatAddon( false );
}

//-----------------------------------------------------------------------------
void C_CHostage::Spawn( void )
{
	m_leader = INVALID_EHANDLE;
	m_blinkTimer.Invalidate();

	SetClientSideHolidayHatAddon( false );

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

const char * C_CHostage::GetCustomHostageNameForMap( const char *szMapName )
{
	for ( int i = 0; i < ARRAYSIZE( s_HostageCharactersNames ); ++i )
	{
		if ( Q_stricmp( s_HostageCharactersNames[i].mapname, szMapName ) == 0 )
		{
			return s_HostageCharactersNames[i].hostageName;
		}
	}

	return s_HostageCharactersNames[0].hostageName;
}

//-----------------------------------------------------------------------------
bool C_CHostage::ShouldDraw( void )
{
	if ( m_createdLowViolenceRagdoll )
		return false;

	switch ( m_nHostageState )
	{
	case k_EHostageStates_BeingCarried:
		{
			return false;
		}
	case k_EHostageStates_GettingPickedUp:
	case k_EHostageStates_GettingDropped:
	case k_EHostageStates_Rescued:
	case k_EHostageStates_Idle:
	case k_EHostageStates_BeingUntied:
	case k_EHostageStates_FollowingPlayer:
	case k_EHostageStates_Dead:
		{
			return true;
		}
	}

	return BaseClass::ShouldDraw();
}

//-----------------------------------------------------------------------------
C_BaseAnimating * C_CHostage::BecomeRagdollOnClient()
{
	if ( g_RagdollLVManager.IsLowViolence() )
	{
		// We can't just play the low-violence anim ourselves, since we're about to be deleted by the server.
		// So, let's create another entity that can play the anim and stick around.
		C_LowViolenceHostageDeathModel *pLowViolenceModel = new C_LowViolenceHostageDeathModel();
		m_createdLowViolenceRagdoll = pLowViolenceModel->SetupLowViolenceModel( this );
		if ( m_createdLowViolenceRagdoll )
		{
			UpdateVisibility();
			g_HostageRagdolls.AddToTail( pLowViolenceModel );
			m_hRagdollOnClient.Set( pLowViolenceModel );
			
			g_HostageRagdolls.AddToTail( m_hHolidayHatAddon );
			if ( m_hHolidayHatAddon.Get() )
			{
				m_hHolidayHatAddon->FollowEntity( pLowViolenceModel );
				m_hHolidayHatAddon.Term();
			}
			return pLowViolenceModel;
		}
		else
		{
			// if we don't have a low-violence death anim, don't create a ragdoll.
			return NULL;
		}
	}

	C_BaseAnimating *pRagdoll = BaseClass::BecomeRagdollOnClient();
	if ( pRagdoll && pRagdoll != this )
	{
		g_HostageRagdolls.AddToTail( pRagdoll );
		g_HostageRagdolls.AddToTail( m_hHolidayHatAddon );
		if ( m_hHolidayHatAddon.Get() )
		{
			m_hHolidayHatAddon->FollowEntity( pRagdoll );
			m_hHolidayHatAddon.Term();
		}
	}
	m_hRagdollOnClient.Set( pRagdoll );
	return pRagdoll;
}

//-----------------------------------------------------------------------------
/** 
 * Set up attachment and pose param indices.
 * We can't do this in the constructor or Spawn() because the data isn't 
 * there yet.
 */
void C_CHostage::Initialize( )
{
	m_eyeAttachment = LookupAttachment( "eyes" );
	m_chestAttachment = LookupAttachment( "chest" );

	m_headYawPoseParam = LookupPoseParameter( "head_yaw" );
	GetPoseParameterRange( m_headYawPoseParam, m_headYawMin, m_headYawMax );

	m_headPitchPoseParam = LookupPoseParameter( "head_pitch" );
	GetPoseParameterRange( m_headPitchPoseParam, m_headPitchMin, m_headPitchMax );

	m_bodyYawPoseParam = LookupPoseParameter( "body_yaw" );
	GetPoseParameterRange( m_bodyYawPoseParam, m_bodyYawMin, m_bodyYawMax );

	Vector pos;
	QAngle angles;

	if (!GetAttachment( m_eyeAttachment, pos, angles ))
	{
		m_vecViewOffset = Vector( 0, 0, 50.0f );
	}
	else
	{
		m_vecViewOffset = pos - GetAbsOrigin();
	}


	if (!GetAttachment( m_chestAttachment, pos, angles ))
	{
		m_lookAt = Vector( 0, 0, 0 );
	}
	else
	{
		Vector forward;
		AngleVectors( angles, &forward );
		m_lookAt = EyePosition() + 100.0f * forward;
	}
}

//-----------------------------------------------------------------------------
CWeaponCSBase* C_CHostage::CSAnim_GetActiveWeapon()
{
	return NULL;
}

//-----------------------------------------------------------------------------
bool C_CHostage::CSAnim_CanMove()
{
	return true;
}

void C_CHostage::EstimateAbsVelocity( Vector& vel )
{
	vel = m_vel;
}

//-----------------------------------------------------------------------------
/**
 * Orient head and eyes towards m_lookAt.
 */
void C_CHostage::UpdateLookAt( CStudioHdr *pStudioHdr )
{
	if (!m_isInit)
	{
		m_isInit = true;
		Initialize( );
	}

	// head yaw
	if (m_headYawPoseParam < 0 || m_bodyYawPoseParam < 0 || m_headPitchPoseParam < 0)
		return;

	if (GetLeader())
	{
		m_lookAt = GetLeader()->EyePosition();
	}
	
	// orient eyes
	m_viewtarget = m_lookAt;

	// blinking
	if (m_blinkTimer.IsElapsed())
	{
		m_blinktoggle = !m_blinktoggle;
		m_blinkTimer.Start( RandomFloat( 1.5f, 4.0f ) );
	}

	// Figure out where we want to look in world space.
	QAngle desiredAngles;
	Vector to = m_lookAt - EyePosition();
	VectorAngles( to, desiredAngles );

	// Figure out where our body is facing in world space.
	float poseParams[MAXSTUDIOPOSEPARAM];
	GetPoseParameters( pStudioHdr, poseParams );
	QAngle bodyAngles( 0, 0, 0 );
	bodyAngles[YAW] = GetRenderAngles()[YAW] + RemapVal( poseParams[m_bodyYawPoseParam], 0, 1, m_bodyYawMin, m_bodyYawMax );


	float flBodyYawDiff = bodyAngles[YAW] - m_flLastBodyYaw;
	m_flLastBodyYaw = bodyAngles[YAW];
	

	// Set the head's yaw.
	float desired = AngleNormalize( desiredAngles[YAW] - bodyAngles[YAW] );
	desired = clamp( desired, m_headYawMin, m_headYawMax );
	m_flCurrentHeadYaw = ApproachAngle( desired, m_flCurrentHeadYaw, HOSTAGE_HEAD_TURN_RATE * gpGlobals->frametime );

	// Counterrotate the head from the body rotation so it doesn't rotate past its target.
	m_flCurrentHeadYaw = AngleNormalize( m_flCurrentHeadYaw - flBodyYawDiff );
	desired = clamp( desired, m_headYawMin, m_headYawMax );
	
	SetPoseParameter( pStudioHdr, m_headYawPoseParam, m_flCurrentHeadYaw );

	
	// Set the head's yaw.
	desired = AngleNormalize( desiredAngles[PITCH] );
	desired = clamp( desired, m_headPitchMin, m_headPitchMax );
	
	m_flCurrentHeadPitch = ApproachAngle( desired, m_flCurrentHeadPitch, HOSTAGE_HEAD_TURN_RATE * gpGlobals->frametime );
	m_flCurrentHeadPitch = AngleNormalize( m_flCurrentHeadPitch );
	SetPoseParameter( pStudioHdr, m_headPitchPoseParam, m_flCurrentHeadPitch );

	SetPoseParameter( pStudioHdr, "head_roll", 0.0f );
}


//-----------------------------------------------------------------------------
/**
 * Look around at various interesting things
 */
void C_CHostage::LookAround( void )
{
	if (GetLeader() == NULL && m_lookAroundTimer.IsElapsed())
	{
		m_lookAroundTimer.Start( RandomFloat( 3.0f, 15.0f ) );

		Vector forward;
		QAngle angles = GetAbsAngles();
		angles[ YAW ] += RandomFloat( m_headYawMin, m_headYawMax );
		angles[ PITCH ] += RandomFloat( m_headPitchMin, m_headPitchMax );
		AngleVectors( angles, &forward );
		m_lookAt = EyePosition() + 100.0f * forward;

		STEAMWORKS_TESTSECRET_AMORTIZE(79);
	}
}

//-----------------------------------------------------------------------------
void C_CHostage::UpdateClientSideAnimation()
{
	if (IsDormant())
	{
		return;
	}

	m_PlayerAnimState->Update( GetAbsAngles()[YAW], GetAbsAngles()[PITCH] );

	// initialize pose parameters
	char *setToZero[] =
	{
		"spine_yaw",
		"head_roll"
	};
	CStudioHdr *pStudioHdr = GetModelPtr();
	for ( int i=0; i < ARRAYSIZE( setToZero ); i++ )
	{
		int index = LookupPoseParameter( pStudioHdr, setToZero[i] );
		if ( index >= 0 )
			SetPoseParameter( pStudioHdr, index, 0 );
	}

	// orient head and eyes
	LookAround();
	UpdateLookAt( pStudioHdr );


	BaseClass::UpdateClientSideAnimation();
}

//-----------------------------------------------------------------------------
void C_CHostage::ClientThink()
{
	C_BaseCombatCharacter::ClientThink();

	int a = 255;

// 	if (  GetRenderAlpha() > 0 )
// 	{
// 		SetNextClientThink( gpGlobals->curtime + 0.001 );
// 	}

	switch ( m_nHostageState )
	{
	case k_EHostageStates_GettingPickedUp:
		{
			a = (1.0f - ((gpGlobals->curtime - m_flGrabSuccessTime) /  CS_HOSTAGE_TRANSTIME_PICKUP)) * 255;
			break;
		}
	case k_EHostageStates_GettingDropped:
		{
			a = ((gpGlobals->curtime - m_flDropStartTime) /  CS_HOSTAGE_TRANSTIME_DROP) * 255;
			break;
		}
	case k_EHostageStates_Rescued:
		{
 			a = (1.0f - ((gpGlobals->curtime - m_flRescueStartTime) /  CS_HOSTAGE_TRANSTIME_RESCUE)) * 255;
			break;
		}
	case k_EHostageStates_BeingCarried:
		{
			a = 0;
			break;
		}
	}

	if ( a < 255 )
		SetRenderMode( kRenderTransAlpha );
	else
		SetRenderMode( kRenderNormal, true );

	a = MAX( 0, a );

	RemoveEffects( EF_NODRAW );
	SetRenderAlpha( a );
}

//-----------------------------------------------------------------------------
void C_CHostage::SetClientSideHolidayHatAddon( bool bEnable )
{
	if ( m_hRagdollOnClient.Get() )
		bEnable = false;	// no hat if we have a ragdoll
	
	if ( bEnable && !m_hHolidayHatAddon.Get() )
	{
		// Create the hat
		C_BreakableProp *pEnt = new C_BreakableProp;
		pEnt->InitializeAsClientEntity( "models/player/holiday/santahat.mdl", false );
		pEnt->FollowEntity( this );
		pEnt->SetLocalOrigin( Vector( 0, 0, 0 ) );
		pEnt->SetLocalAngles( QAngle( 0, 0, 0 ) );
		pEnt->SetUseParentLightingOrigin( true );// This will make it so the weapons get lit with the same ambient cube that the player gets lit with.
		m_hHolidayHatAddon.Set( pEnt );
	}
	if ( !bEnable && m_hHolidayHatAddon.Get() )
	{
		// Remove the hat
		m_hHolidayHatAddon->Release();
		m_hHolidayHatAddon.Term();
	}
}

//-----------------------------------------------------------------------------
void C_CHostage::SetClientSideHoldayHatAddonForAllHostagesAndTheirRagdolls( bool bEnable )
{
	FOR_EACH_VEC( g_Hostages, iHostage )
	{
		g_Hostages[iHostage]->SetClientSideHolidayHatAddon( bEnable );
	}

	for ( int k = 0; k < g_HostageRagdolls.Count(); k+=2 )
	{
		if ( !g_HostageRagdolls[k].Get() )
		{
			if ( g_HostageRagdolls[k+1].Get() )
			{
				( ( C_BreakableProp * ) g_HostageRagdolls[k+1].Get() )->Release();
				g_HostageRagdolls[k+1].Term();
			}
		}
		else
		{
			if ( bEnable && !g_HostageRagdolls[k+1].Get() )
			{
				// Create the hat
				C_BreakableProp *pEnt = new C_BreakableProp;
				pEnt->InitializeAsClientEntity( "models/player/holiday/santahat.mdl", false );
				pEnt->FollowEntity( g_HostageRagdolls[k].Get() );
				pEnt->SetLocalOrigin( Vector( 0, 0, 0 ) );
				pEnt->SetLocalAngles( QAngle( 0, 0, 0 ) );
				pEnt->SetUseParentLightingOrigin( true );// This will make it so the weapons get lit with the same ambient cube that the player gets lit with.
				g_HostageRagdolls[k+1].Set( pEnt );
			}
			if ( !bEnable && g_HostageRagdolls[k+1].Get() )
			{
				// Remove the hat
				( ( C_BreakableProp * ) g_HostageRagdolls[k+1].Get() )->Release();
				g_HostageRagdolls[k+1].Term();
			}
		}
	}
}

//-----------------------------------------------------------------------------
bool C_CHostage::WasRecentlyKilledOrRescued( void )
{
	return ( gpGlobals->curtime < m_flDeadOrRescuedTime );
}

//-----------------------------------------------------------------------------
void C_CHostage::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );

	m_OldLifestate = m_lifeState;
}

//-----------------------------------------------------------------------------
void C_CHostage::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( m_OldLifestate != m_lifeState )
	{
		if( m_lifeState == LIFE_DEAD || m_lifeState == LIFE_DYING )
			m_flDeadOrRescuedTime = gpGlobals->curtime + 2;
	}
}

//-----------------------------------------------------------------------------
void C_CHostage::ImpactTrace( trace_t *pTrace, int iDamageType, char *pCustomImpactName )
{
	static ConVar *violence_hblood = cvar->FindVar( "violence_hblood" );
	if ( violence_hblood && !violence_hblood->GetBool() )
		return;

	BaseClass::ImpactTrace( pTrace, iDamageType, pCustomImpactName );
}

