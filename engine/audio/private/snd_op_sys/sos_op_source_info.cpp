//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "sos_op.h"
#include "sos_op_source_info.h"

#include "snd_dma.h"
#include "soundinfo.h"
#include "cdll_engine_int.h"
#include "../../debugoverlay.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


ConVar snd_sos_show_source_info("snd_sos_show_source_info", "0" );

//-----------------------------------------------------------------------------
// CSosOperatorSourceInfo
//-----------------------------------------------------------------------------

SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorSourceInfo, "get_source_info" )
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorSourceInfo, m_flOutEntityIndex, SO_SINGLE, "output_entity_index" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorSourceInfo, m_flOutPosition, SO_VEC3, "output_position" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorSourceInfo, m_flOutAngles, SO_VEC3, "output_angles" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorSourceInfo, m_flOutRadius, SO_SINGLE, "output_radius" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorSourceInfo, m_flOutVolume, SO_SINGLE, "output_volume" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorSourceInfo, m_flOutLevel, SO_SINGLE, "output_level" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorSourceInfo, m_flOutPitch, SO_SINGLE, "output_pitch" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorSourceInfo, m_flOutSourceCount, SO_SINGLE, "output_source_count" );
SOS_REGISTER_INPUT_FLOAT( CSosOperatorSourceInfo, m_flInputSourceIndex, SO_SINGLE, "input_source_index" );
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorSourceInfo, "get_source_info"  )

void CSosOperatorSourceInfo::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorSourceInfo_t *pStructMem = (CSosOperatorSourceInfo_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutEntityIndex, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutPosition, SO_VEC3, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutAngles, SO_VEC3, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutRadius, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutVolume, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutLevel, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutPitch, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutSourceCount, SO_SINGLE, 1.0 )
	SOS_INIT_INPUT_VAR( m_flInputSourceIndex, SO_SINGLE, 0.0 )
	pStructMem->m_nSource = SO_SRC_EMITTER;
	pStructMem->m_bGameExtraOrigins = true;
}

