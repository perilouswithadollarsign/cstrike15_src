
//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "itempickup_scaleform.h"
#include "item_pickup_panel.h"
#include "game/client/iviewport.h"
#include "scoreboard_scaleform.h"
#include "teammenu_scaleform.h"
#include "c_cs_playerresource.h"
#include "c_cs_player.h"
#include "c_team.h"
#include "voice_status.h"
#include "basepanel.h"
#include "hud_chat.h"
#include "iclientmode.h"
#include "econ_ui.h"
#include "gameui/basemodpanel.h"

#include "components/scaleformcomponent_common.h"
#include "uicomponents/uicomponent_friendslist.h"
#include "uicomponents/uicomponent_mypersona.h"

#include "gc_clientsystem.h"
#include "cstrike15_gcmessages.pb.h"
#include "cstrike15_gcconstants.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

SFHudItemPickupPanel* SFHudItemPickupPanel::m_pInstance = NULL;

SFUI_BEGIN_GAME_API_DEF		
	SFUI_DECL_METHOD( HideFromScript ),
	SFUI_DECL_METHOD( ItemPickupClose ),
	SFUI_DECL_METHOD( NextItem ),
	SFUI_DECL_METHOD( PrevItem ),
	SFUI_DECL_METHOD( DiscardItem ),
	SFUI_DECL_METHOD( OpenLoadout ),
	SFUI_DECL_METHOD( OnConfirmDelete ),
	
SFUI_END_GAME_API_DEF( SFHudItemPickupPanel, ItemPickup );

static CSteamID s_steamIdMyself;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SFHudItemPickupPanel::SFHudItemPickupPanel()
{		
	s_steamIdMyself = steamapicontext->SteamUser()->GetSteamID();
	m_bVisible = false;
	m_iSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	m_bLoading = false;
	m_bDestroyAfterLoading = false;
	m_flLastAddItemSound = 0;

	//g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	m_iSelectedItem = 0;
}

SFHudItemPickupPanel::~SFHudItemPickupPanel()
{
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
	m_lastItemAnim = ITEM_ANIM_NONE;
}

void SFHudItemPickupPanel::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();
	
	SCALEFORM_COMPONENT_FORWARD_EVENT( szEvent );
}

void SFHudItemPickupPanel::LoadDialog( void )
{
	if ( !m_pInstance )
	{
		m_pInstance = new SFHudItemPickupPanel();
		m_pInstance->m_bLoading = true;
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, SFHudItemPickupPanel, m_pInstance, ItemPickup );	
	}
	else if (m_pInstance->m_bLoading)
	{
		if ( m_pInstance->m_bDestroyAfterLoading )
		{
			m_pInstance->m_bDestroyAfterLoading = false;
		}
	}
}

void SFHudItemPickupPanel::UnloadDialog( void )
{
	if ( m_pInstance && !m_pInstance->m_bDestroyAfterLoading)
	{
		if (m_pInstance->m_bLoading)
		{
			m_pInstance->m_bDestroyAfterLoading = true;
			m_pInstance->m_bVisible = false;
		}
		else
		{
			m_pInstance->m_bDestroyAfterLoading = true;
		}

		m_pInstance->Hide();
		m_pInstance->RemoveFlashElement();
	}
}

void SFHudItemPickupPanel::PostUnloadFlash( void )
{
	BasePanel()->DismissPauseMenu();	

	m_pInstance = NULL;
	delete this;
}

void SFHudItemPickupPanel::FlashReady( void )
{
	if ( !m_FlashAPI )
	{
		return;
	}

	m_bFlashReady = true;
	m_bLoading = false;

	ListenForGameEvent( "server_spawn" );

	Hide();	
}

bool SFHudItemPickupPanel::PreUnloadFlash( void )
{
	return true;
}

void SFHudItemPickupPanel::StaticShowPanel( bool bShow )
{
	if ( bShow && !m_pInstance )
	{
		LoadDialog();
	}

	if ( m_pInstance )
	{
		if ( bShow )
		{
			m_pInstance->m_iSelectedItem = 0;
			m_pInstance->Show();
		}
		else
		{
			m_pInstance->Hide();
		}
	}
}

void SFHudItemPickupPanel::ShowPanel( bool bShow )
{
	if ( bShow != m_bVisible )
	{
		if ( bShow )
		{
			Show();
		}
		else
		{
			Hide();
		}
	}
}

