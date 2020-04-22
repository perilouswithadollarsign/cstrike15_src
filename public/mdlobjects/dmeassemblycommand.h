//====== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. =====
//
// DmeAssembleCommand
//
//=============================================================================


#ifndef DMEASSEMBLYCOMMAND_H
#define DMEASSEMBLYCOMMAND_H


#if defined( _WIN32 )
#pragma once
#endif


// Valve includes
#include "datamodel/dmelement.h"


//-----------------------------------------------------------------------------
// DmeAssemblyCommand
//-----------------------------------------------------------------------------
class CDmeAssemblyCommand : public CDmElement
{
	DEFINE_ELEMENT( CDmeAssemblyCommand, CDmElement );

public:
	virtual bool Apply( CDmElement * /* pDmElement */ ) { return false; }

	// No attributes
};


#endif // DMEASSEMBLYCOMMAND_H
