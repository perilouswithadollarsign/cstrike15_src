//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_start_entry.h"

#include "snd_dma.h"
// #include "cdll_engine_int.h"
#include "../../debugoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;

//-----------------------------------------------------------------------------
// CSosOperatorStartEntry
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorStartEntry, "sys_start_entry" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorStartEntry, m_flInputStart, SO_SINGLE, "input_start")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorStartEntry, m_flInputStartDelay, SO_SINGLE, "input_start_delay")
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorStartEntry, "sys_start_entry"  )

void CSosOperatorStartEntry::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorStartEntry_t *pStructMem = (CSosOperatorStartEntry_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flInputStart, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputStartDelay, SO_SINGLE, 0.0 )

	pStructMem->m_nScriptHash = SOUNDEMITTER_INVALID_HASH;
	pStructMem->m_nHasStarted = 0;
	pStructMem->m_bTriggerOnce = false;

}

void CSosOperatorStartEntry::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorStartEntry_t *pStructMem = (CSosOperatorStartEntry_t *)pVoidMem;

	if(  pStructMem->m_flInputStart[0] > 0.0 && !( pStructMem->m_nHasStarted && pStructMem->m_bTriggerOnce ) )
	{
		pStructMem->m_nHasStarted = 1;


		if ( !g_pSoundEmitterSystem )
		{
			Log_Warning( LOG_SND_OPERATORS, OpColor, "Error: SoundEmitterSystem not initialized in engine!");
			return;
		}

		if( !g_pSoundEmitterSystem->GetSoundNameForHash( pStructMem->m_nScriptHash ) )
		{
			Log_Warning( LOG_SND_OPERATORS, OpColor, "Error: Invalid SoundEntry hash %i in operator %s\n", pStructMem->m_nScriptHash , pStack->GetOperatorName( nOpIndex ));
			return;
		}

		
		// try to copy  all these from the current channel?
		// this all needs to GET ORGANIZED, some values should come from params some from channel via?? param switch? use copy methods
		// also, most of this is redundant in that "startsoundentry" calls this same function
		// still a bit messy !!
		StartSoundParams_t startParams;
		CSoundParameters pScriptParams;
		gender_t gender = GENDER_NONE;
		if ( !g_pSoundEmitterSystem->GetParametersForSoundEx( "SoundSciptHandle ERROR", pStructMem->m_nScriptHash, pScriptParams, gender, true ) )
		{
			//DevWarning("Error: Unable to get parameters for soundentry %s", startParams.m_pSoundEntryName );
			return;
		}

		// don't actually need the soundfile yet

	// 		if ( !pScriptParams.soundname[0] )
// 			return;

		// copy emitter params
		startParams.staticsound = ( pScriptParams.channel == CHAN_STATIC ) ? true : false;
		startParams.entchannel = pScriptParams.channel;

		// inherits location and entity
		VectorCopy( pScratchPad->m_vEmitterInfoOrigin, startParams.origin );
		startParams.soundsource = pScratchPad->m_nSoundSource;

		startParams.fvol = pScriptParams.volume;
		startParams.soundlevel = pScriptParams.soundlevel;
		//	startParams.flags = sound.nFlags;
		startParams.pitch = pScriptParams.pitch;
		startParams.fromserver = false;
		startParams.delay = pScriptParams.delay_msec;
		//	startParams.speakerentity = sound.nSpeakerEntity;
		//startParams.m_bIsScriptHandle = ( pScriptParams.m_nSoundEntryVersion > 1 );
		startParams.m_bIsScriptHandle = true;

		startParams.m_nSoundScriptHash = pStructMem->m_nScriptHash;
		g_pSoundOperatorSystem->QueueStartEntry( startParams, pStructMem->m_flInputStartDelay[0], false );
	}


}


void CSosOperatorStartEntry::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorStartEntry_t *pStructMem = (CSosOperatorStartEntry_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sEntry Script Hash: %i\n", nLevel, "    ", pStructMem->m_nScriptHash );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sEntry Name: %s\n", nLevel, "    ", g_pSoundEmitterSystem->GetSoundNameForHash( pStructMem->m_nScriptHash ) );

}

void CSosOperatorStartEntry::OpHelp( ) const
{
}

void CSosOperatorStartEntry::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorStartEntry_t *pStructMem = (CSosOperatorStartEntry_t *)pVoidMem;

	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if ( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ))
				{

				}
				else if ( !V_strcasecmp( pParamString, "entry_name" ) )
				{
					if ( !g_pSoundEmitterSystem )
					{
						DevWarning("Error: SoundEmitterSystem not initialized in engine!");
						return;
					}
					pStructMem->m_nScriptHash = g_pSoundEmitterSystem->HashSoundName( pValueString );

					if( !g_pSoundEmitterSystem->GetSoundNameForHash( pStructMem->m_nScriptHash ))
					{
//						DevMsg( "Error: Invalid SoundEntry index %i from entry %s  operator %s", pStructMem->m_nScriptHandle, pValueString, pStack->GetOperatorName( nOpIndex ) );
						DevMsg( "Error: Invalid SoundEntry hash %i from entry %s", pStructMem->m_nScriptHash, pValueString );

						return;
					}
				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}
