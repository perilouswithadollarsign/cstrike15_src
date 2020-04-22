//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SFHUDITEMPICKUPPANEL_H
#define SFHUDITEMPICKUPPANEL_H
#ifdef _WIN32
#pragma once
#endif //_WIN32

#include "scaleformui/scaleformui.h"
#include "GameEventListener.h"
#include "game/client/iviewport.h"
#include "matchmaking/imatchframework.h"
#include "ienginevgui.h"
#include "gameui_util.h"
#include "../VGUI/counterstrikeviewport.h"

#define ITEMDROP_NUM_SFPANELS		4

class SFHudItemPickupPanel : public ScaleformFlashInterface, public CGameEventListener, public IMatchEventsSink
{
protected:
	static SFHudItemPickupPanel *m_pInstance;

	SFHudItemPickupPanel();
	~SFHudItemPickupPanel();

	//
	// IMatchEventsSink
	//
public:
	virtual void OnEvent( KeyValues *pEvent ) OVERRIDE;

public:
	static void LoadDialog( void );
	static void UnloadDialog( void );
	static void StaticShowPanel( bool bShow );
	static void NotifyCommendationResponse( XUID xuid, bool bSuccess );

	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );	
	virtual void PostUnloadFlash( void );

	void ShowItemPickup( XUID xuid, int nIndex );

	void HideFromScript( SCALEFORM_CALLBACK_ARGS_DECL );
	void ItemPickupClose( SCALEFORM_CALLBACK_ARGS_DECL );
	void NextItem( SCALEFORM_CALLBACK_ARGS_DECL );
	void PrevItem( SCALEFORM_CALLBACK_ARGS_DECL );
	void DiscardItem( SCALEFORM_CALLBACK_ARGS_DECL );
	void OpenLoadout( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnConfirmDelete( SCALEFORM_CALLBACK_ARGS_DECL );

	// CGameEventListener methods
	virtual void FireGameEvent( IGameEvent *event );

	void Show( void );
	void Hide( void );

	enum ItemDropAnimType_t
	{	
		ITEM_ANIM_NONE = 0,
		ITEM_ANIM_FIRST = 1,
		ITEM_ANIM_NEXT = 2,
		ITEM_ANIM_PREV = 3,
	};

	void OnCommand( const char *command );
	void UpdateModelPanels( ItemDropAnimType_t anim = ITEM_ANIM_NONE );
	static void AddItem( CEconItemView *pItem );
	void AddItemInternal( CEconItemView *pItem );
	static void SetReturnToGame( bool bReturn ) { m_pInstance ? m_pInstance->m_bReturnToGame = bReturn : NULL; }

	virtual void ShowPanel( bool state );

	static bool IsActive() { return m_pInstance != NULL; }
	static bool IsVisible() { return (m_pInstance != NULL && m_pInstance->m_bVisible); }

	bool m_bVisible;

	// keeps track of whether flash is ready or not and if it's currently being loaded
	bool m_bFlashReady : 1;

	int		m_iSplitScreenSlot;
	bool	m_bLoading;
	bool	m_bDestroyAfterLoading;
	
protected:
	bool m_bReturnToGame;
	int m_iSelectedItem;

	ItemDropAnimType_t m_lastItemAnim;

	struct founditem_t
	{
		CEconItemView				pItem;
		bool						bDiscarded;
	};
	CUtlVector<founditem_t>			m_aItems;

	float	m_flLastAddItemSound;
};

#endif // SFHUDITEMPICKUPPANEL_H
