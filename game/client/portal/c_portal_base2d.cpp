//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "c_portal_base2d.h"
#include "portal_shareddefs.h"
#include "clientsideeffects.h"
#include "tier0/vprof.h"
#include "materialsystem/ITexture.h"
#include "hud_macros.h"
#include "IGameSystem.h"
#include "view.h"						// For MainViewOrigin()
#include "clientleafsystem.h"			// For finding the leaves our portals are in
#include "portal_render_targets.h"		// Access to static references to Portal-specific render textures
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#include "tier1/keyvalues.h"
#include "rendertexture.h"
#include "portal_base2d_shared.h"
#include "particles_new.h"
#include "materialsystem/imaterialvar.h"
#include "c_baseprojectedentity.h"
#include "c_basetempentity.h"
#include "c_combatweaponworldclone.h"
#include "C_Portal_Player.h"
#include "prediction.h"
#include "tier1/callqueue.h"

#include "c_pixel_visibility.h"

#include "glow_overlay.h"

#include "dlight.h"
#include "iefx.h"

#include "simple_keys.h"

#ifdef _DEBUG
#include "filesystem.h"
#endif

#include "debugoverlay_shared.h"

// HACK: This define can really hose the following macro, so we need to undefine it for a moment while this sets up
#undef CPortal_Base2D

IMPLEMENT_CLIENTCLASS_DT( C_Portal_Base2D, DT_Portal_Base2D, CPortal_Base2D )
	RecvPropVector( RECVINFO_NAME( m_vecNetworkOrigin, m_vecOrigin ) ),
	RecvPropVector( RECVINFO_NAME( m_angNetworkAngles, m_angRotation ) ),

	RecvPropVector( RECVINFO( m_ptOrigin ) ),
	RecvPropVector( RECVINFO( m_qAbsAngle ) ),

	RecvPropEHandle( RECVINFO(m_hLinkedPortal) ),
	RecvPropBool( RECVINFO(m_bActivated) ),
	RecvPropBool( RECVINFO(m_bOldActivatedState) ),
	RecvPropBool( RECVINFO(m_bIsPortal2) ),
	RecvPropFloat( RECVINFO( m_fNetworkHalfWidth ) ),
	RecvPropFloat( RECVINFO( m_fNetworkHalfHeight ) ),
	RecvPropBool( RECVINFO( m_bIsMobile ) ),

	RecvPropDataTable( RECVINFO_DT( m_PortalSimulator ), 0, &REFERENCE_RECV_TABLE(DT_PortalSimulator) )
END_RECV_TABLE()

// HACK: Now we can replace it
#define CPortal_Base2D C_Portal_Base2D

BEGIN_PREDICTION_DATA( C_Portal_Base2D )
	DEFINE_PRED_FIELD( m_bActivated, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bOldActivatedState, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_hLinkedPortal, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_ptOrigin, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_qAbsAngle, FIELD_VECTOR, FTYPEDESC_INSENDTABLE ),

	//not actually networked fields. But we need them backed up and restored in the same way as the networked ones.	
	DEFINE_FIELD( m_vForward, FIELD_VECTOR ),
	DEFINE_FIELD( m_vRight, FIELD_VECTOR ),
	DEFINE_FIELD( m_vUp, FIELD_VECTOR ),
	DEFINE_FIELD( m_plane_Origin, FIELD_VECTOR4D ),
	DEFINE_FIELD( m_matrixThisToLinked, FIELD_VMATRIX ),
END_PREDICTION_DATA()



static ConVar portal_demohack( "portal_demohack", "0", FCVAR_ARCHIVE, "Do the demo_legacy_rollback setting to help during demo playback of going through portals." );
static ConVar portal_ghost_use_network_origin( "portal_ghost_use_network_origin", "0", 0, "Use the network origin for determining bounds in which to ghost renderables, rather than the abs origin." );

class C_PortalInitHelper : public CAutoGameSystem
{
	virtual bool Init()
	{
		if ( portal_demohack.GetBool() )
		{
			ConVarRef demo_legacy_rollback_ref( "demo_legacy_rollback" );
			demo_legacy_rollback_ref.SetValue( false ); //Portal demos are wrong if the eyes rollback as far as regular demos
		}
		// However, there are probably bugs with this when jump ducking, etc.
		return true;
	}
};
static C_PortalInitHelper s_PortalInitHelper;



C_Portal_Base2D::C_Portal_Base2D( void )
{
	CPortal_Base2D_Shared::AllPortals.AddToTail( this );

	m_PortalSimulator.SetPortalSimulatorCallbacks( this );
}

