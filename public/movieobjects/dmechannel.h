//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// The channel class - connects elements together, and allows for logging of data
//
//=============================================================================

#ifndef DMECHANNEL_H
#define DMECHANNEL_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/animsetattributevalue.h"
#include "movieobjects/dmeoperator.h"
#include "movieobjects/dmelog.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/proceduralpresets.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmehandle.h"
#include "tier1/utlbuffer.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeClip;
class CDmeChannel;
class CDmeTimeSelection;
class CUndoAddRecordingLayer;
class CUndoSetTimeSelection;

void RemapFloatLogValues( CDmeChannel *pChannel, float flBias, float flScale );
CDmeChannel *FindChannelTargetingAttribute( CDmAttribute *pTargetAttr );

//-----------------------------------------------------------------------------
// different channel modes of operation
//-----------------------------------------------------------------------------
enum ChannelMode_t
{
	CM_OFF,
	CM_PASS,
	CM_RECORD,
	CM_PLAY,
};

enum PlayMode_t
{
	PM_HOLD,
	PM_LOOP,
};

enum RecordOperationFlags_t
{
	RECORD_OPERATION_FLAG_PRESET		= ( 1 << 0 ),
	RECORD_OPERATION_FLAG_REVERSE		= ( 1 << 1 ),
	RECORD_OPERATION_FLAG_REVEAL		= ( 1 << 2 ),
	RECORD_OPERATION_FLAG_REBASE_TIME	= ( 1 << 3 )
};


//-----------------------------------------------------------------------------
// The layer channel info structure contains information about a channel which 
// is currently being modified within a recording layer.
//-----------------------------------------------------------------------------
struct LayerChannelInfo_t
{
	LayerChannelInfo_t() 
		: m_BaseLayer( 0 )
		, m_ModLayerIndex( -1 )
		, m_HeadPosition( 0 )
		, m_TransformWriteMode( TRANSFORM_WRITE_MODE_OVERWRITE )
		, m_bManipulateInFalloff( false )
		, m_ComponentFlags( LOG_COMPONENTS_ALL )
		, m_pPresetValue( 0 )
		, m_pPresetTimes( 0 )
		, m_pRoot( 0 )
		, m_pShot( 0 )
	{
		SetIdentityMatrix( m_Transform );
	}

	CDmeHandle< CDmeChannel > m_Channel;
	DmeClipStack_t			m_ClipStack;
	CUtlBuffer				m_ToAttrData;			// Data of the channel's to attribute before the recording operation
	CUtlBuffer				m_FromAttrData;			// Data of the channel's from attribute after the recording operation
	int						m_BaseLayer;			// Index of the log layer which represents the modification layer 
	int						m_ModLayerIndex;		// Index of the channel within the modification layer
	DmeTime_t				m_HeadPosition;			// Time position of the head when the recording operation took place
	TransformWriteMode_t	m_TransformWriteMode;	// Specification of how existing values in the channel are to be modified
	bool					m_bManipulateInFalloff;	// Should manipulation be applied in falloff (true), or should interpolation be used (false)
	matrix3x4_t				m_Transform;			// Transform matrix which was applied to the log positions of the channel
	Quaternion				m_DeltaRotationLocal;	// Delta rotation value to be used with rotation operations	( local space )
	Quaternion				m_DeltaRotationParent;	// Delta rotation value to be used with rotation operations	( parent space )
	Vector					m_PivotPosition;		// Pivot position value to be used with rotation operations
	LogComponents_t			m_ComponentFlags;		// Flags specifying which individual components of the channel can be modified
	const CDmAttribute		*m_pPresetValue;		// Pointer to the attribute storing the preset value for the operation
	const CDmAttribute		*m_pPresetTimes;		// Pointer to the attribute storing animated preset times for the operation
	CDmeClip				*m_pRoot;				// Pointer to the root clip (sequence) in which the operation occurs
	CDmeClip				*m_pShot;				// Pointer to the shot clip in which the operation occurs
	CDmeHandle< CDmeLogLayer, HT_STRONG > m_hRawDataLayer;	// Handle to the log layer containing the raw recorded data
};


typedef void ( *FnRecordChannelCallback )( CDmeChannel *pChannel );

//-----------------------------------------------------------------------------
// The recording layer structure contains a set of channels and the information
// about a recording operation which was performed on the channels.
//-----------------------------------------------------------------------------
class CRecordingLayer
{
public:

	CRecordingLayer();
	~CRecordingLayer();

