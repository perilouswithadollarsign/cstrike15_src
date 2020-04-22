//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: CEconItem, a shared object for econ items
//
//=============================================================================

#ifndef ECONITEM_H
#define ECONITEM_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/gcclientsdk.h"
#include "base_gcmessages.pb.h"

#include "econ_item_constants.h"
#include "econ_item_interface.h"
#include "econ_item_schema.h"

#include <typeinfo>			// needed for typeid()

#define ENABLE_TYPED_ATTRIBUTE_PARANOIA		1

namespace GCSDK
{
	class CColumnSet;
};

class CEconItem;
class CSOEconItem;
class CEconItemCustomData;
class CEconSessionItemAudit;
//-----------------------------------------------------------------------------
// Stats tracking for the attributes attached to CEconItem instances.
//-----------------------------------------------------------------------------
struct schema_attribute_stat_bucket_t
{
	const schema_attribute_stat_bucket_t *m_pNext;

	const char *m_pszDesc;
	uint64 m_unLiveInlineCount;
	uint64 m_unLifetimeInlineCount;
	uint64 m_unLiveHeapCount;
	uint64 m_unLifetimeHeapCount;

	void OnAllocateInlineInstance() { m_unLiveInlineCount++; m_unLifetimeInlineCount++; }
	void OnFreeInlineInstance() { /* temporarily disabling to avoid spew until the rest of the minor attribute changelists are brought from TF to Dota: -> Assert( m_unLiveInlineCount > 0 ); */ m_unLiveInlineCount--; }
	void OnAllocateHeapInstance() { m_unLiveHeapCount++; m_unLifetimeHeapCount++; }
	void OnFreeHeapInstance() { Assert( m_unLiveHeapCount ); m_unLiveHeapCount--; }
};

class CSchemaAttributeStats
{
public:
	template < typename TAttribStatsStorageClass, typename TAttribInMemoryType >
	static void RegisterAttributeType()
	{
		TAttribStatsStorageClass::s_InstanceStats.m_pszDesc = typeid( TAttribInMemoryType ).name();
		TAttribStatsStorageClass::s_InstanceStats.m_pNext = m_pHead;

		m_pHead = &TAttribStatsStorageClass::s_InstanceStats;
	}

	static const schema_attribute_stat_bucket_t *GetFirstStatBucket()
	{
		return m_pHead;
	}

private:
	static const schema_attribute_stat_bucket_t *m_pHead;
};

//-----------------------------------------------------------------------------
// Base class interface for attributes of a certain in-memory type.
//-----------------------------------------------------------------------------

template < typename T >
unsigned int GetAttributeTypeUniqueIdentifier();

template < typename TAttribInMemoryType >
class ISchemaAttributeTypeBase : public ISchemaAttributeType
{
	friend class CSchemaAttributeStats;

public:
	ISchemaAttributeTypeBase()
	{
		CSchemaAttributeStats::RegisterAttributeType< ISchemaAttributeTypeBase<TAttribInMemoryType>, TAttribInMemoryType >();
	}

