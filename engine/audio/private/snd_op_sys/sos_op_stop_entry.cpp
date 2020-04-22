//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "snd_dma.h"

#include "sos_op.h"
#include "sos_entry_match_system.h"
#include "sos_op_stop_entry.h"



// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar snd_sos_show_operator_stop_entry("snd_sos_show_operator_stop_entry", "0", FCVAR_CHEAT );

extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;



//-----------------------------------------------------------------------------
// CSosOperatorStopEntry
// 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorStopEntry, "sys_stop_entries" )
	SOS_REGISTER_INPUT_FLOAT( CSosOperatorStopEntry, m_flInputMaxVoices, SO_SINGLE, "input_max_entries" )
	SOS_REGISTER_INPUT_FLOAT( CSosOperatorStopEntry, m_flInputStopDelay, SO_SINGLE, "input_stop_delay" )
	SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorStopEntry, m_flOutputVoicesMatching, SO_SINGLE, "output_entries_matching" )
	SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorStopEntry, m_flOutputIndexOfThis, SO_SINGLE, "output_this_matches_index" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorStopEntry, "sys_stop_entries" )

void CSosOperatorStopEntry::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorStopEntry_t *pStructMem = (CSosOperatorStopEntry_t *)pVoidMem;
	V_strncpy( pStructMem->m_nMatchEntryName, "", sizeof(pStructMem->m_nMatchEntryName) );
	V_strncpy( pStructMem->m_nMatchSoundName, "", sizeof(pStructMem->m_nMatchSoundName) );
	pStructMem->m_bMatchEntry = false;
	pStructMem->m_bMatchSound = false;
	pStructMem->m_bMatchEntity = false;
	pStructMem->m_bMatchChannel = false;
	pStructMem->m_bMatchSubString = false;
	pStructMem->m_bStopOldest = true;
	pStructMem->m_bStopThis = false;
	pStructMem->m_bMatchThisEntry = false;
	pStructMem->m_bInvertMatch = false;

	SOS_INIT_INPUT_VAR( m_flInputMaxVoices, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInputStopDelay, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutputVoicesMatching, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutputIndexOfThis, SO_SINGLE, 0.0 )

}

static int __cdecl ChannelLongestElapsedTimeSortFunc( const int *nChannelIndexA, const int *nChannelIndexB )
{
	return ( S_GetElapsedTimeByGuid( channels[ *nChannelIndexA ].guid ) > S_GetElapsedTimeByGuid( channels[ *nChannelIndexB ].guid ) );
}

// static int __cdecl ChannelLeastVolumeSortFunc( const int *nChannelIndexA, const int *nChannelIndexB )
// {
// 	return ( S_GetElapsedTimeByGuid( channels[ *nChannelIndexA ]. ) < S_GetElapsedTimeByGuid( channels[ *nChannelIndexB ].guid ) );
// }

