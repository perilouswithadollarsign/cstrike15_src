//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: Physics2 interfaces to vphysics DLL
//
// $NoKeywords: $
//=============================================================================//

#ifndef VPHYSICS2_INTERFACE_VERSION_FLAGS_H
#define VPHYSICS2_INTERFACE_VERSION_FLAGS_H

enum Physics2FlagEnum
{
	PHYSICS2_IS_ACTOR_STATIC = 1,
	PHYSICS2_IS_ACTOR_FAST = 1 << 1,

	// assert that Release() call is final
	PHYSICS2_ASSERT_RELEASE_IS_FINAL = 1
};

enum Physics2CookedMeshTypes
{
	PHYSICS2_MESH_TYPE_POLYTOPE,
	PHYSICS2_MESH_TYPE_POLYSOUP,
	PHYSICS2_MESH_TYPE_MOPP,
	PHYSICS2_MESH_TYPE_CUSTOM
};

enum Physics2StockShapes
{
	PHYSICS2_STOCK_SPHERE_1M,
	PHYSICS2_STOCK_BOX_1M,
	PHYSICS2_STOCK_SHAPE_COUNT
};

enum Physics2CreateWorldFlags
{
	PHYSICS2_CREATE_CLIENT_WORLD = 1<<0,
	PHYSICS2_CREATE_SERVER_WORLD = 1<<1
};




#endif