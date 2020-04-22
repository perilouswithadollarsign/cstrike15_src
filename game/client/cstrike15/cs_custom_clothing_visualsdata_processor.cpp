//=========== Copyright © Valve Corporation, All rights reserved. =============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "utlbufferutil.h"
#include "cs_custom_clothing_visualsdata_processor.h"

void CCSClothingVisualsDataCompare::SerializeToBuffer( CUtlBuffer &buf )
{
	CBaseVisualsDataCompare::SerializeToBuffer( buf );
	Serialize( buf, m_nTeamId );
	Serialize( buf, m_bMirrorPattern );
	Serialize( buf, m_nMaterialId );
}

CCSClothingVisualsDataProcessor::CCSClothingVisualsDataProcessor( CCSClothingVisualsDataCompare &&compareObject, const WeaponPaintableMaterial_t *pWeaponPaintableMaterialData, const char *szCompositingShaderName )
	: m_pWeaponPaintableMaterialData( pWeaponPaintableMaterialData )
	, m_szCompositingShaderName( NULL )
{
	m_compareObject = Move( compareObject );
	m_compareObject.FillCompareBlob();
	SetVisualsData( szCompositingShaderName );
}

CCSClothingVisualsDataProcessor::~CCSClothingVisualsDataProcessor()
{
	if ( m_szCompositingShaderName )
	{
		delete [] m_szCompositingShaderName;
		m_szCompositingShaderName = NULL;
	}

}

void CCSClothingVisualsDataProcessor::Refresh()
{
	SetVisualsData( m_szCompositingShaderName );
}

void CCSClothingVisualsDataProcessor::SetVisualsData( const char *pCompositingShaderName )
{
	if ( m_szCompositingShaderName )
	{
		delete[] m_szCompositingShaderName;
		m_szCompositingShaderName = NULL;
	}

	if ( pCompositingShaderName )
	{
		m_szCompositingShaderName = new char[V_strlen( pCompositingShaderName ) + 1];
		V_strcpy( m_szCompositingShaderName, pCompositingShaderName );
	}
	
	memset( &m_visualsData, 0, sizeof( m_visualsData ) );

	V_strcpy( m_visualsData.m_szOrigVMTName, m_pWeaponPaintableMaterialData->m_szName );

	int nNameLength = V_strlen( m_pWeaponPaintableMaterialData->m_szName ) + 1;
	V_FileBase(m_visualsData.m_szOrigVMTName, m_visualsData.m_szOrigVMTBaseName, nNameLength );

	const CPaintKit *pPaintKit = GetItemSchema()->GetPaintKitDefinition( m_compareObject.m_nIndex );
	Assert( pPaintKit );

	V_strcpy( m_visualsData.m_szSkinVMTName, pPaintKit->sVmtPath );

	m_visualsData.pVMTOverrideValues = pPaintKit->kvVmtOverrides;

	KeyValues *pOrigVMTKeyValues = NULL;
	bool bVMTExists = materials->GetCustomMaterialManager()->GetVMTKeyValues( m_visualsData.m_szOrigVMTName, &pOrigVMTKeyValues );

	if (!bVMTExists)
	{
#ifdef DEBUG
		DevWarning( "Warning! Couldn't load VMT keyvalues: %s [check game/csgo/resource/vmtcache.txt]\n", m_visualsData.m_szOrigVMTName );
#else
		DevWarning( "Warning! Couldn't load VMT keyvalues: %s\n", m_visualsData.m_szOrigVMTName );
#endif
	}
	else
	{
		pOrigVMTKeyValues->MergeFrom( pPaintKit->kvVmtOverrides, KeyValues::MERGE_KV_UPDATE );
	}
	
	KeyValues *pSkinVMTKeyValues = NULL;
	bVMTExists = materials->GetCustomMaterialManager()->GetVMTKeyValues( m_visualsData.m_szSkinVMTName, &pSkinVMTKeyValues ); 

	if (!bVMTExists)
	{
#ifdef DEBUG
		DevWarning( "Warning! Couldn't load VMT keyvalues: %s [check game/csgo/resource/vmtcache.txt]\n", m_visualsData.m_szSkinVMTName );
#else
		DevWarning( "Warning! Couldn't load VMT keyvalues: %s\n", m_visualsData.m_szSkinVMTName );
#endif
	}

	m_visualsData.flWearProgress = m_compareObject.m_flWear;

	RandomSeed( m_compareObject.m_nSeed + m_compareObject.m_nMaterialId );

	m_visualsData.flPatternOffsetX = RandomFloat( pSkinVMTKeyValues->GetFloat( "$pattern_offset_x_start", 0.0 ), pSkinVMTKeyValues->GetFloat( "$pattern_offset_x_end", 1.0 ) );
	m_visualsData.flPatternOffsetY = RandomFloat( pSkinVMTKeyValues->GetFloat( "$pattern_offset_y_start", 0.0 ), pSkinVMTKeyValues->GetFloat( "$pattern_offset_y_end", 1.0 ) );
	m_visualsData.flPatternRot = RandomFloat( pSkinVMTKeyValues->GetFloat( "$pattern_rotate_start", 0.0 ), pSkinVMTKeyValues->GetFloat( "$pattern_rotate_end", 360.0 ) );
	m_visualsData.flPatternScaleX = pSkinVMTKeyValues->GetFloat( "$patternscale", 1.0 );
	m_visualsData.flPatternScaleY = m_visualsData.flPatternScaleX;

	m_visualsData.flGrungeOffsetX = RandomFloat( 0, 1 );
	m_visualsData.flGrungeOffsetY = RandomFloat( 0, 1 );
	m_visualsData.flGrungeRot = RandomFloat( 0, 360 );
	m_visualsData.flGrungeScale = pSkinVMTKeyValues->GetFloat( "$grungescale", 1.0 );

	if ( m_compareObject.m_bMirrorPattern )
	{
		m_visualsData.flPatternScaleX *= -1;
		m_visualsData.flFlipFixup = -1;
	}
	else
	{
		m_visualsData.flFlipFixup = 1;
	}

	V_strcpy( m_visualsData.szOrigNormalVTFName, pOrigVMTKeyValues->GetString( "$bumpmap" ) );
	m_visualsData.bHasBump = V_strlen( m_visualsData.szOrigNormalVTFName ) > 0;

	V_strcpy( m_visualsData.szOrigMasks1VTFName, pOrigVMTKeyValues->GetString( "$masks1" ) );
	m_visualsData.bHasMasks1 = V_strlen( m_visualsData.szOrigMasks1VTFName ) > 0;

	m_visualsData.bBaseAlphaPhongMask = pOrigVMTKeyValues->GetInt( "$basealphaphongmask" ) > 0;
	m_visualsData.bBaseAlphaEnvMask = pOrigVMTKeyValues->GetInt( "$basealphaenvmask" ) > 0;
	m_visualsData.bBumpAlphaEnvMask = pOrigVMTKeyValues->GetInt( "$bumpalphaenvmask" ) > 0;

	m_compareObject.FillCompareBlob();
}

