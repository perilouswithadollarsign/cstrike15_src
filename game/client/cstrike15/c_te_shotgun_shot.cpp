//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "fx_cs_shared.h"
#include "c_cs_player.h"
#include "c_basetempentity.h"
#include <cliententitylist.h>

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

class C_TEFireBullets : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEFireBullets, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	C_TEFireBullets();

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	int				m_iPlayer;
	uint16			m_nItemDefIndex;
	Vector			m_vecOrigin;
	QAngle			m_vecAngles;
	CSWeaponID		m_iWeaponID;
	int				m_iMode;
	int				m_iSeed;
	float			m_fInaccuracy;
	float			m_flRecoilIndex;
	float			m_fSpread;
#if defined( WEAPON_FIRE_BULLETS_ACCURACY_FISHTAIL_FEATURE )
	float			m_fAccuracyFishtail;
#endif
	WeaponSound_t	m_iSoundType;
};

C_TEFireBullets::C_TEFireBullets()
{
	m_iPlayer = 0;
	m_nItemDefIndex = 0;
	m_iWeaponID = WEAPON_NONE;
	m_iMode = 0;
	m_iSeed = 0;
	m_fInaccuracy = 0;
	m_flRecoilIndex = 0;
	m_fSpread = 0;
	m_iSoundType = EMPTY;
}

void C_TEFireBullets::PostDataUpdate( DataUpdateType_t updateType )
{
	// Create the effect.

	//
	// Vitaliy: Aug-2013 economy update note
	// fields m_nItemDefIndex and m_iSoundType are new and are not
	// networked in the previous demo recordings, force the sound type
	// to the default SINGLE value if it didn't network correctly
	//
	Assert(
		( m_iSoundType == EMPTY ) ||	// pre-economy update ver. 1.23.1.4
		( m_iSoundType == SINGLE ) ||	// default sound for firing
		( m_iSoundType == SPECIAL1 ) ||	// silenced weapons use this sound
		0 );
	WeaponSound_t wpnSoundToUse = SINGLE;
	switch ( m_iSoundType )
	{
	case SPECIAL1:
		wpnSoundToUse = SPECIAL1;
		break;
	}
	
	m_vecAngles.z = 0;
	
	FX_FireBullets( 
		m_iPlayer+1,
		m_nItemDefIndex,
		m_vecOrigin,
		m_vecAngles,
		m_iWeaponID,
		m_iMode,
		m_iSeed,
		m_fInaccuracy,
		m_fSpread,
#if defined( WEAPON_FIRE_BULLETS_ACCURACY_FISHTAIL_FEATURE )
		m_fAccuracyFishtail,
#else
		0.0f,
#endif
		0,
		wpnSoundToUse,
		m_flRecoilIndex
		);
}


IMPLEMENT_CLIENTCLASS_EVENT( C_TEFireBullets, DT_TEFireBullets, CTEFireBullets );


BEGIN_RECV_TABLE_NOBASE(C_TEFireBullets, DT_TEFireBullets)
	RecvPropVector( RECVINFO( m_vecOrigin ) ),
	RecvPropFloat( RECVINFO( m_vecAngles[0] ) ),
	RecvPropFloat( RECVINFO( m_vecAngles[1] ) ),
	RecvPropInt( RECVINFO( m_iWeaponID ) ),
	RecvPropInt( RECVINFO( m_iMode ) ), 
	RecvPropInt( RECVINFO( m_iSeed ) ),
	RecvPropInt( RECVINFO( m_iPlayer ) ),
	RecvPropFloat( RECVINFO( m_fInaccuracy ) ),
	RecvPropFloat( RECVINFO( m_fSpread ) ),
	RecvPropInt( RECVINFO( m_nItemDefIndex ) ),
	RecvPropInt( RECVINFO( m_iSoundType ) ),
#if defined( WEAPON_FIRE_BULLETS_ACCURACY_FISHTAIL_FEATURE )
	RecvPropFloat( RECVINFO( m_fAccuracyFishtail) ),
#endif
	RecvPropFloat( RECVINFO( m_flRecoilIndex ) ),
END_RECV_TABLE()


class C_TEPlantBomb : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlantBomb, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	int		m_iPlayer;
	Vector	m_vecOrigin;
	PlantBombOption_t	m_option;
};


void C_TEPlantBomb::PostDataUpdate( DataUpdateType_t updateType )
{
	// Create the effect.
	FX_PlantBomb( m_iPlayer+1, m_vecOrigin, m_option );
}


IMPLEMENT_CLIENTCLASS_EVENT( C_TEPlantBomb, DT_TEPlantBomb, CTEPlantBomb );


BEGIN_RECV_TABLE_NOBASE(C_TEPlantBomb, DT_TEPlantBomb)
	RecvPropVector( RECVINFO( m_vecOrigin ) ),
	RecvPropInt( RECVINFO( m_iPlayer ) ),
	RecvPropInt( RECVINFO( m_option ) ),
END_RECV_TABLE()


