
#ifndef ECONITEMDESCRIPTION_H
#define ECONITEMDESCRIPTION_H

#ifdef _WIN32
#pragma once
#endif

#include "localization_provider.h"		// needed for locchar_t type

class IEconItemInterface;
namespace GCSDK
{
	class CSharedObjectTypeCache;
}

//-----------------------------------------------------------------------------
// Purpose: Generate a description block for an IEconItemInterface. What the
//			client does with the description is anyone's guess, but this will
//			generate a block of UTF16 lines of text with meta/color data that
//			can be used for whatever.
//-----------------------------------------------------------------------------

enum EDescriptionLineMetaFlags
{
	kDescLineFlag_Name			= 0x0001,						// the item name (can be renamed by user)
	kDescLineFlag_Type			= 0x0002,						// the item type (ie., "Level 5 Rocket Launcher")
	kDescLineFlag_Desc			= 0x0004,						// base item description (description from the item definition, level, etc.)
	kDescLineFlag_Attribute		= 0x0008,						// some sort of gameplay-affecting attribute
	kDescLineFlag_Misc			= 0x0010,						// not an attribute, not name/level
	kDescLineFlag_Empty			= 0x0020,						// line with no content that needs to be displayed; meant for spacing
	kDescLineFlag_Set			= 0x0040,						// this line is associated with item sets somehow
	kDescLineFlag_LimitedUse	= 0x0080,						// this is a limited use item
	kDescLineFlag_SetName		= 0x0100,						// this line is the title for an item set
	kDescLineFlag_Scorecard		= 0x0200,
	kDescLineFlag_LootList		= 0x0400,						// this line belongs to a lootlist block
	kDescLineFlag_OwnerOnly		= 0x0800,						// this line must be only shown for owner of the item
	kDescLineFlag_DetailedInfo	= 0x1000,						// this line is detailed info like mission xp
	kDescLineFlag_NameUncustomized = 0x2000,						// the item name (not customized)

	kDescLineFlagSet_DisplayInAttributeBlock = ~(kDescLineFlag_Name | kDescLineFlag_Type),
};

struct econ_item_description_line_t
{
	attrib_colors_t					eColor;						// desired color type for this line -- will likely be looked up either in VGUI or in the schema
	uint32							unMetaType;					// type information for this line -- "item name"? "item level"?; etc.; can be game-specific
	CUtlConstStringBase<locchar_t>	sText;						// actual text for this, post-localization
	itemid_t						un64FauxItemId;				// defindex and paint encoded as an itemid.
	bool							bOwned;				// a way to keep track of set/collection item presence
};

class IEconItemDescription
{
public:
	// This may yield on the GC and should never yield on the client.
	static void YieldingFillOutEconItemDescription( IEconItemDescription *out_pDescription, CLocalizationProvider *pLocalizationProvider, const IEconItemInterface *pEconItem );


public:
	IEconItemDescription() { }
	virtual ~IEconItemDescription() { }

	uint32 GetLineCount() const { return m_vecDescLines.Count(); }
	const econ_item_description_line_t& GetLine( int i ) const { return m_vecDescLines[i]; }

	// Finds and returns the first line with *all* of the passed in search flags. Will return NULL if a line
	// will all of the flags cannot be found.
	const econ_item_description_line_t *GetFirstLineWithMetaType( uint32 unMetaTypeSearchFlags ) const;

private:
	// When generating an item description, this is guaranteed to be called once and only once before GenerateDescription()
	// is called. Any data that may yield but will be needed somewhere deep inside GenerateDescription() should be determined
	// and cached off here.
	virtual void YieldingCacheDescriptionData( CLocalizationProvider *pLocalizationProvider, const IEconItemInterface *pEconItem ) { }

	// Take the properties off our pEconItem, and anything that we calculated in YieldingCacheDescriptionData() above and
	// fill out all of our description lines.
	virtual void GenerateDescriptionLines( CLocalizationProvider *pLocalizationProvider, const IEconItemInterface *pEconItem ) = 0;

protected:
	CUtlVector<econ_item_description_line_t> m_vecDescLines;
};

#if defined( TF_DLL ) || defined( TF_CLIENT_DLL ) || defined( TF_GC_DLL )
	#define PROJECT_TF
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CSteamAccountIDAttributeCollector : public IEconItemAttributeIterator
{
public:
	virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, attrib_value_t value ) OVERRIDE
	{
		if ( pAttrDef->GetDescriptionFormat() == ATTDESCFORM_VALUE_IS_ACCOUNT_ID )
		{
			m_vecSteamAccountIDs.AddToTail( value );
		}

		return true;
	}

	virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, float value ) OVERRIDE
	{
		// Intentionally ignore.
		return true;
	}

	virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const CAttribute_String& value ) OVERRIDE
	{
		// Intentionally ignore.
		return true;
	}

	virtual bool OnIterateAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const Vector& value ) OVERRIDE
	{
		// Intentionally ignore.
		return true;
	}

	// Data access.
	const CUtlVector<uint32>& GetAccountIDs()
	{
		return m_vecSteamAccountIDs;
	}

private:
	CUtlVector<uint32> m_vecSteamAccountIDs;
};

#endif // ECONITEMDESCRIPTION_H
