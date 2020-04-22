//===== Copyright c 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef IMATCHEXTENSIONS_H
#define IMATCHEXTENSIONS_H

#ifdef _WIN32
#pragma once
#endif

abstract_class IMatchExtensions
{
public:
	// Registers an extension interface
	virtual void RegisterExtensionInterface( char const *szInterfaceString, void *pvInterface ) = 0;

	// Unregisters an extension interface
	virtual void UnregisterExtensionInterface( char const *szInterfaceString, void *pvInterface ) = 0;

	// Gets a pointer to a registered extension interface
	virtual void * GetRegisteredExtensionInterface( char const *szInterfaceString ) = 0;
};

#endif // IMATCHEXTENSIONS_H
