//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "Box3D.h"
#include "fgdlib/HelperInfo.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/IMesh.h"
#include "MapDoc.h"
#include "maplineoccluder.h"
#include "MapView2D.h"
#include "Material.h"
#include "mathlib/MathLib.h"
#include "Render2D.h"
#include "Render3D.h"
#include "ToolManager.h"
#include "ToolSphere.h"
//#include "..\FoW\FoW.h"
//#include "..\fow\fow_2dplane.h"
//#include "..\fow\fow_lineoccluder.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapLineOccluder)


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapLineOccluder helper from a
//			set of string parameters from the FGD file.
// Input  : pInfo - Pointer to helper info class which gives us information
//				about how to create the helper.
// Output : Returns a pointer to the helper, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapLineOccluder::Create(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	CMapLineOccluder *pOccluder = new CMapLineOccluder;
	if (pOccluder != NULL)
	{
		//
		// The first parameter should be the key name to represent. If it isn't
		// there we assume "radius".
		//
#if 0
		const char *pszKeyName = pHelperInfo->GetParameter(0);
		if (pszKeyName != NULL)
		{
			strcpy(pOccluder->m_szKeyName, pszKeyName);
		}
		else
		{
			strcpy(pOccluder->m_szKeyName, "radius");
		}
#endif
		//
		// Extract the line color from the parameter list.
		//
		unsigned char chRed = 255;
		unsigned char chGreen = 255;
		unsigned char chBlue = 255;

		const char *pszParam = pHelperInfo->GetParameter(1);
		if (pszParam != NULL)
		{
			chRed = atoi(pszParam);
		}

		pszParam = pHelperInfo->GetParameter(2);
		if (pszParam != NULL)
		{
			chGreen = atoi(pszParam);
		}

		pszParam = pHelperInfo->GetParameter(3);
		if (pszParam != NULL)
		{
			chBlue = atoi(pszParam);
		}

		pOccluder->SetRenderColor(chRed, chGreen, chBlue);
	}

	return pOccluder;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CMapLineOccluder::CMapLineOccluder( bool AddToFoW ) :
	CMapHelper()
{
//	m_szKeyName[0] = '\0';

//	m_flRadius = 0;

	r = 255;
	g = 0;
	b = 0;

	SetVisible( true );
	SetVisible2D( true );

	m_pLineOccluder = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor.
//-----------------------------------------------------------------------------
CMapLineOccluder::~CMapLineOccluder(void)
{
	// [smessick] commented out missing FOW stuff
#if 0
	if ( m_pLineOccluder != NULL )
	{
		delete m_pLineOccluder;
		m_pLineOccluder = NULL;
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bFullUpdate - 
//-----------------------------------------------------------------------------
void CMapLineOccluder::CalcBounds(BOOL bFullUpdate)
{
	CMapClass::CalcBounds(bFullUpdate);

	//
	// Pretend we're a point so that we don't change our parent entity bounds
	// in the 2D view.
	//
	m_Render2DBox.ResetBounds();
	m_Render2DBox.UpdateBounds(m_vStart, m_vEnd);

	//
	// Build our bounds for frustum culling in the 3D views.
	//
	m_CullBox.ResetBounds();
	m_CullBox.UpdateBounds(m_vStart, m_vEnd);

	m_BoundingBox.ResetBounds(); // we don't want to use the bounds of the sphere for our bounding box
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapLineOccluder::Copy(bool bUpdateDependencies)
{
	CMapLineOccluder *pCopy = new CMapLineOccluder( false );

	if (pCopy != NULL)
	{
		pCopy->CopyFrom(this, bUpdateDependencies);
	}

	return(pCopy);
}


//-----------------------------------------------------------------------------
// Purpose: Makes this an exact duplicate of pObject.
// Input  : pObject - Object to copy.
// Output : Returns this.
//-----------------------------------------------------------------------------
CMapClass *CMapLineOccluder::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
#if 0
	Assert(pObject->IsMapClass(MAPCLASS_TYPE(CMapLineOccluder)));
	CMapLineOccluder *pFrom = (CMapLineOccluder *)pObject;

	CMapClass::CopyFrom(pObject, bUpdateDependencies);

	m_flRadius = pFrom->m_flRadius;
	strcpy(m_szKeyName, pFrom->m_szKeyName);
#endif

	return(this);
}


void CMapLineOccluder::SetParent(CMapAtom *pParent)
{
	__super::SetParent( pParent );

	// [smessick] commented out missing FOW stuff
#if 0
	CMapEntity	*pParentEnt = dynamic_cast< CMapEntity * >( pParent );
	CFoW		*pFoW = CMapDoc::GetActiveMapDoc()->GetFoW();

	if ( pFoW != NULL && pParentEnt != NULL )
	{
		Vector2D	vStart, vEnd;
		Vector		vPlane;
		int			nSliceNum;

		sscanf( pParentEnt->GetKeyValue( "start" ), "%g %g", &vStart.x, &vStart.y );
		sscanf( pParentEnt->GetKeyValue( "end" ), "%g %g", &vEnd.x, &vEnd.y );
		sscanf( pParentEnt->GetKeyValue( "plane" ), "%g %g %g", &vPlane.x, &vPlane.y, &vPlane.z );
		nSliceNum = atoi( pParentEnt->GetKeyValue( "slice_num" ) );

		CFOW_2DPlane	Plane( vPlane.z, Vector2D( vPlane.x, vPlane.y ) );

		m_pLineOccluder = new CFoW_LineOccluder( vStart, vEnd, Plane, nSliceNum );

		pFoW->AddTriSoupOccluder( m_pLineOccluder, nSliceNum );

		float	flZPos;

		flZPos = pFoW->GetSliceZPosition( nSliceNum );

		m_vStart.Init( vStart.x, vStart.y, flZPos );
		m_vEnd.Init( vEnd.x, vEnd.y, flZPos );

		pParentEnt->SetOrigin( ( m_vStart + m_vEnd ) / 2 );
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Gets the tool object for a given context data from HitTest2D.
//-----------------------------------------------------------------------------
CBaseTool *CMapLineOccluder::GetToolObject(int nHitData, bool bAttachObject)
{
#if 0
	CToolSphere *pTool = (CToolSphere *)ToolManager()->GetToolForID(TOOL_SPHERE);

	if ( bAttachObject )
		pTool->Attach(this);

	return pTool;
#endif
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pView - 
//			point - point in client coordinates
// Output : 
//-----------------------------------------------------------------------------
bool CMapLineOccluder::HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData)
{
#if 0
	if ( m_flRadius <= 0 )
	{
		return NULL;
	}

	Vector2D vecClientOrigin;
	pView->WorldToClient(vecClientOrigin, m_Origin);

	Vector vecRadius = m_Origin;
	vecRadius[pView->axHorz] += m_flRadius;

	Vector2D vecClientRadius;
	pView->WorldToClient(vecClientRadius, vecRadius);

	int nRadius = abs(vecClientRadius.x - vecClientOrigin.x);

	vecClientRadius.x = nRadius;
	vecClientRadius.y = nRadius;

	HitData.pObject = this;
	HitData.nDepth = 0; // handles have no depth
	HitData.uData = nRadius;

	Vector2D vecClientMin = vecClientOrigin - vecClientRadius;
	Vector2D vecClientMax = vecClientOrigin + vecClientRadius;

	//
	// Check the four resize handles.
	//
	Vector2D vecTemp(vecClientOrigin.x, vecClientMin.y - HANDLE_OFFSET);

	if (pView->CheckDistance(point, vecTemp, 6))
	{
		// Top handle
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZENS));
		return true;
	}

	vecTemp.x = vecClientOrigin.x;
	vecTemp.y = vecClientMax.y + HANDLE_OFFSET;
	if (pView->CheckDistance(point, vecTemp, HANDLE_RADIUS))
	{
		// Bottom handle
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZENS));
		return true;
	}

	vecTemp.x = vecClientMin.x - HANDLE_OFFSET;
	vecTemp.y = vecClientOrigin.y;
	if (pView->CheckDistance(point, vecTemp, HANDLE_RADIUS))
	{
		// Left handle
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
		return true;
	}

	vecTemp.x = vecClientMax.x + HANDLE_OFFSET;
	vecTemp.y = vecClientOrigin.y;
	if (pView->CheckDistance(point, vecTemp, HANDLE_RADIUS))
	{
		// Right handle
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
		return true;
	}

	HitData.pObject = NULL;
#endif
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Notifies that this object's parent entity has had a key value change.
// Input  : szKey - The key that changed.
//			szValue - The new value of the key.
//-----------------------------------------------------------------------------
void CMapLineOccluder::OnParentKeyChanged(const char *szKey, const char *szValue)
{
#if 0
	if (!stricmp(szKey, m_szKeyName))
	{
		m_flRadius = atof(szValue);
		PostUpdate(Notify_Changed);

		CFoW	*pFoW = CMapDoc::GetActiveMapDoc()->GetFoW();
		if ( pFoW )
		{
			pFoW->UpdateOccluderSize( m_FoWHandle, m_flRadius );
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : 
//-----------------------------------------------------------------------------
void CMapLineOccluder::OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren)
{
//	CFoW	*pFoW = CMapDoc::GetActiveMapDoc()->GetFoW();
}


#define MAX_SLICE_COLORS	5

static unsigned char nVerticalColors[ MAX_SLICE_COLORS ][ 3 ] =
{
	{ 127, 127, 127 },
	{ 255, 255, 255 },
	{ 255, 0, 0 },
	{ 0, 255, 0 },
	{ 255, 255, 0 }
};



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapLineOccluder::Render2D(CRender2D *pRender)
{
	// [smessick] commented out missing FOW stuff
#if 0
	if ( m_pLineOccluder == NULL )
	{
		return;
	}

	int nSliceNum = m_pLineOccluder->GetSliceNum();

	if ( nSliceNum < MAX_SLICE_COLORS )
	{
		pRender->SetDrawColor( nVerticalColors[ nSliceNum ][ 0 ], nVerticalColors[ nSliceNum ][ 1 ], nVerticalColors[ nSliceNum ][ 2 ] );
	}
	else
	{
		pRender->SetDrawColor( 255, 255, 255 );
	}
	pRender->DrawLine( m_vStart, m_vEnd );

#if 0
	if ( m_flRadius > 0 )
	{
		pRender->SetDrawColor( 255, 0, 0 );

		Vector2D ptClientRadius;
		pRender->TransformNormal(ptClientRadius, Vector(m_flRadius,m_flRadius,m_flRadius) );

		int radius = ptClientRadius.x;

		pRender->DrawCircle( m_Origin, m_flRadius );

		bool bPopMode = pRender->BeginClientSpace();

		pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_SQUARE );
		pRender->SetHandleColor( 255,0,0 );
		if ( IsSelected() )
		{
			//
			// Draw the four resize handles.
			//
			Vector2D offset;
			offset.x = 0;
			offset.y = -(radius + HANDLE_OFFSET);
			pRender->DrawHandle( m_Origin, &offset );

			offset.x = 0;
			offset.y = radius + HANDLE_OFFSET;
			pRender->DrawHandle( m_Origin, &offset );

			offset.x = -(radius + HANDLE_OFFSET);
			offset.y = 0;
			pRender->DrawHandle( m_Origin, &offset );

			offset.x = radius + HANDLE_OFFSET;
			offset.y = 0;
			pRender->DrawHandle( m_Origin, &offset );
		}

		pRender->DrawHandle( m_Origin, &vec2_origin );

		if ( bPopMode )
			pRender->EndClientSpace();
	}
#endif
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Renders the wireframe sphere.
// Input  : pRender - Interface to renderer.
//-----------------------------------------------------------------------------
void CMapLineOccluder::Render3D(CRender3D *pRender)
{
	// [smessick] commented out missing FOW stuff
#if 0
	if ( m_pLineOccluder == NULL )
	{
		return;
	}

	pRender->PushRenderMode( RENDER_MODE_WIREFRAME );

	int nSliceNum = m_pLineOccluder->GetSliceNum();

	if ( nSliceNum < MAX_SLICE_COLORS )
	{
		pRender->SetDrawColor( nVerticalColors[ nSliceNum ][ 0 ], nVerticalColors[ nSliceNum ][ 1 ], nVerticalColors[ nSliceNum ][ 2 ] );
	}
	else
	{
		pRender->SetDrawColor( 255, 255, 255 );
	}
	pRender->DrawLine( m_vStart, m_vEnd );

#if 0
	if ( m_flRadius > 0 )
	{
		pRender->RenderWireframeSphere(m_Origin, m_flRadius, 12, 12, 255, 0, 0);
	}
#endif

	pRender->PopRenderMode();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: Overridden to chain down to our endpoints, which are not children.
//-----------------------------------------------------------------------------
void CMapLineOccluder::SetOrigin(Vector &vecOrigin)
{
	BaseClass::SetOrigin(vecOrigin);
}


//-----------------------------------------------------------------------------
// Purpose: Overridden to chain down to our endpoints, which are not children.
//-----------------------------------------------------------------------------
SelectionState_t CMapLineOccluder::SetSelectionState(SelectionState_t eSelectionState)
{
	SelectionState_t eState = BaseClass::SetSelectionState(eSelectionState);

	return eState;
}


//-----------------------------------------------------------------------------
// Purpose: Overridden to transform our endpoints.
//-----------------------------------------------------------------------------
void CMapLineOccluder::DoTransform(const VMatrix &matrix)
{
	BaseClass::DoTransform(matrix);
}


void CMapLineOccluder::SetRadius(float flRadius)
{
}
