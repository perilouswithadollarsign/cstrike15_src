//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "basetempentity.h"
#include "cstrike15/cs_gamerules.h"
#include "playerdecals_signature.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Dispatches decal tempentity
//-----------------------------------------------------------------------------
class CTEPlayerDecal : public CBaseTempEntity
{
public:
	DECLARE_CLASS( CTEPlayerDecal, CBaseTempEntity );

					CTEPlayerDecal( const char *name );
	virtual			~CTEPlayerDecal( void );

	virtual void	Test( const Vector& current_origin, const QAngle& current_angles );
	
	DECLARE_SERVERCLASS();

public:
	CNetworkVar( int, m_nPlayer );
	CNetworkVector( m_vecOrigin );
	CNetworkVector( m_vecStart );
	CNetworkVector( m_vecRight );
	CNetworkVar( int, m_nEntity );
	CNetworkVar( int, m_nHitbox );
};

class CFEPlayerDecal : public CBaseEntity
{
public:
	DECLARE_CLASS( CFEPlayerDecal, CBaseEntity );

	CFEPlayerDecal() {
		static int s_nUniqueID = 0;
		m_nUniqueID = ++ s_nUniqueID;
		if ( s_nUniqueID >= ( (1<<16) - 1 ) )
			s_nUniqueID = 1;

		m_unAccountID = 0;
		m_unTraceID = 0;
		m_rtGcTime = 0;

		SetEffects( EF_NOINTERP );
		SetPredictionEligible( false );
		
		m_vecEndPos.Init();
		m_vecStart.Init();
		m_vecRight.Init();
		m_vecNormal.Init();
		m_nPlayer = 0;
		m_nEntity = 0;
		m_nHitbox = 0;
		m_nTintID = 0;

		m_flCreationTime = gpGlobals->curtime;

		m_nVersion = 0;
		for ( int k = 0; k < PLAYERDECALS_SIGNATURE_BYTELEN; ++ k ) m_ubSignature.Set( k, 0 );

		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( gpGlobals->curtime + PLAYERDECALS_DURATION_SOLID + PLAYERDECALS_DURATION_FADE2 );

		// Add us to the list
		s_arrFEPlayerDecals.AddToTail( this );

		// If we have too many then force expire older ones
		while ( s_arrFEPlayerDecals.Count() >
			PLAYERDECALS_LIMIT_COUNT
			)
		{
			CFEPlayerDecal *pOther = s_arrFEPlayerDecals.Head();
			s_arrFEPlayerDecals.RemoveMultipleFromHead( 1 );
			pOther->SetNextThink( gpGlobals->curtime );	// must be SUB_Remove
		}
	}
	virtual			~CFEPlayerDecal( void )
	{
		// Remove us from the list
		// Normally we would be first in the list anyways,
		// and even if we are not it's a small number of entries to scan
		( void ) s_arrFEPlayerDecals.FindAndRemove( this );
	}

	// Make sure clients get all the spray entities in the same order
	virtual int UpdateTransmitState() { return SetTransmitState( FL_EDICT_ALWAYS ); }

	DECLARE_SERVERCLASS();

public:
	CNetworkVar( int, m_nUniqueID );
	CNetworkVar( uint32, m_unAccountID );
	CNetworkVar( uint32, m_unTraceID );
	CNetworkVar( uint32, m_rtGcTime );
	CNetworkVector( m_vecEndPos );
	CNetworkVector( m_vecStart );
	CNetworkVector( m_vecRight );
	CNetworkVector( m_vecNormal );
	CNetworkVar( int, m_nPlayer );
	CNetworkVar( int, m_nEntity );
	CNetworkVar( int, m_nHitbox );
	CNetworkVar( float, m_flCreationTime );
	CNetworkVar( int, m_nTintID );
	CNetworkVar( uint8, m_nVersion );
	CNetworkArray( uint8, m_ubSignature, PLAYERDECALS_SIGNATURE_BYTELEN );

private:
	static CUtlVector< CFEPlayerDecal * > s_arrFEPlayerDecals;
};
CUtlVector< CFEPlayerDecal * > CFEPlayerDecal::s_arrFEPlayerDecals;

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CTEPlayerDecal::CTEPlayerDecal( const char *name ) :
	CBaseTempEntity( name )
{
	m_nPlayer = 0;
	m_vecOrigin.Init();
	m_vecStart.Init();
	m_vecRight.Init();
	m_nEntity = 0;
	m_nHitbox = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTEPlayerDecal::~CTEPlayerDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *current_origin - 
//			*current_angles - 
//-----------------------------------------------------------------------------
void CTEPlayerDecal::Test( const Vector& current_origin, const QAngle& current_angles )
{
	// Fill in data
	m_nPlayer = 1;
	m_nEntity = 0;
	m_vecOrigin = current_origin;

	Vector vecEnd;
	
	Vector forward;

	m_vecOrigin.GetForModify()[2] += 24;

	AngleVectors( current_angles, &forward );
	forward[2] = 0.0;
	VectorNormalize( forward );

	VectorMA( m_vecOrigin, 50.0, forward, m_vecOrigin.GetForModify() );
	VectorMA( m_vecOrigin, 1024.0, forward, vecEnd );

	trace_t tr;

	UTIL_TraceLine( m_vecOrigin, vecEnd, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );

	m_vecOrigin = tr.endpos;

	CBroadcastRecipientFilter filter;
	Create( filter, 0.0 );
}

IMPLEMENT_SERVERCLASS_ST(CTEPlayerDecal, DT_TEPlayerDecal)
	SendPropVector( SENDINFO(m_vecOrigin), -1, SPROP_COORD),
	SendPropVector( SENDINFO(m_vecStart), -1, SPROP_COORD),
	SendPropVector( SENDINFO(m_vecRight), -1, SPROP_COORD),
	SendPropInt( SENDINFO(m_nEntity), MAX_EDICT_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_nPlayer), -1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_nHitbox), 16, SPROP_UNSIGNED ), // this is the max number of static props that can be decalled
