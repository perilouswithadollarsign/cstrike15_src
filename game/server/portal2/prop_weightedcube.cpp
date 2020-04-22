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
#include "portal/weapon_physcannon.h"
#include "datacache/imdlcache.h"
#include "prop_weightedcube.h"
#include "portal_player.h"
#include "portal_player_shared.h"
#include "world.h"
#include "vcollide_parse.h"
#include "portal_gamestats.h"
#include "saverestore_utlvector.h"
#include "trigger_portal_cleanser.h"
#include "portal_mp_gamerules.h"
#include "cvisibilitymonitor.h"

ConVar reflector_cube_disabled_think_rate( "reflector_cube_disabled_think_rate", "0.1f", FCVAR_DEVELOPMENTONLY, "The rate at which the cube should think when it is disabled." );
ConVar reflector_cube_disabled_nudge_time( "reflector_cube_disabled_nudge_time", "0.5f", FCVAR_DEVELOPMENTONLY, "The amount of time the cube needs to be touched before it gets enabled again." );
ConVar reflector_cube_disabled_use_touch_check( "reflector_cube_disabled_use_touch_check", "0", FCVAR_DEVELOPMENTONLY, "Use touch checks to determine when to enable the cube." );

ConVar sv_portal2_pickup_hint_range( "sv_portal2_pickup_hint_range", "350.0", FCVAR_NONE );

// FIXME: Bring this back for DLC2
//extern ConVar sv_schrodinger_laser_world_aligned;

//Standard cube skins
enum StandardCubeSkinType_t
{
	CUBE_STANDARD_CLEAN_SKIN = 0,
	CUBE_STANDARD_CLEAN_ACTIVATED_SKIN = 2,
	CUBE_STANDARD_RUSTED_SKIN = 3,
	CUBE_STANDARD_RUSTED_ACTIVATED_SKIN = 5,
	CUBE_STANDARD_BOUNCE_SKIN = 6,
	CUBE_STANDARD_BOUNCE_ACTIVATED_SKIN = 10,
	CUBE_STANDARD_SPEED_SKIN = 7,
	CUBE_STANDARD_SPEED_ACTIVATED_SKIN = 11
};

//Companion cube skins
enum CompanionCubeSkinType_t
{
	CUBE_COMPANION_CLEAN_SKIN = 1,
	CUBE_COMPANION_CLEAN_ACTIVATED_SKIN = 4,
	CUBE_COMPANION_BOUNCE_SKIN = 8,
	CUBE_COMPANION_BOUNCE_ACTIVATED_SKIN = 8,
	CUBE_COMPANION_SPEED_SKIN = 9,
	CUBE_COMPANION_SPEED_ACTIVATED_SKIN = 9
};

//Reflective cubs skins
enum ReflectiveCubeSkinType_t
{
	CUBE_REFLECTIVE_CLEAN_SKIN = 0,
	CUBE_REFLECTIVE_RUSTED_SKIN = 1,
	CUBE_REFLECTIVE_BOUNCE_SKIN = 2,
	CUBE_REFLECTIVE_SPEED_SKIN = 3
};

//Sphere skins
enum WeightedSpherSkinType_t
{
	CUBE_SPHERE_CLEAN_SKIN = 0,
	CUBE_SPHERE_CLEAN_ACTIVATED_SKIN = 1,
	CUBE_SPHERE_BOUNCE_SKIN = 2,
	CUBE_SPHERE_BOUNCE_ACTIVATED_SKIN = 2,
	CUBE_SPHERE_SPEED_SKIN = 3,
	CUBE_SPHERE_SPEED_ACTIVATED_SKIN = 3
};

//Antique cube skins
enum AntiqueCubeSkinType_t
{
	CUBE_ANTIQUE_CLEAN_SKIN = 0,
	CUBE_ANTIQUE_BOUNCE_SKIN = 1,
	CUBE_ANTIQUE_SPEED_SKIN = 2
};

//Schrodinger cube skins
enum SchrodingerCubeSkinType_t
{
	CUBE_SCHRODINGER_CLEAN_SKIN = 4,
	CUBE_SCHRODINGER_BOUNCE_SKIN = 5,
	CUBE_SCHRODINGER_SPEED_SKIN = 6
};

const char SCHRODINGER_THINK_CONTEXT[] = "Schrodinger Think Context";

LINK_ENTITY_TO_CLASS( cube_rotationcontroller, CCubeRotationController );

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC( CCubeRotationController )

	DEFINE_FIELD( m_bEnabled,			FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flSuspendTime,		FIELD_TIME ),
	DEFINE_FIELD( m_worldGoalAxis,		FIELD_VECTOR ),
	DEFINE_FIELD( m_localTestAxis,		FIELD_VECTOR ),
	DEFINE_PHYSPTR( m_pController ),
	DEFINE_FIELD( m_angularLimit,		FIELD_FLOAT ),
	DEFINE_FIELD( m_pParent,			FIELD_CLASSPTR ),

END_DATADESC()

IMPLEMENT_AUTO_LIST( IPropWeightedCubeAutoList );

