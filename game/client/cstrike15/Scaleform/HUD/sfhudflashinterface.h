//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#if !defined( SFHUDFLASHINTERFACE_H_ )
#define SFHUDFLASHINTERFACE_H_

#include "hudelement.h"
#include "scaleformui/scaleformui.h"

class SFHudFlashInterface : public ScaleformFlashInterfaceMixin<CHudElement>
{
public:
	explicit SFHudFlashInterface( const char* name ) : ScaleformFlashInterfaceMixin<CHudElement>()
	{
		InitCHudElementAfterConstruction( name );
	}

	virtual bool ShouldProcessInputBeforeFlashApiReady() { return false; }
};

extern ConVar cl_drawhud;

#endif // SHHUDFLASHINTERFACE_H_
