//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ====
//
// Purpose: 
//
//=============================================================================

#ifndef MAPVIEWER_H
#define MAPVIEWER_H
#ifdef _WIN32
#pragma once
#endif

#include "fgdlib/WCKeyValues.h"
#include "MapSphere.h"
#include "ToolInterface.h"
#include "MapPointHandle.h"


class CToolSphere;
class CHelperInfo;
class CRender2D;
class CRender3D;
class IMesh;


class CMapViewer : public CMapSphere
{
	friend class CToolSphere;

public:

	DECLARE_MAPCLASS(CMapViewer,CMapHelper)

	//
	// Factory for building from a list of string parameters.
	//
	static CMapClass *Create(CHelperInfo *pInfo, CMapEntity *pParent);

	//
	// Construction/destruction:
	//
	CMapViewer(bool AddToFoW = true);
	~CMapViewer(void);

	virtual void CalcBounds(BOOL bFullUpdate = FALSE);

	virtual CMapClass *Copy(bool bUpdateDependencies);
	virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

	virtual void OnParentKeyChanged(const char *szKey, const char *szValue);
	virtual void OnRemoveFromWorld(CMapWorld *pWorld, bool bNotifyChildren);

	virtual void Render2D(CRender2D *pRender);
	virtual void Render3D(CRender3D *pRender);

	virtual int SerializeRMF(std::fstream &File, BOOL bRMF) { return(0); }
	virtual int SerializeMAP(std::fstream &File, BOOL bRMF) { return(0); }

	virtual bool IsVisualElement(void) { return true; } // Only visible when the parent entity is selected.
	virtual bool IsScaleable(void) const { return false; } // TODO: allow for scaling the sphere by itself
	virtual bool IsClutter(void) const { return false; }
	virtual bool CanBeCulledByCordon() const { return false; } // We don't hide unless our parent hides.

	virtual CBaseTool *GetToolObject(int nHitData, bool bAttachObject );

	virtual bool HitTest2D(CMapView2D *pView, const Vector2D &point, HitInfo_t &HitData);

	virtual const char* GetDescription() { return "FoW Viewer helper"; }

	virtual void SetOrigin(Vector &vecOrigin);
	virtual SelectionState_t SetSelectionState(SelectionState_t eSelectionState);

protected:
	virtual void DoTransform(const VMatrix &matrix);
	virtual void SetRadius(float flRadius);

private:

	int		m_FoWHandle;

};

#endif // MAPVIEWER_H
