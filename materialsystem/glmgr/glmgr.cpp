//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// glmgr.cpp
//
//===============================================================================

#include "glmgr/glmgr.h"
#include "glmgr/glmdisplay.h"

#include "tier0/icommandline.h"
#include "../shaderapidx9/dxabstract.h"		// need to be able to see D3D enums
#include "appframework/icocoamgr.h"

#include "convar.h"

// memdbgon -must- be the last include file in a .cpp file.
#include "tier0/memdbgon.h"


//===============================================================================

char g_nullFragmentProgramText [] =
{
	"!!ARBfp1.0  \n"
	"PARAM black = { 0.0, 0.0, 0.0, 1.0 };  \n"		// opaque black
	"MOV result.color, black;  \n"
	"END  \n\n\n"
	"//GLSLfp\n"
	"void main()\n"
	"{\n"
	"gl_FragColor = vec4( 0.0, 0.0, 0.0, 1.0 );\n"
	"}\n"
	
};



	// make dummy programs for doing texture preload via dummy draw
char g_preloadTexVertexProgramText[] =
{
	"//GLSLvp  \n"
	"#version 120  \n"
	"varying vec4 otex;  \n"
	"void main()  \n"
	"{  \n"
	"vec4 pos = ftransform(); // vec4( 0.1, 0.1, 0.1, 0.1 );  \n"
	"vec4 tex = vec4( 0.0, 0.0, 0.0, 0.0 );  \n"
	"  \n"
	"gl_Position = pos;  \n"
	"otex = tex;  \n"
	"}  \n"
};

char g_preload2DTexFragmentProgramText[] =
{
	"//GLSLfp  \n"
	"#version 120  \n"
	"varying vec4 otex;  \n"
	"//SAMPLERMASK-8000		// may not be needed  \n"
	"//HIGHWATER-30			// may not be needed  \n"
	"  \n"
	"uniform vec4 pc[31];  \n"
	"uniform sampler2D sampler15;  \n"
	"  \n"
	"void main()  \n"
	"{  \n"
	"vec4 r0;  \n"
	"r0 = texture2D( sampler15, otex.xy );  \n"
	"gl_FragColor = r0;	//discard;  \n"
	"}  \n"
};

char g_preload3DTexFragmentProgramText[] =
{
	"//GLSLfp  \n"
	"#version 120  \n"
	"varying vec4 otex;  \n"
	"//SAMPLERMASK-8000		// may not be needed  \n"
	"//HIGHWATER-30			// may not be needed  \n"
	"  \n"
	"uniform vec4 pc[31];  \n"
	"uniform sampler3D sampler15;  \n"
	"  \n"
	"void main()  \n"
	"{  \n"
	"vec4 r0;  \n"
	"r0 = texture3D( sampler15, otex.xyz );  \n"
	"gl_FragColor = r0;	//discard;  \n"
	"}  \n"
};

char g_preloadCubeTexFragmentProgramText[] =
{
	"//GLSLfp  \n"
	"#version 120  \n"
	"varying vec4 otex;  \n"
	"//SAMPLERMASK-8000		// may not be needed  \n"
	"//HIGHWATER-30			// may not be needed  \n"
	"  \n"
	"uniform vec4 pc[31];  \n"
	"uniform samplerCube sampler15;  \n"
	"  \n"
	"void main()  \n"
	"{  \n"
	"vec4 r0;  \n"
	"r0 = textureCube( sampler15, otex.xyz );  \n"
	"gl_FragColor = r0;	//discard;  \n"
	"}  \n"
};







//===============================================================================
// helper routines for debug

static bool hasnonzeros( float *values, int count )
{
	for( int i=0; i<count; i++)
	{
		if (values[i] != 0.0)
		{
			return true;
		}
	}
	return false;
}

static void printmat( char *label, int baseSlotNumber, int slots, float *m00 )
{
	// print label..
	// fetch 4 from row, print as a row
	// fetch 4 from column, print as a row
	
	float	row[4];
	float	col[4];
	
	if (hasnonzeros( m00, slots*4) )
	{
		GLMPRINTF(("-D-  %s", label ));
		for( int islot=0; islot<4; islot++ )			// we always run this loop til 4, but we special case the printing if there are only 3 submitted
		{
			// extract row and column floats
			for( int slotcol=0; slotcol<4; slotcol++)
			{
				//copy
				row[slotcol] = m00[(islot*4)+slotcol];
				
				// transpose
				col[slotcol] = m00[(slotcol*4)+islot];
			}
			if (slots==4)
			{
				GLMPRINTF((		"-D-    %03d: [ %10.5f %10.5f %10.5f %10.5f ] T=> [ %10.5f %10.5f %10.5f %10.5f ]",
								baseSlotNumber+islot,
								row[0],row[1],row[2],row[3],
								col[0],col[1],col[2],col[3]						
								));
			}
			else
			{
				if (islot<3)
				{
					GLMPRINTF((		"-D-    %03d: [ %10.5f %10.5f %10.5f %10.5f ] T=> [ %10.5f %10.5f %10.5f ]",
									baseSlotNumber+islot,
									row[0],row[1],row[2],row[3],
									col[0],col[1],col[2]
									));
				}
				else
				{
					GLMPRINTF((		"-D-    %03d:                                                 T=> [ %10.5f %10.5f %10.5f ]",
									baseSlotNumber+islot,
									col[0],col[1],col[2]
									));
				}
			}
		}
		GLMPRINTSTR(("-D-"));
	}
	else
	{
		GLMPRINTF(("-D-  %s - (all 0.0)", label ));
	}

}


static void transform_dp4( float *in4, float *m00, int slots, float *out4 )
{
	// m00 points to a column.
	// each DP is one column of the matrix ( m00[4*n]
	// if we are passed a three slot matrix, this is three columns, the source W plays into all three columns, but we must set the final output W to 1 ?
	for( int n=0; n<slots; n++)
	{
		float col4[4];
		
		col4[0] = m00[(4*n)+0];
		col4[1] = m00[(4*n)+1];
		col4[2] = m00[(4*n)+2];
		col4[3] = m00[(4*n)+3];
		
		out4[n] = 0.0;
		for( int inner = 0; inner < 4; inner++ )
		{
			out4[n] += in4[inner] * col4[inner];
		}
	}
	if (slots==3)
	{
		out4[3] = 1.0;
	}
}

//===============================================================================

extern ICocoaMgr *g_extCocoaMgr;

//===============================================================================
// GLMgr static methods

GLMgr	*g_glmgr = NULL;

void GLMgr::NewGLMgr( void )
{
	if (!g_glmgr)
	{
		GLMSetupExtensions();
		#if GLMDEBUG
			// check debug mode early in program lifetime
			GLMDebugInitialize( true );
		#endif

		g_glmgr = new GLMgr;
	}
}

GLMgr *GLMgr::aGLMgr( void )
{
	assert( g_glmgr != NULL);
	return g_glmgr;
}

void	GLMgr::DelGLMgr( void )
{
	if (g_glmgr)
	{
		delete g_glmgr;
		g_glmgr = NULL;
	}
}

// GLMgr class methods

GLMgr::GLMgr()
{
}	


GLMgr::~GLMgr()
{
}


//===============================================================================

GLMContext *GLMgr::NewContext( GLMDisplayParams *params )
{
	// this now becomes really simple.  We just pass through the params.
	
	return new GLMContext( params );
}

void GLMgr::DelContext( GLMContext *context )
{
	delete context;
}

void GLMgr::SetCurrentContext( GLMContext *context )
{
	CGLError	cgl_err;
	cgl_err = CGLSetCurrentContext( context->m_ctx );
	if (cgl_err)
	{
		// give up
		GLMStop();
	}
}

GLMContext *GLMgr::GetCurrentContext( void )
{
	CGLContextObj ctx = CGLGetCurrentContext();
	
	GLint	glm_context_link = 0;
	
	CGLGetParameter( ctx, kCGLCPClientStorage, &glm_context_link );
	
	if ( glm_context_link )
	{
		return (GLMContext*) glm_context_link;
	}
	else
	{
		return NULL;
	}
}
	

//===============================================================================
// GLMContext public methods
void GLMContext::MakeCurrent( void )
{
//	GLM_FUNC;

	CGLSetCurrentContext( m_ctx );
}

void GLMContext::CheckCurrent( void )
{
	#if 1
//	GLM_FUNC;

	// probably want to make this a no-op for release builds
	// but we can't, because someone is messing with current context and not sure where yet
	CGLContextObj curr = CGLGetCurrentContext();
	if (curr != m_ctx)
	{
		if (!CommandLine()->FindParm("-hushasserts"))
		{
			Assert( !"Current context mismatch");
			
			#if GLMDEBUG
			Debugger();
			#endif
		}
		MakeCurrent();	// you're welcome
	}
	#endif
}


const GLMRendererInfoFields&	GLMContext::Caps( void )
{
	return m_caps;
}

void	GLMContext::DumpCaps( void )
{
	/*
		#define	dumpfield( fff ) printf( "\n  "#fff" : %d", (int) m_caps.fff )
		#define	dumpfield_hex( fff ) printf( "\n  "#fff" : 0x%08x", (int) m_caps.fff )
		#define	dumpfield_str( fff ) printf( "\n  "#fff" : %s", m_caps.fff )
	*/

	#define	dumpfield( fff )		printf( "\n  %-30s : %d", #fff, (int) m_caps.fff )
	#define	dumpfield_hex( fff )	printf( "\n  %-30s : 0x%08x", #fff, (int) m_caps.fff )
	#define	dumpfield_str( fff )	printf( "\n  %-30s : %s", #fff, m_caps.fff )

	printf("\n-------------------------------- context caps for context %08x", (uint)this);

	dumpfield( m_fullscreen );
	dumpfield( m_accelerated );
	dumpfield( m_windowed );
	dumpfield_hex( m_rendererID );
	dumpfield( m_displayMask );
	dumpfield( m_bufferModes );
	dumpfield( m_colorModes );
	dumpfield( m_accumModes );
	dumpfield( m_depthModes );
	dumpfield( m_stencilModes );
	dumpfield( m_maxAuxBuffers );
	dumpfield( m_maxSampleBuffers );
	dumpfield( m_maxSamples );
	dumpfield( m_sampleModes );
	dumpfield( m_sampleAlpha );
	dumpfield_hex( m_vidMemory );
	dumpfield_hex( m_texMemory );

	dumpfield_hex( m_pciVendorID );
	dumpfield_hex( m_pciDeviceID );
	dumpfield_str( m_pciModelString );
	dumpfield_str( m_driverInfoString );

	printf( "\n  m_osComboVersion: 0x%08x (%d.%d.%d)", m_caps.m_osComboVersion, (m_caps.m_osComboVersion>>16)&0xFF, (m_caps.m_osComboVersion>>8)&0xFF, (m_caps.m_osComboVersion)&0xFF );

	dumpfield( m_ati );
	if (m_caps.m_ati)
	{
		dumpfield( m_atiR5xx );
		dumpfield( m_atiR6xx );
		dumpfield( m_atiR7xx );
		dumpfield( m_atiR8xx );
		dumpfield( m_atiNewer );
	}

	dumpfield( m_intel );
	if (m_caps.m_intel)
	{
		dumpfield( m_intel95x );
		dumpfield( m_intel3100 );
		dumpfield( m_intelNewer );
	}

	dumpfield( m_nv );
	if (m_caps.m_nv)
	{
		//dumpfield( m_nvG7x );
		dumpfield( m_nvG8x );
		dumpfield( m_nvNewer );
	}

	dumpfield( m_hasGammaWrites );
	dumpfield( m_hasSRGBDecode );
	dumpfield( m_hasMixedAttachmentSizes );
	dumpfield( m_hasBGRA );
	dumpfield( m_hasNewFullscreenMode );
	dumpfield( m_hasNativeClipVertexMode );
	dumpfield( m_maxAniso );
	dumpfield( m_hasBindableUniforms );
	dumpfield( m_hasUniformBuffers );
	dumpfield( m_hasPerfPackage1 );
	
	dumpfield( m_cantBlitReliably );
	dumpfield( m_cantAttachSRGB );
	dumpfield( m_cantResolveFlipped );
	dumpfield( m_cantResolveScaled );
	dumpfield( m_costlyGammaFlips );
	dumpfield( m_badDriver1064NV );

	printf("\n--------------------------------");
	
	#undef dumpfield
	#undef dumpfield_hex
	#undef dumpfield_str
}

void	DefaultSamplingParams( GLMTexSamplingParams *samp, GLMTexLayoutKey *key )
{
	memset( samp, 0, sizeof(*samp) );
	
	// Default to black, it may make drivers happier
	samp->m_borderColor[0] = 0.0f;
	samp->m_borderColor[0] = 0.0f;
	samp->m_borderColor[0] = 0.0f;
	samp->m_borderColor[0] = 1.0f;

	// generally speaking..
	// if it's a render target, default it to GL_CLAMP_TO_BORDER, else GL_REPEAT
	// if it has mipmaps, default the min filter to GL_LINEAR_MIPMAP_LINEAR, else GL_LINEAR
	
	// ** none of these really matter all that much because the first time we go to render, the d3d sampler state will be consulted
	// and applied directly to the tex object without regard to any previous values..
	
	GLenum rtclamp = GL_CLAMP_TO_EDGE;		//GL_CLAMP_TO_BORDER
	
	switch( key->m_texFlags & (kGLMTexRenderable|kGLMTexMipped) )
	{	
		case 0:
			// -- mipped, -- renderable
			samp->m_addressModes[0] = GL_REPEAT;
			samp->m_addressModes[1] = GL_REPEAT;
			samp->m_addressModes[2] = GL_REPEAT;
			
			samp->m_magFilter	= GL_LINEAR;
			samp->m_minFilter	= GL_LINEAR;
		break;

		case kGLMTexRenderable:
			// -- mipped, ++ renderable
			samp->m_addressModes[0] = rtclamp;
			samp->m_addressModes[1] = rtclamp;
			samp->m_addressModes[2] = rtclamp;
			
			samp->m_magFilter	= GL_LINEAR;
			samp->m_minFilter	= GL_LINEAR;
		break;

		case kGLMTexMipped:
			// ++ mipped, -- renderable
			samp->m_addressModes[0] = GL_REPEAT;
			samp->m_addressModes[1] = GL_REPEAT;
			samp->m_addressModes[2] = GL_REPEAT;
			
			samp->m_magFilter	= GL_LINEAR;
			samp->m_minFilter	= GL_LINEAR_MIPMAP_LINEAR;	// was GL_NEAREST_MIPMAP_LINEAR;
		break;

		case kGLMTexRenderable | kGLMTexMipped:
			// ++ mipped, ++ renderable
			samp->m_addressModes[0] = rtclamp;
			samp->m_addressModes[1] = rtclamp;
			samp->m_addressModes[2] = rtclamp;
			
			samp->m_magFilter	= GL_LINEAR;
			samp->m_minFilter	= GL_LINEAR_MIPMAP_LINEAR;	// was GL_NEAREST_MIPMAP_LINEAR;
		break;

	}

	samp->m_mipmapBias	= 0.0f;

	samp->m_minMipLevel	= 0;		// this drives GL_TEXTURE_MIN_LOD - i.e. lowest MIP selection index clamp (largest size), not "slice defined" boundary
	samp->m_maxMipLevel	= 16;		// this drives GL_TEXTURE_MAX_LOD - i.e. highest MIP selection clamp (smallest size), not "slice defined" boundary

	samp->m_maxAniso	= 1;
	samp->m_compareMode	= GL_NONE;	// only for depth or stencil tex
	
	samp->m_srgb = false;
}

CGLMTex	*GLMContext::NewTex( GLMTexLayoutKey *key, const char *debugLabel )
{
	//hushed GLM_FUNC;
	MakeCurrent();
	
	// get a layout based on the key
	GLMTexLayout *layout = m_texLayoutTable->NewLayoutRef( key );
	
	GLMTexSamplingParams	defsamp;
	DefaultSamplingParams( &defsamp, key );
	
	CGLMTex *tex = new CGLMTex( this, layout, &defsamp, debugLabel );
	
	return tex;
}

void	GLMContext::DelTex( CGLMTex	*tex )
{
	//hushed GLM_FUNC;
	MakeCurrent();

	for( int i=0; i<GLM_SAMPLER_COUNT; i++)
	{
		// clear out any reference in the drawing sampler array
		if (m_samplers[i].m_drawTex == tex)
		{
			m_samplers[i].m_drawTex = NULL;
		}

		if (m_samplers[i].m_boundTex == tex)
		{
			this->BindTexToTMU( NULL, i );			
			m_samplers[i].m_boundTex = NULL;	// for clarity

			tex->m_bindPoints[i] = 0;
		}
	}
	
	if (tex->m_rtAttachCount !=0)
	{
		// leak it and complain - we may have to implement a deferred-delete system for tex like these

		GLMPRINTF(("-D- ################## Leaking tex %08x [ %s ] - was attached for drawing at time of delete",tex, tex->m_layout->m_layoutSummary ));

		#if 0
			// can't actually do this yet as the draw calls will tank
			FOR_EACH_VEC( m_fboTable, i )
			{
				CGLMFBO *fbo = m_fboTable[i];
				fbo->TexScrub( tex );
			}
			tex->m_rtAttachCount = 0;
		#endif
	}
	else
	{	
		delete tex;
	}
}



	// push and pop attrib when blit has mixed srgb source and dest?		
ConVar	gl_radar7954721_workaround_mixed ( "gl_radar7954721_workaround_mixed", "1" );

	// push and pop attrib on any blit?
ConVar	gl_radar7954721_workaround_all ( "gl_radar7954721_workaround_all", "0" );

	// what attrib mask to use ?
ConVar	gl_radar7954721_workaround_maskval ( "gl_radar7954721_workaround_maskval", "0" );

enum eBlitFormatClass
{
	eColor,
	eDepth,				// may not get used.  not sure..
	eDepthStencil
};

uint	glAttachFromClass[ 3 ] = 		{ GL_COLOR_ATTACHMENT0_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_DEPTH_STENCIL_ATTACHMENT_EXT };

void	glScrubFBO			( GLenum target )
{
	glFramebufferRenderbufferEXT	( target, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, 0);								GLMCheckError();
	glFramebufferRenderbufferEXT	( target, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);									GLMCheckError();		
	glFramebufferRenderbufferEXT	( target, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);								GLMCheckError();		

	glFramebufferTexture2DEXT		( target, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0 );									GLMCheckError();
	glFramebufferTexture2DEXT		( target, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, 0, 0 );									GLMCheckError();
	glFramebufferTexture2DEXT		( target, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, 0, 0 );									GLMCheckError();
}

void	glAttachRBOtoFBO	( GLenum target, eBlitFormatClass formatClass, uint rboName )
{
	switch( formatClass )
	{
		case eColor:
			glFramebufferRenderbufferEXT	( target, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, rboName);					GLMCheckError();
		break;
		
		case eDepth:
			glFramebufferRenderbufferEXT	( target, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rboName);					GLMCheckError();		
		break;
		
		case eDepthStencil:
			glFramebufferRenderbufferEXT	( target, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rboName);					GLMCheckError();		
			glFramebufferRenderbufferEXT	( target, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rboName);					GLMCheckError();		
		break;
	}
}

void	glAttachTex2DtoFBO	( GLenum target, eBlitFormatClass formatClass, uint texName, uint texMip )
{
	switch( formatClass )
	{
		case eColor:
			glFramebufferTexture2DEXT		( target, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, texName, texMip );				GLMCheckError();
		break;
		
		case eDepth:
			glFramebufferTexture2DEXT		( target, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, texName, texMip );				GLMCheckError();
		break;
		
		case eDepthStencil:
			glFramebufferTexture2DEXT		( target, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, texName, texMip );				GLMCheckError();
			glFramebufferTexture2DEXT		( target, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, texName, texMip );				GLMCheckError();
		break;
	}
}

ConVar	gl_can_resolve_flipped("gl_can_resolve_flipped", "0" );
ConVar	gl_cannot_resolve_flipped("gl_cannot_resolve_flipped", "0" );


// these are only consulted if the m_cant_resolve_scaled cap bool is false.

ConVar	gl_minify_resolve_mode("gl_minify_resolve_mode", "1" );		// if scaled resolve available, for downscaled resolve blits only (i.e. internal blits)
ConVar	gl_magnify_resolve_mode("gl_magnify_resolve_mode", "2" );	// if scaled resolve available, for upscaled resolve blits only

	// 0 == old style, two steps
	// 1 == faster, one step blit aka XGL_SCALED_RESOLVE_FASTEST_EXT - if available.
	// 2 == faster, one step blit aka XGL_SCALED_RESOLVE_NICEST_EXT - if available.

unsigned short foo[4];

void	GLMContext::Blit2( CGLMTex *srcTex, GLMRect *srcRect, int srcFace, int srcMip, CGLMTex *dstTex, GLMRect *dstRect, int dstFace, int dstMip, uint filter )
{
	Assert( srcFace == 0 );
	Assert( dstFace == 0 );

//	glColor4usv( foo );
	
	//----------------------------------------------------------------- format assessment

	eBlitFormatClass	formatClass;
	uint				blitMask= 0;

	switch( srcTex->m_layout->m_format->m_glDataFormat )
	{
		case GL_BGRA:	case GL_RGB:	case GL_RGBA:	case GL_ALPHA:	case GL_LUMINANCE:	case GL_LUMINANCE_ALPHA:
			formatClass = eColor;
			blitMask = GL_COLOR_BUFFER_BIT;
		break;

		case GL_DEPTH_COMPONENT:
			formatClass = eDepth;
			blitMask = GL_DEPTH_BUFFER_BIT;
		break;
		
		case GL_DEPTH_STENCIL_EXT:
			formatClass = eDepthStencil;
			blitMask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
		break;

		default:
			Assert(!"Unsupported format for blit" );
			GLMStop();
		break;
	}

	//----------------------------------------------------------------- blit assessment
	

	bool blitResolves	=	srcTex->m_rboName != 0;
	bool blitScales		=	((srcRect->xmax - srcRect->xmin) != (dstRect->xmax - dstRect->xmin)) || ((srcRect->ymax - srcRect->ymin) != (dstRect->ymax - dstRect->ymin));

	bool blitToBack		=	(dstTex == NULL);
	bool blitFlips		=	blitToBack;			// implicit y-flip upon blit to GL_BACK supplied
	
	//should we support blitFromBack ?

	bool srcGamma = srcTex && ((srcTex->m_layout->m_key.m_texFlags & kGLMTexSRGB) != 0);
	bool dstGamma = dstTex && ((dstTex->m_layout->m_key.m_texFlags & kGLMTexSRGB) != 0);

	bool doPushPop = (srcGamma != dstGamma) && gl_radar7954721_workaround_mixed.GetInt() && m_caps.m_nv;		// workaround for cross gamma blit problems on NV
		// ^^ need to re-check this on some post-10.6.3 build on NV to see if it was fixed

	if (doPushPop)
	{
		glPushAttrib( 0 );
	}
	
	//----------------------------------------------------------------- figure out the plan
	
	bool blitTwoStep = false;		// think positive
	
	// each subsequent segment here can only set blitTwoStep, not clear it.
	// the common case where these get hit is resolve out to presentation
	// there may be GL extensions or driver revisions which start doing these safely.
	// ideally many blits internally resolve without scaling and can thus go direct without using the scratch tex.
	
	if (blitResolves && (blitFlips||blitToBack))		// flips, blit to back, same thing (for now)
	{
		if( gl_cannot_resolve_flipped.GetInt() )
		{
			blitTwoStep = true;
		}
		else if (!gl_can_resolve_flipped.GetInt())
		{
			blitTwoStep = blitTwoStep || m_caps.m_cantResolveFlipped;	// if neither convar renders an opinion, fall back to the caps to decide if we have to two-step.
		}		
	}

	// only consider trying to use the scaling resolve filter,
	// if we are confident we are not headed for two step mode already.
	if (!blitTwoStep)
	{
		if (blitResolves && blitScales)
		{
			if (m_caps.m_cantResolveScaled)
			{
				// filter is unchanged, two step mode switches on
				blitTwoStep = true;
			}
			else
			{
				bool	blitScalesDown	= ((srcRect->xmax - srcRect->xmin) > (dstRect->xmax - dstRect->xmin)) || ((srcRect->ymax - srcRect->ymin) > (dstRect->ymax - dstRect->ymin));
				int		mode			= (blitScalesDown) ? gl_minify_resolve_mode.GetInt() : gl_magnify_resolve_mode.GetInt();
				
				// roughly speaking, resolve blits that minify represent setup for special effects ("copy framebuffer to me")
				// resolve blits that magnify are almost always on the final present in the case where remder size < display size
				
				switch( mode )
				{
					case 0:
					default:
						// filter is unchanged, two step mode
						blitTwoStep = true;
					break;
						
					case 1:
						// filter goes to fastest, one step mode
						blitTwoStep = false;
						filter = XGL_SCALED_RESOLVE_FASTEST_EXT;
					break;
						
					case 2:
						// filter goes to nicest, one step mode
						blitTwoStep = false;
						filter = XGL_SCALED_RESOLVE_NICEST_EXT;
					break;					
				}
			}
		}	
	}

	//----------------------------------------------------------------- save old scissor state and disable scissor
	GLScissorEnable_t	oldsciss,newsciss;
	m_ScissorEnable.Read( &oldsciss, 0 );

	//	turn off scissor
	newsciss.enable = false;
	m_ScissorEnable.Write( &newsciss, true, true );

	//----------------------------------------------------------------- fork in the road, depending on two-step or not
	if (blitTwoStep)
	{
		// a resolve that can't be done directly due to constraints on scaling or flipping.
		
		// bind scratch FBO0 to read, scrub it, attach RBO
		BindFBOToCtx		( m_scratchFBO[0], GL_READ_FRAMEBUFFER_EXT );							GLMCheckError();						
		glScrubFBO			( GL_READ_FRAMEBUFFER_EXT );
		glAttachRBOtoFBO	( GL_READ_FRAMEBUFFER_EXT, formatClass, srcTex->m_rboName );
		
		// bind scratch FBO1 to write, scrub it, attach scratch tex
		BindFBOToCtx		( m_scratchFBO[1], GL_DRAW_FRAMEBUFFER_EXT );							GLMCheckError();						
		glScrubFBO			( GL_DRAW_FRAMEBUFFER_EXT );
		glAttachTex2DtoFBO	( GL_DRAW_FRAMEBUFFER_EXT, formatClass, srcTex->m_texName, 0 );

		// set read and draw buffers appropriately		
		glReadBuffer		( glAttachFromClass[formatClass] );
		glDrawBuffer		( glAttachFromClass[formatClass] );
		
		// blit#1 - to resolve to scratch
		// implicitly means no scaling, thus will be done with NEAREST sampling

		GLenum resolveFilter = GL_NEAREST;
		
		glBlitFramebufferEXT(	0, 0,	srcTex->m_layout->m_key.m_xSize, srcTex->m_layout->m_key.m_ySize,
								0, 0,	srcTex->m_layout->m_key.m_xSize, srcTex->m_layout->m_key.m_ySize,	// same source and dest rect, whole surface
								blitMask, resolveFilter );
		GLMCheckError();
								
		// FBO1 now holds the interesting content.
		// scrub FBO0, bind FBO1 to READ, fall through to next stage of blit where 1 goes onto 0 (or BACK)
		
		glScrubFBO			( GL_READ_FRAMEBUFFER_EXT );	// zap FBO0
		BindFBOToCtx		( m_scratchFBO[1], GL_READ_FRAMEBUFFER_EXT );							GLMCheckError();								
	}
	else
	{
		// arrange source surface on FBO1 for blit directly to dest (which could be FBO0 or BACK)
		BindFBOToCtx		( m_scratchFBO[1], GL_READ_FRAMEBUFFER_EXT );							GLMCheckError();						
		glScrubFBO			( GL_READ_FRAMEBUFFER_EXT );
		if (blitResolves)
		{
			glAttachRBOtoFBO( GL_READ_FRAMEBUFFER_EXT, formatClass, srcTex->m_rboName );		
		}
		else
		{
			glAttachTex2DtoFBO( GL_READ_FRAMEBUFFER_EXT, formatClass, srcTex->m_texName, srcMip );
		}	
		
		glReadBuffer( glAttachFromClass[formatClass] );
	}
	
	//----------------------------------------------------------------- zero or one blits may have happened above, whichever took place, FBO1 is now on read
	
	bool yflip = false;
	if (blitToBack)
	{
		// backbuffer is special - FBO0 is left out (either scrubbed already, or not used)
		
		BindFBOToCtx		( NULL, GL_DRAW_FRAMEBUFFER_EXT );										GLMCheckError();								
		glDrawBuffer		( GL_BACK );															GLMCheckError();								
		
		yflip = true;
	}
	else
	{
		// not going to GL_BACK - use FBO0. set up dest tex or RBO on it.  i.e. it's OK to blit from MSAA to MSAA if needed, though unlikely.
		Assert( dstTex != NULL );

		BindFBOToCtx		( m_scratchFBO[0], GL_DRAW_FRAMEBUFFER_EXT );							GLMCheckError();								
		glScrubFBO			( GL_DRAW_FRAMEBUFFER_EXT );

		if (dstTex->m_rboName)
		{
			glAttachRBOtoFBO( GL_DRAW_FRAMEBUFFER_EXT, formatClass, dstTex->m_rboName );		
		}
		else
		{
			glAttachTex2DtoFBO( GL_DRAW_FRAMEBUFFER_EXT, formatClass, dstTex->m_texName, dstMip );
		}	

		glDrawBuffer		( glAttachFromClass[formatClass] );										GLMCheckError();								
	}

	// final blit
	
	// i think in general, if we are blitting same size, gl_nearest is the right filter to pass.
	// this re-steering won't kick in if there is scaling or a special scaled resolve going on.
	if (!blitScales)
	{
		// steer it
//		printf("\n re-setting blit filter from %x/%s to GL_NEAREST", filter, GLMDecode(eGL_ENUM, filter) );
		filter = GL_NEAREST;
	}
	
	// this is blit #1 or #2 depending on what took place above.
	if (yflip)
	{
		glBlitFramebufferEXT(	srcRect->xmin, srcRect->ymin, srcRect->xmax, srcRect->ymax,
								dstRect->xmin, dstRect->ymax, dstRect->xmax, dstRect->ymin,		// note dest Y's are flipped
								blitMask, filter );
	}
	else
	{
		glBlitFramebufferEXT(	srcRect->xmin, srcRect->ymin, srcRect->xmax, srcRect->ymax,
								dstRect->xmin, dstRect->ymin, dstRect->xmax, dstRect->ymax,
								blitMask, filter );
	}
	GLMCheckError();

	//----------------------------------------------------------------- scrub READ and maybe DRAW FBO, and unbind

	glScrubFBO			( GL_READ_FRAMEBUFFER_EXT );
	BindFBOToCtx		( NULL, GL_READ_FRAMEBUFFER_EXT );											GLMCheckError();								
	if (!blitToBack)
	{
		glScrubFBO			( GL_DRAW_FRAMEBUFFER_EXT );
		BindFBOToCtx		( NULL, GL_DRAW_FRAMEBUFFER_EXT );										GLMCheckError();								
	}
		
	//----------------------------------------------------------------- restore GLM's drawing FBO

	//	restore GLM drawing FBO
	BindFBOToCtx( m_drawingFBO, GL_READ_FRAMEBUFFER_EXT );											GLMCheckError();						
	BindFBOToCtx( m_drawingFBO, GL_DRAW_FRAMEBUFFER_EXT );											GLMCheckError();						

	if (doPushPop)
	{
		glPopAttrib( );
	}
	

	//----------------------------------------------------------------- restore old scissor state
	m_ScissorEnable.Write( &oldsciss, true, true );
}


