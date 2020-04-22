//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: CItemSelectionCriteria, which serves as a criteria for selection
//			of a econ item
//
//=============================================================================


#include "cbase.h"
#include "item_selection_criteria.h"
#include "game_item_schema.h" // TODO: Fix circular dependency

#if defined(TF_CLIENT_DLL) || defined(TF_DLL)
#include "tf_gcmessages.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// copied from \common\econ_item_view.h
#define AE_USE_SCRIPT_VALUE			9999		// Can't be -1, due to unsigned ints used on the backend

using namespace GCSDK;

//-----------------------------------------------------------------------------
// Purpose: Copy Constructor
//-----------------------------------------------------------------------------
CItemSelectionCriteria::CItemSelectionCriteria( const CItemSelectionCriteria &that )
{
	(*this) = that;
}


struct EmptyStruct_t
{
};

//-----------------------------------------------------------------------------
// Purpose: Operator=
//-----------------------------------------------------------------------------
CItemSelectionCriteria &CItemSelectionCriteria::operator=( const CItemSelectionCriteria &rhs )
{

	// Leverage the serialization code we already have for the copy
	CSOItemCriteria msgTemp;
	rhs.BSerializeToMsg( msgTemp );
	BDeserializeFromMsg( msgTemp );

	return *this;
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CItemSelectionCriteria::~CItemSelectionCriteria( void )
{
	m_vecConditions.PurgeAndDeleteElements();
}

const char *CItemSelectionCriteria::GetValueForFirstConditionOfFieldName( const char *pchName ) const
{
	FOR_EACH_VEC( m_vecConditions, i )
	{
		if ( V_strcmp( m_vecConditions[i]->GetField(), pchName ) == 0 )
			return m_vecConditions[i]->GetValue();
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Initialize from a KV structure
//-----------------------------------------------------------------------------
bool CItemSelectionCriteria::BInitFromKV( KeyValues *pKVCriteria, const CEconItemSchema &pschema )
{
	// Read in the base fields
	if ( pKVCriteria->FindKey( "level" ) )
	{
		SetItemLevel( pKVCriteria->GetInt( "level" ) );
	}

	if ( pKVCriteria->FindKey( "quality" ) )
	{
		uint8 nQuality;
		if ( !pschema.BGetItemQualityFromName( pKVCriteria->GetString( "quality" ), &nQuality ) )
			return false;

		SetQuality( nQuality );
	}

	if ( pKVCriteria->FindKey( "rarity" ) )
	{
		uint8 nRarity;
		if ( !pschema.BGetItemRarityFromName( pKVCriteria->GetString( "rarity" ), &nRarity ) )
			return false;

		SetRarity( nRarity );
	}

	if ( pKVCriteria->FindKey( "inventoryPos" ) )
	{
		SetInitialInventory( pKVCriteria->GetInt( "inventoryPos" ) );
	}

	if ( pKVCriteria->FindKey( "quantity" ) )
	{
		SetInitialQuantity( pKVCriteria->GetInt( "quantity" ) );
	}

	if ( pKVCriteria->FindKey( "ignore_enabled" ) )
	{
		SetIgnoreEnabledFlag( pKVCriteria->GetBool( "ignore_enabled" ) );
	}

	if ( pKVCriteria->FindKey( "recent_only" ) )
	{
		SetRecentOnly( pKVCriteria->GetBool( "recent_only" ) );
	}

	return true;
}

bool CItemSelectionCriteria::BInitFromItemAndPaint( int nItemDef, int nPaintID, const CEconItemSchema &schemaa )
{
	/** Removed for partner depot **/
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Adds a condition to the selection criteria
// Input:	pszField - Field to evaluate on
//			eOp - Operator to apply to the value of the field
//			flValue - The value to compare.
//			bRequired - When true, causes BEvauluate to fail if pszField doesn't
//				exist in the KV being checked.
// Output:	True if the condition was added, false otherwise
//-----------------------------------------------------------------------------
bool CItemSelectionCriteria::BAddCondition( const char *pszField, EItemCriteriaOperator eOp, float flValue, bool bRequired )
{
	// Check for condition limit
	if ( UCHAR_MAX == GetConditionsCount() )
	{		
		AssertMsg( false, "Too many conditions on a a CItemSelectionCriteria. Max: 255" );
		return false;
	}

	// Enforce maximum string lengths
	if ( Q_strlen( pszField ) >= k_cchCreateItemLen )
		return false;

	if ( V_strcmp( pszField, "*lootlist" ) == 0 )
	{
		m_bIsLootList = true;
	}

	// Create the appropriate condition for the operator
	switch ( eOp )
	{
	case k_EOperator_Float_EQ:
	case k_EOperator_Float_Not_EQ:
	case k_EOperator_Float_LT:
	case k_EOperator_Float_Not_LT:
	case k_EOperator_Float_LTE:
	case k_EOperator_Float_Not_LTE:
	case k_EOperator_Float_GT:
	case k_EOperator_Float_Not_GT:
	case k_EOperator_Float_GTE:
	case k_EOperator_Float_Not_GTE:
		m_vecConditions.AddToTail( new CFloatCondition( pszField, eOp, flValue, bRequired ) );
		return true;

	default:
		AssertMsg1( false, "Bad operator (%d) passed to BAddCondition. Float based operator required for this overload.", eOp );
		return false;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Adds a condition to the selection criteria
// Input:	pszField - Field to evaluate on
//			eOp - Operator to apply to the value of the field
//			pszValue - The value to compare.
//			bRequired - When true, causes BEvauluate to fail if pszField doesn't
//				exist in the KV being checked.
// Output:	True if the condition was added, false otherwise
//-----------------------------------------------------------------------------
bool CItemSelectionCriteria::BAddCondition( const char *pszField, EItemCriteriaOperator eOp, const char * pszValue, bool bRequired )
{
	// Check for condition limit
	if ( UCHAR_MAX == GetConditionsCount() )
	{		
		AssertMsg( false, "Too many conditions on a a CItemSelectionCriteria. Max: 255" );
		return false;
	}

	// Enforce maximum string lengths
	if ( V_strlen( pszField ) >= k_cchCreateItemLen || Q_strlen( pszValue ) >= k_cchCreateItemLen )
		return false;

	if ( V_strcmp( pszField, "*lootlist" ) == 0 )
	{
		m_bIsLootList = true;
	}

	// Create the appropriate condition for the operator
	switch ( eOp )
	{
	case k_EOperator_String_EQ:
	case k_EOperator_String_Not_EQ:
		m_vecConditions.AddToTail( new CStringCondition( pszField, eOp, pszValue, bRequired ) );
		return true;

	case k_EOperator_Subkey_Contains:
	case k_EOperator_Subkey_Not_Contains:
		m_vecConditions.AddToTail( new CSetCondition( pszField, eOp, pszValue, bRequired ) );
		return true;

	default:
		// Try the float operators
		return BAddCondition( pszField, eOp, Q_atof( pszValue ), bRequired );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Checks if a given item matches the item selection criteria
// Input:	itemDef - The item definition to evaluate against
// Output:	True is the item passes the filter, false otherwise
//-----------------------------------------------------------------------------
bool CItemSelectionCriteria::BEvaluate( const CEconItemDefinition* pItemDef, const CEconItemSchema &pschema ) const
{
	// Disabled items never match
	if ( !m_bIgnoreEnabledFlag && !pItemDef->BEnabled() )
		return false;

	if ( m_bRecentOnly && !pItemDef->IsRecent() )
		return false;

	// Filter against level
	if ( BItemLevelSet() && (GetItemLevel() != AE_USE_SCRIPT_VALUE) &&
		( GetItemLevel() < pItemDef->GetMinLevel() || GetItemLevel() > pItemDef->GetMaxLevel() ) )
		return false;

	// Filter against quality
	if ( BQualitySet() && (GetQuality() != AE_USE_SCRIPT_VALUE) )
	{
		if ( GetQuality() != pItemDef->GetQuality() )
		{
			// Filter out item defs that have a non-any quality if we have a non-matching & non-any quality criteria
			if ( k_unItemQuality_Any != GetQuality() &&
				k_unItemQuality_Any != pItemDef->GetQuality() )
				return false;

			// Filter out item defs that don't have our exact quality if our quality criteria requires explicit matches
			const CEconItemQualityDefinition *pQuality = pschema.GetQualityDefinition( GetQuality() );
			if ( m_bForcedQualityMatch || ( pQuality && pQuality->GetRequiresExplicitMatches() ) )
				return false;
		}
	}

	// Filter against the additional conditions
	FOR_EACH_VEC( m_vecConditions, i )
	{
		// Skip over dynamic fields that require special logic from an econitem to process
		if ( m_vecConditions[i]->GetField()[ 0 ] == '*' )
			continue;

		if ( !m_vecConditions[i]->BEvaluate( pItemDef->GetRawDefinition() ) )
			return false;
	}

	return true;
}

bool CItemSelectionCriteria::BEvaluate( const CEconItem *pItem, const CEconItemSchema &pschema ) const
{
	const CEconItemDefinition *pItemDef = pItem->GetItemDefinition();

	if ( !BEvaluate( pItemDef, pschema ) )
		return false;

	// Filter against the dynamic conditions
	FOR_EACH_VEC( m_vecConditions, i )
	{
		// Skip over dynamic fields that require special logic from an econitem to process
		const char *pchCondition = m_vecConditions[i]->GetField();
		if ( pchCondition[ 0 ] != '*' )
			continue;

		pchCondition++;

		if ( V_strcmp( pchCondition, "rarity" ) == 0 )
		{
			const CEconItemRarityDefinition *pRarity = pschema.GetRarityDefinitionByName( m_vecConditions[i]->GetValue() );
			Assert( pRarity );

			if ( m_vecConditions[i]->GetEOp() == k_EOperator_Not || m_vecConditions[i]->GetEOp() == k_EOperator_String_Not_EQ )
			{
				if ( pItem->GetRarity() == pRarity->GetDBValue() )
					return false;
			}
			else
			{
				if ( pItem->GetRarity() != pRarity->GetDBValue() )
					return false;
			}
		}
		else if ( V_strcmp( pchCondition, "quality" ) == 0 )
		{
			const CEconItemQualityDefinition *pQuality = pschema.GetQualityDefinitionByName( m_vecConditions[i]->GetValue() );
			Assert( pQuality );

			if ( m_vecConditions[i]->GetEOp() == k_EOperator_Not || m_vecConditions[i]->GetEOp() == k_EOperator_String_Not_EQ )
			{
				if ( pItem->GetQuality() == pQuality->GetDBValue() )
					return false;
			}
			else
			{
				if ( pItem->GetQuality() != pQuality->GetDBValue() )
					return false;
			}
		}
		else if ( V_strcmp( pchCondition, "itemdef" ) == 0 )
		{
			if ( pItemDef->GetDefinitionIndex() != V_atoi( m_vecConditions[i]->GetValue() ) )
				return false;
		}
		else if ( V_strcmp( pchCondition, "paintkit" ) == 0 )
		{
			if ( pItem->GetCustomPaintKitIndex() != V_atoi( m_vecConditions[i]->GetValue() ) )
				return false;
		}
		else if ( V_strcmp( pchCondition, "kill_eater_score_type" ) == 0 )
		{
			static const CSchemaAttributeDefHandle pAttr_KillEaterScoreType( "kill eater score type" );
			attrib_value_t unKillEaterScoreType;
			bool bHasKillEaterScoreType = pItem->FindAttribute( pAttr_KillEaterScoreType, &unKillEaterScoreType );
			if ( !bHasKillEaterScoreType || unKillEaterScoreType != (uint32)V_atoi( m_vecConditions[ i ]->GetValue() ) )
			{
				return false;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Determines if the item matches this condition of the criteria
// Input:	pKVItem - Pointer to the raw KeyValues definition of the item
// Output:	True is the item matches, false otherwise
//-----------------------------------------------------------------------------
bool CItemSelectionCriteria::CCondition::BEvaluate( KeyValues *pKVItem ) const
{
	KeyValues *pKVField = pKVItem->FindKey( m_sField.String() );

	// Treat an empty string as a missing field as well.
	bool bIsEmptyString = false;
	if ( m_EOp == k_EOperator_String_EQ || m_EOp == k_EOperator_String_Not_EQ )
	{
		const char *pszItemVal = pKVField ? pKVField->GetString() : NULL;
		bIsEmptyString = ( pszItemVal == NULL || pszItemVal[0] == '\0' );
	}

	// Deal with missing fields
	if ( NULL == pKVField || bIsEmptyString )
	{
		if ( m_bRequired )
			return false;
		else
			return true;
	}

	// Run the operator specific check
	bool bRet = BInternalEvaluate( pKVItem );

	// If this is a "not" operator, reverse the result 
	if ( m_EOp & k_EOperator_Not )
		return !bRet;
	else
		return bRet;
}


//-----------------------------------------------------------------------------
// Purpose: Runs the operator specific check for this condition
// Input:	pKVItem - Pointer to the raw KeyValues definition of the item
// Output:	True is the item matches, false otherwise
//-----------------------------------------------------------------------------
bool CItemSelectionCriteria::CStringCondition::BInternalEvaluate( KeyValues *pKVItem ) const
{
	Assert( k_EOperator_String_EQ == m_EOp || k_EOperator_String_Not_EQ == m_EOp );
	if( !( k_EOperator_String_EQ == m_EOp || k_EOperator_String_Not_EQ == m_EOp ) )
		return false;

	const char *pszItemVal = pKVItem->GetString( m_sField.String() );
	return ( 0 == Q_stricmp( m_sValue.String(), pszItemVal ) );
}

bool CItemSelectionCriteria::CStringCondition::BSerializeToMsg( CSOItemCriteriaCondition & msg ) const
{
	CCondition::BSerializeToMsg( msg );
	msg.set_string_value( m_sValue.Get() );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Runs the operator specific check for this condition
// Input:	pKVItem - Pointer to the raw KeyValues definition of the item
// Output:	True is the item matches, false otherwise
//-----------------------------------------------------------------------------
bool CItemSelectionCriteria::CSetCondition::BInternalEvaluate( KeyValues *pKVItem ) const
{
	Assert( k_EOperator_Subkey_Contains == m_EOp || k_EOperator_Subkey_Not_Contains == m_EOp );
	if( !( k_EOperator_Subkey_Contains == m_EOp || k_EOperator_Subkey_Not_Contains == m_EOp ) )
		return false;

	return ( NULL != pKVItem->FindKey( m_sField.String() )->FindKey( m_sValue.String() ) );
}

bool CItemSelectionCriteria::CSetCondition::BSerializeToMsg( CSOItemCriteriaCondition & msg ) const
{
	CCondition::BSerializeToMsg( msg );
	msg.set_string_value( m_sValue.Get() );
	return true;
}

 
//-----------------------------------------------------------------------------
// Purpose: Runs the operator specific check for this condition
// Input:	pKVItem - Pointer to the raw KeyValues definition of the item
// Output:	True is the item matches, false otherwise
//-----------------------------------------------------------------------------
bool CItemSelectionCriteria::CFloatCondition::BInternalEvaluate( KeyValues *pKVItem ) const
{
	float itemValue = pKVItem->GetFloat( m_sField.String() );

	switch ( m_EOp )
	{
	case k_EOperator_Float_EQ:
	case k_EOperator_Float_Not_EQ:
		return ( itemValue == m_flValue );

	case k_EOperator_Float_LT:
	case k_EOperator_Float_Not_LT:
		return ( itemValue < m_flValue );

	case k_EOperator_Float_LTE:
	case k_EOperator_Float_Not_LTE:
		return ( itemValue <= m_flValue );

	case k_EOperator_Float_GT:
	case k_EOperator_Float_Not_GT:
		return ( itemValue > m_flValue );

	case k_EOperator_Float_GTE:
	case k_EOperator_Float_Not_GTE:
		return ( itemValue >= m_flValue );

	default:
		AssertMsg1( false, "Unknown operator: %d", m_EOp );
		return false;
	}
}

bool CItemSelectionCriteria::CFloatCondition::BSerializeToMsg( CSOItemCriteriaCondition & msg ) const
{
	CCondition::BSerializeToMsg( msg );
	msg.set_float_value( m_flValue );
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Serialize the item selection criteria to the given message
// Input:	msg - The message to serialize to.
// Output:	True if the operation was successful, false otherwise.
//-----------------------------------------------------------------------------
bool CItemSelectionCriteria::BSerializeToMsg( CSOItemCriteria & msg ) const
{
	msg.set_item_level( m_unItemLevel );
	msg.set_item_quality( m_nItemQuality );
	msg.set_item_rarity( m_nItemRarity );
	msg.set_item_level_set( m_bItemLevelSet );
	msg.set_item_quality_set( m_bQualitySet );
	msg.set_item_rarity_set( m_bRaritySet );
	msg.set_initial_inventory( m_unInitialInventory );
	msg.set_initial_quantity( m_unInitialQuantity );
	msg.set_ignore_enabled_flag( m_bIgnoreEnabledFlag );
	msg.set_recent_only( m_bRecentOnly );

	FOR_EACH_VEC( m_vecConditions, i )
	{
		CSOItemCriteriaCondition *pConditionMsg = msg.add_conditions();
		m_vecConditions[i]->BSerializeToMsg( *pConditionMsg );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Deserializes the item selection criteria from the given message
// Input:	msg - The message to deserialize from.
// Output:	True if the operation was successful, false otherwise.
//-----------------------------------------------------------------------------
bool CItemSelectionCriteria::BDeserializeFromMsg( const CSOItemCriteria & msg )
{
	m_unItemLevel = msg.item_level();
	m_nItemQuality = msg.item_quality();
	m_nItemRarity = msg.item_rarity();
	m_bItemLevelSet = msg.item_level_set();
	m_bQualitySet = msg.item_quality_set();
	m_bRaritySet = msg.item_rarity_set();
	m_unInitialInventory = msg.initial_inventory();
	m_unInitialQuantity = msg.initial_quantity();
	m_bIgnoreEnabledFlag = msg.ignore_enabled_flag();
	m_bRecentOnly = msg.recent_only();


	uint32 unCount = msg.conditions_size();
	m_vecConditions.EnsureCapacity( unCount );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Serializes a condition to a message.
// Input:	msg - The message to serialize to.
// Output:	True if the operation was successful, false otherwise.
//-----------------------------------------------------------------------------
bool CItemSelectionCriteria::CCondition::BSerializeToMsg( CSOItemCriteriaCondition & msg ) const
{
	msg.set_op( m_EOp );
	msg.set_field( m_sField.String() );
	msg.set_required( m_bRequired );
	return true;
}

