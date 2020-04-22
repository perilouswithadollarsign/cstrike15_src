//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_output.h"

#include "snd_dma.h"
// #include "cdll_engine_int.h"
#include "../../debugoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;

//-----------------------------------------------------------------------------
// CSosOperatorOutput
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorOutput, "sys_output" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorOutput, m_flInputSpeakers, SO_SPEAKERS, "input_speakers" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorOutput, m_flInputVec3, SO_VEC3, "input_vec3")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorOutput, m_flInputFloat, SO_SINGLE, "input_float")

SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorOutput, "sys_output"  )

void CSosOperatorOutput::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorOutput_t *pStructMem = (CSosOperatorOutput_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flInputFloat, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputVec3, SO_VEC3, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputSpeakers, SO_SPEAKERS, 0.0 )

	pStructMem->m_nOutType = SOS_OUT_NONE;

}

void CSosOperatorOutput::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorOutput_t *pStructMem = (CSosOperatorOutput_t *)pVoidMem;

	if( pStructMem->m_nOutType == SOS_OUT_DELAY )
	{
		pScratchPad->m_flDelay = pStructMem->m_flInputFloat[0];
	}
	else if( pStructMem->m_nOutType == SOS_OUT_STOPHOLD )
	{
		SOSStopType_t stopType = pStack->GetStopType();
		if( stopType != SOS_STOP_NONE && stopType != SOS_STOP_FORCE && stopType != SOS_STOP_QUEUE  )
		{
			pStack->SetStopType( pStructMem->m_flInputFloat[0] > 0.0 ? SOS_STOP_HOLD : SOS_STOP_NORM );
		}
	}
	else if( pStructMem->m_nOutType == SOS_OUT_BLOCK_START )
	{
		pScratchPad->m_bBlockStart = pStructMem->m_flInputFloat[0] > 0.0 ? true : false;
	}
	else
	{
		if( !pChannel )
		{
			Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s requires valid channel pointer, being called without one\n", pStack->GetOperatorName( nOpIndex ));
			return;
		}

		switch( pStructMem->m_nOutType )
		{
		case SOS_OUT_POSITION:
			pChannel->origin[0] = pStructMem->m_flInputVec3[0];
			pChannel->origin[1] = pStructMem->m_flInputVec3[1];
			pChannel->origin[2] = pStructMem->m_flInputVec3[2];
			break;
		case SOS_OUT_DSP:
			pChannel->dspmix = pStructMem->m_flInputFloat[0];
			break;
		case SOS_OUT_SPEAKERS: {
			float fScaledVolumes[CCHANVOLUMES / 2];
			for (int i = 0; i != CCHANVOLUMES / 2; ++i)
			{
				fScaledVolumes[i] = pStructMem->m_flInputSpeakers[i] * 255.0f;
			}
			ChannelSetVolTargets(pChannel, fScaledVolumes, IFRONT_LEFT, CCHANVOLUMES / 2);
			break;
		}
		case SOS_OUT_FACING:
			pChannel->dspface = (pStructMem->m_flInputFloat[0] * 2.0) - 1.0;
			break;
		case SOS_OUT_DISTVAR:
			pChannel->distmix = pStructMem->m_flInputFloat[0];
			break;
		case SOS_OUT_PITCH:
			pChannel->basePitch = (int)(pStructMem->m_flInputFloat[0] * 100);
			pChannel->pitch = pStructMem->m_flInputFloat[0];
			break;
		case SOS_OUT_MIXLAYER_TRIGGER:
			pChannel->last_vol = pStructMem->m_flInputFloat[0];
			break;
		case SOS_OUT_SAVE_RESTORE:
			pChannel->flags.m_bShouldSaveRestore = pStructMem->m_flInputFloat[0] > 0.0 ? true : false;
			break;
		case SOS_OUT_PHONON_XFADE: {
			pChannel->hrtf.lerp = pStructMem->m_flInputFloat[0];
			break;
		}

		}
	}


}


