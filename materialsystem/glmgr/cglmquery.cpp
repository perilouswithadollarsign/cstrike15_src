//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// cglmquery.cpp
//
//===============================================================================

#include "glmgr/glmgr.h"
#include "glmgr/cglmquery.h"
#include "../shaderapidx9/dxabstract.h"

#include <unistd.h>

// memdbgon -must- be the last include file in a .cpp file.
#include "tier0/memdbgon.h"

//===============================================================================

extern ConVar gl_errorcheckall;
extern ConVar gl_errorcheckqueries;
extern ConVar gl_errorchecknone;

// how many microseconds to wait after a failed query-available test
// presently on MTGL this doesn't happen, but it could change, keep this handy
ConVar  gl_nullqueries( "gl_nullqueries", "0" );

GLenum GetQueryError( void )
{
	if ( ( GLMDEBUG || (gl_errorcheckall.GetInt() != 0)  || (gl_errorcheckqueries.GetInt() != 0) ) && (gl_errorchecknone.GetInt() == 0) )
	{
		return glGetError();
	}
	else
	{
		return (GLenum) 0;	// whistle past graveyard
	}
}

//===============================================================================

CGLMQuery::CGLMQuery( GLMContext *ctx, GLMQueryParams *params )
{
	// make sure context is current
	// get the type of query requested
	// generate name(s) needed
	// set initial state appropriately
	ctx->MakeCurrent();
	
	m_ctx = ctx;
	m_params = *params;

	m_name				=	0;

	m_started = m_stopped = m_done = false;
	
	m_nullQuery = false;
		// assume value of convar at start time
		// does not change during individual query lifetime
		// started null = stays null
		// started live = stays live
	
	switch(m_params.m_type)
	{
		case EOcclusion:
		{
			//make an occlusion query (and a fence to go with it)
			glGenQueriesARB( 1, &m_name );
			GLMPRINTF(("-A-      CGLMQuery(OQ) created name %d", m_name));

			GLenum errorcode = GetQueryError();
			if (errorcode)
			{
				const char *decodedStr = GLMDecode( eGL_ERROR, errorcode );
				printf( "\nCGLMQuery::CGLMQuery (OQ) saw %s error (%d) from glGenQueriesARB", decodedStr, errorcode  );
				m_name = 0;
			}			
		}
		break;

		case EFence:
			//make a fence - no aux fence needed
			glGenFencesAPPLE(1, &m_name );
			GLMPRINTF(("-A-      CGLMQuery(fence) created name %d", m_name));

			GLenum errorcode = GetQueryError();
			if (errorcode)
			{
				const char *decodedStr = GLMDecode( eGL_ERROR, errorcode );
				printf( "\nCGLMQuery::CGLMQuery (fence) saw %s error (%d) from glGenFencesAPPLE", decodedStr, errorcode  );
				m_name = 0;
			}
		break;
	}
	
}

CGLMQuery::~CGLMQuery()
{
	GLMPRINTF(("-A-> ~CGLMQuery"));
	
	// make sure query has completed (might not be necessary)
	// delete the name(s)

	m_ctx->MakeCurrent();

	switch(m_params.m_type)
	{
		case EOcclusion:
		{
			// do a finish occlusion query ?
			GLMPRINTF(("-A-      ~CGLMQuery(OQ) deleting name %d", m_name));
			glDeleteQueries(1, &m_name );

			GLenum errorcode = GetQueryError();
			if (errorcode)
			{
				const char *decodedStr = GLMDecode( eGL_ERROR, errorcode );
				printf( "\nCGLMQuery::~CGLMQuery (OQ) saw %s error (%d) from glDeleteQueries", decodedStr, errorcode  );
			}
		}
		break;

		case EFence:
		{
			// do a finish fence ?
			GLMPRINTF(("-A-      ~CGLMQuery(fence) deleting name %d", m_name));
			glDeleteFencesAPPLE(1, &m_name );

			GLenum errorcode = GetQueryError();
			if (errorcode)
			{
				const char *decodedStr = GLMDecode( eGL_ERROR, errorcode );
				printf( "\nCGLMQuery::~CGLMQuery (fence) saw %s error (%d) from glDeleteFencesAPPLE", decodedStr, errorcode  );
			}
		}
		break;
	}
	
	m_name = 0;

	GLMPRINTF(("-A-< ~CGLMQuery"));
}




