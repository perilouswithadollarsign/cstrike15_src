//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef EIFACE_H
#define EIFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "convar.h"
#include "icvar.h"
#include "edict.h"
#include "mathlib/vplane.h"
#include "iserverentity.h"
#include "engine/ivmodelinfo.h"
#include "soundflags.h"
#include "bitvec.h"
#include "engine/iserverplugin.h"
#include "tier1/bitbuf.h"
#include "iclient.h"
#include "google/protobuf/message.h"
#include "steam/isteamremotestorage.h"

//-----------------------------------------------------------------------------
// forward declarations
//-----------------------------------------------------------------------------
class	SendTable;
class	ServerClass;
class	IMoveHelper;
struct  Ray_t;
class	CGameTrace;
typedef	CGameTrace trace_t;
struct	typedescription_t;
class	CSaveRestoreData;
struct	datamap_t;
class	SendTable;
class	ServerClass;
class	IMoveHelper;
struct  Ray_t;
struct	studiohdr_t;
class	CBaseEntity;
class	CRestore;
class	CSave;
class	variant_t;
struct	vcollide_t;
class	IRecipientFilter;
class	CBaseEntity;
class	ITraceFilter;
struct	client_textmessage_t;
class	INetChannelInfo;
class	ISpatialPartition;
class IScratchPad3D;
class CStandardSendProxies;
class IAchievementMgr;
class CGamestatsData;
class CSteamID;
class ISPSharedMemory;
class CGamestatsData;
class CEngineGotvSyncPacket;	// forward declare protobuf message here

typedef struct player_info_s player_info_t;

//-----------------------------------------------------------------------------
// defines
//-----------------------------------------------------------------------------

#ifdef _WIN32
#define DLLEXPORT __stdcall
#else
#define DLLEXPORT /* */
#endif

#define INTERFACEVERSION_VENGINESERVER	"VEngineServer023"

struct bbox_t
{
	Vector mins;
	Vector maxs;
};

enum EncryptedMessageKeyType_t
{
	// Warning: do not renumber the key types as they get baked in demos
	kEncryptedMessageKeyType_None = 0,
	kEncryptedMessageKeyType_Private = 1,
	kEncryptedMessageKeyType_Public = 2,
};

struct CEngineHltvInfo_t
{
	bool m_bBroadcastActive;		// Whether TV broadcast is active
	bool m_bMasterProxy;			// Whether this is master proxy
	float m_flDelay;				// TV delay
	float m_flTime;					// TV match time
	int m_nTvPort;					// TV port
	int m_numSlots;					// Total number of TV slots
	int m_numClients;				// Clients (spectators+proxies) count
	int m_numProxies;				// Number of proxies
	int m_numLocalSlots;			// Local number of TV slots
	int m_numLocalClients;			// Local clients (spectators+proxies) count
	int m_numLocalProxies;			// Local number of proxies
	int m_numRelaySlots;			// Relay number of TV slots
	int m_numRelayClients;			// Relay clients (spectators+proxies) count
	int m_numRelayProxies;			// Relay number of proxies
	int m_numExternalTotalViewers;	// External total viewers
	int m_numExternalLinkedViewers;	// External linked viewers
	uint32 m_relayAddress;			// Relay address
	uint32 m_relayPort;				// Relay port
};

struct ClientReplayEventParams_t
{
	ClientReplayEventParams_t( int nEventType )
	{
		m_nEventType = nEventType;
		m_flSlowdownLength = 0;
		m_flSlowdownRate = 1.0f;
		m_nPrimaryTargetEntIndex = -1; // unknown by default
		m_flEventTime = -1.0; // unknown by default
	}
	int m_nEventType;

	float m_flSlowdownLength;
	float m_flSlowdownRate;

	int m_nPrimaryTargetEntIndex;
	float m_flEventTime;
};

