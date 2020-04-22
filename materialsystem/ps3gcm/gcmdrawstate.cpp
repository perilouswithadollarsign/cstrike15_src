//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
// Per draw call gcm state
//
//==================================================================================================

#define PPU_DRAW 0

#ifndef SPU
#define CELL_GCM_MEMCPY memcpy						// PPU SNC has no such intrinsic
#endif


#ifndef SPU

#include "sys/memory.h"
#include "sysutil/sysutil_sysparam.h"
#include "cell/sysmodule.h"

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier1/utlbuffer.h"

#include "cell/gcm.h"
#include "gcmconfig.h"
#include "ps3gcmmemory.h"
#include "gcmstate.h"
#include "gcmlabels.h"
#include "gcmdrawstate.h"

#include "ps3/ps3_helpers.h"

#include <materialsystem/imaterialsystem.h>

#include <vprof.h>

#include "tier0/memdbgon.h"

#else

#include "spumgr_spu.h"
#include "gcmdrawstate.h"

#endif 

//--------------------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------------------

ALIGN128 CGcmDrawState gGcmDrawState[GCM_DRAWSTATE_MAX] ALIGN128_POST;
CGcmDrawState* gpGcmDrawState = &gGcmDrawState[0];

int g_bZcullAuto					= 1;
int g_nZcullDefault					= 100;
int g_nZcullMoveForward				= 100;
int g_nZcullPushBack				= 100;

SetVertexDataArrayCache_t			g_cacheSetVertexDataArray[ D3D_MAX_STREAMS ];
vec_float4							g_aFPConst[GCM_DS_MAXFPCONST] = {0,};
vec_float4							g_aVPConst[GCM_DS_MAXVPCONST] = {0,};
D3DStreamDesc						g_dxGcmVertexStreamSources[D3D_MAX_STREAMS];

uint32								g_UPHigh = 0;
uint32								g_UPFrame;

#ifndef SPU
ALIGN16 uint8						g_aDynECB[GCM_DS_MAXDYNECB] ALIGN16_POST;			// Ring buffer of dynamic cmds
uint32								g_nDynECBIdx = 0;
#endif

#ifndef SPU
ALIGN128 CGcmDrawState::FixedData	gFixedData[GCM_DRAWSTATE_MAX] ALIGN128_POST;
#else
ALIGN128 CGcmDrawState::FixedData	gFixedData[1] ALIGN128_POST;
#endif

#ifndef SPU
ALIGN128 uint8	gPackData[GCM_DRAWSTATE_MAX][GCM_DS_MAXDATAPERDRAWCALL] ALIGN128_POST;
#else
ALIGN128 uint8	gPackData[1][GCM_DS_MAXDATAPERDRAWCALL] ALIGN128_POST;
#endif

//--------------------------------------------------------------------------------------------------
// DX lookups etc..
//--------------------------------------------------------------------------------------------------

// THese tables are auto-generated in dxabstract.cpp, UnpackD3DRSITable()
// They provide renderstate classes and their default values....
uint8 g_d3drs_defvalue_indices[D3DRS_VALUE_LIMIT] =
{ 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0300 | 1, 0300 | 2, 0100 | 3, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0300 | 1, 0300 | 0, 0100 | 4, 0000 | 0, 0000 | 0, 0300 | 1, 0300 | 1, 0000 | 0, 0300 | 2, 0300 | 1, 0300 | 0, 0300 | 5, 0100 | 0, 0300 | 1, 0300 | 0, 0100 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0300 | 0, 0300 | 0, 0300 | 0, 0300 | 6, 0300 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0300 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0300 | 0, 0300 | 4, 0300 | 4, 0300 | 4, 0300 | 7, 0300 | 0, 0300 | 8, 0300 | 8, 0100 | 8, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0000 | 0, 0100 | 0, 0100 | 0, 0100 | 0, 0100 | 0, 0100 | 0, 0100 | 0, 0100 | 0, 0100 | 0, 0200 | 4, 0000 | 0, 0000 | 0, 0100 | 0, 0300 | 0, 0100 | 4, 0100 | 4, 0100 | 0, 0000 | 0, 0100 | 0, 0100 | 3, 0100 | 0, 0100 | 0, 0000 | 0, 0000 | 0, 0100 | 0, 0300 | 0, 0000 | 0, 0100 | 6, 0100 | 6, 0100 | 0, 0100 | 0, 0100 | 6, 0100 | 0, 0100 | 0, 0300 | 4, 0300 | 8, 0100 | 0, 0000 | 0, 0100 | 0, 0100 | 9, 0100 | 0, 0300 | 4, 0000 | 0, 0100 | 0, 0300 | 4, 0100 | 2, 0100 | 4, 0300 | 0, 0300 | 0, 0100 | 0, 0000 | 0, 0100 | 6, 0100 | 6, 0100 | 0, 0100 | 0, 0100 | 6, 0100 | 0, 0100 | 0, 0300 | 0, 0300 | 4, 0300 | 4, 0300 | 4, 0300 | 7, 0300 | 10, 0300 | 10, 0300 | 10, 0100 | 8, 0300 | 0, 0300 | 0, 0000 | 0, 0000 | 0, 0100 | 0, 0100 | 0, 0100 | 0, 0100 | 0, 0100 | 0, 0100 | 0, 0100 | 0, 0100 | 0, 0100 | 0, 0100 | 3, 0100 | 4, 0100 | 4};
uint32 g_d3drs_defvalues[11] =
{ 0x0, 0x31415926, 0x3, 0x2, 0x1, 0x7, 0x3F800000, 0x8, 0xFFFFFFFF, 0x42800000, 0xF };

