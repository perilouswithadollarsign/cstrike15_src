//====== Copyright 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef ACTUAL_ECON_ITEM_CONSTANTS_H		// ECON_ITEM_CONSTANTS_H is used by src/common/econ_item_view.h
#define ACTUAL_ECON_ITEM_CONSTANTS_H
#ifdef _WIN32
#pragma once
#endif

//=============================================================================
// To avoid #include dependency chains, this file should
// contain only constants that do not depend on other 
// header files.
// This file is #included in cbase.h to allow schema compiles
// to use these constants to ensure correlation between 
// code data structures and database entries
//=============================================================================

typedef	uint32		item_price_t;						// this is the type that is used to hold currency values for transactions! don't change this without changing the relevant code/databases/etc.
typedef uint8		item_transaction_quantity_t;

class CLocalizationProvider;

//-----------------------------------------------------------------------------
// Econ Item testing
//-----------------------------------------------------------------------------
#define TESTITEM_DEFINITIONS_BEGIN_AT		32000
#define TESTITEM_DEFINITIONS_COUNT			16

//-----------------------------------------------------------------------------
// Type IDs for economy classes. These are part of the client-GC protocol and
// should not change if it can be helped
// NOTE: if possible fill up the gaps in this enum so that cache objects could
// consume less memory!
//-----------------------------------------------------------------------------
enum EEconTypeID
{
	k_EEconTypeItem							=1,
	k_EEconTypePersonaDataPublic			=2,
	k_EEconTypeGameAccountClient			=7,
	k_EEconTypeGameAccount					=8,
	k_EEconTypeEquipInstance				=31,
	k_EEconTypeDefaultEquippedDefinitionInstance		=42,
	k_EEconTypeDefaultEquippedDefinitionInstanceClient	=43,
	k_EEconTypeCoupon						=45,
	k_EEconTypeQuest						=46,
};

//-----------------------------------------------------------------------------
// Actions for the ItemAudit table
//-----------------------------------------------------------------------------
// WARNING!!! Values stored in DB. Do not renumber!
enum EItemAction
{
	k_EItemActionInvalid					 = -1,
	k_EItemActionGSCreate					 = 0,
	k_EItemActionUnpurchase					 = 1,
	k_EItemActionDelete						 = 2,
	k_EItemActionAwardAchievement			 = 3,
	k_EItemActionBanned						 = 4,
	k_EItemActionQuantityChanged			 = 5,
	k_EItemActionRestored					 = 6,
	k_EItemActionAwardTime					 = 7,
	k_EItemActionManualCreate				 = 8,
	k_EItemActionDrop						 = 9,
	k_EItemActionPickUp						 = 10,
	k_EItemActionCraftDestroy				 = 11,
	k_EItemActionCraftCreate				 = 12,
	k_EItemActionLimitExceeded				 = 13,
	k_EItemActionPurchase					 = 14,
	k_EItemActionNameChanged_Add			 = 15,
	k_EItemActionUnlockCrate_Add			 = 16,
	k_EItemActionPaintItem_Add				 = 17,
	k_EItemActionAutoGrantItem				 = 18,
	k_EItemActionCrossGameAchievement		 = 19,
	k_EItemActionAddItemToSocket_Add		 = 20,
	k_EItemActionAddSocketToItem_Add		 = 21,
	k_EItemActionRemoveSocketItem_Add		 = 22,
	k_EItemActionCustomizeItemTexture_Add	 = 23,
	k_EItemActionItemTraded_Add				 = 24,
	k_EItemActionUseItem					 = 25,
	k_EItemActionAwardGift_Receiver			 = 26,
	k_EItemActionNameChanged_Remove			 = 27,
	k_EItemActionUnlockCrate_Remove			 = 28,
	k_EItemActionPaintItem_Remove			 = 29,
	k_EItemActionAddItemToSocket_Remove		 = 30,
	k_EItemActionAddSocketToItem_Remove		 = 31,
	k_EItemActionRemoveSocketItem_Remove	 = 32,
	k_EItemActionCustomizeItemTexture_Remove = 33,
	k_EItemActionItemTraded_Remove			 = 34,
	k_EItemActionUnpackItemBundle			 = 35,
	k_EItemActionCreateItemFromBundle		 = 36,
	k_EItemActionAwardStorePromotionItem	 = 37,
	k_EItemActionConvertItem				 = 38,
	k_EItemActionEarnedItem					 = 39,
	k_EItemActionAwardGift_Giver			 = 40,
	k_EItemActionRefundedItem				 = 41,
	k_EItemActionAwardThirdPartyPromo		 = 42,
	k_EItemActionRemoveItemName_Remove		 = 43,
	k_EItemActionRemoveItemName_Add			 = 44,
	k_EItemActionRemoveItemPaint_Remove		 = 45,
	k_EItemActionRemoveItemPaint_Add		 = 46,
	k_EItemActionHalloweenDrop				 = 47,
	k_EItemActionSteamWorkshopContributor	 = 48,