	virtual void LoadEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const union attribute_data_union_t& value ) const OVERRIDE;

	// Returns a unique identifier per run based on the type of <TAttribInMemoryType>.
	virtual unsigned int GetTypeUniqueIdentifier() const OVERRIDE
	{
		return GetAttributeTypeUniqueIdentifier<TAttribInMemoryType>();
	}

	// ...
	void ConvertTypedValueToEconAttributeValue( const TAttribInMemoryType& typedValue, attribute_data_union_t *out_pValue ) const
	{
		// If our type is smaller than an int, we don't know how to copy the memory into our flat structure. We could write
		// this code but we have no use case for it now so this is set up to fail so if someone does come up with a use case
		// they know where to fix.
		COMPILE_TIME_ASSERT( sizeof( TAttribInMemoryType ) >= sizeof( uint32 ) );

		// Do we fit in the bottom 32-bits?
		if ( sizeof( TAttribInMemoryType ) <= sizeof( uint32 ) )
		{
			*reinterpret_cast<TAttribInMemoryType *>( &out_pValue->asUint32 ) = typedValue;
		}
		// What about in the full 64-bits (if we're running a 64-bit build)?
		else if ( sizeof( TAttribInMemoryType ) <= sizeof( void * ) )
		{
			*reinterpret_cast<TAttribInMemoryType *>( &out_pValue->asBlobPointer ) = typedValue;
		}
		// We're too big for our flat structure. We need to allocate space somewhere outside our attribute instance and point
		// to that.
		else
		{
			if ( !out_pValue->asBlobPointer )
			{
				InitializeNewEconAttributeValue( out_pValue );
			}
			
			*reinterpret_cast<TAttribInMemoryType *>( out_pValue->asBlobPointer ) = typedValue;
		}
	}

	// Guaranteed to return a valid reference (or assert/crash if calling code is behaving inappropriately and calling
	// this before an attribute value is allocated/set).
	const TAttribInMemoryType& GetTypedValueContentsFromEconAttributeValue( const attribute_data_union_t& value ) const
	{
		COMPILE_TIME_ASSERT( sizeof( TAttribInMemoryType ) >= sizeof( uint32 ) );

		// Do we fit in the bottom 32-bits?
		if ( sizeof( TAttribInMemoryType ) <= sizeof( uint32 ) )
			return *reinterpret_cast<const TAttribInMemoryType *>( &value.asUint32 );
		
		// What about in the full 64-bits (if we're running a 64-bit build)?
		if ( sizeof( TAttribInMemoryType ) <= sizeof( void * ) )
			return *reinterpret_cast<const TAttribInMemoryType *>( &value.asBlobPointer );

		// We don't expect to get to a "read value" call without having written a value, which would
		// have allocated this memory.
		Assert( value.asBlobPointer );

		return *reinterpret_cast<const TAttribInMemoryType *>( value.asBlobPointer );
	}

	void ConvertEconAttributeValueToTypedValue( const attribute_data_union_t& value, TAttribInMemoryType *out_pTypedValue ) const
	{
		Assert( out_pTypedValue );

		*out_pTypedValue = GetTypedValueContentsFromEconAttributeValue( value );
	}

	void InitializeNewEconAttributeValue( attribute_data_union_t *out_pValue ) const OVERRIDE
	{
		if ( sizeof( TAttribInMemoryType ) <= sizeof( uint32 ) )
		{
			new( &out_pValue->asUint32 ) TAttribInMemoryType;
			s_InstanceStats.OnAllocateInlineInstance();
		}
		else if ( sizeof( TAttribInMemoryType ) <= sizeof( void * ) )
		{
			new( &out_pValue->asBlobPointer ) TAttribInMemoryType;
			s_InstanceStats.OnAllocateInlineInstance();
		}
		else
		{
			out_pValue->asBlobPointer = reinterpret_cast<byte *>( new TAttribInMemoryType );
			s_InstanceStats.OnAllocateHeapInstance();
		}
	}

	virtual void UnloadEconAttributeValue( attribute_data_union_t *out_pValue ) const OVERRIDE
	{
		COMPILE_TIME_ASSERT( sizeof( TAttribInMemoryType ) >= sizeof( uint32 ) );

		// For smaller types, anything that fits inside the bits of a void pointer, we store the contents
		// inline and only have to worry about calling the correct destructor. We check against the small-/
		// size/medium-size values separately to not worry about which bits we're storing the uint32 in.
		if ( sizeof( TAttribInMemoryType ) <= sizeof( uint32 ) )
		{
			(reinterpret_cast<TAttribInMemoryType *>( &out_pValue->asUint32 ))->~TAttribInMemoryType();
			s_InstanceStats.OnFreeInlineInstance();
		}
		else if ( sizeof( TAttribInMemoryType ) <= sizeof( void * ) )
		{
			(reinterpret_cast<TAttribInMemoryType *>( &out_pValue->asBlobPointer ))->~TAttribInMemoryType();
			s_InstanceStats.OnFreeInlineInstance();
		}
		// For larger types, we have the memory stored on the heap somewhere. We don't have to manually
		// destruct, but we do have to manually free.
		else
		{
			if ( !out_pValue->asBlobPointer )
				return;

			delete reinterpret_cast<TAttribInMemoryType *>( out_pValue->asBlobPointer );
			s_InstanceStats.OnFreeHeapInstance();
		}
	}

	virtual bool OnIterateAttributeValue( IEconItemAttributeIterator *pIterator, const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value ) const OVERRIDE
	{
		Assert( pIterator );
		Assert( pAttrDef );

		// Call the appropriate virtual function on our iterator based on whatever type we represent.
		return pIterator->OnIterateAttributeValue( pAttrDef, GetTypedValueContentsFromEconAttributeValue( value ) );
	}

	virtual void LoadByteStreamToEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const std::string& sBytes ) const OVERRIDE;
	virtual void ConvertEconAttributeValueToByteStream( const attribute_data_union_t& value, ::std::string *out_psBytes ) const;

	virtual void ConvertTypedValueToByteStream( const TAttribInMemoryType& typedValue, ::std::string *out_psBytes ) const = 0;
	virtual void ConvertByteStreamToTypedValue( const ::std::string& sBytes, TAttribInMemoryType *out_pTypedValue ) const = 0;

