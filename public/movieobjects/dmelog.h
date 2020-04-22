//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMELOG_H
#define DMELOG_H
#ifdef _WIN32
#pragma once
#endif

#include <math.h>
#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"
#include "datamodel/dmehandle.h"
#include "datamodel/dmattributevar.h"
#include "interpolatortypes.h"
#include "movieobjects/dmetimeselectiontimes.h"
#include "movieobjects/proceduralpresets.h"

class IUniformRandomStream;

template < class T > class CDmeTypedLog;

enum
{
	FILTER_SMOOTH = 0,
	FILTER_JITTER,
	FILTER_SHARPEN,
	FILTER_SOFTEN,
	FILTER_INOUT,

	NUM_FILTERS
};

enum RecordingMode_t
{
	RECORD_PRESET = 0,  // Preset/fader slider being dragged
	RECORD_ATTRIBUTESLIDER,  // Single attribute slider being dragged
};


// Transform write mode types, specify how a transform is to apply to existing values.
enum TransformWriteMode_t
{
	TRANSFORM_WRITE_MODE_HOLD,
	TRANSFORM_WRITE_MODE_OFFSET,
	TRANSFORM_WRITE_MODE_OVERWRITE,
	TRANSFORM_WRITE_MODE_TRANSFORM
};

enum LogComponents_t
{
	LOG_COMPONENTS_NONE	= 0,
	LOG_COMPONENTS_X	= ( 1 << 0 ),
	LOG_COMPONENTS_Y	= ( 1 << 1 ),
	LOG_COMPONENTS_Z	= ( 1 << 2 ),
	LOG_COMPONENTS_ALL	= LOG_COMPONENTS_X | LOG_COMPONENTS_Y | LOG_COMPONENTS_Z,
};
DEFINE_ENUM_BITWISE_OPERATORS( LogComponents_t )


enum SegmentInterpolation_t
{
	SEGMENT_INTERPOLATE,
	SEGMENT_NOINTERPOLATE,
};


static const unsigned int LOG_MAX_COMPONENTS = 4;


#define DMELOG_DEFAULT_THRESHHOLD	0.0001f

class DmeLog_TimeSelection_t
{
public:
	DmeLog_TimeSelection_t() :
		m_flIntensity( 1.0f ),
		m_bAttachedMode( true ),
		m_bTimeAdvancing( false ),
		m_nResampleInterval( DmeTime_t( .05f ) ),// 50 msec sampling interval by default
		m_flThreshold( DMELOG_DEFAULT_THRESHHOLD ),
		m_pPresetValue( 0 ),
		m_pPresetTimes( 0 ),
		m_pOldHeadValue( 0 ),
		m_OldHeadValueIndex( -1 ),
		m_tHeadPosition( 0 ),
		m_TransformWriteMode( TRANSFORM_WRITE_MODE_OVERWRITE ),
		m_bManipulateInFalloff( false ),
		m_nComponentFlags( LOG_COMPONENTS_ALL ),
		m_RecordingMode( RECORD_PRESET )
	{
		m_nTimes[ TS_LEFT_FALLOFF ] = m_nTimes[ TS_LEFT_HOLD ] = 
			m_nTimes[ TS_RIGHT_HOLD ] = m_nTimes[ TS_RIGHT_FALLOFF ] = DmeTime_t( 0 );
		m_nFalloffInterpolatorTypes[ 0 ] = m_nFalloffInterpolatorTypes[ 1 ] = INTERPOLATE_LINEAR_INTERP;
		m_bInfinite[ 0 ] = m_bInfinite[ 1 ] = false;
	}

	inline void	ResetTimeAdvancing()
	{
		// Reset the time advancing flag
		m_bTimeAdvancing = false;
	}

	inline void	StartTimeAdvancing()
	{
		m_bTimeAdvancing = true;
	}

	inline bool	IsTimeAdvancing() const
	{
		return m_bTimeAdvancing;
	}

	inline RecordingMode_t GetRecordingMode() const
	{
		return m_RecordingMode;
	}

	void SetRecordingMode( RecordingMode_t mode )
	{
		m_RecordingMode = mode;
	}

	float			GetAmountForTime( DmeTime_t curtime ) const;
	float			AdjustFactorForInterpolatorType( float factor, int side ) const;

	// NOTE: See TimeSelectionTimes_t for return values, 0 means before, 1= left falloff, 2=hold, 3=right falloff, 4=after
	int				ComputeRegionForTime( DmeTime_t curtime ) const;

	TimeSelection_t			m_nTimes;
	int						m_nFalloffInterpolatorTypes[ 2 ];
	DmeTime_t				m_nResampleInterval;
	float					m_flIntensity;				// How much to drive values toward m_HeadValue (generally 1.0f)
	float					m_flThreshold;
	const CDmAttribute*		m_pPresetValue;				// Pointer to the attribute storing the value to be used for presets
	const CDmAttribute*		m_pPresetTimes;				// Pointer to the attribute storing the times to be used for animated presets
	CDmAttribute*			m_pOldHeadValue;			// Pointer to the attribute storing the original head value
	int						m_OldHeadValueIndex;		// Array index of the original head value within the specified attribute
	DmeTime_t				m_tHeadPosition;			// Time position of the head
	TransformWriteMode_t	m_TransformWriteMode;		// Specification of how values are to be written into the log with respect to the existing values
	bool					m_bAttachedMode : 1;		// Is the current time "attached" to the head position
	bool					m_bManipulateInFalloff : 1;	// Should the rotation be applied as a transform to the position instead of interpolating
	LogComponents_t			m_nComponentFlags;			// Flag indicating which components of the log should be modified
	bool					m_bInfinite[ 2 ];

private:
	bool					m_bTimeAdvancing : 1; // Has time ever been advancing
	RecordingMode_t			m_RecordingMode;
};

class CDmeChannel;
class CDmeChannelsClip;
class CDmeClip;
class CDmeFilmClip;
class CDmeLog;
class CDmeLogLayer;
struct DmeClipStack_t;

struct LayerSelectionData_t
{
	LayerSelectionData_t();
	void Release();

	CDmeHandle< CDmeChannel >		m_hChannel;
	CDmeHandle< CDmeChannelsClip >	m_hOwner;
	CDmeHandle< CDmeFilmClip >		m_hShot;
	CDmeHandle< CDmeLog >			m_hLog;
	DmAttributeType_t				m_DataType;
	DmeTime_t						m_nTimes[ TS_TIME_COUNT ];

	// This is dynamic and needs to be released
	struct DataLayer_t
	{
		DataLayer_t( float frac, CDmeLogLayer *layer );
		
		float m_flStartFraction;
		CDmeHandle< CDmeLogLayer, HT_STRONG > m_hData;
	};

	CUtlVector< DataLayer_t >		m_vecData;
};

struct DmeLogTransformParams_t
{
	DmeLogTransformParams_t()
		: m_RotationLocal( quat_identity )
		, m_RotationParent( quat_identity )
		, m_Pivot( vec3_origin )
		, m_nProceduralType( PROCEDURAL_PRESET_NOT )
		, m_pRotationLog( NULL )
	{
		SetIdentityMatrix( m_Transform );
	}

	matrix3x4_t								m_Transform;
	Quaternion								m_RotationLocal;
	Quaternion								m_RotationParent;
	Vector									m_Pivot;
	int										m_nProceduralType;
	CDmeTypedLog< Quaternion >				*m_pRotationLog;
};

