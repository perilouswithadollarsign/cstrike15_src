//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_USER_MESSAGE_REGISTER_H
#define C_USER_MESSAGE_REGISTER_H
#ifdef _WIN32
#pragma once
#endif


#include "usermessages.h"

// This provides an alternative to HOOK_MESSAGE, where you can declare it globally
// instead of finding a place to run it.
// It registers a function called __MsgFunc_<msgName>
#define USER_MESSAGE_REGISTER( msgName ) \
	static CUserMessageRegister< CS_UM_##msgName, CCSUsrMsg_##msgName > userMessageRegister_##msgName( __MsgFunc_##msgName );

class CUserMessageRegisterBase
{
public:
	CUserMessageRegisterBase( );

	// This is called at startup to register all the user messages.
	static void RegisterAll();

	virtual void Register() = 0;

private:

	// Linked list of all the CUserMessageRegisters.
	static CUserMessageRegisterBase *s_pHead;
	CUserMessageRegisterBase *m_pNext;
};

template< int msgType, typename PB_OBJECT_TYPE >
class CUserMessageRegister : public CUserMessageRegisterBase 
{
public:
	typedef bool (*pfnHandler )( const PB_OBJECT_TYPE& );

	explicit CUserMessageRegister( pfnHandler msgFunc )
	{
		m_pMsgFunc = msgFunc;
	}

	// This is called at startup to register all the user messages.
	void Register()
	{
		m_msgBinder.Bind< msgType, PB_OBJECT_TYPE >( UtlMakeDelegate( m_pMsgFunc ));
	}
	
	CUserMessageBinder m_msgBinder;
	pfnHandler m_pMsgFunc;
};

#endif // C_USER_MESSAGE_REGISTER_H
