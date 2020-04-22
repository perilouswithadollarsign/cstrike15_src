//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMETRACK_H
#define DMETRACK_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlflags.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmehandle.h"
#include "movieobjects/dmeclip.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeClip;
enum DmeClipType_t;


//-----------------------------------------------------------------------------
// Default track name
//-----------------------------------------------------------------------------
#define DMETRACK_DEFAULT_NAME	"default"


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
class CDmeTrack : public CDmElement
{
	DEFINE_ELEMENT( CDmeTrack, CDmElement );

public:
	// Methods of IDmElement
	virtual void	OnAttributeChanged( CDmAttribute *pAttribute );

	void			SetCollapsed( bool state );
	bool			IsCollapsed() const;
	
	void			SetVolume( float state );
	float			GetVolume() const;


	void			SetMute( bool state );
	bool			IsMute( bool bCheckSoloing = true ) const;

	// Is this track synched to the film track?
	void			SetSynched( bool bState );
	bool			IsSynched() const;

	int				GetClipCount() const;
	CDmeClip		*GetClip( int i ) const;
	const CUtlVector< DmElementHandle_t > &GetClips( ) const;

	void			AddClip( CDmeClip *clip );
	bool			RemoveClip( CDmeClip *clip );
	void			RemoveClip( int i );
	void			RemoveAllClips();
	int				FindClip( CDmeClip *clip );
	CDmeClip		*FindNamedClip( const char *name );

	DmeClipType_t	GetClipType() const;
	void			SetClipType( DmeClipType_t type );

	// Find clips at, intersecting or within a particular time interval
	void			FindClipsAtTime( DmeTime_t time, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const;
	void			FindClipsIntersectingTime( DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const;
	void			FindClipsWithinTime( DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const;

	// Methods to shift clips around
	// These methods shift clips that straddle the start/end time (NOTE: time is measured in local time)
	// NOTE: bTestStartingTime true means if the starting time is after the start time, then shift
	// Setting it to false means if the clip intersects the time at all, then shift
	void			ShiftAllClipsAfter ( DmeTime_t startTime, DmeTime_t dt, bool bTestStartingTime = true );
	void			ShiftAllClipsBefore( DmeTime_t endTime,   DmeTime_t dt, bool bTestEndingTime   = true );
	void			ShiftAllClips( DmeTime_t dt );

	// A version that works only on film clips
	void			ShiftAllFilmClipsAfter ( CDmeClip *pClip, DmeTime_t dt, bool bShiftClip = false, bool bSortClips = true );
	void			ShiftAllFilmClipsBefore( CDmeClip *pClip, DmeTime_t dt, bool bShiftClip = false, bool bSortClips = true );

	// Sorts all children so they ascend in time
	void			SortClipsByStartTime( );

	// Shifts all clips to be non-overlapping
	void			FixOverlaps();

	// Can this track contain clips that overlap in time?
	// NOTE: Non-overlapping clips will be 
	bool			IsNonoverlapping() const;

	// Is this a film track?
	bool			IsFilmTrack() const;

	// Returns the next/previous clip in a film track
	CDmeClip*		FindPrevFilmClip( CDmeClip *pClip );
	CDmeClip*		FindNextFilmClip( CDmeClip *pClip );
	void			FindAdjacentFilmClips( CDmeClip *pClip, CDmeClip *&pPrevClip, CDmeClip *&pNextClip );
	void			FindAdjacentFilmClips( DmeTime_t localTime, CDmeClip *&pPrevClip, CDmeClip *&pNextClip );

	// Finds a clip at a particular time
	CDmeClip*		FindFilmClipAtTime( DmeTime_t localTime );

	// Find first clip that intersects a specific time range
	CDmeClip*		FindFirstFilmClipIntesectingTime( DmeTime_t localStartTime, DmeTime_t localEndTime );

	// Inserts space in a film track for a film clip
	void			InsertSpaceInFilmTrack( DmeTime_t localStartTime, DmeTime_t localEndTime );

	// Singleton solo track	(of the same clip type that this track is)
	CDmeTrack		*GetSoloTrack( ) const;
	void			SetSoloTrack( );
	bool			IsSoloTrack() const;
	static CDmeTrack *GetSoloTrack( DmeClipType_t clipType );
	static void		SetSoloTrack( DmeClipType_t clipType, CDmeTrack *pTrack );

	// Fills all gaps in a film track with slugs
	void			FillAllGapsWithSlugs( const char *pSlugName, DmeTime_t startTime, DmeTime_t endTime );

	// Track vertical display scale
	void SetDisplayScale( float flDisplayScale );
	float GetDisplayScale() const;

private:
	class CSuppressAutoFixup
	{
	public:
		CSuppressAutoFixup( CDmeTrack *pTrack, int nFlags ) : m_pTrack( pTrack ), m_nFlags( nFlags )
		{
			m_pTrack->m_Flags.SetFlag( m_nFlags ); 
		}

		~CSuppressAutoFixup()
		{
			m_pTrack->m_Flags.ClearFlag( m_nFlags ); 
		}

	private:
		CDmeTrack *m_pTrack;
		int m_nFlags;
	};

	enum
	{
		IS_SORTED = 0x1,
		SUPPRESS_OVERLAP_FIXUP = 0x2,
		SUPPRESS_DIRTY_ORDERING = 0x4,
	};

	CDmaElementArray< CDmeClip >	m_Clips;

	CDmaVar< float > m_Volume;
	CDmaVar< bool > m_Collapsed;
	CDmaVar< bool >	m_Mute;
	CDmaVar< bool >	m_Synched;
	CDmaVar< int >	m_ClipType;

	CDmaVar< float > m_flDisplayScale;

	CUtlFlags< unsigned char > m_Flags;
	DmElementHandle_t m_hOwner;

	static DmElementHandle_t m_hSoloTrack[DMECLIP_TYPE_COUNT];

	friend class CSuppressAutoFixup;
};


//-----------------------------------------------------------------------------
// Indicates whether tracks contain clips that are non-overlapping in time
//-----------------------------------------------------------------------------
inline bool CDmeTrack::IsNonoverlapping() const
{
	return m_ClipType == DMECLIP_FILM;
}


//-----------------------------------------------------------------------------
// Is this a film track?
//-----------------------------------------------------------------------------
inline bool CDmeTrack::IsFilmTrack() const
{
	return m_ClipType == DMECLIP_FILM;
}

//-----------------------------------------------------------------------------
// Track vertical display scale
//-----------------------------------------------------------------------------
inline void CDmeTrack::SetDisplayScale( float flDisplayScale )
{
	m_flDisplayScale = flDisplayScale;
}

inline float CDmeTrack::GetDisplayScale() const
{
	return m_flDisplayScale;
}

//-----------------------------------------------------------------------------
// helper methods
//-----------------------------------------------------------------------------
CDmeTrackGroup *GetParentTrackGroup( CDmeTrack *pTrack );

#endif // DMETRACK_H
