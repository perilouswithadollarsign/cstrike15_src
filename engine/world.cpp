//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//
#include "quakedef.h"
#include "world.h"
#include "eiface.h"
#include "server.h"
#include "cmodel_engine.h"
#include "gl_model_private.h"
#include "sv_main.h"
#include "vengineserver_impl.h"
#include "collisionutils.h"
#include "vphysics_interface.h"
#include "ispatialpartitioninternal.h"
#include "staticpropmgr.h"
#include "shadowmgr.h"
#include "string_t.h"
#include "enginetrace.h"
#include "sys_dll.h"
#include "client.h"
#include "cdll_int.h"
#include "cdll_engine_int.h"
#include "icliententitylist.h"
#include "client_class.h"
#include "icliententity.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Method to convert edict to index
//-----------------------------------------------------------------------------
static inline int IndexOfEdict( edict_t* pEdict )
{
	return (int)(pEdict - sv.edicts);
}



//============================================================================


/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld (void)
{
	MDLCACHE_COARSE_LOCK_(g_pMDLCache);
	// Clean up static props from the previous level
#if !defined( DEDICATED )
	if ( !sv.IsDedicated() )
	{
		g_pShadowMgr->LevelShutdown();
	}
#endif // DEDICATED

	StaticPropMgr()->LevelShutdown();

	for ( int i = 0; i < 3; i++ )
	{
		if ( host_state.worldmodel->mins[i] < MIN_COORD_INTEGER || host_state.worldmodel->maxs[i] > MAX_COORD_INTEGER )
		{
			Host_EndGame(true, "Map coordinate extents are too large!!\nCheck for errors!\n" );
		}
	}
	SpatialPartition()->Init( host_state.worldmodel->mins, host_state.worldmodel->maxs );

	// Load all static props into the spatial partition
	StaticPropMgr()->LevelInit();

#if !defined( DEDICATED )
	if ( !sv.IsDedicated() )
	{
		g_pShadowMgr->LevelInit( host_state.worldbrush->numsurfaces );
	}
#endif // DEDICATED
}


//-----------------------------------------------------------------------------
// Trigger world-space bounds
//-----------------------------------------------------------------------------
static void CM_TriggerWorldSpaceBounds( ICollideable *pCollideable, Vector *pMins, Vector *pMaxs )
{
	if ( pCollideable->GetSolidFlags() & FSOLID_USE_TRIGGER_BOUNDS )
	{
		pCollideable->WorldSpaceTriggerBounds( pMins, pMaxs );
	}
	else
	{
		CM_WorldSpaceBounds( pCollideable, pMins, pMaxs );
	}
}

static void CM_GetCollideableTriggerTestBox( ICollideable *pCollide, Vector *pMins, Vector *pMaxs, bool bUseAccurateBbox )
{
	if ( bUseAccurateBbox && pCollide->GetSolid() == SOLID_BBOX )
	{
		*pMins = pCollide->OBBMins();
		*pMaxs = pCollide->OBBMaxs();
	}
	else
	{
		const Vector &vecStart = pCollide->GetCollisionOrigin();
		pCollide->WorldSpaceSurroundingBounds( pMins, pMaxs );
		*pMins -= vecStart;
		*pMaxs -= vecStart;
	}
}

//-----------------------------------------------------------------------------
// Little enumeration class used to try touching all triggers
//-----------------------------------------------------------------------------
class CTouchLinks : public IPartitionEnumerator
{
public:
	CTouchLinks( edict_t* pEnt, const Vector* pPrevAbsOrigin, bool accurateBboxTriggerChecks ) : m_TouchedEntities( 8, 8 )
	{
		m_pEnt = pEnt;
		m_pCollide = pEnt->GetCollideable();
		m_nRequiredTriggerFlags = m_pCollide->GetRequiredTriggerFlags();
		Assert( m_pCollide );

		Vector vecMins, vecMaxs;
		CM_GetCollideableTriggerTestBox( m_pCollide, &vecMins, &vecMaxs, accurateBboxTriggerChecks );
		const Vector &vecStart = m_pCollide->GetCollisionOrigin();

		if (pPrevAbsOrigin)
		{
			m_Ray.Init( *pPrevAbsOrigin, vecStart, vecMins, vecMaxs );
		}
		else
		{
			m_Ray.Init( vecStart, vecStart, vecMins, vecMaxs );
		}
	}

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
		if ( !m_nRequiredTriggerFlags )
			return ITERATION_CONTINUE;

