//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "c_baseanimating.h"
#include "c_sprite.h"
#include "model_types.h"
#include "bone_setup.h"
#if defined( _PS3 )
#include "bone_setup_PS3.h"
#endif
#include "ivrenderview.h"
#include "r_efx.h"
#include "dlight.h"
#include "beamdraw.h"
#include "cl_animevent.h"
#include "engine/IEngineSound.h"
#include "c_te_legacytempents.h"
#include "activitylist.h"
#include "animation.h"
#include "tier0/vprof.h"
#include "IEffects.h"
#include "engine/ivmodelinfo.h"
#include "engine/ivdebugoverlay.h"
#include "c_te_effect_dispatch.h"
#include <keyvalues.h>
#include "c_rope.h"
#include "isaverestore.h"
#include "datacache/imdlcache.h"
#include "eventlist.h"
#include "saverestore.h"
#include "physics_saverestore.h"
#include "vphysics/constraints.h"
#include "ragdoll_shared.h"
#include "view.h"
#include "c_ai_basenpc.h"
#include "c_entitydissolve.h"
#include "saverestoretypes.h"
#include "c_fire_smoke.h"
#include "input.h"
#include "soundinfo.h"
#include "shaderapi/ishaderapi.h"
#include "tools/bonelist.h"
#include "toolframework/itoolframework.h"
#include "datacache/idatacache.h"
#include "gamestringpool.h"
#include "engine/ivdebugoverlay.h"
#include "jigglebones.h"
#include "toolframework_client.h"
#include "vstdlib/jobthread.h"
#include "bonetoworldarray.h"
#include "posedebugger.h"
#include "tier0/icommandline.h"
#include <ctype.h>
#include "prediction.h"
#include "c_entityflame.h"
#include "npcevent.h"
#include "replay_ragdoll.h"
#include "physics_softbody.h"

#if defined ( PORTAL2 )
#include "c_portal_player.h"
#include "portal2/portal_grabcontroller_shared.h"
#endif

#include "clientalphaproperty.h"

#ifdef DEMOPOLISH_ENABLED
#include "demo_polish/demo_polish.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar cl_SetupAllBones( "cl_SetupAllBones", "0" );
ConVar r_sequence_debug( "r_sequence_debug", "" );
ConVar r_debug_sequencesets( "r_debug_sequencesets", "-2" );
ConVar r_jiggle_bones( "r_jiggle_bones", "1" );
ConVar RagdollImpactStrength( "z_ragdoll_impact_strength", "500" );
ConVar cl_disable_ragdolls( "cl_disable_ragdolls", "0", FCVAR_CHEAT );
ConVar r_debug_ik( "r_debug_ik", "" );

ConVar cl_ejectbrass( "cl_ejectbrass", "1" );
ConVar cl_minimal_rtt_shadows( "cl_minimal_rtt_shadows", "1", FCVAR_ARCHIVE );

ConVar cl_custom_material_override( "cl_custom_material_override", "1", FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY, "allow custom material override" );

// [mariod] - am comparing two optimisations to remove what seems like unnecessary bonesetup, particularly during input/aiment code
// method 1. just use stale data in setupbones, by ignoring the condition that enables the path to recalc them
// method 2. Only allow the invalidation of bonecaches to happen during specific sections of the frame
// method 1 uses Enable/DisableNewBoneSetupRequest over critical sections
// method 2 uses Enable/DisableInvalidateBoneCache
// method 2 is enabled, method 1 disabled for now, needs more thorough testing, but I can't see issues on CS:GO with this config
// Have spoken to Yahn about this, and he seems to agree that either the invalidatebonecache calls shouldn't be there,
// and/or the use of stale data during input code should be fine
bool C_BaseAnimating::s_bEnableInvalidateBoneCache	= true;
bool C_BaseAnimating::s_bEnableNewBoneSetupRequest	= true;


#if defined( _PS3 )
//#define DEBUG_BONESETUP_THREADVSNONTHREAD
ConVar cl_PS3_SPU_bones("cl_PS3_SPU_bones", "1", 0, "0: ignore SPU path. 1: run SPU path. 2: emulate SPU path on PPU" );
ConVar cl_PS3_SPU_bones_debug("cl_PS3_SPU_bones_debug", "0", 0, "0: default, run as normal. 1: Force non-parallel SPU jobs, 2: Force debugger break on job entry and PPU execute after SPU job" );
ConVar cl_PS3_SPU_bones_minbonecount("cl_PS3_SPU_bones_minbonecount", "0", 0, "0: default. number of bones below which the bonejob is not pushed to SPU" );
ConVar cl_PS3_SPU_bones_safesync("cl_PS3_SPU_bones_safesync", "1", 1, "0: potentially unsafe sync of dependant jobs, more PPU/SPU overlap, get bone access errors. 1: default safer sync between dependant jobs" );
#endif

// If an NPC is moving faster than this, he should play the running footstep sound
const float RUN_SPEED_ESTIMATE_SQR = 150.0f * 150.0f;

// Removed macro used by shared code stuff
#if defined( CBaseAnimating )
#undef CBaseAnimating
#endif

ConVar sfm_record_hz( "sfm_record_hz", "30" );

static bool g_bInThreadedBoneSetup;

mstudioevent_t *GetEventIndexForSequence( mstudioseqdesc_t &seqdesc );

C_EntityDissolve *DissolveEffect( C_BaseAnimating *pTarget, float flTime );
C_EntityFlame *FireEffect( C_BaseAnimating *pTarget, C_BaseEntity *pServerFire, float *flScaleEnd, float *flTimeStart, float *flTimeEnd );
bool NPC_IsImportantNPC( C_BaseAnimating *pAnimating );
void VCollideWireframe_ChangeCallback( IConVar *pConVar, char const *pOldString, float flOldValue );

ConVar vcollide_wireframe( "vcollide_wireframe", "0", FCVAR_CHEAT, "Render physics collision models in wireframe", VCollideWireframe_ChangeCallback );
ConVar enable_skeleton_draw( "enable_skeleton_draw", "0", FCVAR_CHEAT, "Render skeletons in wireframe" );

extern ConVar r_shadow_deferred;

bool C_AnimationLayer::IsActive( void )
{
	return (m_nOrder != C_BaseAnimatingOverlay::MAX_OVERLAYS);
}

//-----------------------------------------------------------------------------
// Base Animating
//-----------------------------------------------------------------------------

struct clientanimating_t
{
	C_BaseAnimating *pAnimating;
	unsigned int	flags;
	clientanimating_t(C_BaseAnimating *_pAnim, unsigned int _flags ) : pAnimating(_pAnim), flags(_flags) {}
};

const unsigned int FCLIENTANIM_SEQUENCE_CYCLE = 0x00000001;

static CUtlVector< clientanimating_t >	g_ClientSideAnimationList;

BEGIN_RECV_TABLE_NOBASE( C_BaseAnimating, DT_ServerAnimationData )
	RecvPropFloat(RECVINFO(m_flCycle)),
END_RECV_TABLE()


void RecvProxy_Sequence( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	// Have the regular proxy store the data.
	RecvProxy_Int32ToInt32( pData, pStruct, pOut );

	C_BaseAnimating *pAnimating = (C_BaseAnimating *)pStruct;

	if ( !pAnimating )
		return;

	pAnimating->SetReceivedSequence();

	// render bounds may have changed
	pAnimating->UpdateVisibility();

	/*
	if (r_sequence_debug.GetInt() == pAnimating->entindex() )
	{
		DevMsgRT( "%d : RecvProxy_Sequence( %d:%s )\n", pAnimating->entindex(), pAnimating->GetSequence(), pAnimating->GetSequenceName( pAnimating->GetSequence() ) );
		Assert( 1 );
	}
	*/
}

IMPLEMENT_CLIENTCLASS_DT(C_BaseAnimating, DT_BaseAnimating, CBaseAnimating)
	RecvPropInt(RECVINFO(m_nSequence), 0, RecvProxy_Sequence),
	RecvPropInt(RECVINFO(m_nForceBone)),
	RecvPropVector(RECVINFO(m_vecForce)),
	RecvPropInt(RECVINFO(m_nSkin)),
	RecvPropInt(RECVINFO(m_nBody)),
	RecvPropInt(RECVINFO(m_nHitboxSet)),
	RecvPropFloat(RECVINFO(m_flModelScale)),

//	RecvPropArray(RecvPropFloat(RECVINFO(m_flPoseParameter[0])), m_flPoseParameter),
	RecvPropArray3(RECVINFO_ARRAY(m_flPoseParameter), RecvPropFloat(RECVINFO(m_flPoseParameter[0])) ),
	
	RecvPropFloat(RECVINFO(m_flPlaybackRate)),

	RecvPropArray3( RECVINFO_ARRAY(m_flEncodedController), RecvPropFloat(RECVINFO(m_flEncodedController[0]))),

	RecvPropInt( RECVINFO( m_bClientSideAnimation )),
	RecvPropInt( RECVINFO( m_bClientSideFrameReset )),
	RecvPropBool( RECVINFO( m_bClientSideRagdoll )),

	RecvPropInt( RECVINFO( m_nNewSequenceParity )),
	RecvPropInt( RECVINFO( m_nResetEventsParity )),
	RecvPropInt( RECVINFO( m_nMuzzleFlashParity ) ),

	RecvPropEHandle(RECVINFO(m_hLightingOrigin)),

	RecvPropDataTable( "serveranimdata", 0, 0, &REFERENCE_RECV_TABLE( DT_ServerAnimationData ) ),

	RecvPropFloat( RECVINFO( m_flFrozen ) ), 
	RecvPropInt( RECVINFO( m_ScaleType ) ),
	RecvPropBool( RECVINFO( m_bSuppressAnimSounds ) )

END_RECV_TABLE()

