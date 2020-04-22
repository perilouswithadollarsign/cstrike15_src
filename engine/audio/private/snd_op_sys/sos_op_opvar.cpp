//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_opvar.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


//-----------------------------------------------------------------------------
// CSosOperatorSetOpvarFloat
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorSetOpvarFloat, "set_opvar_float" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorSetOpvarFloat, m_flInput, SO_SINGLE, "input" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorSetOpvarFloat, "set_opvar_float"  )

void CSosOperatorSetOpvarFloat::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorSetOpvarFloat_t *pStructMem = (CSosOperatorSetOpvarFloat_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flInput, SO_SINGLE, 0.0 )
	pStructMem->m_nOpVarName[ 0 ] = 0;
}

void CSosOperatorSetOpvarFloat::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorSetOpvarFloat_t *pStructMem = (CSosOperatorSetOpvarFloat_t *)pVoidMem;

	float flOpvarVal = pStructMem->m_flInput[0];
	g_pSoundOperatorSystem->SetOpVarFloat( pStructMem->m_nOpVarName, flOpvarVal );

}

void CSosOperatorSetOpvarFloat::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorSetOpvarFloat_t *pStructMem = (CSosOperatorSetOpvarFloat_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sopvar: %s\n", nLevel, "    ", pStructMem->m_nOpVarName );
}
void CSosOperatorSetOpvarFloat::OpHelp( ) const
{
}

void CSosOperatorSetOpvarFloat::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	extern IVEngineClient *engineClient;
	CSosOperatorSetOpvarFloat_t *pStructMem = (CSosOperatorSetOpvarFloat_t *)pVoidMem;
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
				else if ( !V_strcasecmp( pParamString, "opvar" ) )
				{
					V_strncpy( pStructMem->m_nOpVarName, pValueString, ARRAYSIZE( pStructMem->m_nOpVarName ) );
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
// CSosOperatorGetOpvarFloat
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorGetOpvarFloat, "get_opvar_float" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorGetOpvarFloat, m_flOutput, SO_SINGLE, "output" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorGetOpvarFloat, m_flOpVarExists, SO_SINGLE, "output_opvar_exists" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorGetOpvarFloat, "get_opvar_float"  )

	void CSosOperatorGetOpvarFloat::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorGetOpvarFloat_t *pStructMem = (CSosOperatorGetOpvarFloat_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOpVarExists, SO_SINGLE, 0.0 )
	pStructMem->m_nOpVarName[ 0 ] = 0;
}

void CSosOperatorGetOpvarFloat::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorGetOpvarFloat_t *pStructMem = (CSosOperatorGetOpvarFloat_t *)pVoidMem;

	float flOpvarVal = 0.0;
	bool bExists = g_pSoundOperatorSystem->GetOpVarFloat( pStructMem->m_nOpVarName, flOpvarVal );
	if( bExists )
	{
		pStructMem->m_flOpVarExists[0] = 1.0;
		pStructMem->m_flOutput[0] = flOpvarVal;
	}
	else
	{
		pStructMem->m_flOpVarExists[0] = 0.0;
		pStructMem->m_flOutput[0] = 0.0;
	}

}

void CSosOperatorGetOpvarFloat::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorGetOpvarFloat_t *pStructMem = (CSosOperatorGetOpvarFloat_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sopvar: %s\n", nLevel, "    ", pStructMem->m_nOpVarName );
}
void CSosOperatorGetOpvarFloat::OpHelp( ) const
{
}

void CSosOperatorGetOpvarFloat::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	extern IVEngineClient *engineClient;
	CSosOperatorGetOpvarFloat_t *pStructMem = (CSosOperatorGetOpvarFloat_t *)pVoidMem;
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
				else if ( !V_strcasecmp( pParamString, "opvar" ) )
				{
					V_strncpy( pStructMem->m_nOpVarName, pValueString, ARRAYSIZE( pStructMem->m_nOpVarName ) );
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
// CSosOperatorIncrementOpvarFloat
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorIncrementOpvarFloat, "increment_opvar_float" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorIncrementOpvarFloat, m_flInput, SO_SINGLE, "input" ) 
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorIncrementOpvarFloat, m_flOutput, SO_SINGLE, "output" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorIncrementOpvarFloat, m_flOpVarExists, SO_SINGLE, "output_opvar_exists" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorIncrementOpvarFloat, "increment_opvar_float"  )

	void CSosOperatorIncrementOpvarFloat::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorIncrementOpvarFloat_t *pStructMem = (CSosOperatorIncrementOpvarFloat_t *)pVoidMem;
	
	SOS_INIT_INPUT_VAR( m_flInput, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOpVarExists, SO_SINGLE, 0.0 )
	pStructMem->m_nOpVarName[ 0 ] = 0;
}

void CSosOperatorIncrementOpvarFloat::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorIncrementOpvarFloat_t *pStructMem = (CSosOperatorIncrementOpvarFloat_t *)pVoidMem;

	float flOpvarVal = 0.0;	//value of the opvar currently
	float flOpvarNewVal = 0.0; //value that we'll set it to
	float flOpvarInc = pStructMem->m_flInput[0]; //value to add

	bool bExists = g_pSoundOperatorSystem->GetOpVarFloat( pStructMem->m_nOpVarName, flOpvarVal );

	if( bExists )
	{
		flOpvarNewVal = flOpvarVal + flOpvarInc;
		pStructMem->m_flOpVarExists[0] = 1.0;
		pStructMem->m_flOutput[0] = flOpvarNewVal;
	}
	else
	{
		flOpvarNewVal = flOpvarInc;
		pStructMem->m_flOpVarExists[0] = 0.0;
		pStructMem->m_flOutput[0] = flOpvarNewVal;
	}
	
	g_pSoundOperatorSystem->SetOpVarFloat( pStructMem->m_nOpVarName, flOpvarNewVal );

}

void CSosOperatorIncrementOpvarFloat::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorIncrementOpvarFloat_t *pStructMem = (CSosOperatorIncrementOpvarFloat_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sopvar: %s\n", nLevel, "    ", pStructMem->m_nOpVarName );
}
void CSosOperatorIncrementOpvarFloat::OpHelp( ) const
{
}

void CSosOperatorIncrementOpvarFloat::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	extern IVEngineClient *engineClient;
	CSosOperatorIncrementOpvarFloat_t *pStructMem = (CSosOperatorIncrementOpvarFloat_t *)pVoidMem;
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
				else if ( !V_strcasecmp( pParamString, "opvar" ) )
				{
					V_strncpy( pStructMem->m_nOpVarName, pValueString, ARRAYSIZE( pStructMem->m_nOpVarName ) );
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