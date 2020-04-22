//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// GameEventManager.cpp: implementation of the CGameEventManager class.
//
//////////////////////////////////////////////////////////////////////

#include "GameEventManager.h"
#include "filesystem_engine.h"
#include "server.h"
#include "client.h"
#include "tier0/vprof.h"
#include "cl_splitscreen.h"
#include "tier1/tokenset.h"
#include "gameeventtransmitter.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define NETWORKED_TYPE_BITS 4

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

static CGameEventManager s_GameEventManager;
CGameEventManager &g_GameEventManager = s_GameEventManager;

static const tokenset_t< int > s_GameListenerTypeMap[] =
{						 
	{ "SERVERSIDE",     CGameEventManager::SERVERSIDE     }, // this is a server side listener, event logger etc                             
	{ "CLIENTSIDE",     CGameEventManager::CLIENTSIDE     }, // this is a client side listenet, HUD element etc                              
	{ "CLIENTSTUB",     CGameEventManager::CLIENTSTUB     }, // this is a serverside stub for a remote client listener (used by engine only) 
	{ "SERVERSIDE_OLD", CGameEventManager::SERVERSIDE_OLD }, // legacy support for old server event listeners                                
	{ "CLIENTSIDE_OLD", CGameEventManager::CLIENTSIDE_OLD }, // legecy support for old client event listeners                                
	{ NULL,             -1                                }
};

static const tokenset_t< int > s_GameEventTypesMap[] =
{
	{ "local",    CGameEventManager::TYPE_LOCAL   }, // 0 : don't network this field     
	{ "string",   CGameEventManager::TYPE_STRING  }, // 1 : zero terminated ASCII string 
	{ "float",    CGameEventManager::TYPE_FLOAT   }, // 2 : float 32 bit                 
	{ "long",     CGameEventManager::TYPE_LONG    }, // 3 : signed int 32 bit            
	{ "short",    CGameEventManager::TYPE_SHORT   }, // 4 : signed int 16 bit            
	{ "byte",     CGameEventManager::TYPE_BYTE    }, // 5 : unsigned int 8 bit           
	{ "bool",     CGameEventManager::TYPE_BOOL    }, // 6 : unsigned int 1 bit           
	{ "uint64",   CGameEventManager::TYPE_UINT64  }, // 7 : unsigned int 64 bit           
	{ "wstring",  CGameEventManager::TYPE_WSTRING }, // 8 : zero terminated wide char string 
	{ NULL,       -1                             }
};

static ConVar net_showevents( "net_showevents", "0", 0, "Dump game events to console (1=client only, 2=all)." );
static ConVar net_showeventlisteners( "net_showeventlisteners", "0", 0, "Show listening addition/removals" );

// Expose CVEngineServer to the engine.

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameEventManager, IGameEventManager2, INTERFACEVERSION_GAMEEVENTSMANAGER2, s_GameEventManager );

CGameEvent::CGameEvent( CGameEventDescriptor *descriptor, const char *name )
{
	Assert( descriptor );

	m_pDescriptor = descriptor;
	m_pDataKeys = new KeyValues( name );
	m_pDataKeys->SetInt( "splitscreenplayer", GET_ACTIVE_SPLITSCREEN_SLOT() );
}

CGameEvent::~CGameEvent()
{
	m_pDataKeys->deleteThis();
}

bool CGameEvent::GetBool( const char *keyName, bool defaultValue) const
{
	return m_pDataKeys->GetInt( keyName, defaultValue ) != 0;
}

int CGameEvent::GetInt( const char *keyName, int defaultValue) const
{
	return m_pDataKeys->GetInt( keyName, defaultValue );
}

uint64 CGameEvent::GetUint64( const char *keyName, uint64 defaultValue) const
{
	return m_pDataKeys->GetUint64( keyName, defaultValue );
}

float CGameEvent::GetFloat( const char *keyName, float defaultValue ) const
{
	return m_pDataKeys->GetFloat( keyName, defaultValue );
}

const char *CGameEvent::GetString( const char *keyName, const char *defaultValue ) const
{
	return m_pDataKeys->GetString( keyName, defaultValue );
}

const void *CGameEvent::GetPtr( const char *keyName ) const
{
	Assert( IsLocal() );
	return m_pDataKeys->GetPtr( keyName );
}

const wchar_t *CGameEvent::GetWString( const char *keyName, const wchar_t *defaultValue ) const
{
	return m_pDataKeys->GetWString( keyName, defaultValue );
}


void CGameEvent::SetBool( const char *keyName, bool value )
{
	m_pDataKeys->SetInt( keyName, value?1:0 );
}

void CGameEvent::SetInt( const char *keyName, int value )
{
	m_pDataKeys->SetInt( keyName, value );
}

void CGameEvent::SetUint64( const char *keyName, uint64 value )
{
	m_pDataKeys->SetUint64( keyName, value );
}

