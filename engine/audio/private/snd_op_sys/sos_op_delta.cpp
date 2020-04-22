//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_delta.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"




extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


//-----------------------------------------------------------------------------
// CSosOperatorDelta
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorDelta, "math_delta" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorDelta, m_flInput, SO_SINGLE, "input" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorDelta, m_flOutput, SO_SINGLE, "output" );

SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorDelta, "math_delta"  )

void CSosOperatorDelta::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorDelta_t *pStructMem = (CSosOperatorDelta_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flInput, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 0.0 )
	pStructMem->m_flLastInput = 0.0;
	pStructMem->m_flLastTime = g_pSoundServices->GetHostTime();

}

void CSosOperatorDelta::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorDelta_t *pStructMem = (CSosOperatorDelta_t *)pVoidMem;

	// need to normalize for frame time
	pStructMem->m_flOutput[0] = pStructMem->m_flInput[0] - pStructMem->m_flLastInput;

	pStructMem->m_flLastInput = pStructMem->m_flInput[0];

}
void CSosOperatorDelta::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorDelta_t *pStructMem = (CSosOperatorDelta_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

}
void CSosOperatorDelta::OpHelp( ) const
{

}

void CSosOperatorDelta::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorDelta_t *pStructMem = (CSosOperatorDelta_t *)pVoidMem;
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
