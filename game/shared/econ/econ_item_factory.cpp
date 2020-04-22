//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: EconItemFactory: Manages rolling for items requested by the game server
//
//=============================================================================

#include "cbase.h"

#include "econ_item_view_helpers.h"
#include "playerdecals_signature.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace GCSDK;

//-----------------------------------------------------------------------------
// Purpose:	Constructor
//-----------------------------------------------------------------------------
CEconItemFactory::CEconItemFactory(  )
	: m_ulNextObjID( 0 )
	, m_bIsInitialized( false )
{
}

//-----------------------------------------------------------------------------
itemid_t CEconItemFactory::GetNextID()
{ 
	if( !m_bIsInitialized || !GGCBase()->IsGCRunningType( GC_TYPE_MASTER ) )
	{
		AssertMsg( m_bIsInitialized, "Critical Error: Attempting to get a new item ID without loading in the starting ID first!!!" ); 
		AssertMsg( GGCBase()->IsGCRunningType( GC_TYPE_MASTER ), "Critical Error: Attempting to get an item ID on the non-master GC, this will get out of sync" );
		return INVALID_ITEM_ID;
	}
	return m_ulNextObjID++; 
}

//-----------------------------------------------------------------------------
// Purpose:	Initializes the item factory and schema. Return false if init failed
//-----------------------------------------------------------------------------
bool CEconItemFactory::BYieldingInit()
{
	CUtlVector< CUtlString > vecErrors;
	bool bRet = m_schema.BInit( "scripts/items/unencrypted/items_master.txt", "GAME", &vecErrors );

	FOR_EACH_VEC( vecErrors, nError )
	{
		EmitError( SPEW_GC, "%s\n", vecErrors[nError].Get() );
	}

	static const char *pchMaxIDQuery = "SELECT MAX( ID ) FROM "
		"( select Max(ID) AS ID FROM Item UNION SELECT MAX(ID) AS ID FROM ForeignItem ) as tbl";

	CSQLAccess sqlAccess;
	if( !sqlAccess.BYieldingExecuteSingleResult<uint64, uint64>( NULL, pchMaxIDQuery, k_EGCSQLType_int64, &m_ulNextObjID, NULL ) )
	{
		EmitError( SPEW_GC, "Failed to read max item ID" );
		return false;
	}
	m_ulNextObjID++; // our next ID is one past the current max ID

	m_bIsInitialized = bRet;
	return bRet;
}

static const CEconItemQualityDefinition *GetQualityDefinitionForItemCreation( const CItemSelectionCriteria *pOptionalCriteria, const CEconItemDefinition *pItemDef )
{
	Assert( pItemDef );

	// Do we have a quality specified? If so, is it a valid quality? If not, we fall back to the
	// quality specified by the item definition, the schema, etc.
	uint8 unQuality = k_unItemQuality_Any;

	// Quality specified in generation request via criteria?
	if ( pOptionalCriteria && pOptionalCriteria->BQualitySet() )
	{
		unQuality = pOptionalCriteria->GetQuality();
	}

	// If not: quality specified in item definition?
	if ( unQuality == k_unItemQuality_Any )
	{
		unQuality = pItemDef->GetQuality();
	}

	// Final fallback: default quality in schema.
	if ( unQuality == k_unItemQuality_Any )
	{
		unQuality = GetItemSchema()->GetDefaultQuality();
	}

	AssertMsg( unQuality != k_unItemQuality_Any, "Unable to locate valid quality!" );

	return GetItemSchema()->GetQualityDefinition( unQuality );
}

static const CEconItemRarityDefinition *GetRarityDefinitionForItemCreation( const CItemSelectionCriteria *pOptionalCriteria, const CEconItemDefinition *pItemDef )
{
	Assert( pItemDef );

	// Do we have a quality specified? If so, is it a valid quality? If not, we fall back to the
	// quality specified by the item definition, the schema, etc.
	uint8 unRarity = k_unItemRarity_Any;

	// Quality specified in generation request via criteria?
	if ( pOptionalCriteria && pOptionalCriteria->BRaritySet() )
	{
		unRarity = pOptionalCriteria->GetRarity();
	}

	// If not: quality specified in item definition?
	if ( unRarity == k_unItemRarity_Any )
	{
		unRarity = pItemDef->GetRarity();
	}

	// Final fallback: default quality in schema.
	if ( unRarity == k_unItemRarity_Any )
	{
		unRarity = 1;
	}

	AssertMsg( unRarity != k_unItemRarity_Any, "Unable to locate valid rarity!" );

	return GetItemSchema()->GetRarityDefinition( unRarity );
}

