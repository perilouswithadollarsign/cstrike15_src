//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose:  
//
// $NoKeywords: $
//=============================================================================//

#ifndef IVISUALSDATAPROCESSOR_H
#define IVISUALSDATAPROCESSOR_H

#include "refcount.h"

class KeyValues;
class CUtlBuffer;

//
// Visuals Data Interface
//
// This abstracts the visuals data processing so that the main composite texture code 
// can be used for weapons, clothing, or whatever.
// You need to implement one of these for use with CCompositeTexture.
//

class IVisualsDataCompare
{
public:
	virtual void FillCompareBlob() = 0;
	virtual const CUtlBuffer &GetCompareBlob() const = 0;
	virtual bool Compare( const CUtlBuffer &pOther ) = 0;
};

class IVisualsDataProcessor : public CRefCounted<>
{
public:
	IVisualsDataProcessor() {}

	virtual KeyValues *GenerateCustomMaterialKeyValues() = 0;
	virtual KeyValues *GenerateCompositeMaterialKeyValues( int nMaterialParamId ) = 0;
	virtual IVisualsDataCompare *GetCompareObject() = 0;
	virtual bool HasCustomMaterial() const = 0;
	virtual const char* GetOriginalMaterialName() const = 0;
	virtual const char* GetOriginalMaterialBaseName() const = 0;
	virtual const char* GetPatternVTFName() const = 0;
	virtual void Refresh() = 0;

protected:
	virtual ~IVisualsDataProcessor() {}
};

#endif // IVISUALSDATAPROCESSOR_H
