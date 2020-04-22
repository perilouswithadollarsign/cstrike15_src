//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//===============================================================================

#include "audio_pch.h"
#include "tier2/interval.h"

#include "snd_dma.h"

#include "sos_op.h"
#include "sos_op_tracks.h"



// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



extern Color OpColor;
extern Color ConnectColor;
extern Color ResultColor;





//-----------------------------------------------------------------------------
// CSosOperatorGetTrackSyncPoint_t
// 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorGetTrackSyncPoint, "get_track_syncpoint" )
 	SOS_REGISTER_INPUT_FLOAT( CSosOperatorGetTrackSyncPoint, m_flInputMinTimeToNextSync, SO_SINGLE, "input_min_time_to_next_sync" )
	SOS_REGISTER_INPUT_FLOAT( CSosOperatorGetTrackSyncPoint, m_flInputMaxTimeToNextSync, SO_SINGLE, "input_max_time_to_next_sync" )
 	SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorGetTrackSyncPoint, m_flOutputFirstSyncPoint, SO_SINGLE, "output_first_syncpoint" )
	SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorGetTrackSyncPoint, m_flOutputLastSyncPoint, SO_SINGLE, "output_last_syncpoint" )
 	SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorGetTrackSyncPoint, m_flOutputTimeToNextSync, SO_SINGLE, "output_time_to_next_syncpoint" )
	SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorGetTrackSyncPoint,  "get_track_syncpoint" )

void CSosOperatorGetTrackSyncPoint::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorGetTrackSyncPoint_t *pStructMem = (CSosOperatorGetTrackSyncPoint_t *)pVoidMem;
 	V_strncpy( pStructMem->m_nMatchEntryName, "", sizeof(pStructMem->m_nMatchEntryName) );
	V_strncpy( pStructMem->m_nSyncPointListName, "", sizeof(pStructMem->m_nSyncPointListName ) );
	/// 	V_strncpy( pStructMem->m_nMatchSoundName, "", sizeof(pStructMem->m_nMatchSoundName) );
// 	pStructMem->m_bMatchEntry = false;
// 	pStructMem->m_bMatchSound = false;
// 	pStructMem->m_bMatchEntity = false;
// 	pStructMem->m_bMatchChannel = false;
// 	pStructMem->m_bMatchSubString = false;
// 	pStructMem->m_bStopOldest = true;
// 	pStructMem->m_bStopThis = false;
 	SOS_INIT_INPUT_VAR( m_flInputMinTimeToNextSync, SO_SINGLE, 0.0 )
	SOS_INIT_INPUT_VAR( m_flInputMaxTimeToNextSync, SO_SINGLE, 1000.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutputTimeToNextSync, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutputFirstSyncPoint, SO_SINGLE, 0.0 )

	pStructMem->m_bDataFromThis = false;
	pStructMem->m_bMatchEntry = false;
	pStructMem->m_bSubtractMinTimeToSync = true;
	pStructMem->m_bSubtractMinTimeToSyncFromLastSync = false;
	pStructMem->m_bSubtractFirstSyncFromLastSync = true;
}

static int __cdecl ChannelLongestElapsedTimeSortFunc( const int *nChannelIndexA, const int *nChannelIndexB )
{
	return ( S_GetElapsedTimeByGuid( channels[ *nChannelIndexA ].guid ) > S_GetElapsedTimeByGuid( channels[ *nChannelIndexB ].guid ) );
}

// static int __cdecl ChannelLeastVolumeSortFunc( const int *nChannelIndexA, const int *nChannelIndexB )
// {
// 	return ( S_GetElapsedTimeByGuid( channels[ *nChannelIndexA ]. ) < S_GetElapsedTimeByGuid( channels[ *nChannelIndexB ].guid ) );
// }

