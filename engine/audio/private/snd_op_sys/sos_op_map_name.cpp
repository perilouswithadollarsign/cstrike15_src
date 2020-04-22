//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_map_name.h"

#include "snd_dma.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


//-----------------------------------------------------------------------------
// CSosOperatorMapName
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorMapName, "get_map_name" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorMapName, m_flOutput, SO_SINGLE, "output" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorMapName, "get_map_name"  )

void CSosOperatorMapName::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorMapName_t *pStructMem = (CSosOperatorMapName_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 0.0 )
}

void CSosOperatorMapName::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
//	CSosOperatorMapName_t *pStructMem = (CSosOperatorMapName_t *)pVoidMem;


}

void CSosOperatorMapName::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorMapName_t *pStructMem = (CSosOperatorMapName_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
}
void CSosOperatorMapName::OpHelp( ) const
{
}

void CSosOperatorMapName::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	extern IVEngineClient *engineClient;
	CSosOperatorMapName_t *pStructMem = (CSosOperatorMapName_t *)pVoidMem;
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
				else if ( !V_strcasecmp( pParamString, "map_name" ) )
				{
					pStructMem->m_flOutput[0] = 0.0;
					if( ! V_strcmp( engineClient->GetLevelNameShort(), pValueString ) )
					{
						pStructMem->m_flOutput[0] = 1.0;
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

