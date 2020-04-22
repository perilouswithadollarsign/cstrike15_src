//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef VGUI_ILOCALIZE_H
#define VGUI_ILOCALIZE_H

#ifdef _WIN32
#pragma once
#endif

#include "localize/ilocalize.h"

// Everything moved to localize lib; this is here for backward compat.
namespace vgui
{
// direct references to localized strings
typedef uint32 StringIndex_t;
const uint32 INVALID_STRING_INDEX = (uint32) -1;

abstract_class ILocalize : public ::ILocalize
{
public:
	virtual const char *FindAsUTF8( const char *tokenName ) = 0;
};

}; // namespace vgui

#endif // VGUI_ILOCALIZE_H