void CSosOperatorGetTrackSyncPoint::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{

	CSosOperatorGetTrackSyncPoint_t *pStructMem = (CSosOperatorGetTrackSyncPoint_t *)pVoidMem;

	track_data_t trackData;
	pStack->GetTrackData( trackData );
	
	//	HSOUNDSCRIPTHASH nSoundScriptHashHandle = SOUNDEMITTER_INVALID_HASH;
	float fElapsedTime = 0.0;
	float fDuration = 0.0;
	KeyValues *pSyncPointList = NULL;


	if( ! pStructMem->m_bDataFromThis )
	{
		CChannelList list;
		g_ActiveChannels.GetActiveChannels( list );

		for ( int i = 0; i < list.Count(); i++ )
		{

			int ch_idx = list.GetChannelIndex(i);
			// skip uninitiated entries (like this one!)
			if ( ! channels[ch_idx].sfx )
			{
				continue;
			}

			if( pChannel && &channels[ch_idx] == pChannel )
			{
				continue;
			}


			// this should probably explicitly check for soundentry_version
			if( channels[ch_idx].m_nSoundScriptHash == SOUNDEMITTER_INVALID_HASH  )
			{
				continue;
			}

			KeyValues *pOperatorKV = g_pSoundEmitterSystem->GetOperatorKVByHandle( channels[ch_idx].m_nSoundScriptHash );
			if ( !pOperatorKV )
			{
				// Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s cannot find operator KV data\n", pStack->GetOperatorName( nOpIndex ));
				continue;
			}

			track_data_t chanTrackData;
			S_GetTrackData( pOperatorKV, chanTrackData );

			if( chanTrackData.m_nTrackNumber != trackData.m_nSyncTrackNumber )
			{
				continue;
			}

			// getting elapsed time from entry time due to mp3 weirdness
			fElapsedTime = S_GetElapsedTimeByGuid( channels[ ch_idx ].guid ) * 0.01;			 
// 			if ( channels[ch_idx].m_pStackList )
// 			{
// 				CSosOperatorStack *pTheStack = NULL;
// 				pTheStack = channels[ch_idx].m_pStackList->GetStack( CSosOperatorStack::SOS_UPDATE );
// 
// 				if ( pTheStack )
// 				{
// 					fElapsedTime = pTheStack->GetElapsedTime();
// 				}
// 			}
			fDuration = chanTrackData.m_flEndPoint - chanTrackData.m_flStartPoint;
//			fDuration = S_SoundDuration( &channels[ ch_idx ] );
			fElapsedTime = fmod( fElapsedTime, fDuration );

	
			pSyncPointList = pStack->GetSyncPointsKV( pOperatorKV, pStructMem->m_nSyncPointListName );
			break;
		}
	}
	else
	{
		// nSoundScriptHashHandle = pStack->GetScriptHash();
		pSyncPointList = pStack->GetSyncPointsKV( pStructMem->m_nSyncPointListName );
	}


	CUtlVector< float > vSyncPoints;
	if( pSyncPointList )
	{
		for ( KeyValues *pValue = pSyncPointList->GetFirstValue(); pValue; pValue = pValue->GetNextValue() )
		{
			vSyncPoints.AddToTail( pValue->GetFloat() );
		}
	}
	else
	{
		//Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s cannot find track syncpoint KV data\n", pStack->GetOperatorName( nOpIndex ));
		return;
	}

	float fMinTimeToSyncPoint = pStructMem->m_flInputMaxTimeToNextSync[0];
	for( int i = 0; i < vSyncPoints.Count(); i++ )
	{
		float fDiff = vSyncPoints[i] - fElapsedTime;
		if( fDiff > 0 && fDiff < fMinTimeToSyncPoint && fDiff > pStructMem->m_flInputMinTimeToNextSync[0] )
		{
			fMinTimeToSyncPoint = fDiff;
		}

	}
	fMinTimeToSyncPoint = fMinTimeToSyncPoint - ( pStructMem->m_bSubtractMinTimeToSync ? pStructMem->m_flInputMinTimeToNextSync[0] : 0.0 );
	pStructMem->m_flOutputTimeToNextSync[0] = fMinTimeToSyncPoint;

	float fLastSyncPoint = 0.0;
	int nIndexToLastSyncPoint = vSyncPoints.Count() > 0 ? vSyncPoints.Count() - 1 : -1;
	if( nIndexToLastSyncPoint >= 0 )
	{
		fLastSyncPoint = vSyncPoints[ nIndexToLastSyncPoint ] - ( pStructMem->m_bSubtractMinTimeToSyncFromLastSync ? pStructMem->m_flInputMinTimeToNextSync[0] : 0.0 );
		fLastSyncPoint = fLastSyncPoint - ( pStructMem->m_bSubtractFirstSyncFromLastSync ? vSyncPoints[0] : 0.0 );
	}

	pStructMem->m_flOutputLastSyncPoint[0] = fLastSyncPoint;


	if( vSyncPoints.Count() > 0 )
	{
		pStructMem->m_flOutputFirstSyncPoint[0] = vSyncPoints[0];
	}
	else
	{
		pStructMem->m_flOutputFirstSyncPoint[0] = 0.0;
	}

	// 	pStructMem->m_flOutputVoicesMatching[0] = ( float ) vMatchingIndices.Count();
// 	pStructMem->m_flOutputIndexOfThis[0] = ( float ) nThisIndex;
// 


}

