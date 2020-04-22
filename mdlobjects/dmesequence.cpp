//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =====//
//
// Dme representation of QC: $sequence
//
//===========================================================================//


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects/dmeattributereference.h"
#include "movieobjects/dmeanimationlist.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeconnectionoperator.h"
#include "movieobjects/dmedag.h"
#include "mdlobjects/dmeanimcmd.h"
#include "mdlobjects/dmeik.h"
#include "mdlobjects/dmemotioncontrol.h"
#include "mdlobjects/dmesequence.h"
#include "mdlobjects/dmebonemask.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// CDmeAnimationEvent
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAnimationEvent, CDmeAnimationEvent );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAnimationEvent::OnConstruction()
{
	m_nFrame.Init( this, "frame" );
	m_sDataString.Init( this, "dataString" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAnimationEvent::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// CDmeSequenceActivity
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSequenceActivity, CDmeSequenceActivity );

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceActivity::OnConstruction()
{
	m_nWeight.InitAndSet( this, "weight", 1.0 );
	m_sModifierList.Init( this, "modifierList" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceActivity::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// CDmeSequenceBlendBase
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSequenceBlendBase, CDmeSequenceBlendBase );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceBlendBase::OnConstruction()
{
	m_sPoseParameterName.Init( this, "poseParameterName" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceBlendBase::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// CDmeSequenceBlend
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSequenceBlend, CDmeSequenceBlend );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceBlend::OnConstruction()
{
	m_flParamStart.Init( this, "paramStart" );
	m_flParamEnd.Init( this, "paramEnd" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceBlend::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// CDmeSequenceCalcBlend
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSequenceCalcBlend, CDmeSequenceCalcBlend );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceCalcBlend::OnConstruction()
{
	m_sAttachmentName.Init( this, "attachmentName" );
	m_eMotionControl.InitAndCreate( this, "motionControl" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceCalcBlend::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// CDmeSequenceLayerBase
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSequenceLayerBase, CDmeSequenceLayerBase );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceLayerBase::OnConstruction()
{
	m_eAnimation.Init( this, "animation" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceLayerBase::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// CDmeSequenceAddLayer
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSequenceAddLayer, CDmeSequenceAddLayer );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceAddLayer::OnConstruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceAddLayer::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// CDmeSequenceBlendLayer
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSequenceBlendLayer, CDmeSequenceBlendLayer );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceBlendLayer::OnConstruction()
{
	m_flStartFrame.Init( this, "startFrame" );
	m_flPeakFrame.Init( this, "peakFrame" );
	m_flTailFrame.Init( this, "tailFrame" );
	m_flEndFrame.Init( this, "endFrame" );
	m_bSpline.Init( this, "spline" );
	m_bCrossfade.Init( this, "crossfade" );
	m_bNoBlend.Init( this, "noBlend" );
	m_bLocal.Init( this, "local" );
	m_sPoseParameterName.Init( this, "poseParameterName" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceBlendLayer::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// CDmeSequenceBase
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSequenceBase, CDmeSequenceBase );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceBase::OnConstruction()
{
	m_eActivity.InitAndCreate( this, "activity" );
	m_bHidden.InitAndSet( this, "hidden", false );
	m_bDelta.InitAndSet( this, "delta", false );
	m_bWorldSpace.InitAndSet( this, "worldSpace", false );
	m_bPreDelta.InitAndSet( this, "preDelta", false );
	m_bAutoPlay.InitAndSet( this, "autoPlay", false );
	m_bRealtime.InitAndSet( this, "realtime", false );
	m_flFadeIn.InitAndSet( this, "fadein", 0.2f );
	m_flFadeOut.InitAndSet( this, "fadeout", 0.2f );
	m_sEntryNode.Init( this, "entryNode" );
	m_sExitNode.Init( this, "exitNode" );
	m_bReverseNodeTransition.Init( this, "reverseNodeTransition" );

	m_bSnap.Init( this, "snap" );
	m_bPost.Init( this, "post" );
	m_bLoop.Init( this, "loop" );

	m_eIkLockList.Init( this, "ikLockList", FATTRIB_NEVERCOPY );
	m_eAnimationEventList.Init( this, "animationEventList" );
	m_eLayerList.Init( this, "layerList" );
	m_sKeyValues.Init( this, "keyValues" );

	if ( m_eActivity.GetElement() )
	{
		if ( !Q_strcmp( "unnamed", m_eActivity.GetElement()->GetName() ) )
		{
			m_eActivity.GetElement()->SetName( "" );
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceBase::OnDestruction()
{
}

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
int CDmeSequenceBase::QSortFunction( const void *pVoidSeq1, const void *pVoidSeq2 )
{
	const CDmeSequenceBase *pBaseSeq1 = *( const CDmeSequenceBase ** )pVoidSeq1;
	const CDmeSequenceBase *pBaseSeq2 = *( const CDmeSequenceBase ** )pVoidSeq2;

	if ( !pBaseSeq1 || !pBaseSeq2 )
		return 0;

	const CDmeSequence* pSeq1 = CastElement< CDmeSequence >( pBaseSeq1 );	// NULL if MultiSequence
	const CDmeSequence* pSeq2 = CastElement< CDmeSequence >( pBaseSeq2 );	// NULL if MultiSequence

	if ( pSeq1 && !pSeq2 )	// 1 Seq, 2 Multi, 1 < 2
		return -1;

	if ( !pSeq1 && pSeq2 )	// 1 Multi, 2 Seq, 1 > 2
		return 1;

	if ( !pSeq1 && !pSeq2 )	// Both Multi, 1 == 2, Multi can't refer to other multi
		return 0;

	// Both Seq, check for references
	bool bRef[2] = { false, false };
	const CDmeSequence *const pSeq[2] = { pSeq1, pSeq2 };

	for ( int i = 0; i < 2; ++i )
	{
		for ( int j = 0; j < pSeq[i]->m_eAnimationCommandList.Count(); ++j )
		{
			const CDmeAnimCmd *pAnimCmd = pSeq[i]->m_eAnimationCommandList[j];
			const CDmeAnimCmdSubtract *pAnimCmdSubtract = CastElement< CDmeAnimCmdSubtract >( pAnimCmd );
			if ( pAnimCmdSubtract && pAnimCmdSubtract->m_eAnimation.GetHandle() == pSeq[ (i + 1) % 2 ]->GetHandle() )
			{
				bRef[i] = true;
				break;
			}
			else
			{
				const CDmeAnimCmdAlign *pAnimCmdAlign = CastElement< CDmeAnimCmdAlign >( pAnimCmd );
				if ( pAnimCmdAlign && pAnimCmdAlign->m_eAnimation.GetHandle() == pSeq[ ( i + 1 ) % 2 ]->GetHandle() )
				{
					bRef[i] = true;
					break;
				}
			}
		}
	}

	if ( bRef[0] && !bRef[1] )	// 1 references 2, so 1 > 2
		return 1;

	if ( !bRef[0] && bRef[1] )	// 1 references by 2, so 1 < 2
		return -1;

	if ( bRef[0] && bRef[1] )
	{
		Error( "Animation %s & %s reference each other, circular references are not allowed\n", pSeq1->GetName(), pSeq2->GetName() );
		return 0;
	}

	if ( pSeq1->m_eAnimationCommandList.Count() < pSeq2->m_eAnimationCommandList.Count() )
	{
		return -1;
	}
	else if ( pSeq1->m_eAnimationCommandList.Count() > pSeq2->m_eAnimationCommandList.Count() )
	{
		return 1;
	}

	return 0;
}


//-----------------------------------------------------------------------------
// CDmeSequence
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSequence, CDmeSequence );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequence::OnConstruction()
{
	m_eSkeleton.Init( this, "skeleton" );
	m_eAnimationList.Init( this, "animationList" );

	m_flFPS.InitAndSet( this, "fps", 30.0f );
	m_vOrigin.InitAndSet( this, "origin", Vector( 0.0f, 0.0f, 0.0f ) );
	m_flScale.InitAndSet( this, "scale", 1.0f );
	m_nStartLoop.Init( this, "startLoop" );

	m_bForceLoop.Init( this, "forceLoop" );
	m_bAutoIk.Init( this, "autoIk" );
	m_flMotionRollback.InitAndSet( this, "motionRollback", 0.3f );
	m_bAnimBlocks.InitAndSet( this, "animBlocks", true );
	m_bAnimBlockStall.InitAndSet( this, "animBlockStall", true );

	m_eMotionControl.InitAndCreate( this, "motionControl" );

	m_eAnimationCommandList.Init( this, "animationCommandList" );
	m_eIkRuleList.Init( this, "ikRuleList", FATTRIB_NEVERCOPY );

	m_eBoneMask.Init( this, "boneMask" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequence::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeChannelsClip *CDmeSequence::GetDmeChannelsClip() const
{
	CDmeAnimationList *pDmeAnimationList = m_eAnimationList.GetElement();
	if ( !pDmeAnimationList )
		return NULL;

	for ( int i = 0; i < pDmeAnimationList->GetAnimationCount(); ++i )
	{
		CDmeChannelsClip *pDmeChannelsClip = pDmeAnimationList->GetAnimation( i );
		if ( pDmeChannelsClip )
			return pDmeChannelsClip;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
DmeFramerate_t CDmeSequence::GetFrameRate(
	DmeFramerate_t fallbackFrameRate /* = 30 */,
	bool bForceFallback /* = false */ ) const
{
	CDmeChannelsClip *pDmeChannelsClip = GetDmeChannelsClip();
	if ( !pDmeChannelsClip )
		return fallbackFrameRate;

	DmeFramerate_t dmeFrameRate = fallbackFrameRate;
	if ( !bForceFallback && pDmeChannelsClip->HasAttribute( "frameRate" ) )
	{
		const int nFrameRate = pDmeChannelsClip->GetValue< int >( "frameRate", 0 );
		if ( nFrameRate >= 0 )
		{
			dmeFrameRate = DmeFramerate_t( nFrameRate );
		}
	}

	return dmeFrameRate;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmeSequence::GetFrameCount(
	DmeFramerate_t fallbackFrameRate /* = 30 */,
	bool bForceFallback /* = false */ ) const
{
	CDmeChannelsClip *pDmeChannelsClip = GetDmeChannelsClip();
	if ( !pDmeChannelsClip )
		return 0;

	const DmeFramerate_t dmeFrameRate = GetFrameRate( fallbackFrameRate, bForceFallback );

	const DmeTime_t nStartTime = pDmeChannelsClip->GetStartTime();
	const int nStartFrame = FrameForTime( nStartTime, dmeFrameRate );

	const DmeTime_t nEndTime = pDmeChannelsClip->GetEndTime();
	const int nEndFrame = FrameForTime( nEndTime, dmeFrameRate );

	return nEndFrame - nStartFrame + 1;
}


//-----------------------------------------------------------------------------
// Does a search through connection operators for dependent DmeOperators
//-----------------------------------------------------------------------------
void CDmeSequence::GetDependentOperators( CUtlVector< IDmeOperator * > &operatorList, CDmeOperator *pDmeOperator ) const
{
	if ( !pDmeOperator || !CastElement< CDmeOperator >( pDmeOperator ) )
		return;

	// Abort if the specified operator is already in the operatorList
	for ( int i = 0; i < operatorList.Count(); ++i )
	{
		CDmeOperator *pTmpDmeOperator = CastElement< CDmeOperator >( reinterpret_cast< CDmeOperator * >( operatorList[i] ) );
		if ( pTmpDmeOperator && pTmpDmeOperator == pDmeOperator )
			return;
	}

	operatorList.AddToTail( pDmeOperator );

	CUtlVector< CDmAttribute * > outAttrList;
	pDmeOperator->GetOutputAttributes( outAttrList );

	for ( int i = 0; i < outAttrList.Count(); ++i )
	{
		CDmElement *pDmElement = outAttrList[i]->GetOwner();
		if ( !pDmElement )
			continue;

		if ( pDmElement != pDmeOperator )
		{
			CUtlVector< CDmElement * > reList0;
			FindReferringElements( reList0, pDmElement, g_pDataModel->GetSymbol( "element" ), false );
			for ( int j = 0; j < reList0.Count(); ++j )
			{
				CDmeAttributeReference *pRe0 = CastElement< CDmeAttributeReference >( reList0[j] );
				if ( !pRe0 || pRe0->GetReferencedAttribute() != outAttrList[i] )
					continue;

				CUtlVector< CDmElement * > reList1;
				FindReferringElements( reList1, pRe0, g_pDataModel->GetSymbol( "input" ), false );
				for ( int k = 0; k < reList1.Count(); ++k )
				{
					CDmeConnectionOperator *pRe1 = CastElement< CDmeConnectionOperator >( reList1[k] );
					if ( !pRe1 )
						continue;

					GetDependentOperators( operatorList, pRe1 );
				}
			}
		}

		GetDependentOperators( operatorList, CastElement< CDmeOperator >( pDmElement ) );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequence::PrepareChannels( CUtlVector< IDmeOperator * > &dmeOperatorList )
{
	dmeOperatorList.RemoveAll();

	CDmeAnimationList *pDmeAnimationList = m_eAnimationList.GetElement();
	if ( !pDmeAnimationList )
		return;

	for ( int i = 0; i < pDmeAnimationList->GetAnimationCount(); ++i )
	{
		CDmeChannelsClip *pDmeChannelsClip = pDmeAnimationList->GetAnimation( i );
		if ( !pDmeChannelsClip )
			continue;

		for ( int j = 0; j < pDmeChannelsClip->m_Channels.Count(); ++j )
		{
			CDmeChannel *pDmeChannel = pDmeChannelsClip->m_Channels[j];
			if ( !pDmeChannel )
				continue;

			pDmeChannel->SetMode( CM_PLAY );
			GetDependentOperators( dmeOperatorList, pDmeChannel );
		}
	}
}


//-----------------------------------------------------------------------------
// Update channels so they are in position for the next frame
//-----------------------------------------------------------------------------
void CDmeSequence::UpdateChannels( CUtlVector< IDmeOperator * > &dmeOperatorList, DmeTime_t nClipTime )
{
	CDmeAnimationList *pDmeAnimationList = m_eAnimationList.GetElement();
	if ( !pDmeAnimationList )
		return;

	for ( int i = 0; i < pDmeAnimationList->GetAnimationCount(); ++i )
	{
		CDmeChannelsClip *pDmeChannelsClip = pDmeAnimationList->GetAnimation( i );
		if ( !pDmeChannelsClip )
			continue;

		const DmeTime_t channelTime = pDmeChannelsClip->ToChildMediaTime( nClipTime );

		const int nChannelsCount = pDmeChannelsClip->m_Channels.Count();
		for ( int j = 0; j < nChannelsCount; ++j )
		{
			pDmeChannelsClip->m_Channels[j]->SetCurrentTime( channelTime );
		}
	}

	// Recompute the position of the joints
	{
		CDisableUndoScopeGuard guard;
		g_pDmElementFramework->SetOperators( dmeOperatorList );
		g_pDmElementFramework->Operate( true );
	}

	g_pDmElementFramework->BeginEdit();
}


//-----------------------------------------------------------------------------
// CDmeMultiSequence
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeMultiSequence, CDmeMultiSequence );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMultiSequence::OnConstruction()
{
	m_nBlendWidth.InitAndSet( this, "blendWidth", 0 );
	m_eBlendRef.Init( this, "blendRef", FATTRIB_NEVERCOPY );
	m_eBlendComp.Init( this, "blendComp", FATTRIB_NEVERCOPY );
	m_eBlendCenter.Init( this, "blendCenter", FATTRIB_NEVERCOPY );
	m_eSequenceList.Init( this, "sequenceList", FATTRIB_NEVERCOPY );
	m_eBlendList.Init( this, "blendList" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMultiSequence::OnDestruction()
{
}