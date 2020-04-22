//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMELAYER_H
#define GAMELAYER_H

#ifdef _WIN32
#pragma once
#endif

#include "igameuisystemmgr.h"
#include "gamegraphic.h"
#include "dmxloader/dmxelement.h"
#include "materialsystem/materialsystemutil.h"
#include "bitmap/psheet.h"
#include "rendersystem/irenderdevice.h"
#include "mathlib/vertexcolor.h"
#include "resourcesystem/stronghandle.h"
#include "tier1/utldict.h"
#include "tier1/UtlStringMap.h"

class IRenderContext;
struct GameGraphicMap_t;
class CGameGraphic;
class GameUIDefinition;


//-----------------------------------------------------------------------------
// GameUI vertex format
//-----------------------------------------------------------------------------
struct GameUIVertex_t
{
	Vector m_vecPosition;
	VertexColor_t m_color;
	Vector2D m_vecTexCoord;
};


//-----------------------------------------------------------------------------
// A class that contains all of the rendering objects in a gameui layer
//-----------------------------------------------------------------------------

// There are currently 3 types of these, for static/dynamic/and font.
class CGameLayer
{
	DECLARE_DMXELEMENT_UNPACK()

public:
	CGameLayer( SublayerTypes_t layerType = SUBLAYER_STATIC );
	~CGameLayer();
	void Shutdown();

	bool Unserialize( CDmxElement *pLayer, CUtlDict< CGameGraphic *, int > &unserializedGraphicMapping );

	const char *GetName() const { return m_pName; }
	const SublayerTypes_t GetLayerType() { return m_LayerType; }
	void SetLayerType( SublayerTypes_t layerType ) { m_LayerType = layerType; }

	bool InitSheetTexture();
	bool InitSheetTexture( const char *pBaseTextureName );
	void GetSheetTextureSize( int &nWidth, int &nHeight );
	int AddGraphic( CGameGraphic *pGraphic );
	bool RemoveGraphic( CGameGraphic *pGraphic );
	void ClearGraphics();
	bool HasGraphic( CGameGraphic *pGraphic );
	CGameGraphic *FindGraphicByName( const char *pName );

	IMaterial *GetMaterial();
	HRenderTextureStrong GetTexture() { return m_hTexture; }

	// Sheet symbols (used to avoid string->symbol conversions)
	void InvalidateSheetSymbol();
	void CacheSheetSymbol( CUtlSymbol sheetSymbol );
	bool IsSheetSymbolCached() const { return m_bSheetSymbolCached; }
	CUtlSymbol GetSheetSymbol() const;

	void StartPlaying();
	void StopPlaying();
	void AdvanceState();
	void InitAnims();

	void UpdateGeometry();
	void UpdateRenderTransforms( const StageRenderInfo_t &stageRenderInfo );
	void UpdateRenderData( CGameUIDefinition &gameUIDef, color32 parentColor, CUtlVector< LayerRenderLists_t > &renderLists );

	CGameGraphic *GetGraphic( int x, int y );
	CGameGraphic *GetMouseFocus( int x, int y );
	CGameGraphic *GetNextFocus( bool &bGetNext, CGameGraphic *pCurrentGraphic );


private:

	// Graphics in this layer
	CUtlVector< CGameGraphic * > m_LayerGraphics;

	CSheet *LoadSheet( IMaterial *pMaterial );
	CSheet *LoadSheet( char const *pszFname, ITexture *pTexture );

	CUtlString m_pName;
	SublayerTypes_t m_LayerType;

	CUtlString m_pTextureName;
	HRenderTextureStrong m_hTexture;
	CMaterialReference m_Material;
	CUtlReference< CSheet > m_Sheet;
	CUtlSymbol m_SheetSymbol;
	bool m_bSheetSymbolCached;
	
};


#endif // GAMELAYER_H
