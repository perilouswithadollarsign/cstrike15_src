//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "snd_dma.h"
#include "sos_op.h"
#include "sos_op_entry_time.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"




extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


//-----------------------------------------------------------------------------
// CSosOperatorEntryTime
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorEntryTime, "get_entry_time" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntryTime, m_flOutputEntryElapsed, SO_SINGLE, "output_entry_elapsed" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntryTime, m_flOutputSoundElapsed, SO_SINGLE, "output_sound_elapsed" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntryTime, m_flOutputStopElapsed, SO_SINGLE, "output_stop_elapsed" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntryTime, m_flOutputSoundDuration, SO_SINGLE, "output_sound_duration" );
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorEntryTime, "get_entry_time"  )

void CSosOperatorEntryTime::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorEntryTime_t *pStructMem = (CSosOperatorEntryTime_t *)pVoidMem;
	SOS_INIT_OUTPUT_VAR( m_flOutputSoundDuration, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutputSoundElapsed, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutputStopElapsed, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutputEntryElapsed, SO_SINGLE, 0.0 )

	pStructMem->m_nScriptHash = SOUNDEMITTER_INVALID_HASH;

}

void CSosOperatorEntryTime::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorEntryTime_t *pStructMem = (CSosOperatorEntryTime_t *)pVoidMem;

	channel_t *pTimeChannel = NULL;

	if( pStructMem->m_nScriptHash != SOUNDEMITTER_INVALID_HASH )
	{
		pTimeChannel = S_FindChannelByScriptHash( pStructMem->m_nScriptHash );
	}
	else
	{
		pTimeChannel = S_FindChannelByGuid( pStack->GetChannelGuid() );
	}

	if( !pTimeChannel )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s requires valid channel pointer, being called without one\n", pStack->GetOperatorName( nOpIndex ));
		return;
	}
	
	if( pTimeChannel->m_pStackList )
	{
		CSosOperatorStack *pTheStack = NULL;
		pTheStack = pTimeChannel->m_pStackList->GetStack( CSosOperatorStack::SOS_UPDATE );

		if( pTheStack )
		{
			pStructMem->m_flOutputEntryElapsed[0] = pTheStack->GetElapsedTime();
		}
	}
	pStructMem->m_flOutputSoundElapsed[0] =  S_GetElapsedTime( pTimeChannel ) * 0.01;
	pStructMem->m_flOutputSoundDuration[0] = S_SoundDuration( pTimeChannel );
	pStructMem->m_flOutputStopElapsed[0] = pStack->GetElapsedStopTime( );

}
void CSosOperatorEntryTime::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorEntryTime_t *pStructMem = (CSosOperatorEntryTime_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

}
void CSosOperatorEntryTime::OpHelp( ) const
{

}

void CSosOperatorEntryTime::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorEntryTime_t *pStructMem = (CSosOperatorEntryTime_t *)pVoidMem;
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
				else if ( !V_strcasecmp( pParamString, "entry" ) )
				{
					if ( !g_pSoundEmitterSystem )
					{
						DevWarning("Error: SoundEmitterSystem not initialized in engine!");
						return;
					}
					pStructMem->m_nScriptHash = g_pSoundEmitterSystem->HashSoundName( pValueString );

					if( !g_pSoundEmitterSystem->GetSoundNameForHash( pStructMem->m_nScriptHash ))
					{
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
