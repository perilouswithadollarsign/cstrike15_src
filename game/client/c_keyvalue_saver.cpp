//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Client handler implementations for instruction players how to play
//
//=============================================================================//

#include "cbase.h"

#include "c_keyvalue_saver.h"
#include "filesystem.h"
#include "ixboxsystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// Key Value Saver auto game system instantiation
C_KeyValueSaver g_KeyValueSaver[ MAX_SPLITSCREEN_PLAYERS ];
C_KeyValueSaver &KeyValueSaver()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return g_KeyValueSaver[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}



// C_KeyValueSaver
//

bool C_KeyValueSaver::Init( void )
{
	// Make sure split slot is up to date
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		if ( &KeyValueSaver() == this )
		{
			SetSlot( i );
			break;
		}
	}

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	if ( !IsGameConsole() )
	{
		ListenForGameEvent( "round_end" );
		ListenForGameEvent( "map_transition" );
		ListenForGameEvent( "game_newmap" );
	}

	return true;
}

void C_KeyValueSaver::Shutdown( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	WriteAllDirtyKeyValues();

	for ( int i = 0; i < m_KeyValueData.Count(); ++i )
	{
		m_KeyValueData[ i ].pKeyValues->deleteThis();
		m_KeyValueData[ i ].pKeyValues = NULL;
	}

	m_KeyValueData.RemoveAll();

	// Stop listening for events
	StopListeningForAllEvents();
}

void C_KeyValueSaver::Update( float frametime )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	if ( IsGameConsole() )
	{
		// On X360 we want to save when they're not connected
		if ( !engine->IsInGame() )
		{
			// They aren't in game
			WriteAllDirtyKeyValues();
		}
		else
		{
			const char *levelName = engine->GetLevelName();
			if ( levelName && levelName[0] && engine->IsLevelMainMenuBackground() )
			{
				// The are in game, but it's a background map
				WriteAllDirtyKeyValues();
			}
		}
	}
}

void C_KeyValueSaver::FireGameEvent( IGameEvent *event )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	const char *name = event->GetName();

	if ( !IsGameConsole() )
	{
		if ( Q_strcmp( name, "round_end" ) == 0 || 
			 Q_strcmp( name, "map_transition" ) == 0 || 
			 Q_strcmp( name, "game_newmap" ) == 0 )
		{
			// Good place to save
			WriteAllDirtyKeyValues();
		}
	}
}

bool C_KeyValueSaver::InitKeyValues( const char *pchFileName, KeyValueBuilder funcKeyValueBuilder )
{
	KeyValueSaverData *pKeyValueData = FindKeyValueData( pchFileName );
	if ( pKeyValueData )
	{
		// Already got one by this name
		return false;
	}

	int nNew = m_KeyValueData.AddToTail();
	pKeyValueData = &(m_KeyValueData[ nNew ]);
	V_strcpy( pKeyValueData->szFileName, pchFileName );
	pKeyValueData->bDirtySaveData = false;
	pKeyValueData->pKeyValues = NULL;
	pKeyValueData->funcKeyValueBuilder = funcKeyValueBuilder;

	return true;
}

bool C_KeyValueSaver::WriteDirtyKeyValues( const char *pchFileName, bool bForceWrite /*= false*/ )
{
	return WriteDirtyKeyValues( FindKeyValueData( pchFileName ), bForceWrite );
}

KeyValues * C_KeyValueSaver::GetKeyValues( const char *pchFileName, bool bForceReread /*= false*/ )
{
	KeyValueSaverData *pKeyValueData = FindKeyValueData( pchFileName );

	if ( pKeyValueData )
	{
		if ( !pKeyValueData->pKeyValues  )
		{
			bForceReread = true;
		}

		if ( bForceReread )
		{
			if ( !ReadKeyValues( pKeyValueData ) )
			{
				return NULL;
			}
		}

		return pKeyValueData->pKeyValues;
	}

	return NULL;
}