//-----------------------------------------------------------------------------
// CDmeLogLayer - abstract base class
//-----------------------------------------------------------------------------
abstract_class CDmeLogLayer : public CDmElement
{
	friend class CDmeLog;

	DEFINE_ELEMENT( CDmeLogLayer, CDmElement );

public:
	virtual void CopyLayer( const CDmeLogLayer *src ) = 0;
	virtual void CopyPartialLayer( const CDmeLogLayer *src, DmeTime_t startTime, DmeTime_t endTime, bool bRebaseTimestamps ) = 0;
	virtual void ExplodeLayer( const CDmeLogLayer *src, DmeTime_t startTime, DmeTime_t endTime, bool bRebaseTimestamps, DmeTime_t tResampleInterval ) = 0;
	virtual void InsertKeyFromLayer( DmeTime_t keyTime, const CDmeLogLayer *src, DmeTime_t srcKeyTime ) = 0;

	DmeTime_t GetBeginTime( bool bAllowInfinite ) const;
	DmeTime_t GetEndTime( bool bAllowInfinite ) const;
	int GetKeyCount() const;

	void ScaleSampleTimes( float scale );

	// Returns the index of a key closest to this time, within tolerance
	// NOTE: Insertion or removal may change this index!
	// Returns -1 if the time isn't within tolerance.
	int FindKeyWithinTolerance( DmeTime_t time, DmeTime_t nTolerance );
	
	// Returns the type of attribute being logged
	virtual DmAttributeType_t GetDataType() const = 0;

	// Sets a key, removes all keys after this time
	virtual void SetKey( DmeTime_t time, const CDmAttribute *pAttr, uint index = 0, SegmentInterpolation_t interpSetting = SEGMENT_INTERPOLATE, int curveType = CURVE_DEFAULT ) = 0;
	virtual bool SetDuplicateKeyAtTime( DmeTime_t time ) = 0;
	// This inserts a key using the current values to construct the proper value for the time
	virtual int InsertKeyAtTime( DmeTime_t nTime, int curveType = CURVE_DEFAULT ) = 0;
	virtual void TrimKeys( DmeTime_t tStartTime, DmeTime_t tEndTime ) = 0;

	// Sets the interpolated value of the log at the specified time into the attribute
	virtual void GetValue( DmeTime_t time, CDmAttribute *pAttr, uint index = 0 ) const = 0;

	virtual float GetComponent( DmeTime_t time, int componentIndex ) const = 0;

	// Returns the time at which a particular key occurs
	DmeTime_t GetKeyTime( int nKeyIndex ) const;
	void	SetKeyTime( int nKeyIndex, DmeTime_t keyTime );

	// Scale + bias key times
	void ScaleBiasKeyTimes( double flScale, DmeTime_t nBias );

	// Scale the keys within the source time selection to fill the destination time selection, keys outside the time selection will be shifted 
	void RescaleSamplesInTimeSelection( const TimeSelection_t &srcTimeSeleciton, const TimeSelection_t &dstTimeSelection );

	// Removes a single key	by index
	virtual void RemoveKey( int nKeyIndex, int nNumKeysToRemove = 1 ) = 0;

	// Removes all keys
	virtual void ClearKeys() = 0;

	virtual bool IsConstantValued() const = 0;
	virtual void RemoveRedundantKeys( bool bKeepEnds ) = 0;
	virtual void RemoveRedundantKeys( float threshold, bool bKeepEnds ) = 0;

	// resampling and filtering
	virtual void Resample( DmeFramerate_t samplerate ) = 0;
	virtual void Filter( int nSampleRadius ) = 0;
	virtual void Filter2( DmeTime_t sampleRadius ) = 0;

	virtual void SetOwnerLog( CDmeLog *owner ) = 0;
	      CDmeLog *GetOwnerLog();
	const CDmeLog *GetOwnerLog() const;

	bool			IsUsingCurveTypes() const;
	int				GetDefaultCurveType() const;

	// Override curvetype for specific key
	void			SetKeyCurveType( int nKeyIndex, int curveType );
	int				GetKeyCurveType( int nKeyIndex ) const;

	// Layers may extend to infinity on the left or right, if so the start / end time will be min / max
	void			SetInfinite( bool bLeftInfinite, bool bRightInfinite );
	bool			IsLeftInfinite() { return m_bLeftInfinite; }
	bool			IsRightInfinite() { return m_bRightInfinite; }

	// Validates that all keys are correctly sorted in time
	bool			ValidateKeys() const;

	// Removes all keys outside the specified time range
	void			RemoveKeysOutsideRange( DmeTime_t tStart, DmeTime_t tEnd );
	
	SegmentInterpolation_t			GetSegmentInterpolationSetting( int nKeyIndex ) const;
	SegmentInterpolation_t			GetSegmentInterpolationSetting( int nStartKeyIndex, int nEndKeyIndex ) const; //return SEGMENT_NOINTERPOLATE if any segment checked is non-interpolated
	SegmentInterpolation_t			GetSegmentInterpolationSetting( DmeTime_t time ) const;
	SegmentInterpolation_t			GetSegmentInterpolationSetting( DmeTime_t startTime, DmeTime_t endTime, bool bExcludeActualEndTimeKey ) const; //return SEGMENT_NOINTERPOLATE if any segment checked is non-interpolated

	// Masks all keys within the time range, returns true if keys were modified
	virtual bool MaskKeyRange( DmeTime_t tStartTime, DmeTime_t tEndTime, LogComponents_t nComponentFlags, bool bInfiniteLeft = false, bool bInfiniteRight = false ) = 0;
	virtual void MakeRoomForSamplesMaskedSubcomponents( CDmeLogLayer *pBaseLayer, DmeTime_t tStart, DmeTime_t tEnd, DmeTime_t tLeftShift, DmeTime_t tRightShift, LogComponents_t nComponents ) = 0; 

	virtual void Compress() = 0;
	virtual void Decompress() = 0;

	virtual bool IsCompressed() const = 0;
	virtual size_t GetCompressedSize() const = 0;
	virtual size_t GetDataSize() const = 0;

protected:
	int FindKey( DmeTime_t time ) const;

	void OnUsingCurveTypesChanged();

	CDmeLog *m_pOwnerLog;

	mutable int m_lastKey;
	bool m_bLeftInfinite;
	bool m_bRightInfinite;
	CDmaArray< DmeTime_t > m_times;
	CDmaArray< int > m_CurveTypes;
	CDmaArray< bool > m_NonInterpolatedSegments;
};

template< class T >
CDmeLogLayer *CreateLayer( CDmeTypedLog< T > *ownerLog );


//-----------------------------------------------------------------------------
// CDmeLogLayer - abstract base class
//-----------------------------------------------------------------------------
abstract_class CDmeCurveInfo : public CDmElement
{
	DEFINE_ELEMENT( CDmeCurveInfo, CDmElement );

public:
	// Global override for all keys unless overriden by specific key
	void			SetDefaultCurveType( int curveType );
	int				GetDefaultCurveType() const;

	void	SetMinValue( float val );
	float	GetMinValue() const;
	void	SetMaxValue( float val );
	float	GetMaxValue() const;

protected:
	CDmaVar< int >		m_DefaultCurveType;

	CDmaVar< float >	m_MinValue;
	CDmaVar< float >	m_MaxValue;
};

template <class T > class CDmeTypedLogLayer;

