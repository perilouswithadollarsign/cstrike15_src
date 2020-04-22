//================ Copyright (c) 1996-2009 Valve Corporation. All Rights Reserved. =================
//
//
//
//==================================================================================================

#ifndef IGLXMGR_H
#define IGLXMGR_H
#ifdef _WIN32
#pragma once
#endif


#include "tier0/basetypes.h"


#define GLXMGR_INTERFACE_VERSION "GLXMgrInterface001"


DECLARE_POINTER_HANDLE( GLXMgrContext_t );

class IGLXMgr
{
public:
	// Window creation.
	// pAttributes are GLX_ attributes that go to glXChooseVisual.
	virtual void* CreateWindow( const char *pTitle, bool bWindowed, int width, int height ) = 0;

	// GL context management.
	virtual GLXMgrContext_t	GetMainContext() = 0;
	virtual GLXMgrContext_t CreateExtraContext() = 0;
	virtual void DeleteContext( GLXMgrContext_t hContext ) = 0;
	virtual void MakeContextCurrent( GLXMgrContext_t hContext ) = 0;
};


#endif // IGLXMGR_H