void C_KeyValueSaver::MarkKeyValuesDirty( const char *pchFileName )
{
	KeyValueSaverData *pKeyValueData = FindKeyValueData( pchFileName );
	if ( !pKeyValueData )
		return;

	pKeyValueData->bDirtySaveData = true;
}

bool C_KeyValueSaver::ReadKeyValues( KeyValueSaverData *pKeyValueData )
{
#if !defined( CSTRIKE15 )
#ifdef _GAMECONSOLE
	DevMsg( "Read Game Instructor for splitscreen slot %d\n", m_nSplitScreenSlot );

	if ( m_nSplitScreenSlot < 0 )
		return false;

	if ( m_nSplitScreenSlot >= (int) XBX_GetNumGameUsers() )
		return false;
#endif

	char szFilename[_MAX_PATH];
	Q_snprintf( szFilename, sizeof( szFilename ), VarArgs( "save/%s", pKeyValueData->szFileName ) );
	if ( pKeyValueData->pKeyValues )
	{
		pKeyValueData->pKeyValues->deleteThis();
		pKeyValueData->pKeyValues = NULL;
	}

	pKeyValueData->pKeyValues = new KeyValues( "KeyValueSaverData" );

	if ( pKeyValueData->pKeyValues->LoadFromFile( g_pFullFileSystem, szFilename, NULL ) )
	{
		return true;
	}
#endif // !CSTRIKE15

	// Couldn't read from the file
	return false;
}

bool C_KeyValueSaver::WriteDirtyKeyValues( KeyValueSaverData *pKeyValueData, bool bForceWrite /*= false*/ )
{
	if ( engine->IsPlayingDemo() )
		return false;

	if ( !pKeyValueData )
		return false;

	if ( !pKeyValueData->bDirtySaveData && !bForceWrite )
		return true;

	// Always mark as clean state to avoid re-entry on
	// subsequent frames when storage device might be
	// in a yet-unmounted state.
	pKeyValueData->bDirtySaveData = false;

#ifdef _GAMECONSOLE
	DevMsg( "Write KeyValueSaver for splitscreen slot %d at time: %.1f\n", m_nSplitScreenSlot, Plat_FloatTime() );

	if ( m_nSplitScreenSlot < 0 )
		return false;

	if ( m_nSplitScreenSlot >= (int) XBX_GetNumGameUsers() )
		return false;
#endif

	// Build key value data to save
	if ( pKeyValueData->pKeyValues )
	{
		pKeyValueData->pKeyValues->deleteThis();
		pKeyValueData->pKeyValues = NULL;
	}

	pKeyValueData->pKeyValues = new KeyValues( "KeyValueSaverData" );

	// Build key values
	pKeyValueData->funcKeyValueBuilder( pKeyValueData->pKeyValues );

#if defined( CSTRIKE15 )
	// The key values are saved to the title data block in the callback above.
	return true;
#else
	// Save it!
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	pKeyValueData->pKeyValues->RecursiveSaveToFile( buf, 0 );

	char szFilename[_MAX_PATH];
	Q_snprintf( szFilename, sizeof( szFilename ), VarArgs( "save/%s", pKeyValueData->szFileName ) );
	filesystem->CreateDirHierarchy( "save", "MOD" );
	bool bWriteSuccess = filesystem->WriteFile( szFilename, "MOD", buf );
	return bWriteSuccess;
#endif
}

void C_KeyValueSaver::WriteAllDirtyKeyValues( void )
{
	for ( int i = 0; i < m_KeyValueData.Count(); ++i )
	{
		WriteDirtyKeyValues( &(m_KeyValueData[ i ]) );
	}
}

KeyValueSaverData * C_KeyValueSaver::FindKeyValueData( const char *pchFileName )
{
	for ( int i = 0; i < m_KeyValueData.Count(); ++i )
	{
		KeyValueSaverData *pKeyValueData = &(m_KeyValueData[ i ]);
		if ( V_strcmp( pKeyValueData->szFileName, pchFileName ) == 0 )
		{
			return pKeyValueData;
		}
	}

	return NULL;
}