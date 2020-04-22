//===== Copyright © 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: interface to asynchronous lighting preview creator
//
//===========================================================================//

#ifndef LPREVIEW_THREAD_H
#define LPREVIEW_THREAD_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/threadtools.h"
#include "bitmap/bitmap.h"
#include "bitmap/floatbitmap.h"
#include "tier1/utlvector.h"
#include "mathlib/lightdesc.h"
#include "tier1/utlintrusivelist.h"





class CLightingPreviewLightDescription : public LightDesc_t
{
public:
	CLightingPreviewLightDescription *m_pNext;
	CUtlVector<CLightingPreviewLightDescription*> m_TempChildren;


	int m_nObjectID;
	float m_flJitterAmount;									// for area lights - how much to
															// randomly perturb light pos when
															// tracing


	class CIncrementalLightInfo *m_pIncrementalInfo;
	bool m_bLowRes;											// whether to generate at 1/16 screen res
	bool m_bDidIndirect;									// whether or not we tried to generate pseudo lights yet

	CLightingPreviewLightDescription( void )
	{
		m_flJitterAmount = 0;
		m_bLowRes = true;
		m_bDidIndirect = false;
	}
	

	void Init( int obj_id )
	{
		m_pNext = NULL;
		m_pIncrementalInfo = NULL;
		m_nObjectID = obj_id;
		m_bDidIndirect = false;
	}
};

enum HammerToLightingPreviewMessageType
{
	// messages from hammer to preview task
	LPREVIEW_MSG_STOP,									// no lighting previews open - stop working
	LPREVIEW_MSG_EXIT,										// we're exiting program - shut down
	LPREVIEW_MSG_GEOM_DATA,									  // we have new shadow geometry data
	LPREVIEW_MSG_G_BUFFERS,							 // we have new g buffer data from the renderer
	LPREVIEW_MSG_LIGHT_DATA,								// new light data in m_pLightList
};

enum LightingPreviewToHammerMessageType
{
	// messages from preview task to hammer
	LPREVIEW_MSG_DISPLAY_RESULT,							// we have a result image
};


struct MessageToLPreview
{
	HammerToLightingPreviewMessageType m_MsgType;

	MessageToLPreview( HammerToLightingPreviewMessageType mtype)
	{
		m_MsgType = mtype;
	}
	MessageToLPreview( void)
	{
	}

	// this structure uses a fat format for the args instead of separate classes for each
	// message. the messages are small anyway, since pointers are used for anything of size.
	FloatBitMap_t *m_pDefferedRenderingBMs[4];				// if LPREVIEW_MSG_G_BUFFERS
	CUtlIntrusiveList<CLightingPreviewLightDescription> m_LightList;	// if LPREVIEW_MSG_LIGHT_DATA
	Vector m_EyePosition;									// for LPREVIEW_MSG_LIGHT_DATA & G_BUFFERS
	CUtlVector<Vector> *m_pShadowTriangleList;				// for LPREVIEW_MSG_GEOM_DATA
	int m_nBitmapGenerationCounter;							// for LPREVIEW_MSG_G_BUFFERS

};

struct MessageFromLPreview
{
	LightingPreviewToHammerMessageType m_MsgType;
	Bitmap_t *m_pBitmapToDisplay;							// for LPREVIEW_MSG_DISPLAY_RESULT
	int m_nBitmapGenerationCounter;							// for LPREVIEW_MSG_DISPLAY_RESULT
	MessageFromLPreview( LightingPreviewToHammerMessageType msgtype )
	{
		m_MsgType = msgtype;
	}
	MessageFromLPreview( void )
	{
	}

};

extern CMessageQueue<MessageToLPreview> g_HammerToLPreviewMsgQueue;
extern CMessageQueue<MessageFromLPreview> g_LPreviewToHammerMsgQueue;
extern ThreadHandle_t g_LPreviewThread;

extern CInterlockedInt n_gbufs_queued;
extern CInterlockedInt n_result_bms_queued;

extern Bitmap_t *g_pLPreviewOutputBitmap;

// the lighting preview thread entry point

unsigned LightingPreviewThreadFN( void *thread_start_arg );

// the lighting preview handler. call often
void HandleLightingPreview( void );

#endif

