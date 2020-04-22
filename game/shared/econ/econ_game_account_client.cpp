//========= Copyright (c), Valve Corporation, All rights reserved. ============//
//
// Purpose: Code for the CEconGameAccountClient object
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "econ_game_account_client.h"
#if defined( CLIENT_DLL )
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


uint32 CEconGameAccountClient::ComputeXpBonusFlagsNow() const
{
	/** Removed for partner depot **/
	return 0;
}
