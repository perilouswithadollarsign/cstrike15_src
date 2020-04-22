//===== Copyright © Valve Corporation, All rights reserved. ======//
#ifndef IMESH_DECLARATIONS_HDR
#define IMESH_DECLARATIONS_HDR

enum SpuMeshRenderDataEnum{ MAX_SPU_MESHRENDERDATA = 2, MAX_SPU_PRIM_DATA = 128 };


typedef struct
{
	int			primFirstIndex;
	int			primNumIndices;
} SPUPrimRenderData;

typedef struct
{
	/*GfxPrimType*/int				primType;
	MaterialPrimitiveType_t materialType;
	SPUPrimRenderData		primData[ MAX_SPU_PRIM_DATA ];
	uint32						numPrims;
	uint32						firstIndex;

} SPUMeshRenderData;



#endif