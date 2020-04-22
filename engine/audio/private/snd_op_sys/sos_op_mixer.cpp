//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_mixer.h"

#include "snd_mixgroups.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


//-----------------------------------------------------------------------------
// CSosOperatorMixGroup
// Catchall operator 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorMixGroup, "get_soundmixer" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorMixGroup, m_flOutputVolume, SO_SINGLE, "output_volume" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorMixGroup, m_flOutputLevel, SO_SINGLE, "output_level" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorMixGroup, m_flOutputDSP, SO_SINGLE, "output_dsp" );
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorMixGroup, "get_soundmixer"  )

void CSosOperatorMixGroup::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorMixGroup_t *pStructMem = (CSosOperatorMixGroup_t *)pVoidMem;

	pStructMem->m_nMixGroupIndex = -1;
	pStructMem->m_bSetMixGroupOnChannel = false;
	SOS_INIT_OUTPUT_VAR( m_flOutputVolume, SO_SINGLE, 1.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutputLevel, SO_SINGLE, 1.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutputDSP, SO_SINGLE, 1.0 )

}
void CSosOperatorMixGroup::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorMixGroup_t *pStructMem = (CSosOperatorMixGroup_t *)pVoidMem;

	if( !pChannel )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s requires valid channel pointer, being called without one\n", pStack->GetOperatorName( nOpIndex ));
		return;
	}

	if ( pStructMem->m_nMixGroupIndex < 0 )
	{
		char sndname[MAX_PATH];
		pChannel->sfx->getname(sndname, sizeof(sndname));
		MXR_GetMixGroupFromSoundsource( pChannel );
		pStructMem->m_nMixGroupIndex = MXR_GetFirstValidMixGroup( pChannel );

	//	DevMsg( "***FILE: %s: INDEX: %i: MIXGROUP: %s\n", sndname, pStructMem->m_nMixGroupIndex,  MXR_GetGroupnameFromId( pStructMem->m_nMixGroupIndex ));
		if ( pStructMem->m_nMixGroupIndex < 0 )
		{
			Log_Warning( LOG_SND_OPERATORS, "Error: MixGroup index error, %s, %s", sndname, MXR_GetGroupnameFromId( pStructMem->m_nMixGroupIndex ));
			return;
		}
	}

	if( pStructMem->m_bSetMixGroupOnChannel && pChannel )
	{
		pChannel->mixgroups[0] =  pStructMem->m_nMixGroupIndex;

	}

	mixervalues_t mixValues;
	MXR_GetValuesFromMixGroupIndex( &mixValues, pStructMem->m_nMixGroupIndex );

	pStructMem->m_flOutputLevel[0] = mixValues.level;
	pStructMem->m_flOutputVolume[0] = mixValues.volume;
	pStructMem->m_flOutputDSP[0] = mixValues.dsp;

	pChannel->last_mixgroupid = pStructMem->m_nMixGroupIndex;
}

void CSosOperatorMixGroup::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	CSosOperatorMixGroup_t *pStructMem = (CSosOperatorMixGroup_t *)pVoidMem;
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*smixgroup: %s\n", nLevel, "    ", MXR_GetGroupnameFromId( pStructMem->m_nMixGroupIndex ));

}
void CSosOperatorMixGroup::OpHelp( ) const
{

}
void CSosOperatorMixGroup::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorMixGroup_t *pStructMem = (CSosOperatorMixGroup_t *)pVoidMem;
	bool bHasMixGroupIndex = false;

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
				else if ( !V_strcasecmp( pParamString, "set_mixgroup_to_channel" ) )
				{
					if ( !V_strcasecmp( pValueString, "false" ) )
					{
						pStructMem->m_bSetMixGroupOnChannel = false;
					}
					else if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bSetMixGroupOnChannel = true;
					}
				}
				else if ( !V_strcasecmp( pParamString, "mixgroup" ) )
				{
					pStructMem->m_nMixGroupIndex = MXR_GetMixgroupFromName( pValueString );
					if ( pStructMem->m_nMixGroupIndex > -1 )
					{
						bHasMixGroupIndex = true;
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

	/*	if ( !bHasMixGroupIndex )
	{
	MXR_GetMixGroupFromSoundsource( pStructMem->m_pChannel );
	pStructMem->m_nMixGroupIndex = MXR_GetFirstValidMixGroup( pStructMem->m_pChannel );
	}
	*/
}
