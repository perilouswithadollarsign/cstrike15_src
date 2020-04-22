//========= Copyright © 1996-2012, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "cs_custom_epidermis_visualsdata_processor.h"

void CCSEpidermisVisualsDataCompare::SerializeToBuffer( CUtlBuffer &buf )
{
	CBaseVisualsDataCompare::SerializeToBuffer( buf );
	Serialize( buf, m_bIsBody );
	Serialize( buf, CUtlString( m_pchSkinIdent ) );
}

//
// Custom Epidermis Visual Data Processor
//

CCSEpidermisVisualsDataProcessor::CCSEpidermisVisualsDataProcessor( CCSEpidermisVisualsDataCompare &&compareObject, const char *pCompositingShaderName )
	: m_pCompositingShaderName( NULL )
{
	m_compareObject = Move( compareObject );
	m_compareObject.FillCompareBlob();
	SetVisualsData( pCompositingShaderName );
	if ( m_visualsData.bUsed )
	{
		SetSkinRootIdent();
	}
}

CCSEpidermisVisualsDataProcessor::~CCSEpidermisVisualsDataProcessor()
{
	if ( m_pCompositingShaderName )
	{
		delete [] m_pCompositingShaderName;
		m_pCompositingShaderName = NULL;
	}
}

void CCSEpidermisVisualsDataProcessor::SetVisualsData( const char *pCompositingShaderName )
{
	if ( m_compareObject.m_nIndex == -1 )
		return;

	// this is required so that Compare will work
	memset( &m_visualsData, 0, sizeof( m_visualsData ) );

	if ( m_pCompositingShaderName )
	{
		delete [] m_pCompositingShaderName;
		m_pCompositingShaderName = NULL;
	}

	if ( pCompositingShaderName )
	{
		m_pCompositingShaderName = new char[ V_strlen( pCompositingShaderName ) + 1 ];
		V_strcpy( m_pCompositingShaderName, pCompositingShaderName );
	}

	//read in paint kit
	const CPaintKit *pPaintKit = GetItemSchema()->GetPaintKitDefinition( m_compareObject.m_nIndex );
	m_visualsData.bUsed = ( pPaintKit != NULL );

	if ( !m_visualsData.bUsed )
		return;

	// Initialized pseudo-random number generator
	RandomSeed( m_compareObject.m_nSeed );

	m_visualsData.nStyle = pPaintKit->nStyle;

	if ( m_visualsData.nStyle == 0 )
	{
		m_visualsData.nStyle = 1;
	}

	//paint
	if ( m_visualsData.nStyle == 2 || m_visualsData.nStyle == 4 )
	{
		V_snprintf( m_visualsData.szPaintVTF, sizeof( m_visualsData.szPaintVTF ), "models/player/shared/skin/%s.vtf", pPaintKit->sPattern.Get() );
	}

	//tattoo
	if ( m_visualsData.nStyle == 3 || m_visualsData.nStyle == 4 )
	{
		V_snprintf( m_visualsData.szTatVTF, sizeof( m_visualsData.szTatVTF ), "models/player/shared/skin/%s.vtf", pPaintKit->sLogoMaterial.Get() );
	}

}

void CCSEpidermisVisualsDataProcessor::SetSkinRootIdent()
{
	if ( m_compareObject.m_bIsBody )
	{
		V_snprintf( m_visualsData.szDiffuseVTF, sizeof( m_visualsData.szDiffuseVTF ), "models/player/gen_custom_parts/gen_body_%s.vtf", m_compareObject.m_pchSkinIdent );
		V_snprintf( m_visualsData.szNormalVTF, sizeof( m_visualsData.szNormalVTF ), "models/player/gen_custom_parts/gen_body_normal.vtf" );
		V_snprintf( m_visualsData.szExpVTF, sizeof( m_visualsData.szExpVTF ), "models/player/gen_custom_parts/gen_body_exp.vtf" );
	}
	else
	{
		V_snprintf( m_visualsData.szDiffuseVTF, sizeof( m_visualsData.szDiffuseVTF ), "models/player/ctm_custom_parts/ct_faces/ctm_face_%s.vtf", m_compareObject.m_pchSkinIdent );
		V_snprintf( m_visualsData.szNormalVTF, sizeof( m_visualsData.szNormalVTF ), "models/player/ctm_custom_parts/ct_faces/ctm_face_%s_normal.vtf", m_compareObject.m_pchSkinIdent );
		V_snprintf( m_visualsData.szExpVTF, sizeof( m_visualsData.szExpVTF ), "models/player/ctm_custom_parts/ct_faces/ctm_face_%s_exp.vtf", m_compareObject.m_pchSkinIdent );
	}
	V_snprintf( m_visualsData.szOrigVMTName, sizeof( m_visualsData.szOrigVMTName ), "materials/models/player/player_skin_shared.vmt" );
	 
}

KeyValues *CCSEpidermisVisualsDataProcessor::GenerateCustomMaterialKeyValues()
{
	
	KeyValues *pVMTKeyValues = new KeyValues( "VertexLitGeneric" );
	bool bVMTExists = pVMTKeyValues->LoadFromFile( g_pFullFileSystem , m_visualsData.szOrigVMTName, "MOD" );

	if ( bVMTExists )
	{
		pVMTKeyValues->SetString( "$basetexture", m_visualsData.szDiffuseVTF );
		pVMTKeyValues->SetString( "$bumpmap", m_visualsData.szNormalVTF );
		pVMTKeyValues->SetString( "$phongexponenttexture", m_visualsData.szExpVTF );
	}
	else
	{
		delete pVMTKeyValues;
		pVMTKeyValues = NULL;
	}

	return pVMTKeyValues;
}


//Fills given key values with appropriate custom epidermis shader material parameters
KeyValues *CCSEpidermisVisualsDataProcessor::GenerateCompositeMaterialKeyValues( int nMaterialParamId )
{
	KeyValues *pVMTKeyValues = new KeyValues( m_pCompositingShaderName );

	char charTemp[ 64 ];

	// style
	V_snprintf( charTemp, sizeof( charTemp ), "%i", m_visualsData.nStyle );
	pVMTKeyValues->SetString( "$style", charTemp );

	// vtfs
	pVMTKeyValues->SetString( "$baseTexture", m_visualsData.szDiffuseVTF );

	if ( m_visualsData.nStyle == 2 || m_visualsData.nStyle == 4 )
	{
		pVMTKeyValues->SetString( "$paintTexture", m_visualsData.szPaintVTF );
	}

	if ( m_visualsData.nStyle == 3 || m_visualsData.nStyle == 4 )
	{
		pVMTKeyValues->SetString( "$tatTexture", m_visualsData.szTatVTF );
	}
	
	return pVMTKeyValues;
}

const char* CCSEpidermisVisualsDataProcessor::GetOriginalMaterialName() const
{
	return m_visualsData.szOrigVMTName;
}
