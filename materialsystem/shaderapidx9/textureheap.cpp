//========== Copyright © 2005, Valve Corporation, All rights reserved. ========
//
// Purpose: Texture heap.
//
//=============================================================================

#include "tier1/mempool.h"
#include "tier1/convar.h"
#include "tier1/utlmap.h"
#include "shaderapidx8.h"
#include "texturedx8.h"
#include "textureheap.h"
#include "shaderapidx8_global.h"
#include "filesystem.h"
#include "vstdlib/jobthread.h"
#include "tier0/icommandline.h"

#include "tier0/memdbgon.h"

struct THFreeBlock_t
{
	THInfo_t		heapInfo;
	THFreeBlock_t	*pPrevFree, *pNextFree;
};

#define BASE_COLOR_BEFORE	0xBB	// funky color to show texture while i/o in progress
#define BASE_COLOR_AFTER	0xCC	// funky color to show mip0 texture
enum TextureHeapDebug_t
{
	THD_OFF = 0,
	THD_COLORIZE_BEFOREIO,	// mip0 is colorized until i/o replaces with real bits
	THD_COLORIZE_AFTERIO,	// mip0 is colorized instead of i/o real bits
	THD_SPEW
};
ConVar texture_heap_debug( "texture_heap_debug", "0", 0, "0:Off, 1:Color Before I/O 2:Color After I/O 3:Spew" );

bool g_bUseStandardAllocator = true;	// mixed texture heap is not yet viable
bool g_bUseBasePools = true;			// do texture streaming

void GetCommandLineArgs()
{
#if defined( SUPPORTS_TEXTURE_STREAMING )
	static bool bReadCommandLine;
	if ( !bReadCommandLine )
	{
		bReadCommandLine = true;

		if ( g_pFullFileSystem->IsDVDHosted() )
		{
			g_bUseBasePools = false;
		}

		if ( CommandLine()->FindParm( "-notextureheap" ) )
		{
			g_bUseStandardAllocator = true;
		}

		if ( CommandLine()->FindParm( "-notexturestreaming" ) )
		{
			g_bUseBasePools = false;
		}
	}
#else
	g_bUseStandardAllocator = true;
	g_bUseBasePools = false;
#endif
}
bool UseStandardAllocator()
{
	GetCommandLineArgs();
	return g_bUseStandardAllocator;
}
bool UseBasePoolsForStreaming()
{
	GetCommandLineArgs();
	return g_bUseBasePools;
}

#if !defined( _RELEASE ) && !defined( _CERT )
#define StrongAssert( expr )	if ( (expr) ) ; else { DebuggerBreak(); }
#else
#define StrongAssert( expr )	((void)0)
#endif

//-----------------------------------------------------------------------------
// Set Texture HW bases
//-----------------------------------------------------------------------------
inline void SetD3DTextureBasePtr( IDirect3DBaseTexture* pTex, void *pBaseBuffer )
{
	pTex->Format.BaseAddress = ((unsigned int)pBaseBuffer) >> 12;
}
inline void SetD3DTextureMipPtr( IDirect3DBaseTexture* pTex, void *pMipBuffer )
{
	pTex->Format.MipAddress = ((unsigned int)pMipBuffer) >> 12;
}

MEMALLOC_DEFINE_EXTERNAL_TRACKING(CD3DPoolAllocator);
// only used for the pool allocators
class CD3DPoolAllocator
{
public:
	static void *Alloc( int bytes )
	{		
		DWORD attributes = MAKE_XALLOC_ATTRIBUTES( 
								0, 
								false, 
								TRUE, 
								FALSE, 
								eXALLOCAllocatorId_D3D,
								XALLOC_PHYSICAL_ALIGNMENT_4K, 
								XALLOC_MEMPROTECT_WRITECOMBINE, 
								FALSE,
								XALLOC_MEMTYPE_PHYSICAL );
		void *retval = XMemAlloc( bytes, attributes );
		MemAlloc_RegisterExternalAllocation( CD3DPoolAllocator, retval, XPhysicalSize( retval ) );
		return retval;
	}

	static void Free( void *p )
	{
		DWORD attributes = MAKE_XALLOC_ATTRIBUTES( 
								0, 
								false, 
								TRUE, 
								FALSE, 
								eXALLOCAllocatorId_D3D,
								XALLOC_PHYSICAL_ALIGNMENT_4K, 
								XALLOC_MEMPROTECT_WRITECOMBINE, 
								FALSE,
								XALLOC_MEMTYPE_PHYSICAL );

		MemAlloc_RegisterExternalDeallocation( CD3DPoolAllocator, p, XPhysicalSize( p ) );
		XMemFree( p, attributes );
	}
};

MEMALLOC_DEFINE_EXTERNAL_TRACKING(CD3DStandardAllocator);
class CD3DStandardAllocator
{
public:
	static void *Alloc( int bytes )
	{		
		DWORD attributes = MAKE_XALLOC_ATTRIBUTES( 
			0, 
			false, 
			TRUE, 
			FALSE, 
			eXALLOCAllocatorId_D3D,
			XALLOC_PHYSICAL_ALIGNMENT_4K, 
			XALLOC_MEMPROTECT_WRITECOMBINE, 
			FALSE,
			XALLOC_MEMTYPE_PHYSICAL );
		m_nTotalAllocations++;
		m_nTotalSize += AlignValue( bytes, 4096 );
		void *retval = XMemAlloc( bytes, attributes );
		MemAlloc_RegisterExternalAllocation( CD3DStandardAllocator, retval, XPhysicalSize( retval ) );
		return retval;
	}

	static void Free( void *p )
	{
		DWORD attributes = MAKE_XALLOC_ATTRIBUTES( 
			0, 
			false, 
			TRUE, 
			FALSE, 
			eXALLOCAllocatorId_D3D,
			XALLOC_PHYSICAL_ALIGNMENT_4K, 
			XALLOC_MEMPROTECT_WRITECOMBINE, 
			FALSE,
			XALLOC_MEMTYPE_PHYSICAL );
		m_nTotalAllocations--;
		m_nTotalSize -= XMemSize( p, attributes );
		MemAlloc_RegisterExternalDeallocation( CD3DStandardAllocator, p, XPhysicalSize( p ) );
		XMemFree( p, attributes );
	}

	static int GetAllocations()
	{
		return m_nTotalAllocations;
	}

	static int GetSize()
	{
		return m_nTotalSize;
	}

	static int	m_nTotalSize;
	static int	m_nTotalAllocations;
};


int CD3DStandardAllocator::m_nTotalSize;
int CD3DStandardAllocator::m_nTotalAllocations;

void SetD3DTextureImmobile( IDirect3DBaseTexture *pTexture, bool bImmobile )
{
	if ( pTexture->GetType() == D3DRTYPE_TEXTURE )
	{
		(( CXboxTexture *)pTexture)->bImmobile = bImmobile;
	}
}

CXboxTexture *GetTexture( THInfo_t *pInfo )
{
	if ( !pInfo->bFree && !pInfo->bNonTexture )
	{
		return (CXboxTexture *)((byte *)pInfo - offsetof( CXboxTexture, m_fAllocator ));
	}
	return NULL;
}

inline THFreeBlock_t *GetFreeBlock( THInfo_t *pInfo )
{
	if ( pInfo->bFree )
	{
		return (THFreeBlock_t *)((byte *)pInfo - offsetof( THFreeBlock_t, heapInfo ));
	}
	return NULL;
}

MEMALLOC_DEFINE_EXTERNAL_TRACKING(CMixedTextureHeap);
class CMixedTextureHeap
{
	enum
	{
		SIZE_ALIGNMENT = XBOX_HDD_SECTORSIZE,
		MIN_BLOCK_SIZE = 1024,
	};
public:

	CMixedTextureHeap() :
		m_nLogicalBytes( 0 ),
		m_nActualBytes( 0 ),
		m_nAllocs( 0 ),
		m_nOldBytes( 0 ),
		m_nNonTextureAllocs( 0 ),
		m_nBytesTotal( 0 ),
		m_pBase( NULL ),
		m_pFirstFree( NULL )
	{
	}

	void Init()
	{
		extern ConVar mat_texturecachesize;
		MEM_ALLOC_CREDIT_( "CMixedTextureHeap" );

		m_nBytesTotal = ( mat_texturecachesize.GetInt() * 1024 * 1024 );
#if 0
		m_nBytesTotal = AlignValue( m_nBytesTotal, SIZE_ALIGNMENT );
		m_pBase = CD3DStandardAllocator::Alloc( m_nBytesTotal );
#else
		m_nBytesTotal = AlignValue( m_nBytesTotal, 16*1024*1024 );
		m_pBase = XPhysicalAlloc( m_nBytesTotal, MAXULONG_PTR, 4096, PAGE_READWRITE | PAGE_WRITECOMBINE | MEM_16MB_PAGES );
		MemAlloc_RegisterExternalAllocation( CMixedTextureHeap, m_pBase, XPhysicalSize( m_pBase ) );
		
#endif
		m_pFirstFree = (THFreeBlock_t *)m_pBase;

		m_pFirstFree->heapInfo.bFree = true;
		m_pFirstFree->heapInfo.bNonTexture = false;
		m_pFirstFree->heapInfo.nBytes = m_nBytesTotal;
		m_pFirstFree->heapInfo.pNext = NULL;
		m_pFirstFree->heapInfo.pPrev = NULL;
		m_pFirstFree->pNextFree = NULL;
		m_pFirstFree->pPrevFree = NULL;

		m_pLastFree = m_pFirstFree;
	}

