//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef WEAPON_C4_H
#define WEAPON_C4_H
#ifdef _WIN32
#pragma once
#endif


#include "weapon_csbase.h"
#include "utlvector.h"

#define NUM_BEEPS 7

#if defined( CLIENT_DLL )

	#define CC4 C_C4

#else

	// ------------------------------------------------------------------------------------------ //
	// CPlantedC4 class.
	// ------------------------------------------------------------------------------------------ //

	class CPlantedC4 : public CBaseAnimating
	{
	public:
		DECLARE_CLASS( CPlantedC4, CBaseAnimating );
		DECLARE_DATADESC();
		DECLARE_SERVERCLASS();

		DECLARE_PREDICTABLE();

		CPlantedC4();
		virtual ~CPlantedC4();

		virtual void Spawn();

		virtual int  UpdateTransmitState();
		virtual void SetTransmit( CCheckTransmitInfo *pInfo, bool bAlways );
		virtual int  ShouldTransmit( const CCheckTransmitInfo *pInfo );

		static CPlantedC4* ShootSatchelCharge( CCSPlayer *pevOwner, Vector vecStart, QAngle vecAngles );
		virtual void Precache();
		
		// Set these flags so CTs can use the C4 to disarm it.
		virtual int	ObjectCaps() { return BaseClass::ObjectCaps() | (FCAP_CONTINUOUS_USE | FCAP_ONOFF_USE); }

		void SetBombSiteIndex( int iIndex ){ m_iBombSiteIndex = iIndex;	}

		inline bool IsBombActive( void ) { return m_bBombTicking; }

		CCSPlayer* GetPlanter( void ) { return m_pPlanter; }
		void SetPlanter( CCSPlayer* player ) { m_pPlanter = player; }

		CCSPlayer* GetDefuser( void ) { return m_pBombDefuser; }

		void SetPlantedAfterPickup( bool plantedAfterPickup ) { m_bPlantedAfterPickup = plantedAfterPickup; }

	public:

		CNetworkVar( bool, m_bBombTicking );
		CNetworkVar( float, m_flC4Blow );

		COutputEvent m_OnBombDefused; 
		COutputEvent m_OnBombBeginDefuse; 
		COutputEvent m_OnBombDefuseAborted;

	protected:
		virtual void Init( CCSPlayer *pevOwner, Vector vecStart, QAngle vecAngles, bool	bTrainingPlacedByPlayer );
		// used for the training map where the map spawns the bomb and sets the timer manually
		virtual void ActivateSetTimerLength( float flTimerLength );

		void C4Think();

		bool			m_bTrainingPlacedByPlayer;
		bool			m_bHasExploded;

	private:
		
		// This becomes the think function when the timer has expired and it is about to explode.
		void DetonateThink();
		virtual void Explode( trace_t *pTrace, int bitsDamageType );

		void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

		// Replicate timer length to the client for effects
		CNetworkVar( float, m_flTimerLength );

		// Info for defusing.
		bool			m_bBeingDefused;
		CHandle<CCSPlayer> m_pBombDefuser;
		float			m_fLastDefuseTime;
		int				m_iBombSiteIndex;

		CNetworkVar( float, m_flDefuseLength );		//How long does the defuse take? Depends on if a defuser was used
		CNetworkVar( float, m_flDefuseCountDown );	//What time does the defuse complete?
		CNetworkVar( bool, m_bBombDefused ); 
		CNetworkVar( CHandle<CCSPlayer>, m_hBombDefuser );

		// Control panel
		void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
		void GetControlPanelClassName( int nPanelIndex, const char *&pPanelName );
		void SpawnControlPanels( void );
		void RemoveControlPanels( void );

		typedef CHandle<CVGuiScreen>	ScreenHandle_t;
		CUtlVector<ScreenHandle_t>	m_hScreens;

		int m_iProgressBarTime;
		bool m_bVoiceAlertFired;

		// [tj] We need to store who planted the bomb so we can track who deserves credits for the kills
		CHandle<CCSPlayer>  m_pPlanter;

		// [tj] We need to know if this was planted by a player who recovered the bomb
		bool m_bPlantedAfterPickup;
	};

	// ------------------------------------------------------------------------------------------ //
	// CPlantedC4 class.
	// ------------------------------------------------------------------------------------------ //

	class CPlantedC4Training : public CPlantedC4
	{
	public:
		DECLARE_CLASS( CPlantedC4Training, CPlantedC4 );
		DECLARE_DATADESC();

		//DECLARE_SERVERCLASS();

		DECLARE_PREDICTABLE();

		//CPlantedC4Training();
		//virtual ~CPlantedC4Training();
		void	InputActivateSetTimerLength( inputdata_t &inputdata );

		COutputEvent m_OnBombExploded;	//Fired when the bomb explodes

	protected:
		//virtual void Init( CCSPlayer *pevOwner, Vector vecStart, QAngle vecAngles );
		virtual void Explode( trace_t *pTrace, int bitsDamageType );
	};

	extern CUtlVector< CPlantedC4* > g_PlantedC4s;



