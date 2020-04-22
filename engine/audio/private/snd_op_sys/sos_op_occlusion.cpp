//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_occlusion.h"

#include "snd_dma.h"
#include "../../cl_splitscreen.h"
#include "../../enginetrace.h"
#include "render.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"




extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;

float S_CalcOcclusion( int nSlot, channel_t *ch, const Vector &vecListenerOrigin, Vector vSoundSource, float flOccludedDBLoss );


//-----------------------------------------------------------------------------
// CSosOperatorOcclusion
// Setting a single, simple scratch pad Expression 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorOcclusion, "calc_occlusion" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorOcclusion, m_flInputTraceInterval, SO_SINGLE, "input_trace_interval" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorOcclusion, m_flInputScalar, SO_SINGLE, "input_scalar" )
SOS_REGISTER_INPUT_FLOAT( CSosOperatorOcclusion, m_flInputPosition, SO_VEC3, "input_position" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorOcclusion, m_flOutput, SO_SINGLE, "output" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorOcclusion, "calc_occlusion"  )

void CSosOperatorOcclusion::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorOcclusion_t *pStructMem = (CSosOperatorOcclusion_t *)pVoidMem;

	SOS_INIT_INPUT_VAR( m_flInputScalar, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInputPosition, SO_VEC3, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutput, SO_SINGLE, 1.0 )

	SOS_INIT_INPUT_VAR( m_flInputTraceInterval, SO_SINGLE, -1.0 )
	pStructMem->m_flLastTraceTime = -1.0;
	pStructMem->m_flOccludedDBLoss = snd_obscured_gain_db.GetFloat();
	
}

void CSosOperatorOcclusion::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorOcclusion_t *pStructMem = (CSosOperatorOcclusion_t *)pVoidMem;

	if( !pChannel )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s requires valid channel pointer, being called without one\n", pStack->GetOperatorName( nOpIndex ));
		return;
	}


	// scalar of 0.0 = off, do nothing
	float flScalar = pStructMem->m_flInputScalar[0];
	if( flScalar == 0.0 )
	{
		pStructMem->m_flOutput[0] = 1.0;
		return;
	}

	float flCurHostTime = g_pSoundServices->GetHostTime();


	bool bIntervalHasPassed = ( ( pStructMem->m_flInputTraceInterval[0] >= 0.0 &&
								( pStructMem->m_flInputTraceInterval[0] <= ( flCurHostTime - pStructMem->m_flLastTraceTime ) ))
								|| pChannel->flags.bfirstpass );			
								
	bool bOkToTrace = SND_ChannelOkToTrace( pChannel );

	bool bDoNewTrace = true;

	// During signon just apply regular state machine since world hasn't been
	//  created or settled yet...
	if ( !SND_IsInGame() && !toolframework->InToolMode() )
	{
		bDoNewTrace = false;
	}

	if( ( !pChannel->flags.bfirstpass && !pChannel->flags.isSentence ) && !bIntervalHasPassed || !bOkToTrace )
	{
		bDoNewTrace = false;
	}


	float flResult = 0.0;
	if( bDoNewTrace )
	{

// 		Log_Msg( LOG_SND_OPERATORS, "UPDATING: Sound operator %s\n", pStack->GetOperatorName( nOpIndex ));

		Vector vOrigin;
		vOrigin[0] = pStructMem->m_flInputPosition[0];
		vOrigin[1] = pStructMem->m_flInputPosition[1];
		vOrigin[2] = pStructMem->m_flInputPosition[2];

		// find the loudest, ie: least occluded ss player
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{

			float flGain = S_CalcOcclusion( hh,
				pChannel,
				pScratchPad->m_vPlayerOrigin[ hh ],
				vOrigin,
				pStructMem->m_flOccludedDBLoss );

			// inverse scale
			flGain = 1.0 - ( ( 1.0 - flGain ) * flScalar );

			flGain = SND_FadeToNewGain( &(pChannel->gain[ hh ]), pChannel, flGain );

			flResult = MAX( flResult, flGain );

		}
		pStructMem->m_flLastTraceTime = flCurHostTime;
	}
	else
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			float flGain = SND_FadeToNewGain(  &(pChannel->gain[ hh ] ), pChannel, -1.0 );
			flResult = MAX( flResult, flGain );
		}
	}

	pStructMem->m_flOutput[0] = flResult;


}

