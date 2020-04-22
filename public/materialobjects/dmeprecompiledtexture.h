//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// A class representing a procedural texture
//
//=============================================================================

#ifndef DMEPRECOMPILEDTEXTURE_H
#define DMEPRECOMPILEDTEXTURE_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "materialobjects/dmetexture.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeTexture;


//-----------------------------------------------------------------------------
// A Dme element that describes a texture processing operation
//-----------------------------------------------------------------------------
class CDmeTextureProcessor : public CDmElement
{
	DEFINE_ELEMENT( CDmeTextureProcessor, CDmElement );

public:
	virtual void ProcessTexture( CDmeTexture *pSrcTexture, CDmeTexture *pDstTexture ) = 0;
};


//-----------------------------------------------------------------------------
// A precompiled texture
//-----------------------------------------------------------------------------
class CDmePrecompiledTexture : public CDmElement
{
	DEFINE_ELEMENT( CDmePrecompiledTexture, CDmElement );

public:
	bool ValidateValues();

	template< class T > T *AddProcessor( const char *pName );
	template< class T > T *FindProcessor( const char *pName );

public:
	CDmaElementArray< CDmeTextureProcessor > m_Processors;
	CDmaElement< CDmeTexture > m_pSourceTexture;

	CDmaString m_ImageFileName;
	CDmaVar< int > m_nStartFrame;
	CDmaVar< int > m_nEndFrame;
	CDmaVar< int > m_nTextureArraySize;
	CDmaVar< int > m_nVolumeTextureDepth;
	CDmaVar< int > m_nTextureType;	// 0 = normal, 1 = cubemap
	CDmaVar< float > m_flBumpScale;
	CDmaVar< float > m_flPFMScale;
	CDmaVar< int > m_nFilterType;	// See DmeTextureFilter_t
	CDmaVar< bool > m_bClampS;
	CDmaVar< bool > m_bClampT;
	CDmaVar< bool > m_bClampU;
	CDmaVar< bool > m_bLoadMipLevels;
	CDmaVar< bool > m_bBorder;
	CDmaVar< bool > m_bNormalMap;	// FIXME: Should normal/normalga be combined?
	CDmaVar< bool > m_bSSBump;
	CDmaVar< bool > m_bNoMip;		// FIXME: Should nomip/allmips be combined?
	CDmaVar< bool > m_bAllMips;
	CDmaVar< bool > m_bNoLod;
	CDmaVar< bool > m_bNoDebugOverride;
	CDmaVar< bool > m_bNoCompression;
	CDmaVar< bool > m_bHintDxt5Compression;
};

template< class T > inline T *CDmePrecompiledTexture::AddProcessor( const char *pName )
{
	T* pProcessor = CreateElement< T >( pName, GetFileId() );
	m_Processors.AddToTail( pProcessor );
	return pProcessor;
}

template< class T > inline T *CDmePrecompiledTexture::FindProcessor( const char *pName )
{
	for ( int i = 0; i < m_Processors.Count(); ++i )
	{
		if ( !Q_stricmp( pName, m_Processors[i]->GetName() ) )
			return CastElement< T >( m_Processors[i] );
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Texture processors
//-----------------------------------------------------------------------------
class CDmeTP_ComputeMipmaps : public CDmeTextureProcessor
{
DEFINE_ELEMENT( CDmeTP_ComputeMipmaps, CDmeTextureProcessor );

public:
	virtual void ProcessTexture( CDmeTexture *pSrcTexture, CDmeTexture *pDstTexture );

	CDmaVar< bool > m_bNoNiceFiltering;
	CDmaVar< bool > m_bAlphaTestDownsampling;
	CDmaVar< float > m_flAlphaTestDownsampleThreshhold;
	CDmaVar< float > m_flAlphaTestDownsampleHiFreqThreshhold;
};


//-----------------------------------------------------------------------------
// Changes color channels
//-----------------------------------------------------------------------------
class CDmeTP_ChangeColorChannels : public CDmeTextureProcessor
{
	DEFINE_ELEMENT( CDmeTP_ChangeColorChannels, CDmeTextureProcessor );

public:
	virtual void ProcessTexture( CDmeTexture *pSrcTexture, CDmeTexture *pDstTexture );
	void ProcessImage( CDmeImage *pDstImage, CDmeImage *pSrcImage );

	CDmaVar< int > m_nMaxChannels;
};


#endif // DMEPRECOMPILEDTEXTURE_H
