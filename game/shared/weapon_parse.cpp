//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Weapon data file parsing, shared by game & client dlls.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include <keyvalues.h>
#include <tier0/mem.h>
#include "filesystem.h"
#include "utldict.h"
#include "ammodef.h"
#include "util_shared.h"
#include "weapon_parse.h"
#include "econ_item_view.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CWeaponDatabase g_WeaponDatabase;

extern void LoadEquipmentData();

// The sound categories found in the weapon classname.txt files
// This needs to match the WeaponSound_t enum in weapon_parse.h
#if !defined(_STATIC_LINKED) || defined(CLIENT_DLL)
const char *pWeaponSoundCategories[ NUM_SHOOT_SOUND_TYPES ] = 
{
	"empty",
	"single_shot",
	"single_shot_accurate",
	"single_shot_npc",
	"double_shot",
	"double_shot_npc",
	"burst",
	"reload",
	"reload_npc",
	"melee_miss",
	"melee_hit",
	"melee_hit_world",
	"special1",
	"special2",
	"special3",
	"taunt",
	"nearlyempty",
	"fastreload"
};
#else
extern const char *pWeaponSoundCategories[ NUM_SHOOT_SOUND_TYPES ];
#endif

int GetWeaponSoundFromString( const char *pszString )
{
	for ( int i = EMPTY; i < NUM_SHOOT_SOUND_TYPES; i++ )
	{
		if ( !Q_stricmp(pszString,pWeaponSoundCategories[i]) )
			return (WeaponSound_t)i;
	}
	return -1;
}


// Item flags that we parse out of the file.
typedef struct
{
	const char *m_pFlagName;
	int m_iFlagValue;
} itemFlags_t;
#if !defined(_STATIC_LINKED) || defined(CLIENT_DLL)
itemFlags_t g_ItemFlags[8] =
{
	{ "ITEM_FLAG_SELECTONEMPTY",	ITEM_FLAG_SELECTONEMPTY },
	{ "ITEM_FLAG_NOAUTORELOAD",		ITEM_FLAG_NOAUTORELOAD },
	{ "ITEM_FLAG_NOAUTOSWITCHEMPTY", ITEM_FLAG_NOAUTOSWITCHEMPTY },
	{ "ITEM_FLAG_LIMITINWORLD",		ITEM_FLAG_LIMITINWORLD },
	{ "ITEM_FLAG_EXHAUSTIBLE",		ITEM_FLAG_EXHAUSTIBLE },
	{ "ITEM_FLAG_DOHITLOCATIONDMG", ITEM_FLAG_DOHITLOCATIONDMG },
	{ "ITEM_FLAG_NOAMMOPICKUPS",	ITEM_FLAG_NOAMMOPICKUPS },
	{ "ITEM_FLAG_NOITEMPICKUP",		ITEM_FLAG_NOITEMPICKUP }
};
#else
extern itemFlags_t g_ItemFlags[7];
#endif



#ifdef _DEBUG
// used to track whether or not two weapons have been mistakenly assigned the wrong slot
bool g_bUsedWeaponSlots[MAX_WEAPON_SLOTS][MAX_WEAPON_POSITIONS] = { 0 };

#endif

// Find a weapon slot, assuming the weapon's data has already been loaded.
WEAPON_FILE_INFO_HANDLE LookupWeaponInfoSlot( const char *name )
{
	return g_WeaponDatabase.FindWeaponInfo( name );
}

FileWeaponInfo_t *GetFileWeaponInfoFromHandle( WEAPON_FILE_INFO_HANDLE handle )
{
	return g_WeaponDatabase.GetFileWeaponInfoFromHandle( handle );
}

