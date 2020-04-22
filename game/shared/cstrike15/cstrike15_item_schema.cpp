//========= Copyright (c), Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "cstrike15_item_schema.h"
#include "game_item_schema.h"
#include "schemainitutils.h"
#include "shareddefs.h"
#include "cs_shareddefs.h"
#include "mathlib/lightdesc.h"

#ifndef GC_DLL
	#include "econ_item_system.h"
#endif // !GC_DLL


const char CCStrike15ItemSchema::k_rchCommunitySupportPassItemDefName[] = "Community Season One Spring 2013";


//--------------------------------------------------------------------------------------------------
// Purpose: Constructor.
//--------------------------------------------------------------------------------------------------
CCStrike15ItemDefinition::CCStrike15ItemDefinition()
{
	m_bIsSupplyCrate = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCStrike15ItemDefinition::BInitFromKV( KeyValues *pKVItem, CEconItemSchema &schemaa, CUtlVector<CUtlString> *pVecErrors )
{
	CEconItemDefinition::BInitFromKV( pKVItem, schemaa, pVecErrors );

	CCStrike15ItemSchema *pSchema = ItemSystem()->GetItemSchema();

	// Get the default loadout slot
	const char *pchSubPosition = GetRawDefinition()->GetString( "item_sub_position", NULL );
	m_iDefaultLoadoutSlot = ( pchSubPosition ? StringFieldToInt( pchSubPosition, pSchema->GetLoadoutStringsSubPositions() ) : -1 );
	for ( int i = 0; i < LOADOUT_COUNT; i++ )
	{
		m_iLoadoutSlots[i] = LOADOUT_POSITION_INVALID;
	}

	if ( m_iDefaultLoadoutSlot == LOADOUT_POSITION_CLOTHING_HANDS && !IsBaseItem() )
	{
		SCHEMA_INIT_CHECK( GetWorldDisplayModel(), CFmtStr( "Glove model %s (def idx %d) is missing world model key.", GetDefinitionName(), GetDefinitionIndex() ) );
	}

	// record if this item shares a loadout position with another type of item (m4, cz/p250)
	m_bItemSharesEquipSlot = GetRawDefinition()->GetBool( "item_shares_equip_slot" );

	const char *pchItemClass = GetItemClass();
	SCHEMA_INIT_CHECK( pchItemClass, CFmtStr( "Item \"%s\" is missing schema item class!", pKVItem->GetName() ) );
	m_bIsSupplyCrate = ( pchItemClass && !V_strcmp( pchItemClass, "supply_crate" ) );

	// Class usability--use our copy of kv item
	KeyValues *pClasses = GetRawDefinition()->FindKey( "used_by_classes" );
	if ( pClasses )
	{
		m_vbClassUsability.ClearAll();

		KeyValues *pKVClass = pClasses->GetFirstSubKey();
		while ( pKVClass )
		{
			int iTeam = StringFieldToInt( pKVClass->GetName(), pSchema->GetClassUsabilityStrings() );
			if ( iTeam > -1 )
			{
				m_vbClassUsability.Set(iTeam);
				m_iLoadoutSlots[iTeam] = m_iDefaultLoadoutSlot;

				// If the value is "1", the class uses this item in the default loadout slot.
				const char *pszValue = pKVClass->GetString();
				if ( pszValue[0] != '1' )
				{
					int iSlot = StringFieldToInt( pszValue, pSchema->GetLoadoutStrings() );
					Assert( iSlot != -1 );
					if ( iSlot != -1 )
					{
						m_iLoadoutSlots[iTeam] = iSlot;
					}
				}
			}

			pKVClass = pKVClass->GetNextKey();
		}

		// add "all_class" if applicable
		if ( CanBeUsedByAllTeams() )
		{
			KeyValues *pKVAllClassKey = new KeyValues( "all_class", "all_class", "1" );
			pClasses->AddSubKey( pKVAllClassKey );
		}
	}

	// Verify that no items are set up to be equipped in a wearable slot for some classes and a
	// non-wearable slot other times. "Is this in a wearable slot?" is used to determine whether
	// or not content can be allowed to stream, so we don't allow an item to overlap.
	bool bHasAnyWearableSlots = false,
		 bHasAnyNonwearableSlots = false;

	for ( int i = 0; i < LOADOUT_COUNT; i++ )
	{
		if ( m_iLoadoutSlots[i] != LOADOUT_POSITION_INVALID )
		{
			const bool bThisIsWearableSlot = IsWearableSlot( m_iLoadoutSlots[i] );

			(bThisIsWearableSlot ? bHasAnyWearableSlots : bHasAnyNonwearableSlots) = true;
		}
	}

	SCHEMA_INIT_CHECK(
		!(bHasAnyWearableSlots && bHasAnyNonwearableSlots),
		CFmtStr( "Item definition %i \"%s\" used in both wearable and not wearable slots!", GetDefinitionIndex(), GetItemBaseName() ) );

	// "anim_slot"
	m_iAnimationSlot = -1;
	const char *pszAnimSlot = GetRawDefinition()->GetString("anim_slot");
	if ( pszAnimSlot && pszAnimSlot[0] )
	{
		if ( Q_stricmp(pszAnimSlot, "FORCE_NOT_USED") == 0 )
		{
			m_iAnimationSlot = -2;
		}
		else
		{
			m_iAnimationSlot = StringFieldToInt( pszAnimSlot, pSchema->GetWeaponTypeSubstrings() );
		}
	}

	// Initialize player display model.
	for ( int i = 0; i < LOADOUT_COUNT; i++ )
	{
		m_pszPlayerDisplayModel[i] = NULL;
	}

	// "model_player_per_class"
	KeyValues *pPerClassModels = GetRawDefinition()->FindKey( "model_player_per_class" );
	for ( int i = 1; i < LOADOUT_COUNT; i++ )
	{
		if ( pPerClassModels )
		{
			m_pszPlayerDisplayModel[i] = pPerClassModels->GetString( pSchema->GetClassUsabilityStrings()[i], NULL );

			if ( m_pszPlayerDisplayModel[0] == NULL )
			{
				m_pszPlayerDisplayModel[0] = m_pszPlayerDisplayModel[i];
			}
		}
	}

	// Stomp duplicate properties.
	if ( !m_pszPlayerDisplayModel[0] )
	{
		m_pszPlayerDisplayModel[0] = GetBasePlayerDisplayModel();
	}

	// parse Paint Data
	KeyValues *pPaintData = GetRawDefinition()->FindKey( "paint_data" );
	if ( pPaintData )
	{
		KeyValues *pPaintableMaterialKeys = pPaintData->GetFirstSubKey();
		while ( pPaintableMaterialKeys )
		{
			const char *pName = pPaintableMaterialKeys->GetString( "Name" );
			if ( pName[0] != 0 )
			{
				WeaponPaintableMaterial_t *pPaintableMaterial = &m_PaintData[ m_PaintData.AddToTail() ];

				V_strncpy( pPaintableMaterial->m_szName, pName, sizeof( pPaintableMaterial->m_szName ) );
				const char *pOriginalMaterialName = pPaintableMaterialKeys->GetString( "OrigMat", "" );
				V_strncpy( pPaintableMaterial->m_szOriginalMaterialName, pOriginalMaterialName, sizeof( pPaintableMaterial->m_szOriginalMaterialName ) );
				const char *pFolderName = pPaintableMaterialKeys->GetString( "FolderName", pPaintableMaterial->m_szName ); // defaults to same as the name
				V_strncpy( pPaintableMaterial->m_szFolderName, pFolderName, sizeof( pPaintableMaterial->m_szFolderName ) );
				pPaintableMaterial->m_nViewModelSize = pPaintableMaterialKeys->GetInt( "ViewmodelDim", 1024 );
				pPaintableMaterial->m_nWorldModelSize = pPaintableMaterialKeys->GetInt( "WorldDim", 512 );
				pPaintableMaterial->m_flWeaponLength = pPaintableMaterialKeys->GetFloat( "WeaponLength", 36.0f );
				pPaintableMaterial->m_flUVScale = pPaintableMaterialKeys->GetFloat( "UVScale", 1.0f );
				pPaintableMaterial->m_bBaseTextureOverride = pPaintableMaterialKeys->GetBool( "BaseTextureOverride" );
				pPaintableMaterial->m_bMirrorPattern = pPaintableMaterialKeys->GetBool( "MirrorPattern", false );
			}
			else
			{
				DevMsg( "Error Parsing PaintData in %s! \n", GetDefinitionName() );
			}
			pPaintableMaterialKeys = pPaintableMaterialKeys->GetNextKey();
		}
	}

	// parse inventory image data
	KeyValues *pInventoryImageData = GetRawDefinition()->FindKey( "inventory_image_data" );
	if ( pInventoryImageData )
	{
		m_pInventoryImageData = new InventoryImageData_t;
		m_pInventoryImageData->m_pCameraAngles = NULL;
		m_pInventoryImageData->m_pCameraOffset = NULL;
		m_pInventoryImageData->m_cameraFOV = -1.0f;
		for ( int i = 0; i < MATERIAL_MAX_LIGHT_COUNT; i++ )
		{
			m_pInventoryImageData->m_pLightDesc[ i ] = NULL;
		}

		m_pInventoryImageData->m_bOverrideDefaultLight = pInventoryImageData->GetBool( "override_default_light", false );

		const char *pCameraAngles = pInventoryImageData->GetString( "camera_angles" );
		if ( pCameraAngles[0] != 0 )
		{
			float flX = 0.0f, flY = 0.0f, flZ = 0.0f;
			sscanf( pCameraAngles, "%f %f %f", &flX, &flY, &flZ );
			m_pInventoryImageData->m_pCameraAngles = new QAngle( flX, flY, flZ );
		}

		const char *pCameraOffset = pInventoryImageData->GetString( "camera_offset" );
		if ( pCameraOffset[0] != 0 )
		{
			float flX = 0.0f, flY = 0.0f, flZ = 0.0f;
			sscanf( pCameraOffset, "%f %f %f", &flX, &flY, &flZ );
			m_pInventoryImageData->m_pCameraOffset = new Vector( flX, flY, flZ );
		}

		m_pInventoryImageData->m_cameraFOV = pInventoryImageData->GetFloat( "camera_fov", -1.0f );

		int nNumLightDescs = 0;
		KeyValues *pLightKeys = pInventoryImageData->GetFirstTrueSubKey();
		while ( pLightKeys )
		{
			if ( nNumLightDescs >= ( MATERIAL_MAX_LIGHT_COUNT - ( ( m_pInventoryImageData->m_bOverrideDefaultLight ) ? 0 : 1 ) ) )
			{
				DevMsg( "Too many lights defined in inventory_image_data in %s. Only using first %d. \n", GetDefinitionName(), MATERIAL_MAX_LIGHT_COUNT );
				break;
			}

			const char *pLightType = pLightKeys->GetName();
			if ( pLightType[0] != 0 )
			{
				LightType_t lightType = MATERIAL_LIGHT_DISABLE;

				if ( V_strnicmp( pLightType, "point_light", 11 ) == 0 )
				{
					lightType = MATERIAL_LIGHT_POINT;
				}
				else if ( V_strnicmp( pLightType, "directional_light", 17 ) == 0 )
				{
					lightType = MATERIAL_LIGHT_DIRECTIONAL;
				}
				else if ( V_strnicmp( pLightType, "spot_light", 10 ) == 0 )
				{
					lightType = MATERIAL_LIGHT_SPOT;
				}
				else
				{
					DevMsg( "Error Parsing inventory_image_data in %s! Unknown light type %s. \n", GetDefinitionName(), pLightType );
				}

				if ( lightType != MATERIAL_LIGHT_DISABLE )
				{
					Vector lightPosOrDir( 0, 0, 0 );
					Vector lightColor( 0, 0, 0 );
					const char *pLightPosOrDir = pLightKeys->GetString( ( lightType == MATERIAL_LIGHT_DIRECTIONAL ) ? "direction" : "position" );
					if ( pLightPosOrDir[0] != 0 )
					{
						sscanf( pLightPosOrDir, "%f %f %f", &(lightPosOrDir.x), &(lightPosOrDir.y), &(lightPosOrDir.z) );
					}
					const char *pLightColor = pLightKeys->GetString( "color" );
					if ( pLightColor[0] != 0 )
					{
						sscanf( pLightColor, "%f %f %f", &(lightColor.x), &(lightColor.y), &(lightColor.z) );
					}

					Vector lightLookAt( 0, 0, 0 );
					float lightInnerCone = 1.0f;
					float lightOuterCone = 10.0f;
					if ( lightType == MATERIAL_LIGHT_SPOT )
					{
						const char *pLightLookAt = pLightKeys->GetString( "lookat" );
						if ( pLightLookAt[0] != 0 )
						{
							sscanf( pLightLookAt, "%f %f %f", &(lightLookAt.x), &(lightLookAt.y), &(lightLookAt.z) );
						}
						lightInnerCone = pLightKeys->GetFloat( "inner_cone", 1.0f );
						lightOuterCone = pLightKeys->GetFloat( "outer_cone", 8.0f );
					}

					m_pInventoryImageData->m_pLightDesc[ nNumLightDescs ] = new LightDesc_t;
					switch ( lightType )
					{
						case MATERIAL_LIGHT_DIRECTIONAL:
							m_pInventoryImageData->m_pLightDesc[ nNumLightDescs ]->InitDirectional( lightPosOrDir, lightColor );
							break;
						case MATERIAL_LIGHT_POINT:
							m_pInventoryImageData->m_pLightDesc[ nNumLightDescs ]->InitPoint( lightPosOrDir, lightColor );
							break;
						case MATERIAL_LIGHT_SPOT:
							m_pInventoryImageData->m_pLightDesc[ nNumLightDescs ]->InitSpot( lightPosOrDir, lightColor, lightLookAt, lightInnerCone, lightOuterCone );
							break;
					}
					nNumLightDescs++;
				}
			}

			pLightKeys = pLightKeys->GetNextTrueSubKey();
		}
	}

	return SCHEMA_INIT_SUCCESS();
}

#ifndef GC_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCStrike15ItemDefinition::BInitFromTestItemKVs( int iNewDefIndex, KeyValues *pKVItem, CEconItemSchema &schemaa )
{
	if ( !CEconItemDefinition::BInitFromTestItemKVs( iNewDefIndex, pKVItem, schemaa ) )
		return false;

	// Use the tester's class usage choices, even when testing existing items
	m_vbClassUsability.ClearAll();
	int iTeamUsage = pKVItem->GetInt( "class_usage", 0 );
	for ( int i = 0; i < LOADOUT_COUNT; i++ )
	{
		if ( iTeamUsage & (1 << i) || (iTeamUsage & 1) )
		{
			m_vbClassUsability.Set(i);
			m_iLoadoutSlots[i] = m_iDefaultLoadoutSlot;
		}
	}

	// Stomp duplicate properties.
	m_pszPlayerDisplayModel[0] = GetBasePlayerDisplayModel();

	return true;
}
#endif // !GC_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCStrike15ItemDefinition::CopyPolymorphic( const CEconItemDefinition *pSourceDef )
{
	Assert( dynamic_cast<const CCStrike15ItemDefinition *>( pSourceDef ) != NULL );

	*this = *(const CCStrike15ItemDefinition *)pSourceDef;
}

#ifndef GC_DLL
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CCStrike15ItemDefinition::GeneratePrecacheModelStrings( bool bDynamicLoad, CUtlVector<const char *> *out_pVecModelStrings )
{
	Assert( out_pVecModelStrings );

	// Is this definition supposed to use dynamic-loaded content or precache it?
	if ( !bDynamicLoad || !IsContentStreamable() )
	{
		// Parent class base meshes, if relevant.
		CEconItemDefinition::GeneratePrecacheModelStrings( bDynamicLoad, out_pVecModelStrings );

		// Per-class models.
		for ( int i = 0; i < LOADOUT_COUNT; i++ )
		{
			const char *pszModel = GetPlayerDisplayModel(i);
			if ( pszModel && pszModel[0] )
			{
				out_pVecModelStrings->AddToTail( pszModel );
			}
		}

		const char *pszModel = GetWorldDisplayModel();
		if ( pszModel && pszModel[0] )
		{
			out_pVecModelStrings->AddToTail( pszModel );
		}
	}
}
#endif // !GC_DLL

//-----------------------------------------------------------------------------
// Purpose: Return the load-out slot that this item must be placed into
//-----------------------------------------------------------------------------
int CCStrike15ItemDefinition::GetLoadoutSlot( int iTeam ) const
{
	if ( iTeam <= 0 || iTeam >= LOADOUT_COUNT )
		return m_iDefaultLoadoutSlot;

	return m_iLoadoutSlots[iTeam];
}

#ifndef GC_DLL
//-----------------------------------------------------------------------------
// Purpose: Returns true if this item is in a wearable slot, or is acting as a wearable
//-----------------------------------------------------------------------------
bool CCStrike15ItemDefinition::IsAWearable( int iSlot ) const
{
	if ( IsWearableSlot( iSlot ) )
		return true;

	if ( IsActingAsAWearable() )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the content for this item view should be streamed. If false,
//			it should be preloaded.
//-----------------------------------------------------------------------------
#define ITEM_ENABLE_ITEM_CONTENT_STREAMING true
//ConVar item_enable_content_streaming( "item_enable_content_streaming", "1", FCVAR_ARCHIVE | FCVAR_DEVELOPMENTONLY );

bool CCStrike15ItemDefinition::IsContentStreamable() const
{
	if ( !IsAWearable( GetDefaultLoadoutSlot() ) )
		return false;

	return ITEM_ENABLE_ITEM_CONTENT_STREAMING
		&& CEconItemDefinition::IsContentStreamable();
}
#endif // !GC_DLL

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCStrike15ItemDefinition::FilloutSlotUsage( CBitVec<LOADOUT_COUNT> *pBV ) const
{
	pBV->ClearAll();

	for ( int i = 0; i < LOADOUT_COUNT; i++ )
	{
		if ( m_iLoadoutSlots[i] == LOADOUT_POSITION_INVALID )
			continue;

		pBV->Set( m_iLoadoutSlots[i] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CCStrike15ItemDefinition::GetUsedByTeam( void ) const
{
	if ( CanBeUsedByTeam(TEAM_TERRORIST) && CanBeUsedByTeam(TEAM_CT) )
		return TEAM_UNASSIGNED;

	if ( CanBeUsedByTeam(TEAM_TERRORIST) )
		return TEAM_TERRORIST;
	
	if ( CanBeUsedByTeam(TEAM_CT) )
		return TEAM_CT;

	return TEAM_UNASSIGNED;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCStrike15ItemDefinition::CanBeUsedByAllTeams( void ) const
{
	for ( int iTeam = 1; iTeam < (LOADOUT_COUNT-1); iTeam++ )
	{
		if ( !CanBeUsedByTeam(iTeam) )
			return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CCStrike15ItemDefinition::CanBePlacedInSlot( int nSlot ) const
{
	for ( int i = 0; i < LOADOUT_COUNT; i++ )
	{
		if ( m_iLoadoutSlots[i] == nSlot )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
// Used to convert strings to ints for class usability
const char *g_ClassUsabilityStrings[] =
{
	"noteam",
	"undefined",
	"terrorists",			// TEAM_TERRORIST
	"counter-terrorists",	// TEAM_CT
};

// Loadout positions
const char *g_szLoadoutStrings[] = 
{
	// Weapons & Equipment
	"melee",			// LOADOUT_POSITION_MELEE = 0,
	"c4",				// LOADOUT_POSITION_C4,

	"secondary",		// LOADOUT_POSITION_SECONDARY0,
	"secondary",		// LOADOUT_POSITION_SECONDARY1,
	"secondary",		// LOADOUT_POSITION_SECONDARY2,
	"secondary",		// LOADOUT_POSITION_SECONDARY3,
	"secondary",		// LOADOUT_POSITION_SECONDARY4,
	"secondary",		// LOADOUT_POSITION_SECONDARY5,

	"smg",				// LOADOUT_POSITION_SMG0,
	"smg",				// LOADOUT_POSITION_SMG1,
	"smg",				// LOADOUT_POSITION_SMG2,
	"smg",				// LOADOUT_POSITION_SMG3,
	"smg",				// LOADOUT_POSITION_SMG4,
	"smg",				// LOADOUT_POSITION_SMG5,

	"rifle",			// LOADOUT_POSITION_RIFLE0,
	"rifle",			// LOADOUT_POSITION_RIFLE1,
	"rifle",			// LOADOUT_POSITION_RIFLE2,
	"rifle",			// LOADOUT_POSITION_RIFLE3,
	"rifle",			// LOADOUT_POSITION_RIFLE4,
	"rifle",			// LOADOUT_POSITION_RIFLE5,

	"heavy",			// LOADOUT_POSITION_HEAVY0,
	"heavy",			// LOADOUT_POSITION_HEAVY1,
	"heavy",			// LOADOUT_POSITION_HEAVY2,
	"heavy",			// LOADOUT_POSITION_HEAVY3,
	"heavy",			// LOADOUT_POSITION_HEAVY4,
	"heavy",			// LOADOUT_POSITION_HEAVY5,

	"grenade",			// LOADOUT_POSITION_GRENADE0,
	"grenade",			// LOADOUT_POSITION_GRENADE1,
	"grenade",			// LOADOUT_POSITION_GRENADE2,
	"grenade",			// LOADOUT_POSITION_GRENADE3,
	"grenade",			// LOADOUT_POSITION_GRENADE4,
	"grenade",			// LOADOUT_POSITION_GRENADE5,

	"equipment",		// LOADOUT_POSITION_EQUIPMENT0,
	"equipment",		// LOADOUT_POSITION_EQUIPMENT1,
	"equipment",		// LOADOUT_POSITION_EQUIPMENT2,
	"equipment",		// LOADOUT_POSITION_EQUIPMENT3,
	"equipment",		// LOADOUT_POSITION_EQUIPMENT4,
	"equipment",		// LOADOUT_POSITION_EQUIPMENT5,

	"clothing",			// LOADOUT_POSITION_CLOTHING_APPEARANCE,
	"clothing",			// LOADOUT_POSITION_CLOTHING_TORSO,
	"clothing",			// LOADOUT_POSITION_CLOTHING_LOWERBODY,
	"clothing",			// LOADOUT_POSITION_CLOTHING_HANDS,
	"clothing",			// LOADOUT_POSITION_CLOTHING_HAT,
	"clothing",			// LOADOUT_POSITION_CLOTHING_FACEMASK,
	"clothing",			// LOADOUT_POSITION_CLOTHING_EYEWEAR,
	"clothing",			// LOADOUT_POSITION_CLOTHING_CUSTOMHEAD,
	"clothing",			// LOADOUT_POSITION_CLOTHING_CUSTOMPLAYER,

	"misc",				// LOADOUT_POSITION_MISC0,
	"misc",				// LOADOUT_POSITION_MISC1,
	"misc",				// LOADOUT_POSITION_MISC2,
	"misc",				// LOADOUT_POSITION_MISC3,
	"misc",				// LOADOUT_POSITION_MISC4,
	"misc",				// LOADOUT_POSITION_MISC5,
	"misc",				// LOADOUT_POSITION_MISC6,

	"musickit",			// LOADOUT_POSITION_MUSICKIT,
	"flair0",			// LOADOUT_POSITION_FLAIR0,

	"spray",			// LOADOUT_POSITION_SPRAY0,
};

// Loadout positions
const char *g_szLoadoutStringsSubPositions[] = 
{
	// Weapons & Equipment
	"melee",			// LOADOUT_POSITION_MELEE = 0,
	"c4",				// LOADOUT_POSITION_C4,

	"secondary0",		// LOADOUT_POSITION_SECONDARY0,
	"secondary1",		// LOADOUT_POSITION_SECONDARY1,
	"secondary2",		// LOADOUT_POSITION_SECONDARY2,
	"secondary3",		// LOADOUT_POSITION_SECONDARY3,
	"secondary4",		// LOADOUT_POSITION_SECONDARY4,
	"secondary5",		// LOADOUT_POSITION_SECONDARY5,

	"smg0",				// LOADOUT_POSITION_SMG0,
	"smg1",				// LOADOUT_POSITION_SMG1,
	"smg2",				// LOADOUT_POSITION_SMG2,
	"smg3",				// LOADOUT_POSITION_SMG3,
	"smg4",				// LOADOUT_POSITION_SMG4,
	"smg5",				// LOADOUT_POSITION_SMG5,

	"rifle0",			// LOADOUT_POSITION_RIFLE0,
	"rifle1",			// LOADOUT_POSITION_RIFLE1,
	"rifle2",			// LOADOUT_POSITION_RIFLE2,
	"rifle3",			// LOADOUT_POSITION_RIFLE3,
	"rifle4",			// LOADOUT_POSITION_RIFLE4,
	"rifle5",			// LOADOUT_POSITION_RIFLE5,

	"heavy0",			// LOADOUT_POSITION_HEAVY0,
	"heavy1",			// LOADOUT_POSITION_HEAVY1,
	"heavy2",			// LOADOUT_POSITION_HEAVY2,
	"heavy3",			// LOADOUT_POSITION_HEAVY3,
	"heavy4",			// LOADOUT_POSITION_HEAVY4,
	"heavy5",			// LOADOUT_POSITION_HEAVY5,

	"grenade0",			// LOADOUT_POSITION_GRENADE0,
	"grenade1",			// LOADOUT_POSITION_GRENADE1,
	"grenade2",			// LOADOUT_POSITION_GRENADE2,
	"grenade3",			// LOADOUT_POSITION_GRENADE3,
	"grenade4",			// LOADOUT_POSITION_GRENADE4,
	"grenade5",			// LOADOUT_POSITION_GRENADE5,

	"equipment0",		// LOADOUT_POSITION_EQUIPMENT0,
	"equipment1",		// LOADOUT_POSITION_EQUIPMENT1,
	"equipment2",		// LOADOUT_POSITION_EQUIPMENT2,
	"equipment3",		// LOADOUT_POSITION_EQUIPMENT3,
	"equipment4",		// LOADOUT_POSITION_EQUIPMENT4,
	"equipment5",		// LOADOUT_POSITION_EQUIPMENT5,

	"clothing0",		// LOADOUT_POSITION_CLOTHING_APPEARANCE,
	"clothing1",			// LOADOUT_POSITION_CLOTHING_TORSO,
	"clothing2",		// LOADOUT_POSITION_CLOTHING_LOWERBODY,
	"clothing_hands",			// LOADOUT_POSITION_CLOTHING_HANDS,
	"clothing4",				// LOADOUT_POSITION_CLOTHING_HAT,
	"clothing5",		// LOADOUT_POSITION_CLOTHING_FACEMASK,
	"clothing6",			// LOADOUT_POSITION_CLOTHING_EYEWEAR,
	"clothing7",		// LOADOUT_POSITION_CLOTHING_CUSTOMHEAD,
	"clothing8",	// LOADOUT_POSITION_CLOTHING_CUSTOMPLAYER,

	"misc0",			// LOADOUT_POSITION_MISC0,
	"misc1",			// LOADOUT_POSITION_MISC1,
	"misc2",			// LOADOUT_POSITION_MISC2,
	"misc3",			// LOADOUT_POSITION_MISC3,
	"misc4",			// LOADOUT_POSITION_MISC4,
	"misc5",			// LOADOUT_POSITION_MISC5,
	"misc6",			// LOADOUT_POSITION_MISC6,

	"musickit",			// LOADOUT_POSITION_MUSICKIT,
	"flair0",			// LOADOUT_POSITION_FLAIR0,

	"spray0",			// LOADOUT_POSITION_SPRAY0,
};

// Loadout positions used to display loadout slots to players (localized)
const char *g_szLoadoutStringsForDisplay[] = 
{
	"#LoadoutSlot_Melee",				// LOADOUT_POSITION_MELEE = 0,
	"#LoadoutSlot_C4",					// LOADOUT_POSITION_C4,

	"#LoadoutSlot_Secondary",			// LOADOUT_POSITION_SECONDARY0,
	"#LoadoutSlot_Secondary",			// LOADOUT_POSITION_SECONDARY1,
	"#LoadoutSlot_Secondary",			// LOADOUT_POSITION_SECONDARY2,
	"#LoadoutSlot_Secondary",			// LOADOUT_POSITION_SECONDARY3,
	"#LoadoutSlot_Secondary",			// LOADOUT_POSITION_SECONDARY4,
	"#LoadoutSlot_Secondary",			// LOADOUT_POSITION_SECONDARY5,

	"#LoadoutSlot_SMG",					// LOADOUT_POSITION_SMG0,
	"#LoadoutSlot_SMG",					// LOADOUT_POSITION_SMG1,
	"#LoadoutSlot_SMG",					// LOADOUT_POSITION_SMG2,
	"#LoadoutSlot_SMG",					// LOADOUT_POSITION_SMG3,
	"#LoadoutSlot_SMG",					// LOADOUT_POSITION_SMG4,
	"#LoadoutSlot_SMG",					// LOADOUT_POSITION_SMG5,

	"#LoadoutSlot_Rifle",				// LOADOUT_POSITION_RIFLE0,
	"#LoadoutSlot_Rifle",				// LOADOUT_POSITION_RIFLE1,
	"#LoadoutSlot_Rifle",				// LOADOUT_POSITION_RIFLE2,
	"#LoadoutSlot_Rifle",				// LOADOUT_POSITION_RIFLE3,
	"#LoadoutSlot_Rifle",				// LOADOUT_POSITION_RIFLE4,
	"#LoadoutSlot_Rifle",				// LOADOUT_POSITION_RIFLE5,

	"#LoadoutSlot_Heavy",				// LOADOUT_POSITION_HEAVY0,
	"#LoadoutSlot_Heavy",				// LOADOUT_POSITION_HEAVY1,
	"#LoadoutSlot_Heavy",				// LOADOUT_POSITION_HEAVY2,
	"#LoadoutSlot_Heavy",				// LOADOUT_POSITION_HEAVY3,
	"#LoadoutSlot_Heavy",				// LOADOUT_POSITION_HEAVY4,
	"#LoadoutSlot_Heavy",				// LOADOUT_POSITION_HEAVY5,

	"#LoadoutSlot_Grenade",				// LOADOUT_POSITION_GRENADE0,
	"#LoadoutSlot_Grenade",				// LOADOUT_POSITION_GRENADE1,
	"#LoadoutSlot_Grenade",				// LOADOUT_POSITION_GRENADE2,
	"#LoadoutSlot_Grenade",				// LOADOUT_POSITION_GRENADE3,
	"#LoadoutSlot_Grenade",				// LOADOUT_POSITION_GRENADE4,
	"#LoadoutSlot_Grenade",				// LOADOUT_POSITION_GRENADE5,

	"#LoadoutSlot_Equipment",			// LOADOUT_POSITION_EQUIPMENT0,
	"#LoadoutSlot_Equipment",			// LOADOUT_POSITION_EQUIPMENT1,
	"#LoadoutSlot_Equipment",			// LOADOUT_POSITION_EQUIPMENT2,
	"#LoadoutSlot_Equipment",			// LOADOUT_POSITION_EQUIPMENT3,
	"#LoadoutSlot_Equipment",			// LOADOUT_POSITION_EQUIPMENT4,
	"#LoadoutSlot_Equipment",			// LOADOUT_POSITION_EQUIPMENT5,

	"#LoadoutSlot_Clothing",		// LOADOUT_POSITION_CLOTHING_APPEARANCE,
	"#LoadoutSlot_Clothing",			// LOADOUT_POSITION_CLOTHING_TORSO,
	"#LoadoutSlot_Clothing",		// LOADOUT_POSITION_CLOTHING_LOWERBODY,
	"#LoadoutSlot_Clothing_hands",			// LOADOUT_POSITION_CLOTHING_HANDS,
	"#LoadoutSlot_Clothing",			// LOADOUT_POSITION_CLOTHING_HAT,
	"#LoadoutSlot_Clothing",		// LOADOUT_POSITION_CLOTHING_FACEMASK,
	"#LoadoutSlot_Clothing",		// LOADOUT_POSITION_CLOTHING_EYEWEAR,
	"#LoadoutSlot_Clothing",		// LOADOUT_POSITION_CLOTHING_CUSTOMHEAD,
	"#LoadoutSlot_Clothing",	// LOADOUT_POSITION_CLOTHING_CUSTOMPLAYER,

	"#LoadoutSlot_Misc",				// LOADOUT_POSITION_MISC0,
	"#LoadoutSlot_Misc",				// LOADOUT_POSITION_MISC1,
	"#LoadoutSlot_Misc",				// LOADOUT_POSITION_MISC2,
	"#LoadoutSlot_Misc",				// LOADOUT_POSITION_MISC3,
	"#LoadoutSlot_Misc",				// LOADOUT_POSITION_MISC4,
	"#LoadoutSlot_Misc",				// LOADOUT_POSITION_MISC5,
	"#LoadoutSlot_Misc",				// LOADOUT_POSITION_MISC6,

	"#LoadoutSlot_MusicKit",			// LOADOUT_POSITION_MUSICKIT,	
	"#LoadoutSlot_Flair",				// LOADOUT_POSITION_FLAIR0,
	"#LoadoutSlot_Spray",				// LOADOUT_POSITION_SPRAY0,
};

/*
// Weapon types
const char *g_szWeaponTypeSubstrings[TF_WPN_TYPE_COUNT] =
{
	// Weapons & Equipment
	"PRIMARY",
	"SECONDARY",
	"MELEE",
	"GRENADE",
	"BUILDING",
	"PDA",
	"ITEM1",
	"ITEM2",
	"HEAD",
	"MISC",
	"MELEE_ALLCLASS",
	"SECONDARY2",
	"PRIMARY2"
};
*/

CCStrike15ItemSchema::CCStrike15ItemSchema()
{
	COMPILE_TIME_ASSERT( ARRAYSIZE( g_szLoadoutStringsForDisplay ) == LOADOUT_POSITION_COUNT );
	COMPILE_TIME_ASSERT( ARRAYSIZE( g_szLoadoutStringsSubPositions ) == LOADOUT_POSITION_COUNT );
	COMPILE_TIME_ASSERT( ARRAYSIZE( g_szLoadoutStrings ) == LOADOUT_POSITION_COUNT );

	InitializeStringTable( &g_ClassUsabilityStrings[0],			ARRAYSIZE(g_ClassUsabilityStrings),			&m_vecClassUsabilityStrings );
	Assert( m_vecClassUsabilityStrings.Count() == LOADOUT_COUNT );

	InitializeStringTable( &g_szLoadoutStrings[0],				ARRAYSIZE(g_szLoadoutStrings),				&m_vecLoadoutStrings );
	Assert( m_vecLoadoutStrings.Count() == LOADOUT_POSITION_COUNT );

	InitializeStringTable( &g_szLoadoutStringsSubPositions[0],	ARRAYSIZE(g_szLoadoutStringsSubPositions),	&m_vecLoadoutStringsSubPositions );
	Assert( m_vecLoadoutStringsSubPositions.Count() == LOADOUT_POSITION_COUNT );

	InitializeStringTable( &g_szLoadoutStringsForDisplay[0],	ARRAYSIZE(g_szLoadoutStringsForDisplay),	&m_vecLoadoutStringsForDisplay );
	Assert( m_vecLoadoutStringsForDisplay.Count() == LOADOUT_POSITION_COUNT );

//	InitializeStringTable( &g_szWeaponTypeSubstrings[0],		ARRAYSIZE(g_szWeaponTypeSubstrings),		&m_vecWeaponTypeSubstrings );
//	Assert( m_vecWeaponTypeSubstrings.Count() == TF_WPN_TYPE_COUNT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCStrike15ItemSchema::InitializeStringTable( const char **ppStringTable, unsigned int unStringCount, CUtlVector<const char *> *out_pvecStringTable )
{
	Assert( ppStringTable != NULL );
	Assert( out_pvecStringTable != NULL );
	Assert( out_pvecStringTable->Count() == 0 );

	for ( unsigned int i = 0; i < unStringCount; i++ )
	{
		Assert( ppStringTable[i] != NULL );
		out_pvecStringTable->AddToTail( ppStringTable[i] );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Parses game specific items_master data.
//-----------------------------------------------------------------------------
bool CCStrike15ItemSchema::BInitSchema( KeyValues *pKVRawDefinition, CUtlVector<CUtlString> *pVecErrors )
{
	return CEconItemSchema::BInitSchema( pKVRawDefinition, pVecErrors );
}