private:
	static schema_attribute_stat_bucket_t s_InstanceStats;
};

template < typename TAttribInMemoryType >
schema_attribute_stat_bucket_t ISchemaAttributeTypeBase<TAttribInMemoryType>::s_InstanceStats;

class CEconItem : public GCSDK::CSharedObject, public IEconItemInterface
{
public:
	typedef GCSDK::CSharedObject BaseClass;

	struct attribute_t
	{
		attrib_definition_index_t m_unDefinitionIndex;			// stored as ints here for memory efficiency on the GC
		attribute_data_union_t m_value;

	private:
		void operator=( const attribute_t& rhs );
	};

	struct EquippedInstance_t
	{
		EquippedInstance_t() : m_unEquippedClass( 0 ), m_unEquippedSlot( INVALID_EQUIPPED_SLOT ) {}
		EquippedInstance_t( equipped_class_t unClass, equipped_slot_t unSlot ) : m_unEquippedClass( unClass ), m_unEquippedSlot( unSlot ) {}
		equipped_class_t m_unEquippedClass;
		equipped_slot_t m_unEquippedSlot;
	};

	struct CustomDataOptimizedObject_t
	{
		// This is preceding the attributes block, attributes tightly packed and follow immediately
		// 25% of CS:GO items have no attributes and don't allocate anything here
		// one attribute case = [uint16] + [uint16+uint32] = 8 bytes (SBH bucket 8)
		// 2 attributes case = [uint16]+2x[uint16+uint32] = 14 bytes (SBH bucket 16)
		// 3 attributes case = [uint16]+3x[uint16+uint32] = 20 bytes (SBH bucket 24)  (60% of CS:GO items)
		// 4 attributes case = [uint16]+4x[uint16+uint32] = 26 bytes (SBH bucket 32)
		// 5 attributes case = [uint16]+4x[uint16+uint32] = 26 bytes (SBH bucket 32)

		uint16 m_equipInstanceSlot1 : 6;			// Equip Instance Slot									#6 // if equipped
		uint16 m_equipInstanceClass1 : 3;			// Equip Instance Class									#9
		uint16 m_equipInstanceClass2Bit : 1;		// Whether the item is equipped for complementary class	#10
		uint16 m_numAttributes : 6;					// Length of following attributes						#16

		// 32-bit GC size = 2 bytes!

		inline attribute_t * GetAttribute( uint32 j ) { return ( reinterpret_cast< attribute_t * >( this + 1 ) ) + j ; }
		inline const attribute_t * GetAttribute( uint32 j ) const { return ( reinterpret_cast< const attribute_t * >( this + 1 ) ) + j ; }

