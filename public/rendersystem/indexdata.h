//==== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
//===========================================================================//

#ifndef INDEXDATA_H
#define INDEXDATA_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"
#include "rendersystem/irendercontext.h"


//-----------------------------------------------------------------------------
//
// Helper class used to define index buffers
//
//-----------------------------------------------------------------------------
template < class T > class CIndexData
{
public:
	CIndexData( IRenderContext* pRenderContext, HRenderBuffer hIndexBuffer );
	CIndexData( IRenderContext* pRenderContext, RenderBufferType_t nType, int nIndexCount, const char *pDebugName, const char *pBudgetGroup );

	// This constructor is meant to be used with instance rendering.
	// Passing in either 0 or INT_MAX here means the client code 
	// doesn't know how many instances will be rendered.
	CIndexData( IRenderContext* pRenderContext, RenderBufferType_t nType, int nIndexCount, int nMaxInstanceCount, const char *pDebugName, const char *pBudgetGroup );
	~CIndexData();

	// Begins, ends modification of the index buffer (returns true if the lock succeeded)
	// A lock may not succeed if there isn't enough room
	// Passing in 0 locks the entire buffer
	bool Lock( int nMaxIndexCount = 0 );
	void Unlock();

	// returns the number of indices
	int	IndexCount() const;

	// returns the total # of indices in the entire buffer
	int GetBufferIndexCount() const;

	// Used to define the indices (only used if you aren't using primitives)
	void Index( T nIndex );

	// NOTE: This version is the one you really want to achieve write-combining;
	// Write combining only works if you write in 4 bytes chunks.
	void Index2( T nIndex1, T nIndex2 );

	/*
	void FastTriangle( T nStartVert );
	void FastQuad( T nStartVert );
	void FastPolygon( T nStartVert, int nEdgeCount );
	void FastPolygonList( T nStartVert, int *pVertexCount, int polygonCount );
	void FastIndexList( const T *pIndexList, T nStartVert, int indexCount );
	*/

	// Call this to detach ownership of the vertex buffer. Caller is responsible
	// for deleting it now
	HRenderBuffer TakeOwnership();

protected:
	enum
	{
		BUFFER_OFFSET_UNINITIALIZED = 0xFFFFFFFF
	};

	void Init( IRenderContext* pRenderContext, RenderBufferType_t nType, int nIndexCount, int nMaxInstanceCount, const char *pDebugName, const char *pBudgetGroup );
	void Release();

	// Pointer to the memory we're writing
	T*				m_pMemory;

	// The current index
	int				m_nIndexCount;

	// Amount to increase the index count each time (0 if there was a lock failure)
	int				m_nIndexIncrement;

	// The mesh we're modifying
	IRenderContext*	m_pRenderContext;
	HRenderBuffer	m_hIndexBuffer;

	// Max number of indices
	int				m_nMaxIndexCount;
	int				m_nBufferIndexCount : 31;
	int				m_bShouldDeallocate : 1;
};


//-----------------------------------------------------------------------------
//
// Inline methods related to CIndexData
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
template< class T >
inline CIndexData<T>::CIndexData( IRenderContext* pRenderContext, HRenderBuffer hIndexBuffer )
{
	m_pRenderContext = pRenderContext;
	m_hIndexBuffer = hIndexBuffer;
	m_bShouldDeallocate = false;

	BufferDesc_t desc;
	pRenderContext->GetDevice()->GetIndexBufferDesc( hIndexBuffer, &desc );
	m_nBufferIndexCount = desc.m_nElementCount;

#ifdef _DEBUG
	m_nIndexCount = 0;
	m_nMaxIndexCount = 0;
	m_pMemory = NULL;
	m_nIndexIncrement = 0;
#endif
}

template< class T >
inline CIndexData<T>::CIndexData( IRenderContext* pRenderContext, RenderBufferType_t nType, int nIndexCount, const char *pDebugName, const char *pBudgetGroup )
{
	Init( pRenderContext, nType, nIndexCount, 0, pDebugName, pBudgetGroup );
}

template< class T >
inline CIndexData<T>::CIndexData( IRenderContext* pRenderContext, RenderBufferType_t nType, int nIndexCount, int nMaxInstanceCount, const char *pDebugName, const char *pBudgetGroup )
{
	if ( nMaxInstanceCount == 0 )
	{
		nMaxInstanceCount = INT_MAX;
	}
	Init( pRenderContext, nType, nIndexCount, nMaxInstanceCount, pDebugName, pBudgetGroup );
}

