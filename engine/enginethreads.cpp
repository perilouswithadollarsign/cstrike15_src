//========== Copyright © 2005, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "enginethreads.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


bool g_bThreadedEngine = false;
int g_nMaterialSystemThread = 0;
int g_nServerThread = ( IsPS3() /*single HT1*/ ) ? 0 : 1;
