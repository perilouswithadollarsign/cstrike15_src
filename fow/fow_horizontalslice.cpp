#include "fow.h"
#include "fow_horizontalslice.h"
#include "fow_viewer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


//-----------------------------------------------------------------------------
// Purpose: empty constructor
//-----------------------------------------------------------------------------
CFoW_HorizontalSlice::CFoW_HorizontalSlice( void )
{
}


//-----------------------------------------------------------------------------
// Purpose: clears the sphere tree of all line occluders
//-----------------------------------------------------------------------------
void CFoW_HorizontalSlice::Clear( void )
{
	m_SphereTree.Purge();
}


//-----------------------------------------------------------------------------
// Purpose: adds a line occluder to the sphere tree
// Input  : pLineOccluder - the occluder to add
//-----------------------------------------------------------------------------
void CFoW_HorizontalSlice::AddHorizontalOccluder( CFoW_LineOccluder *pLineOccluder )
{
	Vector2D	start = pLineOccluder->GetStart();
	Vector2D	end = pLineOccluder->GetEnd();
	Vector2D	diff = ( end - start );
	Sphere_t	bounds;

	bounds.x = ( start.x + end.x ) / 2.0f;
	bounds.y = ( start.y + end.y ) / 2.0f;
	bounds.z = 0.0f;
	bounds.w = sqrt( ( diff.x * diff.x ) + ( diff.y * diff.y ) );


	m_SphereTree.Insert( (void *)pLineOccluder, &bounds );


#if 0
	Sphere_t				TestSphere( -320, -1088, 0.0f, 592.77820 );

	Vector	vDiff = TestSphere.AsVector3D() - bounds.AsVector3D();
	float	flLen = vDiff.Length();

	if ( flLen <= ( bounds.w + TestSphere.w ) )
	{
		Msg( "here" );

		CFoW_LineOccluder		*FixedPointerArray[ FOW_MAX_LINE_OCCLUDERS_TO_CHECK ];
		CUtlVector< void * >	FoundOccluders( ( void ** )FixedPointerArray, FOW_MAX_LINE_OCCLUDERS_TO_CHECK );
		int RealCount = m_SphereTree.IntersectWithSphere( TestSphere, true, FoundOccluders, FOW_MAX_LINE_OCCLUDERS_TO_CHECK );

		bool	bFound = false;

		for ( int i = 0; i < FoundOccluders.Count(); i++ )
		{
			if ( FixedPointerArray[ i ] == pLineOccluder )
			{
				bFound = true;
			}
		}

		if ( bFound == true )
		{
			Msg( "We found it!\n" );
		}
		else
		{
			Msg( "WE LOST IT!!!?" );
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: tests the viewer against all line occluders that are near the viewer
// Input  : pFoW - the main FoW object
//			pViewer - the viewer to obstruct
//-----------------------------------------------------------------------------
void CFoW_HorizontalSlice::ObstructViewer( CFoW *pFoW, CFoW_Viewer *pViewer )
{
	Sphere_t				TestSphere( pViewer->GetLocation().x, pViewer->GetLocation().y, 0.0f, pViewer->GetSize() );
	CFoW_LineOccluder		*FixedPointerArray[ FOW_MAX_LINE_OCCLUDERS_TO_CHECK ];
	CUtlVector< void * >	FoundOccluders( ( void ** )FixedPointerArray, FOW_MAX_LINE_OCCLUDERS_TO_CHECK );

	int RealCount = m_SphereTree.IntersectWithSphere( TestSphere, true, FoundOccluders, FOW_MAX_LINE_OCCLUDERS_TO_CHECK );
	if ( RealCount > FOW_MAX_LINE_OCCLUDERS_TO_CHECK )
	{	
		// we overflowed, what should we do?
	}
//	Msg( "Slice Counts: %d / %d\n", FoundOccluders.Count(), RealCount );

	for ( int i = 0; i < FoundOccluders.Count(); i++ )
	{
		FixedPointerArray[ i ]->ObstructViewer( pFoW, pViewer );
	}
}

#include <tier0/memdbgoff.h>
