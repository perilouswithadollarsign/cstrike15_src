//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Weapon data file parsing, shared by game & client dlls.
//
// $NoKeywords: $
//=============================================================================//

#ifndef WEAPON_PARSE_H
#define WEAPON_PARSE_H
#ifdef _WIN32
#pragma once
#endif

#include "shareddefs.h"
#include "GameEventListener.h"
#include "tier1/utlsortvector.h"
#include "gamestringpool.h"

#ifdef CLIENT_DLL
#define CEconItemView C_EconItemView
#endif

class IFileSystem;
class CEconItemView;

typedef unsigned short WEAPON_FILE_INFO_HANDLE;


// -----------------------------------------------------------
// Weapon sound types
// Used to play sounds defined in the weapon's classname.txt file
// This needs to match pWeaponSoundCategories in weapon_parse.cpp
// ------------------------------------------------------------
enum WeaponSound_t {
	EMPTY,
	SINGLE,
	SINGLE_ACCURATE,
	SINGLE_NPC,
	WPN_DOUBLE, // Can't be "DOUBLE" because windows.h uses it.
	DOUBLE_NPC,
	BURST,
	RELOAD,
	RELOAD_NPC,
	MELEE_MISS,
	MELEE_HIT,
	MELEE_HIT_WORLD,
	SPECIAL1,
	SPECIAL2,
	SPECIAL3,
	TAUNT,
	NEARLYEMPTY,
	FAST_RELOAD,

	// Add new shoot sound types here

	NUM_SHOOT_SOUND_TYPES,
}; 

int GetWeaponSoundFromString( const char *pszString );

#define MAX_SHOOT_SOUNDS	16			// Maximum number of shoot sounds per shoot type

#define MAX_WEAPON_STRING	80
#define MAX_WEAPON_PREFIX	16
#define MAX_WEAPON_AMMO_NAME		32

#define WEAPON_PRINTNAME_MISSING "!!! Missing printname on weapon"


class CHudTexture;
class KeyValues;

struct WeaponInfoLookup
{
	size_t m_nWeaponParseDataOffset;
	_fieldtypes m_fieldType;
	CGameString m_iszAttribClassName;

	WeaponInfoLookup( void ) {}
	WeaponInfoLookup( size_t offset, _fieldtypes p_fieldType, const char* szAttribClassName );
	WeaponInfoLookup( const WeaponInfoLookup &WepInfoLookup );
};


class CWeaponInfoLookupListLess
{
public:
	bool Less( WeaponInfoLookup * const &src1, WeaponInfoLookup * const &src2, void *pCtx )
	{
		if ( src1->m_iszAttribClassName.Get() < src2->m_iszAttribClassName.Get() )
			return true;

		return false;
	}
};

//-----------------------------------------------------------------------------
// Purpose: Contains the data read from the weapon's script file. 
// It's cached so we only read each weapon's script file once.
// Each game provides a CreateWeaponInfo function so it can have game-specific
// data (like CS move speeds) in the weapon script.
//-----------------------------------------------------------------------------
class FileWeaponInfo_t
{
public:

	FileWeaponInfo_t();
	virtual ~FileWeaponInfo_t() {}
	
	// Each game can override this to get whatever values it wants from the script.
	virtual void Parse( KeyValues *pKeyValuesData, const char *szWeaponName );

	virtual void RefreshDynamicParameters() {};
	
public:	
	bool					bParsedScript;
	bool					bLoadedHudElements;

// SHARED
	char					szClassName[MAX_WEAPON_STRING];
	char					szPrintName[MAX_WEAPON_STRING];			// Name for showing in HUD, etc.

	int GetIndexofAttribute( string_t iszAttribClassName ) const;
	static CUtlSortVector< WeaponInfoLookup*, CWeaponInfoLookupListLess > ms_vecWeaponInfoLookup;

protected:
	char					szViewModel[MAX_WEAPON_STRING];			// View model of this weapon
	char					szWorldModel[MAX_WEAPON_STRING];		// Model of this weapon seen carried by the player
	char					szAmmo1[MAX_WEAPON_AMMO_NAME];			// "primary" ammo type
	char					szWorldDroppedModel[MAX_WEAPON_STRING];

	static bool				ms_bWeaponInfoLookupInitialized;

public:
	char					szAnimationPrefix[MAX_WEAPON_PREFIX];	// Prefix of the animations that should be used by the player carrying this weapon
	int						iSlot;									// inventory slot.
	int						iPosition;								// position in the inventory slot.
	int						iMaxClip1;								// max primary clip size (-1 if no clip)
	int						iMaxClip2;								// max secondary clip size (-1 if no clip)
	int						iDefaultClip1;							// amount of primary ammo in the gun when it's created
	int						iDefaultClip2;							// amount of secondary ammo in the gun when it's created
	int						iWeight;								// this value used to determine this weapon's importance in autoselection.
	int						iRumbleEffect;							// Which rumble effect to use when fired? (xbox)
	bool					bAutoSwitchTo;							// whether this weapon should be considered for autoswitching to
	bool					bAutoSwitchFrom;						// whether this weapon can be autoswitched away from when picking up another weapon or ammo
	int						iFlags;									// miscellaneous weapon flags