void CSosOperatorGetTrackSyncPoint::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorGetTrackSyncPoint_t *pStructMem = (CSosOperatorGetTrackSyncPoint_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	// 	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sNumber Allowed: %i\n", nLevel, "    ", pStructMem->m_nNumAllowed );

	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*smatch_entry: %s\n", nLevel, "    ", pStructMem->m_bMatchEntry ? pStructMem->m_nMatchEntryName : "\"\"" );	
 	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*ssyncpoint_list: %s\n", nLevel, "    ", pStructMem->m_nSyncPointListName );
 	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*sthis_entry_syncpoints: %s\n", nLevel, "    ", pStructMem->m_bDataFromThis ? "true" : "false" );
 	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*ssubtract_min_time_from_next: %s\n", nLevel, "    ", pStructMem->m_bSubtractMinTimeToSync ? "true" : "false" );

	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*ssubtract_min_time_from_last: %s\n", nLevel, "    ", pStructMem->m_bSubtractMinTimeToSyncFromLastSync ? "true" : "false" );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*ssubtract_first_from_last: %s\n", nLevel, "    ", pStructMem->m_bSubtractFirstSyncFromLastSync ? "true" : "false" );


}
void CSosOperatorGetTrackSyncPoint::OpHelp( ) const
{

}

void CSosOperatorGetTrackSyncPoint::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorGetTrackSyncPoint_t *pStructMem = (CSosOperatorGetTrackSyncPoint_t *)pVoidMem;

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
				else if ( !V_strcasecmp( pParamString, "match_entry" ) )
				{
					pStructMem->m_bMatchEntry = true;
					V_strncpy( pStructMem->m_nMatchEntryName, pValueString, sizeof(pStructMem->m_nMatchEntryName) );
				}
				else if ( !V_strcasecmp( pParamString, "syncpoint_list" ) )
				{
					V_strncpy( pStructMem->m_nSyncPointListName, pValueString, sizeof( pStructMem->m_nSyncPointListName ) );
				}
				else if ( !V_strcasecmp( pParamString, "this_entry_syncpoints" ) )
				{
					if ( !V_strcasecmp( pValueString, "true" ) )
 					{
 						pStructMem->m_bDataFromThis = true;
 					}
 					else
 					{
 						pStructMem->m_bDataFromThis = false;
 					}
				}
				else if ( !V_strcasecmp( pParamString, "subtract_min_time_from_next" ) )
				{
					if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bSubtractMinTimeToSync = true;
					}
					else
					{
						pStructMem->m_bSubtractMinTimeToSync = false;
					}
				}
				else if ( !V_strcasecmp( pParamString, "subtract_min_time_from_last" ) )
				{
					if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bSubtractMinTimeToSyncFromLastSync = true;
					}
					else
					{
						pStructMem->m_bSubtractMinTimeToSyncFromLastSync = false;
					}
				}
				else if ( !V_strcasecmp( pParamString, "subtract_first_from_last" ) )
				{
					if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bSubtractFirstSyncFromLastSync = true;
					}
					else
					{
						pStructMem->m_bSubtractFirstSyncFromLastSync = false;
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




//-----------------------------------------------------------------------------
// CSosOperatorQueueToTrack_t
// 
//-----------------------------------------------------------------------------

ConVar snd_sos_show_queuetotrack("snd_sos_show_queuetotrack", "0", FCVAR_CHEAT );

SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorQueueToTrack, "track_queue" )
	SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorQueueToTrack, m_flOutputTimeToNextSync, SO_SINGLE, "output_time_to_next_syncpoint" )
	SOS_REGISTER_OUTPUT_FLOAT( CSosOperatorQueueToTrack, m_flOutputTimeToStart, SO_SINGLE, "output_time_to_start" )
	SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorQueueToTrack,  "track_queue" )

	void CSosOperatorQueueToTrack::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorQueueToTrack_t *pStructMem = (CSosOperatorQueueToTrack_t *)pVoidMem;
	V_strncpy( pStructMem->m_nSyncPointListName, "", sizeof(pStructMem->m_nSyncPointListName ) );

	SOS_INIT_OUTPUT_VAR( m_flOutputTimeToNextSync, SO_SINGLE, 0.0 )
	SOS_INIT_OUTPUT_VAR( m_flOutputTimeToStart, SO_SINGLE, 0.0 )

}