	k_EItemActionSupportDelete				 = 50,
	k_EItemActionSupportCreatedByUndo		 = 51,
	k_EItemActionSupportDeletedByUndo		 = 52,
	k_EItemActionSupportQuantityChangedByUndo = 53,
	k_EItemActionSupportRename_Add			 = 54,
	k_EItemActionSupportRename_Remove		 = 55,
	k_EItemActionSupportDescribe_Add		 = 56,
	k_EItemActionSupportDescribe_Remove		 = 57,

	k_EItemActionPeriodicScoreReward_Add	 = 58,
	k_EItemActionPeriodicScoreReward_Remove	 = 59,
	k_EItemActionStrangePartApply_Add		 = 60,
	k_EItemActionStrangePartApply_Remove	 = 61,

	k_EItemActionPennantUpgradeApply_Add	 = 62,
	k_EItemActionPennantUpgradeApply_Remove	 = 63,

	k_EItemActionPaintKitItem_Add			 = 64,
	k_EItemActionPaintKitItem_Remove		 = 65,

	k_EItemActionUpgradeChallengeCoin_Add	 = 66,
	k_EItemActionUpgradeChallengeCoin_Remove = 67,

	k_EItemActionStickerApply_Add			 = 68,
	k_EItemActionStickerApply_Remove		 = 69,
	k_EItemActionStickerRemove_Add			 = 70,
	k_EItemActionStickerRemove_Remove		 = 71,

	k_EItemActionGrantQuestReward_Add		 = 72,
	k_EItemActionGrantQuestReward_Remove	 = 73,
	k_EItemActionApplyQuestProgress_Add		 = 74,
	k_EItemActionApplyQuestProgress_Remove	 = 75,

	k_EItemActionPurchaseUnlockCrate_Add	 = 76,
	k_EItemActionPurchaseUnlockCrate_Remove	 = 77,

	k_EItemActionSwapStatTrak_Add			 = 78,
	k_EItemActionSwapStatTrak_Remove		 = 79,

	k_EItemActionPurchaseConsumedAsNonItem	 = 80,

	k_EItemActionDev_ClientLootListRoll		 = 90,

	k_EItemActionGiftWrap_Add				 = 100,
	k_EItemActionGiftWrap_Remove			 = 101,
	k_EItemActionGiftDelivery_Add			 = 102,
	k_EItemActionGiftDelivery_Remove		 = 103,
	k_EItemActionGiftUnwrap_Add				 = 104,
	k_EItemActionGiftUnwrap_Remove			 = 105,
	k_EItemActionPackageItem				 = 106,
	k_EItemActionPackageItem_Revoked		 = 107,
	k_EItemActionHandleMapToken			 	 = 108,
	k_EItemActionCafeOrSchoolItem_Remove	 = 109,
	k_EItemActionVACBanned_Remove			 = 110,
	k_EItemActionUpgradeThirdPartyPromo		 = 111,
	k_EItemActionExpired					 = 112,
	k_EItemActionTradeRollback_Add			 = 113,
	k_EItemActionTradeRollback_Remove		 = 114,
	k_EItemActionCDKeyGrant					 = 115,
	k_EItemActionCDKeyRevoke				 = 116,
	k_EItemActionWeddingRing_Add			 = 117,
	k_EItemActionWeddingRing_Remove			 = 118,
	k_EItemActionWeddingRing_AddPartner		 = 119,
	k_EItemActionEconSetUnowned				 = 120,
	k_EItemActionEconSetOwned				 = 121,

