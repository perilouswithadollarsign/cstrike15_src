//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef ENTS_SHARED_H
#define ENTS_SHARED_H

#include <protocol.h>
#include <iserver.h>
#include "clientframe.h"
#include "packed_entity.h"
#include "iclientnetworkable.h"

#ifdef _WIN32
#pragma once
#endif

typedef intp SerializedEntityHandle_t;

enum
{
	SERIALIZED_ENTITY_HANDLE_INVALID = (SerializedEntityHandle_t)0,
};

abstract_class ISerializedEntities
{
public:
	virtual SerializedEntityHandle_t AllocateSerializedEntity( char const *pFile, int nLine ) = 0;
	virtual void ReleaseSerializedEntity( SerializedEntityHandle_t handle ) = 0;
	virtual SerializedEntityHandle_t CopySerializedEntity( SerializedEntityHandle_t handle, char const *pFile, int nLine ) = 0;
};

extern ISerializedEntities *g_pSerializedEntities;

enum
{
	ENTITY_SENTINEL = 9999	// larger number than any real entity number
};

// Used to classify entity update types in DeltaPacketEntities.
enum UpdateType
{
	EnterPVS = 0,	// Entity came back into pvs, create new entity if one doesn't exist

	LeavePVS,		// Entity left pvs

	DeltaEnt,		// There is a delta for this entity.
	PreserveEnt,	// Entity stays alive but no delta ( could be LOD, or just unchanged )

	Finished,		// finished parsing entities successfully
	Failed,			// parsing error occured while reading entities
};

// Flags for delta encoding header
enum
{
	FHDR_ZERO			= 0x0000,
	FHDR_LEAVEPVS		= 0x0001,
	FHDR_DELETE			= 0x0002,
	FHDR_ENTERPVS		= 0x0004,
};


class CEntityInfo
{
public:

	CEntityInfo() {
		m_nOldEntity = -1;
		m_nNewEntity = -1;
		m_nHeaderBase = -1;
	}
	virtual	~CEntityInfo() {};
	
	CClientFrame	*m_pFrom;
	CClientFrame	*m_pTo;


	int				m_nOldEntity;	// current entity index in m_pFrom
	int				m_nNewEntity;	// current entity index in m_pTo

	int				m_nHeaderBase;
	int				m_nHeaderCount;

	UpdateType		m_UpdateType;
	bool			m_bAsDelta;
	inline void	NextOldEntity( void ) 
	{
		if ( m_pFrom )
		{
			m_nOldEntity = m_pFrom->transmit_entity.FindNextSetBit( m_nOldEntity+1 );

			if ( m_nOldEntity < 0 )
			{
				// Sentinel/end of list....
				m_nOldEntity = ENTITY_SENTINEL;
			}
		}
		else
		{
			m_nOldEntity = ENTITY_SENTINEL;
		}
	}

	inline int GetNextOldEntity( int startEntity ) 
	{
		if ( m_pFrom )
		{
			int nextEntity = m_pFrom->transmit_entity.FindNextSetBit( startEntity+1 );

			if ( nextEntity < 0 )
			{
				// Sentinel/end of list....
				nextEntity = ENTITY_SENTINEL;
			}
			return nextEntity;
		}
		else
		{
			return ENTITY_SENTINEL;
		}
	}

	inline void	NextNewEntity( void ) 
	{
		m_nNewEntity = m_pTo->transmit_entity.FindNextSetBit( m_nNewEntity+1 );

		if ( m_nNewEntity < 0 )
		{
			// Sentinel/end of list....
			m_nNewEntity = ENTITY_SENTINEL;
		}
	}
};

// PostDataUpdate calls are stored in a list until all ents have been updated.
class CPostDataUpdateCall
{
public:
	int					m_iEnt;
	DataUpdateType_t	m_UpdateType;
};


// Passed around the read functions.
class CEntityReadInfo : public CEntityInfo
{

public:

	CEntityReadInfo() 
	{	m_nPostDataUpdateCalls = 0;
		m_nLocalPlayerBits = 0;
		m_nOtherPlayerBits = 0;
		m_UpdateType = PreserveEnt;
		m_DecodeEntity = g_pSerializedEntities->AllocateSerializedEntity( __FILE__, __LINE__ );
	}

	~CEntityReadInfo()
	{
		g_pSerializedEntities->ReleaseSerializedEntity( m_DecodeEntity );
	}

	SerializedEntityHandle_t m_DecodeEntity;
	
	bf_read			*m_pBuf;
	int				m_UpdateFlags;	// from the subheader
	bool			m_bIsEntity;

	int				m_nBaseline;	// what baseline index do we use (0/1)
	bool			m_bUpdateBaselines; // update baseline while parsing snaphsot
		
	int				m_nLocalPlayerBits; // profiling data
	int				m_nOtherPlayerBits; // profiling data

	CPostDataUpdateCall	m_PostDataUpdateCalls[MAX_EDICTS];
	int					m_nPostDataUpdateCalls;
};

#endif // ENTS_SHARED_H
