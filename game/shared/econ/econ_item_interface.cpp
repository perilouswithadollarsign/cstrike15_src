
#include "cbase.h"
#include "econ_item_interface.h"

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
RTime32 IEconItemInterface::GetExpirationDate() const
{
	COMPILE_TIME_ASSERT( sizeof( float ) == sizeof( RTime32 ) );

	// dynamic attributes, if present, will override any static expiration timer
	static CSchemaAttributeDefHandle pAttrib_ExpirationDate( "expiration date" );

	attrib_value_t unAttribExpirationTimeBits;
	COMPILE_TIME_ASSERT( sizeof( unAttribExpirationTimeBits ) == sizeof( RTime32 ) );

	if ( pAttrib_ExpirationDate && FindAttribute( pAttrib_ExpirationDate, &unAttribExpirationTimeBits ) )
		return *(RTime32 *)&unAttribExpirationTimeBits;

	// do we have a static timer set in the schema for all instances to expire?
	return GetItemDefinition()
		? GetItemDefinition()->GetExpirationDate()
		: RTime32( 0 );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
RTime32 IEconItemInterface::GetTradableAfterDateTime() const
{
	static CSchemaAttributeDefHandle pAttrib( "tradable after date" );
	Assert( pAttrib );

	if ( !pAttrib )
		return 0;

	RTime32 rtTimestamp;
	if ( !FindAttribute( pAttrib, &rtTimestamp ) )
		return 0;

	return rtTimestamp;
}

RTime32 IEconItemInterface::GetUseAfterDateTime() const
{
	static CSchemaAttributeDefHandle pAttrib( "use after date" );
	Assert( pAttrib );

	if ( !pAttrib )
		return 0;

	RTime32 rtTimestamp;
	if ( !FindAttribute( pAttrib, &rtTimestamp ) )
		return 0;

	return rtTimestamp;
}

RTime32 IEconItemInterface::GetCacheRefreshDateTime() const
{
	RTime32 rtExpiration = 0;
	/** Removed for partner depot **/
	return rtExpiration;
}

int IEconItemInterface::GetCustomPaintKitIndex( void ) const
{
	static CSchemaAttributeDefHandle pAttrDef_PaintKit( "set item texture prefab" );
	float flPaintKit = 0;
	FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, pAttrDef_PaintKit, &flPaintKit );

	return flPaintKit;
}

int IEconItemInterface::GetCustomPaintKitSeed( void ) const
{
	static CSchemaAttributeDefHandle pAttrDef_PaintKitSeed( "set item texture seed" );
	float flPaintSeed = 0;
	FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, pAttrDef_PaintKitSeed, &flPaintSeed );

	return flPaintSeed;
}

float IEconItemInterface::GetCustomPaintKitWear( float flWearDefault /*= 0.0f*/ ) const
{
	static CSchemaAttributeDefHandle pAttrDef_PaintKitWear( "set item texture wear" );
	float flPaintKitWear = flWearDefault;
	FindAttribute_UnsafeBitwiseCast<attrib_value_t>( this, pAttrDef_PaintKitWear, &flPaintKitWear );

	return flPaintKitWear;
}

float IEconItemInterface::GetStickerAttributeBySlotIndexFloat( int nSlotIndex, EStickerAttributeType type, float flDefault ) const
{
	const CSchemaAttributeDefHandle &attrDef = GetStickerAttributeDefHandle( nSlotIndex, type );
	if ( attrDef )
	{
		if ( attrDef->IsStoredAsFloat() )
		{
			float flOutput = 0.0f;
			if ( FindAttribute_UnsafeBitwiseCast< attrib_value_t >( this, attrDef, &flOutput ) )
			{
				return flOutput;
			}
		}
		else
		{
			Assert( false );
		}
	}

	return flDefault;	
}

