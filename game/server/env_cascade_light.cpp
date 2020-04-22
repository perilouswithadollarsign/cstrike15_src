//========= Copyright © 1996-2010, Valve Corporation, All rights reserved. ============//
//
// Purpose: global dynamic light with cascaded shadow mapping
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "lights.h"
#include "env_cascade_light.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//#define CsmDbgMsg Msg
#define CsmDbgMsg(x)

ConVar cl_csm_auto_entity( "cl_csm_auto_entity", "1", 0, "" );

CCascadeLight *g_pCascadeLight;

LINK_ENTITY_TO_CLASS(env_cascade_light, CCascadeLight);

BEGIN_DATADESC( CCascadeLight )

	DEFINE_KEYFIELD( m_bEnabled,		FIELD_BOOLEAN, "enabled" ),
	DEFINE_KEYFIELD( m_bStartDisabled,	FIELD_BOOLEAN, "StartDisabled" ),
	DEFINE_FIELD( m_LightColor, FIELD_COLOR32 ), 
	DEFINE_FIELD( m_LightColorScale, FIELD_INTEGER ),

// Inputs

	DEFINE_INPUTFUNC( FIELD_COLOR32, "LightColor", InputSetLightColor ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "LightColorScale", InputSetLightColorScale ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SetAngles", InputSetAngles ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST_NOBASE(CCascadeLight, DT_CascadeLight)
	SendPropVector(SENDINFO(m_shadowDirection), -1,  SPROP_NOSCALE ),
	SendPropVector(SENDINFO(m_envLightShadowDirection), -1,  SPROP_NOSCALE ),
	SendPropBool(SENDINFO(m_bEnabled) ),
	SendPropBool(SENDINFO(m_bUseLightEnvAngles) ),
	SendPropInt(SENDINFO(m_LightColor), 32, SPROP_UNSIGNED, SendProxy_Color32ToInt32 ),
	SendPropInt(SENDINFO(m_LightColorScale), 32, 0, SendProxy_Int32ToInt32 ),
	SendPropFloat(SENDINFO(m_flMaxShadowDist), 0, SPROP_NOSCALE ),
END_SEND_TABLE()

float CCascadeLight::m_flEnvLightShadowPitch;
QAngle CCascadeLight::m_EnvLightShadowAngles;
bool CCascadeLight::m_bEnvLightShadowValid;
color32 CCascadeLight::m_EnvLightColor;
int CCascadeLight::m_EnvLightColorScale;

CCascadeLight::CCascadeLight() : 
	CBaseEntity()
{
	CsmDbgMsg( "CCascadeLight::CCascadeLight\n" );

	m_bEnabled = true;

	color32 tmp = { 255, 255, 255, 1 };
	m_LightColor = tmp;

	m_LightColorScale = 255;
		
	QAngle angles;
	angles.Init( 50, 43, 0 );
	Vector vForward;
	AngleVectors( angles, &vForward );
	m_shadowDirection = vForward;
	//m_shadowDirection.Init( 0.0f, 0.0f, -1.0f );
		
	m_envLightShadowDirection = m_shadowDirection;
	m_bUseLightEnvAngles = true;
	
	m_flMaxShadowDist = 400.0f;

	g_pCascadeLight = this;
}

CCascadeLight::~CCascadeLight()
{
	g_pCascadeLight = NULL;

	CsmDbgMsg( "CCascadeLight::~CCascadeLight\n" );
}

//------------------------------------------------------------------------------
// Purpose : Send even though we don't have a model
//------------------------------------------------------------------------------
int CCascadeLight::UpdateTransmitState()
{
	// ALWAYS transmit to all clients.
	return SetTransmitState( FL_EDICT_ALWAYS );
}


bool CCascadeLight::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "color" ) )
	{
		/* unused?
		float tmp[4];
		UTIL_StringToFloatArray( tmp, 4, szValue );

		m_LightColor.SetR( tmp[0] );
		m_LightColor.SetG( tmp[1] );
		m_LightColor.SetB( tmp[2] );
		m_LightColor.SetA( tmp[3] );*/
	}
	else if ( FStrEq( szKeyName, "angles" ) )
	{
		QAngle angles;
		UTIL_StringToVector( angles.Base(), szValue );
		if (angles == vec3_angle)
		{
			angles.Init( 50, 43, 0 );
		}
		Vector vForward;
		AngleVectors( angles, &vForward );
		m_shadowDirection = vForward;
		return true;
	}
	else if ( FStrEq( szKeyName, "uselightenvangles" ) )
	{
		m_bUseLightEnvAngles = ( atoi( szValue ) != 0 );
		return true;
	}
	else if ( FStrEq( szKeyName, "maxshadowdistance" ) )
	{
		m_flMaxShadowDist = atof( szValue );
		return true;
	}
	
	return BaseClass::KeyValue( szKeyName, szValue );
}

