//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Expose things from GameInterface.cpp. Mostly the engine interfaces.
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMEINTERFACE_H
#define GAMEINTERFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "mapentities.h"

#ifndef NO_STEAM
#include "steam/steam_gameserver.h"
#endif

extern INetworkStringTable *g_pStringTableInfoPanel;

// Player / Client related functions
// Most of this is implemented in gameinterface.cpp, but some of it is per-mod in files like cs_gameinterface.cpp, etc.
class CServerGameClients : public IServerGameClients
{
public:
	virtual bool			ClientConnect( edict_t *pEntity, char const* pszName, char const* pszAddress, char *reject, int maxrejectlen );
	virtual void			ClientActive( edict_t *pEntity, bool bLoadGame );
	virtual void			ClientFullyConnect( edict_t *pEntity );
	virtual void			ClientDisconnect( edict_t *pEntity );
	virtual void			ClientPutInServer( edict_t *pEntity, const char *playername );
	virtual void			ClientCommand( edict_t *pEntity, const CCommand &args );
	virtual void			ClientSettingsChanged( edict_t *pEntity );
	virtual void			ClientSetupVisibility( edict_t *pViewEntity, edict_t *pClient, unsigned char *pvs, int pvssize );
	virtual float			ProcessUsercmds( edict_t *player, bf_read *buf, int numcmds, int totalcmds,
								int dropped_packets, bool ignore, bool paused );
	// Player is running a command
	virtual void			PostClientMessagesSent( void );
	virtual void			SetCommandClient( int index );
	virtual CPlayerState	*GetPlayerState( edict_t *player );
	virtual void			ClientEarPosition( edict_t *pEntity, Vector *pEarOrigin );
	virtual bool			ClientReplayEvent( edict_t *pEdict, const ClientReplayEventParams_t &params ) OVERRIDE;

	virtual void			GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const;
	
	// returns number of delay ticks if player is in Replay mode (0 = no delay)
	virtual int				GetReplayDelay( edict_t *player, int& entity );
	// Anything this game .dll wants to add to the bug reporter text (e.g., the entity/model under the picker crosshair)
	//  can be added here
	virtual void			GetBugReportInfo( char *buf, int buflen );

	// A player sent a voice packet
	virtual void			ClientVoice( edict_t *pEdict );

	virtual void			NetworkIDValidated( const char *pszUserName, const char *pszNetworkID, CSteamID steamID ) OVERRIDE;
	virtual int				GetMaxSplitscreenPlayers();
	virtual int				GetMaxHumanPlayers();

	// The client has submitted a keyvalues command
	virtual void			ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues );

	// Server override for supplied client name
	virtual const char *	ClientNameHandler( uint64 xuid, const char *pchName ) OVERRIDE;

	virtual void		ClientSvcUserMessage( edict_t *pEntity, int nType, int nPassthrough, uint32 cbSize, const void *pvBuffer ) OVERRIDE;
};


class CServerGameDLL : public IServerGameDLL
{
public:
	virtual bool			DLLInit(CreateInterfaceFn engineFactory, CreateInterfaceFn physicsFactory, 
										CreateInterfaceFn fileSystemFactory, CGlobalVars *pGlobals);
	virtual void			DLLShutdown( void );
	// Get the simulation interval (must be compiled with identical values into both client and game .dll for MOD!!!)
	virtual float			GetTickInterval( void ) const;
	virtual bool			GameInit( void );
	virtual void			GameShutdown( void );
	virtual bool			LevelInit( const char *pMapName, char const *pMapEntities, char const *pOldLevel, char const *pLandmarkName, bool loadGame, bool background );
	virtual void			ServerActivate( edict_t *pEdictList, int edictCount, int clientMax );
	virtual void			LevelShutdown( void );
	virtual void			GameFrame( bool simulating ); // could be called multiple times before sending data to clients
	virtual void			PreClientUpdate( bool simulating ); // called after all GameFrame() calls, before sending data to clients

	virtual ServerClass*	GetAllServerClasses( void );
	virtual const char     *GetGameDescription( void );      
	virtual void			CreateNetworkStringTables( void );
	
