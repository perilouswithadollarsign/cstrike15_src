//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "physlevelhavok.h"
#include "alignedarray.h"
#include "bsplib.h"
#include "physdll.h"
#include "vbsp.h"

CPhysLevelHavokEmitter::CPhysLevelHavokEmitter()
{
	m_numModels     = nummodels;
	m_pModels       = dmodels;
	m_pNodes        = dnodes;
	m_numBrushes    = numbrushes;
	m_pLeafs        = dleafs;
	m_pLeafBrushes  = dleafbrushes;
	m_pBrushes      = dbrushes;
	m_pBrushSides   = dbrushsides;
	m_pPlanes       = dplanes;

	m_bConvertBrushesToMopp = !g_bPhysNoMopp;
	m_buildInertia = false;
	m_exportObjMopp = !!g_bPhysExportMoppObj;

	m_physics = NULL;

	CreateInterfaceFn physicsFactory = GetPhysicsFactory();
	if ( physicsFactory )
	{
		m_physics = (IPhysics2*)physicsFactory( VPHYSICS2_INTERFACE_VERSION, NULL );
	}

	if ( !m_physics )
		Warning("!!! WARNING: Can't build collision2 data!\n" );

	m_cook = m_physics->GetCook();
}

CPhysLevelHavokEmitter::~CPhysLevelHavokEmitter()
{
}


// TODO: move this to Cook interface
void CPhysLevelHavokEmitter::Emit()
{
	CVarBitVec useBrush(m_numBrushes);
	for ( int i = 0; i < m_numModels; ++i )
	{
		dmodel_t *pModel = &m_pModels[i];
		AddBrushes(pModel->headnode, useBrush);
	}

	CUtlVector<IPhysics2CookedPolytope *> arrPolytopes;
	ConvertBrushesToPolytopes(useBrush, arrPolytopes);

	dphyslevelV0_t *pRoot = Write<dphyslevelV0_t>();
	pRoot->toolVersion = 2;
	pRoot->dataVersion = m_physics->GetSerializeVersion();
	pRoot->sizeofDiskPhysics2Polytope = sizeof(DiskPhysics2Polytope_t);
	pRoot->buildTime = (int)time(NULL);

	bool outputIndividualPolytopes = true;
	// having the number of polytopes, we need to generate MOPP
	if(m_bConvertBrushesToMopp)
	{
		int numPolytopes = arrPolytopes.Count();
		CUtlVector<IPhysics2CookedMeshBase*>arrMeshes(0,numPolytopes);
		for(int i = 0; i < numPolytopes; ++i)
			arrMeshes[i] = arrPolytopes[i]; // conver the pointers
		if(m_exportObjMopp)m_cook->ExportObj("d:\\mopp.obj", arrMeshes.Base(), numPolytopes);

		IPhysics2CookedMopp *pMopp = m_cook->CookMopp(arrMeshes.Base(), numPolytopes);
		if(pMopp)
		{
			Msg("Physics2Mopp cooked, mem size: %.1f\n", pMopp->GetSizeOf()/1024.);

			void *pSerializedMopp = m_cook->Serialize(pMopp->GetMopp(), this);
			IStream::Link(&pRoot->mopp, pSerializedMopp);
			outputIndividualPolytopes = false; // we don't need to output individual polytopes, because MOPP will contain them separately
			m_cook->Destroy(pMopp);
		}
		else
			Warning("Could not build polysoup from brushes\n");
	}

	if(false)
	{
		int numPolytopes = arrPolytopes.Count();
		CUtlVector<IPhysics2CookedMeshBase*>arrMeshes(0,numPolytopes);
		for(int i = 0; i < numPolytopes; ++i)
			arrMeshes[i] = arrPolytopes[i]; // conver the pointers
		IPhysics2CookedPolysoup *polysoup = m_cook->CookPolysoupFromMeshes(arrMeshes.Base(), numPolytopes);
		if(polysoup)
		{
			//polysoup->ExportObj("d:\\test.obj");
			m_cook->Destroy(polysoup);
		}
	}

	int numPolytopes = arrPolytopes.Count();
	DiskPhysics2Polytope_t *pDiskPolytopes = NULL;
	if(outputIndividualPolytopes)
		pDiskPolytopes = WriteAndLinkArray(&pRoot->polytopes,numPolytopes);

	for(int nPolytope = 0; nPolytope < numPolytopes; ++nPolytope)
	{
		IPhysics2CookedPolytope *pPolytope = arrPolytopes[nPolytope];
		if(outputIndividualPolytopes)
		{
			void *pSerializedPolytope = m_cook->Serialize(pPolytope->GetPolytope(), this);
			Link(&pDiskPolytopes[nPolytope].offsetPolytope, pSerializedPolytope);

			if(m_buildInertia)
			{
				IPhysics2CookedInertia *pInertia = m_cook->CookInertia(pPolytope->GetPolytope());
				void* pSerializedInertia = m_cook->Serialize(pInertia->GetInertia(), this);
				Link(&pDiskPolytopes[nPolytope].offsetInertia, pSerializedInertia);
				m_cook->Destroy(pInertia);
			}
		}		
		m_cook->Destroy(pPolytope);
	}
}