void PrecacheFileWeaponInfoDatabase()
{
	g_WeaponDatabase.PrecacheAllWeapons();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : WEAPON_FILE_INFO_HANDLE
//-----------------------------------------------------------------------------
WEAPON_FILE_INFO_HANDLE GetInvalidWeaponInfoHandle( void )
{
	return (WEAPON_FILE_INFO_HANDLE)-1;
}


KeyValues* ReadEncryptedKVFile( IFileSystem *filesystem, const char *szFilenameWithoutExtension, const unsigned char *pICEKey, bool bForceReadEncryptedFile )
{
	Assert( strchr( szFilenameWithoutExtension, '.' ) == NULL );
	char szFullName[512];

	const char *pSearchPath = "GAME";

	// Open the weapon data file, and abort if we can't
	KeyValues *pKV = new KeyValues( "WeaponDatafile" );
	pKV->UsesEscapeSequences( true );

	Q_snprintf(szFullName,sizeof(szFullName), "%s.txt", szFilenameWithoutExtension);

	if ( bForceReadEncryptedFile || !pKV->LoadFromFile( filesystem, szFullName, pSearchPath ) ) // try to load the normal .txt file first
	{
		if ( pICEKey )
		{
			Q_snprintf(szFullName,sizeof(szFullName), "%s.ctx", szFilenameWithoutExtension); // fall back to the .ctx file

			FileHandle_t f = filesystem->Open( szFullName, "rb", pSearchPath );

			if (!f)
			{
				pKV->deleteThis();
				return NULL;
			}
			// load file into a null-terminated buffer
			int fileSize = filesystem->Size(f);
			char *buffer = (char*)MemAllocScratch(fileSize + 1);
		
			Assert(buffer);
		
			filesystem->Read(buffer, fileSize, f); // read into local buffer
			buffer[fileSize] = 0; // null terminate file as EOF
			filesystem->Close( f );	// close file after reading

			UTIL_DecodeICE( (unsigned char*)buffer, fileSize, pICEKey );

			bool retOK = pKV->LoadFromBuffer( szFullName, buffer, filesystem );

			MemFreeScratch();

			if ( !retOK )
			{
				pKV->deleteThis();
				return NULL;
			}
		}
		else
		{
			pKV->deleteThis();
			return NULL;
		}
	}

	return pKV;
}


CUtlSortVector< WeaponInfoLookup*, CWeaponInfoLookupListLess > FileWeaponInfo_t::ms_vecWeaponInfoLookup;

bool							FileWeaponInfo_t::ms_bWeaponInfoLookupInitialized;


WeaponInfoLookup::WeaponInfoLookup( size_t offset, _fieldtypes p_fieldType, const char *szAttribClassName )
{
	m_nWeaponParseDataOffset = offset;
	m_fieldType = p_fieldType;
	m_iszAttribClassName.Set( szAttribClassName );
}

WeaponInfoLookup::WeaponInfoLookup( const WeaponInfoLookup &WepInfoLookup )
{
	m_nWeaponParseDataOffset = WepInfoLookup.m_nWeaponParseDataOffset;
	m_fieldType = WepInfoLookup.m_fieldType;
	m_iszAttribClassName.Set( STRING( WepInfoLookup.m_iszAttribClassName.Get() ) );
}

//-----------------------------------------------------------------------------
// FileWeaponInfo_t implementation.
//-----------------------------------------------------------------------------

FileWeaponInfo_t::FileWeaponInfo_t()
{
	bParsedScript = false;
	bLoadedHudElements = false;
	szClassName[0] = 0;
	szPrintName[0] = 0;

	szViewModel[0] = 0;
	szWorldModel[0] = 0;
	szAnimationPrefix[0] = 0;
	iSlot = 0;
	iPosition = 0;
	iMaxClip1 = 0;
	iMaxClip2 = 0;
	iDefaultClip1 = 0;
	iDefaultClip2 = 0;
	iWeight = 0;
	iRumbleEffect = -1;
	bAutoSwitchTo = false;
	bAutoSwitchFrom = false;
	iFlags = 0;
	szAmmo1[0] = 0;
	szAmmo2[0] = 0;
	szAIAddOn[0] = 0;
	memset( aShootSounds, 0, sizeof( aShootSounds ) );
	iAmmoType = 0;
	iAmmo2Type = 0;
	m_bMeleeWeapon = false;
	iSpriteCount = 0;
	iconActive = 0;
	iconInactive = 0;
	iconAmmo = 0;
	iconAmmo2 = 0;
	iconCrosshair = 0;
	iconAutoaim = 0;
	iconZoomedCrosshair = 0;
	iconZoomedAutoaim = 0;
	bShowUsageHint = false;
	m_bAllowFlipping = true;
	m_bBuiltRightHanded = true;

	if ( !ms_bWeaponInfoLookupInitialized )
	{
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( FileWeaponInfo_t, iMaxClip1 ), FIELD_INTEGER, "primary clip size" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( FileWeaponInfo_t, iMaxClip2 ), FIELD_INTEGER, "secondary clip size" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( FileWeaponInfo_t, iDefaultClip1 ), FIELD_INTEGER, "primary default clip size" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( FileWeaponInfo_t, iDefaultClip2 ), FIELD_INTEGER, "secondary default clip size" ) );
		ms_vecWeaponInfoLookup.Insert( new WeaponInfoLookup( offsetof( FileWeaponInfo_t, iSlot ), FIELD_INTEGER, "bucket slot" ) );

		ms_bWeaponInfoLookupInitialized = true;
	}
}