		// Static props should never be in the trigger list 
		Assert( !StaticPropMgr()->IsStaticProp( pHandleEntity ) );

		IServerUnknown *pUnk = static_cast<IServerUnknown*>( pHandleEntity );
		Assert( pUnk );

		// Convert the IHandleEntity to an edict_t*...
		// Context is the thing we're testing everything against		
		edict_t* pTouch = pUnk->GetNetworkable()->GetEdict();

		// Can't bump against itself
		if ( pTouch == m_pEnt )
			return ITERATION_CONTINUE;

		IServerEntity *pTriggerEntity = pTouch->GetIServerEntity();
		if ( !pTriggerEntity )
			return ITERATION_CONTINUE;

		// Hmmm.. everything in this list should be a trigger....
		ICollideable *pTriggerCollideable = pTriggerEntity->GetCollideable();
		int nTriggerSolidFlags = pTriggerCollideable->GetSolidFlags();
		if ( (nTriggerSolidFlags & m_nRequiredTriggerFlags) == m_nRequiredTriggerFlags )
		{
			if ( nTriggerSolidFlags & FSOLID_USE_TRIGGER_BOUNDS )
			{
				Vector vecTriggerMins, vecTriggerMaxs;
				pTriggerCollideable->WorldSpaceTriggerBounds( &vecTriggerMins, &vecTriggerMaxs ); 
				if ( !IsBoxIntersectingRay( vecTriggerMins, vecTriggerMaxs, m_Ray ) )
					return ITERATION_CONTINUE;
			}
			else
			{
				trace_t tr;
				g_pEngineTraceServer->ClipRayToCollideable( m_Ray, MASK_SOLID, pTriggerCollideable, &tr );
				if ( !(tr.contents & MASK_SOLID) )
					return ITERATION_CONTINUE;
			}

			m_TouchedEntities.AddToTail( pTouch );
		}

		return ITERATION_CONTINUE;
	}

	void HandleTouchedEntities()
	{
		for ( int i = 0; i < m_TouchedEntities.Count(); ++i )
		{
			serverGameEnts->MarkEntitiesAsTouching( m_TouchedEntities[i], m_pEnt );
		}
	}

	Ray_t m_Ray;

private:
	edict_t *m_pEnt;
	ICollideable *m_pCollide;
	uint m_nRequiredTriggerFlags;
	CUtlVector< edict_t* > m_TouchedEntities;
};

//-----------------------------------------------------------------------------
// Little enumeration class used to try touching all triggers
//-----------------------------------------------------------------------------
class CTouchLinks_ClientSide : public IPartitionEnumerator
{
public:
	CTouchLinks_ClientSide( IClientEntity *pEnt, const Vector* pPrevAbsOrigin, bool accurateBboxTriggerChecks ) : m_TouchedEntities( 8, 8 )
	{
		m_pEnt = pEnt;
		m_pCollide = pEnt->GetCollideable();
		m_nRequiredTriggerFlags = m_pCollide->GetRequiredTriggerFlags();
		Assert( m_pCollide );

		Vector vecMins, vecMaxs;
		CM_GetCollideableTriggerTestBox( m_pCollide, &vecMins, &vecMaxs, accurateBboxTriggerChecks );
		const Vector &vecStart = m_pCollide->GetCollisionOrigin();

		if (pPrevAbsOrigin)
		{
			m_Ray.Init( *pPrevAbsOrigin, vecStart, vecMins, vecMaxs );
		}
		else
		{
			m_Ray.Init( vecStart, vecStart, vecMins, vecMaxs );
		}
	}

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
#ifndef DEDICATED
		if ( sv.IsDedicated() )
		{
			return ITERATION_CONTINUE;
		}

		if ( !m_nRequiredTriggerFlags )
			return ITERATION_CONTINUE;
		// Static props should never be in the trigger list 
		Assert( !StaticPropMgr()->IsStaticProp( pHandleEntity ) );

		IClientUnknown *pUnk = static_cast<IClientUnknown*>( pHandleEntity );
		Assert( pUnk );

		// Convert the IHandleEntity to an edict_t*...
		// Context is the thing we're testing everything against		
		IClientEntity *pTriggerEntity = pUnk->GetIClientEntity();

		// Can't bump against itself
		if ( pTriggerEntity == m_pEnt )
			return ITERATION_CONTINUE;

