//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: Attributable entities contain one of these, which handles game specific handling:
//				- Save / Restore
//				- Networking
//				- Attribute providers
//				- Application of attribute effects
//
//=============================================================================

#ifndef ATTRIBUTE_MANAGER_H
#define ATTRIBUTE_MANAGER_H
#ifdef _WIN32
#pragma once
#endif

#include "econ_item_view.h"
#include "ihasattributes.h"

// Provider types
enum attributeprovidertypes_t
{
	PROVIDER_GENERIC,
	PROVIDER_WEAPON,
};

float CollateAttributeValues( const CEconItemAttribute *pAttrib1, const CEconItemAttribute *pAttrib2 );

//-----------------------------------------------------------------------------
// Macros for hooking the application of attributes
#define CALL_ATTRIB_HOOK( vartype, retval, hookName, who, itemlist ) \
	retval = CAttributeManager::AttribHookValue<vartype>( retval, #hookName, static_cast<const CBaseEntity*>( who ), itemlist, true );

#define CALL_ATTRIB_HOOK_INT( retval, hookName )	CALL_ATTRIB_HOOK( int, retval, hookName, this, NULL )
#define CALL_ATTRIB_HOOK_FLOAT( retval, hookName )	CALL_ATTRIB_HOOK( float, retval, hookName, this, NULL )

#define CALL_ATTRIB_HOOK_INT_ON_OTHER( other, retval, hookName )	CALL_ATTRIB_HOOK( int, retval, hookName, other, NULL )
#define CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( other, retval, hookName )	CALL_ATTRIB_HOOK( float, retval, hookName, other, NULL )

#define CALL_ATTRIB_HOOK_BOOL( hookName ) ( 0 != CAttributeManager::AttribHookValue<int>( 0, #hookName, static_cast<const CBaseEntity*>( this ), NULL, true ) )
#define CALL_ATTRIB_HOOK_BOOL_ON_OTHER( other, hookName ) ( 0 != CAttributeManager::AttribHookValue<int>( 0, #hookName, static_cast<const CBaseEntity*>( other ), NULL, true ) )

template< class T > T AttributeConvertFromFloat( float flValue );
template<> float AttributeConvertFromFloat<float>( float flValue );
template<> int AttributeConvertFromFloat<int>( float flValue );

//-----------------------------------------------------------------------------
// Purpose: Base Attribute manager.
//			This class knows how to apply attribute effects that have been
//			provided to its owner by other entities, but doesn't contain attributes itself.
//-----------------------------------------------------------------------------
class CAttributeManager
{
	DECLARE_CLASS_NOBASE( CAttributeManager );
public:
	DECLARE_DATADESC();
	DECLARE_EMBEDDED_NETWORKVAR();

	virtual ~CAttributeManager() {}

	// Call this inside your entity's Spawn()
	virtual void InitializeAttributes( CBaseEntity *pEntity );

	CBaseEntity *GetOuter( void ) { return m_hOuter.Get(); }

	//--------------------------------------------------------
	// Attribute providers.
	// Other entities that are providing attributes to this entity (i.e. weapons being carried by a player)
	void ProvideTo( CBaseEntity *pProvider );
	void StopProvidingTo( CBaseEntity *pProvider );

protected:
	// Not to be called directly. Use ProvideTo() or StopProvidingTo() above.
	void AddProvider( CBaseEntity *pProvider );
	void RemoveProvider( CBaseEntity *pProvider );

public:
	int  GetNumProviders( void ) { return m_Providers.Count(); }
	CBaseEntity *GetProvider( int iIndex );

	// Return true if this entity is providing attributes to the specified entity
	bool IsProvidingTo( CBaseEntity *pEntity );

	// Return true if this entity is being provided attributes by the specified entity
	bool IsBeingProvidedToBy( CBaseEntity *pEntity );

	// Provider types are used to prevent specified providers supplying to certain initiators
	void SetProviderType( attributeprovidertypes_t tType ) { m_ProviderType = tType; }
	attributeprovidertypes_t GetProviderType( void ) { return m_ProviderType; }

	//--------------------------------------------------------
	// Attribute hook. Use the CALL_ATTRIB_HOOK macros above.
	template <class T> static T AttribHookValue( T TValue, const char *pszAttribHook, const CBaseEntity *pEntity, CUtlVector<CBaseEntity*> *pItemList = NULL, bool bIsGlobalConstString = false )
	{
		// Do we have a hook?
		if ( pszAttribHook == NULL || pszAttribHook[0] == '\0' )
			return TValue;

		// Verify that we have an entity, at least as "this"
		if ( pEntity == NULL )
			return TValue;

		IHasAttributes *pAttribInterface = dynamic_cast<IHasAttributes*>( (CBaseEntity*) pEntity );
		Assert(pAttribInterface);	// If you hit this, you've probably got a hook incorrectly setup, because the entity it's hooking on doesn't know about attributes.
		if ( pAttribInterface && pAttribInterface->GetAttributeManager() )
		{
			string_t iszAttribHook = bIsGlobalConstString ? AllocPooledStringConstant(pszAttribHook) : AllocPooledString(pszAttribHook);
			float flValue = pAttribInterface->GetAttributeManager()->ApplyAttributeFloatWrapper( static_cast<float>( TValue ), (CBaseEntity*) pEntity, iszAttribHook, pItemList );
			TValue = AttributeConvertFromFloat<T>( flValue );
		}

		return TValue;
	}
	virtual float	ApplyAttributeFloat( float flValue, CBaseEntity *pInitiator, string_t iszAttribHook = NULL_STRING, CUtlVector<CBaseEntity*> *pItemList = NULL );

	//--------------------------------------------------------
	// Networking
#ifdef CLIENT_DLL
	virtual void	OnPreDataChanged( DataUpdateType_t updateType );
	virtual void	OnDataChanged( DataUpdateType_t updateType );
#endif

	//--------------------------------------------------------
	// memory handling
	void *operator new( size_t stAllocateBlock );
	void *operator new( size_t stAllocateBlock, int nBlockUse, const char *pFileName, int nLine );

protected:
	CUtlVector<EHANDLE>							m_Providers;
	CNetworkVarForDerived( int,					m_iReapplyProvisionParity );
	CNetworkVarForDerived( EHANDLE,				m_hOuter );
	bool										m_bPreventLoopback;
	CNetworkVarForDerived( attributeprovidertypes_t,	m_ProviderType );

public:
	void	ClearCache( void );

private:

	virtual float	ApplyAttributeFloatWrapper( float flValue, CBaseEntity *pInitiator, string_t pszAttribHook, CUtlVector<CBaseEntity*> *pItemList = NULL );

private:

	// Cached attribute results
	// We cache off requests for data, and wipe the cache whenever our providers change.
	struct cached_attribute_float_t
	{
		float		flIn;
		string_t	iAttribHook;
		float		flOut;
	};
	CUtlVector<cached_attribute_float_t>	m_CachedResults;

#ifdef CLIENT_DLL
public:
	// Data received from the server
	int							m_iOldReapplyProvisionParity;
#endif
};


//-----------------------------------------------------------------------------
// Purpose: This is an attribute manager that also knows how to contain attributes.
//-----------------------------------------------------------------------------
class CAttributeContainer : public CAttributeManager
{
public:
	DECLARE_DATADESC();
	DECLARE_CLASS( CAttributeContainer, CAttributeManager );
	DECLARE_EMBEDDED_NETWORKVAR();

	virtual void InitializeAttributes( CBaseEntity *pEntity );

	//--------------------------------------------------------
	// Attribute hook. Use the CALL_ATTRIB_HOOK macros above.
	virtual float	ApplyAttributeFloat( float flValue, CBaseEntity *pInitiator, string_t iszAttribHook = NULL_STRING, CUtlVector<CBaseEntity*> *pItemList = NULL ) OVERRIDE;

	CEconItemView *GetItem( void ) { return m_Item.Get(); }
	const CEconItemView *GetItem( void ) const { return m_Item.Get(); }
	void		SetItem( CEconItemView *pItem ) 
	{ 
		m_Item.CopyFrom( *pItem ); 
	}

private:
	CNetworkVarEmbedded( CEconItemView,	m_Item );
};

//-----------------------------------------------------------------------------
// Purpose: An attribute manager that uses a player's shared attributes.
//-----------------------------------------------------------------------------

#if defined( TF_DLL ) || defined( CSTRIKE_DLL )
class CAttributeContainerPlayer : public CAttributeManager
{
public:
	DECLARE_DATADESC();
	DECLARE_CLASS( CAttributeContainerPlayer, CAttributeManager );
	DECLARE_EMBEDDED_NETWORKVAR();

	virtual void	InitializeAttributes( CBaseEntity *pEntity );
	virtual float	ApplyAttributeFloat( float flValue, CBaseEntity *pInitiator, string_t iszAttribHook = NULL_STRING, CUtlVector<CBaseEntity*> *pItemList = NULL ) OVERRIDE;

	CBasePlayer*	GetPlayer( void ) { return m_hPlayer; }
	void			SetPlayer( CBasePlayer *pPlayer ) { m_hPlayer = pPlayer; }

private:
	CNetworkHandle( CBasePlayer, m_hPlayer );
};
#endif

#ifdef CLIENT_DLL
EXTERN_RECV_TABLE( DT_AttributeManager );
EXTERN_RECV_TABLE( DT_AttributeContainer );
#else
EXTERN_SEND_TABLE( DT_AttributeManager );
EXTERN_SEND_TABLE( DT_AttributeContainer );
#endif

#endif // ATTRIBUTE_MANAGER_H
