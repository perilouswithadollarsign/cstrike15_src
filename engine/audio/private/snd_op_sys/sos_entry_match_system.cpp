 //============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================
#include "audio_pch.h"

#include "sos_system.h"
#include "sos_entry_match_system.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar snd_sos_show_block_debug;
ConVar snd_sos_show_entry_match_free( "snd_sos_show_entry_match_free", "0" );

//-----------------------------------------------------------------------------
// CSosEntryMatch
//-----------------------------------------------------------------------------
bool CSosEntryMatch::IsAMatch( CSosEntryMatch *pEntryMatch )
{

	bool bMatchString1 = true;
	bool bMatchString2 = true;
	bool bMatchInt1 = true;
	bool bMatchInt2 = true;
	bool bMatchInt3 = true;


	if ( m_bMatchString1  )
	{
		if( m_bMatchSubString )
		{
			if ( V_stristr( pEntryMatch->m_nMatchString1, m_nMatchString1 ) == NULL )
			{
				bMatchString1 = false;
			}
		}
		else
		{
			// optimize to be scripthandle
			if ( V_stricmp( pEntryMatch->m_nMatchString1, m_nMatchString1 ) )
			{
				bMatchString1 = false;
			}
		}
	}

	if ( m_bMatchString2 )
	{
		if ( m_bMatchSubString )
		{
			if ( V_stristr( pEntryMatch->m_nMatchString2, m_nMatchString2 ) == NULL )
			{
				bMatchString2 = false;
			}
		}
		else
		{
			// optimize to be scripthandle
			if ( V_stricmp( pEntryMatch->m_nMatchString2, m_nMatchString2 ) )
			{
				bMatchString2 = false;
			}
		}
	}
	if ( m_bMatchInt1 )
	{
		if ( pEntryMatch->m_nMatchInt1 != m_nMatchInt1 )
		{
			bMatchInt1 = false;
		}
	}
	if ( m_bMatchInt2 )
	{
		if ( pEntryMatch->m_nMatchInt2 != m_nMatchInt2 )
		{
			bMatchInt2 = false;
		}
	}
	if ( m_bMatchInt3 )
	{
		if ( pEntryMatch->m_nMatchInt3 != m_nMatchInt3 )
		{
			bMatchInt3 = false;
		}
	}

	if ( ( ( bMatchString1 && m_bMatchString1 ) || !m_bMatchString1 ) && 
		( ( bMatchString2 && m_bMatchString2 ) || !m_bMatchString2 )&&
		( ( bMatchInt1 && m_bMatchInt1 ) || !m_bMatchInt1 ) &&
		( ( bMatchInt2 && m_bMatchInt2 ) || !m_bMatchInt2 ) &&
		( ( bMatchInt3 && m_bMatchInt3 ) || !m_bMatchInt3 ) )
	{
		return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// CSosEntryMatchList
//-----------------------------------------------------------------------------

void CSosEntryMatchList::Update()
{
	uint32 bit = 1;
	for ( int i = 0; i < SOS_BLOCKSYS_MAX_ENTRIES; i++, ( bit <<= 1 ) )
	{
		if ( !( m_Free & bit ) )
		{
			Assert( !m_vEntryMatchList[i]->m_bFree );
			if(  m_vEntryMatchList[i]->m_bTimed )
			{
				if ( m_vEntryMatchList[i]->m_flDuration > -1.0 && m_vEntryMatchList[i]->m_flStartTime > -1.0 )
				{
					float flCurTime = g_pSoundServices->GetClientTime();
					if ( ( m_vEntryMatchList[i]->m_flDuration + m_vEntryMatchList[i]->m_flStartTime ) <= flCurTime )
					{
						if( snd_sos_show_entry_match_free.GetInt() )
						{
							Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nFREEING MATCH ENTRY:\n");
							m_vEntryMatchList[i]->Print();

						}
						m_vEntryMatchList[i]->Reset();
						m_Free |= bit;
					}
				}
				else
				{

					if( snd_sos_show_entry_match_free.GetInt() )
					{
						Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nFREEING MATCH ENTRY:\n");
						m_vEntryMatchList[i]->Print();
					}
					m_vEntryMatchList[i]->Reset();
					m_Free |= bit;
				}
			}
			if( snd_sos_show_block_debug.GetInt() )
			{
				m_vEntryMatchList[i]->Print();
			}
		}
	}
}

int CSosEntryMatchList::GetFreeEntryIndex() const
{
	uint32 bit = 1;
	for( int i = 0; i < SOS_BLOCKSYS_MAX_ENTRIES; i++, ( bit <<= 1 ) )
	{
		if ( m_vEntryMatchList[i]->m_bFree )
		{
			Assert( m_Free & bit );
			m_vEntryMatchList[i]->m_bFree = false;
			m_Free &= ~bit;
			return i;
		}
	}
	return -1;
}

CSosManagedEntryMatch *CSosEntryMatchList::GetEntryFromIndex( int nIndex ) const
{
	Assert( IsValidIndex( nIndex ) );

	if ( IsValidIndex( nIndex ) )
	{
		return m_vEntryMatchList[ nIndex ];
	}
	else
	{
		return NULL;
	}
}
CSosManagedEntryMatch *CSosEntryMatchList::GetFreeEntry( int &nIndex ) const
{
	int nMatchEntryIndex = g_pSoundOperatorSystem->m_sosEntryBlockList.GetFreeEntryIndex();

	CSosManagedEntryMatch *pSosEntryMatch = g_pSoundOperatorSystem->m_sosEntryBlockList.GetEntryFromIndex( nMatchEntryIndex );
	if ( !pSosEntryMatch )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: EntryMatchList has no free slots!\n" );
		nIndex = -1;
		return NULL;
	}
	return pSosEntryMatch;

}
void CSosEntryMatchList::FreeEntry( int nIndex, bool bForce /* = false */ )
{
	if ( !m_vEntryMatchList[ nIndex ]->m_bTimed || bForce )
	{
		m_vEntryMatchList[ nIndex ]->Reset();
		m_Free |= GetBitForBitnum( nIndex );
	}
}

bool CSosEntryMatchList::HasAMatch( CSosEntryMatch *pEntryMatch ) const
{
	uint32 bit = 1;
	for( int i = 0; i < SOS_BLOCKSYS_MAX_ENTRIES; i++, (bit <<= 1) )
	{
		if ( !( m_Free & bit ) && m_vEntryMatchList[i]->m_bActive )
		{
			Assert( !m_vEntryMatchList[i]->m_bFree );
			if ( m_vEntryMatchList[i]->IsAMatch( pEntryMatch ) )
			{
				return true;
			}
		}
	}
	return false;
}
void CSosEntryMatchList::Print() const
{
	Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, StackColor, "\nCUE Operators:\n");
}