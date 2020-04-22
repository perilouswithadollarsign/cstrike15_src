//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: EconItemSchema: Defines a schema for econ items
//
//=============================================================================

#ifndef ECONITEMSCHEMA_H
#define ECONITEMSCHEMA_H
#ifdef _WIN32
#pragma once
#endif

// Valve code doesn't play nicely with standard headers on some platforms sometimes.
#ifdef min
	#undef min
#endif

#ifdef max
	#undef max
#endif

#include <string>
#include "steam/steamtypes.h"				// needed for RTime32
#include "keyvalues.h"
#include "tier1/utldict.h"
#include "econ_item_constants.h"
#include "tier1/utlhashmaplarge.h"
#include "UtlStringMap.h"

#include "item_selection_criteria.h"
#include "bitvec.h"
#include "language.h"
#include "smartptr.h"
#include "vstdlib/random.h"
#include "materialsystem/imaterialsystem.h"

#include "mathlib/expressioncalculator.h"

#if defined(CLIENT_DLL) || defined(GAME_DLL)
#include "weapon_parse.h"
#include "engine/ivmodelinfo.h"
#include "engine/ivmodelrender.h"
#endif

class CEconItem;
#include "game/shared/econ/iecon_item_system.h"

#ifdef SOURCE1
#include "gamestringpool.h"
#endif

class CEconItemSchema;
class CEconSharedObjectCache;
class CSOItemRecipe;
class CStickerList;

class CTimedItemRewardDefinition;
void Helper_TemporaryBuildCopyOfLootlistForQuestDrop( CTimedItemRewardDefinition &build, const CTimedItemRewardDefinition *pDescReward, char const *szGameModeExclude );

#define MAX_PAINT_DATA_NAME 128

#define MAX_STICKER_DATA_PATH 128

#define DEFAULT_EMBROIDER_NCOLORS 10
#define DEFAULT_EMBROIDER_GAMMA 0.8

enum CraftFilter_t
{
	CRAFT_FILTER_TRADEUP = -3,
	CRAFT_FILTER_COLLECT = -2,
};

enum EQuestVar_t
{
	/** Removed for partner depot **/
	k_EQuestVar_First,
	k_EQuestVar_Last
};

//-----------------------------------------------------------------------------
// CEconItemRarityDefinition
//-----------------------------------------------------------------------------
class CEconItemRarityDefinition
{
public:
	CEconItemRarityDefinition( void );
	CEconItemRarityDefinition( const CEconItemRarityDefinition &that );
	CEconItemRarityDefinition &operator=( const CEconItemRarityDefinition& rhs );

	~CEconItemRarityDefinition( void ) { }

	bool		BInitFromKV( KeyValues *pKVItem, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );

	int32		GetDBValue( void ) const			{ return m_nValue; }
	const char	*GetName( void ) const				{ return !m_strName.IsEmpty() ? m_strName.String() : "unknown"; }
	const char  *GetLocKey( void ) const			{ return m_strLocKey.String(); }
	const char  *GetWepLocKey( void ) const			{ return m_strWepLocKey.String(); }
	const char  *GetLootList( void ) const			{ return m_strLootList.String(); }
	const char  *GetRecycleLootList( void ) const			{ return m_strRecycleLootList.String(); }
	const char  *GetDropSound( void ) const			{ return m_strDropSound.String(); }	
	attrib_colors_t		GetAttribColor( void ) const		{ return m_iAttribColor; }
	const char	*GetNextRarity( void ) const		{ return m_strNextRarity.String(); }
	int			GetWhiteCount( void ) const			{ return m_iWhiteCount; }
	int			GetBlackCount( void ) const			{ return m_iBlackCount; }
	float		GetWeight( void ) const				{ return m_flWeight; }

private:

	// The value that the game/DB will know this rarity by
	int32		m_nValue;

	attrib_colors_t		m_iAttribColor;

	// The English name of the rarity
	CUtlString	m_strName; 

	// The localization key for this rarity.
	CUtlString  m_strLocKey;
	// The localization key for this rarity, for weapons.
	CUtlString  m_strWepLocKey;

	// The loot list name associated with this rarity.
	CUtlString  m_strLootList;
	CUtlString  m_strRecycleLootList;
	CUtlString  m_strDropSound;
	
	CUtlString  m_strNextRarity;

	int			m_iWhiteCount;
	int			m_iBlackCount;

	float		m_flWeight;
};

typedef int		econ_tag_handle_t;
typedef	uint16	equipped_class_t;
typedef uint16	equipped_slot_t;
typedef uint8	equipped_preset_t;

#define INVALID_ECON_TAG_HANDLE	((econ_tag_handle_t)-1)
#define INVALID_EQUIPPED_SLOT	((equipped_slot_t)-1)
#define INVALID_EQUIPPED_SLOT_BITPACKED 0x3F
#define INVALID_STYLE_INDEX		((style_index_t)-1)
#define INVALID_PRESET_INDEX	((equipped_preset_t)-1)

union attribute_data_union_t
{
	float asFloat;
	uint32 asUint32;
	byte *asBlobPointer;
};

struct static_attrib_t
{
	static_attrib_t()
	{
		iDefIndex = 0;
		m_value.asBlobPointer = NULL;
		m_bForceGCToGenerate = false;

	}

	attrib_definition_index_t	iDefIndex;
	attribute_data_union_t m_value;
	bool	m_bForceGCToGenerate;

	// Parses a single subsection from a multi-line attribute block that looks like:
	//
	//		"attributes"
	//		{
	//			"cannot trade"
	//			{
	//				"attribute_class"	"cannot_trade"
	//				"value"				"1"
	//			}
	//			"kill eater"
	//			{
	//				"attribute_class"	"kill_eater"
	//				"force_gc_to_generate" "1"
	//				"use_custom_logic"	"gifts_given_out"
	//			}
	//		}
	//
	// The "force_gc_to_generate" and "use_custom_logic" fields will only be parsed on the GC. Will return
	// true/false based on whether the whole attribute and value parsed successfully.
	bool BInitFromKV_MultiLine( const char *pszContext, KeyValues *pKVAttribute, CUtlVector<CUtlString> *pVecErrors );

	// Parses a single subsection from a single-line attribute block that looks like:
	//
	//		CharacterAttributes 
	//		{
	//			"increase buff duration"	9.0
	//			"damage bonus"	2.0 
	//		}
	//
	// It's impossible to specify GC-generated attributes in this format. Will return true/false based on
	// whether the whole attribute and value parsed successfully.
	bool BInitFromKV_SingleLine( const char *pszContext, KeyValues *pKVAttribute, CUtlVector<CUtlString> *pVecErrors );

	// Data access helper.
	const class CEconItemAttributeDefinition *GetAttributeDefinition() const;
};

enum EWebResourceStatus
{
	kWebResource_InvalidName,
	kWebResource_NotLoaded, 
	kWebResource_Loading, 
	kWebResource_Loaded, 
};

struct WeaponPaintableMaterial_t
{
	char m_szName[ MAX_PAINT_DATA_NAME ];
	char m_szOriginalMaterialName[ MAX_PAINT_DATA_NAME ];
	char m_szFolderName[ MAX_PAINT_DATA_NAME ];
	int m_nViewModelSize;						// texture size
	int m_nWorldModelSize;						// texture size
	float m_flWeaponLength;
	float m_flUVScale;
	bool m_bBaseTextureOverride;
	bool m_bMirrorPattern;
};

struct InventoryImageData_t
{
	QAngle *m_pCameraAngles;
	Vector *m_pCameraOffset;
	float m_cameraFOV;
	LightDesc_t *m_pLightDesc[ MATERIAL_MAX_LIGHT_COUNT ];
	bool m_bOverrideDefaultLight;
};

struct StickerData_t
{
	char	m_szStickerModelPath[ MAX_STICKER_DATA_PATH ];
	char	m_szStickerMaterialPath[ MAX_STICKER_DATA_PATH ];
	Vector	m_vWorldModelProjectionStart;
	Vector	m_vWorldModelProjectionEnd;
	char	m_szStickerBoneParentName[ 32 ];
};

//-----------------------------------------------------------------------------
// CEconItemQualityDefinition
//-----------------------------------------------------------------------------
class CEconItemQualityDefinition
{
public:
	CEconItemQualityDefinition( void );
	CEconItemQualityDefinition( const CEconItemQualityDefinition &that );
	CEconItemQualityDefinition &operator=( const CEconItemQualityDefinition& rhs );

	~CEconItemQualityDefinition( void ) { }

	bool		BInitFromKV( KeyValues *pKVItem, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );


	int32		GetDBValue( void ) const			{ return m_nValue; }
	const char	*GetName( void ) const				{ return !m_strName.IsEmpty() ? m_strName.Get() : "unknown"; }
	uint32		GetWeight( void ) const				{ return m_unWeight; }
	bool		GetRequiresExplicitMatches( void ) const { return m_bExplicitMatchesOnly; }
	bool		CanSupportSet( void ) const			{ return m_bCanSupportSet; }
	const char	*GetHexColor( void ) const			{ return !m_strHexColor.IsEmpty() ? m_strHexColor.Get() : "B2B2B2"; }

private:

	// The value that the game/DB will know this quality by
	int32			m_nValue;

	// The English name of the quality
	CUtlConstString	m_strName;

	// The weight used for choosing the quality. The higher the weight, the more likely it will be chosen.
	uint32		m_unWeight; 

	// Whether or not items chosen to be of this quality only match item definitions that are explicitly defined as being of this quality
	bool		m_bExplicitMatchesOnly;

	// if this is true the support tool is allowed to set this quality level on any item
	bool			m_bCanSupportSet;

	// A hex string representing the color this quality should display as. Used primarily for display on the Web.
	CUtlConstString	m_strHexColor;
};

//-----------------------------------------------------------------------------
// CEconColorDefinition
//-----------------------------------------------------------------------------
class CEconColorDefinition
{
public:
	bool		BInitFromKV( KeyValues *pKVColor, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );

	const char *GetName( void ) const			{ return m_strName.Get(); }
	const char *GetColorName( void ) const		{ return m_strColorName.Get(); }		// meant for passing into VGUI styles, etc.
	const char *GetHexColor( void ) const		{ return m_strHexColor.Get(); }

private:
	// The English name of this color. Only used for lookup.
	CUtlConstString m_strName;

	// The VGUI name of the color in our schema. This will be used to set values
	// for VGUI controls.
	CUtlConstString m_strColorName;

	// The hex string value of this color. This will be used for Web display.
	CUtlConstString m_strHexColor;
};

//-----------------------------------------------------------------------------
// CEconColorDefinition
//-----------------------------------------------------------------------------
class CEconGraffitiTintDefinition
{
public:
	bool		BInitFromKV( KeyValues *pKVColor, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );

	int GetID( void ) const			{ return m_nID; }
	const char *GetColorName( void ) const		{ return m_strColorName.Get(); }		// meant for passing into VGUI styles, etc.
	const char *GetHexColor( void ) const		{ return m_strHexColor.Get(); }
	uint32 GetHexColorRGB( void ) const			{ return m_unHexColorRGB; }

private:
	// The ID of the color
	int m_nID;

	// The name of the color in our schema (e.g. "shark_white")
	CUtlConstString m_strColorName;

	// The hex string value of this color. This will be used for Web display. (e.g. "#ff00ff")
	CUtlConstString m_strHexColor;
	uint32 m_unHexColorRGB;
};

//-----------------------------------------------------------------------------
// CEconMusicDefinition
//-----------------------------------------------------------------------------
class CEconMusicDefinition
{
public:
	bool		BInitFromKV( KeyValues *pKVMusicDef, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );

	uint32		GetID( void ) const						{ return nID; }
	const char *GetName( void ) const					{ return m_strName.Get(); }
	const char *GetNameLocToken( void ) const			{ return m_strNameLocToken.Get(); }
	const char *GetDescription( void ) const			{ return m_strLocDescription.Get(); }
	const char *GetInventoryImage( void ) const			{ return m_strInventoryImage.Get(); }
	const char *GetPedestalDisplayModel( void ) const	{ return m_strPedestalDisplayModel.Get(); }

	// Icon URLs for CDN
	const char  *GetIconURLSmall() const			{ return m_sIconURLSmall; }
	const char  *GetIconURLLarge() const			{ return m_sIconURLLarge; }
	void	SetIconURLSmall( const char *szURL )	{ m_sIconURLSmall = szURL; }
	void	SetIconURLLarge( const char *szURL )	{ m_sIconURLLarge = szURL; }

private:

	int				nID;
	CUtlConstString m_strName;
	CUtlConstString m_strNameLocToken;
	CUtlConstString	m_strLocDescription;
	CUtlConstString m_strPedestalDisplayModel;
	CUtlConstString	m_strInventoryImage;

	CUtlConstString	m_sIconURLSmall;
	CUtlConstString	m_sIconURLLarge;
};


class CEconQuestDefinition;

struct quest_event_t
{
	quest_event_t( int nStartDay, const char * pchHHMM, int nDurationInMinutes, const char * strTimeZone, CEconQuestDefinition * pQuestDef, RTime32 start, RTime32 end ) :		m_strHHMM( pchHHMM ), 
																																												m_rtEventStart( start ),
																																												m_rtEventEnd( end ),
																																												m_nStartDay( nStartDay ),
																																												m_nDurationInMinutes( nDurationInMinutes ),
																																												m_pQuestDef( pQuestDef ),
																																												m_strTimeZone( strTimeZone ){}
	RTime32						m_rtEventStart;
	RTime32						m_rtEventEnd;
	int							m_nDurationInMinutes;
	int							m_nStartDay;
	CEconQuestDefinition *		m_pQuestDef;
	CUtlConstString				m_strHHMM;
	CUtlConstString				m_strTimeZone;


};

typedef CUtlVector<quest_event_t*> QuestEvents_t;
typedef CUtlMap< RTime32, quest_event_t*, int, CDefLess< RTime32 > > QuestEventsSchedule_t;	// a map of quest events that gets resorted by start time every time it's queried


//-----------------------------------------------------------------------------
// CEconQuestDefinition
//-----------------------------------------------------------------------------
class CEconQuestDefinition
{
public:
	bool		BInitFromKV( KeyValues *pKVQuestDef, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );

	uint32									GetID( void ) const						{ return nID; }
	const char *							GetName( void ) const					{ return m_strName.Get(); }

	const char *							GetGameMode( void ) const				{ return m_strGameMode.Get(); }
	const char *							GetMapGroup( void ) const				{ return m_strMapGroup.Get(); }
	const char *							GetMap( void ) const					{ return m_strMap.Get(); }
	const char *							GetRewardLootList( void ) const			{ return m_strRewardLootList.Get(); }

	const char *							GetQuestExpression( void ) const		{ return m_strExpr; }
	const char *							GetQuestBonusExpression( void ) const	{ return m_strBonusExpr; }

	const uint32							GetDifficulty( void ) const				{ return m_nDifficulty; }
	const uint32							GetOperationalPoints( void ) const		{ return m_nOperationalPoints; }
	const uint32							GetXPReward( void ) const				{ return m_nXPReward; }
	const uint32							GetTargetTeam( void ) const				{ return m_nTargetTeam; }

	const uint32							GetXPBonusPercent( void ) const			{ return m_nXPBonusPercent; }

	const CCopyableUtlVector< uint32 >&		GetQuestPoints( void ) const			{ return m_vecQuestPoints; }

	const char*								GetQuestConVars( void ) const { return m_strQuestConVars.Get(); }
	const bool								IsAnEvent( void ) const	{ return m_bIsAnEvent; }

	static void								TokenizeQuestExpression( const char * szExpression, KeyValues * pKVExpressionTokens );
	static bool								IsQuestExpressionValid( const char * pszQuestExpr );
	static void								ZeroOutQuestExpressionVariables( CExpressionCalculator &expQuest );
	static void								SetQuestExpressionVariable( CExpressionCalculator &expQuest, EQuestVar_t questVar, float flValue );

private:

	int				nID;
	CUtlConstString m_strName;

	CUtlConstString	m_strMapGroup;
	CUtlConstString m_strMap;
	CUtlConstString	m_strGameMode;
	CUtlConstString m_strRewardLootList;
	
	CUtlConstString m_strExpr;
	CUtlConstString m_strBonusExpr;

	CCopyableUtlVector< uint32 > m_vecQuestPoints;		// how many times does the player need to do the action.
														// schema specifies a list of possible point requirements and 
														// one of the options is selected when the quest is created.

	bool								m_bIsAnEvent;				//Event quests are treated slightly differently.

	uint32								m_nDifficulty;				// quest difficulty for display purposes
	uint32								m_nOperationalPoints;		// points towards coin leveling
	uint32								m_nXPReward;				// xp reward for completing the quest

	uint32								m_nXPBonusPercent;			// bonus xp percentage for completing additionally satisfying the Bonus expression
	uint32								m_nTargetTeam;				// specific team this quest applies to (0 if not applicable)

	CUtlConstString						m_strQuestConVars;			// Cfg file to exec on the server when starting this quest 

//
// client strings. Server and GC don't use these
//
#ifdef CLIENT_DLL 

public:
	const char *							GetNameLocToken( void ) const			{ return m_strNameLocToken.Get(); }
	const char *							GetShortNameLocToken( void ) const		{ return m_strShortNameLocToken.Get(); }
	const char *							GetDescriptionLocToken( void ) const		{ return m_strDescriptionLocToken.Get(); }
	const char *							GetHudDesscriptionLocToken( void ) const	{ return m_strHudDescriptionLocToken.Get(); }
	const char *							GetBonusLocToken( void ) const			{ return m_strLocBonus.Get(); }
	KeyValues*								GetStringTokens( void ) const			{ return m_kvStringTokens; }
	const char*								GetIcon( void ) const { return m_strIcon; }

	static void PopulateQuestStringTokens( CEconQuestDefinition &questDef, KeyValues &kvExpressionTokens, KeyValues &kvStringTokens, bool bBonus = false );

private:
	CUtlConstString m_strNameLocToken;
	CUtlConstString m_strShortNameLocToken;
	CUtlConstString	m_strDescriptionLocToken;
	CUtlConstString	m_strHudDescriptionLocToken;
	KeyValues * m_kvStringTokens;
	CUtlConstString m_strLocBonus;
	CUtlConstString m_strIcon;

#endif

};



//-----------------------------------------------------------------------------
// CEconCampaignDefinition
//-----------------------------------------------------------------------------
class CEconCampaignDefinition
{
public:

	class CEconCampaignNodeDefinition
	{
	public:

#ifdef CLIENT_DLL
		class CEconCampaignNodeStoryBlockDefinition
		{
		public:
			bool		BInitFromKV( int nCampaignIndex, int nNodeID, KeyValues * pKVCampaignNodeStoryBlockDef, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );

			const char *					GetContentFile( void ) const					{ return m_strContentFile.Get(); }
			const char *					GetCharacterName( void ) const				{ return m_strCharacterName.Get(); }
			const char *					GetStoryBlockExpression( void ) const		{ return m_strExpr.Get(); }
			const char *					GetDescription( void ) const				{ return m_strDescription.Get(); }

			float							EvaluateStoryBlockExpression( CEconItemView *pCampaignCoin ) const;

		private:
			CUtlConstString			m_strContentFile;			// mission briefing or other content associated with this node
			CUtlConstString			m_strCharacterName;			// Character associated with campaign node
			CUtlConstString			m_strExpr;					// the first story block with a true expression will get selected
			CUtlConstString			m_strDescription;			// mission description or audio captioning.
		};

		const CUtlVector< CEconCampaignNodeStoryBlockDefinition* >&		GetStoryBlocks( void ) const		{ return m_vecStoryBlocks; }

		CEconCampaignNodeStoryBlockDefinition * GetBestScoringStoryBlock( CEconItemView *pCampaignCoin ) const;

	private:
				CUtlVector< CEconCampaignNodeStoryBlockDefinition* >	m_vecStoryBlocks;			// vector of story blocks
	public:
#endif

		bool		BInitFromKV( int nCampaignIndex, KeyValues *pKVCampaignNodeDef, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );

		const uint32					GetID( void ) const					{ return m_nID; }
		const uint32					GetQuestIndex( void ) const			{ return m_nQuestIndex; }
		const CUtlVector< uint32 >&		GetNextNodes( void ) const			{ return m_vecNextNodes; }
		const uint32					GetCampaignID( void ) const			{ return m_CampaignID; }



	private:

		uint32					m_nID;						// index of this node
		uint32					m_nQuestIndex;				// index of the quest
		uint32					m_CampaignID;
		CUtlVector< uint32 >	m_vecNextNodes;				// list of nodes that completion of this node unlocks



	};

	typedef CUtlMap<int, CEconCampaignNodeDefinition*, int, CDefLess<int> >	CampaignNodeDefinitionsList_t;

	bool		BInitFromKV( KeyValues *pKVCampaignDef, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );

