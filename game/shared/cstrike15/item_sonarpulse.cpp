//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "item_sonarpulse.h"

#ifdef CLIENT_DLL
#include "view.h"
#include "materialsystem/imaterialvar.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SONARPULSE_MODEL "models/props/props_br/br_sonar/br_sonar.mdl"
#define SONARPULSE_SPEED 270 // in units/sec
#define SONARPULSE_PRICE 1000 // in american dollars

#ifdef CLIENT_DLL
CUtlVector<CSonarPulse*> g_SonarPulsers;
CUtlVector<sonarpulseicon_t> g_SonarPulseIcons;
#endif

#if defined( CLIENT_DLL )

#else

BEGIN_DATADESC( CSonarPulse )
END_DATADESC()

#endif

BEGIN_PREDICTION_DATA( CSonarPulse )
END_PREDICTION_DATA()

IMPLEMENT_NETWORKCLASS_ALIASED( SonarPulse, DT_SonarPulse )
BEGIN_NETWORK_TABLE( CSonarPulse, DT_SonarPulse )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO(m_bPulseInProgress) ),
	RecvPropFloat( RECVINFO(m_flPulseInitTime) )
#else
	SendPropBool( SENDINFO(m_bPulseInProgress) ),
	SendPropFloat( SENDINFO(m_flPulseInitTime) )
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS_ALIASED( item_sonarpulse, SonarPulse );
PRECACHE_REGISTER( item_sonarpulse );

CSonarPulse::CSonarPulse()
{
#ifndef CLIENT_DLL
	m_bPulseInProgress = false;
	m_flPulseInitTime = 0;
	m_vecPlayersOutsidePulse.RemoveAll();
	m_vecPingedPlayers.RemoveAll();
	m_flPlaybackRate = 0;
#else
	ListenForGameEvent( "add_player_sonar_icon" );

	g_SonarPulsers.FindAndFastRemove( this );
	g_SonarPulsers.AddToTail( this );

	m_pIconMaterial = NULL;

	g_SonarPulseIcons.RemoveAll();

#endif
}

CSonarPulse::~CSonarPulse()
{
#ifdef CLIENT_DLL
	g_SonarPulsers.FindAndFastRemove( this );
#endif
}

void CSonarPulse::Spawn( void )
{
	BaseClass::Spawn();

	SetMoveType( MOVETYPE_NONE );
	SetSolid( SOLID_VPHYSICS );

	SetModel( SONARPULSE_MODEL );

#ifdef CLIENT_DLL
	m_pIconMaterial = materials->FindMaterial( "dev/sonar_icon.vmt", TEXTURE_GROUP_OTHER, false );

	SetThink( &CSonarPulse::ClientThink );
	SetNextClientThink( CLIENT_THINK_ALWAYS );

#else
	SetPlaybackRate( 0 );
#endif

}


void CSonarPulse::Precache()
{
	PrecacheModel( SONARPULSE_MODEL );
	PrecacheSound( "ambient/atmosphere/cs_metalscrapeverb10.wav" );
}

float CSonarPulse::GetPulseRadius( void )
{
	if ( !m_bPulseInProgress )
		return 0;

	float flCurrentDuration = gpGlobals->curtime - m_flPulseInitTime;
	return flCurrentDuration * SONARPULSE_SPEED;
}

Vector CSonarPulse::GetPulseOrigin( void )
{
	return GetAbsOrigin();
}

#ifdef CLIENT_DLL

void CSonarPulse::ClientThink( void )
{
	StudioFrameAdvance();
	SetNextClientThink( CLIENT_THINK_ALWAYS );
}