BEGIN_PREDICTION_DATA( C_BaseAnimating )

	DEFINE_PRED_FIELD( m_nSkin, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_nBody, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
//	DEFINE_PRED_FIELD( m_nHitboxSet, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
//	DEFINE_PRED_FIELD( m_flModelScale, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	//DEFINE_PRED_FIELD( m_nSequence, FIELD_INTEGER, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_flPlaybackRate, FIELD_FLOAT, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK ),
	//DEFINE_PRED_FIELD( m_flCycle, FIELD_FLOAT, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK ),
//	DEFINE_PRED_ARRAY( m_flPoseParameter, FIELD_FLOAT, MAXSTUDIOPOSEPARAM, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_ARRAY_TOL( m_flEncodedController, FIELD_FLOAT, MAXSTUDIOBONECTRLS, FTYPEDESC_INSENDTABLE, 0.02f ),

	//DEFINE_FIELD( m_flPrevEventCycle, FIELD_FLOAT ),
	//DEFINE_FIELD( m_flEventCycle, FIELD_FLOAT ),
	//DEFINE_FIELD( m_nEventSequence, FIELD_INTEGER ),

	DEFINE_PRED_FIELD( m_nNewSequenceParity, FIELD_INTEGER, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK ),
	DEFINE_PRED_FIELD( m_nResetEventsParity, FIELD_INTEGER, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK ),
	// DEFINE_PRED_FIELD( m_nPrevResetEventsParity, FIELD_INTEGER, 0 ),

	DEFINE_PRED_FIELD( m_nMuzzleFlashParity, FIELD_CHARACTER, FTYPEDESC_INSENDTABLE ),
	//DEFINE_FIELD( m_nOldMuzzleFlashParity, FIELD_CHARACTER ),

	//DEFINE_FIELD( m_nPrevNewSequenceParity, FIELD_INTEGER ),

	// DEFINE_PRED_FIELD( m_vecForce, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	// DEFINE_PRED_FIELD( m_nForceBone, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	// DEFINE_PRED_FIELD( m_bClientSideAnimation, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	// DEFINE_PRED_FIELD( m_bClientSideFrameReset, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	
	// DEFINE_FIELD( m_pRagdollInfo, RagdollInfo_t ),
	// DEFINE_FIELD( m_CachedBones, CUtlVector < CBoneCacheEntry > ),
	// DEFINE_FIELD( m_pActualAttachmentAngles, FIELD_VECTOR ),
	// DEFINE_FIELD( m_pActualAttachmentOrigin, FIELD_VECTOR ),

	// DEFINE_FIELD( m_animationQueue, CUtlVector < CAnimationLayer > ),
	// DEFINE_FIELD( m_pIk, CIKContext ),
	// DEFINE_FIELD( m_bLastClientSideFrameReset, FIELD_BOOLEAN ),
	// DEFINE_FIELD( hdr, studiohdr_t ),
	// DEFINE_FIELD( m_pRagdoll, IRagdoll ),
	// DEFINE_FIELD( m_bStoreRagdollInfo, FIELD_BOOLEAN ),

	// DEFINE_FIELD( C_BaseFlex, m_iEyeAttachment, FIELD_INTEGER ),

END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS_CLIENTONLY( client_ragdoll, C_ClientRagdoll );

BEGIN_DATADESC( C_ClientRagdoll )
	DEFINE_FIELD( m_bFadeOut, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bImportant, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_iCurrentFriction, FIELD_INTEGER ),
	DEFINE_FIELD( m_iMinFriction, FIELD_INTEGER ),
	DEFINE_FIELD( m_iMaxFriction, FIELD_INTEGER ),
	DEFINE_FIELD( m_flFrictionModTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_flFrictionTime, FIELD_TIME ),
	DEFINE_FIELD( m_iFrictionAnimState, FIELD_INTEGER ),
	DEFINE_FIELD( m_bReleaseRagdoll, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nBody, FIELD_INTEGER ),
	DEFINE_FIELD( m_nSkin, FIELD_INTEGER ),
	DEFINE_FIELD( m_nRenderFX, FIELD_CHARACTER ),
	DEFINE_FIELD( m_nRenderMode, FIELD_CHARACTER ),
	DEFINE_FIELD( m_clrRender, FIELD_COLOR32 ),
	DEFINE_FIELD( m_flEffectTime, FIELD_TIME ),
	DEFINE_FIELD( m_bFadingOut, FIELD_BOOLEAN ),

	DEFINE_AUTO_ARRAY( m_flScaleEnd, FIELD_FLOAT ),
	DEFINE_AUTO_ARRAY( m_flScaleTimeStart, FIELD_FLOAT ),
	DEFINE_AUTO_ARRAY( m_flScaleTimeEnd, FIELD_FLOAT ),
	DEFINE_EMBEDDEDBYREF( m_pRagdoll ),

	DEFINE_AUTO_ARRAY( m_flScaleEnd, FIELD_FLOAT ),
	DEFINE_AUTO_ARRAY( m_flScaleTimeStart, FIELD_FLOAT ),
	DEFINE_AUTO_ARRAY( m_flScaleTimeEnd, FIELD_FLOAT ),
	//DEFINE_EMBEDDEDBYREF( m_pRagdoll ), // TODO: FIX: This is dynamically-typed

END_DATADESC()

BEGIN_ENT_SCRIPTDESC( C_BaseAnimating, C_BaseEntity, "Animating models client-side" )
	DEFINE_SCRIPTFUNC_NAMED( ScriptSetPoseParameter, "SetPoseParameter", "Set the specified pose parameter to the specified value"  )
	DEFINE_SCRIPTFUNC( IsSequenceFinished, "Ask whether the main sequence is done playing" )
END_SCRIPTDESC();

C_ClientRagdoll::C_ClientRagdoll( bool bRestoring , bool fullInit)
{
	m_iCurrentFriction = 0;
	m_iFrictionAnimState = RAGDOLL_FRICTION_NONE;
	m_bReleaseRagdoll = false;
	m_bFadeOut = false;
	m_bFadingOut = false;
	m_bImportant = false;

	if(fullInit)
	{
		SetClassname("client_ragdoll");

		if ( bRestoring == true )
		{
			m_pRagdoll = new CRagdoll;
		}
	}
}



void C_ClientRagdoll::OnSave( void )
{
}

void C_ClientRagdoll::OnRestore( void )
{
	CStudioHdr *hdr = GetModelPtr();

	if ( hdr == NULL )
	{
		const char *pModelName = STRING( GetModelName() );
		SetModel( pModelName );

		hdr = GetModelPtr();

		if ( hdr == NULL )
			return;
	}
	
	if ( m_pRagdoll == NULL )
		 return;

	ragdoll_t *pRagdollT = m_pRagdoll->GetRagdoll();

	if ( pRagdollT == NULL || pRagdollT->list[0].pObject == NULL )
	{
		m_bReleaseRagdoll = true;
		m_pRagdoll = NULL;
		Assert( !"Attempted to restore a ragdoll without physobjects!" );
		return;
	}

	if ( GetFlags() & FL_DISSOLVING )
	{
		DissolveEffect( this, m_flEffectTime );
	}
	else if ( GetFlags() & FL_ONFIRE )
	{
		C_EntityFlame *pFireChild = dynamic_cast<C_EntityFlame *>( GetEffectEntity() );
		C_EntityFlame *pNewFireChild = FireEffect( this, pFireChild, m_flScaleEnd, m_flScaleTimeStart, m_flScaleTimeEnd );

		//Set the new fire child as the new effect entity.
		SetEffectEntity( pNewFireChild );
	}

	VPhysicsSetObject( NULL );
	VPhysicsSetObject( pRagdollT->list[0].pObject );

	SetupBones( NULL, -1, BONE_USED_BY_ANYTHING, gpGlobals->curtime );

	pRagdollT->list[0].parentIndex = -1;
	pRagdollT->list[0].originParentSpace.Init();

	RagdollActivate( *pRagdollT, modelinfo->GetVCollide( GetModelIndex() ), GetModelIndex(), true );
	RagdollSetupAnimatedFriction( physenv, pRagdollT, GetModelIndex() );

	m_pRagdoll->BuildRagdollBounds( this );

	// UNDONE: The shadow & leaf system cleanup should probably be in C_BaseEntity::OnRestore()
	// this must be recomputed because the model was NULL when this was set up
	RemoveFromLeafSystem();
	AddToLeafSystem( false );

	DestroyShadow();
 	CreateShadow();

	SetNextClientThink( CLIENT_THINK_ALWAYS );
	
	if ( m_bFadeOut == true )
	{
		s_RagdollLRU.MoveToTopOfLRU( this, m_bImportant );
	}

	NoteRagdollCreationTick( this );
	
	BaseClass::OnRestore();

	RagdollMoved();
}

void C_ClientRagdoll::ImpactTrace( trace_t *pTrace, int iDamageType, char *pCustomImpactName )
{
	VPROF( "C_ClientRagdoll::ImpactTrace" );

	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();

	if( !pPhysicsObject )
		return;

	if ( !pPhysicsObject->IsCollisionEnabled() )
		return;

	Vector dir = pTrace->endpos - pTrace->startpos;

	if ( iDamageType & DMG_BLAST )
	{
		dir *= 500;  // adjust impact strenght

		// apply force at object mass center
		pPhysicsObject->ApplyForceCenter( dir );
	}
	else
	{
		Vector hitpos;  
	
		VectorMA( pTrace->startpos, pTrace->fraction, dir, hitpos );
		VectorNormalize( dir );

		dir *= RagdollImpactStrength.GetFloat();  // adjust impact strength

		// apply force where we hit it
		pPhysicsObject->ApplyForceOffset( dir, hitpos );	
	}

	m_pRagdoll->ResetRagdollSleepAfterTime();
}

ConVar g_debug_ragdoll_visualize( "g_debug_ragdoll_visualize", "0", FCVAR_CHEAT );

void C_ClientRagdoll::HandleAnimatedFriction( void )
{
	if ( m_iFrictionAnimState == RAGDOLL_FRICTION_OFF )
		 return;

	ragdoll_t *pRagdollT = NULL;
	int iBoneCount = 0;

	if ( m_pRagdoll )
	{
		pRagdollT = m_pRagdoll->GetRagdoll();
		iBoneCount = m_pRagdoll->RagdollBoneCount();

	}

	if ( pRagdollT == NULL )
		 return;
	
	switch ( m_iFrictionAnimState )
	{
		case RAGDOLL_FRICTION_NONE:
		{
			m_iMinFriction = pRagdollT->animfriction.minFriction;
			m_iMaxFriction = pRagdollT->animfriction.maxFriction;

			if ( m_iMinFriction != 0 || m_iMaxFriction != 0 )
			{
				m_iFrictionAnimState = RAGDOLL_FRICTION_IN;

				m_flFrictionModTime = pRagdollT->animfriction.timeIn;
				m_flFrictionTime = gpGlobals->curtime + m_flFrictionModTime;
				
				m_iCurrentFriction = m_iMinFriction;
			}
			else
			{
				m_iFrictionAnimState = RAGDOLL_FRICTION_OFF;
			}
			
			break;
		}

		case RAGDOLL_FRICTION_IN:
		{
			float flDeltaTime = (m_flFrictionTime - gpGlobals->curtime);

			m_iCurrentFriction = RemapValClamped( flDeltaTime , m_flFrictionModTime, 0, m_iMinFriction, m_iMaxFriction );

			if ( flDeltaTime <= 0.0f )
			{
				m_flFrictionModTime = pRagdollT->animfriction.timeHold;
				m_flFrictionTime = gpGlobals->curtime + m_flFrictionModTime;
				m_iFrictionAnimState = RAGDOLL_FRICTION_HOLD;
			}
			break;
		}

		case RAGDOLL_FRICTION_HOLD:
		{
			if ( m_flFrictionTime < gpGlobals->curtime )
			{
				m_flFrictionModTime = pRagdollT->animfriction.timeOut;
				m_flFrictionTime = gpGlobals->curtime + m_flFrictionModTime;
				m_iFrictionAnimState = RAGDOLL_FRICTION_OUT;
			}
			
			break;
		}

		case RAGDOLL_FRICTION_OUT:
		{
			float flDeltaTime = (m_flFrictionTime - gpGlobals->curtime);

			m_iCurrentFriction = RemapValClamped( flDeltaTime , 0, m_flFrictionModTime, m_iMinFriction, m_iMaxFriction );

			if ( flDeltaTime <= 0.0f )
			{
				m_iFrictionAnimState = RAGDOLL_FRICTION_OFF;
			}

			break;
		}
	}

	for ( int i = 0; i < iBoneCount; i++ )
	{
		if ( pRagdollT->list[i].pConstraint )
			 pRagdollT->list[i].pConstraint->SetAngularMotor( 0, m_iCurrentFriction );
	}

	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();

	if ( pPhysicsObject )
	{
			pPhysicsObject->Wake();
	}
}

ConVar g_ragdoll_fadespeed( "g_ragdoll_fadespeed", "600" );
ConVar g_ragdoll_lvfadespeed( "g_ragdoll_lvfadespeed", "100" );

void C_ClientRagdoll::OnPVSStatusChanged( bool bInPVS )
{
	if ( bInPVS )
	{
		CreateShadow();
	}
	else
	{
		DestroyShadow();
	}
}

void C_ClientRagdoll::FadeOut( void )
{
	if ( m_bFadingOut == false )
	{
		 return;
	}

	int iAlpha = GetRenderAlpha();
	int iFadeSpeed = ( g_RagdollLVManager.IsLowViolence() ) ? g_ragdoll_lvfadespeed.GetInt() : g_ragdoll_fadespeed.GetInt();

	iAlpha = MAX( iAlpha - ( iFadeSpeed * gpGlobals->frametime ), 0 );

	SetRenderMode( kRenderTransAlpha );
	SetRenderAlpha( iAlpha );

	if ( iAlpha == 0 )
	{
		m_bReleaseRagdoll = true;
	}
}

void C_ClientRagdoll::SUB_Remove( void )
{
	m_bFadingOut = true;
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

//--------------------------------------------------------------------------------------------------------
void C_ClientRagdoll::ClientThink( void )
{
	if ( m_bReleaseRagdoll == true )
	{
		DestroyBoneAttachments();
		Release();
		return;
	}

	if ( g_debug_ragdoll_visualize.GetBool() )
	{
		Vector vMins, vMaxs;
			
		Vector origin = m_pRagdoll->GetRagdollOrigin();
		m_pRagdoll->GetRagdollBounds( vMins, vMaxs );

		debugoverlay->AddBoxOverlay( origin, vMins, vMaxs, QAngle( 0, 0, 0 ), 0, 255, 0, 16, 0 );
	}

	HandleAnimatedFriction();

	FadeOut();
}

//-----------------------------------------------------------------------------
// Purpose: clear out any face/eye values stored in the material system
//-----------------------------------------------------------------------------
float C_ClientRagdoll::LastBoneChangedTime()
{
	// When did this last change?
	return m_pRagdoll ? m_pRagdoll->GetLastVPhysicsUpdateTime() : -FLT_MAX;
}


//----------------------------------------------------------------------------
// Hooks into the fast path render system
//----------------------------------------------------------------------------
IClientModelRenderable*	C_ClientRagdoll::GetClientModelRenderable()
{
	if ( !BaseClass::GetClientModelRenderable() )
		return NULL;

	// NOTE: This is because of code in SetupWeights, which calls SetViewTarget.
	// The view target is a per-instance piece of state which is not yet
	// supported by the model fast path. Once it is, we can eliminate this 
	// code and make it so ragdolls always use the fast path
	if ( m_iEyeAttachment > 0 )
		return NULL;
	return this;
}


//-----------------------------------------------------------------------------
// Purpose: clear out any face/eye values stored in the material system
//-----------------------------------------------------------------------------
void C_ClientRagdoll::SetupWeights( const matrix3x4_t *pBoneToWorld, int nFlexWeightCount, float *pFlexWeights, float *pFlexDelayedWeights )
{
	BaseClass::SetupWeights( pBoneToWorld, nFlexWeightCount, pFlexWeights, pFlexDelayedWeights );

	CStudioHdr *hdr = GetModelPtr();
	if ( !hdr )
		return;

	int nFlexDescCount = hdr->numflexdesc();
	if ( nFlexDescCount )
	{
		memset( pFlexWeights, 0, nFlexWeightCount * sizeof(float) );
		if ( pFlexDelayedWeights )
		{
			memset( pFlexDelayedWeights, 0, nFlexWeightCount * sizeof(float) );
		}
	}

	if ( m_iEyeAttachment > 0 )
	{
		matrix3x4_t attToWorld;
		if ( GetAttachment( m_iEyeAttachment, attToWorld ) )
		{
			Vector local, tmp;
			local.Init( 1000.0f, 0.0f, 0.0f );
			VectorTransform( local, attToWorld, tmp );
			modelrender->SetViewTarget( GetModelPtr(), GetBody(), tmp );
		}
	}
}

void C_ClientRagdoll::Release( void )
{
	C_BaseEntity *pChild = GetEffectEntity();

	if ( pChild && pChild->IsMarkedForDeletion() == false )
	{
		UTIL_Remove( pChild );
	}

	if ( GetThinkHandle() != INVALID_THINK_HANDLE )
	{
		ClientThinkList()->RemoveThinkable( GetClientHandle() );
	}
	ClientEntityList().RemoveEntity( GetClientHandle() );

	::partition->Remove( PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS, CollisionProp()->GetPartitionHandle() );
	RemoveFromLeafSystem();

	BaseClass::Release();
}

//-----------------------------------------------------------------------------
// Incremented each frame in InvalidateModelBones. Models compare this value to what it
// was last time they setup their bones to determine if they need to re-setup their bones.
static unsigned long	g_iModelBoneCounter = 0;
CUtlVector<C_BaseAnimating *> g_PreviousBoneSetups;
static unsigned long	g_iPreviousBoneCounter = (unsigned)-1;

unsigned int g_nNumBonesSetupBlendingRulesOnly;
unsigned int g_nNumBonesSetupBlendingRulesOnlyTemp;
unsigned int g_nNumBonesSetupAll;
unsigned int g_nNumBonesSetupAllTemp;

class C_BaseAnimatingGameSystem : public CAutoGameSystem
{
	void LevelShutdownPostEntity()
	{
		g_iPreviousBoneCounter = (unsigned)-1;
		if ( g_PreviousBoneSetups.Count() != 0 )
		{
			Msg( "%d entities in bone setup array. Should have been cleaned up by now\n", g_PreviousBoneSetups.Count() );
			g_PreviousBoneSetups.RemoveAll();
		}
	}
} g_BaseAnimatingGameSystem;


//-----------------------------------------------------------------------------
// Purpose: convert axis rotations to a quaternion
//-----------------------------------------------------------------------------
C_BaseAnimating::C_BaseAnimating() :
	m_iv_flCycle( "C_BaseAnimating::m_iv_flCycle" ),
	m_iv_flPoseParameter( "C_BaseAnimating::m_iv_flPoseParameter" ),
	m_iv_flEncodedController("C_BaseAnimating::m_iv_flEncodedController")
{

	Assert( (reinterpret_cast<uintp>(this) & 0x0F) == 0 ); // I should be aligned!

	m_nLastNonSkippedFrame = 0;

	m_nCustomBlendingRuleMask = -1;

	ClearAnimLODflags();
	m_nComputedLODframe = 0;
	m_flDistanceFromCamera = 0;

	m_bMaintainSequenceTransitions = true;

	m_iEjectBrassAttachment = -1;

	m_vecForce.Init();
	m_nForceBone = -1;
	SetGlobalFadeScale( 1.0f );
	m_ScaleType = HIERARCHICAL_MODEL_SCALE;

	m_ClientSideAnimationListHandle = INVALID_CLIENTSIDEANIMATION_LIST_HANDLE;

	m_bCanUseFastPath = false;
	m_bIsUsingRelativeLighting = false;
	m_bIsStaticProp = false;
	m_pRagdoll = NULL;
	m_pClientsideRagdoll = NULL;
	m_builtRagdoll = false;

	int i;
	for ( i = 0; i < ARRAYSIZE( m_flEncodedController ); i++ )
	{
		m_flEncodedController[ i ] = 0.0f;
	}

	AddBaseAnimatingInterpolatedVars();

	m_iMostRecentModelBoneCounter = 0xFFFFFFFF;
	m_iMostRecentBoneSetupRequest = g_iPreviousBoneCounter - 1;
	m_flLastBoneSetupTime = -FLT_MAX;
	m_pNextForThreadedBoneSetup = NULL;

	m_vecPreRagdollMins = vec3_origin;
	m_vecPreRagdollMaxs = vec3_origin;

	m_bStoreRagdollInfo = false;
	m_pRagdollInfo = NULL;

	m_flPlaybackRate = 1.0f;

	m_nEventSequence = -1;

	m_pIk = NULL;

	// Assume false.  Derived classes might fill in a receive table entry
	// and in that case this would show up as true
	m_bClientSideAnimation = false;

	m_nPrevNewSequenceParity = -1;
	m_nPrevResetEventsParity = -1;

	m_nOldMuzzleFlashParity = 0;
	m_nMuzzleFlashParity = 0;

	m_boneIndexAttached = -1;

	m_flModelScale = 1.0f;

	m_iEyeAttachment = 0;

	m_pStudioHdr = NULL;
	m_hStudioHdr = MDLHANDLE_INVALID;

	m_bReceivedSequence = false;
	m_bBonePolishSetup = false;
	m_prevClientCycle = 0;
	m_prevClientAnimTime = 0;
	m_flOldModelScale = 0.0f;
	m_vecRenderOriginOverride = vec3_invalid;

	m_pJiggleBones = NULL;
	m_isJiggleBonesEnabled = true;
	AddToEntityList(ENTITY_LIST_SIMULATE);
	m_bForceRTTShadows = false;

	m_flCycle = 0;

	m_bUseParentLightingOrigin = false;

	for ( int i=0; i<MAXSTUDIOBONES; i++ )
	{
		m_pos_cached[i].Init();
		m_q_cached[i].Init();
	}

}

//-----------------------------------------------------------------------------
// Purpose: cleanup
//-----------------------------------------------------------------------------
C_BaseAnimating::~C_BaseAnimating()
{
	Assert( !g_bInThreadedBoneSetup );
	if ( m_iMostRecentBoneSetupRequest == g_iPreviousBoneCounter )
	{
		int i = g_PreviousBoneSetups.Find( this );
		Assert( i != -1 );
		if ( i != -1 )
			g_PreviousBoneSetups.FastRemove( i );
	}
	else
	{
		Assert( g_PreviousBoneSetups.Find( this ) == -1 );
	}

	RemoveFromClientSideAnimationList();

	TermRopes();
	delete m_pRagdollInfo;
	Assert(!m_pRagdoll);
	delete m_pIk;
	delete m_pBoneMergeCache;
	InvalidateMdlCache();
	if ( m_pJiggleBones )
	{
		delete m_pJiggleBones;
		m_pJiggleBones = NULL;
	}

	// Kill off anything bone attached to us.
	DestroyBoneAttachments();

	// clear off any custom materials we might have
	ClearCustomMaterials();
}


bool C_BaseAnimating::UsesPowerOfTwoFrameBufferTexture( void )
{
	return modelinfo->IsUsingFBTexture( GetModel(), GetSkin(), GetBody(), GetClientRenderable() );
}


int C_BaseAnimating::GetRenderFlags( void )
{
	int nRet = 0;
	if ( modelinfo->IsUsingFBTexture( GetModel(), GetSkin(), GetBody(), GetClientRenderable() ) )
		nRet |= ERENDERFLAGS_NEEDS_POWER_OF_TWO_FB;
	return nRet;

}

//-----------------------------------------------------------------------------
// VPhysics object
//-----------------------------------------------------------------------------
int C_BaseAnimating::VPhysicsGetObjectList( IPhysicsObject **pList, int listMax )
{
	if ( IsRagdoll() )
	{
		int i;
		for ( i = 0; i < m_pRagdoll->RagdollBoneCount(); ++i )
		{
			if ( i >= listMax )
				break;

			pList[i] = m_pRagdoll->GetElement(i);
		}
		return i;
	}

	return BaseClass::VPhysicsGetObjectList( pList, listMax );
}


//-----------------------------------------------------------------------------
// Should this object cast render-to-texture shadows?
//-----------------------------------------------------------------------------
ShadowType_t C_BaseAnimating::ShadowCastType()
{
	CStudioHdr *pStudioHdr = GetModelPtr();
	if ( !pStudioHdr || !pStudioHdr->SequencesAvailable() )
		return SHADOWS_NONE;

	if ( IsEffectActive(EF_NODRAW | EF_NOSHADOW) )
		return SHADOWS_NONE;

	if ( cl_minimal_rtt_shadows.GetBool() && m_bForceRTTShadows == false )
	{
		return SHADOWS_NONE;
	}
	else
	{
		return GetShadowCastTypeForStudio( pStudioHdr );
	}
}

ShadowType_t C_BaseAnimating::GetShadowCastTypeForStudio( CStudioHdr *pStudioHdr )
{
	if (pStudioHdr->GetNumSeq() == 0)
		return SHADOWS_RENDER_TO_TEXTURE;
		  
	if ( !IsRagdoll() )
	{
		// If we have pose parameters, always update
		if ( pStudioHdr->GetNumPoseParameters() > 0 )
			return SHADOWS_RENDER_TO_TEXTURE_DYNAMIC;
		
		// If we have bone controllers, always update
		if ( pStudioHdr->numbonecontrollers() > 0 )
			return SHADOWS_RENDER_TO_TEXTURE_DYNAMIC;

		// If we use IK, always update
		if ( pStudioHdr->numikchains() > 0 )
			return SHADOWS_RENDER_TO_TEXTURE_DYNAMIC;
	}

	// FIXME: Do something to check to see how many frames the current animation has
	// If we do this, we have to be able to handle the case of changing ShadowCastTypes
	// at the moment, they are assumed to be constant.
	return SHADOWS_RENDER_TO_TEXTURE;
}

//-----------------------------------------------------------------------------
// Purpose: convert axis rotations to a quaternion
//-----------------------------------------------------------------------------

void C_BaseAnimating::SetPredictable( bool state )
{
	BaseClass::SetPredictable( state );

	UpdateRelevantInterpolatedVars();
}

//-----------------------------------------------------------------------------
// Purpose: sets client side animation
//-----------------------------------------------------------------------------
void C_BaseAnimating::UseClientSideAnimation()
{
	m_bClientSideAnimation = true;
}

void C_BaseAnimating::UpdateRelevantInterpolatedVars()
{
	// Remove any interpolated vars that need to be removed.
	MDLCACHE_CRITICAL_SECTION();
	if ( !GetPredictable() && !IsClientCreated() && GetModelPtr() && GetModelPtr()->SequencesAvailable() && WantsInterpolatedVars() )
	{
		AddBaseAnimatingInterpolatedVars();
	}			
	else
	{
		RemoveBaseAnimatingInterpolatedVars();
	}
}


void C_BaseAnimating::AddBaseAnimatingInterpolatedVars()
{
	AddVar( m_flEncodedController, &m_iv_flEncodedController, LATCH_ANIMATION_VAR, true );
	
	int flags = LATCH_ANIMATION_VAR;
	if ( m_bClientSideAnimation )
		flags |= EXCLUDE_AUTO_INTERPOLATE;
		
	AddVar( m_flPoseParameter, &m_iv_flPoseParameter, flags, true );
	AddVar( &m_flCycle, &m_iv_flCycle, flags, true );
}

void C_BaseAnimating::RemoveBaseAnimatingInterpolatedVars()
{
	RemoveVar( m_flEncodedController, false );
	RemoveVar( m_flPoseParameter, false );
	RemoveVar( &m_flCycle, false );
}

void C_BaseAnimating::LockStudioHdr()
{
	AUTO_LOCK( m_StudioHdrInitLock );
	const model_t *mdl = GetModel();
	if (mdl)
	{
		m_hStudioHdr = modelinfo->GetCacheHandle( mdl );
		if ( m_hStudioHdr != MDLHANDLE_INVALID )
		{
			const studiohdr_t *pStudioHdr = mdlcache->LockStudioHdr( m_hStudioHdr );
			CStudioHdr *pStudioHdrContainer = NULL;
			if ( !m_pStudioHdr )
			{
				if ( pStudioHdr )
				{
					pStudioHdrContainer = new CStudioHdr;
					pStudioHdrContainer->Init( pStudioHdr, mdlcache );
					pStudioHdrContainer->InitSoftbody( &g_SoftbodyEnvironment );
				}
				else
				{
					m_hStudioHdr = MDLHANDLE_INVALID;
				}
			}
			else
			{
				pStudioHdrContainer = m_pStudioHdr;
			}

			Assert( ( pStudioHdr == NULL && pStudioHdrContainer == NULL ) || pStudioHdrContainer->GetRenderHdr() == pStudioHdr );

			if ( pStudioHdrContainer && pStudioHdrContainer->GetVirtualModel() )
			{
				MDLHandle_t hVirtualModel = VoidPtrToMDLHandle( pStudioHdrContainer->GetRenderHdr()->VirtualModel() );
				mdlcache->LockStudioHdr( hVirtualModel );
			}
			m_pStudioHdr = pStudioHdrContainer; // must be last to ensure virtual model correctly set up
		}
	}
}

void C_BaseAnimating::UnlockStudioHdr()
{
	if ( m_pStudioHdr )
	{
		m_pStudioHdr->FreeSoftbody();
		const model_t *mdl = GetModel();
		if (mdl)
		{
			mdlcache->UnlockStudioHdr( m_hStudioHdr );
			if ( m_pStudioHdr->GetVirtualModel() )
			{
				MDLHandle_t hVirtualModel = VoidPtrToMDLHandle( m_pStudioHdr->GetRenderHdr()->VirtualModel() );
				mdlcache->UnlockStudioHdr( hVirtualModel );
			}
		}
	}
}

CStudioHdr *C_BaseAnimating::OnNewModel()
{
	BaseClass::OnNewModel();
	m_bCanUseFastPath = false;

	InvalidateMdlCache();

	// remove transition animations playback
	m_SequenceTransitioner.RemoveAll();

	if (m_pJiggleBones)
	{
		delete m_pJiggleBones;
		m_pJiggleBones = NULL;
	}

	if ( !GetModel() )
		return NULL;

	LockStudioHdr();

	UpdateRelevantInterpolatedVars();

	CStudioHdr *hdr = GetModelPtr();
	if (hdr == NULL)
		return NULL;
	m_bIsStaticProp = ( hdr->flags() & STUDIOHDR_FLAGS_STATIC_PROP ) ? true : false;

	// Can we use the model fast path?
	m_bCanUseFastPath = !modelinfo->ModelHasMaterialProxy( GetModel() ) && GetCustomMaterialCount() == 0;
	if ( m_bCanUseFastPath && m_nSkin != 0 && m_pStudioHdr && m_pStudioHdr->numskinfamilies() > 1 )
	{
		// [REI] Note that this doesn't currently handle the case of a model that starts as skin 0 but
		//       then changes to a different skin index (without triggering NewModel()).
		//
		// $$$REI NOTE THIS DOESN'T SEEM TO BE THE RIGHT ANSWER; the problem we are fixing is that
		// $$$REI material color doesn't seem to work in the fastpath.  But the bug is only triggered
		// $$$REI by WillS colored eggs which use material color set differently on many skins of
		// $$$REI the same model.
		DevWarning( "Model '%s' has skin but thinks it can render fastpath\n", m_pStudioHdr->pszName() );
		m_bCanUseFastPath = false;
	}

	InvalidateBoneCache();
	if ( m_pBoneMergeCache )
	{
		delete m_pBoneMergeCache;
		m_pBoneMergeCache = NULL;
		// recreated in BuildTransformations
	}

	// Make sure m_CachedBones has space.
	if ( m_CachedBoneData.Count() != hdr->numbones() )
	{
		m_CachedBoneData.SetSize( hdr->numbones() );
		for ( int i=0; i < hdr->numbones(); i++ )
		{
			SetIdentityMatrix( m_CachedBoneData[i] );
		}
	}
	m_BoneAccessor.Init( this, m_CachedBoneData.Base() ); // Always call this in case the studiohdr_t has changed.

	// Free any IK data
	if (m_pIk)
	{
		delete m_pIk;
		m_pIk = NULL;
	}

	// Don't reallocate unless a different size. 
	if ( m_Attachments.Count() != hdr->GetNumAttachments() )
	{
		m_Attachments.SetSize( hdr->GetNumAttachments() );

		// This is to make sure we don't use the attachment before its been set up
		for ( int i=0; i < m_Attachments.Count(); i++ )
		{
			m_Attachments[i].m_bAnglesComputed = false;
			m_Attachments[i].m_nLastFramecount = 0;
#ifdef _DEBUG
			m_Attachments[i].m_AttachmentToWorld.Invalidate();
			m_Attachments[i].m_angRotation.Init( VEC_T_NAN, VEC_T_NAN, VEC_T_NAN );
			m_Attachments[i].m_vOriginVelocity.Init( VEC_T_NAN, VEC_T_NAN, VEC_T_NAN );
#endif
		}

	}

	Assert( hdr->GetNumPoseParameters() <= ARRAYSIZE( m_flPoseParameter ) );

	m_iv_flPoseParameter.SetMaxCount( gpGlobals->curtime, hdr->GetNumPoseParameters() );
	
	int i;
	for ( i = 0; i < hdr->GetNumPoseParameters() ; i++ )
	{
		const mstudioposeparamdesc_t &Pose = hdr->pPoseParameter( i );
		m_iv_flPoseParameter.SetLooping( Pose.loop != 0.0f, i );
		// Note:  We can't do this since if we get a DATA_UPDATE_CREATED (i.e., new entity) with both a new model and some valid pose parameters this will slam the 
		//  pose parameters to zero and if the model goes dormant the pose parameter field will never be set to the true value.  We shouldn't have to zero these out
		//  as they are under the control of the server and should be properly set
		if ( !IsServerEntity() )
		{
			SetPoseParameter( hdr, i, 0.0 );
		}
	}

	int boneControllerCount = MIN( hdr->numbonecontrollers(), ARRAYSIZE( m_flEncodedController ) );

	m_iv_flEncodedController.SetMaxCount( gpGlobals->curtime, boneControllerCount );

	for ( i = 0; i < boneControllerCount ; i++ )
	{
		bool loop = (hdr->pBonecontroller( i )->type & (STUDIO_XR | STUDIO_YR | STUDIO_ZR)) != 0;
		m_iv_flEncodedController.SetLooping( loop, i );
		SetBoneController( i, 0.0 );
	}

	InitModelEffects();

	// lookup generic eye attachment, if exists
	m_iEyeAttachment = LookupAttachment( "eyes" );

	// If we didn't have a model before, then we might need to go in the interpolation list now.
	if ( ShouldInterpolate() )
		AddToEntityList( ENTITY_LIST_INTERPOLATE );

	// objects with attachment points need to be queryable even if they're not solid
	if ( hdr->GetNumAttachments() != 0 )
	{
		AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );
	}

	// Most entities clear out their sequences when they change models on the server, but 
	// not all entities network down their m_nSequence (like multiplayer game player entities), 
	// so we need to clear it out here.
	if ( ShouldResetSequenceOnNewModel() )
	{
		SetSequence(0);
	}

	return hdr;
}


//-----------------------------------------------------------------------------
// Purpose: Returns index number of a given named bone
// Input  : name of a bone
// Output :	Bone index number or -1 if bone not found
//-----------------------------------------------------------------------------
int C_BaseAnimating::LookupBone( const char *szName )
{
	Assert( GetModelPtr() );

	if( !GetModelPtr() )
	{
		return -1;
	}

	//AssertMsg( !Q_stristr( szName, "ValveBiped" ), "ValveBiped bone names are deprecated!" );

	int ret = Studio_BoneIndexByName( GetModelPtr(), szName );

	if ( ret == -1 )
	{
		// Try to fix up some common old bone names to new bone names, until I can go through the code and fix all cases or write a data-driven solution.
		if ( Q_stristr( szName, "weapon_bone" ) )
		{
			ret = Studio_BoneIndexByName( GetModelPtr(), "hand_R" );
		}
		else if ( Q_stristr( szName, "Bip01_Head" ) )
		{
			ret = Studio_BoneIndexByName( GetModelPtr(), "head_0" );
		}
		else if ( Q_stristr( szName, "L_Hand" ) )
		{
			ret = Studio_BoneIndexByName( GetModelPtr(), "hand_L" );
		}
		else if ( Q_stristr( szName, "R_Hand" ) )
		{
			ret = Studio_BoneIndexByName( GetModelPtr(), "hand_R" );
		}

		//AssertMsg( ret > 0, "Failed to find an alternate bone name!" );

	}

	return ret;
}

//=========================================================
//=========================================================
void C_BaseAnimating::GetBonePosition ( int iBone, Vector &origin, QAngle &angles )
{
	matrix3x4_t bonetoworld;
	GetBoneTransform( iBone, bonetoworld );
	
	MatrixAngles( bonetoworld, angles, origin );
}

//=========================================================
//=========================================================
void C_BaseAnimating::GetBonePosition ( int iBone, Vector &origin )
{
	matrix3x4_t bonetoworld;
	GetBoneTransform( iBone, bonetoworld );
	
	MatrixPosition( bonetoworld, origin );
}

//=========================================================
//=========================================================
void C_BaseAnimating::GetHitboxBonePosition ( int iBone, Vector &origin, QAngle &angles, QAngle hitboxOrientation )
{
	matrix3x4_t bonetoworld;
	GetBoneTransform( iBone, bonetoworld );
	
	matrix3x4_t temp;
	AngleMatrix( hitboxOrientation, temp);
	MatrixMultiply( bonetoworld, temp, temp );

	MatrixAngles( temp, angles, origin );
}

void C_BaseAnimating::GetHitboxBoneTransform( int iBone, QAngle hitboxOrientation, matrix3x4_t &pOut )
{
	matrix3x4_t bonetoworld;
	GetBoneTransform( iBone, bonetoworld );
	
	matrix3x4_t temp;
	AngleMatrix( hitboxOrientation, temp);
	MatrixMultiply( bonetoworld, temp, pOut );
}

void C_BaseAnimating::GetBoneTransform( int iBone, matrix3x4_t &pBoneToWorld )
{
	CStudioHdr *hdr = GetModelPtr();
	
	if ( hdr && iBone >= 0 && iBone < hdr->numbones() )
	{
		if ( !IsBoneCacheValid() )
		{
			SetupBones( NULL, -1, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
		}
		GetCachedBoneMatrix( iBone, pBoneToWorld );
	}
	else
	{
		AssertMsg( false, "Bone index out of range or null model header." );
		MatrixCopy( EntityToWorldTransform(), pBoneToWorld );
	}


}

//-----------------------------------------------------------------------------
// Purpose: Finds the bone associated with the given hitbox
//-----------------------------------------------------------------------------
int C_BaseAnimating::GetHitboxBone( int hitboxIndex )
{
	CStudioHdr *pStudioHdr = GetModelPtr();
	if ( pStudioHdr )
	{
		mstudiohitboxset_t *set =pStudioHdr->pHitboxSet( m_nHitboxSet );
		if ( set && hitboxIndex < set->numhitboxes )
		{
			return set->pHitbox( hitboxIndex )->bone;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Setup to initialize our model effects once the model's loaded
//-----------------------------------------------------------------------------
void C_BaseAnimating::InitModelEffects( void )
{
	m_bInitModelEffects = true;
	AddToEntityList(ENTITY_LIST_SIMULATE);
	TermRopes();
}

//-----------------------------------------------------------------------------
// Purpose: Load the model's keyvalues section and create effects listed inside it
//-----------------------------------------------------------------------------
void C_BaseAnimating::DelayedInitModelEffects( void )
{
	m_bInitModelEffects = false;

	// Parse the keyvalues and see if they want to make ropes on this model.
	KeyValues * modelKeyValues = new KeyValues("");
	if ( modelKeyValues->LoadFromBuffer( modelinfo->GetModelName( GetModel() ), modelinfo->GetModelKeyValueText( GetModel() ) ) )
	{
		ParseModelEffects( modelKeyValues );
	}
	modelKeyValues->deleteThis();
}

void C_BaseAnimating::ParseModelEffects( KeyValues *modelKeyValues )
{
	// Do we have a cables section?
	KeyValues *pkvAllCables = modelKeyValues->FindKey("Cables");
	if ( pkvAllCables )
	{
		// Start grabbing the sounds and slotting them in
		for ( KeyValues *pSingleCable = pkvAllCables->GetFirstSubKey(); pSingleCable; pSingleCable = pSingleCable->GetNextKey() )
		{
			C_RopeKeyframe *pRope = C_RopeKeyframe::CreateFromKeyValues( this, pSingleCable );
			m_Ropes.AddToTail( pRope );
		}
	}

	// Do we have a particles section?
	KeyValues *pkvAllParticleEffects = modelKeyValues->FindKey("Particles");
	if ( pkvAllParticleEffects )
	{
		// Start grabbing the sounds and slotting them in
		for ( KeyValues *pSingleEffect = pkvAllParticleEffects->GetFirstSubKey(); pSingleEffect; pSingleEffect = pSingleEffect->GetNextKey() )
		{
			const char *pszParticleEffect = pSingleEffect->GetString( "name", "" );
			const char *pszAttachment = pSingleEffect->GetString( "attachment_point", "" );
			const char *pszAttachType = pSingleEffect->GetString( "attachment_type", "" );
			const char *pszAttachOffset = pSingleEffect->GetString( "attachment_offset", "" );

			// Convert attach type
			int iAttachType = GetAttachTypeFromString( pszAttachType );
			if ( iAttachType == -1 )
			{
				Warning("Invalid attach type specified for particle effect in model '%s' keyvalues section. Trying to spawn effect '%s' with attach type of '%s'\n", GetModelName(), pszParticleEffect, pszAttachType );
				return;
			}

			// Convert attachment point
			int iAttachment = atoi(pszAttachment);
			// See if we can find any attachment points matching the name
			if ( pszAttachment[0] != '0' && iAttachment == 0 )
			{
				iAttachment = LookupAttachment( pszAttachment );
				if ( iAttachment == -1 )
				{
					Warning("Failed to find attachment point specified for particle effect in model '%s' keyvalues section. Trying to spawn effect '%s' on attachment named '%s'\n", GetModelName(), pszParticleEffect, pszAttachment );
					return;
				}
			}

			Vector vecOffset = vec3_origin;

			if ( pszAttachOffset )
			{
				float flVec[3];
				UTIL_StringToVector( flVec, pszAttachOffset );
				vecOffset = Vector( flVec[0], flVec[1], flVec[2] );
			}

			CUtlReference<CNewParticleEffect> hModelEffect;
			// Spawn the particle effectw
			hModelEffect = ParticleProp()->Create( pszParticleEffect, (ParticleAttachment_t)iAttachType, iAttachment, vecOffset );

			KeyValues *pkvAllControlPoints = pSingleEffect->FindKey("ControlPoints");
			if ( pkvAllControlPoints )
			{
				// Start grabbing the CPs and slotting them in
				for ( KeyValues *pSingleCP = pkvAllControlPoints->GetFirstSubKey(); pSingleCP; pSingleCP = pSingleCP->GetNextKey() )
				{
					const char *pszControlPoint = pSingleCP->GetString( "cp_number", "" );
					const char *pszAttachment = pSingleCP->GetString( "attachment_point", "" );
					const char *pszAttachType = pSingleCP->GetString( "attachment_type", "" );
					const char *pszAttachOffset = pSingleCP->GetString( "attachment_offset", "" );

					// Convert control point
					int iControlPoint = atoi(pszControlPoint);

					// Convert attach type
					int iAttachType = GetAttachTypeFromString( pszAttachType );
					if ( iAttachType == -1 )
					{
						Warning("Invalid attach type specified for particle effect in model '%s' keyvalues section. Trying to spawn effect '%s' with attach type of '%s'\n", GetModelName(), pszParticleEffect, pszAttachType );
						return;
					}

					Vector vecOffset = vec3_origin;

					if ( pszAttachOffset )
					{
						float flVec[3];
						UTIL_StringToVector( flVec, pszAttachOffset );
						vecOffset = Vector( flVec[0], flVec[1], flVec[2] );
					}

					// Add the control point if we already have the effect
					if ( hModelEffect )
					{
						if ( iAttachType == PATTACH_WORLDORIGIN )
							ParticleProp()->AddControlPoint( hModelEffect, iControlPoint, NULL, (ParticleAttachment_t)iAttachType, NULL, vecOffset );
						else
							ParticleProp()->AddControlPoint( hModelEffect, iControlPoint, this, (ParticleAttachment_t)iAttachType, pszAttachment, vecOffset );
					}

				}
			}
		}
	}
}


void C_BaseAnimating::TermRopes()
{
	FOR_EACH_LL( m_Ropes, i )
	{
		UTIL_Remove( m_Ropes[i] );
	}

	m_Ropes.Purge();
}


// FIXME: redundant?
void C_BaseAnimating::GetBoneControllers(float controllers[MAXSTUDIOBONECTRLS])
{
	// interpolate two 0..1 encoded controllers to a single 0..1 controller
	int i;
	for( i=0; i < MAXSTUDIOBONECTRLS; i++ )
	{
		controllers[ i ] = m_flEncodedController[ i ];
	}
}

float C_BaseAnimating::GetPoseParameterRaw( int iPoseParameter )
{
	CStudioHdr *pStudioHdr = GetModelPtr();

	if ( pStudioHdr == NULL )
		return 0.0f;

	if ( pStudioHdr->GetNumPoseParameters() < iPoseParameter )
		return 0.0f;

	if ( iPoseParameter < 0 )
		return 0.0f;

	return m_flPoseParameter[iPoseParameter];
}

// FIXME: redundant?
void C_BaseAnimating::GetPoseParameters( CStudioHdr *pStudioHdr, float poseParameter[MAXSTUDIOPOSEPARAM])
{
	if ( !pStudioHdr )
		return;

	// interpolate pose parameters
	int i;
	for( i=0; i < pStudioHdr->GetNumPoseParameters(); i++)
	{
		poseParameter[i] = m_flPoseParameter[i];
	}


#if 0 // _DEBUG
	if (r_sequence_debug.GetInt() == entindex())
	{
		DevMsgRT( "%s\n", pStudioHdr->pszName() );
		DevMsgRT( "%6.2f : ", gpGlobals->curtime );
		for( i=0; i < pStudioHdr->GetNumPoseParameters(); i++)
		{
			const mstudioposeparamdesc_t &Pose = pStudioHdr->pPoseParameter( i );

			DevMsgRT( "%s %6.2f ", Pose.pszName(), poseParameter[i] * Pose.end + (1 - poseParameter[i]) * Pose.start );
		}
		DevMsgRT( "\n" );
	}
#endif
}


float C_BaseAnimating::ClampCycle( float flCycle, bool isLooping )
{
	if (isLooping) 
	{
		// FIXME: does this work with negative framerate?
		flCycle = SubtractIntegerPart(flCycle);
		if (flCycle < 0.0f)
		{
			flCycle += 1.0f;
		}
	}
	else 
	{
		flCycle = clamp( flCycle, 0.0f, 0.999f );
	}
	return flCycle;
}


void C_BaseAnimating::EnableJiggleBones( void )
{
	m_isJiggleBonesEnabled = true;
}


void C_BaseAnimating::DisableJiggleBones( void )
{
	m_isJiggleBonesEnabled = false;

	// clear old data so any jiggle bones don't pop if they're enabled again
	if ( m_pJiggleBones )
	{
		delete m_pJiggleBones;
		m_pJiggleBones = NULL;
	}
}

void C_BaseAnimating::ScriptSetPoseParameter( const char *szName, float fValue )
{
	CStudioHdr *pHdr = GetModelPtr();
	if ( pHdr == NULL )
		return;

	int iPoseParam = LookupPoseParameter( pHdr, szName );
	SetPoseParameter( pHdr, iPoseParam, fValue );
}

void C_BaseAnimating::SetRenderOriginOverride( const Vector &vec )
{
	if( m_vecRenderOriginOverride != vec )
	{
		InvalidateBoneCache();
	}
	m_vecRenderOriginOverride = vec;
}
void C_BaseAnimating::DisableRenderOriginOverride( void )
{
	if( m_vecRenderOriginOverride != vec3_invalid )
	{
		InvalidateBoneCache();
	}
	m_vecRenderOriginOverride = vec3_invalid;	
}

void C_BaseAnimating::GetCachedBoneMatrix( int boneIndex, matrix3x4_t &out )
{
	MatrixCopy( GetBone( boneIndex ), out );
}

void C_BaseAnimating::CalcBoneMerge( int boneMask )
{
	// For EF_BONEMERGE entities, copy the bone matrices for any bones that have matching names.
	bool boneMerge = IsEffectActive( EF_BONEMERGE );
	if ( boneMerge || m_pBoneMergeCache )
	{
		if ( boneMerge )
		{
			if ( !m_pBoneMergeCache )
			{
				m_pBoneMergeCache = new CBoneMergeCache;
				m_pBoneMergeCache->Init( this );
			}
			m_pBoneMergeCache->MergeMatchingBones( boneMask );
		}
		else
		{
			delete m_pBoneMergeCache;
			m_pBoneMergeCache = NULL;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose:	move position and rotation transforms into global matrices
//-----------------------------------------------------------------------------
void C_BaseAnimating::BuildTransformations( CStudioHdr *hdr, BoneVector *pos, BoneQuaternion *q, const matrix3x4_t &cameraTransform, int boneMask, CBoneBitList &boneComputed )
{
	VPROF_BUDGET( "C_BaseAnimating::BuildTransformations", ( !g_bInThreadedBoneSetup ) ? VPROF_BUDGETGROUP_CLIENT_ANIMATION : "Client_Animation_Threaded" );
//	SNPROF_ANIM( "C_BaseAnimating::BuildTransformations" );

	if ( !hdr )
		return;

	matrix3x4a_t bonematrix;
	bool boneSimulated[MAXSTUDIOBONES];

	// no bones have been simulated
	memset( boneSimulated, 0, sizeof(boneSimulated) );
	const mstudiobone_t *pbones = hdr->pBone( 0 );
	bool bFixupSimulatedPositions = false;
	if ( m_pRagdoll )
	{
		// simulate bones and update flags
		int oldWritableBones = m_BoneAccessor.GetWritableBones();
		int oldReadableBones = m_BoneAccessor.GetReadableBones();
		m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );
		m_BoneAccessor.SetReadableBones( BONE_USED_BY_ANYTHING );
		
		// If we're playing back a demo, override the ragdoll bones with cached version if available - otherwise, simulate.
#if defined( REPLAY_ENABLED )
		if ( ( !engine->IsPlayingDemo() && !engine->IsPlayingTimeDemo() ) ||
			 !CReplayRagdollCache::Instance().IsInitialized() ||
			 !CReplayRagdollCache::Instance().GetFrame( this, engine->GetDemoPlaybackTick(), boneSimulated, &m_BoneAccessor ) )
#endif
		{
			m_pRagdoll->RagdollBone( this, pbones, hdr->numbones(), boneSimulated, m_BoneAccessor );
		}
		
		m_BoneAccessor.SetWritableBones( oldWritableBones );
		m_BoneAccessor.SetReadableBones( oldReadableBones );
		bFixupSimulatedPositions = !m_pRagdoll->GetRagdoll()->allowStretch;
	}

	CalcBoneMerge( boneMask );

	if ( CSoftbody *pSoftbody = hdr->GetSoftbody() )
	{
		bool bTeleported = ( Teleported() || IsEffectActive( EF_NOINTERP ) );
		pSoftbody->SetAbsOrigin( GetRenderOrigin(), bTeleported );
		pSoftbody->SetAbsAngles( GetRenderAngles(), bTeleported );
		pSoftbody->SetModelScale( GetModelScale() );
	}

	for (int i = 0; i < hdr->numbones(); ++i) 
	{
		// Only update bones reference by the bone mask.
		if ( !( hdr->boneFlags( i ) & boneMask ) )
			continue;

		if ( m_pBoneMergeCache && m_pBoneMergeCache->IsBoneMerged( i ) )
			continue;

		PREFETCH360( &GetBoneForWrite( i ), 0 );

		// animate all non-simulated bones
		if ( boneSimulated[i] )
		{
			ApplyBoneMatrixTransform( GetBoneForWrite( i ) );
			if ( bFixupSimulatedPositions && pbones[i].parent != -1 )
			{
				Vector boneOrigin;
				VectorTransform( pos[i], GetBone(pbones[i].parent), boneOrigin );
				PositionMatrix( boneOrigin, GetBoneForWrite( i ) );
			}
			continue;
		}
		else if ( CalcProceduralBone( hdr, i, m_BoneAccessor ))
		{
			continue;
		}
		// skip bones that the IK has already setup
		else if (boneComputed.IsBoneMarked( i ))
		{
			// dummy operation, just used to verify in debug that this should have happened
			GetBoneForWrite( i );
		}
		else
		{
			QuaternionMatrix( q[i], pos[i], bonematrix );

			Assert( fabs( pos[i].x ) < 100000 );
			Assert( fabs( pos[i].y ) < 100000 );
			Assert( fabs( pos[i].z ) < 100000 );

			if ( (hdr->boneFlags( i ) & BONE_ALWAYS_PROCEDURAL) && 
				(hdr->pBone( i )->proctype & STUDIO_PROC_JIGGLE) &&
				!r_jiggle_bones.GetBool() )
			{
				if ( m_pJiggleBones )
				{
					delete m_pJiggleBones;
					m_pJiggleBones = NULL;
				}
			}

			if ( (hdr->boneFlags( i ) & BONE_ALWAYS_PROCEDURAL) && 
				 (hdr->pBone( i )->proctype & STUDIO_PROC_JIGGLE) &&
				 r_jiggle_bones.GetBool() && m_isJiggleBonesEnabled )
			{
				//
				// Physics-based "jiggle" bone
				// Bone is assumed to be along the Z axis
				// Pitch around X, yaw around Y
				//

				// compute desired bone orientation
				matrix3x4a_t goalMX;

				if (pbones[i].parent == -1) 
				{
					ConcatTransforms( cameraTransform, bonematrix, goalMX );
				} 
				else 
				{
					ConcatTransforms_Aligned( GetBone( pbones[i].parent ), bonematrix, goalMX );
				}

				// get jiggle properties from QC data
				mstudiojigglebone_t *jiggleInfo = (mstudiojigglebone_t *)pbones[i].pProcedure( );

				if (!m_pJiggleBones)
				{
					m_pJiggleBones = new CJiggleBones;
				}

				// do jiggle physics
				m_pJiggleBones->BuildJiggleTransformations( i, gpGlobals->curtime, jiggleInfo, goalMX, GetBoneForWrite( i ), ShouldFlipModel() );

			}
			else if (hdr->boneParent(i) == -1) 
			{
				ConcatTransforms( cameraTransform, bonematrix, GetBoneForWrite( i ) );
			} 
			else 
			{
				ConcatTransforms_Aligned( GetBone( hdr->boneParent(i) ), bonematrix, GetBoneForWrite( i ) );
			}

			if (hdr->boneFlags( i ) & BONE_WORLD_ALIGN)
			{
				Vector vecOrigin = GetBone(i).GetOrigin();
				GetBoneForWrite( i ).Init( Vector(1,0,0), Vector(0,1,0), Vector(0,0,1), vecOrigin );
			}

		}

		if (hdr->boneParent(i) == -1) 
		{
			// Apply client-side effects to the transformation matrix
			ApplyBoneMatrixTransform( GetBoneForWrite( i ) );
		}
	}

	PostBuildTransformations( hdr, pos, q );

	// If a nonhierarchical scale is being applied
	const float scale = GetModelScale();
	if( GetModelScaleType() == NONHIERARCHICAL_MODEL_SCALE &&
		scale > 1.0f+FLT_EPSILON || scale < 1.0f-FLT_EPSILON )
	{
		for( int i = 0; i < hdr->numbones(); ++i ) 
		{
			// Only update bones reference by the bone mask.
			if( !( hdr->boneFlags(i) & boneMask ) )
				continue;

			if( m_pBoneMergeCache && m_pBoneMergeCache->IsBoneMerged(i) )
				continue;

			PREFETCH360( &GetBoneForWrite(i), 0 );

			matrix3x4_t& transform = GetBoneForWrite(i);

			VectorScale( transform[0], scale, transform[0] );
			VectorScale( transform[1], scale, transform[1] );
			VectorScale( transform[2], scale, transform[2] );
		}
	}

	if ( CSoftbody *pSoftbody = hdr->GetSoftbody() )
	{
		pSoftbody->GoWakeup();
		matrix3x4a_t *pBones = m_BoneAccessor.GetBoneArrayForWrite();
		pSoftbody->SetAnimatedTransforms( pBones );
		pSoftbody->FilterTransforms( pBones );
	}

	UpdateBoneAttachments();
}

//-----------------------------------------------------------------------------
// Purpose: Special effects
// Input  : transform - 
//-----------------------------------------------------------------------------
void C_BaseAnimating::ApplyBoneMatrixTransform( matrix3x4_t& transform )
{
	float scale = GetModelHierarchyScale();
	if ( scale > 1.0f+FLT_EPSILON || scale < 1.0f-FLT_EPSILON )
	{
		// The bone transform is in worldspace, so to scale this, we need to translate it back
		Vector pos;
		MatrixGetColumn( transform, 3, pos );
		pos -= GetRenderOrigin();
		pos *= scale;
		pos += GetRenderOrigin();
		MatrixSetColumn( pos, 3, transform );
		
		VectorScale( transform[0], scale, transform[0] );
		VectorScale( transform[1], scale, transform[1] );
		VectorScale( transform[2], scale, transform[2] );
	}
}

void C_BaseAnimating::CreateUnragdollInfo( C_BaseAnimating *pRagdoll )
{
	CStudioHdr *hdr = GetModelPtr();
	if ( !hdr )
	{
		return;
	}

	// It's already an active ragdoll, sigh
	if ( m_pRagdollInfo && m_pRagdollInfo->m_bActive )
	{
		Assert( 0 );
		return;
	}

	// Now do the current bone setup
	pRagdoll->SetupBones( NULL, -1, BONE_USED_BY_ANYTHING, gpGlobals->curtime );

	matrix3x4_t parentTransform;
	QAngle newAngles( 0, pRagdoll->GetAbsAngles()[YAW], 0 );

	AngleMatrix( GetAbsAngles(), GetAbsOrigin(), parentTransform );
	// pRagdoll->SaveRagdollInfo( hdr->numbones, parentTransform, m_BoneAccessor );
	
	if ( !m_pRagdollInfo )
	{
		m_pRagdollInfo = new RagdollInfo_t;
		Assert( m_pRagdollInfo );
		if ( !m_pRagdollInfo )
		{
			Msg( "Memory allocation of RagdollInfo_t failed!\n" );
			return;
		}
	}

	Q_memset( m_pRagdollInfo, 0, sizeof( *m_pRagdollInfo ) );

	int numbones = hdr->numbones();

	m_pRagdollInfo->m_bActive = true;
	m_pRagdollInfo->m_flSaveTime = gpGlobals->curtime;
	m_pRagdollInfo->m_nNumBones = numbones;

	for ( int i = 0;  i < numbones; i++ )
	{
		matrix3x4_t inverted;
		matrix3x4_t output;

		if ( hdr->boneParent(i) == -1 )
		{
			// Decompose into parent space
			MatrixInvert( parentTransform, inverted );
		}
		else
		{
			MatrixInvert( pRagdoll->m_BoneAccessor.GetBone( hdr->boneParent(i) ), inverted );
		}

		ConcatTransforms( inverted, pRagdoll->m_BoneAccessor.GetBone( i ), output );

		MatrixAngles( output, 
			m_pRagdollInfo->m_rgBoneQuaternion[ i ],
			m_pRagdollInfo->m_rgBonePos[ i ] );
	}
}

void C_BaseAnimating::SaveRagdollInfo( int numbones, const matrix3x4_t &cameraTransform, CBoneAccessor &pBoneToWorld )
{
	CStudioHdr *hdr = GetModelPtr();
	if ( !hdr )
	{
		return;
	}

	if ( !m_pRagdollInfo )
	{
		m_pRagdollInfo = new RagdollInfo_t;
		Assert( m_pRagdollInfo );
		if ( !m_pRagdollInfo )
		{
			Msg( "Memory allocation of RagdollInfo_t failed!\n" );
			return;
		}
		memset( m_pRagdollInfo, 0, sizeof( *m_pRagdollInfo ) );
	}

	const mstudiobone_t *pbones = hdr->pBone( 0 );

	m_pRagdollInfo->m_bActive = true;
	m_pRagdollInfo->m_flSaveTime = gpGlobals->curtime;
	m_pRagdollInfo->m_nNumBones = numbones;

	for ( int i = 0;  i < numbones; i++ )
	{
		matrix3x4_t inverted;
		matrix3x4_t output;

		if ( pbones[i].parent == -1 )
		{
			// Decompose into parent space
			MatrixInvert( cameraTransform, inverted );
		}
		else
		{
			MatrixInvert( pBoneToWorld.GetBone( pbones[ i ].parent ), inverted );
		}

		ConcatTransforms( inverted, pBoneToWorld.GetBone( i ), output );

		MatrixAngles( output, 
			m_pRagdollInfo->m_rgBoneQuaternion[ i ],
			m_pRagdollInfo->m_rgBonePos[ i ] );
	}
}

bool C_BaseAnimating::RetrieveRagdollInfo( BoneVector *pos, BoneQuaternion *q )
{
	if ( !m_bStoreRagdollInfo || !m_pRagdollInfo || !m_pRagdollInfo->m_bActive )
		return false;

	for ( int i = 0; i < m_pRagdollInfo->m_nNumBones; i++ )
	{
		pos[ i ] = m_pRagdollInfo->m_rgBonePos[ i ];
		q[ i ] = m_pRagdollInfo->m_rgBoneQuaternion[ i ];
	}

	return true;
}

//-----------------------------------------------------------------------------
// Should we collide?
//-----------------------------------------------------------------------------

CollideType_t C_BaseAnimating::GetCollideType( void )
{
	if ( IsRagdoll() )
		return ENTITY_SHOULD_RESPOND;

	return BaseClass::GetCollideType();
}

//-----------------------------------------------------------------------------
// Purpose: if the active sequence changes, keep track of the previous ones and decay them based on their decay rate
//-----------------------------------------------------------------------------
void C_BaseAnimating::MaintainSequenceTransitions( IBoneSetup &boneSetup, float flCycle, BoneVector pos[], BoneQuaternion q[] )
{
	VPROF( "C_BaseAnimating::MaintainSequenceTransitions" );

	if ( !m_bMaintainSequenceTransitions )
		return;

	if ( !boneSetup.GetStudioHdr() )
		return;

	if ( prediction->InPrediction() )
	{
		m_nPrevNewSequenceParity = m_nNewSequenceParity;
		return;
	}

	if ( IsPlayer() )
	{
		return;
	}

	if ( IsViewModel() && m_nNewSequenceParity != m_nPrevNewSequenceParity && !IsSequenceLooping( GetSequence() ) )
	{
		C_BaseViewModel *pViewModel = assert_cast< C_BaseViewModel* >( this );
		C_BasePlayer *pPlayer = ToBasePlayer( pViewModel->GetOwner() );
		if ( pPlayer && pPlayer->IsHoldingLookAtWeapon() )
		{
			// If we're setting a view model to play the same sequence we need to force the cycle back to zero!
			// For looking at your weapon the client and server timings need to match up so that it can loop properly!
			pViewModel->m_fCycleOffset = -1.0f * ( ( gpGlobals->curtime - m_flAnimTime ) * GetSequenceCycleRate( GetModelPtr(), GetSequence() ) );
			flCycle = 0.0f;
		}
		else
		{
			// Non-lookat animations don't need this offset goofiness
			pViewModel->m_fCycleOffset = 0.0f;
		}
	}

	m_SequenceTransitioner.CheckForSequenceChange( 
		boneSetup.GetStudioHdr(),
		GetSequence(),
		m_nNewSequenceParity != m_nPrevNewSequenceParity,
		!IsEffectActive(EF_NOINTERP)
		);

	m_nPrevNewSequenceParity = m_nNewSequenceParity;

	// Update the transition sequence list.
	m_SequenceTransitioner.UpdateCurrent( 
		boneSetup.GetStudioHdr(),
		GetSequence(),
		flCycle,
		GetPlaybackRate(),
		gpGlobals->curtime
		);


	// process previous sequences
	for (int i = m_SequenceTransitioner.m_animationQueue.Count() - 2; i >= 0; i--)
	{
		CAnimationLayer *blend = &m_SequenceTransitioner.m_animationQueue[i];

		float dt = (gpGlobals->curtime - blend->m_flLayerAnimtime);
		flCycle = blend->GetCycle() + dt * blend->GetPlaybackRate() * GetSequenceCycleRate( boneSetup.GetStudioHdr(), blend->GetSequence() );
		flCycle = ClampCycle( flCycle, IsSequenceLooping( boneSetup.GetStudioHdr(), blend->GetSequence() ) );

#if 1 // _DEBUG
		if (r_sequence_debug.GetInt() == entindex())
		{
			DevMsgRT( "%8.4f : %30s : %5.3f : %4.2f  +\n", gpGlobals->curtime, boneSetup.GetStudioHdr()->pSeqdesc( blend->GetSequence() ).pszLabel(), flCycle, (float)blend->GetWeight() );
		}
#endif

		boneSetup.AccumulatePose( pos, q, blend->GetSequence(), flCycle, blend->GetWeight(), gpGlobals->curtime, m_pIk );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *hdr - 
//			pos[] - 
//			q[] - 
//-----------------------------------------------------------------------------
void C_BaseAnimating::UnragdollBlend( CStudioHdr *hdr, BoneVector pos[], BoneQuaternion q[], float currentTime )
{
//	SNPROF_ANIM( "C_BaseAnimating::UnragdollBlend" );

	if ( !hdr )
	{
		return;
	}

	if ( !m_pRagdollInfo || !m_pRagdollInfo->m_bActive )
		return;

	float dt = currentTime - m_pRagdollInfo->m_flSaveTime;
	if ( dt > 0.2f )
	{
		m_pRagdollInfo->m_bActive = false;
		return;
	}

	// Slerp bone sets together
	float frac = dt / 0.2f;
	frac = clamp( frac, 0.0f, 1.0f );

	int i;
	for ( i = 0; i < hdr->numbones(); i++ )
	{
		VectorLerp( m_pRagdollInfo->m_rgBonePos[ i ], pos[ i ], frac, pos[ i ] );
		QuaternionSlerp( m_pRagdollInfo->m_rgBoneQuaternion[ i ], q[ i ], frac, q[ i ] );
	}
}

void C_BaseAnimating::AccumulateLayers( IBoneSetup &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime )
{
	// Nothing here
}

//-----------------------------------------------------------------------------
// Purpose: Do the default sequence blending rules as done in HL1
//-----------------------------------------------------------------------------
void C_BaseAnimating::StandardBlendingRules( CStudioHdr *hdr, BoneVector pos[], BoneQuaternionAligned q[], float currentTime, int boneMask )
{
	VPROF( "C_BaseAnimating::StandardBlendingRules" );
//	SNPROF_ANIM( "C_BaseAnimating::StandardBlendingRules" );

	float		poseparam[MAXSTUDIOPOSEPARAM];

	if ( !hdr )
		return;

	if ( !hdr->SequencesAvailable() )
	{
		return;
	}

	if (GetSequence() >= hdr->GetNumSeq() || GetSequence() == -1 ) 
	{
		SetSequence( 0 );
	}
	
#ifdef DEMOPOLISH_ENABLED
	if ( IsDemoPolishPlaying() && IsPlayer() )
	{
		float const flDemoPlaybackTime = DemoPolish_GetController().GetAdjustedPlaybackTime();
		int const iSequenceOverride = DemoPolish_GetController().GetSequenceOverride( entindex(), flDemoPlaybackTime );
		if ( iSequenceOverride >= 0 )
		{
			// Override.
			SetSequence( iSequenceOverride );
		}
	}
#endif

	GetPoseParameters( hdr, poseparam );

	// build root animation
	float fCycle = GetCycle();

#if 1 //_DEBUG
	if (r_sequence_debug.GetInt() == entindex())
	{
		DevMsgRT( "%8.4f : %30s(%d) : %5.3f : %4.2f\n", currentTime, hdr->pSeqdesc( GetSequence() ).pszLabel(), GetSequence(), fCycle, 1.0 );
	}
#endif

	IBoneSetup boneSetup( hdr, boneMask, poseparam );
	boneSetup.InitPose( pos, q );
	boneSetup.AccumulatePose( pos, q, GetSequence(), fCycle, 1.0, currentTime, m_pIk );

	// debugoverlay->AddTextOverlay( GetAbsOrigin() + Vector( 0, 0, 64 ), 0, 0, "%30s %6.2f : %6.2f", hdr->pSeqdesc( GetSequence() )->pszLabel( ), fCycle, 1.0 );

	MaintainSequenceTransitions( boneSetup, fCycle, pos, q );

	AccumulateLayers( boneSetup, pos, q, currentTime );

	CIKContext auto_ik;
	auto_ik.Init( hdr, GetRenderAngles(), GetRenderOrigin(), currentTime, gpGlobals->framecount, boneMask );
	boneSetup.CalcAutoplaySequences( pos, q, currentTime, &auto_ik );

	if ( hdr->numbonecontrollers() )
	{
		float controllers[MAXSTUDIOBONECTRLS];
		GetBoneControllers(controllers);
		boneSetup.CalcBoneAdj( pos, q, controllers );
	}
	UnragdollBlend( hdr, pos, q, currentTime );

#ifdef STUDIO_ENABLE_PERF_COUNTERS
#if _DEBUG
	/*
	if (r_sequence_debug.GetInt() == entindex() )
	{
		DevMsgRT( "layers %4d : bones %4d : animated %4d\n", hdr->m_nPerfAnimationLayers, hdr->m_nPerfUsedBones, hdr->m_nPerfAnimatedBones );
	}
	*/
#endif
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Put a value into an attachment point by index
// Input  : number - which point
// Output : float * - the attachment point
//-----------------------------------------------------------------------------
bool C_BaseAnimating::PutAttachment( int number, const matrix3x4_t &attachmentToWorld )
{
	if ( number < 1 || number > m_Attachments.Count() )
		return false;

	CAttachmentData *pAtt = &m_Attachments[number-1];
	if ( gpGlobals->frametime > 0 && pAtt->m_nLastFramecount > 0 && pAtt->m_nLastFramecount == gpGlobals->framecount - 1 )
	{
		Vector vecPreviousOrigin, vecOrigin;
		MatrixPosition( pAtt->m_AttachmentToWorld, vecPreviousOrigin );
		MatrixPosition( attachmentToWorld, vecOrigin );
		pAtt->m_vOriginVelocity = (vecOrigin - vecPreviousOrigin) / gpGlobals->frametime;
	}
	else
	{
		pAtt->m_vOriginVelocity.Init();
	}
	pAtt->m_nLastFramecount = gpGlobals->framecount;
	pAtt->m_bAnglesComputed = false;
	pAtt->m_AttachmentToWorld = attachmentToWorld;

#ifdef _DEBUG
	pAtt->m_angRotation.Init( VEC_T_NAN, VEC_T_NAN, VEC_T_NAN );
#endif

	return true;
}

// when hierarchy changes, these are no longer valid for comparison.  munge the frame counter so they
// get ignored
void C_BaseAnimating::InvalidateAttachments()
{
	int frameCount = -1;
	for ( int i = 0; i < m_Attachments.Count(); i++ )
	{
		m_Attachments[i].m_nLastFramecount = frameCount;
	}
}


void C_BaseAnimating::SetupBones_AttachmentHelper( CStudioHdr *hdr )
{
	if ( !hdr || !hdr->GetNumAttachments() )
		return;

	// calculate attachment points
	matrix3x4_t world;
	for (int i = 0; i < hdr->GetNumAttachments(); i++)
	{
		const mstudioattachment_t &pattachment = hdr->pAttachment( i );
		int iBone = hdr->GetAttachmentBone( i );
		if ( (pattachment.flags & ATTACHMENT_FLAG_WORLD_ALIGN) == 0 )
		{
			ConcatTransforms( GetBone( iBone ), pattachment.local, world ); 
		}
		else
		{
			Vector vecLocalBonePos, vecWorldBonePos;
			MatrixGetColumn( pattachment.local, 3, vecLocalBonePos );
			VectorTransform( vecLocalBonePos, GetBone( iBone ), vecWorldBonePos );

			SetIdentityMatrix( world );
			PositionMatrix( vecWorldBonePos, world );
		}

		// FIXME: this shouldn't be here, it should client side on-demand only and hooked into the bone cache!!
		FormatViewModelAttachment( i, world );
		PutAttachment( i + 1, world );
	}
}

bool C_BaseAnimating::CalcAttachments()
{
	VPROF( "C_BaseAnimating::CalcAttachments" );
//	SNPROF_ANIM( "C_BaseAnimating::CalcAttachments" );


	// Make sure m_CachedBones is valid.
	return SetupBones( NULL, -1, BONE_USED_BY_ATTACHMENT, gpGlobals->curtime );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the world location and world angles of an attachment
// Input  : attachment name
// Output :	location and angles
//-----------------------------------------------------------------------------
bool C_BaseAnimating::GetAttachment( const char *szName, Vector &absOrigin, QAngle &absAngles )
{																
	return GetAttachment( LookupAttachment( szName ), absOrigin, absAngles );
}

//-----------------------------------------------------------------------------
// Purpose: Get attachment point by index
// Input  : number - which point
// Output : float * - the attachment point
//-----------------------------------------------------------------------------
bool C_BaseAnimating::GetAttachment( int number, Vector &origin, QAngle &angles )
{
	// Note: this could be more efficient, but we want the matrix3x4_t version of GetAttachment to be the origin of
	// attachment generation, so a derived class that wants to fudge attachments only 
	// has to reimplement that version. This also makes it work like the server in that regard.
	if ( number < 1 || number > m_Attachments.Count() || !CalcAttachments() )
	{
		// Set this to the model origin/angles so that we don't have stack fungus in origin and angles.
		origin = GetAbsOrigin();
		angles = GetAbsAngles();
		return false;
	}

	CAttachmentData *pData = &m_Attachments[number-1];
	if ( !pData->m_bAnglesComputed )
	{
		MatrixAngles( pData->m_AttachmentToWorld, pData->m_angRotation );
		pData->m_bAnglesComputed = true;

	}
	angles = pData->m_angRotation;
	MatrixPosition( pData->m_AttachmentToWorld, origin );
	return true;
}

bool C_BaseAnimating::GetAttachment( int number, matrix3x4_t& matrix )
{
	if ( number < 1 || number > m_Attachments.Count() )
		return false;

	if ( !CalcAttachments() )
		return false;

	matrix = m_Attachments[number-1].m_AttachmentToWorld;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Get attachment point by index (position only)
// Input  : number - which point
//-----------------------------------------------------------------------------
bool C_BaseAnimating::GetAttachment( int number, Vector &origin )
{
	// Note: this could be more efficient, but we want the matrix3x4_t version of GetAttachment to be the origin of
	// attachment generation, so a derived class that wants to fudge attachments only 
	// has to reimplement that version. This also makes it work like the server in that regard.
	matrix3x4_t attachmentToWorld;
	if ( !GetAttachment( number, attachmentToWorld ) )
	{
		// Set this to the model origin/angles so that we don't have stack fungus in origin and angles.
		origin = GetAbsOrigin();
		return false;
	}

	MatrixPosition( attachmentToWorld, origin );
	return true;
}


bool C_BaseAnimating::GetAttachment( const char *szName, Vector &absOrigin )
{
	return GetAttachment( LookupAttachment( szName ), absOrigin );
}


bool C_BaseAnimating::ComputeLightingOrigin( int nAttachmentIndex, Vector modelLightingCenter, const matrix3x4_t &matrix, Vector &transformedLightingCenter )
{
	if ( m_bUseParentLightingOrigin )
	{
 		if ( GetMoveParent() != NULL )
		{
			C_BaseAnimating *attachmentParent = GetMoveParent()->GetBaseAnimating();
			if ( NULL != attachmentParent )
			{
				if ( attachmentParent->ComputeLightingOrigin(nAttachmentIndex, attachmentParent->GetModelPtr()->illumposition(), attachmentParent->RenderableToWorldTransform(), transformedLightingCenter) )
					return true;
			}
		}
	}

	return BaseClass::ComputeLightingOrigin( nAttachmentIndex, modelLightingCenter, matrix, transformedLightingCenter );
}


bool C_BaseAnimating::GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel )
{
	if ( number < 1 || number > m_Attachments.Count() )
	{
		return false;
	}

	if ( !CalcAttachments() )
		return false;

	originVel = m_Attachments[number-1].m_vOriginVelocity;
	angleVel.Init();

	return true;
}


//-----------------------------------------------------------------------------
// Returns the attachment in local space
//-----------------------------------------------------------------------------
bool C_BaseAnimating::GetAttachmentLocal( int iAttachment, matrix3x4_t &attachmentToLocal )
{
	matrix3x4_t attachmentToWorld;
	if (!GetAttachment(iAttachment, attachmentToWorld))
		return false;

	matrix3x4_t worldToEntity;
	MatrixInvert( EntityToWorldTransform(), worldToEntity );
	ConcatTransforms( worldToEntity, attachmentToWorld, attachmentToLocal ); 
	return true;
}

bool C_BaseAnimating::GetAttachmentLocal( int iAttachment, Vector &origin, QAngle &angles )
{
	matrix3x4_t attachmentToEntity;

	if ( GetAttachmentLocal( iAttachment, attachmentToEntity ) )
	{
		origin.Init( attachmentToEntity[0][3], attachmentToEntity[1][3], attachmentToEntity[2][3] );
		MatrixAngles( attachmentToEntity, angles );
		return true;
	}
	return false;
}

bool C_BaseAnimating::GetAttachmentLocal( int iAttachment, Vector &origin )
{
	matrix3x4_t attachmentToEntity;

	if ( GetAttachmentLocal( iAttachment, attachmentToEntity ) )
	{
		MatrixPosition( attachmentToEntity, origin );
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Move sound location to center of body
//-----------------------------------------------------------------------------
bool C_BaseAnimating::GetSoundSpatialization( SpatializationInfo_t& info )
{
	{
		C_BaseAnimating::AutoAllowBoneAccess boneaccess( true, false );
		if ( !BaseClass::GetSoundSpatialization( info ) )
			return false;
	}

	// move sound origin to center if npc has IK
	if ( info.pOrigin && IsNPC() && m_pIk)
	{
		*info.pOrigin = GetAbsOrigin();

		Vector mins, maxs, center;

		modelinfo->GetModelBounds( GetModel(), mins, maxs );
		VectorAdd( mins, maxs, center );
		VectorScale( center, 0.5f, center );

		(*info.pOrigin) += center;
	}
	return true;
}


bool C_BaseAnimating::IsViewModel() const
{
	return false;
}

bool C_BaseAnimating::IsViewModelOrAttachment() const
{
	return false;
}

bool C_BaseAnimating::IsMenuModel() const
{
	return false;
}

class CTraceFilterSkipNPCsAndPlayers : public CTraceFilterSimple
{
public:
	CTraceFilterSkipNPCsAndPlayers( const IHandleEntity *passentity, int collisionGroup )
		: CTraceFilterSimple( passentity, collisionGroup )
	{
	}

	virtual bool ShouldHitEntity( IHandleEntity *pServerEntity, int contentsMask )
	{
		if ( CTraceFilterSimple::ShouldHitEntity(pServerEntity, contentsMask) )
		{
			C_BaseEntity *pEntity = EntityFromEntityHandle( pServerEntity );
			if ( !pEntity )
				return true;

			do
			{
				if ( pEntity->IsNPC() || pEntity->IsPlayer() )
				{
					return false;
				}
			} while ( ( pEntity = pEntity->GetMoveParent() ) != NULL );

			return true;
		}
		return false;
	}
};


/*
void drawLine(const Vector& origin, const Vector& dest, int r, int g, int b, bool noDepthTest, float duration) 
{
	debugoverlay->AddLineOverlay( origin, dest, r, g, b, noDepthTest, duration );
}
*/

//-----------------------------------------------------------------------------
// Purpose: update latched IK contacts if they're in a moving reference frame.
//-----------------------------------------------------------------------------

void C_BaseAnimating::UpdateIKLocks( float currentTime )
{
	if (!m_pIk) 
		return;

	int targetCount = m_pIk->m_target.Count();
	if ( targetCount == 0 )
		return;

	for (int i = 0; i < targetCount; i++)
	{
		CIKTarget *pTarget = &m_pIk->m_target[i];

		if (!pTarget->IsActive())
			continue;

		if (pTarget->GetOwner() != -1)
		{
			C_BaseEntity *pOwner = cl_entitylist->GetEnt( pTarget->GetOwner() );
			if (pOwner != NULL)
			{
				pTarget->UpdateOwner( pOwner->entindex(), pOwner->GetAbsOrigin(), pOwner->GetAbsAngles() );
			}				
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Find the ground or external attachment points needed by IK rules
//-----------------------------------------------------------------------------

void C_BaseAnimating::CalculateIKLocks( float currentTime )
{
//	SNPROF_ANIM( "C_BaseAnimating::CalculateIKLocks" );

	if (!m_pIk)  
		return;

	int targetCount = m_pIk->m_target.Count();
	if ( targetCount == 0 )
		return;

	// In TF, we might be attaching a player's view to a walking model that's using IK. If we are, it can
	// get in here during the view setup code, and it's not normally supposed to be able to access the spatial
	// partition that early in the rendering loop. So we allow access right here for that special case.
	SpatialPartitionListMask_t curSuppressed = ::partition->GetSuppressedLists();
	::partition->SuppressLists( PARTITION_ALL_CLIENT_EDICTS, false );
	CBaseEntity::PushEnableAbsRecomputations( false );

	Ray_t ray;
	CTraceFilterSkipNPCsAndPlayers traceFilter( this, GetCollisionGroup() );

	// FIXME: trace based on gravity or trace based on angles?
	Vector up;
	AngleVectors( GetRenderAngles(), NULL, NULL, &up );

	for (int i = 0; i < targetCount; i++)
	{
		trace_t trace;
		CIKTarget *pTarget = &m_pIk->m_target[i];

		if (!pTarget->IsActive())
			continue;

		switch( pTarget->type)
		{
		case IK_GROUND:
			{
				Vector estGround;

				// adjust ground to original ground position
				estGround = (pTarget->est.pos - GetRenderOrigin());
				// estGround = estGround - (estGround * up) * up;
				estGround = GetAbsOrigin() + estGround;

				estGround.z = GetAbsOrigin().z;

				pTarget->SetPos( estGround );
				pTarget->SetAngles( GetRenderAngles() );
				pTarget->SetOnWorld( true );

				/*
				if ( pTarget->est.flWeight == 1.0f )
				{
					debugoverlay->AddBoxOverlay( estGround, Vector( -5, -5, -1 ), Vector( 5, 5, 0), QAngle( 0, 0, 0 ), 255, 0, 0, 0, 1.0f );
				}
				*/

				/*
				debugoverlay->AddTextOverlay( p1, i, 0, "%d %.1f %.1f %.1f ", i, 
					pTarget->latched.deltaPos.x, pTarget->latched.deltaPos.y, pTarget->latched.deltaPos.z );
				debugoverlay->AddBoxOverlay( pTarget->est.pos, Vector( -r, -r, -1 ), Vector( r, r, 1), QAngle( 0, 0, 0 ), 255, 0, 0, 0, 0 );
				*/
				// debugoverlay->AddBoxOverlay( pTarget->latched.pos, Vector( -2, -2, 2 ), Vector( 2, 2, 6), QAngle( 0, 0, 0 ), 0, 255, 0, 0, 0 );
			}
			break;

		case IK_ATTACHMENT:
			{
				C_BaseEntity *pEntity = NULL;
				float flDist = pTarget->est.radius;

				// FIXME: make entity finding sticky!
				// FIXME: what should the radius check be?
				for ( CEntitySphereQuery sphere( pTarget->est.pos, 64, 0, PARTITION_CLIENT_IK_ATTACHMENT ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
				{
					C_BaseAnimating *pAnim = pEntity->GetBaseAnimating( );
					if (!pAnim)
						continue;

					int iAttachment = pAnim->LookupAttachment( pTarget->offset.pAttachmentName );
					if (iAttachment <= 0)
						continue;

					Vector origin;
					QAngle angles;
					pAnim->GetAttachment( iAttachment, origin, angles );

					// debugoverlay->AddBoxOverlay( origin, Vector( -1, -1, -1 ), Vector( 1, 1, 1 ), QAngle( 0, 0, 0 ), 255, 0, 0, 0, 0 );

					float d = (pTarget->est.pos - origin).Length();

					if ( d >= flDist)
						continue;

					flDist = d;
					pTarget->SetPos( origin );
					pTarget->SetAngles( angles );
					// debugoverlay->AddBoxOverlay( pTarget->est.pos, Vector( -pTarget->est.radius, -pTarget->est.radius, -pTarget->est.radius ), Vector( pTarget->est.radius, pTarget->est.radius, pTarget->est.radius), QAngle( 0, 0, 0 ), 0, 255, 0, 0, 0 );
				}

				if (flDist >= pTarget->est.radius)
				{
					// debugoverlay->AddBoxOverlay( pTarget->est.pos, Vector( -pTarget->est.radius, -pTarget->est.radius, -pTarget->est.radius ), Vector( pTarget->est.radius, pTarget->est.radius, pTarget->est.radius), QAngle( 0, 0, 0 ), 0, 0, 255, 0, 0 );
					// no solution, disable ik rule
					pTarget->IKFailed( );
				}
			}
			break;
		}
	}

#if defined( HL2_CLIENT_DLL )
	if (minHeight < FLT_MAX)
	{
		input->AddIKGroundContactInfo( entindex(), minHeight, maxHeight );
	}
#endif

	CBaseEntity::PopEnableAbsRecomputations();
	::partition->SuppressLists( curSuppressed, true );
}

bool C_BaseAnimating::GetPoseParameterRange( int index, float &minValue, float &maxValue )
{
	CStudioHdr *pStudioHdr = GetModelPtr();

	if (pStudioHdr)
	{
		if (index >= 0 && index < pStudioHdr->GetNumPoseParameters())
		{
			const mstudioposeparamdesc_t &pose = pStudioHdr->pPoseParameter( index );
			minValue = pose.start;
			maxValue = pose.end;
			return true;
		}
	}
	minValue = 0.0f;
	maxValue = 1.0f;
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Do HL1 style lipsynch
//-----------------------------------------------------------------------------
void C_BaseAnimating::ControlMouth( CStudioHdr *pstudiohdr )
{
	if ( !MouthInfo().NeedsEnvelope() )
		return;

	if ( !pstudiohdr )
		  return;

	int index = LookupPoseParameter( pstudiohdr, LIPSYNC_POSEPARAM_NAME );

	if ( index != -1 )
	{
		float value = GetMouth()->mouthopen / 64.0;

		float raw = value;

		if ( value > 1.0 )  
			 value = 1.0;

		float start, end;
		GetPoseParameterRange( index, start, end );

		value = (1.0 - value) * start + value * end;

		//Adrian - Set the pose parameter value. 
		//It has to be called "mouth".
		SetPoseParameter( pstudiohdr, index, value ); 
		// Reset interpolation here since the client is controlling this rather than the server...
		m_iv_flPoseParameter.SetHistoryValuesForItem( index, raw );
	}
}

CMouthInfo *C_BaseAnimating::GetMouth( void )
{
	return &m_mouth;
}

#ifdef DEBUG_BONE_SETUP_THREADING
ConVar cl_warn_thread_contested_bone_setup("cl_warn_thread_contested_bone_setup", "0" );
#endif
// 7LS - turning off threaded bones on X360 until we find out why there is a perf hit, even though we are running the bonesetups in parallel!
ConVar cl_threaded_bone_setup( "cl_threaded_bone_setup", IsX360() ? "1" : (IsPS3() ? "2" : "0"), FCVAR_RELEASE, "Enable parallel processing of bones" );

//-----------------------------------------------------------------------------
// Purpose: Do the default sequence blending rules as done in HL1
//-----------------------------------------------------------------------------

#ifdef DEBUG_BONE_SETUP_THREADING
CThreadLocalInt<> *pCount;
#endif

void C_BaseAnimating::SetupBonesOnBaseAnimating( C_BaseAnimating *&pBaseAnimating )
{
	C_BaseAnimating *pCurrent = pBaseAnimating;
	C_BaseAnimating *pNext;
	while ( pCurrent )
	{
		pNext = pCurrent->m_pNextForThreadedBoneSetup;
		pCurrent->m_pNextForThreadedBoneSetup = NULL;
		pCurrent->SetupBones( NULL, -1, -1, gpGlobals->curtime );
		pCurrent = pNext;
	}

#ifdef DEBUG_BONE_SETUP_THREADING
	(*pCount)++;
#endif
}

static void PreThreadedBoneSetup()
{
	mdlcache->BeginCoarseLock();
	mdlcache->BeginLock();
}

static void PostThreadedBoneSetup()
{
	mdlcache->EndLock();
	mdlcache->EndCoarseLock();
#ifdef DEBUG_BONE_SETUP_THREADING
 	Msg( "  %x done, %d\n", ThreadGetCurrentId(), (int)(*pCount) );
 	(*pCount) = 0;
#endif
}

static bool g_bDoThreadedBoneSetup;
IThreadPool *g_pBoneSetupThreadPool;

void C_BaseAnimating::InitBoneSetupThreadPool()
{
#ifdef DEBUG_BONE_SETUP_THREADING
	pCount = new CThreadLocalInt<>;
#endif
#ifdef _X360
	if ( g_pAlternateThreadPool )
	{
		g_pBoneSetupThreadPool = g_pAlternateThreadPool;
	}
	else
#endif
	{
		g_pBoneSetupThreadPool = g_pThreadPool;
	}

	// setup SPU bonejobs
#if defined( _PS3 )
	g_pBoneJobs->Init();
#endif
}				 

void C_BaseAnimating::ShutdownBoneSetupThreadPool()
{
}

void C_BaseAnimating::MarkForThreadedBoneSetup()
{
//	SNPROF_ANIM( "C_BaseAnimating::MarkForThreadedBoneSetup" );

	if ( g_bDoThreadedBoneSetup && !g_bInThreadedBoneSetup && m_iMostRecentBoneSetupRequest != g_iPreviousBoneCounter )
	{
		if ( !IsViewModel() )
		{
			// This function is protected by m_BoneSetupLock (see SetupBones)
			if ( m_iMostRecentBoneSetupRequest != g_iPreviousBoneCounter )
			{
//				SNPROF_ANIM( "C_BaseAnimating::MarkForThreadedBoneSetup_AddToTail" );

#ifdef DEBUG_BONESETUP_THREADVSNONTHREAD
				Msg("MARK FOR THREADED: %x\n", this );
#endif

				m_iMostRecentBoneSetupRequest = g_iPreviousBoneCounter;
				LOCAL_THREAD_LOCK();
				Assert( g_PreviousBoneSetups.Find( this ) == -1 );
				g_PreviousBoneSetups.AddToTail( this );
			}
		}
	}

}

void C_BaseAnimating::ThreadedBoneSetup()
{
#ifdef _PS3
    g_bDoThreadedBoneSetup = 1;
#else
	g_bDoThreadedBoneSetup = ( g_pBoneSetupThreadPool && g_pBoneSetupThreadPool->NumThreads() && cl_threaded_bone_setup.GetInt() );
#endif
	if ( g_bDoThreadedBoneSetup )
	{
		int nCount = g_PreviousBoneSetups.Count();
#if defined( _PS3 )
		if ( nCount > 0 )
#else
		if ( nCount > 1 )
#endif
		{
			VPROF_BUDGET( "C_BaseAnimating::ThreadedBoneSetup", "Client_Animation_Threaded" );
			SNPROF_ANIM( "C_BaseAnimating::ThreadedBoneSetup" );

#ifdef DEBUG_BONE_SETUP_THREADING
			Msg( "{\n" );
#endif
			// This loop is here rather than the mark function so we don't have to worry about the list being threadsafe, or worry about entity destruction
#ifdef _DEBUG
			CUtlVector<C_BaseAnimating *> test;
			test.AddVectorToTail( g_PreviousBoneSetups );
#endif
			for ( int i = g_PreviousBoneSetups.Count() - 1; i >= 0; i-- )
			{
				C_BaseAnimating *pAnimating = g_PreviousBoneSetups[i];
				C_BaseAnimating *pDependancy;
				if ( (pDependancy = pAnimating->GetBoneSetupDependancy()) != NULL )
				{
					Assert( pAnimating->m_pNextForThreadedBoneSetup == NULL );
					C_BaseAnimating *pNextDependancy;
					while ( (pNextDependancy = pDependancy->GetBoneSetupDependancy()) != NULL )
					{
						pDependancy = pNextDependancy;
					}
					
					pAnimating->m_pNextForThreadedBoneSetup = pDependancy->m_pNextForThreadedBoneSetup;
					pDependancy->m_pNextForThreadedBoneSetup = pAnimating;
					g_PreviousBoneSetups.FastRemove( i );
					if ( pDependancy->m_iMostRecentBoneSetupRequest != g_iPreviousBoneCounter )
					{
						Assert( g_PreviousBoneSetups.Find( pDependancy ) == -1 );
						pDependancy->m_iMostRecentBoneSetupRequest = g_iPreviousBoneCounter;
						g_PreviousBoneSetups.AddToTail( pDependancy );
					}
				}
			}
			nCount = g_PreviousBoneSetups.Count();

			g_bInThreadedBoneSetup = true;
			if ( cl_threaded_bone_setup.GetInt() == 1 )
			{
				CParallelProcessor<C_BaseAnimating *, CFuncJobItemProcessor<C_BaseAnimating *>, 2 > processor;
				processor.m_ItemProcessor.Init( &SetupBonesOnBaseAnimating, &PreThreadedBoneSetup, &PostThreadedBoneSetup );
				processor.Run( g_PreviousBoneSetups.Base(), nCount, 1, INT_MAX, g_pBoneSetupThreadPool );
			}
			else
			{

#if defined( _PS3 )
				ThreadedBoneSetup_PS3( nCount );
#else
				for ( int i = 0; i < nCount; i++ )
				{
					SetupBonesOnBaseAnimating( g_PreviousBoneSetups[i] );
				}
#endif
			}
			g_bInThreadedBoneSetup = false;

#ifdef _DEBUG
			for ( int i = test.Count() - 1; i > 0; i-- )
			{
				Assert( test[i]->m_pNextForThreadedBoneSetup == NULL );
			}
#endif

#ifdef DEBUG_BONE_SETUP_THREADING
			Msg( "} \n" );
#endif
		}
	}
	g_iPreviousBoneCounter++;
	g_PreviousBoneSetups.RemoveAll();
}


bool C_BaseAnimating::InThreadedBoneSetup()
{
	return g_bInThreadedBoneSetup;
}

#ifdef DEBUG
ConVar cl_limit_anim_fps("cl_limit_anim_fps", "1");
#endif

#define FPS_TO_FRAMETIME_SECS( _n ) (1000.0f / _n) * 0.001f
bool C_BaseAnimating::ShouldSkipAnimationFrame( float currentTime )
{
#ifdef DEBUG
	if ( !cl_limit_anim_fps.GetBool() )
		return false;
#endif

	// only applies to players
	if ( !IsPlayer() )
		return false;

	int nFrameCount = gpGlobals->framecount;
	if ( !m_nLastNonSkippedFrame || abs( nFrameCount - m_nLastNonSkippedFrame ) >= 2 )
		return false;

	if ( gpGlobals->frametime < FPS_TO_FRAMETIME_SECS(300.0f) )
	{
		nFrameCount += (entindex() % 3); // offset lookups
		if ( (nFrameCount % 3) != 0 ) // at 300+ fps, compute every third animation frame to floor animation at 100fps
		{
			return true;
		}
	}
	else if ( gpGlobals->frametime < FPS_TO_FRAMETIME_SECS(200.0f) )
	{
		nFrameCount += (entindex() % 2); // offset lookups
		if ( (nFrameCount % 2) != 0 ) // at 200+ fps, compute every other animation frame to floor animation at 100fps
		{
			return true;
		}
	}
	else if ( gpGlobals->frametime < FPS_TO_FRAMETIME_SECS(150.0f) )
	{
		nFrameCount += (entindex() % 3); // offset lookups
		if ( (nFrameCount % 3) == 0 ) // at 150+ fps, skip every third animation frame to floor animation at 100fps
		{
			return true;
		}
	}

	return false;
}

extern ConVar cl_countbones;

bool C_BaseAnimating::SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime )
{
	VPROF_BUDGET( "C_BaseAnimating::SetupBones", ( !g_bInThreadedBoneSetup ) ? VPROF_BUDGETGROUP_CLIENT_ANIMATION : "Client_Animation_Threaded" );
	SNPROF_ANIM( "C_BaseAnimating::SetupBones" );

	// [pfreese] Added the check for pBoneToWorldOut != NULL in this debug warning
	// code. SetupBones is called in the CSS anytime an attachment wants its
	// parent's transform, hence this warning is hit extremely frequently.
	// I'm not actually sure if this is the right "fix" for this, as the bones are
	// actually accessed as part of the setup process, but since I'm not clear on the
	// purpose of this dev warning, I'm including this comment block.
	if ( pBoneToWorldOut != NULL && !IsBoneAccessAllowed() )
	{
		static float lastWarning = 0.0f;

		// Prevent spammage!!!
		if ( gpGlobals->realtime >= lastWarning + 1.0f )
		{
			DevMsgRT( "*** ERROR: Bone access not allowed (entity %i:%s)\n", index, GetClassname() );
			lastWarning = gpGlobals->realtime;
		}
	}

	//boneMask = BONE_USED_BY_ANYTHING; // HACK HACK - this is a temp fix until we have accessors for bones to find out where problems are.
	
	// some bones are tagged to always setup, they get OR'd in now
	boneMask |= BONE_ALWAYS_SETUP;

	if ( GetSequence() == -1 )
		 return false;

	if ( boneMask == -1 )
	{
		boneMask = m_iPrevBoneMask;
	}

	// We should get rid of this someday when we have solutions for the odd cases where a bone doesn't
	// get setup and its transform is asked for later.
	if ( cl_SetupAllBones.GetInt() )
	{
		boneMask |= BONE_USED_BY_ANYTHING;
	}

	// Set up all bones if recording, too
	if ( IsToolRecording() )
	{
		boneMask |= BONE_USED_BY_ANYTHING;
	}

	// Or lastly if demo polish recording
#ifdef DEMOPOLISH_ENABLED
	if ( IsDemoPolishRecording() && IsPlayer() )
	{
		boneMask |= BONE_USED_BY_ANYTHING;
	}
#endif	// DEMO_POLISH

	if ( g_bInThreadedBoneSetup )
	{
//		boneMask |= ( BONE_USED_BY_ATTACHMENT | BONE_USED_BY_BONE_MERGE | BONE_USED_BY_HITBOX );

		if ( !m_BoneSetupLock.TryLock() )
		{
			// someone else is handling
			// bones are in some intermediate state, wait until the other thread is done.  
			// If they've setup what bones we want, it'll early out down below
			m_BoneSetupLock.Lock();
		}
		// else, we have the lock
	}
	else
	{
		m_BoneSetupLock.Lock();
	}

	// If we're setting up LOD N, we have set up all lower LODs also
	// because lower LODs always use subsets of the bones of higher LODs.
	int nLOD = 0;
	int nMask = BONE_USED_BY_VERTEX_LOD0;
	for( ; nLOD < MAX_NUM_LODS; ++nLOD, nMask <<= 1 )
	{
		if ( boneMask & nMask )
			break;
	}
	for( ; nLOD < MAX_NUM_LODS; ++nLOD, nMask <<= 1 )
	{
		boneMask |= nMask;
	}

#ifdef DEBUG_BONE_SETUP_THREADING
	if ( cl_warn_thread_contested_bone_setup.GetBool() )
	{
		if ( !m_BoneSetupLock.TryLock() )
		{
			Msg( "Contested bone setup in frame %d!\n", gpGlobals->framecount );
		}
		else
		{
			m_BoneSetupLock.Unlock();
		}
	}
#endif	

	// A bit of a hack, but this way when in prediction we use the "correct" gpGlobals->curtime -- rather than the
	// one that the player artificially advances
	if ( GetPredictable() && 
		 prediction->InPrediction() )
	{
		currentTime = prediction->GetSavedTime();
	}

#if defined(_DEBUG_SPUvPPU_ANIMATION)
	m_BoneAccessor.SetReadableBones( 0 );
	m_BoneAccessor.SetWritableBones( 0 );
	m_flLastBoneSetupTime = 0.0f;
	m_iPrevBoneMask = m_iAccumulatedBoneMask;
	m_iAccumulatedBoneMask = 0;
#else

	if( ( m_iMostRecentModelBoneCounter != g_iModelBoneCounter ) && s_bEnableNewBoneSetupRequest )
	{
		// Clear out which bones we've touched this frame if this is 
		// the first time we've seen this object this frame.
		// BUGBUG: Time can go backward due to prediction, catch that here until a better solution is found
		if ( LastBoneChangedTime() >= m_flLastBoneSetupTime || currentTime < m_flLastBoneSetupTime )
		{
//			SNPROF_ANIM("Anim_NewSetupBones");
#ifdef DEBUG_BONESETUP_THREADVSNONTHREAD
			Msg("SetupBones 1st time: %x, mask: %d, mrmbc: %d, time:%f\n", this, boneMask, m_iMostRecentModelBoneCounter, currentTime );
#endif

			m_BoneAccessor.SetReadableBones( 0 );
			m_BoneAccessor.SetWritableBones( 0 );
			m_flLastBoneSetupTime = currentTime;

#if defined( DBGFLAG_ASSERT )
			m_vBoneSetupCachedOrigin = GetRenderOrigin();
			m_qBoneSetupCachedAngles = GetRenderAngles();
#endif
		}
		m_iPrevBoneMask = m_iAccumulatedBoneMask;
		m_iAccumulatedBoneMask = 0;

#ifdef STUDIO_ENABLE_PERF_COUNTERS
		CStudioHdr *hdr = GetModelPtr();
		if (hdr)
		{
			hdr->ClearPerfCounters();
		}
#endif
	}

#endif

	MarkForThreadedBoneSetup();

	// Keep track of everthing asked for over the entire frame
	// But not those things asked for during bone setup
	{
		m_iAccumulatedBoneMask |= boneMask;
	}

	// Make sure that we know that we've already calculated some bone stuff this time around.
#if !defined(_DEBUG_SPUvPPU_ANIMATION)
	m_iMostRecentModelBoneCounter = g_iModelBoneCounter;
#endif

	// Have we cached off all bones meeting the flag set?
	if( ( m_BoneAccessor.GetReadableBones() & boneMask ) != boneMask )
	{
		CStudioHdr *hdr = GetModelPtr();
		if ( !hdr || !hdr->SequencesAvailable() )
		{
			m_BoneSetupLock.Unlock();
			return false;
		}

#if defined( DBGFLAG_ASSERT )
		bool bHadDirtyAbsTransform = IsEFlagSet( EFL_DIRTY_ABSTRANSFORM );
#endif

		// Setup our transform based on render angles and origin.
		ALIGN16 matrix3x4_t parentTransform ALIGN16_POST;
		AngleMatrix( GetRenderAngles(), GetRenderOrigin(), parentTransform );
		AssertMsg( !bHadDirtyAbsTransform || !IsEFlagSet( EFL_DIRTY_ABSTRANSFORM ), "Using an old origin/angles and unable to recompute before caching off the bones" );

		// Load the boneMask with the total of what was asked for last frame.
		// WHY?? 
		boneMask |= m_iPrevBoneMask;

		// Allow access to the bones we're setting up so we don't get asserts in here.
		int oldReadableBones = m_BoneAccessor.GetReadableBones();
		m_BoneAccessor.SetWritableBones( m_BoneAccessor.GetReadableBones() | boneMask );
		m_BoneAccessor.SetReadableBones( m_BoneAccessor.GetWritableBones() );

		// label the entities if we're trying to figure out who is who
		if ( r_sequence_debug.GetInt() == -1)
		{
			Vector theMins, theMaxs;
			GetRenderBounds( theMins, theMaxs );
			debugoverlay->AddTextOverlay( GetAbsOrigin() + (theMins + theMaxs) * 0.5f, 0, 0.0f, "%d:%s", entindex(), hdr->name() );
		}

		if (hdr->flags() & STUDIOHDR_FLAGS_STATIC_PROP)
		{
			MatrixCopy(	parentTransform, GetBoneForWrite( 0 ) );
		}
		else
		{

#ifdef DEBUG_BONE_SETUP_THREADING
			if ( !g_bInThreadedBoneSetup )
			{
				Msg("!%x\n", this );
			}
#endif
			if ( !g_bInThreadedBoneSetup )
			{
				TrackBoneSetupEnt( this );
			}
			
			// This is necessary because it's possible that CalculateIKLocks will trigger our move children
			// to call GetAbsOrigin(), and they'll use our OLD bone transforms to get their attachments
			// since we're right in the middle of setting up our new transforms. 
			//
			// Setting this flag forces move children to keep their abs transform invalidated.
			AddEFlags( EFL_SETTING_UP_BONES );

// NOTE: For model scaling, we need to opt out of IK because it will mark the bones as already being calculated
// [msmith]: What game is it that want's to do model scaling and needs to opt out of IK?  It seems as if opting out of IK should be the exception and not the rule here.
// I suggest we change the #ifdef such that only the games that need to kill IK get used here... rather than ORing in all other games that use this engine in the future.
#if defined( PORTAL2 ) || defined( INFESTED ) || defined( CSTRIKE15 )
			// only allocate an ik block if the npc can use it
			if ( !m_pIk && hdr->numikchains() > 0 && !(m_EntClientFlags & ENTCLIENTFLAG_DONTUSEIK) )
				m_pIk = new CIKContext;
#endif // PORTAL2

#if defined( _PS3 )
			BoneVector						pos[MAXSTUDIOBONES] ALIGN128;
			BoneQuaternionAligned			q[MAXSTUDIOBONES]   ALIGN128;
#else
			BoneVector						pos[MAXSTUDIOBONES];
			BoneQuaternionAligned			q[MAXSTUDIOBONES];
#endif

			if ( m_pIk )
			{
				if (Teleported() || IsEffectActive(EF_NOINTERP))
				{
					m_pIk->ClearTargets();
				}

				m_pIk->Init( hdr, GetRenderAngles(), GetRenderOrigin(), currentTime, gpGlobals->framecount, boneMask );
			}

			// Let pose debugger know that we are blending
			g_pPoseDebugger->StartBlending( this, hdr );

			bool bSkipThisFrame = ShouldSkipAnimationFrame( currentTime );

			CBoneBitList boneComputed;

			if ( !bSkipThisFrame )
			{
				int nTempMask = boneMask;

				if ( m_nCustomBlendingRuleMask != -1 )
				{
					nTempMask &= m_nCustomBlendingRuleMask;
				}

				nTempMask |= BONE_ALWAYS_SETUP; // make sure we always set up these bones

				if ( cl_countbones.GetBool() )
				{
					for ( int i = 0; i < hdr->numbones(); ++i )
					{
						if ( hdr->boneFlags(i) & nTempMask )
							g_nNumBonesSetupBlendingRulesOnlyTemp++;
					}
				}

				StandardBlendingRules( hdr, pos, q, currentTime, nTempMask );
				
				if ( IsPlayer() && nTempMask != boneMask )
				{
					// restore the saved transforms of the leafy bones that got re-initialized during StandardBlendingRules.
					// This will hold bones in their last computed pose and hide the lod pop on the way out (they may still pop on the way in)
					for ( int i = 0; i < hdr->numbones(); ++i )
					{
						if ( (hdr->boneFlags(i) & (BONE_USED_BY_ATTACHMENT | BONE_USED_BY_HITBOX | BONE_ALWAYS_SETUP) ) == 0 )
						{
							pos[i] = m_pos_cached[i];
							q[i] = m_q_cached[i];
						}
					}

				}

				if ( cl_countbones.GetBool() )
				{
					Vector from, to;
					for ( int i = 0; i < hdr->numbones(); ++i )
					{
						if ( !(hdr->boneFlags( i ) & nTempMask) )
							continue;

						int const iParentIndex = hdr->boneParent( i );
						if ( iParentIndex < 0 )
							continue;

						MatrixPosition( m_BoneAccessor[ i            ], from );
						MatrixPosition( m_BoneAccessor[ iParentIndex ], to   );

						if ( IsPlayer() )
						{
							debugoverlay->AddLineOverlay( from, to, 0, 255, 255, true, 0.0f );
						}
						else
						{
							debugoverlay->AddLineOverlay( from + Vector(2,0,0), to + Vector(2,0,0), 200, 0, 255, true, 0.0f );
						}
					}
				}

				m_nLastNonSkippedFrame = gpGlobals->framecount;
			}
			else
			{
				memcpy( pos, m_pos_cached, sizeof( BoneVector ) * hdr->numbones() );
				memcpy( q, m_q_cached, sizeof( BoneQuaternionAligned ) * hdr->numbones() );

				boneComputed.ClearAll(); // because we need to re-BuildTransformations on all our bones with a new root xform
				boneMask = m_BoneAccessor.GetWritableBones();
			}

#ifdef DEMOPOLISH_ENABLED
			bool const bShouldPolish = IsDemoPolishEnabled() && IsPlayer();
			if ( bShouldPolish && engine->IsPlayingDemo() )
			{
				DemoPolish_GetController().MakeLocalAdjustments( entindex(), hdr, boneMask, pos, q, boneComputed );
			}
#endif

			// don't calculate IK on ragdolls
			if ( m_pIk && !IsRagdoll() )
			{
				UpdateIKLocks( currentTime );

				m_pIk->UpdateTargets( pos, q, m_BoneAccessor.GetBoneArrayForWrite(), boneComputed );

				CalculateIKLocks( currentTime );
				m_pIk->SolveDependencies( pos, q, m_BoneAccessor.GetBoneArrayForWrite(), boneComputed );

				if ( IsPlayer() && ( (BONE_USED_BY_VERTEX_LOD0 & boneMask) == BONE_USED_BY_VERTEX_LOD0 ) )
				{
					// only do extra bone processing when setting up bones that influence renderable vertices, and not for attachment position requests
					DoExtraBoneProcessing( hdr, pos, q, m_BoneAccessor.GetBoneArrayForWrite(), boneComputed, m_pIk );
				}

			}

#ifdef DEMOPOLISH_ENABLED
			if ( m_bBonePolishSetup && bShouldPolish && engine->IsRecordingDemo() )
			{
				DemoPolish_GetRecorder().RecordAnimData( entindex(), hdr, GetCycle(), parentTransform, pos, q, boneMask, m_BoneAccessor );
			}
#endif

			BuildTransformations( hdr, pos, q, parentTransform, boneMask, boneComputed );

#if defined(_DEBUG_SPUvPPU_ANIMATION)

// 			m_PPUboneMaskUsed = bonesMaskNeedRecalc;
// 			memcpy( ppuP, pos, m_pStudioHdr->numbones() * sizeof(BoneVector));
// 			memcpy( ppuQ, q, m_pStudioHdr->numbones() * sizeof(BoneQuaternion));

#endif


#ifdef DEMOPOLISH_ENABLED
			// Override bones?
			if ( bShouldPolish && engine->IsPlayingDemo() )
			{
				DemoPolish_GetController().MakeGlobalAdjustments( entindex(), hdr, boneMask, m_BoneAccessor );
			}
#endif

			if ( cl_countbones.GetBool() )
			{
				for ( int i = 0; i < hdr->numbones(); ++i )
				{
					if ( hdr->boneFlags(i) & boneMask )
						g_nNumBonesSetupAllTemp++;
				}
			}

			// Draw skeleton?
			if ( enable_skeleton_draw.GetBool() )
			{
				DrawSkeleton( hdr, boneMask );
			}

			RemoveEFlags( EFL_SETTING_UP_BONES );
			ControlMouth( hdr );
			if ( !bSkipThisFrame )
			{
				memcpy( m_pos_cached, pos, sizeof( BoneVector ) * hdr->numbones() );
				memcpy( m_q_cached, q, sizeof( BoneQuaternionAligned ) * hdr->numbones() );
			}

		}
		
		if( !( oldReadableBones & BONE_USED_BY_ATTACHMENT ) && ( boneMask & BONE_USED_BY_ATTACHMENT ) )
		{
			SetupBones_AttachmentHelper( hdr );
		}
	}
	
	// Do they want to get at the bone transforms? If it's just making sure an aiment has 
	// its bones setup, it doesn't need the transforms yet.
	if ( pBoneToWorldOut )
	{
		AssertMsgOnce( !IsEFlagSet( EFL_DIRTY_ABSTRANSFORM ), "Cached bone data has old abs origin/angles" );
		//AssertMsgOnce( (m_vBoneSetupCachedOrigin == GetRenderOrigin()) && (m_qBoneSetupCachedAngles == GetRenderAngles()), "Renderable moved since cached" );
		if ( nMaxBones >= m_CachedBoneData.Count() )
		{
			Plat_FastMemcpy( pBoneToWorldOut, m_CachedBoneData.Base(), sizeof( matrix3x4_t ) * m_CachedBoneData.Count() );
		}
		else
		{
			Warning( "SetupBones: invalid bone array size (%d - needs %d)\n", nMaxBones, m_CachedBoneData.Count() );
			m_BoneSetupLock.Unlock();
			return false;
		}
	}

	m_BoneSetupLock.Unlock();
	return true;
}

//-----------------------------------------------------------------------------
//
// START OF PS3 BONE JOBS
//
//-----------------------------------------------------------------------------

#if defined( _PS3 )

//-----------------------------------------------------------------------------
// Purpose: to run setupbonesonbaseanimating efficiently on PS3
//
// Take one of three paths depending on cl_PS3_SPU_bones convar:
//
// 0: path as in ThreadedBoneSetup with cl_threaded_bone_setup = 2.
// 1: run on SPU. Right now this means running AccumulatePose on SPU with pre and post AccumulatePose running on PPU.
//    In order to use as many SPU's as possible we will run all the pre steps first, then kick off as many SPU jobs as we can so that AccumulatePose can run in parallel
//    The prologue can only run when the SPU job has finished so we wait until the appropriate job is done before finishing up on PPU
// 2: run as "1" but run job on PPU to ensure rearranging the code this way works!
//
// Note we assume that only one thread will be responsible for kicking off this path on PS3
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// base class empty implementation
//-----------------------------------------------------------------------------
void C_BaseAnimating::AccumulateLayers_AddPoseCalls( IBoneSetup_PS3 &boneSetup, BoneVector pos[], BoneQuaternion q[], float currentTime )
{
	// Nothing here
}

//-----------------------------------------------------------------------------
// save some state in case we need to start the job over again
// this should only happen if we get a data request during the SPU job which means the anim data isn't loaded yet
// since this is rare, we'll reset and run the job from scratch on PPU
//-----------------------------------------------------------------------------
void C_BaseAnimating::SaveSetupBones_PS3( void )
{
	// save so we can reset and run setupbones all over again... don't really want to save the whole class each time

	m_iMostRecentModelBoneCounter_SAVE	= m_iMostRecentModelBoneCounter;
	m_iMostRecentBoneSetupRequest_SAVE	= m_iMostRecentBoneSetupRequest;

	m_iPrevBoneMask_SAVE				= m_iPrevBoneMask;
	m_iAccumulatedBoneMask_SAVE			= m_iAccumulatedBoneMask;

	m_iOldWriteableBones_SAVE			= m_BoneAccessor.GetWritableBones();
	m_iOldReadableBones_SAVE			= m_BoneAccessor.GetReadableBones();

	m_flLastBoneSetupTime_SAVE			= m_flLastBoneSetupTime;
}

//-----------------------------------------------------------------------------
// restore for above
//-----------------------------------------------------------------------------
void C_BaseAnimating::RestoreSetupBones_PS3( void )
{
	// reset so we can run setupbones all over again...

	// if it got to a job, these will have been initialised just fine.

	m_iMostRecentModelBoneCounter	= m_iMostRecentModelBoneCounter_SAVE;
	m_iMostRecentBoneSetupRequest	= m_iMostRecentBoneSetupRequest_SAVE;

	m_iPrevBoneMask					= m_iPrevBoneMask_SAVE;
	m_iAccumulatedBoneMask			= m_iAccumulatedBoneMask_SAVE;

	m_BoneAccessor.SetWritableBones( m_iOldWriteableBones_SAVE );
	m_BoneAccessor.SetReadableBones( m_iOldReadableBones_SAVE );

	m_flLastBoneSetupTime			= m_flLastBoneSetupTime_SAVE;
}

//-----------------------------------------------------------------------------
// start PS3 anim jobs, order is implied since there is an external loop that 
// rolls over the generations in order, guaranteeing all parent jobs started as 
// much ahead of their children as possible giving the SPU job the best chance 
// of finishing it before results are needed. 
// This call effectively only runs the baseanimating setupbones() for generation nGen
//-----------------------------------------------------------------------------
int C_BaseAnimating::SetupBonesOnBaseAnimating_PS3( C_BaseAnimating *&pBaseAnimating, int nGen )
{
//	SNPROF_ANIM( "C_BaseAnimating::SetupBonesOnBaseAnimating_PS3" );

	C_BaseAnimating *pCurrent = pBaseAnimating;
	C_BaseAnimating *pNext;

	int nDependantID = -1;

	while ( pCurrent )
	{
		pNext = pCurrent->m_pNextForThreadedBoneSetup;

		if( nGen == pCurrent->m_iPS3BoneJob_Gen )
		{
			pCurrent->m_iPS3BoneJob_DependantID = nDependantID;

			// save off data we may need to reset if job gets to run and fails
			pCurrent->SaveSetupBones_PS3();
			
			// init new job
			pCurrent->PS3BoneJob_Start( gpGlobals->curtime );

			// TESTING - immediate reset and do over
			//pCurrent->ResetSetupBones();
			//pCurrent->SetupBones( NULL, -1, -1, gpGlobals->curtime );

			return 1;
		}

		nDependantID = pCurrent->m_iPS3BoneJob_ID;
		pCurrent = pNext;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// reset critical fields and count the max number of potential bone jobs
// also sets the generation field appropriately for sorting later
//-----------------------------------------------------------------------------
int C_BaseAnimating::InitAllPS3BoneJobs( int nCount )
{
//	SNPROF_ANIM( "C_BaseAnimating::InitAllPS3BoneJobs" );

	int nTotal = 0;


	for ( int i = 0; i < nCount; i++ )
	{
		C_BaseAnimating *pCurrent = g_PreviousBoneSetups[ i ];
		C_BaseAnimating *pNext;

		int nGen = 0;

		while ( pCurrent )
		{
			// reset
			pCurrent->m_iPS3BoneJob_ID			= -1;
			pCurrent->m_iPS3BoneJob_DependantID = -1;
			pCurrent->m_iPS3BoneJob_Gen			= nGen;
			pCurrent->m_iPS3BoneJob_Port		= -1;

			pCurrent = pCurrent->m_pNextForThreadedBoneSetup;

			nGen++;
			nTotal++;
		}
	}

	return nTotal;
}

//-----------------------------------------------------------------------------
// Add all AccumulatePose calls from MaintainSequenceTransitions to bonejob
//-----------------------------------------------------------------------------
void C_BaseAnimating::MaintainSequenceTransitions_AddPoseCalls( IBoneSetup_PS3 &boneSetup, float flCycle, BoneVector pos[], BoneQuaternion q[] )
{
	if ( !boneSetup.GetStudioHdr() )
		return;

	if ( prediction->InPrediction() )
	{
		m_nPrevNewSequenceParity = m_nNewSequenceParity;
		return;
	}

	m_SequenceTransitioner.CheckForSequenceChange( 
		boneSetup.GetStudioHdr(),
		GetSequence(),
		m_nNewSequenceParity != m_nPrevNewSequenceParity,
		!IsEffectActive(EF_NOINTERP)
		);

	m_nPrevNewSequenceParity = m_nNewSequenceParity;

	// Update the transition sequence list.
	m_SequenceTransitioner.UpdateCurrent( 
		boneSetup.GetStudioHdr(),
		GetSequence(),
		flCycle,
		GetPlaybackRate(),
		gpGlobals->curtime
		);


	// process previous sequences
	for (int i = m_SequenceTransitioner.m_animationQueue.Count() - 2; i >= 0; i--)
	{
		CAnimationLayer *blend = &m_SequenceTransitioner.m_animationQueue[i];

		float dt = (gpGlobals->curtime - blend->m_flLayerAnimtime);
		flCycle = blend->GetCycle() + dt * blend->GetPlaybackRate() * GetSequenceCycleRate( boneSetup.GetStudioHdr(), blend->GetSequence() );
		flCycle = ClampCycle( flCycle, IsSequenceLooping( boneSetup.GetStudioHdr(), blend->GetSequence() ) );

// original call		boneSetup.AccumulatePose( pos, q, blend->GetSequence(), flCycle, blend->GetWeight(), gpGlobals->curtime, m_pIk );
		boneSetup.AccumulatePose_AddToBoneJob( boneSetup.GetBoneJobSPU(), blend->GetSequence(), flCycle, blend->GetWeight(), m_pIk, 0 );
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void C_BaseAnimating::PS3BoneJob_PreInit( void )
{
	//m_iPS3BoneJob_Port				= -1;
	m_iPS3BoneJob_ID				= g_pBoneJobs->AddBoneJob(); 

	PS3BoneJobData	*pBoneJobData   = g_pBoneJobs->GetJobData( m_iPS3BoneJob_ID );
	bonejob_PPU		*pBonejobPPU	= &pBoneJobData->bonejobPPU;

	pBonejobPPU->pBaseAnimating		= this;
}

//-----------------------------------------------------------------------------
// Initialise a PS3 bone job
// ensure any dependant jobs are finished first
//-----------------------------------------------------------------------------
void C_BaseAnimating::PS3BoneJob_Start( float currentTime )
{
//	SNPROF_ANIM( "C_BaseAnimating::PS3BoneJob_Start" );

#ifdef DEBUG_BONESETUP_THREADVSNONTHREAD
	Msg("BONEJOB: %x\n", this );
#endif

	// too many bones for SPU?
	if( m_pStudioHdr->numbones() > MAXSTUDIOBONES_PS3 ) 
	{
		// Not able to run job on SPU, rerun on PPU
		PS3BoneJob_RestartPPU();
		return;
	}

	// if 'this' is already running in a job (<-- see how often this happens?)
	//    SPU - sync on port, and exec pass 2 HERE guaranteeing that we are finished before we start the next job. flag as finished preventing subsequent pass 2 loop execution.
	//    PPU - exec pass 2 on the previous job, and flag it as finished preventing from executing in pass 2 loop, then continue
	if( cl_PS3_SPU_bones_safesync.GetInt() )
	{
		if( m_iPS3BoneJob_Port >= 0 )
		{
			Assert( m_iPS3BoneJob_ID >= 0 );

			// in a bone job already on 'this'

			// on SPU
			// 1. sync on port to wait for finish of previous job
			// 2. exec pass 2 & 3 of previous job here
			// 3. continue with current job

			// on PPU
			// 1. exec pass 2 & 3 of previous job here
			// 2. continue with current job

			PS3BoneJobData *pBoneJob = g_pBoneJobs->GetJobData( m_iPS3BoneJob_ID );
			C_BaseAnimating *pDependantBaseAnim = (C_BaseAnimating *)pBoneJob->bonejobPPU.pBaseAnimating;
			pDependantBaseAnim->PS3BoneJob_WaitForFinish();
		}

		// we need a way on handling dependant jobs when running bonejobs on SPU
		// here seems a good place to sync on those jobs
		// in order to maximise parallelism, we should have not started child jobs immediately after their parents
		// otherwise we'll get immediately here and have to wait
		// we should push some non-dependant jobs first, giving the PPU time to get to a point where the first job might be done
		// i.e.
		// * push all parent jobs
		// * push 1st generation children
		// * push 2nd generation children
		//
		// The current order of jobs (in threadedbonesetup and then setupbonesonbaseanimating) has dependant following their parents, so this will need to change.
		// note: this PS3 path is now sorted by generation

		// if waiting on a parent/dependant job to finish
		//    SPU - sync on port of parent job, exec pass 2 & 3 of parent job HERE guaranteeing a safe continue
		//			OR add a SYNC command so can carry on with PPU
		//    PPU - exec pass 2 & 3 of parent job and continue
		
		if( m_iPS3BoneJob_DependantID != -1 )
		{
			PS3BoneJobData *pBoneJob = g_pBoneJobs->GetJobData( m_iPS3BoneJob_DependantID );
			C_BaseAnimating *pDependantBaseAnim = (C_BaseAnimating *)pBoneJob->bonejobPPU.pBaseAnimating;

			pDependantBaseAnim->PS3BoneJob_WaitForFinish();
		}

		// reset
		PS3BoneJob_PreInit();
	}

	// PASS 1
	SetupBones_Pass1( currentTime );
}

//-----------------------------------------------------------------------------
// Run a PS3 bone job
// 
// collate data for SPU packet and kick SPU job
//-----------------------------------------------------------------------------
void C_BaseAnimating::PS3BoneJob_Run( CStudioHdr *hdr, float currentTime, float fCycle, int nMaxBones, int boneMask, int bonesMaskNeedRecalc, int oldReadableBones, matrix3x4_t &parentTransform, float* poseparam )
{
//	SNPROF_ANIM( "C_BaseAnimating::PS3BoneJob_Run" );

	// if job already running on this, need to sync to it at some point before starting another job on this
	int thisJobID = m_iPS3BoneJob_ID;


	if( !cl_PS3_SPU_bones_safesync.GetInt() )
	{
		// reset here
		PS3BoneJob_PreInit();
	}

	// curr job data, write p, q and other stuff we need in pass 2 here
	PS3BoneJobData  *pBoneJobData = g_pBoneJobs->GetJobData( m_iPS3BoneJob_ID );

	bonejob_PPU		*pBonejobPPU	= &pBoneJobData->bonejobPPU;
	bonejob_SPU		*pBonejobSPU	= &pBoneJobData->bonejobSPU;

	// init bonesetup
	IBoneSetup_PS3 boneSetup( hdr, boneMask, poseparam, pBonejobSPU );
	boneSetup.InitPose_PS3( pBoneJobData->pos, pBoneJobData->q );

	pBoneJobData->addDep_numIKRules		= 0;

	// now push job to SPU (or just execute AccumulatePose on PPU), and finish here for pass 1

	// take a copy of source params to be used in pass 2

	// SPU source data
	memcpy( pBonejobSPU->poseparam, poseparam, MAXSTUDIOPOSEPARAM );
	pBonejobSPU->currentTime			= currentTime;
	pBonejobSPU->pEA_hdr				= (void *)hdr;
	pBonejobSPU->numBones				= hdr->numbones();
	pBonejobSPU->maxBones				= pBonejobSPU->numBones;
	pBonejobSPU->pEA_IKContext			= m_pIk;
	pBonejobSPU->boneMask				= boneMask;
	pBonejobSPU->pEA_pos				= pBoneJobData->pos;
	pBonejobSPU->pEA_q					= pBoneJobData->q;
	pBonejobSPU->pEA_addDep_IKRules		= pBoneJobData->addDep_IKRules;
	pBonejobSPU->pEA_addDep_numIKRules  = &pBoneJobData->addDep_numIKRules;
	for (int i = 0; i < hdr->numbones(); i++)
	{
		pBonejobSPU->boneFlags[i]  = hdr->pBone( i )->flags;
		pBonejobSPU->boneParent[i] = hdr->pBone( i )->parent;
	}
	pBonejobSPU->pEA_studiohdr_ikchains		= (void *)hdr->pIKChain( 0 );
	pBonejobSPU->numikchains				= hdr->numikchains();
	pBonejobSPU->pEA_studiohdr_bones		= (void *)hdr->pBone( 0 );
	pBonejobSPU->pEA_studiohdr_bones_pos	= (void *)(&hdr->pBone( 0 )->pos);
	pBonejobSPU->studiobone_posoffset		= (int)(&hdr->pBone( 0 )->pos) - (int)(hdr->pBone( 0 ));
	mstudiolinearbone_t *pLinearBones		= hdr->pLinearBones();
	pBonejobSPU->pEA_studiohdr_linearBones	= (void *)pLinearBones;
	if( pLinearBones )
	{
		pBonejobSPU->pEA_linearbones_pos		= (void *)(((byte *)pLinearBones) + pLinearBones->posindex);
		pBonejobSPU->pEA_linearbones_posscale	= (void *)(((byte *)pLinearBones) + pLinearBones->posscaleindex);
		pBonejobSPU->pEA_linearbones_rot		= (void *)(((byte *)pLinearBones) + pLinearBones->rotindex);
		pBonejobSPU->pEA_linearbones_rotscale	= (void *)(((byte *)pLinearBones) + pLinearBones->rotscaleindex);
		pBonejobSPU->pEA_linearbones_flags		= (void *)(((byte *)pLinearBones) + pLinearBones->flagsindex);
		pBonejobSPU->pEA_linearbones_quat		= (void *)(((byte *)pLinearBones) + pLinearBones->quatindex);
		pBonejobSPU->pEA_linearbones_qalignment = (void *)(((byte *)pLinearBones) + pLinearBones->qalignmentindex);
	}
	virtualmodel_t *pVModel = hdr->GetVirtualModel();
	if( pVModel )
	{
		pBonejobSPU->pEA_studiohdr_vmodel		= (void *)pVModel;
	}
	else
	{
		pBonejobSPU->pEA_studiohdr_vmodel		= NULL;
	}

	// debug SPU?
	pBonejobSPU->debugJob				= cl_PS3_SPU_bones_debug.GetInt() == 2;

	// PPU data
	pBonejobPPU->cycle					= fCycle;
	pBonejobPPU->maxBones				= nMaxBones;
	pBonejobPPU->boneMask				= boneMask;
	pBonejobPPU->bonesMaskNeedsRecalc	= bonesMaskNeedRecalc;
	pBonejobPPU->oldReadableBones		= oldReadableBones;
	pBonejobPPU->pBaseAnimating			= this;
	pBonejobPPU->pStudioHdr				= hdr;
	pBonejobPPU->pBoneToWorldOut		= NULL;


	BoneVector* pos						= pBoneJobData->pos;
	BoneQuaternion* q					= pBoneJobData->q;



	//---------------------------------------------------------------------------------------------------------------------------------
	// Data prep for SPU, dummy run of AccumulatePose calls, accumulating data for SPU
	//---------------------------------------------------------------------------------------------------------------------------------

	//---------------------------------------------------------------------------------------------------------------------------------
	// build a list of AccumulatePose() calls that will need to be made in the job.
	// there is always at least one.
	// the regular calls that are made in StandardBlendingRules 
	//	1. boneSetup.AccumulatePose()
	//	2. MaintainSequenceTransitions()  <-- may contain AccumPose calls
	//	3. AccumulateLayers() <-- may contain AccumPose calls
	//  4. CalcAutoplaySequences() <-- may contain AccumPose calls, and take care since they're bracketed by some IK autoplaylocks
	//---------------------------------------------------------------------------------------------------------------------------------

	boneSetup.ResetErrorFlags();

	// execute dummy versions of 2 and 3 and build a list of call arguments to AccumulatePose()
	// the only arguments we need to cache are sequence, cycle and weight - the others: pos, q, time, m_pIK stay the same for all calls

	// reset
	pBonejobSPU->numTotalPoses		 = 0;
	pBonejobSPU->numPoses_PreAutoSeq = 0;
	pBonejobSPU->numPoses_AutoSeq	 = 0;


	// add first call boneSetup.AccumulatePose()
	boneSetup.AccumulatePose_AddToBoneJob( boneSetup.GetBoneJobSPU(), GetSequence(), fCycle, 1.0, m_pIk, 0 );
	if( boneSetup.ErrorFlags() ) goto skip;

	// add calls from MaintainSequenceTransitions
	MaintainSequenceTransitions_AddPoseCalls( boneSetup, fCycle, pos, q );
	if( boneSetup.ErrorFlags() ) goto skip;

	// add calls from AccumulateLayers
	AccumulateLayers_AddPoseCalls( boneSetup, pos, q, currentTime );
	if( boneSetup.ErrorFlags() ) goto skip;

	// get number of AccumPose calls to this point since we do some other work on pos and q before potentially adding
	// more AccumPose calls via CalcAutoplaySequences
	pBonejobSPU->numPoses_PreAutoSeq = pBonejobSPU->numTotalPoses;
	
	// execute dummy version of 4 and build a list of call arguments to AccumulatePose()
	// we need a new IKcontext so store the data required to build it

	// init IK context for autoplay sequences - no need, done internally 
	pBonejobSPU->autoikOrigin		= GetRenderOrigin();
	pBonejobSPU->autoikAngles		= GetRenderAngles();
	pBonejobSPU->autoikFramecount	= gpGlobals->framecount;
	pBonejobSPU->numikAutoplayLocks = hdr->GetNumIKAutoplayLocks();
	boneSetup.CalcAutoplaySequences_AddPoseCalls( currentTime );
	if( boneSetup.ErrorFlags() ) goto skip;

	// get number of AccumPose calls during CalcAutoplaySequences
	pBonejobSPU->numPoses_AutoSeq = pBonejobSPU->numTotalPoses - pBonejobSPU->numPoses_PreAutoSeq;


	if( !cl_PS3_SPU_bones_safesync.GetInt() )
	{
		if( m_iPS3BoneJob_Port >= 0 )
		{
			Assert( thisJobID >= 0 );
			PS3BoneJobData *pBoneJob = g_pBoneJobs->GetJobData( thisJobID );
			C_BaseAnimating *pDependantBaseAnim = (C_BaseAnimating *)pBoneJob->bonejobPPU.pBaseAnimating;
			pDependantBaseAnim->PS3BoneJob_WaitForFinish();
		}

		if( m_iPS3BoneJob_DependantID != -1 )
		{
			PS3BoneJobData *pBoneJob = g_pBoneJobs->GetJobData( m_iPS3BoneJob_DependantID );
			C_BaseAnimating *pDependantBaseAnim = (C_BaseAnimating *)pBoneJob->bonejobPPU.pBaseAnimating;

			pDependantBaseAnim->PS3BoneJob_WaitForFinish();
		}

		if ( m_pIk )
		{
			if (Teleported() || IsEffectActive(EF_NOINTERP))
			{
				m_pIk->ClearTargets();
			}

			m_pIk->Init( hdr, GetRenderAngles(), GetRenderOrigin(), currentTime, gpGlobals->framecount, bonesMaskNeedRecalc );
		}

		ALIGN16 matrix3x4_t parentTransform2 ALIGN16_POST;

		AngleMatrix( GetRenderAngles(), GetRenderOrigin(), parentTransform2 );
		MatrixCopy( parentTransform2, pBonejobPPU->parentTransform );
	}
	else
	{
		MatrixCopy( parentTransform, pBonejobPPU->parentTransform );
	}
	pBonejobSPU->autoikOrigin		= GetRenderOrigin();
	pBonejobSPU->autoikAngles		= GetRenderAngles();


	// run SPU jobs
	// everything now ready to push to SPU job
	if( cl_PS3_SPU_bones.GetInt() == 1 )
	{
		// SPU path
		m_iPS3BoneJob_Port = boneSetup.RunAccumulatePoseJobs_SPU( pBonejobSPU, &pBoneJobData->jobDescriptor );
	}
	else
	{
		// PPU path
		m_iPS3BoneJob_Port = boneSetup.RunAccumulatePoseJobs_PPU( pBonejobSPU );
	}	
	return;


skip:

	// Not able to run job on SPU
	
	
	// finish any running/dependant jobs


	// rerun on PPU
	PS3BoneJob_RestartPPU();

	return;
}


//-----------------------------------------------------------------------------
// Finishes up a PS3 bone job (may have already been finished via WaitForFinish, in which 
// case this will tidy up the m_pNextForThreadedBoneSetup ptr)
//
//-----------------------------------------------------------------------------
void C_BaseAnimating::PS3BoneJob_End( void )
{
	// proper finish 
	m_pNextForThreadedBoneSetup = NULL;

	if( ( m_iPS3BoneJob_ID != -1 ) && ( m_iPS3BoneJob_Port != -1 ) ) // job started and running
	{
		if( ( cl_PS3_SPU_bones.GetInt() == 1 ) &&
			( cl_PS3_SPU_bones_debug.GetInt() == 0 ) && 
			( m_iPS3BoneJob_Port < VJobsRoot::MAXPORTS_ANIM ) )
		{
			SNPROF_ANIM( "sync(0) end" );
			PS3BoneJobData  *pBoneJobData = g_pBoneJobs->GetJobData( m_iPS3BoneJob_ID );

			// sync on port
			//Msg("sync job End %d\n", m_iPS3BoneJob_Port);
			CELL_VERIFY( g_pBoneJobs->m_pRoot->m_queuePortAnim[ m_iPS3BoneJob_Port ].sync( 0 ) );
		}

		// PASS 2
		StandardBlendingRules_Pass2();
		SetupBones_Pass2();

		// ensure lock released
		m_BoneSetupLock.Unlock();
	}

	// mark as not started, not running
	m_iPS3BoneJob_ID	= -1;
	m_iPS3BoneJob_Port	= -1;
}


//-----------------------------------------------------------------------------
// when another bonejob is trying to start, but is dependant on the results of this one
// we must wait for this job to finish before continuing.
//
// this will only happen during pass1 (i.e. when bone jobs are started)
// => we must:
// 1. wait for pass1 to finish on SPU
// 2. Run pass2 on PPU only
// 3. Run pass3 (PPU only)
// 4. continue
//
// Only makes sense when running on SPU
//-----------------------------------------------------------------------------
void C_BaseAnimating::PS3BoneJob_WaitForFinish( void )
{
	// SPU path only
	// sync on dependant job port2->sync(tag)

	if( ( m_iPS3BoneJob_ID != -1 ) && ( m_iPS3BoneJob_Port != -1 ) ) // job started and running
	{
		if( ( cl_PS3_SPU_bones.GetInt() == 1 ) &&
			( cl_PS3_SPU_bones_debug.GetInt() == 0 ) && 
			( m_iPS3BoneJob_Port < VJobsRoot::MAXPORTS_ANIM ) )
		{
			SNPROF_ANIM( "sync(0) waitforfinish" );

			// sync on port
			PS3BoneJobData  *pBoneJobData = g_pBoneJobs->GetJobData( m_iPS3BoneJob_ID );

			// sync on port
			//Msg("sync job wait for finish %d\n", m_iPS3BoneJob_Port);
			CELL_VERIFY( g_pBoneJobs->m_pRoot->m_queuePortAnim[ m_iPS3BoneJob_Port ].sync( 0 ) );
		}

		// PASS 2
		StandardBlendingRules_Pass2();
		SetupBones_Pass2();

		// ensure lock released
		m_BoneSetupLock.Unlock();
	}

	// mark as not started, not running
	m_iPS3BoneJob_ID	= -1;
	m_iPS3BoneJob_Port	= -1;
}


//-----------------------------------------------------------------------------
// When an error has occured when initialising a bonejob (too many bones, or an unsupported SPU path encountered)
// we re-run the job over on PPU
//-----------------------------------------------------------------------------
void C_BaseAnimating::PS3BoneJob_RestartPPU( void )
{
	SNPROF_ANIM( "C_BaseAnimating::PS3BoneJob_RestartPPU" );

	// we will have locked the bones, so release the lock
	m_BoneSetupLock.Unlock();

	// start job over on PPU after resetting
	RestoreSetupBones_PS3();

	// call original (non-PS3) SetupBones
	//pBaseAnimating->SetupBones( NULL, -1, -1, g_pPS3BoneJobData[ lp ].m_boneJobSrcParams.m_fCurrentTime );
	SetupBones( NULL, -1, -1, gpGlobals->curtime );

	// mark as not started, not running
	m_iPS3BoneJob_ID	= -1;
	m_iPS3BoneJob_Port	= -1;
}

//-----------------------------------------------------------------------------
// main entry point for PS3 bone jobs
//
// run the g_PreviousBoneSetups list in two parts: things before we can run a job
// and things to do after the job has finished. 
// 
// a bone job is defined as whatever we have managed to comfortably/sensible push
// onto SPU (should be the bulk of the work) - right now it's the main core of
// standardblendingrules, which includes accumulatepose, the largest time sink.
//-----------------------------------------------------------------------------

void C_BaseAnimating::ThreadedBoneSetup_PS3( int nCount )
{
	SNPROF_ANIM( "C_BaseAnimating::ThreadedBoneSetup_PS3" );

#ifdef DEBUG_BONESETUP_THREADVSNONTHREAD
	Msg("*************************************************************\n" );
	Msg("*** THREADED BONESETUP START: gmbc: %d, time: %f ***\n", g_iModelBoneCounter, gpGlobals->curtime );
	Msg("*************************************************************\n" );
#endif

	switch( cl_PS3_SPU_bones.GetInt() )
	{
	case 0:
		// as before
		for ( int i = 0; i < nCount; i++ )
		{
			SetupBonesOnBaseAnimating( g_PreviousBoneSetups[i] );
		}
		break;
	case 1:
	case 2: 
		{
			int lp;

			// allocate working data set (that is live between pass 1 and 2)

			// init 
			int nMaxBoneJobs = InitAllPS3BoneJobs( nCount );

			g_pBoneJobs->StartFrame( nMaxBoneJobs );

			////////////////////////////////////////////////////////////////////////////////////////
			// Pass 1: prologue
			// and kick off AccumulatePose jobs
			// sort in place to process jobs in generation order: parent jobs first, then children, then 2nd generation, etc
			////////////////////////////////////////////////////////////////////////////////////////

			g_pBoneJobs->ResetBoneJobs();

			int nGen = 0;
			int nCount_Gen = 0;

			// generation order
			while( nCount_Gen < nMaxBoneJobs )
			{
				for ( lp = 0; lp < nCount; lp++ )
				{
					nCount_Gen += SetupBonesOnBaseAnimating_PS3( g_PreviousBoneSetups[ lp ], nGen );
				}
				nGen++;
			}

			////////////////////////////////////////////////////////////////////////////////////////
			//     
			////////////////////////////////////////////////////////////////////////////////////////

			////////////////////////////////////////////////////////////////////////////////////////
			// Epilogue - tidy up unfinished jobs
			////////////////////////////////////////////////////////////////////////////////////////
			for( lp = 0; lp < g_pBoneJobs->GetNumBoneJobs(); lp++ )
			{
				PS3BoneJobData*		pBoneJobData	= g_pBoneJobs->GetJobData( lp );
				C_BaseAnimating*	pBaseAnimating	= (C_BaseAnimating *)pBoneJobData->bonejobPPU.pBaseAnimating;

				pBaseAnimating->PS3BoneJob_End();
			}

			// free working data set
			g_pBoneJobs->EndFrame();
		}
		break;
	default:
		// as before
		for ( int i = 0; i < nCount; i++ )
		{
			SetupBonesOnBaseAnimating( g_PreviousBoneSetups[i] );
		}
		break;
	}

#ifdef DEBUG_BONESETUP_THREADVSNONTHREAD
	Msg("********************************\n" );
	Msg("***  THREADED BONESETUP END  ***\n" );
	Msg("********************************\n" );
#endif

}


//-----------------------------------------------------------------------------
// pre bone job pass for standardblendingrules
// this will end up actually running the job
//-----------------------------------------------------------------------------
bool C_BaseAnimating::StandardBlendingRules_Pass1( CStudioHdr *hdr, float currentTime, int nMaxBones, int boneMask, int bonesMaskNeedRecalc, int oldReadableBones, matrix3x4_t &parentTransform )
{
//	SNPROF_ANIM( "C_BaseAnimating::StandardBlendingRules_Pass1" );

	float		poseparam[MAXSTUDIOPOSEPARAM];

	if( !hdr )
		return false;

	if( !hdr->SequencesAvailable() )
	{
		return false;
	}

	if( GetSequence() >= hdr->GetNumSeq() || GetSequence() == -1 ) 
	{
		SetSequence( 0 );
	}
	
#ifdef DEMOPOLISH_ENABLED
	if ( IsDemoPolishPlaying() && IsPlayer() )
	{
		float const flDemoPlaybackTime = DemoPolish_GetController().GetAdjustedPlaybackTime();
		int const iSequenceOverride = DemoPolish_GetController().GetSequenceOverride( entindex(), flDemoPlaybackTime );
		if ( iSequenceOverride >= 0 )
		{
			// Override.
			SetSequence( iSequenceOverride );
		}
	}
#endif

	GetPoseParameters( hdr, poseparam );

	// SET UP AND FIRE OFF JOB
	PS3BoneJob_Run( hdr, currentTime, GetCycle(), nMaxBones, boneMask, bonesMaskNeedRecalc, oldReadableBones, parentTransform, poseparam );

	// p and q filled - end of pass 1
	return true;
}

//-----------------------------------------------------------------------------
// post bone job pass of standardblendingrules
// should be the first thing that happens next when a bone job finishes
//-----------------------------------------------------------------------------

bool C_BaseAnimating::StandardBlendingRules_Pass2( void )
{
//	SNPROF_ANIM( "C_BaseAnimating::StandardBlendingRules_Pass2" );

	Assert( m_iPS3BoneJob_ID != -1 );

	PS3BoneJobData	*pBoneJobData   = g_pBoneJobs->GetJobData( m_iPS3BoneJob_ID );

	bonejob_PPU		*pBonejob_PPU   = &pBoneJobData->bonejobPPU;
	bonejob_SPU		*pBonejob_SPU   = &pBoneJobData->bonejobSPU;

	// re-setup with original args
	IBoneSetup boneSetup( pBonejob_PPU->pStudioHdr, pBonejob_PPU->boneMask, pBonejob_SPU->poseparam );

	// 
	BoneVector *pos					= pBoneJobData->pos;
	BoneQuaternion	*q				= pBoneJobData->q;

	float currentTime				= pBonejob_SPU->currentTime;
	float fCycle					= pBonejob_PPU->cycle;
	CStudioHdr *hdr					= pBonejob_PPU->pStudioHdr;

	int boneMask					= pBonejob_SPU->boneMask;

	if( hdr->numbonecontrollers() )
	{
		float controllers[ MAXSTUDIOBONECTRLS ];
		GetBoneControllers( controllers );
		boneSetup.CalcBoneAdj( pos, q, controllers );
	}

	UnragdollBlend( hdr, pos, q, currentTime );

	return true;
}



//-----------------------------------------------------------------------------
// pre bone job pass for setupbones
// determines whether the job should run
//-----------------------------------------------------------------------------
bool C_BaseAnimating::SetupBones_Pass1( float currentTime )
{
//	SNPROF_ANIM( "C_BaseAnimating::SetupBones_Pass1" );

	int boneMask  = -1;//m_iAccumulatedBoneMask;//-1;
	int nMaxBones = -1;

	// for now, we MUST be in threaded bone setup

	if( GetSequence() == -1 )
		return false;

	if( boneMask == -1 )
	{
		boneMask = m_iPrevBoneMask;
	}

	// We should get rid of this someday when we have solutions for the odd cases where a bone doesn't
	// get setup and its transform is asked for later.
// 	if ( cl_SetupAllBones.GetInt() )
// 	{
// 		boneMask |= BONE_USED_BY_ANYTHING;
// 	}

	if ( !m_BoneSetupLock.TryLock() )
	{
		// someone else is handling
		// bones are in some intermediate state, wait until the other thread is done.  
		// If they've setup what bones we want, it'll early out down below
		m_BoneSetupLock.Lock();
	}
	// else, we have the lock

	// If we're setting up LOD N, we have set up all lower LODs also
	// because lower LODs always use subsets of the bones of higher LODs.
	int nLOD = 0;
	int nMask = BONE_USED_BY_VERTEX_LOD0;
	for( ; nLOD < MAX_NUM_LODS; ++nLOD, nMask <<= 1 )
	{
		if ( boneMask & nMask )
			break;
	}
	for( ; nLOD < MAX_NUM_LODS; ++nLOD, nMask <<= 1 )
	{
		boneMask |= nMask;
	}

	// A bit of a hack, but this way when in prediction we use the "correct" gpGlobals->curtime -- rather than the
	// one that the player artificially advances
	if ( GetPredictable() && 
		prediction->InPrediction() )
	{
		currentTime = prediction->GetSavedTime();
	}


#if defined(_DEBUG_SPUvPPU_ANIMATION)
	m_BoneAccessor.SetReadableBones( 0 );
	m_BoneAccessor.SetWritableBones( 0 );
	m_flLastBoneSetupTime = 0.0f;
	m_iPrevBoneMask = m_iAccumulatedBoneMask;
	m_iAccumulatedBoneMask = 0;
#else
	if( m_iMostRecentModelBoneCounter != g_iModelBoneCounter )
	{
		// Clear out which bones we've touched this frame if this is 
		// the first time we've seen this object this frame.
		// BUGBUG: Time can go backward due to prediction, catch that here until a better solution is found
		if ( LastBoneChangedTime() >= m_flLastBoneSetupTime || currentTime < m_flLastBoneSetupTime )
		{
#ifdef DEBUG_BONESETUP_THREADVSNONTHREAD
			Msg("_Pass1 1st time: %x, mask: %d, mrmbc: %d, time:%f\n", this, boneMask, m_iMostRecentModelBoneCounter, currentTime );
#endif

			m_BoneAccessor.SetReadableBones( 0 );
			m_BoneAccessor.SetWritableBones( 0 );
			m_flLastBoneSetupTime = currentTime;

#if defined( DBGFLAG_ASSERT )
			m_vBoneSetupCachedOrigin = GetRenderOrigin();
			m_qBoneSetupCachedAngles = GetRenderAngles();
#endif
		}
		
		m_iPrevBoneMask = m_iAccumulatedBoneMask;
		m_iAccumulatedBoneMask = 0;
	}
#endif


	// Keep track of everthing asked for over the entire frame
	// But not those things asked for during bone setup
	//	if ( !g_bInThreadedBoneSetup )
	{
		m_iAccumulatedBoneMask |= boneMask;
	}

#if !defined(_DEBUG_SPUvPPU_ANIMATION)
	// Make sure that we know that we've already calculated some bone stuff this time around.
	m_iMostRecentModelBoneCounter = g_iModelBoneCounter;
#endif


	// Have we cached off all bones meeting the flag set?
	if( ( m_BoneAccessor.GetReadableBones() & boneMask ) != boneMask )
	{
		CStudioHdr *hdr = GetModelPtr();
		if ( !hdr || !hdr->SequencesAvailable() )
		{
			m_BoneSetupLock.Unlock();
			return false;
		}

#if defined( DBGFLAG_ASSERT )
		bool bHadDirtyAbsTransform = IsEFlagSet( EFL_DIRTY_ABSTRANSFORM );
#endif

		// Setup our transform based on render angles and origin.
 		ALIGN16 matrix3x4_t parentTransform ALIGN16_POST;

		if( cl_PS3_SPU_bones_safesync.GetInt() )
		{
			AngleMatrix( GetRenderAngles(), GetRenderOrigin(), parentTransform );
			AssertMsg( !bHadDirtyAbsTransform || !IsEFlagSet( EFL_DIRTY_ABSTRANSFORM ), "Using an old origin/angles and unable to recompute before caching off the bones" );
		}

		// Load the boneMask with the total of what was asked for last frame.
		// WHY??
		boneMask |= m_iPrevBoneMask;

		// Allow access to the bones we're setting up so we don't get asserts in here.
		int oldReadableBones = m_BoneAccessor.GetReadableBones();
		m_BoneAccessor.SetWritableBones( m_BoneAccessor.GetReadableBones() | boneMask );
		m_BoneAccessor.SetReadableBones( m_BoneAccessor.GetWritableBones() );

		if (hdr->flags() & STUDIOHDR_FLAGS_STATIC_PROP)
		{
			if( !cl_PS3_SPU_bones_safesync.GetInt() )
			{
				if( m_iPS3BoneJob_Port >= 0 )
				{
					Assert( m_iPS3BoneJob_ID >= 0 );
					PS3BoneJobData *pBoneJob = g_pBoneJobs->GetJobData( m_iPS3BoneJob_ID );
					C_BaseAnimating *pDependantBaseAnim = (C_BaseAnimating *)pBoneJob->bonejobPPU.pBaseAnimating;
					pDependantBaseAnim->PS3BoneJob_WaitForFinish();
				}

				if( m_iPS3BoneJob_DependantID != -1 )
				{
					PS3BoneJobData *pBoneJob = g_pBoneJobs->GetJobData( m_iPS3BoneJob_DependantID );
					C_BaseAnimating *pDependantBaseAnim = (C_BaseAnimating *)pBoneJob->bonejobPPU.pBaseAnimating;

					pDependantBaseAnim->PS3BoneJob_WaitForFinish();
				}

				// reset
				PS3BoneJob_PreInit();

				AngleMatrix( GetRenderAngles(), GetRenderOrigin(), parentTransform );
			}

			MatrixCopy(	parentTransform, GetBoneForWrite( 0 ) );
		}
		else
		{
			//---------------------------------------------------------------------------
			// NOTE: This is the only path taken that actually creates real PS3 bone jobs
			//---------------------------------------------------------------------------

			// This is necessary because it's possible that CalculateIKLocks will trigger our move children
			// to call GetAbsOrigin(), and they'll use our OLD bone transforms to get their attachments
			// since we're right in the middle of setting up our new transforms. 
			//
			// Setting this flag forces move children to keep their abs transform invalidated.
			AddEFlags( EFL_SETTING_UP_BONES );

			// NOTE: For model scaling, we need to opt out of IK because it will mark the bones as already being calculated
			// [msmith]: What game is it that want's to do model scaling and needs to opt out of IK?  It seems as if opting out of IK should be the exception and not the rule here.
			// I suggest we change the #ifdef such that only the games that need to kill IK get used here... rather than ORing in all other games that use this engine in the future.
#if defined( PORTAL2 ) || defined( INFESTED ) || defined( CSTRIKE15 )
			// only allocate an ik block if the npc can use it
			if ( !m_pIk && hdr->numikchains() > 0 && !(m_EntClientFlags & ENTCLIENTFLAG_DONTUSEIK) )
				m_pIk = new CIKContext;
#endif // PORTAL2

			int bonesMaskNeedRecalc = boneMask | oldReadableBones; // Hack to always recalc bones, to fix the arm jitter in the new CS player anims until Ken makes the real fix

			if( cl_PS3_SPU_bones_safesync.GetInt() )
			{
				if ( m_pIk )
				{
					if (Teleported() || IsEffectActive(EF_NOINTERP))
					{
						m_pIk->ClearTargets();
					}

					m_pIk->Init( hdr, GetRenderAngles(), GetRenderOrigin(), currentTime, gpGlobals->framecount, bonesMaskNeedRecalc );
				}
			}

			// return here, and notify that we've set up a pass1 job
			return StandardBlendingRules_Pass1( hdr, currentTime, nMaxBones, boneMask, bonesMaskNeedRecalc, oldReadableBones, parentTransform );
		}

		if( !( oldReadableBones & BONE_USED_BY_ATTACHMENT ) && ( boneMask & BONE_USED_BY_ATTACHMENT ) )
		{
			SetupBones_AttachmentHelper( hdr );
		}

	}
	else
	{
		if( !cl_PS3_SPU_bones_safesync.GetInt() )
		{
			if( m_iPS3BoneJob_Port >= 0 )
			{
				Assert( m_iPS3BoneJob_ID >= 0 );
				PS3BoneJobData *pBoneJob = g_pBoneJobs->GetJobData( m_iPS3BoneJob_ID );
				C_BaseAnimating *pDependantBaseAnim = (C_BaseAnimating *)pBoneJob->bonejobPPU.pBaseAnimating;
				pDependantBaseAnim->PS3BoneJob_WaitForFinish();
			}

			if( m_iPS3BoneJob_DependantID != -1 )
			{
				PS3BoneJobData *pBoneJob = g_pBoneJobs->GetJobData( m_iPS3BoneJob_DependantID );
				C_BaseAnimating *pDependantBaseAnim = (C_BaseAnimating *)pBoneJob->bonejobPPU.pBaseAnimating;

				pDependantBaseAnim->PS3BoneJob_WaitForFinish();
			}

			// reset
			PS3BoneJob_PreInit();
		}

	}


	m_BoneSetupLock.Unlock();

	return false;
}

//-----------------------------------------------------------------------------
// post bone job pass for setupbones
// should be the first thing that follows pass2 of standardblendingrules
//-----------------------------------------------------------------------------
bool C_BaseAnimating::SetupBones_Pass2( void )
{
	SNPROF_ANIM( "C_BaseAnimating::SetupBones_Pass2" );

	if( m_iPS3BoneJob_ID < 0 )
		return false;

	PS3BoneJobData *pJobData	= g_pBoneJobs->GetJobData( m_iPS3BoneJob_ID );

	bonejob_PPU *pBonejobPPU	= &pJobData->bonejobPPU;
	bonejob_SPU *pBonejobSPU	= &pJobData->bonejobSPU;

	CStudioHdr *hdr				= pBonejobPPU->pStudioHdr;
	int	boneMask				= pBonejobPPU->boneMask;
	int oldReadableBones		= pBonejobPPU->oldReadableBones;
	float currentTime			= pBonejobSPU->currentTime;
	int bonesMaskNeedRecalc		= pBonejobPPU->bonesMaskNeedsRecalc;

	BoneVector		*pos		= pJobData->pos;
	BoneQuaternion	*q			= pJobData->q;

	matrix3x4_t parentTransform;
	MatrixCopy( pBonejobPPU->parentTransform, parentTransform );


	CBoneBitList boneComputed;

	// finish _pass2 of AddDependencies

// IKOFF
	if( m_pIk && !IsRagdoll() )
	{
		// don't calculate IK on ragdolls
		m_pIk->AddAllDependencies_PS3( pJobData->addDep_IKRules, pJobData->addDep_numIKRules );

		UpdateIKLocks( currentTime );

		m_pIk->UpdateTargets( pos, q, m_BoneAccessor.GetBoneArrayForWrite(), boneComputed );

		CalculateIKLocks( currentTime );
		m_pIk->SolveDependencies( pos, q, m_BoneAccessor.GetBoneArrayForWrite(), boneComputed );
	}

	BuildTransformations( hdr, pos, q, parentTransform, bonesMaskNeedRecalc, boneComputed );



	// Draw skeleton?
	if( enable_skeleton_draw.GetBool() )
	{
		DrawSkeleton( hdr, boneMask );
	}

	RemoveEFlags( EFL_SETTING_UP_BONES );
	ControlMouth( hdr );

	if( !( oldReadableBones & BONE_USED_BY_ATTACHMENT ) && ( boneMask & BONE_USED_BY_ATTACHMENT ) )
	{
		SetupBones_AttachmentHelper( hdr );
	}

	return true;
}


#endif // _PS3
//-----------------------------------------------------------------------------
//
// END OF PS3 BONE JOBS
//
//-----------------------------------------------------------------------------


C_BaseAnimating* C_BaseAnimating::FindFollowedEntity()
{
	C_BaseEntity *follow = GetFollowedEntity();

	if ( !follow )
		return NULL;

	if ( follow->IsDormant() )
		return NULL;

	if ( !follow->GetModel() )
	{
		Warning( "mod_studio: MOVETYPE_FOLLOW with no model.\n" );
		return NULL;
	}

	if ( modelinfo->GetModelType( follow->GetModel() ) != mod_studio )
	{
		Warning( "Attached %s (mod_studio) to %s (%d)\n", 
			modelinfo->GetModelName( GetModel() ), 
			modelinfo->GetModelName( follow->GetModel() ), 
			modelinfo->GetModelType( follow->GetModel() ) );
		return NULL;
	}

	return assert_cast< C_BaseAnimating* >( follow );
}


void C_BaseAnimating::InvalidateBoneCache()
{
	// mariod - testing
 	if( !s_bEnableInvalidateBoneCache )
 		return;

//	SNPROF_ANIM("Anim_InvalidateBoneCache");


	m_iMostRecentModelBoneCounter = g_iModelBoneCounter - 1;
	m_flLastBoneSetupTime = -FLT_MAX; 
}


bool C_BaseAnimating::IsBoneCacheValid() const
{
	return m_iMostRecentModelBoneCounter == g_iModelBoneCounter;
}


// Causes an assert to happen if bones or attachments are used while this is false.
struct BoneAccess
{
	BoneAccess()
	{
		bAllowBoneAccessForNormalModels = false;
		bAllowBoneAccessForViewModels = false;
		tag = NULL;
	}

	bool bAllowBoneAccessForNormalModels;
	bool bAllowBoneAccessForViewModels;
	char const *tag;
};

static CUtlVector< BoneAccess >		g_BoneAccessStack;
static BoneAccess g_BoneAcessBase;

bool C_BaseAnimating::IsBoneAccessAllowed() const
{
	if ( !ThreadInMainThread() )
	{
		return true;
	}

	if ( IsViewModel() )
		return g_BoneAcessBase.bAllowBoneAccessForViewModels;
	else
		return g_BoneAcessBase.bAllowBoneAccessForNormalModels;
}

// (static function)
void C_BaseAnimating::PushAllowBoneAccess( bool bAllowForNormalModels, bool bAllowForViewModels, char const *tagPush )
{
	if ( !ThreadInMainThread() )
	{
		return;
	}
	BoneAccess save = g_BoneAcessBase;
	g_BoneAccessStack.AddToTail( save );

	Assert( g_BoneAccessStack.Count() < 32 ); // Most likely we are leaking "PushAllowBoneAccess" calls if PopBoneAccess is never called. Consider using AutoAllowBoneAccess.
	g_BoneAcessBase.bAllowBoneAccessForNormalModels = bAllowForNormalModels;
	g_BoneAcessBase.bAllowBoneAccessForViewModels = bAllowForViewModels;
	g_BoneAcessBase.tag = tagPush;
}

void C_BaseAnimating::PopBoneAccess( char const *tagPop )
{
	if ( !ThreadInMainThread() )
	{
		return;
	}
	// Validate that pop matches the push
	Assert( ( g_BoneAcessBase.tag == tagPop ) || ( g_BoneAcessBase.tag && g_BoneAcessBase.tag != ( char const * ) 1 && tagPop && tagPop != ( char const * ) 1 && !strcmp( g_BoneAcessBase.tag, tagPop ) ) );
	int lastIndex = g_BoneAccessStack.Count() - 1;
	if ( lastIndex < 0 )
	{
		Assert( !"C_BaseAnimating::PopBoneAccess:  Stack is empty!!!" );
		return;
	}
	g_BoneAcessBase = g_BoneAccessStack[lastIndex ];
	g_BoneAccessStack.Remove( lastIndex );
}

C_BaseAnimating::AutoAllowBoneAccess::AutoAllowBoneAccess( bool bAllowForNormalModels, bool bAllowForViewModels )
{
	C_BaseAnimating::PushAllowBoneAccess( bAllowForNormalModels, bAllowForViewModels, ( char const * ) 1 );
}

C_BaseAnimating::AutoAllowBoneAccess::~AutoAllowBoneAccess( )
{
	C_BaseAnimating::PopBoneAccess( ( char const * ) 1 );
}

// (static function)
void C_BaseAnimating::InvalidateBoneCaches()
{
	g_nNumBonesSetupAll = g_nNumBonesSetupAllTemp;
	g_nNumBonesSetupAllTemp = 0;
	g_nNumBonesSetupBlendingRulesOnly = g_nNumBonesSetupBlendingRulesOnlyTemp;
	g_nNumBonesSetupBlendingRulesOnlyTemp = 0;

	g_iModelBoneCounter++;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseAnimating::GetRootBone( matrix3x4_t &rootBone )
{
	if ( IsEffectActive( EF_BONEMERGE ) && GetMoveParent() && m_pBoneMergeCache )
		return m_pBoneMergeCache->GetRootBone( rootBone );

	GetBoneTransform( 0, rootBone );
	return true;
}


ConVar r_drawothermodels( "r_drawothermodels", "1", FCVAR_CHEAT, "0=Off, 1=Normal, 2=Wireframe" );


bool C_BaseAnimating::UpdateBlending( int flags, const RenderableInstance_t &instance )
{
	if ( flags & STUDIO_RENDER )
	{
		// Determine blending amount and tell engine
		float blend = (float)( instance.m_nAlpha / 255.0f );

		// Totally gone
		if ( blend <= 0.0f )
			return false;

		// Tell engine
		render->SetBlend( blend );

		float color[3];
		GetColorModulation( color );
		render->SetColorModulation(	color );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Draws the object
// Input  : flags - 
//-----------------------------------------------------------------------------
ConVar r_drawmodelnames("r_drawmodelnames", "0", FCVAR_CHEAT|FCVAR_REPLICATED);
int C_BaseAnimating::DrawModel( int flags, const RenderableInstance_t &instance )
{
	VPROF_BUDGET( "C_BaseAnimating::DrawModel", VPROF_BUDGETGROUP_MODEL_RENDERING );
	if ( !m_bReadyToDraw )
		return 0;

	float flPrevBlend = render->GetBlend();

	if ( IsRenderForceOpaquePass() && GetRenderAlpha() < 255 )
	{
		// if this model is being forced into the opaque pass, it may have non-opaque alpha that should be respected.
		RenderableInstance_t temp;
		temp.m_nAlpha = GetRenderAlpha();
		if ( !UpdateBlending( flags, temp ) )
		{
			return 0;
		}
	}

#if defined ( PORTAL2 )
	if ( IsRenderingWithViewModels() )
	{
		if ( !UpdateBlending( flags, instance ) )
		{
			return 0;
		}
	}
#endif

	int drawn = 0;

	if ( r_drawothermodels.GetInt() )
	{
		MDLCACHE_CRITICAL_SECTION();

		int extraFlags = 0;
		if ( r_drawothermodels.GetInt() == 2 )
		{
			extraFlags |= STUDIO_WIREFRAME;
		}

		if ( flags & STUDIO_SHADOWDEPTHTEXTURE )
		{
			extraFlags |= STUDIO_SHADOWDEPTHTEXTURE;
		}

		if ( flags & STUDIO_SSAODEPTHTEXTURE )
		{
			extraFlags |= STUDIO_SSAODEPTHTEXTURE;
		}

		if ( flags & STUDIO_AOPREPASSTEXURE )
		{
			extraFlags |= STUDIO_AOPREPASSTEXURE;
		}

		if ( flags & STUDIO_DONOTMODIFYSTENCILSTATE )
		{
			extraFlags |= STUDIO_DONOTMODIFYSTENCILSTATE;
		}

		if ( flags & STUDIO_SKIP_DECALS )
		{
			extraFlags |= STUDIO_SKIP_DECALS;
		}

		// Necessary for lighting blending
		CreateModelInstance();

		if ( !IsFollowingEntity() )
		{
			drawn = InternalDrawModel( flags|extraFlags, instance );
		}
		else
		{
			// this doesn't draw unless master entity is visible and it's a studio model!!!
			C_BaseAnimating *follow = FindFollowedEntity();
			if ( follow )
			{
				// recompute master entity bone structure
				int baseDrawn = follow->DrawModel( 0, instance );

				// draw entity
				// FIXME: Currently only draws if aiment is drawn.  
				// BUGBUG: Fixup bbox and do a separate cull for follow object
				if ( baseDrawn )
				{
					drawn = InternalDrawModel( flags|extraFlags, instance );
				}
			}
		}
	}

	render->SetBlend( flPrevBlend );

	// If we're visualizing our bboxes, draw them
	DrawBBoxVisualizations();

	if ( r_drawmodelnames.GetBool() && m_pStudioHdr )
	{
		debugoverlay->AddTextOverlay( GetAbsOrigin(), 0, "Model: %s", m_pStudioHdr->name() );
	}

	return drawn;
}

//-----------------------------------------------------------------------------
// Purpose: Draw skeleton topology & coordinate systems
//-----------------------------------------------------------------------------
void C_BaseAnimating::DrawSkeleton( CStudioHdr const* pHdr, int iBoneMask ) const
{
	if ( !pHdr )
		return;

	Vector from, to;
	for ( int i = 0; i < pHdr->numbones(); ++i )
	{
		if ( !(pHdr->boneFlags( i ) & iBoneMask) )
			continue;

		debugoverlay->AddCoordFrameOverlay( m_BoneAccessor[ i ], 3.0f );

		int const iParentIndex = pHdr->boneParent( i );
		if ( iParentIndex < 0 )
			continue;

		MatrixPosition( m_BoneAccessor[ i            ], from );
		MatrixPosition( m_BoneAccessor[ iParentIndex ], to   );

		debugoverlay->AddLineOverlay( from, to, 0, 255, 255, true, 0.0f );
	}
}

//-----------------------------------------------------------------------------
// Gets the hitbox-to-world transforms, returns false if there was a problem
//-----------------------------------------------------------------------------
bool C_BaseAnimating::HitboxToWorldTransforms( matrix3x4_t *pHitboxToWorld[MAXSTUDIOBONES] )
{
	if ( !IsBoneCacheValid() )
	{
		MDLCACHE_CRITICAL_SECTION();

		if ( !GetModel() )
			return false;

		CStudioHdr *pStudioHdr = GetModelPtr();
		if (!pStudioHdr)
			return false;

		mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( GetHitboxSet() );
		if ( !set )
			return false;

		if ( !set->numhitboxes )
			return false;

		SetupBones( NULL, -1, BONE_USED_BY_HITBOX, gpGlobals->curtime );
	}

	for ( int i = 0; i < m_CachedBoneData.Count(); i++ )
	{
		// UNDONE: Some of these bones haven't been set up.  Is it necessary to check each
		// one for membership in the hitbox set and NULL it if it isn't present?
		pHitboxToWorld[i] = &m_CachedBoneData[i];
	}

	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool C_BaseAnimating::OnPostInternalDrawModel( ClientModelRenderInfo_t *pInfo )
{
	return true;
}

//----------------------------------------------------------------------------
// Hooks into the fast path render system
//----------------------------------------------------------------------------
static ConVar r_drawmodelstatsoverlay( "r_drawmodelstatsoverlay", "0", FCVAR_CHEAT );

IClientModelRenderable*	C_BaseAnimating::GetClientModelRenderable()
{ 
	// Cannot participate if it has a render clip plane
	if ( !m_bCanUseFastPath || m_bIsUsingRelativeLighting )
		return NULL;
	
	if ( r_drawothermodels.GetInt() != 1 || r_drawmodelstatsoverlay.GetInt() != 0 || mat_wireframe.GetInt() != 0 )
		return NULL;

	if ( IsFollowingEntity() && !FindFollowedEntity() )
		return NULL;

#ifdef PORTAL
	if ( GetRenderClipPlane() != NULL )
		return NULL;
#endif

	return this; 
}


//----------------------------------------------------------------------------
// Hooks into the fast path render system
//----------------------------------------------------------------------------
bool C_BaseAnimating::GetRenderData( void *pData, ModelDataCategory_t nCategory )
{
	switch ( nCategory )
	{
	case MODEL_DATA_LIGHTING_MODEL:
		// Necessary for lighting blending
		CreateModelInstance();
		*(RenderableLightingModel_t*)pData = LIGHTING_MODEL_STANDARD;
		return true;

	case MODEL_DATA_STENCIL:
		return ComputeStencilState( (ShaderStencilState_t*)pData );

	default:
		return false;
//		return BaseClass::GetRenderData( pData, nCategory );
	}
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool C_BaseAnimating::OnInternalDrawModel( ClientModelRenderInfo_t *pInfo )
{
	if ( m_hLightingOrigin )
	{
		pInfo->pLightingOrigin = &(m_hLightingOrigin->GetAbsOrigin());
	}
	return true;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void C_BaseAnimating::DoInternalDrawModel( IMatRenderContext *pRenderContext, ClientModelRenderInfo_t *pInfo, DrawModelState_t *pState, matrix3x4_t *pBoneToWorldArray )
{
	if ( pState)
	{
		modelrender->DrawModelExecute( pRenderContext, *pState, *pInfo, pBoneToWorldArray );
	}

	if ( vcollide_wireframe.GetBool() )
	{
		if ( IsRagdoll() )
		{
			m_pRagdoll->DrawWireframe();
		}
		else if ( IsSolid() && CollisionProp()->GetSolid() == SOLID_VPHYSICS )
		{
			vcollide_t *pCollide = modelinfo->GetVCollide( GetModelIndex() );
			if ( pCollide && pCollide->solidCount == 1 )
			{
				static color32 debugColor = {0,255,255,0};
				matrix3x4_t matrix;
				AngleMatrix( GetAbsAngles(), GetAbsOrigin(), matrix );
				engine->DebugDrawPhysCollide( pCollide->solids[0], NULL, matrix, debugColor );
				if ( VPhysicsGetObject() )
				{
					static color32 debugColorPhys = {255,0,0,0};
					matrix3x4_t matrix;
					VPhysicsGetObject()->GetPositionMatrix( &matrix );
					engine->DebugDrawPhysCollide( pCollide->solids[0], NULL, matrix, debugColorPhys );
				}
			}
		}
	}
}



//----------------------------------------------------------------------------
// Computes stencil settings
//----------------------------------------------------------------------------
bool C_BaseAnimating::ComputeStencilState( ShaderStencilState_t *pStencilState )
{
#if defined( _X360 )
	if ( !r_shadow_deferred.GetBool() )
	{
		// Early out if we don't care about deferred shadow masks
		return false;
	}

	uint32 mask = 0x0;
	uint32 nRef = mask;

	mask |= 1 << 2;	// Stencil for masking deferred shadows uses 0x4
	bool bCastsShadows = ( ShadowCastType() != SHADOWS_NONE );
	nRef |= bCastsShadows << 2;

	pStencilState->m_bEnable = true;
	pStencilState->m_nTestMask = 0xFFFFFFFF;
	pStencilState->m_nWriteMask = mask;
	pStencilState->m_nReferenceValue = nRef;
	pStencilState->m_CompareFunc = SHADER_STENCILFUNC_ALWAYS;
	pStencilState->m_PassOp = SHADER_STENCILOP_SET_TO_REFERENCE;
	pStencilState->m_FailOp = SHADER_STENCILOP_KEEP;
	pStencilState->m_ZFailOp = SHADER_STENCILOP_KEEP;

	// Deferred shadow rendering:
	// set or clear hi-stencil depending on shadow cast type
	pStencilState->m_bHiStencilEnable = false;
	pStencilState->m_bHiStencilWriteEnable = true;
	pStencilState->m_HiStencilCompareFunc = SHADER_HI_STENCILFUNC_NOTEQUAL;
	pStencilState->m_nHiStencilReferenceValue = 0;
	/*
	// The old hi-stencil state, that unmasked too many tiles for things receiving shadows
	pStencilState->m_HiStencilCompareFunc = bCastsShadows ? SHADER_HI_STENCILFUNC_NOTEQUAL : SHADER_HI_STENCILFUNC_EQUAL;
	pStencilState->m_nHiStencilReferenceValue = bCastsShadows ? 0 : 1;
	*/

	return true;
#else
	return false;
#endif
}



//-----------------------------------------------------------------------------
// Purpose: Draws the object
// Input  : flags - 
//-----------------------------------------------------------------------------
int C_BaseAnimating::InternalDrawModel( int flags, const RenderableInstance_t &instance )
{
	VPROF( "C_BaseAnimating::InternalDrawModel" );

	if ( !GetModel() )
		return 0;

	// This should never happen, but if the server class hierarchy has bmodel entities derived from CBaseAnimating or does a
	//  SetModel with the wrong type of model, this could occur.
	if ( modelinfo->GetModelType( GetModel() ) != mod_studio )
	{
		return BaseClass::DrawModel( flags, instance );
	}

	// Make sure hdr is valid for drawing
	if ( !GetModelPtr() )
		return 0;

	bool bUsingStencil = false;
	CMatRenderContextPtr pRenderContext( materials );
	if ( !( flags & STUDIO_DONOTMODIFYSTENCILSTATE ) && !( flags & STUDIO_SHADOWDEPTHTEXTURE ) && ( flags & STUDIO_RENDER ) )
	{
		ShaderStencilState_t state;
		bUsingStencil = ComputeStencilState( &state );
		if ( bUsingStencil )
		{
			pRenderContext->SetStencilState( state );
		}
	}

	ClientModelRenderInfo_t info;
	ClientModelRenderInfo_t *pInfo;

	pInfo = &info;

	pInfo->flags = flags;
	pInfo->pRenderable = this;
	pInfo->instance = GetModelInstance();
	pInfo->entity_index = index;
	pInfo->pModel = GetModel();
	pInfo->origin = GetRenderOrigin();
	pInfo->angles = GetRenderAngles();
	pInfo->skin = GetSkin();
	pInfo->body = m_nBody;
	pInfo->hitboxset = m_nHitboxSet;

	bool bMarkAsDrawn = false;
	if ( OnInternalDrawModel( pInfo ) )
	{
		Assert( !pInfo->pModelToWorld);
		if ( !pInfo->pModelToWorld )
		{
			pInfo->pModelToWorld = &pInfo->modelToWorld;

			// Turns the origin + angles into a matrix
			AngleMatrix( pInfo->angles, pInfo->origin, pInfo->modelToWorld );
		}

		// Suppress unlocking
		CMatRenderDataReference rd( pRenderContext );
		DrawModelState_t state;
		matrix3x4_t *pBoneToWorld;
		bMarkAsDrawn = modelrender->DrawModelSetup( pRenderContext, *pInfo, &state, &pBoneToWorld );

		// Scale the base transform if we don't have a bone hierarchy
		if ( GetModelScale() > 1.0f+FLT_EPSILON || GetModelScale() < 1.0f-FLT_EPSILON )
		{
			CStudioHdr *pHdr = GetModelPtr();
			if ( pHdr && pBoneToWorld && pHdr->numbones() == 1 )
			{
				// Scale the bone to world at this point
				const float flScale = GetModelHierarchyScale();
				VectorScale( (*pBoneToWorld)[0], flScale, (*pBoneToWorld)[0] );
				VectorScale( (*pBoneToWorld)[1], flScale, (*pBoneToWorld)[1] );
				VectorScale( (*pBoneToWorld)[2], flScale, (*pBoneToWorld)[2] );
			}
		}

		bool bOverride = false;
		if ( cl_custom_material_override.GetBool() && ( flags & STUDIO_RENDER ) && !modelrender->IsForcedMaterialOverride() && GetCustomMaterialCount() > 0 )
		{
			for ( int i = 0; i < GetCustomMaterialCount(); i++ )
			{
				if ( IsCustomMaterialValid( i ) )
				{
					modelrender->ForcedMaterialOverride( GetCustomMaterial( i )->GetMaterial(), OVERRIDE_SELECTIVE, i );
					bOverride = true;
				}
			}
		}

		DoInternalDrawModel( pRenderContext, pInfo, ( bMarkAsDrawn && ( pInfo->flags & STUDIO_RENDER ) ) ? &state : NULL, pBoneToWorld );

		if ( bOverride )
		{
			modelrender->ForcedMaterialOverride( NULL );
		}
	}

	if ( bUsingStencil )
	{
		ShaderStencilState_t state;
		state.m_bEnable = false;
		#if defined( _X360 )
			// Deferred shadow rendering: Disable Hi-Stencil
			state.m_bHiStencilEnable = false;
			state.m_bHiStencilWriteEnable = false;
		#endif
		pRenderContext->SetStencilState( state );
	}

	OnPostInternalDrawModel( pInfo );

	return bMarkAsDrawn;
}

extern ConVar muzzleflash_light;

void C_BaseAnimating::ProcessMuzzleFlashEvent()
{
	// If we have an attachment, then stick a light on it.
	if ( muzzleflash_light.GetBool() )
	{
		//FIXME: We should really use a named attachment for this
		if ( m_Attachments.Count() > 0 )
		{
			Vector vAttachment;
			QAngle dummyAngles;

			// NOTE! This is typically "1" by name.  Also, if you add an illumination position, the illumination position will usually take the 1 slot which will cause the light to flash at the illumination position.
			AssertMsg(false, "This code assumes that the first attachment in all models is the muzzle flash.  Make sure this is guaranteed or fix this code.");
			GetAttachment( 1, vAttachment, dummyAngles );

			// Make an elight
			dlight_t *el = effects->CL_AllocElight( LIGHT_INDEX_MUZZLEFLASH + index );
			el->origin = vAttachment;
			el->radius = random->RandomInt( 32, 64 ); 
			el->decay = el->radius / 0.05f;
			el->die = gpGlobals->curtime + 0.05f;
			el->color.r = 255;
			el->color.g = 192;
			el->color.b = 64;
			el->color.exponent = 5;
		}
	}
}

//-----------------------------------------------------------------------------
// Internal routine to process animation events for studiomodels
//-----------------------------------------------------------------------------
void C_BaseAnimating::DoAnimationEvents( CStudioHdr *pStudioHdr )
{
	bool watch = false;//IsPlayer(); // Q_strstr( hdr->name, "rifle" ) ? true : false;

	//Adrian: eh? This should never happen.
	if ( GetSequence() == -1 )
		 return;

	// build root animation
	float flEventCycle = GetCycle();

	// If we're invisible, don't draw the muzzle flash
	bool bIsInvisible = !IsVisibleToAnyPlayer() && !IsViewModel() && !IsMenuModel();
	if ( bIsInvisible && !clienttools->IsInRecordingMode() )
		return;

#if !defined( CSTRIKE15 )
	// We already handle muzzle flash events in CSTRIKE15.
	// Also this code has a bug in that it always uses attachment 1 instead of by name.

	// add in muzzleflash effect
	if ( ShouldMuzzleFlash() )
	{
		DisableMuzzleFlash();
		
		ProcessMuzzleFlashEvent();
	}

#endif

	// If we're invisible, don't process animation events.
	if ( bIsInvisible )
		return;

	mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( GetSequence() );

	if (seqdesc.numevents == 0)
		return;

	// Forces anim event indices to get set and returns pEvent(0);
	mstudioevent_t *pevent = GetEventIndexForSequence( seqdesc );

	if ( watch )
	{
		Msg( "%i cycle %f\n", gpGlobals->tickcount, GetCycle() );
	}

	bool resetEvents = m_nResetEventsParity != m_nPrevResetEventsParity;
	m_nPrevResetEventsParity = m_nResetEventsParity;

	if (m_nEventSequence != GetSequence() || resetEvents )
	{
		if ( watch )
		{
			Msg( "new seq: %i - old seq: %i - reset: %s - m_flCycle %f - Model Name: %s - (time %.3f)\n",
				GetSequence(), m_nEventSequence,
				resetEvents ? "true" : "false",
				GetCycle(), pStudioHdr->pszName(),
				gpGlobals->curtime);
		}

		m_nEventSequence = GetSequence();
		flEventCycle = 0.0f;
		m_flPrevEventCycle = -0.01; // back up to get 0'th frame animations
	}

	// stalled?
	if (flEventCycle == m_flPrevEventCycle)
		return;

	if ( watch )
	{
		 Msg( "%i (seq %d cycle %.3f ) evcycle %.3f prevevcycle %.3f (time %.3f)\n",
			 gpGlobals->tickcount, 
			 GetSequence(),
			 GetCycle(),
			 flEventCycle,
			 m_flPrevEventCycle,
			 gpGlobals->curtime );
	}

	// check for looping
	BOOL bLooped = false;
	if (flEventCycle <= m_flPrevEventCycle)
	{
		if (m_flPrevEventCycle - flEventCycle > 0.5)
		{
			bLooped = true;
		}
		else
		{
			// things have backed up, which is bad since it'll probably result in a hitch in the animation playback
			// but, don't play events again for the same time slice
			return;
		}
	}

	// This makes sure events that occur at the end of a sequence occur are
	// sent before events that occur at the beginning of a sequence.
	if (bLooped)
	{
		for (int i = 0; i < (int)seqdesc.numevents; i++)
		{
			// ignore all non-client-side events

			if ( pevent[i].type & AE_TYPE_NEWEVENTSYSTEM )
			{
				if ( !( pevent[i].type & AE_TYPE_CLIENT ) )
					 continue;
			}
			else if ( pevent[i].Event_OldSystem() < EVENT_CLIENT ) //Adrian - Support the old event system
				continue;
		
			if ( pevent[i].cycle <= m_flPrevEventCycle )
				continue;
			
			if ( watch )
			{
				Msg( "%i FE %i Looped cycle %f, prev %f ev %f (time %.3f)\n",
					gpGlobals->tickcount,
					pevent[i].Event(),
					pevent[i].cycle,
					m_flPrevEventCycle,
					flEventCycle,
					gpGlobals->curtime );
			}
				
				
			FireEvent( GetAbsOrigin(), GetAbsAngles(), pevent[ i ].Event(), pevent[ i ].pszOptions() );
		}

		// Necessary to get the next loop working
		m_flPrevEventCycle = -0.01;
	}

	for (int i = 0; i < (int)seqdesc.numevents; i++)
	{
		if ( pevent[i].type & AE_TYPE_NEWEVENTSYSTEM )
		{
			if ( !( pevent[i].type & AE_TYPE_CLIENT ) )
				 continue;
		}
		else if ( pevent[i].Event_OldSystem() < EVENT_CLIENT ) //Adrian - Support the old event system
			continue;

		if ( (pevent[i].cycle > m_flPrevEventCycle && pevent[i].cycle <= flEventCycle) )
		{
			if ( watch )
			{
				Msg( "%i (seq: %d/%s) FE %i Normal cycle %f, prev %f ev %f (time %.3f) (options %s)\n",
					gpGlobals->tickcount,
					GetSequence(),
					GetSequenceActivityName( GetSequence() ),
					pevent[i].Event(),
					pevent[i].cycle,
					m_flPrevEventCycle,
					flEventCycle,
					gpGlobals->curtime,
					pevent[ i ].pszOptions() );
			}

			FireEvent( GetAbsOrigin(), GetAbsAngles(), pevent[ i ].Event(), pevent[ i ].pszOptions() );
		}
	}

	m_flPrevEventCycle = GetCycle();
}

//-----------------------------------------------------------------------------
// Purpose: Parses a muzzle effect event and sends it out for drawing
// Input  : *options - event parameters in text format
//			isFirstPerson - whether this is coming from an NPC or the player
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool C_BaseAnimating::DispatchMuzzleEffect( const char *options, bool isFirstPerson )
{
	const char	*p = options;
	char		token[128];
	int			weaponType = 0;

	// Get the first parameter
	p = nexttoken( token, p, ' ' );

	// Find the weapon type
	if ( token ) 
	{
		//TODO: Parse the type from a list instead
		if ( Q_stricmp( token, "COMBINE" ) == 0 )
		{
			weaponType = MUZZLEFLASH_COMBINE;
		}
		else if ( Q_stricmp( token, "SMG1" ) == 0 )
		{
			weaponType = MUZZLEFLASH_SMG1;
		}
		else if ( Q_stricmp( token, "PISTOL" ) == 0 )
		{
			weaponType = MUZZLEFLASH_PISTOL;
		}
		else if ( Q_stricmp( token, "SHOTGUN" ) == 0 )
		{
			weaponType = MUZZLEFLASH_SHOTGUN;
		}
		else if ( Q_stricmp( token, "357" ) == 0 )
		{
			weaponType = MUZZLEFLASH_357;
		}
		else if ( Q_stricmp( token, "RPG" ) == 0 )
		{
			weaponType = MUZZLEFLASH_RPG;
		}
		else
		{
			//NOTENOTE: This means you specified an invalid muzzleflash type, check your spelling?
			Assert( 0 );
		}
	}
	else
	{
		//NOTENOTE: This means that there wasn't a proper parameter passed into the animevent
		Assert( 0 );
		return false;
	}

	// Get the second parameter
	p = nexttoken( token, p, ' ' );

	int	attachmentIndex = -1;

	// Find the attachment name
	if ( token ) 
	{
		attachmentIndex = LookupAttachment( token );

		// Found an invalid attachment
		if ( attachmentIndex <= 0 )
		{
			//NOTENOTE: This means that the attachment you're trying to use is invalid
			Assert( 0 );
			return false;
		}
	}
	else
	{
		//NOTENOTE: This means that there wasn't a proper parameter passed into the animevent
		Assert( 0 );
		return false;
	}

	// Send it out
	tempents->MuzzleFlash( weaponType, GetRefEHandle(), attachmentIndex, isFirstPerson );

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MaterialFootstepSound( C_BaseAnimating *pEnt, bool bLeftFoot, float flVolume )
{
	trace_t tr;
	Vector traceStart;
	QAngle angles;

	int attachment;

	//!!!PERF - These string lookups here aren't the swiftest, but
	// this doesn't get called very frequently unless a lot of NPCs
	// are using this code.
	if( bLeftFoot )
	{
		attachment = pEnt->LookupAttachment( "LeftFoot" );
	}
	else
	{
		attachment = pEnt->LookupAttachment( "RightFoot" );
	}

	if( attachment == -1 )
	{
		// Exit if this NPC doesn't have the proper attachments.
		return;
	}

	pEnt->GetAttachment( attachment, traceStart, angles );

	UTIL_TraceLine( traceStart, traceStart - Vector( 0, 0, 48.0f), MASK_SHOT_HULL, pEnt, COLLISION_GROUP_NONE, &tr );
	if( tr.fraction < 1.0 && tr.m_pEnt )
	{
		surfacedata_t *psurf = physprops->GetSurfaceData( tr.surface.surfaceProps );
		if( psurf )
		{
			EmitSound_t params;
			if( bLeftFoot )
			{
				params.m_pSoundName = physprops->GetString(psurf->sounds.runStepLeft);
			}
			else
			{
				params.m_pSoundName = physprops->GetString(psurf->sounds.runStepRight);
			}

			CPASAttenuationFilter filter( pEnt, params.m_pSoundName );

			params.m_bWarnOnDirectWaveReference = true;
			params.m_flVolume = flVolume;

			pEnt->EmitSound( filter, pEnt->entindex(), params );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Creates a particle effect and ejects a single shell. If
// the effect is already active, it just restarts it to eject another shell.
//-----------------------------------------------------------------------------
void C_BaseAnimating::EjectParticleBrass( const char *pEffectName, const int iAttachment )
{
	if ( cl_ejectbrass.GetBool() == false )
		return;

	// TODO: Can we change the attachment for an active particle system?
	if ( !m_ejectBrassEffect || m_iEjectBrassAttachment != iAttachment )
	{
		m_iEjectBrassAttachment = iAttachment;
		m_ejectBrassEffect = ParticleProp()->Create( pEffectName, PATTACH_POINT_FOLLOW, iAttachment );
	}
	else
	{
		m_ejectBrassEffect->Restart();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *origin - 
//			*angles - 
//			event - 
//			*options - 
//			numAttachments - 
//			attachments[] - 
//-----------------------------------------------------------------------------
void C_BaseAnimating::FireEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	Vector attachOrigin;
	QAngle attachAngles; 

	switch( event )
	{
	case AE_CL_CREATE_PARTICLE_EFFECT:
		{
			int iAttachment = -1;
			int iAttachType = PATTACH_ABSORIGIN_FOLLOW;
			int iAttachmentCP1 = -1;
			int iAttachTypeCP1 = PATTACH_ABSORIGIN_FOLLOW;
			char token[256];
			char szParticleEffect[256];

			// Get the particle effect name
			const char *p = options;
			p = nexttoken(token, p, ' ');
			if ( token ) 
			{
				const char* mtoken = ModifyEventParticles( token );
				Q_strncpy( szParticleEffect, mtoken, sizeof(szParticleEffect) );
			}

			// Get the attachment type
			p = nexttoken(token, p, ' ');
			if ( token ) 
			{
				iAttachType = GetAttachTypeFromString( token );
				if ( iAttachType == -1 )
				{
					Warning("Invalid attach type specified for particle effect anim event. Trying to spawn effect '%s' with attach type of '%s'\n", szParticleEffect, token );
					return;
				}
			}

			// Get the attachment point index
			p = nexttoken(token, p, ' ');
			if ( token )
			{
				iAttachment = atoi(token);

				// See if we can find any attachment points matching the name
				if ( token[0] != '0' && iAttachment == 0 )
				{
					iAttachment = LookupAttachment( token );
					if ( iAttachment == -1 )
					{
						Warning("Failed to find attachment point specified for particle effect anim event. Trying to spawn effect '%s' on attachment named '%s'\n", szParticleEffect, token );
						return;
					}
				}
			}

			// Spawn the particle effect
			CNewParticleEffect *pEffect = ParticleProp()->Create( szParticleEffect, (ParticleAttachment_t)iAttachType, iAttachment );

			// Get the attachment type for CP1
			p = nexttoken(token, p, ' ');
			if ( !p )
				return;
			if ( token ) 
			{
				iAttachTypeCP1 = GetAttachTypeFromString( token );
				if ( iAttachTypeCP1 == -1 )
				{
					Warning("Invalid attach type specified for particle effect anim event. Trying to spawn effect '%s' with attach type of '%s'\n", szParticleEffect, token );
					return;
				}
			}

			// Get the attachment point index
			p = nexttoken(token, p, ' ');
			if ( token )
			{
				iAttachmentCP1 = atoi(token);

				// See if we can find any attachment points matching the name
				if ( token[0] != '0' && iAttachmentCP1 == 0 )
				{
					iAttachmentCP1 = LookupAttachment( token );
					if ( iAttachmentCP1 == -1 )
					{
						Warning("Failed to find attachment point specified for particle effect anim event. Trying to spawn effect '%s' on attachment named '%s'\n", szParticleEffect, token );
						return;
					}
				}
				
				ParticleProp()->AddControlPoint( pEffect, 1, this, (ParticleAttachment_t)iAttachTypeCP1, token );
			}
		}
		break;
	case AE_CL_STOP_PARTICLE_EFFECT:
		{
			char token[256];
			char szParticleEffect[256];

			// Get the particle effect name
			const char *p = options;
			p = nexttoken(token, p, ' ');
			if ( token ) 
			{
				Q_strncpy( szParticleEffect, token, sizeof(szParticleEffect) );
			}

			// Get the attachment point index
			p = nexttoken(token, p, ' ');
			bool bStopInstantly = ( token && !Q_stricmp( token, "instantly" ) );

			ParticleProp()->StopParticlesNamed( szParticleEffect, bStopInstantly );
		}
		break;
	
	case AE_CL_ADD_PARTICLE_EFFECT_CP:
		{
			int iControlPoint = 1;
			int iAttachment = -1;
			int iAttachType = PATTACH_ABSORIGIN_FOLLOW;
			int iEffectIndex = -1;
			char token[256];
			char szParticleEffect[256];

			// Get the particle effect name
			const char *p = options;
			p = nexttoken(token, p, ' ');
			if ( token ) 
			{
				Q_strncpy( szParticleEffect, token, sizeof(szParticleEffect) );
			}

			// Get the control point number
			p = nexttoken(token, p, ' ');
			if ( token ) 
			{
				iControlPoint = atoi( token );
			}

			// Get the attachment type
			p = nexttoken(token, p, ' ');
			if ( token ) 
			{
				iAttachType = GetAttachTypeFromString( token );
				if ( iAttachType == -1 )
				{
					Warning("Invalid attach type specified for particle effect anim event. Trying to spawn effect '%s' with attach type of '%s'\n", szParticleEffect, token );
					return;
				}
			}

			// Get the attachment point index
			p = nexttoken(token, p, ' ');
			if ( token )
			{
				iAttachment = atoi(token);

				// See if we can find any attachment points matching the name
				if ( token[0] != '0' && iAttachment == 0 )
				{
					iAttachment = LookupAttachment( token );
					if ( iAttachment == -1 )
					{
						Warning("Failed to find attachment point specified for particle effect anim event. Trying to spawn effect '%s' on attachment named '%s'\n", szParticleEffect, token );
						return;
					}
				}
			}
			iEffectIndex = ParticleProp()->FindEffect( szParticleEffect );
			if ( iEffectIndex == -1 )
			{
				Warning("Failed to find specified particle effect. Trying to add CP to '%s' on attachment named '%s'\n", szParticleEffect, token );
				return;
			}
			ParticleProp()->AddControlPoint( iEffectIndex, iControlPoint, this, (ParticleAttachment_t)iAttachType, iAttachment );	
		}
		break;

	case AE_CL_PLAYSOUND:
		{
			if ( m_bSuppressAnimSounds )
				return;
			
			CLocalPlayerFilter filter;

			if ( m_Attachments.Count() > 0)
			{
				GetAttachment( 1, attachOrigin, attachAngles );
				EmitSound( filter, GetSoundSourceIndex(), options, &attachOrigin );
			}
			else
			{
				EmitSound( filter, GetSoundSourceIndex(), options, &GetAbsOrigin() );
			} 
		}
		break;
	case AE_CL_STOPSOUND:
		{
			StopSound( GetSoundSourceIndex(), options );
		}
		break;

	case CL_EVENT_FOOTSTEP_LEFT:
		{
			char pSoundName[256];
			if ( !options || !options[0] )
			{
				options = "NPC_CombineS";
			}

			Vector vel;
			EstimateAbsVelocity( vel );

			// If he's moving fast enough, play the run sound
			if ( vel.Length2DSqr() > RUN_SPEED_ESTIMATE_SQR )
			{
				Q_snprintf( pSoundName, 256, "%s.RunFootstepLeft", options );
			}
			else
			{
				Q_snprintf( pSoundName, 256, "%s.FootstepLeft", options );
			}
			EmitSound( pSoundName );
		}
		break;

	case CL_EVENT_FOOTSTEP_RIGHT:
		{
			char pSoundName[256];
			if ( !options || !options[0] )
			{
				options = "NPC_CombineS";
			}

			Vector vel;
			EstimateAbsVelocity( vel );
			// If he's moving fast enough, play the run sound
			if ( vel.Length2DSqr() > RUN_SPEED_ESTIMATE_SQR )
			{
				Q_snprintf( pSoundName, 256, "%s.RunFootstepRight", options );
			}
			else
			{
				Q_snprintf( pSoundName, 256, "%s.FootstepRight", options );
			}
			EmitSound( pSoundName );
		}
		break;

	case CL_EVENT_MFOOTSTEP_LEFT:
		{
			MaterialFootstepSound( this, true, VOL_NORM * 0.5f );
		}
		break;

	case CL_EVENT_MFOOTSTEP_RIGHT:
		{
			MaterialFootstepSound( this, false, VOL_NORM * 0.5f );
		}
		break;

	case CL_EVENT_MFOOTSTEP_LEFT_LOUD:
		{
			MaterialFootstepSound( this, true, VOL_NORM );
		}
		break;

	case CL_EVENT_MFOOTSTEP_RIGHT_LOUD:
		{
			MaterialFootstepSound( this, false, VOL_NORM );
		}
		break;

	// Eject brass
	case CL_EVENT_EJECTBRASS1:
		if ( m_Attachments.Count() > 0 )
		{
			DevWarning( "Unhandled eject brass animevent\n" );
		}
		break;

	case AE_MUZZLEFLASH:
		{
			// Send out the effect for a player
			DispatchMuzzleEffect( options, true );
			break;
		}

	case AE_NPC_MUZZLEFLASH:
		{
			// Send out the effect for an NPC
			DispatchMuzzleEffect( options, false );
			break;
		}

	// OBSOLETE EVENTS. REPLACED BY NEWER SYSTEMS.
	// See below in FireObsoleteEvent() for comments on what to use instead.
	case AE_CLIENT_EFFECT_ATTACH:
	case CL_EVENT_DISPATCHEFFECT0:
	case CL_EVENT_DISPATCHEFFECT1:
	case CL_EVENT_DISPATCHEFFECT2:
	case CL_EVENT_DISPATCHEFFECT3:
	case CL_EVENT_DISPATCHEFFECT4:
	case CL_EVENT_DISPATCHEFFECT5:
	case CL_EVENT_DISPATCHEFFECT6:
	case CL_EVENT_DISPATCHEFFECT7:
	case CL_EVENT_DISPATCHEFFECT8:
	case CL_EVENT_DISPATCHEFFECT9:
	case CL_EVENT_MUZZLEFLASH0:
	case CL_EVENT_MUZZLEFLASH1:
	case CL_EVENT_MUZZLEFLASH2:
	case CL_EVENT_MUZZLEFLASH3:
	case CL_EVENT_NPC_MUZZLEFLASH0:
	case CL_EVENT_NPC_MUZZLEFLASH1:
	case CL_EVENT_NPC_MUZZLEFLASH2:
	case CL_EVENT_NPC_MUZZLEFLASH3:
	case CL_EVENT_SPARK0:
	case CL_EVENT_SOUND:
		FireObsoleteEvent( origin, angles, event, options );
		break;

	case AE_CL_ENABLE_BODYGROUP:
		{
			int index = FindBodygroupByName( options );
			if ( index >= 0 )
			{
				SetBodygroup( index, 1 );
			}
		}
		break;

	case AE_CL_DISABLE_BODYGROUP:
		{
			int index = FindBodygroupByName( options );
			if ( index >= 0 )
			{
				SetBodygroup( index, 0 );
			}
		}
		break;

	case AE_CL_BODYGROUP_SET_VALUE:
		{
			char szBodygroupName[256];
			int value = 0;

			char token[256];

			const char *p = options;

			// Bodygroup Name
			p = nexttoken(token, p, ' ');
			if ( token ) 
			{
				Q_strncpy( szBodygroupName, token, sizeof(szBodygroupName) );
			}

			// Get the desired value
			p = nexttoken(token, p, ' ');
			if ( token ) 
			{
				value = atoi( token );
			}

			int index = FindBodygroupByName( szBodygroupName );
			if ( index >= 0 )
			{
				SetBodygroup( index, value );
			}
		}
		break;

// 	case AE_BEGIN_TAUNT_LOOP:
// 		{
// 			if ( IsViewModel() )
// 			{
// 				C_BaseViewModel *pViewModel = assert_cast< C_BaseViewModel* >( this );
// 				C_BaseCombatWeapon *pWeapon = pViewModel->GetOwningWeapon();
// 
// 				if ( pWeapon )
// 				{
// 					C_BasePlayer *pPlayer = dynamic_cast< C_BasePlayer* >( pWeapon->GetOwner() );
// 					if ( pPlayer && pPlayer->IsHoldingTaunt() )
// 					{
// 						float flCycle = GetCycle();
// 						pViewModel->SetCycle( V_atof( options ) );
// 						pViewModel->m_fCycleOffset += ( GetCycle() - flCycle );
// 					}
// 				}
// 			}
// 		}
// 		break;

	case AE_BEGIN_TAUNT_LOOP:
		{
			if ( IsViewModel() )
			{
				C_BaseViewModel *pViewModel = assert_cast< C_BaseViewModel* >( this );
				C_BaseCombatWeapon *pWeapon = pViewModel->GetOwningWeapon();

				if ( pWeapon )
				{
					C_BasePlayer *pPlayer = dynamic_cast< C_BasePlayer* >( pWeapon->GetOwner() );
					if ( pPlayer && pPlayer->IsHoldingLookAtWeapon() )
					{
						float flCycle = GetCycle();
						pViewModel->SetCycle( V_atof( options ) );
						pViewModel->m_fCycleOffset += ( GetCycle() - flCycle );
					}
				}
			}
		}
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: These events are all obsolete events, left here to support old games.
//			Their systems have all been replaced with better ones.
//-----------------------------------------------------------------------------
void C_BaseAnimating::FireObsoleteEvent( const Vector& origin, const QAngle& angles, int event, const char *options )
{
	Vector attachOrigin;
	QAngle attachAngles; 

	switch( event )
	{
	// Obsolete. Use the AE_CL_CREATE_PARTICLE_EFFECT event instead, which uses the artist driven particle system & editor.
	case AE_CLIENT_EFFECT_ATTACH:
		{
			int iAttachment = -1;
			int iParam = 0;
			char token[128];
			char effectFunc[128];

			const char *p = options;

			p = nexttoken(token, p, ' ');

			if( token ) 
			{
				Q_strncpy( effectFunc, token, sizeof(effectFunc) );
			}

			p = nexttoken(token, p, ' ');

			if( token )
			{
				if ( V_isdigit( *token ) )
				{
					iAttachment = atoi(token);
				}
				else
				{
					iAttachment = LookupAttachment( token );
				}
			}

			p = nexttoken(token, p, ' ');

			if( token )
			{
				iParam = atoi(token);
			}

			if ( iAttachment != -1 && m_Attachments.Count() >= iAttachment )
			{
				GetAttachment( iAttachment, attachOrigin, attachAngles );

				// Fill out the generic data
				CEffectData data;
				data.m_vOrigin = attachOrigin;
				data.m_vAngles = attachAngles;
				AngleVectors( attachAngles, &data.m_vNormal );
				data.m_hEntity = GetRefEHandle();
				data.m_nAttachmentIndex = iAttachment + 1;
				data.m_fFlags = iParam;

				DispatchEffect( effectFunc, data );
			}
		}
		break;

	// Obsolete. Use the AE_CL_CREATE_PARTICLE_EFFECT event instead, which uses the artist driven particle system & editor.
	case CL_EVENT_DISPATCHEFFECT0:
	case CL_EVENT_DISPATCHEFFECT1:
	case CL_EVENT_DISPATCHEFFECT2:
	case CL_EVENT_DISPATCHEFFECT3:
	case CL_EVENT_DISPATCHEFFECT4:
	case CL_EVENT_DISPATCHEFFECT5:
	case CL_EVENT_DISPATCHEFFECT6:
	case CL_EVENT_DISPATCHEFFECT7:
	case CL_EVENT_DISPATCHEFFECT8:
	case CL_EVENT_DISPATCHEFFECT9:
		{
			int iAttachment = -1;

			// First person muzzle flashes
			switch (event) 
			{
			case CL_EVENT_DISPATCHEFFECT0:
				iAttachment = 0;
				break;

			case CL_EVENT_DISPATCHEFFECT1:
				iAttachment = 1;
				break;

			case CL_EVENT_DISPATCHEFFECT2:
				iAttachment = 2;
				break;

			case CL_EVENT_DISPATCHEFFECT3:
				iAttachment = 3;
				break;

			case CL_EVENT_DISPATCHEFFECT4:
				iAttachment = 4;
				break;

			case CL_EVENT_DISPATCHEFFECT5:
				iAttachment = 5;
				break;

			case CL_EVENT_DISPATCHEFFECT6:
				iAttachment = 6;
				break;

			case CL_EVENT_DISPATCHEFFECT7:
				iAttachment = 7;
				break;

			case CL_EVENT_DISPATCHEFFECT8:
				iAttachment = 8;
				break;

			case CL_EVENT_DISPATCHEFFECT9:
				iAttachment = 9;
				break;
			}

			if ( iAttachment != -1 && m_Attachments.Count() > iAttachment )
			{
				GetAttachment( iAttachment+1, attachOrigin, attachAngles );

				// Fill out the generic data
				CEffectData data;
				data.m_vOrigin = attachOrigin;
				data.m_vAngles = attachAngles;
				AngleVectors( attachAngles, &data.m_vNormal );
				data.m_hEntity = GetRefEHandle();
				data.m_nAttachmentIndex = iAttachment + 1;

				DispatchEffect( options, data );
			}
		}
		break;

	// Obsolete. Use the AE_MUZZLEFLASH / AE_NPC_MUZZLEFLASH events instead.
	case CL_EVENT_MUZZLEFLASH0:
	case CL_EVENT_MUZZLEFLASH1:
	case CL_EVENT_MUZZLEFLASH2:
	case CL_EVENT_MUZZLEFLASH3:
	case CL_EVENT_NPC_MUZZLEFLASH0:
	case CL_EVENT_NPC_MUZZLEFLASH1:
	case CL_EVENT_NPC_MUZZLEFLASH2:
	case CL_EVENT_NPC_MUZZLEFLASH3:
		{
			int iAttachment = -1;
			bool bFirstPerson = true;

			// First person muzzle flashes
			switch (event) 
			{
			case CL_EVENT_MUZZLEFLASH0:
				iAttachment = 0;
				break;

			case CL_EVENT_MUZZLEFLASH1:
				iAttachment = 1;
				break;

			case CL_EVENT_MUZZLEFLASH2:
				iAttachment = 2;
				break;

			case CL_EVENT_MUZZLEFLASH3:
				iAttachment = 3;
				break;

				// Third person muzzle flashes
			case CL_EVENT_NPC_MUZZLEFLASH0:
				iAttachment = 0;
				bFirstPerson = false;
				break;

			case CL_EVENT_NPC_MUZZLEFLASH1:
				iAttachment = 1;
				bFirstPerson = false;
				break;

			case CL_EVENT_NPC_MUZZLEFLASH2:
				iAttachment = 2;
				bFirstPerson = false;
				break;

			case CL_EVENT_NPC_MUZZLEFLASH3:
				iAttachment = 3;
				bFirstPerson = false;
				break;
			}

			if ( iAttachment != -1 && m_Attachments.Count() > iAttachment )
			{
				GetAttachment( iAttachment+1, attachOrigin, attachAngles );
				int entId = render->GetViewEntity();
				ClientEntityHandle_t hEntity = ClientEntityList().EntIndexToHandle( entId );
				tempents->MuzzleFlash( attachOrigin, attachAngles, atoi( options ), hEntity, bFirstPerson );
			}
		}
		break;

	// Obsolete: Use the AE_CL_CREATE_PARTICLE_EFFECT event instead, which uses the artist driven particle system & editor.
	case CL_EVENT_SPARK0:
		{
			Vector vecForward;
			GetAttachment( 1, attachOrigin, attachAngles );
			AngleVectors( attachAngles, &vecForward );
			g_pEffects->Sparks( attachOrigin, atoi( options ), 1, &vecForward );
		}
		break;

	// Obsolete: Use the AE_CL_PLAYSOUND event instead, which doesn't rely on a magic number in the .qc
	case CL_EVENT_SOUND:
		{
			CLocalPlayerFilter filter;

			if ( m_Attachments.Count() > 0)
			{
				GetAttachment( 1, attachOrigin, attachAngles );
				EmitSound( filter, GetSoundSourceIndex(), options, &attachOrigin );
			}
			else
			{
				EmitSound( filter, GetSoundSourceIndex(), options );
			}
		}
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool C_BaseAnimating::IsSelfAnimating()
{
	if ( m_bClientSideAnimation )
		return true;

	// Yes, we use animtime.
	int iMoveType = GetMoveType();
	if ( iMoveType != MOVETYPE_STEP && 
		  iMoveType != MOVETYPE_NONE && 
		  iMoveType != MOVETYPE_WALK &&
		  iMoveType != MOVETYPE_FLY &&
		  iMoveType != MOVETYPE_FLYGRAVITY )
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Called by networking code when an entity is new to the PVS or comes down with the EF_NOINTERP flag set.
//  The position history data is flushed out right after this call, so we need to store off the current data
//  in the latched fields so we try to interpolate
// Input  : *ent - 
//			full_reset - 
//-----------------------------------------------------------------------------
void C_BaseAnimating::ResetLatched( void )
{
	// Reset the IK
	if ( m_pIk )
	{
		delete m_pIk;
		m_pIk = NULL;
	}

	BaseClass::ResetLatched();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------

bool C_BaseAnimating::Interpolate( float flCurrentTime )
{
	// ragdolls don't need interpolation
	if ( m_pRagdoll )
		return true;

	VPROF( "C_BaseAnimating::Interpolate" );

	Vector oldOrigin;
	QAngle oldAngles;
	float flOldCycle = GetCycle();
	int nChangeFlags = 0;

	if ( !m_bClientSideAnimation )
		m_iv_flCycle.SetLooping( IsSequenceLooping( GetSequence() ) );

	int bNoMoreChanges;
	int retVal = BaseInterpolatePart1( flCurrentTime, oldOrigin, oldAngles, bNoMoreChanges );
	if ( retVal == INTERPOLATE_STOP )
	{
		if ( bNoMoreChanges )
			RemoveFromEntityList(ENTITY_LIST_INTERPOLATE);
		return true;
	}


	// Did cycle change?
	if( GetCycle() != flOldCycle )
		nChangeFlags |= ANIMATION_CHANGED;

	if ( bNoMoreChanges )
		RemoveFromEntityList(ENTITY_LIST_INTERPOLATE);
	
	BaseInterpolatePart2( oldOrigin, oldAngles, nChangeFlags );
	return true;
}


//-----------------------------------------------------------------------------
// returns true if we're currently being ragdolled
//-----------------------------------------------------------------------------
bool C_BaseAnimating::IsRagdoll() const
{
	return m_pRagdoll && m_bClientSideRagdoll;
}

Vector C_BaseAnimating::GetThirdPersonViewPosition( void )
{
	return GetRenderOrigin() + GetViewOffset();
}


//-----------------------------------------------------------------------------
// implements these so ragdolls can handle frustum culling & leaf visibility
//-----------------------------------------------------------------------------

void C_BaseAnimating::GetRenderBounds( Vector& theMins, Vector& theMaxs )
{
	if ( IsRagdoll() )
	{
		m_pRagdoll->GetRagdollBounds( theMins, theMaxs );
		Vector vecBloat( 5.0f, 5.0f, 5.0f );
		theMins -= vecBloat;
		theMaxs += vecBloat;
	}
	else if ( GetModel() )
	{
		MDLCACHE_CRITICAL_SECTION();

		CStudioHdr *pStudioHdr = GetModelPtr();
		if ( !pStudioHdr|| !pStudioHdr->SequencesAvailable() || GetSequence() == -1 )
		{
			theMins = vec3_origin;
			theMaxs = vec3_origin;
			return;
		} 
		if (!VectorCompare( vec3_origin, pStudioHdr->view_bbmin() ) || !VectorCompare( vec3_origin, pStudioHdr->view_bbmax() ))
		{
			// clipping bounding box
			VectorCopy ( pStudioHdr->view_bbmin(), theMins);
			VectorCopy ( pStudioHdr->view_bbmax(), theMaxs);
		}
		else
		{
			// movement bounding box
			VectorCopy ( pStudioHdr->hull_min(), theMins);
			VectorCopy ( pStudioHdr->hull_max(), theMaxs);
		}

		mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( GetSequence() );
		VectorMin( seqdesc.bbmin, theMins, theMins );
		VectorMax( seqdesc.bbmax, theMaxs, theMaxs );
	}
	else
	{
		theMins = vec3_origin;
		theMaxs = vec3_origin;
	}

	// Scale this up depending on if our model is currently scaling
	const float flScale = GetModelHierarchyScale();
	theMaxs *= flScale;
	theMins *= flScale;
}


//-----------------------------------------------------------------------------
// implements these so ragdolls can handle frustum culling & leaf visibility
//-----------------------------------------------------------------------------
const Vector& C_BaseAnimating::GetRenderOrigin( void )
{
#ifdef DEMOPOLISH_ENABLED
	if ( DemoPolish_ShouldReplaceRoot( entindex() ) )
	{
		return DemoPolish_GetController().GetRenderOrigin( entindex() );
	}
	else
#endif
	if ( IsRagdoll() )
	{
		return m_pRagdoll->GetRagdollOrigin();
	}

	if ( m_vecRenderOriginOverride != vec3_invalid )
	{
		return m_vecRenderOriginOverride;
	}

	return BaseClass::GetRenderOrigin();	
}


const QAngle& C_BaseAnimating::GetRenderAngles( void )
{
#ifdef DEMOPOLISH_ENABLED
	if ( DemoPolish_ShouldReplaceRoot( entindex() ) )
	{
		return DemoPolish_GetController().GetRenderAngles( entindex() );
	}
	else
#endif
	if ( IsRagdoll() )
	{
		return vec3_angle;
	}

	return BaseClass::GetRenderAngles();	
}

void C_BaseAnimating::RagdollMoved( void ) 
{
	SetAbsOrigin( m_pRagdoll->GetRagdollOrigin() );
	SetAbsAngles( vec3_angle );

	Vector mins, maxs;
	m_pRagdoll->GetRagdollBounds( mins, maxs );
	SetCollisionBounds( mins, maxs );

	// If the ragdoll moves, its render-to-texture shadow is dirty
	InvalidatePhysicsRecursive( BOUNDS_CHANGED ); 
}


//-----------------------------------------------------------------------------
// Purpose: My physics object has been updated, react or extract data
//-----------------------------------------------------------------------------
void C_BaseAnimating::VPhysicsUpdate( IPhysicsObject *pPhysics )
{
	// FIXME: Should make sure the physics objects being passed in
	// is the ragdoll physics object, but I think it's pretty safe not to check
	if (IsRagdoll())
	{	 
		m_pRagdoll->VPhysicsUpdate( pPhysics );
		RagdollMoved();

		return;
	}

	BaseClass::VPhysicsUpdate( pPhysics );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_BaseAnimating::PreDataUpdate( DataUpdateType_t updateType )
{
	m_flOldCycle = GetCycle();
	m_nOldSequence = GetSequence();
	m_flOldModelScale = GetModelHierarchyScale();

	int i;
	for ( i=0;i<MAXSTUDIOBONECTRLS;i++ )
	{
		m_flOldEncodedController[i] = m_flEncodedController[i];
	}

	for ( i=0;i<MAXSTUDIOPOSEPARAM;i++ )
	{
		 m_flOldPoseParameters[i] = m_flPoseParameter[i];
	}

	BaseClass::PreDataUpdate( updateType );
}

void C_BaseAnimating::NotifyShouldTransmit( ShouldTransmitState_t state )
{
	BaseClass::NotifyShouldTransmit( state );

	if ( state == SHOULDTRANSMIT_START )
	{
		// If he's been firing a bunch, then he comes back into the PVS, his muzzle flash
		// will show up even if he isn't firing now.
		DisableMuzzleFlash();

		m_nPrevResetEventsParity = m_nResetEventsParity;
		m_nEventSequence = GetSequence();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : updateType - 
//-----------------------------------------------------------------------------
void C_BaseAnimating::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	if ( m_bClientSideAnimation )
	{
		SetCycle( m_flOldCycle );
		AddToClientSideAnimationList();
	}
	else
	{
		RemoveFromClientSideAnimationList();
	}

	bool bBoneControllersChanged = false;

	int i;
	for ( i=0;i<MAXSTUDIOBONECTRLS && !bBoneControllersChanged;i++ )
	{
		if ( m_flOldEncodedController[i] != m_flEncodedController[i] )
		{
			bBoneControllersChanged = true;
		}
	}

	bool bPoseParametersChanged = false;

	for ( i=0;i<MAXSTUDIOPOSEPARAM && !bPoseParametersChanged;i++ )
	{
		if ( m_flOldPoseParameters[i] != m_flPoseParameter[i] )
		{
			bPoseParametersChanged = true;
		}
	}

	// Cycle change? Then re-render
	bool bAnimationChanged = m_flOldCycle != GetCycle() || bBoneControllersChanged || bPoseParametersChanged;
	bool bSequenceChanged = m_nOldSequence != GetSequence();
	bool bScaleChanged = ( m_flOldModelScale != GetModelHierarchyScale() );
	if ( bAnimationChanged || bSequenceChanged || bScaleChanged )
	{
		int nFlags = bAnimationChanged ? ANIMATION_CHANGED : 0;
		if ( bSequenceChanged )
		{
			nFlags |= BOUNDS_CHANGED | SEQUENCE_CHANGED;
		}
		if ( bScaleChanged )
		{
			nFlags |= BOUNDS_CHANGED;
		}
		InvalidatePhysicsRecursive( nFlags );

		if ( IsViewModel() )
		{
			C_BaseViewModel *pViewModel = assert_cast< C_BaseViewModel* >( this );
			pViewModel->m_fCycleOffset = 0.0f;
		}
	}

	if ( bAnimationChanged || bSequenceChanged )
	{
		if ( m_bClientSideAnimation )
		{
			ClientSideAnimationChanged();
		}
	}

	// reset prev cycle if new sequence
	if (m_nNewSequenceParity != m_nPrevNewSequenceParity)
	{
		// It's important not to call Reset() on a static prop, because if we call
		// Reset(), then the entity will stay in the interpolated entities list
		// forever, wasting CPU.
		MDLCACHE_CRITICAL_SECTION();
		CStudioHdr *hdr = GetModelPtr();
		if ( hdr && !( hdr->flags() & STUDIOHDR_FLAGS_STATIC_PROP ) )
		{
			m_iv_flCycle.Reset( gpGlobals->curtime );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void C_BaseAnimating::OnPreDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnPreDataChanged( updateType );

	m_nPrevBody = GetBody();
	m_nPrevSkin = GetSkin();
	m_bLastClientSideFrameReset = m_bClientSideFrameReset;
}

void C_BaseAnimating::ForceSetupBonesAtTime( matrix3x4a_t *pBonesOut, float flTime )
{
	// blow the cached prev bones
	InvalidateBoneCache();

	// reset root position to flTime
	Interpolate( flTime );

	if ( m_bClientSideAnimation )
	{
		float saveCycle = GetCycle();
		float oldCycle = m_prevClientCycle;
		if ( oldCycle > saveCycle )
		{
			oldCycle -= 1.0f;
		}
		float cycleInterp = RemapVal( flTime, m_prevClientAnimTime, m_flAnimTime, oldCycle, saveCycle );
		cycleInterp = clamp( cycleInterp, 0.0f, 1.0f );
		SetCycle( cycleInterp );
		// Setup bone state at the given time
		SetupBones( pBonesOut, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, flTime );
		SetCycle( saveCycle );
	}
	else
	{
		// Setup bone state at the given time
		SetupBones( pBonesOut, MAXSTUDIOBONES, BONE_USED_BY_ANYTHING, flTime );
	}
}

void C_BaseAnimating::GetRagdollInitBoneArrays( matrix3x4a_t *pDeltaBones0, matrix3x4a_t *pDeltaBones1, matrix3x4a_t *pCurrentBones, float boneDt )
{
	ForceSetupBonesAtTime( pDeltaBones0, gpGlobals->curtime - boneDt );
	ForceSetupBonesAtTime( pDeltaBones1, gpGlobals->curtime );
	float ragdollCreateTime = PhysGetSyncCreateTime();
	if ( ragdollCreateTime != gpGlobals->curtime )
	{
		// The next simulation frame begins before the end of this frame
		// so initialize the ragdoll at that time so that it will reach the current
		// position at curtime.  Otherwise the ragdoll will simulate forward from curtime
		// and pop into the future a bit at this point of transition
		ForceSetupBonesAtTime( pCurrentBones, ragdollCreateTime );
	}
	else
	{
		Plat_FastMemcpy( pCurrentBones, m_CachedBoneData.Base(), sizeof( matrix3x4a_t ) * m_CachedBoneData.Count() );
	}
}

C_ClientRagdoll *C_BaseAnimating::CreateClientRagdoll( bool bRestoring )
{
	DevMsg( "Creating ragdoll at tick %d\n", gpGlobals->tickcount );
	return new C_ClientRagdoll( bRestoring );
}

C_BaseAnimating *C_BaseAnimating::CreateRagdollCopy()
{
	//Adrian: We now create a separate entity that becomes this entity's ragdoll.
	//That way the server side version of this entity can go away. 
	//Plus we can hook save/restore code to these ragdolls so they don't fall on restore anymore.
	C_ClientRagdoll *pRagdoll = CreateClientRagdoll( false );
	if ( pRagdoll == NULL )
		return NULL;

	TermRopes();

	const model_t *model = GetModel();
	const char *pModelName = modelinfo->GetModelName( model );

	if ( pRagdoll->InitializeAsClientEntity( pModelName, false ) == false )
	{
		pRagdoll->Release();
		return NULL;
	}

	// move my current model instance to the ragdoll's so decals are preserved.
	SnatchModelInstance( pRagdoll );

	// We need to take these from the entity
	pRagdoll->SetAbsOrigin( GetAbsOrigin() );
	pRagdoll->SetAbsAngles( GetAbsAngles() );

	pRagdoll->IgniteRagdoll( this );
	pRagdoll->TransferDissolveFrom( this );
	pRagdoll->InitModelEffects();

	if ( AddRagdollToFadeQueue() == true )
	{
		pRagdoll->m_bImportant = NPC_IsImportantNPC( this );
		s_RagdollLRU.MoveToTopOfLRU( pRagdoll, pRagdoll->m_bImportant );
		pRagdoll->m_bFadeOut = true;
	}

	m_builtRagdoll = true;
	AddEffects( EF_NODRAW );

	if ( IsEffectActive( EF_NOSHADOW ) )
	{
		pRagdoll->AddEffects( EF_NOSHADOW );
	}

	pRagdoll->m_bClientSideRagdoll = true;
	pRagdoll->SetRenderMode( GetRenderMode() );
	pRagdoll->SetRenderColor( GetRenderColor().r, GetRenderColor().g, GetRenderColor().b );
	pRagdoll->SetRenderAlpha( GetRenderAlpha() );
	pRagdoll->SetGlobalFadeScale( GetGlobalFadeScale() );

	pRagdoll->SetBody( GetBody() );
	pRagdoll->SetSkin( GetSkin() );
	pRagdoll->m_vecForce = m_vecForce;
	pRagdoll->m_nForceBone = m_nForceBone;
	pRagdoll->SetNextClientThink( CLIENT_THINK_ALWAYS );

	pRagdoll->SetModelName( AllocPooledString(pModelName) );
	pRagdoll->CopySequenceTransitions(this);
	pRagdoll->SetModelScale( this->GetModelScale(), this->GetModelScaleType() );
	return pRagdoll;
}

void C_BaseAnimating::CopySequenceTransitions( C_BaseAnimating *pCopyFrom )
{
	m_SequenceTransitioner.m_animationQueue.RemoveAll();
	int count = pCopyFrom->m_SequenceTransitioner.m_animationQueue.Count();
	m_SequenceTransitioner.m_animationQueue.EnsureCount(count);
	for ( int i = 0; i < count; i++ )
	{
		m_SequenceTransitioner.m_animationQueue[i] = pCopyFrom->m_SequenceTransitioner.m_animationQueue[i];
	}
}


C_BaseAnimating *C_BaseAnimating::BecomeRagdollOnClient()
{
	MoveToLastReceivedPosition( true );
	GetAbsOrigin();
	m_pClientsideRagdoll = CreateRagdollCopy();
	if ( !m_pClientsideRagdoll )
		return NULL;

	matrix3x4a_t boneDelta0[MAXSTUDIOBONES];
	matrix3x4a_t boneDelta1[MAXSTUDIOBONES];
	matrix3x4a_t currentBones[MAXSTUDIOBONES];
	const float boneDt = 0.1f;
	GetRagdollInitBoneArrays( boneDelta0, boneDelta1, currentBones, boneDt );
	m_pClientsideRagdoll->InitAsClientRagdoll( boneDelta0, boneDelta1, currentBones, boneDt );
	return m_pClientsideRagdoll;
}

bool C_BaseAnimating::InitAsClientRagdoll( const matrix3x4_t *pDeltaBones0, const matrix3x4_t *pDeltaBones1, const matrix3x4_t *pCurrentBonePosition, float boneDt, Vector vecForceOverride, bool bleedOut )
{
	CStudioHdr *hdr = GetModelPtr();
	if ( !hdr || m_pRagdoll || m_builtRagdoll )
		return false;

	m_builtRagdoll = true;

	// Store off our old mins & maxs
	m_vecPreRagdollMins = WorldAlignMins();
	m_vecPreRagdollMaxs = WorldAlignMaxs();


	// Force MOVETYPE_STEP interpolation
	SetMoveType( MOVETYPE_STEP );

	// HACKHACK: force time to last interpolation position
	m_flPlaybackRate = 1;
	
	m_pRagdoll = CreateRagdoll( this, hdr, vecForceOverride, m_nForceBone, pDeltaBones0, pDeltaBones1, pCurrentBonePosition, boneDt, false, bleedOut );

	// Cause the entity to recompute its shadow	type and make a
	// version which only updates when physics state changes
	// NOTE: We have to do this after m_pRagdoll is assigned above
	// because that's what ShadowCastType uses to figure out which type of shadow to use.
	DestroyShadow();
	CreateShadow();

	// Cache off ragdoll bone positions/quaternions
	if ( m_bStoreRagdollInfo && m_pRagdoll )
	{
		matrix3x4_t parentTransform;
		AngleMatrix( GetAbsAngles(), GetAbsOrigin(), parentTransform );
		// FIXME/CHECK:  This might be too expensive to do every frame???
		SaveRagdollInfo( hdr->numbones(), parentTransform, m_BoneAccessor );
	}
	
	// Now set the dieragdoll sequence to get transforms for all
	// non-simulated bones
    SetSequence( SelectWeightedSequence( ACT_DIERAGDOLL ) );
	m_flPlaybackRate = 0;
	UpdatePartitionListEntry();

	NoteRagdollCreationTick( this );

	UpdateVisibility();

#if defined( REPLAY_ENABLED )
	// If replay is enabled on server, add an entry to the ragdoll recorder for this entity
	ConVar* pReplayEnable = (ConVar*)cvar->FindVar( "replay_enable" );
	if ( pReplayEnable && pReplayEnable->GetInt() && !engine->IsPlayingDemo() && !engine->IsPlayingTimeDemo() )
	{
		CReplayRagdollRecorder& RagdollRecorder = CReplayRagdollRecorder::Instance();
		int nStartTick = TIME_TO_TICKS( engine->GetLastTimeStamp() );
		RagdollRecorder.AddEntry( this, nStartTick, m_pRagdoll->RagdollBoneCount() );
	}
#endif

	return true;
}

bool C_BaseAnimating::InitAsClientRagdoll( const matrix3x4_t *pDeltaBones0, const matrix3x4_t *pDeltaBones1, const matrix3x4_t *pCurrentBonePosition, float boneDt, bool bleedOut )
{
	return InitAsClientRagdoll( pDeltaBones0, pDeltaBones1, pCurrentBonePosition, boneDt, m_vecForce, bleedOut );
}

#if defined ( CSTRIKE15 )

// [msmith] We want shadows for the following entity classes.
//          We could probably just get rid of this and turn on rtt shadows for everything that would use them.
//          This this is playing it safe until we look at perf etc.
static const char* g_pszForceRTTClassnames[] =
{
	"class CPhysicsPropMultiplayer",
	"class C_PhysicsProp",
	"class C_CSRagdoll",
	"class C_ServerRagdoll",
	"class C_CHostage",
};

#else

// This just looks like portal 2 stuff
static const char* g_pszForceRTTClassnames[] =
{
	"prop_weighted_cube",
	"class C_NPC_Portal_FloorTurret",
	"class C_NPC_Personality_Core",
	"class C_PhysicsProp",
	//"prop_box_monster",
};

#endif

void C_BaseAnimating::CheckIfEntityShouldForceRTTShadows( void )
{
	for ( int i = 0; i < ARRAYSIZE( g_pszForceRTTClassnames ); ++i )
	{
		if ( FClassnameIs( this, g_pszForceRTTClassnames[i] ) )
		{
			m_bForceRTTShadows = true;
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bnewentity - 
//-----------------------------------------------------------------------------
void C_BaseAnimating::OnDataChanged( DataUpdateType_t updateType )
{

	if (updateType == DATA_UPDATE_CREATED)
	{
		// Now that the data has come down from the server, double-check that the clientside animation variables are handled properly
		UpdateRelevantInterpolatedVars();

		// Check if this is one of the ents that wants RTT shadows
		CheckIfEntityShouldForceRTTShadows();
	}

	bool modelchanged = false;

	// UNDONE: The base class does this as well.  So this is kind of ugly
	// but getting a model by index is pretty cheap...
	const model_t *pModel = modelinfo->GetModel( GetModelIndex() );
	
	if ( pModel != GetModel() )
	{
		modelchanged = true;
	}

	BaseClass::OnDataChanged( updateType );

	if ( m_nPrevBody != GetBody() || m_nPrevSkin != GetSkin() )
	{
		OnTranslucencyTypeChanged();
	}

	if ( (updateType == DATA_UPDATE_CREATED) || modelchanged )
	{
		ResetLatched();
		// if you have this pose parameter, activate HL1-style lipsync/wave envelope tracking
		if ( LookupPoseParameter( LIPSYNC_POSEPARAM_NAME ) != -1 )
		{
			MouthInfo().ActivateEnvelope();
		}

		if ( m_pStudioHdr )
		{
			if ( CSoftbody *pSoftbody = m_pStudioHdr->GetSoftbody() )
			{
				pSoftbody->SetAbsOrigin( GetAbsOrigin(), true );
				pSoftbody->SetAbsAngles( GetAbsAngles(), true );
			}
		}
	}

	// If there's a significant change, make sure the shadow updates
	if ( modelchanged )
	{
		InvalidatePhysicsRecursive( BOUNDS_CHANGED | SEQUENCE_CHANGED ); 
	}

	// Only need to think if animating client side
	if ( m_bClientSideAnimation )
	{
		// Check to see if we should reset our frame
		if ( m_bClientSideFrameReset != m_bLastClientSideFrameReset )
		{
			SetCycle( 0 );
		}
	}
	// build a ragdoll if necessary
	if ( m_bClientSideRagdoll && !m_builtRagdoll )
	{
		if ( !cl_disable_ragdolls.GetBool() )
		{
			BecomeRagdollOnClient( );
		}
	}

	//HACKHACK!!!
	if ( m_bClientSideRagdoll && m_builtRagdoll == true )
	{
		if ( m_pRagdoll == NULL )
			 AddEffects( EF_NODRAW );
	}

	if ( m_pRagdoll && !m_bClientSideRagdoll || !m_bClientSideRagdoll && m_builtRagdoll )
	{
		ClearRagdoll();
	}

	// If ragdolling and get EF_NOINTERP, we probably were dead and are now respawning,
	//  don't do blend out of ragdoll at respawn spot.
	if ( IsEffectActive( EF_NOINTERP ) )
	{
		if ( m_pRagdollInfo && m_pRagdollInfo->m_bActive )
		{
			Msg( "delete ragdoll due to nointerp\n" );
			// Remove ragdoll info
			delete m_pRagdollInfo;
			m_pRagdollInfo = NULL;
		}
		AddToEntityList(ENTITY_LIST_SIMULATE);
	}

	m_bIsUsingRelativeLighting = ( m_hLightingOrigin.Get() != NULL );
}


//-----------------------------------------------------------------------------
// Purpose: Get the index of the attachment point with the specified name
//-----------------------------------------------------------------------------
int C_BaseAnimating::LookupAttachment( const char *pAttachmentName )
{
	CStudioHdr *hdr = GetModelPtr();
	if ( !hdr )
	{
		return -1;
	}

	// NOTE: Currently, the network uses 0 to mean "no attachment" 
	// thus the client must add one to the index of the attachment
	// UNDONE: Make the server do this too to be consistent.
	return Studio_FindAttachment( hdr, pAttachmentName ) + 1;
}

//-----------------------------------------------------------------------------
// Purpose: Get a random index of an attachment point with the specified substring in its name
//-----------------------------------------------------------------------------
int C_BaseAnimating::LookupRandomAttachment( const char *pAttachmentNameSubstring )
{
	CStudioHdr *hdr = GetModelPtr();
	if ( !hdr )
	{
		return -1;
	}

	// NOTE: Currently, the network uses 0 to mean "no attachment" 
	// thus the client must add one to the index of the attachment
	// UNDONE: Make the server do this too to be consistent.
	return Studio_FindRandomAttachment( hdr, pAttachmentNameSubstring ) + 1;
}


void C_BaseAnimating::ClientSideAnimationChanged()
{
	if ( !m_bClientSideAnimation || m_ClientSideAnimationListHandle == INVALID_CLIENTSIDEANIMATION_LIST_HANDLE )
		return;

	MDLCACHE_CRITICAL_SECTION();
	
	clientanimating_t &anim = g_ClientSideAnimationList.Element(m_ClientSideAnimationListHandle);
	Assert(anim.pAnimating == this);
	anim.flags = ComputeClientSideAnimationFlags();

	m_SequenceTransitioner.CheckForSequenceChange( 
		GetModelPtr(),
		GetSequence(),
		m_nNewSequenceParity != m_nPrevNewSequenceParity,
		!IsEffectActive(EF_NOINTERP)
		);
}

unsigned int C_BaseAnimating::ComputeClientSideAnimationFlags()
{
	return FCLIENTANIM_SEQUENCE_CYCLE;
}

void C_BaseAnimating::UpdateClientSideAnimation()
{
	// Update client side animation
	if ( m_bClientSideAnimation )
	{
		Assert( m_ClientSideAnimationListHandle != INVALID_CLIENTSIDEANIMATION_LIST_HANDLE );
		if ( GetSequence() != -1 )
		{
#ifdef DOTA_DLL
			if ( IsVisibleToAnyPlayer() )
#endif
			{
				// latch old values
				OnLatchInterpolatedVariables( LATCH_ANIMATION_VAR );
				// move frame forward
				FrameAdvance( 0.0f ); // 0 means to use the time we last advanced instead of a constant
			}
		}
	}
	else
	{
		Assert( m_ClientSideAnimationListHandle == INVALID_CLIENTSIDEANIMATION_LIST_HANDLE );
	}
}


bool C_BaseAnimating::Simulate()
{
	bool bRet = !m_bIsStaticProp; // static prop defaults to false

	if ( m_bInitModelEffects )
	{
		DelayedInitModelEffects();
	}

	if ( gpGlobals->frametime != 0.0f  )
	{
		CStudioHdr *pStudio = GetModelPtr();
		if ( pStudio && pStudio->SequencesAvailable() )
		{
			if ( pStudio->GetRenderHdr()->flags & STUDIOHDR_FLAGS_NO_ANIM_EVENTS )
			{
				bRet = false;
			}
			else
			{
				DoAnimationEvents( pStudio );
			}
		}
	}

	if ( BaseClass::Simulate() )
	{
		bRet = true;
	}
	// Server says don't interpolate this frame, so set previous info to new info.
	if ( IsEffectActive(EF_NOINTERP) )
	{
		ResetLatched();
	}

	if ( GetSequence() != -1 && m_pRagdoll && ( !m_bClientSideRagdoll ) )
	{
		ClearRagdoll();
	}
	return bRet;
}


bool C_BaseAnimating::TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	if ( ray.m_IsRay && IsSolidFlagSet( FSOLID_CUSTOMRAYTEST ))
	{
		if (!TestHitboxes( ray, fContentsMask, tr ))
			return true;

		return tr.DidHit();
	}

	if ( !ray.m_IsRay && IsSolidFlagSet( FSOLID_CUSTOMBOXTEST ))
	{
		if (!TestHitboxes( ray, fContentsMask, tr ))
			return true;

		return true;
	}

	// We shouldn't get here.
	Assert(0);
	return false;
}


// UNDONE: This almost works.  The client entities have no control over their solid box
// Also they have no ability to expose FSOLID_ flags to the engine to force the accurate
// collision tests.
// Add those and the client hitboxes will be robust
bool C_BaseAnimating::TestHitboxes( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	VPROF( "C_BaseAnimating::TestHitboxes" );

	MDLCACHE_CRITICAL_SECTION();

	CStudioHdr *pStudioHdr = GetModelPtr();
	if (!pStudioHdr)
		return false;

	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( m_nHitboxSet );
	if ( !set || !set->numhitboxes )
		return false;

	// Use vcollide for box traces.
	if ( !ray.m_IsRay )
		return false;

	// This *has* to be true for the existing code to function correctly.
	Assert( ray.m_StartOffset == vec3_origin );

	matrix3x4_t *hitboxbones[MAXSTUDIOBONES];
	HitboxToWorldTransforms( hitboxbones );

	if ( TraceToStudioCsgoHitgroupsPriority( physprops, ray, pStudioHdr, set, hitboxbones, fContentsMask, GetRenderOrigin(), GetModelHierarchyScale(), tr ) )
	{
		mstudiobbox_t *pbox = set->pHitbox( tr.hitbox );
		const mstudiobone_t *pBone = pStudioHdr->pBone(pbox->bone);
		tr.surface.name = "**studio**";
		tr.surface.flags = SURF_HITBOX;
		tr.surface.surfaceProps = pBone->GetSurfaceProp();

		if ( IsRagdoll() )
		{
			IPhysicsObject *pReplace = m_pRagdoll->GetElement( tr.physicsbone );
			if ( pReplace )
			{
				VPhysicsSetObject( NULL );
				VPhysicsSetObject( pReplace );
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Check sequence framerate
// Input  : iSequence - 
// Output : float
//-----------------------------------------------------------------------------
float C_BaseAnimating::GetSequenceCycleRate( CStudioHdr *pStudioHdr, int iSequence )
{
	if ( !pStudioHdr )
		return 0.0f;

	return Studio_CPS( pStudioHdr, pStudioHdr->pSeqdesc(iSequence), iSequence, m_flPoseParameter );
}

float C_BaseAnimating::GetAnimTimeInterval( void ) const
{
#define MAX_ANIMTIME_INTERVAL 0.2f

	float flInterval = MIN( gpGlobals->curtime - m_flAnimTime, MAX_ANIMTIME_INTERVAL );
	return flInterval;
}


//-----------------------------------------------------------------------------
// Sets the cycle, marks the entity as being dirty
//-----------------------------------------------------------------------------
void C_BaseAnimating::SetCycle( float flCycle )
{
	if ( m_flCycle != flCycle )
	{
		if ( !IsFinite( flCycle ) )
		{
			Assert( false );
			return;
		}
		
		m_flCycle = flCycle;
		InvalidatePhysicsRecursive( ANIMATION_CHANGED );

		/*
		if (r_sequence_debug.GetInt() == entindex() )
		{
			DevMsgRT("%d : SetCycle %s:%.3f\n", entindex(), GetSequenceName( GetSequence() ), flCycle );
		}
		*/
	}
}

//-----------------------------------------------------------------------------
// Reset any global fields that are dependant on the sequence
//-----------------------------------------------------------------------------
void C_BaseAnimating::OnNewSequence( void )
{ 
	CStudioHdr *pStudioHdr = GetModelPtr();
	// Assert( pStudioHdr );
	if ( pStudioHdr )
	{
		m_bSequenceLoops = ((GetSequenceFlags( pStudioHdr, GetSequence() ) & STUDIO_LOOPING) != 0);
		m_flGroundSpeed = GetSequenceGroundSpeed( pStudioHdr, GetSequence() );

		// FIXME: why is this called here?  Nothing should have changed to make this nessesary
		SetEventIndexForSequence( pStudioHdr->pSeqdesc( GetSequence() ) );
	}
}

//-----------------------------------------------------------------------------
// Sets the sequence, marks the entity as being dirty
//-----------------------------------------------------------------------------
void C_BaseAnimating::SetSequence( int nSequence )
{ 
	if ( m_nSequence != nSequence )
	{
		/*
		CStudioHdr *hdr = GetModelPtr();
		// Assert( hdr );
		if ( hdr )
		{
			Assert( nSequence >= 0 && nSequence < hdr->GetNumSeq() );
		}
		*/

		if (r_debug_sequencesets.GetInt() == entindex())
		{
			Msg("%s : %s : SetSequence\n", GetClassname(), GetSequenceName( GetSequence() ));
		}		

		m_nSequence = nSequence; 
		InvalidatePhysicsRecursive( BOUNDS_CHANGED | SEQUENCE_CHANGED );
		if ( m_bClientSideAnimation )
		{
			ClientSideAnimationChanged();
		}

		OnNewSequence();
		/*
		if (r_sequence_debug.GetInt() == entindex() )
		{
			DevMsgRT("%d : SetSequence %s\n", entindex(), GetSequenceName( GetSequence() ) );
		}
		*/
	}
}

//-----------------------------------------------------------------------------
// Extracts the bounding box
//-----------------------------------------------------------------------------
void C_BaseAnimating::ExtractBbox( int nSequence, Vector &mins, Vector &maxs )
{
	CStudioHdr *pStudioHdr = GetModelPtr();
	Assert( pStudioHdr );

	::ExtractBbox( pStudioHdr, nSequence, mins, maxs );
}



//=========================================================
// StudioFrameAdvance - advance the animation frame up some interval (default 0.1) into the future
//=========================================================
void C_BaseAnimating::StudioFrameAdvance()
{
	if ( m_bClientSideAnimation )
		return;

	CStudioHdr *hdr = GetModelPtr();
	if ( !hdr )
		return;

	bool watch = false; //Q_strstr( hdr->name(), "grip" ) ? true : false;

	//if (!anim.prevanimtime)
	//{
		//anim.prevanimtime = m_flAnimTime = gpGlobals->curtime;
	//}

	// How long since last animtime
	float flInterval = GetAnimTimeInterval();

	if (flInterval <= 0.001)
	{
		// Msg("%s : %s : %5.3f (skip)\n", STRING(pev->classname), GetSequenceName( GetSequence() ), GetCycle() );
		return;
	}

	//anim.prevanimtime = m_flAnimTime;
	float cycleAdvance = flInterval * GetSequenceCycleRate( hdr, GetSequence() ) * GetPlaybackRate();
	float flNewCycle = GetCycle() + cycleAdvance;
	m_flAnimTime = gpGlobals->curtime;

	if ( watch )
	{
		Msg("%s %6.3f : %6.3f (%.3f)\n", GetClassname(), gpGlobals->curtime, m_flAnimTime, flInterval );
	}

	if ( flNewCycle < 0.0f || flNewCycle >= 1.0f ) 
	{
		if ( IsSequenceLooping( hdr, GetSequence() ) )
		{
			 flNewCycle = SubtractIntegerPart(flNewCycle);
		}
		else
		{
		 	 flNewCycle = (flNewCycle < 0.0f) ? 0.0f : 1.0f;
		}
		
		m_bSequenceFinished = true;	// just in case it wasn't caught in GetEvents
	}

	SetCycle( flNewCycle );

	m_flGroundSpeed = GetSequenceGroundSpeed( hdr, GetSequence() );

#if 0
	// I didn't have a test case for this, but it seems like the right thing to do.  Check multi-player!

	// Msg("%s : %s : %5.1f\n", GetClassname(), GetSequenceName( GetSequence() ), GetCycle() );
	InvalidatePhysicsRecursive( ANIMATION_CHANGED );
#endif

	if ( watch )
	{
		Msg("%s : %s : %1.3f\n", GetClassname(), GetSequenceName( GetSequence() ), GetCycle() );
	}
}

float C_BaseAnimating::GetSequenceGroundSpeed( CStudioHdr *pStudioHdr, int iSequence )
{
	float t = SequenceDuration( pStudioHdr, iSequence );

	if (t > 0)
	{
		return GetSequenceMoveDist( pStudioHdr, iSequence ) / t;
	}
	else
	{
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//
// Input  : iSequence - 
//
// Output : float
//-----------------------------------------------------------------------------
float C_BaseAnimating::GetSequenceMoveDist( CStudioHdr *pStudioHdr, int iSequence )
{
	Vector				vecReturn;
	
	::GetSequenceLinearMotion( pStudioHdr, iSequence, m_flPoseParameter, &vecReturn );

	return vecReturn.Length();
}


//-----------------------------------------------------------------------------
// Purpose: 
//
// Input  : iSequence - 
//			*pVec - 
//	
//-----------------------------------------------------------------------------
void C_BaseAnimating::GetSequenceLinearMotion( int iSequence, Vector *pVec )
{
	::GetSequenceLinearMotion( GetModelPtr(), iSequence, m_flPoseParameter, pVec );
}

float C_BaseAnimating::GetSequenceLinearMotionAndDuration( int iSequence, Vector *pVec )
{
	return ::GetSequenceLinearMotionAndDuration( GetModelPtr(), iSequence, m_flPoseParameter, pVec );
}

//-----------------------------------------------------------------------------
// Purpose:
// Output :
//-----------------------------------------------------------------------------
bool C_BaseAnimating::GetSequenceMovement( int nSequence, float fromCycle, float toCycle, Vector &deltaPosition, QAngle &deltaAngles )
{
	CStudioHdr *pstudiohdr = GetModelPtr( );
	if (! pstudiohdr)
		return false;

	return Studio_SeqMovement( pstudiohdr, nSequence, fromCycle, toCycle, m_flPoseParameter, deltaPosition, deltaAngles );
}


//-----------------------------------------------------------------------------
// Purpose: 
//
// Input  : *pVec - 
//-----------------------------------------------------------------------------
void C_BaseAnimating::GetBlendedLinearVelocity( Vector *pVec )
{
	Vector vecDist;
	float flDuration = GetSequenceLinearMotionAndDuration( GetSequence(), &vecDist );
	VectorScale( vecDist, 1.0 / flDuration, *pVec );

	Vector tmp;
	for (int i = m_SequenceTransitioner.m_animationQueue.Count() - 2; i >= 0; i--)
	{
		CAnimationLayer *blend = &m_SequenceTransitioner.m_animationQueue[i];
		float flWeight = blend->GetFadeout( gpGlobals->curtime );
		if ( flWeight == 0.0f )
			continue;

		flDuration = GetSequenceLinearMotionAndDuration( blend->GetSequence(), &vecDist );
		VectorScale( vecDist, 1.0 / flDuration, tmp );
		*pVec = Lerp( flWeight, *pVec, tmp );
	}
}



//-----------------------------------------------------------------------------
// Purpose: convert local movement into pose parameters that take account of blending
// Input:	vecLocalVelocity - local velocity in right-hand-rule coordinates
//			iMoveX, iMoveY - pose parameter indexes for movement
// Output :
//-----------------------------------------------------------------------------

#define MOVEMENT_ERROR_LIMIT  1.0

void C_BaseAnimating::SetMovementPoseParams( const Vector &vecLocalVelocity, int iMoveX, int iMoveY, int iXSign, int iYSign )
{
	CStudioHdr *pStudioHdr = GetModelPtr( );
	if (! pStudioHdr)
		return;

	Vector2D vecCurrentPose( 0.0f, 0.0f );
	// set the pose parameters to the correct direction, but not value
	if ( vecLocalVelocity.x != 0.0f && fabs( vecLocalVelocity.x ) > fabs( vecLocalVelocity.y ) )
	{
		vecCurrentPose.x = ((vecLocalVelocity.x < 0.0) ? -iXSign : iXSign);
		vecCurrentPose.y = iYSign * (vecLocalVelocity.y / fabs( vecLocalVelocity.x ));
	}
	else if (vecLocalVelocity.y != 0.0f)
	{
		vecCurrentPose.x = iXSign * (vecLocalVelocity.x / fabs( vecLocalVelocity.y ));
		vecCurrentPose.y = ((vecLocalVelocity.y < 0.0) ? -iYSign : iYSign);
	}

	if (vecCurrentPose.x == 0.0f && vecCurrentPose.y == 0.0f)
	{
		SetPoseParameter( pStudioHdr, iMoveX, vecCurrentPose.x );
		SetPoseParameter( pStudioHdr, iMoveY, vecCurrentPose.y );
		return;
	}

	// refine pose parameters to be more accurate
	int i = 0;
	float dx, dy;
	Vector vecAnimVelocity;

	// Set the initial 9-way blend movement pose parameters.
	SetPoseParameter( pStudioHdr, iMoveX, vecCurrentPose.x );
	SetPoseParameter( pStudioHdr, iMoveY, vecCurrentPose.y );

	/*
	if ( r_sequence_debug.GetInt() == entindex() )
	{
		DevMsgRT("%d (%d) : %.2f %.2f : %.2f %.2f : %.3f %.3f\n", entindex(), -1, vecLocalVelocity.x, vecLocalVelocity.y,  vecAnimVelocity.x, vecAnimVelocity.y, vecCurrentPose.x, vecCurrentPose.y );
	}
	*/

	bool retry = true;
	do 
	{
		GetBlendedLinearVelocity( &vecAnimVelocity );

		// adjust X pose parameter based on movement error
		if (fabs( vecAnimVelocity.x ) > 0.001)
		{
			vecCurrentPose.x *= vecLocalVelocity.x / vecAnimVelocity.x;
		}
		else
		{
			// too slow, set to zero so it can optimized out during bone setup
			vecCurrentPose.x = 0;
		}
		SetPoseParameter( pStudioHdr, iMoveX, vecCurrentPose.x );

		// adjust Y pose parameter based on movement error
		if (fabs( vecAnimVelocity.y ) > 0.001)
		{
			vecCurrentPose.y *= vecLocalVelocity.y / vecAnimVelocity.y;
		}
		else
		{
			// too slow, set to zero so it can optimized out during bone setup
			vecCurrentPose.y = 0;
		}
		SetPoseParameter( pStudioHdr, iMoveY, vecCurrentPose.y );

		/*
		if ( r_sequence_debug.GetInt() == entindex() )
		{
			DevMsgRT("%d (%d) : %.2f %.2f : %.2f %.2f : %.3f %.3f\n", entindex(), i, vecLocalVelocity.x, vecLocalVelocity.y,  vecAnimVelocity.x, vecAnimVelocity.y, vecCurrentPose.x, vecCurrentPose.y );
		}
		*/

		dx =  vecLocalVelocity.x - vecAnimVelocity.x;
		dy =  vecLocalVelocity.y - vecAnimVelocity.y;

		retry = (vecCurrentPose.x < 1.0 && vecCurrentPose.x > -1.0) && (dx < -MOVEMENT_ERROR_LIMIT || dx > MOVEMENT_ERROR_LIMIT);
		retry = retry || ((vecCurrentPose.y < 1.0 && vecCurrentPose.y > -1.0) && (dy < -MOVEMENT_ERROR_LIMIT || dy > MOVEMENT_ERROR_LIMIT));

	} while (i++ < 5 && retry);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flInterval - 
// Output : float
//-----------------------------------------------------------------------------
float C_BaseAnimating::FrameAdvance( float flInterval )
{
	CStudioHdr *hdr = GetModelPtr();
	if ( !hdr )
		return 0.0f;

	bool bWatch = false; // Q_strstr( hdr->name, "commando" ) ? true : false;

	float curtime = gpGlobals->curtime;

	if (flInterval == 0.0f)
	{
		flInterval = ( curtime - m_flAnimTime );
		if (flInterval <= 0.001f)
		{
			return 0.0f;
		}
	}

	if ( !m_flAnimTime )
	{
		flInterval = 0.0f;
	}

	float cyclerate = GetSequenceCycleRate( hdr, GetSequence() );
	float addcycle = flInterval * cyclerate * GetPlaybackRate();

	m_prevClientCycle = GetCycle();
	m_prevClientAnimTime = m_flAnimTime;

	if( GetServerIntendedCycle() != -1.0f )
	{
		// The server would like us to ease in a correction so that we will animate the same on the client and server.
		// So we will actually advance the average of what we would have done and what the server wants.
		float serverCycle = GetServerIntendedCycle();
		float serverAdvance = serverCycle - GetCycle();
		bool adjustOkay = serverAdvance > 0.0f;// only want to go forward. backing up looks really jarring, even when slight
		if( serverAdvance < -0.8f )
		{
			// Oh wait, it was just a wraparound from .9 to .1.
			serverAdvance += 1;
			adjustOkay = true;
		}

		if( adjustOkay )
		{
			float originalAdvance;
			originalAdvance = addcycle;
			addcycle = (serverAdvance + addcycle) / 2;

			const float MAX_CYCLE_ADJUSTMENT = 0.1f;
			addcycle = MIN( MAX_CYCLE_ADJUSTMENT, addcycle );// Don't do too big of a jump; it's too jarring as well.

			DevMsg( 2, "(%d): Cycle latch used to correct %.2f in to %.2f instead of %.2f.\n",
				entindex(), GetCycle(), GetCycle() + addcycle, GetCycle() + originalAdvance );
		}

		SetServerIntendedCycle(-1.0f); // Only use a correction once, it isn't valid any time but right now.
	}

	float flNewCycle = GetCycle() + addcycle;
	m_flAnimTime = curtime;

	if ( bWatch )
	{
		Msg("%i CLIENT Time: %6.3f : (Interval %f) : cycle %f rate %f add %f\n", 
			gpGlobals->tickcount, gpGlobals->curtime, flInterval, flNewCycle, cyclerate, addcycle );
	}

	if ( (flNewCycle < 0.0f) || (flNewCycle >= 1.0f) ) 
	{
		if (flNewCycle >= 1.0f)
		{
			ReachedEndOfSequence();
		}

		if ( IsSequenceLooping( hdr, GetSequence() ) )
		{
			flNewCycle = SubtractIntegerPart(flNewCycle);
		}
		else
		{
			flNewCycle = (flNewCycle < 0.0f) ? 0.0f : 1.0f;
		}
		m_bSequenceFinished = true;
	}

	SetCycle( flNewCycle );
	InvalidatePhysicsRecursive( ANIMATION_CHANGED );

	m_flGroundSpeed = GetSequenceGroundSpeed( hdr, GetSequence() );

	return flInterval;
}

// Stubs for weapon prediction
void C_BaseAnimating::ResetSequenceInfo( void )
{
	if (GetSequence() == -1)
	{
		SetSequence( 0 );
	}

	/*
	if (r_sequence_debug.GetInt() == entindex() )
	{
		DevMsgRT("%d : client reset %s\n", entindex(), GetSequenceName( GetSequence() ) );
	}
	*/

	m_flPlaybackRate = 1.0;
	m_bSequenceFinished = false;
	m_flLastEventCheck = 0;

	if ( !IsPlayer() )
	{
		m_nNewSequenceParity = ( ++m_nNewSequenceParity ) & EF_PARITY_MASK;
	}
	m_nResetEventsParity = ( ++m_nResetEventsParity ) & EF_PARITY_MASK;
}
			 
//=========================================================
//=========================================================

bool C_BaseAnimating::IsSequenceLooping( CStudioHdr *pStudioHdr, int iSequence )
{
	return (::GetSequenceFlags( pStudioHdr, iSequence ) & STUDIO_LOOPING) != 0;
}

float C_BaseAnimating::SequenceDuration( CStudioHdr *pStudioHdr, int iSequence )
{
	if ( !pStudioHdr )
	{
		return 0.1f;
	}

	if (iSequence >= pStudioHdr->GetNumSeq() || iSequence < 0 )
	{
		DevWarning( 2, "C_BaseAnimating::SequenceDuration( %d ) out of range\n", iSequence );
		return 0.1;
	}

	return Studio_Duration( pStudioHdr, iSequence, m_flPoseParameter );

}

int C_BaseAnimating::FindTransitionSequence( int iCurrentSequence, int iGoalSequence, int *piDir )
{
	CStudioHdr *hdr = GetModelPtr();
	if ( !hdr )
	{
		return -1;
	}

	if (piDir == NULL)
	{
		int iDir = 1;
		int sequence = ::FindTransitionSequence( hdr, iCurrentSequence, iGoalSequence, &iDir );
		if (iDir != 1)
			return -1;
		else
			return sequence;
	}

	return ::FindTransitionSequence( hdr, iCurrentSequence, iGoalSequence, piDir );

}

void C_BaseAnimating::SetSkin( int iSkin )
{
	if ( m_nSkin != iSkin )
	{
		m_nSkin = iSkin;
		OnTranslucencyTypeChanged();
	}
}

void C_BaseAnimating::SetBody( int iBody )
{
	if ( m_nBody != iBody )
	{
		m_nBody = iBody;
		OnTranslucencyTypeChanged();
	}
}

void C_BaseAnimating::SetBodygroup( int iGroup, int iValue )
{
	Assert( GetModelPtr() );

	int nOldBody = m_nBody;
	::SetBodygroup( GetModelPtr( ), m_nBody, iGroup, iValue );
	if ( nOldBody != m_nBody )
	{
		OnTranslucencyTypeChanged();
	}
}

void C_BaseAnimating::SetBodygroupPreset( char const *szName )
{
	Assert( GetModelPtr() );

	int nOldBody = m_nBody;
	::SetBodygroupPreset( GetModelPtr( ), m_nBody, szName );
	if ( nOldBody != m_nBody )
	{
		OnTranslucencyTypeChanged();
	}
}

int C_BaseAnimating::GetBodygroup( int iGroup )
{
	Assert( GetModelPtr() );

	return ::GetBodygroup( GetModelPtr( ), m_nBody, iGroup );
}

const char *C_BaseAnimating::GetBodygroupName( int iGroup )
{
	Assert( GetModelPtr() );

	return ::GetBodygroupName( GetModelPtr( ), iGroup );
}

int C_BaseAnimating::FindBodygroupByName( const char *name )
{
	Assert( GetModelPtr() );

	return ::FindBodygroupByName( GetModelPtr( ), name );
}

int C_BaseAnimating::GetBodygroupCount( int iGroup )
{
	Assert( GetModelPtr() );

	return ::GetBodygroupCount( GetModelPtr( ), iGroup );
}

int C_BaseAnimating::GetNumBodyGroups( void )
{
	Assert( GetModelPtr() );

	return ::GetNumBodyGroups( GetModelPtr( ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : setnum - 
//-----------------------------------------------------------------------------
void C_BaseAnimating::SetHitboxSet( int setnum )
{
#ifdef _DEBUG
	CStudioHdr *pStudioHdr = GetModelPtr();
	if ( !pStudioHdr )
		return;

	if (setnum > pStudioHdr->numhitboxsets())
	{
		// Warn if an bogus hitbox set is being used....
		static bool s_bWarned = false;
		if (!s_bWarned)
		{
			Warning("Using bogus hitbox set in entity %s!\n", GetClassname() );
			s_bWarned = true;
		}
		setnum = 0;
	}
#endif

	m_nHitboxSet = setnum;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *setname - 
//-----------------------------------------------------------------------------
void C_BaseAnimating::SetHitboxSetByName( const char *setname )
{
	m_nHitboxSet = FindHitboxSetByName( GetModelPtr(), setname );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int C_BaseAnimating::GetHitboxSet( void )
{
	return m_nHitboxSet;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *C_BaseAnimating::GetHitboxSetName( void )
{
	return ::GetHitboxSetName( GetModelPtr(), m_nHitboxSet );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int C_BaseAnimating::GetHitboxSetCount( void )
{
	return ::GetHitboxSetCount( GetModelPtr() );
}

static Vector	hullcolor[8] = 
{
	Vector( 1.0, 1.0, 1.0 ),
	Vector( 1.0, 0.5, 0.5 ),
	Vector( 0.5, 1.0, 0.5 ),
	Vector( 1.0, 1.0, 0.5 ),
	Vector( 0.5, 0.5, 1.0 ),
	Vector( 1.0, 0.5, 1.0 ),
	Vector( 0.5, 1.0, 1.0 ),
	Vector( 1.0, 1.0, 1.0 )
};

//-----------------------------------------------------------------------------
// Purpose: Draw the current hitboxes
//-----------------------------------------------------------------------------
void C_BaseAnimating::DrawClientHitboxes( float duration /*= 0.0f*/, bool monocolor /*= false*/  )
{
	CStudioHdr *pStudioHdr = GetModelPtr();
	if ( !pStudioHdr )
		return;

	mstudiohitboxset_t *set =pStudioHdr->pHitboxSet( m_nHitboxSet );
	if ( !set )
		return;

	Vector position;
	QAngle angles;

	int r = 100;
	int g = 0;
	int b = 0;

	for ( int i = 0; i < set->numhitboxes; i++ )
	{
		mstudiobbox_t *pbox = set->pHitbox( i );

		if ( !monocolor )
		{
			int j = (pbox->group % 8);
			r = ( int ) ( 255.0f * hullcolor[j][0] );
			g = ( int ) ( 255.0f * hullcolor[j][1] );
			b = ( int ) ( 255.0f * hullcolor[j][2] );
		}

		if ( pbox->flCapsuleRadius > 0 )
		{
			matrix3x4_t temp;
			GetHitboxBoneTransform( pbox->bone, pbox->angOffsetOrientation, temp );

			Vector vecCapsuleCenters[ 2 ];
			VectorTransform( pbox->bbmin, temp, vecCapsuleCenters[0] );
			VectorTransform( pbox->bbmax, temp, vecCapsuleCenters[1] );
			
			debugoverlay->AddCapsuleOverlay( vecCapsuleCenters[0], vecCapsuleCenters[1], pbox->flCapsuleRadius, r, g, b, 255, duration );
		}
		else
		{
			GetHitboxBonePosition( pbox->bone, position, angles, pbox->angOffsetOrientation );
			debugoverlay->AddBoxOverlay( position, pbox->bbmin, pbox->bbmax, angles, r, g, b, 0 ,duration );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : activity - 
// Output : int C_BaseAnimating::SelectWeightedSequence
//-----------------------------------------------------------------------------
int C_BaseAnimating::SelectWeightedSequence ( int activity )
{
	Assert( activity != ACT_INVALID );

	return ::SelectWeightedSequence( GetModelPtr(), activity, -1 );

}

int C_BaseAnimating::SelectWeightedSequenceFromModifiers( Activity activity, CUtlSymbol *pActivityModifiers, int iModifierCount )
{
	Assert( activity != ACT_INVALID );
	Assert( GetModelPtr() );
	return GetModelPtr()->SelectWeightedSequenceFromModifiers( activity, pActivityModifiers, iModifierCount );
}

//=========================================================
//=========================================================
int C_BaseAnimating::LookupPoseParameter( CStudioHdr *pstudiohdr, const char *szName )
{
	if ( !pstudiohdr )
		return -1;	

	for (int i = 0; i < pstudiohdr->GetNumPoseParameters(); i++)
	{
		if (stricmp( pstudiohdr->pPoseParameter( i ).pszName(), szName ) == 0)
		{
			return i;
		}
	}

	// AssertMsg( 0, UTIL_VarArgs( "poseparameter %s couldn't be mapped!!!\n", szName ) );
	return -1; // Error
}

//=========================================================
//=========================================================
float C_BaseAnimating::SetPoseParameter( CStudioHdr *pStudioHdr, const char *szName, float flValue )
{
	return SetPoseParameter( pStudioHdr, LookupPoseParameter( pStudioHdr, szName ), flValue );
}

float C_BaseAnimating::SetPoseParameter( CStudioHdr *pStudioHdr, int iParameter, float flValue )
{
	if ( !pStudioHdr )
	{
		Assert(!"C_BaseAnimating::SetPoseParameter: model missing");
		return flValue;
	}

	Assert( IsFinite( flValue ) );

	if ( iParameter >= 0 )
	{
		float flNewValue;
		flValue = Studio_SetPoseParameter( pStudioHdr, iParameter, flValue, flNewValue );
		m_flPoseParameter[ iParameter ] = flNewValue;
	}

	return flValue;
}


float C_BaseAnimating::GetPoseParameter( int iParameter )
{
	CStudioHdr *pStudioHdr = GetModelPtr();

	if ( pStudioHdr == NULL )
		return 0.0f;

	if ( !pStudioHdr )
	{
		Assert(!"C_BaseAnimating::SetPoseParameter: model missing");
		return 0.0f;
	}

	if ( iParameter >= 0 )
	{
		return Studio_GetPoseParameter( pStudioHdr, iParameter, m_flPoseParameter[ iParameter ] );
	}

	return 0.0f;
}

//-----------------------------------------------------------------------------

float C_BaseAnimating::GetFirstSequenceAnimTag( int sequence, int nDesiredTag, float flStart, float flEnd )
{
	Assert( GetModelPtr() );
	return ::GetFirstSequenceAnimTag( GetModelPtr(), sequence, nDesiredTag, flStart, flEnd );
}

float C_BaseAnimating::GetAnySequenceAnimTag( int sequence, int nDesiredTag, float flDefault )
{
	Assert( GetModelPtr() );
	return ::GetAnySequenceAnimTag( GetModelPtr(), sequence, nDesiredTag, flDefault );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *label - 
// Output : int
//-----------------------------------------------------------------------------
int C_BaseAnimating::LookupSequence( const char *label )
{
	Assert( GetModelPtr() );
	return ::LookupSequence( GetModelPtr(), label );
}

int C_BaseAnimating::LookupSequence( CStudioHdr* pHdr, const char *label )
{
	Assert( pHdr );
	return ::LookupSequence( pHdr, label );
}

void C_BaseAnimating::Release()
{
	ClearRagdoll();
	BaseClass::Release();
}

void C_BaseAnimating::Clear( void )
{
	Q_memset(&m_mouth, 0, sizeof(m_mouth));
	BaseClass::Clear();	
}

//-----------------------------------------------------------------------------
// Purpose: Clear current ragdoll
//-----------------------------------------------------------------------------
void C_BaseAnimating::ClearRagdoll()
{
	if ( m_pRagdoll )
	{
		// immediately mark the member ragdoll as being NULL,
		// so that we have no reentrancy problems with the delete
		// (such as the disappearance of the ragdoll physics waking up
		// IVP which causes other objects to move and have a touch 
		// callback on the ragdoll entity, which was a crash on TF)
		// That is to say: it is vital that the member be cleared out
		// BEFORE the delete occurs.
		CRagdoll * RESTRICT pDoomed = m_pRagdoll;
		m_pRagdoll = NULL;

		delete pDoomed;

		// Set to null so that the destructor's call to DestroyObject won't destroy
		//  m_pObjects[ 0 ] twice since that's the physics object for the prop
		VPhysicsSetObject( NULL );

		// If we have ragdoll mins/maxs, we've just come out of ragdoll, so restore them
		if ( m_vecPreRagdollMins != vec3_origin || m_vecPreRagdollMaxs != vec3_origin )
		{
			SetCollisionBounds( m_vecPreRagdollMins, m_vecPreRagdollMaxs );
		}

#if defined( REPLAY_ENABLED )
		// Delete entry from ragdoll recorder if Replay is enabled on server
		ConVar* pReplayEnable = (ConVar*)cvar->FindVar( "replay_enable" );
		if ( pReplayEnable && pReplayEnable->GetInt() && !engine->IsPlayingDemo() && !engine->IsPlayingTimeDemo() )
		{
			CReplayRagdollRecorder& RagdollRecorder = CReplayRagdollRecorder::Instance();
			RagdollRecorder.StopRecordingRagdoll( this );
		}
#endif
	}
	m_builtRagdoll = false;
}

//-----------------------------------------------------------------------------
// Purpose: Looks up an activity by name.
// Input  : label - Name of the activity, ie "ACT_IDLE".
// Output : Returns the activity ID or ACT_INVALID.
//-----------------------------------------------------------------------------
int C_BaseAnimating::LookupActivity( const char *label )
{
	Assert( GetModelPtr() );
	return ::LookupActivity( GetModelPtr(), label );
}

//-----------------------------------------------------------------------------
// Purpose: 
//
// Input  : iSequence - 
//
// Output : char
//-----------------------------------------------------------------------------
const char *C_BaseAnimating::GetSequenceActivityName( int iSequence )
{
	if( iSequence == -1 )
	{
		return "Not Found!";
	}

	if ( !GetModelPtr() )
		return "No model!";

	return ::GetSequenceActivityName( GetModelPtr(), iSequence );
}

//=========================================================
//=========================================================
float C_BaseAnimating::SetBoneController ( int iController, float flValue )
{
	Assert( GetModelPtr() );

	CStudioHdr *pmodel = GetModelPtr();

	Assert(iController >= 0 && iController < NUM_BONECTRLS);

	float controller = m_flEncodedController[iController];
	float retVal = Studio_SetController( pmodel, iController, flValue, controller );
	m_flEncodedController[iController] = controller;
	return retVal;
}


void C_BaseAnimating::GetAimEntOrigin( IClientEntity *pAttachedTo, Vector *pAbsOrigin, QAngle *pAbsAngles )
{
	CBaseEntity *pMoveParent;
	if ( IsEffectActive( EF_BONEMERGE ) && IsEffectActive( EF_BONEMERGE_FASTCULL ) && (pMoveParent = GetMoveParent()) != NULL )
	{
		// Doing this saves a lot of CPU.
		*pAbsOrigin = pMoveParent->GetRenderOrigin();
		*pAbsAngles = pMoveParent->GetRenderAngles();
	}
	else
	{
		if ( !m_pBoneMergeCache || !m_pBoneMergeCache->GetAimEntOrigin( pAbsOrigin, pAbsAngles ) )
			BaseClass::GetAimEntOrigin( pAttachedTo, pAbsOrigin, pAbsAngles );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//
// Input  : iSequence - 
//
// Output : char
//-----------------------------------------------------------------------------
const char *C_BaseAnimating::GetSequenceName( int iSequence )
{
	if( iSequence == -1 )
	{
		return "Not Found!";
	}

	if ( !GetModelPtr() )
		return "No model!";

	return ::GetSequenceName( GetModelPtr(), iSequence );
}

Activity C_BaseAnimating::GetSequenceActivity( int iSequence )
{
	if( iSequence == -1 )
	{
		return ACT_INVALID;
	}

	if ( !GetModelPtr() )
		return ACT_INVALID;

	return (Activity)::GetSequenceActivity( GetModelPtr(), iSequence );
}


//-----------------------------------------------------------------------------
// Computes a box that surrounds all hitboxes
//-----------------------------------------------------------------------------
bool C_BaseAnimating::ComputeHitboxSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	// Note that this currently should not be called during position recomputation because of IK.
	// The code below recomputes bones so as to get at the hitboxes,
	// which causes IK to trigger, which causes raycasts against the other entities to occur,
	// which is illegal to do while in the computeabsposition phase.

	CStudioHdr *pStudioHdr = GetModelPtr();
	if (!pStudioHdr)
		return false;

	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( m_nHitboxSet );
	if ( !set || !set->numhitboxes )
		return false;

	matrix3x4_t *hitboxbones[MAXSTUDIOBONES];
	HitboxToWorldTransforms( hitboxbones );

	// Compute a box in world space that surrounds this entity
	pVecWorldMins->Init( FLT_MAX, FLT_MAX, FLT_MAX );
	pVecWorldMaxs->Init( -FLT_MAX, -FLT_MAX, -FLT_MAX );

	Vector vecBoxAbsMins, vecBoxAbsMaxs;
	for ( int i = 0; i < set->numhitboxes; i++ )
	{
		mstudiobbox_t *pbox = set->pHitbox(i);

		TransformAABB( *hitboxbones[pbox->bone], pbox->bbmin, pbox->bbmax, vecBoxAbsMins, vecBoxAbsMaxs );
		VectorMin( *pVecWorldMins, vecBoxAbsMins, *pVecWorldMins );
		VectorMax( *pVecWorldMaxs, vecBoxAbsMaxs, *pVecWorldMaxs );
	}
	return true;
}

//-----------------------------------------------------------------------------
// Computes a box that surrounds all hitboxes, in entity space
//-----------------------------------------------------------------------------
bool C_BaseAnimating::ComputeEntitySpaceHitboxSurroundingBox( Vector *pVecWorldMins, Vector *pVecWorldMaxs )
{
	// Note that this currently should not be called during position recomputation because of IK.
	// The code below recomputes bones so as to get at the hitboxes,
	// which causes IK to trigger, which causes raycasts against the other entities to occur,
	// which is illegal to do while in the computeabsposition phase.

	CStudioHdr *pStudioHdr = GetModelPtr();
	if (!pStudioHdr)
		return false;

	mstudiohitboxset_t *set = pStudioHdr->pHitboxSet( m_nHitboxSet );
	if ( !set || !set->numhitboxes )
		return false;

	matrix3x4_t *hitboxbones[MAXSTUDIOBONES];
	HitboxToWorldTransforms( hitboxbones );

	// Compute a box in world space that surrounds this entity
	pVecWorldMins->Init( FLT_MAX, FLT_MAX, FLT_MAX );
	pVecWorldMaxs->Init( -FLT_MAX, -FLT_MAX, -FLT_MAX );

	matrix3x4_t worldToEntity, boneToEntity;
	MatrixInvert( EntityToWorldTransform(), worldToEntity );

	Vector vecBoxAbsMins, vecBoxAbsMaxs;
	for ( int i = 0; i < set->numhitboxes; i++ )
	{
		mstudiobbox_t *pbox = set->pHitbox(i);

		ConcatTransforms( worldToEntity, *hitboxbones[pbox->bone], boneToEntity );
		TransformAABB( boneToEntity, pbox->bbmin, pbox->bbmax, vecBoxAbsMins, vecBoxAbsMaxs );
		VectorMin( *pVecWorldMins, vecBoxAbsMins, *pVecWorldMins );
		VectorMax( *pVecWorldMaxs, vecBoxAbsMaxs, *pVecWorldMaxs );
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : scale - 
//-----------------------------------------------------------------------------
void C_BaseAnimating::SetModelScale( float scale, ModelScaleType_t scaleType /*= HIERARCHICAL_MODEL_SCALE*/ )
{
	if ( m_flModelScale != scale || scaleType != m_ScaleType )
	{
		m_flModelScale = scale;
		SetModelScaleType( scaleType );
		InvalidatePhysicsRecursive( BOUNDS_CHANGED );
	}
}

float C_BaseAnimating::GetModelHierarchyScale() const
{
	if ( GetModelScaleType() == HIERARCHICAL_MODEL_SCALE )
		return m_flModelScale;
	CStudioHdr* pHdr = GetModelPtr();
	return ( pHdr && pHdr->numbones() == 1 ) ? m_flModelScale : 1.0f;
}

ModelScaleType_t C_BaseAnimating::GetModelScaleType() const
{
	return m_ScaleType;
}

void C_BaseAnimating::SetModelScaleType( ModelScaleType_t scaleType )
{
	m_ScaleType = scaleType;
}

void C_BaseAnimating::SetCustomMaterial( ICustomMaterial *pCustomMaterial, int nIndex )
{
	if ( !pCustomMaterial )
		return;
	
	const char* szCustomMatName = pCustomMaterial->GetBaseMaterialName();

	studiohdr_t	*pStudioHdr = g_pMDLCache->GetStudioHdr( m_hStudioHdr );
	for ( int i = 0; i < pStudioHdr->numtextures; ++i )
	{
		mstudiotexture_t *pTexture = pStudioHdr->pTexture( i );
		if ( V_strcasecmp( szCustomMatName, V_UnqualifiedFileName( pTexture->pszName() ) ) == 0 )
		{
			CCustomMaterialOwner::SetCustomMaterial( pCustomMaterial, i );
			m_bCanUseFastPath = false;
			return;
		}
	}

	Assert( 0 );
	Warning( "Failed to set custom material for '%s', no matching material name found on model %s\n", szCustomMatName, GetModel() ? modelinfo->GetModelName( GetModel() ) : "<nomodel>" );
}

//-----------------------------------------------------------------------------
// Purpose: Clientside bone follower class. Used just to visualize them.
//			Bone followers WON'T be sent to the client if VISUALIZE_FOLLOWERS is
//			undefined in the server's physics_bone_followers.cpp
//-----------------------------------------------------------------------------
class C_BoneFollower : public C_BaseEntity
{
	DECLARE_CLASS( C_BoneFollower, C_BaseEntity );
	DECLARE_CLIENTCLASS();
public:
	C_BoneFollower( void )
	{
	}

	bool	ShouldDraw( void );
	int		DrawModel( int flags, const RenderableInstance_t &instance );
	bool	TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace );

private:
	int m_modelIndex;
	int m_solidIndex;
};

IMPLEMENT_CLIENTCLASS_DT( C_BoneFollower, DT_BoneFollower, CBoneFollower )
	RecvPropInt( RECVINFO( m_modelIndex ) ),
	RecvPropInt( RECVINFO( m_solidIndex ) ),
END_RECV_TABLE()

void VCollideWireframe_ChangeCallback( IConVar *pConVar, char const *pOldString, float flOldValue )
{
	for ( C_BaseEntity *pEntity = ClientEntityList().FirstBaseEntity(); pEntity; pEntity = ClientEntityList().NextBaseEntity(pEntity) )
	{
		pEntity->UpdateVisibility();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether object should render.
//-----------------------------------------------------------------------------
bool C_BoneFollower::ShouldDraw( void )
{
	return ( vcollide_wireframe.GetBool() );  //MOTODO
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_BoneFollower::DrawModel( int flags, const RenderableInstance_t &instance )
{
	vcollide_t *pCollide = modelinfo->GetVCollide( m_modelIndex );
	if ( pCollide )
	{
		static color32 debugColor = {0,255,255,0};
		matrix3x4_t matrix;
		AngleMatrix( GetAbsAngles(), GetAbsOrigin(), matrix );
		engine->DebugDrawPhysCollide( pCollide->solids[m_solidIndex], NULL, matrix, debugColor );
	}
	return 1;
}

bool C_BoneFollower::TestCollision( const Ray_t &ray, unsigned int mask, trace_t& trace )
{
	vcollide_t *pCollide = modelinfo->GetVCollide( m_modelIndex );
	Assert( pCollide && pCollide->solidCount > m_solidIndex );
	if ( !pCollide )
	{
		DevWarning("Failed to get collision model (%d, %d), %s (%s)\n", m_modelIndex, m_solidIndex, modelinfo->GetModelName(modelinfo->GetModel(m_modelIndex)), IsDormant() ? "dormant" : "active" );
		return false;
	}

	physcollision->TraceBox( ray, pCollide->solids[m_solidIndex], GetAbsOrigin(), GetAbsAngles(), &trace );

	if ( trace.fraction >= 1 )
		return false;

	// return owner as trace hit
	trace.m_pEnt = GetOwnerEntity();
	trace.hitgroup = 0;//m_hitGroup;
	trace.physicsbone = 0;//m_physicsBone; // UNDONE: Get physics bone index & hitgroup
	return trace.DidHit();
}


void C_BaseAnimating::DisableMuzzleFlash()
{
	m_nOldMuzzleFlashParity = m_nMuzzleFlashParity;
}


void C_BaseAnimating::DoMuzzleFlash()
{
	m_nMuzzleFlashParity = (m_nMuzzleFlashParity+1) & ((1 << EF_MUZZLEFLASH_BITS) - 1);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DevMsgRT( PRINTF_FORMAT_STRING char const* pMsg, ... )
{
	if (!engine->Con_IsVisible())
	{
		va_list argptr;
		va_start( argptr, pMsg );
		// 
		{
			static char	string[1024];
			Q_vsnprintf (string, sizeof( string ), pMsg, argptr);
			DevMsg( 1, "%s", string );
		}
		// DevMsg( pMsg, argptr );
		va_end( argptr );
	}
}


void C_BaseAnimating::ForceClientSideAnimationOn()
{
	m_bClientSideAnimation = true;
	AddToClientSideAnimationList();
}


void C_BaseAnimating::AddToClientSideAnimationList()
{
	// Already in list
	if ( m_ClientSideAnimationListHandle != INVALID_CLIENTSIDEANIMATION_LIST_HANDLE )
		return;

	clientanimating_t list( this, 0 );
	m_ClientSideAnimationListHandle = g_ClientSideAnimationList.AddToTail( list );
	ClientSideAnimationChanged();
}

void C_BaseAnimating::RemoveFromClientSideAnimationList()
{
	// Not in list yet
	if ( INVALID_CLIENTSIDEANIMATION_LIST_HANDLE == m_ClientSideAnimationListHandle )
		return;

	unsigned int c = g_ClientSideAnimationList.Count();

	Assert( m_ClientSideAnimationListHandle < c );

	unsigned int last = c - 1;

	if ( last == m_ClientSideAnimationListHandle )
	{
		// Just wipe the final entry
		g_ClientSideAnimationList.FastRemove( last );
	}
	else
	{
		clientanimating_t lastEntry = g_ClientSideAnimationList[ last ];
		// Remove the last entry
		g_ClientSideAnimationList.FastRemove( last );

		// And update it's handle to point to this slot.
		lastEntry.pAnimating->m_ClientSideAnimationListHandle = m_ClientSideAnimationListHandle;
		g_ClientSideAnimationList[ m_ClientSideAnimationListHandle ] = lastEntry;
	}

	// Invalidate our handle no matter what.
	m_ClientSideAnimationListHandle = INVALID_CLIENTSIDEANIMATION_LIST_HANDLE;
}


// static method
void C_BaseAnimating::UpdateClientSideAnimations()
{
	SNPROF_ANIM( "C_BaseAnimating::UpdateClientSideAnimations" );
	VPROF_BUDGET( "UpdateClientSideAnimations", VPROF_BUDGETGROUP_CLIENT_ANIMATION );

	int c = g_ClientSideAnimationList.Count();
	for ( int i = 0; i < c ; ++i )
	{
		clientanimating_t &anim = g_ClientSideAnimationList.Element(i);
		if ( !(anim.flags & FCLIENTANIM_SEQUENCE_CYCLE) )
			continue;
		Assert( anim.pAnimating );
		anim.pAnimating->UpdateClientSideAnimation();
	}
}

CBoneList *C_BaseAnimating::RecordBones( CStudioHdr *hdr, matrix3x4_t *pBoneState )
{
	if ( !ToolsEnabled() )
		return NULL;

	VPROF_BUDGET( "C_BaseAnimating::RecordBones", VPROF_BUDGETGROUP_TOOLS );

	// Possible optimization: Instead of inverting everything while recording, record the pos/q stuff into a structure instead?
	Assert( hdr );

	// Setup our transform based on render angles and origin.
	matrix3x4_t parentTransform;
	AngleMatrix( GetRenderAngles(), GetRenderOrigin(), parentTransform );

	Assert( !m_bBoneListInUse );
	CBoneList *boneList = m_bBoneListInUse ? CBoneList::Alloc() : &m_recordingBoneList;
	m_bBoneListInUse = true;

	boneList->m_nBones = hdr->numbones();

	for ( int i = 0;  i < hdr->numbones(); i++ )
	{
		matrix3x4_t inverted;
		matrix3x4_t output;

		const mstudiobone_t *bone = hdr->pBone( i );

		// Only update bones referenced during setup
		if ( !(bone->flags & BONE_USED_BY_ANYTHING ) )
		{
			boneList->m_quatRot[ i ].Init( 0.0f, 0.0f, 0.0f, 1.0f ); // Init by default sets all 0's, which is invalid
			boneList->m_vecPos[ i ].Init();
			continue;
		}

		if ( bone->parent == -1 )
		{
			// Decompose into parent space
			MatrixInvert( parentTransform, inverted );
		}
		else
		{
			MatrixInvert( pBoneState[ bone->parent ], inverted );
		}

		ConcatTransforms( inverted, pBoneState[ i ], output );

		MatrixAngles( output, 
			boneList->m_quatRot[ i ],
			boneList->m_vecPos[ i ] );
	}

	return boneList;
}

void C_BaseAnimating::GetToolRecordingState( KeyValues *msg )
{
	if ( !ToolsEnabled() )
		return;

	VPROF_BUDGET( "C_BaseAnimating::GetToolRecordingState", VPROF_BUDGETGROUP_TOOLS );

	// Force the animation to drive bones
	CStudioHdr *hdr = GetModelPtr();
	matrix3x4a_t *pBones = (matrix3x4a_t*)stackalloc( ( hdr ? hdr->numbones() : 1 ) * sizeof(matrix3x4_t) );
	if ( hdr )
	{
		SetupBones( pBones, hdr->numbones(), BONE_USED_BY_ANYTHING, gpGlobals->curtime );
	}
	else
	{
		SetupBones( NULL, -1, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
	}

	BaseClass::GetToolRecordingState( msg );

	static BaseAnimatingRecordingState_t state;

	state.m_nSkin = GetSkin();
	state.m_nBody = GetBody();
	state.m_nSequence = m_nSequence;
	state.m_pBoneList = hdr ? RecordBones( hdr, pBones ) : NULL;
	msg->SetPtr( "baseanimating", &state );
	msg->SetBool( "viewmodel", IsViewModelOrAttachment() );

	if ( IsViewModel() )
	{
		C_BaseViewModel *pViewModel = assert_cast< C_BaseViewModel* >( this );
		C_BaseCombatWeapon *pWeapon = pViewModel->GetOwningWeapon();
		if ( pWeapon )
		{
			pWeapon->GetToolViewModelState( msg );
		}
	}
}

void C_BaseAnimating::CleanupToolRecordingState( KeyValues *msg )
{
	if ( !ToolsEnabled() )
		return;

	BaseAnimatingRecordingState_t *pState = (BaseAnimatingRecordingState_t*)msg->GetPtr( "baseanimating" );
	if ( pState && pState->m_pBoneList )
	{
		if ( pState->m_pBoneList != &m_recordingBoneList )
		{
			pState->m_pBoneList->Release();
		}
		else
		{
			Assert( m_bBoneListInUse );
			m_bBoneListInUse = false;
		}
	}

	BaseClass::CleanupToolRecordingState( msg );
}

LocalFlexController_t C_BaseAnimating::GetNumFlexControllers( void )
{
	CStudioHdr *pstudiohdr = GetModelPtr( );
	if (! pstudiohdr)
		return LocalFlexController_t(0);

	return pstudiohdr->numflexcontrollers();
}

const char *C_BaseAnimating::GetFlexDescFacs( int iFlexDesc )
{
	CStudioHdr *pstudiohdr = GetModelPtr( );
	if (! pstudiohdr)
		return 0;

	mstudioflexdesc_t *pflexdesc = pstudiohdr->pFlexdesc( iFlexDesc );

	return pflexdesc->pszFACS( );
}

const char *C_BaseAnimating::GetFlexControllerName( LocalFlexController_t iFlexController )
{
	CStudioHdr *pstudiohdr = GetModelPtr( );
	if (! pstudiohdr)
		return 0;

	mstudioflexcontroller_t *pflexcontroller = pstudiohdr->pFlexcontroller( iFlexController );

	return pflexcontroller->pszName( );
}

const char *C_BaseAnimating::GetFlexControllerType( LocalFlexController_t iFlexController )
{
	CStudioHdr *pstudiohdr = GetModelPtr( );
	if (! pstudiohdr)
		return 0;

	mstudioflexcontroller_t *pflexcontroller = pstudiohdr->pFlexcontroller( iFlexController );

	return pflexcontroller->pszType( );
}


//-----------------------------------------------------------------------------
// Purpose: Note that we've been transmitted a sequence
//-----------------------------------------------------------------------------
void C_BaseAnimating::SetReceivedSequence( void )
{
	m_bReceivedSequence = true;
}

//-----------------------------------------------------------------------------
// Purpose: See if we should force reset our sequence on a new model
//-----------------------------------------------------------------------------
bool C_BaseAnimating::ShouldResetSequenceOnNewModel( void )
{
	return ( m_bReceivedSequence == false );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseAnimating::UpdateBoneAttachments( void )
{
	if ( !m_pAttachedTo )
		return;

	//	Assert( IsFollowingEntity() );
	//	Assert( m_boneIndexAttached >= 0 );

	C_BaseAnimating *follow = FindFollowedEntity();
	if ( follow && (m_boneIndexAttached >= 0) )
	{
		matrix3x4_t boneToWorld, localSpace;
		follow->GetCachedBoneMatrix( m_boneIndexAttached, boneToWorld );
		AngleMatrix( m_boneAngles, m_bonePosition, localSpace );
		ConcatTransforms( boneToWorld, localSpace, GetBoneForWrite( 0 ) );

		Vector absOrigin;
		MatrixGetColumn( GetBone( 0 ), 3, absOrigin );
		SetAbsOrigin( absOrigin );

		QAngle absAngle;
		MatrixAngles( GetBone( 0 ), absAngle );
		SetAbsAngles( absAngle);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseAnimating::AttachEntityToBone( C_BaseAnimating* attachTarget, int boneIndexAttached, Vector bonePosition, QAngle boneAngles )
{
	if ( !attachTarget )
		return;

	SetCollisionGroup( COLLISION_GROUP_DEBRIS );

	FollowEntity( attachTarget );
	SetOwnerEntity( attachTarget );

	//	Assert( boneIndexAttached >= 0 );		// We should be attaching to a bone.

	if ( boneIndexAttached >= 0 )
	{
		m_boneIndexAttached = boneIndexAttached;
		m_bonePosition = bonePosition;
		m_boneAngles = boneAngles;
	}

	m_BoneAccessor.SetReadableBones( BONE_USED_BY_ANYTHING );
	m_BoneAccessor.SetWritableBones( BONE_USED_BY_ANYTHING );

	attachTarget->AddBoneAttachment( this );

	NotifyBoneAttached( attachTarget );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseAnimating::NotifyBoneAttached( C_BaseAnimating* attachTarget )
{
	// If we're already attached to something, remove us from it.
	if ( m_pAttachedTo )
	{
		m_pAttachedTo->RemoveBoneAttachment( this );
		m_pAttachedTo = NULL;
	}

	// Remember the new attach target.
	m_pAttachedTo = attachTarget;

	// Special case: if we just attached to the local player and he is hidden, hide us as well.
	C_BasePlayer *pPlayer = dynamic_cast< C_BasePlayer* >( attachTarget );
	if ( pPlayer && pPlayer->IsLocalPlayer() )
	{
		if ( !pPlayer->ShouldDrawLocalPlayer() )
		{
			AddEffects( EF_NODRAW );
		}
	}
	else
	{
		RemoveEffects( EF_NODRAW );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseAnimating::AddBoneAttachment( C_BaseAnimating* newBoneAttachment )
{
	if ( !newBoneAttachment )
		return;

	m_BoneAttachments.AddToTail( newBoneAttachment );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseAnimating::RemoveBoneAttachment( C_BaseAnimating* boneAttachment )
{
	if ( !boneAttachment )
		return;

	m_BoneAttachments.FindAndRemove( boneAttachment );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_BaseAnimating::GetNumBoneAttachments()
{
	return m_BoneAttachments.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BaseAnimating* C_BaseAnimating::GetBoneAttachment( int i )
{
	if ( m_BoneAttachments.IsValidIndex(i) )
	{
		return m_BoneAttachments[i];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseAnimating::DestroyBoneAttachments()
{
	while ( GetNumBoneAttachments() )
	{
		C_BaseAnimating *pAttachment = GetBoneAttachment(0);
		if ( pAttachment )
		{
			pAttachment->Release();
		}
		else
		{
			m_BoneAttachments.Remove(0);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_BaseAnimating::MoveBoneAttachments( C_BaseAnimating* attachTarget )
{
	if ( !attachTarget )
		return;

	// Move all of our bone attachments to this new object.
	// Preserves the specific bone and attachment location information.
	while ( GetNumBoneAttachments() )
	{
		C_BaseAnimating *pAttachment = GetBoneAttachment(0);
		if ( pAttachment )
		{
			pAttachment->AttachEntityToBone( attachTarget );
		}
		else
		{
			m_BoneAttachments.Remove(0);
		}
	}
}


bool C_BaseAnimating::m_bBoneListInUse = false;
CBoneList C_BaseAnimating::m_recordingBoneList;