uint16 dxtogl_blendop[7] =
{
	/*invalid*/CELL_GCM_FUNC_ADD,
	CELL_GCM_FUNC_ADD,
	CELL_GCM_FUNC_SUBTRACT,
	CELL_GCM_FUNC_REVERSE_SUBTRACT,
	CELL_GCM_MIN,
	CELL_GCM_MAX,
	/*invalid*/CELL_GCM_FUNC_ADD,
};

uint32 dxtogl_stencilmode[10] =
{
	/*invalid*/					CELL_GCM_KEEP,
	/*D3DSTENCILOP_KEEP*/		CELL_GCM_KEEP,
	/*D3DSTENCILOP_ZERO*/		CELL_GCM_ZERO,
	/*D3DSTENCILOP_REPLACE*/	CELL_GCM_REPLACE,
	/*D3DSTENCILOP_INCRSAT*/	CELL_GCM_INCR,
	/*D3DSTENCILOP_DECRSAT*/	CELL_GCM_DECR,
	/*D3DSTENCILOP_INVERT*/		CELL_GCM_INVERT,
	/*D3DSTENCILOP_INCR*/		CELL_GCM_INCR_WRAP,
	/*D3DSTENCILOP_DECR*/		CELL_GCM_DECR_WRAP,
	/*invalid*/					CELL_GCM_KEEP,
};


// addressing modes
// 1 D3DTADDRESS_WRAP		Tile the texture at every integer junction.
//   D3DTADDRESS_MIRROR	Similar to D3DTADDRESS_WRAP, except that the texture is flipped at every integer junction.
// 3 D3DTADDRESS_CLAMP	Texture coordinates outside the range [0.0, 1.0] are set to the texture color at 0.0 or 1.0, respectively.
// 4 D3DTADDRESS_BORDER	Texture coordinates outside the range [0.0, 1.0] are set to the border color.
//   D3DTADDRESS_MIRRORONCE Similar to D3DTADDRESS_MIRROR and D3DTADDRESS_CLAMP.
//						Takes the absolute value of the texture coordinate (thus, mirroring around 0),
//						and then clamps to the maximum value. The most common usage is for volume textures,
//						where support for the full D3DTADDRESS_MIRRORONCE texture-addressing mode is not
//						necessary, but the data is symmetric around the one axis.
uint8 dxtogl_addressMode[6] = 
{
	CELL_GCM_TEXTURE_WRAP,				// no zero entry
	CELL_GCM_TEXTURE_WRAP,				// from D3DTADDRESS_WRAP
	CELL_GCM_TEXTURE_MIRROR,			// from D3DTADDRESS_MIRROR
	CELL_GCM_TEXTURE_CLAMP_TO_EDGE,		// from D3DTADDRESS_CLAMP
	CELL_GCM_TEXTURE_BORDER,			// from D3DTADDRESS_BORDER
	CELL_GCM_TEXTURE_MIRROR_ONCE_BORDER, // no D3DTADDRESS_MIRRORONCE support
};