void CGameEvent::SetFloat( const char *keyName, float value )
{
	m_pDataKeys->SetFloat( keyName, value );
}

void CGameEvent::SetString( const char *keyName, const char *value )
{
	m_pDataKeys->SetString( keyName, value );
}

void CGameEvent::SetPtr( const char *keyName, const void *value )
{
	Assert( IsLocal() );
	m_pDataKeys->SetPtr( keyName, const_cast< void *>( value ) );
}

void CGameEvent::SetWString( const char* keyName, const wchar_t* value )
{
	m_pDataKeys->SetWString( keyName, value );
}

bool CGameEvent::IsEmpty( const char *keyName ) const
{
	return m_pDataKeys->IsEmpty( keyName );
}

const char *CGameEvent::GetName() const
{
	return m_pDataKeys->GetName();
}

bool CGameEvent::IsLocal() const
{
	return m_pDescriptor->local;
}

bool CGameEvent::IsReliable() const
{
	return m_pDescriptor->reliable;
}

bool CGameEvent::ForEventData( IGameEventVisitor2* visitor ) const
{
	CGameEventDescriptor *descriptor = m_pDescriptor;

	bool iterate = true;
	for ( KeyValues* key = descriptor->keys->GetFirstSubKey(); key && iterate; key = key->GetNextKey() )
	{
		const char * keyName = key->GetName();

		int type = key->GetInt();

		// see s_GameEventTypesMap for index
		switch ( type )
		{
		case CGameEventManager::TYPE_LOCAL: iterate = visitor->VisitLocal( keyName, GetPtr( keyName ) ); break;
		case CGameEventManager::TYPE_STRING: iterate = visitor->VisitString( keyName, GetString( keyName, "" ) ); break;
		case CGameEventManager::TYPE_FLOAT: iterate = visitor->VisitFloat( keyName, GetFloat( keyName, 0.0f ) ); break;
		case CGameEventManager::TYPE_LONG: iterate = visitor->VisitInt( keyName, GetInt( keyName, 0 ) ); break;
		case CGameEventManager::TYPE_SHORT: iterate = visitor->VisitInt( keyName, GetInt( keyName, 0 ) ); break;
		case CGameEventManager::TYPE_BYTE: iterate = visitor->VisitInt( keyName, GetInt( keyName, 0 ) ); break;
		case CGameEventManager::TYPE_BOOL: iterate = visitor->VisitBool( keyName, GetBool( keyName, false ) ); break;
		case CGameEventManager::TYPE_UINT64: iterate = visitor->VisitUint64( keyName, GetUint64( keyName, 0 ) ); break;
		case CGameEventManager::TYPE_WSTRING: iterate = visitor->VisitWString( keyName, GetWString( keyName, L"" ) ); break;
		}
	}

	return iterate;
}

CGameEventManager::CGameEventManager()
{
	Reset();
}

CGameEventManager::~CGameEventManager()
{
	Reset();
}

bool CGameEventManager::Init()
{
	Reset();

	LoadEventsFromFile( "resource/serverevents.res" );

	g_GameEventTransmitter.Init();

	return true;
}

void CGameEventManager::Shutdown()
{
	Reset();
}

void CGameEventManager::Reset()
{
	AUTO_LOCK_FM( m_mutex );
	int number = m_GameEvents.Count();

	for (int i = 0; i<number; i++)
	{
		CGameEventDescriptor &e = m_GameEvents.Element( i );

		if ( e.keys )
		{
			e.keys->deleteThis(); // free the value keys
			e.keys = NULL;
		}

		e.listeners.Purge();	// remove listeners
	}

	m_GameEvents.Purge();
	m_Listeners.PurgeAndDeleteElements();
	m_EventFiles.RemoveAll();
	m_EventFileNames.RemoveAll();
	m_EventMap.Purge();
	m_bClientListenersChanged = true;
	
	Assert( m_GameEvents.Count() == 0 );
}

bool CGameEventManager::HasClientListenersChanged( bool bReset /* = true  */)
{
	if ( !m_bClientListenersChanged )
		return false;

	if ( bReset )
		m_bClientListenersChanged = false;

	return true;
}

void CGameEventManager::WriteEventList(CSVCMsg_GameEventList *msg)
{
	for (int i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor &descriptor = m_GameEvents[i];

		if ( descriptor.local )
			continue;

		Assert( descriptor.eventid >= 0 && descriptor.eventid < MAX_EVENT_NUMBER );

		CSVCMsg_GameEventList::descriptor_t *pDescriptor = msg->add_descriptors();
		const char *pName = m_EventMap.GetElementName( descriptor.elementIndex );

		pDescriptor->set_eventid( descriptor.eventid );
		pDescriptor->set_name( pName );

		KeyValues *key = descriptor.keys->GetFirstSubKey(); 

		while ( key )
		{
			int type = key->GetInt();

			if ( type != TYPE_LOCAL )
			{
				CSVCMsg_GameEventList::key_t *pKey = pDescriptor->add_keys();

				pKey->set_type( type );
				pKey->set_name( key->GetName() );
			}

			key = key->GetNextKey();
		}
	}
}