	AttributeDict_t						*m_pPresetValuesDict;
	DmeTime_t							m_tHeadShotTime;
	int									m_ProceduralType;
	int									m_OperationFlags;
	int									m_RandomSeed;
	float								m_flThreshold;
	float								m_flIntensity;
	RecordingMode_t						m_RecordingMode;
	TimeSelection_t						m_BaseTimes;
	TimeSelection_t						m_OriginalTimes;
	CUtlVector< LayerChannelInfo_t >	m_LayerChannels;
	CUtlVector< KeyValues * >			m_ClipboardData;
	CUndoAddRecordingLayer				*m_pUndoOperation;
	FnRecordChannelCallback				m_pfnAddChannelCallback;
	FnRecordChannelCallback				m_pfnFinishChannelCallback;
};


//-----------------------------------------------------------------------------
// Simple helper structure used to specify a channel and the components of the
// channel that should be modified. 
//-----------------------------------------------------------------------------
class ModifyChannel
{	
public:

	explicit ModifyChannel( CDmeChannel *pChannel = NULL, LogComponents_t nComponentFlags = LOG_COMPONENTS_ALL ) 
		: m_pChannel( pChannel ), m_nComponentFlags( nComponentFlags ) { }	

	static bool IsChannelInList( CUtlVector< ModifyChannel > const &modifyList, CDmeChannel *pChannel );

	CDmeChannel		*m_pChannel;
	LogComponents_t	m_nComponentFlags;
};


//-----------------------------------------------------------------------------
// The channel modification layer manages a group of log layer modifications
// which can be applied to a varying time range.
//-----------------------------------------------------------------------------
class CDmeChannelModificationLayer
{

public:
	// Default constructor
	CDmeChannelModificationLayer();
	
	// Destructor
	~CDmeChannelModificationLayer();

	// Add a channel to the modification layer if it is not already present,
	// adding the channel to the modification layer causes a new log layer to
	// be added to the channel to which modifications will be made.
	int AddChannel( CDmeChannel *pChannel, bool enableUndo );

	// Add a recording layer to the modification layer.
	CRecordingLayer *AddRecordingLayer();

	// Remove the last recording layer from the modification layer.
	void RemoveLastRecordingLayer();

	// Complete the channel modification layer by flattening all the log layers of 
	// the active channels and clearing out the contents of the modification layer.
	void Finish( bool bFlattenLayers, bool bSaveChanges, bool bRunChannelCallbacks );

	// Return the modification layer of each channel to is base state.
	void WipeChannelModifications();

	// Get the number of recording layers in the modification layer
	int NumRecordingLayers() const;

	// Get a reference to the specified recording layer
	CRecordingLayer &GetRecordingLayer( int index );

	// Is the modification layer visible to the user
	bool IsVisible() const; 

	// Apply the specified transform write mode all of the active channels in each of the recording layers
	void UpdateTransformWriteMode( TransformWriteMode_t mode );


private:

	struct ChannelRef_t
	{
		int								m_RefCount;
		CDmeHandle< CDmeChannel >		m_Channel;
	};

	bool								m_bVisible;
	CUtlVector< CRecordingLayer >		m_RecordingLayerStack;
	CUtlVector< ChannelRef_t >			m_ActiveChannels;
};


//-----------------------------------------------------------------------------
// A class managing channel recording
//-----------------------------------------------------------------------------
class CDmeChannelRecordingMgr
{
public:
	// constructor
	CDmeChannelRecordingMgr();


	// Start and complete a modification layer. All recording layers must be placed within a modification
	// layer, so a modification layer must be active before starting a recording layer.
	void StartModificationLayer( const DmeLog_TimeSelection_t *pTimeSelection = NULL, bool createLayer = true );
	void FinishModificationLayer( bool bSaveChanges = true, bool bFlattenLayers = true );

	// Enable or disable use of the modification layer.
	void EnableModificationLayer( bool enable );

	// Activates, deactivates layer recording.
	void StartLayerRecording( const char * const pUndoRedoDesc, AttributeDict_t *pPresetValuesDict = NULL, DmeTime_t tHeadShotTime = DMETIME_INVALID, int proceduralType = PROCEDURAL_PRESET_NOT, int recordFlags = 0, FnRecordChannelCallback pfnAddChannel = NULL, FnRecordChannelCallback pfnFinishChannel = NULL );
	void FinishLayerRecording( float flThreshold, bool bFlattenLayers = true, bool bAllowFinishModification = true );
	void CancelLayerRecording();