//-----------------------------------------------------------------------------
// CDmeLog - abstract base class
//-----------------------------------------------------------------------------
abstract_class CDmeLog : public CDmElement
{
	DEFINE_ELEMENT( CDmeLog, CDmElement );

public:
	int FindLayerForTime( DmeTime_t time ) const;
	int FindLayerForTimeSkippingTopmost( DmeTime_t time ) const;
	int FindLayerForTimeBelowLayer( DmeTime_t time, int topLayerIndex ) const;
	void FindLayersForTime( DmeTime_t time, CUtlVector< int >& list ) const;

	virtual void		FinishTimeSelection( DmeTime_t tHeadPosition, DmeLog_TimeSelection_t& params ) = 0; // in attached, time advancing mode, we need to blend out of the final sample over the fadeout interval
	virtual void		StampKeyAtHead( DmeTime_t tHeadPosition, DmeTime_t tPreviousHeadPosition, const DmeLog_TimeSelection_t& params, const DmeLogTransformParams_t &transformParams, const CDmAttribute *pAttr, uint arrayIndex = 0, bool bTimeFilter = true, int layerIndex = -1 ) = 0;
	virtual void		FilterUsingTimeSelection( IUniformRandomStream &random, float flScale, const DmeLog_TimeSelection_t& params, int filterType, bool bResample, bool bApplyFalloff, const CDmeLogLayer *baseLayer, CDmeLogLayer *writeLayer  ) = 0;
	virtual void		FilterUsingTimeSelection( IUniformRandomStream &random, const DmeLog_TimeSelection_t& params, int filterType, bool bResample, bool bApplyFalloff ) = 0;
	virtual void		StaggerUsingTimeSelection( const DmeLog_TimeSelection_t& params, DmeTime_t tStaggerAmount, const CDmeLogLayer *baseLayer, CDmeLogLayer *writeLayer ) = 0;
	virtual void		RevealUsingTimeSelection( const DmeLog_TimeSelection_t &params, const CDmeLogLayer *pTargetLayer ) = 0;
	virtual void		RecaleAndRevealUsingTimeSelection( const DmeLog_TimeSelection_t &params, TimeSelection_t &sourceTimeSelection, const CDmeLogLayer *pTargetLayer ) = 0;
	virtual void		GenerateSplineUsingTimeSelection( const DmeLog_TimeSelection_t& params, const CUtlVector< DmeTime_t > &sortedSplineKeyTimes, const CDmeLogLayer *baseLayer, CDmeLogLayer *writeLayer ) = 0;
	virtual void		BlendLayersUsingTimeSelection( const DmeLog_TimeSelection_t &params, int baseLayer = 0 ) = 0;
	virtual void		BlendLayersUsingTimeSelection( const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, bool bUseBaseLayerSamples, bool bUseFalloff, bool bSelectionSamples, DmeTime_t tStartOffset ) = 0;
	virtual void		BlendLayersUsingTimeSelection( const CDmeLogLayer *baseLayer, const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, bool bUseBaseLayerSamples, DmeTime_t tStartOffset ) = 0;
	virtual void		BlendTimesUsingTimeSelection( const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, DmeTime_t tStartOffset, bool bFeatherBlendInFalloff ) = 0;
	virtual void		PasteAndRescaleSamples( const CDmeLogLayer *src, const DmeLog_TimeSelection_t& srcParams, const DmeLog_TimeSelection_t& destParams, bool bBlendAreaInFalloffRegion, bool bReverse ) = 0;
	virtual void		PasteAndRescaleSamples( const CDmeLogLayer *pBaseLayer, const CDmeLogLayer *pDataLayer, CDmeLogLayer *pOutputLayer, const DmeLog_TimeSelection_t& srcParams, const DmeLog_TimeSelection_t& destParams, bool bBlendAreaInFalloffRegion, bool bReverse ) = 0;
	virtual void		BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer ) = 0;
	virtual void		BuildCorrespondingLayer( const CDmeLogLayer *pReferenceLayer, const CDmeLogLayer *pDataLayer, CDmeLogLayer *pOutputLayer ) = 0;
	virtual void		HoldOrReleaseUsingTimeSelection( const DmeLog_TimeSelection_t& params, bool bHold, const CDmeLogLayer *pBaseLayer, CDmeLogLayer *pWriteLayer ) = 0;
	virtual void		SteadyUsingTimeSelection( const DmeLog_TimeSelection_t& params, const CDmeLogLayer *pBaseLayer, CDmeLogLayer *pWriteLayer ) = 0;
	virtual void		CopySamplesFromPreset( const DmeLog_TimeSelection_t& params, const CDmAttribute *pPresetValue, const CDmAttribute *pPresetTimes, DmeTime_t tLogTimeOffset, const CDmeChannelsClip *pChannelsClip, const CDmeLogLayer *pBaseLayer, CDmeLogLayer *pWriteLayer ) = 0; // preset samples are in shot time, not log time


	int GetTopmostLayer() const;
	int	GetNumLayers() const;
	CDmeLogLayer *GetLayer( int index );
	const CDmeLogLayer *GetLayer( int index ) const;

	DmeTime_t GetBeginTime() const;
	DmeTime_t GetEndTime() const;
	int GetKeyCount() const;
	bool	IsEmpty() const;

	void ScaleSampleTimes( float scale );
	virtual void ClearAndAddSampleAtTime( DmeTime_t time ) = 0;

	// Returns the index of a key closest to this time, within tolerance
	// NOTE: Insertion or removal may change this index!
	// Returns -1 if the time isn't within tolerance.
	virtual int FindKeyWithinTolerance( DmeTime_t time, DmeTime_t nTolerance ) = 0;
	
	// Returns the type of attribute being logged
	virtual DmAttributeType_t GetDataType() const = 0;

	
	// Sets a key, removes all keys after this time
	virtual void SetKey( DmeTime_t time, const CDmAttribute *pAttr, uint index = 0, SegmentInterpolation_t interpSetting = SEGMENT_INTERPOLATE, int curveType = CURVE_DEFAULT ) = 0;
	virtual bool SetDuplicateKeyAtTime( DmeTime_t time ) = 0;
	virtual int InsertKeyAtTime( DmeTime_t nTime, int curveType = CURVE_DEFAULT ) = 0;

	// Sets the interpolated value of the log at the specified time into the attribute
	virtual void GetValue( DmeTime_t time, CDmAttribute *pAttr, uint index = 0 ) const = 0;
	virtual void GetValueSkippingTopmostLayer( DmeTime_t time, CDmAttribute *pAttr, uint index = 0 ) const = 0;

	virtual float GetComponent( DmeTime_t time, int componentIndex ) const = 0;

	// Returns the time at which a particular key occurs
	virtual DmeTime_t GetKeyTime( int nKeyIndex ) const = 0;
	virtual void	SetKeyTime( int nKeyIndex, DmeTime_t keyTime ) = 0;

	// Override curvetype for specific key
	void			SetKeyCurveType( int nKeyIndex, int curveType );
	int				GetKeyCurveType( int nKeyIndex ) const;

	// Removes a single key	by index
	virtual void RemoveKey( int nKeyIndex, int nNumKeysToRemove = 1 ) = 0;

	// Removes all keys within the time range, returns true if keys were removed
	bool RemoveKeys( DmeTime_t tStartTime, DmeTime_t tEndTime );

	// Add keys at tStartTime and tEndTime, and remove all keys outside the range
	void TrimKeys( DmeTime_t tStartTime, DmeTime_t tEndTime );

	// Removes all keys
	virtual void ClearKeys() = 0;

	// Scale + bias key times
	void ScaleBiasKeyTimes( double flScale, DmeTime_t nBias );

	virtual bool IsConstantValued() const = 0;
	virtual void RemoveRedundantKeys( bool bKeepEnds ) = 0;
	virtual void RemoveRedundantKeys( float threshold, bool bKeepEnds ) = 0;

	// resampling and filtering
	virtual void Resample( DmeFramerate_t samplerate ) = 0;
	virtual void Filter( int nSampleRadius ) = 0;
	virtual void Filter2( DmeTime_t sampleRadius ) = 0;

	// Creates a log of a requested type
	static CDmeLog *CreateLog( DmAttributeType_t type, DmFileId_t fileid );

	virtual CDmeLogLayer *AddNewLayer() = 0;
	enum
	{
		FLATTEN_NODISCONTINUITY_FIXUP = (1<<0), // Don't add "helper" samples to preserve discontinuities.  This occurs when the time selection is "detached" from the head position
		FLATTEN_SPEW				  = (1<<1),
	};
	virtual void		FlattenLayers( float threshold, int flags, int baseLayer = 0 ) = 0;

	// Only used by undo system!!!
	virtual void		AddLayerToTail( CDmeLogLayer *layer ) = 0;
	virtual CDmeLogLayer *RemoveLayerFromTail() = 0;
	virtual CDmeLogLayer *RemoveLayer( int iLayer ) = 0;


	// Resolve
	virtual void Resolve();

	// curve info helpers
	bool IsUsingCurveTypes() const;
	const CDmeCurveInfo *GetCurveInfo() const;
	      CDmeCurveInfo *GetCurveInfo();
	virtual CDmeCurveInfo *GetOrCreateCurveInfo() = 0;
	virtual void SetCurveInfo( CDmeCurveInfo *pCurveInfo ) = 0;

	// accessors for CurveInfo data
	int GetDefaultCurveType() const;

	// FIXME - this should really be in the CurveInfo
	//         but the animset editor currently asks for these, without having set a curveinfo...
	void			SetMinValue( float val );
	void			SetMaxValue( float val );
	float			GetMinValue() const;
	float			GetMaxValue() const;

	virtual bool			HasDefaultValue() const = 0;


	// Bookmark functions
	virtual void	InitalizeBookmarkArrays() = 0;
	virtual int		GetNumBookmarkComponents() const = 0;
	int				GetNumBookmarks( int nComponentIndex ) const;
	DmeTime_t		GetBookmarkTime( int nBookmarkIndex, int nComponentIndex ) const;
	void			AddBookmark( DmeTime_t time, int nComponentIndex );
	bool			RemoveBookmark( DmeTime_t time, int nComponentIndex );
	void			RemoveAllBookmarks( int nComponentIndex );
	void			SetAllBookmarks( int nComponentIndex, const CUtlVector< DmeTime_t > &time );


	// Masks all keys within the time range, returns true if keys were modified
	virtual bool MaskKeyRange( DmeTime_t tStartTime, DmeTime_t tEndTime, LogComponents_t nComponentFlags, bool bInfiniteLeft = false, bool bInfiniteRight = false ) = 0;


