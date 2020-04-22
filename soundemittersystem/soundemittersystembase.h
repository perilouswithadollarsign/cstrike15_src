//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef SOUNDEMITTERSYSTEMBASE_H
#define SOUNDEMITTERSYSTEMBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "tier1/utldict.h"
#include "soundflags.h"
#include "tier2/interval.h"
#include "tier1/utlsortvector.h"
#include "tier1/mempool.h"
#include "tier2/tier2.h"

soundlevel_t TextToSoundLevel( const char *key );

struct CSoundEntry
{
	CUtlSymbol					m_Name;
	CSoundParametersInternal	m_SoundParams;
	uint16						m_nScriptFileIndex;
	bool						m_bRemoved : 1;
	bool						m_bIsOverride : 1;

	class CSoundEntryLess
	{
	public:
		bool Less( CSoundEntry * const & lhs, CSoundEntry *const & rhs, void *pCtx )
		{
			return ( Q_stricmp( lhs->m_Name.String(), rhs->m_Name.String() ) < 0 ) ? true : false;
		}
	};

	DECLARE_FIXEDSIZE_ALLOCATOR(CSoundEntry);
};

//-----------------------------------------------------------------------------
// Purpose: Base class for sound emitter system handling (can be used by tools)
//-----------------------------------------------------------------------------
class CSoundEmitterSystemBase : public CTier2AppSystem< ISoundEmitterSystemBase >
{
	typedef CTier2AppSystem< ISoundEmitterSystemBase > BaseClass;

public:
	CSoundEmitterSystemBase();
	virtual ~CSoundEmitterSystemBase() { }

	// Methods of IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );

public:
	virtual int	GetSoundIndex( const char *pName ) const;
	virtual bool IsValidIndex( int index );
	virtual int GetSoundCount( void );

	virtual const char *GetSoundName( int index );
	virtual bool GetParametersForSound( const char *soundname, CSoundParameters& params, gender_t gender, bool isbeingemitted = false );
	virtual const char *GetWaveName( CUtlSymbol& sym );

	virtual CUtlSymbol AddWaveName( const char *name );

	virtual soundlevel_t LookupSoundLevel( const char *soundname );
	virtual const char *GetWavFileForSound( const char *soundname, char const *actormodel );
	virtual const char *GetWavFileForSound( const char *soundname, gender_t gender );

	virtual int		CheckForMissingWavFiles( bool verbose );
	virtual const char *GetSourceFileForSound( int index ) const;
	// Iteration methods
	virtual int		First() const;
	virtual int		Next( int i ) const;
	virtual int		InvalidIndex() const;

	virtual CSoundParametersInternal *InternalGetParametersForSound( int index );

	// The host application is responsible for dealing with dirty sound scripts, etc.
	virtual bool		AddSound( const char *soundname, const char *scriptfile, const CSoundParametersInternal& params );
	virtual void		RemoveSound( const char *soundname );

	virtual void		MoveSound( const char *soundname, const char *newscript );

	virtual void		RenameSound( const char *soundname, const char *newname );
	virtual void		UpdateSoundParameters( const char *soundname, const CSoundParametersInternal& params );

	virtual int			GetNumSoundScripts() const;
	virtual char const	*GetSoundScriptName( int index ) const;
	virtual bool		IsSoundScriptDirty( int index ) const;
	virtual int			FindSoundScript( const char *name ) const;

	virtual void		SaveChangesToSoundScript( int scriptindex );

	virtual void		ExpandSoundNameMacros( CSoundParametersInternal& params, char const *wavename );
	virtual gender_t	GetActorGender( char const *actormodel );
	virtual void		GenderExpandString( char const *actormodel, char const *in, char *out, int maxlen );
	virtual void		GenderExpandString( gender_t gender, char const *in, char *out, int maxlen );
	virtual bool		IsUsingGenderToken( char const *soundname );
	virtual unsigned int GetManifestFileTimeChecksum();

	virtual bool			GetParametersForSoundEx( const char *soundname, HSOUNDSCRIPTHASH& handle, CSoundParameters& params, gender_t gender, bool isbeingemitted = false );
	virtual soundlevel_t	LookupSoundLevelByHandle( char const *soundname, HSOUNDSCRIPTHASH& handle );
	virtual KeyValues		*GetOperatorKVByHandle( HSOUNDSCRIPTHASH& handle );

	virtual char const		*GetSoundNameForHash( unsigned int hash ) const; // Returns NULL if hash not found!!!
	virtual int 			GetSoundIndexForHash( unsigned int hash ) const;
	virtual unsigned int	HashSoundName( char const *pchSndName ) const;
	virtual bool			IsValidHash( unsigned int hash ) const;

	// Called from both client and server (single player) or just one (server only in dedicated server and client only if connected to a remote server)
	// Called by LevelInitPreEntity to override sound scripts for the mod with level specific overrides based on custom mapnames, etc.
	virtual void			AddSoundOverrides( char const *scriptfile );

	// Called by either client or server in LevelShutdown to clear out custom overrides
	virtual void			ClearSoundOverrides();

	virtual void			DescribeSound( char const *soundname );
	virtual void			Flush();

	virtual void			AddSoundsFromFile( const char *filename, bool bPreload, bool bAutoCache, bool bIsOverride = false );

private:
	bool InitSoundInternalParameters( const char *soundname, KeyValues *kv, CSoundParametersInternal& params );
	bool LoadGameSoundManifest();
	void ShutdownSounds();
	void LoadGlobalActors();

	float	TranslateAttenuation( const char *key );
	soundlevel_t	TranslateSoundLevel( const char *key );
	int TranslateChannel( const char *name );

	int		FindBestSoundForGender( SoundFile *pSoundnames, int c, gender_t gender, int &nRandomSeed );
	void	EnsureAvailableSlotsForGender( SoundFile *pSoundnames, int c, gender_t gender );
	void	AddSoundName( CSoundParametersInternal& params, char const *wavename, gender_t gender );

	void	AddHash( char const *pchSoundName, int nIndex );
	void	RemoveHash( char const *pchSoundName );

	CUtlDict< gender_t, uint8 >					m_ActorGenders;
	CUtlVector< CSoundEntry * >	m_Sounds;

    CUtlVector< CSoundEntry * >			m_SavedOverrides; 
	CUtlVector< FileNameHandle_t >				m_OverrideFiles;

	struct CSoundScriptFile
	{
		FileNameHandle_t hFilename;
		bool		dirty;
	};

	CUtlVector< CSoundScriptFile >	m_SoundKeyValues;
	int					m_nInitCount;
	unsigned int		m_uManifestPlusScriptChecksum;

	CUtlSymbolTable		m_Waves;
	// This is a reverse mapping from crc of the soundname to the sound entry
	struct soundEntryHash_t
	{
		int soundIndex;
		CSoundEntry *pEntry;
	};

	CUtlMap< HSOUNDSCRIPTHASH, soundEntryHash_t >	m_HashToSoundEntry;

};

#endif // SOUNDEMITTERSYSTEMBASE_H