void	GLMContext::BlitTex( CGLMTex *srcTex, GLMRect *srcRect, int srcFace, int srcMip, CGLMTex *dstTex, GLMRect *dstRect, int dstFace, int dstMip, GLenum filter, bool useBlitFB )
{
	switch( srcTex->m_layout->m_format->m_glDataFormat )
	{
		case GL_BGRA:
		case GL_RGB:
		case GL_RGBA:
		case GL_ALPHA:
		case GL_LUMINANCE:
		case GL_LUMINANCE_ALPHA:
			#if 0
				if (GLMKnob("caps-key",NULL) > 0.0)
				{
					useBlitFB = false;
				}
			#endif

			if ( m_caps.m_cantBlitReliably )	// this is referring to a problem with the x3100..
			{
				useBlitFB = false;
			}
		break;
	}
	
	if (0)
	{
		GLMPRINTF(("-D- Blit from %d %d %d %d  to %d %d %d %d",
			srcRect->xmin, srcRect->ymin, srcRect->xmax, srcRect->ymax,
			dstRect->xmin, dstRect->ymin, dstRect->xmax, dstRect->ymax
		));
		
		GLMPRINTF(( "-D-       src tex layout is %s", srcTex->m_layout->m_layoutSummary ));
		GLMPRINTF(( "-D-       dst tex layout is %s", dstTex->m_layout->m_layoutSummary ));
	}

	int pushed = 0;
	uint pushmask = gl_radar7954721_workaround_maskval.GetInt();
		//GL_COLOR_BUFFER_BIT
		//| GL_CURRENT_BIT
		//| GL_ENABLE_BIT
		//| GL_FOG_BIT
		//| GL_PIXEL_MODE_BIT
		//| GL_SCISSOR_BIT
		//| GL_STENCIL_BUFFER_BIT
		//| GL_TEXTURE_BIT
		//GL_VIEWPORT_BIT
		//;
	
	if (gl_radar7954721_workaround_all.GetInt()!=0)
	{
		glPushAttrib( pushmask );
		pushed++;
	}
	else
	{
		bool srcGamma = (srcTex->m_layout->m_key.m_texFlags & kGLMTexSRGB) != 0;
		bool dstGamma = (dstTex->m_layout->m_key.m_texFlags & kGLMTexSRGB) != 0;

		if (srcGamma != dstGamma)
		{
			if (gl_radar7954721_workaround_mixed.GetInt())
			{
				glPushAttrib( pushmask );
				pushed++;
			}
		}
	}

	if (useBlitFB)
	{
		// state we need to save
		//	current setting of scissor
		//	current setting of the drawing fbo (no explicit save, it's in the context)
		GLScissorEnable_t	oldsciss,newsciss;
		m_ScissorEnable.Read( &oldsciss, 0 );

		// remember to restore m_drawingFBO at end of effort
		
		// setup
		//	turn off scissor
		newsciss.enable = false;
		m_ScissorEnable.Write( &newsciss, true, true );

		// select which attachment enum we're going to use for the blit
		// default to color0, unless it's a depth or stencil flava
		
		Assert( srcTex->m_layout->m_format->m_glDataFormat == dstTex->m_layout->m_format->m_glDataFormat );
		
		EGLMFBOAttachment	attachIndex = (EGLMFBOAttachment)0;
		GLenum				attachIndexGL = 0;
		GLuint				blitMask = 0;
		switch( srcTex->m_layout->m_format->m_glDataFormat )
		{
			case GL_BGRA:
			case GL_RGB:
			case GL_RGBA:
			case GL_ALPHA:
			case GL_LUMINANCE:
			case GL_LUMINANCE_ALPHA:
				attachIndex = kAttColor0;
				attachIndexGL = GL_COLOR_ATTACHMENT0_EXT;
				blitMask = GL_COLOR_BUFFER_BIT;
			break;

			case GL_DEPTH_COMPONENT:
				attachIndex = kAttDepth;
				attachIndexGL = GL_DEPTH_ATTACHMENT_EXT;
				blitMask = GL_DEPTH_BUFFER_BIT;
			break;
			
			case GL_DEPTH_STENCIL_EXT:
				attachIndex = kAttDepthStencil;
				attachIndexGL = GL_DEPTH_STENCIL_ATTACHMENT_EXT;
				blitMask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
			break;

			default:
				Assert(0);
			break;
		}

		//	set the read fb, attach read tex at appropriate attach point, set read buffer
		BindFBOToCtx( m_blitReadFBO, GL_READ_FRAMEBUFFER_EXT );
		GLMCheckError();						

		GLMFBOTexAttachParams attparams;
		attparams.m_tex		=	srcTex;
		attparams.m_face	=	srcFace;
		attparams.m_mip		=	srcMip;
		attparams.m_zslice	=	0;
		m_blitReadFBO->TexAttach( &attparams, attachIndex, GL_READ_FRAMEBUFFER_EXT );
		GLMCheckError();						

		glReadBuffer( attachIndexGL );
		GLMCheckError();						
		

		//	set the write fb and buffer, and attach write tex
		BindFBOToCtx( m_blitDrawFBO, GL_DRAW_FRAMEBUFFER_EXT );
		GLMCheckError();						

		attparams.m_tex		=	dstTex;
		attparams.m_face	=	dstFace;
		attparams.m_mip		=	dstMip;
		attparams.m_zslice	=	0;
		m_blitDrawFBO->TexAttach( &attparams, attachIndex, GL_DRAW_FRAMEBUFFER_EXT );
		GLMCheckError();						

		glDrawBuffer( attachIndexGL );
		GLMCheckError();						

		//	do the blit
		glBlitFramebufferEXT(	srcRect->xmin, srcRect->ymin, srcRect->xmax, srcRect->ymax,
								dstRect->xmin, dstRect->ymin, dstRect->xmax, dstRect->ymax,
								blitMask, filter );
		GLMCheckError();						
							
		// cleanup
		//	unset the read fb and buffer, detach read tex
		//	unset the write fb and buffer, detach write tex

		m_blitReadFBO->TexDetach( attachIndex, GL_READ_FRAMEBUFFER_EXT );
		GLMCheckError();						

		m_blitDrawFBO->TexDetach( attachIndex, GL_DRAW_FRAMEBUFFER_EXT );
		GLMCheckError();						

		//	put the original FB back in place (both read and draw)
		// this bind will hit both read and draw bindings
		BindFBOToCtx( m_drawingFBO, GL_READ_FRAMEBUFFER_EXT );
		GLMCheckError();						
		BindFBOToCtx( m_drawingFBO, GL_DRAW_FRAMEBUFFER_EXT );
		GLMCheckError();						

			//	set the read and write buffers back to... what ? does it matter for anything but copies ?  don't worry about it
		
		// restore the scissor state
		m_ScissorEnable.Write( &oldsciss, true, true );
	}
	else
	{
		// textured quad style

		// we must attach the dest tex as the color buffer on the blit draw FBO
		// so that means we need to re-set the drawing FBO on exit

		EGLMFBOAttachment	attachIndex = (EGLMFBOAttachment)0;
		GLenum				attachIndexGL = 0;
		switch( srcTex->m_layout->m_format->m_glDataFormat )
		{
			case GL_BGRA:
			case GL_RGB:
			case GL_RGBA:
			case GL_ALPHA:
			case GL_LUMINANCE:
			case GL_LUMINANCE_ALPHA:
				attachIndex = kAttColor0;
				attachIndexGL = GL_COLOR_ATTACHMENT0_EXT;
			break;

			default:
				Assert(!"Can't blit that format");
			break;
		}
		
		BindFBOToCtx( m_blitDrawFBO, GL_DRAW_FRAMEBUFFER_EXT );
		GLMCheckError();						

		GLMFBOTexAttachParams attparams;
		attparams.m_tex		=	dstTex;
		attparams.m_face	=	dstFace;
		attparams.m_mip		=	dstMip;
		attparams.m_zslice	=	0;
		m_blitDrawFBO->TexAttach( &attparams, attachIndex, GL_DRAW_FRAMEBUFFER_EXT );
		GLMCheckError();						

		glDrawBuffer( attachIndexGL );
		GLMCheckError();						
		
		// attempt to just set states directly the way we want them, then use the latched states to repair them afterward.
		this->NullProgram();	// out of program mode
		
		glDisable ( GL_ALPHA_TEST );
		glDisable ( GL_CULL_FACE );
		glDisable ( GL_POLYGON_OFFSET_FILL );
		glDisable ( GL_SCISSOR_TEST );

		glDisable ( GL_CLIP_PLANE0 );
		glDisable ( GL_CLIP_PLANE1 );
		
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glDisable ( GL_BLEND );

		glDepthMask ( GL_FALSE );
		glDisable ( GL_DEPTH_TEST );

		glDisable ( GL_STENCIL_TEST );
		glStencilMask ( GL_FALSE );

		GLMCheckError();

		// now do the unlit textured quad...
		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_2D, srcTex->m_texName );
		GLMCheckError();

		glEnable(GL_TEXTURE_2D);
		GLMCheckError();

		// immediate mode is fine

		float topv = 1.0;
		float botv = 0.0;
		
		glBegin(GL_QUADS);
			glTexCoord2f	( 0.0, botv );
			glVertex3f		( -1.0, -1.0, 0.0 );
			
			glTexCoord2f	( 1.0, botv );
			glVertex3f		( 1.0, -1.0, 0.0 );
			
			glTexCoord2f	( 1.0, topv );
			glVertex3f		( 1.0, 1.0, 0.0 );

			glTexCoord2f	( 0.0, topv );
			glVertex3f		( -1.0, 1.0, 0.0 );
		glEnd();
		GLMCheckError();

		glBindTexture( GL_TEXTURE_2D, 0 );
		GLMCheckError();

		glDisable(GL_TEXTURE_2D);
		GLMCheckError();

		// invalidate tex binding 0 so it gets reset
		m_samplers[0].m_boundTex = NULL;
		
		// leave active program empty - flush draw states will fix
		
		// then restore states using the scoreboard

		m_AlphaTestEnable.Flush( true );
		m_AlphaToCoverageEnable.Flush( true );
		m_CullFaceEnable.Flush( true );
		m_DepthBias.Flush( true );
		m_ScissorEnable.Flush( true );
		
		m_ClipPlaneEnable.FlushIndex( 0, true );
		m_ClipPlaneEnable.FlushIndex( 1, true );
		
		m_ColorMaskSingle.Flush( true );
		m_BlendEnable.Flush( true );

		m_DepthMask.Flush( true );
		m_DepthTestEnable.Flush( true );
		
		m_StencilWriteMask.Flush( true );
		m_StencilTestEnable.Flush( true );

		//	unset the write fb and buffer, detach write tex

		m_blitDrawFBO->TexDetach( attachIndex, GL_DRAW_FRAMEBUFFER_EXT );
		GLMCheckError();						

		//	put the original FB back in place (both read and draw)
		BindFBOToCtx( m_drawingFBO, GL_READ_FRAMEBUFFER_EXT );
		GLMCheckError();						
		BindFBOToCtx( m_drawingFBO, GL_DRAW_FRAMEBUFFER_EXT );
		GLMCheckError();						
	}
	
	while(pushed)
	{
		glPopAttrib();
		pushed--;
	}
}

void	GLMContext::ResolveTex( CGLMTex *tex, bool forceDirty )
{
	// only run resolve if it's (a) possible and (b) dirty or force-dirtied
	if ( (tex->m_rboName) && ((tex->m_rboDirty)||forceDirty) )
	{
		// state we need to save
		//	current setting of scissor
		//	current setting of the drawing fbo (no explicit save, it's in the context)
		GLScissorEnable_t	oldsciss,newsciss;
		m_ScissorEnable.Read( &oldsciss, 0 );

		// remember to restore m_drawingFBO at end of effort
		
		// setup
		//	turn off scissor
		newsciss.enable = false;
		m_ScissorEnable.Write( &newsciss, true, true );

		// select which attachment enum we're going to use for the blit
		// default to color0, unless it's a depth or stencil flava
		
		// for resolve, only handle a modest subset of the possible formats
		EGLMFBOAttachment	attachIndex = (EGLMFBOAttachment)0;
		GLenum				attachIndexGL = 0;
		GLuint				blitMask = 0;
		switch( tex->m_layout->m_format->m_glDataFormat )
		{
			case GL_BGRA:
			case GL_RGB:
			case GL_RGBA:
	//		case GL_ALPHA:
	//		case GL_LUMINANCE:
	//		case GL_LUMINANCE_ALPHA:
				attachIndex = kAttColor0;
				attachIndexGL = GL_COLOR_ATTACHMENT0_EXT;
				blitMask = GL_COLOR_BUFFER_BIT;
			break;

	//		case GL_DEPTH_COMPONENT:
	//			attachIndex = kAttDepth;
	//			attachIndexGL = GL_DEPTH_ATTACHMENT_EXT;
	//			blitMask = GL_DEPTH_BUFFER_BIT;
	//		break;
			
			case GL_DEPTH_STENCIL_EXT:
				attachIndex = kAttDepthStencil;
				attachIndexGL = GL_DEPTH_STENCIL_ATTACHMENT_EXT;
				blitMask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
			break;

			default:
				Assert(!"Unsupported format for MSAA resolve" );
			break;
		}

		
		//	set the read fb, attach read RBO at appropriate attach point, set read buffer
		BindFBOToCtx( m_blitReadFBO, GL_READ_FRAMEBUFFER_EXT );
		GLMCheckError();						

		// going to avoid the TexAttach / TexDetach calls due to potential confusion, implement it directly here
		
		//-----------------------------------------------------------------------------------
		// put tex->m_rboName on the read FB's attachment
		if (attachIndexGL==GL_DEPTH_STENCIL_ATTACHMENT_EXT)
		{
			// you have to attach it both places...
			// http://www.opengl.org/wiki/GL_EXT_framebuffer_object

			// bind the RBO to the GL_RENDERBUFFER_EXT target - is this extraneous ?
			//glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, tex->m_rboName );
			//GLMCheckError();
			
			// attach the GL_RENDERBUFFER_EXT target to the depth and stencil attach points
			glFramebufferRenderbufferEXT( GL_READ_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, tex->m_rboName);						
			GLMCheckError();
				
			glFramebufferRenderbufferEXT( GL_READ_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, tex->m_rboName);
			GLMCheckError();

			// no need to leave the RBO hanging on
			//glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, 0 );
			//GLMCheckError();
		}
		else
		{
			//glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, tex->m_rboName );
			//GLMCheckError();
			
			glFramebufferRenderbufferEXT( GL_READ_FRAMEBUFFER_EXT, attachIndexGL, GL_RENDERBUFFER_EXT, tex->m_rboName);
			GLMCheckError();

			//glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, 0 );
			//GLMCheckError();
		}

		glReadBuffer( attachIndexGL );
		GLMCheckError();						

		//-----------------------------------------------------------------------------------
		// put tex->m_texName on the draw FBO attachment

		//	set the write fb and buffer, and attach write tex
		BindFBOToCtx( m_blitDrawFBO, GL_DRAW_FRAMEBUFFER_EXT );
		GLMCheckError();						

		// regular path - attaching a texture2d
		
		if (attachIndexGL==GL_DEPTH_STENCIL_ATTACHMENT_EXT)
		{
			glFramebufferTexture2DEXT( GL_DRAW_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, tex->m_texName, 0 );
			GLMCheckError();

			glFramebufferTexture2DEXT( GL_DRAW_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, tex->m_texName, 0 );
			GLMCheckError();
		}
		else
		{
			glFramebufferTexture2DEXT( GL_DRAW_FRAMEBUFFER_EXT, attachIndexGL, GL_TEXTURE_2D, tex->m_texName, 0 );
			GLMCheckError();
		}

		glDrawBuffer( attachIndexGL );
		GLMCheckError();						

		//-----------------------------------------------------------------------------------

		// blit
		glBlitFramebufferEXT(	0, 0,	tex->m_layout->m_key.m_xSize, tex->m_layout->m_key.m_ySize,
								0, 0,	tex->m_layout->m_key.m_xSize, tex->m_layout->m_key.m_ySize,
								blitMask, GL_NEAREST );
			// or should it be GL_LINEAR?  does it matter ?
			
		GLMCheckError();						
							
		//-----------------------------------------------------------------------------------
		// cleanup
		//-----------------------------------------------------------------------------------


		//	unset the read fb and buffer, detach read RBO
		//glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, 0 );
		//GLMCheckError();

		if (attachIndexGL==GL_DEPTH_STENCIL_ATTACHMENT_EXT)
		{
			// detach the GL_RENDERBUFFER_EXT target from the depth and stencil attach points
			glFramebufferRenderbufferEXT( GL_READ_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);						
			GLMCheckError();
				
			glFramebufferRenderbufferEXT( GL_READ_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);
			GLMCheckError();
		}
		else
		{
			glFramebufferRenderbufferEXT( GL_READ_FRAMEBUFFER_EXT, attachIndexGL, GL_RENDERBUFFER_EXT, 0);
			GLMCheckError();
		}

		//-----------------------------------------------------------------------------------
		//	unset the write fb and buffer, detach write tex
		

		if (attachIndexGL==GL_DEPTH_STENCIL_ATTACHMENT_EXT)
		{
			glFramebufferTexture2DEXT( GL_DRAW_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, 0, 0 );
			GLMCheckError();

			glFramebufferTexture2DEXT( GL_DRAW_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_TEXTURE_2D, 0, 0 );
			GLMCheckError();
		}
		else
		{
			glFramebufferTexture2DEXT( GL_DRAW_FRAMEBUFFER_EXT, attachIndexGL, GL_TEXTURE_2D, 0, 0 );
			GLMCheckError();
		}

		//	put the original FB back in place (both read and draw)
		// this bind will hit both read and draw bindings
		BindFBOToCtx( m_drawingFBO, GL_READ_FRAMEBUFFER_EXT );
		GLMCheckError();						
		BindFBOToCtx( m_drawingFBO, GL_DRAW_FRAMEBUFFER_EXT );
		GLMCheckError();						

		//	set the read and write buffers back to... what ? does it matter for anything but copies ?  don't worry about it
		
		// restore the scissor state
		m_ScissorEnable.Write( &oldsciss, true, true );
		
		// mark the RBO clean on the resolved tex
		tex->m_rboDirty = false;
	}
}

void	GLMContext::PreloadTex( CGLMTex *tex, bool force )
{
	// if conditions allow (i.e. a drawing surface is active)
	// bind the texture on TMU 15
	// set up a dummy program to sample it but not write (use 'discard')
	// draw a teeny little triangle that won't generate a lot of fragments
	if (!m_pairCache)
		return;
		
	if (!m_drawingFBO)
		return;
		
	if (!m_drawingFBO)
		return;
		
	if (tex->m_texPreloaded && !force)	// only do one preload unless forced to re-do
	{
		//printf("\nnot-preloading %s", tex->m_debugLabel ? tex->m_debugLabel : "(unknown)");
		return;
	}

	//printf("\npreloading     %s", tex->m_debugLabel ? tex->m_debugLabel : "(unknown)");

	CGLMProgram *vp = m_preloadTexVertexProgram;
	CGLMProgram *fp = NULL;
	switch(tex->m_layout->m_key.m_texGLTarget)
	{
		case GL_TEXTURE_2D:			fp = m_preload2DTexFragmentProgram;
		break;
		
		case GL_TEXTURE_3D:			fp = m_preload3DTexFragmentProgram;
		break;
		
		case GL_TEXTURE_CUBE_MAP:	fp = m_preloadCubeTexFragmentProgram;
		break;
	}
	if (!fp)
		return;
	
	CGLMShaderPair	*preloadPair = m_pairCache->SelectShaderPair( vp, fp, 0 );
	if (!preloadPair)
		return;

	GLhandleARB		pairProgram		= preloadPair->m_program;
	uint			pairRevision	= preloadPair->m_revision;
		
	m_boundPair = preloadPair;
	m_boundPairProgram = pairProgram;
	m_boundPairRevision = pairRevision;

	glUseProgram( (GLuint)pairProgram );
	GLMCheckError();
					
	// note the binding (not really bound.. just sitting in the linked active GLSL program)
	m_boundProgram[ kGLMVertexProgram ] = vp;
	m_boundProgram[ kGLMFragmentProgram ] = fp;

	// almost ready to draw...

	int tmuForPreload = 15;
	if(!m_boundPair->m_samplersFixed)
	{
		if (m_boundPair->m_locSamplers[tmuForPreload] >=0)
		{
			glUniform1iARB( m_boundPair->m_locSamplers[tmuForPreload], tmuForPreload );
			GLMCheckError();
		}
		m_boundPair->m_samplersFixed = true;
	}
	
	// shut down all the generic attribute arrays on the detention level - next real draw will activate them again
	m_lastKnownVertexAttribMask = 0;
	for( int index=0; index < kGLMVertexAttributeIndexMax; index++ )
	{
		glDisableVertexAttribArray( index );
		GLMCheckError();
	}

	
	// bind texture 
	this->BindTexToTMU( tex, 15 );
	
	// unbind vertex/index buffers
	this->BindBufferToCtx( kGLMVertexBuffer, NULL );
	this->BindBufferToCtx( kGLMIndexBuffer, NULL );
	
	// draw
	static float posns[] = {	0.0f, 0.0f, 0.0f,
								0.0f, 0.0f, 0.0f,
								0.0f, 0.0f, 0.0f };

	static int indices[] = { 0, 1, 2 };
	

	glEnableVertexAttribArray( 0 );
	GLMCheckError();

	glVertexAttribPointer( 0, 3, GL_FLOAT, 0, 0, posns );
	GLMCheckError();

	glDrawRangeElements( GL_TRIANGLES, 0, 3, 3, GL_UNSIGNED_INT, indices);
	GLMCheckError();

	glDisableVertexAttribArray( 0 );
	GLMCheckError();

	m_lastKnownVertexAttribMask = 0;
	m_lastKnownVertexAttribs[0].m_bufferRevision -= 1;	// force mismatch so next FlushDrawStates restores the right attrib source
	
	this->BindTexToTMU( NULL, 15 );

	tex->m_texPreloaded = true;
}

		
void	GLMContext::SetSamplerTex( int sampler, CGLMTex *tex )
{
	GLM_FUNC;
	CheckCurrent();

	m_samplers[sampler].m_drawTex = tex;
}

void	GLMContext::SetSamplerParams( int sampler, GLMTexSamplingParams *params )
{
	GLM_FUNC;
	CheckCurrent();

	m_samplers[sampler].m_samp = *params;
}


CGLMFBO	*GLMContext::NewFBO( void )
{
	GLM_FUNC;
	MakeCurrent();

	CGLMFBO *fbo = new CGLMFBO( this );

	m_fboTable.AddToTail( fbo );
	
	return fbo;
}

void	GLMContext::DelFBO( CGLMFBO *fbo )
{
	GLM_FUNC;
	MakeCurrent();

	if (m_drawingFBO == fbo)
	{
		m_drawingFBO = NULL;	//poof!
	}
	
	if (m_boundReadFBO == fbo )
	{
		this->BindFBOToCtx( NULL, GL_READ_FRAMEBUFFER_EXT );
		m_boundReadFBO = NULL;
	}

	if (m_boundDrawFBO == fbo )
	{
		this->BindFBOToCtx( NULL, GL_DRAW_FRAMEBUFFER_EXT );
		m_boundDrawFBO = NULL;
	}

	m_fboTable.FastRemove( m_fboTable.Find( fbo ) );
	
	delete fbo;
}

void	GLMContext::SetDrawingFBO( CGLMFBO *fbo )
{
	GLM_FUNC;
	CheckCurrent();

	// might want to validate that fbo object?
	m_drawingFBO = fbo;
}

//===============================================================================

CGLMProgram	*GLMContext::NewProgram( EGLMProgramType type, char *progString )
{
	//hushed GLM_FUNC;

	MakeCurrent();

	CGLMProgram *prog = new CGLMProgram( this, type );
	
	prog->SetProgramText( progString );
	bool compile_ok = prog->CompileActiveSources();

	AssertOnce( compile_ok );

	return prog;
}

void	GLMContext::DelProgram( CGLMProgram *prog )
{
	GLM_FUNC;

	this->MakeCurrent();

	if (m_drawingProgram[ prog->m_type ] == prog)
	{
		m_drawingProgram[ prog->m_type ] = NULL;
	}

	// make sure to eliminate any cached pairs using this shader
	bool purgeResult = m_pairCache->PurgePairsWithShader( prog );
	Assert( !purgeResult );	// very unlikely to trigger

	this->NullProgram();
	
	delete prog;
}

void	GLMContext::NullProgram( void )
{
	// just unbind everything on a prog delete
	glSetEnable( GL_VERTEX_PROGRAM_ARB, false );
	glSetEnable( GL_FRAGMENT_PROGRAM_ARB, false );
	
	glBindProgramARB( GL_VERTEX_PROGRAM_ARB, 0 );
	glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, 0 );

	glUseProgram( 0 );
	m_boundPair			= NULL;
	m_boundPairRevision	= 0xFFFFFFFF;
	m_boundPairProgram	= (GLhandleARB)0xFFFFFFFF;
	
	m_boundProgram[ kGLMVertexProgram ] = NULL;
	m_boundProgram[ kGLMFragmentProgram ] = NULL;
}

void	GLMContext::SetDrawingProgram( EGLMProgramType type, CGLMProgram *prog )
{
	GLM_FUNC;

	this->MakeCurrent();

	if (prog)	// OK to pass NULL..
	{
		if (type != prog->m_type)
		{
			Debugger();
		}
	}
	else
	{
		// if a null fragment program is passed, we activate our special null program
		// thus FP is always always enabled.
		if (type==kGLMFragmentProgram)
		{
			prog = m_nullFragmentProgram;
		}
		else
		{
			//Assert(!"Tried to set NULL vertex program");
		}
	}
	m_drawingProgram[type] = prog;
}

void	GLMContext::SetDrawingLang( EGLMProgramLang lang, bool immediate )
{
	if ( !m_caps.m_hasDualShaders ) return;		// ignore attempts to change language when -glmdualshaders is not engaged
	
	m_drawingLangAtFrameStart = lang;
	if (immediate)
	{	
		this->NullProgram();

		m_drawingLang = m_drawingLangAtFrameStart;
	}
}

void	GLMContext::LinkShaderPair( CGLMProgram *vp, CGLMProgram *fp )
{
	if ( (m_pairCache) && (m_drawingLang==kGLMGLSL) && (vp && vp->m_descs[kGLMGLSL].m_valid) && (fp && fp->m_descs[kGLMGLSL].m_valid) )
	{
		CGLMShaderPair	*pair = m_pairCache->SelectShaderPair( vp, fp, 0 );
		
		Assert( pair != NULL );

		this->NullProgram();	// clear out any binds that were done - next draw will set it right
	}
}

void	GLMContext::ClearShaderPairCache( void )
{
	if (m_pairCache)
	{
		this->NullProgram();
		m_pairCache->Purge();	// bye bye all linked pairs
		this->NullProgram();
	}
}

void	GLMContext::QueryShaderPair( int index, GLMShaderPairInfo *infoOut )
{
	if (m_pairCache)
	{
		m_pairCache->QueryShaderPair( index, infoOut );		
	}
	else
	{
		memset( infoOut, sizeof( *infoOut ), 0 );
		infoOut->m_status = -1;
	}
}

