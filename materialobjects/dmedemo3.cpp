//===== Copyright © 2005-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: build a sheet data file and a large image out of multiple images
//
//===========================================================================//

#include "materialobjects/dmedemo3.h"
#include "tier1/strtools.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier0/dbg.h"


//-----------------------------------------------------------------------------
// Dme version of a quad
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeQuadV3, CDmeQuadV3 );

void CDmeQuadV3::OnConstruction()
{
	m_X0.Init( this, "x0" );
	m_Y0.Init( this, "y0" );	
	m_X1.Init( this, "x1" );
	m_Y1.Init( this, "y1" );	
	m_Color.InitAndSet( this, "color", Color( 255, 255, 255, 255 ) );	
}

void CDmeQuadV3::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Dme version of a list of quads
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeQuadListV3, CDmeQuadListV3 );
void CDmeQuadListV3::OnConstruction()
{
	m_Quads.Init( this, "quads" );
}

void CDmeQuadListV3::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Iteration necessary to render
//-----------------------------------------------------------------------------
int CDmeQuadListV3::GetQuadCount() const
{
	return m_Quads.Count();
}

CDmeQuadV3 *CDmeQuadListV3::GetQuad( int i )
{
	return m_Quads[ i ];
}


//-----------------------------------------------------------------------------
// List management
//-----------------------------------------------------------------------------
void CDmeQuadListV3::AddQuad( CDmeQuadV3 *pQuad )
{
	m_Quads.InsertBefore( 0, pQuad );
}

CDmeQuadV3 *CDmeQuadListV3::FindQuadByName( const char *pName )
{
	for ( int i = 0; i < m_Quads.Count(); ++i )
	{
		if ( !Q_stricmp( pName, m_Quads[i]->GetName() ) )
			return m_Quads[i];
	}
	return NULL;
}

void CDmeQuadListV3::RemoveQuad( CDmeQuadV3 *pQuad )
{
	int nIndex = m_Quads.Find( pQuad );
	if ( nIndex != m_Quads.InvalidIndex() )
	{
		m_Quads.Remove( nIndex );
	}
}

void CDmeQuadListV3::RemoveAllQuads()
{
	m_Quads.RemoveAll();
}


//-----------------------------------------------------------------------------
// Render order management
//-----------------------------------------------------------------------------
void CDmeQuadListV3::MoveToFront( CDmeQuadV3 *pQuad )
{
	int nIndex = m_Quads.Find( pQuad );
	if ( nIndex != m_Quads.InvalidIndex() )
	{
		m_Quads.Remove( nIndex );
		m_Quads.AddToTail( pQuad );
	}
}

void CDmeQuadListV3::MoveToBack( CDmeQuadV3 *pQuad )
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
IMPLEMENT_ELEMENT_FACTORY( DmeQuadDocV3, CDmeQuadDocV3 );
void CDmeQuadDocV3::OnConstruction()
{
	m_QuadList.InitAndCreate( this, "quadList" );	 
	m_SelectedQuads.Init( this, "selectedQuads", FATTRIB_DONTSAVE );
}

void CDmeQuadDocV3::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Iteration necessary to render
//-----------------------------------------------------------------------------
int CDmeQuadDocV3::GetQuadCount() const
{
	return m_QuadList->GetQuadCount();
}

CDmeQuadV3 *CDmeQuadDocV3::GetQuad( int i )
{
	return m_QuadList->GetQuad( i );
}

int CDmeQuadDocV3::GetSelectedQuadCount() const
{
	return m_SelectedQuads.Count();
}

CDmeQuadV3 *CDmeQuadDocV3::GetSelectedQuad( int i )
{
	return m_SelectedQuads[i];
}


//-----------------------------------------------------------------------------
// Adds quad, resets selection to new quad
//-----------------------------------------------------------------------------
void CDmeQuadDocV3::AddQuad( const char *pName, int x0, int y0, int x1, int y1 )
{
	CDmeQuadV3 *pQuadV3 = CreateElement< CDmeQuadV3 >( pName, GetFileId() );
	pQuadV3->m_X0 = x0;
	pQuadV3->m_X1 = x1;
	pQuadV3->m_Y0 = y0;
	pQuadV3->m_Y1 = y1;
	m_QuadList->AddQuad( pQuadV3 );

	ClearSelection();
	AddQuadToSelection( pName );
}