#ifdef CLIENT_DLL
extern ConVar hud_fastswitch;
#endif

void FileWeaponInfo_t::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	// Okay, we tried at least once to look this up...
	bParsedScript = true;

	// Classname
	Q_strncpy( szClassName, szWeaponName, MAX_WEAPON_STRING );
	// Printable name
	Q_strncpy( szPrintName, pKeyValuesData->GetString( "printname", WEAPON_PRINTNAME_MISSING ), MAX_WEAPON_STRING );
	// View model & world model
	Q_strncpy( szViewModel, pKeyValuesData->GetString( "viewmodel" ), MAX_WEAPON_STRING );
	Q_strncpy( szWorldModel, pKeyValuesData->GetString( "playermodel" ), MAX_WEAPON_STRING );

	V_StripExtension( szWorldModel, szWorldDroppedModel, sizeof( szWorldDroppedModel ) );
	V_strcat_safe( szWorldDroppedModel, "_dropped.mdl" );
	
	Q_strncpy( szAnimationPrefix, pKeyValuesData->GetString( "anim_prefix" ), MAX_WEAPON_PREFIX );
	iSlot = pKeyValuesData->GetInt( "bucket", 0 );
	iPosition = pKeyValuesData->GetInt( "bucket_position", 0 );
	
	// Use the console (X360) buckets if hud_fastswitch is set to 2.
#ifdef CLIENT_DLL
	if ( hud_fastswitch.GetInt() == 2 )
#else
	if ( IsGameConsole() )
#endif
	{
		iSlot = pKeyValuesData->GetInt( "bucket_360", iSlot );
		iPosition = pKeyValuesData->GetInt( "bucket_position_360", iPosition );
	}
	iMaxClip1 = pKeyValuesData->GetInt( "clip_size", WEAPON_NOCLIP );					// Max primary clips gun can hold (assume they don't use clips by default)
	iMaxClip2 = pKeyValuesData->GetInt( "clip2_size", WEAPON_NOCLIP );					// Max secondary clips gun can hold (assume they don't use clips by default)
	iDefaultClip1 = pKeyValuesData->GetInt( "default_clip", iMaxClip1 );		// amount of primary ammo placed in the primary clip when it's picked up
	iDefaultClip2 = pKeyValuesData->GetInt( "default_clip2", iMaxClip2 );		// amount of secondary ammo placed in the secondary clip when it's picked up
	iWeight = pKeyValuesData->GetInt( "weight", 0 );

	iRumbleEffect = pKeyValuesData->GetInt( "rumble", -1 );
	
	// LAME old way to specify item flags.
	// Weapon scripts should use the flag names.
	iFlags = pKeyValuesData->GetInt( "item_flags", ITEM_FLAG_LIMITINWORLD );

	for ( int i=0; i < ARRAYSIZE( g_ItemFlags ); i++ )
	{
		int iVal = pKeyValuesData->GetInt( g_ItemFlags[i].m_pFlagName, -1 );
		if ( iVal == 0 )
		{
			iFlags &= ~g_ItemFlags[i].m_iFlagValue;
		}
		else if ( iVal == 1 )
		{
			iFlags |= g_ItemFlags[i].m_iFlagValue;
		}
	}

	bShowUsageHint = pKeyValuesData->GetBool( "showusagehint", false );
	bAutoSwitchTo = pKeyValuesData->GetBool( "autoswitchto", true );
	bAutoSwitchFrom = pKeyValuesData->GetBool( "autoswitchfrom", true );
	m_bBuiltRightHanded = pKeyValuesData->GetBool( "BuiltRightHanded", true );
	m_bAllowFlipping = pKeyValuesData->GetBool( "AllowFlipping", true );
	m_bMeleeWeapon = pKeyValuesData->GetBool( "MeleeWeapon", false );