C_Portal_Base2D::~C_Portal_Base2D( void )
{
	CPortal_Base2D_Shared::AllPortals.FindAndRemove( this );
	g_pPortalRender->RemovePortal( this );

	for( int i = m_GhostRenderables.Count(); --i >= 0; )
	{
		UTIL_Remove( m_GhostRenderables[i] );
	}
	m_GhostRenderables.RemoveAll();

	if( m_pCollisionShape )
	{
		physcollision->DestroyCollide( m_pCollisionShape );
		m_pCollisionShape = NULL;
	}
}

void C_Portal_Base2D::Spawn( void )
{
	// disable the fast path for these entities so our custom DrawModel() function gets called
	m_bCanUseFastPath = false;

	m_matrixThisToLinked.Identity(); //don't accidentally teleport objects to zero space
	BaseClass::Spawn();
}

void C_Portal_Base2D::Activate( void )
{
	BaseClass::Activate();
}

ConVar cl_portal_ghost_use_render_bound("cl_portal_ghost_use_render_bound", "1");
bool C_Portal_Base2D::Simulate()
{
	BaseClass::Simulate();

	//clear list of ghosted entities from last frame, and clear the clipping planes we put on them
	for( int i = m_hGhostingEntities.Count(); --i >= 0; )
	{
		C_BaseEntity *pEntity = m_hGhostingEntities[i].Get();

		if( pEntity != NULL )
		{
			pEntity->m_bEnableRenderingClipPlane = false;
		}
	}
	m_hGhostingEntities.RemoveAll();

	if( IsMobile() )
	{
		m_ptOrigin = GetAbsOrigin();
		AngleVectors( GetAbsAngles(), &m_vForward, &m_vRight, &m_vUp );
		PortalMoved(); //updates link matrix and internals

		UpdateTeleportMatrix();
		OnPortalMoved();
	}

	if( !IsActivedAndLinked() )
	{
		//remove all ghost renderables
		for( int i = m_GhostRenderables.Count(); --i >= 0; )
		{
			UTIL_Remove( m_GhostRenderables[i] );
		}
		
		m_GhostRenderables.RemoveAll();

		return true;
	}


	// Portal doesn't support splits creen yet!!!
	//ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );

	//Find objects that are intersecting the portal and mark them for later replication on the remote portal's side
	//C_Portal_Player *pLocalPlayer = C_Portal_Player::GetLocalPlayer();
	//C_BaseViewModel *pLocalPlayerViewModel = pLocalPlayer->GetViewModel();

	CBaseEntity *pEntsNearPortal[1024];

	Vector vExtents = MAX( GetHalfHeight(), GetHalfWidth() ) * Vector(1,1,1);
	Vector vOrigin = portal_ghost_use_network_origin.GetBool() ? GetNetworkOrigin() : GetAbsOrigin();
	int iEntsNearPortal = ( cl_portal_ghost_use_render_bound.GetBool() ) ? UTIL_RenderablesInBox( pEntsNearPortal, 1024, vOrigin - vExtents, vOrigin + vExtents ) : UTIL_EntitiesInSphere( pEntsNearPortal, 1024, vOrigin, MAX( GetHalfHeight(), GetHalfWidth() ), 0, PARTITION_CLIENT_NON_STATIC_EDICTS );

	if( iEntsNearPortal != 0 )
	{
		float fClipPlane[4];
		fClipPlane[0] = m_plane_Origin.normal.x;
		fClipPlane[1] = m_plane_Origin.normal.y;
		fClipPlane[2] = m_plane_Origin.normal.z;
		fClipPlane[3] = m_plane_Origin.dist - 2.0f;

		for( int i = 0; i != iEntsNearPortal; ++i )
		{
			CBaseEntity *pEntity = pEntsNearPortal[i];
			Assert( pEntity != NULL );

			if( C_PortalGhostRenderable::ShouldCloneEntity( pEntity, this, true ) )
			{
				pEntity->m_bEnableRenderingClipPlane = true;
				memcpy( pEntity->m_fRenderingClipPlane, fClipPlane, sizeof( float ) * 4 );

				EHANDLE hEnt = pEntity;
				m_hGhostingEntities.AddToTail( hEnt );
			}
		}
	}

	//now, fix up our list of ghosted renderables.
	{
		bool *bStillInUse = (bool *)stackalloc( sizeof( bool ) * (m_GhostRenderables.Count() + m_hGhostingEntities.Count()) );
		memset( bStillInUse, 0, sizeof( bool ) * (m_GhostRenderables.Count() + m_hGhostingEntities.Count()) );

		for( int i = m_hGhostingEntities.Count(); --i >= 0; )
		{
			C_BaseEntity *pRenderable = m_hGhostingEntities[i].Get();

			int j;
			for( j = m_GhostRenderables.Count(); --j >= 0; )
			{
				if ( pRenderable == m_GhostRenderables[j]->m_hGhostedRenderable )
				{
					bStillInUse[j] = true;
					m_GhostRenderables[j]->PerFrameUpdate();
					break;
				}
			}
			
			if ( j >= 0 )
				continue;
			
			GetSimulateCallQueue()->QueueCall( C_PortalGhostRenderable::CreateGhostRenderable, pRenderable, this );
		}

		for( int i = m_GhostRenderables.Count(); --i >= 0; )
		{
			if( !bStillInUse[i] &&
					((m_GhostRenderables[i]->m_fDisablePositionChecksUntilTime > gpGlobals->curtime) && 
					(m_GhostRenderables[i]->m_hGhostedRenderable.Get() != NULL) &&
					C_PortalGhostRenderable::ShouldCloneEntity( m_GhostRenderables[i]->m_hGhostedRenderable, this, false )) )
			{
				//this ghost is in a transitional state and gets a temporary reprieve from any position requirements to its ghosting
				bStillInUse[i] = true;
				m_GhostRenderables[i]->PerFrameUpdate();
			}
		}

		//remove unused ghosts
		for ( int i = m_GhostRenderables.Count(); --i >= 0; )
		{
			if ( bStillInUse[i] )
				continue;

			// HACK - I just copied the CClientTools::OnEntityDeleted code here,
			// since the ghosts aren't really entities - they don't have an entindex,
			// they're not in the entitylist, and they get created during Simulate(),
			// which isn't valid for real entities, since it changes the simulate list
			// -jd
			C_PortalGhostRenderable *pGhost = m_GhostRenderables[i];

#if( DEBUG_GHOSTRENDERABLES == 1 )
			C_BaseEntity *pRenderable = pGhost->m_hGhostedRenderable;
			if( pRenderable )
			{
				Vector vTransformedOrigin;
				QAngle qTransformedAngles;
				VectorTransform( pRenderable->GetRenderOrigin(), m_matrixThisToLinked.As3x4(), vTransformedOrigin );
				qTransformedAngles = TransformAnglesToWorldSpace( pRenderable->GetRenderAngles(), m_matrixThisToLinked.As3x4() );
				NDebugOverlay::BoxAngles( vTransformedOrigin, pRenderable->CollisionProp()->OBBMins(), pRenderable->CollisionProp()->OBBMaxs(), qTransformedAngles, 255, 0, 0, 16, 0.0f );
			}
#endif

			if ( ToolsEnabled() )
			{
				HTOOLHANDLE handle = pGhost ? pGhost->GetToolHandle() : (HTOOLHANDLE)0;
				if ( handle != (HTOOLHANDLE)0 )
				{
					if ( clienttools->IsInRecordingMode() )
					{
						// Send deletion message to tool interface
						KeyValues *kv = new KeyValues( "deleted" );
						ToolFramework_PostToolMessage( handle, kv );
						kv->deleteThis();
					}

					clienttools->DetachFromEntity( pGhost );
				}
			}

			UTIL_Remove( pGhost );
			m_GhostRenderables.FastRemove( i );
		}
	}

	//ensure the shared clip plane is up to date
	C_Portal_Base2D *pLinkedPortal = m_hLinkedPortal.Get();

	// When the portal is flat on the ground or on a ceiling, tweak the clip plane offset to hide the player feet sticking 
	// through the floor. We can't use this offset in other configurations -- it visibly would cut of parts of the model.
	float flTmp = fabsf( DotProduct( pLinkedPortal->m_plane_Origin.normal, Vector( 0, 0, 1 ) ) );
	flTmp = fabsf( flTmp - 1.0f );
	float flClipPlaneFudgeOffset = ( flTmp < 0.01f ) ? 2.0f : -2.0f;

	m_fGhostRenderablesClip[0] = pLinkedPortal->m_plane_Origin.normal.x;
	m_fGhostRenderablesClip[1] = pLinkedPortal->m_plane_Origin.normal.y;
	m_fGhostRenderablesClip[2] = pLinkedPortal->m_plane_Origin.normal.z;
	m_fGhostRenderablesClip[3] = pLinkedPortal->m_plane_Origin.dist - 2.0f;

	m_fGhostRenderablesClipForPlayer[0] = pLinkedPortal->m_plane_Origin.normal.x;
	m_fGhostRenderablesClipForPlayer[1] = pLinkedPortal->m_plane_Origin.normal.y;
	m_fGhostRenderablesClipForPlayer[2] = pLinkedPortal->m_plane_Origin.normal.z;
	m_fGhostRenderablesClipForPlayer[3] = pLinkedPortal->m_plane_Origin.dist + flClipPlaneFudgeOffset;
	return true;
}