void CPhysLevelHavokEmitter::ConvertBrushesToPolytopes(CVarBitVec &useBrushIn, CUtlVector<IPhysics2CookedPolytope*> &arrPolytopesOut)
{
	uint numCouldntCreate = 0;
	for( int nBrush = 0; nBrush < m_numBrushes; ++nBrush )
	{
		if(useBrushIn[nBrush])
		{
			int numBrushSides = m_pBrushes[nBrush].numsides;
			CUtlVector_Vector4DAligned arrSides(numBrushSides);

			dbrushside_t *pThisBrushSides = m_pBrushSides + m_pBrushes[nBrush].firstside;
			for(int nSide = 0; nSide < numBrushSides; ++nSide)
			{
				dbrushside_t *pSide = pThisBrushSides + nSide;
				if(!pSide->bevel)
				{
					dplane_t &plane = m_pPlanes[pSide->planenum];
					Vector4DAligned side;
					side.Init(plane.normal.x,plane.normal.y,plane.normal.z, -plane.dist);
					arrSides.AddToTail(side);
				}
			}
			IPhysics2CookedPolytope *pPolytope = m_cook->CookPolytopeFromPlanes((Vector4DAligned*)arrSides.Base(), arrSides.Size());
			if(pPolytope)
			{
				arrPolytopesOut.AddToTail(pPolytope);
			}
			else
			{
				//Warning("Couldn't create a convex out of brush #%d\n", nBrush);
				++numCouldntCreate;
			}
		}
	}

	if(numCouldntCreate)
		Warning("Couldn't create %u/%u brushes\n", numCouldntCreate, m_numBrushes);
	else
		Msg("Created %u brushes\n", m_numBrushes);
}


void CPhysLevelHavokEmitter::AddBrushes(int nNode, CVarBitVec &isBrushAdded)
{
	if(nNode < 0)
	{
		int leafIndex = -1 - nNode;
		for ( int i = 0; i < m_pLeafs[leafIndex].numleafbrushes; ++i )
		{
			int brushIndex = m_pLeafBrushes[m_pLeafs[leafIndex].firstleafbrush + i];
			isBrushAdded.Set(brushIndex);
		}
	}
	else
	{
		dnode_t *pNode = m_pNodes + nNode;

		AddBrushes( pNode->children[0] , isBrushAdded );
		AddBrushes( pNode->children[1] , isBrushAdded );
	}
}

// This is the only public entry to this file.
// The global data touched in the file is:
// from bsplib.h:
//		g_pPhysLevel		: This is an output from this file.
//		g_PhysLevelSize	: This is set in this file.
//		g_dispinfo			: This is an input to this file.
//		int  nummodels;
//      dmodel_t dmodels[MAX_MAP_MODELS];
//		numnodewaterdata	: This is an input to this file from EmitPhysCollision()
//		dleafwaterdata		: This is an input to this file from EmitPhysCollision()
// from vbsp.h:
//		g_SurfaceProperties : This is an input to this file.
void EmitPhysLevel()
{
	if(g_pPhysLevel)
		MemAlloc_FreeAligned(g_pPhysLevel);
	CPhysLevelHavokEmitter emitter;
	emitter.Emit();

	g_PhysLevelSize = emitter.GetTotalSize();
	g_pPhysLevel = (byte*)MemAlloc_AllocAligned(g_PhysLevelSize, 16);
	if(!emitter.Compile(g_pPhysLevel))
	{
		free(g_pPhysLevel);
		g_pPhysLevel = NULL;
		g_PhysLevelSize = 0;
	}
	else
	{//	Msg("Compiled PhysLevel: %.1fk\n", g_PhysLevelSize / 1024.0);
		emitter.PrintStats();
	}
}
