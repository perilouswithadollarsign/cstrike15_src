//========= Copyright  1996-2012, Valve Corporation, All rights reserved. ============//
//
// Purpose: Provide custom texture generation (compositing) for use on weapons and clothing
//
// $NoKeywords: $
//=============================================================================//

#ifndef CUSTOM_MATERIAL_H
#define CUSTOM_MATERIAL_H

#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/icustommaterial.h"
#include "materialsystem/icustommaterialmanager.h"
#include "convar.h"
#include "utlvector.h"
#include "utlmap.h"

//#define DISABLE_CUSTOM_MATERIAL_GENERATION


class IVisualsDataProcessor;
class IVisualsDataCompare;
class ICompositeTexture;
class CCompositeTexture;
class KeyValues;

//
// Custom Materials
//
// This handles making a material to contain composite textures,
// and it will be used during rendering of the model(s)
//

class CCustomMaterial : public ICustomMaterial
{
public:
	CCustomMaterial( KeyValues *pKeyValues );
	virtual ~CCustomMaterial();

	// inherited from ICustomMaterial

	// returns the actual end result material to be used for drawing the model
	virtual IMaterial *GetMaterial() { return m_Material; }  // the material reference converts itself into an IMaterial
	virtual void AddTexture( ICompositeTexture *pTexture );
	virtual ICompositeTexture *GetTexture( int nIndex );
	virtual bool IsValid() const { return m_bValid;	}
	virtual bool CheckRegenerate( int nSize );
	virtual const char* GetBaseMaterialName( void ) { return m_szBaseMaterialName; }
	
	// ----


	void SetBaseMaterialName( const char* szNmae );

	void Shutdown();
	void SetValid( bool bState ) { m_bValid = bState; }
	void Usage( int& nTextures, int& nMaterials );
	bool TexturesReady() const;
	void RegenerateTextures();
	bool ShouldRelease();

	bool Compare( const CUtlVector< SCompositeTextureInfo > &vecTextures );

	// actually creates the internal material that references the textures (which need to be finalized before calling this)
	void Finalize();

private:
	void CreateProceduralMaterial( const char *pMaterialName, KeyValues *pKeyValues );
	void DestroyProceduralMaterial();

	CMaterialReference m_Material;
 	CUtlVector< CCompositeTexture * > m_pTextures;

	bool m_bValid;
	int m_nModelMaterialIndex;
	KeyValues *m_pVMTKeyValues;
	const char *m_szBaseMaterialName;

	static int m_nMaterialCount;
};

class CCustomMaterialManager : public ICustomMaterialManager
{
public:
	CCustomMaterialManager();
	virtual ~CCustomMaterialManager();

	bool Init();
	void Shutdown();

	virtual bool Process();
	virtual ICustomMaterial *GetOrCreateCustomMaterial( KeyValues *pKeyValues, const CUtlVector< SCompositeTextureInfo > &vecTextureInfos, bool bIgnorePicMip = false  );

	virtual int DebugGetNumActiveCustomMaterials();
	virtual bool GetVMTKeyValues( const char *pszVMTName, KeyValues **ppVMTKeyValues );

private:
	CON_COMMAND_MEMBER_F( CCustomMaterialManager, "mat_reloadallcustommaterials", ReloadAllMaterials, "Reloads all custom materials", FCVAR_CHEAT | FCVAR_CLIENTDLL );
	CON_COMMAND_MEMBER_F( CCustomMaterialManager, "mat_custommaterialusage", Usage, "Show memory usage for custom weapon materials.", FCVAR_CLIENTDLL );
	CON_COMMAND_MEMBER_F( CCustomMaterialManager, "mat_reloadvmtcache", ReloadVmtCache, "Reload vmts from vmtcache.txt.", FCVAR_CHEAT | FCVAR_CLIENTDLL | FCVAR_DEVELOPMENTONLY );

	void DestroyMaterials();

	CUtlVector< CCustomMaterial * > m_pCustomMaterials;
	CUtlMap< const char *, KeyValues * > m_mapVMTKeyValues;
};

#endif // CUSTOM_MATERIAL_H
