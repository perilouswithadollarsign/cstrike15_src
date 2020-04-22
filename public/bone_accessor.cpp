//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "bone_accessor.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

bool CBoneAccessor::isBoneAvailableForRead( int iBone ) const
{
	if ( m_pAnimating )
	{
		CStudioHdr *pHdr = m_pAnimating->GetModelPtr();
		if ( pHdr )
		{
			return ( pHdr->boneFlags( iBone ) & m_ReadableBones ) != 0;
		}
	}

	return false;
}

bool CBoneAccessor::isBoneAvailableForWrite( int iBone ) const
{
	if ( m_pAnimating )
	{
		CStudioHdr *pHdr = m_pAnimating->GetModelPtr();
		if ( pHdr )
		{
			// double check consistency
			// !!! DbgAssert( pHdr->pBone( iBone )->flags == pHdr->boneFlags( iBone ) );
			return ( pHdr->boneFlags( iBone ) & m_WritableBones ) != 0;
		}
	}

	return false;
}


#if defined( CLIENT_DLL ) && defined( _DEBUG )

	void CBoneAccessor::SanityCheckBone( int iBone, bool bReadable ) const
	{
		if ( !m_pAnimating )
		{
			return;
		}

		if ( bReadable )
		{
			Assert( isBoneAvailableForRead( iBone ) );
		}
		else
		{
			Assert( isBoneAvailableForWrite( iBone ) );
		}
	}

#endif