void SFHudItemPickupPanel::Show()
{
	if ( !m_pScaleformUI )
		return;

	if ( !m_bLoading )
	{
		if ( FlashAPIIsValid() )
		{
			WITH_SLOT_LOCKED
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );			
			}
		}
		else
		{
			m_bLoading = true;
			SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, SFHudItemPickupPanel, this, ItemPickup );	
		}
	}

	m_bVisible = true;
	m_aItems.Purge();
}

void SFHudItemPickupPanel::Hide( void )
{
	if ( !m_pScaleformUI )
		return;

	if ( m_bVisible )
		OnCommand( "itempickupclose" );

	if ( FlashAPIIsValid() && !m_bLoading )
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", NULL, 0 );			
		}			

		m_bVisible = false;
	}

	m_iSelectedItem = 0;
	m_aItems.Purge();
}

void SFHudItemPickupPanel::ItemPickupClose( SCALEFORM_CALLBACK_ARGS_DECL )
{
	OnCommand( "itempickupclose" );
}

void SFHudItemPickupPanel::NextItem( SCALEFORM_CALLBACK_ARGS_DECL )
{
	OnCommand( "nextitem" );
}

void SFHudItemPickupPanel::PrevItem( SCALEFORM_CALLBACK_ARGS_DECL )
{
	OnCommand( "previtem" );
}

void SFHudItemPickupPanel::DiscardItem( SCALEFORM_CALLBACK_ARGS_DECL )
{
	OnCommand( "discarditem" );
}

