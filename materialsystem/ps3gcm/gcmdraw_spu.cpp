//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

//--------------------------------------------------------------------------------------------------
// Inlcudes
//--------------------------------------------------------------------------------------------------

#include <libsn_spu.h>
#include "SpuMgr_spu.h"
#include "gcmdraw_spu.h"
#include "gcmdrawstate.h"
#include "gcmstate.h"

//--------------------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------------------

ALIGN16 VertexShader9Data_t					gVertexShaderData ALIGN16_POST;
ALIGN16 PixelShader9Data_t					gPixelShaderData ALIGN16_POST;

ALIGN16 CellGcmContextData					gGcmContext ALIGN16_POST ALIGN16_POST;

ALIGN16 CPs3gcmGlobalState					g_ps3gcmGlobalState ALIGN16_POST ;

ALIGN16 CPs3gcmTextureLayout::Format_t		g_ps3texFormats[PS3_TEX_MAX_FORMAT_COUNT] ALIGN16_POST;

ALIGN16 IDirect3DVertexDeclaration9			gDecl ALIGN16_POST;

ALIGN16 CellGcmContextData*					gpGcmContext = &gGcmContext ALIGN16_POST;

ALIGN16 uint8								gFp[0x2000] ALIGN16_POST;
ALIGN16 uint8								gVp[0x2000] ALIGN16_POST;

ALIGN16 CPs3gcmTextureLayout				gaLayout[D3D_MAX_TEXTURES] ALIGN16_POST;

ALIGN16 uint8								gaECB[3][0x1000];

ALIGN16 CPs3gcmLocalMemoryBlock				gLmBlock ALIGN16_POST;

int gEA;


//--------------------------------------------------------------------------------------------------
// Routine to DMA in texture Layouts
//--------------------------------------------------------------------------------------------------

void GetTextureLayouts()
{
	// Loop and DMA in texture layouts

	for (uint32 lp = 0; lp < ARRAYSIZE(gaLayout); lp++)
	{
		uintp ea = gpGcmDrawState->m_textures[lp].m_eaLayout;
		
		gEA = ea;

		if (ea) gSpuMgr.DmaGetSAFE( &gaLayout[lp], ea, sizeof(CPs3gcmTextureLayout), SPU_DMAGET_TAG );
	}

	gSpuMgr.DmaDone( SPU_DMAGET_TAG_WAIT );
}

//--------------------------------------------------------------------------------------------------
// main()
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Protocol 
//
// Simplest possible for starters : 
// PPU sends SPU Mbx the last part of the drawcall to perform.
// SPU performs it and DMAs down the data. When it's complete it send the PPUMbx the length of the drawcall
// PPU prepares next packet which waits on PPUMbx completion before sending another.
//
// Relies on PPU calling cellGcmReserveMethodSize with 16k, so that the SPU can go ahead and DMA back the 
// draw..
//--------------------------------------------------------------------------------------------------