bool CCascadeLight::GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen )
{
	if ( FStrEq( szKeyName, "color" ) )
	{
		// path unused?
		Q_snprintf( szValue, iMaxLen, "%d %d %d %d", m_LightColor.GetR(), m_LightColor.GetG(), m_LightColor.GetB(), m_LightColor.GetA() );
		return true;
	}
	return BaseClass::GetKeyValue( szKeyName, szValue, iMaxLen );
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CCascadeLight::Spawn( void )
{
	Precache();
	SetSolid( SOLID_NONE );

	if( m_bStartDisabled )
	{
		m_bEnabled = false;
	}
	else
	{
		m_bEnabled = true;
	}

	if ( m_bEnvLightShadowValid )
	{
		UpdateEnvLight();
	}

	//SetClassname( "cascadelight" );

	BaseClass::Spawn();
}

void CCascadeLight::Release( void )
{
	g_pCascadeLight = NULL;
}



//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CCascadeLight::OnActivate()
{
}

//------------------------------------------------------------------------------
// Purpose :
//------------------------------------------------------------------------------
void CCascadeLight::OnDeactivate()
{
}

//------------------------------------------------------------------------------
// Input values
//------------------------------------------------------------------------------
void CCascadeLight::InputSetAngles( inputdata_t &inputdata )
{
	const char *pAngles = inputdata.value.String();

	QAngle angles;
	UTIL_StringToVector( angles.Base(), pAngles );

	Vector vTemp;
	AngleVectors( angles, &vTemp );
	m_shadowDirection = vTemp;
}

//------------------------------------------------------------------------------
// Purpose : Input handlers
//------------------------------------------------------------------------------
void CCascadeLight::InputEnable( inputdata_t &inputdata )
{
	m_bEnabled = true;
	if ( g_pCascadeLight )
	{
		g_pCascadeLight->UpdateEnvLight();
	}
}

void CCascadeLight::InputDisable( inputdata_t &inputdata )
{
	m_bEnabled = false;
	if ( g_pCascadeLight )
	{
		g_pCascadeLight->UpdateEnvLight();
	}
}

void CCascadeLight::InputSetLightColor( inputdata_t &inputdata )
{
	m_LightColor = inputdata.value.Color32();
}

void CCascadeLight::InputSetLightColorScale( inputdata_t &inputdata )
{
	m_LightColorScale = inputdata.value.Int();
	if ( g_pCascadeLight )
	{
		g_pCascadeLight->UpdateEnvLight();
	}
}

void CCascadeLight::SetLightColor( int r, int g, int b, int a )
{
	m_EnvLightColor.r = r;
	m_EnvLightColor.g = g;
	m_EnvLightColor.b = b;

	m_EnvLightColor.a = 0; // use light scale as potentially > 255

	m_EnvLightColorScale = a;

	if ( g_pCascadeLight )
	{
		g_pCascadeLight->UpdateEnvLight();
	}
}

void CCascadeLight::SetEnabled( bool bEnable )
{
	m_bEnabled = bEnable;
}

void CCascadeLight::UpdateEnvLight()
{
	QAngle angles;
	angles.x = -m_flEnvLightShadowPitch;
	angles.y = m_EnvLightShadowAngles.y;
	angles.z = 0;
		
	Vector vForward;
	AngleVectors( angles, &vForward );
	m_envLightShadowDirection = vForward;

	m_LightColor		= m_EnvLightColor;
	m_LightColorScale	= m_EnvLightColorScale;
}

void CCascadeLight::SetEnvLightShadowPitch( float flPitch ) 
{ 
	m_flEnvLightShadowPitch = flPitch; 
	m_bEnvLightShadowValid = true;

	if ( g_pCascadeLight )
	{
		g_pCascadeLight->UpdateEnvLight();
	}
}

void CCascadeLight::SetEnvLightShadowAngles( const QAngle &angles ) 
{ 
	m_EnvLightShadowAngles = angles; 
	m_bEnvLightShadowValid = true;

	if ( g_pCascadeLight )
	{
		g_pCascadeLight->UpdateEnvLight();
	}
}

class CCSMLightManager : public CAutoGameSystemPerFrame
{
public:
	CCSMLightManager()
	{
	}
	
	virtual ~CCSMLightManager()
	{
	}

	virtual void LevelInitPreEntity()
	{
		CsmDbgMsg( "**** LevelInitPreEntity\n" );
	}

	virtual void LevelInitPostEntity()
	{
		CsmDbgMsg( "**** LevelInitPostEntity\n" );

		if ( !cl_csm_auto_entity.GetBool() )
			return;
		if ( g_pCascadeLight ) 
			return;
				
		// Create the env_cascade_light automatically for cs:go - this is a hack that will hopefully go away as we add the entity to all of our maps.
		CBaseEntity *entity = dynamic_cast< CBaseEntity * >( CreateEntityByName( "env_cascade_light" ) );
		if (entity)
		{
			entity->Precache();
			entity->KeyValue( "targetname", "cascadelight" );
			DispatchSpawn(entity);
		}
	}

	virtual void LevelShutdownPreEntity()
	{
		CsmDbgMsg( "**** LevelShutdownPreEntity\n" );
	}
	
	virtual void LevelShutdownPostEntity()
	{
		CsmDbgMsg( "**** LevelShutdownPostEntity\n" );
	}

	virtual void Shutdown()
	{
		CsmDbgMsg( "**** Shutdown\n" );
	}
};

CCSMLightManager g_CSMLightManager;

void C_CSM_Server_Status( const CCommand& args )
{
	Msg( "Entity exists: %u\n", g_pCascadeLight != NULL );
}

static ConCommand cl_csm_server_status("cl_csm_server_status", C_CSM_Server_Status, "Usage:\n cl_csm_server_status\n", 0);
