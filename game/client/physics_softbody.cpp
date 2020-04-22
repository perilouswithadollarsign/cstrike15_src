//========= Copyright © Valve Corporation, All rights reserved. ============//
#include "cbase.h"
#include "physics_softbody.h"

CSoftbodyEnvironment g_SoftbodyEnvironment;
static CSoftbodyProcess s_ClothSystemProcess;

void CSoftbodyProcess::OnRestore()
{
	// ToDo: implement softbody reset-all 
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output :	
//-----------------------------------------------------------------------------
void CSoftbodyProcess::Update( float frametime )
{
	g_SoftbodyEnvironment.Step( frametime );
}

static RnDebugDrawOptions_t s_SoftbodyDebugDrawOptions( RN_SOFTBODY_DRAW_EDGES | RN_SOFTBODY_DRAW_POLYGONS );

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output :	
//-----------------------------------------------------------------------------
void CSoftbodyProcess::PostRender( )
{
#ifdef _DEBUG
	if ( softbody_debug.GetInt() <= 1 )
	{
		return;
	}

	if ( IMaterial *pDebugWireframeMaterial = materials->FindMaterial( "debug/softbodywire.vmt", TEXTURE_GROUP_OTHER, true ) )
	{
		CMatRenderContextPtr	pRenderContext( materials );
		//IMaterial *pDebugSolidMaterial = materials->FindMaterial( "debug/clothsolid.vmt", TEXTURE_GROUP_OTHER, true );
		pRenderContext->Bind( pDebugWireframeMaterial );
		IMesh* pMatMesh = pRenderContext->GetDynamicMesh();

		for ( int i = 0; i < g_SoftbodyEnvironment.GetSoftbodyCount(); ++i )
		{
			g_SoftbodyEnvironment.GetSoftbody( i )->Draw( s_SoftbodyDebugDrawOptions, pMatMesh );
		}
		pMatMesh->Draw();
	}
#endif
}

