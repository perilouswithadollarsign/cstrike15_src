//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMEUIDEFINITION_H
#define GAMEUIDEFINITION_H

#ifdef _WIN32
#pragma once
#endif

#include "dmxloader/dmxelement.h"
#include "gamegraphic.h"
#include "gameuischeme.h"
#include "tier1/utlobjectreference.h"
#include "tier1/utldict.h"
#include "gamestage.h"
#include "tier1/UtlStringMap.h"


class CGameLayer;
class CGameUIScript;
enum EScriptExecution;
struct GameUIEvent_t;

struct GameGraphicMap_t 
{
	DmObjectId_t m_Id;
	CGameGraphic *pGraphic;	
};

//-----------------------------------------------------------------------------
// A template describing how a gameUI will function
// It loads CVguiCompiledDoc 
//-----------------------------------------------------------------------------
class CGameUIDefinition
{
	DECLARE_DMXELEMENT_UNPACK();
	DECLARE_REFERENCED_CLASS( CGameUIDefinition );
	
public:
	explicit CGameUIDefinition( IGameUISystem *pGameUISystem = NULL );
	~CGameUIDefinition();

	bool CreateDefault( const char *pName );
	void Shutdown();

	// Serialization, unserialization
	bool Unserialize( CDmxElement *pElement );
	void InitializeScripts();
	void CallInitFunction();
	
	// Scripts associated with the menu all live in a folder by that name.
	// The main script for a menu is the same name as the menu.
	const char *GetName() const { return m_pName; }
	int GetLayerCount();

	void InvalidateSheetSymbols();

	void UpdateGeometry();
	void UpdateRenderTransforms( const Rect_t &viewport );
	void GetRenderData( CUtlVector< LayerRenderLists_t > &renderLists );

	bool ExecuteScript( KeyValues *kvEvent, KeyValues **ppResult );

	void StartPlaying();
	void StopPlaying();
	void AdvanceState();
	void InitAnims();

	IGameUISystem * GetGameUISystem() { return m_pGameUISystem; }
	IGameUIScheme * GetScheme() { return m_hScheme; }

	CGameGraphic *GraphicExists( const char *pName ) const;
	CGameGraphic *FindGraphicByName( const char *pName ) const;

	bool AddGraphicToLayer( CGameGraphic *pGraphic, int nLayerType );
	bool RemoveGraphic( CGameGraphic *pGraphic );
	bool HasGraphic( CGameGraphic *pGraphic );

	CGameGraphic *GetGraphic( int x, int y );
	CHitArea *GetMouseFocus( int x, int y );
	CHitArea *GetNextFocus( CHitArea *pCurrentGraphic );

	virtual void SetVisible( bool bVisible );
	virtual bool GetVisible(){ return m_bVisible; }

	void UpdateAspectRatio( const Rect_t &viewport );
	void SetStageSize( int nWide, int nTall );
	void GetStageSize( Vector2D &stageSize );
	void GetMaintainAspectRatioStageSize( Vector2D &stageSize );


	bool CanAcceptInput() { return m_bVisible && m_bCanAcceptInput; }
	void SetAcceptInput( bool bAcceptInput ) { m_bCanAcceptInput = bAcceptInput; }

	bool IsMouseFocusEqualToKeyboardFocus(){ return bMouseFocusEqualsKeyboardFocus; }

	void BuildScopedGraphicName( CUtlString &name, CGameGraphic *pGraphic );

	CGameUIScript * GetScript() { return m_Scripts.Count() ? m_Scripts[0] : NULL; }


private:
	bool UnserializeLayer( CDmxElement *pLayer, CUtlDict< CGameGraphic *, int > &unserializedGraphicMapping );
	void AddGraphic( CGameGraphic *pGraphic, int layerIndex );

	CUtlString m_pName;
	bool m_bVisible;
	bool m_bCanAcceptInput;
	CUtlVector< CGraphicGroup *> m_Groups;
	CUtlVector< CGameLayer *> m_Layers;
	CUtlVector< CGameUIScript * > m_Scripts;

	IGameUISystem *m_pGameUISystem;
	IGameUIScheme *m_hScheme;
	CGameStage *m_pGameStage;
	bool bMouseFocusEqualsKeyboardFocus;
};



#endif // GAMEUIDEFINITION_H
