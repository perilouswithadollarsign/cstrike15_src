//====== Copyright (C), Valve Corporation, All rights reserved. =======
//
// Purpose: This file defines all of our over-the-wire net protocols for the
//			Game Coordinator for the item system.  Note that we never use types
//			with undefined length (like int).  Always use an explicit type 
//			(like int32).
//
//=============================================================================

#ifndef ITEM_GCMESSAGES_H
#define ITEM_GCMESSAGES_H
#ifdef _WIN32
#pragma once
#endif

#pragma pack( push, 1 )

// generic zero-length message struct
struct MsgGCEmpty_t
{

};

// k_EMsgGCSetItemPosition 
struct MsgGCSetItemPosition_t
{
	uint64 m_unItemID;
	uint32 m_unNewPosition;
};

// k_EMsgGCCraft
struct MsgGCCraft_t
{
	int16	m_nRecipeDefIndex;
	uint16	m_nItemCount;
	// list of m_nItemCount uint64 item IDs
};

// k_EMsgGCDelete
struct MsgGCDelete_t
{
	uint64 m_unItemID;
	bool m_bRecycle;
};

// k_EMsgGCCraftResponse
struct MsgGCStandardResponse_t
{
	int16			m_nResponseIndex;
	uint32	m_eResponse;
};

// k_EMsgGCVerifyCacheSubscription
struct MsgGCVerifyCacheSubscription_t
{
	uint64 m_ulSteamID;
};

// k_EMsgGCNameItem
struct MsgGCNameItem_t
{
	uint64 m_unToolItemID;		// the Nametag item
	uint64 m_unSubjectItemID;	// the item to be renamed
	bool m_bDescription;
	// Varchar: Item name
};

// k_EMsgGCNameBaseItem
struct MsgGCNameBaseItem_t
{
	uint64 m_unToolItemID;				// the Nametag item
	uint32 m_unBaseItemDefinitionID;	// the base item definition to be renamed
	bool m_bDescription;
	// Varchar: Item name
};

// k_EMsgGCUnlockCrate
struct MsgGCUnlockCrate_t
{
	uint64 m_unToolItemID;		// the crate key
	uint64 m_unSubjectItemID;	// the crate to be decoded
};

// k_EMsgGCPaintItem
struct MsgGCPaintItem_t
{
	uint64 m_unToolItemID;		// the Paint Can item
	uint64 m_unSubjectItemID;	// the item to be painted
	bool m_bUnusualPaint;		// the paint event is unusual paint
};

// k_EMsgGCPaintKitItem
struct MsgGCPaintKitItem_t
{
	uint64 m_unToolItemID;		// the Paint Kit item
	uint64 m_unSubjectItemID;	// the item to be painted
	bool m_bUnusualPaint;		// the paint event is unusual paint
};

// k_EMsgGCPaintKitBaseItem
struct MsgGCPaintKitBaseItem_t
{
	uint64 m_unToolItemID;				// the Paint Kit item
	uint32 m_unBaseItemDefinitionID;	// the base item definition to be painted
};

// k_EMsgGCPaintKitBaseItem
struct MsgGCDev_PaintKitItemDrop_t
{
	uint32 m_unPaintID;					// the Paint Kit ID
	uint32 m_unBaseItemDefinitionID;	// the base item definition to be painted
	int m_nStrangeType;
};

// k_EMsgGCGiftWrapItem
struct MsgGCGiftWrapItem_t
{
	uint64 m_unToolItemID;		// the Gift Wrap item
	uint64 m_unSubjectItemID;	// the item to be wrapped
};

// k_EMsgGCDeliverGift
struct MsgGCDeliverGift_t
{
	uint64 m_unGiftID;
	uint64 m_ulGiverSteamID;
	uint64 m_ulTargetSteamID;
};

// k_EMsgGCUnwrapGiftRequest
struct MsgGCUnwrapGiftRequest_t
{
	uint64 m_unItemID;
};

// k_EMsgGCMOTDRequest
struct MsgGCMOTDRequest_t
{
	RTime32		m_nLastMOTDRequest;	// Time at which the client last asked for MOTDs. GC will send back all MOTDs posted since.
	int16	m_eLanguage;
};