uint8 dxtogl_anisoIndexHalf[32] = // indexed by [ dxsamp->maxAniso / 2 ]
{
	CELL_GCM_TEXTURE_MAX_ANISO_1, // 0-1
	CELL_GCM_TEXTURE_MAX_ANISO_2, // 2-3
	CELL_GCM_TEXTURE_MAX_ANISO_4, // 4-5
	CELL_GCM_TEXTURE_MAX_ANISO_6, // 6-7
	CELL_GCM_TEXTURE_MAX_ANISO_8, // 8-9
	CELL_GCM_TEXTURE_MAX_ANISO_10, // 10-11
	CELL_GCM_TEXTURE_MAX_ANISO_12, // 12-13
	CELL_GCM_TEXTURE_MAX_ANISO_16, // 14-15
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 16
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 18
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 20
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 22
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 24
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 26
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 28
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 30
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 32
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 34
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 36
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 38
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 40
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 42
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 44
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 46
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 48
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 50
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 52
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 54
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 56
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 58
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 60
	CELL_GCM_TEXTURE_MAX_ANISO_16, // ... rest >= 62
};

uint8 dxtogl_minFilter[4][4] =		// indexed by _D3DTEXTUREFILTERTYPE on both axes: [row is min filter][col is mip filter]. 
{
	/*  mip filter ---------------> D3DTEXF_NONE	D3DTEXF_POINT				D3DTEXF_LINEAR				(D3DTEXF_ANISOTROPIC not applicable to mip filter) */
	/* min = D3DTEXF_NONE */		{	CELL_GCM_TEXTURE_NEAREST,		CELL_GCM_TEXTURE_NEAREST_NEAREST,	CELL_GCM_TEXTURE_NEAREST_LINEAR,	CELL_GCM_TEXTURE_NEAREST	},		// D3DTEXF_NONE we just treat like POINT
	/* min = D3DTEXF_POINT */		{	CELL_GCM_TEXTURE_NEAREST,		CELL_GCM_TEXTURE_NEAREST_NEAREST,	CELL_GCM_TEXTURE_NEAREST_LINEAR,	CELL_GCM_TEXTURE_NEAREST	},
	/* min = D3DTEXF_LINEAR */		{	CELL_GCM_TEXTURE_LINEAR,		CELL_GCM_TEXTURE_LINEAR_NEAREST,	CELL_GCM_TEXTURE_LINEAR_LINEAR,		CELL_GCM_TEXTURE_NEAREST	},
	/* min = D3DTEXF_ANISOTROPIC */	{	CELL_GCM_TEXTURE_LINEAR,		CELL_GCM_TEXTURE_LINEAR_NEAREST,	CELL_GCM_TEXTURE_LINEAR_LINEAR,		CELL_GCM_TEXTURE_NEAREST	},		// no diff from prior row, set maxAniso to effect the sampling
};

uint8 dxtogl_magFilter[4] =		// indexed by _D3DTEXTUREFILTERTYPE
{
	CELL_GCM_TEXTURE_NEAREST,				// D3DTEXF_NONE not applicable to mag filter but we handle it like POINT (mat_showmiplevels hits this)
	CELL_GCM_TEXTURE_NEAREST,				// D3DTEXF_POINT
	CELL_GCM_TEXTURE_LINEAR,				// D3DTEXF_LINEAR
	CELL_GCM_TEXTURE_LINEAR,				// D3DTEXF_ANISOTROPIC (aniso will be driven by setting maxAniso, not by a GL filter mode)
};

//--------------------------------------------------------------------------------------------------
// Send to SPU
//--------------------------------------------------------------------------------------------------

#ifndef SPU

int gSpuJobIssued = 0;
uint32 gSpuStartIdx = 0;
uint32 gSpuCount = 0;

//--------------------------------------------------------------------------------------------------
// SPU DRAW CODE
//--------------------------------------------------------------------------------------------------

#if !PPU_DRAW