//Generates keyvalues for the generated material to save
KeyValues *CCSClothingVisualsDataProcessor::GenerateCustomMaterialKeyValues()
{
	KeyValues *pOrigVMTKeyValues = NULL;
	bool bVMTExists = materials->GetCustomMaterialManager()->GetVMTKeyValues( m_visualsData.m_szOrigVMTName, &pOrigVMTKeyValues ); 

	if (!bVMTExists)
	{
#ifdef DEBUG
		DevWarning( "Warning! Couldn't load VMT keyvalues: %s [check game/csgo/resource/vmtcache.txt]\n", m_visualsData.m_szOrigVMTName );
#else
		DevWarning( "Warning! Couldn't load VMT keyvalues: %s\n", m_visualsData.m_szOrigVMTName );
#endif
	}

	if ( pOrigVMTKeyValues )
		pOrigVMTKeyValues->MergeFrom( m_visualsData.pVMTOverrideValues, KeyValues::MERGE_KV_UPDATE);

	// the custom material manager uses its inbound texture list to determine which parameters in the shader get replaced when the textures
	// finish generation.  The parameter names are assigned by the enum values of MaterialParamID_t, and are defined in base_visuals_data_processor.cpp
	// Currently there is nothing else to modify.

	return pOrigVMTKeyValues;
}

