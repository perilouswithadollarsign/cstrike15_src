//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// A class representing session state for the SFM
//
//=============================================================================

#include "movieobjects/exportfacialanimation.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmegamemodel.h"
#include "movieobjects/dmetrackgroup.h"
#include "movieobjects/dmetrack.h"
#include "movieobjects/dmesound.h"
#include "movieobjects/dmelog.h"
#include "movieobjects/dmechannel.h"


//-----------------------------------------------------------------------------
// Contains export information
//-----------------------------------------------------------------------------
struct ExportInfo_t
{
	CDmeFilmClip *m_pMovie;
	CDmeFilmClip *m_pShot;
	CDmeAnimationSet *m_pAnimationSet;
	DmeTime_t m_tExportStart;
	DmeTime_t m_tExportEnd;
};


//-----------------------------------------------------------------------------
// Used to transform channel data into export time
//-----------------------------------------------------------------------------
static void ComputeExportChannelScaleBias( double *pScale, DmeTime_t *pBias, ExportInfo_t &info, CDmeChannel *pChannel )
{
	DmeClipStack_t channelToGlobal;
	if ( pChannel->BuildClipStack( &channelToGlobal, info.m_pMovie, info.m_pShot ) )
	{
		DmeTime_t tOffset = channelToGlobal.FromChildMediaTime( DMETIME_ZERO, false );
		DmeTime_t tScale = channelToGlobal.FromChildMediaTime( DmeTime_t( 1.0f ), false );
		*pBias = tOffset - info.m_pShot->GetStartTime();
		*pScale = ( tScale - tOffset ).GetSeconds();
	}
}

static void GetExportTimeRange( DmeTime_t *pExportStart, DmeTime_t *pExportEnd, CDmeFilmClip *pShot )
{
	*pExportStart = DMETIME_ZERO;
	*pExportEnd = pShot->GetDuration();
}


//-----------------------------------------------------------------------------
// Adds a log layer to the list of logs for export
//-----------------------------------------------------------------------------
static void AddLogLayerForExport( ExportInfo_t &info, CDmElement *pRoot, const char *pControlName, CDmeChannel *pChannel )
{
	CDmeLog *pLog = pChannel->GetLog();
	if ( !pLog || pLog->GetNumLayers() == 0 ) 
		return;

	CDmrElementArray<> animations( pRoot, "animations" );

	DmeTime_t tBias;
	double flScale;
	ComputeExportChannelScaleBias( &flScale, &tBias, info, pChannel );

	// Only export the base layer
	CDmeLogLayer* pLogLayer = pLog->GetLayer( 0 )->Copy();
	pLogLayer->SetName( pControlName );
	pLogLayer->ScaleBiasKeyTimes( flScale, tBias );

	// Forcibly add keys @ the start + end time
	DmeTime_t tStartTime = ( info.m_tExportStart - tBias ) / flScale;
	DmeTime_t tEndTime = ( info.m_tExportEnd - tBias ) / flScale;
	pLogLayer->InsertKeyFromLayer( info.m_tExportStart, pLog->GetLayer(0), tStartTime );
	pLogLayer->InsertKeyFromLayer( info.m_tExportEnd, pLog->GetLayer(0), tEndTime );

	pLogLayer->RemoveKeysOutsideRange( info.m_tExportStart, info.m_tExportEnd );
	animations.AddToTail( pLogLayer );
}


//-----------------------------------------------------------------------------
// Exports animations
//-----------------------------------------------------------------------------
static void ExportAnimations( ExportInfo_t &info, CDmElement *pRoot )
{
	CDmrElementArray<> animations( pRoot, "animations", true );

	// Build a list of all controls
	const CDmaElementArray< CDmElement > &controls = info.m_pAnimationSet->GetControls();
	int nControlCount = controls.Count();
	for ( int i = 0; i < nControlCount; ++i )
	{
		CDmElement *pControl = controls[i];
		if ( !pControl || IsTransformControl( pControl ) )
			continue;

		bool bIsStereo = IsStereoControl( pControl );
		if ( bIsStereo )
		{
			char pControlName[512];
			Q_snprintf( pControlName, sizeof(pControlName), "left_%s", pControl->GetName() );
			CDmeChannel *pLeftChannel = pControl->GetValueElement<CDmeChannel>( "leftvaluechannel" );
			AddLogLayerForExport( info, pRoot, pControlName, pLeftChannel );

			Q_snprintf( pControlName, sizeof(pControlName), "right_%s", pControl->GetName() );
			CDmeChannel *pRightChannel = pControl->GetValueElement<CDmeChannel>( "leftvaluechannel" );
			AddLogLayerForExport( info, pRoot, pControlName, pRightChannel );
		}
		else
		{
			CDmeChannel *pChannel = pControl->GetValueElement<CDmeChannel>( "channel" );
			AddLogLayerForExport( info, pRoot, pControl->GetName(), pChannel );
		}
	}
}


