//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Implementation of China Government Censorship enforced on all user-generated strings
//
//=============================================================================

//
// Pre-compiled header (cbase.h in the game branch; none in Steam Client branch)
//
#include "cbase.h"

//
// Custom implementations for sharing code verbatim between Steam Client and the game branch
//
#ifdef CSTRIKE15
static bool BannedWords_LoadFileIntoBuffer( char const *szFilename, CUtlBuffer &buf )
{
	return g_pFullFileSystem->ReadFile( szFilename, "MOD", buf );
}
#else
#include "utlbuffer.h"
#include "utlmap.h"
#include "filesystem.h"
#include "filesystem_helpers.h"
#include "tier1/fileio.h"
static bool BannedWords_LoadFileIntoBuffer( char const *szFilename, CUtlBuffer &buf )
{
	return LoadFileIntoBuffer( szFilename, buf, false );
}
#endif

#include "bannedwords.h"
#include "utlmemory.h"
#include "utlbuffer.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//////////////////////////////////////////////////////////////////////////
//
// Banned words dictionary
//
//////////////////////////////////////////////////////////////////////////

class CBannedWordsDictionary
{
public:
	~CBannedWordsDictionary() { m_mapExternalStrings.PurgeAndDeleteElements(); }
	bool InitFromFile( char const *szFilename );
	int CensorBannedWordsInplace( wchar_t *wsz ) const;
	char const * CensorExternalString( uint64 ullKey, char const *szExternalString );

public:
	CUtlBuffer m_buf;
	struct ExternalStringCache_t
	{
		char m_chExternalString[256];
		char m_chCensoredString[256];
	};
	typedef CUtlMap< uint64, ExternalStringCache_t *, int, CDefLess< uint64 > > KeyStringMap_t;
	KeyStringMap_t m_mapExternalStrings;
};

bool CBannedWordsDictionary::InitFromFile( char const *szFilename )
{
	if ( !BannedWords_LoadFileIntoBuffer( szFilename, m_buf ) )
		return false;
	if ( m_buf.TellPut() % 2 )
		return false;
	if ( m_buf.TellPut() <= 0x10000 )
		return false;
	if ( V_memcmp( m_buf.Base(), "BDR1", 4 ) )
		return false;
	return true;
}

int CBannedWordsDictionary::CensorBannedWordsInplace( wchar_t *wsz ) const
{
	wchar_t const * const wszStartOfInput = wsz;
	int numReplaced = 0;
	for ( ; *wsz; ++ wsz )
	{
		wchar_t wchThisLetter = *wsz;
		if ( ( wchThisLetter >= 'A' ) && ( wchThisLetter <= 'Z' ) )
			wchThisLetter += 'a' - 'A'; // ensure input is also lowercase

		int32 nOffset = reinterpret_cast< int32 const * >( m_buf.Base() )[ wchThisLetter ];
		if ( !nOffset ) continue; // no banned words start with this char
		for ( wchar_t const *pwchBan = ( wchar_t const * ) ( ( byte const * )( m_buf.Base() ) + nOffset );
			pwchBan[1] == wchThisLetter; pwchBan += 1 + *pwchBan + 1 ) // (len wchar) + (actual number of wchars) + wnull-terminator
		{
			bool bWordBanned = true;
			bool bWordIsAllAlpha = true;
			{	// if ( wcsncmp( wsz, pwchBan + 1, *pwchBan ) ) continue;
				wchar_t const *x = wsz;
				wchar_t const *y = pwchBan + 1; // dictionary word, compiled as lowercase
				for ( wchar_t numChecksRemaining = *pwchBan;
					numChecksRemaining -- > 0;
					++ x, ++ y )
				{
					wchar_t wchx = *x;
					if ( ( wchx >= 'A' ) && ( wchx <= 'Z' ) )
						wchx += 'a' - 'A'; // ensure input is also lowercase
					if ( wchx != *y )
					{
						bWordBanned = false;
						break;
					}
					if ( !( ( wchx >= 'a' ) && ( wchx <= 'z' ) ) )
						bWordIsAllAlpha = false;
				}
			}
			if ( bWordBanned && bWordIsAllAlpha )
			{
				bool bBannedSequenceStartsWord = ( ( wsz <= wszStartOfInput ) || ( wsz[ -1 ] >= 0xFF ) || !V_isalpha( wsz[ -1 ] ) );
				bool bBannedSequenceEndsWord = ( !wsz[ *pwchBan ] || ( wsz[ *pwchBan ] >= 0xFF ) || !V_isalpha( wsz[ *pwchBan ] ) );

				// Must match the full word, not substring in English word (otherwise banned words like
				// "ri", "mb", "sm" cause censoring all around)
				if ( *pwchBan < 4 )
					bWordBanned = bBannedSequenceStartsWord && bBannedSequenceEndsWord;
				// Otherwise require that the banned word appears at the start or end of a word
				// so that it censored words like "bullshit" or "shitshow", but didn't censor
				// pro player name "pashaBiceps" containing "shabi"
				else
					bWordBanned = bBannedSequenceStartsWord || bBannedSequenceEndsWord;
			}
			if ( !bWordBanned )
				continue;
			// BANNED WORD!
			for ( int kk = *pwchBan; kk -- > 0; ++ wsz )
			{
				*wsz = L'*';
				++ numReplaced;
			}
			-- wsz; // already advanced by number of asterisks inserted (-1 because the loop will ++)
			break;
		}
	}
	return numReplaced;
}

