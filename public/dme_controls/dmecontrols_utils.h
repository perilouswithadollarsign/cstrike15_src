//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef DMECONTROLS_UTILS_H
#define DMECONTROLS_UTILS_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/keyvalues.h"
#include "tier1/fmtstr.h"
#include "tier1/utlbufferutil.h"
#include "tier1/utlsymbollarge.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"

#include "movieobjects/dmegamemodel.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeanimationset.h"

#include "vgui_controls/TextEntry.h"
#include "vgui_controls/InputDialog.h"
#include "vgui_controls/Menu.h"


inline int AddCheckableMenuItem( vgui::Menu *pMenu, const char *pItemName, KeyValues *pKeyValues, vgui::Panel *pTarget, bool bState, bool bEnabled = true )
{
	int id = pMenu->AddCheckableMenuItem( pItemName, pKeyValues, pTarget );
	pMenu->SetMenuItemChecked( id, bState );
	pMenu->SetItemEnabled( id, bEnabled );
	return id;
}

inline int AddCheckableMenuItem( vgui::Menu *pMenu, const char *pItemName, const char *pKVName, vgui::Panel *pTarget, bool bState, bool bEnabled = true )
{
	return AddCheckableMenuItem( pMenu, pItemName, new KeyValues( pKVName ), pTarget, bState, bEnabled );
}


//-----------------------------------------------------------------------------
// Helper method to insert + extract DmElement handles into keyvalues
//-----------------------------------------------------------------------------
inline void SetElementKeyValue( KeyValues *pKeyValues, const char *pName, CDmElement *pElement )
{
	pKeyValues->SetInt( pName, pElement ? pElement->GetHandle() : DMELEMENT_HANDLE_INVALID );
}

template< class T >
T* GetElementKeyValue( KeyValues *pKeyValues, const char *pName )
{
	DmElementHandle_t h = (DmElementHandle_t)pKeyValues->GetInt( pName, DMELEMENT_HANDLE_INVALID );
	return GetElement<T>( h );
}

inline KeyValues *CreateElementKeyValues( const char *pName, const char *pKey, CDmElement *pElement )
{
	return new KeyValues( pName, pKey, pElement ? ( int )pElement->GetHandle() : DMELEMENT_HANDLE_INVALID );
}

inline void AddStandardElementKeys( KeyValues *pKeyValues, CDmElement *pElement )
{
	SetElementKeyValue( pKeyValues, "dmeelement", pElement );

	if ( pElement )
	{
		char buf[ 256 ];
		UniqueIdToString( pElement->GetId(), buf, sizeof( buf ) );
		pKeyValues->SetString( "text", buf );
		pKeyValues->SetString( "type", pElement->GetTypeString() );
	}
}

inline void SetMatrixKeyValue( KeyValues *pKeyValues, const char *pName, const VMatrix &mat )
{
	CUtlBuffer buf;
	buf.SetBufferType( true, false );
	Serialize( buf, mat );
	pKeyValues->SetString( pName, ( char* )buf.Base() );
}

inline void GetMatrixKeyValue( KeyValues *pKeyValues, const char *pName, VMatrix &mat )
{
	const char *pMatrixString = pKeyValues->GetString( pName );
	CUtlBuffer buf( pMatrixString, V_strlen( pMatrixString ) + 1, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );
	Unserialize( buf, mat );
}

//-----------------------------------------------------------------------------
// Helper method to insert + extract DmeTime_t into keyvalues
//-----------------------------------------------------------------------------
inline void SetDmeTimeKeyValue( KeyValues *pKeyValues, const char *pName, DmeTime_t t )
{
	pKeyValues->SetInt( pName, t.GetTenthsOfMS() );
}

inline DmeTime_t GetDmeTimeKeyValue( KeyValues *pKeyValues, const char *pName, DmeTime_t defaultTime = DMETIME_ZERO )
{
	return DmeTime_t( pKeyValues->GetInt( pName, defaultTime.GetTenthsOfMS() ) );
}


inline bool ElementTree_IsArrayItem( KeyValues *itemData )
{
	return !itemData->IsEmpty( "arrayIndex" );
}

inline CDmAttribute *ElementTree_GetAttribute( KeyValues *itemData )
{
	CDmElement *pOwner = GetElementKeyValue< CDmElement >( itemData, "ownerelement" );
	if ( !pOwner )
		return NULL;

	const char *pAttributeName = itemData->GetString( "attributeName", "" );
	return pOwner->GetAttribute( pAttributeName );
}

inline DmAttributeType_t ElementTree_GetAttributeType( KeyValues *itemData )
{
	CDmElement *pOwner = GetElementKeyValue< CDmElement >( itemData, "ownerelement" );
	if ( !pOwner )
		return AT_UNKNOWN;

	const char *pAttributeName = itemData->GetString( "attributeName", "" );
	CDmAttribute *pAttribute = pOwner->GetAttribute( pAttributeName );
	if ( !pAttribute )
		return AT_UNKNOWN;

	return pAttribute->GetType();
}



