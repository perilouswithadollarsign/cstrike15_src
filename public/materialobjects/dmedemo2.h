//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMEDEMO2_H
#define DMEDEMO2_H
#ifdef _WIN32
#pragma once
#endif
	
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"


//-----------------------------------------------------------------------------
// Demo 2: Defining editable versions of the in-game classes
//
// This is a tricky thing to get right. You want to design for several things:
// 1) Ease of data change
// 2) Separation of editable state from user interface
// 3) Ease of discoverability of data from just looking at the output file
//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------
// Dme version of a quad
// Very straightforward, this is identical to the in-game representation
// with the exception of the 'name' attribute all DmElements have.
//-----------------------------------------------------------------------------
class CDmeQuadV2 : public CDmElement
{
	DEFINE_ELEMENT( CDmeQuadV2, CDmElement );

public:
	CDmaVar< int > m_X0;
	CDmaVar< int > m_Y0;	
	CDmaVar< int > m_X1;
	CDmaVar< int > m_Y1;
	CDmaColor m_Color;
};


//-----------------------------------------------------------------------------
// Dme version of a list of quads
// Note that we hide the list of quads here and instead provide a set of
// service functions limited to the types of editing operations we expect
// to perform on the quad list
//
// Also note when you need to edit an array of struct data, 
// it often results in easier to use code if you create a dme class which 
// represents the array of structs (CDmeQuadListV2 in this case) with utility
// methods as opposed to simply using CDmaElementArray< CDmeQuadV2 > in
// containing classes (CDmeQuadDocV2 in this case)
//
// You also want to avoid using parallel arrays of CDmaIntArrays<> etc
// for each field of the struct. It sucks having to add an element into
// each array every time you add a struct
//-----------------------------------------------------------------------------
class CDmeQuadListV2 : public CDmElement
{
	DEFINE_ELEMENT( CDmeQuadListV2, CDmElement );

public:
	// List management
	void AddQuad( CDmeQuadV2 *pQuad );
	CDmeQuadV2 *FindQuadByName( const char *pName );
	void RemoveQuad( CDmeQuadV2 *pQuad );
	void RemoveAllQuads();

	// Render order management
	void MoveToFront( CDmeQuadV2 *pQuad );
	void MoveToBack( CDmeQuadV2 *pQuad );

private:
	CDmaElementArray< CDmeQuadV2 > m_Quads;
};


//-----------------------------------------------------------------------------
// Dme version of a the editor 'document'
//
// The interface here is designed to be able to be used directly from
// python. I'm currently hiding direct access to CDmeQuadV2 to here to
// make python usage easier, but python can handle it if we pass CDmeQuadV2s
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
class CDmeQuadDocV2 : public CDmElement
{
	DEFINE_ELEMENT( CDmeQuadDocV2, CDmElement );

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
	CDmaElement< CDmeQuadListV2 > m_Quads;
	CDmaElementArray< CDmeQuadV2 > m_SelectedQuads;
};


//-----------------------------------------------------------------------------
// Usage in python (works from the debugger!)
//-----------------------------------------------------------------------------
// 1)	Python at commandline
// 2)	import vs
// 3)	vs.dm.SetUndoEnabled( 0 )
// 4)	doc = vs.CreateElement( ‘DmeQuadDocV2’, ‘root’, -1 )
// 5)	   … doc stuff, e.g. doc.AddQuad( 'quad1', 5, 5, 30, 40 )
// 6)	vs.dm.SaveToFile( ‘file name’, ‘’, ‘keyvalues2’, ‘dmx’, doc )


#endif // DMEDEMO2_H