	private:	// forbid all operations including constructor/destructor
		CustomDataOptimizedObject_t();
		CustomDataOptimizedObject_t( const CustomDataOptimizedObject_t & );
		~CustomDataOptimizedObject_t();
		CustomDataOptimizedObject_t & operator=( const CustomDataOptimizedObject_t & );

	public:
		static CustomDataOptimizedObject_t *Alloc( uint32 numAttributes );			// allocates enough memory to include attributes
		static attribute_t * AddAttribute( CustomDataOptimizedObject_t * &rptr );	// reallocs the ptr
		
		void RemoveAndFreeAttrMemory( uint32 idxAttributeInArray );					// keeps the memory allocated
		void FreeObjectAndAttrMemory();
	};

	// Set only the top 16 bits for field ID types! These will be or'd into the index of
	// the field itself and then pulled apart later.
	enum
	{
		kUpdateFieldIDType_FieldID				= 0x00000000,	// this must stay as 0 for legacy code
		kUpdateFieldIDType_AttributeID			= 0x00010000,	// this will modify existing attribute in the database
	};

	const static int k_nTypeID = k_EEconTypeItem;
	virtual int GetTypeID() const { return k_nTypeID; }

	CEconItem();
	virtual ~CEconItem();

private:
	CEconItem( const CEconItem &copy ); // no impl - don't want to allow accidental pass-by-value

public:
	CEconItem &operator=( const CEconItem& rhs );

	// IEconItemInterface interface.
	const GameItemDefinition_t *GetItemDefinition() const;
public:

#ifndef GC_DLL
	virtual void SetSOUpdateFrame( int nNewValue ) const { m_bSOUpdateFrame = nNewValue; }
	virtual int GetSOUpdateFrame( void ) const           { return m_bSOUpdateFrame; }
#endif

	virtual void IterateAttributes( class IEconItemAttributeIterator *pIterator ) const;

	// Accessors/Settors
	virtual itemid_t GetItemID() const { return m_ulID; }
	void SetItemID( uint64 ulID );

	itemid_t GetOriginalID() const;
	void SetOriginalID( uint64 ulOriginalID );

	uint32 GetAccountID() const { return m_unAccountID; }
	void SetAccountID( uint32 unAccountID ) { m_unAccountID = unAccountID; }

	item_definition_index_t GetDefinitionIndex() const { return m_unDefIndex; }
	void SetDefinitionIndex( uint32 unDefinitionIndex ) { m_unDefIndex = unDefinitionIndex; }

	uint32 GetItemLevel() const { return m_unLevel; }
	void SetItemLevel( uint32 unItemLevel ) { m_unLevel = unItemLevel; }

	int32 GetQuality() const { return m_nQuality; }
	void SetQuality( int32 nQuality ) { m_nQuality = nQuality; }

	int32 GetRarity() const;
	void SetRarity( int32 nRarity ) { m_nRarity = nRarity; }

	uint32 GetInventoryToken() const { return m_unInventory; }
	void SetInventoryToken( uint32 unToken ) { m_unInventory = unToken; }

	uint16 GetQuantity() const;
	void SetQuantity( uint16 unQuantity );

	uint8 GetFlags() const { return m_unFlags; }
	void SetFlags( uint8 unFlags ) { m_unFlags = unFlags; }

	void SetFlag( uint8 unFlag ) { m_unFlags |= unFlag; }
	void ClearFlag( uint8 unFlag ) { m_unFlags &= ~unFlag; }
	bool CheckFlags( uint8 unFlags ) const { return ( m_unFlags & unFlags ) != 0; } 

	eEconItemOrigin GetOrigin() const { return (eEconItemOrigin)m_unOrigin; }
	void SetOrigin( eEconItemOrigin unOrigin ) { m_unOrigin = unOrigin; Assert( m_unOrigin == unOrigin ); }
	bool IsForeign() const { return m_unOrigin == kEconItemOrigin_Foreign; }

