//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "singleplayersharedmemory.h"
#include "tier1/utlstring.h"

#include "tier0/memdbgon.h"

CSPSharedMemoryManager s_SinglePlayerSharedMemoryManager;
CSPSharedMemoryManager *g_pSinglePlayerSharedMemoryManager = &s_SinglePlayerSharedMemoryManager;

class CSPSharedMemory : public ISPSharedMemory
{
public:
	CSPSharedMemory( void ) : m_pMemory(NULL), m_Size(0), m_iRefCount(0) { };
	~CSPSharedMemory( void );

	virtual bool	Init( size_t iSize ); //Initial implementation assumes the size is fixed/hardcoded
	virtual uint8 *	Base( void );
	virtual size_t	Size( void );

	virtual void	AddRef( void );
	virtual void	Release( void );

	
	CUtlString m_Name;
	int m_iEntNum;
	uint8 *m_pMemory;
	size_t m_Size;
	unsigned int m_iRefCount;
};

CSPSharedMemory::~CSPSharedMemory( void )
{
	Assert( m_iRefCount == 0 );
	
	//if( m_pMemory )
	//	delete []m_pMemory;
}

bool CSPSharedMemory::Init( size_t iSize )
{
	Assert( iSize != 0 );
	Assert( (m_Size == 0) || (m_Size == iSize) );

	if( m_Size == 0 )
	{
		m_pMemory = new uint8 [iSize];
		m_Size = iSize;
		memset( m_pMemory, 0, iSize );
		return true;
	}
	return false;
}

uint8 * CSPSharedMemory::Base( void )
{
	Assert( m_pMemory != NULL );
	return m_pMemory;
}

size_t CSPSharedMemory::Size( void )
{
	return m_Size;
}

void CSPSharedMemory::AddRef( void )
{
	++m_iRefCount;
}

void CSPSharedMemory::Release( void )
{
	--m_iRefCount;
	
	if( m_iRefCount == 0 )
	{
		for( int i = s_SinglePlayerSharedMemoryManager.m_SharedSpaces.Count(); --i >= 0; )
		{
			if( s_SinglePlayerSharedMemoryManager.m_SharedSpaces[i] == this )
			{
				s_SinglePlayerSharedMemoryManager.m_SharedSpaces.FastRemove(i);
				break;
			}
		}
		//delete this;
	}
}




CSPSharedMemoryManager::~CSPSharedMemoryManager( void )
{
	Assert( m_SharedSpaces.Count() == 0 );

	for( int i = m_SharedSpaces.Count(); --i >= 0; )
	{
		Warning( "Shared memory space %s (%i) still has %i references.\n", m_SharedSpaces[i]->m_Name.String(), m_SharedSpaces[i]->m_iEntNum, m_SharedSpaces[i]->m_iRefCount );
		//leak it?
	}
}


ISPSharedMemory *CSPSharedMemoryManager::GetSharedMemory( const char *szName, int ent_num )
{
	for( int i = m_SharedSpaces.Count(); --i >= 0; )
	{
		if( m_SharedSpaces[i]->m_Name == szName && m_SharedSpaces[i]->m_iEntNum == ent_num )
		{
			m_SharedSpaces[i]->AddRef();
			return m_SharedSpaces[i];
		}
	}

	//create a new one
	CSPSharedMemory *pNew = new CSPSharedMemory;
	pNew->m_Name = szName;
	pNew->m_iEntNum = ent_num;
	m_SharedSpaces.AddToTail( pNew );
	pNew->AddRef();
	return pNew;
}
