//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. =====
//
// Purpose:
//
//============================================================================


#ifndef MDLOBJECTS_UTILS_H
#define MDLOBJECTS_UTILS_H


#if defined( _WIN32 )
#pragma once
#endif


// Valve includes
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmemodel.h"
#include "mdlobjects/dmeasset.h"
#include "mdlobjects/dmesequence.h"
#include "tier1/utlvector.h"
#include "tier1/utlstring.h"


//-----------------------------------------------------------------------------
// Ok to pass NULL
//-----------------------------------------------------------------------------
void ReorientMppFile( CDmElement *pDmElementRoot, bool bMakeZUp );
void MppReorient( CDmElement *pDmElementRoot, bool bMakeZUp );

void GetAbsMotion( CDmeChannel **ppDmePChannel, CDmeChannel **ppDmeOChannel, CDmeDag *pDmeDag );
bool SetAbsMotion( CDmeDag *pDmeDag, CDmeChannel *pDmePositionChannel, CDmeChannel *pDmeOrientationChannel );

// Creates a guaranteed unique DmFileId_t
DmFileId_t CreateUniqueDmFileId();

//-----------------------------------------------------------------------------
// Iterates over all CDmeSequence's in the MPP File (Not CDmeMultiSequence)
// Get() only returns NULL if IsDone() also returns true so this usage is safe
//
// for ( MppSequenceIt sIt( pDmeAssetRoot ); !sIt.IsDone(); sIt.Next() )
// {
//    sIt.Get()->SomeFunctionOnCDmeSequence()
// }
//-----------------------------------------------------------------------------
class MppSequenceIt
{
public:
	MppSequenceIt( CDmeAssetRoot *pDmeAssetRoot );
	CDmeSequence *Get() const;
	bool IsDone() const;
	void Next();
	void Reset();

protected:
	CUtlVector< DmElementHandle_t > m_hDmeSequenceList;
	int m_nSequenceIndex;
};

// Masks for MppGetSkeletonLIst
enum MppSkeletonMask_t
{
	MPP_ANIM_SKELETON_MASK =	1 << 0,
	MPP_PHYSICS_SKELETON_MASK = 1 << 1,
	MPP_MODEL_SKELETON_MASK =	1 << 2,
	MPP_ALL_SKELETON_MASK = MPP_ANIM_SKELETON_MASK | MPP_PHYSICS_SKELETON_MASK | MPP_MODEL_SKELETON_MASK
};

// Returns a list of all unique skeletons under the specified MPP DmeAssetRoot
void MppGetSkeletonList( CUtlVector< CDmeModel * > &skeletonList, CDmeAssetRoot *pDmeAssetRoot, int nMppSkeletonMask = MPP_ALL_SKELETON_MASK );

// Connects all of the non-animation skeletons to each animation skeleton via DmeConnectionOperators
DmFileId_t MppConnectSkeletonsForAnimation( CDmeAssetRoot *pDmeAssetRoot );

// Disconnects all of the non-animation skeletons from each animation skeleton
// and destroys the elements created by MppConnectSkeletonsForAnimation
void MppDisconnectSkeletonsFromAnimation( CDmeAssetRoot *pDmeAssetRoot );

// Utility to return DmElement id as name:string
CUtlString ComputeDmElementIdStr( const CDmElement *pDmElement );

#endif // MDLOBJECTS_UTILS_H