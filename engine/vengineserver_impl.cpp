//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//===========================================================================//
#include "server_pch.h"
#include <algorithm>
#include "vengineserver_impl.h"
#include "vox.h"
#include "sound.h"
#include "gl_model_private.h"
#include "host_saverestore.h"
#include "world.h"
#include "l_studio.h"
#include "decal.h"
#include "sys_dll.h"
#include "sv_log.h"
#include "sv_main.h"
#include "tier1/strtools.h"
#include "collisionutils.h"
#include "staticpropmgr.h"
#include "string_t.h"
#include "vstdlib/random.h"
#include "EngineSoundInternal.h"
#include "dt_send_eng.h"
#include "PlayerState.h"
#include "irecipientfilter.h"
#include "sv_user.h"
#include "server_class.h"
#include "cdll_engine_int.h"
#include "enginesingleuserfilter.h"
#include "ispatialpartitioninternal.h"
#include "con_nprint.h"
#include "tmessage.h"
#include "iscratchpad3d.h"
#include "pr_edict.h"
#include "networkstringtableserver.h"
#include "networkstringtable.h"
#include "LocalNetworkBackdoor.h"
#include "host_phonehome.h"
#include "sv_plugin.h"
#include "singleplayersharedmemory.h"
#include "MapReslistGenerator.h"
#include "sv_steamauth.h"
#include "LoadScreenUpdate.h"
#include "paint.h"
#include "matchmaking/imatchframework.h"
#include "r_decal.h"
#include "igame.h"
#include "sv_packedentities.h"
#include "hltvserver.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MAX_MESSAGE_SIZE	2500
#define MAX_TOTAL_ENT_LEAFS		128

void SV_DetermineMulticastRecipients( bool usepas, const Vector& origin, CPlayerBitVec& playerbits );

void DecalRemove( const model_t *model, const Vector& origin, float radius );

extern CNetworkStringTableContainer *networkStringTableContainerServer;

extern ConVar host_timescale;

CSharedEdictChangeInfo g_SharedEdictChangeInfo;
CSharedEdictChangeInfo *g_pSharedChangeInfo = &g_SharedEdictChangeInfo;
IAchievementMgr *g_pAchievementMgr = NULL;
CGamestatsData *g_pGamestatsData = NULL;

static ConVar sv_show_usermessage( "sv_show_usermessage", "0", 0, "Shows the user messages that the server is sending to clients. Setting this to 2 will show the contents of the message");

void InvalidateSharedEdictChangeInfos()
{
	if ( g_SharedEdictChangeInfo.m_iSerialNumber == 0xFFFF )
	{
		// Reset all edicts to 0.
		g_SharedEdictChangeInfo.m_iSerialNumber = 1;
		for ( int i=0; i < sv.num_edicts; i++ )
		{
			//if the edict has any changes, force a full state change (which will release the change info)
			if( sv.edicts[i].HasStateChanged() )
				sv.edicts[i].StateChanged();
		}
	}
	else
	{
		g_SharedEdictChangeInfo.m_iSerialNumber++;
	}

	g_SharedEdictChangeInfo.m_nChangeInfos = 0;
}


// ---------------------------------------------------------------------- //
// Globals.
// ---------------------------------------------------------------------- //

struct MsgData
{
	MsgData()
	{
		Reset();

		// link buffers to messages
		m_DataOut.StartWriting( entitydata, sizeof(entitydata) );
		m_DataOut.SetDebugName( "s_MsgData.entityMsg.m_DataOut" );
	}

	void Reset()
	{
		filter			= NULL;
		reliable		= false;
		started			= false;
		entityMsg.Clear();
	}

	byte				entitydata[ PAD_NUMBER( MAX_ENTITY_MSG_DATA, 4 ) ]; // buffer for outgoing entity messages

	IRecipientFilter	*filter;		// clients who get this message
	bool				reliable;
	
	bool				started;			// IS THERE A MESSAGE IN THE PROCESS OF BEING SENT?

	CSVCMsg_EntityMsg_t entityMsg;
	bf_write			m_DataOut;


};

static MsgData s_MsgData;

void SeedRandomNumberGenerator( bool random_invariant )
{
	if (!random_invariant)
	{
		int iSeed = -(long)Sys_FloatTime();
		if (1000 < iSeed)
		{
			iSeed = -iSeed;
		}
		else if (-1000 < iSeed)
		{
			iSeed -= 22261048;
		}
		RandomSeed( iSeed );
	}
	else
	{
		// Make those random numbers the same every time!
		RandomSeed( 0 );
	}
}

// ---------------------------------------------------------------------- //
// Static helpers.
// ---------------------------------------------------------------------- //

static void PR_CheckEmptyString (const char *s)
{
	if (s[0] <= ' ')
		Host_Error ("Bad string: %s", s);
}

// Average a list a vertices to find an approximate "center"
static void CenterVerts( Vector verts[], int vertCount, Vector& center )
{
	int i;
	float scale;

	if ( vertCount )
	{
		Vector edge0, edge1, normal;

		VectorCopy( vec3_origin, center );
		// sum up verts
		for ( i = 0; i < vertCount; i++ )
		{
			VectorAdd( center, verts[i], center );
		}
		scale = 1.0f / (float)vertCount;
		VectorScale( center, scale, center );	// divide by vertCount

		// Compute 2 poly edges
		VectorSubtract( verts[1], verts[0], edge0 );
		VectorSubtract( verts[vertCount-1], verts[0], edge1 );
		// cross for normal
		CrossProduct( edge0, edge1, normal );
		// Find the component of center that is outside of the plane
		scale = DotProduct( center, normal ) - DotProduct( verts[0], normal );
		// subtract it off
		VectorMA( center, scale, normal, center );
		// center is in the plane now
	}
}


// Copy the list of verts from an msurface_t int a linear array
static void SurfaceToVerts( model_t *model, SurfaceHandle_t surfID, Vector verts[], int *vertCount )
{
	if ( *vertCount > MSurf_VertCount( surfID ) )
		*vertCount = MSurf_VertCount( surfID );

	// Build the list of verts from 0 to n
	for ( int i = 0; i < *vertCount; i++ )
	{
		int vertIndex = model->brush.pShared->vertindices[ MSurf_FirstVertIndex( surfID ) + i ];
		Vector& vert = model->brush.pShared->vertexes[ vertIndex ].position;
		VectorCopy( vert, verts[i] );
	}
	// vert[0] is the first and last vert, there is no copy
}


// Calculate the surface area of an arbitrary msurface_t polygon (convex with collinear verts)
static float SurfaceArea( model_t *model, SurfaceHandle_t surfID )
{
	Vector	center, verts[32];
	int		vertCount = 32;
	float	area;
	int		i;

	// Compute a "center" point and fan
	SurfaceToVerts( model, surfID, verts, &vertCount );
	CenterVerts( verts, vertCount, center );

	area = 0;
	// For a triangle of the center and each edge
	for ( i = 0; i < vertCount; i++ )
	{
		Vector edge0, edge1, out;
		int next;

		next = (i+1)%vertCount;
		VectorSubtract( verts[i], center, edge0 );			// 0.5 * edge cross edge (0.5 is done once at the end)
		VectorSubtract( verts[next], center, edge1 );
		CrossProduct( edge0, edge1, out );
		area += VectorLength( out );
	}
	return area * 0.5;										// 0.5 here
}


// Average the list of vertices to find an approximate "center"
static void SurfaceCenter( model_t *model, SurfaceHandle_t surfID, Vector& center )
{
	Vector	verts[32];		// We limit faces to 32 verts elsewhere in the engine
	int		vertCount = 32;

	SurfaceToVerts( model, surfID, verts, &vertCount );
	CenterVerts( verts, vertCount, center );
}


static bool ValidCmd( const char *pCmd )
{
	int len;

	len = strlen(pCmd);

	// Valid commands all have a ';' or newline '\n' as their last character
	if ( len && (pCmd[len-1] == '\n' || pCmd[len-1] == ';') )
		return true;

	return false;
}

//
// HLTV relay proxy whitelist
//
struct HltvRelayProxyWhitelistMask_t
{
	uint32 a, b, c, d;
	uint32 numbits;

	bool operator==( HltvRelayProxyWhitelistMask_t const &other ) const
	{
		return 0 == Q_memcmp( this, &other, sizeof( *this ) );
	}
};
static CUtlVector< HltvRelayProxyWhitelistMask_t > s_arrHltvRelayProxyWhitelist;

bool IsHltvRelayProxyWhitelisted( ns_address const &adr )
{
	if ( !adr.IsType<netadr_t>() )
		return false;

	uint32 uiServerIP = adr.AsType<netadr_t>().GetIPHostByteOrder();
#define MAKE_IP_MASK( a, b, c, d, numbits ) ( uint32( ( (uint32(a)&0xFF) << 24 ) | ( (uint32(b)&0xFF) << 16 ) | ( (uint32(c)&0xFF) << 8 ) | ( (uint32(d)&0xFF) ) ) & ( (~0u) << (32-numbits) ) )
#define IPRANGEENTRY( a, b, c, d, numbits ) if ( ( uiServerIP & ( (~0u) << (32-numbits) ) ) == MAKE_IP_MASK( a, b, c, d, numbits ) ) return true;
	FOR_EACH_VEC( s_arrHltvRelayProxyWhitelist, idx )
	{
		HltvRelayProxyWhitelistMask_t const &e = s_arrHltvRelayProxyWhitelist[idx];
		IPRANGEENTRY( e.a, e.b, e.c, e.d, e.numbits );
	}
#undef IPRANGEENTRY
#undef MAKE_IP_MASK
	return false;
}

// ---------------------------------------------------------------------- //
// CVEngineServer
// ---------------------------------------------------------------------- //
class CVEngineServer : public IVEngineServer
{
public:

