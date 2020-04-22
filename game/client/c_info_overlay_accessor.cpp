//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "materialsystem/imesh.h"
#include "toolframework_client.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// -------------------------------------------------------------------------------- //
// An entity used to access overlays (and change their texture)
// -------------------------------------------------------------------------------- //
class C_InfoOverlayAccessor : public C_BaseEntity
{
public:

	DECLARE_CLASS( C_InfoOverlayAccessor, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_InfoOverlayAccessor();

	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void GetToolRecordingState( KeyValues *msg );

	void RestoreToToolRecordedState( KeyValues *pKV );
	void DestroyToolRecording( void );

private:

	int		m_iOverlayID;
};

// Expose it to the engine.
IMPLEMENT_CLIENTCLASS(C_InfoOverlayAccessor, DT_InfoOverlayAccessor, CInfoOverlayAccessor);

BEGIN_RECV_TABLE_NOBASE(C_InfoOverlayAccessor, DT_InfoOverlayAccessor)
	RecvPropInt(RECVINFO(m_iTextureFrameIndex)),
	RecvPropInt(RECVINFO(m_iOverlayID)),
END_RECV_TABLE()


// -------------------------------------------------------------------------------- //
// Functions.
// -------------------------------------------------------------------------------- //

C_InfoOverlayAccessor::C_InfoOverlayAccessor()
{
}

void C_InfoOverlayAccessor::OnDataChanged( DataUpdateType_t updateType )
{
	if ( updateType == DATA_UPDATE_CREATED )
	{
		// Update overlay's bind proxy
		engine->SetOverlayBindProxy( m_iOverlayID, GetClientRenderable() );
	}
}


void C_InfoOverlayAccessor::GetToolRecordingState( KeyValues *msg )
{
	BaseClass::GetToolRecordingState( msg );

	KeyValues *pKV = CIFM_EntityKeyValuesHandler_AutoRegister::FindOrCreateNonConformantKeyValues( msg );
	pKV->SetString( CIFM_EntityKeyValuesHandler_AutoRegister::GetHandlerIDKeyString(), "C_InfoOverlayAccessor" );

	pKV->SetInt( "entIndex", index );

	pKV->SetInt( "overlayID", m_iOverlayID );
	pKV->SetInt( "textureFrame", GetTextureFrameIndex() );


	//mark entity as visible so we'll get playback (even though we're invisible, the overlay we talk to isn't)
	{
		BaseEntityRecordingState_t dummyState;
		BaseEntityRecordingState_t *pState = (BaseEntityRecordingState_t *)msg->GetPtr( "baseentity", &dummyState );
		pState->m_bVisible = true;		
	}
}

void C_InfoOverlayAccessor::RestoreToToolRecordedState( KeyValues *pKV )
{
	m_iOverlayID = pKV->GetInt( "overlayID" );
	SetTextureFrameIndex( pKV->GetInt( "textureFrame" ) );
	engine->SetOverlayBindProxy( m_iOverlayID, GetClientRenderable() );
}

void C_InfoOverlayAccessor::DestroyToolRecording( void )
{
	engine->SetOverlayBindProxy( m_iOverlayID, NULL );
}



class C_InfoOverlayAccessor_NonConformantDataHandler : public CIFM_EntityKeyValuesHandler_RecreateEntities
{
public:
	C_InfoOverlayAccessor_NonConformantDataHandler( void ) 
		: CIFM_EntityKeyValuesHandler_RecreateEntities( "C_InfoOverlayAccessor" )
	{ }

	virtual void *CreateInstance( void )
	{
		return new C_InfoOverlayAccessor;
	}

	virtual void DestroyInstance( void *pEntity )
	{
		C_InfoOverlayAccessor *pCastEntity = (C_InfoOverlayAccessor *)pEntity;
		//clienttools->RemoveClientRenderable( pCastEntity );
		pCastEntity->DestroyToolRecording();
		delete pCastEntity;
	}

	virtual void HandleInstance( void *pEntity, KeyValues *pKeyValues )
	{
		C_InfoOverlayAccessor *pCastEntity = (C_InfoOverlayAccessor *)pEntity;
		pCastEntity->RestoreToToolRecordedState( pKeyValues );
		//if( pCastEntity->RenderHandle() == INVALID_CLIENT_RENDER_HANDLE )
		//{
		//	clienttools->AddClientRenderable( pCastEntity, false, RENDERABLE_IS_TRANSLUCENT );
		//}
		//clienttools->MarkClientRenderableDirty( pCastEntity );
	}
};

static C_InfoOverlayAccessor_NonConformantDataHandler s_InfoOverlayAccessorEntityIFMHandler;