//-----------------------------------------------------------------------------
// Purpose: Interface the engine exposes to the game DLL
//-----------------------------------------------------------------------------
abstract_class IVEngineServer
{
public:
	// Tell engine to change level ( "changelevel s1\n" or "changelevel2 s1 s2\n" )
	virtual void		ChangeLevel( const char *s1, const char *s2 ) = 0;
	
	// Ask engine whether the specified map is a valid map file (exists and has valid version number).
	virtual int			IsMapValid( const char *filename ) = 0;
	
	// Is this a dedicated server?
	virtual bool		IsDedicatedServer( void ) = 0;
	
	// Is in Hammer editing mode?
	virtual int			IsInEditMode( void ) = 0;

	// get arbitrary launch options
	virtual KeyValues* GetLaunchOptions( void ) = 0;
	
	// Add to the server/client lookup/precache table, the specified string is given a unique index
	// NOTE: The indices for PrecacheModel are 1 based
	//  a 0 returned from those methods indicates the model or sound was not correctly precached
	// However, generic and decal are 0 based
	// If preload is specified, the file is loaded into the server/client's cache memory before level startup, otherwise
	//  it'll only load when actually used (which can cause a disk i/o hitch if it occurs during play of a level).
	virtual int			PrecacheModel( const char *s, bool preload = false ) = 0;
	virtual int			PrecacheSentenceFile( const char *s, bool preload = false ) = 0;
	virtual int			PrecacheDecal( const char *name, bool preload = false ) = 0;
	virtual int			PrecacheGeneric( const char *s, bool preload = false ) = 0;

	// Check's if the name is precached, but doesn't actually precache the name if not...
	virtual bool		IsModelPrecached( char const *s ) const = 0;
	virtual bool		IsDecalPrecached( char const *s ) const = 0;
	virtual bool		IsGenericPrecached( char const *s ) const = 0;

	// Note that sounds are precached using the IEngineSound interface

	// Special purpose PVS checking
	// Get the cluster # for the specified position
	virtual int			GetClusterForOrigin( const Vector &org ) = 0;
	// Get the PVS bits for a specified cluster and copy the bits into outputpvs.  Returns the number of bytes needed to pack the PVS
	virtual int			GetPVSForCluster( int cluster, int outputpvslength, unsigned char *outputpvs ) = 0;
	// Check whether the specified origin is inside the specified PVS
	virtual bool		CheckOriginInPVS( const Vector &org, const unsigned char *checkpvs, int checkpvssize ) = 0;
	// Check whether the specified worldspace bounding box is inside the specified PVS
	virtual bool		CheckBoxInPVS( const Vector &mins, const Vector &maxs, const unsigned char *checkpvs, int checkpvssize ) = 0;

	// Returns the server assigned userid for this player.  Useful for logging frags, etc.  
	//  returns -1 if the edict couldn't be found in the list of players.
	virtual int			GetPlayerUserId( const edict_t *e ) = 0; 
	virtual const char	*GetPlayerNetworkIDString( const edict_t *e ) = 0;
	virtual bool		IsUserIDInUse( int userID ) = 0;	// TERROR: used for transitioning
	virtual int			GetLoadingProgressForUserID( int userID ) = 0;	// TERROR: used for transitioning

	// Return the current number of used edict slots
	virtual int			GetEntityCount( void ) = 0;
	
	// Get stats info interface for a client netchannel
	virtual INetChannelInfo* GetPlayerNetInfo( int playerIndex ) = 0;
	
	// Allocate space for string and return index/offset of string in global string list
	// If iForceEdictIndex is not -1, then it will return the edict with that index. If that edict index
	// is already used, it'll return null.
	virtual edict_t		*CreateEdict( int iForceEdictIndex = -1 ) = 0;
	// Remove the specified edict and place back into the free edict list
	virtual void		RemoveEdict( edict_t *e ) = 0;
	
	// Memory allocation for entity class data
	virtual void		*PvAllocEntPrivateData( long cb ) = 0;
	virtual void		FreeEntPrivateData( void *pEntity ) = 0;

	// Save/restore uses a special memory allocator (which zeroes newly allocated memory, etc.)
	virtual void		*SaveAllocMemory( size_t num, size_t size ) = 0;
	virtual void		SaveFreeMemory( void *pSaveMem ) = 0;
	
	// Emit an ambient sound associated with the specified entity
	virtual void		EmitAmbientSound( int entindex, const Vector &pos, const char *samp, float vol, soundlevel_t soundlevel, int fFlags, int pitch, float delay = 0.0f ) = 0;

	// Fade out the client's volume level toward silence (or fadePercent)
	virtual void        FadeClientVolume( const edict_t *pEdict, float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds ) = 0;
	
	// Sentences / sentence groups
	virtual int			SentenceGroupPick( int groupIndex, char *name, int nameBufLen ) = 0;
	virtual int			SentenceGroupPickSequential( int groupIndex, char *name, int nameBufLen, int sentenceIndex, int reset ) = 0;
	virtual int			SentenceIndexFromName( const char *pSentenceName ) = 0;
	virtual const char *SentenceNameFromIndex( int sentenceIndex ) = 0;
	virtual int			SentenceGroupIndexFromName( const char *pGroupName ) = 0;
	virtual const char *SentenceGroupNameFromIndex( int groupIndex ) = 0;
	virtual float		SentenceLength( int sentenceIndex ) = 0;

	// Issue a command to the command parser as if it was typed at the server console.	
	virtual void		ServerCommand( const char *str ) = 0;
	// Execute any commands currently in the command parser immediately (instead of once per frame)
	virtual void		ServerExecute( void ) = 0;
	// Issue the specified command to the specified client (mimics that client typing the command at the console).
	virtual void		ClientCommand( edict_t *pEdict, PRINTF_FORMAT_STRING const char *szFmt, ... ) FMTFUNCTION( 3, 4 ) = 0;

	// Set the lightstyle to the specified value and network the change to any connected clients.  Note that val must not 
	//  change place in memory (use MAKE_STRING) for anything that's not compiled into your mod.
	virtual void		LightStyle( int style, PRINTF_FORMAT_STRING const char *val ) = 0;

	// Project a static decal onto the specified entity / model (for level placed decals in the .bsp)
	virtual void		StaticDecal( const Vector &originInEntitySpace, int decalIndex, int entityIndex, int modelIndex, bool lowpriority ) = 0;
	
	// Given the current PVS(or PAS) and origin, determine which players should hear/receive the message
	virtual void		Message_DetermineMulticastRecipients( bool usepas, const Vector& origin, CPlayerBitVec& playerbits ) = 0;

	// Begin a message from a server side entity to its client side counterpart (func_breakable glass, e.g.)
	virtual bf_write	*EntityMessageBegin( int ent_index, ServerClass * ent_class, bool reliable ) = 0;
	// Finish the Entity or UserMessage and dispatch to network layer
	virtual void		MessageEnd( void ) = 0;

	// Send a protobuf based user message
	virtual void		SendUserMessage( IRecipientFilter& filter, int message, const ::google::protobuf::Message &msg ) = 0;

	// Print szMsg to the client console.
	virtual void		ClientPrintf( edict_t *pEdict, const char *szMsg ) = 0;

	// SINGLE PLAYER/LISTEN SERVER ONLY (just matching the client .dll api for this)
	// Prints the formatted string to the notification area of the screen ( down the right hand edge
	//  numbered lines starting at position 0
	virtual void		Con_NPrintf( int pos, const char *fmt, ... ) = 0;
	// SINGLE PLAYER/LISTEN SERVER ONLY(just matching the client .dll api for this)
	// Similar to Con_NPrintf, but allows specifying custom text color and duration information
	virtual void		Con_NXPrintf( const struct con_nprint_s *info, const char *fmt, ... ) = 0;

	// Change a specified player's "view entity" (i.e., use the view entity position/orientation for rendering the client view)
	virtual void		SetView( const edict_t *pClient, const edict_t *pViewent ) = 0;

	// Set the player's crosshair angle
	virtual void		CrosshairAngle( const edict_t *pClient, float pitch, float yaw ) = 0;

	// Get the current game directory (hl2, tf2, hl1, cstrike, etc.)
	virtual void        GetGameDir( char *szGetGameDir, int maxlength ) = 0;

	// Used by AI node graph code to determine if .bsp and .ain files are out of date
	virtual int 		CompareFileTime( const char *filename1, const char *filename2, int *iCompare ) = 0;

	// Locks/unlocks the network string tables (.e.g, when adding bots to server, this needs to happen).
	// Be sure to reset the lock after executing your code!!!
	virtual bool		LockNetworkStringTables( bool lock ) = 0;

	// Create a bot with the given name.  Returns NULL if fake client can't be created
	virtual edict_t		*CreateFakeClient( const char *netname ) = 0;	

	// Get a convar keyvalue for s specified client
	virtual const char	*GetClientConVarValue( int clientIndex, const char *name ) = 0;
	
	// Parse a token from a file
	virtual const char	*ParseFile( const char *data, char *token, int maxlen ) = 0;
	// Copies a file
	virtual bool		CopyFile( const char *source, const char *destination ) = 0;

	// Reset the pvs, pvssize is the size in bytes of the buffer pointed to by pvs.
	// This should be called right before any calls to AddOriginToPVS
	virtual void		ResetPVS( byte *pvs, int pvssize ) = 0;
	// Merge the pvs bits into the current accumulated pvs based on the specified origin ( not that each pvs origin has an 8 world unit fudge factor )
	virtual void		AddOriginToPVS( const Vector &origin ) = 0;
	
	// Mark a specified area portal as open/closed.
	// Use SetAreaPortalStates if you want to set a bunch of them at a time.
	virtual void		SetAreaPortalState( int portalNumber, int isOpen ) = 0;
	
	// Queue a temp entity for transmission
	virtual void		PlaybackTempEntity( IRecipientFilter& filter, float delay, const void *pSender, const SendTable *pST, int classID  ) = 0;
	// Given a node number and the specified PVS, return with the node is in the PVS
	virtual int			CheckHeadnodeVisible( int nodenum, const byte *pvs, int vissize ) = 0;
	// Using area bits, cheeck whether area1 flows into area2 and vice versa (depends on area portal state)
	virtual int			CheckAreasConnected( int area1, int area2 ) = 0;
	// Given an origin, determine which area index the origin is within
	virtual int			GetArea( const Vector &origin ) = 0;
	// Get area portal bit set
	virtual void		GetAreaBits( int area, unsigned char *bits, int buflen ) = 0;
	// Given a view origin (which tells us the area to start looking in) and a portal key,
	// fill in the plane that leads out of this area (it points into whatever area it leads to).
	virtual bool		GetAreaPortalPlane( Vector const &vViewOrigin, int portalKey, VPlane *pPlane ) = 0;

	// Save/restore wrapper - FIXME:  At some point we should move this to it's own interface
	virtual bool		LoadGameState( char const *pMapName, bool createPlayers ) = 0;
	virtual void		LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName ) = 0;
	virtual void		ClearSaveDir() = 0;

	// Get the pristine map entity lump string.  (e.g., used by CS to reload the map entities when restarting a round.)
	virtual const char*	GetMapEntitiesString() = 0;

	// Text message system -- lookup the text message of the specified name
	virtual client_textmessage_t *TextMessageGet( const char *pName ) = 0;

	// Print a message to the server log file
	virtual void		LogPrint( const char *msg ) = 0;
	virtual bool		IsLogEnabled() = 0;
	// Builds PVS information for an entity
	virtual void		BuildEntityClusterList( edict_t *pEdict, PVSInfo_t *pPVSInfo ) = 0;

	// A solid entity moved, update spatial partition
	virtual void SolidMoved( edict_t *pSolidEnt, ICollideable *pSolidCollide, const Vector* pPrevAbsOrigin, bool testSurroundingBoundsOnly ) = 0;
	// A trigger entity moved, update spatial partition
	virtual void TriggerMoved( edict_t *pTriggerEnt, bool testSurroundingBoundsOnly ) = 0;
	
	// Create/destroy a custom spatial partition
	virtual ISpatialPartition *CreateSpatialPartition( const Vector& worldmin, const Vector& worldmax ) = 0;
	virtual void 		DestroySpatialPartition( ISpatialPartition * ) = 0;

	// Draw the brush geometry in the map into the scratch pad.
	// Flags is currently unused.
	virtual void		DrawMapToScratchPad( IScratchPad3D *pPad, unsigned long iFlags ) = 0;

	// This returns which entities, to the best of the server's knowledge, the client currently knows about.
	// This is really which entities were in the snapshot that this client last acked.
	// This returns a bit vector with one bit for each entity.
	//
	// USE WITH CARE. Whatever tick the client is really currently on is subject to timing and
	// ordering differences, so you should account for about a quarter-second discrepancy in here.
	// Also, this will return NULL if the client doesn't exist or if this client hasn't acked any frames yet.
	// 
	// iClientIndex is the CLIENT index, so if you use pPlayer->entindex(), subtract 1.
	virtual const CBitVec<MAX_EDICTS>* GetEntityTransmitBitsForClient( int iClientIndex ) = 0;
	
	// Is the game paused?
	virtual bool		IsPaused() = 0;

	// What is the game timescale multiplied with the host_timescale?
	virtual float GetTimescale( void ) const = 0;
	
	// Marks the filename for consistency checking.  This should be called after precaching the file.
	virtual void		ForceExactFile( const char *s ) = 0;
	virtual void		ForceModelBounds( const char *s, const Vector &mins, const Vector &maxs ) = 0;
	virtual void		ClearSaveDirAfterClientLoad() = 0;

	// Sets a USERINFO client ConVar for a fakeclient
	virtual void		SetFakeClientConVarValue( edict_t *pEntity, const char *cvar, const char *value ) = 0;
	
	// Marks the material (vmt file) for consistency checking.  If the client and server have different
	// contents for the file, the client's vmt can only use the VertexLitGeneric shader, and can only
	// contain $baseTexture and $bumpmap vars.
	virtual void		ForceSimpleMaterial( const char *s ) = 0;

	// Is the engine in Commentary mode?
	virtual int			IsInCommentaryMode( void ) = 0;

	// Is the engine running a background map?
	virtual bool		IsLevelMainMenuBackground( void ) = 0;

	// Mark some area portals as open/closed. It's more efficient to use this
	// than a bunch of individual SetAreaPortalState calls.
	virtual void		SetAreaPortalStates( const int *portalNumbers, const int *isOpen, int nPortals ) = 0;

	// Called when relevant edict state flags change.
	virtual void		NotifyEdictFlagsChange( int iEdict ) = 0;
	
	// Only valid during CheckTransmit. Also, only the PVS, networked areas, and
	// m_pTransmitInfo are valid in the returned strucutre.
	virtual const CCheckTransmitInfo* GetPrevCheckTransmitInfo( edict_t *pPlayerEdict ) = 0;
	
	virtual CSharedEdictChangeInfo* GetSharedEdictChangeInfo() = 0;

	// Tells the engine we can immdiately re-use all edict indices
	// even though we may not have waited enough time
	virtual void			AllowImmediateEdictReuse( ) = 0;

	// Returns true if the engine is an internal build. i.e. is using the internal bugreporter.
	virtual bool		IsInternalBuild( void ) = 0;

	virtual IChangeInfoAccessor *GetChangeAccessor( const edict_t *pEdict ) = 0;	

	// Name of most recently load .sav file
	virtual char const *GetMostRecentlyLoadedFileName() = 0;
	virtual char const *GetSaveFileName() = 0;

	// Cleans up the cluster list
	virtual void CleanUpEntityClusterList( PVSInfo_t *pPVSInfo ) = 0;

	virtual void SetAchievementMgr( IAchievementMgr *pAchievementMgr ) =0;
	virtual IAchievementMgr *GetAchievementMgr() = 0;

	virtual int	GetAppID() = 0;
	
	virtual bool IsLowViolence() = 0;

	virtual bool IsAnyClientLowViolence() = 0;

	// Call this to find out the value of a cvar on the client.
	//
	// It is an asynchronous query, and it will call IServerGameDLL::OnQueryCvarValueFinished when
	// the value comes in from the client.
	//
	// Store the return value if you want to match this specific query to the OnQueryCvarValueFinished call.
	// Returns InvalidQueryCvarCookie if the entity is invalid.
	virtual QueryCvarCookie_t StartQueryCvarValue( edict_t *pPlayerEntity, const char *pName ) = 0;

	virtual void InsertServerCommand( const char *str ) = 0;

	// Fill in the player info structure for the specified player index (name, model, etc.)
	virtual bool GetPlayerInfo( int ent_num, player_info_t *pinfo ) = 0;

	// Returns true if this client has been fully authenticated by Steam
	virtual bool IsClientFullyAuthenticated( edict_t *pEdict ) = 0;

	// This makes the host run 1 tick per frame instead of checking the system timer to see how many ticks to run in a certain frame.
	// i.e. it does the same thing timedemo does.
	virtual void SetDedicatedServerBenchmarkMode( bool bBenchmarkMode ) = 0;

	virtual bool IsSplitScreenPlayer( int ent_num ) = 0;
	virtual edict_t *GetSplitScreenPlayerAttachToEdict( int ent_num ) = 0;
	virtual int	GetNumSplitScreenUsersAttachedToEdict( int ent_num ) = 0;
	virtual edict_t *GetSplitScreenPlayerForEdict( int ent_num, int nSlot ) = 0;

	// Used by Foundry to hook into the loadgame process and override the entities that are getting loaded.
	virtual bool IsOverrideLoadGameEntsOn() = 0;

	// Used by Foundry when it changes an entity (and possibly its class) but preserves its serial number.
	virtual void ForceFlushEntity( int iEntity ) = 0;

	//Finds or Creates a shared memory space, the returned pointer will automatically be AddRef()ed
	virtual ISPSharedMemory *GetSinglePlayerSharedMemorySpace( const char *szName, int ent_num = MAX_EDICTS ) = 0;

	// Allocate hunk memory
	virtual void *AllocLevelStaticData( size_t bytes ) = 0;

	// Gets a list of all clusters' bounds.  Returns total number of clusters.
	virtual int GetClusterCount() = 0;
	virtual int GetAllClusterBounds( bbox_t *pBBoxList, int maxBBox ) = 0;

	virtual bool IsCreatingReslist() = 0;
	virtual bool IsCreatingXboxReslist() = 0;
	virtual bool IsDedicatedServerForXbox() = 0;
	virtual bool IsDedicatedServerForPS3( void ) = 0;

	virtual void Pause( bool bPause, bool bForce = false ) = 0;

	virtual void SetTimescale( float flTimescale ) = 0;

	// Methods to set/get a gamestats data container so client & server running in same process can send combined data
	virtual void SetGamestatsData( CGamestatsData *pGamestatsData ) = 0;
	virtual CGamestatsData *GetGamestatsData() = 0;

	// Returns the SteamID of the specified player. It'll be NULL if the player hasn't authenticated yet.
	virtual const CSteamID	*GetClientSteamID( const edict_t *pPlayerEdict, bool bRequireFullyAuthenticated = false ) = 0;

	// Returns the SteamID of the game server
	virtual const CSteamID	*GetGameServerSteamID() = 0;
	
	// Validate session
	virtual void HostValidateSession() = 0;

	// Update the 360 pacifier/spinner
	virtual void RefreshScreenIfNecessary() = 0;

	// Tells the engine to allocate paint surfaces
	virtual bool HasPaintmap() = 0;

	// Returns true if the surface paint colors changed
	virtual bool SpherePaintSurface( const model_t *pModel, const Vector& vPosition, BYTE color, float flSphereRadius, float flPaintCoatPercent ) = 0;

	virtual void SphereTracePaintSurface( const model_t *pModel, const Vector& vPosition, const Vector& vContactNormal, float flSphereRadius, CUtlVector<BYTE>& surfColor ) = 0;
	virtual void RemoveAllPaint() = 0;
	virtual void PaintAllSurfaces( BYTE color ) = 0;
	virtual void RemovePaint( const model_t* pModel ) = 0;

	// Send a client command keyvalues
	// keyvalues are deleted inside the function
	virtual void ClientCommandKeyValues( edict_t *pEdict, KeyValues *pCommand ) = 0;

	// Returns the XUID of the specified player. It'll be NULL if the player hasn't connected yet.
	virtual uint64 GetClientXUID( edict_t *pPlayerEdict ) = 0;
	virtual bool IsActiveApp() = 0;

	virtual void SetNoClipEnabled( bool bEnabled ) = 0;

	virtual void GetPaintmapDataRLE( CUtlVector<uint32> &data ) = 0;
	virtual void LoadPaintmapDataRLE( const CUtlVector< uint32 > &data ) = 0;
	virtual void SendPaintmapDataToClient( edict_t *pPlayerEdict ) = 0;

	// Gets the accumulated latency for the sounds related to choreos.
	virtual float GetLatencyForChoreoSounds() = 0;

	virtual CrossPlayPlatform_t GetClientCrossPlayPlatform( int ent_num ) = 0;

	// Create the instance baseline for a serverclass if it doesn't exist in the engine's list already
	virtual void EnsureInstanceBaseline( int ent_num ) = 0;

	// Sets server reservation payload
	virtual bool ReserveServerForQueuedGame( char const *szReservationPayload ) = 0;

	// Get the TV information
	virtual bool GetEngineHltvInfo( CEngineHltvInfo_t &info ) = 0;

	// Add HLTV proxy whitelist to bypass password and Steam Auth checks upon connection, as CIDR a.b.c.d/numbits
	virtual void AddHltvRelayProxyWhitelist( uint32 a, uint32 b, uint32 c, uint32 d, uint32 numbits ) = 0;

	// Server version from the steam.inf, this will be compared to the GC version
	virtual int GetServerVersion() const = 0;

	// On master HLTV this call updates number of external viewers and which portion of those are linked with Steam
	virtual void UpdateHltvExternalViewers( uint32 numTotalViewers, uint32 numLinkedViewers ) = 0;

	// Check whether sv_shutdown was requested
	virtual bool WasShutDownRequested() const = 0;

	virtual bool StartClientHltvReplay( int nClientIndex, const HltvReplayParams_t &params ) = 0;
	virtual void StopClientHltvReplay( int nClientIndex ) = 0;
	virtual int GetClientHltvReplayDelay( int nClientIndex ) = 0;
	virtual bool HasHltvReplay() = 0;
	virtual bool ClientCanStartHltvReplay( int nClientIndex ) = 0;
	virtual void ClientResetReplayRequestTime( int nClientIndex ) = 0;
	virtual bool AnyClientsInHltvReplayMode() = 0;
	virtual int GetLocalClientIndex( void ) = 0;
};