	virtual void ChangeLevel( const char* s1, const char* s2)
	{
		static	int	last_spawncount;
		
		// make sure we don't issue two changelevels
		if (sv.GetSpawnCount() == last_spawncount)
			return;

		last_spawncount = sv.GetSpawnCount();
		
		if ( !s1 )
		{
			Sys_Error( "CVEngineServer::Changelevel with NULL s1\n" );
		}
		
		char cmd[ 256 ];
		
		if ( !s2 ) // no indication of where they are coming from;  so just do a standard old changelevel
		{
			Q_snprintf( cmd, sizeof( cmd ), "changelevel %s\n", s1 );
		}
		else
		{
			Q_snprintf( cmd, sizeof( cmd ), "changelevel2 %s %s\n", s1, s2 );
		}
		
		Cbuf_AddText( CBUF_SERVER, cmd );
	}
	
	
	virtual int	IsMapValid( const char *filename )
	{
		return modelloader->Map_IsValid( filename );
	}
	
	virtual bool IsDedicatedServer( void )
	{
		return sv.IsDedicated();
	}

	virtual int GetLocalClientIndex( void )
	{
#ifndef DEDICATED
		if ( sv.IsDedicated() )
		{
			return 0; // this query really makes no sense on a dedicated server, but we can agree that "local client" means "the person typing at the text console"
		}
		else
	 	{
			// return splitscreen->IsLocalPlayerResolvable() ? GetLocalClient().m_nPlayerSlot : GetBaseLocalClient().m_nPlayerSlot; // this is the same check as in Cmd_ExecuteCommand()
			return GetBaseLocalClient().m_nPlayerSlot + 1; // Conver 0-based slot to 1-based client index. Look at UTIL_GetCommandClientIndex() usage for reference.
		}
#else
		return 0;
#endif
	}
	
	virtual int IsInEditMode( void )
	{
#ifdef DEDICATED
		return false;
#else
		return g_bInEditMode;
#endif
	}

	virtual int IsInCommentaryMode( void )
	{
#ifdef DEDICATED
		return false;
#else
		return g_bInCommentaryMode;
#endif
	}

	virtual KeyValues* GetLaunchOptions( void )
	{
		return g_pLaunchOptions;
	}
	
	virtual bool IsLevelMainMenuBackground( void )
	{
		return sv.IsLevelMainMenuBackground();
	}
	
	virtual void NotifyEdictFlagsChange( int iEdict )
	{
		if ( g_pLocalNetworkBackdoor )
			g_pLocalNetworkBackdoor->NotifyEdictFlagsChange( iEdict );
	}

	virtual const CCheckTransmitInfo* GetPrevCheckTransmitInfo( edict_t *pPlayerEdict )
	{
		int entnum = NUM_FOR_EDICT( pPlayerEdict );
		if ( entnum < 1 || entnum > sv.GetClientCount() )
		{
			Error( "Invalid client specified in GetPrevCheckTransmitInfo\n" );
			return NULL;
		}
		
		CGameClient *client = sv.Client( entnum-1 );
		return client->GetPrevPackInfo();		
	}
	
	virtual int PrecacheDecal( const char *name, bool preload /*=false*/ )
	{
		PR_CheckEmptyString( name );
		int i = SV_FindOrAddDecal( name, preload );
		if ( i >= 0 )
		{
			return i;
		}
		
		Host_Error( "CVEngineServer::PrecacheDecal: '%s' overflow, too many decals", name );
		return 0;
	}
	
	virtual int PrecacheModel( const char *s, bool preload /*= false*/ )
	{
		PR_CheckEmptyString (s);
		int i = SV_FindOrAddModel( s, preload );
		if ( i >= 0 )
		{
			return i;
		}
		
		Host_Error( "CVEngineServer::PrecacheModel: '%s' overflow, too many models", s );
		return 0;
	}
	
	
	virtual int PrecacheGeneric(const char *s, bool preload /*= false*/ )
	{
		int		i;
		
		PR_CheckEmptyString (s);
		i = SV_FindOrAddGeneric( s, preload );
		if (i >= 0)
		{
			return i;
		}
		
		Host_Error ("CVEngineServer::PrecacheGeneric: '%s' overflow", s);
		return 0;
	}
	
	virtual bool IsModelPrecached( char const *s ) const
	{
		int idx = SV_ModelIndex( s );
		return idx != -1 ? true : false;
	}

	virtual bool IsDecalPrecached( char const *s ) const
	{
		int idx = SV_DecalIndex( s );
		return idx != -1 ? true : false;
	}

	virtual bool IsGenericPrecached( char const *s ) const
	{
		int idx = SV_GenericIndex( s );
		return idx != -1 ? true : false;
	}

	virtual void ForceExactFile( const char *s )
	{
		PR_CheckEmptyString( s );
		SV_ForceExactFile( s );
	}

	virtual void ForceModelBounds( const char *s, const Vector &mins, const Vector &maxs )
	{
		PR_CheckEmptyString( s );
		SV_ForceModelBounds( s, mins, maxs );
	}

	virtual void ForceSimpleMaterial( const char *s )
	{
		PR_CheckEmptyString( s );
		SV_ForceSimpleMaterial( s );
	}

	virtual bool IsInternalBuild( void )
	{
		return !phonehome->IsExternalBuild();
	}

	//-----------------------------------------------------------------------------
	// Purpose: Precache a sentence file (parse on server, send to client)
	// Input  : *s - file name
	//-----------------------------------------------------------------------------
	virtual int PrecacheSentenceFile( const char *s, bool preload /*= false*/ )
	{
		// UNDONE:  Set up preload flag
		
		// UNDONE: Send this data to the client to support multiple sentence files
		VOX_ReadSentenceFile( s );
		
		return 0;
	}
	
	//-----------------------------------------------------------------------------
	// Purpose: Retrieves the pvs for an origin into the specified array
	// Input  : *org - origin
	//			outputpvslength - size of outputpvs array in bytes
	//			*outputpvs - If null, then return value is the needed length
	// Output : int - length of pvs array used ( in bytes )
	//-----------------------------------------------------------------------------
	virtual int GetClusterForOrigin( const Vector& org )
	{
		return CM_LeafCluster( CM_PointLeafnum( org ) );
	}
	
	virtual int GetPVSForCluster( int clusterIndex, int outputpvslength, unsigned char *outputpvs )
	{
		int length = (CM_NumClusters()+7)>>3;
		
		if ( outputpvs )
		{
			if ( outputpvslength < length )
			{
				Sys_Error( "GetPVSForOrigin called with inusfficient sized pvs array, need %i bytes!", length );
				return length;
			}
			
			CM_Vis( outputpvs, outputpvslength, clusterIndex, DVIS_PVS );
		}
		
		return length;
	}
	
	//-----------------------------------------------------------------------------
	// Purpose: Test origin against pvs array retreived from GetPVSForOrigin
	// Input  : *org - origin to chec
	//			checkpvslength - length of pvs array
	//			*checkpvs - 
	// Output : bool - true if entity is visible
	//-----------------------------------------------------------------------------
	virtual bool CheckOriginInPVS( const Vector& org, const unsigned char *checkpvs, int checkpvssize )
	{
		int clusterIndex = CM_LeafCluster( CM_PointLeafnum( org ) );
		
		if ( clusterIndex < 0 )
			return false;
		
		int offset = clusterIndex>>3;
		if ( offset > checkpvssize )
		{
			Sys_Error( "CheckOriginInPVS:  cluster would read past end of pvs data (%i:%i)\n",
				offset, checkpvssize );
			return false;
		}
		
		if ( !(checkpvs[offset] & (1<<(clusterIndex&7)) ) )
		{
			return false;
		}
		
		return true;
	}
	
	//-----------------------------------------------------------------------------
	// Purpose: Test origin against pvs array retreived from GetPVSForOrigin
	// Input  : *org - origin to chec
	//			checkpvslength - length of pvs array
	//			*checkpvs - 
	// Output : bool - true if entity is visible
	//-----------------------------------------------------------------------------
	virtual bool CheckBoxInPVS( const Vector& mins, const Vector& maxs, const unsigned char *checkpvs, int checkpvssize )
	{
		if ( !CM_BoxVisible( mins, maxs, checkpvs, checkpvssize ) )
		{
			return false;
		}
		
		return true;
	}
	
	virtual int GetPlayerUserId( const edict_t *e )
	{
		if ( !sv.IsActive() || !e)
			return -1;
		
		for ( int i = 0; i < sv.GetClientCount(); i++ )
		{
			CGameClient *cl = sv.Client(i);
			
			if ( cl->edict == e )
			{
				return cl->m_UserID;
			}
		}
		
		// Couldn't find it
		return -1;
	}

	virtual const char *GetPlayerNetworkIDString( const edict_t *e )
	{
		if ( !sv.IsActive() || !e)
			return NULL;
		
		for ( int i = 0; i < sv.GetClientCount(); i++ )
		{
			CGameClient *cl = sv.Client(i);
			
			if ( cl->edict == e )
			{
				return cl->GetNetworkIDString();
			}
		}
		
		// Couldn't find it
		return NULL;

	}

	virtual bool IsUserIDInUse( int userID )
	{
		if ( !sv.IsActive() )
			return false;

		for ( int i = 0; i < sv.GetClientCount(); i++ )
		{
			CGameClient *cl = sv.Client(i);

			if ( cl->GetUserID() == userID )
			{
				return true;
			}
		}

		// Couldn't find it
		return false;
	}


	virtual int GetLoadingProgressForUserID( int userID )
	{
		if ( !sv.IsActive() )
			return false;

		for ( int i = 0; i < sv.GetClientCount(); i++ )
		{
			CGameClient *cl = sv.Client(i);

			if ( cl->GetUserID() == userID )
			{
				return cl->m_nLoadingProgress;
			}
		}

		// Couldn't find it
		return -1;
	}

	virtual int	GetEntityCount( void )
	{
		return sv.num_edicts;
	}
	
	

	virtual INetChannelInfo* GetPlayerNetInfo( int playerIndex )
	{
		if ( playerIndex < 1 || playerIndex > sv.GetClientCount() )
			return NULL;

		CGameClient *client = sv.Client( playerIndex - 1 );
		
		return client->m_NetChannel;
	}

