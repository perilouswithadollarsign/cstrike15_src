//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "basetypes.h"
#include "changeframelist.h"
#include "dt.h"
#include "utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
DEFINE_FIXEDSIZE_ALLOCATOR_MT( CChangeFrameList, 2048, CUtlMemoryPool::GROW_FAST );

//-----------------------------------------------------------------------------

CChangeFrameList::CChangeFrameList( int nProperties, int iCurTick )
{
	//determine how many buckets we need for our properties
	int nNumBuckets = ( nProperties + knBucketSize - 1 ) / knBucketSize;
	m_nNumProps = nProperties;

	m_ChangeTicks.SetCount( nProperties + nNumBuckets );
	m_ChangeTicks.FillWithValue( iCurTick );
}

CChangeFrameList::~CChangeFrameList()
{
}

void CChangeFrameList::Release()
{
	delete this;
}

CChangeFrameList::CChangeFrameList( const CChangeFrameList &rhs )
{
	m_ChangeTicks = rhs.m_ChangeTicks;
	m_nNumProps = rhs.m_nNumProps;
}

CChangeFrameList *CChangeFrameList::Copy()
{
	CChangeFrameList *pRet = new CChangeFrameList( *this );
	return pRet;
}

void CChangeFrameList::SetChangeTick( const int* RESTRICT pProps, int nNumProps, const int iTick )
{
	//avoid loads inside the array by the compiler thinking they could overlap
	int* pBuckets		= m_ChangeTicks.Base() + m_nNumProps;
	for ( int i=0; i < nNumProps; i++ )
	{
		//update our tick, and the parent bucket for the tick
		m_ChangeTicks[ pProps[i] ] = iTick;
		pBuckets[ pProps[i] / knBucketSize ] = iTick;		
	}
}