void	GLMContext::SetProgramParametersF( EGLMProgramType type, uint baseSlot, float *slotData, uint slotCount )
{
	GLM_FUNC;

	Assert( baseSlot < kGLMProgramParamFloat4Limit );
	Assert( baseSlot+slotCount <= kGLMProgramParamFloat4Limit );
	
	GLMPRINTF(("-S-GLMContext::SetProgramParametersF %s slots %d - %d: ", (type==kGLMVertexProgram) ? "VS" : "FS", baseSlot, baseSlot + slotCount - 1 ));
	for( int i=0; i<slotCount; i++ )
	{
		GLMPRINTF((		"-S-    %03d: [ %7.4f %7.4f %7.4f %7.4f ]",
						baseSlot+i,
						slotData[i*4], slotData[i*4+1], slotData[i*4+2], slotData[i*4+3]
						));
	}
	
	// copy to mirror
	// actual delivery happens in FlushDrawStates now
	memcpy( &m_programParamsF[type].m_values[baseSlot][0], slotData, (4 * sizeof(float)) * slotCount );

	// adjust dirty count
	if ( (baseSlot+slotCount) > m_programParamsF[type].m_dirtySlotCount)
	{
		m_programParamsF[type].m_dirtySlotCount = baseSlot+slotCount;
	}
}

void	GLMContext::SetProgramParametersB( EGLMProgramType type, uint baseSlot, int *slotData, uint boolCount )
{
	GLM_FUNC;

	Assert( m_drawingLang == kGLMGLSL );
	Assert( type==kGLMVertexProgram );
	
	Assert( baseSlot < kGLMProgramParamBoolLimit );
	Assert( baseSlot+boolCount <= kGLMProgramParamBoolLimit );
	
	GLMPRINTF(("-S-GLMContext::SetProgramParametersB %s bools %d - %d: ", (type==kGLMVertexProgram) ? "VS" : "FS", baseSlot, baseSlot + boolCount - 1 ));
	for( int i=0; i<boolCount; i++ )
	{
		GLMPRINTF((		"-S-    %03d: %d (bool)",
						baseSlot+i,
						slotData[i]
						));
	}
	
	// copy to mirror
	// actual delivery happens in FlushDrawStates now
	memcpy( &m_programParamsB[type].m_values[baseSlot], slotData, sizeof(int) * boolCount );
	
	// adjust dirty count
	if ( (baseSlot+boolCount) > m_programParamsB[type].m_dirtySlotCount)
	{
		m_programParamsB[type].m_dirtySlotCount = baseSlot+boolCount;
	}
}

void	GLMContext::SetProgramParametersI( EGLMProgramType type, uint baseSlot, int *slotData, uint slotCount )	// groups of 4 ints...
{
	GLM_FUNC;

	Assert( m_drawingLang == kGLMGLSL );
	Assert( type==kGLMVertexProgram );
	
	Assert( baseSlot < kGLMProgramParamInt4Limit );
	Assert( baseSlot+slotCount <= kGLMProgramParamInt4Limit );
	
	GLMPRINTF(("-S-GLMContext::SetProgramParametersI %s slots %d - %d: ", (type==kGLMVertexProgram) ? "VS" : "FS", baseSlot, baseSlot + slotCount - 1 ));
	for( int i=0; i<slotCount; i++ )
	{
		GLMPRINTF((		"-S-    %03d: %d %d %d %d (int4)",
						baseSlot+i,
						slotData[i*4],slotData[i*4+1],slotData[i*4+2],slotData[i*4+3]
						));
	}
	
	// copy to mirror
	// actual delivery happens in FlushDrawStates now
	memcpy( &m_programParamsI[type].m_values[baseSlot][0], slotData, (4*sizeof(int)) * slotCount );

	// adjust dirty count
	if ( (baseSlot+slotCount) > m_programParamsI[type].m_dirtySlotCount)
	{
		m_programParamsI[type].m_dirtySlotCount = baseSlot+slotCount;
	}
}


CGLMBuffer	*GLMContext::NewBuffer( EGLMBufferType type, uint size, uint options )
{
	//hushed GLM_FUNC;

	MakeCurrent();
	
	CGLMBuffer *prog = new CGLMBuffer( this, type, size, options );

	return prog;
}

void	GLMContext::DelBuffer( CGLMBuffer *buff )
{
	GLM_FUNC;

	this->MakeCurrent();

	for( int index=0; index < kGLMVertexAttributeIndexMax; index++ )
	{
		if (m_drawVertexSetup.m_attrs[index].m_buffer == buff)
		{
			// just clear the enable mask - this will force all the attrs to get re-sent on next sync
			m_drawVertexSetup.m_attrMask = 0;
		}
	}

	if (m_drawIndexBuffer == buff)
	{
		m_drawIndexBuffer = NULL;
	}
	
	if (m_lastKnownBufferBinds[ buff->m_type ] == buff)
	{
		// shoot it down
		this->BindBufferToCtx( buff->m_type, NULL );
		m_lastKnownBufferBinds[ buff->m_type ] = NULL;
	}
	
	delete buff;
}


void	GLMContext::SetIndexBuffer( CGLMBuffer *buff )
{
	GLM_FUNC;
	CheckCurrent();
	
	m_drawIndexBuffer = buff;

	// draw time is welcome to re-check, but we bind it immediately.
	this->BindBufferToCtx( kGLMIndexBuffer, buff );	
}

GLMVertexSetup g_blank_setup;

void	GLMContext::SetVertexAttributes( GLMVertexSetup *setup )
{
	GLM_FUNC;

	// we now just latch the vert setup and then execute on it at flushdrawstatestime if shaders are enabled.
	if (setup)
	{
		m_drawVertexSetup = *setup;
	}
	else
	{
		memset( &m_drawVertexSetup, 0, sizeof(m_drawVertexSetup) );
	}

	return;
}

void	GLMContext::Clear( bool color, unsigned long colorValue, bool depth, float depthValue, bool stencil, unsigned int stencilValue, GLScissorBox_t *box )
{
	GLM_FUNC;
	m_debugBatchIndex++;				// clears are batches too (maybe blits should be also...)

#if GLMDEBUG
	GLMDebugHookInfo info;
	memset( &info, 0, sizeof(info) );
	info.m_caller = eClear;
	
	do
	{
#endif
		uint mask = 0;

		GLClearColor_t clearcol;
		GLClearDepth_t cleardep = { depthValue };
		GLClearStencil_t clearsten = { stencilValue };

		// depth write mask must be saved&restored
		GLDepthMask_t			olddepthmask;
		GLDepthMask_t			newdepthmask = { true };

		// stencil write mask must be saved and restored
		GLStencilWriteMask_t			oldstenmask;
		GLStencilWriteMask_t			newstenmask = { 0xFFFFFFFF };
		
		GLColorMaskSingle_t		oldcolormask;
		GLColorMaskSingle_t		newcolormask = { -1,-1,-1,-1 };	// D3D clears do not honor color mask, so force it
		
		if (color)
		{
			// #define D3DCOLOR_ARGB(a,r,g,b) ((D3DCOLOR)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))

			clearcol.r =	((colorValue >> 16) & 0xFF) / 255.0f;	//R
			clearcol.g =	((colorValue >>  8) & 0xFF) / 255.0f;	//G
			clearcol.b =	((colorValue      ) & 0xFF) / 255.0f;	//B
			clearcol.a =	((colorValue >> 24) & 0xFF) / 255.0f;	//A

			m_ClearColor.Write( &clearcol, true, true );	// no check, no wait
			mask |= GL_COLOR_BUFFER_BIT;
			
			// save and set color mask
			m_ColorMaskSingle.Read( &oldcolormask, 0 );			
			m_ColorMaskSingle.Write( &newcolormask, true, true );			
		}

		if (depth)
		{
			// get old depth write mask
			m_DepthMask.Read( &olddepthmask, 0 );
			m_DepthMask.Write( &newdepthmask, true, true );
			m_ClearDepth.Write( &cleardep, true, true );	// no check, no wait
			mask |= GL_DEPTH_BUFFER_BIT;
		}

		if (stencil)
		{
			m_ClearStencil.Write( &clearsten, true, true );	// no check, no wait
			mask |= GL_STENCIL_BUFFER_BIT;

			// save and set sten mask
			m_StencilWriteMask.Read( &oldstenmask, 0 );			
			m_StencilWriteMask.Write( &newstenmask, true, true );			
		}

		bool subrect = (box != NULL);
		GLScissorEnable_t scissorEnableSave;
		GLScissorEnable_t scissorEnableNew = { true };
		
		GLScissorBox_t scissorBoxSave;
		GLScissorBox_t scissorBoxNew;
		
		if (subrect)
		{
			// save current scissorbox and enable
			m_ScissorEnable.Read( &scissorEnableSave, 0 );
			m_ScissorBox.Read( &scissorBoxSave, 0 );
			
			if(0)
			{
				// calc new scissorbox as intersection against *box

					// max of the mins
				scissorBoxNew.x = MAX(scissorBoxSave.x, box->x);
				scissorBoxNew.y = MAX(scissorBoxSave.y, box->y);
				
					// min of the maxes
				scissorBoxNew.width = ( MIN(scissorBoxSave.x+scissorBoxSave.width, box->x+box->width)) - scissorBoxNew.x;
				
					// height is just min of the max y's, minus the new base Y
				scissorBoxNew.height = ( MIN(scissorBoxSave.y+scissorBoxSave.height, box->y+box->height)) - scissorBoxNew.y;				
			}
			else
			{
				// ignore old scissor box completely.
				scissorBoxNew = *box;
			}
			// set new box and enable
			m_ScissorEnable.Write( &scissorEnableNew, true, true );
			m_ScissorBox.Write( &scissorBoxNew, true, true );
		}
		
		glClear( mask );

		if (subrect)
		{
			// put old scissor box and enable back
			m_ScissorEnable.Write( &scissorEnableSave, true, true );
			m_ScissorBox.Write( &scissorBoxSave, true, true );
		}
		
		if (depth)
		{
			// put old depth write mask
			m_DepthMask.Write( &olddepthmask );
		}
		
		if (color)
		{
			// put old color write mask
			m_ColorMaskSingle.Write( &oldcolormask, true, true );			
		}
		
		if (stencil)
		{
			// put old sten mask
			m_StencilWriteMask.Write( &oldstenmask, true, true );			
		}

#if GLMDEBUG
		this->DebugHook( &info );
	} while (info.m_loop);
#endif
}


// stolen from glmgrbasics.cpp
extern "C" uint GetCurrentKeyModifiers( void );
enum ECarbonModKeyIndex
{
  EcmdKeyBit                     = 8,    /* command key down?*/
  EshiftKeyBit                   = 9,    /* shift key down?*/
  EalphaLockBit                  = 10,   /* alpha lock down?*/
  EoptionKeyBit                  = 11,   /* option key down?*/
  EcontrolKeyBit                 = 12    /* control key down?*/
};

enum ECarbonModKeyMask
{
  EcmdKey                        = 1 << EcmdKeyBit,
  EshiftKey                      = 1 << EshiftKeyBit,
  EalphaLock                     = 1 << EalphaLockBit,
  EoptionKey                     = 1 << EoptionKeyBit,
  EcontrolKey                    = 1 << EcontrolKeyBit
};

static	ConVar gl_flushpaircache ("gl_flushpaircache", "0");
static	ConVar gl_paircachestats ("gl_paircachestats", "0");
static	ConVar gl_mtglflush_at_tof ("gl_mtglflush_at_tof", "0");
static	ConVar gl_texlayoutstats ("gl_texlayoutstats", "0" );

void	GLMContext::BeginFrame( void )
{
	GLM_FUNC;

	MakeCurrent();

	m_debugFrameIndex++;
	m_debugBatchIndex = -1;

	// check for lang change at TOF
	if (m_caps.m_hasDualShaders)
	{
		if (m_drawingLang != m_drawingLangAtFrameStart)
		{
			// language change.  unbind everything..
			this->NullProgram();

			m_drawingLang = m_drawingLangAtFrameStart;
		}
	}

	// scrub some critical shock absorbers
	for( int i=0; i< 16; i++)
	{
		glDisableVertexAttribArray( i );						// enable GLSL attribute- this is just client state - will be turned back off
		GLMCheckError();
	}
	m_lastKnownVertexAttribMask = 0;
	
	//FIXME should we also zap the m_lastKnownAttribs array ? (worst case it just sets them all again on first batch)

	BindBufferToCtx( kGLMVertexBuffer, NULL, true );
	BindBufferToCtx( kGLMIndexBuffer, NULL, true );

	if (gl_flushpaircache.GetInt())
	{
		// do the flush and then set back to zero
		this->ClearShaderPairCache();
		
		printf("\n\n##### shader pair cache cleared\n\n");
		gl_flushpaircache.SetValue( 0 );
	}
	
	if (gl_paircachestats.GetInt())
	{
		// do the flush and then set back to zero
		this->m_pairCache->DumpStats();
		
		gl_paircachestats.SetValue( 0 );
	}
	
	if (gl_texlayoutstats.GetInt())
	{
		this->m_texLayoutTable->DumpStats();
		
		gl_texlayoutstats.SetValue( 0 );
	}
	
	if (gl_mtglflush_at_tof.GetInt())
	{
		glFlush();									// TOF flush - skip this if benchmarking, enable it if human playing (smoothness)
	}
	
#if GLMDEBUG
	// init debug hook information
	GLMDebugHookInfo info;
	memset( &info, 0, sizeof(info) );
	info.m_caller = eBeginFrame;
	
	do
	{
		this->DebugHook( &info );
	} while (info.m_loop);

#endif

}

void	GLMContext::EndFrame( void )
{
	GLM_FUNC;

#if GLMDEBUG
	// init debug hook information
	GLMDebugHookInfo info;
	memset( &info, 0, sizeof(info) );
	info.m_caller = eEndFrame;
	
	do
	{
#endif
		if (!m_oneCtxEnable)	// if using dual contexts, this flush is needed
		{
			glFlush();
		}
#if GLMDEBUG
		this->DebugHook( &info );
	} while (info.m_loop);
#endif
}

//===============================================================================

CGLMQuery *GLMContext::NewQuery( GLMQueryParams *params )
{
	CGLMQuery *query = new CGLMQuery( this, params );
	
	return query;
}

void	GLMContext::DelQuery( CGLMQuery *query )
{
	// may want to do some finish/
	delete query;
}

static ConVar mat_vsync( "mat_vsync", "0", FCVAR_NONE, "Force sync to vertical retrace", true, 0.0, true, 1.0 );

//===============================================================================

ConVar glm_nullrefresh_capslock( "glm_nullrefresh_capslock", "0" );
ConVar glm_literefresh_capslock( "glm_literefresh_capslock", "0" );

extern ConVar gl_blitmode;

void	GLMContext::Present( CGLMTex *tex )
{
	GLM_FUNC;

	MakeCurrent();

	bool newRefreshMode = false;
	// two ways to go:
	
	// old school, do the resolve, had the tex down to cocoamgr to actually blit.
	// that way is required if you are not in one-context mode (10.5.8)
	
	if ( m_oneCtxEnable && (gl_blitmode.GetInt() != 0) )
	{
		newRefreshMode = true;
	}
	
	// this is the path whether full screen or windowed... we always blit.
	CShowPixelsParams showparams;
	memset( &showparams, 0, sizeof(showparams) );

	showparams.m_srcTexName	= tex->m_texName;
	showparams.m_width		= tex->m_layout->m_key.m_xSize;
	showparams.m_height		= tex->m_layout->m_key.m_ySize;
	showparams.m_vsyncEnable = m_displayParams.m_vsyncEnable = mat_vsync.GetBool();
	showparams.m_fsEnable	= m_displayParams.m_fsEnable;
	showparams.m_useBlit	= m_caps.m_hasFramebufferBlit;

	// we call showpixels once with the "only sync view" arg set, so we know what the latest surface size is, before trying to do our own blit !
	showparams.m_onlySyncView = true;
	g_extCocoaMgr->ShowPixels(&showparams);	// doesn't actually show anything, just syncs window/fs state (would make a useful separate call)
	showparams.m_onlySyncView = false;
	
	bool refresh = true;
	if ( (glm_nullrefresh_capslock.GetInt()) && (GetCurrentKeyModifiers() & EalphaLock) )
	{
		refresh = false;
	}
	
	static int counter;
	counter ++;

	if ( (glm_literefresh_capslock.GetInt()) && (GetCurrentKeyModifiers() & EalphaLock) && (counter & 127) )
	{
		// just show every 128th frame
		refresh = false;
	}
	
	if (refresh)
	{
		if (newRefreshMode)
		{
			// blit to GL_BACK done here, not in CocoaMgr, this lets us do resolve directly if conditions are right

			GLMRect	srcRect, dstRect;
			
			uint dstWidth,dstHeight;
			g_extCocoaMgr->DisplayedSize( dstWidth,dstHeight );

			srcRect.xmin	=	0;
			srcRect.ymin	=	0;
			srcRect.xmax	=	showparams.m_width;
			srcRect.ymax	=	showparams.m_height;

			dstRect.xmin	=	0;
			dstRect.ymin	=	0;
			dstRect.xmax	=	dstWidth;
			dstRect.ymax	=	dstHeight;
			
			// do not ask for LINEAR if blit is unscaled
			// NULL means targeting GL_BACK.  Blit2 will break it down into two steps if needed, and will handle resolve, scale, flip.
			bool blitScales	=	(showparams.m_width != dstWidth) || (showparams.m_height != dstHeight);
			this->Blit2(	tex, &srcRect, 0,0,
							NULL, &dstRect, 0,0,
							blitScales ? GL_LINEAR : GL_NEAREST );

			// we set showparams.m_noBlit, and just let CocoaMgr handle the swap (flushbuffer / page flip)
			showparams.m_noBlit = true;

			if (m_oneCtxEnable)			// if using single context, we need to blast some state so GLM will recover after the FBO fiddlin'
			{
				BindFBOToCtx( NULL, GL_READ_FRAMEBUFFER_EXT );
				GLMCheckError();						
				BindFBOToCtx( NULL, GL_DRAW_FRAMEBUFFER_EXT );
				GLMCheckError();						
			}
		}
		else
		{
			ResolveTex( tex, true );	// dxabstract used to do this unconditionally.we still do if new refresh mode doesn't engage.
			
			if (m_oneCtxEnable)			// if using single context, we need to blast some state so GLM will recover after the FBO fiddlin'
			{
				BindFBOToCtx( NULL, GL_READ_FRAMEBUFFER_EXT );
				GLMCheckError();						
				BindFBOToCtx( NULL, GL_DRAW_FRAMEBUFFER_EXT );
				GLMCheckError();						
			}
			else
			{
				glFlush();					// this call is needed for dual context mode to get pixels flushed
			}
			
			// showparams.m_noBlit is left set to 0.  CocoaMgr does the blit.
		}

		g_extCocoaMgr->ShowPixels(&showparams);
	}

	if (m_oneCtxEnable)
	{
		//	put the original FB back in place (both read and draw)
		// this bind will hit both read and draw bindings
		BindFBOToCtx( m_drawingFBO, GL_READ_FRAMEBUFFER_EXT );
		GLMCheckError();						
		BindFBOToCtx( m_drawingFBO, GL_DRAW_FRAMEBUFFER_EXT );
		GLMCheckError();						

		// put em back !!
		m_ScissorEnable.Flush( true );	
		m_ScissorBox.Flush( true );
		m_ViewportBox.Flush( true );		
	}
	else
	{
		MakeCurrent();	
	}
}



//===============================================================================
// GLMContext protected methods

// a naive implementation of this would just clear-drawable on the context at entry,
// and then capture and set fullscreen if requested.
// however that would glitch thescreen every time the user changed resolution while staying in full screen.
// but in windowed mode there's really not much to do in here.  Yeah, this routine centers around obtaining
// drawables for fullscreen mode, and/or dropping those drawables if we're going back to windowed.

// um, are we expected to re-make the standard surfaces (color, depthstencil) if the res changes?  is that now this routine's job ?

// so, kick it off with an assessment of whather we were FS previously or not.
// if there was no prior display params latched, then it wasn't.

// changes in here take place immediately.  If you want to defer display changes then that's going to be a different method.
// common assumption is that there will be two places that call this: context create and the implementation of the DX9 Reset method.
// in either case the client code is aware of what it signed up for.

bool	GLMContext::SetDisplayParams( GLMDisplayParams *params )
{	
	m_displayParams = *params;	// latch em
	m_displayParamsValid = true;
}

extern ConVar gl_singlecontext;	// single context mode go-ahead if 10.6.3 or higher
ConVar gl_can_query_fast("gl_can_query_fast", "0");

GLMContext::GLMContext( GLMDisplayParams *params )
{
	// flag our copy of display params as blank
	m_displayParamsValid = false;

	// peek at any CLI options
	m_slowAssertEnable = CommandLine()->FindParm("-glmassertslow");
	m_slowSpewEnable = CommandLine()->FindParm("-glmspewslow");
	m_slowCheckEnable = m_slowAssertEnable || m_slowSpewEnable;

	m_drawingLangAtFrameStart = m_drawingLang = kGLMGLSL;		// default to GLSL
	
	// this affects FlushDrawStates which will route program bindings, uniform delivery, sampler setup, and enables accordingly.

	if ( CommandLine()->FindParm("-glslmode") )
	{
		m_drawingLangAtFrameStart = m_drawingLang = kGLMGLSL;
	}
	if ( CommandLine()->FindParm("-arbmode") && !CommandLine()->FindParm("-glslcontrolflow") )
	{
		m_drawingLangAtFrameStart = m_drawingLang = kGLMARB;
	}

	// proceed with rest of init
	
	m_nsctx	= NULL;
	m_ctx	= NULL;
	
	// call Cocoa manager, ask for the attrib list (also naming the specific renderer ID) and use that to make our context
	CGLPixelFormatAttribute *selAttribs	=	NULL;
	uint					selWords	=	0;

	memset( &m_caps, 0, sizeof( m_caps ) );
	g_extCocoaMgr->GetDesiredPixelFormatAttribsAndRendererInfo( (uint**)&selAttribs, &selWords, &m_caps );
	uint selBytes = selWords * sizeof( uint ); 

	// call Cocoa manager, ask it about the window we're targeting, get the NSGLContext back, share against that	
	PseudoNSGLContextPtr shareNsCtx = g_extCocoaMgr->GetNSGLContextForWindow( (void*)params->m_focusWindow );


	// decide if we're going to try single context mode.
	m_oneCtxEnable = (m_caps.m_osComboVersion >= 0x000A0603) && (gl_singlecontext.GetInt() );

	bool success = false;	//NewNSGLContext( (unsigned long*)selAttribs, shareNsCtx, &m_nsctx, &m_ctx );
	if(m_oneCtxEnable)
	{
		// just steal the window's context
		m_nsctx	= shareNsCtx;
		m_ctx	= GetCGLContextFromNSGL( shareNsCtx );
		
		success	= (m_nsctx != NULL) && (m_ctx != NULL);
	}
	else
	{
		success = NewNSGLContext( (unsigned long*)selAttribs, shareNsCtx, &m_nsctx, &m_ctx );
	}
	
	if (success)
	{
		//write a cookie into the CGL context leading back to the GLM context object
		GLint	glm_context_link = (GLint) this;
		CGLSetParameter( m_ctx, kCGLCPClientStorage, &glm_context_link );
		
		// save off the pixel format attributes we used		
		memcpy(m_pixelFormatAttribs, selAttribs, selBytes );
	}
	else
	{
		Debugger(); //FIXME #PMB# bad news, maybe exit to shell if this happens
	}

	if (CommandLine()->FindParm("-glmspewcaps"))
	{
		DumpCaps();
	}
	
	SetDisplayParams( params );

	m_texLayoutTable = new CGLMTexLayoutTable;
	
	memset( m_samplers, 0, sizeof( m_samplers ) );
	m_activeTexture = -1;
	
	m_texLocks.EnsureCapacity( 16 );	// should be sufficient

	// FIXME need a texture tracking table so we can reliably delete CGLMTex objects at context teardown
		
	m_boundReadFBO = NULL;
	m_boundDrawFBO = NULL;
	m_drawingFBO = NULL;
											
	memset( m_boundProgram, 0, sizeof(m_boundProgram) );
	memset( m_drawingProgram, 0, sizeof(m_boundProgram) );
	memset( m_programParamsF , 0, sizeof (m_programParamsF) );
	memset( m_programParamsB , 0, sizeof (m_programParamsB) );
	memset( m_programParamsI , 0, sizeof (m_programParamsI) );

	m_paramWriteMode = eParamWriteDirtySlotRange;	// default to fastest mode
	
	if (CommandLine()->FindParm("-glmwriteallslots"))				m_paramWriteMode = eParamWriteAllSlots;
	if (CommandLine()->FindParm("-glmwriteshaderslots"))			m_paramWriteMode = eParamWriteShaderSlots;
	if (CommandLine()->FindParm("-glmwriteshaderslotsoptional"))	m_paramWriteMode = eParamWriteShaderSlotsOptional;
	if (CommandLine()->FindParm("-glmwritedirtyslotrange"))			m_paramWriteMode = eParamWriteDirtySlotRange;
	
	m_attribWriteMode = eAttribWriteDirty;

	if (CommandLine()->FindParm("-glmwriteallattribs"))				m_attribWriteMode = eAttribWriteAll;
	if (CommandLine()->FindParm("-glmwritedirtyattribs"))			m_attribWriteMode = eAttribWriteDirty;	

	m_pairCache	= new CGLMShaderPairCache( this );
	m_boundPair = NULL;
	m_boundPairRevision	= 0xFFFFFFFF;
	m_boundPairProgram	= (GLhandleARB)0xFFFFFFFF;			// GLSL only
	
	memset( m_lastKnownBufferBinds, 0, sizeof(m_lastKnownBufferBinds) );
	memset( m_lastKnownVertexAttribs, 0, sizeof(m_lastKnownVertexAttribs) );
	m_lastKnownVertexAttribMask = 0;

	// make a null program for use when client asks for NULL FP
	m_nullFragmentProgram = this->NewProgram(kGLMFragmentProgram, g_nullFragmentProgramText );

	// make dummy programs for doing texture preload via dummy draw
	m_preloadTexVertexProgram		=	this->NewProgram(kGLMVertexProgram, g_preloadTexVertexProgramText );
	m_preload2DTexFragmentProgram	=	this->NewProgram(kGLMFragmentProgram, g_preload2DTexFragmentProgramText );
	m_preload3DTexFragmentProgram	=	this->NewProgram(kGLMFragmentProgram, g_preload3DTexFragmentProgramText );
	m_preloadCubeTexFragmentProgram	=	this->NewProgram(kGLMFragmentProgram, g_preloadCubeTexFragmentProgramText );
	
	m_drawIndexBuffer			=	NULL;

	//memset( &m_drawVertexSetup, 0, sizeof(m_drawVertexSetup) );
	SetVertexAttributes( NULL );	// will set up all the entries in m_drawVertexSetup
	
	m_debugFontTex = NULL;

	// debug state
	m_debugFrameIndex = -1;
	m_debugBatchIndex = -1;

#if GLMDEBUG
	// #######################################################################################

	// DebugHook state - we could set these to more interesting values in response to a CLI arg like "startpaused" or something if desired
	//m_paused = false;
	m_holdFrameBegin = -1;
	m_holdFrameEnd = -1;
	m_holdBatch = m_holdBatchFrame = -1;
	
	m_debugDelayEnable = false;
	m_debugDelay = 1<<19;	// ~0.5 sec delay

	m_autoClearColor = m_autoClearDepth = m_autoClearStencil = false;
	m_autoClearColorValues[0] = 0.0;	//red
	m_autoClearColorValues[1] = 1.0;	//green
	m_autoClearColorValues[2] = 0.0;	//blue
	m_autoClearColorValues[3] = 1.0;	//alpha
	
	m_selKnobIndex = 0;
	m_selKnobMinValue = -10.0f;
	m_selKnobMaxValue = 10.0f;
	m_selKnobIncrement = 1/256.0f;

	// #######################################################################################
#endif

	// make two scratch FBO's for blit purposes
	m_blitReadFBO = this->NewFBO();
	m_blitDrawFBO = this->NewFBO();

	for( int i=0; i<kGLMScratchFBOCount; i++)
	{
		m_scratchFBO[i] = this->NewFBO();
	}
	
	bool new_mtgl = m_caps.m_hasPerfPackage1;	// i.e. 10.6.4 plus new driver
	
	if ( CommandLine()->FindParm("-glmenablemtgl2") )
	{
		new_mtgl = true;
	}

	if ( CommandLine()->FindParm("-glmdisablemtgl2") )
	{
		new_mtgl = false;
	}

	bool mtgl_on = params->m_mtgl;
	if (CommandLine()->FindParm("-glmenablemtgl"))
	{
		mtgl_on = true;
	}
	
	if (CommandLine()->FindParm("-glmdisablemtgl"))
	{
		mtgl_on = false;
	}

	CGLError result = (CGLError)0;
	if (mtgl_on)
	{
		bool ready = false;
		
		if (new_mtgl)
		{
			// afterburner
			CGLContextEnable kCGLCPGCDMPEngine = ((CGLContextEnable)1314);
			result = CGLEnable( m_ctx, kCGLCPGCDMPEngine );
			if (!result)
			{
				ready = true;	// succeeded - no need to try non-MTGL
				printf("\nMTGL detected.\n");
			}
			else
			{
				printf("\nMTGL *not* detected, falling back.\n");
			}
		}

		if (!ready)
		{
			// try old MTGL
			result = CGLEnable( m_ctx, kCGLCEMPEngine );
			if (!result)
			{
				printf("\nMTGL has been detected.\n");
				ready = true;	// succeeded - no need to try non-MTGL
			}
		}
	}
	
	// also, set the remote convar "gl_can_query_fast" to 1 if perf package present, else 0.
	gl_can_query_fast.SetValue( m_caps.m_hasPerfPackage1?1:0 );
	
	GLMCheckError();
}