void	CGcmDrawState::SendToSpu()
{
	SpuTaskHandle *pTask = &g_ps3gcmGlobalState.m_spuHandle;
    
    // Get this drawcall indx and the next
    uint32 idx = gpGcmDrawState - gGcmDrawState;
    uint32 nextidx = (idx + 1) % GCM_DRAWSTATE_MAX;
    gSpuCount ++;

	// Move gpGcmDrawState to the next set of Data
	
	CGcmDrawState* pPrevDrawState = gpGcmDrawState;
	gpGcmDrawState = &gGcmDrawState[nextidx];
	
	gpGcmDrawState->m_shaderVxConstants = pPrevDrawState->m_shaderVxConstants;
	gpGcmDrawState->m_pPixelShaderData = pPrevDrawState->m_pPixelShaderData;
	gpGcmDrawState->m_pVertexShaderData = pPrevDrawState->m_pVertexShaderData;

	gpGcmDrawState->m_nBackBufferSize[0] = pPrevDrawState->m_nBackBufferSize[0];
	gpGcmDrawState->m_nBackBufferSize[1] = pPrevDrawState->m_nBackBufferSize[1];

	gpGcmDrawState->m_pDataCursor = gpGcmDrawState->m_pData;

	gpGcmDrawState->m_dirtySamplersMask = 0;
	gpGcmDrawState->m_dirtyCachesMask = 0;
	gpGcmDrawState->m_dirtyStatesMask = 0;

	gpGcmDrawState->m_nFreeLabel = 0;

	memset(gpGcmDrawState->m_pFixed->m_aSamplerIdx, 0xff, sizeof(m_pFixed->m_aSamplerIdx));
	gpGcmDrawState->m_pFixed->m_nSampler = 0;
	gpGcmDrawState->m_pFixed->m_nInstanced = 0;

	gpGcmDrawState->m_nNumECB = 0;
	memset(gpGcmDrawState->m_aECB, 0, sizeof(m_aECB));

    if ( (gSpuCount < 4) && (m_cmd != CmdEndFrame) ) return;
 
    // Send the state(s) to the SPU

    // Wait on previous drawcall

    if (gSpuJobIssued)
    {
        uint32 fifoPosn;
        gSpuMgr.ReadMailbox(pTask, &fifoPosn);
        gpGcmContext->current = (uint32*)fifoPosn;
    }

    // Makesure we have 16K at least, per drawcall (we issue 4 calls at a time)
    cellGcmReserveMethodSizeInline(gpGcmContext, (GCM_DS_FIFOPERDRAW*GCM_NUMDRAWCALLS_SPU)/4);			// 16K per draw call, /4 because api takes wordcount

    // Makesure FIFO is on a 16B boundary

    while (uintp(gpGcmContext->current) & 0xf)
    {
        *gpGcmContext->current = 0;
        gpGcmContext->current++;
    }

    // Build count and startidx parameter to send to SPU
    uint32 mailboxparam = (gSpuCount<<16) | gSpuStartIdx;

    //Send this drawstate
    m_eaOutputFIFO = (uint32)gpGcmContext->current;
    __asm ( "eieio" );
    gSpuMgr.WriteMailbox(pTask, mailboxparam);

    gSpuJobIssued = 1;

    // If it's an endframe, wait for result now
    // comment out this if to always wait for the dma to come back

    if (m_cmd == CmdEndFrame)
    {
        uint32 fifoPosn;
        gSpuMgr.ReadMailbox(pTask, &fifoPosn);
        gpGcmContext->current = (uint32*)fifoPosn;
        gSpuJobIssued = 0;
    }

    gSpuStartIdx = nextidx;
    gSpuCount = 0;

}

#else		// PPU_DRAW.....

//--------------------------------------------------------------------------------------------------
// Draw on PPU
//--------------------------------------------------------------------------------------------------

