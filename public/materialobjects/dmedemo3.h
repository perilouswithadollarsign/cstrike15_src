//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMEDEMO3_H
#define DMEDEMO3_H
#ifdef _WIN32
#pragma once
#endif
	
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"


//-----------------------------------------------------------------------------
// Demo 3: Creating an in-game editor for the editable versions
//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------
// No changes here from demo 2
//-----------------------------------------------------------------------------
class CDmeQuadV3 : public CDmElement
{
	DEFINE_ELEMENT( CDmeQuadV3, CDmElement );

	// NEW METHODS IN VERSION 3, needed by renderer
public:
	int MinX() const { return MIN( m_X0, m_X1 ); }
	int MinY() const { return MIN( m_Y0, m_Y1 ); }
	int MaxX() const { return MAX( m_X0, m_X1 ); }
	int MaxY() const { return MAX( m_Y0, m_Y1 ); }

	// OLD METHODS FROM VERSION 3
public:
	CDmaVar< int > m_X0;
	CDmaVar< int > m_Y0;	
	CDmaVar< int > m_X1;
	CDmaVar< int > m_Y1;
	CDmaColor m_Color;
};


//-----------------------------------------------------------------------------
// No changes here from demo 2
//-----------------------------------------------------------------------------
class CDmeQuadListV3 : public CDmElement
{
	DEFINE_ELEMENT( CDmeQuadListV3, CDmElement );

	// NEW STUFF FOR DEMO 3
public:
	// Iteration necessary to render
	int GetQuadCount() const;
	CDmeQuadV3 *GetQuad( int i );

	// OLD STUFF FROM DEMO 2
public:
	// List management
	void AddQuad( CDmeQuadV3 *pQuad );
	CDmeQuadV3 *FindQuadByName( const char *pName );
	void RemoveQuad( CDmeQuadV3 *pQuad );
	void RemoveAllQuads();

	// Render order management
	void MoveToFront( CDmeQuadV3 *pQuad );
	void MoveToBack( CDmeQuadV3 *pQuad );

private:
	CDmaElementArray< CDmeQuadV3 > m_Quads;
};


//-----------------------------------------------------------------------------
// Dme version of a the editor 'document'
//
// The interface here is designed to be able to be used directly from
// python. I'm currently hiding direct access to CDmeQuadV3 to here to
// make python usage easier, but python can handle it if we pass CDmeQuadV3s
// in the interface. We may well want to start passing them around once
// we get to the VGUI-based editor.
//
// Early editors we wrote didn't clearly separate data from UI at the doc 
// level which resulted in a bunch of complexity as our tools got bigger.
// Actually making a Dm element which contains a notion of selection in it
// I believe will reduce this problem in the future (this is still an untested
// theory in-house, although other 3rd party editors use this technique also).
//
// Remember that only attributes can be saved and have undo support.
// If you want to add members to a Dme element which are not saved and
// never need undo, you can	either use normal non-CDma members, 
// or mark attributes to not be saved. In this case, I make the 
// selection state be an attribute to get undo  but mark the selection 
// attribute to not save it to the file.
//-----------------------------------------------------------------------------
class CDmeQuadDocV3 : public CDmElement
{
	DEFINE_ELEMENT( CDmeQuadDocV3, CDmElement );

	// NEW STUFF FOR DEMO 3
public:
	// Iteration necessary to render
	int GetQuadCount() const;
	CDmeQuadV3 *GetQuad( int i );

	// Iteration necessary to render
	int GetSelectedQuadCount() const;
	CDmeQuadV3 *GetSelectedQuad( int i );

	// Add quads in rect to selection
	void AddQuadsInRectToSelection( int x0, int y0, int x1, int y1 );

	// Is point in selected quad?
	bool IsPointInSelectedQuad( int x, int y ) const;

	// OLD STUFF FROM DEMO 2
public:
	// Adds quad, resets selection to new quad
	void AddQuad( const char *pName, int x0, int y0, int x1, int y1 );

	// Clears selection
	void ClearSelection();

	// Adds quad to selection
	void AddQuadToSelection( const char *pName );

	// Deletes selected quads
	void DeleteSelectedQuads();

	// Changes quad color
	void SetSelectedQuadColor( int r, int g, int b, int a );

	// Moves quads
	void MoveSelectedQuads( int dx, int dy );

	// Resizes selected quad (works only when 1 quad is selected)
	void ResizeSelectedQuad( int nWidth, int nHeight );

	// Moves selected quad to front/back (works only when 1 quad is selected)
	void MoveSelectedToFront();
	void MoveSelectedToBack();

private:
	CDmaElement< CDmeQuadListV3 > m_QuadList;
	CDmaElementArray< CDmeQuadV3 > m_SelectedQuads;
};


//-----------------------------------------------------------------------------
// Usage in python (works from the debugger!)
//-----------------------------------------------------------------------------
// 1)	Python at commandline
// 2)	import vs
// 3)	vs.dm.SetUndoEnabled( 0 )
// 4)	doc = vs.CreateElement( ‘DmeQuadDocV3’, ‘root’, -1 )
// 5)	   … doc stuff, e.g. doc.AddQuad( 'quad1', 5, 5, 30, 40 )
// 6)	vs.dm.SaveToFile( ‘file name’, ‘’, ‘keyvalues2’, ‘dmx’, doc )


#endif // DMEDEMO3_H