//================ Copyright (c) 1996-2010 Valve Corporation. All Rights Reserved. =================

#ifndef PS3GCMFUNC_H
#define PS3GCMFUNC_H

// this is the buffer that all PPU GCM functions assume is the normal command buffer,
// but it is not in IO-mapped memory and it's the SPU that picks up and submits it to RSX.
// it's a level of indirection necessary to interleave SPU and PPU calls to GCM
#define GCM_CTX gCellGcmCurrentContext



#if GCM_CTX_UNSAFE_MODE

#error "This mode is not supported any more. Use SPU draw mode."

#endif

extern int32_t SpuGcmCommandBufferReserveCallback( struct CellGcmContextData *context, uint32_t nCount );

#define GCM_CTX_RESERVE( WORDS ) SpuGcmCommandBufferReserveCallback( GCM_CTX, WORDS )

#define GCM_FUNC_NOINLINE( GCM_FUNCTION, ...) GCM_FUNCTION( GCM_CTX, ##__VA_ARGS__ )

#ifdef _CERT
#define GCM_PERF_RANGE( NAME )
#define GCM_PERF_PUSH_MARKER( NAME )
#define GCM_PERF_POP_MARKER( ) 
#define GCM_PERF_MARKER( NAME )
#else
class CGcmPerfAutoRange 
{
public:
	CGcmPerfAutoRange( const char * pName ){ GCM_FUNC_NOINLINE( cellGcmSetPerfMonPushMarker, pName ); }	
	~CGcmPerfAutoRange( ){ GCM_FUNC_NOINLINE( cellGcmSetPerfMonPopMarker ); }	
};
#define GCM_PERF_RANGE( NAME ) CGcmPerfAutoRange _gcmAutoRange( NAME )
#define GCM_PERF_PUSH_MARKER( NAME ) GCM_FUNC_NOINLINE( cellGcmSetPerfMonPushMarker, NAME )
#define GCM_PERF_POP_MARKER( ) GCM_FUNC_NOINLINE( cellGcmSetPerfMonPopMarker )
#define GCM_PERF_MARKER( NAME ) GCM_FUNC_NOINLINE( cellGcmSetPerfMonMarker, ( NAME ) )
#endif

#define GCM_FUNC( GCM_FUNCTION, ...)												   \
{																						\
	uint nReserveWords = GCM_FUNCTION ## MeasureSizeInline( 0, ##__VA_ARGS__ );			 \
	GCM_CTX_RESERVE( nReserveWords );													  \
	GCM_FUNCTION ## UnsafeInline( GCM_CTX, ##__VA_ARGS__ );									   \
}

extern void SpuGcmCommandBufferFlush();

#define GCM_CTX_FLUSH_CHECKPOINT() void SpuGcmCommandBufferFlush()

#define cellGcmFlush must_use_____g_ps3gcmGlobalState_CmdBufferFlush

#endif