	void *Alloc( int bytes, THInfo_t *pInfo, bool bNonTexture = false )
	{
		pInfo->nBytes = AlignValue( bytes, SIZE_ALIGNMENT );

		if ( !m_pBase )
		{
			Init();
		}

		if ( bNonTexture && m_nNonTextureAllocs == 0 )
		{
			Compact();
		}

		void *p = FindBlock( pInfo );

		if ( !p )
		{
			p = ExpandToFindBlock( pInfo );
		}

		if ( p )
		{
			pInfo->nLogicalBytes = bytes;
			pInfo->bNonTexture = bNonTexture;
			m_nLogicalBytes += bytes;
			m_nOldBytes += AlignValue( bytes, 4096 );
			m_nActualBytes += pInfo->nBytes;
			m_nAllocs++;

			if ( bNonTexture )
			{
				m_nNonTextureAllocs++;
			}
		}
		return p;
	}

	void Free( void *p, THInfo_t *pInfo )
	{
		if ( !p )
		{
			return;
		}

		m_nOldBytes -= AlignValue( pInfo->nLogicalBytes, 4096 );

		if ( pInfo->bNonTexture )
		{
			m_nNonTextureAllocs--;
		}

		m_nLogicalBytes -= pInfo->nLogicalBytes;
		m_nAllocs--;
		m_nActualBytes -= pInfo->nBytes;

		THFreeBlock_t *pFree = (THFreeBlock_t *)p;
		pFree->heapInfo = *pInfo;
		pFree->heapInfo.bFree = true;

		AddToBlocksList( &pFree->heapInfo, pFree->heapInfo.pPrev, pFree->heapInfo.pNext );

		pFree = MergeLeft( pFree );
		pFree = MergeRight( pFree );

		AddToFreeList( pFree );

		if ( pInfo->bNonTexture && m_nNonTextureAllocs == 0 )
		{
			Compact();
		}
	}

	int Size( void *p, THInfo_t *pInfo )
	{
		return AlignValue( pInfo->nBytes, SIZE_ALIGNMENT );
	}

	bool IsOwner( void *p )
	{
		return ( m_pBase && p >= m_pBase && p < (byte *)m_pBase + m_nBytesTotal );
	}

	//-----------------------------------------------------

	void *FindBlock( THInfo_t *pInfo )
	{
		THFreeBlock_t *pCurrent = m_pFirstFree;

		int nBytesDesired = pInfo->nBytes;

		// Find the first block big enough to hold, then split it if appropriate
		while ( pCurrent && pCurrent->heapInfo.nBytes < nBytesDesired )
		{
			pCurrent = pCurrent->pNextFree;
		}

		if ( pCurrent )
		{
			return ClaimBlock( pCurrent, pInfo );
		}

		return NULL;
	}

	void AddToFreeList( THFreeBlock_t *pFreeBlock )
	{
		pFreeBlock->heapInfo.nLogicalBytes = 0;

		if ( m_pFirstFree )
		{
			THFreeBlock_t *pPrev = NULL;
			THFreeBlock_t *pNext = m_pFirstFree;

			int nBytes = pFreeBlock->heapInfo.nBytes;

			while ( pNext && pNext->heapInfo.nBytes < nBytes )
			{
				pPrev = pNext;
				pNext = pNext->pNextFree;
			}

			pFreeBlock->pPrevFree = pPrev;
			pFreeBlock->pNextFree = pNext;

			if ( pPrev )
			{
				pPrev->pNextFree = pFreeBlock;
			}
			else
			{
				m_pFirstFree = pFreeBlock;
			}

			if ( pNext )
			{
				pNext->pPrevFree = pFreeBlock;
			}
			else
			{
				m_pLastFree = pFreeBlock;
			}
		}
		else
		{
			pFreeBlock->pPrevFree = pFreeBlock->pNextFree = NULL;
			m_pLastFree = m_pFirstFree = pFreeBlock;
		}
	}

	void RemoveFromFreeList( THFreeBlock_t *pFreeBlock )
	{
		if ( m_pFirstFree == pFreeBlock )
		{
			m_pFirstFree = m_pFirstFree->pNextFree;
		}
		else if ( pFreeBlock->pPrevFree )
		{
			pFreeBlock->pPrevFree->pNextFree = pFreeBlock->pNextFree;
		}

		if ( m_pLastFree == pFreeBlock )
		{
			m_pLastFree = pFreeBlock->pPrevFree;
		}
		else if ( pFreeBlock->pNextFree )
		{
			pFreeBlock->pNextFree->pPrevFree = pFreeBlock->pPrevFree;
		}

		pFreeBlock->pPrevFree = pFreeBlock->pNextFree = NULL;
	}

	THFreeBlock_t *GetLastFree()
	{
		return m_pLastFree;
	}

	void AddToBlocksList( THInfo_t *pBlock, THInfo_t *pPrev, THInfo_t *pNext )
	{
		if ( pPrev )
		{
			pPrev->pNext = pBlock;
		}

		if ( pNext)
		{
			pNext->pPrev = pBlock;
		}

		pBlock->pPrev = pPrev;
		pBlock->pNext = pNext;
	}

	void RemoveFromBlocksList( THInfo_t *pBlock )
	{
		if ( pBlock->pPrev )
		{
			pBlock->pPrev->pNext = pBlock->pNext;
		}

		if ( pBlock->pNext )
		{
			pBlock->pNext->pPrev = pBlock->pPrev;
		}
	}

	//-----------------------------------------------------

	void *ClaimBlock( THFreeBlock_t *pFreeBlock, THInfo_t *pInfo )
	{
		RemoveFromFreeList( pFreeBlock );

		int nBytesDesired = pInfo->nBytes;
		int nBytesRemainder = pFreeBlock->heapInfo.nBytes - nBytesDesired;
		*pInfo = pFreeBlock->heapInfo;
		pInfo->bFree = false;
		pInfo->bNonTexture = false;
		if ( nBytesRemainder >= MIN_BLOCK_SIZE )
		{
			pInfo->nBytes = nBytesDesired;

			THFreeBlock_t *pRemainder = (THFreeBlock_t *)(((byte *)(pFreeBlock)) + nBytesDesired);
			pRemainder->heapInfo.bFree = true;
			pRemainder->heapInfo.nBytes = nBytesRemainder;

			AddToBlocksList( &pRemainder->heapInfo, pInfo, pInfo->pNext );
			AddToFreeList( pRemainder );
		}
		AddToBlocksList( pInfo, pInfo->pPrev, pInfo->pNext );
		return pFreeBlock;
	}

	THFreeBlock_t *MergeLeft( THFreeBlock_t *pFree )
	{
		THInfo_t *pPrev = pFree->heapInfo.pPrev;
		if ( pPrev && pPrev->bFree )
		{
			pPrev->nBytes += pFree->heapInfo.nBytes;
			RemoveFromBlocksList( &pFree->heapInfo );
			pFree = GetFreeBlock( pPrev );
			RemoveFromFreeList( pFree );
		}
		return pFree;
	}

	THFreeBlock_t *MergeRight( THFreeBlock_t *pFree )
	{
		THInfo_t *pNext = pFree->heapInfo.pNext;
		if ( pNext && pNext->bFree )
		{
			pFree->heapInfo.nBytes += pNext->nBytes;
			RemoveFromBlocksList( pNext );
			RemoveFromFreeList( GetFreeBlock( pNext ) );
		}
		return pFree;
	}

	//-----------------------------------------------------

	bool GetExpansionList( THFreeBlock_t *pFreeBlock, THInfo_t **ppStart, THInfo_t **ppEnd, int depth = 1 )
	{
		THInfo_t *pStart;
		THInfo_t *pEnd;
		int i;

		pStart = &pFreeBlock->heapInfo;
		pEnd = &pFreeBlock->heapInfo;

		if ( m_nNonTextureAllocs > 0 )
		{
			return false;
		}

		// Walk backwards to start of expansion
		i = depth;
		while ( i > 0 && pStart->pPrev)
		{
			THInfo_t *pScan = pStart->pPrev;

			while ( i > 0 && pScan && !pScan->bFree && GetTexture( pScan )->CanRelocate() )
			{
				pScan = pScan->pPrev;
				i--;
			}

			if ( !pScan || !pScan->bFree )
			{
				break;
			}

			pStart = pScan;
		}

		// Walk forwards to start of expansion
		i = depth;
		while ( i > 0 && pEnd->pNext)
		{
			THInfo_t *pScan = pStart->pNext;

			while ( i > 0 && pScan && !pScan->bFree && GetTexture( pScan )->CanRelocate() )
			{
				pScan = pScan->pNext;
				i--;
			}

			if ( !pScan || !pScan->bFree )
			{
				break;
			}

			pEnd = pScan;
		}

		*ppStart = pStart;
		*ppEnd = pEnd;

		return ( pStart != pEnd );
	}

	THFreeBlock_t *CompactExpansionList( THInfo_t *pStart, THInfo_t *pEnd )
	{
		// X360TBD:
		Assert( 0 );
		return NULL;
#if 0
#ifdef TH_PARANOID
		Validate();
#endif
		StrongAssert( pStart->bFree );
		StrongAssert( pEnd->bFree );
		byte *pNextBlock = (byte *)pStart;

		THInfo_t *pTextureBlock = pStart;
		THInfo_t *pLastBlock = pStart->pPrev;

		while ( pTextureBlock != pEnd )
		{
			CXboxTexture *pTexture = GetTexture( pTextureBlock );
			// If it's a texture, move it and thread it on. Otherwise, discard it
			if ( pTexture )
			{
				void *pTextureBits = GetD3DTextureBasePtr( pTexture );
				int nBytes = pTextureBlock->nBytes;

				if ( pNextBlock + nBytes <= pTextureBits)
				{
					memcpy( pNextBlock, pTextureBits, nBytes );
				}
				else
				{
					memmove( pNextBlock, pTextureBits, nBytes );
				}

				pTexture->Data = 0;
				pTexture->Register( pNextBlock );

				pNextBlock += nBytes;
				if ( pLastBlock)
				{
					pLastBlock->pNext = pTextureBlock;
				}
				pTextureBlock->pPrev = pLastBlock;
				pLastBlock = pTextureBlock;
			}
			else
			{
				StrongAssert( pTextureBlock->bFree );
				RemoveFromFreeList( GetFreeBlock( pTextureBlock ) );
			}
			pTextureBlock = pTextureBlock->pNext;
		}

		RemoveFromFreeList( GetFreeBlock( pEnd ) );

		// Make a new block and fix up the block lists
		THFreeBlock_t *pFreeBlock = (THFreeBlock_t *)pNextBlock;
		pFreeBlock->heapInfo.pPrev = pLastBlock;
		pLastBlock->pNext = &pFreeBlock->heapInfo;
		pFreeBlock->heapInfo.pNext = pEnd->pNext;
		if ( pEnd->pNext )
		{
			pEnd->pNext->pPrev = &pFreeBlock->heapInfo;
		}
		pFreeBlock->heapInfo.bFree = true;
		pFreeBlock->heapInfo.nBytes = ( (byte *)pEnd - pNextBlock ) + pEnd->nBytes;
		
		AddToFreeList( pFreeBlock );

#ifdef TH_PARANOID
		Validate();
#endif
		return pFreeBlock;
#endif
	}