	k_EItemActionRemoveItemCraftIndex_Remove = 150,
	k_EItemActionRemoveItemCraftIndex_Add	 = 151,
	k_EItemActionRemoveItemMakersMark_Remove = 152,				// early versions of this will be in the database as 150
	k_EItemActionRemoveItemMakersMark_Add	 = 153,				// early versions of this will be in the database as 151 because I am a terrible person

	k_EItemActionCollectItem_CollectedItem			 = 154,
	k_EItemActionCollectItem_UpdateCollection		 = 155,
	k_EItemActionCollectItem_RemoveCollection		 = 156,
	k_EItemActionCollectItem_RedeemCollectionReward	 = 157,

	k_EItemActionPreviewItem_BeginPreviewPeriod  = 158,
	k_EItemActionPreviewItem_EndPreviewPeriodExpired	= 159,
	k_EItemActionPreviewItem_EndPreviewPeriodItemBought	= 160,

	k_EItemActionRecycling					= 161,
	k_EItemActionAwardXP					 = 162,
	k_EItemActionTournamentDrop					= 163,
	k_EItemDiretideReward = 164,


	k_EItemDiretideEggAddOnModify				= 165,
	k_EItemDiretideEssenceRemove				= 166,
	k_EItemDiretideEggRemoveOnModify			= 167,

	k_EItemActionUnpackItems					= 168,

	k_EItemActionUnlockStyle					= 169,
	k_EItemActionUnlockStyle_Remove				= 170,

	k_EItemActionTutorialDrop					= 171,

	k_EItemActionAutographAdd					= 172,
	k_EItemActionAutographRemove				= 173,

	k_EItemActionApplyIntermediateStickerWear	= 174,
	k_EItemActionTemplateItemGrant				= 175,

	k_EItemActionCoinCampaignProgress			= 176,
	
	k_EItemActionMarket_Add						= 177,
	k_EItemActionMarket_Remove					= 178,
	// Let's be consistent with the underscores please.
	// k_EItemActionYourNewAction, not k_EItemAction_YourNewAction
	// Yes, it matters. See PchLocalizedNameFromEItemAction for why.
};
extern const char		*PchNameFromEItemAction( EItemAction eAction );
extern const char		*PchNameFromEItemActionUnsafe( EItemAction eAction );

extern bool BIsActionCreative( EItemAction );
extern bool BIsActionDestructive( EItemAction );

enum EItemActionMissingBehavior { kEItemAction_FriendlyNameLookup_ReturnNULLIfMissing, kEItemAction_FriendlyNameLookup_ReturnDummyStringIfMissing };
extern const char		*PchFriendlyNameFromEItemAction( EItemAction eAction, EItemActionMissingBehavior eMissingBehavior );
extern const char		*PchLocalizedNameFromEItemAction( EItemAction eAction, CLocalizationProvider &localizationProvider );


//-----------------------------------------------------------------------------
// Purpose: Used to pass audit actions to asset servers for SetUnowned and 
//			SetOwned methods. 
//-----------------------------------------------------------------------------
enum EEconOwnershipAction
{
	k_EEconOwnershipAction_Invalid = 0,

	k_EEconOwnershipAction_TradeBase = 100,
	k_EEconOwnershipAction_TradeCommit = 101,		// precommit and docommit step of a trade. Reference is trade ID
	k_EEconOwnershipAction_TradeRollback = 102,		// cancelcommit and rollbackcommit step of a trade. Reference is trade ID
};

// old
enum eEconItemFlags_Deprecated
{
	kDeprecated_EconItemFlag_AchievementGrantedItem	= 1 << 0,
	kDeprecated_EconItemFlag_CannotTrade				= 1 << 1,
	kDeprecated_EconItemFlag_Purchased				= 1 << 2,
	kDeprecated_EconItemFlag_CannotBeUsedInCrafting	= 1 << 3,
	kDeprecated_EconItemFlag_Promotion				= 1 << 4,
};