void CSosOperatorSourceInfo::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorSourceInfo_t *pStructMem = (CSosOperatorSourceInfo_t *)pVoidMem;

	bool bUseEntity = ( pStructMem->m_nSource == SO_SRC_ENTITY );

	int nSourceCount = pScratchPad->m_UtlVecMultiOrigins.Count();

	// have we gotten our sources yet?
	if( nSourceCount < 1 )
	{

		// update channel's position in case ent that made the sound is moving.
		QAngle source_angles;
		source_angles.Init(0.0, 0.0, 0.0);

		// default entity to emitter in case there is no entity available
		int nSoundSource = pScratchPad->m_nSoundSource;
		int nEntChannel = pScratchPad->m_nChannel;
		const char *pSoundName = pScratchPad->m_emitterInfoSoundName;
		Vector vOrigin = pScratchPad->m_vEmitterInfoOrigin;
		Vector vDirection = pScratchPad->m_vEmitterInfoDirection;
		short nMasterVolume = (short) pScratchPad->m_flEmitterInfoMasterVolume * 255;
		soundlevel_t nSoundLevel = pScratchPad->m_vEmitterInfoSoundLevel;
		bool bIsLooping = pScratchPad->m_bIsLooping;
		float flPitch = pScratchPad->m_bIsLooping;
		int nSpeakerEntity = pScratchPad->m_nEmitterInfoSpeakerEntity;

		Vector vListenerOrigin = pScratchPad->m_vBlendedListenerOrigin;  // HACK FOR SPLITSCREEN, only client\c_func_tracktrain.cpp(100):		CalcClosestPointOnLine( info.info.vListenerOrigin, vecStart, vecEnd, *info.pOrigin, &t );  every looked at listener origin in this structure...

		Vector vEntOrigin = vOrigin;
		float flRadius = 0.0;

		SpatializationInfo_t si;
		
		si.info.Set( 
			nSoundSource,
			nEntChannel,
			pSoundName,
			vOrigin,
			vDirection,
			nMasterVolume,
			nSoundLevel,
			bIsLooping,
			flPitch,
			vListenerOrigin,
			nSpeakerEntity, 
			0 ); // FIX SEEDING?

		si.type = SpatializationInfo_t::SI_INSPATIALIZATION;
		si.pOrigin = &vEntOrigin;
		si.pAngles = &source_angles;
		si.pflRadius = &flRadius;

		CUtlVector< Vector > utlVecMultiOrigins;
		si.m_pUtlVecMultiOrigins = &pScratchPad->m_UtlVecMultiOrigins;

		CUtlVector< Vector > utlVecMultiAngles;
		//		si.m_pUtlVecMultiAngles = &pScratchPad->m_UtlVecMultiAngles;

		// TODO: morasky, verify it's been handled
		// 	if ( pChannel->soundsource != 0 && pChannel->radius == 0 )
		// 	{
		// 		si.pflRadius = &pChannel->radius;
		// 	}

		// this returns the position of the entity in vEntOrigin
		//VPROF_("SoundServices->GetSoundSpatializtion", 2, VPROF_BUDGETGROUP_OTHER_SOUND, false, BUDGETFLAG_OTHER );
		bUseEntity &= g_pSoundServices->GetSoundSpatialization( nSoundSource, si );	
		

		if( !bUseEntity )
		{
			si.pOrigin = &si.info.vOrigin;
		}

#ifndef DEDICATED
		// extra, non-entity related, game specific processing (portal2, multi-source, etc.)
		if( pStructMem->m_bGameExtraOrigins )
		{
			g_ClientDLL->GetSoundSpatialization( si );
		}
#endif

		// TODO: morasky, verify it's been handled
		// 	// just about pChannel->direction?!
		// //	if ( pChannel->flags.bUpdatePositions )
		// 	{
		// 		// angles -> forward vector
		//  		AngleVectors( source_angles, &pChannel->direction );
		// 		//XXX		pChannel->origin = vEntOrigin;
		// 	}
		// //	else
		// 	{
		// 		// vector -> angles
		// 		VectorAngles( pChannel->direction, source_angles );
		// 	}

		// ???
		// 	if ( IsPC() && pChannel->userdata != 0 )
		// 	{
		// 		g_pSoundServices->GetToolSpatialization( pChannel->userdata, pChannel->guid, si );
		// 		if ( pChannel->flags.bUpdatePositions )
		// 		{
		// 			AngleVectors( source_angles, &pChannel->direction );
		// 			//XXX			pChannel->origin = vEntOrigin;
		// 		}
		// 	}

		pScratchPad->m_vEntityInfoAngle = source_angles;
		pScratchPad->m_vEntityInfoOrigin = vEntOrigin;
		pScratchPad->m_flEntityInfoRadius = flRadius;

	}

	nSourceCount = pScratchPad->m_UtlVecMultiOrigins.Count();
	pStructMem->m_flOutSourceCount[0] = (float) nSourceCount;


	int nCurIndex = (int) pStructMem->m_flInputSourceIndex[0];
	// BIG ERROR CHECK HERE!


	if ( nCurIndex == 0 )
	{
		if ( pStructMem->m_nSource == SO_SRC_ENTITY )
		{
			pStructMem->m_flOutPosition[0] = pScratchPad->m_vEntityInfoOrigin[0];
			pStructMem->m_flOutPosition[1] = pScratchPad->m_vEntityInfoOrigin[1];
			pStructMem->m_flOutPosition[2] = pScratchPad->m_vEntityInfoOrigin[2];

			pStructMem->m_flOutAngles[0] = pScratchPad->m_vEntityInfoAngle[0];
			pStructMem->m_flOutAngles[1] = pScratchPad->m_vEntityInfoAngle[1];
			pStructMem->m_flOutAngles[2] = pScratchPad->m_vEntityInfoAngle[2];

			pStructMem->m_flOutRadius[0] = pScratchPad->m_flEntityInfoRadius;
		}
		else if ( pStructMem->m_nSource == SO_SRC_EMITTER )
		{
			pStructMem->m_flOutPosition[0] = pScratchPad->m_vEmitterInfoOrigin[0];
			pStructMem->m_flOutPosition[1] = pScratchPad->m_vEmitterInfoOrigin[1];
			pStructMem->m_flOutPosition[2] = pScratchPad->m_vEmitterInfoOrigin[2];

			QAngle source_angles;
			VectorAngles( pScratchPad->m_vEmitterInfoDirection, source_angles );
			pStructMem->m_flOutAngles[0] = source_angles[0];
			pStructMem->m_flOutAngles[1] = source_angles[1];
			pStructMem->m_flOutAngles[2] = source_angles[2];

			pStructMem->m_flOutRadius[0] = pScratchPad->m_nEmitterInfoRadius;
		}
	}
	else
	{

		pStructMem->m_flOutPosition[0] = pScratchPad->m_UtlVecMultiOrigins[nCurIndex - 1][0];
		pStructMem->m_flOutPosition[1] = pScratchPad->m_UtlVecMultiOrigins[nCurIndex - 1][1];
		pStructMem->m_flOutPosition[2] = pScratchPad->m_UtlVecMultiOrigins[nCurIndex - 1][2];

		// 		pStructMem->m_flOutAngles[0] = pScratchPad->m_vEntityInfoAngle[0];
		// 		pStructMem->m_flOutAngles[1] = pScratchPad->m_vEntityInfoAngle[1];
		// 		pStructMem->m_flOutAngles[2] = pScratchPad->m_vEntityInfoAngle[2];

		// 		pStructMem->m_flOutRadius[0] = pScratchPad->m_flEntityInfoRadius;

	}
	pStructMem->m_flOutVolume[0] = pScratchPad->m_flEmitterInfoMasterVolume;
	pStructMem->m_flOutLevel[0] = pScratchPad->m_vEmitterInfoSoundLevel;
	pStructMem->m_flOutPitch[0] = pScratchPad->m_flEmitterInfoPitch;
	pStructMem->m_flOutEntityIndex[0] = (float) pScratchPad->m_nSoundSource;


