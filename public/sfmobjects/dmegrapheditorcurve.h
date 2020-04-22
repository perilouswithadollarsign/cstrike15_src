//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// Declaration of CDmeGraphEditorState, a data model element which stores 
// the active state data for the graph editor. 
//
//=============================================================================

#ifndef DMEGRAPHEDITORCURVE_H
#define DMEGRAPHEDITORCURVE_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/dmechannel.h"
#include "datamodel/dmelement.h"
#include "checksum_crc.h"

enum SelectionMode_t
{
	SELECT_SET,
	SELECT_ADD,
	SELECT_REMOVE,
	SELECT_TOGGLE,
};	

enum AddKeyMode_t
{
	ADD_KEYS_AUTO,
	ADD_KEYS_INTERPOLATE,
	ADD_KEYS_STEPPED,
};


enum TangentOperation_t
{
	TANGENTS_FLAT,			// Tangents are modified so the value of the end points are equal to the key value
	TANGENTS_LINEAR,		// Tangents are modified so the endpoints are on the line between the key values
	TANGENTS_SPLINE,		// Tangents are modified so the that they are parallel to the line from the proceeding key to the next key
	TANGENTS_UNIFIED,		// Tangents are modified so both sides of the tangent lie on the same line
	TANGENTS_ISOMETRIC,		// Tangents are modified so both sides of the tangent are of equal length
	TANGENTS_STEP,			// Set the key to step mode, out tangent is ignored and all values between the key and the next key are equal to the key value
	TANGENTS_WEIGHTED,		// Set the key to perform tangent calculations using the time of of the tangents
	TANGENTS_UNWEIGHTED,	// Set the key to perform tangent calculations such that the delta time of the tangents is ignored ( always 1/3 time to next key ).
};

enum KeyTangentMode_t
{
	TANGENT_MODE_DEFAULT,
	TANGENT_MODE_LINEAR,
	TANGENT_MODE_SPLINE,
	TANGENT_MODE_STEPPED
};

class CDmeGraphEditorCurve;


//-----------------------------------------------------------------------------
// The CDmeCurveKey class represents a single key on a curve an hold 
// information about the position of the key and its tangents. The tangent
// values are stored as delta so they do not have to be updated when the value
// of the key changes.
//-----------------------------------------------------------------------------
class CDmeCurveKey : public CDmElement
{
	DEFINE_ELEMENT( CDmeCurveKey, CDmElement );

	enum KeyDirtyFlags_t
	{
		KEY_CLEAN		= 0,
		KEY_IN_DIRTY	= 1,
		KEY_OUT_DIRTY	= 2
	};

	static const int UNWEIGHTED_DISPLAY_LENGTH = 60;

public:

	// Initialize the key with the specified time and value
	void Initialize( DmeTime_t time, float flValue, int nComponent );

	// Get a pointer to the curve to which the key belongs
	CDmeGraphEditorCurve *GetCurve() const;
	
	// Return the sort value for the two keys based on their times
	static int SortFunc( CDmeCurveKey * const *pKeyA, CDmeCurveKey * const *pKeyB );

	// Clear the selection state of the key and its tangents
	void ClearSelection();

	// Set the key tangents to be flat
	void SetTangentsFlat( const CDmeCurveKey *pPrevKey, const CDmeCurveKey *pNextKey, bool bSetIn, bool bSetOut );

	// Set the key tangents to be linear 
	void SetTangentsLinear( const CDmeCurveKey *pPrevKey, const CDmeCurveKey *pNextKey, bool bSetIn, bool bSetOut );

	// Set the key tangents using a spline method
	void SetTangentsSpline( const CDmeCurveKey *pPrevKey, const CDmeCurveKey *pNextKey, bool bSetIn, bool bSetOut );

	// Make both of the tangents of the key lie one the same line
	void SetTangentsUnified( float flUnitsPerSecond, bool bSetIn, bool bSetOut );