	const uint32							GetID( void ) const						{ return m_nID; }
	const char*								GetNameLocToken( void ) const			{ return m_strNameLocToken.Get(); }
	const char*								GetDescription( void ) const			{ return m_strLocDescription.Get(); }
	const CampaignNodeDefinitionsList_t&	GetStartNodes( void ) const				{ return m_mapStartNodes; }
	const CampaignNodeDefinitionsList_t&	GetCampaignNodes( void ) const			{ return m_mapCampaignNodes; }
	uint32									GetSeasonNumber() const { return m_nSeasonNumber; }

	void								GetAccessibleCampaignNodes( const uint32 unCampaignCompletionBitfield, CUtlVector< CEconCampaignNodeDefinition* > &vecAccessibleNodes );
	void								Helper_RecursiveGetAccessibleCampaignNodes( const uint32 unCampaignCompletionBitfield, const CEconCampaignNodeDefinition* pNode, CUtlVector< CEconCampaignNodeDefinition* > &vecAccessibleNodes );


private:

	int								m_nID;
	CUtlConstString					m_strName;
	CUtlConstString					m_strNameLocToken;
	CUtlConstString					m_strLocDescription;
	CampaignNodeDefinitionsList_t	m_mapCampaignNodes;
	CampaignNodeDefinitionsList_t	m_mapStartNodes;
	uint32							m_nSeasonNumber;			// operation season this campaign applies to

};

bool ResolveQuestIdToCampaignAndIndex( uint16 unQuestID, uint32 &unCampaignID, uint32 &unCamapaignNodeID );
void TokenizeCalculatorExpression( const char * szExpression, KeyValues * pKVExpressionTokens );

struct item_list_entry_t
{
	item_list_entry_t()
	{
		memset( this, 0, sizeof( *this ) );
		// -1 : random generation of attribute value
		// -2 : do not generate and set the attribute
		m_nPaintKitSeed = -2;
		m_flPaintKitWear = -2;
	}

	bool operator==( const item_list_entry_t other ) const
	{
		return ( 
			( m_nItemDef == other.m_nItemDef ) &&
			( m_nPaintKit == other.m_nPaintKit ) &&
			( m_nPaintKitSeed == other.m_nPaintKitSeed ) &&
			( m_flPaintKitWear == other.m_flPaintKitWear ) &&
			( m_nStickerKit == other.m_nStickerKit ) &&
			( m_nMusicKit == other.m_nMusicKit ) &&
			( m_bIsNestedList == other.m_bIsNestedList ) &&
			( m_bIsUnusualList == other.m_bIsUnusualList ) &&
			( m_bAlreadyUsedInRecursiveCreation == m_bAlreadyUsedInRecursiveCreation )
			);
	}

	bool InitFromName( const char *pchName );

	// Item def
	int m_nItemDef;
	
	// Paint kit applied to it
	int m_nPaintKit;
	int m_nPaintKitSeed;
	float m_flPaintKitWear;
	
	// Sticker kit applied to it
	uint32 m_nStickerKit;

	// music kit applied to it
	uint32 m_nMusicKit;
	
	bool m_bIsNestedList;
	bool m_bIsUnusualList;
	mutable bool m_bAlreadyUsedInRecursiveCreation;
};

//-----------------------------------------------------------------------------
// CEconItemSetDefinition
// Definition of an item set
//-----------------------------------------------------------------------------
class CEconItemSetDefinition : public IEconItemSetDefinition
{
public:
	CEconItemSetDefinition( void );
	CEconItemSetDefinition( const CEconItemSetDefinition &that );
	CEconItemSetDefinition &operator=( const CEconItemSetDefinition& rhs );

	~CEconItemSetDefinition( void ) {}

	virtual bool BInitFromKV( KeyValues *pKVItemSet, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );

	virtual const char* GetName( void )	const { return m_pszName; }
//	virtual const char* GetSetName( void )	const { return m_pszSetName; }
	virtual const char* GetLocKey( void ) const { return m_pszLocalizedName; }
	virtual const char* GetUnlocalizedName( void ) const { return m_pszUnlocalizedName; }
	virtual int			GetBundle( void ) const { return m_iBundleItemDef; }
	virtual int			GetItemCount( void ) const { return m_ItemEntries.Count(); }
	virtual int			GetItemDef( int iIndex ) const { return m_ItemEntries[iIndex].m_nItemDef; }
	virtual int			GetItemPaintKit( int iIndex ) const { return m_ItemEntries[iIndex].m_nPaintKit; }
	virtual int			GetItemRarity( int iIndex ) const;
	virtual int			GetHighestItemRarityValue( void ) const;
	virtual item_definition_index_t GetCraftReward( void ) const { return m_nCraftReward; }

public:

	const char			*m_pszName;	//	e.g. "set_dust"
//	const char			*m_pszSetName;	//	unused
	const char			*m_pszLocalizedName;
	const char			*m_pszUnlocalizedName;
	const char			*m_pszLocalizedDescription;
	CUtlVector<item_list_entry_t> m_ItemEntries;
	int					m_iBundleItemDef;	// Item def of the store bundle for this set, if any
	bool				m_bIsCollection;
	bool				m_bIsHiddenSet;		// If true, this set and any bonuses will only be visible if the whole set is equipped.
	item_definition_index_t	m_nCraftReward;

	struct itemset_attrib_t
	{
		int		m_iAttribDefIndex;
		attrib_value_t	m_valValue;
	};
	CUtlVector<itemset_attrib_t>	m_iAttributes;
};

//-----------------------------------------------------------------------------
// CEconLootListDefinition
// Definition of a loot list
//-----------------------------------------------------------------------------
struct favored_lootlist_t
{
	favored_lootlist_t()
	{
		iHeroID = 0;
		iBonus = 0;
	}

	favored_lootlist_t &operator=( const favored_lootlist_t& rhs )
	{
		iHeroID = rhs.iHeroID;
		iBonus = rhs.iBonus;
		return *this;
	}

	uint iHeroID;
	uint iBonus;
};

class CEconLootListDefinition : public IEconLootListDefinition
{
public:
	CEconLootListDefinition( void );
	CEconLootListDefinition( const CEconLootListDefinition &that );
	CEconLootListDefinition &operator=( const CEconLootListDefinition& rhs );

	~CEconLootListDefinition( void );

	bool AddRandomAtrributes( KeyValues *pRandomAttributesKV, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );
	bool	BInitFromKV( KeyValues *pKVLootList, KeyValues *pKVRandomAttributeTemplates, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL, bool bServerList = false );
	virtual const char *GetName() const { return m_pszName; }
	const CUtlVector<item_list_entry_t>& GetLootListContents() const { return m_ItemEntries; }
	virtual float GetWeight( int iIdx ) const { return m_flWeights[iIdx]; }
	virtual KeyValues* GenerateKeyValues() const;
	virtual bool IsServerList() const { return m_bServerList; }
	virtual bool HasUnusualLoot() const;
//	virtual const char *GetLocName() const { return m_pszLocName; };

	// Additional Drops
	virtual int GetAdditionalDropCount( void ) const { return m_AdditionalDrops.Count(); }
	virtual bool GetAdditionalDrop( int iIndex, CUtlString& strLootList, float& flChance ) const;

	// Random Attributes
	virtual int GetRandomAttributeGroupCount( void ) const { return m_RandomAttribs.Count(); }
	virtual bool GetRandomAttributeGroup( int iIndex, float& flChance, float& flTotalWeight ) const;
	virtual int GetRandomAttributeCount( int iGroup ) const;
	virtual bool GetRandomAttribute( int iGroup, int iIndex, float& flWeight, int& iValue, int& iDefIndex ) const;

	// Editable Interface
	virtual void PurgeItems( void );

	bool GetPublicListContents() const
	{
		// clients will only have access to the loot lists that are public anyway
		return true;
	}

	struct loot_list_additional_drop_t
	{
		float		m_fChance;
		bool		m_bPremiumOnly;
		const char *m_pszLootListDefName;
	};
	CUtlVector<loot_list_additional_drop_t>	m_AdditionalDrops;

protected:
	const char			*m_pszName;

	CUtlVector<item_list_entry_t> m_ItemEntries;
	uint32				 m_unHeroID;

	bool				m_bPublicListContents;	// do not show loot list contents to users (ie., when listing crate contents on Steam)
#ifndef GC_DLL
	bool				m_bWillProduceStatTrak;	// will produce stattrak
#endif
	float				m_flTotalWeight;
	CUtlVector<float>	m_flWeights;

	struct lootlist_attrib_t
	{
		static_attrib_t	m_staticAttrib;
		float	m_flWeight;
		float	m_flRangeMin;
		float	m_flRangeMax;

		CCopyableUtlVector< uint32 > m_vecValues;

		bool BInitFromKV( const char *pszContext, KeyValues *pKVKey, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors );
	};

	struct random_attrib_t
	{
		float				m_flChanceOfRandomAttribute;
		float				m_flTotalAttributeWeight;
		bool				m_bPickAllAttributes;
		CUtlVector<lootlist_attrib_t> m_RandomAttributes;
	};

	CUtlVector<random_attrib_t*>			m_RandomAttribs;
	bool				m_bServerList;
};

//-----------------------------------------------------------------------------
// CEconCraftingRecipeDefinition
// Template Definition of an item recipe
//-----------------------------------------------------------------------------
class CEconCraftingRecipeDefinition
{
public:
	CEconCraftingRecipeDefinition( void );
	virtual ~CEconCraftingRecipeDefinition( void ) { }

	bool		BInitFromKV( KeyValues *pKVItem, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );
	bool		BInitFromSet( const IEconItemSetDefinition *pSet, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );

	virtual void CopyPolymorphic( const CEconCraftingRecipeDefinition *pSourceDef ) { *this = *pSourceDef; }

	void		SetDefinitionIndex( uint32 iIndex ) { m_nDefIndex = iIndex; }
	int32		GetDefinitionIndex( void ) const	{ return m_nDefIndex; }
	const char	*GetName( void ) const				{ return !m_strName.IsEmpty() ? m_strName.String() : "unknown"; }
	const char	*GetName_A( void ) const				{ return !m_strN_A.IsEmpty() ? m_strN_A.String() : "unknown"; }
	const char	*GetDescInputs( void ) const				{ return !m_strDescInputs.IsEmpty() ? m_strDescInputs.String() : "unknown"; }
	const char	*GetDescOutputs( void ) const				{ return !m_strDescOutputs.IsEmpty() ? m_strDescOutputs.String() : "unknown"; }

	const char	*GetDescI_A( void ) const				{ return !m_strDI_A.IsEmpty() ? m_strDI_A.String() : "unknown"; }
	const char	*GetDescI_B( void ) const				{ return !m_strDI_B.IsEmpty() ? m_strDI_B.String() : "unknown"; }
	const char	*GetDescI_C( void ) const				{ return !m_strDI_C.IsEmpty() ? m_strDI_C.String() : "unknown"; }
	const char	*GetDescO_A( void ) const				{ return !m_strDO_A.IsEmpty() ? m_strDO_A.String() : "unknown"; }
	const char	*GetDescO_B( void ) const				{ return !m_strDO_B.IsEmpty() ? m_strDO_B.String() : "unknown"; }
	const char	*GetDescO_C( void ) const				{ return !m_strDO_C.IsEmpty() ? m_strDO_C.String() : "unknown"; }

	const wchar_t*	GetLocName( void ) const { return m_wszName; }
	const wchar_t*	GetLocDescription( void ) const { return m_wszDesc; }

	void		SetDisabled( bool bDisabled ) { m_bDisabled = bDisabled; }
	bool		IsDisabled( void ) const { return m_bDisabled; }
	bool		RequiresAllSameClass( void ) { return m_bRequiresAllSameClass; }
	bool		RequiresAllSameSlot( void ) { return m_bRequiresAllSameSlot; }
	bool		IsAlwaysKnown( void ) const { return m_bAlwaysKnown; }
	bool		IsPremiumAccountOnly( void ) const { return m_bPremiumAccountOnly; }
	recipecategories_t	GetCategory( void ) const { return m_iCategory; }
	int			GetFilter( void ) const { return m_iFilter; }
	void		SetFilter( int nFilter ) { m_iFilter = nFilter; }
	int			GetTotalInputItemsRequired( void ) const;
	int			GetTotalOutputItems( void ) const { return m_OutputItemsCriteria.Count(); }

	// Returns true if the vector contains a set of items that matches the inputs for this recipe
	virtual bool ItemListMatchesInputs( const CUtlVector< CEconItem* > &vecCraftingItems, bool bAllowPartialMatch = false ) const;

	const CUtlVector<CItemSelectionCriteria> *GetInputItems( void ) const { return &m_InputItemsCriteria; }
	const CUtlVector<uint8>					 &GetInputItemDupeCounts( void ) const { return m_InputItemDupeCounts; }
	const CUtlVector<CItemSelectionCriteria> &GetOutputItems( void ) const { return m_OutputItemsCriteria; }

	// Serializes the criteria to and from messages
	bool		BSerializeToMsg( CSOItemRecipe & msg ) const;
	bool		BDeserializeFromMsg( const CSOItemRecipe & msg );

private:
	void GenerateLocStrings( void );

protected:
	// The number used to refer to this definition in the DB
	int32		m_nDefIndex;

	// Localization key strings
	CUtlString	m_strName; 
	CUtlString	m_strN_A; 
	CUtlString	m_strDescInputs; 
	CUtlString	m_strDescOutputs; 
	CUtlString	m_strDI_A;
	CUtlString	m_strDI_B;
	CUtlString	m_strDI_C;
	CUtlString	m_strDO_A;
	CUtlString	m_strDO_B;
	CUtlString	m_strDO_C;

	wchar_t		m_wszName[ 64 ];
	wchar_t		m_wszDesc[ 512 ];

	bool		m_bDisabled;
	bool		m_bRequiresAllSameClass;
	bool		m_bRequiresAllSameSlot;
	int			m_iCacheClassUsageForOutputFromItem;
	int			m_iCacheSlotUsageForOutputFromItem;
	int			m_iCacheSetForOutputFromItem;
	bool		m_bAlwaysKnown;
	bool		m_bPremiumAccountOnly;
	recipecategories_t	m_iCategory;
	int			m_iFilter;

	// The list of items that a required to make this recipe
	CUtlVector<CItemSelectionCriteria>	m_InputItemsCriteria;
	CUtlVector<uint8>					m_InputItemDupeCounts;

	// The list of items that are generated by this recipe
	CUtlVector<CItemSelectionCriteria>	m_OutputItemsCriteria;
};

//-----------------------------------------------------------------------------
// Purpose: Attribute definition details
//-----------------------------------------------------------------------------
enum
{
	ATTDESCFORM_VALUE_IS_PERCENTAGE,			// Printed as:	((m_flValue*100)-100.0)
	ATTDESCFORM_VALUE_IS_INVERTED_PERCENTAGE,	// Printed as:	((m_flValue*100)-100.0) if it's > 1.0, or ((1.0-m_flModifier)*100) if it's < 1.0
	ATTDESCFORM_VALUE_IS_ADDITIVE,				// Printed as:	m_flValue
	ATTDESCFORM_VALUE_IS_ADDITIVE_PERCENTAGE,	// Printed as:	(m_flValue*100)
	ATTDESCFORM_VALUE_IS_OR,					// Printed as:  m_flValue, but results are ORd together instead of added
	ATTDESCFORM_VALUE_IS_DATE,					// Printed as a date
	ATTDESCFORM_VALUE_IS_ACCOUNT_ID,			// Printed as steam user name
	ATTDESCFORM_VALUE_IS_PARTICLE_INDEX,		// Printed as a particle description
	ATTDESCFORM_VALUE_IS_ITEM_DEF,				// Printed as item name
	ATTDESCFORM_VALUE_IS_COLOR,					// Printed as R, G, B
	ATTDESCFORM_VALUE_IS_GAME_TIME,				// Printed as:	00:00
	ATTDESCFORM_VALUE_IS_MINS_AS_HOURS,			// Printed as:  N hours when >2 hours or N minutes otherwise
	ATTDESCFORM_VALUE_IS_REPLACE,				// Printed as:  m_flValue
};

// Coloring for attribute lines
enum attrib_effect_types_t
{
	ATTRIB_EFFECT_NEUTRAL = 0,
	ATTRIB_EFFECT_POSITIVE,
	ATTRIB_EFFECT_NEGATIVE,

	NUM_EFFECT_TYPES,
};

enum EAssetClassAttrExportRule_t
{
	k_EAssetClassAttrExportRule_Default		= 0,
	k_EAssetClassAttrExportRule_Bucketed	= ( 1 << 0 ),	// attribute exports bucketed value to Steam Community
	k_EAssetClassAttrExportRule_Skip		= ( 1 << 1 ),	// attribute value is not exported to Steam Community
	k_EAssetClassAttrExportRule_GCOnly		= ( 1 << 2 ),	// attribute only lives on GC and not exported to any external request
};

//-----------------------------------------------------------------------------
// CEconItemAttributeDefinition
// Template definition of a randomly created attribute
//-----------------------------------------------------------------------------
class CEconItemAttributeDefinition : public IEconItemAttributeDefinition
{
public:
	CEconItemAttributeDefinition( void );
	CEconItemAttributeDefinition( const CEconItemAttributeDefinition &that );
	CEconItemAttributeDefinition &operator=( const CEconItemAttributeDefinition& rhs );

	~CEconItemAttributeDefinition( void );

	bool	BInitFromKV( KeyValues *pKVAttribute, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );

	virtual attrib_definition_index_t GetDefinitionIndex( void ) const	{ return m_nDefIndex; }
	// Attribute name referenced in the db.
	virtual const char	*GetDefinitionName( void ) const	{ return m_pszDefinitionName; }
	
	virtual KeyValues	*GetRawDefinition( void ) const		{ return m_pKVAttribute; }

	// Data accessing
	bool		IsHidden( void ) const				{ return m_bHidden; }
	bool		BForceWebSchemaOutput( void ) const	{ return m_bWebSchemaOutputForced; }
	bool		IsStoredAsInteger( void ) const		{ return m_bStoredAsInteger; }
	bool		IsStoredAsFloat( void ) const		{ return !m_bStoredAsInteger; }
	bool		IsInstanceData() const				{ return m_bInstanceData; }
	EAssetClassAttrExportRule_t GetAssetClassAttrExportRule() const			{ return m_eAssetClassAttrExportRule; }
	uint32		GetAssetClassBucket() const			{ return m_unAssetClassBucket; }
	int			GetDescriptionFormat( void ) const	{ return m_iDescriptionFormat; }
	int			GetScore( void ) const				{ return m_iScore; }
	virtual const char *GetDescriptionString( void ) const	{ return m_pszDescriptionString; }
	const char *GetArmoryDescString( void ) const	{ return m_pszArmoryDesc; }
	virtual const char *GetAttributeClass( void ) const		{ return m_pszAttributeClass; }
	attrib_effect_types_t GetEffectType( void ) const	{ return m_iEffectType; }

	const class ISchemaAttributeType *GetAttributeType( void ) const { return m_pAttrType; }

#ifndef GC_DLL
	void		ClearStringCache( void ) const		{ m_iszAttributeClass = NULL_STRING; }
	string_t	GetCachedClass( void ) const
	{
		if ( m_iszAttributeClass == NULL_STRING && m_pszAttributeClass )
		{
			m_iszAttributeClass = AllocPooledString( m_pszAttributeClass );
		}
		return m_iszAttributeClass;
	}
#endif

private:
	// The raw keyvalues for this attribute definition.
	KeyValues	*m_pKVAttribute;

	// Required valued from m_pKVAttribute:

	// The number used to refer to this definition in the DB
	attrib_definition_index_t	m_nDefIndex;

	// ...
	const class ISchemaAttributeType *m_pAttrType;

	// ---------------------------------------------
	// Display related data
	// ---------------------------------------------
	// If true, this attribute isn't shown in the item description
	bool		m_bHidden;

	// If true, this attribute's description is always output in web api calls regardless of the hidden flag.
	bool		m_bWebSchemaOutputForced;

	// Whether or not the value is stored as an integer in the DB.
	bool		m_bStoredAsInteger;

	// If this is true the attribute is counted as "instance" data for purposes of asset class in the Steam Economy. Non-instance
	// properties are considered things that can differentiate items at a fundamental level (ie., definition index, quality); instance
	// properties are more things like additional customizations -- score for strange items, paint color, etc.
	bool		m_bInstanceData;
	EAssetClassAttrExportRule_t	m_eAssetClassAttrExportRule;			// if this is true the attribute will not be exported for asset class
	uint32		m_unAssetClassBucket;		// if this is set then attribute value is bucketed when exported for asset class

	// Overall positive/negative effect. Used to color the attribute.
	attrib_effect_types_t m_iEffectType;

	// Contains the description format & string for this attribute
	int			m_iDescriptionFormat;
	const char	*m_pszDescriptionString;

	const char	*m_pszDescriptionTag;

	// Contains information on how to describe items with this attribute in the Armory
	const char	*m_pszArmoryDesc;
	int			m_iScore;

	// Used to allow unique items to specify attributes by name.
	const char	*m_pszDefinitionName;

