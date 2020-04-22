//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// Declaration of CLogGraph, a utility for drawing logs in a graph with scale 
// and offset.
//
//=============================================================================

#ifndef LOGGRAPH_H
#define LOGGRAPH_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui/VGUI.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmelog.h"
#include "materialsystem/materialsystemutil.h"
#include "mathlib/beziercurve.h"
#include "Color.h"


class CDmeGraphEditorCurve;
class CDmeCurveKey;


//-----------------------------------------------------------------------------
// The CLogGraph class provides functionality for drawing logs within a graph 
// display. It stores the offset and scale of the graph and provides 
// functionality to draw log data relative to graph.
//-----------------------------------------------------------------------------
class CLogGraph
{

public:

	struct ColorSettings_t
	{
		Color	m_BackgroundColor;		// Color to fill the background area of the graph
		Color	m_TimeBoundsColor;		// Color to draw the time range bounds overlay with
		Color	m_GridColor;			// Color to draw the grid lines with
		Color	m_FontColor;			// Color to draw the graph labels with
		Color	m_CurveColor;			// Color to draw generic log curves with
		Color	m_SegmentColor;			// Color to draw selected curve segments with
		Color	m_SelectionColor;		// Color to draw the selection rectangle with
		Color	m_CrossHairColor;		// Color to draw the cross hair with
		Color	m_KeyColor;				// Color to draw keys with
		Color	m_KeySelectedColor;		// Color to draw selected keys
		Color	m_KeyAddColor;			// Color to draw keys being added to the selection
		Color	m_KeyRemoveColor;		// Color to draw keys being removed from the selection
		Color	m_TangentColor;			// Color to draw tangents with
		Color	m_BrokenTangentColor;	// Color to draw the in tangent of broken tangent keys with
		Color	m_CurveColorX;			// Color in which x components are to be displayed
		Color	m_CurveColorY;			// Color in which y components are to be displayed
		Color	m_CurveColorZ;			// Color in which z components are to be displayed
	};

	// Constructor
	CLogGraph( float flMinScale, float flMaxScale );

	// Set the screen space position of the graph area
	void SetScreenPosition( int x, int y );

	// Set the area in which the graph may be drawn
	void SetGraphDisplayBounds( int x, int y, int width, int height );

	// Set the time range being displayed by the graph
	void SetGraphTimeRange( DmeTime_t minTime, DmeTime_t maxTime );

	// Apply an offset to the graph range specified in pixels
	void ApplyVerticalOffset( int nPixels );

	// Get the current vertical scale ( total vertical value range )
	float GetVerticalScale() const;

	// Set the current vertical scale
	void SetVerticalScale( float scale, float center );

	// Set the vertical range of the graph
	void SetVerticalRange( float minValue, float maxValue );

	// Set the font with which the graph should display its text
	void SetFont( vgui::HFont hFont );

	// Set the colors with which the graph is to be displayed
	void SetColors( const ColorSettings_t &colors );

	// Draw the graph background 
	void DrawBackground( bool bDrawGrid, const DmeFramerate_t &frameRate, bool bDrawFrames, const DmeClipStack_t &shotToRoot ) const;

	// Draw the bounds of the specified time range as rectangles extending to the edge of the graph area
	void DrawTimeRangeBounds( DmeTime_t startTime, DmeTime_t endTime );

	// Draw the specified rectangle to show the selection area
	void DrawSelectionRect( const Rect_t &rect ) const;

	// Draw a cross hair display across the graph at the specified location
	void DrawCrosshair( int x, int y, const DmeFramerate_t &frameRate, bool bDrawFrames, bool bFrameSnap, const DmeClipStack_t &shotToRoot ) const;

	// Draw the specified graph curve
	void DrawGraphCurve( const CDmeGraphEditorCurve *pCurve, const DmeClipStack_t &channelToRoot ) const;

	// Draw the specified set of keys
	void DrawKeysSelectionPreview( const CUtlVector< CDmeCurveKey * > &keyList, const DmeClipStack_t &channelToRoot, int nSelectionMode ) const;

	// Draw the specified log
	void DrawLog( const CDmeLog *pLog, const DmeClipStack_t &channelToRoot, LogComponents_t componentFlags, DmeTime_t minTime = DMETIME_MINTIME, DmeTime_t maxTime = DMETIME_MAXTIME, const Color *pColor = NULL, IMesh *pMesh = NULL ) const;

	// Get the pixel rectangle of a point at the specified location using the current rendering point size
	void GetPointRect( float flValue, DmeTime_t time, Rect_t &rect ) const;

	// Convert a panel coordinate into a graph pixel coordinate
	void PanelPositionToPixel( int &x, int &y );

	// Get the number of pixels per unit of time displayed on the graph
	float GetPixelsPerSecond() const;

	// Get the number of pixels per value unit
	float GetPixelsPerUnit() const;	

	// Get the number of units per pixel
	float GetUnitsPerPixel() const;

	// Get the amount of time for single pixel in tenths of a ms
	float GetTimePerPixel() const;