template< class T >
inline void CIndexData<T>::Init( IRenderContext* pRenderContext, RenderBufferType_t nType, 
	int nIndexCount, int nMaxInstanceCount, const char *pDebugName, const char *pBudgetGroup )
{
	m_pRenderContext = pRenderContext;

	BufferDesc_t indexDesc;
	indexDesc.m_nElementSizeInBytes = sizeof(T);
	indexDesc.m_nElementCount = nIndexCount;
	indexDesc.m_pDebugName = pDebugName;
	indexDesc.m_pBudgetGroupName = pBudgetGroup;
	m_hIndexBuffer = pRenderContext->GetDevice()->CreateIndexBuffer( nType, indexDesc, nMaxInstanceCount );
	m_nBufferIndexCount = nIndexCount;
	m_bShouldDeallocate = true;

	ResourceAddRef( m_hIndexBuffer );

#ifdef _DEBUG
	m_nIndexCount = 0;
	m_nMaxIndexCount = 0;
	m_pMemory = NULL;
	m_nIndexIncrement = 0;
#endif
}

template< class T >
inline CIndexData<T>::~CIndexData()
{
	// If this assertion fails, you forgot to unlock
	Assert( !m_pMemory );
	Release();
}


//-----------------------------------------------------------------------------
// Release
//-----------------------------------------------------------------------------
template< class T >
void CIndexData<T>::Release()
{
	if ( m_bShouldDeallocate && ( m_hIndexBuffer != RENDER_BUFFER_HANDLE_INVALID ) )
	{
		ResourceRelease( m_hIndexBuffer );
		m_pRenderContext->GetDevice()->DestroyIndexBuffer( m_hIndexBuffer );
		m_hIndexBuffer = RENDER_BUFFER_HANDLE_INVALID;
		m_bShouldDeallocate = false;
	}
}


//-----------------------------------------------------------------------------
// Call this to take ownership of the vertex buffer
//-----------------------------------------------------------------------------
template< class T >
inline HRenderBuffer CIndexData<T>::TakeOwnership()
{
	if ( m_bShouldDeallocate )
	{
		ResourceRelease( m_hIndexBuffer );
	}
	m_bShouldDeallocate = false;
	return m_hIndexBuffer;
}


//-----------------------------------------------------------------------------
// Returns the buffer vertex count
//-----------------------------------------------------------------------------
template< class T >
inline int CIndexData<T>::GetBufferIndexCount() const
{
	return m_nBufferIndexCount;
}


//-----------------------------------------------------------------------------
// Begins, ends modification of the index buffer
//-----------------------------------------------------------------------------
template< class T >
inline bool CIndexData<T>::Lock( int nMaxIndexCount )
{
	if ( nMaxIndexCount == 0 )
	{
		nMaxIndexCount = m_nBufferIndexCount;
	}

	// Lock the index buffer
	LockDesc_t desc;
	bool bOk = m_pRenderContext->LockIndexBuffer( m_hIndexBuffer, nMaxIndexCount * sizeof(T), &desc );
	m_nIndexIncrement = bOk ? 1 : 0;
	m_nMaxIndexCount = nMaxIndexCount * m_nIndexIncrement;
	m_nIndexCount = 0;
	m_pMemory = (T*)desc.m_pMemory;
	return bOk;
}

template< class T >
inline void CIndexData<T>::Unlock()
{
	LockDesc_t desc;
	desc.m_pMemory = m_pMemory;
	m_pRenderContext->UnlockIndexBuffer( m_hIndexBuffer, m_nIndexCount * sizeof(T), &desc );

#ifdef _DEBUG
	m_nIndexCount = 0;
	m_nMaxIndexCount = 0;
	m_pMemory = 0;
	m_nIndexIncrement = 0;
#endif
}

/*
//-----------------------------------------------------------------------------
// Binds this index buffer
//-----------------------------------------------------------------------------
inline void CIndexData<T>::Bind( IMatRenderContext *pContext )
{
	pContext->BindIndexBuffer( m_pIndexBuffer, 0 );
}
*/


//-----------------------------------------------------------------------------
// returns the number of indices
//-----------------------------------------------------------------------------
template< class T >
inline int CIndexData<T>::IndexCount() const
{
	return m_nIndexCount;
}


//-----------------------------------------------------------------------------
// Used to write data into the index buffer
//-----------------------------------------------------------------------------
template< class T >
inline void CIndexData<T>::Index( T nIndex )
{
	// FIXME: Should we prevent use of this with T = uint16? (write-combining)
	Assert( m_pMemory );
	Assert( ( m_nIndexIncrement == 0 ) || ( m_nIndexCount < m_nMaxIndexCount ) );
	m_pMemory[m_nIndexCount] = nIndex;
	m_nIndexCount += m_nIndexIncrement;
}