bool CGameEventManager::ParseEventList(const CSVCMsg_GameEventList& msg)
{
	int i;
	AUTO_LOCK_FM( m_mutex );

	// reset eventids to -1 first
	for ( i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor &descriptor = m_GameEvents[i];
		descriptor.eventid = -1;
	}

	// map server event IDs 
	int nNumEvents = msg.descriptors_size();
	for (i = 0; i<nNumEvents; i++)
	{
		const CSVCMsg_GameEventList::descriptor_t& EventDescriptor = msg.descriptors( i );

		const char *name = EventDescriptor.name().c_str();

		CGameEventDescriptor *descriptor = GetEventDescriptor( name );

		// if event is known to client...
		if ( descriptor )
		{
			descriptor->eventid = EventDescriptor.eventid();

			// remove old definition list
			if ( descriptor->keys )
				descriptor->keys->deleteThis();

			descriptor->keys = new KeyValues("descriptor");

			int nNumKeys = EventDescriptor.keys_size();
			for (int key = 0; key < nNumKeys; key++)
			{
				const CSVCMsg_GameEventList::key_t &Key = EventDescriptor.keys( key );

				descriptor->keys->SetInt( Key.name().c_str(), Key.type() );
			}
		}
	}

	// force client to answer what events he listens to
	m_bClientListenersChanged = true;

	return true;
}
	
void CGameEventManager::WriteListenEventList(CCLCMsg_ListenEvents *msg)
{
	CBitVec<MAX_EVENT_NUMBER> EventArray;

	EventArray.ClearAll();

	// and know tell the server what events we want to listen to
	for (int i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor &descriptor = m_GameEvents[i];

		if ( descriptor.local )
		{
			continue; // event isn't networked
		}

		bool bHasClientListener = false;

		for ( int j=0; j<descriptor.listeners.Count(); j++ )
		{
			CGameEventCallback *listener = descriptor.listeners[j];

			if ( listener->m_nListenerType == CGameEventManager::CLIENTSIDE ||
				listener->m_nListenerType == CGameEventManager::CLIENTSIDE_OLD	)
			{
				// if we have a client side listener and server knows this event, add it
				bHasClientListener = true;
				break;
			}
		}

		if ( !bHasClientListener )
			continue;

		if ( descriptor.eventid == -1 )
		{
			const char *pName = m_EventMap.GetElementName(descriptor.elementIndex);
			DevMsg("Warning! Client listens to event '%s' unknown by server.\n", pName );
			continue;
		}

		EventArray.Set( descriptor.eventid );
	}

	int count = ( m_GameEvents.Count() + 31 ) / 32;
	for( int i = 0; i < count; i++ )
	{
		msg->add_event_mask( EventArray.GetDWord( i ) );
	}
}

IGameEvent *CGameEventManager::CreateEvent( CGameEventDescriptor *descriptor, const char *name )
{
	AUTO_LOCK_FM( m_mutex );
	return new CGameEvent ( descriptor, name );
}

IGameEvent *CGameEventManager::CreateEvent( const char *name, bool bForce, int *pCookie )
{
	AUTO_LOCK_FM( m_mutex );
	if ( !name || !name[0] )
		return NULL;

	CGameEventDescriptor *descriptor = GetEventDescriptor( name, pCookie );

	// check if this event name is known
	if ( !descriptor )
	{
		DevMsg( "CreateEvent: event '%s' not registered.\n", name );
		return NULL;
	}

	// event is known but no one listen to it
	if ( descriptor->listeners.Count() == 0 && !bForce )
	{
		return NULL;
	}

	// create & return the new event 
	return new CGameEvent ( descriptor, name );
}

ConVar display_game_events("display_game_events", "0", FCVAR_CHEAT );
bool CGameEventManager::FireEvent( IGameEvent *event, bool bServerOnly )
{
	if( display_game_events.GetBool() )
	{
		Msg("Game Event Fired: %s\n", event->GetName() );
	}

	return FireEventIntern( event, bServerOnly, false );
}

bool CGameEventManager::FireEventClientSide( IGameEvent *event )
{
	return FireEventIntern( event, false, true );
}

IGameEvent *CGameEventManager::DuplicateEvent( IGameEvent *event )
{
	CGameEvent *gameEvent = dynamic_cast<CGameEvent*>(event);

	if ( !gameEvent )
		return NULL;

	// create new instance
	const char *pName = m_EventMap.GetElementName(gameEvent->m_pDescriptor->elementIndex );
	CGameEvent *newEvent = new CGameEvent ( gameEvent->m_pDescriptor, pName );

	// free keys
	newEvent->m_pDataKeys->deleteThis();

	// and make copy
	newEvent->m_pDataKeys = gameEvent->m_pDataKeys->MakeCopy();

	return newEvent;
}