protected:
//	int FindKey( DmeTime_t time ) const;

	void	OnUsingCurveTypesChanged();

	virtual void OnAttributeChanged( CDmAttribute *pAttribute );

	CDmaElementArray< CDmeLogLayer >	m_Layers;
	CDmaElement< CDmeCurveInfo > m_CurveInfo;
	CDmaArray< DmeTime_t >	m_BookmarkTimes[ LOG_MAX_COMPONENTS ];
};



//-----------------------------------------------------------------------------
// CDmeTypedCurveInfo - implementation class for all logs
//-----------------------------------------------------------------------------
template< class T >
class CDmeTypedCurveInfo : public CDmeCurveInfo
{
	DEFINE_ELEMENT( CDmeTypedCurveInfo, CDmeCurveInfo );

public:
	// For "faceposer" style left/right edges, this controls whether interpolators try to mimic faceposer left/right edge behavior
	void			SetUseEdgeInfo( bool state );
	bool			IsUsingEdgeInfo() const;

	void			SetEdgeInfo( int edge, bool active, const T& val, int curveType );
	void			GetEdgeInfo( int edge, bool& active, T& val, int& curveType ) const;

	void			SetDefaultEdgeZeroValue( const T& val );
	const T&		GetDefaultEdgeZeroValue() const;

	void			SetRightEdgeTime( DmeTime_t time );
	DmeTime_t		GetRightEdgeTime() const;

	bool			IsEdgeActive( int edge ) const;
	void			GetEdgeValue( int edge, T& value ) const;

	int				GetEdgeCurveType( int edge ) const;
	void			GetZeroValue( int side, T& val ) const;

protected:
	CDmaVar< bool >			m_bUseEdgeInfo;
	// Array of 2 for left/right edges...
	CDmaVar< bool >			m_bEdgeActive[ 2 ];
	CDmaVar< T >			m_EdgeValue[ 2 ];
	CDmaVar< int >			m_EdgeCurveType[ 2 ];
	CDmaTime				m_RightEdgeTime;
	CDmaVar< T >			m_DefaultEdgeValue;
};


// forward declaration
template< class T > class CDmeTypedLog;

template< typename T >
struct LogKeyValue_t
{
	T			value;
	DmeTime_t	time;
};

//-----------------------------------------------------------------------------
// CDmeTypedLogLayer - implementation class for all logs
//-----------------------------------------------------------------------------
template< class T >
class CDmeTypedLogLayer : public CDmeLogLayer
{
	DEFINE_ELEMENT( CDmeTypedLogLayer, CDmeLogLayer );

public:

	virtual void CopyLayer( const CDmeLogLayer *src );
	virtual void CopyPartialLayer( const CDmeLogLayer *src, DmeTime_t startTime, DmeTime_t endTime, bool bRebaseTimestamps );
	virtual void ExplodeLayer( const CDmeLogLayer *src, DmeTime_t startTime, DmeTime_t endTime, bool bRebaseTimestamps, DmeTime_t tResampleInterval );
	virtual void InsertKeyFromLayer( DmeTime_t keyTime, const CDmeLogLayer *src, DmeTime_t srcKeyTime );

	// Finds a key within tolerance, or adds one. Unlike SetKey, this will *not* delete keys after the specified time
	int FindOrAddKey( DmeTime_t nTime, DmeTime_t nTolerance, const T& value, SegmentInterpolation_t interpSetting = SEGMENT_INTERPOLATE, int curveType = CURVE_DEFAULT );

	// Sets a key, removes all keys after this time
	void SetKey( DmeTime_t time, const T& value, SegmentInterpolation_t interpSetting = SEGMENT_INTERPOLATE, int curveType = CURVE_DEFAULT, bool removeRedundant = true );

	// Sets all of the keys on the layer from the provided array of times and values
	void SetAllKeys( const CUtlVector< DmeTime_t > &times, const CUtlVector< T > &values );

	// Copy all of the keys into the specified array
	void GetAllKeys( CUtlVector< DmeTime_t > &times, CUtlVector< T > &values ) const;


	// This inserts a key using the current values to construct the proper value for the time
	virtual int InsertKeyAtTime( DmeTime_t nTime, int curveType = CURVE_DEFAULT );

	// Add keys at tStartTime and tEndTime, and remove all keys outside the range
	virtual void TrimKeys( DmeTime_t tStartTime, DmeTime_t tEndTime );

	void SetKeyValue( int nKey, const T& value );

	const T& GetValue( DmeTime_t time ) const;

	const T& GetKeyValue( int nKeyIndex ) const;
	const T& GetValueSkippingKey( int nKeyToSkip ) const;

	// Returns the key time / value pair for the specified key
	void GetKeyValue( int keyIndex, LogKeyValue_t< T > &keyValue ) const;