		// Hmmm.. everything in this list should be a trigger....
		ICollideable *pTriggerCollideable = pTriggerEntity->GetCollideable();
		int nTriggerSolidFlags = pTriggerCollideable->GetSolidFlags();
		if ( (nTriggerSolidFlags & m_nRequiredTriggerFlags) == m_nRequiredTriggerFlags )
		{
			if ( pTriggerCollideable->GetSolidFlags() & FSOLID_USE_TRIGGER_BOUNDS )
			{
				Vector vecTriggerMins, vecTriggerMaxs;
				pTriggerCollideable->WorldSpaceTriggerBounds( &vecTriggerMins, &vecTriggerMaxs ); 
				if ( !IsBoxIntersectingRay( vecTriggerMins, vecTriggerMaxs, m_Ray ) )
				{
					return ITERATION_CONTINUE;
				}
			}
			else
			{
				trace_t tr;
				g_pEngineTraceClient->ClipRayToCollideable( m_Ray, MASK_SOLID, pTriggerCollideable, &tr );
				if ( !(tr.contents & MASK_SOLID) )
					return ITERATION_CONTINUE;
			}
		}

		m_TouchedEntities.AddToTail( pTriggerEntity );
#endif

		return ITERATION_CONTINUE;
	}

	void HandleTouchedEntities()
	{
#ifndef DEDICATED
		if ( sv.IsDedicated() )
		{
			return;
		}

		for ( int i = 0; i < m_TouchedEntities.Count(); ++i )
		{
			g_ClientDLL->MarkEntitiesAsTouching( m_TouchedEntities[i], m_pEnt );
		}
#endif
	}

	Ray_t m_Ray;

private:
	IClientEntity *m_pEnt;
	ICollideable *m_pCollide;
	uint m_nRequiredTriggerFlags;
	CUtlVector< IClientEntity* > m_TouchedEntities;
};

// enumerator class that's used to update touch links for a trigger when 
// it moves or changes solid type
class CTriggerMoved : public IPartitionEnumerator
{
public:
	CTriggerMoved( bool accurateBboxTriggerChecks ) : m_TouchedEntities( 8, 8 )
	{
		m_bAccurateBBoxCheck = accurateBboxTriggerChecks;
	}

	void TriggerMoved( edict_t *pTriggerEntity )
	{
		m_pTriggerEntity = pTriggerEntity;
		m_pTrigger = pTriggerEntity->GetCollideable();
		m_triggerSolidFlags = m_pTrigger->GetSolidFlags();
		Vector vecAbsMins, vecAbsMaxs;
		CM_TriggerWorldSpaceBounds( m_pTrigger, &vecAbsMins, &vecAbsMaxs );
		SpatialPartition()->EnumerateElementsInBox( PARTITION_ENGINE_SOLID_EDICTS,
			vecAbsMins, vecAbsMaxs, false, this );
	}

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
#ifndef DEDICATED
		if ( sv.IsDedicated() )
		{
			return ITERATION_CONTINUE;
		}

		// skip static props, the game DLL doesn't care about them
		if ( StaticPropMgr()->IsStaticProp( pHandleEntity ) )
			return ITERATION_CONTINUE;

		IServerUnknown *pUnk = static_cast< IServerUnknown* >( pHandleEntity );
		Assert( pUnk );

		// Convert the user ID to and edict_t*...
		edict_t* pTouch = pUnk->GetNetworkable()->GetEdict();
		Assert( pTouch );
		ICollideable *pTouchCollide = pUnk->GetCollideable();

		// Can't ever touch itself because it's in the other list
		if ( pTouchCollide == m_pTrigger )
			return ITERATION_CONTINUE;

		int nReqFlags = pTouchCollide->GetRequiredTriggerFlags();
		if ( !nReqFlags || (nReqFlags & m_triggerSolidFlags) != nReqFlags )
			return ITERATION_CONTINUE;

		IServerEntity *serverEntity = pTouch->GetIServerEntity();
		if ( !serverEntity )
			return ITERATION_CONTINUE;

		// FIXME: Should we be using the surrounding bounds here?
		Vector vecMins, vecMaxs;
		CM_GetCollideableTriggerTestBox( pTouchCollide, &vecMins, &vecMaxs, m_bAccurateBBoxCheck );

		const Vector &vecStart = pTouchCollide->GetCollisionOrigin();
		Ray_t ray;
		ray.Init( vecStart, vecStart, vecMins, vecMaxs ); 