void CSosOperatorQueueToTrack::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{

	CSosOperatorQueueToTrack_t *pStructMem = (CSosOperatorQueueToTrack_t *)pVoidMem;

	track_data_t trackData;
	pStack->GetTrackData( trackData );

	float fElapsedTime = 0.0;
	float fDuration = 0.0;
	float flMinTimeToSyncPoint = 0.0;
	KeyValues *pSyncPointList = NULL;


	channel_t *pCurrentChannelOnThisTrack = NULL;
	int nThisTrackIndex = g_pSoundOperatorSystem->m_vTrackDict.Find( trackData.m_pTrackName );
	if( g_pSoundOperatorSystem->m_vTrackDict.IsValidIndex( nThisTrackIndex ) )
	{
		pCurrentChannelOnThisTrack = g_pSoundOperatorSystem->m_vTrackDict[nThisTrackIndex];

		if( pCurrentChannelOnThisTrack && pCurrentChannelOnThisTrack->m_nSoundScriptHash != SOUNDEMITTER_INVALID_HASH )
		{

			KeyValues *pOperatorKV = g_pSoundEmitterSystem->GetOperatorKVByHandle( pCurrentChannelOnThisTrack->m_nSoundScriptHash );
			if ( pOperatorKV )
			{
				track_data_t thatTrackData;
				S_GetTrackData( pOperatorKV, thatTrackData );

				if( !S_TrackHasPriority( trackData, thatTrackData ) )
				{
					if( snd_sos_show_queuetotrack.GetInt() )
					{
						Log_Warning( LOG_SND_OPERATORS, "QUEUETOTRACK: %s priority blocked by %s\n", g_pSoundEmitterSystem->GetSoundNameForHash( pStack->GetScriptHash() ), g_pSoundEmitterSystem->GetSoundNameForHash( pCurrentChannelOnThisTrack->m_nSoundScriptHash ));
					}
					pScratchPad->m_flDelayToQueue = 0.0;
					pScratchPad->m_bBlockStart = true;
					return;
				}
			}
		}
	}

	// if there's nothing playing on the sync track do we just play? 
	// currently YES
	channel_t *pCurrentChannelOnSyncTrack = NULL;
	int nTrackIndex = g_pSoundOperatorSystem->m_vTrackDict.Find( trackData.m_pSyncTrackName );
	if( g_pSoundOperatorSystem->m_vTrackDict.IsValidIndex( nTrackIndex ) )
	{
		pCurrentChannelOnSyncTrack = g_pSoundOperatorSystem->m_vTrackDict[nTrackIndex];
	
		if( pCurrentChannelOnSyncTrack && pCurrentChannelOnSyncTrack->m_nSoundScriptHash != SOUNDEMITTER_INVALID_HASH )
		{

			KeyValues *pOperatorKV = g_pSoundEmitterSystem->GetOperatorKVByHandle( pCurrentChannelOnSyncTrack->m_nSoundScriptHash );
			if ( pOperatorKV )
			{

				track_data_t syncTrackData;
				S_GetTrackData( pOperatorKV, syncTrackData );

				fElapsedTime = S_GetElapsedTimeByGuid( pCurrentChannelOnSyncTrack->guid ) * 0.01;
//				fDuration = S_SoundDuration( pCurrentChannelOnSyncTrack );
				fDuration = syncTrackData.m_flEndPoint - syncTrackData.m_flStartPoint;
				fElapsedTime = fmod( fElapsedTime, fDuration );


				pSyncPointList = pStack->GetSyncPointsKV( pOperatorKV, pStructMem->m_nSyncPointListName );

				// collect up sync points
				CUtlVector< float > vSyncPoints;
				if( pSyncPointList )
				{
					for ( KeyValues *pValue = pSyncPointList->GetFirstValue(); pValue; pValue = pValue->GetNextValue() )
					{
						vSyncPoints.AddToTail( pValue->GetFloat() );
					}
				}
				else
				{
					//Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s cannot find track syncpoint KV data\n", pStack->GetOperatorName( nOpIndex ));
					return;
				}

				flMinTimeToSyncPoint = fDuration;
				bool bFoundSyncPoint = false;
				for( int i = 0; i < vSyncPoints.Count(); i++ )
				{
					float fDiff = vSyncPoints[i] - fElapsedTime;
					if( fDiff > 0 && fDiff < flMinTimeToSyncPoint && fDiff > trackData.m_flStartPoint )
					{
						bFoundSyncPoint = true;
						flMinTimeToSyncPoint = fDiff;
					}

				}
				if( bFoundSyncPoint )
				{
					pStructMem->m_flOutputTimeToNextSync[0] = flMinTimeToSyncPoint;

					// subtract start point
					flMinTimeToSyncPoint = flMinTimeToSyncPoint - trackData.m_flStartPoint;
					pStructMem->m_flOutputTimeToStart[0] = flMinTimeToSyncPoint;
				}
				else
				{
					pStructMem->m_flOutputTimeToNextSync[0] = 0.0;
					pStructMem->m_flOutputTimeToStart[0] = 0.0;
					flMinTimeToSyncPoint = 0.0;
				}

				if( snd_sos_show_queuetotrack.GetInt() )
				{
					Log_Warning( LOG_SND_OPERATORS, "QUEUETOTRACK: %s queued with a delay of %f\n", g_pSoundEmitterSystem->GetSoundNameForHash( pStack->GetScriptHash() ), flMinTimeToSyncPoint);
				}
			}
		}
	}

	pScratchPad->m_flDelayToQueue = flMinTimeToSyncPoint;

}

