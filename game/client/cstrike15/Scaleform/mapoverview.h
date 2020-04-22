//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#ifndef SFOVERVIEWMAP_H_
#define SFOVERVIEWMAP_H_

#include "hud.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "HUD/sfhudflashinterface.h"
#include "HUD/sfhudradar.h"
#include "c_cs_hostage.h"
#include "usermessages.h"

#define MAX_GRENADES 30

class SFMapOverview : public SFHudFlashInterface
{
	// this manages the display of the players and hostages
	// in the radar

protected:
	enum ICON_PACK_TYPE
	{
		ICON_PACK_PLAYER,
		ICON_PACK_HOSTAGE,
		ICON_PACK_GRENADES,
		ICON_PACK_DEFUSER,
	};

	enum
	{
		R_BELOW = 0,
		R_SAMELEVEL = 1,
		R_ABOVE = 2,
	};

	// each enum represents an icon that this class is managing
	enum PLAYER_ICON_INDICES
	{
		PI_GRENADE_HE,
		PI_GRENADE_FLASH,
		PI_GRENADE_SMOKE,
		PI_GRENADE_MOLOTOV,
		PI_GRENADE_DECOY,

		PI_DEFUSER,
		PI_ABOVE,
		PI_BELOW,
		PI_FLASHED,
		PI_PLAYER_NUMBER,
		PI_PLAYER_NAME_CT,
		PI_PLAYER_NAME_T,

		PI_FIRST_ROTATED,
		PI_PLAYER_INDICATOR = PI_FIRST_ROTATED,
		PI_SPEAKING,
		PI_HOSTAGE_MOVING,

		PI_CT,
		PI_CT_DEAD,
		PI_CT_GHOST,

		PI_T,
		PI_T_DEAD,
		PI_T_GHOST,

		PI_ENEMY,
		PI_ENEMY_DEAD,
		PI_ENEMY_GHOST,

		PI_HOSTAGE,
		PI_HOSTAGE_DEAD,
		PI_HOSTAGE_GHOST,

		PI_DIRECTION_INDICATOR,
		PI_MUZZLE_FLASH,

		PI_LOWHEALTH,
		PI_SELECTED,

		PI_NUM_ICONS
	};

	class SFMapOverviewIconPackage
	{

	public:
		SFMapOverviewIconPackage();
		~SFMapOverviewIconPackage();

		// zero all the internal variables
		void ClearAll( void );

		// get handles to the icons which will all be children
		// of the iconPackage handle
		void Init( IScaleformUI* pui, SFVALUE iconPackage );

		// release all the handles, and clear all the variables
		// used when removing players or changing maps
		void NukeFromOrbit( SFMapOverview* pSFUI );

		// reset all variables to their start of round values
		void StartRound( void );

		// set the states for this player
		void SetIsPlayer( bool value );
		void SetIsSpeaking ( bool value );
		void SetIsOffMap( bool value );
		void SetIsLowHealth( bool value );
		void SetIsSelected( bool value );	
		void SetIsFlashed( bool value );
		void SetIsFiring( bool value );
		void SetIsAboveOrBelow( int value );
		void SetIsMovingHostage( bool value );
		void SetIsDead( bool value );
		void SetIsRescued( bool value );
		void SetPlayerTeam( int team );
		void SetIsSpotted( bool value );
		void SetAlpha( float newAlpha );
		void SetIsOnLocalTeam( bool value );
		void SetIsControlledBot( void );
		void SetIsDefuse( bool bValue );
		void SetGrenadeType( int value );
		void SetGrenadeExpireTime( float value );

		// given the current set of states, decide which
		// icons should be shown and which should be hidden
 		void SetupIconsFromStates( void );

		// each bit in newFlags represents the visibility of one of the
		// icons in the PLAYER_ICON_INDICES.  If the bit is on, the icon
		// is shown.
		void SetVisibilityFlags( uint64 newFlags );