#define INTERFACEVERSION_SERVERGAMEDLL				"ServerGameDLL005"

//-----------------------------------------------------------------------------
// Purpose: These are the interfaces that the game .dll exposes to the engine
//-----------------------------------------------------------------------------
abstract_class IServerGameDLL
{
public:
	// Initialize the game (one-time call when the DLL is first loaded )
	// Return false if there is an error during startup.
	virtual bool			DLLInit(	CreateInterfaceFn engineFactory, 
										CreateInterfaceFn physicsFactory, 
										CreateInterfaceFn fileSystemFactory, 
										CGlobalVars *pGlobals) = 0;

	// This is called when a new game is started. (restart, map)
	virtual bool			GameInit( void ) = 0;

	// Called any time a new level is started (after GameInit() also on level transitions within a game)
	virtual bool			LevelInit( char const *pMapName, 
									char const *pMapEntities, char const *pOldLevel, 
									char const *pLandmarkName, bool loadGame, bool background ) = 0;

	// The server is about to activate
	virtual void			ServerActivate( edict_t *pEdictList, int edictCount, int clientMax ) = 0;

	// The server should run physics/think on all edicts
	virtual void			GameFrame( bool simulating ) = 0;

	// Called once per simulation frame on the final tick
	virtual void			PreClientUpdate( bool simulating ) = 0;

	// Called when a level is shutdown (including changing levels)
	virtual void			LevelShutdown( void ) = 0;
	// This is called when a game ends (server disconnect, death, restart, load)
	// NOT on level transitions within a game
	virtual void			GameShutdown( void ) = 0;

	// Called once during DLL shutdown
	virtual void			DLLShutdown( void ) = 0;

	// Get the simulation interval (must be compiled with identical values into both client and game .dll for MOD!!!)
	// Right now this is only requested at server startup time so it can't be changed on the fly, etc.
	virtual float			GetTickInterval( void ) const = 0;

	// Give the list of datatable classes to the engine.  The engine matches class names from here with
	//  edict_t::classname to figure out how to encode a class's data for networking
	virtual ServerClass*	GetAllServerClasses( void ) = 0;

	// Returns string describing current .dll.  e.g., TeamFortress 2, Half-Life 2.  
	//  Hey, it's more descriptive than just the name of the game directory
	virtual const char     *GetGameDescription( void ) = 0;      
	
	// Let the game .dll allocate it's own network/shared string tables
	virtual void			CreateNetworkStringTables( void ) = 0;
	
	// Save/restore system hooks
	virtual CSaveRestoreData  *SaveInit( int size ) = 0;
	virtual void			SaveWriteFields( CSaveRestoreData *, const char *, void *, datamap_t *, typedescription_t *, int ) = 0;
	virtual void			SaveReadFields( CSaveRestoreData *, const char *, void *, datamap_t *, typedescription_t *, int ) = 0;
	virtual void			SaveGlobalState( CSaveRestoreData * ) = 0;
	virtual void			RestoreGlobalState( CSaveRestoreData * ) = 0;
	virtual void			PreSave( CSaveRestoreData * ) = 0;
	virtual void			Save( CSaveRestoreData * ) = 0;
	virtual void			GetSaveComment( char *comment, int maxlength, float flMinutes, float flSeconds, bool bNoTime = false ) = 0;
	virtual void			WriteSaveHeaders( CSaveRestoreData * ) = 0;
	virtual void			ReadRestoreHeaders( CSaveRestoreData * ) = 0;
	virtual void			Restore( CSaveRestoreData *, bool ) = 0;
	virtual bool			IsRestoring() = 0;
	virtual bool			SupportsSaveRestore() = 0;

	// Returns the number of entities moved across the transition
	virtual int				CreateEntityTransitionList( CSaveRestoreData *, int ) = 0;
	// Build the list of maps adjacent to the current map
	virtual void			BuildAdjacentMapList( void ) = 0;

	// Hand over the StandardSendProxies in the game DLL's module.
	virtual CStandardSendProxies*	GetStandardSendProxies() = 0;

	// Called once during startup, after the game .dll has been loaded and after the client .dll has also been loaded
	virtual void			PostInit() = 0;
	// Called once per frame even when no level is loaded...
	virtual void			Think( bool finalTick ) = 0;

	virtual void			PreSaveGameLoaded( char const *pSaveName, bool bCurrentlyInGame ) = 0;

	// Returns true if the game DLL wants the server not to be made public.
	// Used by commentary system to hide multiplayer commentary servers from the master.
	virtual bool			ShouldHideServer( void ) = 0;

	virtual void			InvalidateMdlCache() = 0;

	// * This function is new with version 6 of the interface.
	//
	// This is called when a query from IServerPluginHelpers::StartQueryCvarValue is finished.
	// iCookie is the value returned by IServerPluginHelpers::StartQueryCvarValue.
	// Added with version 2 of the interface.
	virtual void			OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue ) = 0;

	// Called after tools are initialized (i.e. when Foundry is initialized so we can get IServerFoundry).
	virtual void			PostToolsInit() = 0;

	// When bActive = true the function is called after the steam API has been activated post-level startup
	// when bActive = false the function is called before the game server session will LogOff and all interfaces will shutdown
	virtual void			GameServerSteamAPIActivated( bool bActive ) = 0;
	
	// Called to apply lobby settings to a dedicated server
	virtual void			ApplyGameSettings( KeyValues *pKV ) = 0;

	// 
	virtual void			GetMatchmakingTags( char *buf, size_t bufSize ) = 0;

	virtual void			ServerHibernationUpdate( bool bHibernating ) = 0;

	virtual bool			ShouldPreferSteamAuth() = 0;

	// Added for CS:GO - 
	// In Competetive mode we do not want to allow direct-connect to Valve servers
	virtual bool			ShouldAllowDirectConnect() = 0;
	virtual bool			FriendsReqdForDirectConnect() = 0;
	virtual bool			IsLoadTestServer() = 0;

	// Is this an Official dedicated server for ranked matchmaking?
	virtual bool			IsValveDS() = 0;

	// Builds extended server info for new connecting client
	virtual KeyValues*		GetExtendedServerInfoForNewClient() = 0;

	// Updates GC information for this server
	virtual void UpdateGCInformation() = 0;

	// Marks the queue matchmaking game as starting
	virtual void ReportGCQueuedMatchStart( int32 iReservationStage, uint32 *puiConfirmedAccounts, int numConfirmedAccounts ) = 0;

	// Get the published file id for the community map this server is running. 0 if non-ugc map or no map is running.
	virtual PublishedFileId_t GetUGCMapFileID( const char* szMapPath ) = 0;
	
	// Matchmaking game data buffer to set into SteamGameServer()->SetGameData
	virtual void			GetMatchmakingGameData( char *buf, size_t bufSize ) = 0;

	// Returns true if server is in the process of updating the given map
	virtual bool HasPendingMapDownloads( void ) const = 0;

	virtual void UpdateUGCMap( PublishedFileId_t id ) = 0;

	// Returns which encryption key to use for messages to be encrypted for TV
	virtual EncryptedMessageKeyType_t GetMessageEncryptionKey( INetMessage *pMessage ) = 0;

	// If server game dll needs more time before server process quits then
	// it should return true to hold game server reservation from this interface method.
	// If this method returns false then the server process will clear the reservation
	// and might shutdown to meet uptime or memory limit requirements.
	virtual bool ShouldHoldGameServerReservation( float flTimeElapsedWithoutClients ) = 0;

	// Pure server validation failed for the given client, client supplied
	// data is included in the payload
	virtual void OnPureServerFileValidationFailure( edict_t *edictClient, const char *path, const char *fileName, uint32 crc, int32 hashType, int32 len, int packNumber, int packFileID ) = 0;

	// Precaches particle systems defined in the specific file
	virtual void PrecacheParticleSystemFile( const char *pParticleSystemFile ) = 0;

	// Last chance validation on connect packet for the client, non-NULL return value
	// causes the client connect to be aborted with the provided error
	virtual char const * ClientConnectionValidatePreNetChan( bool bGameServer, char const *adr, int nAuthProtocol, uint64 ullSteamID ) = 0;

	// Network channel notification from engine to game server code
	virtual void OnEngineClientNetworkEvent( edict_t *edictClient, uint64 ullSteamID, int nEventType, void *pvParam ) = 0;

	// Engine notifying GC with a message
	virtual void EngineGotvSyncPacket( const CEngineGotvSyncPacket *pPkt ) = 0;

	// GOTV client attempt redirect over SDR
	virtual bool OnEngineClientProxiedRedirect( uint64 ullClient, const char *adrProxiedRedirect, const char *adrRegular ) = 0;

	// Tell server about a line we will write to the log file which may be sent to remote listeners
	virtual bool LogForHTTPListeners( const char* szLogLine ) = 0;
};