	THFreeBlock_t *ExpandBlock( THFreeBlock_t *pFreeBlock, int depth = 1 )
	{
		THInfo_t *pStart;
		THInfo_t *pEnd;

		if ( GetExpansionList( pFreeBlock, &pStart, &pEnd, depth ) )
		{
			return CompactExpansionList( pStart, pEnd );
		}

		return pFreeBlock;
	}

	THFreeBlock_t *ExpandBlockToFit( THFreeBlock_t *pFreeBlock, unsigned bytes )
	{
		if ( pFreeBlock )
		{
			THInfo_t *pStart;
			THInfo_t *pEnd;

			if ( GetExpansionList( pFreeBlock, &pStart, &pEnd, 2 ) )
			{
				unsigned sum = 0;
				THInfo_t *pCurrent = pStart;
				while( pCurrent != pEnd->pNext ) 
				{
					if ( pCurrent->bFree )
					{
						sum += pCurrent->nBytes;
					}
					pCurrent = pCurrent->pNext;
				}

				if ( sum >= bytes )
				{
					pFreeBlock = CompactExpansionList( pStart, pEnd );
				}
			}
		}

		return pFreeBlock;
	}

	void *ExpandToFindBlock( THInfo_t *pInfo )
	{
		THFreeBlock_t *pFreeBlock = ExpandBlockToFit( GetLastFree(), pInfo->nBytes );
		if ( pFreeBlock && pFreeBlock->heapInfo.nBytes >= pInfo->nBytes )
		{
			return ClaimBlock( pFreeBlock, pInfo );
		}
		return NULL;
	}

	void Compact()
	{
		if ( m_nNonTextureAllocs > 0 )
		{
			return;
		}

		for (;;)
		{
			THFreeBlock_t *pCurrent = m_pFirstFree;
			THFreeBlock_t *pNew;
			while ( pCurrent )
			{
				int nBytesOld = pCurrent->heapInfo.nBytes;
				pNew = ExpandBlock( pCurrent, 999999 );

				if ( pNew != pCurrent || pNew->heapInfo.nBytes != nBytesOld )
				{
#ifdef TH_PARANOID
					Validate();
#endif
					break;
				}

#ifdef TH_PARANOID
				pNew = ExpandBlock( pCurrent, 999999 );
				StrongAssert( pNew == pCurrent && pNew->heapInfo.nBytes == nBytesOld );
#endif

				pCurrent = pCurrent->pNextFree;
			}

			if ( !pCurrent )
			{
				break;
			}
		}
	}

	void Validate()
	{
		if ( !m_pFirstFree )
		{
			return;
		}

		if ( m_nNonTextureAllocs > 0 )
		{
			return;
		}

		THInfo_t *pLast = NULL;
		THInfo_t *pInfo = &m_pFirstFree->heapInfo;

		while ( pInfo->pPrev )
		{
			pInfo = pInfo->pPrev;
		}

		void *pNextExpectedAddress = m_pBase;

		while ( pInfo )
		{
			byte *pCurrentAddress = (byte *)(( pInfo->bFree ) ? GetFreeBlock( pInfo ) : GetD3DTextureBasePtr( GetTexture( pInfo ) ) );
			StrongAssert( pCurrentAddress == pNextExpectedAddress );
			StrongAssert( pInfo->pPrev == pLast );
			pNextExpectedAddress = pCurrentAddress + pInfo->nBytes;
			pLast = pInfo;
			pInfo = pInfo->pNext;
		}

		THFreeBlock_t *pFree = m_pFirstFree;
		THFreeBlock_t *pLastFree = NULL;
		int nBytesHeap;
		nBytesHeap = XPhysicalSize( m_pBase );

		while ( pFree )
		{
			StrongAssert( pFree->pPrevFree == pLastFree );
			StrongAssert( (void *)pFree >= m_pBase && (void *)pFree < (byte *)m_pBase + nBytesHeap );
			StrongAssert( !pFree->pPrevFree || ( (void *)pFree->pPrevFree >= m_pBase && (void *)pFree->pPrevFree < (byte *)m_pBase + nBytesHeap ) );
			StrongAssert( !pFree->pNextFree || ( (void *)pFree->pNextFree >= m_pBase && (void *)pFree->pNextFree < (byte *)m_pBase + nBytesHeap ) );
			StrongAssert( !pFree->pPrevFree || pFree->pPrevFree->heapInfo.nBytes <= pFree->heapInfo.nBytes );
			pLastFree = pFree;
			pFree = pFree->pNextFree;
		}
	}

	//-----------------------------------------------------

	THFreeBlock_t *m_pFirstFree;
	THFreeBlock_t *m_pLastFree;
	void *m_pBase;

	int m_nLogicalBytes;
	int m_nActualBytes;
	int m_nAllocs;
	int m_nOldBytes;
	int m_nNonTextureAllocs;
	int m_nBytesTotal;
};
CMixedTextureHeap g_MixedTextureHeap;

CAlignedMemPool< 1024*1024, D3DTEXTURE_ALIGNMENT, BASEPOOL1024_SIZE, CD3DPoolAllocator > g_TextureBasePool_1024;
CAlignedMemPool< 512*1024, D3DTEXTURE_ALIGNMENT, BASEPOOL512_SIZE, CD3DPoolAllocator > g_TextureBasePool_512;
CAlignedMemPool< 256*1024, D3DTEXTURE_ALIGNMENT, BASEPOOL256_SIZE, CD3DPoolAllocator > g_TextureBasePool_256;
CAlignedMemPool< 128*1024, D3DTEXTURE_ALIGNMENT, BASEPOOL128_SIZE, CD3DPoolAllocator > g_TextureBasePool_128;
CAlignedMemPool< 64*1024, D3DTEXTURE_ALIGNMENT, BASEPOOL64_SIZE, CD3DPoolAllocator > g_TextureBasePool_64;
CAlignedMemPool< 32*1024, D3DTEXTURE_ALIGNMENT, BASEPOOL32_SIZE, CD3DPoolAllocator > g_TextureBasePool_32;

CTextureHeap g_TextureHeap;

//-----------------------------------------------------------------------------
// Resolve texture parameters into sizes and allocator
//-----------------------------------------------------------------------------
TextureAllocator_t TextureParamsToAllocator( int width, int height, int levels, DWORD usage, D3DFORMAT d3dFormat, unsigned int *pBaseSize, unsigned int *pMipSize )
{
	// determine texture component sizes
	XGSetTextureHeaderEx( 
		width, 
		height, 
		levels, 
		usage, 
		d3dFormat, 
		0, 
		0, 
		0, 
		0, 
		0, 
		NULL, 
		pBaseSize, 
		pMipSize );

	// based on "Xbox 360 Texture Storage"
	// can truncate the terminal level due to using tiled and packed tails
	// the terminal level must be at 32x32 or 16x16 packed
	if ( width == height && levels != 0 )
	{
		unsigned int nReduction = 0;
		int terminalWidth = width >> (levels - 1);
		if ( d3dFormat == D3DFMT_DXT1 )
		{
			if ( terminalWidth <= 32 ) 
			{
				nReduction = 4*1024;
			}
		}
		else if ( d3dFormat == D3DFMT_DXT5 ) 
		{
			if ( terminalWidth == 32 )
			{
				nReduction = 8*1024;
			}
			else if ( terminalWidth <= 16 )
			{
				nReduction = 12*1024;
			}
		}

		if ( levels == 1 )
		{
			*pBaseSize -= nReduction;
		}
		else
		{
			*pMipSize -= nReduction;
		}
	}

	if ( UseBasePoolsForStreaming() )
	{
		if ( *pMipSize && *pBaseSize >= 32*1024 )
		{
			if ( *pBaseSize <= 32*1024 )
			{
				return TA_BASEPOOL_32;
			}
			if ( *pBaseSize <= 64*1024 )
			{
				return TA_BASEPOOL_64;
			}
			if ( *pBaseSize <= 128*1024 )
			{
				return TA_BASEPOOL_128;
			}
			if ( *pBaseSize <= 256*1024 )
			{
				return TA_BASEPOOL_256;
			}
			if ( *pBaseSize <= 512*1024 && g_TextureBasePool_512.BytesTotal() )
			{
				// only allow the pool if the pool has been warmed (proper conditions cause init)
				return TA_BASEPOOL_512;
			}
			if ( *pBaseSize <= 1024*1024 && g_TextureBasePool_1024.BytesTotal() )
			{
				// only allow the pool if the pool has been warmed (proper conditions cause init)
				return TA_BASEPOOL_1024;
			}
		}
	}

	return TA_STANDARD;
}

