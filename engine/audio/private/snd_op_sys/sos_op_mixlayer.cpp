//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op_mixlayer.h"


// #include "cdll_engine_int.h"
#include "../../debugoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;

//-----------------------------------------------------------------------------
// CSosOperatorMixLayer
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorMixLayer, "sys_mixlayer" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorMixLayer, m_flInput, SO_SINGLE, "input" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorMixLayer, "sys_mixlayer"  )

void CSosOperatorMixLayer::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorMixLayer_t *pStructMem = (CSosOperatorMixLayer_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flInput, SO_SINGLE, 0.0 )
	pStructMem->m_nFieldType = MXR_MIXGROUP_NONE;
	pStructMem->m_nMixLayerIndex = -1;
	pStructMem->m_nMixGroupIndex = -1;

}

void CSosOperatorMixLayer::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorMixLayer_t *pStructMem = (CSosOperatorMixLayer_t *)pVoidMem;

	if( pStructMem->m_nMixGroupIndex < 0 )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: MixLayer operator has invalid mix group index!\n" );
		return;
	}
	if( pStructMem->m_nMixLayerIndex < 0 )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: MixLayer operator has invalid mix layer index!\n" );
		return;
	}

	S_SetMixGroupOfMixLayer( pStructMem->m_nMixGroupIndex, pStructMem->m_nMixLayerIndex, pStructMem->m_nFieldType, pStructMem->m_flInput[0] );


}


void CSosOperatorMixLayer::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorMixLayer_t *pStructMem = (CSosOperatorMixLayer_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

	const char *pFieldTypeString = "none";
	switch( pStructMem->m_nFieldType )
	{
	case MXR_MIXGROUP_VOL:
		pFieldTypeString = "volume";
		break;
	case MXR_MIXGROUP_LEVEL:
		pFieldTypeString = "level";
		break;
	case MXR_MIXGROUP_DSP:
		pFieldTypeString = "dsp";
		break;
	case MXR_MIXGROUP_SOLO:
		pFieldTypeString = "solo";
		break;
	case MXR_MIXGROUP_MUTE:
		pFieldTypeString = "mute";
		break;
	default:
		break;
	}
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sField: %s\n", nLevel, "    ", pFieldTypeString );

}

void CSosOperatorMixLayer::OpHelp( ) const
{
}

void CSosOperatorMixLayer::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorMixLayer_t *pStructMem = (CSosOperatorMixLayer_t *)pVoidMem;

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
				else if ( !V_strcasecmp( pParamString, "field" ) )
				{
					if ( !V_strcasecmp( pValueString, "volume" ) )
					{
						pStructMem->m_nFieldType = MXR_MIXGROUP_VOL;
					}
					else if ( !V_strcasecmp( pValueString, "level" ) )
					{
						pStructMem->m_nFieldType = MXR_MIXGROUP_LEVEL;
					}
					else if ( !V_strcasecmp( pValueString, "dsp" ) )
					{
						pStructMem->m_nFieldType = MXR_MIXGROUP_DSP;
					}
					else if ( !V_strcasecmp( pValueString, "solo" ) )
					{
						pStructMem->m_nFieldType = MXR_MIXGROUP_SOLO;
					}
					else if ( !V_strcasecmp( pValueString, "mute" ) )
					{
						pStructMem->m_nFieldType = MXR_MIXGROUP_MUTE;
					}
				}
				else if ( !V_strcasecmp( pParamString, "mixlayer" ) )
				{
					int nMixLayerID = MXR_GetMixLayerIndexFromName( pValueString );
					if ( nMixLayerID == -1 )
					{
						Log_Warning( LOG_SND_OPERATORS, "Error: Failed to get mix layer %s!\n", pValueString );
					}
					pStructMem->m_nMixLayerIndex = nMixLayerID;
				}	
				else if ( !V_strcasecmp( pParamString, "mixgroup" ) )
				{
					int nMixGroupID = S_GetMixGroupIndex( pValueString );
					if ( nMixGroupID == -1 )
					{
						Log_Warning( LOG_SND_OPERATORS, "Error: Failed to get mix group %s!\n", pValueString );
					}
					pStructMem->m_nMixGroupIndex = nMixGroupID;
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
