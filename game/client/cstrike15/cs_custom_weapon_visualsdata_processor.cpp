//========= Copyright © 1996-2012, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "utlbufferutil.h"
#include "cs_custom_weapon_visualsdata_processor.h"
#include "materialsystem/icustommaterialmanager.h"
#include "tier1/strtools.h"

void CCSWeaponVisualsDataCompare::SerializeToBuffer( CUtlBuffer &buf )
{
	CBaseVisualsDataCompare::SerializeToBuffer( buf );
	Serialize( buf, m_flWeaponLength );
	Serialize( buf, m_flUVScale );
}

//
// Custom Weapon Visual Data Processor
//

CCSWeaponVisualsDataProcessor::CCSWeaponVisualsDataProcessor( CCSWeaponVisualsDataCompare &&compareObject, const WeaponPaintableMaterial_t *pWeaponPaintableMaterialData, const char *pCompositingShaderName )
	: m_pWeaponPaintableMaterialData( pWeaponPaintableMaterialData )
	, m_pCompositingShaderName( NULL )
	, m_bIgnoreWeaponSizeScale( false )
	, m_flPhongAlbedoFactor( 1.0f )
	, m_nPhongIntensity( 0 )
{
	m_compareObject = Move( compareObject );
	m_compareObject.FillCompareBlob();
	SetVisualsData( pCompositingShaderName );
}

CCSWeaponVisualsDataProcessor::~CCSWeaponVisualsDataProcessor()
{
	if ( m_pCompositingShaderName )
	{
		delete [] m_pCompositingShaderName;
		m_pCompositingShaderName = NULL;
	}
}

char *g_pPatternNames[ VISUALS_DATA_PAINTSTYLE_COUNT ] =
{
	"original",
	"solid",
	"hydrographic",
	"spray",
	"anodized",
	"anodized_multi",
	"anodized_air",
	"custom",
	"antiqued",
	"gunsmith"
};

void CCSWeaponVisualsDataProcessor::Refresh()
{
	SetVisualsData( m_pCompositingShaderName );
}

