//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "C_PortalGhostRenderable.h"
#include "PortalRender.h"
#include "c_portal_player.h"
#include "model_types.h"
#include "c_basecombatweapon.h"
#include "c_combatweaponworldclone.h"
#include "toolframework_client.h"

ConVar portal_ghosts_disable( "portal_ghosts_disable", "0", 0, "Disables rendering of ghosted objects in portal environments" );

inline float GhostedRenderableOriginTime( C_BaseEntity *pGhostedRenderable, float fCurTime )
{
	return pGhostedRenderable->GetOriginInterpolator().GetInterpolatedTime( pGhostedRenderable->GetEffectiveInterpolationCurTime( fCurTime ) );
}

#define GHOST_RENDERABLE_TEN_TON_HAMMER 0

#if (GHOST_RENDERABLE_TEN_TON_HAMMER == 1)
void GhostResetEverything( C_PortalGhostRenderable *pGhost )
{
	C_BaseEntity *pGhostedRenderable = pGhost->m_hGhostedRenderable;
	if( pGhostedRenderable )
	{
		pGhostedRenderable->MarkRenderHandleDirty();
		if( pGhost->m_bSourceIsBaseAnimating )
		{
			//((C_BaseAnimating *)pGhostedRenderable)->MarkForThreadedBoneSetup();
			((C_BaseAnimating *)pGhostedRenderable)->InvalidateBoneCache();
		}
		pGhostedRenderable->CollisionProp()->MarkPartitionHandleDirty();
		pGhostedRenderable->CollisionProp()->MarkSurroundingBoundsDirty();
		pGhostedRenderable->AddEFlags( EFL_DIRTY_ABSTRANSFORM | EFL_DIRTY_SPATIAL_PARTITION );

		g_pClientLeafSystem->DisableCachedRenderBounds( pGhostedRenderable->RenderHandle(), true );
		g_pClientLeafSystem->RenderableChanged( pGhostedRenderable->RenderHandle() );

		if( C_BaseEntity::IsAbsQueriesValid() )
		{
			pGhostedRenderable->CollisionProp()->UpdatePartition();
			//pGhostedRenderable->SetupBones( NULL, -1, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
		}
	}

	pGhost->MarkRenderHandleDirty();
	//pGhost->MarkForThreadedBoneSetup();
	pGhost->InvalidateBoneCache();
	pGhost->CollisionProp()->MarkPartitionHandleDirty();
	pGhost->CollisionProp()->MarkSurroundingBoundsDirty();
	pGhost->AddEFlags( EFL_DIRTY_ABSTRANSFORM | EFL_DIRTY_SPATIAL_PARTITION );

	g_pClientLeafSystem->DisableCachedRenderBounds( pGhost->RenderHandle(), true );
	g_pClientLeafSystem->RenderableChanged( pGhost->RenderHandle() );

	if( C_BaseEntity::IsAbsQueriesValid() )
	{
		pGhost->CollisionProp()->UpdatePartition();
		//pGhost->SetupBones( NULL, -1, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
	}
}
#endif //#if (GHOST_RENDERABLE_TEN_TON_HAMMER == 1)


C_PortalGhostRenderable::C_PortalGhostRenderable( C_Portal_Base2D *pOwningPortal, C_BaseEntity *pGhostSource, const VMatrix &matGhostTransform, float *pSharedRenderClipPlane, C_BasePlayer *pPlayer ) :
	m_hGhostedRenderable( pGhostSource ), 
	m_matGhostTransform( matGhostTransform ), 
	m_pSharedRenderClipPlane( pSharedRenderClipPlane ),
	m_pPortalExitRenderClipPlane( NULL ),
	m_hHoldingPlayer( pPlayer ),
	m_pOwningPortal( pOwningPortal )
{
	m_fRenderableRange[0] = -FLT_MAX;
	m_fRenderableRange[1] = FLT_MAX;
	m_fNoTransformBeforeTime = -FLT_MAX;
	m_fDisablePositionChecksUntilTime = -FLT_MAX;

#if( DEBUG_GHOSTRENDERABLES == 1 )
	if( pOwningPortal->m_bIsPortal2 )
	{
		m_iDebugColor[0] = 255;
		m_iDebugColor[1] = 0;
		m_iDebugColor[2] = 0;				
	}
	else
	{
		m_iDebugColor[0] = 0;
		m_iDebugColor[1] = 0;
		m_iDebugColor[2] = 255;
	}
	m_iDebugColor[3] = 16;
#endif

	m_bSourceIsBaseAnimating = (dynamic_cast<C_BaseAnimating *>(pGhostSource) != NULL);

	RenderWithViewModels( pGhostSource->IsRenderingWithViewModels() );
	SetModelName( m_hGhostedRenderable->GetModelName() );

	m_bCombatWeapon = (dynamic_cast<C_BaseCombatWeapon *>(pGhostSource) != NULL);
	SetModelIndex( m_bCombatWeapon ? ((C_BaseCombatWeapon *)pGhostSource)->GetWorldModelIndex() : pGhostSource->GetModelIndex() );

	m_bCombatWeaponWorldClone = ( dynamic_cast< C_CombatWeaponClone* >( pGhostSource ) != NULL );

	m_bPlayerHeldClone = ( dynamic_cast< C_PlayerHeldObjectClone* >( pGhostSource ) != NULL );

	SetSize( pGhostSource->CollisionProp()->OBBMins(), pGhostSource->CollisionProp()->OBBMaxs() );

	Assert( m_hGhostedRenderable.Get() != NULL );
}