void CGameEventManager::ConPrintEvent( IGameEvent *event)
{
	CGameEventDescriptor *descriptor = GetEventDescriptor( event );

	if ( !descriptor )
		return;

	KeyValues *key = descriptor->keys->GetFirstSubKey(); 

	while ( key )
	{
		const char * keyName = key->GetName();

		int type = key->GetInt();

		switch ( type )
		{
		case TYPE_LOCAL : ConMsg( "- \"%s\" = \"%s\" (local)\n", keyName, event->GetString(keyName) ); break;
		case TYPE_STRING : ConMsg( "- \"%s\" = \"%s\"\n", keyName, event->GetString(keyName) ); break;
		case TYPE_WSTRING : ConMsg( "- \"%s\" = \"" PRI_WS_FOR_S "\"\n", keyName, event->GetWString(keyName) ); break;
		case TYPE_FLOAT : ConMsg( "- \"%s\" = \"%.2f\"\n", keyName, event->GetFloat(keyName) ); break;
		default: ConMsg( "- \"%s\" = \"%i\"\n", keyName, event->GetInt(keyName) ); break;
		}
		key = key->GetNextKey();
	}
}

bool CGameEventManager::FireEventIntern( IGameEvent *event, bool bServerOnly, bool bClientOnly )
{
	AUTO_LOCK_FM( m_mutex );
	if ( event == NULL )
		return false;

	Assert( !(bServerOnly && bClientOnly) ); // it can't be both

	VPROF_("CGameEventManager::FireEvent", 1, VPROF_BUDGETGROUP_OTHER_UNACCOUNTED, false, 
		bClientOnly ? BUDGETFLAG_CLIENT : ( bServerOnly ? BUDGETFLAG_SERVER : BUDGETFLAG_OTHER ) );

	CGameEventDescriptor *descriptor = GetEventDescriptor( event );

	if ( descriptor == NULL )
	{
		DevMsg( "FireEvent: event '%s' not registered.\n", event->GetName() );
		FreeEvent( event );
		return false;
	}

	// show game events in console
	if ( net_showevents.GetInt() > 0 )
	{
		if ( bClientOnly )
		{
#ifndef DEDICATED
			const char *pName = m_EventMap.GetElementName(descriptor->elementIndex);
			ConMsg( "Game event \"%s\", Tick %i:\n", pName, GetBaseLocalClient().GetClientTickCount() );
			ConPrintEvent( event );
#endif
		}
		else if ( net_showevents.GetInt() > 1 )
		{
			const char *pName = m_EventMap.GetElementName(descriptor->elementIndex);
			ConMsg( "Server event \"%s\", Tick %i:\n", pName, sv.GetTick() );
			ConPrintEvent( event );
		}

		
	}

	for ( int i = 0; i < descriptor->listeners.Count(); i++ )
	{
		CGameEventCallback *listener = descriptor->listeners.Element( i );
		
		Assert ( listener );

		// don't trigger server listners for clientside only events
		if ( ( listener->m_nListenerType == SERVERSIDE ||
			   listener->m_nListenerType == SERVERSIDE_OLD ) &&
			   bClientOnly  )
			continue;

        // don't trigger clientside events, if not explicit a clientside event
		if ( ( listener->m_nListenerType == CLIENTSIDE ||
			   listener->m_nListenerType == CLIENTSIDE_OLD ) &&  
			   !bClientOnly  )
			continue;

		// don't broadcast events if server side only
		if ( listener->m_nListenerType == CLIENTSTUB && (bServerOnly || bClientOnly) )
			continue;

		// if the event is local, don't tell clients about it
		if ( listener->m_nListenerType == CLIENTSTUB && descriptor->local )
			continue;

		// TODO optimized the serialize event for clients, call only once and not per client

		// fire event in this listener module
		if ( listener->m_nListenerType == CLIENTSIDE_OLD ||
			 listener->m_nListenerType == SERVERSIDE_OLD )
		{
			// legacy support for old system
			IGameEventListener *pCallback = static_cast<IGameEventListener*>(listener->m_pCallback);
			CGameEvent *pEvent = static_cast<CGameEvent*>(event);

			pCallback->FireGameEvent( pEvent->m_pDataKeys );
		}
		else
		{
			// new system
			IGameEventListener2 *pCallback =  static_cast<IGameEventListener2*>(listener->m_pCallback);
			Assert( pCallback );
			if ( pCallback )
			{			
				if ( pCallback->GetEventDebugID() != EVENT_DEBUG_ID_INIT )
				{
					Msg( "GameEventListener2 callback in list that should NOT be - %s!\n", event->GetName() );
					Assert( 0 );
				}
				else
				{
					pCallback->FireGameEvent( event );
				}
			}
			else
			{
				Warning( "Callback for event \"%s\" is NULL!!!\n", event->GetName() );
			}
		}	 
	}

	if ( bClientOnly || ( !Q_stricmp( "portal2", COM_GetModDirectory() ) ) )
	{
		// Pass all client only events  to the game event transmiter
		g_GameEventTransmitter.TransmitGameEvent( event );
	}

	// free event resources
	FreeEvent( event );

	return true;
}

