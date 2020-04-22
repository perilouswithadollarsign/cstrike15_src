//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_falloff.h"

#include "snd_dma.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


//-----------------------------------------------------------------------------
// CSosOperatorFalloff
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorFalloff, "calc_falloff" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorFalloff, m_flOutput, SO_SINGLE, "output" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFalloff, m_flInputDist, SO_SINGLE, "input_distance" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFalloff, m_flInputLevel, SO_SINGLE, "input_level" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorFalloff, "calc_falloff"  )

void CSosOperatorFalloff::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorFalloff_t *pStructMem = (CSosOperatorFalloff_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 1.0 )
		SOS_INIT_INPUT_VAR( m_flInputDist, SO_SINGLE, 1.0 )
		SOS_INIT_INPUT_VAR( m_flInputLevel, SO_SINGLE, 1.0 )

}

void CSosOperatorFalloff::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorFalloff_t *pStructMem = (CSosOperatorFalloff_t *)pVoidMem;

	float flDistMult = SNDLVL_TO_DIST_MULT( pStructMem->m_flInputLevel[0] );

	// the first arg used to be because there was a form of compression on the falloff
	// based on overall volume level, we are not having volume effect our falloff
	pStructMem->m_flOutput[0] = SND_GetGainFromMult( 1.0, flDistMult, pStructMem->m_flInputDist[0] );

}

void CSosOperatorFalloff::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorFalloff_t *pStructMem = (CSosOperatorFalloff_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
}
void CSosOperatorFalloff::OpHelp( ) const
{
}

void CSosOperatorFalloff::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorFalloff_t *pStructMem = (CSosOperatorFalloff_t *)pVoidMem;
	KeyValues *pParams = pOpKeys->GetFirstSubKey();
	while ( pParams )
	{
		const char *pParamString = pParams->GetName();
		const char *pValueString = pParams->GetString();
		if ( pParamString && *pParamString )
		{
			if ( pValueString && *pValueString )
			{
				if( BaseParseKV( pStack, (CSosOperator_t *)pStructMem, pParamString, pValueString ))
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

//-----------------------------------------------------------------------------
// CSosOperatorFalloffTail
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorFalloffTail, "calc_falloff_curve" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorFalloffTail, m_flOutput, SO_SINGLE, "output" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFalloffTail, m_flInputDist, SO_SINGLE, "input_distance" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFalloffTail, m_flInputExp, SO_SINGLE, "input_curve_amount" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFalloffTail, m_flInputDistantMin, SO_SINGLE, "input_min" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFalloffTail, m_flInputDistantMax, SO_SINGLE, "input_max" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFalloffTail, m_flInputAtten, SO_SINGLE, "input_atten" )
// SOS_REGISTER_INPUT_FLOAT( CSosOperatorFalloffTail, m_flInputTailMin, SO_SINGLE, "input_tail_min" )
// SOS_REGISTER_INPUT_FLOAT( CSosOperatorFalloffTail, m_flInputTailMax, SO_SINGLE, "input_tail_max" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorFalloffTail, "calc_falloff_curve" )

void CSosOperatorFalloffTail::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorFalloffTail_t *pStructMem = (CSosOperatorFalloffTail_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInputDist, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputExp, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputDistantMin, SO_SINGLE, 36.0 )
	SOS_INIT_INPUT_VAR( m_flInputDistantMax, SO_SINGLE, 360.0 )
	SOS_INIT_INPUT_VAR( m_flInputAtten, SO_SINGLE, 360.0 )

// 	SOS_INIT_INPUT_VAR( m_flInputTailMin, SO_SINGLE, 240.0 )
// 	SOS_INIT_INPUT_VAR( m_flInputTailMax, SO_SINGLE, 540.0 )
}
#define SOP_FALLOFF_MIN_EXP 0.1f
void CSosOperatorFalloffTail::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{

	CSosOperatorFalloffTail_t *pStructMem = (CSosOperatorFalloffTail_t *)pVoidMem;

	float flDistance = pStructMem->m_flInputDist[0];
	float flDistantMin = pStructMem->m_flInputDistantMin[0];
	float flDistantMax = pStructMem->m_flInputDistantMax[0];
// 	float flTailMin = pStructMem->m_flInputTailMin[0];
// 	float flTailMax = pStructMem->m_flInputTailMax[0];

	flDistantMax = flDistantMax > flDistantMin ? flDistantMax : flDistantMin;
// 	flTailMin < flDistantMax ? flTailMin : flDistantMax;
// 	flTailMax > flDistantMax ? flTailMax : flDistantMax;

	float flResult;
	if ( flDistance <= flDistantMin )
	{
		flResult = pStructMem->m_flInputAtten[0];
	}
// 	else if ( flDistance >= flTailMax ||
// 		( flDistance > flDistantMax && flTailMin == flDistantMax ) )
	else if ( flDistance > flDistantMax )
	{

		flResult = 0.0;
	}
// 	else if ( flDistance > flTailMin && flDistance < flTailMax )
// 	{
// 		flResult = 1.0 - ( ( flDistance - flTailMin ) / ( flTailMax - flTailMin ) );
// 		flResult *= FastPow( 1.0 - ( flTailMin - flDistantMin ) / ( flDistantMax - flDistantMin ), pStructMem->m_flInputDistantExp[0] );
// 	}
//	else if ( flDistance > flDistantMin && flDistance <= flTailMin )
	else // flDistance is between flDistantMin and flDistantMax
	{
		flResult = ( flDistance - flDistantMin ) / ( flDistantMax - flDistantMin );

		flResult = (1.0 - FastPow( flResult, ( 1.0 + SOP_FALLOFF_MIN_EXP) - pStructMem->m_flInputExp[0] )) * pStructMem->m_flInputAtten[0];
	}
	pStructMem->m_flOutput[0] = flResult;
}

void CSosOperatorFalloffTail::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorFalloffTail_t *pStructMem = (CSosOperatorFalloffTail_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
}
void CSosOperatorFalloffTail::OpHelp() const
{
}
void CSosOperatorFalloffTail::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorFalloffTail_t *pStructMem = (CSosOperatorFalloffTail_t *)pVoidMem;
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
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n", pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}