//-----------------------------------------------------------------------------
// Flags for CEconItem
//-----------------------------------------------------------------------------
// WARNING!!! Values stored in DB.  DO NOT CHANGE EXISTING VALUES.  Add values to the end.
enum eEconItemFlags
{
	kEconItemFlag_CannotTrade				= 1 << 0,
	kEconItemFlag_CannotBeUsedInCrafting	= 1 << 1,
	kEconItemFlag_CanBeTradedByFreeAccounts	= 1 << 2,
	kEconItemFlag_NonEconomy				= 1 << 3,		// used for items that are meant to not interact in the economy -- these can't be traded, gift-wrapped, crafted, etc.

#ifdef CLIENT_DLL
#ifdef TF_CLIENT_DLL
	kEconItemFlagClient_ForceBlueTeam		= 1 << 5,
#endif // TF_CLIENT_DLL
	kEconItemFlagClient_StoreItem			= 1 << 6,
	kEconItemFlagClient_Preview				= 1 << 7,		// only set on the client; means "this item is being previewed"
#endif // CLIENT_DLL

	// combination of the above flags used in code
	kEconItemFlags_CheckFlags_CannotTrade			= kEconItemFlag_CannotTrade,
	kEconItemFlags_CheckFlags_NotUsableInCrafting	= kEconItemFlag_CannotBeUsedInCrafting,

	kEconItemFlags_CheckFlags_AllGCFlags			= kEconItemFlags_CheckFlags_CannotTrade | kEconItemFlags_CheckFlags_NotUsableInCrafting,
};

//-----------------------------------------------------------------------------
// Origin for an item for CEconItem
//-----------------------------------------------------------------------------
// WARNING!!! Values stored in DB.  DO NOT CHANGE EXISTING VALUES.  Add values to the end.
enum eEconItemOrigin
{
	kEconItemOrigin_Invalid = -1,				// should never be stored in the DB! used to indicate "invalid" for in-memory objects only

	kEconItemOrigin_Drop = 0,
	kEconItemOrigin_Achievement,
	kEconItemOrigin_Purchased,
	kEconItemOrigin_Traded,
	kEconItemOrigin_Crafted,
	kEconItemOrigin_StorePromotion,
	kEconItemOrigin_Gifted,
	kEconItemOrigin_SupportGranted,
	kEconItemOrigin_FoundInCrate,
	kEconItemOrigin_Earned,
	kEconItemOrigin_ThirdPartyPromotion,
	kEconItemOrigin_GiftWrapped,
	kEconItemOrigin_HalloweenDrop,
	kEconItemOrigin_PackageItem,
	kEconItemOrigin_Foreign,
	kEconItemOrigin_CDKey,
	kEconItemOrigin_CollectionReward,
	kEconItemOrigin_PreviewItem,
	kEconItemOrigin_SteamWorkshopContribution,
	kEconItemOrigin_PeriodicScoreReward,
	kEconItemOrigin_Recycling,
	kEconItemOrigin_TournamentDrop,
	kEconItemOrigin_StockItem,
	KEconItemOrigin_QuestReward,
	KEconItemOrigin_LevelUpReward,

	kEconItemOrigin_Max,
};
extern const char		*PchNameFromeEconItemOrigin( eEconItemOrigin eOrigin );

// The Steam backend representation of a unique item index
typedef uint64	itemid_t;
typedef uint16	item_definition_index_t;
typedef uint16	attrib_definition_index_t;
typedef uint32	attrib_value_t;

// Misc typedefs for clarity.
typedef uint32	equip_region_mask_t;
typedef uint8	style_index_t;

const uint64 INVALID_ITEM_ID							= (itemid_t)-1;
const item_definition_index_t INVALID_ITEM_DEF_INDEX	= ((item_definition_index_t)-1);
const attrib_definition_index_t INVALID_ATTRIB_DEF_INDEX= ((attrib_definition_index_t)-1);

//-----------------------------------------------------------------------------

