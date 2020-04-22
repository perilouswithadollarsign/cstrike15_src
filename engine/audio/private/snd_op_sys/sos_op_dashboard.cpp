//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_dashboard.h"

#include "snd_dma.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


//-----------------------------------------------------------------------------
// CSosOperatorDashboard
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorDashboard, "get_dashboard" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorDashboard, m_flOutput, SO_SINGLE, "output" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorDashboard, "get_dashboard" )

void CSosOperatorDashboard::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorDashboard_t *pStructMem = (CSosOperatorDashboard_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 1.0 )
		pStructMem->m_bMusic = true;

}

void CSosOperatorDashboard::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorDashboard_t *pStructMem = (CSosOperatorDashboard_t *)pVoidMem;

	float flValue = 1.0;
	if ( pStructMem->m_bMusic )
	{
		flValue = S_GetDashboarMusicMixValue();
	}
	pStructMem->m_flOutput[0] = flValue;

}
void CSosOperatorDashboard::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorDashboard_t *pStructMem = (CSosOperatorDashboard_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	///	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sDashboard: %s\n", nLevel, "    ", pStructMem->m_nConvar );
}
void CSosOperatorDashboard::OpHelp( ) const
{
}

void CSosOperatorDashboard::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorDashboard_t *pStructMem = (CSosOperatorDashboard_t *)pVoidMem;
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
				else if ( !V_strcasecmp( pParamString, "ds_type" ) )
				{
					if ( !V_strcasecmp( pValueString, "music" ) )
					{
						pStructMem->m_bMusic = true;
					}
					else
					{
						pStructMem->m_bMusic = false;
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
