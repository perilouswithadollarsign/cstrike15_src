//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: CEconItem, a shared object for econ items
//
//=============================================================================

#include "cbase.h"
#include "econ_item.h"
#include "econ_item_schema.h"
#include "smartptr.h"

#ifdef CSTRIKE15
	#include "cstrike15_gcmessages.pb.h"
#endif

#define ECON_ITEM_SET_NOT_YET_SCANNED 0xFFu
#define ECON_ITEM_SET_INVALID 0xFEu

using namespace GCSDK;

#ifdef CLIENT_DLL
#include "bannedwords.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

/*static*/ const schema_attribute_stat_bucket_t *CSchemaAttributeStats::m_pHead;

#ifndef GC_DLL
//-----------------------------------------------------------------------------
// Purpose: Utility function to match two items based on their item views
//-----------------------------------------------------------------------------
bool ItemsMatch( CEconItemView *pCurItem, CEconItemView *pNewItem )
{
	if ( !pNewItem || !pNewItem->IsValid() || !pCurItem || !pCurItem->IsValid() )
		return false;

	// If we already have an item in this slot but is not the same type, nuke it (changed classes)
	// We don't need to do this for non-base items because they've already been verified above.
	bool bHasNonBase = pNewItem ? pNewItem->GetItemQuality() != AE_NORMAL : false;
	if ( bHasNonBase )
	{
		// If the item isn't the one we're supposed to have, nuke it
		if ( pCurItem->GetItemID() != pNewItem->GetItemID() || pCurItem->GetItemID() == 0 || pNewItem->GetItemID() == 0 )
		{
			/*
			Msg("Removing %s because its global index (%d) doesn't match the loadout's (%d)\n", p->GetDebugName(), 
				pCurItem->GetItemID(),
				pNewItem->GetItemID() );
			*/
			return false;
		}
	}
	else
	{
		if ( pCurItem->GetItemQuality() != AE_NORMAL || (pCurItem->GetItemIndex() != pNewItem->GetItemIndex()) )
		{
			//Msg("Removing %s because it's not the right type for the class.\n", p->GetDebugName() );
			return false;
		}
	}

	return true;
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Utility function to convert datafile strings to ints.
//-----------------------------------------------------------------------------
int StringFieldToInt( const char *szValue, const char **pValueStrings, int iNumStrings, bool bDontAssert ) 
{
	if ( !szValue || !szValue[0] )
		return -1;

	for ( int i = 0; i < iNumStrings; i++ )
	{
		if ( !Q_stricmp(szValue, pValueStrings[i]) )
			return i;
	}

	if ( !bDontAssert )
	{
		Assert( !"Missing value in StringFieldToInt()!" );
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Utility function to convert datafile strings to ints.
//-----------------------------------------------------------------------------
int StringFieldToInt( const char *szValue, const CUtlVector<const char *>& vecValueStrings, bool bDontAssert )
{
	return StringFieldToInt( szValue, (const char **)&vecValueStrings[0], vecValueStrings.Count(), bDontAssert );
}


//////////////////////////////////////////////////////////////////////////
//
// Implementation of CS:GO optimized custom data object
//


static inline uint32 ComputeEconItemCustomDataOptimizedObjectAllocationSize( uint32 numAttributes )
{
	uint32 numBytesNeeded = sizeof( CEconItem::CustomDataOptimizedObject_t ) + numAttributes*sizeof( CEconItem::attribute_t );

	// Since we know that attributes are allocated in SBH we can allocate more memory then needed to allow
	// for less memory copying when growing the container -
	if ( sizeof( CEconItem::attribute_t ) < 8 )
		numBytesNeeded = ( ( numBytesNeeded <= 128 ) ? ( ( numBytesNeeded + 7 ) / 8 ) * 8 : ( ( numBytesNeeded + 31 ) / 32 ) * 32 ); // round up to SBH boundaries

	return numBytesNeeded;
}

void HelperDumpMemStatsEconItemAttributes() {}

bool Helper_IsSticker( const CEconItemDefinition * pEconItemDefinition )
{
	static CSchemaItemDefHandle hItemDefCompare( "sticker" );
	return pEconItemDefinition && hItemDefCompare && pEconItemDefinition->IsTool() && ( pEconItemDefinition->GetDefinitionIndex() == hItemDefCompare->GetDefinitionIndex() );
}
bool Helper_IsSpray( const CEconItemDefinition * pEconItemDefinition )
{
	static CSchemaItemDefHandle hItemDefCompare( "spray" );
	static CSchemaItemDefHandle hItemDefCompare2( "spraypaint" );
	return pEconItemDefinition && pEconItemDefinition->IsTool() && (
		( hItemDefCompare && ( pEconItemDefinition->GetDefinitionIndex() == hItemDefCompare->GetDefinitionIndex() ) )
		|| ( hItemDefCompare2 && ( pEconItemDefinition->GetDefinitionIndex() == hItemDefCompare2->GetDefinitionIndex() ) )
		)
		;
}
bool Helper_IsGraphicTool( const CEconItemDefinition * pEconItemDefinition )
{
	return Helper_IsSticker( pEconItemDefinition ) || Helper_IsSpray( pEconItemDefinition );
}


CEconItem::CustomDataOptimizedObject_t * CEconItem::CustomDataOptimizedObject_t::Alloc( uint32 numAttributes )
{
	uint32 numBytesNeeded = ComputeEconItemCustomDataOptimizedObjectAllocationSize( numAttributes );
	CEconItem::CustomDataOptimizedObject_t *ptr = reinterpret_cast< CEconItem::CustomDataOptimizedObject_t * >( malloc( numBytesNeeded ) );
	ptr->m_equipInstanceSlot1 = INVALID_EQUIPPED_SLOT_BITPACKED;
	ptr->m_numAttributes = numAttributes;
	return ptr;
}

CEconItem::attribute_t * CEconItem::CustomDataOptimizedObject_t::AddAttribute( CustomDataOptimizedObject_t * &rptr )
{
	uint32 numAttributesOld = rptr->m_numAttributes;

	uint32 numBytesOldAllocated = ComputeEconItemCustomDataOptimizedObjectAllocationSize( numAttributesOld );
	uint32 numBytesNeededNew = ComputeEconItemCustomDataOptimizedObjectAllocationSize( numAttributesOld + 1 );

	if ( numBytesNeededNew > numBytesOldAllocated )
	{
		CEconItem::CustomDataOptimizedObject_t *ptr = CEconItem::CustomDataOptimizedObject_t::Alloc( numAttributesOld + 1 );
		Q_memcpy( ptr, rptr, sizeof( CEconItem::CustomDataOptimizedObject_t ) + rptr->m_numAttributes*sizeof( CEconItem::attribute_t ) );
		free( rptr );
		rptr = ptr;
	}
	rptr->m_numAttributes = numAttributesOld + 1;
	return rptr->GetAttribute( numAttributesOld );
}

void CEconItem::CustomDataOptimizedObject_t::RemoveAndFreeAttrMemory( uint32 idxAttributeInArray )
{
	Assert( m_numAttributes );
	Assert( idxAttributeInArray < m_numAttributes );
	attribute_t *pAttr = GetAttribute( idxAttributeInArray );
	CEconItem::FreeAttributeMemory( pAttr );
	uint32 idxLastAttribute = m_numAttributes - 1;
	if ( idxAttributeInArray != idxLastAttribute )
	{
		Q_memcpy( pAttr, GetAttribute( idxLastAttribute ), sizeof( CEconItem::attribute_t ) );
	}
	m_numAttributes = idxLastAttribute;
}

void CEconItem::CustomDataOptimizedObject_t::FreeObjectAndAttrMemory()
{
	CEconItem::attribute_t *pAttr = reinterpret_cast< CEconItem::attribute_t * >( this + 1 );
	CEconItem::attribute_t *pAttrEnd = pAttr + m_numAttributes;
	for ( ; pAttr < pAttrEnd; ++ pAttr )
	{
		CEconItem::FreeAttributeMemory( pAttr );
	}
	free( this );
}

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
CEconItem::CEconItem()
	: BaseClass( ),
	m_pCustomDataOptimizedObject( NULL )
{
	m_ulID = 0;
	m_ulOriginalID = 0;
	m_iItemSet = ECON_ITEM_SET_NOT_YET_SCANNED;
#ifndef GC_DLL
	m_bSOUpdateFrame = -1;
#endif
	
	m_unAccountID = 0;
	m_unInventory = 0;
	m_unLevel = 0;
	m_nQuality = 0;
	m_unOrigin = 0;
	m_nRarity = 0;
	m_unFlags = 0;
	m_dirtybitInUse = 0;
}

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
CEconItem::~CEconItem()
{
	if ( m_pCustomDataOptimizedObject )
		m_pCustomDataOptimizedObject->FreeObjectAndAttrMemory();
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::CopyAttributesFrom( const CEconItem& source )
{
	// Copy attributes -- each new instance needs to be allocated and then copied into by somewhere
	// that knows what the actual type is. Rather than do anything type-specific here, we just have each
	// attribute serialize it's value to a bytestream and then deserialize it. This is as safe as we can
	// make it but sort of silly wasteful.
	for ( int i = 0; i < source.GetDynamicAttributeCountInternal(); i++ )
	{
		attribute_t const &attr = source.GetDynamicAttributeInternal( i );

		const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( attr.m_unDefinitionIndex );
		Assert( pAttrDef );

		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		std::string sBytes;
		pAttrType->ConvertEconAttributeValueToByteStream( attr.m_value, &sBytes );
		pAttrType->LoadByteStreamToEconAttributeValue( this, pAttrDef, sBytes );
	}
}

void CEconItem::CopyWithoutAttributesFrom( const CEconItem& rhs )
{
	// We do destructive operations on our local object, including freeing attribute memory, as part of
	// the copy, so we force self-copies to be a no-op.
	if ( &rhs == this )
	{
		Assert( false ); // this probably not what you want!! This will not wipe attributes on the item.
		return;
	}

	m_ulID = rhs.m_ulID;
	SetOriginalID( rhs.GetOriginalID() );
	m_unAccountID = rhs.m_unAccountID;
	m_unDefIndex = rhs.m_unDefIndex;
	m_unLevel = rhs.m_unLevel;
	m_nQuality = rhs.m_nQuality;
	m_nRarity = rhs.m_nRarity;
	m_unInventory = rhs.m_unInventory;
	SetQuantity( rhs.GetQuantity() );
	m_unFlags = rhs.m_unFlags;
	m_unOrigin = rhs.m_unOrigin;

	m_dirtybitInUse = rhs.m_dirtybitInUse;

	m_iItemSet = rhs.m_iItemSet;
	if ( m_pCustomDataOptimizedObject )
	{
		m_pCustomDataOptimizedObject->FreeObjectAndAttrMemory();
		m_pCustomDataOptimizedObject = NULL;
	}
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
CEconItem &CEconItem::operator=( const CEconItem& rhs )
{
	// We do destructive operations on our local object, including freeing attribute memory, as part of
	// the copy, so we force self-copies to be a no-op.
	if ( &rhs == this )
		return *this;

	// Copy all plain data
	CopyWithoutAttributesFrom( rhs );
	
	//
	// see -- CopyAttributesFrom( rhs );
	//
	// copied for more efficient memory management
	//
	if ( rhs.m_pCustomDataOptimizedObject && rhs.m_pCustomDataOptimizedObject->m_numAttributes )
	{
		uint32 numAttributes = rhs.m_pCustomDataOptimizedObject->m_numAttributes;
		m_pCustomDataOptimizedObject = CustomDataOptimizedObject_t::Alloc( numAttributes );
		for ( uint32 i = 0; i < numAttributes; ++ i )
		{
			attribute_t const &attr = * rhs.m_pCustomDataOptimizedObject->GetAttribute( i );

			const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( attr.m_unDefinitionIndex );
			Assert( pAttrDef );

			const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
			Assert( pAttrType );

			std::string sBytes;
			pAttrType->ConvertEconAttributeValueToByteStream( attr.m_value, &sBytes );

			// Make an efficient copy of the attribute at the end of the array
			attribute_t *pEconAttribCopy = m_pCustomDataOptimizedObject->GetAttribute( i );
			pEconAttribCopy->m_unDefinitionIndex = pAttrDef->GetDefinitionIndex();
			pAttrType->InitializeNewEconAttributeValue( &pEconAttribCopy->m_value );
			pAttrType->LoadByteStreamToEconAttributeValue( this, pAttrDef, sBytes );
		}
	}

	// Transfer equip state as well
	if ( rhs.m_pCustomDataOptimizedObject )
	{
		// Attributes transfer can allocate optimized custom data, but if we had no attributes then allocate it here
		if ( !m_pCustomDataOptimizedObject )
			m_pCustomDataOptimizedObject = CustomDataOptimizedObject_t::Alloc( 0 );

		// Transfer equip state values as well
		m_pCustomDataOptimizedObject->m_equipInstanceSlot1 = rhs.m_pCustomDataOptimizedObject->m_equipInstanceSlot1;
		m_pCustomDataOptimizedObject->m_equipInstanceClass1 = rhs.m_pCustomDataOptimizedObject->m_equipInstanceClass1;
		m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit = rhs.m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit;
	}

	return *this;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::SetItemID( itemid_t ulID ) 
{
	uint64 ulOldID = m_ulID;
	m_ulID = ulID;
	// only overwrite if we don't have an original id currently and we are a new item cloned off an old item
	if ( ulOldID != 0 && ulOldID != ulID && m_ulOriginalID == 0 && ulID != INVALID_ITEM_ID && ulOldID != INVALID_ITEM_ID )
	{
		SetOriginalID( ulOldID );
	}
	 
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
itemid_t CEconItem::GetOriginalID() const
{
	return m_ulOriginalID ? m_ulOriginalID : m_ulID;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::SetOriginalID( itemid_t ulOriginalID )
{
	if ( ulOriginalID != m_ulID )
		m_ulOriginalID = ulOriginalID;
}

int32 CEconItem::GetRarity() const
{
	int nDefRarity = GetItemDefinition() ? GetItemDefinition()->GetRarity() : 1;
	bool bIsTool = GetItemDefinition() ? GetItemDefinition()->IsTool() : false;

	const CEconItemRarityDefinition *pImmortal = GetItemSchema()->GetRarityDefinitionByName( "immortal" );
	if ( nDefRarity == pImmortal->GetDBValue() )
	{
		// Items that had their definition marked as immortal just use that!
		return nDefRarity;
	}

	// Check if the item has its rarity set in the database
	if ( m_nRarity != k_unItemRarity_Any )
	{
		// HACK: The default value for rarity used to be 0 and some coins and passes were created that way, 
		// but since their rarity should have been k_unItemRarity_Any they actually need to fall back to the definition rarity
		if ( !( m_nRarity == 0 && ( nDefRarity >= 6 || bIsTool ) ) )
		{
			return m_nRarity;
		}
	}

	return nDefRarity;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
uint16 CEconItem::GetQuantity() const
{
	return 1;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::SetQuantity( uint16 unQuantity )
{
	Assert( unQuantity <= 1 );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::InitAttributesDroppedFromListEntry( item_list_entry_t const *pEntryInfo )
{
	const CEconItemDefinition *pItemDef = GetItemDefinition();

	// Set the paint kit
	if ( pEntryInfo )
	{
		extern bool Helper_IsGraphicTool( const CEconItemDefinition * pEconItemDefinition );
		if ( pEntryInfo->m_nStickerKit && pItemDef && Helper_IsGraphicTool( pItemDef ) )
		{
			const CStickerKit *pStickerKit = GetItemSchema()->GetStickerKitDefinition( pEntryInfo->m_nStickerKit );
			if ( pStickerKit )
			{
				static CSchemaAttributeDefHandle pAttr_StickerKit( "sticker slot 0 id" );
				if ( pAttr_StickerKit )
				{
					SetDynamicAttributeValue( pAttr_StickerKit, pEntryInfo->m_nStickerKit );
				}

				SetRarity( EconRarity_CombinedItemAndPaintRarity( pItemDef->GetRarity(), pStickerKit->nRarity ) );
			}
		}
		else if ( pEntryInfo->m_nPaintKit )
		{
			const CPaintKit *pPaintKit = GetItemSchema()->GetPaintKitDefinition( pEntryInfo->m_nPaintKit );
			if ( pPaintKit )
			{
				static CSchemaAttributeDefHandle pAttr_PaintKit( "set item texture prefab" );
				if ( pAttr_PaintKit )
				{
					AddOrSetCustomAttribute( pAttr_PaintKit->GetDefinitionIndex(), pEntryInfo->m_nPaintKit );
				}

				static CSchemaAttributeDefHandle pAttr_PaintKitSeed( "set item texture seed" );
				if ( pAttr_PaintKitSeed && ( pEntryInfo->m_nPaintKitSeed >= -1 ) )
				{
					int nSeed = pEntryInfo->m_nPaintKitSeed;
					if ( nSeed < 0 )
					{
						nSeed = RandomInt( 0, 1000 );
					}


					AddOrSetCustomAttribute( pAttr_PaintKitSeed->GetDefinitionIndex(), nSeed );
				}

				static CSchemaAttributeDefHandle pAttr_PaintKitWear( "set item texture wear" );
				if ( pAttr_PaintKitWear && ( pEntryInfo->m_flPaintKitWear >= -1.1 ) )
				{
					float flWear = pEntryInfo->m_flPaintKitWear;
					if ( flWear < 0 )
					{
						flWear = RandomFloat();
					}
					AddOrSetCustomAttribute( pAttr_PaintKitWear->GetDefinitionIndex(), 
											 RemapValClamped( flWear, 0.0f, 1.0f, pPaintKit->flWearRemapMin, pPaintKit->flWearRemapMax ) );
				}

				SetRarity( EconRarity_CombinedItemAndPaintRarity( GetItemDefinition()->GetRarity(), pPaintKit->nRarity ) );
			}
		}
		else if ( pEntryInfo->m_nMusicKit )
		{
			const CEconMusicDefinition *pMusicDef = GetItemSchema()->GetMusicDefinition( pEntryInfo->m_nMusicKit );
			if ( pMusicDef )
			{
				static CSchemaAttributeDefHandle pAttr_Music( "music id" );
				if ( pAttr_Music )
				{
					SetDynamicAttributeValue( pAttr_Music, pEntryInfo->m_nMusicKit );	
				}
			}
		}
	}

	if ( pItemDef )
	{
		int nDefaultDropQuality = pItemDef->GetDefaultDropQuality();
		if ( nDefaultDropQuality != k_unItemQuality_Any )
		{
			SetQuality( nDefaultDropQuality );
		}
	}
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
static const char *GetCustomNameOrAttributeDesc( const CEconItem *pItem, const CEconItemAttributeDefinition *pAttrDef )
{
	if ( !pAttrDef )
	{
		// If we didn't specify the attribute in the schema we can't possibly have an
		// answer. This isn't really an error in that case.
		return NULL;
	}

	const char *pszStrContents;
	if ( FindAttribute_UnsafeBitwiseCast<CAttribute_String>( pItem, pAttrDef, &pszStrContents ) )
	{
		#ifdef CLIENT_DLL
		if ( pszStrContents && *pszStrContents )
			g_BannedWords.CensorBannedWordsInplace( const_cast< char * >( pszStrContents ) );
		#endif
		return pszStrContents;
	}

	return NULL;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
static void SetCustomNameOrDescAttribute( CEconItem *pItem, const CEconItemAttributeDefinition *pAttrDef, const char *pszNewValue )
{
	Assert( pItem );

	if ( !pAttrDef )
	{
		// If we didn't specify the attribute in the schema, that's fine if we're setting
		// the empty name/description string, but it isn't fine if we're trying to set
		// actual content.
		AssertMsg( !pszNewValue, "Attempt to set non-empty value for custom name/desc with no attribute present." );
		return;
	}

	// Removing existing value?
	if ( !pszNewValue || !pszNewValue[0] )
	{
		pItem->RemoveDynamicAttribute( pAttrDef );
		return;
	}

	CAttribute_String attrStr;
	attrStr.set_value( pszNewValue );

	pItem->SetDynamicAttributeValue( pAttrDef, attrStr );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
const char *CEconItem::GetCustomName() const
{
	static CSchemaAttributeDefHandle pAttrDef_CustomName( "custom name attr" );

	return GetCustomNameOrAttributeDesc( this, pAttrDef_CustomName );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::SetCustomName( const char *pName )
{
	static CSchemaAttributeDefHandle pAttrDef_CustomName( "custom name attr" );

	SetCustomNameOrDescAttribute( this, pAttrDef_CustomName, pName );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItem::IsEquipped() const
{
	return m_pCustomDataOptimizedObject && ( m_pCustomDataOptimizedObject->m_equipInstanceSlot1 != INVALID_EQUIPPED_SLOT_BITPACKED );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItem::IsEquippedForClass( equipped_class_t unClass ) const
{
	return m_pCustomDataOptimizedObject && ( m_pCustomDataOptimizedObject->m_equipInstanceSlot1 != INVALID_EQUIPPED_SLOT_BITPACKED ) &&
		(	// CS:GO optimized test - equip class is 0 or 2/3
			( unClass == m_pCustomDataOptimizedObject->m_equipInstanceClass1 ) ||
			( ( unClass == 3 ) && ( m_pCustomDataOptimizedObject->m_equipInstanceClass1 == 2 ) && ( m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit != 0 ) )
		);
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
equipped_slot_t CEconItem::GetEquippedPositionForClass( equipped_class_t unClass ) const
{
	return IsEquippedForClass( unClass ) ? m_pCustomDataOptimizedObject->m_equipInstanceSlot1 : INVALID_EQUIPPED_SLOT;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::UpdateEquippedState( EquippedInstance_t equipInstance )
{
	// If it's invalid we need to remove it
	if ( equipInstance.m_unEquippedSlot == INVALID_EQUIPPED_SLOT )
	{
		if ( m_pCustomDataOptimizedObject && ( m_pCustomDataOptimizedObject->m_equipInstanceSlot1 != INVALID_EQUIPPED_SLOT_BITPACKED ) )
		{
			if ( equipInstance.m_unEquippedClass == m_pCustomDataOptimizedObject->m_equipInstanceClass1 )
			{
				if ( m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit )
				{
					// Move the 2nd class down
					if ( equipInstance.m_unEquippedClass == 2 )
					{	// leave it equipped for class 3
						m_pCustomDataOptimizedObject->m_equipInstanceClass1 = 3;
						m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit = 0;
						return;
					}
					else
					{
						Assert( equipInstance.m_unEquippedClass == 2 ); // weird case, claims to be equipped for both classes, but first class is invalid!
					}
				}

				// Fully unequip
				m_pCustomDataOptimizedObject->m_equipInstanceSlot1 = INVALID_EQUIPPED_SLOT_BITPACKED;
				if ( !m_pCustomDataOptimizedObject->m_numAttributes )
				{
					m_pCustomDataOptimizedObject->FreeObjectAndAttrMemory();
					m_pCustomDataOptimizedObject = NULL;
				}
				return;
			}
			else if ( ( equipInstance.m_unEquippedClass == 3 ) && ( m_pCustomDataOptimizedObject->m_equipInstanceClass1 == 2 ) && ( m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit != 0 ) )
			{
				// was equipped for both, unequipping 3rd class
				m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit = 0;
				return;
			}
		}
		// item was not equipped to begin with
		return;
	}

	// If this item is already equipped
	if ( m_pCustomDataOptimizedObject && ( m_pCustomDataOptimizedObject->m_equipInstanceSlot1 != INVALID_EQUIPPED_SLOT_BITPACKED ) )
	{
		m_pCustomDataOptimizedObject->m_equipInstanceSlot1 = equipInstance.m_unEquippedSlot;
		switch ( equipInstance.m_unEquippedClass )
		{
		case 0: // non-team item
			m_pCustomDataOptimizedObject->m_equipInstanceClass1 = 0;
			m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit = 0;
			return;
		case 2:
			if ( m_pCustomDataOptimizedObject->m_equipInstanceClass1 == 3 )
			{
				m_pCustomDataOptimizedObject->m_equipInstanceClass1 = 2;
				m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit = 1;
			}
			else
			{
				m_pCustomDataOptimizedObject->m_equipInstanceClass1 = 2;
				m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit = 0;
			}
			return;
		case 3:
			if ( m_pCustomDataOptimizedObject->m_equipInstanceClass1 == 2 )
			{
				m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit = 1;
			}
			else
			{
				m_pCustomDataOptimizedObject->m_equipInstanceClass1 = 3;
				m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit = 0;
			}
			return;
		default:
			Assert( false );
			return;
		}
	}
	
	// Otherwise this item has not been equipped yet
	if ( !m_pCustomDataOptimizedObject )
	{
		m_pCustomDataOptimizedObject = CustomDataOptimizedObject_t::Alloc( 0 );
	}

	m_pCustomDataOptimizedObject->m_equipInstanceSlot1 = equipInstance.m_unEquippedSlot;
	m_pCustomDataOptimizedObject->m_equipInstanceClass1 = equipInstance.m_unEquippedClass;
	m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit = 0;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
const char *CEconItem::GetCustomDesc() const
{
	static CSchemaAttributeDefHandle pAttrDef_CustomDesc( "custom desc attr" );

	return GetCustomNameOrAttributeDesc( this, pAttrDef_CustomDesc );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::SetCustomDesc( const char *pDesc )
{
	static CSchemaAttributeDefHandle pAttrDef_CustomDesc( "custom desc attr" );

	SetCustomNameOrDescAttribute( this, pAttrDef_CustomDesc, pDesc );
}

int CEconItem::GetItemSetIndex() const
{
	// If we already cached it, use that one
	if ( m_iItemSet != ECON_ITEM_SET_NOT_YET_SCANNED )
		return ( ( m_iItemSet == ECON_ITEM_SET_INVALID ) ? -1 : m_iItemSet );
	
	// Mark it as cached and invalid
	m_iItemSet = ECON_ITEM_SET_INVALID;

	const CEconItemDefinition  *pItemDef = GetItemSchema()->GetItemDefinition( GetDefinitionIndex() );
	if ( !pItemDef )
		return ( ( m_iItemSet == ECON_ITEM_SET_INVALID ) ? -1 : m_iItemSet );

	// They might not have any possible sets
	const CUtlVector< int > &itemSets = pItemDef->GetItemSets();
	if ( itemSets.Count() == 0 )
		return ( ( m_iItemSet == ECON_ITEM_SET_INVALID ) ? -1 : m_iItemSet );
		
	// Paint kit specified, so we need to match it
	int nPaintKit = GetCustomPaintKitIndex();

	FOR_EACH_VEC( itemSets, nItemSet )
	{
		int nItemSetIndex = itemSets[ nItemSet ];

		const CEconItemSetDefinition *pItemSetDef = GetItemSchema()->GetItemSetByIndex( nItemSetIndex );
		if ( !pItemSetDef )
			continue;

		FOR_EACH_VEC( pItemSetDef->m_ItemEntries, i )
		{
			if ( pItemSetDef->m_ItemEntries[ i ].m_nItemDef != pItemDef->GetDefinitionIndex() || 
				 pItemSetDef->m_ItemEntries[ i ].m_nPaintKit != nPaintKit )
			{
				continue;
			}

			m_iItemSet = nItemSetIndex;
			return ( ( m_iItemSet == ECON_ITEM_SET_INVALID ) ? -1 : m_iItemSet );
		}
	}

	return ( ( m_iItemSet == ECON_ITEM_SET_INVALID ) ? -1 : m_iItemSet );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItem::GetInUse() const
{
	return m_dirtybitInUse != 0;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::SetInUse( bool bInUse )
{
	m_dirtybitInUse = bInUse ? 1 : 0;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
const GameItemDefinition_t *CEconItem::GetItemDefinition() const
{
	const CEconItemDefinition  *pRet	  = GetItemSchema()->GetItemDefinition( GetDefinitionIndex() );
	const GameItemDefinition_t *pTypedRet = dynamic_cast<const GameItemDefinition_t *>( pRet );

	AssertMsg( pRet == pTypedRet, "Item definition of inappropriate type." );

	return pTypedRet;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItem::IsTradable() const
{
	return !m_dirtybitInUse
		&& IEconItemInterface::IsTradable();
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItem::IsMarketable() const
{
	return !m_dirtybitInUse
		&& IEconItemInterface::IsMarketable();
}

void CEconItem::IterateAttributes( IEconItemAttributeIterator *pIterator ) const
{
	Assert( pIterator );

	// custom attributes?
	for ( int i = 0; i < GetDynamicAttributeCountInternal(); i++ )
	{
		attribute_t const &attrib = GetDynamicAttributeInternal( i );
		const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( attrib.m_unDefinitionIndex );
		if ( !pAttrDef )
			continue;

		if ( !pAttrDef->GetAttributeType()->OnIterateAttributeValue( pIterator, pAttrDef, attrib.m_value ) )
			return;
	}

	// in static attributes?
	const CEconItemDefinition *pItemDef = GetItemDefinition();
	if ( !pItemDef )
		return;

	pItemDef->IterateAttributes( pIterator );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
bool CEconItem::IsUsableInCrafting() const
{
	return !m_dirtybitInUse
		&& IEconItemInterface::IsUsableInCrafting();
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
int CEconItem::GetDynamicAttributeCountInternal() const
{
	return m_pCustomDataOptimizedObject ? m_pCustomDataOptimizedObject->m_numAttributes : 0;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
const CEconItem::attribute_t & CEconItem::GetDynamicAttributeInternal( int iAttrIndexIntoArray ) const
{
	Assert( iAttrIndexIntoArray >= 0 );
	Assert( iAttrIndexIntoArray < GetDynamicAttributeCountInternal() );

	return *m_pCustomDataOptimizedObject->GetAttribute( iAttrIndexIntoArray );
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
CEconItem::attribute_t *CEconItem::FindDynamicAttributeInternal( const CEconItemAttributeDefinition *pAttrDef )
{
	Assert( pAttrDef );

	if ( m_pCustomDataOptimizedObject )
	{
		attribute_t *pAttr = m_pCustomDataOptimizedObject->GetAttribute( 0 );
		attribute_t *pAttrEnd = pAttr + m_pCustomDataOptimizedObject->m_numAttributes;
		for ( ; pAttr < pAttrEnd; ++pAttr )
		{
			if ( pAttr->m_unDefinitionIndex == pAttrDef->GetDefinitionIndex() )
				return pAttr;
		}
	}

	return NULL;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::AddCustomAttribute( uint16 usDefinitionIndex, float flValue )
{
	attribute_t &attrib = AddDynamicAttributeInternal();
	attrib.m_unDefinitionIndex = usDefinitionIndex;
	attrib.m_value.asFloat = flValue;
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
void CEconItem::AddOrSetCustomAttribute( uint16 usDefinitionIndex, float flValue )
{
	attribute_t *pAttrib = FindDynamicAttributeInternal( GetItemSchema()->GetAttributeDefinition( usDefinitionIndex ) );
	if ( NULL != pAttrib )
	{
		pAttrib->m_value.asFloat = flValue;
		return;
	}

	AddCustomAttribute( usDefinitionIndex, flValue );
}

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
CEconItem::attribute_t &CEconItem::AddDynamicAttributeInternal()
{
	if ( !m_pCustomDataOptimizedObject )
		return * ( m_pCustomDataOptimizedObject = CustomDataOptimizedObject_t::Alloc( 1 ) )->GetAttribute( 0 );
	else
		return * CustomDataOptimizedObject_t::AddAttribute( m_pCustomDataOptimizedObject );
}

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
void CEconItem::RemoveDynamicAttribute( const CEconItemAttributeDefinition *pAttrDef )
{
	Assert( pAttrDef );
	Assert( pAttrDef->GetDefinitionIndex() != INVALID_ITEM_DEF_INDEX );

	if ( m_pCustomDataOptimizedObject )
	{
		attribute_t *pAttr = m_pCustomDataOptimizedObject->GetAttribute( 0 );
		attribute_t *pAttrStart = pAttr;
		attribute_t *pAttrEnd = pAttr + m_pCustomDataOptimizedObject->m_numAttributes;
		for ( ; pAttr < pAttrEnd; ++pAttr )
		{
			if ( pAttr->m_unDefinitionIndex == pAttrDef->GetDefinitionIndex() )
			{
				m_pCustomDataOptimizedObject->RemoveAndFreeAttrMemory( pAttr - pAttrStart );
				return;
			}
		}
	}
}

// --------------------------------------------------------------------------
// Purpose:
// --------------------------------------------------------------------------
/*static*/ void CEconItem::FreeAttributeMemory( CEconItem::attribute_t *pAttrib )
{
	Assert( pAttrib );

	const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( pAttrib->m_unDefinitionIndex );
	Assert( pAttrDef );

	const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
	Assert( pAttrType );

	pAttrType->UnloadEconAttributeValue( &pAttrib->m_value );
}

// --------------------------------------------------------------------------
// Purpose: This item has been traded. Give it an opportunity to update any internal
//			properties in response.
// --------------------------------------------------------------------------
void CEconItem::OnTransferredOwnership()
{
	/** Removed for partner depot **/
}

// --------------------------------------------------------------------------
// Purpose: Parses the bits required to create a econ item from the message. 
//			Overloaded to include support for attributes.
// --------------------------------------------------------------------------
bool CEconItem::BParseFromMessage( const CUtlBuffer & buffer ) 
{
	CSOEconItem msgItem;
	if( !msgItem.ParseFromArray( buffer.Base(), buffer.TellMaxPut() ) )
		return false;

	DeserializeFromProtoBufItem( msgItem );
	return true;
}

// --------------------------------------------------------------------------
// Purpose: Parses the bits required to create a econ item from the message. 
//			Overloaded to include support for attributes.
// --------------------------------------------------------------------------
bool CEconItem::BParseFromMessage( const std::string &buffer ) 
{
	CSOEconItem msgItem;
	if( !msgItem.ParseFromString( buffer ) )
		return false;

	DeserializeFromProtoBufItem( msgItem );
	return true;
}


//----------------------------------------------------------------------------
// Purpose: Overrides all the fields in msgLocal that are present in the 
//			network message
//----------------------------------------------------------------------------
bool CEconItem::BUpdateFromNetwork( const CSharedObject & objUpdate )
{
	const CEconItem & econObjUpdate = (const CEconItem &)objUpdate;

	*this = econObjUpdate;
	return true;
}

//----------------------------------------------------------------------------
// Purpose: Adds the relevant bits to update this object to the message. This
//			must include any relevant information about which fields are being
//			updated. This is called once for all subscribers.
//----------------------------------------------------------------------------
static CSOEconItem g_msgEconItem;
bool CEconItem::BAddToMessage( std::string *pBuffer ) const
{
	VPROF_BUDGET( "CEconItem::BAddToMessage::std::string", VPROF_BUDGETGROUP_STEAM );

	//this function is called A LOT, therefore we use a static item to avoid a lot of re-allocations. However, this means that
	//this should never be re-entrant
	g_msgEconItem.Clear();
	SerializeToProtoBufItem( g_msgEconItem );
	return g_msgEconItem.SerializeToString( pBuffer );
}

//----------------------------------------------------------------------------
// Purpose: Adds just the item ID to the message so that the client can find
//			which item to destroy
//----------------------------------------------------------------------------
bool CEconItem::BAddDestroyToMessage( std::string *pBuffer ) const
{
	CSOEconItem msgItem;
	msgItem.set_id( GetItemID() );
	return msgItem.SerializeToString( pBuffer );
}

//----------------------------------------------------------------------------
// Purpose: Returns true if this is less than than the object in soRHS. This
//			comparison is deterministic, but it may not be pleasing to a user
//			since it is just going to compare raw memory. If you need a sort 
//			that is user-visible you will need to do it at a higher level that
//			actually knows what the data in these objects means.
//----------------------------------------------------------------------------
bool CEconItem::BIsKeyLess( const CSharedObject & soRHS ) const
{
	Assert( GetTypeID() == soRHS.GetTypeID() );
	const CEconItem & soSchemaRHS = (const CEconItem &)soRHS;

	return m_ulID < soSchemaRHS.m_ulID;
}

//----------------------------------------------------------------------------
// Purpose: Copy the data from the specified schema shared object into this. 
//			Both objects must be of the same type.
//----------------------------------------------------------------------------
void CEconItem::Copy( const CSharedObject & soRHS )
{
	*this = (const CEconItem &)soRHS;
}

//----------------------------------------------------------------------------
// Purpose: Dumps diagnostic information about the shared object
//----------------------------------------------------------------------------
void CEconItem::Dump() const
{
	CSOEconItem msgItem;
	SerializeToProtoBufItem( msgItem );
	CProtoBufSharedObjectBase::Dump( msgItem );
}


//-----------------------------------------------------------------------------
// Purpose: Deserializes an item from a KV object
// Input:	pKVItem - Pointer to the KV structure that represents an item
//			schema - Econ item schema used for decoding human readable names
//			pVecErrors - Pointer to a vector where human readable errors will
//				be added
// Output:	True if the item deserialized successfully, false otherwise
//-----------------------------------------------------------------------------
bool CEconItem::BDeserializeFromKV( KeyValues *pKVItem, const CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors )
{
	Assert( NULL != pKVItem );
	if ( NULL == pKVItem )
		return false;

	// The basic properties
	SetItemID( pKVItem->GetUint64( "ID", INVALID_ITEM_ID ) );
	SetInventoryToken( pKVItem->GetInt( "InventoryPos", GetUnacknowledgedPositionFor(UNACK_ITEM_DROPPED) ) );	// Start by assuming it's a drop
	SetQuantity( pKVItem->GetInt( "Quantity", 1 ) );

	// Look up the following properties based on names from the schema
	const CEconItemQualityDefinition *pQuality = NULL;
	const CEconItemDefinition *pItemDef = NULL;

	const char *pchDefName = pKVItem->GetString( "DefName" );
	pItemDef = pschema.GetItemDefinitionByName( pchDefName );
	if( !pItemDef )
	{
		if ( pVecErrors )
		{
			pVecErrors->AddToTail( CUtlString( CFmtStr( "Item definition \"%s\" not found", pchDefName ) ) );
		}

		// we can't do any reasonable validation with no item def, so just stop here
		return false;
	}

	SetDefinitionIndex( pItemDef->GetDefinitionIndex() );

	uint8 unValueGet = 0;

	const char *pchQualityName = pKVItem->GetString( "QualityName" );
	if( !pchQualityName || ! *pchQualityName )
	{
		// set the default quality for the definition
		if( pItemDef->GetQuality() == k_unItemQuality_Any )
		{
			if ( NULL == pVecErrors )
				return false;
			pVecErrors->AddToTail( CUtlString( CFmtStr( "Quality was not specified and this item def doesn't define one either." ) ) );
		}
		else
		{
			SetQuality( pItemDef->GetQuality() );
		}
	}
	else if ( !pschema.BGetItemQualityFromName( pchQualityName, &unValueGet ) || (( m_nQuality = unValueGet ),true) || k_unItemQuality_Any == GetQuality() )
	{
		if ( NULL == pVecErrors )
			return false;
		pVecErrors->AddToTail( CUtlString( CFmtStr( "Quality \"%s\" not found", pchQualityName ) ) );
	}
	else
	{
		pQuality = pschema.GetQualityDefinition( GetQuality() );
	}

	const char *pchRarityName = pKVItem->GetString( "RarityName" );
	if( !pchRarityName || ! *pchRarityName )
	{
		// set the default quality for the definition
		if( pItemDef->GetRarity() == k_unItemRarity_Any )
		{
			if ( NULL == pVecErrors )
				return false;
			pVecErrors->AddToTail( CUtlString( CFmtStr( "Rarity was not specified and this item def doesn't define one either." ) ) );
		}
		else
		{
			SetRarity( pItemDef->GetRarity() );
		}
	}
	else if ( !pschema.BGetItemRarityFromName( pchRarityName, &unValueGet ) || (( m_nRarity = unValueGet ),true) || k_unItemRarity_Any == GetRarity() )
	{
		if ( NULL == pVecErrors )
			return false;
		pVecErrors->AddToTail( CUtlString( CFmtStr( "Rarity \"%s\" not found", pchRarityName ) ) );
	}

	// make sure the level is sane
	SetItemLevel( pKVItem->GetInt( "Level", pItemDef->GetMinLevel() ) );

	// read the flags
	uint8 unFlags = GetFlags();
	if( pKVItem->GetInt( "flag_cannot_trade", 0 ) )
	{
		unFlags |= kEconItemFlag_CannotTrade;
	}
	else
	{
		unFlags = unFlags & ~kEconItemFlag_CannotTrade;
	}
	if( pKVItem->GetInt( "flag_cannot_craft", 0 ) )
	{
		unFlags |= kEconItemFlag_CannotBeUsedInCrafting;
	}
	else
	{
		unFlags = unFlags & ~kEconItemFlag_CannotBeUsedInCrafting;
	}
	if( pKVItem->GetInt( "flag_non_economy", 0 ) )
	{
		unFlags |= kEconItemFlag_NonEconomy;
	}
	else
	{
		unFlags = unFlags & ~kEconItemFlag_NonEconomy;
	}
	SetFlag( unFlags );

	// Deserialize the attributes
	KeyValues *pKVAttributes = pKVItem->FindKey( "Attributes" );
	if ( NULL != pKVAttributes )
	{
		FOR_EACH_SUBKEY( pKVAttributes, pKVAttr )
		{
			// Try to load each line into an attribute in memory. It's possible that if we fail to successfully
			// load some attribute contents here we'll leak small amounts of memory, but if that happens we're
			// going to fail to start up anyway so we don't really care.
			static_attrib_t staticAttrib;
			if ( !staticAttrib.BInitFromKV_SingleLine( __FUNCTION__, pKVAttr, pVecErrors ) )
				continue;

			const CEconItemAttributeDefinition *pAttrDef = staticAttrib.GetAttributeDefinition();
			Assert( pAttrDef );

			const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
			Assert( pAttrType );

			// Load the attribute contents into memory on the item.
			pAttrType->LoadEconAttributeValue( this, pAttrDef, staticAttrib.m_value );

			// Free up our temp loading memory.
			pAttrType->UnloadEconAttributeValue( &staticAttrib.m_value );
		}
	}

	return ( NULL == pVecErrors || 0 == pVecErrors->Count() );
}


// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
void CEconItem::SerializeToProtoBufItem( CSOEconItem &msgItem ) const
{
	msgItem.set_id( m_ulID );
	msgItem.set_account_id( m_unAccountID );
	msgItem.set_def_index( m_unDefIndex );
	msgItem.set_level( m_unLevel );
	msgItem.set_quality( m_nQuality );
	msgItem.set_rarity( m_nRarity );
	msgItem.set_inventory( m_unInventory );	
	msgItem.set_quantity( GetQuantity() );
	msgItem.set_flags( m_unFlags );
	msgItem.set_origin( m_unOrigin );
	msgItem.set_in_use( m_dirtybitInUse );

	
	if ( m_pCustomDataOptimizedObject )
	{
		//
		// Write our custom attributes
		//
		attribute_t const *pAttr = m_pCustomDataOptimizedObject->GetAttribute( 0 );
		attribute_t const *pAttrEnd = pAttr + m_pCustomDataOptimizedObject->m_numAttributes;
		for ( ; pAttr < pAttrEnd; ++pAttr )
		{
			const attribute_t & attr = *pAttr;

			// skip over attributes we don't understand
			const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( attr.m_unDefinitionIndex );
			if ( !pAttrDef )
				continue;

			const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
			Assert( pAttrType );

			CSOEconItemAttribute *pMsgAttr = msgItem.add_attribute();
			pMsgAttr->set_def_index( attr.m_unDefinitionIndex );

			pAttrType->ConvertEconAttributeValueToByteStream( attr.m_value, pMsgAttr->mutable_value_bytes() );
		}

		//
		// Write equipped instances
		//
		if ( m_pCustomDataOptimizedObject->m_equipInstanceSlot1 != INVALID_EQUIPPED_SLOT_BITPACKED )
		{
			CSOEconItemEquipped *pMsgEquipped = msgItem.add_equipped_state();
			pMsgEquipped->set_new_class( m_pCustomDataOptimizedObject->m_equipInstanceClass1 );
			pMsgEquipped->set_new_slot( m_pCustomDataOptimizedObject->m_equipInstanceSlot1 );

			if ( m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit && ( m_pCustomDataOptimizedObject->m_equipInstanceClass1 == 2 ) )
			{
				pMsgEquipped = msgItem.add_equipped_state();
				pMsgEquipped->set_new_class( 3 );
				pMsgEquipped->set_new_slot( m_pCustomDataOptimizedObject->m_equipInstanceSlot1 );
			}
		}
	}

	#ifndef GC_DLL	// no need to send original IDs outside of GC
	if ( m_ulID != GetOriginalID() )
		msgItem.set_original_id( GetOriginalID() );
	#endif

#if 0
	// Names and descriptions are now attributes, no need to duplicate them here (perf)
	const char *pszCustomName = GetCustomName();
	if ( pszCustomName )
	{
		msgItem.set_custom_name( pszCustomName );
	}

	const char *pszCustomDesc = GetCustomDesc();
	if ( pszCustomDesc )
	{
		msgItem.set_custom_desc( pszCustomDesc );
	}
#endif
}

// --------------------------------------------------------------------------
// Purpose: 
// --------------------------------------------------------------------------
void CEconItem::DeserializeFromProtoBufItem( const CSOEconItem &msgItem )
{
	VPROF_BUDGET( "CEconItem::DeserializeFromProtoBufItem()", VPROF_BUDGETGROUP_STEAM );

	// Start by resetting
	if ( m_pCustomDataOptimizedObject )
	{
		m_pCustomDataOptimizedObject->FreeObjectAndAttrMemory();
		m_pCustomDataOptimizedObject = NULL;
	}

	// Now copy from the message
	m_ulID = msgItem.id();
	SetOriginalID( msgItem.has_original_id() ? msgItem.original_id() : m_ulID );
	m_unAccountID = msgItem.account_id();
	m_unDefIndex = msgItem.def_index();
	m_nQuality = msgItem.quality();
	m_nRarity = msgItem.rarity();
	m_unInventory = msgItem.inventory();
	m_unFlags = msgItem.flags();
	m_unOrigin = msgItem.origin();

	//the default value of these fields will be switched from zero to 1 since almost all items have 1 for both of these. However, to 
	//ensure a smooth transition, we ensure that the values are always 1 (zero is invalid for these). This can be removed after a few
	//versions and the GC/client/server are all in sync
	m_unLevel = MAX( 1, msgItem.level() );
	SetQuantity( MAX( 1, msgItem.quantity() ) );

	m_dirtybitInUse = msgItem.in_use() ? 1 : 0;

	// set name if any
	if( msgItem.has_custom_name() )
	{
		SetCustomName( msgItem.custom_name().c_str() );
	}

	// set desc if any
	if( msgItem.has_custom_desc() )
	{
		SetCustomDesc( msgItem.custom_desc().c_str() );
	}

	// read the attributes
	for( int nAttr = 0; nAttr < msgItem.attribute_size(); nAttr++ )
	{
		// skip over old-format messages
		const CSOEconItemAttribute& msgAttr = msgItem.attribute( nAttr );
		if ( msgAttr.has_value() || !msgAttr.has_value_bytes() )
			continue;

		// skip over attributes we don't understand
		const CEconItemAttributeDefinition *pAttrDef = GetItemSchema()->GetAttributeDefinition( msgAttr.def_index() );
		if ( !pAttrDef )
			continue;

		const ISchemaAttributeType *pAttrType = pAttrDef->GetAttributeType();
		Assert( pAttrType );

		pAttrType->LoadByteStreamToEconAttributeValue( this, pAttrDef, msgAttr.value_bytes() );
	}

	// Check to see if the item has an interior object.
	Assert( !msgItem.has_interior_item() );

	// update equipped state
	for ( int i = 0; i < msgItem.equipped_state_size(); i++ )
	{
		UpdateEquippedState( msgItem.equipped_state(i).new_class(), msgItem.equipped_state(i).new_slot() );
	}
}


int CEconItem::GetEquippedInstanceArray( EquippedInstanceArray_t &equips ) const
{
	//build up a list of the equip instances we need to copy over, since as we equip new items into that slot, it will unequip these other items
	equips.RemoveAll();

	if ( m_pCustomDataOptimizedObject && ( m_pCustomDataOptimizedObject->m_equipInstanceSlot1 != INVALID_EQUIPPED_SLOT_BITPACKED ) )
	{
		EquippedInstance_t instEquip( m_pCustomDataOptimizedObject->m_equipInstanceClass1, m_pCustomDataOptimizedObject->m_equipInstanceSlot1 );
		equips.AddToTail( instEquip );

		if ( m_pCustomDataOptimizedObject->m_equipInstanceClass2Bit && ( m_pCustomDataOptimizedObject->m_equipInstanceClass1 == 2 ) )
		{
			instEquip.m_unEquippedClass = 3;
			equips.AddToTail( instEquip );
		}
	}

	return equips.Count();
}