void CSosOperatorOutput::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorOutput_t *pStructMem = (CSosOperatorOutput_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

	const char *pOutTypeString = "none";
	switch( pStructMem->m_nOutType )
	{
	case SOS_OUT_VOLUME:
		pOutTypeString = "volume";
		break;
	case SOS_OUT_POSITION:
		pOutTypeString = "position";
		break;
	case SOS_OUT_DSP:
		pOutTypeString = "dsp";
		break;
	case SOS_OUT_SPEAKERS:
		pOutTypeString = "speakers";
		break;
	case SOS_OUT_FACING:
		pOutTypeString = "facing";
		break;
	case SOS_OUT_DISTVAR:
		pOutTypeString = "distvar";
		break;
	case SOS_OUT_PITCH:
		pOutTypeString = "pitch";
		break;
	case SOS_OUT_DELAY:
		pOutTypeString = "delay";
		break;
	case SOS_OUT_STOPHOLD:
		pOutTypeString = "stop_hold";
		break;
	case SOS_OUT_MIXLAYER_TRIGGER:
		pOutTypeString = "mixlayer_trigger";
		break;
	case SOS_OUT_SAVE_RESTORE:
		pOutTypeString = "save_restore";
		break;
	case SOS_OUT_BLOCK_START:
		pOutTypeString = "block_start";
		break;
	case SOS_OUT_PHONON_XFADE:
		pOutTypeString = "phonon_xfade";
		break;
	}
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sOutput: %s\n", nLevel, "    ", pOutTypeString );

}

void CSosOperatorOutput::OpHelp( ) const
{
}

void CSosOperatorOutput::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorOutput_t *pStructMem = (CSosOperatorOutput_t *)pVoidMem;

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
				else if ( !V_strcasecmp( pParamString, "output" ) )
				{
					if ( !V_strcasecmp( pValueString, "volume" ) )
					{
						pStructMem->m_nOutType = SOS_OUT_VOLUME;
					}
					else if ( !V_strcasecmp( pValueString, "dsp" ) )
					{
						pStructMem->m_nOutType = SOS_OUT_DSP;
					}
					else if ( !V_strcasecmp( pValueString, "position" ) )
					{
						pStructMem->m_nOutType = SOS_OUT_POSITION;
					}
					else if ( !V_strcasecmp( pValueString, "speakers" ) )
					{
						pStructMem->m_nOutType = SOS_OUT_SPEAKERS;
					}
					else if ( !V_strcasecmp( pValueString, "facing" ) )
					{
						pStructMem->m_nOutType = SOS_OUT_FACING;
					}
					else if ( !V_strcasecmp( pValueString, "distvar" ) )
					{
						pStructMem->m_nOutType = SOS_OUT_DISTVAR;
					}
					else if ( !V_strcasecmp( pValueString, "pitch" ) )
					{
						pStructMem->m_nOutType = SOS_OUT_PITCH;
					}
					else if ( !V_strcasecmp( pValueString, "delay" ) )
					{
						pStructMem->m_nOutType = SOS_OUT_DELAY;
					}
					else if ( !V_strcasecmp( pValueString, "stop_hold" ) )
					{
						pStructMem->m_nOutType = SOS_OUT_STOPHOLD;
					}
					else if ( !V_strcasecmp( pValueString, "mixlayer_trigger" ) )
					{
						pStructMem->m_nOutType = SOS_OUT_MIXLAYER_TRIGGER;
					}
					else if ( !V_strcasecmp( pValueString, "save_restore" ) )
					{
						pStructMem->m_nOutType = SOS_OUT_SAVE_RESTORE;
					}
					else if ( !V_strcasecmp( pValueString, "block_start" ) )
					{
						pStructMem->m_nOutType = SOS_OUT_BLOCK_START;
					}
					else if (!V_strcasecmp(pValueString, "phonon_xfade"))
					{
						pStructMem->m_nOutType = SOS_OUT_PHONON_XFADE;
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