	// Adds a channel to the recording layer
	int AddChannelToRecordingLayer( CDmeChannel *pChannel, LogComponents_t componentWriteFlags = LOG_COMPONENTS_ALL, CDmeClip *pRoot = NULL, CDmeClip *pShot = NULL );

	// Explicitly set the clipboard data for the active recording layer
	void CopyClipboardDataForRecordingLayer( const CUtlVector< KeyValues * > &keyValuesList );

	// Used to iterate over all channels currently being recorded
	// NOTE: Use CDmeChannel::AddToRecordingLayer to add a channel to the recording layer
	int GetLayerRecordingChannelCount();
	CDmeChannel* GetLayerRecordingChannel( int nIndex );

	// Computes time selection info in log time for a particular recorded channel
	// NOTE: Only valid if IsUsingTimeSelection() returns true
	void GetLocalTimeSelection( DmeLog_TimeSelection_t& selection, int nIndex );

	// Get the index of the modification base layer for the specified channel within the current recording layer.
	int GetModificationBaseLayer( int nIndex );

	// Set the time selection for the modification layer and re-apply all recording layers that are on the current modification
	// layer with the new time selection. Must be done with active modification layer, but without an active recording layer.
	void SetTimeSelection( const DmeLog_TimeSelection_t &timeSelection, bool bUpdateBaseTime );
	
	// Start or continue processing of the recording layers within the modification layer.
	CRecordingLayer *ProcessModificationLayer( int &recordingLayer );

	// Complete the modification layer processing for the specified recording layer.
	void CompleteModificationProcessing( );

	// Save the to or from attribute data of the specified channel within the current recording layer.
	void StoreChannelAttributeData( int nChannelIndex, bool fromAttr );

	// Get the current time selection
	void GetTimeSelection( CDmeTimeSelection &timeSelection ) const;
	const DmeLog_TimeSelection_t &GetTimeSelection() const;
	
	// Methods which control various aspects of recording
	void UpdateTimeAdvancing( bool bPaused, DmeTime_t tCurTime );
	void UpdateRecordingChannelHeadPositions( DmeTime_t tCurTime );
	void UpdateRecordingTimeSelectionTimes( const DmeLog_TimeSelection_t& timeSelection );
	void SetIntensityOnAllLayers( float flIntensity );
	void SetRecordingMode( RecordingMode_t mode );
	void SetPresetValue( CDmeChannel* pChannel, const CDmAttribute *pPresetValue, const CDmAttribute *pPresetTimes );
	void SetInRedo( bool bInRedo );


	void SetProceduralTarget( int nProceduralMode, const CDmAttribute *pTarget );
	void SetProceduralTarget( int nProceduralMode, const CUtlVector< KeyValues * >& list, int randomSeed );
	int GetProceduralType() const;
	const CDmAttribute *GetProceduralTarget() const;
	const CUtlVector< KeyValues * > &GetPasteTarget() const;
	void GetPasteClipboardData( CUtlVector< KeyValues * > &list ) const;

	void SetModificationLayerDirty();
	CDmeChannelModificationLayer *GetModificationLayer();

	void				 SetTransformWriteMode( TransformWriteMode_t mode );
	TransformWriteMode_t GetTransformWriteMode() const;	

	void				UpdateActiveLayerManipulateInFalloff( bool bManipulateInFalloff );
	
	static void RunFinishCallbacksOnRecordingLayer( CRecordingLayer *pRecordingLayer );
	
	// Methods to query aspects of recording
	bool IsTimeAdvancing() const;
	bool IsUsingDetachedTimeSelection() const;
	bool IsUsingTimeSelection() const;
	bool IsRecordingLayerActive() const;
	bool IsModificationLayerActive() const;
	bool IsModificationLayerVisible() const;
	bool IsProcessingModifications() const;
	bool IsModificationLayerEnabled() const;
	bool IsInRedo() const;

private:
	
	// Methods available for CDmeChannel
	bool ShouldRecordUsingTimeSelection() const;

	// Internal methods
	void FlattenLayers( CRecordingLayer *pRecordingLayer );
	void RemoveAllChannelsFromRecordingLayer( CRecordingLayer *pRecordingLayer, bool destroyLogLayers );

	// Apply the effects of the recording layer to its channels with the active time selection.
	bool ApplyRecordingLayer( CRecordingLayer &recordingLayer );

