//====== Copyright © Valve Corporation, All rights reserved. =================
//
// Purpose:
//
//=============================================================================
#include "cbase.h"
#include "cs_gameplay_hints.h"
#include "keyvalues.h"
#include "fmtstr.h"
#include "filesystem.h"
#include "vgui/ILocalize.h"
#include "gametypes/igametypes.h"
#include "cs_gamerules.h"

struct CSGameplayHint_t
{
	CSGameplayHint_t( const char* pszToken, uint32 flags ): 
	m_pszLocToken( pszToken ), m_nRequiredContextFlags( flags ), m_nDisplayCount( 0 ) {}
	const char* m_pszLocToken;
	uint32 m_nRequiredContextFlags;
	uint32 m_nDisplayCount;
};

CCSGameplayHints::CCSGameplayHints()
{
	m_pHintKV = NULL;
}

CCSGameplayHints::~CCSGameplayHints()
{
	Cleanup();
}

CCSGameplayHints g_CSGameplayHints;


uint32 CCSGameplayHints::GetCurrentContextFlags( void )
{
	uint32 flags = 0;

	if ( CSGameRules() && CSGameRules()->IsBombDefuseMap() )
		flags |= HINT_CONTEXT_BOMB_MAP;
	if ( CSGameRules() && CSGameRules()->IsHostageRescueMap() )
		flags |= HINT_CONTEXT_HOSTAGE_MAP;
	if ( CSGameRules() && CSGameRules()->IsPlayingGunGame() )
		flags |= HINT_CONTEXT_GUNGAME;

	return flags;
}

uint32 ContextEntryToBitFlag( const char* szName )
{
	if ( V_stricmp( szName, "bomb_map_only" ) == 0 )
	{
		return CCSGameplayHints::HINT_CONTEXT_BOMB_MAP;
	}
	else if ( V_stricmp( szName, "hostage_map_only" ) == 0 )
	{
		return CCSGameplayHints::HINT_CONTEXT_HOSTAGE_MAP;
	}
	else if ( V_stricmp( szName, "gungame_map_only" ) == 0 )
	{
		return CCSGameplayHints::HINT_CONTEXT_GUNGAME;
	}
	else
	{
		Warning( "HintConfig.txt contains invalid context setting: %s\n", szName );
		Assert( 0 );
	}

	// Show it as a failsafe.
	return CCSGameplayHints::HINT_CONTEXT_ALWAYS_SHOW; 
}

uint32 BuildContextFlags( KeyValues *pContextKeys )
{
	if ( pContextKeys == NULL || pContextKeys->GetFirstSubKey() == NULL )
		return CCSGameplayHints::HINT_CONTEXT_ALWAYS_SHOW; 

	uint32 nFlags = 0;
	for ( KeyValues *entry = pContextKeys->GetFirstSubKey(); entry != NULL; entry = entry->GetNextKey() )
	{
		if ( entry->GetInt() > 0 )
		{
			nFlags |= ContextEntryToBitFlag( entry->GetName() );
		}
	}
	return nFlags;
}

void CCSGameplayHints::PostInit()
{
	KeyValues *m_pHintKV = new KeyValues( "HintConfig.txt" );
	if ( m_pHintKV->LoadFromFile( g_pFullFileSystem, "resource/HintConfig.txt" ) )
	{
		KeyValues *hints = m_pHintKV->FindKey( "hints" );
		if ( hints )
		{
			for ( KeyValues *entry = hints->GetFirstSubKey(); entry != NULL; entry = entry->GetNextKey() )
			{
				const char* szLocToken = entry->GetString( "locToken", NULL );
				if ( szLocToken )
				{
					const wchar_t *wszText = g_pVGuiLocalize->Find( szLocToken );
					Assert( wszText );
					// Sanity check length here
					if ( wszText )
					{
						uint32 nContextFlags = BuildContextFlags( entry->FindKey( "context", NULL ) );
						m_HintList.AddToTail( new CSGameplayHint_t( szLocToken, nContextFlags ) );
					}
					else
					{
						Warning( "Bad localization token in resource/HintConfig.txt: '%s'\n", szLocToken );
					}
				}
			}
		}
	}
	
	Assert( m_HintList.Count() > 0 );
 }

void CCSGameplayHints::Cleanup( void )
{
	m_HintList.PurgeAndDeleteElements();
	if ( m_pHintKV )
	{
		m_pHintKV->deleteThis();
		m_pHintKV = NULL;
	}
}
void CCSGameplayHints::Shutdown()
{
	Cleanup();
}

int CompareHintsByDisplayCount( CSGameplayHint_t* const* lhs, CSGameplayHint_t* const* rhs )
{
	if ( (*lhs)->m_nDisplayCount > (*rhs)->m_nDisplayCount )
		return 1;
	else if ( (*lhs)->m_nDisplayCount < (*rhs)->m_nDisplayCount )
		return -1;
	return 0;
}

const char* CCSGameplayHints::GetRandomLeastPlayedHint( void )
{
	if ( m_HintList.Count() == 0 )
		return NULL;

	// Sort by times 'used' and pick among the lowest counts. Counts aren't stored and will 
	// re-zero every time this the CCSGameplayHints class is created (so, every app launch).
	m_HintList.InPlaceQuickSort( CompareHintsByDisplayCount );

	uint32 contextFlags = GetCurrentContextFlags();

	CUtlVector<CSGameplayHint_t*> validHints;
	// Filter out based on current context flags
	FOR_EACH_VEC( m_HintList, i )
	{
		CSGameplayHint_t* hint = m_HintList[i];

		// If bits are set for required context, make sure all of them are currently met. 
		if ( hint->m_nRequiredContextFlags == HINT_CONTEXT_ALWAYS_SHOW ||
			 ( contextFlags & hint->m_nRequiredContextFlags ) == hint->m_nRequiredContextFlags )
		{
			// Early out if we found at least one valid hint and we've moved 
			// on to checking hints that have been displayed more often.
			// After the sort, the first found valid hint should have the lowest display count.
			if ( validHints.Count() > 0 && validHints[0]->m_nDisplayCount < hint->m_nDisplayCount )
				break;

			validHints.AddToTail( hint );
		}
	}

	int pick = RandomInt( 0, validHints.Count()-1 );
	CSGameplayHint_t *hint = validHints[pick];
	hint->m_nDisplayCount++;
	return hint->m_pszLocToken;
}
/*
CON_COMMAND( pick_hint, "" )
{
	Msg( "Hint: '%s'\n", g_CSGameplayHints.GetRandomLeastPlayedHint() );
}
*/