C_PortalGhostRenderable *C_Portal_Base2D::GetGhostRenderableForEntity( C_BaseEntity *pEntity )
{
	for( int i = 0; i != m_GhostRenderables.Count(); ++i )
	{
		if( m_GhostRenderables[i]->m_hGhostedRenderable == pEntity )
			return m_GhostRenderables[i];
	}

	return NULL;
}

void C_Portal_Base2D::UpdateOnRemove( void )
{
	g_pPortalRender->RemovePortal( this );

	BaseClass::UpdateOnRemove();
}

void C_Portal_Base2D::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	if ( Q_stricmp( pszParticleName, "portal_1_overlap" ) == 0 || Q_stricmp( pszParticleName, "portal_2_overlap" ) == 0 )
	{
		float fClosestDistanceSqr = -1.0f;
		Vector vClosestPosition;

		int iPortalCount = CPortal_Base2D_Shared::AllPortals.Count();
		if( iPortalCount != 0 )
		{
			C_Portal_Base2D **pPortals = CPortal_Base2D_Shared::AllPortals.Base();
			for( int i = 0; i != iPortalCount; ++i )
			{
				C_Portal_Base2D *pTempPortal = pPortals[i];
				if ( pTempPortal != this && pTempPortal->IsActive() )
				{
					Vector vPosition = pTempPortal->GetAbsOrigin();

					float fDistanceSqr = pNewParticleEffect->GetRenderOrigin().DistToSqr( vPosition );

					if ( fClosestDistanceSqr == -1.0f || fClosestDistanceSqr > fDistanceSqr )
					{
						fClosestDistanceSqr = fDistanceSqr;
						vClosestPosition = vPosition;
					}
				}
			}
		}

		if ( fClosestDistanceSqr != -1.0f )
		{
			pNewParticleEffect->SetControlPoint( 1, vClosestPosition );
		}
	}
}

