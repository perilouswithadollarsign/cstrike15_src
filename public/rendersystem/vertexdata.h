//==== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
//===========================================================================//

#ifndef VERTEXDATA_H
#define VERTEXDATA_H

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "tier0/platform.h"
#include "rendersystem/irenderdevice.h"


//-----------------------------------------------------------------------------
// Vertex field creation
//-----------------------------------------------------------------------------
template< class T > class ALIGN16 CVertexData
{
public:
	CVertexData( IRenderContext* pRenderContext, HRenderBuffer hVertexBuffer );
	CVertexData( IRenderContext* pRenderContext, RenderBufferType_t nType, int nVertexCount, const char *pDebugName, const char *pBudgetGroup );
	~CVertexData();

	// Begins, ends modification of the vertex buffer (returns true if the lock succeeded)
	// A lock may not succeed if there isn't enough room
	bool Lock( int nMaxSizeInBytes = 0 );
	void Unlock();

	// returns the number of vertices
	int	VertexCount() const;

	// returns the total # of vertices in the entire buffer
	int GetBufferVertexCount() const;

	// Call this to move forward a vertex
	void AdvanceVertex();

	IRenderContext *GetRenderContext() { return m_pRenderContext; }

	// Call this to detach ownership of the vertex buffer. Caller is responsible
	// for deleting it now
	HRenderBuffer TakeOwnership();

	// Allows us to iterate on this algorithm at a later date
	FORCEINLINE T* operator->() { return &m_Scratch; }
	FORCEINLINE const T* operator->() const { return &m_Scratch; }

protected:
	enum
	{
		BUFFER_OFFSET_UNINITIALIZED = 0xFFFFFFFF
	};

	void Release();

	// The temporary memory we're writing into
	T		m_Scratch;

	// The mesh we're modifying
	T*		m_pMemory;

	// The current vertex
	int						m_nVertexCount;

	// Amount to increase the vertex count each time (0 if there was a lock failure)
	int						m_nVertexIncrement;

	IRenderContext*			m_pRenderContext;
	HRenderBuffer			m_hVertexBuffer;
	int						m_nMaxVertexCount;
	int						m_nBufferVertexCount : 31;
	int						m_bShouldDeallocate : 1;
};


//-----------------------------------------------------------------------------
//
// Inline methods related to CVertexData
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
template< class T >
inline CVertexData<T>::CVertexData( IRenderContext* pRenderContext, HRenderBuffer hVertexBuffer )
{
	m_pRenderContext = pRenderContext;
	m_hVertexBuffer = hVertexBuffer;
	m_bShouldDeallocate = false;

	BufferDesc_t desc;
	pRenderContext->GetDevice()->GetVertexBufferDesc( hVertexBuffer, &desc );
	m_nBufferVertexCount = desc.m_nElementCount;

#ifdef _DEBUG
	// Initialize the vertex fields to NAN
	memset( &m_Scratch, 0xFF, sizeof(m_Scratch) );
	m_nVertexCount = 0;
	m_nMaxVertexCount = 0;
	m_pMemory = NULL;
	m_nVertexIncrement = 0;
#endif
}

template< class T >
inline CVertexData<T>::CVertexData( IRenderContext* pRenderContext, RenderBufferType_t nType, int nVertexCount, const char *pDebugName, const char *pBudgetGroup )
{
	m_pRenderContext = pRenderContext;

	BufferDesc_t vertexDesc;
	vertexDesc.m_nElementSizeInBytes = sizeof(T);
	vertexDesc.m_nElementCount = nVertexCount;
	vertexDesc.m_pDebugName = pDebugName;
	vertexDesc.m_pBudgetGroupName = pBudgetGroup;
	m_hVertexBuffer = pRenderContext->GetDevice()->CreateVertexBuffer( nType, vertexDesc );
	m_nBufferVertexCount = nVertexCount;
	m_bShouldDeallocate = true;

	ResourceAddRef( m_hVertexBuffer );

#ifdef _DEBUG
	// Initialize the vertex fields to NAN
	memset( &m_Scratch, 0xFF, sizeof(m_Scratch) );
	m_nVertexCount = 0;
	m_nMaxVertexCount = 0;
	m_pMemory = NULL;
	m_nVertexIncrement = 0;
#endif
}

template< class T >
inline CVertexData<T>::~CVertexData()
{
	// If this assertion fails, you forgot to unlock
	Assert( !m_pMemory );
	Release();
}


//-----------------------------------------------------------------------------
// Release
//-----------------------------------------------------------------------------
template< class T >
void CVertexData<T>::Release()
{
	if ( m_bShouldDeallocate && ( m_hVertexBuffer != RENDER_BUFFER_HANDLE_INVALID ) )
	{
		ResourceRelease( m_hVertexBuffer );
		m_pRenderContext->GetDevice()->DestroyVertexBuffer( m_hVertexBuffer );
		m_hVertexBuffer = RENDER_BUFFER_HANDLE_INVALID;
		m_bShouldDeallocate = false;
	}
}