		bool IsHostageType( void ) { return m_IconPackType == ICON_PACK_HOSTAGE;}
		bool IsGrenadeType( void ) { return m_IconPackType == ICON_PACK_GRENADES;}
		bool IsPlayerType( void ) { return m_IconPackType == ICON_PACK_PLAYER;}
		bool IsDefuserType( void ) { return m_IconPackType == ICON_PACK_DEFUSER;}

		bool IsVisible( void );


	public:
		// pointer to scaleform
		IScaleformUI* m_pScaleformUI;

		// the parent for all the icons
		SFVALUE m_IconPackage;
		SFVALUE	m_IconPackageRotate;

		// the handles for all the icons listed in PLAYER_ICON_INDICES
		SFVALUE m_Icons[PI_NUM_ICONS];

		// the location and position of this player/hostage
		// only updated when the player is spotted
		Vector	m_Position;	// current x,y pos
		QAngle	m_Angle;		// view origin 0..360

		// ignore visibility updates until a little time has passed
		// this keeps track of when the round started
		float m_fRoundStartTime;

		// the time at which this player/hostage died ( or was rescued )
		// used to calculate the alpha of the X icon.
		float m_fDeadTime;

		// the time at which the player / hostage was last spotted
		// used to fade out the ? icon
		float m_fGhostTime;

		// the alpha currently used to display all icons
		// used to lazy update the actual scaleform value
		float m_fCurrentAlpha;


		// each bit represents one of the PLAYER_ICON_INDICES
		// used to lazy update the visibility of the icons in scaleform
		uint64 m_iCurrentVisibilityFlags;

		// the index of this player/hostage in the radar.
		// used to create the instance name of the icon package in flash
		int m_iIndex;

		// set from the player objects UserID or EntityID ( for the hostages ). Lets us find the radar
		// object that represents a player / hostage
		int m_iEntityID;


		// state variables used to keep track of the player / hostage state
		// so we know which icon( s ) to show

		int	m_Health;		// 0..100, 7 bit

		wchar_t m_wcName[MAX_PLAYER_NAME_LENGTH+1];


		// the base icon for the player
		int m_iPlayerType; // will be PI_CT, PI_T, or PI_HOSTAGE

		int m_nAboveOrBelow;//			R_BELOW = 0,R_SAMELEVEL = 1,R_ABOVE = 2,

		int m_nGrenadeType;
		float m_fGrenExpireTime;

		ICON_PACK_TYPE m_IconPackType;

		bool m_bIsActive : 1;
		bool m_bOffMap : 1;
		bool m_bIsLowHealth : 1;
		bool m_bIsSelected : 1;
		bool m_bIsFlashed : 1;
		bool m_bIsFiring : 1;
		bool m_bIsPlayer : 1;
		bool m_bIsSpeaking : 1;
		bool m_bIsDead : 1;
		bool m_bIsMovingHostage : 1;
		bool m_bIsSpotted : 1;
		bool m_bIsRescued : 1;
		bool m_bIsOnLocalTeam : 1;
		bool m_bIsDefuser : 1;

		// don't put anything new after the bitfields or suffer the Wrath of the Compiler!
	};


	// this little class manages the display of the hostage
	// indicators in the panel

	class SFMapOverviewHostageIcons
	{
	public:
		enum HOSTAGE_ICON_INDICES
		{
			HI_DEAD,
			HI_RESCUED,
			HI_ALIVE,
			HI_TRANSIT,
			HI_NUM_ICONS,

			HI_UNUSED = HI_NUM_ICONS,
		};
	public:

		SFMapOverviewHostageIcons();
		~SFMapOverviewHostageIcons();

		void Init( IScaleformUI* scaleformui, SFVALUE iconPackage );
		void ReleaseHandles( SFMapOverview* pradar );

		void SetStatus( int status );

	public:
 		IScaleformUI* m_pScaleformUI;