void	CGLMQuery::Start( void )		// "start counting"
{
	m_ctx->MakeCurrent();

	// on occlusion query:
	//	glBeginQueryARB on the OQ name. counting starts.
	
	// on fence: glSetFence on m_name.
	
	// note, fences finish themselves via command progress - OQ's do not.
	
	Assert(!m_started);
	Assert(!m_stopped);
	Assert(!m_done);
	
	m_nullQuery = (gl_nullqueries.GetInt() != 0);	// latch value for remainder of query life
	
	switch(m_params.m_type)
	{
		case EOcclusion:
		{
			if (m_nullQuery)
			{
				// do nothing..
			}
			else
			{
				glBeginQueryARB( GL_SAMPLES_PASSED_ARB, m_name );
				GLenum errorcode = GetQueryError();
				if (errorcode)
				{
					const char *decodedStr = GLMDecode( eGL_ERROR, errorcode );
					printf( "\nCGLMQuery::Start(OQ) saw %s error (%d) from glBeginQueryARB (GL_SAMPLES_PASSED_ARB) name=%d", decodedStr, errorcode, m_name  );
				}
			}
		}
		break;

		case EFence:
			glSetFenceAPPLE( m_name );

			GLenum errorcode = GetQueryError();
			if (errorcode)
			{
				const char *decodedStr = GLMDecode( eGL_ERROR, errorcode );
				printf( "\nCGLMQuery::Start(fence) saw %s error (%d) from glSetFenceAPPLE name=%d", decodedStr, errorcode, m_name  );
			}
			
			m_stopped = true;	// caller should not call Stop on a fence, it self-stops
		break;
	}
	
	m_started = true;
}

void CGLMQuery::Stop( void )			// "stop counting"
{
	m_ctx->MakeCurrent();

	Assert(m_started);
	Assert(!m_stopped);					// this will assert if you try to call Stop on a fence that is started
	Assert(!m_done);
	
	switch(m_params.m_type)
	{
		case EOcclusion:
		{
			if (m_nullQuery)
			{
				// do nothing..
			}
			else
			{
				glEndQueryARB( GL_SAMPLES_PASSED_ARB );	// we are only putting the request-to-stop-counting into the cmd stream.

				GLenum errorcode = GetQueryError();
				if (errorcode)
				{
					const char *decodedStr = GLMDecode( eGL_ERROR, errorcode );
					printf( "\nCGLMQuery::Stop(OQ) saw %s error (%d) from glEndQueryARB( GL_SAMPLES_PASSED_ARB ) name=%d", decodedStr, errorcode, m_name  );
				}
			}
		}
		break;

		case EFence:
			// nop - you don't "end" a fence, you just test it and/or finish it out in Complete
		break;
	}
	
	m_stopped = true;
}