//-----------------------------------------------------------------------------
// Purpose:	Creates an item matching the incoming item selection criteria
// Input:	pItem - Pointer to the item to fill in
//			criteria - The criteria that the generated item must match
// Output:	True if a matching item could be generated, false otherwise
//-----------------------------------------------------------------------------
CEconItem *CEconItemFactory::CreateRandomItem( const CEconGameAccount *pGameAccount, const CItemSelectionCriteria &criteria )
{
	// Find a matching item definition.
	const CEconItemDefinition *pItemDef = RollItemDefinition( criteria );
	if ( NULL == pItemDef )
	{
		EmitWarning( SPEW_GC, 2, "CEconItemFactory::CreateRandomItem(): Item creation request with no matching definition\n" );
		return NULL;
	}

	const CEconItemQualityDefinition *pQualityDef = GetQualityDefinitionForItemCreation( &criteria, pItemDef );
	if ( NULL == pQualityDef )
	{
		EmitWarning( SPEW_GC, 2, "CEconItemFactory::CreateRandomItem(): Item creation request with unknown quality\n" );
		return NULL;
	}
	const CEconItemRarityDefinition *pRarityDef = GetRarityDefinitionForItemCreation( &criteria, pItemDef );
	if ( NULL == pRarityDef )
	{
		EmitWarning( SPEW_GC, 2, "CEconItemFactory::CreateRandomItem(): Item creation request with unknown rarity\n" );
		return NULL;
	}

	// At this point we have everything that can fail will already have failed, so we can safely
	// create an item and just move properties over to it.
	CEconItem *pItem = new CEconItem();
	pItem->SetItemID( GetNextID() );
	pItem->SetDefinitionIndex( pItemDef->GetDefinitionIndex() );
	pItem->SetItemLevel( criteria.BItemLevelSet() ? criteria.GetItemLevel() : pItemDef->RollItemLevel() );
	pItem->SetQuality( pQualityDef->GetDBValue() );
	pItem->SetRarity( pRarityDef->GetDBValue() );
	pItem->SetInventoryToken( criteria.GetInitialInventory() );
	pItem->SetQuantity( criteria.GetInitialQuantity() );
	// don't set account ID
	
	// Add any custom attributes we need
	AddGCGeneratedAttributesToItem( pGameAccount, pItem );

	// Set any painkit data specified
	const char *pchPaintKit = criteria.GetValueForFirstConditionOfFieldName( "*paintkit" );
	if ( pchPaintKit )
	{
		const char *pchWear = criteria.GetValueForFirstConditionOfFieldName( "*wear" );

		item_list_entry_t itemInit;
		itemInit.m_nItemDef = pItemDef->GetDefinitionIndex();
		itemInit.m_nPaintKit = atoi( pchPaintKit );
		itemInit.m_nPaintKitSeed = 0;
		itemInit.m_flPaintKitWear = pchWear ? atof( pchWear ) : 0.001;
		pItem->InitAttributesDroppedFromListEntry( &itemInit );
	}

	return pItem;
}