//-----------------------------------------------------------------------------
// Clears selection
//-----------------------------------------------------------------------------
void CDmeQuadDocV3::ClearSelection()
{
	m_SelectedQuads.RemoveAll();
}


//-----------------------------------------------------------------------------
// Adds quad to selection
//-----------------------------------------------------------------------------
void CDmeQuadDocV3::AddQuadToSelection( const char *pName )
{
	CDmeQuadV3 *pQuad = m_QuadList->FindQuadByName( pName );
	if ( pQuad )
	{
		if ( m_SelectedQuads.Find( pQuad ) == m_SelectedQuads.InvalidIndex() )
		{
			m_SelectedQuads.AddToTail( pQuad );
		}
	}
}


//-----------------------------------------------------------------------------
// Add quads in rect to selection
//-----------------------------------------------------------------------------
void CDmeQuadDocV3::AddQuadsInRectToSelection( int x0, int y0, int x1, int y1 )
{
	if ( x0 > x1 )
	{
		swap( x0, x1 );
	}
	if ( y0 > y1 )
	{
		swap( y0, y1 );
	}

	int nCount = m_QuadList->GetQuadCount();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeQuadV3 *pQuad = m_QuadList->GetQuad( i );

		if ( x0 < pQuad->MaxX() && x1 > pQuad->MinX() && y0 < pQuad->MaxY() && y1 > pQuad->MinY() )
		{
			if ( m_SelectedQuads.Find( pQuad ) == m_SelectedQuads.InvalidIndex() )
			{
				m_SelectedQuads.AddToTail( pQuad );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Is point in selected quad?
//-----------------------------------------------------------------------------
bool CDmeQuadDocV3::IsPointInSelectedQuad( int x, int y ) const
{
	int nCount = m_SelectedQuads.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeQuadV3 *pQuad = m_SelectedQuads[i];
		if ( x >= pQuad->MinX() && x <= pQuad->MaxX() && y >= pQuad->MinY() && y <= pQuad->MaxY() )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Deletes selected quads
//-----------------------------------------------------------------------------
void CDmeQuadDocV3::DeleteSelectedQuads()
{
	int nCount = m_SelectedQuads.Count();
	for ( int i = nCount; --i >= 0; )
	{
		CDmeQuadV3 *pQuad = m_SelectedQuads[i];
		m_SelectedQuads.FastRemove( i );
		DestroyElement( pQuad );
	}
}


//-----------------------------------------------------------------------------
// Changes quad color
//-----------------------------------------------------------------------------
void CDmeQuadDocV3::SetSelectedQuadColor( int r, int g, int b, int a )
{
	Color c( r, g, b, a );
	int nCount = m_SelectedQuads.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeQuadV3 *pQuad = m_SelectedQuads[i];
		pQuad->m_Color.Set( c );
	}
}


//-----------------------------------------------------------------------------
// Moves quads
//-----------------------------------------------------------------------------
void CDmeQuadDocV3::MoveSelectedQuads( int dx, int dy )
{
	int nCount = m_SelectedQuads.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeQuadV3 *pQuad = m_SelectedQuads[i];
		pQuad->m_X0 += dx;
		pQuad->m_X1 += dx;
		pQuad->m_Y0 += dy;
		pQuad->m_Y1 += dy;
	}
}


//-----------------------------------------------------------------------------
// Resizes selected quad (works only when 1 quad is selected)
//-----------------------------------------------------------------------------
void CDmeQuadDocV3::ResizeSelectedQuad( int nWidth, int nHeight )
{
	if ( m_SelectedQuads.Count() != 1 )
		return;

	CDmeQuadV3 *pQuad = m_SelectedQuads[0];
	pQuad->m_X1 = pQuad->m_X0 + nWidth;
	pQuad->m_Y1 = pQuad->m_Y0 + nHeight;
}


//-----------------------------------------------------------------------------
// Moves selected quad to front/back (works only when 1 quad is selected)
//-----------------------------------------------------------------------------
void CDmeQuadDocV3::MoveSelectedToFront()
{
	if ( m_SelectedQuads.Count() != 1 )
		return;

	m_QuadList->MoveToFront( m_SelectedQuads[0] );
}

void CDmeQuadDocV3::MoveSelectedToBack()
{
	if ( m_SelectedQuads.Count() != 1 )
		return;

	m_QuadList->MoveToBack( m_SelectedQuads[0] );
}
