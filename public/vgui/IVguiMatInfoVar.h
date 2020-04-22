//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IVGUIMATINFOVAR_H
#define IVGUIMATINFOVAR_H


// wrapper for IMaterialVar
class IVguiMatInfoVar
{
public:
	virtual int GetIntValue ( void ) const = 0;
	virtual void SetIntValue ( int val ) = 0;

	// todo: if you need to add more IMaterialVar functions add them here
};

#endif //IVGUIMATINFOVAR_H
