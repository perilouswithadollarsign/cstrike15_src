//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: Provides names for GC message types for inter GC messages
//
//=============================================================================
#include "cbase.h"
#include "shared_gcmessages.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: A big array of message types for keeping track of their names
//-----------------------------------------------------------------------------
GCSDK::MsgInfo_t g_SharedMsgInfo[] =
{
	DECLARE_GC_MSG( k_EMsgInterGCAchievementAwarded ),
	DECLARE_GC_MSG( k_EMsgInterGCAchievementAwardedResponse ),
	DECLARE_GC_MSG( k_EMsgInterGCLoadAchievements ),
	DECLARE_GC_MSG( k_EMsgInterGCLoadAchievementResponse ),
};

void InitGCSharedMessageTypes()
{
	static GCSDK::CMessageListRegistration m_reg( g_SharedMsgInfo, Q_ARRAYSIZE(g_SharedMsgInfo) );
}