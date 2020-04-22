//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_iterate_merge_speakers.h"

// #include "../../debugoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


//-----------------------------------------------------------------------------
// CSosOperatorIterateAndMergeSpeakers_t
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorIterateAndMergeSpeakers, "iterate_merge_speakers" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorIterateAndMergeSpeakers, m_flInputMaxIterations, SO_SINGLE, "input_max_iterations" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorIterateAndMergeSpeakers, m_flOutputIndex, SO_SINGLE, "output_index" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorIterateAndMergeSpeakers, m_flOutputSpeakers, SO_SPEAKERS, "output" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorIterateAndMergeSpeakers, m_flInputSpeakers, SO_SPEAKERS, "input")
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorIterateAndMergeSpeakers, "iterate_merge_speakers"  )

void CSosOperatorIterateAndMergeSpeakers::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorIterateAndMergeSpeakers_t *pStructMem = ( CSosOperatorIterateAndMergeSpeakers_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flInputMaxIterations, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputSpeakers, SO_SPEAKERS, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutputSpeakers, SO_SPEAKERS, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutputIndex, SO_SINGLE, 0.0 )

	pStructMem->m_nMaxIterations = 8;
	pStructMem->m_nOperatorIndex = -1;
}

void CSosOperatorIterateAndMergeSpeakers::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{

	CSosOperatorIterateAndMergeSpeakers_t *pStructMem = ( CSosOperatorIterateAndMergeSpeakers_t *)pVoidMem;

	if( !pChannel )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s requires valid channel pointer, being called without one\n", pStack->GetOperatorName( nOpIndex ));
		return;
	}

	int nCurIndex = ( int )pStructMem->m_flOutputIndex[0];
	int nLastIndex = MIN( ( int ) pStructMem->m_flInputMaxIterations[0], pStructMem->m_nMaxIterations );

	if( nCurIndex == 0 )
	{
		for ( int i = 0; i < SO_MAX_SPEAKERS; i++ )
		{
			pStructMem->m_flOutputSpeakers[i] = 0.0;
		}
	}

	// merge speaker volumes
	for ( int v = 0; v < SO_MAX_SPEAKERS; ++v )
	{
		// 		switch ( g_SndMergeMethod )
		// 		switch ( SND_MERGE_MAX )
		// 		{
		// 		default:
		// 		case SND_MERGE_SUMANDCLIP:
		// 			{
		// 				pStructMem->m_flOutputSpeakers[v] = min( pStructMem->m_flInputSpeakers[v]+pStructMem->m_flOutputSpeakers[v], 1.0 );
		// 			}
		// 			break;
		// 		case SND_MERGE_MAX:
		// 			{
		pStructMem->m_flOutputSpeakers[v] = MAX( pStructMem->m_flInputSpeakers[v], pStructMem->m_flOutputSpeakers[v] );
		// 			}
		// 			break;

		// 		}
	}	


	// are we done?
	if ( nCurIndex < nLastIndex )
	{
		nCurIndex++;
		pStructMem->m_flOutputIndex[0] = ( float )nCurIndex;
		pStack->ExecuteIterator( pChannel, pScratchPad, pVoidMem, pStructMem->m_nStartOperatorName, &pStructMem->m_nOperatorIndex );
		return;
	}
	else
	{
		// clear index for next time around
		pStructMem->m_flOutputIndex[0] = 0.0;
		return;
	}
}

void CSosOperatorIterateAndMergeSpeakers::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorIterateAndMergeSpeakers_t *pStructMem = ( CSosOperatorIterateAndMergeSpeakers_t * )pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sIterate Op: %s\n", nLevel, "    ", pStructMem->m_nStartOperatorName );	
}
void CSosOperatorIterateAndMergeSpeakers::OpHelp( ) const
{
}

void CSosOperatorIterateAndMergeSpeakers::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorIterateAndMergeSpeakers_t *pStructMem = (CSosOperatorIterateAndMergeSpeakers_t *)pVoidMem;

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
				else if ( !V_strcasecmp( pParamString, "iterate_operator" ) )
				{
					Q_strncpy( pStructMem->m_nStartOperatorName, pValueString, sizeof(pStructMem->m_nStartOperatorName) );
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