void	CGcmDrawState::SendToSpu()
{

	// Makesure we have 16K at least

	cellGcmReserveMethodSizeInline(gpGcmContext, GCM_DS_FIFOPERDRAW/4);			// 16K per draw call

	// Makesure FIFO is on a 16B boundary

	while (uintp(gpGcmContext->current) & 0xf)
	{
		*gpGcmContext->current = 0;
		gpGcmContext->current++;
	}

	// Process cmd on PPU

	switch (m_cmd)
	{

	case CmdCommitStates:
	case CmdEndFrame:

		if (m_nFreeLabel) UnpackSetWriteBackEndLabel(GCM_LABEL_MEMORY_FREE, m_nFreeLabel);

		if ( m_dirtyStatesMask & kDirtyResetRsx) UnpackResetRsxState();

		if (m_dirtyStatesMask & kDirtyZeroAllPSConsts) ZeroFPConsts();
		if (m_dirtyStatesMask & kDirtyZeroAllVSConsts) ZeroVPConsts();

		UnpackData();					// Pulls out pixel shader consts and sets vertex shader consts
		CommitRenderStates();
		break;

	case CmdDrawPrim:
		{
			gpGcmDrawState->CommitAll((IDirect3DVertexDeclaration9 *)m_param[0], m_param[1]);

			// Draw

			GCM_FUNC( cellGcmSetDrawIndexArray, 
				m_param[2], m_param[5],
				CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16, CELL_GCM_LOCATION_LOCAL,
				m_param[3] );
		}
		break;

	case CmdDrawPrimUP:
		{

			D3DStreamDesc &dsd = g_dxGcmVertexStreamSources[0];

			dsd.m_offset = 0;
			dsd.m_stride = m_param[2];
			dsd.m_vtxBuffer = ( IDirect3DVertexBuffer9 * )( uintp )1; // invalid pointer, but non-NULL to signal it's a real vertex buffer;
			dsd.m_nLocalBufferOffset = 0;

			gpGcmDrawState->CommitAll((IDirect3DVertexDeclaration9 *)m_param[0], 0);
			GCM_FUNC(cellGcmSetCallCommand, m_param[1]);

		}

		break;
	}

	// Flip to the other set of Data

	if (gpGcmDrawState->m_pData == gPackData1)
	{
		gpGcmDrawState->m_pData = gPackData2;
		gpGcmDrawState->m_pFixed = &gFixedData2;
	}
	else
	{
		gpGcmDrawState->m_pData = gPackData1;
		gpGcmDrawState->m_pFixed = &gFixedData1;
	}

	gpGcmDrawState->m_pDataCursor = gpGcmDrawState->m_pData;

	m_dirtySamplersMask = 0;
	m_dirtyCachesMask = 0;
	m_dirtyStatesMask = 0;

	m_nFreeLabel = 0;

	memset(m_pFixed->m_aSamplerIdx, 0xff, sizeof(m_pFixed->m_aSamplerIdx));
	m_pFixed->m_nSampler = 0;
	m_pFixed->m_nInstanced = 0;

	m_nNumECB = 0;
	memset(m_aECB, 0, sizeof(m_aECB));
}

#endif		// ndef SPU

#endif



//--------------------------------------------------------------------------------------------------
// test func to try to find corrupted ECBs
//--------------------------------------------------------------------------------------------------

