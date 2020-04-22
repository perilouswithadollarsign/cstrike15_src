//====== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. =====
//
// Purpose:
//
//=============================================================================


// Valve includes
#include "tier1/utlstack.h"
#include "movieobjects/dmeanimationlist.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeconnectionoperator.h"
#include "movieobjects/dmelog.h"
#include "mdlobjects/dmebbox.h"
#include "mdlobjects/dmebodygrouplist.h"
#include "mdlobjects/dmebodygroup.h"
#include "mdlobjects/dmecollisionmodel.h"
#include "mdlobjects/dmeeyeball.h"
#include "mdlobjects/dmeeyeballglobals.h"
#include "mdlobjects/dmeik.h"
#include "mdlobjects/dmelodlist.h"
#include "mdlobjects/dmelod.h"
#include "mdlobjects/dmesequencelist.h"
#include "mdlobjects/dmesequence.h"
#include "mdlobjects/dmedefinebonelist.h"
#include "mdlobjects/dmedefinebone.h"
#include "mdlobjects/mpp_utils.h"


//-----------------------------------------------------------------------------
// Returns the matrix & quaternion to reorient
//-----------------------------------------------------------------------------
void GetReorientData( matrix3x4_t &m, Quaternion &q, bool bMakeZUp )
{
	if ( bMakeZUp )
	{
		static const matrix3x4_t mYtoZ(
			1.0f,  0.0f,  0.0f, 0.0f,
			0.0f,  0.0f, -1.0f, 0.0f,
			0.0f,  1.0f,  0.0f, 0.0f );

		m = mYtoZ;
	}
	else
	{
		static const matrix3x4_t mZtoY(
			1.0f,  0.0f,  0.0f, 0.0f,
			0.0f,  0.0f,  1.0f, 0.0f,
			0.0f, -1.0f,  0.0f, 0.0f );

		m = mZtoY;
	}

	MatrixQuaternion( m, q );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void ReorientDmeAnimation( CDmeDag *pDmeDag, const matrix3x4_t &mOrient, const Quaternion &qOrient )
{
	if ( !pDmeDag )
		return;

	CUtlVector< CDmeChannel * > dmeChannelList;

	if ( !FindReferringElements( dmeChannelList, pDmeDag->GetTransform(), g_pDataModel->GetSymbol( "toElement" ) ) || dmeChannelList.Count() < 0 )
		return;

	const int nDmeChannelCount = dmeChannelList.Count();
	for ( int i = 0; i < nDmeChannelCount; ++i )
	{
		CDmeChannel *pDmeChannel = dmeChannelList[ i ];
		if ( !pDmeChannel )
			continue;

		CDmeLog *pDmeLog = pDmeChannel->GetLog();
		if ( !pDmeLog )
			continue;

		const int nLogLayerCount = pDmeLog->GetNumLayers();
		for ( int j = 0; j < nLogLayerCount; ++j )
		{
			CDmeLogLayer *pDmeLogLayer = pDmeLog->GetLayer( j );

			CDmeVector3LogLayer *pDmeVector3LogLayer = CastElement< CDmeVector3LogLayer >( pDmeLogLayer );
			if ( pDmeVector3LogLayer  )
			{
				RotatePositionLog( pDmeVector3LogLayer, mOrient );
				continue;
			}

			CDmeQuaternionLogLayer *pDmeQuaternionLogLayer = CastElement< CDmeQuaternionLogLayer >( pDmeLogLayer );
			if ( pDmeQuaternionLogLayer  )
			{
				RotateOrientationLog( pDmeQuaternionLogLayer, mOrient, true );
				continue;
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void ReorientDmeTransform( CDmeTransform *pDmeTransform, const matrix3x4_t &mOrient, const Quaternion &qOrient )
{
	if ( !pDmeTransform )
		return;

	Vector vTmp;
	VectorRotate( pDmeTransform->GetPosition(), mOrient, vTmp );
	pDmeTransform->SetPosition( vTmp );

	Quaternion qTmp;
	QuaternionMult( qOrient, pDmeTransform->GetOrientation(), qTmp );
	pDmeTransform->SetOrientation( qTmp );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void ReorientDmeDefineBone( CDmeDefineBone *pDmeDefineBone, const matrix3x4_t &mOrient, const Quaternion &qOrient )
{
	if ( !pDmeDefineBone )
		return;

	Vector vTmp;
	VectorRotate( pDmeDefineBone->m_Translation, mOrient, vTmp );
	pDmeDefineBone->m_Translation = vTmp;

	Quaternion qTmp0;
	Quaternion qTmp1;
	QAngle aTmp;
	AngleQuaternion( pDmeDefineBone->m_Rotation, qTmp0 );
	QuaternionMult( qOrient, qTmp0, qTmp1 );
	QuaternionAngles( qTmp1, aTmp );
	pDmeDefineBone->m_Rotation = aTmp;

	// Don't touch realign rotation for now
	/*
	VectorRotate( pDmeDefineBone->m_RealignTranslation, mOrient, vTmp );
	pDmeDefineBone->m_RealignTranslation = vTmp;

	AngleQuaternion( pDmeDefineBone->m_RealignRotation, qTmp0 );
	QuaternionMult( qOrient, qTmp0, qTmp1 );
	QuaternionAngles( qTmp1, aTmp );
	pDmeDefineBone->m_RealignRotation = aTmp;
	*/
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void ReorientDmeModelChildren( CDmeModel *pDmeModel, const matrix3x4_t &mOrient, const Quaternion &qOrient, CDmeDefineBoneList *pDmeDefineBoneList, bool bDefineBoneDone )
{
	if ( !pDmeModel )
		return;

	CDmeTransformList *pDmeTransformList = pDmeModel->FindBaseState( "bind" );
	const int nTransformCount = pDmeTransformList ? pDmeTransformList->GetTransformCount() : 0;

	const int nChildCount = pDmeModel->GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeDag *pDmeDag = pDmeModel->GetChild( i );
		if ( !pDmeDag )
			continue;

		ReorientDmeTransform( pDmeDag->GetTransform(), mOrient, qOrient );

		// TODO: Separate out DmeDefineBoneList
		if ( !bDefineBoneDone && pDmeDefineBoneList )
		{
			for ( int j = 0; j < pDmeDefineBoneList->m_DefineBones.Count(); ++j )
			{
				CDmeDefineBone *pDmeDefineBone = pDmeDefineBoneList->m_DefineBones[j];
				if ( !pDmeDefineBone )
					continue;

				if ( Q_strcmp( pDmeDag->GetName(), pDmeDefineBone->GetName() ) )
					continue;

				ReorientDmeDefineBone( pDmeDefineBone, mOrient, qOrient );
			}
		}

		if ( pDmeTransformList )
		{
			int nJointIndex = pDmeModel->GetJointIndex( pDmeDag );
			if ( nJointIndex >= 0 && nJointIndex < nTransformCount )
			{
				ReorientDmeTransform( pDmeTransformList->GetTransform( nJointIndex ), mOrient, qOrient );
			}
		}

		ReorientDmeAnimation( pDmeDag, mOrient, qOrient );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool ReorientDmeModel( CDmeModel *pDmeModel, bool bMakeZUp, CDmeDefineBoneList *pDmeDefineBoneList, bool bDefineBoneDone )
{
	if ( !pDmeModel )
		return false;

	if ( bMakeZUp == pDmeModel->IsZUp() )
		return true;	// Nothing to do

	matrix3x4_t m;
	Quaternion q;

	GetReorientData( m, q, bMakeZUp );

	ReorientDmeModelChildren( pDmeModel, m, q, pDmeDefineBoneList, bDefineBoneDone );

	pDmeModel->ZUp( true );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool ReorientDmeModel( CDmeModel *pDmeModel, bool bMakeZUp )
{
	return ReorientDmeModel( pDmeModel, bMakeZUp, NULL, true );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool ReorientDmxModelFile( CDmElement *pDmElementRoot, bool bMakeZUp )
{
	if ( !pDmElementRoot )
		return false;

	const bool bModel = ReorientDmeModel( pDmElementRoot->GetValueElement< CDmeModel >( "model" ), bMakeZUp );
	const bool bSkel = ReorientDmeModel( pDmElementRoot->GetValueElement< CDmeModel >( "skeleton" ), bMakeZUp );

	const char *pSrcFile = g_pDataModel->GetFileName( pDmElementRoot->GetFileId() );
	if ( pSrcFile && *pSrcFile )
	{
		char szPath[ MAX_PATH ];

		Q_strncpy( szPath, pSrcFile, sizeof( szPath ) );
		Q_SetExtension( szPath, "zup", sizeof( szPath ) );
		g_pDataModel->SaveToFile( szPath, NULL, "keyvalues2", "model", pDmElementRoot );
	}

	return bModel || bSkel;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void ReorientIkRuleList( CDmeSequence *pDmeSimpleSequence, const matrix3x4_t &m )
{
	if ( !pDmeSimpleSequence )
		return;

	const int nIkRuleCount = pDmeSimpleSequence->m_eIkRuleList.Count();
	if ( nIkRuleCount <= 0 )
		return;

	for ( int i = 0; i < nIkRuleCount; ++i )
	{
		CDmeIkRule *pDmeIkRule = pDmeSimpleSequence->m_eIkRuleList[ i ];
		if ( !pDmeIkRule )
			continue;

		CDmeIkAttachmentRule *pDmeIkAttachmentRule = CastElement< CDmeIkAttachmentRule >( pDmeIkRule );
		if ( pDmeIkAttachmentRule )
		{
			if ( pDmeIkAttachmentRule->HasAttribute( "fallbackPosition", AT_VECTOR3 ) )
			{
				Vector p;
				VectorIRotate( pDmeIkAttachmentRule->GetValue< Vector >( "fallbackPosition" ), m, p );
				pDmeIkAttachmentRule->SetValue< Vector >( "fallbackPosition", p );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void ReorientSequenceList( CDmeSequenceList *pDmeSequenceList, bool bMakeZUp, const matrix3x4_t &m )
{
	if ( !pDmeSequenceList )
		return;

	const int nSequenceCount = pDmeSequenceList->m_Sequences.Count();
	for ( int i = 0; i < nSequenceCount; ++i )
	{
		CDmeSequence *pDmeSimpleSequence = CastElement< CDmeSequence >( pDmeSequenceList->m_Sequences[i] );
		if ( !pDmeSimpleSequence )
			continue;

		ReorientDmeModel( CastElement< CDmeModel >( pDmeSimpleSequence->m_eSkeleton.GetElement() ), bMakeZUp );

		ReorientIkRuleList( pDmeSimpleSequence, m );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void ReorientGlobalFlags( CDmElement *pDmeRoot, const matrix3x4_t &m )
{
	if ( !pDmeRoot )
		return;

	CDmeBBox *pDmeBBox = pDmeRoot->GetValueElement< CDmeBBox >( "bbox" );
	if ( pDmeBBox )
	{
		Vector bbmn;
		Vector bbmx;

		ITransformAABB( m, pDmeBBox->m_vMinBounds, pDmeBBox->m_vMaxBounds, bbmn, bbmx );
		pDmeBBox->m_vMinBounds = bbmn;
		pDmeBBox->m_vMaxBounds = bbmx;
	}

	if ( pDmeRoot->HasAttribute( "illuminationPosition", AT_VECTOR3 ) )
	{
		Vector vIllumPosition;
		VectorIRotate( pDmeRoot->GetValue< Vector >( "illuminationPosition" ), m, vIllumPosition );
		pDmeRoot->SetValue< Vector >( "illuminationPosition", vIllumPosition );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void ReorientEyeballGlobals( CDmeEyeballGlobals *pDmeEyeballGlobals, const matrix3x4_t &m )
{
	if ( !pDmeEyeballGlobals )
		return;

	Vector vEyePosition;
	VectorIRotate( pDmeEyeballGlobals->m_vEyePosition.Get(), m, vEyePosition );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void ReorientEyeballs( CDmeLODList *pDmeLODList, const matrix3x4_t &m )
{
	if ( !pDmeLODList )
		return;

	const int nEyeballCount = pDmeLODList->m_EyeballList.Count();
	if ( nEyeballCount <= 0 )
		return;

	for ( int nEyeballIndex = 0; nEyeballIndex < nEyeballCount; ++nEyeballIndex )
	{
		CDmeEyeball *pDmeEyeball = pDmeLODList->m_EyeballList.Get( nEyeballIndex );
		if ( !pDmeEyeball )
			continue;
	}
}


//-----------------------------------------------------------------------------
//
// Returns true is the rest of the MPP file should be reoriented
// Returns false if the orientation could not be determined or
// it's already oriented correctly
//
//-----------------------------------------------------------------------------
bool ReorientBodyGroupList( CDmeBodyGroupList *pDmeBodyGroupList, CDmeDefineBoneList *pDmeDefineBoneList, bool bMakeZUp, const matrix3x4_t &m )
{
	if ( !pDmeBodyGroupList )
		return false;

	bool bUpAxisSet = false;
	bool bZUp = false;
	bool bDefineBoneDone = false;

	const int nBodyGroupCount = pDmeBodyGroupList->m_BodyGroups.Count();
	for ( int i = 0; i < nBodyGroupCount; ++i )
	{
		CDmeBodyGroup *pDmeBodyGroup = pDmeBodyGroupList->m_BodyGroups[i];
		if ( !pDmeBodyGroup )
			continue;

		const int nBodyPartCount = pDmeBodyGroup->m_BodyParts.Count();
		for ( int j = 0; j < nBodyPartCount; ++j )
		{
			CDmeBodyPart *pDmeBodyPart = pDmeBodyGroup->m_BodyParts[j];
			CDmeLODList *pDmeLODList = CastElement< CDmeLODList >( pDmeBodyPart );
			if ( !pDmeBodyPart || !pDmeLODList || pDmeBodyPart->LODCount() == 0 )
				continue;

			const int nLODCount = pDmeLODList->m_LODs.Count();
			for ( int k = 0; k < nLODCount; ++k )
			{
				CDmeLOD *pDmeLOD = pDmeLODList->m_LODs[k];
				if ( !pDmeLOD )
					continue;

				CDmeModel *pDmeModel = pDmeLOD->m_Model.GetElement();
				if ( !pDmeModel )
					continue;

				if ( !bUpAxisSet )
				{
					bZUp = pDmeModel->IsZUp();
					bUpAxisSet = true;
				}
				else
				{
					if ( pDmeModel->IsZUp() != bZUp )
					{
						Warning( "Z Up Mismatch\n" );
					}
				}

				if ( bZUp != bMakeZUp )
				{
					ReorientDmeModel( pDmeModel, bMakeZUp, pDmeDefineBoneList, bDefineBoneDone );
					if ( !bDefineBoneDone )
					{
						bDefineBoneDone = true;
					}
					ReorientEyeballs( pDmeLODList, m );
				}
			}
		}
	}

	return bZUp != bMakeZUp;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void ReorientCollisionModel( CDmeCollisionModel *pDmeCollisionModel, bool bMakeZUp )
{
	if ( !pDmeCollisionModel )
		return;

	ReorientDmeModel( pDmeCollisionModel->GetValueElement< CDmeModel >( "model" ), bMakeZUp );
}


//-----------------------------------------------------------------------------
// Reorients everything in an MPP file
//-----------------------------------------------------------------------------
void ReorientMppFile( CDmElement *pRoot, bool bMakeZUp )
{
	matrix3x4_t m;
	Quaternion q;
	GetReorientData( m, q, bMakeZUp );

	if ( ReorientBodyGroupList(
		pRoot->GetValueElement< CDmeBodyGroupList >( "bodyGroupList" ),
		pRoot->GetValueElement< CDmeDefineBoneList >( "defineBoneList" ),
		bMakeZUp, m ) )
	{
		ReorientGlobalFlags( pRoot, m );

		ReorientCollisionModel( pRoot->GetValueElement< CDmeCollisionModel >( "collisionModel" ), bMakeZUp );

		ReorientEyeballGlobals( pRoot->GetValueElement< CDmeEyeballGlobals >( "eyeballGlobals" ), m );

		ReorientSequenceList( pRoot->GetValueElement< CDmeSequenceList >( "sequenceList" ), bMakeZUp, m );
	}
	else
	{
//		Msg( "File Already %s Up\n", bMakeZUp ? "Z" : "Y" );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void MppReorient( CDmElement *pRoot, bool bMakeZUp )
{
	ReorientMppFile( pRoot, bMakeZUp );
}


//-----------------------------------------------------------------------------
// Get a list of all DmeDag nodes which are the ancestors of the
// specified node.  Returns false if this cannot be computed because
// of a cycle or a NULL DmeDag was passed
//
// + hierarchyList[0]					// Root
//   + hierarchyList[1]					// 0 is parent of 1
//     + hierarchyList[2]				// 1 is parent of 2
//       ...
//       + hierarchyList[n] == pDmeDag	// 
//
//-----------------------------------------------------------------------------
bool GetDagHierarchy( CUtlVector< CDmeDag * > &hierarchyList, CDmeDag *pDmeDag )
{
	if ( !pDmeDag )
		return false;

	CUtlRBTree< DmElementHandle_t > visited( DefLessFunc( DmElementHandle_t ) );

	hierarchyList.RemoveAll();
	for ( ; pDmeDag; pDmeDag = pDmeDag->GetParent() )
	{
		if ( visited.IsValidIndex( visited.Find( pDmeDag->GetHandle() ) ) )
		{
			// Found a cycle
			return false;
		}

		visited.Insert( pDmeDag->GetHandle() );
		hierarchyList.AddToHead( pDmeDag );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void GetDmeChannelList(
	CUtlVector< CDmeChannel * > &PChannelList,
	CUtlVector< CDmeChannel * > &OChannelList,
	const CUtlVector< CDmeDag * > &dagList )
{
	PChannelList.RemoveAll();

	for ( int i = 0; i < dagList.Count(); ++i )
	{
		CUtlVector< CDmeChannel * > dmeChannelList;

		bool bPHandled = false;
		bool bOHandled = false;

		if ( FindReferringElements( dmeChannelList, dagList[i]->GetTransform(), g_pDataModel->GetSymbol( "toElement" ) ) && dmeChannelList.Count() > 0 )
		{
			for ( int j = 0; j < dmeChannelList.Count(); ++j )
			{
				CDmeLog *pDmeLog = dmeChannelList[j]->GetLog();
				if ( !pDmeLog )
					continue;

				for ( int k = 0; k < pDmeLog->GetNumLayers(); ++k )
				{
					CDmeLogLayer *pDmeLogLayer = pDmeLog->GetLayer( k );

					CDmeVector3LogLayer *pDmeVector3LogLayer = CastElement< CDmeVector3LogLayer >( pDmeLogLayer );
					if ( !bPHandled && pDmeVector3LogLayer )
					{
						bPHandled = true;
						PChannelList.AddToTail( dmeChannelList[j] );
						continue;
					}

					CDmeQuaternionLogLayer *pDmeQuaternionLogLayer = CastElement< CDmeQuaternionLogLayer >( pDmeLogLayer );
					if ( !bOHandled && pDmeQuaternionLogLayer  )
					{ 
						bOHandled = true;
						OChannelList.AddToTail( dmeChannelList[j] );
						continue;
					}
				}
			}
		}

		if ( !bPHandled )
		{
			PChannelList.AddToTail( NULL );
		}

		if ( !bOHandled )
		{
			OChannelList.AddToTail( NULL );
		}
	}
}


//-----------------------------------------------------------------------------
// Computes common data for GetAbsMotion & SetAbsMotion
//-----------------------------------------------------------------------------
bool PrepAndValidateMotionData(
	CDmeChannel **pDmeDagPositionChannel,
	CDmeChannel **pDmeDagOrientationChannel,
	CDmeDag *pDmeDag,
	CUtlVector< CDmeDag * > &hierarchyList,
	CUtlVector< CDmeChannel * > &PChannelList,
	CUtlVector< CDmeChannel * > &OChannelList,
	CUtlRBTree< DmeTime_t > &keyTimes )
{
	if ( !pDmeDag  )
		return false;

	hierarchyList.RemoveAll();
	if ( !GetDagHierarchy( hierarchyList, pDmeDag ) )
		return false;	// Found a cycle, abort!

	PChannelList.RemoveAll();
	OChannelList.RemoveAll();
	GetDmeChannelList( PChannelList, OChannelList, hierarchyList );
	if ( hierarchyList.Count() != PChannelList.Count()
		|| hierarchyList.Count() != OChannelList.Count() )
		return false;

	CDmeChannel *pSrcPChannel = PChannelList.Tail();
	if ( pDmeDagPositionChannel )
	{
		*pDmeDagPositionChannel = pSrcPChannel;
	}

	CDmeLog *pSrcPLog = pSrcPChannel ? pSrcPChannel->GetLog() : NULL;

	CDmeChannel *pSrcOChannel = OChannelList.Tail();
	if ( pDmeDagOrientationChannel )
	{
		*pDmeDagOrientationChannel = pSrcOChannel;
	}

	CDmeLog *pSrcOLog = pSrcOChannel ? pSrcOChannel->GetLog() : NULL;

	// Build a list of key times containing all keys from both logs
	if ( pSrcPLog )
	{
		for ( int i = 0; i < pSrcPLog->GetKeyCount(); ++i )
		{
			keyTimes.InsertIfNotFound( pSrcPLog->GetKeyTime( i ) );
		}
	}

	if ( pSrcOLog )
	{
		for ( int i = 0; i < pSrcOLog->GetKeyCount(); ++i )
		{
			keyTimes.InsertIfNotFound( pSrcOLog->GetKeyTime( i ) );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Returns a CDmElement with two attributes:
//
// "positionChannel"    - CDmeChannel
// "orientationChannel" - CDmeChannel
//
// These two CDmeChannel's are the worldSpace position and orientation
// of the specified DmeDag node computed over the same time frame as
// the original DmeChannel driving the DmeDag node.
//
// If the DmeDag node isn't animated then two single frame DmeChannels
// are returned with a single sample at DmeTime_t( 0.0 )
//
// NOTE: This should probably take a time frame over which to bake
//       and a sample rate and then simply set time and evaluate the
//       DmeOperator's in the scene so that rig operator's, etc...
//       will work and it doesn't have to be driven by just a DmeChannel
//-----------------------------------------------------------------------------
void GetAbsMotion( CDmeChannel **ppDmePChannel, CDmeChannel **ppDmeOChannel, CDmeDag *pDmeDag )
{
	if ( !pDmeDag | !ppDmePChannel || !ppDmeOChannel )
		return;

	*ppDmePChannel = NULL;
	*ppDmeOChannel = NULL;

	CUtlVector< CDmeDag * > hierarchyList;
	CUtlVector< CDmeChannel * > PChannelList;
	CUtlVector< CDmeChannel * > OChannelList;
	CUtlRBTree< DmeTime_t > keyTimes( DefLessFunc( DmeTime_t ) );
	CDmeChannel *pSrcPChannel = NULL;
	CDmeChannel *pSrcOChannel = NULL;

	if ( !PrepAndValidateMotionData( &pSrcPChannel, &pSrcOChannel, pDmeDag, hierarchyList, PChannelList, OChannelList, keyTimes ) )
		return;

	CDmeChannel *pDstPChannel = pSrcPChannel ?
		CreateElement< CDmeChannel >( pSrcPChannel->GetName(), pSrcPChannel->GetFileId() ) :
		CreateElement< CDmeChannel >( ( CUtlString( pDmeDag->GetName() ) + "_p" ).Get(), pDmeDag->GetFileId() );
	*ppDmePChannel = pDstPChannel;
	CDmeVector3Log *pDstPLog = pDstPChannel->CreateLog< Vector >();
	pDstPLog->SetValueThreshold( 1.0e-6 );

	CDmeChannel *pDstOChannel = pSrcOChannel ?
		CreateElement< CDmeChannel >( pSrcOChannel->GetName(), pSrcPChannel->GetFileId() ) :
		CreateElement< CDmeChannel >( ( CUtlString( pDmeDag->GetName() ) + "_o" ).Get(), pDmeDag->GetFileId() );
	*ppDmeOChannel = pDstOChannel;
	CDmeQuaternionLog *pDstOLog = pDstOChannel->CreateLog< Quaternion >();
	pDstOLog->SetValueThreshold( 1.0e-6 );

	matrix3x4_t wm;
	matrix3x4_t twm;
	matrix3x4_t m;

	Vector v;
	Quaternion q;

	if ( keyTimes.Count() > 0 )
	{
		// Node is animated, so loop through all of the keys and sample all
		// animation curves
		// Loop through all logs, get current time, accumulate matrix
		for ( CUtlRBTree< DmeTime_t >::IndexType_t i = keyTimes.FirstInorder(); keyTimes.IsValidIndex( i ); i = keyTimes.NextInorder( i ) )
		{
			const DmeTime_t dmeTime = keyTimes[i];

			SetIdentityMatrix( wm );

			for ( int j = 0; j < hierarchyList.Count(); ++j )
			{
				v.Init( 0.0f, 0.0f, 0.0f );
				q = quat_identity;

				CDmeChannel *pPChannel = PChannelList[j];
				if ( pPChannel )
				{
					pPChannel->GetPlaybackValueAtTime( dmeTime, v );
				}
				else
				{
					v = hierarchyList[j]->GetTransform()->GetPosition();
				}

				CDmeChannel *pOChannel = OChannelList[j];
				if ( pOChannel )
				{
					pOChannel->GetPlaybackValueAtTime( dmeTime, q );
				}
				else
				{
					q = hierarchyList[j]->GetTransform()->GetOrientation();
				}

				QuaternionMatrix( q, v, m );
				MatrixCopy( wm, twm );
				ConcatTransforms( twm, m, wm );
			}

			MatrixAngles( wm, q, v );
			pDstOLog->SetKey( dmeTime, q );
			pDstPLog->SetKey( dmeTime, v );
		}
	}
	else
	{
		// Node is not animated, so use current value for all nodes...
		SetIdentityMatrix( wm );
		Vector wt( 0.0f, 0.0f, 0.0f );

		for ( int j = 0; j < hierarchyList.Count(); ++j )
		{
			v.Init( 0.0f, 0.0f, 0.0f );
			q = quat_identity;

			CDmeTransform *pDmeTransform = hierarchyList[j]->GetTransform();
			if ( pDmeTransform )
			{
				v = hierarchyList[j]->GetTransform()->GetPosition();
				q = hierarchyList[j]->GetTransform()->GetOrientation();
			}

			wt += v;

			QuaternionMatrix( q, v, m );
			MatrixCopy( wm, twm );
			ConcatTransforms( twm, m, wm );
		}

		MatrixAngles( wm, q, v );

		pDstOLog->SetKey( DmeTime_t( 0.0 ), q );
		pDstPLog->SetKey( DmeTime_t( 0.0 ), v );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool SetAbsMotion( CDmeDag *pDmeDag, CDmeChannel *pDmePositionChannel, CDmeChannel *pDmeOrientationChannel )
{
	CUtlVector< CDmeDag * > hierarchyList;
	CUtlVector< CDmeChannel * > PChannelList;
	CUtlVector< CDmeChannel * > OChannelList;
	CUtlRBTree< DmeTime_t > keyTimes( DefLessFunc( DmeTime_t ) );
	CDmeChannel *pDmeDagPChannel = NULL;
	CDmeChannel *pDmeDagOChannel = NULL;

	if ( !PrepAndValidateMotionData( &pDmeDagPChannel, &pDmeDagOChannel, pDmeDag, hierarchyList, PChannelList, OChannelList, keyTimes ) )
		return false;

	CDmeVector3Log *pDstPLog = pDmeDagPChannel ? CastElement< CDmeVector3Log >( pDmeDagPChannel->GetLog() ) : NULL;
	CDmeQuaternionLog *pDstOLog = pDmeDagOChannel ? CastElement< CDmeQuaternionLog >( pDmeDagOChannel->GetLog() ) : NULL;

	matrix3x4_t pwm;
	matrix3x4_t ipwm;
	matrix3x4_t tpwm;
	matrix3x4_t m;
	matrix3x4_t wm;

	CUtlVector< matrix3x4_t > localMatrices;

	Vector v;
	Quaternion q;

	// Loop through all logs except last one to get parent world matrix at each key time
	if ( keyTimes.Count() <= 0 )
	{
		CDmeLog *pSrcPLog = pDmePositionChannel ? pDmePositionChannel->GetLog() : NULL;
		if ( pSrcPLog )
		{
			for ( int i = 0; i < pSrcPLog->GetKeyCount(); ++i )
			{
				keyTimes.InsertIfNotFound( pSrcPLog->GetKeyTime( i ) );
			}
		}

		CDmeLog *pSrcOLog = pDmeOrientationChannel ? pDmeOrientationChannel->GetLog() : NULL;
		if ( pSrcOLog )
		{
			for ( int i = 0; i < pSrcOLog->GetKeyCount(); ++i )
			{
				keyTimes.InsertIfNotFound( pSrcOLog->GetKeyTime( i ) );
			}
		}
	}

	if ( keyTimes.Count() <= 0 )
	{
		keyTimes.Insert( DmeTime_t( 0.0 ) );
	}

	for ( CUtlRBTree< DmeTime_t >::IndexType_t i = keyTimes.FirstInorder(); keyTimes.IsValidIndex( i ); i = keyTimes.NextInorder( i ) )
	{
		const DmeTime_t dmeTime = keyTimes[i];

		SetIdentityMatrix( pwm );

		for ( int j = 0; j < hierarchyList.Count() - 1; ++j )
		{
			v.Init( 0.0f, 0.0f, 0.0f );
			q = quat_identity;

			CDmeChannel *pPChannel = PChannelList[j];
			if ( pPChannel )
			{
				pPChannel->GetPlaybackValueAtTime( dmeTime, v );
			}
			else
			{
				v = hierarchyList[j]->GetTransform()->GetPosition();
			}

			CDmeChannel *pOChannel = OChannelList[j];
			if ( pOChannel )
			{
				pOChannel->GetPlaybackValueAtTime( dmeTime, q );
			}
			else
			{
				q = hierarchyList[j]->GetTransform()->GetOrientation();
			}

			QuaternionMatrix( q, v, m );
			MatrixCopy( pwm, tpwm );
			ConcatTransforms( tpwm, m, pwm );
		}

		// pwm is the parent world matrix at dmeTime
		MatrixInvert( pwm, ipwm );

		// wm is the world matrix of dmeDag at dmeTime
		pDmePositionChannel->GetPlaybackValueAtTime( dmeTime, v );
		pDmeOrientationChannel->GetPlaybackValueAtTime( dmeTime, q );
		QuaternionMatrix( q, v, wm );

		ConcatTransforms( ipwm, wm, m );
		localMatrices.AddToTail( m );
	}

	Assert( static_cast< int >( keyTimes.Count() ) == localMatrices.Count() );

	if ( pDstPLog && pDstOLog )
	{
		// Destination node is animated
		pDstPLog->ClearKeys();
		pDstOLog->ClearKeys();

		// Now set the key/values in the logs
		int nMatIndex = 0;
		for ( CUtlRBTree< DmeTime_t >::IndexType_t i = keyTimes.FirstInorder(); keyTimes.IsValidIndex( i ); i = keyTimes.NextInorder( i ), ++nMatIndex )
		{
			const DmeTime_t dmeTime = keyTimes[i];
			MatrixAngles( localMatrices[nMatIndex], q, v );
			pDstPLog->SetKey( dmeTime, v );
			pDstOLog->SetKey( dmeTime, q );
		}
	}
	else
	{
		// Destination node is static
		hierarchyList.Tail()->GetTransform()->SetTransform( localMatrices[0] );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Creates a guaranteed unique DmFileId_t
//-----------------------------------------------------------------------------
DmFileId_t CreateUniqueDmFileId()
{
	UniqueId_t uniqueId;
	CreateUniqueId( &uniqueId );

	char buf[64];
	UniqueIdToString( uniqueId, buf, ARRAYSIZE( buf ) );

	return g_pDataModel->FindOrCreateFileId( buf );
}


//=============================================================================
// MppSequenceIt - MPP File Sequence Iterator
//=============================================================================
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
MppSequenceIt::MppSequenceIt( CDmeAssetRoot *pDmeAssetRoot )
: m_nSequenceIndex( -1 )
{
	if ( pDmeAssetRoot )
	{
		CDmeSequenceList *pDmeSequenceList = pDmeAssetRoot->GetValueElement< CDmeSequenceList >( "sequenceList" );
		if ( pDmeSequenceList )
		{
			CUtlVector< CDmeSequenceBase * > sortedSequenceList;
			pDmeSequenceList->GetSortedSequenceList( sortedSequenceList );

			const int nSortedSequenceCount = sortedSequenceList.Count();
			m_hDmeSequenceList.EnsureCapacity( nSortedSequenceCount );
			for ( int i = 0; i < nSortedSequenceCount; ++i )
			{
				CDmeSequence *pDmeSequence = CastElement< CDmeSequence >( sortedSequenceList[i] );
				if ( !pDmeSequence )
					continue;

				m_hDmeSequenceList.AddToTail( pDmeSequence->GetHandle() );
			}
		}
	}

	Next();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeSequence *MppSequenceIt::Get() const
{
	if ( m_nSequenceIndex < 0 || m_nSequenceIndex >= m_hDmeSequenceList.Count() )
		return NULL;

	return CastElement< CDmeSequence >( g_pDataModel->GetElement( m_hDmeSequenceList[m_nSequenceIndex] ) );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool MppSequenceIt::IsDone() const
{
	return Get() == NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void MppSequenceIt::Next()
{
	for ( int i = m_nSequenceIndex + 1; i < m_hDmeSequenceList.Count(); ++i )
	{
		CDmeSequence *pDmeSequence = CastElement< CDmeSequence >( g_pDataModel->GetElement( m_hDmeSequenceList[i] ) );
		if ( pDmeSequence )
		{
			m_nSequenceIndex = i;
			return;
		}
	}

	m_nSequenceIndex = m_hDmeSequenceList.Count();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void MppSequenceIt::Reset()
{
	m_nSequenceIndex = -1;
	Next();
}


//-----------------------------------------------------------------------------
// Adds the passed pDmeModel to the skeletonList if it isn't already in the
// list and it isn't NULL
//-----------------------------------------------------------------------------
static void AddUniqueSkeleton( CUtlVector< CDmeModel * > &skeletonList, CDmeModel *pDmeModel ) 
{
	if ( !pDmeModel )
		return;

	bool bFound = false;

	for ( int i = 0; i < skeletonList.Count(); ++i )
	{
		if ( pDmeModel == skeletonList[i] )
		{
			bFound = true;
			break;
		}
	}

	if ( !bFound )
	{
		skeletonList.AddToTail( pDmeModel );
	}
}


//-----------------------------------------------------------------------------
// Returns a list of all unique skeletons used by animations/sequences under the MPP DmeAssetRoot
//-----------------------------------------------------------------------------
void MppGetAnimationSkeletonList( CUtlVector< CDmeModel * > &skeletonList, CDmeAssetRoot *pDmeAssetRoot )
{
	skeletonList.RemoveAll();

	for ( MppSequenceIt sIt( pDmeAssetRoot ); !sIt.IsDone(); sIt.Next() )
	{
		AddUniqueSkeleton( skeletonList, CastElement< CDmeModel >( sIt.Get()->m_eSkeleton.GetElement() ) );
	}
}


//-----------------------------------------------------------------------------
// Returns a list of all unique skeletons used by the physics model under the MPP DmeAssetRoot
//-----------------------------------------------------------------------------
void MppGetPhysicsSkeletonList( CUtlVector< CDmeModel * > &skeletonList, CDmeAssetRoot *pDmeAssetRoot )
{
	skeletonList.RemoveAll();

	if ( !pDmeAssetRoot )
		return;

	CDmeCollisionModel *pDmeCollisionModel = pDmeAssetRoot->GetValueElement< CDmeCollisionModel >( "collisionModel" );
	if ( !pDmeCollisionModel )
		return;

	const char *const pszAttrs[] = { "skeleton", "model" };

	for ( int i = 0; i < 2; ++i )
	{
		AddUniqueSkeleton( skeletonList, pDmeCollisionModel->GetValueElement< CDmeModel >( pszAttrs[i] ) );
	}
}


//-----------------------------------------------------------------------------
// Returns a list of all unique skeletons used by the models under the MPP DmeAssetRoot
//-----------------------------------------------------------------------------
void MppGetModelSkeletonList( CUtlVector< CDmeModel * > &skeletonList, CDmeAssetRoot *pDmeAssetRoot )
{
	skeletonList.RemoveAll();

	if ( !pDmeAssetRoot )
		return;

	CDmeBodyGroupList *pDmeBodyGroupList = pDmeAssetRoot->GetValueElement< CDmeBodyGroupList >( "bodyGroupList" );
	if ( !pDmeBodyGroupList )
		return;

	const char *const pszAttrs[] = { "skeleton", "model" };

	for ( int i = 0; i < pDmeBodyGroupList->m_BodyGroups.Count(); ++i )
	{
		CDmeBodyGroup *pDmeBodyGroup = pDmeBodyGroupList->m_BodyGroups[i];
		if ( !pDmeBodyGroup )
			continue;

		for ( int j = 0; j < pDmeBodyGroup->m_BodyParts.Count(); ++j )
		{
			CDmeLODList *pDmeLODList = CastElement< CDmeLODList >( pDmeBodyGroup->m_BodyParts[j] );
			if ( !pDmeLODList )
				continue;

			for ( int k = 0; k < pDmeLODList->m_LODs.Count(); ++k )
			{
				CDmeLOD *pDmeLOD = pDmeLODList->m_LODs[k];
				if ( !pDmeLOD )
					continue;

				for ( int i = 0; i < 2; ++i )
				{
					AddUniqueSkeleton( skeletonList, pDmeLOD->GetValueElement< CDmeModel >( pszAttrs[i] ) );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Returns a list of all unique skeletons under the specified MPP DmeAssetRoot
//-----------------------------------------------------------------------------
void MppGetSkeletonList( CUtlVector< CDmeModel * > &skeletonList, CDmeAssetRoot *pDmeAssetRoot, int nMppSkeletonMask /* = kMppAllSkeletonMask */ )
{
	skeletonList.RemoveAll();

	if ( !pDmeAssetRoot )
		return;

	for ( int i = 0; i < 3; ++i )
	{
		CUtlVector< CDmeModel * > tmpSkeletonList;
		switch ( i )
		{
		case 0:
			if ( nMppSkeletonMask & MPP_ANIM_SKELETON_MASK )
			{
				MppGetAnimationSkeletonList( tmpSkeletonList, pDmeAssetRoot );
			}
			break;
		case 1:
			if ( nMppSkeletonMask & MPP_PHYSICS_SKELETON_MASK )
			{
				MppGetPhysicsSkeletonList( tmpSkeletonList, pDmeAssetRoot );
			}
			break;
		case 2:
			if ( nMppSkeletonMask & MPP_MODEL_SKELETON_MASK )
			{
				MppGetModelSkeletonList( tmpSkeletonList, pDmeAssetRoot );
			}
			break;
		}

		for ( int j = 0; j < tmpSkeletonList.Count(); ++j )
		{
			CDmeModel *pDmeModel = tmpSkeletonList[j];
			if ( !pDmeModel )
				continue;

			bool bFound = false;

			for ( int k = 0; k < skeletonList.Count(); ++k )
			{
				if ( pDmeModel == skeletonList[k] )
				{
					bFound = true;
					break;
				}
			}

			if ( !bFound )
			{
				skeletonList.AddToTail( pDmeModel );
			}
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeTransform *FindMatchingSkeletonTransform( CDmeModel *pDmeModel, CDmeTransform *pDmeTransform )
{
	if ( !pDmeModel || !pDmeTransform )
		return NULL;

	CDmeDag *pDmeDag = NULL;
	CDmeDag *pDmeDagChild = NULL;
	CDmeTransform *pDmeDagTransform = NULL;

	CUtlStack< CDmeDag * > depthFirstDagStack;
	depthFirstDagStack.Push( pDmeModel );

	while ( depthFirstDagStack.Count() )
	{
		depthFirstDagStack.Pop( pDmeDag );
		if ( !pDmeDag )
			continue;

		pDmeDagTransform = pDmeDag->GetTransform();
		if ( pDmeDagTransform && !Q_stricmp( pDmeTransform->GetName(), pDmeDagTransform->GetName() ) )
			return pDmeDagTransform;

		for ( int i = pDmeDag->GetChildCount() - 1; i >= 0; --i )
		{
			pDmeDagChild = pDmeDag->GetChild( i );
			if ( !pDmeDagChild )
				continue;

			depthFirstDagStack.Push( pDmeDagChild );
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeConnectionOperator *CreateOutgoingConnectionOperator( CDmeTransform *pSrcTransform, CDmAttribute *pSrcAttribute, DmFileId_t nFileId )
{
	if ( !pSrcTransform || !pSrcAttribute )
		return NULL;

	CUtlString sName( pSrcTransform->GetName() );
	sName += ".";
	sName += pSrcAttribute->GetName();

	CDmeConnectionOperator *pDmeConnectionOperator = CreateElement< CDmeConnectionOperator >( sName.Get(), nFileId );
	if ( !pDmeConnectionOperator )
		return NULL;

	pDmeConnectionOperator->SetInput( pSrcTransform, pSrcAttribute->GetName() );

	return pDmeConnectionOperator;
}


//-----------------------------------------------------------------------------
// Adds the appropriate attribute of the specified DmeTransform as an output
// of the DmeConnectionOperator if an attribute of the same name as the
// input operator can be found on the specified DmeTransform
//-----------------------------------------------------------------------------
static bool MppConnectDmeTransforms( CDmeConnectionOperator *pDmeConnectionOperator, CDmeTransform *pDstTransform )
{
	if ( !pDmeConnectionOperator || !pDstTransform )
		return false;

	CDmAttribute *pInputAttribute = pDmeConnectionOperator->GetInputAttribute();
	if ( !pInputAttribute )
		return false;

	CDmeAttributeReference *pDmeAttributeReference = pInputAttribute->GetValueElement< CDmeAttributeReference >();
	if ( !pDmeAttributeReference )
		return false;

	CDmAttribute *pSrcAttribute = pDmeAttributeReference->GetReferencedAttribute();
	if ( !pSrcAttribute )
		return false;

	CDmAttribute *pDstAttribute = pDstTransform->GetAttribute( pSrcAttribute->GetName() );
	if ( !pDstAttribute )
		return false;

	pDmeConnectionOperator->AddOutput( pDstTransform, pDstAttribute->GetName() );

	return true;
}


//-----------------------------------------------------------------------------
// Return true if this DmeTransform has a DmeChannel driving either the
// position or orientation attributes (or both).
//-----------------------------------------------------------------------------
static bool MppIsDmeTransformAnimated( CDmeTransform *pDmeTransform )
{
//	Assert( 0 );
	return true;
}


//-----------------------------------------------------------------------------
// For each DmeTransform in the source skeleton that has its position or
// orientation attribute driven by a DmeChannel, create two DmeConnectionOperator
// elements, one for position and one for orientation and use those attributes
// as the input to the DmeConnectionOperator.  For each skeleton in the
// dstSkeletonList, if there is a DmeTransform that matches up by a case insensitive
// name search via a depth first walk, connect the appropriate position or
// orientation attribute to the outputs of the DmeConnectionOperator.
// All created DmeConnectionOperators are stored in an attribute on pSrcSkeleton
// called __MppAnimationDmeConnectionOperators which should already exist
// and are given the nFileId passed
//-----------------------------------------------------------------------------
static void MppConnectSkeletons( CDmeModel *pSrcSkeleton, const CUtlVector< CDmeModel * > &dstSkeletonList, DmFileId_t nFileId )
{
	if ( !pSrcSkeleton || dstSkeletonList.Count() <= 0 )
		return;

	CDmAttribute *pConnectionOps = pSrcSkeleton->GetAttribute( "__MppAnimationDmeConnectionOperators", AT_ELEMENT_ARRAY );
	if ( !pConnectionOps )
		return;

	CDmrElementArray< CDmeConnectionOperator > connectionOps( pConnectionOps );

	// Do a depth first DmeDag walk of the source skeleton finding DmeTransforms which
	// have connections to DmeOperators... which is tricky because of the way DmeOperators
	// are defined... kind of needs an upfront knowledge of what DmeOperators might be
	// connected and how they are structured for this to function...

	CUtlStack< CDmeDag * > depthFirstStack;
	depthFirstStack.Push( pSrcSkeleton );

	CDmeDag *pDmeDag = NULL;

	while ( depthFirstStack.Count() )
	{
		depthFirstStack.Pop( pDmeDag );
		if ( !pDmeDag )
			continue;

		CDmeTransform *pSrcDmeTransform = pDmeDag->GetTransform();
		if ( pSrcDmeTransform )
		{
			if ( MppIsDmeTransformAnimated( pSrcDmeTransform ) )
			{
				for ( int i = 0; i < 2; ++i )
				{
					CDmAttribute *pSrcAttribute = i == 0 ? pSrcDmeTransform->GetPositionAttribute() : pSrcDmeTransform->GetOrientationAttribute();

					CDmeConnectionOperator *pDmeConnectionOperator = CreateOutgoingConnectionOperator( pSrcDmeTransform, pSrcAttribute, nFileId );
					if ( !pDmeConnectionOperator )
						continue;

					for ( int j = 0; j < dstSkeletonList.Count(); ++j )
					{
						CDmeModel *pDstSkeleton = dstSkeletonList[j];
						if ( pDstSkeleton == pSrcSkeleton )
							continue;

						CDmeTransform *pDstDmeTransform = FindMatchingSkeletonTransform( pDstSkeleton, pSrcDmeTransform );
						if ( !pDstDmeTransform || pSrcDmeTransform == pDstDmeTransform )
							continue;

						MppConnectDmeTransforms( pDmeConnectionOperator, pDstDmeTransform );
					}

					if ( pDmeConnectionOperator->NumOutputAttributes() <= 0 )
					{
						g_pDataModel->DestroyElement( pDmeConnectionOperator->GetHandle() );
					}
					else
					{
						connectionOps.AddToTail( pDmeConnectionOperator );
					}
				}
			}
		}

		for ( int i = pDmeDag->GetChildCount() - 1; i >= 0; --i )
		{
			depthFirstStack.Push( pDmeDag->GetChild( i ) );
		}
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void MppDisconnectAnimationSkeleton( CDmeModel *pDmeModel )
{
//	Assert( 0 );
}


//-----------------------------------------------------------------------------
// Connects all of the non-animation skeletons to each animation skeleton via DmeConnectionOperators
//-----------------------------------------------------------------------------
DmFileId_t MppConnectSkeletonsForAnimation( CDmeAssetRoot *pDmeAssetRoot )
{
	if ( !pDmeAssetRoot )
		return DMFILEID_INVALID;

	CUtlVector< CDmeModel * > animationSkeletonList;
	MppGetSkeletonList( animationSkeletonList, pDmeAssetRoot, MPP_ANIM_SKELETON_MASK );

	CUtlVector< CDmeModel * > skeletonList;
	MppGetSkeletonList( skeletonList, pDmeAssetRoot, MPP_MODEL_SKELETON_MASK | MPP_PHYSICS_SKELETON_MASK );

	if ( animationSkeletonList.Count() <= 0 || skeletonList.Count() <= 0 )
		return DMFILEID_INVALID;

	DmFileId_t nFileId = CreateUniqueDmFileId();

	for ( int i = 0; i < animationSkeletonList.Count(); ++i )
	{
		CDmeModel *pSrcSkeleton = animationSkeletonList[i];
		if ( !pSrcSkeleton )
			continue;

		MppDisconnectAnimationSkeleton( pSrcSkeleton );

		CDmAttribute *pConnectionOps = pSrcSkeleton->AddAttribute( "__MppAnimationDmeConnectionOperators", AT_ELEMENT_ARRAY );
		pConnectionOps->AddFlag( FATTRIB_DONTSAVE );
		CDmrElementArray< CDmeConnectionOperator > connectionOps( pConnectionOps );

		MppConnectSkeletons( pSrcSkeleton, skeletonList, nFileId );
	}

	return nFileId;
}


//-----------------------------------------------------------------------------
// Disconnects all of the non-animation skeletons from each animation skeleton
// and destroys the elements created by MppConnectSkeletonsForAnimation
//-----------------------------------------------------------------------------
void MppDisconnectSkeletonsFromAnimation( CDmeAssetRoot *pDmeAssetRoot )
{
}


//-----------------------------------------------------------------------------
// Utility to return DmElement id as name:string
//-----------------------------------------------------------------------------
CUtlString ComputeDmElementIdStr( const CDmElement *pDmElement )
{
	if ( !pDmElement )
		return CUtlString( "NULL(Unknown):\"Unknown\"" );

	CUtlString sReturn;

	sReturn = pDmElement->GetTypeString();
	sReturn += "(";
	sReturn = pDmElement->GetName();
	sReturn += "):\"";

	char pszBuf[64];
	UniqueIdToString( pDmElement->GetId(), pszBuf, ARRAYSIZE( pszBuf ) );
	sReturn += pszBuf;

	sReturn += "\"";

	return sReturn;
}