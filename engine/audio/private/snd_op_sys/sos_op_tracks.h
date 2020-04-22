//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_TRACKS_H
#define SOS_OP_TRACKS_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"

//-----------------------------------------------------------------------------
// get sync data
//-----------------------------------------------------------------------------
struct CSosOperatorGetTrackSyncPoint_t : CSosOperator_t
{
 	SOS_INPUT_FLOAT( m_flInputMinTimeToNextSync, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputMaxTimeToNextSync, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputFirstSyncPoint, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputLastSyncPoint, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputTimeToNextSync, SO_SINGLE )
// 
	char m_nSyncPointListName[64];

	char m_nMatchEntryName[64];
	bool m_bMatchEntry;

// 	bool m_bMatchEntry;
// 	char m_nMatchSoundName[64];
// 	bool m_bMatchSound;
// 	bool m_bMatchSubString;
// 	bool m_bMatchEntity;
// 	bool m_bMatchChannel;
// 	bool m_bStopOldest;
// 	bool m_bStopThis;
	bool m_bDataFromThis;
	bool m_bSubtractMinTimeToSync;
	bool m_bSubtractMinTimeToSyncFromLastSync;
	bool m_bSubtractFirstSyncFromLastSync;

};

class CSosOperatorGetTrackSyncPoint : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorGetTrackSyncPoint )

};

//-----------------------------------------------------------------------------
// queue to a track
//-----------------------------------------------------------------------------
struct CSosOperatorQueueToTrack_t : CSosOperator_t
{
	SOS_OUTPUT_FLOAT( m_flOutputTimeToStart, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputTimeToNextSync, SO_SINGLE )
	char m_nSyncPointListName[64];
};

class CSosOperatorQueueToTrack : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorQueueToTrack )

};

//-----------------------------------------------------------------------------
// play on a track
//-----------------------------------------------------------------------------
struct CSosOperatorPlayOnTrack_t : CSosOperator_t
{
// 	SOS_OUTPUT_FLOAT( m_flOutputTimeToStart, SO_SINGLE )
// 	SOS_OUTPUT_FLOAT( m_flOutputTimeToNextSync, SO_SINGLE )
// 	char m_nSyncPointListName[64];
	HSOUNDSCRIPTHASH m_nAutoQueueEndPointScriptHash;

	track_data_t m_trackData;
	float m_fAutoQueueStartPoint;
	bool m_bHasTriggeredAutoQue;
	bool m_bStopChannelOnTrack;

};

class CSosOperatorPlayOnTrack : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorPlayOnTrack )

};

//-----------------------------------------------------------------------------
// stop channel(s) a track
//-----------------------------------------------------------------------------
struct CSosOperatorStopTrack_t : CSosOperator_t
{
	char m_nTrackName[64];
};

class CSosOperatorStopTrack : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorStopTrack )

};
#endif // SOS_OP_TRACKS_H