//-----------------------------------------------------------------------------
// Call this to take ownership of the vertex buffer
//-----------------------------------------------------------------------------
template< class T >
inline HRenderBuffer CVertexData<T>::TakeOwnership()
{
	if ( m_bShouldDeallocate )
	{
		ResourceRelease( m_hVertexBuffer );
	}
	m_bShouldDeallocate = false;
	return m_hVertexBuffer;
}


//-----------------------------------------------------------------------------
// Returns the buffer vertex count
//-----------------------------------------------------------------------------
template< class T >
inline int CVertexData<T>::GetBufferVertexCount() const
{
	return m_nBufferVertexCount;
}


//-----------------------------------------------------------------------------
// Begins, ends modification of the vertex buffer
//-----------------------------------------------------------------------------
template< class T >
inline bool CVertexData<T>::Lock( int nMaxVertexCount )
{
	if ( nMaxVertexCount == 0 )
	{
		nMaxVertexCount = m_nBufferVertexCount;
	}

	// Lock the vertex buffer
	LockDesc_t desc;
	bool bOk = m_pRenderContext->LockVertexBuffer( m_hVertexBuffer, nMaxVertexCount * sizeof(T), &desc );
	m_nVertexIncrement = bOk ? 1 : 0;
	m_nMaxVertexCount = nMaxVertexCount * m_nVertexIncrement;
	m_nVertexCount = 0;
	m_pMemory = (T*)desc.m_pMemory;
	return bOk;
}

template< class T >
inline void CVertexData<T>::Unlock()
{
	LockDesc_t desc;
	desc.m_pMemory = m_pMemory;
	m_pRenderContext->UnlockVertexBuffer( m_hVertexBuffer, m_nVertexCount * sizeof(T), &desc );

#ifdef _DEBUG
	m_nVertexCount = 0;
	m_nMaxVertexCount = 0;
	m_pMemory = 0;
	m_nVertexIncrement = 0;
#endif
}


//-----------------------------------------------------------------------------
// returns the number of vertices
//-----------------------------------------------------------------------------
template< class T >
inline int CVertexData<T>::VertexCount() const
{
	return m_nVertexCount;
}


//-----------------------------------------------------------------------------
// NOTE: This version is the one you really want to achieve write-combining;
// Write combining only works if you write in 4 bytes chunks.
//-----------------------------------------------------------------------------
template< class T >
inline void CVertexData<T>::AdvanceVertex()
{
	Assert( ( m_nVertexIncrement == 0 ) || ( m_nVertexCount < m_nMaxVertexCount ) );

	T *pDest = &m_pMemory[ m_nVertexCount ];
	T *pSrc = &m_Scratch;

#if defined( COMPILER_MSVC32 )
	if ( sizeof(T) == 16 )
	{
		__asm
		{
			mov esi, pSrc
			mov edi, pDest

			movaps xmm0, [esi + 0]
			movntps [edi + 0], xmm0
		}
	}
	else if ( sizeof(T) == 32 )
	{
		__asm
		{
			mov esi, pSrc
			mov edi, pDest

			movaps xmm0, [esi + 0]
			movaps xmm1, [esi + 16]

			movntps [edi + 0], xmm0
			movntps [edi + 16], xmm1
		}
	}
	else if ( sizeof(T) == 48 )
	{
		__asm
		{
			mov esi, pSrc
			mov edi, pDest

			movaps xmm0, [esi + 0]
			movaps xmm1, [esi + 16]
			movaps xmm2, [esi + 32]

			movntps [edi + 0], xmm0
			movntps [edi + 16], xmm1
			movntps [edi + 32], xmm2
		}
	}
	else if ( sizeof(T) == 64 )
	{
		__asm
		{
			mov esi, pSrc
			mov edi, pDest

			movaps xmm0, [esi + 0]
			movaps xmm1, [esi + 16]
			movaps xmm2, [esi + 32]
			movaps xmm3, [esi + 48]

			movntps [edi + 0], xmm0
			movntps [edi + 16], xmm1
			movntps [edi + 32], xmm2
			movntps [edi + 48], xmm3
		}
	}
	else
#elif defined ( PLATFORM_X360 )
	if ( sizeof(T) == 16 )
	{
		__vector4 v4Read = __lvx( pSrc, 0 );
		__stvx( v4Read, pDest, 0 );
	}
	else if ( sizeof(T) == 32 )
	{
		__vector4 v4Read0 = __lvx( pSrc, 0 );
		__vector4 v4Read1 = __lvx( pSrc, 16 );
		__stvx( v4Read0, pDest, 0 );
		__stvx( v4Read1, pDest, 16 );
	}
	else if ( sizeof(T) == 48 )
	{
		__vector4 v4Read0 = __lvx( pSrc, 0 );
		__vector4 v4Read1 = __lvx( pSrc, 16 );
		__vector4 v4Read2 = __lvx( pSrc, 32 );
		__stvx( v4Read0, pDest, 0 );
		__stvx( v4Read1, pDest, 16 );
		__stvx( v4Read2, pDest, 32 );
	}
	else if ( sizeof(T) == 64 )
	{
		__vector4 v4Read0 = __lvx( pSrc, 0 );
		__vector4 v4Read1 = __lvx( pSrc, 16 );
		__vector4 v4Read2 = __lvx( pSrc, 32 );
		__vector4 v4Read3 = __lvx( pSrc, 48 );
		__stvx( v4Read0, pDest, 0 );
		__stvx( v4Read1, pDest, 16 );
		__stvx( v4Read2, pDest, 32 );
		__stvx( v4Read3, pDest, 48 );
	}
	else			 
#endif					    
		*pDest = *pSrc;
	m_nVertexCount += m_nVertexIncrement;
}


