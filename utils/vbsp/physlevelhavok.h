//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYS_LEVEL_HAVOK_H
#define PHYS_LEVEL_HAVOK_H

#include "platform.h"
#include "basetypes.h"
#include "datalinker.h"
#include "bspfile.h"
#include "bitvec.h"

#include "vphysics2_interface.h"

extern byte               *g_pPhysLevel;
extern int                 g_PhysLevelSize;

class CPhysLevelHavokEmitter: public DataLinker::Stream
{
public:
	CPhysLevelHavokEmitter();
	~CPhysLevelHavokEmitter();

	void Emit();

protected:
	void AddBrushes(int headnode, CVarBitVec &isBrushAdded);
	void ConvertBrushesToPolytopes(CVarBitVec &useBrushIn, CUtlVector<IPhysics2CookedPolytope*> &arrPolytopesOut);
protected:
public:
	bool            m_bConvertBrushesToMopp, m_buildInertia, m_exportObjMopp;

	int             m_numModels;
	dmodel_t       *m_pModels;
	dnode_t        *m_pNodes;
	dleaf_t        *m_pLeafs;
	dbrush_t       *m_pBrushes;
	dbrushside_t   *m_pBrushSides;
	int             m_numBrushes;
	dplane_t       *m_pPlanes;
	unsigned short *m_pLeafBrushes;

	IPhysics2 *m_physics;
	IPhysics2Cook *m_cook;
};

#endif