//-----------------------------------------------------------------------------
// NOTE: This version is the one you really want to achieve write-combining;
// Write combining only works if you write in 4 bytes chunks.
//-----------------------------------------------------------------------------
template< >
inline void CIndexData<uint16>::Index2( uint16 nIndex1, uint16 nIndex2 )
{
	Assert( m_pMemory );
	Assert( ( m_nIndexIncrement == 0 ) || ( m_nIndexCount < m_nMaxIndexCount - 1 ) );

#ifndef _X360
	uint32 nIndices = ( (uint32)nIndex1 ) | ( ( (uint32)nIndex2 ) << 16 );
#else
	uint32 nIndices = ( (uint32)nIndex2 ) | ( ( (uint32)nIndex1 ) << 16 );
#endif

	*(uint32*)( &m_pMemory[m_nIndexCount] ) = nIndices;
	m_nIndexCount += m_nIndexIncrement + m_nIndexIncrement;
}

template< >
inline void CIndexData<uint32>::Index2( uint32 nIndex1, uint32 nIndex2 )
{
	Assert( m_pMemory );
	Assert( ( m_nIndexIncrement == 0 ) || ( m_nIndexCount < m_nMaxIndexCount - 1 ) );

	m_pMemory[m_nIndexCount] = nIndex1;
	m_pMemory[m_nIndexCount+1] = nIndex2;
	m_nIndexCount += m_nIndexIncrement + m_nIndexIncrement;
}

template< class T >
inline void CIndexData<T>::Index2( T nIndex1, T nIndex2 )
{
	COMPILE_TIME_ASSERT( 0 );
}


#if 0
template< class T >
inline void CIndexData<T>::FastTriangle( int startVert )
{
	startVert += m_nIndexOffset;
	m_pIndices[m_nCurrentIndex+0] = startVert;
	m_pIndices[m_nCurrentIndex+1] = startVert + 1;
	m_pIndices[m_nCurrentIndex+2] = startVert + 2;

	AdvanceIndices(3);
}

template< class T >
inline void CIndexData<T>::FastQuad( int startVert )
{
	startVert += m_nIndexOffset;
	m_pIndices[m_nCurrentIndex+0] = startVert;
	m_pIndices[m_nCurrentIndex+1] = startVert + 1;
	m_pIndices[m_nCurrentIndex+2] = startVert + 2;
	m_pIndices[m_nCurrentIndex+3] = startVert;
	m_pIndices[m_nCurrentIndex+4] = startVert + 2;
	m_pIndices[m_nCurrentIndex+5] = startVert + 3;
	AdvanceIndices(6);
}

template< class T >
inline void CIndexData<T>::FastPolygon( int startVert, int triangleCount )
{
	unsigned short *pIndex = &m_pIndices[m_nCurrentIndex];
	startVert += m_nIndexOffset;
	if ( !IsX360() )
	{
		// NOTE: IndexSize is 1 or 0 (0 for alt-tab)
		// This prevents us from writing into bogus memory
		Assert( m_nIndexSize == 0 || m_nIndexSize == 1 );
		triangleCount *= m_nIndexSize;
	}
	for ( int v = 0; v < triangleCount; ++v )
	{
		*pIndex++ = startVert;
		*pIndex++ = startVert + v + 1;
		*pIndex++ = startVert + v + 2;
	}
	AdvanceIndices(triangleCount*3);
}

template< class T >
inline void CIndexData<T>::FastPolygonList( int startVert, int *pVertexCount, int polygonCount )
{
	unsigned short *pIndex = &m_pIndices[m_nCurrentIndex];
	startVert += m_nIndexOffset;
	int indexOut = 0;

	if ( !IsX360() )
	{
		// NOTE: IndexSize is 1 or 0 (0 for alt-tab)
		// This prevents us from writing into bogus memory
		Assert( m_nIndexSize == 0 || m_nIndexSize == 1 );
		polygonCount *= m_nIndexSize;
	}

	for ( int i = 0; i < polygonCount; i++ )
	{
		int vertexCount = pVertexCount[i];
		int triangleCount = vertexCount-2;
		for ( int v = 0; v < triangleCount; ++v )
		{
			*pIndex++ = startVert;
			*pIndex++ = startVert + v + 1;
			*pIndex++ = startVert + v + 2;
		}
		startVert += vertexCount;
		indexOut += triangleCount * 3;
	}
	AdvanceIndices(indexOut);
}

template< class T >
inline void CIndexData<T>::FastIndexList( const unsigned short *pIndexList, int startVert, int indexCount )
{
	unsigned short *pIndexOut = &m_pIndices[m_nCurrentIndex];
	startVert += m_nIndexOffset;
	if ( !IsX360() )
	{
		// NOTE: IndexSize is 1 or 0 (0 for alt-tab)
		// This prevents us from writing into bogus memory
		Assert( m_nIndexSize == 0 || m_nIndexSize == 1 );
		indexCount *= m_nIndexSize;
	}
	for ( int i = 0; i < indexCount; ++i )
	{
		pIndexOut[i] = startVert + pIndexList[i];
	}
	AdvanceIndices(indexCount);
}
#endif



