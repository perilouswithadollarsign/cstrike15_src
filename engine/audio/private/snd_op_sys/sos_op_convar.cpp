//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_convar.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar snd_op_test_convar("snd_op_test_convar", "1.0", FCVAR_CHEAT );


extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


//-----------------------------------------------------------------------------
// CSosOperatorSetConvar
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorSetConvar, "set_convar" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorSetConvar, m_flInput, SO_SINGLE, "input" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorSetConvar, "set_convar" )


void CSosOperatorSetConvar::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorSetConvar_t *pStructMem = (CSosOperatorSetConvar_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flInput, SO_SINGLE, 0.0 )

}

void CSosOperatorSetConvar::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorSetConvar_t *pStructMem = (CSosOperatorSetConvar_t *)pVoidMem;

	if ( pStructMem->m_ConVarRef.IsValid() )
	{
		pStructMem->m_ConVarRef.SetValue( pStructMem->m_flInput[0] );
	}
	else
	{
		// The stored ref is not valid, maybe was not initialized at load time, let's try again
		pStructMem->m_ConVarRef.Init( pStructMem->m_nConvar, true );
		if ( pStructMem->m_ConVarRef.IsValid() )
		{
			pStructMem->m_ConVarRef.SetValue( pStructMem->m_flInput[0] );		// Great success! Will be cached for next time.
		}
		else
		{
			Log_Warning( LOG_SND_OPERATORS, "Warning: Unable to set convar value: %s\n", pStructMem->m_nConvar );
		}
	}
}
void CSosOperatorSetConvar::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorSetConvar_t *pStructMem = (CSosOperatorSetConvar_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sconvar: %s\n", nLevel, "    ", pStructMem->m_nConvar );
}
void CSosOperatorSetConvar::OpHelp( ) const
{
}

void CSosOperatorSetConvar::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorSetConvar_t *pStructMem = (CSosOperatorSetConvar_t *)pVoidMem;
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
				else if ( !V_strcasecmp( pParamString, "convar" ) )
				{
					V_strcpy_safe( pStructMem->m_nConvar, pValueString );
					pStructMem->m_ConVarRef.Init( pValueString, true );
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
// CSosOperatorConvar
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorConvar, "get_convar" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorConvar, m_flOutput, SO_SINGLE, "output" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorConvar, "get_convar" )



void CSosOperatorConvar::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorConvar_t *pStructMem = (CSosOperatorConvar_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 1.0 )

}

void CSosOperatorConvar::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorConvar_t *pStructMem = (CSosOperatorConvar_t *)pVoidMem;

	float flValue;
	if ( pStructMem->m_ConVarRef.IsValid() )
	{
		flValue = pStructMem->m_ConVarRef.GetFloat();
	}
	else
	{
		// The stored ref is not valid, maybe was not initialized at load time, let's try again
		pStructMem->m_ConVarRef.Init( pStructMem->m_nConvar, true );
		if ( pStructMem->m_ConVarRef.IsValid() )
		{
			flValue = pStructMem->m_ConVarRef.GetFloat();		// Great success! Will be cached for next time.
		}
		else
		{
			Log_Warning( LOG_SND_OPERATORS, "Warning: Unable to acquire convar value: %s\n", pStructMem->m_nConvar );
			flValue = 1.0;
		}
	}
	pStructMem->m_flOutput[0] = flValue;

}
void CSosOperatorConvar::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorConvar_t *pStructMem = (CSosOperatorConvar_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sconvar: %s\n", nLevel, "    ", pStructMem->m_nConvar );
}
void CSosOperatorConvar::OpHelp( ) const
{
}

void CSosOperatorConvar::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorConvar_t *pStructMem = (CSosOperatorConvar_t *)pVoidMem;
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
				else if ( !V_strcasecmp( pParamString, "convar" ) )
				{
					V_strcpy_safe( pStructMem->m_nConvar, pValueString );
					pStructMem->m_ConVarRef.Init( pValueString, true );
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
