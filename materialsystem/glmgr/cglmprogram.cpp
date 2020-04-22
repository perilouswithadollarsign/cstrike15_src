//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// cglmprogram.cpp
//
//===============================================================================

#include "glmgr/glmgr.h"
#include "glmgr/cglmprogram.h"
#include "filesystem.h"
#include "tier1/keyvalues.h"

// memdbgon -must- be the last include file in a .cpp file.
#include "tier0/memdbgon.h"


//===============================================================================

ConVar	gl_shaderpair_cacherows_lg2( "gl_paircache_rows_lg2", "10");		// 10 is minimum
ConVar	gl_shaderpair_cacheways_lg2( "gl_paircache_ways_lg2", "5");		// 5 is minimum
ConVar	gl_shaderpair_cachelog( "gl_shaderpair_cachelog", "0" );

//===============================================================================


GLenum	GLMProgTypeToARBEnum( EGLMProgramType type )
{
	GLenum	result = 0;
	switch(type)
	{
		case	kGLMVertexProgram:		result = GL_VERTEX_PROGRAM_ARB; break;
		case	kGLMFragmentProgram:	result = GL_FRAGMENT_PROGRAM_ARB; break;
		default:	Assert( !"bad program type"); result = 0; break;
	}
	return result;
}

GLenum	GLMProgTypeToGLSLEnum( EGLMProgramType type )
{
	GLenum	result = 0;
	switch(type)
	{
		case	kGLMVertexProgram:		result = GL_VERTEX_SHADER_ARB; break;
		case	kGLMFragmentProgram:	result = GL_FRAGMENT_SHADER_ARB; break;
		default:	Assert( !"bad program type"); result = 0; break;
	}
	return result;
}

static uint g_shader_serial = 1000000;

CGLMProgram::CGLMProgram( GLMContext *ctx, EGLMProgramType type )
{
	m_ctx = ctx;
	m_ctx->CheckCurrent();

	m_type		= type;
	m_serial	= g_shader_serial++;
	m_text		= NULL;	// no text yet
	
#if GLMDEBUG
	m_editable	= NULL;
#endif

	memset( &m_descs, 0, sizeof( m_descs ) );

	m_samplerMask	 = 0;	// dxabstract sets this field later

	// create an ARB vp/fp program object name.  No need to bind it yet.
	GLMShaderDesc *arbDesc = &m_descs[ kGLMARB ];
	glGenProgramsARB( 1, &arbDesc->m_object.arb );

	// create a GLSL shader object.
	GLMShaderDesc *glslDesc = &m_descs[ kGLMGLSL ];
	GLenum glslStage = GLMProgTypeToGLSLEnum( m_type );

	glslDesc->m_object.glsl = glCreateShaderObjectARB( glslStage );;

	// no text has arrived yet.  That's done in SetProgramText.
}

CGLMProgram::~CGLMProgram( )
{
	m_ctx->CheckCurrent();
	
	// if there is an arb program, delete it
	GLMShaderDesc *arbDesc = &m_descs[ kGLMARB ];
	if (arbDesc->m_object.arb)
	{
		glDeleteProgramsARB( 1, &arbDesc->m_object.arb );
		GLMCheckError();
		arbDesc->m_object.arb = 0;
	}
	
	// if there is a GLSL shader, delete it
	GLMShaderDesc *glslDesc = &m_descs[kGLMGLSL];
	if (glslDesc->m_object.glsl)
	{
		glDeleteShader( (uint)glslDesc->m_object.glsl );	// why do I need a cast here again ?
		GLMCheckError();
		glslDesc->m_object.glsl = 0;
	}

#if GLMDEBUG
	if (m_editable)
	{
		delete m_editable;
		m_editable = NULL;
	}
#endif

	if (m_text)
	{
		free( m_text );
		m_text = NULL;
	}
	m_ctx = NULL;
}

enum EShaderSection
{
	kGLMARBVertex,		kGLMARBVertexDisabled,
	kGLMARBFragment,	kGLMARBFragmentDisabled,
	kGLMGLSLVertex,		kGLMGLSLVertexDisabled,
	kGLMGLSLFragment,	kGLMGLSLFragmentDisabled,

};

const char *g_shaderSectionMarkers[] =	// match ordering of enum
{
	"!!ARBvp",	"-!!ARBvp",			// enabled and disabled markers.  so you can have multiple flavors in a blob and activate the one you want.
	"!!ARBfp",	"-!!ARBfp",
	"//GLSLvp",	"-//GLSLvp",
	"//GLSLfp",	"-//GLSLfp",
	NULL
};