bool CGameEventManager::SerializeEvent( IGameEvent *event, CSVCMsg_GameEvent *eventMsg )
{
	AUTO_LOCK_FM( m_mutex );
	CGameEventDescriptor *descriptor = GetEventDescriptor( event );

	Assert( descriptor );

	eventMsg->set_eventid( descriptor->eventid );

	// now iterate trough all fields described in gameevents.res and put them in the buffer

	KeyValues * key = descriptor->keys->GetFirstSubKey();

	if ( net_showevents.GetInt() > 2 )
	{
		const char *pName = m_EventMap.GetElementName(descriptor->elementIndex);
		DevMsg("Serializing event '%s' (%i):\n", pName, descriptor->eventid );
	}

	while ( key )
	{
		const char * keyName = key->GetName();

		int type = key->GetInt();

		if ( net_showevents.GetInt() > 2 )
		{
			DevMsg(" - %s (%s)\n", keyName, s_GameEventTypesMap->GetNameByToken( type ) );
		}

		if( type != TYPE_LOCAL )
		{
			CSVCMsg_GameEvent::key_t *pKey = eventMsg->add_keys();

			//Make sure every key is used in the event
			// Assert( event->FindKey(keyName) && "GameEvent field not found in passed KeyValues" );

			pKey->set_type( type );

			// see s_GameEventTypesMap for index
			switch ( type )
			{
			case TYPE_STRING: pKey->set_val_string( event->GetString( keyName, "") ); break;
			case TYPE_FLOAT : pKey->set_val_float( event->GetFloat( keyName, 0.0f) ); break;
			case TYPE_LONG	: pKey->set_val_long( event->GetInt( keyName, 0) ); break;
			case TYPE_SHORT	: pKey->set_val_short( event->GetInt( keyName, 0) ); break;
			case TYPE_BYTE	: pKey->set_val_byte( event->GetInt( keyName, 0) ); break;
			case TYPE_BOOL	: pKey->set_val_bool( !!event->GetInt( keyName, 0) ); break;
			case TYPE_UINT64: pKey->set_val_uint64( event->GetUint64( keyName, 0) ); break;				
			case TYPE_WSTRING: 
				{
					const wchar_t *pStr = event->GetWString( keyName, L"");
					pKey->set_val_wstring( pStr, wcslen( pStr ) + 1 );
				}
				break;				
			default: DevMsg(1, "CGameEventManager: unknown type %i for key '%s'.\n", type, key->GetName() ); break;
			}
		}

		key = key->GetNextKey();
	}

	if ( net_showevents.GetInt() > 2 )
	{
		int nBytes = eventMsg->ByteSize();
		Msg( " took %d bits, %d bytes\n", nBytes * 8, nBytes );
	}

	descriptor->numSerialized++;
	descriptor->totalSerializedBits += eventMsg->ByteSize() * 8;

	return true;
}

IGameEvent *CGameEventManager::UnserializeEvent( const CSVCMsg_GameEvent& eventMsg )
{
	AUTO_LOCK_FM( m_mutex );

	// read event id

	int eventid = eventMsg.eventid();

	// get event description
	CGameEventDescriptor *descriptor = GetEventDescriptor( eventid );

	if ( descriptor == NULL )
	{
		DevMsg( "CGameEventManager::UnserializeEvent:: unknown event id %i.\n", eventid );
		return NULL;
	}

	// create new event
	const char *pName = m_EventMap.GetElementName(descriptor->elementIndex);
	IGameEvent *event = CreateEvent( descriptor, pName );

	if ( !event )
	{
		DevMsg( "CGameEventManager::UnserializeEvent:: failed to create event %s.\n", pName );
		return NULL;
	}

	int nNumKeys = eventMsg.keys_size();
	KeyValues * key = descriptor->keys->GetFirstSubKey(); 

	for( int i = 0; key && ( i < nNumKeys ); i++, key = key->GetNextKey() )
	{
		const CSVCMsg_GameEvent::key_t &KeyEvent = eventMsg.keys( i );
		const char * keyName = key->GetName();

		int type = KeyEvent.type();

		switch ( type )
		{
		case TYPE_LOCAL		: break; // ignore 
		case TYPE_STRING	: event->SetString( keyName, KeyEvent.val_string().c_str() ); break;
		case TYPE_FLOAT		: event->SetFloat( keyName, KeyEvent.val_float() ); break;
		case TYPE_LONG		: event->SetInt( keyName, KeyEvent.val_long() ); break;
		case TYPE_SHORT		: event->SetInt( keyName, KeyEvent.val_short() ); break;
		case TYPE_BYTE		: event->SetInt( keyName, KeyEvent.val_byte() ); break;
		case TYPE_BOOL		: event->SetInt( keyName, KeyEvent.val_bool() ); break;
		case TYPE_UINT64	: event->SetUint64( keyName, KeyEvent.val_uint64() ); break;
		case TYPE_WSTRING	: event->SetWString( keyName, (wchar_t*)KeyEvent.val_wstring().data() ); break;

		default: DevMsg(1, "CGameEventManager: unknown type %i for key '%s' [%s].\n", type, key->GetName(), pName ); break;
		}
	}

	descriptor->numUnSerialized++;
	descriptor->totalUnserializedBits += eventMsg.ByteSize() * 8;

	return event;
}

