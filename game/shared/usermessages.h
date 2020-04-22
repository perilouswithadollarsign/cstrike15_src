//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef USERMESSAGES_H
#define USERMESSAGES_H
#ifdef _WIN32
#pragma once
#endif

#include <utldict.h>
#include <utlvector.h>
#include <bitbuf.h>
#include <utldelegate.h>
#include "shareddefs.h"

#include "networksystem/inetworksystem.h"

// eliminates a conflict with TYPE_BOOL in OSX
#ifdef TYPE_BOOL
#undef TYPE_BOOL
#endif

#include "cstrike15_usermessages.pb.h"

//-----------------------------------------------------------------------------
class IUserMessageBinder;

//-----------------------------------------------------------------------------
// Purpose: Interface for registering and dispatching usermessages
// Shred code creates same ordered list on client/server
//-----------------------------------------------------------------------------
class CUserMessages
{
public:

	CUserMessages();
	~CUserMessages();

	bool DispatchUserMessage( int msg_type, int32 nPassthroughFlags, int size, const void *msg );
	void BindMessage( IUserMessageBinder *pMessageBinder );
	bool UnbindMessage( IUserMessageBinder *pMessageBinder );

private:
	typedef CCopyableUtlVector< IUserMessageBinder * > UserMessageBinderVec_t;
	typedef CUtlMap< int, CCopyableUtlVectorFixed< UserMessageBinderVec_t, MAX_SPLITSCREEN_PLAYERS > > UserMessageHandlerMap_t;
	UserMessageHandlerMap_t m_UserMessageBinderMap;
};

extern CUserMessages *UserMessages();

//-----------------------------------------------------------------------------
class IUserMessageBinder
{
public:
	virtual ~IUserMessageBinder() = 0;
	virtual int GetType() const = 0;
	virtual ::google::protobuf::Message *Parse( int32 nPassthroughFlags, const void *msg, int size, bool &bSilentIgnore ) = 0;
	virtual bool Invoke( ::google::protobuf::Message const *msg ) = 0;
	virtual const CUtlAbstractDelegate &GetAbstractDelegate() = 0;
};

//-----------------------------------------------------------------------------
class CUserMessageBinder
{
public:
	CUserMessageBinder()
		: m_pBind( NULL )
	{
	}

	~CUserMessageBinder()
	{
		delete m_pBind;
	}

	template< int msgType, typename PB_OBJECT_TYPE >
	void Bind( CUtlDelegate< bool ( const PB_OBJECT_TYPE & obj ) > handler )
	{
		delete m_pBind;
		m_pBind = new BindParams< msgType, PB_OBJECT_TYPE, 0 >( handler );
	}

	template< int msgType, typename PB_OBJECT_TYPE >
	void BindRealtimePassthrough( CUtlDelegate< bool( const PB_OBJECT_TYPE & obj ) > handler )
	{
		delete m_pBind;
		m_pBind = new BindParams< msgType, PB_OBJECT_TYPE, 1 >( handler );
	}

	void Unbind()
	{
		if ( m_pBind )
		{
			delete m_pBind;
			m_pBind = NULL;
		}
	}
	bool IsBound() const
	{
		return m_pBind != NULL;
	}

private:
	template< int msgType, typename PB_OBJECT_TYPE, int32 nExpectedPassthroughInReplay >
	struct BindParams : public IUserMessageBinder
	{
		BindParams( CUtlDelegate< bool ( PB_OBJECT_TYPE const &msg ) > handler )
			: m_handler( handler )
		{
			UserMessages()->BindMessage( this );
		}

		virtual ~BindParams()
		{
			UserMessages()->UnbindMessage( this );
		}

		virtual int GetType() const
		{
			return msgType;
		}

		virtual ::google::protobuf::Message *Parse( int32 nPassthroughFlags, const void *msg, int size, bool &bSilentIgnore ) OVERRIDE
		{
			if ( size < 0 || size > NET_MAX_PAYLOAD )
			{
				return NULL;
			}

			extern int CL_GetHltvReplayDelay();
			if ( CL_GetHltvReplayDelay() )
			{
				if ( nPassthroughFlags != nExpectedPassthroughInReplay )
				{
					// this is a wrong timeline message. Ignore it.
					bSilentIgnore = true;
					return NULL;
				}
			}

			::google::protobuf::Message *pMsg = new PB_OBJECT_TYPE();

			if ( !pMsg->ParseFromArray( msg, size ) )
			{
				delete pMsg;
				return NULL;
			}

			return pMsg;
		}


		virtual bool Invoke( ::google::protobuf::Message const *msg )
		{
			if ( msg )
			{
				return m_handler( static_cast< PB_OBJECT_TYPE const & >( *msg ) );
			}
			return false;
		}

		virtual const CUtlAbstractDelegate &GetAbstractDelegate()
		{
			return m_handler.GetAbstractDelegate();
		}

		CUtlDelegate< bool ( PB_OBJECT_TYPE const &obj ) > m_handler;
	};

	IUserMessageBinder *m_pBind;
};

#endif // USERMESSAGES_H
