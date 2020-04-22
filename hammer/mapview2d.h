//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef MAPVIEW2D_H
#define MAPVIEW2D_H

#ifdef _WIN32
#pragma once
#endif


#include "MapView2DBase.h"
#include "tier1/utlvector.h"

class CMapInstance;


class CMapView2D : public CMapView2DBase
{
protected:
	CMapView2D();           // protected constructor used by dynamic creation
	virtual ~CMapView2D();
	DECLARE_DYNCREATE(CMapView2D)
	
	virtual bool IsLogical() { return false; }
	virtual void OnRenderListDirty();

	void RenderInstance( CMapInstance *pInstanceClass, CMapClass *pMapClass, Vector &InstanceOrigin, QAngle &InstanceAngles );

private:
	void DrawPointFile( CRender2D *pRender );
	void AddToRenderLists( CMapClass *pObject );
	void RenderFoW( void );
	void Render();
	void SetDrawType( DrawType_t drawType );
	virtual void ActivateView( bool bActivate );
 	bool UpdateRenderObjects();
	void DrawCullingCircleHelper2D( CRender2D *pRender );

	void RenderInstanceMapClass_r( CMapClass *pObject );

	// general variables:	
	bool m_bLastActiveView;					// is this the last active view?
	CUtlVector<CMapClass *> m_RenderList;	// list of current rendered objects
	bool m_bUpdateRenderObjects;			// if true, update render list on next draw

//	bool		m_bInstanceRendering;		// if true, we are rendering an instance
//	Vector		m_InstanceOrigin;			// the origin offset of instance rendering
//	QAngle		m_InstanceAngles;			// the rotation of the instance rendering
//	VMatrix		m_InstanceMatrix;			// matrix of the origin and rotation of rendering
//	matrix3x4_t	m_Instance3x4Matrix;		// matrix of the origin and rotation of rendering

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMapView2D)
	protected:
	virtual void OnInitialUpdate();     // first time after construct
	afx_msg void OnView2dxy();
	afx_msg void OnView2dyz();
	afx_msg void OnView2dxz();
	afx_msg void OnUpdateEditSelection(CCmdUI *pCmdUI);
	afx_msg BOOL OnToolsAlign(UINT nID);
	afx_msg BOOL OnFlip(UINT nID);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

inline bool CMapView2D::UpdateRenderObjects() 
{ 
	return m_bUpdateRenderObjects;	
}


#endif // MAPVIEW2D_H
