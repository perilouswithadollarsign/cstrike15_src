//====== Copyright ?, Valve Corporation, All rights reserved. =======
//
// Purpose: EconItemSchema: Defines a schema for econ items
//
//=============================================================================

#include "cbase.h"
#include "econ_item_schema.h"
#include "tier1/fmtstr.h"
#include "tier2/tier2.h"
#include "filesystem.h"
#include "schemainitutils.h"
#include "cstrike15_gcconstants.h"

#include <google/protobuf/text_format.h>

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	#include "econ_item_system.h"
	#include "econ_item.h"
	#include "activitylist.h"

	#if defined(TF_CLIENT_DLL) || defined(TF_DLL)
		#include "tf_gcmessages.h"
	#endif
#endif

#if defined(CSTRIKE_CLIENT_DLL) || defined(CSTRIKE_DLL)
#include "cstrike15_gcmessages.pb.h"
#endif

#include "gametypes.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace GCSDK;

#if defined(GAME_DLL)
void PrecacheParticleFileAndSystems( const char *pParticleSystemFile );
#endif

#if defined(CLIENT_DLL)
bool CWebResource::s_Initialized = false;
#endif

CEconItemSchema & GEconItemSchema()
{
#if defined( EXTERNALTESTS_DLL )
	static CEconItemSchema g_econItemSchema;
	return g_econItemSchema;
#else
	return *ItemSystem()->GetItemSchema();
#endif
}

const char * g_arrQuestVars[ k_EQuestVar_Last ] = { "" };
	/** Removed for partner depot **/


static void HelperValidateLocalizationStringToken( char const *pszToken )
{
#ifdef CLIENT_DLL
	if ( pszToken && *pszToken == '#' )
		AssertMsg1( g_pVGuiLocalize->Find( pszToken ), "Schema is referencing a non-existant token: %s", pszToken );
#endif
}

const char *g_szDropTypeStrings[] =
{
	"",		 // Blank and none mean the same thing: stay attached to the body.
	"none",
	"drop",	 // The item drops off the body.
	"break", // Not implemented, but an example of a type that could be added.
};

#if defined(CLIENT_DLL) || defined(GAME_DLL)
// Used to convert strings to ints for wearable animation types
const char *g_WearableAnimTypeStrings[NUM_WAP_TYPES] =
{
	"on_spawn",			// WAP_ON_SPAWN,
	"start_building",	// WAP_START_BUILDING,
	"stop_building",	// WAP_STOP_BUILDING,
};
#endif

const char *g_AttributeDescriptionFormats[] =
{
	"value_is_percentage",				// ATTDESCFORM_VALUE_IS_PERCENTAGE,
	"value_is_inverted_percentage",		// ATTDESCFORM_VALUE_IS_INVERTED_PERCENTAGE
	"value_is_additive",				// ATTDESCFORM_VALUE_IS_ADDITIVE
	"value_is_additive_percentage",		// ATTDESCFORM_VALUE_IS_ADDITIVE_PERCENTAGE
	"value_is_or",						// ATTDESCFORM_VALUE_IS_OR
	"value_is_date",					// ATTDESCFORM_VALUE_IS_DATE
	"value_is_account_id",				// ATTDESCFORM_VALUE_IS_ACCOUNT_ID
	"value_is_particle_index",			// ATTDESCFORM_VALUE_IS_PARTICLE_INDEX -> Could change to "string index"
	"value_is_item_def",				// ATTDESCFORM_VALUE_IS_ITEM_DEF
	"value_is_color",					// ATTDESCFORM_VALUE_IS_COLOR
	"value_is_game_time",				// ATTDESCFORM_VALUE_IS_GAME_TIME
	"value_is_mins_as_hours",			// ATTDESCFORM_VALUE_IS_MINS_AS_HOURS
	"value_is_replace",					// ATTDESCFORM_VALUE_IS_REPLACE
};

const char *g_EffectTypes[NUM_EFFECT_TYPES] =
{
	"neutral",		// ATTRIB_EFFECT_NEUTRAL = 0,
	"positive",		// ATTRIB_EFFECT_POSITIVE,
	"negative",		// ATTRIB_EFFECT_NEGATIVE,
};

const char *g_szParticleAttachTypes[] =
{
	"absorigin",			//	PATTACH_ABSORIGIN = 0,			// Create at absorigin, but don't follow
	"absorigin_follow",		//		PATTACH_ABSORIGIN_FOLLOW,		// Create at absorigin, and update to follow the entity
	"customorigin",			//		PATTACH_CUSTOMORIGIN,			// Create at a custom origin, but don't follow
	"customorigin_follow",	//		PATTACH_CUSTOMORIGIN_FOLLOW,	// Create at a custom origin, follow relative position to specified entity
	"point",				//		PATTACH_POINT,					// Create on attachment point, but don't follow
	"point_follow",			//		PATTACH_POINT_FOLLOW,			// Create on attachment point, and update to follow the entity
	"eyes_follow",			//		PATTACH_EYES_FOLLOW,			// Create on eyes of the attached entity, and update to follow the entity
	"overhead_follow",		//		PATTACH_OVERHEAD_FOLLOW,		// Create at the top of the entity's bbox
	"worldorigin",			//		PATTACH_WORLDORIGIN,			// Used for control points that don't attach to an entity
	"rootbone_follow",		//		PATTACH_ROOTBONE_FOLLOW,		// Create at the root bone of the entity, and update to follow
	"point_follow_substepped", //	PATTACH_POINT_FOLLOW_SUBSTEPPED,// Like point_follow with interpolation
	"renderorigin_follow",	// PATTACH_RENDERORIGIN_FOLLOW			// Create at the renderorigin of the entity, and update to follow
};

const char *g_szParticleAttachToEnt[] =
{
	"self",					//		ATTPART_TO_SELF,
	"parent",				//		ATTPART_TO_PARENT,
};

//-----------------------------------------------------------------------------
// Purpose: Set the capabilities bitfield based on whether the entry is true/false.
//-----------------------------------------------------------------------------
const char *g_Capabilities[] =
{
	"paintable",				// ITEM_CAP_PAINTABLE
	"nameable",					// ITEM_CAP_NAMEABLE
	"decodable",				// ITEM_CAP_DECODABLE
	"can_delete",				// ITEM_CAP_CAN_DELETE
	"can_customize_texture",	// ITEM_CAP_CAN_CUSTOMIZE_TEXTURE
	"usable",					// ITEM_CAP_USABLE
	"usable_gc",				// ITEM_CAP_USABLE_GC
	"can_gift_wrap",			// ITEM_CAP_CAN_GIFT_WRAP
	"usable_out_of_game",		// ITEM_CAP_USABLE_OUT_OF_GAME
	"can_collect",				// ITEM_CAP_CAN_COLLECT
	"can_craft_count",			// ITEM_CAP_CAN_CRAFT_COUNT
	"can_craft_mark",			// ITEM_CAP_CAN_CRAFT_MARK
	"paintable_team_colors",	// ITEM_CAP_PAINTABLE_TEAM_COLORS
	"can_be_restored",			// ITEM_CAP_CAN_BE_RESTORED
	"strange_parts",			// ITEM_CAP_CAN_USE_STRANGE_PARTS
	"paintable_unusual",		// ITEM_CAP_PAINTABLE_UNUSUAL
	"can_increment",			// ITEM_CAP_CAN_INCREMENT
	"uses_essence",				// ITEM_CAP_USES_ESSENCE
	"autograph",				// ITEM_CAP_AUTOGRAPH
	"recipe",					// ITEM_CAP_RECIPE
	"can_sticker",				// ITEM_CAP_CAN_STICKER
	"can_stattrack_swap",		// ITEM_CAP_STATTRACK_SWAP
};

const char* g_AssetModifiers[] =
{
	"activity",
	"announcer",
	"announcer_preview",
	"hud_skin",
	"ability_name",
	"sound",
	"speech",
	"particle",
	"particle_snapshot",
	"particle_control_point",
	"entity_model",
	"entity_scale",
	"icon_replacement",
	"ability_icon_replacement",
	"courier",
	"courier_flying",
	"hero_model_change"
};


#define RETURN_ATTRIBUTE_STRING( attrib_name, default_string ) \
	static CSchemaAttributeDefHandle pAttribString( attrib_name ); \
	const char *pchResultAttribString = default_string; \
	FindAttribute_UnsafeBitwiseCast< CAttribute_String >( this, pAttribString, &pchResultAttribString ); \
	return pchResultAttribString;

#define RETURN_ATTRIBUTE_STRING_F( func_name, attrib_name, default_string ) \
	const char *func_name( void ) const { RETURN_ATTRIBUTE_STRING( attrib_name, default_string ) }

EAssetModifier GetAssetModifierType( const char* pszType )
{
	for ( int i=0; i<AM_MAX; ++i )
	{
		if ( Q_strcmp( pszType, g_AssetModifiers[i] ) == 0 )
			return (EAssetModifier) i;
	}

	return AM_Invalid;
}

CUniformRandomStream CEconItemSchema::m_RandomStream;

static void ParseCapability( item_capabilities_t &capsBitfield, KeyValues* pEntry )
{
	COMPILE_TIME_ASSERT( ARRAYSIZE(g_Capabilities) == NUM_ITEM_CAPS );
	int idx = StringFieldToInt(  pEntry->GetName(), g_Capabilities, ARRAYSIZE(g_Capabilities) );
	if ( idx < 0 )
	{
		return;
	}
	int bit = 1 << idx;
	if ( pEntry->GetBool() )
	{
		(int&)capsBitfield |= bit;
	}
	else
	{
		(int&)capsBitfield &= ~bit;
	}
}

//-----------------------------------------------------------------------------
// Purpose: interpret tint colors as integers for the values
//-----------------------------------------------------------------------------
static bool Helper_ExtractIntegerFromValueStringEntry( const char *szValue, int *pnValue )
{
	if ( V_isdigit( *szValue ) )
	{
		*pnValue = V_atoi( szValue );
		return true;
	}
	else if ( char const *szTint = StringAfterPrefix( szValue, "tint_") )
	{
		if ( !V_stricmp( szTint, "min" ) )
		{
			*pnValue = 1;
			return true;
		}
		if ( !V_stricmp( szTint, "max" ) )
		{
			*pnValue = GEconItemSchema().GetGraffitiTintMaxValidDefID();
			return true;
		}
		if ( const CEconGraffitiTintDefinition *pDef = GEconItemSchema().GetGraffitiTintDefinitionByName( szTint ) )
		{
			*pnValue = pDef->GetID();
			return true;
		}
		return false;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: interprets a string such as "1-3,5-20,45,46,47" and adds the 
//			individual uint32s into the specified vector.
//-----------------------------------------------------------------------------
static bool Helper_ExtractIntegersFromValuesString( const char * pszValues, CCopyableUtlVector< uint32 > &vecValues )
{
	bool bResult = true;
	CUtlVector< char* > vecValueStrings;
	V_SplitString( pszValues, ",", vecValueStrings );

	if ( vecValueStrings.Count() < 1 )
		bResult = false;


	FOR_EACH_VEC( vecValueStrings, i )
	{
		if ( V_strstr( vecValueStrings[ i ], "-" ) )
		{
			CUtlVector< char* > vecRangeStrings;
			V_SplitString( vecValueStrings[ i ], "-", vecRangeStrings );

			if ( vecRangeStrings.Count() == 2 )
			{
				int iRangeMin = 0, iRangeMax = 0;
				if ( !Helper_ExtractIntegerFromValueStringEntry( vecRangeStrings[ 0 ], &iRangeMin ) ||
					!Helper_ExtractIntegerFromValueStringEntry( vecRangeStrings[ 1 ], &iRangeMax ) ||
					( iRangeMin >= iRangeMax ) )
				{
					bResult = false;
					break;
				}

				for ( int j = iRangeMin; j <= iRangeMax; j++ )
				{
					vecValues.AddToTail( j );
				}
			}
			else
			{
				bResult = false;
				break;
			}

			vecRangeStrings.PurgeAndDeleteElements();
		}
		else
		{
			int iValue = 0;
			if ( !Helper_ExtractIntegerFromValueStringEntry( vecValueStrings[ i ], &iValue ) )
			{
				bResult = false;
				break;
			}
			vecValues.AddToTail( iValue );
		}

	}

	vecValueStrings.PurgeAndDeleteElements();

	return bResult;

}


#if defined ( CSTRIKE15 )
const uint32 g_unNumWearBuckets = 3;
uint64 Helper_GetAlternateIconKeyForWeaponPaintWearItem( item_definition_index_t nDefIdx, uint32 nPaintId, uint32 nWear )
{
	return ( nDefIdx << 16 ) + ( nPaintId << 2 ) + nWear;

}
uint64 Helper_GetAlternateIconKeyForTintedStickerItem( uint32 nStickerKitID, uint32 unTintID )
{
	unTintID = CombinedTintIDGetHSVID( unTintID );
	return ( uint64( 1 ) << 32 ) | ( ( nStickerKitID & 0xFFFFFF ) << 8 ) | ( unTintID & 0xFF );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEconItemQualityDefinition::CEconItemQualityDefinition( void )
:	m_nValue( INT_MAX )
,	m_bCanSupportSet( false )
,	m_unWeight( 0 )
,	m_bExplicitMatchesOnly( false )
{
}


//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CEconItemQualityDefinition::CEconItemQualityDefinition( const CEconItemQualityDefinition &that )
{
	(*this) = that;
}


//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconItemQualityDefinition &CEconItemQualityDefinition::operator=( const CEconItemQualityDefinition &rhs )
{
	m_nValue = rhs.m_nValue; 
	m_strName =	rhs.m_strName; 
	m_bCanSupportSet = rhs.m_bCanSupportSet;
	m_unWeight = rhs.m_unWeight;
	m_bExplicitMatchesOnly = rhs.m_bExplicitMatchesOnly;

	return *this;
}


//-----------------------------------------------------------------------------
// Purpose:	Initialize the quality definition
// Input:	pKVQuality - The KeyValues representation of the quality
//			schema - The overall item schema for this attribute
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemQualityDefinition::BInitFromKV( KeyValues *pKVQuality, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{

	m_nValue = pKVQuality->GetInt( "value", -1 );
	m_strName = pKVQuality->GetName();	
	m_bCanSupportSet = pKVQuality->GetBool( "canSupportSet" );
	m_strHexColor = pKVQuality->GetString( "hexColor" );
	m_unWeight = pKVQuality->GetInt( "weight", 0 );

	// Check for required fields
	SCHEMA_INIT_CHECK( 
		NULL != pKVQuality->FindKey( "value" ), 
		CFmtStr( "Quality definition %s: Missing required field \"value\"", pKVQuality->GetName() ) );

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	return SCHEMA_INIT_SUCCESS();
#endif

	// Check for data consistency
	SCHEMA_INIT_CHECK( 
		0 != Q_stricmp( GetName(), "any" ), 
		CFmtStr( "Quality definition any: The quality name \"any\" is a reserved keyword and cannot be used." ) );

	SCHEMA_INIT_CHECK( 
		m_nValue != k_unItemQuality_Any, 
		CFmtStr( "Quality definition %s: Invalid value (%d). It is reserved for Any", GetName(), k_unItemQuality_Any ) );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEconItemRarityDefinition::CEconItemRarityDefinition( void )
	:	m_nValue( INT_MAX )
{
}


//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CEconItemRarityDefinition::CEconItemRarityDefinition( const CEconItemRarityDefinition &that )
{
	(*this) = that;
}


//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconItemRarityDefinition &CEconItemRarityDefinition::operator=( const CEconItemRarityDefinition &rhs )
{
	m_nValue = rhs.m_nValue; 
	m_strName =	rhs.m_strName; 
	m_strLocKey = rhs.m_strLocKey;
	m_strWepLocKey = rhs.m_strWepLocKey;
	m_strLootList = rhs.m_strLootList;
	m_strRecycleLootList = rhs.m_strRecycleLootList;
	m_strDropSound = rhs.m_strDropSound;
	m_strNextRarity = rhs.m_strNextRarity;
	m_iWhiteCount = rhs.m_iWhiteCount;
	m_iBlackCount = rhs.m_iBlackCount;
	m_flWeight = rhs.m_flWeight;

	return *this;
}


//-----------------------------------------------------------------------------
// Purpose:	Initialize the rarity definition
//-----------------------------------------------------------------------------
bool CEconItemRarityDefinition::BInitFromKV( KeyValues *pKVRarity, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_nValue = pKVRarity->GetInt( "value", -1 );
	m_strName = pKVRarity->GetName();
	m_strLocKey = pKVRarity->GetString( "loc_key" );
	m_strWepLocKey = pKVRarity->GetString( "loc_key_weapon" );

	m_iAttribColor = GetAttribColorIndexForName( pKVRarity->GetString( "color" ) );
	m_strLootList = pKVRarity->GetString( "loot_list" ); // Not required.
	m_strRecycleLootList = pKVRarity->GetString( "recycle_list" ); // Not required.
	m_strDropSound = pKVRarity->GetString( "drop_sound" );
	m_strNextRarity = pKVRarity->GetString( "next_rarity" ); // Not required.
	m_flWeight = pKVRarity->GetFloat( "weight", 0 );

	// Check for required fields
	SCHEMA_INIT_CHECK( 
		NULL != pKVRarity->FindKey( "value" ), 
		CFmtStr( "Rarity definition %s: Missing required field \"value\"", pKVRarity->GetName() ) );

	SCHEMA_INIT_CHECK( 
		NULL != pKVRarity->FindKey( "loc_key" ), 
		CFmtStr( "Rarity definition %s: Missing required field \"loc_key\"", pKVRarity->GetName() ) );

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	return SCHEMA_INIT_SUCCESS();
#endif

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconColorDefinition::BInitFromKV( KeyValues *pKVColor, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_strName		= pKVColor->GetName();
	m_strColorName	= pKVColor->GetString( "color_name" );
	m_strHexColor	= pKVColor->GetString( "hex_color" );

	SCHEMA_INIT_CHECK(
		!m_strColorName.IsEmpty(),
		CFmtStr( "Quality definition %s: missing \"color_name\"", GetName() ) );

	SCHEMA_INIT_CHECK(
		!m_strHexColor.IsEmpty(),
		CFmtStr( "Quality definition %s: missing \"hex_color\"", GetName() ) );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconGraffitiTintDefinition::BInitFromKV( KeyValues *pKVColor, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_nID = pKVColor->GetInt( "id" );
	m_strColorName = pKVColor->GetName();
	m_strHexColor = pKVColor->GetString( "hex_color" );

	SCHEMA_INIT_CHECK(
		!m_strColorName.IsEmpty(),
		CFmtStr( "Graffiti tint definition #%d %s: missing name", GetID(), GetColorName() ) );

	SCHEMA_INIT_CHECK(
		!m_strHexColor.IsEmpty() && ( m_strHexColor.Get()[0] == '#' ),
		CFmtStr( "Graffiti tint #%d %s: missing or invalid \"hex_color\"", GetID(), GetColorName() ) );

	m_unHexColorRGB = ( !m_strHexColor.IsEmpty() ) ? V_atoi( CFmtStr( "0x%s", m_strHexColor.Get() + 1 ) ) : 0;

	return !m_strColorName.IsEmpty() && !m_strHexColor.IsEmpty() && ( m_strHexColor.Get()[0] == '#' );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconMusicDefinition::BInitFromKV( KeyValues *pKVMusicDef, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	nID							= Q_atoi( pKVMusicDef->GetName() );	
	m_strName					= pKVMusicDef->GetString( "name" );
	m_strNameLocToken			= pKVMusicDef->GetString( "loc_name" );
	m_strLocDescription			= pKVMusicDef->GetString( "loc_description" );
	m_strPedestalDisplayModel	= pKVMusicDef->GetString( "pedestal_display_model" );
	m_strInventoryImage			= pKVMusicDef->GetString( "image_inventory" );

	SCHEMA_INIT_CHECK(
		( nID > 0 ),
		CFmtStr( "Music definition %s: name must be a positive integer", GetName() ) );

	SCHEMA_INIT_CHECK(
		!m_strName.IsEmpty(),
		CFmtStr( "Music definition %s: missing \"name\"", GetName() ) );

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconQuestDefinition::BInitFromKV( KeyValues *pKVQuestDef, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	//TODO - do we care to parse quest definitions on GC at all or only on game server and client?
	nID = Q_atoi( pKVQuestDef->GetName() );
	SCHEMA_INIT_CHECK(
		( nID > 0 ),
		CFmtStr( "Quest definition %s: name must be a positive integer", GetName() ) );

	m_strName = pKVQuestDef->GetString( "name" );
	SCHEMA_INIT_CHECK(
		!m_strName.IsEmpty(),
		CFmtStr( "Quest definition %s: missing \"name\"", GetName() ) );

	m_strGameMode = pKVQuestDef->GetString( "gamemode" );
	m_strMapGroup = pKVQuestDef->GetString( "mapgroup" );
	m_strMap = pKVQuestDef->GetString( "map" );

	#ifndef GC_DLL
	static bool bLoadBannedWords = ( !!CommandLine()->FindParm( "-usebanlist" ) ) || (
		!!CommandLine()->FindParm( "-perfectworld" )
		);
	if ( bLoadBannedWords && !V_stricmp( m_strMap.Get(), "ar_monastery" ) )
		m_strMap = "ar_shoots";
	#endif

	m_bIsAnEvent = pKVQuestDef->GetBool( "is_an_event", false );

	// Campaign quests require client-exposed points remaining.
	// load non-GC only points if it exists.
	// Otherwise look for "gc_generation_settings/points"

	// quest events are always 1 point.
	if ( m_bIsAnEvent )
	{
		m_vecQuestPoints.AddToTail( 1 );
	}
	else
	{
		const char* pszPointsRemaining = pKVQuestDef->GetString( "points" );

		m_vecQuestPoints.Purge();

		if ( pszPointsRemaining && pszPointsRemaining[ 0 ] )
		{
			SCHEMA_INIT_CHECK(
				Helper_ExtractIntegersFromValuesString( pszPointsRemaining, m_vecQuestPoints ),
				CFmtStr( "Quest definition %s specifies a malformed values range: %s", GetName(), pszPointsRemaining ) );
		}
		else
		{
			// if points aren't specified then default to '1'
			m_vecQuestPoints.AddToTail( 1 );
		}
	}


	// campaigns use quest-defined rewards rather than attributes rolled later.
	m_strRewardLootList = pKVQuestDef->GetString( "quest_reward" );

	m_nDifficulty = pKVQuestDef->GetInt( "difficulty", 1 );

	m_nOperationalPoints = pKVQuestDef->GetInt( "operational_points", 0 );

	if ( !m_bIsAnEvent )
	{
		m_nXPReward = pKVQuestDef->GetInt( "xp_reward", ( m_nDifficulty + 1 ) * 100 );	// campaign quest: 1 = 200, 2 = 300, 3 = 400
	}
	else
	{
		m_nXPReward = pKVQuestDef->GetInt( "gc_generation_settings/xp_reward", 1 );	// default quest event xp reward to 1 per action
	}

	m_nTargetTeam = pKVQuestDef->GetInt( "target_team", 0 );

	m_strExpr = pKVQuestDef->GetString( "expression" );

	// BONUS
	m_strBonusExpr = pKVQuestDef->GetString( "expr_bonus" );

	// set bonus percent only if a bonus expression exists
	if ( !m_strBonusExpr.IsEmpty() )
	{
		m_nXPBonusPercent = pKVQuestDef->GetInt( "xp_bonus_percent", 100 );	// default bonus is 100%
	}
	else
	{
		m_nXPBonusPercent = 0;
	}

#ifdef CLIENT_DLL // strings

	m_strNameLocToken = pKVQuestDef->GetString( "loc_name" );
	if ( !m_strNameLocToken.IsEmpty() )
		HelperValidateLocalizationStringToken( m_strNameLocToken );

	m_strShortNameLocToken = pKVQuestDef->GetString( "loc_shortname" );
	if ( !m_strShortNameLocToken.IsEmpty() )
		HelperValidateLocalizationStringToken( m_strShortNameLocToken );

	m_strDescriptionLocToken = pKVQuestDef->GetString( "loc_description" );
	if ( !m_strDescriptionLocToken.IsEmpty() )
		HelperValidateLocalizationStringToken( m_strDescriptionLocToken );

	m_strHudDescriptionLocToken = pKVQuestDef->GetString( "loc_huddescription" );
	if ( !m_strHudDescriptionLocToken.IsEmpty() )
		HelperValidateLocalizationStringToken( m_strHudDescriptionLocToken );

	// Localizers have request the ability to explicitly write each quest's description because the
	// programatic string construction in place is not flexible enough to accommodate various grammars.
	//
	// Description will defer to this string, if it exists. It should not be used in CSGO_English.

	if ( g_pVGuiLocalize->Find( CFmtStr( "Override_Quest_Description_%s", pKVQuestDef->GetName( ) ) ) )
		m_strDescriptionLocToken = CFmtStr( "Override_Quest_Description_%s", pKVQuestDef->GetName( ) );

	m_strLocBonus = pKVQuestDef->GetString( "loc_bonus" );
	if ( !m_strLocBonus.IsEmpty() )
		HelperValidateLocalizationStringToken( m_strLocBonus );

	m_strIcon = pKVQuestDef->GetString( "quest_icon" );


	// Quests used to use explicitly define "string_tokens" in their schema definitions.
	// Now the string tokens kv is populated by interpreting the expression
	// but we may still want to add tokens manually in the future.

	m_kvStringTokens = pKVQuestDef->FindKey( "string_tokens" );

	if ( !m_kvStringTokens )
		m_kvStringTokens = new KeyValues( "string_tokens" );

	KeyValues * pkvExpressionTokens = new KeyValues( "ExpressionTokens" );
	KeyValues::AutoDelete autodelete_pKVExpressionTokens( pkvExpressionTokens );

	TokenizeQuestExpression( m_strExpr, pkvExpressionTokens );

	Assert( pkvExpressionTokens->GetFirstSubKey() != NULL );

	PopulateQuestStringTokens( *this, *pkvExpressionTokens, *m_kvStringTokens );

	if ( !m_strBonusExpr.IsEmpty() )
	{
		KeyValues * pKVBonusExpressionTokens = new KeyValues( "BonusExpressionTokens" );
		KeyValues::AutoDelete autodelete_pKVBonusExpressionTokens( pKVBonusExpressionTokens );

		TokenizeQuestExpression( m_strBonusExpr, pKVBonusExpressionTokens );

		Assert( pKVBonusExpressionTokens->GetFirstSubKey() != NULL );

		PopulateQuestStringTokens( *this, *pKVBonusExpressionTokens, *m_kvStringTokens, true );
	}


#endif // ifndef GAME_DLL

	// MAPGROUP
	//
	//
	bool bMapGroupValid = false;
	if ( m_strMapGroup.IsEmpty() )	{ bMapGroupValid = true; }	// "": empty mapgroups are technically valid
	/** Removed for partner depot **/

	AssertMsg2( bMapGroupValid, "QUEST %d references a non-existant mapgroup: %s.\n", nID, m_strMapGroup.Get() );

#ifndef GAME_DLL	// strings

	KeyValues *pGameModestxt = new KeyValues( "GameModes.txt" );
	KeyValues::AutoDelete autodelete( pGameModestxt );

	if ( !pGameModestxt->LoadFromFile( g_pFullFileSystem, "GameModes.txt" ) )
	{
		pGameModestxt = NULL;
	}

	// MAP
	//
	//

	// get map string from gamemodes.txt and insert it into m_kvStringTokens
	//
	//
	if ( !m_strMap.IsEmpty( ) && StringIsEmpty( m_kvStringTokens->GetString( "map" ) ) )
	{
		KeyValues *pMaps = pGameModestxt->FindKey( "maps" );

		if ( pMaps )
		{
			KeyValues *pMapKV = pMaps->FindKey( m_strMap );
			Assert( pMapKV );

			if ( pMapKV )
			{
				m_kvStringTokens->SetString( "map", pMapKV->GetString( "nameID" ) );
				m_kvStringTokens->SetString( "location", pMapKV->GetString( "nameID" ) );
			}
		}
	}

	// get mapgroup string from gamemodes.txt and insert it into m_kvStringTokens
	//
	//
	if ( !m_strMapGroup.IsEmpty() && StringIsEmpty( m_kvStringTokens->GetString( "mapgroup" ) ) )
	{
		KeyValues *pMapGroups = pGameModestxt->FindKey( "mapgroups" );

		if ( pMapGroups )
		{
			KeyValues *pMapGroup = pMapGroups->FindKey( m_strMapGroup );
			Assert( pMapGroup );

			if ( pMapGroup )
			{
				m_kvStringTokens->SetString( "mapgroup", pMapGroup->GetString( "nameID" ) );
			}

			// if we didn't specify a map then use the mapgroup as the location
			if ( !m_kvStringTokens->FindKey("location")  )
			{
				m_kvStringTokens->SetString( "location", pMapGroup->GetString( "nameID" ) );
			}
		}
	}


	//GAMEMODE
	//
	//
#ifdef DBGFLAG_ASSERT
	const char * szType;
	bool bGameModeValid = g_pGameTypes->GetGameTypeFromMode( m_strGameMode, szType );
	AssertMsg2( bGameModeValid, "QUEST %d references a non-existant gamemode: %s.\n", nID, m_strGameMode );
#endif
	if ( StringIsEmpty( m_kvStringTokens->GetString( "gamemode" ) ) )	
	{

		// get gamemode string from gamemodes.txt and insert it into m_kvStringTokens
		//
		//
		KeyValues *pGameTypes = pGameModestxt->FindKey( "gameTypes" );

		FOR_EACH_SUBKEY( pGameTypes, kvGameType )
		{
			KeyValues *pGameModes = kvGameType->FindKey( "GameModes" );
			if ( pGameModes )
			{
				KeyValues *pGameMode = pGameModes->FindKey( m_strGameMode );
				if ( pGameMode )
				{
					m_kvStringTokens->SetString( "gamemode", pGameModes->FindKey( m_strGameMode )->GetString( "NameID" ) );

					break;
				}
			}
		}
	}

	// check all strings in m_kvStringTokens
	FOR_EACH_VALUE( m_kvStringTokens, pKVStringToken )
	{
		const char* token = pKVStringToken->GetString();
		HelperValidateLocalizationStringToken( token );
	}

	FOR_EACH_TRUE_SUBKEY( m_kvStringTokens, pKVSubKey )
	{
		FOR_EACH_VALUE( pKVSubKey, pKVStringToken )
		{
			const char* token = pKVStringToken->GetString( );
			HelperValidateLocalizationStringToken( token );
		}
	}
#endif // not GAME_DLL

	return SCHEMA_INIT_SUCCESS();
}

#ifdef CLIENT_DLL
///////////////////////////////////////////////////////////////////////////////////////////
//
// read the expression ( i.e. " %act_kill_human% && %cond_gun_borrowed% && %weapon_smg% ) 
// and insert appropriate string tokens into the string token keyvalues structure
//
////////////////////////////////////////////////////////////////////////////////////////////
void CEconQuestDefinition::PopulateQuestStringTokens( CEconQuestDefinition &questDef, KeyValues &kvExpressionTokens, KeyValues &kvStringTokens, bool bBonus )
{
	// 1. Validate that every token ( %TOKEN% ) is accounted for.
	// 2. Populate pkvStringTokens with tokens derived from expression keywords e.g. %weapon_ak47%

	KeyValues * pkvExpressionTokens = &kvExpressionTokens;

	int nItemCount = 0;

	FOR_EACH_SUBKEY( pkvExpressionTokens, kvSubKey )
	{
		bool bFound = false;

		if ( V_stristr( kvSubKey->GetName(), "act_" ) || V_stristr( kvSubKey->GetName(), "cond_" ) )
		{
			// validate that action or condition is recognized.
			for ( int i = 0; i < k_EQuestVar_Last; i++ )
			{
				if ( FStrEq( g_arrQuestVars[ i ], kvSubKey->GetName() ) )
				{
					if ( g_pVGuiLocalize->Find( CFmtStr( "#quest_action_singular_%s", kvSubKey->GetName() ) ) )
					{
						if ( !kvStringTokens.FindKey( "action" ) )
						{
							kvStringTokens.SetString( "action", CFmtStr( "#quest_action_singular_%s", kvSubKey->GetName() ) );
						}

						if ( !kvStringTokens.FindKey( "actions" ) )
						{
							kvStringTokens.SetString( "actions", CFmtStr( "#quest_action_plural_%s", kvSubKey->GetName() ) );
						}
					}
					
					/** Removed for partner depot **/

					// note that we have at least one generic weapon if we use any item condition.
					if ( V_stristr( kvSubKey->GetName( ), "cond_item" ) )
					{
						if ( nItemCount == 0 ) nItemCount = 1;
					}

// debug					kvStringTokens.SaveToFile( g_pFullFileSystem, "~expressiontokens.txt" );
// debug					pkvExpressionTokens->SaveToFile( g_pFullFileSystem, "~expressiontokens.txt" );

					bFound = true;
					break;
				}
			}
		}
		else if ( V_stristr( kvSubKey->GetName(), "weapon_" ) )
		{
			// find the items subkey or create one if it doesnt exist
			KeyValues* pkvItems = kvStringTokens.FindKey( "items" );
			if ( !pkvItems )
			{
				pkvItems = new KeyValues( "items" );
				kvStringTokens.AddSubKey( pkvItems );
			}

			CEconItemDefinition * pWeaponItemDef = GetItemSchema( )->GetItemDefinitionByName( kvSubKey->GetName( ) );

			if ( pWeaponItemDef )
			{
				// LEGACY PRE-2016
				if ( !kvStringTokens.FindKey( "weapon" ) )
				{
					kvStringTokens.SetString( "weapon", pWeaponItemDef->GetItemBaseName() );
				}
				// LEGACY PRE-2016

				// insert the weapon into the 'items' true subkey
				for ( int i = 1; i < 20; i++ )
				{
					const char * szItemKeyName = CFmtStr( "item%d", i );
					if ( !pkvItems->FindKey( szItemKeyName ) )
					{
						pkvItems->SetString( szItemKeyName, pWeaponItemDef->GetItemBaseName( ) );

						nItemCount++;
						break;
					}
				}

				bFound = true;

				// set the quest icon to the weapon if an icon was not explicitly set.
				if ( questDef.m_strIcon.IsEmpty( ) && !bBonus )
				{
					questDef.m_strIcon = kvSubKey->GetName() + WEAPON_CLASSNAME_PREFIX_LENGTH;
				}
			}
			else
			{
				// look for a weapon category/type
				for ( int i = 0; i < LOADOUT_POSITION_COUNT; i++ )
				{
					if ( V_stristr( kvSubKey->GetName(), CFmtStr( "weapon_%s", g_szLoadoutStrings[ i ] ) ) )
					{
						// LEGACY PRE-2016
						if ( !kvStringTokens.FindKey( "weapon" ) )
						{
							kvStringTokens.SetString( "weapon", g_szLoadoutStringsForDisplay[ i ] );
						}
						// LEGACY PRE-2016

						// insert the weapon into the 'items' true subkey
						for ( int j = 1; j < 20; j++ )
						{
							const char * szItemKeyName = CFmtStr( "item%d", j );
							if ( !pkvItems->FindKey( szItemKeyName ) )
							{
								const char * szLoadoutStringNoHashMark = g_szLoadoutStringsForDisplay[ i ] + 1;
								pkvItems->SetString( szItemKeyName, CFmtStr( "#quest_%s", szLoadoutStringNoHashMark ) );

								nItemCount++;
								break;
							}
						}

						bFound = true;
						break;
					}
				}
			}
		}
		else if ( V_stristr( kvSubKey->GetName(), "set_" ) )
		{
			for ( int i = 0; i < GetItemSchema()->GetItemSetCount(); i++ )
			{
				const CEconItemSetDefinition *pItemSetDef = GetItemSchema()->GetItemSetByIndex( i );

				if ( pItemSetDef && FStrEq( kvSubKey->GetName(), pItemSetDef->GetName() ) )
				{
					if ( !kvStringTokens.FindKey( "set" ) )
					{
						kvStringTokens.SetString( "set", CFmtStr( "#CSGO_%s", pItemSetDef->GetName() ) );
					}

					bFound = true;
					break;
				}
			}
		}
		else if ( V_stristr( kvSubKey->GetName(), "map_" ) )
		{

			/** Removed for partner depot **/

		}

		// debug save
//		kvStringTokens.SaveToFile( g_pFullFileSystem, "~expressiontokens.txt" );

		AssertMsg2( bFound, "QUEST %u refs a non-existant var: %s\n", questDef.GetID(), kvSubKey->GetName() );
	}

	// defaults
	if ( !kvStringTokens.FindKey( "commandverb" ) )
	{
		kvStringTokens.SetString( "commandverb", "#quest_commandverb_default" );
	}

	if ( !kvStringTokens.FindKey( "target" ) )
	{
		kvStringTokens.SetString( "target", "#emptystring" );
	}

	if ( !kvStringTokens.FindKey( "item_quality" ) )
	{
		kvStringTokens.SetString( "item_quality", "#emptystring" );
	}

	// no items were specified but due to specified item conditions we know we need one.
	if ( !kvStringTokens.FindKey( "items/item1" ) && ( nItemCount > 0 ) )
	{
		kvStringTokens.SetString( "items/item1", "#quest_item_default" );
	}

	// no description was specified so use one of the default ones.
	if ( questDef.m_strDescriptionLocToken.IsEmpty( ) && !bBonus )
	{
		questDef.m_strDescriptionLocToken = CFmtStr( "quest_default_%d_items_desc", nItemCount );
	}

	// no quest tracker description was specified, so use one of the default ones.
	if ( questDef.m_strHudDescriptionLocToken.IsEmpty( ) && !bBonus )
	{
		questDef.m_strHudDescriptionLocToken = CFmtStr( "quest_default_hud_%d_items_desc", nItemCount );
	}
	
//	kvStringTokens.SaveToFile( g_pFullFileSystem, "~stringtokens.txt" ); // debug only
}

#endif

// NOTE: This does not detect syntax or unrecognized symbols. TODO: Still need to figure out how to detect those.
bool CEconQuestDefinition::IsQuestExpressionValid( const char * pszQuestExpr )
{
	if ( !pszQuestExpr || !pszQuestExpr[0] )
		return false;

	CExpressionCalculator expr = CExpressionCalculator( pszQuestExpr ); 

	ZeroOutQuestExpressionVariables( expr );


	// This seemed like a good idea but it doesn't work.
	//
// 	for ( int i = 0; i < expr.VariableCount(); i++ )
// 	{
// 		if ( !StringIsEmpty( expr.GetVariableName( i ) ) && expr.GetVariableValue( i ) != 1.0 )
// 		{
// 			AssertMsg1( 0, "unknown variable %s in quest expression", expr.GetVariableName( i ) );
// 			return false;
// 		}
// 	}

	float flResult;
	bool bSuccess = expr.Evaluate( flResult );

	return ( bSuccess && IsFinite(flResult) );
}

void CEconQuestDefinition::SetQuestExpressionVariable( CExpressionCalculator &expQuest, EQuestVar_t questVar, float flValue )
{
	expQuest.SetVariable( CFmtStr( "%%%s%%", g_arrQuestVars[ questVar ] ), flValue );
}

void CEconQuestDefinition::ZeroOutQuestExpressionVariables( CExpressionCalculator &expr )
{
	// it's not possible to iterate through expression variables but this would be ideal. VariableCount() returns 1 :/
// 	expQuest.BuildVariableListFromExpression();
// 
// 	for ( int i = 0; i < expQuest.VariableCount(); i++ )
// 	{
// 		expQuest.SetVariable( i, 0 );
// 	}


	// ACTIONS and CONDITIONS
	/** Removed for partner depot **/

	// WEAPON TYPES
	for ( int i = 0; i < LOADOUT_POSITION_COUNT; i++ )
	{
		expr.SetVariable( CFmtStr( "%%weapon_%s%%", g_szLoadoutStrings[ i ] ), 0 );
	}

	// SETS
	for ( int i = 0; i < GetItemSchema()->GetItemSetCount(); i++ )
	{
		const CEconItemSetDefinition *pItemSetDef = GetItemSchema()->GetItemSetByIndex( i );

		expr.SetVariable( CFmtStr( "%%%s%%", pItemSetDef->GetName() ), 0 );
	}

	// WEAPONS
	static CUtlVector< const CEconItemDefinition * > s_vecWeaponItemDefs;

	// cache these items so we don't do string compares every time we initialize an expression
	if ( s_vecWeaponItemDefs.Count() == 0 )
	{
		FOR_EACH_MAP_FAST( GetItemSchema()->GetItemDefinitionMap(), i )
		{
			const CEconItemDefinition * pItemDef = GetItemSchema()->GetItemDefinitionByMapIndex( i );

			if ( V_stristr( pItemDef->GetDefinitionName(), "weapon_" ) )
			{
				s_vecWeaponItemDefs.AddToTail( pItemDef );
			}
		}
	}

	FOR_EACH_VEC( s_vecWeaponItemDefs, i )
	{
		expr.SetVariable( CFmtStr( "%%%s%%", s_vecWeaponItemDefs[ i ]->GetDefinitionName() ), 0 );
	}

	// MAPS
	/** Removed for partner depot **/

}


bool CEconCampaignDefinition::BInitFromKV( KeyValues *pKVCampaignDef, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /*= NULL */ )
{
	m_nID = Q_atoi( pKVCampaignDef->GetName() );
	SCHEMA_INIT_CHECK(
		( m_nID > 0 ),
		CFmtStr( "Campaign definition %d: name must be a positive integer", GetID() ) );

	m_strNameLocToken = pKVCampaignDef->GetString( "loc_name" );
	HelperValidateLocalizationStringToken( m_strNameLocToken );

	m_strLocDescription = pKVCampaignDef->GetString( "loc_description" );
	HelperValidateLocalizationStringToken( m_strLocDescription );

	m_nSeasonNumber = pKVCampaignDef->GetInt( "season_number" );

	SCHEMA_INIT_CHECK( m_nSeasonNumber > 0, CFmtStr( "Campaign %d must specifiy a season number", GetID() ) );

	FOR_EACH_TRUE_SUBKEY( pKVCampaignDef, pKVCampaignNode )
	{
		CEconCampaignNodeDefinition *pNewCampaignNodeDef = new CEconCampaignNodeDefinition;

		int CampaignNodeIndex = Q_atoi( pKVCampaignNode->GetName() );

		uint32 index = m_mapCampaignNodes.Find( CampaignNodeIndex );
		if ( !m_mapCampaignNodes.IsValidIndex( index ) )
		{
			m_mapCampaignNodes.Insert( CampaignNodeIndex, pNewCampaignNodeDef );
			SCHEMA_INIT_SUBSTEP( pNewCampaignNodeDef->BInitFromKV( GetID(), pKVCampaignNode, pschema ) );
		}
		else
		{
			SCHEMA_INIT_CHECK(
				false,
				CFmtStr( "Campaign definition %d: campaign node index %d is not unique", GetID(), CampaignNodeIndex ) );
		}
	}

	// go back and verify that all nodes point to valid nodes
	FOR_EACH_MAP_FAST( m_mapCampaignNodes, i )
	{
		const CUtlVector< uint32 >& pNextNodes = m_mapCampaignNodes.Element( i )->GetNextNodes();

		FOR_EACH_VEC( pNextNodes, j )
		{
			int nextnode = pNextNodes[ j ];

			int index = m_mapCampaignNodes.Find( nextnode );

			SCHEMA_INIT_CHECK( 
				m_mapCampaignNodes.IsValidIndex( index ),
				CFmtStr( "Campaign definition %d: campaign node index %d leads to an invalid campaign node %d", GetID(), m_mapCampaignNodes.Element( i )->GetID(), nextnode ) );
		
		}

	}

	// build list of start nodes ( nodes that have no predecessors )

	m_mapStartNodes.Purge();

	// collect all nodes
	FOR_EACH_MAP_FAST( m_mapCampaignNodes, i )
	{
		m_mapStartNodes.Insert( m_mapCampaignNodes.Key( i ), m_mapCampaignNodes.Element( i ) );
	}

	// subtract nodes referenced as successors
	FOR_EACH_MAP_FAST( m_mapCampaignNodes, i )
	{
		const CUtlVector< uint32 >& pCampaignNodes = m_mapCampaignNodes.Element( i )->GetNextNodes();

		FOR_EACH_VEC( pCampaignNodes, j )
		{
			int nextnode = pCampaignNodes.Element( j );

			m_mapStartNodes.Remove( nextnode );
		}
	}

	return SCHEMA_INIT_SUCCESS();

}

void CEconCampaignDefinition::GetAccessibleCampaignNodes( const uint32 unCampaignCompletionBitfield, CUtlVector< CEconCampaignNodeDefinition* > &vecAccessibleNodes )
{
	vecAccessibleNodes.Purge();

	// If no campaign quests have been done yet then the only possible quests are the starting quests.
	if ( unCampaignCompletionBitfield == 0 )
	{
		FOR_EACH_MAP_FAST( GetStartNodes(), i )
		{
			vecAccessibleNodes.AddToTail( GetStartNodes().Element( i ) );
		}
	}
	else
	{
		// otherwise recursively look for quests that are successors of completed quests.
		FOR_EACH_MAP_FAST( GetStartNodes(), i )
		{
			uint32 nCampaignNodeIndex = GetStartNodes().Key( i );

			// is this start quest already complete?
			if ( ( 1 << ( nCampaignNodeIndex - 1 ) ) & unCampaignCompletionBitfield )
			{
				// yes. recurse.
				Helper_RecursiveGetAccessibleCampaignNodes( unCampaignCompletionBitfield, GetStartNodes().Element( i ), vecAccessibleNodes );
			}
			else
			{
				// no. add it to the list and return;
				vecAccessibleNodes.AddToTail( GetStartNodes().Element( i ) );
			}
		}
	}
}

void CEconCampaignDefinition::Helper_RecursiveGetAccessibleCampaignNodes( const uint32 unCampaignCompletionBitfield, const CEconCampaignNodeDefinition* pNode, CUtlVector< CEconCampaignNodeDefinition* > &vecAccessibleNodes )
{
	// recursively look for nodes that don't represent completed quests. Do not recurse incomplete quests.
	FOR_EACH_VEC( pNode->GetNextNodes(), i )
	{
		uint32 unNextNodeIndex = pNode->GetNextNodes().Element( i );
		int index = GetCampaignNodes().Find( unNextNodeIndex );
		if ( GetCampaignNodes().IsValidIndex( index ) )
		{
			CEconCampaignDefinition::CEconCampaignNodeDefinition* pNextNode = GetCampaignNodes().Element( index );

			// is this successor quest already complete?
			if ( ( 1 << ( unNextNodeIndex - 1 ) ) & unCampaignCompletionBitfield )	
			{
				// yes. recurse.
				Helper_RecursiveGetAccessibleCampaignNodes( unCampaignCompletionBitfield, pNextNode, vecAccessibleNodes );
			}
			else
			{
				// no. add it to the list and return;
				vecAccessibleNodes.AddToTail( pNextNode );
			}
		}
	}

}

bool ResolveQuestIdToCampaignAndIndex( uint16 unQuestID, uint32 &unCampaignID, uint32 &unCamapaignNodeID )
{
	static bool s_bQuestMappingInitialized = false;
	static CUtlMap< uint16, uint64, uint16, CDefLess< uint16 > > s_mapping;
	if ( !s_bQuestMappingInitialized )
	{
		s_bQuestMappingInitialized = true;
		for ( uint32 unPossibleCampaignID = 1; unPossibleCampaignID <= g_nNumCampaigns; ++unPossibleCampaignID )
		{
			CEconCampaignDefinition const *pCampaignDef = GetItemSchema()->GetCampaignDefinition( unPossibleCampaignID );
			if ( !pCampaignDef )
				continue;

			FOR_EACH_MAP_FAST( pCampaignDef->GetCampaignNodes(), iCN )
			{
				CEconCampaignDefinition::CEconCampaignNodeDefinition const &node = *pCampaignDef->GetCampaignNodes().Element( iCN );
				uint16 unMappingKey = node.GetQuestIndex();
				if ( unMappingKey )
				{
					uint64 unMappingValue = ( uint64( unPossibleCampaignID ) << 32 ) | uint64( node.GetID() );
					Assert( s_mapping.Find( unMappingKey ) == s_mapping.InvalidIndex() );
					s_mapping.InsertOrReplace( unMappingKey, unMappingValue );
				}
			}
		}
	}

	uint16 idxMapping = s_mapping.Find( unQuestID );
	if ( idxMapping == s_mapping.InvalidIndex() )
		return false;

	unCampaignID = uint32( s_mapping.Element( idxMapping ) >> 32 );
	unCamapaignNodeID = uint32( s_mapping.Element( idxMapping ) );
	return true;
}

bool CEconCampaignDefinition::CEconCampaignNodeDefinition::BInitFromKV( int nCampaignIndex, KeyValues *pKVCampaignNodeDef, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /*= NULL */ )
{
	m_nID = Q_atoi( pKVCampaignNodeDef->GetName() );
	SCHEMA_INIT_CHECK(
		( m_nID > 0 ),
		CFmtStr( "Campaign Node definition %d: name must be a positive integer", GetID() ) );

	m_CampaignID = nCampaignIndex;


	m_nQuestIndex = Q_atoi( pKVCampaignNodeDef->GetString( "quest_index" ) );
	if ( m_nQuestIndex ) // validate only if specified, story-only nodes don't have quest indices
	{
		SCHEMA_INIT_CHECK(
			( GetItemSchema()->GetQuestDefinition( m_nQuestIndex ) != NULL ),
			CFmtStr( "Campaign definition %d, Node definition %d: Quest Index %d is not a valid quest index", nCampaignIndex, m_nID, m_nQuestIndex ) );
	}
	m_vecNextNodes.Purge();

	FOR_EACH_SUBKEY( pKVCampaignNodeDef, pKVCampaignNodeKey )
	{
		if ( !V_strcmp( pKVCampaignNodeKey->GetName(), "->" ) )
		{
			m_vecNextNodes.AddToTail( Q_atoi( pKVCampaignNodeKey->GetString() ) );
		}
	}

#ifdef CLIENT_DLL
	FOR_EACH_TRUE_SUBKEY( pKVCampaignNodeDef, pKVCampaignNodeKey )
	{
		if ( FStrEq( pKVCampaignNodeKey->GetName(), "story_block" )  )
		{
			CEconCampaignNodeStoryBlockDefinition *pNewCampaignNodeStoryBlockDef = new CEconCampaignNodeStoryBlockDefinition;

			m_vecStoryBlocks.AddToTail( pNewCampaignNodeStoryBlockDef );
			SCHEMA_INIT_SUBSTEP( pNewCampaignNodeStoryBlockDef->BInitFromKV( nCampaignIndex, m_nID, pKVCampaignNodeKey, pschema ) );
		}
	}
#endif

	return SCHEMA_INIT_SUCCESS();

}

#ifdef CLIENT_DLL
// score every story block and return the highest scoring one
CEconCampaignDefinition::CEconCampaignNodeDefinition::CEconCampaignNodeStoryBlockDefinition * CEconCampaignDefinition::CEconCampaignNodeDefinition::GetBestScoringStoryBlock(  CEconItemView *pCampaignCoin  ) const
{
	CEconCampaignDefinition::CEconCampaignNodeDefinition::CEconCampaignNodeStoryBlockDefinition * pBestStoryBlock = NULL;
	float flBestStoryBlockScore = 0;

	
	FOR_EACH_VEC( GetStoryBlocks(), iSB )
	{
		float flCurStoryBlockScore = GetStoryBlocks()[ iSB ]->EvaluateStoryBlockExpression(  pCampaignCoin  );

		// only consider story block expressions that have a positive score
		if ( flCurStoryBlockScore > 0 )
		{
			if ( !pBestStoryBlock )
			{
				pBestStoryBlock = GetStoryBlocks()[ iSB ];
				flBestStoryBlockScore = flCurStoryBlockScore;
			}
			else
			{
				if ( flCurStoryBlockScore > flBestStoryBlockScore )
				{
					pBestStoryBlock = GetStoryBlocks()[ iSB ];
					flBestStoryBlockScore = flCurStoryBlockScore;
				}
			}
		}
	}

	return pBestStoryBlock;
}

bool CEconCampaignDefinition::CEconCampaignNodeDefinition::CEconCampaignNodeStoryBlockDefinition::BInitFromKV( int nCampaignIndex, int nNodeID, KeyValues * pKVCampaignNodeStoryBlockDef, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{

	m_strContentFile = pKVCampaignNodeStoryBlockDef->GetString( "content_file" );
	m_strCharacterName = pKVCampaignNodeStoryBlockDef->GetString( "character_name" );
	m_strDescription = pKVCampaignNodeStoryBlockDef->GetString( "description" );
	if ( !m_strDescription.IsEmpty() )
		HelperValidateLocalizationStringToken( m_strDescription );

	m_strExpr = pKVCampaignNodeStoryBlockDef->GetString( "expression" );

	return SCHEMA_INIT_SUCCESS();
}

// Take the expression of an CExpressionCalculator, such as " %weapon_ak47% && %kill% " and extract the % delimited keywords.
//
void CEconQuestDefinition::TokenizeQuestExpression( const char * szExpression, KeyValues * pKVExpressionTokens )
{
	if ( !szExpression || !szExpression[0] )
		return;

	if ( !pKVExpressionTokens )
		return;

	const char * pCur = szExpression;

	char szKeyword[ 256 ] = { '\0' };
	char *pCurCopy = szKeyword;

	bool bReadingKeyword = false;

	while ( *pCur != '\0' )
	{
		if ( *pCur == '%' && !bReadingKeyword )
		{
			bReadingKeyword = true;

			// reset destination buffer;
			V_memset( szKeyword, '\0', sizeof( szKeyword ) );
			pCurCopy = szKeyword;

			// step past '%'
			pCur++;
		}
		else if ( *pCur == '%' && bReadingKeyword )
		{
			// end of the keyword

			bReadingKeyword = false;

			// terminate the string with a null char and then look it up
			pCurCopy++;
			*pCurCopy = '\0';

			pKVExpressionTokens->SetBool( szKeyword, true );

			// step past '%'
			pCur++;
		}

		if ( bReadingKeyword )
		{
			// keep copying the keyword until we reach the terminating '%'
			*pCurCopy = *pCur;

			pCurCopy++;
			pCur++;
		}
		else
		{
			// keep walking until we find the start of a keyword: '%'
			pCur++;
		}
	}


}

float CEconCampaignDefinition::CEconCampaignNodeDefinition::CEconCampaignNodeStoryBlockDefinition::EvaluateStoryBlockExpression( CEconItemView *pCampaignCoin ) const
{
	// if there's no expression specified then return a minimally true value.
	if ( m_strExpr.IsEmpty() )
		return 0.99;	// default without an expression is trumped by any block with an expression result of 1.0. This means that any true expression block will get picked
						// over any block with no expression.

	CExpressionCalculator expStoryBlock = CExpressionCalculator( GetStoryBlockExpression() );

	KeyValues * pKVExpressionTokens = new KeyValues( "ExpressionTokens" );
	KeyValues::AutoDelete autodelete_pKVExpressionTokens( pKVExpressionTokens );

	CEconQuestDefinition::TokenizeQuestExpression( GetStoryBlockExpression(), pKVExpressionTokens );

	// go through each referenced quest and set the variable %questid% to the value of its completion state ( 0 or 1 )
	FOR_EACH_SUBKEY( pKVExpressionTokens, kvSubKey )
	{
		uint16 unQuestID = V_atoi( kvSubKey->GetName() );

		uint32 unCampaignID;
		uint32 unCampaignNodeID;

		ResolveQuestIdToCampaignAndIndex( unQuestID, unCampaignID, unCampaignNodeID );

		const CSchemaAttributeDefHandle pAttr_CampaignCompletionBitfield = GetCampaignAttributeDefHandle( unCampaignID, k_ECampaignAttribute_CompletionBitfield );
		
		uint32 unCampaignCompletionBitfield;
		if ( !pCampaignCoin->FindAttribute( pAttr_CampaignCompletionBitfield, &unCampaignCompletionBitfield ) )
			return 0;

		uint32 unMask = ( 1 << ( unCampaignNodeID - 1 ) );

		const char * szKeyWord = CFmtStr( "%%%s%%", kvSubKey->GetName() );
		float flCompletionState = ( unCampaignCompletionBitfield & unMask ) ? 1.0 : 0.0;
		expStoryBlock.SetVariable( szKeyWord, flCompletionState );
	}

	expStoryBlock.BuildVariableListFromExpression();

	float flReturn;
	expStoryBlock.Evaluate( flReturn );

	return flReturn;

}
#endif

bool item_list_entry_t::InitFromName( const char *pchName )
{
	if ( pchName[ 0 ] == '[' )
	{
		// This item has an attached paint type that can affect the name and rarity!
		pchName++;

		char szPaintName[ 256 ];
		int i;
		for( i = 0; i < ARRAYSIZE( szPaintName ) - 1 && pchName[ i ] != ']'; ++i )
		{
			szPaintName[ i ] = pchName[ i ];
		}

		szPaintName[ i ] = '\0';

		if ( pchName[ i ] != ']' )
			return false;

		pchName += i + 1;

		const CPaintKit *pPaintKit = GetItemSchema()->GetPaintKitDefinitionByName( szPaintName );
		if ( pPaintKit )
		{
			m_nPaintKit = pPaintKit->nID;
		}
		else
		{
			const CStickerKit *pStickerKit = GetItemSchema()->GetStickerKitDefinitionByName( szPaintName );
			if ( pStickerKit )
			{
				m_nStickerKit = pStickerKit->nID;
			}
			else
			{
				const CEconMusicDefinition *pMusicKit = GetItemSchema()->GetMusicKitDefinitionByName( szPaintName );
				if ( pMusicKit )
				{
					m_nMusicKit = pMusicKit->GetID();
				}
				else
				{
					DevWarning( "Kit \"[%s]\" specified, but doesn't exist!! You're probably missing an entry in items_paintkits.txt or items_stickerkits.txt or need to run with -use_local_item_data\n", szPaintName );
					return false;
				}
			}
		}
	}

	CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinitionByName( pchName );
	if ( pDef )
	{
		m_nItemDef = pDef->GetDefinitionIndex();
	}

	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CEconItemSetDefinition::CEconItemSetDefinition( void )
	: m_pszName( NULL )
	, m_pszLocalizedName( NULL )
	, m_pszLocalizedDescription( NULL )
	, m_pszUnlocalizedName( NULL )
	, m_iBundleItemDef( INVALID_ITEM_DEF_INDEX )
	, m_bIsCollection( false )
	, m_bIsHiddenSet( false )
	, m_nCraftReward( 0 )
{
}

//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CEconItemSetDefinition::CEconItemSetDefinition( const CEconItemSetDefinition &that )
{
	(*this) = that;
}

//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconItemSetDefinition &CEconItemSetDefinition::operator=( const CEconItemSetDefinition &other )
{
	m_pszName = other.m_pszName;
	m_pszLocalizedName = other.m_pszLocalizedName;
	m_pszUnlocalizedName = other.m_pszUnlocalizedName;
	m_pszLocalizedDescription = other.m_pszLocalizedDescription;
	m_pszUnlocalizedName = other.m_pszUnlocalizedName;
	m_ItemEntries = other.m_ItemEntries;
	m_iAttributes = other.m_iAttributes;
	m_iBundleItemDef = other.m_iBundleItemDef;
	m_bIsCollection = other.m_bIsCollection;
	m_bIsHiddenSet = other.m_bIsHiddenSet;
	m_nCraftReward = other.m_nCraftReward;

	return *this;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CEconItemSetDefinition::BInitFromKV( KeyValues *pKVItemSet, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors )
{
	m_pszName = pKVItemSet->GetName();

	m_iBundleItemDef = INVALID_ITEM_DEF_INDEX;
	const char *pszBundleName = pKVItemSet->GetString( "store_bundle" );
	if ( pszBundleName && pszBundleName[0] )
	{
		const CEconItemDefinition *pDef = pschema.GetItemDefinitionByName( pszBundleName );
		if ( pDef )
		{
			m_iBundleItemDef = pDef->GetDefinitionIndex();
		}

		SCHEMA_INIT_CHECK( 
			pDef != NULL,
			CFmtStr( "Item Set %s: Bundle definition \"%s\" was not found", m_pszName, pszBundleName ) );
	}

	FOR_EACH_SUBKEY( pKVItemSet, pKVSetItem )
	{
		const char *pszName = pKVSetItem->GetName();

		if ( !Q_strcmp( pszName, "name" ) )
		{
			SCHEMA_INIT_CHECK(
				m_pszLocalizedName == NULL,
				CFmtStr( "Item Set %s: Duplicate name specified", m_pszName ) );

			m_pszLocalizedName = pKVSetItem->GetString();
		}
		else if ( !Q_strcmp( pszName, "set_description" ) )
		{
			m_pszLocalizedDescription = pKVSetItem->GetString();
		}
		else if ( !Q_strcmp( pszName, "unlocalized_name" ) )
		{
			m_pszUnlocalizedName = pKVSetItem->GetString();
		}
		else if ( !Q_strcmp( pszName, "is_collection" ) )
		{
			m_bIsCollection = pKVSetItem->GetBool();
		}
		else if ( !Q_strcmp( pszName, "is_hidden_set" ) )
		{
			m_bIsHiddenSet = pKVSetItem->GetBool();
		}
// 		else if ( !Q_strcmp( pszName, "craft_reward" ) )
// 		{
// 			const char *pchCraftReward = pKVSetItem->GetString();
// 			if ( pchCraftReward && pchCraftReward[ 0 ] != '\0' )
// 			{
// 				const CEconItemDefinition *pItemDef = pschema.GetItemDefinitionByName( pKVSetItem->GetString() );
// 
// 	 			SCHEMA_INIT_CHECK( 
// 	 				pItemDef && pItemDef->GetDefinitionIndex() > 0,
// 	 				CFmtStr( "Item Collection Craft Reward \"%s\" specifies invalid item def", pKVItemSet->GetName() ) );
// 
// 				m_nCraftReward = pItemDef->GetDefinitionIndex();
// 			}
// 		}
		else if ( !Q_strcmp( pszName, "items" ) )
		{
			FOR_EACH_SUBKEY( pKVSetItem, pKVItem )
			{
				pszName = pKVItem->GetName();

				item_list_entry_t entry;
				bool bEntrySuccess = entry.InitFromName( pszName );
				bEntrySuccess;

				m_ItemEntries.AddToTail( entry );
			}
		}
		else if ( !Q_strcmp( pszName, "attributes" ) )
		{
			FOR_EACH_SUBKEY( pKVSetItem, pKVAttribute )
			{
				pszName = pKVAttribute->GetName();

				const CEconItemAttributeDefinition *pDef = pschema.GetAttributeDefinitionByName( pszName );
//				SCHEMA_INIT_CHECK( 
//					pDef != NULL,
//					CFmtStr( "Item set %s: Attribute definition \"%s\" was not found", m_pszName, pszName ) );

				if ( pDef )
				{
					int iIndex = m_iAttributes.AddToTail();
					m_iAttributes[iIndex].m_iAttribDefIndex = pDef->GetDefinitionIndex();
					if ( pDef->IsStoredAsInteger() )
					{
						m_iAttributes[iIndex].m_valValue = pKVAttribute->GetInt( "value" );
					}
					else
					{
						float flValue = pKVAttribute->GetFloat( "value" );
						Q_memcpy( &m_iAttributes[iIndex].m_valValue, &flValue, sizeof( float ) );
					}
				}
			}
		}
	}

	// Sanity check.
	SCHEMA_INIT_CHECK( !(m_bIsCollection && m_bIsHiddenSet),
		CFmtStr( "Item Set %s: Invalid to set a collection to be hidden", m_pszName ) );

	SCHEMA_INIT_CHECK( m_ItemEntries.Count() > 0,
		CFmtStr( "Item Set %s: Set contains no items", m_pszName ) );

	return SCHEMA_INIT_SUCCESS();
}

int CEconItemSetDefinition::GetItemRarity( int iIndex ) const
{
	int nKitRarity = 1;
	const CStickerKit *pStickerKit = NULL;
	const CPaintKit *pPaintKit = NULL;
	const CEconMusicDefinition *pMusicKitDefinition = NULL;

	item_list_entry_t const &entry = m_ItemEntries[iIndex];

	if ( entry.m_nPaintKit )
	{
		pPaintKit = GetItemSchema()->GetPaintKitDefinition( entry.m_nPaintKit );
		if ( pPaintKit )
		{
			nKitRarity = MAX( nKitRarity, pPaintKit->nRarity );
		}
	}
	if ( entry.m_nStickerKit )
	{
		pStickerKit = GetItemSchema()->GetStickerKitDefinition( entry.m_nStickerKit );
		if ( pStickerKit )
		{
			nKitRarity = MAX( nKitRarity, pStickerKit->nRarity );
		}
	}
	if ( entry.m_nMusicKit )
	{
		pMusicKitDefinition = GetItemSchema()->GetMusicDefinition( entry.m_nMusicKit );
		if ( pMusicKitDefinition )
		{
			nKitRarity = Max ( nKitRarity, 1/* pMusicKitDefinition->GetRarity() */);
		}

	}

	// Now combine the rarity
	const CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinition( entry.m_nItemDef );
	return pDef ? EconRarity_CombinedItemAndPaintRarity( pDef->GetRarity(), nKitRarity ) : -1;
}

int CEconItemSetDefinition::GetHighestItemRarityValue( void ) const
{
	int nHighestRarityValue = -1;
	for ( int i = 0; i < GetItemCount(); ++i )
	{
		int nRarity = GetItemRarity( i );
		if ( nHighestRarityValue < nRarity )
		{
			nHighestRarityValue = nRarity;
		}
	}

	return nHighestRarityValue;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CEconLootListDefinition::CEconLootListDefinition( void )
{
	m_bServerList = false;
}

//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CEconLootListDefinition::CEconLootListDefinition( const CEconLootListDefinition &that )
{
	(*this) = that;
}

//-----------------------------------------------------------------------------
// Dtor
//-----------------------------------------------------------------------------
CEconLootListDefinition::~CEconLootListDefinition( void )
{
}


//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconLootListDefinition &CEconLootListDefinition::operator=( const CEconLootListDefinition &other )
{
	m_pszName = other.m_pszName;
	m_ItemEntries = other.m_ItemEntries;
	m_AdditionalDrops = other.m_AdditionalDrops;
	m_bServerList = other.m_bServerList;

	return *this;
}

bool CEconLootListDefinition::AddRandomAtrributes( KeyValues *pRandomAttributesKV, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /*= NULL*/ )
{
	// We've found the random attribute block. Parse it.
	SCHEMA_INIT_CHECK( 
		NULL != pRandomAttributesKV->FindKey( "chance" ), 
		CFmtStr( "Loot List %s: Missing required field \"chance\" in the \"random_attributes\" block.", m_pszName ) );

	random_attrib_t *randomAttrib = new random_attrib_t;

	randomAttrib->m_flChanceOfRandomAttribute = pRandomAttributesKV->GetFloat( "chance" );
	randomAttrib->m_bPickAllAttributes = ( pRandomAttributesKV->GetFloat( "pick_all_attributes" ) != 0 );
	randomAttrib->m_flTotalAttributeWeight = 0;

	FOR_EACH_TRUE_SUBKEY( pRandomAttributesKV, pKVAttribute )
	{
		const char *pszName = pKVAttribute->GetName();

		if ( !Q_strcmp( pszName, "chance" ) )
			continue;

		const CEconItemAttributeDefinition *pDef = pschema.GetAttributeDefinitionByName( pszName );
		SCHEMA_INIT_CHECK( 
			pDef != NULL,
			CFmtStr( "Loot list %s: Attribute definition \"%s\" was not found", m_pszName, pszName ) );

		if ( pDef )
		{
			lootlist_attrib_t lootListAttrib;

			SCHEMA_INIT_SUBSTEP( lootListAttrib.BInitFromKV( m_pszName, pKVAttribute, pschema, pVecErrors ) );

			randomAttrib->m_flTotalAttributeWeight += lootListAttrib.m_flWeight;

			randomAttrib->m_RandomAttributes.AddToTail( lootListAttrib );
		}
	}

	m_RandomAttribs.AddToTail( randomAttrib );

	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CEconLootListDefinition::BInitFromKV( KeyValues *pKVLootList, KeyValues *pKVRandomAttributeTemplates, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors, bool bServerList )
{
	m_pszName = pKVLootList->GetName();

	m_unHeroID = 0;

	m_bServerList = bServerList;

	FOR_EACH_SUBKEY( pKVLootList, pKVListItem )
	{
		const char *pszName = pKVListItem->GetName();

		if ( !Q_strcmp( pszName, "random_attributes" ) )
		{
			AddRandomAtrributes( pKVListItem, pschema, pVecErrors );

			continue;
		}
		else if ( !Q_strcmp( pszName, "attribute_templates" ) )
		{
			if ( !pKVRandomAttributeTemplates )
			{
				// Clients don't load the attribute template list
				// Skip over attribute templates
				continue;
			}

			FOR_EACH_SUBKEY( pKVListItem, pKVAttributeTemplate )
			{
				if ( pKVAttributeTemplate->GetDataType() == KeyValues::TYPE_NONE )
				{	// Support full template specified inplace
					AddRandomAtrributes( pKVAttributeTemplate, pschema, pVecErrors );
				}
				else
				{
					if ( pKVAttributeTemplate->GetInt() == 0 )
						continue;

					KeyValues *pKVRandomAttribute = pKVRandomAttributeTemplates->FindKey( pKVAttributeTemplate->GetName() );
					SCHEMA_INIT_CHECK(
						pKVRandomAttribute,
						CFmtStr( "Loot List %s: Looking for attribute template \"%s\" that couldn't be found in \"random_attribute_templates\" block.\n", m_pszName, pKVAttributeTemplate->GetName() ) );

					AddRandomAtrributes( pKVRandomAttribute, pschema, pVecErrors );
				}
			}

			continue;
		}
		else if ( !Q_strcmp( pszName, "public_list_contents" ) )
		{
			m_bPublicListContents = pKVListItem->GetBool();
			continue;
		}
		if ( !Q_strcmp( pszName, "additional_drop" ) )
		{
			float		fChance		 = pKVListItem->GetFloat( "chance", 0.0f );
			bool		bPremiumOnly = pKVListItem->GetBool( "premium_only", false );
			const char *pszLootList	 = pKVListItem->GetString( "loot_list", "" );

			SCHEMA_INIT_CHECK(
				fChance > 0.0f && fChance <= 1.0f,
				CFmtStr( "Loot list %s: Invalid \"additional_drop\" chance %.2f\n", m_pszName, fChance ) );

			SCHEMA_INIT_CHECK(
				pszLootList && pszLootList[0],
				CFmtStr( "Loot list %s: Missing \"additional_drop\" loot list name\n", m_pszName ) );

			if ( pszLootList )
			{
				const CEconLootListDefinition *pLootListDef = pschema.GetLootListByName( pszLootList );
				SCHEMA_INIT_CHECK(
					pLootListDef,
					CFmtStr( "Loot list %s: Invalid \"additional_drop\" loot list \"%s\"\n", m_pszName, pszLootList ) );

				if ( pLootListDef )
				{
					loot_list_additional_drop_t additionalDrop = { fChance, bPremiumOnly, pszLootList };
					m_AdditionalDrops.AddToTail( additionalDrop );
				}
			}
			continue;
		}

		if ( !Q_strcmp( pszName, "hero" ) )
		{
			const char* pszHeroName = pKVListItem->GetString( "name" );
			m_unHeroID = GEconItemSchema().GetHeroID( pszHeroName );
			continue;
		}

		item_list_entry_t entry;

		// First, see if we've got a loot list name, for embedded loot lists
		int iIdx = 0;

		if ( V_strstr( pszName, "unusual" ) != NULL )
		{
			entry.m_bIsUnusualList = true;
		}

		if ( pschema.GetLootListByName( pszName, &iIdx ) )
		{
			entry.m_nItemDef = iIdx;
			entry.m_bIsNestedList = true;
		}
		else
		{
			bool bEntrySuccess = entry.InitFromName( pszName );
			bEntrySuccess;
			
		}

		// Make sure we never put non-enabled items into loot lists
		if ( !entry.m_bIsNestedList && entry.m_nItemDef > 0 )
		{
			const CEconItemDefinition *pItemDef = pschema.GetItemDefinition( entry.m_nItemDef );
			SCHEMA_INIT_CHECK( 
				pItemDef,
				CFmtStr( "Loot list %s: Item definition index \"%s\" (%d) was not found\n", m_pszName, pszName, entry.m_nItemDef ) );
		}

		m_ItemEntries.AddToTail( entry );
		
		float fItemWeight = pKVListItem->GetFloat();
		SCHEMA_INIT_CHECK( 
			fItemWeight > 0.0f,
			CFmtStr( "Loot list %s: Item definition index \"%s\" (%d) has invalid weight %.2f\n", m_pszName, pszName, entry.m_nItemDef, fItemWeight ) );

		m_flWeights.AddToTail( fItemWeight );
		m_flTotalWeight += fItemWeight;
	}

	return SCHEMA_INIT_SUCCESS();
}

bool CEconLootListDefinition::HasUnusualLoot() const
{
	FOR_EACH_VEC(GetLootListContents(), i)
	{
		const item_list_entry_t& entry = GetLootListContents()[i];

		if (entry.m_bIsUnusualList)
			return true;

		if (entry.m_bIsNestedList)
		{
			const CEconLootListDefinition *pNestedLootList = GetItemSchema()->GetLootListByIndex(entry.m_nItemDef);
			if (pNestedLootList)
			{
				if (pNestedLootList->HasUnusualLoot())
					return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CEconLootListDefinition::GetAdditionalDrop( int iIndex, CUtlString& strLootList, float& flChance ) const
{
	if ( !m_AdditionalDrops.IsValidIndex( iIndex ) )
		return false;

	strLootList = m_AdditionalDrops[iIndex].m_pszLootListDefName;
	flChance = m_AdditionalDrops[iIndex].m_fChance;
	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CEconLootListDefinition::GetRandomAttributeGroup( int iIndex, float& flChance, float& flTotalWeight ) const
{
	if ( !m_RandomAttribs.IsValidIndex( iIndex ) )
		return false;

	flChance = m_RandomAttribs[iIndex]->m_flChanceOfRandomAttribute;
	flTotalWeight = m_RandomAttribs[iIndex]->m_flTotalAttributeWeight;

	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CEconLootListDefinition::GetRandomAttributeCount( int iGroup ) const
{
	if ( !m_RandomAttribs.IsValidIndex( iGroup ) )
		return false;

	return m_RandomAttribs[iGroup]->m_RandomAttributes.Count();
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CEconLootListDefinition::GetRandomAttribute( int iGroup, int iIndex, float& flWeight, int& iValue, int& iDefIndex ) const
{
	Assert( !"Tell whoever is working on the item editor that loot list random attributes don't support typed attributes!" );

	if ( !m_RandomAttribs.IsValidIndex( iGroup ) )
		return false;

	if ( !m_RandomAttribs[iGroup]->m_RandomAttributes.IsValidIndex( iIndex ) )
		return false;

	flWeight = m_RandomAttribs[iGroup]->m_RandomAttributes[iIndex].m_flWeight;
	iValue = (int) m_RandomAttribs[iGroup]->m_RandomAttributes[iIndex].m_staticAttrib.m_value.asFloat;
	iDefIndex = m_RandomAttribs[iGroup]->m_RandomAttributes[iIndex].m_staticAttrib.iDefIndex;

	return true;
}

//-----------------------------------------------------------------------------
// Outputs a keyvalues schema representation of this loot list.
//-----------------------------------------------------------------------------
KeyValues* CEconLootListDefinition::GenerateKeyValues() const
{
	KeyValues* pLootListKV = new KeyValues( m_pszName );

	// Write out our items and weights.
	FOR_EACH_VEC( m_ItemEntries, i )
	{
		const CEconItemDefinition* pItemDef = GetItemSchema()->GetItemDefinition( m_ItemEntries[i].m_nItemDef );
		if ( !pItemDef )
			continue;
		pLootListKV->SetFloat( pItemDef->GetRawDefinition()->GetString( "name" ), m_flWeights[i] );
	}

	// Write out additional drops.

	// Write out random attributes.

	return pLootListKV;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CEconLootListDefinition::PurgeItems( void )
{
	m_ItemEntries.Purge();
	m_flWeights.Purge();
	m_flTotalWeight = 0;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconLootListDefinition::lootlist_attrib_t::BInitFromKV( const char *pszContext, KeyValues *pKVKey, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors )
{
	SCHEMA_INIT_SUBSTEP( m_staticAttrib.BInitFromKV_MultiLine( pszContext, pKVKey, pVecErrors ) );

	SCHEMA_INIT_CHECK(
		pKVKey->FindKey( "weight" ),
		CFmtStr( "Loot definition %s: Attribute \"%s\" missing required 'weight' field", pszContext, pKVKey->GetName() ) );

	SCHEMA_INIT_CHECK(
		( !pKVKey->FindKey( "range_min" ) && !pKVKey->FindKey( "range_max" ) ) ||
		( !pKVKey->FindKey( "values" ) ),
		CFmtStr( "Loot definition %s: Attribute \"%s\" specifies both value ranges and explicit values but should only use one method or the other", pszContext, pKVKey->GetName() ) );

	m_flWeight = pKVKey->GetFloat( "weight" );
	m_flRangeMin = pKVKey->GetFloat( "range_min" );
	m_flRangeMax = pKVKey->GetFloat( "range_max" );

	const char* pszValues = pKVKey->GetString( "values" );

	m_vecValues.Purge();

	if ( pszValues && pszValues[ 0 ] )
	{
		SCHEMA_INIT_CHECK( 
			Helper_ExtractIntegersFromValuesString( pszValues, m_vecValues ),
			CFmtStr( "Loot definition %s: Attribute \"%s\" specifies a malformed values range: %s.", pszContext, pKVKey->GetName(), pszValues ) );
	}


	// validate that quest id values are valid
	// TODO: create a common class for indexed definition classes and have this code validate all of them
	if ( !strcmp( pKVKey->GetName(), "quest id" ) )
	{
		if ( m_flRangeMin != 0 || m_flRangeMax != 0 )
		{
			for ( int i = m_flRangeMin; i <= m_flRangeMax; i++ )
			{
				SCHEMA_INIT_CHECK(
					GetItemSchema()->GetQuestDefinition( i ),
					CFmtStr( "Loot definition %s: Attribute \"%s\" specifies a quest id that does not exist: %d.\n", pszContext, pKVKey->GetName(), i ) );
			}
		}
		else if ( m_vecValues.Count() != 0 )
		{
			FOR_EACH_VEC( m_vecValues, i )
			{
				SCHEMA_INIT_CHECK(
					GetItemSchema()->GetQuestDefinition( m_vecValues[ i ] ),
					CFmtStr( "Loot definition %s: Attribute \"%s\" specifies a quest id that does not exist: %d.\n", pszContext, pKVKey->GetName(), m_vecValues[ i ] ) );
			}
		}
	}
	else if ( !strcmp( pKVKey->GetName(), "quest reward lootlist" ) )
	{
		const CEconItemSchema::RevolvingLootListDefinitionMap_t* pQuestRewardLL= &( GetItemSchema()->GetQuestRewardLootLists() );

		if ( m_flRangeMin != 0 || m_flRangeMax != 0 )
		{
			for ( int i = m_flRangeMin; i <= m_flRangeMax; i++ )
			{
				int iLLIndex = pQuestRewardLL->Find( i );

				SCHEMA_INIT_CHECK(
					pQuestRewardLL->IsValidIndex( iLLIndex ),
					CFmtStr( "Loot definition %s: Attribute \"%s\" specifies a quest reward lootlist that does not exist: %d.\n", pszContext, pKVKey->GetName(), i ) );
			}
		}
		else if ( m_vecValues.Count() != 0 )
		{
			FOR_EACH_VEC( m_vecValues, i )
			{
				int iLLIndex = pQuestRewardLL->Find( m_vecValues[ i ] );

				SCHEMA_INIT_CHECK(
					pQuestRewardLL->IsValidIndex( iLLIndex ),
					CFmtStr( "Loot definition %s: Attribute \"%s\" specifies a quest reward lootlist that does not exist: %d.\n", pszContext, pKVKey->GetName(), m_vecValues[ i ] ) );
			}
		}


	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEconCraftingRecipeDefinition::CEconCraftingRecipeDefinition( void )
{
	m_nDefIndex = 0;
	m_wszName[ 0 ] = L'\0';
	m_wszDesc[ 0 ] = L'\0';
}

//-----------------------------------------------------------------------------
// Purpose:	Initialize the attribute definition
// Input:	pKVAttribute - The KeyValues representation of the attribute
//			schema - The overall item schema for this attribute
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconCraftingRecipeDefinition::BInitFromKV( KeyValues *pKVRecipe, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_nDefIndex = Q_atoi( pKVRecipe->GetName() );
	
	// Check for required fields
	SCHEMA_INIT_CHECK( 
		NULL != pKVRecipe->FindKey( "input_items" ), 
		CFmtStr( "Recipe definition %d: Missing required field \"input_items\"", m_nDefIndex ) );

	SCHEMA_INIT_CHECK( 
		NULL != pKVRecipe->FindKey( "output_items" ), 
		CFmtStr( "Recipe definition %d: Missing required field \"output_items\"", m_nDefIndex ) );

	m_bDisabled = pKVRecipe->GetBool( "disabled" );
	m_strName = pKVRecipe->GetString( "name" );	
	m_strN_A = pKVRecipe->GetString( "n_A" );	 
	m_strDescInputs = pKVRecipe->GetString( "desc_inputs" );	
	m_strDescOutputs = pKVRecipe->GetString( "desc_outputs" );	 
	m_strDI_A = pKVRecipe->GetString( "di_A" );	 
	m_strDI_B = pKVRecipe->GetString( "di_B" );	 
	m_strDI_C = pKVRecipe->GetString( "di_C" );	 
	m_strDO_A = pKVRecipe->GetString( "do_A" );	 
	m_strDO_B = pKVRecipe->GetString( "do_B" );	 
	m_strDO_C = pKVRecipe->GetString( "do_C" );	 

	m_bRequiresAllSameClass = pKVRecipe->GetBool( "all_same_class" );
	m_bRequiresAllSameSlot = pKVRecipe->GetBool( "all_same_slot" );
	m_iCacheClassUsageForOutputFromItem = pKVRecipe->GetInt( "add_class_usage_to_output", -1 );
	m_iCacheSlotUsageForOutputFromItem = pKVRecipe->GetInt( "add_slot_usage_to_output", -1 );
	m_iCacheSetForOutputFromItem = pKVRecipe->GetInt( "add_set_to_output", -1 );
	m_bAlwaysKnown = pKVRecipe->GetBool( "always_known", true );
	m_bPremiumAccountOnly = pKVRecipe->GetBool( "premium_only", false );
	m_iCategory = (recipecategories_t)StringFieldToInt( pKVRecipe->GetString("category"), g_szRecipeCategoryStrings, ARRAYSIZE(g_szRecipeCategoryStrings) );
	m_iFilter = pKVRecipe->GetInt( "filter", -1 );

	// Read in all the input items
	KeyValues *pKVInputItems = pKVRecipe->FindKey( "input_items" );
	if ( NULL != pKVInputItems )
	{
		FOR_EACH_TRUE_SUBKEY( pKVInputItems, pKVInputItem )
		{
			int index = m_InputItemsCriteria.AddToTail();
			SCHEMA_INIT_SUBSTEP( m_InputItemsCriteria[index].BInitFromKV( pKVInputItem, pschema ) );

			// Recipes ignore the enabled flag when generating items
			m_InputItemsCriteria[index].SetIgnoreEnabledFlag( true );

			index = m_InputItemDupeCounts.AddToTail();
			m_InputItemDupeCounts[index] = atoi( pKVInputItem->GetName() );
		}
	}

	// Read in all the output items
	KeyValues *pKVOutputItems = pKVRecipe->FindKey( "output_items" );
	if ( NULL != pKVOutputItems )
	{
		FOR_EACH_TRUE_SUBKEY( pKVOutputItems, pKVOutputItem )
		{
			int index = m_OutputItemsCriteria.AddToTail();
			SCHEMA_INIT_SUBSTEP( m_OutputItemsCriteria[index].BInitFromKV( pKVOutputItem, pschema ) );

			// Recipes ignore the enabled flag when generating items
			m_OutputItemsCriteria[index].SetIgnoreEnabledFlag( true );
		}
	}

	GenerateLocStrings();

	return SCHEMA_INIT_SUCCESS();
}

bool CEconCraftingRecipeDefinition::BInitFromSet( const IEconItemSetDefinition *pSet, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_bDisabled = false;
	m_strName = "#RT_T_A";			// "Trade In %s1"
	m_strN_A = pSet->GetLocKey();	// "The Office Collection"
	m_strDescInputs =  "#RDI_AB";	// "Requires: %s1 %s2"
	m_strDescOutputs = "RDO_AB";	// "Produces: %s1 %s2"
	m_strDI_A = "1";				// "1"
	m_strDI_B = pSet->GetLocKey();	// "The Office Collection"
	m_strDI_C = NULL;	 
	m_strDO_A = "1";				// "1"

	const CEconItemDefinition *pItemDef = pschema.GetItemDefinition( pSet->GetCraftReward() );

	SCHEMA_INIT_CHECK( 
		pItemDef && pItemDef->GetDefinitionIndex() > 0,
		CFmtStr( "Item Collection Craft Reward (%d) specifies invalid item def", pSet->GetCraftReward() ) );

	m_strDO_B = pItemDef->GetItemBaseName();	 
	m_strDO_C = NULL;

	m_bRequiresAllSameClass = false;
	m_bRequiresAllSameSlot = false;
	m_iCacheClassUsageForOutputFromItem = -1;
	m_iCacheSlotUsageForOutputFromItem = -1;
	m_iCacheSetForOutputFromItem = -1;
	m_bAlwaysKnown = true;
	m_bPremiumAccountOnly = false;
	m_iCategory = (recipecategories_t)StringFieldToInt( "crafting", g_szRecipeCategoryStrings, ARRAYSIZE(g_szRecipeCategoryStrings) );
	m_iFilter = -1;

	for ( int i = 0; i < pSet->GetItemCount(); ++i )
	{
		int index = m_InputItemsCriteria.AddToTail();
		SCHEMA_INIT_SUBSTEP( m_InputItemsCriteria[index].BInitFromItemAndPaint( pSet->GetItemDef( i ), pSet->GetItemPaintKit( i ), pschema ) );

		m_InputItemsCriteria[index].SetIgnoreEnabledFlag( true );

		index = m_InputItemDupeCounts.AddToTail();
		m_InputItemDupeCounts[index] = 1;
	}

	int index = m_OutputItemsCriteria.AddToTail();
	SCHEMA_INIT_SUBSTEP( m_OutputItemsCriteria[index].BInitFromItemAndPaint( pSet->GetCraftReward(), 0, pschema ) );

	GenerateLocStrings();

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose: Serializes the criteria to and from messages
//-----------------------------------------------------------------------------
bool CEconCraftingRecipeDefinition::BSerializeToMsg( CSOItemRecipe & msg ) const
{
	msg.set_def_index( m_nDefIndex );
	msg.set_name( m_strName );
	msg.set_n_a( m_strN_A );
	msg.set_desc_inputs( m_strDescInputs );
	msg.set_desc_outputs( m_strDescOutputs );
	msg.set_di_a( m_strDI_A );
	msg.set_di_b( m_strDI_B );
	msg.set_di_c( m_strDI_C );
	msg.set_do_a( m_strDO_A );
	msg.set_do_b( m_strDO_B );
	msg.set_do_c( m_strDO_C );
	msg.set_requires_all_same_class( m_bRequiresAllSameClass );
	msg.set_requires_all_same_slot( m_bRequiresAllSameSlot );
	msg.set_class_usage_for_output( m_iCacheClassUsageForOutputFromItem );
	msg.set_slot_usage_for_output( m_iCacheSlotUsageForOutputFromItem );
	msg.set_set_for_output( m_iCacheSetForOutputFromItem );

	FOR_EACH_VEC( m_InputItemsCriteria, i )
	{
		CSOItemCriteria *pCrit = msg.add_input_items_criteria();
		if ( !m_InputItemsCriteria[i].BSerializeToMsg( *pCrit ) )
			return false;
	}

	FOR_EACH_VEC( m_InputItemDupeCounts, i )
	{
		msg.add_input_item_dupe_counts( m_InputItemDupeCounts[i] );
	}

	FOR_EACH_VEC( m_OutputItemsCriteria, i )
	{
		CSOItemCriteria *pCrit = msg.add_output_items_criteria();
		if ( !m_OutputItemsCriteria[i].BSerializeToMsg( *pCrit ) )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Serializes the criteria to and from messages
//-----------------------------------------------------------------------------
bool CEconCraftingRecipeDefinition::BDeserializeFromMsg( const CSOItemRecipe & msg )
{
	m_nDefIndex = msg.def_index();
	m_strName = msg.name().c_str();
	m_strN_A = msg.n_a().c_str();
	m_strDescInputs = msg.desc_inputs().c_str();
	m_strDescOutputs = msg.desc_outputs().c_str();
	m_strDI_A = msg.di_a().c_str();
	m_strDI_B = msg.di_b().c_str();
	m_strDI_C = msg.di_c().c_str();
	m_strDO_A = msg.do_a().c_str();
	m_strDO_B = msg.do_b().c_str();
	m_strDO_C = msg.do_c().c_str();

	m_bRequiresAllSameClass = msg.requires_all_same_class();
	m_bRequiresAllSameSlot = msg.requires_all_same_slot();
	m_iCacheClassUsageForOutputFromItem = msg.class_usage_for_output();
	m_iCacheSlotUsageForOutputFromItem = msg.slot_usage_for_output();
	m_iCacheSetForOutputFromItem = msg.set_for_output();

	// Read how many input items there are
	uint32 unCount = msg.input_items_criteria_size();
	m_InputItemsCriteria.SetSize( unCount );
	for ( uint32 i = 0; i < unCount; i++ )
	{
		if ( !m_InputItemsCriteria[i].BDeserializeFromMsg( msg.input_items_criteria( i ) ) )
			return false;
	}

	// Read how many input item dupe counts there are
	unCount = msg.input_item_dupe_counts_size();
	m_InputItemDupeCounts.SetSize( unCount );
	for ( uint32 i = 0; i < unCount; i++ )
	{
		m_InputItemDupeCounts[i] = msg.input_item_dupe_counts( i );
	}

	// Read how many output items there are
	unCount = msg.output_items_criteria_size();
	m_OutputItemsCriteria.SetSize( unCount );
	for ( uint32 i = 0; i < unCount; i++ )
	{
		if ( !m_OutputItemsCriteria[i].BDeserializeFromMsg( msg.output_items_criteria( i ) ) )
			return false;
	}

	GenerateLocStrings();

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the vector contains a set of items that matches the inputs for this recipe
//			Note it will fail if the vector contains extra items that aren't needed.
//
//-----------------------------------------------------------------------------
bool CEconCraftingRecipeDefinition::ItemListMatchesInputs( const CUtlVector< CEconItem* > &vecCraftingItems, bool bAllowPartialMatch /*= false*/ ) const
{
	bool bResult = true;

	CUtlVector< uint8 > arrDupesUsed;
	arrDupesUsed.SetCount( m_InputItemsCriteria.Count() );

	uint8 *pDupesUsed = arrDupesUsed.Base();
	memset( pDupesUsed, 0, m_InputItemsCriteria.Count() );

	int nSet = -1;

	if ( ( GetFilter() == CRAFT_FILTER_COLLECT || GetFilter() == CRAFT_FILTER_TRADEUP ) && 
		 vecCraftingItems.Count() > 0 )
	{
		nSet = vecCraftingItems[ 0 ]->GetItemSetIndex();

		if ( nSet < 0 )
		{
			// This item must be in a set!
			return false;
		}
	}

	FOR_EACH_VEC( vecCraftingItems, nItem )
	{
		CEconItem *pEconItem = vecCraftingItems[ nItem ];

#if 0	// Relaxing set restriction on crafting
		if ( nSet >= 0 && pEconItem->GetItemSetIndex() != nSet )
		{
			bResult = false;
			break;
		}
#endif

		// Any items at the top rarity of their set are illegal in crafting
		if ( GetFilter() == CRAFT_FILTER_TRADEUP )
		{
			const IEconItemSetDefinition *pSetDef = GetItemSchema()->GetItemSet( vecCraftingItems[ nItem ]->GetItemSetIndex() );
			if ( !pSetDef ) 
			{
				return false;
			}

			bool bHasItemsAtNextRarityTier = false;
			for ( int i = 0; i < pSetDef->GetItemCount(); ++i )
			{
				if ( pSetDef->GetItemRarity( i ) == pEconItem->GetRarity() + 1 )
				{
					bHasItemsAtNextRarityTier = true;
					break;
				}
			}

			if ( !bHasItemsAtNextRarityTier )
				return false;
		}

		bool bFoundCriteriaMatch = false;

		FOR_EACH_VEC( m_InputItemsCriteria, nCriteria )
		{
			const CItemSelectionCriteria *pCriteria = &( m_InputItemsCriteria[ nCriteria ] );
			if ( !pCriteria )
				continue;

			// Skip if we've used this criteria to the limit
			if ( pDupesUsed[ nCriteria ] >= m_InputItemDupeCounts[ nCriteria ] )
				continue;

			bFoundCriteriaMatch = pCriteria->BEvaluate( pEconItem, *GetItemSchema() );
			if ( bFoundCriteriaMatch )
			{
				pDupesUsed[ nCriteria ]++;
				break;
			}
		}

		if ( !bFoundCriteriaMatch )
		{
			// Did the item not fit any criteria!
			bResult = false;
			break;
		}
	}

	if ( !bAllowPartialMatch )
	{
		// Make sure we've matched ALL the criteria
		FOR_EACH_VEC( m_InputItemsCriteria, nCriteria )
		{
			if ( pDupesUsed[ nCriteria ] < m_InputItemDupeCounts[ nCriteria ] )
			{
				// Not all criteria dupes were matched
				bResult = false;
				break;
			}
		}
	}

	return bResult;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
int CEconCraftingRecipeDefinition::GetTotalInputItemsRequired( void ) const
{
	int iCount = 0;
	FOR_EACH_VEC( m_InputItemsCriteria, i )
	{
		if ( m_InputItemDupeCounts[i] )
		{
			iCount += m_InputItemDupeCounts[i];
		}
		else
		{
			iCount++;
		}
	}
	return iCount;
}

#if defined( CLIENT_DLL )
wchar_t *LocalizeRecipeStringPiece( const char *pszString, wchar_t *pszConverted, int nConvertedSizeInBytes ) 
{
	if ( !pszString )
		return L"";

	if ( pszString[0] == '#' )
		return g_pVGuiLocalize->Find( pszString );

	g_pVGuiLocalize->ConvertANSIToUnicode( pszString, pszConverted, nConvertedSizeInBytes );
	return pszConverted;
}
#endif

void CEconCraftingRecipeDefinition::GenerateLocStrings( void )
{
#if defined( CLIENT_DLL )
	wchar_t *pName_A = g_pVGuiLocalize->Find( GetName_A() );
	g_pVGuiLocalize->ConstructString( m_wszName, sizeof( m_wszName ), g_pVGuiLocalize->Find( GetName() ), 1, pName_A );

	wchar_t wcTmpA[32];
	wchar_t wcTmpB[32];
	wchar_t wcTmpC[32];
	wchar_t	wcTmp[512];

	// Build the input string
	wchar_t *pInp_A = LocalizeRecipeStringPiece( GetDescI_A(), wcTmpA, sizeof( wcTmpA ) );
	wchar_t *pInp_B = LocalizeRecipeStringPiece( GetDescI_B(), wcTmpB, sizeof( wcTmpB ) );
	wchar_t *pInp_C = LocalizeRecipeStringPiece( GetDescI_C(), wcTmpC, sizeof( wcTmpC ) );
	g_pVGuiLocalize->ConstructString( m_wszDesc, sizeof( m_wszDesc ), g_pVGuiLocalize->Find( GetDescInputs() ), 3, pInp_A, pInp_B, pInp_C );

	// Build the output string
	wchar_t *pOut_A = LocalizeRecipeStringPiece( GetDescO_A(), wcTmpA, sizeof( wcTmpA ) );
	wchar_t *pOut_B = LocalizeRecipeStringPiece( GetDescO_B(), wcTmpB, sizeof( wcTmpB ) );
	wchar_t *pOut_C = LocalizeRecipeStringPiece( GetDescO_C(), wcTmpC, sizeof( wcTmpC ) );
	g_pVGuiLocalize->ConstructString( wcTmp, sizeof( wcTmp ), g_pVGuiLocalize->Find( GetDescOutputs() ), 3, pOut_A, pOut_B, pOut_C );

	// Concatenate, and mark the text changes
	V_wcscat_safe( m_wszDesc, L"\n" );
	V_wcscat_safe( m_wszDesc, wcTmp );
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
#define GC_SCH_REFERENCE( TAttribSchType )


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
unsigned int Internal_GetAttributeTypeUniqueIdentifierNextValue()
{
	static unsigned int s_unUniqueCounter = 0;

	unsigned int unCounter = s_unUniqueCounter;
	s_unUniqueCounter++;
	return unCounter;
}


template < typename T >
unsigned int GetAttributeTypeUniqueIdentifier()
{
	static unsigned int s_unUniqueCounter = Internal_GetAttributeTypeUniqueIdentifierNextValue();
	return s_unUniqueCounter;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template < GC_SCH_REFERENCE( typename TAttribSchType ) typename TAttribInMemoryType >
class CSchemaAttributeTypeBase : public ISchemaAttributeTypeBase<TAttribInMemoryType>
{
public:
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template < GC_SCH_REFERENCE( typename TAttribSchType ) typename TProtobufValueType >
class CSchemaAttributeTypeProtobufBase : public CSchemaAttributeTypeBase< GC_SCH_REFERENCE( TAttribSchType ) TProtobufValueType >
{
public:
	virtual void ConvertTypedValueToByteStream( const TProtobufValueType& typedValue, ::std::string *out_psBytes ) const OVERRIDE
	{
		DbgVerify( typedValue.SerializeToString( out_psBytes ) );
	}

	virtual void ConvertByteStreamToTypedValue( const ::std::string& sBytes, TProtobufValueType *out_pTypedValue ) const OVERRIDE
	{
		DbgVerify( out_pTypedValue->ParseFromString( sBytes ) );
	}

	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_pValue );
		
		std::string sValue( pszValue );
		TProtobufValueType typedValue;
		if ( !google::protobuf::TextFormat::ParseFromString( sValue, &typedValue ) )
			return false;

		this->ConvertTypedValueToEconAttributeValue( typedValue, out_pValue );
		return true;
	}

	virtual void ConvertEconAttributeValueToString( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, std::string *out_ps ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_ps );

		google::protobuf::TextFormat::PrintToString( this->GetTypedValueContentsFromEconAttributeValue( value ), out_ps );
	}
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSchemaAttributeType_String : public CSchemaAttributeTypeProtobufBase< GC_SCH_REFERENCE( CSchItemAttributeString ) CAttribute_String >
{
public:
	// We intentionally override the convert-to-/convert-from-string functions for strings so that string literals can be
	// specified in the schema, etc. without worrying about the protobuf text format.
	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_pValue );

		CAttribute_String typedValue;
		typedValue.set_value( pszValue );

		this->ConvertTypedValueToEconAttributeValue( typedValue, out_pValue );

		return true;
	}

	virtual void ConvertEconAttributeValueToString( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, std::string *out_ps ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_ps );

		*out_ps = this->GetTypedValueContentsFromEconAttributeValue( value ).value().c_str();
	}
};

void CopyStringAttributeValueToCharPointerOutput( const CAttribute_String *pValue, const char **out_pValue )
{
	Assert( pValue );
	Assert( out_pValue );

	*out_pValue = pValue->value().c_str();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSchemaAttributeType_Vector : public CSchemaAttributeTypeBase< GC_SCH_REFERENCE( CSchItemAttributeVector ) Vector >
{
public:
	// We intentionally override the convert-to-/convert-from-string functions for strings so that string literals can be
	// specified in the schema, etc. without worrying about the protobuf text format.
	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_pValue );

		Vector typedValue;
		if ( sscanf( pszValue, "%f %f %f", &typedValue.x, &typedValue.y, &typedValue.z ) < 3 )
		{
			AssertMsg1( false, "Vector attribute value has invalid string: %s", pszValue );
		}

		this->ConvertTypedValueToEconAttributeValue( typedValue, out_pValue );

		return true;
	}

	virtual void ConvertEconAttributeValueToString( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, std::string *out_ps ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_ps );

		Vector typedValue;
		this->ConvertEconAttributeValueToTypedValue( value, &typedValue );

		char szTemp[ 256 ];
		V_sprintf_safe( szTemp, "%f %f %f", typedValue[ 0 ], typedValue[ 1 ], typedValue[ 2 ] );

		*out_ps = szTemp;
	}

	virtual void ConvertTypedValueToByteStream( const Vector& typedValue, ::std::string *out_psBytes ) const OVERRIDE
	{
		Assert( out_psBytes );
		Assert( out_psBytes->size() == 0 );

		out_psBytes->resize( sizeof( Vector ) );
		*reinterpret_cast<Vector *>( &((*out_psBytes)[0]) ) = typedValue;
	}

	virtual void ConvertByteStreamToTypedValue( const ::std::string& sBytes, Vector *out_pTypedValue ) const OVERRIDE
	{
		Assert( out_pTypedValue );
		Assert( sBytes.size() == sizeof( Vector ) );

		*out_pTypedValue = *reinterpret_cast<const Vector *>( &sBytes[0] );
	}
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSchemaAttributeType_Uint32 : public CSchemaAttributeTypeBase< GC_SCH_REFERENCE( CSchItemAttributeUint32 ) uint32 >
{
public:
	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_pValue );

		out_pValue->asUint32 = Q_atoi( pszValue );
		return true;
	}

	virtual void ConvertEconAttributeValueToString( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, std::string *out_ps ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_ps );

		*out_ps = CFmtStr( "%u", value.asUint32 ).Get();
	}

	virtual void ConvertTypedValueToByteStream( const uint32& typedValue, ::std::string *out_psBytes ) const OVERRIDE
	{
		Assert( out_psBytes );
		Assert( out_psBytes->size() == 0 );

		out_psBytes->resize( sizeof( uint32 ) );
		*reinterpret_cast<uint32 *>( &((*out_psBytes)[0]) ) = typedValue;
	}

	virtual void ConvertByteStreamToTypedValue( const ::std::string& sBytes, uint32 *out_pTypedValue ) const OVERRIDE
	{
		Assert( out_pTypedValue );
		Assert( sBytes.size() == sizeof( uint32 ) );

		*out_pTypedValue = *reinterpret_cast<const uint32 *>( &sBytes[0] );
	}

	virtual bool BSupportsGameplayModificationAndNetworking() const OVERRIDE
	{
		return true;
	}
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSchemaAttributeType_Float : public CSchemaAttributeTypeBase< GC_SCH_REFERENCE( CSchItemAttributeFloat ) float >
{
public:
	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_pValue );

		out_pValue->asFloat = Q_atof( pszValue );
		return true;
	}

	virtual void ConvertEconAttributeValueToString( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, std::string *out_ps ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_ps );

		*out_ps = CFmtStr( "%f", value.asFloat ).Get();
	}

	virtual void ConvertTypedValueToByteStream( const float& typedValue, ::std::string *out_psBytes ) const OVERRIDE
	{
		Assert( out_psBytes );
		Assert( out_psBytes->size() == 0 );

		out_psBytes->resize( sizeof( float ) );
		*reinterpret_cast<float *>( &((*out_psBytes)[0]) ) = typedValue;		// overwrite string contents (sizeof( float ) bytes)
	}

	virtual void ConvertByteStreamToTypedValue( const ::std::string& sBytes, float *out_pTypedValue ) const OVERRIDE
	{
		Assert( out_pTypedValue );
		Assert( sBytes.size() == sizeof( float ) );

		*out_pTypedValue = *reinterpret_cast<const float *>( &sBytes[0] );
	}

	virtual bool BSupportsGameplayModificationAndNetworking() const OVERRIDE
	{
		return true;
	}

	virtual bool OnIterateAttributeValue( IEconItemAttributeIterator *pIterator, const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value ) const OVERRIDE
	{
		Assert( pIterator );
		Assert( pAttrDef );

		// Call the appropriate virtual function on our iterator based on whatever type we represent.
		return pIterator->OnIterateAttributeValue( pAttrDef, value.asFloat );
	}
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CSchemaAttributeType_Default : public CSchemaAttributeTypeBase< GC_SCH_REFERENCE( CSchItemAttribute ) attrib_value_t >
{
public:
	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_pValue );

		// This is terrible backwards-compatibility code to support the pulling of values from econ asset classes.
		if ( pAttrDef->IsStoredAsInteger() )
		{
			out_pValue->asUint32 = pszValue ? (uint32)V_atoui64( pszValue ) : 0;
		}
		else if ( pAttrDef->IsStoredAsFloat() )
		{
			out_pValue->asFloat = pszValue ? V_atof( pszValue ) : 0.0f;
		}
		else
		{
			Assert( !"Unknown storage type for CSchemaAttributeType_Default::BConvertStringToEconAttributeValue()!" );
			return false;
		}

		return true;
	}

	virtual void ConvertEconAttributeValueToString( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, std::string *out_ps ) const OVERRIDE
	{
		Assert( pAttrDef );
		Assert( out_ps );

		if( pAttrDef->IsStoredAsFloat() )
		{
			*out_ps = CFmtStr( "%f", value.asFloat ).Get();
		}
		else if( pAttrDef->IsStoredAsInteger() )
		{
			*out_ps = CFmtStr( "%u", value.asUint32 ).Get();
		}
		else
		{
			Assert( !"Unknown storage type for CSchemaAttributeType_Default::ConvertEconAttributeValueToString()!" );
		}
	}
	
	virtual void ConvertTypedValueToByteStream( const attrib_value_t& typedValue, ::std::string *out_psBytes ) const OVERRIDE
	{
		Assert( out_psBytes );
		Assert( out_psBytes->size() == 0 );

		out_psBytes->resize( sizeof( attrib_value_t ) );
		*reinterpret_cast<attrib_value_t *>( &((*out_psBytes)[0]) ) = typedValue;		// overwrite string contents (sizeof( attrib_value_t ) bytes)
	}

	virtual void ConvertByteStreamToTypedValue( const ::std::string& sBytes, attrib_value_t *out_pTypedValue ) const OVERRIDE
	{
		Assert( out_pTypedValue );
		// Game clients and servers may have partially out-of-date information, or may have downloaded a new schema
		// but not know how to parse an attribute of a certain type, etc. In these cases, because we know we
		// aren't on the GC, temporarily failing to load these values until the client shuts down and updates
		// is about the best we can hope for.
		if ( sBytes.size() < sizeof( attrib_value_t ) )
		{
			*out_pTypedValue = attrib_value_t();
			return;
		}

		*out_pTypedValue = *reinterpret_cast<const attrib_value_t *>( &sBytes[0] );
	}

	virtual bool BSupportsGameplayModificationAndNetworking() const OVERRIDE
	{
		return true;
	}

private:
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEconItemAttributeDefinition::CEconItemAttributeDefinition( void )
:	m_pKVAttribute( NULL ),
	m_pAttrType( NULL ),
	m_bHidden( false ),
	m_bWebSchemaOutputForced( false ),
	m_bStoredAsInteger( false ),
	m_iEffectType( ATTRIB_EFFECT_NEUTRAL ),
	m_iDescriptionFormat( 0 ),
	m_pszDescriptionString( NULL ),
	m_pszDescriptionTag( NULL ),
	m_pszArmoryDesc( NULL ),
	m_iScore( 0 ),
	m_pszDefinitionName( NULL ),
	m_pszAttributeClass( NULL )
#ifndef GC_DLL
  , m_iszAttributeClass( NULL_STRING )
#endif
{
}


//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CEconItemAttributeDefinition::CEconItemAttributeDefinition( const CEconItemAttributeDefinition &that )
{
	(*this) = that;
}


//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconItemAttributeDefinition &CEconItemAttributeDefinition::operator=( const CEconItemAttributeDefinition &rhs )
{
	m_nDefIndex = rhs.m_nDefIndex;
	m_pAttrType = rhs.m_pAttrType;
	m_bHidden = rhs.m_bHidden;
	m_bWebSchemaOutputForced = rhs.m_bWebSchemaOutputForced;
	m_bStoredAsInteger = rhs.m_bStoredAsInteger;
	m_bInstanceData = rhs.m_bInstanceData;
	m_eAssetClassAttrExportRule = rhs.m_eAssetClassAttrExportRule;
	m_iEffectType = rhs.m_iEffectType;
	m_iDescriptionFormat = rhs.m_iDescriptionFormat;
	m_pszDescriptionString = rhs.m_pszDescriptionString;
	m_pszDescriptionTag = rhs.m_pszDescriptionTag;
	m_pszArmoryDesc = rhs.m_pszArmoryDesc;
	m_iScore = rhs.m_iScore;
	m_pszDefinitionName = rhs.m_pszDefinitionName;
	m_pszAttributeClass = rhs.m_pszAttributeClass;
	m_unAssetClassBucket = rhs.m_unAssetClassBucket;
#ifndef GC_DLL
	m_iszAttributeClass = rhs.m_iszAttributeClass;
#endif

	m_pKVAttribute = NULL;
	if ( NULL != rhs.m_pKVAttribute )
	{
		m_pKVAttribute = rhs.m_pKVAttribute->MakeCopy();

		// Re-assign string pointers
		m_pszDefinitionName = m_pKVAttribute->GetString("name");
		m_pszDescriptionString = m_pKVAttribute->GetString( "description_string", NULL );
		m_pszDescriptionTag = m_pKVAttribute->GetString( "description_tag", NULL );

		m_pszArmoryDesc = m_pKVAttribute->GetString( "armory_desc", NULL );
		m_pszAttributeClass = m_pKVAttribute->GetString( "attribute_class", NULL );

		Assert( V_strcmp( m_pszDefinitionName, rhs.m_pszDefinitionName ) == 0 );
		Assert( V_strcmp( m_pszDescriptionString, rhs.m_pszDescriptionString ) == 0 );
		Assert( V_strcmp( m_pszDescriptionTag, rhs.m_pszDescriptionTag ) == 0 );
		Assert( V_strcmp( m_pszArmoryDesc, rhs.m_pszArmoryDesc ) == 0 );
		Assert( V_strcmp( m_pszAttributeClass, rhs.m_pszAttributeClass ) == 0 );
	}
	else
	{
		Assert( m_pszDefinitionName == NULL );
		Assert( m_pszDescriptionString == NULL );
		Assert( m_pszArmoryDesc == NULL );
		Assert( m_pszAttributeClass == NULL );
	}
	return *this;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CEconItemAttributeDefinition::~CEconItemAttributeDefinition( void )
{
	if ( m_pKVAttribute )
		m_pKVAttribute->deleteThis();
	m_pKVAttribute = NULL;
}


//-----------------------------------------------------------------------------
// Purpose:	Initialize the attribute definition
// Input:	pKVAttribute - The KeyValues representation of the attribute
//			schema - The overall item schema for this attribute
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemAttributeDefinition::BInitFromKV( KeyValues *pKVAttribute, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_pKVAttribute = pKVAttribute->MakeCopy();
	m_nDefIndex = Q_atoi( m_pKVAttribute->GetName() );
	
	m_pszDefinitionName = m_pKVAttribute->GetString("name");
	m_bHidden = m_pKVAttribute->GetInt( "hidden", 0 ) != 0;
	m_bWebSchemaOutputForced = m_pKVAttribute->GetInt( "force_output_description", 0 ) != 0;
	m_bStoredAsInteger = m_pKVAttribute->GetInt( "stored_as_integer", 0 ) != 0;
	m_iScore = m_pKVAttribute->GetInt( "score", 0 );
	m_iEffectType = (attrib_effect_types_t)StringFieldToInt( m_pKVAttribute->GetString("effect_type"), g_EffectTypes, ARRAYSIZE(g_EffectTypes) );
	m_iDescriptionFormat = StringFieldToInt( m_pKVAttribute->GetString("description_format"), g_AttributeDescriptionFormats, ARRAYSIZE(g_AttributeDescriptionFormats) );
	m_pszDescriptionString = m_pKVAttribute->GetString( "description_string", NULL );
	m_pszDescriptionTag = m_pKVAttribute->GetString( "description_tag", NULL );
	m_pszArmoryDesc = m_pKVAttribute->GetString( "armory_desc", NULL );
	m_pszAttributeClass = m_pKVAttribute->GetString( "attribute_class", NULL );
	m_bInstanceData = pKVAttribute->GetBool( "instance_data", false );

	const char *pszAttrType = m_pKVAttribute->GetString( "attribute_type", NULL );		// NULL implies "default type" for backwards compatibility
	m_pAttrType = pschema.GetAttributeType( pszAttrType );
	SCHEMA_INIT_CHECK(
		NULL != m_pAttrType,
		CFmtStr( "Attribute definition %s: Unable to find attribute data type '%s'", m_pszDefinitionName, pszAttrType ? pszAttrType : "(default)" ) );

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	m_iszAttributeClass = NULL_STRING;
#endif

	// Check for required fields
	SCHEMA_INIT_CHECK( 
		NULL != m_pKVAttribute->FindKey( "name" ), 
		CFmtStr( "Attribute definition %s: Missing required field \"name\"", m_pKVAttribute->GetName() ) );

	m_unAssetClassBucket = pKVAttribute->GetInt( "asset_class_bucket", 0 );
	m_eAssetClassAttrExportRule = k_EAssetClassAttrExportRule_Default;
	if ( char const *szRule = pKVAttribute->GetString( "asset_class_export", NULL ) )
	{
		if ( !V_stricmp( szRule, "skip" ) )
		{
			m_eAssetClassAttrExportRule = k_EAssetClassAttrExportRule_Skip;
		}
		else if ( !V_stricmp( szRule, "gconly" ) )
		{
			m_eAssetClassAttrExportRule = EAssetClassAttrExportRule_t( k_EAssetClassAttrExportRule_GCOnly | k_EAssetClassAttrExportRule_Skip );
		}
		else if ( !V_stricmp( szRule, "bucketed" ) )
		{
			SCHEMA_INIT_CHECK(
				m_unAssetClassBucket,
				CFmtStr( "Attribute definition %s: Asset class export rule '%s' is incompatible", m_pszDefinitionName, szRule ) );
			m_eAssetClassAttrExportRule = k_EAssetClassAttrExportRule_Bucketed;
		}
		else if ( !V_stricmp( szRule, "default" ) )
		{
			m_eAssetClassAttrExportRule = k_EAssetClassAttrExportRule_Default;
		}
		else
		{
			SCHEMA_INIT_CHECK(
				false,
				CFmtStr( "Attribute definition %s: Invalid asset class export rule '%s'", m_pszDefinitionName, szRule ) );
		}
	}

	// Check for misuse of asset class bucket
	SCHEMA_INIT_CHECK(
		( !m_unAssetClassBucket || m_bInstanceData ),
		CFmtStr( "Attribute definition %s: Cannot use \"asset_class_bucket\" on class-level attributes", m_pKVAttribute->GetName() )
		);

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CEconSoundMaterialDefinition::CEconSoundMaterialDefinition( void )
	:	m_nID( INT_MAX )
{
}


//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CEconSoundMaterialDefinition::CEconSoundMaterialDefinition( const CEconSoundMaterialDefinition &that )
{
	(*this) = that;
}


//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconSoundMaterialDefinition &CEconSoundMaterialDefinition::operator=( const CEconSoundMaterialDefinition &rhs )
{
	m_nID = rhs.m_nID; 
	m_strName =	rhs.m_strName; 
	m_strStartDragSound = rhs.m_strStartDragSound;
	m_strEndDragSound = rhs.m_strEndDragSound;
	m_strEquipSound = rhs.m_strEquipSound;

	return *this;
}


//-----------------------------------------------------------------------------
// Purpose:	Initialize the rarity definition
//-----------------------------------------------------------------------------
bool CEconSoundMaterialDefinition::BInitFromKV( KeyValues *pKVSoundMaterial, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	m_nID = pKVSoundMaterial->GetInt( "value", -1 );
	m_strName = pKVSoundMaterial->GetName();
	m_strStartDragSound = pKVSoundMaterial->GetString( "start_drag_sound" );
	m_strEndDragSound = pKVSoundMaterial->GetString( "end_drag_sound" );
	m_strEquipSound = pKVSoundMaterial->GetString( "equip_sound" );

	// Check for required fields
	SCHEMA_INIT_CHECK( 
		NULL != pKVSoundMaterial->FindKey( "value" ), 
		CFmtStr( "Sound material definition %s: Missing required field \"value\"", pKVSoundMaterial->GetName() ) );

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	return SCHEMA_INIT_SUCCESS();
#endif // GC_DLL

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Constructor
//-----------------------------------------------------------------------------
CEconItemDefinition::CEconItemDefinition( void )
:	m_pKVItem( NULL ),
m_bEnabled( false ),
m_unMinItemLevel( 0 ),
m_unMaxItemLevel( 100 ),
m_iArmoryRemap( 0 ),
m_iStoreRemap( 0 ),
m_nItemRarity( k_unItemRarity_Any ),
m_nItemQuality( k_unItemQuality_Any ),
m_nForcedItemQuality( k_unItemQuality_Any ),
m_nDefaultDropItemQuality( k_unItemQuality_Any ),
m_nDefaultDropQuantity( 1 ),
m_pProxyCriteria( NULL ),
m_bLoadOnDemand( false ),
m_pTool( NULL ),
m_rtExpiration( 0 ),
m_rtDefCreation( 0 ),
m_BundleInfo( NULL ),
m_unNumConcreteItems( 0 ),
m_nSoundMaterialID( 0 ),
m_nPopularitySeed( 0 ),
m_pszDefinitionName( NULL ),
m_pszItemClassname( NULL ),
m_pszClassToken( NULL ),
m_pszSlotToken( NULL ),
m_pszItemBaseName( NULL ),
m_pszItemTypeName( NULL ),
m_unItemTypeID( 0 ),
m_pszItemDesc( NULL ),
m_pszArmoryDesc( NULL ),
m_pszInventoryModel( NULL ),
m_pszInventoryImage( NULL ),
m_pszHolidayRestriction( NULL ),
m_iSubType( 0 ),
m_pszBaseDisplayModel( NULL ),
m_pszWorldDisplayModel( NULL ),
m_pszWorldDroppedModel( NULL ),
m_pszWorldExtraWearableModel( NULL ),
m_pszIconDefaultImage( NULL ),
m_pszParticleFile( NULL ),
m_pszParticleSnapshotFile( NULL ),
m_pInventoryImageData( NULL ),
m_pszBrassModelOverride( NULL ),
m_bHideBodyGroupsDeployedOnly( false ),
m_bAttachToHands( false ),
m_bAttachToHandsVMOnly( false ),
m_bProperName( false ),
m_bFlipViewModel( false ),
m_bActAsWearable( false ),
m_iDropType( 1 ),
m_bHidden( false ),
m_bShouldShowInArmory( false ),
m_bIsPackBundle( false ),
m_pOwningPackBundle( NULL ),
m_bBaseItem( false ),
m_pszItemLogClassname( NULL ),
m_pszItemIconClassname( NULL ),
#if ECONITEM_DATABASE_AUDIT_TABLES_FEATURE
m_pszDatabaseAuditTable( NULL ),
#endif
m_bImported( false ),
m_bOnePerAccountCDKEY( false ),
m_pszArmoryRemap( NULL ),
m_pszStoreRemap( NULL ),
m_bPublicItem( false ),
m_bIgnoreInCollectionView( false ),
m_nAssociatedItemsDefIndexes( NULL ),
m_pPortraitsKV( NULL ),
m_pAssetInfo( NULL ),
m_eItemType( k_EItemTypeNone ),
m_bAllowPurchaseStandalone( false )
{
	m_pMapAlternateIcons = new CUtlMap<uint32, const char*>;
	m_pMapAlternateIcons->SetLessFunc( DefLessFunc(uint32) );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CEconItemDefinition::~CEconItemDefinition( void )
{
	if ( m_pKVItem )
		m_pKVItem->deleteThis();
	m_pKVItem = NULL;
	delete m_pProxyCriteria;
	delete m_pTool;
	delete m_BundleInfo;
	delete m_pMapAlternateIcons;

	if ( m_pInventoryImageData )
	{
		for ( int i = 0; i < MATERIAL_MAX_LIGHT_COUNT; i++ )
		{
			delete m_pInventoryImageData->m_pLightDesc[ i ];
		}
		delete m_pInventoryImageData->m_pCameraOffset;
		delete m_pInventoryImageData->m_pCameraAngles;
		delete m_pInventoryImageData;
	}

	PurgeStaticAttributes();
}

void CEconItemDefinition::PurgeStaticAttributes() 
{
	FOR_EACH_VEC( m_vecStaticAttributes, i )
	{
		static_attrib_t *pAttrib = &(m_vecStaticAttributes[ i ]);

		const CEconItemAttributeDefinition *pAttrDef = pAttrib->GetAttributeDefinition();
		Assert( pAttrDef );

		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		// Free up our temp loading memory.
		pAttrType->UnloadEconAttributeValue( &pAttrib->m_value );
	}

	m_vecStaticAttributes.Purge();
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose: Stomp our base data with extra testing data specified by the player
//-----------------------------------------------------------------------------
bool CEconItemDefinition::BInitFromTestItemKVs( int iNewDefIndex, KeyValues *pKVItem, CEconItemSchema &pschema )
{
	// The KeyValues are stored in the player entity, so we can cache our name there

	m_nDefIndex = iNewDefIndex;

	bool bTestingExistingItem = pKVItem->GetInt( "test_existing_item", 0 ) != 0;
	if ( !bTestingExistingItem )
	{
		m_pszDefinitionName = pKVItem->GetString( "name", NULL );
		m_pszItemBaseName = pKVItem->GetString( "name", NULL );

#ifdef CLIENT_DLL
		pKVItem->SetString( "name", VarArgs("Test Item %d", iNewDefIndex) );
#else
		pKVItem->SetString( "name", UTIL_VarArgs("Test Item %d", iNewDefIndex) );
#endif

		m_pszBaseDisplayModel = pKVItem->GetString( "model_player", NULL );
		m_bAttachToHands = pKVItem->GetInt( "attach_to_hands", 0 ) != 0;

		BInitVisualBlockFromKV( pKVItem, pschema );

		m_pszParticleFile = pKVItem->GetString( "particle_file", NULL );
		m_pszParticleSnapshotFile = pKVItem->GetString( "particle_snapshot", NULL );
	}

	// Handle attributes
	PurgeStaticAttributes();

	int iPaintCanIndex = pKVItem->GetInt("paintcan_index", 0);
	if ( iPaintCanIndex )
	{
		static CSchemaAttributeDefHandle pAttrDef_PaintRGB( "set item tint RGB" );

		const CEconItemDefinition *pCanDef = pschema.GetItemDefinition(iPaintCanIndex);

		float flRGBVal;
		if ( pCanDef && pAttrDef_PaintRGB && FindAttribute_UnsafeBitwiseCast<attrib_value_t>( pCanDef, pAttrDef_PaintRGB, &flRGBVal ) )
		{
			static_attrib_t& StaticAttrib = m_vecStaticAttributes[ m_vecStaticAttributes.AddToTail() ];

			StaticAttrib.iDefIndex = pAttrDef_PaintRGB->GetDefinitionIndex();
			StaticAttrib.m_value.asFloat = flRGBVal;							// this is bad! but we're in crazy hack code for UI customization of item definitions that don't exist so
		}
	}

	return true;
}
#endif

void AssetInfo::AddAssetModifier( AssetModifier* newMod )
{
	int idx = m_mapAssetModifiers.Find( newMod->m_Type );
	if ( !m_mapAssetModifiers.IsValidIndex( idx ) )
	{
		idx = m_mapAssetModifiers.Insert( newMod->m_Type, new CUtlVector<AssetModifier*> );
	}	
	m_mapAssetModifiers[idx]->AddToTail( newMod );
}

CUtlVector<AssetModifier*>* AssetInfo::GetAssetModifiers( EAssetModifier type )
{
	int idx = m_mapAssetModifiers.Find( type );
	if ( m_mapAssetModifiers.IsValidIndex( idx ) )
	{
		return m_mapAssetModifiers[idx];
	}
	else
	{
		return NULL;
	}
}

const char* AssetInfo::GetModifierByAsset( EAssetModifier type, const char* pszAsset, int iStyle )
{
	CUtlVector<AssetModifier*>* pAssetModifierList = GetAssetModifiers( type );
	if ( pAssetModifierList )
	{
		if ( iStyle > -1 )
		{
			// See if a modifier matches this style.
			FOR_EACH_VEC( *pAssetModifierList, i )
			{
				AssetModifier* pMod = pAssetModifierList->Element(i);
				if ( pMod->m_strAsset == pszAsset && pMod->m_iStyle == iStyle )
					return pMod->m_strModifier;
			}
		}

		// Return the first match.
		FOR_EACH_VEC( *pAssetModifierList, i )
		{
			AssetModifier* pMod = pAssetModifierList->Element(i);
			if ( pMod->m_strAsset == pszAsset )
				return pMod->m_strModifier;
		}
	}

	return NULL;
}

const char* AssetInfo::GetAssetByModifier( EAssetModifier type, const char* pszModifier, int iStyle )
{
	CUtlVector<AssetModifier*>* pAssetModifierList = GetAssetModifiers( type );
	if ( pAssetModifierList )
	{
		if ( iStyle > -1 )
		{
			// See if a modifier matches this style.
			FOR_EACH_VEC( *pAssetModifierList, i )
			{
				AssetModifier* pMod = pAssetModifierList->Element(i);
				if ( pMod->m_strModifier == pszModifier && pMod->m_iStyle == iStyle )
					return pMod->m_strAsset;
			}
		}

		// Return the first match.
		FOR_EACH_VEC( *pAssetModifierList, i )
		{
			AssetModifier* pMod = pAssetModifierList->Element(i);
			if ( pMod->m_strModifier == pszModifier )
				return pMod->m_strAsset;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Handle parsing the per-team visual block from the keyvalues
//-----------------------------------------------------------------------------
void CEconItemDefinition::BInitVisualBlockFromKV( KeyValues *pKVItem, IEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors )
{
	// Visuals
	m_pAssetInfo = NULL;
	KeyValues *pVisualsKV = pKVItem->FindKey( "visuals" );
	if ( pVisualsKV )
	{
		AssetInfo *pVisData = new AssetInfo();

#if defined(CLIENT_DLL) || defined(GAME_DLL)
		KeyValues *pKVEntry = pVisualsKV->GetFirstSubKey();
		while ( pKVEntry )
		{
			const char *pszEntry = pKVEntry->GetName();

			if ( !Q_stricmp( pszEntry, "attached_models" ) )
			{
				FOR_EACH_SUBKEY( pKVEntry, pKVAttachedModelData )
				{
					int iAtt = pVisData->m_AttachedModels.AddToTail();
					pVisData->m_AttachedModels[iAtt].m_iModelDisplayFlags = pKVAttachedModelData->GetInt( "model_display_flags", kAttachedModelDisplayFlag_MaskAll );
					pVisData->m_AttachedModels[iAtt].m_pszModelName = pKVAttachedModelData->GetString( "model", NULL );
				}
			}
			else if ( StringHasPrefix( pszEntry, "attached_particlesystem" ) )
			{
				// This replaces TF's "attached_particlesystem", "custom_particlesystem", and "custom_particlesystem2"
				// It now indexes into the attributecontrolledparticlesystems list (use the "custom_type" field if you want a custom attribute)
				int iParticleIndex;
				if ( pschema.FindAttributeControlledParticleSystem( pKVEntry->GetString( "system", NULL ), &iParticleIndex ) )
				{
					attachedparticle_t attachedParticle;
					attachedParticle.m_iParticleIndex = iParticleIndex;
					attachedParticle.m_nStyle = pKVEntry->GetInt( "style",  0 );

					pVisData->m_AttachedParticles.AddToTail( attachedParticle );
				}
			}
			else if ( StringHasPrefix( pszEntry, "asset_modifier" ) )
			{
				AssetModifier* newMod = new AssetModifier();
				newMod->m_Type = GetAssetModifierType( pKVEntry->GetString( "type", NULL ) );
				newMod->m_strModifier = pKVEntry->GetString( "modifier", NULL );
				newMod->m_flModifier = pKVEntry->GetFloat( "modifier", 0.f );
				newMod->m_strAsset = pKVEntry->GetString( "asset", NULL );
				newMod->m_iStyle = pKVEntry->GetInt( "style", 0 );
				pVisData->AddAssetModifier( newMod );

				// Items can have any number of asset modifiers.
				// The variables can be used in any way, based on type, but the format is all the same to simplify the editor.
				const char* pszAssetModifierType = pKVEntry->GetString( "type", NULL );

				const char* pszAssetName = pKVEntry->GetString( "asset", NULL );
				const char* pszModifierName = pKVEntry->GetString( "modifier", NULL );
				float flFrequency = pKVEntry->GetFloat( "frequency", 1.0 );

				if ( FStrEq( pszAssetModifierType, "activity" ) )
				{
					int iAtt = pVisData->m_Animations.AddToTail();
					pVisData->m_Animations[iAtt].iActivity = -2;
					pVisData->m_Animations[iAtt].pszActivity = pszAssetName;
					pVisData->m_Animations[iAtt].iReplacement = kActivityLookup_Unknown;
					pVisData->m_Animations[iAtt].pszReplacement = pszModifierName;
					pVisData->m_Animations[iAtt].flFrequency = flFrequency;
				}
				else if ( FStrEq( pszAssetModifierType, "announcer" ) )
				{
					pVisData->m_pszAnnouncerName = pszAssetName;
					pVisData->m_pszAnnouncerResource = pszModifierName;
				}
				else if ( FStrEq( pszAssetModifierType, "announcer_preview" ) )
				{
					CUtlString strFilename = pszAssetName;

					announcer_preview_t preview;

					preview.m_strFileName = pszAssetName;
					preview.m_strCaption = pszModifierName;

					pVisData->m_vecAnnouncerPreview.AddToTail( preview );
				}
				else if ( FStrEq( pszAssetModifierType, "hud_skin" ) )
				{
					pVisData->m_pszHudSkinName = pszAssetName;
				}
				else if ( FStrEq( pszAssetModifierType, "ability_name" ) )
				{
					pVisData->m_pszAbilityName = pszAssetName;
				}
				else if ( FStrEq( pszAssetModifierType, "sound" ) )
				{
					int iSound = pVisData->m_Sounds.AddToTail();
					pVisData->m_Sounds[iSound].pszSound = pszAssetName;
					pVisData->m_Sounds[iSound].pszReplacement = pszModifierName;
				}
				else if ( FStrEq( pszAssetModifierType, "speech" ) )
				{
					pVisData->m_pszSpeechConcept = pszAssetName;
					pVisData->m_pszChatMessage = pszModifierName;
				}
				else if ( FStrEq( pszAssetModifierType, "particle" ) )
				{
					int iParticle = pVisData->m_Particles.AddToTail();
					pVisData->m_Particles[iParticle].pszParticle = pszAssetName;
					pVisData->m_Particles[iParticle].pszReplacement = pszModifierName;
				}
				else if ( FStrEq( pszAssetModifierType, "particle_snapshot" ) )
				{
					int iParticle = pVisData->m_ParticleSnapshots.AddToTail();
					pVisData->m_ParticleSnapshots[iParticle].pszParticleSnapshot = pszAssetName;
					pVisData->m_ParticleSnapshots[iParticle].pszReplacement = pszModifierName;
				}
				else if ( FStrEq( pszAssetModifierType, "particle_control_point" ) )
				{
					int iParticleCP = pVisData->m_ParticleControlPoints.AddToTail();
					pVisData->m_ParticleControlPoints[iParticleCP].pszParticle = pszAssetName;
					pVisData->m_ParticleControlPoints[iParticleCP].nParticleControlPoint = pKVEntry->GetInt( "control_point_number", 0 );
					UTIL_StringToVector( pVisData->m_ParticleControlPoints[iParticleCP].vecCPValue.Base(), pKVEntry->GetString( "cp_position", "0 0 0" ) );
				}
				else if ( FStrEq( pszAssetModifierType, "entity_model" ) )
				{
					pVisData->m_pszEntityClass = pszAssetName;
					pVisData->m_pszEntityModel = pszModifierName;
				}
				else if ( FStrEq( pszAssetModifierType, "view_model" ) )
				{
					pVisData->m_pszEntityClass = pszAssetName;
					pVisData->m_pszViewModel = pszModifierName;
				}
				else if ( FStrEq( pszAssetModifierType, "icon_replacement" ) )
				{
					pVisData->m_pszOriginalIcon = pszAssetName;
					pVisData->m_pszNewIcon = pszModifierName;
				}
				else if ( FStrEq( pszAssetModifierType, "ability_icon_replacement" ) )
				{						
					ability_icon_replacement_t iconReplacement;

					iconReplacement.m_strAbilityName = pszAssetName;
					iconReplacement.m_strReplacement = pszModifierName;

					pVisData->m_vecAbilityIconReplacements.AddToTail( iconReplacement );
				}
				else if ( FStrEq( pszAssetModifierType, "entity_scale" ) )
				{
					pVisData->m_pszScaleClass = pszAssetName;
					pVisData->m_flScaleSize = pKVEntry->GetFloat( "modifier", 1.f );
				}
			}
			else if ( !Q_stricmp( pszEntry, "animation" ) )
			{
				int iAtt = pVisData->m_Animations.AddToTail();
				pVisData->m_Animations[iAtt].iActivity = -2;	// We can't look it up yet, the activity list hasn't been populated.
				pVisData->m_Animations[iAtt].pszActivity = pKVEntry->GetString( "activity", NULL );

				const char* playback = pKVEntry->GetString( "playback" );
				if ( playback )
				{
					pVisData->m_Animations[iAtt].iPlayback = (wearableanimplayback_t)StringFieldToInt( pKVEntry->GetString("playback"), g_WearableAnimTypeStrings, ARRAYSIZE(g_WearableAnimTypeStrings) );
				}

				pVisData->m_Animations[iAtt].iReplacement = kActivityLookup_Unknown;
				pVisData->m_Animations[iAtt].pszReplacement = pKVEntry->GetString( "replacement", NULL );

				pVisData->m_Animations[iAtt].pszSequence = pKVEntry->GetString( "sequence", NULL );
				pVisData->m_Animations[iAtt].pszScene = pKVEntry->GetString( "scene", NULL );
				pVisData->m_Animations[iAtt].pszRequiredItem = pKVEntry->GetString( "required_item", NULL );
				pVisData->m_Animations[iAtt].flFrequency = pKVEntry->GetFloat( "frequency", 1.0 );
			}
			else if ( !Q_stricmp( pszEntry, "player_bodygroups" ) )
			{
				FOR_EACH_SUBKEY( pKVEntry, pKVBodygroupKey )
				{
					const char *pszBodygroupName = pKVBodygroupKey->GetName();
					int iValue = pKVBodygroupKey->GetInt();

					// Track bodygroup information for this item in particular.
					pVisData->m_ModifiedBodyGroupNames.Insert( pszBodygroupName, iValue );

					// Track global schema state.
					CEconItemSchema* pItemSchema = (CEconItemSchema*) &pschema;
					pItemSchema->AssignDefaultBodygroupState( pszBodygroupName, iValue );
				}
			}
			else if ( !Q_stricmp( pszEntry, "skin" ) )
			{
				pVisData->iSkin = pKVEntry->GetInt();
			}
			else if ( !Q_stricmp( pszEntry, "use_per_class_bodygroups" ) )
			{
				pVisData->bUsePerClassBodygroups = pKVEntry->GetBool();
			}
			else if ( !Q_stricmp( pszEntry, "muzzle_flash" ) )
			{
				pVisData->m_pszMuzzleFlash = pKVEntry->GetString();
			}
			else if ( !Q_stricmp( pszEntry, "tracer_effect" ) )
			{
				pVisData->m_pszTracerEffect = pKVEntry->GetString();
			}
			else if ( !Q_stricmp( pszEntry, "particle_effect" ) )
			{
				pVisData->m_pszParticleEffect = pKVEntry->GetString();
			}
			else if ( !Q_stricmp( pszEntry, "particle_snapshot" ) )
			{
				pVisData->m_pszParticleSnapshot = pKVEntry->GetString();
			}
			else if ( StringHasPrefix(  pszEntry, "custom_sound" ) ) // TF2 style custom sounds.
			{
				int iIndex = 0;
				if ( pszEntry[12] )
				{
					iIndex = clamp( atoi( &pszEntry[12] ), 0, MAX_VISUALS_CUSTOM_SOUNDS-1 );
				}
				pVisData->m_pszCustomSounds[iIndex] = pKVEntry->GetString();
			}
			else if ( !Q_stricmp( pszEntry, "material_override" ) )
			{
				pVisData->m_pszMaterialOverride = pKVEntry->GetString();
			}
			else if ( StringHasPrefix(  pszEntry, "sound_" ) ) // Orange box style weapon sounds.
			{
				int iIndex = GetWeaponSoundFromString( &pszEntry[6] );
				if ( iIndex != -1 )
				{
					pVisData->m_pszWeaponSoundReplacements[iIndex] = pKVEntry->GetString();
				}
			}
			else if ( !Q_stricmp( pszEntry, "code_controlled_bodygroup" ) )
			{
				const char *pBodyGroupName = pKVEntry->GetString( "bodygroup", NULL );
				const char *pFuncName = pKVEntry->GetString( "function", NULL );
				if ( pBodyGroupName && pFuncName )
				{
					codecontrolledbodygroupdata_t ccbgd = { pFuncName, NULL };
					pVisData->m_CodeControlledBodyGroupNames.Insert( pBodyGroupName, ccbgd );
				}
			}
			else if ( !Q_stricmp( pszEntry, "vm_bodygroup_override" ) )
			{
				pVisData->m_iViewModelBodyGroupOverride = pKVEntry->GetInt();
			}
			else if ( !Q_stricmp( pszEntry, "vm_bodygroup_state_override" ) )
			{
				pVisData->m_iViewModelBodyGroupStateOverride = pKVEntry->GetInt();
			}
			else if ( !Q_stricmp( pszEntry, "wm_bodygroup_override" ) )
			{
				pVisData->m_iWorldModelBodyGroupOverride = pKVEntry->GetInt();
			}
			else if ( !Q_stricmp( pszEntry, "wm_bodygroup_state_override" ) )
			{
				pVisData->m_iWorldModelBodyGroupStateOverride = pKVEntry->GetInt();
			}
			else if ( !Q_stricmp( pszEntry, "skip_model_combine" ) )
			{
				pVisData->m_bSkipModelCombine = pKVEntry->GetBool();
			}
			else if ( !Q_stricmp( pszEntry, "animation_modifiers" ) )
			{
				FOR_EACH_SUBKEY( pKVEntry, pKVAnimMod )
				{
					pVisData->m_vecAnimationModifiers.AddToTail( pKVAnimMod->GetName() );
				}
			}
#ifdef CSTRIKE15
			////////////// CS:GO string attributes
			else if ( !Q_stricmp( pszEntry, "primary_ammo" ) )
			{
				pVisData->m_pszPrimaryAmmo = pKVEntry->GetString();
			}
			else if ( !Q_stricmp( pszEntry, "weapon_type" ) )
			{
				pVisData->m_pszWeaponTypeString = pKVEntry->GetString();
			}
			else if ( !Q_stricmp( pszEntry, "addon_location" ) )
			{
				pVisData->m_pszAddonLocation = pKVEntry->GetString();
			}
			else if ( !Q_stricmp( pszEntry, "eject_brass_effect" ) )
			{
				pVisData->m_pszEjectBrassEffect = pKVEntry->GetString();
			}
			else if ( !Q_stricmp( pszEntry, "tracer_effect" ) )
			{
				pVisData->m_pszTracerEffect = pKVEntry->GetString();
			}
			else if ( !Q_stricmp( pszEntry, "muzzle_flash_effect_1st_person" ) )
			{
				pVisData->m_pszMuzzleFlashEffect1stPerson = pKVEntry->GetString();
			}
			else if ( !Q_stricmp( pszEntry, "muzzle_flash_effect_1st_person_alt" ) )
			{
				pVisData->m_pszMuzzleFlashEffect1stPersonAlt = pKVEntry->GetString();
			}
			else if ( !Q_stricmp( pszEntry, "muzzle_flash_effect_3rd_person" ) )
			{
				pVisData->m_pszMuzzleFlashEffect3rdPerson = pKVEntry->GetString();
			}
			else if ( !Q_stricmp( pszEntry, "muzzle_flash_effect_3rd_person_alt" ) )
			{
				pVisData->m_pszMuzzleFlashEffect3rdPersonAlt = pKVEntry->GetString();
			}
			else if ( !Q_stricmp( pszEntry, "heat_effect" ) )
			{
				pVisData->m_pszHeatEffect = pKVEntry->GetString();
			}
			else if ( !Q_stricmp( pszEntry, "player_animation_extension" ) )
			{
				pVisData->m_pszPlayerAnimationExtension = pKVEntry->GetString();
			}

#endif //#ifdef CSTRIKE15

			pKVEntry = pKVEntry->GetNextKey();
		}
#endif // defined(CLIENT_DLL) || defined(GAME_DLL)
		KeyValues *pStylesDataKV = pVisualsKV->FindKey( "styles" );
		if ( pStylesDataKV )
		{
			BInitStylesBlockFromKV( pStylesDataKV, *(CEconItemSchema*)( &pschema ), pVisData, pVecErrors );
		}

		KeyValues * pKVAlternateIcons = pVisualsKV->FindKey( "alternate_icons" );
		if ( NULL != pKVAlternateIcons )
		{
			BInitAlternateIconsFromKV( pKVAlternateIcons, *(CEconItemSchema*)( &pschema ), pVisData, pVecErrors );
		}


		m_pAssetInfo = pVisData;
	}
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEconItemDefinition::GeneratePrecacheModelStrings( bool bDynamicLoad, CUtlVector<const char *> *out_pVecModelStrings ) const
{
	Assert( out_pVecModelStrings );

	// Add base model.
	out_pVecModelStrings->AddToTail( GetBasePlayerDisplayModel() );

	// Add styles.
	if ( GetNumStyles() )
	{
		for ( style_index_t i=0; i<GetNumStyles(); ++i )
		{
			const CEconStyleInfo *pStyle = GetStyleInfo( i );
			Assert( pStyle );

			pStyle->GeneratePrecacheModelStringsForStyle( out_pVecModelStrings );
		}
	}

	if ( m_pAssetInfo )
	{
		// Precache all the attached models.
		for ( int model = 0; model < m_pAssetInfo->m_AttachedModels.Count(); model++ )
		{
			out_pVecModelStrings->AddToTail( m_pAssetInfo->m_AttachedModels[model].m_pszModelName );
		}

		// Precache any model overrides.
		if ( m_pAssetInfo->m_pszEntityModel )
		{
			out_pVecModelStrings->AddToTail( m_pAssetInfo->m_pszEntityModel );
		}
		
		if ( m_pAssetInfo->m_pszViewModel )
		{
			out_pVecModelStrings->AddToTail( m_pAssetInfo->m_pszViewModel );
		}

		// Precache hero model change.
		CUtlVector<AssetModifier*>* pHeroModelChanges = m_pAssetInfo->GetAssetModifiers( AM_HeroModelChange );
		if ( pHeroModelChanges )
		{
			FOR_EACH_VEC( *pHeroModelChanges, i )
			{
				AssetModifier* pAssetMod = pHeroModelChanges->Element( i );
				if ( pAssetMod )
				{
					out_pVecModelStrings->AddToTail( pAssetMod->m_strModifier.Get() );
				}
			}
		}

		// Precache couriers.
		pHeroModelChanges = m_pAssetInfo->GetAssetModifiers( AM_Courier );
		if ( pHeroModelChanges )
		{
			FOR_EACH_VEC( *pHeroModelChanges, i )
			{
				AssetModifier* pAssetMod = pHeroModelChanges->Element( i );
				if ( pAssetMod )
				{
					out_pVecModelStrings->AddToTail( pAssetMod->m_strModifier.Get() );
				}
			}
		}

		// Precache flying couriers.
		pHeroModelChanges = m_pAssetInfo->GetAssetModifiers( AM_CourierFlying );
		if ( pHeroModelChanges )
		{
			FOR_EACH_VEC( *pHeroModelChanges, i )
			{
				AssetModifier* pAssetMod = pHeroModelChanges->Element( i );
				if ( pAssetMod )
				{
					out_pVecModelStrings->AddToTail( pAssetMod->m_strModifier.Get() );
				}
			}
		}
	}

#ifdef DOTA_DLL
	// We don't need to cache the inventory model, because it's never loaded by the game.
#else
	if ( GetIconDisplayModel() )
	{
		out_pVecModelStrings->AddToTail( GetIconDisplayModel() );
	}
#endif
	if ( GetBuyMenuDisplayModel() )
	{
		out_pVecModelStrings->AddToTail( GetBuyMenuDisplayModel() );
	}

	if ( GetWorldDroppedModel() )
	{
		out_pVecModelStrings->AddToTail( GetWorldDroppedModel() );
	}

	if ( GetMagazineModel() )
	{
		out_pVecModelStrings->AddToTail( GetMagazineModel() );
	}

	if ( GetScopeLensMaskModel() )
	{
		out_pVecModelStrings->AddToTail( GetScopeLensMaskModel() );
	}

	const KillEaterScoreMap_t& mapKillEaterScoreTypes = GetItemSchema()->GetKillEaterScoreTypes();
	FOR_EACH_MAP( mapKillEaterScoreTypes, i )
	{
		const char *pchStatTrakModel = GetStatTrakModelByType( mapKillEaterScoreTypes[ i ].m_nValue );
		if ( pchStatTrakModel )
		{
			out_pVecModelStrings->AddToTail( pchStatTrakModel );
		}
	}

	if ( GetUidModel() )
	{
		out_pVecModelStrings->AddToTail( GetUidModel() );
	}

	for ( int i=0; i<GetNumSupportedStickerSlots(); ++i )
	{
		if ( GetStickerSlotModelBySlotIndex(i) )
		{
			out_pVecModelStrings->AddToTail( GetStickerSlotModelBySlotIndex(i) );
		}
	}

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEconItemDefinition::GeneratePrecacheSoundStrings( CUtlVector<const char*> *out_pVecSoundStrings ) const
{
	Assert( out_pVecSoundStrings );

	// Precache all replacement sounds.
	if ( m_pAssetInfo )
	{
		for ( int sound=0; sound<m_pAssetInfo->m_Sounds.Count(); sound++ )
		{
			out_pVecSoundStrings->AddToTail( m_pAssetInfo->m_Sounds[sound].pszReplacement );
		}
		for ( int sound = 0; sound < ARRAYSIZE( m_pAssetInfo->m_pszWeaponSoundReplacements ); sound++ )
		{
			if ( !m_pAssetInfo->m_pszWeaponSoundReplacements[ sound ] )
				continue;

			out_pVecSoundStrings->AddToTail( m_pAssetInfo->m_pszWeaponSoundReplacements[ sound ] );
		}
	}
}

void CEconItemDefinition::GeneratePrecacheEffectStrings( CUtlVector<const char*> *out_pVecEffectStrings ) const
{
	Assert( out_pVecEffectStrings );

	if ( !m_pAssetInfo )
		return;

	if ( m_pAssetInfo->m_pszEjectBrassEffect )
	{
		out_pVecEffectStrings->AddToTail( m_pAssetInfo->m_pszEjectBrassEffect );
	}
	if ( m_pAssetInfo->m_pszTracerEffect )
	{
		out_pVecEffectStrings->AddToTail( m_pAssetInfo->m_pszTracerEffect );
	}
	if ( m_pAssetInfo->m_pszMuzzleFlashEffect1stPerson )
	{
		out_pVecEffectStrings->AddToTail( m_pAssetInfo->m_pszMuzzleFlashEffect1stPerson );
	}
	if ( m_pAssetInfo->m_pszMuzzleFlashEffect1stPersonAlt )
	{
		out_pVecEffectStrings->AddToTail( m_pAssetInfo->m_pszMuzzleFlashEffect1stPersonAlt );
	}
	if ( m_pAssetInfo->m_pszMuzzleFlashEffect3rdPerson )
	{
		out_pVecEffectStrings->AddToTail( m_pAssetInfo->m_pszMuzzleFlashEffect3rdPerson );
	}
	if ( m_pAssetInfo->m_pszMuzzleFlashEffect3rdPersonAlt )
	{
		out_pVecEffectStrings->AddToTail( m_pAssetInfo->m_pszMuzzleFlashEffect3rdPersonAlt );
	}
	if ( m_pAssetInfo->m_pszHeatEffect )
	{
		out_pVecEffectStrings->AddToTail( m_pAssetInfo->m_pszHeatEffect );
	}
}

#endif // #if defined(CLIENT_DLL) || defined(GAME_DLL)

//-----------------------------------------------------------------------------
// Purpose: Parse the alternate icons for this item.
//-----------------------------------------------------------------------------
bool CEconItemDefinition::BInitAlternateIconsFromKV( KeyValues *pKVAlternateIcons, CEconItemSchema &pschema, AssetInfo *pVisData, CUtlVector<CUtlString> *pVecErrors )
{
	m_pMapAlternateIcons->Purge();

	FOR_EACH_TRUE_SUBKEY( pKVAlternateIcons, pKVAlternateIcon )
	{
		int iIconIndex = Q_atoi( pKVAlternateIcon->GetName() );

		SCHEMA_INIT_CHECK( 
			!m_pMapAlternateIcons->Find( iIconIndex ) != m_pMapAlternateIcons->InvalidIndex(),
			CFmtStr( "Duplicate alternate icon definition (%d)", iIconIndex ) );

		SCHEMA_INIT_CHECK( 
			iIconIndex >= 0,
			CFmtStr( "Alternate icon definition index %d must be greater than or equal to zero", iIconIndex ) );

		m_pMapAlternateIcons->Insert( iIconIndex, pKVAlternateIcon->GetString( "icon_path" ) );
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose: Parse the styles sub-section of the visuals block.
//-----------------------------------------------------------------------------
void CEconItemDefinition::BInitStylesBlockFromKV( KeyValues *pKVStyles, CEconItemSchema &pschema, AssetInfo *pVisData, CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_SUBKEY( pKVStyles, pKVStyle )
	{
		CEconStyleInfo *pStyleInfo = pschema.CreateEconStyleInfo();
		Assert( pStyleInfo );

		pStyleInfo->BInitFromKV( pKVStyle, pschema, pVecErrors );

		pVisData->m_Styles.AddToTail( pStyleInfo );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Parse one style from the styles block.
//-----------------------------------------------------------------------------
void CEconStyleInfo::BInitFromKV( KeyValues *pKVStyle, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors )
{
	enum { kInvalidSkinKey = -1, };

	Assert( pKVStyle );

	m_iIndex = atoi( pKVStyle->GetName() );

	// A "skin" entry means "use this index for all of our teams, no matter how many we have".
	int iCommonSkin = pKVStyle->GetInt( "skin", kInvalidSkinKey );
	if ( iCommonSkin != kInvalidSkinKey )
	{
		m_iSkin = iCommonSkin;
	}

	// If we don't have a base entry, we look for a unique entry for each team. This will be
	// handled in a subclass if necessary.

	// Are we hiding additional bodygroups when this style is active?
	KeyValues *pKVHideBodygroups = pKVStyle->FindKey( "additional_hidden_bodygroups" );
	if ( pKVHideBodygroups )
	{
		FOR_EACH_SUBKEY( pKVHideBodygroups, pKVBodygroup )
		{
			m_vecAdditionalHideBodygroups.AddToTail( pKVBodygroup->GetName() );
		}
	}

	// Remaining common properties.
	m_pszName = pKVStyle->GetString( "name", "#TF_UnknownStyle" );
	m_pszBasePlayerModel = pKVStyle->GetString( "model_player", NULL );

	// An alternate icon to use.
	m_iIcon = pKVStyle->GetInt( "alternate_icon", 0 );

	// Unlock rules.
	KeyValues* pUnlockInfo = pKVStyle->FindKey( "unlock" );
	if ( pUnlockInfo )
	{
		m_UnlockInfo.iPrice = pUnlockInfo->GetInt( "price", 0 );
		m_UnlockInfo.pszItemName = pUnlockInfo->GetString( "item", NULL );
		m_UnlockInfo.iStylePreReq = pUnlockInfo->GetInt( "style", 0 );
		m_UnlockInfo.pszAttrib = pUnlockInfo->GetString( "attribute", NULL );
		m_UnlockInfo.iAttribValue = pUnlockInfo->GetInt( "attribute_value", 0 );
	}
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEconStyleInfo::GeneratePrecacheModelStringsForStyle( CUtlVector<const char *> *out_pVecModelStrings ) const
{
	Assert( out_pVecModelStrings );

	if ( GetBasePlayerDisplayModel() != NULL )
	{
		out_pVecModelStrings->AddToTail( GetBasePlayerDisplayModel() );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose:	Item definition initialization helpers.
//-----------------------------------------------------------------------------
static void RecursiveInheritKeyValues( KeyValues *out_pValues, KeyValues *pInstance )
{
	FOR_EACH_SUBKEY( pInstance, pSubKey )
	{
		KeyValues::types_t eType = pSubKey->GetDataType();
		switch ( eType )
		{
		case KeyValues::TYPE_STRING:		out_pValues->SetString( pSubKey->GetName(), pSubKey->GetString() );			break;
		case KeyValues::TYPE_INT:			out_pValues->SetInt( pSubKey->GetName(), pSubKey->GetInt() );				break;
		case KeyValues::TYPE_FLOAT:			out_pValues->SetFloat( pSubKey->GetName(), pSubKey->GetFloat() );			break;
		case KeyValues::TYPE_WSTRING:		out_pValues->SetWString( pSubKey->GetName(), pSubKey->GetWString() );		break;
		case KeyValues::TYPE_COLOR:			out_pValues->SetColor( pSubKey->GetName(), pSubKey->GetColor() ) ;			break;
		case KeyValues::TYPE_UINT64:		out_pValues->SetUint64( pSubKey->GetName(), pSubKey->GetUint64() ) ;		break;

		// "NONE" means "KeyValues"
		case KeyValues::TYPE_NONE:
		{
			// We may already have this part of the tree to stuff data into/overwrite, or we
			// may have to make a new block.
			KeyValues *pNewChild = out_pValues->FindKey( pSubKey->GetName() );
			if ( !pNewChild )
			{
				pNewChild = out_pValues->CreateNewKey();
				pNewChild->SetName( pSubKey->GetName() );
			}

			RecursiveInheritKeyValues( pNewChild, pSubKey );
			break;
		}

		case KeyValues::TYPE_PTR:
		default:
			Assert( !"Unhandled data type for KeyValues inheritance!" );
			break;
		}
	}
}

#ifdef _DEBUG
#define DEBUG_SCHEMA_WRITE_TMP_FILE 0
#if DEBUG_SCHEMA_WRITE_TMP_FILE
#include <stdio.h>
class CKeyValuesDumpContextAsSchemaFile : public IKeyValuesDumpContextAsText
{
public:
	// Overrides developer level to dump in DevMsg, zero to dump as Msg
	CKeyValuesDumpContextAsSchemaFile() {}

public:
	virtual bool KvWriteText( char const *szText )
	{
		FILE *f = NULL;
		fopen_s( &f, "c:/tmp/econ_item_schema.txt", "a+t" );
		if ( f )
		{
			fprintf( f, "%s", szText );
			fclose( f );
		}
		return true;
	}
}
g_schemadbg;
#endif
#endif

// Always returns a newly allocated copy of KeyValues
// Applies prefabs supporting multiple inheritance from Right-to-Left (RTL)
// this way "prefab" "valve tournament p250" will start with "p250" then apply "tournament" prefab on top of that,
//						then will apply "valve" prefab on top of that and then the values of the actual instance.
static KeyValues *InheritKeyValuesRTLMulti( KeyValues *pInstance, CEconItemSchema &pschema )
{
	Assert( pInstance );
	KeyValues *pFinalValues = pInstance->MakeCopy();

#if DEBUG_SCHEMA_WRITE_TMP_FILE
	static int s_nDebugIndentLevel = 0;
#endif

	CUtlVector< char * > arrPrefabs;
	if ( const char *svPrefabName = pFinalValues->GetString( "prefab", NULL ) )
	{
		Q_SplitString( svPrefabName, " ", arrPrefabs );

#if DEBUG_SCHEMA_WRITE_TMP_FILE
		if ( arrPrefabs.Count() )
		{
			++ s_nDebugIndentLevel;
			
			g_schemadbg.KvWriteIndent( s_nDebugIndentLevel );
			g_schemadbg.KvWriteText( CFmtStr( "Expanding data with %d prefabs:--\n", arrPrefabs.Count() ) );
			pFinalValues->Dump( &g_schemadbg, s_nDebugIndentLevel );
			g_schemadbg.KvWriteIndent( s_nDebugIndentLevel );
			g_schemadbg.KvWriteText( "--\n" );
		}
#endif
	}
	FOR_EACH_VEC_BACK( arrPrefabs, i )
	{
		KeyValues *pKVPrefab = pschema.FindDefinitionPrefabByName( arrPrefabs[i] );
		if ( !pKVPrefab ) continue;
#if DEBUG_SCHEMA_WRITE_TMP_FILE
		{
			g_schemadbg.KvWriteIndent( s_nDebugIndentLevel );
			g_schemadbg.KvWriteText( CFmtStr( "Appending prefab '%s':\n", arrPrefabs[i] ) );
			pKVPrefab->Dump( &g_schemadbg, s_nDebugIndentLevel );
		}
#endif
		pKVPrefab = InheritKeyValuesRTLMulti( pKVPrefab, pschema );
		pKVPrefab->SetName( pFinalValues->GetName() );

		RecursiveInheritKeyValues( pKVPrefab, pFinalValues );
		pFinalValues->deleteThis();
		pFinalValues = pKVPrefab;

#if DEBUG_SCHEMA_WRITE_TMP_FILE
		{
			g_schemadbg.KvWriteIndent( s_nDebugIndentLevel );
			g_schemadbg.KvWriteText( CFmtStr( "After appending prefab '%s':\n", arrPrefabs[ i ] ) );
			pFinalValues->Dump( &g_schemadbg, s_nDebugIndentLevel );
		}
#endif
	}

#if DEBUG_SCHEMA_WRITE_TMP_FILE
	if ( arrPrefabs.Count() )
	{
		g_schemadbg.KvWriteIndent( s_nDebugIndentLevel );
		g_schemadbg.KvWriteText( CFmtStr( "Finished expanding data with %d prefabs:--\n", arrPrefabs.Count() ) );
		pFinalValues->Dump( &g_schemadbg, s_nDebugIndentLevel );
		g_schemadbg.KvWriteIndent( s_nDebugIndentLevel );
		g_schemadbg.KvWriteText( "--\n" );

		-- s_nDebugIndentLevel;
	}
#endif

	arrPrefabs.PurgeAndDeleteElements();

	return pFinalValues;
}

KeyValues *CEconItemSchema::FindDefinitionPrefabByName( const char *pszPrefabName ) const
{
	int iIndex = m_mapDefinitionPrefabs.Find( pszPrefabName );
	if ( m_mapDefinitionPrefabs.IsValidIndex( iIndex ) )
		return m_mapDefinitionPrefabs[iIndex];

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	Initialize the item definition
// Input:	pKVItem - The KeyValues representation of the item
//			schema - The overall item schema for this item
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemDefinition::BInitFromKV( KeyValues *pKVItem, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	// Set standard members
	m_pKVItem = InheritKeyValuesRTLMulti( pKVItem, pschema );
	m_bEnabled = m_pKVItem->GetBool( "enabled" );
	m_bLoadOnDemand = m_pKVItem->GetBool( "loadondemand" );
	m_nDefIndex = Q_atoi( m_pKVItem->GetName() );
	m_unMinItemLevel = (uint32)m_pKVItem->GetInt( "min_ilevel", pschema.GetMinLevel() );
	m_unMaxItemLevel = (uint32)m_pKVItem->GetInt( "max_ilevel", pschema.GetMaxLevel() );
	m_nDefaultDropQuantity = m_pKVItem->GetInt( "default_drop_quantity", 1 );

	KeyValues *pKVMultiAssocItems = m_pKVItem->FindKey( "associated_items" ),
		*pKVSingleAssocItem = m_pKVItem->FindKey( "associated_item" );

	// Maybe we have multiple entries?
	if ( pKVMultiAssocItems )
	{
		for ( KeyValues *pKVAssocItem = pKVMultiAssocItems->GetFirstSubKey(); pKVAssocItem; pKVAssocItem = pKVAssocItem->GetNextKey() )
		{
			m_nAssociatedItemsDefIndexes.AddToTail( atoi( pKVAssocItem->GetName() ) );
		}
	}
	// This is our one-and-only-one associated item
	else if ( pKVSingleAssocItem )
	{
		const char *pAssocItem = pKVSingleAssocItem->GetString( (const char *)NULL, NULL );
		if ( pAssocItem )
		{
			m_nAssociatedItemsDefIndexes.AddToTail( atoi( pAssocItem ) );
		}
	}

	m_nSoundMaterialID = m_pKVItem->GetInt( "sound_material" );

	m_nPopularitySeed = m_pKVItem->GetInt( "popularity_seed", 0 );

	// initializing this one first so that it will be available for all the errors below
	m_pszDefinitionName = m_pKVItem->GetString( "name", NULL );

	m_bDisableStyleSelection = m_pKVItem->GetBool( "disable_style_selector", false );
	
#if defined(CLIENT_DLL) || defined(GAME_DLL)
	// We read this manually here in the game dlls. The GC reads it below while checking pschema.
	pschema.BGetItemQualityFromName( m_pKVItem->GetString( "item_quality" ), &m_nItemQuality );
	pschema.BGetItemQualityFromName( m_pKVItem->GetString( "forced_item_quality" ), &m_nForcedItemQuality );
	pschema.BGetItemQualityFromName( m_pKVItem->GetString( "default_drop_quality" ), &m_nDefaultDropItemQuality );

	pschema.BGetItemRarityFromName( m_pKVItem->GetString( "item_rarity" ), &m_nItemRarity );
#endif

	// Check for required fields
	SCHEMA_INIT_CHECK( 
		NULL != m_pKVItem->FindKey( "name" ), 
		CFmtStr( "Item definition %s: Missing required field \"name\"", m_pKVItem->GetName() ) );

	SCHEMA_INIT_CHECK( 
		NULL != m_pKVItem->FindKey( "item_class" ), 
		CFmtStr( "Item definition %s: Missing required field \"item_class\"", m_pKVItem->GetName() ) );

	// Check value ranges
	SCHEMA_INIT_CHECK( 
		m_pKVItem->GetInt( "min_ilevel" ) >= 0, 
		CFmtStr( "Item definition %s: \"min_ilevel\" must be greater than or equal to 0", GetDefinitionName() ) );

	SCHEMA_INIT_CHECK( 
		m_pKVItem->GetInt( "max_ilevel" ) >= 0, 
		CFmtStr( "Item definition %s: \"max_ilevel\" must be greater than or equal to 0", GetDefinitionName() ) );

	// Get the item class
	m_pszItemClassname = m_pKVItem->GetString( "item_class", NULL );

	m_bAllowPurchaseStandalone = m_pKVItem->GetBool( "allow_purchase_standalone", false );

	m_pszClassToken = m_pKVItem->GetString( "class_token_id", NULL );
	m_pszSlotToken = m_pKVItem->GetString( "slot_token_id", NULL );

	m_bPublicItem = m_pKVItem->GetInt( "developer" ) == 0;
	m_bIgnoreInCollectionView = m_pKVItem->GetBool( "ignore_in_collection_view", false );

	// Display data
	m_pszItemBaseName = m_pKVItem->GetString( "item_name", "" ); // non-NULL to ensure we can sort
	m_pszItemTypeName = m_pKVItem->GetString( "item_type_name", "" ); // non-NULL to ensure we can sort
	m_pszItemDesc = m_pKVItem->GetString( "item_description", NULL );
	m_pszArmoryDesc = m_pKVItem->GetString( "armory_desc", NULL );
	m_pszInventoryModel = m_pKVItem->GetString( "model_inventory", NULL );
	m_pszInventoryImage = m_pKVItem->GetString( "image_inventory", NULL );

	m_pPortraitsKV = m_pKVItem->FindKey( "portraits" );

	m_unItemTypeID = CRC32_ProcessSingleBuffer( m_pszItemTypeName, V_strlen( m_pszItemTypeName ) );
	const char* pOverlay = m_pKVItem->GetString( "image_inventory_overlay", NULL );
	if ( pOverlay )
	{
		m_pszInventoryOverlayImages.AddToTail( pOverlay );
	}
	pOverlay = m_pKVItem->GetString( "image_inventory_overlay2", NULL );
	if ( pOverlay )
	{
		m_pszInventoryOverlayImages.AddToTail( pOverlay );
	}

	m_iInventoryImagePosition[0] = m_pKVItem->GetInt( "image_inventory_pos_x", 0 );
	m_iInventoryImagePosition[1] = m_pKVItem->GetInt( "image_inventory_pos_y", 0 );
	m_iInventoryImageSize[0] = m_pKVItem->GetInt( "image_inventory_size_w", 0 );
	m_iInventoryImageSize[1] = m_pKVItem->GetInt( "image_inventory_size_h", 0 );
	m_pszHolidayRestriction = m_pKVItem->GetString( "holiday_restriction", NULL );
	m_iSubType = m_pKVItem->GetInt( "subtype", 0 );
	m_pszBaseDisplayModel = m_pKVItem->GetString( "model_player", NULL );
	m_pszWorldDisplayModel = m_pKVItem->GetString( "model_world", NULL ); // Not the ideal method. c_models are better, but this is to solve a retrofit problem with the sticky launcher.
	m_pszWorldDroppedModel = m_pKVItem->GetString( "model_dropped", NULL );

	//build the world dropped model string by appending "_dropped"
	if ( ( m_pszWorldDroppedModel == NULL ) && ( m_pszWorldDisplayModel != NULL ) )
	{
		V_StripExtension( m_pszWorldDisplayModel, m_szWorldDroppedModel, sizeof( m_szWorldDroppedModel ) );
		V_strcat_safe( m_szWorldDroppedModel, "_dropped.mdl" );
		m_pszWorldDroppedModel = m_szWorldDroppedModel;
	}
	
	// Add new StatTrak modules here.

	m_pszIconDefaultImage = m_pKVItem->GetString( "icon_default_image", NULL );
	m_pszWorldExtraWearableModel = m_pKVItem->GetString( "extra_wearable", NULL ); 
	m_pszBrassModelOverride = m_pKVItem->GetString( "brass_eject_model", NULL );
	m_pszWorldExtraWearableModel = m_pKVItem->GetString( "extra_wearable", NULL ); 
	m_pszBrassModelOverride = m_pKVItem->GetString( "brass_eject_model", NULL );
	m_bHideBodyGroupsDeployedOnly = m_pKVItem->GetBool( "hide_bodygroups_deployed_only" );
	m_bAttachToHands = m_pKVItem->GetInt( "attach_to_hands", 0 ) != 0;
	m_bAttachToHandsVMOnly = m_pKVItem->GetInt( "attach_to_hands_vm_only", 0 ) != 0;
	m_bProperName = m_pKVItem->GetInt( "propername", 0 ) != 0;
	m_bFlipViewModel = m_pKVItem->GetInt( "flip_viewmodel", 0 ) != 0;
	m_bActAsWearable = m_pKVItem->GetInt( "act_as_wearable", 0 ) != 0;
	m_iDropType = StringFieldToInt( m_pKVItem->GetString("drop_type"), g_szDropTypeStrings, ARRAYSIZE(g_szDropTypeStrings) );

	// Creation data
	m_bHidden = m_pKVItem->GetInt( "hidden", 0 ) != 0;
	m_bShouldShowInArmory = m_pKVItem->GetInt( "show_in_armory", 0 ) != 0;
	m_bBaseItem = m_pKVItem->GetInt( "baseitem", 0 ) != 0;
	m_bDefaultSlotItem = m_pKVItem->GetInt( "default_slot_item", 0 ) != 0;
	m_pszItemLogClassname = m_pKVItem->GetString( "item_logname", NULL );
	m_pszItemIconClassname = m_pKVItem->GetString( "item_iconname", NULL );
#if ECONITEM_DATABASE_AUDIT_TABLES_FEATURE
	m_pszDatabaseAuditTable = m_pKVItem->GetString( "database_audit_table", NULL );
#endif
	m_bImported = m_pKVItem->FindKey( "import_from" ) != NULL;
	m_bOnePerAccountCDKEY = m_pKVItem->GetInt( "one_per_account_cdkey", 0 ) != 0;

	m_pszParticleFile = m_pKVItem->GetString( "particle_file", NULL );
	m_pszParticleSnapshotFile = m_pKVItem->GetString( "particle_snapshot", NULL );

	m_pszLootListName = m_pKVItem->GetString( "loot_list_name", NULL );

	// Stickers
	KeyValues *pStickerDataKV = m_pKVItem->FindKey( "stickers" );
	if ( pStickerDataKV )
	{
		FOR_EACH_SUBKEY( pStickerDataKV, pStickerModelEntry )
		{

			StickerData_t *pStickerData = &m_vStickerModels[ m_vStickerModels.AddToTail() ];
			
			const char *pStickerModelPath = pStickerModelEntry->GetString( "viewmodel_geometry" );
			V_strcpy_safe( pStickerData->m_szStickerModelPath, pStickerModelPath );

			const char *pStickerMaterialPath = pStickerModelEntry->GetString( "viewmodel_material" );
			V_strcpy_safe( pStickerData->m_szStickerMaterialPath, pStickerMaterialPath );

			const char *pStickerWorldModelProjectionPosition = pStickerModelEntry->GetString( "worldmodel_decal_pos" );
			if ( pStickerWorldModelProjectionPosition[0] != 0 )
			{
				float flX = 0.0f, flY = 0.0f, flZ = 0.0f;
				sscanf( pStickerWorldModelProjectionPosition, "%f %f %f", &flX, &flY, &flZ );
				pStickerData->m_vWorldModelProjectionStart = Vector( flX, flY, flZ );
			}

			const char *pStickerWorldModelProjectionEnd = pStickerModelEntry->GetString( "worldmodel_decal_end" );
			if ( pStickerWorldModelProjectionEnd[0] != 0 )
			{
				float flX = 0.0f, flY = 0.0f, flZ = 0.0f;
				sscanf( pStickerWorldModelProjectionEnd, "%f %f %f", &flX, &flY, &flZ );
				pStickerData->m_vWorldModelProjectionEnd = Vector( flX, flY, flZ );
			}
			else
			{
				pStickerData->m_vWorldModelProjectionEnd = Vector(-10,0,0) + pStickerData->m_vWorldModelProjectionStart;
			}

			V_strcpy_safe( pStickerData->m_szStickerBoneParentName, pStickerModelEntry->GetString( "worldmodel_decal_bone", "ValveBiped.weapon_bone" ) );

		}
	}

	// Tool data
	m_pTool = NULL;
	KeyValues *pToolDataKV = m_pKVItem->FindKey( "tool" );
	if ( pToolDataKV )
	{
		const char *pszType = pToolDataKV->GetString( "type", NULL );
		SCHEMA_INIT_CHECK( pszType, CFmtStr( "Tool '%s' missing required type.", m_pKVItem->GetName() ) );

		// Common-to-all-tools settings.
		const char *pszUseString = pToolDataKV->GetString( "use_string", NULL );
		const char *pszUsageRestriction = pToolDataKV->GetString( "restriction", NULL );
		KeyValues *pToolUsageKV = pToolDataKV->FindKey( "usage" );

		// Common-to-all-tools usage capability flags.
		item_capabilities_t usageCapabilities = (item_capabilities_t)ITEM_CAP_TOOL_DEFAULT;
		KeyValues *pToolUsageCapsKV = pToolDataKV->FindKey( "usage_capabilities" );
		if ( pToolUsageCapsKV )
		{
			KeyValues *pEntry = pToolUsageCapsKV->GetFirstSubKey();
			while ( pEntry )
			{
				ParseCapability( usageCapabilities, pEntry );
				pEntry = pEntry->GetNextKey();
			}
		}

		m_pTool = pschema.CreateEconToolImpl( pszType, pszUseString, pszUsageRestriction, usageCapabilities, pToolUsageKV );
		SCHEMA_INIT_CHECK( m_pTool, CFmtStr( "Unable to create tool implementation for '%s', of type '%s'.", m_pKVItem->GetName(), pszType ) );
	}

	// capabilities
	m_iCapabilities = (item_capabilities_t)ITEM_CAP_DEFAULT;
	KeyValues *pCapsKV = m_pKVItem->FindKey( "capabilities" );
	if ( pCapsKV )
	{
		KeyValues *pEntry = pCapsKV->GetFirstSubKey();
		while ( pEntry )
		{
			ParseCapability( m_iCapabilities, pEntry );
			pEntry = pEntry->GetNextKey();
		}
	}

	// cache item map names
	m_pszArmoryRemap = m_pKVItem->GetString( "armory_remap", NULL );
	m_pszStoreRemap = m_pKVItem->GetString( "store_remap", NULL );

	// Don't bother parsing visual data on the GC, it's unused.
	BInitVisualBlockFromKV( m_pKVItem, pschema, pVecErrors );

	// Calculate our equip region mask.
	{
		m_unEquipRegionMask = 0;
		m_unEquipRegionConflictMask = 0;

		// Our equip region will come from one of two places -- either we have an "equip_regions" (plural) section,
		// in which case we have any number of regions specified; or we have an "equip_region" (singular) section
		// which will have one and exactly one region. If we have "equip_regions" (plural), we ignore whatever is
		// in "equip_region" (singular).
		//
		// Yes, this is sort of dumb.
		CUtlVector<const char *> vecEquipRegionNames;

		KeyValues *pKVMultiEquipRegions = m_pKVItem->FindKey( "equip_regions" ),
				  *pKVSingleEquipRegion = m_pKVItem->FindKey( "equip_region" );

		// Maybe we have multiple entries?
		if ( pKVMultiEquipRegions )
		{
			for ( KeyValues *pKVRegion = pKVMultiEquipRegions->GetFirstSubKey(); pKVRegion; pKVRegion = pKVRegion->GetNextKey() )
			{
				vecEquipRegionNames.AddToTail( pKVRegion->GetName() );
			}
		}
		// This is our one-and-only-one equip region.
		else if ( pKVSingleEquipRegion )
		{
			const char *pEquipRegionName = pKVSingleEquipRegion->GetString( (const char *)NULL, NULL );
			if ( pEquipRegionName )
			{
				vecEquipRegionNames.AddToTail( pEquipRegionName );
			}
		}

		// For each of our regions, add to our conflict mask both ourself and all the regions
		// that we conflict with.
		FOR_EACH_VEC( vecEquipRegionNames, i )
		{
			const char *pszEquipRegionName = vecEquipRegionNames[i];
			equip_region_mask_t unThisRegionMask = pschema.GetEquipRegionMaskByName( pszEquipRegionName );

			SCHEMA_INIT_CHECK(
				unThisRegionMask != 0,
				CFmtStr( "Item definition %s: Unable to find equip region mask for region named \"%s\"", GetDefinitionName(), vecEquipRegionNames[i] ) );

			m_unEquipRegionMask |= pschema.GetEquipRegionBitMaskByName( pszEquipRegionName );
			m_unEquipRegionConflictMask |= unThisRegionMask;
		}
	}

	// Parse the static attributes for this definition
	char const *szStaticAttributesSubkeyNames[] = { "attributes"
	};
	for ( int iStaticAttrGroup = 0; iStaticAttrGroup < Q_ARRAYSIZE( szStaticAttributesSubkeyNames ); ++ iStaticAttrGroup )
	{
		KeyValues *kvStaticAttrGroup = m_pKVItem->FindKey( szStaticAttributesSubkeyNames[iStaticAttrGroup] );
		if ( !kvStaticAttrGroup ) continue;
		FOR_EACH_SUBKEY( kvStaticAttrGroup, pKVKey )
		{
			static_attrib_t staticAttrib;

			SCHEMA_INIT_SUBSTEP( staticAttrib.BInitFromKV_SingleLine( GetDefinitionName(), pKVKey, pVecErrors ) );

			m_vecStaticAttributes.AddToTail( staticAttrib );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

bool static_attrib_t::BInitFromKV_MultiLine( const char *pszContext, KeyValues *pKVAttribute, CUtlVector<CUtlString> *pVecErrors )
{
	const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinitionByName( pKVAttribute->GetName() );

	SCHEMA_INIT_CHECK( 
		NULL != pAttrDef,
		CFmtStr( "Context '%s': Attribute \"%s\" in \"attributes\" did not match any attribute definitions", pszContext, pKVAttribute->GetName() ) );

	if ( pAttrDef )
	{
		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		iDefIndex = pAttrDef->GetDefinitionIndex();

		const char *pszValue = pKVAttribute->GetString( "value", NULL );
		const bool bSuccessfullyLoadedValue = pAttrType->BConvertStringToEconAttributeValue( pAttrDef, pszValue, &m_value );

		SCHEMA_INIT_CHECK(
			bSuccessfullyLoadedValue,
			CFmtStr( "Context '%s': Attribute \"%s\" could not parse value \"%s\"!", pszContext, pKVAttribute->GetName(), pszValue ? pszValue : "(null)" ) );

		m_bForceGCToGenerate	= pKVAttribute->GetBool( "force_gc_to_generate" );
	}

	return SCHEMA_INIT_SUCCESS();
}

bool static_attrib_t::BInitFromKV_SingleLine( const char *pszContext, KeyValues *pKVAttribute, CUtlVector<CUtlString> *pVecErrors )
{
	if ( pKVAttribute->GetFirstSubKey() )
	{
		return BInitFromKV_MultiLine( pszContext, pKVAttribute, pVecErrors );
	}

	const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinitionByName( pKVAttribute->GetName() );

	SCHEMA_INIT_CHECK( 
		NULL != pAttrDef,
		CFmtStr( "Context '%s': Attribute \"%s\" in \"attributes\" did not match any attribute definitions", pszContext, pKVAttribute->GetName() ) );

	if ( pAttrDef )
	{
		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		iDefIndex = pAttrDef->GetDefinitionIndex();

		const char *pszValue = pKVAttribute->GetString();
		const bool bSuccessfullyLoadedValue = pAttrType->BConvertStringToEconAttributeValue( pAttrDef, pszValue, &m_value );

		SCHEMA_INIT_CHECK(
			bSuccessfullyLoadedValue,
			CFmtStr( "Context '%s': Attribute \"%s\" could not parse value \"%s\"!", pszContext, pKVAttribute->GetName(), pszValue ? pszValue : "(null)" ) );

		m_bForceGCToGenerate = false;
	}

	return SCHEMA_INIT_SUCCESS();
}

bool CEconItemDefinition::BInitItemMappings( CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors )
{
	// Armory remapping
	if ( m_pszArmoryRemap && m_pszArmoryRemap[0] )
	{
		const CEconItemDefinition *pDef = pschema.GetItemDefinitionByName( m_pszArmoryRemap );
		if ( pDef )
		{
			m_iArmoryRemap = pDef->GetDefinitionIndex();
		}

		SCHEMA_INIT_CHECK( 
			pDef != NULL,
			CFmtStr( "Item %s: Armory remap definition \"%s\" was not found", m_pszItemBaseName, m_pszArmoryRemap ) );
	}

	// Store remapping
	if ( m_pszStoreRemap && m_pszStoreRemap[0] )
	{
		const CEconItemDefinition *pDef = pschema.GetItemDefinitionByName( m_pszStoreRemap );
		if ( pDef )
		{
			m_iStoreRemap = pDef->GetDefinitionIndex();
		}

		SCHEMA_INIT_CHECK(
			pDef != NULL,
			CFmtStr( "Item %s: Store remap definition \"%s\" was not found", m_pszItemBaseName, m_pszStoreRemap ) );
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
KeyValues* CEconItemDefinition::GetPortraitKVForModel( const char* pszModelName ) const 
{
	if ( !m_pPortraitsKV )
		return NULL;
	
	return m_pPortraitsKV->FindKey( pszModelName ); 
}


void CEconItemDefinition::AddItemSet( int nIndex )
{
	if ( m_iItemSets.Find( nIndex ) != m_iItemSets.InvalidIndex() )
		return;

	m_iItemSets.AddToTail( nIndex );
}

const CUtlVector< int >& CEconItemDefinition::GetItemSets( void ) const
{
	return m_iItemSets;
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
attachedparticlesystem_t *CEconItemDefinition::GetAttachedParticleData( int iIdx ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	Assert( iIdx < GetAssetInfo()->m_AttachedParticles.Count() );
	if ( iIdx >= GetAssetInfo()->m_AttachedParticles.Count() )
		return NULL;

	int iParticleIndex = GetAssetInfo()->m_AttachedParticles[iIdx].m_iParticleIndex;
	return ItemSystem()->GetItemSchema()->GetAttributeControlledParticleSystemByIndex( iParticleIndex );
}

bool CEconItemDefinition::IsAttachedParticleDataValidForStyle( int iIdx, int nStyle ) const
{
	Assert( iIdx < GetAssetInfo()->m_AttachedParticles.Count() );
	if ( iIdx >= GetAssetInfo()->m_AttachedParticles.Count() )
		return false;

	return ( GetAssetInfo()->m_AttachedParticles[iIdx].m_nStyle == nStyle );
}

#endif // #if defined(CLIENT_DLL) || defined(GAME_DLL)

//-----------------------------------------------------------------------------
// Purpose: Generate and return a random level according to whatever leveling
//			curve this definition uses.
//-----------------------------------------------------------------------------
uint32 CEconItemDefinition::RollItemLevel( void ) const
{
	return RandomInt( GetMinLevel(), GetMaxLevel() );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEconItemDefinition::IterateAttributes( IEconItemAttributeIterator *pIterator ) const
{
	FOR_EACH_VEC( GetStaticAttributes(), i )
	{
		const static_attrib_t& staticAttrib = GetStaticAttributes()[i];
		
		// we skip over static attributes that the GC will turn into dynamic attributes because otherwise we'll have
		// the appearance of iterating over them twice; for clients these attributes won't even make it into the
		// list
		if ( staticAttrib.m_bForceGCToGenerate )
			continue;

		const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( staticAttrib.iDefIndex );
		if ( !pAttrDef )
			continue;

		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		if ( !pAttrType->OnIterateAttributeValue( pIterator, pAttrDef, staticAttrib.m_value ) )
			return;
	}
}

const char *CEconItemDefinition::GetFirstSaleDate() const
{
	return GetRawDefinition()->GetString( "first_sale_date", "1970/01/01" );
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Activity CEconItemDefinition::GetActivityOverride( Activity baseAct ) const
{
	int iAnims = GetNumAnimations();
	for ( int i = 0; i < iAnims; i++ )
	{
		animation_on_wearable_t *pData = GetAnimationData( i );
		if ( !pData )
			continue;
		if ( pData->iActivity == kActivityLookup_Unknown )
		{
			pData->iActivity = ActivityList_IndexForName( pData->pszActivity );
		}

		if ( pData->iActivity == baseAct )
		{
			if ( pData->iReplacement == kActivityLookup_Unknown )
			{
				pData->iReplacement = ActivityList_IndexForName( pData->pszReplacement );
			}

			if ( pData->iReplacement > 0 )
			{
				return (Activity) pData->iReplacement;
			}
		}
	}

	return baseAct;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CEconItemDefinition::GetReplacementForActivityOverride( Activity baseAct ) const
{
	int iAnims = GetNumAnimations();
	for ( int i = 0; i < iAnims; i++ )
	{
		animation_on_wearable_t *pData = GetAnimationData( i );
		if ( pData->iActivity == kActivityLookup_Unknown )
		{
			pData->iActivity = ActivityList_IndexForName( pData->pszActivity );
		}
		if ( pData && pData->iActivity == baseAct )
		{
			if ( CEconItemSchema::GetRandomStream().RandomFloat() < pData->flFrequency )
				return pData->pszReplacement;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CEconItemDefinition::GetReplacementSound( const char* pszSoundName ) const
{
	int iSounds = GetNumSounds();
	for ( int i=0; i<iSounds; i++ )
	{
		sound_on_wearable_t *pData = GetSoundData( i );
		if ( pData && FStrEq( pData->pszSound, pszSoundName ) )
			return pData->pszReplacement;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CEconItemDefinition::GetReplacementParticleEffect( const char* pszParticleName ) const
{
	int iParticles = GetNumParticles();
	for ( int i=0; i<iParticles; i++ )
	{
		particle_on_wearable_t *pData = GetParticleData( i );
		if ( pData && FStrEq( pData->pszParticle, pszParticleName ) )
			return pData->pszReplacement;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char *CEconItemDefinition::GetReplacementParticleSnapshot( const char* pszParticleSnapshotName ) const
{
	int iSnapshots = GetNumParticleSnapshots();
	for ( int i=0; i<iSnapshots; i++ )
	{
		particlesnapshot_on_wearable_t *pData = GetParticleSnapshotData( i );
		if ( pData && FStrEq( pData->pszParticleSnapshot, pszParticleSnapshotName ) )
			return pData->pszReplacement;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconItemDefinition::GetReplacementControlPoint( int nIndex, const char* pszParticleName, int &nOutputCP, Vector &vecCPValue ) const
{
	int iParticles = GetNumParticleControlPoints();
	if ( nIndex < iParticles )
	{
		particle_control_point_on_wearable_t *pData = GetParticleControlPointData( nIndex );
		if ( pData && FStrEq( pData->pszParticle, pszParticleName ) )
		{ 
			nOutputCP = pData->nParticleControlPoint;
			vecCPValue = pData->vecCPValue;
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the content for this item view should be streamed. If false,
//			it should be preloaded.
//-----------------------------------------------------------------------------

// DO NOT MERGE THIS CONSOLE VARIABLE TO REL WE SHOULD NOT SHIP THIS OH GOD
#define ITEM_ENABLE_DYNAMIC_LOADING true
//ConVar item_enable_dynamic_loading( "item_enable_dynamic_loading", "1", FCVAR_REPLICATED, "Enable/disable dynamic streaming of econ content." );

bool CEconItemDefinition::IsContentStreamable() const
{
	if ( !BLoadOnDemand() )
		return false;

	return ITEM_ENABLE_DYNAMIC_LOADING;
}
#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char* CEconItemDefinition::GetAlternateIcon( int iAlternateIcon ) const
{
	int iIdx = m_pMapAlternateIcons->Find( iAlternateIcon );
	if ( !m_pMapAlternateIcons->IsValidIndex( iIdx ) )
		return NULL;
	else
		return m_pMapAlternateIcons->Element( iIdx );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Sticker model paths
//-----------------------------------------------------------------------------
const int CEconItemDefinition::GetNumSupportedStickerSlots() const
{
	return m_vStickerModels.Count();
}
const char *CEconItemDefinition::GetStickerSlotModelBySlotIndex( uint32 index ) const
{
	return m_vStickerModels[index].m_szStickerModelPath;
}
const Vector &CEconItemDefinition::GetStickerSlotWorldProjectionStartBySlotIndex( uint32 index ) const
{
	return m_vStickerModels[index].m_vWorldModelProjectionStart;
}
const Vector &CEconItemDefinition::GetStickerSlotWorldProjectionEndBySlotIndex( uint32 index ) const
{
	return m_vStickerModels[index].m_vWorldModelProjectionEnd;
}
const char *CEconItemDefinition::GetStickerWorldModelBoneParentNameBySlotIndex( uint32 index ) const
{
	return m_vStickerModels[index].m_szStickerBoneParentName;
}
const char *CEconItemDefinition::GetStickerSlotMaterialBySlotIndex( uint32 index ) const
{
	return m_vStickerModels[index].m_szStickerMaterialPath;
}

RETURN_ATTRIBUTE_STRING_F( CEconItemDefinition::GetIconDisplayModel, "icon display model", m_pszWorldDisplayModel );
RETURN_ATTRIBUTE_STRING_F( CEconItemDefinition::GetBuyMenuDisplayModel, "buymenu display model", m_pszWorldDisplayModel );
RETURN_ATTRIBUTE_STRING_F( CEconItemDefinition::GetPedestalDisplayModel, "pedestal display model", m_pszBaseDisplayModel );
RETURN_ATTRIBUTE_STRING_F( CEconItemDefinition::GetMagazineModel, "magazine model", NULL );
RETURN_ATTRIBUTE_STRING_F( CEconItemDefinition::GetUidModel, "uid model", "models/weapons/uid.mdl" );
RETURN_ATTRIBUTE_STRING_F( CEconItemDefinition::GetScopeLensMaskModel, "aimsight lens mask", NULL );

const char *CEconItemDefinition::GetStatTrakModelByType( uint32 nType ) const
{
	const kill_eater_score_type_t *pScoreType = GetItemSchema()->FindKillEaterScoreType( nType );
	if ( !pScoreType )
	{
		pScoreType = GetItemSchema()->FindKillEaterScoreType( 0 );
	}

	if ( !pScoreType )
		return NULL;

	CSchemaAttributeDefHandle pAttrib_StatTrakModel( pScoreType->m_pszModelAttributeString );
	const char *pchStatTrakModel = NULL;
	FindAttribute_UnsafeBitwiseCast< CAttribute_String >( this, pAttrib_StatTrakModel, &pchStatTrakModel );

	return pchStatTrakModel;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTimedItemRewardDefinition::CTimedItemRewardDefinition( void )
:	m_unMinFreq( 0 ),
	m_unMaxFreq( UINT_MAX ),
	m_rtForcedBaselineAdjustment( 0 ),
	m_rtForcedLastDropTimeAdjustment( 0 ),
	m_unHoursInRewardPeriod( 0 ),
	m_unHoursBetweenDropsRealtime( 0 ),
	m_unPointsPerHourOverplayed( 0 ),
	m_flChance( 0.0f )
{
}


//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CTimedItemRewardDefinition::CTimedItemRewardDefinition( const CTimedItemRewardDefinition &that )
{
	(*this) = that;
}


//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CTimedItemRewardDefinition &CTimedItemRewardDefinition::operator=( const CTimedItemRewardDefinition &rhs )
{
	m_unMinFreq = rhs.m_unMinFreq;
	m_unMaxFreq = rhs.m_unMaxFreq;
	m_rtForcedBaselineAdjustment = rhs.m_rtForcedBaselineAdjustment;
	m_rtForcedLastDropTimeAdjustment = rhs.m_rtForcedLastDropTimeAdjustment;
	m_unHoursInRewardPeriod = rhs.m_unHoursInRewardPeriod;
	m_unHoursBetweenDropsRealtime = rhs.m_unHoursBetweenDropsRealtime;
	m_unPointsPerHourOverplayed = rhs.m_unPointsPerHourOverplayed;
	m_arrTotalPointsBasedOnHoursPlayed.RemoveAll();
	m_arrTotalPointsBasedOnHoursPlayed.EnsureCapacity( rhs.m_arrTotalPointsBasedOnHoursPlayed.Count() );
	m_arrTotalPointsBasedOnHoursPlayed.AddMultipleToTail( rhs.m_arrTotalPointsBasedOnHoursPlayed.Count(), rhs.m_arrTotalPointsBasedOnHoursPlayed.Base() );
	m_criteria = rhs.m_criteria;
	m_arrLootLists.CopyArray( rhs.m_arrLootLists.Base(), rhs.m_arrLootLists.Count() );
	m_flChance = rhs.m_flChance;

	return *this;
}

CTimedItemRewardDefinition::~CTimedItemRewardDefinition()
{
	m_arrDynamicLootLists.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose:	Initialize the attribute definition
// Input:	pKVTimedReward - The KeyValues representation of the timed reward
//			schema - The overall item schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CTimedItemRewardDefinition::BInitFromKV( KeyValues *pKVTimedReward, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	// Parse the basic values
	m_flChance = pKVTimedReward->GetFloat( "pctChance" );
	
#ifdef DOTA_DLL
	m_unMinFreq = pKVTimedReward->GetInt( "value_min", 0 );
	m_unMaxFreq = pKVTimedReward->GetInt( "value_max", UINT_MAX );

	// Check required fields
	SCHEMA_INIT_CHECK( 
		NULL != pKVTimedReward->FindKey( "value_min" ), 
		CFmtStr( "Time reward %s: Missing required field \"value_min\"", pKVTimedReward->GetName() ) );
	SCHEMA_INIT_CHECK( 
		NULL != pKVTimedReward->FindKey( "value_max" ), 
		CFmtStr( "Time reward %s: Missing required field \"value_max\"", pKVTimedReward->GetName() ) );
#endif

	//
	// Parse the basic values
	//
	SCHEMA_INIT_CHECK( 
		NULL != pKVTimedReward->FindKey( "force_baseline_timestamp" ), 
		CFmtStr( "Time reward %s: Missing required field \"force_baseline_timestamp\"", pKVTimedReward->GetName() ) );
	m_rtForcedBaselineAdjustment = pKVTimedReward->GetInt( "force_baseline_timestamp" );

	m_rtForcedLastDropTimeAdjustment = 0;
	if ( pKVTimedReward->FindKey( "force_lastdrop_timestamp" ) )
		m_rtForcedLastDropTimeAdjustment = pKVTimedReward->GetInt( "force_lastdrop_timestamp" );
	
	m_unHoursInRewardPeriod = pKVTimedReward->GetInt( "period_hours", 0 );
	SCHEMA_INIT_CHECK( 
		NULL != pKVTimedReward->FindKey( "period_hours" ), 
		CFmtStr( "Time reward %s: Missing required field \"period_hours\"", pKVTimedReward->GetName() ) );
	SCHEMA_INIT_CHECK( 
		m_unHoursInRewardPeriod > 0, 
		CFmtStr( "Time reward %s: Required field \"period_hours\" has invalid value", pKVTimedReward->GetName() ) );

	m_unHoursBetweenDropsRealtime = pKVTimedReward->GetInt( "drop_interval_hours", 0 );
	SCHEMA_INIT_CHECK( 
		NULL != pKVTimedReward->FindKey( "drop_interval_hours" ), 
		CFmtStr( "Time reward %s: Missing required field \"drop_interval_hours\"", pKVTimedReward->GetName() ) );
	SCHEMA_INIT_CHECK( 
		m_unHoursBetweenDropsRealtime >= 0, 
		CFmtStr( "Time reward %s: Required field \"drop_interval_hours\" has invalid value", pKVTimedReward->GetName() ) );

	SCHEMA_INIT_CHECK( 
		NULL != pKVTimedReward->FindKey( "points_progression_hourly" ), 
		CFmtStr( "Time reward %s: Missing required field \"points_progression_hourly\"", pKVTimedReward->GetName() ) );
	uint32 numPointsAccumulatedValidation = 0;
	for ( KeyValues *kvPoints = pKVTimedReward->FindKey( "points_progression_hourly" )->GetFirstSubKey(); kvPoints; kvPoints = kvPoints->GetNextKey() )
	{
		uint32 numPoints = kvPoints->GetInt();
		SCHEMA_INIT_CHECK( 
			numPoints > numPointsAccumulatedValidation,
			CFmtStr( "Time reward %s: Required field \"points_progression_hourly\" defines invalid points after %u hours", pKVTimedReward->GetName(), m_arrTotalPointsBasedOnHoursPlayed.Count() ) );
		m_arrTotalPointsBasedOnHoursPlayed.AddToTail( numPoints );
		numPointsAccumulatedValidation = numPoints;
	}
	SCHEMA_INIT_CHECK( 
		NULL != pKVTimedReward->FindKey( "points_per_additional_hour" ), 
		CFmtStr( "Time reward %s: Missing required field \"points_per_additional_hour\"", pKVTimedReward->GetName() ) );
	m_unPointsPerHourOverplayed = pKVTimedReward->GetInt( "points_per_additional_hour", 0 );

	SCHEMA_INIT_CHECK( 
		NULL != pKVTimedReward->FindKey( "period_points_rollover" ), 
		CFmtStr( "Time reward %s: Missing required field \"period_points_rollover\"", pKVTimedReward->GetName() ) );
	m_unPointsPerPeriodRollover = pKVTimedReward->GetInt( "period_points_rollover", 0 );
	SCHEMA_INIT_CHECK( 
		m_unPointsPerPeriodRollover >= numPointsAccumulatedValidation,
		CFmtStr( "Time reward %s: Required field \"period_points_rollover\" defines invalid points after %u hours", pKVTimedReward->GetName(), m_arrTotalPointsBasedOnHoursPlayed.Count() ) );

	SCHEMA_INIT_CHECK( 
		NULL != pKVTimedReward->FindKey( "pctChance" ), 
		CFmtStr( "Time reward %s: Missing required field \"pctChance\"", pKVTimedReward->GetName() ) );
	
	SCHEMA_INIT_CHECK( 
		( m_flChance >= 0.0f ) && ( m_flChance <= 1.0f ),
		CFmtStr( "Time reward %s: Required field \"pctChance\" has invalid value", pKVTimedReward->GetName() ) );

	// Parse the criteria or loot_list
	if ( pKVTimedReward->FindKey( "criteria" ) )
	{
		bool bCriteriaOK = m_criteria.BInitFromKV( pKVTimedReward->FindKey( "criteria", true ), pschema );
		SCHEMA_INIT_CHECK( bCriteriaOK, CFmtStr( "Time Reward %s: Invalid criteria", pKVTimedReward->GetName() ) );

		// Check to make sure this criteria doesn't filter to an empty set
		if ( bCriteriaOK )
		{
			bool bMatch = false;
			FOR_EACH_MAP_FAST( pschema.GetItemDefinitionMap(), i )
			{
				if ( m_criteria.BEvaluate( pschema.GetItemDefinitionMap()[i], pschema ) )
				{
					bMatch = true;
					break;
				}
			}

			SCHEMA_INIT_CHECK(
				bMatch,
				CFmtStr( "Time Reward %s: No items match the critera", pKVTimedReward->GetName() ) );
		}
	}

	const char *pszLootList = pKVTimedReward->GetString("loot_list", NULL);
	if ( pszLootList && pszLootList[0] )
	{
		CUtlVector< char * > arrLootListsTokens;
		V_SplitString( pszLootList, ",", arrLootListsTokens );

		SCHEMA_INIT_CHECK( 
			arrLootListsTokens.Count() != 0,
			CFmtStr( "Time Reward %s: loot_list (%s) has zero elements", pKVTimedReward->GetName(), pszLootList ) );

		FOR_EACH_VEC( arrLootListsTokens, iLootListToken )
		{
			const CEconLootListDefinition *pLootList = pschema.GetLootListByName( arrLootListsTokens[iLootListToken] );

			// Make sure the item index is correct because we use this index as a reference
			SCHEMA_INIT_CHECK( 
				NULL != pLootList,
				CFmtStr( "Time Reward %s: loot_list (%s) element '%s' does not exist", pKVTimedReward->GetName(), pszLootList, arrLootListsTokens[iLootListToken] ) );

			if ( pLootList )
				m_arrLootLists.AddToTail( pLootList );
		}

		arrLootListsTokens.PurgeAndDeleteElements();
	}

	return SCHEMA_INIT_SUCCESS();
}

float CTimedItemRewardDefinition::GetChance( void ) const
{
	return m_flChance;
}

const CSchemaAttributeDefHandle& GetCampaignAttributeDefHandle( int nCampaignID, ECampaignAttributeType type )
{
	static CSchemaAttributeDefHandle* pAttrCampaignCompletionBitfield[ g_nNumCampaigns + 1 ];
	static CSchemaAttributeDefHandle* pAttrCampaignLastCompletedQuest[ g_nNumCampaigns + 1 ];

	static bool s_bCampaignAttrInitialized = false;
	static bool s_bCampaignAttrValid = false;

	if ( !s_bCampaignAttrInitialized )
	{
		s_bCampaignAttrInitialized = true;
		s_bCampaignAttrValid = true;
		for ( int j = 1; j <= g_nNumCampaigns; ++j )
		{
			pAttrCampaignCompletionBitfield[ j ]= new CSchemaAttributeDefHandle( ( new CFmtStr( "campaign %d completion bitfield", j ) )->Access() );
			if ( !( *pAttrCampaignCompletionBitfield[ j ] ) )
				s_bCampaignAttrValid = false;

			pAttrCampaignLastCompletedQuest[ j ] = new CSchemaAttributeDefHandle( ( new CFmtStr( "campaign %d last completed quest", j ) )->Access() );
			if ( !( *pAttrCampaignLastCompletedQuest[ j ] ) )
				s_bCampaignAttrValid = false;
		}		
	}


	if ( ( nCampaignID <= 0 ) || ( nCampaignID > g_nNumCampaigns ) )
	{
		Assert( 0 );
		Warning( "Attempting to lookup campaign %d, which does not exist. Verify that the code recognizes that campaigns are 1-based.", nCampaignID );
	}
	else if ( s_bCampaignAttrValid )
	{
		switch( type )
		{
		case k_ECampaignAttribute_CompletionBitfield:
			return *pAttrCampaignCompletionBitfield[ nCampaignID ];

		case k_ECampaignAttribute_LastCompletedQuest:
			return *pAttrCampaignLastCompletedQuest[ nCampaignID ];

		default:
			Assert( 0 );
			Warning( "Attempting to lookup campaign %d type %d, which does not exist.", nCampaignID, type );
			break;
		}
		
	}

	static CSchemaAttributeDefHandle s_DummyAttr( "undefined" );
	return s_DummyAttr;
}


const CSchemaAttributeDefHandle& GetStickerAttributeDefHandle( int attrNum, EStickerAttributeType type )
{
	// Attributes of the schema validation
	static bool s_bStickerAttrsSetup = false;
	static bool s_bStickerAttrsValid = false;
	static CSchemaAttributeDefHandle* pAttrStickerSlotID[ g_nNumStickerAttrs ];
	static CSchemaAttributeDefHandle* pAttrStickerSlotWear[ g_nNumStickerAttrs ];
	static CSchemaAttributeDefHandle* pAttrStickerSlotScale[ g_nNumStickerAttrs ];
	static CSchemaAttributeDefHandle* pAttrStickerSlotRotation[ g_nNumStickerAttrs ];

	if ( !s_bStickerAttrsSetup )
	{
		s_bStickerAttrsSetup = true;
		s_bStickerAttrsValid = true;
		for ( int j = 0; j < g_nNumStickerAttrs; ++j )
		{
			pAttrStickerSlotID[ j ] = new CSchemaAttributeDefHandle( ( new CFmtStr( "sticker slot %d id", j ) )->Access() );
			if ( !( *pAttrStickerSlotID[ j ] ) ) s_bStickerAttrsValid = false;
			pAttrStickerSlotWear[ j ] = new CSchemaAttributeDefHandle( ( new CFmtStr( "sticker slot %d wear", j ) )->Access() );
			if ( !( *pAttrStickerSlotWear[ j ] ) ) s_bStickerAttrsValid = false;
			pAttrStickerSlotScale[ j ] = new CSchemaAttributeDefHandle( ( new CFmtStr( "sticker slot %d scale", j ) )->Access() );
			if ( !( *pAttrStickerSlotScale[ j ] ) ) s_bStickerAttrsValid = false;
			pAttrStickerSlotRotation[ j ] = new CSchemaAttributeDefHandle( ( new CFmtStr( "sticker slot %d rotation", j ) )->Access() );

			if ( !( *pAttrStickerSlotRotation[ j ] ) ) s_bStickerAttrsValid = false;
		}
	}

	if ( s_bStickerAttrsValid )
	{
		switch ( type )
		{
			case k_EStickerAttribute_ID:
				return *pAttrStickerSlotID[ attrNum ];
				break;
			case k_EStickerAttribute_Wear:
				return *pAttrStickerSlotWear[ attrNum ];
				break;
			case k_EStickerAttribute_Scale:
				return *pAttrStickerSlotScale[ attrNum ];
				break;
			case k_EStickerAttribute_Rotation:
				return *pAttrStickerSlotRotation[ attrNum ];
				break;

			default:
				Assert( 0 );
				break;
		};
	}

	static CSchemaAttributeDefHandle s_DummyAttr( "undefined" );
	return s_DummyAttr;
}

bool CStickerKit::InitFromKeyValues( KeyValues *pKVEntry, const CStickerKit *pDefault, CUtlVector<CUtlString> *pVecErrors /*= NULL*/ )
{
	sName.Set( pKVEntry->GetString( "name", pDefault->sName.String() ) );
	sDescriptionString.Set( pKVEntry->GetString( "description_string", pDefault->sDescriptionString.String() ) );
	sItemName.Set( pKVEntry->GetString( "item_name", pDefault->sItemName.String() ) );

	uint8 ucRarity;
	GetItemSchema()->BGetItemRarityFromName( pKVEntry->GetString( "item_rarity", "common" ), &ucRarity );

	nRarity = ucRarity;

	sMaterialPath.Set( pKVEntry->GetString( "sticker_material", pDefault->sMaterialPath.String() ) );
	if ( *sMaterialPath.String() )
	{
		sMaterialPathNoDrips.Set( pKVEntry->GetString( "sticker_material_nodrips", CFmtStr( "%s_nodrips", sMaterialPath.String() ) ) );
	}

	m_strInventoryImage.Set( pKVEntry->GetString( "image_inventory", CFmtStr( "econ/stickers/%s", sMaterialPath.String() ) ) );

	SetIconURLSmall( GetInventoryImage() );
	SetIconURLLarge( CFmtStr( "%s_large", GetInventoryImage() ) );

	//
	// gc_generation_settings
	//

	flRotateStart = pKVEntry->GetFloat( "gc_generation_settings/rotate_start", pDefault->flRotateStart );
	flRotateEnd = pKVEntry->GetFloat( "gc_generation_settings/rotate_end", pDefault->flRotateEnd );
	SCHEMA_INIT_CHECK( 
		( flRotateStart >= -360.0f ) && ( flRotateStart <= flRotateEnd ) && ( flRotateEnd <= 360.0f ),
		CFmtStr( "Sticker kit '%s' rotate range is not valid %f:%f", pKVEntry->GetName(), flRotateStart, flRotateEnd ) );

	flScaleMin = pKVEntry->GetFloat( "gc_generation_settings/scale_min", pDefault->flScaleMin );
	flScaleMax = pKVEntry->GetFloat( "gc_generation_settings/scale_max", pDefault->flScaleMax );
	SCHEMA_INIT_CHECK( // Scale is in UV space so max is a smaller float, min is a larger float
		( flScaleMax > 0.0f ) && ( flScaleMax <= flScaleMin ),
		CFmtStr( "Sticker kit '%s' scale range is not valid %f:%f", pKVEntry->GetName(), flScaleMax, flScaleMin ) );

	flWearMin = pKVEntry->GetFloat( "gc_generation_settings/wear_min", pDefault->flWearMin );
	flWearMax = pKVEntry->GetFloat( "gc_generation_settings/wear_max", pDefault->flWearMax );
	SCHEMA_INIT_CHECK( 
		( flWearMin >= 0.0f ) && ( flWearMin <= flWearMax ) && ( flWearMax <= 1.0f ),
		CFmtStr( "Sticker kit '%s' wear range is not valid %f:%f", pKVEntry->GetName(), flWearMin, flWearMax ) );

	m_nEventID = pKVEntry->GetInt( "tournament_event_id", pDefault->m_nEventID );
	m_nEventTeamID = pKVEntry->GetInt( "tournament_team_id", pDefault->m_nEventTeamID );
	m_nPlayerID = pKVEntry->GetInt( "tournament_player_id", pDefault->m_nPlayerID );

	m_pKVItem = pKVEntry ? pKVEntry->MakeCopy() : NULL;
		
	return true;
}

bool CStickerList::InitFromKeyValues( KeyValues *pKVEntry, CUtlVector<CUtlString> *pVecErrors /*= NULL*/ )
{
	flWearMin = pKVEntry->GetFloat( "gc_generation_settings/wear_min", 0.0f );
	flWearMax = pKVEntry->GetFloat( "gc_generation_settings/wear_max", 1.0f );
	SCHEMA_INIT_CHECK( 
		( flWearMin >= 0.0f ) && ( flWearMin <= flWearMax ) && ( flWearMax <= 1.0f ),
		CFmtStr( "Sticker list '%s' wear range is not valid %f:%f", pKVEntry->GetName(), flWearMin, flWearMax ) );

	flTotalWeight = 0.0f;

	float flLargestMinWear = 0.0f;
	float flSmallestMaxWear = 1.0f;

	for ( KeyValues *kvChild = pKVEntry->GetFirstValue(); kvChild; kvChild = kvChild->GetNextValue() )
	{
		char const *szName = kvChild->GetName();
		const CStickerKit *pStickerKit = GetItemSchema()->GetStickerKitDefinitionByName( szName );
		const CStickerList *pStickerList = GetItemSchema()->GetStickerListDefinitionByName( szName );
		
		SCHEMA_INIT_CHECK( 
			( pStickerKit || pStickerList ) && !( pStickerKit && pStickerList ),
			CFmtStr( "Sticker list '%s' refers to an unknown stickerkit or stickerlist '%s'", pKVEntry->GetName(), szName ) );

		float flWeight = kvChild->GetFloat();
		SCHEMA_INIT_CHECK( 
			( flWeight > 0.0f ),
			CFmtStr( "Sticker list '%s' has invalid weight %f", pKVEntry->GetName(), flWeight ) );

		if ( pStickerKit )
		{
			flLargestMinWear = MAX( flLargestMinWear, pStickerKit->flWearMin );
			flSmallestMaxWear = MIN( flSmallestMaxWear, pStickerKit->flWearMax );
		}
		
		if ( pStickerList )
		{
			flLargestMinWear = MAX( flLargestMinWear, pStickerList->flWearMin );
			flSmallestMaxWear = MIN( flSmallestMaxWear, pStickerList->flWearMax );
		}

		// add the entry
		sticker_list_entry_t entry;
		entry.pKit = pStickerKit;
		entry.pList = pStickerList;
		entry.flWeight = flWeight;
		
		arrElements.AddToTail( entry );
		flTotalWeight += entry.flWeight;
	}

	SCHEMA_INIT_CHECK( 
		( arrElements.Count() > 0 ),
		CFmtStr( "Sticker list '%s' has no elements", pKVEntry->GetName() ) );

	SCHEMA_INIT_CHECK( 
		( flTotalWeight > 0.0f ),
		CFmtStr( "Sticker list '%s' has no elements weight", pKVEntry->GetName() ) );

	SCHEMA_INIT_CHECK( 
		( flSmallestMaxWear >= flWearMin ),
		CFmtStr( "Sticker list '%s' wear_min %f exceeds elements max range wear %f", pKVEntry->GetName(), flWearMin, flSmallestMaxWear ) );
	
	SCHEMA_INIT_CHECK( 
		( flLargestMinWear <= flWearMax ),
		CFmtStr( "Sticker list '%s' wear_max %f below elements min range wear %f", pKVEntry->GetName(), flWearMax, flLargestMinWear ) );

	return true;
}

bool CStickerKit::GenerateStickerApplicationInfo( CAppliedStickerInfo_t *pInfo ) const
{
	if ( !pInfo )
		return false;

	pInfo->flWearMin = MAX( pInfo->flWearMin, flWearMin );
	pInfo->flWearMax = MIN( pInfo->flWearMax, flWearMax );
	if ( pInfo->flWearMin > pInfo->flWearMax )
		return false;

	pInfo->nID = nID;
	pInfo->flScale = CEconItemSchema::GetRandomStream().RandomFloat( flScaleMax, flScaleMin );
	pInfo->flRotate = CEconItemSchema::GetRandomStream().RandomFloat( flRotateStart, flRotateEnd );

	return true;
}

bool CStickerList::GenerateStickerApplicationInfo( CAppliedStickerInfo_t *pInfo ) const
{
	if ( !pInfo )
		return false;

	// Generate a random roll into our list
	float flRand = CEconItemSchema::GetRandomStream().RandomFloat(0.f, 1.f) * flTotalWeight;

	float flAccum = 0.f;
	for ( int i=0; i<arrElements.Count(); ++i )
	{
		flAccum += arrElements[i].flWeight;
		if ( flRand <= flAccum )
		{
			const sticker_list_entry_t &entry = arrElements[i];
			
			pInfo->flWearMin = MAX( pInfo->flWearMin, flWearMin );
			pInfo->flWearMax = MIN( pInfo->flWearMax, flWearMax );
			if ( pInfo->flWearMin > pInfo->flWearMax )
				return false;

			if ( entry.pList )
				return entry.pList->GenerateStickerApplicationInfo( pInfo );
			
			if ( entry.pKit )
				return entry.pKit->GenerateStickerApplicationInfo( pInfo );

			return false;
		}
	}

	return false;
}


bool CPaintKit::InitFromKeyValues( KeyValues *pKVEntry, const CPaintKit *pDefault, bool bHandleAbsolutePaths )
{
	sName.Set( pKVEntry->GetString( "name", pDefault->sName.String() ) );
	sDescriptionString.Set( pKVEntry->GetString( "description_string", pDefault->sDescriptionString.String() ) );
	sDescriptionTag.Set( pKVEntry->GetString( "description_tag", pDefault->sDescriptionTag.String() ) );
	nRarity = 1;	// rarities set in the paint_kits_rarity block

	// Character paint kit fields
	sVmtPath.Set( pKVEntry->GetString( "vmt_path", pDefault->sVmtPath.String() ) );

	if ( kvVmtOverrides != nullptr )
	{
		kvVmtOverrides->deleteThis();
		kvVmtOverrides = nullptr;
	}

	KeyValues* pVmtOverrides = pKVEntry->FindKey( "vmt_overrides" );
	if ( pVmtOverrides != nullptr )
	{
		kvVmtOverrides = new KeyValues( "CustomCharacter" );
		kvVmtOverrides->MergeFrom( pVmtOverrides, KeyValues::MERGE_KV_UPDATE );
	}

	// Regular paint kit fields
	if (bHandleAbsolutePaths)
	{
		// this solution is less than ideal, it'll only work for windows drive paths (not network paths), and in 
		// order to make this better we need to change things down inside the material system / texture code.
		// it's only used for the workshop preview con command right now.
		const char* pPatternPath = pKVEntry->GetString( "pattern", pDefault->sPattern.String() );
#ifdef PLATFORM_WINDOWS
		int nLength = V_strlen( pPatternPath );
		if ( nLength > 2 && !( pPatternPath[0] == '/' && pPatternPath[1] == '/' && pPatternPath[2] != '/' ) && V_IsAbsolutePath( pPatternPath ) )
		{
			sPattern.Format( "//./%s", pPatternPath );
		}
		else
#endif
		{
			sPattern.Set( pPatternPath );
		}
	}
	else
	{
		sPattern.Set( pKVEntry->GetString( "pattern", pDefault->sPattern.String() ) );
	}

	sLogoMaterial.Set( pKVEntry->GetString( "logo_material", pDefault->sLogoMaterial.String() ) );

	nStyle = pKVEntry->GetInt( "style", pDefault->nStyle );
	
	flWearDefault = pKVEntry->GetFloat( "wear_default", pDefault->flWearDefault );
	flWearRemapMin = pKVEntry->GetFloat( "wear_remap_min", pDefault->flWearRemapMin );
	flWearRemapMax = pKVEntry->GetFloat( "wear_remap_max", pDefault->flWearRemapMax );
	nFixedSeed = pKVEntry->GetInt( "seed", pDefault->nFixedSeed );
	uchPhongExponent = pKVEntry->GetInt( "phongexponent", pDefault->uchPhongExponent );
	uchPhongAlbedoBoost = pKVEntry->GetInt( "phongalbedoboost", static_cast< int >( pDefault->uchPhongAlbedoBoost ) - 1 ) + 1;
	uchPhongIntensity = pKVEntry->GetInt( "phongintensity", pDefault->uchPhongIntensity );
	flPatternScale = pKVEntry->GetFloat( "pattern_scale", pDefault->flPatternScale );
	flPatternOffsetXStart = pKVEntry->GetFloat( "pattern_offset_x_start", pDefault->flPatternOffsetXStart );
	flPatternOffsetXEnd = pKVEntry->GetFloat( "pattern_offset_x_end", pDefault->flPatternOffsetXEnd );
	flPatternOffsetYStart = pKVEntry->GetFloat( "pattern_offset_y_start", pDefault->flPatternOffsetYStart );
	flPatternOffsetYEnd = pKVEntry->GetFloat( "pattern_offset_y_end", pDefault->flPatternOffsetYEnd );
	flPatternRotateStart = pKVEntry->GetFloat( "pattern_rotate_start", pDefault->flPatternRotateStart );
	flPatternRotateEnd = pKVEntry->GetFloat( "pattern_rotate_end", pDefault->flPatternRotateEnd );
	flLogoScale = pKVEntry->GetFloat( "logo_scale", pDefault->flLogoScale );
	flLogoOffsetX = pKVEntry->GetFloat( "logo_offset_x", pDefault->flLogoOffsetX );
	flLogoOffsetY = pKVEntry->GetFloat( "logo_offset_y", pDefault->flLogoOffsetY );
	flLogoRotation = pKVEntry->GetFloat( "logo_rotation", pDefault->flLogoRotation );
	bIgnoreWeaponSizeScale = pKVEntry->GetBool( "ignore_weapon_size_scale", pDefault->bIgnoreWeaponSizeScale );
	nViewModelExponentOverrideSize = pKVEntry->GetInt( "view_model_exponent_override_size", pDefault->nViewModelExponentOverrideSize );
	bOnlyFirstMaterial = pKVEntry->GetBool( "only_first_material", pDefault->bOnlyFirstMaterial );

	char szColorName[ 16 ];
	for ( int nColor = 0; nColor < CPaintKit::NUM_COLORS; ++nColor )
	{
		// Regular Colors
		V_snprintf( szColorName, sizeof( szColorName ), "color%i", nColor );

		KeyValues *pKVColor = pKVEntry->FindKey( szColorName );
		if ( pKVColor )
		{
			const char *pchColor = pKVColor->GetString();
			unsigned int cR, cG, cB;
			sscanf( pchColor, "%u %u %u", &cR, &cG, &cB );
			rgbaColor[ nColor ].SetColor( cR, cG, cB, 255 );
		}
		else
		{
			rgbaColor[ nColor ] = pDefault->rgbaColor[ nColor ];
		}

		// Logo Colors
		V_snprintf( szColorName, sizeof( szColorName ), "logocolor%i", nColor );

		pKVColor = pKVEntry->FindKey( szColorName );
		if ( pKVColor )
		{
			const char *pchColor = pKVColor->GetString();
			unsigned int cR, cG, cB;
			sscanf( pchColor, "%u %u %u", &cR, &cG, &cB );
			rgbaLogoColor[ nColor ].SetColor( cR, cG, cB, 255 );
		}
		else
		{
			rgbaLogoColor[ nColor ] = pDefault->rgbaLogoColor[ nColor ];
		}
	}

	return true;
}

void CPaintKit::FillKeyValuesForWorkshop( KeyValues *pKVToFill ) const
{
	pKVToFill->SetInt( "style", nStyle );
	pKVToFill->SetString( "pattern", sPattern.String() );

	char szColorName[ 16 ];
	for ( int nColor = 0; nColor < CPaintKit::NUM_COLORS; ++nColor )
	{
		// Regular Colors
		V_snprintf( szColorName, sizeof( szColorName ), "color%i", nColor );
		pKVToFill->SetColor( szColorName, rgbaColor[ nColor ] );
	}

	pKVToFill->SetFloat( "pattern_scale", flPatternScale );
	pKVToFill->SetFloat( "pattern_offset_x_start", flPatternOffsetXStart );
	pKVToFill->SetFloat( "pattern_offset_x_end", flPatternOffsetXEnd );
	pKVToFill->SetFloat( "pattern_offset_y_start", flPatternOffsetYStart );
	pKVToFill->SetFloat( "pattern_offset_y_end", flPatternOffsetYEnd );
	pKVToFill->SetFloat( "pattern_rotate_start", flPatternRotateStart );
	pKVToFill->SetFloat( "pattern_rotate_end", flPatternRotateEnd );

	pKVToFill->SetFloat( "wear_remap_min", flWearRemapMin );
	pKVToFill->SetFloat( "wear_remap_max", flWearRemapMax );
	pKVToFill->SetInt( "phongexponent", uchPhongExponent );
	pKVToFill->SetInt( "phongalbedoboost", static_cast< int >( uchPhongAlbedoBoost ) - 1 );
	pKVToFill->SetInt( "phongintensity", uchPhongIntensity );
	pKVToFill->SetBool( "ignore_weapon_size_scale", bIgnoreWeaponSizeScale );
	pKVToFill->SetInt( "view_model_exponent_override_size", nViewModelExponentOverrideSize );
	pKVToFill->SetBool( "only_first_material", bOnlyFirstMaterial );

	//pKVToFill->SetString( "logo_material", sLogoMaterial.String() );
	//pKVToFill->SetFloat( "logo_scale", flLogoScale );
	//pKVToFill->SetFloat( "logo_offset_x", flLogoOffsetX );
	//pKVToFill->SetFloat( "logo_offset_y", flLogoOffsetY );
	//pKVToFill->SetFloat( "logo_rotation", flLogoRotation );

	//for ( int nColor = 0; nColor < CPaintKit::NUM_COLORS; ++nColor )
	//{
	//	// Logo Colors
	//	V_snprintf( szColorName, sizeof( szColorName ), "logocolor%i", nColor );

	//	pKVToFill->SetColor( szColorName, rgbaLogoColor[ nColor ] );
	//}

	//pKVToFill->SetInt( "seed", nFixedSeed );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a foreign item definition to local definition mapping for a 
//			foreign app
//-----------------------------------------------------------------------------
void CForeignAppImports::AddMapping( uint16 unForeignDefIndex, const CEconItemDefinition *pDefn )
{
	m_mapDefinitions.InsertOrReplace( unForeignDefIndex, pDefn );
}


//-----------------------------------------------------------------------------
// Purpose: Adds a foreign item definition to local definition mapping for a 
//			foreign app
//-----------------------------------------------------------------------------
const CEconItemDefinition *CForeignAppImports::FindMapping( uint16 unForeignDefIndex ) const
{
	int i = m_mapDefinitions.Find( unForeignDefIndex );
	if( m_mapDefinitions.IsValidIndex( i ) )
		return m_mapDefinitions[i];
	else
		return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	Less function comparator for revolving loot lists map
//-----------------------------------------------------------------------------
static bool LLLessFunc( const int& e1, const int&e2 )
{
	return e1 < e2;
}

//-----------------------------------------------------------------------------
// Purpose:	Constructor
//-----------------------------------------------------------------------------
CEconItemSchema::CEconItemSchema( )
: 	m_unResetCount( 0 )
,	m_pKVRawDefinition( NULL )
,	m_unVersion( 0 )
,	m_pDefaultItemDefinition( NULL )
,	m_mapItemSets( DefLessFunc(const char*) )
,	m_mapDefinitionPrefabs( DefLessFunc(const char*) )
,	m_mapDefaultBodygroupState( DefLessFunc(const char*) )
,	m_bSchemaParsingItems(false)
#if defined(CLIENT_DLL) || defined(GAME_DLL)
,	m_pDelayedSchemaData( NULL )
#endif
,	m_mapKillEaterScoreTypes( DefLessFunc( unsigned int ) )
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IEconTool *CEconItemSchema::CreateEconToolImpl( const char *pszToolType, const char *pszUseString, const char *pszUsageRestriction, item_capabilities_t unCapabilities, KeyValues *pUsageKV )
{
	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose:	Resets the schema to before BInit was called
//-----------------------------------------------------------------------------
void CEconItemSchema::Reset( void )
{
	++m_unResetCount;

	m_unFirstValidClass = 0;
	m_unLastValidClass = 0;
	m_unFirstValidItemSlot = 0;
	m_unLastValidItemSlot = 0;
	m_unNumItemPresets = 0;
	m_unMinLevel = 0;
	m_unMaxLevel = 0;
	m_nMaxValidGraffitiTintDefID = 0;
	m_unSumQualityWeights = 0;
	m_mapRarities.Purge();
	m_mapSoundMaterials.Purge();
	m_mapQualities.Purge();
	m_mapItems.PurgeAndDeleteElements();
	m_mapItemsSorted.Purge();
	m_mapPaintKits.PurgeAndDeleteElements();
	m_mapStickerKits.PurgeAndDeleteElements();
	m_mapMusicDefs.PurgeAndDeleteElements();
	m_dictStickerKits.Purge();
	m_dictStickerLists.Purge();
	m_mapAttributesContainer.PurgeAndDeleteElements();
	FOR_EACH_VEC( m_vecAttributeTypes, i )
	{
		delete m_vecAttributeTypes[i].m_pAttrType;
	}
	m_vecAttributeTypes.Purge();
	m_mapRecipes.Purge();
	m_vecTimedRewards.Purge();
	m_mapAlternateIcons.Purge();
	m_mapItemSets.Purge();
	m_dictLootLists.Purge();
	m_mapAttributeControlledParticleSystems.Purge();
	m_unVersion = 0;
	if ( m_pKVRawDefinition )
	{
		m_pKVRawDefinition->deleteThis();
		m_pKVRawDefinition = NULL;
	}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	delete m_pDefaultItemDefinition;
	m_pDefaultItemDefinition = NULL;
#endif

	FOR_EACH_MAP_FAST( m_mapRecipes, i )
	{
		delete m_mapRecipes[i];
	}

	FOR_EACH_MAP_FAST( m_mapDefinitionPrefabs, i )
	{
		m_mapDefinitionPrefabs[i]->deleteThis();
	}
	m_mapDefinitionPrefabs.Purge();

	m_vecEquipRegionsList.Purge();
//	m_vecItemLevelingData.PurgeAndDeleteElements();
	m_vecItemLevelingData.Purge();

	m_RandomStream.SetSeed( 0 );
}


//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CEconItemSchema &CEconItemSchema::operator=( CEconItemSchema &rhs )
{
	Reset();
	BInitSchema( rhs.m_pKVRawDefinition );
	return *this;
}

#if defined( CLIENT_DLL )
#define MERGE_LIVE_PLAYERS_CODE 0
static void Helper_MergeKeyValuesUsingTemplate( KeyValues *kvInto, KeyValues *kvSrc, KeyValues *kvTemplate )
{
	FOR_EACH_SUBKEY( kvTemplate, kvTemplateSubkey )
	{
		if ( kvTemplateSubkey->GetDataType() == KeyValues::TYPE_NONE )
		{
			// This is a nested subkey
			if ( !V_strcmp( kvTemplateSubkey->GetName(), "*" ) )
			{
				// This is a wildcard subkey
				FOR_EACH_TRUE_SUBKEY( kvSrc, kvWildcardSrcSubkey )
				{
					KeyValues *pSubInto = kvInto->FindKey( kvWildcardSrcSubkey->GetNameSymbol() );
					if ( !pSubInto )
					{
#if MERGE_LIVE_PLAYERS_CODE
						kvInto->AddSubKey( pSubInto = kvWildcardSrcSubkey->MakeCopy() );
#else
						kvInto->AddSubKey( pSubInto = new KeyValues( kvWildcardSrcSubkey->GetName() ) );
#endif
					}
					Helper_MergeKeyValuesUsingTemplate( pSubInto, kvWildcardSrcSubkey, kvTemplateSubkey );
				}
			}
			else
			{
				// This is a direct dive into subkey
				KeyValues *pSubInto = kvInto->FindKey( kvTemplateSubkey->GetNameSymbol() );
				KeyValues *pSubSrc = kvSrc->FindKey( kvTemplateSubkey->GetNameSymbol() );
				if ( pSubInto && pSubSrc )
					Helper_MergeKeyValuesUsingTemplate( pSubInto, pSubSrc, kvTemplateSubkey );
			}
		}
		else
		{
			// This is a direct value copy
			if ( KeyValues *pSubSrc = kvSrc->FindKey( kvTemplateSubkey->GetNameSymbol() ) )
			{
				if ( KeyValues *pSubInto = kvInto->FindKey( kvTemplateSubkey->GetNameSymbol() ) )
				{
					// Cannot override an existing value!
					Assert( !"Cannot override existing schema value" );
				}
				else
				{
					// Make a copy and append
					kvInto->AddSubKey( pSubSrc->MakeCopy() );
				}
			}
		}
	}
}
#endif

//-----------------------------------------------------------------------------
// Initializes the schema, given KV filename
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInit( const char *fileName, const char *pathID, CUtlVector<CUtlString> *pVecErrors /* = NULL */)
{
	Reset();
	m_pKVRawDefinition = new KeyValues( "CEconItemSchema" );
	m_pKVRawDefinition->UsesEscapeSequences( true );
	if ( !m_pKVRawDefinition->LoadFromFile( g_pFullFileSystem, fileName, pathID ) )
	{
		m_pKVRawDefinition->deleteThis();
		m_pKVRawDefinition = NULL;
	}

	if ( m_pKVRawDefinition )
	{
#if defined( CLIENT_DLL )
		if ( KeyValues *kvLiveTemplate = m_pKVRawDefinition->FindKey( "items_game_live" ) )
		{
			// for client code we also load the live file to supplement certain portions of schema
			char chLiveFilename[MAX_PATH];
			V_strcpy_safe( chLiveFilename, fileName );
			if ( char *chExt = V_strrchr( chLiveFilename, '.' ) )
			{
				char chSuffix[ 64 ] = {};
				V_sprintf_safe( chSuffix, "_live%s", chExt );
				
				*chExt = 0;
				V_strcat_safe( chLiveFilename, chSuffix );

				KeyValues *kvLive = new KeyValues( "CEconItemSchema" );
				kvLive->UsesEscapeSequences( true );

				if ( kvLive->LoadFromFile( g_pFullFileSystem, chLiveFilename, pathID ) )
				{
					Helper_MergeKeyValuesUsingTemplate( m_pKVRawDefinition, kvLive, kvLiveTemplate );
				}

#if MERGE_LIVE_PLAYERS_CODE
				if ( KeyValues *kvDump = m_pKVRawDefinition->FindKey( "pro_players" ) )
				{
					kvDump->SaveToFile( g_pFullFileSystem, "pro_players_merged.txt" );
				}
#endif

				kvLive->deleteThis();
				kvLive = NULL;
			}
		}
#endif
		return BInitSchema( m_pKVRawDefinition, pVecErrors );
	}
	else
	{
#if !defined(CSTRIKE_DLL)
		Warning( "No %s file found. May be unable to create items.\n", fileName );
#endif // CSTRIKE_DLL
		return false;
	}	
}

//-----------------------------------------------------------------------------
// Initializes the schema, given KV in binary form
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitBinaryBuffer( CUtlBuffer &buffer, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	Reset();
	m_pKVRawDefinition = new KeyValues( "CEconItemSchema" );
	m_pKVRawDefinition->UsesEscapeSequences( true );
	if ( m_pKVRawDefinition->ReadAsBinary( buffer ) )
	{
		return BInitSchema( m_pKVRawDefinition, pVecErrors );
	}
	if ( pVecErrors )
	{
		pVecErrors->AddToTail( "Error parsing keyvalues" );
	}
	return false;
}

//-----------------------------------------------------------------------------
// Initializes the schema, given KV in text form
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitTextBuffer( CUtlBuffer &buffer, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	Reset();
	m_pKVRawDefinition = new KeyValues( "CEconItemSchema" );
	m_pKVRawDefinition->UsesEscapeSequences( true );
	if ( m_pKVRawDefinition->LoadFromBuffer( NULL, buffer ) )
	{
		return BInitSchema( m_pKVRawDefinition, pVecErrors );
	}
	if ( pVecErrors )
	{
		pVecErrors->AddToTail( "Error parsing keyvalues" );
	}
	return false;
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Set up the buffer to use to reinitialize our schema next time we can do so safely.
//-----------------------------------------------------------------------------
void CEconItemSchema::MaybeInitFromBuffer( IDelayedSchemaData *pDelayedSchemaData )
{
	// Use whatever our most current data block is.
	if ( m_pDelayedSchemaData )
	{
		delete m_pDelayedSchemaData;
	}

	m_pDelayedSchemaData = pDelayedSchemaData;

#ifdef CLIENT_DLL
	// If we aren't in a game we can parse immediately now.
	if ( !engine->IsInGame() )
	{
		BInitFromDelayedBuffer();
	}
#endif // CLIENT_DLL
}

//-----------------------------------------------------------------------------
// We're in a safe place to change the contents of the schema, so do so and clean
// up whatever memory we were using.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitFromDelayedBuffer()
{
	if ( !m_pDelayedSchemaData )
		return true;

	bool bSuccess = m_pDelayedSchemaData->InitializeSchema( this );
	delete m_pDelayedSchemaData;
	m_pDelayedSchemaData = NULL;

	return bSuccess;
}
#endif // !GC_DLL

static void CalculateKeyValuesCRCRecursive( KeyValues *pKV, CRC32_t *crc, bool bIgnoreName = false )
{
	// Hash in the key name in LOWERCASE.  Keyvalues files are not deterministic due
	// to the case insensitivity of the keys and the dependence on the existing
	// state of the name table upon entry.
	if ( !bIgnoreName )
	{
		const char *s = pKV->GetName();  
		for (;;)
		{
			unsigned char x = tolower(*s);
			CRC32_ProcessBuffer( crc, &x, 1 ); // !SPEED! This is slow, but it works.
			if (*s == '\0') break;
			++s;
		}
	}

	// Now hash in value, depending on type
	// !FIXME! This is not byte-order independent!
	switch ( pKV->GetDataType() )
	{
	case KeyValues::TYPE_NONE:
		{
			FOR_EACH_SUBKEY( pKV, pChild )
			{
				CalculateKeyValuesCRCRecursive( pChild, crc );
			}
			break;
		}
	case KeyValues::TYPE_STRING:
		{
			const char *val = pKV->GetString();
			CRC32_ProcessBuffer( crc, val, V_strlen(val)+1 );
			break;
		}

	case KeyValues::TYPE_INT:
		{
			int val = pKV->GetInt();
			CRC32_ProcessBuffer( crc, &val, sizeof(val) );
			break;
		}

	case KeyValues::TYPE_UINT64:
		{
			uint64 val = pKV->GetUint64();
			CRC32_ProcessBuffer( crc, &val, sizeof(val) );
			break;
		}

	case KeyValues::TYPE_FLOAT:
		{
			float val = pKV->GetFloat();
			CRC32_ProcessBuffer( crc, &val, sizeof(val) );
			break;
		}
	case KeyValues::TYPE_COLOR:
		{
			int val = pKV->GetColor().GetRawColor();
			CRC32_ProcessBuffer( crc, &val, sizeof(val) );
			break;
		}

	default:
	case KeyValues::TYPE_PTR:
	case KeyValues::TYPE_WSTRING:
		{
			Assert( !"Unsupport data type!" );
			break;
		}
	}
}

uint32 CEconItemSchema::CalculateKeyValuesVersion( KeyValues *pKV )
{
	CRC32_t crc;
	CRC32_Init( &crc );

	// Calc CRC recursively.  Ignore the very top-most
	// key name, which isn't set consistently
	CalculateKeyValuesCRCRecursive( pKV, &crc, true );
	CRC32_Final( &crc );
	return crc;
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the schema
// Input:	pKVRawDefinition - The raw KeyValues representation of the schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitSchema( KeyValues *pKVRawDefinition, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
#if !defined( GC_DLL )
	m_unVersion = CalculateKeyValuesVersion( pKVRawDefinition );
#endif

	m_unMinLevel = pKVRawDefinition->GetInt( "item_level_min", 0 );
	m_unMaxLevel = pKVRawDefinition->GetInt( "item_level_max", 0 );

	// Pre-parsed arrays in order of appearance for schema initialization
	CUtlVector< KeyValues * > arrRootPrefabs;
	CUtlVector< KeyValues * > arrRootClientLootLists;
	CUtlVector< KeyValues * > arrRootRevolvingLootlists;
	CUtlVector< KeyValues * > arrRootLootlists;
	CUtlVector< KeyValues * > arrRootMiscLootlists;
	CUtlVector< KeyValues * > arrRootStickerKits;
	CUtlVector< KeyValues * > arrRootStickerLists;
	CUtlVector< KeyValues * > arrRootPaintKits;
	CUtlVector< KeyValues * > arrRootPaintKitsRarity;
	CUtlVector< KeyValues * > arrRootItems;
	CUtlVector< KeyValues * > arrRootItemSets;
	CUtlVector< KeyValues * > arrRootMusicDefinitions;
	FOR_EACH_TRUE_SUBKEY( pKVRawDefinition, kvRootSubKey )
	{
		if ( !V_stricmp( kvRootSubKey->GetName(), "prefabs" ) )
			arrRootPrefabs.AddToTail( kvRootSubKey );
		else if ( !V_stricmp( kvRootSubKey->GetName(), "loot_lists" ) )
			arrRootLootlists.AddToTail( kvRootSubKey );
		else if ( !V_stricmp( kvRootSubKey->GetName(), "client_loot_lists" ) )
			arrRootClientLootLists.AddToTail( kvRootSubKey );
		else if ( !V_stricmp( kvRootSubKey->GetName(), "revolving_loot_lists" ) )
			arrRootRevolvingLootlists.AddToTail( kvRootSubKey );
		else if ( !V_stricmp( kvRootSubKey->GetName(), "loot_lists_misc" ) )
			arrRootMiscLootlists.AddToTail( kvRootSubKey );
		else if ( !V_stricmp( kvRootSubKey->GetName(), "sticker_kits" ) )
			arrRootStickerKits.AddToTail( kvRootSubKey );
		else if ( !V_stricmp( kvRootSubKey->GetName(), "sticker_lists" ) )
			arrRootStickerLists.AddToTail( kvRootSubKey );
		else if ( !V_stricmp( kvRootSubKey->GetName(), "paint_kits" ) )
			arrRootPaintKits.AddToTail( kvRootSubKey );
		else if ( !V_stricmp( kvRootSubKey->GetName(), "paint_kits_rarity" ) )
			arrRootPaintKitsRarity.AddToTail( kvRootSubKey );
		else if ( !V_stricmp( kvRootSubKey->GetName(), "items" ) )
			arrRootItems.AddToTail( kvRootSubKey );
		else if ( !V_stricmp( kvRootSubKey->GetName(), "item_sets" ) )
			arrRootItemSets.AddToTail( kvRootSubKey );
		else if ( !V_stricmp( kvRootSubKey->GetName(), "music_definitions" ) )
			arrRootMusicDefinitions.AddToTail( kvRootSubKey );
	}

	// Parse the prefabs block first so the prefabs will be populated in case anything else wants
	// to use them later.
	FOR_EACH_VEC( arrRootPrefabs, i )
	{
		SCHEMA_INIT_SUBSTEP( BInitDefinitionPrefabs( arrRootPrefabs[i], pVecErrors ) );
	}

	// Initialize the game info block
	KeyValues *pKVGameInfo = pKVRawDefinition->FindKey( "game_info" );
	SCHEMA_INIT_CHECK( NULL != pKVGameInfo, CFmtStr( "Required key \"game_info\" missing.\n" ) );

	if ( NULL != pKVGameInfo )
	{
		SCHEMA_INIT_SUBSTEP( BInitGameInfo( pKVGameInfo, pVecErrors ) );
	}

	// Initialize our attribute types. We don't actually pull this data from the schema right now but it
	// still makes sense to initialize it at this point.
	SCHEMA_INIT_SUBSTEP( BInitAttributeTypes( pVecErrors ) );

	// Initialize the rarity block
	KeyValues *pKVRarities = pKVRawDefinition->FindKey( "rarities" );
	SCHEMA_INIT_CHECK( NULL != pKVRarities, CFmtStr( "Required key \"rarities\" missing.\n" ) );
	if ( NULL  != pKVRarities )
	{
		SCHEMA_INIT_SUBSTEP( BInitRarities( pKVRarities, pVecErrors ) );
	}

	// Initialize the qualities block
	KeyValues *pKVQualities = pKVRawDefinition->FindKey( "qualities" );
	SCHEMA_INIT_CHECK( NULL != pKVQualities, CFmtStr( "Required key \"qualities\" missing.\n" ) );
	if ( NULL != pKVQualities )
	{
		SCHEMA_INIT_SUBSTEP( BInitQualities( pKVQualities, pVecErrors ) );
	}

	// Initialize the colors block
	KeyValues *pKVColors = pKVRawDefinition->FindKey( "colors" );
	SCHEMA_INIT_CHECK( NULL != pKVColors, CFmtStr( "Required key \"colors\" missing.\n" ) );

	if ( NULL != pKVColors )
	{
		SCHEMA_INIT_SUBSTEP( BInitColors( pKVColors, pVecErrors ) );
	}

	SCHEMA_INIT_SUBSTEP( BInitGraffitiTints( pKVRawDefinition->FindKey( "graffiti_tints" ), pVecErrors ) );

	// Initialize the music block
	FOR_EACH_VEC( arrRootMusicDefinitions, i )
	{
		SCHEMA_INIT_SUBSTEP( BInitMusicDefs( arrRootMusicDefinitions[i], pVecErrors ) );
	}


	// Initialize the attributes block
	KeyValues *pKVAttributes = pKVRawDefinition->FindKey( "attributes" );
	SCHEMA_INIT_CHECK( NULL != pKVAttributes, CFmtStr( "Required key \"attributes\" missing.\n" ) );
	if ( NULL != pKVAttributes )
	{
		SCHEMA_INIT_SUBSTEP( BInitAttributes( pKVAttributes, pVecErrors ) );
	}

	// Initialize the sound materials block
	KeyValues *pKVSoundMaterials = pKVRawDefinition->FindKey( "sound_materials" );
	if ( NULL  != pKVSoundMaterials )
	{
		SCHEMA_INIT_SUBSTEP( BInitSoundMaterials( pKVSoundMaterials, pVecErrors ) );
	}

	// Initialize the "equip_regions_list" block -- this is an optional block
	KeyValues *pKVEquipRegions = pKVRawDefinition->FindKey( "equip_regions_list" );
	if ( NULL != pKVEquipRegions )
	{
		SCHEMA_INIT_SUBSTEP( BInitEquipRegions( pKVEquipRegions, pVecErrors ) );
	}

	// Initialize the "equip_conflicts" block -- this is an optional block, though it doesn't
	// make any sense and will probably fail internally if there is no corresponding "equip_regions"
	// block as well
	KeyValues *pKVEquipRegionConflicts = pKVRawDefinition->FindKey( "equip_conflicts" );
	if ( NULL != pKVEquipRegionConflicts )
	{
		SCHEMA_INIT_SUBSTEP( BInitEquipRegionConflicts( pKVEquipRegionConflicts, pVecErrors ) );
	}

	// Do before items, so items can refer into it.
	KeyValues *pKVParticleSystems = pKVRawDefinition->FindKey( "attribute_controlled_attached_particles" );
	SCHEMA_INIT_SUBSTEP( BInitAttributeControlledParticleSystems( pKVParticleSystems, pVecErrors ) );

	FOR_EACH_VEC( arrRootStickerKits, i )
	{
		SCHEMA_INIT_SUBSTEP( BInitStickerKits( arrRootStickerKits[i], pVecErrors ) );
	}

	FOR_EACH_VEC( arrRootPaintKits, i )
	{
		SCHEMA_INIT_SUBSTEP( BInitPaintKits( arrRootPaintKits[i], pVecErrors ) );
	}

	FOR_EACH_VEC( arrRootPaintKitsRarity, i )
	{
		SCHEMA_INIT_SUBSTEP( BInitPaintKitsRarity( arrRootPaintKitsRarity[i], pVecErrors ) );
	}

	// Initialize the items block
	SCHEMA_INIT_CHECK( arrRootItems.Count(), CFmtStr( "Required key(s) \"items\" missing.\n" ) );
	FOR_EACH_VEC( arrRootItems, i )
	{
		m_bSchemaParsingItems = true;
		SCHEMA_INIT_SUBSTEP( BInitItems( arrRootItems[i], pVecErrors ) );
		m_bSchemaParsingItems = false;
	}
	SCHEMA_INIT_SUBSTEP( BInitItemMappings( pVecErrors ) );

	// Setup bundle links
	SCHEMA_INIT_SUBSTEP( BInitBundles( pVecErrors ) );

	// Setup payment rules.
	SCHEMA_INIT_SUBSTEP( BInitPaymentRules( pVecErrors ) );

	// Parse the item_sets block.
	FOR_EACH_VEC( arrRootItemSets, i )
	{
		SCHEMA_INIT_SUBSTEP( BInitItemSets( arrRootItemSets[i], pVecErrors ) );
	}


	// Initialize the quest block
	KeyValues *pKVQuestDefs = pKVRawDefinition->FindKey( "quest_definitions" );
	SCHEMA_INIT_CHECK( NULL != pKVQuestDefs, CFmtStr( "Required key \"quest_definitions\" missing.\n" ) );

	if ( NULL != pKVQuestDefs )
	{
		SCHEMA_INIT_SUBSTEP( BInitQuestDefs( pKVQuestDefs, pVecErrors ) );
	}

	// Initialize the quest event schedule block
	SCHEMA_INIT_SUBSTEP( BInitQuestEvents( pKVRawDefinition->FindKey( "quest_schedule" ), pVecErrors ) );

	// Initialize the campaign block
	KeyValues *pKVCampaignDefs = pKVRawDefinition->FindKey( "campaign_definitions" );
	SCHEMA_INIT_CHECK( NULL != pKVCampaignDefs, CFmtStr( "Required key \"campaign_definitions\" missing.\n" ) );

	if ( NULL != pKVCampaignDefs )
	{
		SCHEMA_INIT_SUBSTEP( BInitCampaignDefs( pKVCampaignDefs, pVecErrors ) );
	}

	// Parse any recipes block
	KeyValues *pKVRecipes = pKVRawDefinition->FindKey( "recipes" );
	if ( NULL != pKVRecipes )
	{
		SCHEMA_INIT_SUBSTEP( BInitRecipes( pKVRecipes, pVecErrors ) );
	}

	// Parse alternate icons block.
	KeyValues * pKVAlternateIcons = pKVRawDefinition->FindKey( "alternate_icons2" );
	if ( NULL != pKVAlternateIcons )
	{
		SCHEMA_INIT_SUBSTEP( BInitAlternateIcons( pKVAlternateIcons, pVecErrors ) );
	}

	// Reset our loot lists.
	m_dictLootLists.Purge();

	// Parse the collection loot lists block
	KeyValues *pKVQuestRewardLootLists = pKVRawDefinition->FindKey( "quest_reward_loot_lists" );
	SCHEMA_INIT_SUBSTEP( BInitQuestRewardLootLists( pKVQuestRewardLootLists, pVecErrors ) );

	// Parse the loot lists block (on the GC)
	KeyValues *pKVRandomAttributeTemplates = pKVRawDefinition->FindKey( "random_attribute_templates" );
	
	// Parse the client loot lists block (everywhere)
	FOR_EACH_VEC( arrRootClientLootLists, i )
	{
		SCHEMA_INIT_SUBSTEP( BInitLootLists( arrRootClientLootLists[i], pKVRandomAttributeTemplates, pVecErrors, false ) );
	}

	// Parse the revolving loot lists block
	FOR_EACH_VEC( arrRootRevolvingLootlists, i )
	{
		SCHEMA_INIT_SUBSTEP( BInitRevolvingLootLists( arrRootRevolvingLootlists[i], pVecErrors ) );
	}

#if defined( CLIENT_DLL ) || defined( GAME_DLL )
	KeyValues *pKVArmoryData = pKVRawDefinition->FindKey( "armory_data" );
	if ( NULL != pKVArmoryData )
	{
		SCHEMA_INIT_SUBSTEP( BInitArmoryData( pKVArmoryData, pVecErrors ) );
	}
#endif

	// Parse any achievement rewards
	KeyValues *pKVAchievementRewards = pKVRawDefinition->FindKey( "achievement_rewards" );
	if ( NULL != pKVAchievementRewards )
	{
		SCHEMA_INIT_SUBSTEP( BInitAchievementRewards( pKVAchievementRewards, pVecErrors ) );
	}

#ifdef TF_CLIENT_DLL
	// Compute the number of concrete items, for each item, and cache for quick access
	SCHEMA_INIT_SUBSTEP( BInitConcreteItemCounts( pVecErrors ) );
#endif // TF_CLIENT_DLL

	// Parse the item levels block
	KeyValues *pKVItemLevels = pKVRawDefinition->FindKey( "item_levels" );
	SCHEMA_INIT_SUBSTEP( BInitItemLevels( pKVItemLevels, pVecErrors ) );

	// Parse the kill eater score types
	KeyValues *pKVKillEaterScoreTypes = pKVRawDefinition->FindKey( "kill_eater_score_types" );
	SCHEMA_INIT_SUBSTEP( BInitKillEaterScoreTypes( pKVKillEaterScoreTypes, pVecErrors ) );

#ifdef CLIENT_DLL
	// Load web resource references.
	KeyValues* pKVWebResources = pKVRawDefinition->FindKey( "web_resources" );
	SCHEMA_INIT_SUBSTEP( BInitWebResources( pKVWebResources, pVecErrors ) );
#endif

	// Load web resource references.
	KeyValues* pProPlayers = pKVRawDefinition->FindKey( "pro_players" );
	SCHEMA_INIT_SUBSTEP( BInitProPlayers( pProPlayers, pVecErrors ) );

	SCHEMA_INIT_SUBSTEP( BPostSchemaInitStartupChecks( pVecErrors ) );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the "game_info" section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitGameInfo( KeyValues *pKVGameInfo, CUtlVector<CUtlString> *pVecErrors )
{
	m_unFirstValidClass = pKVGameInfo->GetInt( "first_valid_class", 0 );
	m_unLastValidClass = pKVGameInfo->GetInt( "last_valid_class", 0 );
	SCHEMA_INIT_CHECK( 0 <= m_unFirstValidClass, CFmtStr( "First valid class must be greater or equal to 0." ) );
	SCHEMA_INIT_CHECK( m_unFirstValidClass <= m_unLastValidClass, CFmtStr( "First valid class must be less than or equal to last valid class." ) );

	m_unFirstValidItemSlot = pKVGameInfo->GetInt( "first_valid_item_slot", INVALID_EQUIPPED_SLOT );
	m_unLastValidItemSlot = pKVGameInfo->GetInt( "last_valid_item_slot", INVALID_EQUIPPED_SLOT );
	SCHEMA_INIT_CHECK( INVALID_EQUIPPED_SLOT != m_unFirstValidItemSlot, CFmtStr( "first_valid_item_slot not set!" ) );
	SCHEMA_INIT_CHECK( INVALID_EQUIPPED_SLOT != m_unLastValidItemSlot, CFmtStr( "last_valid_item_slot not set!" ) );
	SCHEMA_INIT_CHECK( m_unFirstValidItemSlot <= m_unLastValidItemSlot, CFmtStr( "First valid item slot must be less than or equal to last valid item slot." ) );

	m_unNumItemPresets = pKVGameInfo->GetInt( "num_item_presets", -1 );
	SCHEMA_INIT_CHECK( (uint32)-1 != m_unNumItemPresets, CFmtStr( "num_item_presets not set!" ) );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitAttributeTypes( CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_VEC( m_vecAttributeTypes, i )
	{
		delete m_vecAttributeTypes[i].m_pAttrType;
	}
	m_vecAttributeTypes.Purge();

	m_vecAttributeTypes.AddToTail( attr_type_t( NULL,			new CSchemaAttributeType_Default ) );
	m_vecAttributeTypes.AddToTail( attr_type_t( "uint32",		new CSchemaAttributeType_Uint32 ) );
	m_vecAttributeTypes.AddToTail( attr_type_t( "float",		new CSchemaAttributeType_Float ) );
	m_vecAttributeTypes.AddToTail( attr_type_t( "string",		new CSchemaAttributeType_String ) );
	m_vecAttributeTypes.AddToTail( attr_type_t( "vector",		new CSchemaAttributeType_Vector ) );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the "prefabs" section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitDefinitionPrefabs( KeyValues *pKVPrefabs, CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_TRUE_SUBKEY( pKVPrefabs, pKVPrefab )
	{
		const char *pszPrefabName = pKVPrefab->GetName();

		int nMapIndex = m_mapDefinitionPrefabs.Find( pszPrefabName );

		// Make sure the item index is correct because we use this index as a reference
		SCHEMA_INIT_CHECK( 
			!m_mapDefinitionPrefabs.IsValidIndex( nMapIndex ),
			CFmtStr( "Duplicate prefab name (%s)", pszPrefabName ) );

		m_mapDefinitionPrefabs.Insert( pszPrefabName, pKVPrefab->MakeCopy() );
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the rarity section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitRarities( KeyValues *pKVRarities, CUtlVector<CUtlString> *pVecErrors )
{
	// initialize the item definitions
	if ( NULL != pKVRarities )
	{
		FOR_EACH_TRUE_SUBKEY( pKVRarities, pKVRarity )
		{
			int nRarityIndex = pKVRarity->GetInt( "value" );
			int nMapIndex = m_mapRarities.Find( nRarityIndex );

			// Make sure the item index is correct because we use this index as a reference
			SCHEMA_INIT_CHECK( 
				!m_mapRarities.IsValidIndex( nMapIndex ),
				CFmtStr( "Duplicate rarity value (%d)", nRarityIndex ) );

			nMapIndex = m_mapRarities.Insert( nRarityIndex );
			SCHEMA_INIT_SUBSTEP( m_mapRarities[nMapIndex].BInitFromKV( pKVRarity, *this, pVecErrors ) );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the qualities section of the schema
// Input:	pKVQualities - The qualities section of the KeyValues 
//				representation of the schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitQualities( KeyValues *pKVQualities, CUtlVector<CUtlString> *pVecErrors )
{
	// initialize the item definitions
	if ( NULL != pKVQualities )
	{
		FOR_EACH_TRUE_SUBKEY( pKVQualities, pKVQuality )
		{
			int nQualityIndex = pKVQuality->GetInt( "value" );
			int nMapIndex = m_mapQualities.Find( nQualityIndex );

			// Make sure the item index is correct because we use this index as a reference
			SCHEMA_INIT_CHECK( 
				!m_mapQualities.IsValidIndex( nMapIndex ),
				CFmtStr( "Duplicate quality value (%d)", nQualityIndex ) );

			nMapIndex = m_mapQualities.Insert( nQualityIndex );
			SCHEMA_INIT_SUBSTEP( m_mapQualities[nMapIndex].BInitFromKV( pKVQuality, *this, pVecErrors ) );
		}
	}

	// Check the integrity of the quality definitions

	// Check for duplicate quality names
	CUtlRBTree<const char *> rbQualityNames( CaselessStringLessThan );
	rbQualityNames.EnsureCapacity( m_mapQualities.Count() );
	FOR_EACH_MAP_FAST( m_mapQualities, i )
	{
		int iIndex = rbQualityNames.Find( m_mapQualities[i].GetName() );
		SCHEMA_INIT_CHECK( 
			!rbQualityNames.IsValidIndex( iIndex ),
			CFmtStr( "Quality definition %d: Duplicate quality name %s", m_mapQualities[i].GetDBValue(), m_mapQualities[i].GetName() ) );

		if( !rbQualityNames.IsValidIndex( iIndex ) )
			rbQualityNames.Insert( m_mapQualities[i].GetName() );
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitColors( KeyValues *pKVColors, CUtlVector<CUtlString> *pVecErrors )
{
	// initialize the color definitions
	if ( NULL != pKVColors )
	{
		FOR_EACH_TRUE_SUBKEY( pKVColors, pKVColor )
		{
			CEconColorDefinition *pNewColorDef = new CEconColorDefinition;

			SCHEMA_INIT_SUBSTEP( pNewColorDef->BInitFromKV( pKVColor, *this, pVecErrors ) );
			m_vecColorDefs.AddToTail( pNewColorDef );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

bool CEconItemSchema::BInitGraffitiTints( KeyValues *pKVColors, CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_TRUE_SUBKEY( pKVColors, pKVColor )
	{
		CEconGraffitiTintDefinition *pNewDef = new CEconGraffitiTintDefinition;

		bool bResult = pNewDef->BInitFromKV( pKVColor, *this, pVecErrors );
		const int nMaxIDallowed = 64;
		if ( bResult )
		{
			SCHEMA_INIT_CHECK( ( pNewDef->GetID() > 0 ) && ( pNewDef->GetID() < nMaxIDallowed ),
				CFmtStr( "Graffiti tint definition id out of range [1..%d]: #%d %s\n", nMaxIDallowed, pNewDef->GetID(), pNewDef->GetColorName() ) );
			SCHEMA_INIT_CHECK( !m_vecGraffitiTintDefs.IsValidIndex( pNewDef->GetID() ) || !m_vecGraffitiTintDefs[pNewDef->GetID()],
				CFmtStr( "Graffiti tint definition duplicate: #%d %s\n", pNewDef->GetID(), pNewDef->GetColorName() ) );
			SCHEMA_INIT_CHECK( pNewDef->GetColorName() && *pNewDef->GetColorName() && !m_mapGraffitiTintByName.Defined( pNewDef->GetColorName() ),
				CFmtStr( "Graffiti tint name duplicate: #%d %s\n", pNewDef->GetID(), pNewDef->GetColorName() ) );
			bResult &= ( pNewDef->GetID() > 0 ) && ( pNewDef->GetID() < nMaxIDallowed ) &&
				( !m_vecGraffitiTintDefs.IsValidIndex( pNewDef->GetID() ) || !m_vecGraffitiTintDefs[pNewDef->GetID()] ) &&
				pNewDef->GetColorName() && *pNewDef->GetColorName() && !m_mapGraffitiTintByName.Defined( pNewDef->GetColorName() );
		}
		if ( !bResult )
		{
			SCHEMA_INIT_SUBSTEP( bResult );
			delete pNewDef;
		}
		else
		{
			if ( !m_vecGraffitiTintDefs.Count() )
			{
				for ( int j = 0; j < nMaxIDallowed; ++ j )
					m_vecGraffitiTintDefs.AddToTail( NULL );
			}
			m_vecGraffitiTintDefs[pNewDef->GetID()] = pNewDef;
			m_mapGraffitiTintByName[pNewDef->GetColorName()] = pNewDef;
			m_nMaxValidGraffitiTintDefID = MAX( m_nMaxValidGraffitiTintDefID, pNewDef->GetID() );
		}
	}

	SCHEMA_INIT_CHECK( m_vecGraffitiTintDefs.Count() > 0, CFmtStr( "Graffiti tint definitions failed to parse\n" ) );

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
AlternateIconData_t* CEconItemSchema::GetAlternateIcon( uint64 ullAlternateIcon )
{
	int iIdx = m_mapAlternateIcons.Find( ullAlternateIcon );
	if ( !m_mapAlternateIcons.IsValidIndex( iIdx ) )
		return NULL;
	else
		return &(m_mapAlternateIcons[iIdx]);
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the rarity section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitSoundMaterials( KeyValues *pKVSoundMaterials, CUtlVector<CUtlString> *pVecErrors )
{
	// initialize the item definitions
	if ( NULL != pKVSoundMaterials )
	{
		FOR_EACH_TRUE_SUBKEY( pKVSoundMaterials, pKVSoundMaterial )
		{
			int nSoundMaterialIndex = pKVSoundMaterial->GetInt( "value" );
			int nMapIndex = m_mapSoundMaterials.Find( nSoundMaterialIndex );

			// Make sure the item index is correct because we use this index as a reference
			SCHEMA_INIT_CHECK( 
				!m_mapSoundMaterials.IsValidIndex( nMapIndex ),
				CFmtStr( "Duplicate sound material value (%d)", nSoundMaterialIndex ) );

			nMapIndex = m_mapSoundMaterials.Insert( nSoundMaterialIndex );
			SCHEMA_INIT_SUBSTEP( m_mapSoundMaterials[nMapIndex].BInitFromKV( pKVSoundMaterial, *this, pVecErrors ) );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CEconItemSchema::GetEquipRegionIndexByName( const char *pRegionName ) const
{
	FOR_EACH_VEC( m_vecEquipRegionsList, i )
	{
		const char *szEntryRegionName = m_vecEquipRegionsList[i].m_sName;
		if ( !V_stricmp( szEntryRegionName, pRegionName ) )
			return i;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
equip_region_mask_t CEconItemSchema::GetEquipRegionBitMaskByName( const char *pRegionName ) const
{
	int iRegionIndex = GetEquipRegionIndexByName( pRegionName );
	if ( !m_vecEquipRegionsList.IsValidIndex( iRegionIndex ) )
		return 0;

	equip_region_mask_t unRegionMask = 1 << m_vecEquipRegionsList[iRegionIndex].m_unBitIndex;
	Assert( unRegionMask > 0 );

	return unRegionMask;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CEconItemSchema::SetEquipRegionConflict( int iRegion, unsigned int unBit )
{
	Assert( m_vecEquipRegionsList.IsValidIndex( iRegion ) );

	equip_region_mask_t unRegionMask = 1 << unBit;
	Assert( unRegionMask > 0 );

	m_vecEquipRegionsList[iRegion].m_unMask |= unRegionMask;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
equip_region_mask_t CEconItemSchema::GetEquipRegionMaskByName( const char *pRegionName ) const
{
	int iRegionIdx = GetEquipRegionIndexByName( pRegionName );
	if ( iRegionIdx < 0 )
		return 0;

	return m_vecEquipRegionsList[iRegionIdx].m_unMask;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
void CEconItemSchema::AssignDefaultBodygroupState( const char *pszBodygroupName, int iValue )
{
	// Flip the value passed in -- if we specify in the schema that a region should be off, we assume that it's
	// on by default.
	int iDefaultValue = iValue == 0 ? 1 : 0;

	// Make sure that we're constantly reinitializing our default value to the same default value. This is sort
	// of dumb but it works for everything we've got now. In the event that conflicts start cropping up it would
	// be easy enough to make a new schema section.
	int iIndex = m_mapDefaultBodygroupState.Find( pszBodygroupName );
	if ( (m_mapDefaultBodygroupState.IsValidIndex( iIndex ) && m_mapDefaultBodygroupState[iIndex] != iDefaultValue) ||
		 (iValue < 0 || iValue > 1) )
	{
		EmitWarning( SPEW_GC, 4, "Unable to get accurate read on whether bodygroup '%s' is enabled or disabled by default. (The schema is fine, but the code is confused and could stand to be made smarter.)\n", pszBodygroupName );
	}

	if ( !m_mapDefaultBodygroupState.IsValidIndex( iIndex ) )
	{
		m_mapDefaultBodygroupState.Insert( pszBodygroupName, iDefaultValue );
	}
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitEquipRegions( KeyValues *pKVEquipRegions, CUtlVector<CUtlString> *pVecErrors )
{
	CUtlVector<const char *> vecNames;

	FOR_EACH_SUBKEY( pKVEquipRegions, pKVRegion )
	{
		const char *pRegionKeyName = pKVRegion->GetName();

		vecNames.Purge();

		// The "shared" name is special for equip regions -- it means that all of the sub-regions specified
		// will use the same bit to store equipped-or-not data, but that one bit can be accessed by a whole
		// bunch of different names. This is useful in TF where different classes have different regions, but
		// those regions cannot possibly conflict with each other. For example, "scout_backpack" cannot possibly
		// overlap with "pyro_shoulder" because they can't even be equipped on the same character.
		if ( pRegionKeyName && !Q_stricmp( pRegionKeyName, "shared" ) )
		{
			FOR_EACH_SUBKEY( pKVRegion, pKVSharedRegionName )
			{
				vecNames.AddToTail( pKVSharedRegionName->GetName() );
			}
		}
		// We have a standard name -- this one entry is its own equip region.
		else
		{
			vecNames.AddToTail( pRegionKeyName );
		}

		// What bit will this equip region use to mask against conflicts? If we don't have any equip regions
		// at all, we'll use the base bit, otherwise we just grab one higher than whatever we used last.
		unsigned int unNewBitIndex = m_vecEquipRegionsList.Count() <= 0 ? 0 : m_vecEquipRegionsList.Tail().m_unBitIndex + 1;
		
		FOR_EACH_VEC( vecNames, i )
		{
			const char *pRegionName = vecNames[i];

			// Make sure this name is unique.
			if ( GetEquipRegionIndexByName( pRegionName ) >= 0 )
			{
				pVecErrors->AddToTail( CFmtStr( "Duplicate equip region named \"%s\".", pRegionName ).Access() );
				continue;
			}

			// Make a new region.
			EquipRegion newEquipRegion;
			newEquipRegion.m_sName		= pRegionName;
			newEquipRegion.m_unMask		= 0;				// we'll update this mask later
			newEquipRegion.m_unBitIndex	= unNewBitIndex;

			int iIdx = m_vecEquipRegionsList.AddToTail( newEquipRegion );

			// Tag this region to conflict with itself so that if nothing else two items in the same
			// region can't equip over each other.
			SetEquipRegionConflict( iIdx, unNewBitIndex );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitEquipRegionConflicts( KeyValues *pKVEquipRegionConflicts, CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_TRUE_SUBKEY( pKVEquipRegionConflicts, pKVConflict )
	{
		// What region is the base of this conflict?
		const char *pRegionName = pKVConflict->GetName();
		int iRegionIdx = GetEquipRegionIndexByName( pRegionName );
		if ( iRegionIdx < 0 )
		{
			pVecErrors->AddToTail( CFmtStr( "Unable to find base equip region named \"%s\" for conflicts.", pRegionName ).Access() );
			continue;
		}

		FOR_EACH_SUBKEY( pKVConflict, pKVConflictOther )
		{
			const char *pOtherRegionName = pKVConflictOther->GetName();
			int iOtherRegionIdx = GetEquipRegionIndexByName( pOtherRegionName );
			if ( iOtherRegionIdx < 0 )
			{
				pVecErrors->AddToTail( CFmtStr( "Unable to find other equip region named \"%s\" for conflicts.", pOtherRegionName ).Access() );
				continue;
			}

			SetEquipRegionConflict( iRegionIdx,		 m_vecEquipRegionsList[iOtherRegionIdx].m_unBitIndex );
			SetEquipRegionConflict( iOtherRegionIdx, m_vecEquipRegionsList[iRegionIdx].m_unBitIndex );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the attributes section of the schema
// Input:	pKVAttributes - The attributes section of the KeyValues 
//				representation of the schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitAttributes( KeyValues *pKVAttributes, CUtlVector<CUtlString> *pVecErrors )
{
	// Initialize the attribute definitions
	FOR_EACH_TRUE_SUBKEY( pKVAttributes, pKVAttribute )
	{
		int nAttrIndex = Q_atoi( pKVAttribute->GetName() );
		
		// Make sure the index is positive
		SCHEMA_INIT_CHECK(
			( nAttrIndex >= 0 ) && ( nAttrIndex < 30*1000 ),
			CFmtStr( "Attribute definition index %d must be greater than or equal to zero", nAttrIndex ) );

		if ( !( ( nAttrIndex >= 0 ) && ( nAttrIndex < 30*1000 ) ) )
			continue;

		// Build the direct mapping container
		while ( nAttrIndex >= m_mapAttributesContainer.Count() )
			m_mapAttributesContainer.AddToTail( NULL );
		Assert( nAttrIndex < m_mapAttributesContainer.Count() );

		// Make sure the attribute index is not repeated
		SCHEMA_INIT_CHECK( 
			!m_mapAttributesContainer[nAttrIndex],
			CFmtStr( "Duplicate attribute definition index (%d)", nAttrIndex ) );
		if ( m_mapAttributesContainer[nAttrIndex] )
			continue;

		m_mapAttributesContainer[nAttrIndex] = new CEconItemAttributeDefinition;
		SCHEMA_INIT_SUBSTEP( m_mapAttributesContainer[nAttrIndex]->BInitFromKV( pKVAttribute, *this, pVecErrors ) );
	}

	// Check the integrity of the attribute definitions

	// Check for duplicate attribute definition names
	CUtlRBTree<const char *> rbAttributeNames( CaselessStringLessThan );
	rbAttributeNames.EnsureCapacity( m_mapAttributesContainer.Count() );
	FOR_EACH_VEC( m_mapAttributesContainer, i )
	{
		if ( !m_mapAttributesContainer[i] )
			continue;

		int iIndex = rbAttributeNames.Find( m_mapAttributesContainer[i]->GetDefinitionName() );
		SCHEMA_INIT_CHECK( 
			!rbAttributeNames.IsValidIndex( iIndex ),
			CFmtStr( "Attribute definition %d: Duplicate name \"%s\"", i, m_mapAttributesContainer[i]->GetDefinitionName() ) );
		if( !rbAttributeNames.IsValidIndex( iIndex ) )
			rbAttributeNames.Insert( m_mapAttributesContainer[i]->GetDefinitionName() );
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	After bundles are set up, we can do payment rules.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitPaymentRules( CUtlVector<CUtlString> *pVecErrors )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose:	After items are initialized, this method links bundles to their contents.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitBundles( CUtlVector<CUtlString> *pVecErrors )
{
	// Link each bundle with its contents.
	FOR_EACH_MAP_FAST( m_mapItems, i )
	{
		CEconItemDefinition *pItemDef = m_mapItems[ i ];
		if ( !pItemDef )
			continue;

		KeyValues* pItemKV = pItemDef->GetRawDefinition();
		if ( !pItemKV )
			continue;

		KeyValues *pBundleDataKV = pItemKV->FindKey( "bundle" );
		if ( pBundleDataKV )
		{
			pItemDef->m_BundleInfo = new bundleinfo_t();
			FOR_EACH_SUBKEY( pBundleDataKV, pKVCurItem )
			{
				item_list_entry_t entry;
				bool bEntrySuccess = entry.InitFromName( pKVCurItem->GetName() );

				SCHEMA_INIT_CHECK( 
					bEntrySuccess,
					CFmtStr( "Bundle %s: Item definition \"%s\" failed to initialize\n", pItemDef->m_pszDefinitionName, pKVCurItem->GetName() ) );

				const CEconItemDefinition *pBundleItemDef = GetItemDefinition( entry.m_nItemDef );
				SCHEMA_INIT_CHECK( pBundleItemDef, CFmtStr( "Unable to find item definition '%s' for bundle '%s'.", pKVCurItem->GetName(), pItemDef->m_pszDefinitionName ) );

				pItemDef->m_BundleInfo->vecItemEntries.AddToTail( entry );
			}

			// Only check for pack bundle if the item is actually a bundle - note that we could do this programatically by checking that all items in the bundle are flagged as a "pack item" - but for now the bundle needs to be explicitly flagged as a pack bundle.
			pItemDef->m_bIsPackBundle = pItemKV->GetInt( "is_pack_bundle", 0 ) != 0;
		}

		// Make a list of all of our bundles.
		if ( pItemDef->IsBundle() )
		{
			// Cache off the item def for the bundle, since we'll need both the bundle info and the item def index later.
			m_vecBundles.AddToTail( pItemDef );

			// If the bundle is a pack bundle, mark all the contained items as pack items / link to the owning pack bundle
			if ( pItemDef->IsPackBundle() )
			{
				const bundleinfo_t *pBundleInfo = pItemDef->GetBundleInfo();
				FOR_EACH_VEC( pBundleInfo->vecItemEntries, iCurItem )
				{
					CEconItemDefinition *pCurItemDef = GetItemDefinitionMutable( pBundleInfo->vecItemEntries[ iCurItem ].m_nItemDef );
					SCHEMA_INIT_CHECK( NULL == pCurItemDef->m_pOwningPackBundle, CFmtStr( "Pack item \"%s\" included in more than one pack bundle - not allowed!", pCurItemDef->GetDefinitionName() ) );
					pCurItemDef->m_pOwningPackBundle = pItemDef;
				}
			}
		}
	}

	// Go through all regular (ie non-pack) bundles and ensure that if any pack items are included, *all* pack items in the owning pack bundle are included.
	FOR_EACH_VEC( m_vecBundles, iBundle )
	{
		const CEconItemDefinition *pBundleItemDef = m_vecBundles[ iBundle ];
		if ( pBundleItemDef->IsPackBundle() )
			continue;

		// Go through all items in the bundle and look for pack items
		const bundleinfo_t *pBundle = pBundleItemDef->GetBundleInfo();
		if ( pBundle )
		{
			FOR_EACH_VEC( pBundle->vecItemEntries, iContainedBundleItem )
			{
				// Get the associated pack bundle
				const CEconItemDefinition *pContainedBundleItemDef = GetItemDefinition( pBundle->vecItemEntries[ iContainedBundleItem ].m_nItemDef );

				// Ignore non-pack items
				if ( !pContainedBundleItemDef || !pContainedBundleItemDef->IsPackItem() )
					continue;

				// Get the pack bundle that contains this particular pack item
				const CEconItemDefinition *pOwningPackBundleItemDef = pContainedBundleItemDef->GetOwningPackBundle();

				// Make sure all items in the owning pack bundle are in pBundleItemDef's bundle info (pBundle)
				const bundleinfo_t *pOwningPackBundle = pOwningPackBundleItemDef->GetBundleInfo();
				FOR_EACH_VEC( pOwningPackBundle->vecItemEntries, iCurPackBundleItem )
				{
					bool bFound = false;
					FOR_EACH_VEC( pBundle->vecItemEntries, i )
					{
						if ( pOwningPackBundle->vecItemEntries[ iCurPackBundleItem ].m_nItemDef == pBundle->vecItemEntries[ i ].m_nItemDef && 
							 pOwningPackBundle->vecItemEntries[ iCurPackBundleItem ].m_nPaintKit == pBundle->vecItemEntries[ i ].m_nPaintKit )
						{
							bFound = true;
							break;
						}
					}


					if ( !bFound )
					{
						SCHEMA_INIT_CHECK(
							false,
							CFmtStr( "The bundle \"%s\" contains some, but not all pack items required specified by pack bundle \"%s.\"",
							pBundleItemDef->GetDefinitionName(),
							pOwningPackBundleItemDef->GetDefinitionName()
							)
							);
					}
				}
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the items section of the schema
// Input:	pKVItems - The items section of the KeyValues 
//				representation of the schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitItems( KeyValues *pKVItems, CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_TRUE_SUBKEY( pKVItems, pKVItem )
	{
		if ( Q_stricmp( pKVItem->GetName(), "default" ) == 0 )
		{
#if defined(CLIENT_DLL) || defined(GAME_DLL)
			SCHEMA_INIT_CHECK(
				m_pDefaultItemDefinition == NULL,
				CFmtStr( "Duplicate 'default' item definition." ) );

			if ( m_pDefaultItemDefinition == NULL )
			{
				m_pDefaultItemDefinition = CreateEconItemDefinition();
			}
			SCHEMA_INIT_SUBSTEP( m_pDefaultItemDefinition->BInitFromKV( pKVItem, *this, pVecErrors ) );
#endif
		}
		else
		{
			int nItemIndex = Q_atoi( pKVItem->GetName() );
			int nMapIndex = m_mapItems.Find( nItemIndex );

			// Make sure the item index is correct because we use this index as a reference
			SCHEMA_INIT_CHECK( 
				!m_mapItems.IsValidIndex( nMapIndex ),
				CFmtStr( "Duplicate item definition (%d)", nItemIndex ) );

			// Check to make sure the index is positive
			SCHEMA_INIT_CHECK( 
				nItemIndex >= 0,
				CFmtStr( "Item definition index %d must be greater than or equal to zero", nItemIndex ) );

			CEconItemDefinition *pItemDef = CreateEconItemDefinition();
			nMapIndex = m_mapItems.Insert( nItemIndex, pItemDef );
			m_mapItemsSorted.Insert( nItemIndex, pItemDef );
			SCHEMA_INIT_SUBSTEP( m_mapItems[nMapIndex]->BInitFromKV( pKVItem, *this, pVecErrors ) );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

bool CEconItemSchema::BInitItemMappings( CUtlVector<CUtlString> *pVecErrors )
{
	// Check the integrity of the item definitions
	CUtlRBTree<const char *> rbItemNames( CaselessStringLessThan );
	rbItemNames.EnsureCapacity( m_mapItems.Count() );
	FOR_EACH_MAP_FAST( m_mapItems, i )
	{
		CEconItemDefinition *pItemDef = m_mapItems[ i ];

		// Check for duplicate item definition names
		int iIndex = rbItemNames.Find( pItemDef->GetDefinitionName() );
		SCHEMA_INIT_CHECK( 
			!rbItemNames.IsValidIndex( iIndex ),
			CFmtStr( "Item definition %s: Duplicate name on index %d", pItemDef->GetDefinitionName(), m_mapItems.Key( i ) ) );
		if( !rbItemNames.IsValidIndex( iIndex ) )
			rbItemNames.Insert( m_mapItems[i]->GetDefinitionName() );

		// Link up armory and store mappings for the item
		SCHEMA_INIT_SUBSTEP( pItemDef->BInitItemMappings( *this, pVecErrors ) );
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Delete an item definition. Moderately dangerous as cached references will become bad.
// Intended for use by the item editor.
//-----------------------------------------------------------------------------
bool CEconItemSchema::DeleteItemDefinition( int iDefIndex )
{
	m_mapItemsSorted.Remove( iDefIndex );

	int nMapIndex = m_mapItems.Find( iDefIndex );
	if ( m_mapItems.IsValidIndex( nMapIndex ) )
	{
		CEconItemDefinition* pItemDef = m_mapItems[nMapIndex];
		if ( pItemDef )
		{
			m_mapItems.RemoveAt( nMapIndex );
			delete pItemDef;
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
const CEconItemSetDefinition* CEconItemSchema::GetItemSet( const char* pSetName, int *piIndex ) const
{
	for ( unsigned int i=0; i<m_mapItemSets.Count(); ++i )
	{
		if ( !Q_strcmp( m_mapItemSets.Key(i), pSetName ) )
		{
			if ( piIndex )
			{
				*piIndex = i;
			}
			return (CEconItemSetDefinition*) &(m_mapItemSets[i]);
		}
	}

	return NULL;
}

const IEconItemSetDefinition* CEconItemSchema::GetItemSet( int iIndex ) const
{
	if ( m_mapItemSets.IsValidIndex( iIndex ) )
		return &m_mapItemSets[iIndex];
	else
		return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	Parses the Item Sets section.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitItemSets( KeyValues *pKVItemSets, CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_TRUE_SUBKEY( pKVItemSets, pKVItemSet )
	{
		const char* setName = pKVItemSet->GetName();

		SCHEMA_INIT_CHECK( setName != NULL, CFmtStr( "All itemsets must have names.") );

		if ( m_mapItemSets.Count() > 0 )
		{
			SCHEMA_INIT_CHECK( GetItemSet( setName ) == NULL, CFmtStr( "Duplicate itemset name (%s) found!", setName ) );
		}

		int idx = m_mapItemSets.Insert( setName );
		SCHEMA_INIT_SUBSTEP( m_mapItemSets[idx].BInitFromKV( pKVItemSet, *this, pVecErrors ) );

		FOR_EACH_VEC( m_mapItemSets[idx].m_ItemEntries, nEntry )
		{
			item_list_entry_t &itemListEntry = m_mapItemSets[idx].m_ItemEntries[ nEntry ];
			CEconItemDefinition *pItemDef = GetItemDefinitionMutable( itemListEntry.m_nItemDef );
			if ( pItemDef )
			{
				pItemDef->AddItemSet( idx );
			}
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the timed rewards section of the schema
// Input:	pKVTimedRewards - The timed_rewards section of the KeyValues 
//				representation of the schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitTimedRewards( KeyValues *pKVTimedRewards, CUtlVector<CUtlString> *pVecErrors )
{
	m_vecTimedRewards.Purge();

	// initialize the rewards sections
	if ( NULL != pKVTimedRewards )
	{
		FOR_EACH_TRUE_SUBKEY( pKVTimedRewards, pKVTimedReward )
		{
			int index = m_vecTimedRewards.AddToTail();
			SCHEMA_INIT_SUBSTEP( m_vecTimedRewards[index].BInitFromKV( pKVTimedReward, *this, pVecErrors ) );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
const CTimedItemRewardDefinition* CEconItemSchema::GetTimedReward( eTimedRewardType type ) const
{
	if ( (int)type < m_vecTimedRewards.Count() )
	{
		return &m_vecTimedRewards[type];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	
//-----------------------------------------------------------------------------
const CEconLootListDefinition* CEconItemSchema::GetLootListByName( const char* pListName, int *out_piIndex ) const
{
	int iIdx = m_dictLootLists.Find( pListName );
	if ( out_piIndex )
		*out_piIndex = iIdx;
	if ( m_dictLootLists.IsValidIndex( iIdx ) )
		return &m_dictLootLists[iIdx];
	else
		return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the loot lists section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitLootLists( KeyValues *pKVLootLists, KeyValues *pKVRandomAttributeTemplates, CUtlVector<CUtlString> *pVecErrors, bool bServerLists )
{
 	FOR_EACH_TRUE_SUBKEY( pKVLootLists, pKVLootList )
	{
		const char* listName = pKVLootList->GetName();

		SCHEMA_INIT_CHECK( listName != NULL, CFmtStr( "All lootlists must have names.") );

		if ( m_dictLootLists.Count() > 0 )
		{
			SCHEMA_INIT_CHECK( GetLootListByName( listName ) == NULL, CFmtStr( "Duplicate lootlist name (%s) found!", listName ) );
		}

		int idx = m_dictLootLists.Insert( listName );
		SCHEMA_INIT_SUBSTEP( m_dictLootLists[idx].BInitFromKV( pKVLootList, pKVRandomAttributeTemplates, *this, pVecErrors, bServerLists ) );
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the revolving loot lists section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitRevolvingLootLists( KeyValues *pKVLootLists, CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_SUBKEY( pKVLootLists, pKVList )
	{
		int iListIdx = atoi( pKVList->GetName() );
		const char* strListName = pKVList->GetString();
		m_mapRevolvingLootLists.Insert( iListIdx, strListName );
	}

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose:	Initializes the quest reward loot lists section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitQuestRewardLootLists( KeyValues *pKVLootLists, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapQuestRewardLootLists.Purge();

	if ( NULL != pKVLootLists )
	{
		FOR_EACH_SUBKEY( pKVLootLists, pKVList )
		{
			int iListIdx = atoi( pKVList->GetName() );
			const char* strListName = pKVList->GetString();
			m_mapQuestRewardLootLists.Insert( iListIdx, strListName );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Alternate Icons
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitAlternateIcons( KeyValues *pKVAlternateIcons, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapAlternateIcons.Purge();

	FOR_EACH_TRUE_SUBKEY( pKVAlternateIcons, pKVBlock )
	{
		bool bSprayTints = !V_stricmp( pKVBlock->GetName(), "spray_tint_icons" );

		FOR_EACH_TRUE_SUBKEY( pKVBlock, pKVAlternateIcon )
		{
			if ( bSprayTints )
			{
				// Custom readable syntax for spray tints:
				uint32 unSprayKitID = 0, unTintID1 = 0, unTintID2 = 0;
				if ( 3 == sscanf( pKVAlternateIcon->GetName(), "%u:%u:%u", &unSprayKitID, &unTintID1, &unTintID2 ) )
				{
					CFmtStr fmtValueLookup( "icon_path_range:%u:%u", unTintID1, unTintID2 );
					char const *szIconPath = pKVAlternateIcon->GetString( fmtValueLookup );
					if ( szIconPath && *szIconPath )
					{
						for ( uint32 unTintID = unTintID1; unTintID <= unTintID2; ++unTintID )
						{
							uint64 ullAltIconKey = Helper_GetAlternateIconKeyForTintedStickerItem( unSprayKitID, unTintID );
							BInitAlternateIcon( ullAltIconKey, CFmtStr( "%s_%u", szIconPath, unTintID ), pKVAlternateIcon, pVecErrors );
						}
					}
					else
					{
						SCHEMA_INIT_CHECK( false,
							CFmtStr( "Alternate icon '%s' > '%s' must define '%s'", pKVBlock->GetName(), pKVAlternateIcon->GetName(), fmtValueLookup.Get() ) );
					}
				}
			}
			else
			{
				uint64 ullAltIconKey = V_atoui64( pKVAlternateIcon->GetName() );
				char const *szIconPath = pKVAlternateIcon->GetString( "icon_path" );
				if ( szIconPath && *szIconPath )
				{
					BInitAlternateIcon( ullAltIconKey, szIconPath, pKVAlternateIcon, pVecErrors );
				}
				else
				{
					SCHEMA_INIT_CHECK( false,
						CFmtStr( "Alternate icon '%s' > '%s' must define '%s'", pKVBlock->GetName(), pKVAlternateIcon->GetName(), "icon_path" ) );
				}
			}
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

bool CEconItemSchema::BInitAlternateIcon( uint64 ullAltIconKey, char const *szSimpleName, KeyValues *pKVAlternateIcon, CUtlVector<CUtlString> *pVecErrors )
{
	SCHEMA_INIT_CHECK(
		!m_mapAlternateIcons.IsValidIndex( m_mapAlternateIcons.Find( ullAltIconKey ) ),
		CFmtStr( "Duplicate alternate icon definition '%s' (%llu)", pKVAlternateIcon->GetName(), ullAltIconKey ) );

	SCHEMA_INIT_CHECK( 
		int64( ullAltIconKey ) > 0,
		CFmtStr( "Alternate icon definition index '%s' (%llu) must be greater than zero", pKVAlternateIcon->GetName(), ullAltIconKey ) );

	int nNew = m_mapAlternateIcons.Insert( ullAltIconKey );
	m_mapAlternateIcons[ nNew ].sSimpleName = szSimpleName;
	m_mapAlternateIcons[ nNew ].sLargeSimpleName.Format( "%s_large", szSimpleName );

	return true;
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Web Resources
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitWebResources( KeyValues *pKVWebResources, CUtlVector<CUtlString> *pVecErrors )
{
	if ( CWebResource::s_Initialized || !pKVWebResources )
		return true;

	FOR_EACH_SUBKEY( pKVWebResources, pDef )
	{
		CWebResource* pWebResource = new CWebResource;
		pWebResource->m_strName = pDef->GetString( "name" );
		pWebResource->m_strURL = pDef->GetString( "url" );
		pWebResource->m_bOnDemand = pDef->GetBool( "on_demand" );
		pWebResource->m_pKeyValues = NULL;
		pWebResource->m_fnLoadCallback = NULL;
		m_dictWebResources.Insert( pDef->GetString( "name" ), pWebResource );
	}

	if ( CommandLine()->CheckParm( "-enabledownloadtest" ) )
	{
		CWebResource* pWebResource = new CWebResource;
		pWebResource->m_strName = "Test";
		pWebResource->m_strURL = "http://media.steampowered.com/apps/570/test/test.txt";
		pWebResource->m_bOnDemand = true;
		pWebResource->m_pKeyValues = NULL;
		pWebResource->m_fnLoadCallback = NULL;
		m_dictWebResources.Insert( "Test", pWebResource );
	}

	// If any of our resources need to be requested immediately, do it now.
	FOR_EACH_DICT_FAST( m_dictWebResources, idx )
	{
		CWebResource* pWebResource = m_dictWebResources[idx];
		if ( !pWebResource )
			continue;

		if ( !pWebResource->m_bOnDemand )
		{
			LoadWebResource( pWebResource->m_strName, NULL );
		}
	}

	CWebResource::s_Initialized = true;

	return SCHEMA_INIT_SUCCESS();
}

EWebResourceStatus CEconItemSchema::LoadWebResource( CUtlString strName, void (*fnCallback)( const char*, KeyValues* ), bool bForceReload )
{
	return kWebResource_NotLoaded;
}

void CEconItemSchema::SetWebResource( CUtlString strName, KeyValues* pResourceKV )
{
	int idx = m_dictWebResources.Find( strName.Get() );
	if ( !m_dictWebResources.IsValidIndex( idx ) )
		return;

	CWebResource* pResource = m_dictWebResources[idx];
	if ( pResource->m_pKeyValues )
	{
		pResource->m_pKeyValues->deleteThis();
		pResource->m_pKeyValues = NULL;
	}
	pResource->m_pKeyValues = pResourceKV;
	if ( pResource->m_fnLoadCallback )
		pResource->m_fnLoadCallback( strName.Get(), pResourceKV );
}

#endif

bool CEconItemSchema::BInitProPlayers( KeyValues *pKVData, CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_SUBKEY( pKVData, pDef )
	{
		// Create the pro player entry
		CProPlayerData *pData = new CProPlayerData;
		if ( !pData->BInitFromKeyValues( pDef, pVecErrors ) )
		{
			delete pData;
			continue;
		}

		if ( m_mapProPlayersByAccountID.Find( pData->GetAccountID() ) != m_mapProPlayersByAccountID.InvalidIndex() )
		{
			SCHEMA_INIT_CHECK( false,
				CFmtStr( "Pro-player entry '%s' maps to a duplicate AccountID", pDef->GetName() ) );
			Assert( false );
			delete pData;
			continue;
		}

		if ( m_mapProPlayersByCode.Find( pData->GetCode() ) != UTL_INVAL_SYMBOL )
		{
			SCHEMA_INIT_CHECK( false,
				CFmtStr( "Pro-player entry '%s' has a duplicate code '%s'", pDef->GetName(), pData->GetCode() ) );
			Assert( false );
			delete pData;
			continue;
		}

		m_mapProPlayersByAccountID.InsertOrReplace( pData->GetAccountID(), pData );
		FOR_EACH_TRUE_SUBKEY( pDef->FindKey( "events" ), kvEvent )
		{
			int nEventID = Q_atoi( kvEvent->GetName() );
			int nTeamID = kvEvent->GetInt( "team" );
			if ( nEventID && nTeamID )
			{
				uint64 uiKey = ( uint64( uint32( nEventID ) ) << 32 ) | uint32( nTeamID );
				MapProPlayersByEventIDTeamID_t::IndexType_t idx = m_mapProPlayersByEventIDTeamID.Find( uiKey );
				if ( idx == m_mapProPlayersByEventIDTeamID.InvalidIndex() )
					idx = m_mapProPlayersByEventIDTeamID.InsertOrReplace( uiKey, new CUtlVector< const CProPlayerData * > );
				m_mapProPlayersByEventIDTeamID.Element( idx )->AddToTail( pData );
			}
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

const CProPlayerData * CEconItemSchema::GetProPlayerDataByAccountID( uint32 unAccountID ) const
{
	MapProPlayersByAccountID_t::IndexType_t idx = m_mapProPlayersByAccountID.Find( unAccountID );
	return ( idx == m_mapProPlayersByAccountID.InvalidIndex() ) ? NULL : m_mapProPlayersByAccountID.Element( idx );
}

const CUtlVector< const CProPlayerData * > * CEconItemSchema::GetProPlayersDataForEventIDTeamID( int nEventID, int nTeamID ) const
{
	uint64 uiKey = ( uint64( uint32( nEventID ) ) << 32 ) | uint32( nTeamID );
	MapProPlayersByEventIDTeamID_t::IndexType_t idx = m_mapProPlayersByEventIDTeamID.Find( uiKey );
	return ( idx != m_mapProPlayersByEventIDTeamID.InvalidIndex() )
		? m_mapProPlayersByEventIDTeamID.Element( idx ) : NULL;
}

bool CProPlayerData::BInitFromKeyValues( KeyValues *pDef, CUtlVector<CUtlString> *pVecErrors /* = NULL */ )
{
	uint32 unAccountID = Q_atoi( pDef->GetName() );
	SCHEMA_INIT_CHECK(
		unAccountID != 0,
		CFmtStr( "Pro-player entry '%s' doesn't have a valid AccountID", pDef->GetName() ) );
	if ( !unAccountID )
		return false;
	m_nAccountID = unAccountID;

	char const *szName = pDef->GetString( "name" );
	if ( !szName || !*szName )
	{
		SCHEMA_INIT_CHECK( false,
			CFmtStr( "Pro-player entry '%s' has no name", pDef->GetName() ) );
		Assert( false );
		return false;
	}
	m_sName = szName;

	char const *szCode = pDef->GetString( "code" );
	if ( !szCode || !*szCode )
	{
		SCHEMA_INIT_CHECK( false,
			CFmtStr( "Pro-player entry '%s' has no code", pDef->GetName() ) );
		Assert( false );
		return false;
	}
	HelperValidateLocalizationStringToken( CFmtStr( "#SFUI_ProPlayer_%s", szCode ) );
	m_sCode = szCode;

	char const *szGeo = pDef->GetString( "geo" );
	SCHEMA_INIT_CHECK( szGeo && *szGeo,
		CFmtStr( "Pro-player entry '%s' has no geo defined", pDef->GetName() ) );
	Assert( szGeo && *szGeo );
	HelperValidateLocalizationStringToken( CFmtStr( "#SFUI_Country_%s", szGeo ) );
	m_sGeo = szGeo;

	m_pKVItem = pDef->MakeCopy();

	return true; // typically it should be return SCHEMA_INIT_SUCCESS(); but we don't want spurious "doesn't have a matching pro-player entry" errors
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the recipes section of the schema
// Input:	pKVRecipes - The recipes section of the KeyValues 
//				representation of the schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitRecipes( KeyValues *pKVRecipes, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapRecipes.Purge();

	// initialize the rewards sections
	if ( NULL != pKVRecipes )
	{
		int nHighestRecipeIndex = 0;
		FOR_EACH_TRUE_SUBKEY( pKVRecipes, pKVRecipe )
		{
			int nRecipeIndex = Q_atoi( pKVRecipe->GetName() );

			// Remember highest recipe index so we can make more past that 
			if ( nHighestRecipeIndex < nRecipeIndex + 1 )
			{
				nHighestRecipeIndex = nRecipeIndex + 1;
			}

			int nMapIndex = m_mapRecipes.Find( nRecipeIndex );

			// Make sure the recipe index is correct because we use this index as a reference
			SCHEMA_INIT_CHECK( 
				!m_mapRecipes.IsValidIndex( nMapIndex ),
				CFmtStr( "Duplicate recipe definition (%d)", nRecipeIndex ) );

			// Check to make sure the index is positive
			SCHEMA_INIT_CHECK( 
				nRecipeIndex >= 0,
				CFmtStr( "Recipe definition index %d must be greater than or equal to zero", nRecipeIndex ) );

			CEconCraftingRecipeDefinition *recipeDef = CreateCraftingRecipeDefinition();
			SCHEMA_INIT_SUBSTEP( recipeDef->BInitFromKV( pKVRecipe, *this, pVecErrors ) );

			// On clients, toss out any recipes that aren't always known. (Really, the clients
			// shouldn't ever get these, but just in case.)
#if defined(CLIENT_DLL) || defined(GAME_DLL)
			if ( recipeDef->IsAlwaysKnown() )
#endif // !GC_DLL
			{
#ifdef _DEBUG
				// Sanity check in debug builds so that we know we aren't putting the same recipe in
				// multiple times.
				FOR_EACH_MAP_FAST( m_mapRecipes, i )
				{
					Assert( i != nRecipeIndex );
					Assert( m_mapRecipes[i] != recipeDef );
				}
#endif // _DEBUG

				// Store this recipe.
				m_mapRecipes.Insert( nRecipeIndex, recipeDef );
			}
		}

#ifdef CSTRIKE15
		int nSetCount = GetItemSetCount();
		for ( int i = 0; i < nSetCount; ++i )
		{
			const IEconItemSetDefinition *pSet = GetItemSet( i );
			if ( !pSet || pSet->GetCraftReward() <= 0 )
				continue;

			CEconCraftingRecipeDefinition *recipeDef = CreateCraftingRecipeDefinition();
			SCHEMA_INIT_SUBSTEP( recipeDef->BInitFromSet( pSet, *this, pVecErrors ) );
			recipeDef->SetDefinitionIndex( nHighestRecipeIndex );
			recipeDef->SetFilter( CRAFT_FILTER_COLLECT );

			// Disabled for now until we ship collection badges!
			recipeDef->SetDisabled( true );

			m_mapRecipes.Insert( nHighestRecipeIndex, recipeDef );

			nHighestRecipeIndex++;
		}
#endif //#ifdef CSTRIKE15
	}

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose:	Builds the name of a achievement in the form App<ID>.<AchName>
// Input:	unAppID - native app ID
//			pchNativeAchievementName - name of the achievement in its native app
// Returns: The combined achievement name
//-----------------------------------------------------------------------------
CUtlString CEconItemSchema::ComputeAchievementName( AppId_t unAppID, const char *pchNativeAchievementName ) 
{
	return CFmtStr1024( "App%u.%s", unAppID, pchNativeAchievementName ).Access();
}


//-----------------------------------------------------------------------------
// Purpose:	Initializes the achievement rewards section of the schema
// Input:	pKVAchievementRewards - The achievement_rewards section of the KeyValues 
//				representation of the schema
//			pVecErrors - An optional vector that will contain error messages if 
//				the init fails.
// Output:	True if initialization succeeded, false otherwise
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitAchievementRewards( KeyValues *pKVAchievementRewards, CUtlVector<CUtlString> *pVecErrors )
{
	m_dictAchievementRewards.Purge();
	m_mapAchievementRewardsByData.PurgeAndDeleteElements();

	// initialize the rewards sections
	if ( NULL != pKVAchievementRewards )
	{
		FOR_EACH_SUBKEY( pKVAchievementRewards, pKVReward )
		{
			AchievementAward_t award;
			if( pKVReward->GetDataType() == KeyValues::TYPE_NONE )
			{
				int32 nItemIndex = pKVReward->GetInt( "DefIndex", -1 );
				if( nItemIndex != -1 )
				{
					award.m_vecDefIndex.AddToTail( (uint16)nItemIndex );			
				}
				else
				{
					KeyValues *pkvItems = pKVReward->FindKey( "Items" );
					SCHEMA_INIT_CHECK( 
						pkvItems != NULL,
						CFmtStr( "Complex achievement %s must have an Items key or a DefIndex field", pKVReward->GetName() ) );
					if( !pkvItems )
					{
						continue;
					}

					FOR_EACH_VALUE( pkvItems, pkvItem )
					{
						award.m_vecDefIndex.AddToTail( (uint16)Q_atoi( pkvItem->GetName() ) );
					}
				}

			}
			else
			{
				award.m_vecDefIndex.AddToTail( (uint16)pKVReward->GetInt("", -1 ) );
			} 
		
			// make sure all the item types are valid
			bool bFoundAllItems = true;
			FOR_EACH_VEC( award.m_vecDefIndex, nItem )
			{
				const CEconItemDefinition *pDefn = GetItemDefinition( award.m_vecDefIndex[nItem] );
				SCHEMA_INIT_CHECK( 
					pDefn != NULL,
					CFmtStr( "Item definition index %d in achievement reward %s was not found", award.m_vecDefIndex[nItem], pKVReward->GetName() ) );
				if( !pDefn )
				{
					bFoundAllItems = false;
				}
			}
			if( !bFoundAllItems )
				continue;

			SCHEMA_INIT_CHECK( 
				award.m_vecDefIndex.Count() > 0,
				CFmtStr( "Achievement reward %s has no items!", pKVReward->GetName() ) );
			if( award.m_vecDefIndex.Count() == 0 )
				continue;

			award.m_unSourceAppId = k_uAppIdInvalid;
			if( pKVReward->GetDataType() == KeyValues::TYPE_NONE )
			{
				// cross game achievement
				award.m_sNativeName = pKVReward->GetName();
				award.m_unAuditData = pKVReward->GetInt( "AuditData", 0 );
				award.m_unSourceAppId = pKVReward->GetInt( "SourceAppID", award.m_unSourceAppId );
			}
			else
			{
				award.m_sNativeName = pKVReward->GetName();
				award.m_unAuditData = 0;
			}

			AchievementAward_t *pAward = new AchievementAward_t;
			*pAward = award;

			m_dictAchievementRewards.Insert( ComputeAchievementName( pAward->m_unSourceAppId, pAward->m_sNativeName ), pAward );
			m_mapAchievementRewardsByData.Insert( pAward->m_unAuditData, pAward );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

#ifdef TF_CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Go through all items and cache the number of concrete items in each.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitConcreteItemCounts( CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_MAP_FAST( m_mapItems, i )
	{
		CEconItemDefinition *pItemDef = m_mapItems[ i ];
		pItemDef->m_unNumConcreteItems = CalculateNumberOfConcreteItems( pItemDef );
	}

	return true;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Returns the number of actual "real" items referenced by the item definition
// (i.e. items that would take up space in the inventory)
//-----------------------------------------------------------------------------
int CEconItemSchema::CalculateNumberOfConcreteItems( const CEconItemDefinition *pItemDef )
{
	AssertMsg( pItemDef, "NULL item definition!  This should not happen!" );
	if ( !pItemDef )
		return 0;

	if ( pItemDef->IsBundle() )
	{
		uint32 unNumConcreteItems = 0;
		
		const bundleinfo_t *pBundle = pItemDef->GetBundleInfo();
		Assert( pBundle );

		FOR_EACH_VEC( pBundle->vecItemEntries, i )
		{
			unNumConcreteItems += CalculateNumberOfConcreteItems(  GetItemDefinition( pBundle->vecItemEntries[i].m_nItemDef ) );
		}

		return unNumConcreteItems;
	}

	if ( pItemDef->GetItemClass() && !Q_strcmp( pItemDef->GetItemClass(), "map_token" ) )
		return 0;

	return 1;
}


bool CEconItemSchema::BInitStickerKits( KeyValues *pKVStickerKits, CUtlVector<CUtlString> *pVecErrors )
{
	CStickerKit *pDefault = NULL;
	FOR_EACH_TRUE_SUBKEY( pKVStickerKits, pKVEntry )
	{
		const char *pchName = pKVEntry->GetString( "name", "" );
		if ( !pchName || pchName[ 0 ] == '\0' )
			continue;

		CStickerKit *pStickerKit = new CStickerKit();
		if ( pStickerKit )
		{
			pStickerKit->nID = atoi( pKVEntry->GetName() );

			if ( !pDefault )
			{
				if ( pStickerKit->nID == 0 )
					pDefault = pStickerKit;
				else
					pDefault = m_mapStickerKits.Element( m_mapStickerKits.Find( 0 ) );
			}

			if ( !pStickerKit->InitFromKeyValues( pKVEntry, pDefault, pVecErrors ) )
			{
				SCHEMA_INIT_CHECK( 
					false,
					CFmtStr( "Failed to initialize sticker kit ID %d '%s'", pStickerKit->nID, pStickerKit->sName.Get() ) );
				continue;
			}

			if( m_mapStickerKits.Find( pStickerKit->nID ) != m_mapStickerKits.InvalidIndex() )
			{
				SCHEMA_INIT_CHECK( 
					false,
					CFmtStr( "Duplicate sticker kit ID %d", pStickerKit->nID ) );
				continue;
			}

			if( m_dictStickerKits.HasElement( pStickerKit->sName ) )
			{
				SCHEMA_INIT_CHECK( 
					false,
					CFmtStr( "Duplicate sticker kit name '%s'", pStickerKit->sName.Get() ) );
				continue;
			}

			m_mapStickerKits.Insert( pStickerKit->nID, pStickerKit );
			m_dictStickerKits.Insert( pStickerKit->sName.Get(), pStickerKit );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

bool CEconItemSchema::BInitStickerLists( KeyValues *pKVStickerLists, CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_TRUE_SUBKEY( pKVStickerLists, pKVEntry )
	{
		char const *szName = pKVEntry->GetName();
		if ( !szName || !*szName )
			continue;

		if( m_dictStickerKits.HasElement( szName ) )
		{
			SCHEMA_INIT_CHECK( 
				false,
				CFmtStr( "Sticker list name duplicating sticker kit name '%s'", szName ) );
			continue;
		}

		if( m_dictStickerLists.HasElement( szName ) )
		{
			SCHEMA_INIT_CHECK( 
				false,
				CFmtStr( "Duplicate sticker list name '%s'", szName ) );
			continue;
		}

		CStickerList *pStickerList = new CStickerList();
		if ( !pStickerList->InitFromKeyValues( pKVEntry, pVecErrors ) )
		{
			SCHEMA_INIT_CHECK( 
				false,
				CFmtStr( "Failed to initialize sticker list '%s'", szName ) );
			continue;
		}

		m_dictStickerLists.Insert( szName, pStickerList );
	}

	return SCHEMA_INIT_SUCCESS();
}

bool CEconItemSchema::BInitPaintKits( KeyValues *pKVPaintKits, CUtlVector<CUtlString> *pVecErrors )
{
	CPaintKit *pDefault = NULL;

	FOR_EACH_TRUE_SUBKEY( pKVPaintKits, pKVEntry )
	{
		const char *pchName = pKVEntry->GetString( "name", "" );
		if ( !pchName || pchName[ 0 ] == '\0' )
			continue;

		CPaintKit *pPaintKit = new CPaintKit();
		if ( pPaintKit )
		{
			pPaintKit->nID = atoi( pKVEntry->GetName() );

			if ( !pDefault )
			{
				if ( pPaintKit->nID == 0 )
					pDefault = pPaintKit;
				else
					pDefault = m_mapPaintKits.Element( m_mapPaintKits.Find( 0 ) );
			}

			pPaintKit->InitFromKeyValues( pKVEntry, pDefault );
				
			m_mapPaintKits.Insert( pPaintKit->nID, pPaintKit );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

bool CEconItemSchema::BInitPaintKitsRarity( KeyValues *pKVPaintKitsRarity, CUtlVector<CUtlString> *pVecErrors )
{
	CPaintKit *pDefault = const_cast< CPaintKit* >( GetPaintKitDefinitionByName( "default" ) );
	if ( !pDefault )
	{
		SCHEMA_INIT_CHECK( 
			false,
			CFmtStr( "Unable to find \"default\" paint kit in \"paint_kits_rarity\"" ) );
		return false;
	}

	uint8 nRarity;
	GetItemSchema()->BGetItemRarityFromName( pKVPaintKitsRarity->GetString( "default", "common" ), &nRarity );
	pDefault->nRarity = nRarity;

	FOR_EACH_SUBKEY( pKVPaintKitsRarity, pKVEntry )
	{
		CPaintKit *pPaintKit = const_cast< CPaintKit* >( GetPaintKitDefinitionByName( pKVEntry->GetName() ) );
		if ( !pPaintKit )
			continue;

		const char *pchRarity = pKVEntry->GetString();
		if ( pchRarity )
		{
			uint8 nRarity;
			GetItemSchema()->BGetItemRarityFromName( pchRarity, &nRarity );
			pPaintKit->nRarity = nRarity;
		}
		else
		{
			pPaintKit->nRarity = pDefault->nRarity;
		}
	}

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitMusicDefs( KeyValues *pKVMusicDefs, CUtlVector<CUtlString> *pVecErrors )
{
	FOR_EACH_TRUE_SUBKEY( pKVMusicDefs, pKVMusicDef )
	{
		CEconMusicDefinition *pNewMusicDef = new CEconMusicDefinition;

		SCHEMA_INIT_SUBSTEP( pNewMusicDef->BInitFromKV( pKVMusicDef, *this, pVecErrors ) );

		if( m_mapMusicDefs.Find( pNewMusicDef->GetID() ) != m_mapMusicDefs.InvalidIndex() )
		{
			SCHEMA_INIT_CHECK( 
				false,
				CFmtStr( "Duplicate music definition id %d", pNewMusicDef->GetID() ) );
			continue;
		}

		m_mapMusicDefs.Insert( pNewMusicDef->GetID(), pNewMusicDef );
	}

	return SCHEMA_INIT_SUCCESS();
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitQuestDefs( KeyValues *pKVQuestDefs, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapQuestDefs.PurgeAndDeleteElements();

	// initialize the Quest definitions
	if ( NULL != pKVQuestDefs )
	{
		FOR_EACH_TRUE_SUBKEY( pKVQuestDefs, pKVQuestDef )
		{
			CEconQuestDefinition *pNewQuestDef = new CEconQuestDefinition;

			SCHEMA_INIT_SUBSTEP( pNewQuestDef->BInitFromKV( pKVQuestDef, *this, pVecErrors ) );

			if ( m_mapQuestDefs.Find( pNewQuestDef->GetID() ) != m_mapQuestDefs.InvalidIndex() )
			{
				SCHEMA_INIT_CHECK(
					false,
					CFmtStr( "Duplicate Quest definition id %d", pNewQuestDef->GetID() ) );
				continue;
			}

			m_mapQuestDefs.Insert( pNewQuestDef->GetID(), pNewQuestDef );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}


bool CEconItemSchema::BInitQuestEvents( KeyValues *pKVQuestSchedule, CUtlVector<CUtlString> *pVecErrors )
{
	m_vecQuestEvents.Purge();

	// Removed for partner depot

	return SCHEMA_INIT_SUCCESS();
}

const QuestEventsSchedule_t& CEconItemSchema::GetAndUpdateQuestEventsSchedule()
{
	return m_mapQuestEventsSchedule;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitCampaignDefs( KeyValues *pKVCampaignDefs, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapCampaignDefs.PurgeAndDeleteElements();

	// initialize the Campaign definitions
	if ( NULL != pKVCampaignDefs )
	{
		FOR_EACH_TRUE_SUBKEY( pKVCampaignDefs, pKVCampaignDef )
		{
			CEconCampaignDefinition *pNewCampaignDef = new CEconCampaignDefinition;

			SCHEMA_INIT_SUBSTEP( pNewCampaignDef->BInitFromKV( pKVCampaignDef, *this, pVecErrors ) );

			if ( m_mapCampaignDefs.Find( pNewCampaignDef->GetID() ) != m_mapCampaignDefs.InvalidIndex() )
			{
				SCHEMA_INIT_CHECK(
					false,
					CFmtStr( "Duplicate Campaign definition id %d", pNewCampaignDef->GetID() ) );
				continue;
			}

			m_mapCampaignDefs.Insert( pNewCampaignDef->GetID(), pNewCampaignDef );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}


attachedparticlesystem_t CEconItemSchema::GetAttachedParticleSystemInfo( KeyValues* pKVEntry, int32 nItemIndex ) const
{
	attachedparticlesystem_t system;

	system.pEffectKV = pKVEntry;
	system.pszResourceName = pKVEntry->GetString( "resource", NULL );
	system.pszSystemName = pKVEntry->GetString( "system", NULL );
	system.pszAttachmentName = pKVEntry->GetString( "attachment", NULL );
	system.bFollowRootBone = pKVEntry->GetInt( "attach_to_rootbone", 0 ) != 0;
	system.bFlyingCourier = pKVEntry->GetInt( "flying_courier_effect", 0 ) != 0;
	system.iCustomType = pKVEntry->GetInt( "custom_type", 0 );
	system.iCount = 0;
	system.nSystemID = nItemIndex;
	system.nRootAttachType = StringFieldToInt( pKVEntry->GetString("attach_type"), g_szParticleAttachTypes, ARRAYSIZE(g_szParticleAttachTypes) );
	int iAttachToEnt = StringFieldToInt( pKVEntry->GetString("attach_entity"), g_szParticleAttachToEnt, ARRAYSIZE(g_szParticleAttachToEnt) );
	system.nAttachToEntity = ( iAttachToEnt == -1 ) ? ATTPART_TO_SELF : (attachedparticle_toent_t)iAttachToEnt;

	// Find any control point settings
	KeyValues *pKVControlPoints = pKVEntry->FindKey( "control_points" );
	if ( pKVControlPoints )
	{
		FOR_EACH_TRUE_SUBKEY( pKVControlPoints, pKVPoint )
		{
			int iIdx = system.vecControlPoints.AddToTail();
			system.vecControlPoints[iIdx].nControlPoint = pKVPoint->GetInt( "control_point_index", -1 );
			system.vecControlPoints[iIdx].nAttachType = StringFieldToInt( pKVPoint->GetString("attach_type"), g_szParticleAttachTypes, ARRAYSIZE(g_szParticleAttachTypes) );
			system.vecControlPoints[iIdx].pszAttachmentName = pKVPoint->GetString( "attachment", NULL );
			float flVec[3];
			V_StringToVector( flVec, pKVPoint->GetString( "position", "0 0 0" ) );
			system.vecControlPoints[iIdx].vecPosition = Vector( flVec[0], flVec[1], flVec[2] );
		}
	}

	return system;
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the attribute-controlled-particle-systems section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitAttributeControlledParticleSystems( KeyValues *pKVParticleSystems, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapAttributeControlledParticleSystems.Purge();
	if ( NULL != pKVParticleSystems )
	{
		FOR_EACH_TRUE_SUBKEY( pKVParticleSystems, pKVEntry )
		{
			int32 nItemIndex = atoi( pKVEntry->GetName() );
			// Check to make sure the index is positive
			SCHEMA_INIT_CHECK( 
				nItemIndex > 0,
				CFmtStr( "Particle system index %d greater than zero", nItemIndex ) );
			if ( nItemIndex <= 0 )
				continue;
			int iIndex = m_mapAttributeControlledParticleSystems.Insert( nItemIndex );
			attachedparticlesystem_t *system = &m_mapAttributeControlledParticleSystems[iIndex];
			*system = GetAttachedParticleSystemInfo( pKVEntry, nItemIndex );
		}
	}
	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose: Inits data for items that can level up through kills, etc.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitItemLevels( KeyValues *pKVItemLevels, CUtlVector<CUtlString> *pVecErrors )
{
	m_vecItemLevelingData.RemoveAll();

	// initialize the rewards sections
	if ( NULL != pKVItemLevels )
	{
		FOR_EACH_TRUE_SUBKEY( pKVItemLevels, pKVItemLevelBlock )
		{
			const char *pszLevelBlockName = pKVItemLevelBlock->GetName();
			SCHEMA_INIT_CHECK( GetItemLevelingData( pszLevelBlockName ) == NULL,
				CFmtStr( "Duplicate leveling data block named \"%s\".", pszLevelBlockName ) );

			// Allocate a new structure for this block and assign it. We'll fill in the contents later.
			CUtlVector<CItemLevelingDefinition> *pLevelingData = new CUtlVector<CItemLevelingDefinition>;
			m_vecItemLevelingData.Insert( pszLevelBlockName, pLevelingData );

			FOR_EACH_TRUE_SUBKEY( pKVItemLevelBlock, pKVItemLevel )
			{
				int index = pLevelingData->AddToTail();
				SCHEMA_INIT_SUBSTEP( (*pLevelingData)[index].BInitFromKV( pKVItemLevel, *this, pszLevelBlockName, pVecErrors ) );
			}
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose: Inits data for kill eater types.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitKillEaterScoreTypes( KeyValues *pKVKillEaterScoreTypes, CUtlVector<CUtlString> *pVecErrors )
{
	m_mapKillEaterScoreTypes.RemoveAll();

	// initialize the rewards sections
	if ( NULL != pKVKillEaterScoreTypes )
	{
		FOR_EACH_TRUE_SUBKEY( pKVKillEaterScoreTypes, pKVScoreType )
		{
			unsigned int unIndex = (unsigned int)atoi( pKVScoreType->GetName() );
			SCHEMA_INIT_CHECK( m_mapKillEaterScoreTypes.Find( unIndex ) == KillEaterScoreMap_t::InvalidIndex(),
				CFmtStr( "Duplicate kill eater score type index %u.", unIndex ) );
			
			kill_eater_score_type_t ScoreType;
			ScoreType.m_nValue = V_atoi( pKVScoreType->GetName() );
			ScoreType.m_pszTypeString = pKVScoreType->GetString( "type_name" );
			ScoreType.m_pszModelAttributeString = pKVScoreType->GetString( "model_attribute" );
#ifdef CLIENT_DLL
			HelperValidateLocalizationStringToken( CFmtStr( "#KillEaterDescriptionNotice_%s", ScoreType.m_pszTypeString ) );
			HelperValidateLocalizationStringToken( CFmtStr( "#KillEaterEventType_%s", ScoreType.m_pszTypeString ) );
#endif

			// Default to on.
			ScoreType.m_bUseLevelBlock = pKVScoreType->GetBool( "use_level_data", 1 );

			const char *pszLevelBlockName = pKVScoreType->GetString( "level_data", "KillEaterRank" );
			SCHEMA_INIT_CHECK( GetItemLevelingData( pszLevelBlockName ) != NULL,
				CFmtStr( "Unable to find leveling data block named \"%s\" for kill eater score type %u.", pszLevelBlockName, unIndex ) );

			ScoreType.m_pszLevelBlockName = pszLevelBlockName;

			m_mapKillEaterScoreTypes.Insert( unIndex, ScoreType );
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const ISchemaAttributeType *CEconItemSchema::GetAttributeType( const char *pszAttrTypeName ) const
{
	FOR_EACH_VEC( m_vecAttributeTypes, i )
	{
		if ( m_vecAttributeTypes[i].m_sName == pszAttrTypeName )
			return m_vecAttributeTypes[i].m_pAttrType;
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// CItemLevelingDefinition Accessor
//-----------------------------------------------------------------------------
const CItemLevelingDefinition *CEconItemSchema::GetItemLevelForScore( const char *pszLevelBlockName, uint32 unScore ) const
{
	const CUtlVector<CItemLevelingDefinition> *pLevelingData = GetItemLevelingData( pszLevelBlockName );
	if ( !pLevelingData )
		return NULL;

	if ( pLevelingData->Count() == 0 )
		return NULL;

	FOR_EACH_VEC( (*pLevelingData), i )
	{
		if ( unScore < (*pLevelingData)[i].GetRequiredScore() )
			return &(*pLevelingData)[i];
	}

	return &(*pLevelingData).Tail();
}

//-----------------------------------------------------------------------------
// Kill eater score type accessor
//-----------------------------------------------------------------------------
const kill_eater_score_type_t *CEconItemSchema::FindKillEaterScoreType( uint32 unScoreType ) const
{
	KillEaterScoreMap_t::IndexType_t i = m_mapKillEaterScoreTypes.Find( unScoreType );
	if ( i == KillEaterScoreMap_t::InvalidIndex() )
		return NULL;

	return &m_mapKillEaterScoreTypes[i];
}

//-----------------------------------------------------------------------------
// Kill eater score type accessor
//-----------------------------------------------------------------------------
const char *CEconItemSchema::GetKillEaterScoreTypeLocString( uint32 unScoreType ) const
{
	const kill_eater_score_type_t *pScoreType = FindKillEaterScoreType( unScoreType );

	return pScoreType
		 ? pScoreType->m_pszTypeString
		 : NULL;
}


//-----------------------------------------------------------------------------
// Kill eater score type accessor for "use_level_data"
//-----------------------------------------------------------------------------
bool CEconItemSchema::GetKillEaterScoreTypeUseLevelData( uint32 unScoreType ) const
{
	const kill_eater_score_type_t *pScoreType = FindKillEaterScoreType( unScoreType );
	return pScoreType ? pScoreType->m_bUseLevelBlock : false;
}


//-----------------------------------------------------------------------------
// Kill eater score type accessor
//-----------------------------------------------------------------------------
const char *CEconItemSchema::GetKillEaterScoreTypeLevelingDataName( uint32 unScoreType ) const
{
	const kill_eater_score_type_t *pScoreType = FindKillEaterScoreType( unScoreType );

	return pScoreType
		 ? pScoreType->m_pszLevelBlockName
		 : NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
econ_tag_handle_t CEconItemSchema::GetHandleForTag( const char *pszTagName )
{
	EconTagDict_t::IndexType_t i = m_dictTags.Find( pszTagName );
	if ( m_dictTags.IsValidIndex( i ) )
		return i;

	return m_dictTags.Insert( pszTagName );
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose:	Clones the specified item definition, and returns the new item def.
//-----------------------------------------------------------------------------
void CEconItemSchema::ItemTesting_CreateTestDefinition( int iCloneFromItemDef, int iNewDef, KeyValues *pNewKV )
{
	int nMapIndex = m_mapItems.Find( iNewDef );
	if ( !m_mapItems.IsValidIndex( nMapIndex ) )
	{
		nMapIndex = m_mapItems.Insert( iNewDef, CreateEconItemDefinition() );
		m_mapItemsSorted.Insert( iNewDef, m_mapItems[nMapIndex] );
	}

	// Find & copy the clone item def's data in
	const CEconItemDefinition *pCloneDef = GetItemDefinition( iCloneFromItemDef );
	if ( !pCloneDef )
		return;
	m_mapItems[nMapIndex]->CopyPolymorphic( pCloneDef );

	// Then stomp it with the KV test contents
	m_mapItems[nMapIndex]->BInitFromTestItemKVs( iNewDef, pNewKV, *this );
}

//-----------------------------------------------------------------------------
// Purpose:	Discards the specified item definition
//-----------------------------------------------------------------------------
void CEconItemSchema::ItemTesting_DiscardTestDefinition( int iDef )
{
	m_mapItems.Remove( iDef );
	m_mapItemsSorted.Remove( iDef );
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the armory data section of the schema
//-----------------------------------------------------------------------------
bool CEconItemSchema::BInitArmoryData( KeyValues *pKVArmoryData, CUtlVector<CUtlString> *pVecErrors )
{
	m_dictArmoryItemDataStrings.Purge();
	m_dictArmoryAttributeDataStrings.Purge();
	if ( NULL != pKVArmoryData )
	{
		KeyValues *pKVItemTypes = pKVArmoryData->FindKey( "armory_item_types" );
		if ( pKVItemTypes )
		{
			FOR_EACH_SUBKEY( pKVItemTypes, pKVEntry )
			{
				const char *pszDataKey = pKVEntry->GetName();
				const char *pszLocString = pKVEntry->GetString();
				m_dictArmoryItemTypesDataStrings.Insert( pszDataKey, pszLocString );
			}
		}

		pKVItemTypes = pKVArmoryData->FindKey( "armory_item_classes" );
		if ( pKVItemTypes )
		{
			FOR_EACH_SUBKEY( pKVItemTypes, pKVEntry )
			{
				const char *pszDataKey = pKVEntry->GetName();
				const char *pszLocString = pKVEntry->GetString();
				m_dictArmoryItemClassesDataStrings.Insert( pszDataKey, pszLocString );
			}
		}

		KeyValues *pKVAttribs = pKVArmoryData->FindKey( "armory_attributes" );
		if ( pKVAttribs )
		{
			FOR_EACH_SUBKEY( pKVAttribs, pKVEntry )
			{
				const char *pszDataKey = pKVEntry->GetName();
				const char *pszLocString = pKVEntry->GetString();
				m_dictArmoryAttributeDataStrings.Insert( pszDataKey, pszLocString );
			}
		}

		KeyValues *pKVItems = pKVArmoryData->FindKey( "armory_items" );
		if ( pKVItems )
		{
			FOR_EACH_SUBKEY( pKVItems, pKVEntry )
			{
				const char *pszDataKey = pKVEntry->GetName();
				const char *pszLocString = pKVEntry->GetString();
				m_dictArmoryItemDataStrings.Insert( pszDataKey, pszLocString );
			}
		}
	}
	return SCHEMA_INIT_SUCCESS();
}
#endif


//-----------------------------------------------------------------------------
// Purpose:	Returns the achievement award that matches the provided defindex or NULL
//			if there is no such award.
// Input:	unData - The data field that would be stored in ItemAudit
//-----------------------------------------------------------------------------
const AchievementAward_t *CEconItemSchema::GetAchievementRewardByDefIndex( uint16 usDefIndex ) const
{
	FOR_EACH_MAP_FAST( m_mapAchievementRewardsByData, nIndex )
	{
		if( m_mapAchievementRewardsByData[nIndex]->m_vecDefIndex.HasElement( usDefIndex ) )
			return m_mapAchievementRewardsByData[nIndex];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	Gets a rarity value for a name.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BGetItemRarityFromName( const char *pchName, uint8 *nRarity ) const
{
	if ( 0 == Q_stricmp( "any", pchName ) )
	{
		*nRarity = k_unItemRarity_Any;
		return true;
	}

	FOR_EACH_MAP_FAST( m_mapRarities, i )
	{
		if ( 0 == Q_stricmp( m_mapRarities[i].GetName(), pchName ) )
		{
			*nRarity = m_mapRarities[i].GetDBValue();
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose:	Gets a quality value for a name.
// Input:	pchName - The name to translate.
//			nQuality - (out)The quality number for this name, if found.
// Output:	True if the string matched a quality for this schema, false otherwise.
//-----------------------------------------------------------------------------
bool CEconItemSchema::BGetItemQualityFromName( const char *pchName, uint8 *nQuality ) const
{
	if ( 0 == Q_stricmp( "any", pchName ) )
	{
		*nQuality = k_unItemQuality_Any;
		return true;
	}

	FOR_EACH_MAP_FAST( m_mapQualities, i )
	{
		if ( 0 == Q_stricmp( m_mapQualities[i].GetName(), pchName ) )
		{
			*nQuality = m_mapQualities[i].GetDBValue();
			return true;
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose:	Gets a quality definition for an index
// Input:	nQuality - The quality to get.
// Output:	A pointer to the desired definition, or NULL if it is not found.
//-----------------------------------------------------------------------------
const CEconItemQualityDefinition *CEconItemSchema::GetQualityDefinition( int nQuality ) const
{ 
	int iIndex = m_mapQualities.Find( nQuality );
	if ( m_mapQualities.IsValidIndex( iIndex ) )
		return &m_mapQualities[iIndex]; 
	return NULL;
}

const CEconItemQualityDefinition *CEconItemSchema::GetQualityDefinitionByName( const char *pszDefName ) const
{
	FOR_EACH_MAP_FAST( m_mapQualities, i )
	{
		if ( !strcmp( pszDefName, m_mapQualities[i].GetName()) )
			return &m_mapQualities[i]; 
	}
	return NULL;
}

const char* CEconItemSchema::GetQualityName( uint8 iQuality )
{
	const CEconItemQualityDefinition* pItemQuality = GetQualityDefinition( iQuality );
	if ( !pItemQuality )
		return NULL;
	else
		return pItemQuality->GetName();
}

int CEconItemSchema::GetQualityIndex( const char* pszQuality )
{
	const CEconItemQualityDefinition* pItemQuality = GetQualityDefinitionByName( pszQuality );
	if ( pItemQuality )
		return pItemQuality->GetDBValue();
	else
		return 0;
}

const CEconItemRarityDefinition *CEconItemSchema::GetRarityDefinitionByMapIndex( int nRarityIndex ) const
{ 
	if ( m_mapRarities.IsValidIndex( nRarityIndex ) )
		return &m_mapRarities[nRarityIndex];

	return NULL;
}

const CEconItemRarityDefinition *CEconItemSchema::GetRarityDefinition( int nRarity ) const
{ 
	int iIndex = m_mapRarities.Find( nRarity );
	if ( m_mapRarities.IsValidIndex( iIndex ) )
		return &m_mapRarities[iIndex]; 
	return NULL;
}

const CEconItemRarityDefinition *CEconItemSchema::GetRarityDefinitionByName( const char *pszDefName ) const
{
	FOR_EACH_MAP_FAST( m_mapRarities, i )
	{
		if ( !strcmp( pszDefName, m_mapRarities[i].GetName()) )
			return &m_mapRarities[i]; 
	}
	return NULL;
}

const char* CEconItemSchema::GetRarityName( uint8 iRarity )
{
	const CEconItemRarityDefinition* pItemRarity = GetRarityDefinition( iRarity );
	if ( !pItemRarity )
		return NULL;
	else
		return pItemRarity->GetName();
}

const char* CEconItemSchema::GetRarityLocKey( uint8 iRarity )
{
	const CEconItemRarityDefinition* pItemRarity = GetRarityDefinition( iRarity );
	if ( !pItemRarity )
		return NULL;
	else
		return pItemRarity->GetLocKey();
}

const char* CEconItemSchema::GetRarityColor( uint8 iRarity )
{
	const CEconItemRarityDefinition* pItemRarity = GetRarityDefinition( iRarity );
	if ( !pItemRarity )
		return NULL;
	else
		return GetHexColorForAttribColor( pItemRarity->GetAttribColor() );
}

const char* CEconItemSchema::GetRarityLootList( uint8 iRarity )
{
	const CEconItemRarityDefinition* pItemRarity = GetRarityDefinition( iRarity );
	if ( !pItemRarity )
		return NULL;
	else
		return pItemRarity->GetLootList();
}

int CEconItemSchema::GetRarityIndex( const char* pszRarity )
{
	const CEconItemRarityDefinition* pRarity = GetRarityDefinitionByName( pszRarity );
	if ( pRarity )
		return pRarity->GetDBValue();
	else
		return 0;
}

const CEconSoundMaterialDefinition *CEconItemSchema::GetSoundMaterialDefinitionByID( int nSoundMaterialID ) const
{ 
	int iIndex = m_mapSoundMaterials.Find( nSoundMaterialID );
	if ( m_mapSoundMaterials.IsValidIndex( iIndex ) )
		return &m_mapSoundMaterials[iIndex]; 
	return NULL;
}

const CEconSoundMaterialDefinition *CEconItemSchema::GetSoundMaterialDefinitionByName( const char *pszSoundMaterialName ) const
{
	FOR_EACH_MAP_FAST( m_mapSoundMaterials, i )
	{
		if ( !strcmp( pszSoundMaterialName, m_mapSoundMaterials[i].GetName()) )
			return &m_mapSoundMaterials[i]; 
	}
	return NULL;
}

const char* CEconItemSchema::GetSoundMaterialNameByID( int nSoundMaterialID )
{
	const CEconSoundMaterialDefinition* pSoundMaterial = GetSoundMaterialDefinitionByID( nSoundMaterialID );
	if ( !pSoundMaterial )
		return NULL;
	else
		return pSoundMaterial->GetName();
}

int CEconItemSchema::GetSoundMaterialID( const char* pszSoundMaterial )
{
	const CEconSoundMaterialDefinition* pSoundMaterial = GetSoundMaterialDefinitionByName( pszSoundMaterial );
	if ( pSoundMaterial )
		return pSoundMaterial->GetID();
	else
		return 0;
}

int CEconItemSchema::GetSoundMaterialIDByIndex( int nIndex )
{
	// This makes for a slow iteration pattern. Only used by the item editor currently, should make it a UtlVector/UtlMap pair if it's too slow.
	FOR_EACH_MAP( m_mapSoundMaterials, i )
	{
		nIndex--;
		if ( nIndex < 0 )
		{
			return m_mapSoundMaterials[i].GetID();
		}
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose:	Gets an item definition for the specified map index
//-----------------------------------------------------------------------------
const CEconItemDefinition *CEconItemSchema::GetItemDefinitionByMapIndex( int iMapIndex ) const
{
	if ( m_mapItems.IsValidIndex( iMapIndex ) )
		return m_mapItems[iMapIndex];

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	Gets an item definition for the specified definition index
// Input:	iItemIndex - The index of the desired definition.
// Output:	A pointer to the desired definition, or NULL if it is not found.
//-----------------------------------------------------------------------------
CEconItemDefinition *CEconItemSchema::GetItemDefinitionMutable( int iItemIndex, bool bNoDefault )
{
#if defined(CLIENT_DLL) || defined(GAME_DLL)
#if !defined(CSTRIKE_DLL)
	AssertMsg( m_pDefaultItemDefinition, "No default item definition set up for item schema." );
#endif // CSTRIKE_DLL
#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

	int iIndex = m_mapItems.Find( iItemIndex );
	if ( m_mapItems.IsValidIndex( iIndex ) )
		return m_mapItems[iIndex]; 

	if ( bNoDefault )
		return NULL;

	if ( m_pDefaultItemDefinition )
		return m_pDefaultItemDefinition;

#if !defined(CSTRIKE_DLL)
	// We shouldn't ever get down here, but all the same returning a valid pointer is very slightly
	// a better plan than returning an invalid pointer to code that won't check to see if it's valid.
	static CEconItemDefinition *s_pEmptyDefinition = CreateEconItemDefinition();
	return s_pEmptyDefinition;
#else
	return NULL;
#endif // CSTRIKE_DLL
}
const CEconItemDefinition *CEconItemSchema::GetItemDefinition( int iItemIndex, bool bNoDefault ) const
{
	return const_cast<CEconItemSchema *>(this)->GetItemDefinitionMutable( iItemIndex, bNoDefault );
}

//-----------------------------------------------------------------------------
// Purpose:	Gets an item definition that has a name matching the specified name.
// Input:	pszDefName - The name of the desired definition.
// Output:	A pointer to the desired definition, or NULL if it is not found.
//-----------------------------------------------------------------------------
CEconItemDefinition *CEconItemSchema::GetItemDefinitionByName( const char *pszDefName )
{
    SNPROF(__PRETTY_FUNCTION__);

	if ( m_bSchemaParsingItems )
	{
		AssertMsg( 0, "GetItemDefinitionByName while parsing item definitions. This is not a valid operation." );
		return NULL;
	}

	// This shouldn't happen, but let's not crash if it ever does.
	Assert( pszDefName != NULL );
	if ( pszDefName == NULL )
		return NULL;

	FOR_EACH_MAP_FAST( m_mapItems, i )
	{
		if ( !strcmp( pszDefName, m_mapItems[i]->GetDefinitionName()) )
			return m_mapItems[i]; 
	}
	return NULL;
}

const CEconItemDefinition *CEconItemSchema::GetItemDefinitionByName( const char *pszDefName ) const
{
	return const_cast<CEconItemSchema *>(this)->GetItemDefinitionByName( pszDefName );
}

//-----------------------------------------------------------------------------
// Purpose:	Gets an attribute definition for an index
// Input:	iAttribIndex - The index of the desired definition.
// Output:	A pointer to the desired definition, or NULL if it is not found.
//-----------------------------------------------------------------------------
const CEconItemAttributeDefinition *CEconItemSchema::GetAttributeDefinition( int iAttribIndex ) const
{
	if ( m_mapAttributesContainer.IsValidIndex( iAttribIndex ) )
		return m_mapAttributesContainer[iAttribIndex];
	return NULL;
}

const CEconItemAttributeDefinition *CEconItemSchema::GetAttributeDefinitionByName( const char *pszDefName ) const
{
	VPROF_BUDGET( "CEconItemSchema::GetAttributeDefinitionByName", VPROF_BUDGETGROUP_STEAM );
    SNPROF(__PRETTY_FUNCTION__);

	FOR_EACH_VEC( m_mapAttributesContainer, i )
	{
		if ( m_mapAttributesContainer[i] && !strcmp( pszDefName, m_mapAttributesContainer[i]->GetDefinitionName()) )
			return m_mapAttributesContainer[i]; 
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	Gets a recipe definition for an index
// Input:	iRecipeIndex - The index of the desired definition.
// Output:	A pointer to the desired definition, or NULL if it is not found.
//-----------------------------------------------------------------------------
const CEconCraftingRecipeDefinition *CEconItemSchema::GetRecipeDefinition( int iRecipeIndex ) const
{ 
	int iIndex = m_mapRecipes.Find( iRecipeIndex );
	if ( m_mapRecipes.IsValidIndex( iIndex ) )
		return m_mapRecipes[iIndex]; 
	return NULL;
}

//-----------------------------------------------------------------------------

void CEconItemSchema::AddStickerKitDefinition( int iStickerKitID, CStickerKit *pStickerKit )
{
	int iIndex = m_mapStickerKits.Find( iStickerKitID );
	if ( m_mapStickerKits.IsValidIndex( iIndex ) )
	{
		m_mapStickerKits.Remove( iStickerKitID );
	}

	m_mapStickerKits.Insert( iStickerKitID, pStickerKit );
}

void CEconItemSchema::RemoveStickerKitDefinition( int iStickerKitID )
{
	m_mapStickerKits.Remove( iStickerKitID );
}

const CStickerKit *CEconItemSchema::GetStickerKitDefinition( int iStickerKitID ) const
{
	int iIndex = m_mapStickerKits.Find( iStickerKitID );
	if ( m_mapStickerKits.IsValidIndex( iIndex ) )
		return m_mapStickerKits[iIndex];
	return NULL;
}

const CStickerKit *CEconItemSchema::GetStickerKitDefinitionByMapIndex( int iMapIndex )
{
	if ( m_mapStickerKits.IsValidIndex( iMapIndex ) )
		return m_mapStickerKits[ iMapIndex ];

	return NULL;
}

const CStickerKit *CEconItemSchema::GetStickerKitDefinitionByName( const char *pchName ) const
{
	int idx = m_dictStickerKits.Find( pchName );
	if ( idx != m_dictStickerKits.InvalidIndex() )
		return m_dictStickerKits.Element( idx );
	else
		return NULL;
}

const CStickerList *CEconItemSchema::GetStickerListDefinitionByName( const char *pchName ) const
{
	int idx = m_dictStickerLists.Find( pchName );
	if ( idx != m_dictStickerLists.InvalidIndex() )
		return m_dictStickerLists.Element( idx );
	else
		return NULL;
}

const CEconMusicDefinition *CEconItemSchema::GetMusicKitDefinitionByName( const char *pchName ) const
{
	FOR_EACH_MAP_FAST( m_mapMusicDefs, i )
	{
		if ( !V_strcmp( m_mapMusicDefs.Element( i )->GetName(), pchName ) )
		{
			return m_mapMusicDefs.Element( i );
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------

void CEconItemSchema::AddPaintKitDefinition( int iPaintKitID, CPaintKit *pPaintKit )
{
	int iIndex = m_mapPaintKits.Find( iPaintKitID );
	if ( m_mapPaintKits.IsValidIndex( iIndex ) )
	{
		m_mapPaintKits.Remove( iPaintKitID );
	}

	m_mapPaintKits.Insert( iPaintKitID, pPaintKit );
}

void CEconItemSchema::RemovePaintKitDefinition( int iPaintKitID )
{
	m_mapPaintKits.Remove( iPaintKitID );
}

const CPaintKit *CEconItemSchema::GetPaintKitDefinition( int iPaintKitID ) const
{
	int iIndex = m_mapPaintKits.Find( iPaintKitID );
	if ( m_mapPaintKits.IsValidIndex( iIndex ) )
		return m_mapPaintKits[iIndex];
	return NULL;
}

const CPaintKit *CEconItemSchema::GetPaintKitDefinitionByMapIndex( int iMapIndex )
{
	if ( m_mapPaintKits.IsValidIndex( iMapIndex ) )
		return m_mapPaintKits[ iMapIndex ];

	return NULL;
}

const CPaintKit *CEconItemSchema::GetPaintKitDefinitionByName( const char *pchName ) const
{
	FOR_EACH_MAP_FAST( m_mapPaintKits, i )
	{
		CPaintKit *pPaintKit = m_mapPaintKits[ i ];
		if ( V_strcmp( pPaintKit->sName.Access(), pchName ) == 0 )
		{
			return pPaintKit;
		}
	}

	return NULL;
}

const unsigned int CEconItemSchema::GetPaintKitCount() const
{
	return m_mapPaintKits.Count();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const CEconColorDefinition *CEconItemSchema::GetColorDefinitionByName( const char *pszDefName ) const
{
	FOR_EACH_VEC( m_vecColorDefs, i )
	{
		if ( !Q_stricmp( m_vecColorDefs[i]->GetName(), pszDefName ) )
			return m_vecColorDefs[i];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const CEconGraffitiTintDefinition * CEconItemSchema::GetGraffitiTintDefinitionByID( int nID ) const
{
	return m_vecGraffitiTintDefs.IsValidIndex( nID ) ? m_vecGraffitiTintDefs[ nID ] : NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const CEconGraffitiTintDefinition * CEconItemSchema::GetGraffitiTintDefinitionByName( const char *pszDefName ) const
{
	UtlSymId_t utlsym = m_mapGraffitiTintByName.Find( pszDefName );
	return ( utlsym != UTL_INVAL_SYMBOL ) ? m_mapGraffitiTintByName[ utlsym ] : NULL;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const CEconMusicDefinition *CEconItemSchema::GetMusicDefinition( uint32 unMusicID ) const
{
	int iIndex = m_mapMusicDefs.Find( unMusicID );
	if ( m_mapMusicDefs.IsValidIndex( iIndex ) )
		return m_mapMusicDefs[iIndex];
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEconQuestDefinition *CEconItemSchema::GetQuestDefinition( uint32 unQuestID ) const
{
	int iIndex = m_mapQuestDefs.Find( unQuestID );
	if ( m_mapQuestDefs.IsValidIndex( iIndex ) )
		return m_mapQuestDefs[ iIndex ];
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CEconCampaignDefinition *CEconItemSchema::GetCampaignDefinition( uint32 unCampaignID ) const
{
	int iIndex = m_mapCampaignDefs.Find( unCampaignID );
	if ( m_mapCampaignDefs.IsValidIndex( iIndex ) )
		return m_mapCampaignDefs[ iIndex ];
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:	Return the attribute specified attachedparticlesystem_t* associated with the given id.
//-----------------------------------------------------------------------------
attachedparticlesystem_t* CEconItemSchema::GetAttributeControlledParticleSystem( int id )
{
	int iIndex = m_mapAttributeControlledParticleSystems.Find( id );
	if ( m_mapAttributeControlledParticleSystems.IsValidIndex( iIndex ) )
		return &m_mapAttributeControlledParticleSystems[iIndex];
	return NULL;
}

attachedparticlesystem_t* CEconItemSchema::GetAttributeControlledParticleSystemByIndex( int id )
{
	return &m_mapAttributeControlledParticleSystems[id];
}

attachedparticlesystem_t* CEconItemSchema::FindAttributeControlledParticleSystem( const char *pchSystemName, int *outID )
{
	FOR_EACH_MAP_FAST( m_mapAttributeControlledParticleSystems, nSystem )
	{
		if( !Q_stricmp( m_mapAttributeControlledParticleSystems[nSystem].pszSystemName, pchSystemName ) )
		{
			if ( outID )
			{
				*outID = nSystem;
			}
			return &m_mapAttributeControlledParticleSystems[nSystem];
		}
	}
	if ( outID )
	{
		*outID = -1;
	}
	return NULL;
}

bool CEconItemSchema::BPostSchemaInitStartupChecks( CUtlVector<CUtlString> *pVecErrors )
{
#if defined( GC_DLL ) || defined( CLIENT_DLL )
	// confirm that all stickers that reference players have valid link
	FOR_EACH_MAP_FAST( m_mapStickerKits, i )
	{
		CStickerKit * pStickerKit = m_mapStickerKits.Element( i );
		if ( !pStickerKit->m_nPlayerID ) continue;
		SCHEMA_INIT_CHECK(
			( m_mapProPlayersByAccountID.Find( pStickerKit->m_nPlayerID ) != m_mapProPlayersByAccountID.InvalidIndex() ),
			CFmtStr( "Pro-player sticker %d reference player %u which doesn't have a matching pro-player entry", pStickerKit->nID, pStickerKit->m_nPlayerID ) );
	}

#endif // #if defined( GC_DLL ) || defined( CLIENT_DLL )

#ifdef CLIENT_DLL
	//// Confirm that every story block expression references only existing quest indices.
	FOR_EACH_MAP_FAST( m_mapCampaignDefs, iC )
	{
		FOR_EACH_MAP_FAST( m_mapCampaignDefs.Element( iC )->GetCampaignNodes(), iCn )
		{
			CEconCampaignDefinition::CEconCampaignNodeDefinition * pCNodeDef = m_mapCampaignDefs.Element( iC )->GetCampaignNodes().Element( iCn );

			FOR_EACH_VEC( pCNodeDef->GetStoryBlocks(), iSB )
			{

				KeyValues * pKVExpressionTokens = new KeyValues( "ExpressionTokens" );
				KeyValues::AutoDelete autodelete( pKVExpressionTokens );

				CEconQuestDefinition::TokenizeQuestExpression( pCNodeDef->GetStoryBlocks()[ iSB ]->GetStoryBlockExpression(), pKVExpressionTokens );

				FOR_EACH_SUBKEY( pKVExpressionTokens, kvSubKey )
				{
					int iQ = m_mapQuestDefs.Find( V_atoi( kvSubKey->GetName() ) );

					SCHEMA_INIT_CHECK(
						( m_mapQuestDefs.IsValidIndex( iQ ) ),
						CFmtStr( "campaign %d, node %d, Story_block %d, references a non-existant quest index %d.", iC, iCn, iSB, iQ ) );

				}
			}
		}
	}

	// confirm that all quest expressions are valid

	FOR_EACH_MAP_FAST( m_mapQuestDefs, i )
	{
		CEconQuestDefinition * pQuestDef = m_mapQuestDefs.Element( i );

		SCHEMA_INIT_CHECK(
			( CEconQuestDefinition::IsQuestExpressionValid( pQuestDef->GetQuestExpression() ) ),
			CFmtStr( "Quest definition %s specifies an expression that does not evaluate.", pQuestDef->GetName() ) );

		if ( pQuestDef->GetQuestBonusExpression() && pQuestDef->GetQuestBonusExpression()[ 0 ] )
		{
			SCHEMA_INIT_CHECK(
				( CEconQuestDefinition::IsQuestExpressionValid( pQuestDef->GetQuestBonusExpression() ) ),
				CFmtStr( "Quest definition %s specifies a bonus expression that does not evaluate.", pQuestDef->GetName() ) );
		}
	}

#endif // CLIENT_DLL

	return true;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CItemLevelingDefinition::CItemLevelingDefinition( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:	Copy constructor
//-----------------------------------------------------------------------------
CItemLevelingDefinition::CItemLevelingDefinition( const CItemLevelingDefinition &that )
{
	(*this) = that;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CItemLevelingDefinition::~CItemLevelingDefinition()
{
	// Free up strdup() memory.
	free( m_pszLocalizedName_LocalStorage );
	CUtlVector< CUtlString > vecErrors;
	bool bSuccess = GEconItemSchema().BInit( "scripts/items/unencrypted/items_master.txt", "MOD", &vecErrors );
	if( !bSuccess )
	{
		FOR_EACH_VEC( vecErrors, nError )
		{
			Msg( "%s", vecErrors[nError].String() );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:	Operator=
//-----------------------------------------------------------------------------
CItemLevelingDefinition &CItemLevelingDefinition::operator=( const CItemLevelingDefinition &other )
{
	m_unLevel = other.m_unLevel;
	m_unRequiredScore = other.m_unRequiredScore;
	m_pszLocalizedName_LocalStorage = strdup( other.m_pszLocalizedName_LocalStorage );

	return *this;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CItemLevelingDefinition::BInitFromKV( KeyValues *pKVItemLevel, CEconItemSchema &pschema, const char *pszLevelBlockName, CUtlVector<CUtlString> *pVecErrors )
{
	m_unLevel = Q_atoi( pKVItemLevel->GetName() );
	m_unRequiredScore = pKVItemLevel->GetInt( "score" );
	m_pszLocalizedName_LocalStorage = strdup( pKVItemLevel->GetString( "rank_name", CFmtStr( "%s%i", pszLevelBlockName, m_unLevel ).Access() ) );

	return SCHEMA_INIT_SUCCESS();
}

#ifdef GAME_DLL

void ReloadMasterItemSchema( void )
{
	// This command does nothing on the public universe.
	if ( steamapicontext->SteamUtils() && steamapicontext->SteamUtils()->GetConnectedUniverse() == k_EUniversePublic )
		return;

	CUtlVector< CUtlString > vecErrors;
	bool bSuccess = GEconItemSchema().BInit( "scripts/items/unencrypted/items_master.txt", "MOD", &vecErrors );
	if( !bSuccess )
	{
		FOR_EACH_VEC( vecErrors, nError )
		{
			Msg( "%s", vecErrors[nError].String() );
		}
	}
}

CON_COMMAND_F( load_master_item_schema, "Reloads the item master schema.", FCVAR_GAMEDLL | FCVAR_CHEAT | FCVAR_DEVELOPMENTONLY | FCVAR_HIDDEN )
{
	ReloadMasterItemSchema();
}

#endif