C_PortalGhostRenderable::~C_PortalGhostRenderable( void )
{
	m_hGhostedRenderable = NULL;
}

void C_PortalGhostRenderable::UpdateOnRemove( void )
{
	m_hGhostedRenderable = NULL;
	BaseClass::UpdateOnRemove();
}

void C_PortalGhostRenderable::PerFrameUpdate( void )
{
	C_BaseEntity *pGhostedRenderable = m_hGhostedRenderable;

	if( !pGhostedRenderable )
		return;

	SetModelName( pGhostedRenderable->GetModelName() );

	SetModelIndex( m_bCombatWeapon ? ((C_BaseCombatWeapon *)pGhostedRenderable)->GetWorldModelIndex() : pGhostedRenderable->GetModelIndex() );
	SetEffects( pGhostedRenderable->GetEffects() | EF_NOINTERP );		
	m_flAnimTime = pGhostedRenderable->m_flAnimTime;		

	if( m_bSourceIsBaseAnimating && !m_bCombatWeapon )
	{
		C_BaseAnimating *pSource = (C_BaseAnimating *)pGhostedRenderable;
		SetCycle( pSource->GetCycle() );
		SetSequence( pSource->GetSequence() );
		SetBody( pSource->GetBody() );
		SetSkin( pSource->GetSkin() );
	}

	SetSize( pGhostedRenderable->CollisionProp()->OBBMins(), pGhostedRenderable->CollisionProp()->OBBMaxs() );

	// Set position and angles relative to the object it's ghosting
	Vector ptNewOrigin;
	QAngle qNewAngles;

	if( GhostedRenderableOriginTime( pGhostedRenderable, gpGlobals->curtime ) >= m_fNoTransformBeforeTime )
	{	
		ptNewOrigin = m_matGhostTransform * pGhostedRenderable->GetNetworkOrigin();		
		qNewAngles = TransformAnglesToWorldSpace( pGhostedRenderable->GetNetworkAngles(), m_matGhostTransform.As3x4() );
	}
	else
	{
		ptNewOrigin = pGhostedRenderable->GetNetworkOrigin();
		qNewAngles = pGhostedRenderable->GetNetworkAngles();
	}

	SetNetworkOrigin( ptNewOrigin );
	SetLocalOrigin( ptNewOrigin );
	SetAbsOrigin( ptNewOrigin );
	SetNetworkAngles( qNewAngles );
	SetLocalAngles( qNewAngles );
	SetAbsAngles( qNewAngles );

	g_pClientLeafSystem->RenderableChanged( RenderHandle() );

#if (GHOST_RENDERABLE_TEN_TON_HAMMER == 1)
	GhostResetEverything( this );
#endif
}

Vector const& C_PortalGhostRenderable::GetRenderOrigin( void )
{
	C_BaseEntity *pGhostedRenderable = m_hGhostedRenderable;
	if( pGhostedRenderable == NULL )
		return m_ReferencedReturns.vRenderOrigin;

	if( GhostedRenderableOriginTime( pGhostedRenderable, gpGlobals->curtime ) < m_fNoTransformBeforeTime )
	{
		m_ReferencedReturns.vRenderOrigin = pGhostedRenderable->GetRenderOrigin();
	}
	else
	{
		m_ReferencedReturns.vRenderOrigin = m_matGhostTransform * pGhostedRenderable->GetRenderOrigin();
	}

	return m_ReferencedReturns.vRenderOrigin;
}

QAngle const& C_PortalGhostRenderable::GetRenderAngles( void )
{
	C_BaseEntity *pGhostedRenderable = m_hGhostedRenderable;
	if( pGhostedRenderable == NULL )
		return m_ReferencedReturns.qRenderAngle;

	if( GhostedRenderableOriginTime( pGhostedRenderable, gpGlobals->curtime ) < m_fNoTransformBeforeTime )
	{
		m_ReferencedReturns.qRenderAngle = pGhostedRenderable->GetRenderAngles();
	}
	else
	{
		m_ReferencedReturns.qRenderAngle = TransformAnglesToWorldSpace( pGhostedRenderable->GetRenderAngles(), m_matGhostTransform.As3x4() );
	}

	return m_ReferencedReturns.qRenderAngle;
}

