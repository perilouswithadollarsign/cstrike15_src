//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_triggers.h"
#include "in_buttons.h"
#include "c_func_brush.h"
#include "collisionutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_BaseTrigger, DT_BaseTrigger, CBaseTrigger )
	RecvPropBool( RECVINFO( m_bClientSidePredicted ) ),
	RecvPropInt( RECVINFO( m_spawnflags ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: Disables auto movement on players that touch it
//-----------------------------------------------------------------------------
class C_TriggerPlayerMovement : public C_BaseTrigger
{
public:
	DECLARE_CLASS( C_TriggerPlayerMovement, C_BaseTrigger );
	DECLARE_CLIENTCLASS();

	C_TriggerPlayerMovement();
	~C_TriggerPlayerMovement();

	void StartTouch( C_BaseEntity *pOther );
	void EndTouch( C_BaseEntity *pOther );

protected:

	virtual void UpdatePartitionListEntry();

public:
	C_TriggerPlayerMovement	*m_pNext;
};

IMPLEMENT_CLIENTCLASS_DT( C_TriggerPlayerMovement, DT_TriggerPlayerMovement, CTriggerPlayerMovement )
END_RECV_TABLE()

C_EntityClassList< C_TriggerPlayerMovement > g_TriggerPlayerMovementList;
template<> C_TriggerPlayerMovement *C_EntityClassList<C_TriggerPlayerMovement>::m_pClassList = NULL;

C_TriggerPlayerMovement::C_TriggerPlayerMovement()
{
	g_TriggerPlayerMovementList.Insert( this );
}

C_TriggerPlayerMovement::~C_TriggerPlayerMovement()
{
	g_TriggerPlayerMovementList.Remove( this );
}

//-----------------------------------------------------------------------------
// Little enumeration class used to try touching all triggers
//-----------------------------------------------------------------------------
template< class T >
class CFastTouchTriggers
{
public:
	CFastTouchTriggers( C_BaseEntity *pEnt, T *pTriggers ) : m_pEnt( pEnt ), m_pTriggers( pTriggers )
	{
		m_pCollide = pEnt->GetCollideable();
		m_nRequiredTriggerFlags = m_pCollide->GetRequiredTriggerFlags();
		Assert( m_pCollide );

		Vector vecMins, vecMaxs;
		CM_GetCollideableTriggerTestBox( m_pCollide, &vecMins, &vecMaxs );
		const Vector &vecStart = m_pCollide->GetCollisionOrigin();
		m_Ray.Init( vecStart, vecStart, vecMins, vecMaxs );
	}

	FORCEINLINE void CM_GetCollideableTriggerTestBox( ICollideable *pCollide, Vector *pMins, Vector *pMaxs )
	{
		if ( pCollide->GetSolid() == SOLID_BBOX )
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

	FORCEINLINE void Check( T *pEntity, bool bIgnoreTriggerSolidFlags )
	{
		// Hmmm.. everything in this list should be a trigger....
		ICollideable *pTriggerCollideable = pEntity->GetCollideable();
		int nTriggerSolidFlags = pTriggerCollideable->GetSolidFlags();
		if ( !bIgnoreTriggerSolidFlags
			&& m_nRequiredTriggerFlags && ( nTriggerSolidFlags & m_nRequiredTriggerFlags ) == m_nRequiredTriggerFlags )
			return;

		if ( nTriggerSolidFlags & FSOLID_USE_TRIGGER_BOUNDS )
		{
			Vector vecTriggerMins, vecTriggerMaxs;
			pTriggerCollideable->WorldSpaceTriggerBounds( &vecTriggerMins, &vecTriggerMaxs ); 
			if ( !IsBoxIntersectingRay( vecTriggerMins, vecTriggerMaxs, m_Ray ) )
			{
				return;
			}
		}
		else
		{
			trace_t tr;
			enginetrace->ClipRayToCollideable( m_Ray, MASK_SOLID, pTriggerCollideable, &tr );
			if ( !(tr.contents & MASK_SOLID) )
				return;
		}

		trace_t tr;
		UTIL_ClearTrace( tr );
		tr.endpos = (m_pEnt->GetAbsOrigin() + pEntity->GetAbsOrigin()) * 0.5;
		m_pEnt->PhysicsMarkEntitiesAsTouching( pEntity, tr );
	}

	FORCEINLINE void Run( bool bIgnoreTriggerSolidFlags = false )
	{
		for ( T *trigger = m_pTriggers; trigger ; trigger = trigger->m_pNext )
		{
			if ( trigger->IsDormant() )
				continue;
			Check( trigger, bIgnoreTriggerSolidFlags );
		}
	}

	Ray_t m_Ray;

private:
	C_BaseEntity			*m_pEnt;
	ICollideable			*m_pCollide;
	uint					m_nRequiredTriggerFlags;
	T						*m_pTriggers;
};

void TouchTriggerPlayerMovement( C_BaseEntity *pEntity )
{
	CFastTouchTriggers< C_TriggerPlayerMovement > helper( pEntity, g_TriggerPlayerMovementList.m_pClassList );
	helper.Run();
}


void C_TriggerPlayerMovement::UpdatePartitionListEntry()
{
	if ( !m_bClientSidePredicted )
	{
		BaseClass::UpdatePartitionListEntry();
		return;
	}

	::partition->RemoveAndInsert( 
		PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS,  // remove
		PARTITION_CLIENT_TRIGGER_ENTITIES,  // add
		CollisionProp()->GetPartitionHandle() );
}

void C_TriggerPlayerMovement::StartTouch( C_BaseEntity *pOther )
{	
	C_BasePlayer *pPlayer = ToBasePlayer( pOther );

	if ( !pPlayer )
		return;

	if ( HasSpawnFlags( SF_TRIGGER_AUTO_DUCK ) )
	{
		pPlayer->ForceButtons( IN_DUCK );
	}

	if ( HasSpawnFlags( SF_TRIGGER_AUTO_WALK ) )
	{
		pPlayer->ForceButtons( IN_SPEED );
	}

	// UNDONE: Currently this is the only operation this trigger can do
	if ( HasSpawnFlags( SF_TRIGGER_MOVE_AUTODISABLE ) )
	{
		pPlayer->m_Local.m_bAllowAutoMovement = false;
	}
}

void C_TriggerPlayerMovement::EndTouch( C_BaseEntity *pOther )
{
	C_BasePlayer *pPlayer = ToBasePlayer( pOther );
	if ( !pPlayer )
		return;

	if ( HasSpawnFlags( SF_TRIGGER_AUTO_DUCK ) )
	{
		pPlayer->UnforceButtons( IN_DUCK );
	}

	if ( HasSpawnFlags( SF_TRIGGER_AUTO_WALK ) )
	{
		pPlayer->UnforceButtons( IN_SPEED );
	}

	if ( HasSpawnFlags( SF_TRIGGER_MOVE_AUTODISABLE ) )
	{
		pPlayer->m_Local.m_bAllowAutoMovement = true;
	}
}


IMPLEMENT_CLIENTCLASS_DT( C_BaseVPhysicsTrigger, DT_BaseVPhysicsTrigger, CBaseVPhysicsTrigger )
	//RecvPropBool	( RECVINFO( m_bDisabled ) )
END_RECV_TABLE()


//////////////////////////////////////////////////////////////////////////
//
// Sound operator trigger
//

class C_TriggerSoundOperator : public C_BaseTrigger
{
public:
	DECLARE_CLASS( C_TriggerSoundOperator, C_BaseTrigger );
	DECLARE_CLIENTCLASS();

	C_TriggerSoundOperator();
	~C_TriggerSoundOperator();

	void StartTouch( C_BaseEntity *pOther );
	void EndTouch( C_BaseEntity *pOther );

protected:
	virtual void UpdatePartitionListEntry();

	int m_nSoundOperator;
	void UpdateSosVar( bool bTouchingNow );

public:
	C_TriggerSoundOperator	*m_pNext;
};

IMPLEMENT_CLIENTCLASS_DT( C_TriggerSoundOperator, DT_TriggerSoundOperator, CTriggerSoundOperator )
	RecvPropInt( RECVINFO( m_nSoundOperator ) ),
END_RECV_TABLE()

C_EntityClassList< C_TriggerSoundOperator > g_TriggerSoundOperators;
template<> C_TriggerSoundOperator *C_EntityClassList<C_TriggerSoundOperator>::m_pClassList = NULL;

C_TriggerSoundOperator::C_TriggerSoundOperator()
{
	g_TriggerSoundOperators.Insert( this );
	m_nSoundOperator = 0;
}

C_TriggerSoundOperator::~C_TriggerSoundOperator()
{
	g_TriggerSoundOperators.Remove( this );
}


static char const * g_szTriggerSoundOperatorVariables[] = {
	"sosVarCustomVolume1",
	"sosVarCustomVolume2"
};
static bool g_bTriggerSoundOperatorVariablesSet[ Q_ARRAYSIZE( g_szTriggerSoundOperatorVariables ) ] = {
};

void TouchTriggerSoundOperator( C_BaseEntity *pEntity )
{
	CFastTouchTriggers< C_TriggerSoundOperator > helper( pEntity, g_TriggerSoundOperators.m_pClassList );
	helper.Run( true );
}

void UntouchAllTriggerSoundOperator( C_BaseEntity *pEntity )
{
	for ( int j = 0; j < Q_ARRAYSIZE( g_szTriggerSoundOperatorVariables ); ++ j )
	{
		if ( g_bTriggerSoundOperatorVariablesSet[ j ] )
		{
			g_bTriggerSoundOperatorVariablesSet[ j ] = false;
			engine->SOSSetOpvarFloat( g_szTriggerSoundOperatorVariables[ j ], 0.0f );
		}
	}
}


void C_TriggerSoundOperator::UpdatePartitionListEntry()
{
	if ( !m_bClientSidePredicted )
	{
		BaseClass::UpdatePartitionListEntry();
		return;
	}

	::partition->RemoveAndInsert(
		PARTITION_CLIENT_SOLID_EDICTS | PARTITION_CLIENT_RESPONSIVE_EDICTS | PARTITION_CLIENT_NON_STATIC_EDICTS,  // remove
		PARTITION_CLIENT_TRIGGER_ENTITIES,  // add
		CollisionProp()->GetPartitionHandle() );
}

void C_TriggerSoundOperator::StartTouch( C_BaseEntity *pOther )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer || ( pOther != pLocalPlayer ) )
		return;

	UpdateSosVar( true );
}

void C_TriggerSoundOperator::EndTouch( C_BaseEntity *pOther )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer || ( pOther != pLocalPlayer ) )
		return;

	UpdateSosVar( false );
}

void C_TriggerSoundOperator::UpdateSosVar( bool bTouchingNow )
{
	int nVar = 0;
	if ( ( m_nSoundOperator >= 0 )
		&& ( m_nSoundOperator < Q_ARRAYSIZE( g_szTriggerSoundOperatorVariables ) ) )
		nVar = m_nSoundOperator;
	char const *szVar = g_szTriggerSoundOperatorVariables[ nVar ];
	g_bTriggerSoundOperatorVariablesSet[ nVar ] = bTouchingNow;
	engine->SOSSetOpvarFloat( szVar, bTouchingNow ? 1.0f : 0.0f );
}



