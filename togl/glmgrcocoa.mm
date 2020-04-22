//========= Copyright  1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: provide some call-out glue to ObjC from the C++ GLMgr code
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//


#include <Cocoa/Cocoa.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

#undef MIN
#undef MAX
#define DONT_DEFINE_BOOL	// Don't define BOOL!
#include "tier0/threadtools.h"
#include "tier1/interface.h"
#include "tier1/strtools.h"
#include "tier1/utllinkedlist.h"
#include "togl/rendermechanism.h"



// ------------------------------------------------------------------------------------ //
// some glue to let GLMgr call into NS/ObjC classes.
// ------------------------------------------------------------------------------------ //

bool NewNSGLContext( unsigned long *attribs, PseudoNSGLContextPtr nsglShareCtx, PseudoNSGLContextPtr *nsglCtxOut, CGLContextObj *cglCtxOut )
{
	NSAutoreleasePool	*tempPool = [[NSAutoreleasePool alloc] init ];
	NSOpenGLPixelFormat	*pixFmt = NULL; 
	NSOpenGLContext		*nsglCtx = NULL;

	bool result = true;		// optimism
	
	if (result)
	{
		pixFmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:(NSOpenGLPixelFormatAttribute*)attribs];
		if (!pixFmt)
		{
			Debugger();	// bad news
			result = false;
		}
	}

	if (result)
	{
		nsglCtx = [[NSOpenGLContext alloc] initWithFormat: pixFmt shareContext: (NSOpenGLContext*) nsglShareCtx ];
		if (!nsglCtx)
		{
			Debugger();
			result = false;
		}
	}

	if (result)
	{
		[nsglCtx makeCurrentContext];
		
		*nsglCtxOut = nsglCtx;
		*cglCtxOut = (CGLContextObj)[ (NSOpenGLContext*)nsglCtx CGLContextObj ];
	}
	else
	{
		*nsglCtxOut = NULL;
		*cglCtxOut = NULL;
	}

	[tempPool release];
	
	return result;
}

CGLContextObj GetCGLContextFromNSGL( PseudoNSGLContextPtr nsglCtx )
{
	return (CGLContextObj)[ (NSOpenGLContext*)nsglCtx CGLContextObj];
}

void DelNSGLContext( PseudoNSGLContextPtr nsglCtx )
{
	[ (NSOpenGLContext*)nsglCtx release ];
}

