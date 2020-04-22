//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Game-specific impact effect hooks
//
//=============================================================================//
#include "cbase.h"

#include "c_te_effect_dispatch.h"
#include "tempent.h"
#include "c_te_legacytempents.h"
#include "c_cs_player.h"
#include "c_rope.h"

// NOTE: Always include this last!
#include "tier0/memdbgon.h"

extern int		g_sModelXmasBulb;	// holds the sprite index for splattered blood

// Used for cycling so that they're sequencially ordered on each strand
int g_nHolidayLightColor = 0;

// 4 light colors
Color g_rgbaHolidayRed( 255, 0, 0, 255 );
Color g_rgbaHolidayYellow( 2, 110, 197, 255 );
Color g_rgbaHolidayGreen( 117, 193, 8, 255 );
Color g_rgbaHolidayBlue( 255, 151, 29, 255 );

Color g_rgbaHalloweenOrange( 239, 136, 15, 255 );
Color g_rgbaHalloweenPurple( 128, 46, 204, 255 );

Color *(rgbaHolidayLightColors[]) = { &g_rgbaHolidayRed, 
									  &g_rgbaHolidayYellow, 
									  &g_rgbaHolidayGreen, 
									  &g_rgbaHolidayBlue };

Color *( rgbaHalloweenLightColors[] ) = { &g_rgbaHalloweenOrange,
										&g_rgbaHalloweenPurple };


struct HolidayLightData_t
{
	Vector vOrigin;
	int nID;
	int nSubID;
	float fScale;
};


void CreateHolidayLight( const HolidayLightData_t &holidayLight );


class CHolidayLightManager : public CAutoGameSystemPerFrame
{
public:
	explicit CHolidayLightManager( char const *name );

	// Methods of IGameSystem
	virtual void Update( float frametime );

	virtual void LevelInitPostEntity( void );
	virtual void LevelShutdownPreEntity();

	void AddHolidayLight( const CEffectData &data );

private:

	CUtlVector< HolidayLightData_t > m_PendingLightData;
};

CHolidayLightManager g_CHolidayLightManager( "CHolidayLightManager" );


CHolidayLightManager::CHolidayLightManager( char const *name ) : CAutoGameSystemPerFrame( name )
{
}

// Methods of IGameSystem
void CHolidayLightManager::Update( float frametime )
{
	for ( int i = 0; i < m_PendingLightData.Count(); ++i )
	{
		CreateHolidayLight( m_PendingLightData[ i ] );
	}

	m_PendingLightData.RemoveAll();
}

void CHolidayLightManager::LevelInitPostEntity( void )
{
	m_PendingLightData.RemoveAll();
}

void CHolidayLightManager::LevelShutdownPreEntity()
{
	m_PendingLightData.RemoveAll();
}

void CHolidayLightManager::AddHolidayLight( const CEffectData &data )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	Vector vecPlayerPos = pPlayer->GetNetworkOrigin();
	// Too far away?
	if ( vecPlayerPos.DistTo( data.m_vOrigin ) > 2000.0f )
		return;

	// In the skybox?
	sky3dparams_t *pSky = &(pPlayer->m_Local.m_skybox3d);
	if ( pSky->origin->DistTo( data.m_vOrigin ) < 2000.0f )
		return;

	HolidayLightData_t newData;

	newData.vOrigin = data.m_vOrigin;

	// HACK: Use these ints to ID the light later
	newData.nID = data.m_nMaterial;
	newData.nSubID = data.m_nHitBox;

	// Skybox lights pass in a smaller scale
	newData.fScale = data.m_flScale;

	if ( m_PendingLightData.Count() < CTempEnts::MAX_TEMP_ENTITIES / 2 )
	{
		m_PendingLightData.AddToTail( newData );
	}
}


void CS_HolidayLightCallback( const CEffectData &data )
{
	g_CHolidayLightManager.AddHolidayLight( data );
}