//-----------------------------------------------------------------------------
// Purpose:	Creates an item based on a specific item definition index
// Input:	pItem - Pointer to the item to fill in
//			unDefinitionIndex - The definition index of the item to create
// Output:	True if a matching item could be generated, false otherwise
//-----------------------------------------------------------------------------
CEconItem *CEconItemFactory::CreateSpecificItem( const CEconGameAccount *pGameAccount, item_definition_index_t unDefinitionIndex, ECreateItemPolicy_t eCreateItemPolicy )
{
	if ( !pGameAccount )
		return NULL;

	// Find the matching index
	const CEconItemDefinition *pItemDef = m_schema.GetItemDefinition( unDefinitionIndex );
	if ( NULL == pItemDef )
	{
		EmitWarning( SPEW_GC, 2, "CEconItemFactory::CreateSpecificItem(): Item creation request with no matching definition (def index %u)\n", unDefinitionIndex );
		return NULL;
	}

	const CEconItemQualityDefinition *pQualityDef = GetQualityDefinitionForItemCreation( NULL, pItemDef );
	if ( NULL == pQualityDef )
	{
		EmitWarning( SPEW_GC, 2, "CEconItemFactory::CreateSpecificItem(): Item creation request with unknown quality\n" );
		return NULL;
	}
	const CEconItemRarityDefinition *pRarityDef = GetRarityDefinitionForItemCreation( NULL, pItemDef );
	if ( NULL == pRarityDef )
	{
		EmitWarning( SPEW_GC, 2, "CEconItemFactory::CreateSpecificItem(): Item creation request with unknown rarity\n" );
		return NULL;
	}

	CEconItem *pItem = new CEconItem();
	if ( eCreateItemPolicy & k_ECreateItemPolicy_NoSqlItemID )
		pItem->SetItemID( 0ull );
	else
		pItem->SetItemID( GetNextID() );

	pItem->SetDefinitionIndex( unDefinitionIndex );
	pItem->SetItemLevel( pItemDef->RollItemLevel() );
	pItem->SetQuality( pQualityDef->GetDBValue() );
	pItem->SetRarity( pRarityDef->GetDBValue() );
	// don't set inventory token
	pItem->SetQuantity( MAX( 1, pItemDef->GetDefaultDropQuantity() ) );
	pItem->SetAccountID( pGameAccount->Obj().m_unAccountID );

	// Add any custom attributes we need
	AddGCGeneratedAttributesToItem( pGameAccount, pItem );

	return pItem;
}


//-----------------------------------------------------------------------------
// Purpose:	Randomly chooses an item definition that matches the criteria
// Input:	sCriteria - The criteria that the generated item must match
// Output:	The chosen item definition, or NULL if no item could be selected
//-----------------------------------------------------------------------------
const CEconItemDefinition *CEconItemFactory::RollItemDefinition( const CItemSelectionCriteria &criteria ) const
{
	// Determine which item templates match the criteria
	CUtlVector<int> vecMatches;
	const CEconItemSchema::ItemDefinitionMap_t &mapDefs = m_schema.GetItemDefinitionMap();

	FOR_EACH_MAP_FAST( mapDefs, i )
	{
		if ( criteria.BEvaluate( mapDefs[i], m_schema ) )
		{
			vecMatches.AddToTail( mapDefs.Key( i ) );
		}
	}

	if ( 0 == vecMatches.Count() )
		return NULL;

	// Choose a random match
	int iIndex = RandomInt( 0, vecMatches.Count() - 1 );
	return m_schema.GetItemDefinition( vecMatches[iIndex] );
}

