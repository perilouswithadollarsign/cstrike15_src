//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// Declaration of CDmeGraphEditorState, a data model element which stores 
// the active state data for the graph editor. 
//
//=============================================================================

#ifndef DMEGRAPHEDITORSTATE_H
#define DMEGRAPHEDITORSTATE_H
#ifdef _WIN32
#pragma once
#endif

#include "sfmobjects/dmegrapheditorcurve.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmebookmark.h"
#include "datamodel/dmelement.h"



// Function declaration for bookmark update callbacks.
typedef void (*FnUpdateBookmarks)( const CDmaElementArray< CDmeBookmark > &bookmarkSet, void *pData );
typedef void (*FnOnBookmarkTimeChange )( DmeTime_t oldTime, DmeTime_t newTime, void *pData );


//-----------------------------------------------------------------------------
// The CDmeGraphEditorState class is a data model element which represents the
// active state of the CGraphEditor class. It contains the visible curve set,
// selection, and curve handle data. It provides access to selection and 
// handles, but does not perform any data manipulation.
//-----------------------------------------------------------------------------
class CDmeGraphEditorState : public CDmElement
{
	DEFINE_ELEMENT( CDmeGraphEditorState, CDmElement );

public:

	// Remove all curves from the list of active curves
	void RemoveAllCurves();

	// Add or update the specified channel for editing.
	CDmeGraphEditorCurve *AddCurve( CDmeChannel *pChannel, DmeFramerate_t framerate, bool bFrameSnap, const DmeClipStack_t &clipstack  );

	// Find the graph editor curve for the specified channel
	CDmeGraphEditorCurve *FindCurve( const CDmeChannel *pChannel ) const;

	// Find the graph editor curve for the specified channel within the active set.
	CDmeGraphEditorCurve *FindActiveCurve( const CDmeChannel *pChannel ) const;

	// Find the graph editor curve with in the active set that is associated with a channel of the specified name.
	CDmeGraphEditorCurve *FindActiveCurveByChannelName( const char *name ) const;

	// Remove all curves from the list of active curves
	void DeactiveateAllCurves();

	// Remove and curves which do not remove to a valid channel
	void RemoveInvalidCurves();

	// Add the specified curve to the list of active curves
	void MakeCurveActive( CDmeGraphEditorCurve *pCurve, LogComponents_t nComponents );

	// Clear the current key selection
	void ClearSelection();

	// Select the specified keys according to the specified selection mode
	void SelectKeys( const CUtlVector< CDmeCurveKey * > &keyList, SelectionMode_t selectionMode );

	// Find the index of the the key in the selection set
	int FindSelectedKey( const CDmeCurveKey *pKey ) const;

	// Set the tangent selection for the specified keys
	void SelectKeyTangents( const CUtlVector< CDmeCurveKey * > &keyList, const CUtlVector< bool > &inList, 
						    const CUtlVector< bool > &outList, SelectionMode_t selectionMode );

	// Select the specified component of the specified curve and all the keys on the component
	void SelectCurveComponents( const CUtlVector< CDmeGraphEditorCurve * > &curveList, const CUtlVector < LogComponents_t > &nComponentFlagsList, SelectionMode_t selectionMode );

	// Add a bookmark to the set at the specified time
	CDmeBookmark *AddBookmark( DmeTime_t time, bool bAddToSet = true );

	// Find a bookmark with the specified time
	CDmeBookmark *FindBookmark( DmeTime_t time ) const;

	// Remove all of the current bookmarks from the bookmark set
	void ClearBookmarkSet();

	// Rebuild the bookmark set from the specified list of times
	void SetAllBookmarks( const CUtlVector< DmeTime_t > &times );

	
	// Accessors
	bool ShouldDisplayGrid() const												{ return m_bDisplayGrid;		}
	CDmaElementArray< CDmeBookmark > &GetBookmarkSet()							{ return m_BookmarkSet;			}
	const CDmaElementArray< CDmeBookmark > &GetBookmarkSet() const				{ return m_BookmarkSet;			}
	const CDmaElementArray< CDmeGraphEditorCurve > &GetFullCurveList() const	{ return m_CurveList;			}
	const CDmaElementArray< CDmeGraphEditorCurve > &GetActiveCurveList() const	{ return m_ActiveCurveList;		}
	const CDmaElementArray< CDmeCurveKey > &GetSelectedKeys() const				{ return m_SelectedKeys;		}


private:

	// Select the specified key according to the specified selection mode
	void SelectKey( CDmeCurveKey *pKey, bool bInTangent, bool bOutTangent, SelectionMode_t selectionMode );

	// Add the specified key to the selection
	void AddKeyToSelection( CDmeCurveKey *pKey, bool bInSelection, bool bOutSelection );

	// Remove the specified key from the selection
	void RemoveKeyFromSelection( CDmeCurveKey *pKey, bool bInSelection, bool bOutSelection );		

	// Toggle the selection of the specified key
	void ToggleKeySelection( CDmeCurveKey *pKey, bool bInSelection, bool bOutSelection );	

	// Remove the invalid curves from the specified array
	static void RemoveInvalidCurves( CDmaElementArray< CDmeGraphEditorCurve > &curveList );


	CDmaElementArray< CDmeGraphEditorCurve >	m_CurveList;			// "curveList" : Array of all curves currently available to the graph editor
	CDmaElementArray< CDmeGraphEditorCurve >	m_ActiveCurveList;		// "activeCurveList" : Array of curves visible in the graph editor which will be modified by operations
	CDmaElementArray< CDmeCurveKey >			m_SelectedKeys;			// "selectedKeys" : Array of curve keys that are currently selected
	CDmaElementArray< CDmeBookmark >			m_BookmarkSet;			// "bookmarkSet" : Array of bookmark elements generated from the active log bookmarks
	CDmaVar< bool >								m_bDisplayGrid;			// "displayGrid" : Flag indicating if the graph grid should be displayed		

	friend class CUndoGraphEditorSelectKeys;
};


#endif // DMEGRAPHEDITORSTATE_H