//-----------------------------------------------------------------------------
// Helper to export sounds
//-----------------------------------------------------------------------------
static void ExportSounds( ExportInfo_t &info, CDmElement *pRoot, CDmeClip *pClip, DmeTime_t tOffset )
{
	CDmrElementArray<> sounds( pRoot, "sounds", true );

	DmeClipStack_t soundToGlobal;
	int gc = pClip->GetTrackGroupCount();
	for ( int i = 0; i < gc; ++i )
	{
		CDmeTrackGroup *pTrackGroup = pClip->GetTrackGroup( i );
		DMETRACKGROUP_FOREACH_CLIP_TYPE_START( CDmeSoundClip, pTrackGroup, pTrack, pSoundClip )

			const char *pGameSoundName = pSoundClip->m_Sound->m_GameSoundName;
			if ( !pGameSoundName || !pGameSoundName[0] )
				continue;

			if ( pSoundClip->IsMute() )
				continue;

			if ( !pSoundClip->BuildClipStack( &soundToGlobal, info.m_pMovie, pClip ) )
				continue;

			DmeTime_t tStart = soundToGlobal.FromChildMediaTime( DMETIME_ZERO, false );
			DmeTime_t tEnd = soundToGlobal.FromChildMediaTime( pSoundClip->GetDuration(), false );
			tStart -= tOffset;
			tEnd -= tOffset;
			if ( tStart >= info.m_tExportEnd || tEnd <= info.m_tExportStart )
				continue;

			const char *pName = pSoundClip->GetName();
			CDmElement *pSoundEvent = CreateElement<CDmElement>( pName, pRoot->GetFileId() );
			pSoundEvent->SetValue( "start", tStart );
			pSoundEvent->SetValue( "end", tEnd );
			pSoundEvent->SetValue( "gamesound", pGameSoundName );
			sounds.AddToTail( pSoundEvent );

		DMETRACKGROUP_FOREACH_CLIP_TYPE_END()
	}
}

static void ExportSounds_R( ExportInfo_t &info, CDmElement *pRoot, CDmeClip *pClip, DmeTime_t tOffset )
{
	ExportSounds( info, pRoot, pClip, tOffset );

	// Recurse
	DmeClipStack_t childToGlobal;
	int gc = pClip->GetTrackGroupCount();
	for ( int i = 0; i < gc; ++i )
	{
		CDmeTrackGroup *pTrackGroup = pClip->GetTrackGroup( i );
		DMETRACKGROUP_FOREACH_CLIP_START( pTrackGroup, pTrack, pChild )

			if ( !pChild->BuildClipStack( &childToGlobal, info.m_pMovie, pClip ) )
				continue;

		DmeTime_t tStart = childToGlobal.FromChildMediaTime( DMETIME_ZERO, false );
		DmeTime_t tEnd = childToGlobal.FromChildMediaTime( pChild->GetDuration(), false );
		tStart -= tOffset;
		tEnd -= tOffset;
		if ( tStart >= info.m_tExportEnd || tEnd <= info.m_tExportStart )
			continue;

		ExportSounds_R( info, pRoot, pChild, tOffset );

		DMETRACKGROUP_FOREACH_CLIP_END()
	}
}


//-----------------------------------------------------------------------------
// Exports sounds, default implementation
//-----------------------------------------------------------------------------
static void ExportSounds( ExportInfo_t &info, CDmElement *pRoot )
{
	DmeTime_t tOffset = info.m_pShot->GetStartTime();
	ExportSounds( info, pRoot, info.m_pMovie, tOffset );
	ExportSounds_R( info, pRoot, info.m_pShot, tOffset );
}


//-----------------------------------------------------------------------------
// Exports an .fac file
//-----------------------------------------------------------------------------
bool ExportFacialAnimation( const char *pFileName, CDmeFilmClip *pMovie, CDmeFilmClip *pShot, CDmeAnimationSet *pAnimationSet )
{
	if ( !pMovie || !pShot || !pAnimationSet )
		return false;

	const char *pFileFormat = "facial_animation";
	CDmElement *pRoot = CreateElement< CDmElement >( pAnimationSet->GetName(), DMFILEID_INVALID );

	ExportInfo_t info;
	info.m_pMovie = pMovie;
	info.m_pShot = pShot;
	info.m_pAnimationSet = pAnimationSet;
	GetExportTimeRange( &info.m_tExportStart, &info.m_tExportEnd, pShot );

	CDmeGameModel *pGameModel = pAnimationSet->GetValueElement<CDmeGameModel>( "gameModel" );
	if ( pGameModel )
	{
		pRoot->SetValue( "gamemodel", pGameModel->GetModelName() );
	}
	ExportAnimations( info, pRoot );
	ExportSounds( info, pRoot );

	pRoot->SetFileId( DMFILEID_INVALID, TD_DEEP );
	const char *pEncoding = "keyvalues2_flat";
	bool bOk = g_pDataModel->SaveToFile( pFileName, NULL, pEncoding, pFileFormat, pRoot ); 
	DestroyElement( pRoot, TD_DEEP );
	return bOk;
}


