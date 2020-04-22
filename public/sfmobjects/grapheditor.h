//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// Declaration of the CGraphEditor class, which is used to perform direct 
// manipulation of log data as if it were a curve.
//
//=============================================================================

#ifndef GRAPHEDITOR_H
#define GRAPHEDITOR_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "sfmobjects/dmegrapheditorstate.h"


class CDmeGraphEditorState;

//-----------------------------------------------------------------------------
// The CGraphEditor class performs direct manipulations of log data by fitting
// a cubic bezier curve to a segment of sample data and then manipulating the 
// controls points of the curve.
//-----------------------------------------------------------------------------
class CGraphEditor
{

public:

	enum PasteMode_t
	{
		PASTE_MERGE,			// Keys are merged with other keys within the paste time range
		PASTE_INSERT,			// Keys after the paste location are offset in time by the paste time range
		PASTE_OVERWRITE			// Keys within the paste time range are removed
	};

	enum Operation_t
	{
		OPERATION_NONE,
		OPERATION_MOVE_KEYS,
		OPERATION_SCALE_KEYS,
		OPERATION_SET_KEYS,
		OPERATION_BLEND_KEYS,
		OPERATION_COUNT,
	};

	// Constructor
	CGraphEditor();

	// Destructor
	~CGraphEditor();
	
	// Set the pointer to the current state element
	void SetStateElement( CDmeGraphEditorState *pStateElement );

	// Set the frame rate to be used by the graph editor when performing sampling operations
	void SetFramerate( DmeFramerate_t framerate );

	// Set the current clip stack, defining the local time frame in which the graph is operating
	void SetClipstack( const DmeClipStack_t &clipStack );

	// Get the pointer to the state element in use by the graph editor
	CDmeGraphEditorState *GetStateElement();

	// Start a new operation
	void StartOperation();
	
	// Finish the current operation and finalize the curve modifications
	void FinishOperation();

	// Cancel the current operation
	void AbortOperation();

	// Move the currently selected keys by the specified amount and time
	void MoveSelectedKeys( DmeTime_t timeDelta, float valueDelta, float flValueScale, float timeScale, DmeTime_t cursorTime, bool bSnapToFrames, bool bUnifiedTangents, bool bEditLayerUndoable );

	// Scale the currently selected keys by the specified amount using the provided origin
	void ScaleSelectedKeys( float flTimeScaleFctor, float flValueScaleFactor, DmeTime_t originTime, float flOriginValue, bool bEditLayerUndoable );

	// Delete the currently selected keys
	bool DeleteSelectedKeys();

	// Modify the tangents of the currently selected keys using the specified operation
	bool ModifyTangents( TangentOperation_t operation, float flUnitsPerSecond, bool bUpdateLog );

	// Remove all of the active curves from the graph editor
	void RemoveAllCurves();

	// Remove all of the curves from the list of active curves ( the ones visible in the graph editor )
	void DeactivateAllCurves();

	// Update the curves associated with the specified channels from their base log data
	void UpdateActiveCurvesFromChannels( const CUtlVector< CDmeChannel * > &channelList );

	// Set the active channels which are to be edited
	void SetActiveChannels( CUtlVector< CDmeChannel * > &channelList, CUtlVector< LogComponents_t > &componentFlags, bool bFrameSnap );

	// Set a key value for all active components of the curves associated with the specified channels
	void SetKeysForChannels( const CUtlVector< CDmeChannel * > &channelList, DmeTime_t globalTime, bool bFinal );

	// Set a key value for the specified components of the curve associated with the provided channel
	void SetKeysForChannels( const CUtlVector< CDmAttribute * > &keyValueAttributes, const CUtlVector< CDmeChannel * > &channelList, const CUtlVector< LogComponents_t > &nComponentFlags, DmeTime_t globalTime, bool bFinal );

	// Get the value of all of the components of the curve associated with the specified channel at the given time
	int GetValuesForChannel( CDmeChannel *pChannel, DmeTime_t time, float *pValues, int nMaxValues ) const;

	// Set the value of all of the components of the curve associated with the specified channel at the given time
	bool SetValuesForChannel( CDmeChannel *pChannel, DmeTime_t time, const float *pValues, int nNumValues ) const;

	// Blend the selected keys on each of the specified channels to the provided value for each channel
	void BlendSelectedKeysForChannels( const CUtlVector< CDmeChannel * > &channelList, const CUtlVector< Vector4D > &valueList, float flBlendFactor, bool bFinal );

	// Blend the selected keys on the curves associated with each of the specified channels to the value on the curve at the specified time.
	void BlendSelectedKeysForChannels( const CUtlVector< CDmeChannel * > &channelList, DmeTime_t globalTargetTime, float flBlendFactor, bool bFinal );

	// Select the specified set of keys according to the provided selection mode
	void SelectKeys( CUtlVector< CDmeCurveKey * > &keyList, SelectionMode_t selectionMode );

