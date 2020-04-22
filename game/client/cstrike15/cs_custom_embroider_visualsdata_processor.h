//=========== Copyright © Valve Corporation, All rights reserved. =============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_CUSTOM_EMBROIDER_VISUALSDATA_PROCESSOR_H
#define CS_CUSTOM_EMBROIDER_VISUALSDATA_PROCESSOR_H

#include "cs_shareddefs.h"
#include "materialsystem/base_visuals_data_processor.h"
#include "materialsystem/icustommaterialmanager.h"


struct CCSEmbroiderData_t
{
	char m_szOrigVTFName[MAX_PATH];
	char m_szOrigVMTName[MAX_PATH];
	char m_szVTFName[MAX_PATH];
	float m_flGamma;
	int m_nNColors;
};

class CCSEmbroiderVisualsDataCompare : public CBaseVisualsDataCompare
{
public:
	// Need to declare default constructor here since we have declared a move constructor
	CCSEmbroiderVisualsDataCompare() = default;

	// Unfortunately, VS2013 doesn't support default declaration of move functions.
	CCSEmbroiderVisualsDataCompare( CCSEmbroiderVisualsDataCompare&& moveFrom ) // = default;
	: CBaseVisualsDataCompare( Move( moveFrom ) )
	{}
	CCSEmbroiderVisualsDataCompare& operator=( CCSEmbroiderVisualsDataCompare&& moveFrom ) // = default;
	{
		*(CBaseVisualsDataCompare*)this = Move( moveFrom );
		return *this;
	}

	virtual void SerializeToBuffer( CUtlBuffer &buf );
};

class CCSEmbroiderVisualsDataProcessor : public CBaseVisualsDataProcessor< CCSEmbroiderVisualsDataCompare >
{
public:
	CCSEmbroiderVisualsDataProcessor( CCSEmbroiderVisualsDataCompare &&compareObject, CCSEmbroiderData_t visualsData, const char *pCompositingShaderName = NULL );

	virtual KeyValues* GenerateCustomMaterialKeyValues();
	virtual KeyValues* GenerateCompositeMaterialKeyValues( int nMaterialParamId );
	virtual bool HasCustomMaterial() const;
	virtual const char* GetOriginalMaterialName() const;
	virtual const char* GetOriginalMaterialBaseName() const;
	virtual void Refresh();

	virtual void SetVisualsData( const char *pCompositingShaderName = NULL );

private:
	virtual ~CCSEmbroiderVisualsDataProcessor();
	//IMaterial *m_pCompositeTargetMaterial;

	CCSEmbroiderData_t m_visualsData;

	char *m_pCompositingShaderName;
};

#endif // CS_CUSTOM_EMBROIDER_VISUALSDATA_PROCESSOR_H