CON_COMMAND( texture_heap_stats, "" )
{
	Msg( "Texture heap stats:\n" );

	int nActualTotal = 0;
	for ( int i = 0; i < TA_MAX; i++ )
	{
		const char *pName = "???";
		int nAllocated = 0;
		int nSize = 0;
		int nTotal = 0;
		switch ( i )
		{
		case TA_BASEPOOL_1024:
			pName = "Mip0 - 1024K";
			nAllocated = g_TextureBasePool_1024.NumAllocated();
			nSize = g_TextureBasePool_1024.BytesAllocated();
			nTotal = g_TextureBasePool_1024.BytesTotal();
			break;
		case TA_BASEPOOL_512:
			pName = "Mip0 - 512K";
			nAllocated = g_TextureBasePool_512.NumAllocated();
			nSize = g_TextureBasePool_512.BytesAllocated();
			nTotal = g_TextureBasePool_512.BytesTotal();
			break;
		case TA_BASEPOOL_256:
			pName = "Mip0 - 256K";
			nAllocated = g_TextureBasePool_256.NumAllocated();
			nSize = g_TextureBasePool_256.BytesAllocated();
			nTotal = g_TextureBasePool_256.BytesTotal();
			break;
		case TA_BASEPOOL_128:
			pName = "Mip0 - 128K";
			nAllocated = g_TextureBasePool_128.NumAllocated();
			nSize = g_TextureBasePool_128.BytesAllocated();
			nTotal = g_TextureBasePool_128.BytesTotal();
			break;
		case TA_BASEPOOL_64:
			pName = "Mip0 - 64K";
			nAllocated = g_TextureBasePool_64.NumAllocated();
			nSize = g_TextureBasePool_64.BytesAllocated();
			nTotal = g_TextureBasePool_64.BytesTotal();
			break;
		case TA_BASEPOOL_32:
			pName = "Mip0 - 32K";
			nAllocated = g_TextureBasePool_32.NumAllocated();
			nSize = g_TextureBasePool_32.BytesAllocated();
			nTotal = g_TextureBasePool_32.BytesTotal();
			break;
		case TA_MIXED:
			continue;
		case TA_STANDARD:
			pName = "Standard";
			nAllocated = CD3DStandardAllocator::GetAllocations();
			nSize = CD3DStandardAllocator::GetSize();
			nTotal = nSize;
			break;
		}
		Msg( "Pool:%-12s Allocations:%4d Used:%6.2f MB Total:%6.2fMB\n", pName, nAllocated, nSize/( 1024.0f * 1024.0f ), nTotal/( 1024.0f * 1024.0f ) );
		nActualTotal += nTotal;
	}
	Msg( "Total: %.2f MB\n", nActualTotal/( 1024.0f * 1024.0f ) );

	if ( !UseStandardAllocator() )
	{
		Msg( "Mixed textures:    %dk/%dk allocated in %d textures\n", g_MixedTextureHeap.m_nLogicalBytes/1024, g_MixedTextureHeap.m_nActualBytes/1024, g_MixedTextureHeap.m_nAllocs );
		float oldFootprint, newFootprint;
		oldFootprint = g_MixedTextureHeap.m_nOldBytes;
		newFootprint = g_MixedTextureHeap.m_nActualBytes;
		Msg( "\n  Old: %.3fmb, New: %.3fmb\n", oldFootprint / (1024.0*1024.0), newFootprint / (1024.0*1024.0) );
	}
}

CON_COMMAND( texture_heap_compact, "" )
{
	if ( UseStandardAllocator() )
	{
		return;
	}

	Msg( "Validating mixed texture heap...\n" );
	g_MixedTextureHeap.Validate();

	Msg( "Compacting mixed texture heap...\n" );
	unsigned int oldLargest, newLargest;
	oldLargest = ( g_MixedTextureHeap.GetLastFree() ) ? g_MixedTextureHeap.GetLastFree()->heapInfo.nBytes : 0;
	g_MixedTextureHeap.Compact();
	newLargest = ( g_MixedTextureHeap.GetLastFree() ) ? g_MixedTextureHeap.GetLastFree()->heapInfo.nBytes : 0;

	Msg( "\n  Old largest block: %.3fk, New largest block: %.3fk\n\n", oldLargest / 1024.0, newLargest / 1024.0 );

	Msg( "Validating mixed texture heap...\n" );
	g_MixedTextureHeap.Validate();

	Msg( "Done.\n" );
}

CON_COMMAND( texture_heap_flushlru, "" )
{
	g_TextureHeap.FlushTextureCache();
}

CON_COMMAND( texture_heap_spewlru, "" )
{
	g_TextureHeap.SpewTextureCache();
}

