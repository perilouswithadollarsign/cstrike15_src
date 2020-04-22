//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Animation commands
//
//==========================================================================


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeanimationassemblycommand.h"
#include "mdlobjects/dmebonemask.h"
#include "mdlobjects/dmesequence.h"
#include "mdlobjects/mpp_utils.h"
#include "bone_setup.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_DME_AAC, "DmeAnimationAssemblyCommand" );


//-----------------------------------------------------------------------------
// Get the CDmeSequence & CDmeChannelsClip from the specified CDmElement
//-----------------------------------------------------------------------------
static bool ConvertToDmeSequenceAndDmeChannelsClip(
	CDmeSequence *&pDmeSequence,
	CDmeChannelsClip *&pDmeChannelsClip,
	CDmElement *pDmElement,
	const CUtlString &sDmElementId )
{
	if ( !pDmElement )
	{
		Log_Error( LOG_DME_AAC, "%s: No DmElement Specified specified\n", sDmElementId.Get() );
		return false;
	}

	CDmeSequence *pDmeSequenceTmp = CastElement< CDmeSequence >( pDmElement );
	if ( !pDmeSequenceTmp )
	{
		Log_Error( LOG_DME_AAC, "%s: No DmeSequence Specified specified\n", sDmElementId.Get() );
		return false;
	}

	CDmeChannelsClip *pDmeChannelsClipTmp = pDmeSequenceTmp->GetDmeChannelsClip();
	if ( !pDmeChannelsClipTmp )
	{
		Log_Error( LOG_DME_AAC, "%s: Specified Sequence %s Has No DmeChannelsClip\n", sDmElementId.Get(), ComputeDmElementIdStr( pDmeSequenceTmp ).Get() );
		return false;
	}

	pDmeSequence = pDmeSequenceTmp;
	pDmeChannelsClip = pDmeChannelsClipTmp;

	return true;
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAnimationAssemblyCommand, CDmeAnimationAssemblyCommand );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAnimationAssemblyCommand::OnConstruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAnimationAssemblyCommand::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeFixupLoop, CDmeFixupLoop );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeFixupLoop::OnConstruction()
{
	m_nStartFrame.Init( this, "startFrame" );
	m_nEndFrame.Init( this, "endFrame" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeFixupLoop::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeFixupLoop::Apply( CDmElement *pDmElement )
{
	CDmeSequence *pDmeSequenceDst = NULL;
	CDmeChannelsClip *pDmeChannelsClipDst = NULL;

	if ( !ConvertToDmeSequenceAndDmeChannelsClip( pDmeSequenceDst, pDmeChannelsClipDst, pDmElement, ComputeDmElementIdStr( this ) ) )
		return false;

	const DmeFramerate_t dmeFrameRateDst = pDmeSequenceDst->GetFrameRate();
	const int nFrameCountDst = pDmeSequenceDst->GetFrameCount();
	int nStartFrame = m_nStartFrame.Get();
	int nEndFrame = m_nEndFrame.Get();

	// Make sure loop doesn't exceed animation length
	if ( nEndFrame - nStartFrame > nFrameCountDst )
	{
		nEndFrame = nFrameCountDst + nStartFrame;
		if ( nEndFrame < 0 )
		{
			nEndFrame = 0;
			nStartFrame = -( nFrameCountDst - 1 );
		}
	}

	DmeTime_t nStartTime( nStartFrame, dmeFrameRateDst );
	DmeTime_t nEndTime( nEndFrame, dmeFrameRateDst );

	for ( int i = 0; i < pDmeChannelsClipDst->m_Channels.Count(); ++i )
	{
		CDmeChannel *pDmeChannelDst = pDmeChannelsClipDst->m_Channels[i];
		if ( !pDmeChannelDst )
			continue;

		CDmeLog *pDmeLogDst = pDmeChannelDst->GetLog();
		if ( !pDmeLogDst )
			continue;

		CDmeVector3Log *pDmeVector3LogDst = CastElement< CDmeVector3Log >( pDmeLogDst );
		if ( pDmeVector3LogDst )
		{
			Apply( pDmeVector3LogDst, nStartTime, nEndTime );
			continue;
		}

		CDmeQuaternionLog *pDmeQuaternionLogDst = CastElement< CDmeQuaternionLog >( pDmeLogDst );
		if ( pDmeQuaternionLogDst )
		{
			Apply( pDmeQuaternionLogDst, nStartTime, nEndTime );
			continue;
		}

		// ERROR - Unsupported log type
		Log_Warning( LOG_DME_AAC, "%s: Unsupported DmeLog Type: \"%s\"\n", ComputeDmElementIdStr( this ).Get(), ComputeDmElementIdStr( pDmeSequenceDst ).Get() );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T >
void ComputeDelta( T &result, const T &a, const T &b );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template<> void ComputeDelta< Vector >( Vector &vResult, const Vector &vA, const Vector &vB )
{
	VectorSubtract( vA, vB, vResult );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template<> void ComputeDelta< Quaternion >( Quaternion &qResult, const Quaternion &qA, const Quaternion &qB )
{
	QuaternionMA( qA, -1.0f, qB, qResult );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T >
void AddScaledDelta( T &result, float flScale, const T &a, const T &b );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template<> void AddScaledDelta< Vector >( Vector &vResult, float flScale, const Vector &vDelta, const Vector &vOrig )
{
	VectorMA( vOrig, flScale, vDelta, vResult );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template<> void AddScaledDelta< Quaternion >( Quaternion &qResult, float flScale, const Quaternion &qDelta, const Quaternion &qOrig )
{
	QuaternionSM( flScale, qDelta, qOrig, qResult );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T >
void CDmeFixupLoop::Apply(
	CDmeTypedLog< T > *pDmeTypedLogDst,
	const DmeTime_t &dmeTimeStart,
	const DmeTime_t &dmeTimeEnd ) const
{
	if ( !pDmeTypedLogDst )
		return;

	const DmeTime_t dmeTimeRange( dmeTimeEnd - dmeTimeStart );
	if ( dmeTimeRange.GetSeconds() <= 0.0 )
		return;

	if ( dmeTimeStart.GetSeconds() > 0.0f )
		return;

	const float flTimeRange = dmeTimeRange.GetSeconds();

	const int nKeyCount = pDmeTypedLogDst->GetKeyCount();
	if ( nKeyCount <= 0 )
		return;

	CUtlVector< DmeTime_t > times;
	CUtlVector< T > values;

	for ( int i = 0; i < nKeyCount; ++i )
	{
		times.AddToTail( pDmeTypedLogDst->GetKeyTime( i ) );
		values.AddToTail( pDmeTypedLogDst->GetKeyValue( i ) );
	}

	Assert( nKeyCount == pDmeTypedLogDst->GetKeyCount() );

	T delta;
	ComputeDelta< T >( delta, values[ nKeyCount - 1 ], values[ 0 ] );

	T newValue;

	if ( dmeTimeStart.GetSeconds() < 0.0f )
	{
		const DmeTime_t dmeTimeBegin = times[ nKeyCount - 1 ] + dmeTimeStart;
		for ( int nKeyIndex = 0; nKeyIndex < nKeyCount; ++nKeyIndex )
		{
			const DmeTime_t &dmeTimeKey = times[ nKeyIndex ];
			if ( dmeTimeKey < dmeTimeBegin )
				continue;

			float flScale = ( dmeTimeKey - dmeTimeBegin ).GetSeconds() / flTimeRange;
			flScale = 3.0f * flScale * flScale - 2.0f * flScale * flScale * flScale;

			AddScaledDelta( newValue, -flScale, delta, values[ nKeyIndex ] );
			values[ nKeyIndex ] = newValue;
		}
	}

	if ( dmeTimeEnd.GetSeconds() > 0.0f )
	{
		const DmeTime_t dmeTimeBegin = times[ 0 ];
		for ( int nKeyIndex = 0; nKeyIndex < nKeyCount; ++nKeyIndex )
		{
			const DmeTime_t dmeTimeKey = times[ nKeyIndex ];
			if ( dmeTimeKey > dmeTimeEnd )
				break;

			float flScale = ( dmeTimeEnd - ( dmeTimeKey - dmeTimeBegin ) ).GetSeconds() / flTimeRange;
			flScale = 3.0f * flScale * flScale - 2.0f * flScale * flScale * flScale;

			AddScaledDelta( newValue, flScale, delta, values[ nKeyIndex ] );
			values[ nKeyIndex ] = newValue;
		}
	}

	CDmeTypedLogLayer< T > *pDmeTypedLogLayer = CastElement< CDmeTypedLogLayer< T > >( pDmeTypedLogDst->AddNewLayer() );
	if ( pDmeTypedLogLayer )
	{
		pDmeTypedLogLayer->SetAllKeys( times, values );
	}
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSubtract, CDmeSubtract );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSubtract::OnConstruction()
{
	m_eSequence.Init( this, "sequence", FATTRIB_NEVERCOPY );
	m_nFrame.Init( this, "frame" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSubtract::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeSubtract::Apply( CDmElement *pDmElement )
{
	CDmeSequence *pDmeSequenceDst = NULL;
	CDmeChannelsClip *pDmeChannelsClipDst = NULL;

	if ( !ConvertToDmeSequenceAndDmeChannelsClip( pDmeSequenceDst, pDmeChannelsClipDst, pDmElement, ComputeDmElementIdStr( this ) ) )
		return false;

	CDmeSequence *pDmeSequenceSrc = NULL;
	CDmeChannelsClip *pDmeChannelsClipSrc = NULL;

	if ( !ConvertToDmeSequenceAndDmeChannelsClip( pDmeSequenceSrc, pDmeChannelsClipSrc, m_eSequence.GetElement(), ComputeDmElementIdStr( this ) ) )
		return false;

	const DmeFramerate_t dmeFrameRateSrc = pDmeSequenceSrc->GetFrameRate();
	const DmeTime_t dmeTimeSrc( m_nFrame.Get(), dmeFrameRateSrc );

	if ( dmeTimeSrc < pDmeChannelsClipSrc->GetStartTime() )
	{
		Log_Warning( LOG_DME_AAC, "%s: .%s %d (%.2fs @ %g fps) < %s Start Time of %.2fs\n",
			ComputeDmElementIdStr( this ).Get(),
			m_nFrame.GetAttribute()->GetName(),
			m_nFrame.Get(),
			dmeTimeSrc.GetSeconds(),
			dmeFrameRateSrc.GetFramesPerSecond(),
			ComputeDmElementIdStr( pDmeSequenceSrc ).Get(),
			pDmeChannelsClipSrc->GetStartTime().GetSeconds() );
	}

	if ( dmeTimeSrc > pDmeChannelsClipSrc->GetEndTime() )
	{
		Log_Warning( LOG_DME_AAC, "%s: .%s %d (%.2fs @ %g fps) > %s End Time of %.2fs\n",
			ComputeDmElementIdStr( this ).Get(),
			m_nFrame.GetAttribute()->GetName(),
			m_nFrame.Get(),
			dmeTimeSrc.GetSeconds(),
			dmeFrameRateSrc.GetFramesPerSecond(),
			ComputeDmElementIdStr( pDmeSequenceSrc ).Get(),
			pDmeChannelsClipSrc->GetEndTime().GetSeconds() );
	}

	// Match up channels by name.
	for ( int i = 0; i < pDmeChannelsClipDst->m_Channels.Count(); ++i )
	{
		CDmeChannel *pDmeChannelDst = pDmeChannelsClipDst->m_Channels[i];
		if ( !pDmeChannelDst )
			continue;

		CDmeLog *pDmeLogDst = pDmeChannelDst->GetLog();
		if ( !pDmeLogDst )
			continue;

		const char *pszDmeChannelName = pDmeChannelDst->GetName();

		CDmeVector3Log *pDmeVector3LogDst = CastElement< CDmeVector3Log >( pDmeLogDst );
		CDmeQuaternionLog *pDmeQuaternionLogDst = CastElement< CDmeQuaternionLog >( pDmeLogDst );

		bool bFound = false;

		for ( int j = 0; j < pDmeChannelsClipSrc->m_Channels.Count(); ++j )
		{
			CDmeChannel *pDmeChannelSrc = pDmeChannelsClipSrc->m_Channels[j];
			if ( !pDmeChannelSrc || Q_stricmp( pszDmeChannelName, pDmeChannelSrc->GetName() ) )
				continue;

			CDmeLog *pDmeLogSrc = pDmeChannelSrc->GetLog();
			if ( !pDmeLogDst )
				continue;

			CDmeVector3Log *pDmeVector3LogSrc = CastElement< CDmeVector3Log >( pDmeLogSrc );
			if ( pDmeVector3LogSrc && pDmeVector3LogDst )
			{
				Subtract( pDmeVector3LogDst, pDmeVector3LogSrc, dmeTimeSrc );
				bFound = true;
				continue;
			}

			CDmeQuaternionLog *pDmeQuaternionLogSrc = CastElement< CDmeQuaternionLog >( pDmeLogSrc );
			if ( pDmeQuaternionLogSrc && pDmeQuaternionLogDst )
			{
				Subtract( pDmeQuaternionLogDst, pDmeQuaternionLogSrc, dmeTimeSrc );
				bFound = true;
				continue;
			}
		}

		if ( !bFound )
		{
			Log_Warning( LOG_DME_AAC, "%s: No Channel Found To Subtract From %s\n",
				ComputeDmElementIdStr( this ).Get(),
				ComputeDmElementIdStr( pDmeChannelDst ).Get() );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template< class T > void CDmeSubtract::Subtract(
	CDmeTypedLog< T > *pDmeTypedLogDst,
	const CDmeTypedLog< T > *pDmeTypedLogSrc,
	const DmeTime_t &dmeTimeSrc ) const
{
	const T valueSrc = pDmeTypedLogSrc->GetValue( dmeTimeSrc );

	CUtlVector< DmeTime_t > times;
	CUtlVector< T > values;

	T valueDst;

	for ( int i = 0; i < pDmeTypedLogDst->GetKeyCount(); ++i )
	{
		times.AddToTail( pDmeTypedLogDst->GetKeyTime( i ) );
		Subtract( valueDst, pDmeTypedLogDst->GetKeyValue( i ), valueSrc );
		values.AddToTail( valueDst );
	}

	CDmeTypedLogLayer< T > *pDmeTypedLogLayer = CastElement< CDmeTypedLogLayer< T > >( pDmeTypedLogDst->AddNewLayer() );
	if ( pDmeTypedLogLayer )
	{
		pDmeTypedLogLayer->SetAllKeys( times, values );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSubtract::Subtract( Vector &vResult, const Vector &vDst, const Vector &vSrc ) const
{
	VectorSubtract( vDst, vSrc, vResult );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSubtract::Subtract( Quaternion &qResult, const Quaternion &qDst, const Quaternion &qSrc ) const
{
	QuaternionSM( -1, qSrc, qDst, qResult );
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePreSubtract, CDmePreSubtract );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmePreSubtract::OnConstruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmePreSubtract::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmePreSubtract::Subtract( Vector &vResult, const Vector &vDst, const Vector &vSrc ) const
{
	VectorSubtract( vSrc, vDst, vResult );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmePreSubtract::Subtract( Quaternion &qResult, const Quaternion &qDst, const Quaternion &qSrc ) const
{
	QuaternionMA( qDst, -1, qSrc, qResult );
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeRotateTo, CDmeRotateTo );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeRotateTo::OnConstruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeRotateTo::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeRotateTo::Apply( CDmElement *pDmElement )
{
	CDmeSequence *pDmeSequenceDst = NULL;
	CDmeChannelsClip *pDmeChannelsClipDst = NULL;

	if ( !ConvertToDmeSequenceAndDmeChannelsClip( pDmeSequenceDst, pDmeChannelsClipDst, pDmElement, ComputeDmElementIdStr( this ) ) )
		return false;

	CDmeDag *pDmeDag = pDmeSequenceDst->m_eSkeleton.GetElement();
	if ( !pDmeDag )
	{
		Log_Error( LOG_DME_AAC, "%s: Specified Sequence %s Has No Skeleton\n", ComputeDmElementIdStr( this ).Get(), ComputeDmElementIdStr( pDmeSequenceDst ).Get() );
		return false;
	}

	// If the skeleton is a CDmeModel then all children of the skeleton
	// are nodes that have no parent, i.e. root nodes which must be
	// adjusted, otherwise if it's a normal DmeDag assume it's the only
	// root node of the skeleton
	CDmeModel *pDmeModel = CastElement< CDmeModel >( pDmeDag );
	if ( pDmeModel )
	{
		for ( int i = 0; i < pDmeModel->GetChildCount(); ++i )
		{
			SubApply( pDmeModel->GetChild( i ), pDmeChannelsClipDst, pDmeModel->IsZUp() );
		}
	}
	else
	{
		Log_Warning( LOG_DME_AAC, "%s: Cannot Determine If Sequence %s Is Y Or Z Up, Assuming Z Up\n", ComputeDmElementIdStr( this ).Get(), ComputeDmElementIdStr( pDmeSequenceDst ).Get() );
		SubApply( pDmeDag, pDmeChannelsClipDst, true );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Find the translate & rotate channels and logs which refer to the specified
// DmeDag
//-----------------------------------------------------------------------------
static bool GetDmeChannelsForDag(
	CDmeTypedLog< Vector > *&pDmeTranslateLog,
	CDmeTypedLog< Quaternion > *&pDmeRotateLog,
	CDmeDag *pDmeDag,
	CDmeChannelsClip *pDmeChannelsClip )
{
	pDmeTranslateLog = NULL;
	pDmeRotateLog = NULL;

	if ( !pDmeDag || !pDmeChannelsClip )
		return false;

	CDmeTransform *pDmeTransform = pDmeDag->GetTransform();
	if ( !pDmeTransform )
		return false;

	for ( int i = 0; pDmeChannelsClip->m_Channels.Count(); ++i )
	{
		CDmeChannel *pDmeChannel = pDmeChannelsClip->m_Channels[i];
		if ( !pDmeChannel )
			continue;

		if ( pDmeChannel->GetToElement() != pDmeTransform )
			continue;

		CDmeLog *pDmeLog = pDmeChannel->GetLog();
		if ( !pDmeLog )
			continue;

		CDmeTypedLog< Vector > *pDmeTranslateLogTmp = CastElement< CDmeTypedLog< Vector > >( pDmeLog );
		if ( pDmeTranslateLogTmp )
		{
			if ( pDmeTranslateLog == NULL )
			{
				pDmeTranslateLog = pDmeTranslateLogTmp;
				// Quit if we've found both translate & rotate
				if ( pDmeRotateLog )
					break;
			}
			else
			{
				Log_Warning( LOG_DME_AAC, "%s: Multiple Translate Channels Found For Dag, Using %s, Ignoring %s\n",
					ComputeDmElementIdStr( pDmeDag ).Get(),
					ComputeDmElementIdStr( pDmeTranslateLog ).Get(),
					ComputeDmElementIdStr( pDmeChannel ).Get() );
			}

			continue;
		}

		CDmeTypedLog< Quaternion > *pDmeRotateLogTmp = CastElement< CDmeTypedLog< Quaternion > >( pDmeLog );
		if ( pDmeRotateLogTmp )
		{
			if ( pDmeRotateLog == NULL )
			{
				pDmeRotateLog = pDmeRotateLogTmp;
				// Quit if we've found both translate & rotate
				if ( pDmeTranslateLog )
					break;
			}
			else
			{
				Log_Warning( LOG_DME_AAC, "%s: Multiple Rotate Channels Found For Dag, Using %s, Ignoring %s\n",
					ComputeDmElementIdStr( pDmeDag ).Get(),
					ComputeDmElementIdStr( pDmeRotateLog ).Get(),
					ComputeDmElementIdStr( pDmeChannel ).Get() );
			}
			continue;
		}
	}

	return pDmeTranslateLog != NULL && pDmeRotateLog != NULL;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void ComputeMergedKeyTimes(
	CUtlVector< DmeTime_t > &mergedTimes,
	CDmeLog *pDmeLogA,
	CDmeLog *pDmeLogB )
{
	mergedTimes.RemoveAll();

	CUtlRBTree< DmeTime_t > timesTree( CDefOps< DmeTime_t >::LessFunc );

	for ( int i = 0; i < 2; ++i )
	{
		CDmeLog *pDmeLog = ( i == 0 ) ? pDmeLogA : pDmeLogB;
		if ( !pDmeLog )
			continue;

		for ( int j = 0; j < pDmeLog->GetKeyCount(); ++j )
		{
			timesTree.InsertIfNotFound( pDmeLog->GetKeyTime( j ) );
		}
	}

	for ( CUtlRBTree< DmeTime_t >::IndexType_t i = timesTree.FirstInorder(); timesTree.IsValidIndex( i ); i = timesTree.NextInorder( i ) )
	{
		mergedTimes.AddToTail( timesTree.Element( i ) );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
template < class T >
static void GetAllKeys(
	CUtlVector< DmeTime_t > &keyTimes,
	CUtlVector< T > &keyValues,
	CDmeTypedLog< T > *pDmeTypedLog )
{
	keyTimes.RemoveAll();
	keyValues.RemoveAll();

	const int nKeyCount = pDmeTypedLog->GetKeyCount();
	if ( nKeyCount <= 0 )
		return;

	keyTimes.EnsureCapacity( nKeyCount );
	keyValues.EnsureCapacity( nKeyCount );

	Assert( keyTimes.Count() == 0 );
	Assert( keyValues.Count() == 0 );

	for ( int i = 0; i < nKeyCount; ++i )
	{
		keyTimes.AddToTail( pDmeTypedLog->GetKeyTime( i ) );
		keyValues.AddToTail( pDmeTypedLog->GetKeyValue( i ) );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeRotateTo::SubApply( CDmeDag *pDmeDag, CDmeChannelsClip *pDmeChannelsClip, bool bZUp )
{
	if ( !pDmeDag || !pDmeChannelsClip )
		return;

	CDmeTransform *pDmeTransform = pDmeDag->GetTransform();
	if ( !pDmeTransform )
		return;

	CDmeTypedLog< Vector > *pDmeTranslateLog = NULL;
	CDmeTypedLog< Quaternion > *pDmeRotateLog = NULL;

	if ( !GetDmeChannelsForDag( pDmeTranslateLog, pDmeRotateLog, pDmeDag, pDmeChannelsClip ) )
	{
		Log_Error( LOG_DME_AAC, "%s: Couldn't Find Translate & Rotate channels for DmeDag %s\n",
			ComputeDmElementIdStr( this ).Get(),
			ComputeDmElementIdStr( pDmeDag ).Get() );

		return;
	}

	Assert( pDmeTranslateLog && pDmeRotateLog );

	matrix3x4_t mRoot;

	{
		float flDeltaAngle = 0.0f;

		const DmeTime_t dmeTimeBegin = pDmeTranslateLog->GetBeginTime();
		const DmeTime_t dmeTimeEnd = pDmeTranslateLog->GetEndTime();

		const Vector vPos( pDmeTranslateLog->GetValue( dmeTimeEnd ) - pDmeTranslateLog->GetValue( dmeTimeBegin ) );

		// TODO: Handle Y/Z Up

		if ( bZUp )
		{
			// ZUp
			if ( vPos.x != 0.0f || vPos.y != 0.0f )
			{
				const float flAngle = atan2( vPos.y, vPos.x ) * ( 180.0f / M_PI );
				flDeltaAngle = m_flAngle.Get() - flAngle;
			}

			AngleMatrix( QAngle( 0.0f, flDeltaAngle, 0.0f ), mRoot );
		}
		else
		{
			// YUp
			if ( vPos.x != 0.0f || vPos.z != 0.0f )
			{
				const float flAngle = atan2( vPos.x, vPos.z ) * ( 180.0f / M_PI );
				flDeltaAngle = m_flAngle.Get() - flAngle;
			}

			AngleMatrix( QAngle( flDeltaAngle, 0.0f, 0.0f ), mRoot );
		}
	}

	matrix3x4_t mSrc;
	matrix3x4_t mDst;

	CUtlVector< DmeTime_t > mergedKeyTimes;
	ComputeMergedKeyTimes( mergedKeyTimes, pDmeTranslateLog, pDmeRotateLog );

	Vector vTmp;
	CUtlVector< Vector > vValues;

	Quaternion qTmp;
	CUtlVector< Quaternion > qValues;

	for ( int i = 0; i < mergedKeyTimes.Count(); ++i )
	{
		const DmeTime_t &dmeTime = mergedKeyTimes[i];
		AngleMatrix( RadianEuler( pDmeRotateLog->GetValue( dmeTime ) ), pDmeTranslateLog->GetValue( dmeTime ), mSrc );
		ConcatTransforms( mRoot, mSrc, mDst );
		MatrixAngles( mDst, qTmp, vTmp );
		vValues.AddToTail( vTmp );
		qValues.AddToTail( qTmp );
	}

	CDmeTypedLogLayer< Vector > *pDmeTranslateLayer = CastElement< CDmeTypedLogLayer< Vector > >( pDmeTranslateLog->AddNewLayer() );
	if ( pDmeTranslateLayer )
	{
		pDmeTranslateLayer->SetAllKeys( mergedKeyTimes, vValues );
		pDmeTranslateLayer->RemoveRedundantKeys( true );
	}
	else
	{
		Log_Error( LOG_DME_AAC, "%s: Couldn't Create Translate Layer\n", ComputeDmElementIdStr( this ).Get() );
	}

	CDmeTypedLogLayer< Quaternion > *pDmeRotateLayer = CastElement< CDmeTypedLogLayer< Quaternion > >( pDmeRotateLog->AddNewLayer() );
	if ( pDmeRotateLayer )
	{
		pDmeRotateLayer->SetAllKeys( mergedKeyTimes, qValues );
		pDmeRotateLayer->RemoveRedundantKeys( true );
	}
	else
	{
		Log_Error( LOG_DME_AAC, "%s: Couldn't Create Rotate Layer\n", ComputeDmElementIdStr( this ).Get() );
	}
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBoneMaskCmd, CDmeBoneMaskCmd );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneMaskCmd::OnConstruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneMaskCmd::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeBoneMaskCmd::Apply( CDmElement *pDmElement )
{
	CDmeSequence *pDmeSequenceDst = NULL;
	CDmeChannelsClip *pDmeChannelsClipDst = NULL;

	if ( !ConvertToDmeSequenceAndDmeChannelsClip( pDmeSequenceDst, pDmeChannelsClipDst, pDmElement, ComputeDmElementIdStr( this ) ) )
		return false;

	CDmeDag *pDmeDag = pDmeSequenceDst->m_eSkeleton.GetElement();
	if ( !pDmeDag )
	{
		Log_Error( LOG_DME_AAC, "%s: Specified Sequence %s Has No Skeleton\n", ComputeDmElementIdStr( this ).Get(), ComputeDmElementIdStr( pDmeSequenceDst ).Get() );
		return false;
	}

	CDmeBoneMask *pDmeBoneMask = pDmeSequenceDst->m_eBoneMask.GetElement();
	if ( !pDmeBoneMask )
	{
		Log_Error( LOG_DME_AAC, "%s: Specified Sequence %s Has No Bone Mask\n", ComputeDmElementIdStr( this ).Get(), ComputeDmElementIdStr( pDmeSequenceDst ).Get() );
		return false;
	}

	CUtlStack< CDmeDag * > depthFirstStack;
	depthFirstStack.Push( pDmeDag );

	while ( depthFirstStack.Count() )
	{
		depthFirstStack.Pop( pDmeDag );
		if ( !pDmeDag )
			continue;

		for ( int i = pDmeDag->GetChildCount() - 1; i >= 0; --i )
		{
			depthFirstStack.Push( pDmeDag->GetChild( i ) );
		}

		SubApply( pDmeChannelsClipDst, pDmeDag, pDmeBoneMask );
	}

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneMaskCmd::SubApply( CDmeChannelsClip *pDmeChannelsClip, CDmeDag *pDmeDag, CDmeBoneMask *pDmeBoneMask )
{
	if ( !pDmeChannelsClip || !pDmeDag || !pDmeBoneMask )
		return;

	const float flWeight = pDmeBoneMask->GetBoneWeight( pDmeDag->GetName() );

	// Nothing to do if full weight
	if ( flWeight == 1.0f )
		return;

	CDmeTransform *pDmeTransform = pDmeDag->GetTransform();
	if ( !pDmeTransform )
		return;

	CDmeTypedLog< Vector > *pDmeTranslateLog = NULL;
	CDmeTypedLog< Quaternion > *pDmeRotateLog = NULL;

	if ( !GetDmeChannelsForDag( pDmeTranslateLog, pDmeRotateLog, pDmeDag, pDmeChannelsClip ) )
	{
		Log_Error( LOG_DME_AAC, "%s: Couldn't Find Translate & Rotate channels for DmeDag %s\n",
			ComputeDmElementIdStr( this ).Get(),
			ComputeDmElementIdStr( pDmeDag ).Get() );

		return;
	}

	Vector vTmp;
	CUtlVector< DmeTime_t > vTimes;
	CUtlVector< Vector > vValues;

	GetAllKeys( vTimes, vValues, pDmeTranslateLog );

	for ( int i = 0; i < vValues.Count(); ++i )
	{
		VectorScale( vValues[i], flWeight, vTmp );
		vValues[i] = vTmp;
	}

	Quaternion qTmp;
	CUtlVector< DmeTime_t > qTimes;
	CUtlVector< Quaternion > qValues;
	
	GetAllKeys( qTimes, qValues, pDmeRotateLog );

	for ( int i = 0; i < qValues.Count(); ++i )
	{
		QuaternionScale( qValues[i], flWeight, qTmp );
		qValues[i] = qTmp;
	}

	CDmeTypedLogLayer< Vector > *pDmeTranslateLayer = CastElement< CDmeTypedLogLayer< Vector > >( pDmeTranslateLog->AddNewLayer() );
	if ( pDmeTranslateLayer )
	{
		pDmeTranslateLayer->SetAllKeys( vTimes, vValues );
		pDmeTranslateLayer->RemoveRedundantKeys( true );
	}
	else
	{
		Log_Error( LOG_DME_AAC, "%s: Couldn't Create Translate Layer\n", ComputeDmElementIdStr( this ).Get() );
	}

	CDmeTypedLogLayer< Quaternion > *pDmeRotateLayer = CastElement< CDmeTypedLogLayer< Quaternion > >( pDmeRotateLog->AddNewLayer() );
	if ( pDmeRotateLayer )
	{
		pDmeRotateLayer->SetAllKeys( qTimes, qValues );
		pDmeRotateLayer->RemoveRedundantKeys( true );
	}
	else
	{
		Log_Error( LOG_DME_AAC, "%s: Couldn't Create Rotate Layer\n", ComputeDmElementIdStr( this ).Get() );
	}
}