void CSonarPulse::RenderIcons( void )
{
	if( !g_SonarPulseIcons.Count() || m_pIconMaterial == NULL )
		return;	
	
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return;

	Vector vecEyePos = MainViewOrigin(GET_ACTIVE_SPLITSCREEN_SLOT());
	
	CMatRenderContextPtr pRenderContext(materials);
	FOR_EACH_VEC_BACK( g_SonarPulseIcons, n )
	{
		float flLifeTimeRemaining = g_SonarPulseIcons[n].GetLifeRemaining();
		if ( flLifeTimeRemaining > 0 )
		{
			Vector vecIconPos = g_SonarPulseIcons[n].m_vecPos;
			
			Vector vecOrigin;
			ScreenTransform( vecIconPos, vecOrigin );

			float flSize = RemapValClamped( flLifeTimeRemaining, 10, 9, 1, 32 );
			float flEdgeAlpha = 1.0f;

			float flTheta = 0.95f;
			if ( vecOrigin.Length() > flTheta )
			{
				vecOrigin = vecOrigin.Normalized() * flTheta;
				flSize = MIN( flSize, 24 );
				flEdgeAlpha = 0.7f;
			}
			ConvertNormalizedScreenSpaceToPixelScreenSpace( vecOrigin );			

			//scale size by distance a bit, as per Brian's request.
			flSize *= RemapValClamped( vecEyePos.DistToSqr( vecIconPos ), 1600*1600, 500*500, 0.5f, 1.0f );

			vecOrigin.x -= flSize * 0.5f;
			vecOrigin.y -= flSize * 0.5f;

			IMaterialVar* pVar = m_pIconMaterial->FindVar( "$alpha", NULL );
			if ( pVar )
			{
				pVar->SetFloatValue( flEdgeAlpha * clamp( flLifeTimeRemaining * 0.5f, 0, 1 ) );
			}

			pRenderContext->DrawScreenSpaceRectangle( m_pIconMaterial, 
									vecOrigin.x, vecOrigin.y,
									flSize, flSize,
									0, 0, 32, 32, 32, 32 );
		}
		else
		{
			g_SonarPulseIcons.FastRemove( n );
		}
	}
}

void CSonarPulse::FireGameEvent( IGameEvent *event )
{
	if ( Q_strcmp( event->GetName(), "add_player_sonar_icon" ) == 0 )
	{
		// don't make an icon for the local player - it's confusing
		int nUserID = event->GetInt( "userid", -1 );
		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pLocalPlayer && pLocalPlayer->GetUserID() == nUserID )
			return;

		Vector vecPos = Vector( event->GetFloat("pos_x",0), event->GetFloat("pos_y",0), event->GetFloat("pos_z",0) );
		sonarpulseicon_t newIcon( vecPos );
		g_SonarPulseIcons.AddToTail( newIcon );

		//CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByUserId( nUserID ) );
		//ConColorMsg( Color(0,255,255,255), "RECEIVE: sonar icon added at [%f, %f, %f] for player: %s\n", vecPos.x, vecPos.y, vecPos.z, pPlayer ? pPlayer->GetPlayerName() : "null" );
	}
}
#endif


#ifndef CLIENT_DLL
void CSonarPulse::PulseStart( void )
{
	EmitSound( "ambient/atmosphere/cs_metalscrapeverb10.wav" );
	
	m_vecPlayersOutsidePulse.RemoveAll();
	m_vecPingedPlayers.RemoveAll();

	m_bPulseInProgress = true;
	m_flPulseInitTime = gpGlobals->curtime;

	SetPlaybackRate( 1 );

	SetThink( &CSonarPulse::PulseThink );
	SetNextThink( gpGlobals->curtime );
}

void CSonarPulse::PulseReset( void )
{
	SetThink( NULL );
	SetPlaybackRate( 0 );
	m_bPulseInProgress = false;
	m_flPulseInitTime = 0;
	m_vecPlayersOutsidePulse.RemoveAll();
	m_vecPingedPlayers.RemoveAll();
	m_flPlaybackRate = 0;
}

bool CSonarPulse::IsOkToPulse( CCSPlayer* pPlayer )
{
	return ( pPlayer && pPlayer->IsAlive() && !pPlayer->IsChickenClass() && !pPlayer->IsFlyingDroneClass() && !pPlayer->m_bIsParachuting );
}