	// This inserts a key. Unlike SetKey, this will *not* delete keys after the specified time
	int InsertKey( DmeTime_t nTime, const T& value, SegmentInterpolation_t interpSetting = SEGMENT_INTERPOLATE, int curveType = CURVE_DEFAULT, bool bIgnoreTolerance = false );

	// inherited from CDmeLog
	virtual void ClearKeys();
	virtual void SetKey( DmeTime_t time, const CDmAttribute *pAttr, uint index = 0, SegmentInterpolation_t interpSetting = SEGMENT_INTERPOLATE, int curveType = CURVE_DEFAULT );
	virtual bool SetDuplicateKeyAtTime( DmeTime_t time );
	virtual void GetValue( DmeTime_t time, CDmAttribute *pAttr, uint index = 0 ) const;
	virtual float GetComponent( DmeTime_t time, int componentIndex ) const;
	virtual DmAttributeType_t GetDataType() const;
	virtual bool IsConstantValued() const;
	virtual void RemoveRedundantKeys( bool bKeepEnds );
	virtual void RemoveRedundantKeys( float threshold, bool bKeepEnds );

	virtual void RemoveKey( int nKeyIndex, int nNumKeysToRemove = 1 );
	virtual void Resample( DmeFramerate_t samplerate );
	virtual void Filter( int nSampleRadius );
	virtual void Filter2( DmeTime_t sampleRadius );

	void RemoveKeys( DmeTime_t starttime );

	// curve info helpers
	const CDmeTypedCurveInfo< T > *GetTypedCurveInfo() const;
	      CDmeTypedCurveInfo< T > *GetTypedCurveInfo();

	bool			IsUsingEdgeInfo() const;
	void			GetEdgeInfo( int edge, bool& active, T& val, int& curveType ) const;
	const T&		GetDefaultEdgeZeroValue() const;
	DmeTime_t		GetRightEdgeTime() const;

	void			SetOwnerLog( CDmeLog *owner );

	      CDmeTypedLog< T >	*GetTypedOwnerLog();
	const CDmeTypedLog< T >	*GetTypedOwnerLog() const;

	T	MaskValue( DmeTime_t time, const T& value, LogComponents_t componentFlags ) const;

	// Masks all keys within the time range, returns true if keys were modified
	virtual bool MaskKeyRange( DmeTime_t tStartTime, DmeTime_t tEndTime, LogComponents_t nComponentFlags, bool bInfiniteLeft = false, bool bInfiniteRight = false );
	virtual void MakeRoomForSamplesMaskedSubcomponents( CDmeLogLayer *pBaseLayer, DmeTime_t tStart, DmeTime_t tEnd, DmeTime_t tLeftShift, DmeTime_t tRightShift, LogComponents_t nComponents ); 

	virtual void Compress();
	virtual void Decompress();

	virtual bool IsCompressed() const;
	virtual size_t GetCompressedSize() const;

	virtual size_t GetDataSize() const;

protected:

	int GetEdgeCurveType( int edge ) const;
	void GetZeroValue( int side, T& val ) const;

	void GetValueUsingCurveInfo( DmeTime_t time, T& out ) const;
	void GetValueUsingCurveInfoSkippingKey( int nKeyToSkip, T& out ) const;
	void GetBoundedSample( int keyindex, DmeTime_t& time, T& val, int& curveType ) const;

	void CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< T > *output );

	friend CDmeTypedLog< T >;

	void GetTwoKeyValues( int keyindex, T &v1, T &v2 ) const;
	void GetCompressedValue( int nKeyIndex, T &value ) const;
	void GetCompressedValues( int nKeyIndex, T &value1, T &value2 ) const;
	void CompressValues( CDmaArray< T > &stream, CUtlBinaryBlock &block, float flMaxError = 0.1f );

protected:
	CDmaArray< T > m_values;
	// When compressed, m_values is empty and data is read from here
	CDmaVar< CUtlBinaryBlock > m_Compressed;
};


//-----------------------------------------------------------------------------
// CDmeTypedLog - implementation class for all logs
//-----------------------------------------------------------------------------
template< class T >
class CDmeTypedLog : public CDmeLog
{
	DEFINE_ELEMENT( CDmeTypedLog, CDmeLog );

public:


	virtual void OnAttributeArrayElementAdded( CDmAttribute *pAttribute, int nFirstElem, int nLastElem );

	CDmeTypedLogLayer< T > *GetLayer( int index );
	const CDmeTypedLogLayer< T > *GetLayer( int index ) const;

	void		StampKeyAtHead( DmeTime_t tHeadPosition, DmeTime_t tPreviousHeadPosition, const DmeLog_TimeSelection_t& params, const DmeLogTransformParams_t &transformParams, const T& value, bool bTimeFilter = true, int layerIndex = -1 );
	void		StampKeyAtHead( DmeTime_t tHeadPosition, DmeTime_t tPreviousHeadPosition, const DmeLog_TimeSelection_t& params, const DmeLogTransformParams_t &transformParams, const CDmAttribute *pAttr,  uint arrayIndex = 0, bool bTimeFilter = true, int logLayer = -1  );
	void		FinishTimeSelection( DmeTime_t tHeadPosition, DmeLog_TimeSelection_t& params ); // in attached, time advancing mode, we need to blend out of the final sample over the fadeout interval
	void		FilterUsingTimeSelection( IUniformRandomStream &random, float flScale, const DmeLog_TimeSelection_t& params, int filterType, bool bResample, bool bApplyFalloff, const CDmeLogLayer *baseLayer, CDmeLogLayer *writeLayer );
	void		FilterUsingTimeSelection( IUniformRandomStream &random, const DmeLog_TimeSelection_t& params, int filterType, bool bResample, bool bApplyFalloff );
	void		StaggerUsingTimeSelection( const DmeLog_TimeSelection_t& params, DmeTime_t tStaggerAmount, const CDmeLogLayer *baseLayer, CDmeLogLayer *writeLayer );
	void		RevealUsingTimeSelection( const DmeLog_TimeSelection_t &params, const CDmeLogLayer *pTargetLayer );
	void		RecaleAndRevealUsingTimeSelection( const DmeLog_TimeSelection_t &params, TimeSelection_t &sourceTimeSelection, const CDmeLogLayer *pTargetLayer );
	void		GenerateSplineUsingTimeSelection( const DmeLog_TimeSelection_t& params, const CUtlVector< DmeTime_t > &sortedSplineKeyTimes, const CDmeLogLayer *baseLayer, CDmeLogLayer *writeLayer );
	void		BlendLayersUsingTimeSelection( const DmeLog_TimeSelection_t &params, int baseLayer = 0 );
	void		BlendLayersUsingTimeSelection( const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, bool bUseBaseLayerSamples, bool bUseFalloff, bool bSelectionSamples, DmeTime_t tStartOffset );
	void		BlendLayersUsingTimeSelection( const CDmeLogLayer *baseLayer, const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, bool bUseBaseLayerSamples, DmeTime_t tStartOffset );
	void		BlendLayersUsingTimeSelection( const DmeLog_TimeSelection_t &params, const CDmeTypedLogLayer< T > *pBaseLayer, const CDmeTypedLogLayer< T > *pBlendLayer, CDmeTypedLogLayer< T > *pOutputLayer );
	void		BlendTimesUsingTimeSelection( const CDmeLogLayer *firstLayer, const CDmeLogLayer *secondLayer, CDmeLogLayer *outputLayer, const DmeLog_TimeSelection_t &params, DmeTime_t tStartOffset, bool bFeatherBlendInFalloff );
	void		HoldOrReleaseUsingTimeSelection( const DmeLog_TimeSelection_t& params, bool bHold, const CDmeLogLayer *pBaseLayer, CDmeLogLayer *pWriteLayer );
	void		SteadyUsingTimeSelection( const DmeLog_TimeSelection_t& params, const CDmeLogLayer *pBaseLayer, CDmeLogLayer *pWriteLayer );
	void		CopySamplesFromPreset( const DmeLog_TimeSelection_t& params, const CDmAttribute *pPresetValue, const CDmAttribute *pPresetTimes, DmeTime_t tLogTimeOffset, const CDmeChannelsClip *pChannelsClip, const CDmeLogLayer *pBaseLayer, CDmeLogLayer *pWriteLayer ); // preset samples are in shot time, not log time
	virtual void		PasteAndRescaleSamples( const CDmeLogLayer *src, const DmeLog_TimeSelection_t& srcParams, const DmeLog_TimeSelection_t& destParams, bool bBlendAreaInFalloffRegion, bool bReverse );
	virtual void		PasteAndRescaleSamples( const CDmeLogLayer *pBaseLayer, const CDmeLogLayer *pDataLayer, CDmeLogLayer *pOutputLayer, const DmeLog_TimeSelection_t& srcParams, const DmeLog_TimeSelection_t& destParams, bool bBlendAreaInFalloffRegion, bool bReverse );
	virtual void		BuildCorrespondingLayer( const CDmeLogLayer *pReferenceLayer, const CDmeLogLayer *pDataLayer, CDmeLogLayer *pOutputLayer );