//-----------------------------------------------------------------------------
// Just an interface version name for the random number interface
// See vstdlib/random.h for the interface definition
// NOTE: If you change this, also change VENGINE_CLIENT_RANDOM_INTERFACE_VERSION in cdll_int.h
//-----------------------------------------------------------------------------
#define VENGINE_SERVER_RANDOM_INTERFACE_VERSION	"VEngineRandom001"

#define INTERFACEVERSION_SERVERGAMEENTS			"ServerGameEnts001"
//-----------------------------------------------------------------------------
// Purpose: Interface to get at server entities
//-----------------------------------------------------------------------------
abstract_class IServerGameEnts
{
public:
	virtual					~IServerGameEnts()	{}

	// The engine wants to mark two entities as touching
	virtual void			MarkEntitiesAsTouching( edict_t *e1, edict_t *e2 ) = 0;

	// Frees the entity attached to this edict
	virtual void			FreeContainingEntity( edict_t * ) = 0; 

	// This allows the engine to get at edicts in a CGameTrace.
	virtual edict_t*		BaseEntityToEdict( CBaseEntity *pEnt ) = 0;
	virtual CBaseEntity*	EdictToBaseEntity( edict_t *pEdict ) = 0;

	// This sets a bit in pInfo for each edict in the list that wants to be transmitted to the 
	// client specified in pInfo.
	//
	// This is also where an entity can force other entities to be transmitted if it refers to them
	// with ehandles.
	virtual void			CheckTransmit( CCheckTransmitInfo *pInfo, const unsigned short *pEdictIndices, int nEdicts ) = 0;

	// TERROR: Perform any PVS cleanup before a full update
	virtual void			PrepareForFullUpdate( edict_t *pEdict ) = 0;
};