	virtual edict_t* CreateEdict( int iForceEdictIndex )
	{
		edict_t	*pedict = ED_Alloc( iForceEdictIndex );
		if ( g_pServerPluginHandler )
		{
			g_pServerPluginHandler->OnEdictAllocated( pedict );
		}
		return pedict;
	}
	
	
	virtual void RemoveEdict(edict_t* ed)
	{
		if ( g_pServerPluginHandler )
		{
			g_pServerPluginHandler->OnEdictFreed( ed );
		}
		ED_Free(ed);
	}
	
	//
	// Request engine to allocate "cb" bytes on the entity's private data pointer.
	//
	virtual void *PvAllocEntPrivateData( long cb )
	{
		return calloc( 1, cb );
	}
	
	
	//
	// Release the private data memory, if any.
	//
	virtual void FreeEntPrivateData( void *pEntity )
	{
#if defined( _DEBUG ) && defined( WIN32 )
		// set the memory to a known value
		int size = _msize( pEntity );
		memset( pEntity, 0xDD, size );
#endif		
		
		if ( pEntity )
		{
			free( pEntity );
		}
	}
	
	virtual void		*SaveAllocMemory( size_t num, size_t size )
	{
#ifndef DEDICATED
		return ::SaveAllocMemory(num, size);
#else
		return NULL;
#endif
	}
	
	virtual void		SaveFreeMemory( void *pSaveMem )
	{
#ifndef DEDICATED
		::SaveFreeMemory(pSaveMem);
#endif
	}
	
	/*
	=================
	EmitAmbientSound
	
	  =================
	*/
	virtual void EmitAmbientSound( int entindex, const Vector& pos, const char *samp, float vol, 
		soundlevel_t soundlevel, int fFlags, int pitch, float soundtime /*=0.0f*/ )
	{
		SoundInfo_t sound; 
		sound.SetDefault();
		
		sound.nEntityIndex = entindex;
		sound.fVolume = vol;
		sound.Soundlevel = soundlevel;
		sound.nFlags = fFlags;
		sound.nPitch = pitch;
		sound.nChannel = CHAN_STATIC;
		sound.vOrigin = pos;
		sound.bIsAmbient = true;

		ASSERT_COORD( sound.vOrigin );
		
		// set sound delay
		
		if ( soundtime != 0.0f )
		{
			sound.fDelay = soundtime - sv.GetTime();
			sound.nFlags |= SND_DELAY;
		}
		
		// if this is a sentence, get sentence number
		if ( TestSoundChar(samp, CHAR_SENTENCE) )
		{
			sound.bIsSentence = true;
			sound.nSoundNum = Q_atoi( PSkipSoundChars(samp) );
			if ( sound.nSoundNum >= (unsigned int)VOX_SentenceCount() )
			{
				ConMsg("EmitAmbientSound: invalid sentence number: %s", PSkipSoundChars(samp));
				return;
			}
		}
		else
		{
			// check to see if samp was properly precached
			sound.bIsSentence = false;
			sound.nSoundNum = SV_SoundIndex( samp );
			if (sound.nSoundNum <= 0)
			{
				ConMsg ("EmitAmbientSound:  sound not precached: %s\n", samp);
				return;
			}
		}

		if ( (fFlags & SND_SPAWNING) && sv.allowsignonwrites )
		{
			CSVCMsg_Sounds_t sndmsg;

			sndmsg.set_reliable_sound( true );
			sound.WriteDelta( NULL, sndmsg, sv.GetFinalTickTime() );
			
			 // write into signon buffer
			if ( !sndmsg.WriteToBuffer( sv.m_Signon ) )
			{
				Sys_Error( "EmitAmbientSound: Init message would overflow signon buffer!\n" );
				return;
			}
		}
		else
		{
			if ( fFlags & SND_SPAWNING )
			{
				DevMsg("EmitAmbientSound: warning, broadcasting sound labled as SND_SPAWNING.\n" );
			}

			// send sound to all active players
			CEngineRecipientFilter filter;
			filter.AddAllPlayers();
			filter.MakeReliable();
			sv.BroadcastSound( sound, filter );
		}
	}
	
	
	virtual void FadeClientVolume(const edict_t *clientent,
		float fadePercent, float fadeOutSeconds, float holdTime, float fadeInSeconds)
	{
		int entnum = NUM_FOR_EDICT(clientent);
		
		if (entnum < 1 || entnum > sv.GetClientCount() )
		{
			ConMsg ("tried to DLL_FadeClientVolume a non-client\n");
			return;
		}
		
		IClient	*client = sv.Client(entnum-1);

		CNETMsg_StringCmd_t sndMsg( va("soundfade	%.1f %.1f %.1f %.1f", fadePercent, holdTime, fadeOutSeconds, fadeInSeconds ) );
				
		client->SendNetMsg( sndMsg );
	}
	
	
	//-----------------------------------------------------------------------------
	//
	// Sentence API
	//
	//-----------------------------------------------------------------------------
	
	virtual int SentenceGroupPick( int groupIndex, char *name, int nameLen )
	{
		if ( !name )
		{
			Sys_Error( "SentenceGroupPick with NULL name\n" );
		}
		
		Assert( nameLen > 0 );
		
		return VOX_GroupPick( groupIndex, name, nameLen );
	}
	
	
	virtual int SentenceGroupPickSequential( int groupIndex, char *name, int nameLen, int sentenceIndex, int reset )
	{
		if ( !name )
		{
			Sys_Error( "SentenceGroupPickSequential with NULL name\n" );
		}
		
		Assert( nameLen > 0 );
		
		return VOX_GroupPickSequential( groupIndex, name, nameLen, sentenceIndex, reset );
	}
	
	virtual int SentenceIndexFromName( const char *pSentenceName )
	{
		if ( !pSentenceName )
		{
			Sys_Error( "SentenceIndexFromName with NULL pSentenceName\n" );
		}
		
		int sentenceIndex = -1;
		
		VOX_LookupString( pSentenceName, &sentenceIndex );
		
		return sentenceIndex;
	}
	
	virtual const char *SentenceNameFromIndex( int sentenceIndex )
	{
		return VOX_SentenceNameFromIndex( sentenceIndex );
	}
	
	
	virtual int SentenceGroupIndexFromName( const char *pGroupName )
	{
		if ( !pGroupName )
		{
			Sys_Error( "SentenceGroupIndexFromName with NULL pGroupName\n" );
		}
		
		return VOX_GroupIndexFromName( pGroupName );
	}
	
	virtual const char *SentenceGroupNameFromIndex( int groupIndex )
	{
		return VOX_GroupNameFromIndex( groupIndex );
	}
	
	
	virtual float SentenceLength( int sentenceIndex )
	{
		return VOX_SentenceLength( sentenceIndex );
	}
	//-----------------------------------------------------------------------------
	
	virtual int			CheckHeadnodeVisible( int nodenum, const byte *visbits, int vissize )
	{
		return CM_HeadnodeVisible(nodenum, visbits, vissize );
	}
	
	/*
	=================
	ServerCommand
	
	  Sends text to servers execution buffer
	  
		localcmd (string)
		=================
	*/
	virtual void ServerCommand( const char *str )
	{
		if ( !str )
		{
			Sys_Error( "ServerCommand with NULL string\n" );
		}
		if ( ValidCmd( str ) )
		{
			Cbuf_AddText( CBUF_SERVER, str );
		}
		else
		{
			ConMsg( "Error, bad server command %s\n", str );
		}
	}
	
	
	/*
	=================
	ServerExecute
	
	  Executes all commands in server buffer
	  
		localcmd (string)
		=================
	*/
	virtual void ServerExecute( void )
	{
		Cbuf_Execute();
	}
	
	
	/*
	=================
	ClientCommand
	
	  Sends text over to the client's execution buffer
	  
		stuffcmd (clientent, value)
		=================
	*/
	virtual void ClientCommand(edict_t* pEdict, const char* szFmt, ...)
	{
		va_list		argptr; 
		static char	szOut[1024];
		
		va_start(argptr, szFmt);
		Q_vsnprintf(szOut, sizeof( szOut ), szFmt, argptr);
		va_end(argptr);

		if ( szOut[0] == 0 )
		{
			Warning( "ClientCommand, 0 length string supplied.\n" );
			return;
		}

		int entnum = NUM_FOR_EDICT( pEdict );
		
		if ( ( entnum < 1 ) || ( entnum >  sv.GetClientCount() ) )
		{
			ConMsg("\n!!!\n\nStuffCmd:  Some entity tried to stuff '%s' to console buffer of entity %i when maxclients was set to %i, ignoring\n\n",
				szOut, entnum, sv.GetMaxClients() );
			return;
		}
		
		CNETMsg_StringCmd_t string( szOut );
		sv.GetClient(entnum-1)->SendNetMsg( string );
		
	}

	// Send a client command keyvalues
	// keyvalues are deleted inside the function
	virtual void ClientCommandKeyValues( edict_t *pEdict, KeyValues *pCommand )
	{
		if ( !pCommand )
			return;
		
		// Ensure the contract of deleting the key values here
		KeyValues::AutoDelete autodelete_pCommand( pCommand );

		int entnum = NUM_FOR_EDICT( pEdict );

		if ( ( entnum < 1 ) || ( entnum >  sv.GetClientCount() ) )
		{
			ConMsg("\n!!!\n\nClientCommandKeyValues:  Some entity tried to stuff '%s' to console buffer of entity %i when maxclients was set to %i, ignoring\n\n",
				pCommand->GetName(), entnum, sv.GetMaxClients() );
			return;
		}

		CSVCMsg_CmdKeyValues_t cmd;
		CmdKeyValuesHelper::SVCMsg_SetKeyValues( cmd, pCommand );
		sv.GetClient(entnum-1)->SendNetMsg( cmd );	
	}
	