	virtual void		BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer );

	// Finds a key within tolerance, or adds one. Unlike SetKey, this will *not* delete keys after the specified time
	int FindOrAddKey( DmeTime_t nTime, DmeTime_t nTolerance, const T& value, SegmentInterpolation_t interpSetting = SEGMENT_INTERPOLATE, int curveType = CURVE_DEFAULT );

	// Sets a key, removes all keys after this time
	void SetKey( DmeTime_t time, const T& value, SegmentInterpolation_t interpSetting = SEGMENT_INTERPOLATE, int curveType = CURVE_DEFAULT );
	int InsertKeyAtTime( DmeTime_t nTime, int curveType = CURVE_DEFAULT );
	bool ValuesDiffer( const T& a, const T& b ) const;
	const T& GetValue( DmeTime_t time ) const;
	const T& GetValueSkippingTopmostLayer( DmeTime_t time ) const;
	const T& GetValueBelowLayer( DmeTime_t time, int nTopLayerIndex ) const;

	const T& GetKeyValue( int nKeyIndex ) const;

	// This inserts a key. Unlike SetKey, this will *not* delete keys after the specified time
	int InsertKey( DmeTime_t nTime, const T& value, SegmentInterpolation_t interpSetting = SEGMENT_INTERPOLATE, int curveType = CURVE_DEFAULT, bool bIgnoreTolerance = false );

	// inherited from CDmeLog
	virtual void ClearKeys();
	virtual void SetKey( DmeTime_t time, const CDmAttribute *pAttr, uint index = 0, SegmentInterpolation_t interpSetting = SEGMENT_INTERPOLATE, int curveType = CURVE_DEFAULT );
	virtual bool SetDuplicateKeyAtTime( DmeTime_t time );
	virtual void GetValue( DmeTime_t time, CDmAttribute *pAttr, uint index = 0 ) const;
	virtual void GetValueSkippingTopmostLayer( DmeTime_t time, CDmAttribute *pAttr, uint index = 0 ) const;
	virtual float GetComponent( DmeTime_t time, int componentIndex ) const;
	virtual DmAttributeType_t GetDataType() const;
	virtual bool IsConstantValued() const;
	virtual void RemoveRedundantKeys( bool bKeepEnds );
	virtual void RemoveRedundantKeys( float threshold, bool bKeepEnds );
	virtual void RemoveKey( int nKeyIndex, int nNumKeysToRemove = 1 );
	virtual void Resample( DmeFramerate_t samplerate );
	virtual void Filter( int nSampleRadius );
	virtual void Filter2( DmeTime_t sampleRadius );

	virtual int FindKeyWithinTolerance( DmeTime_t time, DmeTime_t nTolerance );
	virtual DmeTime_t GetKeyTime( int nKeyIndex ) const;
	virtual void	SetKeyTime( int nKeyIndex, DmeTime_t keyTime );
	virtual void ClearAndAddSampleAtTime( DmeTime_t time );
	

	virtual CDmeLogLayer *AddNewLayer();
	virtual void		FlattenLayers( float threshhold, int flags, int baseLayer = 0 );

	// Only used by undo system!!!
	virtual void		AddLayerToTail( CDmeLogLayer *layer );
	virtual CDmeLogLayer *RemoveLayerFromTail();
	virtual CDmeLogLayer *RemoveLayer( int iLayer );

	// curve info helpers
	const CDmeTypedCurveInfo< T > *GetTypedCurveInfo() const;
	      CDmeTypedCurveInfo< T > *GetTypedCurveInfo();
	virtual CDmeCurveInfo *GetOrCreateCurveInfo();
	virtual void SetCurveInfo( CDmeCurveInfo *pCurveInfo );

	// For "faceposer" style left/right edges, this controls whether interpolators try to mimic faceposer left/right edge behavior
	void			SetUseEdgeInfo( bool state );
	bool			IsUsingEdgeInfo() const;

	void			SetEdgeInfo( int edge, bool active, const T& val, int curveType );
	void			GetEdgeInfo( int edge, bool& active, T& val, int& curveType ) const;

	void			SetDefaultEdgeZeroValue( const T& val );
	const T&		GetDefaultEdgeZeroValue() const;

	void			SetRightEdgeTime( DmeTime_t time );
	DmeTime_t		GetRightEdgeTime() const;

	bool			IsEdgeActive( int edge ) const;
	void			GetEdgeValue( int edge, T& value ) const;

	int				GetEdgeCurveType( int edge ) const;
	void			GetZeroValue( int side, T& val ) const;

	T				ClampValue( const T& value );
	T				MaskValue( DmeTime_t time, const T& value, LogComponents_t componentFlags ) const;

	void			SetDefaultValue( const T& value );
	const T&		GetDefaultValue() const;
	bool			HasDefaultValue() const;
	void			ClearDefaultValue();

	static float s_threshold;
	static float GetValueThreshold() { return s_threshold; }
	static void  SetValueThreshold( float s_threshold );


	// Bookmark functions
	virtual void	InitalizeBookmarkArrays();
	virtual int		GetNumBookmarkComponents() const;

	// Removes all keys within the time range, returns true if keys were removed
	virtual bool MaskKeyRange( DmeTime_t tStartTime, DmeTime_t tEndTime, LogComponents_t nComponentFlags, bool bInfiniteLeft = false, bool bInfiniteRight = false );

	void			MaskAgainstLayer( CDmeTypedLogLayer< T > *pFinalLayer, const CDmeTypedLogLayer< T > *pReferenceLayer, LogComponents_t nComponentFlags );

	SegmentInterpolation_t GetSegmentInterpolationSetting( DmeTime_t time ) const;
	SegmentInterpolation_t GetSegmentInterpolationSetting_SkippingTopmostLayer( DmeTime_t time ) const;

protected:
	void RemoveKeys( DmeTime_t starttime );
	
	void		_StampKeyAtHeadResample( DmeTime_t tHeadPosition, const DmeLog_TimeSelection_t & params, const DmeLogTransformParams_t &transformParams, const T& value, bool bSkipToHead, bool bClearPreviousKeys, int layerIndex = -1 );
	void		_StampKeyAtHead( DmeTime_t tHeadPosition, DmeTime_t tPreviousHeadPosition, const DmeLog_TimeSelection_t & params, const T& value, bool bFilteredByTimeSelection, int layerIndex = -1 );
	void		_StampKeyAtTime( CDmeTypedLogLayer< T > *pWriteLayer, DmeTime_t t, const DmeLog_TimeSelection_t &params, const T& value, bool bFilterByTimeSelection, bool bForce = false );

