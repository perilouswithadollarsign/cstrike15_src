//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef BVH_SUPPORT_H
#define BVH_SUPPORT_H

#include <tier0/platform.h>
#include <mathlib/vector.h>
#include <mathlib/vector4d.h>
#include <mathlib/mathlib.h>
#include <mathlib/vmatrix.h>

//--------------------------------------------------------------------------------------
// Temp material: this is all throwaway
//--------------------------------------------------------------------------------------
#define MAX_SHADER_NAME				48
#define MAX_BINDS					16

enum BindStage_t
{
	STAGE_VS						= 0,
	STAGE_HS						= 1,
	STAGE_DS						= 2,
	STAGE_GS						= 3,
	STAGE_PS						= 4
};

enum BindSampler_t
{
	SAMPLER_LINEAR					= 0,
	SAMPLER_POINT					= 1,
	SAMPLER_ANISO					= 2,
	SAMPLER_SHADOW_LESS				= 3
};

struct MaterialResourceBinding_t
{
	DECLARE_BYTESWAP_DATADESC();

	uint8							m_cBindStage;				// Which stage to bind to (VS,HS,DS,GS,PS)
	uint8							m_cBindSlot;				// Which slot to bind to in this stage
	uint8							m_cBindSampler;				// If this is a texture, this is the sampler enum to use
	//uint8							m_padding;					// don't worry about padding, because we're storing an array of 16 of them
};

struct Material_t
{
	DECLARE_BYTESWAP_DATADESC();

	char							m_szShaderVS[MAX_SHADER_NAME];
	char							m_szShaderPS[MAX_SHADER_NAME];
	int32							m_nBinds;
	MaterialResourceBinding_t		m_Binds[MAX_BINDS];

	float							m_flPhongExp;
	float							m_flPhongBoost;

	// Alloc these in blocks of 4 for alignment
	bool							m_bAlphaTest;				// Need to explicitly call out alpha-test so that we can bind textures during the shadow pass
	bool							m_bInstanced;				// use instancing for this material
	bool							m_bUseAtlas;				// use an atlas with an indirection texture
	bool							m_bVertexColor;				// don't use a texture map, color is in the vertices

	bool							m_bNormalMap;				// have a normal map
	bool							m_bPhong;					// use phong
	bool							m_bPhongTexture;			// pull phong exp from a texture
	bool							m_bPadding;
};

//--------------------------------------------------------------------------------------
// Tiled coordinate
//--------------------------------------------------------------------------------------
struct IntVector
{
	DECLARE_BYTESWAP_DATADESC();
	int x,y,z;
};

extern Vector g_vWorldUnitsPerTile;
struct TiledPosition_t
{
	DECLARE_BYTESWAP_DATADESC();

	IntVector						m_vTile;
	Vector							m_vLocal;

	void Rationalize()
	{
		Vector vFloor( floor( m_vLocal.x / g_vWorldUnitsPerTile.x ),
						 floor( m_vLocal.y / g_vWorldUnitsPerTile.y ),
						 floor( m_vLocal.z / g_vWorldUnitsPerTile.z ) );
		m_vTile.x += (int)vFloor.x;
		m_vTile.y += (int)vFloor.y;
		m_vTile.z += (int)vFloor.z;
		m_vLocal -= vFloor  * g_vWorldUnitsPerTile;
	}

	TiledPosition_t operator +( TiledPosition_t &Other )
	{
		TiledPosition_t retVal;
		retVal.m_vTile.x = m_vTile.x + Other.m_vTile.x;
		retVal.m_vTile.y = m_vTile.y + Other.m_vTile.y;
		retVal.m_vTile.z = m_vTile.z + Other.m_vTile.z;
		retVal.m_vLocal = m_vLocal + Other.m_vLocal;
		retVal.Rationalize();
		return retVal;
	}

	TiledPosition_t operator -( TiledPosition_t &Other )
	{
		TiledPosition_t retVal;
		retVal.m_vTile.x = m_vTile.x - Other.m_vTile.x;
		retVal.m_vTile.y = m_vTile.y - Other.m_vTile.y;
		retVal.m_vTile.z = m_vTile.z - Other.m_vTile.z;
		retVal.m_vLocal = m_vLocal - Other.m_vLocal;
		retVal.Rationalize();
		return retVal;
	}

