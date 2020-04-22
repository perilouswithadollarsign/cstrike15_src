//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Templates for use in response_types_marhsal.h
//			Placed here just to keep code a little bit cleaner.
//
// $NoKeywords: $
//=============================================================================//

#ifndef RESPONSE_TYPES_MARSHAL_WRAPPERS_H
#define RESPONSE_TYPES_MARSHAL_WRAPPERS_H
#pragma once

#include "utlcontainer_cli.h"

/// Some (unpublished) template specializations for the above.
extern ResponseRules::ResponseParams *CopyResponseParams( ResponseRules::ResponseParams *pSourceNativeParams  );
template < > 
NativeTypeCopyWrapper< ResponseRules::ResponseParams >::NativeTypeCopyWrapper( ResponseRules::ResponseParams *pSourceNativeParams, bool bCopy )
{
	m_bIsCopy = bCopy;
	if ( bCopy )
		m_pNative = CopyResponseParams( pSourceNativeParams );
	else
		m_pNative = pSourceNativeParams;
}

#endif