//=========== Copyright © Valve Corporation, All rights reserved. =============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "utlbufferutil.h"
#include "cs_custom_embroider_visualsdata_processor.h"

void CCSEmbroiderVisualsDataCompare::SerializeToBuffer( CUtlBuffer &buf )
{
	CBaseVisualsDataCompare::SerializeToBuffer( buf );
}

CCSEmbroiderVisualsDataProcessor::CCSEmbroiderVisualsDataProcessor( CCSEmbroiderVisualsDataCompare &&compareObject, CCSEmbroiderData_t visualsData, const char *pCompositingShaderName )
	: m_pCompositingShaderName( NULL )
	, m_visualsData( visualsData )
{
	m_compareObject = Move( compareObject );
	SetVisualsData( pCompositingShaderName );
}

CCSEmbroiderVisualsDataProcessor::~CCSEmbroiderVisualsDataProcessor()
{
	if ( m_pCompositingShaderName )
	{
		delete [] m_pCompositingShaderName;
		m_pCompositingShaderName = NULL;
	}
}

void CCSEmbroiderVisualsDataProcessor::Refresh()
{
	SetVisualsData( m_pCompositingShaderName );
}

void CCSEmbroiderVisualsDataProcessor::SetVisualsData( const char *pCompositingShaderName )
{
	if ( m_pCompositingShaderName )
	{
		delete[] m_pCompositingShaderName;
		m_pCompositingShaderName = NULL;
	}

	if ( pCompositingShaderName )
	{
		m_pCompositingShaderName = new char[V_strlen( pCompositingShaderName ) + 1];
		V_strcpy( m_pCompositingShaderName, pCompositingShaderName );
	}
	m_compareObject.FillCompareBlob();
}

//Generates keyvalues for the generated material to save
KeyValues *CCSEmbroiderVisualsDataProcessor::GenerateCustomMaterialKeyValues()
{
	char bumpFileName[MAX_PATH], anisoFileName[MAX_PATH];
	V_snprintf( bumpFileName, MAX_PATH, "%s%s", m_visualsData.m_szVTFName, "_normal" );
	V_snprintf( anisoFileName, MAX_PATH, "%s%s", m_visualsData.m_szVTFName, "_anisodir" );
	KeyValues *pGeneratedVMTKeyValues = new KeyValues( "WeaponDecal" );
	pGeneratedVMTKeyValues->SetString( "$basetexture", m_visualsData.m_szVTFName );
	pGeneratedVMTKeyValues->SetString( "$normalmap", bumpFileName );
	pGeneratedVMTKeyValues->SetString( "$anisodirtexture", anisoFileName );
	pGeneratedVMTKeyValues->SetInt( "$decalstyle", 5 );
	pGeneratedVMTKeyValues->SetInt( "$phong", 1 );
	pGeneratedVMTKeyValues->SetFloat( "$phongalbedotint", 0.8 );
	pGeneratedVMTKeyValues->SetFloat( "$phongexponent", 3.0 );
	pGeneratedVMTKeyValues->SetString( "$phongfresnelranges", "[1 0 0]" );
	pGeneratedVMTKeyValues->SetInt( "$phongboost", 2 );

	return pGeneratedVMTKeyValues;
}

//Generates keyvalues for the composite material
KeyValues *CCSEmbroiderVisualsDataProcessor::GenerateCompositeMaterialKeyValues( int nMaterialParamId )
{
	KeyValues *pGeneratedVMTKeyValues = new KeyValues( m_pCompositingShaderName );
	pGeneratedVMTKeyValues->SetString( "$basetexture", m_visualsData.m_szOrigVTFName );
	pGeneratedVMTKeyValues->SetInt( "$ncolors", m_visualsData.m_nNColors );
	pGeneratedVMTKeyValues->SetFloat( "$colorgamma", m_visualsData.m_flGamma );
	pGeneratedVMTKeyValues->SetInt( "$texturemode", nMaterialParamId );

	return pGeneratedVMTKeyValues;
}

bool CCSEmbroiderVisualsDataProcessor::HasCustomMaterial() const
{
	return true;
}

const char* CCSEmbroiderVisualsDataProcessor::GetOriginalMaterialBaseName() const
{
	return m_visualsData.m_szOrigVMTName;
}

const char* CCSEmbroiderVisualsDataProcessor::GetOriginalMaterialName() const
{
	return m_visualsData.m_szOrigVMTName;
}