void	CGLMProgram::SetProgramText( char *text )
{
	// free old text if any
	// clone new text
	// scan newtext to find sections
	// walk sections, and mark descs to indicate where text is at
	
	if (m_text)
	{
		free( m_text );
		m_text = NULL;
	}
	
	// scrub desc text references
	for( int i=0; i<kGLMNumProgramTypes; i++)
	{
		GLMShaderDesc	*desc = &m_descs[i];
		
		desc->m_textPresent = false;
		desc->m_textOffset	= 0;
		desc->m_textLength	= 0;
	}
	
	m_text = strdup( text );
	Assert( m_text != NULL );	

	#if GLMDEBUG
		// create editable text item, if it does not already exist
		if (!m_editable)
		{
			char	*suffix = "";

			switch(m_type)
			{
				case	kGLMVertexProgram:		suffix = ".vsh"; break;
				case	kGLMFragmentProgram:	suffix = ".fsh"; break;
				default:	GLMDebugger();
			}

			m_editable = new CGLMEditableTextItem( m_text, strlen(m_text), false, "/debugshaders/", suffix );
			
			// pull our string back from the editable (it has probably munged it)
			if (m_editable->HasData())
			{
				ReloadStringFromEditable();
			}
		}
	#endif

	
	// scan the text and find sections
	CGLMTextSectioner		sections( m_text, strlen( m_text ), g_shaderSectionMarkers );
	
	int sectionCount = sections.Count();
	for( int i=0; i < sectionCount; i++ )
	{
		uint subtextOffset	= 0;
		uint subtextLength	= 0;
		int markerIndex		= 0;
		
		sections.GetSection( i, &subtextOffset, &subtextLength, &markerIndex );

		// act on the section
		GLMShaderDesc *desc = NULL;
		switch( m_type )
		{
			case kGLMVertexProgram:
				switch( markerIndex )
				{
					case	kGLMARBVertex:
					case	kGLMGLSLVertex:
						desc = &m_descs[ (markerIndex==kGLMARBVertex) ? kGLMARB : kGLMGLSL];

						// these steps are generic across both langs
						desc->m_textPresent	= true;
						desc->m_textOffset	= subtextOffset;
						desc->m_textLength	= subtextLength;
						desc->m_compiled	= false;
						desc->m_valid		= false;
					break;

					case	kGLMARBVertexDisabled:
					case	kGLMGLSLVertexDisabled:
						// ignore quietly
					break;
					
					default: Assert(!"Mismatched section marker seen in SetProgramText (VP)"); break;
				}
			break;
			
			case kGLMFragmentProgram:
				switch( markerIndex )
				{
					case	kGLMARBFragment:
					case	kGLMGLSLFragment:
						desc = &m_descs[ (markerIndex==kGLMARBFragment) ? kGLMARB : kGLMGLSL];

						// these steps are generic across both langs
						desc->m_textPresent	= true;
						desc->m_textOffset	= subtextOffset;
						desc->m_textLength	= subtextLength;
						desc->m_compiled	= false;
						desc->m_valid		= false;
					break;

					case	kGLMARBFragmentDisabled:
					case	kGLMGLSLFragmentDisabled:
						// ignore quietly
					break;
					
					default: Assert(!"Mismatched section marker seen in SetProgramText (VP)"); break;
				}
			break;			
		}
	}
}

bool	CGLMProgram::CompileActiveSources	( void )
{
	bool result = true;	// assume success
	
	// compile everything we have text for
	for( int i=0; i<kGLMNumProgramTypes; i++)
	{
		if (m_descs[i].m_textPresent)
		{
			if (!Compile( (EGLMProgramLang)i ))
			{
				result = false;
			}
		}
	}
	return result;
}

bool	CGLMProgram::Compile( EGLMProgramLang lang )
{
	bool result = true;	// indicating validity..
	bool noisy = false;
	int loglevel = gl_shaderpair_cachelog.GetInt();
	
	switch( lang )
	{
		case kGLMARB:
		{
			GLMShaderDesc *arbDesc;
			
			arbDesc = &m_descs[ kGLMARB ];
			
			// make sure no GLSL program is set up
			glUseProgram(0);
			GLMCheckError();
			// bind our program container to context
			GLenum arbTarget = GLMProgTypeToARBEnum( m_type );

			glSetEnable( arbTarget, true );							// unclear if I need this to compile or just to draw...
			GLMCheckError();

			glBindProgramARB( arbTarget, arbDesc->m_object.arb );	// object created or just re-bound
			GLMCheckError();

			char *section = m_text + arbDesc->m_textOffset;
			char *lastCharOfSection = section + arbDesc->m_textLength;	// actually it's one past the last textual character

			#if GLMDEBUG				
				if(noisy)
				{
					GLMPRINTF((">-D- CGLMProgram::Compile submitting following text for ARB %s program (name %d) ---------------------",
						arbTarget == GL_FRAGMENT_PROGRAM_ARB ? "fragment" : "vertex",
						arbDesc->m_object.arb ));

					// we don't have a "print this many chars" call yet
					// just temporarily null terminate the text we want to print
					
					char saveChar = *lastCharOfSection;
					
					*lastCharOfSection= 0;
					GLMPRINTTEXT(( section, eDebugDump ));
					*lastCharOfSection= saveChar;

					GLMPRINTF(("<-D- CGLMProgram::Compile ARB EOT--" ));
				}
			#endif

			glProgramStringARB( arbTarget, GL_PROGRAM_FORMAT_ASCII_ARB, arbDesc->m_textLength, section );
			GLMCheckError( true, false );			
			arbDesc->m_compiled = true;	// compiled but not necessarily valid
			
			CheckValidity( lang );
			GLMCheckError();
			// leave it bound n enabled, don't care (draw will sort it all out)

			result = arbDesc->m_valid;
		}
		break;

		case kGLMGLSL:
		{
			GLMShaderDesc *glslDesc;
			
			glslDesc = &m_descs[ kGLMGLSL ];

			GLenum glslStage = GLMProgTypeToGLSLEnum( m_type );
			
			// there's no binding to do for GLSL.  but make sure no ARB stuff is bound for tidiness.
			glSetEnable( GL_VERTEX_PROGRAM_ARB, false );
			glSetEnable( GL_FRAGMENT_PROGRAM_ARB, false );	// add check errors on these
			
			glBindProgramARB( GL_VERTEX_PROGRAM_ARB, 0 );
			glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, 0 );

			// no GLSL program either
			glUseProgram(0);
			
			// pump text into GLSL shader object

			char *section = m_text + glslDesc->m_textOffset;
			char *lastCharOfSection = section + glslDesc->m_textLength;	// actually it's one past the last textual character

			#if GLMDEBUG
				if(noisy)
				{
					GLMPRINTF((">-D- CGLMProgram::Compile submitting following text for GLSL %s program (name %d) ---------------------",
						glslStage == GL_FRAGMENT_SHADER_ARB ? "fragment" : "vertex",
						glslDesc->m_object.glsl ));

					// we don't have a "print this many chars" call yet
					// just temporarily null terminate the text we want to print
					
					char saveChar = *lastCharOfSection;
					
					*lastCharOfSection= 0;
					GLMPRINTTEXT(( section, eDebugDump ));
					*lastCharOfSection= saveChar;

					GLMPRINTF(("<-D- CGLMProgram::Compile GLSL EOT--" ));
				}
			#endif

			glShaderSourceARB( glslDesc->m_object.glsl, 1, (const GLchar **)&section, &glslDesc->m_textLength);	
			GLMCheckError( true, false );			

			// compile
			glCompileShaderARB( glslDesc->m_object.glsl );
			glslDesc->m_compiled = true;	// compiled but not necessarily valid
			GLMCheckError( true, false );			

			CheckValidity( lang );
			GLMCheckError();			

			if (loglevel>=2)
			{
				char tempname[128];
				int tempindex = -1;
				int tempcombo = -1;

				//GetLabelIndexCombo( tempname, sizeof(tempname), &tempindex, &tempcombo );
				//printf("\ncompile: - [ %s/%d/%d ] on GL name %d ", tempname, tempindex, tempcombo, glslDesc->m_object.glsl );
				

				GetComboIndexNameString( tempname, sizeof(tempname) );
				printf("\ncompile: %s on GL name %d ", tempname, glslDesc->m_object.glsl );
			}

			result = glslDesc->m_valid;
		}
		break;
	}
	return result;
}