//-----------------------------------------------------------------------------
// Purpose:	Generates attributes that the item definition insists it always has, but must be generated by the GC
// Input:	
//-----------------------------------------------------------------------------
void CEconItemFactory::AddGCGeneratedAttributesToItem( const CEconGameAccount *pGameAccount, CEconItem *pItem ) const
{
	const CEconItemDefinition *pDef = m_schema.GetItemDefinition( pItem->GetDefinitionIndex() );
	if ( !pDef )
		return;

	const CUtlVector<static_attrib_t> &vecStaticAttribs = pDef->GetStaticAttributes();

	// Only generate attributes that force the GC to generate them (so they vary per item created)
	FOR_EACH_VEC( vecStaticAttribs, i )
	{
		if ( vecStaticAttribs[i].m_bForceGCToGenerate )
		{
			ApplyStaticAttributeToItem( pItem, vecStaticAttribs[i], pGameAccount );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CEconItemFactory::ApplyStaticAttributeToItem( CEconItem *pItem, const static_attrib_t& staticAttrib, const CEconGameAccount *pGameAccount, const CCopyableUtlVector< uint32 > *vecValues, float flRangeMin, float flRangeMax ) const
{
	static CSchemaAttributeDefHandle pAttr_ElevateQuality( "elevate quality" );
	static CSchemaAttributeDefHandle pAttr_SetItemTextureWear( "set item texture wear" );
	static CSchemaAttributeDefHandle pAttr_SprayTintID( "spray tint id" );
	static CSchemaAttributeDefHandle pAttr_SpraysRemaining( "sprays remaining" );

	const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( staticAttrib.iDefIndex );
	Assert( pAttrDef );

	// Special-case the elevate-quality attribute.
	if ( pAttrDef->GetDefinitionIndex() == pAttr_ElevateQuality->GetDefinitionIndex() )
	{
		//AssertMsg( CEconItem::GetTypedAttributeType<CSchemaAttributeType_Default>( pAttrDef ), "Elevate quality attribute doesn't have the right type!" );

		pItem->SetQuality( staticAttrib.m_value.asFloat );
		return;
	}

	static CSchemaItemDefHandle hItemSpray( "spray" );
	static CSchemaItemDefHandle hItemSprayPaint( "spraypaint" );
	// Special-case the sprays remaining to mean unsealing the spray into spray paint
	if ( pAttr_SpraysRemaining && hItemSpray && hItemSprayPaint && pItem->GetDefinitionIndex() == hItemSpray->GetDefinitionIndex() &&
		pAttrDef->GetDefinitionIndex() == pAttr_SpraysRemaining->GetDefinitionIndex() )
	{
		pItem->SetDefinitionIndex( hItemSprayPaint->GetDefinitionIndex() );
		pItem->SetFlag( kEconItemFlag_NonEconomy );
		pItem->SetDynamicAttributeValue( pAttr_SpraysRemaining, uint32( PLAYERDECALS_NUMCHARGES ) );
		return;
	}

	attribute_data_union_t attrValue = staticAttrib.m_value;

	// pick from 'values' if they exist. Otherwise use min/max ranges specified.
	if ( vecValues && ( *vecValues ).Count() != 0 )
	{
		uint32 uiRandIndex = CEconItemSchema::GetRandomStream().RandomInt( 0, ( *vecValues ).Count() - 1 );

		if ( pAttrDef->IsStoredAsFloat() )
		{
			attrValue.asFloat = ( *vecValues ).Element( uiRandIndex );	
		}
		else
		{
			attrValue.asUint32 = ( *vecValues ).Element( uiRandIndex );	
		}
	}
	else if ( flRangeMin != 0.0f || flRangeMax != 0.0 )
	{
		if ( pAttrDef->IsStoredAsFloat() )
		{
			attrValue.asFloat = CEconItemSchema::GetRandomStream().RandomFloat( flRangeMin, flRangeMax );
		}
		else
		{
			attrValue.asUint32 = CEconItemSchema::GetRandomStream().RandomInt( flRangeMin, flRangeMax );
		}
	}

	if ( pAttr_SetItemTextureWear && staticAttrib.iDefIndex == pAttr_SetItemTextureWear->GetDefinitionIndex() )
	{
		const CPaintKit *pPaintKit = GetItemSchema()->GetPaintKitDefinition( pItem->GetCustomPaintKitIndex() );
		if ( pPaintKit )
		{
			attrValue.asFloat = RemapValClamped( attrValue.asFloat, 0.0f, 1.0f, pPaintKit->flWearRemapMin, pPaintKit->flWearRemapMax );
		}
	}

#if ECON_SPRAY_TINT_IDS_FLOAT_COLORSPACE
 	if ( pAttr_SprayTintID && staticAttrib.iDefIndex == pAttr_SprayTintID->GetDefinitionIndex()
 		&& pAttr_SprayTintID->IsStoredAsInteger()
 		&& ( attrValue.asUint32 & 0xFF ) )
 	{	// Generation of random HSV space tints
 		attrValue.asUint32 = CombinedTintIDMakeFromIDHSVu( attrValue.asUint32,
 			CEconItemSchema::GetRandomStream().RandomInt( 0x00, 0x7F ),
 			CEconItemSchema::GetRandomStream().RandomInt( 0x00, 0x7F ),
 			CEconItemSchema::GetRandomStream().RandomInt( 0x00, 0x7F ) );
 	}
#endif

	// Custom attribute initialization code?
	pAttrDef->GetAttributeType()->LoadOrGenerateEconAttributeValue( pItem, pAttrDef, staticAttrib.m_pszCustomLogic, attrValue, pGameAccount );
}