void C_Portal_Base2D::OnPreDataChanged( DataUpdateType_t updateType )
{
	//PreDataChanged.m_matrixThisToLinked = m_matrixThisToLinked;
	PreDataChanged.m_bIsPortal2 = m_bIsPortal2;
	PreDataChanged.m_bActivated = m_bActivated;
	PreDataChanged.m_bOldActivatedState = m_bOldActivatedState;
	PreDataChanged.m_hLinkedTo = m_hLinkedPortal.Get();
	PreDataChanged.m_bIsMobile = m_bIsMobile;
	PreDataChanged.m_vOrigin = m_ptOrigin;
	PreDataChanged.m_qAngles = m_qAbsAngle;

	BaseClass::OnPreDataChanged( updateType );
}

//ConVar r_portal_light_innerangle( "r_portal_light_innerangle", "90.0", FCVAR_CLIENTDLL );
//ConVar r_portal_light_outerangle( "r_portal_light_outerangle", "90.0", FCVAR_CLIENTDLL );
//ConVar r_portal_light_forward( "r_portal_light_forward", "0.0", FCVAR_CLIENTDLL );

void C_Portal_Base2D::HandleNetworkChanges( void )
{
	C_Portal_Base2D *pRemote = m_hLinkedPortal;
	m_pLinkedPortal = pRemote;

	//get absolute origin and angles, but cut out interpolation, use the network position and angles as transformed by any move parent
	{		
		ALIGN16 matrix3x4_t finalMatrix;
		AngleMatrix( m_qAbsAngle, finalMatrix );

		MatrixGetColumn( finalMatrix, 0, m_vForward );
		MatrixGetColumn( finalMatrix, 1, m_vRight );
		MatrixGetColumn( finalMatrix, 2, m_vUp );
		m_vRight = -m_vRight;
	}
	SetHalfSizes( m_fNetworkHalfWidth, m_fNetworkHalfHeight );

	const PS_PlacementData_t &placement = m_PortalSimulator.GetInternalData().Placement;

	bool bActivityChanged = PreDataChanged.m_bActivated != IsActive();
	bool bPortalMoved = ((/*(m_ptOrigin != PreDataChanged.m_vOrigin) &&*/ (m_ptOrigin != placement.ptCenter)) || //moved
							(/*(m_qAbsAngle != PreDataChanged.m_qAngles) &&*/ (m_qAbsAngle != placement.qAngles)) ||  //rotated
							(placement.fHalfWidth != m_fNetworkHalfWidth) || //resized
							(placement.fHalfHeight != m_fNetworkHalfHeight) || //resized
							(PreDataChanged.m_bIsPortal2 != m_bIsPortal2) ) || //swapped portal id
							((PreDataChanged.m_bIsMobile == true) && (m_bIsMobile == false)); //count an end of nudging as a move
	bool bNewLinkage = ( (PreDataChanged.m_hLinkedTo.Get() != m_hLinkedPortal.Get()) );

	if( bNewLinkage || IsMobile() )
		m_PortalSimulator.DetachFromLinked(); //detach now so moves are theoretically faster

	if( IsActive() )
	{
		//generic stuff we'll need
		g_pPortalRender->AddPortal( this ); //will know if we're already added and avoid adding twice
		 
		if( bPortalMoved || bActivityChanged )
		{
			if( !IsMobile() )
			{
				m_PortalSimulator.SetSize( GetHalfWidth(), GetHalfHeight() );
				m_PortalSimulator.MoveTo( m_ptOrigin, m_qAbsAngle );
			}
		}

		if( pRemote && !IsMobile() )
			m_PortalSimulator.AttachTo( &pRemote->m_PortalSimulator );
	}
	else
	{
		g_pPortalRender->RemovePortal( this );
		m_PortalSimulator.DetachFromLinked();

		if( bPortalMoved || bActivityChanged )
		{
			if( !IsMobile() )
			{
				m_PortalSimulator.SetSize( GetHalfWidth(), GetHalfHeight() );
				m_PortalSimulator.MoveTo( m_ptOrigin, m_qAbsAngle );
			}
		}
	}

	if( bNewLinkage || bPortalMoved || bActivityChanged )
	{
		//Warning_SpewCallStack( 10, "C_Portal_Base2D::HandleNetworkChanges( %.2f )\n", gpGlobals->curtime );
		PortalMoved(); //updates link matrix and internals
		UpdateTeleportMatrix();

		if ( bPortalMoved )
		{
			OnPortalMoved();
		}

		if( bActivityChanged )
		{
			OnActiveStateChanged();
		}

		if( bNewLinkage )
		{
			OnLinkageChanged( PreDataChanged.m_hLinkedTo.Get() );
		}

		UpdateGhostRenderables();
		if( pRemote )
			pRemote->UpdateGhostRenderables();
	}

	m_PortalSimulator.SetCarvedParent( GetMoveParent() );

	if( m_bIsPortal2 )
	{
		m_PortalSimulator.EditDebuggingData().overlayColor.SetColor( 255, 0, 0, 255 );
	}
	else
	{
		m_PortalSimulator.EditDebuggingData().overlayColor.SetColor( 0, 0, 255, 255 );
	}
}