#if GLMDEBUG

	bool CGLMProgram::PollForChanges( void )
	{
		bool result = false;
		if (m_editable)
		{
			result = m_editable->PollForChanges();
		}
		return result;
	}

	void	CGLMProgram::ReloadStringFromEditable( void )
	{
		uint	dataSize=0;
		char	*data=NULL;
		
		m_editable->GetCurrentText( &data, &dataSize );
		
		char *buf = (char *)malloc( dataSize+1 );	// we will NULL terminate it, since the mirror copy might not be
		memcpy( buf, data, dataSize );
		buf[dataSize] = 0;
		
		SetProgramText( buf );
		
		free( buf );
	}

	bool	CGLMProgram::SyncWithEditable( void )
	{
		bool result = false;
		
		if (m_editable->PollForChanges())
		{
			ReloadStringFromEditable();

			CompileActiveSources();
			
			// invalidate shader pair cache entries using this shader..
			m_ctx->m_pairCache->PurgePairsWithShader( this );
			
			result = true;	// result true means "it changed"
		}
		return result;
	}
	
#endif


// attributes which are general to both stages
//	VP and FP:
//	
//	0x88A0         PROGRAM_INSTRUCTIONS_ARB                         VP  FP
//	0x88A1         MAX_PROGRAM_INSTRUCTIONS_ARB                     VP  FP
//	0x88A2         PROGRAM_NATIVE_INSTRUCTIONS_ARB                  VP  FP
//	0x88A3         MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB              VP  FP
//	
//	0x88A4         PROGRAM_TEMPORARIES_ARB                          VP  FP
//	0x88A5         MAX_PROGRAM_TEMPORARIES_ARB                      VP  FP
//	0x88A6         PROGRAM_NATIVE_TEMPORARIES_ARB                   VP  FP
//	0x88A7         MAX_PROGRAM_NATIVE_TEMPORARIES_ARB               VP  FP
//	
//	0x88A8         PROGRAM_PARAMETERS_ARB                           VP  FP
//	0x88A9         MAX_PROGRAM_PARAMETERS_ARB                       VP  FP
//	0x88AA         PROGRAM_NATIVE_PARAMETERS_ARB                    VP  FP
//	0x88AB         MAX_PROGRAM_NATIVE_PARAMETERS_ARB                VP  FP
//	
//	0x88AC         PROGRAM_ATTRIBS_ARB                              VP  FP
//	0x88AD         MAX_PROGRAM_ATTRIBS_ARB                          VP  FP
//	0x88AE         PROGRAM_NATIVE_ATTRIBS_ARB                       VP  FP
//	0x88AF         MAX_PROGRAM_NATIVE_ATTRIBS_ARB                   VP  FP
//	
//	0x88B4         MAX_PROGRAM_LOCAL_PARAMETERS_ARB                 VP  FP
//	0x88B5         MAX_PROGRAM_ENV_PARAMETERS_ARB                   VP  FP
//	0x88B6         PROGRAM_UNDER_NATIVE_LIMITS_ARB                  VP  FP
//
//	VP only:
//	
//	0x88B0         PROGRAM_ADDRESS_REGISTERS_ARB                    VP
//	0x88B1         MAX_PROGRAM_ADDRESS_REGISTERS_ARB                VP
//	0x88B2         PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB             VP
//	0x88B3         MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB         VP
//	
//	FP only:
//	
//	0x8805         PROGRAM_ALU_INSTRUCTIONS_ARB                         FP
//	0x880B         MAX_PROGRAM_ALU_INSTRUCTIONS_ARB                     FP
//	0x8808         PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB                  FP
//	0x880E         MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB              FP

//	0x8806         PROGRAM_TEX_INSTRUCTIONS_ARB                         FP
//	0x880C         MAX_PROGRAM_TEX_INSTRUCTIONS_ARB                     FP
//	0x8809         PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB                  FP
//	0x880F         MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB              FP

//	0x8807         PROGRAM_TEX_INDIRECTIONS_ARB                         FP
//	0x880D         MAX_PROGRAM_TEX_INDIRECTIONS_ARB                     FP
//	0x880A         PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB                  FP
//	0x8810         MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB              FP

struct GLMShaderLimitDesc
{
	GLenum	m_valueEnum;
	GLenum	m_limitEnum;
	const char	*m_debugName;
	char	m_flags;
	// m_flags - 0x01 for VP, 0x02 for FP, or set both if applicable to both
};

// macro to help make the table of what to check
#ifndef	LMD( val, limit, flags )
#define	LMD( val, flags )	{ GL_PROGRAM_##val##_ARB, GL_MAX_PROGRAM_##val##_ARB, #val, flags }
#else
#error you need to use a different name for this macro.
#endif

GLMShaderLimitDesc	g_glmShaderLimitDescs[] = 
{
	// VP and FP..
	LMD( INSTRUCTIONS,				3 ),
	LMD( NATIVE_INSTRUCTIONS,		3 ),
	LMD( NATIVE_TEMPORARIES,		3 ),
	LMD( PARAMETERS,				3 ),
	LMD( NATIVE_PARAMETERS,			3 ),
	LMD( ATTRIBS,					3 ),
	LMD( NATIVE_ATTRIBS,			3 ),

	// VP only..
	LMD( ADDRESS_REGISTERS,			1 ),
	LMD( NATIVE_ADDRESS_REGISTERS,	1 ),

	// FP only..
	LMD( ALU_INSTRUCTIONS,			2 ),
	LMD( NATIVE_ALU_INSTRUCTIONS,	2 ),
	LMD( TEX_INSTRUCTIONS,			2 ),
	LMD( NATIVE_TEX_INSTRUCTIONS,	2 ),
	LMD( TEX_INDIRECTIONS,			2 ),
	LMD( NATIVE_TEX_INDIRECTIONS,	2 ),
	
	{ 0, 0, NULL, 0 }
};