KeyValues* CGameEventManager::GetEventDataTypes( IGameEvent* event )
{
	if ( event == nullptr )
		return nullptr;

	CGameEventDescriptor* descriptor = GetEventDescriptor( event );
	if ( descriptor == nullptr )
		return nullptr;

	return descriptor->keys;
}

// returns true if this listener is listens to given event
bool CGameEventManager::FindListener( IGameEventListener2 *listener, const char *name )
{
	AUTO_LOCK_FM( m_mutex );
	CGameEventDescriptor *pDescriptor = GetEventDescriptor( name );

	if ( !pDescriptor )
		return false; // event is unknown

	CGameEventCallback *pCallback = FindEventListener( listener );

	if ( !pCallback )
		return false; // listener is unknown

	// see if listener is in the list for this event
	return pDescriptor->listeners.IsValidIndex( pDescriptor->listeners.Find( pCallback ) );
}

CGameEventCallback* CGameEventManager::FindEventListener( void* pCallback )
{
	for (int i=0; i < m_Listeners.Count(); i++ )
	{
		CGameEventCallback *listener = m_Listeners.Element(i);

		if ( listener->m_pCallback == pCallback )
		{
			return listener;
		}
	}

	return NULL;
}

void CGameEventManager::RemoveListener(IGameEventListener2 *listener)
{
	AUTO_LOCK_FM( m_mutex );
	CGameEventCallback *pCallback = FindEventListener( listener );
	
	if ( pCallback == NULL )
	{
		return;
	}

	// remove reference from events 
	for (int i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor &et = m_GameEvents.Element( i );
		et.listeners.FindAndRemove( pCallback );
	}

	// and from global list
	m_Listeners.FindAndRemove( pCallback );

	if ( pCallback->m_nListenerType == CLIENTSIDE )
	{
		m_bClientListenersChanged = true;
	}

	delete pCallback;
}

bool CGameEventManager::AddListenerGlobal( IGameEventListener2 *listener, bool bServerSide )
{
	AUTO_LOCK_FM( m_mutex );

	if ( !listener )
		return false;

	for ( int i = 0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor *descriptor = &m_GameEvents[ i ];

		AddListener( listener, descriptor, bServerSide ? SERVERSIDE : CLIENTSIDE );
	}

	return true;
}

int CGameEventManager::LoadEventsFromFile( const char * filename )
{
	AUTO_LOCK_FM( m_mutex );
	if ( UTL_INVAL_SYMBOL == m_EventFiles.Find( filename ) )
	{
		CUtlSymbol id = m_EventFiles.AddString( filename );
		m_EventFileNames.AddToTail( id );
	}

	KeyValues * key = new KeyValues(filename);
	KeyValues::AutoDelete autodelete_key( key );

	if  ( !key->LoadFromFile( g_pFileSystem, filename, "GAME" ) )
		return false;

	int count = 0;	// number new events

	KeyValues * subkey = key->GetFirstSubKey();

	while ( subkey )
	{
		if ( subkey->GetDataType() == KeyValues::TYPE_NONE )
		{
			RegisterEvent( subkey );
			count++;
		}

		subkey = subkey->GetNextKey();
	}

	if ( net_showevents.GetBool() )
		DevMsg( "Event System loaded %i events from file %s.\n", m_GameEvents.Count(), filename );

	return m_GameEvents.Count();
}

void CGameEventManager::ReloadEventDefinitions()
{
	for ( int i=0; i< m_EventFileNames.Count(); i++ )
	{
		const char *filename = m_EventFiles.String( m_EventFileNames[i] );
		LoadEventsFromFile( filename );
	}

	// we are the server, build string table now
	int number = m_GameEvents.Count();

	for (int j = 0; j<number; j++)
	{
		m_GameEvents[j].eventid = j;
	}
}

bool CGameEventManager::AddListener( IGameEventListener2 *listener, const char *event, bool bServerSide )
{
	AUTO_LOCK_FM( m_mutex );
	if ( !event )
		return false;

	// look for the event descriptor
	CGameEventDescriptor *descriptor = GetEventDescriptor( event );

	if ( !descriptor )
	{
		Warning( "CGameEventManager::AddListener: event '%s' unknown.\n", event );
		return false;	// that should not happen
	}

	return AddListener( listener, descriptor, bServerSide ? SERVERSIDE : CLIENTSIDE );
}

