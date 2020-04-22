//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
//=============================================================================


// Valve includes
#include "fbxsystem/ifbxsystem.h"
#include "tier0/dbg.h"

// Local includes
#include "fbxsystem.h"


// Last include
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_FBX_SYSTEM, "FbxSystem" );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CFbxSystem s_fbxSystem;
IFbxSystem *g_pFbx = &s_fbxSystem;


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CFbxSystem::CFbxSystem()
: m_pFbxManager( NULL )
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CFbxSystem::~CFbxSystem()
{
}


//-----------------------------------------------------------------------------
// Initialize the FBX SDK, hook up the pointer, FBX is started up on first use
// via CFbxSystem::GetFbxManager
//-----------------------------------------------------------------------------
InitReturnVal_t CFbxSystem::Init()
{
	return INIT_OK;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CFbxSystem::Shutdown()
{
	if ( m_pFbxManager )
	{
		m_pFbxManager->Destroy();
		m_pFbxManager = NULL;
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
FbxManager *CFbxSystem::GetFbxManager()
{
	if ( m_pFbxManager )
		return m_pFbxManager;

	m_pFbxManager = FbxManager::Create();
	if ( m_pFbxManager )
	{
		FbxIOSettings *pFbxIOSettings = FbxIOSettings::Create( m_pFbxManager, IOSROOT );
		if ( !pFbxIOSettings )
		{
			m_pFbxManager->Destroy();
			m_pFbxManager = NULL;
			Log_Error( LOG_FBX_SYSTEM, "Error! Cannot create FbxIOSettings\n" );
		}
		m_pFbxManager->SetIOSettings( pFbxIOSettings );

		// TODO: This can be slow... and currently no FBX plug-ins exist (must be looking at all files to see if they are plug-ins)
		// TODO: Do we need FBX plug-ins? - sApplicationPath will be d:/dev/main/game/bin, for example (wherever studiomdl.exe is)
		// TODO: If we need plug-ins in the future, then we should put them in a specific directory under game/bin maybe?
		/*
		const FbxString sApplicationPath = FbxGetApplicationDirectory();
		pFbxManager->LoadPluginsDirectory( sApplicationPath.Buffer() );
		*/
	}
	else
	{
		Log_Error( LOG_FBX_SYSTEM, "Error! Cannot create FbxManager\n" );
	}

	return m_pFbxManager;
}