	// The class name of this attribute. Used in creation, and to hook the attribute into the actual code that uses it.
	const char	*m_pszAttributeClass;

#if defined(CLIENT_DLL) || defined(GAME_DLL) || defined(GC)
	mutable string_t	m_iszAttributeClass;	// Same as the above, but used for fast lookup when applying attributes.
#endif

};

//-----------------------------------------------------------------------------
// CEconSoundMaterialDefinition
//-----------------------------------------------------------------------------
class CEconSoundMaterialDefinition
{
public:
	CEconSoundMaterialDefinition( void );
	CEconSoundMaterialDefinition( const CEconSoundMaterialDefinition &that );
	CEconSoundMaterialDefinition &operator=( const CEconSoundMaterialDefinition& rhs );

	~CEconSoundMaterialDefinition( void ) { }

	bool		BInitFromKV( KeyValues *pKVItem, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );

	int32		GetID( void ) const					{ return m_nID; }
	const char	*GetName( void ) const				{ return !m_strName.IsEmpty() ? m_strName.String() : "unknown"; }
	
	const char *GetStartDragSound( void ) const		{ return m_strStartDragSound.Get(); }
	const char *GetEndDragSound( void ) const		{ return m_strEndDragSound.Get(); }
	const char *GetEquipSound( void ) const			{ return m_strEquipSound.Get(); }

private:

	// The value that the game/DB will know this sound material by
	int32		m_nID;

	// The English name of the sound material
	CUtlString	m_strName; 

	// sounds played when dragging items of this material around the backpack
	CUtlString  m_strStartDragSound;
	CUtlString  m_strEndDragSound;

	// sound played when equipping an item of this material
	CUtlString  m_strEquipSound;
};


//-----------------------------------------------------------------------------
// Visual data storage in item definitions
//-----------------------------------------------------------------------------
#define MAX_VISUALS_CUSTOM_SOUNDS		10


#if defined(CLIENT_DLL) || defined(GAME_DLL)
enum
{
	kAttachedModelDisplayFlag_WorldModel = 0x01,
	kAttachedModelDisplayFlag_ViewModel	 = 0x02,

	kAttachedModelDisplayFlag_MaskAll	 = kAttachedModelDisplayFlag_WorldModel | kAttachedModelDisplayFlag_ViewModel,
};

struct attachedmodel_t
{
	const char *m_pszModelName;
	int m_iModelDisplayFlags;
};

struct attachedparticle_t
{
	int m_iParticleIndex;
	int m_nStyle;
};

enum wearableanimplayback_t
{
	WAP_ON_SPAWN,				// Play this animation immediately on spawning the wearable
	WAP_START_BUILDING,			// Game code will start this anim whenever a player wearing this item deploys their builder weapon.
	WAP_STOP_BUILDING,			// Game code will start this anim whenever a player wearing this item holsters their builder weapon.

	NUM_WAP_TYPES,
};

struct animation_on_wearable_t
{
	int						iActivity;
	const char				*pszActivity;
	wearableanimplayback_t	iPlayback;
	const char				*pszReplacement;
	int						iReplacement; // Replacement activity to play. Might be set to one of kActivityLookup_Unknown/kActivityLookup_Missing.
	const char				*pszSequence;
	const char				*pszRequiredItem;
	const char				*pszScene;
	float					flFrequency;
};

struct sound_on_wearable_t
{
	const char*				pszSound;
	const char*				pszReplacement;
};

struct particle_on_wearable_t
{
	const char*				pszParticle;
	const char*				pszReplacement;
	bool					bFlyingCourierEffect;
};

struct particlesnapshot_on_wearable_t
{
	const char*				pszParticleSnapshot;
	const char*				pszReplacement;
};

struct particle_control_point_on_wearable_t
{
	const char*				pszParticle;
	int						nParticleControlPoint;
	Vector					vecCPValue;
};

struct codecontrolledbodygroupdata_t
{
	const char *pFuncName;
	void *pFunc;
};

#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

struct style_unlock_info
{
	const char*	pszItemName;
	int iPrice;
	int iStylePreReq;
	const char* pszAttrib;
	uint32 iAttribValue;

	bool IsLockable() const
	{
		return pszItemName || pszAttrib;
	}

	bool HasUnlockPrice() const
	{
		return iPrice && pszItemName;
	}

	bool HasUnlockAttrib() const
	{
		return iAttribValue && pszAttrib;
	}
};

class CEconStyleInfo
{
public:
	CEconStyleInfo()
	{
		m_iIndex = 0;
		m_iSkin = 0;
		m_iIcon = 0;
		m_pszName = NULL;
		m_pszBasePlayerModel = NULL;
		m_UnlockInfo.pszItemName = NULL;
		m_UnlockInfo.iPrice = 0;
		m_UnlockInfo.iStylePreReq = 0;
		m_UnlockInfo.pszAttrib = NULL;
		m_UnlockInfo.iAttribValue = 0;
	}

	virtual ~CEconStyleInfo()
	{
		//
	}

	virtual void BInitFromKV( KeyValues *pKVItem, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors );

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	virtual void GeneratePrecacheModelStringsForStyle( CUtlVector<const char *> *out_pVecModelStrings ) const;
#endif

	int GetSkin() const
	{
		return m_iSkin;
	}

	int GetIndex() const { return m_iIndex; }
	const char *GetName() const { return m_pszName; }
	const char *GetBasePlayerDisplayModel() const { return m_pszBasePlayerModel; }
	const CUtlVector<const char *>& GetAdditionalHideBodygroups() const { return m_vecAdditionalHideBodygroups; }
	int GetIcon() const { return m_iIcon; }
	const style_unlock_info* GetUnlockInfo() const { return &m_UnlockInfo; }

protected:
	int m_iIndex;
	int m_iSkin;
	int m_iIcon;
	const char *m_pszName;
	const char *m_pszBasePlayerModel;
	CUtlVector<const char *> m_vecAdditionalHideBodygroups;
	style_unlock_info m_UnlockInfo;
};

struct courier_t
{
	const char *m_pszModelName;
	int m_iTeam;
};

struct announcer_preview_t
{
	CUtlString m_strFileName;
	CUtlString m_strCaption;
};

struct ability_icon_replacement_t
{
	CUtlString m_strAbilityName;
	CUtlString m_strReplacement;
};

enum EAssetModifier
{
	AM_Invalid=-1,
	AM_Activity,
	AM_Announcer,
	AM_AnnouncerPreview,
	AM_HudSkin,
	AM_AbilityName,
	AM_Sound,
	AM_Speech,
	AM_Particle,
	AM_ParticleSnapshot,
	AM_ParticleControlPoint,
	AM_EntityModel,
	AM_ViewModel,
	AM_EntityScale,
	AM_Icon,
	AM_AbilityIcon,
	AM_Courier,
	AM_CourierFlying,
	AM_HeroModelChange,
	AM_MAX
};

struct AssetModifier
{
	EAssetModifier	m_Type;
	CUtlString		m_strAsset;
	CUtlString		m_strModifier;
	float			m_flModifier;
	float			m_flFrequency;
	int				m_iStyle;
};

struct AssetInfo
{
	AssetInfo()
	{
#if defined(CLIENT_DLL) || defined(GAME_DLL)
		iHideParentBodyGroup = -1;

		m_ModifiedBodyGroupNames.SetLessFunc( StringLessThan );
		m_CodeControlledBodyGroupNames.SetLessFunc( StringLessThan );

		iSkin = 0;
		bUsePerClassBodygroups = false;
		m_pszMaterialOverride = NULL;
		m_pszParticleEffect = NULL;
		m_pszParticleSnapshot = NULL;
		for ( int i = 0; i < MAX_VISUALS_CUSTOM_SOUNDS; i++ )
		{
			m_pszCustomSounds[i] = NULL;
		}

		for ( int i = 0; i < NUM_SHOOT_SOUND_TYPES; i++ )
		{
			m_pszWeaponSoundReplacements[i] = NULL;
		}

		m_iViewModelBodyGroupOverride = -1;
		m_iViewModelBodyGroupStateOverride = -1;
		m_iWorldModelBodyGroupOverride = -1;
		m_iWorldModelBodyGroupStateOverride = -1;

		m_pszSpeechConcept = NULL;
		m_pszChatMessage = NULL;
		m_pszAnnouncerName.Clear();
		m_pszAnnouncerResource.Clear();
		m_pszEntityModel = NULL;
		m_pszViewModel = NULL;
		m_pszEntityClass = NULL;
//		m_pszHeroModelFrom = NULL;
//		m_pszHeroModelTo = NULL;
		m_flScaleSize = 1.f;
		m_pszScaleClass = NULL;
		m_bSkipModelCombine = false;

		m_pszPrimaryAmmo = NULL;
		m_pszWeaponTypeString = NULL;
		m_pszAddonLocation = NULL;
		m_pszEjectBrassEffect = NULL;
		m_pszTracerEffect = NULL;
		m_pszMuzzleFlashEffect1stPerson = NULL;
		m_pszMuzzleFlashEffect1stPersonAlt = NULL;
		m_pszMuzzleFlashEffect3rdPerson = NULL;
		m_pszMuzzleFlashEffect3rdPersonAlt = NULL;
		m_pszHeatEffect = NULL;
		m_pszPlayerAnimationExtension = NULL;

		m_pszOriginalIcon = NULL;
		m_pszNewIcon = NULL;

		m_mapAssetModifiers.SetLessFunc( DefLessFunc(int) );


#endif // defined(CLIENT_DLL) || defined(GAME_DLL)
	}

	~AssetInfo()
	{
		m_Styles.PurgeAndDeleteElements();
	}

	void AddAssetModifier( AssetModifier* newMod );
	CUtlVector<AssetModifier*>* GetAssetModifiers( EAssetModifier type );
	const char* GetModifierByAsset( EAssetModifier type, const char* pszAsset, int iStyle=-1 );
	const char* GetAssetByModifier( EAssetModifier type, const char* pszModifier, int iStyle=-1 );

	CUtlMap< int, CUtlVector<AssetModifier*>* >	m_mapAssetModifiers;

#if defined(CLIENT_DLL) || defined(GAME_DLL)	
	// Properties necessary for the game client/server but not for the GC.

	int iHideParentBodyGroup;
	CUtlMap<const char*, int> m_ModifiedBodyGroupNames; // Better method: hide multiple body groups by name.
	CUtlMap<const char*, codecontrolledbodygroupdata_t> m_CodeControlledBodyGroupNames;
	int			iSkin;
	bool		bUsePerClassBodygroups;
	CUtlVector<attachedmodel_t>	m_AttachedModels;
	CUtlVector<attachedparticle_t>	m_AttachedParticles;
	CUtlVector<animation_on_wearable_t> m_Animations;
	CUtlVector<sound_on_wearable_t> m_Sounds;
	CUtlVector<particle_on_wearable_t> m_Particles;
	CUtlVector<particlesnapshot_on_wearable_t> m_ParticleSnapshots;
	CUtlVector<particle_control_point_on_wearable_t> m_ParticleControlPoints;
	const char *m_pszCustomSounds[MAX_VISUALS_CUSTOM_SOUNDS];
	const char *m_pszMaterialOverride;
	const char *m_pszMuzzleFlash;
	const char *m_pszTracerEffect;
	const char *m_pszParticleEffect;
	const char *m_pszParticleSnapshot;
	const char *m_pszWeaponSoundReplacements[NUM_SHOOT_SOUND_TYPES];

	const char *m_pszPrimaryAmmo;
	const char *m_pszWeaponTypeString;
	const char *m_pszAddonLocation;
	const char *m_pszEjectBrassEffect;
	const char *m_pszMuzzleFlashEffect1stPerson;
	const char *m_pszMuzzleFlashEffect1stPersonAlt;
	const char *m_pszMuzzleFlashEffect3rdPerson;
	const char *m_pszMuzzleFlashEffect3rdPersonAlt;
	const char *m_pszHeatEffect;
	const char *m_pszPlayerAnimationExtension;

	int m_iViewModelBodyGroupOverride;
	int m_iViewModelBodyGroupStateOverride;
	int m_iWorldModelBodyGroupOverride;
	int m_iWorldModelBodyGroupStateOverride;
	bool m_bSkipModelCombine;
	CUtlVector<const char*>								m_vecAnimationModifiers;

	// For custom speech events and chat messages on use.
	const char *m_pszSpeechConcept;
	const char *m_pszChatMessage;

	// For Announcer
	CUtlString m_pszAnnouncerName;
	CUtlString m_pszAnnouncerResource;
	CUtlVector< announcer_preview_t > m_vecAnnouncerPreview;

	CUtlString m_pszHudSkinName;

	// for consumables that create an in-game ability
	CUtlString m_pszAbilityName;	

	// For Overriding Entity Base Models
	const char *m_pszEntityModel;
	const char *m_pszViewModel;
	const char *m_pszEntityClass;

	// TEMP: For interrupting hero model changes
//	const char *m_pszEntityModel;
//	const char *m_pszEntityClass;

	// For changing an entity's size.
	float		m_flScaleSize;
	const char *m_pszScaleClass;

	//Override HUD icons
	const char *m_pszOriginalIcon;
	const char *m_pszNewIcon;

	CUtlVector< ability_icon_replacement_t > m_vecAbilityIconReplacements;


#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

	// The GC does care about styles.
	CUtlVector<CEconStyleInfo *> m_Styles;
};

enum item_capabilities_t
{
	ITEM_CAP_NONE					= 0,
	ITEM_CAP_PAINTABLE				= 1 << 0,		// some items are tagged in CS:GO schema, but we don't use it
	ITEM_CAP_NAMEABLE				= 1 << 1,		// used in CS:GO on all weapons that can get a name tag
	ITEM_CAP_DECODABLE				= 1 << 2,		// used in CS:GO on supply crates containers
	ITEM_CAP_CAN_DELETE				= 1 << 3,		// used in CS:GO on supply crates containers
	ITEM_CAP_CAN_CUSTOMIZE_TEXTURE	= 1 << 4,	// NOT USED
	ITEM_CAP_USABLE					= 1 << 5,	// NOT USED
	ITEM_CAP_USABLE_GC				= 1 << 6,		// some items are tagged in CS:GO schema, but we don't use it
	ITEM_CAP_CAN_GIFT_WRAP			= 1 << 7,	// NOT USED
	ITEM_CAP_USABLE_OUT_OF_GAME		= 1 << 8,		// some items are tagged in CS:GO schema, but we don't use it
	ITEM_CAP_CAN_COLLECT			= 1 << 9,	// NOT USED
	ITEM_CAP_CAN_CRAFT_COUNT		= 1 << 10,	// NOT USED
	ITEM_CAP_CAN_CRAFT_MARK			= 1 << 11,	// NOT USED
	ITEM_CAP_PAINTABLE_TEAM_COLORS	= 1 << 12,	// NOT USED
	ITEM_CAP_CAN_BE_RESTORED		= 1 << 13,	// NOT USED
	ITEM_CAP_CAN_USE_STRANGE_PARTS	= 1 << 14,	// NOT USED
	ITEM_CAP_PAINTABLE_UNUSUAL		= 1 << 15,	// NOT USED
	ITEM_CAP_CAN_INCREMENT			= 1 << 16,	// NOT USED
	ITEM_CAP_USES_ESSENCE			= 1 << 17,	// NOT USED
	ITEM_CAP_AUTOGRAPH				= 1 << 18,	// NOT USED
	ITEM_CAP_RECIPE					= 1 << 19,	// NOT USED
	ITEM_CAP_CAN_STICKER			= 1 << 20,		// used in CS:GO on sticker tools, primary and secondary weapons
	ITEM_CAP_STATTRACK_SWAP			= 1 << 21,		// used in CS:GO on stattrack items
	NUM_ITEM_CAPS					= 22,
};

enum { ITEM_CAP_DEFAULT		 = ITEM_CAP_NONE }; // what are the default capabilities on an item?
enum { ITEM_CAP_TOOL_DEFAULT = ITEM_CAP_NONE };	// what are the default capabilities of a tool?

struct bundleinfo_t
{
	CUtlVector <item_list_entry_t > vecItemEntries;
};

#ifdef CLIENT_DLL
namespace vgui
{
	class Panel;
}
#endif // CLIENT_DLL

class IEconConsumable
{

};

class IEconItemInterface;

class IEconTool
{
	friend class CEconSharedToolSupport;

public:
	IEconTool( const char *pszTypeName, const char *pszUseString, const char *pszUsageRestriction, item_capabilities_t unCapabilities, KeyValues* pUsageKV )
		: m_pszTypeName( pszTypeName )
		, m_pszUseString( pszUseString )
		, m_pszUsageRestriction( pszUsageRestriction )
		, m_unCapabilities( unCapabilities )
	{
		if ( pUsageKV )
		{
			KeyValues* pBonusItemDefs = pUsageKV->FindKey( "bonus_itemdefs" );
			if ( pBonusItemDefs )
			{
				FOR_EACH_SUBKEY( pBonusItemDefs, pBonusItemDef )
				{
					m_vecBonusItemDef.AddToTail( atoi( pBonusItemDef->GetName() ) );
				}
			}
		}
	}

	virtual ~IEconTool() { }

	// Shared code.
	const char *GetUsageRestriction() const { return m_pszUsageRestriction; }
	item_capabilities_t GetCapabilities() const { return m_unCapabilities; }

	virtual bool CanApplyTo( const IEconItemInterface *pTool, const IEconItemInterface *pToolSubject ) const { Assert( pTool ); Assert( pToolSubject ); return true; }
	virtual bool ShouldDisplayQuantity( const IEconItemInterface *pTool ) const;
	virtual bool RequiresToolEscrowPeriod() const { return false; }

	// We don't support throwing exceptions from tool construction so this is intended to be checked afterwards
	// whenever a new tool is created. (See CreateEconToolImpl().)
	virtual bool IsValid() const { return true; }
	
	// Used by the GC only for WebAPI responses and for some weird internal code.
	const char *GetTypeName() const { return m_pszTypeName; }		// would like to disable on the client so we aren't tempted to check against it, but used for building a unique tool list
	const char *GetUseString() const { return m_pszUseString; }

	// Bonus Items
	int GetBonusItemDefCount( void ) const { return m_vecBonusItemDef.Count(); }
	int GetBonusItemDef( int i ) const { return m_vecBonusItemDef[i]; }

	virtual IEconConsumable* CreateEconConsumable() const
	{
		Assert( !"IEconTool::CreateEconConsumable(): unimplemented call!" );
		return NULL; 
	}

#ifdef CLIENT_DLL
	virtual bool ShouldShowContainedItemPanel( const IEconItemInterface *pItem ) const { Assert( !"IEconTool::ShouldShowContainedItemPanel(): we don't expect this to be called on anything besides gifts!" ); return false; }
	virtual const char *GetUseCommandLocalizationToken( const IEconItemInterface *pItem, const char* pszDefault="#ApplyOnItem" ) const;

	// Client "do something" interface. At least one of these functions must be implemented or your tool
	// won't do anything on the client. Some tools (ie., collections) will implement both because they
	// have one application behavior and one client-UI behavior.

	// When the client attempts to use a consumable item of any kind, this function will be called. This
	// is called from the UI in response to things like using dueling pistols, using a noisemaker, etc.
	// Usually this opens up some UI, sends off a GC message, etc.
	//
	// There is a "default" implementation of this function in ClientConsumableTool_Generic() that can
	// be called if specific behavior isn't needed.
	virtual void OnClientUseConsumable( class C_EconItemView *pItem ) const
	{
		Assert( !"IEconTool::OnClientUseConsumable(): unimplemented call!" );
	}

	// When the client attempts to apply a tool to a specific other item in their inventory, this function
	// will be called. This is called from the UI is response to things like putting paint on an item,
	// using a key to unlock a crate, etc.
	virtual void OnClientApplyTool( class C_EconItemView *pTool, class C_EconItemView *pSubject ) const
	{
		Assert( !"IEconTool::OnClientApplyTool(): unimplemented call!" );
	}
	virtual void OnClientApplyCommit( class C_EconItemView *pTool, class C_EconItemView *pSubject ) const
	{
		Assert( !"IEconTool::OnClientApplyCommit(): unimplemented call!" );
	}
#endif // CLIENT_DLL

private:
	const char *m_pszTypeName;
	const char *m_pszUseString;
	const char *m_pszUsageRestriction;
	item_capabilities_t m_unCapabilities;
	CUtlVector<int>	m_vecBonusItemDef;
};

enum EItemType
{
	k_EItemTypeNone,
	k_EItemTypeCoupon,
	k_EItemTypeCampaign,
	k_EItemTypeSelfOpeningPurchase,
	k_EItemTypeOperationCoin,
	k_EItemTypePrestigeCoin,
	k_EItemTypeTool,
};
const char* PchNameFromEItemType( EItemType eType );

enum EItemSlot
{
	k_EItemSlotNone,
	k_EItemSlotMelee,
	k_EItemSlotSecondary,
	k_EItemSlotSMG,
	k_EItemSlotRifle,
	k_EItemSlotHeavy,
	k_EItemSlotFlair,
	k_EItemSlotMusicKit,
};
const char* PchNameFromEItemSlot( EItemSlot eSlot );
EItemSlot EItemSlotFromName( const char * pchName );

