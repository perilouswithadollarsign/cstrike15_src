//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// cglmbuffer.cpp
//
//===============================================================================

#include "glmgr/glmgr.h"
#include "glmgr/cglmbuffer.h"

// memdbgon -must- be the last include file in a .cpp file.
#include "tier0/memdbgon.h"

//    void BindBufferARB(enum target, uint buffer);
//    void DeleteBuffersARB(sizei n, const uint *buffers);
//    void GenBuffersARB(sizei n, uint *buffers);
//    boolean IsBufferARB(uint buffer);
//
//    void BufferDataARB(enum target, sizeiptrARB size, const void *data,
//                       enum usage);
//    void BufferSubDataARB(enum target, intptrARB offset, sizeiptrARB size,
//                          const void *data);
//    void GetBufferSubDataARB(enum target, intptrARB offset,
//                             sizeiptrARB size, void *data);
//
//    void *MapBufferARB(enum target, enum access);
//    boolean UnmapBufferARB(enum target);
//
//    void GetBufferParameterivARB(enum target, enum pname, int *params);
//    void GetBufferPointervARB(enum target, enum pname, void **params);
//
//New Tokens
//
//    Accepted by the <target> parameters of BindBufferARB, BufferDataARB,
//    BufferSubDataARB, MapBufferARB, UnmapBufferARB,
//    GetBufferSubDataARB, GetBufferParameterivARB, and
//    GetBufferPointervARB:
//
//        ARRAY_BUFFER_ARB                             0x8892
//        ELEMENT_ARRAY_BUFFER_ARB                     0x8893
//
//    Accepted by the <pname> parameter of GetBooleanv, GetIntegerv,
//    GetFloatv, and GetDoublev:
//
//        ARRAY_BUFFER_BINDING_ARB                     0x8894
//        ELEMENT_ARRAY_BUFFER_BINDING_ARB             0x8895
//        VERTEX_ARRAY_BUFFER_BINDING_ARB              0x8896
//        NORMAL_ARRAY_BUFFER_BINDING_ARB              0x8897
//        COLOR_ARRAY_BUFFER_BINDING_ARB               0x8898
//        INDEX_ARRAY_BUFFER_BINDING_ARB               0x8899
//        TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB       0x889A
//        EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB           0x889B
//        SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB     0x889C
//        FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB      0x889D
//        WEIGHT_ARRAY_BUFFER_BINDING_ARB              0x889E
//
//    Accepted by the <pname> parameter of GetVertexAttribivARB:
//
//        VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB       0x889F
//
//    Accepted by the <usage> parameter of BufferDataARB:
//
//        STREAM_DRAW_ARB                              0x88E0
//        STREAM_READ_ARB                              0x88E1
//        STREAM_COPY_ARB                              0x88E2
//        STATIC_DRAW_ARB                              0x88E4
//        STATIC_READ_ARB                              0x88E5
//        STATIC_COPY_ARB                              0x88E6
//        DYNAMIC_DRAW_ARB                             0x88E8
//        DYNAMIC_READ_ARB                             0x88E9
//        DYNAMIC_COPY_ARB                             0x88EA
//
//    Accepted by the <access> parameter of MapBufferARB:
//
//        READ_ONLY_ARB                                0x88B8
//        WRITE_ONLY_ARB                               0x88B9
//        READ_WRITE_ARB                               0x88BA
//
//    Accepted by the <pname> parameter of GetBufferParameterivARB:
//
//        BUFFER_SIZE_ARB                              0x8764
//        BUFFER_USAGE_ARB                             0x8765
//        BUFFER_ACCESS_ARB                            0x88BB
//        BUFFER_MAPPED_ARB                            0x88BC
//
//    Accepted by the <pname> parameter of GetBufferPointervARB:
//
//        BUFFER_MAP_POINTER_ARB                       0x88BD

// http://www.opengl.org/registry/specs/ARB/pixel_buffer_object.txt
//    Accepted by the <target> parameters of BindBuffer, BufferData,
//    BufferSubData, MapBuffer, UnmapBuffer, GetBufferSubData,
//    GetBufferParameteriv, and GetBufferPointerv:
//        PIXEL_PACK_BUFFER_ARB                        0x88EB
//        PIXEL_UNPACK_BUFFER_ARB                      0x88EC


	// gl_bufmode: zero means we mark all vertex/index buffers static

	// non zero means buffers are initially marked static..
	// ->but can shift to dynamic upon first 'discard' (orphaning)

ConVar	gl_bufmode( "gl_bufmode", "1" );