	// Update the time selection state, this is used by undo
	void UpdateTimeSelection( const TimeSelection_t &timeSelection, const CUtlVector< TimeSelection_t > &baseTimeSelection, int leftFalloff, int rightFalloff, bool bLeftInfinite, bool bRightInfinite );

	bool									m_bSavedUndoState : 1;
	bool									m_bUseTimeSelection : 1;				
	bool									m_bModificationLayerDirty : 1;
	bool									m_bModificationProcessing : 1;
	bool									m_bWantsToFinish : 1;
	bool									m_bFinishFlattenLayers : 1;
	bool									m_bModificationLayerEnabled : 1;
	bool									m_bInRedo : 1;
	CRecordingLayer							*m_pActiveRecordingLayer;
	CDmeChannelModificationLayer			*m_pModificationLayer;

	DmeLog_TimeSelection_t					m_TimeSelection;
	int										m_nProceduralType;
	const CDmAttribute						*m_pRevealTarget;
	CUtlVector< KeyValues * >				m_PasteTarget;
	int										m_RandomSeed;
	TransformWriteMode_t					m_TransformWriteMode;


	friend CDmeChannel;
	friend CUndoAddRecordingLayer;
	friend CUndoSetTimeSelection;
};

// Singleton
extern CDmeChannelRecordingMgr *g_pChannelRecordingMgr;

struct TimeState_t
{
	TimeState_t() : m_timeOutsideTimeframe( DMETIME_INVALID ), m_tCurrentTime( DMETIME_INVALID ), m_tPreviousTime( DMETIME_INVALID )
	{
	}
	DmeTime_t m_timeOutsideTimeframe;
	DmeTime_t m_tCurrentTime;
	DmeTime_t m_tPreviousTime;
};

//-----------------------------------------------------------------------------
// A class representing a channel
//-----------------------------------------------------------------------------
class CDmeChannel : public CDmeOperator
{
	DEFINE_ELEMENT( CDmeChannel, CDmeOperator );

public:
	virtual bool IsDirty(); // ie needs to operate
	virtual void Operate();

	virtual void GetInputAttributes ( CUtlVector< CDmAttribute * > &attrs );
	virtual void GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs );

	void SetInput ( CDmElement* pElement, const char* pAttribute, int index = 0 );
	void SetOutput( CDmElement* pElement, const char* pAttribute, int index = 0 );

	void SetInput( CDmAttribute *pAttribute, int index = 0 );
	void SetOutput( CDmAttribute *pAttribute, int index = 0 );

	CDmElement *GetFromElement() const;
	CDmElement *GetToElement() const;

	CDmAttribute *GetFromAttribute();
	CDmAttribute *GetToAttribute();

	int GetFromArrayIndex() const;
	int GetToArrayIndex() const;

	int GetRecordLayerIndex() const;
	void SetRecordLayerIndex( int layerIndex );

	ChannelMode_t GetMode();
	void SetMode( ChannelMode_t mode );

	void ClearLog();
	CDmeLog *GetLog();
	void SetLog( CDmeLog *pLog );
	CDmeLog *CreateLog( DmAttributeType_t type );
	template < class T > CDmeTypedLog<T> *CreateLog();

	void ClearTimeMetric();
	void SetCurrentTime( DmeTime_t time, DmeTime_t start, DmeTime_t end );
	void SetCurrentTime( DmeTime_t time ); // Simple version. Only works if multiple active channels clips do not reference the same channels
	DmeTime_t GetCurrentTime() const;
	
	const TimeState_t &GetTimeState() const;
	void SetTimeState( TimeState_t &TimeState );
		
	void SetChannelToPlayToSelf( const char *outputAttributeName, float defaultValue, bool force = false );

	template< class T >
	void SetValueOnInput( const T &value );

	// need this until we have the EditApply message queue
	void OnAttributeChanged( CDmAttribute *pAttribute );

	template< class T >
	bool GetInputValue( T &value );
	template< class T >
	bool GetOutputValue( T &value );
	template< class T >
	bool	GetCurrentPlaybackValue( T& value );
	template< class T >
	bool	GetPlaybackValueAtTime( DmeTime_t time, T& value );

	void Play( bool useEmptyLog = false );
	void Record();

	void SetNextKeyCurveType( int nCurveType );

	// Builds a clip stack for the channel
	CDmeClip* FindOwnerClipForChannel( CDmeClip *pRoot );
	bool BuildClipStack( DmeClipStack_t *pClipStack, CDmeClip *pRoot, CDmeClip *pShot ) const;

	void ScaleSampleTimes( float scale );

	void ClearAndAddSampleAtTime( DmeTime_t time );

