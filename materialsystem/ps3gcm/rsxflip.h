//========== Copyright © 2010, Valve Corporation, All rights reserved. ========
#ifndef MATERIALSYSTEM_PS3GCM_RSXFLIP_HDR
#define MATERIALSYSTEM_PS3GCM_RSXFLIP_HDR

#ifndef _CERT
#define GCM_ALLOW_TIMESTAMPS 1
void OnFrameTimestampAvailableFlip( float ms );
void OnFrameTimestampAvailableRsx( float ms );
void OnFrameTimestampAvailableMain( float ms );
void OnFrameTimestampAvailableMST( float ms );
extern int32 g_ps3_timestampBeginIdx;
#endif

#include "ps3/ps3gcmmemory.h"

class RsxInterruptFifo
{
public:
	struct Event_t
	{
		uint8 m_nCause;
		uint8 m_nSurfaceFlipIdx;
		
	};
protected:
	enum { MAX_EVENT_COUNT = 0x80 };
	volatile uint m_nGet;
	uint m_nPut;
	Event_t m_queue[MAX_EVENT_COUNT];
public:
	void Init();

	uint Queue( uint8 nCause, uint8 nSurfaceFlipIdx );
	uint Queue( const Event_t &event );
	uint GetPutMarker()const;
	int HasEvents( uint nMarker );
	bool IsIdle()const { return m_nPut == m_nGet;}
	bool IsValidMarker( uint nMarker );
	Event_t & PeekEvent();
	const Event_t DequeueEvent( );
	void QueueRsxInterrupt();
};



class CFlipHandler
{
public:
	void Init();
	void Shutdown();
	void Flip();
	void BeginScene();
	void EndScene(){}
	bool OnRsxInterrupt( const RsxInterruptFifo::Event_t event );
	void TryFlipVblank();
	void TryPumpEvents( uint nMarker, uint isVblank );
	void QmsPrepareFlipSubmit( GcmUserCommandEnum_t nEvent, uint surfaceFlipIdx );
	bool QmsAdviceBeforeDrawPrevFramebuffer();
	void DisableMlaa(){ m_nMlaaFlagsThisFrame = 0; }
	void DisableMlaaPermannetly(){ m_nMlaaFlagMaskNextFrame = 0; }
	void EnableMlaaPermannetly(){ m_nMlaaFlagMaskNextFrame = ~0u; }
	//void DisableMlaaForTwoFrames(){ m_nMlaaFlagsThisFrame = m_nMlaaFlagMaskNextFrame = 0; }
	int IsMlaaEnabled()const { return m_nMlaaFlagsThisFrame; }
	
	enum DebugStateEnum_t
	{
		RENDERING_SURFACE,
		DISPLAYING_SURFACE,
		DEBUG_STATE_COUNT
	};

	//void OnState( int nState, int nValue );

public:
	static void INTERRUPT_VBlankHandler( const uint32 head );
	static void INTERRUPT_UserHandler( const uint32 cause );

	void PumpEventsUnsafe( uint nMarker );
	bool TryFlipSurface( uint isVblank );
protected:
	void TransferMlaaResultIfNecessary( uint nSurfacePrevFlipIdx );

public:
	//int m_nDebugStates[DEBUG_STATE_COUNT];


	// How often to present in terms of vblanks?
	// (@60Hz scanout TV: 1 = 60 Hz = every vblank, 2 = 30 Hz = every other vblank, 3 = 20 Hz = every 3rd vblank)
	// (@50Hz PAL TV: 1 = 50 Hz = every vblank, 2 = 25 Hz = every other vblank, 3 = 17 Hz = every 3rd vblank)
	int m_nPresentFrequency;

	// Interrupt-driven data
#ifdef GCM_ALLOW_TIMESTAMPS
	double m_flFlipImmediateTimestamp;
#endif
	double m_flVBlankTimestamp, m_flVBlankTimestamp0;

	// Mutex to sync with interrupt thread
	CThreadMutex m_mutexOfInterruptThread;
	CThreadManualEvent m_evFlipReady[ CPs3gcmDisplay::SURFACE_COUNT ];
	uint m_nFlipSurfaceIdx, m_nFlipSurfaceCount; // the next surface to flip, count of surfaces to flip
	uint m_nSystemFlipId[ CPs3gcmDisplay::SURFACE_COUNT ];
	//uint m_nLastFlippedSurfaceIdx; // used to check for duplicate TryFlip callbacks
	uint m_nVblankCounter;
	uint32 * m_pLastInterruptGet;
	RsxInterruptFifo m_interruptFifo;
	uint8 m_surfaceEdgePost[CPs3gcmDisplay::SURFACE_COUNT]; // true when the corresponding surface must be post-processed 
	// VSync enabled?
	// true		= Syncronize with VSync = true
	// false	= Syncronize with every HSync scanline
	bool m_bVSync;
	bool m_bEdgePostResultAlreadyInLocalMemory;
	int m_nMlaaFlagsThisFrame;
	int m_nMlaaFlagMaskNextFrame;
};

extern CFlipHandler g_flipHandler;




#endif