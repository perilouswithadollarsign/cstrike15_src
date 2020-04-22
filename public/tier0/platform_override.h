//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
// This file is a force-include override

#ifdef PLATFORM_OVERRIDE_MATERIALSYSTEM
#define PLATFORM_OVERRIDE_MEM_ALLOC_INTERNAL_PS3 g_pMemAllocInternalPS3Override_MaterialSystem
#endif

#ifdef PLATFORM_OVERRIDE_TIER0
#define PLATFORM_OVERRIDE_MEM_ALLOC_INTERNAL_PS3 g_pMemAllocInternalPS3Override_Tier0
#endif

#ifdef PLATFORM_OVERRIDE_MEM_ALLOC_INTERNAL_PS3

class IMemAlloc;
struct IMemAlloc_CachedInterface_t
{
	IMemAlloc_CachedInterface_t();
	IMemAlloc *m_pMemAllocCached;
};
extern IMemAlloc_CachedInterface_t PLATFORM_OVERRIDE_MEM_ALLOC_INTERNAL_PS3;
#define PLATFORM_INTERFACE_MEM_ALLOC_INTERNAL_PS3_OVERRIDE PLATFORM_OVERRIDE_MEM_ALLOC_INTERNAL_PS3.m_pMemAllocCached

#define PLATFORM_OVERRIDE_MEM_ALLOC_INTERNAL_PS3_IMPL \
	IMemAlloc_CachedInterface_t PLATFORM_OVERRIDE_MEM_ALLOC_INTERNAL_PS3 CONSTRUCT_EARLY; \
	IMemAlloc_CachedInterface_t::IMemAlloc_CachedInterface_t() \
	{ \
		m_pMemAllocCached = g_pMemAllocInternalPS3; \
	}

#endif

// Force-include delegation to platform.h
#include "platform.h"
