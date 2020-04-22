//=========== Copyright © Valve Corporation, All rights reserved. ===========//
//
// Purpose: Decal flags that can be set publicly by other modules for the decal system.
//
//===========================================================================//
#pragma once

// NOTE: If you add a flag here, make sure to add one to the private set of flags as well, 
// and update R_ConvertToPrivateDecalFlags

#define EDF_PLAYERSPRAY			0x00000001
#define EDF_IMMEDIATECLEANUP	0x00000002