void C_Portal_Base2D::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	bool bActivityChanged = PreDataChanged.m_bActivated != IsActive();

	HandleNetworkChanges();

	if ( bActivityChanged )
	{
		UpdateVisibility();
	}
}

void C_Portal_Base2D::HandlePredictionError( bool bErrorInThisEntity )
{
	if( bErrorInThisEntity )
	{
		HandleNetworkChanges();
	}

	BaseClass::HandlePredictionError( bErrorInThisEntity );
}

void C_Portal_Base2D::UpdateGhostRenderables( void )
{
	//lastly, update all ghost renderables
	for( int i = m_GhostRenderables.Count(); --i >= 0; )
	{
		m_GhostRenderables[i]->m_matGhostTransform = m_matrixThisToLinked;
		m_GhostRenderables[i]->m_fDisablePositionChecksUntilTime = -FLT_MAX;
	}
}

extern ConVar building_cubemaps;

bool C_Portal_Base2D::ShouldDraw()
{
	if ( !BaseClass::ShouldDraw() )
		return false;

	if ( !IsActive() || building_cubemaps.GetBool() )
		return false;

	return true;
}

void C_Portal_Base2D::StartTouch( C_BaseEntity *pOther )
{
	if( pOther->IsPlayer() )
		return;

	//Warning( "C_Portal_Base2D::StartTouch(%s)\n", pOther->GetClassname() );
	BaseClass::StartTouch( pOther );

	// Since prop_portal is a trigger it doesn't send back start touch, so I'm forcing it
	pOther->StartTouch( this );

	if( (m_hLinkedPortal == NULL) || (IsActive() == false) || IsMobile() || m_hLinkedPortal->IsMobile() )
		return;

	if( CPortal_Base2D_Shared::IsEntityTeleportable( pOther ) )
	{
		CCollisionProperty *pOtherCollision = pOther->CollisionProp();
		Vector vWorldMins, vWorldMaxs;
		pOtherCollision->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
		Vector ptOtherCenter = (vWorldMins + vWorldMaxs) / 2.0f;

		if( m_plane_Origin.normal.Dot( ptOtherCenter ) > m_plane_Origin.dist )
		{
			//we should be interacting with this object, add it to our environment
			if( true )//SharedEnvironmentCheck( pOther ) )
			{
				Assert( IsMobile() || m_hLinkedPortal->IsMobile() || ((m_PortalSimulator.GetLinkedPortalSimulator() == NULL) && (m_hLinkedPortal.Get() == NULL)) || 
					(m_PortalSimulator.GetLinkedPortalSimulator() == &m_hLinkedPortal->m_PortalSimulator) ); //make sure this entity is linked to the same portal as our simulator

				CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pOther );
				if( pOwningSimulator && (pOwningSimulator != &m_PortalSimulator) )
					pOwningSimulator->ReleaseOwnershipOfEntity( pOther );

				m_PortalSimulator.TakeOwnershipOfEntity( pOther );
			}
		}
	}
}