// k_EMsgGCMOTDRequestResponse
struct MsgGCMOTDRequestResponse_t
{
	int16		m_nEntries;
};

// k_EMsgGCCustomizeItemTexture
struct MsgGCCustomizeItemTexture_t
{
	uint64 m_unToolItemID;		// the tool
	uint64 m_unSubjectItemID;	// the item wants the texture
	uint64 m_unImageUGCHandle;	// cloud ID of image file (UGCHandle_t)
};

// k_EMsgGCSetItemStyle
struct MsgGCSetItemStyle_t
{
	uint64 m_unItemID;
	uint8 m_iStyle;
};

// k_EMsgGCUnlockItemStyle
struct MsgGCUnlockItemStyle_t
{
	uint64 m_unItemID;
	uint8 m_iStyle;
};

// k_EMsgGCUnlockItemStyleResponse
struct MsgGCUnlockItemStyleResponse_t
{
	uint64 m_unItemID;
	uint8 m_iStyle;
	uint8 m_iStylePreReq;
	uint8 m_iResult;
};

// k_EMsgGCItemPreviewCheckStatus
struct MsgGCCheckItemPreviewStatus_t
{
	uint32 m_unItemDefIndex;
};

// k_EMsgGCItemPreviewCheckStatusResponse
struct MsgGCItemPreviewCheckStatusResponse_t
{
	uint32 m_unItemDefIndex;
	uint32 m_eResponse;
	RTime32 m_timePreviewTime;
};

// k_EMsgGCItemPreviewRequest
struct MsgGCItemPreviewRequest_t
{
	uint32 m_unItemDefIndex;
};

// k_EMsgGCItemPreviewRequestResponse
struct MsgGCItemPreviewRequestResponse_t
{
	uint32 m_unItemDefIndex;
	uint32	m_eResponse;
};

// k_EMsgGCItemPreviewExpireNotification
struct MsgGCItemPreviewExpireNotification_t
{
	uint32 m_unItemDefIndex;
};

// k_EMsgGCItemPreviewItemBoughtNotification
struct MsgGCItemPreviewItemBoughtNotification_t
{
	uint32 m_unItemDefIndex;
};

//-----------------------------------------------------------------------------

// k_EMsgGCUseItemResponse
enum EGCMsgUseItemResponse
{
	k_EGCMsgUseItemResponse_ItemUsed				 = 0,
	k_EGCMsgUseItemResponse_GiftNoOtherPlayers		 = 1,
	k_EGCMsgUseItemResponse_ServerError				 = 2,
	k_EGCMsgUseItemResponse_MiniGameAlreadyStarted	 = 3,
	k_EGCMsgUseItemResponse_ItemUsed_ItemsGranted	 = 4,
	k_EGCMsgUseItemResponse_DropRateBonusAlreadyGranted = 5,
	k_EGCMsgUseItemResponse_NotInLowPriorityPool	= 6,
	k_EGCMsgUseItemResponse_ForceSizeInt			 = 0x7FFFFFFF
};

// k_EMsgGCUseItemResponse
struct MsgGCUseItemResponse_t
{
	uint32 m_eResponse;
	uint32 m_unDefIndex;
};

// DEPRECATED : k_EMsgGCGiftedItems_DEPRECATED
// struct MsgGCGiftedItems_t
// {
// 	uint64 m_ulGifterSteamID;
// 	bool m_bRandomPerson;
// 	uint8 m_unNumGiftRecipients;
// 	// other data dynamically added:
// 	// steam ids of all the recipients
// };

// k_EMsgGCRemoveItemName
struct MsgGCRemoveItemName_t
{
	uint64 m_unItemID;
	bool m_bDescription;
};

// k_EMsgGCRemoveItemPaint
struct MsgGCRemoveItemPaint_t
{
	uint64 m_unItemID;
};

// MsgGCRemoveItemCustomTexture_t
struct MsgGCRemoveItemCustomTexture_t
{
	uint64 m_unItemID;
};

// MsgGCRemoveItemMakersMark_t
struct MsgGCRemoveItemMakersMark_t
{
	uint64 m_unItemID;
};

// MsgGCRemoveItemUniqueCraftIndex_t
struct MsgGCRemoveItemUniqueCraftIndex_t
{
	uint64 m_unItemID;
};