	Vector ToWorldPosition()
	{
		Vector vTile( m_vTile.x, m_vTile.y, m_vTile.z );
		return vTile * g_vWorldUnitsPerTile + m_vLocal;
	}
};


#include "frustum.h"

//--------------------------------------------------------------------------------------
// generic buffer for variable sized vertices
//--------------------------------------------------------------------------------------
class CGenericBuffer
{
public:
	CGenericBuffer( int nElementStride = 0, int nGrowElements = 1024, int nInitElements = 1024 ) :
	  m_pMemory( NULL ),
	  m_nAllocBytes( 0 )
	{
		Init( nElementStride, nGrowElements, nInitElements );
	}
	~CGenericBuffer()
	{
		Purge();
	}

	void Init( int nElementStride, int nGrowElements = 1024, int nInitElements = 1024 )
	{
		Purge();

		m_nElementStride = nElementStride;
		m_nGrowBytes = nGrowElements * nElementStride;

		m_nAllocBytes = nInitElements * nElementStride;
		m_pMemory = new unsigned char[ nInitElements * nElementStride ];

	}

	int GetElementStride() const { return m_nElementStride; }
	unsigned char *Base() const { return m_pMemory; }
	int ByteCount() const { return m_nElements * m_nElementStride; }
	int Count() const { return m_nElements; }
	int AddToTail( unsigned char* pElement )
	{
		EnsureAddSize( 1 );

		Q_memcpy( m_pMemory + m_nElements * m_nElementStride, pElement, m_nElementStride );
		m_nElements++;
		return m_nElements-1;
	}
	int AddMultipleToTail( int nCount, unsigned char* pElements )
	{
		EnsureAddSize( nCount );

		Q_memcpy( m_pMemory + m_nElements * m_nElementStride, pElements, nCount * m_nElementStride );
		m_nElements += nCount;
		return m_nElements-1;
	}
	int AddMultipleToTail( int nCount )
	{
		EnsureAddSize( nCount );

		Q_memset( m_pMemory + m_nElements * m_nElementStride, 0, nCount * m_nElementStride );
		m_nElements += nCount;
		return m_nElements-1;
	}

	void RestrideElements( int nNewStride )
	{
		uint8 *pNewMemory = new uint8[ nNewStride * m_nElements ];
		int nMinStride = MIN( nNewStride, m_nElementStride );

		uint8 *pNewStart = pNewMemory;
		for ( int i=0; i<m_nElements; ++i )
		{
			Q_memcpy( pNewStart, m_pMemory + i * m_nElementStride, nMinStride );
			pNewStart += nNewStride;
		}

		delete []m_pMemory;
		m_pMemory = pNewMemory;
		m_nElementStride = nNewStride;
		m_nAllocBytes = nNewStride * m_nElements;
	}

	void RemoveAll()
	{
		m_nElements = 0;
	}
	void Purge()
	{
		if ( m_pMemory )
			delete []m_pMemory;
		m_pMemory = NULL;
		
		m_nElements = 0;
		m_nAllocBytes = 0;
	}

	unsigned char* operator[]( int i )
	{
		return m_pMemory + i * m_nElementStride;
	}

private:
	void Resize( int nNeededBytes )
	{
		unsigned char *pNewMemory = new unsigned char[ nNeededBytes ];
		Q_memcpy( pNewMemory, m_pMemory, m_nAllocBytes );
		m_nAllocBytes = nNeededBytes;

		delete []m_pMemory;
		m_pMemory = pNewMemory;
	}

	inline void EnsureAddSize( int nElementsToAdd )
	{
		int nUsedBytes = m_nElements * m_nElementStride;
		int nNeededBytes = nUsedBytes + nElementsToAdd * m_nElementStride;

		if ( m_nAllocBytes < nNeededBytes )
		{
			Resize( MAX( nNeededBytes, nUsedBytes + m_nGrowBytes ) );
		}
	}

private:
	unsigned char					*m_pMemory;
	int								m_nElementStride;
	int								m_nElements;
	int								m_nAllocBytes;

	int								m_nGrowBytes;
};

//--------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------
#endif