		if ( m_pTrigger->GetSolidFlags() & FSOLID_USE_TRIGGER_BOUNDS )
		{
			Vector vecTriggerMins, vecTriggerMaxs;
			m_pTrigger->WorldSpaceTriggerBounds( &vecTriggerMins, &vecTriggerMaxs ); 
			if ( !IsBoxIntersectingRay( vecTriggerMins, vecTriggerMaxs, ray ) )
			{
				return ITERATION_CONTINUE;
			}
		}
		else
		{
			trace_t tr;
			g_pEngineTraceServer->ClipRayToCollideable( ray, MASK_SOLID, m_pTrigger, &tr );
			if ( !(tr.contents & MASK_SOLID) )
				return ITERATION_CONTINUE;
		}

		m_TouchedEntities.AddToTail( pTouch );
#endif

		return ITERATION_CONTINUE;
	}

	void HandleTouchedEntities( )
	{
		for ( int i = 0; i < m_TouchedEntities.Count(); ++i )
		{
			serverGameEnts->MarkEntitiesAsTouching( m_TouchedEntities[i], m_pTriggerEntity );
		}
	}

private:
	edict_t* m_pTriggerEntity;
	ICollideable* m_pTrigger;
	int m_triggerSolidFlags;
	Vector m_vecDelta;
	CUtlVector< edict_t* > m_TouchedEntities;
	bool m_bAccurateBBoxCheck;
};

// enumerator class that's used to update touch links for a trigger when 
// it moves or changes solid type
class CTriggerMoved_ClientSide : public IPartitionEnumerator
{
public:
	CTriggerMoved_ClientSide( bool accurateBboxTriggerChecks ) : m_TouchedEntities( 8, 8 )
	{
		m_bAccurateBBoxCheck = accurateBboxTriggerChecks;
	}

	void TriggerMoved( IClientEntity *pTriggerEntity )
	{
		m_pTriggerEntity = pTriggerEntity;
		m_pTrigger = pTriggerEntity->GetCollideable();
		m_triggerSolidFlags = m_pTrigger->GetSolidFlags();
		Vector vecAbsMins, vecAbsMaxs;
		CM_TriggerWorldSpaceBounds( m_pTrigger, &vecAbsMins, &vecAbsMaxs );
		SpatialPartition()->EnumerateElementsInBox( PARTITION_CLIENT_SOLID_EDICTS,
			vecAbsMins, vecAbsMaxs, false, this );
	}

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{
#ifndef DEDICATED
		if ( sv.IsDedicated() )
		{
			return ITERATION_CONTINUE;
		}

		// skip static props, the game DLL doesn't care about them
		if ( StaticPropMgr()->IsStaticProp( pHandleEntity ) )
			return ITERATION_CONTINUE;

		IClientUnknown *pUnk = static_cast< IClientUnknown* >( pHandleEntity );
		Assert( pUnk );

		// Convert the user ID to and edict_t*...
		IClientEntity* pTouch = pUnk->GetIClientEntity();
		Assert( pTouch );
		ICollideable *pTouchCollide = pUnk->GetCollideable();

		// Can't ever touch itself because it's in the other list
		if ( pTouchCollide == m_pTrigger )
			return ITERATION_CONTINUE;

		int nReqFlags = pTouchCollide->GetRequiredTriggerFlags();
		if ( !nReqFlags || (nReqFlags & m_triggerSolidFlags) != nReqFlags )
			return ITERATION_CONTINUE;

		// FIXME: Should we be using the surrounding bounds here?
		Vector vecMins, vecMaxs;
		CM_GetCollideableTriggerTestBox( pTouchCollide, &vecMins, &vecMaxs, m_bAccurateBBoxCheck );

		const Vector &vecStart = pTouchCollide->GetCollisionOrigin();
		Ray_t ray;
		ray.Init( vecStart, vecStart, vecMins, vecMaxs ); 

		if ( m_pTrigger->GetSolidFlags() & FSOLID_USE_TRIGGER_BOUNDS )
		{
			Vector vecTriggerMins, vecTriggerMaxs;
			m_pTrigger->WorldSpaceTriggerBounds( &vecTriggerMins, &vecTriggerMaxs ); 
			if ( !IsBoxIntersectingRay( vecTriggerMins, vecTriggerMaxs, ray ) )
			{
				return ITERATION_CONTINUE;
			}
		}
		else
		{
			trace_t tr;
			g_pEngineTraceClient->ClipRayToCollideable( ray, MASK_SOLID, m_pTrigger, &tr );
			if ( !(tr.contents & MASK_SOLID) )
				return ITERATION_CONTINUE;
		}

		m_TouchedEntities.AddToTail( pTouch );
#endif

		return ITERATION_CONTINUE;
	}