	// Make both of the tangents of the key be the same length
	void SetTangentsIsometric( float flUnitsPerSecond, bool bSetIn, bool bSetOut );

	// Set the key tangents as being broken (not unified)
	void SetTangentsBroken();

	// Set the key as being in stepped mode
	void SetStepped( bool bStepped );

	// Enable or disable weighted tangents
	void SetWeighted( bool bWeighted, const CDmeCurveKey *pPrevKey, const CDmeCurveKey *pNextKey );

	// Make the specified key match the unweighted form
	void ConformUnweighted( const CDmeCurveKey *pPrevKey, const CDmeCurveKey *pNextKey );



	void				SetSelected( bool bSelect )		{ m_Selected = bSelect;				}
	void				SetInSelected( bool bSelect )	{ m_InSelected = bSelect;			}
	void				SetOutSelected( bool bSelect )	{ m_OutSelected	= bSelect;			}
	void				SetKeyClean()					{ m_KeyDirtyFlags = KEY_CLEAN; 		}
	void				SetKeyInClean()					{ m_KeyDirtyFlags &= ~KEY_IN_DIRTY; }
	void				SetKeyOutClean()				{ m_KeyDirtyFlags &= ~KEY_OUT_DIRTY;}
	void				SetKeyInDirty()					{ m_KeyDirtyFlags |= KEY_IN_DIRTY;	}
	void				SetKeyOutDirty()				{ m_KeyDirtyFlags |= KEY_OUT_DIRTY;	}

	// Tangent manipulation
	void				SetInTime( DmeTime_t dt )		{ m_InTime = dt;				}
	void				SetInDelta( float flDelta )		{ m_InDelta = flDelta;			}
	void				SetOutTime( DmeTime_t dt )		{ m_OutTime = dt;				}
	void				SetOutDelta( float flDelta )	{ m_OutDelta = flDelta;			}

	// Accessors
	bool				IsSelected() const				{ return m_Selected;			}
	bool				InTangentSelected() const		{ return m_InSelected;			}
	bool				OutTangentSelected() const		{ return m_OutSelected;			}
	bool				IsKeyWeighted() const			{ return m_Weighted;			}
	bool				IsKeyUnified() const			{ return m_Unified;				}
	int					GetComponent() const			{ return m_Component;			}
	DmeTime_t			GetTime() const					{ return m_Time;				}
	float				GetValue() const				{ return m_Value;				}
	DmeTime_t			GetInTime() const				{ return m_InTime;				}
	float				GetInDelta() const				{ return m_InDelta;				}
	DmeTime_t			GetOutTime() const				{ return m_OutTime;				}
	float				GetOutDelta() const				{ return m_OutDelta;			}
	bool				IsKeyStepped() const			{ return m_OutMode == TANGENT_MODE_STEPPED;			}
	KeyTangentMode_t	GetInMode() const				{ return ( KeyTangentMode_t )m_InMode.Get();		}	
	KeyTangentMode_t	GetOutMode() const				{ return ( KeyTangentMode_t )m_OutMode.Get();		}
	
	bool				IsInTangentValid() const		{ return ( m_InTime  != DMETIME_ZERO );				}
	bool				IsOutTangentValid() const		{ return ( m_OutTime != DMETIME_ZERO );				}
	bool				IsKeyDirty() const				{ return ( m_KeyDirtyFlags != KEY_CLEAN );			}
	bool				IsKeyInDirty() const			{ return ( m_KeyDirtyFlags & KEY_IN_DIRTY ) != 0;	}
	bool				IsKeyOutDirty() const			{ return ( m_KeyDirtyFlags & KEY_OUT_DIRTY ) != 0;	}

	static int			GetUnweightedDisplayLength()	{ return UNWEIGHTED_DISPLAY_LENGTH; }

private:

	friend class CDmeGraphEditorCurve;			// GraphEditorCurve is a friend so that it may manipulate the key values
	friend class CUndoGraphEditorModifyKeys;	// The modify keys undo element is also allowed to touch keys directly to restore state