GLMContext::~GLMContext	()
{
	// a lot of stuff that needs to be freed / destroyed

	if (m_debugFontTex)
	{
		this->DelTex( m_debugFontTex );
		m_debugFontTex = NULL;
	}

	if ( m_nullFragmentProgram )
	{
		this->DelProgram( m_nullFragmentProgram );
		m_nullFragmentProgram = NULL;
	}
	
	// walk m_fboTable and free them up..
	FOR_EACH_VEC( m_fboTable, i )
	{
		CGLMFBO *fbo = m_fboTable[i];
		this->DelFBO( fbo );
	}
	m_fboTable.SetSize( 0 );

	if (m_pairCache)
	{
		delete m_pairCache;
		m_pairCache = NULL;
	}
	
	// we need a m_texTable I think..

	// m_texLayoutTable can be scrubbed once we know that all the tex are freed

	if (m_nsctx && (!m_oneCtxEnable) )
	{
		DelNSGLContext( m_nsctx );
		m_nsctx	= NULL;
		m_ctx	= NULL;	
	}
}



void	GLMContext::SelectTMU( int tmu )
{
	//GLM_FUNC;

	CheckCurrent();
	if (tmu != m_activeTexture)
	{
		glActiveTexture( GL_TEXTURE0+tmu );
		GLMCheckError();

		m_activeTexture = tmu;
	}
}

int		GLMContext::BindTexToTMU( CGLMTex *tex, int tmu, bool noCheck )
{
	GLM_FUNC;
	GLMPRINTF(("--- GLMContext::BindTexToTMU tex %p GL name %d -> TMU %d ", tex, tex ? tex->m_texName : -1, tmu ));

	CheckCurrent();

	#if GLMDEBUG
		if ( tex && tex->m_debugLabel && (!strcmp( tex->m_debugLabel, "error" ) ) )
		{
			static char stop_here = 0;
			if (stop_here)
			{
				stop_here = 1;
			}
		}
	#endif
	
	if (tex && (tex->m_layout->m_key.m_texFlags & kGLMTexMultisampled) )
	{
		if (tex->m_rboDirty)
		{
			// the texture must be a multisampled render target which has been targeted recently for drawing.
			// check that it's not still attached...
			Assert( tex->m_rtAttachCount==0 );
			
			// let it resolve the MSAA RBO back to the texture
			ResolveTex( tex );
		}
	}
	
	SelectTMU( tmu );

	// if another texture was previously bound there, mark it not bound now
	// this should not be skipped
	
	if (m_samplers[tmu].m_boundTex)
	{
		m_samplers[tmu].m_boundTex->m_bindPoints[ tmu ] = false;

		// if new tex is not the same, then bind 0 for old tex's target
		//if (m_samplers[tmu].m_boundTex != tex)
		//{
		//	glBindTexture( m_samplers[tmu].m_boundTex->m_layout->m_key.m_texGLTarget, m_samplers[tmu].m_boundTex->m_texName );
		//}
		
		// note m_samplers[tmu].m_boundTex is now stale but we will step on it shortly
	} 

	// if texture chosen is different, or if noCheck is set, do the bind
	if (tex)
	{
		// bind new tex and mark it
		if ((tex != m_samplers[tmu].m_boundTex) || noCheck)
		{
			// if not being forced, we should see if the bind point (target) of the departing tex is different.
			if (!noCheck)
			{
				if ( (m_samplers[tmu].m_boundTex) )
				{
					// there is an outgoing tex.
					// same target?
					if ( m_samplers[tmu].m_boundTex->m_layout->m_key.m_texGLTarget != tex->m_layout->m_key.m_texGLTarget )
					{
						// no, different target.  inbound tex will be set below.  Here, just clear the different target of the outbound tex.
						glBindTexture( m_samplers[tmu].m_boundTex->m_layout->m_key.m_texGLTarget, 0 );
					}
					else
					{
						// same target, new tex, no work to do.
					}
				}
			}
			else
			{
				// mega scrub
				glBindTexture( GL_TEXTURE_1D, 0 );
				glBindTexture( GL_TEXTURE_2D, 0 );
				glBindTexture( GL_TEXTURE_3D, 0 );
				glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
			}

			glBindTexture( tex->m_layout->m_key.m_texGLTarget, tex->m_texName );
			GLMCheckError();
		}
		tex->m_bindPoints[ tmu ] = true;
		m_samplers[tmu].m_boundTex = tex;	
	}
	else
	{
		// this is an unbind request, bind name 0
		if (m_samplers[tmu].m_boundTex)
		{
			// no inbound tex.  Just clear the one target that the old tex occupied.
			glBindTexture( m_samplers[tmu].m_boundTex->m_layout->m_key.m_texGLTarget, 0 );
			GLMCheckError();
		}
		else
		{
			// none was bound before, so no action
		}
		m_samplers[tmu].m_boundTex = NULL;	
	}
}

void	GLMContext::BindFBOToCtx( CGLMFBO *fbo, GLenum bindPoint )
{
	GLM_FUNC;
	GLMPRINTF(( "--- GLMContext::BindFBOToCtx fbo %p, GL name %d", fbo, (fbo) ? fbo->m_name : -1 ));

	CheckCurrent();
	
	bool	targetRead = (bindPoint==GL_READ_FRAMEBUFFER_EXT) || (bindPoint==GL_FRAMEBUFFER_EXT);
	bool	targetDraw = (bindPoint==GL_DRAW_FRAMEBUFFER_EXT) || (bindPoint==GL_FRAMEBUFFER_EXT);
	
	if (targetRead)
	{
		if (fbo)	// you can pass NULL to go back to no-FBO
		{
			glBindFramebufferEXT( GL_READ_FRAMEBUFFER_EXT, fbo->m_name );
			GLMCheckError();
			
			m_boundReadFBO = fbo;
			//dontcare fbo->m_bound = true;
		}
		else
		{
			glBindFramebufferEXT( GL_READ_FRAMEBUFFER_EXT, 0 );
			GLMCheckError();
			
			m_boundReadFBO = NULL;
		}
	}
	
	if (targetDraw)
	{
		if (fbo)	// you can pass NULL to go back to no-FBO
		{
			glBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, fbo->m_name );
			GLMCheckError();
			
			m_boundDrawFBO = fbo;
			//dontcare fbo->m_bound = true;
		}
		else
		{
			glBindFramebufferEXT( GL_DRAW_FRAMEBUFFER_EXT, 0 );
			GLMCheckError();
			
			m_boundDrawFBO = NULL;
		}
	}
}

void	GLMContext::BindBufferToCtx( EGLMBufferType type, CGLMBuffer *buff, bool force )
{
	GLM_FUNC;
	GLMPRINTF(( "--- GLMContext::BindBufferToCtx buff %p, GL name %d", buff, (buff) ? buff->m_name : -1 ));

	CheckCurrent();

	if (!force)
	{
		// compare desired bind to last known bind, and see if we can bail
		if (m_lastKnownBufferBinds[ type ] == buff)
		{
			return;
		}
	}
	
	GLenum	target=0;
	switch( type )
	{	
		case	kGLMVertexBuffer:		target = GL_ARRAY_BUFFER_ARB;			break;
		case	kGLMIndexBuffer:		target = GL_ELEMENT_ARRAY_BUFFER_ARB;	break;
		case	kGLMUniformBuffer:		target = GL_UNIFORM_BUFFER_EXT;			break;
		case	kGLMPixelBuffer:		target = GL_PIXEL_UNPACK_BUFFER_ARB;	break;

		default:	Assert(!"Unknown buffer type" );
	}
	
	bool wasBound = false;
	bool isBound = false;
	
	if (m_lastKnownBufferBinds[type])
	{
		m_lastKnownBufferBinds[type]->m_bound = false;
		m_lastKnownBufferBinds[type] = NULL;
		wasBound = true;
	}
	
	if (buff)
	{
		if (buff->m_buffGLTarget != target)
			Debugger();
			
		glBindBufferARB( buff->m_buffGLTarget, buff->m_name );
		GLMCheckError();

		m_lastKnownBufferBinds[ type ] = buff;
		buff->m_bound = true;
		
		isBound = true;
	}
	else
	{
		// isBound stays false
		// bind name 0
		// note that no buffer is bound in the ctx state
		
		glBindBufferARB( target, 0 );
		GLMCheckError();
		
		m_lastKnownBufferBinds[ type ] = NULL;
	}	
}

ConVar	gl_can_mix_shader_gammas( "gl_can_mix_shader_gammas", 0 );
ConVar	gl_cannot_mix_shader_gammas( "gl_cannot_mix_shader_gammas", 0 );

void	GLMContext::FlushDrawStates( bool shadersOn )	// shadersOn = true for draw calls, false for clear calls
{
	GLM_FUNC;
	
	CheckCurrent();

	// FBO
	if ( (m_drawingFBO != m_boundDrawFBO) ||  (m_drawingFBO != m_boundReadFBO) )
	{
		//GLMPRINTF(("\nGLMContext::FlushDrawStates, setting FBO to %8x(gl %d), was %8x(gl %d)", m_drawingFBO, (m_drawingFBO? m_drawingFBO->m_name: -1),m_boundFBO, (m_boundFBO ? m_boundFBO->m_name : -1) ));
		this->BindFBOToCtx( m_drawingFBO, GL_READ_FRAMEBUFFER_EXT );
		this->BindFBOToCtx( m_drawingFBO, GL_DRAW_FRAMEBUFFER_EXT );
	}
	
	// if drawing FBO has any MSAA attachments, mark them dirty
	{
		CGLMTex *tex;
		for( int att=kAttColor0; att<kAttCount; att++)
		{
			if (m_drawingFBO->m_attach[ att ].m_tex)
			{
				CGLMTex *tex = m_drawingFBO->m_attach[ att ].m_tex;
				
				if (tex->m_rboName)		// is it MSAA
				{
					// mark it dirty
					tex->m_rboDirty++;
				}
			}
		}
	}
	
	// renderstates
	this->FlushStates();	// latched renderstates..

	// if there is no color target - bail out 
	// OK, this doesn't work in general - you can't leave the color target floating(null) or you will get FBO errors
	//if (!m_boundDrawFBO[0].m_attach[0].m_tex)
	//{
	//	GLMPRINTF(("-D- GLMContext::FlushDrawStates -> no color target! exiting.. " ));
	//	return;
	//}

	bool tex0_srgb = (m_boundDrawFBO[0].m_attach[0].m_tex->m_layout->m_key.m_texFlags & kGLMTexSRGB) != 0;

	// you can only actually use the sRGB FB state on some systems.. check caps
	if (m_caps.m_hasGammaWrites)
	{
		GLBlendEnableSRGB_t	writeSRGBState;
		m_BlendEnableSRGB.Read( &writeSRGBState, 0 );	// the client set value, not the API-written value yet..
		bool draw_srgb = writeSRGBState.enable;
		
		if (draw_srgb)
		{
			if (tex0_srgb)
			{
				// good - draw mode and color tex agree
			}
			else
			{
				// bad

				// Client has asked to write sRGB into a texture that can't do it.
				// there is no way to satisfy this unless we change the RT tex and we avoid doing that.
				// (although we might consider a ** ONE TIME ** promotion.
				// this shouldn't be a big deal if the tex format is one where it doesn't matter like 32F.

				GLMPRINTF(("-Z- srgb-enabled FBO conflict: attached tex %08x [%s] is not SRGB", m_boundDrawFBO[0].m_attach[0].m_tex, m_boundDrawFBO[0].m_attach[0].m_tex->m_layout->m_layoutSummary ));
				
				// do we shoot down the srgb-write state for this batch?
				// I think the runtime will just ignore it.
			}
		}
		else
		{
			if (tex0_srgb)
			{
				// odd - client is not writing sRGB into a texture which *can* do it.
				//GLMPRINTF(( "-Z- srgb-disabled FBO conflict: attached tex %08x [%s] is SRGB", m_boundFBO[0].m_attach[0].m_tex, m_boundFBO[0].m_attach[0].m_tex->m_layout->m_layoutSummary ));
				//writeSRGBState.enable = true;
				//m_BlendEnableSRGB.Write( &writeSRGBState );
			}
			else
			{
				// good - draw mode and color tex agree
			}
		}
		
		// now go ahead and flush the SRGB write state for real
		// set the noDefer on it too
		m_BlendEnableSRGB.Flush( /*true*/ );
	}
	// else... FlushDrawStates will work it out via flSRGBWrite in the fragment shader..

	// textures and sampling
	// note we generate a mask of which samplers are running "decode sRGB" mode, to help out the shader pair cache mechanism below.
	uint	srgbMask = 0;
	for( int i=0; i<GLM_SAMPLER_COUNT; i++)
	{
		GLMTexSampler *samp = &m_samplers[i];
		
		// push tex binding?
		if (samp->m_boundTex != samp->m_drawTex)
		{
			this->BindTexToTMU( samp->m_drawTex, i );
			samp->m_boundTex = samp->m_drawTex;
		}
		
		// push sampling params? it will check each one individually.
		if (samp->m_boundTex)
		{
			samp->m_boundTex->ApplySamplingParams( &samp->m_samp );
		}
		
		if (samp->m_samp.m_srgb)
		{
			srgbMask |= (1<<i);
		}
	}

	// index buffer
	if (m_drawIndexBuffer != m_lastKnownBufferBinds[ kGLMIndexBuffer ] )
	{
		BindBufferToCtx( kGLMIndexBuffer, m_drawIndexBuffer );		// note this could be a pseudo buffer..
	}

	// shader setup
	if (shadersOn)
	{
		switch( m_drawingLang )
		{
			case kGLMARB:
			{
				// disable any GLSL program
				glUseProgram( 0 );
				m_boundPair = NULL;
				
				// bind selected drawing programs in ARB flavor.
				// asking for "null" fragment shader is allowed, we offer up the dummy frag shader in response.

				// vertex side
				bool vpgood = false;
				bool fpgood = false;

				{
					CGLMProgram *vp = m_drawingProgram[ kGLMVertexProgram ];
					if (vp)
					{
						if (vp->m_descs[ kGLMARB ].m_valid)
						{
							glSetEnable( GL_VERTEX_PROGRAM_ARB, true );
							glBindProgramARB(GL_VERTEX_PROGRAM_ARB, vp->m_descs[ kGLMARB ].m_object.arb);
							GLMCheckError();
							
							m_boundProgram[ kGLMVertexProgram ] = vp;
							vpgood = true;
						}
						else
						{
							//Assert( !"Trying to draw with invalid ARB vertex program" );
						}
					}
					else
					{
						//Assert( !"Trying to draw with NULL ARB vertex program" );
					}
				}

				// fragment side
				{
					CGLMProgram *fp = m_drawingProgram[ kGLMFragmentProgram ];
					if (fp)
					{
						if (fp->m_descs[ kGLMARB ].m_valid)
						{
							glSetEnable( GL_FRAGMENT_PROGRAM_ARB, true );
							glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fp->m_descs[ kGLMARB ].m_object.arb);
							GLMCheckError();
							
							m_boundProgram[ kGLMFragmentProgram ] = fp;
							fpgood = true;
						}
						else
						{
							//Assert( !"Trying to draw with invalid ARB fragment program" );
							m_boundProgram[ kGLMFragmentProgram ] = NULL;
						}
					}
					else
					{
						// this is actually OK, we substitute a dummy shader
						glSetEnable( GL_FRAGMENT_PROGRAM_ARB, true );
						glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_nullFragmentProgram->m_descs[kGLMARB].m_object.arb );
						m_boundProgram[ kGLMFragmentProgram ] = m_nullFragmentProgram;
						fpgood = true;
					}
				}

				if (fpgood & vpgood)
				{
					// flush parameter values to both stages
					// FIXME: this can be optimized by dirty range, since ARB supports single-parameter-bank aka .env
					// FIXME: magic numbers, yuk
					
					glProgramEnvParameters4fvEXT( GL_VERTEX_PROGRAM_ARB, 0, 256, (const GLfloat*)&m_programParamsF[kGLMVertexProgram].m_values[0][0] );
					GLMCheckError();

					glProgramEnvParameters4fvEXT( GL_FRAGMENT_PROGRAM_ARB, 0, 32, (const GLfloat*)&m_programParamsF[kGLMFragmentProgram].m_values[0][0] );
					GLMCheckError();
				}
				else
				{
					// silence all (clears wind up here for example)

					glBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0 );
					glSetEnable( GL_VERTEX_PROGRAM_ARB, false );
					m_boundProgram[ kGLMVertexProgram ] = NULL;

					glSetEnable( GL_FRAGMENT_PROGRAM_ARB, false );
					glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0 );
					m_boundProgram[ kGLMFragmentProgram ] = NULL;
				}
///////////////////////////////////

				// ARB vert setup. maybe generalize this to handle both ARB and GLSL after we see what GLSL attrib setup looks like.

				//http://www.opengl.org/sdk/docs/man/xhtml/glVertexAttribPointer.xml
				//http://www.opengl.org/sdk/docs/man/xhtml/glEnableVertexAttribArray.xml	
				
				//	for (each attrib)
				//		if (enable unchanged and off) -> do nothing
				//		if (enable changed to off) -> disable that array ... set the attrib pointer to nil for clarity
				//		if (enable changed to on) -> bind the appropriate vertex buffer, set that attrib, log it
				//		if (enable unchanged and on) -> diff the attrib setup, re-bind if needed, log it
				
				GLMVertexSetup	*setup = &m_drawVertexSetup;
				uint	relevantMask =	setup->m_attrMask;
				
				for( int index=0; index < kGLMVertexAttributeIndexMax; index++ )
				{
					uint mask = 1<<index;
					if (relevantMask & mask)
					{
						GLMVertexAttributeDesc *setdesc = &setup->m_attrs[index];	// ptr to desired setup
						CGLMBuffer * buf = setdesc->m_buffer;						// bind buffer
						Assert( buf );
						
						BindBufferToCtx( kGLMVertexBuffer, buf );
						
						glEnableVertexAttribArray( index );							// enable attribute, set pointer.
						GLMCheckError();

						glVertexAttribPointer( index, setdesc->m_datasize, setdesc->m_datatype, setdesc->m_normalized, setdesc->m_stride, (	const GLvoid *)setdesc->m_offset );
						GLMCheckError();
						//GLMPRINTF(("--- GLMContext::SetVertexAttributes attr %d set to offset/stride %d/%d in buffer %d (normalized=%s)", index, setdesc->m_offset, setdesc->m_stride, setdesc->m_buffer->m_name, setdesc->m_normalized?"true":"false" ));
					}
					else
					{
						// disable attribute
						glDisableVertexAttribArray( index );
						GLMCheckError();
						//GLMPRINTF((" -- GLMContext::SetVertexAttributes attr %d is disabled", index ));

						// tidy up in case there was garbage? necessary ?
						memset ( &setup->m_attrs[index], 0, sizeof(setup->m_attrs[index]) );
					}
				}

///////////////////////////////////
			}
			break;

			case kGLMGLSL:
			{
				// early out if one of the stages is not set.
				// draw code needs to watch for this too.
				if ( (m_drawingProgram[ kGLMVertexProgram ]==NULL) || (m_drawingProgram[ kGLMFragmentProgram ]==NULL) )
				{
					this->NullProgram();
					return;
				}
				
				// examine selected drawing programs for both stages				
				// try to find a match in thelinked-pair-cache				
				// if no match, link one
					// examine metadata
					// get uniform locations for parameters, attributes, and samplers
					// put in cache
					
				// dispatch vertex attribute locations to shader (could be one-time)				
				// dispatch parameter values to both stages (could be optimized with UBO)
				// dispatch sampler locations to shader (need sampler metadata)

				// new way - use the pair cache

				// cook up some extra bits so that we can track different srgb-usages of the same vp/fp pair.
				// note that this is only important on some hardware/OS combos.
				// let the pair cache decide if it needs to honor the extra key bits or not.
				

				// decide if we need to mix extra bits into the lookup key.
				bool useExtraKeyBits = m_caps.m_costlyGammaFlips;
				
				// the "can" variable is allowed to override the static assessment.
				if ( gl_can_mix_shader_gammas.GetInt() )
				{
					useExtraKeyBits = false;
				}
				 
				// the "cannot" variable is allowed to override the first two
				if ( gl_cannot_mix_shader_gammas.GetInt() )
				{
					useExtraKeyBits = true;
				}
				 
				uint extraKeyBits = 0;
				
				if (useExtraKeyBits)
				{
					extraKeyBits = (srgbMask & m_drawingProgram[ kGLMFragmentProgram ]->m_samplerMask);
				}
				
				CGLMShaderPair	*newPair		= m_pairCache->SelectShaderPair( m_drawingProgram[ kGLMVertexProgram ], m_drawingProgram[ kGLMFragmentProgram ], extraKeyBits );
				GLhandleARB		newPairProgram	= newPair->m_program;
				uint			newPairRevision	= newPair->m_revision;
				
				// you cannot only key on the pair address, since pairs get evicted and pair records likely get recycled.
				// so key on all three - pair address, program name, revision number
				// this will also catch cases where a pair is re-linked (batch debugger / live edit)
				
				if ( (newPair != m_boundPair) || (newPairProgram != m_boundPairProgram) || (newPairRevision != m_boundPairRevision) )
				{
					m_boundPair = newPair;
					m_boundPairProgram = newPairProgram;
					m_boundPairRevision = newPairRevision;

					glUseProgram( (GLuint)newPairProgram );
					GLMCheckError();
					
					// set the dirty levels appropriately since the program changed and has never seen any of the current values.
					m_programParamsF[kGLMVertexProgram].m_dirtySlotCount = m_drawingProgram[ kGLMVertexProgram ]->m_descs[kGLMGLSL].m_highWater+1;
					m_programParamsF[kGLMFragmentProgram].m_dirtySlotCount = m_drawingProgram[ kGLMFragmentProgram ]->m_descs[kGLMGLSL].m_highWater+1;

					// bool and int dirty levels get set to max, we don't have actual high water marks for them
					// code which sends the values must clamp on these types.
					m_programParamsB[kGLMVertexProgram].m_dirtySlotCount = kGLMProgramParamBoolLimit;
					m_programParamsB[kGLMFragmentProgram].m_dirtySlotCount = 0;
					
					m_programParamsI[kGLMVertexProgram].m_dirtySlotCount = kGLMProgramParamInt4Limit;
					m_programParamsI[kGLMFragmentProgram].m_dirtySlotCount = 0;
				}
				
				// note the binding (not really bound.. just sitting in the linked active GLSL program)
				m_boundProgram[ kGLMVertexProgram ] = m_drawingProgram[ kGLMVertexProgram ];
				m_boundProgram[ kGLMFragmentProgram ] = m_drawingProgram[ kGLMFragmentProgram ];
				
				// now pave the way for drawing
				
				// parameters - find and set

				// vertex stage --------------------------------------------------------------------
				// find "vc" in VS
				GLint vconstLoc = m_boundPair->m_locVertexParams;
				if (vconstLoc >=0)
				{
					#if GLMDEBUG
						static uint paramsPushed=0,paramsSkipped=0,callsPushed=0;	// things that happened on pushed param trips
						static uint callsSkipped=0,paramsSkippedByCallSkip=0;		// on unpushed param trips (zero dirty)
					#endif
					
					int slotCountToPush	= 0;
					int shaderSlots		= m_boundPair->m_vertexProg->m_descs[kGLMGLSL].m_highWater+1;
					int dirtySlots		= m_programParamsF[kGLMVertexProgram].m_dirtySlotCount;
					
					
					switch( m_paramWriteMode )
					{
						case	eParamWriteAllSlots:		slotCountToPush = kGLMVertexProgramParamFloat4Limit;			break;
						case	eParamWriteShaderSlots:		slotCountToPush = shaderSlots;									break;
						
						case	eParamWriteShaderSlotsOptional:
						{
							slotCountToPush = shaderSlots;
							
							// ...unless, we're actually unchanged since last draw
							if (dirtySlots == 0)
							{
								// write none
								slotCountToPush = 0;
							}
						}
						break;
						
						case	eParamWriteDirtySlotRange:	slotCountToPush = dirtySlots;									break;						
					}
					
					if (slotCountToPush)
					{
						glUniform4fv( vconstLoc, slotCountToPush, &m_programParamsF[kGLMVertexProgram].m_values[0][0] );
						GLMCheckError();

						#if GLMDEBUG
							paramsPushed	+=	slotCountToPush;
							paramsSkipped	+=	shaderSlots - slotCountToPush;
	
							callsPushed++;
						#endif
					}
					else
					{
						#if GLMDEBUG
							paramsSkippedByCallSkip += shaderSlots;
							
							callsSkipped++;
						#endif
					}

					#if GLMDEBUG && 0
						if (GLMKnob("caps-key",NULL) > 0.0)
						{
							// spew
							GLMPRINTF(("VP callsPushed=%d  ( paramsPushed=%d   paramsSkipped=%d )      callsSkipped=%d (paramsSkippedByCallSkip=%d)",
								callsPushed, paramsPushed, paramsSkipped, callsSkipped, paramsSkippedByCallSkip
							));
						}
					#endif
					
					m_programParamsF[kGLMVertexProgram].m_dirtySlotCount = 0;	//ack
				}

				// see if VS uses i0, b0, b1, b2, b3.
				// use a glUniform1i to set any one of these if active.  skip all of them if no dirties reported.
				// my kingdom for the UBO extension!
				
					// ------- bools ---------- //
				if ( 1 /*m_programParamsB[kGLMVertexProgram].m_dirtySlotCount*/ )	// optimize this later after the float param pushes are proven out
				{
					GLint vconstBool0Loc = m_boundPair->m_locVertexBool0;									//glGetUniformLocationARB( prog, "b0");
					if ( vconstBool0Loc >= 0 )
					{
						glUniform1i( vconstBool0Loc, m_programParamsB[kGLMVertexProgram].m_values[0] );	//FIXME magic number
						GLMCheckError();
					}

					GLint vconstBool1Loc = m_boundPair->m_locVertexBool1;									//glGetUniformLocationARB( prog, "b1");
					if ( vconstBool1Loc >= 0 )
					{
						glUniform1i( vconstBool1Loc, m_programParamsB[kGLMVertexProgram].m_values[1] );	//FIXME magic number
						GLMCheckError();
					}

					GLint vconstBool2Loc = m_boundPair->m_locVertexBool2;									//glGetUniformLocationARB( prog, "b2");
					if ( vconstBool2Loc >= 0 )
					{
						glUniform1i( vconstBool2Loc, m_programParamsB[kGLMVertexProgram].m_values[2] );	//FIXME magic number
						GLMCheckError();
					}

					GLint vconstBool3Loc = m_boundPair->m_locVertexBool3;									//glGetUniformLocationARB( prog, "b3");
					if ( vconstBool3Loc >= 0 )
					{
						glUniform1i( vconstBool3Loc, m_programParamsB[kGLMVertexProgram].m_values[3] );	//FIXME magic number
						GLMCheckError();
					}
					m_programParamsB[kGLMVertexProgram].m_dirtySlotCount = 0;	//ack
				}

					// ------- int ---------- //
				if ( 1 /*m_programParamsI[kGLMVertexProgram].m_dirtySlotCount*/ )	// optimize this later after the float param pushes are proven out
				{
					GLint vconstInt0Loc = m_boundPair->m_locVertexInteger0;									//glGetUniformLocationARB( prog, "i0");
					if ( vconstInt0Loc >= 0 )
					{
						glUniform1i( vconstInt0Loc, m_programParamsI[kGLMVertexProgram].m_values[0][0] );	//FIXME magic number
						GLMCheckError();
					}
					m_programParamsI[kGLMVertexProgram].m_dirtySlotCount = 0;	//ack
				}


				// attribs - find and set
				// GLSL vert setup - clone/edit of ARB setup.  try to re-unify these later.

				GLMVertexSetup	*setup = &m_drawVertexSetup;
				uint	relevantMask =	setup->m_attrMask;
				
				//static char *attribnames[] = { "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15" };
				
				CGLMBuffer *loopCurrentBuf = NULL;		// local shock absorber for this loop
				for( int index=0; index < kGLMVertexAttributeIndexMax; index++ )
				{
					uint mask = 1<<index;
					if (relevantMask & mask)
					{
						GLMVertexAttributeDesc *newDesc = &setup->m_attrs[index];	// ptr to desired setup

						bool writeAttrib = false;
						
						switch(m_attribWriteMode)
						{
							case eAttribWriteAll:
								writeAttrib = true;
							break;
							
							case eAttribWriteDirty:
								static uint hits=0,misses=0;
								// first see if we have to do anything at all.
								// the equality operator checks buffer name, offset, stride, datatype and normalized.
								// we check buffer revision separately, submitter of vertex setup is not expected to provide it (zero is preferred).
								// consult the actual buffer directly.
								
								// note also, we're only doing thi compare when attrib #index is active for this batch.
								// previously-active attribs which are becoming disabled need not be checked..
						
								GLMVertexAttributeDesc *lastDesc = &m_lastKnownVertexAttribs[index];
								if ( (!(*newDesc == *lastDesc)) || (newDesc->m_buffer->m_revision != lastDesc->m_bufferRevision) )
								{
									*lastDesc = *newDesc;										// latch new setup
									lastDesc->m_bufferRevision = newDesc->m_buffer->m_revision;	// including proper revision of the sourcing buffer

									writeAttrib = true;
									misses++;
								}
								else
								{
									hits++;
								}
								
								#if 0
								if ( ((hits+misses) % 10000)==0)
								{
									printf("\n** attrib setup  hits %d  misses %d",hits,misses);
								}
								#endif
							break;
						}
						
						if( writeAttrib )
						{
							CGLMBuffer * buf = newDesc->m_buffer;						// bind buffer
							Assert( buf );
							
							if (buf != loopCurrentBuf)
							{
								BindBufferToCtx( kGLMVertexBuffer, buf );				// (if not already on the bind point of interest)
								GLMCheckError();
								
								loopCurrentBuf = buf;
							}

							glVertexAttribPointer( index, newDesc->m_datasize, newDesc->m_datatype, newDesc->m_normalized, newDesc->m_stride, (	const GLvoid *)newDesc->m_offset );
							GLMCheckError();
						}
						
						// enable is checked separately from the attrib binding
						if (! (m_lastKnownVertexAttribMask & (1<<index)) )
						{
							glEnableVertexAttribArray( index );						// enable GLSL attribute- this is just client state - will be turned back off
							GLMCheckError();
							
							m_lastKnownVertexAttribMask |= (1<<index);
						}
					}
					else
					{
						// this shader doesnt use that pair.
						if ( m_lastKnownVertexAttribMask & (1<<index) )
						{
							glDisableVertexAttribArray( index );						// enable GLSL attribute- this is just client state - will be turned back off
							GLMCheckError();
							
							m_lastKnownVertexAttribMask &= ~(1<<index);
						}
					}
				}
				
				// fragment  stage --------------------------------------------------------------------

				// find "pc" in FS ("pixel constants")
				GLint fconstLoc = m_boundPair->m_locFragmentParams;
				if (fconstLoc >=0)
				{
					#if GLMDEBUG
						static uint paramsPushed=0,paramsSkipped=0,callsPushed=0;	// things that happened on pushed param trips
						static uint callsSkipped=0,paramsSkippedByCallSkip=0;		// on unpushed param trips (zero dirty)
					#endif
					
					int slotCountToPush	= 0;
					int shaderSlots		= m_boundPair->m_fragmentProg->m_descs[kGLMGLSL].m_highWater+1;
					int dirtySlots		= m_programParamsF[kGLMFragmentProgram].m_dirtySlotCount;
					
					switch( m_paramWriteMode )
					{
						case	eParamWriteAllSlots:		slotCountToPush = kGLMFragmentProgramParamFloat4Limit;			break;
						case	eParamWriteShaderSlots:		slotCountToPush = shaderSlots;									break;
						
						case	eParamWriteShaderSlotsOptional:
						{
							slotCountToPush = shaderSlots;
							
							// ...unless, we're actually unchanged since last draw
							if (dirtySlots == 0)
							{
								// write none
								slotCountToPush = 0;
							}
						}
						break;
						
						case	eParamWriteDirtySlotRange:	slotCountToPush = dirtySlots;									break;						
					}
					
					if (slotCountToPush)
					{
						glUniform4fv( fconstLoc, slotCountToPush, &m_programParamsF[kGLMFragmentProgram].m_values[0][0] );
						GLMCheckError();

						#if GLMDEBUG
							paramsPushed	+=	slotCountToPush;
							paramsSkipped	+=	shaderSlots - slotCountToPush;

							callsPushed++;
						#endif
					}
					else
					{
						#if GLMDEBUG
							paramsSkippedByCallSkip += shaderSlots;
						
							callsSkipped++;
						#endif
					}

					#if GLMDEBUG && 0
						if ( 0 && (GLMKnob("caps-key",NULL) > 0.0) )	// turn on as needed
						{
							// spew
							GLMPRINTF(("FP callsPushed=%d  ( paramsPushed=%d   paramsSkipped=%d )      callsSkipped=%d (paramsSkippedByCallSkip=%d)",
								callsPushed, paramsPushed, paramsSkipped, callsSkipped, paramsSkippedByCallSkip
							));
						}
					#endif

					m_programParamsF[kGLMFragmentProgram].m_dirtySlotCount = 0;	//ack
				}
				
				// fake SRGB
				if (!m_caps.m_hasGammaWrites)														// do we need to think about fake SRGB?
				{
					if (m_boundPair->m_locFragmentFakeSRGBEnable >= 0)								// does the shader have that uniform handy?
					{
						float desiredValue = m_FakeBlendEnableSRGB ? 1.0 : 0.0;						// what should it be set to?
						
						if (desiredValue != m_boundPair->m_fakeSRGBEnableValue)						// and is that different from what it is known to be set to ?
						{
							glUniform1f( m_boundPair->m_locFragmentFakeSRGBEnable, desiredValue );	// if so, write it
							GLMCheckError();
							
							m_boundPair->m_fakeSRGBEnableValue = desiredValue;						// and recall that we did so
						}
					}
				}
				
				//samplers
				if (m_boundPair)
				{
					if(!m_boundPair->m_samplersFixed)
					{
						for( int sampler=0; sampler<16; sampler++)
						{
							if (m_boundPair->m_locSamplers[sampler] >=0)
							{
								glUniform1iARB( m_boundPair->m_locSamplers[sampler], sampler );
								GLMCheckError();
							}
						}
						m_boundPair->m_samplersFixed = true;
					}
				}
			}
			break;
		}
	}
	else
	{
		this->NullProgram();
	}
}