//-----------------------------------------------------------------------------
// CEconItemDefinition
// Template Definition of a randomly created item
//-----------------------------------------------------------------------------
class CEconItemDefinition : public IEconItemDefinition
{
public:
	CEconItemDefinition( void );
	virtual ~CEconItemDefinition( void );

	void PurgeStaticAttributes( void );

public:
	// BInitFromKV can be implemented on subclasses to parse additional values.
	virtual bool	BInitFromKV( KeyValues *pKVItem, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );
#if defined(CLIENT_DLL) || defined(GAME_DLL)
	virtual bool	BInitFromTestItemKVs( int iNewDefIndex, KeyValues *pKVItem, CEconItemSchema &pschema );
	virtual void	GeneratePrecacheModelStrings( bool bDynamicLoad, CUtlVector<const char *> *out_pVecModelStrings ) const;
	virtual void	GeneratePrecacheSoundStrings( CUtlVector<const char*> *out_pVecSoundStrings ) const;
	virtual void	GeneratePrecacheEffectStrings( CUtlVector<const char*> *out_pVecEffectStrings ) const;
#endif
	virtual void	CopyPolymorphic( const CEconItemDefinition *pSourceDef ) { *this = *pSourceDef; }

	bool		BInitItemMappings( CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors );

	virtual void BInitVisualBlockFromKV( KeyValues *pKVItem, IEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );
	void		BInitStylesBlockFromKV( KeyValues *pKVStyles, CEconItemSchema &pschema, AssetInfo *pAssetInfo, CUtlVector<CUtlString> *pVecErrors );
	bool		BInitAlternateIconsFromKV( KeyValues *pKVAlternateIcons, CEconItemSchema &pschema, AssetInfo *pAssetInfo, CUtlVector<CUtlString> *pVecErrors );

	virtual item_definition_index_t	GetDefinitionIndex( void ) const	{ return m_nDefIndex; }
	const CUtlVector<item_definition_index_t>&	GetAssociatedItemsDefIndexes() const { return m_nAssociatedItemsDefIndexes; }
	virtual const char* GetPrefabName( void ) const	{ return m_szPrefab; }
	bool		BEnabled( void ) const				{ return m_bEnabled; }
	bool		BLoadOnDemand( void ) const			{ return m_bLoadOnDemand; }
	bool		BHasBeenLoaded( void ) const		{ return m_bHasBeenLoaded; }
	const char	*GetDefinitionName( void ) const	{ return m_pszDefinitionName; }
	const char	*GetItemClass( void ) const			{ return m_pszItemClassname; }
	virtual const char	*GetItemBaseName( void ) const		{ return m_pszItemBaseName; }
	const char	*GetBrassModelOverride( void ) const{ return m_pszBrassModelOverride; }
	virtual const char	*GetItemTypeName( void ) const		{ return m_pszItemTypeName; }
	virtual uint32 GetItemTypeID( void ) const		{ return m_unItemTypeID; }
	uint8		GetMinLevel( void ) const			{ return m_unMinItemLevel; }
	uint8		GetMaxLevel( void ) const			{ return m_unMaxItemLevel; }
	virtual uint8	GetRarity( void ) const				{ return m_nItemRarity; }
	uint8		GetQuality( void ) const			{ return m_nItemQuality; }
	uint8		GetForcedQuality( void ) const		{ return m_nForcedItemQuality; }
	uint8		GetDefaultDropQuality( void ) const		{ return m_nDefaultDropItemQuality; }
	uint8		GetDefaultDropQuantity( void ) const{ return m_nDefaultDropQuantity; }
	KeyValues	*GetRawDefinition( void ) const		{ return m_pKVItem; }
	const CUtlVector<static_attrib_t> &GetStaticAttributes( void ) const	{ return m_vecStaticAttributes; }
	uint32		GetNumConcreteItems() const			{ return m_unNumConcreteItems; }
	int			GetSoundMaterialID() const			{ return m_nSoundMaterialID; }

	// Data accessing
	bool		IsHidden( void ) const				{ return m_bHidden; }
	bool		IsImported( void ) const			{ return m_bImported; }
	bool		IsOnePerAccountCDKEY( void ) const	{ return m_bOnePerAccountCDKEY; }
	bool		IsAllowedInMatch( void ) const		{ return m_bAllowedInThisMatch; }
	virtual bool IsBaseItem( void ) const			{ return m_bBaseItem; }
	virtual bool IsDefaultSlotItem( void ) const	{ return m_bDefaultSlotItem; }
	virtual bool IsBundle( void ) const				{ return m_BundleInfo != NULL; }
	bool		HasProperName( void ) const			{ return m_bProperName; }
	const char	*GetClassToken( void ) const		{ return m_pszClassToken; }
	const char	*GetSlotToken( void ) const			{ return m_pszSlotToken; }
	bool		ShouldAttachToHands( void ) const	{ return m_bAttachToHands; }
	bool		ShouldAttachToHandsVMOnly( void ) const	{ return m_bAttachToHandsVMOnly; }
	bool		ShouldFlipViewmodels( void ) const	{ return m_bFlipViewModel; }
	int			GetInventoryImagePosition( int iIndex ) const	{ Assert( iIndex >= 0 && iIndex < 2); return m_iInventoryImagePosition[iIndex]; }
	int			GetInventoryImageSize( int iIndex ) const	{ Assert( iIndex >= 0 && iIndex < 2); return m_iInventoryImageSize[iIndex]; }
	int			GetDropType( void ) const			{ return m_iDropType; }
	const char	*GetHolidayRestriction( void ) const	{ return m_pszHolidayRestriction; }
	int			GetSubType( void ) const	{ return m_iSubType; }
	item_capabilities_t GetCapabilities( void ) const { return m_iCapabilities; }
	void		AddItemSet( int nIndex );
	const CUtlVector< int >& GetItemSets( void ) const;
	int			GetArmoryRemap( void ) const		{ return m_iArmoryRemap; }
	int			GetStoreRemap( void ) const			{ return m_iStoreRemap; }
	virtual int	GetLoadoutSlot( void ) const { return 0; }
	virtual int	GetHeroID( void ) const { return 0; }
	KeyValues*	GetPortraitsKV( void ) const			{ return m_pPortraitsKV; }
	KeyValues*  GetPortraitKVForModel( const char* pszModelName ) const;

	AssetInfo	*GetAssetInfo() const				{ return m_pAssetInfo; }

	bool IsTool() const									{ return m_pTool != NULL; }
	bool IsToolAndNotACrate() const							{ return ( IsTool() && GetEconTool() && V_strcmp( GetEconTool()->GetTypeName(), "supply_crate" ) != 0 ); }
	const IEconTool	*GetEconTool() const				{ return m_pTool; }
	template < class T >
	const T *GetTypedEconTool() const					{ return dynamic_cast<const T *>( GetEconTool() ); }

	virtual bool IsPreviewableInStore() const { return true; }

	const bundleinfo_t *GetBundleInfo( void ) const { return m_BundleInfo; }
	virtual int GetBundleItemCount( void ) const { return m_BundleInfo ? m_BundleInfo->vecItemEntries.Count() : 0; }
	virtual int GetBundleItem( int iIndex ) const { return m_BundleInfo ? m_BundleInfo->vecItemEntries[iIndex].m_nItemDef : -1; }
	virtual int GetBundleItemPaintKitID( int iIndex ) const { return m_BundleInfo ? m_BundleInfo->vecItemEntries[iIndex].m_nPaintKit : -1; }

	// Is this item contained in any bundles? GetContainingBundles() gets the CEconItemDefinitions for those bundles.
	const CUtlVector< const CEconItemDefinition * > &GetContainingBundles() const { return m_vecContainingBundleItemDefs; }
	uint32 GetContainingBundleCount() const { return m_vecContainingBundleItemDefs.Count(); }

	typedef CUtlVector< uint32 > WorkshopContributorList_t;

	void AddSteamWorkshopContributor( uint32 unAccountID ) { if ( m_vecSteamWorkshopContributors.InvalidIndex() == m_vecSteamWorkshopContributors.Find( unAccountID ) ) { m_vecSteamWorkshopContributors.AddToTail( unAccountID ); } }
	const WorkshopContributorList_t &GetSteamWorkshopContributors() const { return m_vecSteamWorkshopContributors; }
	bool BIsSteamWorkshopItem() const { return m_vecSteamWorkshopContributors.Count() > 0; }

	const char	*GetIconClassname( void ) const		{ return m_pszItemIconClassname; }
	const char	*GetLogClassname( void ) const		{ return m_pszItemLogClassname; }
	const char	*GetInventoryModel( void ) const	{ return m_pszInventoryModel; }
	virtual const char	*GetInventoryImage( void ) const	{ return m_pszInventoryImage; }
	const char	*GetInventoryOverlayImage( int idx ) const	{ if ( m_pszInventoryOverlayImages.IsValidIndex( idx ) ) return m_pszInventoryOverlayImages[idx]; else return NULL; }
	int			GetInventoryOverlayImageCount( void ) const { return m_pszInventoryOverlayImages.Count(); }
	const char  *GetIconURLSmall() const			{ return m_sIconURLSmall; }
	const char  *GetIconURLLarge() const			{ return m_sIconURLLarge; }
	void	SetIconURLSmall( const char *szURL )	{ m_sIconURLSmall = szURL; }
	void	SetIconURLLarge( const char *szURL )	{ m_sIconURLLarge = szURL; }
	virtual const char	*GetBasePlayerDisplayModel() const		{ return m_pszBaseDisplayModel; }
	virtual const char  *GetWorldDisplayModel()	const			{ return m_pszWorldDisplayModel; }
	virtual const char  *GetWorldDroppedModel()	const			{ return m_pszWorldDroppedModel; }

	// Some weapons need a custom model for icon generation. If this value is not present, the world model is used.
	virtual const char  *GetIconDisplayModel()	const;
	// Use a custom model for the buy menu, typically to pose the model in a particular way that's different from the world, view or icon models
	virtual const char  *GetBuyMenuDisplayModel()	const;
	// Some weapons need a custom model for pedestal display. If this value is not present, the base model is used.
	virtual const char  *GetPedestalDisplayModel()	const;
	// weapons drop physical mags in the world. Most should have custom mags, but specifying the model means some can be generic
	virtual const char  *GetMagazineModel()	const;
	// mask shape for scope blur effect
	virtual const char  *GetScopeLensMaskModel()	const;
	// Weapons may feature nametags in the form of UID nameplates. This may be aesthetically different per weapon
	virtual const char	*GetUidModel() const;
	// StatTrak weapons show their stats by merging a 'stattrak module' onto their viewmodel. This may be aesthetically different per weapon
	virtual const char	*GetStatTrakModelByType( uint32 nType ) const;

	virtual const int	 GetNumSupportedStickerSlots() const;
	virtual const char	*GetStickerSlotModelBySlotIndex( uint32 nIndex ) const;
	virtual const Vector &GetStickerSlotWorldProjectionStartBySlotIndex( uint32 nIndex ) const;
	virtual const Vector &GetStickerSlotWorldProjectionEndBySlotIndex( uint32 nIndex ) const;
	virtual const char	*GetStickerWorldModelBoneParentNameBySlotIndex( uint32 nIndex ) const;
	virtual const char	*GetStickerSlotMaterialBySlotIndex( uint32 nIndex ) const;
	virtual const char  *GetIconDefaultImage() const			{ return m_pszIconDefaultImage; }
	virtual const char	*GetExtraWearableModel( void ) const	{ return m_pszWorldExtraWearableModel; }
	virtual const char	*GetParticleFile( void ) const	{ return m_pszParticleFile; }
	virtual const char	*GetParticleSnapshotFile( void ) const	{ return m_pszParticleSnapshotFile; }
	const CUtlVector< WeaponPaintableMaterial_t >* GetPaintData( void ) const { return &m_PaintData; }
	const InventoryImageData_t* GetInventoryImageData( void ) const { return m_pInventoryImageData; }
	virtual const char	*GetItemDesc( void ) const			{ return m_pszItemDesc; }
	const char	*GetArmoryDescString( void ) const	{ return m_pszArmoryDesc; }
	RTime32		GetExpirationDate( void ) const		{ return m_rtExpiration; }
	RTime32		GetDefCreationDate( void ) const	{ return m_rtDefCreation; }
	bool		ShouldShowInArmory( void ) const	{ return m_bShouldShowInArmory; }
	bool		IsActingAsAWearable( void ) const	{ return m_bActAsWearable; }
	bool		GetHideBodyGroupsDeployedOnly( void ) const { return m_bHideBodyGroupsDeployedOnly; }
	virtual bool	IsPackBundle( void ) const			{ return m_bIsPackBundle; }
	virtual bool	IsPackItem( void ) const			{ return NULL != m_pOwningPackBundle; }
	CEconItemDefinition	*GetOwningPackBundle()		{ return m_pOwningPackBundle; }
	const CEconItemDefinition	*GetOwningPackBundle() const	{ return m_pOwningPackBundle; }
#if ECONITEM_DATABASE_AUDIT_TABLES_FEATURE
	const char	*GetDatabaseAuditTableName( void ) const	{ return m_pszDatabaseAuditTable; }
#endif
	const char* GetAlternateIcon( int iAlternateIcon ) const;

	equip_region_mask_t GetEquipRegionMask( void ) const { return m_unEquipRegionMask; }
	equip_region_mask_t GetEquipRegionConflictMask( void ) const { return m_unEquipRegionConflictMask; }

	// Dynamic modification during gameplay
	void		SetAllowedInMatch( bool bAllowed )	{ m_bAllowedInThisMatch = bAllowed; }
	void		SetHasBeenLoaded( bool bLoaded )	{ m_bHasBeenLoaded = bLoaded; }

	// Functions to deal with the case where an item definition is actually a proxy for 
	// a random selection
	bool		BRandomProxy( void ) const			{ return NULL != m_pProxyCriteria; }
	CItemSelectionCriteria *PProxyCriteria( void ) const { return m_pProxyCriteria; }

	// Generate and return a random level according to whatever leveling curve this definition uses.
	uint32		RollItemLevel( void ) const;

	const char *GetFirstSaleDate( void ) const;

	virtual bool	IsRecent( void ) const			{ return false;}

	void		IterateAttributes( class IEconItemAttributeIterator *pIterator ) const;

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	// Visuals
	// Attached models
	int						GetNumAttachedModels() const;
	attachedmodel_t			*GetAttachedModelData( int iIdx ) const;
	// Attached particle systems
	int						GetNumAttachedParticles() const;
	attachedparticlesystem_t *GetAttachedParticleData( int iIdx ) const;
	bool					IsAttachedParticleDataValidForStyle( int iIdx, int nStyle ) const;
	// Animations
	int						GetNumAnimations() const;
	animation_on_wearable_t	*GetAnimationData( int iIdx ) const;
	// Animation Modifiers
	int						GetNumAnimationModifiers() const;
	const char*				GetAnimationModifier( int iIdx ) const;

	// Animation Overrides
	Activity				GetActivityOverride( Activity baseAct ) const;
	const char				*GetReplacementForActivityOverride( Activity baseAct ) const;
	// Sounds
	int						GetNumSounds() const;
	sound_on_wearable_t		*GetSoundData( int iIdx ) const;
	// Sound Overrides
	const char				*GetReplacementSound( const char* pszSoundName ) const;
	// Particles
	int						GetNumParticles() const;
	particle_on_wearable_t	*GetParticleData( int iIdx ) const;
	// Particle Overrides
	const char				*GetReplacementParticleEffect( const char* pszParticleName ) const;
	// Particle Snapshots
	int						GetNumParticleSnapshots() const;
	particlesnapshot_on_wearable_t	*GetParticleSnapshotData( int iIdx ) const;
	// Particle Snapshot Overrides
	const char				*GetReplacementParticleSnapshot( const char* pszParticleSnapshotName ) const;

	// Particle ControlPoints
	int						GetNumParticleControlPoints() const;
	particle_control_point_on_wearable_t	*GetParticleControlPointData( int iIdx ) const;

	// Particle Control Point Overrides
	bool					GetReplacementControlPoint( int nIndex, const char* pszParticleName, int &nOutputCP, Vector &nCPValue ) const;

	virtual bool			IsContentStreamable() const;

	// Announcer Overrides
	const char				*GetAnnouncerName() const;
	const char				*GetAnnouncerResource() const;

	// Entity Model Overrides
	const char				*GetEntityOverrideModel() const;
	const char				*GetViewOverrideModel() const;
	const char				*GetEntityOverrideClass() const;

	// Hero Change Model Overrides
	const char				*GetHeroModelChangeModel() const;

	// UI Icons Overrides
	const char				*GetUIOverrideIcon() const;
	const char				*GetUIOverrideOriginalIcon() const;

	const char				*GetUIOverrideAbilityIcon( const char *pszAbilityName ) const;

	// Entity Scale Overrides
	const char				*GetScaleOverrideClass() const;
	float					GetScaleOverrideSize() const;

	// Model Combiner
	bool					SkipModelCombine( void ) const;

	// FX Overrides
	const char				*GetMuzzleFlash() const;
	const char				*GetTracerEffect() const;
	const char				*GetParticleEffect() const;
	const char				*GetParticleSnapshot() const;
	// Materials
	const char				*GetMaterialOverride() const;
	// Sounds
	const char				*GetCustomSound( int iSound ) const;
	const char				*GetWeaponReplacementSound( /*WeaponSound_t*/ int iSound ) const;

	const char				*GetPrimaryAmmo( void ) const;
	const char				*GetWeaponTypeString( void ) const;
	const char				*GetAddonLocation( void ) const;
	const char				*GetEjectBrassEffect( void ) const;
	const char				*GetMuzzleFlashEffect1stPerson( void ) const;
	const char				*GetMuzzleFlashEffect1stPersonAlt( void ) const;
	const char				*GetMuzzleFlashEffect3rdPerson( void ) const;
	const char				*GetMuzzleFlashEffect3rdPersonAlt( void ) const;
	const char				*GetHeatEffect( void ) const;
	const char				*GetPlayerAnimationExtension( void ) const;

	// Bodygroups
	int						GetHiddenParentBodygroup() const;
	int						GetNumModifiedBodyGroups() const;
	const char*				GetModifiedBodyGroup( int i, int& body ) const;
	bool					UsesPerClassBodygroups() const;
	int						GetNumCodeControlledBodyGroups() const;
	const char*				GetCodeControlledBodyGroup( int i, struct codecontrolledbodygroupdata_t &ccbgd ) const;

	int						GetViewmodelBodygroupOverride() const;
	int						GetViewmodelBodygroupStateOverride() const;
	int						GetWorldmodelBodygroupOverride() const;
	int						GetWorldmodelBodygroupStateOverride() const;

#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

	style_index_t			GetNumStyles() const;
	const CEconStyleInfo   *GetStyleInfo( style_index_t unStyle ) const;


	int						GetPopularitySeed() const { return m_nPopularitySeed; }

	bool					HasEconTag( econ_tag_handle_t tag ) const { return m_vecTags.IsValidIndex( m_vecTags.Find( tag ) ); }

	bool					ShoulDisableStyleSelector( void ) const { return m_bDisableStyleSelection; }

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	int						GetStyleSkin( style_index_t unStyle ) const;
#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

	virtual bool			IsPublicItem( void ) const { return m_bPublicItem; }
	virtual bool			IgnoreInCollectionView( void ) const { return m_bIgnoreInCollectionView; }

	const char*				GetLootListName( void ) const { return m_pszLootListName; }


	EItemType GetItemType() const { return m_eItemType; }
	bool GetAllowPurchaseStandalone() const { return m_bAllowPurchaseStandalone; }

private:
	// Pointer to the raw KeyValue definition of the item
	KeyValues *	m_pKVItem;

	// Required values from m_pKVItem:

	// The number used to refer to this definition in the DB
	item_definition_index_t	m_nDefIndex;

	// List of associated items, such as multiple different keys for the same crate.
	CUtlVector< item_definition_index_t > m_nAssociatedItemsDefIndexes;

	// False if this definition has been turned off and we're not using it to generate items
	bool		m_bEnabled;

	// The prefab used by the item.
	const char* m_szPrefab;

	// These values specify the range of item levels that an item based off this definition can be generated within.
	uint8		m_unMinItemLevel;
	uint8		m_unMaxItemLevel;

	// This specifies an item's rarity.
	uint8		m_nItemRarity;

	// This specifies an item quality that items from this definition must be set to. Used mostly to specify unique item definitions.
	uint8		m_nItemQuality;
	uint8		m_nForcedItemQuality;
	uint8		m_nDefaultDropItemQuality;

	// Default drop quantity
	uint8		m_nDefaultDropQuantity;

	// Static attributes (ones that are always on these items)
	CUtlVector<static_attrib_t> m_vecStaticAttributes;

	// Seeds the popular item list with this number of the item when the list is reset.
	uint8		m_nPopularitySeed;

