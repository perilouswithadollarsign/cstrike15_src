//=========== (C) Copyright Valve, L.L.C. All rights reserved. ===========

#ifndef UI_NUGGET_H
#define UI_NUGGET_H

#include "game_controls/igameuisystemmgr.h"
#include "matchmaking/imatchframework.h"
#include "fmtstr.h"
#include "utlstringmap.h"

class CUiNuggetBase;
class CUiNuggetReference;
class CUiNuggetFactoryRegistrarBase;
class CUiNuggetFactoryRegistrarBaseInstances;
class CUiNuggetFactoryRegistrarBaseSingleton;

//////////////////////////////////////////////////////////////////////////
//
// Base class for implementing UI nuggets
//

class CUiNuggetBase : public IGameUIScreenController
{
public:
	CUiNuggetBase();
	virtual ~CUiNuggetBase();

	// IGameUIScreenController
public:
	// Connects a screen to the controller, returns number of
	// remaining connected screens (or 1 for the first connection)
	virtual int OnScreenConnected( IGameUISystem *pScreenView );

	// Releases the screen from controller, returns number of
	// remaining connected screens (returns 0 if no screens are
	// connected - new object must be reacquired from factory
	// in this case)
	virtual int OnScreenDisconnected( IGameUISystem *pScreenView );

	// Callback for screen events handling
	virtual KeyValues * OnScreenEvent( IGameUISystem *pScreenView, KeyValues *kvEvent );

	// Broadcast an event to all connected screens (caller retains ownership of keyvalues)
	virtual void BroadcastEventToScreens( KeyValues *kvEvent );

public:
	// Add a reference to the nugget to be notified upon release
	virtual void AddReferenceSink( CUiNuggetReference *pSink ) { m_arrReferences.AddToTail( pSink ); }

protected:
	// Policy for whether the object should be deleted when no screen references remain
	virtual bool ShouldDeleteOnLastScreenDisconnect() { return true; }

protected:
	struct ConnectionInfo_t
	{
		IGameUISystem *m_pScreen;
		int m_nRefCount;

		explicit ConnectionInfo_t( IGameUISystem *pScreen = NULL ) : m_pScreen( pScreen ), m_nRefCount( 0 ) {}
		bool operator == ( ConnectionInfo_t const &other ) const { return m_pScreen == other.m_pScreen; }
	};
	typedef CUtlVector< ConnectionInfo_t > ConnectedScreens;
	ConnectedScreens m_arrConnectedScreens;
	CUtlVector< int > m_arrEventsDisabledScreenHandles;

	KeyValues *m_pUiNuggetData;
	KeyValues::AutoDelete m_autodelete_m_pUiNuggetData;

private:
	CUtlVector< int * > m_arrBroadcastEventIdxArray;
	CUtlVector< CUiNuggetReference * > m_arrReferences;
};

//////////////////////////////////////////////////////////////////////////
//
// Declaration of UI nuggets factories
//

class CUiNuggetFactoryRegistrarBase : public IGameUIScreenControllerFactory
{
public:
	CUiNuggetFactoryRegistrarBase();
	~CUiNuggetFactoryRegistrarBase();

	virtual void Register();

	virtual char const *GetName() = 0;

public:
	static void RegisterAll();

private:
	CUiNuggetFactoryRegistrarBase *m_pPrev, *m_pNext;
};

class CUiNuggetFactoryRegistrarBaseGlobalInstance : public CUiNuggetFactoryRegistrarBase
{
public:
	// Returns an instance of a controller interface
	virtual IGameUIScreenController * GetController( KeyValues *kvRequest ) = 0;

	// Access controller instances
	virtual int GetControllerInstancesCount() { return 1; }
	virtual IGameUIScreenController * GetControllerInstance( int iIndex ) = 0;
};

class CUiNuggetFactoryRegistrarBaseSingleton : public CUiNuggetFactoryRegistrarBase
{
	friend class CUiNuggetFactoryRegistrarBaseSingletonReferenceTracker;
public:
	CUiNuggetFactoryRegistrarBaseSingleton();

public:
	// Returns an instance of a controller interface
	virtual IGameUIScreenController * GetController( KeyValues *kvRequest );

	// Access controller instances
	virtual int GetControllerInstancesCount() { return !!m_pSingleton; }
	virtual IGameUIScreenController * GetControllerInstance( int iIndex ) { return m_pSingleton; }

public:
	// Creates an instance of a controller interface
	virtual CUiNuggetBase * CreateNewController() = 0;

protected:
	CUiNuggetBase *m_pSingleton;
};

class CUiNuggetFactoryRegistrarBaseInstances : public CUiNuggetFactoryRegistrarBase
{
	friend class CUiNuggetFactoryRegistrarBaseInstancesReferenceTracker;
public:
	// Returns an instance of a controller interface
	virtual IGameUIScreenController * GetController( KeyValues *kvRequest );

	// Access controller instances
	virtual int GetControllerInstancesCount() { return m_arrInstances.Count(); }
	virtual IGameUIScreenController * GetControllerInstance( int iIndex ) { return m_arrInstances.IsValidIndex( iIndex ) ? m_arrInstances[iIndex] : NULL; }

public:
	// Creates an instance of a controller interface
	virtual CUiNuggetBase * CreateNewController() = 0;

protected:
	// Nugget instances
	CUtlVector< CUiNuggetBase * > m_arrInstances;
};


//////////////////////////////////////////////////////////////////////////
//
// Macros to be used to declare UI nuggets factories
//

