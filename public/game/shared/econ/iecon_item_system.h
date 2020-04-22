//====== Copyright © 1996-2010, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef __ECON_ITEM_SYSTEM_INTERFACE_H
#define __ECON_ITEM_SYSTEM_INTERFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "utlmap.h"
#include "econ/econ_item_constants.h"

class KeyValues;
class IEconItemSchema;
class CEconItemDefinition;
struct item_list_entry_t;

//-----------------------------------------------------------------------------
// Particle system attachments exposed for editors.
//-----------------------------------------------------------------------------
struct attachedparticlecontrolpoint_t
{
	short		nControlPoint;
	byte		nAttachType;
	const char	*pszAttachmentName;
	Vector		vecPosition;
};

enum attachedparticle_toent_t
{
	ATTPART_TO_SELF,
	ATTPART_TO_PARENT,
};

struct attachedparticlesystem_t
{
	const char *pszResourceName;
	const char *pszSystemName;
	const char *pszAttachmentName;
	bool		bFollowRootBone;
	bool		bFlyingCourier;
	int			iCustomType;
	int			iCount;
	int			nSystemID;
	KeyValues*	pEffectKV;

	// Advanced particle controlling
	byte						nRootAttachType;
	attachedparticle_toent_t	nAttachToEntity;
	CCopyableUtlVector<attachedparticlecontrolpoint_t> vecControlPoints;
};

//-----------------------------------------------------------------------------
// Item Attribute
//-----------------------------------------------------------------------------

class IEconItemAttributeDefinition
{
public:
	virtual uint16 GetDefinitionIndex( void ) const	= 0;
	virtual const char	*GetDefinitionName( void ) const = 0;

	virtual const char *GetDescriptionString( void ) const = 0;
	virtual const char *GetAttributeClass( void ) const	= 0;

	virtual KeyValues* GetRawDefinition( void ) const = 0;
};

//-----------------------------------------------------------------------------
// Loot List
//-----------------------------------------------------------------------------

class IEconLootListDefinition
{
public:
	virtual const char *GetName() const = 0;
	virtual const CUtlVector<item_list_entry_t>& GetLootListContents() const = 0;
	virtual float GetWeight( int iIdx ) const = 0;
	virtual KeyValues* GenerateKeyValues() const = 0;
	virtual bool IsServerList() const = 0;

	// Additional Drops
	virtual int GetAdditionalDropCount( void ) const = 0;
	virtual bool GetAdditionalDrop( int iIndex, CUtlString& strLootList, float& flChance ) const = 0;

	// Random Attributes
	virtual int GetRandomAttributeGroupCount( void ) const = 0;
	virtual bool GetRandomAttributeGroup( int iIndex, float& flChance, float& flTotalWeight ) const = 0;
	virtual int GetRandomAttributeCount( int iGroup ) const = 0;
	virtual bool GetRandomAttribute( int iGroup, int iIndex, float& flWeight, int& iValue, int& iDefIndex ) const = 0;

	// Editable Interface
	virtual void PurgeItems( void ) = 0;
};

//-----------------------------------------------------------------------------
// Item Set
//-----------------------------------------------------------------------------
class IEconItemSetDefinition
{
public:
	virtual const char* GetName( void ) const = 0;
	virtual const char* GetLocKey( void ) const = 0;
	virtual const char* GetUnlocalizedName( void ) const = 0;
	virtual int			GetBundle( void ) const = 0;
	virtual int			GetItemCount( void ) const = 0;
	virtual int			GetItemDef( int iIndex ) const = 0;
	virtual int			GetItemPaintKit( int iIndex ) const = 0;
	virtual item_definition_index_t GetCraftReward( void ) const = 0;
	virtual int			GetItemRarity( int iIndex ) const = 0;
	virtual int			GetHighestItemRarityValue( void ) const = 0;
};

//-----------------------------------------------------------------------------
// Static Item Data
//-----------------------------------------------------------------------------
class IEconItemDefinition
{
public:
	virtual item_definition_index_t	GetDefinitionIndex( void ) const = 0;
	virtual const char* GetPrefabName( void ) const	= 0;
	virtual const char* GetItemBaseName( void ) const = 0;
	virtual const char* GetItemTypeName( void ) const = 0;
	virtual const char* GetItemDesc( void ) const = 0;
	virtual const char* GetInventoryImage( void ) const	= 0;
	virtual const char* GetBasePlayerDisplayModel() const = 0;
	virtual const char* GetWorldDisplayModel() const = 0;
	virtual const char* GetExtraWearableModel( void ) const = 0;
	virtual int			GetLoadoutSlot( void ) const = 0;
	virtual KeyValues*	GetRawDefinition( void ) const = 0;
	virtual int			GetHeroID( void ) const = 0;
	virtual uint8		GetRarity( void ) const = 0;
	virtual const CUtlVector< int >& GetItemSets( void ) const = 0;

	virtual int GetBundleItemCount( void ) const = 0;
	virtual int GetBundleItem( int iIndex ) const = 0;

	virtual bool IsBaseItem( void ) const = 0;
	virtual bool IsPublicItem( void ) const = 0;
	virtual bool IsBundle( void ) const = 0;
	virtual bool IsPackBundle( void ) const = 0;
	virtual bool IsPackItem( void ) const = 0;

	virtual void BInitVisualBlockFromKV( KeyValues *pKVItem, IEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL ) = 0;
};