	// Portraits KV
	KeyValues*	m_pPortraitsKV;

	// ---------------------------------------------
	// Display related data
	// ---------------------------------------------
	// The base name of this item. i.e. "The Kritz-Krieg".
	const char		*m_pszItemBaseName;
	bool			m_bProperName;		// If set, the name will have "The" prepended to it, unless it's got a non-unique quality
										// in which case it'll have "A" prepended to the quality. i.e. A Community Kritzkrieg

	// The base type of this item. i.e. "Rocket Launcher" or "Shotgun".
	// This is often the same as the base name, but not always.
	const char		*m_pszItemTypeName;

	// A unique identifier for m_pszItemTypeName
	uint32			m_unItemTypeID;

	// The item's non-attribute description.
	const char		*m_pszItemDesc;

	// expiration time
	RTime32			m_rtExpiration;

	// item def creation time
	RTime32			m_rtDefCreation;

	// The .mdl file used for this item when it's displayed in inventory-style boxes.
	const char		*m_pszInventoryModel;
	// Alternatively, the image used for this item when it's displayed in inventory-style boxes. If specified, it's used over the model.
	const char		*m_pszInventoryImage;
	// An optional image that's overlayed over the top of the base inventory image. It'll be RGB colored by the tint color of the item.
	CUtlVector<const char*>	m_pszInventoryOverlayImages;
	int				m_iInventoryImagePosition[2];
	int				m_iInventoryImageSize[2];

	const char		*m_pszBaseDisplayModel;
	bool			m_bLoadOnDemand;
	bool			m_bHasBeenLoaded;

	bool			m_bHideBodyGroupsDeployedOnly;

	// The .mdl file used for the world view.
	// This is inferior to using a c_model, but because the geometry of the sticky bomb launcher's
	// world model is significantly different from the view model the demoman pack requires
	// using two separate models for now.
	const char		*m_pszWorldDisplayModel;
	const char		*m_pszWorldExtraWearableModel;		// Some weapons attach an extra wearable item to the player

	const char		*m_pszWorldDroppedModel;
	char m_szWorldDroppedModel[80];

	// Sticker model paths
	CUtlVector<StickerData_t>  m_vStickerModels;

	// This is the name of the default image used for the inventory until the generated image is ready
	const char		*m_pszIconDefaultImage;

	// If set, we use the base hands model for a viewmodel, and bonemerge the above player model
	bool			m_bAttachToHands;
	bool			m_bAttachToHandsVMOnly;

	// If set, we will force the view model to render flipped. Good for models built left handed.
	bool			m_bFlipViewModel;

	// This is a wearable that sits in a non-wearable loadout slot
	bool			m_bActAsWearable;	

	// The possible sets this item is a member of
	mutable CUtlVector< int > m_iItemSets;

	// Asset Modifiers & Info
	AssetInfo		*m_pAssetInfo;

	// Another way to group econ items with similar properties, separate from item_class.
	// (items of the same class may need to have different types). 
	EItemType		m_eItemType;
	bool			m_bAllowPurchaseStandalone;

	// Optional override for specifying a custom shell ejection model
	const char		*m_pszBrassModelOverride;

	IEconTool		*m_pTool;
	bundleinfo_t	*m_BundleInfo;
	item_capabilities_t m_iCapabilities;

	uint32			m_unNumConcreteItems;		// This is the number of items that will actually end up in a user's inventory - this can be 0 for some items (e.g. map stamps in TF), 1 for a "regular" item, or many for bundles, etc.

	int				m_nSoundMaterialID;

	bool			m_bDisableStyleSelection;

	CUtlString		m_sIconURLSmall;
	CUtlString		m_sIconURLLarge;
	
	//particle file
	const char		*m_pszParticleFile;				// Some items have custom particle files attached to them
	const char		*m_pszParticleSnapshotFile;		// Some weapons override a generic effect with a custom snapshot

	const char		*m_pszLootListName;				// Optionally specified loot list for a crate item (instead of a supply crate series). 

	CUtlVector<int> m_nCharacterPaintKitIndices;

protected:
	CUtlVector< WeaponPaintableMaterial_t > m_PaintData;
	InventoryImageData_t					*m_pInventoryImageData;

private:

	// ---------------------------------------------
	// Creation related data
	// ---------------------------------------------
	// The entity classname for this item.
	const char		*m_pszItemClassname;

	// The entity name that will be displayed in log files.
	const char		*m_pszItemLogClassname;

	// The name of the icon used in the death notices.
	const char		*m_pszItemIconClassname;

	// This is the script file name of this definition. Used to generate items by script name.
	const char		*m_pszDefinitionName;

#if ECONITEM_DATABASE_AUDIT_TABLES_FEATURE
	// This is used for auditing purposes
	const char		*m_pszDatabaseAuditTable;
#endif

	bool			m_bHidden;
	bool			m_bShouldShowInArmory;
	bool			m_bBaseItem;
	bool			m_bDefaultSlotItem;
	bool			m_bImported;
	bool			m_bOnePerAccountCDKEY;

	// A pack bundle is a bundle that contains items that are not for sale individually
	bool			m_bIsPackBundle;
	
	// A pack item is an item which is not for sale individually and is only for sale as part of a pack bundle. A 'regular' bundle can only include a pack bundle by explicitly including all of the pack bundle's items individually.
	// If this pointer is non-NULL, this item is considered to be a pack item (see CEconItemDefinition::IsPackItem()).
	CEconItemDefinition	*m_pOwningPackBundle;

	// Contains information on how to describe items with this attribute in the Armory
	const char		*m_pszArmoryDesc;

	// ---------------------------------------------
	// Remapping data for armory/store
	// ---------------------------------------------
	int				m_iArmoryRemap;
	int				m_iStoreRemap;
	const char		*m_pszArmoryRemap;
	const char		*m_pszStoreRemap;

	// ---------------------------------------------
	// Crafting related data
	// ---------------------------------------------
	const char		*m_pszClassToken;
	const char		*m_pszSlotToken;

	// ---------------------------------------------
	// Gameplay related data
	// ---------------------------------------------
	// How to behave when the player wearing the item dies.
	int				m_iDropType;

	// Holiday restriction. Item only has an appearance when the holiday is in effect.
	const char		*m_pszHolidayRestriction;

	// Temporary. Revisit this in the engineer update. Enables an additional buildable.
	int				m_iSubType;

	// Whitelist support for tournament mode
	bool			m_bAllowedInThisMatch;

	// this will hold a pointer to the criteria that should be used to generate the real item
	CItemSelectionCriteria *m_pProxyCriteria;

	equip_region_mask_t	m_unEquipRegionMask;			// which equip regions does this item cover directly
	equip_region_mask_t m_unEquipRegionConflictMask;	// which equip regions does equipping this item prevent from having something in them

	// Alternate icons.
	CUtlMap<uint32, const char*>*		m_pMapAlternateIcons;

	CUtlVector<econ_tag_handle_t>	m_vecTags;
	CUtlVector<const CEconItemDefinition *> m_vecContainingBundleItemDefs;	// Item definition indices for any bundles which contain this item
	WorkshopContributorList_t m_vecSteamWorkshopContributors;