#if GLMDEBUG

enum EGLMDebugDumpOptions
{
	eDumpBatchInfo,
	eDumpSurfaceInfo,
	eDumpStackCrawl,
	eDumpShaderLinks,
//	eDumpShaderText,			// we never use this one
	eDumpShaderParameters,
	eDumpTextureSetup,
	eDumpVertexAttribSetup,
	eDumpVertexData,
	eOpenShadersForEdit
};

enum EGLMVertDumpMode
{
	// options that affect eDumpVertexData above
	eDumpVertsNoTransformDump,
	eDumpVertsTransformedByViewProj,
	eDumpVertsTransformedByModelViewProj,
	eDumpVertsTransformedByBoneZeroThenViewProj,
	eDumpVertsTransformedByBonesThenViewProj,
	eLastDumpVertsMode
};

char *g_vertDumpModeNames[] = 
{
	"noTransformDump",
	"transformedByViewProj",
	"transformedByModelViewProj",
	"transformedByBoneZeroThenViewProj",
	"transformedByBonesThenViewProj"
};

static void CopyTilEOL( char *dst, char *src, int dstSize )
{
	dstSize--;
	
	int i=0;
	while ( (i<dstSize) && (src[i] != 0) && (src[i] != '\n') && (src[i] != '\r') )
	{
		dst[i] = src[i];
		i++;
	}
	dst[i] = 0;
}

static uint g_maxVertsToDumpLog2 = 4;
static uint g_maxFramesToCrawl = 20;			// usually enough.  Not enough? change it..

extern char sg_pPIXName[128];

// min is eDumpVertsNormal, max is the one before eLastDumpVertsMode
static enum EGLMVertDumpMode g_vertDumpMode = eDumpVertsNoTransformDump;

void	GLMContext::DebugDump( GLMDebugHookInfo *info, uint options, uint vertDumpMode )
{
	int oldIndent = GLMGetIndent();
	GLMSetIndent(0);
	
	CGLMProgram *vp = m_boundProgram[kGLMVertexProgram];
	CGLMProgram *fp = m_boundProgram[kGLMFragmentProgram];

	bool is_draw = (info->m_caller==eDrawElements);
	const char *batchtype = is_draw ? "draw" : "clear";

	if (options & (1<<eDumpBatchInfo))
	{
		GLMPRINTF(("-D- %s === %s %d ======================================================== %s %d  frame %d", sg_pPIXName, batchtype, m_debugBatchIndex, batchtype, m_debugBatchIndex, m_debugFrameIndex ));
	}

	if (options & (1<<eDumpSurfaceInfo))
	{
		GLMPRINTF(("-D-" ));
		GLMPRINTF(("-D- surface info:"));
		GLMPRINTF(("-D- drawing FBO: %8x   bound draw-FBO: %8x  (%s)", m_drawingFBO, m_boundDrawFBO, (m_drawingFBO==m_boundDrawFBO) ? "in sync" : "desync!" ));

		CGLMFBO	*fbo = m_boundDrawFBO;
		for( int i=0; i<kAttCount; i++)
		{
			CGLMTex *tex = fbo->m_attach[i].m_tex;
			if (tex)
			{
				GLMPRINTF(("-D-    bound FBO (%8x)  attachment %d = tex %8x (GL %d) (%s)", fbo, i, tex, tex->m_texName, tex->m_layout->m_layoutSummary ));
			}
			else
			{
				// warning if no depthstencil attachment
				switch(i)
				{
					case kAttDepth:
					case kAttStencil:
					case kAttDepthStencil:
						GLMPRINTF(("-D-    bound FBO (%8x)  attachment %d = NULL, warning!", fbo, i ));
					break;
				}
			}
		}
	}

	if (options & (1<<eDumpStackCrawl))
	{
		CStackCrawlParams	cp;
		memset( &cp, 0, sizeof(cp) );
		cp.m_frameLimit = g_maxFramesToCrawl;
		
		g_extCocoaMgr->GetStackCrawl(&cp);
		
		GLMPRINTF(("-D-" ));
		GLMPRINTF(("-D- stack crawl"));
		for( int i=0; i< cp.m_frameCount; i++)
		{
			GLMPRINTF(("-D-\t%s", cp.m_crawlNames[i] ));
		}
	}

	if ( (options & (1<<eDumpShaderLinks)) && is_draw)
	{
		// we want to print out - GL name, pathname to disk copy if editable, extra credit would include the summary translation line
		// so grep for "#// trans#"
		char	attribtemp[1000];
		char	transtemp[1000];

		if (vp)
		{
			char *attribmap = strstr(vp->m_text, "#//ATTRIBMAP");
			if (attribmap)
			{
				CopyTilEOL( attribtemp, attribmap, sizeof(attribtemp) );
			}
			else
			{
				strcpy( attribtemp, "no attrib map" );
			}
			
			char *trans = strstr(vp->m_text, "#// trans#");
			if (trans)
			{
				CopyTilEOL( transtemp, trans, sizeof(transtemp) );
			}
			else
			{
				strcpy( transtemp, "no translation info" );
			}
			
			char *linkpath = "no file link";

			#if GLMDEBUG
				linkpath = vp->m_editable->m_mirror->m_path;
			#endif
			
			GLMPRINTF(("-D-"));
			GLMPRINTF(("-D- ARBVP ||  GL %d || Path %s ", vp->m_descs[kGLMARB].m_object.arb, linkpath ));
			GLMPRINTF(("-D-   Attribs %s", attribtemp ));
			GLMPRINTF(("-D-   Trans %s", transtemp ));

			/*
			if ( (options & (1<<eDumpShaderText)) && is_draw )
			{
				GLMPRINTF(("-D-"));
				GLMPRINTF(("-D- VP text " ));
				GLMPRINTTEXT(vp->m_string, eDebugDump ));
			}
			*/
		}
		else
		{
			GLMPRINTF(("-D- VP (none)" ));
		}

		if (fp)
		{
			char *trans = strstr(fp->m_text, "#// trans#");
			if (trans)
			{
				CopyTilEOL( transtemp, trans, sizeof(transtemp) );
			}
			else
			{
				strcpy( transtemp, "no translation info" );
			}
			
			char *linkpath = "no file link";

			#if GLMDEBUG
				linkpath = fp->m_editable->m_mirror->m_path;
			#endif
			
			GLMPRINTF(("-D-"));
			GLMPRINTF(("-D- FP ||  GL %d || Path %s ", fp->m_descs[kGLMARB].m_object.arb, linkpath ));
			GLMPRINTF(("-D-   Trans %s", transtemp ));

			/*
			if ( (options & (1<<eDumpShaderText)) && is_draw )
			{
				GLMPRINTF(("-D-"));
				GLMPRINTF(("-D- FP text " ));
				GLMPRINTTEXT((fp->m_string, eDebugDump));
			}
			*/
		}
		else
		{
			GLMPRINTF(("-D- FP (none)" ));
		}
	}

	if ( (options & (1<<eDumpShaderParameters)) && is_draw )
	{
		GLMPRINTF(("-D-"));
		GLMPRINTF(("-D- VP parameters" ));
		char *label = "";
		int labelcounter = 0;
		
		static int vmaskranges[] = { /*18,47,*/ -1,-1 };
		float	transposeTemp;				// row, column for printing
		
		int slotIndex = 0;
		int upperSlotLimit = 61;
			
			// take a peek at the vertex attrib setup.  If it has an attribute for bone weights, then raise the shader param dump limit to 256.
		bool usesSkinning = false;
		GLMVertexSetup *setup = &this->m_drawVertexSetup;			
		for( int index=0; index < kGLMVertexAttributeIndexMax; index++ )
		{
			usesSkinning |= (setup->m_attrMask & (1<<index)) && ((setup->m_vtxAttribMap[index]>>4)== D3DDECLUSAGE_BLENDWEIGHT);
		}
		if (usesSkinning)
		{
			upperSlotLimit = 256;
		}

		while( slotIndex < upperSlotLimit )
		{
			// if slot index is in a masked range, skip it
			// if slot index is the start of a matrix, label it, print it, skip ahead 4 slots
			for( int maski=0; vmaskranges[maski] >=0; maski+=2)
			{
				if ( (slotIndex >= vmaskranges[maski]) && (slotIndex <= vmaskranges[maski+1]) )
				{
					// that index is masked. set to one past end of range, print a blank line for clarity
					slotIndex = vmaskranges[maski+1]+1;
					GLMPrintStr("-D-     .....");
				}
			}

			if (slotIndex < upperSlotLimit)
			{
				float *values = &m_programParamsF[ kGLMVertexProgram ].m_values[slotIndex][0];
				switch( slotIndex )
				{
					case	4:
						printmat( "MODELVIEWPROJ", slotIndex, 4, values );
						slotIndex += 4;
					break;
					
					case	8:
						printmat( "VIEWPROJ", slotIndex, 4, values );
						slotIndex += 4;
						break;
						
					default:
						if (slotIndex>=58)
						{
							// bone
							char bonelabel[100];

							sprintf(bonelabel, "MODEL_BONE%-2d", (slotIndex-58)/3 );
							printmat( bonelabel, slotIndex, 3, values );

							slotIndex += 3;
						}
						else
						{
							// just print the one slot
							GLMPRINTF(("-D-    %03d: [ %10.5f %10.5f %10.5f %10.5f ]  %s",  slotIndex, values[0], values[1], values[2], values[3], label ));
							slotIndex++;
						}
					break;
				}
			}
		}

		// VP stage still, if in GLSL mode, find the bound pair and see if it has live i0, b0-b3 uniforms
		if (m_boundPair)	// should only be non-NULL in GLSL mode
		{
			if (m_boundPair->m_locVertexBool0>=0)
			{
				GLMPRINTF(("-D- GLSL 'b0': %d",  m_programParamsB[kGLMVertexProgram].m_values[0] ));
			}

			if (m_boundPair->m_locVertexBool1>=0)
			{
				GLMPRINTF(("-D- GLSL 'b1': %d",  m_programParamsB[kGLMVertexProgram].m_values[1] ));
			}

			if (m_boundPair->m_locVertexBool2>=0)
			{
				GLMPRINTF(("-D- GLSL 'b2': %d",  m_programParamsB[kGLMVertexProgram].m_values[2] ));
			}

			if (m_boundPair->m_locVertexBool3>=0)
			{
				GLMPRINTF(("-D- GLSL 'b3': %d",  m_programParamsB[kGLMVertexProgram].m_values[3] ));
			}

			if (m_boundPair->m_locVertexInteger0>=0)
			{
				GLMPRINTF(("-D- GLSL 'i0': %d",  m_programParamsI[kGLMVertexProgram].m_values[0][0] ));
			}
		}

		GLMPRINTF(("-D-"));
		GLMPRINTF(("-D- FP parameters " ));

		static int fmaskranges[] = { 40,41, -1,-1 };
		
		slotIndex = 0;
		label = "";
		while(slotIndex < 40)
		{
			// if slot index is in a masked range, skip it
			// if slot index is the start of a matrix, label it, print it, skip ahead 4 slots
			for( int maski=0; fmaskranges[maski] >=0; maski+=2)
			{
				if ( (slotIndex >= fmaskranges[maski]) && (slotIndex <= fmaskranges[maski+1]) )
				{
					// that index is masked. set to one past end of range, print a blank line for clarity
					slotIndex = fmaskranges[maski+1]+1;
					GLMPrintStr("-D-     .....");
				}
			}

			if (slotIndex < 40)
			{
				float *values = &m_programParamsF[ kGLMFragmentProgram ].m_values[slotIndex][0];
				switch( slotIndex )
				{
					case 0:	label = "g_EnvmapTint";										break;
					case 1:	label = "g_DiffuseModulation";								break;
					case 2:	label = "g_EnvmapContrast_ShadowTweaks";					break;
					case 3:	label = "g_EnvmapSaturation_SelfIllumMask (xyz, and w)";	break;
					case 4:	label = "g_SelfIllumTint_and_BlendFactor (xyz, and w)";		break;

					case 12:	label = "g_ShaderControls";				break;
					case 13:	label = "g_DepthFeatheringConstants";	break;

					case 20:	label = "g_EyePos";						break;
					case 21:	label = "g_FogParams";					break;
					case 22:	label = "g_FlashlightAttenuationFactors";	break;
					case 23:	label = "g_FlashlightPos";				break;
					case 24:	label = "g_FlashlightWorldToTexture";	break;

					case 28:	label = "cFlashlightColor";				break;
					case 29:	label = "g_LinearFogColor";				break;
					case 30:	label = "cLightScale";					break;
					case 31:	label = "cFlashlightScreenScale";		break;

					default:
						label = "";
					break;
				}

				GLMPRINTF(("-D-    %03d: [ %10.5f %10.5f %10.5f %10.5f ]  %s",  slotIndex, values[0], values[1], values[2], values[3], label ));

				slotIndex ++;
			}
		}
		
		if (m_boundPair->m_locFragmentFakeSRGBEnable)
		{
			GLMPRINTF(("-D- GLSL 'flEnableSRGBWrite': %f",  m_boundPair->m_fakeSRGBEnableValue ));
		}
	}

	if ( (options & (1<<eDumpTextureSetup)) && is_draw )
	{
		GLMPRINTF(( "-D-" ));
		GLMPRINTF(( "-D- Texture / Sampler setup" ));

		for( int i=0; i<GLM_SAMPLER_COUNT; i++ )
		{
			if (m_samplers[i].m_boundTex)
			{
				GLMTexSamplingParams *samp = &m_samplers[i].m_samp;
				GLMPRINTF(( "-D-" ));
				GLMPRINTF(("-D- Texture: %s", m_samplers[i].m_boundTex->m_debugLabel ));
				GLMPRINTF(("-D- Sampler %-2d tex %08x  layout %s", i, m_samplers[i].m_boundTex, m_samplers[i].m_boundTex->m_layout->m_layoutSummary ));

				GLMPRINTF(("-D-           addressMode[ %s %s %s ]",
					GLMDecode( eGL_ENUM, samp->m_addressModes[0] ),
					GLMDecode( eGL_ENUM, samp->m_addressModes[1] ),
					GLMDecode( eGL_ENUM, samp->m_addressModes[2] )
				));
					
				GLMPRINTF( ("-D-           magFilter    [ %s ]", GLMDecode( eGL_ENUM, samp->m_magFilter ) ) );
				GLMPRINTF( ("-D-           minFilter    [ %s ]", GLMDecode( eGL_ENUM, samp->m_minFilter ) ) );
				GLMPRINTF( ("-D-           maxAniso     [ %d ]", samp->m_maxAniso ) );
				GLMPRINTF( ("-D-           srgb         [ %s ]", samp->m_srgb ? "T" : "F" ) );
				GLMPRINTF( ("-D-           shadowFilter [ %s ]", samp->m_compareMode == GL_COMPARE_R_TO_TEXTURE_ARB ? "T" : "F" ) );
				
				// add more as needed later..
			}
		}		
	}

	if ( (options & (1<<eDumpVertexAttribSetup)) && is_draw )
	{
		GLMVertexSetup *setup = &this->m_drawVertexSetup;
		
		uint	relevantMask =	setup->m_attrMask;		
		for( int index=0; index < kGLMVertexAttributeIndexMax; index++ )
		{
			uint mask = 1<<index;
			if (relevantMask & mask)
			{
				GLMVertexAttributeDesc *setdesc = &setup->m_attrs[index];

				char	sizestr[100];
				if (setdesc->m_datasize < 32)
				{
					sprintf( sizestr, "%d", setdesc->m_datasize);
				}
				else
				{
					strcpy( sizestr, GLMDecode( eGL_ENUM, setdesc->m_datasize ) );
				}
				
				if (setup->m_vtxAttribMap[index] != 0xBB)
				{
					GLMPRINTF(("-D-   attr=%-2d  decl=$%s%1d  stride=%-2d  offset=%-3d  buf=%08x  size=%s  type=%s  normalized=%s  ",
									index,
									GLMDecode(eD3D_VTXDECLUSAGE, setup->m_vtxAttribMap[index]>>4 ),
									setup->m_vtxAttribMap[index]&0x0F,
									setdesc->m_stride,
									setdesc->m_offset,
									setdesc->m_buffer,
									sizestr,
									GLMDecode( eGL_ENUM, setdesc->m_datatype),
									setdesc->m_normalized?"Y":"N"
								));
				}
				else
				{
					// the attrib map is referencing an attribute that is not wired up in the vertex setup...
					Debugger();
				}
			}
		}
	}

	if ( (options & (1<<eDumpVertexData)) && is_draw )
	{
		GLMVertexSetup *setup = &this->m_drawVertexSetup;
		int start = info->m_drawStart;
		int end = info->m_drawEnd;
		int endLimit = start + (1<<g_maxVertsToDumpLog2);
		int realEnd = MIN( end, endLimit );

		// vertex data
		GLMPRINTF(("-D-"));
		GLMPRINTF(("-D- Vertex Data : %d of %d verts (index %d through %d)", realEnd-start, end-start, start, realEnd-1));
	
		for( int vtxIndex=-1; vtxIndex < realEnd; vtxIndex++ )	// vtxIndex will jump from -1 to start after first spin, not necessarily to 0
		{
			char buf[64000];
			char *mark = buf;
			
			// index -1 is the first run through the loop, we just print a header
			
			// iterate attrs
			if (vtxIndex>=0)
			{
				mark += sprintf(mark, "-D-  %04d: ", vtxIndex );
			}
			
				// for transform dumping, we latch values as we spot them
			float	vtxPos[4];
			int		vtxBoneIndices[4];	// only three get used
			float	vtxBoneWeights[4];	// only three get used and index 2 is synthesized from 0 and 1
			
			vtxPos[0] = vtxPos[1] = vtxPos[2] = 0.0;
			vtxPos[3] = 1.0;
			
			vtxBoneIndices[0] = vtxBoneIndices[1] = vtxBoneIndices[2] = vtxBoneIndices[3] = 0;
			vtxBoneWeights[0] = vtxBoneWeights[1] = vtxBoneWeights[2] = vtxBoneWeights[3] = 0.0;
			
			for( int attr = 0; attr < kGLMVertexAttributeIndexMax; attr++ )
			{
				if (setup->m_attrMask & (1<<attr) )
				{
					GLMVertexAttributeDesc *desc = &setup->m_attrs[ attr ];
					
					// print that attribute.

					// on OSX, VB's never move unless resized.  You can peek at them when unmapped.  Safe enough for debug..
					char *bufferBase = (char*)desc->m_buffer->m_lastMappedAddress;

					uint stride = desc->m_stride;
					uint fieldoffset = desc->m_offset;
					uint baseoffset = vtxIndex * stride;
					
					char *attrBase = bufferBase + baseoffset + fieldoffset;

					uint usage = setup->m_vtxAttribMap[attr]>>4;
					uint usageindex = setup->m_vtxAttribMap[attr]&0x0F;
					
					if (vtxIndex <0)
					{
						mark += sprintf(mark, "[%s%1d @ offs=%04d / strd %03d] ", GLMDecode(eD3D_VTXDECLUSAGE, usage ), usageindex, fieldoffset, stride );
					}
					else
					{
						mark += sprintf(mark, "[%s%1d ", GLMDecode(eD3D_VTXDECLUSAGE, usage ), usageindex );
						
						if (desc->m_datasize<32)
						{
							for( int which = 0; which < desc->m_datasize; which++ )
							{
								static char *fieldname = "xyzw";
								switch( desc->m_datatype )
								{
									case GL_FLOAT:
									{
										float	*floatbase = (float*)attrBase;
										mark += sprintf(mark, (usage != D3DDECLUSAGE_TEXCOORD) ? "%c%7.3f " : "%c%.3f", fieldname[which], floatbase[which] );
										
										if (usage==D3DDECLUSAGE_POSITION)
										{
											if (which<4)
											{
												// latch pos
												vtxPos[which] = floatbase[which];
											}
										}

										if (usage==D3DDECLUSAGE_BLENDWEIGHT)
										{
											if (which<4)
											{
												// latch weight
												vtxBoneWeights[which] = floatbase[which];
											}
										}
									}
									break;
									
									case GL_UNSIGNED_BYTE:
									{
										unsigned char *unchbase = (unsigned char*)attrBase;
										mark += sprintf(mark, "%c$%02X ", fieldname[which], unchbase[which] );
									}
									break;

									default:
										// hold off on other formats for now
										mark += sprintf(mark, "%c????? ", fieldname[which] );
									break;
								}
							}
						}
						else	// special path for BGRA bytes which are expressed in GL by setting the *size* to GL_BGRA (gross large enum)
						{
							switch(desc->m_datasize)
							{
								case GL_BGRA:		// byte reversed color
								{
									for( int which = 0; which < 4; which++ )
									{
										static const char *fieldname = "BGRA";
										switch( desc->m_datatype )
										{
											case GL_UNSIGNED_BYTE:
											{
												unsigned char *unchbase = (unsigned char*)attrBase;
												mark += sprintf(mark, "%c$%02X ", fieldname[which], unchbase[which] );
												
												if (usage==D3DDECLUSAGE_BLENDINDICES)
												{
													if (which<4)
													{
														// latch index
														vtxBoneIndices[which] = unchbase[which];		// ignoring the component reverse which BGRA would inflict, but we also ignore it below so it matches up.
													}
												}
											}												
											break;

											default:
												Debugger();
												break;
										}
									}
								}
								break;
							}
						}
						mark += sprintf(mark, "] " );
					}
				}
			}
			GLMPrintStr( buf, eDebugDump );

			if (vtxIndex >=0)
			{
				// if transform dumping requested, and we've reached the actual vert dump phase, do it
				float	vtxout[4];
				char	*translabel = NULL;   // NULL means no print...
				
				switch( g_vertDumpMode )
				{
					case eDumpVertsNoTransformDump:	break;
					
					case eDumpVertsTransformedByViewProj:				// viewproj is slot 8
					{
						float *viewproj = &m_programParamsF[ kGLMVertexProgram ].m_values[8][0];
						transform_dp4( vtxPos, viewproj, 4, vtxout );
						translabel = "post-viewproj";
					}
					break;
					
					case eDumpVertsTransformedByModelViewProj:			// modelviewproj is slot 4
					{
						float *modelviewproj = &m_programParamsF[ kGLMVertexProgram ].m_values[4][0];
						transform_dp4( vtxPos, modelviewproj, 4, vtxout );
						translabel = "post-modelviewproj";
					}
					break;
					
					case eDumpVertsTransformedByBoneZeroThenViewProj:
					{
						float	postbone[4];
						postbone[3] = 1.0;
						
						float *bonemat = &m_programParamsF[ kGLMVertexProgram ].m_values[58][0];
						transform_dp4( vtxPos, bonemat, 3, postbone );
						
						float *viewproj = &m_programParamsF[ kGLMVertexProgram ].m_values[8][0];	// viewproj is slot 8
						transform_dp4( postbone, viewproj, 4, vtxout );

						translabel = "post-bone0-viewproj";
					}
					break;
					
					case eDumpVertsTransformedByBonesThenViewProj:
					{
						float	bone[4][4];			// [bone index][bone member]	// members are adjacent
						
						vtxout[0] = vtxout[1] = vtxout[2] = vtxout[3] = 0;
						
						// unpack the third weight
						vtxBoneWeights[2] = 1.0 - (vtxBoneWeights[0] + vtxBoneWeights[1]);
						
						for( int ibone=0; ibone<3; ibone++ )
						{
							int boneindex = vtxBoneIndices[ ibone ];
							float *bonemat = &m_programParamsF[ kGLMVertexProgram ].m_values[58+(boneindex*3)][0];
							
							float boneweight = vtxBoneWeights[ibone];
							
							float	postbonevtx[4];
							
							transform_dp4( vtxPos, bonemat, 3, postbonevtx );
							
							// add weighted sum into output
							for( int which=0; which<4; which++ )
							{
								vtxout[which] += boneweight * postbonevtx[which];
							}
						}
						
						// fix W ?  do we care ?  check shaders to see what they do...
						translabel = "post-skin3bone-viewproj";
					}
					break;
				}
				if(translabel)
				{
					// for extra credit, do the perspective divide and viewport
					
					GLMPRINTF(("-D-   %-24s: [ %7.4f %7.4f %7.4f %7.4f ]", translabel, vtxout[0],vtxout[1],vtxout[2],vtxout[3] ));
					GLMPRINTF(("-D-" ));
				}
			}
			
			if (vtxIndex<0)
			{
				vtxIndex = start-1; // for printing of the data (note it will be incremented at bottom of loop, so bias down by 1)
			}
			else
			{	// no more < and > around vert dump lines
				//mark += sprintf(mark, "" );
			}
		}
	}

	if (options & (1<<eOpenShadersForEdit) )
	{
		#if GLMDEBUG
			if (m_drawingProgram[ kGLMVertexProgram ])
			{
				m_drawingProgram[ kGLMVertexProgram ]->m_editable->OpenInEditor();
			}
			
			if (m_drawingProgram[ kGLMFragmentProgram ])
			{
				m_drawingProgram[ kGLMFragmentProgram ]->m_editable->OpenInEditor();
			}
		#endif
	}
/*
	if (options & (1<<))
	{
	}
*/
	// trailer line
	GLMPRINTF(("-D- ===================================================================================== end %s %d  frame %d", batchtype, m_debugBatchIndex, m_debugFrameIndex  ));

	GLMSetIndent(oldIndent);
}