bool CGameEventManager::AddListener( void *listener, CGameEventDescriptor *descriptor,  int nListenerType )
{
	AUTO_LOCK_FM( m_mutex );
	if ( !listener || !descriptor )
		return false;	// bahh

	// check if we already know this listener
	CGameEventCallback *pCallback = FindEventListener( listener );

	if ( pCallback == NULL )
	{
		// add new callback 
		pCallback = new CGameEventCallback;
		m_Listeners.AddToTail( pCallback );

		pCallback->m_nListenerType = nListenerType;
		pCallback->m_pCallback = listener;
	}
	else
	{
		// make sure that it hasn't changed:
		Assert( pCallback->m_nListenerType == nListenerType	 );
		Assert( pCallback->m_pCallback == listener );
	}

	// add to event listeners list if not already in there
	if ( descriptor->listeners.Find( pCallback ) == descriptor->listeners.InvalidIndex() )
	{
		descriptor->listeners.AddToTail( pCallback );

		if ( net_showeventlisteners.GetBool() )
		{
			const char *name = m_EventMap.GetElementName( descriptor->elementIndex );
			Msg("[GAMEEVENT] Event '%s' added %s listener %p\n", 
				name ? name : "UNKNOWN", 
				s_GameListenerTypeMap->GetNameByToken( nListenerType ),
				listener );
		}

		if ( nListenerType == CLIENTSIDE || nListenerType == CLIENTSIDE_OLD )
			m_bClientListenersChanged = true;
	}

	return true;
}


bool CGameEventManager::RegisterEvent( KeyValues * event)
{
	if ( event == NULL )
		return false;

	AUTO_LOCK_FM( m_mutex );
	if ( m_GameEvents.Count() == MAX_EVENT_NUMBER )
	{
		DevMsg( "CGameEventManager: couldn't register event '%s', limit reached (%i).\n",
			event->GetName(), MAX_EVENT_NUMBER );
		return false;
	}

	const char *name = event->GetName();

	CGameEventDescriptor *descriptor = GetEventDescriptor( name );

	if ( !descriptor )
	{
		// event not known yet, create new one
		int index = m_GameEvents.AddToTail();
		descriptor =  &m_GameEvents.Element(index);

		descriptor->elementIndex = m_EventMap.Insert( event->GetName(), index );
	}
	else
	{
		// descriptor already know, but delete old definitions
		descriptor->keys->deleteThis();
	}

	// create new descriptor keys
	descriptor->keys = new KeyValues("descriptor");
	
	KeyValues *subkey = event->GetFirstSubKey();

	// interate through subkeys

	while ( subkey )
	{
		const char *keyName = subkey->GetName();

		// ok, check it's data type
		const char * type = subkey->GetString();

		if ( !Q_strcmp( "local", keyName) )
		{	
			descriptor->local = Q_atoi( type ) != 0;
		}
		else if ( !Q_strcmp( "reliable", keyName) )
		{
			descriptor->reliable = Q_atoi( type ) != 0;
		}
		else
		{
			int i = s_GameEventTypesMap->GetToken( type );

			if ( i < 0 )
			{
				descriptor->keys->SetInt( keyName, 0 );	// unknown
				DevMsg( "CGameEventManager:: unknown type '%s' for key '%s' [%s].\n", type, subkey->GetName(), name );
			}
			else
			{
				// set data type
				descriptor->keys->SetInt( keyName, i );	// set data type
			}
		}
		
		subkey = subkey->GetNextKey();
	}
	
	return true;
}

CGameEventDescriptor *CGameEventManager::GetEventDescriptor(IGameEvent *event)
{
	CGameEvent *gameevent = dynamic_cast<CGameEvent*>(event);

	if ( !gameevent )
		return NULL;

	return gameevent->m_pDescriptor;
}

CGameEventDescriptor *CGameEventManager::GetEventDescriptor(int eventid) // returns event name or NULL
{
	if ( eventid < 0 )
		return NULL;

	for ( int i = 0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor *descriptor = &m_GameEvents[i];

		if ( descriptor->eventid == eventid )
			return descriptor;
	}

	return NULL;
}

void CGameEventManager::FreeEvent( IGameEvent *event )
{
	AUTO_LOCK_FM( m_mutex );
	if ( !event )
		return;

	delete event;
}