void CCSWeaponVisualsDataProcessor::SetVisualsData( const char *pCompositingShaderName )
{
	if ( m_pCompositingShaderName )
	{
		delete [] m_pCompositingShaderName;
		m_pCompositingShaderName = NULL;
	}

	if ( pCompositingShaderName )
	{
		m_pCompositingShaderName = V_strdup( pCompositingShaderName );
	}

	memset( &m_visualsData, 0, sizeof( m_visualsData ) );

	m_visualsData.flPhongAlbedoFactor = 1.0f;

	//read in paint kit
	const CPaintKit *pPaintKit = GetItemSchema()->GetPaintKitDefinition( m_compareObject.m_nIndex );
	Assert( pPaintKit );

	//read in diffuse override bool
	m_visualsData.nStyle = (VisualsDataPaintStyle_t)pPaintKit->nStyle;

	m_visualsData.col0 = Vector( pPaintKit->rgbaColor[0].r(), pPaintKit->rgbaColor[0].g(), pPaintKit->rgbaColor[0].b() );
	m_visualsData.col1 = Vector( pPaintKit->rgbaColor[1].r(), pPaintKit->rgbaColor[1].g(), pPaintKit->rgbaColor[1].b() );
	m_visualsData.col2 = Vector( pPaintKit->rgbaColor[2].r(), pPaintKit->rgbaColor[2].g(), pPaintKit->rgbaColor[2].b() );
	m_visualsData.col3 = Vector( pPaintKit->rgbaColor[3].r(), pPaintKit->rgbaColor[3].g(), pPaintKit->rgbaColor[3].b() );

	m_visualsData.nPhongExponent = pPaintKit->uchPhongExponent;

	m_visualsData.flWearProgress = m_compareObject.m_flWear;

	m_visualsData.nPhongAlbedoBoost = pPaintKit->uchPhongAlbedoBoost;
	m_visualsData.nPhongAlbedoBoost -= 1;

	m_visualsData.nPhongIntensity = pPaintKit->uchPhongIntensity;

	m_visualsData.flPatternScale = pPaintKit->flPatternScale;

	m_bIgnoreWeaponSizeScale = pPaintKit->bIgnoreWeaponSizeScale;

	RandomSeed( m_compareObject.m_nSeed );

	float flOffX0 = pPaintKit->flPatternOffsetXStart;
	float flOffX1 = pPaintKit->flPatternOffsetXEnd;
	m_visualsData.flPatternOffsetX = RandomFloat( flOffX0, flOffX1 );

	float flOffY0 = pPaintKit->flPatternOffsetYStart;
	float flOffY1 = pPaintKit->flPatternOffsetYEnd;
	m_visualsData.flPatternOffsetY = RandomFloat( flOffY0, flOffY1 );

	float flRot0 = pPaintKit->flPatternRotateStart;
	float flRot1 = pPaintKit->flPatternRotateEnd;
	m_visualsData.flPatternRot = RandomFloat( flRot0, flRot1 );

	// Pseudo-random values based on skin's seed for grunge and wear:
	m_visualsData.flWearScale = RandomFloat( 1.6, 1.8 );
	m_visualsData.flWearOffsetX = RandomFloat( 0, 1 );
	m_visualsData.flWearOffsetY = RandomFloat( 0, 1 );
	m_visualsData.flWearRot = RandomFloat( 0, 360 );
	
	m_visualsData.flGrungeScale = RandomFloat( 1.6, 1.8 );
	m_visualsData.flGrungeOffsetX = RandomFloat( 0, 1 );
	m_visualsData.flGrungeOffsetY = RandomFloat( 0, 1 );
	m_visualsData.flGrungeRot = RandomFloat( 0, 360 );

	m_visualsData.bUsesPattern = true;
	switch ( m_visualsData.nStyle )
	{
		case VISUALS_DATA_PAINTSTYLE_HYDROGRAPHIC:
		case VISUALS_DATA_PAINTSTYLE_SPRAY:
		case VISUALS_DATA_PAINTSTYLE_ANO_MULTI:
		case VISUALS_DATA_PAINTSTYLE_ANO_AIR:
		case VISUALS_DATA_PAINTSTYLE_CUSTOM:
		case VISUALS_DATA_PAINTSTYLE_ANTIQUED:
		case VISUALS_DATA_PAINTSTYLE_GUNSMITH:
			if ( V_IsAbsolutePath( pPaintKit->sPattern.String() ) )
			{
				V_strcpy_safe( m_visualsData.szPatternVTFName, pPaintKit->sPattern.String() );
			}
			else if ( !V_strcasecmp( pPaintKit->sPattern.String(), "black" ) )
			{
				V_snprintf( m_visualsData.szPatternVTFName, sizeof( m_visualsData.szPatternVTFName ), "%s.vtf", pPaintKit->sPattern.String() );
			}
			else
			{
				V_snprintf( m_visualsData.szPatternVTFName, sizeof( m_visualsData.szPatternVTFName ), "models/weapons/customization/paints/%s/%s.vtf", g_pPatternNames[ m_visualsData.nStyle ], pPaintKit->sPattern.String() );
			}
			break;
		default:
			m_visualsData.szPatternVTFName[0] = '\0';
			m_visualsData.bUsesPattern = false;
			break;
	}

	//weapon name and original mat name (if different from weapon name)
	const char *pWeaponName = m_pWeaponPaintableMaterialData->m_szName;
	const char *pFolderName = m_pWeaponPaintableMaterialData->m_szFolderName;
	const char *pOriginalMatName = GetOriginalMaterialBaseName();

	if ( pOriginalMatName && pWeaponName && pFolderName )
	{
		// texture file names
		V_snprintf( m_visualsData.szOrigVMTName, sizeof( m_visualsData.szOrigVMTName ), "materials/models/weapons/v_models/%s/%s.vmt", pFolderName, pOriginalMatName );

		// some default skins have baked-in paint, so must be overridden with a simplified version of the texture
		if ( m_pWeaponPaintableMaterialData->m_bBaseTextureOverride )
		{
			V_snprintf( m_visualsData.szOrigDiffuseVTFName, sizeof( m_visualsData.szOrigDiffuseVTFName ), "models/weapons/customization/%s/%s.vtf", pFolderName, pWeaponName );
		}
		else
		{
			V_snprintf( m_visualsData.szOrigDiffuseVTFName, sizeof( m_visualsData.szOrigDiffuseVTFName ), "models/weapons/v_models/%s/%s.vtf", pFolderName, pOriginalMatName );
		}

		// exponent
		V_snprintf( m_visualsData.szOrigExpVTFName, sizeof( m_visualsData.szOrigExpVTFName ), "models/weapons/v_models/%s/%s_exponent.vtf", pFolderName, pOriginalMatName );

		// Composite sources
		V_snprintf( m_visualsData.szPosVTFName, sizeof( m_visualsData.szPosVTFName ), "models/weapons/customization/%s/%s_pos.vtf", pFolderName, pWeaponName );
		V_snprintf( m_visualsData.szAOVTFName, sizeof( m_visualsData.szAOVTFName ), "models/weapons/customization/%s/%s_ao.vtf", pFolderName, pWeaponName );
		V_snprintf( m_visualsData.szSurfaceVTFName, sizeof( m_visualsData.szSurfaceVTFName ), "models/weapons/customization/%s/%s_surface.vtf", pFolderName, pWeaponName );
		V_snprintf( m_visualsData.szMaskVTFName, sizeof( m_visualsData.szMaskVTFName ), "models/weapons/customization/%s/%s_masks.vtf", pFolderName, pWeaponName );

		// will these ever be per-skin definition? atm they are all the same, so perhaps defining this in m_visualsData is redundant
		V_snprintf( m_visualsData.szGrungeVTFName, sizeof( m_visualsData.szGrungeVTFName ), "models/weapons/customization/shared/gun_grunge.vtf" );
		V_snprintf( m_visualsData.szWearVTFName, sizeof( m_visualsData.szWearVTFName ), "models/weapons/customization/shared/paint_wear.vtf" );
	}
}

