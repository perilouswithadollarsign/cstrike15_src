//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: Provide custom texture generation (compositing) 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASE_VISUALS_DATA_PROCESSOR_H
#define BASE_VISUALS_DATA_PROCESSOR_H

#include "ivisualsdataprocessor.h"
#include "utlbuffer.h"
#include "utlbufferutil.h"


// derive from and extend this class if you have additional items to compare in your Visuals Data Processor
class CBaseVisualsDataCompare : public IVisualsDataCompare
{
public:
	CBaseVisualsDataCompare() = default;

	CBaseVisualsDataCompare( CBaseVisualsDataCompare&& moveFrom ) // = default;
		: m_nIndex( Move( moveFrom.m_nIndex ) )
		, m_nSeed( Move( moveFrom.m_nSeed ) )
		, m_flWear( Move( moveFrom.m_flWear ) )
		, m_nLOD( Move( moveFrom.m_nLOD ) )
		, m_nModelID( Move( moveFrom.m_nModelID ) )
		, m_compareBlob( Move( moveFrom.m_compareBlob ) )
	{}

	CBaseVisualsDataCompare& operator=( CBaseVisualsDataCompare&& moveFrom ) // = default;
	{
		m_nIndex = Move( moveFrom.m_nIndex );
		m_nSeed = Move( moveFrom.m_nSeed );
		m_flWear = Move( moveFrom.m_flWear );
		m_nLOD = Move( moveFrom.m_nLOD );
		m_nModelID = Move( moveFrom.m_nModelID );
		m_compareBlob = Move( moveFrom.m_compareBlob );
		return *this;
	}

	virtual void FillCompareBlob()
	{
		SerializeToBuffer( m_compareBlob );
	}
	virtual const CUtlBuffer &GetCompareBlob() const
	{
		return m_compareBlob;
	}

	virtual bool Compare( const CUtlBuffer &otherBuf )
	{
		return ( m_compareBlob.TellPut() == otherBuf.TellPut() && V_memcmp( otherBuf.Base(), m_compareBlob.Base(), m_compareBlob.TellPut() ) == 0 );
	}

	int m_nIndex;
	int m_nSeed;
	float m_flWear;
	int m_nLOD;
	int m_nModelID;  // for weapons this is CSWeaponID, for clothing this is ClothingDefinitionSlotId_t

protected:
	virtual void SerializeToBuffer( CUtlBuffer &buf )
	{
		buf.Clear();
		Serialize( buf, m_nIndex );
		Serialize( buf, m_nSeed );
		Serialize( buf, m_flWear );
		Serialize( buf, m_nLOD );
		Serialize( buf, m_nModelID );
	}

private:
	CUtlBuffer m_compareBlob;
};

template< class T >
class CBaseVisualsDataProcessor : public IVisualsDataProcessor
{
public:
	CBaseVisualsDataProcessor()	{}

	virtual bool Compare( const CUtlBuffer &otherBuf ) { return GetCompareObject()->Compare( otherBuf ); }
	virtual IVisualsDataCompare *GetCompareObject() { return &m_compareObject; }
	virtual const char *GetPatternVTFName() const { return NULL; }

protected:
	T m_compareObject;
};

enum MaterialParamID_t
{
	MATERIAL_PARAM_ID_BASE_DIFFUSE_TEXTURE = 0,
	MATERIAL_PARAM_ID_PHONG_EXPONENT_TEXTURE,
	MATERIAL_PARAM_ID_BUMP_MAP,
	MATERIAL_PARAM_ID_ANISOTROPY_MAP,
	MATERIAL_PARAM_ID_MASKS1_MAP,

	MATERIAL_PARAM_ID_COUNT
};

extern const char g_szMaterialParamNames[MATERIAL_PARAM_ID_COUNT][32];

#endif // BASE_VISUALS_DATA_PROCESSOR_H