void C_Portal_Base2D::Touch( C_BaseEntity *pOther )
{
	if( pOther->IsPlayer() )
		return;

	return BaseClass::Touch( pOther );
}

void C_Portal_Base2D::EndTouch( C_BaseEntity *pOther )
{
	if( pOther->IsPlayer() )
		return;

	//Warning( "C_Portal_Base2D::EndTouch(%s)\n", pOther->GetClassname() );
	BaseClass::EndTouch( pOther );

	// Since prop_portal is a trigger it doesn't send back end touch, so I'm forcing it
	pOther->EndTouch( this );

	// Don't do anything on end touch if it's not active
	if( !IsActive() || IsMobile() || ((m_hLinkedPortal.Get() != NULL) && m_hLinkedPortal->IsMobile())  )
	{
		return;
	}

	if( ShouldTeleportTouchingEntity( pOther ) ) //an object passed through the plane and all the way out of the touch box
	{
		TeleportTouchingEntity( pOther );
	}
	else
	{
		m_PortalSimulator.ReleaseOwnershipOfEntity( pOther );
	}
}


//-----------------------------------------------------------------------------
// Handle recording for the SFM
//-----------------------------------------------------------------------------
void C_Portal_Base2D::GetToolRecordingState( KeyValues *msg )
{
	if ( !ToolsEnabled() )
		return;

	VPROF_BUDGET( "C_Portal_Base2D::GetToolRecordingState", VPROF_BUDGETGROUP_TOOLS );
	BaseClass::GetToolRecordingState( msg );

	if ( !IsActive() )
	{
		BaseEntityRecordingState_t *pBaseEntity = (BaseEntityRecordingState_t*)msg->GetPtr( "baseentity" );
		pBaseEntity->m_bVisible = false;
	}
}

void C_Portal_Base2D::UpdateTeleportMatrix( void )
{
	//setup our origin plane
	m_plane_Origin.normal = m_vForward;
	m_plane_Origin.dist = m_plane_Origin.normal.Dot( m_ptOrigin );
	m_plane_Origin.signbits = SignbitsForPlane( &m_plane_Origin );

	Vector vAbsNormal;
	vAbsNormal.x = fabs(m_plane_Origin.normal.x);
	vAbsNormal.y = fabs(m_plane_Origin.normal.y);
	vAbsNormal.z = fabs(m_plane_Origin.normal.z);

	if( vAbsNormal.x > vAbsNormal.y )
	{
		if( vAbsNormal.x > vAbsNormal.z )
		{
			if( vAbsNormal.x > 0.999f )
				m_plane_Origin.type = PLANE_X;
			else
				m_plane_Origin.type = PLANE_ANYX;
		}
		else
		{
			if( vAbsNormal.z > 0.999f )
				m_plane_Origin.type = PLANE_Z;
			else
				m_plane_Origin.type = PLANE_ANYZ;
		}
	}
	else
	{
		if( vAbsNormal.y > vAbsNormal.z )
		{
			if( vAbsNormal.y > 0.999f )
				m_plane_Origin.type = PLANE_Y;
			else
				m_plane_Origin.type = PLANE_ANYY;
		}
		else
		{
			if( vAbsNormal.z > 0.999f )
				m_plane_Origin.type = PLANE_Z;
			else
				m_plane_Origin.type = PLANE_ANYZ;
		}
	}

	UTIL_Portal_ComputeMatrix( this, m_pLinkedPortal );
}

void C_Portal_Base2D::SetIsPortal2( bool bValue )
{
	m_bIsPortal2 = bValue;
}

void C_Portal_Base2D::UpdatePartitionListEntry()
{
	::partition->RemoveAndInsert( 
		PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS,  // remove
		PARTITION_CLIENT_TRIGGER_ENTITIES,  // add
		CollisionProp()->GetPartitionHandle() );
}

