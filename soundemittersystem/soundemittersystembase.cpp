//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//


#include <keyvalues.h>
#include "filesystem.h"
#include "utldict.h"
#include "tier2/interval.h"
#include "engine/IEngineSound.h"
#include "soundemittersystembase.h"
#include "utlbuffer.h"
#include "soundchars.h"
#include "vstdlib/random.h"
#include "checksum_crc.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "tier2/tier2.h"
#include "datacache/iresourceaccesscontrol.h"
#include "checksum_crc.h"
#include "tier1/generichash.h"

#if IsPlatformX360()
#include "filesystem/IXboxInstaller.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MANIFEST_FILE				"scripts/game_sounds_manifest.txt"
#define GAME_SOUNDS_HEADER_BLOCK	"scripts/game_sounds_header.txt"

BEGIN_DEFINE_LOGGING_CHANNEL( LOG_SOUNDEMITTER_SYSTEM, "SoundEmitterSystem", LCF_CONSOLE_ONLY, LS_MESSAGE );
END_DEFINE_LOGGING_CHANNEL();

//-----------------------------------------------------------------------------

#define MAX_MEASURED_SOUNDENTRIES 6124

// Allocate sound entries in 64k blocks
DEFINE_FIXEDSIZE_ALLOCATOR( CSoundEntry, 64*1024 / sizeof( CSoundEntry ), CUtlMemoryPool::GROW_SLOW );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSoundEmitterSystemBase::CSoundEmitterSystemBase() : 
	m_ActorGenders( true, 0, 0 ),  // Case insensitive
	m_nInitCount( 0 ),
	m_uManifestPlusScriptChecksum( 0 ),
	m_HashToSoundEntry( 0, 0, DefLessFunc( unsigned int ) )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int	
//-----------------------------------------------------------------------------
int	 CSoundEmitterSystemBase::First() const
{
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : i - 
// Output : int
//-----------------------------------------------------------------------------
int CSoundEmitterSystemBase::Next( int i ) const
{
	if ( ++i >= m_Sounds.Count() )
	{
		return m_Sounds.InvalidIndex();
	}
	return i;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CSoundEmitterSystemBase::InvalidIndex() const
{
	return m_Sounds.InvalidIndex();
}

//-----------------------------------------------------------------------------
//
// implementation of IUniformRandomStream
//
//-----------------------------------------------------------------------------
class CSoundEmitterUniformRandomStream : public IUniformRandomStream
{
public:
	// Sets the seed of the random number generator
	void	SetSeed( int iSeed )
	{
		// Never call this from the client or game!
		Assert(0);
	}

	// Generates random numbers
	float	RandomFloat( float flMinVal = 0.0f, float flMaxVal = 1.0f )
	{
		return ::RandomFloat( flMinVal, flMaxVal );
	}

	int		RandomInt( int iMinVal, int iMaxVal )
	{
		return ::RandomInt( iMinVal, iMaxVal );
	}

	float	RandomFloatExp( float flMinVal = 0.0f, float flMaxVal = 1.0f, float flExponent = 1.0f )
	{
		return ::RandomFloatExp( flMinVal, flMaxVal, flExponent );
	}

};

static CSoundEmitterUniformRandomStream g_RandomStream;
IUniformRandomStream *randomStream = &g_RandomStream;

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CSoundEmitterSystemBase::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	if ( !g_pFullFileSystem )
	{
		Error( "The soundemittersystem system requires the filesystem to run!\n" );
		return false;
	}

	return true;
}


void CSoundEmitterSystemBase::Disconnect()
{
	BaseClass::Disconnect();
}


//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CSoundEmitterSystemBase::QueryInterface( const char *pInterfaceName )
{
	// Loading the engine DLL mounts *all* soundemitter interfaces
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, NULL );				// to prevent the LTCG compiler from crashing.
}