// Standard/default backpack size
#define DEFAULT_NUM_BACKPACK_SLOTS_PER_PAGE				100
#define DEFAULT_NUM_BACKPACK_SLOTS						1000
#define DEFAULT_NUM_BACKPACK_SLOTS_FREE_TRIAL_ACCOUNT	100
#define MAX_NUM_BACKPACK_SLOTS							2000

// Current item level range
#define MIN_ITEM_LEVEL					0
#define MAX_ITEM_LEVEL					100

// Maximum number of attributes allowed on a single item
#define MAX_ATTRIBUTES_PER_ITEM					32
// The maximum length of a single attribute's description
//	divide by locchar_t, so we can ensure 192 bytes, whether that's 128 wchars on client or 256 utf-8 bytes on gc
#define MAX_ATTRIBUTE_DESCRIPTION_LENGTH		( 256 / sizeof( locchar_t ) )

// The maximum length of an item's name
#define MAX_ITEM_NAME_LENGTH					128
#define MAX_ITEM_DESC_LENGTH					256
#define MAX_ITEM_LONG_DESC_LENGTH				2048
// The maximum length of an item description. (Extra +1 line is for the base item type line)
// #define MAX_ITEM_DESCRIPTION_LENGTH				((MAX_ATTRIBUTES_PER_ITEM+1) * MAX_ATTRIBUTE_DESCRIPTION_LENGTH)

// For custom user-naming of econ items.
#define MAX_ITEM_CUSTOM_NAME_LENGTH				40
#define MAX_ITEM_CUSTOM_NAME_DATABASE_SIZE		((4 * MAX_ITEM_CUSTOM_NAME_LENGTH) + 1)	// Ensures we can store MAX_ITEM_CUSTOM_NAME_LENGTH
																						// characters worth of obscure unicode characters in UTF8
#define MAX_ITEM_CUSTOM_DESC_LENGTH				80
#define MAX_ITEM_CUSTOM_DESC_DATABASE_SIZE		((4 * MAX_ITEM_CUSTOM_DESC_LENGTH) + 1)

// max length in the DB for claim codes
#define MAX_CLAIM_CODE_LENGTH					128	

// The number of items to work on in a job before checking if a yield is necessary
#define MAX_ITEMS_BEFORE_YIELD					50

// TF team-color paints (moved from econ_item_view.h)
#define RGB_INT_RED  12073019
#define RGB_INT_BLUE 5801378

// Custom textures
const int k_nCustomImageSize = 256;
const int k_nMaxCustomImageFileSize = k_nCustomImageSize*k_nCustomImageSize*4 + 4*1024; // Is this about right?

//-----------------------------------------------------------------------------
// Purpose: Quality types of items
//-----------------------------------------------------------------------------
typedef int32 entityquality_t;
enum EEconItemQuality
{
	AE_UNDEFINED = -1,

	AE_NORMAL = 0,
	AE_GENUINE = 1,
	AE_VINTAGE,	
	AE_UNUSUAL,
	AE_UNIQUE,
	AE_COMMUNITY,
	AE_DEVELOPER,
	AE_SELFMADE,
	AE_CUSTOMIZED,
	AE_STRANGE,
	AE_COMPLETED,
	AE_HAUNTED,
	AE_TOURNAMENT,
	AE_FAVORED,

	AE_MAX_TYPES,
};


//-----------------------------------------------------------------------------
// Purpose: colors used in the display of attributes
//-----------------------------------------------------------------------------
enum attrib_colors_t
{
	ATTRIB_COL_LEVEL = 0,
	ATTRIB_COL_NEUTRAL,
	ATTRIB_COL_POSITIVE,
	ATTRIB_COL_NEGATIVE,
	ATTRIB_COL_ITEMSET_NAME,
	ATTRIB_COL_ITEMSET_EQUIPPED,
	ATTRIB_COL_ITEMSET_MISSING,
	ATTRIB_COL_BUNDLE_ITEM,
	ATTRIB_COL_LIMITED_USE,
	ATTRIB_COL_ITEM_FLAGS,
	ATTRIB_COL_RARITY_DEFAULT,
	ATTRIB_COL_RARITY_COMMON,
	ATTRIB_COL_RARITY_UNCOMMON,
	ATTRIB_COL_RARITY_RARE,
	ATTRIB_COL_RARITY_MYTHICAL,
	ATTRIB_COL_RARITY_LEGENDARY,
	ATTRIB_COL_RARITY_ANCIENT,
	ATTRIB_COL_RARITY_IMMORTAL,
	ATTRIB_COL_RARITY_ARCANA,
	ATTRIB_COL_STRANGE,
	ATTRIB_COL_UNUSUAL,

