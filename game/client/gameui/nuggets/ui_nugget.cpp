//=========== (C) Copyright Valve, L.L.C. All rights reserved. ===========

#include "cbase.h"
#include "ui_nugget.h"
#include "vstdlib/ikeyvaluessystem.h"


//////////////////////////////////////////////////////////////////////////
//
// CUiNuggetReference implementation
//

class CUiNuggetReference
{
public:
	virtual void OnNuggetReleased( CUiNuggetBase *pNugget ) = 0;
};


//////////////////////////////////////////////////////////////////////////
//
// CUiNuggetBase implementation
//

CUiNuggetBase::CUiNuggetBase() :
	m_pUiNuggetData( new KeyValues( "" ) ),
	m_autodelete_m_pUiNuggetData( m_pUiNuggetData )
{
}

CUiNuggetBase::~CUiNuggetBase()
{
	while ( m_arrReferences.Count() )
	{
		CUiNuggetReference *pSink = m_arrReferences.Head();
		m_arrReferences.RemoveMultipleFromHead( 1 );
		pSink->OnNuggetReleased( this );
	}
}

int CUiNuggetBase::OnScreenConnected( IGameUISystem *pScreenView )
{
	ConnectionInfo_t ci( pScreenView );
	int idx = m_arrConnectedScreens.Find( ci );
	if ( idx != m_arrConnectedScreens.InvalidIndex() )
	{
		++ m_arrConnectedScreens[idx].m_nRefCount;
	}
	else
	{
		ci.m_nRefCount = 1;
		m_arrConnectedScreens.AddToTail( ci );
	}
	return m_arrConnectedScreens.Count();
}

int CUiNuggetBase::OnScreenDisconnected( IGameUISystem *pScreenView )
{
	ConnectionInfo_t ci( pScreenView );
	int idx = m_arrConnectedScreens.Find( ci );
	if ( idx != m_arrConnectedScreens.InvalidIndex() )
	{
		if ( -- m_arrConnectedScreens[idx].m_nRefCount )
			return m_arrConnectedScreens.Count();

		// Otherwise this screen must be released
		m_arrConnectedScreens.Remove( idx );

		// Assert that screen is not disconnecting in the middle of
		// event broadcast (otherwise need to implement index adjustment
		// so that broadcast could succeed)
		Assert( !m_arrBroadcastEventIdxArray.Count() );

		// Check if we need to delete us
		int numRemaining = m_arrConnectedScreens.Count();
		if ( !numRemaining && ShouldDeleteOnLastScreenDisconnect() )
		{
			delete this;
		}
		return numRemaining;
	}
	else
	{
		Warning( "CUiNuggetBase::OnScreenDisconnected %p for not connected screen!\n", pScreenView );
		Assert( !"OnScreenDisconnected" );
		return m_arrConnectedScreens.Count();
	}
}

KeyValues * CUiNuggetBase::OnScreenEvent( IGameUISystem *pScreenView, KeyValues *kvEvent )
{
	int nEvent = kvEvent->GetNameSymbol();

	static int const s_nGetData = KeyValuesSystem()->GetSymbolForString( "GetData" );
	if ( nEvent == s_nGetData )
	{
		return m_pUiNuggetData->MakeCopy();
	}

	static int const s_nEnableEvents = KeyValuesSystem()->GetSymbolForString( "EnableEvents" );
	if ( nEvent == s_nEnableEvents )
	{
		int nScriptHandle = kvEvent->GetInt( "scripthandle" );
		bool bEnabled = kvEvent->GetBool( "enable", true );
		m_arrEventsDisabledScreenHandles.FindAndFastRemove( nScriptHandle );
		if ( !bEnabled )
			m_arrEventsDisabledScreenHandles.AddToTail( nScriptHandle );
		return NULL;
	}

	DevWarning( "CUiNuggetBase(%p)::OnScreenEvent for unknown command %s!\n", this, kvEvent->GetName() );
	return NULL;
}

void CUiNuggetBase::BroadcastEventToScreens( KeyValues *kvEvent )
{
	int idx = 0;
	m_arrBroadcastEventIdxArray.AddToTail( &idx );

	for ( ; idx < m_arrConnectedScreens.Count(); ++ idx )
	{
		IGameUISystem *pUI = m_arrConnectedScreens[idx].m_pScreen;
		int iScriptHandle = pUI->GetScriptHandle();
		if ( m_arrEventsDisabledScreenHandles.Find( iScriptHandle ) != m_arrEventsDisabledScreenHandles.InvalidIndex() )
			continue;
		pUI->ExecuteScript( kvEvent );
	}

	m_arrBroadcastEventIdxArray.RemoveMultipleFromTail( 1 );
}