bool C_PortalGhostRenderable::SetupBones( matrix3x4a_t *pBoneToWorldOut, int nMaxBones, int boneMask, float currentTime )
{
	C_BaseEntity *pGhostedRenderable = m_hGhostedRenderable;
	if( pGhostedRenderable == NULL )
		return false;

	int iOldModelIndex = 0, iWorldModelIndex = 0;
	bool bChangeModelIndex = m_bCombatWeapon && 
								((iOldModelIndex = ((C_BaseCombatWeapon *)pGhostedRenderable)->GetModelIndex()) != (iWorldModelIndex = ((C_BaseCombatWeapon *)pGhostedRenderable)->GetWorldModelIndex()));
	
	if( bChangeModelIndex )
	{
		((C_BaseCombatWeapon *)pGhostedRenderable)->SetModelIndex( iWorldModelIndex );
	}

	if( pGhostedRenderable->SetupBones( pBoneToWorldOut, nMaxBones, boneMask, currentTime ) )
	{
		if( pBoneToWorldOut && (GhostedRenderableOriginTime( pGhostedRenderable, currentTime ) >= m_fNoTransformBeforeTime) )
		{
			matrix3x4a_t matGhostTransform;
			matGhostTransform = m_matGhostTransform.As3x4();
			CStudioHdr *hdr = GetModelPtr();
			int nBoneCount = hdr->numbones();
			nBoneCount = MIN( nMaxBones, nBoneCount );
			for( int i = 0; i != nBoneCount; ++i )
			{
				ConcatTransforms_Aligned( matGhostTransform, pBoneToWorldOut[i], pBoneToWorldOut[i] );
			}
		}

		if( bChangeModelIndex )
		{
			((C_BaseCombatWeapon *)pGhostedRenderable)->SetModelIndex( iOldModelIndex );
		}
		return true;
	}

	if( bChangeModelIndex )
	{
		((C_BaseCombatWeapon *)pGhostedRenderable)->SetModelIndex( iOldModelIndex );
	}

	return false;
}

C_BaseAnimating *C_PortalGhostRenderable::GetBoneSetupDependancy( void )
{
	return m_bSourceIsBaseAnimating ? (C_BaseAnimating *)(m_hGhostedRenderable.Get()) : NULL;
}

void C_PortalGhostRenderable::GetRenderBounds( Vector& mins, Vector& maxs )
{
	if( m_hGhostedRenderable == NULL )
	{
		mins = maxs = vec3_origin;
		return;
	}

	m_hGhostedRenderable->GetRenderBounds( mins, maxs );
}

void C_PortalGhostRenderable::GetRenderBoundsWorldspace( Vector& mins, Vector& maxs )
{
	C_BaseEntity *pGhostedRenderable = m_hGhostedRenderable;
	if( pGhostedRenderable == NULL )
	{
		mins = maxs = vec3_origin;
		return;
	}

	if( GhostedRenderableOriginTime( pGhostedRenderable, gpGlobals->curtime ) < m_fNoTransformBeforeTime )
		return pGhostedRenderable->GetRenderBoundsWorldspace( mins, maxs );

	Vector vTempMins, vTempMaxs;
	pGhostedRenderable->GetRenderBoundsWorldspace( vTempMins, vTempMaxs );
	TransformAABB( m_matGhostTransform.As3x4(), vTempMins, vTempMaxs, mins, maxs );
}

bool C_PortalGhostRenderable::ShouldReceiveProjectedTextures( int flags )
{
	return false;
}

void C_PortalGhostRenderable::GetShadowRenderBounds( Vector &mins, Vector &maxs, ShadowType_t shadowType )
{
	C_BaseEntity *pGhostedRenderable = m_hGhostedRenderable;
	if( pGhostedRenderable == NULL )
	{
		mins = maxs = vec3_origin;
		return;
	}

	if( GhostedRenderableOriginTime( pGhostedRenderable, gpGlobals->curtime ) < m_fNoTransformBeforeTime )
		return pGhostedRenderable->GetShadowRenderBounds( mins, maxs, shadowType );

	Vector vTempMins, vTempMaxs;
	pGhostedRenderable->GetShadowRenderBounds( vTempMins, vTempMaxs, shadowType );
	TransformAABB( m_matGhostTransform.As3x4(), vTempMins, vTempMaxs, mins, maxs );
}

/*bool C_PortalGhostRenderable::GetShadowCastDistance( float *pDist, ShadowType_t shadowType ) const
{
	if( m_hGhostedRenderable == NULL )
		return false;

	return m_hGhostedRenderable->GetShadowCastDistance( pDist, shadowType );
}

bool C_PortalGhostRenderable::GetShadowCastDirection( Vector *pDirection, ShadowType_t shadowType ) const
{
	if( m_hGhostedRenderable == NULL )
		return false;

	if( m_hGhostedRenderable->GetShadowCastDirection( pDirection, shadowType ) )
	{
		if( pDirection )
			*pDirection = m_matGhostTransform.ApplyRotation( *pDirection );

		return true;
	}
	return false;
}*/

const matrix3x4_t & C_PortalGhostRenderable::RenderableToWorldTransform()
{
	C_BaseEntity *pGhostedRenderable = m_hGhostedRenderable;
	if( pGhostedRenderable == NULL )
		return m_ReferencedReturns.matRenderableToWorldTransform;

	if( GhostedRenderableOriginTime( pGhostedRenderable, gpGlobals->curtime ) < m_fNoTransformBeforeTime )
	{
		m_ReferencedReturns.matRenderableToWorldTransform = pGhostedRenderable->RenderableToWorldTransform();
	}
	else
	{
		ConcatTransforms( m_matGhostTransform.As3x4(), pGhostedRenderable->RenderableToWorldTransform(), m_ReferencedReturns.matRenderableToWorldTransform );
	}

	return m_ReferencedReturns.matRenderableToWorldTransform;
}