	/*
	===============
	LightStyle
	
	  void(float style, string value) lightstyle
	  ===============
	*/
	virtual void LightStyle(int style, const char* val)
	{
		if ( !val )
		{
			Sys_Error( "LightStyle with NULL value!\n" );
		}

		// change the string in string table

		INetworkStringTable *stringTable = sv.GetLightStyleTable();

		stringTable->SetStringUserData( style, Q_strlen(val)+1, val );
	}
		
		
	virtual void StaticDecal( const Vector& origin, int decalIndex, int entityIndex, int modelIndex, bool lowpriority )
	{
		CSVCMsg_BSPDecal_t decal;

		decal.mutable_pos()->set_x( origin.x );
		decal.mutable_pos()->set_y( origin.y );
		decal.mutable_pos()->set_z( origin.z );
		decal.set_decal_texture_index( decalIndex );
		decal.set_entity_index( entityIndex );
		decal.set_model_index( modelIndex );
		decal.set_low_priority( lowpriority );

		if ( sv.allowsignonwrites )
		{
			decal.WriteToBuffer( sv.m_Signon );
		}
		else
		{
			sv.BroadcastMessage( decal, false, true );
		}
	}

	void Message_DetermineMulticastRecipients( bool usepas, const Vector& origin, CPlayerBitVec& playerbits )
	{
		SV_DetermineMulticastRecipients( usepas, origin, playerbits );
	}
	
	/*
	===============================================================================
	
	  MESSAGE WRITING
	  
		===============================================================================
	*/
	
	virtual bf_write *EntityMessageBegin( int ent_index, ServerClass * ent_class, bool reliable )
	{
		if ( s_MsgData.started )
		{
			Sys_Error( "EntityMessageBegin:  New message started before matching call to EndMessage.\n " );
			return NULL;
		}
		
		s_MsgData.Reset();
		
		Assert( ent_class );
				
		s_MsgData.filter = NULL;
		s_MsgData.reliable = reliable;
		
		s_MsgData.started = true;
		
		s_MsgData.entityMsg.set_ent_index( ent_index );
		s_MsgData.entityMsg.set_class_id( ent_class->m_ClassID );
		s_MsgData.m_DataOut.Reset();	
				
		return &s_MsgData.m_DataOut;
	}
	
	// Validates user message type and checks to see if it's variable length
	// returns true if variable length
	int Message_CheckMessageLength()
	{
		int bytesWritten = s_MsgData.m_DataOut.GetNumBytesWritten();
			
		if ( bytesWritten > MAX_ENTITY_MSG_DATA )	// TODO use a define or so
		{
			Warning( "Entity Message to %i, %i bytes written (max is %d)\n",
				s_MsgData.entityMsg.ent_index(), bytesWritten, MAX_ENTITY_MSG_DATA );
			return -1;
		}
			
		return bytesWritten; // all checks passed, estimated final length
	}
	
	virtual void MessageEnd( void )
	{
		if ( !s_MsgData.started )
		{
			Sys_Error( "MESSAGE_END called with no active message\n" );
			return;
		}
		
		int length = Message_CheckMessageLength();
		
		// check to see if it's a valid message
		if ( length < 0 )
		{
			s_MsgData.Reset(); // clear message data
			return;
		}

		s_MsgData.entityMsg.set_ent_data( s_MsgData.entitydata, s_MsgData.m_DataOut.GetNumBytesWritten() );

		if ( s_MsgData.filter )
		{
			// send entity/user messages only to full connected clients in filter
			sv.BroadcastMessage( s_MsgData.entityMsg, *s_MsgData.filter );
		}
		else
		{
			// send entity messages to all full connected clients 
			sv.BroadcastMessage( s_MsgData.entityMsg, true, s_MsgData.reliable );
		}
		
		s_MsgData.Reset(); // clear message data
	}
	
	virtual void SendUserMessage( IRecipientFilter& filter, int message, const ::google::protobuf::Message &msg )
	{
		CSVCMsg_UserMessage_t _userMsg;

		if ( !msg.IsInitialized() )
		{
			Msg("SendUserMessage %s(%d) is not initialized! Probably missing required fields!\n", msg.GetTypeName().c_str(), message );
		}

		int size = msg.ByteSize();

		if ( sv_show_usermessage.GetBool() )
		{
			Msg("SendUserMessage - %s(%d) bytes: %d\n", msg.GetTypeName().c_str(), message, size );
			if( sv_show_usermessage.GetInt() > 1 )
				Msg("%s", msg.DebugString().c_str() );
		}

		_userMsg.set_msg_type( message );
		_userMsg.mutable_msg_data()->resize( size );
		if ( !msg.SerializeWithCachedSizesToArray( (uint8*)&(*_userMsg.mutable_msg_data())[0] ) )
		{
			Msg( "SendUserMessage: Error serializing %s!\n", msg.GetTypeName().c_str() );
			return;
		}

		sv.BroadcastMessage( _userMsg, filter );
	}

	/* single print to a specific client */
	virtual void ClientPrintf( edict_t *pEdict, const char *szMsg )
	{
		int entnum = NUM_FOR_EDICT( pEdict );
		
		if (entnum < 1 || entnum > sv.GetClientCount() )
		{
			ConMsg ("tried to sprint to a non-client\n");
			return;
		}
		
		sv.Client(entnum-1)->ClientPrintf( "%s", szMsg );
	}
	
#ifdef DEDICATED
	void Con_NPrintf( int pos, const char *fmt, ... )
	{
	}

	void Con_NXPrintf( const struct con_nprint_s *info, const char *fmt, ... )
	{
	}
#else

	void Con_NPrintf( int pos, const char *fmt, ... )
	{
		if ( IsDedicatedServer() )
			return;

		va_list		argptr;
		char		text[4096];
		va_start (argptr, fmt);
		Q_vsnprintf(text, sizeof( text ), fmt, argptr);
		va_end (argptr);

		::Con_NPrintf( pos, "%s", text );
	}

	void Con_NXPrintf( const struct con_nprint_s *info, const char *fmt, ... )
	{
		if ( IsDedicatedServer() )
			return;

		va_list		argptr;
		char		text[4096];
		va_start (argptr, fmt);
		Q_vsnprintf(text, sizeof( text ), fmt, argptr);
		va_end (argptr);

		::Con_NXPrintf( info, "%s", text );
	}
#endif

	virtual void SetView(const edict_t *clientent, const edict_t *viewent)
	{
		int clientnum = NUM_FOR_EDICT( clientent );
		if (clientnum < 1 || clientnum > sv.GetClientCount() )
			Host_Error ("DLL_SetView: not a client");
		
		CGameClient *client = sv.Client(clientnum-1);

		client->m_pViewEntity = viewent;
		
		CSVCMsg_SetView_t view;
		view.set_entity_index( NUM_FOR_EDICT(viewent) );

		client->SendNetMsg( view );
	}
	
	virtual void CrosshairAngle(const edict_t *clientent, float pitch, float yaw)
	{
		int clientnum = NUM_FOR_EDICT( clientent );

		if (clientnum < 1 || clientnum > sv.GetClientCount() )
			Host_Error ("DLL_Crosshairangle: not a client");
		
		IClient *client = sv.Client(clientnum-1);

		if (pitch > 180)
			pitch -= 360;
		if (pitch < -180)
			pitch += 360;
		if (yaw > 180)
			yaw -= 360;
		if (yaw < -180)
			yaw += 360;
		
		CSVCMsg_CrosshairAngle_t crossHairMsg;

		crossHairMsg.mutable_angle()->set_x( pitch );
		crossHairMsg.mutable_angle()->set_y( yaw );
		crossHairMsg.mutable_angle()->set_z( 0 );

		client->SendNetMsg( crossHairMsg );
	}
	
	
	virtual void GetGameDir( char *szGetGameDir, int maxlength )
	{
		COM_GetGameDir(szGetGameDir, maxlength );
	}		
	
	virtual int CompareFileTime( const char *filename1, const char *filename2, int *iCompare)
	{
		return COM_CompareFileTime(filename1, filename2, iCompare);
	}
	
	virtual bool LockNetworkStringTables( bool lock )
	{
		return networkStringTableContainerServer->Lock( lock );
	}

	// For use with FAKE CLIENTS
	virtual edict_t* CreateFakeClient( const char *netname )
	{
		CGameClient *fcl = static_cast<CGameClient*>(sv.CreateFakeClient(netname));
		if ( !fcl )
		{
			// server is full
			return NULL;		
		}

		fcl->UpdateUserSettings();

		return fcl->edict;
		
	}
	
	// Get a keyvalue for s specified client
	virtual const char *GetClientConVarValue( int clientIndex, const char *name )
	{
		if ( clientIndex < 1 || clientIndex > sv.GetClientCount() )
		{
			DevMsg( 1, "GetClientConVarValue: player invalid index %i\n", clientIndex );
			return "";
		}

		return sv.GetClient( clientIndex - 1 )->GetUserSetting( name );
	}
	
	virtual const char *ParseFile(const char *data, char *token, int maxlen)
	{
		return ::COM_ParseFile(data, token, maxlen );
	}

	virtual bool CopyFile( const char *source, const char *destination )
	{
		return ::COM_CopyFile( source, destination );
	}
	
	virtual void AddOriginToPVS( const Vector& origin )
	{
		::SV_AddOriginToPVS(origin);
	}
	
	virtual void ResetPVS( byte* pvs, int pvssize )
	{
		::SV_ResetPVS( pvs, pvssize );
	}
	
	virtual void		SetAreaPortalState( int portalNumber, int isOpen )
	{
		CM_SetAreaPortalState(portalNumber, isOpen);
	}

	virtual void		SetAreaPortalStates( const int *portalNumbers, const int *isOpen, int nPortals )
	{
		CM_SetAreaPortalStates( portalNumbers, isOpen, nPortals );
	}

	virtual void		DrawMapToScratchPad( IScratchPad3D *pPad, unsigned long iFlags )
	{
		worldbrushdata_t *pData = host_state.worldmodel->brush.pShared;
		if ( !pData )
			return;

		SurfaceHandle_t surfID = SurfaceHandleFromIndex( host_state.worldmodel->brush.firstmodelsurface, pData );
		for (int i=0; i< host_state.worldmodel->brush.nummodelsurfaces; ++i, ++surfID)
		{
			// Don't bother with nodraw surfaces
			if( MSurf_Flags( surfID ) & SURFDRAW_NODRAW )
				continue;

			CSPVertList vertList;
			for ( int iVert=0; iVert < MSurf_VertCount( surfID ); iVert++ )
			{
				int iWorldVert = pData->vertindices[surfID->firstvertindex + iVert];
				const Vector &vPos = pData->vertexes[iWorldVert].position;

				vertList.m_Verts.AddToTail( CSPVert( vPos ) );
			}

			pPad->DrawPolygon( vertList );
		}
	}

