//=========== Copyright © Valve Corporation, All rights reserved. =============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_CUSTOM_CHARACTER_VISUALSDATA_PROCESSOR_H
#define CS_CUSTOM_CHARACTER_VISUALSDATA_PROCESSOR_H

#include "cs_shareddefs.h"
#include "materialsystem/base_visuals_data_processor.h"
#include "materialsystem/icustommaterialmanager.h"


struct CCSClothingData_t
{
	bool bHasBump;
	bool bHasMasks1;

	char szOrigNormalVTFName[ MAX_PATH ];
	char szOrigMasks1VTFName[ MAX_PATH ];
	char m_szOrigVMTName[MAX_PATH];
	char m_szOrigVMTBaseName[MAX_PATH];
	char m_szSkinVMTName[MAX_PATH];

	float flWearProgress;

	float flPatternScaleX;
	float flPatternScaleY;
	float flPatternOffsetX;
	float flPatternOffsetY;
	float flPatternRot;

	float flGrungeScale;
	float flGrungeOffsetX;
	float flGrungeOffsetY;
	float flGrungeRot;

	bool bBaseAlphaPhongMask;
	bool bBaseAlphaEnvMask;
	bool bBumpAlphaEnvMask;

	float flFlipFixup;

	KeyValues *pVMTOverrideValues;
};

class CCSClothingVisualsDataCompare : public CBaseVisualsDataCompare
{
public:
	// Need to declare default constructor here since we have declared a move constructor
	CCSClothingVisualsDataCompare() = default;

	// Unfortunately, VS2013 doesn't support default declaration of move functions.
	CCSClothingVisualsDataCompare( CCSClothingVisualsDataCompare&& moveFrom ) // = default;
	: CBaseVisualsDataCompare( Move( moveFrom ) )
	, m_nTeamId( Move( moveFrom.m_nTeamId ) )
	, m_bMirrorPattern( Move( moveFrom.m_bMirrorPattern ) )
	, m_nMaterialId( Move( moveFrom.m_nMaterialId ) )
	{}
	CCSClothingVisualsDataCompare& operator=( CCSClothingVisualsDataCompare&& moveFrom ) // = default;
	{
		*(CBaseVisualsDataCompare*)this = Move( moveFrom );
		m_nTeamId = Move( moveFrom.m_nTeamId );
		m_bMirrorPattern = Move( moveFrom.m_bMirrorPattern );
		m_nMaterialId = Move( moveFrom.m_nMaterialId );
		return *this;
	}

	int m_nTeamId;
	bool m_bMirrorPattern;
	int m_nMaterialId;

	virtual void SerializeToBuffer( CUtlBuffer &buf );
};

class CCSClothingVisualsDataProcessor : public CBaseVisualsDataProcessor< CCSClothingVisualsDataCompare >
{
public:
	CCSClothingVisualsDataProcessor( CCSClothingVisualsDataCompare &&compareObject, const WeaponPaintableMaterial_t *pWeaponPaintableMaterialDat, const char *szCompositingShaderName = NULL );

	virtual KeyValues* GenerateCustomMaterialKeyValues();
	virtual KeyValues* GenerateCompositeMaterialKeyValues( int nMaterialParamId );
	virtual bool HasCustomMaterial() const;
	virtual const char* GetOriginalMaterialName() const;
	virtual const char* GetOriginalMaterialBaseName() const;
	virtual const char* GetSkinMaterialName() const;
	virtual void Refresh();

	virtual void SetVisualsData( const char *pCompositingShaderName = NULL );

private:
	virtual ~CCSClothingVisualsDataProcessor();

	const WeaponPaintableMaterial_t *m_pWeaponPaintableMaterialData;

	CCSClothingData_t m_visualsData;

	char *m_szCompositingShaderName;
};

#endif // CS_CUSTOM_CLOTHING_VISUALSDATA_PROCESSOR_H