bool C_PortalGhostRenderable::GetAttachment( int number, Vector &origin, QAngle &angles )
{
	C_BaseEntity *pGhostedRenderable = m_hGhostedRenderable;
	if( pGhostedRenderable == NULL )
		return false;

	if( GhostedRenderableOriginTime( pGhostedRenderable, gpGlobals->curtime ) < m_fNoTransformBeforeTime )
		return pGhostedRenderable->GetAttachment( number, origin, angles );

	if( pGhostedRenderable->GetAttachment( number, origin, angles ) )
	{
		origin = m_matGhostTransform * origin;
		angles = TransformAnglesToWorldSpace( angles, m_matGhostTransform.As3x4() );
		return true;
	}
	return false;
}

bool C_PortalGhostRenderable::GetAttachment( int number, matrix3x4_t &matrix )
{
	C_BaseEntity *pGhostedRenderable = m_hGhostedRenderable;
	if( pGhostedRenderable == NULL )
		return false;

	if( GhostedRenderableOriginTime( pGhostedRenderable, gpGlobals->curtime ) < m_fNoTransformBeforeTime )
		return pGhostedRenderable->GetAttachment( number, matrix );

	if( pGhostedRenderable->GetAttachment( number, matrix ) )
	{
		ConcatTransforms( m_matGhostTransform.As3x4(), matrix, matrix );
		return true;
	}
	return false;
}

bool C_PortalGhostRenderable::GetAttachment( int number, Vector &origin )
{
	C_BaseEntity *pGhostedRenderable = m_hGhostedRenderable;
	if( pGhostedRenderable == NULL )
		return false;

	if( GhostedRenderableOriginTime( pGhostedRenderable, gpGlobals->curtime ) < m_fNoTransformBeforeTime )
		return pGhostedRenderable->GetAttachment( number, origin );

	if( pGhostedRenderable->GetAttachment( number, origin ) )
	{
		origin = m_matGhostTransform * origin;
		return true;
	}
	return false;
}

bool C_PortalGhostRenderable::GetAttachmentVelocity( int number, Vector &originVel, Quaternion &angleVel )
{
	C_BaseEntity *pGhostedRenderable = m_hGhostedRenderable;
	if( pGhostedRenderable == NULL )
		return false;

	if( GhostedRenderableOriginTime( pGhostedRenderable, gpGlobals->curtime ) < m_fNoTransformBeforeTime )
		return pGhostedRenderable->GetAttachmentVelocity( number, originVel, angleVel );

	Vector ghostVel;
	if( pGhostedRenderable->GetAttachmentVelocity( number, ghostVel, angleVel ) )
	{
		Vector3DMultiply( m_matGhostTransform, ghostVel, originVel );
		Vector3DMultiply( m_matGhostTransform, *(Vector*)( &angleVel ), *(Vector*)( &angleVel ) );
		return true;
	}
	return false;
}


bool C_PortalGhostRenderable::ShouldDrawForThisView( void )
{
	if ( portal_ghosts_disable.GetBool() )
		return false;

	C_BaseEntity *pGhostedRenderable = m_hGhostedRenderable.Get();
	if( pGhostedRenderable == NULL )
	{
		return false;
	}

	float fInterpTime = GhostedRenderableOriginTime( pGhostedRenderable, gpGlobals->curtime );

	if( (fInterpTime < m_fRenderableRange[0]) ||
		(fInterpTime >= m_fRenderableRange[1]) )
		return false;


	if ( m_bSourceIsBaseAnimating )
	{
		C_Portal_Player *pViewPlayer = ToPortalPlayer( GetSplitScreenViewPlayer() );
		C_Portal_Player *pHoldingPlayer = ToPortalPlayer( m_hHoldingPlayer.Get() );

		if ( pHoldingPlayer && (pHoldingPlayer == pViewPlayer) && !pViewPlayer->ShouldDrawLocalPlayer() )
		{
			if ( !pHoldingPlayer->IsAlive() )
			{
				// Dead player uses a ragdoll to draw, so don't ghost the dead entity
				return false;
			}
			else if( g_pPortalRender->GetViewRecursionLevel() == 0 )
			{
				if ( pHoldingPlayer->m_bEyePositionIsTransformedByPortal )
					return false;

				C_PlayerHeldObjectClone *pClone = NULL;
				if ( m_bPlayerHeldClone )
				{
					pClone = assert_cast< C_PlayerHeldObjectClone* >( m_hGhostedRenderable.Get() );
					if( pClone && pClone->m_bOnOppositeSideOfPortal )
						return false;
				}
			}
			else if ( g_pPortalRender->GetViewRecursionLevel() == 1 )
			{
				C_PlayerHeldObjectClone *pClone = NULL;
				if ( m_bPlayerHeldClone )
				{
					pClone = assert_cast< C_PlayerHeldObjectClone* >( m_hGhostedRenderable.Get() );
				}

				if ( (!pHoldingPlayer->m_bEyePositionIsTransformedByPortal && (g_pPortalRender->GetCurrentViewEntryPortal() == m_pOwningPortal)) && 
					!( pClone && pClone->m_bOnOppositeSideOfPortal ) )
					return false;
			}
		}
	}

	return true;
}