	// Key time and value manipulation, these functions are private as they are intended to be used 
	// only by the CDmeGraphEditorCurve to which the key belongs. This is because the curve needs 
	// to know when these values change, in particular the keys are stored ordered by time in the 
	// curve, so the change of key time may require re-ordering of the key list in the curve.
	void SetTime( DmeTime_t time )						{ m_Time = time;	}
	void SetValue( float value )						{ m_Value = value;	}
	void SetInMode( KeyTangentMode_t mode )				{ m_InMode = mode;	}
	void SetOutMode( KeyTangentMode_t mode )			{ m_OutMode = mode;	}

	// Save all of the current values of the key internally.
	void StoreCurrentValues();

	// Mark the previous values as being invalid
	void ClearPreviousValues()			{ m_bOldValuesValid = false; }

	// The old values are copied from the current values of the key by calling StoreCurrentValues(). These
	// are temporary values that are used to determine the delta values of operations. The following functions 
	// return the old values if they have been stored or the current values if the old values have not been stored.
	bool GetPreviousStepped() const			{ return m_bOldValuesValid ? ( m_OldOutMode == TANGENT_MODE_STEPPED ) : ( m_OutMode == TANGENT_MODE_STEPPED ); }
	DmeTime_t GetPreviousTime()	const		{ return m_bOldValuesValid ? m_OldTime : m_Time; }
	float GetPreviousValue() const			{ return m_bOldValuesValid ? m_OldValue : m_Value; }
	DmeTime_t GetPreviousInTime() const		{ return m_bOldValuesValid ? m_OldInTime : m_InTime; }
	float GetPreviousInDelta() const		{ return m_bOldValuesValid ? m_OldInDelta : m_InDelta; }
	DmeTime_t GetPreviousOutTime() const	{ return m_bOldValuesValid ? m_OldOutTime : m_OutTime; }
	float GetPreviousOutDelta()	const		{ return m_bOldValuesValid ? m_OldOutDelta : m_OutDelta; }

	// Multi-selected tangent state, these will return true if the specified tangent is selected or just the key is selected
	bool InMultiSelected() const			{ return m_InSelected || ( m_Selected && !m_OutSelected ); }
	bool OutMultiSelected() const			{ return m_OutSelected || ( m_Selected && !m_InSelected ); }


	int						m_Component;		// Index of the component of the curve the key is associated with
	int						m_KeyDirtyFlags;	// Flags indicating if the key has been changed and on which side				
	CDmaVar< bool >			m_Selected;			// "selected" : flag indicating if the key is currently selected 
	CDmaVar< bool >			m_InSelected;		// "inSelected" : flag indicating if the in tangent of the key is selected
	CDmaVar< bool >			m_OutSelected;		// "outSelected" : flag indicating if the out tangent of the key is selected
	CDmaVar< bool >			m_Weighted;			// "weighted" : flag indicating if the tangents of the keys are operating in weighted mode
	CDmaVar< bool >			m_Unified;			// "unified" : flag indicating if the tangents of the key are to be manipulated together
	CDmaVar< int >			m_InMode;			// "inMode" : mode in which the in tangent is currently operating
	CDmaVar< int >			m_OutMode;			// "outMode" : mode in which the out tangent is currently operating
	CDmaVar< DmeTime_t >	m_Time;				// "time" : The time the key is located at		
	CDmaVar< float >		m_Value;			// "value" : The value of the curve at the point of the key
	CDmaVar< DmeTime_t >	m_InTime;			// "inTime" : The time length of the left tangent of the key
	CDmaVar< float >		m_InDelta;			// "inDelta" : The delta value of the left tangent of the key
	CDmaVar< DmeTime_t >	m_OutTime;			// "outTime" : The time length of the right tangent of the key
	CDmaVar< float >		m_OutDelta;			// "outDelta" : The delta value of the right tangent of the key

