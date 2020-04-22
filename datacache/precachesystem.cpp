//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:
//
//===========================================================================//

#include "tier1/utlsymbol.h"
#include "tier1/UtlStringMap.h"
#include "tier2/tier2.h"
#include "datacache/iprecachesystem.h"
#include "datacache/iresourceaccesscontrol.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Implementation class
//-----------------------------------------------------------------------------
class CPrecacheSystem : public CTier2AppSystem< IPrecacheSystem >
{
	typedef CTier2AppSystem< IPrecacheSystem > BaseClass;

	// Inherited from IAppSystem
public:

	// Inherited from IResourceAccessControl
public:
	void Register( IResourcePrecacher *pResourcePrecacherFirst, PrecacheSystem_t nSystem );

	// Precaches/uncaches all resources used by a particular system
	void Cache( IPrecacheHandler *pPrecacheHandler, PrecacheSystem_t nSystem, const char *pName, bool bPrecache, ResourceList_t hResourceList, bool bBuildResourceList );

	void UncacheAll( IPrecacheHandler *pPrecacheHandler );

	// Limits resource access to only resources used by this particular system
	// Use GLOBAL system, and NULL name to disable limited resource access
	void LimitResourceAccess( PrecacheSystem_t nSystem, const char *pName );
	void EndLimitedResourceAccess();

private:
	IResourcePrecacher *m_pFirstPrecacher[PRECACHE_SYSTEM_COUNT];
	CUtlStringMap< ResourceList_t > m_ResourceList[ PRECACHE_SYSTEM_COUNT ];
};

//-----------------------------------------------------------------------------
// String names corresponding to resource types
//-----------------------------------------------------------------------------
static const char *s_pResourceSystemName[] =
{
	"global client resource",
	"global server resource",
	"vgui panel",		
	"dispatch effect",	
	"shared system",
};

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CPrecacheSystem g_PrecacheSystem;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CPrecacheSystem, IPrecacheSystem, 
	PRECACHE_SYSTEM_INTERFACE_VERSION, g_PrecacheSystem );

//-----------------------------------------------------------------------------
// Precaches/uncaches all resources used by a particular system
//-----------------------------------------------------------------------------
void CPrecacheSystem::Cache( IPrecacheHandler *pPrecacheHandler, PrecacheSystem_t nSystem, const char *pName, bool bPrecache, ResourceList_t hResourceList, bool bBuildResourceList )
{	
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_pResourceSystemName ) == PRECACHE_SYSTEM_COUNT );

	IResourcePrecacher *pPrecacher = m_pFirstPrecacher[nSystem];
	for( ; pPrecacher; pPrecacher = pPrecacher->GetNext() )
	{
		if ( pName && Q_stricmp( pName, pPrecacher->GetName() ) )
			continue;
		
		if ( bBuildResourceList && g_pResourceAccessControl )
		{
			Assert( hResourceList == RESOURCE_LIST_INVALID );
			UtlSymId_t idx = m_ResourceList[ nSystem ].Find( pName );
			if ( idx != UTL_INVAL_SYMBOL )
			{
				hResourceList = m_ResourceList[ nSystem ][ idx ];
			}
			else
			{
				char pDebugName[256];
				Q_snprintf( pDebugName, sizeof(pDebugName), "%s \"%s\"", s_pResourceSystemName[nSystem], pName );
				hResourceList = g_pResourceAccessControl->CreateResourceList( pDebugName );
				m_ResourceList[ nSystem ][ pName ] = hResourceList;
			}
		}
		pPrecacher->Cache( pPrecacheHandler, bPrecache, hResourceList, false );

		if ( !bPrecache )
		{
			m_ResourceList[ nSystem ][ pName ] = NULL;
		}
	}
}

//-----------------------------------------------------------------------------
// Uncaches everything
//-----------------------------------------------------------------------------
void CPrecacheSystem::UncacheAll( IPrecacheHandler *pPrecacheHandler )
{
	int nSystem;
	for ( nSystem = 0; nSystem < PRECACHE_SYSTEM_COUNT; nSystem ++ )
	{
		IResourcePrecacher *pPrecacher = m_pFirstPrecacher[nSystem];
		for( ; pPrecacher; pPrecacher = pPrecacher->GetNext() )
		{
			pPrecacher->Cache( pPrecacheHandler, false, RESOURCE_LIST_INVALID, false );
		}

		m_ResourceList[nSystem].Purge();
	}
}

//-----------------------------------------------------------------------------
// Called to register a list of resource precachers for a given system
//-----------------------------------------------------------------------------
void CPrecacheSystem::Register( IResourcePrecacher *pResourcePrecacherFirst, PrecacheSystem_t nSystem )
{
	// do we already have any precachers registered for this system?
	IResourcePrecacher *pCur = m_pFirstPrecacher[nSystem];

	if ( pCur )
	{
		while ( pCur->GetNext() != NULL )
		{
			pCur = pCur->GetNext();
		}
		// add the head of the new list to the tail of the existing list
		pCur->SetNext( pResourcePrecacherFirst );
	}
	else
	{
		m_pFirstPrecacher[nSystem] = pResourcePrecacherFirst;
	}	
}

//-----------------------------------------------------------------------------
// Limits resource access to only resources used by this particular system
// Use GLOBAL system, and NULL name to disable limited resource access
//-----------------------------------------------------------------------------
void CPrecacheSystem::LimitResourceAccess( PrecacheSystem_t nSystem, const char *pName )
{
	if ( g_pResourceAccessControl )
	{
		UtlSymId_t nSym = ( pName != NULL ) ? m_ResourceList[nSystem].Find( pName ) : UTL_INVAL_SYMBOL;
		if ( nSym != UTL_INVAL_SYMBOL )
		{
			g_pResourceAccessControl->LimitAccess( m_ResourceList[nSystem][nSym] );
		}
		else
		{
			g_pResourceAccessControl->LimitAccess( RESOURCE_LIST_INVALID );
		}
	}
}


void CPrecacheSystem::EndLimitedResourceAccess()
{
	if ( g_pResourceAccessControl )
	{
		g_pResourceAccessControl->LimitAccess( RESOURCE_LIST_INVALID );
	}
}