#undef LMD

bool CGLMProgram::CheckValidity( EGLMProgramLang lang )
{
	static char *targnames[] = { "vertex", "fragment" };

	switch(lang)
	{
		case kGLMARB:
		{
			GLMShaderDesc *arbDesc;
			arbDesc = &m_descs[ kGLMARB ];
			
			GLenum arbTarget = GLMProgTypeToARBEnum( m_type );

			Assert( arbDesc->m_compiled );
			
			arbDesc->m_valid = true;	// assume success til we see otherwise

			// assume program is bound.  is there anything wrong with it ?

			GLint isNative=0;
			glGetProgramivARB( arbTarget, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &isNative );
			GLMCheckError();
			
			// If the program is over the hardware's limits, print out some information
			if (isNative!=1)
			{
				arbDesc->m_valid = false;
				
				// check everything we can check
				char checkmask = (1<<m_type);	// 1 for VP, 2 for FP
				
				for( GLMShaderLimitDesc *desc = g_glmShaderLimitDescs; desc->m_valueEnum !=0; desc++ )
				{
					if ( desc->m_flags & checkmask )
					{
						// test it
						GLint value = 0;
						GLint limit = 0;
						glGetProgramivARB(arbTarget, desc->m_valueEnum, &value);
						GLMCheckError();
						
						glGetProgramivARB(arbTarget, desc->m_limitEnum, &limit);
						GLMCheckError();
						
						if (value > limit)
						{
							GLMPRINTF(("-D- Invalid %s program: program has %d %s; limit is %d", targnames[ m_type ], value, desc->m_debugName, limit ));
						}
					}
				}
			}

			// syntax error check
			GLint errorLine;
			glGetIntegerv( GL_PROGRAM_ERROR_POSITION_ARB, &errorLine );
			GLMCheckError();

			if ( errorLine!=-1 )
			{
				const GLubyte* errorString = glGetString(GL_PROGRAM_ERROR_STRING_ARB);
				GLMPRINTF(( "-D- Syntax error in ARB %s program: %s",targnames[ m_type ], errorString ));
				arbDesc->m_valid = false;
			}
			if (!arbDesc->m_valid)
			{
				char *temp = strdup(m_text);
				temp[ arbDesc->m_textOffset + arbDesc->m_textLength ] = 0;
				GLMPRINTF(("-D- ----- ARB compile failed; bad source follows -----" ));
				GLMPRINTTEXT(( temp + arbDesc->m_textOffset, eDebugDump, GLMPRINTTEXT_NUMBEREDLINES ));
				GLMPRINTF(("-D- -----end-----" ));
				free( temp );
			}
			
			return arbDesc->m_valid;
		}
		break;
		
		case kGLMGLSL:
		{
			GLMShaderDesc *glslDesc;			
			glslDesc = &m_descs[ kGLMGLSL ];
			
			GLenum glslStage = GLMProgTypeToGLSLEnum( m_type );

			Assert( glslDesc->m_compiled );
			
			glslDesc->m_valid = true;	// assume success til we see otherwise

			// GLSL error check
			int compiled = 0, length = 0, laux = 0;

			glGetObjectParameterivARB( (GLhandleARB)glslDesc->m_object.glsl, GL_OBJECT_COMPILE_STATUS_ARB, &compiled);
			glGetObjectParameterivARB( (GLhandleARB)glslDesc->m_object.glsl, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length);
			GLcharARB *logString = (GLcharARB *)malloc(length * sizeof(GLcharARB));
			glGetInfoLogARB((GLhandleARB)glslDesc->m_object.glsl, length, &laux, logString);	
			
			// we may not be able to check "native limits" stuff until link time. meh

			if (!compiled)
			{
				glslDesc->m_valid = false;
			}
			
			if (!glslDesc->m_valid)
			{
				char *temp = strdup(m_text);
				temp[ glslDesc->m_textOffset + glslDesc->m_textLength ] = 0;
				GLMPRINTF(("-D- ----- GLSL compile failed: \n %s \n",logString ));
				GLMPRINTTEXT(( temp + glslDesc->m_textOffset, eDebugDump, GLMPRINTTEXT_NUMBEREDLINES ));
				GLMPRINTF(("-D- -----end-----" ));
				free( temp );
			}
			
			free( logString );

			return glslDesc->m_valid;
		}
		break;
	}
}

void	CGLMProgram::LogSlow( EGLMProgramLang lang )
{
	// find the desc, see if it's marked
	GLMShaderDesc *desc = &m_descs[ lang ];

	if (!desc->m_slowMark)
	{
		// log it
		printf(	"\n-------------- Slow %s ( CGLMProgram @ %08x, lang %s, name %d ) : \n%s \n",
				m_type==kGLMVertexProgram ? "VS" : "FS",
				this,
				lang==kGLMGLSL ? "GLSL" : "ARB",
				(int)(lang==kGLMGLSL ? (int)desc->m_object.glsl : (int)desc->m_object.arb),
				m_text
		);
	}
	else	// complain on a decreasing basis (powers of two)
	{
		if ( (desc->m_slowMark & (desc->m_slowMark-1)) == 0 )
		{
			// short blurb
			printf(	"\n               Slow %s ( CGLMProgram @ %08x, lang %s, name %d ) (%d times)",
					m_type==kGLMVertexProgram ? "VS" : "FS",
					this,
					lang==kGLMGLSL ? "GLSL" : "ARB",
					(int)(lang==kGLMGLSL ? (int)desc->m_object.glsl : (int)desc->m_object.arb),
					desc->m_slowMark+1
			);
		}
	}

	// mark it
	desc->m_slowMark++;
		

}

