//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#ifndef ECON_ITEM_SYSTEM_H
#define ECON_ITEM_SYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "econ_item_view.h"
#include "game_item_schema.h"
#include "game/shared/econ/iecon_item_system.h"

//==================================================================================
// ITEM SYSTEM
//==================================================================================

#define INVALID_ITEM_INDEX			-1

#define GC_MOTD_CACHE_FILE			"cfg/motd_entries.txt"

// The item system parses and creates a list of item classes from the item_classes.txt file.

//-----------------------------------------------------------------------------
// Criteria used by the system to generate a random attribute
struct randomattributecriteria_t
{
	randomattributecriteria_t()
	{
		entityType = AE_MAX_TYPES;
		iAttributeLevel = 0;
		pItem = NULL;
	}

	entityquality_t		entityType;
	int					iAttributeLevel;
	CEconItemView		*pItem;
};

//-----------------------------------------------------------------------------
// Purpose: Container for all the static item definitions in the item data files
//-----------------------------------------------------------------------------
class CEconItemSystem : public IEconItemSystem
{
public:
	CEconItemSystem( void );
	virtual ~CEconItemSystem( void );

	// Setup & parse in the item data files.
	void		Init( void );
	void		Shutdown( void );

	// Return the static item data for the specified item index
	const GameItemDefinition_t *GetStaticDataForItemByDefIndex( item_definition_index_t iItemDefIndex ) const
	{
		return static_cast< const GameItemDefinition_t* >( m_itemSchema.GetItemDefinition( iItemDefIndex ) );
	}
	const CEconItemDefinition *GetStaticDataForItemByName( const char *pszDefName ) const 
	{ 
		return m_itemSchema.GetItemDefinitionByName( pszDefName );
	}
	const CEconItemAttributeDefinition *GetStaticDataForAttributeByDefIndex( attrib_definition_index_t iAttribDefinitionIndex ) const
	{ 
		return m_itemSchema.GetAttributeDefinition( iAttribDefinitionIndex );
	}
	const CEconItemAttributeDefinition *GetStaticDataForAttributeByName( const char *pszDefName ) const
	{ 
		return m_itemSchema.GetAttributeDefinitionByName( pszDefName );
	}

	// Select and return a random item's definition index matching the specified criteria
	int	GenerateRandomItem( CItemSelectionCriteria *pCriteria, entityquality_t *outEntityQuality );

	// Select and return the base item definition index for a class's load-out slot 
	// Note: baseitemcriteria_t is game-specific and/or may not exist!
	virtual int GenerateBaseItem( struct baseitemcriteria_t *pCriteria ) { return INVALID_ITEM_INDEX; }

	// Return a random item quality
	entityquality_t GetRandomQualityForItem( bool bPreventUnique = false );

	// Generate an attribute of the specified definition
	CEconItemAttribute *GenerateAttribute( attrib_definition_index_t iAttributeDefinition, float flValue );

	// Generate an attribute by name. Used for debugging.
	CEconItemAttribute *GenerateAttribute( const char *pszName, float flValue );

	// Decrypt the item files and return the keyvalue
	bool	DecryptItemFiles( KeyValues *pKV, const char *pName );		

	GameItemSchema_t *GetItemSchema() { return &m_itemSchema; }

	// Open the server's whitelist, and if it exists, set the appropriate items allowed.
	void		ReloadWhitelist( void );

	void		ResetAttribStringCache( void );

	// Public interface.
	virtual IEconItemSchema* GetItemSchemaInterface() { return GetItemSchema(); }
	virtual void RequestReservedItemDefinitionList() {};
	virtual void ReserveItemDefinition( uint32 nDefIndex, const char* pszUserName ) {};
	virtual void ReleaseItemDefReservation( uint32 nDefIndex, const char* pszUserName ) {};

protected:
	// Read the specified item schema file. Init the item schema with the contents
	void		ParseItemSchemaFile( const char *pFilename );

	// Key to decrypt the item description files
	const unsigned char *GetEncryptionKey( void ) { return (unsigned char *)"A5fSXbf7"; }

private:
	GameItemSchema_t m_itemSchema;
};

CEconItemSystem *ItemSystem( void );

// CGCFetchWebResource
class CGCFetchWebResource : public GCSDK::CGCClientJob
{
public:
	CGCFetchWebResource( GCSDK::CGCClient *pClient, CUtlString strName, CUtlString strURL, bool bForceSkipCache = false, KeyValues *pkvGETParams = NULL );
	virtual ~CGCFetchWebResource();

	bool BYieldingRunGCJob();
	void OnHTTPCompleted( HTTPRequestCompleted_t *arg, bool bFailed );

private:
	CUtlString m_strName;
	CUtlString m_strURL;
	bool m_bForceSkipCache;
	bool m_bHTTPCompleted;
	KeyValues *m_pkvGETParams;
	CCallResult< CGCFetchWebResource, HTTPRequestCompleted_t > callback;
};

#endif // ECON_ITEM_SYSTEM_H