int C_PortalGhostRenderable::DrawModel( int flags, const RenderableInstance_t &instance )
{
	if( !ShouldDrawForThisView() )
		return 0;
	

#if( DEBUG_GHOSTRENDERABLES == 1 )
	if( m_iDebugColor[3] != 0 )
	{
		//NDebugOverlay::BoxAngles( GetRenderOrigin(), m_hGhostedRenderable->CollisionProp()->OBBMins(), m_hGhostedRenderable->CollisionProp()->OBBMaxs(), GetRenderAngles(), m_iDebugColor[0], m_iDebugColor[1], m_iDebugColor[2], m_iDebugColor[3], 0.0f );
		NDebugOverlay::Sphere( GetNetworkOrigin(), 5.0f, m_iDebugColor[0], m_iDebugColor[1], m_iDebugColor[2], true, 0.0f );
	}
#endif

	if ( m_bSourceIsBaseAnimating )
	{
		return C_BaseAnimating::DrawModel( flags, instance );
	}
	else
	{
		DrawBrushModelMode_t mode = DBM_DRAW_ALL;
		if ( flags & STUDIO_TWOPASS )
		{
			mode = ( flags & STUDIO_TRANSPARENCY ) ? DBM_DRAW_TRANSLUCENT_ONLY : DBM_DRAW_OPAQUE_ONLY;
		}

		render->DrawBrushModelEx( m_hGhostedRenderable, 
								(model_t *)m_hGhostedRenderable->GetModel(), 
								GetRenderOrigin(), 
								GetRenderAngles(), 
								mode );
		
		return 1;
	}

	return 0;
}

ModelInstanceHandle_t C_PortalGhostRenderable::GetModelInstance()
{
	// HACK: This causes problems if the ghosted renderable is
	// has it's model instance destructed mid-render... this is currently
	// the case for the portalgun view model due to model switching
	// so we're not going to use the ghost's model instance for combat weapon ents.
	// This will only mean the decal state is wrong, the worldmodel doesn't get decals anyway.
	// Real fix would be to 'sync' the decal state between this and the ghosted ent, but 
	// this will fix it for now.
	if ( m_hGhostedRenderable && (m_bCombatWeapon == false) && (m_bCombatWeaponWorldClone == false) )
		return m_hGhostedRenderable->GetModelInstance();

	return BaseClass::GetModelInstance();
}



RenderableTranslucencyType_t C_PortalGhostRenderable::ComputeTranslucencyType( void ) 
{ 
	if ( m_hGhostedRenderable == NULL )
		return RENDERABLE_IS_OPAQUE;

	return m_hGhostedRenderable->ComputeTranslucencyType();
}

int C_PortalGhostRenderable::GetRenderFlags()
{
	if( m_hGhostedRenderable == NULL )
		return false;

	return m_hGhostedRenderable->GetRenderFlags();
}

/*const model_t* C_PortalGhostRenderable::GetModel( ) const
{
	if( m_hGhostedRenderable == NULL )
		return NULL;

	return m_hGhostedRenderable->GetModel();
}

int C_PortalGhostRenderable::GetBody()
{
	if( m_hGhostedRenderable == NULL )
		return 0;

	return m_hGhostedRenderable->GetBody();
}*/

void C_PortalGhostRenderable::GetColorModulation( float* color )
{
	if( m_hGhostedRenderable == NULL )
		return;

#if(DEBUG_GHOSTRENDERABLES == 1)
	if( color[3] != 0 )
	{
		color[0] = m_iDebugColor[0] / 255.0f;
		color[1] = m_iDebugColor[1] / 255.0f;
		color[2] = m_iDebugColor[2] / 255.0f;
		return;
	}
#endif

	return m_hGhostedRenderable->GetColorModulation( color );
}

/*ShadowType_t C_PortalGhostRenderable::ShadowCastType()
{
	if( m_hGhostedRenderable == NULL )
		return SHADOWS_NONE;

	return m_hGhostedRenderable->ShadowCastType();
}*/

int C_PortalGhostRenderable::LookupAttachment( const char *pAttachmentName )
{
	if( m_hGhostedRenderable == NULL )
		return -1;


	return m_hGhostedRenderable->LookupAttachment( pAttachmentName );
}

/*
int C_PortalGhostRenderable::GetSkin()
{
	if( m_hGhostedRenderable == NULL )
		return -1;


	return m_hGhostedRenderable->GetSkin();
}
*/

float *C_PortalGhostRenderable::GetRenderClipPlane( void )
{
	if( !ShouldDrawForThisView() ) //Fix for systems that do not support clip planes. The occluding depth box should be avoided if it's not going to be useful
		return NULL;

	if( m_pPortalExitRenderClipPlane && //have an exit portal special clip plane
		g_pPortalRender->IsRenderingPortal() &&  //rendering through a portal
		(g_pPortalRender->GetCurrentViewEntryPortal() == m_pOwningPortal) ) //we're being drawn on the exit side
	{
		return m_pPortalExitRenderClipPlane;
	}

	return m_pSharedRenderClipPlane;
}