void	CGLMProgram::GetLabelIndexCombo		( char *labelOut, int labelOutMaxChars, int *indexOut, int *comboOut )
{
	// find the label string
	// example:
	// trans#2871 label:vs-file vertexlit_and_unlit_generic_vs20 vs-index 294912 vs-combo 1234
	
	*labelOut = 0;
	*indexOut = -1;
	
	char *lineStr = strstr( m_text, "// trans#" );
	if (lineStr)
	{
		char	temp1[1024];
		int		temp2,temp3;
		int		scratch=-1;
		temp1[0] = 0;
		temp2 = -1;
		temp3 = -1;
		
		if (this->m_type==kGLMVertexProgram)
		{
			sscanf( lineStr, "// trans#%d label:vs-file %s vs-index %d vs-combo %d", &scratch, temp1, &temp2, &temp3 );
		}
		else
		{
			sscanf( lineStr, "// trans#%d label:ps-file %s ps-index %d ps-combo %d", &scratch, temp1, &temp2, &temp3 );
		}

		if ( (strlen(temp1)!=0) )
		{
			strncpy( labelOut, temp1, labelOutMaxChars );
			*indexOut = temp2;
			*comboOut = temp3;
		}
	}
}

void	CGLMProgram::GetComboIndexNameString	( char *stringOut, int stringOutMaxChars )		// mmmmmmmm-nnnnnnnn-filename
{
	// find the label string
	// example:
	// trans#2871 label:vs-file vertexlit_and_unlit_generic_vs20 vs-index 294912 vs-combo 1234
	
	*stringOut = 0;
	
	char *lineStr = strstr( m_text, "// trans#" );
	if (lineStr)
	{
		char	temp1[1024];
		int		temp2,temp3;
		int		scratch=-1;

		temp1[0] = 0;
		temp2 = -1;
		temp3 = -1;
		
		if (this->m_type==kGLMVertexProgram)
		{
			sscanf( lineStr, "// trans#%d label:vs-file %s vs-index %d vs-combo %d", &scratch, temp1, &temp2, &temp3 );
		}
		else
		{
			sscanf( lineStr, "// trans#%d label:ps-file %s ps-index %d ps-combo %d", &scratch, temp1, &temp2, &temp3 );
		}

		int len = strlen(temp1);
		
		if ( (len+20) < stringOutMaxChars )
		{
			// output formatted version
			sprintf( stringOut, "%08X-%08X-%s", temp3, temp2, temp1 );
		}
	}
}

//===============================================================================


CGLMShaderPair::CGLMShaderPair( GLMContext *ctx  )
{
	m_ctx = ctx;
	m_ctx->MakeCurrent();
	
	m_vertexProg = m_fragmentProg = NULL;

	m_program = glCreateProgramObjectARB();
	GLMCheckError();

	m_locVertexParams = -1;
	m_locVertexInteger0 = -1;	// "i0"
	m_locVertexBool0 = -1;		// "b0"
	m_locVertexBool1 = -1;		// "b1"
	m_locVertexBool2 = -1;		// "b2"
	m_locVertexBool3 = -1;		// "b3"
	
	m_locFragmentParams = -1;
	m_locFragmentFakeSRGBEnable = -1;
	m_fakeSRGBEnableValue = -1.0f;
	
	memset( m_locSamplers, 0xFF, sizeof( m_locSamplers ) );
	
	m_valid = false;
	m_samplersFixed = false;	// fix them at draw time, and only do it once.
	m_revision = 0;				// bumps to 1 once linked
}

CGLMShaderPair::~CGLMShaderPair( )
{
	if (m_program)
	{
		glDeleteObjectARB( (GLhandleARB)m_program );
		m_program = 0;
	}
}