protected:
	// Used to cache off handles to attributes
	CDmAttribute* SetupFromAttribute();
	CDmAttribute* SetupToAttribute();

	template< class T >
	bool GetValue( T &value, const CDmAttribute *pAttr, int nIndex );

	void Pass();

	CDmaElement< CDmElement > m_fromElement;
	CDmaString m_fromAttribute;
	CDmaVar< int > m_fromIndex;
	CDmaElement< CDmElement > m_toElement;
	CDmaString m_toAttribute;
	CDmaVar< int > m_toIndex;
	CDmaVar< int > m_mode;
	CDmaElement< CDmeLog > m_log;

	DmAttributeHandle_t m_FromAttributeHandle;
	DmAttributeHandle_t m_ToAttributeHandle;

	TimeState_t m_TimeState;

	int m_nRecordLayerIndex;
	int m_nNextCurveType;

};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
template < class T > 
inline CDmeTypedLog<T> *CDmeChannel::CreateLog()
{
	return CastElement< CDmeTypedLog<T> >( CreateLog( CDmAttributeInfo<T>::AttributeType() ) );
}

inline CDmAttribute *CDmeChannel::GetFromAttribute()
{
	CDmAttribute *pAttribute = g_pDataModel->GetAttribute( m_FromAttributeHandle );
	if ( !pAttribute )
	{
		pAttribute = SetupFromAttribute();
	}
	return pAttribute;
}

inline CDmAttribute *CDmeChannel::GetToAttribute()
{
	CDmAttribute *pAttribute = g_pDataModel->GetAttribute( m_ToAttributeHandle );
	if ( !pAttribute )
	{
		pAttribute = SetupToAttribute();
	}
	return pAttribute;
}

template< class T >
inline bool CDmeChannel::GetPlaybackValueAtTime( DmeTime_t time, T& value )
{
	CDmeTypedLog< T > *pLog = CastElement< CDmeTypedLog< T > >( GetLog() );
	if ( !pLog || pLog->IsEmpty() )
		return false;

	DmeTime_t t0 = pLog->GetBeginTime();
	DmeTime_t tn = pLog->GetEndTime();

	PlayMode_t pmode = PM_HOLD;
	switch ( pmode )
	{
	case PM_HOLD:
		time = clamp( time, t0, tn );
		break;

	case PM_LOOP:
		if ( tn == t0 )
		{
			time = t0;
		}
		else
		{
			time -= t0;
			time = time % ( tn - t0 );
			time += t0;
		}
		break;
	}

	value = pLog->GetValue( time );
	return true;
}

template< class T >
inline bool CDmeChannel::GetValue( T &value, const CDmAttribute *pAttr, int nIndex )
{
	if ( pAttr )
	{
		if ( IsArrayType( pAttr->GetType() ) )
		{
			CDmrArrayConst< T > array( pAttr );
			if ( nIndex >= 0 && nIndex < array.Count() )
			{
				value = array[ nIndex ];
				return true;
			}
		}
		else
		{
			value = pAttr->GetValue< T >();
			return true;
		}
	}

	CDmAttributeInfo< T >::SetDefaultValue( value );
	return false;
}


template< class T >
inline bool CDmeChannel::GetInputValue( T &value )
{
	return GetValue< T >( value, GetFromAttribute(), m_fromIndex );
}

template< class T >
inline bool CDmeChannel::GetOutputValue( T &value )
{
	return GetValue< T >( value, GetToAttribute(), m_toIndex );
}

template< class T >
inline bool CDmeChannel::GetCurrentPlaybackValue( T& value )
{
	return GetPlaybackValueAtTime( GetCurrentTime(), value );
}

template< class T >
void CDmeChannel::SetValueOnInput( const T &value )
{
	CDmAttribute *pFromAttr = GetFromAttribute();
	if ( !pFromAttr )
		return;

	if ( IsArrayType( pFromAttr->GetType() ) )
	{
		if ( ArrayTypeToValueType( pFromAttr->GetType() ) != CDmAttributeInfo< T >::AttributeType() )
			return;

		CDmrArray< T > array( pFromAttr );
		array.Set( m_fromIndex.Get(), value );
	}
	else
	{
		if ( pFromAttr->GetType() != CDmAttributeInfo< T >::AttributeType() )
			return;

		pFromAttr->SetValue( value );
	}
}

#endif // DMECHANNEL_H