	// Select the specified keys and their tangents according to the specified selection mode
	void SelectKeyTangents( CUtlVector< CDmeCurveKey * > &keyList, CUtlVector< bool > &inTangents, CUtlVector< bool > &outTangents, SelectionMode_t selectionMode );

	// Select the specified component of the specified curve and all the keys on the component
	void SelectCurveComponents( const CUtlVector< CDmeGraphEditorCurve * > &curveList, const CUtlVector < LogComponents_t > &nComponentFlagsList, SelectionMode_t selectionMode );

	// Compute the tangent values of any selected keys that currently have invalid tangent values
	void ComputeSelectedKeyTangents();

	// Copy the currently selected keys to the clipboard
	void CopySelectedKeys();

	// Cut the currently selected keys and store them in the clipboard
	void CutSelectedKeys();

	// Paste the keys from the clipboard at the specified time
	void PasteKeysFromClipboard( DmeTime_t pasteTime, PasteMode_t pasteMode, bool bConnect );

	// Add a bookmark to the current curve set
	void AddBookmark( CDmeBookmark *pBookmark );

	// Remove a bookmark from the current curve set
	void RemoveBookmark( CDmeBookmark *pBookmark );

	// Update the time of the associated bookmark proxy if it no longer matches the bookmark.
	void UpdateBookmarkTime( const CDmeBookmark *pBookmark, DmeTime_t oldTime );

	// Get the composite set of bookmarks for all active curves
	CDmaElementArray< CDmeBookmark > *GetBookmarkSet();

	// Perform the required update when the time of a bookmark changes
	void OnBookmarkTimeChange( DmeTime_t oldTime, DmeTime_t newTime );

	// Get the number of selected keys
	int GetNumSelectedKeys() const;

	// Get the selected keys in arrays grouped by the curve to which the key belongs
	int GetSelectedKeysByCurve( CUtlVector< CUtlVector< CDmeCurveKey * > > &curveKeyList, bool bIncludeNeighbors = false ) const;
	
	// Get all of the keys on the active curves
	int GetAllKeysByCurve( CUtlVector< CUtlVector< CDmeCurveKey * > > &curveKeyList ) const;

	// Get all of the active curves
	void GetActiveCurves( CUtlVector< CDmeGraphEditorCurve * > &curveList ) const;

	// Get the current number of active curves
	int GetNumActiveCurves() const;


	
	// Compute the time selection for the currently selected keys
	bool ComputeTimeSelction( DmeTime_t times[ TS_TIME_COUNT ] );

	// Enable or disable offset mode
	void EnableOffsetMode( bool bEnable );

	// Determine if offset mode is enabled
	bool IsOffsetModeEnabled() const;

	// Enable or disable adding keys in stepped mode
	void SetAddKeysStepped( bool bAddKeysStepped );

	// Determine if add keys in stepped mode is enabled
	bool IsAddingKeysStepped() const;

	// Determine if the graph editor is currently performing a drag operation
	bool IsDragging() const;


private:


	struct OperationEntry_t
	{
		int				mode;			// Operation mode identifier
		char			name[ 32 ];		// Name of the operation (used for undo)
	};


	// Set the specified operation mode and perform initialization for that mode
	void SetOperationMode( Operation_t mode );

	// Finalize all of the modifications that have been made to the curves
	void FinalizeChanges() const;

	// Build the bookmark list from the current curve set
	void BuildBookmarksFromCurves();

	// Paste the keys from the provided curve data to the corresponding components of the destination curve
	void PasteKeysToCurve( KeyValues *pCurveData, CDmeGraphEditorCurve *pDstCurve, DmeTime_t timeOffset, DmeTime_t timeSpan, PasteMode_t pasteMode, bool bConnect );

	// Paste the keys from the provided component data to the corresponding destination curve component
	void PasteKeysToComponent( KeyValues *pComponentData, CDmeGraphEditorCurve *pDstCurve, int nComponentIndex, DmeTime_t timeOffset, DmeTime_t timeSpan, PasteMode_t pasteMode, bool bConnect );



	CDmeGraphEditorState			*m_pState;				// Pointer to the state element to be modified by the graph editor
	DmeFramerate_t					m_framerate;			// Frame rate of the current document
	DmeClipStack_t					m_clipstack;			// Clip stack from global time to the local time of the graph
	bool							m_bBuildingBookmarks;	// Flag indicating if the building process is in progress
	bool							m_bStartOperation;		// Flag specifying if the next call to set operation mode will start a new operation
	bool							m_bOffsetMode;			// Flag indicating if the graph editor is operating in offset mode
	bool							m_bAddKeysStepped;		// Flag indicating if new keys added should be created in stepped mode
	Operation_t						m_OperationMode;		// The currently active operation mode

	static const OperationEntry_t	sm_OperationTable[];
	static const OperationEntry_t	sm_TangentOperationTable[];


};



#endif // GRAPHEDITOR_H