END_SEND_TABLE()

IMPLEMENT_SERVERCLASS_ST(CFEPlayerDecal, DT_FEPlayerDecal)
	SendPropInt( SENDINFO(m_nUniqueID), -1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_unAccountID), -1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_unTraceID), -1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_rtGcTime), -1, SPROP_UNSIGNED ),
	SendPropVector( SENDINFO(m_vecEndPos), -1, SPROP_NOSCALE),
	SendPropVector( SENDINFO(m_vecStart), -1, SPROP_NOSCALE),
	SendPropVector( SENDINFO(m_vecRight), -1, SPROP_NOSCALE),
	SendPropVector( SENDINFO(m_vecNormal), -1, SPROP_NOSCALE),
	SendPropInt( SENDINFO(m_nEntity), MAX_EDICT_BITS, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_nPlayer), -1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_nHitbox), 16, SPROP_UNSIGNED ), // this is the max number of static props that can be decalled
	SendPropInt( SENDINFO( m_nTintID ), -1, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO( m_flCreationTime ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_nVersion ), 3, SPROP_UNSIGNED ), // support versions 0..7 initially
	SendPropArray3( SENDINFO_ARRAY3( m_ubSignature ), SendPropInt( SENDINFO_ARRAY( m_ubSignature ), 8, SPROP_UNSIGNED ) ),
END_SEND_TABLE()
LINK_ENTITY_TO_CLASS( cfe_player_decal, CFEPlayerDecal );

void FE_PlayerDecal( CCSGameRules::ServerPlayerDecalData_t const &data, std::string const &signature )
{
	CFEPlayerDecal *pEnt = ( CFEPlayerDecal * ) CBaseEntity::Create( "cfe_player_decal", data.m_vecOrigin, vec3_angle );
	pEnt->m_unAccountID = data.m_unAccountID;
	pEnt->m_unTraceID = data.m_nTraceID;
	pEnt->m_rtGcTime = data.m_rtGcTime;
	pEnt->m_nPlayer = data.m_nPlayer;
	pEnt->m_vecEndPos = data.m_vecOrigin;
	pEnt->m_vecStart = data.m_vecStart;
	pEnt->m_vecRight = data.m_vecRight;
	pEnt->m_vecNormal = data.m_vecNormal;
	pEnt->m_nEntity = data.m_nEntity;
	pEnt->m_nHitbox = data.m_nHitbox;
	pEnt->m_nTintID = data.m_nTintID;
	pEnt->m_flCreationTime = data.m_flCreationTime;
	pEnt->m_nVersion = PLAYERDECALS_SIGNATURE_VERSION;
	if ( signature.size() == PLAYERDECALS_SIGNATURE_BYTELEN )
	{
		for ( int k = 0; k < PLAYERDECALS_SIGNATURE_BYTELEN; ++ k )
			pEnt->m_ubSignature.Set( k, signature[k] );
	}
}


// Singleton to fire TEPlayerDecal objects
static CTEPlayerDecal g_TEPlayerDecal( "Player Decal" );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pos - 
//			player - 
//			entity - 
//			index - 
//-----------------------------------------------------------------------------
void TE_PlayerDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, const Vector* start, const Vector* right, int player, int entity, int hitbox )
{
	g_TEPlayerDecal.m_vecOrigin		= *pos;
	g_TEPlayerDecal.m_vecStart		= *start;
	g_TEPlayerDecal.m_vecRight		= *right;
	g_TEPlayerDecal.m_nPlayer		= player;
	g_TEPlayerDecal.m_nEntity		= entity;
	g_TEPlayerDecal.m_nHitbox		= hitbox;

	// Send it over the wire
	g_TEPlayerDecal.Create( filter, delay );
}
