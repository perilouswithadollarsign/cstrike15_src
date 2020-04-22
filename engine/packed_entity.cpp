//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <string.h>
#include <assert.h>
#include "packed_entity.h"
#include "basetypes.h"
#include "changeframelist.h"
#include "dt_send.h"
#include "dt_send_eng.h"
#include "server_class.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// -------------------------------------------------------------------------------------------------- //
// PackedEntity.
// -------------------------------------------------------------------------------------------------- //

PackedEntity::PackedEntity()
{
	m_SerializedEntity = SERIALIZED_ENTITY_HANDLE_INVALID;
	m_pChangeFrameList = NULL;
	m_nSnapshotCreationTick = 0;
	m_nShouldCheckCreationTick = 0;
}

PackedEntity::~PackedEntity()
{
	ReleasePackedData();

	if ( m_pChangeFrameList )
	{
		m_pChangeFrameList->Release();
		m_pChangeFrameList = NULL;
	}
}


void PackedEntity::ReleasePackedData()
{
	if ( SERIALIZED_ENTITY_HANDLE_INVALID != m_SerializedEntity )
	{
		g_pSerializedEntities->ReleaseSerializedEntity( m_SerializedEntity );
		m_SerializedEntity = SERIALIZED_ENTITY_HANDLE_INVALID;
	}
}

void PackedEntity::CopyPackedData( SerializedEntityHandle_t handle )
{
	ReleasePackedData();
	m_SerializedEntity = g_pSerializedEntities->CopySerializedEntity( handle, __FILE__, __LINE__ );
	return;
}

// Like copy, but just takes ownership of handle.  Assumes that parent caller is done with entity and will NOT
// call ReleaseSerializedEntity
void PackedEntity::SetPackedData( SerializedEntityHandle_t handle )
{
	if ( handle == m_SerializedEntity )
		return;
	ReleasePackedData();
	m_SerializedEntity = handle;
	return;
}

const CSendProxyRecipients*	PackedEntity::GetRecipients() const
{
	return m_Recipients.Base();
}


int PackedEntity::GetNumRecipients() const
{
	return m_Recipients.Count();
}


void PackedEntity::SetRecipients( const CUtlMemory<CSendProxyRecipients> &recipients )
{
	m_Recipients.CopyArray( recipients.Base(), recipients.Count() );
}


bool PackedEntity::CompareRecipients( const CUtlMemory<CSendProxyRecipients> &recipients ) const
{
	if ( recipients.Count() != m_Recipients.Count() )
		return false;
	
	return memcmp( recipients.Base(), m_Recipients.Base(), sizeof( CSendProxyRecipients ) * m_Recipients.Count() ) == 0;
}	

void PackedEntity::SetServerAndClientClass( ServerClass *pServerClass, ClientClass *pClientClass )
{
	m_pServerClass = pServerClass;
	m_pClientClass = pClientClass;
	if ( pServerClass )
	{
		Assert( pServerClass->m_pTable );
		SetShouldCheckCreationTick( pServerClass->m_pTable->HasPropsEncodedAgainstTickCount() );
	}
}