	bool					m_bOldValuesValid;	// Flag indicating if the set of old values have been stored and are valid
	int						m_OldInMode;		// Previous in tangent operation mode
	int						m_OldOutMode;		// Previous out tangent operation mode
	DmeTime_t				m_OldTime;			// Previous time of the key
	float					m_OldValue;			// Previous value of the key 
	DmeTime_t				m_OldInTime;		// Previous time length of the in tangent
	float					m_OldInDelta;		// Previous delta value of the in tangent
	DmeTime_t				m_OldOutTime;		// Previous time length of the out tangent
	float					m_OldOutDelta;		// Previous delta value of the out tangent
};


//-----------------------------------------------------------------------------
// The CDmeEditCurve represents a channel which is being displayed for editing
// int the graph editor.
//-----------------------------------------------------------------------------
class CDmeGraphEditorCurve : public CDmElement
{
	DEFINE_ELEMENT( CDmeGraphEditorCurve, CDmElement );

	static const int MAX_COMPONENTS = LOG_MAX_COMPONENTS;

	struct SamplePoint_t
	{
		DmeTime_t	time;
		float		value;
	};

public:

	// Set the channel and components assigned to the curve and initialize the curve data.
	void Initialize( CDmeChannel *pChannel, DmeFramerate_t framerate, bool bFrameSnap, const DmeClipStack_t &clipstack );
	
	// Initialize the edit log.
	void InitializeEditLog( DmeFramerate_t framerate, const DmeClipStack_t &clipstack );

	// Make sure all results of curve editing are applied to the log
	void Finalize();

	// Determine if the CRC of the log is the same as the last time finalize was called on the curve
	bool VerifyLogCRC() const;

	// Compute the tangents of the specified key
	void ComputeTangentsForKey( CDmeCurveKey *pKey, bool bStepped ) const;

	// Set the tangents for the key to be the specified type
	void SetKeyTangents( CDmeCurveKey *pKey, TangentOperation_t tangentType, bool bRespectSelection, float flUnitsPerSecond ) const;

	// Add keys to the curve at the specified time for the specified components
	void AddKeysAtTime( DmeTime_t time, LogComponents_t nComponentFlags, bool bComputeTangents, AddKeyMode_t addMode, bool bVisibleOnly );

	// Remove all of keys at the specified time 
	bool RemoveKeysAtTime( DmeTime_t time );

	// Add a key to the curve at the specified time on the specified component
	void AddKeyAtTime( DmeTime_t time, int nComponent, bool bRecomputeNeigbors );

	// Set the value of the key at the specified time or create a new key at the time with the specified value
	CDmeCurveKey *SetKeyAtTime( DmeTime_t time, int nComponent, float flValue );

	// Set the position value of the key at the specified time or create a new key at the time with the specified value
	void SetKeyAtTime( DmeTime_t time, LogComponents_t nComponentFlags, const Vector &position );

	// Set the position value of the key at the specified time or create a new key at the time with the specified value
	void SetKeyAtTime( DmeTime_t time, LogComponents_t nComponentFlags, const Quaternion &orientation );

	// Set the position value of the key at the specified time or create a new key at the time with the specified value
	void SetKeyAtTime( DmeTime_t time, LogComponents_t nComponentFlags, const CDmAttribute *pAttr );

	// Remove a key from the specified component of the curve at the specified time
	bool RemoveKeyAtTime( DmeTime_t time, int nComponent );

	// Find the keys within the specified time range
	void FindKeysInTimeRange( CUtlVector< CDmeCurveKey * > &keyList, DmeTime_t startTime, DmeTime_t endTime, int nComponent );

	// Offset the specified keys by the specified about of time
	void OffsetKeyTimes( const CUtlVector< CDmeCurveKey * > &keyList, DmeTime_t timeDelta );
	