CGLMBuffer::CGLMBuffer( GLMContext *ctx, EGLMBufferType type, uint size, uint options )
{
	m_ctx = ctx;
	m_type = type;
	switch(m_type)
	{
		case	kGLMVertexBuffer:		m_buffGLTarget = GL_ARRAY_BUFFER_ARB; break;
		case	kGLMIndexBuffer:		m_buffGLTarget = GL_ELEMENT_ARRAY_BUFFER_ARB; break;
		case	kGLMUniformBuffer:		m_buffGLTarget = GL_UNIFORM_BUFFER_EXT; break;
		case	kGLMPixelBuffer:		m_buffGLTarget = GL_PIXEL_UNPACK_BUFFER_ARB; break;
		
		default:	Assert(!"Unknown buffer type" );
	}
	m_size = size;
	m_bound = false;
	m_mapped = false;
	m_lastMappedAddress = NULL;

	m_enableAsyncMap = false;
	m_enableExplicitFlush = false;
	m_dirtyMinOffset = m_dirtyMaxOffset = 0;								// adjust/grow on lock, clear on unlock

	m_ctx->CheckCurrent();
	m_revision = rand();

	// make a decision about pseudo mode
	// this looked like it didn't help much or was actually slower, so leave it available but only as opt-in.
	// a more clever implementation would be able to select pseudo buf storage for small batches only..
	m_pseudo = (m_type==kGLMIndexBuffer) && (CommandLine()->FindParm("-gl_enable_pseudobufs"));	
	if (m_pseudo)
	{
		m_name		= 0;		
		m_pseudoBuf	= (char*)malloc( size );
		
		m_ctx->BindBufferToCtx( m_type, NULL );		// exit with no buffer bound
	}
	else
	{
		glGenBuffersARB( 1, &m_name );
		GLMCheckError();

		m_ctx->BindBufferToCtx( m_type, this );	// causes glBindBufferARB

		// buffers start out static, but if they get orphaned and gl_bufmode is non zero,
		// then they will get flipped to dynamic.
		
		GLenum hint = GL_STATIC_DRAW_ARB;
		switch(m_type)
		{
			case	kGLMVertexBuffer:		hint = (options & GLMBufferOptionDynamic) ? GL_DYNAMIC_DRAW_ARB : GL_STATIC_DRAW_ARB; break;
			case	kGLMIndexBuffer:		hint = (options & GLMBufferOptionDynamic) ? GL_DYNAMIC_DRAW_ARB : GL_STATIC_DRAW_ARB; break;
			case	kGLMUniformBuffer:		hint = GL_DYNAMIC_DRAW_ARB; break;	// "fwiw" - shrug
			case	kGLMPixelBuffer:		hint = (options & GLMBufferOptionDynamic) ? GL_DYNAMIC_DRAW_ARB : GL_STATIC_DRAW_ARB; break;
			
			default:	Assert(!"Unknown buffer type" );
		}

		glBufferDataARB( m_buffGLTarget, m_size, NULL, hint );	// may ultimately need more hints to set the usage correctly (esp for streaming)

		this->SetModes( true, true, true );

		m_ctx->BindBufferToCtx( m_type, NULL );	// unbind me
	}
}

CGLMBuffer::~CGLMBuffer( )
{
	m_ctx->CheckCurrent();
	
	if (m_pseudo)
	{
		free (m_pseudoBuf);
		m_pseudoBuf = NULL;
	}
	else
	{
		glDeleteBuffersARB( 1, &m_name );
		GLMCheckError();
	}
	
	m_ctx = NULL;
	m_name = 0;
	m_bound = 0;
	
	m_lastMappedAddress = NULL;
}

void	CGLMBuffer::SetModes			( bool asyncMap, bool explicitFlush, bool force )
{
	// assumes buffer is bound. called by constructor and by Lock.

	if (m_pseudo)
	{
		// ignore it...
	}
	else
	{
		if (force || (m_enableAsyncMap != asyncMap) )
		{
			// note the sense of the parameter, it's TRUE if you *want* serialization, so for async you turn it to false.
			glBufferParameteriAPPLE( this->m_buffGLTarget, GL_BUFFER_SERIALIZED_MODIFY_APPLE, asyncMap==false );
			m_enableAsyncMap = asyncMap;
		}

		if (force || (m_enableExplicitFlush != explicitFlush) )
		{
			// note the sense of the parameter, it's TRUE if you *want* auto-flush-on-unmap, so for explicit-flush, you turn it to false.
			glBufferParameteriAPPLE( this->m_buffGLTarget, GL_BUFFER_FLUSHING_UNMAP_APPLE, explicitFlush==false );
			m_enableExplicitFlush = explicitFlush;
		}
	}
}

