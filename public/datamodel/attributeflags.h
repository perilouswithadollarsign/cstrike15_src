//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef ATTRIBUTEFLAGS_H
#define ATTRIBUTEFLAGS_H

#ifdef _WIN32
#pragma once
#endif

enum
{
	// NOTE: The first 5 flags bits are reserved for attribute type
	FATTRIB_TYPEMASK		= 0x1F,

	FATTRIB_READONLY		= (1<<5), // Don't allow editing value in editors
	FATTRIB_DONTSAVE		= (1<<6), // Don't persist to .dmx file
	FATTRIB_DIRTY			= (1<<7), // Indicates the attribute has been changed since the resolve phase
	FATTRIB_HAS_CALLBACK	= (1<<8), // Indicates that this will notify its owner and/or other elements when it changes
	FATTRIB_EXTERNAL		= (1<<9), // Indicates this attribute's data is externally owned (in a CDmElement somewhere)
	FATTRIB_TOPOLOGICAL		= (1<<10), // Indicates this attribute effects the scene's topology (ie it's an attribute name or element)
	FATTRIB_MUSTCOPY		= (1<<11), // parent element must make a new copy during CopyInto, even for shallow copy
	FATTRIB_NEVERCOPY		= (1<<12), // parent element shouldn't make a new copy during CopyInto, even for deep copy
	FATTRIB_USERDEFINED		= (1<<13), // This flag WAS used to sort attributes. Now it's used to guess whether vmtdoc and vmfentity attributes are shaderparams or entitykeys. TODO - remove this
	FATTRIB_OPERATOR_DIRTY	= (1<<14), // Used and cleared only by operator phase of datamodel
	FATTRIB_HIDDEN			= (1<<15), // shouldn't be shown in ui
};

#endif // ATTRIBUTEFLAGS_H
