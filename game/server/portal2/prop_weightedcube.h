//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: First-class cube entity so we can query by type and generally make inferences
//			that are harder to do without an entity of that type.
//
//=====================================================================================//

#include "cbase.h"
#include "props.h"
#include "ai_utils.h"
#include "physics_saverestore.h"
#include "phys_controller.h"
#include "portal_base2d.h"
#include "datacache/imdlcache.h"
#include "func_portal_detector.h"

#include "player_pickup_paint_power_user.h"

DECLARE_AUTO_LIST( IPropWeightedCubeAutoList );

enum WeightedCubeType_e
{
	CUBE_STANDARD,
	CUBE_COMPANION,
	CUBE_REFLECTIVE,
	CUBE_SPHERE,
	CUBE_ANTIQUE,
	CUBE_SCHRODINGER,
};

// 
// Tip controller
//

class CCubeRotationController : public CPointEntity, public IMotionEvent
{
	DECLARE_CLASS( CCubeRotationController, CPointEntity );
	DECLARE_DATADESC();

public:

	~CCubeRotationController( void );
	void Spawn( void );
	void Activate( void );
	void Enable( bool state = true );
	void Suspend( float time );
	float SuspendedTill( void );
	void SetAlignmentVector( const Vector &vecAlign );
	Vector GetAlignmentVector( void ) { return m_worldGoalAxis; }

	void SuspendAfter( float flTime );

	bool Enabled( void );

	static CCubeRotationController	*CreateRotationController( CBaseEntity *pOwner );

	// IMotionEvent
	virtual simresult_e	Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular );

private:
	bool						m_bEnabled;
	float						m_flSuspendTime;
	Vector						m_worldGoalAxis;
	Vector						m_localTestAxis;
	IPhysicsMotionController	*m_pController;
	float						m_angularLimit;
	CBaseEntity					*m_pParent;
};

class CPropWeightedCube : public PlayerPickupPaintPowerUser< CPhysicsProp >, public IPropWeightedCubeAutoList
{
public:
	DECLARE_CLASS( CPropWeightedCube, PlayerPickupPaintPowerUser< CPhysicsProp > );
	DECLARE_SERVERCLASS();
	IMPLEMENT_AUTO_LIST_GET();

	CPropWeightedCube();

	virtual void	Spawn( void );
	virtual void	Activate( void );
	virtual void	Precache( void );
	virtual int		ObjectCaps( void );
	
	virtual void	OnFizzled( void ) 
	{ 
		// Handle the special Summer Sale achievement case
		// If a cube ever fizzles on the "red racer" map, the achievement is lost
		if ( V_stricmp( STRING(gpGlobals->mapname), "mp_coop_paint_red_racer" ) == 0 )
		{
			CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, "@glados" );
			if ( pEntity )
			{
				pEntity->RunScript( "CoopCubeFizzle()", "OnFizzled" );
			}

		}

		m_OnFizzled.FireOutput( this, this ); 
	}

	virtual bool	HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer );
	virtual QAngle	PreferredCarryAngles( void );
	virtual void	OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason );
	virtual void	OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason );
	virtual void	UpdateOnRemove( void );
	virtual void	NotifySystemEvent(CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params );

	virtual void	StartTouch( CBaseEntity *pOther );
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	
	void			SetSkin( int skinNum );

	void SetActivated( bool bActivate );

	// instead of getting which model it uses, we can just ask this
	WeightedCubeType_e GetCubeType( void ) { return m_nCubeType; }

#ifndef CLIENT_DLL
	CPropWeightedCube* GetSchrodingerTwin( void );
#endif

	void SetLaser( CBaseEntity *pLaser );
	CBaseEntity* GetLaser()
	{
		return m_hLaser.Get();
	}
	bool HasLaser( void )
	{
		return m_hLaser.Get() != NULL;
	}

	virtual int	UpdateTransmitState();

	virtual void Paint( PaintPowerType paintType, const Vector &worldContactPt );

	void UpdateSchrodingerSound( void );

	void SchrodingerThink( void );
	void DisabledThink( void );
	bool IsMovementDisabled() const { return m_bMovementDisabled; }
	bool ShouldEnterDisabledState( void );
	void EnterDisabledState( void );
	void ExitDisabledState( void );

	void OnEnteredTractorBeam( void );
	void OnExitedTractorBeam( void );
	void TractorBeamThink( void );
	void ExitTractorBeamThink( void );

	bool WasTouchedByPlayer() { return m_bTouchedByPlayer; }

#ifndef CLIENT_DLL
	#define CREATE_CUBE_AT_POSITION false
	static void CreatePortalWeightedCube( WeightedCubeType_e objectType, bool bAtCursorPosition = true, const Vector &position = vec3_origin );
#endif

private:

	void ConvertOldSkins( void );
	void SetCubeType( void );
	void SetPaintedMaterial( PaintPowerType paintType );
	void SetCubeSkin( void );

	void	InputDissolve( inputdata_t &in );
	void	InputSilentDissolve( inputdata_t &in );
	void	InputPreDissolveJoke( inputdata_t &in );
	QAngle	CalculatePreferredAngles( CBasePlayer *pPlayer );
	void	UpdatePreferredAngles( CBasePlayer *pPlayer );

	COutputEvent			m_OnFizzled;
	COutputEvent			m_OnBluePickUp;
	COutputEvent			m_OnOrangePickUp;
	COutputEvent			m_OnPainted;
	QAngle					m_vecCarryAngles;
	CHandle<CCubeRotationController>	m_pController;
	EHANDLE					m_hLaser;

	int m_nBouncyMaterialIndex;

	void InputDisablePortalFunnel( inputdata_t &in );
	void InputEnablePortalFunnel( inputdata_t &in );

	void InputExitDisabledState( inputdata_t &in );

	void InputSetPaint( inputdata_t &in );

	void InputDisablePickup( inputdata_t &in );
	void InputEnablePickup( inputdata_t &in );

	DECLARE_DATADESC();

	CSoundPatch *m_pSchrodingerSound;

	WeightedCubeType_e m_nCubeType;
	bool m_bRusted;
	bool m_bActivated;
	bool m_bNewSkins;

	PaintPowerType m_nCurrentPaintedType;

	float m_flDisabledNudgeStartTime;
	bool m_bMovementDisabled;
	bool m_bTouchedByPlayer;
	bool m_bPickupDisabled;

#ifndef CLIENT_DLL
	// Schrodinger's Balls
	CHandle< CPropWeightedCube > m_hSchrodingerTwin;
	static CHandle< CPropWeightedCube > m_hSchrodingerDangling;
#endif
};

bool UTIL_IsReflectiveCube( CBaseEntity *pEntity );
bool UTIL_IsWeightedCube( CBaseEntity *pEntity );

#ifndef CLIENT_DLL
	bool UTIL_IsSchrodinger( CBaseEntity *pEntity );
	CPropWeightedCube* UTIL_GetSchrodingerTwin( CBaseEntity *pEntity );
#endif
