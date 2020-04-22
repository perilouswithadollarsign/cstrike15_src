//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#include "attribute_manager.h"
#include "saverestore.h"
#include "saverestore_utlvector.h"
#include "fmtstr.h"
#include "keyvalues.h"
#include "econ_item_system.h"

#define PROVIDER_PARITY_BITS		6
#define PROVIDER_PARITY_MASK		((1<<PROVIDER_PARITY_BITS)-1)

//==================================================================================================================
// ATTRIBUTE MANAGER SAVE/LOAD & NETWORKING
//===================================================================================================================
BEGIN_DATADESC_NO_BASE( CAttributeManager )
	DEFINE_UTLVECTOR( m_Providers,			FIELD_EHANDLE ),
	DEFINE_FIELD( m_iReapplyProvisionParity,		FIELD_INTEGER ),
	DEFINE_FIELD( m_hOuter,					FIELD_EHANDLE ),
	// DEFINE_FIELD( m_bPreventLoopback,	FIELD_BOOLEAN ),		// Don't need to save
	DEFINE_FIELD( m_ProviderType,			FIELD_INTEGER ),
END_DATADESC()

BEGIN_DATADESC( CAttributeContainer )
	DEFINE_EMBEDDED( m_Item ),
END_DATADESC()

#if defined( TF_DLL ) || defined( CSTRIKE_DLL )
BEGIN_DATADESC( CAttributeContainerPlayer )
END_DATADESC()
#endif

#ifndef CLIENT_DLL
EXTERN_SEND_TABLE( DT_ScriptCreatedItem );
#else
EXTERN_RECV_TABLE( DT_ScriptCreatedItem );
#endif

BEGIN_NETWORK_TABLE_NOBASE( CAttributeManager, DT_AttributeManager )
#ifndef CLIENT_DLL
	SendPropEHandle( SENDINFO(m_hOuter) ),
	SendPropInt( SENDINFO(m_ProviderType), 4, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_iReapplyProvisionParity), PROVIDER_PARITY_BITS, SPROP_UNSIGNED ),
#else
	RecvPropEHandle( RECVINFO(m_hOuter) ),
	RecvPropInt( RECVINFO(m_ProviderType) ),
	RecvPropInt( RECVINFO(m_iReapplyProvisionParity) ),
#endif
END_NETWORK_TABLE()

BEGIN_NETWORK_TABLE_NOBASE( CAttributeContainer, DT_AttributeContainer )
#ifndef CLIENT_DLL
	SendPropEHandle( SENDINFO(m_hOuter) ),
	SendPropInt( SENDINFO(m_ProviderType), 4, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_iReapplyProvisionParity), PROVIDER_PARITY_BITS, SPROP_UNSIGNED ),
	SendPropDataTable(SENDINFO_DT(m_Item), &REFERENCE_SEND_TABLE(DT_ScriptCreatedItem)),
#else
	RecvPropEHandle( RECVINFO(m_hOuter) ),
	RecvPropInt( RECVINFO(m_ProviderType) ),
	RecvPropInt( RECVINFO(m_iReapplyProvisionParity) ),
	RecvPropDataTable(RECVINFO_DT(m_Item), 0, &REFERENCE_RECV_TABLE(DT_ScriptCreatedItem)),
#endif
END_NETWORK_TABLE()

#if defined( TF_DLL ) || defined( CSTRIKE_DLL )
BEGIN_NETWORK_TABLE_NOBASE( CAttributeContainerPlayer, DT_AttributeContainerPlayer )
#ifndef CLIENT_DLL
	SendPropEHandle( SENDINFO(m_hOuter) ),
	SendPropInt( SENDINFO(m_ProviderType), 4, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_iReapplyProvisionParity), PROVIDER_PARITY_BITS, SPROP_UNSIGNED ),
	SendPropEHandle( SENDINFO(m_hPlayer) ),
#else
	RecvPropEHandle( RECVINFO(m_hOuter) ),
	RecvPropInt( RECVINFO(m_ProviderType) ),
	RecvPropInt( RECVINFO(m_iReapplyProvisionParity) ),
	RecvPropEHandle( RECVINFO( m_hPlayer ) ),
#endif
END_NETWORK_TABLE()
#endif

template< class T > T AttributeConvertFromFloat( float flValue )
{
	return static_cast<T>( flValue );
}

template<> float AttributeConvertFromFloat<float>( float flValue )
{
	return flValue;
}

