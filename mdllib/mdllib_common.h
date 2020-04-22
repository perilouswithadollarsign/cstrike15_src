//====== Copyright © 1996-2007, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef MDLLIB_COMMON_H
#define MDLLIB_COMMON_H

#ifdef _WIN32
#pragma once
#endif

#include "mdllib/mdllib.h"

#include "platform.h"
#pragma warning( disable : 4018 )
#pragma warning( disable : 4389 )

DECLARE_LOGGING_CHANNEL( LOG_ModelLib );

//-----------------------------------------------------------------------------
// Purpose: Interface to accessing P4 commands
//-----------------------------------------------------------------------------
class CMdlLib : public CBaseAppSystem< IMdlLib >
{
public:
	// Destructor
	virtual ~CMdlLib();

	//////////////////////////////////////////////////////////////////////////
	//
	// Methods of IAppSystem
	//
	//////////////////////////////////////////////////////////////////////////
public:
	virtual bool Connect( CreateInterfaceFn factory );
	virtual InitReturnVal_t Init();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual void Shutdown();
	virtual void Disconnect();


	//////////////////////////////////////////////////////////////////////////
	//
	// Methods of IMdlLib
	//
	//////////////////////////////////////////////////////////////////////////
public:
	//
	// StripModelBuffers
	//	The main function that strips the model buffers
	//		mdlBuffer			- mdl buffer, updated, no size change
	//		vvdBuffer			- vvd buffer, updated, size reduced
	//		vtxBuffer			- vtx buffer, updated, size reduced
	//		ppStripInfo			- if nonzero on return will be filled with the stripping info
	//
	virtual bool StripModelBuffers( CUtlBuffer &mdlBuffer, CUtlBuffer &vvdBuffer, CUtlBuffer &vtxBuffer, IMdlStripInfo **ppStripInfo );

	//
	// CreateNewStripInfo
	//	Creates an empty strip info so that it can be reused.
	//
	virtual bool CreateNewStripInfo( IMdlStripInfo **ppStripInfo );

	//
	// ParseMdlMesh
	//	The main function that parses the mesh buffers
	//		mdlBuffer			- mdl buffer
	//		vvdBuffer			- vvd buffer
	//		vtxBuffer			- vtx buffer
	//		mesh				- on return will be filled with the mesh info
	//
	virtual bool ParseMdlMesh( CUtlBuffer &mdlBuffer, CUtlBuffer &vvdBuffer, CUtlBuffer &vtxBuffer, MdlLib::MdlMesh &mesh );

	//
	// PrepareModelForPs3
	//	The main function that strips the model buffers
	//		mdlBuffer			- mdl buffer, updated, size possibly increased
	//		vvdBuffer			- vvd buffer, updated, size possibly increased
	//		vtxBuffer			- vtx buffer, updated, size possibly increased
	//		ppStripInfo			- if nonzero on return will be filled with the stripping info
	//
	virtual bool PrepareModelForPs3( CUtlBuffer &mdlBuffer, CUtlBuffer &vvdBuffer, CUtlBuffer &vtxBuffer, IMdlStripInfo **ppStripInfo )
#if defined( PS3SDK_INSTALLED )
;
#else
	{
		return false;
	}
#endif
};

#endif // #ifndef MDLLIB_COMMON_H