		// the parent object of all the icons
		SFVALUE m_IconPackage;

		// the icons which represent each of the HOSTAGE_ICON_INDICES
		SFVALUE m_Icons[HI_NUM_ICONS];

		// the index of the icon that is currently shown
		int m_iCurrentIcon;

	};



	// this just keeps track of the bombzone and hostagezone
	// icons that are shown on the radar

	struct SFMapOverviewGoalIcon
	{
		Vector m_Position;
		SFVALUE m_Icon;
	};


public:
	explicit SFMapOverview( const char *value );
	virtual ~SFMapOverview();

	// These overload the CHudElement class
	virtual void ProcessInput( void );
	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual void SetActive( bool bActive );
	virtual void Init( void );
	virtual bool ShouldDraw( void );
	bool CanShowOverview( void );

	// these overload the ScaleformFlashInterfaceMixin class
	virtual void FlashLoaded( void );
	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );
	void MapLoaded( SCALEFORM_CALLBACK_ARGS_DECL );
	void ToggleOverviewMap( SCALEFORM_CALLBACK_ARGS_DECL );

	// overloads for the CGameEventListener class
	virtual void FireGameEvent( IGameEvent *event );

	bool MsgFunc_ProcessSpottedEntityUpdate( const CCSUsrMsg_ProcessSpottedEntityUpdate &msg );

	void UpdateAllPlayerNamesAndNumbers( void );
	void UpdatePlayerNameAndNumber( SFMapOverviewIconPackage* pPackage );

	void ShowMapOverview( bool value );
	bool IsMapOverviewShown( void ) {return (m_bShowMapOverview && m_bVisible);}

	void AllowMapDrawing( SCALEFORM_CALLBACK_ARGS_DECL );
//	void GetNavPath( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetWorldDistance( SCALEFORM_CALLBACK_ARGS_DECL );

	void RefreshGraphs( void );

protected:
	void ResetRadar( bool bResetGlobalStates = true );

	void ResetForNewMap( void );
	void ResetRound( void );
 	void SetMap( const char* pMapName );
 	void WorldToRadar( const Vector& ptin, Vector& ptout );
	void RadarToWorld( const Vector& ptin, Vector& ptout );
 	void LazyCreateGoalIcons( void );
 	void FlashLoadMap( const char* pMapName );

	void InitIconPackage( SFMapOverviewIconPackage* pPlayer, int iAbsoluteIndex, ICON_PACK_TYPE packType );
	void RemoveIconPackage( SFMapOverviewIconPackage* pPlayer );

	SFMapOverviewIconPackage* CreatePlayer( int index );
	void ResetPlayer( int index );
	void RemovePlayer( int index );

	SFMapOverviewIconPackage* CreateHostage( int index );
	void ResetHostage( int index );
	void RemoveHostage( int index );
	void RemoveStaleHostages( void );

	SFMapOverviewIconPackage* CreateGrenade( int entityID, int nGrenadeType );
	void RemoveAllGrenades( void );
	void RemoveGrenade( int index );

	SFMapOverviewIconPackage * CreateDefuser( int nEntityID );
	SFMapOverviewIconPackage * GetDefuser( int nEntityID, bool bCreateIfNotFound = false );
	void SetDefuserPos( int nEntityID, int x, int y, int z, int a );
	void UpdateAllDefusers( void );
	void RemoveAllDefusers( void );
	void RemoveDefuser( int index );

	bool LazyUpdateIconArray( SFMapOverviewIconPackage* pArray, int lastIndex );
 	virtual bool LazyCreateIconPackage( SFMapOverviewIconPackage* pPackage );

	void LazyCreatePlayerIcons( void );

	void SetPlayerTeam( int index, int team );

	int GetPlayerIndexFromUserID( int userID );
	int GetHostageIndexFromHostageEntityID( int entityID );
	int GetGrenadeIndexFromEntityID( int entityID );
	int GetDefuseIndexFromEntityID( int nEntityID );

	void ApplySpectatorModes( void );

 	void PositionRadarViewpoint( void );
 	void PlaceGoalIcons( void );
 	void Show( bool show );
	void ShowPanel( bool bShow );
 	void PlacePlayers();
 	void PlaceHostages();
 	void SetIconPackagePosition( SFMapOverviewIconPackage* pPackage );
 	void UpdateMiscIcons( void );
	void SetVisibilityFlags( uint64 newFlags );
	void SetupIconsFromStates( void );
	bool IsEnemyCloseEnoughToShow( Vector vecEnemyPos );

	void SetLocationText( wchar_t *newText );	

	void ResetRoundVariables( bool bResetGlobalStates = true );

	void UpdateGrenades( void );

	SFMapOverviewIconPackage* GetRadarPlayer( int index );
	SFMapOverviewIconPackage* GetRadarHostage( int index );
	SFMapOverviewIconPackage* GetRadarGrenade( int index );
	SFMapOverviewIconPackage* GetRadarDefuser( int index );
	SFMapOverviewIconPackage* GetRadarHeight( int index );

	CUserMessageBinder m_UMCMsgProcessSpottedEntityUpdate;

