//========= Copyright © 2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the regular grid nav as required by DOTA. Builds, renders,
//				and saves out the nav. Traversable edges and cells are determined by
//				picking into the world.
//
//=============================================================================//

#include "stdafx.h"
#include "gridnav.h"
#include "render3dms.h"
#include "mapdoc.h"
#include "filesystem.h"
#include "resource.h"
#include "progdlg.h"

bool CGridNav::sm_bEnabled = false;
float CGridNav::sm_flEdgeSize = 0.0f;
float CGridNav::sm_flOffsetX = 0.0f;
float CGridNav::sm_flOffsetY = 0.0f;
float CGridNav::sm_flTraceHeight = 0.0f;

CGridNav::CGridNav()
	: m_vLatestCameraPos( Vector( 0.0f, 0.0f, 0.0f ) )
	, m_vLatestCameraDir( Vector( 0.0f, 0.0f, 0.0f ) )
	, m_bNeedsCameraRecompute( true )
	, m_flTimeCameraLastMoved( 0.0f )
	, m_nTicksCameraStill( 0 )
	, m_bPreviewActive( false )
{
}
	
void CGridNav::Init( bool bEnabled, float flEdgeSize, float flOffsetX, float flOffsetY, float flTraceHeight )
{
	sm_bEnabled = bEnabled;
	sm_flEdgeSize = flEdgeSize;
	sm_flOffsetX = flOffsetX;
	sm_flOffsetY = flOffsetY;
	sm_flTraceHeight = flTraceHeight;
}


void CGridNav::Render( CRender3D *pRender, const Vector &vViewPos, const Vector &vViewDir )
{
	const float flEdgeSize = sm_flEdgeSize;
	const float flHalfEdgeSize = flEdgeSize * 0.5f;
	const float flHalfEdgeSizeBuffered = flHalfEdgeSize * 0.95f;

	EditorRenderMode_t oldRenderMode = pRender->GetCurrentRenderMode();
	Color oldDrawColor;
	pRender->GetDrawColor( oldDrawColor );
	pRender->SetRenderMode( RENDER_MODE_WIREFRAME );
	

	FOR_EACH_VEC( m_CurrentCells, it )
	{
		const CGridNavCell &cell = m_CurrentCells[it];

		const Color drawColor = ( cell.m_bTraversable ? Color( 0, 255, 0 ) : Color( 255, 0, 0 ) );

		const float CELL_DRAW_LEVITATION = 5.0f;

		const int i = cell.m_nGridPosX;
		const int j = cell.m_nGridPosY;
		const Vector vCenter( i * sm_flEdgeSize + sm_flOffsetX, j * sm_flEdgeSize + sm_flOffsetY, cell.m_flHeight + CELL_DRAW_LEVITATION );

		const float d = flHalfEdgeSizeBuffered;
		const Vector p1( vCenter.x - d, vCenter.y - d, vCenter.z );
		const Vector p2( vCenter.x + d, vCenter.y - d, vCenter.z );
		const Vector p3( vCenter.x + d, vCenter.y + d, vCenter.z );
		const Vector p4( vCenter.x - d, vCenter.y + d, vCenter.z );

		pRender->SetDrawColor( drawColor );
		pRender->DrawLine( p1, p2 );
		pRender->DrawLine( p2, p3 );
		pRender->DrawLine( p3, p4 );
		pRender->DrawLine( p4, p1 );
	}

	pRender->SetRenderMode( oldRenderMode );
	pRender->SetDrawColor( oldDrawColor );
}


