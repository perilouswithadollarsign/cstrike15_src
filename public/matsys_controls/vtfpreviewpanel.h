//====== Copyright © 1996-2003, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef VTFPREVIEWPANEL_H
#define VTFPREVIEWPANEL_H

#ifdef _WIN32
#pragma once
#endif


#include "vgui_controls/Panel.h"
#include "tier1/utlstring.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "mathlib/vector.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
//
// VTF Preview panel
//
//-----------------------------------------------------------------------------
class CVTFPreviewPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CVTFPreviewPanel, vgui::Panel );

public:
	// constructor
	CVTFPreviewPanel( vgui::Panel *pParent, const char *pName );
	void SetVTF( const char *pFullPath, bool bLoadImmediately = true );
	void SetTwoVTFs( const char *pFullPath, const char *pSecondFullPath );
	const char *GetVTF() const;
	const char *GetSecondVTF() const;

	// Paints the texture
	virtual void Paint( void );

private:
	void PaintNormalMapTexture( void );
	void PaintCubeTexture( void );
	void PaintStandardTexture( void );
	void PaintVolumeTexture( void );

	// Set up a projection matrix for a 90 degree fov
	void SetupProjectionMatrix( int nWidth, int nHeight );

	// Sets the camera to look at the the thing we're spinning around
	void LookAt( const Vector &vecLookAt, float flRadius );

	// Draw a sphere
	void RenderSphere( const Vector &vCenter, float flRadius, int nTheta, int nPhi );

	CUtlString m_VTFName;
	CUtlString m_SecondVTFName;
	CTextureReference m_PreviewTexture;
	CTextureReference m_SecondPreviewTexture;
	CMaterialReference m_PreviewMaterial;
	int m_nTextureID;
	Vector m_vecCameraDirection;
	float m_flLastRotationTime;
};


#endif // VTFPREVIEWPANEL_H
