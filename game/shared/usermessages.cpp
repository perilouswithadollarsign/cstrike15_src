//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "usermessages.h"
#include <bitbuf.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#include "tier0/microprofiler.h"

static ConVar cl_show_usermessage( "cl_show_usermessage", "0", 0, "Shows the incoming user messages for this client and dumps them out the type and size of the messages to the console. Setting this to 2 will display message contents as well" ); // filter incoming voice data

//-----------------------------------------------------------------------------
IUserMessageBinder::~IUserMessageBinder()
{
}


//-----------------------------------------------------------------------------
CUserMessages::CUserMessages()
{
	SetDefLessFunc( m_UserMessageBinderMap );
}

//-----------------------------------------------------------------------------
CUserMessages::~CUserMessages()
{
	m_UserMessageBinderMap.Purge();
}

// we might wanna move this definition here::
// template< int msgType, typename PB_OBJECT_TYPE, int32 nExpectedPassthroughInReplay >
// virtual ::google::protobuf::Message * CUserMessageBinder::BindParams<msgType, PB_OBJECT_TYPE, nExpectedPassthroughInReplay >::Parse( int32 nPassthroughFlags, const void *msg, int size )

//-----------------------------------------------------------------------------
bool CUserMessages::DispatchUserMessage( int msg_type, int32 nPassthroughFlags, int size, const void *msg )
{
	UserMessageHandlerMap_t::IndexType_t index = m_UserMessageBinderMap.Find( msg_type );
	if ( index == UserMessageHandlerMap_t::InvalidIndex() )
	{
		DevMsg( "CUserMessages::DispatchUserMessage:  Unknown msg type %i\n", msg_type );
		return true;
	}
		
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	if ( m_UserMessageBinderMap.Element( index )[ nSlot ].Count() == 0 )
	{
		// not hooking a usermessage is acceptable, pretend we parsed it
		return true;
	}

	IUserMessageBinder *pHandler = m_UserMessageBinderMap.Element( index )[ nSlot ][0];
	if ( !pHandler )
	{
		// not hooking a usermessage is acceptable, pretend we parsed it
		return true;
	}

	bool bSilentIgnore = false;
	::google::protobuf::Message *pMsg = pHandler->Parse( nPassthroughFlags, msg, size, bSilentIgnore );

	if ( bSilentIgnore )
	{
		if ( pMsg )
			delete pMsg;
		//DevMsg( "CUserMessages::DispatchUserMessage: Silently ignoring alt-timeline msg type %i\n", msg_type );
		return true;
	}

	if ( !pMsg )
	{
		DevMsg( "CUserMessages::DispatchUserMessage:  Parse error msg type=%i\n", msg_type );
		Assert( 0 );
		return false;
	}

	//handle logging to the console if this is enabled
	if ( cl_show_usermessage.GetBool() )
	{
		Msg("DispatchUserMessage - %s(%d) bytes: %d\n", pMsg->GetTypeName().c_str(), msg_type, pMsg->ByteSize() );
		//handle message content display if they have it set to a value > 1
		if( cl_show_usermessage.GetInt() > 1 )
			Msg("%s", pMsg->DebugString().c_str() );
	}

	bool result = true;
	FOR_EACH_VEC( m_UserMessageBinderMap.Element( index )[ nSlot ], i )
	{
		IUserMessageBinder *h = m_UserMessageBinderMap.Element( index )[ nSlot ][i];
		if ( h )
		{
			result = h->Invoke( pMsg );
		}

		if ( !result )
		{
			break;
		}
	}

	delete pMsg;

	return result;
}

//-----------------------------------------------------------------------------
void CUserMessages::BindMessage( IUserMessageBinder *pMessageBinder )
{
	if ( !pMessageBinder )
	{
		return;
	}

	int message = pMessageBinder->GetType();

	UserMessageHandlerMap_t::IndexType_t index = m_UserMessageBinderMap.Find( message );
	if ( index == UserMessageHandlerMap_t::InvalidIndex() )
	{
		index = m_UserMessageBinderMap.Insert( message );
		m_UserMessageBinderMap.Element( index ).SetCount( MAX_SPLITSCREEN_PLAYERS );
	}

	ASSERT_LOCAL_PLAYER_RESOLVABLE();

#ifdef DEBUG
	FOR_EACH_VEC( m_UserMessageBinderMap.Element( index )[ GET_ACTIVE_SPLITSCREEN_SLOT() ], j )
	{
		if( m_UserMessageBinderMap.Element( index )[ GET_ACTIVE_SPLITSCREEN_SLOT() ][ j ]->GetAbstractDelegate().IsEqual( pMessageBinder->GetAbstractDelegate() ) )
		{
			DevMsg( "CUserMessages::BindMessage called duplicate %d!!!\n", pMessageBinder->GetType() );
		}
	}
#endif

	m_UserMessageBinderMap.Element( index )[ GET_ACTIVE_SPLITSCREEN_SLOT() ].AddToTail( pMessageBinder );
}

//-----------------------------------------------------------------------------
bool CUserMessages::UnbindMessage( IUserMessageBinder *pMessageBinder )
{
	if ( !pMessageBinder )
	{
		return true;
	}

	int message = pMessageBinder->GetType();

	UserMessageHandlerMap_t::IndexType_t index = m_UserMessageBinderMap.Find( message );
	
	if ( index == UserMessageHandlerMap_t::InvalidIndex() )
	{
		return false;
	}

	for ( int split = 0; split < MAX_SPLITSCREEN_PLAYERS; ++split )
	{
		FOR_EACH_VEC( m_UserMessageBinderMap.Element( index )[ split ], i )
		{
			if ( m_UserMessageBinderMap.Element( index )[ split ][i] == pMessageBinder )
			{
				m_UserMessageBinderMap.Element( index )[ split ].Remove(i);
				return true;
			}
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Singleton
CUserMessages *UserMessages()
{
	static CUserMessages g_UserMessages;
	return &g_UserMessages;
}

