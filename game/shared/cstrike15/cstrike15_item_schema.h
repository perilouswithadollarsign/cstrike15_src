//========= Copyright (c), Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#ifndef CSTRIKE15_ITEM_SCHEMA_H
#define CSTRIKE15_ITEM_SCHEMA_H
#ifdef _WIN32
#pragma once
#endif

#include "econ_item_schema.h"
#include "cstrike15_item_constants.h"


class CCStrike15ItemDefinition : public CEconItemDefinition
{
public:

	// Constructor
	CCStrike15ItemDefinition();
	
	// CCStrike15ItemDefinition interface.
	virtual bool	BInitFromKV( KeyValues *pKVItem, CEconItemSchema &schemaa, CUtlVector<CUtlString> *pVecErrors = NULL );
#ifndef GC_DLL
	virtual bool	BInitFromTestItemKVs( int iNewDefIndex, KeyValues *pKVItem, CEconItemSchema &schemaa );
#endif


	virtual void	CopyPolymorphic( const CEconItemDefinition *pSourceDef );
#ifndef GC_DLL
	virtual void	GeneratePrecacheModelStrings( bool bDynamicLoad, CUtlVector<const char *> *out_pVecModelStrings );
#endif // !GC_DLL

	int			GetAnimSlot( void ) const			{ return m_iAnimationSlot; }

	// Class & Slot handling
	int			GetDefaultLoadoutSlot( void ) const { return m_iDefaultLoadoutSlot; }
	const CBitVec<LOADOUT_COUNT> *GetClassUsability( void ) const { return &m_vbClassUsability; }
	void		FilloutSlotUsage( CBitVec<LOADOUT_COUNT> *pBV ) const;
	bool		CanBeUsedByTeam( int iTeam ) const { return m_vbClassUsability.IsBitSet( iTeam ); }
	int			GetUsedByTeam( void ) const;
	bool		CanBeUsedByAllTeams( void ) const;
	
	bool		IsSupplyCrate( void ) const { return m_bIsSupplyCrate; } 

	// Items that share a slot (m4, cz/p250) have special rules
	bool		SharesSlot( void ) const { return m_bItemSharesEquipSlot; }

		
	bool		CanBePlacedInSlot( int nSlot ) const;
	const char	*GetPlayerDisplayModel( int iTeam ) const	{ Assert( iTeam >= 0 && iTeam < LOADOUT_COUNT ); return m_pszPlayerDisplayModel[iTeam]; }

	int			GetLoadoutSlot( int iLoadoutClass ) const;
#ifndef GC_DLL
	bool		IsAWearable( int iSlot ) const;
	bool		IsContentStreamable() const;
#endif // !GC_DLL

private:
	// The load-out slot that this item can be placed into.
	int				m_iDefaultLoadoutSlot;
	int				m_iAnimationSlot;

	// The .mdl file used for this item when it's being carried by a player.
	const char		*m_pszPlayerDisplayModel[LOADOUT_COUNT];

	// Specifies which class can use this item.
	CBitVec<LOADOUT_COUNT> m_vbClassUsability;
	int				m_iLoadoutSlots[LOADOUT_COUNT];		// Slot that each class places the item into.
	bool m_bIsSupplyCrate : 1;
	bool m_bItemSharesEquipSlot : 1;
};


class CCStrike15ItemSchema : public CEconItemSchema
{
public:
	CCStrike15ItemSchema();

	CCStrike15ItemDefinition *GetTFItemDefinition( int iItemIndex )
	{
		return (CCStrike15ItemDefinition *)GetItemDefinition( iItemIndex );
	}

	const CUtlVector<const char *>& GetClassUsabilityStrings() const { return m_vecClassUsabilityStrings; }
	const CUtlVector<const char *>& GetLoadoutStrings() const { return m_vecLoadoutStrings; }
	const CUtlVector<const char *>& GetLoadoutStringsSubPositions() const { return m_vecLoadoutStringsSubPositions; }
	const CUtlVector<const char *>& GetLoadoutStringsForDisplay() const { return m_vecLoadoutStringsForDisplay; }
	const CUtlVector<const char *>& GetWeaponTypeSubstrings() const { return m_vecWeaponTypeSubstrings; }

	static const char k_rchCommunitySupportPassItemDefName[];

public:
	// CEconItemSchema interface.
	virtual CEconItemDefinition *CreateEconItemDefinition() { return new CCStrike15ItemDefinition; }

	virtual bool BInitSchema( KeyValues *pKVRawDefinition, CUtlVector<CUtlString> *pVecErrors = NULL );

private:
	void InitializeStringTable( const char **ppStringTable, unsigned int unStringCount, CUtlVector<const char *> *out_pvecStringTable );

	CUtlVector<const char *> m_vecClassUsabilityStrings;
	CUtlVector<const char *> m_vecLoadoutStrings;
	CUtlVector<const char *> m_vecLoadoutStringsSubPositions;
	CUtlVector<const char *> m_vecLoadoutStringsForDisplay;
	CUtlVector<const char *> m_vecWeaponTypeSubstrings;
};


extern const char *g_szLoadoutStrings[ LOADOUT_POSITION_COUNT ];
extern const char *g_szLoadoutStringsForDisplay[ LOADOUT_POSITION_COUNT ];


#endif // CSTRIKE15_ITEM_SCHEMA_H