//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:
//
//===========================================================================//

#include "datacache/iresourceaccesscontrol.h"
#include "tier1/utlvector.h"
#include "tier1/utlstring.h"
#include "tier2/tier2.h"
#include "tier1/convar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// NOTE: Necessary until we have all our resources wrangled.
//-----------------------------------------------------------------------------
static ConVar res_restrict_access( "res_restrict_access", "0" );


//-----------------------------------------------------------------------------
// Implementation class
//-----------------------------------------------------------------------------
class CResourceAccessControl : public CTier1AppSystem< IResourceAccessControl >
{
	typedef CTier1AppSystem< IResourceAccessControl > BaseClass;

	// Inherited from IAppSystem
public:
	virtual void Shutdown();

	// Inherited from IResourceAccessControl
public:
	virtual ResourceList_t CreateResourceList( const char *pDebugName );
	virtual void DestroyAllResourceLists( );
	virtual void AddResource( ResourceList_t hResourceList, ResourceTypeOld_t nType, const char *pResourceName );
	virtual void LimitAccess( ResourceList_t hResourceList );
	virtual bool IsAccessAllowed( ResourceTypeOld_t nType, const char *pResource );

	// Other public methods
public:
	CResourceAccessControl();

private:
	enum
	{
		MAX_THREADS = 16
	};

	struct ResourceInfo_t
	{
		CUtlString m_DebugName;
		CUtlVector< CUtlString > m_Resources[RESOURCE_TYPE_OLD_COUNT]; 
	};

	bool ContainsResource( ResourceList_t hResourceList, ResourceTypeOld_t nType, const char *pResourceName );
	int FindOrAddCurrentThreadID();
	void FixupResourceName( const char *pResourceName, char *pBuf, int nBufLen );

	CUtlVector< ResourceInfo_t > m_ResourceLists;
	int m_pLimitAccess[MAX_THREADS];
	unsigned long m_pThread[MAX_THREADS];
	int m_nThreadCount;
	CThreadMutex m_Mutex;
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CResourceAccessControl g_ResourceAccessControl;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CResourceAccessControl, IResourceAccessControl, 
	RESOURCE_ACCESS_CONTROL_INTERFACE_VERSION, g_ResourceAccessControl );


//-----------------------------------------------------------------------------
// Resource type names
//-----------------------------------------------------------------------------
static const char *s_pResourceTypeName[] = 
{
	"vgui panel",		// .res file
	"material",			// .vmt file
	"model",			// .mdl file
	"particle system",	// particle system
	"game sound",		// game sound
};


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CResourceAccessControl::CResourceAccessControl()
{
	m_nThreadCount = 0;
	memset( m_pLimitAccess, 0xFF, sizeof(m_pLimitAccess) );
	memset( m_pThread, 0, sizeof(m_pThread) );
}


//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------
void CResourceAccessControl::Shutdown()
{
	DestroyAllResourceLists();
	memset( m_pLimitAccess, 0xFF, sizeof(m_pLimitAccess) );
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Creates a resource list
//-----------------------------------------------------------------------------
int CResourceAccessControl::FindOrAddCurrentThreadID()
{
	unsigned long nThreadId = ThreadGetCurrentId();
	for ( int i = 0; i < m_nThreadCount; ++i )
	{
		if ( m_pThread[i] == nThreadId )
			return i;
	}

	if ( m_nThreadCount >= MAX_THREADS )
	{
		Error( "Exceeded maximum number of unique threads (%d) attempting to access datacache.\n", MAX_THREADS );
		return -1;
	}

	m_Mutex.Lock();
	int nIndex = m_nThreadCount;
	m_pThread[m_nThreadCount++] = nThreadId;
	m_Mutex.Unlock();
	return nIndex;
}


//-----------------------------------------------------------------------------
// Fixes up the asset name
//-----------------------------------------------------------------------------
void CResourceAccessControl::FixupResourceName( const char *pResourceName, char *pBuf, int nBufLen )
{
	Q_StripExtension( pResourceName, pBuf, nBufLen );
	Q_FixSlashes( pBuf, '/' );
	V_RemoveDotSlashes( pBuf, '/' );
}


//-----------------------------------------------------------------------------
// Creates a resource list
//-----------------------------------------------------------------------------
ResourceList_t CResourceAccessControl::CreateResourceList( const char *pDebugName )
{
	int nIndex = m_ResourceLists.AddToTail();
	m_ResourceLists[nIndex].m_DebugName.Set( pDebugName );
	return (ResourceList_t)(intp)nIndex;
}

void CResourceAccessControl::DestroyAllResourceLists( )
{
	m_ResourceLists.Purge();
	memset( m_pLimitAccess, 0xFF, sizeof(m_pLimitAccess) );
}

bool CResourceAccessControl::ContainsResource( ResourceList_t hResourceList, ResourceTypeOld_t nType, const char *pResourceName )
{
	char pBuf[MAX_PATH];
	FixupResourceName( pResourceName, pBuf, sizeof(pBuf) );

	int nIndex = size_cast< int >( (intp)hResourceList );
	CUtlVector< CUtlString > &list = m_ResourceLists[nIndex].m_Resources[nType];
	int nCount = list.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pBuf, list[i].Get() ) )
			return true;
	}
	return false;
}

void CResourceAccessControl::AddResource( ResourceList_t hResourceList, ResourceTypeOld_t nType, const char *pResourceName )
{
	if ( ContainsResource( hResourceList, nType, pResourceName ) )
		return;

	int nIndex = size_cast< int >( (intp)hResourceList );
	int nSubIndex = m_ResourceLists[nIndex].m_Resources[nType].AddToTail();

	char pBuf[MAX_PATH];
	FixupResourceName( pResourceName, pBuf, sizeof(pBuf) );
	m_ResourceLists[nIndex].m_Resources[nType][nSubIndex].Set( pBuf );
}

void CResourceAccessControl::LimitAccess( ResourceList_t hResourceList )
{
	if ( !res_restrict_access.GetInt() )
		return;

	int nIndex = FindOrAddCurrentThreadID();
	if ( ( m_pLimitAccess[nIndex] >= 0 ) && ( hResourceList != RESOURCE_LIST_INVALID ) )
	{
		Warning( "Attempted to limit access while already limiting access!\n" );
		return;
	}

	m_pLimitAccess[nIndex] = size_cast< int >( (intp)hResourceList );
}

bool CResourceAccessControl::IsAccessAllowed( ResourceTypeOld_t nType, const char *pResourceName )
{
	int nIndex = FindOrAddCurrentThreadID();
	if ( m_pLimitAccess[nIndex] < 0 )
		return true;

	int hResourceList = m_pLimitAccess[nIndex];
	if ( ContainsResource( (ResourceList_t)( intp )hResourceList, nType, pResourceName ) )
		return true;

	COMPILE_TIME_ASSERT( ARRAYSIZE( s_pResourceTypeName ) == RESOURCE_TYPE_OLD_COUNT );
	Warning( "Access to %s resource \"%s\" denied. Missing precache in %s?\n",
		s_pResourceTypeName[nType], pResourceName, m_ResourceLists[hResourceList].m_DebugName.Get() );

	return false;
}