void CGcmDrawState::TestCommandBuffer( uint8 *pCmdBuf )
{
	uint8* pStart = pCmdBuf;

	uint8 *pReturnStack[20];
	uint8 **pSP = &pReturnStack[ARRAYSIZE(pReturnStack)];
	uint8 *pLastCmd;
	for(;;)
	{
		uint8 *pCmd=pCmdBuf;
		int nCmd = GetData<int>( pCmdBuf );

		if (nCmd > CBCMD_SET_VERTEX_SHADER_NEARZFARZ_STATE) DebuggerBreak();

		switch( nCmd )
		{
		case CBCMD_END:
			{
				if ( pSP == &pReturnStack[ARRAYSIZE(pReturnStack)] )
					return;
				else
				{
					// pop pc
					pCmdBuf = *( pSP ++ );
					break;
				}
			}

		case CBCMD_JUMP:
			pCmdBuf = GetData<uint8 *>(  pCmdBuf + sizeof( int ) );
			break;

		case CBCMD_JSR:
			{
				Assert( pSP > &(pReturnStack[0] ) );
				// 				*(--pSP ) = pCmdBuf + sizeof( int ) + sizeof( uint8 *);
				// 				pCmdBuf = GetData<uint8 *>(  pCmdBuf + sizeof( int ) );
				TestCommandBuffer( GetData<uint8 *>(  pCmdBuf + sizeof( int ) ) );
				pCmdBuf = pCmdBuf + sizeof( int ) + sizeof( uint8 *);
				break;
			}

		case CBCMD_SET_PIXEL_SHADER_FLOAT_CONST:
			{
				int nStartConst = GetData<int>( pCmdBuf + sizeof( int ) );
				int nNumConsts = GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				pCmdBuf += nNumConsts * 4 * sizeof( float ) + 3 * sizeof( int );
				break;
			}


		case CBCMD_SETPIXELSHADERFOGPARAMS:
			{
				Error("Pixel Shader Fog params not supported\n");
				break;
			}

		case CBCMD_STORE_EYE_POS_IN_PSCONST:
			{
				pCmdBuf += 2 * sizeof( int ) + sizeof( float );

				break;
			}

		case CBCMD_SET_DEPTH_FEATHERING_CONST:
			{
				// 				int nConst = GetData<int>( pCmdBuf + sizeof( int ) );
				// 				float fDepthBlendScale = GetData<float>( pCmdBuf + 2 * sizeof( int ) );
				pCmdBuf += 2 * sizeof( int ) + sizeof( float );
				//				SetDepthFeatheringPixelShaderConstant( nConst, fDepthBlendScale );
				break;
			}

		case CBCMD_SET_VERTEX_SHADER_FLOAT_CONST:
			{
				int nStartConst = GetData<int>( pCmdBuf + sizeof( int ) );
				int nNumConsts = GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				float const *pValues = reinterpret_cast< float const *> ( pCmdBuf + 3 * sizeof( int ) );
				pCmdBuf += nNumConsts * 4 * sizeof( float ) + 3 * sizeof( int );
				break;
			}


		case CBCMD_BIND_PS3_TEXTURE:
			{
				CPs3BindTexture_t tex = GetData<CPs3BindTexture_t> (pCmdBuf + sizeof( int ));
				if (tex.m_pLmBlock->Offset() & 0x7e) DebuggerBreak();
				pCmdBuf += sizeof(int) + sizeof(tex);
				break;
			}


		case CBCMD_BIND_PS3_STANDARD_TEXTURE:
			{
				CPs3BindTexture_t tex = GetData<CPs3BindTexture_t> (pCmdBuf + sizeof( int ));

				if (m_pFixed->m_nInstanced)
				{
					uint32 nBindFlags = tex.m_nBindFlags;
					uint32 nSampler   = tex.m_sampler;

					switch (tex.m_boundStd)
					{
					case TEXTURE_LOCAL_ENV_CUBEMAP:
						if (m_pFixed->m_nInstanced & GCM_DS_INST_ENVMAP) tex = m_pFixed->m_instanceEnvCubemap;
						break;
					case TEXTURE_LIGHTMAP:
						if (m_pFixed->m_nInstanced & GCM_DS_INST_LIGHTMAP) tex = m_pFixed->m_instanceLightmap;
						break;
					case TEXTURE_PAINT:
						if (m_pFixed->m_nInstanced & GCM_DS_INST_PAINTMAP) tex = m_pFixed->m_instancePaintmap;
						break;
					}

					tex.m_nBindFlags = nBindFlags;
					tex.m_sampler    = nSampler;
				}

				// Test texture

				if (tex.m_pLmBlock->Offset() & 0x7e) DebuggerBreak();

				pCmdBuf += sizeof(int) + sizeof(tex);
				break;
			}


		case CBCMD_PS3TEX:
			{
				pCmdBuf += sizeof(int) + (CBCMD_MAX_PS3TEX*sizeof(int));
				break;
			}

		case CBCMD_LENGTH:
			{
				pCmdBuf += sizeof(int) *2 ;
				break;
			}

		case CBCMD_SET_PSHINDEX:
			{
				// 				int nIdx = GetData<int>( pCmdBuf + sizeof( int ) );
				// 				ShaderManager()->SetPixelShaderIndex( nIdx );
				// 				pCmdBuf += 2 * sizeof( int );

				Error("PSHINDEX Not Supported\n");
				break;
			}

		case CBCMD_SET_VSHINDEX:
			{
				// 				int nIdx = GetData<int>( pCmdBuf + sizeof( int ) );
				// 				ShaderManager()->SetVertexShaderIndex( nIdx );
				pCmdBuf += 2 * sizeof( int );

				Error("VSHINDEX Not Supported\n");
				break;
			}

		case CBCMD_SET_VERTEX_SHADER_FLASHLIGHT_STATE:
			{
				// 				int nStartConst = GetData<int>( pCmdBuf + sizeof( int ) );
				// 				SetVertexShaderConstantInternal( nStartConst, m_FlashlightWorldToTexture.Base(), 4, false );
				// 				pCmdBuf += 2 * sizeof( int );

				//				Error("Flashlight unsupported\n");

				pCmdBuf += 2 * sizeof( int );
				break;
			}

		case CBCMD_SET_VERTEX_SHADER_NEARZFARZ_STATE:
			{
				Error("SetVertexShaderNearAndFarZ NOt SUPPORTED\n");

				// 				int nStartConst = GetData<int>( pCmdBuf + sizeof( int ) );
				// 
				// 				VMatrix m;
				// 				
				// 				m = m_MaterialProjectionMatrix;
				// 
				// //				GetMatrix( MATERIAL_PROJECTION, m.m[0] );
				// 
				// 				// m[2][2] =  F/(N-F)   (flip sign if RH)
				// 				// m[3][2] = NF/(N-F)
				// 
				// 				float vNearFar[4];
				// 
				// 				float N =     m[3][2] / m[2][2];
				// 				float F = (m[3][2]*N) / (N + m[3][2]);
				// 
				// 				vNearFar[0] = N;
				// 				vNearFar[1] = F;
				// 
				// 				SetVertexShaderConstantInternal( nStartConst, vNearFar, 1, false );
				pCmdBuf += 2 * sizeof( int );
				break;
			}

		case CBCMD_SET_PIXEL_SHADER_FLASHLIGHT_STATE:
			{
				// 				int nLightSampler		= GetData<int>( pCmdBuf + sizeof( int ) );
				// 				int nDepthSampler		= GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				// 				int nShadowNoiseSampler = GetData<int>( pCmdBuf + 3 * sizeof( int ) );
				// 				int nColorConst			= GetData<int>( pCmdBuf + 4 * sizeof( int ) );
				// 				int nAttenConst			= GetData<int>( pCmdBuf + 5 * sizeof( int ) );
				// 				int nOriginConst		= GetData<int>( pCmdBuf + 6 * sizeof( int ) );
				// 				int nDepthTweakConst	= GetData<int>( pCmdBuf + 7 * sizeof( int ) );
				// 				int nScreenScaleConst	= GetData<int>( pCmdBuf + 8 * sizeof( int ) );
				// 				int nWorldToTextureConstant = GetData<int>( pCmdBuf + 9 * sizeof( int ) );
				// 				bool bFlashlightNoLambert = GetData<int>( pCmdBuf + 10 * sizeof( int ) ) != 0;
				// 				bool bSinglePassFlashlight = GetData<int>( pCmdBuf + 11 * sizeof( int ) ) != 0;
				// 				pCmdBuf += 12 * sizeof( int );
				// 
				// 				ShaderAPITextureHandle_t hTexture = g_pShaderUtil->GetShaderAPITextureBindHandle( m_FlashlightState.m_pSpotlightTexture, m_FlashlightState.m_nSpotlightTextureFrame, 0 );
				// 				BindTexture( (Sampler_t)nLightSampler, TEXTURE_BINDFLAGS_SRGBREAD, hTexture ); // !!!BUG!!!srgb or not?
				// 
				// 				SetPixelShaderConstantInternal( nAttenConst, m_pFlashlightAtten, 1, false );
				// 				SetPixelShaderConstantInternal( nOriginConst, m_pFlashlightPos, 1, false );
				// 
				// 				m_pFlashlightColor[3] = bFlashlightNoLambert ? 2.0f : 0.0f; // This will be added to N.L before saturate to force a 1.0 N.L term
				// 
				// 				// DX10 hardware and single pass flashlight require a hack scalar since the flashlight is added in linear space
				// 				float flashlightColor[4] = { m_pFlashlightColor[0], m_pFlashlightColor[1], m_pFlashlightColor[2], m_pFlashlightColor[3] };
				// 				if ( ( g_pHardwareConfig->UsesSRGBCorrectBlending() ) || ( bSinglePassFlashlight ) )
				// 				{
				// 					// Magic number that works well on the 360 and NVIDIA 8800
				// 					flashlightColor[0] *= 2.5f;
				// 					flashlightColor[1] *= 2.5f;
				// 					flashlightColor[2] *= 2.5f;
				// 				}
				// 
				// 				SetPixelShaderConstantInternal( nColorConst, flashlightColor, 1, false );
				// 
				// 				if ( nWorldToTextureConstant >= 0 )
				// 				{
				// 					SetPixelShaderConstantInternal( nWorldToTextureConstant, m_FlashlightWorldToTexture.Base(), 4, false );
				// 				}
				// 
				// 				BindStandardTexture( (Sampler_t)nShadowNoiseSampler, TEXTURE_BINDFLAGS_NONE, TEXTURE_SHADOW_NOISE_2D );
				// 				if( m_pFlashlightDepthTexture && m_FlashlightState.m_bEnableShadows && ShaderUtil()->GetConfig().ShadowDepthTexture() )
				// 				{
				// 					ShaderAPITextureHandle_t hDepthTexture = g_pShaderUtil->GetShaderAPITextureBindHandle( m_pFlashlightDepthTexture, 0, 0 );
				// 					BindTexture( (Sampler_t)nDepthSampler, TEXTURE_BINDFLAGS_SHADOWDEPTH, hDepthTexture );
				// 
				// 					SetPixelShaderConstantInternal( nDepthTweakConst, m_pFlashlightTweaks, 1, false );
				// 
				// 					// Dimensions of screen, used for screen-space noise map sampling
				// 					float vScreenScale[4] = {1280.0f / 32.0f, 720.0f / 32.0f, 0, 0};
				// 					int nWidth, nHeight;
				// 					BaseClass::GetBackBufferDimensions( nWidth, nHeight );
				// 
				// 					int nTexWidth, nTexHeight;
				// 					GetStandardTextureDimensions( &nTexWidth, &nTexHeight, TEXTURE_SHADOW_NOISE_2D );
				// 
				// 					vScreenScale[0] = (float) nWidth  / nTexWidth;
				// 					vScreenScale[1] = (float) nHeight / nTexHeight;
				// 					vScreenScale[2] = 1.0f / m_FlashlightState.m_flShadowMapResolution;
				// 					vScreenScale[3] = 2.0f / m_FlashlightState.m_flShadowMapResolution;
				// 					SetPixelShaderConstantInternal( nScreenScaleConst, vScreenScale, 1, false );
				// 				}
				// 				else
				// 				{
				// 					BindStandardTexture( (Sampler_t)nDepthSampler, TEXTURE_BINDFLAGS_NONE, TEXTURE_WHITE );
				// 				}

				//				Error("Flashlight unsupported\n");

				pCmdBuf += 12 * sizeof( int );
				break;
			}

		case CBCMD_SET_PIXEL_SHADER_UBERLIGHT_STATE:
			{
				// 				int iEdge0Const			= GetData<int>( pCmdBuf + sizeof( int ) );
				// 				int iEdge1Const			= GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				// 				int iEdgeOOWConst		= GetData<int>( pCmdBuf + 3 * sizeof( int ) );
				// 				int iShearRoundConst	= GetData<int>( pCmdBuf + 4 * sizeof( int ) );
				// 				int iAABBConst			= GetData<int>( pCmdBuf + 5 * sizeof( int ) );
				// 				int iWorldToLightConst	= GetData<int>( pCmdBuf + 6 * sizeof( int ) );
				pCmdBuf += 7 * sizeof( int );
				// 
				// 				SetPixelShaderConstantInternal( iEdge0Const, m_UberlightRenderState.m_vSmoothEdge0.Base(), 1, false );
				// 				SetPixelShaderConstantInternal( iEdge1Const, m_UberlightRenderState.m_vSmoothEdge1.Base(), 1, false );
				// 				SetPixelShaderConstantInternal( iEdgeOOWConst, m_UberlightRenderState.m_vSmoothOneOverW.Base(), 1, false );
				// 				SetPixelShaderConstantInternal( iShearRoundConst, m_UberlightRenderState.m_vShearRound.Base(), 1, false );
				// 				SetPixelShaderConstantInternal( iAABBConst, m_UberlightRenderState.m_vaAbB.Base(), 1, false );
				// 				SetPixelShaderConstantInternal( iWorldToLightConst, m_UberlightRenderState.m_WorldToLight.Base(), 4, false );

				Error("Uberlight state unsupported\n");

				break;
			}

#ifndef NDEBUG
		default:
			Assert(0);
			break;
#endif
		}
		pLastCmd = pCmd;
	}
}
