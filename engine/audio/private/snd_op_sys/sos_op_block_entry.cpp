//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_entry_match_system.h"
#include "sos_op_block_entry.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;



//-----------------------------------------------------------------------------
// CSosOperatorBlockEntry
// 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorBlockEntry, "sys_block_entries" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorBlockEntry, m_flInputDuration, SO_SINGLE, "input_duration" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorBlockEntry, m_flInputActive, SO_SINGLE, "input_active" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorBlockEntry, "sys_block_entries" )


void CSosOperatorBlockEntry::StackShutdown( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex )
{
	CSosOperatorBlockEntry_t *pStructMem = (CSosOperatorBlockEntry_t *)pVoidMem;

	if( pStructMem->m_nEntryIndex > -1 )
	{
		//g_pSoundOperatorSystem->m_sosEntryBlockList.FreeEntry( pStructMem->m_nEntryIndex );
	}
}

void CSosOperatorBlockEntry::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorBlockEntry_t *pStructMem = (CSosOperatorBlockEntry_t *)pVoidMem;
	V_strncpy( pStructMem->m_nMatchEntryName, "", sizeof(pStructMem->m_nMatchEntryName) );
	V_strncpy( pStructMem->m_nMatchSoundName, "", sizeof(pStructMem->m_nMatchSoundName) );
	pStructMem->m_bMatchEntry = false;
	pStructMem->m_bMatchSound = false;
	pStructMem->m_bMatchEntity = false;
	pStructMem->m_bMatchChannel = false;
	pStructMem->m_bMatchSubString = false;
	pStructMem->m_nEntryIndex = -1;

	SOS_INIT_INPUT_VAR( m_flInputDuration, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputActive, SO_SINGLE, 1.0 )

}

void CSosOperatorBlockEntry::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	//	THREAD_LOCK_SOUND();

	CSosOperatorBlockEntry_t *pStructMem = (CSosOperatorBlockEntry_t *)pVoidMem;


	if( pStructMem->m_nEntryIndex < 0 )
	{

		int nMatchEntryIndex = g_pSoundOperatorSystem->m_sosEntryBlockList.GetFreeEntryIndex();

		CSosManagedEntryMatch *pSosEntryMatch = g_pSoundOperatorSystem->m_sosEntryBlockList.GetEntryFromIndex( nMatchEntryIndex );
		if( !pSosEntryMatch )
		{
			Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, EntryMatchList has no free slots!\n", pStack->GetOperatorName( nOpIndex ) );
			return;
		}

		pStructMem->m_nEntryIndex = nMatchEntryIndex;
		pSosEntryMatch->m_bMatchString1 = pStructMem->m_bMatchEntry;
		V_strncpy( pSosEntryMatch->m_nMatchString1, pStructMem->m_nMatchEntryName, sizeof( pSosEntryMatch->m_nMatchString1 ) );

// 		pSosEntryMatch->m_bMatchString2 = pStructMem->m_bMatchSound;
// 		V_strncpy( pSosEntryMatch->m_nMatchString2, pStructMem->m_nMatchSoundName, sizeof( pSosEntryMatch->m_nMatchString2 ) );

		pSosEntryMatch->m_bMatchSubString = pStructMem->m_bMatchSubString;

		pSosEntryMatch->m_bMatchInt1 = pStructMem->m_bMatchChannel;
		pSosEntryMatch->m_nMatchInt1 = pScratchPad->m_nChannel;

		pSosEntryMatch->m_bMatchInt2 = pStructMem->m_bMatchEntity;
		pSosEntryMatch->m_nMatchInt2 = pScratchPad->m_nSoundSource;

		pSosEntryMatch->Start();

	}

	CSosManagedEntryMatch *pSosEntryMatch = g_pSoundOperatorSystem->m_sosEntryBlockList.GetEntryFromIndex( pStructMem->m_nEntryIndex );
	if( pSosEntryMatch )
	{
		// defaults for now
		pSosEntryMatch->m_bActive = pStructMem->m_flInputActive[0] > 0.0 ? true : false;

		pSosEntryMatch->m_flDuration = pStructMem->m_flInputDuration[0] >= 0.0 ? pStructMem->m_flInputDuration[0] : 0.0;
		//pSosEntryMatch->m_bTimed = pSosEntryMatch->m_flDuration > -1 ? true : false;
		pSosEntryMatch->m_bTimed = true;

	}

}

void CSosOperatorBlockEntry::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorBlockEntry_t *pStructMem = (CSosOperatorBlockEntry_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	// 	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sNumber Allowed: %i\n", nLevel, "    ", pStructMem->m_nNumAllowed );

	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*smatch_entry: %s\n", nLevel, "    ", pStructMem->m_bMatchEntry ? pStructMem->m_nMatchEntryName : "\"\"" );	
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*smatch_sound: %s\n", nLevel, "    ", pStructMem->m_bMatchSound ? pStructMem->m_nMatchSoundName : "\"\"" );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*smatch_entity: %s\n", nLevel, "    ", pStructMem->m_bMatchEntity ? "true" : "false" );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*smatch_channel: %s\n", nLevel, "    ", pStructMem->m_bMatchEntity ? "true" : "false" );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*smatch_substring: %s\n", nLevel, "    ", pStructMem->m_bMatchSubString ? "true" : "false" );


}
void CSosOperatorBlockEntry::OpHelp( ) const
{

}

void CSosOperatorBlockEntry::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorBlockEntry_t *pStructMem = (CSosOperatorBlockEntry_t *)pVoidMem;

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
				else if ( !V_strcasecmp( pParamString, "match_entry" ) )
				{
					pStructMem->m_bMatchEntry = true;
					V_strncpy( pStructMem->m_nMatchEntryName, pValueString, sizeof(pStructMem->m_nMatchEntryName) );
				}
// 				else if ( !V_strcasecmp( pParamString, "match_sound" ) )
// 				{
// 					pStructMem->m_bMatchSound = true;
// 					V_strncpy( pStructMem->m_nMatchSoundName, pValueString, sizeof(pStructMem->m_nMatchSoundName) );
// 				}
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
// 				else if ( !V_strcasecmp( pParamString, "stop_oldest" ) )
// 				{
// 					if ( !V_strcasecmp( pValueString, "true" ) )
// 					{
// 						pStructMem->m_bStopOldest = true;
// 					}
// 					else
// 					{
// 						pStructMem->m_bStopOldest = false;
// 					}
// 				}
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
