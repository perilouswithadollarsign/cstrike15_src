//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ====
//
// Purpose: Defines the interface to the entity sprinkle tool.
//
//=============================================================================

#ifndef __TOOL_SPRINKLE_H
#define __TOOL_SPRINKLE_H
#pragma once


#include "ToolInterface.h"
#include "Tool3D.h"


class CRender2D;
class CRender3D;
class CEntitySprinkleDlg;

class CToolEntitySprinkle : public Tool3D
{
public:

	CToolEntitySprinkle(void);
	~CToolEntitySprinkle(void);

	inline void GetPos(Vector &vecPos);

	//
	// CBaseTool implementation.
	//
	virtual ToolID_t GetToolID(void) { return TOOL_ENTITY_SPRINKLE; }

	virtual void OnActivate();
	virtual void OnDeactivate();

	virtual bool OnContextMenu2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);

	virtual bool OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint);

	virtual bool OnMouseMove3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags);
	virtual bool OnLMouseDown3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnLMouseUp3D(CMapView3D *pView, UINT nFlags, const Vector2D &vPoint);
	virtual bool OnRMouseDown3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	virtual bool OnRMouseUp3D( CMapView3D *pView, UINT nFlags, const Vector2D &vPoint );
	
	virtual void RenderTool2D(CRender2D *pRender);
	virtual void RenderTool3D(CRender3D *pRender);

protected:

	//
	// Tool3D implementation.
	//
	virtual bool UpdateTranslation(const Vector &vUpdate, UINT flags);
	virtual void FinishTranslation(bool bSave);
	virtual int  HitTest(CMapView *pView, const Vector2D &vPoint, bool bTestHandles = false);

private:

	void DetermineKeysDown( );
	void OnEscape( );
	void RemoveMapObjects( Vector &vOrigin, KeyValues *pSprinkleType, int nMode, int nDensity, CUtlVector< CMapEntity * > *pRemovedLocations = NULL, CMapEntity *pTouchedEntity = NULL );
	void PopulateEntity( CMapEntity *pEntity, KeyValues *pFields );
	void CreateMapObject( Vector &vOrigin, KeyValues *pSprinkleType, int nMode, bool bRandomYaw, CMapEntity *pExisting = NULL );
	bool FindWorldMousePoint( CMapView3D *pView, const Vector2D &vPoint );
	bool FindWorldSpot( Vector &vOrigin );
	bool IsInSprinkle( KeyValues *pSprinkleType, const char *pszClassname );
	const char *FindField( KeyValues *pSprinkleType, const char *pszClassname, const char *pszFieldName );
	bool DoSizing( const Vector2D &vPoint );
	void CalcGridInfo( KeyValues *pSprinkleType, float &flGridXSize, float &flGridYSize, float &flXSize, float &flYSize, Vector &vCenter );
	void PerformSprinkle( bool bInitial );

	Vector						m_vecPos;			// Current position of the marker.
	CEntitySprinkleDlg			*pSprinkleDlg;
	Vector2D					m_vMousePoint;
	Vector						m_vWorldMousePoint;
	Vector						m_vLastDrawPoint;
	VMatrix						m_LocalMatrix;
	VMatrix						m_LocalMatrixNeg;
	CMapFace					*m_pHitFace;
	bool						m_bWorldValid;
	bool						m_InSizingMode;
	bool						m_InDrawMode;
	bool						m_bCtrlDown;
	Vector2D					m_StartSizingPoint;
	float						m_BrushSize;
	float						m_OrigBrushSize;
	KeyValues					*m_pSprinkleInfo;
};


//-----------------------------------------------------------------------------
// Purpose: Returns the current position of the marker.
//-----------------------------------------------------------------------------
inline void CToolEntitySprinkle::GetPos(Vector &vecPos)
{
	vecPos = m_vecPos;
}


#endif // __TOOL_SPRINKLE_H
