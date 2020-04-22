//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmeclip.h"

#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/dmehandle.h"

#include "movieobjects/dmetimeframe.h"
#include "movieobjects/dmebookmark.h"
#include "movieobjects/dmesound.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmecamera.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmeinput.h"
#include "movieobjects/dmeoperator.h"
#include "movieobjects/dmematerial.h"
#include "movieobjects/dmetrack.h"
#include "movieobjects/dmetrackgroup.h"
#include "movieobjects/dmematerialoverlayfxclip.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects_interfaces.h"

#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imesh.h"
#include "tier3/tier3.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// String to clip type + back
//-----------------------------------------------------------------------------
static const char *s_pClipTypeNames[DMECLIP_TYPE_COUNT] = 
{
	"Channel",
	"Audio",
	"Effects",
	"Film",
};

DmeClipType_t ClipTypeFromString( const char *pName )
{
	for ( DmeClipType_t i = DMECLIP_FIRST; i <= DMECLIP_LAST; ++i )
	{
		if ( !Q_stricmp( pName, s_pClipTypeNames[i] ) )
			return i;
	}
	return DMECLIP_UNKNOWN;
}

const char *ClipTypeToString( DmeClipType_t type )
{
	if ( type >= DMECLIP_FIRST && type <= DMECLIP_LAST )
		return s_pClipTypeNames[ type ];
	return "Unknown";
}


//-----------------------------------------------------------------------------
// CDmeClip - common base class for filmclips, soundclips, and channelclips
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeClip, CDmeClip );

void CDmeClip::OnConstruction()
{
	m_TimeFrame.InitAndCreate( this, "timeFrame" );
	m_ClipColor.InitAndSet( this, "color", Color( 0, 0, 0, 0 ) );
	m_ClipText.Init( this, "text" );
	m_bMute.Init( this, "mute" );
	m_TrackGroups.Init( this, "trackGroups", FATTRIB_MUSTCOPY );
	m_flDisplayScale.InitAndSet( this, "displayScale", 1.0f );
}

void CDmeClip::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Clip color
//-----------------------------------------------------------------------------
void CDmeClip::SetClipColor( const Color& clr )
{
	m_ClipColor.Set( clr );
}

Color CDmeClip::GetClipColor() const
{
	return m_ClipColor.Get();
}


//-----------------------------------------------------------------------------
// Clip text
//-----------------------------------------------------------------------------
void CDmeClip::SetClipText( const char *pText )
{
	m_ClipText = pText;
}

const char*	CDmeClip::GetClipText() const
{
	return m_ClipText;
}


//-----------------------------------------------------------------------------
// Returns the time frame
//-----------------------------------------------------------------------------
CDmeTimeFrame *CDmeClip::GetTimeFrame() const
{
	return m_TimeFrame.GetElement();
}

DmeTime_t CDmeClip::ToChildMediaTime( DmeTime_t t, bool bClamp ) const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->ToChildMediaTime( t, bClamp ) : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::FromChildMediaTime( DmeTime_t t, bool bClamp ) const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->FromChildMediaTime( t, bClamp ) : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::ToChildMediaDuration( DmeTime_t dt ) const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->ToChildMediaDuration( dt ) : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::FromChildMediaDuration( DmeTime_t dt ) const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->FromChildMediaDuration( dt ) : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::GetTimeOffset() const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->GetTimeOffset() : DmeTime_t( 0 );
}

float CDmeClip::GetTimeScale() const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->GetTimeScale() : 0.0f;
}

DmeTime_t CDmeClip::GetStartTime() const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->GetStartTime() : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::GetEndTime() const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->GetStartTime() + tf->GetDuration() : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::GetDuration() const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->GetDuration() : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::GetStartInChildMediaTime() const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->GetStartInChildMediaTime() : DmeTime_t( 0 );
}

DmeTime_t CDmeClip::GetEndInChildMediaTime() const
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	return tf ? tf->GetEndInChildMediaTime() : DmeTime_t( 0 );
}

void CDmeClip::SetTimeOffset( DmeTime_t t )
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	if ( tf )
	{
		tf->SetTimeOffset( t );
	}
}

void CDmeClip::SetTimeScale( float s )
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	if ( tf )
	{
		tf->SetTimeScale( s );
	}
}

void CDmeClip::SetStartTime( DmeTime_t t )
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	if ( tf )
	{
		tf->SetStartTime( t );
	}
}

void CDmeClip::SetDuration( DmeTime_t t )
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	if ( tf )
	{
		tf->SetDuration( t );
	}
}