#if defined(_DEBUG) && defined(HL2_CLIENT_DLL)
	// make sure two weapons aren't in the same slot & position
	if ( iSlot >= MAX_WEAPON_SLOTS ||
		iPosition >= MAX_WEAPON_POSITIONS )
	{
		Warning( "Invalid weapon slot or position [slot %d/%d max], pos[%d/%d max]\n",
			iSlot, MAX_WEAPON_SLOTS - 1, iPosition, MAX_WEAPON_POSITIONS - 1 );
	}
	else
	{
		if (g_bUsedWeaponSlots[iSlot][iPosition])
		{
			Warning( "Duplicately assigned weapon slots in selection hud:  %s (%d, %d)\n", szPrintName, iSlot, iPosition );
		}
		g_bUsedWeaponSlots[iSlot][iPosition] = true;
	}
#endif

	// Primary ammo used
	const char *pAmmo = pKeyValuesData->GetString( "primary_ammo", "None" );
	if ( strcmp("None", pAmmo) == 0 )
		Q_strncpy( szAmmo1, "", sizeof( szAmmo1 ) );
	else
		Q_strncpy( szAmmo1, pAmmo, sizeof( szAmmo1 )  );
	iAmmoType = GetAmmoDef()->Index( szAmmo1 );
	
	// Secondary ammo used
	pAmmo = pKeyValuesData->GetString( "secondary_ammo", "None" );
	if ( strcmp("None", pAmmo) == 0)
		Q_strncpy( szAmmo2, "", sizeof( szAmmo2 ) );
	else
		Q_strncpy( szAmmo2, pAmmo, sizeof( szAmmo2 )  );
	iAmmo2Type = GetAmmoDef()->Index( szAmmo2 );

	// AI AddOn
	const char *pAIAddOn = pKeyValuesData->GetString( "ai_addon", "ai_addon_basecombatweapon" );
	if ( strcmp("None", pAIAddOn) == 0)
		Q_strncpy( szAIAddOn, "", sizeof( szAIAddOn ) );
	else
		Q_strncpy( szAIAddOn, pAIAddOn, sizeof( szAIAddOn )  );

	// Now read the weapon sounds
	memset( aShootSounds, 0, sizeof( aShootSounds ) );
	KeyValues *pSoundData = pKeyValuesData->FindKey( "SoundData" );
	if ( pSoundData )
	{
		for ( int i = EMPTY; i < NUM_SHOOT_SOUND_TYPES; i++ )
		{
			const char *soundname = pSoundData->GetString( pWeaponSoundCategories[i] );
			if ( soundname && soundname[0] )
			{
				Q_strncpy( aShootSounds[i], soundname, MAX_WEAPON_STRING );
			}
			else
			{
				// FIXME: find a better way to populate defaults
			
				if ( i == NEARLYEMPTY ) // explicit check because it looks like most weapon sound categories don't fall back by default
				{
					V_sprintf_safe( aShootSounds[i], "Default.%s", pWeaponSoundCategories[i] );
				}
			}
		}
	}
}

const char* FileWeaponInfo_t::GetWorldModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pchWorldOverride = pWepView->GetStaticData()->GetEntityOverrideModel();
		if ( pchWorldOverride )
		{
			return pchWorldOverride;
		}

		return pWepView->GetItemDefinition()->GetWorldDisplayModel();
	}
	else
	{
		return szWorldModel;
	}
}

const char* FileWeaponInfo_t::GetViewModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pchViewOverride = pWepView->GetStaticData()->GetViewOverrideModel();
		if ( pchViewOverride )
		{
			return pchViewOverride;
		}

		return pWepView->GetItemDefinition()->GetBasePlayerDisplayModel();
	}
	else
	{
		return szViewModel;
	}
}

