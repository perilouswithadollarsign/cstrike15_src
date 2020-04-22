//===== Copyright © 2005-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: build a sheet data file and a large image out of multiple images
//
//===========================================================================//

#include "materialobjects/dmedemo2.h"
#include "tier1/strtools.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier0/dbg.h"


//-----------------------------------------------------------------------------
// Dme version of a quad
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeQuadV2, CDmeQuadV2 );

void CDmeQuadV2::OnConstruction()
{
	m_X0.Init( this, "x0" );
	m_Y0.Init( this, "y0" );	
	m_X1.Init( this, "x1" );
	m_Y1.Init( this, "y1" );	
	m_Color.InitAndSet( this, "color", Color( 255, 255, 255, 255 ) );	
}

void CDmeQuadV2::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Dme version of a list of quads
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeQuadListV2, CDmeQuadListV2 );
void CDmeQuadListV2::OnConstruction()
{
	m_Quads.Init( this, "quads" );
}

void CDmeQuadListV2::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// List management
//-----------------------------------------------------------------------------
void CDmeQuadListV2::AddQuad( CDmeQuadV2 *pQuad )
{
	m_Quads.InsertBefore( 0, pQuad );
}

CDmeQuadV2 *CDmeQuadListV2::FindQuadByName( const char *pName )
{
	for ( int i = 0; i < m_Quads.Count(); ++i )
	{
		if ( !Q_stricmp( pName, m_Quads[i]->GetName() ) )
			return m_Quads[i];
	}
	return NULL;
}

void CDmeQuadListV2::RemoveQuad( CDmeQuadV2 *pQuad )
{
	int nIndex = m_Quads.Find( pQuad );
	if ( nIndex != m_Quads.InvalidIndex() )
	{
		m_Quads.Remove( nIndex );
	}
}

void CDmeQuadListV2::RemoveAllQuads()
{
	m_Quads.RemoveAll();
}


//-----------------------------------------------------------------------------
// Render order management
//-----------------------------------------------------------------------------
void CDmeQuadListV2::MoveToFront( CDmeQuadV2 *pQuad )
{
	int nIndex = m_Quads.Find( pQuad );
	if ( nIndex != m_Quads.InvalidIndex() )
	{
		m_Quads.Remove( nIndex );
		m_Quads.AddToTail( pQuad );
	}
}

void CDmeQuadListV2::MoveToBack( CDmeQuadV2 *pQuad )
{
	int nIndex = m_Quads.Find( pQuad );
	if ( nIndex != m_Quads.InvalidIndex() )
	{
		m_Quads.Remove( nIndex );
		m_Quads.InsertBefore( 0, pQuad );
	}
}


//-----------------------------------------------------------------------------
// Dme version of a the editor 'document'
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeQuadDocV2, CDmeQuadDocV2 );
void CDmeQuadDocV2::OnConstruction()
{
	m_Quads.InitAndCreate( this, "quadList" );	 
	m_SelectedQuads.Init( this, "selectedQuads", FATTRIB_DONTSAVE );
}

void CDmeQuadDocV2::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Adds quad, resets selection to new quad
//-----------------------------------------------------------------------------
void CDmeQuadDocV2::AddQuad( const char *pName, int x0, int y0, int x1, int y1 )
{
	CDmeQuadV2 *pQuadV2 = CreateElement< CDmeQuadV2 >( pName, GetFileId() );
	pQuadV2->m_X0 = x0;
	pQuadV2->m_X1 = x1;
	pQuadV2->m_Y0 = y0;
	pQuadV2->m_Y1 = y1;
	m_Quads->AddQuad( pQuadV2 );

	ClearSelection();
	AddQuadToSelection( pName );
}


//-----------------------------------------------------------------------------
// Clears selection
//-----------------------------------------------------------------------------
void CDmeQuadDocV2::ClearSelection()
{
	m_SelectedQuads.RemoveAll();
}


//-----------------------------------------------------------------------------
// Adds quad to selection
//-----------------------------------------------------------------------------
void CDmeQuadDocV2::AddQuadToSelection( const char *pName )
{
	CDmeQuadV2 *pQuad = m_Quads->FindQuadByName( pName );
	if ( pQuad )
	{
		m_SelectedQuads.AddToTail( pQuad );
	}
}


//-----------------------------------------------------------------------------
// Deletes selected quads
//-----------------------------------------------------------------------------
void CDmeQuadDocV2::DeleteSelectedQuads()
{
	int nCount = m_SelectedQuads.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		DestroyElement( m_SelectedQuads[i] );
	}
	ClearSelection();
}


//-----------------------------------------------------------------------------
// Changes quad color
//-----------------------------------------------------------------------------
void CDmeQuadDocV2::SetSelectedQuadColor( int r, int g, int b, int a )
{
	Color c( r, g, b, a );
	int nCount = m_SelectedQuads.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeQuadV2 *pQuad = m_SelectedQuads[i];
		pQuad->m_Color.Set( c );
	}
}


//-----------------------------------------------------------------------------
// Moves quads
//-----------------------------------------------------------------------------
void CDmeQuadDocV2::MoveSelectedQuads( int dx, int dy )
{
	int nCount = m_SelectedQuads.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeQuadV2 *pQuad = m_SelectedQuads[i];
		pQuad->m_X0 += dx;
		pQuad->m_X1 += dx;
		pQuad->m_Y0 += dy;
		pQuad->m_Y1 += dy;
	}
}


//-----------------------------------------------------------------------------
// Resizes selected quad (works only when 1 quad is selected)
//-----------------------------------------------------------------------------
void CDmeQuadDocV2::ResizeSelectedQuad( int nWidth, int nHeight )
{
	if ( m_SelectedQuads.Count() != 1 )
		return;

	CDmeQuadV2 *pQuad = m_SelectedQuads[0];
	pQuad->m_X1 = pQuad->m_X0 + nWidth;
	pQuad->m_Y1 = pQuad->m_Y0 + nHeight;
}


//-----------------------------------------------------------------------------
// Moves selected quad to front/back (works only when 1 quad is selected)
//-----------------------------------------------------------------------------
void CDmeQuadDocV2::MoveSelectedToFront()
{
	if ( m_SelectedQuads.Count() != 1 )
		return;

	m_Quads->MoveToFront( m_SelectedQuads[0] );
}

void CDmeQuadDocV2::MoveSelectedToBack()
{
	if ( m_SelectedQuads.Count() != 1 )
		return;

	m_Quads->MoveToBack( m_SelectedQuads[0] );
}
