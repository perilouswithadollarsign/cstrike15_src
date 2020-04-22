//======= Copyright © 1996-2006, Valve Corporation, All rights reserved. ======
//
// Purpose:
//
//=============================================================================

#ifndef VALVEMAYA_H
#define VALVEMAYA_H

#if defined( _WIN32 )
#pragma once
#endif

// std includes

#include <ostream>


// Valve Includes

#include "tier1/stringpool.h"
#include "tier1/utlstring.h"
#include "tier1/utlstringmap.h"
#include "tier1/utlvector.h"
#include "tier1/interface.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IMayaVGui;


//-----------------------------------------------------------------------------
//
// minfo, mwarn & merr are ostreams which can be used to send stuff to
// the Maya history window
//
//-----------------------------------------------------------------------------

extern std::ostream minfo;
extern std::ostream mwarn;
extern std::ostream merr;


//-----------------------------------------------------------------------------
// Maya-specific library singletons
//-----------------------------------------------------------------------------
extern IMayaVGui *g_pMayaVGui;


//-----------------------------------------------------------------------------
//
// Purpose: Group a bunch of functions into the Valve Maya Namespace
//
//-----------------------------------------------------------------------------


namespace ValveMaya
{

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool ConnectLibraries( CreateInterfaceFn factory );
void DisconnectLibraries();


}  // end namespace ValveMaya


// Make an alias for the ValveMaya namespace

namespace vm = ValveMaya;


#endif // VALVEMAYA_H
