//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =====//
//
// Dme representation of QC: $sequence
//
//===========================================================================//

#ifndef DMESEQUENCE_H
#define DMESEQUENCE_H


#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "datamodel/dmattributevar.h"
#include "datamodel/dmelement.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeDag;
class CDmeAnimationList;
class CDmeIkRule;
class CDmeIkLock;
class CDmeAnimCmd;
class CDmeEvent;
class CDmeMotionControl;
class CDmeSequenceBase;
class CDmeChannelsClip;
class CDmeBoneMask;


//-----------------------------------------------------------------------------
// Animation event
//-----------------------------------------------------------------------------
class CDmeAnimationEvent : public CDmElement
{
	DEFINE_ELEMENT( CDmeAnimationEvent, CDmElement );

public:
	// Name is the event name
	CDmaVar< int > m_nFrame;
	CDmaString m_sDataString;
};


//-----------------------------------------------------------------------------
//
// QC $sequence activity <name> <weight>
//
// .name = <name>
// .weight = <weight>
// .modifierList = $QC activityModifier
//-----------------------------------------------------------------------------
#ifdef SWIG
%ignore CDmeSequenceActivity::m_sModifierList;
#endif // #ifdef SWIG

class CDmeSequenceActivity : public CDmElement
{
	DEFINE_ELEMENT( CDmeSequenceActivity, CDmElement );

public:
	CDmaVar< int > m_nWeight;
	CDmaStringArray m_sModifierList;
};


//-----------------------------------------------------------------------------
// CDmeSequenceLayerBase
//-----------------------------------------------------------------------------
class CDmeSequenceLayerBase : public CDmElement
{
	DEFINE_ELEMENT( CDmeSequenceLayerBase, CDmElement );

public:
	CDmaElement< CDmeSequenceBase > m_eAnimation;

};


//-----------------------------------------------------------------------------
// CDmeSequenceAddLayer
//-----------------------------------------------------------------------------
class CDmeSequenceAddLayer : public CDmeSequenceLayerBase
{
	DEFINE_ELEMENT( CDmeSequenceAddLayer, CDmeSequenceLayerBase );

public:
};


//-----------------------------------------------------------------------------
// CDmeSequenceBlendLayer
//-----------------------------------------------------------------------------
class CDmeSequenceBlendLayer : public CDmeSequenceLayerBase
{
	DEFINE_ELEMENT( CDmeSequenceBlendLayer, CDmeSequenceLayerBase );

public:
	CDmaVar< float > m_flStartFrame;
	CDmaVar< float > m_flPeakFrame;
	CDmaVar< float > m_flTailFrame;
	CDmaVar< float > m_flEndFrame;
	CDmaVar< bool > m_bSpline;
	CDmaVar< bool > m_bCrossfade;
	CDmaVar< bool > m_bNoBlend;
	CDmaVar< bool > m_bLocal;
	CDmaString m_sPoseParameterName;
};


//-----------------------------------------------------------------------------
// QC seq blend/calcblend base class
//-----------------------------------------------------------------------------
class CDmeSequenceBlendBase : public CDmElement
{
	DEFINE_ELEMENT( CDmeSequenceBlendBase, CDmElement );

public:
	CDmaString m_sPoseParameterName;

};


//-----------------------------------------------------------------------------
// QC seq blend
//-----------------------------------------------------------------------------
class CDmeSequenceBlend : public CDmeSequenceBlendBase
{
	DEFINE_ELEMENT( CDmeSequenceBlend, CDmeSequenceBlendBase );

public:
	CDmaVar< float > m_flParamStart;
	CDmaVar< float > m_flParamEnd;
};


//-----------------------------------------------------------------------------
// QC seq calcblend
//-----------------------------------------------------------------------------
class CDmeSequenceCalcBlend : public CDmeSequenceBlendBase
{
	DEFINE_ELEMENT( CDmeSequenceCalcBlend, CDmeSequenceBlendBase );

public:
	CDmaString m_sAttachmentName;
	CDmaElement< CDmeMotionControl > m_eMotionControl;	
};


//-----------------------------------------------------------------------------
// CDmeSequenceBase - Base Class For CDmeSequence & CDmeMultiSequence
//-----------------------------------------------------------------------------
class CDmeSequenceBase : public CDmElement
{
	DEFINE_ELEMENT( CDmeSequenceBase, CDmElement );

public:
	CDmaElement< CDmeSequenceActivity > m_eActivity;	// QC activity
	CDmaVar< bool > m_bHidden;							// QC hidden
	CDmaVar< bool > m_bDelta;							// QC delta
	CDmaVar< bool > m_bWorldSpace;						// QC worldspace
	CDmaVar< bool > m_bPreDelta;						// QC predelta
	CDmaVar< bool > m_bAutoPlay;						// QC autoplay
	CDmaVar< bool > m_bRealtime;						// QC realtime
	CDmaVar< float > m_flFadeIn;						// QC fadein
	CDmaVar< float > m_flFadeOut;						// QC fadeout
	CDmaString m_sEntryNode;							// QC node, transition, rtransition
	CDmaString m_sExitNode;								// QC node, transition, rtransition
	CDmaVar< bool > m_bReverseNodeTransition;			// QC rtransition

