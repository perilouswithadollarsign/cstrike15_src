//====== Copyright c 1996-20013, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef IRENDERCAPTURECONFIGURATION_H
#define IRENDERCAPTURECONFIGURATION_H
#ifdef _WIN32
#pragma once
#endif


class ITexture;
class IVRenderView;


//////////////////////////////////////////////////////////////////////////
//
// Generic interface to capture shadow in a render target
//

class CRenderCaptureConfigurationState
{
public:
	FlashlightState_t m_renderFlashlightState;
	VMatrix m_renderMatrixWorldToShadow;
	ITexture *m_pFlashlightDepthTexture;
	ITexture *m_pDummyColorBufferTexture;
	IVRenderView *m_pIVRenderView;
};


#endif // IRENDERCAPTURECONFIGURATION_H