void CSosOperatorQueueToTrack::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorQueueToTrack_t *pStructMem = (CSosOperatorQueueToTrack_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*ssyncpoint_list: %s\n", nLevel, "    ", pStructMem->m_nSyncPointListName );
}
void CSosOperatorQueueToTrack::OpHelp( ) const
{

}

void CSosOperatorQueueToTrack::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorQueueToTrack_t *pStructMem = (CSosOperatorQueueToTrack_t *)pVoidMem;

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
				else if ( !V_strcasecmp( pParamString, "syncpoint_list" ) )
				{
					V_strncpy( pStructMem->m_nSyncPointListName, pValueString, sizeof( pStructMem->m_nSyncPointListName ) );
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
// CSosOperatorPlayOnTrack_t
// 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorPlayOnTrack, "track_update" )
	SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorPlayOnTrack,  "track_update" )

void CSosOperatorPlayOnTrack::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorPlayOnTrack_t *pStructMem = (CSosOperatorPlayOnTrack_t *)pVoidMem;
	pStructMem->m_nAutoQueueEndPointScriptHash = SOUNDEMITTER_INVALID_HASH;
	pStructMem->m_trackData.SetDefaults();
	pStructMem->m_bHasTriggeredAutoQue = false;
	pStructMem->m_bStopChannelOnTrack = true;

}

