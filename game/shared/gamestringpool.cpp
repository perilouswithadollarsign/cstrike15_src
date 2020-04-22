//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "stringpool.h"

#if !defined( GC )
#include "igamesystem.h"
#endif

#include "gamestringpool.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: The actual storage for pooled per-level strings
//-----------------------------------------------------------------------------
#if !defined( GC )
class CGameStringPool : public CStringPool,	public CBaseGameSystem
#else
class CGameStringPool : public CStringPool
#endif
{
	virtual char const *Name() { return "CGameStringPool"; }

	virtual void LevelShutdownPostEntity() 
	{
		FreeAll();
		PurgeDeferredDeleteList();
		CGameString::IncrementSerialNumber();
	}

public:
	~CGameStringPool()
	{
		PurgeDeferredDeleteList();
	}
	
	void PurgeDeferredDeleteList()
	{
		for ( int i = 0; i < m_DeferredDeleteList.Count(); ++ i )
		{
			free( ( void * )m_DeferredDeleteList[ i ] );
		}
		m_DeferredDeleteList.Purge();
	}

	void Dump( void )
	{
		for ( int i = m_Strings.FirstInorder(); i != m_Strings.InvalidIndex(); i = m_Strings.NextInorder(i) )
		{
			DevMsg( "  %d (0x%p) : %s\n", i, m_Strings[i], m_Strings[i] );
		}
		DevMsg( "\n" );
		DevMsg( "Size:  %d items\n", m_Strings.Count() );
	}

	void Remove( const char *pszValue )
	{
		int i = m_Strings.Find( pszValue );
		if ( i != m_Strings.InvalidIndex() )
		{
			m_DeferredDeleteList.AddToTail( m_Strings[ i ] );
			m_Strings.RemoveAt( i );
		}
	}

private:
	CUtlVector< const char * > m_DeferredDeleteList;
};

static CGameStringPool g_GameStringPool;


//-----------------------------------------------------------------------------
// String system accessor
//-----------------------------------------------------------------------------
#if !defined( GC )
IGameSystem *GameStringSystem()
{
	return &g_GameStringPool;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: The public accessor for the level-global pooled strings
//-----------------------------------------------------------------------------
string_t AllocPooledString( const char * pszValue )
{
	if (pszValue && *pszValue)
		return MAKE_STRING( g_GameStringPool.Allocate( pszValue ) );
	return NULL_STRING;
}

string_t FindPooledString( const char *pszValue )
{
	return MAKE_STRING( g_GameStringPool.Find( pszValue ) );
}

void RemovePooledString( const char *pszValue )
{
	g_GameStringPool.Remove( pszValue );
}

int CGameString::gm_iSerialNumber = 1;

#if !defined(CLIENT_DLL) && !defined( GC )
//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
void CC_DumpGameStringTable( void )
{
	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	g_GameStringPool.Dump();
}
static ConCommand dumpgamestringtable("dumpgamestringtable", CC_DumpGameStringTable, "Dump the contents of the game string table to the console.", FCVAR_CHEAT);
#endif
