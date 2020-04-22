//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: Provide interface to custom material manager
//
// $NoKeywords: $
//=============================================================================//

#ifndef I_CUSTOM_MATERIAL_MANAGER_H
#define I_CUSTOM_MATERIAL_MANAGER_H

#include "materialsystem/icustommaterial.h"
#include "materialsystem/icompositetexturegenerator.h"

class IVisualsDataCompare;
class KeyValues;

class ICustomMaterialManager
{
public:
	virtual bool Process() = 0;

	// get or create a custom material with the given attributes
	virtual ICustomMaterial *GetOrCreateCustomMaterial( KeyValues *pKeyValues, const CUtlVector< SCompositeTextureInfo > &vecTextureInfos, bool bIgnorePicMip = false ) = 0;

	virtual int DebugGetNumActiveCustomMaterials() = 0;
	virtual bool GetVMTKeyValues( const char *pszVMTName, KeyValues **ppVMTKeyValues ) = 0;
};

#endif // I_CUSTOM_MATERIAL_MANAGER_H
