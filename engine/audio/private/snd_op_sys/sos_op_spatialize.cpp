//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_spatialize.h"

#include "snd_dma.h"
#include "../../cl_splitscreen.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


//-----------------------------------------------------------------------------
// CSosOperatorSpatializeSpeakers
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorSpatializeSpeakers, "calc_spatialize_speakers" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorSpatializeSpeakers, m_flOutput, SO_SPEAKERS, "output" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorSpatializeSpeakers, m_flInputRadiusMax, SO_SINGLE, "input_radius_max" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorSpatializeSpeakers, m_flInputRadiusMin, SO_SINGLE, "input_radius_min" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorSpatializeSpeakers, m_flInputTimeStartStereoSpread, SO_SINGLE, "input_time_start_stereo_spread")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorSpatializeSpeakers, m_flInputTimeFinishStereoSpread, SO_SINGLE, "input_time_finish_stereo_spread")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorSpatializeSpeakers, m_flInputFinalStereoSpread, SO_SINGLE, "input_final_stereo_spread")
SOS_REGISTER_INPUT_FLOAT( CSosOperatorSpatializeSpeakers, m_flInputRearStereoScale, SO_SINGLE, "input_rear_stereo_scale" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorSpatializeSpeakers, m_flInputDistance, SO_SINGLE, "input_distance" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorSpatializeSpeakers, m_flInputPosition, SO_VEC3, "input_position" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorSpatializeSpeakers, "calc_spatialize_speakers"  )

void CSosOperatorSpatializeSpeakers::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorSpatializeSpeakers_t *pStructMem = (CSosOperatorSpatializeSpeakers_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flInputRadiusMax, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputRadiusMin, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputTimeStartStereoSpread, SO_SINGLE, 0.0)
	SOS_INIT_INPUT_VAR( m_flInputTimeFinishStereoSpread, SO_SINGLE, 0.0)
	SOS_INIT_INPUT_VAR( m_flInputFinalStereoSpread, SO_SINGLE, 0.0)
	SOS_INIT_INPUT_VAR( m_flInputRearStereoScale, SO_SINGLE, 0.75 )
	SOS_INIT_INPUT_VAR( m_flInputDistance, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputPosition, SO_VEC3, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SPEAKERS, 0.0 )
}

void CSosOperatorSpatializeSpeakers::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorSpatializeSpeakers_t *pStructMem = (CSosOperatorSpatializeSpeakers_t *)pVoidMem;

	float flRadius = pStructMem->m_flInputRadiusMax[0];
	float flRadiusMin = pStructMem->m_flInputRadiusMin[0];
	float flDist = pStructMem->m_flInputDistance[0];
	float flRearStereoScale = pStructMem->m_flInputRearStereoScale[0];

	flRadius = flRadius > 0.0 ? flRadius : 0.0;
	flRadiusMin = flRadiusMin > 0.0 ? flRadiusMin : 0.0;
	flRadiusMin = flRadiusMin < flRadius ? flRadiusMin : flRadius;

	float flMono = 0.0;
	if ( flDist <= flRadiusMin )
	{
		flMono = 1.0;
	}
	else if ( flDist >= flRadius )
	{
		flMono = 0.0;
	}
	else
	{
		flMono = ((flDist - flRadiusMin) / ( flRadius - flRadiusMin ));
	}

	if (flDist > flRadiusMin && pStructMem->m_flInputTimeStartStereoSpread[0] != pStructMem->m_flInputTimeFinishStereoSpread[0])
	{
		float flElapsedSpatialized = pStructMem->m_flInputTimeStartStereoSpread[0];
		float flElapsedMono = pStructMem->m_flInputTimeFinishStereoSpread[0];
		float flMax = flElapsedMono > flElapsedSpatialized ? flElapsedMono : flElapsedSpatialized;
		float flMin = flElapsedMono < flElapsedSpatialized ? flElapsedMono : flElapsedSpatialized;
		float flBeginSpatialized = flElapsedSpatialized >= flElapsedMono ? 0.0f : 1.0f;
		float flEndSpatialized = 1.0f - flBeginSpatialized * pStructMem->m_flInputFinalStereoSpread[0];

		float flSpatialized = 1.0 - flMono;

		float flElapsed = 0.0f;
		if (pChannel->sfx && pChannel->sfx->pSource && pChannel->pMixer)
		{
			const int nSamples = pChannel->sfx->pSource->SampleCount();
			const int nPos = pChannel->pMixer->GetSamplePosition();

			if (nSamples > 0)
			{
				flElapsed = float(nPos) / float(nSamples);
			}
		}

		if (flElapsed <= flMin)
		{
			flSpatialized *= flBeginSpatialized;
		}
		else if (flElapsed >= flMax)
		{
			flSpatialized *= flEndSpatialized;
		}
		else
		{
			float ratio = (flElapsed - flMin) / (flMax - flMin);
			flSpatialized *= ratio*flEndSpatialized + (1.0 - ratio)*flBeginSpatialized;
		}

		flMono = 1.0f - flSpatialized;
	}

// 	if ( flRadius > 0.0 && (  flDist < flRadius ) )
// 	{
// 		float interval = flRadius - flRadiusMin;
// 		flMono = flDist - interval;
// 
// 		if ( flMono < 0.0 )
// 			flMono = 0.0;
// 
// 		flMono /= interval;
// 
// 		// flMono is 0.0 -> 1.0 from radius 100% to radius 50%
// 		flMono = 1.0 - flMono;
// 	}

	// fill out channel volumes for single sound source location
	Vector vSourceVector;
	Vector vPosition;
	vPosition[0] = pStructMem->m_flInputPosition[0];
	vPosition[1] = pStructMem->m_flInputPosition[1];
	vPosition[2] = pStructMem->m_flInputPosition[2];

	float build_volumes[ MAX_SPLITSCREEN_CLIENTS ][SO_MAX_SPEAKERS] = { 0.0 };

	// collect up all ss players and merge them here
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		VectorSubtract( vPosition, pScratchPad->m_vPlayerOrigin[ hh ], vSourceVector );
		VectorNormalize( vSourceVector );
		Device_SpatializeChannel( hh, &build_volumes[hh][0], vSourceVector, flMono, flRearStereoScale );
	}
	SND_MergeVolumes( build_volumes, pStructMem->m_flOutput );

}

void CSosOperatorSpatializeSpeakers::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	// CSosOperatorSpatializeSpeakers_t *pStructMem = (CSosOperatorSpatializeSpeakers_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

}
void CSosOperatorSpatializeSpeakers::OpHelp( ) const
{

}


void CSosOperatorSpatializeSpeakers::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorSpatializeSpeakers_t *pStructMem = (CSosOperatorSpatializeSpeakers_t *)pVoidMem;

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
