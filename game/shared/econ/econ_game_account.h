//========= Copyright (c), Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CEconGameAccount object
//
// $NoKeywords: $
//=============================================================================//

#ifndef ECON_GAME_ACCOUNT_H
#define ECON_GAME_ACCOUNT_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/schemasharedobject.h"
#include "rtime.h"

enum
{
	kGameAccountFlags_ConvertedUniques				= 1 << 0,
	kGameAccountFlags_MadeFirstPurchase				= 1 << 1,
	kGameAccountFlags_ConvertItemFlagsToOrigin		= 1 << 2,
	kGameAccountFlags_ConvertPackageItemGrants		= 1 << 3,
	kGameAccountFlags_RemoveCafeOrSchoolItems		= 1 << 4,
	kGameAccountFlags_CleanupItemNames				= 1 << 5,
	kGameAccountFlags_NeedToChooseMostHelpfulFriend	= 1 << 6,
	kGameAccountFlags_DONT_USE_THIS_BUI_LIES		= 1 << 7,	// some accounts might have this set! it used to be the "needs to thank a friend" bit
	kGameAccountFlags_OwnedGameServersDisabled		= 1 << 8,
	kGameAccountFlags_UpdatedEquippedSlots			= 1 << 9,
	kGameAccountFlags_MadeFirstWebPurchase			= 1 << 10,	
};

//---------------------------------------------------------------------------------
// Purpose: All the account-level information that the GC tracks
//---------------------------------------------------------------------------------
class CEconGameAccount : public GCSDK::CSchemaSharedObject< CSchGameAccount, k_EEconTypeGameAccount >
{
#ifdef GC_DLL
	DECLARE_CLASS_MEMPOOL( CEconGameAccount );
#endif

public:
	CEconGameAccount()  {}
	CEconGameAccount( uint32 unAccountID ) 
	{
		Obj().m_unAccountID = unAccountID;
	}
};

#endif //ECON_GAME_ACCOUNT_H