//-----------------------------------------------------------------------------
// Handle recording for the SFM
//-----------------------------------------------------------------------------
void C_PortalGhostRenderable::GetToolRecordingState( KeyValues *msg )
{
	VPROF_BUDGET( "C_PortalGhostRenderable::GetToolRecordingState", VPROF_BUDGETGROUP_TOOLS );
	BaseClass::GetToolRecordingState( msg );

	C_Portal_Player *pViewPlayer = ToPortalPlayer( GetSplitScreenViewPlayer() );

	if ( m_hHoldingPlayer.Get() && m_hHoldingPlayer.Get() == pViewPlayer )
	{
		msg->SetInt( "worldmodel", 2 ); //world model that should only draw in third person
	}
}



bool C_PortalGhostRenderable::ShouldCloneEntity( C_BaseEntity *pEntity, C_Portal_Base2D *pPortal, bool bUsePositionChecks )
{
	if( pEntity->IsEffectActive( EF_NODRAW ) )
		return false;

	bool bIsMovable = false;

	C_BaseEntity *pMoveEntity = pEntity;
	MoveType_t moveType = MOVETYPE_NONE;

	//unmoveables and doors can never get halfway in the portal
	while ( pMoveEntity )
	{
		moveType = pMoveEntity->GetMoveType();

		if ( !( moveType == MOVETYPE_NONE || moveType == MOVETYPE_PUSH ) )
		{
			bIsMovable = true;
			pMoveEntity = NULL;
		}
		else
			pMoveEntity = pMoveEntity->GetMoveParent();
	}

	if ( !bIsMovable )
		return false;

	if( (moveType == MOVETYPE_NOCLIP) && dynamic_cast<C_Portal_Base2D *>(pEntity) != NULL ) //potentially "mobile" portals in p2 can use MOVETYPE_NOCLIP
		return false;

	Assert( dynamic_cast<C_Portal_Base2D *>(pEntity) == NULL ); //should have been killed with (pEntity->GetMoveType() == MOVETYPE_NONE) check. Infinite recursion is infinitely bad.

	if( ToBaseViewModel(pEntity) )
		return false; //avoid ghosting view models

	if( pEntity->IsRenderingWithViewModels() )
		return false; //avoid ghosting anything that draws with viewmodels 

	bool bActivePlayerWeapon = false;

	C_BaseCombatWeapon *pWeapon = ToBaseCombatWeapon( pEntity );
	if ( pWeapon )
	{
		C_Portal_Player *pPortalPlayer = ToPortalPlayer( pWeapon->GetOwner() );
		if ( pPortalPlayer ) 
		{
			if ( pWeapon->m_iState != WEAPON_IS_ACTIVE )
			{
				return false; // don't ghost player owned non active weapons
			}
			else
			{
				bActivePlayerWeapon = true;
			}
		}
	}

	// Weapon is not teleportable, and we don't want to make that function use a dynamic cast to test for it,
	// so do the simple thing and just ignore this check for player-held weapons
	if( !bActivePlayerWeapon && !CPortal_Base2D_Shared::IsEntityTeleportable( pEntity ) )
		return false;

	if( bUsePositionChecks )
	{
		Vector ptEntCenter = pEntity->WorldSpaceCenter();
		float fEntCenterDist = (pPortal->m_plane_Origin.normal.Dot( ptEntCenter ) - pPortal->m_plane_Origin.dist);

		if( fEntCenterDist < -5.0f )
		{
			//view model held objects don't actually teleport to the other side of a portal when their center crosses the plane.
			//Alternate test is to ensure that a line drawn from the player center to the clone intersects the portal
			C_Portal_Player *pHoldingPlayer = (C_Portal_Player *)GetPlayerHoldingEntity( pEntity );

			if( (pHoldingPlayer == NULL) || //not held
				!pHoldingPlayer->IsUsingVMGrab() || //not in ViewModel grab mode
				dynamic_cast<C_PlayerHeldObjectClone *>(pEntity) ) //clones actually handle the teleportation correctly on the client.
			{
				return false; //entity is behind the portal, most likely behind the wall the portal is placed on
			}
			else
			{				
				Vector vPlayerCenter = pHoldingPlayer->WorldSpaceCenter();
				float fPlayerCenterDist = (pPortal->m_plane_Origin.normal.Dot( vPlayerCenter ) - pPortal->m_plane_Origin.dist);

				if( fPlayerCenterDist < 0.0f )
					return false;

				float fTotalDist = fPlayerCenterDist - fEntCenterDist;
				if( fTotalDist == 0.0f ) //should be >= 5.0 at all times, but I've been bitten too many times by impossibly 0 cases
					return false;

				Vector vPlaneIntersect = (ptEntCenter * (fPlayerCenterDist/fTotalDist)) - (vPlayerCenter * (fEntCenterDist/fTotalDist));
				Vector vPortalCenterToIntersect = vPlaneIntersect - pPortal->m_ptOrigin;

				if( (fabs( vPortalCenterToIntersect.Dot( pPortal->m_vRight ) ) > pPortal->GetHalfWidth()) ||
					(fabs( vPortalCenterToIntersect.Dot( pPortal->m_vUp ) ) > pPortal->GetHalfHeight()) )
				{
					return false;
				}
				//else intersects on the portal quad and the test passes
			}
		}

		if ( bActivePlayerWeapon )
		{
			// Both the player AND the weapon must be in the portal hole.
			// 
			// We test the player's collision AABB against the portal hole because it's guaranteed not to intersect
			// multiple adjacent portal holes at the same time in a degenerate case.
			//
			// We test the player's weapon hitbox against the portal hole because it doesn't have a good collision AABB
			// and because we'll only bother to even do this test if the player's AABB is acutally inside the portal hole.
			//
			// If we only test the weapon, we get false positives (phantom renderables) when its bounds span multiple adjacent 
			// portal holes.
			if( !pPortal->m_PortalSimulator.EntityHitBoxExtentIsInPortalHole( pWeapon->GetOwner(), true /* bUseCollisionAABB */ ) || 
				!pPortal->m_PortalSimulator.EntityHitBoxExtentIsInPortalHole( (C_BaseAnimating*)pEntity, false /* bUseCollisionAABB */ ) )
				return false;
		}
		else if( pEntity->IsPlayer() )
		{
			if( !pPortal->m_PortalSimulator.EntityHitBoxExtentIsInPortalHole( (C_BaseAnimating*)pEntity, true /* bUseCollisionAABB */ ) )
				return false;
		}
		else
		{
			if( !pPortal->m_PortalSimulator.EntityIsInPortalHole( pEntity ) )
				return false;
		}
	}
	
	return true;
}