	const CBitVec<MAX_EDICTS>* GetEntityTransmitBitsForClient( int iClientIndex )
	{
		if ( iClientIndex < 0 || iClientIndex >= sv.GetClientCount() )
		{
			Assert( false );
			return NULL;
		}

		CGameClient *pClient = sv.Client( iClientIndex );
		CClientFrame *deltaFrame = pClient->GetClientFrame( pClient->m_nDeltaTick );
		if ( !deltaFrame )
			return NULL;

		return &deltaFrame->transmit_entity;
	}

	virtual bool IsPaused()
	{
		return sv.IsPaused();
	}

	virtual float GetTimescale( void ) const
	{
		extern float CL_GetHltvReplayTimeScale();
		return sv.GetTimescale() * host_timescale.GetFloat() * CL_GetHltvReplayTimeScale();
	}

	virtual void SetFakeClientConVarValue( edict_t *pEntity, const char *cvar, const char *value )
	{
		int clientnum = NUM_FOR_EDICT( pEntity );
		if (clientnum < 1 || clientnum > sv.GetClientCount() )
			Host_Error ("DLL_SetView: not a client");

		CGameClient *client = sv.Client(clientnum-1);
		if ( client->IsFakeClient() )
		{
			client->SetUserCVar( cvar, value );
			client->m_bConVarsChanged = true;
		}
	}

	virtual CSharedEdictChangeInfo* GetSharedEdictChangeInfo()
	{
		return &g_SharedEdictChangeInfo;
	}

	virtual IChangeInfoAccessor *GetChangeAccessor( const edict_t *pEdict )
	{
		extern int NUM_FOR_EDICTINFO( const edict_t * e );

		int idx = NUM_FOR_EDICTINFO( pEdict );
		return &sv.edictchangeinfo[ idx ];
	}

	virtual QueryCvarCookie_t StartQueryCvarValue( edict_t *pPlayerEntity, const char *pCvarName )
	{
		int clientnum = NUM_FOR_EDICT( pPlayerEntity );
		if (clientnum < 1 || clientnum > sv.GetClientCount() )
			Host_Error( "StartQueryCvarValue: not a client" );

		CGameClient *client = sv.Client( clientnum-1 );
		return SendCvarValueQueryToClient( client, pCvarName, false );
	}

	// Name of most recently load .sav file
	virtual char const *GetMostRecentlyLoadedFileName()
	{
#if !defined( DEDICATED )
		return saverestore->GetMostRecentlyLoadedFileName();
#else
		return "";
#endif
	}

	virtual char const *GetSaveFileName()
	{
#if !defined( DEDICATED )
		return saverestore->GetSaveFileName();
#else
		return "";
#endif
	}

	// Tells the engine we can immdiately re-use all edict indices
	// even though we may not have waited enough time
	virtual void AllowImmediateEdictReuse( )
	{
		ED_AllowImmediateReuse();
	}
			
	virtual void SetAchievementMgr( IAchievementMgr *pAchievementMgr )
	{
		g_pAchievementMgr = pAchievementMgr;
	}
	
	virtual IAchievementMgr *GetAchievementMgr() 
	{
		return g_pAchievementMgr;
	}

	virtual int GetAppID()
	{
		return GetSteamAppID();
	}
	
	virtual bool IsLowViolence();

	virtual bool IsAnyClientLowViolence();

	/*
	=================
	InsertServerCommand
	
	  Sends text to servers execution buffer
	  
		localcmd (string)
		=================
	*/
	virtual void InsertServerCommand( const char *str )
	{
		if ( !str )
		{
			Sys_Error( "InsertServerCommand with NULL string\n" );
		}
		if ( ValidCmd( str ) )
		{
			Cbuf_InsertText( CBUF_SERVER, str );
		}
		else
		{
			ConMsg( "Error, bad server command %s (InsertServerCommand)\n", str );
		}
	}

	bool GetPlayerInfo( int ent_num, player_info_t *pinfo )
	{
		// Entity numbers are offset by 1 from the player numbers
		return sv.GetPlayerInfo( (ent_num-1), pinfo );
	}

	bool IsClientFullyAuthenticated( edict_t *pEdict )
	{
		int entnum = NUM_FOR_EDICT( pEdict );
		if (entnum < 1 || entnum > sv.GetClientCount() )
			return false;

		// Entity numbers are offset by 1 from the player numbers
		CGameClient *client = sv.Client(entnum-1);
		if ( client )
			return client->IsFullyAuthenticated();

		return false;
	}
	
	virtual ISPSharedMemory *GetSinglePlayerSharedMemorySpace( const char *szName, int ent_num = MAX_EDICTS )
	{
		return g_pSinglePlayerSharedMemoryManager->GetSharedMemory( szName, ent_num );
	}

	virtual void *AllocLevelStaticData( size_t bytes )
	{
		return Hunk_AllocName( bytes, "AllocLevelStaticData", false );
	}
	bool IsSplitScreenPlayer( int entnum )
	{
		if (entnum < 1 || entnum > sv.GetClientCount() )
			return false;

		CGameClient *client = sv.Client(entnum-1);
		return client->IsSplitScreenUser();
	}

	edict_t *GetSplitScreenPlayerAttachToEdict( int ent_num )
	{
		if (ent_num < 1 || ent_num > sv.GetClientCount() )
			return NULL;

		CGameClient *client = sv.Client(ent_num-1);
		if ( !client->IsSplitScreenUser() )
			return NULL;

		Assert( client->m_pAttachedTo );
		if ( !client->m_pAttachedTo )
			return NULL;

		return static_cast< CGameClient * >( client->m_pAttachedTo )->edict;
	}

	CrossPlayPlatform_t GetClientCrossPlayPlatform( int entnum )
	{
		if (entnum < 1 || entnum > sv.GetClientCount() )
			return CROSSPLAYPLATFORM_UNKNOWN;

		CGameClient *client = sv.Client(entnum-1);
		return client->GetClientPlatform();
	}

	void EnsureInstanceBaseline( int ent_num )
	{
		edict_t *pEnt = EDICT_NUM( ent_num );
		Assert ( pEnt && pEnt->GetNetworkable() );
		if ( pEnt && pEnt->GetNetworkable() )
		{
			SerializedEntityHandle_t handle = g_pSerializedEntities->AllocateSerializedEntity(__FILE__, __LINE__);
			Assert( handle != SERIALIZED_ENTITY_HANDLE_INVALID);
			if ( handle != SERIALIZED_ENTITY_HANDLE_INVALID )
			{
				ServerClass *pServerClass = pEnt->GetNetworkable()->GetServerClass();
				SV_EnsureInstanceBaseline( pServerClass, ent_num, handle );
			}
			g_pSerializedEntities->ReleaseSerializedEntity( handle );
		}
	}

	// Sets server reservation payload
	bool ReserveServerForQueuedGame( char const *szReservationPayload )
	{
		bool bResult = sv.ReserveServerForQueuedGame( szReservationPayload );
		if ( bResult && szReservationPayload && ( szReservationPayload[0] != 'R' ) )
		{
			ServerCommand( "removeallids\n" );
		}
		return bResult;
	}

	bool GetEngineHltvInfo( CEngineHltvInfo_t &info )
	{
		Q_memset( &info, 0, sizeof( info ) );
		CActiveHltvServerIterator hltv;
		if ( !hltv )
			return false; // broadcast is not active, no active GOTV[] instances

		info.m_bBroadcastActive = true;
		info.m_bMasterProxy = !hltv->IsDemoPlayback() && hltv->IsMasterProxy();
		
		if ( info.m_bMasterProxy )
			info.m_flDelay = hltv->GetDirector()->GetDelay();

		info.m_nTvPort = hltv->GetUDPPort();
		info.m_flTime = hltv->GetTime();

		hltv->GetGlobalStats( info.m_numProxies, info.m_numSlots, info.m_numClients );
		hltv->GetLocalStats( info.m_numLocalProxies, info.m_numLocalSlots, info.m_numLocalClients );
		hltv->GetRelayStats( info.m_numRelayProxies, info.m_numRelaySlots, info.m_numRelayClients );
		hltv->GetExternalStats( info.m_numExternalTotalViewers, info.m_numExternalLinkedViewers );

		const netadr_t *pRelayAdr = info.m_bMasterProxy ? NULL : hltv->GetRelayAddress();
		if ( pRelayAdr )
		{
			info.m_relayAddress = pRelayAdr->GetIPHostByteOrder();
			info.m_relayPort = pRelayAdr->GetPort();
		}
		else
		{
			info.m_relayAddress = 0;
			info.m_relayPort = 0;
		}

		// while ( hltv.Next() )
		// {
		// 	// ... reduce the information from multiple broadcasts here?
		// }

		return true;
	}

	// Add HLTV proxy whitelist to bypass password and Steam Auth checks upon connection, as CIDR a.b.c.d/numbits
	void AddHltvRelayProxyWhitelist( uint32 a, uint32 b, uint32 c, uint32 d, uint32 numbits )
	{
		HltvRelayProxyWhitelistMask_t mask;
		Q_memset( &mask, 0, sizeof( mask ) );
		mask.a = a;
		mask.b = b;
		mask.c = c;
		mask.d = d;
		mask.numbits = numbits;
		// Add a mask if it hasn't been added yet
		if ( s_arrHltvRelayProxyWhitelist.Find( mask ) == s_arrHltvRelayProxyWhitelist.InvalidIndex() )
			s_arrHltvRelayProxyWhitelist.AddToTail( mask );
	}

