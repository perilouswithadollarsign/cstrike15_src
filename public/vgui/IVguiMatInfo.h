//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IVGUIMATINFO_H
#define IVGUIMATINFO_H

#include "IVguiMatInfoVar.h"

// wrapper for IMaterial
class IVguiMatInfo
{
public:	
	// make sure to delete the returned object after use!
	virtual IVguiMatInfoVar* FindVarFactory ( const char *varName, bool *found ) = 0;

	virtual int GetNumAnimationFrames ( ) = 0;

	// todo: if you need to add more IMaterial functions add them here
};

#endif //IVGUIMATINFO_H
