//=========== (C) Copyright Valve, L.L.C. All rights reserved. ===========

#include "cbase.h"
#include "ui_nugget.h"

class CUiNuggetPlayerManager : public CUiNuggetBase
{
	DECLARE_NUGGET_FN_MAP( CUiNuggetPlayerManager, CUiNuggetBase );

	NUGGET_FN( GetLocalPlayer )
	{
		// Request for local player info
		int iController = args->GetInt( "index" );
		IPlayerLocal *pLocalPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
		if ( !pLocalPlayer )
			return NULL;

		return PlayerAsKeyValues( pLocalPlayer, CFmtStr( "localplayer%d", iController ) );
	}

	NUGGET_FN( JoinFriend )
	{
		// Request to join a friend
		XUID xuid = args->GetUint64( "xuid" );
		IPlayerFriend *pFriend = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetFriendByXUID( xuid );
		if ( !pFriend )
			return NULL;

		pFriend->Join();
		return NULL;
	}

	KeyValues * PlayerAsKeyValues( IPlayer *pPlayer, char const *szResultTitle = "" )
	{
		KeyValues *kv = new KeyValues( szResultTitle );
		kv->SetString( "name", pPlayer->GetName() );
		kv->SetString( "xuid", CFmtStr( "%llu", pPlayer->GetXUID() ) );
		kv->SetInt( "index", pPlayer->GetPlayerIndex() );
		kv->SetInt( "state", pPlayer->GetOnlineState() );
		return kv;
	}
};

UI_NUGGET_FACTORY_SINGLETON( CUiNuggetPlayerManager, "playermanager" );