//Generates keyvalues for the composite material
KeyValues *CCSClothingVisualsDataProcessor::GenerateCompositeMaterialKeyValues( int nMaterialParamId )
{
	KeyValues *pOrigVMTKeyValues = NULL;
	bool bVMTExists = materials->GetCustomMaterialManager()->GetVMTKeyValues( m_visualsData.m_szOrigVMTName, &pOrigVMTKeyValues ); 

	if (!bVMTExists)
	{
#ifdef DEBUG
		DevWarning( "Warning! Couldn't load VMT keyvalues: %s [check game/csgo/resource/vmtcache.txt]\n", m_visualsData.m_szOrigVMTName );
#else
		DevWarning( "Warning! Couldn't load VMT keyvalues: %s\n", m_visualsData.m_szOrigVMTName );
#endif
		return NULL;
	}

	KeyValues *pSkinVMTKeyValues = NULL;
	bVMTExists = materials->GetCustomMaterialManager()->GetVMTKeyValues(  m_visualsData.m_szSkinVMTName, &pSkinVMTKeyValues );

	if (!bVMTExists)
	{
#ifdef DEBUG
		DevWarning( "Warning! Couldn't load VMT keyvalues: %s [check game/csgo/resource/vmtcache.txt]\n", m_visualsData.m_szSkinVMTName );
#else
		DevWarning( "Warning! Couldn't load VMT keyvalues: %s\n", m_visualsData.m_szSkinVMTName );
#endif
		return NULL;
	}

	if ( nMaterialParamId == MATERIAL_PARAM_ID_BUMP_MAP )
	{
		pSkinVMTKeyValues->SetString( "$bumpmap", m_visualsData.szOrigNormalVTFName );
	}
	else if ( nMaterialParamId == MATERIAL_PARAM_ID_MASKS1_MAP )
	{
		pSkinVMTKeyValues->SetString( "$masks1", m_visualsData.szOrigMasks1VTFName );
	}
	// else assume MATERIAL_PARAM_ID_BASE_DIFFUSE_TEXTURE which requires nothing special to be set

	if ( nMaterialParamId != MATERIAL_PARAM_ID_MASKS1_MAP )
	{
		pSkinVMTKeyValues->SetBool( "$basealphaphongmask", m_visualsData.bBaseAlphaPhongMask );
		pSkinVMTKeyValues->SetBool( "$basealphaenvmask", m_visualsData.bBaseAlphaEnvMask );
		pSkinVMTKeyValues->SetBool( "$bumpalphaenvmask", m_visualsData.bBumpAlphaEnvMask );
	}

	char charTemp[ 64 ];
	V_snprintf( charTemp, sizeof( charTemp ), "scale %.2f %.2f translate %.2f %.2f rotate %.6f", m_visualsData.flPatternScaleX, m_visualsData.flPatternScaleY, m_visualsData.flPatternOffsetX, m_visualsData.flPatternOffsetY, m_visualsData.flPatternRot );
	pSkinVMTKeyValues->SetString( "$patterntexturetransform", charTemp );
	V_snprintf( charTemp, sizeof( charTemp ), "scale %.2f %.2f translate %.2f %.2f rotate %.6f", m_visualsData.flGrungeScale, m_visualsData.flGrungeScale, m_visualsData.flGrungeOffsetX, m_visualsData.flGrungeOffsetY, m_visualsData.flGrungeRot );
	pSkinVMTKeyValues->SetString( "$grungetexturetransform", charTemp );

	float fPatternNormalFixup = fmod( -m_visualsData.flPatternRot * m_visualsData.flFlipFixup, 360.0f );
	if ( fPatternNormalFixup < 0 )
		fPatternNormalFixup += 360.0;

	float fGrungeNormalFixup = fmod( -m_visualsData.flGrungeRot, 360.0f );
	if ( fGrungeNormalFixup < 0 )
		fGrungeNormalFixup += 360.0;

	V_snprintf( charTemp, sizeof( charTemp ), "scale 1 1 translate 0 0 rotate %.6f", fPatternNormalFixup );
	pSkinVMTKeyValues->SetString("$patterntexturerotation", charTemp);
	V_snprintf( charTemp, sizeof( charTemp ), "scale 1 1 translate 0 0 rotate %.6f", fGrungeNormalFixup );
	pSkinVMTKeyValues->SetString("$grungetexturerotation", charTemp);

	pSkinVMTKeyValues->SetFloat( "$wearprogress", m_visualsData.flWearProgress );

	pSkinVMTKeyValues->SetFloat( "$flipfixup", m_visualsData.flFlipFixup );
	
	return pSkinVMTKeyValues;
}

bool CCSClothingVisualsDataProcessor::HasCustomMaterial() const
{
	return true;
}

const char* CCSClothingVisualsDataProcessor::GetSkinMaterialName() const
{
	return m_visualsData.m_szSkinVMTName;
}

const char* CCSClothingVisualsDataProcessor::GetOriginalMaterialName() const
{
	return m_visualsData.m_szOrigVMTName;
}

const char* CCSClothingVisualsDataProcessor::GetOriginalMaterialBaseName() const
{
	return m_visualsData.m_szOrigVMTBaseName;
}