	void HandleTouchedEntities( )
	{
#ifndef DEDICATED
		if ( sv.IsDedicated() )
		{
			return;
		}

		for ( int i = 0; i < m_TouchedEntities.Count(); ++i )
		{
			g_ClientDLL->MarkEntitiesAsTouching( m_TouchedEntities[i], m_pTriggerEntity );
		}
#endif
	}

private:
	IClientEntity* m_pTriggerEntity;
	ICollideable* m_pTrigger;
	int m_triggerSolidFlags;
	Vector m_vecDelta;
	CUtlVector< IClientEntity* > m_TouchedEntities;
	bool m_bAccurateBBoxCheck;
};

//-----------------------------------------------------------------------------
// Touches triggers. Or, if it is a trigger, causes other things to touch it
// returns true if untouch needs to be checked
//-----------------------------------------------------------------------------
void SV_TriggerMoved( edict_t *pTriggerEnt, bool accurateBboxTriggerChecks )
{
	CTriggerMoved triggerEnum( accurateBboxTriggerChecks );
	triggerEnum.TriggerMoved( pTriggerEnt ); 
	triggerEnum.HandleTouchedEntities( );
}

void CL_TriggerMoved( IClientEntity *pTriggerEnt, bool accurateBboxTriggerChecks )
{
	CTriggerMoved_ClientSide triggerEnum( accurateBboxTriggerChecks );
	triggerEnum.TriggerMoved( pTriggerEnt ); 
	triggerEnum.HandleTouchedEntities();
}


void SV_SolidMoved( edict_t *pSolidEnt, ICollideable *pSolidCollide, const Vector* pPrevAbsOrigin, bool accurateBboxTriggerChecks )
{
	if (!pPrevAbsOrigin)
	{
		CTouchLinks touchEnumerator(pSolidEnt, NULL, accurateBboxTriggerChecks);

		Vector vecWorldMins, vecWorldMaxs;
		pSolidCollide->WorldSpaceSurroundingBounds( &vecWorldMins, &vecWorldMaxs );

		SpatialPartition()->EnumerateElementsInBox( PARTITION_ENGINE_TRIGGER_EDICTS,
			vecWorldMins, vecWorldMaxs, false, &touchEnumerator );

		touchEnumerator.HandleTouchedEntities( );
	}
	else
	{
		CTouchLinks touchEnumerator(pSolidEnt, pPrevAbsOrigin, accurateBboxTriggerChecks);

		// A version that checks against an extruded ray indicating the motion
		SpatialPartition()->EnumerateElementsAlongRay( PARTITION_ENGINE_TRIGGER_EDICTS,
			touchEnumerator.m_Ray, false, &touchEnumerator );

		touchEnumerator.HandleTouchedEntities( );
	}
}

void CL_SolidMoved( IClientEntity *pTriggerEnt, ICollideable *pSolidCollide, const Vector* pPrevAbsOrigin, bool accurateBboxTriggerChecks )
{
	if (!pPrevAbsOrigin)
	{
		CTouchLinks_ClientSide touchEnumerator(pTriggerEnt, NULL, accurateBboxTriggerChecks);

		Vector vecWorldMins, vecWorldMaxs;
		pSolidCollide->WorldSpaceSurroundingBounds( &vecWorldMins, &vecWorldMaxs );

		SpatialPartition()->EnumerateElementsInBox( PARTITION_CLIENT_TRIGGER_ENTITIES,
			vecWorldMins, vecWorldMaxs, false, &touchEnumerator );

		touchEnumerator.HandleTouchedEntities();
	}
	else
	{
		CTouchLinks_ClientSide touchEnumerator(pTriggerEnt, pPrevAbsOrigin, accurateBboxTriggerChecks);

		// A version that checks against an extruded ray indicating the motion
		SpatialPartition()->EnumerateElementsAlongRay( PARTITION_CLIENT_TRIGGER_ENTITIES,
			touchEnumerator.m_Ray, false, &touchEnumerator );

		touchEnumerator.HandleTouchedEntities();
	}
}