	NUM_ATTRIB_COLORS,
};


#define AE_USE_SCRIPT_VALUE			9999		// Can't be -1, due to unsigned ints used on the backend

const char *EconQuality_GetQualityString( EEconItemQuality eQuality );
const char *EconQuality_GetColorString( EEconItemQuality eQuality );
const char *EconQuality_GetLocalizationString( EEconItemQuality eQuality );

// Sort order for rarities
int EconQuality_GetRarityScore( EEconItemQuality eQuality );

int EconRarity_CombinedItemAndPaintRarity( int nItemDefRarity, int nPaintRarity );

int EconWear_ToIntCategory( float flWear );
float EconWear_FromIntCategory( int nWearCategory );

int EconMinutes_ToRoundMinHrsCategory( float flMinutes );
float EconMinutes_FromRoundMinHrsCategory( int nRoundMinHrsCategory );

int EconTintID_ToRoundCategory( uint32 unTintID );
uint32 EconTintID_FromRoundCategory( int nRoundCategory );

extern attrib_colors_t GetAttribColorIndexForName( const char* pszName );

extern const char *GetColorNameForAttribColor( attrib_colors_t unAttribColor );
extern const char *GetHexColorForAttribColor( attrib_colors_t unAttribColor );

// Utility function that'll get you an item quality from a string
entityquality_t GetItemQualityFromString( const char *sQuality );

enum recipecategories_t
{
	RECIPE_CATEGORY_CRAFTINGITEMS = 0,
	RECIPE_CATEGORY_COMMONITEMS,
	RECIPE_CATEGORY_RAREITEMS,
	RECIPE_CATEGORY_SPECIAL,

	NUM_RECIPE_CATEGORIES
};
extern const char *g_szRecipeCategoryStrings[NUM_RECIPE_CATEGORIES];

//-----------------------------------------------------------------------------
// Kill eater support.
//-----------------------------------------------------------------------------
enum kill_eater_event_t
{
	kKillEaterEvent_PlayerKill = 0,		
	kKillEaterEvent_MVPs,

// 	kKillEaterEvent_KillAssists,			// not yet implemented
// 	kKillEaterEvent_Wins,
// 	kKillEaterEvent_Treants_Created,
// 	kKillEaterEvent_Omnislash_Jumps,
// 	kKillEaterEvent_Blade_Fury_Damage,
// 	kKillEaterEvent_Blade_Dance_Crits,
// 	kKillEaterEvent_Wards_Placed,
// 	kKillEaterEvent_Killing_Sprees,			// not yet implemented
// 	kKillEaterEvent_Trees_Created,
// 	kKillEaterEvent_Carries_Killed,			// not yet implemented
// 	kKillEaterEvent_First_Blood,
// 	kKillEaterEvent_Godlike,
// 	kKillEaterEvent_Gold_Earned,
// 	kKillEaterEvent_Gold_Spent,
// 	kKillEaterEvent_Towers_Destroyed,
// 	kKillEaterEventType_Bounty_TrackedKills,
// 	kKillEaterEventType_Bounty_TrackedInvisKills,
// 	kKillEaterEventType_Bounty_InvisJinadaHits,
// 	kKillEaterEventType_Bounty_TrackGold,
// 	kKillEaterEventType_Tide_GoodRavages,
// 	kKillEaterEventType_Windrunner_DoubleShackles,
// 	kKillEaterEventType_Windrunner_GoodPowershots,
// 	kKillEaterEventType_Invoker_SunstrikeKills,
// 	kKillEaterEventType_Couriers_Purchased,
// 	kKillEaterEventType_Wards_Purchased,
// 	kKillEaterEventType_Tide_EnemiesGushed,
// 	kKillEaterEventType_Axe_Culls,
// 	kKillEaterEventType_Axe_BattleHungerKills,
// 	kKillEaterEventType_Axe_LowHealthKills,
// 	kKillEaterEventType_Axe_CalledDamageDone,
// 	kKillEaterEventType_Axe_CalledDamageTaken,
// 	kKillEaterEventType_Invoker_TornadoKills,
// 	kKillEaterEventType_Games_Viewed,
// 	kKillEaterEventType_Sven_DoubleStuns,
// 	kKillEaterEventType_Sven_WarcryAssists,
// 	kKillEaterEventType_Sven_CleaveDoubleKills,
// 	kKillEaterEventType_Sven_TeleportInterrupts,
// 	kKillEaterEventType_Faceless_MultiChrono,
// 	kKillEaterEventType_Faceless_ChronoKills,
// 	kKillEaterEventType_Ursa_MultiShocks,
// 	kKillEaterEventType_RoshanKills,


