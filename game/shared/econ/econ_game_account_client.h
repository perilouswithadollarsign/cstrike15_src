//========= Copyright (c), Valve Corporation, All rights reserved. ============//
//
// Purpose: Holds the CEconGameAccountClient object
//
// $NoKeywords: $
//=============================================================================//

#ifndef ECON_GAME_ACCOUNT_CLIENT_H
#define ECON_GAME_ACCOUNT_CLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/protobufsharedobject.h"
#include "base_gcmessages.pb.h"
#include "cstrike15_gcmessages.pb.h"

enum EGameAccountElevatedState_t
{
	k_EGameAccountElevatedState_None,							// account has no verified phone on file
	k_EGameAccountElevatedState_NotIdentifying,					// account has phone, but it's not identifying
	k_EGameAccountElevatedState_AwaitingCooldown,				// account has identifying phone, but it cannot be used to become elevated yet (cooldown on changes)
	k_EGameAccountElevatedState_Eligible,						// account is apriori eligible to become premium, just needs to ask!
	k_EGameAccountElevatedState_EligibleWithTakeover,			// account is completely eligible, just need to confirm the phone takeover
	k_EGameAccountElevatedState_Elevated,						// account is fully elevated to premium
	k_EGameAccountElevatedState_AccountCooldown,				// account has identifying phone, but it is a different phone than recently used for upgrading (so account has a cooldown)
};

//---------------------------------------------------------------------------------
// Purpose: All the account-level information that the GC tracks
//---------------------------------------------------------------------------------
class CEconGameAccountClient : public GCSDK::CProtoBufSharedObject< CSOEconGameAccountClient, k_EEconTypeGameAccountClient >
{
public:
	uint32 ComputeXpBonusFlagsNow() const;
};

// Persona data shared to the game server
class CEconPersonaDataPublic : public GCSDK::CProtoBufSharedObject < CSOPersonaDataPublic, k_EEconTypePersonaDataPublic >
{
public:
};

#endif //ECON_GAME_ACCOUNT_CLIENT_H
