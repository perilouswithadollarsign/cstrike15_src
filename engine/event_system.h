//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#if !defined( EVENT_SYSTEM_H )
#define EVENT_SYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "event_flags.h"
#include "common.h"
#include "enginesingleuserfilter.h"
#include "ents_shared.h"


class SendTable;
class ClientClass;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CEventInfo
{
public:
	enum
	{
		EVENT_INDEX_BITS = 8,
		EVENT_DATA_LEN_BITS = 11,
		MAX_EVENT_DATA = 192,  // ( 1<<8 bits == 256, but only using 192 below )
	};

	inline CEventInfo()
	{
		classID = 0;
		fire_delay = 0.0f;
		flags = 0;
		pSendTable = NULL;
		pClientClass = NULL;
		m_Packed = SERIALIZED_ENTITY_HANDLE_INVALID;
	}

	~CEventInfo()
	{
		g_pSerializedEntities->ReleaseSerializedEntity( m_Packed );
	}

	CEventInfo( const CEventInfo& src )
	{
		m_Packed = g_pSerializedEntities->CopySerializedEntity( src.m_Packed, __FILE__, __LINE__ );
		classID = src.classID;
		fire_delay = src.fire_delay;
		flags = src.flags;
		pSendTable = src.pSendTable;
		pClientClass = src.pClientClass;
		filter.AddPlayersFromFilter( &src.filter );
	}

	// 0 implies not in use
	short classID;
	
	// If non-zero, the delay time when the event should be fired ( fixed up on the client )
	float fire_delay;

	// send table pointer or NULL if send as full update
	const SendTable *pSendTable;
	const ClientClass *pClientClass;
	
	SerializedEntityHandle_t m_Packed;
	// CLIENT ONLY Reliable or not, etc.
	int		flags;
	
	// clients that see that event
	CEngineRecipientFilter filter;
};


#endif // EVENT_SYSTEM_H