//-----------------------------------------------------------------------------
// Dynamic index field creation
// NOTE: Draw call must occur prior to destruction of this class!
//-----------------------------------------------------------------------------
template< class T > class CDynamicIndexData : public CIndexData< T >
{
	typedef CIndexData< T > BaseClass;

public:
	CDynamicIndexData( IRenderContext* pRenderContext, int nIndexCount, const char *pDebugName, const char *pBudgetGroup );

	// This constructor is meant to be used with instance rendering.
	// Passing in either 0 or INT_MAX here means the client code 
	// doesn't know how many instances will be rendered.
	CDynamicIndexData( IRenderContext* pRenderContext, int nIndexCount, int nMaxInstanceCount, const char *pDebugName, const char *pBudgetGroup );
	~CDynamicIndexData();
	void Release();

	// Begins, ends modification of the vertex buffer (returns true if the lock succeeded)
	// A lock may not succeed if there isn't enough room
	bool Lock( );

	// Binds the vb to a particular slot using a particular offset
	void Bind( int nOffset );

private:
	void Init( IRenderContext* pRenderContext, int nIndexCount, int nMaxInstanceCount, const char *pDebugName, const char *pBudgetGroup );
};


//-----------------------------------------------------------------------------
//
// Inline methods related to CDynamicIndexData
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
template< class T >
inline CDynamicIndexData<T>::CDynamicIndexData( IRenderContext* pRenderContext,
	int nIndexCount, const char *pDebugName, const char *pBudgetGroup ) :
	BaseClass( pRenderContext, RENDER_BUFFER_HANDLE_INVALID )
{
	Init( pRenderContext, nIndexCount, 0, pDebugName, pBudgetGroup );
}

template< class T >
inline CDynamicIndexData<T>::CDynamicIndexData( IRenderContext* pRenderContext, 
	int nIndexCount, int nMaxInstanceCount, const char *pDebugName, const char *pBudgetGroup ) :
	BaseClass( pRenderContext, RENDER_BUFFER_HANDLE_INVALID )
{
	if ( nMaxInstanceCount == 0 )
	{
		nMaxInstanceCount = INT_MAX;
	}
	Init( pRenderContext, nIndexCount, nMaxInstanceCount, pDebugName, pBudgetGroup );
}

template< class T >
inline void CDynamicIndexData<T>::Init( IRenderContext* pRenderContext,
	int nIndexCount, int nMaxInstanceCount, const char *pDebugName, const char *pBudgetGroup )
{
	BufferDesc_t indexDesc;
	indexDesc.m_nElementSizeInBytes = sizeof(T);
	indexDesc.m_nElementCount = nIndexCount;
	indexDesc.m_pDebugName = pDebugName;
	indexDesc.m_pBudgetGroupName = pBudgetGroup;
	this->m_hIndexBuffer = pRenderContext->CreateDynamicIndexBuffer( indexDesc, nMaxInstanceCount );
	this->m_nBufferIndexCount = nIndexCount;

	ResourceAddRef( m_hIndexBuffer );
}

template< class T >
inline CDynamicIndexData<T>::~CDynamicIndexData()
{
	Release();
}


//-----------------------------------------------------------------------------
// Release
//-----------------------------------------------------------------------------
template< class T >
void CDynamicIndexData<T>::Release()
{
	if ( this->m_hIndexBuffer != RENDER_BUFFER_HANDLE_INVALID )
	{
		this->m_pRenderContext->DestroyDynamicIndexBuffer( this->m_hIndexBuffer );
		ResourceRelease( m_hIndexBuffer );
		this->m_hIndexBuffer = RENDER_BUFFER_HANDLE_INVALID;
	}
}


//-----------------------------------------------------------------------------
// Begins, ends modification of the buffer
//-----------------------------------------------------------------------------
template< class T >
inline bool CDynamicIndexData<T>::Lock( )
{
	Assert( this->m_hIndexBuffer != RENDER_BUFFER_HANDLE_INVALID );
	return BaseClass::Lock( );
}


//-----------------------------------------------------------------------------
// Binds the ib to a particular stream using a particular offset
//-----------------------------------------------------------------------------
template< class T >
inline void CDynamicIndexData<T>::Bind( int nOffset )
{
	this->m_pRenderContext->BindIndexBuffer( this->m_hIndexBuffer, nOffset );
}



#endif // INDEXDATA_H
