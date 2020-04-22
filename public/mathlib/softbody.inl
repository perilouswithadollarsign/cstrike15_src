//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef MATHLIB_SOFTBODY_INL_HDR
#define MATHLIB_SOFTBODY_INL_HDR

#include "mathlib/softbodyenvironment.h"
inline void CSoftbody::Shutdown()
{
	m_pEnvironment->Unregister( this );
	MemAlloc_FreeAligned( m_pParticles );
	m_StickyBuffer.Clear();
}

#endif // MATHLIB_SOFTBODY_INL_HDR