int main(void)
{
	gSpuMgr.Init();

	// Initialise SPUs drawstate class

	uint32 eaGcmDrawState;
	gpGcmDrawState->Init();
	uint8* pData = gpGcmDrawState->m_pData;

	// Initialise context

	gGcmContext.begin = (uint32*)MemAlloc_AllocAligned(GCM_DS_FIFOPERDRAW * GCM_NUMDRAWCALLS_SPU, 128);
	gGcmContext.end = gGcmContext.begin + (GCM_DS_FIFOPERDRAW * GCM_NUMDRAWCALLS_SPU)/4;
	gGcmContext.callback = NULL;

	// Pull in globalstate

	volatile uint32 eagGlobalState;

	gSpuMgr.ReadMailbox( (uint32_t *) &eagGlobalState );
	gSpuMgr.DmaGetUNSAFE( &g_ps3gcmGlobalState, eagGlobalState, SPUMGR_ALIGN_UP( sizeof(g_ps3gcmGlobalState), 16 ), SPU_DMAGET_TAG );
	gSpuMgr.DmaDone( SPU_DMAGET_TAG_WAIT );

	while(1)
	{
        uint32 startidx, count, loop;
		gSpuMgr.ReadMailbox( (uint32_t *) &startidx );
        count = startidx >>16;
        startidx &= 0xFFFF;

		gpGcmContext->current = gpGcmContext->begin;

        // Loop over the drawstates

        for (loop = 0; loop < count; loop++)
        {
            uint32 idx = (startidx +loop) % GCM_DRAWSTATE_MAX;
            eaGcmDrawState = g_ps3gcmGlobalState.m_eaDrawStates + (idx*sizeof(CGcmDrawState));

		    // Read drawstate

		    gSpuMgr.DmaGetUNSAFE( gpGcmDrawState, eaGcmDrawState, SPUMGR_ALIGN_UP( DRAWSTATE_SIZEOFDMA, 16 ), SPU_DMAGET_TAG );
		    gSpuMgr.DmaDone( SPU_DMAGET_TAG_WAIT );

		    // Read Fixed Data

		    gSpuMgr.DmaGetUNSAFE( &gFixedData[0], uintp(gpGcmDrawState->m_pFixed), SPUMGR_ALIGN_UP(sizeof(gFixedData[0]), 16), SPU_DMAGET_TAG );
		    gSpuMgr.DmaDone( SPU_DMAGET_TAG_WAIT );
		    gpGcmDrawState->m_pFixed = &gFixedData[0];

		    // Read Packed Data

		    uint32* pParam = gpGcmDrawState->m_param;

		    if (gpGcmDrawState->m_cmd & 0x80000000) snPause();
		    gpGcmDrawState->m_cmd &= 0x7fffffff;

		    uint32	packSize = gpGcmDrawState->m_pDataCursor - gpGcmDrawState->m_pData;
		    gSpuMgr.DmaGetUNSAFE( pData, uintp(gpGcmDrawState->m_pData), SPUMGR_ALIGN_UP( packSize, 16 ), SPU_DMAGET_TAG );
		    gpGcmDrawState->m_pData = pData;
		    gpGcmDrawState->m_pDataCursor = pData + packSize;

		    // DMA in any ECBs we will need...
    	
		    for ( uint32 lp = 0; lp < 3; lp++ )
		    {
			    if (gpGcmDrawState->m_aECB[lp])
			    {
				    gSpuMgr.DmaGetSAFE( gaECB[lp], uintp(gpGcmDrawState->m_aECB[lp]), gpGcmDrawState->m_aSizeECB[lp], SPU_DMAGET_TAG );
				    gpGcmDrawState->m_aECB[lp] = gaECB[lp];
			    }
		    }

		    gSpuMgr.DmaDone( SPU_DMAGET_TAG_WAIT );

		    // Read Pixel Shader and Vertex Shader

		    if ( (gpGcmDrawState->m_cmd != CmdCommitStates) && (gpGcmDrawState->m_cmd != CmdEndFrame ))
		    {
			    if(gpGcmDrawState->m_pVertexShaderData) 
			    {
				    gSpuMgr.DmaGetUNSAFE( &gVertexShaderData, uintp(gpGcmDrawState->m_pVertexShaderData), SPUMGR_ALIGN_UP( sizeof(gVertexShaderData), 16 ), SPU_DMAGET_TAG );
				    gpGcmDrawState->m_pVertexShaderData = &gVertexShaderData;

				    gSpuMgr.DmaDone( SPU_DMAGET_TAG_WAIT );

				    // FPHeader, UCode, patches etc...

				    uintp ea = uintp(gVertexShaderData.m_pVertexShaderCmdBuffer);
				    gSpuMgr.DmaGetUNSAFE( &gVp, ea, SPUMGR_ALIGN_UP((gVertexShaderData.m_nVertexShaderCmdBufferWords*4),16), SPU_DMAGET_TAG );
				    gVertexShaderData.m_pVertexShaderCmdBuffer = (uint32*)gVp;

			    }

			    if(gpGcmDrawState->m_pPixelShaderData) 
			    {
				    // PS Data

				    gSpuMgr.DmaGetUNSAFE( &gPixelShaderData, uintp(gpGcmDrawState->m_pPixelShaderData), SPUMGR_ALIGN_UP( sizeof(gPixelShaderData), 16 ), SPU_DMAGET_TAG );
				    gpGcmDrawState->m_pPixelShaderData = &gPixelShaderData;

				    gSpuMgr.DmaDone( SPU_DMAGET_TAG_WAIT );

				    // FPHeader, UCode, patches etc...
    				
				    uintp ea = uintp(gPixelShaderData.m_eaFp);
				    gSpuMgr.DmaGetUNSAFE( &gFp, ea, SPUMGR_ALIGN_UP(gPixelShaderData.m_nTotalSize,16), SPU_DMAGET_TAG );
				    gPixelShaderData.m_eaFp = (FpHeader_t*)gFp;
			    }

			    // Decl

			    gSpuMgr.DmaGetUNSAFE( &gDecl, uintp(pParam[0]), SPUMGR_ALIGN_UP( sizeof(gDecl), 16 ), SPU_DMAGET_TAG );

			    // Texture Fomrats

			    gSpuMgr.DmaGetUNSAFE( &g_ps3texFormats, uintp(pParam[4]), SPUMGR_ALIGN_UP( sizeof(g_ps3texFormats), 16 ), SPU_DMAGET_TAG );	

			    gSpuMgr.DmaDone( SPU_DMAGET_TAG_WAIT );


		    }

		    // Process command

		    switch(gpGcmDrawState->m_cmd)
		    {
			    case CmdCommitStates:
			    case CmdEndFrame:
				    gpGcmDrawState->CommitStates();
				    break;

			    case CmdDrawPrim:
				    gpGcmDrawState->CommitAll(&gDecl, pParam[1]);

				    // Draw

				    GCM_FUNC( cellGcmSetDrawIndexArray, 
					    pParam[2], pParam[5],
					    CELL_GCM_DRAW_INDEX_ARRAY_TYPE_16, CELL_GCM_LOCATION_LOCAL,
					    pParam[3] );



				    break;

			    case CmdDrawPrimUP:
				    {
					    D3DStreamDesc &dsd = g_dxGcmVertexStreamSources[0];

					    dsd.m_offset = 0;
					    dsd.m_stride = pParam[2];
					    dsd.m_vtxBuffer = ( IDirect3DVertexBuffer9 * )( uintp )1; // invalid pointer, but non-NULL to signal it's a real vertex buffer;
					    dsd.m_nLocalBufferOffset = 0;

					    gpGcmDrawState->CommitAll(&gDecl, 0);
					    GCM_FUNC(cellGcmSetCallCommand, pParam[1]);

				    }

				    break;

		    }

        }   // End Loop over drawstates

		// DMA out packet
		// first fill context to a 16B boundary

		while (uintp(gpGcmContext->current) & 0xf)
		{
			*gpGcmContext->current = 0;
			gpGcmContext->current++;
		}

		// Send to fifo

		uint32 bytesUsed = (uint8*)gpGcmContext->current - (uint8*)gpGcmContext->begin;

		gSpuMgr.DmaSync();
		gSpuMgr.DmaPut(gpGcmDrawState->m_eaOutputFIFO, (void*)gpGcmContext->begin, 
					   bytesUsed, SPU_DMAPUT_TAG);
		gSpuMgr.DmaDone(SPU_DMAPUT_TAG_WAIT);

		// Send to SPU mailbox

		gSpuMgr.WriteMailbox(gpGcmDrawState->m_eaOutputFIFO + bytesUsed);

	}

}