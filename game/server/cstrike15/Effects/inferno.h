// Inferno.h
// An Inferno
// Author: Michael S. Booth, February 2005
// Copyright (c) 2005 Turtle Rock Studios, Inc. - All Rights Reserved

#ifndef _INFERNO_H_
#define _INFERNO_H_

#include "nav.h"			// for extent class - should be moved somewhere more general
#include "GameEventListener.h"

// Forward declaration
class CBasePlayer;

/**
 * The server-side Inferno entity.
 * This entity manages the growth of the flames and damage-dealing.
 * Keep this in sync with C_Inferno on the client, which manages the fire rendering
 */


enum
{
	INFERNO_TYPE_FIRE = 0,
	INFERNO_TYPE_INCGREN_FIRE,  // incendiary grenade fire, used to play a different sound
	INFERNO_TYPE_FIREWORKS,
};

class CInferno : public CBaseEntity, public CGameEventListener
{
public:
	DECLARE_CLASS( CInferno, CBaseEntity );
	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CInferno();
	virtual ~CInferno();

	virtual void Precache( void );

	virtual void Spawn( void );

	void SetMaxFlames( int nMaxFlames )		{ m_nMaxFlames = nMaxFlames; }

	virtual int UpdateTransmitState( void )
	{
		return FL_EDICT_ALWAYS;
	}

	void StartBurning( const Vector &pos, const Vector &normal, const Vector &velocity, int initialDepth = 0 );	// start the Inferno burning
	void InfernoThink( void );

	bool BShouldExtinguishSmokeGrenadeBounce( CBaseEntity *entity, Vector &posDropSmoke ) const;
	bool IsTouching( const Vector &from, const Vector &to, Vector *where = NULL ) const;	// return true if given ray intersects any fires, point of intersection in "where" if non-NULL
	bool IsTouching( const CNavArea *area ) const;	// return true if given area overlaps any fires

	bool WasCreatedInSmoke( void ) const { return m_bWasCreatedInSmoke; }

	virtual int		GetDamageType()	{ return DMG_BURN; }
	virtual float	GetDamagePerSecond();

	// I've moved some functionality into these member functions in order to 
	// make the insect swarm easier to implement (sjb)
	virtual const char *GetParticleEffectName();
	virtual const char *GetImpactParticleEffectName();
	virtual float		GetFlameLifetime() const;
	virtual bool		CanHarm( CBaseEntity *pEnt ) const { return true; }
	virtual float		GetFlameSpreadDelay() { return 0.0f; }

	int GetInfernoType() const		{ return m_nInfernoType; }
	void SetInfernoType( int type )	{ m_nInfernoType = type; }

	void SetSourceWeaponInfo( const CCSWeaponInfo* pWeaponInfo ) { m_pWeaponInfo = pWeaponInfo; }
	const CCSWeaponInfo* GetSourceWeaponInfo() const { return m_pWeaponInfo; }

protected:
	void FireGameEvent( IGameEvent *event );
	int ExtinguishFlamesAroundSmokeGrenade( Vector vecStart ); // returns count of fire patches extinguished
	void ExtinguishIndividualFlameBySmokeGrenade( int iFire, Vector vecStart );

	// Checks if all flames have expired and we should delete the inferno
	bool CheckExpired();

	// Marks covered areas as damaging so bots will avoid them
	void MarkCoveredAreaAsDamaging();

private:
	enum { MAX_INFERNO_FIRES = 64 };
	enum ECreateFireResult_t
	{
		k_ECreateFireResult_OK,
		k_ECreateFireResult_LimitExceeded,
		k_ECreateFireResult_AlreadyOnFire,
		k_ECreateFireResult_InSmoke,
		k_ECreateFireResult_AllSolid,
	};
	CNetworkArray( int, m_fireXDelta, MAX_INFERNO_FIRES );
	CNetworkArray( int, m_fireYDelta, MAX_INFERNO_FIRES );
	CNetworkArray( int, m_fireZDelta, MAX_INFERNO_FIRES );
	CNetworkArray( bool, m_bFireIsBurning, MAX_INFERNO_FIRES );
	CNetworkArray( Vector, m_BurnNormal, MAX_INFERNO_FIRES );
	CNetworkVar( int, m_fireCount );				// total number of flames spawned
	CNetworkVar( int, m_nInfernoType );

	bool m_bWasCreatedInSmoke;

	struct FireInfo
	{
		Vector m_pos;								// location of this fire
		Vector m_center;							// center of fire
		Vector m_normal;							// surface normal at this fire
		bool m_burning;
		int m_treeDepth;
		int m_spawnCount;
		FireInfo *m_parent;							// the fire that spawned us
		CountdownTimer m_spawnLifetime;				// how long we attempt to spawn new fires
		CountdownTimer m_spawnTimer;				// when we try to spawn a new fire
		CountdownTimer m_lifetime;					// lifetime of this fire
		float m_flWaterHeight;						// how much we were raised above water
	};

	bool IsFirePosInSmokeCloud( const Vector &pos ) const;
	void Spread( const Vector &spreadVelocity );
	ECreateFireResult_t CreateFire( const Vector &pos, const Vector &normal, FireInfo *parent, int depth );		// create an actual fire entity at the given position

	FireInfo *m_fire[ MAX_INFERNO_FIRES ];			// set of all active fires

	Extent m_extent;
	void RecomputeExtent( void );

	CountdownTimer m_damageTimer;
	CountdownTimer m_damageRampTimer;				// damage starts at zero and ramps up
	bool IsTouching( CBaseEntity *entity, float radius, bool checkLOS ) const;	// return true if position is in contact with a fire within the Inferno
	
	Vector m_splashVelocity;						// the velocity of the flame-causing incendiary that hit
	Vector m_startPos;								// the ignition point of the entire inferno

	IntervalTimer m_activeTimer;					// How long this inferno has been active.
	int m_fireSpawnOffset;

	int m_nMaxFlames;

	CountdownTimer m_BookkeepingTimer; // Only check spread / force expiry checks every 0.1s;
	CountdownTimer m_NextSpreadTimer;

	// the weapon info for the grenade type (if any) that spawned this inferno
	const CCSWeaponInfo* m_pWeaponInfo;
};

class CFireCrackerBlast : public CInferno
{
public:
	DECLARE_CLASS( CFireCrackerBlast, CInferno );
	DECLARE_SERVERCLASS();

	virtual void	Spawn();

	virtual const char *GetParticleEffectName();
	virtual const char *GetImpactParticleEffectName();
};


#endif // _INFERNO_H_
