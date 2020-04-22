//========= Copyright © 1996-2012, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_CUSTOM_WEAPON_VISUALSDATA_PROCESSOR_H
#define CS_CUSTOM_WEAPON_VISUALSDATA_PROCESSOR_H


#include "cs_shareddefs.h"
#include "materialsystem/base_visuals_data_processor.h"

//
// Weapon specific Visuals Data & Processor
//

enum VisualsDataPaintStyle_t
{
	VISUALS_DATA_PAINTSTYLE_ORIGINAL = 0,
	VISUALS_DATA_PAINTSTYLE_SOLID,			// solid color blocks
	VISUALS_DATA_PAINTSTYLE_HYDROGRAPHIC,	// hydrodipped
	VISUALS_DATA_PAINTSTYLE_SPRAY,			// spraypainted
	VISUALS_DATA_PAINTSTYLE_ANODIZED,		// anodized color blocks
	VISUALS_DATA_PAINTSTYLE_ANO_MULTI,		// anodized with a design
	VISUALS_DATA_PAINTSTYLE_ANO_AIR,		// anodized and spraypainted with dyes
	VISUALS_DATA_PAINTSTYLE_CUSTOM,			// custom paintjob
	VISUALS_DATA_PAINTSTYLE_ANTIQUED,		// antiqued
	VISUALS_DATA_PAINTSTYLE_GUNSMITH,		// half custom paintjob half antiqued

	VISUALS_DATA_PAINTSTYLE_COUNT
};

struct CCSSWeaponVisualsData_t 
{
	char szOrigDiffuseVTFName[ MAX_PATH ];
	char szOrigExpVTFName[ MAX_PATH ];
	char szMaskVTFName[ MAX_PATH ];
	char szPosVTFName[ MAX_PATH ];
	char szAOVTFName[ MAX_PATH ];
	char szSurfaceVTFName[ MAX_PATH ];
	char szPatternVTFName[ MAX_PATH ];
	char szOrigVMTName[ MAX_PATH ];

	VisualsDataPaintStyle_t nStyle;
	bool bUsesPattern;

	Vector col0;
	Vector col1;
	Vector col2;
	Vector col3;

	int nPhongAlbedoBoost;
	int nPhongExponent;
	int nPhongIntensity;

	float flPhongAlbedoFactor;

	float flWearProgress;

	float flPatternScale;
	float flPatternOffsetX;
	float flPatternOffsetY;
	float flPatternRot;

	float flWearScale;
	float flWearOffsetX;
	float flWearOffsetY;
	float flWearRot;

	float flGrungeScale;
	float flGrungeOffsetX;
	float flGrungeOffsetY;
	float flGrungeRot;

	char szGrungeVTFName[ MAX_PATH ];
	char szWearVTFName[ MAX_PATH ];
};

class CCSWeaponVisualsDataCompare : public CBaseVisualsDataCompare
{
public:
	CCSWeaponVisualsDataCompare() = default;
	CCSWeaponVisualsDataCompare( CCSWeaponVisualsDataCompare&& moveFrom ) // = default;
		: CBaseVisualsDataCompare( Move( moveFrom ) )
		, m_flWeaponLength( Move( moveFrom.m_flWeaponLength ) )
		, m_flUVScale( Move( moveFrom.m_flUVScale ) )
	{
	}

	CCSWeaponVisualsDataCompare& operator= (CCSWeaponVisualsDataCompare&& moveFrom) // = default;
	{
		*( CBaseVisualsDataCompare* )this = Move( moveFrom );
		m_flWeaponLength = Move( moveFrom.m_flWeaponLength );
		m_flUVScale = Move( moveFrom.m_flUVScale );
		return *this;
	}

	virtual void SerializeToBuffer( CUtlBuffer &buf );

	float m_flWeaponLength;
	float m_flUVScale;
};

class CCSWeaponVisualsDataProcessor : public CBaseVisualsDataProcessor< CCSWeaponVisualsDataCompare >
{
public:
	CCSWeaponVisualsDataProcessor( CCSWeaponVisualsDataCompare &&compareObject, const WeaponPaintableMaterial_t *pWeaponPaintableMaterialData, const char *pCompositingShaderName = NULL );
	
	virtual void SetVisualsData( const char *pCompositingShaderName = NULL );
	virtual KeyValues* GenerateCustomMaterialKeyValues();
	virtual KeyValues* GenerateCompositeMaterialKeyValues( int nMaterialParamId );
	virtual bool HasCustomMaterial() const;
	virtual const char* GetOriginalMaterialName() const;
	virtual const char* GetOriginalMaterialBaseName() const;
	virtual const char* GetPatternVTFName() const { return m_visualsData.szPatternVTFName; }
	virtual void Refresh();

private:
	virtual ~CCSWeaponVisualsDataProcessor();

	const WeaponPaintableMaterial_t *m_pWeaponPaintableMaterialData;

	char *m_pCompositingShaderName;
	CCSSWeaponVisualsData_t m_visualsData;
	float m_flPhongAlbedoFactor;
	int m_nPhongIntensity;
	bool m_bIgnoreWeaponSizeScale;
};

#endif // CUSTOM_WEAPON_VISUALSDATA_PROCESSOR