protected:
	CDmaVar< bool >			m_UseDefaultValue;
	CDmaVar< T >			m_DefaultValue;
};


//-----------------------------------------------------------------------------
// Template methods
//-----------------------------------------------------------------------------
template< class T >
DmAttributeType_t CDmeTypedLogLayer<T>::GetDataType() const 
{
	return CDmAttributeInfo< T >::AttributeType();
}

template< class T >
bool CDmeTypedLogLayer<T>::IsConstantValued() const
{
	if ( m_values.Count() < 2 )
		return true;

	if ( m_values.Count() == 2 && !GetTypedOwnerLog()->ValuesDiffer( m_values[ 0 ], m_values[ 1 ] ) )
		return true;

	// we're throwing away duplicate values during recording, so this is generally correct
	// although there are paths to set keys that don't use the duplicate test, so it's not 100%
	return false;
}

//-----------------------------------------------------------------------------
// Template methods
//-----------------------------------------------------------------------------
template< class T >
DmAttributeType_t CDmeTypedLog<T>::GetDataType() const 
{
	return CDmAttributeInfo< T >::AttributeType();
}

template< class T >
void CDmeTypedLog<T>::SetValueThreshold( float thresh ) 
{
	s_threshold = thresh;
}

template< class T >
bool CDmeTypedLog<T>::IsConstantValued() const
{
	int c = m_Layers.Count();
	for ( int i = 0; i < c; ++i )
	{
		if ( !GetLayer( i )->IsConstantValued() )
			return false;
	}

	return true;
}

template< class T >
void CDmeTypedLog<T>::RemoveRedundantKeys( bool bKeepEnds )
{
	int bestLayer = GetTopmostLayer();
	if ( bestLayer < 0 )
		return;

	GetLayer( bestLayer )->RemoveRedundantKeys( bKeepEnds );
}


template< class T >
inline T MaskValue( const T& newValue, const T &currentValue, LogComponents_t componentFlags )
{
	return newValue;
}

template<>
inline Vector MaskValue( const Vector& value, const Vector &curValue, LogComponents_t componentFlags )
{
	Vector writeValue;
	writeValue.x = ( componentFlags & LOG_COMPONENTS_X ) ? value.x : curValue.x;
	writeValue.y = ( componentFlags & LOG_COMPONENTS_Y ) ? value.y : curValue.y;
	writeValue.z = ( componentFlags & LOG_COMPONENTS_Z ) ? value.z : curValue.z;
	return writeValue;
}

template<>
inline Quaternion MaskValue( const Quaternion& value, const Quaternion &curQuat, LogComponents_t componentFlags )
{
	Quaternion writeValue;
	// Convert to euler
	QAngle curQA;
	QuaternionAngles( curQuat, curQA );
	QAngle valueQA;
	QuaternionAngles( value, valueQA );
	// Mask euler
	valueQA.x = ( componentFlags & LOG_COMPONENTS_X ) ? valueQA.x : curQA.x;
	valueQA.y = ( componentFlags & LOG_COMPONENTS_Y ) ? valueQA.y : curQA.y;
	valueQA.z = ( componentFlags & LOG_COMPONENTS_Z ) ? valueQA.z : curQA.z;
	// convert back to Quaternion for output
	AngleQuaternion( valueQA, writeValue );
	return writeValue;
}


template< class T >
inline float Normalize( const T& val )
{
	Assert( 0 );
	return 0.5f;
}

// AT_INT
// AT_FLOAT
// AT_VECTOR*

template<>
inline float Normalize( const bool& val )
{
	return val ? 1.0f : 0.0f;
}

template<>
inline float Normalize( const Color& val )
{
	float sum = 0.0f;
	for ( int i = 0 ; i < 4; ++i )
	{
		sum += val[ i ];
	}
	sum /= 4.0f;
	return clamp( sum / 255.0f, 0.0f, 1.0f );
}

template<>
inline float Normalize( const QAngle& val )
{
	float sum = 0.0f;
	for ( int i = 0 ; i < 3; ++i )
	{
		float ang = val[ i ];
		if ( ang < 0.0f )
		{
			ang += 360.0f;
		}

		sum += ang;
	}
	return clamp( ( sum / 3.0f ) / 360.0f, 0.0f, 1.0f );
}

template<>
inline float Normalize( const Quaternion& val )
{
 	float flAngle = 2.0f * acos( fabs( val.w ) );
 	return flAngle / M_PI;

// 	QAngle angle;
// 	QuaternionAngles( val, angle );
// 	return Normalize( angle );
}

template< class T >
inline void CDmeTypedLog< T >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer )
{
	VPROF_BUDGET( "CDmeTypedLog< T >::BuildNormalizedLayer", "SFM" );

	Assert( pChannels );
	Assert( GetDataType() != AT_FLOAT );

	CDmeTypedLogLayer< T > *pBaseLayer = static_cast< CDmeTypedLogLayer< T > * >( GetLayer( nLayer ) );
	if ( !pBaseLayer )
		return;

	int kc = pBaseLayer->GetKeyCount();
	for ( int i = 0; i < kc; ++i )
	{
		DmeTime_t tKeyTime = pBaseLayer->GetKeyTime( i );
		T keyValue = pBaseLayer->GetKeyValue( i );
		float flNormalized = Normalize( keyValue );

		pChannels[ 0 ]->InsertKey( tKeyTime, flNormalized, pBaseLayer->GetSegmentInterpolationSetting( i ) );
	}

	if ( HasDefaultValue() )
	{
		pChannels[ 0 ]->GetTypedOwnerLog()->SetDefaultValue( Normalize( GetDefaultValue() ) );
	}
}

// Generic implementations all stubbed
// Forward declare specific typed instantiations for float types

template< class T > T CDmeTypedLog< T >::ClampValue( const T& value ) { return value; }
template<> float CDmeTypedLog< float >::ClampValue( const float& value );


template< class T > void CDmeTypedCurveInfo< T >::GetZeroValue( int side, T& val ) const{ Assert( 0 ); }
template< class T > bool CDmeTypedCurveInfo< T >::IsEdgeActive( int edge ) const{ Assert( 0 ); return false; }
template< class T > void CDmeTypedCurveInfo< T >::GetEdgeValue( int edge, T &value ) const{ Assert( 0 ); }

template<> void CDmeTypedCurveInfo< float >::GetZeroValue( int side, float& val ) const;
template<> bool CDmeTypedCurveInfo< float >::IsEdgeActive( int edge ) const;
template<> void CDmeTypedCurveInfo< float >::GetEdgeValue( int edge, float &value ) const;

template<> void CDmeTypedCurveInfo< Vector >::GetZeroValue( int side, Vector& val ) const;
template<> void CDmeTypedCurveInfo< Quaternion >::GetZeroValue( int side, Quaternion& val ) const;

template< class T > void CDmeTypedLogLayer< T >::GetValueUsingCurveInfo( DmeTime_t time, T& out ) const { Assert( 0 ); }
template< class T > void CDmeTypedLogLayer< T >::GetValueUsingCurveInfoSkippingKey( int nKeyToSkip, T& out ) const { Assert( 0 ); }
template<> void CDmeTypedLogLayer< float >::GetValueUsingCurveInfo( DmeTime_t time, float& out ) const;
template<> void CDmeTypedLogLayer< float >::GetValueUsingCurveInfoSkippingKey( int nKeyToSkip, float& out ) const;