//-----------------------------------------------------------------------------
// Purpose: Helper for checksuming script files and manifest to determine if soundname caches
//  need to be blown away.
// Input  : *crc - 
//			*filename - 
// Output : static void
//-----------------------------------------------------------------------------
static void AccumulateFileNameAndTimestampIntoChecksum( CRC32_t *crc, char const *filename )
{
	if ( IsX360() )
	{
		// this is an expensive i/o operation due to search path fall through
		// 360 doesn't need or use the checksums
		return;
	}

	long ft = g_pFullFileSystem->GetFileTime( filename, "GAME" );
	CRC32_ProcessBuffer( crc, &ft, sizeof( ft ) );
	CRC32_ProcessBuffer( crc, filename, Q_strlen( filename ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
InitReturnVal_t CSoundEmitterSystemBase::Init()
{
	++m_nInitCount;
	if ( m_nInitCount > 1 )
		return INIT_OK;

	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	bool bLoaded = LoadGameSoundManifest();
	return bLoaded ? INIT_OK : INIT_FAILED;
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::Shutdown()
{
	if ( --m_nInitCount > 0 )
		return;

	ShutdownSounds();
	BaseClass::Shutdown();
}


bool CSoundEmitterSystemBase::LoadGameSoundManifest()
{
	/*
	if ( m_SoundKeyValues.Count() > 0 )
	{
		Shutdown();
	}
	*/

	LoadGlobalActors();

	m_uManifestPlusScriptChecksum = 0u;

	CRC32_t crc;
	CRC32_Init( &crc );

#if 0
	AccumulateFileNameAndTimestampIntoChecksum( &crc, "scripts/game_sounds_music/game_sounds_music_deathcams.txt" );
	AddSoundsFromFile( "scripts/game_sounds_music/game_sounds_music_deathcams.txt", true, true );
#endif


	KeyValues *manifest = new KeyValues( MANIFEST_FILE );
	if ( g_pFullFileSystem->LoadKeyValues( *manifest, IFileSystem::TYPE_SOUNDEMITTER, MANIFEST_FILE, "GAME" ) )
	{
		AccumulateFileNameAndTimestampIntoChecksum( &crc, MANIFEST_FILE );

		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
		{
			if ( !Q_stricmp( sub->GetName(), "precache_file" ) )
			{
				AccumulateFileNameAndTimestampIntoChecksum( &crc, sub->GetString() );

				// Add and always precache
				AddSoundsFromFile( sub->GetString(), false, false );
				continue;
			}
			if ( !Q_stricmp( sub->GetName(), "autocache_file" ) )
			{
				AccumulateFileNameAndTimestampIntoChecksum( &crc, sub->GetString() );

				// Add and always precache and autocache
				AddSoundsFromFile( sub->GetString(), false, true );
				continue;
			}
			else if ( !Q_stricmp( sub->GetName(), "preload_file" ) )
			{
				AccumulateFileNameAndTimestampIntoChecksum( &crc, sub->GetString() );

				// Add and always precache
				AddSoundsFromFile( sub->GetString(), true, false );
				continue;
			}

			Warning( "CSoundEmitterSystemBase::BaseInit:  Manifest '%s' with bogus file type '%s', expecting 'declare_file' or 'precache_file'\n", 
				MANIFEST_FILE, sub->GetName() );
		}
	}
	else
	{
		if( IsPS3() )
		{
			return false;
		}
		else
		{
			Error( "Unable to load manifest file '%s'\n", MANIFEST_FILE );
		}		
	}
	manifest->deleteThis();

	CRC32_Final( &crc );

	m_uManifestPlusScriptChecksum = ( unsigned int )crc;

	// Only print total once, on server
#if !defined( CLIENT_DLL ) && !defined( FACEPOSER )
	DevMsg( 1, "CSoundEmitterSystem:  Registered %i sounds\n", m_Sounds.Count() );
#endif

	// Helpful code to dump out sound entry lists if we suspect this of being out-of-sync with RTM
	//if ( 0 )
	//{
	//	FileHandle_t hSndDumpFile = NULL;

	//	hSndDumpFile = g_pFullFileSystem->Open( "sound_dump.csv", "w" );
	//	for ( int i = 0; i < m_Sounds.Count(); ++ i )
	//	{
	//		int nHash = HashSoundName( m_Sounds[ i ]->m_Name.String() );
	//		int nSlot = m_HashToSoundIndex.Find( nHash );
	//		nSlot;
	//		Assert( nSlot != m_HashToSoundIndex.InvalidIndex() );
	//		Assert( m_HashToSoundIndex[ nSlot ] == i );
	//		g_pFullFileSystem->FPrintf( hSndDumpFile, "%s,%X\n", m_Sounds[ i ]->m_Name.String(), nHash );
	//	}	

	//	g_pFullFileSystem->Close( hSndDumpFile );
	//}
	

	return true;
}


void CSoundEmitterSystemBase::ShutdownSounds()
{
	int i;
	m_SoundKeyValues.RemoveAll();
	for ( i = 0; i < m_Sounds.Count(); ++i )
	{
		delete m_Sounds[ i ];
	}
	m_Sounds.Purge();

	for ( i = 0; i < m_SavedOverrides.Count() ; ++i )
	{
		delete m_SavedOverrides[ i ];
	}
	m_SavedOverrides.Purge();
	m_Waves.RemoveAll();
	m_ActorGenders.Purge();
	m_HashToSoundEntry.Purge();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pName - 
//-----------------------------------------------------------------------------
int	CSoundEmitterSystemBase::GetSoundIndex( const char *pName ) const
{
	// Use the hash, its faster
	HSOUNDSCRIPTHASH hash = HashSoundName( pName );
	int idx = GetSoundIndexForHash( hash );
	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSoundEmitterSystemBase::IsValidIndex( int index )
{
	return m_Sounds.IsValidIndex( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
const char *CSoundEmitterSystemBase::GetSoundName( int index )
{
	if ( !IsValidIndex( index ) )
		return "";

	return m_Sounds[ index ]->m_Name.String();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CSoundEmitterSystemBase::GetSoundCount( void )
{
	return m_Sounds.Count();
}

void CSoundEmitterSystemBase::EnsureAvailableSlotsForGender( SoundFile *pSoundnames, int c, gender_t gender )
{
	int i;
	if ( c <= 0 )
	{
		return;
	}

	CUtlVector< int > slots;

	bool needsreset = false;
	for ( i = 0; i < c; i++ )
	{
		if ( pSoundnames[ i ].gender != gender )
			continue;

		// There was at least one match for the gender
		needsreset = true;

		// This sound is unavailable
		if ( !pSoundnames[ i ].available )
			continue;

		slots.AddToTail( i );
	}

	if ( slots.Count() == 0 && needsreset )
	{
		// Reset all slots for the specified gender!!!
		for ( i = 0; i < c; i++ )
		{
			if ( pSoundnames[ i ].gender != gender )
				continue;

			pSoundnames[ i ].available = true;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : gender - 
//			soundnames - 
//-----------------------------------------------------------------------------
int	CSoundEmitterSystemBase::FindBestSoundForGender( SoundFile *pSoundnames, int c, gender_t gender, int &nRandomSeed )
{
	// Check for recycling of random sounds...
	EnsureAvailableSlotsForGender( pSoundnames, c, gender );
#if 0
	Msg( "nRandomSeed(1) %i : ", nRandomSeed );
#endif

	// because this random int / index came across the network as a 6 bit uint
	// we utilize the 0 slot as "undefined", however 0 is a valid index
	// therefore the shift
	int nAdjRandomSeed = nRandomSeed - 1;

	if ( c <= 0 )
	{
		return -1;
	}

	// have we been passed a valid index
	int idx;
	if ( nAdjRandomSeed >= 0 )
	{
		// extract LSB index
		idx = nAdjRandomSeed % c;
	}
	else
	{
		// make a list of possible indices
		CUtlVector< int > slots;
		for ( int i = 0; i < c; i++ )
		{
			if ( pSoundnames[ i ].gender == gender &&
				( pSoundnames[ i ].available ) )
			{
				slots.AddToTail( i );
			}
		}

		if ( slots.Count() >= 1 )
		{

			// TODO: morasky, this should get tested at load time?
			Assert( slots.Count() < MAX_SOUND_SEED_VALUE );

			int nRandomIndex = randomStream->RandomInt( 0, slots.Count() - 1 );

			idx = slots[ nRandomIndex ];

			// create random MSB for full res seed
			int nMaxBitDiv = MAX_SOUND_SEED_VALUE / c;
			int nRandomMSB = randomStream->RandomInt( 0, nMaxBitDiv );
			int nRandomSum = ( nRandomMSB * c ) + idx;

			// we are using 0 = undefined
			nRandomSum += 1;
			if( nRandomSum > MAX_SOUND_SEED_VALUE )
			{
				nRandomSum -= c;
			}
			nRandomSeed = nRandomSum;
#if 0
			Msg( "nRandomIndex %i : nRandomMSB %i : nRandomSum %i : ", nRandomIndex, nRandomMSB, nRandomSum );
#endif
		}
		else
		{
			idx = -1;
			nRandomSeed = 0;
		}
	}

#if 0
		Msg( "nRandomSeed %i : nAdjRandomSeed %i : nRandomLSB %i : idx %i : %i\n", nRandomSeed, nAdjRandomSeed, nRandomLSB, idx );
#endif
	return idx;


// 	int idx = randomStream->RandomInt( 0, c - 1 );
// 	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
//			params - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSoundEmitterSystemBase::GetParametersForSound( const char *soundname, CSoundParameters& params, gender_t gender, bool isbeingemitted /*= false*/ )
{
	HSOUNDSCRIPTHASH hash = HashSoundName( soundname );
	int index = GetSoundIndexForHash( hash );

	if ( index == m_Sounds.InvalidIndex() )
	{
		static CUtlSymbolTable soundWarnings;
		char key[ 256 ];
		Q_snprintf( key, sizeof( key ), "%s:%s", soundname, params.soundname );
		if ( UTL_INVAL_SYMBOL == soundWarnings.Find( key ) )
		{
			soundWarnings.AddString( key );

			Warning( "CSoundEmitterSystemBase::GetParametersForSound:  No such sound %s\n", soundname );
		}
		return GetParametersForSoundEx( "Error", hash, params, gender, isbeingemitted );
	}

	return GetParametersForSoundEx( soundname, hash, params, gender, isbeingemitted );
}

CSoundParametersInternal *CSoundEmitterSystemBase::InternalGetParametersForSound( int index )
{
	if ( !m_Sounds.IsValidIndex( index ) )
	{
		Assert( !"CSoundEmitterSystemBase::InternalGetParametersForSound:  Bogus index" );
		return NULL;
	}

	return &m_Sounds[ index ]->m_SoundParams;
}

static void SplitName( char const *input, int splitchar, int splitlen, char *before, int beforelen, char *after, int afterlen )
{
	char const *in = input;
	char *out = before;

	int c = 0;
	int l = 0;
	int maxl = beforelen;
	while ( *in )
	{
		if ( c == splitchar )
		{
			while ( --splitlen >= 0 )
			{
				in++;
			}

			*out = 0;
			out = after;
			maxl = afterlen;
			c++;
			continue;
		}

		if ( l >= maxl )
		{
			in++;
			c++;
			continue;
		}

		*out++ = *in++;
		l++;
		c++;
	}

	*out = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : params - 
//			*wavename - 
//			gender - 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::AddSoundName( CSoundParametersInternal& params, char const *wavename, gender_t gender )
{
	CUtlSymbol sym = m_Waves.AddString( wavename );
	SoundFile e;
	e.symbol = sym;
	e.gender = gender;
	if ( gender != GENDER_NONE )
	{
		params.SetUsesGenderToken( true );
	}
	params.AddSoundName( e );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static const char *FindGenderMacro( const char *wavename, int *duration )
{
	char const *p = Q_stristr( wavename, SOUNDGENDER_MACRO );
	if ( p )
	{
		*duration = SOUNDGENDER_MACRO_LENGTH;
	}

	return p;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : params - 
//			*wavename - 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::ExpandSoundNameMacros( CSoundParametersInternal& params, char const *wavename )
{
	int duration = SOUNDGENDER_MACRO_LENGTH;
	const char *p = FindGenderMacro( wavename, &duration );

	if ( !p )
	{
		AddSoundName( params, wavename, GENDER_NONE );
		return;
	}

	int offset = p - wavename;
	Assert( offset >= 0 );

	// Create a "male" and "female" version of the sound
	char before[ 256 ], after[ 256 ];
	Q_memset( before, 0, sizeof( before ) );
	Q_memset( after, 0, sizeof( after ) );

	SplitName( wavename, offset, duration, before, sizeof( before ), after, sizeof( after ) );

	char temp[ 256 ];
	Q_snprintf( temp, sizeof( temp ), "%s%s%s", before, "male", after );
	AddSoundName( params, temp, GENDER_MALE );
	Q_snprintf( temp, sizeof( temp ), "%s%s%s", before, "female", after );
	AddSoundName( params, temp, GENDER_FEMALE );

	// Add the conversion entry with the gender tags still in it
	CUtlSymbol sym = m_Waves.AddString( wavename );
	SoundFile e;
	e.symbol = sym;
	e.gender = GENDER_NONE;
	params.AddConvertedName( e );
}

void CSoundEmitterSystemBase::GenderExpandString( gender_t gender, char const *in, char *out, int maxlen )
{
	// Assume the worst
	Q_strncpy( out, in, maxlen );

	int duration = SOUNDGENDER_MACRO_LENGTH;
	const char *p = FindGenderMacro( in, &duration );
	if ( !p )
	{
		return;
	}

	// Look up actor gender
	if ( gender == GENDER_NONE )
	{
		return;
	}

	int offset = p - in;
	Assert( offset >= 0 );

	// Create a "male" and "female" version of the sound
	char before[ 256 ], after[ 256 ];
	Q_memset( before, 0, sizeof( before ) );
	Q_memset( after, 0, sizeof( after ) );

	SplitName( in, offset, duration, before, sizeof( before ), after, sizeof( after ) );

	switch ( gender )
	{
	default:
	case GENDER_NONE:
		{
			Assert( !"CSoundEmitterSystemBase::GenderExpandString:  expecting MALE or FEMALE!" );
		}
		break;
	case GENDER_MALE:
		{
			Q_snprintf( out, maxlen, "%s%s%s", before, "male", after );
		}
		break;
	case GENDER_FEMALE:
		{
			Q_snprintf( out, maxlen, "%s%s%s", before, "female", after );
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actorname - 
//			*in - 
//			*out - 
//			maxlen - 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::GenderExpandString( char const *actormodel, char const *in, char *out, int maxlen )
{
	gender_t gender = GetActorGender( actormodel );
	GenderExpandString( gender, in, out, maxlen );
}

void CSoundEmitterSystemBase::LoadGlobalActors()
{
	// Now load the global actor list from the scripts/globalactors.txt file
	KeyValues *allActors = NULL;
	
	allActors = new KeyValues( "allactors" );
	if ( allActors->LoadFromFile( g_pFullFileSystem, "scripts/global_actors.txt", NULL ) )
	{
		KeyValues *pvkActor;
		for ( pvkActor = allActors->GetFirstSubKey(); pvkActor != NULL; pvkActor = pvkActor->GetNextKey() )
		{
			int idx = m_ActorGenders.Find( pvkActor->GetName() );
			if ( idx == m_ActorGenders.InvalidIndex() )
			{
				if ( m_ActorGenders.Count() + 1 == m_ActorGenders.InvalidIndex() )
				{
					Warning( "Exceeded max number of actors in scripts/global_actors.txt\n" );
					break;
				}

				gender_t gender = GENDER_NONE;
				if ( !Q_stricmp( pvkActor->GetString(), "male" ) )
				{
					gender = GENDER_MALE;
				}
				else if (!Q_stricmp( pvkActor->GetString(), "female" ) )
				{
					gender = GENDER_FEMALE;
				}
				m_ActorGenders.Insert( pvkActor->GetName(), gender );
			}
		}
	}
	allActors->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actorname - 
// Output : gender_t
//-----------------------------------------------------------------------------
gender_t CSoundEmitterSystemBase::GetActorGender( char const *actormodel )
{
	char actor[ 256 ];
	actor[0] = 0;
	if ( actormodel )
	{
		Q_FileBase( actormodel, actor, sizeof( actor ) );
	}

	int idx = m_ActorGenders.Find( actor );
	if ( idx == m_ActorGenders.InvalidIndex() )
		return GENDER_NONE;

	return m_ActorGenders[ idx ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
//			params - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSoundEmitterSystemBase::InitSoundInternalParameters( const char *soundname, KeyValues *kv, CSoundParametersInternal& params )
{
	// for special case soundentry version error handling
	const char *pSoundEntryVersionValueStr = kv->GetString( "soundentry_version", "1" );
	int nSoundEntryVersion = 1;
	if( pSoundEntryVersionValueStr && pSoundEntryVersionValueStr[0] )
	{
		nSoundEntryVersion = V_atoi( pSoundEntryVersionValueStr );
	}
	bool bEntryNumHasErrored = false;

	KeyValues *pKey = kv->GetFirstSubKey();
	while ( pKey )
	{
		if ( !Q_strcasecmp( pKey->GetName(), "channel" ) )
		{
			params.ChannelFromString( pKey->GetString() );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "volume" ) )
		{
			params.VolumeFromString( pKey->GetString() );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "pitch" ) )
		{
			params.PitchFromString( pKey->GetString() );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "wave" ) )
		{
			ExpandSoundNameMacros( params, pKey->GetString() );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "rndwave" ) )
		{
			KeyValues *pWaves = pKey->GetFirstSubKey();
			while ( pWaves )
			{
				if( params.NumSoundNames() >= MAX_SOUND_RNDWAVE_NUM && 
					nSoundEntryVersion > 1 )
				{
					if( !bEntryNumHasErrored )
					{
						Assert( params.NumSoundNames() >= MAX_SOUND_RNDWAVE_NUM );
						Log_Warning( LOG_SOUNDEMITTER_SYSTEM, "Error: SoundEmitterSystemBase: %s attempting to load too many rndwave soundfiles!\n", soundname );
					}
					bEntryNumHasErrored	= true;
				}
				else
				{
					ExpandSoundNameMacros( params, pWaves->GetString() );
				}

				pWaves = pWaves->GetNextKey();
			}
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "attenuation" ) || !Q_strcasecmp( pKey->GetName(), "CompatibilityAttenuation" ) )
		{
			if ( params.GetSoundLevel().start != SNDLVL_NORM || params.GetSoundLevel().range != 0 )
			{
				DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound %s has multiple attenuation, CompatabilityAttenuation, and/or soundlevel entries.\n", soundname );
			}

			if ( !Q_strncasecmp( pKey->GetString(), "SNDLVL_", strlen( "SNDLVL_" ) ) )
			{
				DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound %s has \"attenuation\" with %s value!\n",
					soundname, pKey->GetString() );
			}

			if ( !Q_strncasecmp( pKey->GetString(), "ATTN_", strlen( "ATTN_" ) ) )
			{
				params.SetSoundLevel( ATTN_TO_SNDLVL( TranslateAttenuation( pKey->GetString() ) ) );
			}
			else
			{
				interval_t interval;
				interval = ReadInterval( pKey->GetString() );

				// Translate from attenuation to soundlevel
				float start = interval.start;
				float end	= interval.start + interval.range;

				params.SetSoundLevel( ATTN_TO_SNDLVL( start ), ATTN_TO_SNDLVL( end ) - ATTN_TO_SNDLVL( start ) );
			}

			// Goldsrc compatibility mode.. feed the sndlevel value through the sound engine interface in such a way
			// that it can reconstruct the original sndlevel value and flag the sound as using Goldsrc attenuation.
			bool bCompatibilityAttenuation = !Q_strcasecmp( pKey->GetName(), "CompatibilityAttenuation" );
			if ( bCompatibilityAttenuation )
			{
				if ( params.GetSoundLevel().range != 0 )
				{
					Warning( "CompatibilityAttenuation for sound %s must have same start and end values.\n", soundname );
				}

				params.SetSoundLevel( SNDLEVEL_TO_COMPATIBILITY_MODE( params.GetSoundLevel().start ) );
			}
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "soundlevel" ) || !Q_strcasecmp( pKey->GetName(), "CompatibilitySoundlevel" ) )
		{
			if ( params.GetSoundLevel().start != SNDLVL_NORM || params.GetSoundLevel().range != 0 )
			{
				DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound %s has multiple attenuation, CompatabilityAttenuation, and/or soundlevel entries.\n", soundname );
			}

			if ( !Q_strncasecmp( pKey->GetString(), "ATTN_", strlen( "ATTN_" ) ) )
			{
				DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound %s has \"soundlevel\" with %s value!\n",
					soundname, pKey->GetString() );
			}

			params.SoundLevelFromString( pKey->GetString() );

			// Goldsrc compatibility mode.. feed the sndlevel value through the sound engine interface in such a way
			// that it can reconstruct the original sndlevel value and flag the sound as using Goldsrc attenuation.
			bool bCompatibilityAttenuation = !Q_strcasecmp( pKey->GetName(), "CompatibilitySoundlevel" );
			if ( bCompatibilityAttenuation )
			{
				if ( params.GetSoundLevel().range != 0 )
				{
					Warning( "CompatibilitySoundlevel for sound %s must have same start and end values.\n", soundname );
				}

				params.SetSoundLevel( SNDLEVEL_TO_COMPATIBILITY_MODE( params.GetSoundLevel().start ) );
			}
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "play_to_owner_only" ) )
		{
			params.SetOnlyPlayToOwner( pKey->GetBool() );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "delay_msec" ) )
		{
			// Don't allow negative delay
			params.SetDelayMsec( MAX( 0, pKey->GetInt() ) );

		}
		else if ( !Q_strcasecmp( pKey->GetName(), "soundentry_version" ) )
		{
			params.SetSoundEntryVersion( pKey->GetInt() );
		}
		else if ( !V_strcasecmp( pKey->GetName(), "operator_stacks" ) )
		{
			params.SetOperatorsKV( pKey );
		}
		else if (!V_strcasecmp(pKey->GetName(), "hrtf_follow"))
		{
			params.SetHRTFFollowEntity(pKey->GetBool());
		}
		else if (!V_strcasecmp(pKey->GetName(), "hrtf_bilinear"))
		{
			params.SetHRTFBilinear(pKey->GetBool());
		}


		pKey = pKey->GetNextKey();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
// Output : char const
//-----------------------------------------------------------------------------
const char *CSoundEmitterSystemBase::GetWavFileForSound( const char *soundname, char const *actormodel )
{
	gender_t gender = GetActorGender( actormodel );
	return GetWavFileForSound( soundname, gender );
}

const char *CSoundEmitterSystemBase::GetWavFileForSound( const char *soundname, gender_t gender )
{
	CSoundParameters params;
	if ( !GetParametersForSound( soundname, params, gender ) )
	{
		return soundname;
	}

	if ( !params.soundname[ 0 ] )
	{
		return soundname;
	}

	static char outsound[ 512 ];
	Q_strncpy( outsound, params.soundname, sizeof( outsound ) );
	return outsound;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
// Output : soundlevel_t
//-----------------------------------------------------------------------------
soundlevel_t CSoundEmitterSystemBase::LookupSoundLevel( const char *soundname )
{
	CSoundParameters params;
	if ( !GetParametersForSound( soundname, params, GENDER_NONE ) )
	{
		return SNDLVL_NORM;
	}

	return params.soundlevel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::AddSoundsFromFile( const char *filename, bool bPreload, bool bAutoCache, bool bIsOverride /*=false*/ )
{
	CSoundScriptFile sf;
	sf.hFilename = g_pFullFileSystem->FindOrAddFileName( filename );
	sf.dirty = false;

	int scriptindex = m_SoundKeyValues.AddToTail( sf );

	int replaceCount = 0;
	int newOverrideCount = 0;
	int duplicatedReplacements = 0;

	// Open the soundscape data file, and abort if we can't
	KeyValues *kv = new KeyValues( "" );
	if ( g_pFullFileSystem->LoadKeyValues( *kv, IFileSystem::TYPE_SOUNDEMITTER, filename, "GAME" ) )
	{
		// parse out all of the top level sections and save their names
		KeyValues *pKeys = kv;
		while ( pKeys )
		{
			if ( pKeys->GetFirstSubKey() )
			{
				if ( m_Sounds.Count() + 1 == m_Sounds.InvalidIndex() )
				{
					Warning( "Exceeded maximum number of sound emitter entries\n" );
					break;
				}

				CSoundEntry *pEntry;

				{
					MEM_ALLOC_CREDIT();
					pEntry = new CSoundEntry;
				}
			
				const char *pName = pKeys->GetName();
				if ( !V_strlen( pName ) )
				{
					Error( "Syntax Error! Empty named KV block in %s\n", filename );
				}

				pEntry->m_Name = pName;
				pEntry->m_bRemoved			= false;
				pEntry->m_nScriptFileIndex	= scriptindex;
				pEntry->m_bIsOverride		= bIsOverride;

				HSOUNDSCRIPTHASH hash = HashSoundName( pEntry->m_Name.String() );

				if ( bIsOverride )
				{
					++newOverrideCount;
				}

				bool add = true;

				int lookup = GetSoundIndexForHash( hash );
				if ( lookup != m_Sounds.InvalidIndex() )
				{
					add = false;
					if ( bIsOverride )
					{
						MEM_ALLOC_CREDIT();

						// Store off the old sound if it's not already an "override" from another file!!!
						// Otherwise, just whack it again!!!
						if ( !m_Sounds[ lookup ]->m_bIsOverride )
						{
							m_SavedOverrides.AddToTail( m_Sounds[ lookup ] );
						}
						else
						{
							delete m_Sounds[ lookup ];
							++duplicatedReplacements;
						}

						InitSoundInternalParameters( pKeys->GetName(), pKeys, pEntry->m_SoundParams );
						pEntry->m_SoundParams.SetShouldPreload( bPreload ); // this gets handled by game code after initting.
						pEntry->m_SoundParams.SetShouldAutoCache( bAutoCache ); // this gets handled by game code after initting.

						m_Sounds[ lookup ] = pEntry;

						++replaceCount;
					}
					else
					{
						delete pEntry;
					 //	DevMsg( "CSoundEmitterSystem::AddSoundsFromFile(%s):  Entry %s duplicated, skipping\n", filename, pKeys->GetName() );
					}
				}
				
				if ( add )
				{
					MEM_ALLOC_CREDIT();

					InitSoundInternalParameters( pKeys->GetName(), pKeys, pEntry->m_SoundParams );
					pEntry->m_SoundParams.SetShouldPreload( bPreload ); // this gets handled by game code after initting.
					pEntry->m_SoundParams.SetShouldAutoCache( bAutoCache ); // this gets handled by game code after initting.

					int idx = m_Sounds.AddToTail( pEntry );
					AddHash( pEntry->m_Name.String(), idx );
				}
			}
			pKeys = pKeys->GetNextKey();
		}

		kv->deleteThis();
	}
	else
	{
		if ( !bIsOverride )
		{
			Warning( "CSoundEmitterSystem::AddSoundsFromFile:  No such file %s\n", filename );
		}

		// Discard
		m_SoundKeyValues.Remove( scriptindex );

		kv->deleteThis();

		return;
	}

	
	if ( bIsOverride )
	{
		Warning( "SoundEmitter:  adding map sound overrides from %s [%i total, %i replacements, %i duplicated replacements]\n", 
			filename,
			newOverrideCount,
			replaceCount,
			duplicatedReplacements );
	}

	Assert( scriptindex >= 0 );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CSoundEmitterSystemBase::CheckForMissingWavFiles( bool verbose )
{
	int missing = 0;

	int c = GetSoundCount();
	int i;
	char testfile[ 512 ];

	for ( i = 0; i < c; i++ )
	{
		CSoundParametersInternal *internal = InternalGetParametersForSound( i );
		if ( !internal )
		{
			Assert( 0 );
			continue;
		}

		int waveCount = internal->NumSoundNames();
		for ( int wave = 0; wave < waveCount; wave++ )
		{
			CUtlSymbol sym = internal->GetSoundNames()[ wave ].symbol;
			const char *name = m_Waves.String( sym );
			if ( !name || !name[ 0 ] )
			{
				Assert( 0 );
				continue;
			}

			// Skip ! sentence stuff
			if ( name[0] == CHAR_SENTENCE )
				continue;
			Q_snprintf( testfile, sizeof( testfile ), "sound/%s", PSkipSoundChars( name ) );
			if ( g_pFullFileSystem->FileExists( testfile ) )
				continue;

			internal->SetHadMissingWaveFiles( true );

			++missing;

			if ( verbose )
			{
				DevMsg( "Sound %s references missing file %s\n", GetSoundName( i ), name );
			}
		}
	}

	return missing;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *key - 
// Output : float
//-----------------------------------------------------------------------------
float CSoundEmitterSystemBase::TranslateAttenuation( const char *key )
{
	if ( !key )
	{
		Assert( 0 );
		return ATTN_NORM;
	}

	if ( !Q_strcasecmp( key, "ATTN_NONE" ) )
		return ATTN_NONE;

	if ( !Q_strcasecmp( key, "ATTN_NORM" ) )
		return ATTN_NORM;

	if ( !Q_strcasecmp( key, "ATTN_IDLE" ) )
		return ATTN_IDLE;

	if ( !Q_strcasecmp( key, "ATTN_STATIC" ) )
		return ATTN_STATIC;

	if ( !Q_strcasecmp( key, "ATTN_RICOCHET" ) )
		return ATTN_RICOCHET;

	if ( !Q_strcasecmp( key, "ATTN_GUNFIRE" ) )
		return ATTN_GUNFIRE;

	DevMsg( "CSoundEmitterSystem:  Unknown attenuation key %s\n", key );

	return ATTN_NORM;
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *key - 
// Output : soundlevel_t
//-----------------------------------------------------------------------------
soundlevel_t CSoundEmitterSystemBase::TranslateSoundLevel( const char *key )
{
	return TextToSoundLevel( key );
}

//-----------------------------------------------------------------------------
// Purpose: Convert "chan_xxx" into integer value for channel
// Input  : *name - 
// Output : static int
//-----------------------------------------------------------------------------
int CSoundEmitterSystemBase::TranslateChannel( const char *name )
{
	return TextToChannel( name );
}

const char *CSoundEmitterSystemBase::GetSourceFileForSound( int index ) const
{
	if ( index < 0 || index >= (int)m_Sounds.Count() )
	{
		Assert( 0 );
		return "";
	}

	CSoundEntry const *entry = m_Sounds[ index ];
	int scriptindex = entry->m_nScriptFileIndex;
	if ( scriptindex < 0 || scriptindex >= m_SoundKeyValues.Count() )
	{
		Assert( 0 );
		return "";
	}
	static char fn[ 512 ];
	if ( g_pFullFileSystem->String( m_SoundKeyValues[ scriptindex ].hFilename, fn, sizeof( fn ) ))
	{
		return fn;
	}
	Assert( 0 );
	return "";
}

const char *CSoundEmitterSystemBase::GetWaveName( CUtlSymbol& sym )
{
	return m_Waves.String( sym );
}

int	CSoundEmitterSystemBase::FindSoundScript( const char *name ) const
{
	int i, c;

	FileNameHandle_t hFilename = g_pFullFileSystem->FindFileName( name );
	if ( hFilename )
	{
		// First, make sure it's known
		c = m_SoundKeyValues.Count();
		for ( i = 0; i < c ; i++ )
		{
			if ( m_SoundKeyValues[ i ].hFilename == hFilename )
			{
				return i;
			}
		}
	}

	return m_SoundKeyValues.InvalidIndex();
}

bool CSoundEmitterSystemBase::AddSound( const char *soundname, const char *scriptfile, const CSoundParametersInternal& params )
{
	int idx = GetSoundIndex( soundname );


	int i = FindSoundScript( scriptfile );
	if ( i == m_SoundKeyValues.InvalidIndex() )
	{
		Warning( "CSoundEmitterSystemBase::AddSound( '%s', '%s', ... ), script file not list in manifest '%s'\n",
			soundname, scriptfile, MANIFEST_FILE );
		return false;
	}

	MEM_ALLOC_CREDIT();

	// More like an update...
	if ( IsValidIndex( idx ) )
	{
		CSoundEntry *entry = m_Sounds[ idx ];

		entry->m_bRemoved				= false;
		entry->m_nScriptFileIndex		= i;
		entry->m_SoundParams.CopyFrom( params );

		m_SoundKeyValues[ i ].dirty = true;

		return true;
	}

	CSoundEntry *pEntry = new CSoundEntry;
	pEntry->m_Name = soundname;
	pEntry->m_bRemoved			= false;
	pEntry->m_nScriptFileIndex	= i;
	pEntry->m_SoundParams.CopyFrom( params );

	idx = m_Sounds.AddToTail( pEntry );
	AddHash( pEntry->m_Name.String(), idx );

	m_SoundKeyValues[ i ].dirty = true;

	return true;
}

void CSoundEmitterSystemBase::RemoveSound( const char *soundname )
{
	int idx = GetSoundIndex( soundname );
	if ( !IsValidIndex( idx ) )
	{
		Warning( "Can't remove %s, no such sound!\n", soundname );
		return;
	}

	m_Sounds[ idx ]->m_bRemoved = true;

	// Mark script as dirty
	int scriptindex = m_Sounds[ idx ]->m_nScriptFileIndex;
	if ( scriptindex < 0 || scriptindex >= m_SoundKeyValues.Count() )
	{
		Assert( 0 );
		return;
	}

	m_SoundKeyValues[ scriptindex ].dirty = true;
}

void CSoundEmitterSystemBase::MoveSound( const char *soundname, const char *newscript )
{
	int idx = GetSoundIndex( soundname );
	if ( !IsValidIndex( idx ) )
	{
		Warning( "Can't move '%s', no such sound!\n", soundname );
		return;
	}

	int oldscriptindex = m_Sounds[ idx ]->m_nScriptFileIndex;
	if ( oldscriptindex < 0 || oldscriptindex >= m_SoundKeyValues.Count() )
	{
		Assert( 0 );
		return;
	}

	int newscriptindex = FindSoundScript( newscript );
	if ( newscriptindex == m_SoundKeyValues.InvalidIndex() )
	{
		Warning( "CSoundEmitterSystemBase::MoveSound( '%s', '%s' ), script file not list in manifest '%s'\n",
			soundname, newscript, MANIFEST_FILE );
		return;
	}

	// No actual change
	if ( oldscriptindex == newscriptindex )
	{
		return;
	}

	// Move it
	m_Sounds[ idx ]->m_nScriptFileIndex = newscriptindex;

	// Mark both scripts as dirty
	m_SoundKeyValues[ oldscriptindex ].dirty = true;
	m_SoundKeyValues[ newscriptindex ].dirty = true;
}

int CSoundEmitterSystemBase::GetNumSoundScripts() const
{
	return m_SoundKeyValues.Count();
}

const char *CSoundEmitterSystemBase::GetSoundScriptName( int index ) const
{
	if ( index < 0 || index >= m_SoundKeyValues.Count() )
		return NULL;

	static char fn[ 512 ];
	if ( g_pFullFileSystem->String( m_SoundKeyValues[ index ].hFilename, fn, sizeof( fn ) ) )
	{
		return fn;
	}
	return "";
}

bool CSoundEmitterSystemBase::IsSoundScriptDirty( int index ) const
{
	if ( index < 0 || index >= m_SoundKeyValues.Count() )
		return false;

	return m_SoundKeyValues[ index ].dirty;
}

void CSoundEmitterSystemBase::SaveChangesToSoundScript( int scriptindex )
{
	const char *outfile = GetSoundScriptName( scriptindex );
	if ( !outfile )
	{
		Msg( "CSoundEmitterSystemBase::SaveChangesToSoundScript:  No script file for index %i\n", scriptindex );
		return;
	}

	if ( g_pFullFileSystem->FileExists( outfile ) &&
		 !g_pFullFileSystem->IsFileWritable( outfile ) )
	{
		Warning( "%s is not writable, can't save data to file\n", outfile );
		return;
	}

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	// FIXME:  Write sound script header
	if ( g_pFullFileSystem->FileExists( GAME_SOUNDS_HEADER_BLOCK ) )
	{
		FileHandle_t header = g_pFullFileSystem->Open( GAME_SOUNDS_HEADER_BLOCK, "rb", NULL );
		if ( header != FILESYSTEM_INVALID_HANDLE )
		{
			int len = g_pFullFileSystem->Size( header );
			
			unsigned char *data = new unsigned char[ len + 1 ];
			Q_memset( data, 0, len + 1 );
			
			g_pFullFileSystem->Read( data, len, header );
			g_pFullFileSystem->Close( header );

			data[ len ] = 0;

			char *p = (char *)data;
			while ( *p )
			{
				if ( *p != '\r' )
				{
					buf.PutChar( *p );
				}
				++p;
			}

			delete[] data;
		}

		buf.Printf( "\n" );
	}


	int c = GetSoundCount();
	for ( int i = 0; i < c; i++ )
	{
		if ( Q_stricmp( outfile, GetSourceFileForSound( i ) ) )
			continue;

		// It's marked for deletion, just skip it
		if ( m_Sounds[ i ]->m_bRemoved )
			continue;

		CSoundParametersInternal *p = InternalGetParametersForSound( i );
		if ( !p )
			continue;
		
		buf.Printf( "\"%s\"\n{\n", GetSoundName( i ) );

		buf.Printf( "\t\"channel\"\t\t\"%s\"\n", p->ChannelToString() );
		buf.Printf( "\t\"volume\"\t\t\"%s\"\n", p->VolumeToString() );
		buf.Printf( "\t\"pitch\"\t\t\t\"%s\"\n", p->PitchToString() );
		buf.Printf( "\n" );
		buf.Printf( "\t\"soundlevel\"\t\"%s\"\n", p->SoundLevelToString() );

		if ( p->OnlyPlayToOwner() )
		{
			buf.Printf( "\t\"play_to_owner_only\"\t\"1\"\n" );
		}

		if ( p->GetDelayMsec() != 0 )
		{
			buf.Printf( "\t\"delay_msec\"\t\"%i\"\n", p->GetDelayMsec() );
		}

		int totalCount = 0;

		int waveCount = p->NumSoundNames();
		int convertedCount = p->NumConvertedNames();

		totalCount = ( waveCount - 2 * convertedCount ) + convertedCount;

		if  ( totalCount > 0 )
		{
			buf.Printf( "\n" );

			if ( waveCount == 1 )
			{
				Assert( p->GetSoundNames()[ 0 ].gender == GENDER_NONE );
				buf.Printf( "\t\"wave\"\t\t\t\"%s\"\n", GetWaveName( p->GetSoundNames()[ 0 ].symbol ) );
			}
			else if ( convertedCount == 1 )
			{
				Assert( p->GetConvertedNames()[ 0 ].gender == GENDER_NONE );
				buf.Printf( "\t\"wave\"\t\t\t\"%s\"\n", GetWaveName( p->GetConvertedNames()[ 0 ].symbol ) );
			}
			else
			{
				buf.Printf( "\t\"rndwave\"\n" );
				buf.Printf( "\t{\n" );

				int wave;
				for ( wave = 0; wave < waveCount; wave++ )
				{
					// Skip macro-expanded names
					if ( p->GetSoundNames()[ wave ].gender != GENDER_NONE )
						continue;

					buf.Printf( "\t\t\"wave\"\t\"%s\"\n", GetWaveName( p->GetSoundNames()[ wave ].symbol ) );
				}
				for ( wave = 0; wave < convertedCount; wave++ )
				{
					buf.Printf( "\t\t\"wave\"\t\"%s\"\n", GetWaveName( p->GetConvertedNames()[ wave ].symbol ) );
				}

				buf.Printf( "\t}\n" );
			}

		}

		buf.Printf( "}\n" );

		if ( i != c - 1 )
		{
			buf.Printf( "\n" );
		}
	}

	// Write it out baby
	FileHandle_t fh = g_pFullFileSystem->Open( outfile, "wt" );
	if (fh)
	{
		g_pFullFileSystem->Write( buf.Base(), buf.TellPut(), fh );
		g_pFullFileSystem->Close(fh);

		// Changed saved successfully
		m_SoundKeyValues[ scriptindex ].dirty = false;
	}
	else
	{
		Warning( "SceneManager_SaveSoundsToScriptFile:  Unable to write file %s!!!\n", outfile );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : CUtlSymbol
//-----------------------------------------------------------------------------
CUtlSymbol CSoundEmitterSystemBase::AddWaveName( const char *name )
{
	return m_Waves.AddString( name );
}

void CSoundEmitterSystemBase::RenameSound( const char *soundname, const char *newname )
{
	// Same name?
	if ( !Q_stricmp( soundname, newname ) )
	{
		return;
	}

	HSOUNDSCRIPTHASH oldHash = HashSoundName( soundname );
	HSOUNDSCRIPTHASH newHash = HashSoundName( newname );

	int index = GetSoundIndexForHash( oldHash );
	if ( !IsValidIndex( index ) )
	{
		Msg( "Can't rename %s, no such sound\n", soundname );
		return;
	}

	int check = GetSoundIndexForHash( newHash );
	if ( IsValidIndex( check ) )
	{
		Msg( "Can't rename %s to %s, new name already in list\n", soundname, newname );
		return;
	}

	MEM_ALLOC_CREDIT();

	// Copy out old entry
	CSoundEntry *pEntry = m_Sounds[ index ];
	pEntry->m_Name = newname;
	RemoveHash( soundname );
	AddHash( pEntry->m_Name.String(), index );

	// Mark associated script as dirty
	m_SoundKeyValues[ pEntry->m_nScriptFileIndex ].dirty = true;
}

void CSoundEmitterSystemBase::UpdateSoundParameters( const char *soundname, const CSoundParametersInternal& params )
{
	int idx = GetSoundIndex( soundname );
	if ( !IsValidIndex( idx ) )
	{
		Msg( "Can't UpdateSoundParameters %s, no such sound\n", soundname );
		return;
	}

	CSoundEntry *entry = m_Sounds[ idx ];

	if ( entry->m_SoundParams == params )
	{
		// No changes
		return;
	}

	// Update parameters
	entry->m_SoundParams.CopyFrom( params );
	// Set dirty flag
	m_SoundKeyValues[ entry->m_nScriptFileIndex ].dirty = true;
}

bool CSoundEmitterSystemBase::IsUsingGenderToken( char const *soundname )
{
	HSOUNDSCRIPTHASH hash = HashSoundName( soundname );
	int soundindex = GetSoundIndexForHash( hash );

	// Look up the sound level from the soundemitter system
	CSoundParametersInternal *params = InternalGetParametersForSound( soundindex );
	if ( !params )
		return false;

	return params->UsesGenderToken();
}

unsigned int CSoundEmitterSystemBase::GetManifestFileTimeChecksum()
{
	return m_uManifestPlusScriptChecksum;
}

bool CSoundEmitterSystemBase::GetParametersForSoundEx( const char *soundname, HSOUNDSCRIPTHASH& handle, CSoundParameters& params, gender_t gender, bool isbeingemitted /*= false*/ )
{
	if ( g_pResourceAccessControl )
	{
		if ( !g_pResourceAccessControl->IsAccessAllowed( RESOURCE_GAMESOUND, soundname ) )
			return false;
	}

	if ( handle == SOUNDEMITTER_INVALID_HASH )
	{
		handle = HashSoundName( soundname );
	}

	int index = GetSoundIndexForHash( handle );

	CSoundParametersInternal *internal = InternalGetParametersForSound( index );

	if ( !internal )
	{
		Assert( 0 );
		Warning( "CSoundEmitterSystemBase::GetParametersForSound:  No such sound %s\n", soundname );

		HSOUNDSCRIPTHASH hash = HashSoundName( "Error" );
		int index = GetSoundIndexForHash( hash );
		internal = InternalGetParametersForSound( index );
		if ( !internal )
			return false;
	}

	int nNumberOfSoundNames = internal->NumSoundNames();
#if IsPlatformPS3()
	if ( g_pFullFileSystem->IsPrefetchingDone() == false )
	{
		nNumberOfSoundNames = imin( nNumberOfSoundNames, 5 );			// The HDD is not filled yet, we are going to play up to 5 variations max
	}
#elif IsPlatformX360()
	if ( g_pXboxInstaller->IsFullyInstalled() == false )
	{
		nNumberOfSoundNames = imin( nNumberOfSoundNames, 5 );			// Either there is no HDD, or it has not been fully installed
	}
#endif

	params.channel = internal->GetChannel();
	params.volume = internal->GetVolume().Random();
	params.pitch = internal->GetPitch().Random();
	params.pitchlow = internal->GetPitch().start;
	params.pitchhigh = params.pitchlow + internal->GetPitch().range;
	params.delay_msec = internal->GetDelayMsec();
	params.count = nNumberOfSoundNames;
	params.soundname[ 0 ] = 0;

	params.m_nSoundEntryVersion = (int)internal->GetSoundEntryVersion();
	params.m_hSoundScriptHash = handle;
	params.m_pOperatorsKV = internal->GetOperatorsKV();

	int bestIndex = FindBestSoundForGender( internal->GetSoundNames(), nNumberOfSoundNames, gender,  params.m_nRandomSeed );

	if ( bestIndex >= 0 )
	{
		Q_strncpy( params.soundname, GetWaveName( internal->GetSoundNames()[ bestIndex ].symbol), sizeof( params.soundname ) );

		// If we are actually emitting the sound, mark it as not available...
		if ( isbeingemitted )
		{
			internal->GetSoundNames()[ bestIndex ].available = 0;
		}
	}
	params.soundlevel = (soundlevel_t)(int)internal->GetSoundLevel().Random();
	params.play_to_owner_only = internal->OnlyPlayToOwner();
	params.m_bHRTFBilinear = internal->HasHRTFBilinear();
	params.m_bHRTFFollowEntity = internal->HasHRTFFollowEntity();

	if ( !params.soundname[ 0 ] )
	{
		DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound %s has no wave or rndwave key!\n", soundname );
		return false;
	}

	if ( internal->HadMissingWaveFiles() &&
		params.soundname[ 0 ] != CHAR_SENTENCE )
	{
		char testfile[ 256 ];
		Q_snprintf( testfile, sizeof( testfile ), "sound/%s", PSkipSoundChars( params.soundname ) );
		if ( !g_pFullFileSystem->FileExists( testfile ) )
		{
			// Prevent repetitive spew...
			static CUtlSymbolTable soundWarnings;
			char key[ 256 ];
			Q_snprintf( key, sizeof( key ), "%s:%s", soundname, params.soundname );
			if ( UTL_INVAL_SYMBOL == soundWarnings.Find( key ) )
			{
				soundWarnings.AddString( key );
			
				DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound '%s' references wave '%s' which doesn't exist on disk!\n", 
					soundname,
					params.soundname );
			}
			return false;
		}
	}

	return true;
}

soundlevel_t CSoundEmitterSystemBase::LookupSoundLevelByHandle( char const *soundname, HSOUNDSCRIPTHASH& handle )
{
	if ( handle == SOUNDEMITTER_INVALID_HASH )
	{
		handle = HashSoundName( soundname );
	}
	int index = GetSoundIndexForHash( handle );

	CSoundParametersInternal *internal = InternalGetParametersForSound( index );
	if ( !internal )
	{
		return SNDLVL_NORM;
	}

	return (soundlevel_t)(int)internal->GetSoundLevel().Random();
}

KeyValues * CSoundEmitterSystemBase::GetOperatorKVByHandle( HSOUNDSCRIPTHASH& handle )
{
	if ( handle == SOUNDEMITTER_INVALID_HASH )
	{
		return NULL;
	}
	int index = GetSoundIndexForHash( handle );

	CSoundParametersInternal *internal = InternalGetParametersForSound( index );
	if ( !internal )
	{
		return NULL;
	}

	return internal->GetOperatorsKV();
}

// Called from both client and server (single player) or just one (server only in dedicated server and client only if connected to a remote server)
// Called by LevelInitPreEntity to override sound scripts for the mod with level specific overrides based on custom mapnames, etc.
void CSoundEmitterSystemBase::AddSoundOverrides( char const *scriptfile )
{
	FileNameHandle_t handle = g_pFullFileSystem->FindOrAddFileName( scriptfile );
	if ( m_OverrideFiles.Find( handle ) != m_OverrideFiles.InvalidIndex() )
		return;

	m_OverrideFiles.AddToTail( handle );
	// These are overrides and assume bShoudPreload and bShouldAutocache are false
	AddSoundsFromFile( scriptfile, false, false, true );
}

// Called by either client or server in LevelShutdown to clear out custom overrides
void CSoundEmitterSystemBase::ClearSoundOverrides()
{
	for ( int i = 0; i < m_SavedOverrides.Count(); ++i )
	{
		CSoundEntry *entry = m_SavedOverrides[ i ];
		HSOUNDSCRIPTHASH hash = HashSoundName( entry->m_Name.String() );
		int idx = GetSoundIndexForHash( hash );
		if ( IsValidIndex( idx ) )
		{
			delete m_Sounds[ idx ];
			m_Sounds[ idx ] = m_SavedOverrides[i];
		}
	}

	m_SavedOverrides.Purge();
	m_OverrideFiles.Purge();
}

void CSoundEmitterSystemBase::AddHash( char const *pchSoundName, int nIndex )
{
	Assert( nIndex >= 0 && nIndex < m_Sounds.Count() );
	CSoundEntry *entry = m_Sounds[ nIndex ];
	HSOUNDSCRIPTHASH hash = HashSoundName( pchSoundName );
	
	// Check for collisions
	int slot = m_HashToSoundEntry.Find( hash );
	if ( slot != m_HashToSoundEntry.InvalidIndex() )
	{
		Error( "Sound name hash collision!  '%s' collides with '%s' %i!", pchSoundName, m_HashToSoundEntry[ slot ].pEntry->m_Name.String(), hash );
		return;
	}

	soundEntryHash_t soundEntryHash =
	{
		nIndex,
		entry
	};
	m_HashToSoundEntry.Insert( hash, soundEntryHash );
}

void CSoundEmitterSystemBase::RemoveHash( char const *pchSoundName )
{
	m_HashToSoundEntry.Remove( HashSoundName( pchSoundName ) );
}

char const *CSoundEmitterSystemBase::GetSoundNameForHash( HSOUNDSCRIPTHASH hash ) const
{
	int slot = m_HashToSoundEntry.Find( hash );
	if ( slot == m_HashToSoundEntry.InvalidIndex() )
		return NULL;
	CSoundEntry *entry = m_HashToSoundEntry[ slot ].pEntry;
	return entry->m_Name.String();
}

int CSoundEmitterSystemBase::GetSoundIndexForHash( HSOUNDSCRIPTHASH hash ) const
{
	int slot = m_HashToSoundEntry.Find( hash );
	if ( slot == m_HashToSoundEntry.InvalidIndex() )
		return m_Sounds.InvalidIndex();
	return m_HashToSoundEntry[ slot ].soundIndex;
}

#define SOUNDEMITTER_MURMURHASH_SEED ( ( 'D' << 24 ) | ( 'O' << 16 ) | ( 'T' << 8 ) | 'A' )

HSOUNDSCRIPTHASH CSoundEmitterSystemBase::HashSoundName( char const *pchSndName ) const
{
	HSOUNDSCRIPTHASH hash = MurmurHash2LowerCase( pchSndName, SOUNDEMITTER_MURMURHASH_SEED );
	return hash;
}

bool CSoundEmitterSystemBase::IsValidHash( HSOUNDSCRIPTHASH hash ) const
{
	int idx = m_HashToSoundEntry.Find( hash );
	return idx != m_HashToSoundEntry.InvalidIndex();
}

void CSoundEmitterSystemBase::DescribeSound( char const *soundname )
{
	HSOUNDSCRIPTHASH hash = HashSoundName( soundname );
	int index = GetSoundIndexForHash( hash );
	if ( index == m_Sounds.InvalidIndex() )
	{
		Msg( "SoundEmitterSystemBase::DescribeSound:  No such sound %s\n", soundname );
		return;
	}

	CSoundParametersInternal *p = InternalGetParametersForSound( index );
	if ( !p )
	{
		Msg( "SoundEmitterSystemBase::DescribeSound:  No such sound %s\n", soundname );
		return;
	}

	Msg( "\"%s\"\n{\n", GetSoundName( index ) );

	Msg( "\t\"channel\"\t\t\"%s\"\n", p->ChannelToString() );
	Msg( "\t\"volume\"\t\t\"%s\"\n", p->VolumeToString() );
	Msg( "\t\"pitch\"\t\t\t\"%s\"\n", p->PitchToString() );
	Msg( "\n" );
	Msg( "\t\"soundlevel\"\t\"%s\"\n", p->SoundLevelToString() );

	if ( p->OnlyPlayToOwner() )
	{
		Msg( "\t\"play_to_owner_only\"\t\"1\"\n" );
	}

	if ( p->GetDelayMsec() != 0 )
	{
		Msg( "\t\"delay_msec\"\t\"%i\"\n", p->GetDelayMsec() );
	}

	int totalCount = 0;

	int waveCount = p->NumSoundNames();
	int convertedCount = p->NumConvertedNames();

	totalCount = ( waveCount - 2 * convertedCount ) + convertedCount;

	if  ( totalCount > 0 )
	{
		Msg( "\n" );

		if ( waveCount == 1 )
		{
			Assert( p->GetSoundNames()[ 0 ].gender == GENDER_NONE );
			Msg( "\t\"wave\"\t\t\t\"%s\"\n", GetWaveName( p->GetSoundNames()[ 0 ].symbol ) );
		}
		else if ( convertedCount == 1 )
		{
			Assert( p->GetConvertedNames()[ 0 ].gender == GENDER_NONE );
			Msg( "\t\"wave\"\t\t\t\"%s\"\n", GetWaveName( p->GetConvertedNames()[ 0 ].symbol ) );
		}
		else
		{
			Msg( "\t\"rndwave\"\n" );
			Msg( "\t{\n" );

			int wave;
			for ( wave = 0; wave < waveCount; wave++ )
			{
				// Skip macro-expanded names
				if ( p->GetSoundNames()[ wave ].gender != GENDER_NONE )
					continue;

				Msg( "\t\t\"wave\"\t\"%s\"\n", GetWaveName( p->GetSoundNames()[ wave ].symbol ) );
			}
			for ( wave = 0; wave < convertedCount; wave++ )
			{
				Msg( "\t\t\"wave\"\t\"%s\"\n", GetWaveName( p->GetConvertedNames()[ wave ].symbol ) );
			}

			Msg( "\t}\n" );
		}
	}

	Msg( "}\n" );
}

void CSoundEmitterSystemBase::Flush()
{
	ShutdownSounds();
	LoadGameSoundManifest();
}

CSoundEmitterSystemBase g_SoundEmitterSystemBase;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CSoundEmitterSystemBase, ISoundEmitterSystemBase, 
						SOUNDEMITTERSYSTEM_INTERFACE_VERSION, g_SoundEmitterSystemBase );