void CSosOperatorStopEntry::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{

	CSosOperatorStopEntry_t *pStructMem = (CSosOperatorStopEntry_t *)pVoidMem;


	if( pStructMem->m_bStopThis )
	{
		if( !pChannel )
		{
			Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s requires valid channel pointer, being called without one\n", pStack->GetOperatorName( nOpIndex ));
			return;
		}
		if ( snd_sos_show_operator_stop_entry.GetInt() )
		{
			Print( pVoidMem, pStack, nOpIndex, 0 );
			Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sStopping: %s : elapsed time: %f.2\n", 1, "    ", g_pSoundEmitterSystem->GetSoundNameForHash( pChannel->m_nSoundScriptHash ), S_GetElapsedTime( pChannel ) * .001 );
		}
		g_pSoundOperatorSystem->QueueStopChannel( pChannel->guid, pStructMem->m_flInputStopDelay[0] );
		return;
	}

	if( pScratchPad->m_nSoundScriptHash == SOUNDEMITTER_INVALID_HASH &&
		pStructMem->m_bMatchThisEntry )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s scratchpad has invalid script hash, \"match_this_entry\" set to false\n", pStack->GetOperatorName( nOpIndex ));
		pStructMem->m_bMatchThisEntry = false;
	}


	CSosEntryMatch sosEntryMatch;
	sosEntryMatch.m_bMatchString1 = pStructMem->m_bMatchEntry;
	V_strncpy( sosEntryMatch.m_nMatchString1, pStructMem->m_nMatchEntryName, sizeof( sosEntryMatch.m_nMatchString1 ) );

	sosEntryMatch.m_bMatchString2 = pStructMem->m_bMatchSound;
	V_strncpy( sosEntryMatch.m_nMatchString2, pStructMem->m_nMatchSoundName, sizeof( sosEntryMatch.m_nMatchString2 ) );

	sosEntryMatch.m_bMatchSubString = pStructMem->m_bMatchSubString;

	sosEntryMatch.m_bMatchInt1 = pStructMem->m_bMatchChannel;
	sosEntryMatch.m_nMatchInt1 = pScratchPad->m_nChannel;

	sosEntryMatch.m_bMatchInt2 = pStructMem->m_bMatchEntity;
	sosEntryMatch.m_nMatchInt2 = pScratchPad->m_nSoundSource;

	sosEntryMatch.m_bMatchInt3 = pStructMem->m_bMatchThisEntry;
	sosEntryMatch.m_nMatchInt3 = pScratchPad->m_nSoundScriptHash;

	CUtlVector< int > vMatchingIndices;
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	int nThisIndex = 0;

	for ( int i = 0; i < list.Count(); i++ )
	{
		sosEntryMatch.m_bMatchString1 = pStructMem->m_bMatchEntry;

		int ch_idx = list.GetChannelIndex(i);
		// skip uninitiated entries (like this one!)
		if ( ! channels[ch_idx].sfx )
		{
			continue;
		}
		if ( channels[ch_idx].sfx->pSource && channels[ch_idx].sfx->pSource->GetType() == CAudioSource::AUDIO_SOURCE_VOICE )
		{
			continue;
		}

		if( pChannel && &channels[ch_idx] == pChannel )
		{
			continue;
		}

		bool bIsAMatch = false;
		bool bInvertMatch = false;
		// this should probably explicitly check for soundentry_version
		if( channels[ch_idx].m_nSoundScriptHash == SOUNDEMITTER_INVALID_HASH && pStructMem->m_bMatchEntry )
		{
			if( pStructMem->m_bInvertMatch )
			{
				bInvertMatch = true;
			}
			else
			{
				continue;
			}
		}

		if( !bInvertMatch )
		{
			CSosEntryMatch sosChanMatch;

			if ( sosEntryMatch.m_bMatchString1 && channels[ch_idx].m_nSoundScriptHash != SOUNDEMITTER_INVALID_HASH )
			{
				const char *pEntryName = g_pSoundEmitterSystem->GetSoundNameForHash( channels[ch_idx].m_nSoundScriptHash );
				if ( !pEntryName )
				{
					pEntryName = "";
				}

				V_strncpy( sosChanMatch.m_nMatchString1, pEntryName, sizeof( sosChanMatch.m_nMatchString1 ) );
			}
			else
			{
				sosEntryMatch.m_bMatchString1 = false;
			}
			if ( sosEntryMatch.m_bMatchString2 )
			{
				channels[ch_idx].sfx->GetFileName( sosChanMatch.m_nMatchString2, sizeof( sosChanMatch.m_nMatchString2 ) );
			}
			sosChanMatch.m_nMatchInt1 = channels[ch_idx].entchannel;
			sosChanMatch.m_nMatchInt2 = channels[ch_idx].soundsource;
			sosChanMatch.m_nMatchInt3 = channels[ch_idx].m_nSoundScriptHash;

			bIsAMatch = sosEntryMatch.IsAMatch( &sosChanMatch );
			bIsAMatch = ( bIsAMatch && !pStructMem->m_bInvertMatch ) || ( !bIsAMatch && pStructMem->m_bInvertMatch );
		}

		
		if ( bIsAMatch || bInvertMatch )
		{
			int nNewIndex  = vMatchingIndices.AddToTail( ch_idx );
			if( pChannel && &channels[ ch_idx ] == pChannel )
			{
				nThisIndex = nNewIndex;
			}
		}
	}

	pStructMem->m_flOutputVoicesMatching[0] = ( float ) vMatchingIndices.Count();
	pStructMem->m_flOutputIndexOfThis[0] = ( float ) nThisIndex;


 	if ( vMatchingIndices.Count() > (int) pStructMem->m_flInputMaxVoices[0] )
	{
		if( pStructMem->m_bStopOldest )
		{
			vMatchingIndices.Sort( &ChannelLongestElapsedTimeSortFunc );
		}
		for( int i = (int) pStructMem->m_flInputMaxVoices[0]; i < vMatchingIndices.Count(); i++ )
		{
			if ( snd_sos_show_operator_stop_entry.GetInt() )
			{
				Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sStopping: %s : elapsed time: %f.2\n", 1, "    ", g_pSoundEmitterSystem->GetSoundNameForHash( channels[ vMatchingIndices[ i ] ].m_nSoundScriptHash ), S_GetElapsedTime( &channels[ vMatchingIndices[ i ]  ] ) * .001 );
			}

			g_pSoundOperatorSystem->QueueStopChannel( channels[ vMatchingIndices[ i ] ].guid, pStructMem->m_flInputStopDelay[0] );

		}
	}
}