void CompactTextureHeap()
{
	unsigned oldLargest, newLargest;
	oldLargest = ( g_MixedTextureHeap.GetLastFree() ) ? g_MixedTextureHeap.GetLastFree()->heapInfo.nBytes : 0;
	g_MixedTextureHeap.Compact();
	newLargest = ( g_MixedTextureHeap.GetLastFree() ) ? g_MixedTextureHeap.GetLastFree()->heapInfo.nBytes : 0;

	DevMsg( "Compacted texture heap. Old largest block: %.3fk, New largest block: %.3fk\n", oldLargest / 1024.0, newLargest / 1024.0 );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CTextureHeap::CTextureHeap()
{
}


//-----------------------------------------------------------------------------
// Return the allocator used to create the texture
//-----------------------------------------------------------------------------
inline TextureAllocator_t GetTextureAllocator( IDirect3DBaseTexture9 *pTexture )
{
	return ( pTexture->GetType() == D3DRTYPE_CUBETEXTURE ) ? (( CXboxCubeTexture *)pTexture)->m_fAllocator : (( CXboxTexture *)pTexture)->m_fAllocator;
}

//-----------------------------------------------------------------------------
// Build and alloc a texture resource
//-----------------------------------------------------------------------------
IDirect3DTexture *CTextureHeap::AllocTexture( int width, int height, int levels, DWORD usage, D3DFORMAT d3dFormat, bool bNoD3DMemory, bool bCacheable )
{
#if defined( SUPPORTS_TEXTURE_STREAMING )
	static bool bInitializedPools;
	if ( !bInitializedPools )
	{
		// Warm the pools, now once
		// Pools that should not exist due to state, will inhibit any further potential latent use
		bInitializedPools = true;
		if ( !g_pFullFileSystem->IsDVDHosted() )
		{
			MEM_ALLOC_CREDIT_( __FILE__ ": Base, Pooled D3D" );
			bool bNo1024 = CommandLine()->FindParm( "-no1024" ) && !CommandLine()->FindParm( "-allow1024" );
			if ( !bNo1024 )
			{
				g_TextureBasePool_1024.Free( g_TextureBasePool_1024.Alloc() );
				g_TextureBasePool_512.Free( g_TextureBasePool_512.Alloc() );
			}
			g_TextureBasePool_256.Free( g_TextureBasePool_256.Alloc() );
			g_TextureBasePool_128.Free( g_TextureBasePool_128.Alloc() );
			g_TextureBasePool_64.Free( g_TextureBasePool_64.Alloc() );
			g_TextureBasePool_32.Free( g_TextureBasePool_32.Alloc() );
		}
	}
#endif

	// determine allocator and texture component sizes
	unsigned int nBaseSize;
	unsigned int nMipSize;
	TextureAllocator_t nAllocator = TextureParamsToAllocator( width, height, levels, usage, d3dFormat, &nBaseSize, &nMipSize );

	if ( bCacheable && nAllocator == TA_STANDARD )
	{
		// texture doesn't meet the cacheing guidelines
		bCacheable = false;
	}

	int mipOffset;
	if ( !bCacheable )
	{
		// a non-cacheable texture is setup with a contiguous base
		nAllocator = TA_STANDARD;
		mipOffset = XGHEADER_CONTIGUOUS_MIP_OFFSET;
		nBaseSize += nMipSize;
		nMipSize = 0;
	}
	else
	{
		// a cacheable texture has a non-contiguous base and mips
		mipOffset = 0;
		Assert( nBaseSize && nMipSize );
	}

	void *pBaseBuffer = NULL;
	void *pMipBuffer = NULL;
	if ( !bNoD3DMemory )
	{
		if ( !bCacheable )
		{
			MEM_ALLOC_CREDIT_( __FILE__ ": Standard D3D" );
			// the base and mips are contiguous
			// the base is mandatory
			pBaseBuffer = CD3DStandardAllocator::Alloc( nBaseSize );
			if ( !pBaseBuffer )
			{
				// shouldn't happen, out of memory
				return NULL;
			}
		}
		else
		{
			{
				MEM_ALLOC_CREDIT_( __FILE__ ": Base, Pooled D3D" );
				// base goes into its own seperate pool seperate from mips
				// pools may be full, a failed pool allocation is permissible
				switch ( nAllocator )
				{
				case TA_BASEPOOL_1024:
					pBaseBuffer = g_TextureBasePool_1024.Alloc();
					break;
				case TA_BASEPOOL_512:
					pBaseBuffer = g_TextureBasePool_512.Alloc();
					break;
				case TA_BASEPOOL_256:
					pBaseBuffer = g_TextureBasePool_256.Alloc();
					break;
				case TA_BASEPOOL_128:
					pBaseBuffer = g_TextureBasePool_128.Alloc();
					break;
				case TA_BASEPOOL_64:
					pBaseBuffer = g_TextureBasePool_64.Alloc();
					break;
				case TA_BASEPOOL_32:
					pBaseBuffer = g_TextureBasePool_32.Alloc();
					break;
				}
			}
			{
				MEM_ALLOC_CREDIT_( __FILE__ ": Mips, Standard D3D" );
				// the mips go elsewhere
				// the mips are mandatory
				pMipBuffer = CD3DStandardAllocator::Alloc( nMipSize );
				if ( !pMipBuffer )
				{
					// shouldn't happen, out of memory
					return NULL;
				}
			}
		}
	}

	MEM_ALLOC_CREDIT_( __FILE__ ": Texture Headers" );

	CXboxTexture* pXboxTexture = new CXboxTexture;

	XGSetTextureHeaderEx( 
		width, 
		height, 
		levels, 
		usage, 
		d3dFormat, 
		0, 
		0, 
		0, 
		mipOffset, 
		0, 
		(IDirect3DTexture*)pXboxTexture, 
		NULL, 
		NULL );

	pXboxTexture->m_fAllocator = nAllocator;
	pXboxTexture->m_nBaseSize = nBaseSize;
	pXboxTexture->m_nMipSize = nMipSize;
	pXboxTexture->m_bBaseAllocated = ( pBaseBuffer != NULL );
	pXboxTexture->m_bMipAllocated = ( pMipBuffer != NULL );

	// assuming that a texture that allocates now is using the synchronous loader 
	// and is about to blit textures
	pXboxTexture->m_BaseValid = pXboxTexture->m_bBaseAllocated;

	if ( bCacheable )
	{
		pXboxTexture->m_tcHandle = m_TextureCache.AddToTail( (IDirect3DTexture*)pXboxTexture );
	}

	if ( !bNoD3DMemory )
	{
		if ( !bCacheable )
		{
			// non cacheable texture, base and mip are contiguous
			SetD3DTextureBasePtr( (IDirect3DTexture*)pXboxTexture, pBaseBuffer );
			// retrieve offset and fixup
			void *pMipOffset = GetD3DTextureMipPtr( (IDirect3DTexture*)pXboxTexture );
			if ( pMipOffset )
			{
				SetD3DTextureMipPtr( (IDirect3DTexture*)pXboxTexture, (unsigned char *)pBaseBuffer + (unsigned int)pMipOffset );
			}
		}
		else
		{
			// cacheable texture, base may or may not be allocated now
			if ( !pBaseBuffer )
			{
				// d3d error checking requires we stuff a valid, but bogus base pointer
				// the base pointer gets properly set when this cacheable texture is touched
				SetD3DTextureBasePtr( (IDirect3DTexture*)pXboxTexture, pMipBuffer );
				SetD3DTextureMipPtr( (IDirect3DTexture*)pXboxTexture, pMipBuffer );
			}
			else
			{
				SetD3DTextureBasePtr( (IDirect3DTexture*)pXboxTexture, pBaseBuffer );
				SetD3DTextureMipPtr( (IDirect3DTexture*)pXboxTexture, pMipBuffer );
			}
		}
	}

	return (IDirect3DTexture*)pXboxTexture;
}

//-----------------------------------------------------------------------------
// Build and alloc a cube texture resource
//-----------------------------------------------------------------------------
IDirect3DCubeTexture *CTextureHeap::AllocCubeTexture( int width, int levels, DWORD usage, D3DFORMAT d3dFormat, bool bNoD3DMemory )
{
	MEM_ALLOC_CREDIT_( __FILE__ ": Texture Headers" );

	CXboxCubeTexture* pXboxCubeTexture = new CXboxCubeTexture;

	// create a cube texture with contiguous mips and packed tails
	DWORD dwTextureSize = XGSetCubeTextureHeaderEx( 
		width,  
		levels, 
		usage, 
		d3dFormat, 
		0, 
		0, 
		0,
		XGHEADER_CONTIGUOUS_MIP_OFFSET, 
		(IDirect3DCubeTexture*)pXboxCubeTexture, 
		NULL, 
		NULL );

	pXboxCubeTexture->m_fAllocator = TA_STANDARD;
	pXboxCubeTexture->m_nBaseSize = dwTextureSize;
	pXboxCubeTexture->m_bBaseAllocated = ( bNoD3DMemory == false );

	if ( bNoD3DMemory )
	{
		return (IDirect3DCubeTexture*)pXboxCubeTexture;
	}

	void *pBits;
	{
		MEM_ALLOC_CREDIT_( __FILE__ ": Cubemap, Standard D3D" );
		pBits = CD3DStandardAllocator::Alloc( dwTextureSize );
		if ( !pBits )
		{
			delete pXboxCubeTexture;
			return NULL;
		}
	}

	// base and mips are contiguous
	SetD3DTextureBasePtr( (IDirect3DTexture*)pXboxCubeTexture, pBits );
	// retrieve offset and fixup
	void *pMipOffset = GetD3DTextureMipPtr( (IDirect3DTexture*)pXboxCubeTexture );
	if ( pMipOffset )
	{
		SetD3DTextureMipPtr( (IDirect3DTexture*)pXboxCubeTexture, (unsigned char *)pBits + (unsigned int)pMipOffset );
	}

	return (IDirect3DCubeTexture*)pXboxCubeTexture;
}

//-----------------------------------------------------------------------------
// Allocate an Volume Texture
//-----------------------------------------------------------------------------
IDirect3DVolumeTexture *CTextureHeap::AllocVolumeTexture( int width, int height, int depth, int levels, DWORD usage, D3DFORMAT d3dFormat )
{
	MEM_ALLOC_CREDIT_( __FILE__ ": Texture Headers" );

	CXboxVolumeTexture *pXboxVolumeTexture = new CXboxVolumeTexture;

	// create a cube texture with contiguous mips and packed tails
	DWORD dwTextureSize = XGSetVolumeTextureHeaderEx( 
		width,
		height, 
		depth,
		levels, 
		usage, 
		d3dFormat, 
		0, 
		0, 
		0,
		XGHEADER_CONTIGUOUS_MIP_OFFSET, 
		(IDirect3DVolumeTexture *)pXboxVolumeTexture, 
		NULL, 
		NULL );
	
	void *pBits;
	{
		MEM_ALLOC_CREDIT_( __FILE__ ": Volume, Standard D3D" );
		pBits = CD3DStandardAllocator::Alloc( dwTextureSize );
		if ( !pBits )
		{
			delete pXboxVolumeTexture;
			return NULL;
		}
	}

	pXboxVolumeTexture->m_fAllocator = TA_STANDARD;
	pXboxVolumeTexture->m_nBaseSize = dwTextureSize;
	pXboxVolumeTexture->m_bBaseAllocated = true;

	// base and mips are contiguous
	SetD3DTextureBasePtr( (IDirect3DTexture*)pXboxVolumeTexture, pBits );
	// retrieve offset and fixup
	void *pMipOffset = GetD3DTextureMipPtr( (IDirect3DTexture*)pXboxVolumeTexture );
	if ( pMipOffset )
	{
		SetD3DTextureMipPtr( (IDirect3DTexture*)pXboxVolumeTexture, (unsigned char *)pBits + (unsigned int)pMipOffset );
	}

	return pXboxVolumeTexture; 
}

//-----------------------------------------------------------------------------
// Get current backbuffer multisample type (used in AllocRenderTargetSurface() )
//-----------------------------------------------------------------------------
D3DMULTISAMPLE_TYPE CTextureHeap::GetBackBufferMultiSampleType()
{
	int backWidth, backHeight;
	ShaderAPI()->GetBackBufferDimensions( backWidth, backHeight );

	// 2xMSAA at 640x480 and 848x480 are the only supported multisample mode on 360 (2xMSAA for 720p would
	// use predicated tiling, which would require a rewrite of *all* our render target code)
	// FIXME: shuffle the EDRAM surfaces to allow 4xMSAA for standard def
	//        (they would overlap & trash each other with the current allocation scheme)
	D3DMULTISAMPLE_TYPE backBufferMultiSampleType = g_pShaderDevice->IsAAEnabled() ? D3DMULTISAMPLE_2_SAMPLES : D3DMULTISAMPLE_NONE;
	Assert( ( g_pShaderDevice->IsAAEnabled() == false ) || (backHeight == 480) );

	return backBufferMultiSampleType;
}

//-----------------------------------------------------------------------------
// Allocate an EDRAM surface
//-----------------------------------------------------------------------------
IDirect3DSurface *CTextureHeap::AllocRenderTargetSurface( int width, int height, D3DFORMAT d3dFormat, RTMultiSampleCount360_t multiSampleCount, int base )
{	
	// render target surfaces don't need to exist simultaneously
	// force their allocations to overlap at the end of back buffer and zbuffer
	// this should leave 3MB (of 10) free assuming 1280x720 (and 5MB with 640x480@2xMSAA)
	D3DMULTISAMPLE_TYPE backBufferMultiSampleType = GetBackBufferMultiSampleType();
	D3DMULTISAMPLE_TYPE multiSampleType = D3DMULTISAMPLE_NONE;

	switch ( multiSampleCount )
	{
	case RT_MULTISAMPLE_MATCH_BACKBUFFER:
		multiSampleType = backBufferMultiSampleType;
		break;

	case RT_MULTISAMPLE_NONE:
		multiSampleType = D3DMULTISAMPLE_NONE;
		break;

	case RT_MULTISAMPLE_2_SAMPLES:
		multiSampleType = D3DMULTISAMPLE_2_SAMPLES;
		break;

	case RT_MULTISAMPLE_4_SAMPLES:
		multiSampleType = D3DMULTISAMPLE_4_SAMPLES;
		break;

	default:
		Assert( !"Invalid multisample count" );
		multiSampleType = D3DMULTISAMPLE_NONE;
	}

	if ( base < 0 )
	{
		int backWidth, backHeight;
		ShaderAPI()->GetBackBufferDimensions( backWidth, backHeight );
		D3DFORMAT backBufferFormat = ImageLoader::ImageFormatToD3DFormat( g_pShaderDevice->GetBackBufferFormat() );
		// this is the size of back+depthbuffer in EDRAM
		base = 2*XGSurfaceSize( backWidth, backHeight, backBufferFormat, backBufferMultiSampleType );
	}

	D3DSURFACE_PARAMETERS surfParameters;
	V_memset( &surfParameters, 0, sizeof( surfParameters ) );
	surfParameters.Base = base;
	surfParameters.ColorExpBias = 0;
	surfParameters.HiZFunc = D3DHIZFUNC_DEFAULT;

	if ( ( d3dFormat == D3DFMT_D24FS8 ) || ( d3dFormat == D3DFMT_D24S8 ) || ( d3dFormat == D3DFMT_D16 ) )
	{
		surfParameters.HierarchicalZBase = 0;
		if ( ( surfParameters.HierarchicalZBase + XGHierarchicalZSize( width, height, multiSampleType ) ) > GPU_HIERARCHICAL_Z_TILES )
		{
			// overflow, can't hold the tiles so disable
			surfParameters.HierarchicalZBase = 0xFFFFFFFF; 
		}
	}
	else
	{
		// not using
		surfParameters.HierarchicalZBase = 0xFFFFFFFF;
	}	

	HRESULT hr;
	IDirect3DSurface9 *pSurface = NULL;
	hr = Dx9Device()->CreateRenderTarget( width, height, d3dFormat, multiSampleType, 0, FALSE, &pSurface, &surfParameters );
	Assert( !FAILED( hr ) );

	return pSurface;
}

//-----------------------------------------------------------------------------
// Perform the real d3d allocation, returns true if succesful, false otherwise.
// Only valid for a texture created with no d3d bits, otherwise no-op.
//-----------------------------------------------------------------------------
bool CTextureHeap::FixupAllocD3DMemory( IDirect3DBaseTexture *pD3DTexture )
{
	if ( !pD3DTexture )
	{
		return false;
	}

	if ( pD3DTexture->GetType() == D3DRTYPE_SURFACE )
	{
		// there are no d3d bits for a surface
		return false;
	}

	if ( GetD3DTextureBasePtr( pD3DTexture ) )
	{
		// already allocated
		return true;
	}

	if ( pD3DTexture->GetType() == D3DRTYPE_TEXTURE )
	{
		if ( ((CXboxTexture *)pD3DTexture)->m_bMipAllocated )
		{
			// cacheable texture has already been setup
			return true;
		}

		int nAllocator = ((CXboxTexture *)pD3DTexture)->m_fAllocator;
		unsigned int nBaseSize = ((CXboxTexture *)pD3DTexture)->m_nBaseSize;
		unsigned int nMipSize = ((CXboxTexture *)pD3DTexture)->m_nMipSize;

		bool bCacheable = ( nAllocator != TA_STANDARD );
		void *pBaseBuffer = NULL;
		void *pMipBuffer = NULL;
		if ( !bCacheable )
		{
			MEM_ALLOC_CREDIT_( __FILE__ ": Standard D3D" );
			// base and mips are contiguous
			Assert( nBaseSize && nMipSize == 0 );
			pBaseBuffer = CD3DStandardAllocator::Alloc( nBaseSize );
			if ( !pBaseBuffer )
			{
				// base is required, out of memory
				return false;
			}
		}
		else
		{
			{
				MEM_ALLOC_CREDIT_( __FILE__ ": Mips, Standard D3D" );
				// base and mips are non-contiguous
				Assert( nBaseSize && nMipSize );
				pMipBuffer = CD3DStandardAllocator::Alloc( nMipSize );
				if ( !pMipBuffer )
				{
					// mips are required, out of memory
					return false;
				}
			}

			{
				MEM_ALLOC_CREDIT_( __FILE__ ": Base, Pooled D3D" );
				// base goes into its own seperate pool seperate from mips
				// pools my be filled, failure is allowed
				switch ( nAllocator )
				{
				case TA_BASEPOOL_1024:
					pBaseBuffer = g_TextureBasePool_1024.Alloc();
					break;
				case TA_BASEPOOL_512:
					pBaseBuffer = g_TextureBasePool_512.Alloc();
					break;
				case TA_BASEPOOL_256:
					pBaseBuffer = g_TextureBasePool_256.Alloc();
					break;
				case TA_BASEPOOL_128:
					pBaseBuffer = g_TextureBasePool_128.Alloc();
					break;
				case TA_BASEPOOL_64:
					pBaseBuffer = g_TextureBasePool_64.Alloc();
					break;
				case TA_BASEPOOL_32:
					pBaseBuffer = g_TextureBasePool_32.Alloc();
					break;
				}
			}
		}

		((CXboxTexture *)pD3DTexture)->m_bBaseAllocated = ( pBaseBuffer != NULL );
		((CXboxTexture *)pD3DTexture)->m_bMipAllocated = ( pMipBuffer != NULL );

		if ( !bCacheable )
		{
			// non cacheable texture, base and mip are contiguous
			SetD3DTextureBasePtr( pD3DTexture, pBaseBuffer );
			// retrieve offset and fixup
			void *pMipOffset = GetD3DTextureMipPtr( pD3DTexture );
			if ( pMipOffset )
			{
				SetD3DTextureMipPtr( pD3DTexture, (unsigned char *)pBaseBuffer + (unsigned int)pMipOffset );
			}
			// the async queued loader is about to blit textures, so mark valid for render
			((CXboxTexture *)pD3DTexture)->m_BaseValid = 1;
		}
		else
		{
			// cacheable texture, base may or may not be allocated now
			if ( !pBaseBuffer )
			{
				// d3d error checking requires we stuff a valid, but bogus base pointer
				// the base pointer gets properly set when this cacheable texture is touched
				SetD3DTextureBasePtr( pD3DTexture, pMipBuffer );
				SetD3DTextureMipPtr( pD3DTexture, pMipBuffer );
			}
			else
			{
				SetD3DTextureBasePtr( pD3DTexture, pBaseBuffer );
				SetD3DTextureMipPtr( pD3DTexture, pMipBuffer );
				// the async queued loader is about to blit textures, so mark valid for render
				((CXboxTexture *)pD3DTexture)->m_BaseValid = 1;
			}
		}

		return true;
	}
	else if ( pD3DTexture->GetType() == D3DRTYPE_CUBETEXTURE )
	{
		MEM_ALLOC_CREDIT_( __FILE__ ": Cubemap, Standard D3D" );
		void *pBaseBuffer = CD3DStandardAllocator::Alloc( ((CXboxCubeTexture *)pD3DTexture)->m_nBaseSize );
		if ( !pBaseBuffer )
		{
			// base is required, out of memory
			return false;
		}

		SetD3DTextureBasePtr( pD3DTexture, pBaseBuffer );
		// retrieve offset and fixup
		void *pMipOffset = GetD3DTextureMipPtr( pD3DTexture );
		if ( pMipOffset )
		{
			SetD3DTextureMipPtr( pD3DTexture, (unsigned char *)pBaseBuffer + (unsigned int)pMipOffset );
		}

		((CXboxCubeTexture *)pD3DTexture)->m_bBaseAllocated = true;

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Release the allocated store
//-----------------------------------------------------------------------------
void CTextureHeap::FreeTexture( IDirect3DBaseTexture *pD3DTexture )
{
	if ( !pD3DTexture )
	{
		return;
	}

	if ( pD3DTexture->GetType() == D3DRTYPE_SURFACE )
	{
		// texture heap doesn't own render target surfaces
		// allow callers to call through for less higher level detection
		int ref = ((IDirect3DSurface*)pD3DTexture)->Release();
		Assert( ref == 0 );
		ref = ref; 
		return;
	}
	
	if ( IsBaseAllocated( pD3DTexture ) )
	{
		byte *pBaseBits = (byte *)GetD3DTextureBasePtr( pD3DTexture );
		if ( pBaseBits )
		{
			switch ( GetTextureAllocator( pD3DTexture ) )
			{
			case TA_BASEPOOL_1024:
				g_TextureBasePool_1024.Free( pBaseBits );
				break;
			case TA_BASEPOOL_512:
				g_TextureBasePool_512.Free( pBaseBits );
				break;
			case TA_BASEPOOL_256:
				g_TextureBasePool_256.Free( pBaseBits );
				break;
			case TA_BASEPOOL_128:
				g_TextureBasePool_128.Free( pBaseBits );
				break;
			case TA_BASEPOOL_64:
				g_TextureBasePool_64.Free( pBaseBits );
				break;
			case TA_BASEPOOL_32:
				g_TextureBasePool_32.Free( pBaseBits );
				break;
			case TA_STANDARD:
				CD3DStandardAllocator::Free( pBaseBits );
				break;
			case TA_MIXED:
				g_MixedTextureHeap.Free( pBaseBits, ((CXboxTexture *)pD3DTexture) );
				break;
			}
		}
	}

	if ( pD3DTexture->GetType() == D3DRTYPE_TEXTURE )
	{
		if ( ((CXboxTexture *)pD3DTexture)->m_tcHandle != INVALID_TEXTURECACHE_HANDLE )
		{
			m_TextureCache.Remove( ((CXboxTexture *)pD3DTexture)->m_tcHandle );
		}
		if ( ((CXboxTexture *)pD3DTexture)->m_bMipAllocated )
		{
			// not an offset, but a true pointer
			void *pMipBits = GetD3DTextureMipPtr( pD3DTexture );
			if ( pMipBits )
			{
				CD3DStandardAllocator::Free( pMipBits );
			}
		}
		delete (CXboxTexture *)pD3DTexture;
	}
	else if ( pD3DTexture->GetType() == D3DRTYPE_VOLUMETEXTURE )
	{
		delete (CXboxVolumeTexture *)pD3DTexture;
	}
	else if ( pD3DTexture->GetType() == D3DRTYPE_CUBETEXTURE )
	{
		delete (CXboxCubeTexture *)pD3DTexture;
	}
}

//-----------------------------------------------------------------------------
// Returns the allocated footprint.
//-----------------------------------------------------------------------------
int	CTextureHeap::GetSize( IDirect3DBaseTexture *pD3DTexture )
{
	if ( !pD3DTexture )
	{
		return 0;
	}

	if ( pD3DTexture->GetType() == D3DRTYPE_SURFACE )
	{
		D3DSURFACE_DESC surfaceDesc;
		HRESULT hr = ((IDirect3DSurface*)pD3DTexture)->GetDesc( &surfaceDesc );
		Assert( !FAILED( hr ) );
		hr = hr;

		int size = ImageLoader::GetMemRequired( 
			surfaceDesc.Width,
			surfaceDesc.Height,
			0,
			ImageLoader::D3DFormatToImageFormat( surfaceDesc.Format ),
			false );

		return size;
	}
	else if ( pD3DTexture->GetType() == D3DRTYPE_TEXTURE )
	{
		return ((CXboxTexture *)pD3DTexture)->m_nBaseSize + ((CXboxTexture *)pD3DTexture)->m_nMipSize;
	}
	else if ( pD3DTexture->GetType() == D3DRTYPE_CUBETEXTURE )
	{
		return ((CXboxCubeTexture *)pD3DTexture)->m_nBaseSize;
	}
	else if ( pD3DTexture->GetType() == D3DRTYPE_VOLUMETEXTURE )
	{
		return ((CXboxVolumeTexture *)pD3DTexture)->m_nBaseSize;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Returns the amount of memory needed just for the cacheable component.
//-----------------------------------------------------------------------------
int CTextureHeap::GetCacheableSize( IDirect3DBaseTexture *pD3DTexture )
{
	if ( !pD3DTexture )
	{
		return 0;
	}

	if ( pD3DTexture->GetType() == D3DRTYPE_TEXTURE && ((CXboxTexture *)pD3DTexture)->m_fAllocator != TA_STANDARD )
	{
		// the base is the cacheable component
		return ((CXboxTexture *)pD3DTexture)->m_nBaseSize;
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Crunch the pools
//-----------------------------------------------------------------------------
void CTextureHeap::Compact()
{
	g_MixedTextureHeap.Compact();
}

//-----------------------------------------------------------------------------
// Query to determine if texture was setup for cacheing.
//-----------------------------------------------------------------------------
bool CTextureHeap::IsTextureCacheManaged( IDirect3DBaseTexture *pD3DTexture )
{
	if ( !pD3DTexture )
	{
		return false;
	}

	if ( pD3DTexture->GetType() == D3DRTYPE_TEXTURE )
	{
		return ((CXboxTexture *)pD3DTexture)->m_tcHandle != INVALID_TEXTURECACHE_HANDLE;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Query to determine if texture has an allocated base.
//-----------------------------------------------------------------------------
bool CTextureHeap::IsBaseAllocated( IDirect3DBaseTexture *pD3DTexture )
{
	if ( !pD3DTexture )
	{
		return false;
	}

	if ( pD3DTexture->GetType() == D3DRTYPE_TEXTURE )
	{
		return ((CXboxTexture *)pD3DTexture)->m_bBaseAllocated;
	}
	else if ( pD3DTexture->GetType() == D3DRTYPE_CUBETEXTURE )
	{
		return ((CXboxCubeTexture *)pD3DTexture)->m_bBaseAllocated;
	}
	else if ( pD3DTexture->GetType() == D3DRTYPE_VOLUMETEXTURE )
	{
		return ((CXboxVolumeTexture *)pD3DTexture)->m_bBaseAllocated;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Query to determine if texture is valid for hi-res rendering.
//-----------------------------------------------------------------------------
bool CTextureHeap::IsTextureResident( IDirect3DBaseTexture *pD3DTexture )
{
	if ( !pD3DTexture )
	{
		return false;
	}

	// only the simple texture type streams and can be evicted
	if ( pD3DTexture->GetType() == D3DRTYPE_TEXTURE )
	{
		return ( ((CXboxTexture *)pD3DTexture)->m_BaseValid != 0 );
	}

	// all other types, cube, volume, are defined as resident
	return true;
}

//-----------------------------------------------------------------------------
// Used for debugging purposes only!!! Forceful eviction. Can cause system lockups
// under repeated use due to desired forced behavior and ignoring GPU.
//-----------------------------------------------------------------------------
void CTextureHeap::FlushTextureCache()
{
	TextureCacheHandle_t tcHandle;
	for ( tcHandle = m_TextureCache.Head(); tcHandle != m_TextureCache.InvalidIndex(); tcHandle = m_TextureCache.Next( tcHandle ) )
	{
		CXboxTexture *pPurgeCandidate = (CXboxTexture *)m_TextureCache[tcHandle];

		if ( !pPurgeCandidate->m_BaseValid )
		{
			continue;
		}

		byte *pBaseBits = (byte *)GetD3DTextureBasePtr( pPurgeCandidate );
		Assert( pBaseBits );
		if ( pBaseBits )
		{
			switch ( pPurgeCandidate->m_fAllocator )
			{
			case TA_BASEPOOL_1024:
				g_TextureBasePool_1024.Free( pBaseBits );
				break;
			case TA_BASEPOOL_512:
				g_TextureBasePool_512.Free( pBaseBits );
				break;
			case TA_BASEPOOL_256:
				g_TextureBasePool_256.Free( pBaseBits );
				break;
			case TA_BASEPOOL_128:
				g_TextureBasePool_128.Free( pBaseBits );
				break;
			case TA_BASEPOOL_64:
				g_TextureBasePool_64.Free( pBaseBits );
				break;
			case TA_BASEPOOL_32:
				g_TextureBasePool_32.Free( pBaseBits );
				break;
			}
		}
		pPurgeCandidate->m_bBaseAllocated = false;
		pPurgeCandidate->m_BaseValid = 0;
	}
}

//-----------------------------------------------------------------------------
// Computation job to do work after IO, runs callback
//-----------------------------------------------------------------------------
static void IOComputationJob( IDirect3DBaseTexture *pD3DTexture, void *pData, int nDataSize, TextureLoadError_t loaderError )
{
	CXboxTexture *pXboxTexture = (CXboxTexture *)pD3DTexture;

	if ( texture_heap_debug.GetInt() == THD_SPEW )
	{
		char szFilename[MAX_PATH];
		g_pFullFileSystem->String( pXboxTexture->m_hFilename, szFilename, sizeof( szFilename ) );
		Msg( "Arrived: size:%d thread:0x%8.8x %s\n", nDataSize, ThreadGetCurrentId(), szFilename );
	}
	
	if ( texture_heap_debug.GetInt() == THD_COLORIZE_AFTERIO )
	{
		// colorize the base to use during the i/o latency
		V_memset( GetD3DTextureBasePtr( pD3DTexture ), BASE_COLOR_AFTER, pXboxTexture->m_nBaseSize );
	}
	else if ( pData && nDataSize )
	{
		// get a unique vtf and mount texture
		// vtf can expect non-volatile buffer data to be stable through vtf lifetime
		// this prevents redundant copious amounts of image memory transfers
		IVTFTexture *pVTFTexture = CreateVTFTexture();

		CUtlBuffer vtfBuffer;
		vtfBuffer.SetExternalBuffer( (void *)pData, nDataSize, nDataSize );	
		if ( pVTFTexture->UnserializeFromBuffer( vtfBuffer, false, false, false, 0 ) )
		{
			// provided vtf buffer is all mips, determine top mip due to possible picmip
			int mipWidth, mipHeight, mipDepth;
			pVTFTexture->ComputeMipLevelDimensions( pXboxTexture->m_nMipSkipCount, &mipWidth, &mipHeight, &mipDepth );
			
			// blit the hi-res texture bits into d3d memory
			unsigned char *pSourceBits = pVTFTexture->ImageData( 0, 0, pXboxTexture->m_nMipSkipCount, 0, 0, 0 );

			TextureLoadInfo_t info;
			info.m_TextureHandle = INVALID_SHADERAPI_TEXTURE_HANDLE;
			info.m_pTexture = pD3DTexture;
			info.m_nLevel = 0;
			info.m_nCopy = 0;
			info.m_CubeFaceID = (D3DCUBEMAP_FACES)0;
			info.m_nWidth = mipWidth;
			info.m_nHeight = mipHeight;
			info.m_nZOffset = 0;
			info.m_SrcFormat = pVTFTexture->Format();
			info.m_pSrcData = pSourceBits;
			info.m_bSrcIsTiled = pVTFTexture->IsPreTiled();
			info.m_bCanConvertFormat = false;

			LoadTexture( info );
		}

		if ( pVTFTexture )
		{
			DestroyVTFTexture( pVTFTexture );
		}
	}

	if ( pData )
	{
		g_pFullFileSystem->FreeOptimalReadBuffer( pData );
	}

	g_pFullFileSystem->AsyncRelease( pXboxTexture->m_hAsyncControl );
	pXboxTexture->m_hAsyncControl = NULL;

	// ready for render
	pXboxTexture->m_BaseValid = 1;
}

//-----------------------------------------------------------------------------
// Callback from I/O job thread. Purposely lightweight as possible to keep i/o from stalling.
//-----------------------------------------------------------------------------
static void IOAsyncCallback( const FileAsyncRequest_t &asyncRequest, int numReadBytes, FSAsyncStatus_t asyncStatus )
{
	IDirect3DBaseTexture *pD3DTexture = (IDirect3DBaseTexture *)asyncRequest.pContext;

	// interpret the async error
	TextureLoadError_t loaderError;
	switch ( asyncStatus )
	{
	case FSASYNC_OK:
		loaderError = TEXLOADERROR_NONE;
		break;
	case FSASYNC_ERR_FILEOPEN:
		loaderError = TEXLOADERROR_FILEOPEN;
		break;
	default:
		loaderError = TEXLOADERROR_READING;
	}

	// have data or error, do callback as a computation job
	CJob *pComputationJob = new CFunctorJob( CreateFunctor( IOComputationJob, pD3DTexture, asyncRequest.pData, numReadBytes, loaderError ) );
	pComputationJob->SetServiceThread( 1 );
	pComputationJob->SetFlags( pComputationJob->GetFlags() | JF_QUEUE );
	g_pThreadPool->AddJob( pComputationJob );
	pComputationJob->Release();
}

//-----------------------------------------------------------------------------
// Attempts to restore a cacheable texture. An async i/o operation may be kicked
// off.
//-----------------------------------------------------------------------------
bool CTextureHeap::RestoreCacheableTexture( IDirect3DBaseTexture *pD3DTexture )
{
	static unsigned int s_failedAllocator[TA_MAX];

	unsigned int nFrameCount = g_pShaderAPIDX8->GetCurrentFrameCounter();
	CXboxTexture *pXboxTexture = (CXboxTexture *)pD3DTexture;

	if ( s_failedAllocator[pXboxTexture->m_fAllocator] == nFrameCount )
	{
		// allocator has already failed an eviction this frame
		// avoid costly pointless search, retry next frame
		return false;
	}

	void *pBaseBuffer = NULL;
	TextureCacheHandle_t tcHandle = m_TextureCache.Head();

	int numAttempts = 0;
	while ( numAttempts < 2 )
	{
		{
			MEM_ALLOC_CREDIT_( __FILE__ ": Base, Pooled D3D" );
			switch ( pXboxTexture->m_fAllocator )
			{
			case TA_BASEPOOL_1024:
				pBaseBuffer = g_TextureBasePool_1024.Alloc();
				break;
			case TA_BASEPOOL_512:
				pBaseBuffer = g_TextureBasePool_512.Alloc();
				break;
			case TA_BASEPOOL_256:
				pBaseBuffer = g_TextureBasePool_256.Alloc();
				break;
			case TA_BASEPOOL_128:
				pBaseBuffer = g_TextureBasePool_128.Alloc();
				break;
			case TA_BASEPOOL_64:
				pBaseBuffer = g_TextureBasePool_64.Alloc();
				break;
			case TA_BASEPOOL_32:
				pBaseBuffer = g_TextureBasePool_32.Alloc();
				break;
			}
		}
		if ( pBaseBuffer )
		{
			// found memory!
			break;
		}

		// squeeze lru for memory
		for ( ; tcHandle != m_TextureCache.InvalidIndex(); tcHandle = m_TextureCache.Next( tcHandle ) )
		{
			if ( tcHandle == pXboxTexture->m_tcHandle )
			{
				// skip self
				continue;
			}
			
			CXboxTexture *pPurgeCandidate = (CXboxTexture *)m_TextureCache[tcHandle];

			if ( !pPurgeCandidate->m_BaseValid ||
				pPurgeCandidate->m_fAllocator != pXboxTexture->m_fAllocator ||
				pPurgeCandidate->m_nFrameCount >= nFrameCount-1 )
			{
				// only allowing eviction from the expected pool
				// using frame counter as the cheapest method to cull GPU busy resources
				continue;
			}

			byte *pBaseBits = (byte *)GetD3DTextureBasePtr( pPurgeCandidate );
			Assert( pBaseBits );
			if ( pBaseBits )
			{
				switch ( pPurgeCandidate->m_fAllocator )
				{
				case TA_BASEPOOL_1024:
					g_TextureBasePool_1024.Free( pBaseBits );
					break;
				case TA_BASEPOOL_512:
					g_TextureBasePool_512.Free( pBaseBits );
					break;
				case TA_BASEPOOL_256:
					g_TextureBasePool_256.Free( pBaseBits );
					break;
				case TA_BASEPOOL_128:
					g_TextureBasePool_128.Free( pBaseBits );
					break;
				case TA_BASEPOOL_64:
					g_TextureBasePool_64.Free( pBaseBits );
					break;
				case TA_BASEPOOL_32:
					g_TextureBasePool_32.Free( pBaseBits );
					break;
				}
			}
			pPurgeCandidate->m_bBaseAllocated = false;
			pPurgeCandidate->m_BaseValid = 0;

			if ( texture_heap_debug.GetInt() == THD_SPEW )
			{
				char szFilename[MAX_PATH];
				g_pFullFileSystem->String( pXboxTexture->m_hFilename, szFilename, sizeof( szFilename ) );
				Msg( "Evicted: %s\n", szFilename );
			}

			// retry allocation
			break;
		}

		numAttempts++;
	}

	if ( !pBaseBuffer )
	{
		// no eviction occured
		// mark which allocator failed
		s_failedAllocator[pXboxTexture->m_fAllocator] = nFrameCount;
		return false;
	}

	pXboxTexture->m_bBaseAllocated = true;
	SetD3DTextureBasePtr( pD3DTexture, pBaseBuffer );

	// setup i/o
	char szFilename[MAX_PATH];
	g_pFullFileSystem->String( pXboxTexture->m_hFilename, szFilename, sizeof( szFilename ) );

	if ( texture_heap_debug.GetInt() == THD_COLORIZE_BEFOREIO || texture_heap_debug.GetInt() == THD_COLORIZE_AFTERIO )
	{
		// colorize the base to use during the i/o latency
		V_memset( pBaseBuffer, BASE_COLOR_BEFORE, pXboxTexture->m_nBaseSize );
	}
	if ( texture_heap_debug.GetInt() == THD_SPEW )
	{
		Msg( "Queued: %s\n", szFilename );
	}

	FileAsyncRequest_t asyncRequest;
	asyncRequest.pszFilename = szFilename;
	asyncRequest.priority = -1;
	asyncRequest.flags = FSASYNC_FLAGS_ALLOCNOFREE;
	asyncRequest.pContext = (void *)pD3DTexture;
	asyncRequest.pfnCallback = IOAsyncCallback;
	g_pFullFileSystem->AsyncRead( asyncRequest, &pXboxTexture->m_hAsyncControl );

	return true;
}

//-----------------------------------------------------------------------------
// Moves to head of LRU. Allocates and queues for loading if evicted. Returns
// true if texture is resident, false otherwise.
//-----------------------------------------------------------------------------
bool CTextureHeap::TouchTexture( CXboxTexture *pXboxTexture )
{
	bool bValid = true;
	if ( pXboxTexture->m_tcHandle != INVALID_TEXTURECACHE_HANDLE )
	{
		// touch
		m_TextureCache.LinkToTail( pXboxTexture->m_tcHandle );
		bValid = ( pXboxTexture->m_BaseValid != 0 );
		if ( !bValid && !pXboxTexture->m_bBaseAllocated )
		{
			RestoreCacheableTexture( (IDirect3DBaseTexture *)pXboxTexture );
		}
		
		if ( texture_heap_debug.GetInt() == THD_COLORIZE_BEFOREIO || texture_heap_debug.GetInt() == THD_COLORIZE_AFTERIO )
		{
			// debug mode allows the render to proceed before the i/o completes
			bValid = pXboxTexture->m_bBaseAllocated;
		}
	}
	return bValid;
}

//-----------------------------------------------------------------------------
// Save file info for d3d texture restore process.
//-----------------------------------------------------------------------------
void CTextureHeap::SetCacheableTextureParams( IDirect3DBaseTexture *pD3DTexture, const char *pFilename, int mipSkipCount )
{
	if ( !IsTextureCacheManaged( pD3DTexture ) )
	{
		// wasn't setup for cacheing
		return;
	}

	if ( pD3DTexture->GetType() == D3DRTYPE_TEXTURE )
	{
		// store compact absolute filename
		((CXboxTexture *)pD3DTexture)->m_hFilename = g_pFullFileSystem->FindOrAddFileName( pFilename );
		((CXboxTexture *)pD3DTexture)->m_nMipSkipCount = mipSkipCount;
	}
}

void CTextureHeap::SpewTextureCache()
{
	Msg( "LRU:\n" );

	TextureCacheHandle_t tcHandle;
	for ( tcHandle = m_TextureCache.Head(); tcHandle != m_TextureCache.InvalidIndex(); tcHandle = m_TextureCache.Next( tcHandle ) )
	{
		CXboxTexture *pXboxTexture = (CXboxTexture *)m_TextureCache[tcHandle];

		char szFilename[MAX_PATH];
		g_pFullFileSystem->String( pXboxTexture->m_hFilename, szFilename, sizeof( szFilename ) );

		const char *pState = "???";
		if ( pXboxTexture->m_BaseValid )
		{
			pState = "Valid";
		}
		else
		{
			if ( !pXboxTexture->m_bBaseAllocated )
			{
				pState = "Evicted";
			}
			else if ( pXboxTexture->m_hAsyncControl )
			{
				pState = "Loading";
			}
		}

		Msg( "0x%8.8x (%8s) %s\n", pXboxTexture->m_nFrameCount, pState, szFilename );
	}
}

//-----------------------------------------------------------------------------
// Return the total amount of memory allocated for the base pools.
//-----------------------------------------------------------------------------
int CTextureHeap::GetCacheableHeapSize()
{
	int nTotal = 0;
	for ( int i = 0; i < TA_MAX; i++ )
	{
		switch ( i )
		{
		case TA_BASEPOOL_1024:
			nTotal += g_TextureBasePool_1024.BytesTotal();
			break;
		case TA_BASEPOOL_512:
			nTotal += g_TextureBasePool_512.BytesTotal();
			break;
		case TA_BASEPOOL_256:
			nTotal += g_TextureBasePool_256.BytesTotal();
			break;
		case TA_BASEPOOL_128:
			nTotal += g_TextureBasePool_128.BytesTotal();
			break;
		case TA_BASEPOOL_64:
			nTotal += g_TextureBasePool_64.BytesTotal();
			break;
		case TA_BASEPOOL_32:
			nTotal += g_TextureBasePool_32.BytesTotal();
			break;
		default:
			continue;
		}
	}
	return nTotal;
}