inline bool ElementTree_GetDroppableItems( CUtlVector< KeyValues * >& msglist, const char *elementType, CUtlVector< CDmElement * >& list )
{
	int c = msglist.Count();
	for ( int i = 0; i < c; ++i )
	{	
		KeyValues *data = msglist[ i ];

		CDmElement *e = GetElementKeyValue<CDmElement>( data, "dmeelement" );
		if ( !e )
		{
			continue;
		}

		//if ( !e->IsA( elementType ) )
		//{
		//	continue;
		//}

		list.AddToTail( e );
	}

	return list.Count() != 0;
}

inline void ElementTree_RemoveListFromArray( CDmAttribute *pArrayAttribute, CUtlVector< CDmElement * >& list )
{
	CDmrElementArray<> array( pArrayAttribute );
	int c = array.Count();
	for ( int i = c - 1 ; i >= 0 ; --i )
	{
		CDmElement *element = array[ i ];
		if ( list.Find( element ) != list.InvalidIndex() )
		{
			array.Remove( i );
		}
	}
}


struct PresetGroupInfo_t
{
	PresetGroupInfo_t( CUtlSymbolLarge group = UTL_INVAL_SYMBOL_LARGE ) : presetGroupSym( group ) {}
	CUtlSymbolLarge presetGroupSym;
	bool bGroupShared;
	bool bGroupReadOnly;
	bool bGroupVisible;
	friend bool operator==( const PresetGroupInfo_t &lhs, const PresetGroupInfo_t &rhs ) { return lhs.presetGroupSym == rhs.presetGroupSym; }
	friend bool operator!=( const PresetGroupInfo_t &lhs, const PresetGroupInfo_t &rhs ) { return lhs.presetGroupSym != rhs.presetGroupSym; }
};

inline void CollectPresetGroupInfo( CDmeFilmClip *pFilmClip, CUtlVector< PresetGroupInfo_t > &presetInfo, bool bSkipReadOnly = false, bool bSkipInvisible = false )
{
	CAnimSetGroupAnimSetTraversal traversal( pFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		const CDmrElementArray< CDmePresetGroup > presetGroups = pAnimSet->GetPresetGroups();
		int nPresetGroups = presetGroups.Count();
		for ( int i = 0; i < nPresetGroups; ++i )
		{
			CDmePresetGroup *pPresetGroup = presetGroups[ i ];
			if ( !pPresetGroup )
				continue;

			if ( bSkipReadOnly && pPresetGroup->m_bIsReadOnly )
				continue;

			if ( bSkipInvisible && !pPresetGroup->m_bIsVisible )
				continue;

			PresetGroupInfo_t info( g_pDataModel->GetSymbol( pPresetGroup->GetName() ) );
			info.bGroupReadOnly = pPresetGroup->m_bIsReadOnly;
			info.bGroupVisible  = pPresetGroup->m_bIsVisible;
			info.bGroupShared   = pPresetGroup->IsShared();

			int idx = presetInfo.Find( info );
			if ( idx != presetInfo.InvalidIndex() )
				continue;

			presetInfo.AddToTail( info );
		}
	}
}

inline void CollectProceduralPresetNames( CUtlVector< CUtlSymbolLarge > &presetNames )
{
	static const CUtlSymbolLarge proceduralPresetGroupNameSymbol = g_pDataModel->GetSymbol( PROCEDURAL_PRESET_GROUP_NAME );

	int idx = presetNames.AddMultipleToTail( NUM_PROCEDURAL_PRESET_TYPES - 1 );
	for ( int i = 1; i < NUM_PROCEDURAL_PRESET_TYPES; ++i, ++idx ) // skip PROCEDURAL_PRESET_NOT
	{
		presetNames[ idx ] = g_pDataModel->GetSymbol( GetProceduralPresetName( i ) );
	}
}

inline void CollectPresetNamesForGroup( CDmeFilmClip *pFilmClip, const char *pPresetGroupName, CUtlVector< CUtlSymbolLarge > &presetNames )
{
	CAnimSetGroupAnimSetTraversal traversal( pFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( pPresetGroupName );
		if ( !pPresetGroup )
			continue;

		CDmrElementArray< CDmePreset > presets = pPresetGroup->GetPresets();

		int cp = presets.Count();
		for ( int j = 0; j < cp; ++j )
		{
			CDmePreset *pPreset = presets[ j ];
			Assert( pPreset );
			if ( !pPreset )
				continue;

			CUtlSymbolLarge symPresetName = g_pDataModel->GetSymbol( pPreset->GetName() );
			if ( presetNames.Find( symPresetName ) != presetNames.InvalidIndex() )
				continue;

			presetNames.AddToTail( symPresetName );
		}
	}
}

#endif // DMECONTROLS_UTILS_H