bool	CGLMShaderPair::SetProgramPair			( CGLMProgram *vp, CGLMProgram *fp )
{
	m_valid	= false;			// assume failure
	
	// true result means successful link and query
	bool vpgood = (vp!=NULL) && (vp->m_descs[ kGLMGLSL ].m_valid);
	bool fpgood = (fp!=NULL) && (fp->m_descs[ kGLMGLSL ].m_valid);
	
	if (!fpgood)
	{
		// fragment side allowed to be "null".
		fp = m_ctx->m_nullFragmentProgram;
	}

	if (vpgood && fpgood)
	{
		// attempt link. but first, detach any previously attached programs
		if (m_vertexProg)
		{
			glDetachObjectARB(m_program, m_vertexProg->m_descs[kGLMGLSL].m_object.glsl);
			GLMCheckError();
			m_vertexProg = NULL;			
		}
		
		if (m_fragmentProg)
		{
			glDetachObjectARB(m_program, m_fragmentProg->m_descs[kGLMGLSL].m_object.glsl);
			GLMCheckError();
			m_fragmentProg = NULL;			
		}
		
		// now attach
		
		glAttachObjectARB(m_program, vp->m_descs[kGLMGLSL].m_object.glsl);
		m_vertexProg = vp;
		GLMCheckError();

		glAttachObjectARB(m_program, fp->m_descs[kGLMGLSL].m_object.glsl);
		m_fragmentProg = fp;
		GLMCheckError();
	
		// force the locations for input attributes v0-vN to be at locations 0-N
		// use the vertex attrib map to know which slots are live or not... oy!  we don't have that map yet... but it's OK.
		// fallback - just force v0-v15 to land in locations 0-15 as a standard.
		
		if (vp->m_descs[kGLMGLSL].m_valid)
		{
			for( int i=0; i < 16; i++)
			{
				char tmp[16];
				sprintf(tmp, "v%d", i);	// v0 v1 v2 ... et al
				
				glBindAttribLocationARB( m_program, i, tmp );
				GLMCheckError();			
			}
		}
			
		// now link
		glLinkProgramARB( m_program );
		GLMCheckError();			

		// check for success
		GLint result = 0;
		glGetObjectParameterivARB(m_program,GL_OBJECT_LINK_STATUS_ARB,&result);	// want GL_TRUE
		
		if (result == GL_TRUE)
		{
			// success
			
			m_valid	= true;
			m_revision++;
		}
		else
		{
			GLint length = 0;
			GLint laux = 0;
			
			// do some digging
			glGetObjectParameterivARB(m_program,GL_OBJECT_INFO_LOG_LENGTH_ARB,&length);

			GLcharARB *logString = (GLcharARB *)malloc(length * sizeof(GLcharARB));
			glGetInfoLogARB(m_program, length, &laux, logString);	

			char *vtemp = strdup(vp->m_text);
			vtemp[ vp->m_descs[kGLMGLSL].m_textOffset + vp->m_descs[kGLMGLSL].m_textLength ] = 0;
			
			char *ftemp = strdup(fp->m_text);
			ftemp[ fp->m_descs[kGLMGLSL].m_textOffset + fp->m_descs[kGLMGLSL].m_textLength ] = 0;
			
			GLMPRINTF(("-D- ----- GLSL link failed: \n %s ",logString ));

			GLMPRINTF(("-D- ----- GLSL vertex program selected: %08x (handle %08x)", vp, vp->m_descs[kGLMGLSL].m_object.glsl ));
			GLMPRINTTEXT(( vtemp + vp->m_descs[kGLMGLSL].m_textOffset, eDebugDump, GLMPRINTTEXT_NUMBEREDLINES ));
			
			GLMPRINTF(("-D- ----- GLSL fragment program selected: %08x (handle %08x)", fp, vp->m_descs[kGLMGLSL].m_object.glsl ));
			GLMPRINTTEXT(( ftemp + fp->m_descs[kGLMGLSL].m_textOffset, eDebugDump, GLMPRINTTEXT_NUMBEREDLINES ));
			
			GLMPRINTF(("-D- -----end-----" ));

			free( ftemp );
			free( vtemp );
			free( logString );
		}
	}
	else
	{
		// fail
		Assert(!"Can't link these programs");
	}
	
	if (m_valid)
	{
		m_locVertexParams = glGetUniformLocationARB( m_program, "vc");
		GLMCheckError();

		m_locVertexInteger0 = glGetUniformLocationARB( m_program, "i0");
		GLMCheckError();

		m_locVertexBool0 = glGetUniformLocationARB( m_program, "b0");
		GLMCheckError();
		m_locVertexBool1 = glGetUniformLocationARB( m_program, "b1");
		GLMCheckError();
		m_locVertexBool2 = glGetUniformLocationARB( m_program, "b2");
		GLMCheckError();
		m_locVertexBool3 = glGetUniformLocationARB( m_program, "b3");
		GLMCheckError();

 		m_locFragmentParams = glGetUniformLocationARB( m_program, "pc");
		GLMCheckError();

 		m_locFragmentFakeSRGBEnable = glGetUniformLocationARB( m_program, "flSRGBWrite");
		GLMCheckError();
		m_fakeSRGBEnableValue = -1.0f;
		
		for( int sampler=0; sampler<16; sampler++)
		{
			char tmp[16];
			sprintf(tmp, "sampler%d", sampler);	// sampler0 .. sampler1.. etc
			
			m_locSamplers[sampler] = glGetUniformLocationARB( m_program, tmp );
			GLMCheckError();
		}
	}
	else
	{
		m_locVertexParams = -1;

		m_locVertexInteger0 = -1;
		m_locVertexBool0 = -1;
		m_locVertexBool1 = -1;
		m_locVertexBool2 = -1;
		m_locVertexBool3 = -1;

		m_locFragmentParams = -1;
		m_locFragmentFakeSRGBEnable = -1;
		m_fakeSRGBEnableValue = -999;
		
		memset( m_locSamplers, 0xFF, sizeof( m_locSamplers ) );
		
		m_revision = 0;		
	}

	return m_valid;
}


bool	CGLMShaderPair::RefreshProgramPair		( void )
{
	// re-link and re-query the uniforms.
	
	// since SetProgramPair knows how to detach previously attached shader objects, just pass the same ones in again.
	CGLMProgram	*vp = m_vertexProg;
	CGLMProgram	*fp = m_fragmentProg;
	
	bool vpgood = (vp!=NULL) && (vp->m_descs[ kGLMGLSL ].m_valid);
	bool fpgood = (fp!=NULL) && (fp->m_descs[ kGLMGLSL ].m_valid);

	if (vpgood && fpgood)
	{
		SetProgramPair( vp, fp );
	}
	else
	{
		Debugger();
	}
}


//===============================================================================

CGLMShaderPairCache::CGLMShaderPairCache( GLMContext *ctx  )
{
	m_ctx = ctx;
	
	m_mark = 1;
	
	m_rowsLg2 = gl_shaderpair_cacherows_lg2.GetInt();
	if (m_rowsLg2 < 10)
			m_rowsLg2 = 10;
	m_rows = 1<<m_rowsLg2;

	m_waysLg2 = gl_shaderpair_cacheways_lg2.GetInt();
	if (m_waysLg2 < 5)
		m_waysLg2 = 5;
	m_ways = 1<<m_waysLg2;

	m_entryCount = m_rows * m_ways;
	
	uint entryTableSize = m_rows * m_ways * sizeof(CGLMPairCacheEntry);
	m_entries = (CGLMPairCacheEntry*)malloc( entryTableSize );				// array[ m_rows ][ m_ways ]
	memset( m_entries, 0, entryTableSize );
	
	uint evictTableSize = m_rows * sizeof(uint);
	m_evictions = (uint*)malloc( evictTableSize );
	memset (m_evictions, 0, evictTableSize);

	// hit counter table is same size
	m_hits = (uint*)malloc( evictTableSize );
	memset (m_hits, 0, evictTableSize);
}

CGLMShaderPairCache::~CGLMShaderPairCache( )
{
	if (gl_shaderpair_cachelog.GetInt())
	{
		DumpStats();
	}

	// free all the built pairs
	// free the entry table
	bool purgeResult = this->Purge();
	Assert( !purgeResult );
	
	if (m_entries)
	{
		free( m_entries );
		m_entries = NULL;
	}

	if (m_evictions)
	{
		free( m_evictions );
		m_evictions = NULL;
	}

	if (m_hits)
	{
		free( m_hits );
		m_hits = NULL;
	}
}

// Set this convar internally to build or add to the shader pair cache file (link hints)
// We really only expect this to work on POSIX
static ConVar glm_cacheprograms( "glm_cacheprograms", "0", FCVAR_DEVELOPMENTONLY );

