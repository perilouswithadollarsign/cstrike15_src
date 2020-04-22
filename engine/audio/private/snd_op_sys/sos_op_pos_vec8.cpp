//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_pos_vec8.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


//-----------------------------------------------------------------------------
// CSosOperatorPosVec8
//-----------------------------------------------------------------------------

SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorPosVec8, "util_pos_vec8" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorPosVec8, m_flOutPosition, SO_VEC3, "output_position" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorPosVec8, m_flOutMaxIndex, SO_SINGLE, "output_max_index" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorPosVec8, m_flInputIndex, SO_SINGLE, "input_index" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorPosVec8, m_flInputEntryCount, SO_SINGLE, "input_entry_count" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorPosVec8, m_flInputPos0, SO_VEC3, "input_position_0" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorPosVec8, m_flInputPos1, SO_VEC3, "input_position_1" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorPosVec8, m_flInputPos2, SO_VEC3, "input_position_2" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorPosVec8, m_flInputPos3, SO_VEC3, "input_position_3" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorPosVec8, m_flInputPos4, SO_VEC3, "input_position_4" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorPosVec8, m_flInputPos5, SO_VEC3, "input_position_5" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorPosVec8, m_flInputPos6, SO_VEC3, "input_position_6" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorPosVec8, m_flInputPos7, SO_VEC3, "input_position_7" );

SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorPosVec8, "util_pos_vec8"  )

void CSosOperatorPosVec8::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorPosVec8_t *pStructMem = (CSosOperatorPosVec8_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutPosition, SO_VEC3, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutMaxIndex, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputEntryCount, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInputIndex, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputPos0, SO_VEC3, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputPos1, SO_VEC3, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputPos2, SO_VEC3, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputPos3, SO_VEC3, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputPos4, SO_VEC3, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputPos5, SO_VEC3, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputPos6, SO_VEC3, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputPos7, SO_VEC3, 0.0 )


}

void CSosOperatorPosVec8::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorPosVec8_t *pStructMem = (CSosOperatorPosVec8_t *)pVoidMem;

	int nIndex = (int)pStructMem->m_flInputIndex[0];
	int nEntryCount = pStructMem->m_flInputEntryCount[0];
	nIndex = ( nIndex >=  nEntryCount ) ? nEntryCount - 1 : nIndex;
	nIndex = ( nIndex < 0 ) ? 0 : nIndex;

	float *pIndexedPos = pStructMem->m_flInputPos0;

	switch( nIndex )
	{
	case 0:
		pIndexedPos = pStructMem->m_flInputPos0;
		break;
	case 1:
		pIndexedPos = pStructMem->m_flInputPos1;
		break;
	case 2:
		pIndexedPos = pStructMem->m_flInputPos2;
		break;
	case 3:
		pIndexedPos = pStructMem->m_flInputPos3;
		break;
	case 4:
		pIndexedPos = pStructMem->m_flInputPos4;
		break;
	case 5:
		pIndexedPos = pStructMem->m_flInputPos5;
		break;
	case 6:
		pIndexedPos = pStructMem->m_flInputPos6;
		break;
	case 7:
		pIndexedPos = pStructMem->m_flInputPos7;
		break;
	}

	pStructMem->m_flOutPosition[0] = pIndexedPos[0];
	pStructMem->m_flOutPosition[1] = pIndexedPos[1];
	pStructMem->m_flOutPosition[2] = pIndexedPos[2];

	pStructMem->m_flOutMaxIndex[0] = pStructMem->m_flInputEntryCount[0] - 1.0;

}



void CSosOperatorPosVec8::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorPosVec8_t *pStructMem = (CSosOperatorPosVec8_t *)pVoidMem;
	NOTE_UNUSED( pStructMem );
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

}
void CSosOperatorPosVec8::OpHelp( ) const
{
}

void CSosOperatorPosVec8::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorPosVec8_t *pStructMem = (CSosOperatorPosVec8_t *)pVoidMem;

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

