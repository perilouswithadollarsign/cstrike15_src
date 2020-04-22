//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef GAMEUISYSTEM_H
#define GAMEUISYSTEM_H
#ifdef _WIN32
#pragma once
#endif


#include "tier1/timeutils.h"
#include "materialsystem/imaterialsystem.h"
#include "gameuidefinition.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IGameUIScreenController;
class CUtlBuffer;
class IRenderContext;
class IRenderDevice;
class KeyValues;
struct RenderViewport_t;


//-----------------------------------------------------------------------------
// Class to render a zillion ui elements
//-----------------------------------------------------------------------------
class CGameUISystem : public IGameUISystem
{

public:
	CGameUISystem();
	virtual ~CGameUISystem();

	virtual char const * GetName();

	virtual bool Init( KeyValues *kvLoadSettings );
	virtual void Release();

	virtual void Render( const Rect_t &viewport );
	virtual void Render( IRenderContext *pRenderContext, const Rect_t &viewport );

	virtual void LoadEmptyGameUI( const char *pName );
	virtual bool LoadGameUIDefinition( CUtlBuffer &buf, const char *pFileName );

	CGameUIDefinition &Definition() { return m_GameUIDef; }

	virtual bool ExecuteScript( KeyValues *kvEvent, KeyValues **ppResult = NULL );

	virtual void SetStageSize( int nWide, int nTall );
	virtual void GetStageSize( Vector2D &stageSize );

	virtual int32 GetScriptHandle() { return m_iScriptHandle; }
	static CGameUISystem * FromScriptHandle( int32 iScriptHandle );

private:

	void GenerateUIMesh( IMatRenderContext *pRenderContext, IMesh* pMesh, CUtlVector< CRenderGeometry > &renderGeometry, CSheet *pSheet );

	void RenderStaticLayer( LayerRenderLists_t &renderList, int geometryIndex );
	void RenderStaticLayer( IRenderContext *pRenderContext, LayerRenderLists_t &renderList, int geometryIndex );

	void RenderDynamicLayer( LayerRenderLists_t &renderList, int geometryIndex );

	void RenderTextLayer( CUtlVector< CRenderGeometry > &renderGeometry );
	void RenderTextLayer( IRenderContext *pRenderContext, CUtlVector< CRenderGeometry > &renderGeometry );

	CGameUIDefinition m_GameUIDef;

	int32 m_iScriptHandle;
	bool m_bDrawReport;
};


#endif // GAMEUISYSTEM_H