// k_EMsgGCApplyConsumableEffects
struct MsgGCApplyConsumableEffects_t
{
	uint64 m_ulInitiatorSteamID;
	uint32 m_unItemDefinitionID;
};

//-----------------------------------------------------------------------------
// Trading

// k_EMsgGCTrading_InitiateTradeRequest
struct MsgGCTrading_InitiateTradeRequest_t
{
	uint32 m_unTradeRequestID;
	uint64 m_ulOtherSteamID;
	// @note player A's name as string when sent to party B
};

enum EGCMsgInitiateTradeResponse
{
	k_EGCMsgInitiateTradeResponse_Accepted				  = 0,
	k_EGCMsgInitiateTradeResponse_Declined				  = 1,
	k_EGCMsgInitiateTradeResponse_VAC_Banned_Initiator	  = 2,
	k_EGCMsgInitiateTradeResponse_VAC_Banned_Target		  = 3,
	k_EGCMsgInitiateTradeResponse_Target_Already_Trading  = 4,
	k_EGCMsgInitiateTradeResponse_Disabled				  = 5,
	k_EGCMsgInitiateTradeResponse_NotLoggedIn			  = 6,
	k_EGCMsgInitiateTradeResponse_Cancel				  = 7,
	k_EGCMsgInitiateTradeResponse_TooSoon				  = 8,
	k_EGCMsgInitiateTradeResponse_TooSoonPenalty		  = 9,
	k_EGCMsgInitiateTradeResponse_Trade_Banned_Initiator  = 10,
	k_EGCMsgInitiateTradeResponse_Trade_Banned_Target	  = 11,
	k_EGCMsgInitiateTradeResponse_Free_Account_Initiator_DEPRECATED  = 12,			// free accounts can initiate trades now
	k_EGCMsgInitiateTradeResponse_Shared_Account_Initiator= 13,
	k_EGCMsgInitiateTradeResponse_Service_Unavailable	  = 14,

	k_EGCMsgInitiateTradeResponse_Count,
	k_EGCMsgInitiateTradeResponse_ForceSizeInt			  = 0x7FFFFFFF
};

// k_EMsgGCTrading_InitiateTradeResponse
struct MsgGCTrading_InitiateTradeResponse_t
{
	uint32	m_eResponse;
	uint32 m_unTradeRequestID;
};

// k_EMsgGCTrading_StartSession
struct MsgGCTrading_StartSession_t
{
	uint32 m_unSessionVersion;
	uint64 m_ulSteamIDPartyA;
	uint64 m_ulSteamIDPartyB;
	// @note strings from player names will be added to the message
};

// k_EMsgGCTrading_SetItem
struct MsgGCTrading_SetItem_t
{
	bool m_bShowcase;
	uint64 m_unItemID;
	uint8 m_unPosition;
};

// k_EMsgGCTrading_RemoveItem
struct MsgGCTrading_RemoveItem_t
{
	uint64 m_unItemID;
};

// k_EMsgGCTrading_UpdateTradeInfo
struct MsgGCTrading_UpdateTradeInfo_t
{
	uint32 m_unSessionVersion;
	uint8 m_unNumItemsPartyA;
	uint8 m_unNumItemsPartyB;
	uint8 m_unNumShowcaseItemsPartyA;
	uint8 m_unNumShowcaseItemsPartyB;
	// @note 4 lists of item ids in the order above
};

// k_EMsgGCTrading_SetReadiness
struct MsgGCTrading_SetReadiness_t
{
	uint32 m_unSessionVersion;
	bool m_bReady;
};

// k_EMsgGCTrading_ConfirmOffer
struct MsgGCTrading_ConfirmOffer_t
{
	uint32 m_unConfirmedSessionVersion;
};

// k_EMsgGCTrading_ReadinessResponse
struct MsgGCTrading_ReadinessResponse_t
{
	uint32 m_unSessionVersion;
	bool m_bIsReadyPartyA;
	bool m_bIsReadyPartyB;
	bool m_bConfirmedByPartyA;
	bool m_bConfirmedByPartyB;
};