#endif

#define WEAPON_C4_CLASSNAME "weapon_c4"
#define PLANTED_C4_CLASSNAME "planted_c4"
#define PLANTED_C4TRAINING_CLASSNAME "planted_c4_training"

class CC4 : public CWeaponCSBase
{
public:
	DECLARE_CLASS( CC4, CWeaponCSBase );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
	CC4();
	virtual ~CC4();
	
	virtual void Spawn();

	void ItemPostFrame();
	virtual void WeaponReset( void );
	virtual void PrimaryAttack();
	virtual void WeaponIdle();
	virtual void UpdateShieldState( void );
	virtual float GetMaxSpeed() const;

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_C4; }

	virtual bool			Deploy( void );								// returns true is deploy was successful
	virtual bool			Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

	#ifdef CLIENT_DLL
	
		void ClientThink( void );
		virtual void	OnDataChanged( DataUpdateType_t type );
		virtual void	UpdateOnRemove( void );
		virtual bool OnFireEvent( C_BaseViewModel *pViewModel, const Vector& origin, const QAngle& angles, int event, const char *options );
		char *GetScreenText( void );
		char m_szScreenText[32];

		CUtlReference<CNewParticleEffect> m_hC4LED;
		void CreateLEDEffect( void );
		void RemoveLEDEffect( void );
		EHANDLE		   m_hParticleEffectOwner;

		virtual Vector GetGlowColor( void ) { return Vector( (240.0f/255.0f), (225.0f/255.0f), (90.0f/255.0f) ); }
	#else
		virtual void Precache();
		virtual int  UpdateTransmitState();
		virtual int  ShouldTransmit( const CCheckTransmitInfo *pInfo );
		virtual void GetControlPanelInfo( int nPanelIndex, const char *&pPanelName );
		virtual unsigned int PhysicsSolidMaskForEntity( void ) const;

		virtual bool ShouldRemoveOnRoundRestart();

        void SetDroppedFromDeath (bool droppedFromDeath) { m_bDroppedFromDeath = droppedFromDeath; }
	
		void Think( void );
		void ResetToLastValidPlayerHeldPosition();
		virtual void PhysicsTouchTriggers(const Vector *pPrevAbsOrigin = NULL);

private:
		Vector m_vecLastValidPlayerHeldPosition;
public:

	#endif

	void AbortBombPlant();

	void PlayArmingBeeps( void );
	void PlayPlantInitSound( void );
	virtual void	OnPickedUp( CBaseCombatCharacter *pNewOwner );
	virtual void	Drop( const Vector &vecVelocity );

	CNetworkVar( bool, m_bStartedArming );
	CNetworkVar( float, m_fArmedTime );
	CNetworkVar( bool, m_bBombPlacedAnimation );
	CNetworkVar( bool, m_bShowC4LED );
	CNetworkVar( bool, m_bIsPlantingViaUse );

	virtual bool IsRemoveable( void ) { return false; }

private:	
	bool m_bPlayedArmingBeeps[NUM_BEEPS];
	bool m_bBombPlanted;

	// [tj] we want to store if this bomb was dropped because the original owner was killed
	bool m_bDroppedFromDeath;

private:
	
	CC4( const CC4 & );
};


#endif // WEAPON_C4_H