void CDmeClip::BakeTimeScale( float scale /*= 1.0f*/ )
{
	CDmeTimeFrame *tf = m_TimeFrame.GetElement();
	Assert( tf );
	if ( !tf )
		return;

	float flNewScale = tf->GetTimeScale();
	tf->SetTimeScale( 1.0f );

	if ( scale != 1.0f )
	{
		tf->SetStartTime ( tf->GetStartTime () / scale );
		tf->SetDuration  ( tf->GetDuration  () / scale );
		tf->SetTimeOffset( tf->GetTimeOffset() / scale );
		flNewScale *= scale;
	}

	int nTrackGroups = m_TrackGroups.Count();
	for ( int gi = 0; gi < nTrackGroups; ++gi )
	{
		CDmeTrackGroup *pTrackGroup = m_TrackGroups[ gi ];
		if ( !pTrackGroup )
			continue;

		int nTracks = pTrackGroup->GetTrackCount();
		for ( int ti = 0; ti < nTracks; ++ti )
		{
			CDmeTrack *pTrack = pTrackGroup->GetTrack( ti );
			if ( !pTrack )
				continue;

			int nClips = pTrack->GetClipCount();
			for ( int ci = 0; ci < nClips; ++ci )
			{
				CDmeClip *pClip = pTrack->GetClip( ci );
				if ( !pClip )
					continue;

				pClip->BakeTimeScale( flNewScale );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Track iteration
//-----------------------------------------------------------------------------
const CUtlVector< DmElementHandle_t > &CDmeClip::GetTrackGroups( ) const
{
	return m_TrackGroups.Get();
}

int CDmeClip::GetTrackGroupCount( ) const
{
	// Make sure no invalid clip types have snuck in
	return m_TrackGroups.Count();
}

CDmeTrackGroup *CDmeClip::GetTrackGroup( int nIndex ) const
{
	if ( ( nIndex >= 0 ) && ( nIndex < m_TrackGroups.Count() ) )
		return m_TrackGroups[ nIndex ];
	return NULL;
}
	
//-----------------------------------------------------------------------------
// Is a track group valid to add?
//-----------------------------------------------------------------------------
bool CDmeClip::IsTrackGroupValid( CDmeTrackGroup *pTrackGroup )
{
	// FIXME: If track groups have allowed types, we can check for validity
	for ( DmeClipType_t i = DMECLIP_FIRST; i <= DMECLIP_LAST; ++i )
	{
		if ( !IsSubClipTypeAllowed( i ) && pTrackGroup->IsSubClipTypeAllowed( i ) )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Track group addition/removal
//-----------------------------------------------------------------------------
void CDmeClip::AddTrackGroup( CDmeTrackGroup *pTrackGroup )
{
	if ( !IsTrackGroupValid( pTrackGroup ) )
		return;

	// FIXME:  Should check if track with same name already exists???
	if ( GetTrackGroupIndex( pTrackGroup ) < 0 )
	{
		m_TrackGroups.AddToTail( pTrackGroup );
	}
}

void CDmeClip::AddTrackGroupBefore( CDmeTrackGroup *pTrackGroup, CDmeTrackGroup *pBefore )
{
	if ( !IsTrackGroupValid( pTrackGroup ) )
		return;

	// FIXME:  Should check if track with same name already exists???
	if ( GetTrackGroupIndex( pTrackGroup ) < 0 )
	{
		int nBeforeIndex = pBefore ? GetTrackGroupIndex( pBefore ) : GetTrackGroupCount();
		if ( nBeforeIndex >= 0 )
		{
			m_TrackGroups.InsertBefore( nBeforeIndex, pTrackGroup );
		}
	}
}


CDmeTrackGroup *CDmeClip::AddTrackGroup( const char *pTrackGroupName )
{
	CDmeTrackGroup *pTrackGroup = CreateElement< CDmeTrackGroup >( pTrackGroupName, GetFileId() );
	pTrackGroup->SetMinimized( false );
	m_TrackGroups.AddToTail( pTrackGroup );
	return pTrackGroup;
}

void CDmeClip::RemoveTrackGroup( int nIndex )
{
	Assert( nIndex >= 0 && nIndex < m_TrackGroups.Count() );

	m_TrackGroups.Remove( nIndex );
}

void CDmeClip::RemoveTrackGroup( CDmeTrackGroup *pTrackGroup )
{
	int i = GetTrackGroupIndex( pTrackGroup );
	if ( i < 0 )
		return;

	m_TrackGroups.Remove( i );
}

void CDmeClip::RemoveTrackGroup( const char *pTrackGroupName )
{	
	if ( !pTrackGroupName )
	{
		pTrackGroupName = DMETRACKGROUP_DEFAULT_NAME;
	}

	int c = m_TrackGroups.Count();
	for ( int i = c; --i >= 0; )
	{
		if ( !Q_strcmp( m_TrackGroups[i]->GetName(), pTrackGroupName ) )
		{
			m_TrackGroups.Remove( i );
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// Swap track groups
//-----------------------------------------------------------------------------
void CDmeClip::SwapOrder( CDmeTrackGroup *pTrackGroup1, CDmeTrackGroup *pTrackGroup2 )
{
	if ( pTrackGroup1 == pTrackGroup2 )
		return;
	if ( pTrackGroup1->IsFilmTrackGroup() || pTrackGroup2->IsFilmTrackGroup() )
		return;

	int nIndex1 = -1, nIndex2 = -1;
	int c = m_TrackGroups.Count();
	for ( int i = c; --i >= 0; )
	{
		if ( m_TrackGroups[i] == pTrackGroup1 )
		{
			nIndex1 = i;
		}
		if ( m_TrackGroups[i] == pTrackGroup2 )
		{
			nIndex2 = i;
		}
	}
	if ( ( nIndex1 < 0 ) || ( nIndex2 < 0 ) )
		return;

	m_TrackGroups.Swap( nIndex1, nIndex2 );
}

//-----------------------------------------------------------------------------
// Track group finding
//-----------------------------------------------------------------------------
CDmeTrackGroup *CDmeClip::FindTrackGroup( const char *pTrackGroupName ) const
{
	if ( !pTrackGroupName )
	{
		pTrackGroupName = DMETRACKGROUP_DEFAULT_NAME;
	}

	int c = m_TrackGroups.Count();
	for ( int i = 0 ; i < c; ++i )
	{
		CDmeTrackGroup *pTrackGroup = m_TrackGroups[i];
		if ( !pTrackGroup )
			continue;

		if ( !Q_strcmp( pTrackGroup->GetName(), pTrackGroupName ) )
			return pTrackGroup;
	}
	return NULL;
}

int CDmeClip::GetTrackGroupIndex( CDmeTrackGroup *pTrackGroup ) const
{
	int nTrackGroups = m_TrackGroups.Count();
	for ( int i = 0 ; i < nTrackGroups; ++i )
	{
		if ( pTrackGroup == m_TrackGroups[i] )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Find or create a track group
//-----------------------------------------------------------------------------
CDmeTrackGroup *CDmeClip::FindOrAddTrackGroup( const char *pTrackGroupName )
{
	CDmeTrackGroup *pTrackGroup = FindTrackGroup( pTrackGroupName );
	if ( !pTrackGroup )
	{
		pTrackGroup = AddTrackGroup( pTrackGroupName );
	}
	return pTrackGroup;
}


//-----------------------------------------------------------------------------
// Finding clips in track groups
//-----------------------------------------------------------------------------
CDmeTrack *CDmeClip::FindTrackForClip( CDmeClip *pClip, CDmeTrackGroup **ppTrackGroup /*= NULL*/ ) const
{
//	DmeClipType_t type = pClip->GetClipType();
	int c = m_TrackGroups.Count();
	for ( int i = 0 ; i < c; ++i )
	{
		// FIXME: If trackgroups have valid types, can early out here
		CDmeTrack *pTrack = m_TrackGroups[i]->FindTrackForClip( pClip );
		if ( pTrack )
		{
			if ( ppTrackGroup )
			{
				*ppTrackGroup = m_TrackGroups[i];
			}
			return pTrack;
		}
	}

	return NULL;
}

bool CDmeClip::FindMultiTrackGroupForClip( CDmeClip *pClip, int *pTrackGroupIndex, int *pTrackIndex, int *pClipIndex ) const
{
	int nTrackGroups = m_TrackGroups.Count();
	for ( int gi = 0 ; gi < nTrackGroups; ++gi )
	{
		CDmeTrackGroup *pTrackGroup = m_TrackGroups[ gi ];
		if ( !pTrackGroup )
			continue;

		if ( !pTrackGroup->FindTrackForClip( pClip, pTrackIndex, pClipIndex ) )
			continue;

		if ( pTrackGroupIndex )
		{
			*pTrackGroupIndex = gi;
		}
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Finding clips in tracks by time
//-----------------------------------------------------------------------------
void CDmeClip::FindClipsAtTime( DmeClipType_t clipType, DmeTime_t time, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( clipType == DMECLIP_FILM )
		return;

	int gc = GetTrackGroupCount();
	for ( int i = 0; i < gc; ++i )
	{
		CDmeTrackGroup *pTrackGroup = GetTrackGroup( i );
		if ( !pTrackGroup )
			continue;

		pTrackGroup->FindClipsAtTime( clipType, time, flags, clips );
	}
}

void CDmeClip::FindClipsIntersectingTime( DmeClipType_t clipType, DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( clipType == DMECLIP_FILM )
		return;

	int gc = GetTrackGroupCount();
	for ( int i = 0; i < gc; ++i )
	{
		CDmeTrackGroup *pTrackGroup = GetTrackGroup( i );
		if ( !pTrackGroup )
			continue;

		pTrackGroup->FindClipsIntersectingTime( clipType, startTime, endTime, flags, clips );
	}
}

void CDmeClip::FindClipsWithinTime( DmeClipType_t clipType, DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( clipType == DMECLIP_FILM )
		return;

	int gc = GetTrackGroupCount();
	for ( int i = 0; i < gc; ++i )
	{
		CDmeTrackGroup *pTrackGroup = GetTrackGroup( i );
		if ( !pTrackGroup )
			continue;

		pTrackGroup->FindClipsWithinTime( clipType, startTime, endTime, flags, clips );
	}
}


//-----------------------------------------------------------------------------
// Build a list of all referring clips
//-----------------------------------------------------------------------------
static int BuildReferringClipList( const CDmeClip *pClip, CDmeClip** ppParents, int nMaxCount )
{
	int nCount = 0;

	DmAttributeReferenceIterator_t it, it2, it3;
	for ( it = g_pDataModel->FirstAttributeReferencingElement( pClip->GetHandle() );
		it != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;
		it = g_pDataModel->NextAttributeReferencingElement( it ) )
	{
		CDmAttribute *pAttribute = g_pDataModel->GetAttribute( it );
		const char *pName = pAttribute->GetName();
		CDmElement *pElement = pAttribute->GetOwner();
		CDmeTrack *pTrack = CastElement< CDmeTrack >( pElement );
		if ( !pTrack )
			continue;

		for ( it2 = g_pDataModel->FirstAttributeReferencingElement( pTrack->GetHandle() );
			it2 != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;
			it2 = g_pDataModel->NextAttributeReferencingElement( it2 ) )
		{
			pAttribute = g_pDataModel->GetAttribute( it2 );
			pName = pAttribute->GetName();
			pElement = pAttribute->GetOwner();
			CDmeTrackGroup *pTrackGroup = CastElement< CDmeTrackGroup >( pElement );
			if ( !pTrackGroup )
				continue;

			for ( it3 = g_pDataModel->FirstAttributeReferencingElement( pTrackGroup->GetHandle() );
				it3 != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;
				it3 = g_pDataModel->NextAttributeReferencingElement( it3 ) )
			{
				pAttribute = g_pDataModel->GetAttribute( it3 );
				pName = pAttribute->GetName();
				pElement = pAttribute->GetOwner();
				CDmeClip *pParent = CastElement< CDmeClip >( pElement );
				if ( !pParent )
					continue;

				Assert( nCount < nMaxCount );
				if ( nCount >= nMaxCount )
					return nCount;
				ppParents[nCount++] = pParent;
			}
		}
	}
	return nCount;
}

bool CDmeClip::BuildClipStack( DmeClipStack_t* pStack, const CDmeClip *pMovie, CDmeClip *pShot /*=NULL*/ )
{
	// Walk through each shot in the movie and look for the subClip, if don't find it recurse into each shot
	return pStack->BuildClipStack( pMovie, pShot, this );
}


//-----------------------------------------------------------------------------
// Clip stack
//-----------------------------------------------------------------------------
bool DmeClipStack_t::BuildClipStack_R( const CDmeClip *pMovie, const CDmeClip *pShot, const CDmeClip *pCurrent )
{
	// Add this clip to the stack
	int nIndex = AddClipToHead( pCurrent );

	// Is this clip the shot? We don't need to look for it any more.
	if ( pCurrent == pShot )
	{
		pShot = NULL;
	}

	// Is this clip the movie? We succeeded if we already found the shot!
	if ( pCurrent == pMovie )
	{
		if ( !pShot )
			return true;
	}
	else
	{
		// NOTE: This algorithm assumes a clip can never appear twice under another clip
		// at a single level of hierarchy.
		CDmeClip* ppParents[1024];
		int nCount = BuildReferringClipList( pCurrent, ppParents, 1024 );
		for ( int i = 0; i < nCount; ++i )
		{
			// Can we find a path to the root through the shot? We succeeded!
			if ( BuildClipStack_R( pMovie, pShot, ppParents[i] ) )
				return true;
		}
	}

	// This clip didn't work out for us. Remove it.
	RemoveClip( nIndex );

	return false;
}

bool DmeClipStack_t::BuildClipStack( const CDmeClip *pMovie, const CDmeClip *pShot, const CDmeClip *pClip )
{
	// Walk through each shot in the movie and look for the subClip, if don't find it recurse into each shot
	RemoveAll();
	return BuildClipStack_R( pMovie, pShot, pClip );
}

int DmeClipStack_t::FindClip( const CDmeClip *pClip ) const
{
	if ( !pClip )
		return m_clips.InvalidIndex();

	return m_clips.Find( pClip->GetHandle() );
}

int DmeClipStack_t::AddClipToHead( const CDmeClip *pClip )
{
	if ( !pClip )
		return m_clips.InvalidIndex();

	m_bOptimized = false;
	return m_clips.AddToHead( pClip->GetHandle() );
}

int DmeClipStack_t::AddClipToTail( const CDmeClip *pClip )
{
	if ( !pClip )
		return m_clips.InvalidIndex();

	m_bOptimized = false;
	return m_clips.AddToTail( pClip->GetHandle() );
}

//#define TEST_CLIP_STACK_OPTIMIZER

void DmeClipStack_t::Optimize() const
{
	m_bOptimized = true;

	int nClips = m_clips.Count();
	if ( nClips == 0 )
	{
		m_tStart = m_tOffset = DMETIME_MINTIME + DMETIME_MAXTIME / 2; // HACK
		m_tDuration = DMETIME_MAXTIME;
		m_flScale = 1.0f;
		return;
	}

	// 0 = global, n-1 = local

#ifdef TEST_CLIP_STACK_OPTIMIZER
	DmeTime_t tOptimizedClamped0 = DMETIME_ZERO;
	DmeTime_t tTimeFrameClamped0 = DMETIME_ZERO;
	DmeTime_t tOptimizedUnclamped0 = DMETIME_ZERO;
	DmeTime_t tTimeFrameUnclamped0 = DMETIME_ZERO;
	DmeTime_t tOptimizedClamped1 = DmeTime_t( 10000 );
	DmeTime_t tTimeFrameClamped1 = DmeTime_t( 10000 );
	DmeTime_t tOptimizedUnclamped1 = DmeTime_t( 10000 );
	DmeTime_t tTimeFrameUnclamped1 = DmeTime_t( 10000 );
#endif

	const CDmeClip *pClip = m_clips[ nClips - 1 ];
	m_tStart    = pClip->GetStartTime();
	m_tOffset   = pClip->GetTimeOffset();
	m_tDuration = pClip->GetDuration();
	m_flScale   = pClip->GetTimeScale();

#ifdef TEST_CLIP_STACK_OPTIMIZER
	tOptimizedClamped0 = FromChildMediaTime( DMETIME_ZERO, true );
	tTimeFrameClamped0 = pClip->FromChildMediaTime( tTimeFrameClamped0, true );
	Assert( tOptimizedClamped0 == tTimeFrameClamped0 );

	tOptimizedUnclamped0 = FromChildMediaTime( DMETIME_ZERO, false );
	tTimeFrameUnclamped0 = pClip->FromChildMediaTime( tTimeFrameUnclamped0, false );
	Assert( tOptimizedUnclamped0 == tTimeFrameUnclamped0 );

	tOptimizedClamped1 = FromChildMediaTime( DmeTime_t( 10000 ), true );
	tTimeFrameClamped1 = pClip->FromChildMediaTime( tTimeFrameClamped1, true );
	Assert( tOptimizedClamped1 == tTimeFrameClamped1 );

	tOptimizedUnclamped1 = FromChildMediaTime( DmeTime_t( 10000 ), false );
	tTimeFrameUnclamped1 = pClip->FromChildMediaTime( tTimeFrameUnclamped1, false );
	Assert( tOptimizedUnclamped1 == tTimeFrameUnclamped1 );
#endif

	for ( int i = nClips - 2; i > 0; --i )
	{
		const CDmeClip *pClip = m_clips[ i ];

#ifdef TEST_CLIP_STACK_OPTIMIZER
		DmeTime_t tOldStart = m_tStart;
		DmeTime_t tOldOffset = m_tOffset;
		DmeTime_t tOldDuration = m_tDuration;
		float flOldScale = m_flScale;

		DmeTime_t tClipStart = pClip->GetStartTime();
		DmeTime_t tClipOffset = pClip->GetTimeOffset();
		DmeTime_t tClipDuration = pClip->GetDuration();
		float flClipSCale = pClip->GetTimeScale();
#endif

		DmeTime_t tChildStartInParentTime = pClip->FromChildMediaTime( m_tStart, false );
		DmeTime_t tChildOffsetInParentTime = pClip->FromChildMediaDuration( m_tOffset );
		DmeTime_t tChildDurationInParentTime = pClip->FromChildMediaDuration( m_tDuration );
		DmeTime_t tChildEndInParentTime = tChildStartInParentTime + tChildDurationInParentTime;
		m_flScale = pClip->GetTimeScale() * m_flScale;

		m_tStart = MAX( tChildStartInParentTime, pClip->GetStartTime() );
		DmeTime_t tDiff = tChildStartInParentTime - m_tStart;
		m_tOffset = tChildOffsetInParentTime - tDiff;
		DmeTime_t tEnd = MIN( tChildEndInParentTime, pClip->GetEndTime() );
		m_tDuration = MAX( DMETIME_ZERO, tEnd - m_tStart );

#ifdef TEST_CLIP_STACK_OPTIMIZER
		tOptimizedClamped0 = FromChildMediaTime( DMETIME_ZERO, true );
		tTimeFrameClamped0 = pClip->FromChildMediaTime( tTimeFrameClamped0, true );
		Assert( tOptimizedClamped0 == tTimeFrameClamped0 );

		tOptimizedUnclamped0 = FromChildMediaTime( DMETIME_ZERO, false );
		tTimeFrameUnclamped0 = pClip->FromChildMediaTime( tTimeFrameUnclamped0, false );
		Assert( tOptimizedUnclamped0 == tTimeFrameUnclamped0 );

		tOptimizedClamped1 = FromChildMediaTime( DmeTime_t( 10000 ), true );
		tTimeFrameClamped1 = pClip->FromChildMediaTime( tTimeFrameClamped1, true );
		Assert( tOptimizedClamped1 == tTimeFrameClamped1 );

		tOptimizedUnclamped1 = FromChildMediaTime( DmeTime_t( 10000 ), false );
		tTimeFrameUnclamped1 = pClip->FromChildMediaTime( tTimeFrameUnclamped1, false );
		Assert( tOptimizedUnclamped1 == tTimeFrameUnclamped1 );
#endif
	}
}

DmeTime_t DmeClipStack_t::ToChildMediaTime( DmeTime_t t, bool bClamp /*=true*/ ) const
{
	if ( !m_bOptimized )
	{
		Optimize();
	}

	t -= m_tStart;
	if ( bClamp )
	{
		t.Clamp( DMETIME_ZERO, m_tDuration );
	}
	return ( t + m_tOffset ) * m_flScale;
}

DmeTime_t DmeClipStack_t::FromChildMediaTime( DmeTime_t t, bool bClamp /*=true*/ ) const
{
	if ( !m_bOptimized )
	{
		Optimize();
	}

	t = t / m_flScale - m_tOffset;
	if ( bClamp )
	{
		t.Clamp( DMETIME_ZERO, m_tDuration );
	}
	return t + m_tStart;
}

DmeTime_t DmeClipStack_t::ToChildMediaDuration( DmeTime_t t ) const
{
	if ( !m_bOptimized )
	{
		Optimize();
	}

	return t * m_flScale;
}

DmeTime_t DmeClipStack_t::FromChildMediaDuration( DmeTime_t t ) const
{
	if ( !m_bOptimized )
	{
		Optimize();
	}

	return t / m_flScale;
}

void DmeClipStack_t::ToChildMediaTime( TimeSelection_t &params ) const
{
	for ( int i = 0; i < TS_TIME_COUNT; ++i )
	{
		params.m_Times[i] = ToChildMediaTime( params.m_Times[i], false );
	}
}

//-----------------------------------------------------------------------------
//
// CDmeSoundClip - timeframe view into a dmesound
//
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSoundClip, CDmeSoundClip );

void CDmeSoundClip::OnConstruction()
{
	m_Sound.Init( this, "sound" );
	m_bShowWave.InitAndSet( this, "showwave", false );
	m_fadeInDuration .InitAndSet( this, "fadeIn", DMETIME_ZERO );
	m_fadeOutDuration.InitAndSet( this, "fadeOut", DMETIME_ZERO );
}

void CDmeSoundClip::OnDestruction()
{
}

void CDmeSoundClip::SetShowWave( bool state )
{
	m_bShowWave = state;
}

bool CDmeSoundClip::ShouldShowWave( ) const
{
	return m_bShowWave;
}

float CDmeSoundClip::GetVolumeFade( DmeTime_t tParent )
{
	float fade = 1.0f;
	if ( tParent < GetStartTime() + m_fadeInDuration )
	{
		fade = ( tParent - GetStartTime() ) / m_fadeInDuration;
	}
	if ( tParent > GetEndTime() - m_fadeOutDuration )
	{
		fade = MIN( fade, ( GetEndTime() - tParent ) / m_fadeOutDuration );
	}
	return fade;
}

void CDmeSoundClip::BakeTimeScale( float scale /*= 1.0f*/ )
{
	float flNewScale = scale * GetTimeScale();
	float flInvScale = 1.0f / flNewScale;

	m_fadeInDuration  *= flInvScale;
	m_fadeOutDuration *= flInvScale;

	BaseClass::BakeTimeScale( scale );
}

//-----------------------------------------------------------------------------
// CDmeChannelsClip - timeframe view into a set of channels
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeChannelsClip, CDmeChannelsClip );

void CDmeChannelsClip::OnConstruction()
{
	m_Channels.Init( this, "channels" );
}

void CDmeChannelsClip::OnDestruction()
{
}

bool IsParticleSystemChannelsClip( CDmeChannelsClip *pChannelsClip )
{
	int nChannels = pChannelsClip->m_Channels.Count();
	for ( int i = 0; i < nChannels; ++i )
	{
		CDmeChannel *pChannel = pChannelsClip->m_Channels[ i ];
		if ( !pChannel )
			continue;

		CDmElement *pToElement = pChannel->GetToElement();
		if ( !pToElement )
			continue;

		if ( !V_stricmp( pToElement->GetTypeString(), "DmeGameParticleSystem" ) )
			return true;

		CDmeTransform *pTransform = CastElement< CDmeTransform >( pToElement );
		if ( !pTransform )
			continue;

		CUtlVector< CDmeDag* > dags;
		FindAncestorsReferencingElement( pTransform, dags );
		int nDags = dags.Count();
		for ( int i = 0; i < nDags; ++i )
		{
			if ( !V_stricmp( dags[ i ]->GetTypeString(), "DmeGameParticleSystem" ) )
				return true;
		}
	}

	return false;
}

void CDmeChannelsClip::BakeTimeScale( float scale /*= 1.0f*/ )
{
	float flNewScale = scale * GetTimeScale();

	// HACK - particle systems can't really be un-scaled in this way, for apparently two reasons:
	//		1) we're storing time data in the dmegameparticlesystem (ie in the scene, rather than in clips/channels)
	//		2) the game's particle systems appear to be semi-hardcoded in relation to time (so blinking materials don't blink at the scaled rate after baking)
	if ( IsParticleSystemChannelsClip( this ) )
	{
		CDmeTimeFrame *tf = m_TimeFrame.GetElement();
		tf->SetStartTime ( tf->GetStartTime () / scale );
		tf->SetDuration  ( tf->GetDuration  () / scale );
		tf->SetTimeOffset( tf->GetTimeOffset() / scale );
		tf->SetTimeScale ( scale );
		return; // skip particle system channels clips
	}

	int nChannels = m_Channels.Count();
	for ( int i = 0; i < nChannels; ++i )
	{
		CDmeChannel *pChannel = m_Channels[ i ];
		if ( !pChannel )
			continue;
 		pChannel->ScaleSampleTimes( 1.0f / flNewScale );
	}

	BaseClass::BakeTimeScale( scale );
}

CDmeChannel *CDmeChannelsClip::CreatePassThruConnection( char const *passThruName,
														CDmElement *pFrom, char const *pFromAttribute,
														CDmElement *pTo, char const *pToAttribute, int index /*= 0*/ )
{
	CDmeChannel *helper = CreateElement< CDmeChannel >( passThruName, GetFileId() );
	Assert( helper );

	helper->SetMode( CM_PASS );
	helper->SetInput( pFrom, pFromAttribute );
	helper->SetOutput( pTo, pToAttribute, index );

	m_Channels.AddToTail( helper );

	return helper;
}

void CDmeChannelsClip::RemoveChannel( CDmeChannel *pChannel )
{
	int nCount = m_Channels.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pChannel == m_Channels[i] )
		{
			m_Channels.Remove( i );
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the mode of all of the channels in the clip
//-----------------------------------------------------------------------------
void CDmeChannelsClip::SetChannelMode( const ChannelMode_t &mode )
{
	int nCount = m_Channels.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeChannel *pChannel = m_Channels[ i ];
		if ( pChannel )
		{
			pChannel->SetMode( mode );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Operate all of the channels in the clip
//-----------------------------------------------------------------------------
void CDmeChannelsClip::OperateChannels()
{
	int nCount = m_Channels.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeChannel *pChannel = m_Channels[ i ];
		if ( pChannel )
		{
			pChannel->Operate();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Play all of the channels in the clip
//-----------------------------------------------------------------------------
void CDmeChannelsClip::PlayChannels()
{
	int nCount = m_Channels.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeChannel *pChannel = m_Channels[ i ];
		if ( pChannel )
		{
			pChannel->Play();
		}
	}
}


//-----------------------------------------------------------------------------
// CDmeFXClip - timeframe view into an effect
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeFXClip, CDmeFXClip );

void CDmeFXClip::OnConstruction()
{
}

void CDmeFXClip::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Global list of FX clip types
//-----------------------------------------------------------------------------
const char *CDmeFXClip::s_pFXClipTypes[MAX_FXCLIP_TYPES];
const char *CDmeFXClip::s_pFXClipDescriptions[MAX_FXCLIP_TYPES];
int CDmeFXClip::s_nFXClipTypeCount = 0;

void CDmeFXClip::InstallFXClipType( const char *pElementType, const char *pDescription )
{
	s_pFXClipTypes[s_nFXClipTypeCount] = pElementType;
	s_pFXClipDescriptions[s_nFXClipTypeCount] = pDescription;
	++s_nFXClipTypeCount;
}

int CDmeFXClip::FXClipTypeCount()
{
	return s_nFXClipTypeCount;
}

const char *CDmeFXClip::FXClipType( int nIndex )
{
	Assert( s_nFXClipTypeCount > nIndex );
	return s_pFXClipTypes[nIndex];
}

const char *CDmeFXClip::FXClipDescription( int nIndex )
{
	Assert( s_nFXClipTypeCount > nIndex );
	return s_pFXClipDescriptions[nIndex];
}


//-----------------------------------------------------------------------------
// CDmeFilmClip - hierarchical clip (movie, sequence or shot) w/ scene info
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeFilmClip, CDmeFilmClip );

void CDmeFilmClip::OnConstruction()
{
	m_pRemoteVideoMaterial = NULL;
	m_MaterialOverlayEffect.Init( this, "materialOverlay" );

	m_MapName.Init( this, "mapname" );
	m_Camera.Init( this, "camera" );
	m_MonitorCameras.Init( this, "monitorCameras" );
	m_nActiveMonitor.InitAndSet( this, "activeMonitor", -1 );
	m_Scene.Init( this, "scene" );
	m_AVIFile.Init( this, "aviFile", FATTRIB_HAS_CALLBACK );
	m_fadeInDuration .InitAndSet( this, "fadeIn", DMETIME_ZERO );
	m_fadeOutDuration.InitAndSet( this, "fadeOut", DMETIME_ZERO );

	m_Inputs.Init( this, "inputs" );
	m_Operators.Init( this, "operators" );
	m_bIsUsingCachedVersion.Init( this, "useAviFile", FATTRIB_HAS_CALLBACK );
	m_AnimationSets.Init( this, "animationSets" );
	m_BookmarkSets.Init( this, "bookmarkSets" );
	m_nActiveBookmarkSet.Init(this, "activeBookmarkSet", 0 );
	m_FilmTrackGroup.Init( this, "subClipTrackGroup", FATTRIB_HAS_CALLBACK );
	m_Volume.InitAndSet( this, "volume", 1.0);
	m_ConCommands.Init( this, "concommands" );
	m_ConVars.Init( this, "convars" );

	m_hCachedVersion = AVIMATERIAL_INVALID;
	m_bIsUsingCachedVersion = false;
	m_bReloadCachedVersion = false;

	m_CameraStack.Init( this, "camerastack" );

	m_nCurrentStackCamera = 0;
}

void CDmeFilmClip::OnDestruction()
{
	AssignRemoteVideoMaterial( NULL );
	if ( m_hCachedVersion != AVIMATERIAL_INVALID )
	{
		g_pAVI->DestroyAVIMaterial( m_hCachedVersion );
		m_hCachedVersion = AVIMATERIAL_INVALID;
	}

	PurgeCameraStack();
}

void CDmeFilmClip::BakeTimeScale( float scale /*= 1.0f*/ )
{
	float flNewScale = scale * GetTimeScale();
	float flInvScale = 1.0f / flNewScale;

	int nBookmarkSets = m_BookmarkSets.Count();
	for ( int i = 0; i < nBookmarkSets; ++i )
	{
		CDmeBookmarkSet *pBookmarkSet = m_BookmarkSets[ i ];
		if ( !pBookmarkSet )
			continue;

		pBookmarkSet->ScaleBookmarkTimes( flInvScale );
	}

	m_fadeInDuration  *= flInvScale;
	m_fadeOutDuration *= flInvScale;

	if ( CDmeTrack *pTrack = GetFilmTrack() )
	{
		int nClips = pTrack->GetClipCount();
		for ( int i = 0; i < nClips; ++i )
		{
			CDmeClip *pClip = pTrack->GetClip( i );
			if ( !pClip )
				continue;

			pClip->BakeTimeScale( flNewScale );
		}
	}

	BaseClass::BakeTimeScale( scale );
}


//-----------------------------------------------------------------------------
// Returns the special film track group
//-----------------------------------------------------------------------------
CDmeTrackGroup *CDmeFilmClip::GetFilmTrackGroup() const
{
	return m_FilmTrackGroup;
}

//-----------------------------------------------------------------------------
// Returns the special film track
//-----------------------------------------------------------------------------
CDmeTrack *CDmeFilmClip::GetFilmTrack() const
{
	CDmeTrackGroup *pTrackGroup = m_FilmTrackGroup.GetElement();
	if ( pTrackGroup )
		return pTrackGroup->GetFilmTrack();
	return NULL;
}

CDmeTrackGroup *CDmeFilmClip::FindOrCreateFilmTrackGroup()
{
	if ( !m_FilmTrackGroup )
	{
		m_FilmTrackGroup = CreateElement< CDmeTrackGroup >( "subClipTrackGroup", GetFileId() );
		m_FilmTrackGroup->SetMinimized( false );
		m_FilmTrackGroup->SetMaxTrackCount( 1 );
	}
	return m_FilmTrackGroup;
}

CDmeTrack *CDmeFilmClip::FindOrCreateFilmTrack()
{
	CDmeTrackGroup *pTrackGroup = FindOrCreateFilmTrackGroup();
	CDmeTrack *pTrack = pTrackGroup->GetFilmTrack();
	return pTrack ? pTrack : m_FilmTrackGroup->CreateFilmTrack();
}


//-----------------------------------------------------------------------------
// Finding clips in track groups
//-----------------------------------------------------------------------------
CDmeTrack *CDmeFilmClip::FindTrackForClip( CDmeClip *pClip, CDmeTrackGroup **ppTrackGroup /*= NULL*/ ) const
{
	if ( m_FilmTrackGroup.GetElement() )
	{
		CDmeTrack *pTrack = m_FilmTrackGroup->FindTrackForClip( pClip );
		if ( pTrack )
		{
			if ( ppTrackGroup )
			{
				*ppTrackGroup = m_FilmTrackGroup;
			}
			return pTrack;
		}
	}

	return CDmeClip::FindTrackForClip( pClip, ppTrackGroup );
}

//-----------------------------------------------------------------------------
// Finding clips in tracks by time
//-----------------------------------------------------------------------------
void CDmeFilmClip::FindClipsAtTime( DmeClipType_t clipType, DmeTime_t time, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( ( clipType == DMECLIP_FILM ) || ( clipType == DMECLIP_UNKNOWN ) )
	{
		if ( m_FilmTrackGroup )
		{
			m_FilmTrackGroup->FindClipsAtTime( clipType, time, flags, clips );
		}
	}

	CDmeClip::FindClipsAtTime( clipType, time, flags, clips );
}

void CDmeFilmClip::FindClipsIntersectingTime( DmeClipType_t clipType, DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( ( clipType == DMECLIP_FILM ) || ( clipType == DMECLIP_UNKNOWN ) )
	{
		if ( m_FilmTrackGroup )
		{
			m_FilmTrackGroup->FindClipsIntersectingTime( clipType, startTime, endTime, flags, clips );
		}
	}

	CDmeClip::FindClipsIntersectingTime( clipType, startTime, endTime, flags, clips );
}

void CDmeFilmClip::FindClipsWithinTime( DmeClipType_t clipType, DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const
{
	if ( ( clipType == DMECLIP_FILM ) || ( clipType == DMECLIP_UNKNOWN ) )
	{
		if ( m_FilmTrackGroup )
		{
			m_FilmTrackGroup->FindClipsWithinTime( clipType, startTime, endTime, flags, clips );
		}
	}

	CDmeClip::FindClipsWithinTime( clipType, startTime, endTime, flags, clips );
}


//-----------------------------------------------------------------------------
// Volume
//-----------------------------------------------------------------------------
void CDmeFilmClip::SetVolume( float state )
{
	m_Volume = state;
}
float CDmeFilmClip::GetVolume() const
{
	return m_Volume.Get();
}

int	CDmeFilmClip::GetConCommandCount() const
{
	return m_ConCommands.Count();
}

const char *CDmeFilmClip::GetConCommand( int i ) const
{
	return m_ConCommands[ i ];
}

int	CDmeFilmClip::GetConVarCount() const
{
	return m_ConVars.Count();
}

const char *CDmeFilmClip::GetConVar( int i ) const
{
	return m_ConVars[ i ];
}

//-----------------------------------------------------------------------------
// mapname helper methods
//-----------------------------------------------------------------------------
const char *CDmeFilmClip::GetMapName()
{
	return m_MapName.Get();
}

void CDmeFilmClip::SetMapName( const char *pMapName )
{
	m_MapName.Set( pMapName );
}


//-----------------------------------------------------------------------------
// Attribute changed
//-----------------------------------------------------------------------------
void CDmeFilmClip::OnAttributeChanged( CDmAttribute *pAttribute )
{
	BaseClass::OnAttributeChanged( pAttribute );
	if ( pAttribute == m_FilmTrackGroup.GetAttribute() )
	{
		if ( m_FilmTrackGroup.GetElement() )
		{
			m_FilmTrackGroup->SetMaxTrackCount( 1 );
		}																											 
	}
	else if ( pAttribute->GetOwner() == m_TimeFrame.GetElement() )
	{
		InvokeOnAttributeChangedOnReferrers( GetHandle(), pAttribute );
	}
	else if( pAttribute == m_bIsUsingCachedVersion.GetAttribute()  || pAttribute == m_AVIFile.GetAttribute() )
	{
		// video caching info has changed ...
		UpdateRemoteVideoMaterialStatus();
	}
	
}


//-----------------------------------------------------------------------------
// methods for dealing with remote cached video
//-----------------------------------------------------------------------------
void CDmeFilmClip::AssignRemoteVideoMaterial( IRemoteVideoMaterial *theMaterial )
{
	if ( theMaterial == m_pRemoteVideoMaterial ) return;		// no change

	// ok, release any previous material
	if ( m_pRemoteVideoMaterial != NULL )
	{
		m_pRemoteVideoMaterial->Release();
	}

	// assign the new material
	m_pRemoteVideoMaterial = theMaterial;
}


void CDmeFilmClip::UpdateRemoteVideoMaterialStatus()
{
	// is the quicktime Video caching service available?   If not, we don't do anything...
	if ( m_pRemoteVideoMaterial == NULL || !m_pRemoteVideoMaterial->IsInitialized() )
	{
		return;
	}

	// are we selecting to not used cached video?
	if ( m_bIsUsingCachedVersion == false )
	{
		// check to see if we've gone from using cached video for this clip to off
		if ( m_pRemoteVideoMaterial->IsRemoteVideoAvailable() && m_pRemoteVideoMaterial->IsConnectedToRemoteVideo() )
		{
			// turn off video caching for the specified clip
			m_pRemoteVideoMaterial->DisconnectFromRemoteVideo();
		}
		return;
	}
	
	// ok, we've selected to use a remotely cached video.  If the filename is setup correctly
	// this should attempt to connect to the remote video
	m_pRemoteVideoMaterial->ConnectToRemoteVideo( m_AVIFile.Get() );

}

bool CDmeFilmClip::HasRemoteVideo()
{
	// if we don't have a working connection, say we don't have any video
	return  ( ( m_pRemoteVideoMaterial == NULL ) ? false : m_pRemoteVideoMaterial->IsRemoteVideoAvailable() );
}

bool CDmeFilmClip::GetCachedQTVideoFrameAt( float theTime )
{
	if ( m_pRemoteVideoMaterial == NULL )  return false;
	
	return m_pRemoteVideoMaterial->GetRemoteVideoFrame( theTime );
}

IMaterial* CDmeFilmClip::GetRemoteVideoMaterial()
{
	return ( m_pRemoteVideoMaterial == NULL ) ? NULL : m_pRemoteVideoMaterial->GetRemoteVideoFrameMaterial();
}

void CDmeFilmClip::GetRemoteVideoMaterialTexCoordRange( float *u, float *v )
{
	if ( m_pRemoteVideoMaterial == NULL )
	{
		*u = 0.0f;
		*v = 0.0f;
	}
	else
	{
		m_pRemoteVideoMaterial->GetRemoteVideoFrameTextureCoordRange( *u, *v );
	}
}



void CDmeFilmClip::OnElementUnserialized()
{
	BaseClass::OnElementUnserialized();

	CDmeTrackGroup *pFilmTrackGroup = m_FilmTrackGroup.GetElement();
	if ( pFilmTrackGroup )
	{
		pFilmTrackGroup->SetMaxTrackCount( 1 );
		if ( pFilmTrackGroup->GetTrackCount() == 0 )
		{
			pFilmTrackGroup->CreateFilmTrack();
		}
	}
}


//-----------------------------------------------------------------------------
// Resolve
//-----------------------------------------------------------------------------
void CDmeFilmClip::Resolve()
{
	BaseClass::Resolve();
	if ( m_AVIFile.IsDirty() )
	{
		m_bReloadCachedVersion = true;
		m_AVIFile.GetAttribute()->RemoveFlag( FATTRIB_DIRTY );
	}
}


//-----------------------------------------------------------------------------
// Helper for overlays
//-----------------------------------------------------------------------------
void CDmeFilmClip::SetOverlay( const char *pMaterialName )
{
	if ( pMaterialName && pMaterialName[0] )
	{
		if ( !m_MaterialOverlayEffect.GetElement() )
		{
			m_MaterialOverlayEffect = CreateElement<CDmeMaterialOverlayFXClip>( "materialOverlay", GetFileId() );
		}

		m_MaterialOverlayEffect->SetOverlayEffect( pMaterialName );
	}
	else
	{
		m_MaterialOverlayEffect.Set( NULL );
	}
}

IMaterial *CDmeFilmClip::GetOverlayMaterial()
{
	return m_MaterialOverlayEffect.GetElement() ? m_MaterialOverlayEffect->GetMaterial() : NULL;
}

float CDmeFilmClip::GetOverlayAlpha()
{
	return m_MaterialOverlayEffect.GetElement() ? m_MaterialOverlayEffect->GetAlpha() : 0.0f;
}

void CDmeFilmClip::SetOverlayAlpha( float alpha )
{
	if ( m_MaterialOverlayEffect.GetElement() )
	{
		m_MaterialOverlayEffect->SetAlpha( alpha );
	}
}

bool CDmeFilmClip::HasOpaqueOverlay( void )
{
	if ( m_MaterialOverlayEffect->GetMaterial()->IsTranslucent() ||
	   ( m_MaterialOverlayEffect->GetAlpha() < 1.0f ) )
	{
		return false;
	}

	return true;
}

void CDmeFilmClip::DrawOverlay( DmeTime_t time, Rect_t &currentRect, Rect_t &totalRect )
{
	if ( m_MaterialOverlayEffect.GetElement() )
	{
		m_MaterialOverlayEffect->ApplyEffect( ToChildMediaTime( time ), currentRect, totalRect, NULL );
	}

	float fade = 1.0f;
	if ( time < GetStartTime() + m_fadeInDuration )
	{
		fade = ( time - GetStartTime() ) / m_fadeInDuration;
	}
	if ( time > GetEndTime() - m_fadeOutDuration )
	{
		fade = MIN( fade, ( GetEndTime() - time ) / m_fadeOutDuration );
	}
	if ( fade < 1.0f )
	{
		if ( !m_FadeMaterial.IsValid() )
		{
			m_FadeMaterial.Init( "engine\\singlecolor.vmt", NULL, false );
		}

		float r, g, b;
		m_FadeMaterial->GetColorModulation( &r, &g, &b );
		float a = m_FadeMaterial->GetAlphaModulation();

		m_FadeMaterial->ColorModulate( 0.0f, 0.0f, 0.0f );
		m_FadeMaterial->AlphaModulate( 1.0f - fade );

		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->Bind( m_FadeMaterial );

		float w = currentRect.width;
		float h = currentRect.height;

		IMesh *pMesh = pRenderContext->GetDynamicMesh();
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_TRIANGLE_STRIP, 2 );

		meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();

		meshBuilder.Position3f( 0.0f, h, 0.0f );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();

		meshBuilder.Position3f( w, 0.0f, 0.0f );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();

		meshBuilder.Position3f( w, h, 0.0f );
		meshBuilder.AdvanceVertexF<VTX_HAVEPOS, 0>();

		meshBuilder.End();
		pMesh->Draw();

		m_FadeMaterial->ColorModulate( r, g, b );
		m_FadeMaterial->AlphaModulate( a );
	}
}


//-----------------------------------------------------------------------------
// AVI tape out
//-----------------------------------------------------------------------------
void CDmeFilmClip::UseCachedVersion( bool bUseCachedVersion )
{
	m_bIsUsingCachedVersion = bUseCachedVersion;
}

bool CDmeFilmClip::IsUsingCachedVersion() const
{
	return m_bIsUsingCachedVersion;
}

AVIMaterial_t CDmeFilmClip::GetCachedAVI()
{
	if ( m_bReloadCachedVersion )
	{
		if ( g_pAVI )
		{
			if ( m_hCachedVersion != AVIMATERIAL_INVALID )
			{
				g_pAVI->DestroyAVIMaterial( m_hCachedVersion );
				m_hCachedVersion = AVIMATERIAL_INVALID;
			}
			if ( m_AVIFile[0] )
			{
				m_hCachedVersion = g_pAVI->CreateAVIMaterial( m_AVIFile, m_AVIFile, "MOD" );
			}
		}
		m_bReloadCachedVersion = false;
	}
	return m_hCachedVersion;
}
	
void CDmeFilmClip::SetCachedAVI( const char *pAVIFile )
{
	m_AVIFile = pAVIFile;
	m_bReloadCachedVersion = true;
}


//-----------------------------------------------------------------------------
// Camera helper methods
//-----------------------------------------------------------------------------
CDmeCamera *CDmeFilmClip::GetCamera()
{
	return m_Camera;
}

void CDmeFilmClip::SetCamera( CDmeCamera *pCamera )
{
	m_Camera = pCamera;
}


//-----------------------------------------------------------------------------
// Returns the monitor camera associated with the clip (for now, only 1 supported)
//-----------------------------------------------------------------------------
CDmeCamera *CDmeFilmClip::GetMonitorCamera()
{
	if ( m_nActiveMonitor < 0 )
		return NULL;
	return m_MonitorCameras[ m_nActiveMonitor ];
}

void CDmeFilmClip::AddMonitorCamera( CDmeCamera *pCamera )
{
	m_MonitorCameras.AddToTail( pCamera );
}

int CDmeFilmClip::FindMonitorCamera( CDmeCamera *pCamera )
{
	return m_MonitorCameras.Find( pCamera->GetHandle() );
}

void CDmeFilmClip::RemoveMonitorCamera( CDmeCamera *pCamera )
{
	int i = m_MonitorCameras.Find( pCamera->GetHandle() );
	if ( i >= 0 )
	{
		if ( m_nActiveMonitor == i )
		{
			m_nActiveMonitor = -1;
		}
		m_MonitorCameras.FastRemove( i );
	}
}

void CDmeFilmClip::SelectMonitorCamera( CDmeCamera *pCamera )
{
	m_nActiveMonitor = pCamera ? m_MonitorCameras.Find( pCamera->GetHandle() ) : -1;
}


//-----------------------------------------------------------------------------
// Scene / Dag helper methods
//-----------------------------------------------------------------------------
CDmeDag *CDmeFilmClip::GetScene( bool bCreateIfNull /*= false*/ )
{
	CDmeDag *pScene = m_Scene.GetElement();
	if ( !pScene && bCreateIfNull )
	{
		pScene = CreateElement< CDmeDag >( "scene", GetFileId() );
		m_Scene = pScene;
	}
	return pScene;
}

void CDmeFilmClip::SetScene( CDmeDag *pDag )
{
	m_Scene.Set( pDag );
}


//-----------------------------------------------------------------------------
// helper for inputs and operators
//-----------------------------------------------------------------------------
int CDmeFilmClip::GetInputCount()
{
	return m_Inputs.Count();
}

CDmeInput *CDmeFilmClip::GetInput( int nIndex )
{
	if ( nIndex < 0 || nIndex >= m_Inputs.Count() )
		return NULL;

	return m_Inputs[ nIndex ];
}

void CDmeFilmClip::AddInput( CDmeInput *pInput )
{
	m_Inputs.AddToTail( pInput );
}

void CDmeFilmClip::RemoveAllInputs()
{
	m_Inputs.RemoveAll();
}

void CDmeFilmClip::AddOperator( CDmeOperator *pOperator )
{
	m_Operators.AddToTail( pOperator );
}

void CDmeFilmClip::RemoveOperator( CDmeOperator *pOperator )
{
	for ( int i = m_Operators.Count() - 1 ; i >= 0; --i )
	{
		if ( m_Operators[ i ] == pOperator )
		{
			m_Operators.Remove( i );
		}
	}
}

void CDmeFilmClip::CollectOperators( CUtlVector< DmElementHandle_t > &operators )
{
	int numInputs = m_Inputs.Count();
	for ( int i = 0; i < numInputs; ++i )
	{
		operators.AddToTail( m_Inputs[ i ]->GetHandle() );
	}

	int numOperators = m_Operators.Count();
	for ( int i = 0; i < numOperators; ++i )
	{
		operators.AddToTail( m_Operators[ i ]->GetHandle() );
	}
}

CDmaElementArray< CDmeOperator > &CDmeFilmClip::GetOperators()
{
	return m_Operators;
}

CDmaElementArray< CDmeAnimationSet > &CDmeFilmClip::GetAnimationSets()
{
	return m_AnimationSets;
}

const CDmaElementArray< CDmeAnimationSet > &CDmeFilmClip::GetAnimationSets() const
{
	return m_AnimationSets;
}

CDmeAnimationSet *CDmeFilmClip::FindAnimationSet( const char *pAnimSetName ) const
{
	int nAnimSets = m_AnimationSets.Count();
	for ( int i = 0; i < nAnimSets; ++i )
	{
		CDmeAnimationSet *pAnimSet = m_AnimationSets[ i ];
		if ( !pAnimSet )
			continue;

		if ( V_stricmp( pAnimSet->GetName(), pAnimSetName ) == 0 )
			return pAnimSet;
	}
	return NULL;
}

const CDmaElementArray< CDmeBookmarkSet > &CDmeFilmClip::GetBookmarkSets() const
{
	return m_BookmarkSets;
}

CDmaElementArray< CDmeBookmarkSet > &CDmeFilmClip::GetBookmarkSets()
{
	return m_BookmarkSets;
}

int CDmeFilmClip::GetActiveBookmarkSetIndex() const
{
	return m_nActiveBookmarkSet;
}

void CDmeFilmClip::SetActiveBookmarkSetIndex( int nActiveBookmarkSet )
{
	m_nActiveBookmarkSet = nActiveBookmarkSet;
}

CDmeBookmarkSet *CDmeFilmClip::GetActiveBookmarkSet()
{
	int nBookmarkSets = m_BookmarkSets.Count();
	if ( m_nActiveBookmarkSet >= nBookmarkSets )
		return NULL;

	return m_BookmarkSets[ m_nActiveBookmarkSet ];
}

CDmeBookmarkSet *CDmeFilmClip::CreateBookmarkSet( const char *pName /*= "default set"*/ )
{
	CDmeBookmarkSet *pBookmarkSet = CreateElement< CDmeBookmarkSet >( pName, GetFileId() );
	m_BookmarkSets.AddToTail( pBookmarkSet );
	return pBookmarkSet;
}


//-----------------------------------------------------------------------------
// Used to move clips in non-film track groups with film clips
// Call BuildClipAssociations before modifying the film track,
// then UpdateAssociatedClips after modifying it.
//-----------------------------------------------------------------------------
void CDmeFilmClip::BuildClipAssociations( CUtlVector< ClipAssociation_t > &association, bool bHandleGaps )
{
	association.RemoveAll();

	CDmeTrack *pFilmTrack = GetFilmTrack();
	if ( !pFilmTrack )
		return;

	int c = pFilmTrack->GetClipCount();
	int gc = GetTrackGroupCount();
	if ( c == 0 || gc == 0 )
		return;

	DmeTime_t clipStartTime = GetStartInChildMediaTime();
	DmeTime_t clipEndTime   = GetEndInChildMediaTime();

	// These slugs will be removed in UpdateAssociatedClips
	if ( bHandleGaps )
	{
		pFilmTrack->FillAllGapsWithSlugs( "__tempSlug__", clipStartTime, clipEndTime );
	}

	for ( int i = 0; i < gc; ++i )
	{
		CDmeTrackGroup *pTrackGroup = GetTrackGroup( i );
		int tc = pTrackGroup->GetTrackCount();
		for ( int j = 0; j < tc; ++j )
		{
			CDmeTrack *pTrack = pTrackGroup->GetTrack( j );
			if ( !pTrack->IsSynched() )
				continue;

			// Only have visible tracks now
			int cc = pTrack->GetClipCount();
			association.EnsureCapacity( association.Count() + cc );
			for ( int k = 0; k < cc; ++k )
			{
				CDmeClip *pClip = pTrack->GetClip( k );
				CDmeClip *pFilmClip = pFilmTrack->FindFilmClipAtTime( pClip->GetStartTime() );
				
				int nIndex = association.AddToTail();
				association[nIndex].m_hClip = pClip;
				association[nIndex].m_hAssociation = pFilmClip;
				association[nIndex].m_startTimeInAssociatedClip = DMETIME_ZERO;
				association[nIndex].m_offset = DMETIME_ZERO;
				if ( pFilmClip )
				{
					association[nIndex].m_startTimeInAssociatedClip = pFilmClip->ToChildMediaTime( pClip->GetStartTime(), false );
					association[nIndex].m_nType = ClipAssociation_t::HAS_CLIP;
					continue;
				}

				// Handle edge cases
				if ( pClip->GetStartTime() <= clipStartTime )
				{
					association[nIndex].m_offset = pClip->GetStartTime() - clipStartTime;
					association[nIndex].m_nType = ClipAssociation_t::BEFORE_START;
					continue;
				}

				if ( pClip->GetStartTime() >= clipEndTime )
				{
					association[nIndex].m_offset = pClip->GetStartTime() - clipEndTime;
					association[nIndex].m_nType = ClipAssociation_t::AFTER_END;
					continue;
				}

				association[nIndex].m_nType = ClipAssociation_t::NO_MOVEMENT;
			}
		}
	}
}


void CDmeFilmClip::UpdateAssociatedClips( CUtlVector< ClipAssociation_t > &association )
{
	int i;

	CDmeTrack *pFilmTrack = GetFilmTrack();
	if ( !pFilmTrack )
		return;

	int c = association.Count(); 
	if ( c > 0 )
	{
		DmeTime_t clipStartTime = GetStartInChildMediaTime();
		DmeTime_t clipEndTime   = GetEndInChildMediaTime();

		for ( i = 0; i < c; ++i )
		{
			ClipAssociation_t &curr = association[i];
			if ( !curr.m_hClip.Get() )
				continue;

			switch( curr.m_nType )
			{
			case ClipAssociation_t::HAS_CLIP:
				if ( curr.m_hAssociation.Get() )
				{
					curr.m_hClip->SetStartTime( curr.m_hAssociation->FromChildMediaTime( curr.m_startTimeInAssociatedClip, false ) );
				}
				break;

			case ClipAssociation_t::BEFORE_START:
				curr.m_hClip->SetStartTime( clipStartTime + curr.m_offset ); 
				break;

			case ClipAssociation_t::AFTER_END:
 				curr.m_hClip->SetStartTime( clipEndTime + curr.m_offset ); 
				break;
			}
		}
	}

	c = pFilmTrack->GetClipCount();
	for ( i = c; --i >= 0; )
	{
		CDmeClip *pClip = pFilmTrack->GetClip(i);
		if ( !Q_strcmp( pClip->GetName(), "__tempSlug__" ) )
		{
			pFilmTrack->RemoveClip( i );
		}
	}
}


//-----------------------------------------------------------------------------
// Creates a slug clip
//-----------------------------------------------------------------------------
CDmeFilmClip *CreateSlugClip( const char *pClipName, DmeTime_t startTime, DmeTime_t endTime, DmFileId_t fileid )
{
	CDmeFilmClip *pSlugClip = CreateElement<CDmeFilmClip>( pClipName, fileid );
	pSlugClip->CreateBookmarkSet();
	pSlugClip->GetTimeFrame()->SetName( "timeframe" );
	pSlugClip->SetStartTime( startTime );
	pSlugClip->SetDuration( endTime - startTime );
	pSlugClip->SetTimeOffset( DmeTime_t( 0 ) );
	pSlugClip->SetTimeScale( 1.0f );
	pSlugClip->SetClipColor( Color( 0, 0, 0, 128 ) );
	pSlugClip->SetOverlay( "vgui/black" );
	return pSlugClip;
}

//-----------------------------------------------------------------------------
// helper methods
//-----------------------------------------------------------------------------
CDmeTrack *GetParentTrack( CDmeClip *pClip )
{
	DmAttributeReferenceIterator_t hAttr = g_pDataModel->FirstAttributeReferencingElement( pClip->GetHandle() );
	for ( ; hAttr != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID; hAttr = g_pDataModel->NextAttributeReferencingElement( hAttr ) )
	{
		CDmAttribute *pAttr = g_pDataModel->GetAttribute( hAttr );
		if ( !pAttr )
			continue;

		CDmeTrack *pTrack = CastElement< CDmeTrack >( pAttr->GetOwner() );
		if ( pTrack )
			return pTrack;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Find the channel within targeting the specified element and 
// attribute.
//-----------------------------------------------------------------------------
CDmeChannel *FindChannelTargetingElement( CDmElement *pElement, const char *pAttributeName )
{
	CUtlVector< CDmeChannel* > channels( 0, 8 );
	FindAncestorsReferencingElement( pElement, channels );

	int nChannels = channels.Count();
	for ( int i = 0; i < nChannels; ++i )
	{
		CDmeChannel *pChannel = channels[ i ];
		if ( !pChannel )
			continue;

		CDmElement *toElement = pChannel->GetToElement();
		if ( toElement != pElement )
			continue;

		if ( pAttributeName && ( Q_stricmp( pChannel->GetToAttribute()->GetName(), pAttributeName ) != 0 ) )
			continue;

		return pChannel;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Find the channel within the specified channels clip targeting the 
// specified element and attribute.
//-----------------------------------------------------------------------------
CDmeChannel *FindChannelTargetingElement( CDmeChannelsClip *pChannelsClip, CDmElement *pElement, const char *pAttributeName )
{
	int nChannels = pChannelsClip->m_Channels.Count();
	for ( int i = 0; i < nChannels; ++i )
	{
		CDmeChannel *pChannel = pChannelsClip->m_Channels[ i ];
		if ( !pChannel )
			continue;

		CDmElement *toElement = pChannel->GetToElement();
		if ( toElement != pElement )
			continue;

		if ( pAttributeName && ( Q_stricmp( pChannel->GetToAttribute()->GetName(), pAttributeName ) != 0 ) )
			continue;

		return pChannel;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Find the channel within the specified film clip targeting the 
// specified element and attribute.
//-----------------------------------------------------------------------------
CDmeChannel *FindChannelTargetingElement( CDmeFilmClip *pClip, CDmElement *pElement, const char *pAttributeName, CDmeChannelsClip **ppChannelsClip, CDmeTrack **ppTrack, CDmeTrackGroup **ppTrackGroup )
{
	int gc = pClip->GetTrackGroupCount();
	for ( int i = 0; i < gc; ++i )
	{
		CDmeTrackGroup *pTrackGroup = pClip->GetTrackGroup( i );
		DMETRACKGROUP_FOREACH_CLIP_TYPE_START( CDmeChannelsClip, pTrackGroup, pTrack, pChannelsClip )

			CDmeChannel *pChannel = FindChannelTargetingElement( pChannelsClip, pElement, pAttributeName );
			if ( !pChannel )
				continue;

			if ( ppChannelsClip )
			{
				*ppChannelsClip = pChannelsClip;
			}
			if ( ppTrack )
			{
				*ppTrack = pTrack;
			}
			if ( ppTrackGroup )
			{
				*ppTrackGroup = pTrackGroup;
			}
			return pChannel;

		DMETRACKGROUP_FOREACH_CLIP_TYPE_END()
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Construct a clip stack for each one of the channels in the provided
// list using the specified root movie and shot.
//-----------------------------------------------------------------------------
void BuildClipStackList( const CUtlVector< CDmeChannel* > &channelList, CUtlVector< DmeClipStack_t > &clipStackList, CUtlVector< DmeTime_t > &orginalTimeList, const CDmeClip *pMovie, CDmeClip *pShot )
{
	int nChannels = channelList.Count();
	clipStackList.Purge();
	clipStackList.EnsureCount( nChannels );
	orginalTimeList.Purge();
	orginalTimeList.EnsureCount( nChannels );

	for ( int iChannel = 0; iChannel < nChannels; ++iChannel )
	{
		CDmeChannel *pChannel = channelList[ iChannel ];
		CDmeChannelsClip *pChannelsClip = FindAncestorReferencingElement< CDmeChannelsClip >( pChannel );

		if ( pChannelsClip )
		{
			pChannelsClip->BuildClipStack( &clipStackList[ iChannel ], pMovie, pShot );
		}
		else
		{		
			if ( pMovie )
			{
				clipStackList[ iChannel ].AddClipToTail( pMovie );
			}
			if ( pShot && ( pShot != pMovie ) )
			{
				clipStackList[ iChannel ].AddClipToTail( pShot );
			}
		}

		orginalTimeList[ iChannel ] = pChannel->GetCurrentTime();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Play each of the channels in the provided list at the specified 
// global time.
//-----------------------------------------------------------------------------
void PlayChannelsAtTime( DmeTime_t time, const CUtlVector< CDmeChannel* > &channelList, const CUtlVector< CDmeOperator* > &operatorList, const CUtlVector< DmeClipStack_t > &clipStackList, bool forcePlay )
{
	int nCount = channelList.Count();
	for ( int iChannel = 0; iChannel < nCount; ++iChannel )
	{
		CDmeChannel *pChannel = channelList[ iChannel ];
		DmeTime_t localTime = clipStackList[ iChannel ].ToChildMediaTime( time, false );
		pChannel->SetCurrentTime( localTime );
		if ( forcePlay || ( pChannel->GetMode() == CM_RECORD ) )
		{
			pChannel->Play();
		}
		else
		{
			pChannel->Operate();
		}
	}

	int nOperators = operatorList.Count();
	for ( int iOpeator = 0; iOpeator < nOperators; ++iOpeator )
	{
		operatorList[ iOpeator ]->Operate();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Play each of the channels in the provided list at the specified 
// local time.
//-----------------------------------------------------------------------------
void PlayChannelsAtLocalTimes( const CUtlVector< DmeTime_t > &timeList, const CUtlVector< CDmeChannel* > &channelList, const CUtlVector< CDmeOperator* > &operatorList, bool forcePlay )
{
	int nCount = channelList.Count();

	Assert( timeList.Count() >= channelList.Count() );
	if ( timeList.Count() < nCount )
		return;

	for ( int iChannel = 0; iChannel < nCount; ++iChannel )
	{
		CDmeChannel *pChannel = channelList[ iChannel ];
		pChannel->SetCurrentTime( timeList[ iChannel ] );
		if ( forcePlay || ( pChannel->GetMode() == CM_RECORD ) )
		{
			pChannel->Play();
		}
		else
		{
			pChannel->Operate();
		}
	}

	int nOperators = operatorList.Count();
	for ( int iOpeator = 0; iOpeator < nOperators; ++iOpeator )
	{
		operatorList[ iOpeator ]->Operate();
	}
}


CDmeFilmClip *FindFilmClipContainingDag( CDmeDag *pDag )
{
	CDmeFilmClip *pFilmClip = FindReferringElement< CDmeFilmClip >( pDag, "scene" );
	if ( pFilmClip )
		return pFilmClip;

	for ( DmAttributeReferenceIterator_t it = g_pDataModel->FirstAttributeReferencingElement( pDag->GetHandle() );
		it != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;
		it = g_pDataModel->NextAttributeReferencingElement( it ) )
	{
		CDmAttribute *pAttr = g_pDataModel->GetAttribute( it );
		Assert( pAttr );

		static const CUtlSymbolLarge symChildren = g_pDataModel->GetSymbol( "children" );
		if ( pAttr->GetNameSymbol() != symChildren )
			continue;

		CDmeDag* pParent = CastElement< CDmeDag >( pAttr->GetOwner() );
		if ( !pParent )
			continue;

		CDmeFilmClip *pFilmClip = FindFilmClipContainingDag( pParent );
		if ( pFilmClip )
			return pFilmClip;
	}

	return NULL;
}


static ConVar sfm_maxcamerastack( "sfm_maxcamerastack", "10", FCVAR_ARCHIVE, "Number of work cameras to store in camera stack." );

void CDmeFilmClip::LatchWorkCamera( CDmeCamera *pCamera )
{
	// Wipe everything after current stack ptr
	while ( ( m_CameraStack.Count() - 1 ) > m_nCurrentStackCamera )
	{
		m_CameraStack.Remove( m_CameraStack.Count() - 1 );
	}

	// Add to end
	CDmeCamera *newEntry = CreateElement< CDmeCamera >( "camerastack", GetFileId() );
	newEntry->FromCamera( pCamera );

	m_CameraStack.AddToTail( newEntry );

	m_nCurrentStackCamera = m_CameraStack.Count() - 1;

	int maxStack = clamp( sfm_maxcamerastack.GetInt(), 4, 100 );

	while ( m_CameraStack.Count() > maxStack )
	{
		m_CameraStack.Remove( 0 );
		--m_nCurrentStackCamera;
	}
}

void CDmeFilmClip::UpdateWorkCamera( CDmeCamera *pCamera )
{
	m_nCurrentStackCamera = clamp( m_nCurrentStackCamera, 0, m_CameraStack.Count() - 1 );
	if ( m_nCurrentStackCamera < 0 || m_nCurrentStackCamera >= m_CameraStack.Count() )
		return;

	m_CameraStack[ m_nCurrentStackCamera ]->FromCamera( pCamera );
}


void CDmeFilmClip::PreviousWorkCamera()
{
	m_nCurrentStackCamera -= 1;
}


void CDmeFilmClip::NextWorkCamera()
{
	m_nCurrentStackCamera += 1;
}

CDmeCamera *CDmeFilmClip::GetCurrentCameraStackEntry()
{
	m_nCurrentStackCamera = clamp( m_nCurrentStackCamera, 0, m_CameraStack.Count() - 1 );
	if ( m_nCurrentStackCamera < 0 || m_nCurrentStackCamera >= m_CameraStack.Count() )
		return NULL;

	CDmeCamera *entry = m_CameraStack[ m_nCurrentStackCamera ];
	return entry;
}

void CDmeFilmClip::PurgeCameraStack()
{
	m_nCurrentStackCamera = -1;
	while ( m_CameraStack.Count() > 0 )
	{
		m_CameraStack.Remove( 0 );
	}
}