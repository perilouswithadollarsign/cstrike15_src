//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//


#ifndef _IPLAYER_H_
#define _IPLAYER_H_

#include "tier1/keyvalues.h"

struct UserProfileData
{
	float	reputation;
	int32	difficulty;
	int32	sensitivity;
	int32	yaxis;
	int32	vibration;
	int32	color1, color2;
	int32	action_autoaim;
	int32	action_autocenter;
	int32	action_movementcontrol;
	int32	region;
	int32	achearned;
	int32	cred;
	int32	zone;
	int32	titlesplayed;
	int32	titleachearned;
	int32	titlecred;
};

//Players are a wrapper or a networked player, as such they may not have all the information current, particularly when first created.
abstract_class IPlayer
{
public:
	enum OnlineState_t
	{
		STATE_OFFLINE,
		STATE_NO_MULTIPLAYER,
		STATE_ONLINE,
	};

public:
	//Info
	virtual XUID GetXUID() = 0;
	virtual int GetPlayerIndex() = 0;

	virtual char const * GetName() = 0;

	virtual OnlineState_t GetOnlineState() = 0;
};

abstract_class IPlayerFriend : public IPlayer
{
public:
	virtual wchar_t const * GetRichPresence() = 0;

	virtual KeyValues *GetGameDetails() = 0;
	virtual KeyValues *GetPublishedPresence() = 0;

	virtual bool IsJoinable() = 0;
	virtual void Join() = 0;
	virtual uint64 GetTitleID() = 0;
	virtual uint32 GetGameServerIP() = 0;
};

struct MatchmakingData;
class IPlayerRankingDataStore; 

abstract_class IPlayerLocal : public IPlayer
{
public:
	virtual const UserProfileData& GetPlayerProfileData() = 0;

	virtual MatchmakingData * GetPlayerMatchmakingData( void ) = 0;
	virtual void UpdatePlayerMatchmakingData( int mmDataType ) = 0;
	virtual void ResetPlayerMatchmakingData( int mmDataScope ) = 0;

	virtual const void * GetPlayerTitleData( int iTitleDataIndex ) = 0;
	virtual void UpdatePlayerTitleData( TitleDataFieldsDescription_t const *fdKey, const void *pvNewTitleData, int numNewBytes ) = 0;

	virtual void GetLeaderboardData( KeyValues *pLeaderboardInfo ) = 0;
	virtual void UpdateLeaderboardData( KeyValues *pLeaderboardInfo ) = 0;

	virtual void GetAwardsData( KeyValues *pAwardsData ) = 0;
	virtual void UpdateAwardsData( KeyValues *pAwardsData ) = 0;

	virtual void SetNeedsSave( void ) = 0;

#if defined ( _X360 )
	virtual bool IsTitleDataValid( void ) = 0;
	virtual bool IsTitleDataBlockValid( int blockId ) = 0;
	virtual void SetIsTitleDataValid( bool isValid ) = 0;
	virtual bool IsFreshPlayerProfile( void ) = 0;
	virtual void ClearBufTitleData( void ) = 0;
#endif
	virtual bool IsTitleDataStorageConnected( void ) = 0;
};

#endif
