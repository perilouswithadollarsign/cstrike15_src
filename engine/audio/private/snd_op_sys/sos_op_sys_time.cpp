//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "snd_dma.h"
#include "sos_op.h"
#include "sos_op_sys_time.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;

//-----------------------------------------------------------------------------
// CSosOperatorSysTime
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorSysTime, "get_sys_time" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorSysTime, m_flOutputClientElapsed, SO_SINGLE, "output_client_time" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorSysTime, m_flOutputHostElapsed, SO_SINGLE, "output_host_time" );
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorSysTime, "get_sys_time"  )

void CSosOperatorSysTime::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorSysTime_t *pStructMem = (CSosOperatorSysTime_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutputClientElapsed, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutputHostElapsed, SO_SINGLE, 0.0 )
}

void CSosOperatorSysTime::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorSysTime_t *pStructMem = (CSosOperatorSysTime_t *)pVoidMem;

	pStructMem->m_flOutputHostElapsed[0] = g_pSoundServices->GetHostTime();
	pStructMem->m_flOutputClientElapsed[0] = g_pSoundServices->GetClientTime();

}
void CSosOperatorSysTime::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorSysTime_t *pStructMem = (CSosOperatorSysTime_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

}
void CSosOperatorSysTime::OpHelp( ) const
{

}

void CSosOperatorSysTime::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorSysTime_t *pStructMem = (CSosOperatorSysTime_t *)pVoidMem;
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