void CSosOperatorPlayOnTrack::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{

	if( !pChannel )
	{
		Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s requires valid channel pointer, being called without one\n", pStack->GetOperatorName( nOpIndex ));
		return;
	}

	CSosOperatorPlayOnTrack_t *pStructMem = (CSosOperatorPlayOnTrack_t *)pVoidMem;

	// first time
	if( !pStructMem->m_bHasExecuted )
	{
		if( pStructMem->m_bStopChannelOnTrack )
		{
			g_pSoundOperatorSystem->StopChannelOnTrack( pStructMem->m_trackData.m_pTrackName, false, pStructMem->m_trackData.m_flStartPoint );
		}
		g_pSoundOperatorSystem->SetChannelOnTrack( pStructMem->m_trackData.m_pTrackName, pChannel );
 	}


	// in the case that the incoming cue has a starttime longer than the time left on the track, there will be silence.
	// punt for now
	//		( pStack->GetStopType() == SOS_STOP_NONE || ( pStack->GetStopType() == SOS_STOP_QUEUE ) ) ) //&& ( pStack->GetStopTime() > pStructMem->m_trackData.m_flStartPoint )

	if ( pStructMem->m_nAutoQueueEndPointScriptHash != SOUNDEMITTER_INVALID_HASH &&
			pStack->GetStopType() == SOS_STOP_NONE )
	{
		float fThisChannelsElapsedTime = S_GetElapsedTime( pChannel ) * 0.01;
		float fTimeToTrigger = pStructMem->m_trackData.m_flEndPoint - pStructMem->m_fAutoQueueStartPoint;
		if( ( fThisChannelsElapsedTime >= fTimeToTrigger ) &&
				!pStructMem->m_bHasTriggeredAutoQue )
		{
			// copied from start_entry.... UGH, PROPAGATING UGLINESS!!

			StartSoundParams_t startParams;
			CSoundParameters pScriptParams;
			gender_t gender = GENDER_NONE;
			if ( !g_pSoundEmitterSystem->GetParametersForSoundEx( "SoundSciptHandle ERROR", pStructMem->m_nAutoQueueEndPointScriptHash, pScriptParams, gender, true ) )
			{
				//DevWarning("Error: Unable to get parameters for soundentry %s", startParams.m_pSoundEntryName );
				return;
			}

			// don't actually need the soundfile yet

			// 		if ( !pScriptParams.soundname[0] )
			// 			return;

			// copy emitter params
			startParams.staticsound = ( pScriptParams.channel == CHAN_STATIC ) ? true : false;
			startParams.entchannel = pScriptParams.channel;

			// inherits location and entity
			VectorCopy( pScratchPad->m_vEmitterInfoOrigin, startParams.origin );
			startParams.soundsource = pScratchPad->m_nSoundSource;

			startParams.fvol = pScriptParams.volume;
			startParams.soundlevel = pScriptParams.soundlevel;
			//	startParams.flags = sound.nFlags;
			startParams.pitch = pScriptParams.pitch;
			startParams.fromserver = false;
			startParams.delay = pScriptParams.delay_msec;
			//	startParams.speakerentity = sound.nSpeakerEntity;
			//startParams.m_bIsScriptHandle = ( pScriptParams.m_nSoundEntryVersion > 1 );
			startParams.m_bIsScriptHandle = true;

			startParams.m_nSoundScriptHash = pStructMem->m_nAutoQueueEndPointScriptHash;
			g_pSoundOperatorSystem->QueueStartEntry( startParams, 0.0, false );

			pStructMem->m_bHasTriggeredAutoQue = true;
			g_pSoundOperatorSystem->RemoveChannelFromTrack( pStructMem->m_trackData.m_pTrackName, pStack->GetChannelGuid() );
		}
	}
}

void CSosOperatorPlayOnTrack::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
//	CSosOperatorPlayOnTrack_t *pStructMem = (CSosOperatorPlayOnTrack_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
}
void CSosOperatorPlayOnTrack::OpHelp( ) const
{

}