	// NEW ENTRIES MUST BE ADDED AT THE BOTTOM
};

// Make sure to update this when you add a new type.
bool EventRequiresVictim( kill_eater_event_t ke_event );

// Ugh -- these are shared between the GC and the client. Maybe #define is slightly better than
// magic string literals?
#define KILL_EATER_RANK_LEVEL_BLOCK_NAME		"KillEaterRank"

int GetKillEaterAttrPairCount();
const class CEconItemAttributeDefinition *GetKillEaterAttrPair_Score( int i );
const class CEconItemAttributeDefinition *GetKillEaterAttrPair_Type( int i );
bool GetKillEaterAttrPair_IsUserCustomizable( int i );

//===============================================================================================================
// POSITION HANDLING
//===============================================================================================================
// TF Inventory Position cracking

// REALLY OLD FORMAT (??):
//		We store a bag index in the highbits of the inventory position.
//		The lowbit stores the position of the item within the bag.
//
// LESS OLD FORMAT (up through July, 2011):
//		If Bit 31 is 0: 
//			Bits 1-16 are the backpack position.
//			Bits 17-26 are a bool for whether the item is equipped in the matching class.
//		Otherwise, if Bit 31 is 1:
//			Item hasn't been acknowledged by the player yet.
//			Bits 1-16 are the method by the player found the item (see unacknowledged_item_inventory_positions_t)
//		Bit 32 is 1, to note the new format.
//
// CURRENT FORMAT:
//		If Bit 31 is 0: 
//			Bits 1-16 are the backpack position.
//		Otherwise, if Bit 31 is 1:
//			Item hasn't been acknowledged by the player yet.
//			Bits 1-16 are the method by the player found the item (see unacknowledged_item_inventory_positions_t)
//		Equipped state is stored elsewhere.
//		This is the only format that should exist on clients.


enum unacknowledged_item_inventory_positions_t
{
	UNACK_ITEM_DROPPED = 1,
	UNACK_ITEM_CRAFTED,
	UNACK_ITEM_TRADED,
	UNACK_ITEM_PURCHASED,
	UNACK_ITEM_FOUND_IN_CRATE,
	UNACK_ITEM_GIFTED,
	UNACK_ITEM_SUPPORT,
	UNACK_ITEM_PROMOTION,
	UNACK_ITEM_EARNED,
	UNACK_ITEM_REFUNDED,
	UNACK_ITEM_GIFT_WRAPPED,
	UNACK_ITEM_FOREIGN,
	UNACK_ITEM_COLLECTION_REWARD,
	UNACK_ITEM_PREVIEW_ITEM,
	UNACK_ITEM_PREVIEW_ITEM_PURCHASED,
	UNACK_ITEM_PERIODIC_SCORE_REWARD,
	UNACK_ITEM_RECYCLING,
	UNACK_ITEM_TOURNAMENT_DROP,
	UNACK_ITEM_QUEST_REWARD,
	UNACK_ITEM_LEVEL_UP_REWARD,
#ifdef ENABLE_STORE_RENTAL_BACKEND
	UNACK_ITEM_RENTAL_PURCHASE,
#endif