	CDmaVar< bool > m_bSnap;							// QC snap - Both Sequence & Animation
	CDmaVar< bool > m_bPost;							// QC post - Both Sequence & Animation
	CDmaVar< bool > m_bLoop;							// QC loop - Both Sequence & Animation

	CDmaElementArray< CDmeIkLock > m_eIkLockList;
	CDmaElementArray< CDmeAnimationEvent > m_eAnimationEventList;
	CDmaElementArray< CDmeSequenceLayerBase > m_eLayerList;
	CDmaString m_sKeyValues;							// Sequence KeyValues

	//-----------------------------------------------------------------------------
	// qsort function for sorting DmeBaseSequence elements based on type and
	// sequence references.
	//
	// * A DmeBaseSequence must be either a DmeSequence or a DmeMultiSequence
	//   They are mutually exclusive, cannot be both
	// * DmeMultiSequence refer to DmeSequence's so should always go last
	// * DmeMultiSequence cannot refer to other DmeMultiSequence so they are
	//   considered equal
	// * DmeSequence can refer to other DmeSequence elements via DmeAnimCmd's
	//   but circular references are not allowed.  If a DmeSequence refers
	//   to another, the DmeSequence being referenced needs to be before the
	//   DmeSequence doing the referring
	// * If no referrals between two DmeSequence's, sort based on DmeAnimCmd count
	//   so DmeSequence's with fewer DmeAnimCmd's go first
	//-----------------------------------------------------------------------------
	static int QSortFunction( const void *pVoidSeq1, const void *pVoidSeq2 );
};


//-----------------------------------------------------------------------------
// Animation data
//
// QC $sequence
//
//-----------------------------------------------------------------------------
class CDmeSequence : public CDmeSequenceBase
{
	DEFINE_ELEMENT( CDmeSequence, CDmeSequenceBase );

public:
	CDmaElement< CDmeDag > m_eSkeleton;
	CDmaElement< CDmeAnimationList > m_eAnimationList;		// QC animation file

	CDmaVar< float > m_flFPS;								// QC fps
	CDmaVar< Vector > m_vOrigin;							// QC origin
	CDmaVar< float > m_flScale;								// QC scale
	CDmaVar< int > m_nStartLoop;							// QC startloop
	CDmaVar< bool > m_bForceLoop;							// QC !( noforceloop )
	CDmaVar< bool > m_bAutoIk;								// QC autoik / noautoik
	CDmaVar< float > m_flMotionRollback;					// QC motionrollback
	CDmaVar< bool > m_bAnimBlocks;							// QC !( noanimblock )
	CDmaVar< bool > m_bAnimBlockStall;						// QC !( noanimblockstalls )
	CDmaElement< CDmeMotionControl > m_eMotionControl;		// QC STUDIO_X, etc...

	CDmaElementArray< CDmeAnimCmd > m_eAnimationCommandList;
	CDmaElementArray< CDmeIkRule > m_eIkRuleList;
	CDmaElement< CDmeBoneMask > m_eBoneMask;

	CDmeChannelsClip *GetDmeChannelsClip() const;

	DmeFramerate_t GetFrameRate(
		DmeFramerate_t fallbackFrameRate = DmeFramerate_t( 30 ),
		bool bForceFallback = false ) const;

	// Gets the maximum frame count from all animations in the DmeSequence
	int GetFrameCount(
		DmeFramerate_t fallbackFrameRate = DmeFramerate_t( 30 ),
		bool bForceFallback = false ) const;

	// Put all DmeChannel's in this DmeSequence to CM_PLAY
	void PrepareChannels( CUtlVector< IDmeOperator * > &dmeOperatorList );

	// Operate all DmeChannel's in this DmeSequence
	// Pass the dmeOperatorList returned by PrepareChannels
	void UpdateChannels( CUtlVector< IDmeOperator * > &dmeOperatorList, DmeTime_t nClipTime );

	void GetDependentOperators( CUtlVector< IDmeOperator * > &operatorList, CDmeOperator *pDmeOperator ) const;
};


//-----------------------------------------------------------------------------
// Animation data
//
// QC $sequence
//
//-----------------------------------------------------------------------------
class CDmeMultiSequence : public CDmeSequenceBase
{
	DEFINE_ELEMENT( CDmeMultiSequence, CDmeSequenceBase );

public:

	CDmaVar< int > m_nBlendWidth;							// QC blendwidth
	CDmaElement< CDmeSequence > m_eBlendRef;				// QC blendref
	CDmaElement< CDmeSequence > m_eBlendComp;				// QC blendcomp
	CDmaElement< CDmeSequence > m_eBlendCenter;				// QC blendcenter
	CDmaElementArray< CDmeSequence > m_eSequenceList;
	CDmaElementArray< CDmeSequenceBlendBase > m_eBlendList;
};


#endif // DMESEQUENCE_H