CCubeRotationController::~CCubeRotationController()
{
	if ( m_pController )
	{
		physenv->DestroyMotionController( m_pController );
		m_pController = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCubeRotationController::Spawn( void )
{
	m_bEnabled = false;

	// align the object's local Z axis
	m_localTestAxis.Init( 1, 0, 0 );
	
	// with the world's Z axis
	m_worldGoalAxis.Init( 0, 0, 1 );

	// recover from up to 25 degrees / sec angular velocity
	m_angularLimit	= 25;
	m_flSuspendTime	= 0;

	SetMoveType( MOVETYPE_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: Set the vector we'll try to match
//-----------------------------------------------------------------------------
void CCubeRotationController::SetAlignmentVector( const Vector &vecAlign )
{
	m_worldGoalAxis = vecAlign;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCubeRotationController::Activate( void )
{
	BaseClass::Activate();

	if ( m_pParent == NULL )
	{
		UTIL_Remove(this);
		return;
	}

	IPhysicsObject *pPhys = m_pParent->VPhysicsGetObject();

	if ( pPhys == NULL )
	{
		UTIL_Remove(this);
		return;
	}

	//Setup the motion controller
	if ( !m_pController )
	{
		m_pController = physenv->CreateMotionController( (IMotionEvent *)this );
		m_pController->AttachObject( pPhys, true );
	}
	else
	{
		m_pController->SetEventHandler( this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Simulation will be suspended after this amount of time
//-----------------------------------------------------------------------------
void CCubeRotationController::SuspendAfter( float flSuspendTime )
{
	m_flSuspendTime = flSuspendTime;
}

//-----------------------------------------------------------------------------
// Purpose: Actual simulation for tip controller
//-----------------------------------------------------------------------------
IMotionEvent::simresult_e CCubeRotationController::Simulate( IPhysicsMotionController *pController, IPhysicsObject *pObject, float deltaTime, Vector &linear, AngularImpulse &angular )
{
	if ( Enabled() == false )
		return SIM_NOTHING;

	// Don't simulate if we're being carried by the player
	// if ( m_pParent->IsBeingCarriedByPlayer() )
	//	return SIM_NOTHING;

	float flAngularLimit = m_angularLimit;

	// If we were just dropped by a friendly player, stabilise better
	/*
	if ( m_pParent->WasJustDroppedByPlayer() )
	{
		// Increase the controller strength a little
		flAngularLimit += 20;
	}
	else
	*/
	{
		// If the turret has some vertical velocity, don't simulate
		/*
		Vector vecVelocity;
		AngularImpulse angImpulse;
		pObject->GetVelocity( &vecVelocity, &angImpulse );
		if ( (vecVelocity.LengthSqr() > CNPC_FloorTurret::fMaxTipControllerVelocity) || (angImpulse.LengthSqr() > CNPC_FloorTurret::fMaxTipControllerAngularVelocity) )
			return SIM_NOTHING;
		*/
	}

	linear.Init();

	AngularImpulse angVel;
	pObject->GetVelocity( NULL, &angVel );

	matrix3x4_t matrix;
	// get the object's local to world transform
	pObject->GetPositionMatrix( &matrix );

	// Get the alignment axis in object space
	Vector currentLocalTargetAxis;
	VectorIRotate( m_worldGoalAxis, matrix, currentLocalTargetAxis );

	float invDeltaTime = (1/deltaTime);
	angular = ComputeRotSpeedToAlignAxes( m_localTestAxis, currentLocalTargetAxis, angVel, 1.0, invDeltaTime * invDeltaTime, flAngularLimit * invDeltaTime );

	return SIM_LOCAL_ACCELERATION;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCubeRotationController::Enable( bool state )
{
	m_bEnabled = state;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CCubeRotationController::Enabled( void )
{
	if ( m_flSuspendTime > gpGlobals->curtime )
		return true;

	return m_bEnabled;
}

CEG_NOINLINE CCubeRotationController * CCubeRotationController::CreateRotationController( CBaseEntity *pOwner )
{
	if ( pOwner == NULL )
		return NULL;

	CCubeRotationController *pController = (CCubeRotationController *) Create( "cube_rotationcontroller", pOwner->GetAbsOrigin(), pOwner->GetAbsAngles() );

	if ( pController != NULL )
	{
		pController->m_pParent = pOwner;
	}

	return pController;
}

CEG_PROTECT_STATIC_MEMBER_FUNCTION( CCubeRotationController_CreateRotationController, CCubeRotationController::CreateRotationController );

LINK_ENTITY_TO_CLASS( prop_weighted_cube, CPropWeightedCube );

BEGIN_DATADESC( CPropWeightedCube )

	DEFINE_FIELD( m_vecCarryAngles, FIELD_VECTOR ),
	DEFINE_FIELD( m_pController,	FIELD_EHANDLE ),
	DEFINE_FIELD( m_bMovementDisabled,		FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bActivated, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bTouchedByPlayer,	FIELD_BOOLEAN ),
	DEFINE_FIELD( m_nCurrentPaintedType,	FIELD_INTEGER ),
	DEFINE_FIELD( m_bPickupDisabled, FIELD_BOOLEAN ),

	DEFINE_SOUNDPATCH( m_pSchrodingerSound ),

	DEFINE_THINKFUNC( SchrodingerThink ),
	DEFINE_THINKFUNC( DisabledThink ),
	DEFINE_THINKFUNC( TractorBeamThink ),
	DEFINE_THINKFUNC( ExitTractorBeamThink ),

	DEFINE_KEYFIELD( m_bRusted, FIELD_BOOLEAN, "SkinType" ),
	DEFINE_KEYFIELD( m_nCubeType, FIELD_INTEGER, "CubeType" ),
	DEFINE_KEYFIELD( m_bNewSkins, FIELD_BOOLEAN, "NewSkins" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Dissolve", InputDissolve ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SilentDissolve", InputSilentDissolve ),
	DEFINE_INPUTFUNC( FIELD_VOID, "PreDissolveJoke", InputPreDissolveJoke ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisablePortalFunnel", InputDisablePortalFunnel ),
	DEFINE_INPUTFUNC( FIELD_VOID, "EnablePortalFunnel", InputEnablePortalFunnel ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ExitDisabledState", InputExitDisabledState ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetPaint", InputSetPaint ),
	DEFINE_INPUTFUNC( FIELD_VOID, "DisablePickup", InputDisablePickup ),
	DEFINE_INPUTFUNC( FIELD_VOID, "EnablePickup", InputEnablePickup ),

	DEFINE_OUTPUT( m_OnFizzled, "OnFizzled" ),
	DEFINE_OUTPUT( m_OnOrangePickUp, "OnOrangePickUp" ),
	DEFINE_OUTPUT( m_OnBluePickUp, "OnBluePickUp" ),
	DEFINE_OUTPUT( m_OnPainted, "OnPainted" ),

END_DATADESC()

//no new networked fields, just need entity specific virtual functions defined on the client
IMPLEMENT_SERVERCLASS_ST( CPropWeightedCube, DT_PropWeightedCube )
END_SEND_TABLE()

const char *CUBE_MODEL = "models/props/metal_box.mdl";
const char *CUBE_REFLECT_MODEL = "models/props/reflection_cube.mdl";
const char *CUBE_SPHERE_MODEL = "models/props_gameplay/mp_ball.mdl";
const char *CUBE_FX_FIZZLER_MODEL = "models/props/metal_box_fx_fizzler.mdl";
const char *CUBE_ANTIQUE_MODEL = "models/props_underground/underground_weighted_cube.mdl";
const char *CUBE_SCHRODINGER_MODEL = "models/props/reflection_cube.mdl";


CHandle< CPropWeightedCube > CPropWeightedCube::m_hSchrodingerDangling;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CPropWeightedCube::CPropWeightedCube()
				 : m_bMovementDisabled( false ),
				   m_bRusted( false ),
				   m_bActivated( false ),
				   m_nCubeType( CUBE_STANDARD ),
				   m_bTouchedByPlayer( false )
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEG_NOINLINE void CPropWeightedCube::Spawn( void )
{
	// Start out with nothing
	m_vecCarryAngles.Init(0,0,0);

	ConvertOldSkins();

	Precache();

	m_nCurrentPaintedType = NO_POWER;

	m_bPickupDisabled = false;

	SetCubeType();

	CEG_PROTECT_VIRTUAL_FUNCTION( CPropWeightedCube_Spawn );

	m_nBouncyMaterialIndex = physprops->GetSurfaceIndex( "WeightedCube_Bounce" );
	SetInteraction( PROPINTER_PHYSGUN_ALLOW_OVERHEAD );

	BaseClass::Spawn();

	SetCollisionGroup( COLLISION_GROUP_WEIGHTED_CUBE );

	if ( m_nCubeType == CUBE_SCHRODINGER )
	{
		SetContextThink( &CPropWeightedCube::SchrodingerThink, gpGlobals->curtime + reflector_cube_disabled_think_rate.GetFloat(), SCHRODINGER_THINK_CONTEXT );
	}

#if !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
	g_PortalGameStats.Event_CubeSpawn();
#endif

	VisibilityMonitor_AddEntity_NotVisibleThroughGlass( this, sv_portal2_pickup_hint_range.GetFloat() - 50.0f, NULL, NULL );

	SetFadeDistance( -1.0f, 0.0f );
	SetGlobalFadeScale( 0.0f );
}


void CPropWeightedCube::Activate( void )
{
	SetPaintedMaterial( (PaintPowerType)( m_PrePaintedPower ) );

#if 0
	if ( !m_pSchrodingerSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		CPASAttenuationFilter filter( this );

		m_pSchrodingerSound = controller.SoundCreate( filter, entindex(), "music.laser_node_02.play" );
		controller.Play( m_pSchrodingerSound, 0, RandomFloat( 99, 101 ) );
	}
#endif

	BaseClass::Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::UpdateOnRemove( void )
{
	BaseClass::UpdateOnRemove();

	if ( m_pController )
	{
		UTIL_Remove( m_pController );
	}

	CPropWeightedCube *pTwin = m_hSchrodingerTwin.Get();
	if ( pTwin && !pTwin->IsMarkedForDeletion() )
	{
		CTriggerPortalCleanser::FizzleBaseAnimating( NULL, pTwin );
	}

#if 0
	CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

	controller.SoundDestroy( m_pSchrodingerSound );
	m_pSchrodingerSound = NULL;

	BaseClass::StopLoopingSounds();
#endif

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::Precache( void )
{
	ConvertOldSkins();

	switch ( m_nCubeType )
	{
	default:
	case CUBE_STANDARD:
	case CUBE_COMPANION:
		PrecacheModel( CUBE_MODEL );
		break;

	case CUBE_REFLECTIVE:
		PrecacheModel( CUBE_REFLECT_MODEL );
		break;

	case CUBE_SPHERE:
		PrecacheModel( CUBE_SPHERE_MODEL );
		break;

	case CUBE_ANTIQUE:
		PrecacheModel( CUBE_ANTIQUE_MODEL );

	case CUBE_SCHRODINGER:
		PrecacheModel( CUBE_SCHRODINGER_MODEL );
		PrecacheScriptSound( "music.laser_node_02.play" );
		PrecacheScriptSound( "prop_laser_catcher.poweron" );
		PrecacheScriptSound( "prop_laser_catcher.poweroff" );
		break;
	}

	PrecacheModel( CUBE_FX_FIZZLER_MODEL );

	PrecacheScriptSound( "WeightedCube.JumpPowerActivateShort" );
	PrecacheScriptSound( "WeightedCube.JumpPowerActivateLong" );

	BaseClass::Precache();
}

int CPropWeightedCube::ObjectCaps( void )
{
	int flags = (BaseClass::ObjectCaps()|FCAP_IMPULSE_USE);

	if ( GetPaintedPower() == BOUNCE_POWER )
	{
		flags |= FCAP_USE_IN_RADIUS;
	}

	return flags;
}


int	CPropWeightedCube::UpdateTransmitState()
{
	if ( HasLaser() )
	{
		return SetTransmitState( FL_EDICT_ALWAYS );
	}

	return BaseClass::UpdateTransmitState();
}

void CPropWeightedCube::ConvertOldSkins( void )
{
	//HACK HACK: Make the cubes choose skins using the new method even though the maps have not been updated to use them.
	if( !m_bNewSkins )
	{
		if( m_nSkin > 1 )
		{
			m_nSkin--;
		}

		m_nCubeType = static_cast<WeightedCubeType_e>( m_nSkin.Get() );
		m_bNewSkins = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::SetCubeType( void )
{
	// FIXME: Remove for DLC2
	if ( m_nCubeType == CUBE_SCHRODINGER )
	{
	 	m_nCubeType = CUBE_REFLECTIVE;
	}
	
	switch( m_nCubeType )
	{
		//Standard cube
		case CUBE_STANDARD:
		case CUBE_COMPANION:
		{
			SetModelName( MAKE_STRING( CUBE_MODEL ) );
			break;
		}
		
		//Reflective cube
		case CUBE_REFLECTIVE:
		{
			SetModelName( MAKE_STRING( CUBE_REFLECT_MODEL ) );
			m_pController = CCubeRotationController::CreateRotationController( this );
			AddSpawnFlags( SF_PHYSPROP_ENABLE_ON_PHYSCANNON );
			break;
		}
		
		//Sphere
		case CUBE_SPHERE:
		{
			SetModelName( MAKE_STRING( CUBE_SPHERE_MODEL ) );
			break;
		}
		
		//Antique cube
		case CUBE_ANTIQUE:
		{
			SetModelName( MAKE_STRING( CUBE_ANTIQUE_MODEL ) );
			break;
		}

		//Schrodinger cube
		case CUBE_SCHRODINGER:
		{
			SetModelName( MAKE_STRING( CUBE_SCHRODINGER_MODEL ) );
			m_pController = CCubeRotationController::CreateRotationController( this );
			AddSpawnFlags( SF_PHYSPROP_ENABLE_ON_PHYSCANNON );

			if ( m_hSchrodingerDangling.Get() == NULL )
			{
				m_hSchrodingerDangling = this;
			}
			else
			{
				m_hSchrodingerDangling->m_hSchrodingerTwin = this;
				m_hSchrodingerTwin = m_hSchrodingerDangling;
				m_hSchrodingerDangling = NULL;
			}
			break;
		}
	}

	SetCubeSkin();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::SetActivated( bool bActivate )
{
	m_bActivated  = bActivate;

	SetCubeSkin();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::SetCubeSkin( void )
{
	switch( m_nCubeType )
	{
		//Standard cube
		case CUBE_STANDARD:
		{
			//Rusted cubes don't show paint
			if( m_bRusted )
			{
				if( m_bActivated )
				{
					SetSkin( CUBE_STANDARD_RUSTED_ACTIVATED_SKIN );
				}
				else
				{
					SetSkin( CUBE_STANDARD_RUSTED_SKIN );
				}
			}
			else
			{
				switch( GetPaintedPower() )
				{
					//Bounce painted
					case BOUNCE_POWER:
					{
						if( m_bActivated )
						{
							RANDOM_CEG_TEST_SECRET_PERIOD( 98, 106 );
							SetSkin( CUBE_STANDARD_BOUNCE_ACTIVATED_SKIN );
						}
						else
						{
							SetSkin( CUBE_STANDARD_BOUNCE_SKIN );
						}
					}
					break;
					//Speed painted
					case SPEED_POWER:
					{
						if( m_bActivated )
						{
							SetSkin( CUBE_STANDARD_SPEED_ACTIVATED_SKIN );
						}
						else
						{
							SetSkin( CUBE_STANDARD_SPEED_SKIN );
						}
					}
					break;
					//Not painted
					default:
					{
						if( m_bActivated )
						{
							SetSkin( CUBE_STANDARD_CLEAN_ACTIVATED_SKIN );
						}
						else
						{
							SetSkin( CUBE_STANDARD_CLEAN_SKIN );
						}
					}
					break;
				}
			}
		}
		break;
		//Companion cube
		case CUBE_COMPANION:
		{
			switch( GetPaintedPower() )
			{
				//Bounce painted
				case BOUNCE_POWER:
				{
					if( m_bActivated )
					{
						SetSkin( CUBE_COMPANION_BOUNCE_ACTIVATED_SKIN );
					}
					else
					{
						SetSkin( CUBE_COMPANION_BOUNCE_SKIN );
					}
				}
				break;
				//Speed painted
				case SPEED_POWER:
				{
					if( m_bActivated )
					{
						SetSkin( CUBE_COMPANION_SPEED_ACTIVATED_SKIN );
					}
					else
					{
						SetSkin( CUBE_COMPANION_SPEED_SKIN );
					}
				}
				break;
				//Not painted
				default:
				{
					if( m_bActivated )
					{
						SetSkin( CUBE_COMPANION_CLEAN_ACTIVATED_SKIN );
					}
					else
					{
						SetSkin( CUBE_COMPANION_CLEAN_SKIN );
					}
				}
				break;
			}
		}
		break;
		//Reflective cube
		case CUBE_REFLECTIVE:
		{
			switch( GetPaintedPower() )
			{
				//Bounce painted
			case BOUNCE_POWER:
				{
					if( m_bRusted )
					{
						// FIXME
						SetSkin( CUBE_REFLECTIVE_BOUNCE_SKIN );
					}
					else
					{
						SetSkin( CUBE_REFLECTIVE_BOUNCE_SKIN );
					}
				}
				break;
				//Speed painted
			case SPEED_POWER:
				{
					if( m_bRusted )
					{
						// FIXME
						SetSkin( CUBE_REFLECTIVE_SPEED_SKIN );
					}
					else
					{
						SetSkin( CUBE_REFLECTIVE_SPEED_SKIN );
					}
				}
				break;
				//Not painted
			default:
				{
					if( m_bRusted )
					{
						SetSkin( CUBE_REFLECTIVE_RUSTED_SKIN );
					}
					else
					{
						SetSkin( CUBE_REFLECTIVE_CLEAN_SKIN );
					}
				}
				break;
			}
		}
		break;
		//Sphere
		case CUBE_SPHERE:
		{
			switch( GetPaintedPower() )
			{
				//Bounce painted
				case BOUNCE_POWER:
				{
					if( m_bActivated )
					{
						SetSkin( CUBE_SPHERE_BOUNCE_ACTIVATED_SKIN );
					}
					else
					{
						SetSkin( CUBE_SPHERE_BOUNCE_SKIN );
					}
				}
				break;
				//Speed painted
				case SPEED_POWER:
				{
					if( m_bActivated )
					{
						SetSkin( CUBE_SPHERE_SPEED_ACTIVATED_SKIN );
					}
					else
					{
						SetSkin( CUBE_SPHERE_SPEED_SKIN );
					}
				}
				break;
				//Not painted
				default:
				{
					if( m_bActivated )
					{
						SetSkin( CUBE_SPHERE_CLEAN_ACTIVATED_SKIN );
					}
					else
					{
						SetSkin( CUBE_SPHERE_CLEAN_SKIN );
					}
				}
				break;
			}
		}
		break;
		//Antique cube
		case CUBE_ANTIQUE:
		{
			switch( GetPaintedPower() )
			{
				//Bounce painted
				case BOUNCE_POWER:
				{
					SetSkin( CUBE_ANTIQUE_BOUNCE_SKIN );
				}
				break;
				//Speed painted
				case SPEED_POWER:
				{
					SetSkin( CUBE_ANTIQUE_SPEED_SKIN );
				}
				break;
				//Not painted
				default:
				{
					SetSkin( CUBE_ANTIQUE_CLEAN_SKIN );
				}
				break;
			}
		}
		break;

		//Antique cube
		case CUBE_SCHRODINGER:
		{
			switch( GetPaintedPower() )
			{
				//Bounce painted
				case BOUNCE_POWER:
				{
					SetSkin( CUBE_SCHRODINGER_BOUNCE_SKIN );
				}
				break;
				//Speed painted
				case SPEED_POWER:
				{
					SetSkin( CUBE_SCHRODINGER_SPEED_SKIN );
				}
				break;
				//Not painted
				default:
				{
					SetSkin( CUBE_SCHRODINGER_CLEAN_SKIN );
				}
				break;
			}
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::SetSkin( int skinNum )
{
	m_nSkin = skinNum;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::InputDissolve( inputdata_t &in ) 
{ 
	CTriggerPortalCleanser::FizzleBaseAnimating( NULL, this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::InputSilentDissolve( inputdata_t &in ) 
{
	OnFizzled();
	UTIL_Remove( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::InputPreDissolveJoke( inputdata_t &in ) 
{ 
	CBaseEntity *pEntity = gEntList.FindEntityByName( NULL, "@glados" );
	if ( pEntity )
	{
		pEntity->RunScript( "CoopCubeFizzle()", "PreDissolveJoke" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::InputDisablePortalFunnel( inputdata_t &in )
{
	m_bAllowPortalFunnel = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::InputEnablePortalFunnel( inputdata_t &in )
{
	m_bAllowPortalFunnel = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
QAngle CPropWeightedCube::CalculatePreferredAngles( CBasePlayer *pPlayer )
{
	return QAngle(0,0,0);
}

void CPropWeightedCube::UpdatePreferredAngles( CBasePlayer *pPlayer )
{
	m_vecCarryAngles = CalculatePreferredAngles( pPlayer );

	if( HasPreferredCarryAnglesForPlayer( pPlayer ) )
	{
		m_qPreferredPlayerCarryAngles = m_vecCarryAngles;
	}
	else
	{
		if( m_qPreferredPlayerCarryAngles.Get().x < FLT_MAX )
		{
			m_qPreferredPlayerCarryAngles.GetForModify().Init( FLT_MAX, FLT_MAX, FLT_MAX );
		}
	}
}

extern void ComputePlayerMatrix( CBasePlayer *pPlayer, matrix3x4_t &out );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
QAngle CPropWeightedCube::PreferredCarryAngles( void )
{
	static QAngle s_prefAngles;
	s_prefAngles = m_vecCarryAngles;

	CBasePlayer *pPlayer = GetPlayerHoldingEntity( this );
	if ( pPlayer )
	{
		Vector vecRight;
		pPlayer->GetVectors( NULL, &vecRight, NULL );

		Quaternion qRotation;
		AxisAngleQuaternion( vecRight, pPlayer->EyeAngles().x, qRotation );

		matrix3x4_t tmp;
		ComputePlayerMatrix( pPlayer, tmp );

		QAngle qTemp = TransformAnglesToWorldSpace( s_prefAngles, tmp );
		Quaternion qExisting;
		AngleQuaternion( qTemp, qExisting );
		Quaternion qFinal;
		QuaternionMult( qRotation, qExisting, qFinal );

		QuaternionAngles( qFinal, qTemp );
		s_prefAngles = TransformAnglesToLocalSpace( qTemp, tmp );
	}

	return s_prefAngles;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::OnPhysGunPickup( CBasePlayer *pPhysGunUser, PhysGunPickup_t reason )
{
	BaseClass::OnPhysGunPickup( pPhysGunUser, reason );

	m_bMovementDisabled = false;
	m_bTouchedByPlayer = true;

	// Calculate our preferred angles on the first pickup
	if ( reason == PICKED_UP_BY_CANNON || reason == PICKED_UP_BY_PLAYER )
	{
		UpdatePreferredAngles( pPhysGunUser );

		if ( m_pController )
		{
			m_pController->Enable( false );
		}

		CPortal_Player *pPlayer = ToPortalPlayer( pPhysGunUser );
		if ( pPlayer )
		{
			// Force a cool-down on the +USE key after a successful grab
			pPlayer->SetUseKeyCooldownTime( 0.5f );
		}
	}

	if ( pPhysGunUser )
	{
		if ( pPhysGunUser->GetTeamNumber() == TEAM_RED )
		{
			m_OnOrangePickUp.FireOutput( pPhysGunUser, this );
		}
		else if ( pPhysGunUser->GetTeamNumber() == TEAM_BLUE )
		{
			m_OnBluePickUp.FireOutput( pPhysGunUser, this );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Turn on our rotation controller when we're dropped nicely
//-----------------------------------------------------------------------------
ConVar sv_box_physgundrop_angle_threshold("sv_box_physgundrop_angle_threshold", "70.f");
void CPropWeightedCube::OnPhysGunDrop( CBasePlayer *pPhysGunUser, PhysGunDrop_t reason )
{
	BaseClass::OnPhysGunDrop( pPhysGunUser, reason );
	
	// Only care about this if we're dropped, as opposed to launched or thrown
	if ( reason != DROPPED_BY_PLAYER && reason != DROPPED_BY_CANNON )
		return;

	// Enable the controller for a short time
	if ( m_pController )
	{
		m_pController->Activate();
		Vector vecForward;
		AngleVectors( GetAbsAngles(), &vecForward );
		m_pController->SetAlignmentVector( vecForward );
		m_pController->SuspendAfter( gpGlobals->curtime + 0.5f );
	}

	// When player drop the box and player's up is not world up, try to throw the box in the local down direction
	bool bThrowBoxLocalDown = false;
	CPortal_Player *pPortalPlayer = ToPortalPlayer( pPhysGunUser );
	if ( pPortalPlayer && !AlmostEqual( DotProduct( pPortalPlayer->GetPortalPlayerLocalData().m_Up, Vector( 0, 0, 1 ) ), 1.f ) )
	{
		// check if player looks too far off the ground, then just drop the box with gravity
		Vector vForward;
		pPortalPlayer->GetVectors( &vForward, NULL, NULL );
		float flLookAngle = RAD2DEG( acosf( DotProduct( vForward, pPortalPlayer->GetPortalPlayerLocalData().m_StickNormal ) ) );
		bThrowBoxLocalDown = flLookAngle >= sv_box_physgundrop_angle_threshold.GetFloat();

		if( bThrowBoxLocalDown )
		{
			float flDropSpeed = 400.f;
			Vector vecDownVelocity = -( flDropSpeed * pPortalPlayer->GetPortalPlayerLocalData().m_Up );
			IPhysicsObject *pPhysics = VPhysicsGetObject();
			if ( pPhysics )
			{
				pPhysics->SetVelocityInstantaneous( &vecDownVelocity, NULL );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Only bother with preferred carry angles if we're a reflective cube
//-----------------------------------------------------------------------------
bool CPropWeightedCube::HasPreferredCarryAnglesForPlayer( CBasePlayer *pPlayer )
{
	return ( m_nCubeType == CUBE_REFLECTIVE ) || ( /*FIXME: Bring back for DLC2 !sv_schrodinger_laser_world_aligned.GetBool() && */ m_nCubeType == CUBE_SCHRODINGER && m_hLaser.Get() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::NotifySystemEvent(CBaseEntity *pNotify, notify_system_event_t eventType, const notify_system_event_params_t &params )
{
	// On teleport, we record a pointer to the portal we are arriving at
	if ( eventType == NOTIFY_EVENT_TELEPORT )
	{
		CPortal_Base2D *pEnteredPortal = dynamic_cast<CPortal_Base2D*>( pNotify );
		if ( pEnteredPortal && m_pController )
		{
			Vector vecWorldAign = pEnteredPortal->m_matrixThisToLinked.ApplyRotation( m_pController->GetAlignmentVector() );
			vecWorldAign.NormalizeInPlace();
			m_pController->SetAlignmentVector( vecWorldAign );
		}
	}

	BaseClass::NotifySystemEvent( pNotify, eventType, params );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPropWeightedCube::Paint( PaintPowerType paintType, const Vector &worldContactPt )
{
	BaseClass::Paint( paintType, worldContactPt );

	SetPaintedMaterial( paintType );
	SetCubeSkin();

	CPropWeightedCube *pTwin = m_hSchrodingerTwin.Get();
	if ( pTwin && pTwin->GetPaintedPower() != paintType )
	{
		pTwin->Paint( paintType, worldContactPt );
	}
}


void CPropWeightedCube::SetPaintedMaterial( PaintPowerType paintType )
{
	if ( m_nCurrentPaintedType != paintType && paintType != NO_POWER )
	{
		m_OnPainted.FireOutput( this, this );
	}

	m_nCurrentPaintedType = paintType;

	switch( paintType )
	{
		case BOUNCE_POWER:
		{
			//Set the box to be bouncy
			IPhysicsObject* pPhysObject = VPhysicsGetObject();
			if( pPhysObject )
			{
				pPhysObject->SetMaterialIndex( m_nBouncyMaterialIndex );
			}

			ExitDisabledState();

			break;

		}
		case SPEED_POWER:
		{
			IPhysicsObject* pPhysObject = VPhysicsGetObject();
			if( pPhysObject )
			{
				pPhysObject->SetMaterialIndex( BaseClass::GetSpeedMaterialIndex() );
			}
			break;
		}
		case PORTAL_POWER:
		case REFLECT_POWER:
		case NO_POWER:
		default:
		{
			// Store our material index
			IPhysicsObject* pPhysObject = VPhysicsGetObject();
			if( pPhysObject )
			{
				pPhysObject->SetMaterialIndex( m_nOriginalMaterialIndex );
			}
			break;
		}
	}
}

CPropWeightedCube* CPropWeightedCube::GetSchrodingerTwin( void )
{
	return m_hSchrodingerTwin;
}

void CPropWeightedCube::UpdateSchrodingerSound( void )
{
	if ( !m_hSchrodingerTwin.Get() )
		return;

	float fDist = m_hSchrodingerTwin->GetDistanceToEntity( this );

	if ( m_pSchrodingerSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundChangeVolume( m_pSchrodingerSound, RemapValClamped( fDist, 350.0f, 0.0f, 0.0f, 1.0f ), 0.1 );
	}
}

void CPropWeightedCube::SetLaser( CBaseEntity *pLaser )
{
	m_hLaser = pLaser;

	if ( pLaser )
	{
		CBasePlayer *pPlayer = GetPlayerHoldingEntity( this );
		if ( pPlayer )
		{
			UpdatePreferredAngles( pPlayer );
		}

		if ( GetCubeType() == CUBE_SCHRODINGER )
		{
			// FIXME: Need a better sound for this
			//EmitSound( "prop_laser_catcher.poweron" );
		}
	}
	else
	{
		if ( GetCubeType() == CUBE_SCHRODINGER )
		{
			// FIXME: Need a better sound for this
			//EmitSound( "prop_laser_catcher.poweroff" );
		}
	}

	// need to update transmitstate to prevent laser going through box when box goes outside PVS
	UpdateTransmitState();
}


bool CPropWeightedCube::ShouldEnterDisabledState( void )
{
	IPhysicsObject *pPhysicsObject = VPhysicsGetObject();

	if( pPhysicsObject )
	{
		if( !( pPhysicsObject->GetGameFlags() & FVPHYSICS_PLAYER_HELD ) && pPhysicsObject->IsAsleep() )
		{
			return true;
		}
	}

	return false;
}


void CPropWeightedCube::EnterDisabledState( void )
{
	if ( !m_bMovementDisabled )
	{
		IPhysicsObject *pPhysicsObject = VPhysicsGetObject();

		if( pPhysicsObject )
		{
			pPhysicsObject->EnableMotion( false );
		}
		
		m_bMovementDisabled = true;

		SetThink( &CPropWeightedCube::DisabledThink );
		SetNextThink( gpGlobals->curtime + reflector_cube_disabled_think_rate.GetFloat() );
	}
}


void CPropWeightedCube::ExitDisabledState( void )
{
	if ( m_bMovementDisabled )
	{
		m_bMovementDisabled = false;

		EnableMotion();
	}
}

void CPropWeightedCube::InputExitDisabledState( inputdata_t &in )
{
	ExitDisabledState();
}

void CPropWeightedCube::OnEnteredTractorBeam( void )
{
	SetThink( &CPropWeightedCube::TractorBeamThink );
	SetNextThink( gpGlobals->curtime );
}

void CPropWeightedCube::OnExitedTractorBeam( void )
{
	SetThink( &CPropWeightedCube::ExitTractorBeamThink );
	SetNextThink( gpGlobals->curtime );
}

void CPropWeightedCube::TractorBeamThink( void )
{
	if ( m_bMovementDisabled )
		return;

	// Stop colliding with player and freeze any rotational speed
	SetCollisionGroup( COLLISION_GROUP_PLAYER_HELD );

	IPhysicsObject *pPhys = VPhysicsGetObject();
	if ( pPhys )
	{
		AngularImpulse vZeroRotation( vec3_origin );
		pPhys->SetVelocity( NULL, &vZeroRotation );
	}

	// Give players 2 seconds to get out of the way
	SetThink( &CPropWeightedCube::ExitTractorBeamThink );
	SetNextThink( gpGlobals->curtime + 2.0f );
}

void CPropWeightedCube::ExitTractorBeamThink( void )
{
	bool bIntersectingPlayer = false;

	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( pPlayer )
		{
			if ( Intersects( pPlayer ) )
			{
				bIntersectingPlayer = true;
				break;
			}
		}
	}

	if ( bIntersectingPlayer )
	{
		SetThink( &CPropWeightedCube::ExitTractorBeamThink );
		SetNextThink( gpGlobals->curtime + 0.2f );
	}
	else
	{
		// Start colliding with player
		SetCollisionGroup( COLLISION_GROUP_WEIGHTED_CUBE );
	}
}

void CPropWeightedCube::InputSetPaint( inputdata_t &in )
{
	Paint( static_cast< PaintPowerType >( in.value.Int() ), Vector( 0.0f, 0.0f, 0.0f ) );
}


void CPropWeightedCube::StartTouch( CBaseEntity *pOther )
{
	if( m_bMovementDisabled )
	{
		if( pOther->IsPlayer() )
		{
			Vector vecPlayerForward;
			AngleVectors( pOther->EyeAngles(), &vecPlayerForward );
			vecPlayerForward.NormalizeInPlace();
			Vector vecCubeToPlayer = (GetAbsOrigin() - pOther->EyePosition()).Normalized();

			float flPlayerLookDot = DotProduct( vecCubeToPlayer, vecPlayerForward );
			float flCubeDirDot = DotProduct( Forward().Normalized(), vecPlayerForward );

			//DevMsg( "Dot:%f, CubeDot:%f\n", flPlayerLookDot, flCubeDirDot );

			//If the cube is in front of the player
			if( ( flPlayerLookDot > 0.8f && flCubeDirDot > 0.8f ) || ( flPlayerLookDot > 0.85f ) )
			{
				ExitDisabledState();
			}
		}
	}
	
	if( pOther->IsPlayer() )
	{
		m_bTouchedByPlayer = true;
	}

	BaseClass::StartTouch( pOther );
}

void CPropWeightedCube::SchrodingerThink( void )
{
	UpdateSchrodingerSound();

	//Keep thinking
	SetContextThink( &CPropWeightedCube::SchrodingerThink, gpGlobals->curtime + reflector_cube_disabled_think_rate.GetFloat(), SCHRODINGER_THINK_CONTEXT );
}

void CPropWeightedCube::DisabledThink( void )
{
	bool hasPaintPower = false;
	if( engine->HasPaintmap() )
	{
		if( GetPaintedPower() != NO_POWER )
		{
			hasPaintPower = true;
		}
		else
		{
			for( int i = 0; i < PAINT_POWER_TYPE_COUNT; ++i )
			{
				if( !IsInactivePower( GetPaintPower(i) ) )
				{
					hasPaintPower = true;
					break;
				}
			}
		}
	}

	//If the cube no longer has a laser attached to it or has a paint power
	if( !HasLaser() || hasPaintPower )
	{
		ExitDisabledState();
		return;
	}

	//Keep thinking
	SetNextThink( gpGlobals->curtime + reflector_cube_disabled_think_rate.GetFloat() );
}


void CPropWeightedCube::InputDisablePickup( inputdata_t &in )
{
	m_bPickupDisabled = true;
}

void CPropWeightedCube::InputEnablePickup( inputdata_t &in )
{
	m_bPickupDisabled = false;
}

void CPropWeightedCube::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if( m_bPickupDisabled == false )
	{
		BaseClass::Use( pActivator, pCaller, useType, value );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool UTIL_IsWeightedCube( CBaseEntity *pEntity )
{
	if ( pEntity == NULL )
		return false;

	return ( FClassnameIs( pEntity, "prop_weighted_cube" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool UTIL_IsReflectiveCube( CBaseEntity *pEntity )
{
	if ( UTIL_IsWeightedCube( pEntity ) == false )
		return false;

	CPropWeightedCube *pCube = assert_cast<CPropWeightedCube*>( pEntity );
	return ( pCube && pCube->GetCubeType() == CUBE_REFLECTIVE );
}

#ifndef CLIENT_DLL
bool UTIL_IsSchrodinger( CBaseEntity *pEntity )
{
	if ( !UTIL_IsWeightedCube( pEntity ) )
		return false;

	CPropWeightedCube *pCube = assert_cast<CPropWeightedCube*>( pEntity );
	if ( !pCube )
		return false;

	return pCube->GetCubeType() == CUBE_SCHRODINGER;
}

CPropWeightedCube* UTIL_GetSchrodingerTwin( CBaseEntity *pEntity )
{
	if ( !UTIL_IsSchrodinger( pEntity ) )
		return NULL;

	CPropWeightedCube *pCube = assert_cast<CPropWeightedCube*>( pEntity );
	if ( !pCube )
		return NULL;

	return pCube->GetSchrodingerTwin();
}
#endif


#define PORTAL_REFLECTOR_CUBE_MODEL_NAME "models/props/reflectocube.mdl"
#define PORTAL_WEIGHT_BOX_MODEL_NAME "models/props/metal_box.mdl"

#ifndef CLIENT_DLL

//-----------------------------------------------------------------------------
// Creates a weighted cube of a specific type
//-----------------------------------------------------------------------------
void CPropWeightedCube::CreatePortalWeightedCube( WeightedCubeType_e objectType, bool bAtCursorPosition, const Vector &position )
{
	MDLCACHE_CRITICAL_SECTION();

	bool allowPrecache = CBaseEntity::IsPrecacheAllowed();
	CBaseEntity::SetAllowPrecache( true );

	// Try to create entity
	CPropWeightedCube *entity = ( CPropWeightedCube* )CreateEntityByName("prop_weighted_cube");
	if (entity)
	{
		//entity->PrecacheModel( PORTAL_REFLECTOR_CUBE_MODEL_NAME );
		//entity->SetModel( PORTAL_REFLECTOR_CUBE_MODEL_NAME );
		entity->SetName( MAKE_STRING("cube") );
		entity->AddSpawnFlags( SF_PHYSPROP_ENABLE_PICKUP_OUTPUT );
		entity->m_nCubeType = objectType;
		entity->m_bNewSkins = true;
		entity->Precache();

		if ( !bAtCursorPosition )
		{
			entity->SetAbsOrigin( position );
		}

		DispatchSpawn(entity);

		if ( bAtCursorPosition )
		{
			// Now attempt to drop into the world
			CBasePlayer* pPlayer = UTIL_GetCommandClient();
			trace_t tr;
			Vector forward;
			pPlayer->EyeVectors( &forward );
			UTIL_TraceLine(pPlayer->EyePosition(),
				pPlayer->EyePosition() + forward * MAX_TRACE_LENGTH,MASK_SOLID, 
				pPlayer, COLLISION_GROUP_WEIGHTED_CUBE, &tr );
			if ( tr.fraction != 1.0 )
			{
				tr.endpos.z += 12;
				entity->Teleport( &tr.endpos, NULL, NULL );
				UTIL_DropToFloor( entity, MASK_SOLID );
			}
		}

		// This entity should send its object caps to the client
		entity->UpdateObjectCapsCache();

	}
	CBaseEntity::SetAllowPrecache( allowPrecache );
}


// Console command functions
void CC_Create_PortalWeightedCube()
{
	CPropWeightedCube::CreatePortalWeightedCube( CUBE_STANDARD );
}

void CC_Create_PortalCompanionCube()
{
	CPropWeightedCube::CreatePortalWeightedCube( CUBE_COMPANION );
}

void CC_Create_PortalReflectorCube()
{
	CPropWeightedCube::CreatePortalWeightedCube( CUBE_REFLECTIVE );
}

void CC_Create_PortalWeightedSphere()
{
	CPropWeightedCube::CreatePortalWeightedCube( CUBE_SPHERE );
}

void CC_Create_PortalWeightedAntique()
{
	CPropWeightedCube::CreatePortalWeightedCube( CUBE_ANTIQUE );
}

void CC_Create_PortalWeightedSchrodinger()
{
	CPropWeightedCube::CreatePortalWeightedCube( CUBE_SCHRODINGER );
}

// Console commands for creating cubes
static ConCommand ent_create_portal_reflector_cube("ent_create_portal_reflector_cube", CC_Create_PortalReflectorCube, "Creates a laser reflector cube cube where the player is looking.", FCVAR_GAMEDLL | FCVAR_CHEAT);
static ConCommand ent_create_portal_companion_cube("ent_create_portal_companion_cube", CC_Create_PortalCompanionCube, "Creates a companion cube where the player is looking.", FCVAR_GAMEDLL | FCVAR_CHEAT);
static ConCommand ent_create_portal_weighted_cube("ent_create_portal_weighted_cube", CC_Create_PortalWeightedCube, "Creates a standard cube where the player is looking.", FCVAR_GAMEDLL | FCVAR_CHEAT);
static ConCommand ent_create_portal_weighted_sphere("ent_create_portal_weighted_sphere", CC_Create_PortalWeightedSphere, "Creates a weighted sphere where the player is looking.", FCVAR_GAMEDLL | FCVAR_CHEAT);
static ConCommand ent_create_portal_weighted_antique("ent_create_portal_weighted_antique", CC_Create_PortalWeightedAntique, "Creates an antique cube where the player is looking.", FCVAR_GAMEDLL | FCVAR_CHEAT);
// FIXME: Bring this back for DLC2
//static ConCommand ent_create_portal_weighted_schrodinger("ent_create_portal_weighted_schrodinger", CC_Create_PortalWeightedSchrodinger, "Creates an Schrodinger cube where the player is looking.", FCVAR_GAMEDLL | FCVAR_CHEAT);

#endif // CLIENT_DLL