enum EGCMsgSessionClosed
{
	k_EGCMsgSessionClosed_ItemsTraded	  = 0,
	k_EGCMsgSessionClosed_Canceled		  = 1,
	k_EGCMsgSessionClosed_Error			  = 2,
	k_EGCMsgSessionClosed_DoesNotOwnItems = 3,
	k_EGCMsgSessionClosed_UntradableItems = 4,
	k_EGCMsgSessionClosed_NoItems		  = 5,
	k_EGCMsgSessionClosed_Disabled		  = 6,
	k_EGCMsgSessionClosed_ForceSizeInt	  = 0x7FFFFFFF
};

// k_EMsgGCTrading_SessionClosed
struct MsgGCTrading_SessionClosed_t
{
	uint32 m_eReason;
};

// k_EMsgGCTrading_CancelSession
struct MsgGCTrading_CancelSession_t
{
};

// k_EMsgGCTrading_TradeChatMsg
struct MsgGCTrading_TradeChatMsg_t
{
	// char* variable length chat message, up to 128 characters
};

// k_EMsgGCTrading_TradeTypingChatMsg
struct MsgGCTrading_TradeTypingChatMsg_t
{
	// empty
};

// k_EMsgGCUsedClaimCodeItem
struct MsgGCUsedClaimCodeItem_t
{
	// string of URL
};

//-----------------------------------------------------------------------------
// ServerBrowser messages

enum EGCMsgServerBrowser
{
	k_EGCMsgServerBrowser_FromServerBrowser = 0,
	k_EGCMsgServerBrowser_FromAutoAskDialog = 1,
};

// k_EMsgGCServerBrowser_FavoriteServer
// k_EMsgGCServerBrowser_BlacklistServer
struct MsgGCServerBrowser_Server_t
{
	uint32 m_unIP;
	int m_usPort;
	uint8 m_ubSource;		// 0=serverbrowser, 1=auto-ask dialog
};

//-----------------------------------------------------------------------------
// Public facing loot lists.

// k_EMsgGC_RevolvingLootList
struct MsgGC_RevolvingLootList_t
{
	uint8		m_usListID; // Id of this list.
	// Var Data:
	// Serialized Lootlist KV
};

//-----------------------------------------------------------------------------
// Microtransactions

// k_EMsgGCStorePurchaseFinalize
struct MsgGCStorePurchaseFinalize_t
{
	uint64		m_ulTxnID;					// Transaction ID for the the transaction
};

//k_EMsgGCStorePurchaseFinalizeResponse
struct MsgGCStorePurchaseFinalizeResponse_t
{
	int16			m_eResult;				// Result of the operation
	uint32		m_cItemIDs;					// Number of items contained in the purchase
	// Var Data:
	// If successful, list of m_cItemIDs uint64's
	// that represent the purchased items
};

//k_EMsgGCStorePurchaseCancel
struct MsgGCStorePurchaseCancel_t
{
	uint64		m_ulTxnID;					// Transaction ID for the the transaction
};

//k_EMsgGCStorePurchaseCancelResponse
struct MsgGCStorePurchaseCancelResponse_t
{
	int16	m_eResult;				// Result of the operation
};

//k_EMsgGCStorePurchaseQueryTxn
struct MsgGCStorePurchaseQueryTxn_t
{
	uint64		m_ulTxnID;					// Transaction ID for the the transaction
};

//k_EMsgGCStorePurchaseQueryTxnResponse
struct MsgGCStorePurchaseQueryTxnResponse_t
{
	int16		m_eResult;				// Result of the operation
	int16		m_ePurchaseState;		// State of the queried transaction
	uint32		m_cItemIDs;					// Number of items contained in the purchase
	// Var Data:
	// If the order is final, list of m_cItemIDs uint64's
	// that represent the items purchased by this order
};

// k_EMsgGCLookupAccount 
struct MsgGCLookupAccount_t
{
	uint16	m_uiFindType;

	// Var Data
	// string containing Persona / URL / etc
};

// k_EMsgGCLookupAccountResponse 

// k_EMsgGCLookupAccountName 
struct MsgGCLookupAccountName_t
{
	uint32 m_unAccountID;
};

// k_EMsgGCLookupAccountNameResponse
struct MsgGCLookupAccountNameResponse_t
{
	uint32 m_unAccountID;
	// string containing persona name
};

#pragma pack( pop )

#endif
