//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// glmgrext.h
// helper file for extension testing and runtime importing of entry points
//
//===============================================================================

#pragma once

#include <OpenGL/gl.h>
#include <OpenGL/glext.h>

// #define symbol INSTANTIATE_GL_IMPORTS controls whether the following macro "GL_IMPORT" writes externs or writes decls
// normally only glmgr.cpp sets that symbol and includes this file

// to simplify usage, a function ptr type must exist for every entry point, following the name##FuncPtr convention.

#ifdef INSTANTIATE_GL_IMPORTS
	#define GL_IMPORT( name ) name##FuncPtr name = 0
#else
	#define GL_IMPORT( name ) extern name##FuncPtr name;
#endif


// before declaring each import, check to see if the EXT symbol is in effect,
// and if so, don't do it!

#ifndef GL_EXT_draw_buffers2
	typedef void (* glColorMaskIndexedEXTFuncPtr)	(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a);
	typedef void (* glEnableIndexedEXTFuncPtr)		(GLenum target, GLuint index);
	typedef void (* glDisableIndexedEXTFuncPtr)		(GLenum target, GLuint index);

	GL_IMPORT(glColorMaskIndexedEXT);
	GL_IMPORT(glEnableIndexedEXT);
	GL_IMPORT(glDisableIndexedEXT);
#endif

#ifndef GL_EXT_framebuffer_sRGB
	#define GL_FRAMEBUFFER_SRGB_EXT                 0x8DB9
	#define GL_FRAMEBUFFER_SRGB_CAPABLE_EXT         0x8DBA
#endif

#ifndef ARB_texture_rg
	#define GL_COMPRESSED_RED                 0x8225
	#define GL_COMPRESSED_RG                  0x8226
	#define GL_RG                             0x8227
	#define GL_RG_INTEGER                     0x8228
	#define GL_R8                             0x8229
	#define GL_R16                            0x822A
	#define GL_RG8                            0x822B
	#define GL_RG16                           0x822C
	#define GL_R16F                           0x822D
	#define GL_R32F                           0x822E
	#define GL_RG16F                          0x822F
	#define GL_RG32F                          0x8230
	#define GL_R8I                            0x8231
	#define GL_R8UI                           0x8232
	#define GL_R16I                           0x8233
	#define GL_R16UI                          0x8234
	#define GL_R32I                           0x8235
	#define GL_R32UI                          0x8236
	#define GL_RG8I                           0x8237
	#define GL_RG8UI                          0x8238
	#define GL_RG16I                          0x8239
	#define GL_RG16UI                         0x823A
	#define GL_RG32I                          0x823B
	#define GL_RG32UI                         0x823C
	#define GL_RGB10_A2UI_ARB                 0x906F
#endif

#ifndef GL_EXT_bindable_uniform
	#define GL_UNIFORM_BUFFER_EXT             0x8DEE
#endif

// unpublished extension enums (thus the "X")

// from EXT_framebuffer_multisample_blit_scaled..
#define XGL_SCALED_RESOLVE_FASTEST_EXT 0x90BA
#define XGL_SCALED_RESOLVE_NICEST_EXT 0x90BB


void * NSGLGetProcAddress (const char *name);

// call this to find all the entry points.
void	GLMSetupExtensions( void );



typedef void (*PFNglColorMaskIndexedEXT)(GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a);
typedef void (*PFNglEnableIndexedEXT)(GLenum target, GLuint index);
typedef void (*PFNglDisableIndexedEXT)(GLenum target, GLuint index);
typedef void (*PFNglUniformBufferEXT)(GLuint program, GLint location, GLuint buffer);

extern PFNglColorMaskIndexedEXT pfnglColorMaskIndexedEXT;
extern PFNglEnableIndexedEXT pfnglEnableIndexedEXT;
extern PFNglDisableIndexedEXT pfnglDisableIndexedEXT;
extern PFNglUniformBufferEXT pfnglUniformBufferEXT;


typedef void (*PFNglGetFramebufferAttachmentParameteriv)(GLenum target, GLenum attachment, GLenum pname, GLint *params);
extern PFNglGetFramebufferAttachmentParameteriv pfnglGetFramebufferAttachmentParameteriv;