template<> int AttributeConvertFromFloat<int>( float flValue )
{
	return RoundFloatToInt( flValue );
}

//-----------------------------------------------------------------------------
// All fields in the object are all initialized to 0.
//-----------------------------------------------------------------------------
void *CAttributeManager::operator new( size_t stAllocateBlock )
{
	ASSERT_MEMALLOC_WILL_ALIGN( CAttributeManager );
	void *pMem = MemAlloc_Alloc( stAllocateBlock );
	memset( pMem, 0, stAllocateBlock );
	return pMem;
};

void *CAttributeManager::operator new( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine )
{
	ASSERT_MEMALLOC_WILL_ALIGN( CAttributeManager );
	void *pMem = MemAlloc_Alloc( stAllocateBlock, pFileName, nLine );
	memset( pMem, 0, stAllocateBlock );
	return pMem;
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeManager::OnPreDataChanged( DataUpdateType_t updateType )
{
	m_iOldReapplyProvisionParity = m_iReapplyProvisionParity;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeManager::OnDataChanged( DataUpdateType_t updateType )
{
	if ( m_iReapplyProvisionParity != m_iOldReapplyProvisionParity )
	{
		// We've changed who we're providing to in some way. Reapply it.
		IHasAttributes *pAttribInterface = dynamic_cast<IHasAttributes *>( GetOuter() );
		if ( pAttribInterface )
		{
			pAttribInterface->ReapplyProvision();
		}

		ClearCache();

		m_iOldReapplyProvisionParity = m_iReapplyProvisionParity.Get();
	}
}
#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Call this inside your entity's Spawn()
//-----------------------------------------------------------------------------
void CAttributeManager::InitializeAttributes( CBaseEntity *pEntity )
{
	Assert( dynamic_cast<IHasAttributes*>( pEntity ) );
	m_hOuter = pEntity;
	m_bPreventLoopback = false;
}

//=====================================================================================================
// ATTRIBUTE PROVIDERS
//=====================================================================================================
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeManager::ProvideTo( CBaseEntity *pProvider )
{
	IHasAttributes *pOwnerAttribInterface = dynamic_cast<IHasAttributes *>( pProvider );
	if ( pOwnerAttribInterface )
	{
		if ( CAttributeManager * pAttrMgrForUse = pOwnerAttribInterface->GetAttributeManager() )
			pAttrMgrForUse->AddProvider( m_hOuter.Get() );

#ifndef CLIENT_DLL
		m_iReapplyProvisionParity = (m_iReapplyProvisionParity + 1) & PROVIDER_PARITY_MASK;
		NetworkStateChanged();
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeManager::StopProvidingTo( CBaseEntity *pProvider )
{
	IHasAttributes *pOwnerAttribInterface = dynamic_cast<IHasAttributes *>( pProvider );
	if ( pOwnerAttribInterface )
	{
		if ( CAttributeManager * pAttrMgrForUse = pOwnerAttribInterface->GetAttributeManager() )
			pAttrMgrForUse->RemoveProvider( m_hOuter.Get() );

#ifndef CLIENT_DLL
		m_iReapplyProvisionParity = (m_iReapplyProvisionParity + 1) & PROVIDER_PARITY_MASK;
		NetworkStateChanged();
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeManager::AddProvider( CBaseEntity *pProvider )
{
	// Make sure he's not already in our list, and prevent circular provision
	Assert( !IsBeingProvidedToBy(pProvider) );
	Assert( !IsProvidingTo(pProvider) );

	// Ensure he's allowed to provide
	Assert( dynamic_cast<IHasAttributes *>(pProvider) );

	m_Providers.AddToTail( pProvider );

	ClearCache();

	/*
#ifdef CLIENT_DLL
	Msg("CLIENT PROVIDER UPDATE: %s now has %d providers:\n", STRING(GetOuter()->GetDebugName()), m_Providers.Count() );
	for ( int i = 0; i < m_Providers.Count(); i++ )
	{
		Msg("    %d: %s\n", i, STRING(m_Providers[i]->GetDebugName()) );
	}
#else
	Msg("SERVER PROVIDER UPDATE: %s now has %d providers:\n", GetOuter()->GetDebugName(), m_Providers.Count() );
	for ( int i = 0; i < m_Providers.Count(); i++ )
	{
		Msg("    %d: %s\n", i, m_Providers[i]->GetDebugName() );
	}
#endif
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeManager::RemoveProvider( CBaseEntity *pProvider )
{
	m_Providers.FindAndRemove( pProvider );
	ClearCache();

/*
#ifdef CLIENT_DLL
	Msg("CLIENT PROVIDER UPDATE: %s now has %d providers:\n", STRING(GetOuter()->GetDebugName()), m_Providers.Count() );
	for ( int i = 0; i < m_Providers.Count(); i++ )
	{
		Msg("    %d: %s\n", i, STRING(m_Providers[i]->GetDebugName()) );
	}
#else
	Msg("SERVER PROVIDER UPDATE: %s now has %d providers:\n", GetOuter()->GetDebugName(), m_Providers.Count() );
	for ( int i = 0; i < m_Providers.Count(); i++ )
	{
		Msg("    %d: %s\n", i, m_Providers[i]->GetDebugName() );
	}
#endif
*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAttributeManager::ClearCache( void )
{
	if ( m_bPreventLoopback )
		return;

	m_CachedResults.Purge();

	m_bPreventLoopback = true;

	// Tell all providers relying on me that they need to wipe their cache too
	int iCount = m_Providers.Count();
	for ( int iHook = 0; iHook < iCount; iHook++ )
	{
		IHasAttributes *pAttribInterface = dynamic_cast<IHasAttributes *>(m_Providers[iHook].Get());
		if ( pAttribInterface )
		{
			if ( CAttributeManager * pAttrMgrForUse = pAttribInterface->GetAttributeManager() )
				pAttrMgrForUse->ClearCache();
		}
	}

	// Tell our owner that he needs to clear his too, in case he has attributes affecting him
	IHasAttributes *pMyAttribInterface = dynamic_cast<IHasAttributes *>( m_hOuter.Get().Get() );
	if ( pMyAttribInterface )
	{
		if ( CAttributeManager * pAttrMgrForUse = pMyAttribInterface->GetAttributeManager() )
			pAttrMgrForUse->ClearCache();
	}

	m_bPreventLoopback = false;

#ifndef CLIENT_DLL
	// Force out client to clear their cache as well
	m_iReapplyProvisionParity = (m_iReapplyProvisionParity + 1) & PROVIDER_PARITY_MASK;
	NetworkStateChanged();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CBaseEntity *CAttributeManager::GetProvider( int iIndex )
{ 
	Assert( iIndex >= 0 && iIndex < m_Providers.Count() );
	return m_Providers[iIndex]; 
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this entity is providing attributes to the specified entity
//-----------------------------------------------------------------------------
bool CAttributeManager::IsProvidingTo( CBaseEntity *pEntity )
{
	IHasAttributes *pAttribInterface = dynamic_cast<IHasAttributes *>(pEntity);
	if ( pAttribInterface )
	{
		if ( CAttributeManager * pAttrMgrForUse = pAttribInterface->GetAttributeManager() )
			if ( pAttrMgrForUse->IsBeingProvidedToBy( GetOuter() ) )
				return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this entity is being provided attributes by the specified entity
//-----------------------------------------------------------------------------
bool CAttributeManager::IsBeingProvidedToBy( CBaseEntity *pEntity )
{
	return ( m_Providers.Find( pEntity ) != m_Providers.InvalidIndex() );
}

//=====================================================================================================
// ATTRIBUTE HOOKS
//=====================================================================================================

//-----------------------------------------------------------------------------
// Purpose: Wrapper that checks to see if we've already got the result in our cache
//-----------------------------------------------------------------------------
float CAttributeManager::ApplyAttributeFloatWrapper( float flValue, CBaseEntity *pInitiator, string_t iszAttribHook, CUtlVector<CBaseEntity*> *pItemList )
{
	int iCount = m_CachedResults.Count();
	for ( int i = iCount-1; i >= 0; i-- )
	{
		if ( m_CachedResults[i].iAttribHook == iszAttribHook )
		{
			if ( m_CachedResults[i].flIn == flValue )
				return m_CachedResults[i].flOut;

			// We've got a cached result for a different flIn value. Remove the cached result to
			// prevent stacking up entries for different requests (i.e. crit chance)
			m_CachedResults.Remove(i);
			break;
		}
	}

	// Wasn't in cache. Do the work.
	float flResult = ApplyAttributeFloat( flValue, pInitiator, iszAttribHook, pItemList );

	// Add it to our cache
	int iIndex = m_CachedResults.AddToTail();
	m_CachedResults[iIndex].flIn = flValue;
	m_CachedResults[iIndex].flOut = flResult;
	m_CachedResults[iIndex].iAttribHook = iszAttribHook;

	return flResult;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CAttributeManager::ApplyAttributeFloat( float flValue, CBaseEntity *pInitiator, string_t iszAttribHook, CUtlVector<CBaseEntity*> *pItemList )
{
	if ( m_bPreventLoopback || !GetOuter() )
		return flValue;

	// We need to prevent loopback between two items both providing to the same entity.
	m_bPreventLoopback = true;

	// See if we have any providers. If we do, tell them to apply.
	int iCount = m_Providers.Count();
	for ( int iHook = 0; iHook < iCount; iHook++ )
	{
		if ( m_Providers[iHook].Get() == pInitiator )
			continue;

		// Don't allow weapons to provide to other weapons being carried by the same person
		IHasAttributes *pAttribInterface = dynamic_cast<IHasAttributes *>(m_Providers[iHook].Get());
		if ( pInitiator && pAttribInterface->GetAttributeManager() &&
			pAttribInterface->GetAttributeManager()->GetProviderType() == PROVIDER_WEAPON )
		{
			IHasAttributes *pInitiatorAttribInterface = dynamic_cast<IHasAttributes *>(pInitiator);
			if ( pInitiatorAttribInterface->GetAttributeManager() &&
				pInitiatorAttribInterface->GetAttributeManager()->GetProviderType() == PROVIDER_WEAPON )
				continue;
		}

		if ( CAttributeManager *pAttrMgrForUse = pAttribInterface->GetAttributeManager() )
			flValue = pAttrMgrForUse->ApplyAttributeFloat( flValue, pInitiator, iszAttribHook, pItemList );
	}

	// Then see if our owner has any attributes he wants to apply as well.
	// i.e. An aura is providing attributes to this weapon's carrier.
	IHasAttributes *pMyAttribInterface = dynamic_cast<IHasAttributes *>( m_hOuter.Get().Get() );
	if ( pMyAttribInterface->GetAttributeOwner() )
	{
		IHasAttributes *pOwnerAttribInterface = dynamic_cast<IHasAttributes *>( pMyAttribInterface->GetAttributeOwner() );
		if ( pOwnerAttribInterface )
		{
			if ( CAttributeManager * pAttrMgrForUse = pOwnerAttribInterface->GetAttributeManager() )
				flValue = pAttrMgrForUse->ApplyAttributeFloat( flValue, pInitiator, iszAttribHook, pItemList );
		}
	}

	m_bPreventLoopback = false;

	return flValue;
}

//=====================================================================================================
// ATTRIBUTE CONTAINER
//=====================================================================================================

//-----------------------------------------------------------------------------
// Purpose: Call this inside your entity's Spawn()
//-----------------------------------------------------------------------------
void CAttributeContainer::InitializeAttributes( CBaseEntity *pEntity )
{
	BaseClass::InitializeAttributes( pEntity );

#ifndef CLIENT_DLL
	/*
	if ( !m_Item.IsValid() )
	{
		Warning("Item '%s' not setup correctly. Attempting to create attributes on an unitialized item.\n", m_hOuter.Get()->GetDebugName() );
	}
	*/
#endif

	m_Item.GetAttributeList()->SetManager( this );

	ClearCache();
}

static void ApplyAttribute( const CEconItemAttributeDefinition *pAttributeDef, float& flValue, const float flValueModifier )
{
	Assert( pAttributeDef );
	Assert( pAttributeDef->GetAttributeType() );
	AssertMsg1( pAttributeDef->GetAttributeType()->BSupportsGameplayModificationAndNetworking(), "Attempt to hook the value of attribute '%s' which doesn't support hooking! Pull the value of the attribute directly using FindAttribute()!", pAttributeDef->GetDefinitionName() );

	const int iAttrDescFormat = pAttributeDef->GetDescriptionFormat();

	switch ( iAttrDescFormat )
	{
	case ATTDESCFORM_VALUE_IS_PERCENTAGE:
	case ATTDESCFORM_VALUE_IS_INVERTED_PERCENTAGE:
		{
			flValue *= flValueModifier;
		}
		break;

	case ATTDESCFORM_VALUE_IS_COLOR:
	case ATTDESCFORM_VALUE_IS_ADDITIVE:
	case ATTDESCFORM_VALUE_IS_ADDITIVE_PERCENTAGE:
	case ATTDESCFORM_VALUE_IS_PARTICLE_INDEX:
		{
			flValue += flValueModifier;
		}
		break;

	case ATTDESCFORM_VALUE_IS_REPLACE:
		{
			flValue = flValueModifier;
		}
		break;

	case ATTDESCFORM_VALUE_IS_OR:
		{
			int iTmp = flValue;
			iTmp |= (int)flValueModifier;
			flValue = iTmp;
		}
		break;

	case ATTDESCFORM_VALUE_IS_GAME_TIME:
	case ATTDESCFORM_VALUE_IS_DATE:
		Assert( !"Attempt to apply date attribute in ApplyAttribute()." ); // No-one should be hooking date descriptions
		break;

	default:
		// Unknown value format. 
		AssertMsg1( false, "Unknown attribute value type %i in ApplyAttribute().", iAttrDescFormat );
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Given two attributes, return a collated value.
//-----------------------------------------------------------------------------
float CollateAttributeValues( const CEconItemAttribute *pAttrib1, const CEconItemAttribute *pAttrib2 )
{
	// We can only collate attributes of matching definitions
	Assert( !Q_stricmp( pAttrib1->GetStaticData()->GetAttributeClass(), pAttrib2->GetStaticData()->GetAttributeClass() ) );

	const CEconItemAttributeDefinition *pDef = pAttrib1->GetStaticData();
	const int iAttrDescFormat = pDef->GetDescriptionFormat();

	float flValue = 0;
	switch ( iAttrDescFormat )
	{
	case ATTDESCFORM_VALUE_IS_PERCENTAGE:
	case ATTDESCFORM_VALUE_IS_INVERTED_PERCENTAGE:
		{
			flValue = 1.0;
		}
		break;

	case ATTDESCFORM_VALUE_IS_COLOR:
	case ATTDESCFORM_VALUE_IS_ADDITIVE:
	case ATTDESCFORM_VALUE_IS_ADDITIVE_PERCENTAGE:
	case ATTDESCFORM_VALUE_IS_OR:
	case ATTDESCFORM_VALUE_IS_REPLACE:
		{
			flValue = 0;
		}
		break;

	case ATTDESCFORM_VALUE_IS_DATE:
		Assert( !"Attempt to apply date attribute in ApplyAttribute()." ); // No-one should be hooking date descriptions
		break;

	default:
		// Unknown value format. 
		AssertMsg1( false, "Unknown attribute value type %i in ApplyAttribute().", iAttrDescFormat );
		break;
	}

	ApplyAttribute( pDef, flValue, pAttrib1->GetValue() );
	ApplyAttribute( pDef, flValue, pAttrib2->GetValue() );

	return flValue;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
class CEconItemAttributeIterator_ApplyAttributeFloat : public IEconItemAttributeIterator
{
public:
	CEconItemAttributeIterator_ApplyAttributeFloat( CBaseEntity *pOuter, float flInitialValue, string_t iszAttribHook, CUtlVector<CBaseEntity *> *pItemList )
		: m_pOuter( pOuter )
		, m_flValue( flInitialValue )
		, m_iszAttribHook( iszAttribHook )
		, m_pItemList( pItemList )
	{
		Assert( pOuter );
	}

	virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, attrib_value_t value )
	{
		COMPILE_TIME_ASSERT( sizeof( value ) == sizeof( float ) );

		Assert( pAttrDef );

		if ( pAttrDef->GetCachedClass() != m_iszAttribHook )
			return true;

		if ( m_pItemList && !m_pItemList->HasElement( m_pOuter ) )
		{
			m_pItemList->AddToTail( m_pOuter );
		}

		ApplyAttribute( pAttrDef, m_flValue, *reinterpret_cast<float *>( &value ) );

		// We assume that each attribute can only be in the attribute list for a single item once, but we're
		// iterating over attribute *classes* here, not unique attribute types, so we carry on looking.
		return true;
	}

	virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, float value )
	{
		Assert( pAttrDef );

		if ( pAttrDef->GetCachedClass() != m_iszAttribHook )
			return true;

		if ( m_pItemList && !m_pItemList->HasElement( m_pOuter ) )
		{
			m_pItemList->AddToTail( m_pOuter );
		}

		ApplyAttribute( pAttrDef, m_flValue, value );

		// We assume that each attribute can only be in the attribute list for a single item once, but we're
		// iterating over attribute *classes* here, not unique attribute types, so we carry on looking.
		return true;
	}

	virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const CAttribute_String& value )
	{
		// We can't possibly process an attribute of this type!
		Assert( pAttrDef );
		Assert( pAttrDef->GetCachedClass() != m_iszAttribHook );

		return true;
	}

	virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const Vector& value )
	{
		// We can't possibly process an attribute of this type!
		Assert( pAttrDef );
		Assert( pAttrDef->GetCachedClass() != m_iszAttribHook );

		return true;
	}

	float GetResultValue() const
	{
		return m_flValue;
	}

private:
	CBaseEntity *m_pOuter;
	float m_flValue;
	string_t m_iszAttribHook;
	CUtlVector<CBaseEntity *> *m_pItemList;
};

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CAttributeContainer::ApplyAttributeFloat( float flValue, CBaseEntity *pInitiator, string_t iszAttribHook, CUtlVector<CBaseEntity*> *pItemList )
{
	if ( m_bPreventLoopback || !GetOuter() )
		return flValue;

	// We need to prevent loopback between two items both providing to the same entity.
	m_bPreventLoopback = true;

	// ...
	CEconItemAttributeIterator_ApplyAttributeFloat it( GetOuter(), flValue, iszAttribHook, pItemList );
	m_Item.IterateAttributes( &it );

	m_bPreventLoopback = false;

	return BaseClass::ApplyAttributeFloat( it.GetResultValue(), pInitiator, iszAttribHook, pItemList );
}

#if defined( TF_DLL ) || defined( CSTRIKE_DLL )
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CAttributeContainerPlayer::InitializeAttributes( CBaseEntity *pEntity )
{
	BaseClass::InitializeAttributes( pEntity );

	ClearCache();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CAttributeContainerPlayer::ApplyAttributeFloat( float flValue, CBaseEntity *pInitiator, string_t iszAttribHook, CUtlVector<CBaseEntity*> *pItemList )
{
	if ( m_bPreventLoopback || !GetOuter() )
		return flValue;

	m_bPreventLoopback = true;
#ifdef DEBUG
	int iFoundAttributeCount = 0;
#endif // DEBUG

	CEconItemAttributeIterator_ApplyAttributeFloat it( GetOuter(), flValue, iszAttribHook, pItemList );

#if defined( USE_PLAYER_ATTRIBUTE_MANAGER )
	CBasePlayer *pPlayer = GetPlayer();
	if ( pPlayer )
	{
		pPlayer->m_AttributeList.IterateAttributes( &it );

		// Apply all the attributes within this manager
		int iAttributes = pPlayer->m_AttributeList.GetNumAttributes();
		for ( int i = 0; i < iAttributes; i++ )
		{
			CEconItemAttribute *pAttribute = pPlayer->m_AttributeList.GetAttribute(i);
			CEconItemAttributeDefinition *pData = pAttribute->GetStaticData();

			// The first time we try to compare to an attribute, we alloc this for faster future lookup
			if ( pData->GetCachedClass() == iszAttribHook )
			{
				// If we are keep track (ie. pItemList != NULL), then put the item in the list.
				if ( pItemList )
				{
					if ( pItemList->Find( GetOuter() ) == -1 )
					{
						pItemList->AddToTail( GetOuter() );
					}
				}

				ApplyAttribute( pData, flValue, pAttribute->GetValue() );
#ifdef DEBUG
				iFoundAttributeCount++;
#endif // DEBUG
			}
		}
	}
#endif //#if defined( USE_PLAYER_ATTRIBUTE_MANAGER )

#ifdef DEBUG
	// If we didn't find any attributes on this object, loop through all the attributes in our schema to find
	// out whether this attribute even exists so we can spew a warning if it doesn't.
	if ( iFoundAttributeCount == 0 )
	{
		const CEconItemSchema::EconAttrDefsContainer_t & mapAttrDefs = ItemSystem()->GetItemSchema()->GetAttributeDefinitionContainer();
		FOR_EACH_VEC( mapAttrDefs, i )
		{
			if ( mapAttrDefs[i] && ( mapAttrDefs[i]->GetCachedClass() == iszAttribHook ) )
			{
				iFoundAttributeCount++;
				break;
			}
		}
	}

	AssertMsg1( iFoundAttributeCount != 0, "Attempt to apply unknown attribute '%s'.", STRING( iszAttribHook ) );
#endif // DEBUG

	m_bPreventLoopback = false;

	return BaseClass::ApplyAttributeFloat( it.GetResultValue(), pInitiator, iszAttribHook, pItemList );
}


#endif
