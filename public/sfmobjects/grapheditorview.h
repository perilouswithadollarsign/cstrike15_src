//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// Declaration of CGraphEditorView class
//
//=============================================================================

#ifndef GRAPHEDITORVIEW_H
#define GRAPHEDITORVIEW_H
#ifdef _WIN32
#pragma once
#endif

#include "sfmobjects/grapheditor.h"
#include "loggraph.h"

// Forward declarations
class CDmeGraphEditorState;

// Function declarations for head time manipulation callbacks.
typedef void (*FnSetHeadTime )( DmeTime_t newTime, void *pData );
typedef void (*FnMoveHead )( int nPixels, void *pData );
typedef void (*FnScaleAboutHead )( float flScaleFactor, void *pData );
typedef void (*FnSetTimeSelection)( DmeTime_t times[ TS_TIME_COUNT ], void *pData );

// Enumeration for the major types of operations
// which can be performed within the graph editor view.
enum GraphOperationType_t
{
	GRAPH_OPERATION_NONE,
	GRAPH_OPERATION_SELECT,
	GRAPH_OPERATION_ZOOM,
	GRAPH_OPERATION_PAN,
	GRAPH_OPERATION_DRAG_KEYS,
	GRAPH_OPERATION_SCALE_KEYS,
};

//-----------------------------------------------------------------------------
// The CGraphEditorView provides a management and manipulation of the the graph
// editor and the graph display of the the curves and logs being edited.
//-----------------------------------------------------------------------------
class CGraphEditorView
{

public:

	enum AxisMode_t
	{
		AXIS_MODE_XY,
		AXIS_MODE_SELECT,
		AXIS_MODE_X,
		AXIS_MODE_Y
	};

	struct Point_t
	{
		Point_t() { x = 0; y = 0; }
		Point_t( int xVal, int yVal ) { x = xVal; y = yVal; }
		int	x;
		int y;
	};

public:

	// Constructor, sets the graph editor which is being viewed
	CGraphEditorView( CGraphEditor &graphEditor );

	// Set the head position and time manipulation callback functions
	void SetCallbacks( FnSetHeadTime pfnSetHeadTime, FnMoveHead pfnSetMoveTime, FnScaleAboutHead pfnScaleAboutHead, FnSetTimeSelection pfnSetTimeSelection, void *pData );

	// Set the area in which the curve graph may be drawn
	void SetGraphDisplayBounds( int x, int y, int width, int height );

	// Set the time range currently being displayed by the graph
	void SetGraphTimeRange( DmeTime_t minTime, DmeTime_t maxTime );

	// Set the active time range, this time range where the graph operations are effective, such as the time range of the current shot
	void SetActiveTimeRange( DmeTime_t startTime, DmeTime_t endTime );

	// Set the screen position 
	void SetScreenPosition( int x, int y );

	// Draw the background of the graph 
	void DrawBackground( DmeFramerate_t frame, bool bDisplayFrames, bool bFrameSnap, const DmeClipStack_t &shotToRoot );

	// Draw the graph elements 
	void Draw();

	// Draw the selection preview of the keys
	void DrawSelection() const;

	// Start an operation of the specified type if an operation is not currently underway
	bool StartOperation( GraphOperationType_t operationType );

	// Complete the current active operation
	void FinishOperation();

	// Update the currently active operation
	bool UpdateOperation( int nCursorPosX, int nCursorPosY, bool bCtrlDown, bool bAltDown, bool bShiftDown, bool bFrameSnap, DmeTime_t currentTime, SelectionMode_t selectionMode, bool bFinal );

	// Apply the specified vertical scale to the the graph using the provided center location
	void ApplyVerticalScale( int delta, int nCursorPosY );

	// Set the vertical range of the graph
	void SetVerticalRange( float minValue, float maxValue );

	// Compute the bounding time and values of the current selection
	bool ComputeSelectionBounds( DmeTime_t &minTime, DmeTime_t &maxTime, float &minValue, float &maxValue ) const;

	// Determine if the cursor is currently within the graph display area
	bool IsPointInGraphArea( int x, int y ) const;

	// Find the curves at the specified location
	void FindCurvesAtPosition( int nPosX, int nPosY, CUtlVector< CDmeGraphEditorCurve * > &curveList, CUtlVector< LogComponents_t > &componentFlags ) const;