C_PortalGhostRenderable *C_PortalGhostRenderable::CreateGhostRenderable( C_BaseEntity *pEntity, C_Portal_Base2D *pPortal )
{
	Assert( ShouldCloneEntity( pEntity, pPortal, false ) );

	C_BasePlayer *pPlayerOwner = NULL;
	bool bRenderableIsPlayer = pEntity->IsPlayer();
	if ( bRenderableIsPlayer )
	{
		pPlayerOwner = ToBasePlayer( pEntity );
	}
	else
	{
		C_BaseCombatWeapon *pWeapon = ToBaseCombatWeapon( pEntity );
		if ( pWeapon )
		{
			C_Portal_Player *pOwningPlayer = ToPortalPlayer( pWeapon->GetOwner() );
			if ( pOwningPlayer )
			{
				pPlayerOwner = pOwningPlayer;
			}
		}
		else
		{
			C_PlayerHeldObjectClone *pClone = dynamic_cast< C_PlayerHeldObjectClone* >( pEntity );
			if ( pClone )
			{
				C_Portal_Player *pOwningPlayer = ToPortalPlayer( pClone->m_hPlayer );
				if ( pOwningPlayer )
				{
					pPlayerOwner = pOwningPlayer;
				}
			}
		}
	}

#if( DEBUG_GHOSTRENDERABLES == 1 )
	{
		Vector vTransformedOrigin;
		QAngle qTransformedAngles;
		VectorTransform( pEntity->GetRenderOrigin(), pPortal->m_matrixThisToLinked.As3x4(), vTransformedOrigin );
		qTransformedAngles = TransformAnglesToWorldSpace( pEntity->GetRenderAngles(), pPortal->m_matrixThisToLinked.As3x4() );
		NDebugOverlay::BoxAngles( vTransformedOrigin, pEntity->CollisionProp()->OBBMins(), pEntity->CollisionProp()->OBBMaxs(), qTransformedAngles, 0, 255, 0, 16, 0.0f );
	}
#endif

	C_PortalGhostRenderable *pNewGhost = new C_PortalGhostRenderable( pPortal,
																		pEntity, 
																		pPortal->m_matrixThisToLinked, 
																		bRenderableIsPlayer ? pPortal->m_fGhostRenderablesClipForPlayer : pPortal->m_fGhostRenderablesClip,
																		pPlayerOwner );

	if( !pNewGhost->InitializeAsClientEntity( pEntity->GetModelName(), false ) )
	{
		pNewGhost->Release();
		return NULL;
	}

	Assert( pNewGhost );
	if( pPlayerOwner )
	{
		pNewGhost->SetOwnerEntity( pPlayerOwner );
	}

	if( pNewGhost->m_pSharedRenderClipPlane == pPortal->m_fGhostRenderablesClipForPlayer )
	{
		pNewGhost->m_pPortalExitRenderClipPlane = pPortal->m_fGhostRenderablesClip;
	}

	pPortal->m_GhostRenderables.AddToTail( pNewGhost );

	{
		// HACK - I just copied the CClientTools::OnEntityCreated code here,
		// since the ghosts aren't really entities - they don't have an entindex,
		// they're not in the entitylist, and they get created during Simulate(),
		// which isn't valid for real entities, since it changes the simulate list
		// -jd
		if ( ToolsEnabled() && clienttools->IsInRecordingMode() )
		{
			// Send deletion message to tool interface
			KeyValues *kv = new KeyValues( "created" );
			HTOOLHANDLE h = clienttools->AttachToEntity( pNewGhost );
			ToolFramework_PostToolMessage( h, kv );

			kv->deleteThis();
		}
	}

	g_pClientLeafSystem->DisableCachedRenderBounds( pNewGhost->RenderHandle(), true );
	pNewGhost->PerFrameUpdate();

	return pNewGhost;
}