// Global instance factory - a nugget instance always exists in a global variable
// and is always shared with all screens.
#define UI_NUGGET_FACTORY_GLOBAL_INSTANCE( nuggetclassname, instanceptr, scriptname ) \
	namespace { \
		class Factory_##nuggetclassname##_Class : public CUiNuggetFactoryRegistrarBaseGlobalInstance \
		{ \
			virtual IGameUIScreenController * GetController( KeyValues *kvRequest ) { return static_cast< nuggetclassname * >( instanceptr ); } \
			virtual IGameUIScreenController * GetControllerInstance( int iIndex )   { return static_cast< nuggetclassname * >( instanceptr ); } \
			virtual char const * GetName() { return scriptname; } \
		} \
		g_factory_##nuggetclassname##_globalinstance; \
	};

// Singleton factory - create a new nugget instance and share it with all screens
// until all references to the nugget are released and nugget is destroyed.
// If nugget policy is not deleting the nugget upon last release, then a single nugget
// instance will be created once and shared with all screens.
#define UI_NUGGET_FACTORY_SINGLETON( nuggetclassname, scriptname ) \
	namespace { \
		class Factory_##nuggetclassname##_Class : public CUiNuggetFactoryRegistrarBaseSingleton \
		{ \
			virtual CUiNuggetBase * CreateNewController() { return new nuggetclassname; } \
			virtual char const * GetName() { return scriptname; } \
		} \
		g_factory_##nuggetclassname##_singleton; \
	};

// Instances factory - create a new nugget instance per each request.
// Screens need to implement own methods of sharing data from a single nugget
// instance. Nugget instance must be marked for delete upon last release.
#define UI_NUGGET_FACTORY_INSTANCES( nuggetclassname, scriptname ) \
	namespace { \
		class Factory_##nuggetclassname##_Class : public CUiNuggetFactoryRegistrarBaseInstances \
		{ \
			virtual CUiNuggetBase * CreateNewController() { return new nuggetclassname; } \
			virtual char const * GetName() { return scriptname; } \
		} \
		g_factory_##nuggetclassname##_instances; \
	};


//////////////////////////////////////////////////////////////////////////
//
// Macros to be used to declare nuggets functions
//

#define DECLARE_NUGGET_FN_MAP( classname, baseclass ) \
	typedef classname NuggetEventMapClass; \
	typedef baseclass NuggetEventMapBaseClass; \
	class NuggetEventMap : \
	public CUtlStringMap< KeyValues * (classname::*)( IGameUISystem *pScreenView, KeyValues *args ) > \
	{} \
	m_NuggetEventMap; \
	class NuggetPreBroadcastMap : \
	public CUtlStringMap< bool (classname::*)( KeyValues *args ) > \
	{} \
	m_NuggetPreBroadcastMap; \
	virtual KeyValues * OnScreenEvent( IGameUISystem *pScreenView, KeyValues *args ) { \
		char const *szEvent = args->GetName(); \
		UtlSymId_t sym = m_NuggetEventMap.Find( szEvent ); \
		if ( sym != m_NuggetEventMap.InvalidIndex() ) { \
			return (this->* (m_NuggetEventMap[sym]) )( pScreenView, args ); \
		} \
		return NuggetEventMapBaseClass::OnScreenEvent( pScreenView, args ); \
	} \
	virtual void BroadcastEventToScreens( KeyValues *args ) { \
		char const *szEvent = args->GetName(); \
		UtlSymId_t sym = m_NuggetPreBroadcastMap.Find( szEvent ); \
		if ( sym != m_NuggetPreBroadcastMap.InvalidIndex() ) { \
			if ( ! (this->* (m_NuggetPreBroadcastMap[sym]) )( args ) ) return; \
		} \
		return NuggetEventMapBaseClass::BroadcastEventToScreens( args ); \
	}

#define NUGGET_FN( eventname ) \
	class eventname##_EventRegistrar { \
		public: typedef eventname##_EventRegistrar ThisClass; \
		eventname##_EventRegistrar() { \
		NuggetEventMapClass *pNugget = reinterpret_cast< NuggetEventMapClass * >( reinterpret_cast< size_t >( this ) - offsetof( NuggetEventMapClass, m_##eventname##_EventRegistrar ) ); \
		pNugget->m_NuggetEventMap[ #eventname ] = &NuggetEventMapClass::Event_##eventname; \
		COMPILE_TIME_ASSERT( offsetof( NuggetEventMapClass, m_##eventname##_EventRegistrar ) > offsetof( NuggetEventMapClass, m_NuggetEventMap ) ); \
	} } m_##eventname##_EventRegistrar; \
	KeyValues * Event_##eventname( IGameUISystem *pScreenView, KeyValues *args )


#define NUGGET_BROADCAST_FN( eventname ) \
	class eventname##_BroadcastRegistrar { \
		public: typedef eventname##_BroadcastRegistrar ThisClass; \
		eventname##_BroadcastRegistrar() { \
		NuggetEventMapClass *pNugget = reinterpret_cast< NuggetEventMapClass * >( reinterpret_cast< size_t >( this ) - offsetof( NuggetEventMapClass, m_##eventname##_BroadcastRegistrar ) ); \
		pNugget->m_NuggetPreBroadcastMap[ #eventname ] = &NuggetEventMapClass::Broadcast_##eventname; \
		COMPILE_TIME_ASSERT( offsetof( NuggetEventMapClass, m_##eventname##_BroadcastRegistrar ) > offsetof( NuggetEventMapClass, m_NuggetPreBroadcastMap ) ); \
	} } m_##eventname##_BroadcastRegistrar; \
	bool Broadcast_##eventname( KeyValues *args )


#endif // UI_NUGGET_H