	// Accessors
	GraphOperationType_t	GetCurrentOperation() const	{ return m_CurrentOperation;	}
	AxisMode_t				GetAxisMode() const			{ return m_AxisMode;			}
	const Rect_t			&GetArea() const			{ return m_GraphArea;			}
	CLogGraph				&GetLogGraph()				{ return m_LogGraph;			}


private:

	// Update the axis mode based on the mouse movement
	bool UpdateAxisMode( bool bAxisLock );

	// Update the panning operation
	void UpdatePanning( bool bMoveHead, bool bAxisLock );
	
	// Update the zoom operation
	void UpdateZoom( bool bAxisLock );

	// Update the current key manipulation operation
	bool UpdateKeyManipulation( bool bAxisLock, bool bSnapToFrames, bool bUnifiedTangents, GraphOperationType_t operationType, bool bFinal );

	// Update the selection operation
	void UpdateSelection( SelectionMode_t selectionMode );
	
	// Compute the selection rectangle base on the current cursor position and the pressed cursor position
	void ComputeSelectionRectangle( const Point_t &startPoint, const Point_t &endPoint, Rect_t &rect ) const;

	// Select the keys in the specified rectangle
	void SelectKeysInRectangle( const Rect_t &rect, SelectionMode_t selectionMode );

	// Get a list of the keys in the rectangle specified local to the panel
	void FindKeysInRectangle( CDmeGraphEditorCurve *pCurve, const Rect_t &rect, CUtlVector< CDmeCurveKey * > &keyList ) const;

	// Get a list of the tangents belonging to the provided keys that are in the specified rectangle 
	void FindKeyTangentsInRectangle( const Rect_t &rect, CUtlVector< CDmeCurveKey * > &keyList, CUtlVector< bool > &inTangents, CUtlVector< bool > &outTangents ) const;

	// Get a list of the curves in the specified rectangle
	void FindCurvesInRectangle( const Rect_t &rect, CUtlVector< CDmeGraphEditorCurve * > &curveList, CUtlVector< LogComponents_t > &componentFlags ) const;

	// Determine which components of the specified curve intersect the specified rectangle
	void FindCurveComponentsInRectangle( const Rect_t &rect, CDmeGraphEditorCurve *pCurve, LogComponents_t &nComponentFlags ) const;

	// Determine if the specified log intersects the specified rectangle.
	template < typename T >
	void IsLogInRectangle( const Rect_t &rect, const CDmeTypedLog< T > *pLog, const DmeClipStack_t &curveClipStack, LogComponents_t &nComponentFlags ) const;



	CGraphEditor			&m_GraphEditor;				// Graph editor instance used to manipulate the logs using curves
	CLogGraph				m_LogGraph;					// Pointer to the log graph to be used to draw the logs
	GraphOperationType_t	m_CurrentOperation;			// Current state of the panel, specifies what operations are currently happening
	AxisMode_t				m_AxisMode;					// Current axis mode selected for panning
	bool					m_bUnifiedTangents;			// Current unified tangent mode for tangent manipulations
	SelectionMode_t			m_SelectionMode;			// Current selection mode ( add, remove, toggle )
	Rect_t					m_SelectionRect;			// Current area selection rectangle
	Rect_t					m_GraphArea;				// Graph area within the panel
	float					m_flVerticalScale;			// Vertical scale (range) of the graph
	DmeTime_t				m_ActiveStartTime;			// Start time of the active time range of the graph
	DmeTime_t				m_ActiveEndTime;			// End time of the active time range of the graph
	DmeTime_t				m_StartDocTime;				// Document time at the start of the current operation
	DmeTime_t				m_CurrentDocTime;			// Current document time
	Point_t					m_CursorPos;				// Position of the cursor when the last Think() occurred
	Point_t					m_CursorDelta;				// Number of pixels the cursor moved during the last frame
	Point_t					m_StartCursorPos;			// Cursor position at the start of the current operation	

	FnSetHeadTime			m_pfnSetHeadTime;			// Callback function for setting the head time
	FnMoveHead				m_pfnMoveHead;				// Callback function for moving the head
	FnScaleAboutHead		m_pfnScaleAboutHead;		// Callback function for scaling time about the head position
	FnSetTimeSelection		m_pfnSetTimeSelection;		// Callback function for setting the current time selection
	void					*m_pCallbackData;			// Pointer to be provided to the callback time


};


#endif // GRAPHEDITORPANEL_H
