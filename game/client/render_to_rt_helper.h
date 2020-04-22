//===== Copyright © 2013-2013, Valve Corporation, All rights reserved. ======//
//
// Purpose: Class to help with rendering CMDLs & CMergedMDLs to textures.
//
//===========================================================================//


#ifndef RENDER_TO_RT_HELPER_H
#define RENDER_TO_RT_HELPER_H

#if defined( _WIN32 )
#pragma once
#endif

#include "utllinkedlist.h"
#include "irendertorthelperobject.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IVTFTexture;
class ITexture;
class CThreadEvent;

class CRenderToRTHelper	: public IRenderToRTHelper
{
public:
	CRenderToRTHelper();
	virtual ~CRenderToRTHelper();

	// Methods of IRenderToRTHelper
	virtual bool Init();
	virtual void Shutdown();
	virtual RenderToRTData_t *CreateRenderToRTData( IRenderToRTHelperObject *pObject, IVTFTexture *pResultVTF );
	virtual void StartRenderToRT( RenderToRTData_t *pRendertoRTData );
	virtual void DestroyRenderToRTData( RenderToRTData_t *pRendertoRTData );
	virtual bool Process();

	void LookAt( Camera_t &camera, const Vector &vecCenter, float flRadius, QAngle cameraAngles, Vector cameraOffset );

private:
	CUtlLinkedList< RenderToRTData_t * > m_objectsToRender;
	RenderToRTData_t *m_pCurrentObjectToRender;
	MaterialLightingState_t m_LightingState;	

	CThreadEvent *m_pPixelsReadEvent;
	ITexture *m_pRenderTarget;
};

#endif // RENDER_TO_RT_HELPER_H