	// On master HLTV this call updates number of external viewers and which portion of those are linked with Steam
	void UpdateHltvExternalViewers( uint32 numTotalViewers, uint32 numLinkedViewers )
	{
		for ( CHltvServerIterator hltv; hltv; hltv.Next() )
		{
			hltv->UpdateHltvExternalViewers( numTotalViewers, numLinkedViewers );
		}
	}

	void SetDedicatedServerBenchmarkMode( bool bBenchmarkMode )
	{
		g_bDedicatedServerBenchmarkMode = bBenchmarkMode;
		if ( bBenchmarkMode )
		{
			extern ConVar sv_stressbots;
			sv_stressbots.SetValue( (int)1 );
		}
	}

	// Returns the SteamID of the game server
	const CSteamID	*GetGameServerSteamID()
	{
		if ( !Steam3Server().GetGSSteamID().IsValid() )
			return NULL;

		return &Steam3Server().GetGSSteamID();
	}


	int	GetNumSplitScreenUsersAttachedToEdict( int ent_num )
	{
		if (ent_num < 1 || ent_num > sv.GetClientCount() )
			return 0;

		CGameClient *client = sv.Client(ent_num-1);
		if ( client->IsSplitScreenUser() )
			return 0;

		int c = 0;
		for ( int i = 1; i < host_state.max_splitscreen_players; ++i )
		{
			if ( client->m_SplitScreenUsers[ i ] )
				++c;
		}

		return c;
	}
	
	edict_t *GetSplitScreenPlayerForEdict( int ent_num, int nSlot )
	{
		if (ent_num < 1 || ent_num > sv.GetClientCount() )
			return NULL;

		CGameClient *client = sv.Client(ent_num-1);
		if ( client->IsSplitScreenUser() )
			return NULL;

		if ( nSlot <= 0 || nSlot >= host_state.max_splitscreen_players )
			return NULL;

		CBaseClient *cl = client->m_SplitScreenUsers[ nSlot ];
		if ( !cl )
			return NULL;

		return (( CGameClient * )cl)->edict;
	}

	virtual int GetClusterCount()
	{
		CCollisionBSPData *pBSPData = GetCollisionBSPData();
		if ( pBSPData && pBSPData->map_vis )
			return pBSPData->map_vis->numclusters;
		return 0;
	}

	virtual int GetAllClusterBounds( bbox_t *pBBoxList, int maxBBox )
	{
		CCollisionBSPData *pBSPData = GetCollisionBSPData();
		if ( pBSPData && pBSPData->map_vis && host_state.worldbrush )
		{
			// clamp to max clusters in the map
			if ( maxBBox > pBSPData->map_vis->numclusters )
			{
				maxBBox = pBSPData->map_vis->numclusters;
			}
			// reset all of the bboxes
			for ( int i =  0; i < maxBBox; i++ )
			{
				ClearBounds( pBBoxList[i].mins, pBBoxList[i].maxs );
			}
			// add each leaf's bounds to the bounds for that cluster
			for ( int i = 0; i < host_state.worldbrush->numleafs; i++ )
			{
				mleaf_t *pLeaf = &host_state.worldbrush->leafs[i];
				// skip solid leaves and leaves with cluster < 0
				if ( !(pLeaf->contents & CONTENTS_SOLID) && pLeaf->cluster >= 0 && pLeaf->cluster < maxBBox )
				{
					Vector mins, maxs;
					mins = pLeaf->m_vecCenter - pLeaf->m_vecHalfDiagonal;
					maxs = pLeaf->m_vecCenter + pLeaf->m_vecHalfDiagonal;
					AddPointToBounds( mins, pBBoxList[pLeaf->cluster].mins, pBBoxList[pLeaf->cluster].maxs );
					AddPointToBounds( maxs, pBBoxList[pLeaf->cluster].mins, pBBoxList[pLeaf->cluster].maxs );
				}
			}

			return pBSPData->map_vis->numclusters;
		}
		return 0;
	}

	virtual bool IsCreatingReslist()
	{
		return MapReslistGenerator().IsEnabled();
	}
	virtual bool IsCreatingXboxReslist()
	{
		return MapReslistGenerator().IsCreatingForXbox();
	}

	virtual bool IsDedicatedServerForXbox()
	{
		return sv.IsDedicatedForXbox();
	}

	virtual bool IsDedicatedServerForPS3( void )
	{
		return sv.IsDedicatedForPS3();
	}

	virtual void Pause( bool bPause, bool bForce )
	{
		ConVarRef sv_pausable( "sv_pausable" );
		int oldValue = sv_pausable.GetInt();
		if ( bForce && !oldValue )
		{
			sv_pausable.SetValue( 1 );
		}

		sv.SetPaused( bPause );

		if ( bForce && !oldValue )
		{
			sv_pausable.SetValue( 0 );
		}
	}

	virtual void SetTimescale( float flTimescale )
	{
		sv.SetTimescale( flTimescale );
	}

	virtual bool HasPaintmap()
	{
		return g_PaintManager.m_bShouldRegister;
	}

	virtual bool SpherePaintSurface( const model_t *pModel, const Vector& vPosition, BYTE colorIndex, float flSphereRadius, float flPaintCoatPercent )
	{
		return ShootPaintSphere( pModel, vPosition, colorIndex, flSphereRadius, flPaintCoatPercent );
	}

	virtual void SphereTracePaintSurface( const model_t *pModel, const Vector& vPosition, const Vector& vContactNormal, float flSphereRadius, CUtlVector<BYTE>& surfColors )
	{
		TracePaintSphere( pModel, vPosition, vContactNormal, flSphereRadius, surfColors );
	}
	
	virtual void RemoveAllPaint()
	{
		g_PaintManager.RemoveAllPaint();
	}

	virtual void PaintAllSurfaces( BYTE color )
	{
		g_PaintManager.PaintAllSurfaces( color );
	}

	virtual void GetPaintmapDataRLE( CUtlVector<uint32> &data )
	{
		g_PaintManager.GetPaintmapDataRLE( data );
	}

	virtual void LoadPaintmapDataRLE( const CUtlVector<uint32> &data )
	{
		g_PaintManager.LoadPaintmapDataRLE( data );
	}

	virtual void RemovePaint( const model_t* pModel )
	{
		g_PaintManager.RemovePaint( pModel );
	}

	virtual void SendPaintmapDataToClient( edict_t *pPlayerEdict )
	{
		int entnum = NUM_FOR_EDICT( pPlayerEdict );
		if (entnum < 1 || entnum > sv.GetClientCount() )
			return;

		// Entity numbers are offset by 1 from the player numbers
		CGameClient *client = sv.Client(entnum-1);
		if ( !client )
			return;

		CUtlVector< uint32 > data;
		g_PaintManager.GetPaintmapDataRLE( data );
		
		AssertMsg2( data.Count() < NET_MAX_PAYLOAD, "Sending paint data with size [%d bytes] to client, max payload is [%d bytes]\n", data.Count(), NET_MAX_PAYLOAD );

		if ( data.Count() > 0 )
		{
			//handle endian issue between platforms
			CByteswap swap;
			swap.ActivateByteSwapping( !CByteswap::IsMachineBigEndian() );
			swap.SwapBufferToTargetEndian( data.Base(), data.Base(), data.Count() );

			CSVCMsg_PaintmapData_t svcPaintmap;
			int nBytes = data.Count() * sizeof( data.Base()[0] );
			svcPaintmap.set_paintmap( (void*)data.Base(), nBytes );
			
			client->SendNetMsg( svcPaintmap, true );
		}
	}


	// Returns the SteamID of the specified player. It'll be NULL if the player hasn't authenticated yet.
	const CSteamID	*GetClientSteamID( const edict_t *pPlayerEdict, bool bRequireFullyAuthenticated )
	{
		int entnum = NUM_FOR_EDICT( pPlayerEdict );
		if (entnum < 1 || entnum > sv.GetClientCount() )
			return NULL;

		// Entity numbers are offset by 1 from the player numbers
		CGameClient *client = sv.Client(entnum-1);
		if ( !client )
			return NULL;

		if ( !client->m_SteamID.IsValid() )
			return NULL;

		if ( bRequireFullyAuthenticated && !client->IsFullyAuthenticated() )
			return NULL;

		return &client->m_SteamID;
	}

	// Returns the XUID of the specified player. It'll be NULL if the player hasn't connected yet.
	XUID GetClientXUID( edict_t *pPlayerEdict )
	{
		int entnum = NUM_FOR_EDICT( pPlayerEdict );
		if (entnum < 1 || entnum > sv.GetClientCount() )
			return 0ull;

		// Entity numbers are offset by 1 from the player numbers
		CGameClient *client = sv.Client(entnum-1);
		if ( !client )
			return 0ull;

		return client->GetClientXuid();
	}

	void SetGamestatsData( CGamestatsData *pGamestatsData )
	{
		g_pGamestatsData = pGamestatsData;
	}

	CGamestatsData *GetGamestatsData()
	{
		return g_pGamestatsData;
	}

	void HostValidateSession()
	{
		extern void HostValidateSessionImpl();
		HostValidateSessionImpl();
	}

	void RefreshScreenIfNecessary()
	{
		::RefreshScreenIfNecessary();
	}

	float GetLatencyForChoreoSounds();

	int GetServerVersion() const
	{
		return ::GetServerVersion();
	}

	bool WasShutDownRequested() const
	{
		extern bool sv_ShutDown_WasRequested();
		return sv_ShutDown_WasRequested();
	}

private:
	
