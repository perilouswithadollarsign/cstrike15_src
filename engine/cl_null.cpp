//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: replaces the cl_*.cpp files with stubs
//
//=============================================================================//

#include "client_pch.h"
#ifdef DEDICATED
#include "hltvclientstate.h"
#include "netconsole.h"
#include "convar.h"
#include "enginestats.h"
#include "bspfile.h" // dworldlight_t
#include "audio/public/soundservice.h"

ISoundServices *g_pSoundServices = NULL;
Vector		listener_origin;

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	MAXPRINTMSG	4096

CEngineStats g_EngineStats;

ClientClass *g_pClientClassHead = NULL;

bool g_bReplayLoadedTools = false;

bool CL_IsHL2Demo()
{
	return false;
}

bool CL_IsPortalDemo()
{
	return false;
}

bool HandleRedirectAndDebugLog( const char *msg );

void BeginLoadingUpdates( MaterialNonInteractiveMode_t mode ) {}
void RefreshScreenIfNecessary() {}
void EndLoadingUpdates() {}


void Con_ColorPrintf( const Color& clr, const char *fmt, ... )
{
    va_list argptr; 
	char		msg[MAXPRINTMSG];
	static bool	inupdate;

	if ( !host_initialized )
		return;

	va_start (argptr,fmt);
	Q_vsnprintf (msg,sizeof( msg ), fmt,argptr);
	va_end (argptr);

	if ( !HandleRedirectAndDebugLog( msg ) )
	{
		return;
	}

	SendStringToNetConsoles( msg );
#if defined( LINUX )
	// linux prints output elsewhere.  This disables standard printf's to keep from double printing all linux output
#elif POSIX
	printf( "%s", msg );
#else
	Msg( "%s", msg );
#endif
}

void Con_NPrintf( int pos, const char *fmt, ... )
{
	va_list		argptr;
	char		text[4096];
	va_start (argptr, fmt);
	Q_vsnprintf(text, sizeof( text ), fmt, argptr);
	va_end (argptr);

	return;
//	printf( "%s", text );
}

void SCR_UpdateScreen (void)
{
}

void SCR_EndLoadingPlaque (void)
{
}

void ClientDLL_FrameStageNotify( ClientFrameStage_t frameStage )
{
}

ClientClass *ClientDLL_GetAllClasses( void )
{
	return g_pClientClassHead;
}

#define LIGHT_MIN_LIGHT_VALUE 0.03f

float ComputeLightRadius( dworldlight_t *pLight, bool bIsHDR )
{
	float flLightRadius = pLight->radius;
	if (flLightRadius == 0.0f)
	{
		// Compute the light range based on attenuation factors
		float flIntensity = sqrtf( DotProduct( pLight->intensity, pLight->intensity ) );
		if (pLight->quadratic_attn == 0.0f)
		{
			if (pLight->linear_attn == 0.0f)
			{
				// Infinite, but we're not going to draw it as such
				flLightRadius = 2000;
			}
			else
			{
				flLightRadius = (flIntensity / LIGHT_MIN_LIGHT_VALUE - pLight->constant_attn) / pLight->linear_attn;
			}
		}
		else
		{
			float a = pLight->quadratic_attn;
			float b = pLight->linear_attn;
			float c = pLight->constant_attn - flIntensity / LIGHT_MIN_LIGHT_VALUE;
			float discrim = b * b - 4 * a * c;
			if (discrim < 0.0f)
			{
				// Infinite, but we're not going to draw it as such
				flLightRadius = 2000;
			}
			else
			{
				flLightRadius = (-b + sqrtf(discrim)) / (2.0f * a);
				if (flLightRadius < 0)
					flLightRadius = 0;
			}
		}
	}

	return flLightRadius;
}