uint32 IEconItemInterface::GetStickerAttributeBySlotIndexInt( int nSlotIndex, EStickerAttributeType type, uint32 uiDefault ) const
{
	const CSchemaAttributeDefHandle &attrDef = GetStickerAttributeDefHandle( nSlotIndex, type );
	if ( attrDef )
	{
		if ( attrDef->IsStoredAsFloat() )
		{
			Assert( false );
		}
		else
		{
			uint32 unOutput;
			if ( FindAttribute( attrDef, &unOutput ) )
			{
				return unOutput;
			}
		}
	}

	return uiDefault;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool IEconItemInterface::IsTradable() const
{
	/** Removed for partner depot **/
	return false;
}

bool IEconItemInterface::IsPotentiallyTradable() const
{
	if ( GetItemDefinition() == NULL )
		return false;

	// check attributes
	
	static CSchemaAttributeDefHandle pAttrDef_AlwaysTradableAndUsableInCrafting( "always tradable" );
	static CSchemaAttributeDefHandle pAttrib_CannotTrade( "cannot trade" );

	
	Assert( pAttrDef_AlwaysTradableAndUsableInCrafting != NULL );
	Assert( pAttrib_CannotTrade != NULL );

	if ( pAttrDef_AlwaysTradableAndUsableInCrafting == NULL || pAttrib_CannotTrade == NULL )
		return false;

// NOTE: we are not checking the time delay on trade restriction here - the item is considered potentially tradable in future = true
// 	if ( GetTradableAfterDateTime() > CRTime::RTime32TimeCur() )
// 		return false;

	// Explicit rules by item quality to validate trading eligibility
	switch ( GetQuality() )
	{
	case AE_UNIQUE:
		break;
	case AE_STRANGE:
		return ( GetOrigin() == kEconItemOrigin_FoundInCrate || GetOrigin() == kEconItemOrigin_Crafted || GetOrigin() == kEconItemOrigin_Purchased );
	case AE_UNUSUAL:
	case AE_TOURNAMENT:
		return ( GetOrigin() == kEconItemOrigin_FoundInCrate );
	default:
		// all other qualities are untradable
		return false;
	}

	if ( FindAttribute( pAttrDef_AlwaysTradableAndUsableInCrafting ) )
		return true;

	if ( FindAttribute( pAttrib_CannotTrade ) )
		return false;

	// items gained in this way are not tradable
	switch ( GetOrigin() )
	{
	case kEconItemOrigin_Invalid:
	case kEconItemOrigin_Achievement:
	case kEconItemOrigin_Foreign:
	case kEconItemOrigin_PreviewItem:
	case kEconItemOrigin_SteamWorkshopContribution:
	case kEconItemOrigin_StockItem:
		return false;
	}

	// certain quality levels are not tradable
	if ( GetQuality() >= AE_COMMUNITY && GetQuality() <= AE_SELFMADE )
		return false;

	// explicitly marked cannot trade?
	if ( ( kEconItemFlags_CheckFlags_CannotTrade & GetFlags() ) != 0 )
		return false;

	// tagged to not be a part of the economy?
	if ( ( kEconItemFlag_NonEconomy & GetFlags() ) != 0 )
		return false;

	// This code catches stock items with name tags (rarity is stock) and prevents them from trading/marketing
	// until we have a better solution for extracting value out of reselling these items and avoiding scams
	if ( GetRarity() <= 0 )
		return false;

	return true;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool IEconItemInterface::IsMarketable() const
{
	/** Removed for partner depot **/
	return false;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool IEconItemInterface::IsCommodity() const
{
	const CEconItemDefinition *pItemDef = GetItemDefinition();
	if ( pItemDef == NULL )
		return false;

	static CSchemaAttributeDefHandle pAttrib_IsCommodity( "is commodity" );
	Assert( pAttrib_IsCommodity != NULL );
	if ( pAttrib_IsCommodity == NULL )
		return false;

	attrib_value_t unAttribValue;
	if ( FindAttribute( pAttrib_IsCommodity, &unAttribValue ) && unAttribValue )
		return true;

	return false;
}

bool IEconItemInterface::IsHiddenFromDropList() const
{
	const CEconItemDefinition *pItemDef = GetItemDefinition();
	if ( pItemDef == NULL )
		return false;

	static CSchemaAttributeDefHandle pAttrib_HideFromDropList( "hide from drop list" );
	Assert( pAttrib_HideFromDropList != NULL );
	if ( pAttrib_HideFromDropList == NULL )
		return false;

	attrib_value_t unAttribValue;
	if ( FindAttribute( pAttrib_HideFromDropList, &unAttribValue ) && unAttribValue )
		return true;

	return false;
}


// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool IEconItemInterface::IsUsableInCrafting() const
{
	if ( GetItemDefinition() == NULL )
		return false;

	// check attribute
	static CSchemaAttributeDefHandle pAttrDef_AlwaysTradableAndUsableInCrafting( "always tradable" );
	Assert( pAttrDef_AlwaysTradableAndUsableInCrafting );

	if ( FindAttribute( pAttrDef_AlwaysTradableAndUsableInCrafting ) )
		return true;

	// explicitly marked not usable in crafting?
	if ( ( kEconItemFlags_CheckFlags_NotUsableInCrafting & GetFlags() ) != 0 )
		return false;

	// items gained in this way are not craftable
	switch ( GetOrigin() )
	{
	case kEconItemOrigin_Invalid:
	case kEconItemOrigin_Foreign:
	case kEconItemOrigin_PreviewItem:
	case kEconItemOrigin_Purchased:
	case kEconItemOrigin_StorePromotion:
	case kEconItemOrigin_SteamWorkshopContribution:
		return false;
	}

	// certain quality levels are not craftable
	if ( GetQuality() >= AE_COMMUNITY && GetQuality() <= AE_SELFMADE )
		return false;

	// tagged to not be a part of the economy?
	if ( ( kEconItemFlag_NonEconomy & GetFlags() ) != 0 )
		return false;

	return true;
}