char const * CBannedWordsDictionary::CensorExternalString( uint64 ullKey, char const *szExternalString )
{
	KeyStringMap_t::IndexType_t idx = m_mapExternalStrings.Find( ullKey );
	if ( idx == m_mapExternalStrings.InvalidIndex() )
	{
		ExternalStringCache_t *pNewEntry = new ExternalStringCache_t;
		V_memset( pNewEntry, 0, sizeof( ExternalStringCache_t ) );
		idx = m_mapExternalStrings.InsertOrReplace( ullKey, pNewEntry );
	}

	ExternalStringCache_t *pEntry = m_mapExternalStrings.Element( idx );
	if ( V_strcmp( pEntry->m_chExternalString, szExternalString ) )
	{
		V_strcpy_safe( pEntry->m_chExternalString, szExternalString );
		
		wchar_t *wch = ( wchar_t * ) stackalloc( sizeof(pEntry->m_chExternalString)*sizeof( wchar_t ) );
		V_UTF8ToUnicode( pEntry->m_chExternalString, wch, sizeof(pEntry->m_chExternalString)*sizeof( wchar_t ) );
		if ( CensorBannedWordsInplace( wch ) )
			V_UnicodeToUTF8( wch, pEntry->m_chCensoredString, sizeof( pEntry->m_chCensoredString ) );
		else
			V_strcpy_safe( pEntry->m_chCensoredString, pEntry->m_chExternalString );
	}

	return pEntry->m_chCensoredString;
}


//////////////////////////////////////////////////////////////////////////
//
// Banned words interface exposed to clients
//
//////////////////////////////////////////////////////////////////////////

CBannedWords::~CBannedWords()
{
	delete m_pDictionary;
	m_pDictionary = NULL;
}

bool CBannedWords::InitFromFile( char const *szFilename )
{
	CBannedWordsDictionary *pDictionary = new CBannedWordsDictionary;
	if ( pDictionary->InitFromFile( szFilename ) )
	{
		delete m_pDictionary;
		m_pDictionary = pDictionary;
		return true;
	}
	else
	{
		delete pDictionary;
		return false;
	}
}

int CBannedWords::CensorBannedWordsInplace( wchar_t *wsz ) const
{
	return ( m_pDictionary && wsz && *wsz ) ? m_pDictionary->CensorBannedWordsInplace( wsz ) : 0;
}

int CBannedWords::CensorBannedWordsInplace( char *sz ) const
{
	if ( !m_pDictionary )
		return 0;

	if ( !sz || !*sz )
		return 0;

	int nLen = V_strlen( sz );
	wchar_t *wch = ( wchar_t * ) stackalloc( ( 1 + nLen )*sizeof( wchar_t ) );
	V_UTF8ToUnicode( sz, wch, ( 1 + nLen )*sizeof( wchar_t ) );
	int numCensored = m_pDictionary->CensorBannedWordsInplace( wch );
	if ( !numCensored )
		return 0;

	V_UnicodeToUTF8( wch, sz, nLen + 1 );
	return numCensored;
}

char const * CBannedWords::CensorExternalString( uint64 ullKey, char const *szExternalString ) const
{
	return ( m_pDictionary && szExternalString && *szExternalString ) ? m_pDictionary->CensorExternalString( ullKey, szExternalString ) : szExternalString;
}

CBannedWords g_BannedWords;