bool C_Portal_Base2D::TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	if ( !m_pCollisionShape )
	{
		//HACK: This is a last-gasp type fix for a crash caused by m_pCollisionShape not yet being set up
		// during a restore.
		UpdateCollisionShape();
	}

	physcollision->TraceBox( ray, MASK_ALL, NULL, m_pCollisionShape, m_ptOrigin, m_qAbsAngle, &tr );
	return tr.DidHit();
}

void C_Portal_Base2D::NewLocation( const Vector &vOrigin, const QAngle &qAngles )
{
	//Warning( "C_Portal_Base2D::NewLocation(client) %f     %.2f %.2f %.2f\n", gpGlobals->curtime, XYZ( vOrigin ) );

	//get absolute origin and angles, but cut out interpolation, use the network position and angles as transformed by any move parent
	{
		ALIGN16 matrix3x4_t finalMatrix;
		AngleMatrix( qAngles, finalMatrix );

		MatrixGetColumn( finalMatrix, 0, m_vForward );
		MatrixGetColumn( finalMatrix, 1, m_vRight );
		MatrixGetColumn( finalMatrix, 2, m_vUp );
		m_vRight = -m_vRight;

		m_ptOrigin = vOrigin;
		m_qAbsAngle = qAngles;
	}

	AddEffects( EF_NOINTERP );
	//PredictClearNoInterpEffect();
	if( GetMoveParent() )
	{
		SetAbsOrigin( vOrigin );
		SetAbsAngles( qAngles );
	}
	else
	{
		SetNetworkOrigin( vOrigin );
		SetNetworkAngles( qAngles );
	}
	GetOriginInterpolator().ClearHistory();
	GetRotationInterpolator().ClearHistory();

	if( IsActive() == false )
	{
		SetActive( true );
		OnActiveStateChanged();
	}

	m_PortalSimulator.MoveTo( m_ptOrigin, m_qAbsAngle );
	m_PortalSimulator.SetSize( GetHalfWidth(), GetHalfHeight() );

	if( m_hLinkedPortal.Get() && !IsMobile() )
		m_PortalSimulator.AttachTo( &m_hLinkedPortal.Get()->m_PortalSimulator );

	m_pLinkedPortal = m_hLinkedPortal;

	
	PortalMoved(); //updates link matrix and internals
	UpdateTeleportMatrix();

	OnPortalMoved();
	UpdateGhostRenderables();

	C_Portal_Base2D *pRemote = m_hLinkedPortal.Get();
	if( pRemote )
		pRemote->UpdateGhostRenderables();

	g_pPortalRender->AddPortal( this ); //will know if we're already added and avoid adding twice
}

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecachePortalBased2DMaterials )
	PRECACHE( MATERIAL, "models/portals/portal_depthdoubler" )
	PRECACHE( MATERIAL, "models/portals/portal_stencil_hole" )
PRECACHE_REGISTER_END()

class CAutoInitPortalDrawingMaterials : public CAutoGameSystem
{
public:
	PortalDrawingMaterials m_Materials;
	void LevelInitPreEntity()
	{
		m_Materials.m_Portal_Stencil_Hole.Init( "decals/portalstencildecal", TEXTURE_GROUP_CLIENT_EFFECTS );
	}
};
static CAutoInitPortalDrawingMaterials s_FlatBasicPortalDrawingMaterials;

PortalDrawingMaterials& C_Portal_Base2D::m_Materials = s_FlatBasicPortalDrawingMaterials.m_Materials;

