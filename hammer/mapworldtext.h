//========= Copyright Valve Corporation, All rights reserved. ============//

#ifndef WORLDTEXTHELPER_H
#define WORLDTEXTHELPER_H
#pragma once


#include "MapHelper.h"

class CRender3D;

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CWorldTextHelper : public CMapHelper
{
public:
	//
	// Factories.
	//
	static CMapClass *CreateWorldText(CHelperInfo *pHelperInfo, CMapEntity *pParent);

	//
	// Construction/destruction:
	//
	CWorldTextHelper();
	virtual ~CWorldTextHelper();

	DECLARE_MAPCLASS( CWorldTextHelper, CMapHelper )

	void SetText( const char *pNewText );

	void CalcBounds(BOOL bFullUpdate = FALSE);

	virtual CMapClass *Copy(bool bUpdateDependencies);
	virtual CMapClass *CopyFrom(CMapClass *pFrom, bool bUpdateDependencies);

	void Initialize();
	void Render2D(CRender2D *pRender);
	void Render3D(CRender3D *pRender);
	void Render3DText( CRender3D *pRender, const char* szText, const float flTextSize );

	void GetAngles(QAngle &Angles);

	int SerializeRMF(std::fstream &File, BOOL bRMF);
	int SerializeMAP(std::fstream &File, BOOL bRMF);

	static void SetRenderDistance(float fRenderDistance);

	bool ShouldRenderLast(void);

	bool IsVisualElement(void) { return(true); }
	
	const char* GetDescription() { return("WorldText"); }

	void OnParentKeyChanged(const char* szKey, const char* szValue);

protected:

	//
	// Implements CMapAtom transformation functions.
	//
	void DoTransform(const VMatrix &matrix);
	void SetRenderMode( int mode );

	QAngle m_Angles;

	int m_eRenderMode;				// Our render mode (transparency, etc.).
	colorVec m_RenderColor;			// Our render color.
	float m_flTextSize;
	char *m_pText;

private:
	void ComputeCornerVertices( Vector *pVerts, float flBloat = 0.0f ) const;

};

#endif // WORLDTEXTHELPER_H
