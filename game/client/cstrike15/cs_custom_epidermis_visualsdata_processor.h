//========= Copyright © 1996-2012, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_CUSTOM_EPIDERMIS_VISUALSDATA_PROCESSOR_H
#define CS_CUSTOM_EPIDERMIS_VISUALSDATA_PROCESSOR_H

#define CUSTOM_EPIDERMIS_MAX_VALID_STYLE 4

#include "cs_shareddefs.h"
#include "materialsystem/base_visuals_data_processor.h"

//
// Epidermis specific Visuals Data & Processor
//

struct CCSSEpidermisVisualsData_t 
{
	bool bUsed;

	int nStyle;

	char szDiffuseVTF[ MAX_PATH ];
	char szOrigVMTName[ MAX_PATH ];
	char szNormalVTF[ MAX_PATH ];
	char szExpVTF[ MAX_PATH ];
	char szPaintVTF[ MAX_PATH ];
	char szTatVTF[ MAX_PATH ];

};

class CCSEpidermisVisualsDataCompare : public CBaseVisualsDataCompare
{
public:
	CCSEpidermisVisualsDataCompare() = default;
	CCSEpidermisVisualsDataCompare( CCSEpidermisVisualsDataCompare&& moveFrom ) // = default;
		: CBaseVisualsDataCompare( Move( moveFrom ) )
		, m_bIsBody( Move( moveFrom.m_bIsBody ) )
	{
		memcpy( m_pchSkinIdent, moveFrom.m_pchSkinIdent, sizeof( m_pchSkinIdent ) );
	}
	CCSEpidermisVisualsDataCompare& operator=(CCSEpidermisVisualsDataCompare && moveFrom) // = default;
	{
		*( CBaseVisualsDataCompare* )this = Move( moveFrom );
		m_bIsBody = Move( moveFrom.m_bIsBody );
		memcpy( m_pchSkinIdent, moveFrom.m_pchSkinIdent, sizeof( m_pchSkinIdent ) );
		return *this;
	}

	virtual void SerializeToBuffer( CUtlBuffer &buf );

	bool m_bIsBody;
	char m_pchSkinIdent[ 16 ];
};

class CCSEpidermisVisualsDataProcessor : public CBaseVisualsDataProcessor< CCSEpidermisVisualsDataCompare >
{
public:
	CCSEpidermisVisualsDataProcessor( CCSEpidermisVisualsDataCompare &&compareObject, const char *pCompositingShaderName = NULL );

	void SetVisualsData( const char *pCompositingShaderName = NULL );

	virtual KeyValues *GenerateCustomMaterialKeyValues();
	virtual KeyValues *GenerateCompositeMaterialKeyValues( int nMaterialParamId );
	virtual bool HasCustomMaterial() const { return ( m_visualsData.nStyle > 0 && m_visualsData.nStyle <= CUSTOM_EPIDERMIS_MAX_VALID_STYLE ); }
	virtual const char* GetOriginalMaterialName() const;
	virtual const char* GetOriginalMaterialBaseName() const { return NULL; }

	int GetStyle() const { return  m_visualsData.nStyle; }

private:
	virtual ~CCSEpidermisVisualsDataProcessor();

	void SetSkinRootIdent();

	CCSSEpidermisVisualsData_t m_visualsData;
	char *m_pCompositingShaderName;
};


#endif // CUSTOM_EPIDERMIS_VISUALSDATA_PROCESSOR