void CGridNav::Update( CMapDoc *pMapDoc, const Vector &vViewPos, const Vector &vViewDir )
{
	Assert( pMapDoc );

	bool cameraMoving = ( vViewPos != m_vLatestCameraPos || vViewDir != m_vLatestCameraDir );
	
	m_vLatestCameraPos = vViewPos;
	m_vLatestCameraDir = vViewDir;

	if ( cameraMoving )
	{
		m_bNeedsCameraRecompute = true;
		m_flTimeCameraLastMoved = pMapDoc->GetTime();
		m_nTicksCameraStill = 0;
		return;
	}

	if ( !m_bNeedsCameraRecompute )
		return;

	++m_nTicksCameraStill;

	// don't process until we've been still for long enough both in real time and frame count
	if ( pMapDoc->GetTime() - m_flTimeCameraLastMoved < 0.2f || m_nTicksCameraStill < 10 )
	{
		return;
	}

	// the camera is still, recompute working set of nav cells
	m_bNeedsCameraRecompute = false;

	Vector vPickHitPos;
	if ( !pMapDoc->PickTrace( vViewPos, vViewDir, &vPickHitPos ) )
		return;

	m_CurrentCells.RemoveAll();

	const int CELL_GRAB_RADIUS = 15;
	const float flEdgeSize = sm_flEdgeSize;
	const float flHalfEdgeSize = flEdgeSize * 0.5f;
	const float flCenterToCornerLen = sqrtf( 2.f ) * flHalfEdgeSize;

	const int nCenterI = CoordToGridPosX( vPickHitPos.x );
	const int nCenterJ = CoordToGridPosY( vPickHitPos.y );

	for ( int j = -CELL_GRAB_RADIUS; j <= CELL_GRAB_RADIUS; ++j )
	{
		const int curJ = nCenterJ + j;			
		const float y = GridPosYToCoordCenter( curJ );
		
		for ( int i = -CELL_GRAB_RADIUS; i <= CELL_GRAB_RADIUS; ++i )
		{
			const int curI = nCenterI + i;
			const float x = GridPosXToCoordCenter( curI );

			const Vector vTracePos( x, y, sm_flTraceHeight );
			Vector vTraceHitPos;
			bool bHitClip = false;
			if ( !pMapDoc->DropTraceOnDisplacementsAndClips( vTracePos, &vTraceHitPos, &bHitClip ) )
				continue;

			const float centerHeight = vTraceHitPos.z;
			float maxHeight = centerHeight;				

			// reject cells with centers outside a tolerance of the view vector
			const Vector vViewToCenter = vTraceHitPos - vViewPos;
			const Vector vViewToCenterDir = vViewToCenter.Normalized();
			if ( DotProduct( vViewToCenterDir, vViewDir ) < 0.8f )
			{
				continue;
			}

			const float d = flHalfEdgeSize;
			Vector cornerTracePoints[] = {
				Vector( vTracePos.x - d, vTracePos.y + d, vTracePos.z ),
				Vector( vTracePos.x - d, vTracePos.y - d, vTracePos.z ),
				Vector( vTracePos.x + d, vTracePos.y + d, vTracePos.z ),
				Vector( vTracePos.x + d, vTracePos.y - d, vTracePos.z )
			};

			bool bTracesOk = true;
			bool bSlopesWalkable = true;
			for ( int nCorner = 0; nCorner < 4; ++nCorner )
			{
				Vector vCornerTraceHitPos;
				bool bCornerHitClip;
				if ( !pMapDoc->DropTraceOnDisplacementsAndClips( cornerTracePoints[nCorner], &vCornerTraceHitPos, &bCornerHitClip ) )
				{
					bTracesOk = false;
					break;
				}
				else
				{
					bHitClip = bHitClip || bCornerHitClip;

					maxHeight = max( vCornerTraceHitPos.z, maxHeight );

					float flDelta = fabs( vCornerTraceHitPos.z - centerHeight );
					if ( flDelta > flCenterToCornerLen ) // slope > 45 degrees
					{
						bSlopesWalkable = false;
					}
				}
			}
			if ( !bTracesOk )
				continue; // this cell is invalid because we encountered a bad trace, try next cell

			CGridNavCell newCell;
			newCell.m_nGridPosX = curI;
			newCell.m_nGridPosY = curJ;
			newCell.m_bTraversable = !bHitClip && bSlopesWalkable;
			newCell.m_flHeight = maxHeight;
			m_CurrentCells.AddToTail( newCell );
		}
	}
}