#define XMAS_LIGHT_MODEL "models/props/holiday_light/holiday_light.mdl" 
void CreateHolidayLight( const HolidayLightData_t &holidayLight )
{
	int nHolidayLightStyle = RopeManager()->GetHolidayLightStyle();

	model_t *pModel = ( nHolidayLightStyle == 0 ? (model_t *)engine->LoadModel( "effects/christmas_bulb.vmt" ) : (model_t *)engine->LoadModel( XMAS_LIGHT_MODEL ) );
	if ( !pModel )
		return;

	Assert( pModel );

	C_LocalTempEntity *pTemp = tempents->FindTempEntByID( holidayLight.nID, holidayLight.nSubID );

	if ( !pTemp )
	{
		// Didn't find one with that ID, so make a new one!
		// Randomize the angle
		QAngle angOrientation = ( nHolidayLightStyle == 0 ? QAngle( 0.0f, 0.0f, RandomFloat( -180.0f, 180.0f ) ) : vec3_angle );
		//pTemp = tempents->SpawnTempModel( pModel, holidayLight.vOrigin, angOrientation, vec3_origin, 2.0f, FTENT_NEVERDIE );
		pTemp = tempents->SpawnTempModel( pModel, holidayLight.vOrigin, angOrientation, vec3_origin, 2.0f, FTENT_NONE );

		if ( !pTemp )
		{
			return;
		}

		pTemp->clientIndex = 0;

		// HACK: Use these ints to ID the light later
		pTemp->m_nSkin = holidayLight.nID;
		pTemp->hitSound = holidayLight.nSubID;

		pTemp->SetDistanceFade( 1024, 1200 );

		// Skybox lights pass in a smaller scale
		pTemp->m_flSpriteScale = holidayLight.fScale;

		// Smuggle the color index here
		if ( nHolidayLightStyle == 0 )
		{
			pTemp->m_nHitboxSet = g_nHolidayLightColor;

			//Set the color
			{	
				pTemp->SetRenderColor( rgbaHolidayLightColors[ g_nHolidayLightColor ]->r(), 
									   rgbaHolidayLightColors[ g_nHolidayLightColor ]->g(), 
									   rgbaHolidayLightColors[ g_nHolidayLightColor ]->b()/*, 
									   rgbaHolidayLightColors[ g_nHolidayLightColor ]->a() */);
			}
		}
		
		pTemp->SetRenderMode( kRenderTransColor );

		pTemp->SetProxyRandomValue( RandomFloat() );

		// Next color in the pattern
		g_nHolidayLightColor = ( g_nHolidayLightColor + 1 ) % ARRAYSIZE( rgbaHolidayLightColors );
	}
	else
	{
		// Update the position
		pTemp->SetAbsOrigin( holidayLight.vOrigin );

		// Every 10 light strands have a blink cycle
		if ( nHolidayLightStyle == 0 && pTemp->m_nSkin % 5 == 0 )
		{
			// Magic! Basically this makes the on/off cycle of each color different and offsets it by the segment index.
			// That way it looks like a timed pattern but is also chaotic.
			int nCycle = ( pTemp->hitSound + static_cast< int >( gpGlobals->curtime * 2.0f ) ) % ( pTemp->m_nHitboxSet + ARRAYSIZE( rgbaHolidayLightColors ) + 1 );
			pTemp->SetRenderAlpha( nCycle < ARRAYSIZE( rgbaHolidayLightColors ) ? 255 : 64 );
		}

		// Update the scale
		pTemp->m_flSpriteScale = holidayLight.fScale;

		Vector vecMin, vecMax;
		pTemp->GetRenderBounds( vecMin, vecMax );

		// Extend it's life
		pTemp->die = gpGlobals->curtime + 2.0f;
	}
}

DECLARE_CLIENT_EFFECT( CS_HolidayLight, CS_HolidayLightCallback );

void RopesHolidayLightColor( const CCommand &args )
{
	if ( args.ArgC() < 5 )
		return;

	int nLight = atoi( args[ 1 ] );

	{
		if ( nLight < 0 || nLight >= ARRAYSIZE( rgbaHolidayLightColors ) )
			return;

		rgbaHolidayLightColors[nLight]->SetColor( atoi( args[2] ), atoi( args[3] ), atoi( args[4] ) );
	}
}
ConCommand r_ropes_holiday_light_color( "r_ropes_holiday_light_color", RopesHolidayLightColor, "Set each light's color: [light0-3] [r0-255] [g0-255] [b0-255]", FCVAR_NONE );