	// Purpose: Sends a temp entity to the client ( follows the format of the original MESSAGE_BEGIN stuff from HL1
	virtual void PlaybackTempEntity( IRecipientFilter& filter, float delay, const void *pSender, const SendTable *pST, int classID  );
	virtual int	CheckAreasConnected( int area1, int area2 );
	virtual int GetArea( const Vector& origin );
	virtual void GetAreaBits( int area, unsigned char *bits, int buflen );
	virtual bool GetAreaPortalPlane( Vector const &vViewOrigin, int portalKey, VPlane *pPlane );
	virtual client_textmessage_t *TextMessageGet( const char *pName );
	virtual void LogPrint(const char * msg);
	virtual bool IsLogEnabled();
	virtual bool LoadGameState( char const *pMapName, bool createPlayers );
	virtual bool IsOverrideLoadGameEntsOn();
	virtual void ForceFlushEntity( int iEntity );
	virtual void LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName );
	virtual void ClearSaveDir();
	virtual void ClearSaveDirAfterClientLoad();

	virtual const char* GetMapEntitiesString();
	virtual void BuildEntityClusterList( edict_t *pEdict, PVSInfo_t *pPVSInfo );
	virtual void CleanUpEntityClusterList( PVSInfo_t *pPVSInfo );
	virtual void SolidMoved( edict_t *pSolidEnt, ICollideable *pSolidCollide, const Vector* pPrevAbsOrigin, bool accurateBboxTriggerChecks );
	virtual void TriggerMoved( edict_t *pTriggerEnt, bool accurateBboxTriggerChecks );

	virtual ISpatialPartition *CreateSpatialPartition( const Vector& worldmin, const Vector& worldmax ) { return ::CreateSpatialPartition( worldmin, worldmax );	}
	virtual void 		DestroySpatialPartition( ISpatialPartition *pPartition )						{ ::DestroySpatialPartition( pPartition );					}

	public:

	virtual bool IsActiveApp()
	{
		return game->IsActiveApp();
	}

	virtual void SetNoClipEnabled( bool bEnabled )
	{
		extern bool g_bNoClipEnabled;
		g_bNoClipEnabled = bEnabled;
	}

	virtual bool StartClientHltvReplay( int nClientIndex, const HltvReplayParams_t &params ) OVERRIDE
	{
		if ( IClient *pClient = sv.GetClient( nClientIndex ) )
		{
			return pClient->StartHltvReplay( params );
		}
		return false;
	}

	virtual void StopClientHltvReplay( int nClientIndex ) OVERRIDE
	{
		if ( IClient *pClient = sv.GetClient( nClientIndex ) )
		{
			pClient->StopHltvReplay();
		}
	}

	virtual int GetClientHltvReplayDelay( int nClientIndex ) OVERRIDE
	{
		if ( IClient *pClient = sv.GetClient( nClientIndex ) )
		{
			return pClient->GetHltvReplayDelay();
		}
		return 0;
	}
	
	virtual bool ClientCanStartHltvReplay( int nClientIndex ) OVERRIDE
	{
		if ( IClient* pClient = sv.GetClient( nClientIndex ) )
		{
			return pClient->CanStartHltvReplay();
		}
		return false;
	}

	virtual bool HasHltvReplay( ) OVERRIDE
	{
		CActiveHltvServerIterator hltv;

		return hltv && hltv->GetOldestTick() > 0;
	}

	virtual void ClientResetReplayRequestTime( int nClientIndex ) OVERRIDE
	{
		if ( IClient* pClient = sv.GetClient( nClientIndex ) )
		{
			return pClient->ResetReplayRequestTime();
		}
	}

	virtual bool AnyClientsInHltvReplayMode() OVERRIDE
	{
		return sv.AnyClientsInHltvReplayMode();
	}
};

//-----------------------------------------------------------------------------
// Expose CVEngineServer to the game DLL.
//-----------------------------------------------------------------------------
static CVEngineServer	g_VEngineServer;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CVEngineServer, IVEngineServer, INTERFACEVERSION_VENGINESERVER, g_VEngineServer);

//-----------------------------------------------------------------------------
// Expose CVEngineServer to the engine.
//-----------------------------------------------------------------------------
IVEngineServer *g_pVEngineServer = &g_VEngineServer;


//-----------------------------------------------------------------------------
// Used to allocate pvs infos
//-----------------------------------------------------------------------------
static CUtlMemoryPool s_PVSInfoAllocator( 128, 128 * 64, CUtlMemoryPool::GROW_SLOW, "pvsinfopool", 128 );