	// Move the specified key by the specified amount
	void MoveKeys( const CUtlVector< CDmeCurveKey * > &moveKeyList, DmeTime_t timeDelta, float flValueDelta, 
				   const DmeClipStack_t &localTimeClipStack, DmeFramerate_t framerate, float flValueScale, 
				   float timeScale, DmeTime_t cursorTime, bool bFrameSnap, bool bUnifiedTangents );

	// Scale the specified keys about the specified 
	void ScaleKeys( const CUtlVector< CDmeCurveKey * > &keyList, float flTimeScaleFactor, float flValueScaleFactor,
					DmeTime_t originTime, float flOriginValue, const DmeClipStack_t &localTimeClipStack, DmeFramerate_t framerate );

	// Blend the specified keys toward the provided value using the specified blend factor
	void BlendKeys( const CUtlVector< CDmeCurveKey * > &keyList, Vector4D targetValue, DmAttributeType_t targetValueType, float flBlendFactor );

	// Blend the specified keys toward the value at the specified time using the specified blend factor
	void BlendKeys( const CUtlVector< CDmeCurveKey * > &keyList, DmeTime_t targetValueTime, float flBlendFactor );

	// Delete the specified keys from the curve
	void DeleteKeys( const CUtlVector< CDmeCurveKey * > &keyList );

	// Build the keys from the log bookmarks
	void BuildKeysFromLog( DmeFramerate_t framerate, bool bFrameSnap, const DmeClipStack_t &graphClipStack );

	// Update the edit layer for changes to the specified keys
	void UpdateEditLayer( DmeFramerate_t framerate, const DmeClipStack_t &graphClipStack, bool bEditLayerUndoable, bool bOffsetMode );

	// Flatten the layers of the edit log, overwriting the base log layer with the active edit layer
	void FlattenEditLog();

	// Get the range of values within the specified time range
	bool GetValueRangeForTime( DmeTime_t minTime, DmeTime_t maxTime, float &minValue, float &maxValue ) const;

	// Get the shot relative time of the specified key
	DmeTime_t GetKeyShotTime( int nKeyIndex, int nComponent ) const;

	// Get the shot relative time of the specified key
	DmeTime_t GetKeyShotTime( CDmeCurveKey *pKey ) const;

	// Get the shot relative time of the key and its tangents
	void GetKeyShotTimes( CDmeCurveKey *pKey, DmeTime_t &keyTime, DmeTime_t &inTime, DmeTime_t &outTime ) const;
	
	// Find the neighboring keys of the specified key
	int FindKeyNeighbors( const CDmeCurveKey *pKey, CDmeCurveKey *&pPreviousKey, CDmeCurveKey *&pNextKey ) const;

	// Build the clip stack for channel used by the the curve
	bool BuildClipStack( DmeClipStack_t *pClipStack, CDmeClip *pRoot, CDmeClip *pShot ) const;

	// Get the channels clip to which the channel of the curve belongs
	CDmeChannelsClip *GetChannelsClip() const;

	// Mark all components as not being visible
	void ClearComponents();
	
	// Add the specified components to the current set of components
	void UpdateComponents( LogComponents_t nComponentFlags );

	// Get the flag specifying which components of the log are to be displayed.
	LogComponents_t GetComponentFlags() const;

	// Get the number of components in the curve
	int GetNumComponents() const;

	// Determine if the component specified by index is visible
	bool IsComponentVisible( int nComponentIndex ) const;

	// Determine if the specified component is selected 
	bool IsComponentSelected( int nComponentIndex ) const;

	// Set the selected state of the component
	void SetComponentSelection( LogComponents_t nComponentIndex, SelectionMode_t selectionMode );

	// Clear the component selection of the curve
	void ClearComponentSelection();

	// Get all of the keys for the components specified by the provided flags
	void GetKeysForComponents( CUtlVector< CDmeCurveKey * > &keyList, LogComponents_t nComponentFlags ) const;

	// Get the value of the specified component of the edit log at the specified time
	float GetEditValue( DmeTime_t time, int nComponentIndex ) const;