KeyValues *CCSWeaponVisualsDataProcessor::GenerateCustomMaterialKeyValues()
{
	// load the VMT key values for the compositing material, then adjust them based on visuals data

	KeyValues *pVMTKeyValues = NULL;
	bool bVMTExists = materials->GetCustomMaterialManager()->GetVMTKeyValues( GetOriginalMaterialName(), &pVMTKeyValues ); 

	if (!bVMTExists)
	{
#ifdef DEBUG
		DevWarning( "Warning! Couldn't load VMT keyvalues: %s [check game/csgo/resource/vmtcache.txt]\n", GetOriginalMaterialName() );
#else
		DevWarning( "Warning! Couldn't load VMT keyvalues: %s\n", GetOriginalMaterialName() );
#endif
	}

	m_flPhongAlbedoFactor = m_visualsData.flPhongAlbedoFactor;
	m_nPhongIntensity = m_visualsData.nPhongIntensity;

	if ( bVMTExists )
	{
		float flPhongAlbedoBoost = m_visualsData.nPhongAlbedoBoost;
		float flOrigPhongAlbedoBoost = ( pVMTKeyValues->GetFloat( "$phongalbedoboost", -1 ) );
		bool bPhongAlbedoBoost = true;
		if ( flOrigPhongAlbedoBoost == -1 )
		{
			flOrigPhongAlbedoBoost = ( pVMTKeyValues->GetFloat( "$phongboost", 1.0 ) );
			bPhongAlbedoBoost = false;
		}

		if ( (flPhongAlbedoBoost < 0 ) && ( flOrigPhongAlbedoBoost > 0 ) )
		{
			m_flPhongAlbedoFactor = flOrigPhongAlbedoBoost;
		}
		else
		{
			m_flPhongAlbedoFactor =  flOrigPhongAlbedoBoost / flPhongAlbedoBoost;
		}

		flPhongAlbedoBoost = ( flOrigPhongAlbedoBoost < flPhongAlbedoBoost ) ? flPhongAlbedoBoost : flOrigPhongAlbedoBoost;

		float flOrigPhongBoost = pVMTKeyValues->GetFloat( "$phongboost", 1 );
		if ( ( flOrigPhongBoost > 0 ) && !( ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANODIZED ) ||
											( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANO_MULTI) ||
											( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANO_AIR ) ||
											( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANTIQUED ) 
										  ) 
			)
		{
			m_nPhongIntensity = m_nPhongIntensity / flOrigPhongBoost;
		}

		if ( ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANODIZED ) || 
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANO_AIR ) || 
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANO_MULTI ) || 
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANTIQUED ) ||
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_GUNSMITH )
		   )
		{
			pVMTKeyValues->SetInt( "$phongalbedotint", 1 );
			pVMTKeyValues->SetFloat( "$phongalbedoboost", flPhongAlbedoBoost );
		}
		/*else if ( bPhongAlbedoBoost )
		{
			pVMTKeyValues->SetFloat( "$phongalbedoboost", flPhongAlbedoBoost );
		}*/
	
	}
	else
	{
		m_flPhongAlbedoFactor = 1.0f;
		delete pVMTKeyValues;
		pVMTKeyValues = NULL;
	}

	pVMTKeyValues->SetString("$envmaptint", "[0 0 0]");

	return pVMTKeyValues;
}