//-----------------------------------------------------------------------------
// Purpose: Sends a temp entity to the client, using the reliable data stream
// Input  : delay - 
//			*pSender - 
//			*pST - 
//			classID - 
//-----------------------------------------------------------------------------
static void WriteReliableEvent( const SendTable *pST, float delay, int classID, SerializedEntityHandle_t handle, IClient *client, bf_write *buf )
{
	CSVCMsg_TempEntities_t eventMsg;

	eventMsg.set_reliable( true );

	// special case 0 signals single reliable event
	eventMsg.set_num_entries( 0 );
	
	eventMsg.mutable_entity_data()->resize( CEventInfo::MAX_EVENT_DATA );
	bf_write buffer( &(*eventMsg.mutable_entity_data())[0], eventMsg.entity_data().size() );

	if ( delay == 0.0f )
	{
		buffer.WriteOneBit( 0 ); // no delay
	} 
	else
	{
		buffer.WriteOneBit( 1 );
		buffer.WriteUBitLong( delay*100.0f, 16 );
	}

	buffer.WriteOneBit( 1 ); // full update

	buffer.WriteUBitLong( classID, sv.serverclassbits ); // classID 

	// write event properties
	SendTable_WritePropList( pST, handle, &buffer, -1, NULL );

	// write message
	if ( client )
	{
		client->SendNetMsg( eventMsg, true );
	}

	if ( buf )
	{
		eventMsg.WriteToBuffer( *buf );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sends a temp entity to the client ( follows the format of the original MESSAGE_BEGIN stuff from HL1
// Input  : msg_dest - 
//			delay - 
//			*origin - 
//			*recipient - 
//			*pSender - 
//			*pST - 
//			classID - 
//-----------------------------------------------------------------------------
void CVEngineServer::PlaybackTempEntity( IRecipientFilter& filter, float delay, const void *pSender, const SendTable *pST, int classID  )
{
	VPROF( "PlaybackTempEntity" );

	// don't add more events to a snapshot than a client can receive
	if ( sv.m_TempEntities.Count() >= ((1<<CEventInfo::EVENT_INDEX_BITS)-1) )
	{
		// remove oldest effect
		delete sv.m_TempEntities[0]; 
		sv.m_TempEntities.Remove( 0 );
	}
	
	// Make this start at 1
	classID = classID + 1;

	SerializedEntityHandle_t handle = g_pSerializedEntities->AllocateSerializedEntity(__FILE__, __LINE__);

	// write all properties, if init or reliable message delta against zero values
	if( !SendTable_Encode( pST, handle, pSender, classID, NULL ) )
	{
		Host_Error( "PlaybackTempEntity: SendTable_Encode returned false (ent %d), overflow?\n", classID );
		return;
	}
	
	bool bSendReliable = filter.IsReliable();

	// Reliable events are sent one by one to each recipient
	// Unreliable events are queued below and added to delta update packet if there is space...
	if ( bSendReliable )
	{
		int c = filter.GetRecipientCount();
		for ( int slot = 0; slot < c; slot++ )
		{
			int index = filter.GetRecipientIndex( slot );
			if ( index < 1 || index > sv.GetClientCount() )
				continue;
			CGameClient *cl = sv.Client( index - 1 );
			if ( ( cl->IsFakeClient() && !cl->IsHLTV() ) || !cl->IsActive() )
				continue;

			WriteReliableEvent( pST, delay, classID, handle, cl, NULL );
		}

		g_pSerializedEntities->ReleaseSerializedEntity( handle );
		return;
	}

	// create CEventInfo:
	CEventInfo *newEvent = new CEventInfo;

	//copy client filter
	newEvent->filter.AddPlayersFromFilter( &filter );

	newEvent->classID	= classID;
	newEvent->pSendTable= pST;
	newEvent->fire_delay= delay;

	newEvent->m_Packed = handle;

	// add to list
	sv.m_TempEntities[sv.m_TempEntities.AddToTail()] = newEvent;
}

int	CVEngineServer::CheckAreasConnected( int area1, int area2 )
{
	return CM_AreasConnected(area1, area2);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *origin - 
//			*bits - 
// Output : void
//-----------------------------------------------------------------------------
int CVEngineServer::GetArea( const Vector& origin )
{
	return CM_LeafArea( CM_PointLeafnum( origin ) );
}

void CVEngineServer::GetAreaBits( int area, unsigned char *bits, int buflen )
{
	CM_WriteAreaBits( bits, buflen, area );
}

bool CVEngineServer::GetAreaPortalPlane( Vector const &vViewOrigin, int portalKey, VPlane *pPlane )
{
	return CM_GetAreaPortalPlane( vViewOrigin, portalKey, pPlane );
}

client_textmessage_t *CVEngineServer::TextMessageGet( const char *pName )
{
	return ::TextMessageGet( pName );
}

void CVEngineServer::LogPrint(const char * msg)
{
	g_Log.Print( msg );
}
bool CVEngineServer::IsLogEnabled()
{
	return g_Log.IsActive();
}

// HACKHACK: Save/restore wrapper - Move this to a different interface
bool CVEngineServer::LoadGameState( char const *pMapName, bool createPlayers )
{
#ifndef DEDICATED
	return saverestore->LoadGameState( pMapName, createPlayers ) != 0;
#else
	return 0;
#endif
}

bool CVEngineServer::IsOverrideLoadGameEntsOn()
{
#ifndef DEDICATED
	return saverestore->IsOverrideLoadGameEntsOn();
#else
	return false;
#endif
}

void CVEngineServer::ForceFlushEntity( int iEntity )
{
	if ( g_pLocalNetworkBackdoor )
	{
		g_pLocalNetworkBackdoor->ForceFlushEntity( iEntity );
	}
}

void CVEngineServer::LoadAdjacentEnts( const char *pOldLevel, const char *pLandmarkName )
{
#ifndef DEDICATED
	saverestore->LoadAdjacentEnts( pOldLevel, pLandmarkName );
#endif
}

void CVEngineServer::ClearSaveDir()
{
#ifndef DEDICATED
	saverestore->ClearSaveDir();
#endif
}

void CVEngineServer::ClearSaveDirAfterClientLoad()
{
#ifndef DEDICATED
	saverestore->RequestClearSaveDir();
#endif
}


const char* CVEngineServer::GetMapEntitiesString()
{
	return CM_EntityString();
}

//-----------------------------------------------------------------------------
// Builds PVS information for an entity
//-----------------------------------------------------------------------------
inline bool SortClusterLessFunc( const int &left, const int &right )
{
	return left < right;
}

void CVEngineServer::BuildEntityClusterList( edict_t *pEdict, PVSInfo_t *pPVSInfo )
{
	int		i, j;
	int		topnode;
	int		leafCount;
	int		leafs[MAX_TOTAL_ENT_LEAFS], clusters[MAX_TOTAL_ENT_LEAFS];
	int		area;

	CleanUpEntityClusterList( pPVSInfo );
	pPVSInfo->m_pClusters = 0;
	pPVSInfo->m_nClusterCount = 0;
	pPVSInfo->m_nAreaNum = 0;
	pPVSInfo->m_nAreaNum2 = 0;
	if ( !pEdict )
		return;

	ICollideable *pCollideable = pEdict->GetCollideable();
	Assert( pCollideable );
	if ( !pCollideable )
		return;

	topnode = -1;

	//get all leafs, including solids
	Vector vecWorldMins, vecWorldMaxs;
	pCollideable->WorldSpaceSurroundingBounds( &vecWorldMins, &vecWorldMaxs );
	leafCount = CM_BoxLeafnums( vecWorldMins, vecWorldMaxs, leafs, MAX_TOTAL_ENT_LEAFS, &topnode );

	// set areas
	bool bAreaCheck = false;
	for ( i = 0; i < leafCount; i++ )
	{
		clusters[i] = CM_LeafCluster( leafs[i] );
		area = CM_LeafArea( leafs[i] );
		if ( area == 0 )
			continue;

		// doors may legally straggle two areas,
		// but nothing should ever need more than that
		if ( pPVSInfo->m_nAreaNum && pPVSInfo->m_nAreaNum != area )
		{
			if ( !bAreaCheck && pPVSInfo->m_nAreaNum2 && pPVSInfo->m_nAreaNum2 != area )
			{
				// if you are touching more than 2 areas, do a check to get the approximate best area
				bAreaCheck = true;
				if ( sv.IsLoading() )
				{
					ConDMsg ("Object touching 3 areas at %f %f %f\n", vecWorldMins[0], vecWorldMins[1], vecWorldMins[2]);
				}
			}
			pPVSInfo->m_nAreaNum2 = area;
		}
		else
		{
			pPVSInfo->m_nAreaNum = area;
		}
	}

	Vector center = (vecWorldMins+vecWorldMaxs) * 0.5f; // calc center

	if ( bAreaCheck )
	{
		// make sure the area of your center is being tested, otherwise just pick the first ones 
		// in the list
		int leaf = CM_PointLeafnum( center );
		int area = CM_LeafArea(leaf);
		if ( pPVSInfo->m_nAreaNum != area && pPVSInfo->m_nAreaNum2 != area )
		{
			pPVSInfo->m_nAreaNum = area;
		}
	}
	pPVSInfo->m_nHeadNode = topnode;	// save headnode

	// save origin
	pPVSInfo->m_vCenter[0] = center[0];
	pPVSInfo->m_vCenter[1] = center[1];
	pPVSInfo->m_vCenter[2] = center[2];

	if ( leafCount >= MAX_TOTAL_ENT_LEAFS )
	{
		// assume we missed some leafs, and mark by headnode
		pPVSInfo->m_nClusterCount = -1;
		return;
	}

	pPVSInfo->m_pClusters = pPVSInfo->m_pClustersInline;
	if ( leafCount >= 16 )
	{
		std::make_heap( clusters, clusters + leafCount, SortClusterLessFunc ); 
		std::sort_heap( clusters, clusters + leafCount, SortClusterLessFunc ); 
		for ( i = 0; i < leafCount; i++ )
		{
			if ( clusters[i] == -1 )
				continue;		// not a visible leaf

			if ( ( i > 0 ) && ( clusters[i] == clusters[i-1] ) )
				continue;

			if ( pPVSInfo->m_nClusterCount == MAX_FAST_ENT_CLUSTERS )
			{
				unsigned short *pClusters = (unsigned short *)s_PVSInfoAllocator.Alloc();
				memcpy( pClusters, pPVSInfo->m_pClusters, MAX_FAST_ENT_CLUSTERS * sizeof(unsigned short) );
				pPVSInfo->m_pClusters = pClusters;
			}
			else if ( pPVSInfo->m_nClusterCount == MAX_ENT_CLUSTERS )
			{
				// assume we missed some leafs, and mark by headnode
				s_PVSInfoAllocator.Free( pPVSInfo->m_pClusters );
				pPVSInfo->m_pClusters = 0;
				pPVSInfo->m_nClusterCount = -1;
				break;
			}

			pPVSInfo->m_pClusters[pPVSInfo->m_nClusterCount++] = (short)clusters[i];
		}
		return;
	}

	for ( i = 0; i < leafCount; i++ )
	{
		if ( clusters[i] == -1 )
			continue;		// not a visible leaf

		for ( j = 0; j < i; j++ )
		{
			if ( clusters[j] == clusters[i] )
				break;
		}

		if ( j != i )
			continue;

		if ( pPVSInfo->m_nClusterCount == MAX_FAST_ENT_CLUSTERS )
		{
			unsigned short *pClusters = (unsigned short*)s_PVSInfoAllocator.Alloc();
			memcpy( pClusters, pPVSInfo->m_pClusters, MAX_FAST_ENT_CLUSTERS * sizeof(unsigned short) );
			pPVSInfo->m_pClusters = pClusters;
		}
		else if ( pPVSInfo->m_nClusterCount == MAX_ENT_CLUSTERS )
		{
			// assume we missed some leafs, and mark by headnode
			s_PVSInfoAllocator.Free( pPVSInfo->m_pClusters );
			pPVSInfo->m_pClusters = 0;
			pPVSInfo->m_nClusterCount = -1;
			break;
		}

		pPVSInfo->m_pClusters[pPVSInfo->m_nClusterCount++] = (short)clusters[i];
	}
}


//-----------------------------------------------------------------------------
// Cleans up the cluster list
//-----------------------------------------------------------------------------
void CVEngineServer::CleanUpEntityClusterList( PVSInfo_t *pPVSInfo )
{
	if ( pPVSInfo->m_nClusterCount > MAX_FAST_ENT_CLUSTERS )
	{
		s_PVSInfoAllocator.Free( pPVSInfo->m_pClusters );
		pPVSInfo->m_pClusters = 0;
		pPVSInfo->m_nClusterCount = 0;
	}
}


//-----------------------------------------------------------------------------
// Adds a handle to the list of entities to update when a partition query occurs
//-----------------------------------------------------------------------------
void CVEngineServer::SolidMoved( edict_t *pSolidEnt, ICollideable *pSolidCollide, const Vector* pPrevAbsOrigin, bool accurateBboxTriggerChecks )
{
	SV_SolidMoved( pSolidEnt, pSolidCollide, pPrevAbsOrigin, accurateBboxTriggerChecks );
}

void CVEngineServer::TriggerMoved( edict_t *pTriggerEnt, bool accurateBboxTriggerChecks )
{
	SV_TriggerMoved( pTriggerEnt, accurateBboxTriggerChecks );
}


//-----------------------------------------------------------------------------
// Called by the server to determine violence settings.
//-----------------------------------------------------------------------------
bool CVEngineServer::IsLowViolence()
{
	return g_bLowViolence;
}

//-----------------------------------------------------------------------------
// Called by the (multiplayer) server to determine violence settings.
//-----------------------------------------------------------------------------
bool CVEngineServer::IsAnyClientLowViolence()
{
	for ( int i=0; i<sv.GetClientCount(); ++i )
	{
		CGameClient *client = sv.Client(i);
		if ( client && client->IsLowViolenceClient() )
		{
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Called by server to delay choreo sounds accordingly to IO latency.
//-----------------------------------------------------------------------------
float CVEngineServer::GetLatencyForChoreoSounds()
{
#ifdef DEDICATED
  return 0.0f;
#elif LINUX
  return 0.0f;
#else
	extern ConVar	snd_mixahead;
	extern ConVar	snd_delay_for_choreo_enabled;
	extern ConVar	snd_delay_for_choreo_reset_after_N_milliseconds;
	extern float	g_fDelayForChoreo;
	extern uint32	g_nDelayForChoreoLastCheckInMs;
	extern int		g_nDelayForChoreoNumberOfSoundsPlaying;

	float fResult = snd_mixahead.GetFloat();
	if ( snd_delay_for_choreo_enabled.GetBool() )
	{
		float fDelayForChoreo = g_fDelayForChoreo;
		if ( fDelayForChoreo != 0.0f )
		{
			// Let's see if we have to reset the delay due to choreo (do this just before we return any useful information from scene entity).
			// Note that this access is not thread safe, however there is no dire consequence here in case of race conditions.
			// Delay may be reset when it should not, or delay may not be reset when it should (that case would be corrected by a subsequent call anyway).
			if ( g_nDelayForChoreoNumberOfSoundsPlaying == 0 )
			{
				// We only reset the delay if no other sound is still behind in term of latency. Several VCDs could be running in parallel.
				// As if we do it later, like when the samples are ready, we are going to hit the timeout more easily. We would then lose previously accumulated delay,
				// and the sound could be potentially cut off later.
				uint32 nCurrentTime = Plat_MSTime();
				uint32 nLastCheck = g_nDelayForChoreoLastCheckInMs;
				if ( nLastCheck + snd_delay_for_choreo_reset_after_N_milliseconds.GetInt() < nCurrentTime )
				{
					// Msg( "Reset delay for choreo as no choreo has been executed for the past %f seconds. Old value=%f.\n", (float)( nCurrentTime - nLastCheck ) / 1000.0f, fDelayForChoreo );
					g_fDelayForChoreo = fDelayForChoreo = 0.0f;
				}
			}
		}

		// Remove the delay to the mix-ahead, which is going to push back the latency.
		fResult -= fDelayForChoreo;
	}
	return fResult;
#endif
}

