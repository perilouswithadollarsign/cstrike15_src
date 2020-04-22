//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_extensions.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CMatchExtensions::CMatchExtensions()
{
	;
}

CMatchExtensions::~CMatchExtensions()
{
	;
}

static CMatchExtensions g_MatchExtensions;
CMatchExtensions *g_pMatchExtensions = &g_MatchExtensions;

//
// Implementation
//

void CMatchExtensions::RegisterExtensionInterface( char const *szInterfaceString, void *pvInterface )
{
	Assert( szInterfaceString );
	Assert( pvInterface );
	if ( !szInterfaceString || !pvInterface )
		return;

	RegisteredInterface_t &rInfo = m_mapRegisteredInterfaces[ szInterfaceString ];
	Assert( rInfo.m_nRefCount >= 0 );

	if ( rInfo.m_nRefCount > 0 )
	{
		// Interface already registered
		Assert( pvInterface == rInfo.m_pvInterface );
	}
	else
	{
		// Fresh registration of the interface
		Assert( !rInfo.m_pvInterface );
		rInfo.m_pvInterface = pvInterface;
	}

	// Increment refcount
	++ rInfo.m_nRefCount;

	// Fire a callback for interface registration
	OnExtensionInterfaceUpdated( szInterfaceString, pvInterface );
}

void CMatchExtensions::UnregisterExtensionInterface( char const *szInterfaceString, void *pvInterface )
{
	Assert( szInterfaceString );
	Assert( pvInterface );
	if ( !szInterfaceString || !pvInterface )
		return;

	RegisteredInterface_t &rInfo = m_mapRegisteredInterfaces[ szInterfaceString ];
	Assert( rInfo.m_nRefCount > 0 );
	if ( rInfo.m_nRefCount <= 0 )
		return;

	Assert( pvInterface == rInfo.m_pvInterface );

	-- rInfo.m_nRefCount;
	if ( 0 == rInfo.m_nRefCount )
	{
		rInfo.m_pvInterface = NULL;
		OnExtensionInterfaceUpdated( szInterfaceString, NULL );
	}
}

void * CMatchExtensions::GetRegisteredExtensionInterface( char const *szInterfaceString )
{
	RegisteredInterface_t &rInfo = m_mapRegisteredInterfaces[ szInterfaceString ];
	
	if ( rInfo.m_nRefCount > 0 && rInfo.m_pvInterface )
		return rInfo.m_pvInterface;
	else
		return NULL;
}

//
// Known interfaces recognition
//

void CMatchExtensions::OnExtensionInterfaceUpdated( char const *szInterfaceString, void *pvInterface )
{
	typedef void * Exts_t::* Ext_t;

	struct CachedInterfacePtr_t
	{
		char const *m_szName;
		Ext_t m_ppInterface;
	};

	static CachedInterfacePtr_t s_table[] =
	{
		{ LOCALIZE_INTERFACE_VERSION,		(Ext_t) &Exts_t::m_pILocalize },
		{ INETSUPPORT_VERSION_STRING,		(Ext_t) &Exts_t::m_pINetSupport },
		{ IENGINEVOICE_INTERFACE_VERSION,	(Ext_t) &Exts_t::m_pIEngineVoice },
		{ VENGINE_CLIENT_INTERFACE_VERSION,	(Ext_t) &Exts_t::m_pIVEngineClient },
		{ INTERFACEVERSION_VENGINESERVER,	(Ext_t) &Exts_t::m_pIVEngineServer },
		{ INTERFACEVERSION_SERVERGAMEDLL,	(Ext_t) &Exts_t::m_pIServerGameDLL },
		{ INTERFACEVERSION_GAMEEVENTSMANAGER2, (Ext_t) &Exts_t::m_pIGameEventManager2 },
		{ CLIENT_DLL_INTERFACE_VERSION,		( Ext_t ) &Exts_t::m_pIBaseClientDLL },
#ifdef _X360
		{ XBOXSYSTEM_INTERFACE_VERSION,		(Ext_t) &Exts_t::m_pIXboxSystem },
		{ XONLINE_INTERFACE_VERSION,		(Ext_t) &Exts_t::m_pIXOnline },
#endif
		{ NULL, NULL }
	};

	for ( CachedInterfacePtr_t *ptr = s_table; ptr->m_szName; ++ ptr )
	{
		if ( !Q_stricmp( ptr->m_szName, szInterfaceString ) )
		{
			m_exts.*(ptr->m_ppInterface) = pvInterface;
		}
	}
}