	friend class CEconItemSchema;
	bool			m_bPublicItem;
	bool			m_bIgnoreInCollectionView;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline style_index_t CEconItemDefinition::GetNumStyles() const
{
	const AssetInfo *pAssetInfo = GetAssetInfo();

	if ( !pAssetInfo )
		return 0;

	// Bad things will happen if we ever get more styles than will fit in our
	// style index type. Not Very Bad things, but bad things. Mostly we'll fail
	// to iterate over all our styles.
	return pAssetInfo->m_Styles.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const CEconStyleInfo *CEconItemDefinition::GetStyleInfo( style_index_t unStyle ) const
{
	const AssetInfo *pAssetInfo = GetAssetInfo();
	if ( !pAssetInfo || !pAssetInfo->m_Styles.IsValidIndex( unStyle ) )
		return NULL;

	return pAssetInfo->m_Styles[unStyle];
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetNumAttachedModels() const
{
	if ( !GetAssetInfo() )
		return 0;

	return GetAssetInfo()->m_AttachedModels.Count(); 
}

inline attachedmodel_t *CEconItemDefinition::GetAttachedModelData( int iIdx ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	Assert( iIdx < GetAssetInfo()->m_AttachedModels.Count() );
	if ( iIdx >= GetAssetInfo()->m_AttachedModels.Count() )
		return NULL;

	return &GetAssetInfo()->m_AttachedModels[iIdx];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetNumAnimations() const
{
	if ( !GetAssetInfo() )
		return 0;

	return GetAssetInfo()->m_Animations.Count(); 
}

inline animation_on_wearable_t *CEconItemDefinition::GetAnimationData( int iIdx ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	Assert( iIdx < GetAssetInfo()->m_Animations.Count() );
	if ( iIdx < 0 || iIdx >= GetAssetInfo()->m_Animations.Count() )
		return NULL;

	return &GetAssetInfo()->m_Animations[iIdx];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetNumAnimationModifiers() const
{
	if ( !GetAssetInfo() )
		return 0;

	return GetAssetInfo()->m_vecAnimationModifiers.Count(); 
}

inline const char* CEconItemDefinition::GetAnimationModifier( int iIdx ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	if ( iIdx < 0 || iIdx >= GetAssetInfo()->m_vecAnimationModifiers.Count() )
		return NULL;

	return GetAssetInfo()->m_vecAnimationModifiers[iIdx];
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetAnnouncerName() const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszAnnouncerName;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetAnnouncerResource() const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszAnnouncerResource;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetUIOverrideIcon() const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszNewIcon;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetUIOverrideOriginalIcon() const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszOriginalIcon;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetUIOverrideAbilityIcon( const char *pszAbilityName ) const
{
	if ( !pszAbilityName )
		return NULL;

	if ( !GetAssetInfo() )
		return NULL;

	FOR_EACH_VEC( GetAssetInfo()->m_vecAbilityIconReplacements, i )
	{
		ability_icon_replacement_t *pszAbilityReplacement = &GetAssetInfo()->m_vecAbilityIconReplacements.Element(i);
		if ( pszAbilityReplacement )
		{
			if ( !Q_strcmp( pszAbilityName, pszAbilityReplacement->m_strAbilityName ) )
			{
				return pszAbilityReplacement->m_strReplacement;
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetEntityOverrideModel() const
{
	if ( !GetAssetInfo() )
		return NULL;

	CUtlVector<AssetModifier*>* pAssetModifierList = GetAssetInfo()->GetAssetModifiers( AM_EntityModel );
	if ( pAssetModifierList && pAssetModifierList->Count() > 0 )
		return pAssetModifierList->Element(0)->m_strModifier.String();

	return NULL;
}

inline const char *CEconItemDefinition::GetViewOverrideModel() const
{
	if ( !GetAssetInfo() )
		return NULL;

	CUtlVector<AssetModifier*>* pAssetModifierList = GetAssetInfo()->GetAssetModifiers( AM_ViewModel );
	if ( pAssetModifierList && pAssetModifierList->Count() > 0 )
		return pAssetModifierList->Element(0)->m_strModifier.String();

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetEntityOverrideClass() const
{
	if ( !GetAssetInfo() )
		return NULL;

	CUtlVector<AssetModifier*>* pAssetModifierList = GetAssetInfo()->GetAssetModifiers( AM_EntityModel );
	if ( pAssetModifierList && pAssetModifierList->Count() > 0 )
		return pAssetModifierList->Element(0)->m_strAsset.String();

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetScaleOverrideClass() const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszScaleClass;
}

//-----------------------------------------------------------------------------
// Purpose: accessor for hero_model_change asset modifier
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetHeroModelChangeModel() const
{
	if ( !GetAssetInfo() )
		return NULL;

	CUtlVector<AssetModifier*>* pAssetModifierList = GetAssetInfo()->GetAssetModifiers( AM_HeroModelChange );
	if ( pAssetModifierList && pAssetModifierList->Count() > 0 )
		return pAssetModifierList->Element(0)->m_strModifier.String();

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline float CEconItemDefinition::GetScaleOverrideSize() const
{
	if ( !GetAssetInfo() )
		return 0.0f;

	return GetAssetInfo()->m_flScaleSize;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
inline bool CEconItemDefinition::SkipModelCombine( void ) const
{
	if ( !GetAssetInfo() )
		return false;

	return GetAssetInfo()->m_bSkipModelCombine;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetNumSounds() const
{
	if ( !GetAssetInfo() )
		return 0;

	return GetAssetInfo()->m_Sounds.Count(); 
}

inline sound_on_wearable_t *CEconItemDefinition::GetSoundData( int iIdx ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	Assert( iIdx < GetAssetInfo()->m_Sounds.Count() );
	if ( iIdx >= GetAssetInfo()->m_Sounds.Count() )
		return NULL;

	return &GetAssetInfo()->m_Sounds[iIdx];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetNumParticles() const
{
	if ( !GetAssetInfo() )
		return 0;

	return GetAssetInfo()->m_Particles.Count(); 
}

inline particle_on_wearable_t *CEconItemDefinition::GetParticleData( int iIdx ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	Assert( iIdx < GetAssetInfo()->m_Particles.Count() );
	if ( iIdx >= GetAssetInfo()->m_Particles.Count() )
		return NULL;

	return &GetAssetInfo()->m_Particles[iIdx];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetNumParticleSnapshots() const
{
	if ( !GetAssetInfo() )
		return 0;

	return GetAssetInfo()->m_ParticleSnapshots.Count(); 
}

inline particlesnapshot_on_wearable_t *CEconItemDefinition::GetParticleSnapshotData( int iIdx ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	Assert( iIdx < GetAssetInfo()->m_ParticleSnapshots.Count() );
	if ( iIdx >= GetAssetInfo()->m_ParticleSnapshots.Count() )
		return NULL;

	return &GetAssetInfo()->m_ParticleSnapshots[iIdx];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetNumParticleControlPoints() const
{
	if ( !GetAssetInfo() )
		return 0;

	return GetAssetInfo()->m_ParticleControlPoints.Count(); 
}

inline particle_control_point_on_wearable_t *CEconItemDefinition::GetParticleControlPointData( int iIdx ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	Assert( iIdx < GetAssetInfo()->m_ParticleControlPoints.Count() );
	if ( iIdx >= GetAssetInfo()->m_ParticleControlPoints.Count() )
		return NULL;

	return &GetAssetInfo()->m_ParticleControlPoints[iIdx];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetNumAttachedParticles() const
{ 
	if ( !GetAssetInfo() )
		return 0;

	return GetAssetInfo()->m_AttachedParticles.Count(); 
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetMaterialOverride() const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszMaterialOverride;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetMuzzleFlash() const
{
	if ( !GetAssetInfo() )
		return NULL;
		
	return GetAssetInfo()->m_pszMuzzleFlash;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetTracerEffect() const
{
	if ( !GetAssetInfo() )
		return NULL;
		
	return GetAssetInfo()->m_pszTracerEffect;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetParticleEffect() const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszParticleEffect;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetParticleSnapshot() const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszParticleSnapshot;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetHiddenParentBodygroup() const
{
	if ( !GetAssetInfo() )
		return -1;

	return GetAssetInfo()->iHideParentBodyGroup;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetNumModifiedBodyGroups() const
{
	if ( !GetAssetInfo() )
		return -1;

	return GetAssetInfo()->m_ModifiedBodyGroupNames.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char* CEconItemDefinition::GetModifiedBodyGroup( int i, int& body ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	body = GetAssetInfo()->m_ModifiedBodyGroupNames[i];
	return GetAssetInfo()->m_ModifiedBodyGroupNames.Key(i);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetNumCodeControlledBodyGroups() const
{
	if ( !GetAssetInfo() )
		return -1;

	return GetAssetInfo()->m_CodeControlledBodyGroupNames.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char* CEconItemDefinition::GetCodeControlledBodyGroup( int i, codecontrolledbodygroupdata_t &ccbgd ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	ccbgd = GetAssetInfo()->m_CodeControlledBodyGroupNames[i];
	return GetAssetInfo()->m_CodeControlledBodyGroupNames.Key(i);
}

#if defined(CLIENT_DLL) || defined(GAME_DLL)
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetStyleSkin( style_index_t unStyle ) const
{
	const CEconStyleInfo *pStyle = GetStyleInfo( unStyle );

	// Return our skin if we have a style or our default skin of -1 otherwise.
	return pStyle
		 ? pStyle->GetSkin()
		 : -1;
}
#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetViewmodelBodygroupOverride() const
{
	if ( !GetAssetInfo() )
		return 0;

	return GetAssetInfo()->m_iViewModelBodyGroupOverride;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetViewmodelBodygroupStateOverride() const
{
	if ( !GetAssetInfo() )
		return 0;

	return GetAssetInfo()->m_iViewModelBodyGroupStateOverride;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetWorldmodelBodygroupOverride() const
{
	if ( !GetAssetInfo() )
		return 0;

	return GetAssetInfo()->m_iWorldModelBodyGroupOverride;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline int CEconItemDefinition::GetWorldmodelBodygroupStateOverride() const
{
	if ( !GetAssetInfo() )
		return 0;

	return GetAssetInfo()->m_iWorldModelBodyGroupStateOverride;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline bool CEconItemDefinition::UsesPerClassBodygroups() const
{
	if ( !GetAssetInfo() )
		return false;

	return GetAssetInfo()->bUsePerClassBodygroups;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetCustomSound( int iSound ) const
{
	if ( !GetAssetInfo() )
		return NULL;
	if ( iSound < 0 || iSound >= MAX_VISUALS_CUSTOM_SOUNDS )
		return NULL;
	return GetAssetInfo()->m_pszCustomSounds[iSound];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetWeaponReplacementSound( /* WeaponSound_t */ int iSound ) const
{
	if ( !GetAssetInfo() )
		return NULL;
	if ( iSound < 0 || iSound >= NUM_SHOOT_SOUND_TYPES )
		return NULL;
	return GetAssetInfo()->m_pszWeaponSoundReplacements[iSound];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetPrimaryAmmo( void ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszPrimaryAmmo;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetWeaponTypeString( void ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszWeaponTypeString;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetAddonLocation( void ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszAddonLocation;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetEjectBrassEffect( void ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszEjectBrassEffect;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetMuzzleFlashEffect1stPerson( void ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszMuzzleFlashEffect1stPerson;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetMuzzleFlashEffect1stPersonAlt( void ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszMuzzleFlashEffect1stPersonAlt;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetMuzzleFlashEffect3rdPerson( void ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszMuzzleFlashEffect3rdPerson;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetMuzzleFlashEffect3rdPersonAlt( void ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszMuzzleFlashEffect3rdPersonAlt;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetHeatEffect( void ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszHeatEffect;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline const char *CEconItemDefinition::GetPlayerAnimationExtension( void ) const
{
	if ( !GetAssetInfo() )
		return NULL;

	return GetAssetInfo()->m_pszPlayerAnimationExtension;
}

#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

//-----------------------------------------------------------------------------
// CTimedItemRewardDefinition
// Describes a periodic item reward
//-----------------------------------------------------------------------------
class CTimedItemRewardDefinition
{
public:
	CTimedItemRewardDefinition( void );
	CTimedItemRewardDefinition( const CTimedItemRewardDefinition &that );
	CTimedItemRewardDefinition &operator=( const CTimedItemRewardDefinition& rhs );

	~CTimedItemRewardDefinition( void );

	bool		BInitFromKV( KeyValues *pKVTimedReward, CEconItemSchema &pschema, CUtlVector<CUtlString> *pVecErrors = NULL );

	uint32		GetRandomFrequency( void ) const	{ return RandomFloat( m_unMinFreq, m_unMaxFreq ); }
	uint32		GetMinFrequency( void ) const		{ return m_unMinFreq; }
	uint32		GetMaxFrequency( void ) const		{ return m_unMaxFreq; }
	
	float		GetChance( void ) const; 
	RTime32		GetForcedBaselineAdjustmentTime( void ) const	{ return m_rtForcedBaselineAdjustment; }
	RTime32		GetForcedLastDropAdjustmentTime( void ) const	{ return m_rtForcedLastDropTimeAdjustment; }
	uint32		GetHoursInRewardPeriod( void ) const			{ return m_unHoursInRewardPeriod; }
	uint32		GetHoursBetweenDropsRealtime( void ) const		{ return m_unHoursBetweenDropsRealtime; }
	uint32		GetTotalPointsForPeriod( float flHoursOfPlaytimeInPeriod ) const
	{
		float flHours = flHoursOfPlaytimeInPeriod;
		if ( flHours <= 0.0f )
			return 0;

		uint32 unPoints = 0;
		uint32 numPointsToProrate = 0;
		uint32 nHoursPlayed = ( uint32 ) flHours; // truncating hours
		if ( !nHoursPlayed )
		{	// Crediting the first hour of the play period (prorating)
			numPointsToProrate = ( m_arrTotalPointsBasedOnHoursPlayed.Count() ) ? m_arrTotalPointsBasedOnHoursPlayed.Head() : m_unPointsPerHourOverplayed;
		}
		else if ( uint32 numHoursToCreditFromTable = MIN( nHoursPlayed, ( uint32 ) m_arrTotalPointsBasedOnHoursPlayed.Count() ) )
		{	// We have a defined table, credit from it as much as we can
			nHoursPlayed -= numHoursToCreditFromTable;
			flHours -= numHoursToCreditFromTable;
			unPoints += m_arrTotalPointsBasedOnHoursPlayed[ numHoursToCreditFromTable - 1 ];
			numPointsToProrate = ( numHoursToCreditFromTable < ( uint32 ) m_arrTotalPointsBasedOnHoursPlayed.Count() )
				? ( m_arrTotalPointsBasedOnHoursPlayed[ numHoursToCreditFromTable ] - m_arrTotalPointsBasedOnHoursPlayed[ numHoursToCreditFromTable - 1 ] )
				: m_unPointsPerHourOverplayed;
		}

		if ( nHoursPlayed )
		{	// We have hours that go beyond the table definition, or had no table
			unPoints += nHoursPlayed * m_unPointsPerHourOverplayed;
			numPointsToProrate = m_unPointsPerHourOverplayed;
			flHours -= nHoursPlayed;
		}

		// At this point we have a fraction of an hour that we are prorating remaining
		unPoints += flHours * numPointsToProrate;
		return unPoints;
	}
	uint32		GetPointsPerPeriodRollover( void ) const		{ return m_unPointsPerPeriodRollover; }

	const CItemSelectionCriteria &GetCriteria( void ) const		{ return m_criteria; }
	int GetLootListCount( void ) const							{ return m_arrLootLists.Count(); }
	const CEconLootListDefinition *GetLootList( int iLootListIdx = 0 ) const { return m_arrLootLists.IsValidIndex( iLootListIdx ) ? m_arrLootLists[iLootListIdx] : NULL; }
private:
	// Frequency of how often the item is awarded
	uint32		m_unMinFreq;
	uint32		m_unMaxFreq;

	RTime32		m_rtForcedBaselineAdjustment;	// Forces all baselines to be adjusted at least beyond this mark, when tweaking drop rates and reward periods
	RTime32		m_rtForcedLastDropTimeAdjustment;	// Forces last drop time to be adjusted to at least this mark, when tweaking drop rates and reward periods
	uint32		m_unHoursInRewardPeriod;		// How many hours a reward period lasts before rolling over
	uint32		m_unHoursBetweenDropsRealtime;	// How many realtime hours before dropping next drop due from this series
	CUtlVector< uint32 > m_arrTotalPointsBasedOnHoursPlayed;	// Description table of total points a player can earn in reward period based on hours played
	uint32		m_unPointsPerHourOverplayed;	// Every hour beyond the points table will reward a player with so many points
	uint32		m_unPointsPerPeriodRollover;	// Points per period that will rollover

	// The chance, between 0 and 1, that the item is rewarded
	float		m_flChance;


	// The criteria to use to select the item to reward
	CItemSelectionCriteria m_criteria;
	// Alternatively, the loot_list to use instead
	CUtlVector< const CEconLootListDefinition * > m_arrLootLists;

	// dynamically allocated loot lists are in both vectors, but we keep this list to delete on destruct
	// loot lists from the schema are in the above vector as well and are stored statically.
	CUtlVector< const CEconLootListDefinition * > m_arrDynamicLootLists;
};

//-----------------------------------------------------------------------------
// CItemLevelingDefinition
//-----------------------------------------------------------------------------
class CItemLevelingDefinition
{
public:
	CItemLevelingDefinition( void );
	CItemLevelingDefinition( const CItemLevelingDefinition &that );
	CItemLevelingDefinition &operator=( const CItemLevelingDefinition& rhs );

	~CItemLevelingDefinition( void );

	bool		BInitFromKV( KeyValues *pKVItemLevel, CEconItemSchema &pschema, const char *pszLevelBlockName, CUtlVector<CUtlString> *pVecErrors = NULL );

	uint32		GetLevel( void ) const { return m_unLevel; }
	uint32		GetRequiredScore( void ) const { return m_unRequiredScore; }
	const char *GetNameLocalizationKey( void ) const { return m_pszLocalizedName_LocalStorage; }

private:
	uint32		m_unLevel;
	uint32		m_unRequiredScore;
	char	   *m_pszLocalizedName_LocalStorage;
};

//-----------------------------------------------------------------------------
// AchievementAward_t
// Holds the item to give away and the Data value to audit it with ( for cross
// game achievements)
//-----------------------------------------------------------------------------
struct AchievementAward_t
{
	AchievementAward_t( const AchievementAward_t & rhs )
		: m_sNativeName( rhs.m_sNativeName ),
		m_unSourceAppId( rhs.m_unSourceAppId ),
		m_unAuditData( rhs.m_unAuditData )
	{
		m_vecDefIndex.CopyArray( rhs.m_vecDefIndex.Base(), rhs.m_vecDefIndex.Count() );
	}
	AchievementAward_t(  ) {}

	CUtlString m_sNativeName;
	AppId_t m_unSourceAppId;
	uint32 m_unAuditData;
	CUtlVector<uint16> m_vecDefIndex;
};

enum eTimedRewardType
{
	kTimedRewards_Reward,
	kTimedRewards_PremiumTimedDrop,
	kTimedRewards_Crate,
	kTimedRewards_Operation,
	kTimedRewards_Capsule,
	kTimedRewards_Quest,
	kTimedRewards_FreeSprayDropsOct2016,
	kNumTimedRewards
};

struct kill_eater_score_type_t
{
	// The value that the game/DB will know this rarity by
	int32		m_nValue;
	const char *m_pszTypeString;
	const char *m_pszModelAttributeString;
	const char *m_pszLevelBlockName;
	bool		m_bUseLevelBlock;
};

//-----------------------------------------------------------------------------
// CWebResource
//-----------------------------------------------------------------------------
class CWebResource
{
public:
	CWebResource() : m_bOnDemand( false ), m_pKeyValues( NULL ), m_fnLoadCallback( NULL ) {}
	CWebResource( const CWebResource& other )
	{
		m_strName = other.m_strName;
		m_strURL = other.m_strURL;
		m_bOnDemand = other.m_bOnDemand;
		m_pKeyValues = other.m_pKeyValues->MakeCopy();
		m_fnLoadCallback = other.m_fnLoadCallback;
	}
	~CWebResource() { m_pKeyValues->deleteThis(); }

	CUtlString	m_strName;
	CUtlString	m_strURL;
	bool		m_bOnDemand;
	KeyValues*	m_pKeyValues;
	void (*m_fnLoadCallback)( const char*, KeyValues* );

	static		bool	s_Initialized;
};

//-----------------------------------------------------------------------------
// CForeignAppImports
// Defines the way a single foreign app's items are mapped into this app
//-----------------------------------------------------------------------------

class CForeignAppImports
{
public:
	CForeignAppImports() : m_mapDefinitions( DefLessFunc( uint16 ) ) {}

	void AddMapping( uint16 unForeignDefIndex, const CEconItemDefinition *pDefn );
	const CEconItemDefinition *FindMapping( uint16 unForeignDefIndex ) const;

private:
	CUtlMap< uint16, const CEconItemDefinition *> m_mapDefinitions;
};

//-----------------------------------------------------------------------------
// ISchemaAttributeType
//-----------------------------------------------------------------------------

// ISchemaAttributeType is the base interface for a "type" of attribute, where "type" is defined as
// "something that describes the memory layout, the DB layout, how to convert between them, etc.".
// Most of the low-level work done with attributes, including DB reading/writing, packing/unpacking
// for wire traffic, and other leaf code works exclusively through this interface.
//
// The class hierarchy looks like:
//
//		ISchemaAttributeTypeBase< TAttribInMemoryType >:
//	
//			This describes a specific in-memory format for an attribute, without any association to
//			a particular DB, wire format, etc. We can't template the base class because it's an
//			interface. This implements about half of ISchemaAttributeType and has its own mini
//			interface consisting of ConvertTypedValueToByteStream()	and ConvertByteStreamToTypedValue(),
//			both of which do work on statically-typed values that don't exist at higher levels.
//
//		CSchemaAttributeTypeBase< TAttribSchType, TAttribInMemoryType >:
//
//			This handles the schema-related functions on ISchemaAttributeType. These exist at a lower
//			inheritance level than ISchemaAttributeTypeBase to allow code that needs to work type-safely
//			on attributes in memory, but that doesn't know or need to know anything about databases,
//			to exist. Examples of this include code that calls CEconItem::SetDynamicAttributeValue<T>().
//
//			Individual implementations of custom attribute type start making sense immediately as
//			subclasses of CSchemaAttributeTypeBase, for example CSchemaAttributeType_Default, which
//			implements all of the old, untyped attribute system logic.
//
//		CSchemaAttributeTypeProtobufBase< TAttribSchType, TProtobufValueType >
//
//			An easy way of automating most of the work for making a new attribute type is to have
//			the in-memory format be a protobuf object, allowing reflection, automatic network support,
//			etc.
//
// Creating a new custom protobuf attribute consists of three steps:
//
//		- create a new DB table that will hold your attribute data. This needs an itemid_t-sized item ID
//		  column named "ItemID", an attrib_definition_index_t-sized definition index column named "AttrDefIndex",
//		  and then whatever data you want to store.
//
//		- create a new protobuf message type that will hold your custom attribute contents. This exists
//		  on the client and the GC in the same format.
//
//		- implement a subclass of CSchemaAttributeTypeProtobufBase<>, for example:
//
//				class CSchemaAttributeType_StrangeScore : public CSchemaAttributeTypeProtobufBase< GC_SCH_REFERENCE( CSchItemAttributeStrangeScore ) CAttribute_StrangeScore >
//				{
//					virtual void ConvertEconAttributeValueToSch( itemid_t unItemId, const CEconItemAttributeDefinition *pAttrDef, const union attribute_data_union_t& value, GCSDK::CRecordBase *out_pSchRecord ) const OVERRIDE;
//					virtual void LoadSchToEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const GCSDK::CRecordBase *pSchRecord ) const OVERRIDE;
//				};
//
//		  Implement these two GC-only functions to convert from the in-memory format to the DB format and
//		  vice versa and you're good to go.
//
//		- register the new type in CEconItemSchema::BInitAttributeTypes().
//
// If the attribute type can't be silently converted to an already-existing attribute value type, a few other
// places will also fail to compile -- things like typed iteration, or compile-time type checking.
//
// Functions that start with "Convert" change the format of an attribute value (in a type-safe way wherever
// possible), copying the value from one of the passed-in parameters to the other. Functions that start with
// "Load" do a format conversion, but also add the post-conversion value to the passed-in CEconItem. This
// comes up most often when generating new items, either from the DB (LoadSch), the network (LoadByteSteam),
// or creation of a new item on the GC (LoadOrGenerate).
class ISchemaAttributeType
{
public:
	virtual ~ISchemaAttributeType() { }

	// Returns a unique integer describing the C++-in-memory-layout type used by this attribute type.
	// For example, something that stores "int" might return 0 and "CSomeFancyWideAttributeType" might
	// return 1. The actual values don't matter and can even differ between different runs of the game/GC.
	// The only important thing is that during a single run the value for a single type is consistent.
	virtual unsigned int GetTypeUniqueIdentifier() const = 0;

	// Have this attribute type copy the data out of the value union and type-copy it onto the item. This
	// is accessible on clients as well as the GC.
	virtual void LoadEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const union attribute_data_union_t& value ) const = 0;

	// ...
	virtual void ConvertEconAttributeValueToByteStream( const union attribute_data_union_t& value, std::string *out_psBytes ) const = 0;

	// ...
	virtual bool BConvertStringToEconAttributeValue( const CEconItemAttributeDefinition *pAttrDef, const char *pszValue, union attribute_data_union_t *out_pValue ) const = 0;

	// ...
	virtual void ConvertEconAttributeValueToString( const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value, std::string *out_ps ) const = 0;

	// Used to deserialize a byte stream, probably from an on-wire protobuf message, instead an instance
	// of the attribute in memory. See ConvertByteStreamToTypedValue() for example implementation, or
	// ConvertTypedValueToByteStream() for an example of the byte-stream generator code.
	virtual void LoadByteStreamToEconAttributeValue( CEconItem *pTargetItem, const CEconItemAttributeDefinition *pAttrDef, const std::string& sBytes ) const = 0;

	// ...
	virtual void InitializeNewEconAttributeValue( attribute_data_union_t *out_pValue ) const = 0;

	// Free any heap-allocated memory from this attribute value. Is not responsible for zeroing out
	// pointers, etc.
	virtual void UnloadEconAttributeValue( union attribute_data_union_t *out_pValue ) const = 0;

	// ...
	virtual bool OnIterateAttributeValue( class IEconItemAttributeIterator *pIterator, const CEconItemAttributeDefinition *pAttrDef, const attribute_data_union_t& value ) const = 0;

	// This could also be called "BIsHackyMessyOldAttributeType()". This determines whether the attribute
	// can be set at runtime on a CEconItemView instance, whether the gameserver can replicate the value to
	// game clients, etc. It really only makes sense for value types 32 bits or smaller.
	virtual bool BSupportsGameplayModificationAndNetworking() const { return false; }
};




struct CAppliedStickerInfo_t
{
	int nID;
	float flWearMin;
	float flWearMax;
	float flScale;
	float flRotate;
};

class CStickerKit
{
public:
	
	CStickerKit( const CStickerKit & rhs )
		: sName( rhs.sName ), 
		sDescriptionString( rhs.sDescriptionString ),
		sItemName( rhs.sItemName ),
		sMaterialPath( rhs.sMaterialPath ),
		m_strInventoryImage( rhs.m_strInventoryImage )
	{
		nID = rhs.nID;
		nRarity = rhs.nRarity;
		flRotateStart = rhs.flRotateStart;
		flRotateEnd = rhs.flRotateEnd;
		flScaleMin = rhs.flScaleMin;
		flScaleMax = rhs.flScaleMax;
		flWearMin = rhs.flWearMin;
		flWearMax = rhs.flWearMax;
		bMaterialPathIsAbsolute = rhs.bMaterialPathIsAbsolute;

		m_nEventID = rhs.m_nEventID;
		m_nEventTeamID = rhs.m_nEventTeamID;
		m_nPlayerID = rhs.m_nPlayerID;

		m_pKVItem = rhs.m_pKVItem ? rhs.m_pKVItem->MakeCopy() : NULL;
	}

	CStickerKit( void )
	{
		nID = 0;
		nRarity = 1;
		flRotateStart = 0.0f;
		flRotateEnd = 0.0f;
		flScaleMax = flScaleMin = 1.0f;
		flWearMin = 0.0f;
		flWearMax = 1.0f;
		bMaterialPathIsAbsolute = false;

		m_nEventID = 0;
		m_nEventTeamID = 0;
		m_nPlayerID = 0;

		m_pKVItem = NULL;
	}

	~CStickerKit()
	{
		if ( m_pKVItem )
			m_pKVItem->deleteThis();
		m_pKVItem = NULL;
	}

	bool InitFromKeyValues( KeyValues *pKVEntry, const CStickerKit *pDefault, CUtlVector<CUtlString> *pVecErrors = NULL );
	bool GenerateStickerApplicationInfo( CAppliedStickerInfo_t *pInfo ) const;

	int nID;
	int nRarity;
	CUtlString sName;
	CUtlString sDescriptionString;
	CUtlString sItemName;
	CUtlString sMaterialPath;
	CUtlString sMaterialPathNoDrips;
	CUtlString m_strInventoryImage;

	int m_nEventID;
	int m_nEventTeamID;
	int m_nPlayerID;

	bool bMaterialPathIsAbsolute;

	float flRotateStart;
	float flRotateEnd;

	float flScaleMin;
	float flScaleMax;

	float flWearMin;
	float flWearMax;

	const char *GetInventoryImage( void ) const			{ return m_strInventoryImage; }

	const char  *GetIconURLSmall() const			{ return m_sIconURLSmall; }
	const char  *GetIconURLLarge() const			{ return m_sIconURLLarge; }
	void	SetIconURLSmall( const char *szURL )	{ m_sIconURLSmall = szURL; }
	void	SetIconURLLarge( const char *szURL )	{ m_sIconURLLarge = szURL; }

	KeyValues	*GetRawDefinition( void ) const		{ return m_pKVItem; }

private:
	CUtlString m_sIconURLSmall;
	CUtlString m_sIconURLLarge;
	KeyValues *m_pKVItem;
};

class CStickerList
{
public:
	CStickerList( const CStickerList & rhs )
	{
		flWearMin = rhs.flWearMin;
		flWearMax = rhs.flWearMax;

		flTotalWeight = rhs.flTotalWeight;
		arrElements.AddMultipleToTail( rhs.arrElements.Count(), rhs.arrElements.Base() );
	}
	CStickerList()
	{
		flWearMin = 0.0f;
		flWearMax = 1.0f;

		flTotalWeight = 0.0f;
	}

	bool InitFromKeyValues( KeyValues *pKVEntry, CUtlVector<CUtlString> *pVecErrors = NULL );

	bool GenerateStickerApplicationInfo( CAppliedStickerInfo_t *pInfo ) const;

	struct sticker_list_entry_t
	{
		const CStickerList *pList;
		const CStickerKit *pKit;
		float flWeight;
	};
	CUtlVector< sticker_list_entry_t > arrElements;
	float flTotalWeight;

	float flWearMin;
	float flWearMax;
};

class CPaintKit
{
public:
	enum { NUM_COLORS = 4 };

	CPaintKit( const CPaintKit & rhs )
		: sName( rhs.sName ), 
		  sDescriptionString( rhs.sDescriptionString ),
		  sDescriptionTag( rhs.sDescriptionTag ),
		  sVmtPath( rhs.sVmtPath ),
		  sPattern( rhs.sPattern ),
		  sLogoMaterial( rhs.sLogoMaterial )
	{
		nID = rhs.nID;

		kvVmtOverrides = rhs.kvVmtOverrides->MakeCopy();

		bBaseDiffuseOverride = rhs.bBaseDiffuseOverride;
		nRarity = rhs.nRarity;
		nStyle = rhs.nStyle;
		flWearDefault = rhs.flWearDefault;
		flWearRemapMin = rhs.flWearRemapMin;
		flWearRemapMax = rhs.flWearRemapMax;
		nFixedSeed = rhs.nFixedSeed;
		uchPhongExponent = rhs.uchPhongExponent;
		uchPhongAlbedoBoost = rhs.uchPhongAlbedoBoost;
		uchPhongIntensity = rhs.uchPhongIntensity;
		flPatternScale = rhs.flPatternScale;
		flPatternOffsetXStart = rhs.flPatternOffsetXStart;
		flPatternOffsetXEnd = rhs.flPatternOffsetXEnd;
		flPatternOffsetYStart = rhs.flPatternOffsetYStart;
		flPatternOffsetYEnd = rhs.flPatternOffsetYEnd;
		flPatternRotateStart = rhs.flPatternRotateStart;
		flPatternRotateEnd = rhs.flPatternRotateEnd;
		flLogoScale = rhs.flLogoScale;
		flLogoOffsetX = rhs.flLogoOffsetX;
		flLogoOffsetY = rhs.flLogoOffsetY;
		flLogoRotation = rhs.flLogoRotation;
		bIgnoreWeaponSizeScale = rhs.bIgnoreWeaponSizeScale;
		nViewModelExponentOverrideSize = rhs.nViewModelExponentOverrideSize;
		bOnlyFirstMaterial = rhs.bOnlyFirstMaterial;

		memcpy( rgbaLogoColor, rhs.rgbaLogoColor, sizeof( rgbaLogoColor ) );
		memcpy( rgbaLogoColor, rhs.rgbaLogoColor, sizeof( rgbaLogoColor ) );
	}

	CPaintKit( void )
	{
		nID = 0;

		kvVmtOverrides = nullptr;

		bBaseDiffuseOverride = false;
		nRarity = 1;
		nStyle = 0;
		flWearDefault = 0.5f;
		flWearRemapMin = 0.0f;
		flWearRemapMax = 1.0f;
		nFixedSeed = 0;
		uchPhongExponent = 16;
		uchPhongAlbedoBoost = 0;
		uchPhongIntensity = 255;
		flPatternScale = 0.5f;
		flPatternOffsetXStart = 0.0f;
		flPatternOffsetXEnd = 0.0f;
		flPatternOffsetYStart = 0.0f;
		flPatternOffsetYEnd = 0.0f;
		flPatternRotateStart = 0.0f;
		flPatternRotateEnd = 0.0f;
		flLogoScale = 1.8f;
		flLogoOffsetX = 1.0f;
		flLogoOffsetY = 2.0f;
		flLogoRotation = 0.0f;
		bIgnoreWeaponSizeScale = false;
		nViewModelExponentOverrideSize = 256;
		bOnlyFirstMaterial = false;

		memset( rgbaColor, 128, sizeof( rgbaColor ) );
		memset( rgbaLogoColor, 128, sizeof( rgbaLogoColor ) );
	}

	~CPaintKit()
	{
		if ( kvVmtOverrides )
		{
			kvVmtOverrides->deleteThis();
			kvVmtOverrides = nullptr;
		}
	}

	bool InitFromKeyValues( KeyValues *pKVEntry, const CPaintKit *pDefault, bool bHandleAbsolutePaths = false );
	void FillKeyValuesForWorkshop( KeyValues *pKVToFill ) const;

	// Generic fields
	int nID;
	CUtlString sName;
	CUtlString sDescriptionString;
	CUtlString sDescriptionTag;
	
	// (Generally) Weapon paint kit fields
	// Note that some fields may affect item generation for other paint kit types;
	// in particular the wear settings.
	CUtlString sPattern;
	CUtlString sLogoMaterial;
	bool bBaseDiffuseOverride;
	int nRarity;
	int nStyle;
	Color rgbaColor[ NUM_COLORS ];
	Color rgbaLogoColor[ NUM_COLORS ];
	float flWearDefault;
	float flWearRemapMin;
	float flWearRemapMax;
	unsigned char nFixedSeed;
	unsigned char uchPhongExponent;
	unsigned char uchPhongAlbedoBoost;
	unsigned char uchPhongIntensity;
	float flPatternScale;
	float flPatternOffsetXStart;
	float flPatternOffsetXEnd;
	float flPatternOffsetYStart;
	float flPatternOffsetYEnd;
	float flPatternRotateStart;
	float flPatternRotateEnd;
	float flLogoScale;
	float flLogoOffsetX;
	float flLogoOffsetY;
	float flLogoRotation;
	bool bIgnoreWeaponSizeScale;
	int nViewModelExponentOverrideSize;
	bool bOnlyFirstMaterial;

	// Character paint kit fields
	CUtlString sVmtPath;
	KeyValues* kvVmtOverrides;
};

class AlternateIconData_t
{
public:
	AlternateIconData_t()
	{
	}

	const char *GetInventoryImage( void ) const			{ return sSimpleName; }

	const char  *GetIconURLSmall() const			{ return m_sIconURLSmall; }
	const char  *GetIconURLLarge() const			{ return m_sIconURLLarge; }
	void	SetIconURLSmall( const char *szURL )	{ m_sIconURLSmall = szURL; }
	void	SetIconURLLarge( const char *szURL )	{ m_sIconURLLarge = szURL; }

	CUtlString sSimpleName;
	CUtlString sLargeSimpleName;

private:
	CUtlString m_sIconURLSmall;
	CUtlString m_sIconURLLarge;
};

class CProPlayerData
{
public:

	CProPlayerData( const CProPlayerData & rhs )
	{
		m_nAccountID = rhs.m_nAccountID;
		m_sName = rhs.m_sName;
		m_sCode = rhs.m_sCode;
		m_rtDOB = rhs.m_rtDOB;
		m_sGeo = rhs.m_sGeo;

		m_pKVItem = rhs.m_pKVItem ? rhs.m_pKVItem->MakeCopy() : NULL;
	}

	CProPlayerData( void )
	{
		m_nAccountID = 0;
		m_rtDOB = 0;
		m_pKVItem = NULL;
	}

	~CProPlayerData()
	{
		if ( m_pKVItem )
			m_pKVItem->deleteThis();
		m_pKVItem = NULL;
	}

	bool BInitFromKeyValues( KeyValues *pDef, CUtlVector<CUtlString> *pVecErrors = NULL );

	KeyValues	*GetRawDefinition( void ) const		{ return m_pKVItem; }
	uint32 GetAccountID() const { return m_nAccountID; }
	char const * GetName() const { return m_sName; }
	char const * GetCode() const { return m_sCode; }
	RTime32 GetDOB() const { return m_rtDOB; }
	char const * GetGeo() const { return m_sGeo; }

private:
	KeyValues *m_pKVItem;
	uint32 m_nAccountID;
	CUtlString m_sName;
	CUtlString m_sCode;
	RTime32 m_rtDOB;
	CUtlString m_sGeo;
};

//-----------------------------------------------------------------------------
// CEconItemSchema
// Defines the way econ items can be used in a game
//-----------------------------------------------------------------------------
typedef CUtlDict<CUtlConstString, int> ArmoryStringDict_t;
typedef CUtlDict< CUtlVector<CItemLevelingDefinition> * > LevelBlockDict_t;
typedef CUtlMap<unsigned int, kill_eater_score_type_t>	KillEaterScoreMap_t;

struct attr_type_t
{
	CUtlConstString m_sName;
	const ISchemaAttributeType *m_pAttrType;

	attr_type_t( const char *pszName, const ISchemaAttributeType *pAttrType )
		: m_sName( pszName )
		, m_pAttrType( pAttrType )
	{
	}
};

#if defined(CLIENT_DLL) || defined(GAME_DLL)
class IDelayedSchemaData
{
public:
	virtual ~IDelayedSchemaData() {}
	virtual bool InitializeSchema( CEconItemSchema *pItemSchema ) = 0;

protected:
	// Passing '0' as the expected version means "we weren't expecting any version in particular" and will
	// skip the sanity checking.
	bool InitializeSchemaInternal( CEconItemSchema *pItemSchema, CUtlBuffer& bufRawData, bool bInitAsBinary, uint32 nExpectedVersion );
};
#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

class CEconStorePriceSheet;

class CEconItemSchema : public IEconItemSchema
{
public:
	CEconItemSchema( );

private:
	CEconItemSchema( const CEconItemSchema & rhs );
	CEconItemSchema &operator=( CEconItemSchema & rhs );

public:
	virtual ~CEconItemSchema( void ) { Reset(); };

	// Setup & parse in the item data files.
	virtual bool BInit( const char *fileName, const char *pathID, CUtlVector<CUtlString> *pVecErrors = NULL );
	bool		BInitBinaryBuffer( CUtlBuffer &buffer, CUtlVector<CUtlString> *pVecErrors = NULL );
	bool		BInitTextBuffer( CUtlBuffer &buffer, CUtlVector<CUtlString> *pVecErrors = NULL );

	uint32		GetVersion() const { return m_unVersion; }
	uint32		GetResetCount() const { return m_unResetCount; }

	static CUniformRandomStream& GetRandomStream() { return m_RandomStream; }

	// Perform the computation used to calculate the schema version
	static uint32 CalculateKeyValuesVersion( KeyValues *pKV );

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	// This function will immediately reinitialize the schema if it's safe to do so, or store off the data
	// if it isn't safe to update at the moment.
	void		MaybeInitFromBuffer( IDelayedSchemaData *pDelayedSchemaData );

	// If there is saved schema initialization data, initialize it now. If there is no saved data, this
	// will return success.
	bool		BInitFromDelayedBuffer();
#endif // defined(CLIENT_DLL) || defined(GAME_DLL)

	// Accessors to the base properties
	equipped_class_t	GetFirstValidClass() const							{ return m_unFirstValidClass; }
	equipped_class_t	GetLastValidClass() const							{ return m_unLastValidClass; }
	bool				IsValidClass( equipped_class_t unClass )			{ return unClass >= m_unFirstValidClass && unClass <= m_unLastValidClass; }
	equipped_slot_t		GetFirstValidItemSlot() const						{ return m_unFirstValidItemSlot; }
	equipped_slot_t		GetLastValidItemSlot() const						{ return m_unLastValidItemSlot; }
	bool				IsValidItemSlot( equipped_slot_t unSlot )			{ return unSlot >= m_unFirstValidItemSlot && unSlot <= m_unLastValidItemSlot; }
	uint32				GetNumAllowedItemPresets() const					{ return m_unNumItemPresets; }
	bool				IsValidPreset( equipped_preset_t unPreset ) const	{ return unPreset <= m_unNumItemPresets; }
	uint32				GetMinLevel() const									{ return m_unMinLevel; }
	uint32				GetMaxLevel() const									{ return m_unMaxLevel; }
	uint32				GetSumQualityWeights() const						{ return m_unSumQualityWeights; }

	// Accessors to the underlying sections
	typedef CUtlHashMapLarge<int, CEconItemDefinition*>	ItemDefinitionMap_t;
	const ItemDefinitionMap_t &GetItemDefinitionMap() const { return m_mapItems; }

	typedef CUtlMap<int, CEconItemDefinition*, int, CDefLess<int> >	SortedItemDefinitionMap_t;
	const SortedItemDefinitionMap_t &GetSortedItemDefinitionMap() const { return m_mapItemsSorted; }

	typedef CUtlDict<CEconLootListDefinition> LootListDefinitionDict_t;
	const LootListDefinitionDict_t &GetLootLists() const { return m_dictLootLists; }

	typedef CUtlMap<int, const char*, int, CDefLess<int> > RevolvingLootListDefinitionMap_t;
	const RevolvingLootListDefinitionMap_t  &GetRevolvingLootLists() const { return m_mapRevolvingLootLists; }
	const RevolvingLootListDefinitionMap_t  &GetQuestRewardLootLists() const { return m_mapQuestRewardLootLists; }

	typedef CUtlMap<const char*, int> BodygroupStateMap_t;
	const BodygroupStateMap_t  &GetDefaultBodygroupStateMap() const { return m_mapDefaultBodygroupState; }

	typedef CUtlVector<CEconColorDefinition *>	ColorDefinitionsList_t;

	typedef CUtlMap<int, CEconMusicDefinition*, int, CDefLess<int> >	MusicDefinitionsList_t;
	typedef CUtlMap<int, CEconQuestDefinition*, int, CDefLess<int> >	QuestDefinitionsList_t;
	typedef CUtlMap<int, CEconCampaignDefinition*, int, CDefLess<int> >	CampaignDefinitionsList_t;


	const MusicDefinitionsList_t &GetMusicDefinitionMap() const { return m_mapMusicDefs; }

	const CEconItemDefinition *GetDefaultItemDefinition() const { return m_pDefaultItemDefinition; }
	IEconItemDefinition *GetDefaultItemDefinitionInterface() { return m_pDefaultItemDefinition; }

	const CUtlMap<int, CEconItemQualityDefinition, int, CDefLess<int> > &GetQualityDefinitionMap() const { return m_mapQualities; }

	typedef CUtlVector< CEconItemAttributeDefinition* > EconAttrDefsContainer_t;
	EconAttrDefsContainer_t &GetAttributeDefinitionContainer() { return m_mapAttributesContainer; }
	const EconAttrDefsContainer_t &GetAttributeDefinitionContainer() const { return m_mapAttributesContainer; }

	typedef CUtlMap<int, CEconCraftingRecipeDefinition*, int, CDefLess<int> > RecipeDefinitionMap_t;
	const RecipeDefinitionMap_t &GetRecipeDefinitionMap() const { return m_mapRecipes; }
	const CUtlMap<const char*, CEconItemSetDefinition, int > &GetItemSets() const { return m_mapItemSets; }

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	const ArmoryStringDict_t	&GetArmoryDataItemClasses() const { return m_dictArmoryItemClassesDataStrings; }
	const ArmoryStringDict_t	&GetArmoryDataItemTypes() const { return m_dictArmoryItemTypesDataStrings; }
	const ArmoryStringDict_t	&GetArmoryDataItems() const { return m_dictArmoryItemDataStrings; }
	const ArmoryStringDict_t	&GetArmoryDataAttributes() const { return m_dictArmoryAttributeDataStrings; }
#endif

	const CTimedItemRewardDefinition* GetTimedReward( eTimedRewardType type ) const;

	const CEconLootListDefinition* GetLootListByName( const char* pListName, int *out_piIndex = NULL ) const;
	const CEconLootListDefinition* GetLootListByIndex( int iIdx ) const { return m_dictLootLists.IsValidIndex(iIdx) ? &(m_dictLootLists[iIdx]) : NULL; }

	virtual void PurgeLootLists( void ) { m_dictLootLists.Purge(); }
	virtual const IEconLootListDefinition* GetLootListInterfaceByName( const char* pListName, int *out_piIndex = NULL ) { return GetLootListByName( pListName, out_piIndex ); }
	virtual const IEconLootListDefinition* GetLootListInterfaceByIndex( int iIdx ) const { return GetLootListByIndex( iIdx ); }
	virtual const int GetLootListIndex( const char* pListName ) {  return m_dictLootLists.Find( pListName ); }
	virtual const int GetLootListInterfaceCount( void ) const { return m_dictLootLists.Count(); }

	const CEconItemSetDefinition* GetItemSet( const char* pSetName, int *piIndex = NULL ) const;
	const CEconItemSetDefinition* GetItemSetByIndex( int iIdx ) const { return m_mapItemSets.IsValidIndex(iIdx) ? &(m_mapItemSets[iIdx]) : NULL; }

	uint8 GetDefaultQuality() const { return AE_UNIQUE; }

	virtual int GetItemSetCount( void ) const { return m_mapItemSets.Count(); }
	virtual const IEconItemSetDefinition* GetItemSet( int iIndex ) const;

	void AssignDefaultBodygroupState( const char *pszBodygroupName, int iValue );

	equip_region_mask_t GetEquipRegionMaskByName( const char *pRegionName ) const;

	struct EquipRegion
	{
		CUtlConstString		m_sName;
		unsigned int		m_unBitIndex;		// which bit are we claiming ownership over? there might be multiple equip regions with the same bit if we're in a "shared" block
		equip_region_mask_t m_unMask;			// full region conflict mask
	};

	typedef CUtlVector<EquipRegion>		EquipRegionsList_t;
	const EquipRegionsList_t& GetEquipRegionsList() const { return m_vecEquipRegionsList; }

	virtual KeyValues *FindDefinitionPrefabByName( const char *pszPrefabName ) const;

	equip_region_mask_t GetEquipRegionBitMaskByName( const char *pRegionName ) const;

	CUtlVector< CEconItemDefinition * > &GetBundles() { return m_vecBundles; }	// Retrieve a cached list of all bundles

private:
	void SetEquipRegionConflict( int iRegion, unsigned int unBit );
	int GetEquipRegionIndexByName( const char *pRegionName ) const;

public:
	// Common lookup methods
	bool BGetItemQualityFromName( const char *pchName, uint8 *nQuality ) const;
	const CEconItemQualityDefinition *GetQualityDefinition( int nQuality ) const;
	const CEconItemQualityDefinition *GetQualityDefinitionByName( const char *pszDefName ) const;
	virtual int GetQualityDefinitionCount( void ) { return m_mapQualities.Count(); }
	virtual const char* GetQualityName( uint8 iQuality );
	virtual int GetQualityIndex( const char* pszQuality );
	bool BGetItemRarityFromName( const char* pchName, uint8 *nRarity ) const;
	const CEconItemRarityDefinition *GetRarityDefinitionByMapIndex( int nRarityIndex ) const;
	const CEconItemRarityDefinition *GetRarityDefinition( int nRarity ) const;
	const CEconItemRarityDefinition *GetRarityDefinitionByName( const char *pszDefName ) const;
	virtual int GetRarityDefinitionCount( void ) const { return m_mapRarities.Count(); }
	virtual const char* GetRarityName( uint8 iRarity );
	virtual const char* GetRarityLocKey( uint8 iRarity );
	virtual const char* GetRarityColor( uint8 iRarity );
	virtual const char* GetRarityLootList( uint8 iRarity );
	virtual int GetRarityIndex( const char* pszRarity );
	CEconItemDefinition *GetItemDefinitionMutable( int iItemIndex, bool bNoDefault=false );
	const CEconItemDefinition *GetItemDefinition( int iItemIndex, bool bNoDefault=false ) const;
	const CEconItemDefinition *GetItemDefinitionByMapIndex( int iMapIndex ) const;
	const CEconItemAttributeDefinition *GetAttributeDefinition( int iAttribIndex ) const;
	virtual const IEconItemAttributeDefinition *GetAttributeDefinitionInterface( int iAttribIndex ) const { return GetAttributeDefinition( iAttribIndex ); }
	const CEconItemAttributeDefinition *GetAttributeDefinitionByName( const char *pszDefName ) const;
	const CEconCraftingRecipeDefinition *GetRecipeDefinition( int iRecipeIndex ) const;
	int GetPaintKitDefinitionCount( void ) const { return m_mapPaintKits.Count(); }
	void AddPaintKitDefinition( int iPaintKitID, CPaintKit *pPaintKit );
	void RemovePaintKitDefinition( int iPaintKitID );
	const unsigned int GetPaintKitCount() const;
	const CPaintKit *GetPaintKitDefinition( int iPaintKitID ) const;
	const CPaintKit *GetPaintKitDefinitionByMapIndex( int iMapIndex );
	const CPaintKit *GetPaintKitDefinitionByName( const char *pchName ) const;

	int GetStickerKitDefinitionCount( void ) const { return m_mapStickerKits.Count(); }
	void AddStickerKitDefinition( int iStickerKitID, CStickerKit *pStickerKit );
	void RemoveStickerKitDefinition( int iStickerKitID );
	const CStickerKit *GetStickerKitDefinition( int iStickerKitID ) const;
	const CStickerKit *GetStickerKitDefinitionByMapIndex( int iMapIndex );
	const CStickerKit *GetStickerKitDefinitionByName( const char *pchName ) const;
	const CStickerList *GetStickerListDefinitionByName( const char *pchName ) const;
	const CEconMusicDefinition *GetMusicKitDefinitionByName( const char *pchName ) const;

	const CEconColorDefinition *GetColorDefinitionByName( const char *pszDefName ) const;

	int GetGraffitiTintMaxValidDefID() const { return m_nMaxValidGraffitiTintDefID; }
	const CEconGraffitiTintDefinition *GetGraffitiTintDefinitionByID( int nID ) const;
	const CEconGraffitiTintDefinition *GetGraffitiTintDefinitionByName( const char *pszDefName ) const;

	const CEconMusicDefinition *GetMusicDefinition( uint32 unMusicID ) const;

	CEconQuestDefinition *GetQuestDefinition( uint32 unQuestID ) const;
	const QuestDefinitionsList_t& GetQuestDefinitionMap( void ) const { return m_mapQuestDefs; }

	const QuestEventsSchedule_t& GetAndUpdateQuestEventsSchedule( void );
	const QuestEvents_t&		GetQuestEvents( void ) const { return m_vecQuestEvents; }

	CEconCampaignDefinition *GetCampaignDefinition( uint32 unCampaignID ) const;
	const CampaignDefinitionsList_t& GetCampaignDefinitionMap( void ) const { return m_mapCampaignDefs; }

	virtual int GetToolType( const char *pszToolType ) const { return -1; }
	virtual int GetNumPrefabs( void ) { return m_mapDefinitionPrefabs.Count(); }
	virtual const char* GetPrefabName( int idx ) { return m_mapDefinitionPrefabs.Key( idx ); }

	const CEconSoundMaterialDefinition *GetSoundMaterialDefinitionByID( int nSoundMaterialID ) const;
	const CEconSoundMaterialDefinition *GetSoundMaterialDefinitionByName( const char *pszSoundMaterialName ) const;
	virtual const char* GetSoundMaterialNameByID( int nSoundMaterialID );
	virtual int GetSoundMaterialID( const char* pszSoundMaterial );

	// iterating sound materials
	virtual int GetSoundMaterialCount( void ) { return m_mapSoundMaterials.Count(); }
	virtual int GetSoundMaterialIDByIndex( int nIndex );

#ifdef CLIENT_DLL
	// Web Resources
	EWebResourceStatus LoadWebResource( CUtlString pszName, void (*fnCallback)( const char*, KeyValues* ), bool bForceReload=false );
	void SetWebResource( CUtlString strName, KeyValues* pResourceKV );
#endif

	// Pro players data
	const CProPlayerData * GetProPlayerDataByAccountID( uint32 unAccountID ) const;
	const CUtlVector< const CProPlayerData * > * GetProPlayersDataForEventIDTeamID( int nEventID, int nTeamID ) const;

	virtual uint32 GetHeroID( const char* pszHeroName ) { return 0; }
	
	bool BCanGSCreateItems( uint32 unIP ) const;
	const AchievementAward_t *GetAchievementRewardByDefIndex( uint16 usDefIndex ) const;
	bool BHasAchievementRewards( void ) const { return (m_dictAchievementRewards.Count() > 0); }

	static CUtlString ComputeAchievementName( AppId_t unAppID, const char *pchNativeAchievementName );

	// Iterating over the item definitions. Game needs this to precache data.
	CEconItemDefinition *GetItemDefinitionByName( const char *pszDefName );
	const CEconItemDefinition *GetItemDefinitionByName( const char *pszDefName ) const;

	attachedparticlesystem_t* GetAttributeControlledParticleSystem( int id );
	attachedparticlesystem_t* GetAttributeControlledParticleSystemByIndex( int id );
	attachedparticlesystem_t* FindAttributeControlledParticleSystem( const char *pchSystemName, int *outID = NULL );
	typedef CUtlMap<int, attachedparticlesystem_t, int, CDefLess<int> > ParticleDefinitionMap_t;
	const ParticleDefinitionMap_t& GetAttributeControlledParticleSystems() const { return m_mapAttributeControlledParticleSystems; }
	virtual int GetNumAttributeControlledParticleSystems() const { return GetAttributeControlledParticleSystems().Count(); }

	// Only intended to be used for generating data for the WebAPI.
	const KillEaterScoreMap_t& GetKillEaterScoreTypes() const { return m_mapKillEaterScoreTypes; }

	// Note: this returns pointers to the inside of a vector and/or NULL. Pointers are not intended to be
	// saved off and used later.
	const kill_eater_score_type_t *FindKillEaterScoreType( uint32 unScoreType ) const;

	const CUtlVector<attr_type_t>& GetAttributeTypes() const { return m_vecAttributeTypes; }
	const ISchemaAttributeType *GetAttributeType( const char *pszAttrTypeName ) const;

	const LevelBlockDict_t&	GetItemLevelingDataDict() const { return m_vecItemLevelingData; }

	const CUtlVector<CItemLevelingDefinition> *GetItemLevelingData( const char *pszLevelBlockName ) const
	{
		LevelBlockDict_t::IndexType_t i = m_vecItemLevelingData.Find( pszLevelBlockName );
		if ( i == LevelBlockDict_t::InvalidIndex() )
			return NULL;

		return m_vecItemLevelingData[i];
	}

	const CItemLevelingDefinition *GetItemLevelForScore( const char *pszLevelBlockName, uint32 unScore ) const;
	const char *GetKillEaterScoreTypeLocString( uint32 unScoreType ) const;
	bool GetKillEaterScoreTypeUseLevelData( uint32 unScoreType ) const;
	const char *GetKillEaterScoreTypeLevelingDataName( uint32 unScoreType ) const;
#if defined(CLIENT_DLL) || defined(GAME_DLL)
	virtual void ItemTesting_CreateTestDefinition( int iCloneFromItemDef, int iNewDef, KeyValues *pNewKV );
	virtual void ItemTesting_DiscardTestDefinition( int iDef );
#endif

	econ_tag_handle_t GetHandleForTag( const char *pszTagName );			// non-const because it may create a new tag handle

	typedef CUtlDict<econ_tag_handle_t> EconTagDict_t;

public:
	// Subclass interface.
	virtual CEconItemDefinition				*CreateEconItemDefinition()			{ return new CEconItemDefinition; }
	virtual CEconCraftingRecipeDefinition	*CreateCraftingRecipeDefinition()	{ return new CEconCraftingRecipeDefinition; }
	virtual CEconStyleInfo					*CreateEconStyleInfo()				{ return new CEconStyleInfo; }

	virtual IEconTool						*CreateEconToolImpl( const char *pszToolType, const char *pszUseString, const char *pszUsageRestriction, item_capabilities_t unCapabilities, KeyValues *pUsageKV );
	virtual int GetItemDefinitionCount() { return m_mapItems.Count(); }

	AlternateIconData_t *GetAlternateIcon( uint64 ullAlternateIcon );

	typedef CUtlMap< uint64, AlternateIconData_t, int, CDefLess<uint64> >	AlternateIconsMap_t;
	AlternateIconsMap_t &GetAlternateIconsMap() { return m_mapAlternateIcons; }


protected:
	virtual void Reset( void );

	// IEconItemSchema
	// This interface isn't fully generic. Expand as needed.
	virtual KeyValues*	GetRawDefinition( void ) const { return m_pKVRawDefinition; }
	virtual IEconItemDefinition* GetItemDefinitionInterface( int iDefIndex ) { return GetItemDefinitionMutable( iDefIndex, true ); }
	virtual int GetLoadoutSlotCount() { return 0; }
	virtual const char* GetLoadoutSlotName( int iSlot ) { return NULL; }
	virtual int GetLoadoutSlot( const char* pszSlotName ) { return 0; }
	virtual int GetCharacterSlotType( int iCharacter, int iSlot ) { return 0; }
	virtual int GetCharacterID( const char* pszCharacterName ) { return 0; }
	virtual bool DeleteItemDefinition( int iDefIndex );
	virtual int GetCharacterCount( void ) { return 0; }
	virtual const char* GetCharacterName( int iCharacter ) { return NULL; }
	virtual const char* GetCharacterModel( int iCharacter ) { return NULL; }

	virtual int CalculateNumberOfConcreteItems( const CEconItemDefinition *pItemDef );	// Let derived classes handle custom item types

	virtual bool BInitItems( KeyValues *pKVItems, CUtlVector<CUtlString> *pVecErrors );
	virtual bool BInitItemMappings( CUtlVector<CUtlString> *pVecErrors );
	virtual bool BInitBundles( CUtlVector<CUtlString> *pVecErrors );
	virtual bool BInitPaymentRules( CUtlVector<CUtlString> *pVecErrors );
	virtual bool BInitItemSets( KeyValues *pKVItemSets, CUtlVector<CUtlString> *pVecErrors );

private:
	bool BInitGameInfo( KeyValues *pKVGameInfo, CUtlVector<CUtlString> *pVecErrors );
	bool BInitAttributeTypes( CUtlVector<CUtlString> *pVecErrors );
	bool BInitDefinitionPrefabs( KeyValues *pKVPrefabs, CUtlVector<CUtlString> *pVecErrors );
	bool BInitRarities( KeyValues *pKVRarities, CUtlVector<CUtlString> *pVecErrors );
	bool BInitQualities( KeyValues *pKVAttributes, CUtlVector<CUtlString> *pVecErrors );
	bool BInitColors( KeyValues *pKVColors, CUtlVector<CUtlString> *pVecErrors );
	bool BInitGraffitiTints( KeyValues *pKVColors, CUtlVector<CUtlString> *pVecErrors );
	bool BInitAttributes( KeyValues *pKVAttributes, CUtlVector<CUtlString> *pVecErrors );
	bool BInitEquipRegions( KeyValues *pKVEquipRegions, CUtlVector<CUtlString> *pVecErrors );
	bool BInitEquipRegionConflicts( KeyValues *pKVEquipRegions, CUtlVector<CUtlString> *pVecErrors );
	bool BInitTimedRewards( KeyValues *pKVTimeRewards, CUtlVector<CUtlString> *pVecErrors );
	bool BInitAchievementRewards( KeyValues *pKVTimeRewards, CUtlVector<CUtlString> *pVecErrors );
	bool BInitRecipes( KeyValues *pKVRecipes, CUtlVector<CUtlString> *pVecErrors );
	virtual bool BInitLootLists( KeyValues *pKVLootLists, KeyValues *pKVRandomAttributeTemplates, CUtlVector<CUtlString> *pVecErrors, bool bServerLists );
	bool BInitRevolvingLootLists( KeyValues *pKVRevolvingLootLists, CUtlVector<CUtlString> *pVecErrors );
	bool BInitQuestRewardLootLists( KeyValues *pKVQuestRewardLootLists, CUtlVector<CUtlString> *pVecErrors );
#ifdef TF_CLIENT_DLL
	bool BInitConcreteItemCounts( CUtlVector<CUtlString> *pVecErrors );
#endif
	bool BInitItemLevels( KeyValues *pKVItemLevels, CUtlVector<CUtlString> *pVecErrors );
	bool BInitKillEaterScoreTypes( KeyValues *pKVItemLevels, CUtlVector<CUtlString> *pVecErrors );
	bool BInitAlternateIcons( KeyValues *pKVAlternateIcons, CUtlVector<CUtlString> *pVecErrors );
	bool BInitAlternateIcon( uint64 ullAltIconKey, char const *szSimpleName, KeyValues *pKVAlternateIcon, CUtlVector<CUtlString> *pVecErrors );
	bool BInitStickerKits( KeyValues *pKVStickerKits, CUtlVector<CUtlString> *pVecErrors );
	bool BInitSoundMaterials( KeyValues *pKVSoundMaterials, CUtlVector<CUtlString> *pVecErrors );
	bool BInitStickerLists( KeyValues *pKVStickerKits, CUtlVector<CUtlString> *pVecErrors );

	bool BInitPaintKits( KeyValues *pKVPaintKits, CUtlVector<CUtlString> *pVecErrors );
	bool BInitPaintKitsRarity( KeyValues *pKVPaintKitsRarity, CUtlVector<CUtlString> *pVecErrors );

	bool BInitMusicDefs( KeyValues *pKVMusicDefs, CUtlVector<CUtlString> *pVecErrors );

	bool BInitQuestDefs( KeyValues *pKVQuestDefs, CUtlVector<CUtlString> *pVecErrors );

	bool BInitQuestEvents( KeyValues *pKVQuestEvents, CUtlVector<CUtlString> *pVecErrors );

	bool BInitCampaignDefs( KeyValues *pKVCampaignDefs, CUtlVector<CUtlString> *pVecErrors );

	bool BInitProPlayers( KeyValues *pKVData, CUtlVector<CUtlString> *pVecErrors );

#ifdef CLIENT_DLL
	bool BInitWebResources( KeyValues *pKVWebResources, CUtlVector<CUtlString> *pVecErrors );
#endif

	bool BPostSchemaInitStartupChecks( CUtlVector<CUtlString> *pVecErrors );

	virtual attachedparticlesystem_t GetAttachedParticleSystemInfo( KeyValues* pParticleSystemKV, int32 nItemIndex ) const;
	virtual bool BInitAttributeControlledParticleSystems( KeyValues *pKVParticleSystems, CUtlVector<CUtlString> *pVecErrors );
#if defined(CLIENT_DLL) || defined(GAME_DLL)
	bool BInitArmoryData( KeyValues *pKVArmoryData, CUtlVector<CUtlString> *pVecErrors );
#else
	bool BInitExperiements( KeyValues *pKVExperiments, CUtlVector<CUtlString> *pVecErrors );
	bool BInitForeignImports( CUtlVector<CUtlString> *pVecErrors );

	CForeignAppImports *FindOrAddAppImports( AppId_t unAppID );
#endif

protected:
	virtual bool BInitSchema( KeyValues *pKVRawDefinition, CUtlVector<CUtlString> *pVecErrors = NULL );
private:
	bool			m_bSchemaUpdatesEnabled;

	uint32			m_unResetCount;

	KeyValues		*m_pKVRawDefinition;
	uint32			m_unVersion;

	// Class range
	equipped_class_t	m_unFirstValidClass;
	equipped_class_t	m_unLastValidClass;
	
	// Item slot range
	equipped_slot_t		m_unFirstValidItemSlot;
	equipped_slot_t		m_unLastValidItemSlot;

	// Number of allowed presets
	uint32			m_unNumItemPresets;

	// Allowable range of item levels for this app
	uint32			m_unMinLevel;
	uint32			m_unMaxLevel;

	// Total value of all the weights of the qualities
	uint32			m_unSumQualityWeights;

	// Name-to-implementation list of all unique attribute types (ie., "wide strange score").
	CUtlVector<attr_type_t>								m_vecAttributeTypes;

	// Contains the list of rarity definitions
	CUtlMap<int, CEconItemRarityDefinition, int, CDefLess<int> >		m_mapRarities;

	// Contains the list of quality definitions read in from all data files.
	CUtlMap<int, CEconItemQualityDefinition, int, CDefLess<int> >		m_mapQualities;

	// Contains the list of item definitions read in from all data files.
	ItemDefinitionMap_t									m_mapItems;

	// A sorted version of the same map, for instances where we really want sorted data
	SortedItemDefinitionMap_t							m_mapItemsSorted;

	// What is the default item definition we'll return in the client code if we can't find the correct one?
	CEconItemDefinition								   *m_pDefaultItemDefinition;

	// Contains the list of attribute definitions read in from all data files.
	EconAttrDefsContainer_t								m_mapAttributesContainer;

	// Contains the list of item recipes read in from all data files.
	RecipeDefinitionMap_t								m_mapRecipes;

	// Contains the list of item sets.
	CUtlMap<const char*, CEconItemSetDefinition, int >	m_mapItemSets;

	// Revolving loot lists.
	CUtlMap<int, const char*, int, CDefLess<int> >	m_mapRevolvingLootLists;

	// Revolving loot lists.
	CUtlMap<int, const char*, int, CDefLess<int> >	m_mapQuestRewardLootLists;

	// Contains the list of loot lists.
	LootListDefinitionDict_t							m_dictLootLists;

	// List of events that award items based on time played
	CUtlVector<CTimedItemRewardDefinition>				m_vecTimedRewards;

	// List of web resources.
	CUtlDict<CWebResource*, int>						m_dictWebResources;

	// List of alternate icons.
	AlternateIconsMap_t	m_mapAlternateIcons;

	// list of items that will be awarded from achievements
	CUtlDict< AchievementAward_t *, int >				m_dictAchievementRewards;
	CUtlMap< uint32, AchievementAward_t *, int, CDefLess<uint32> >				m_mapAchievementRewardsByData;

	// Contains the map of paint kits
	CUtlMap<int, CPaintKit*, int, CDefLess<int> >						m_mapPaintKits;

	// Contains the map of sticker kits
	CUtlMap<int, CStickerKit*, int, CDefLess<int> >					m_mapStickerKits;
	CUtlDict< CStickerKit *, int >						m_dictStickerKits;

	// Contains the map of sticker lists
	CUtlDict< CStickerList *, int >						m_dictStickerLists;

	// Contains information for attribute attached particle systems
	CUtlMap<int, attachedparticlesystem_t, int, CDefLess<int> >				m_mapAttributeControlledParticleSystems;

	// Contains information on which equip regions conflict with each other regions and how to
	// test for overlap.
	EquipRegionsList_t									m_vecEquipRegionsList;

	// Contains information about prefab KeyValues blocks that be can referenced elsewhere
	// in the schema.
	CUtlMap<const char *, KeyValues *, int>			m_mapDefinitionPrefabs;

	// Contains runtime color information, looked-up by name.
	ColorDefinitionsList_t								m_vecColorDefs;
	CUtlVector< CEconGraffitiTintDefinition * >			m_vecGraffitiTintDefs;
	CUtlStringMap< CEconGraffitiTintDefinition * >		m_mapGraffitiTintByName;
	int													m_nMaxValidGraffitiTintDefID;

	// Contains runtime music definitions for music kits.
	MusicDefinitionsList_t								m_mapMusicDefs;

	QuestDefinitionsList_t								m_mapQuestDefs;

	CampaignDefinitionsList_t							m_mapCampaignDefs;

	QuestEvents_t										m_vecQuestEvents;
	QuestEventsSchedule_t								m_mapQuestEventsSchedule;

	typedef CUtlMap< uint32, CProPlayerData *, int, CDefLess< uint32 > > MapProPlayersByAccountID_t;
	typedef CUtlStringMap< CProPlayerData * > MapProPlayersByName_t;
	typedef CUtlMap< uint64, CUtlVector< const CProPlayerData * > *, int, CDefLess< uint64 > > MapProPlayersByEventIDTeamID_t;
	MapProPlayersByAccountID_t m_mapProPlayersByAccountID;
	MapProPlayersByName_t m_mapProPlayersByCode;
	MapProPlayersByEventIDTeamID_t m_mapProPlayersByEventIDTeamID;

	// Contains the list of sound material definitions
	CUtlMap<int, CEconSoundMaterialDefinition, int, CDefLess<int> >	m_mapSoundMaterials;

	// Contains information about: a) every bodygroup that appears anywhere in the schema, and
	// b) whether they default to on or off.
	BodygroupStateMap_t									m_mapDefaultBodygroupState;

	// Various definitions can have any number of unique tags associated with them.
	EconTagDict_t										m_dictTags;

	// List of item leveling data.
	KillEaterScoreMap_t									m_mapKillEaterScoreTypes;

	LevelBlockDict_t m_vecItemLevelingData;

#if defined(CLIENT_DLL) || defined(GAME_DLL)
	// Contains Armory data key->localization string mappings
	ArmoryStringDict_t m_dictArmoryItemTypesDataStrings;
	ArmoryStringDict_t m_dictArmoryItemClassesDataStrings;
	ArmoryStringDict_t m_dictArmoryAttributeDataStrings;
	ArmoryStringDict_t m_dictArmoryItemDataStrings;

	// Used for delaying the parsing of the item schema until its safe to swap out the back end data.
	IDelayedSchemaData *m_pDelayedSchemaData;
#endif

	CUtlVector< CEconItemDefinition * > m_vecBundles;	// A cached list of all bundles

	static CUniformRandomStream m_RandomStream; // Random stream for item generation.

	bool m_bSchemaParsingItems;
};

extern CEconItemSchema & GEconItemSchema();

//-----------------------------------------------------------------------------
// CSchemaFieldHandle
//-----------------------------------------------------------------------------
template < class T >
class CSchemaFieldHandle
{
public:
	explicit CSchemaFieldHandle( const char *szName )
		: m_szName( szName )
	{
		m_pRef = GetTypedRef();
		m_unSchemaGeneration = GEconItemSchema().GetResetCount();
#if _DEBUG
		m_unVersion_Debug = GEconItemSchema().GetVersion();
#endif
	}

	operator const T *( void ) const
	{
		uint32 unSchemaGeneration = GEconItemSchema().GetResetCount();
		if ( m_unSchemaGeneration != unSchemaGeneration )
		{
			m_pRef = GetTypedRef();
			m_unSchemaGeneration = unSchemaGeneration;
#if _DEBUG
			m_unVersion_Debug = GEconItemSchema().GetVersion();
#endif
		}

#if _DEBUG
		Assert( m_unVersion_Debug == GEconItemSchema().GetVersion() );
#endif
		return m_pRef;
	}

	const T *operator->( void ) const
	{
		return static_cast<const T *>( *this );
	}

	const char *GetName( void ) const
	{
		return m_szName;
	}

private:
	const T *GetTypedRef() const;

private:
	const char *m_szName;

	mutable const T *m_pRef;
	mutable uint32 m_unSchemaGeneration;
#if _DEBUG
	mutable uint32 m_unVersion_Debug;
#endif
};

template < >
inline const CEconColorDefinition *CSchemaFieldHandle<CEconColorDefinition>::GetTypedRef( void ) const
{
	return GEconItemSchema().GetColorDefinitionByName( m_szName );
}

template < >
inline const CEconItemAttributeDefinition *CSchemaFieldHandle<CEconItemAttributeDefinition>::GetTypedRef( void ) const
{
	return GEconItemSchema().GetAttributeDefinitionByName( m_szName );
}

template < >
inline const CEconItemDefinition *CSchemaFieldHandle<CEconItemDefinition>::GetTypedRef( void ) const
{
	return GEconItemSchema().GetItemDefinitionByName( m_szName );
}

template < >
inline const CEconLootListDefinition *CSchemaFieldHandle<CEconLootListDefinition>::GetTypedRef( void ) const
{
	return GEconItemSchema().GetLootListByName( m_szName );
}

typedef CSchemaFieldHandle<CEconColorDefinition>			CSchemaColorDefHandle;
typedef CSchemaFieldHandle<CEconMusicDefinition>			CSchemaMusicDefHandle;
typedef CSchemaFieldHandle<CEconItemAttributeDefinition>	CSchemaAttributeDefHandle;
typedef CSchemaFieldHandle<CEconItemDefinition>				CSchemaItemDefHandle;
typedef CSchemaFieldHandle<CEconLootListDefinition>			CSchemaLootListDefHandle;

// Implementation reliant on earlier class content.
inline const CEconItemAttributeDefinition *static_attrib_t::GetAttributeDefinition() const
{
	return GEconItemSchema().GetAttributeDefinition( iDefIndex );
}

// Utility function to convert datafile strings to ints.
int StringFieldToInt( const char *szValue, const char **pValueStrings, int iNumStrings, bool bDontAssert = false );
int StringFieldToInt( const char *szValue, const CUtlVector<const char *>& vecValueStrings, bool bDontAssert = false );

// Helper to get a sticker attribute at a slot
// while only doing the schema lookup once. 
enum EStickerAttributeType
{
	k_EStickerAttribute_ID,
	k_EStickerAttribute_Wear,
	k_EStickerAttribute_Scale,
	k_EStickerAttribute_Rotation,
	k_EStickerAttribute_Count,
};
const int g_nNumStickerAttrs = 6;
const CSchemaAttributeDefHandle& GetStickerAttributeDefHandle( int attrNum, EStickerAttributeType type );

// Helper to get a specific campaign's attribute
// while only doing the schema lookup once. 
enum ECampaignAttributeType
{
	k_ECampaignAttribute_CompletionBitfield,
	k_ECampaignAttribute_LastCompletedQuest,

};
const int g_nNumCampaigns = 8;	// 1-based! first campaign is 1, last campaign is g_nNumCampaigns
const CSchemaAttributeDefHandle& GetCampaignAttributeDefHandle( int nCampaignID, ECampaignAttributeType type );

// Share some code between schema initialization checks and the client-only icon building process so we can warn about missing icons
extern const uint32 g_unNumWearBuckets;
uint64 Helper_GetAlternateIconKeyForWeaponPaintWearItem( item_definition_index_t nDefIdx, uint32 nPaintId, uint32 nWear );

uint64 Helper_GetAlternateIconKeyForTintedStickerItem( uint32 nStickerKitID, uint32 unTintID );


#endif //ECONITEMSCHEMA_H