	char					szAmmo2[MAX_WEAPON_AMMO_NAME];			// "secondary" ammo type
	char					szAIAddOn[MAX_WEAPON_STRING];			// addon that this weapon can become

	// Sound blocks
	char					aShootSounds[NUM_SHOOT_SOUND_TYPES][MAX_WEAPON_STRING];	

private:
	int						iAmmoType;
	int						iAmmo2Type;	

public:

	bool					m_bMeleeWeapon;		// Melee weapons can always "fire" regardless of ammo.

	// This tells if the weapon was built right-handed (defaults to true).
	// This helps cl_righthand make the decision about whether to flip the model or not.
	bool					m_bBuiltRightHanded;
	bool					m_bAllowFlipping;	// False to disallow flipping the model, regardless of whether
												// it is built left or right handed.



	virtual int		GetPrimaryClipSize( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const { return 0; }
	virtual int		GetSecondaryClipSize( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const { return 0; }
	virtual int		GetDefaultPrimaryClipSize( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const { return 0; }
	virtual int		GetDefaultSecondaryClipSize( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const{ return 0; }
	virtual int		GetPrimaryReserveAmmoMax( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const{ return 0; }
	virtual int		GetSecondaryReserveAmmoMax( const CEconItemView* pWepView = NULL, int nAlt = 0, float flScale = 1.0f ) const{ return 0; }

	const char* GetWorldModel( const CEconItemView* pWepView = NULL, int iTeam = 0 ) const;
	const char* GetViewModel( const CEconItemView* pWepView = NULL, int iTeam = 0 ) const;
	const char* GetWorldDroppedModel( const CEconItemView* pWepView = NULL, int iTeam = 0 ) const;
	const char* GetPrimaryAmmo( const CEconItemView* pWepView = NULL ) const;
	
	int GetPrimaryAmmoType( const CEconItemView* pWepView = NULL ) const;

// CLIENT DLL
	// Sprite data, read from the data file
	int						iSpriteCount;
	CHudTexture				*iconActive;
	CHudTexture	 			*iconInactive;
	CHudTexture 			*iconAmmo;
	CHudTexture 			*iconAmmo2;
	CHudTexture 			*iconCrosshair;
	CHudTexture 			*iconAutoaim;
	CHudTexture 			*iconZoomedCrosshair;
	CHudTexture 			*iconZoomedAutoaim;
	CHudTexture				*iconSmall;

// TF2 specific
	bool					bShowUsageHint;							// if true, then when you receive the weapon, show a hint about it

// SERVER DLL

};

WEAPON_FILE_INFO_HANDLE LookupWeaponInfoSlot( const char *name );
FileWeaponInfo_t *GetFileWeaponInfoFromHandle( WEAPON_FILE_INFO_HANDLE handle );
WEAPON_FILE_INFO_HANDLE GetInvalidWeaponInfoHandle( void );

void PrecacheFileWeaponInfoDatabase();


// 
// Read a possibly-encrypted KeyValues file in. 
// If pICEKey is NULL, then it appends .txt to the filename and loads it as an unencrypted file.
// If pICEKey is non-NULL, then it appends .ctx to the filename and loads it as an encrypted file.
//
// (This should be moved into a more appropriate place).
//
KeyValues* ReadEncryptedKVFile( IFileSystem *filesystem, const char *szFilenameWithoutExtension, const unsigned char *pICEKey, bool bForceReadEncryptedFile = false );


// Each game implements this. It can return a derived class and override Parse() if it wants.
extern FileWeaponInfo_t* CreateWeaponInfo();

extern void LoadEquipmentData();

class CWeaponDatabase : public CAutoGameSystem, public CGameEventListener
{
public:
	CWeaponDatabase();

	void Reset();
	bool LoadManifest();
	void PrecacheAllWeapons();
	void RefreshAllWeapons();

	WEAPON_FILE_INFO_HANDLE FindWeaponInfo( const char *name );
	FileWeaponInfo_t *GetFileWeaponInfoFromHandle( WEAPON_FILE_INFO_HANDLE handle );

protected:
	friend void LoadEquipmentData();

	virtual bool Init();

	WEAPON_FILE_INFO_HANDLE FindOrCreateWeaponInfo( const char *name );
	bool LoadWeaponDataFromFile( IFileSystem* filesystem, const char *szWeaponName, const unsigned char *pICEKey );
	void FireGameEvent( IGameEvent *event );

private:
	CUtlDict< FileWeaponInfo_t*, unsigned short > m_WeaponInfoDatabase;
	bool m_bPreCached;
};

extern CWeaponDatabase g_WeaponDatabase;


#endif // WEAPON_PARSE_H