	// Return true if the specified channel is the one in use by the curve
	bool IsCurveUsingChannel( const CDmeChannel *pChannel ) { return m_Channel == pChannel; }

	// Accessors
	CDmeChannel			*GetChannel() const							{ return m_Channel;								}
	const CDmeLog		*GetEditLog() const							{ return m_EditLog;								}
	const CDmeCurveKey	*GetKey( int nIndex, int nComponent ) const	{ return m_KeyList[ nComponent ][ nIndex ];		}
	CDmeCurveKey		*GetKey( int nIndex, int nComponent )		{ return m_KeyList[ nComponent ][ nIndex ];		}
	int					GetNumKeys( int nComponent ) const			{ return m_KeyList[ nComponent ].Count();		}
	bool				IsVisibleX() const							{ return GetValue< bool >( "xVisible", false );	}
	bool				IsVisibleY() const							{ return GetValue< bool >( "yVisible", false );	}
	bool				IsVisibleZ() const 							{ return GetValue< bool >( "zVisible", false );	}

		
private:


	// Find the key on the specified component at the specified time
	int FindKeyAtLocalTime( DmeTime_t localTime, int nComponent ) const;

	// Find the index of the specified key
	int FindKeyIndex( const CDmeCurveKey *pKey ) const;

	// Find the neighboring keys of the key with the specified index
	void FindKeyNeighbors( int nKeyIndex, int nComponent, CDmeCurveKey *&pPreviousKey, CDmeCurveKey *&pNextKey ) const;

	// Find the previous neighbors of the key using the stored key ordering list
	bool FindOldKeyNeighbors( const CDmeCurveKey *pKey, CDmeCurveKey *&pPreviousKey, CDmeCurveKey *&pNextKey ) const;

	// Find the key which was previously at the first key of the curve segment containing the specified time.
	const CDmeCurveKey *FindOldSegmentForTime( DmeTime_t time, int nComponent ) const;

	// Create a key at the specified time, with the specified value.
	CDmeCurveKey *CreateKey( DmeTime_t time, float flValue, int nComponent, bool bRecomputeNeighbors );

	// Remove the specified key from the curve
	void RemoveKey( int nKeyIndex, int nComponent );

	// Compute the tangents for the curve segment between the two keys
	void ComputeKeyTangentsForSegment( CDmeCurveKey *pStartKey, CDmeCurveKey *pEndKey, bool bAllowStepped ) const;

	// Update the tangent handles of keys based on the current mode
	void UpdateTangentHandles();

	// Update the key list so that all keys are valid and in order
	void FixupKeyList( const CUtlVector< CDmeCurveKey * > &editKeyList );

	// Make sure all of the keys are in order, but make no other changes to the keys
	void SortKeyList();

	// Make a copy of the current key list so that it may be used to compare the key ordering after changes have been made
	void StoreCurrentKeyList();

	// Clear the copy of the stored key list
	void ClearCachedKeyList();

	// Update the base log from the edit log. 
	void UpdateBaseLog( bool bUseEditLayer );

	// Update the edit layer for changes to the specified keys
	template < typename T >
	void UpdateEditLayer( CDmeTypedLog< T > *pEditLog, DmeFramerate_t framerate, const DmeClipStack_t &graphClipstack, bool bEditLayerUndoable, bool bOffsetMode );

	// Generate samples for the curve between two keys and write them into the edit layer
	template < typename T >
	void GenerateCurveSamples( const CDmeCurveKey *pKeyA, const CDmeCurveKey *pKeyB, CUtlVector< DmeTime_t > &sampleTimes, CUtlVector< T > &sampleValues, int nComponent, 
		const CDmeTypedLogLayer< T > *pBaseLayer, int &nLayerKeyIndex, DmeFramerate_t sampleRate, const DmeClipStack_t &clipstack, bool bOffsetMode ) const;