protected:

	// these are the icons used individually by the radar and panel
	enum RADAR_ICON_INDICES
	{
		RI_BOMB_IS_PLANTED,
		RI_BOMB_IS_PLANTED_MEDIUM,
		RI_BOMB_IS_PLANTED_FAST,		
		RI_IN_HOSTAGE_ZONE,
		RI_DASHBOARD,
		RI_BOMB_ICON_PLANTED,
		RI_BOMB_ICON_DROPPED,
		RI_BOMB_ICON_BOMB_CT,
		RI_BOMB_ICON_BOMB_T,
		RI_BOMB_ICON_BOMB_ABOVE,
		RI_BOMB_ICON_BOMB_BELOW,
		RI_BOMB_ICON_PACKAGE,
		RI_DEFUSER_ICON_DROPPED,
		RI_DEFUSER_ICON_PACKAGE,

		RI_NUM_ICONS,
	};

	enum
	{
		MAX_BOMB_ZONES = 2,
	};
	

	// this holds the names and indexes of the messages we receive so that
	// we don't have to do a whole bunch of string compares to find them
	static CUtlMap<const char*, int> m_messageMap;

	// these are used to scale world coordinates to radar coordinates
	Vector m_MapOrigin;
	float m_fMapSize;
	float m_fMapScale;
	float m_fPixelToRadarScale;
	float m_fWorldToPixelScale;
	float m_fWorldToRadarScale;


	// this is center of the radar in world and map coordinates
	Vector m_RadarViewpointWorld;
	Vector m_RadarViewpointMap;
	float  m_RadarRotation;


	// the current position of the bomb
	Vector m_BombPosition;

	// the last time the bomb was seen.  Used to fade
	// out the bomb icon after it has dropped out of sight
	float m_fBombSeenTime;
	float m_fBombAlpha;


	// the current position of the defuser
	Vector m_DefuserPosition;

	// the last time the defuser was seen.  Used to fade
	// out the defuser icon after it has dropped out of sight
	float m_fDefuserSeenTime;
	float m_fDefuserAlpha;

	// a bitmap of the icons that are currently beeing shown.
	// each bit corresponds to one the RADAR_ICON_INDICES
	uint64 m_iCurrentVisibilityFlags;

	// the handles to the RADAR_ICON_INDICES icons
	SFVALUE m_AllIcons[RI_NUM_ICONS];

	// Background panel
	SFVALUE m_BackgroundPanel;

	// handles to the radar movie clips in flash.
	// there is a rotation and a translation layer each for the icons and for the background map.
	// The map and the icons have separate layers because the map is behind a mask layer, and the
	// icons are not.

	// the root of the entire radar and dashboard
	SFVALUE m_RadarModule;

	// the root of the radar part of the module
	SFVALUE m_Radar;

	// the layers that handle the icons
	SFVALUE m_IconTranslation;
	SFVALUE m_IconRotation;

	// the layers that handle the map
	SFVALUE m_MapRotation;
	SFVALUE m_MapTranslation;

	// handles to the actual bomb zone and hostage icons that are defined
	// in the flash file
	SFVALUE m_HostageZoneIcons[MAX_HOSTAGE_RESCUES];
	SFVALUE m_BombZoneIcons[MAX_BOMB_ZONES];

	// "handle" to the text that holds the current location
	ISFTextObject* m_LocationText;

	// the last index of an active player in the m_Players array
	int m_iLastPlayerIndex;

	// the last index of an active hostage in the m_Hostages array
	int m_iLastHostageIndex;

	// the last index of an active decoy in the m_Grenades array
	int m_iLastGrenadeIndex;

	// the last index of an active defuser in the m_Defuser array
	int m_iLastDefuserIndex;

	// keeps the state information and icon handles for the players
	SFMapOverviewIconPackage m_Players[MAX_PLAYERS];

	// keeps the state information and icon handles for the hostages
	SFMapOverviewIconPackage m_Hostages[MAX_HOSTAGES];

	// keeps the state information and icon handles for the decoys
	SFMapOverviewIconPackage m_Grenades[MAX_GRENADES];

	// keeps the state information and icon handles for the decoys
	SFMapOverviewIconPackage m_Defusers[MAX_PLAYERS];

	// the handles to the hostage status icons that appear beneath the dashboard
	SFMapOverviewHostageIcons m_HostageStatusIcons[MAX_HOSTAGES];

	// a goal icon is either a bomb-area or a hostage-area icon
	// This array holds the positions / handles of the ones that are active for the current map
	int m_iNumGoalIcons;
	SFMapOverviewGoalIcon m_GoalIcons[MAX_HOSTAGE_RESCUES + MAX_BOMB_ZONES];

	// the current observer mode. Figures into the placement of the center of the radar
	// and a few other things
	int m_iObserverMode;

	// there is a loaded and a desired so that we don't load the same map twice, and so that we
	// can request that a map be loaded before the flash stuff is able to actually load it.
	char m_cLoadedMapName[MAX_MAP_NAME+1];
	char m_cDesiredMapName[MAX_MAP_NAME+1];

	// the name of our current location
	wchar_t m_wcLocationString[MAX_LOCATION_TEXT_LENGTH+1];

