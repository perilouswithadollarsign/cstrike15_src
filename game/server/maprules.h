//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef MAPRULES_H
#define MAPRULES_H

class CRuleEntity : public CBaseEntity
{
	public:
	DECLARE_CLASS( CRuleEntity, CBaseEntity );

	void	Spawn( void );

	DECLARE_DATADESC();

	void	SetMaster( string_t iszMaster ) { m_iszMaster = iszMaster; }

	protected:
	bool	CanFireForActivator( CBaseEntity *pActivator );

	private:
	string_t	m_iszMaster;
};

// 
// CRulePointEntity -- base class for all rule "point" entities (not brushes)
//
class CRulePointEntity : public CRuleEntity
{
	public:
	DECLARE_DATADESC();
	DECLARE_CLASS( CRulePointEntity, CRuleEntity );

	int		m_Score;
	void		Spawn( void );
};

class CGameCoopMissionManager : public CRulePointEntity
{
	public:
	DECLARE_CLASS( CGameCoopMissionManager, CRulePointEntity );
	DECLARE_ENT_SCRIPTDESC();
	DECLARE_DATADESC();

	void	Spawn( void );
	bool	KeyValue( const char *szKeyName, const char *szValue );

	int		GetWaveNumber( void );
	void	SetWaveCompleted( void );
	void	SetRoundReset( void );
	void	SetSpawnsReset( void ); // used for spawns
	void	SetRoundLostKilled( void );
	void	SetRoundLostTime( void );
	void	SetMissionCompleted( void );

	//void InputGetWaveNumber( inputdata_t &inputdata );

	private:
	COutputEvent	m_OnWaveCompleted;
	COutputEvent	m_OnRoundReset;
	COutputEvent	m_OnSpawnsReset;
	COutputEvent	m_OnRoundLostKilled;
	COutputEvent	m_OnRoundLostTime;
	COutputEvent	m_OnMissionCompleted;
};

#define SF_PLAYEREQUIP_USEONLY				0x0001
#define SF_PLAYEREQUIP_STRIPFIRST			0x0002
#define SF_PLAYEREQUIP_ONLYSTRIPSAME		0x0004
#define MAX_EQUIP		32

class CGamePlayerEquip : public CRulePointEntity
{
	DECLARE_DATADESC();

	public:
	DECLARE_CLASS( CGamePlayerEquip, CRulePointEntity );

	//inputs
	void		InputTriggerForAllPlayers( inputdata_t &inputdata );
	void		InputTriggerForActivatedPlayer( inputdata_t &inputdata );

	bool		KeyValue( const char *szKeyName, const char *szValue );
	void		TriggerForAllPlayers( void );
	void		TriggerForActivatedPlayer( CBasePlayer *pPlayer, const char *szWeapon );
	void		Touch( CBaseEntity *pOther );
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );

	inline bool	UseOnly( void ) { return ( m_spawnflags & SF_PLAYEREQUIP_USEONLY ) ? true : false; }
	inline bool	StripFirst( void ) { return ( m_spawnflags & SF_PLAYEREQUIP_STRIPFIRST ) ? true : false; }
	inline bool	OnlyStripSameWeaponType( void ) { return ( m_spawnflags & SF_PLAYEREQUIP_ONLYSTRIPSAME ) ? true : false; }

	private:

	void		EquipPlayer( CBaseEntity *pPlayer, const char *szWeapon = NULL );

	string_t	m_weaponNames[MAX_EQUIP];
	int			m_weaponCount[MAX_EQUIP];
};

#endif		// MAPRULES_H