#define INTERFACEVERSION_SERVERGAMECLIENTS		"ServerGameClients004"

//-----------------------------------------------------------------------------
// Purpose: Player / Client related functions
//-----------------------------------------------------------------------------
abstract_class IServerGameClients
{
public:
	// Get server maxplayers and lower bound for same
	virtual void			GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const = 0;

	// Client is connecting to server ( return false to reject the connection )
	//	You can specify a rejection message by writing it into reject
	virtual bool			ClientConnect( edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen ) = 0;

	// Client is going active
	// If bLoadGame is true, don't spawn the player because its state is already setup.
	virtual void			ClientActive( edict_t *pEntity, bool bLoadGame ) = 0;
	
	virtual void			ClientFullyConnect( edict_t *pEntity ) = 0;

	// Client is disconnecting from server
	virtual void			ClientDisconnect( edict_t *pEntity ) = 0;
	
	// Client is connected and should be put in the game
	virtual void			ClientPutInServer( edict_t *pEntity, char const *playername ) = 0;
	
	// The client has typed a command at the console
	virtual void			ClientCommand( edict_t *pEntity, const CCommand &args ) = 0;

	// Sets the client index for the client who typed the command into his/her console
	virtual void			SetCommandClient( int index ) = 0;
	
	// A player changed one/several replicated cvars (name etc)
	virtual void			ClientSettingsChanged( edict_t *pEdict ) = 0;
	
	// Determine PVS origin and set PVS for the player/viewentity
	virtual void			ClientSetupVisibility( edict_t *pViewEntity, edict_t *pClient, unsigned char *pvs, int pvssize ) = 0;
	
	// A block of CUserCmds has arrived from the user, decode them and buffer for execution during player simulation
	virtual float			ProcessUsercmds( edict_t *player, bf_read *buf, int numcmds, int totalcmds,
								int dropped_packets, bool ignore, bool paused ) = 0;
	
	// Let the game .dll do stuff after messages have been sent to all of the clients once the server frame is complete
	virtual void			PostClientMessagesSent( void ) = 0;

	// For players, looks up the CPlayerState structure corresponding to the player
	virtual CPlayerState	*GetPlayerState( edict_t *player ) = 0;

	// Get the ear position for a specified client
	virtual void			ClientEarPosition( edict_t *pEntity, Vector *pEarOrigin ) = 0;

	virtual bool			ClientReplayEvent( edict_t *pEntity, const ClientReplayEventParams_t &params ) = 0;

	// returns number of delay ticks if player is in Replay mode (0 = no delay)
	virtual int				GetReplayDelay( edict_t *player, int& entity ) = 0;

	// Anything this game .dll wants to add to the bug reporter text (e.g., the entity/model under the picker crosshair)
	//  can be added here
	virtual void			GetBugReportInfo( char *buf, int buflen ) = 0;

	// TERROR: A player sent a voice packet
	virtual void			ClientVoice( edict_t *pEdict ) = 0;

	// A user has had their network id setup and validated 
	virtual void			NetworkIDValidated( const char *pszUserName, const char *pszNetworkID, CSteamID steamID ) = 0;

	// Returns max splitscreen slot count ( 1 == no splits, 2 for 2-player split screen )
	virtual int				GetMaxSplitscreenPlayers() = 0;

	// Return # of human slots, -1 if can't determine or don't care (engine will assume it's == maxplayers )
	virtual int				GetMaxHumanPlayers() = 0;

	// The client has submitted a keyvalues command
	virtual void			ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues ) = 0;

	// Server override for supplied client name
	virtual const char *	ClientNameHandler( uint64 xuid, const char *pchName ) = 0;

	// Client submitted a user command
	virtual void			ClientSvcUserMessage( edict_t *pEntity, int nType, int nPassthrough, uint32 cbSize, const void *pvBuffer ) = 0;

};