// 	// keeps track of weather flash is ready or not and if it's currently being loaded
	bool m_bFlashLoading : 1;
	bool m_bFlashReady : 1;

	// this is set by a con command to hide the whole radar
 	bool m_bShowMapOverview : 1;
// 
// 	// set to true in spectator mode if we're not in pro mode
 	bool m_bShowAll : 1;
// 
	// keep track of whether we've already gotten all the goal icons and player icons from the
	// flash file.  This is necessary because some of the level information is loaded before
	// flash is ready
	bool m_bGotGoalIcons : 1;
	bool m_bGotPlayerIcons : 1;

	// state information about which icons should be displayed	
	bool m_bShowingHostageZone : 1;
	bool m_bBombPlanted : 1;
	bool m_bBombDropped : 1;
	bool m_bBombDefused : 1;
	bool m_bBombExploded : 1;
	bool m_bShowBombHighlight : 1;
	bool m_bShowingDashboard : 1;

	bool m_bBombIsSpotted : 1;			
	int	 m_nBombEntIndex;

	bool m_bTrackDefusers;

	bool m_bVisible;

	// entities spotted last ProcessSpottedEntityUpdate
	CBitVec<MAX_EDICTS> m_EntitySpotted;
};

#endif /* SFOVERVIEWMAP_H_ */
