//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Insert this file into all projects using the memory system
// It will cause that project to use the shader memory allocator
//
// $NoKeywords: $
//=============================================================================//

#ifdef SN_TARGET_PS3

struct MemOverrideRawCrtFunctions_t
{
	void * (*pfn_malloc)( size_t size );
	void * (*pfn_calloc)( size_t nelem, size_t size );
	void * (*pfn_memalign)(size_t boundary, size_t size_arg);
	void * (*pfn_realloc)( void *p, size_t size_arg );
	void * (*pfn_reallocalign)( void *p, size_t size_arg, size_t boundary );
	size_t (*pfn_malloc_usable_size)( void *p );
	int (*pfn_malloc_stats)( struct malloc_managed_size *mms );
	void (*pfn_free)( void *p );
};

#endif
