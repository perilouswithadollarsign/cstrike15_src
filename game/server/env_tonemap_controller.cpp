//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "env_tonemap_controller.h"
#include "baseentity.h"
#include "entityoutput.h"
#include "convar.h"
#include "triggers.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
// Spawn Flags
#define SF_TONEMAP_MASTER			0x0001

// 0 - eyes fully closed / fully black
// 1 - nominal 
// 16 - eyes wide open / fully white

//-----------------------------------------------------------------------------
// Purpose: Entity that controls player's tonemap
//-----------------------------------------------------------------------------
class CEnvTonemapController : public CPointEntity
{
	DECLARE_CLASS( CEnvTonemapController, CPointEntity );
public:
	DECLARE_DATADESC();
	DECLARE_SERVERCLASS();

	CEnvTonemapController();

	void	Spawn( void );
	int		UpdateTransmitState( void );

	bool	IsMaster( void ) const					{ return HasSpawnFlags( SF_TONEMAP_MASTER ); }

	// Inputs
	void	InputSetTonemapRate( inputdata_t &inputdata );
	void	InputSetAutoExposureMin( inputdata_t &inputdata );
	void	InputSetAutoExposureMax( inputdata_t &inputdata );
	void	InputUseDefaultAutoExposure( inputdata_t &inputdata );
	void	InputSetBloomScale( inputdata_t &inputdata );
	void	InputUseDefaultBloomScale( inputdata_t &inputdata );
	void	InputSetBloomScaleRange( inputdata_t &inputdata );

	void	InputSetBloomExponent( inputdata_t &inputdata );
	void	InputSetBloomSaturation( inputdata_t &inputdata );
	void	InputSetTonemapPercentTarget( inputdata_t &inputdata );
	void	InputSetTonemapPercentBrightPixels( inputdata_t &inputdata );
	void	InputSetTonemapMinAvgLum( inputdata_t &inputdata );

public:
	CNetworkVar( bool, m_bUseCustomAutoExposureMin );
	CNetworkVar( bool, m_bUseCustomAutoExposureMax );
	CNetworkVar( bool, m_bUseCustomBloomScale );
	CNetworkVar( float, m_flCustomAutoExposureMin );
	CNetworkVar( float, m_flCustomAutoExposureMax );
	CNetworkVar( float, m_flCustomBloomScale);
	CNetworkVar( float, m_flCustomBloomScaleMinimum);
	CNetworkVar( float, m_flBloomExponent);
	CNetworkVar( float, m_flBloomSaturation);
	CNetworkVar( float, m_flTonemapPercentTarget );
	CNetworkVar( float, m_flTonemapPercentBrightPixels );
	CNetworkVar( float, m_flTonemapMinAvgLum );
	CNetworkVar( float, m_flTonemapRate );
};

LINK_ENTITY_TO_CLASS( env_tonemap_controller, CEnvTonemapController );