	// Save/restore system hooks
	virtual CSaveRestoreData  *SaveInit( int size );
	virtual void			SaveWriteFields( CSaveRestoreData *, char const* , void *, datamap_t *, typedescription_t *, int );
	virtual void			SaveReadFields( CSaveRestoreData *, char const* , void *, datamap_t *, typedescription_t *, int );
	virtual void			SaveGlobalState( CSaveRestoreData * );
	virtual void			RestoreGlobalState( CSaveRestoreData * );
	virtual int				CreateEntityTransitionList( CSaveRestoreData *, int );
	virtual void			BuildAdjacentMapList( void );

	virtual void			PreSave( CSaveRestoreData * );
	virtual void			Save( CSaveRestoreData * );
	virtual void			GetSaveComment( char *comment, int maxlength, float flMinutes, float flSeconds, bool bNoTime = false );

	virtual void			WriteSaveHeaders( CSaveRestoreData * );

	virtual void			ReadRestoreHeaders( CSaveRestoreData * );
	virtual void			Restore( CSaveRestoreData *, bool );
	virtual bool			IsRestoring();
	virtual bool			SupportsSaveRestore();

	virtual CStandardSendProxies*	GetStandardSendProxies();

	virtual void			PostInit();
	virtual void			PostToolsInit();
	virtual void			Think( bool finalTick );

	virtual void			OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue );

	virtual void			PreSaveGameLoaded( char const *pSaveName, bool bInGame );

	// Returns true if the game DLL wants the server not to be made public.
	// Used by commentary system to hide multiplayer commentary servers from the master.
	virtual bool			ShouldHideServer( void );

	virtual void			InvalidateMdlCache();

	// Called to apply lobby settings to a dedicated server
	virtual void			ApplyGameSettings( KeyValues *pKV );

	virtual void			GetMatchmakingTags( char *buf, size_t bufSize );

	virtual void			ServerHibernationUpdate( bool bHibernating );

	virtual bool			ShouldPreferSteamAuth();

	virtual bool			ShouldAllowDirectConnect( void );
	virtual bool			FriendsReqdForDirectConnect( void );
	virtual bool			IsLoadTestServer( void );

	virtual bool			IsValveDS( void );

	// Builds extended server info for new connecting client
	virtual KeyValues*		GetExtendedServerInfoForNewClient();

	virtual void UpdateGCInformation();
	
	// Marks the queue matchmaking game as starting
	virtual void ReportGCQueuedMatchStart( int32 iReservationStage, uint32 *puiConfirmedAccounts, int numConfirmedAccounts );

	// Given a path to a map (relative to the game dir) returns a publish file id if its a UGC file known by the server, or 0 otherwise.
	virtual PublishedFileId_t GetUGCMapFileID( const char* szMapPath );

	// Query steam for the latest file info, download new version if needed.
	virtual bool GetNewestSubscribedFiles( void );

	// Same as above but for a single ugc map
	virtual void UpdateUGCMap( PublishedFileId_t id );

	// Returns true if we are currently downloading a new version of the map, or if there is a query pending to check for a newer version.
	virtual bool HasPendingMapDownloads( void ) const;
	
	// Precaches particle systems defined in the specific file
	virtual void PrecacheParticleSystemFile( const char *pParticleSystemFile );

	// Matchmaking game data buffer to set into SteamGameServer()->SetGameData
	virtual void			GetMatchmakingGameData( char *buf, size_t bufSize );

	float	m_fAutoSaveDangerousTime;
	float	m_fAutoSaveDangerousMinHealthToCommit;
	bool	m_bIsHibernating;

	// Called after the steam API has been activated post-level startup
	virtual void			GameServerSteamAPIActivated( bool bActive );

	// Returns which encryption key to use for messages to be encrypted for TV
	virtual EncryptedMessageKeyType_t GetMessageEncryptionKey( INetMessage *pMessage );

	// If server game dll needs more time before server process quits then
	// it should return true to hold game server reservation from this interface method.
	// If this method returns false then the server process will clear the reservation
	// and might shutdown to meet uptime or memory limit requirements.
	virtual bool ShouldHoldGameServerReservation( float flTimeElapsedWithoutClients );

	// Pure server validation failed for the given client, client supplied
	// data is included in the payload
	virtual void OnPureServerFileValidationFailure( edict_t *edictClient, const char *path, const char *fileName, uint32 crc, int32 hashType, int32 len, int packNumber, int packFileID );

	// Last chance validation on connect packet for the client, non-NULL return value
	// causes the client connect to be aborted with the provided error
	virtual char const * ClientConnectionValidatePreNetChan( bool bGameServer, char const *adr, int nAuthProtocol, uint64 ullSteamID );

	// validate if player is a caster and add them to the active caster list
	virtual bool ValidateAndAddActiveCaster( const CSteamID &steamID );

	// Network channel notification from engine to game server code
	virtual void OnEngineClientNetworkEvent( edict_t *edictClient, uint64 ullSteamID, int nEventType, void *pvParam ) OVERRIDE;
	
	virtual void EngineGotvSyncPacket( const CEngineGotvSyncPacket *pPkt ) OVERRIDE;

	// GOTV client attempt redirect over SDR
	virtual bool OnEngineClientProxiedRedirect( uint64 ullClient, const char *adrProxiedRedirect, const char *adrRegular ) OVERRIDE;

	// Tell server about a line we will write to the log file which may be sent to remote listeners
	bool LogForHTTPListeners( const char* szLogLine ) OVERRIDE;