//-----------------------------------------------------------------------------
// Dynamic vertex field creation
// NOTE: Draw call must occur prior to destruction of this class!
//-----------------------------------------------------------------------------
enum VertexDataStrideType_t
{
	VD_STRIDE_ZERO = 0,
	VD_STRIDE_DEFAULT,
};

template< class T > class ALIGN16 CDynamicVertexData : public CVertexData< T >
{
	typedef CVertexData< T > BaseClass;

public:
	CDynamicVertexData( IRenderContext* pRenderContext, int nVertexCount, const char *pDebugName, const char *pBudgetGroup );
	~CDynamicVertexData();
	void Release();

	// Begins, ends modification of the vertex buffer (returns true if the lock succeeded)
	// A lock may not succeed if there isn't enough room
	bool Lock( );

	// Binds the vb to a particular slot using a particular offset
	void Bind( int nSlot, int nOffset, VertexDataStrideType_t nStride = VD_STRIDE_DEFAULT );
};


//-----------------------------------------------------------------------------
//
// Inline methods related to CDynamicVertexData
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
template< class T >
inline CDynamicVertexData<T>::CDynamicVertexData( IRenderContext* pRenderContext, int nVertexCount, const char *pDebugName, const char *pBudgetGroup ) :
	BaseClass( pRenderContext, RENDER_BUFFER_HANDLE_INVALID )
{
	BufferDesc_t vertexDesc;
	vertexDesc.m_nElementSizeInBytes = sizeof(T);
	vertexDesc.m_nElementCount = nVertexCount;
	vertexDesc.m_pDebugName = pDebugName;
	vertexDesc.m_pBudgetGroupName = pBudgetGroup;
	this->m_hVertexBuffer = pRenderContext->CreateDynamicVertexBuffer( vertexDesc );
	this->m_nBufferVertexCount = nVertexCount;

	ResourceAddRef( this->m_hVertexBuffer );
}

template< class T >
inline CDynamicVertexData<T>::~CDynamicVertexData()
{
	Release();
}


//-----------------------------------------------------------------------------
// Release
//-----------------------------------------------------------------------------
template< class T >
void CDynamicVertexData<T>::Release()
{
	if ( this->m_hVertexBuffer != RENDER_BUFFER_HANDLE_INVALID )
	{
		this->m_pRenderContext->DestroyDynamicVertexBuffer( this->m_hVertexBuffer );
		ResourceRelease( this->m_hVertexBuffer );
		this->m_hVertexBuffer = RENDER_BUFFER_HANDLE_INVALID;
	}
}


//-----------------------------------------------------------------------------
// Begins, ends modification of the buffer
//-----------------------------------------------------------------------------
template< class T >
inline bool CDynamicVertexData<T>::Lock( )
{
	Assert( this->m_hVertexBuffer != RENDER_BUFFER_HANDLE_INVALID );
	return BaseClass::Lock( );
}


//-----------------------------------------------------------------------------
// Binds the vb to a particular stream using a particular offset
//-----------------------------------------------------------------------------
template< class T >
inline void CDynamicVertexData<T>::Bind( int nSlot, int nOffset, VertexDataStrideType_t nStrideType )
{
	int nStride = ( nStrideType == VD_STRIDE_DEFAULT ) ? sizeof( T ) : 0;
	this->m_pRenderContext->BindVertexBuffer( nSlot, this->m_hVertexBuffer, nOffset, nStride );
}


#endif // VERTEXDATA_H
