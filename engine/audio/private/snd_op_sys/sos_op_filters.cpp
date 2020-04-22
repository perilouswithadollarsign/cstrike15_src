//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"
#include "client.h"

#include "sos_op.h"
#include "sos_op_filters.h"
#include "icliententitylist.h"
#include "icliententity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


// ConVar snd_sos_show_source_info("snd_sos_show_source_info", "0" );

//-----------------------------------------------------------------------------
// CSosOperatorFloatFilter
//-----------------------------------------------------------------------------

SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorFloatFilter, "math_float_filter" )

SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorFloatFilter, m_flOutput, SO_SINGLE, "output" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloatFilter, m_flInput, SO_SINGLE, "input" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorFloatFilter, m_flInputMaxVel, SO_SINGLE, "input_max_velocity" );

SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorFloatFilter, "math_float_filter"  )


void CSosOperatorFloatFilter::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorFloatFilter_t *pStructMem = (CSosOperatorFloatFilter_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInput, SO_SINGLE, 0.0 )

	//pStructMem->m_flPrevTime = g_pSoundServices->GetHostTime();
	pStructMem->m_flPrevTime = -1.0;
}

void CSosOperatorFloatFilter::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
#ifndef DEDICATED
	CSosOperatorFloatFilter_t *pStructMem = (CSosOperatorFloatFilter_t *)pVoidMem;
	

	// first frame init position to avoid, erroneous deltas, zero is best option
	if(	pStructMem->m_flPrevTime < 0.0 )
	{
		pStructMem->m_flOutput[0] = pStructMem->m_flInput[0];
		pStructMem->m_flPrevTime = g_ClientGlobalVariables.curtime;
		
	}
	else
	{
		
		// calculate delta time for this operator updates
		float flCurTime = g_ClientGlobalVariables.curtime;
		float flDeltaTime = flCurTime - pStructMem->m_flPrevTime;
		pStructMem->m_flPrevTime = flCurTime;

		float flDeltaValue = pStructMem->m_flInput[0] - pStructMem->m_flOutput[0];

		if( flDeltaTime > 0.0 && flDeltaValue != 0.0 )
		{
			float flMaxVel = pStructMem->m_flInputMaxVel[0];

			// per/sec factor	
			float flPerSec = 1.0 / flDeltaTime;
			float flValueVel = flPerSec * flDeltaValue;
			if( fabs(flValueVel) > flMaxVel )
			{
				float flVector = ( flDeltaTime * (flMaxVel * ( flDeltaValue < 0.0 ? -1.0 : 1.0 )));
				pStructMem->m_flOutput[0] += flVector;
			}
			else
			{
				pStructMem->m_flOutput[0] = pStructMem->m_flInput[0];
			}
		}
	}

#endif
}



void CSosOperatorFloatFilter::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
//	CSosOperatorFloatFilter_t *pStructMem = (CSosOperatorFloatFilter_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
}

void CSosOperatorFloatFilter::OpHelp( ) const
{
}

void CSosOperatorFloatFilter::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorFloatFilter_t *pStructMem = (CSosOperatorFloatFilter_t *)pVoidMem;

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

