//=========== Copyright Valve Corporation, All rights reserved. =============//
//
// Purpose: Common location for hard-coded knowledge about module
// bundles, such as tier2_bundle and tier3_bundle.
//
//===========================================================================//

#pragma once

#include "tier0/platform.h"

// Some places in code, such as vconsole2, have an explicit
// list of DLLs to copy for execution.  Make a central point
// to control selecting the right thing based on whether bundles
// are used or not.

#if USE_TIER2_BUNDLE
#define WITH_TIER2_BUNDLE( _Exp ) _Exp
#define WITHOUT_TIER2_BUNDLE( _Exp )
#define WITH_TIER2_BUNDLE_COMMA( _Exp ) _Exp,
#define WITHOUT_TIER2_BUNDLE_COMMA( _Exp )
#else
#define WITH_TIER2_BUNDLE( _Exp )
#define WITHOUT_TIER2_BUNDLE( _Exp ) _Exp
#define WITH_TIER2_BUNDLE_COMMA( _Exp )
#define WITHOUT_TIER2_BUNDLE_COMMA( _Exp ) _Exp,
#endif

#if USE_TIER3_BUNDLE
#define WITH_TIER3_BUNDLE( _Exp ) _Exp
#define WITHOUT_TIER3_BUNDLE( _Exp )
#define WITH_TIER3_BUNDLE_COMMA( _Exp ) _Exp,
#define WITHOUT_TIER3_BUNDLE_COMMA( _Exp )
#else
#define WITH_TIER3_BUNDLE( _Exp )
#define WITHOUT_TIER3_BUNDLE( _Exp ) _Exp
#define WITH_TIER3_BUNDLE_COMMA( _Exp )
#define WITHOUT_TIER3_BUNDLE_COMMA( _Exp ) _Exp,
#endif

// Given a specific module name return the bundled module
// name if the specific module is part of a bundle, otherwise
// return the given module name.
const char *RemapBundledModuleName( const char *pModuleName );