// here is the table that binds knob numbers to names.  change at will.
char	*g_knobnames[] = 
{
/*0*/	"dummy",

/*1*/	"FB-SRGB",
	#if 0
		/*1*/	"tex-U0-bias",	// src left
		/*2*/	"tex-V0-bias",	// src upper
		/*3*/	"tex-U1-bias",	// src right
		/*4*/	"tex-V1-bias",	// src bottom

		/*5*/	"pos-X0-bias",	// dst left
		/*6*/	"pos-Y0-bias",	// dst upper
		/*7*/	"pos-X1-bias",	// dst right
		/*8*/	"pos-Y1-bias",	// dst bottom
	#endif

};
int g_knobcount = sizeof( g_knobnames ) / sizeof( g_knobnames[0] );

void	GLMContext::DebugHook( GLMDebugHookInfo *info )
{
	bool debughook = false;
	// debug hook is called after an action has taken place.
	// that would be the initial action, or a repeat.
	// if paused, we stay inside this function until return.
	// when returning, we inform the caller if it should repeat its last action or continue.
	// there is no global pause state.  The rest of the app runs at the best speed it can.

	// initial stuff we do unconditionally
	
	// increment iteration
	info->m_iteration++;						// can be thought of as "number of times the caller's action has now occurred - starting at 1"

	// now set initial state guess for the info block (outcome may change below)
	info->m_loop = false;

	// check prior hold-conditions to see if any of them hit.
	// note we disarm each trigger once the hold has occurred (one-shot style)
	
	switch( info->m_caller )
	{
		case eBeginFrame:
			if (debughook) GLMPRINTF(("-D- Caller: BeginFrame" ));
			if ( (m_holdFrameBegin>=0) && (m_holdFrameBegin==m_debugFrameIndex) )		// did we hit a frame breakpoint?
			{
				if (debughook) GLMPRINTF(("-D-         BeginFrame trigger match, clearing m_holdFrameBegin, hold=true" ));

				m_holdFrameBegin = -1;

				info->m_holding = true;
			}
		break;

		case eClear:
			if (debughook) GLMPRINTF(("-D- Caller: Clear" ));
			if ( (m_holdBatch>=0) && (m_holdBatchFrame>=0) && (m_holdBatch==m_debugBatchIndex) && (m_holdBatchFrame==m_debugFrameIndex) )
			{
				if (debughook) GLMPRINTF(("-D-         Clear trigger match, clearing m_holdBatch&Frame, hold=true" ));

				m_holdBatch = m_holdBatchFrame = -1;

				info->m_holding = true;
			}
			break;

		case eDrawElements:
			if (debughook) GLMPRINTF(( (info->m_caller==eClear) ? "-D- Caller: Clear" : "-D- Caller: Draw" ));
			if ( (m_holdBatch>=0) && (m_holdBatchFrame>=0) && (m_holdBatch==m_debugBatchIndex) && (m_holdBatchFrame==m_debugFrameIndex) )
			{
				if (debughook) GLMPRINTF(("-D-         Draw trigger match, clearing m_holdBatch&Frame, hold=true" ));

				m_holdBatch = m_holdBatchFrame = -1;

				info->m_holding = true;
			}
		break;

		case eEndFrame:
			if (debughook) GLMPRINTF(("-D- Caller: EndFrame" ));

			// check for any expired batch hold req
			if ( (m_holdBatch>=0) && (m_holdBatchFrame>=0) && (m_holdBatchFrame==m_debugFrameIndex) )
			{
				// you tried to say 'next batch', but there wasn't one in this frame.
				// target first batch of next frame instead
				if (debughook) GLMPRINTF(("-D-         EndFrame noticed an expired draw hold trigger, rolling to next frame, hold=false"));

				m_holdBatch = 0;
				m_holdBatchFrame++;

				info->m_holding = false;
			}
			
			// now check for an explicit hold on end of this frame..
			if ( (m_holdFrameEnd>=0) && (m_holdFrameEnd==m_debugFrameIndex) )
			{
				if (debughook) GLMPRINTF(("-D-         EndFrame trigger match, clearing m_holdFrameEnd, hold=true" ));

				m_holdFrameEnd = -1;

				info->m_holding = true;
			}
		break;
	}

	// spin until event queue is empty *and* hold is false

	int evtcount=0;

	bool refresh = info->m_holding || m_debugDelayEnable;  // only refresh once per initial visit (if paused!) or follow up event input	
	int breakToDebugger = 0;
		// 1 = break to GDB
		// 2 = break to OpenGL Profiler if attached
	
	do
	{
		if (refresh)
		{
			if (debughook) GLMPRINTF(("-D- pushing pixels" ));
			this->DebugPresent();		// show pixels
			
			uint minidumpOptions = (1<<eDumpBatchInfo) /* | (1<<eDumpSurfaceInfo) */;
			this->DebugDump( info, minidumpOptions, g_vertDumpMode );
			
			usleep(10000);				// lil sleep
			
			refresh = false;
		}

		bool eventCheck = true;			// event pull will be skipped if we detect a shader edit being done
		// keep editable shaders in sync
		#if GLMDEBUG
		
			bool redrawBatch = false;
			if (m_drawingProgram[ kGLMVertexProgram ])
			{
				if( m_drawingProgram[ kGLMVertexProgram ]->SyncWithEditable() )
				{
					redrawBatch = true;
				}
			}
			
			if (m_drawingProgram[ kGLMFragmentProgram ])
			{
				if( m_drawingProgram[ kGLMFragmentProgram ]->SyncWithEditable() )
				{
					redrawBatch = true;
				}
			}
			
			if (redrawBatch)
			{
				// act as if user pressed the option-\ key
				
				if (m_drawingLang == kGLMGLSL)
				{
					// if GLSL mode, force relink - and refresh the pair cache as needed
					if (m_boundPair)
					{
						// fix it in place
						m_boundPair->RefreshProgramPair();
					}
				}
				FlushDrawStates( true );	// this is key, because the linked shader pair may have changed (note call to PurgePairsWithShader in cglmprogram.cpp)
				
				GLMPRINTF(("-- Shader changed, re-running batch" ));

				m_holdBatch = m_debugBatchIndex;
				m_holdBatchFrame = m_debugFrameIndex;
				m_debugDelayEnable = false;
				
				info->m_holding = false;
				info->m_loop = true;
				
				eventCheck = false;
			}
		#endif
		
		if(eventCheck)
		{
			g_extCocoaMgr->PumpWindowsMessageLoop();
			CCocoaEvent	evt;
			evtcount = 	g_extCocoaMgr->GetEvents( &evt, 1, true );	// asking for debug events only.
			if (evtcount)
			{
				// print it
				if (debughook) GLMPRINTF(("-D- Received debug key '%c' with modifiers %x", evt.m_UnicodeKeyUnmodified, evt.m_ModifierKeyMask ));
				
				// flag for refresh if we spin again
				refresh = 1;
				
				switch(evt.m_UnicodeKeyUnmodified)
				{
					case ' ':					// toggle pause
						// clear all the holds to be sure
						m_holdFrameBegin = m_holdFrameEnd = m_holdBatch = m_holdBatchFrame = -1;
						info->m_holding = !info->m_holding;
						
						if (!info->m_holding)
						{
							m_debugDelayEnable = false;	// coming out of pause means no slow mo
						}

						GLMPRINTF((info->m_holding ? "-D- Paused." :  "-D- Unpaused." ));
					break;
					
					case 'f':					// frame advance
						GLMPRINTF(("-D- Command: next frame" ));
						m_holdFrameBegin = m_debugFrameIndex+1;		// stop at top of next numbered frame
						m_debugDelayEnable = false;					// get there fast

						info->m_holding = false;
					break;

					case ']':					// ahead 1 batch
					case '}':					// ahead ten batches
					{
						int delta = evt.m_UnicodeKeyUnmodified == ']' ? 1 : 10;
						m_holdBatch = m_debugBatchIndex+delta;
						m_holdBatchFrame = m_debugFrameIndex;
						m_debugDelayEnable = false;					// get there fast
						info->m_holding = false;
						GLMPRINTF(("-D- Command: advance %d batches to %d", delta, m_holdBatch ));
					}
					break;

					case '[':					// back one batch
					case '{':					// back 10 batches
					{
						int delta = evt.m_UnicodeKeyUnmodified == '[' ? -1 : -10;
						m_holdBatch = m_debugBatchIndex + delta;
						if (m_holdBatch<0)
						{
							m_holdBatch = 0;
						}
						m_holdBatchFrame = m_debugFrameIndex+1;		// next frame, but prev batch #
						m_debugDelayEnable = false;					// get there fast
						info->m_holding = false;
						GLMPRINTF(("-D- Command: rewind %d batches to %d", delta, m_holdBatch ));
					}
					break;

					case '\\':					// batch rerun

						m_holdBatch = m_debugBatchIndex;
						m_holdBatchFrame = m_debugFrameIndex;
						m_debugDelayEnable = false;						
						info->m_holding = false;
						info->m_loop = true;
						GLMPRINTF(("-D- Command: re-run batch %d", m_holdBatch ));
					break;
					
					case 'c':					// toggle auto color clear
						m_autoClearColor = !m_autoClearColor;
						GLMPRINTF((m_autoClearColor ? "-D- Auto color clear ON" :  "-D- Auto color clear OFF" ));
					break;

					case 's':					// toggle auto stencil clear
						m_autoClearStencil = !m_autoClearStencil;
						GLMPRINTF((m_autoClearStencil ? "-D- Auto stencil clear ON" :  "-D- Auto stencil clear OFF" ));
					break;

					case 'd':					// toggle auto depth clear
						m_autoClearDepth = !m_autoClearDepth;
						GLMPRINTF((m_autoClearDepth ? "-D- Auto depth clear ON" :  "-D- Auto depth clear OFF" ));
					break;

					case '.':					// break to debugger  or insta-quit
						if (evt.m_ModifierKeyMask & (1<<eControlKey))
						{
							GLMPRINTF(( "-D- INSTA QUIT!  (TM) (PAT PEND)" ));
							abort();
						}
						else
						{
							GLMPRINTF(( "-D- Breaking to debugger" ));
							breakToDebugger = 1;

							info->m_holding = true;
							info->m_loop = true;	// so when you come back from debugger, you get another spin (i.e. you enter paused mode)
						}						
					break;
					
					case 'g':					// break to OGLP and enable OGLP logging of spew
						if (GLMDetectOGLP())	// if this comes back true, there will be a breakpoint set on glColor4sv.
						{
							uint channelMask = GLMDetectAvailableChannels();	// will re-assert whether spew goes to OGLP log
							
							if (channelMask & (1<<eGLProfiler))
							{
								GLMDebugChannelMask(&channelMask);
								breakToDebugger = 2;

								info->m_holding = true;
								info->m_loop = true;	// so when you come back from debugger, you get another spin (i.e. you enter paused mode)
							}
						}
					break;

					case '_':					// toggle slow mo
						m_debugDelayEnable = !m_debugDelayEnable;
					break;

					case '-':					// go slower
						if (m_debugDelayEnable)
						{
							// already in slow mo, so lower speed
							m_debugDelay <<= 1;	// double delay
							if (m_debugDelay > (1<<24))
							{
								m_debugDelay = (1<<24);
							}
						}
						else
						{
							// enter slow mo
							m_debugDelayEnable = true;
						}
					break;

					case '=':					// go faster
						if (m_debugDelayEnable)
						{
							// already in slow mo, so raise speed
							m_debugDelay >>= 1;	// halve delay
							if (m_debugDelay < (1<<17))
							{
								m_debugDelay = (1<<17);
							}
						}
						else
						{
							// enter slow mo
							m_debugDelayEnable = true;
						}
					break;
					
					case 'v':
						// open vs in editor (foreground pop)
						#if GLMDEBUG
							if (m_boundProgram[ kGLMVertexProgram ])
							{
								m_boundProgram[ kGLMVertexProgram ]->m_editable->OpenInEditor( true );
							}
						#endif						
					break;

					case 'p':
						// open fs/ps in editor (foreground pop)
						#if GLMDEBUG
							if (m_boundProgram[ kGLMFragmentProgram ])
							{
								m_boundProgram[ kGLMFragmentProgram ]->m_editable->OpenInEditor( true );
							}
						#endif
					break;
					
					case '<':	// dump fewer verts
					case '>':	// dump more verts
					{
						int delta = (evt.m_UnicodeKeyUnmodified=='>') ? 1 : -1;
						g_maxVertsToDumpLog2 = MIN( MAX( g_maxVertsToDumpLog2+delta, 0 ), 16 );
						
						// just re-dump the verts
						DebugDump( info, 1<<eDumpVertexData, g_vertDumpMode );
					}
					break;
					
					case 'x':	// adjust transform dump mode
					{
						int newmode = g_vertDumpMode+1;
						if (newmode >= eLastDumpVertsMode)
						{
							// wrap
							newmode = eDumpVertsNoTransformDump;
						}
						g_vertDumpMode = (EGLMVertDumpMode)newmode;
						
						GLMPRINTF(("-D- New vert dump mode is %s", g_vertDumpModeNames[g_vertDumpMode] ));
					}
					break;

					case	'u':	// more crawl
					{
						CStackCrawlParams	cp;
						memset( &cp, 0, sizeof(cp) );
						cp.m_frameLimit = kMaxCrawlFrames;
						
						g_extCocoaMgr->GetStackCrawl(&cp);
						
						GLMPRINTF(("-D-" ));
						GLMPRINTF(("-D- extended stack crawl:"));
						for( int i=0; i< cp.m_frameCount; i++)
						{
							GLMPRINTF(("-D-\t%s", cp.m_crawlNames[i] ));
						}
					}
					break;
						
					case 'q':
						DebugDump( info, 0xFFFFFFFF, g_vertDumpMode );
					break;
					
	
					case 'H':
					case 'h':
					{
						// toggle drawing language.  hold down shift key to do it immediately.
						
						if (m_caps.m_hasDualShaders)
						{
							bool immediate;
							
							immediate = evt.m_UnicodeKeyUnmodified == 'H';	// (evt.m_ModifierKeyMask & (1<<eShiftKey)) != 0;
							
							if (m_drawingLang==kGLMARB)
							{
								GLMPRINTF(( "-D- Setting GLSL language mode %s.", immediate ? "immediately" : "for next frame start" ));
								SetDrawingLang( kGLMGLSL, immediate );
							}
							else
							{
								GLMPRINTF(( "-D- Setting ARB language mode %s.", immediate ? "immediately" : "for next frame start" ));
								SetDrawingLang( kGLMARB, immediate );
							}
							refresh = immediate;
						}
						else
						{
							GLMPRINTF(("You can't change shader languages unless you launch with -glmdualshaders enabled"));
						}

					}
					break;
					

					// ======================================================== debug knobs.  change these as needed to troubleshoot stuff
					
					// keys to select a knob
					// or, toggle a debug flavor, if control is being held down
					case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
					{
						if (evt.m_ModifierKeyMask & (1<<eControlKey))
						{
							// '0' toggles the all-channels on or off
							int flavorSelect = evt.m_UnicodeKeyUnmodified - '0';
							
							if ( (flavorSelect >=0) && (flavorSelect<eFlavorCount) )
							{
								uint mask = GLMDebugFlavorMask();
								
								mask ^= (1<<flavorSelect);

								GLMDebugFlavorMask(&mask);
							}
						}
						else
						{
							// knob selection
							m_selKnobIndex = evt.m_UnicodeKeyUnmodified - '0';

							GLMPRINTF(("-D- Knob # %d (%s) selected.", m_selKnobIndex, g_knobnames[ m_selKnobIndex ] ));
							
							m_selKnobIncrement = (m_selKnobIndex<5) ? (1.0f / 2048.0f) : (1.0 / 256.0f);

							usleep( 500000 );
						}
						refresh = false;
					}
					break;
					
					// keys to adjust or zero a knob
					case 't':		// toggle
					{
						if (m_selKnobIndex < g_knobcount)
						{
							GLMKnobToggle( g_knobnames[ m_selKnobIndex ] );
						}
					}
					break;
					
					case 'l':		// less
					case 'm':		// more
					case 'z':		// zero
					{						
						if (m_selKnobIndex < g_knobcount)
						{
							float val = GLMKnob( g_knobnames[ m_selKnobIndex ], NULL );
							
							if (evt.m_UnicodeKeyUnmodified == 'l')
							{
								// minus (less)
								val -= m_selKnobIncrement;
								if (val < m_selKnobMinValue)
								{
									val = m_selKnobMinValue;
								}
								// send new value back to the knob
								GLMKnob( g_knobnames[ m_selKnobIndex ], &val );
							}

							if (evt.m_UnicodeKeyUnmodified == 'm')
							{
								// plus (more)
								val += m_selKnobIncrement;
								if (val > m_selKnobMaxValue)
								{
									val = m_selKnobMaxValue;
								}
								// send new value back to the knob
								GLMKnob( g_knobnames[ m_selKnobIndex ], &val );
							}
							
							if (evt.m_UnicodeKeyUnmodified == 'z')
							{
								// zero
								val = 0.0f;

								// send new value back to the knob
								GLMKnob( g_knobnames[ m_selKnobIndex ], &val );
							}
							
							GLMPRINTF(("-D- Knob # %d (%s) set to %f  (%f/1024.0)", m_selKnobIndex, g_knobnames[ m_selKnobIndex ], val, val * 1024.0 ));
							
							usleep( 500000 );
							refresh = false;
						}
					}
					break;

				}
			}
		}		
	}	while( ((evtcount>0) || info->m_holding) && (!breakToDebugger) );

	if (m_debugDelayEnable)
	{
		usleep( m_debugDelay );
	}

	if (breakToDebugger)
	{
		switch (breakToDebugger)
		{
			case 1:
				Debugger();
			break;
			
			case 2:
				short fakecolor[4];
				glColor4sv( fakecolor );	// break to OGLP
			break;
		}
		// re-flush all GLM states so you can fiddle with them in the debugger. then run the batch again and spin..
		FlushStates( true );
	}
}

void	GLMContext::DebugPresent( void )
{
	CGLMTex *drawBufferTex = m_drawingFBO->m_attach[kAttColor0].m_tex;
	glFinish();
	this->Present( drawBufferTex );
}

void	GLMContext::DebugClear( void )
{
	// get old clear color
	GLClearColor_t clearcol_orig;
	m_ClearColor.Read( &clearcol_orig,0 );
	
	// new clear color
	GLClearColor_t clearcol;
	clearcol.r = m_autoClearColorValues[0];
	clearcol.g = m_autoClearColorValues[1];
	clearcol.b = m_autoClearColorValues[2];
	clearcol.a = m_autoClearColorValues[3];
	m_ClearColor.Write( &clearcol, true, true ); // don't check, don't defer
	
	uint mask = 0;
	
	if (m_autoClearColor) mask |= GL_COLOR_BUFFER_BIT;
	if (m_autoClearDepth) mask |= GL_DEPTH_BUFFER_BIT;
	if (m_autoClearStencil) mask |= GL_STENCIL_BUFFER_BIT;
	
	glClear( mask );
	glFinish();

	// put old color back
	m_ClearColor.Write( &clearcol_orig, true, true ); // don't check, don't defer
}

#endif

void	GLMContext::DrawRangeElements(	GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices )
{
	GLM_FUNC;

//	CheckCurrent();
	
	m_debugBatchIndex++;				// batch index increments unconditionally on entry
	
	bool	hasVP = m_boundProgram[ kGLMVertexProgram ] != NULL;
	bool	hasFP = m_boundProgram[ kGLMFragmentProgram ] != NULL;
	
	void *indicesActual = (void*)indices;
	if (m_drawIndexBuffer->m_pseudo)
	{
		// you have to pass actual address, not offset... shhh... secret
		indicesActual = (void*)((int)indicesActual + (int)m_drawIndexBuffer->m_pseudoBuf);
	}
	
#if GLMDEBUG
	// init debug hook information
	GLMDebugHookInfo info;
	memset( &info, 0, sizeof(info) );
	info.m_caller = eDrawElements;
	
	// relay parameters we're operating under
	info.m_drawMode = mode;
	info.m_drawStart = start;
	info.m_drawEnd = end;
	info.m_drawCount = count;
	info.m_drawType = type;
	info.m_drawIndices = indices;
	
	do
	{
		// obey global options re pre-draw clear
		if (m_autoClearColor || m_autoClearDepth || m_autoClearStencil)
		{
			GLMPRINTF(("-- DrawRangeElements auto clear" ));
			this->DebugClear();
		}

		// always sync with editable shader text prior to draw
		#if GLMDEBUG
			//FIXME disengage this path if context is in GLSL mode..
			// it will need fixes to get the shader pair re-linked etc if edits happen anyway.
			
			if (m_boundProgram[ kGLMVertexProgram ])
			{
				m_boundProgram[ kGLMVertexProgram ]->SyncWithEditable();
			}
			else
			{
				AssertOnce(!"drawing with no vertex program bound");
			}

			
			if (m_boundProgram[ kGLMFragmentProgram ])
			{
				m_boundProgram[ kGLMFragmentProgram ]->SyncWithEditable();
			}
			else
			{
				AssertOnce(!"drawing with no fragment program bound");
			}
		#endif
		
		// do the drawing
		if (hasVP && hasFP)
		{
			glDrawRangeElements( mode, start, end, count, type, indicesActual );
			GLMCheckError();

			if (m_slowCheckEnable)
			{
				CheckNative();
			}
		}
		this->DebugHook( &info );
	} while (info.m_loop);
#else
	if (hasVP && hasFP)
	{
		glDrawRangeElements( mode, start, end, count, type, indicesActual );
		GLMCheckError();

		if (m_slowCheckEnable)
		{
			CheckNative();
		}
	}
#endif
}

void GLMContext::CheckNative( void )
{
	// note that this is available in release.  We don't use GLMPRINTF for that reason.
	// note we do not get called unless either slow-batch asserting or logging is enabled.
	
	bool gpuProcessing;
	GLint fragmentGPUProcessing, vertexGPUProcessing;
	
	CGLGetParameter (CGLGetCurrentContext(), kCGLCPGPUFragmentProcessing, &fragmentGPUProcessing);
	CGLGetParameter(CGLGetCurrentContext(), kCGLCPGPUVertexProcessing, &vertexGPUProcessing);

	// spews then asserts.
	// that way you can enable both, get log output on a pair if it's slow, and then the debugger will pop.
	if(m_slowSpewEnable)
	{
		if ( !vertexGPUProcessing )
		{
			m_boundProgram[ kGLMVertexProgram ]->LogSlow( m_drawingLang );
		}
		if ( !fragmentGPUProcessing )
		{
			m_boundProgram[ kGLMFragmentProgram ]->LogSlow( m_drawingLang );
		}
	}

	if(m_slowAssertEnable)
	{
		if ( !vertexGPUProcessing || !fragmentGPUProcessing)
		{
			Assert( !"slow batch" );
		}
	}
}



// debug font
void	GLMContext::GenDebugFontTex( void )
{
	if(!m_debugFontTex)
	{
		// make a 128x128 RGBA texture
		GLMTexLayoutKey key;
		memset( &key, 0, sizeof(key) );
		
		key.m_texGLTarget	= GL_TEXTURE_2D;
		key.m_xSize			= 128;
		key.m_ySize			= 128;
		key.m_zSize			= 1;
		key.m_texFormat		= D3DFMT_A8R8G8B8;
		key.m_texFlags		= 0;

		m_debugFontTex = this->NewTex( &key, "GLM debug font" );
		

		//-----------------------------------------------------
		GLMTexLockParams lockreq;
		
		lockreq.m_tex = m_debugFontTex;
		lockreq.m_face = 0;
		lockreq.m_mip = 0;

		GLMTexLayoutSlice *slice = &m_debugFontTex->m_layout->m_slices[ lockreq.m_tex->CalcSliceIndex( lockreq.m_face, lockreq.m_mip ) ];
		
		lockreq.m_region.xmin = lockreq.m_region.ymin = lockreq.m_region.zmin = 0;
		lockreq.m_region.xmax = slice->m_xSize;
		lockreq.m_region.ymax = slice->m_ySize;
		lockreq.m_region.zmax = slice->m_zSize;
		
		char	*lockAddress;
		int		yStride;
		int		zStride;
		
		m_debugFontTex->Lock( &lockreq, &lockAddress, &yStride, &zStride );
		GLMCheckError();
		
		//-----------------------------------------------------
		// fetch elements of font data and make texels... we're doing the whole slab so we don't really need the stride info
		unsigned long *destTexelPtr = (unsigned long *)lockAddress;

		for( int index = 0; index < 16384; index++ )
		{
			if (g_glmDebugFontMap[index] == ' ')
			{
				// clear
				*destTexelPtr = 0x00000000;
			}
			else
			{
				// opaque white (drawing code can modulate if desired)
				*destTexelPtr = 0xFFFFFFFF;
			}
			destTexelPtr++;
		}
		
		//-----------------------------------------------------
		GLMTexLockParams unlockreq;
		
		unlockreq.m_tex = m_debugFontTex;
		unlockreq.m_face = 0;
		unlockreq.m_mip = 0;

		// region need not matter for unlocks
		unlockreq.m_region.xmin = unlockreq.m_region.ymin = unlockreq.m_region.zmin = 0;
		unlockreq.m_region.xmax = unlockreq.m_region.ymax = unlockreq.m_region.zmax = 0;

		m_debugFontTex->Unlock( &unlockreq );
		GLMCheckError();

		//-----------------------------------------------------
			// change up the tex sampling on this texture to be "nearest" not linear
			
		//-----------------------------------------------------

		// don't leave texture bound on the TMU
		this->BindTexToTMU(NULL, 0 );
		
		// also make the index and vertex buffers for use - up to 1K indices and 1K verts
		
		uint indexBufferSize = 1024*2;
		
		m_debugFontIndices = this->NewBuffer(kGLMIndexBuffer, indexBufferSize, 0);	// two byte indices
		
		// we go ahead and lock it now, and fill it with indices 0-1023.
		char *indices = NULL;
		GLMBuffLockParams		idxLock;
		idxLock.m_offset		= 0;
		idxLock.m_size			= indexBufferSize;
		idxLock.m_nonblocking	= false;
		idxLock.m_discard		= false;
		m_debugFontIndices->Lock( &idxLock, &indices );
		for( int i=0; i<1024; i++)
		{
			unsigned short *idxPtr = &((unsigned short*)indices)[i];
			*idxPtr = i;
		}
		m_debugFontIndices->Unlock();
		
		m_debugFontVertices = this->NewBuffer(kGLMVertexBuffer, 1024 * 128, 0);	// up to 128 bytes per vert
	}
}

#define MAX_DEBUG_CHARS 256
struct GLMDebugTextVertex
{
	float	x,y,z;
	float	u,v;
	char	rgba[4];
};

