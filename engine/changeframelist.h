//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CHANGEFRAMELIST_H
#define CHANGEFRAMELIST_H
#ifdef _WIN32
#pragma once
#endif

#include "mempool.h"
#include "dt_common.h"

// This class holds the last tick (from host_tickcount) that each property in 
// a datatable changed at.
//
// It provides fast access to a list of properties that changed within a certain frame range.
//
// These are created once per entity per frame. Since usually a very small percentage of an
// entity's properties actually change each frame, this allows you to get a small set of 
// properties to delta for each client.
class CChangeFrameList
{
public:

	//properties are bucketed into batches to keep the iteration through change frames fast. This
	//allows for a time stamp to be checked on a bucket, and thereby skip the entire bucket
	static const uint32 knBucketSize = 32;

	CChangeFrameList( int nProperties, int iCurTick );
	~CChangeFrameList();

	CChangeFrameList( const CChangeFrameList &rhs );

	// Call this to delete the object.
	void Release();

	// This just returns the value you passed into AllocChangeFrameList().
	int GetNumProps() const											{ return m_nNumProps; }
	int GetPropTick( int nProp ) const								{ return m_ChangeTicks[ nProp ]; }

	// Sets the change frames for the specified properties to iFrame.
	void SetChangeTick( const int* RESTRICT pProps, int nNumProps, const int iTick );
	void SetChangeTick( const int nProp, const int iTick )			{ m_ChangeTicks[ nProp ] = iTick; m_ChangeTicks[ m_nNumProps + nProp / knBucketSize ] = iTick; }
	
	//given a bucket index, this will determine the lastest tick on that bucket
	const int	GetNumBuckets() const								{ return m_ChangeTicks.Count() - m_nNumProps; }
	const int*	GetBucketChangeTicks() const						{ return m_ChangeTicks.Base() + m_nNumProps; }
	const int*	GetChangeTicks() const								{ return m_ChangeTicks.Base(); }

	// Given a tick and a property index, this will determine if it was changed after that time
	bool DidPropChangeAfterTick( int iTick, int nProp ) const		{ return m_ChangeTicks[ nProp ] > iTick; }

	CChangeFrameList* Copy(); // return a copy of itself

private:
	// Change frames for each property. The buckets for the property set are appended onto the end.
	CUtlVector< int >	m_ChangeTicks;
	int					m_nNumProps;

	CChangeFrameList &operator=( const CChangeFrameList &rhs );

	DECLARE_FIXEDSIZE_ALLOCATOR_MT( CChangeFrameList );
};				
#endif // CHANGEFRAMELIST_H