	const char *GetCustomName() const;
	void SetCustomName( const char *pName );

	const char *GetCustomDesc() const;
	void SetCustomDesc( const char *pDesc );

	virtual int GetItemSetIndex() const;

	void InitAttributesDroppedFromListEntry( item_list_entry_t const *pEntryInfo );

	bool IsEquipped() const;
	bool IsEquippedForClass( equipped_class_t unClass ) const;
	equipped_slot_t GetEquippedPositionForClass( equipped_class_t unClass ) const;
	void UpdateEquippedState( equipped_class_t unClass, equipped_slot_t unSlot ) { UpdateEquippedState( EquippedInstance_t( unClass, unSlot ) ); }
	void UpdateEquippedState( EquippedInstance_t equipInstance );

	virtual bool GetInUse() const;
	void SetInUse( bool bInUse );

	bool IsTradable() const;
	bool IsMarketable() const;
	bool IsUsableInCrafting() const;

	// --------------------------------------------------------------------------------------------
	// Typed attributes. These are methods for accessing and setting values of attributes with
	// some semblance of type information and type safety.
	// --------------------------------------------------------------------------------------------

	// Assign the value of the attribute [pAttrDef] to [value]. Passing in a type for [value] that
	// doesn't match the storage type specified by the attribute definition will fail asserts a bunch
	// of asserts all the way down the stack and may or may not crash -- it would be nice to make this
	// fail asserts at compile time.
	//
	// This function has undefined results (besides asserting) if called to add a dynamic version of
	// an attrib that's already specified statically.
	template < typename T >
	void SetDynamicAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const T& value )
	{
		Assert( pAttrDef );

		const ISchemaAttributeTypeBase<T> *pAttrType = GetTypedAttributeType<T>( pAttrDef );
		Assert( pAttrType );
		if ( !pAttrType )
			return;
		
		// Fail right off the bat if we're trying to write a dynamic attribute value for an item that already
		// has this as a static value.

		/*
		AssertMsg( !::FindAttribute( GetItemDefinition(), pAttrDef ),
				   "Item id %llu (%s) attempting to set dynamic attribute value for '%s' (%d) when static attribute exists!",
				   GetItemID(), GetItemDefinition()->GetDefinitionName(), pAttrDef->GetDefinitionName(), pAttrDef->GetDefinitionIndex() );
				   */
		
		// Alright, we have a data type match so we can safely store data. Some types may need to initialize
		// their data to a current state if it's the first time we're writing to this value (as opposed to
		// updating an existing value).
		attribute_t *pEconAttrib = FindDynamicAttributeInternal( pAttrDef );

		if ( !pEconAttrib )
		{
			pEconAttrib = &(AddDynamicAttributeInternal());
			pEconAttrib->m_unDefinitionIndex = pAttrDef->GetDefinitionIndex();
			pAttrType->InitializeNewEconAttributeValue( &pEconAttrib->m_value );
		}

		pAttrType->ConvertTypedValueToEconAttributeValue( value, &pEconAttrib->m_value );

#if ENABLE_TYPED_ATTRIBUTE_PARANOIA
		// Paranoia!: make sure that our read/write functions are mirrored correctly, and that if we attempt
		// to read back a value we get something identical to what we just wrote. We do this via converting
		// to strings and then comparing those because there may or not be equality comparisons for our type
		// T that make sense (ie., protobufs).
		{
			T readValue;
			DbgVerify( FindAttribute( pAttrDef, &readValue ) );

			std::string sBytes, sReadBytes;
			pAttrType->ConvertTypedValueToByteStream( value, &sBytes );
			pAttrType->ConvertTypedValueToByteStream( readValue, &sReadBytes );
			AssertMsg1( sBytes == sReadBytes, "SetDynamicAttributeValue(): read/write mismatch for attribute '%s'.", pAttrDef->GetDefinitionName() );
		}
#endif // ENABLE_TYPED_ATTRIBUTE_PARANOIA
	}

	// Remove an instance of an attribute from this item. This will also free any dynamic memory associated
	// with that instance if any was allocated.
	void RemoveDynamicAttribute( const CEconItemAttributeDefinition *pAttrDef );

	// Copy all attributes and values in a type-safe way from [source] to ourself. Attributes that we have
	// that don't exist on [source] will maintain their current values. All other attributes will get their
	// values set to whatever [source] specifies.
	void CopyAttributesFrom( const CEconItem& source );

	// Copy everything about the item without the attributes
	void CopyWithoutAttributesFrom( const CEconItem& source );