#define INTERFACEVERSION_UPLOADGAMESTATS		"ServerUploadGameStats001"

abstract_class IUploadGameStats
{
public:
	// Note that this call will block the server until the upload is completed, so use only at levelshutdown if at all.
	virtual bool UploadGameStats( 
		char const *mapname,				// Game map name
		unsigned int blobversion,			// Version of the binary blob data
		unsigned int blobsize,				// Size in bytes of blob data
		const void *pvBlobData ) = 0;		// Pointer to the blob data.

	// Call when created to init the CSER connection
	virtual void InitConnection( void ) = 0;

	// Call periodically to poll steam for a CSER connection
	virtual void UpdateConnection( void ) = 0;

	// If user has disabled stats tracking, do nothing
	virtual bool IsGameStatsLoggingEnabled() = 0;

	// Gets a non-personally identifiable unique ID for this steam user, used for tracking total gameplay time across
	//  multiple stats sessions, but isn't trackable back to their Steam account or id.
	// Buffer should be 16 bytes, ID will come back as a hexadecimal string version of a GUID
	virtual void GetPseudoUniqueId( char *buf, size_t bufsize ) = 0;

	// For determining general % of users running using cyber cafe accounts...
	virtual bool IsCyberCafeUser( void ) = 0;

	// Only works in single player
	virtual bool IsHDREnabled( void ) = 0;
};