CClientState::CClientState() {}
CClientState::~CClientState() {}
void CClientState::ConnectionStart( INetChannel *chan ){}
void CClientState::ConnectionStop(){}
bool CClientState::SVCMsg_HltvReplay( const CSVCMsg_HltvReplay &msg ){ return false; }
void CClientState::ConnectionClosing( const char * reason ) {}
void CClientState::ConnectionCrashed( const char * reason ) {}
bool CClientState::ProcessConnectionlessPacket( netpacket_t *packet ){ return false; }
void CClientState::PacketStart(int incoming_sequence, int outgoing_acknowledged) {}
void CClientState::PacketEnd( void ) {}
void CClientState::FileRequested(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile ) {}
void CClientState::Disconnect( bool showmainmenu  ) {}
void CClientState::FullConnect( const ns_address &adr, int nEncryptionKey ) {}
bool CClientState::SetSignonState ( int state, int count, const CNETMsg_SignonState *msg ) { return false;}
void CClientState::SendClientInfo( void ) {}
void CClientState::SendLoadingProgress( int nProgress ) {}
void CClientState::SendServerCmdKeyValues( KeyValues *pKeyValues ) {}
void CClientState::InstallStringTableCallback( char const *tableName ) {}
bool CClientState::InstallEngineStringTableCallback( char const *tableName ) { return false;}
void CClientState::ReadPacketEntities( CEntityReadInfo &u ) {}
const char *CClientState::GetCDKeyHash( void ) { return "123";}
void CClientState::Clear( void ) {}
bool CClientState::SVCMsg_UserMessage( const CSVCMsg_UserMessage& msg ) { return true; }
bool CClientState::SVCMsg_GameEvent( const CSVCMsg_GameEvent& msg) { return true; }
bool CClientState::SVCMsg_BSPDecal( const CSVCMsg_BSPDecal& msg ) OVERRIDE { return true; }
bool CClientState::SVCMsg_CrosshairAngle( const CSVCMsg_CrosshairAngle& msg ) OVERRIDE { return true; }
bool CClientState::SVCMsg_FixAngle( const CSVCMsg_FixAngle &msg ) OVERRIDE { return true; }
bool CClientState::SVCMsg_VoiceData( const CSVCMsg_VoiceData& msg ) OVERRIDE { return true; }
bool CClientState::SVCMsg_VoiceInit( const CSVCMsg_VoiceInit& msg ) OVERRIDE { return true; }
bool CClientState::SVCMsg_SetPause( const CSVCMsg_SetPause& msg ) OVERRIDE { return true; } 
bool CClientState::SVCMsg_ClassInfo( const CSVCMsg_ClassInfo& msg ) OVERRIDE { return true; }
bool CClientState::NETMsg_StringCmd( const CNETMsg_StringCmd& msg ) OVERRIDE { return true; }
bool CClientState::SVCMsg_ServerInfo( const CSVCMsg_ServerInfo& msg ) OVERRIDE { return true; }
bool CClientState::NETMsg_Tick( const CNETMsg_Tick& msg ) OVERRIDE { return true; }
bool CClientState::SVCMsg_TempEntities( const CSVCMsg_TempEntities& msg ) { return true; }
bool CClientState::SVCMsg_PacketEntities( const CSVCMsg_PacketEntities& msg ) { return true; }
bool CClientState::SVCMsg_Sounds( const CSVCMsg_Sounds& msg ) OVERRIDE { return true; }
bool CClientState::SVCMsg_Prefetch( const CSVCMsg_Prefetch& msg ) OVERRIDE { return true; }
bool CClientState::SVCMsg_PaintmapData( const CSVCMsg_PaintmapData& msg ) { return true; }
bool CClientState::SVCMsg_EntityMsg( const CSVCMsg_EntityMsg& msg ) { return true; }
float CClientState::GetTime() const { return 0.0f;}
void CClientState::FileDenied(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile ){}
void CClientState::FileReceived( const char * fileName, unsigned int transferID, bool bIsReplayDemoFile ) {}
void CClientState::RunFrame() {}
void CClientState::ConsistencyCheck( bool bChanged ) {}
bool CClientState::HookClientStringTable( char const *tableName ) { return false; }

CClientState	cl;
float CL_GetHltvReplayTimeScale()
{
	return 1.0f; 
}

#endif