	UNACK_NUM_METHODS,
};

extern const char *g_pszItemPickupMethodStrings[UNACK_NUM_METHODS];
extern const char *g_pszItemPickupMethodStringsUnloc[UNACK_NUM_METHODS];
extern const char *g_pszItemFoundMethodStrings[UNACK_NUM_METHODS];

enum
{
	kGCItemSort_NoSort				= 0,			// this won't do anything, but can be used as a safe "header" value

	kGCItemSort_SortByName			= 1,
	kGCItemSort_SortByDefIndex		= 2,
	kGCItemSort_SortByRarity		= 3,
	kGCItemSort_SortByType			= 4,
	kGCItemSort_SortByQuality		= 5,

	kGCItemSort_GameSpecificBase	= 100,
};

// FIXME: these should be moved... somewhere; where?
enum
{
	kTFGCItemSort_SortByClass		= kGCItemSort_GameSpecificBase + 1,
	kTFGCItemSort_SortBySlot		= kGCItemSort_GameSpecificBase + 2,
	kDOTAGCItemSort_SortByHero		= kGCItemSort_GameSpecificBase + 3,
};

enum
{
	kBackendPosition_Unacked	= 1 << 30,
	kBackendPosition_Reserved	= 1 << 31,

	kBackendPositionMask_Position		= 0x0000ffff,
	kBackendPositionMask_FormatFlags	= (kBackendPosition_Unacked | kBackendPosition_Reserved),
};

inline void SetBackpackPosition( uint32 *pPosition, uint32 iPackPosition )
{
	(*pPosition) = iPackPosition;

	// Remove the unack'd flag
	(*pPosition) &= ~kBackendPosition_Unacked;	
}

inline bool IsUnacknowledged( uint32 iBackendPosition )
{
	// For backwards compatibility, we consider position 0 as unacknowledged too
	return ( iBackendPosition == 0 || (iBackendPosition & kBackendPosition_Unacked) != 0 );
}

inline int ExtractBackpackPositionFromBackend( uint32 iBackendPosition )
{
	if ( IsUnacknowledged( iBackendPosition) )
		return 0;

	return iBackendPosition & kBackendPositionMask_Position;
}

inline unacknowledged_item_inventory_positions_t GetUnacknowledgedReason( uint32 iBackendPosition )
{
	return (unacknowledged_item_inventory_positions_t)( iBackendPosition &= ~kBackendPositionMask_FormatFlags );
}

inline uint32 GetUnacknowledgedPositionFor( unacknowledged_item_inventory_positions_t iMethod )
{
	return ( iMethod | kBackendPosition_Unacked | kBackendPosition_Reserved );
}

//-----------------------------------------------------------------------------
// Item Preview event IDs for logging.
//-----------------------------------------------------------------------------
enum EEconItemPreviewEventIDs
{
	k_EEconItemPreview_Start				=1,
	k_EEconItemPreview_Expired				=2,
	k_EEconItemPreview_ItemPurchased		=3,
};

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
enum EHoliday
{
	kHoliday_None							= 0,		// must stay at zero for backwards compatability
	kHoliday_Halloween,
	kHoliday_Christmas,
	kHoliday_FullMoon,
	kHoliday_HalloweenOrFullMoon,
	kHoliday_Easter,

	kHolidayCount,
};

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
enum ECartItemType
{
	kCartItem_Purchase,				// a normal lifetime purchase (needs to stay as entry 0!)
	kCartItem_TryOutUpgrade,		// an upgrade from "try-it-out"
	kCartItem_Rental_1Day,
	kCartItem_Rental_3Day,
	kCartItem_Rental_7Day,
};

void GetHexStringFromPaintColor( float flPaint, char *pszRGB );
inline bool IsRentalCartItemType( ECartItemType eCartType )
{
	return eCartType == kCartItem_Rental_1Day
		|| eCartType == kCartItem_Rental_3Day
		|| eCartType == kCartItem_Rental_7Day;
}


#endif // ACTUAL_ECON_ITEM_CONSTANTS_H