void CSonarPulse::PulseThink( void )
{
	if ( !m_bPulseInProgress )
	{
		PulseReset();
		return;
	}

	float flRadius = GetPulseRadius();
	Vector vecPulseCenter = GetPulseOrigin();
	float flRadSqrCurrent = flRadius * flRadius;

	// ping the players that were outside the pulse last think and are now inside
	FOR_EACH_VEC_BACK( m_vecPlayersOutsidePulse, n )
	{
		CCSPlayer *pPlayer = ToCSPlayer( m_vecPlayersOutsidePulse[n] );
		if ( IsOkToPulse( pPlayer ) )
		{
			Vector vecPlayerPos = pPlayer->GetAbsOrigin() + Vector(0,0,40);
			float flPlayerDist = vecPlayerPos.DistToSqr( vecPulseCenter );

			if ( flPlayerDist < flRadSqrCurrent && !m_vecPingedPlayers.HasElement( pPlayer ) )
			{
				// The pulse crossed this player - ping them!
				pPlayer->EmitSound( "Bot.StuckSound" );

				IGameEvent * sonar_ping_event = gameeventmanager->CreateEvent( "add_player_sonar_icon" );
				if ( sonar_ping_event )
				{
					sonar_ping_event->SetInt( "userid", pPlayer->GetUserID() );
					sonar_ping_event->SetFloat( "pos_x", vecPlayerPos.x );
					sonar_ping_event->SetFloat( "pos_y", vecPlayerPos.y );
					sonar_ping_event->SetFloat( "pos_z", vecPlayerPos.z );
					gameeventmanager->FireEvent( sonar_ping_event );
				}

				m_vecPlayersOutsidePulse.FindAndRemove( pPlayer );
				m_vecPingedPlayers.AddToTail( pPlayer );
				//ConColorMsg( Color(0,255,0,255), "SEND:    sonar icon added at [%f, %f, %f] for player: %s\n", vecPlayerPos.x, vecPlayerPos.y, vecPlayerPos.z, pPlayer->GetPlayerName() );
			}
		}
		else
		{
			// player is invalid for some reason, might have left, been killed, etc
			m_vecPlayersOutsidePulse.FindAndRemove( pPlayer );
		}
	}

	// re-gather players to add to the list that are outside the pulse
	for ( int i = 1; i <= MAX_PLAYERS; i++ )
	{
		CCSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		if ( IsOkToPulse( pPlayer ) )
		{
			Vector vecPlayerPos = pPlayer->GetAbsOrigin() + Vector(0,0,40);
			float flPlayerDist = vecPlayerPos.DistToSqr( vecPulseCenter );

			if ( flPlayerDist > flRadSqrCurrent && !m_vecPlayersOutsidePulse.HasElement(pPlayer) )
			{
				m_vecPlayersOutsidePulse.AddToTail( pPlayer );
			}
		}
	}

	if ( m_vecPlayersOutsidePulse.Count() == 0 )
	{
		PulseReset();
		return;
	}

	SetNextThink( gpGlobals->curtime + 0.1f );	
}

void CSonarPulse::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CCSPlayer *pPlayer = dynamic_cast< CCSPlayer* >( pActivator );
	if ( !pPlayer )
		return;

	int nCost = SONARPULSE_PRICE;
	if ( m_bPulseInProgress || pPlayer->GetAccountBalance() < nCost )
	{
		// we dont have enough money
		// play an error sound
		Vector soundPosition = GetAbsOrigin() + Vector( 0, 0, 32 );
		CPASAttenuationFilter filter( soundPosition );
		EmitSound( filter, 0, "Vote.Failed", &GetAbsOrigin() );
	
		return;
	}
	
	// deduct the amount
	pPlayer->AddAccount( -nCost, false, true, "" );

	m_bPulseInProgress = true;

	SetThink( &CSonarPulse::PulseStart );
	SetNextThink( gpGlobals->curtime + 1 );

	EmitSound( "UI.Guardian.TooFarWarning" );

}

#endif // #ifndef CLIENT_DLL

