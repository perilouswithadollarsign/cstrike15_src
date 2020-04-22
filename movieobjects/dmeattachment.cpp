//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Dme version of a model attachment point
//
//=============================================================================
#include "movieobjects/dmeattachment.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "tier1/keyvalues.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Statics
//-----------------------------------------------------------------------------
IMaterial *CDmeAttachment::sm_pMatAttachment = NULL;


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAttachment, CDmeAttachment );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeAttachment::OnConstruction()
{
	m_bIsRigid.Init( this, "isRigid" );
	m_bIsWorldAligned.Init( this, "isWorldAligned" );

	if ( !g_pMaterialSystem )
		return;

	if ( !sm_pMatAttachment )
	{
		KeyValues *pVMTKeyValues = new KeyValues( "wireframe" );
		pVMTKeyValues->SetInt( "$vertexcolor", 1 );
		pVMTKeyValues->SetInt( "$ignorez", 0 );
		sm_pMatAttachment = g_pMaterialSystem->CreateMaterial( "__DmeAttachment", pVMTKeyValues );
		if ( sm_pMatAttachment )
		{
			m_MatRefAttachment.Init( sm_pMatAttachment );
			sm_pMatAttachment->DecrementReferenceCount();	// CreateMaterial adds a ref, just want the CMaterialReference's

			// Cache material now to avoid an unwanted implicit Ref that occurs on first use which is never cleared
			g_pMaterialSystem->CacheUsedMaterials();
		}
	}
	else
	{
		m_MatRefAttachment.Init( sm_pMatAttachment );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAttachment::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// For rendering joints
//-----------------------------------------------------------------------------
#define AXIS_SIZE 6.0f


//-----------------------------------------------------------------------------
// Rendering method for the dag
//-----------------------------------------------------------------------------
void CDmeAttachment::Draw( const matrix3x4_t &shapeToWorld, CDmeDrawSettings *pDrawSettings /* = NULL */ )
{
	if ( !g_pMaterialSystem )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadMatrix( shapeToWorld );

	pRenderContext->Bind( sm_pMatAttachment );
	IMesh *pMesh = pRenderContext->GetDynamicMesh();

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, 3 );

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.Color4ub( 255, 0, 0, 255 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 0>();

	meshBuilder.Position3f( AXIS_SIZE, 0.0f, 0.0f );
	meshBuilder.Color4ub( 255, 0, 0, 255 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 0>();

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.Color4ub( 0, 255, 0, 255 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 0>();

	meshBuilder.Position3f( 0.0f, AXIS_SIZE, 0.0f );
	meshBuilder.Color4ub( 0, 255, 0, 255 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 0>();

	meshBuilder.Position3f( 0.0f, 0.0f, 0.0f );
	meshBuilder.Color4ub( 0, 0, 255, 255 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 0>();

	meshBuilder.Position3f( 0.0f, 0.0f, AXIS_SIZE );
	meshBuilder.Color4ub( 0, 0, 255, 255 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 0>();

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();
}