//Fills given key values with appropriate custom weapon shader material parameters
KeyValues *CCSWeaponVisualsDataProcessor::GenerateCompositeMaterialKeyValues( int nMaterialParamId )
{
	KeyValues *pVMTKeyValues = new KeyValues( m_pCompositingShaderName );

	pVMTKeyValues->SetString( "$aotexture", m_visualsData.szAOVTFName );
	pVMTKeyValues->SetString( "$weartexture", m_visualsData.szWearVTFName );

	if ( nMaterialParamId == MATERIAL_PARAM_ID_PHONG_EXPONENT_TEXTURE )
	{
		pVMTKeyValues->SetString( "$exponentmode", "1" );
		pVMTKeyValues->SetString( "$exptexture", m_visualsData.szOrigExpVTFName );
		if ( ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_HYDROGRAPHIC ) || 
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANO_MULTI ) ||
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_CUSTOM ) ||
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_GUNSMITH )
		   )
		{
			pVMTKeyValues->SetString( "$painttexture", m_visualsData.szPatternVTFName );
		}
		if ( ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANODIZED ) || 
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANO_MULTI ) ||
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANO_AIR ) ||
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANTIQUED ) ||
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_GUNSMITH )
		   )
		{
			pVMTKeyValues->SetString( "$maskstexture", m_visualsData.szMaskVTFName );
		}
	}
	else if ( nMaterialParamId == MATERIAL_PARAM_ID_BASE_DIFFUSE_TEXTURE )
	{
		pVMTKeyValues->SetString( "$exponentmode", "0" );
		pVMTKeyValues->SetString( "$baseTexture", m_visualsData.szOrigDiffuseVTFName );
		pVMTKeyValues->SetString( "$maskstexture", m_visualsData.szMaskVTFName );
		pVMTKeyValues->SetString( "$grungetexture", m_visualsData.szGrungeVTFName );
		if ( ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_HYDROGRAPHIC ) || 
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_SPRAY ) ||
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANO_MULTI ) ||
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANO_AIR ) ||
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_CUSTOM ) ||
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANTIQUED ) ||
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_GUNSMITH )
		   )
		{
			pVMTKeyValues->SetString( "$painttexture", m_visualsData.szPatternVTFName );
		}
		if ( ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_SPRAY ) || ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANO_AIR ) )
		{
			pVMTKeyValues->SetString( "$postexture", m_visualsData.szPosVTFName );
			pVMTKeyValues->SetString( "$surfacetexture", m_visualsData.szSurfaceVTFName );
		}
		if ( ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANODIZED ) || 
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANO_MULTI ) ||
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANO_AIR ) ||
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANTIQUED ) ||
			 ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_GUNSMITH )
		   )
		{
			pVMTKeyValues->SetString( "$exptexture", m_visualsData.szOrigExpVTFName );
		}	
	}
	else
	{
		AssertMsg(0, "Invalid Material Param ID");
	}

	// Reassemble the color values into 0-1 space floats in strings.
	char charTemp[ 64 ];
	const float fColor255toFloat = 1.0f / 255.0f;
	V_snprintf( charTemp, sizeof( charTemp ), "[%f %f %f]", m_visualsData.col0[0] * fColor255toFloat,  m_visualsData.col0[1] * fColor255toFloat,  m_visualsData.col0[2] * fColor255toFloat );
	pVMTKeyValues->SetString( "$camocolor0", charTemp );
	V_snprintf( charTemp, sizeof( charTemp ), "[%f %f %f]", m_visualsData.col1[0] * fColor255toFloat,  m_visualsData.col1[1] * fColor255toFloat,  m_visualsData.col1[2] * fColor255toFloat );
	pVMTKeyValues->SetString( "$camocolor1", charTemp );
	V_snprintf( charTemp, sizeof( charTemp ), "[%f %f %f]", m_visualsData.col2[0] * fColor255toFloat,  m_visualsData.col2[1] * fColor255toFloat,  m_visualsData.col2[2] * fColor255toFloat );
	pVMTKeyValues->SetString( "$camocolor2", charTemp );
	V_snprintf( charTemp, sizeof( charTemp ), "[%f %f %f]", m_visualsData.col3[0] * fColor255toFloat,  m_visualsData.col3[1] * fColor255toFloat,  m_visualsData.col3[2] * fColor255toFloat );
	pVMTKeyValues->SetString( "$camocolor3", charTemp );

	V_snprintf( charTemp, sizeof( charTemp ), "%f", m_visualsData.flWearProgress );
	pVMTKeyValues->SetString( "$wearprogress", charTemp );

	V_snprintf( charTemp, sizeof( charTemp ), "%i", m_visualsData.nStyle );
	pVMTKeyValues->SetString( "$paintstyle", charTemp );

	float flWeaponSizeScale = 1.0f;
	if ( ( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_SPRAY ) ||
		( m_visualsData.nStyle == VISUALS_DATA_PAINTSTYLE_ANO_AIR ) )
	{
		flWeaponSizeScale = ( m_bIgnoreWeaponSizeScale == true ) ? 1.0f : m_compareObject.m_flWeaponLength / 36.0f;
	}
	else
	{
		flWeaponSizeScale = ( m_bIgnoreWeaponSizeScale == true ) ? 1.0f : m_compareObject.m_flUVScale;
	}
	float flPatternScale = m_visualsData.flPatternScale * flWeaponSizeScale;
	float flWearScale = m_visualsData.flWearScale * flWeaponSizeScale;
	float flGrungeScale = m_visualsData.flGrungeScale * flWeaponSizeScale;

	V_snprintf( charTemp, sizeof( charTemp ), "scale %.2f %.2f translate %.2f %.2f rotate %.2f", flPatternScale, flPatternScale, m_visualsData.flPatternOffsetX, m_visualsData.flPatternOffsetY, m_visualsData.flPatternRot );
	pVMTKeyValues->SetString( "$patterntexturetransform", charTemp );
	V_snprintf( charTemp, sizeof( charTemp ), "scale %.2f %.2f translate %.2f %.2f rotate %.2f", flWearScale, flWearScale, m_visualsData.flWearOffsetX, m_visualsData.flWearOffsetY, m_visualsData.flWearRot );
	pVMTKeyValues->SetString( "$weartexturetransform", charTemp );
	V_snprintf( charTemp, sizeof( charTemp ), "scale %.2f %.2f translate %.2f %.2f rotate %.2f", flGrungeScale, flGrungeScale, m_visualsData.flGrungeOffsetX, m_visualsData.flGrungeOffsetY, m_visualsData.flGrungeRot );
	pVMTKeyValues->SetString( "$grungetexturetransform", charTemp );

	V_snprintf( charTemp, sizeof( charTemp ), "%f", m_flPhongAlbedoFactor );
	pVMTKeyValues->SetString( "$phongalbedofactor", charTemp );

	V_snprintf( charTemp, sizeof( charTemp ), "%f", m_nPhongIntensity * fColor255toFloat );
	pVMTKeyValues->SetString( "$phongintensity", charTemp );

	V_snprintf( charTemp, sizeof( charTemp ), "%f", m_visualsData.nPhongExponent * fColor255toFloat );
	pVMTKeyValues->SetString( "$phongexponent", charTemp );

	return pVMTKeyValues;
}

bool CCSWeaponVisualsDataProcessor::HasCustomMaterial() const
{
	return ( m_visualsData.nStyle != 0 );
}

const char* CCSWeaponVisualsDataProcessor::GetOriginalMaterialBaseName() const
{
	return m_pWeaponPaintableMaterialData->m_szOriginalMaterialName[0] != 0 ? m_pWeaponPaintableMaterialData->m_szOriginalMaterialName : m_pWeaponPaintableMaterialData->m_szName;
}

const char* CCSWeaponVisualsDataProcessor::GetOriginalMaterialName() const
{
	return m_visualsData.szOrigVMTName;
}