private:
	template < typename T >
	static const ISchemaAttributeTypeBase<T> *GetTypedAttributeType( const CEconItemAttributeDefinition *pAttrDef )
	{
		// Make sure the type of data we're passing in matches the type of data we're claiming that we can
		// store in the attribute definition.
		const ISchemaAttributeType *pIAttr = pAttrDef->GetAttributeType();
		Assert( pIAttr );
		Assert( pIAttr->GetTypeUniqueIdentifier() == GetAttributeTypeUniqueIdentifier<T>() );

#if ENABLE_TYPED_ATTRIBUTE_PARANOIA
		return dynamic_cast<const ISchemaAttributeTypeBase<T> *>( pIAttr );
#else
		return static_cast<const ISchemaAttributeTypeBase<T> *>( pIAttr );
#endif
	}

public:
	void AddCustomAttribute( uint16 usDefinitionIndex, float flValue );
	void AddOrSetCustomAttribute( uint16 usDefinitionIndex, float flValue );
	
	bool BDeserializeFromKV( KeyValues *pKVItem, const CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );


	virtual bool BParseFromMessage( const CUtlBuffer &buffer ) OVERRIDE;
	virtual bool BParseFromMessage( const std::string &buffer ) OVERRIDE;
	virtual bool BUpdateFromNetwork( const CSharedObject & objUpdate ) OVERRIDE;

	virtual bool BAddToMessage( std::string *pBuffer ) const OVERRIDE; // short cut to remove an extra copy
	virtual bool BAddDestroyToMessage( std::string *pBuffer ) const OVERRIDE;

	virtual bool BIsKeyLess( const CSharedObject & soRHS ) const ;
	virtual void Copy( const CSharedObject & soRHS );
	virtual void Dump() const;

	void SerializeToProtoBufItem( CSOEconItem &msgItem ) const;
	void DeserializeFromProtoBufItem( const CSOEconItem &msgItem );

	typedef CUtlVectorFixed< CEconItem::EquippedInstance_t, 2 > EquippedInstanceArray_t;
	int GetEquippedInstanceArray( EquippedInstanceArray_t &equips ) const;

protected:
	// CSharedObject
	// adapted from CSchemaSharedObject
	void GetDirtyColumnSet( const CUtlVector< int > &fields, GCSDK::CColumnSet &cs ) const;
	void OnTransferredOwnership();

	// Internal attribute interface.
	friend class CWebAPIStringExporterAttributeIterator;

	attribute_t&	   AddDynamicAttributeInternal();													// add another chunk of data to our internal storage to store a new attribute -- initialization is the responsibility of the caller
	attribute_t		  *FindDynamicAttributeInternal( const CEconItemAttributeDefinition *pAttrDef );	// search for an instance of a dynamic attribute with this definition -- ignores static properties, etc. and will return NULL if not found
	int				   GetDynamicAttributeCountInternal() const;										// how many attributes are there attached to this instance?
	const attribute_t & GetDynamicAttributeInternal( int iAttrIndexIntoArray ) const;