#ifndef DEDICATED
	if( snd_sos_show_source_info.GetInt() )
	{
		Vector vDebug;
		vDebug[0] = pStructMem->m_flOutPosition[0];
		vDebug[1] = pStructMem->m_flOutPosition[1];
		vDebug[2] = pStructMem->m_flOutPosition[2];
		CDebugOverlay::AddTextOverlay( vDebug, 2.0f, g_pSoundEmitterSystem->GetSoundNameForHash( pStack->GetScriptHash() ) );
	}
#endif

}



void CSosOperatorSourceInfo::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorSourceInfo_t *pStructMem = (CSosOperatorSourceInfo_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );

	switch( pStructMem->m_nSource )
	{
	case SO_SRC_EMITTER:
		Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sSource: emitter\n", nLevel, "    " );	
		break;
	case SO_SRC_ENTITY:
		Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sSource: entity\n", nLevel, "    " );	
		break;
	}
}
void CSosOperatorSourceInfo::OpHelp( ) const
{
}

void CSosOperatorSourceInfo::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorSourceInfo_t *pStructMem = (CSosOperatorSourceInfo_t *)pVoidMem;

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
				else if ( !V_strcasecmp( pParamString, "source" ) )
				{
					if ( !V_strcasecmp( pValueString, "none" ) )
					{
						pStructMem->m_nSource = SO_SRC_NONE;
					}
					else if ( !V_strcasecmp( pValueString, "emitter" ) )
					{
						pStructMem->m_nSource = SO_SRC_EMITTER;
					}
					else if ( !V_strcasecmp( pValueString, "entity" ) )
					{
						pStructMem->m_nSource = SO_SRC_ENTITY;
					}
				}
				else if ( !V_strcasecmp( pParamString, "game_multi_origin" ) )
				{
					if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bGameExtraOrigins = true;
					}
					else
					{
						pStructMem->m_bGameExtraOrigins = false;
					}
				}
				// 				else
				// 				{
				// 					S_GetFloatFromString( pStructMem->m_vValues, pValueString, SO_POSITION_ARRAY_SIZE );
				// 				}
				// 				else if ( !V_strcasecmp( pParamString, "xpos" ) )
				// 				{
				// 					pStructMem->m_vValues[XPOSITION] = RandomInterval( ReadInterval( pValueString ) );
				// 				}
				// 				else if ( !V_strcasecmp( pParamString, "ypos" ) )
				// 				{
				// 					pStructMem->m_vValues[YPOSITION] = RandomInterval( ReadInterval( pValueString ) );
				// 				}
				// 				else if ( !V_strcasecmp( pParamString, "zpos" ) )
				// 				{
				// 					pStructMem->m_vValues[ZPOSITION] = RandomInterval( ReadInterval( pValueString ) );
				// 				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s, unknown sound operator attribute %s\n",  pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}

