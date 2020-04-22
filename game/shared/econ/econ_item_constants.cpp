//====== Copyright (c), Valve Corporation, All rights reserved. =======
//
// Purpose: Holds constants for the econ item system
//
//=============================================================================

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *g_szQualityStrings[] =
{
	"normal",
	"genuine",
	"vintage",
	"unusual",
	"unique",
	"community",
	"developer",
	"selfmade",
	"customized",
	"strange",
	"completed",
	"haunted",
	"tournament",
	"favored"
};

COMPILE_TIME_ASSERT( ARRAYSIZE( g_szQualityStrings ) == AE_MAX_TYPES );

const char *EconQuality_GetQualityString( EEconItemQuality eQuality )
{
	// This is a runtime check and not an assert because we could theoretically bounce the GC with new
	// qualities while the client is running.
	if ( eQuality >= 0 && eQuality < AE_MAX_TYPES )
		return g_szQualityStrings[ eQuality ];

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *g_szQualityColorStrings[] =
{
	"QualityColorNormal",
	"QualityColorGenuine",
	"QualityColorVintage",
	"QualityColorUnusual",
	"QualityColorUnique",
	"QualityColorCommunity",
	"QualityColorDeveloper",
	"QualityColorSelfMade",
	"QualityColorSelfMadeCustomized",
	"QualityColorStrange",
	"QualityColorCompleted",
	"QualityColorHaunted",				// AE_HAUNTED,			// same as AE_UNUSUAL
	"QualityColorTournament",
	"QualityColorFavored",
};

COMPILE_TIME_ASSERT( ARRAYSIZE( g_szQualityColorStrings ) == AE_MAX_TYPES );

const char *EconQuality_GetColorString( EEconItemQuality eQuality )
{
	if ( eQuality >= 0 && eQuality < AE_MAX_TYPES )
		return g_szQualityColorStrings[ eQuality ];

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *g_szQualityLocalizationStrings[] =
{
	"#normal",
	"#genuine",
	"#vintage",
	"#unusual",
	"#unique",
	"#community",
	"#developer",
	"#selfmade",
	"#customized",
	"#strange",
	"#completed",
	"#haunted",
	"#tournament",
	"#favored"
};

COMPILE_TIME_ASSERT( ARRAYSIZE( g_szQualityLocalizationStrings ) == AE_MAX_TYPES );

const char *EconQuality_GetLocalizationString( EEconItemQuality eQuality )
{
	if ( eQuality >= 0 && eQuality < AE_MAX_TYPES )
		return g_szQualityLocalizationStrings[ eQuality ];

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Sort order for rarities
//-----------------------------------------------------------------------------
int g_nRarityScores[] =
{
	11,		// AE_NORMAL = 0,
	9,		// AE_GENUINE = 1,
	5,		// AE_VINTAGE
	7,		// AE_UNUSUAL,
	0,		// AE_UNIQUE,
	2,		// AE_COMMUNITY,
	3,		// AE_DEVELOPER,
	-1,		// AE_SELFMADE,
	4,		// AE_CUSTOMIZED,
	10,		// AE_STRANGE,
	12,		// AE_COMPLETED,
	1,		// AE_HAUNTED,			// same as AE_UNIQUE
	13,		// AE_TOURNAMENT,
	14,		// AE_FAVORED
};

COMPILE_TIME_ASSERT( ARRAYSIZE( g_nRarityScores ) == AE_MAX_TYPES );

int EconQuality_GetRarityScore( EEconItemQuality eQuality )
{
	if ( eQuality >= 0 && eQuality < AE_MAX_TYPES )
		return g_nRarityScores[ eQuality ];

	return 0;
}

int EconRarity_CombinedItemAndPaintRarity( int nItemDefRarity, int nPaintRarity )
{
	return clamp( ( nItemDefRarity + nPaintRarity ) - 1, 0, ( nPaintRarity == 7 ) ? 7 : 6 );
}

int EconWear_ToIntCategory( float flWear )
{
	if ( flWear < 0.07f )
	{
		return 0;
	}
	else if ( flWear < 0.15f )
	{
		return 1;
	}
	else if ( flWear < 0.38f )
	{
		return 2;
	}
	else if ( flWear < 0.45f )
	{
		return 3;
	}
	else
	{
		return 4;
	}
}

float EconWear_FromIntCategory( int nWearCategory )
{
	if ( nWearCategory <= 0 )
		return 0.05f;
	else if ( nWearCategory == 1 )
		return 0.14f;
	else if ( nWearCategory == 2 )
		return 0.37f;
	else if ( nWearCategory == 3 )
		return 0.44f;
	else
		return 0.5f;
}

int EconMinutes_ToRoundMinHrsCategory( float flMinutes )
{
	int32 iMinutes = int32( flMinutes );
	if ( iMinutes <= 120 )
		return iMinutes;

	iMinutes /= 60;
	iMinutes *= 60;
	++ iMinutes;	// this way we round up all minutes above 2 hours to a whole number of hours + 1 minute
	return iMinutes;
}

float EconMinutes_FromRoundMinHrsCategory( int nRoundMinHrsCategory )
{
	return nRoundMinHrsCategory;
}

int EconTintID_ToRoundCategory( uint32 unTintID )
{
	return unTintID & 0xFF;
}

uint32 EconTintID_FromRoundCategory( int nRoundCategory )
{
	return nRoundCategory;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSchemaColorDefHandle g_AttribColorDefs[] =
{
	CSchemaColorDefHandle( "desc_level" ),				// ATTRIB_COL_LEVEL
	CSchemaColorDefHandle( "desc_attrib_neutral" ),		// ATTRIB_COL_NEUTRAL
	CSchemaColorDefHandle( "desc_attrib_positive" ),	// ATTRIB_COL_POSITIVE
	CSchemaColorDefHandle( "desc_attrib_negative" ),	// ATTRIB_COL_NEGATIVE
	CSchemaColorDefHandle( "desc_itemset_name" ),		// ATTRIB_COL_ITEMSET_NAME
	CSchemaColorDefHandle( "desc_itemset_equipped" ),	// ATTRIB_COL_ITEMSET_EQUIPPED
	CSchemaColorDefHandle( "desc_itemset_missing" ),	// ATTRIB_COL_ITEMSET_MISSING
	CSchemaColorDefHandle( "desc_bundle" ),				// ATTRIB_COL_BUNDLE_ITEM
	CSchemaColorDefHandle( "desc_limited_use" ),		// ATTRIB_COL_LIMITED_USE
	CSchemaColorDefHandle( "desc_flags" ),				// ATTRIB_COL_ITEM_FLAGS
	CSchemaColorDefHandle( "desc_default" ),			// ATTRIB_COL_RARITY_DEFAULT
	CSchemaColorDefHandle( "desc_common" ),				// ATTRIB_COL_RARITY_COMMON
	CSchemaColorDefHandle( "desc_uncommon" ),			// ATTRIB_COL_RARITY_UNCOMMON
	CSchemaColorDefHandle( "desc_rare" ),				// ATTRIB_COL_RARITY_RARE
	CSchemaColorDefHandle( "desc_mythical" ),			// ATTRIB_COL_RARITY_MYTHICAL
	CSchemaColorDefHandle( "desc_legendary" ),			// ATTRIB_COL_RARITY_LEGENDARY
	CSchemaColorDefHandle( "desc_ancient" ),			// ATTRIB_COL_RARITY_ANCIENT
	CSchemaColorDefHandle( "desc_immortal" ),			// ATTRIB_COL_RARITY_IMMORTAL
	CSchemaColorDefHandle( "desc_arcana" ),				// ATTRIB_COL_RARITY_ARCANA
	CSchemaColorDefHandle( "desc_strange" ),			// ATTRIB_COL_STRANGE
	CSchemaColorDefHandle( "desc_unusual" ),			// ATTRIB_COL_UNUSUAL
};

COMPILE_TIME_ASSERT( ARRAYSIZE( g_AttribColorDefs ) == NUM_ATTRIB_COLORS );

attrib_colors_t GetAttribColorIndexForName( const char* pszName )
{
	for ( int i=0; i<NUM_ATTRIB_COLORS; ++i )
	{
		if ( !Q_strcmp( g_AttribColorDefs[i].GetName(), pszName ) )
			return (attrib_colors_t) i;
	}

	return (attrib_colors_t) 0;
}

const char *GetColorNameForAttribColor( attrib_colors_t unAttribColor )
{
	Assert( unAttribColor >= 0 );
	Assert( unAttribColor < NUM_ATTRIB_COLORS );

	return g_AttribColorDefs[unAttribColor]
		 ? g_AttribColorDefs[unAttribColor]->GetColorName()
		 : "ItemAttribNeutral";
}

const char *GetHexColorForAttribColor( attrib_colors_t unAttribColor )
{
	Assert( unAttribColor >= 0 );
	Assert( unAttribColor < NUM_ATTRIB_COLORS );

	return g_AttribColorDefs[unAttribColor]
	     ? g_AttribColorDefs[unAttribColor]->GetHexColor()
		 : "#ebe2ca";
}

entityquality_t GetItemQualityFromString( const char *sQuality )
{
	for ( int i = 0; i < AE_MAX_TYPES; i++ )
	{
		if ( !Q_strnicmp( sQuality, g_szQualityStrings[i], 16 ) )
			return (entityquality_t)i;
	}

	return AE_NORMAL;
}

const char *g_szRecipeCategoryStrings[] =
{
	"crafting",		// RECIPE_CATEGORY_CRAFTINGITEMS = 0,
	"commonitem",	// RECIPE_CATEGORY_COMMONITEMS,
	"rareitem",		// RECIPE_CATEGORY_RAREITEMS,
	"special",		// RECIPE_CATEGORY_SPECIAL,
};

COMPILE_TIME_ASSERT( ARRAYSIZE( g_szRecipeCategoryStrings ) == NUM_RECIPE_CATEGORIES );

//-----------------------------------------------------------------------------
// Item acquisition.
//-----------------------------------------------------------------------------
// Strings shown to the local player in the pickup dialog
const char *g_pszItemPickupMethodStrings[] = 
{
	"#NewItemMethod_Dropped",			// UNACK_ITEM_DROPPED = 1,
	"#NewItemMethod_Crafted",			// UNACK_ITEM_CRAFTED,
	"#NewItemMethod_Traded",			// UNACK_ITEM_TRADED,
	"#NewItemMethod_Purchased",			// UNACK_ITEM_PURCHASED,
	"#NewItemMethod_FoundInCrate",		// UNACK_ITEM_FOUND_IN_CRATE,
	"#NewItemMethod_Gifted",			// UNACK_ITEM_GIFTED,
	"#NewItemMethod_Support",			// UNACK_ITEM_SUPPORT,
	"#NewItemMethod_Promotion",			// UNACK_ITEM_PROMOTION,
	"#NewItemMethod_Earned",			// UNACK_ITEM_EARNED,
	"#NewItemMethod_Refunded",			// UNACK_ITEM_REFUNDED,
	"#NewItemMethod_GiftWrapped",		// UNACK_ITEM_GIFT_WRAPPED,
	"#NewItemMethod_Foreign",			// UNACK_ITEM_FOREIGN,
	"#NewItemMethod_CollectionReward",	// UNACK_ITEM_COLLECTION_REWARD
	"#NewItemMethod_PreviewItem",		// UNACK_ITEM_PREVIEW_ITEM
	"#NewItemMethod_PreviewItemPurchased", // UNACK_ITEM_PREVIEW_ITEM_PURCHASED
	"#NewItemMethod_PeriodicScoreReward",// UNACK_ITEM_PERIODIC_SCORE_REWARD
	"#NewItemMethod_Recycling",			// UNACK_ITEM_RECYCLING
	"#NewItemMethod_TournamentDrop",	// UNACK_ITEM_TOURNAMENT_DROP
	"#NewItemMethod_QuestReward",		// UNACK_ITEM_QUEST_REWARD
	"#NewItemMethod_LevelUpReward",		// UNACK_ITEM_LEVEL_UP_REWARD
#ifdef ENABLE_STORE_RENTAL_BACKEND
	"#NewItemMethod_RentalPurchase",	// UNACK_ITEM_RENTAL_PURCHASE
#endif
};

COMPILE_TIME_ASSERT( ARRAYSIZE( g_pszItemPickupMethodStrings ) == UNACK_NUM_METHODS );

const char *g_pszItemPickupMethodStringsUnloc[] = 
{
	"dropped",			// UNACK_ITEM_DROPPED = 1,
	"crafted",			// UNACK_ITEM_CRAFTED,
	"traded",			// UNACK_ITEM_TRADED,
	"purchased",		// UNACK_ITEM_PURCHASED,
	"found_in_crate",	// UNACK_ITEM_FOUND_IN_CRATE,
	"gifted",			// UNACK_ITEM_GIFTED,
	"support",			// UNACK_ITEM_SUPPORT,
	"promotion",		// UNACK_ITEM_PROMOTION,
	"earned",			// UNACK_ITEM_EARNED,
	"refunded",			// UNACK_ITEM_REFUNDED,
	"gift_wrapped",		// UNACK_ITEM_GIFT_WRAPPED
	"foreign",			// UNACK_ITEM_FOREIGN
	"collection_reward",// UNACK_ITEM_COLLECTION_REWARD
	"preview_item",		// UNACK_ITEM_PREVIEW_ITEM
	"preview_item_purchased", // UNACK_ITEM_PREVIEW_ITEM_PURCHASED
	"periodic_score_reward", // UNACK_ITEM_PERIODIC_SCORE_REWARD
	"recycling",		// UNACK_ITEM_RECYCLING
	"tournament_drop",	// UNACK_ITEM_TOURNAMENT_DROP
	"quest_reward",		// UNACK_ITEM_QUEST_REWARD
	"level_up_reward",	// UNACK_ITEM_LEVEL_UP_REWARD
#ifdef ENABLE_STORE_RENTAL_BACKEND
	"rental_purchase",	// UNACK_ITEM_RENTAL_PURCHASE
#endif
};

COMPILE_TIME_ASSERT( ARRAYSIZE( g_pszItemPickupMethodStringsUnloc ) == UNACK_NUM_METHODS );

// Strings shown to other players in the chat dialog
const char *g_pszItemFoundMethodStrings[] = 
{
	"#Item_Found",				// UNACK_ITEM_DROPPED = 1,
	"#Item_Crafted",			// UNACK_ITEM_CRAFTED,
	"#Item_Traded",				// UNACK_ITEM_TRADED,
	NULL,						// UNACK_ITEM_PURCHASED,
	"#Item_FoundInCrate",		// UNACK_ITEM_FOUND_IN_CRATE,
	"#Item_Gifted",				// UNACK_ITEM_GIFTED,
	NULL,						// UNACK_ITEM_SUPPORT,
	NULL,						// UNACK_ITEM_PROMOTION
	"#Item_Earned",				// UNACK_ITEM_EARNED
	"#Item_Refunded",			// UNACK_ITEM_REFUNDED
	"#Item_GiftWrapped",		// UNACK_ITEM_GIFT_WRAPPED
	"#Item_Foreign",			// UNACK_ITEM_FOREIGN
	"#Item_CollectionReward",	// UNACK_ITEM_COLLECTION_REWARD
	"#Item_PreviewItem",		// UNACK_ITEM_PREVIEW_ITEM
	"#Item_PreviewItemPurchased",// UNACK_ITEM_PREVIEW_ITEM_PURCHASED
	"#Item_PeriodicScoreReward",// UNACK_ITEM_PERIODIC_SCORE_REWARD
	"#Item_Recycling",			// UNACK_ITEM_RECYCLING
	"#Item_TournamentDrop",		// UNACK_ITEM_TOURNAMENT_DROP
	"#Item_QuestReward",		// UNACK_ITEM_QUEST_REWARD
	"#Item_LevelUpReward",		// UNACK_ITEM_LEVEL_UP_REWARD
#ifdef ENABLE_STORE_RENTAL_BACKEND
	NULL,						// UNACK_ITEM_RENTAL_PURCHASE
#endif
};


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
struct kill_eater_attr_pair_t
{
	kill_eater_attr_pair_t( const char *pScoreAttrName, const char *pTypeAttrName, bool bIsUserCustomizable )
		: m_attrScore( pScoreAttrName )
		, m_attrType( pTypeAttrName )
		, m_bIsUserCustomizable( bIsUserCustomizable )
	{
		//
	}

	CSchemaAttributeDefHandle m_attrScore;
	CSchemaAttributeDefHandle m_attrType;
	bool m_bIsUserCustomizable;
};

kill_eater_attr_pair_t g_KillEaterAttr[] =
{
	kill_eater_attr_pair_t( "kill eater",			"kill eater score type",		false ),
// 	kill_eater_attr_pair_t( "kill eater 2",			"kill eater score type 2",		false ),
// 
// 	// assumption: all of the user-customizable attributes will follow all of the schema-specified attributes
// 	kill_eater_attr_pair_t( "kill eater user 1",	"kill eater user score type 1",	true ),
// 	kill_eater_attr_pair_t( "kill eater user 2",	"kill eater user score type 2", true ),
// 	kill_eater_attr_pair_t( "kill eater user 3",	"kill eater user score type 3", true ),
};

int GetKillEaterAttrPairCount()
{
	return ARRAYSIZE( g_KillEaterAttr );
}

const CEconItemAttributeDefinition *GetKillEaterAttrPair_Score( int i )
{
	Assert( i >= 0 );
	Assert( i < GetKillEaterAttrPairCount() );

	return g_KillEaterAttr[i].m_attrScore;
}

const CEconItemAttributeDefinition *GetKillEaterAttrPair_Type( int i )
{
	Assert( i >= 0 );
	Assert( i < GetKillEaterAttrPairCount() );

	return g_KillEaterAttr[i].m_attrType;
}

bool GetKillEaterAttrPair_IsUserCustomizable( int i )
{
	Assert( i >= 0 );
	Assert( i < GetKillEaterAttrPairCount() );

	return g_KillEaterAttr[i].m_bIsUserCustomizable;
}

bool EventRequiresVictim( kill_eater_event_t ke_event )
{
	switch( ke_event )
	{
		case kKillEaterEvent_MVPs:
			return false;
	default:
		break;
	}

	return true;
}

// -----------------------------------------------------------------------------
// Purpose: convert the paint color float into rgb string.  Char buffer should be at least 7 char long
// -----------------------------------------------------------------------------
void GetHexStringFromPaintColor( float flPaint, char *pszRGB )
{
	uint32 unValue = (uint32)flPaint;
	Color rgb;
	rgb.SetRawColor( unValue );

	// it appears to be in bgr instead of rgb, so reverse the values when we read them out here
	byte b = rgb.r();
	byte g = rgb.g();
	byte r = rgb.b();
	V_binarytohex( &r, 1, pszRGB, 3 );
	V_binarytohex( &g, 1, pszRGB + 2, 3 );
	V_binarytohex( &b, 1, pszRGB + 4, 3 );
}
