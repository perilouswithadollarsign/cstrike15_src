//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: gameeventmanager.h: interface for the CGameEventManager class.
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//

#if !defined ( GAMEEVENTMANAGER_H )
#define GAMEEVENTMANAGER_H

#ifdef _WIN32
#pragma once
#endif 

#include <igameevents.h>
#include <utlvector.h>
#include <keyvalues.h>
#include <networkstringtabledefs.h>
#include <utlsymbol.h>
#include <utldict.h>
#include "netmessages.h"

class CSVCMsg_GameEventList;
class CCLCMsg_ListenEvents;

class CGameEventCallback
{
public:
	void				*m_pCallback;		// callback pointer
	int					m_nListenerType;	// client or server side ?
};

class CGameEventDescriptor
{
public:
	CGameEventDescriptor()
	{
		eventid = -1;
		keys = NULL;
		local = false;
		reliable = true;
		elementIndex = -1;

		numSerialized = 0;
		numUnSerialized = 0;
		totalSerializedBits = 0;
		totalUnserializedBits = 0;
	}

public:
	int			eventid;	// network index number, -1 = not networked
	int			elementIndex;
	KeyValues	*keys;		// KeyValue describing data types, if NULL only name 
    CUtlVector<CGameEventCallback*>	listeners;	// registered listeners
	bool		local;		// local event, never tell clients about that
	bool		reliable;	// send this event as reliable message

	// Extra data for network monitoring
	int numSerialized;
	int numUnSerialized;
	int totalSerializedBits;
	int totalUnserializedBits;
};

class CGameEvent : public IGameEvent
{
public:
	CGameEvent( CGameEventDescriptor *descriptor, const char *name );
	virtual ~CGameEvent();

	virtual const char *GetName() const OVERRIDE;
	virtual bool  IsEmpty(const char *keyName = NULL) const OVERRIDE;
	virtual bool  IsLocal() const OVERRIDE;
	virtual bool  IsReliable() const OVERRIDE;

	virtual bool  GetBool( const char *keyName = NULL, bool defaultValue = false ) const OVERRIDE;
	virtual int   GetInt( const char *keyName = NULL, int defaultValue = 0 ) const OVERRIDE;
	virtual uint64 GetUint64( const char *keyName = NULL, uint64 defaultValue = 0 ) const OVERRIDE;
	virtual float GetFloat( const char *keyName = NULL, float defaultValue = 0.0f ) const OVERRIDE;
	virtual const char *GetString( const char *keyName = NULL, const char *defaultValue = "" ) const OVERRIDE;
	virtual const wchar_t *GetWString( const char *keyName = NULL, const wchar_t *defaultValue = L"" ) const OVERRIDE;
	virtual const void *GetPtr( const char *keyName = NULL ) const OVERRIDE;

	virtual void SetBool( const char *keyName, bool value ) OVERRIDE;
	virtual void SetInt( const char *keyName, int value ) OVERRIDE;
	virtual void SetUint64( const char *keyName, uint64 value ) OVERRIDE;
	virtual void SetFloat( const char *keyName, float value ) OVERRIDE;
	virtual void SetString( const char *keyName, const char *value ) OVERRIDE;
	virtual void SetWString( const char *keyName, const wchar_t *value ) OVERRIDE;
	virtual void SetPtr( const char *keyName, const void * value ) OVERRIDE;

	virtual bool ForEventData( IGameEventVisitor2* visitor ) const OVERRIDE;

	CGameEventDescriptor	*m_pDescriptor;
	KeyValues				*m_pDataKeys;
};


// NOTE: Every non-threadsafe externally-callable function must lock m_mutex before modifying the event manager
// member variables.  Client & server threads can call into this class simultaneously
class CGameEventManager : public IGameEventManager2
{
	friend class CGameEventManagerOld;

public:	// IGameEventManager functions

