//============ Copyright  Valve Corporation, All rights reserved. =============
//
// When embroidered patches are generated from sticker kits, we must wait
// for the textures to be generated before saving to disk.
//
//=============================================================================

#ifndef CS_CUSTOM_EMBROIDER_GENERATION_MGR_H
#define CS_CUSTOM_EMBROIDER_GENERATION_MGR_H

#include "igamesystem.h"
#include "materialsystem/icustommaterialmanager.h"
#include "utlvector.h"

struct CustomMaterialGenerationData_t
{
	ICustomMaterial *pCustomMaterial;
	char fileNames[COMBINER_MAX_TEXTURES_PER_MATERIAL][MAX_PATH];
	const char *pchChangeListName;
	int nTex;
};

class CCSCustomTextureSaver : public CAutoGameSystemPerFrame
{
public:
	CCSCustomTextureSaver() { m_bHasJob = false;  }
	~CCSCustomTextureSaver() {}

	virtual bool Init();
	virtual void Shutdown() {};
	void AddMaterialToWatch( CustomMaterialGenerationData_t embroiderMaterial );

private:

	virtual void Update( float frametime );

	CUtlVector<CustomMaterialGenerationData_t> m_materials;

	bool m_bHasJob;

};

#endif //CS_CUSTOM_EMBROIDER_GENERATION_MGR_H