#define PROGRAM_CACHE_FILE "program_cache.cfg"

static void WriteToProgramCache( CGLMShaderPair *pair )
{
	KeyValues *pProgramCache = new KeyValues( "programcache" );
	pProgramCache->LoadFromFile( g_pFullFileSystem, PROGRAM_CACHE_FILE, "MOD" );

	if ( !pProgramCache )
	{
		Warning( "Could not write to program cache file!\n" );
		return;
	}

	// extract values of interest which represent a pair of shaders
	
	char	vprogramName[128];
	int		vprogramStaticIndex = -1;
	int		vprogramDynamicIndex = -1;
	pair->m_vertexProg->GetLabelIndexCombo( vprogramName, sizeof(vprogramName), &vprogramStaticIndex, &vprogramDynamicIndex );

	
	char	pprogramName[128];
	int		pprogramStaticIndex = -1;
	int		pprogramDynamicIndex = -1;
	pair->m_fragmentProg->GetLabelIndexCombo( pprogramName, sizeof(pprogramName), &pprogramStaticIndex, &pprogramDynamicIndex );

	// make up a key - this thing is really a list of tuples, so need not be keyed by anything particular
	KeyValues *pProgramKey = pProgramCache->CreateNewKey();
	Assert( pProgramKey );

	pProgramKey->SetString	( "vs", vprogramName );
	pProgramKey->SetString	( "ps", pprogramName );

	pProgramKey->SetInt		( "vs_static", vprogramStaticIndex );
	pProgramKey->SetInt		( "ps_static", pprogramStaticIndex );

	pProgramKey->SetInt		( "vs_dynamic", vprogramDynamicIndex );
	pProgramKey->SetInt		( "ps_dynamic", pprogramDynamicIndex );

	pProgramCache->SaveToFile( g_pFullFileSystem, PROGRAM_CACHE_FILE, "MOD" );
	pProgramCache->deleteThis();
}


CGLMShaderPair	*CGLMShaderPairCache::SelectShaderPair( CGLMProgram *vp, CGLMProgram *fp, uint extraKeyBits )
{
	CGLMShaderPair	*result = NULL;

	int loglevel = gl_shaderpair_cachelog.GetInt();
	char vtempname[128];
	int vtempindex = -1;
	int vtempcombo = -1;

	char ptempname[128];
	int ptempindex = -1;
	int ptempcombo = -1;
		
	
	// select row where pair would be found if it exists
	uint rowIndex = HashRowIndex( vp, fp, extraKeyBits );
	
	CGLMPairCacheEntry *row = HashRowPtr( rowIndex );
	
	// probe row and see if we get a hit
	int hitway = -1;int emptyway = -1; int oldestway = -1;

	HashRowProbe( row, vp, fp, extraKeyBits, &hitway, &emptyway, &oldestway );
	
	if (hitway >=0)
	{
		// found it.  mark it and return
		CGLMPairCacheEntry *hit = row + hitway;
		hit->m_lastMark = m_mark;
		
		m_mark = m_mark+1;
		if (!m_mark)		// somewhat unlikely this will ever be reached.. but we need to avoid zero as a mark value
		{
			m_mark = 1;
		}
		
		// count the hit
		m_hits[ rowIndex ] ++;

		if (loglevel >= 3)  // hits logged at level 3 and higher
		{
			printf("\nSSP: hit  - row %05d - pair $%08x (%d'th hit on row)",rowIndex, hit->m_pair, m_hits[ rowIndex ] );
		}

		result = hit->m_pair;
	}
	else
	{
		// we missed.  if there is no empty way, then somebody's getting evicted.
		int destway = -1;
		
		if (emptyway>=0)
		{			
			destway = emptyway;

			if (loglevel >= 2)  // misses logged at level 3 and higher
			{
				printf("\nSSP: miss - row %05d - ", rowIndex );
			}
		}
		else
		{
			// evict the oldest way
			Assert( oldestway >= 0);	// better not come back negative
			
			CGLMPairCacheEntry *evict = row + oldestway;
			
			Assert( evict->m_pair != NULL );
			Assert( evict->m_pair != m_ctx->m_boundPair );	// just check
			
			///////////////////////FIXME may need to do a shoot-down if the pair being evicted is currently active in the context
			
			m_evictions[ rowIndex ]++;

			// log eviction if desired
			if (loglevel >= 2)  // misses logged at level 3 and higher
			{
				//evict->m_vertexProg->GetLabelIndexCombo( vtempname, sizeof(vtempname), &vtempindex, &vtempcombo );
				//evict->m_fragmentProg->GetLabelIndexCombo( ptempname, sizeof(ptempname), &ptempindex, &ptempcombo );
				//printf("\nSSP: miss - row %05d - [ %s/%d/%d %s/%d/%d ]'s %d'th eviction - ", rowIndex, vtempname, vtempindex, vtempcombo, ptempname, ptempindex, ptempcombo, m_evictions[ rowIndex ] );

				evict->m_vertexProg->GetComboIndexNameString( vtempname, sizeof(vtempname) );
				evict->m_fragmentProg->GetComboIndexNameString( ptempname, sizeof(ptempname) );				
				printf("\nSSP: miss - row %05d - [ %s + %s ]'s %d'th eviction - ", rowIndex, vtempname, ptempname, m_evictions[ rowIndex ] );
			}

			delete evict->m_pair;	evict->m_pair = NULL;
			memset( evict, 0, sizeof(*evict) );
			
			destway = oldestway;
		}

		// make the new entry
		CGLMPairCacheEntry *newentry = row + destway;
		
		newentry->m_lastMark = m_mark;
		newentry->m_vertexProg = vp;
		newentry->m_fragmentProg = fp;
		newentry->m_extraKeyBits = extraKeyBits;
		newentry->m_pair = new CGLMShaderPair( m_ctx );
		newentry->m_pair->SetProgramPair( vp, fp );

		if (loglevel >= 2)  // say a little bit more
		{
			//newentry->m_vertexProg->GetLabelIndexCombo( vtempname, sizeof(vtempname), &vtempindex, &vtempcombo );
			//newentry->m_fragmentProg->GetLabelIndexCombo( ptempname, sizeof(ptempname), &ptempindex, &ptempcombo );			
			//printf("new [ %s/%d/%d %s/%d/%d ]", vtempname, vtempindex, vtempcombo, ptempname, ptempindex, ptempcombo );

			newentry->m_vertexProg->GetComboIndexNameString( vtempname, sizeof(vtempname) );
			newentry->m_fragmentProg->GetComboIndexNameString( ptempname, sizeof(ptempname) );				
			printf("new [ %s + %s ]", vtempname, ptempname );
		}

		m_mark = m_mark+1;
		if (!m_mark)		// somewhat unlikely this will ever be reached.. but we need to avoid zero as a mark value
		{
			m_mark = 1;
		}

		result = newentry->m_pair;

		if (glm_cacheprograms.GetInt())
		{
			WriteToProgramCache( newentry->m_pair );
		}
	}
	
	return result;
}