private:

	// This can just be a wrapper on MapEntity_ParseAllEntities, but CS does some tricks in here
	// with the entity list.
	void LevelInit_ParseAllEntities( const char *pMapEntities );
	void LoadMessageOfTheDay();
};


// Normally, when the engine calls ClientPutInServer, it calls a global function in the game DLL
// by the same name. Use this to override the function that it calls. This is used for bots.
typedef CBasePlayer* (*ClientPutInServerOverrideFn)( edict_t *pEdict, const char *playername );

void ClientPutInServerOverride( ClientPutInServerOverrideFn fn );

// -------------------------------------------------------------------------------------------- //
// Entity list management stuff.
// -------------------------------------------------------------------------------------------- //
// These are created for map entities in order as the map entities are spawned.
class CMapEntityRef
{
public:
	int		m_iEdict;			// Which edict slot this entity got. -1 if CreateEntityByName failed.
	int		m_iSerialNumber;	// The edict serial number. TODO used anywhere ?
};

extern CUtlLinkedList<CMapEntityRef, unsigned short> g_MapEntityRefs;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CMapLoadEntityFilter : public IMapEntityFilter
{
public:
	virtual bool ShouldCreateEntity( const char *pClassname )
	{
		// During map load, create all the entities.
		return true;
	}

	virtual CBaseEntity* CreateNextEntity( const char *pClassname )
	{
		CBaseEntity *pRet = CreateEntityByName( pClassname );

		CMapEntityRef ref;
		ref.m_iEdict = -1;
		ref.m_iSerialNumber = -1;

		if ( pRet )
		{
			ref.m_iEdict = pRet->entindex();
			if ( pRet->edict() )
				ref.m_iSerialNumber = pRet->edict()->m_NetworkSerialNumber;
		}

		g_MapEntityRefs.AddToTail( ref );
		return pRet;
	}
};

bool IsEngineThreaded();

class CServerGameTags : public IServerGameTags
{
public:
	virtual void GetTaggedConVarList( KeyValues *pCvarTagList );

};
EXPOSE_SINGLE_INTERFACE( CServerGameTags, IServerGameTags, INTERFACEVERSION_SERVERGAMETAGS );

#ifndef NO_STEAM
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CSteam3Server : public CSteamGameServerAPIContext
{
public:
	CSteam3Server();

	void Shutdown( void )
	{
		Clear();
		m_bInitialized = false;
	}

	bool CheckInitialized( void )
	{
		if ( !m_bInitialized )
		{
			Init();
			m_bInitialized = true;
			return true;
		}

		return false;
	}

private:
	bool	m_bInitialized;
};
CSteam3Server &Steam3Server();
#endif

#endif // GAMEINTERFACE_H

