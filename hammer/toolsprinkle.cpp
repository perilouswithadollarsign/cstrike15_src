//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the entity/prefab placement tool.
//
//=============================================================================//

#include "stdafx.h"
#include "History.h"
#include "MainFrm.h"
#include "MapDefs.h"
#include "MapSolid.h"
#include "MapDoc.h"
#include "MapWorld.h"
#include "MapView2D.h"
#include "MapView3D.h"
#include "Material.h"
#include "materialsystem/IMesh.h"
#include "Render2D.h"
#include "Render3D.h"
#include "StatusBarIDs.h"
#include "TextureSystem.h"
#include "toolsprinkle.h"
#include "ToolManager.h"
#include "hammer.h"
#include "vgui/Cursor.h"
#include "Selection.h"
#include "vstdlib/random.h"
#include "KeyValues.h"
#include "entitysprinkledlg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#define SPRINKLE_PATH			"scripts/hammer/sprinkle/"


//#pragma warning(disable:4244)


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CToolEntitySprinkle::CToolEntitySprinkle(void)
{
	SetEmpty();

	m_vecPos.Init();

	pSprinkleDlg = NULL;

	m_pSprinkleInfo = NULL;

	m_OrigBrushSize = m_BrushSize = 256;
	m_vMousePoint.Init();
	m_bWorldValid = false;
	m_vWorldMousePoint.Init();

	m_InSizingMode = false;
	m_InDrawMode = false;
	m_bCtrlDown = false;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CToolEntitySprinkle::~CToolEntitySprinkle(void)
{
}


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgoff.h>


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CToolEntitySprinkle::OnActivate()
{
	if ( pSprinkleDlg == NULL )
	{
		pSprinkleDlg = new CEntitySprinkleDlg();
		pSprinkleDlg->Create( CEntitySprinkleDlg::IDD, NULL );
	}

	if ( m_pSprinkleInfo != NULL )
	{
		m_pSprinkleInfo->deleteThis();
		m_pSprinkleInfo = NULL;
	}


	m_pSprinkleInfo = new KeyValues( "Sprinkles" );

	FileFindHandle_t findHandle;
	const char *pFileName = g_pFullFileSystem->FindFirstEx( SPRINKLE_PATH "*.txt", "GAME", &findHandle );
	while( pFileName )
	{
		CString		FullPath = SPRINKLE_PATH;

		FullPath += pFileName;

		KeyValues *pLocalInfo = new KeyValues( "loading" );
		if ( !pLocalInfo->LoadFromFile( g_pFileSystem, FullPath, "GAME" ) )
		{
			pLocalInfo->deleteThis();
		}
		m_pSprinkleInfo->AddSubKey( pLocalInfo );

		pFileName = g_pFullFileSystem->FindNext( findHandle );
	}
	g_pFullFileSystem->FindClose( findHandle );

	pSprinkleDlg->SetSprinkleTypes( m_pSprinkleInfo );

	pSprinkleDlg->ShowWindow( SW_SHOW );
}


void CToolEntitySprinkle::OnDeactivate()
{
	pSprinkleDlg->ShowWindow( SW_HIDE );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pt - 
//			BOOL - 
// Output : 
//-----------------------------------------------------------------------------
int CToolEntitySprinkle::HitTest(CMapView *pView, const Vector2D &ptClient, bool bTestHandles)
{
	return HitRect( pView, ptClient, m_vecPos, 8 )?TRUE:FALSE;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bSave - 
//-----------------------------------------------------------------------------
void CToolEntitySprinkle::FinishTranslation(bool bSave)
{
	Tool3D::FinishTranslation(bSave);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pt - 
//			uFlags - 
//			size - 
// Output : Returns true if the translation delta was nonzero.
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::UpdateTranslation( const Vector &vUpdate, UINT uFlags)
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: determines if any of the special keys ( control, shift, alt ) are pressed
//-----------------------------------------------------------------------------
void CToolEntitySprinkle::DetermineKeysDown( )
{
	m_bCtrlDown = ( ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) != 0 );
//	m_bShiftDown = ( ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) != 0 );
//	m_bAltDown = ( ( GetAsyncKeyState( VK_MENU ) & 0x8000 ) != 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CToolEntitySprinkle::RenderTool2D(CRender2D *pRender)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			point - 
// Output : 
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::OnContextMenu2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pView - 
//			nChar - 
//			nRepCnt - 
//			nFlags - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags)
{
	switch (nChar)
	{
		case VK_RETURN:
		{
			return true;
		}

		case VK_ESCAPE:
		{
			OnEscape();
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			nFlags - 
//			point - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseDown2D(pView, nFlags, vPoint);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : Pre CWnd::OnLButtonUp.
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseUp2D(pView, nFlags, vPoint);

	return true;
}


//-----------------------------------------------------------------------------
// Returns true if the message was handled, false otherwise.
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnMouseMove2D(pView, nFlags, vPoint);

	return true;
}


//-----------------------------------------------------------------------------
// Returns true if the message was handled, false otherwise.
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	DetermineKeysDown();

#if 0
	Vector vecWorld;
	pView->ClientToWorld( vecWorld, vPoint );

	m_MousePoint.x = vecWorld.x;
	m_MousePoint.y = vecWorld.y;
#else
	if ( m_InSizingMode == true )
	{
		DoSizing( vPoint );
	}
	else
	{
		if ( FindWorldMousePoint( pView, vPoint ) == true )
		{
			if ( m_InDrawMode == true )
			{
				PerformSprinkle( false );
			}
		}
	}
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Returns true if the message was handled, false otherwise.
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags)
{
	CMapDoc *pDoc = pView->GetMapDoc();
	if (pDoc == NULL)
	{
		return false;
	}

	switch (nChar)
	{
		case VK_RETURN:
		{
			return true;
		}

		case VK_ESCAPE:
		{
			OnEscape();
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the escape key in the 2D or 3D views.
//-----------------------------------------------------------------------------
void CToolEntitySprinkle::OnEscape(void)
{
	ToolManager()->SetTool( TOOL_PICK_ENTITY );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pView - 
//			nFlags - 
//			point - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	DetermineKeysDown();

	if ( FindWorldMousePoint( pView, vPoint ) == true && m_InSizingMode == false )
	{
		m_InDrawMode = true;
		PerformSprinkle( true );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pView - 
//			nFlags - 
//			point - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint)
{
	DetermineKeysDown();

	m_InDrawMode = false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	DetermineKeysDown();

	if ( FindWorldMousePoint( pView, vPoint ) == true )
	{
		m_InDrawMode = false;
		DoSizing( vPoint );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::OnRMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint )
{
	DetermineKeysDown();

	m_InSizingMode = false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Renders a selection gizmo at our bounds center.
// Input  : pRender - Rendering interface.
//-----------------------------------------------------------------------------
void CToolEntitySprinkle::RenderTool3D(CRender3D *pRender)
{
	if ( m_bWorldValid == true )
	{
		pRender->PushRenderMode( RENDER_MODE_WIREFRAME );
		if ( m_InSizingMode )
		{	// yellow for sizing mode
			pRender->RenderWireframeSphere( m_vWorldMousePoint, m_BrushSize, 12, 12, 255, 255, 0 );
		}
		else 
		{
			if ( m_bCtrlDown == true )
			{
				pRender->RenderWireframeSphere( m_vWorldMousePoint, m_BrushSize, 12, 12, 255, 0, 0 );
			}
			else
			{
				pRender->RenderWireframeSphere( m_vWorldMousePoint, m_BrushSize, 12, 12, 0, 255, 0 );
			}

			KeyValues	*pSprinkleType = pSprinkleDlg->GetSprinkleType( );
			if ( pSprinkleType != NULL )
			{
				float		flGridXSize, flGridYSize;
				float		flXSize, flYSize;
				Vector		vCenter;
				float		flBrushSizeSq = m_BrushSize * m_BrushSize;

				Vector		vOffset = Vector( 0.0f, 0.0f, 64.0f );

				CalcGridInfo( pSprinkleType, flGridXSize, flGridYSize, flXSize, flYSize, vCenter );

				pRender->SetDrawColor( 255, 255, 0 );

				for( float x = -flXSize; x <= flXSize; x += flGridXSize )
				{
					for( float y = -flYSize; y <= flYSize; y += flGridYSize )
					{
#if 0
						if ( ( ( x * x ) + ( y * y ) ) > flBrushSizeSq )
						{
							continue;
						}
#endif
						Vector	vOrigin = vCenter + Vector( x, y, 0.0f );

						if ( CToolEntitySprinkle::FindWorldSpot( vOrigin ) == true )
						{
							Vector	vDelta = vOrigin - m_vWorldMousePoint;

							if ( vDelta.LengthSqr() > flBrushSizeSq )
							{
								continue;
							}

							Vector vEnd = vOrigin + vOffset;

							pRender->DrawLine( vOrigin, vEnd );
						}
					}
				}
			}
		}
		pRender->PopRenderMode();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CToolEntitySprinkle::RemoveMapObjects( Vector &vOrigin, KeyValues *pSprinkleType, int nMode, int nDensity, CUtlVector< CMapEntity * > *pRemovedEntities, CMapEntity *pTouchedEntity )
{
	float						flCheckSizeSq;
	KeyValues					*pBaseInfo = pSprinkleType->FindKey( "base" );
	KeyValues					*pBaseClass = NULL;
	CMapDoc						*pDoc = CMapDoc::GetActiveMapDoc();
	CMapWorld					*pWorld = pDoc->GetMapWorld();
	const CMapEntityList		*pEntityList = pWorld->EntityList_GetList();
	CUtlVector< CMapEntity *>	RemoveList;
	Vector						m_CheckOrigin;

	if ( pTouchedEntity != NULL )
	{
		pTouchedEntity->GetOrigin( m_CheckOrigin );
		flCheckSizeSq = 32 * 32;
	}
	else
	{
		m_CheckOrigin = m_vWorldMousePoint;
		flCheckSizeSq = m_BrushSize * m_BrushSize;
	}

	if ( pBaseInfo != NULL )
	{
		pBaseClass = pBaseInfo->FindKey( "classname" );
	}

	FOR_EACH_OBJ( *pEntityList, pos )
	{
		CMapEntity *pEntity = ( CUtlReference< CMapEntity > )pEntityList->Element( pos );
		if ( pEntity == NULL || pEntity == pTouchedEntity )
		{
			continue;
		}

		if ( pEntity->IsVisible() == false )
		{
			continue;
		}

		Vector	vOrigin;
		pEntity->GetOrigin( vOrigin );
		Vector	vDelta = vOrigin - m_CheckOrigin;
		if ( vDelta.LengthSqr() > flCheckSizeSq )
		{
			continue;
		}

		if ( IsInSprinkle( pSprinkleType, pEntity->GetClassName() ) == false )
		{
			continue;
		}

		bool	bRemove = false;

		if ( pBaseClass != NULL && pEntity->ClassNameMatches( pBaseClass->GetString() )== true )
		{
			bRemove = true;
		}
		else
		{
			for ( KeyValues *pSub = pSprinkleType->GetFirstSubKey() ; pSub != NULL; pSub = pSub->GetNextKey() )
			{
				KeyValues	*pClass = pSub->FindKey( "classname" );
				if ( pClass != NULL && pEntity->ClassNameMatches( pClass->GetString() )== true )
				{
					bRemove = true;
					break;
				}
			}
		}

		if ( nMode == SPRINKLE_MODE_SUBTRACTIVE && pTouchedEntity == NULL )
		{
			if ( RandomInt( 1, 100 ) > nDensity )
			{
				continue;
			}
		}

		if ( bRemove == true )
		{
			RemoveList.AddToTail( pEntity );
			if ( pRemovedEntities != NULL )
			{
				pRemovedEntities->AddToTail( pEntity );
			}
		}
	}

	if ( nMode != SPRINKLE_MODE_OVERWRITE )	
	{
		for( int i = 0; i < RemoveList.Count(); i++ )
		{
			GetHistory()->KeepForDestruction( RemoveList[ i ] );
			pDoc->RemoveObjectFromWorld( RemoveList[ i ], true );
		}
	}
}


static const char *pszReserved[ ] =
{
	"classname",
	"grid",
	NULL
};


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CToolEntitySprinkle::PopulateEntity( CMapEntity *pEntity, KeyValues *pFields )
{
	if ( pFields != NULL )
	{
		for ( KeyValues *pSub = pFields->GetFirstSubKey() ; pSub != NULL; pSub = pSub->GetNextKey() )
		{
			int i;

			for( i = 0; pszReserved[ i ] != NULL; i++ )
			{
				if ( strcmpi( pSub->GetName(), pszReserved[ i ] ) == 0 )
				{
					break;
				}
			}

			if ( pszReserved[ i ] == NULL )
			{
				pEntity->SetKeyValue( pSub->GetName(), pSub->GetString() );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CToolEntitySprinkle::CreateMapObject( Vector &vOrigin, KeyValues *pSprinkleType, int nMode, bool bRandomYaw, CMapEntity *pExisting )
{
	int			nTotal = 0;
	KeyValues	*pUseInfo = NULL;

	for ( KeyValues *pSub = pSprinkleType->GetFirstSubKey() ; pSub != NULL; pSub = pSub->GetNextKey() )
	{
		nTotal += atoi( pSub->GetName() );
	}

	int nPick = RandomInt( 1, nTotal );
	nTotal = 0;
	for ( KeyValues *pSub = pSprinkleType->GetFirstSubKey() ; pSub != NULL; pSub = pSub->GetNextKey() )
	{
		nTotal += atoi( pSub->GetName() );
		if ( nPick <= nTotal )
		{
			pUseInfo = pSub;
			break;
		}
	}

	if ( pUseInfo == NULL )
	{
		return;
	}
	
	KeyValues	*pBaseInfo = pSprinkleType->FindKey( "base" );
	KeyValues	*pClass = pUseInfo->FindKey( "classname" );
	if ( pClass == NULL )
	{
		if ( pBaseInfo != NULL )
		{
			pClass = pBaseInfo->FindKey( "classname" );
		}
	}
	if ( pClass == NULL )
	{
		return;
	}

	CMapEntity *pEntity;
	
	if ( pExisting != NULL )
	{
		pEntity = pExisting;
	}
	else
	{
		pEntity = new CMapEntity;
	}

	pEntity->SetOrigin( vOrigin );
	pEntity->SetClass( pClass->GetString() );

	PopulateEntity( pEntity, pBaseInfo );
	PopulateEntity( pEntity, pUseInfo );

	if ( bRandomYaw == true )
	{
		QAngle	vAngles;

		pEntity->GetAngles( vAngles );
		vAngles[ YAW ] = ( float )RandomInt( 0.0f, 360.0f );
		pEntity->SetAngles( vAngles );
	}

	if ( pExisting == NULL )
	{
		m_pDocument->AddObjectToWorld( pEntity );
	
		GetHistory()->KeepNew( pEntity );

		RemoveMapObjects( vOrigin, pSprinkleType, nMode, 0, NULL, pEntity );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::FindWorldMousePoint( CMapView3D *pView, const Vector2D &vPoint )
{
	m_vMousePoint = vPoint;

	ULONG ulFace;
	CMapClass *pObject = pView->NearestObjectAt( m_vMousePoint, ulFace, FLAG_OBJECTS_AT_RESOLVE_INSTANCES | FLAG_OBJECTS_AT_ONLY_SOLIDS, &m_LocalMatrix );

	m_bWorldValid = false;

	if (pObject != NULL)
	{
		CMapSolid *pSolid = dynamic_cast <CMapSolid *> ( pObject );
		if ( pSolid == NULL )
		{	// Clicked on a point entity - do nothing.
			return false;
		}

		m_LocalMatrix.InverseTR( m_LocalMatrixNeg );

		// Build a ray to trace against the face that they clicked on to
		// find the point of intersection.

		Vector Start, End;
		pView->GetCamera()->BuildRay( vPoint, Start, End);

		Vector HitPos, HitNormal;
		m_pHitFace = pSolid->GetFace( ulFace );
		Vector vFinalStart, vFinalEnd;
		m_LocalMatrixNeg.V3Mul( Start, vFinalStart );
		m_LocalMatrixNeg.V3Mul( End, vFinalEnd );
		if ( m_pHitFace->TraceLineInside( HitPos, HitNormal, vFinalStart, vFinalEnd ) )
		{
			Vector vFinalHitPos, vFinalHitNormal;

			m_LocalMatrix.V3Mul( HitPos, vFinalHitPos );
			vFinalHitNormal = m_LocalMatrix.ApplyRotation( HitNormal );

			m_vWorldMousePoint = vFinalHitPos;
			m_bWorldValid = true;
			//CMapClass *pNewObject = NULL;

			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::FindWorldSpot( Vector &vOrigin )
{
	Vector vStart, vEnd;
	Vector vFinalStart, vFinalEnd;
	Vector vHitPos, vHitNormal;

	vStart = vOrigin - Vector( 0.0f, 0.0f, 600.0f );
	vEnd = vOrigin + Vector( 0.0f, 0.0f, 600.0f );
	m_LocalMatrixNeg.V3Mul( vStart, vFinalStart );
	m_LocalMatrixNeg.V3Mul( vEnd, vFinalEnd );

	if ( m_pHitFace->TraceLineInside( vHitPos, vHitNormal, vFinalStart, vFinalEnd ) )
	{
		Vector vFinalHitPos, vFinalHitNormal;

		m_LocalMatrix.V3Mul( vHitPos, vFinalHitPos );
		vFinalHitNormal = m_LocalMatrix.ApplyRotation( vHitNormal );

		vOrigin = vFinalHitPos;
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::IsInSprinkle( KeyValues *pSprinkleType, const char *pszClassname )
{
	KeyValues	*pBaseInfo = pSprinkleType->FindKey( "base" );
	if ( pBaseInfo != NULL )
	{
		KeyValues *pClass = pBaseInfo->FindKey( "classname" );

		if ( pClass != NULL && strcmpi( pClass->GetString(), pszClassname ) == 0 )
		{
			return true;
		}
	}

	for ( KeyValues *pSub = pSprinkleType->GetFirstSubKey() ; pSub != NULL; pSub = pSub->GetNextKey() )
	{
		KeyValues *pClass = pSub->FindKey( "classname" );

		if ( pClass != NULL && strcmpi( pClass->GetString(), pszClassname ) == 0 )
		{
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
const char *CToolEntitySprinkle::FindField( KeyValues *pSprinkleType, const char *pszClassname, const char *pszFieldName )
{
	KeyValues *pFoundClass = NULL;

	for ( KeyValues *pSub = pSprinkleType->GetFirstSubKey() ; pSub != NULL; pSub = pSub->GetNextKey() )
	{
		KeyValues *pClass = pSub->FindKey( "classname" );

		if ( pClass != NULL && strcmpi( pClass->GetString(), pszClassname ) == 0 )
		{
			pFoundClass = pClass;
			break;
		}
	}

	if ( pFoundClass != NULL )
	{
		pFoundClass = pFoundClass->FindKey( pszFieldName, false );
		if ( pFoundClass != NULL )
		{
			return pFoundClass->GetString();
		}
	}

	KeyValues	*pBaseInfo = pSprinkleType->FindKey( "base" );
	if ( pBaseInfo != NULL )
	{
		pFoundClass = pBaseInfo->FindKey( pszFieldName, false );
		if ( pFoundClass != NULL )
		{
			return pFoundClass->GetString();
		}
	}

	return "";
}


//-----------------------------------------------------------------------------
// Purpose: toggles the sizing mode
// Input  : vPoint - the mouse point
// Output : returns true if successful
//-----------------------------------------------------------------------------
bool CToolEntitySprinkle::DoSizing( const Vector2D &vPoint )
{
	if ( !m_InSizingMode )
	{
		m_InSizingMode = true;
		m_StartSizingPoint = vPoint;
		m_OrigBrushSize = m_BrushSize;
	}
	else
	{
		m_BrushSize = m_OrigBrushSize + ( vPoint.x - m_StartSizingPoint.x );
		if ( m_BrushSize < 1.0f )
		{
			m_BrushSize = 1.0f;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CToolEntitySprinkle::CalcGridInfo( KeyValues *pSprinkleType, float &flGridXSize, float &flGridYSize, float &flXSize, float &flYSize, Vector &vCenter )
{
	flGridXSize = 64;
	flGridYSize = 64;
	if ( pSprinkleDlg->UseDefinitionGridSize() )
	{
		KeyValues	*pBaseInfo = pSprinkleType->FindKey( "base" );
		if ( pBaseInfo != NULL )
		{
			KeyValues	*pGridInfo = pBaseInfo->FindKey( "grid" );
			if ( pGridInfo != NULL )
			{
				sscanf( pGridInfo->GetString(), "%g %g", &flGridXSize, &flGridYSize );
			}
		}
	}
	else
	{
		pSprinkleDlg->GetGridSize( flGridXSize, flGridYSize );
	}
	flXSize = ceil( m_BrushSize / flGridXSize ) * flGridXSize;
	flYSize = ceil( m_BrushSize / flGridYSize ) * flGridXSize;

	vCenter = m_vWorldMousePoint;
	vCenter.x -= fmod( vCenter.x, flGridXSize );
	vCenter.y -= fmod( vCenter.y, flGridYSize );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CToolEntitySprinkle::PerformSprinkle( bool bInitial )
{
	if ( m_bWorldValid == false )
	{
		return;
	}

	KeyValues					*pSprinkleType = pSprinkleDlg->GetSprinkleType( );
	if ( pSprinkleType == NULL )
	{
		return;
	}

	int							nDensity = pSprinkleDlg->GetSprinkleDensity();
	int							nMode = pSprinkleDlg->GetSprinkleMode();
	bool						bRandomYaw = pSprinkleDlg->UseRandomYaw();

	float						flGridXSize, flGridYSize;
	float						flXSize, flYSize;
	float						flBrushSizeSq = m_BrushSize * m_BrushSize;
	Vector						vCenter = m_vWorldMousePoint;
	CUtlVector< CMapEntity * >	ReplacedEntities;

	if ( bInitial == true )
	{
		GetHistory()->MarkUndoPosition( m_pDocument->GetSelection()->GetList(), "Sprinkle" );
	}
	else if ( m_vLastDrawPoint.x == vCenter.x && m_vLastDrawPoint.y == vCenter.y )
	{
		return;
	}

	CalcGridInfo( pSprinkleType, flGridXSize, flGridYSize, flXSize, flYSize, vCenter );

	if ( m_bCtrlDown == true )
	{
		nMode = SPRINKLE_MODE_SUBTRACTIVE;
	}

	m_vLastDrawPoint = vCenter;

	if ( nMode == SPRINKLE_MODE_REPLACE || nMode == SPRINKLE_MODE_OVERWRITE )
	{
		RemoveMapObjects( m_vWorldMousePoint, pSprinkleType, nMode, nDensity, &ReplacedEntities );
	}

	switch( nMode )
	{
		case SPRINKLE_MODE_OVERWRITE:
			for( int i = 0; i < ReplacedEntities.Count(); i++ )
			{
				Vector vOrigin;

				ReplacedEntities[ i ]->GetOrigin( vOrigin );
				CreateMapObject( vOrigin, pSprinkleType, nMode, bRandomYaw, ReplacedEntities[ i ] );
			}
			break;

		case SPRINKLE_MODE_SUBTRACTIVE:
			break;

		default:
			for( float x = -flXSize; x <= flXSize; x += flGridXSize )
			{
				for( float y = -flYSize; y <= flYSize; y += flGridYSize )
				{
#if 0
					if ( ( ( x * x ) + ( y * y ) ) > flBrushSizeSq )
					{
						continue;
					}
#endif
					int nValue = RandomInt( 1, 100 );
					if ( nValue > nDensity )
					{
						continue;
					}

					Vector	vOrigin = vCenter + Vector( x, y, 0.0f );

					if ( CToolEntitySprinkle::FindWorldSpot( vOrigin ) == true )
					{
						Vector	vDelta = vOrigin - m_vWorldMousePoint;

						if ( vDelta.LengthSqr() > flBrushSizeSq )
						{
							continue;
						}

						CreateMapObject( vOrigin, pSprinkleType, nMode, bRandomYaw );
					}
				}
			}
			break;
	}

	if ( nMode == SPRINKLE_MODE_SUBTRACTIVE )
	{
		RemoveMapObjects( m_vWorldMousePoint, pSprinkleType, nMode, nDensity );
	}

	m_pDocument->SetModifiedFlag();
}