	// Generate sample values for the specified component using the provided sample points
	template < typename T >
	void GenerateSampleValues( const CUtlVector< Vector2D > &points, const CUtlVector< DmeTime_t > &pointTimes, CUtlVector< DmeTime_t > &sampleTimes, CUtlVector< T > &sampleValues, 
		int nComponent, const CDmeTypedLogLayer< T > *pBaseLayer, int &nLayerKeyIndex, DmeFramerate_t sampleRate, const DmeClipStack_t &clipstack ) const;
	
	// Generate sample values for the specified component using the provided previous curve points and the new curve points
	template < typename T >
	void GenerateOffsetSampleValues( const CUtlVector< Vector2D > &points, const CUtlVector< DmeTime_t > &pointTimes, const CUtlVector< Vector2D > &prevPoints, 
		const CUtlVector< DmeTime_t > &prevPointTimes, CUtlVector< DmeTime_t > &sampleTimes, CUtlVector< T > &sampleValues, int nComponent, 
		const CDmeTypedLogLayer< T > *pBaseLayer, int nLayerKeyIndex, DmeFramerate_t sampleRate, const DmeClipStack_t &clipstack, bool bRescaleTime ) const;
	
	// Get the range of values within the specified time range for the specified log 
	template < typename T >
	bool GetValueRangeForTime( CDmeTypedLog< T > *pEditLog, DmeTime_t minTime, DmeTime_t maxTime, float &minValue, float &maxValue ) const;

	// Update the log bookmarks to match the keys 
	void UpdateLogBookmarks() const;

	// Compute the crc of the log associated with channel
	CRC32_t ComputeLogCRC() const;

	// Re-scale the provided list of times from one time frame to another. 
	static void RescaleTimes( DmeTime_t prevStart, DmeTime_t prevEnd, DmeTime_t newStart, DmeTime_t newEnd, const CUtlVector< DmeTime_t > &oldTimes, CUtlVector< DmeTime_t > &newTimes );

	// Interleave a set of times with times occurring at a regular global interval between the provided start and end time.
	static void InterleaveGlobalSampleTimes( DmeTime_t startTime, DmeTime_t endTime, const CUtlVector< DmeTime_t > &existingSampleTimes, CUtlVector< DmeTime_t > &sampleTimes, DmeFramerate_t sampleRate, const DmeClipStack_t &clipstack );

private:

	CDmaElement< CDmeChannel >			m_Channel;	// "channel" : The channel associated with the curve	
	CDmaElement< CDmeLog >				m_EditLog;	// "editLog" : Log that will be used for editing, for quaternions this is the log of Euler angles. 
	CDmaElementArray< CDmeCurveKey >	m_KeyList[ MAX_COMPONENTS ]; // "keyList" : List of keys on the curve
	CDmaVar< int >						m_ComponentSelection; // "componentSelection" : Flags specifying which components are selected

	CRC32_t								m_logCRC;		// CRC value of the log the last time Finalize() was called
	CDmeLogLayer						*m_pEditLayer;	// Pointer to the log layer used to store the results of the curve manipulation 
	CUtlVector< CDmeCurveKey * >		m_OldKeyList[ MAX_COMPONENTS ]; // A list of the ordering of keys before the current modification

	friend class CUndoGraphEditorModifyKeys;
};

// Quaternion <==> Euler conversion utilities.
void QuaternionToEuler( const Quaternion &q, Vector &rotation, Vector &absRotation );
void EulerToQuaternion( const Vector &euler, Quaternion &quat );
bool ConvertLogEulerToQuaternion( const CDmeVector3LogLayer *pEulerLayer, CDmeQuaternionLogLayer *pQuatLayer );
bool ConvertLogQuaterionToEuler( const CDmeQuaternionLogLayer *pQuatLayer, CDmeVector3LogLayer *pEulerLayer, DmeFramerate_t sampleRate, const DmeClipStack_t &clipstack );



#endif // DMEGRAPHEDITORCURVE_H
