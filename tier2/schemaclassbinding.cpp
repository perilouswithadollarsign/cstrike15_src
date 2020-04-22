//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "tier2/tier2.h"
#include "resourcefile/schema.h"
#include "resourcesystem/iresourcesystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

CSchemaClassBindingBase *CSchemaClassBindingBase::sm_pClassBindingList;

void CSchemaClassBindingBase::Install()
{
	for ( CSchemaClassBindingBase *pBinding = sm_pClassBindingList; pBinding; pBinding = pBinding->m_pNextBinding )
	{
		g_pResourceSystem->InstallSchemaClassBinding( pBinding );
	}
}

const CResourceStructIntrospection *CSchemaClassBindingBase::GetIntrospection() const
{
	if ( m_pIntrospection == NULL )
	{
		m_pIntrospection = g_pResourceSystem->FindStructIntrospection( m_pClassName );
	}

	return m_pIntrospection;
}
