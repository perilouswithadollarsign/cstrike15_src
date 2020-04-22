//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// glmgrext.h
// helper file for extension testing and runtime importing of entry points
//
//===============================================================================

#pragma once

#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <mach-o/dyld.h>
#include <stdlib.h>
#include <string.h>
#include "glmgr/glmgr.h"

PFNglColorMaskIndexedEXT pfnglColorMaskIndexedEXT;
PFNglEnableIndexedEXT pfnglEnableIndexedEXT;
PFNglDisableIndexedEXT pfnglDisableIndexedEXT;
PFNglGetFramebufferAttachmentParameteriv pfnglGetFramebufferAttachmentParameteriv;
PFNglUniformBufferEXT pfnglUniformBufferEXT;

void * NSGLGetProcAddress (const char *name)
{
    NSSymbol symbol;
    char *symbolName = (char *)malloc (strlen (name) + 2); 
    strcpy(symbolName + 1, name); 
    symbolName[0] = '_'; 
    symbol = NULL;
    if (NSIsSymbolNameDefined (symbolName)) 
        symbol = NSLookupAndBindSymbol (symbolName);
    free (symbolName);
    return symbol ? NSAddressOfSymbol (symbol) : NULL;
}


void GLMSetupExtensions( void )
{
	pfnglColorMaskIndexedEXT = (PFNglColorMaskIndexedEXT) NSGLGetProcAddress( "glColorMaskIndexedEXT" );
	pfnglEnableIndexedEXT = (PFNglEnableIndexedEXT) NSGLGetProcAddress( "glEnableIndexedEXT" );
	pfnglDisableIndexedEXT = (PFNglDisableIndexedEXT) NSGLGetProcAddress( "glDisableIndexedEXT" );

	pfnglGetFramebufferAttachmentParameteriv = (PFNglGetFramebufferAttachmentParameteriv) NSGLGetProcAddress( "glGetFramebufferAttachmentParameteriv" );

	pfnglUniformBufferEXT = (PFNglUniformBufferEXT) NSGLGetProcAddress( "glUniformBufferEXT" );
}

/*
#define INSTANTIATE_GL_IMPORTS
#include "glmgr/glmgr.h"		// will include glmgrext.h
#undef INSTANTIATE_GL_IMPORTS


// helper class for looking up function names
// see http://andrewtolbert.com/svn/personal/OpenGLSuperBible/shared/gltools.cpp
// also http://developer.apple.com/mac/library/DOCUMENTATION/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_entrypts/opengl_entrypts.html

class CFunctionImporter
{
public:
	CFBundleRef m_bundle;

	CFunctionImporter( CFStringRef bundleID )	// for example CFSTR("com.apple.OpenGL")
	{
		m_bundle = CFBundleGetBundleWithIdentifier( bundleID );
		if ( m_bundle )
			CFRetain( m_bundle );
	}
	
	~CFunctionImporter()
	{
		if( m_bundle )
		{
			CFRelease(m_bundle);
			m_bundle = NULL;
		}
	}
		
	void	*FindFunctionByName(CFStringRef name)		// ex CFSTR("glColorMaskedIndexedEXT")
	{
		void *result = NULL;
		if (m_bundle)
		{
			result = CFBundleGetFunctionPointerForName(m_bundle, name);
		}
		return result;
	}
};


void GLMSetupExtensions( void )
{
	CFunctionImporter	importer( CFSTR("com.apple.OpenGL") );
	
	#define	DO_IMPORT(name)	name = (name##FuncPtr)importer.FindFunctionByName( CFSTR(#name) );
	
	#ifndef GL_EXT_draw_buffers2
		// FIXME we're not checking for the extension string yet, we're just grabbing func ptrs
		DO_IMPORT(glColorMaskIndexedEXT);
		DO_IMPORT(glEnableIndexedEXT);
		DO_IMPORT(glDisableIndexedEXT);
	#endif
}
*/