void	GLMContext::DrawDebugText( float x, float y, float z, float drawCharWidth, float drawCharHeight, char *string )
{
	if (!m_debugFontTex)
	{
		GenDebugFontTex();
	}
	
	// setup needed to draw text
	
	// we're assuming that +x goes left to right on screen, no billboarding math in here
	// and that +y goes bottom up
	// caller knows projection / rectangle so it gets to decide vertex spacing
	
	// debug font must be bound to TMU 0
	// texturing enabled
	// alpha blending enabled
	// generate a quad per character
	//  characters are 6px wide by 11 px high.
	//	upper left character in tex is 0x20
	// y axis will need to be flipped for display
	
	// for any character in 0x20 - 0x7F - here are the needed UV's
	
	// leftU = ((character % 16) * 6.0f / 128.0f)
	// rightU = lowU + (6.0 / 128.0);
	// topV = ((character - 0x20) * 11.0f / 128.0f)
	// bottomV = lowV + (11.0f / 128.0f)
	
	int stringlen = strlen( string );
	if (stringlen > MAX_DEBUG_CHARS)
	{
		stringlen = MAX_DEBUG_CHARS;
	}

	// lock
	char					*vertices = NULL;
	GLMBuffLockParams		vtxLock;
	vtxLock.m_offset		= 0;
	vtxLock.m_size			= 1024 * stringlen;
	vtxLock.m_nonblocking	= false;
	vtxLock.m_discard		= false;
	m_debugFontVertices->Lock( &vtxLock, &vertices );
			
	GLMDebugTextVertex	*vtx =  (GLMDebugTextVertex*)vertices;
	GLMDebugTextVertex *vtxOutPtr = vtx;
	
	for( int charindex = 0; charindex < stringlen; charindex++ )
	{
		float	leftU,rightU,topV,bottomV;
		
		int character = (int)string[charindex];
		character -= 0x20;
		if ( (character<0) || (character > 0x7F) )
		{
			character = '*' - 0x20;
		}
		
		leftU	= ((character & 0x0F) * 6.0f ) / 128.0f;
		rightU	= leftU + (6.0f / 128.0f);

		topV	= ((character >> 4) * 11.0f ) / 128.0f;
		bottomV	= topV + (11.0f / 128.0f);
		
		float posx,posy,posz;
		
		posx = x + (drawCharWidth * (float)charindex);
		posy = y;
		posz = z;
		
		// generate four verts
		// first vert will be upper left of displayed quad (low X, high Y) then we go clockwise
		for( int quadvert = 0; quadvert < 4; quadvert++ )
		{
			bool isTop	= (quadvert <2);						// verts 0 and 1
			bool isLeft	= (quadvert & 1) == (quadvert >> 1);	// verts 0 and 3
			
			vtxOutPtr->x = posx + (isLeft ? 0.0f : drawCharWidth);
			vtxOutPtr->y = posy + (isTop ? drawCharHeight : 0.0f);
			vtxOutPtr->z = posz;
			
			vtxOutPtr->u = isLeft ? leftU : rightU;
			vtxOutPtr->v = isTop ? topV : bottomV;
			
			vtxOutPtr++;
		}
	}
	
	// verts are done.
	// unlock...
	
	m_debugFontVertices->Unlock();
	
	// make a vertex setup
	GLMVertexSetup vertSetup;
	
		// position, color, tc = 0, 3, 8
	vertSetup.m_attrMask = (1<<kGLMGenericAttr00) |  (1<<kGLMGenericAttr03) |  (1<<kGLMGenericAttr08);
	
	vertSetup.m_attrs[kGLMGenericAttr00].m_buffer = m_debugFontVertices;
	vertSetup.m_attrs[kGLMGenericAttr00].m_datasize	= 3;			// 3 floats
	vertSetup.m_attrs[kGLMGenericAttr00].m_datatype	= GL_FLOAT;
	vertSetup.m_attrs[kGLMGenericAttr00].m_stride	= sizeof(GLMDebugTextVertex);
	vertSetup.m_attrs[kGLMGenericAttr00].m_offset	= offsetof(GLMDebugTextVertex, x);
	vertSetup.m_attrs[kGLMGenericAttr00].m_normalized= false;

	vertSetup.m_attrs[kGLMGenericAttr03].m_buffer = m_debugFontVertices;
	vertSetup.m_attrs[kGLMGenericAttr03].m_datasize	= 4;			// four bytes
	vertSetup.m_attrs[kGLMGenericAttr03].m_datatype	= GL_UNSIGNED_BYTE;
	vertSetup.m_attrs[kGLMGenericAttr03].m_stride	= sizeof(GLMDebugTextVertex);
	vertSetup.m_attrs[kGLMGenericAttr03].m_offset	= offsetof(GLMDebugTextVertex, rgba);
	vertSetup.m_attrs[kGLMGenericAttr03].m_normalized= true;

	vertSetup.m_attrs[kGLMGenericAttr08].m_buffer = m_debugFontVertices;
	vertSetup.m_attrs[kGLMGenericAttr08].m_datasize	= 2;			// 2 floats
	vertSetup.m_attrs[kGLMGenericAttr08].m_datatype	= GL_FLOAT;
	vertSetup.m_attrs[kGLMGenericAttr08].m_stride	= sizeof(GLMDebugTextVertex);
	vertSetup.m_attrs[kGLMGenericAttr08].m_offset	= offsetof(GLMDebugTextVertex, u);
	vertSetup.m_attrs[kGLMGenericAttr03].m_normalized= false;

		
	// bind texture and draw it..
	this->BindTexToTMU( m_debugFontTex, 0 );
	
	SelectTMU(0);	// somewhat redundant
	
	glDisable( GL_DEPTH_TEST );
	
	glEnable(GL_TEXTURE_2D);
	GLMCheckError();

	if (0)
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		GLMCheckError();

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		GLMCheckError();
		
		glVertexPointer( 3, GL_FLOAT, sizeof( vtx[0] ), &vtx[0].x );
		GLMCheckError();
		
		glClientActiveTexture(GL_TEXTURE0);
		GLMCheckError();

		glTexCoordPointer( 2, GL_FLOAT, sizeof( vtx[0] ), &vtx[0].u );
		GLMCheckError();
	}
	else
	{
		SetVertexAttributes( &vertSetup );
	}

	glDrawArrays( GL_QUADS, 0, stringlen * 4 );
	GLMCheckError();

	// disable all the input streams
	if (0)
	{
		glDisableClientState(GL_VERTEX_ARRAY);
		GLMCheckError();

		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		GLMCheckError();
	}
	else
	{
		SetVertexAttributes( NULL );
	}

	glDisable(GL_TEXTURE_2D);
	GLMCheckError();

	this->BindTexToTMU( NULL, 0 );
}



//===============================================================================

void GLMgrSelfTests( void )	
{
	return;	// until such time as the tests are revised or axed
	
	// make a new context on renderer 0.
	GLMContext *ctx = GLMgr::aGLMgr()->NewContext( 0 );	////FIXME you can't make contexts this way any more.
	if (!ctx)
	{
		Debugger();		// no go
		return;
	}

	// make a test object based on that context.
	int alltests[] = {0,1,2,3,   -1};
	int newtests[] = {3, -1};
	int notests[] = {-1};
	
	int *testlist = notests;

	GLMTestParams	params;
	memset( &params, 0, sizeof(params) );

	params.m_ctx = ctx;
	params.m_testList = testlist;

	params.m_glErrToDebugger = true;
	params.m_glErrToConsole = true;
	
	params.m_intlErrToDebugger = true;
	params.m_intlErrToConsole = true;
	
	params.m_frameCount = 1000;

	GLMTester testobj( &params );

	testobj.RunTests( );
	
	GLMgr::aGLMgr()->DelContext( ctx );
}

void GLMContext::SetDefaultStates( void )
{
	GLM_FUNC;
	CheckCurrent();

	m_AlphaTestEnable.Default();
	m_AlphaTestFunc.Default();

	m_AlphaToCoverageEnable.Default();
	
	m_CullFaceEnable.Default();
	m_CullFrontFace.Default();
	
	m_PolygonMode.Default();
	m_DepthBias.Default();

	m_ClipPlaneEnable.Default();
	m_ClipPlaneEquation.Default();
	
	m_ScissorEnable.Default();	
	m_ScissorBox.Default();
	
	m_ViewportBox.Default();		
	m_ViewportDepthRange.Default();

	m_ColorMaskSingle.Default();	
	m_ColorMaskMultiple.Default();

	m_BlendEnable.Default();
	m_BlendFactor.Default();
	m_BlendEquation.Default();
	m_BlendColor.Default();
	//m_BlendEnableSRGB.Default();	// this isn't useful until there is an FBO bound - in fact it will trip a GL error.

	m_DepthTestEnable.Default();
	m_DepthFunc.Default();
	m_DepthMask.Default();
	
	m_StencilTestEnable.Default();
	m_StencilFunc.Default();
	m_StencilOp.Default();
	m_StencilWriteMask.Default();

	m_ClearColor.Default();
	m_ClearDepth.Default();
	m_ClearStencil.Default();	
}

void GLMContext::FlushStates( bool noDefer )
{
	GLM_FUNC;
	CheckCurrent();

	m_AlphaTestEnable.Flush( noDefer );
	m_AlphaTestFunc.Flush( noDefer );
	
	m_AlphaToCoverageEnable.Flush( noDefer );

	m_CullFaceEnable.Flush( noDefer );
	m_CullFrontFace.Flush( noDefer );
	
	m_PolygonMode.Flush( noDefer );
	m_DepthBias.Flush( noDefer );

	#if GLMDEBUG
		m_ClipPlaneEnable.Flush( true );	// always push clip state
		m_ClipPlaneEquation.Flush( true );
	#else
		m_ClipPlaneEnable.Flush( noDefer );
		m_ClipPlaneEquation.Flush( noDefer );
	#endif
	
	
	m_ScissorEnable.Flush( noDefer );	
	m_ScissorBox.Flush( noDefer );
	
	m_ViewportBox.Flush( noDefer );		
	m_ViewportDepthRange.Flush( noDefer );

	m_ColorMaskSingle.Flush( noDefer );	
	m_ColorMaskMultiple.Flush( noDefer );

	m_BlendEnable.Flush( noDefer );
	m_BlendFactor.Flush( noDefer );
	m_BlendEquation.Flush( noDefer );
	m_BlendColor.Flush( noDefer );
	
	// the next call should not occur until we're sure the proper SRGB tex format is underneath the FBO.
	// So, we're moving it up to FlushDrawStates so it can happen at just the right time.
	//m_BlendEnableSRGB.Flush( noDefer );
	
	m_DepthTestEnable.Flush( noDefer );
	m_DepthFunc.Flush( noDefer );
	m_DepthMask.Flush( noDefer );
	
	m_StencilTestEnable.Flush( noDefer );
	m_StencilFunc.Flush( noDefer );
	m_StencilOp.Flush( noDefer );
	m_StencilWriteMask.Flush( noDefer );

	m_ClearColor.Flush( noDefer );
	m_ClearDepth.Flush( noDefer );
	m_ClearStencil.Flush( noDefer );

	GLMCheckError();
}

void GLMContext::VerifyStates		( void )
{
	GLM_FUNC;
	CheckCurrent();

	// bare bones sanity check, head over to the debugger if our sense of the current context state is not correct
	// we should only want to call this after a flush or the checks will flunk.
	
	if( m_AlphaTestEnable.Check() )			GLMStop();
	if( m_AlphaTestFunc.Check() )			GLMStop();

	if( m_AlphaToCoverageEnable.Check() )	GLMStop();	

	if( m_CullFaceEnable.Check() )			GLMStop();
	if( m_CullFrontFace.Check() )			GLMStop();
	
	if( m_PolygonMode.Check() )				GLMStop();
	if( m_DepthBias.Check() )				GLMStop();

	if( m_ClipPlaneEnable.Check() )			GLMStop();
	//if( m_ClipPlaneEquation.Check() )		GLMStop();
	
	if( m_ScissorEnable.Check() )			GLMStop();	
	if( m_ScissorBox.Check() )				GLMStop();
	

	if( m_ViewportBox.Check() )				GLMStop();		
	if( m_ViewportDepthRange.Check() )		GLMStop();

	if( m_ColorMaskSingle.Check() )			GLMStop();	
	if( m_ColorMaskMultiple.Check() )		GLMStop();

	if( m_BlendEnable.Check() )				GLMStop();
	if( m_BlendFactor.Check() )				GLMStop();
	if( m_BlendEquation.Check() )			GLMStop();
	if( m_BlendColor.Check() )				GLMStop();
	
	// only do this as caps permit
	if (m_caps.m_hasGammaWrites)
	{
		if( m_BlendEnableSRGB.Check() )		GLMStop();
	}

	if( m_DepthTestEnable.Check() )			GLMStop();
	if( m_DepthFunc.Check() )				GLMStop();
	if( m_DepthMask.Check() )				GLMStop();
	
	if( m_StencilTestEnable.Check() )		GLMStop();
	if( m_StencilFunc.Check() )				GLMStop();
	if( m_StencilOp.Check() )				GLMStop();
	if( m_StencilWriteMask.Check() )		GLMStop();

	if( m_ClearColor.Check() )				GLMStop();
	if( m_ClearDepth.Check() )				GLMStop();
	if( m_ClearStencil.Check() )			GLMStop();
}

void	GLMContext::WriteAlphaTestEnable( GLAlphaTestEnable_t *src )
{
	m_AlphaTestEnable.Write( src );
}

void	GLMContext::WriteAlphaTestFunc( GLAlphaTestFunc_t *src )
{
	m_AlphaTestFunc.Write( src );
}

void	GLMContext::WriteAlphaToCoverageEnable( GLAlphaToCoverageEnable_t *src )
{
	m_AlphaToCoverageEnable.Write( src );
}

void	GLMContext::WriteCullFaceEnable( GLCullFaceEnable_t *src )
{
	m_CullFaceEnable.Write( src );
}

void	GLMContext::WriteCullFrontFace( GLCullFrontFace_t *src )
{
	m_CullFrontFace.Write( src );
}

void	GLMContext::WritePolygonMode( GLPolygonMode_t *src )
{
	m_PolygonMode.Write( src );
}

void	GLMContext::WriteDepthBias( GLDepthBias_t *src )
{
	m_DepthBias.Write( src );
}

void	GLMContext::WriteClipPlaneEnable( GLClipPlaneEnable_t *src, int which )
{
	m_ClipPlaneEnable.WriteIndex( src, which );
}

void	GLMContext::WriteClipPlaneEquation( GLClipPlaneEquation_t *src, int which )
{
	m_ClipPlaneEquation.WriteIndex( src, which );
}

void	GLMContext::WriteScissorEnable( GLScissorEnable_t *src )
{
	m_ScissorEnable.Write( src );
}

void	GLMContext::WriteScissorBox( GLScissorBox_t *src )
{
	m_ScissorBox.Write( src );
}

void	GLMContext::WriteViewportBox( GLViewportBox_t *src )
{
	m_ViewportBox.Write( src );
}

void	GLMContext::WriteViewportDepthRange( GLViewportDepthRange_t *src )
{
	m_ViewportDepthRange.Write( src );
}

void	GLMContext::WriteColorMaskSingle( GLColorMaskSingle_t *src )
{
	m_ColorMaskSingle.Write( src );
}

void	GLMContext::WriteColorMaskMultiple( GLColorMaskMultiple_t *src, int which )
{
	m_ColorMaskMultiple.WriteIndex( src, which );
}

void	GLMContext::WriteBlendEnable( GLBlendEnable_t *src )
{
	m_BlendEnable.Write( src );
}

void	GLMContext::WriteBlendFactor( GLBlendFactor_t *src )
{
	m_BlendFactor.Write( src );
}

void	GLMContext::WriteBlendEquation( GLBlendEquation_t *src )
{
	m_BlendEquation.Write( src );
}

void	GLMContext::WriteBlendColor( GLBlendColor_t *src )
{
	m_BlendColor.Write( src );
}

void	GLMContext::WriteBlendEnableSRGB( GLBlendEnableSRGB_t *src )
{
	if (m_caps.m_hasGammaWrites)	// only if caps allow do we actually push it through to the extension
	{
		m_BlendEnableSRGB.Write( src );
	}
	else
	{
		m_FakeBlendEnableSRGB = src->enable;
	}	
	// note however that we're still tracking what this mode should be, so FlushDrawStates can look at it and adjust the pixel shader
	// if fake SRGB mode is in place (m_caps.m_hasGammaWrites is false)
}

void	GLMContext::WriteDepthTestEnable( GLDepthTestEnable_t *src )
{
	m_DepthTestEnable.Write( src );
}

void	GLMContext::WriteDepthFunc( GLDepthFunc_t *src )
{
	m_DepthFunc.Write( src );
}

void	GLMContext::WriteDepthMask( GLDepthMask_t *src )
{
	m_DepthMask.Write( src );
}

void	GLMContext::WriteStencilTestEnable( GLStencilTestEnable_t *src )
{
	m_StencilTestEnable.Write( src );
}

void	GLMContext::WriteStencilFunc( GLStencilFunc_t *src )
{
	m_StencilFunc.Write( src );
}

void	GLMContext::WriteStencilOp( GLStencilOp_t *src, int which )
{
	m_StencilOp.WriteIndex( src, which );
}

void	GLMContext::WriteStencilWriteMask( GLStencilWriteMask_t *src )
{
	m_StencilWriteMask.Write( src );
}

void	GLMContext::WriteClearColor( GLClearColor_t *src )
{
	m_ClearColor.Write( src );
}

void	GLMContext::WriteClearDepth( GLClearDepth_t *src )
{
	m_ClearDepth.Write( src );
}

void	GLMContext::WriteClearStencil( GLClearStencil_t *src )
{
	m_ClearStencil.Write( src );
}

//===============================================================================
// template specializations for each type of state


//                                                                      --- GLAlphaTestEnable ---
void GLContextSet( GLAlphaTestEnable_t *src )
{
	glSetEnable( GL_ALPHA_TEST, src->enable );
}

void GLContextGet( GLAlphaTestEnable_t *dst )
{
	dst->enable = glIsEnabled( GL_ALPHA_TEST );
}

void GLContextGetDefault( GLAlphaTestEnable_t *dst )
{
	dst->enable = GL_FALSE;	
}

//                                                                      --- GLAlphaTestFunc ---
void GLContextSet( GLAlphaTestFunc_t *src )
{
	glAlphaFunc( src->func, src->ref );
}

void GLContextGet( GLAlphaTestFunc_t *dst )
{
	glGetEnumv( GL_ALPHA_TEST_FUNC, &dst->func );
	glGetFloatv( GL_ALPHA_TEST_REF, &dst->ref );
}

void GLContextGetDefault( GLAlphaTestFunc_t *dst )
{
	dst->func = GL_ALWAYS;
	dst->ref = 0.0f;
}

//                                                                      --- GLAlphaToCoverageEnable ---
void GLContextSet( GLAlphaToCoverageEnable_t *src )
{
	glSetEnable( GL_SAMPLE_ALPHA_TO_COVERAGE_ARB, src->enable );
}

void GLContextGet( GLAlphaToCoverageEnable_t *dst )
{
	dst->enable = glIsEnabled( GL_SAMPLE_ALPHA_TO_COVERAGE_ARB );
}

void GLContextGetDefault( GLAlphaToCoverageEnable_t *dst )
{
	dst->enable = GL_FALSE;	
}

//                                                                      --- GLCullFaceEnable ---
void GLContextSet( GLCullFaceEnable_t *src )
{
	glSetEnable( GL_CULL_FACE, src->enable );
}

void GLContextGet( GLCullFaceEnable_t *dst )
{
	dst->enable = glIsEnabled( GL_CULL_FACE );
}

void GLContextGetDefault( GLCullFaceEnable_t *dst )
{
	dst->enable = GL_TRUE;	
}


//                                                                      --- GLCullFrontFace ---
void GLContextSet( GLCullFrontFace_t *src )
{
	glFrontFace( src->value );	// legal values are GL_CW or GL_CCW
}

void GLContextGet( GLCullFrontFace_t *dst )
{
	glGetEnumv( GL_FRONT_FACE, &dst->value );
}

void GLContextGetDefault( GLCullFrontFace_t *dst )
{
	dst->value = GL_CCW;
}


//                                                                      --- GLPolygonMode ---
void GLContextSet( GLPolygonMode_t *src )
{
	glPolygonMode( GL_FRONT, src->values[0] );
	glPolygonMode( GL_BACK, src->values[1] );
}

void GLContextGet( GLPolygonMode_t *dst )
{
	glGetEnumv( GL_POLYGON_MODE, &dst->values[0] );

}

void GLContextGetDefault( GLPolygonMode_t *dst )
{
	dst->values[0] = dst->values[1] = GL_FILL;
}


//                                                                      --- GLDepthBias ---
// note the implicit enable / disable.
// if you set non zero values, it is enabled, otherwise not.
void GLContextSet( GLDepthBias_t *src )
{
	bool enable = (src->factor != 0.0f) || (src->units != 0.0f);
	
	glSetEnable( GL_POLYGON_OFFSET_FILL, enable );
	glPolygonOffset( src->factor, src->units );
}

void GLContextGet( GLDepthBias_t *dst )
{
	glGetFloatv		( GL_POLYGON_OFFSET_FACTOR, &dst->factor );
	glGetFloatv		( GL_POLYGON_OFFSET_UNITS, &dst->units );
}

void GLContextGetDefault( GLDepthBias_t *dst )
{
	dst->factor		= 0.0;
	dst->units		= 0.0;
}


//                                                                      --- GLScissorEnable ---
void GLContextSet( GLScissorEnable_t *src )
{
	glSetEnable( GL_SCISSOR_TEST, src->enable );
}

void GLContextGet( GLScissorEnable_t *dst )
{
	dst->enable = glIsEnabled( GL_SCISSOR_TEST );
}

void GLContextGetDefault( GLScissorEnable_t *dst )
{
	dst->enable = GL_FALSE;
}


//                                                                      --- GLScissorBox ---
void GLContextSet( GLScissorBox_t *src )
{
	glScissor ( src->x, src->y, src->width, src->height );
}

void GLContextGet( GLScissorBox_t *dst )
{
	glGetIntegerv ( GL_SCISSOR_BOX, &dst->x );
}

void GLContextGetDefault( GLScissorBox_t *dst )
{
	// hmmmm, good question?  we can't really know a good answer so we pick a silly one
	// and the client better come back with a better answer later.
	dst->x = dst->y = 0;
	dst->width = dst->height = 16;
}


//                                                                      --- GLViewportBox ---

void GLContextSet( GLViewportBox_t *src )
{
	glViewport (src->x, src->y, src->width, src->height );
}

void GLContextGet( GLViewportBox_t *dst )
{
	glGetIntegerv	( GL_VIEWPORT, &dst->x ); 
}

void GLContextGetDefault( GLViewportBox_t *dst )
{
	// as with the scissor box, we don't know yet, so pick a silly one and change it later
	dst->x = dst->y = 0;
	dst->width = dst->height = 16;
}


//                                                                      --- GLViewportDepthRange ---
void GLContextSet( GLViewportDepthRange_t *src )
{
	glDepthRange	( src->near, src->far );
}

void GLContextGet( GLViewportDepthRange_t *dst )
{
	glGetDoublev	( GL_DEPTH_RANGE, &dst->near );
}

void GLContextGetDefault( GLViewportDepthRange_t *dst )
{
	dst->near = 0.0;
	dst->far = 1.0;
}

//                                                                      --- GLClipPlaneEnable ---
void GLContextSetIndexed( GLClipPlaneEnable_t *src, int index )
{
	#if GLMDEBUG
	if (CommandLine()->FindParm("-caps_noclipplanes"))
	{
		if (GLMKnob("caps-key",NULL) > 0.0)
		{
			// caps ON means NO clipping
			src->enable = false;
		}
	}
	#endif
	glSetEnable( GL_CLIP_PLANE0 + index, src->enable );
	GLMCheckError();
}

void GLContextGetIndexed( GLClipPlaneEnable_t *dst, int index )
{
	dst->enable = glIsEnabled( GL_CLIP_PLANE0 + index );
}

void GLContextGetDefaultIndexed( GLClipPlaneEnable_t *dst, int index )
{
	dst->enable = 0;
}



//                                                                      --- GLClipPlaneEquation ---
void GLContextSetIndexed( GLClipPlaneEquation_t *src, int index )
{
	// shove into glGlipPlane
	GLdouble coeffs[4] = { src->x, src->y, src->z, src->w };

	glClipPlane( GL_CLIP_PLANE0 + index, coeffs );
	GLMCheckError();
}

void GLContextGetIndexed( GLClipPlaneEquation_t *dst, int index )
{
	Debugger();	 // do this later
//	glClipPlane( GL_CLIP_PLANE0 + index, coeffs );
//	GLdouble coeffs[4] = { src->x, src->y, src->z, src->w };
}

void GLContextGetDefaultIndexed( GLClipPlaneEquation_t *dst, int index )
{
	dst->x = 1.0;
	dst->y = 0.0;
	dst->z = 0.0;
	dst->w = 0.0;
}


//                                                                      --- GLColorMaskSingle ---
void GLContextSet( GLColorMaskSingle_t *src )
{
	glColorMask( src->r, src->g, src->b, src->a );
}

void GLContextGet( GLColorMaskSingle_t *dst )
{
	glGetBooleanv( GL_COLOR_WRITEMASK, (GLboolean*)&dst->r);
}

void GLContextGetDefault( GLColorMaskSingle_t *dst )
{
	dst->r = dst->g = dst->b = dst->a = 1;
}


//                                                                      --- GLColorMaskMultiple ---
void GLContextSetIndexed( GLColorMaskMultiple_t *src, int index )
{
	// FIXME:  this call is not in the Leopard headers.  A runtime-lookup will be needed.	
	pfnglColorMaskIndexedEXT ( index, src->r, src->g, src->b, src->a );
}

void GLContextGetIndexed( GLColorMaskMultiple_t *dst, int index )
{
	// FIXME:  this call is not in the Leopard headers.  A runtime-lookup will be needed.	
	glGetBooleanIndexedvEXT ( GL_COLOR_WRITEMASK, index, (GLboolean*)&dst->r );
}

void GLContextGetDefaultIndexed( GLColorMaskMultiple_t *dst, int index )
{
	dst->r = dst->g = dst->b = dst->a = 1;
}


//                                                                      --- GLBlendEnable ---
void GLContextSet( GLBlendEnable_t *src )
{
	glSetEnable( GL_BLEND, src->enable );
}

void GLContextGet( GLBlendEnable_t *dst )
{
	dst->enable = glIsEnabled( GL_BLEND );
}

void GLContextGetDefault( GLBlendEnable_t *dst )
{
	dst->enable = GL_FALSE;
}


//                                                                      --- GLBlendFactor ---
void GLContextSet( GLBlendFactor_t *src )
{
	glBlendFunc ( src->srcfactor, src->dstfactor );
}

void GLContextGet( GLBlendFactor_t *dst )
{
	glGetEnumv	( GL_BLEND_SRC, &dst->srcfactor );
	glGetEnumv	( GL_BLEND_DST, &dst->dstfactor );
}

void GLContextGetDefault( GLBlendFactor_t *dst )
{
	dst->srcfactor = GL_ONE;
	dst->dstfactor = GL_ZERO;
}


//                                                                      --- GLBlendEquation ---
void GLContextSet( GLBlendEquation_t *src )
{
	glBlendEquation ( src->equation );
}

void GLContextGet( GLBlendEquation_t *dst )
{
	glGetEnumv	( GL_BLEND_EQUATION, &dst->equation );
}

void GLContextGetDefault( GLBlendEquation_t *dst )
{
	dst->equation = GL_FUNC_ADD;
}


//                                                                      --- GLBlendColor ---
void GLContextSet( GLBlendColor_t *src )
{
	glBlendColor ( src->r, src->g, src->b, src->a );
}

void GLContextGet( GLBlendColor_t *dst )
{
	glGetFloatv	( GL_BLEND_COLOR, &dst->r );
}

void GLContextGetDefault( GLBlendColor_t *dst )
{
	//solid white
	dst->r = dst->g = dst->b = dst->a = 1.0;
}


//                                                                      --- GLBlendEnableSRGB ---

#define GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING	0x8210
#define GL_COLOR_ATTACHMENT0						0x8CE0

void GLContextSet( GLBlendEnableSRGB_t *src )
{
	#if GLMDEBUG
		// just check in debug... this is too expensive to look at on MTGL
		if (src->enable)
		{
			GLboolean	srgb_capable = false;
			glGetBooleanv( GL_FRAMEBUFFER_SRGB_CAPABLE_EXT, &srgb_capable);

			if (src->enable && !srgb_capable)
			{
				GLMPRINTF(("-Z- srgb-state-set FBO conflict: attempt to enable SRGB on non SRGB capable FBO config"));
			}
		}
	#endif
			// this query is not useful unless you have the ARB_framebuffer_srgb ext.
			//GLint encoding = 0;
			//pfnglGetFramebufferAttachmentParameteriv( GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding );
			//GLMCheckError();
	
	glSetEnable( GL_FRAMEBUFFER_SRGB_EXT, src->enable );
	GLMCheckError();
}

void GLContextGet( GLBlendEnableSRGB_t *dst )
{
	//dst->enable = glIsEnabled( GL_FRAMEBUFFER_SRGB_EXT );
	dst->enable = true; // wtf ?
}

void GLContextGetDefault( GLBlendEnableSRGB_t *dst )
{
	dst->enable = GL_FALSE;
}


//                                                                      --- GLDepthTestEnable ---
void GLContextSet( GLDepthTestEnable_t *src )
{
	glSetEnable( GL_DEPTH_TEST, src->enable );
}

void GLContextGet( GLDepthTestEnable_t *dst )
{
	dst->enable = glIsEnabled( GL_DEPTH_TEST );
}

void GLContextGetDefault( GLDepthTestEnable_t *dst )
{
	dst->enable = GL_FALSE;
}


//                                                                      --- GLDepthFunc ---
void GLContextSet( GLDepthFunc_t *src )
{
	glDepthFunc				( src->func );
}

void GLContextGet( GLDepthFunc_t *dst )
{
	glGetEnumv				( GL_DEPTH_FUNC, &dst->func );
}

void GLContextGetDefault( GLDepthFunc_t *dst )
{
	dst->func = GL_GEQUAL;
}


//                                                                      --- GLDepthMask ---
void GLContextSet( GLDepthMask_t *src )
{
	glDepthMask ( src->mask );
}

void GLContextGet( GLDepthMask_t *dst )
{
	glGetBooleanv ( GL_DEPTH_WRITEMASK, (GLboolean*)&dst->mask );
}

void GLContextGetDefault( GLDepthMask_t *dst )
{
	dst->mask = GL_TRUE;
}


//                                                                      --- GLStencilTestEnable ---
void GLContextSet( GLStencilTestEnable_t *src )
{
	glSetEnable( GL_STENCIL_TEST, src->enable );
}

void GLContextGet( GLStencilTestEnable_t *dst )
{
	dst->enable = glIsEnabled( GL_STENCIL_TEST );
}

void GLContextGetDefault( GLStencilTestEnable_t *dst )
{
	dst->enable = GL_FALSE;
}


//                                                                      --- GLStencilFunc ---
void GLContextSet( GLStencilFunc_t *src )
{
	glStencilFuncSeparateATI( src->frontfunc, src->backfunc, src->ref, src->mask);
}