const char* FileWeaponInfo_t::GetWorldDroppedModel( const CEconItemView* pWepView, int iTeam ) const
{
	if ( pWepView && pWepView->IsValid() )
	{
		const char *pchWorldDroppedModel = pWepView->GetItemDefinition()->GetWorldDroppedModel();
		if ( pchWorldDroppedModel )
		{
			return pchWorldDroppedModel;
		}
	}
	
	return szWorldDroppedModel;
}

const char* FileWeaponInfo_t::GetPrimaryAmmo( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = pWepView->GetStaticData()->GetPrimaryAmmo();

		if ( pszString )
		{
			return pszString;
		}
	}

	return szAmmo1;
}


int FileWeaponInfo_t::GetPrimaryAmmoType( const CEconItemView* pWepView ) const
{

	if ( pWepView && pWepView->IsValid() )
	{
		// TODO: replace visual data with attributes when attributes support strings.
		const char *pszString = GetPrimaryAmmo( pWepView );

		if ( pszString )
		{
			return GetAmmoDef()->Index( pszString );
		}
	}

	return iAmmoType;
}


static int WeaponInfoLookupCompare( WeaponInfoLookup * const * src1, WeaponInfoLookup * const * src2 )
{
	if ( ( *src1 )->m_iszAttribClassName.Get() == ( *src2 )->m_iszAttribClassName.Get() )
		return 0;

	if ( ( *src1 )->m_iszAttribClassName.Get() < ( *src2 )->m_iszAttribClassName.Get() )
		return -1;

	return 1;
}

// Convert a schema attribute classname into an index of g_WeaponInfoTable
int FileWeaponInfo_t::GetIndexofAttribute( string_t iszAttribClassName ) const
{
	if ( ms_vecWeaponInfoLookup.Count() == 0 )
		return -1;

	if ( ms_vecWeaponInfoLookup[ 0 ]->m_iszAttribClassName.IsSerialNumberOutOfDate() )
	{
		// If the strings are out of date, rebuild and resort them
		FOR_EACH_VEC( ms_vecWeaponInfoLookup, i )
		{
			ms_vecWeaponInfoLookup[ i ]->m_iszAttribClassName.Get();
		}

		ms_vecWeaponInfoLookup.Sort( &WeaponInfoLookupCompare );
	}

	WeaponInfoLookup searchKey;
	searchKey.m_iszAttribClassName.SetFastNoCopy( iszAttribClassName );
	return ms_vecWeaponInfoLookup.Find( &searchKey );
}


#if defined( GAME_DLL )
CON_COMMAND_F ( weapon_reload_database, "Reload the weapon database", FCVAR_CHEAT | FCVAR_GAMEDLL| FCVAR_SERVER_CAN_EXECUTE )
{
	g_WeaponDatabase.LoadManifest();
	IGameEvent *event = gameeventmanager->CreateEvent( "weapon_reload_database" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}
}
#endif


CWeaponDatabase::CWeaponDatabase() : m_bPreCached( false )
{
}

bool CWeaponDatabase::Init()
{
	ListenForGameEvent( "weapon_reload_database" );
	return LoadManifest();
}

bool CWeaponDatabase::LoadManifest()
{
	// FIXME[pmf]: this is defined as a virtual in g_pGameRules, but it isn't construction yet...
	const unsigned char szEncryptionKey[] = "d7NSuLq2";

	// KeyValues::AutoDelete manifest = ReadEncryptedKVFile( filesystem, "scripts/weapon_manifest.txt", szEncryptionKey );
	KeyValues::AutoDelete manifest( "weaponscripts" );
	manifest->UsesEscapeSequences( true );
	if ( manifest->LoadFromFile( filesystem, "scripts/weapon_manifest.txt", "GAME" ) )
	{
		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL ; sub = sub->GetNextKey() )
		{
			if ( !Q_stricmp( sub->GetName(), "file" ) )
			{
				char fileBase[512];
				Q_FileBase( sub->GetString(), fileBase, sizeof(fileBase) );

				LoadWeaponDataFromFile( filesystem, fileBase, szEncryptionKey );
			}
			else
			{
				Error( "Expecting 'file', got %s\n", sub->GetName() );
			}
		}
	}

	LoadEquipmentData();

	return true;
}