void SFHudItemPickupPanel::OpenLoadout( SCALEFORM_CALLBACK_ARGS_DECL )
{
	engine->ClientCmd_Unrestricted("open_econui_backpack\n");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SFHudItemPickupPanel::OnConfirmDelete( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( m_iSelectedItem >= 0 && m_iSelectedItem < m_aItems.Count() )
	{
		if ( m_aItems[m_iSelectedItem].pItem.IsValid() )
		{
			CPlayerInventory *pInventory = InventoryManager()->GetLocalInventory();
			if ( pInventory )
			{
				EconUI()->Gamestats_ItemTransaction( IE_ITEM_DISCARDED, &m_aItems[m_iSelectedItem].pItem );

				InventoryManager()->DeleteItem( m_aItems[m_iSelectedItem].pItem.GetItemID() );

				m_aItems[m_iSelectedItem].bDiscarded = true;

				vgui::surface()->PlaySound( "physics/metal/weapon_impact_hard2.wav" );
				vgui::surface()->PlaySound( "physics/metal/metal_barrel_impact_hard2.wav" );	

				// If we've discarded all our items, we exit immediately
				bool bFoundUndiscarded = false;
				for ( int i = 0; i < m_aItems.Count(); i++ )
				{
					if ( !m_aItems[i].bDiscarded )
					{
						bFoundUndiscarded = true;
						break;
					}
				}

				if ( !bFoundUndiscarded )
				{
					OnCommand( "itempickupclose" );
				}
			}
		}

		UpdateModelPanels();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SFHudItemPickupPanel::OnCommand( const char *command )
{
	if ( !Q_stricmp( command, "itempickupclose" ) )
	{
		//ShowPanel( false );

		// If we were crafting, and the craft panel is up, we return to that instead.
		if ( EconUI()->IsUIPanelVisible( ECONUI_CRAFTING ) )
			return;

		// Check to make sure the player has room for all his items. If not, bring up the discard panel. Otherwise, go away.
		if ( !InventoryManager()->CheckForRoomAndForceDiscard(  ) )
		{
			// If we're connected to a game server, we also close the game UI.
			if ( m_bReturnToGame && engine->IsInGame() )
			{
				engine->ClientCmd_Unrestricted( "gameui_hide" );
			}
		}	
	}
	else if ( !Q_stricmp( command, "changeloadout" ) )
	{
		ShowPanel( false );
		EconUI()->OpenEconUI( ECONUI_LOADOUT, true );
	}
	else if ( !Q_stricmp( command, "nextitem" ) )
	{
		m_iSelectedItem = clamp( m_iSelectedItem+1, 0, m_aItems.Count()-1 );
		UpdateModelPanels( ITEM_ANIM_NEXT );
	}
	else if ( !Q_stricmp( command, "previtem" ) )
	{
		m_iSelectedItem = clamp( m_iSelectedItem-1, 0, m_aItems.Count()-1 );
		UpdateModelPanels( ITEM_ANIM_PREV );
	}
	else if ( !Q_stricmp( command, "discarditem" ) )
	{
		CEconItemView *pItem = &m_aItems[m_iSelectedItem].pItem;	
		if ( pItem )
		{
			// Bring up confirm dialog
			WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, pItem->GetItemName() );
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showConfirmDeletePanel", args, 1 );
			}
		}
	}
	else
	{
		engine->ClientCmd( const_cast<char *>( command ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SFHudItemPickupPanel::UpdateModelPanels( ItemDropAnimType_t anim )
{
	if ( anim != ITEM_ANIM_NONE )
		m_lastItemAnim = anim;

	int nItemsCount = m_aItems.Count();
	bool bDiscarded = false;

	int iPos = 0;
	for ( int i = 0; i < ITEMDROP_NUM_SFPANELS; i++ )
	{
		bool bHideItemPanel = false;
		int nItemUsedByTeam = 0;
		bool bThisItemDiscarded = false;
		int iFoundMethod = 0;
		char xuidText[255];
		xuidText[0] = '\0';
		char itemIDText[255];
		itemIDText[0] = '\0';

		if ( m_lastItemAnim == ITEM_ANIM_FIRST || m_lastItemAnim == ITEM_ANIM_NEXT )
		{
			iPos = (m_iSelectedItem-2) + i;
		}
		else if ( m_lastItemAnim == ITEM_ANIM_PREV )
		{
			iPos = (m_iSelectedItem-1) + i;
		}

		if ( iPos < 0 || iPos >= nItemsCount )
		{
			bHideItemPanel = true;
		}
		else
		{
			CEconItemView *pItem = &m_aItems[iPos].pItem;	
			if ( pItem )
			{
				nItemUsedByTeam = pItem->GetStaticData()->GetUsedByTeam();

				bThisItemDiscarded = m_aItems[iPos].bDiscarded;

				iFoundMethod = GetUnacknowledgedReason( m_aItems[iPos].pItem.GetInventoryPosition() );
				iFoundMethod--;

				if ( iFoundMethod < 0 || iFoundMethod >= ARRAYSIZE(g_pszItemPickupMethodStrings) )
				{
					iFoundMethod = 0;
				}

				XUID xuidMyself = s_steamIdMyself.ConvertToUint64();
				V_snprintf( xuidText, ARRAYSIZE(xuidText), "%llu", xuidMyself );

				itemid_t itemID = pItem->GetItemID(); 
				V_snprintf( itemIDText, ARRAYSIZE(itemIDText), "%llu", itemID );
			}
			else
			{
				bHideItemPanel = true;
			}
		}

		wchar_t szFoundMethodText[ 256 ];
		g_pVGuiLocalize->ConstructString( szFoundMethodText, sizeof( szFoundMethodText ), g_pVGuiLocalize->Find( g_pszItemPickupMethodStrings[iFoundMethod] ), 0 );

		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 7 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, i );
			m_pScaleformUI->ValueArray_SetElement( args, 1, bHideItemPanel );
			m_pScaleformUI->ValueArray_SetElement( args, 2, bThisItemDiscarded );
			m_pScaleformUI->ValueArray_SetElement( args, 3, szFoundMethodText );
			m_pScaleformUI->ValueArray_SetElement( args, 4, nItemUsedByTeam );	
			m_pScaleformUI->ValueArray_SetElement( args, 5, xuidText );	
			m_pScaleformUI->ValueArray_SetElement( args, 6, itemIDText );	
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetItemInSlot", args, 7 );
		}
	}

	bool bAllowDiscard = true;

	int iFoundMethod = 0;
	if ( m_iSelectedItem >= 0 && m_iSelectedItem < nItemsCount )
	{
		iFoundMethod = GetUnacknowledgedReason( m_aItems[m_iSelectedItem].pItem.GetInventoryPosition() );
		bAllowDiscard = ( iFoundMethod <= UNACK_ITEM_DROPPED );
	}

	bDiscarded = m_aItems[m_iSelectedItem].bDiscarded;
	bool bCanDiscard = ( !bDiscarded && bAllowDiscard );
	bool bShowOpenLoadoutButton = !bDiscarded;

	wchar_t szNewItemsStr[ 128 ];

	if ( m_aItems.Count() > 1 )
	{
		char szBuff[ 64 ];
		wchar_t szWideBuff2[ 32 ];
		Q_snprintf( szBuff, sizeof( szBuff ), "%i", m_aItems.Count() );
		g_pVGuiLocalize->ConvertANSIToUnicode( szBuff, szWideBuff2, sizeof( szWideBuff2 ) );
		g_pVGuiLocalize->ConstructString( szNewItemsStr, sizeof( szNewItemsStr ), g_pVGuiLocalize->Find( "#NewItemsAcquired" ), 1, szWideBuff2 );
	}
	else
	{
		g_pVGuiLocalize->ConstructString( szNewItemsStr, sizeof( szNewItemsStr ), g_pVGuiLocalize->Find( "#NewItemAcquired" ), 0 );
	}

	if ( !m_bVisible )
	{
		Show();
	}

	bool bReturnToGame = (m_bReturnToGame && engine->IsInGame());

	WITH_SFVALUEARRAY_SLOT_LOCKED( args, 7 )
	{
		m_pScaleformUI->ValueArray_SetElement( args, 0, (int)anim );
		m_pScaleformUI->ValueArray_SetElement( args, 1, bCanDiscard );
		m_pScaleformUI->ValueArray_SetElement( args, 2, bShowOpenLoadoutButton );
		m_pScaleformUI->ValueArray_SetElement( args, 3, szNewItemsStr );
		m_pScaleformUI->ValueArray_SetElement( args, 4, bReturnToGame );
		m_pScaleformUI->ValueArray_SetElement( args, 5, m_iSelectedItem );	
		m_pScaleformUI->ValueArray_SetElement( args, 6, nItemsCount );	
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowSelectedItem", args, 7 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SFHudItemPickupPanel::AddItem( CEconItemView *pItem )
{
	if ( m_pInstance )
	{
		if ( !m_pInstance->m_bVisible )
			StaticShowPanel( true );

		m_pInstance->AddItemInternal( pItem );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SFHudItemPickupPanel::AddItemInternal( CEconItemView *pItem )
{
	int iIdx = m_aItems.AddToTail();
	m_aItems[iIdx].pItem = *pItem;
	m_aItems[iIdx].bDiscarded = false;

	if ( (m_flLastAddItemSound + 2.0f) < gpGlobals->curtime )
	{
		m_flLastAddItemSound = gpGlobals->curtime;
		vgui::surface()->PlaySound( "ui/store_item_purchased.wav" );
	}

	if ( m_bVisible )
	{
		ItemDropAnimType_t anim = ITEM_ANIM_NONE;
		if (iIdx == 0)
		{
			anim = ITEM_ANIM_FIRST;
		}

		UpdateModelPanels( anim );
	}
}

void SFHudItemPickupPanel::ShowItemPickup( XUID xuid, int nIndex )
{
	if ( !m_bVisible )
	{
		Show();
	}
}

void SFHudItemPickupPanel::HideFromScript( SCALEFORM_CALLBACK_ARGS_DECL )
{
	Hide();
}

void SFHudItemPickupPanel::FireGameEvent( IGameEvent *event )
{
	if ( !m_bVisible )
		return;

// 	//if ( !CSGameRules() )
// 	//	return;
// 
// 	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
// 
// 	const char *type = event->GetName();
// 	/////////////////////////////////////////////////////////////////
// 	// Game Event Handling
// 	/////////////////////////////////////////////////////////////////
// 
// 	if ( !V_strcmp( type, "server_spawn") )
// 	{
// 		const char *hostname = event->GetString( "hostname" );
// 		g_pVGuiLocalize->ConvertANSIToUnicode( hostname, m_szHostName, sizeof(m_szHostName) );
// 		// The truncate player name function is just a generic truncate.  Use it to truncate the server name.
// 		TruncatePlayerName( m_szHostName, ARRAYSIZE( m_szHostName ), 80, true );
// 	}

	const char * type = event->GetName();

	if ( Q_strcmp(type, "gameui_hidden") == 0 )
	{
		// If they haven't discarded down to <MAX items, bring us right back up again
		if ( InventoryManager()->CheckForRoomAndForceDiscard() )
		{
			//engine->ClientCmd_Unrestricted( "gameui_activate" );
			Show();
		}
		else
		{
			ShowPanel( false );
		}
	}
}

#endif // INCLUDE_SCALEFORM