void CSosOperatorOcclusion::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	//	CSosOperatorOcclusion_t *pStructMem = (CSosOperatorOcclusion_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

}
void CSosOperatorOcclusion::OpHelp( ) const
{

}
void CSosOperatorOcclusion::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorOcclusion_t *pStructMem = (CSosOperatorOcclusion_t *)pVoidMem;
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
				else if ( !V_strcasecmp( pParamString, "occlusio_db_loss" ) )
				{
					pStructMem->m_flOccludedDBLoss = V_atof( pValueString ) ;
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

float S_CalcOcclusion( int nSlot, channel_t *ch, const Vector &vecListenerOrigin, Vector vSoundSource, float flOccludedDBLoss )
{
	float gain = 1.0;
	int count = 1;

	trace_t tr;
	CTraceFilterWorldOnly filter;	// UNDONE: also test for static props?
	Ray_t ray;
	ray.Init( MainViewOrigin( nSlot ), vSoundSource );
	g_pEngineTraceClient->TraceRay( ray, MASK_BLOCK_AUDIO, &filter, &tr );
	// total traces this frame
	g_snd_trace_count++;				

	if (tr.DidHit() && tr.fraction < 0.99)
	{
		// can't see center of sound source:
		// build extents based on dB sndlvl of source,
		// test to see how many extents are visible,
		// drop gain by snd_gain_db per extent hidden

		Vector vSoundSources[4];
		soundlevel_t sndlvl = DIST_MULT_TO_SNDLVL( ch->dist_mult );
		float radius;
		Vector vsrc_forward;
		Vector vsrc_right;
		Vector vsrc_up;
		Vector vecl;
		Vector vecr;
		Vector vecl2;
		Vector vecr2;
		int i;

		// get radius

		if ( ch->radius > 0 )
			radius = ch->radius;
		else
			radius = dB_To_Radius( sndlvl);		// approximate radius from soundlevel

		// set up extent vSoundSources - on upward or downward diagonals, facing player

		for (i = 0; i < 4; i++)
			vSoundSources[i] = vSoundSource;

		// vsrc_forward is normalized vector from sound source to listener

		VectorSubtract( vecListenerOrigin, vSoundSource, vsrc_forward );
		VectorNormalize( vsrc_forward );
		VectorVectors( vsrc_forward, vsrc_right, vsrc_up );

		VectorAdd( vsrc_up, vsrc_right, vecl );

		// if src above listener, force 'up' vector to point down - create diagonals up & down

		if ( vSoundSource.z > vecListenerOrigin.z + (10 * 12) )
			vsrc_up.z = -vsrc_up.z;

		VectorSubtract( vsrc_up, vsrc_right, vecr );
		VectorNormalize( vecl );
		VectorNormalize( vecr );

		// get diagonal vectors from sound source 

		vecl2 = radius * vecl;
		vecr2 = radius * vecr;
		vecl = (radius / 2.0) * vecl;
		vecr = (radius / 2.0) * vecr;

		// vSoundSources from diagonal vectors

		vSoundSources[0] += vecl;
		vSoundSources[1] += vecr;
		vSoundSources[2] += vecl2;
		vSoundSources[3] += vecr2;

		// drop gain for each point on radius diagonal that is obscured

		for (count = 0, i = 0; i < 4; i++)
		{
			// UNDONE: some vSoundSources are in walls - in this case, trace from the wall hit location

			Ray_t ray;
			ray.Init( MainViewOrigin( nSlot ), vSoundSources[i] );
			g_pEngineTraceClient->TraceRay( ray, MASK_BLOCK_AUDIO, &filter, &tr );

			if (tr.DidHit() && tr.fraction < 0.99 && !tr.startsolid )
			{
				count++;	// skip first obscured point: at least 2 points + center should be obscured to hear db loss
				if (count > 1)
					gain = gain * dB_To_Gain( flOccludedDBLoss );
			}
		}
	}


	if ( snd_showstart.GetInt() == 7)
	{
		static float g_drop_prev = 0;
		float drop = (count-1) * flOccludedDBLoss;

		if (drop != g_drop_prev)
		{
			DevMsg( "dB drop: %1.4f \n", drop);
			g_drop_prev = drop;
		}
	}
	return gain;

}