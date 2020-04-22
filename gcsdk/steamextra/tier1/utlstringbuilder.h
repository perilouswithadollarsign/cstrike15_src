//====== Copyright © Valve Corporation, All rights reserved. ==================
//
// Purpose: String class thats more suited to frequent modification/appends
//			than CUtlString. Copied from Steam's tier1 utlstring.h instead of
//			a full utlstring.h merge because the files differed nearly 100%.
//
//=============================================================================

#ifndef UTLSTRINGBUILDER_H
#define UTLSTRINGBUILDER_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlmemory.h"
#include "tier1/strtools.h"
#include "tier1/utlstring.h"
#if 0
#include "limits.h"
#include "tier1/utlbinaryblock.h"
#endif


//-----------------------------------------------------------------------------
// Data and memory validation
//-----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
inline void CUtlStringBuilder::Validate( CValidator &validator, const char *pchName )
{
#ifdef _WIN32
	validator.Push( typeid(*this).raw_name(), this, pchName );
#else
	validator.Push( typeid(*this).name(), this, pchName );
#endif

	if ( m_data.IsHeap() )
		validator.ClaimMemory( Access() );

	validator.Pop();
}
#endif // DBGFLAG_VALIDATE


#endif // UTLSTRINGBUILDER_H