//////////////////////////////////////////////////////////////////////////
//
// CUiNuggetFactoryRegistrarBase implementation
//

static CUiNuggetFactoryRegistrarBase *g_pUiNuggetFactoriesCircularList = NULL;

CUiNuggetFactoryRegistrarBase::CUiNuggetFactoryRegistrarBase()
{
	// Add us to list
	CUiNuggetFactoryRegistrarBase *pLeft = this;
	CUiNuggetFactoryRegistrarBase *pRight = this;

	if ( g_pUiNuggetFactoriesCircularList )
	{
		pLeft = g_pUiNuggetFactoriesCircularList;
		pRight = g_pUiNuggetFactoriesCircularList->m_pNext;
		
		pLeft->m_pNext = this;
		pRight->m_pPrev = this;
	}

	g_pUiNuggetFactoriesCircularList = this;
	m_pNext = pRight;
	m_pPrev = pLeft;
}

CUiNuggetFactoryRegistrarBase::~CUiNuggetFactoryRegistrarBase()
{
	// Remove us from list
	if ( m_pPrev == this || m_pNext == this )
	{
		Assert( m_pPrev == this && m_pNext == this && g_pUiNuggetFactoriesCircularList == this );
		g_pUiNuggetFactoriesCircularList = NULL;
	}
	else
	{
		m_pPrev->m_pNext = m_pNext;
		m_pNext->m_pPrev = m_pPrev;
		if ( g_pUiNuggetFactoriesCircularList == this )
			g_pUiNuggetFactoriesCircularList = m_pPrev;
	}
}

void CUiNuggetFactoryRegistrarBase::Register()
{
	Assert( g_pGameUISystemMgr );
	g_pGameUISystemMgr->RegisterScreenControllerFactory( GetName(), this );
}

void CUiNuggetFactoryRegistrarBase::RegisterAll()
{
	CUiNuggetFactoryRegistrarBase *p = g_pUiNuggetFactoriesCircularList;
	if ( !p )
		return;

	do
	{
		p->Register();
		p = p->m_pNext;
	}
	while ( p != g_pUiNuggetFactoriesCircularList );
}

//////////////////////////////////////////////////////////////////////////
//
// factories implementation
//

CUiNuggetFactoryRegistrarBaseSingleton::CUiNuggetFactoryRegistrarBaseSingleton() :
	m_pSingleton( NULL )
{
}

class CUiNuggetFactoryRegistrarBaseSingletonReferenceTracker : public CUiNuggetReference
{
public:
	CUiNuggetFactoryRegistrarBaseSingletonReferenceTracker( CUiNuggetBase *pNugget, CUiNuggetFactoryRegistrarBaseSingleton *pFactory )
	{
		m_pFactory = pFactory;
		m_pFactory->m_pSingleton = pNugget;
		pNugget->AddReferenceSink( this );
	}

private:
	virtual void OnNuggetReleased( CUiNuggetBase *pNugget )
	{
		m_pFactory->m_pSingleton = NULL;
		delete this;
	}

private:
	CUiNuggetFactoryRegistrarBaseSingleton *m_pFactory;
};

IGameUIScreenController * CUiNuggetFactoryRegistrarBaseSingleton::GetController( KeyValues *kvRequest )
{
	if ( Q_stricmp( GetName(), kvRequest->GetName() ) )
		return NULL;

	if ( !m_pSingleton )
	{
		m_pSingleton = CreateNewController();
		new CUiNuggetFactoryRegistrarBaseSingletonReferenceTracker( m_pSingleton, this );
	}
	
	return m_pSingleton;
}

class CUiNuggetFactoryRegistrarBaseInstancesReferenceTracker : public CUiNuggetReference
{
public:
	CUiNuggetFactoryRegistrarBaseInstancesReferenceTracker( CUiNuggetBase *pNugget, CUiNuggetFactoryRegistrarBaseInstances *pFactory )
	{
		m_pFactory = pFactory;
		m_pFactory->m_arrInstances.AddToTail( pNugget );
		pNugget->AddReferenceSink( this );
	}

private:
	virtual void OnNuggetReleased( CUiNuggetBase *pNugget )
	{
		m_pFactory->m_arrInstances.FindAndRemove( pNugget );
		delete this;
	}

private:
	CUiNuggetFactoryRegistrarBaseInstances *m_pFactory;
};

IGameUIScreenController * CUiNuggetFactoryRegistrarBaseInstances::GetController( KeyValues *kvRequest )
{
	if ( Q_stricmp( GetName(), kvRequest->GetName() ) )
		return NULL;

	CUiNuggetBase *pInstance = CreateNewController();
	if ( !pInstance )
		return NULL;

	new CUiNuggetFactoryRegistrarBaseInstancesReferenceTracker( pInstance, this );
	return pInstance;
}

