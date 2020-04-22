//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Main interface for DCC
//
// $NoKeywords: $
//===========================================================================//

#ifndef IDCCMAIN_H
#define IDCCMAIN_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "appframework/iappsystem.h"


//-----------------------------------------------------------------------------
// Purpose: Main interface for DCC
//-----------------------------------------------------------------------------
#define DCC_MAIN_INTERFACE_VERSION "VDCCMain001"
abstract_class IDCCMain : public IAppSystem
{
public:
	virtual bool IsInitialized( ) = 0; 
};

extern IDCCMain* g_pDCCMain;


#endif // IDCCMAIN_H