void GLContextGet( GLStencilFunc_t *dst )
{
	glGetEnumv		( GL_STENCIL_FUNC, &dst->frontfunc );
	glGetEnumv		( GL_STENCIL_BACK_FUNC_ATI, &dst->backfunc );
	glGetIntegerv	( GL_STENCIL_REF, &dst->ref );
	glGetIntegerv	( GL_STENCIL_VALUE_MASK, (GLint*)&dst->mask );
}

void GLContextGetDefault( GLStencilFunc_t *dst )
{
	dst->frontfunc	= GL_ALWAYS;
	dst->backfunc	= GL_ALWAYS;
	dst->ref		= 0;
	dst->mask		= 0xFFFFFFFF;
}


//                                                                      --- GLStencilOp --- indexed 0=front, 1=back

void GLContextSetIndexed( GLStencilOp_t *src, int index )
{
	GLenum face = (index==0) ? GL_FRONT : GL_BACK;
	
    glStencilOpSeparateATI( face, src->sfail, src->dpfail, src->dppass );
}

void GLContextGetIndexed( GLStencilOp_t *dst, int index )
{
	GLenum face = (index==0) ? GL_FRONT : GL_BACK;

	glGetEnumv		( (index==0) ? GL_STENCIL_FAIL : GL_STENCIL_BACK_FAIL_ATI, &dst->sfail );
	glGetEnumv		( (index==0) ? GL_STENCIL_PASS_DEPTH_FAIL : GL_STENCIL_BACK_PASS_DEPTH_FAIL_ATI, &dst->dpfail );
	glGetEnumv		( (index==0) ? GL_STENCIL_PASS_DEPTH_PASS : GL_STENCIL_BACK_PASS_DEPTH_PASS_ATI, &dst->dppass );
}

void GLContextGetDefaultIndexed( GLStencilOp_t *dst, int index )
{
	dst->sfail = dst->dpfail = dst->dppass = GL_KEEP;
}


//                                                                      --- GLStencilWriteMask ---
void GLContextSet( GLStencilWriteMask_t *src )
{
	glStencilMask( src->mask );
}

void GLContextGet( GLStencilWriteMask_t *dst )
{
	glGetIntegerv	( GL_STENCIL_WRITEMASK, &dst->mask );
}

void GLContextGetDefault( GLStencilWriteMask_t *dst )
{
	dst->mask = 0xFFFFFFFF;
}


//                                                                      --- GLClearColor ---
void GLContextSet( GLClearColor_t *src )
{
	glClearColor( src->r, src->g, src->b, src->a );
}

void GLContextGet( GLClearColor_t *dst )
{
	glGetFloatv		( GL_COLOR_CLEAR_VALUE, &dst->r );
}

void GLContextGetDefault( GLClearColor_t *dst )
{
	dst->r = dst->g = dst->b = 0.5;
	dst->a = 1.0;
}


//                                                                      --- GLClearDepth ---
void GLContextSet( GLClearDepth_t *src )
{
	glClearDepth ( src->d );
}

void GLContextGet( GLClearDepth_t *dst )
{
	glGetDoublev ( GL_DEPTH_CLEAR_VALUE, &dst->d );
}

void GLContextGetDefault( GLClearDepth_t *dst )
{
	dst->d = 1.0;
}


//                                                                      --- GLClearStencil ---
void GLContextSet( GLClearStencil_t *src )
{
	glClearStencil( src->s );
}

void GLContextGet( GLClearStencil_t *dst )
{
	glGetIntegerv	( GL_STENCIL_CLEAR_VALUE, &dst->s );
}

void GLContextGetDefault( GLClearStencil_t *dst )
{
	dst->s = 0;
}

//===============================================================================


GLMTester::GLMTester(GLMTestParams *params)
{
	m_params = *params;
	
	m_drawFBO = NULL;
	m_drawColorTex = NULL;
	m_drawDepthTex = NULL;
}

GLMTester::~GLMTester()
{
}

void	GLMTester::StdSetup( void )
{
	GLMContext *ctx = m_params.m_ctx;	

	m_drawWidth = 1024;
	m_drawHeight = 768;
	
	// make an FBO to draw into and activate it. no depth buffer yet	
	m_drawFBO = ctx->NewFBO();					

	// make color buffer texture

	GLMTexLayoutKey colorkey;
	CGLMTex			*colortex;
	memset( &colorkey, 0, sizeof(colorkey) );
	
	colorkey.m_texGLTarget = GL_TEXTURE_2D;
	colorkey.m_xSize =	m_drawWidth;
	colorkey.m_ySize =	m_drawHeight;
	colorkey.m_zSize =	1;

	colorkey.m_texFormat	= D3DFMT_A8R8G8B8;
	colorkey.m_texFlags		= kGLMTexRenderable;

	m_drawColorTex = ctx->NewTex( &colorkey );

	// do not leave that texture bound on the TMU
	ctx->BindTexToTMU(NULL, 0 );
	
	
	// attach color to FBO
	GLMFBOTexAttachParams	colorParams;
	memset( &colorParams, 0, sizeof(colorParams) );
	
	colorParams.m_tex	= m_drawColorTex;
	colorParams.m_face	= 0;
	colorParams.m_mip	= 0;
	colorParams.m_zslice= 0;	// for clarity..
	
	m_drawFBO->TexAttach( &colorParams, kAttColor0 );
	
	// check it.
	bool ready = m_drawFBO->IsReady();
	InternalError( !ready, "drawing FBO no go");

	// bind it
	ctx->BindFBOToCtx( m_drawFBO, GL_READ_FRAMEBUFFER_EXT );
	ctx->BindFBOToCtx( m_drawFBO, GL_DRAW_FRAMEBUFFER_EXT );

	glViewport(0, 0, (GLsizei) m_drawWidth, (GLsizei) m_drawHeight );
	CheckGLError("stdsetup viewport");
	
	glScissor( 0,0,  (GLsizei) m_drawWidth, (GLsizei) m_drawHeight );
	CheckGLError("stdsetup scissor");

	glOrtho( -1,1, -1,1, -1,1 );
	CheckGLError("stdsetup ortho");
	
	// activate debug font
	ctx->GenDebugFontTex();
}

void	GLMTester::StdCleanup( void )
{
	GLMContext *ctx = m_params.m_ctx;	

	// unbind
	ctx->BindFBOToCtx( NULL, GL_READ_FRAMEBUFFER_EXT );
	ctx->BindFBOToCtx( NULL, GL_DRAW_FRAMEBUFFER_EXT );

	// del FBO
	if (m_drawFBO)
	{
		ctx->DelFBO( m_drawFBO );
		m_drawFBO = NULL;
	}
	
	// del tex
	if (m_drawColorTex)
	{
		ctx->DelTex( m_drawColorTex );
		m_drawColorTex = NULL;
	}

	if (m_drawDepthTex)
	{
		ctx->DelTex( m_drawDepthTex );
		m_drawDepthTex = NULL;
	}
}


void	GLMTester::Clear( void )
{
	GLMContext *ctx = m_params.m_ctx;	
	ctx->MakeCurrent();
	
	glViewport(0, 0, (GLsizei) m_drawWidth, (GLsizei) m_drawHeight );	
	glScissor( 0,0,  (GLsizei) m_drawWidth, (GLsizei) m_drawHeight );
	glOrtho( -1,1, -1,1, -1,1 );
	CheckGLError("clearing viewport");

	// clear to black
	GLfloat clear_color[4] = { 0.0f, 0.0f, 0.0, 1.0f };		
	glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
	CheckGLError("clearing color");

	glClear(GL_COLOR_BUFFER_BIT+GL_DEPTH_BUFFER_BIT+GL_STENCIL_BUFFER_BIT);
	CheckGLError("clearing");

	//glFinish();
	//CheckGLError("clear finish");
}

void	GLMTester::Present( int seed )
{
	GLMContext *ctx = m_params.m_ctx;	
	ctx->Present( m_drawColorTex );
	
}

void	GLMTester::CheckGLError( const char *comment )
{
	char errbuf[1024];

	//borrowed from GLMCheckError.. slightly different
	
	if (!comment)
	{
		comment = "";
	}
	
	GLenum errorcode = (GLenum)glGetError();
	GLenum errorcode2 = 0;
	if ( errorcode != GL_NO_ERROR )
	{
		const char	*decodedStr = GLMDecode( eGL_ERROR, errorcode );
		const char	*decodedStr2 = "";
				
		if ( errorcode == GL_INVALID_FRAMEBUFFER_OPERATION_EXT )
		{
			// dig up the more detailed FBO status
			errorcode2 = glCheckFramebufferStatusEXT( GL_FRAMEBUFFER_EXT );
			
			decodedStr2 = GLMDecode( eGL_ERROR, errorcode2 );

			sprintf( errbuf, "\n%s - GL Error %08x/%08x = '%s / %s'", comment, errorcode, errorcode2, decodedStr, decodedStr2 );
		}
		else
		{
			sprintf( errbuf, "\n%s - GL Error %08x = '%s'", comment, errorcode, decodedStr );
		}

		if ( m_params.m_glErrToConsole )
		{
			printf("%s", errbuf );
		}
		
		if ( m_params.m_glErrToDebugger )
		{
			Debugger();
		}
	}
}

void	GLMTester::InternalError( int errcode, char *comment )
{
	if (errcode)
	{
		if (m_params.m_intlErrToConsole)
		{	
			printf("\%s - error %d", comment, errcode );
		}

		if (m_params.m_intlErrToDebugger)
		{
			Debugger();
		}
	}
}


void	GLMTester::RunTests( void )
{
	int *testList = m_params.m_testList;
	
	while( (*testList >=0) && (*testList < 20) )
	{
		RunOneTest( *testList++ );
	}
}

void	GLMTester::RunOneTest( int testindex )
{
	// this might be better with 'ptmf' style
	switch(testindex)
	{
		case 0:	Test0();	break;
		case 1:	Test1();	break;
		case 2:	Test2();	break;
		case 3:	Test3();	break;

		default:
			Debugger();	// unrecognized
	}
}

// #####################################################################################################################

// some fixed lists which may be useful to all tests

D3DFORMAT g_drawTexFormatsGLMT[] =		// -1 terminated
{
	D3DFMT_A8R8G8B8,
	D3DFMT_A4R4G4B4,
	D3DFMT_X8R8G8B8,
	D3DFMT_X1R5G5B5,
	D3DFMT_A1R5G5B5,
	D3DFMT_L8,
	D3DFMT_A8L8,	
	D3DFMT_R8G8B8,	
	D3DFMT_A8,
	D3DFMT_R5G6B5,
	D3DFMT_DXT1,
	D3DFMT_DXT3,
	D3DFMT_DXT5,
	D3DFMT_A32B32G32R32F,
	D3DFMT_A16B16G16R16,

	(D3DFORMAT)-1
};

D3DFORMAT g_fboColorTexFormatsGLMT[] =		// -1 terminated
{
	D3DFMT_A8R8G8B8,
	//D3DFMT_A4R4G4B4,			//unsupported
	D3DFMT_X8R8G8B8,
	D3DFMT_X1R5G5B5,
	//D3DFMT_A1R5G5B5,			//unsupported
	D3DFMT_A16B16G16R16F,
	D3DFMT_A32B32G32R32F,
	D3DFMT_R5G6B5,

	(D3DFORMAT)-1			
};

D3DFORMAT g_fboDepthTexFormatsGLMT[] =		// -1 terminated, but note 0 for "no depth" mode
{
	(D3DFORMAT)0,
	D3DFMT_D16,
	D3DFMT_D24X8,
	D3DFMT_D24S8,
	
	(D3DFORMAT)-1	
};


// #####################################################################################################################

void	GLMTester::Test0( void )
{
	// make and delete a bunch of textures.
	// lock and unlock them.
	// use various combos of - 

	//	√texel format
	//	√2D | 3D | cube map
	//	√mipped / not
	//	√POT / NPOT
	//	large / small / square / rect
	//	square / rect
	
	GLMContext *ctx = m_params.m_ctx;	
	ctx->MakeCurrent();
	
	CUtlVector< CGLMTex* >	testTextures;		// will hold all the built textures
	
	// test stage loop
	// 0 is creation
	// 1 is lock/unlock
	// 2 is deletion
	
	for( int teststage = 0; teststage < 3; teststage++)
	{
		int innerindex = 0;	// increment at stage switch
		// format loop
		for( D3DFORMAT *fmtPtr = g_drawTexFormatsGLMT; *fmtPtr != ((D3DFORMAT)-1); fmtPtr++ )
		{
			// form loop
			GLenum	forms[] = { GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP, (GLenum)-1 };

			for( GLenum *formPtr = forms; *formPtr != ((GLenum)-1); formPtr++ )
			{
				// mip loop
				for( int mipped = 0; mipped < 2; mipped++ )
				{
					// large / square / pot loop
					// &4 == large		&2 == square		&1 == POT
					// NOTE you *have to be square* for cube maps.
					
					for( int aspect = 0; aspect < 8; aspect++ )
					{
						switch( teststage )
						{
							case 0:
							{
								GLMTexLayoutKey key;
								memset( &key, 0, sizeof(key) );
								
								key.m_texGLTarget	= *formPtr;
								key.m_texFormat		= *fmtPtr;
								if (mipped)
									key.m_texFlags |= kGLMTexMipped;
								
								// assume big, square, POT, and 3D, then adjust as needed
								key.m_xSize = key.m_ySize = key.m_zSize = 256;
								
								if ( !(aspect&4) )		// big or little ?
								{
									// little
									key.m_xSize >>= 2;
									key.m_ySize >>= 2;
									key.m_zSize >>= 2;
								}
								
								if ( key.m_texGLTarget != GL_TEXTURE_CUBE_MAP )
								{
									if ( !(aspect & 2) )	// square or rect?
									{
										// rect
										key.m_ySize >>= 1;
										key.m_zSize >>= 2;
									}
								}
								
								if ( !(aspect&1) )		// POT or NPOT?
								{
									// NPOT
									key.m_xSize += 56;
									key.m_ySize += 56;
									key.m_zSize += 56;
								}
								
								// 2D, 3D, cube map ?
								if (key.m_texGLTarget!=GL_TEXTURE_3D)
								{
									// 2D or cube map: flatten Z extent to one texel
									key.m_zSize = 1;
								}
								else
								{
									// 3D: knock down Z quite a bit so our test case does not run out of RAM
									key.m_zSize >>= 3;
									if (!key.m_zSize)
									{
										key.m_zSize = 1;
									}
								}

								CGLMTex *newtex = ctx->NewTex( &key );
								CheckGLError( "tex create test");
								InternalError( newtex==NULL, "tex create test" );
								
								testTextures.AddToTail( newtex );
								printf("\n[%5d] created tex %s",innerindex,newtex->m_layout->m_layoutSummary );
							}
							break;

							case 1:
							{
								CGLMTex	*ptex = testTextures[innerindex];

								for( int face=0; face <ptex->m_layout->m_faceCount; face++)
								{
									for( int mip=0; mip <ptex->m_layout->m_mipCount; mip++)
									{
										GLMTexLockParams lockreq;
										
										lockreq.m_tex = ptex;
										lockreq.m_face = face;
										lockreq.m_mip = mip;

										GLMTexLayoutSlice *slice = &ptex->m_layout->m_slices[ ptex->CalcSliceIndex( face, mip ) ];
										
										lockreq.m_region.xmin = lockreq.m_region.ymin = lockreq.m_region.zmin = 0;
										lockreq.m_region.xmax = slice->m_xSize;
										lockreq.m_region.ymax = slice->m_ySize;
										lockreq.m_region.zmax = slice->m_zSize;
										
										char	*lockAddress;
										int		yStride;
										int		zStride;
										
										ptex->Lock( &lockreq, &lockAddress, &yStride, &zStride );
										CheckGLError( "tex lock test");
										InternalError( lockAddress==NULL, "null lock address");

										// write some texels of this flavor:
										//	red 75%  green 40%  blue 15%  alpha 80%
										
										GLMGenTexelParams gtp;

										gtp.m_format			=	ptex->m_layout->m_format->m_d3dFormat;
										gtp.m_dest				=	lockAddress;
										gtp.m_chunkCount		=	(slice->m_xSize * slice->m_ySize * slice->m_zSize) / (ptex->m_layout->m_format->m_chunkSize * ptex->m_layout->m_format->m_chunkSize);
										gtp.m_byteCountLimit	=	slice->m_storageSize;
										gtp.r = 0.75;
										gtp.g = 0.40;
										gtp.b = 0.15;
										gtp.a = 0.80;

										GLMGenTexels( &gtp );
										
										InternalError( gtp.m_bytesWritten != gtp.m_byteCountLimit, "byte count mismatch from GLMGenTexels" );
									}
								}

								for( int face=0; face <ptex->m_layout->m_faceCount; face++)
								{
									for( int mip=0; mip <ptex->m_layout->m_mipCount; mip++)
									{
										GLMTexLockParams unlockreq;
										
										unlockreq.m_tex = ptex;
										unlockreq.m_face = face;
										unlockreq.m_mip = mip;

										// region need not matter for unlocks
										unlockreq.m_region.xmin = unlockreq.m_region.ymin = unlockreq.m_region.zmin = 0;
										unlockreq.m_region.xmax = unlockreq.m_region.ymax = unlockreq.m_region.zmax = 0;

										char	*lockAddress;
										int		yStride;
										int		zStride;
										
										ptex->Unlock( &unlockreq );

										CheckGLError( "tex unlock test");
									}
								}
								printf("\n[%5d] locked/wrote/unlocked tex %s",innerindex, ptex->m_layout->m_layoutSummary );
							}
							break;

							case 2:
							{
								CGLMTex	*dtex = testTextures[innerindex];

								printf("\n[%5d] deleting tex %s",innerindex, dtex->m_layout->m_layoutSummary );								
								ctx->DelTex( dtex );
								CheckGLError( "tex delete test");
							}
							break;
						}	// end stage switch
						innerindex++;
					}	// end aspect loop
				}	// end mip loop
			}	// end form loop
		}	// end format loop
	}	// end stage loop
}

// #####################################################################################################################
void	GLMTester::Test1( void )
{
	// FBO exercises
	GLMContext *ctx = m_params.m_ctx;	
	ctx->MakeCurrent();

	// FBO color format loop
	for( D3DFORMAT *colorFmtPtr = g_fboColorTexFormatsGLMT; *colorFmtPtr != ((D3DFORMAT)-1); colorFmtPtr++ )
	{
		// FBO depth format loop
		for( D3DFORMAT *depthFmtPtr = g_fboDepthTexFormatsGLMT; *depthFmtPtr != ((D3DFORMAT)-1); depthFmtPtr++ )
		{
			// mip loop
			for( int mipped = 0; mipped < 2; mipped++ )
			{
				GLenum	forms[] = { GL_TEXTURE_2D, GL_TEXTURE_3D, GL_TEXTURE_CUBE_MAP, (GLenum)-1 };

				// form loop
				for( GLenum *formPtr = forms; *formPtr != ((GLenum)-1); formPtr++ )
				{
					//=============================================== make an FBO
					CGLMFBO *fbo = ctx->NewFBO();					

					//=============================================== make a color texture
					GLMTexLayoutKey colorkey;
					memset( &colorkey, 0, sizeof(colorkey) );
					
					switch(*formPtr)
					{
						case GL_TEXTURE_2D:
							colorkey.m_texGLTarget = GL_TEXTURE_2D;
							colorkey.m_xSize =	800;
							colorkey.m_ySize =	600;
							colorkey.m_zSize =	1;
						break;
						
						case GL_TEXTURE_3D:
							colorkey.m_texGLTarget = GL_TEXTURE_3D;
							colorkey.m_xSize =	800;
							colorkey.m_ySize =	600;
							colorkey.m_zSize =	32;
						break;
						
						case GL_TEXTURE_CUBE_MAP:
							colorkey.m_texGLTarget = GL_TEXTURE_CUBE_MAP;
							colorkey.m_xSize =	800;
							colorkey.m_ySize =	800;	// heh, cube maps have to have square sides...
							colorkey.m_zSize =	1;
						break;
					}

					colorkey.m_texFormat	= *colorFmtPtr;
					colorkey.m_texFlags		= kGLMTexRenderable;
					// decide if we want mips
					if (mipped)
					{
						colorkey.m_texFlags		|= kGLMTexMipped;
					}

					CGLMTex	*colorTex = ctx->NewTex( &colorkey );
					// Note that GLM will notice the renderable flag, and force texels to be written
					// so the FBO will be complete

					//=============================================== attach color
					GLMFBOTexAttachParams	colorParams;
					memset( &colorParams, 0, sizeof(colorParams) );
					
					colorParams.m_tex	= colorTex;
					colorParams.m_face	= (colorkey.m_texGLTarget == GL_TEXTURE_CUBE_MAP) ? 2 : 0;	// just steer to an alternate face as a test

					colorParams.m_mip	= (colorkey.m_texFlags & kGLMTexMipped) ? 2 : 0;	// pick non-base mip slice

					colorParams.m_zslice= (colorkey.m_texGLTarget == GL_TEXTURE_3D) ? 3 : 0;		// just steer to an alternate slice as a test;
					
					fbo->TexAttach( &colorParams, kAttColor0 );
					

					//=============================================== optional depth tex
					CGLMTex *depthTex = NULL;
					
					if (*depthFmtPtr > 0 )
					{
						GLMTexLayoutKey depthkey;
						memset( &depthkey, 0, sizeof(depthkey) );
						
						depthkey.m_texGLTarget		= GL_TEXTURE_2D;
						depthkey.m_xSize			= colorkey.m_xSize >> colorParams.m_mip;	// scale depth tex to match color tex
						depthkey.m_ySize			= colorkey.m_ySize >> colorParams.m_mip;
						depthkey.m_zSize			= 1;

						depthkey.m_texFormat		= *depthFmtPtr;
						depthkey.m_texFlags			= kGLMTexRenderable | kGLMTexIsDepth;		// no mips.
						if (depthkey.m_texFormat==D3DFMT_D24S8)
						{
							depthkey.m_texFlags |= kGLMTexIsStencil;
						}

						depthTex = ctx->NewTex( &depthkey );


						//=============================================== attach depth
						GLMFBOTexAttachParams	depthParams;
						memset( &depthParams, 0, sizeof(depthParams) );
						
						depthParams.m_tex	= depthTex;
						depthParams.m_face	= 0;
						depthParams.m_mip	= 0;
						depthParams.m_zslice= 0;
						
						EGLMFBOAttachment depthAttachIndex = (depthkey.m_texFlags & kGLMTexIsStencil) ? kAttDepthStencil : kAttDepth;
						fbo->TexAttach( &depthParams, depthAttachIndex );
					}

					printf("\n FBO:\n   color tex %s\n   depth tex %s",
						colorTex->m_layout->m_layoutSummary,
						depthTex ? depthTex->m_layout->m_layoutSummary : "none"
						);
					
					// see if FBO is happy
					bool ready = fbo->IsReady();

					printf("\n   -> %s\n", ready ? "pass" : "fail" );
					
					// unbind
					ctx->BindFBOToCtx( NULL, GL_READ_FRAMEBUFFER_EXT );
					ctx->BindFBOToCtx( NULL, GL_DRAW_FRAMEBUFFER_EXT );

					// del FBO
					ctx->DelFBO(fbo);
					
					// del texes
					ctx->DelTex( colorTex );
					if (depthTex) ctx->DelTex( depthTex );
				} // end form loop
			} // end mip loop
		} // end depth loop
	} // end color loop
}

// #####################################################################################################################

static int selftest2_seed = 0;	// inc this every run to force main thread to teardown/reset display view
void	GLMTester::Test2( void )
{
	GLMContext *ctx = m_params.m_ctx;	
	ctx->MakeCurrent();

	this->StdSetup();	// default test case drawing setup

	// draw stuff (loop...)
	for( int i=0; i<m_params.m_frameCount; i++)
	{
		// ramping shades of blue...
		GLfloat clear_color[4] = { 0.50f, 0.05f, ((float)(i%100)) / 100.0, 1.0f };		
		glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
		CheckGLError("test2 clear color");

		glClear(GL_COLOR_BUFFER_BIT+GL_DEPTH_BUFFER_BIT+GL_STENCIL_BUFFER_BIT);
		CheckGLError("test2 clearing");

		// try out debug text
		for( int j=0; j<16; j++)
		{
			char text[256];
			sprintf(text, "The quick brown fox jumped over the lazy dog %d times", i );
			
			float theta = ( (i*0.10f) + (j * 6.28f) ) / 16.0f;
			
			float posx = cos(theta) * 0.5;
			float posy = sin(theta) * 0.5;
			
			float charwidth = 6.0 * (2.0 / 1024.0);
			float charheight = 11.0 * (2.0 / 768.0);
			
			ctx->DrawDebugText( posx, posy, 0.0f, charwidth, charheight, text );
		}
		glFinish();
		CheckGLError("test2 finish");

		this->Present( selftest2_seed );
	}
	
	this->StdCleanup();
	
	selftest2_seed++;
}

// #####################################################################################################################

static char g_testVertexProgram01 [] = 
{
	"!!ARBvp1.0  \n"
	"TEMP vertexClip;  \n"
	"DP4 vertexClip.x, state.matrix.mvp.row[0], vertex.position;  \n"
	"DP4 vertexClip.y, state.matrix.mvp.row[1], vertex.position;  \n"
	"DP4 vertexClip.z, state.matrix.mvp.row[2], vertex.position;  \n"
	"DP4 vertexClip.w, state.matrix.mvp.row[3], vertex.position;  \n"
	"ADD vertexClip.y, vertexClip.x, vertexClip.y;  \n"
	"MOV result.position, vertexClip;  \n"
	"MOV result.color, vertex.color;  \n"
	"MOV result.texcoord[0], vertex.texcoord;  \n"
	"END  \n"
};

static char g_testFragmentProgram01 [] =
{
	"!!ARBfp1.0  \n"
	"TEMP color;  \n"
	"MUL color, fragment.texcoord[0].y, 2.0;  \n"
	"ADD color, 1.0, -color;  \n"
	"ABS color, color;  \n"
	"ADD result.color, 1.0, -color;  \n"
	"MOV result.color.a, 1.0;  \n"
	"END  \n"
};


// generic attrib versions..

static char g_testVertexProgram01_GA [] = 
{
	"!!ARBvp1.0  \n"
	"TEMP vertexClip;  \n"
	"DP4 vertexClip.x, state.matrix.mvp.row[0], vertex.attrib[0];  \n"
	"DP4 vertexClip.y, state.matrix.mvp.row[1], vertex.attrib[0];  \n"
	"DP4 vertexClip.z, state.matrix.mvp.row[2], vertex.attrib[0];  \n"
	"DP4 vertexClip.w, state.matrix.mvp.row[3], vertex.attrib[0];  \n"
	"ADD vertexClip.y, vertexClip.x, vertexClip.y;  \n"
	"MOV result.position, vertexClip;  \n"
	"MOV result.color, vertex.attrib[3];  \n"
	"MOV result.texcoord[0], vertex.attrib[8];  \n"
	"END  \n"
};

static char g_testFragmentProgram01_GA [] =
{
	"!!ARBfp1.0  \n"
	"TEMP color;  \n"
	"TEX color, fragment.texcoord[0], texture[0], 2D;"
	//"MUL color, fragment.texcoord[0].y, 2.0;  \n"
	//"ADD color, 1.0, -color;  \n"
	//"ABS color, color;  \n"
	//"ADD result.color, 1.0, -color;  \n"
	//"MOV result.color.a, 1.0;  \n"
	"MOV result.color, color;  \n"
	"END  \n"
};


void	GLMTester::Test3( void )
{
	/**************************
	XXXXXXXXXXXXXXXXXXXXXX	stale test code until we revise the program interface
		
	GLMContext *ctx = m_params.m_ctx;	
	ctx->MakeCurrent();

	this->StdSetup();	// default test case drawing setup

	// make vertex&pixel shader
	CGLMProgram *vprog = ctx->NewProgram( kGLMVertexProgram, g_testVertexProgram01_GA );
	ctx->BindProgramToCtx( kGLMVertexProgram, vprog );
	
	CGLMProgram *fprog = ctx->NewProgram( kGLMFragmentProgram, g_testFragmentProgram01_GA );
	ctx->BindProgramToCtx( kGLMFragmentProgram, fprog );
	
	// draw stuff (loop...)
	for( int i=0; i<m_params.m_frameCount; i++)
	{
		// ramping shades of blue...
		GLfloat clear_color[4] = { 0.50f, 0.05f, ((float)(i%100)) / 100.0, 1.0f };		
		glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
		CheckGLError("test3 clear color");

		glClear(GL_COLOR_BUFFER_BIT+GL_DEPTH_BUFFER_BIT+GL_STENCIL_BUFFER_BIT);
		CheckGLError("test3 clearing");

		// try out debug text
		for( int j=0; j<16; j++)
		{
			char text[256];
			sprintf(text, "This here is running through a trivial vertex shader");
			
			float theta = ( (i*0.10f) + (j * 6.28f) ) / 16.0f;
			
			float posx = cos(theta) * 0.5;
			float posy = sin(theta) * 0.5;
			
			float charwidth = 6.0 * (2.0 / 800.0);
			float charheight = 11.0 * (2.0 / 640.0);
			
			ctx->DrawDebugText( posx, posy, 0.0f, charwidth, charheight, text );
		}
		glFinish();
		CheckGLError("test3 finish");

		this->Present( 3333 );
	}
	
	this->StdCleanup();
	*****************************/
}