void	CGLMShaderPairCache::QueryShaderPair( int index, GLMShaderPairInfo *infoOut )
{
	if ( (index<0) || ( index >= (m_rows*m_ways) ) )
	{
		// no such location
		memset( infoOut, sizeof(*infoOut), 0 );
		
		infoOut->m_status = -1;
	}
	else
	{
		// locate the entry, and see if an active pair is present.
		// if so, extract info and return with m_status=1.
		// if not, exit with m_status = 0.

		CGLMPairCacheEntry *entry = &m_entries[index];
		
		if (entry->m_pair)
		{
			// live
			// extract values of interest for caller

			entry->m_pair->m_vertexProg->GetLabelIndexCombo		( infoOut->m_vsName, sizeof(infoOut->m_vsName), &infoOut->m_vsStaticIndex, &infoOut->m_vsDynamicIndex );
			entry->m_pair->m_fragmentProg->GetLabelIndexCombo	( infoOut->m_psName, sizeof(infoOut->m_psName), &infoOut->m_psStaticIndex, &infoOut->m_psDynamicIndex );

			infoOut->m_status = 1;
		}
		else
		{
			// not
			memset( infoOut, sizeof(*infoOut), 0 );
			infoOut->m_status = 0;
		}
	}
}

bool	CGLMShaderPairCache::PurgePairsWithShader( CGLMProgram *prog )
{
	bool result = false;
	
	// walk all rows*ways
	int limit = m_rows * m_ways;
	for( int i=0; i < limit; i++)
	{
		CGLMPairCacheEntry *entry = &m_entries[i];
		
		if (entry->m_pair)
		{
			//scrub it, if not currently bound, and if the supplied shader matches either stage
			if ( (entry->m_vertexProg==prog) || (entry->m_fragmentProg==prog) )
			{
				// found it, but does it conflict with bound pair ?
				if (entry->m_pair == m_ctx->m_boundPair)
				{
					m_ctx->m_boundPair = NULL;
				}
				delete entry->m_pair;
				memset( entry, 0, sizeof(*entry) );
			}
		}
	}
	return result;
}

bool			CGLMShaderPairCache::Purge( void )
{
	bool result = false;
	
	// walk all rows*ways
	int limit = m_rows * m_ways;
	for( int i=0; i < limit; i++)
	{
		CGLMPairCacheEntry *entry = &m_entries[i];
		
		if (entry->m_pair)
		{
			//scrub it, unless the pair is the currently bound pair in our parent glm context
			if (entry->m_pair != m_ctx->m_boundPair)
			{
				delete entry->m_pair;
				memset( entry, 0, sizeof(*entry) );
			}
			else
			{
				result = true;
			}
		}
	}
	return result;
}
	
void			CGLMShaderPairCache::DumpStats			( void )
{
	printf("\n------------------\npair cache stats");
	int total = 0;
	for( int row=0; row < m_rows; row++ )
	{
		if ( (m_evictions[row] != 0) || (m_hits[row] != 0) )
		{
			printf("\n row %d : %d evictions, %d hits",row,m_evictions[row], m_hits[row]);
			total += m_evictions[row];
		}
	}
	printf("\n\npair cache evictions: %d\n-----------------------\n",total );
}
	
	//===============================

uint			CGLMShaderPairCache::HashRowIndex		( CGLMProgram *vp, CGLMProgram *fp, uint extraKeyBits )
{
	// calculate row index for this pair
	uint vp_hash = vp->m_serial * 47;
	uint fp_hash = fp->m_serial;
	
	uint hash_row_index = ((vp_hash * fp_hash) + (vp_hash ^ fp_hash) + (extraKeyBits * 7) ) & (m_rows-1);
	
	return hash_row_index;
}

CGLMPairCacheEntry*	CGLMShaderPairCache::HashRowPtr		( uint hashRowIndex )
{
	return &m_entries[ hashRowIndex * m_ways ];
}

void			CGLMShaderPairCache::HashRowProbe		( CGLMPairCacheEntry *row, CGLMProgram *vp, CGLMProgram *fp, uint extraKeyBits, int *hitwayOut, int *emptywayOut, int *oldestwayOut )
{
	// scan this row to see if the desired pair is present
	CGLMPairCacheEntry *cursor = row;
	int hitway = -1;
	int emptyway = -1;
	int oldestway = -1;
	long long oldestmark = 0xFFFFFFFFFFFFFFFFLL;
	
	for( int way = 0; (way < m_ways) && (hitway<0); way++)
	{
		if (cursor->m_lastMark != 0)	// occupied slot
		{
			if ( (cursor->m_vertexProg == vp) && (cursor->m_fragmentProg == fp) && (cursor->m_extraKeyBits == extraKeyBits) )	// match?
			{
				// found it
				hitway = way;
			}
			
			// check if this is the oldest one on the row - only occupied slots are checked
			if (cursor->m_lastMark < oldestmark)
			{
				oldestway = way;
				oldestmark = cursor->m_lastMark;
			}
		}
		else
		{
			// empty way, log it if first one seen
			if (emptyway<0)
			{
				emptyway = way;
			}
		}
		cursor++;
	}

	if (hitwayOut)		*hitwayOut = hitway;
	if (emptywayOut)	*emptywayOut = emptyway;
	if (oldestwayOut)	*oldestwayOut = oldestway;
}