#define INTERFACEVERSION_PLUGINHELPERSCHECK		"PluginHelpersCheck001"

//-----------------------------------------------------------------------------
// Purpose: allows the game dll to control which plugin functions can be run
//-----------------------------------------------------------------------------
abstract_class IPluginHelpersCheck
{
public:
	virtual bool CreateMessage( const char *plugin, edict_t *pEntity, DIALOG_TYPE type, KeyValues *data ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: Interface exposed from the client .dll back to the engine for specifying shared .dll IAppSystems (e.g., ISoundEmitterSystem)
//-----------------------------------------------------------------------------
abstract_class IServerDLLSharedAppSystems
{
public:
	virtual int	Count() = 0;
	virtual char const *GetDllName( int idx ) = 0;
	virtual char const *GetInterfaceName( int idx ) = 0;
};

#define SERVER_DLL_SHARED_APPSYSTEMS		"VServerDllSharedAppSystems001"

#define INTERFACEVERSION_SERVERGAMETAGS		"ServerGameTags001"

//-----------------------------------------------------------------------------
// Purpose: querying the game dll for Server cvar tags
//-----------------------------------------------------------------------------
abstract_class IServerGameTags
{
public:
	// Get the list of cvars that require tags to show differently in the server browser
	virtual void			GetTaggedConVarList( KeyValues *pCvarTagList ) = 0;
};

#endif // EIFACE_H