BEGIN_DATADESC( CEnvTonemapController )
	DEFINE_FIELD( m_bUseCustomAutoExposureMin, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bUseCustomAutoExposureMax, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flCustomAutoExposureMin, FIELD_FLOAT ),
	DEFINE_FIELD( m_flCustomAutoExposureMax, FIELD_FLOAT ),
	DEFINE_FIELD( m_flCustomBloomScale, FIELD_FLOAT ),
	DEFINE_FIELD( m_flCustomBloomScaleMinimum, FIELD_FLOAT ),
	DEFINE_FIELD( m_bUseCustomBloomScale, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flTonemapPercentTarget, FIELD_FLOAT ),
	DEFINE_FIELD( m_flTonemapPercentBrightPixels, FIELD_FLOAT ),
	DEFINE_FIELD( m_flTonemapMinAvgLum, FIELD_FLOAT ),
	DEFINE_FIELD( m_flTonemapRate, FIELD_FLOAT ),

	DEFINE_FIELD( m_flBloomExponent, FIELD_FLOAT ),
	DEFINE_FIELD( m_flBloomSaturation, FIELD_FLOAT ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetTonemapRate", InputSetTonemapRate ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetAutoExposureMin", InputSetAutoExposureMin ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetAutoExposureMax", InputSetAutoExposureMax ),
	DEFINE_INPUTFUNC( FIELD_VOID, "UseDefaultAutoExposure", InputUseDefaultAutoExposure ),
	DEFINE_INPUTFUNC( FIELD_VOID, "UseDefaultBloomScale", InputUseDefaultBloomScale ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetBloomScale", InputSetBloomScale ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetBloomScaleRange", InputSetBloomScaleRange ),

	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetBloomExponent", InputSetBloomExponent ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetBloomSaturation", InputSetBloomSaturation ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetTonemapPercentTarget", InputSetTonemapPercentTarget ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetTonemapPercentBrightPixels", InputSetTonemapPercentBrightPixels ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetTonemapMinAvgLum", InputSetTonemapMinAvgLum ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CEnvTonemapController, DT_EnvTonemapController )
	SendPropInt( SENDINFO(m_bUseCustomAutoExposureMin), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_bUseCustomAutoExposureMax), 1, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_bUseCustomBloomScale), 1, SPROP_UNSIGNED ),
	SendPropFloat( SENDINFO(m_flCustomAutoExposureMin), 0, SPROP_NOSCALE),
	SendPropFloat( SENDINFO(m_flCustomAutoExposureMax), 0, SPROP_NOSCALE),
	SendPropFloat( SENDINFO(m_flCustomBloomScale), 0, SPROP_NOSCALE),
	SendPropFloat( SENDINFO(m_flCustomBloomScaleMinimum), 0, SPROP_NOSCALE),

	SendPropFloat( SENDINFO(m_flBloomExponent), 0, SPROP_NOSCALE),
	SendPropFloat( SENDINFO(m_flBloomSaturation), 0, SPROP_NOSCALE),
	SendPropFloat( SENDINFO(m_flTonemapPercentTarget), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flTonemapPercentBrightPixels), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flTonemapMinAvgLum), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO(m_flTonemapRate), 0, SPROP_NOSCALE ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvTonemapController::CEnvTonemapController()
{
	m_flBloomExponent = 2.5f;
	m_flBloomSaturation = 1.0f;
	m_flTonemapPercentTarget = 65.0f;
	m_flTonemapPercentBrightPixels = 2.0f;
	m_flTonemapMinAvgLum = 3.0f;
	m_flTonemapRate = 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::Spawn( void )
{
	SetSolid( SOLID_NONE );
	SetMoveType( MOVETYPE_NONE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CEnvTonemapController::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}

//-----------------------------------------------------------------------------
// Purpose: set a base and minimum bloom scale
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetBloomScaleRange( inputdata_t &inputdata )
{
	float bloom_max=1, bloom_min=1;
	int nargs=sscanf("%f %f",inputdata.value.String(), bloom_max, bloom_min );
	if (nargs != 2)
	{
		Warning("%s (%s) received SetBloomScaleRange input without 2 arguments. Syntax: <max bloom> <min bloom>\n", GetClassname(), GetDebugName() );
		return;
	}
	m_flCustomBloomScale=bloom_max;
	m_flCustomBloomScale=bloom_min;
}

//-----------------------------------------------------------------------------
// Purpose: Set the auto exposure min to the specified value
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetAutoExposureMin( inputdata_t &inputdata )
{
	m_flCustomAutoExposureMin = inputdata.value.Float();
	m_bUseCustomAutoExposureMin = true;
}

//-----------------------------------------------------------------------------
// Purpose: Set the auto exposure max to the specified value
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetAutoExposureMax( inputdata_t &inputdata )
{
	m_flCustomAutoExposureMax = inputdata.value.Float();
	m_bUseCustomAutoExposureMax = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputUseDefaultAutoExposure( inputdata_t &inputdata )
{
	m_bUseCustomAutoExposureMin = false;
	m_bUseCustomAutoExposureMax = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetBloomScale( inputdata_t &inputdata )
{
	m_flCustomBloomScale = inputdata.value.Float();
	m_flCustomBloomScaleMinimum = m_flCustomBloomScale;
	m_bUseCustomBloomScale = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputUseDefaultBloomScale( inputdata_t &inputdata )
{
	m_bUseCustomBloomScale = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetBloomExponent( inputdata_t &inputdata )
{
	m_flBloomExponent = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetBloomSaturation( inputdata_t &inputdata )
{
	m_flBloomSaturation = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetTonemapPercentTarget( inputdata_t &inputdata )
{
	m_flTonemapPercentTarget = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetTonemapPercentBrightPixels( inputdata_t &inputdata )
{
	m_flTonemapPercentBrightPixels= inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetTonemapMinAvgLum( inputdata_t &inputdata )
{
	m_flTonemapMinAvgLum = inputdata.value.Float();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEnvTonemapController::InputSetTonemapRate( inputdata_t &inputdata )
{
	m_flTonemapRate = inputdata.value.Float();
}

//--------------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( trigger_tonemap, CTonemapTrigger );

BEGIN_DATADESC( CTonemapTrigger )
	DEFINE_KEYFIELD( m_tonemapControllerName,	FIELD_STRING,	"TonemapName" ),
END_DATADESC()


//--------------------------------------------------------------------------------------------------------
void CTonemapTrigger::Spawn( void )
{
	AddSpawnFlags( SF_TRIGGER_ALLOW_CLIENTS );

	BaseClass::Spawn();
	InitTrigger();

	m_hTonemapController = gEntList.FindEntityByName( NULL, m_tonemapControllerName );
}


//--------------------------------------------------------------------------------------------------------
void CTonemapTrigger::StartTouch( CBaseEntity *other )
{
	if ( !PassesTriggerFilters( other ) )
		return;

	BaseClass::StartTouch( other );

	CBasePlayer *player = ToBasePlayer( other );
	if ( !player )
		return;

	player->OnTonemapTriggerStartTouch( this );
}


//--------------------------------------------------------------------------------------------------------
void CTonemapTrigger::EndTouch( CBaseEntity *other )
{
	if ( !PassesTriggerFilters( other ) )
		return;

	BaseClass::EndTouch( other );

	CBasePlayer *player = ToBasePlayer( other );
	if ( !player )
		return;

	player->OnTonemapTriggerEndTouch( this );
}


//-----------------------------------------------------------------------------
// Purpose: Clear out the tonemap controller.
//-----------------------------------------------------------------------------
void CTonemapSystem::LevelInitPreEntity( void )
{
	m_hMasterController = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: On level load find the master tonemap controller.  If no controller is 
//			set as Master, use the first tonemap controller found.
//-----------------------------------------------------------------------------
void CTonemapSystem::LevelInitPostEntity( void )
{
	// Overall master controller
	CEnvTonemapController *pTonemapController = NULL;
	do
	{
		pTonemapController = static_cast<CEnvTonemapController*>( gEntList.FindEntityByClassname( pTonemapController, "env_tonemap_controller" ) );
		if ( pTonemapController )
		{
			if ( m_hMasterController == NULL )
			{
				m_hMasterController = pTonemapController;
			}
			else
			{
				if ( pTonemapController->IsMaster() )
				{
					m_hMasterController = pTonemapController;
				}
			}
		}
	} while ( pTonemapController );

	
}


//--------------------------------------------------------------------------------------------------------
CTonemapSystem s_TonemapSystem( "TonemapSystem" );


//--------------------------------------------------------------------------------------------------------
CTonemapSystem *TheTonemapSystem( void )
{
	return &s_TonemapSystem;
}


//--------------------------------------------------------------------------------------------------------
