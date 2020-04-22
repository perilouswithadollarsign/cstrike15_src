//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: Provide interface to the custom texture generator
//
// $NoKeywords: $
//=============================================================================//

#ifndef I_COMPOSITE_TEXTURE_GENERATOR_H
#define I_COMPOSITE_TEXTURE_GENERATOR_H

#include "materialsystem/icompositetexture.h"

class IVisualsDataProcessor;


enum CompositeTextureFormat_t
{
	COMPOSITE_TEXTURE_FORMAT_DXT1 = 1,
	COMPOSITE_TEXTURE_FORMAT_DXT5 = 5
};

enum CompositeTextureSize_t
{
	COMPOSITE_TEXTURE_SIZE_256 = 8,
	COMPOSITE_TEXTURE_SIZE_512,
	COMPOSITE_TEXTURE_SIZE_1024,
	COMPOSITE_TEXTURE_SIZE_2048
};

struct SCompositeTextureInfo
{
	IVisualsDataProcessor *m_pVisualsDataProcessor;
	CompositeTextureSize_t m_size;
	CompositeTextureFormat_t m_format;
	int m_nMaterialParamID;
	bool m_bSRGB;
};

class ICompositeTextureGenerator
{
public:
	virtual bool Process( void ) = 0;
	virtual ICompositeTexture *GetCompositeTexture( IVisualsDataProcessor *pVisualsDataProcessor, int nMaterialParamNameId, CompositeTextureSize_t size, CompositeTextureFormat_t format, bool bSRGB, bool bIgnorePicMip = false , bool bAllowCreate = true ) = 0;
	virtual ICompositeTexture *GetCompositeTexture( const SCompositeTextureInfo &textureInfo, bool bIgnorePicMip = false , bool bAllowCreate = true ) = 0;
	virtual bool ForceRegenerate( ICompositeTexture *pTexture ) = 0;
};

#endif // I_COMPOSITE_TEXTURE_GENERATOR_H