public:
	//
	// GC object data breakdown (32-bit build)
	//
	// already starting with TWO VTBLS due to multiple inheritance :(
	// 8 bytes spent on VTBLS
	//

	// data that is most commonly changed
	uint64 m_ulID;							// Item ID
	uint64 m_ulOriginalID;					// Original ID is often different from item ID itself
	
	// optional data (custom name, additional attributes, etc.)
	CustomDataOptimizedObject_t *m_pCustomDataOptimizedObject;
	
	uint32 m_unAccountID;					// Item Owner
	uint32 m_unInventory;					// App managed int representing inventory placement

	// === + 7*4 = 36 bytes here already, uint16 packing below ===

	item_definition_index_t m_unDefIndex;	// Item definition index							+16

	uint16 m_unOrigin : 5;					// Origin (eEconItemOrigin) (0-31)					#5
	uint16 m_nQuality : 4;					// Item quality (0-14)								#9
	uint16 m_unLevel:2;						// Item Level (0-1-2-3)								#11
	uint16 m_nRarity:4;						// Item rarity	(0-14)								#15
	uint16 m_dirtybitInUse:1;				// In Use											#16
protected:
	mutable int16 m_iItemSet;				// packs it tightly with defindex field
	mutable int  m_bSOUpdateFrame;
public:
	uint8 m_unFlags;						// Flags
	// + 6 bytes = 42 bytes total (in 32-bit build)

	static void FreeAttributeMemory( CEconItem::attribute_t *pAttrib );
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template < typename TAttribInMemoryType >
/*virtual*/ void ISchemaAttributeTypeBase<TAttribInMemoryType>::LoadByteStreamToEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const std::string& sBytes ) const
{
	Assert( pTargetItem );
	Assert( pAttrDef );

	TAttribInMemoryType typedValue;
	ConvertByteStreamToTypedValue( sBytes, &typedValue );

	pTargetItem->SetDynamicAttributeValue( pAttrDef, typedValue );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
template < typename TAttribInMemoryType >
/*virtual*/ void ISchemaAttributeTypeBase<TAttribInMemoryType>::ConvertEconAttributeValueToByteStream( const attribute_data_union_t& value, ::std::string *out_psBytes ) const
{
	ConvertTypedValueToByteStream( GetTypedValueContentsFromEconAttributeValue( value ), out_psBytes );
}

bool ItemsMatch( CEconItemView *pCurItem, CEconItemView *pNewItem );

class CEconDefaultEquippedDefinitionInstanceClient : public GCSDK::CProtoBufSharedObject< CSOEconDefaultEquippedDefinitionInstanceClient, k_EEconTypeDefaultEquippedDefinitionInstanceClient >
{
public:
	const static int k_nTypeID = k_EEconTypeDefaultEquippedDefinitionInstanceClient;
	virtual int GetTypeID() const { return k_nTypeID; }
};

void YieldingAddAuditRecord( GCSDK::CSQLAccess *sqlAccess, const CEconItem *pItem, uint32 unOwnerID, EItemAction eAction, uint32 unData, CEconSessionItemAudit* pItemAudit = NULL );
void YieldingAddAuditRecord( GCSDK::CSQLAccess *sqlAccess, uint64 ulItemID, uint32 unOwnerID, EItemAction eAction, uint32 unData, CEconSessionItemAudit* pItemAudit = NULL );
void YieldingAddAttributeChangeAuditRecord( GCSDK::CSQLAccess *sqlAccess, uint64 ulItemID, uint32 unOwnerID, uint16 unAttributeDefIndex, uint32 uiOriginalValue, EItemAction eAction, uint32 unData );
bool YieldingAddItemToDatabase( CEconItem *pItem, const CSteamID & steamID, EItemAction eAction, uint32 unData, CEconSessionItemAudit* pItemAudit = NULL );

template < typename TAttribInMemoryType >
void ISchemaAttributeTypeBase<TAttribInMemoryType>::LoadEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const union attribute_data_union_t& value ) const
{
	pTargetItem->SetDynamicAttributeValue( pAttrDef, GetTypedValueContentsFromEconAttributeValue( value ) );
}

#endif // ECONITEM_H
