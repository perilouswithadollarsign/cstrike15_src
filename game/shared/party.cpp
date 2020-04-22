//====== Copyright c, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================


#include "cbase.h"
#include "base_gcmessages.pb.h"
#include "party.h"
#include "gcsdk/accountdetails.h"

using namespace GCSDK;

#ifdef GC

void CPartyInvite::YldInitFromPlayerGroup( const GCSDK::IPlayerGroup *pPlayerGroup )
{
	SetGroupID( pPlayerGroup->GetGroupID() );
	SetSenderID( pPlayerGroup->GetLeader().ConvertToUint64() );
	SetSenderName( GGCBase()->YieldingGetPersonaName( pPlayerGroup->GetLeader(), "Unknown Player" ) );
}
#endif
