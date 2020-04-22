//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BASECSGRENADE_PROJECTILE_H
#define BASECSGRENADE_PROJECTILE_H
#ifdef _WIN32
#pragma once
#endif


#include "basegrenade_shared.h"


#ifdef CLIENT_DLL
	#define CBaseCSGrenadeProjectile C_BaseCSGrenadeProjectile
#else
	class CCSWeaponInfo;
#endif


class CBaseCSGrenadeProjectile : public CBaseGrenade
{
public:
	DECLARE_CLASS( CBaseCSGrenadeProjectile, CBaseGrenade );
	DECLARE_NETWORKCLASS(); 

	virtual void Spawn();
	//virtual bool	IsGrenadeProjectile( void ) { return true; };

public:

	// This gets sent to the client and placed in the client's interpolation history
	// so the projectile starts out moving right off the bat.
	CNetworkVector( m_vInitialVelocity );
	CNetworkVar( int, m_nBounces );

#ifdef CLIENT_DLL
	CBaseCSGrenadeProjectile() {}
	CBaseCSGrenadeProjectile( const CBaseCSGrenadeProjectile& ) {}
	virtual ~CBaseCSGrenadeProjectile();
	virtual int DrawModel( int flags, const RenderableInstance_t &instance );
	virtual void PostDataUpdate( DataUpdateType_t type );
	virtual void	ClientThink( void );

	void CreateGrenadeTrail( void );

	float m_flSpawnTime;
	Vector vecLastTrailLinePos;
	float flNextTrailLineTime;
#else
	DECLARE_DATADESC();

	CBaseCSGrenadeProjectile() : m_pWeaponInfo(NULL), m_bDetonationRecorded(false) {}
	virtual void PostConstructor( const char *className );
	virtual ~CBaseCSGrenadeProjectile();

	virtual void Precache();

	virtual int				UpdateTransmitState();
	virtual int				ShouldTransmit( const CCheckTransmitInfo *pInfo );

	//Constants for all CS Grenades
	static inline float GetGrenadeGravity() { return 0.4f; }
	static inline const float GetGrenadeFriction() { return 0.2f; }
	static inline const float GetGrenadeElasticity() { return 0.45f; }

	//Think function to emit danger sounds for the AI
	void DangerSoundThink( void );

	virtual void OnBounced( void ) {}
	virtual void Explode( trace_t *pTrace, int bitsDamageType );
	
	virtual float GetShakeAmplitude( void ) { return 0.0f; }
	virtual void Splash();

	virtual GrenadeType_t GetGrenadeType( void ) { return GRENADE_TYPE_EXPLOSIVE; }

	// Specify what velocity we want the grenade to have on the client immediately.
	// Without this, the entity wouldn't have an interpolation history initially, so it would
	// sit still until it had gotten a few updates from the server.
	void SetupInitialTransmittedGrenadeVelocity( const Vector &velocity );

    // [jpaquin] give grenade projectiles a link back to the type
	// of weapon they are
	const CCSWeaponInfo *m_pWeaponInfo;

	enum
	{
		GRENADE_EXTINGUISHED_INFERNO = 1 << 0,
	};
	uint8				m_unOGSExtraFlags;			// Misc flags about the grenade's effect in game to report to ogs

	EHANDLE m_lastHitPlayer;

	void DetonateOnNextThink( void ) { m_flDetonateTime = 0.0f; }

	virtual unsigned int PhysicsSolidMaskForEntity( void ) const;

protected:

	//Set the time to detonate ( now + timer )
	void SetDetonateTimerLength( float timer );

	// Called when this grenade explodes. If your decended class overides some part of the detonation process
	// and prevents detonate from being called, you need to call this to make ogs still records it.
	void RecordDetonation( void );
	bool m_bDetonationRecorded;

private:	
	
	//Custom collision to allow for constant elasticity on hit surfaces
	virtual void ResolveFlyCollisionCustom( trace_t &trace, Vector &vecVelocity );
	
	float m_flDetonateTime;

#endif
};

#endif // BASECSGRENADE_PROJECTILE_H