void C_Portal_Base2D::DrawStencilMask( IMatRenderContext *pRenderContext )
{
	DrawSimplePortalMesh( pRenderContext, m_Materials.m_Portal_Stencil_Hole );
	DrawRenderFixMesh( pRenderContext, g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
}


ConVar cl_portal_teleportation_interpolation_fixup_method( "cl_portal_teleportation_interpolation_fixup_method", "1", 0, "0 = transform history only, 1 = insert discontinuity transform" );

void EntityPortalledMessageHandler( C_BaseEntity *pEntity, C_Portal_Base2D *pPortal, float fTime, bool bForcedDuck )
{
#if( PLAYERPORTALDEBUGSPEW == 1 )
	Warning( "EntityPortalledMessageHandler() %f -=- %f %i======================\n", fTime, engine->GetLastTimeStamp(), prediction->GetLastAcknowledgedCommandNumber() );
#endif

	C_PortalGhostRenderable *pGhost = pPortal->GetGhostRenderableForEntity( pEntity );
	if( !pGhost )
	{
		//high velocity edge case. Entity portalled before it ever created a clone. But will need one for the interpolated origin history
		if( C_PortalGhostRenderable::ShouldCloneEntity( pEntity, pPortal, false ) )
		{
			pGhost = C_PortalGhostRenderable::CreateGhostRenderable( pEntity, pPortal );
			if( pGhost )
			{
				Assert( !pPortal->m_hGhostingEntities.IsValidIndex( pPortal->m_hGhostingEntities.Find( pEntity ) ) );
				pPortal->m_hGhostingEntities.AddToTail( pEntity );
				Assert( pPortal->m_GhostRenderables.IsValidIndex( pPortal->m_GhostRenderables.Find( pGhost ) ) );
				pGhost->PerFrameUpdate();
			}
		}
	}

	if( pGhost )
	{
		C_PortalGhostRenderable::CreateInversion( pGhost, pPortal, fTime );
	}
	
	if( pEntity->IsPlayer() )
	{
		((C_Portal_Player *)pEntity)->PlayerPortalled( pPortal, fTime, bForcedDuck );
		return;
	}	

	pEntity->AddEFlags( EFL_DIRTY_ABSTRANSFORM );

	VMatrix matTransform = pPortal->MatrixThisToLinked();

	CDiscontinuousInterpolatedVar< QAngle > &rotInterp = pEntity->GetRotationInterpolator();
	CDiscontinuousInterpolatedVar< Vector > &posInterp = pEntity->GetOriginInterpolator();


	if( cl_portal_teleportation_interpolation_fixup_method.GetInt() == 0 )
	{
		UTIL_TransformInterpolatedAngle( rotInterp, matTransform.As3x4(), fTime );
		UTIL_TransformInterpolatedPosition( posInterp, matTransform, fTime );
	}
	else
	{
		rotInterp.InsertDiscontinuity( matTransform.As3x4(), fTime );
		posInterp.InsertDiscontinuity( matTransform.As3x4(), fTime );
	}

	if ( pEntity->IsToolRecording() )
	{
		static EntityTeleportedRecordingState_t state;

		KeyValues *msg = new KeyValues( "entity_teleported" );
		msg->SetPtr( "state", &state );
		state.m_bTeleported = true;
		state.m_bViewOverride = false;
		state.m_vecTo = pEntity->GetAbsOrigin();
		state.m_qaTo = pEntity->GetAbsAngles();
		state.m_teleportMatrix = matTransform.As3x4();

		// Post a message back to all IToolSystems
		Assert( (int)pEntity->GetToolHandle() != 0 );
		ToolFramework_PostToolMessage( pEntity->GetToolHandle(), msg );

		msg->deleteThis();
	}
	
	C_Portal_Player* pPlayer = C_Portal_Player::GetLocalPortalPlayer( GET_ACTIVE_SPLITSCREEN_SLOT() );
	if ( pPlayer && pEntity == pPlayer->GetAttachedObject() )
	{
		C_BaseAnimating *pAnim = pEntity->GetBaseAnimating();	
		if ( pAnim && pAnim->IsUsingRenderOriginOverride() )
		{
			pPlayer->ResetHeldObjectOutOfEyeTransitionDT();
		}
	}
}


struct PortalTeleportationLogEntry_t
{
	CHandle<C_BaseEntity> hEntity;
	CHandle<C_Portal_Base2D> hPortal;
	float fTeleportTime;
	bool bForcedDuck;
};

static CThreadFastMutex s_PortalTeleportationLogMutex;
static CUtlVector<PortalTeleportationLogEntry_t> s_PortalTeleportationLog;

void RecieveEntityPortalledMessage( CHandle<C_BaseEntity> hEntity, CHandle<C_Portal_Base2D> hPortal, float fTime, bool bForcedDuck )
{
	PortalTeleportationLogEntry_t temp;
	temp.hEntity = hEntity;
	temp.hPortal = hPortal;
	temp.fTeleportTime = fTime;
	temp.bForcedDuck = bForcedDuck;

	s_PortalTeleportationLogMutex.Lock();
	s_PortalTeleportationLog.AddToTail( temp );
	s_PortalTeleportationLogMutex.Unlock();
}


void ProcessPortalTeleportations( void )
{
	s_PortalTeleportationLogMutex.Lock();
	for( int i = 0; i != s_PortalTeleportationLog.Count(); ++i )
	{
		PortalTeleportationLogEntry_t &entry = s_PortalTeleportationLog[i];

		C_Portal_Base2D *pPortal = entry.hPortal;
		if( pPortal == NULL )
			continue;

		//grab other entity's EHANDLE
		C_BaseEntity *pEntity = entry.hEntity;
		if( pEntity == NULL )
			continue;

		EntityPortalledMessageHandler( pEntity, pPortal, entry.fTeleportTime, entry.bForcedDuck );
	}
	s_PortalTeleportationLog.RemoveAll();
	s_PortalTeleportationLogMutex.Unlock();
}