void CSosOperatorPlayOnTrack::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorPlayOnTrack_t *pStructMem = (CSosOperatorPlayOnTrack_t *)pVoidMem;


	pStack->GetTrackData( pStructMem->m_trackData );

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
				else if ( !V_strcasecmp( pParamString, "autoqueue_entry_at_end_point" ) )
				{
					if ( !g_pSoundEmitterSystem )
					{
						DevWarning("Error: SoundEmitterSystem not initialized in engine!");
						return;
					}
					pStructMem->m_nAutoQueueEndPointScriptHash = g_pSoundEmitterSystem->HashSoundName( pValueString );

					if( !g_pSoundEmitterSystem->GetSoundNameForHash( pStructMem->m_nAutoQueueEndPointScriptHash ))
					{
						//	DevMsg( "Error: Invalid SoundEntry index %i from entry %s  operator %s", pStructMem->m_nScriptHandle, pValueString, pStack->GetOperatorName( nOpIndex ) );
						DevMsg( "Error: Invalid SoundEntry hash %i from entry %s", pStructMem->m_nAutoQueueEndPointScriptHash, pValueString );
						pStructMem->m_nAutoQueueEndPointScriptHash = SOUNDEMITTER_INVALID_HASH;
					}
					else
					{
						KeyValues *pOperatorKV = g_pSoundEmitterSystem->GetOperatorKVByHandle( pStructMem->m_nAutoQueueEndPointScriptHash );
						if ( !pOperatorKV )
						{
							// Log_Warning( LOG_SND_OPERATORS, "Error: Sound operator %s cannot find operator KV data\n", pStack->GetOperatorName( nOpIndex ));
							pStructMem->m_fAutoQueueStartPoint = 0.0;
						}
						else
						{
							track_data_t trackData;
							S_GetTrackData( pOperatorKV, trackData );
							pStructMem->m_fAutoQueueStartPoint = trackData.m_flStartPoint;
						}
					}
				}
				else if ( !V_strcasecmp( pParamString, "stop_channel_on_track" ) )
				{
					if ( !V_strcasecmp( pValueString, "true" ) )
					{
						pStructMem->m_bStopChannelOnTrack = true;
					}
					else if ( !V_strcasecmp( pValueString, "false" ) )
					{
						pStructMem->m_bStopChannelOnTrack = false;
					}
				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s Unknown sound operator attribute %s\n", pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}

//-----------------------------------------------------------------------------
// CSosOperatorStopTrack_t
// 
//-----------------------------------------------------------------------------
SOS_BEGIN_OPERATOR_CONSTRUCTOR( CSosOperatorStopTrack, "track_stop" )
SOS_END_OPERATOR_CONSTRUCTOR( CSosOperatorStopTrack,  "track_stop" )

void CSosOperatorStopTrack::SetDefaults( void *pVoidMem ) const
{
	CSosOperatorStopTrack_t *pStructMem = (CSosOperatorStopTrack_t *)pVoidMem;
	pStructMem->m_nTrackName[0] = 0;
}

void CSosOperatorStopTrack::Execute( void *pVoidMem, channel_t *pChannel, CScratchPad *pScratchPad, CSosOperatorStack *pStack, int nOpIndex ) const
{
	CSosOperatorStopTrack_t *pStructMem = (CSosOperatorStopTrack_t *)pVoidMem;
	g_pSoundOperatorSystem->StopChannelOnTrack( pStructMem->m_nTrackName );

	// add stopall and delay as "features"
	//	g_pSoundOperatorSystem->StopChannelOnTrack( pStructMem->m_nTrackName, bStopAllQueued, flDelay );
}

void CSosOperatorStopTrack::Print( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex, int nLevel ) const
{
	CSosOperatorStopTrack_t *pStructMem = (CSosOperatorStopTrack_t *)pVoidMem;
	PrintBaseParams( pVoidMem, pStack, nOpIndex, nLevel );
	Log_Msg( LOG_SND_OPERATORS, OpColor, "%*strack: %s\n", nLevel, "    ", pStructMem->m_nTrackName );
}
void CSosOperatorStopTrack::OpHelp( ) const
{

}

void CSosOperatorStopTrack::ParseKV( CSosOperatorStack *pStack, void *pVoidMem, KeyValues *pOpKeys ) const
{
	CSosOperatorStopTrack_t *pStructMem = (CSosOperatorStopTrack_t *)pVoidMem;

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
				else if ( !V_strcasecmp( pParamString, "track_name" ) )
				{
					V_strncpy( pStructMem->m_nTrackName, pValueString, sizeof(pStructMem->m_nTrackName ) );
				}
				else
				{
					Log_Warning( LOG_SND_OPERATORS, "Error: Operator %s Unknown sound operator attribute %s\n", pStack->m_pCurrentOperatorName, pParamString );
				}
			}
		}
		pParams = pParams->GetNextKey();
	}
}