void CGridNav::GenerateGridNavFile( const char *pFileFullPath )
{
	Assert( pFileFullPath );

	CMapDoc *pMapDoc = CMapDoc::GetActiveMapDoc();
	if ( !pMapDoc )
		return;

	// Error if we can't open the file for writing
	if ( g_pFullFileSystem->FileExists( pFileFullPath, NULL ) && !g_pFullFileSystem->IsFileWritable( pFileFullPath, NULL ) )
	{
		//AfxMessageBox( NULL, "Grid nav file already exists and is not writable. Unable to generate grid nav.", "Error", MB_OK );
		AfxMessageBox( "Grid nav file already exists and is not writable. Unable to generate grid nav.");
		return;
	}

	// Progress dialog
	CProgressDlg *pProgress = new CProgressDlg;
	pProgress->Create();
	pProgress->SetStep( 1 );
	pProgress->SetWindowText( "Constructing Navigation Grid..." );
	pProgress->SetRange( 0, 100 );

	// find the edges. Edge test order: NORTH, EAST, SOUTH, WEST. Assumes origin is on map.
	const float EDGE_TEST_MAX = 100000.0f;
	const float EDGE_TEST_TERMINATE_INTERVAL = 1.0f;
	int nGridMinX, nGridMaxX, nGridMinY, nGridMaxY;
	int *result[4] = { &nGridMaxY, &nGridMaxX, &nGridMinY, &nGridMinX };

	for ( int nDir = 0; nDir < 4; ++nDir )
	{
		const int nTestAxis = ( nDir + 1 ) % 2;
		const int nStillAxis = 1 - nTestAxis;
		const float flSign = ( nDir <= 1 ? 1.0f : -1.0f );

		float pos[2];
		float flCurDelta = EDGE_TEST_MAX * 0.5f;
		float flCurDist = EDGE_TEST_MAX * 0.5f;
		float flMaxDist = 0.0f;

		pos[nStillAxis] = 0.0f;

		while ( flCurDelta > EDGE_TEST_TERMINATE_INTERVAL )
		{
			pos[nTestAxis] = flCurDist * flSign;

			// trace here
			Vector vTracePos( pos[0], pos[1], sm_flTraceHeight );
			float moveDir = -1.0f;
			if ( pMapDoc->DropTraceOnDisplacementsAndClips( vTracePos, NULL, NULL ) )
			{
				// we hit something, must move forward
				moveDir = 1.0f;
				flMaxDist = flCurDist;
			}

			flCurDelta *= 0.5f;

			flCurDist += flCurDelta * moveDir;
		}

		(*result[nDir]) = ( nTestAxis == 0 ? CoordToGridPosX( flMaxDist * flSign ) : CoordToGridPosY( flMaxDist * flSign ) );
	}

	int nGridWidth = nGridMaxX - nGridMinX + 1;
	int nGridHeight = nGridMaxY - nGridMinY + 1;

	byte writeByte = 0;
	int nWriteBitPos = 0;

	// open the file for writing
	CUtlBuffer fileBuffer( 1024, 1024 );

	// .gnv file header
	const unsigned int GRID_NAV_MAGIC_NUMBER = 0xFADEBEAD;
	fileBuffer.PutInt( GRID_NAV_MAGIC_NUMBER );
	fileBuffer.PutFloat( sm_flEdgeSize );
	fileBuffer.PutFloat( sm_flOffsetX );
	fileBuffer.PutFloat( sm_flOffsetY );
	fileBuffer.PutInt( nGridWidth );
	fileBuffer.PutInt( nGridHeight );
	fileBuffer.PutInt( nGridMinX );
	fileBuffer.PutInt( nGridMinY );

	const int nMaxProgressVal = nGridHeight * nGridWidth - 1;
	
	const float flEdgeSize = sm_flEdgeSize;
	const float flHalfEdgeSize = flEdgeSize * 0.5f;
	const float flCenterToCornerLen = sqrtf( 2.f ) * flHalfEdgeSize;

	for( int j = 0; j < nGridHeight; ++j )
	{
		int curJ = nGridMinY + j;
		float flCenterY = GridPosYToCoordCenter( curJ );

		float prevHeights[2] = { 0.0f, 0.0f }; // northeast, southeast corners
		bool prevHitClip[2] = { false, false };
		bool bPrevOk = false;

		for( int i = 0; i < nGridWidth; ++i )
		{
			const int nProgressVal = ( 100 * ( j * nGridWidth + i ) ) / nMaxProgressVal;
			pProgress->SetPos( nProgressVal );

			int curI = nGridMinX + i;
			float flCenterX = GridPosXToCoordCenter( curI );
			
			bool bSlopesWalkable = true;
			bool bTracesOk = true;

			// trace to find the center of this cell
			const Vector vTracePos( flCenterX, flCenterY, sm_flTraceHeight );
			Vector vTraceHitPos;
			bool bHitClip = false;
			if ( !pMapDoc->DropTraceOnDisplacementsAndClips( vTracePos, &vTraceHitPos, &bHitClip ) )
			{
				bTracesOk = false;
			}
			else
			{
				const float centerHeight = vTraceHitPos.z;

				const float d = flHalfEdgeSize;
				Vector cornerTracePoints[] = {
					Vector( vTracePos.x - d, vTracePos.y + d, vTracePos.z ),
					Vector( vTracePos.x - d, vTracePos.y - d, vTracePos.z ),	
					Vector( vTracePos.x + d, vTracePos.y + d, vTracePos.z ),
					Vector( vTracePos.x + d, vTracePos.y - d, vTracePos.z )
				};

				for ( int nCorner = 0; nCorner < 4; ++nCorner )
				{
					// after passing west corners that could have been previously computed, reset ok flag
					if ( nCorner == 2 )
					{
						bPrevOk = true;
					}

					Vector vCornerTraceHitPos;
					bool bCornerHitClip;

					// see if we already have the trace results from the previous cell
					if ( nCorner <= 1 && bPrevOk )
					{
						vCornerTraceHitPos = cornerTracePoints[nCorner];
						vCornerTraceHitPos.z = prevHeights[nCorner];
						bCornerHitClip = prevHitClip[nCorner];
					}
					else
					{
						// trace
						if ( !pMapDoc->DropTraceOnDisplacementsAndClips( cornerTracePoints[nCorner], &vCornerTraceHitPos, &bCornerHitClip ) )
						{
							bTracesOk = false;
							break;
						}
					}

					// on east 2 corners, store result for next cell
					if ( nCorner >= 2 )
					{
						prevHeights[nCorner-2] = vCornerTraceHitPos.z;
						prevHitClip[nCorner-2] = bCornerHitClip;
					}

					bHitClip = bHitClip || bCornerHitClip;

					float flDelta = fabs( vCornerTraceHitPos.z - centerHeight );
					if ( flDelta > flCenterToCornerLen ) // slope > 45 degrees
					{
						bSlopesWalkable = false;
					}
				}
			}

			bool bTraversable = !bHitClip && bSlopesWalkable;

			if ( !bTracesOk )
			{
				// this cell is invalid because we encountered a bad trace
				bTraversable = false;
				bPrevOk = false;
			}

			// add resulting bit
			if ( bTraversable )
			{
				writeByte |= ( 1 << nWriteBitPos );
			}			
			++nWriteBitPos;

			// write when byte is full
			if ( nWriteBitPos >= 8 )
			{				
				fileBuffer.PutUnsignedChar( writeByte );
				writeByte = 0;
				nWriteBitPos = 0;
			}
		}
	}

	// write any trailing bits
	if ( nWriteBitPos != 0 )
	{
		fileBuffer.PutUnsignedChar( writeByte );
	}

	// write to file
	if ( !g_pFullFileSystem->WriteFile( pFileFullPath, NULL, fileBuffer ) )
	{
		Warning( "Unable to save %d bytes to %s\n", fileBuffer.Size(), pFileFullPath );
	}

	// Destroy the progress meter
	pProgress->DestroyWindow();
	delete pProgress;
}