//-----------------------------------------------------------------------------
// Item Schema
//-----------------------------------------------------------------------------
class IEconItemSchema
{
public:
	// Schema Parsing
	virtual bool BInit( const char *fileName, const char *pathID, CUtlVector<CUtlString> *pVecErrors = NULL ) = 0;
	virtual bool BInitLootLists( KeyValues *pKVLootLists, KeyValues *pKVRandomAttributeTemplates, CUtlVector<CUtlString> *pVecErrors, bool bServerLists ) = 0;

	virtual KeyValues*	GetRawDefinition( void ) const = 0;

	virtual int GetItemDefinitionCount() = 0;
	virtual IEconItemDefinition* GetItemDefinitionInterface( int iDefIndex ) = 0;
	virtual IEconItemDefinition* GetDefaultItemDefinitionInterface() = 0;

	virtual int GetLoadoutSlotCount() = 0;
	virtual const char* GetLoadoutSlotName( int iSlot ) = 0;
	virtual int GetLoadoutSlot( const char* pszSlotName ) = 0;
	virtual int GetCharacterSlotType( int iCharacter, int iSlot ) = 0;
	virtual int GetCharacterID( const char* pszCharacterName ) = 0;
	virtual int GetCharacterCount( void ) = 0;
	virtual const char* GetCharacterName( int iCharacter ) = 0;
	virtual const char* GetCharacterModel( int iCharacter ) = 0;

	// Particle Systems
	virtual attachedparticlesystem_t* GetAttributeControlledParticleSystem( int id ) = 0;
	virtual attachedparticlesystem_t* GetAttributeControlledParticleSystemByIndex( int id ) = 0;
	virtual attachedparticlesystem_t* FindAttributeControlledParticleSystem( const char *pchSystemName, int *outID = NULL ) = 0;
	virtual int GetNumAttributeControlledParticleSystems() const = 0;

	// Quality
	virtual int GetQualityDefinitionCount( void ) = 0;
	virtual const char* GetQualityName( uint8 iQuality ) = 0;
	virtual int GetQualityIndex( const char* pszQuality ) = 0;

	// Rarity
	virtual int GetRarityDefinitionCount( void ) const = 0;
	virtual const char* GetRarityName( uint8 iRarity ) = 0;
	virtual const char* GetRarityLocKey( uint8 iRarity ) = 0;
	virtual const char* GetRarityColor( uint8 iRarity ) = 0;
	virtual const char* GetRarityLootList( uint8 iRarity ) = 0;
	virtual int GetRarityIndex( const char* pszRarity ) = 0;

	// Attributes
	virtual const IEconItemAttributeDefinition *GetAttributeDefinitionInterface( int iAttribIndex ) const = 0;

	// Item Sets
	virtual int GetItemSetCount( void ) const = 0;
	virtual const IEconItemSetDefinition* GetItemSet( int iIndex ) const = 0;

	// Loot Lists
	virtual void PurgeLootLists( void ) = 0;
	virtual const IEconLootListDefinition* GetLootListInterfaceByName( const char* pListName, int *out_piIndex = NULL ) = 0;
	virtual const IEconLootListDefinition* GetLootListInterfaceByIndex( int iIdx ) const = 0;
	virtual const int GetLootListIndex( const char* pListName ) = 0;
	virtual const int GetLootListInterfaceCount( void ) const = 0;

	virtual KeyValues *FindDefinitionPrefabByName( const char *pszPrefabName ) const = 0;

	// Schema Init, for Editing
	virtual attachedparticlesystem_t GetAttachedParticleSystemInfo( KeyValues* pParticleSystemKV, int32 nItemIndex ) const = 0;
	virtual bool BInitAttributeControlledParticleSystems( KeyValues *pKVParticleSystems, CUtlVector<CUtlString> *pVecErrors ) = 0;
	virtual bool BInitItems( KeyValues *pKVAttributes, CUtlVector<CUtlString> *pVecErrors ) = 0;
	virtual bool BInitItemSets( KeyValues *pKVItemSets, CUtlVector<CUtlString> *pVecErrors ) = 0;

	virtual bool DeleteItemDefinition( int iDefIndex ) = 0;

	// Iterating over the item definitions. 
	virtual CEconItemDefinition *GetItemDefinitionByName( const char *pszDefName ) = 0;
	virtual const CEconItemDefinition *GetItemDefinitionByName( const char *pszDefName ) const = 0;

#if defined(CLIENT_DLL) || defined(GAME_DLL) || defined(CLIENT_EDITOR_DLL)
	virtual void ItemTesting_CreateTestDefinition( int iCloneFromItemDef, int iNewDef, KeyValues *pNewKV ) = 0;
	virtual void ItemTesting_DiscardTestDefinition( int iDef ) = 0;
#endif

	// sound materials
	virtual const char* GetSoundMaterialNameByID( int nSoundMaterialID ) = 0;
	virtual int GetSoundMaterialID( const char* pszSoundMaterial ) = 0;

	// iterating sound materials
	virtual int GetSoundMaterialCount( void ) = 0;
	virtual int GetSoundMaterialIDByIndex( int nIndex ) = 0;			// returns the Nth sound material ID
};

//-----------------------------------------------------------------------------
// Item System
//-----------------------------------------------------------------------------
class IEconItemSystem
{
public:
	virtual IEconItemSchema* GetItemSchemaInterface() = 0;

	// Item Reservations
	virtual void RequestReservedItemDefinitionList() = 0;
	virtual void ReserveItemDefinition( uint32 nDefIndex, const char* pszUserName ) = 0;
	virtual void ReleaseItemDefReservation( uint32 nDefIndex, const char* pszUserName ) = 0;
};

#endif // __ECON_ITEM_SYSTEM_INTERFACE_H