void CSosOperatorStopEntry::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorStopEntry_t *pStructMem = (CSosOperatorStopEntry_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
// 	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sNumber Allowed: %i\n", nLevel, "    ", pStructMem->m_nNumAllowed );

	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*smatch_entry: %s\n", nLevel, "    ", pStructMem->m_bMatchEntry ? pStructMem->m_nMatchEntryName : "\"\"" );	
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*smatch_sound: %s\n", nLevel, "    ", pStructMem->m_bMatchSound ? pStructMem->m_nMatchSoundName : "\"\"" );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*smatch_entity: %s\n", nLevel, "    ", pStructMem->m_bMatchEntity ? "true" : "false" );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*smatch_channel: %s\n", nLevel, "    ", pStructMem->m_bMatchEntity ? "true" : "false" );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*smatch_substring: %s\n", nLevel, "    ", pStructMem->m_bMatchSubString ? "true" : "false" );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*smatch_this_entry: %s\n", nLevel, "    ", pStructMem->m_bMatchThisEntry ? "true" : "false" );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sstop_oldest: %s\n", nLevel, "    ", pStructMem->m_bStopOldest ? "true" : "false" );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sinvert_match: %s\n", nLevel, "    ", pStructMem->m_bInvertMatch ? "true" : "false" );

}
void CSosOperatorStopEntry::OpHelp( ) const
{

}

void CSosOperatorStopEntry::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorStopEntry_t *pStructMem = (CSosOperatorStopEntry_t *)pVoidMem;

	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ) )
				{

				}
				else if ( !V_strcasecmp( pParamString, "match_this_entry" ) )
				{
					if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bMatchThisEntry = true;
					}
					else
					{
						pStructMem->m_bMatchChannel = false;
					}
					
				}
				else if ( !V_strcasecmp( pParamString, "match_entry" ) )
				{
					pStructMem->m_bMatchEntry = true;
					V_strncpy( pStructMem->m_nMatchEntryName, pValueString, sizeof(pStructMem->m_nMatchEntryName) );
				}
				else if ( !V_strcasecmp( pParamString, "match_sound" ) )
				{
					pStructMem->m_bMatchSound = true;
					V_strncpy( pStructMem->m_nMatchSoundName, pValueString, sizeof(pStructMem->m_nMatchSoundName) );
				}
				else if ( !V_strcasecmp( pParamString, "match_entity" ) )
				{
					if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bMatchEntity = true;
					}
					else
					{
						pStructMem->m_bMatchEntity = false;
					}
				}
				else if ( !V_strcasecmp( pParamString, "match_channel" ) )
				{
					if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bMatchChannel = true;
					}
					else
					{
						pStructMem->m_bMatchChannel = false;
					}
				}
				else if ( !V_strcasecmp( pParamString, "match_substring" ) )
				{
					if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bMatchSubString = true;
					}
					else
					{
						pStructMem->m_bMatchSubString = false;
					}
				}
				else if ( !V_strcasecmp( pParamString, "stop_oldest" ) )
				{
					if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bStopOldest = true;
					}
					else
					{
						pStructMem->m_bStopOldest = false;
					}
				}
				else if ( !V_strcasecmp( pParamString, "stop_this_entry" ) )
				{
					if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bStopThis = true;
					}
					else
					{
						pStructMem->m_bStopThis = false;
					}
				}
				else if ( !V_strcasecmp( pParamString, "invert_match" ) )
				{
					if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bInvertMatch = true;
					}
					else
					{
						pStructMem->m_bInvertMatch = false;
					}
				}
// 				else if ( !V_strcasecmp( pParamString, "num_allowed" ) )
// 				{
// 					pStructMem->m_nNumAllowed = V_atoi( pValueString );
// 				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}


			}
		}
		pParams = pParams->GetNextKey();
	}
}