	// Get the time for the specified pixel within the graph area
	DmeTime_t GetTimeForPixel( int xPos ) const;

	// Get the value for the specified pixel within the graph area
	float GetValueForPixel( int yPos ) const;

	// Get the x coordinate of the pixel representing the specified time within the graph
	int GetPixelForTime( DmeTime_t time, RoundStyle_t roundStyle = ROUND_NEAREST ) const;

	// Get the y coordinate of the pixel representing the specified time within the graph
	int GetPixelForValue( float value, RoundStyle_t roundStyle = ROUND_NEAREST ) const;

	// Compute the screen space position of the specified key
	bool ComputeKeyPositionTangents( const CDmeCurveKey *pKey, const DmeClipStack_t &channelToRoot, Vector &vPos, Vector &vInPos, Vector &vOutPos, bool bScreenSpace ) const;

	// Accessors
	DmeTime_t	GetMinTime() const	{ return m_MinTime;	}
	DmeTime_t	GetMaxTime() const	{ return m_MaxTime;	}

private:

	// Get a temporary render mesh
	IMesh *GetDynamicRenderMesh() const;

	// Draw the keys for the specified curve 
	void DrawCurveKeys( const CDmeGraphEditorCurve *pCurve, const DmeClipStack_t &channelToRoot, IMesh *pMesh ) const;

	// Draw the curve segments associated with the selected keys of the specified curve
	void DrawSelectedCurveSegments( const CDmeGraphEditorCurve *pCurve, const DmeClipStack_t &channelToRoot, IMesh *pMesh ) const;

	// Draw the curve segment between the two specified keys
	void DrawCurveSegment( const CDmeCurveKey *pStartKey, const CDmeCurveKey *pEndKey, DmeTime_t channelMinTime, DmeTime_t channelMaxTime, IMesh *pMesh ) const;

	// Draw the specified Bezier curve segment
	void DrawCurveSegment( const CCubicBezierCurve< Vector > &curveSegment, IMesh *pMesh ) const;

	// Draw a curve segment as a step
	void DrawCurveStep( const Vector &vStart, const Vector &vEnd, IMesh *pMesh ) const;

	// Draw the log of the specified type
	template < typename T >
	void DrawLog( const CDmeTypedLog< T > *pLog, const DmeClipStack_t &channelToRoot, LogComponents_t nComponentFlags, DmeTime_t minTime, DmeTime_t maxTime, const Color *pColor, IMesh *pMesh ) const;

	// Draw the specified log layer
	template < typename T >
	void DrawLogLayer( const CDmeLog *pLog, const CDmeTypedLogLayer< T > *pLogLayer, const DmeClipStack_t &channelToRoot, LogComponents_t componentFlags, bool bSubLayer, DmeTime_t minTime, DmeTime_t maxTime, const Color *pColor, IMesh *pMesh ) const;

	// Draw the specified set of keys from the provided log layer
	template < typename T >
	void DrawLogKeys( CUtlVector< LogKeyValue_t< T > > &displayKeys, LogComponents_t componentFlags, bool bSubLayer, DmeTime_t logMinTime, DmeTime_t logMaxTime, const Color *pColor, IMesh *pMesh ) const;

	// Compute the screen space position of the specified key
	bool ComputeKeyPosition( const CDmeCurveKey *pKey, DmeTime_t timeMin, DmeTime_t timeMax, Vector &vPos ) const;

	// Compute the screen space position of the specified key
	bool ComputeKeyPositionTangents( const CDmeCurveKey *pKey, DmeTime_t timeMin, DmeTime_t timeMax, Vector &vPos, Vector &vInPos, Vector &vOutPos, bool bScreenSpace, bool bNormalizeUnweighted, float flPointOffset = 0 ) const;


	CMaterialReference		m_pMaterial;		// Material to be used to draw the logs
	vgui::HFont				m_hFont;			// Font to be used to draw the value labels on the graph
	int						m_nPointSize;		// Size to be used when rendering a point.
	const float				m_flMinScale;		// Minimum range between the min value and the max value
	const float				m_flMaxScale;		// Maximum range between the min value and the max value
	float					m_flMinValue;		// Minimum value visible on the graph
	float					m_flMaxValue;		// Maximum value visible on the graph
	DmeTime_t				m_MinTime;			// Minimum time visible on the graph
	DmeTime_t				m_MaxTime;			// Maximum time visible on the graph
	
	int						m_nScreenPosX;		// Offset of the graph area in screen space
	int						m_nScreenPosY;		// Offset of the graph area in screen space
	int						m_nAreaX;			// X Coordinate of the upper left corner of the graph display area	
	int						m_nAreaY;			// Y Coordinate of the upper left corner of the graph display area
	int						m_nAreaWidth;		// Width of the graph display area
	int						m_nAreaHeight;		// Height of the graph display area

	ColorSettings_t			m_ColorSettings;	// Structure containing the color settings to be used by the graph

};


DmeTime_t GetTimeForFrame( float frame, const DmeFramerate_t &framerate );


#endif // LOGGRAPH_H