C_PortalGhostRenderable *C_PortalGhostRenderable::CreateInversion( C_PortalGhostRenderable *pSrc, C_Portal_Base2D *pSourcePortal, float fTime )
{
	if( !(pSourcePortal && pSrc) )
		return NULL;

	C_Portal_Base2D *pRemotePortal = pSourcePortal->m_hLinkedPortal.Get();
	if( !pRemotePortal )
		return NULL;
		
	C_BaseEntity *pRootEntity = pSrc->m_hGhostedRenderable;
	if( !pRootEntity )
		return NULL;

	C_PortalGhostRenderable *pNewGhost = NULL;

	//use existing if we already have one
	for( int i = 0; i != pRemotePortal->m_GhostRenderables.Count(); ++i )
	{
		if( pRemotePortal->m_GhostRenderables[i]->m_hGhostedRenderable == pRootEntity )
		{
			pNewGhost = pRemotePortal->m_GhostRenderables[i];

			//reset time based variables
			pNewGhost->m_fRenderableRange[0] = -FLT_MAX;
			pNewGhost->m_fRenderableRange[1] = FLT_MAX;
			pNewGhost->m_fNoTransformBeforeTime = -FLT_MAX;
			break;
		}
	}

	if( pNewGhost == NULL )
	{
		pNewGhost = new C_PortalGhostRenderable( pRemotePortal, pSrc->m_hGhostedRenderable, pRemotePortal->m_matrixThisToLinked, 
			pSrc->m_pSharedRenderClipPlane == pSourcePortal->m_fGhostRenderablesClipForPlayer ? pRemotePortal->m_fGhostRenderablesClipForPlayer : pRemotePortal->m_fGhostRenderablesClip, pSrc->m_hHoldingPlayer );

		if( !pNewGhost->InitializeAsClientEntity( pRootEntity->GetModelName(), false ) )
		{
			pNewGhost->Release();
			return NULL;
		}
		
		if( pSrc->m_hHoldingPlayer )
		{
			pNewGhost->SetOwnerEntity( pSrc->m_hHoldingPlayer );
		}

		if( pSrc->m_pPortalExitRenderClipPlane != NULL )
		{
			if( pSrc->m_pPortalExitRenderClipPlane == pSourcePortal->m_fGhostRenderablesClipForPlayer )
			{
				pNewGhost->m_pPortalExitRenderClipPlane = pRemotePortal->m_fGhostRenderablesClipForPlayer;
			}
			else if( pSrc->m_pPortalExitRenderClipPlane == pSourcePortal->m_fGhostRenderablesClip )
			{
				pNewGhost->m_pPortalExitRenderClipPlane = pRemotePortal->m_fGhostRenderablesClip;
			}
			else
			{
				Assert( false );
			}
		}

		pRemotePortal->m_GhostRenderables.AddToTail( pNewGhost );

#if( DEBUG_GHOSTRENDERABLES == 1 )
		{
			Vector vTransformedOrigin;
			QAngle qTransformedAngles;
			VectorTransform( pSrc->m_hGhostedRenderable->GetNetworkOrigin(), pRemotePortal->m_matrixThisToLinked.As3x4(), vTransformedOrigin );
			qTransformedAngles = TransformAnglesToWorldSpace( pSrc->m_hGhostedRenderable->GetNetworkAngles(), pRemotePortal->m_matrixThisToLinked.As3x4() );
			NDebugOverlay::BoxAngles( vTransformedOrigin, pSrc->m_hGhostedRenderable->CollisionProp()->OBBMins(), pSrc->m_hGhostedRenderable->CollisionProp()->OBBMaxs(), qTransformedAngles, 0, 255, 0, 16, 0.0f );
		}
#endif
	}

	//these times are in effective origin interpolator time m_hGhostedRenderable->GetOriginInterpolator().GetInterpolatedTime( m_hGhostedRenderable->GetEffectiveInterpolationCurTime() )
	pNewGhost->m_fRenderableRange[0] = fTime;
	pNewGhost->m_fRenderableRange[1] = FLT_MAX;
	pSrc->m_fRenderableRange[0] = -FLT_MAX;
	pSrc->m_fRenderableRange[1] = fTime;
	pNewGhost->m_fNoTransformBeforeTime = fTime;
	
	pSrc->m_fDisablePositionChecksUntilTime = fTime + (TICK_INTERVAL * 2.0f) + pSrc->m_hGhostedRenderable->GetOriginInterpolator().GetInterpolationAmount();
	pNewGhost->m_fDisablePositionChecksUntilTime = pSrc->m_fDisablePositionChecksUntilTime;

	g_pClientLeafSystem->DisableCachedRenderBounds( pNewGhost->RenderHandle(), true );
	pNewGhost->PerFrameUpdate();

	return pNewGhost;
}