template<> void CDmeTypedLogLayer< Vector >::GetValueUsingCurveInfo( DmeTime_t time, Vector& out ) const;
template<> void CDmeTypedLogLayer< Vector >::GetValueUsingCurveInfoSkippingKey( int nKeyToSkip, Vector& out ) const;

template<> void CDmeTypedLogLayer< Quaternion >::GetValueUsingCurveInfo( DmeTime_t time, Quaternion& out ) const;
template<> void CDmeTypedLogLayer< Quaternion >::GetValueUsingCurveInfoSkippingKey( int nKeyToSkip, Quaternion& out ) const;

template<class T> void CDmeTypedLogLayer< T >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< T > *output );
template<> void CDmeTypedLogLayer< bool >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< bool > *output );
template<> void CDmeTypedLogLayer< int >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< int > *output );
template<> void CDmeTypedLogLayer< Color >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< Color > *output );
template<> void CDmeTypedLogLayer< Quaternion >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< Quaternion > *output );
template<> void CDmeTypedLogLayer< VMatrix >::CurveSimplify_R( float thresholdSqr, int startPoint, int endPoint, CDmeTypedLogLayer< VMatrix > *output );

template<> void CDmeTypedLog< Vector >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer );
template<> void CDmeTypedLog< Vector2D >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer );
template<> void CDmeTypedLog< Vector4D >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer );
template<> void CDmeTypedLog< float >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer );
template<> void CDmeTypedLog< int >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer );
template<> void CDmeTypedLog< DmeTime_t >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer );
template<> void CDmeTypedLog< Quaternion >::BuildNormalizedLayer( int nChannels, CDmeTypedLogLayer< float > **pChannels, int nLayer );

template< class T >int CDmeTypedLog< T >::GetNumBookmarkComponents() const;
template<> int CDmeTypedLog< Vector >::GetNumBookmarkComponents() const;
template<> int CDmeTypedLog< Vector2D >::GetNumBookmarkComponents() const;
template<> int CDmeTypedLog< Vector4D >::GetNumBookmarkComponents() const;
template<> int CDmeTypedLog< Quaternion >::GetNumBookmarkComponents() const;

template< class T > void CDmeTypedLog< T >::InitalizeBookmarkArrays();
template<> void CDmeTypedLog< Vector >::InitalizeBookmarkArrays();
template<> void CDmeTypedLog< Vector2D >::InitalizeBookmarkArrays();
template<> void CDmeTypedLog< Vector4D >::InitalizeBookmarkArrays();
template<> void CDmeTypedLog< Quaternion >::InitalizeBookmarkArrays();


//template<> void CDmeTypedLog< float >::FinishTimeSelection( DmeTime_t tHeadPosition, DmeLog_TimeSelection_t& params );
//template<> void CDmeTypedLog< bool >::_StampKeyAtHeadResample( const DmeLog_TimeSelection_t& params, const bool& value ) { Assert( 0 ); }

//-----------------------------------------------------------------------------
// typedefs for convenience (and so the user-supplied names match the programmer names)
//-----------------------------------------------------------------------------
typedef CDmeTypedLog<int>			CDmeIntLog;
typedef CDmeTypedLog<float>			CDmeFloatLog;
typedef CDmeTypedLog<bool>			CDmeBoolLog;
typedef CDmeTypedLog<Color>			CDmeColorLog;
typedef CDmeTypedLog<Vector2D>		CDmeVector2Log;
typedef CDmeTypedLog<Vector>		CDmeVector3Log;
typedef CDmeTypedLog<Vector4D>		CDmeVector4Log;
typedef CDmeTypedLog<QAngle>		CDmeQAngleLog;
typedef CDmeTypedLog<Quaternion>	CDmeQuaternionLog;
typedef CDmeTypedLog<VMatrix>		CDmeVMatrixLog;
typedef CDmeTypedLog<CUtlSymbolLarge>	CDmeStringLog;
typedef CDmeTypedLog<DmeTime_t>		CDmeTimeLog;

//-----------------------------------------------------------------------------
// typedefs for convenience (and so the user-supplied names match the programmer names)
//-----------------------------------------------------------------------------
typedef CDmeTypedLogLayer<int>			CDmeIntLogLayer;
typedef CDmeTypedLogLayer<float>		CDmeFloatLogLayer;
typedef CDmeTypedLogLayer<bool>			CDmeBoolLogLayer;
typedef CDmeTypedLogLayer<Color>		CDmeColorLogLayer;
typedef CDmeTypedLogLayer<Vector2D>		CDmeVector2LogLayer;
typedef CDmeTypedLogLayer<Vector>		CDmeVector3LogLayer;
typedef CDmeTypedLogLayer<Vector4D>		CDmeVector4LogLayer;
typedef CDmeTypedLogLayer<QAngle>		CDmeQAngleLogLayer;
typedef CDmeTypedLogLayer<Quaternion>	CDmeQuaternionLogLayer;
typedef CDmeTypedLogLayer<VMatrix>		CDmeVMatrixLogLayer;
typedef CDmeTypedLogLayer<CUtlSymbolLarge>	CDmeStringLogLayer;
typedef CDmeTypedLogLayer<DmeTime_t>	CDmeTimeLogLayer;

//-----------------------------------------------------------------------------
// typedefs for convenience (and so the user-supplied names match the programmer names)
//-----------------------------------------------------------------------------
typedef CDmeTypedCurveInfo<int>			CDmeIntCurveInfo;
typedef CDmeTypedCurveInfo<float>		CDmeFloatCurveInfo;
typedef CDmeTypedCurveInfo<bool>		CDmeBoolCurveInfo;
typedef CDmeTypedCurveInfo<Color>		CDmeColorCurveInfo;
typedef CDmeTypedCurveInfo<Vector2D>	CDmeVector2CurveInfo;
typedef CDmeTypedCurveInfo<Vector>		CDmeVector3CurveInfo;
typedef CDmeTypedCurveInfo<Vector4D>	CDmeVector4CurveInfo;
typedef CDmeTypedCurveInfo<QAngle>		CDmeQAngleCurveInfo;
typedef CDmeTypedCurveInfo<Quaternion>	CDmeQuaternionCurveInfo;
typedef CDmeTypedCurveInfo<VMatrix>		CDmeVMatrixCurveInfo;
typedef CDmeTypedCurveInfo<CUtlSymbolLarge>	CDmeStringCurveInfo;
typedef CDmeTypedCurveInfo<DmeTime_t>	CDmeTimeCurveInfo;

// the following types are not supported
// AT_ELEMENT,
// AT_VOID,
// <all array types>

//-----------------------------------------------------------------------------
// Helpers for particular types of log layers
//-----------------------------------------------------------------------------
void GenerateRotationLog( CDmeQuaternionLogLayer *pLayer, const Vector &vecAxis, DmeTime_t pTime[4], float pRevolutionsPerSec[4] );

// rotates a position log
void RotatePositionLog( CDmeVector3LogLayer *pPositionLog, const matrix3x4_t& matrix );

// rotates an orientation log
void RotateOrientationLog( CDmeQuaternionLogLayer *pOrientationLog, const matrix3x4_t& matrix, bool bPreMultiply );


float ComputeInterpolationFactor( float flFactor, int nInterpolatorType );
float GetAmountForTime( DmeTime_t dmetime, const TimeSelection_t &times, const int nInterpolationTypes[ 2 ] );

#endif // DMELOG_H
