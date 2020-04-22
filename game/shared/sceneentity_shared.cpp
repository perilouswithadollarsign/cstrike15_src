//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "sceneentity_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar scene_print( "scene_print", "0", FCVAR_REPLICATED, "When playing back a scene, print timing and event info to console." );
ConVar scene_clientflex( "scene_clientflex", "1", FCVAR_REPLICATED, "Do client side flex animation." );


CSceneEventInfo::CSceneEventInfo()
	:
m_pEvent( 0 ),
m_pScene( 0 ),
m_pActor( 0 ),
m_hSceneEntity( INVALID_EHANDLE ),
m_bStarted( false ),
m_iLayer( -1 ),
m_iPriority( 0 ),
m_nSequence( 0 ),
m_bIsGesture( false ),
m_flWeight( 0.0f ),
m_hTarget(),
m_bIsMoving( false ),
m_bHasArrived( false ),
m_flInitialYaw( 0.0f ),
m_flTargetYaw( 0.0f ),
m_flFacingYaw( 0.0f ),
m_nType( 0 ),
m_flNext( 0.0f ),
m_bClientSide( false ),
m_pExpHdr( NULL )
{
}

CSceneEventInfo::~CSceneEventInfo()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pFormat - 
//			... - 
// Output : static void
//-----------------------------------------------------------------------------
void Scene_Printf( const char *pFormat, ... )
{
	int val = scene_print.GetInt();
	if ( !val )
		return;

	if ( val >= 2 )
	{
		if ( CBaseEntity::IsServer() && val != 2 )
		{
			return;
		}
        else if ( !CBaseEntity::IsServer() && val != 3 )
		{
			return;
		}
	}

	va_list marker;
	char msg[8192];

	va_start(marker, pFormat);
	Q_vsnprintf(msg, sizeof(msg), pFormat, marker);
	va_end(marker);	
	
	Msg( "%8.3f[%d] %s:  %s", gpGlobals->curtime, gpGlobals->tickcount, CBaseEntity::IsServer() ? "sv" : "cl", msg );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : const char
//-----------------------------------------------------------------------------
const char *CSceneTokenProcessor::CurrentToken( void )
{
	return m_szToken;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : crossline - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSceneTokenProcessor::GetToken( bool crossline )
{
	// NOTE: crossline is ignored here, may need to implement if needed
	m_pBuffer = engine->ParseFile( m_pBuffer, m_szToken, sizeof( m_szToken ) );
	if ( 0 <= (int)strlen( m_szToken ) )
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSceneTokenProcessor::TokenAvailable( void )
{
	char const *search_p = m_pBuffer;

	while ( *search_p <= 32)
	{
		if (*search_p == '\n')
			return false;
		search_p++;
		if ( !*search_p )
			return false;

	}

	if (*search_p == ';' || *search_p == '#' ||		 // semicolon and # is comment field
		(*search_p == '/' && *((search_p)+1) == '/')) // also make // a comment field
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *fmt - 
//			... - 
//-----------------------------------------------------------------------------
void CSceneTokenProcessor::Error( const char *fmt, ... )
{
	char string[ 2048 ];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( string, sizeof(string), fmt, argptr );
	va_end( argptr );

	Warning( "%s", string );
	Assert(0);
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *buffer - 
//-----------------------------------------------------------------------------
void CSceneTokenProcessor::SetBuffer( char *buffer )
{
	m_pBuffer = buffer;
}

CSceneTokenProcessor g_TokenProcessor;