void	CGLMBuffer::FlushRange			( uint offset, uint size )
{
	if (m_pseudo)
	{
		// nothing to do
	}
	else
	{
		// assumes buffer is bound.
		glFlushMappedBufferRangeAPPLE(this->m_buffGLTarget, (GLintptr)offset, (GLsizeiptr)size);
	}
}
	
ConVar	gl_buffer_alignment_quantum		( "gl_buffer_alignment_quantum", "32" );		// the alignment we use pre-SLGU
ConVar	gl_buffer_alignment_quantum_slgu( "gl_buffer_alignment_quantum_slgu", "2" );	// alignment used post-SLGU

void	CGLMBuffer::Lock( GLMBuffLockParams *params, char **addressOut )
{
	char	*resultPtr = NULL;
	
	Assert( !m_mapped);
	
	m_ctx->CheckCurrent();
	GLMCheckError();

	if (params->m_offset >= m_size)
		Debugger();
	
	if (params->m_offset + params->m_size > m_size)
		Debugger();

	// bind (yes, even for pseudo - this binds name 0)
	m_ctx->BindBufferToCtx( this->m_type, this );
	
	if (m_pseudo)
	{
		// discard is a no-op

		// async map modes are a no-op

		// latch last mapped address (silly..)
		m_lastMappedAddress = (float*)m_pseudoBuf;

		// calc lock address
		resultPtr = m_pseudoBuf + params->m_offset;
		
		// dirty range is a no-op
	}
	else
	{
		// perform discard if requested
		if (params->m_discard)
		{
			// observe gl_bufmode on any orphan event.
			// if orphaned and bufmode is nonzero, flip it to dynamic.
			GLenum hint = gl_bufmode.GetInt() ? GL_DYNAMIC_DRAW_ARB : GL_STATIC_DRAW_ARB;
			glBufferDataARB( m_buffGLTarget, m_size, NULL, hint );
			
			m_lastMappedAddress = NULL;
			
			m_revision++;	// revision grows on orphan event
		}

		// adjust async map option appropriately, leave explicit flush unchanged
		this->SetModes( params->m_nonblocking, m_enableExplicitFlush );

		// map
		char *mapPtr = (char*)glMapBufferARB( this->m_buffGLTarget, GL_READ_WRITE_ARB );
		
		if (!mapPtr)
		{
			Debugger();
		}
		
		if (m_lastMappedAddress)
		{
			// just check if it moved
			Assert (m_lastMappedAddress == (float*)mapPtr);
		}

		m_lastMappedAddress = (float*)mapPtr;

		// calculate offset location
		resultPtr = mapPtr + params->m_offset;

		// adjust dirty range
		if (m_dirtyMinOffset != m_dirtyMaxOffset)
		{
			// grow range
			m_dirtyMinOffset = MIN( m_dirtyMinOffset, params->m_offset );
			m_dirtyMaxOffset = MIN( m_dirtyMaxOffset, params->m_offset+params->m_size );
		}
		else
		{
			// set range
			m_dirtyMinOffset = params->m_offset;
			m_dirtyMaxOffset = params->m_offset+params->m_size;
		}

		// pad and clamp dirty range to choice of boundary
		uint quantum = (m_ctx->Caps().m_hasPerfPackage1) ? gl_buffer_alignment_quantum_slgu.GetInt() : gl_buffer_alignment_quantum.GetInt();
		uint quantum_mask = quantum - 1;

		m_dirtyMinOffset = m_dirtyMinOffset & (~quantum_mask);
		m_dirtyMaxOffset = (m_dirtyMaxOffset + quantum_mask) & (~quantum_mask);
		m_dirtyMaxOffset = MIN( m_dirtyMaxOffset, m_size );
	}

	m_mapped = true;
	
	*addressOut = resultPtr;
}

void	CGLMBuffer::Unlock( void )
{
	m_ctx->CheckCurrent();

	Assert (m_mapped);

	if (m_pseudo)
	{
		// nothing to do actually
	}
	else
	{
		m_ctx->BindBufferToCtx( this->m_type, this );

		// time to do explicit flush
		if (m_enableExplicitFlush)
		{
			this->FlushRange( m_dirtyMinOffset, m_dirtyMaxOffset - m_dirtyMinOffset );
		}
		
		// clear dirty range no matter what
		m_dirtyMinOffset = m_dirtyMaxOffset = 0;								// adjust/grow on lock, clear on unlock

		glUnmapBuffer( this->m_buffGLTarget );
		
	}

	m_mapped = false;
}
