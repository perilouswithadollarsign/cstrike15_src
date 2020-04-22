//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"
#include "client.h"

#include "sos_op.h"
#include "sos_op_entity_info.h"
#include "icliententitylist.h"
#include "icliententity.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;


// ConVar snd_sos_show_source_info("snd_sos_show_source_info", "0" );

//-----------------------------------------------------------------------------
// CSosOperatorEntityInfo
//-----------------------------------------------------------------------------

SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorEntityInfo, "game_entity_info" )

SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntityInfo, m_flOutPosition, SO_VEC3, "output_position" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntityInfo, m_flOutPosition_x, SO_SINGLE, "output_position_x" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntityInfo, m_flOutPosition_y, SO_SINGLE, "output_position_y" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntityInfo, m_flOutPosition_z, SO_SINGLE, "output_position_z" );

SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntityInfo, m_flOutAngles, SO_VEC3, "output_angles" );

SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntityInfo, m_flOutVelocityVector, SO_VEC3, "output_velocity_vector" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntityInfo, m_flOutVelocityVector_x, SO_SINGLE, "output_velocity_vector_x" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntityInfo, m_flOutVelocityVector_y, SO_SINGLE, "output_velocity_vector_y" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntityInfo, m_flOutVelocityVector_z, SO_SINGLE, "output_velocity_vector_z" );

SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntityInfo, m_flOutVelocity, SO_SINGLE, "output_velocity" );
SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorEntityInfo, m_flOutVelocityXY, SO_SINGLE, "output_velocity_xy" );

SOS_REGISTER_INPUT_FLOAT( CSosOperatorEntityInfo, m_flInputEntityIndex, SO_SINGLE, "input_entity_index" );

SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorEntityInfo, "game_entity_info"  )


void CSosOperatorEntityInfo::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorEntityInfo_t *pStructMem = (CSosOperatorEntityInfo_t *)pVoidMem;

	SOS_INIT_OUTPUT_VAR( m_flOutPosition, SO_VEC3, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutPosition_x, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutPosition_y, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutPosition_z, SO_SINGLE, 0.0 )

	SOS_INIT_OUTPUT_VAR( m_flOutAngles, SO_VEC3, 0.0 )

	SOS_INIT_OUTPUT_VAR( m_flOutVelocity, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutVelocityVector, SO_VEC3, 0.0 )

	SOS_INIT_OUTPUT_VAR( m_flOutVelocityVector_x, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutVelocityVector_y, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutVelocityVector_z, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutVelocityXY, SO_SINGLE, 0.0 )

	SOS_INIT_INPUT_VAR( m_flInputEntityIndex, SO_SINGLE, 0.0 )

	pStructMem->m_vecPrevPosition.Init();
	pStructMem->m_flPrevTime = g_pSoundServices->GetHostTime();
}

void CSosOperatorEntityInfo::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
#ifndef DEDICATED
	CSosOperatorEntityInfo_t *pStructMem = (CSosOperatorEntityInfo_t *)pVoidMem;
	
	// for future use with splitscreen
	int nCurIndex = (int) pStructMem->m_flInputEntityIndex[0];
	if( nCurIndex < 0 )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: %s, input_entity_index %i invalid\n", pStack->GetOperatorName( nOpIndex ), nCurIndex );
		return;
	}

	Vector vEntityAbsPos;
	vEntityAbsPos.Init( 0.0, 0.0, 0.0 );
	IClientEntity *pClientEntity = entitylist->GetClientEntity(nCurIndex );
	if ( pClientEntity )
	{
		vEntityAbsPos = pClientEntity->GetAbsOrigin();
	}



	// first frame init position to avoid, erroneous deltas, zero is best option
	if(	pStructMem->m_flPrevTime == 0.0 )
	{
		VectorCopy( ( pScratchPad->m_vPlayerOrigin[ nCurIndex ]), pStructMem->m_vecPrevPosition );	
	}

	// calculate delta time for this operator updates
	float flCurTime = g_ClientGlobalVariables.curtime;
	float flDeltaTime = flCurTime - pStructMem->m_flPrevTime;
	pStructMem->m_flPrevTime = flCurTime;

	if( flDeltaTime > 0.0 )
	{

		// per/sec factor	
		float flPerSec = 1.0 / flDeltaTime;	

		pStructMem->m_flOutPosition[0] = vEntityAbsPos[0];
		pStructMem->m_flOutPosition[1] = vEntityAbsPos[1];
		pStructMem->m_flOutPosition[2] = vEntityAbsPos[2];

		// this is temporary for accessing single elements in array, will be a [] "feature" later when there's time
		pStructMem->m_flOutPosition_x[0] = 	pStructMem->m_flOutPosition[0];
		pStructMem->m_flOutPosition_y[0] = 	pStructMem->m_flOutPosition[1];
		pStructMem->m_flOutPosition_z[0] = 	pStructMem->m_flOutPosition[2];

		// get raw velocity vector
		Vector vPositionDelta;
		vPositionDelta[0] = pStructMem->m_flOutPosition[0] - pStructMem->m_vecPrevPosition[0];
		vPositionDelta[1] = pStructMem->m_flOutPosition[1] - pStructMem->m_vecPrevPosition[1];
		vPositionDelta[2] = pStructMem->m_flOutPosition[2] - pStructMem->m_vecPrevPosition[2];

		// scale vector to per/sec
		Vector vVelocity = vPositionDelta * flPerSec;
		pStructMem->m_flOutVelocityVector[0] = vVelocity[0];
		pStructMem->m_flOutVelocityVector[1] = vVelocity[1];
		pStructMem->m_flOutVelocityVector[2] = vVelocity[2];

		// this is temporary for accessing single elements in array, will be a "feature" later when there's time
		pStructMem->m_flOutVelocityVector_x[0] = pStructMem->m_flOutVelocityVector[0];
		pStructMem->m_flOutVelocityVector_y[0] = pStructMem->m_flOutVelocityVector[1];
		pStructMem->m_flOutVelocityVector_z[0] = pStructMem->m_flOutVelocityVector[2];
		
		pStructMem->m_flOutVelocity[0] = vVelocity.Length();

		pStructMem->m_flOutVelocityXY[0] = vVelocity.Length2D();

		VectorCopy( vEntityAbsPos, pStructMem->m_vecPrevPosition );	

	}

#endif
}



void CSosOperatorEntityInfo::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
//	CSosOperatorEntityInfo_t *pStructMem = (CSosOperatorEntityInfo_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
}

void CSosOperatorEntityInfo::OpHelp( ) const
{
}

void CSosOperatorEntityInfo::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorEntityInfo_t *pStructMem = (CSosOperatorEntityInfo_t *)pVoidMem;

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