void CWeaponDatabase::PrecacheAllWeapons()
{
// NOTE[pmf]: removed this precache check for now, since it seems that the caches are getting evicted on map change
// 	if ( m_bPreCached )
// 		return;

	FOR_EACH_DICT( m_WeaponInfoDatabase, it )
	{
		const char* szClassName = m_WeaponInfoDatabase.GetElementName( it );

#if defined( CLIENT_DLL )
		// don't try to precache items that don't have entities (e.g. the items don't exist as entities on the client)
		if ( GetClassMap().GetClassSize(szClassName) <= 0 )
// 		if ( V_strncasecmp(szClassName, "item_", 5) == 0)
			continue;
#endif

		UTIL_PrecacheOther( szClassName );
	}
	m_bPreCached = true;
}


void CWeaponDatabase::RefreshAllWeapons()
{
	FOR_EACH_DICT( m_WeaponInfoDatabase, it )
	{
		FileWeaponInfo_t* pWeaponInfo = m_WeaponInfoDatabase[it];
		if ( pWeaponInfo )
		{
			pWeaponInfo->RefreshDynamicParameters();
		}
	}
}

WEAPON_FILE_INFO_HANDLE CWeaponDatabase::FindWeaponInfo( const char *name )
{
	return m_WeaponInfoDatabase.Find( name );
}


WEAPON_FILE_INFO_HANDLE CWeaponDatabase::FindOrCreateWeaponInfo( const char *name )
{
	// Complain about duplicately defined metaclass names...
	unsigned short lookup = m_WeaponInfoDatabase.Find( name );
	if ( lookup != m_WeaponInfoDatabase.InvalidIndex() )
	{
		return lookup;
	}

	FileWeaponInfo_t *insert = CreateWeaponInfo();

	lookup = m_WeaponInfoDatabase.Insert( name, insert );
	Assert( lookup != m_WeaponInfoDatabase.InvalidIndex() );
	return lookup;
}

static FileWeaponInfo_t * Helper_GetNullWeaponInfo()
{
	static FileWeaponInfo_t gNullWeaponInfo;
	return &gNullWeaponInfo;
}

FileWeaponInfo_t * CWeaponDatabase::GetFileWeaponInfoFromHandle( WEAPON_FILE_INFO_HANDLE handle )
{
	if ( handle >= m_WeaponInfoDatabase.Count() )
	{
		return Helper_GetNullWeaponInfo();
	}

	if ( handle == m_WeaponInfoDatabase.InvalidIndex() )
	{
		return Helper_GetNullWeaponInfo();
	}

	return m_WeaponInfoDatabase[ handle ];
}



//-----------------------------------------------------------------------------
// Purpose: Read data on weapon from script file
// Output:  true  - if data2 successfully read
//			false - if data load fails
//-----------------------------------------------------------------------------
bool CWeaponDatabase::LoadWeaponDataFromFile( IFileSystem* filesystem, const char *szWeaponName, const unsigned char *pICEKey )
{
	char sz[128];
	Q_snprintf( sz, sizeof( sz ), "scripts/%s", szWeaponName );
	KeyValues *pKV = ReadEncryptedKVFile( filesystem, sz, pICEKey );
	if ( !pKV )
		return false;

	WEAPON_FILE_INFO_HANDLE handle = FindOrCreateWeaponInfo( szWeaponName );
	FileWeaponInfo_t *pFileInfo = GetFileWeaponInfoFromHandle( handle );
	Assert( pFileInfo );

	pFileInfo->Parse( pKV, szWeaponName );

	pKV->deleteThis();

	return true;
}

void CWeaponDatabase::Reset()
{
	int c = m_WeaponInfoDatabase.Count(); 
	for ( int i = 0; i < c; ++i )
	{
		delete m_WeaponInfoDatabase[ i ];
	}
	m_WeaponInfoDatabase.RemoveAll();

#ifdef _DEBUG
	memset(g_bUsedWeaponSlots, 0, sizeof(g_bUsedWeaponSlots));
#endif
	m_bPreCached = false;
}

void CWeaponDatabase::FireGameEvent( IGameEvent *pEvent )
{
#if defined( CLIENT_DLL )
	const char *pEventName = pEvent->GetName();
	if ( 0 == Q_strcmp( pEventName, "weapon_reload_database" ) )
	{
		LoadManifest();
	}
#endif
}
