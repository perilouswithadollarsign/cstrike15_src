//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_distant_dsp.h"

#include "snd_dma.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


//-----------------------------------------------------------------------------
// CSosOperatorDistantDSP
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorDistantDSP, "calc_distant_dsp" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorDistantDSP, m_flOutput, SO_SINGLE, "output" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorDistantDSP, m_flInputDist, SO_SINGLE, "input_distance" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorDistantDSP, m_flInputLevel, SO_SINGLE, "input_level" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorDistantDSP, "calc_distant_dsp"  )


void CSosOperatorDistantDSP::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorDistantDSP_t *pStructMem = (CSosOperatorDistantDSP_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 1.0 )
		SOS_INIT_INPUT_VAR( m_flInputDist, SO_SINGLE, 1.0 )
		SOS_INIT_INPUT_VAR( m_flInputLevel, SO_SINGLE, 65.0 )

}


void CSosOperatorDistantDSP::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorDistantDSP_t *pStructMem = (CSosOperatorDistantDSP_t *)pVoidMem;

	if( !pChannel )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s requires valid channel pointer, being called without one\n", pStack->GetOperatorName( nOpIndex ));
		return;
	}


	pStructMem->m_flOutput[0] = SND_GetDspMix( pChannel, pStructMem->m_flInputDist[0], pStructMem->m_flInputLevel[0] );

}


void CSosOperatorDistantDSP::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//CSosOperatorDistantDSP_t *pStructMem = (CSosOperatorDistantDSP_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

}
void CSosOperatorDistantDSP::OpHelp(  ) const
{
}

void CSosOperatorDistantDSP::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorDistantDSP_t *pStructMem = (CSosOperatorDistantDSP_t *)pVoidMem;
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
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}