CGameEventDescriptor *CGameEventManager::GetEventDescriptor(const char * name, int *pCookie)
{
	const uint32 cookieBit = 0x80000000;
	const uint32 cookieMask = ~cookieBit;

	if ( !name || !name[0] )
		return NULL;

	if ( pCookie && *pCookie )
	{
		int gameEventIndex = uint32(*pCookie) & cookieMask;
		CGameEventDescriptor *pDescriptor = &m_GameEvents[gameEventIndex];
		if ( !V_stricmp( m_EventMap.GetElementName(pDescriptor->elementIndex), name ) )
		{
			return pDescriptor;
		}
	}
	int eventMapIndex = m_EventMap.Find( name );
	if ( eventMapIndex == m_EventMap.InvalidIndex() )
		return NULL;
	int gameEventIndex = m_EventMap[eventMapIndex];
	if ( pCookie )
	{
		*pCookie = cookieBit | gameEventIndex;
	}
	return &m_GameEvents[gameEventIndex];
}

bool CGameEventManager::AddListenerAll( void *listener, int nListenerType )
{
	AUTO_LOCK_FM( m_mutex );
	if ( !listener )
		return false;

	for (int i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor *descriptor = &m_GameEvents[i];

		AddListener( listener, descriptor, nListenerType );
	}

	DevMsg("Warning! Game event listener registerd for all events. Use newer game event interface.\n");

	return true;
}

void CGameEventManager::RemoveListenerOld( void *listener)
{
	AUTO_LOCK_FM( m_mutex );
	CGameEventCallback *pCallback = FindEventListener( listener );

	if ( pCallback == NULL )
	{
		DevMsg("RemoveListenerOld: couldn't find listener\n");
		return;
	}

	// remove reference from events 
	for (int i=0; i < m_GameEvents.Count(); i++ )
	{
		CGameEventDescriptor &et = m_GameEvents.Element( i );
		et.listeners.FindAndRemove( pCallback );
	}

	// and from global list
	m_Listeners.FindAndRemove( pCallback );

	if ( pCallback->m_nListenerType == CLIENTSIDE_OLD )
	{
		m_bClientListenersChanged = true;
	}

	delete pCallback;
}

void CGameEventManager::VerifyListenerList( void )
{
	int nGameEventCount = m_GameEvents.Count();
	for ( int iEvent = 0; iEvent < nGameEventCount; ++iEvent )
	{
		CGameEventDescriptor *pEvent = &m_GameEvents.Element( iEvent );
		const char *pName = m_EventMap.GetElementName( iEvent );
		if ( !pEvent )
		{
			Msg( "VerifyListenerList-Bug: Bad event in list! (%d)\n", iEvent);
			continue;
		}

		int nListenerCount = pEvent->listeners.Count();
		for ( int iListen = 0; iListen < nListenerCount; ++iListen )
		{
			CGameEventCallback *pListener = pEvent->listeners.Element( iListen );
			if ( !pListener)
			{
				Msg( "VerifyListenerList-Bug: Bad listener in list! (%s)\n", pName );
				continue;
			}

			IGameEventListener2 *pCallback = static_cast<IGameEventListener2*>( pListener->m_pCallback );
			if ( pCallback->GetEventDebugID() != EVENT_DEBUG_ID_INIT )
			{
				Msg( "VerifyListenerList-Bug: Bad callback in list! (%s)\n", pName );
				continue;
			}
		}		
	}
}

void CGameEventManager::DumpEventNetworkStats( void )
{
	// find longest name
	int len = 0;
	for ( int i = 0; i < m_GameEvents.Count(); ++i )
	{
		CGameEventDescriptor &e = m_GameEvents.Element( i );
		const char *name = m_EventMap.GetElementName( e.elementIndex );
		if ( !name )
		{
			continue;
		}

		int l = Q_strlen( name );
		if ( l > len )
		{
			len = l;
		}
	}

	//        ----- ----- ------- ------- ------- ------- 
	Msg( "%*s  Out    In  OutBits InBits  OutSize InSize  Notes\n", len, "Name" );
	//            %5d %5d %7d %7d %7d %7d 


	for ( int i = 0; i < m_GameEvents.Count(); ++i )
	{
		CGameEventDescriptor &e = m_GameEvents.Element( i );
		const char *name = m_EventMap.GetElementName( e.elementIndex );
		if ( !name )
		{
			continue;
		}

		if ( !e.numSerialized && !e.numUnSerialized )
		{
			continue;
		}

		Msg( "%*s %5d %5d %7d %7d %7d %7d",
			 len,
			 name,
			 e.numSerialized,
			 e.numUnSerialized,
			 e.totalSerializedBits,
			 e.totalUnserializedBits,
			 e.numSerialized ? e.totalSerializedBits / e.numSerialized : 0,
			 e.numUnSerialized ? e.totalUnserializedBits / e.numUnSerialized : 0 );
		if ( e.local )
		{
			Msg( " local" );
		}
		if ( e.reliable )
		{
			Msg( " reliable" );
		}
		Msg( "\n" );
	}

}


CON_COMMAND_F( net_dumpeventstats, "Dumps out a report of game event network usage", 0 )
{
	s_GameEventManager.DumpEventNetworkStats();
}