bool	CGLMQuery::IsDone( void )
{
	m_ctx->MakeCurrent();

	Assert(m_started);
	Assert(m_stopped);

	if(!m_done)		// you can ask more than once, but we only check until it comes back as done.
	{
		// on occlusion: glGetQueryObjectivARB - large cost on pre SLGU, cheap after
		// on fence: glTestFenceAPPLE on the fence
		switch(m_params.m_type)
		{
			case EOcclusion:	// just test the fence that was set after the query begin
			{
				if (m_nullQuery)
				{
					// do almost nothing.. but claim work is complete
					m_done = true;
				}
				else
				{
					// prepare to pay a big price on drivers prior to 10.6.4+SLGU
					
					GLint available = 0;
					glGetQueryObjectivARB(m_name, GL_QUERY_RESULT_AVAILABLE_ARB, &available );
					
					GLenum errorcode = GetQueryError();
					if (errorcode)
					{
						const char *decodedStr = GLMDecode( eGL_ERROR, errorcode );
						printf( "\nCGLMQuery::IsDone saw %s error (%d) from glGetQueryObjectivARB(a2) name=%d", decodedStr, errorcode, m_name  );
					}

					m_done = (available != 0);					
				}
			}
			break;

			case EFence:
			{
				m_done = glTestFenceAPPLE( m_name );
				GLenum errorcode = GetQueryError();
				if (errorcode)
				{
					const char *decodedStr = GLMDecode( eGL_ERROR, errorcode );
					printf( "\nCGLMQuery::IsDone saw %s error (%d) from glTestFenceAPPLE(b) name=%d", decodedStr, errorcode, m_name  );
				}

				if (m_done)
				{
					glFinishFenceAPPLE( m_name );	// no set fence goes un-finished

					errorcode = GetQueryError();
					if (errorcode)
					{
						const char *decodedStr = GLMDecode( eGL_ERROR, errorcode );
						printf( "\nCGLMQuery::IsDone saw %s error (%d) from glFinishFenceAPPLE(b) name=%d", decodedStr, errorcode, m_name  );
					}
				}
			}
			break;
		}
	}
	
	return m_done;
}

void	CGLMQuery::Complete( uint *result )
{
	m_ctx->MakeCurrent();

	uint resultval = 0;
	GLint available = 0;
	bool bogus_available = false;
	
	// blocking call if not done
	Assert(m_started);
	Assert(m_stopped);

	switch(m_params.m_type)
	{
		case EOcclusion:
		{
			if (m_nullQuery)
			{
				m_done = true;
				resultval = 0;		// we did say "null queries..."
			}
			else
			{
				// accept that the query is going to drain pipe in 10.6.4 and prior.
				// check the error on the spot.
				glGetQueryObjectivARB(m_name, GL_QUERY_RESULT_AVAILABLE_ARB, &available );
				GLenum errorcode = GetQueryError();
				
				if (errorcode)
				{
					const char *decodedStr = GLMDecode( eGL_ERROR, errorcode );
					printf( "\nCGLMQuery::Complete saw %s error (%d) from glGetQueryObjectivARB GL_QUERY_RESULT_AVAILABLE_ARB name=%d", decodedStr, errorcode, m_name  );

					resultval=0;
				}
				else
				{
					if (!available)
					{
						// this does happen with some very modest frequency.
						if (!m_ctx->Caps().m_hasPerfPackage1)
						{
							glFlush();		// ISTR some deadlock cases on pre-SLGU drivers if you didn't do this to kick the queue along..
						}
					}
					
					glGetQueryObjectuivARB( m_name, GL_QUERY_RESULT_ARB, &resultval);
					
					GLenum errorcode = GetQueryError();
					if (errorcode)
					{
						const char *decodedStr = GLMDecode( eGL_ERROR, errorcode );
						printf( "\nCGLMQuery::Complete saw %s error (%d) from glGetQueryObjectivARB GL_QUERY_RESULT_ARB name=%d", decodedStr, errorcode, m_name  );
						
						resultval=0;
					}
					else
					{
						// resultval is legit
					}
				}
				m_done = true;
			}
		}
		break;

		case EFence:
		{
			if(!m_done)
			{
				glFinishFenceAPPLE( m_name );

				GLenum errorcode = GetQueryError();
				if (errorcode)
				{
					const char *decodedStr = GLMDecode( eGL_ERROR, errorcode );
					printf( "\nCGLMQuery::Complete saw %s error (%d) from glFinishFenceAPPLE (EFence) name=%d", decodedStr, errorcode, m_name  );
				}
				
				m_done = true;					// for clarity or if they try to Complete twice
			}
		}
		break;
	}

	Assert( m_done );
	
	// reset state for re-use - i.e. you have to call Complete if you want to re-use the object
	m_started = m_stopped = m_done = false;
	
	if (result)	// caller may pass NULL if not interested in result, for example to clear a fence
	{
		*result = resultval;
	}
}

// accessors for the started/stopped state
bool	CGLMQuery::IsStarted	( void )
{
	return m_started;
}

bool	CGLMQuery::IsStopped	( void )
{
	return m_stopped;
}