	enum
	{
		SERVERSIDE = 0,		// this is a server side listener, event logger etc
		CLIENTSIDE,			// this is a client side listenet, HUD element etc
		CLIENTSTUB,			// this is a serverside stub for a remote client listener (used by engine only)
		SERVERSIDE_OLD,		// legacy support for old server event listeners
		CLIENTSIDE_OLD,		// legecy support for old client event listeners
	};

	enum
	{
		TYPE_LOCAL = 0,	// not networked
		TYPE_STRING,	// zero terminated ASCII string
		TYPE_FLOAT,		// float 32 bit
		TYPE_LONG,		// signed int 32 bit
		TYPE_SHORT,		// signed int 16 bit
		TYPE_BYTE,		// unsigned int 8 bit
		TYPE_BOOL,		// unsigned int 1 bit
		TYPE_UINT64,	// unsigned int 64 bit
		TYPE_WSTRING,	// zero terminated wide char string
		TYPE_COUNT, 
	};

	CGameEventManager();
	virtual ~CGameEventManager();
	
	int	 LoadEventsFromFile( const char * filename );
	void Reset();
			
	bool AddListener( IGameEventListener2 *listener, const char *name, bool bServerSide );
	bool FindListener( IGameEventListener2 *listener, const char *name );
	void RemoveListener( IGameEventListener2 *listener);

	virtual bool AddListenerGlobal( IGameEventListener2 *listener, bool bServerSide );
		
	IGameEvent *CreateEvent( const char *name, bool bForce = false, int *pCookie = NULL );
	IGameEvent *DuplicateEvent( IGameEvent *event);
	bool FireEvent( IGameEvent *event, bool bDontBroadcast = false );
	bool FireEventClientSide( IGameEvent *event );
	void FreeEvent( IGameEvent *event );

	bool SerializeEvent( IGameEvent *event, CSVCMsg_GameEvent *eventMsg );
	IGameEvent *UnserializeEvent( const CSVCMsg_GameEvent& eventMsg );

	virtual KeyValues* GetEventDataTypes( IGameEvent* event );

	void DumpEventNetworkStats();

public:
	bool Init();
	void Shutdown();
	void ReloadEventDefinitions();	// called by server on new map
	bool AddListener( void *listener, CGameEventDescriptor *descriptor, int nListenerType );

    CGameEventDescriptor *GetEventDescriptor( const char *name, int *pCookie = NULL );
	CGameEventDescriptor *GetEventDescriptor( IGameEvent *event );
	CGameEventDescriptor *GetEventDescriptor( int eventid );

	void WriteEventList(CSVCMsg_GameEventList *msg);
	bool ParseEventList(const CSVCMsg_GameEventList& msg);

	void WriteListenEventList(CCLCMsg_ListenEvents *msg);
	bool HasClientListenersChanged( bool bReset = true );
	void ConPrintEvent( IGameEvent *event);
	
	// legacy support 
	bool AddListenerAll( void *listener, int nListenerType );
	void RemoveListenerOld( void *listener);
	
	// Debug!
	void VerifyListenerList( void );
	
protected:

	IGameEvent *CreateEvent( CGameEventDescriptor *descriptor, const char *name );
	bool RegisterEvent( KeyValues * keys );
	void UnregisterEvent(int index);
	bool FireEventIntern( IGameEvent *event, bool bServerSide, bool bClientOnly );
	CGameEventCallback* FindEventListener( void* listener );
	
	CUtlVector<CGameEventDescriptor>	m_GameEvents;	// list of all known events
	CUtlVector<CGameEventCallback*>		m_Listeners;	// list of all registered listeners
	CUtlSymbolTable						m_EventFiles;	// list of all loaded event files
	CUtlVector<CUtlSymbol>				m_EventFileNames; 
	CUtlDict<int, int>					m_EventMap;
	CThreadFastMutex					m_mutex;		// lock this when modifying the event table

	bool	m_bClientListenersChanged;	// true every time client changed listeners
};

extern CGameEventManager &g_GameEventManager;

#endif 
