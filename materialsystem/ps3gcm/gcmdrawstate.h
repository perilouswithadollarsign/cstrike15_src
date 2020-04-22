//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
// Per draw call Gcm state
// Render states, vo/fp consts
//
//==================================================================================================

#ifndef INCLUDED_GCMDRAWSTATE_H
#define INCLUDED_GCMDRAWSTATE_H

#ifndef SPU

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "cell\gcm.h"
#include "gcmconfig.h"
#include "ps3gcmmemory.h"
#include "dxabstract_def.h"
#include "dxabstract.h"

#include "shaderapi/commandbuffer.h"
#include "shaderapi/shareddefs.h"
#include "mathlib/vector4d.h" 
#include "mathlib/vmatrix.h"

#include <shaderapi/ishaderdynamic.h>
#include <vprof.h>

#include "SpuMgr_ppu.h"

#else

#include "spumgr_spu.h"
#include "cell/gcm_spu.h"
#include "cell/gcm/gcm_method_data.h"

#include "dxabstract_def.h"
#include "gcmtexture.h"
#include "gcmlabels.h"

#include "shaderapi/shareddefs.h"
#include "shaderapi/commandbuffer.h"
#include <shaderapi/ishaderdynamic.h>

#include "gcmdraw_spu.h"

#endif

//--------------------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------------------

//#define GCM_DS_SAFE

#define GCM_DRAWSTATE_MAX 9                         // We have this many drawstate structures
                                                    // we fill half of them and send to SPU
                                                    // then we fill the other half
                                                    // Need an odd number since one extra is required in SendToSPU
#define GCM_NUMDRAWCALLS_SPU ((GCM_DRAWSTATE_MAX-1)/2)

#define GCM_DS_FIFOPERDRAW 0x4000                   // 16K is our max epr draw call FIFO.
                                                    // in practice we see a highwater of 0x2800
                                                    // which combines a full RSx reset with a drawcall

#define GCM_DS_MAXDATAPERDRAWCALL 0x2000			// Highwater mark is abt 3K
#define GCM_DS_MAXFPCONST		  96
#define GCM_DS_MAXVPCONST		  256

#define GCM_DS_MAXDYNECB		  0x40000			// 64K ring buffer. if <8K left wraps

#define GCM_DS_INST_ENVMAP		  1
#define GCM_DS_INST_LIGHTMAP	  2
#define GCM_DS_INST_PAINTMAP	  4
#define MAX_SAMPLERS 16

//--------------------------------------------------------------------------------------------------
// Global externs
//--------------------------------------------------------------------------------------------------

extern 						uint8 g_d3drs_defvalue_indices[D3DRS_VALUE_LIMIT];
extern 						uint32 g_d3drs_defvalues[11];
extern 						uint32 dxtogl_stencilmode[10];
extern 						uint16 dxtogl_blendop[7];

extern 						uint8 dxtogl_addressMode[6]; 
extern 						uint8 dxtogl_anisoIndexHalf[32]; // indexed by [ dxsamp->maxAniso / 2 ]
extern 						uint8 dxtogl_minFilter[4][4];		// indexed by _D3DTEXTUREFILTERTYPE on both axes: [row is min filter][col is mip filter]. 
extern 						uint8 dxtogl_magFilter[4];		// indexed by _D3DTEXTUREFILTERTYPE

extern int 					g_bZcullAuto;
extern int 					g_nZcullDefault;
extern int 					g_nZcullMoveForward;
extern int 					g_nZcullPushBack;

extern vec_float4			g_aFPConst[GCM_DS_MAXFPCONST];
extern vec_float4			g_aVPConst[GCM_DS_MAXVPCONST];
extern D3DStreamDesc		g_dxGcmVertexStreamSources[D3D_MAX_STREAMS];

extern uint32 				g_UPHigh;
extern uint32 				g_UPFrame;

extern volatile uint32_t *	g_label_fppatch_ring_seg;

extern uint8				g_aDynECB[GCM_DS_MAXDYNECB];
extern uint32				g_nDynECBIdx;

extern uint8				gPackData[][GCM_DS_MAXDATAPERDRAWCALL];

//--------------------------------------------------------------------------------------------------
// Structs used as params
//--------------------------------------------------------------------------------------------------

struct DrawScissor_t
{
	uint16 x, y, w, h;
};

struct UpdateSurface_t
{
	// if the scissor is logically disabled, set scissor to this size
	//uint16 m_nRenderTargetWidth, m_nRenderTargetHeight;
	
	CPs3gcmTextureData_t m_texC, m_texZ;
};

struct FpHeader_t
{
	uint32 m_nUcodeSize;
	uint32 m_nPatchCount;

	uint32 m_nShaderControl0;
	uint32 m_nTexControls; //   Always <= 16; 1 tex control corresponds to 2 words in the tex control table

	// data[]
	// Allocate memory layout as : 
	// FpHeader_t
	// uCode
	// Patches
	// Texcontrols
	// total size = AlignValue( sizeof( FpHeader_t ) + m_nUcodeSize + (sizeof( uint32 ) * nPatchCount) 
	// + (2 * sizeof( uint32 ) * nTexControls) , 16);
};

//--------------------------------------------------------------------------------------------------
// Vertex streams
//--------------------------------------------------------------------------------------------------

struct SetVertexDataArrayCache_t
{
	union Data_t
	{

		vector signed int m_vi;
		struct Unpacked_t
		{
			uint32 m_uiLocalMemoryBuffer; // after adding the offset 

			uint32 m_nSize;
			uint32 m_nStride;
			uint32 m_nType;

			//IDirect3DVertexBuffer9 *m_vtxBuffer; // for debug only
			//uint32 m_nBaseVertexOffset; // debug only
		} m_unpacked;
	} m_data;

	SetVertexDataArrayCache_t(){}
	SetVertexDataArrayCache_t( D3DStreamDesc &dsd, D3DVERTEXELEMENT9_GCM::GcmDecl_t const &gcmvad, uint nBaseVertexIndex )
	{
		//m_vtxBuffer           = dsd.m_vtxBuffer;
		uint nBaseVertexOffset   = dsd.m_offset + ( nBaseVertexIndex * dsd.m_stride ) + gcmvad.m_offset;
		uint uiLocalMemoryBuffer = dsd.m_nLocalBufferOffset + nBaseVertexOffset;

		m_data.m_vi = ( vector signed int ) { uiLocalMemoryBuffer, gcmvad.m_datasize, gcmvad.m_datatype, dsd.m_stride };

		// 		m_stride              = dsd.m_stride;
		// 		m_size                = gcmvad.m_datasize;
		// 		m_type                = gcmvad.m_datatype;
	}

	uint GetLocalOffset()const { return m_data.m_unpacked.m_uiLocalMemoryBuffer; }

	bool IsNull()const { return vec_all_eq( m_data.m_vi, (vector signed int){0,0,0,0} ); }
	void SetNull(){ m_data.m_vi = ( vector signed int ){0,0,0,0}; }
	void Invalidate(){ m_data.m_vi = (vector signed int){-1,-1,-1,-1};}
	bool operator != ( const SetVertexDataArrayCache_t& that ) const { return !vec_all_eq( m_data.m_vi, that.m_data.m_vi ); }
	void operator = ( const SetVertexDataArrayCache_t& that ) { m_data.m_vi = that.m_data.m_vi ; }
};

// This is global, since it is only written by the flush code

extern SetVertexDataArrayCache_t	g_cacheSetVertexDataArray[ D3D_MAX_STREAMS ];			// Vertex stream setup

//--------------------------------------------------------------------------------------------------
// SPU draw commands
//--------------------------------------------------------------------------------------------------

enum DrawCmd
{
	CmdCommitStates = 1,
	CmdDrawPrim,
	CmdDrawPrimUP,
	CmdEndFrame
};

//--------------------------------------------------------------------------------------------------
// GcmDrawState.. Holds data that is commited once a draw, clear etc... is made..
//--------------------------------------------------------------------------------------------------

#define DRAWSTATE_SIZEOFDMA (uintp(&(((CGcmDrawState*)(0))->m_pData)+1)-uintp(&(((CGcmDrawState*)(0))->m_cmd)))

struct CGcmDrawState
{
	// DrawData used by DrawPrimUP

	struct DrawData	{ uint8 m_type; uint8 m_idx; uint16 m_size; /*uint8 m_data[m_count];*/ };

	//--------------------------------------------------------------------------------------------------
	// Enums
	//--------------------------------------------------------------------------------------------------

	// Data that gets packes and then unpacked as a cmd stream

	enum GcmDataType
	{
		kDataFpuConsts = 1,
		kDataVpuConsts,
		kDataStreamDesc,
		kDataZcullStats,
		kDataZcullLimit,
		kDataViewport,
		kDataSetRenderState,
		kDataSetZpassPixelCountEnable,
		kDataSetClearReport,
		kDataSetReport,
		kDataUpdateSurface,
		kDataClearSurface,
		kDataResetSurface,
		kDataTransferImage,
		kDataViewPort,
		kDataScissor,
		kDataTexture,
        kDataEcbTexture,
		kDataResetTexture,
		kDataUpdateVtxBufferOffset,
		kDataECB,
		kDataBeginScene,
		kDataSetWorldSpaceCameraPosition,
		kDataSetWriteBackEndLabel
	};
	
	// RenderStates

	enum GcmDirtyStateFlags_t
	{
		kDirtyBlendFactor					=	( 1 << 0 ),
		kDirtyAlphaFunc						=	( 1 << 1 ),
		kDirtyStencilOp						=	( 1 << 2 ),
		kDirtyStencilFunc					=	( 1 << 3 ),
		kDirtyDepthBias						=	( 1 << 4 ),
		kDirtyScissor						=	( 1 << 5 ),
		kDirtyDepthMask						=	( 1 << 6 ),
		kDirtyZEnable						=   ( 1 << 7 ),
		kDirtyZFunc							=	( 1 << 8 ),
		kDirtyColorWriteEnable				=	( 1 << 9 ),
		kDirtyCullMode						=	( 1 << 10 ),
		kDirtyAlphablendEnable				=	( 1 << 11 ),
		kDirtySrgbWriteEnable				=	( 1 << 12 ),
		kDirtyAlphaTestEnable				=	( 1 << 13 ),
		kDirtyStencilEnable					=	( 1 << 14 ),
		kDirtyStencilWriteMask				=	( 1 << 15 ),
		kDirtyFillMode						=	( 1 << 16 ),
		kDirtyBlendOp						=   ( 1 << 17 ),
		kDirtyResetRsx						=	( 1 << 18 ),
		kDirtyZeroAllPSConsts				=   ( 1 << 19 ),
		kDirtyZeroAllVSConsts				=   ( 1 << 20)
	};

	// Dirty flags for caches and other misc settings

	enum GcmDirtyCacheFlags_t
	{
		kDirtyVxConstants		=	( 1 << 0 ),
		kDirtyClipPlanes		=	( 1 << 1 ),
		kDirtyVxShader			=	( 1 << 2 ),
		kDirtyPxShader			=	( 1 << 3 ),
		kDirtyPxConstants		=	( 1 << 4 ),
		kDirtyVxCache           =   ( 1 << 5 ),
		kDirtyTxCache           =   ( 1 << 6 )
	};

	//--------------------------------------------------------------------------------------------------
	// Data we are interested in per draw call
	//--------------------------------------------------------------------------------------------------
	
	// Data that is DMA'd to the SPU directly and not packed

	uint32									m_cmd;
	uint32									m_param[8];
	uint32									m_eaOutputFIFO;
	uint32									m_eaOutputUCode;

	uint32									m_nFreeLabel;					// Nonzero values are set

	uint16									m_nBackBufferSize[2]; 

	uint16									m_dirtySamplersMask;			// Sampler dirty flags
	uint16									m_dirtyCachesMask;				// Caches reset for Shaders flush
	uint32									m_dirtyStatesMask;				// Render state dirty flags

	uint32									m_shaderVxConstants;			// Booleans, go into a SetTransformbranchbits call

	PixelShader9Data_t*						m_pPixelShaderData;
	VertexShader9Data_t*					m_pVertexShaderData;

	uint32									m_nNumECB;
	uint8*									m_aECB[3];						// No More than three per draw call (static, semi-static & dynamic)
	uint32									m_aSizeECB[3];

	struct FixedData
	{
		uint32									m_nSampler;
		uint8									m_aSamplerIdx[D3D_MAX_SAMPLERS];
		D3DSamplerDesc		 					m_aSamplers[D3D_MAX_SAMPLERS];	

		uint32									m_nInstanced;
		CPs3BindTexture_t						m_instanceEnvCubemap;
		CPs3BindTexture_t						m_instanceLightmap;
		CPs3BindTexture_t						m_instancePaintmap;
	};

	// Unpack pointer and cursors

	FixedData*								m_pFixed;							// Fixed sized data uploaded per call

	uint8*									m_pDataCursor;
	uint8*									m_pData;

	// Fixed Data that is unpacked

	D3DSamplerDesc		 					m_aSamplers[D3D_MAX_SAMPLERS];	

	// Data that is unpacked, or derived, or code generated somewhere (Init etc...)


    CPs3BindTexture_t                       m_aBindTexture[CBCMD_MAX_PS3TEX];   // Textures that are set from ECBs

	float									m_vecWorldSpaceCameraPosition[4];

	uint32									m_nSetTransformBranchBits;		// here for now because they init in begin scene
	uint32									m_nDisabledSamplers;


	uint16									m_blends[2];
	struct { uint32 func, ref; }			m_alphaFunc;
	struct { uint32 fail, dfail, dpass; }	m_stencilOp;
	struct { uint32 func, ref, mask; }		m_stencilFunc;
	struct { uint32 factor, units; }		m_depthBias;
	struct { uint16 x, y, w, h, enabled; }	m_scissor;						// kDirtyScissor
	uint16									m_nSetDepthMask;


	uint32									m_ZEnable;
	uint32									m_ZFunc;				
	uint32									m_ColorWriteEnable;
	uint32									m_CullMode;			
	uint32									m_AlphablendEnable;	
	uint32									m_SrgbWriteEnable;	
	uint32									m_AlphaTestEnable;	
	uint32									m_StencilEnable;		
	uint32									m_StencilWriteMask;
	uint32									m_FillMode;			
	uint32									m_BlendOp;

	uint32									m_userClipPlanesState;

	CPs3gcmTextureData_t					m_textures[D3D_MAX_TEXTURES];				

	float									m_viewZ[2];
	uint16 									m_viewportSize[4];




	//--------------------------------------------------------------------------------------------------
	// Methods
	//--------------------------------------------------------------------------------------------------

public:

	// Init etc.. (ppu functions...)

#ifndef SPU
	inline void		Init(IDirect3DDevice9Params *params);
#endif
	inline void		Init();

	void			SendToSpu();
	inline void		Reset();									// Reset for re-use

#ifndef SPU
	inline void		BeginScene();								// Sets report for Zcull
	inline void		EndScene();									// Gets report for Zcull

	inline void		CmdBufferFlush();							// Flush RSX via SPU
	inline void		CmdBufferFinish();							// Flush RSX and wait for it
#endif 

	inline void		ResetRsxState();							// Lots of GCM_FUNC to default vals

	// Dynamic ECB mgmt

	inline uint8*   OpenDynECB();
	inline void	    CloseDynECB(uint32 size);

	// Viewport and scissor

	inline void		UnpackSetViewport(CONST D3DVIEWPORT9* pViewport);
	inline HRESULT	SetViewport(CONST D3DVIEWPORT9* pViewport);
	inline void     UnpackSetScissorRect(DrawScissor_t * pScissor);
	inline void		SetScissorRect( DrawScissor_t * pScissor );

	// Reports, Zpass and labels (all packed)

	inline void 	SetZpassPixelCountEnable(uint32 enable);
	inline void 	SetClearReport(uint32 type);
	inline void 	SetReport(uint32 type, uint32 index);
	inline void 	SetWriteBackEndLabel(uint8 index, uint32 value);

	// RenderStates

	inline void		UnpackSetRenderState( D3DRENDERSTATETYPE State, uint Value );
	inline void		SetRenderState( D3DRENDERSTATETYPE State, uint Value );

	// Texture samplers, textures, texture cache

	inline void		SetInvalidateTextureCache();
	inline void		SetSamplerState( uint Sampler,D3DSAMPLERSTATETYPE Type,DWORD Value );
	inline void		UnpackSetTexture( DWORD Stage, uint32 offset, uint32 eaLayout );
	inline void		UnpackResetTexture( DWORD Stage );
	inline void		SetTexture( DWORD Stage, CPs3gcmTexture *tex );
	inline void		ResetTexture( DWORD Stage );

	// Vertex buffers, vertex cache, , vertex constants

	inline void		SetInvalidateVertexCache();
	inline void		UnpackUpdateVtxBufferOffset( IDirect3DVertexBuffer9 * vtxBuffer, uint nLocalBufferOffset );
	inline void		UpdateVtxBufferOffset( IDirect3DVertexBuffer9 * vtxBuffer, uint nLocalBufferOffset );
	inline void		SetVertexStreamSource(uint nStreamIndex, IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride );
	inline void		_SetVertexShaderConstantB( UINT StartRegister, uint BoolCount, uint shaderVxConstants );
	inline void		SetVertexShaderConstantB( UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount) ;
	inline void		SetVertexShaderConstantF( UINT StartRegister, void* pUnalignedConstantData, UINT Vector4fCount );
// 	inline void		VertexConstantExtractor( float *pDestStorage, int kRegisterFirst, int kRegisterLength,
// 					int StartRegister, const float *pConstantData, int Vector4fCount );

	// Pixel shader consts

	inline void		SetPixelShaderConstantF(uint32 StartRegister, float* pConstantData, uint32 Vector4fCount);
	inline void		UnpackSetWorldSpaceCameraPosition(float* pWCP);
	inline void		SetWorldSpaceCameraPosition(float* pWCP);

	// Surfaces and render targets

	inline void		Ps3Helper_UpdateSurface( UpdateSurface_t * pSurface );
	inline void		UnpackUpdateSurface(CellGcmSurface* pSf);
	inline void		ResetSurfaceToKnownDefaultState();
	inline void		UnpackResetSurfaceToKnownDefaultState();
	inline void		Helper_IntersectRectsXYWH( uint16 const *a, uint16 const *b, uint16 *result );	
	inline void		ClearSurface( DWORD nFlags, D3DCOLOR nColor, float    flZ, uint32   nStencil,
								  uint32   nDepthStencilBitDepth );
	inline void		UnpackClearSurface( DWORD nFlags, D3DCOLOR nColor, float    flZ, uint32   nStencil,
								uint32   nDepthStencilBitDepth );


	// Blit (packed)
	
	inline void		SetTransferImage(uint8 mode, uint32 dstOffset, uint32 dstPitch, uint32 dstX, uint32 dstY, uint32 srcOffset, 
													uint32 srcPitch, uint32 srcX, uint32 srcY, uint32 width, uint32 height, uint32 bytesPerPixel );

	// DrawPrim
	
	inline void		DrawPrimitiveUP( IDirect3DVertexDeclaration9 * pDecl, D3DPRIMITIVETYPE nPrimitiveType,UINT nPrimitiveCount,
									 CONST void *pVertexStreamZeroData, UINT nVertexStreamZeroStride );
	inline void     DrawIndexedPrimitive( uint32 offset, IDirect3DVertexDeclaration9 * pDecl, D3DPRIMITIVETYPE Type,INT BaseVertexIndex,UINT MinVertexIndex,
										  UINT NumVertices,UINT startIndex,UINT nDrawPrimCount );

	inline void		ExecuteCommandBuffer( uint8 *pCmdBuf );
	inline void		UnpackExecuteCommandBuffer( uint8 *pCmdBuf );
	void		TestCommandBuffer( uint8 *pCmdBuf );

	inline void		TextureReplace(uint32 id, CPs3BindTexture_t tex);

	// Commit, pack etc..


	inline void		PackData(uint8 type, uint8 idx, uint16 size, void* pSrc);
	inline void		PackData(uint8 type, uint16 size, void* pSrc);
	inline void		PackData(uint8 type, uint32 val1, uint32 val2, uint32 val3);
	inline void		PackData(uint8 type, uint32 val1, uint32 val2);
	inline void		PackData(uint8 type, uint32 val1);
	inline void		PackData(uint8 type);
	inline void		PackData(uint8 type, DWORD nFlags, D3DCOLOR nColor, float    flZ, uint32   nStencil,
							 uint32   nDepthStencilBitDepth );			// used to pack clear surface


	inline void		UnpackData();			// Unpacks variable sized data and sets vertex consts
	inline void		CommitStates();			// Currently unused on PPU	
	inline void		EndFrame();             // called by Flip()
	inline void		CommitAll(IDirect3DVertexDeclaration9 * pDecl, uint32 baseVertexIndex);
	inline void		CommitRenderStates();
	inline void		CommitVertexBindings(IDirect3DVertexDeclaration9 * pDecl, uint32 baseVertexIndex);
	inline void		CommitSampler(uint32 nSampler);
	inline void		CommitSamplers();
	inline void		CommitShaders();
	inline void		BindFragmentProgram(uint32 nVertexToFragmentProgramAttributeMask);
	inline void		PatchUcode(fltx4 * pUCode16, uint32 * pPatchTable, uint nPatchCount);
	inline fltx4*	CopyUcode(FpHeader_t* pFp);
#ifndef SPU
	inline void		AllocateUcode(FpHeader_t* pFp);				// Reserves space in the patchbuffer for this 
#endif


	// ExecuteCommandBuffer Subs

	inline void		SetVertexShaderConstantInternal( int var, float const* pVec, int numVecs = 1, bool bForce = false );
	inline void 	SetPixelShaderConstantInternal( int var, float const* pValues, int nNumConsts = 1, bool bForce = false );
	inline void		BindTexture2( CPs3BindTexture_t bindTex );

	// Misc

	inline int		IsLayerRender() { return 1;}				// 7LTODO : zprepass !

};

//--------------------------------------------------------------------------------------------------
// Externs
//--------------------------------------------------------------------------------------------------

extern CGcmDrawState* gpGcmDrawState;
extern CGcmDrawState gGcmDrawState[];
extern CGcmDrawState::FixedData	gFixedData[];


//--------------------------------------------------------------------------------------------------
// inlines
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// Generic pack data
//--------------------------------------------------------------------------------------------------

inline void	CGcmDrawState::PackData(uint8 type, uint8 idx, uint16 size, void* pSrc)
{
//	SNPROF("CGcmDrawState::PackData(uint8 type, uint8 idx, uint16 size, void* pSrc)");

	uint32 spacereqd = size + sizeof(DrawData);
#ifdef GCM_DS_SAFE
	uint32 spaceleft = (GCM_DS_MAXDATAPERDRAWCALL - (m_pDataCursor - m_pData ));
	if(spacereqd > spaceleft) Error("Out of per draw call data\n");
#endif
	DrawData* pData = (DrawData*) m_pDataCursor;

	pData->m_type = type;
	pData->m_idx  = idx;
	pData->m_size = size;

	V_memcpy(pData+1, pSrc, size);

	m_pDataCursor += spacereqd;
}

inline void	CGcmDrawState::PackData(uint8 type, uint16 size, void* pSrc)
{
//	SNPROF("CGcmDrawState::PackData(uint8 type, uint16 size, void* pSrc)");

	uint32 spacereqd = size + sizeof(DrawData);

#ifdef GCM_DS_SAFE
	uint32 spaceleft = (GCM_DS_MAXDATAPERDRAWCALL - (m_pDataCursor - m_pData ));
	if(spacereqd > spaceleft) Error("Out of per draw call data\n");
#endif

	DrawData* pData = (DrawData*) m_pDataCursor;

	pData->m_type = type;
	pData->m_idx  = 0;
	pData->m_size = size;

	V_memcpy(pData+1, pSrc, size);

	m_pDataCursor += spacereqd;

}

inline void		CGcmDrawState::PackData(uint8 type, uint32 val1, uint32 val2, uint32 val3)
{
//	SNPROF("CGcmDrawState::PackData(uint8 type, uint32 val1, uint32 val2, uint32 val3)");

	const uint32 size = 12;
	uint32 spacereqd = size + sizeof(DrawData);

#ifdef GCM_DS_SAFE
	uint32 spaceleft = (GCM_DS_MAXDATAPERDRAWCALL - (m_pDataCursor - m_pData ));
	if(spacereqd > spaceleft) Error("Out of per draw call data\n");
#endif

	DrawData* pData = (DrawData*) m_pDataCursor;

	pData->m_type = type;
	pData->m_idx  = 0;
	pData->m_size = size;

	uint32* pDest = (uint32*)(pData + 1);
	pDest[0] = val1;
	pDest[1] = val2;
	pDest[2] = val3;

	m_pDataCursor += spacereqd;

	

}
inline void		CGcmDrawState::PackData(uint8 type, uint32 val1, uint32 val2)
{
//	SNPROF("CGcmDrawState::PackData(uint8 type, uint32 val1, uint32 val2)");

	const uint32 size = 8;

	uint32 spacereqd = size + sizeof(DrawData);

#ifdef GCM_DS_SAFE
	uint32 spaceleft = (GCM_DS_MAXDATAPERDRAWCALL - (m_pDataCursor - m_pData ));
	if(spacereqd > spaceleft) Error("Out of per draw call data\n");
#endif

	DrawData* pData = (DrawData*) m_pDataCursor;

	pData->m_type = type;
	pData->m_idx  = 0;
	pData->m_size = size;

	uint32* pDest = (uint32*)(pData + 1);
	pDest[0] = val1;
	pDest[1] = val2;

	m_pDataCursor += spacereqd;

	

}
inline void		CGcmDrawState::PackData(uint8 type, uint32 val1)
{
//	SNPROF("CGcmDrawState::PackData(uint8 type, uint32 val1)");

	const uint32 size = 4;

	uint32 spacereqd = size + sizeof(DrawData);

#ifdef GCM_DS_SAFE
	uint32 spaceleft = (GCM_DS_MAXDATAPERDRAWCALL - (m_pDataCursor - m_pData ));
	if(spacereqd > spaceleft) Error("Out of per draw call data\n");
#endif

	DrawData* pData = (DrawData*) m_pDataCursor;

	pData->m_type = type;
	pData->m_idx  = 0;
	pData->m_size = size;

	uint32* pDest = (uint32*)(pData + 1);
	pDest[0] = val1;

	m_pDataCursor += spacereqd;

	
}

inline void		CGcmDrawState::PackData(uint8 type)
{
//	SNPROF("CGcmDrawState::PackData(uint8 type)");

	const uint32 size = 0;

	uint32 spacereqd = size + sizeof(DrawData);
#ifdef GCM_DS_SAFE
	uint32 spaceleft = (GCM_DS_MAXDATAPERDRAWCALL - (m_pDataCursor - m_pData ));
	if(spacereqd > spaceleft) Error("Out of per draw call data\n");
#endif

	DrawData* pData = (DrawData*) m_pDataCursor;

	pData->m_type = type;
	pData->m_idx  = 0;
	pData->m_size = size;

	m_pDataCursor += spacereqd;
}

inline void		CGcmDrawState::PackData(uint8 type, DWORD nFlags, D3DCOLOR nColor, float    flZ, uint32   nStencil,	 uint32   nDepthStencilBitDepth )			// used to pack clear surface
{
//	SNPROF("CGcmDrawState::PackData(uint8 type, DWORD nFlags, D3DCOLOR nColor, float    flZ, uint32   nStencil,	 uint32   nDepthStencilBitDepth )");

	const uint32 size = 20;

	uint32 spacereqd = size + sizeof(DrawData);
#ifdef GCM_DS_SAFE
	uint32 spaceleft = (GCM_DS_MAXDATAPERDRAWCALL - (m_pDataCursor - m_pData ));
	if(spacereqd > spaceleft) Error("Out of per draw call data\n");
#endif

	DrawData* pData = (DrawData*) m_pDataCursor;

	pData->m_type = type;
	pData->m_idx  = 0;
	pData->m_size = size;

	uint32* pDest = (uint32*)(pData + 1);
	float*  pDestf = (float*) pDest;
	pDest[0] = nFlags;
	pDest[1] = nColor;
	pDestf[2] = flZ;
	pDest[3] = nStencil;
	pDest[4] = nDepthStencilBitDepth;

	m_pDataCursor += spacereqd;

	

}

//--------------------------------------------------------------------------------------------------
// Init, Begin/EndScene. Flush and Finish, ResetRsxState
//--------------------------------------------------------------------------------------------------

inline void CGcmDrawState::Init()
{
	//  Initialize GCM state to defaults

	memset(this, 0, sizeof(CGcmDrawState));

	m_scissor.enabled = 1;

	m_viewZ[0] = 0.1;
	m_viewZ[1] = 1000.0f;
	
	m_blends[0] = CELL_GCM_ONE;
	m_blends[1] = CELL_GCM_ZERO;
	
	m_alphaFunc.func = CELL_GCM_ALWAYS;
	m_alphaFunc.ref = 0;
	
	m_stencilOp.fail = CELL_GCM_KEEP;
	m_stencilOp.dfail = CELL_GCM_KEEP;
	m_stencilOp.dpass = CELL_GCM_KEEP;
	
	m_stencilFunc.func = CELL_GCM_ALWAYS;
	m_stencilFunc.ref = 0;
	m_stencilFunc.mask = 0xFF;
	
	
	m_depthBias.factor = 0;
	m_depthBias.units  = 0;
	
	m_userClipPlanesState = 0;
	
	m_shaderVxConstants = 0;

	// Init fixed sized data

	m_pFixed = &gFixedData[0];

	memset(m_pFixed->m_aSamplerIdx, 0xff, sizeof(m_pFixed->m_aSamplerIdx));
	m_pFixed->m_nSampler = 0;
	m_pFixed->m_nInstanced = 0;

	// Init variable sized data....

 	m_pData  = gPackData[0];
 	m_pDataCursor = m_pData;

}


#ifndef SPU
inline void CGcmDrawState::Init(IDirect3DDevice9Params *params)
{
	for (int lp = 0; lp < GCM_DRAWSTATE_MAX; lp++)
	{
		CGcmDrawState *pGcmDrawState = &gGcmDrawState[lp];

		pGcmDrawState->Init();

		m_nBackBufferSize[0]  = params->m_presentationParameters.BackBufferWidth;
		m_nBackBufferSize[1] = params->m_presentationParameters.BackBufferHeight;

		pGcmDrawState->m_pData = gPackData[lp];
		pGcmDrawState->m_pFixed = &gFixedData[lp];

		DrawScissor_t temp;

		temp.x = 0;
		temp.y = 0;
		temp.w = m_nBackBufferSize[0];
		temp.h = m_nBackBufferSize[1];

		SetScissorRect(&temp);

	}

}
#endif

#ifndef SPU

inline void CGcmDrawState::BeginScene()
{
	// redundant: will lead to redundant disabling of all samplers at the beginning of the frame, even though they're disabled anyway after flip

	PackData(kDataBeginScene);

	SetRenderState(D3DRS_ZWRITEENABLE, 1); // CELL_GCM_TRUE

	if ( g_bZcullAuto )
	{
		PackData(kDataZcullStats);
	}

	PackData(kDataZcullLimit, g_nZcullMoveForward, g_nZcullPushBack);

	g_UPFrame = 0;

}

inline void CGcmDrawState::EndScene()
{
	int nZcullDefault = g_nZcullDefault;

	// Update zcull settings based on metrics
	if ( g_bZcullAuto )
	{
		int	nMaxSlope = cellGcmGetReport( CELL_GCM_ZCULL_STATS, GCM_REPORT_ZCULL_STATS_0 );
		int nSumSlope = cellGcmGetReport( CELL_GCM_ZCULL_STATS1, GCM_REPORT_ZCULL_STATS_1 );
		int nNumTiles, nAvgSlope;

		nNumTiles = nMaxSlope & 0xffff;
		nMaxSlope = ( nMaxSlope & 0xFFFF0000 ) >> 16;
		nAvgSlope = nNumTiles ? nSumSlope / nNumTiles : 0;

		g_nZcullMoveForward = ( nAvgSlope + nMaxSlope ) / 2;
		g_nZcullPushBack = g_nZcullMoveForward / 2;

		if ( g_nZcullMoveForward < 1 || g_nZcullPushBack < 1 )
		{
			// pick reasonable defaults in the failure case
			g_nZcullMoveForward = nZcullDefault;
			g_nZcullPushBack = nZcullDefault;
		}
	}
	else
	{
		g_nZcullMoveForward = nZcullDefault;
		g_nZcullPushBack = nZcullDefault;
	}


//	Msg("DrawPrimUP Frame %d\n", g_UPFrame);


}

inline void CGcmDrawState::CmdBufferFlush()
{
	CellGcmControl volatile *control = cellGcmGetControlRegister();

	// Out-of-order write protection.
	// this needs to be sync, not eieio as command buffer is on main memory(which is cached)
	// but control registers are mapped as cache inhibited, eieio doesn't gurantee order 
	// between cached and cache inhibited region
#ifdef __SNC__
	__builtin_sync();
#else
	__asm__ volatile("sync");
#endif // __SNC__

	uint32_t offsetInBytes = (uint32)gpGcmContext->current - (uint32)g_ps3gcmGlobalState.m_pIoAddress;
	control->put = offsetInBytes;
}

inline void CGcmDrawState::CmdBufferFinish()
{
	uint32 ref = g_ps3gcmGlobalState.m_finishIdx;

	GCM_FUNC(cellGcmSetReferenceCommand, ref);
	g_ps3gcmGlobalState.m_finishIdx ^=1;

	CmdBufferFlush();

	CellGcmControl volatile *control = cellGcmGetControlRegister();
	while( control->ref != ref )
	{
		// Don't be a ppu hog ;)
		sys_timer_usleep(30);
	}
}

#endif

//--------------------------------------------------------------------------------------------------
// Dynamic ECB management
//--------------------------------------------------------------------------------------------------

inline uint8*   CGcmDrawState::OpenDynECB()
{
	return &g_aDynECB[g_nDynECBIdx];
}

inline void	    CGcmDrawState::CloseDynECB(uint32 size)
{
	g_nDynECBIdx += AlignValue(size,16);

	// If we don't have 8K left then wrap

	if (g_nDynECBIdx > (GCM_DS_MAXDYNECB - 0x2000))
		g_nDynECBIdx = 0;
}

//--------------------------------------------------------------------------------------------------
// Resets RSX to default state
//--------------------------------------------------------------------------------------------------

inline void UnpackResetRsxState()
{
	GCM_FUNC( cellGcmSetAlphaFunc, CELL_GCM_ALWAYS, 0);
	GCM_FUNC( cellGcmSetAlphaTestEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetBackStencilFunc, CELL_GCM_ALWAYS, 0, 0xff);
	GCM_FUNC( cellGcmSetBackStencilMask, 0xff);
	GCM_FUNC( cellGcmSetBackStencilOp, CELL_GCM_KEEP, CELL_GCM_KEEP, CELL_GCM_KEEP);
	GCM_FUNC( cellGcmSetBlendColor, 0, 0);
	GCM_FUNC( cellGcmSetBlendEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetBlendEnableMrt, CELL_GCM_FALSE, CELL_GCM_FALSE, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetBlendEquation, CELL_GCM_FUNC_ADD, CELL_GCM_FUNC_ADD);
	GCM_FUNC( cellGcmSetBlendFunc, CELL_GCM_ONE, CELL_GCM_ZERO, CELL_GCM_ONE, CELL_GCM_ZERO);
//	GCM_FUNC( cellGcmSetClearDepthStencil, 0xffffff00);
//	GCM_FUNC( cellGcmSetClearSurface, 0);
	GCM_FUNC( cellGcmSetColorMask, CELL_GCM_COLOR_MASK_A|CELL_GCM_COLOR_MASK_R|CELL_GCM_COLOR_MASK_G|CELL_GCM_COLOR_MASK_B);
	GCM_FUNC( cellGcmSetCullFaceEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetCullFace, CELL_GCM_BACK);
	GCM_FUNC( cellGcmSetDepthBounds, 0.0f, 1.0f);
	GCM_FUNC( cellGcmSetDepthBoundsTestEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetDepthFunc, CELL_GCM_LESS);
	GCM_FUNC( cellGcmSetDepthMask, CELL_GCM_TRUE);
	GCM_FUNC( cellGcmSetDepthTestEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetDitherEnable, CELL_GCM_TRUE);
	GCM_FUNC( cellGcmSetFragmentProgramGammaEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetFrequencyDividerOperation, 0);
	GCM_FUNC( cellGcmSetFrontFace, CELL_GCM_CCW);
	GCM_FUNC( cellGcmSetLineWidth, 8); // fixed point [0:6:3]
	GCM_FUNC( cellGcmSetLogicOpEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetLogicOp, CELL_GCM_COPY);
	// GCM_FUNC( cellGcmSetNotifyIndex, -=something invalid=- ); // initial value is an invalid system reserved area
	GCM_FUNC( cellGcmSetPointSize, 1.0f);
	GCM_FUNC( cellGcmSetPolygonOffsetFillEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetPolygonOffset, 0.0f, 0.0f);
	GCM_FUNC( cellGcmSetRestartIndexEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetRestartIndex, 0xffffffff);
	GCM_FUNC( cellGcmSetScissor, 0,0,4096,4096);
	GCM_FUNC( cellGcmSetShadeMode, CELL_GCM_SMOOTH);
	GCM_FUNC( cellGcmSetStencilFunc, CELL_GCM_ALWAYS, 0, 0xff);
	GCM_FUNC( cellGcmSetStencilMask, 0xff);
	GCM_FUNC( cellGcmSetStencilOp, CELL_GCM_KEEP, CELL_GCM_KEEP, CELL_GCM_KEEP);
	GCM_FUNC( cellGcmSetStencilTestEnable, CELL_GCM_FALSE);
	for( uint nTextureSampler = 0; nTextureSampler < 16; ++nTextureSampler )
	{
		GCM_FUNC( cellGcmSetTextureAddress, nTextureSampler, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP, 
			CELL_GCM_TEXTURE_CLAMP_TO_EDGE, CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL, 
			CELL_GCM_TEXTURE_ZFUNC_NEVER, 0);
		GCM_FUNC( cellGcmSetTextureBorderColor, nTextureSampler, 0);
		GCM_FUNC( cellGcmSetTextureControl, nTextureSampler, CELL_GCM_FALSE, 0, 12<<8, CELL_GCM_TEXTURE_MAX_ANISO_1);
		GCM_FUNC( cellGcmSetTextureFilter, nTextureSampler, 0, CELL_GCM_TEXTURE_NEAREST_LINEAR, 
			CELL_GCM_TEXTURE_LINEAR, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
	}
	for( uint nVertexAttribute = 0; nVertexAttribute < 16; ++nVertexAttribute )
	{
		GCM_FUNC( cellGcmSetVertexDataArray, nVertexAttribute, 0, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0);
	}
	GCM_FUNC( cellGcmSetTwoSidedStencilTestEnable, CELL_GCM_FALSE);
	float scale[4] = {2048.0f, 2048.0f, 0.5f, 0.0f};
	float offset[4] = {2048.0f, 2048.0f, 0.5f, 0.0f};
	GCM_FUNC( cellGcmSetViewport, 0, 0, 4096, 4096, 0.0f, 1.0f, scale, offset);
	GCM_FUNC( cellGcmSetZcullStatsEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetAntiAliasingControl, CELL_GCM_FALSE, CELL_GCM_FALSE, CELL_GCM_FALSE, 0xffff);
	GCM_FUNC( cellGcmSetBackPolygonMode, CELL_GCM_POLYGON_MODE_FILL);
	GCM_FUNC( cellGcmSetClearColor, 0);
	GCM_FUNC( cellGcmSetColorMaskMrt, 0);
	GCM_FUNC( cellGcmSetFrontPolygonMode, CELL_GCM_POLYGON_MODE_FILL);
	GCM_FUNC( cellGcmSetLineSmoothEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetLineStippleEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetPointSpriteControl, CELL_GCM_FALSE, 0, 0);
	GCM_FUNC( cellGcmSetPolySmoothEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetPolygonStippleEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetRenderEnable, CELL_GCM_TRUE, 0);
	GCM_FUNC( cellGcmSetUserClipPlaneControl, CELL_GCM_FALSE,CELL_GCM_FALSE,CELL_GCM_FALSE,CELL_GCM_FALSE,CELL_GCM_FALSE,CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetVertexAttribInputMask, 0xffff);
	GCM_FUNC( cellGcmSetZpassPixelCountEnable, CELL_GCM_FALSE);
	for( uint i = 0; i < 4 ; ++i )
	{
		GCM_FUNC( cellGcmSetVertexTextureAddress, i, CELL_GCM_TEXTURE_WRAP, CELL_GCM_TEXTURE_WRAP);
		GCM_FUNC( cellGcmSetVertexTextureBorderColor, i, 0);
		GCM_FUNC( cellGcmSetVertexTextureControl, i, CELL_GCM_FALSE, 0, 12<<8);
		GCM_FUNC( cellGcmSetVertexTextureFilter, i, 0);
	}
	GCM_FUNC( cellGcmSetTransformBranchBits, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetTwoSideLightEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetZMinMaxControl, CELL_GCM_TRUE, CELL_GCM_FALSE, CELL_GCM_FALSE);
	// GCM_FUNC( cellGcmSetTextureOptimization, 1<<3); --<sergiy>-- who cares?   this won't compile the way it's described in documentation.
	// GCM_FUNC( cellGcmSetCylindricalWrap, CELL_GCM_FALSE); --<sergiy>-- who cares?   this won't compile the way it's described in documentation.
	GCM_FUNC( cellGcmSetTwoSideLightEnable, CELL_GCM_FALSE);
	GCM_FUNC( cellGcmSetTransformBranchBits, 0);
	GCM_FUNC( cellGcmSetVertexDataBase, 0,0);
	// --<sergiy>-- I don't wanna set the surface to the default surface that we never use, as it generates unneeded stall in RSX
/*
	CellGcmSurface surface = {
		CELL_GCM_SURFACE_PITCH, // type
		CELL_GCM_SURFACE_CENTER_1, // antialias
		CELL_GCM_SURFACE_X1R5G5B5_Z1R5G5B5,// colorFormat
		CELL_GCM_SURFACE_TARGET_0, // colorTarget
		{0, 0, 0, 0}, // colorLocation
		{0, 0, 0, 0}, // colorOffset
		{64, 64, 64, 64}, // colorPitch
		CELL_GCM_SURFACE_Z16, // depthFormat
		CELL_GCM_LOCATION_LOCAL, // depthLocation
		{0,0}, // __padding
		0, // depthOffset
		64, // depthPitch
		1,1, // width,height
		0,0 // x,y
	};
	GCM_FUNC( cellGcmSetSurface, &surface);
*/

	// After ^this, the cached vertex array data is worthless....

	for( uint i = 0; i < D3D_MAX_STREAMS; ++i )
		g_cacheSetVertexDataArray[i].SetNull();
}

inline void CGcmDrawState::ResetRsxState()
{
	m_dirtyStatesMask |= kDirtyResetRsx;
}

//--------------------------------------------------------------------------------------------------
// Viewport and scissor
//--------------------------------------------------------------------------------------------------

inline void CGcmDrawState::UnpackSetViewport(CONST D3DVIEWPORT9* pViewport)
{
	m_viewZ[0] = pViewport->MinZ;
	m_viewZ[1] = pViewport->MaxZ;

	m_viewportSize[0] = pViewport->X;
	m_viewportSize[1] = pViewport->Y;
	m_viewportSize[2] = pViewport->Width;
	m_viewportSize[3] = pViewport->Height;

	float viewScale[4] = { m_viewportSize[2]/2, m_viewportSize[3]/2,
		( m_viewZ[1] - m_viewZ[0] ) / 2.0f, 0.0f };

	float viewOffset[4] = { m_viewportSize[0] + m_viewportSize[2]/2,  m_viewportSize[1] + m_viewportSize[3]/2,
		( m_viewZ[1] + m_viewZ[0] ) / 2.0f, 0.0f };

	GCM_FUNC ( cellGcmSetViewport,  m_viewportSize[0], m_viewportSize[1],
		m_viewportSize[2], m_viewportSize[3],
		m_viewZ[0], m_viewZ[1],
		viewScale, viewOffset );

}


inline HRESULT CGcmDrawState::SetViewport(CONST D3DVIEWPORT9* pViewport)
{
	PackData(kDataViewport, sizeof(D3DVIEWPORT9), (void*)pViewport);

	return S_OK;
}

inline void CGcmDrawState::UnpackSetScissorRect( DrawScissor_t * pScissor )
{
	m_scissor.x = pScissor->x;
	m_scissor.y = pScissor->y;
	m_scissor.w = pScissor->w;
	m_scissor.h = pScissor->h;
	m_dirtyStatesMask |= kDirtyScissor; 
}

inline void CGcmDrawState::SetScissorRect( DrawScissor_t * pScissor )
{
	PackData(kDataScissor, sizeof(DrawScissor_t), pScissor);
}


//--------------------------------------------------------------------------------------------------
// Reports, Zpass and labels
//--------------------------------------------------------------------------------------------------

inline void UnpackSetZpassPixelCountEnable(uint32 enable)
{
	GCM_FUNC(cellGcmSetZpassPixelCountEnable, enable);
}

inline void UnpackSetClearReport(uint32 type)
{
	GCM_FUNC(cellGcmSetClearReport, type);
}

inline void UnpackSetReport(uint32 type, uint32 index)
{
	GCM_FUNC(cellGcmSetReport, type, index);
}

inline void UnpackSetWriteBackEndLabel(uint8 index, uint32 value)
{
	GCM_FUNC(cellGcmSetWriteBackEndLabel, index, value);
}


inline void CGcmDrawState::SetZpassPixelCountEnable(uint32 enable)
{
	PackData(kDataSetZpassPixelCountEnable, enable);
}

inline void CGcmDrawState::SetClearReport(uint32 type)
{
	PackData(kDataSetClearReport, type);
}

inline void CGcmDrawState::SetReport(uint32 type, uint32 index)
{
	PackData(kDataSetReport, type, index);
}

inline void CGcmDrawState::SetWriteBackEndLabel(uint8 index, uint32 value)
{
	if (index == GCM_LABEL_MEMORY_FREE)
	{
		m_nFreeLabel = value;			// 0 is not valid...
	}
	else
	{
		PackData(kDataSetWriteBackEndLabel, index, value);
	}
}

//--------------------------------------------------------------------------------------------------
// Renderstates
//--------------------------------------------------------------------------------------------------

inline void CGcmDrawState::UnpackSetRenderState( D3DRENDERSTATETYPE State, uint Value )
{
	char	ignored = 0;

	Assert( State < D3DRS_VALUE_LIMIT );

	uint nDefvalueIndex = g_d3drs_defvalue_indices[State];
	uint8 nClass = nDefvalueIndex >> 6;
#ifdef DBGFLAG_ASSERT
	nDefvalueIndex &= 0077;
	Assert( nDefvalueIndex < ARRAYSIZE( g_d3drs_defvalues ) );
	uint32 nDefValue = g_d3drs_defvalues[nDefvalueIndex];
#endif
	switch( nClass )
	{
	case	0:		// just ignore quietly. example: D3DRS_LIGHTING
		ignored = 1;
		break;

	case	1:
		{
			// no GL response - and no error as long as the write value matches the default
			Assert( Value == nDefValue );
		}
		break;

	case	2:

		// provide GL response, but only support known default value
		Assert( Value == nDefValue );
		// fall through to mode 3

	case	3:

		// full GL response, support any legal value
		// note we're handling the class-2's as well.
		switch( State )
		{
		default:
			Msg( "Cannot interpret State %d", (int)State );
			break;

		case	D3DRS_ZENABLE:				// kGLDepthTestEnable
			m_ZEnable = !!Value;
			m_dirtyStatesMask |= kDirtyZEnable;
			break;

		case	D3DRS_ZWRITEENABLE:			// kGLDepthMask
			{
				uint32 newMask = Value ? 1 : 0;
				if(m_nSetDepthMask != newMask)
				{
					m_nSetDepthMask = newMask;
					m_dirtyStatesMask |= kDirtyDepthMask;
				}
			}
			break;

		case	D3DRS_ZFUNC:	
			{
				// kGLDepthFunc
				m_ZFunc = D3DCompareFuncToGL( Value );
				m_dirtyStatesMask |= kDirtyZFunc;
			}
			break;

		case	D3DRS_COLORWRITEENABLE:		// kGLColorMaskSingle
			if( IsLayerRender() )
			{
				m_ColorWriteEnable = ( ((Value & D3DCOLORWRITEENABLE_RED)  != 0) ? CELL_GCM_COLOR_MASK_R : 0x00 )
					| ( ((Value & D3DCOLORWRITEENABLE_GREEN)  != 0) ? CELL_GCM_COLOR_MASK_G : 0x00 )
					| ( ((Value & D3DCOLORWRITEENABLE_BLUE)  != 0) ? CELL_GCM_COLOR_MASK_B : 0x00 )
					| ( ((Value & D3DCOLORWRITEENABLE_ALPHA)  != 0) ? CELL_GCM_COLOR_MASK_A : 0x00 );
				
				m_dirtyStatesMask |= kDirtyColorWriteEnable;
			}
			break;

		case	D3DRS_COLORWRITEENABLE1:	// kGLColorMaskMultiple
		case	D3DRS_COLORWRITEENABLE2:	// kGLColorMaskMultiple
		case	D3DRS_COLORWRITEENABLE3:	// kGLColorMaskMultiple
			ignored = 1;
			break;

		case	D3DRS_CULLMODE:				// kGLCullFaceEnable / kGLCullFrontFace
			{
				m_CullMode = Value;
				m_dirtyStatesMask |= kDirtyCullMode;

			}
			break;


			//-------------------------------------------------------------------------------------------- alphablend stuff

		case	D3DRS_ALPHABLENDENABLE:		// kGLBlendEnable
			if( IsLayerRender() )
				m_AlphablendEnable = !!Value;
				m_dirtyStatesMask |= kDirtyAlphablendEnable;
			break;

		case	D3DRS_BLENDOP:				// kGLBlendEquation				// D3D blend-op ==> GL blend equation
			if( IsLayerRender() )
			{
				m_BlendOp = Value;
				m_dirtyStatesMask |= kDirtyBlendOp;
			}
			break;

		case	D3DRS_SRCBLEND:				// kGLBlendFactor				// D3D blend-factor ==> GL blend factor
		case	D3DRS_DESTBLEND:			// kGLBlendFactor
			{
				uint16	factor = D3DBlendFactorToGL( Value );
				m_blends[!( State == D3DRS_SRCBLEND )] = factor;
				m_dirtyStatesMask |= kDirtyBlendFactor;
			}
			break;

		case	D3DRS_SEPARATEALPHABLENDENABLE:
		case	D3DRS_BLENDOPALPHA:
		case	D3DRS_SRCBLENDALPHA:
		case	D3DRS_DESTBLENDALPHA:
			ignored = 1;
			break;

		case	D3DRS_SRGBWRITEENABLE:			// kGLBlendEnableSRGB
			if( IsLayerRender() )
			{
				m_SrgbWriteEnable = Value;
				m_dirtyStatesMask |= kDirtySrgbWriteEnable;

			} 
			break;					

			//-------------------------------------------------------------------------------------------- alphatest stuff

		case	D3DRS_ALPHATESTENABLE:
			m_AlphaTestEnable = Value;			
			m_dirtyStatesMask |= kDirtyAlphaTestEnable;
			break;

		case	D3DRS_ALPHAREF:
			m_alphaFunc.ref = Value;
			m_dirtyStatesMask |= kDirtyAlphaFunc;
			break;

		case	D3DRS_ALPHAFUNC:
			{
				uint32 func = D3DCompareFuncToGL( Value );
				m_alphaFunc.func = func;
				m_dirtyStatesMask |= kDirtyAlphaFunc;
			}
			break;


			//-------------------------------------------------------------------------------------------- stencil stuff

		case	D3DRS_STENCILENABLE:		// GLStencilTestEnable_t
			m_StencilEnable = Value;
			m_dirtyStatesMask |= kDirtyStencilEnable;
			break;

		case	D3DRS_STENCILFAIL:			// GLStencilOp_t		"what do you do if stencil test fails"
			{
				m_stencilOp.fail = dxtogl_stencilmode[Value];
				m_dirtyStatesMask |= kDirtyStencilOp;
			}
			break;

		case	D3DRS_STENCILZFAIL:			// GLStencilOp_t		"what do you do if stencil test passes *but* depth test fails, if depth test happened"
			{
				m_stencilOp.dfail = dxtogl_stencilmode[Value];
				m_dirtyStatesMask |= kDirtyStencilOp;
			}
			break;

		case	D3DRS_STENCILPASS:			// GLStencilOp_t		"what do you do if stencil test and depth test both pass"
			{
				m_stencilOp.dpass = dxtogl_stencilmode[Value];
				m_dirtyStatesMask |= kDirtyStencilOp;
			}
			break;

		case	D3DRS_STENCILFUNC:			// GLStencilFunc_t
			{
				uint32 stencilfunc = D3DCompareFuncToGL( Value );
				m_stencilFunc.func = stencilfunc;
				m_dirtyStatesMask |= kDirtyStencilFunc;
			}
			break;

		case	D3DRS_STENCILREF:			// GLStencilFunc_t
			m_stencilFunc.ref = (Value & 0xFF);
			m_dirtyStatesMask |= kDirtyStencilFunc;
			break;

		case	D3DRS_STENCILMASK:			// GLStencilFunc_t
			{
				m_stencilFunc.mask = (Value & 0xFF);
				m_dirtyStatesMask |= kDirtyStencilFunc;
			}
			break;

		case D3DRS_STENCILWRITEMASK:		// GLStencilWriteMask_t
			{
				//if (Value==255)
				//{
				//	Value = 0xFFFFFFFF;	// mask blast
				//}

				m_StencilWriteMask = Value;
				m_dirtyStatesMask |= kDirtyStencilWriteMask;
			}
			break;

			//-------------------------------------------------------------------------------------------- two-sided stencil stuff
		case	D3DRS_TWOSIDEDSTENCILMODE:	// -> GL_STENCIL_TEST_TWO_SIDE_EXT... not yet implemented ?
		case	D3DRS_CCW_STENCILFAIL:		// GLStencilOp_t
		case	D3DRS_CCW_STENCILZFAIL:		// GLStencilOp_t
		case	D3DRS_CCW_STENCILPASS:		// GLStencilOp_t
		case	D3DRS_CCW_STENCILFUNC:		// GLStencilFunc_t
			ignored = 1;
			break;

		case	D3DRS_FOGENABLE:			// none of these are implemented yet... erk
		case	D3DRS_FOGCOLOR:
		case	D3DRS_FOGTABLEMODE:
		case	D3DRS_FOGSTART:
		case	D3DRS_FOGEND:
		case	D3DRS_FOGDENSITY:
		case	D3DRS_RANGEFOGENABLE:
		case	D3DRS_FOGVERTEXMODE:
			ignored = 1;
			break;

		case	D3DRS_MULTISAMPLEANTIALIAS:
		case	D3DRS_MULTISAMPLEMASK:
			ignored = 1;
			break;

		case	D3DRS_SCISSORTESTENABLE:	// kGLScissorEnable
			{
				m_scissor.enabled = !!Value;
				m_dirtyStatesMask |= kDirtyScissor;
			}
			break;

		case	D3DRS_DEPTHBIAS:			// kGLDepthBias
			{
				// the value in the dword is actually a float
				m_depthBias.units = Value;
				m_dirtyStatesMask |= kDirtyDepthBias;
			}
			break;

			// good ref on these: http://aras-p.info/blog/2008/06/12/depth-bias-and-the-power-of-deceiving-yourself/
		case	D3DRS_SLOPESCALEDEPTHBIAS:
			{
				// the value in the dword is actually a float
				m_depthBias.factor = Value;
				m_dirtyStatesMask |= kDirtyDepthBias;
			}
			break;

		case	D3DRS_CLIPPING:				// ???? is clipping ever turned off ??
			ignored = 1;
			break;

		case	D3DRS_CLIPPLANEENABLE:		// kGLClipPlaneEnable
			{
				m_userClipPlanesState = 0;
				for ( uint32 j = 0, uiValueMask = 1, uiClipSetMask = CELL_GCM_USER_CLIP_PLANE_ENABLE_GE;
					j < 6; ++ j, uiValueMask <<= 1, uiClipSetMask <<= 2 )
				{
					m_userClipPlanesState |= ( ( Value & uiValueMask ) != 0 ) ? uiClipSetMask : 0;
				}
				m_dirtyCachesMask |= kDirtyClipPlanes;
			}
			break;

			//-------------------------------------------------------------------------------------------- polygon/fill mode

		case D3DRS_FILLMODE:
			m_FillMode = Value;
			m_dirtyStatesMask |= kDirtyFillMode;
			break;

		}
		break;
	}
}

inline void CGcmDrawState::SetRenderState( D3DRENDERSTATETYPE State, uint Value )
{
	PackData(kDataSetRenderState, State, Value);
}

//--------------------------------------------------------------------------------------------------
// Texture samplers, textures, texture cache
//--------------------------------------------------------------------------------------------------

inline void CGcmDrawState::SetSamplerState( uint Sampler,D3DSAMPLERSTATETYPE Type,DWORD Value )
{
#ifndef CERT
	if (Sampler>=D3D_MAX_SAMPLERS) Error("Invalid sampler %d, PS3 suppoerts %d\n", Sampler, D3D_MAX_SAMPLERS );
#endif

	// indirect sampler index

	uint32 SamplerIdx = m_pFixed->m_aSamplerIdx[Sampler];

	if (SamplerIdx == 0xFF)
	{
		SamplerIdx = m_pFixed->m_nSampler;
		m_pFixed->m_nSampler++;
		m_pFixed->m_aSamplerIdx[Sampler] = SamplerIdx;
	}

	// the D3D-to-GL translation has been moved to CommitSamplers since we want to do it at draw time
	// so this call just stuffs values in slots.

 	D3DSamplerDesc *samp = m_pFixed->m_aSamplers + SamplerIdx;

	switch( Type )
	{
		// addressing modes can be
		// D3DTADDRESS_WRAP		Tile the texture at every integer junction.
		// D3DTADDRESS_MIRROR	Similar to D3DTADDRESS_WRAP, except that the texture is flipped at every integer junction.
		// D3DTADDRESS_CLAMP	Texture coordinates outside the range [0.0, 1.0] are set to the texture color at 0.0 or 1.0, respectively.
		// D3DTADDRESS_BORDER	Texture coordinates outside the range [0.0, 1.0] are set to the border color.
		// D3DTADDRESS_MIRRORONCE Similar to D3DTADDRESS_MIRROR and D3DTADDRESS_CLAMP.
		//						Takes the absolute value of the texture coordinate (thus, mirroring around 0),
		//						and then clamps to the maximum value. The most common usage is for volume textures,
		//						where support for the full D3DTADDRESS_MIRRORONCE texture-addressing mode is not
		//						necessary, but the data is symmetric around the one axis.

	case	D3DSAMP_ADDRESSU:
		samp->m_addressModeU = Value;
		break;
	case	D3DSAMP_ADDRESSV:
		samp->m_addressModeV = Value;
		break;
	case	D3DSAMP_ADDRESSW:
		samp->m_addressModeW = Value;
		break;

	case	D3DSAMP_BORDERCOLOR:
		// samp->m_borderColor = Value;		// Border color always 0
		break;

	case	D3DSAMP_MAGFILTER:	samp->m_magFilter = (D3DTEXTUREFILTERTYPE)Value; break;
	case	D3DSAMP_MINFILTER:	samp->m_minFilter = (D3DTEXTUREFILTERTYPE)Value; break;
	case	D3DSAMP_MIPFILTER:	samp->m_mipFilter = (D3DTEXTUREFILTERTYPE)Value; break;
	case	D3DSAMP_MIPMAPLODBIAS: samp->m_mipmapBias = Value; break;		// float in sheep's clothing - check this one out
	case	D3DSAMP_MAXMIPLEVEL: samp->m_maxMipLevel = Value; break;		//FIXME (unsure here)
	case	D3DSAMP_MAXANISOTROPY: samp->m_maxAniso = Value; break;
	case	D3DSAMP_SRGBTEXTURE: samp->m_srgb = Value; break;
	case	D3DSAMP_SHADOWFILTER: samp->m_shadowFilter = Value; break;

	default:
		Msg( "Unknown sampler parameter" );
		DebuggerBreak();
		break;

	}

	m_dirtySamplersMask |=  ( 1 << Sampler );
}

inline void CGcmDrawState::UnpackSetTexture( DWORD Stage, uint32 offset, uint32 eaLayout )
{
	// texture sets are finalized in CommitSamplers
	m_textures[Stage].m_nLocalOffset = offset;
	m_textures[Stage].m_eaLayout = eaLayout;

	m_dirtySamplersMask |= ( 1 << Stage );
}

inline void CGcmDrawState::UnpackResetTexture( DWORD Stage )
{
	// texture sets are finalized in CommitSamplers
	m_textures[Stage].Reset();
	m_dirtySamplersMask |= ( 1 << Stage );
}

inline void CGcmDrawState::SetTexture( DWORD Stage, CPs3gcmTexture *tex )
{
	m_textures[Stage].Assign(tex);

	if (tex->m_lmBlock.IsLocalMemory() )
	{
		m_textures[Stage].m_nLocalOffset |= 1;
	}

	PackData(kDataTexture, Stage, m_textures[Stage].m_nLocalOffset, m_textures[Stage].m_eaLayout );
}

inline void CGcmDrawState::ResetTexture( DWORD Stage )
{
	PackData(kDataResetTexture, Stage);
}


inline void	UnpackSetInvalidateTextureCache()
{
	GCM_FUNC( cellGcmSetInvalidateTextureCache, CELL_GCM_INVALIDATE_TEXTURE );
}

inline void	CGcmDrawState::SetInvalidateTextureCache()
{
	m_dirtyCachesMask |= kDirtyTxCache;
}

//--------------------------------------------------------------------------------------------------
// Vertex buffers, vertex cache, , vertex constants
//--------------------------------------------------------------------------------------------------

#ifndef SPU

inline void CGcmDrawState::SetVertexStreamSource( uint nStreamIndex, IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride )
{
//	SNPROF("CGcmDrawState::SetVertexStreamSource( uint nStreamIndex, IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride )");

	// Write stream descriptor into variable data

#ifdef GCM_DS_SAFE
	uint32 spacereqd = sizeof(D3DStreamDesc) + sizeof(DrawData);
	uint32 spaceleft = (GCM_DS_MAXDATAPERDRAWCALL - (m_pDataCursor - m_pData ));
	if(spacereqd > spaceleft) Error("Out of per draw call data\n");
#endif

	DrawData* pData = (DrawData*) m_pDataCursor;

	pData->m_type = kDataStreamDesc;
	pData->m_size = sizeof(D3DStreamDesc);
	pData->m_idx = nStreamIndex;

	D3DStreamDesc* pDsd = (D3DStreamDesc*)(pData+1);
	if ( pStreamData && pStreamData->m_pBuffer )
	{
		// we pass this pointer as a BufferBase later to compare, so we need to make sure they're binarily the same
		Assert( uintp( pStreamData ) == uintp( static_cast<IDirect3DGcmBufferBase*>( pStreamData ) ) );

		pDsd->m_offset = OffsetInBytes;
		pDsd->m_stride = Stride;
		pDsd->m_vtxBuffer = pStreamData;
		pDsd->m_nLocalBufferOffset = pStreamData->m_pBuffer->Offset();
	}
	else
	{
		V_memset(pDsd, 0, sizeof( *pDsd ) );
	}

	m_pDataCursor = (uint8*)pDsd + sizeof(D3DStreamDesc);

	
}

#endif

inline void CGcmDrawState::_SetVertexShaderConstantB( UINT StartRegister, uint BoolCount, uint shaderVxConstants )
{
	uint nMask = ( 1 << ( StartRegister + BoolCount ) ) - ( 1 << StartRegister ) ;
	m_shaderVxConstants &= ~nMask;
	m_shaderVxConstants |= shaderVxConstants;
	m_dirtyCachesMask   |= kDirtyVxConstants;
}

inline void CGcmDrawState::SetVertexShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
{
	uint shaderVxConstants = 0;
	for ( uint32 k = MIN( StartRegister, 32 ), kEnd = MIN( StartRegister + BoolCount, 32 ),
		uiConstantBit = ( 1 << StartRegister ), uiDataIdx = 0;
		k < kEnd; ++ k, uiConstantBit <<= 1, ++ uiDataIdx )
	{
		if( pConstantData[ uiDataIdx ] )
		{
			shaderVxConstants |= uiConstantBit;
		}
	}

	_SetVertexShaderConstantB( StartRegister, BoolCount, shaderVxConstants );
}


// inline void CGcmDrawState::VertexConstantExtractor(
// 	float *pDestStorage, int kRegisterFirst, int kRegisterLength,
// 	int StartRegister, const float *pConstantData, int Vector4fCount )
// {
// 	int iMatrixRegister = Max<int>( 0, StartRegister - kRegisterFirst ); // which part of matrix is updated
// 	int iConstantDataMatrixStart = Max<int>( StartRegister, kRegisterFirst ); // where in constant data the new values start
// 	int numMatrixRegisters = StartRegister + Vector4fCount - iConstantDataMatrixStart; // how many new values can be used
// 	numMatrixRegisters = Min<int>( numMatrixRegisters, kRegisterLength - iMatrixRegister ); // we shouldn't use more values than there's room in the matrix
// 	if ( numMatrixRegisters > 0 )
// 	{
// 		iConstantDataMatrixStart -= StartRegister; // constant data values are relative to StartRegister
// 		V_memcpy( &pDestStorage[ iMatrixRegister * 4 ], &pConstantData[ iConstantDataMatrixStart * 4 ], numMatrixRegisters * 4 * sizeof( float ) );
// 	}
// }


inline void CGcmDrawState::SetVertexShaderConstantF( UINT StartRegister, void* pUnalignedConstantData, UINT Vector4fCount )
{
//	SNPROF("CGcmDrawState::SetVertexShaderConstantF( UINT StartRegister, void* pUnalignedConstantData, UINT Vector4fCount )");
	
	// 	// Intercept the vertex constants affecting model-view-projection [ registers C8,C9,C10,C11 ]
// 	VertexConstantExtractor( m_matViewProjection, 8, 4, StartRegister, pConstantData, Vector4fCount );
// 	// Intercept the vertex constants affecting model matrix [ registers C58,C59,C60 ]
// 	VertexConstantExtractor( m_matModel, 58, 3, StartRegister, pConstantData, Vector4fCount );

	uint32 spacereqd = (Vector4fCount*sizeof(vec_float4)) + sizeof(DrawData);
#ifdef GCM_DS_SAFE
	uint32 spaceleft = (GCM_DS_MAXDATAPERDRAWCALL - (m_pDataCursor - m_pData ));
	if(spacereqd > spaceleft) Error("Out of per draw call data\n");
#endif

	DrawData* pDest   = (DrawData*)m_pDataCursor;
	uint8*	  pVals   = (uint8*)(pDest+1);

	pDest->m_type = kDataVpuConsts;
	pDest->m_size = Vector4fCount * sizeof(vec_float4);
	pDest->m_idx = StartRegister;

	V_memcpy(pVals, pUnalignedConstantData, Vector4fCount * sizeof(vec_float4));

	m_pDataCursor += spacereqd;

	
}

inline void	UnpackSetInvalidateVertexCache()
{
	GCM_FUNC( cellGcmSetInvalidateVertexCache );
}

inline void	CGcmDrawState::SetInvalidateVertexCache()
{
	m_dirtyCachesMask |= kDirtyVxCache;
}


inline void CGcmDrawState::UnpackUpdateVtxBufferOffset( IDirect3DVertexBuffer9 * vtxBuffer, uint nLocalBufferOffset )
{
	for( uint i = 0; i < D3D_MAX_STREAMS; ++i )
	{
		if( g_dxGcmVertexStreamSources[i].m_vtxBuffer == vtxBuffer )
		{
			g_dxGcmVertexStreamSources[i].m_nLocalBufferOffset = nLocalBufferOffset; // new local buffer offset
		}
	}
}

inline void CGcmDrawState::UpdateVtxBufferOffset( IDirect3DVertexBuffer9 * vtxBuffer, uint nLocalBufferOffset )
{
	PackData(kDataUpdateVtxBufferOffset, (uint32)vtxBuffer, nLocalBufferOffset);
}

//--------------------------------------------------------------------------------------------------
// Pixel Shader Consts
//--------------------------------------------------------------------------------------------------

inline void CGcmDrawState::SetPixelShaderConstantF(uint32 StartRegister, float* pConstantData, uint32 Vector4fCount)
{
//	SNPROF("CGcmDrawState::SetPixelShaderConstantF(uint32 StartRegister, float* pConstantData, uint32 Vector4fCount)");

	m_dirtyCachesMask |= CGcmDrawState::kDirtyPxConstants;

 	uint32 spacereqd = (Vector4fCount*sizeof(vec_float4)) + sizeof(DrawData);
#ifdef GCM_DS_SAFE
	uint32 spaceleft = (GCM_DS_MAXDATAPERDRAWCALL - (m_pDataCursor - m_pData ));
	if(spacereqd > spaceleft) Error("Out of per draw call data\n");
#endif

	DrawData* pDest   = (DrawData*)m_pDataCursor;
	uint8*	  pVals   = (uint8*)(pDest+1);

	pDest->m_type = kDataFpuConsts;
	pDest->m_size = Vector4fCount * sizeof(vec_float4);
	pDest->m_idx = StartRegister;

	V_memcpy(pVals, pConstantData, Vector4fCount * sizeof(vec_float4));

	m_pDataCursor += spacereqd;

	
}

inline void	CGcmDrawState::UnpackSetWorldSpaceCameraPosition(float* pWCP)
{
	memcpy(m_vecWorldSpaceCameraPosition, pWCP, sizeof(m_vecWorldSpaceCameraPosition));
}

inline void	CGcmDrawState::SetWorldSpaceCameraPosition(float* pWCP)
{
	PackData(kDataSetWorldSpaceCameraPosition, (uint16)sizeof(m_vecWorldSpaceCameraPosition), (void*)pWCP);
}

//--------------------------------------------------------------------------------------------------
// Surfaces and render teargets
//--------------------------------------------------------------------------------------------------

inline void CGcmDrawState::Ps3Helper_UpdateSurface( UpdateSurface_t * pSurface )
{
	const CPs3gcmTextureData_t &texC = pSurface->m_texC, &texZ = pSurface->m_texZ;
	const CPs3gcmTextureData_t *pTexCZ = &texC;

	CPs3gcmTextureLayout texC_layout, texZ_layout, *pTexCZ_layout = &texC_layout;

	if( texZ )
	{
		memcpy (&texZ_layout, (void*)texZ.m_eaLayout, sizeof( texZ_layout ));
		pTexCZ = &texZ;
		pTexCZ_layout = &texZ_layout;
	}
	if( texC )
	{
		memcpy( &texC_layout, (void*)texC.m_eaLayout, sizeof( texC_layout ));
		pTexCZ = &texC;
		pTexCZ_layout = &texC_layout;
	}

	CellGcmSurface sf;
	V_memset( &sf, 0, sizeof( sf ) );
	sf.colorFormat 	= CELL_GCM_SURFACE_A8R8G8B8;
	sf.colorTarget	= texC.NotNull() ? CELL_GCM_SURFACE_TARGET_0 : CELL_GCM_SURFACE_TARGET_NONE;
	sf.colorLocation[0]	= CELL_GCM_LOCATION_LOCAL;
	sf.colorOffset[0] 	= texC ? texC.Offset() : 0;
	sf.colorPitch[0] 	= texC ? texC_layout.DefaultPitch2( g_ps3texFormats ) : 64;

	sf.colorLocation[1]	= CELL_GCM_LOCATION_LOCAL;
	sf.colorLocation[2]	= CELL_GCM_LOCATION_LOCAL;
	sf.colorLocation[3]	= CELL_GCM_LOCATION_LOCAL;
	sf.colorOffset[1] 	= 0;
	sf.colorOffset[2] 	= 0;
	sf.colorOffset[3] 	= 0;
	sf.colorPitch[1]	= 64;
	sf.colorPitch[2]	= 64;
	sf.colorPitch[3]	= 64;

	sf.depthFormat 		= CELL_GCM_SURFACE_Z24S8;
	if ( texZ )
	{
		CPs3gcmTextureLayout::Format_t &zFmt = g_ps3texFormats[texZ_layout.m_nFormat];
		if ( ( zFmt.m_gcmFormat == CELL_GCM_TEXTURE_DEPTH16 ) || ( zFmt.m_gcmFormat == CELL_GCM_TEXTURE_DEPTH16_FLOAT ) )
		{
			sf.depthFormat = CELL_GCM_SURFACE_Z16;
		}
	}

	sf.depthLocation = CELL_GCM_LOCATION_LOCAL;
	sf.depthOffset	= texZ ? texZ.Offset() : 0;
	sf.depthPitch 	= texZ ? texZ_layout.DefaultPitch2( g_ps3texFormats ) : 64;

	sf.type			= ( texC && texC_layout.IsSwizzled() ) ? CELL_GCM_SURFACE_SWIZZLE : CELL_GCM_SURFACE_PITCH;
	sf.antialias	= CELL_GCM_SURFACE_CENTER_1;

	sf.width 		= *pTexCZ ? pTexCZ_layout->m_key.m_size[0] : g_ps3gcmGlobalState.m_nRenderSize[0];
	sf.height 		= *pTexCZ ? pTexCZ_layout->m_key.m_size[1] : g_ps3gcmGlobalState.m_nRenderSize[1];
	sf.x 			= 0;
	sf.y 			= 0;

	PackData(kDataUpdateSurface, 0, (uint16)sizeof(sf), (void*)&sf);
}

inline void CGcmDrawState::UnpackUpdateSurface(CellGcmSurface* pSf)
{
	GCM_FUNC( cellGcmSetSurface, pSf );

	// cellGcmSetZcullControl invalidates Zcull, and these are the default settings anyways (LESS / LONES)
	// so don't bother doing anything here.
	// If other settings are needed, set them once at the beginning of time for each zcull region
	//GCM_FUNC( cellGcmSetZcullControl, CELL_GCM_ZCULL_LESS, CELL_GCM_ZCULL_LONES );

	// These calls do NOT invalidate Zcull
	GCM_FUNC( cellGcmSetZcullEnable, CELL_GCM_TRUE, CELL_GCM_TRUE );


	// when render target changes, and scissor is not enabled, and the target dimensions change,
	// we need to flush the scissor dimensions because we always maintain scissor ON state, and 
	// the scissor size must conform to surface size (which just changed)
	m_dirtyStatesMask |= kDirtyScissor;
}



inline void CGcmDrawState::Helper_IntersectRectsXYWH( uint16 const *a, uint16 const *b, uint16 *result )
// Takes 2 rects a&b specified as top,left,width,height
// Produces an intersection also as top,left,width,height
// Intersection can have zero width and/or height
{
	result[0] = a[0] > b[0] ? a[0] : b[0];
	result[1] = a[1] > b[1] ? a[1] : b[1];

	uint16 ca = a[0]+a[2], cb = b[0]+b[2];
	ca = ca < cb ? ca : cb;
	if ( int16(ca) < int16(result[0]) )
		ca = result[0];
	result[2] = ca - result[0];

	ca = a[1]+a[3], cb = b[1]+b[3];
	ca = ca < cb ? ca : cb;
	if ( int16(ca) < int16(result[1]) )
		ca = result[1];
	result[3] = ca - result[1];
}

inline void CGcmDrawState::UnpackClearSurface( DWORD nFlags, D3DCOLOR nColor, float    flZ, uint32   nStencil,
										uint32   nDepthStencilBitDepth )
{
	uint32 uiGcmClearMask = 0
		| ( ( nFlags & D3DCLEAR_STENCIL ) ? CELL_GCM_CLEAR_S : 0 )
		| ( ( nFlags & D3DCLEAR_ZBUFFER ) ? CELL_GCM_CLEAR_Z : 0 )
		| ( ( nFlags & D3DCLEAR_TARGET ) ? (CELL_GCM_CLEAR_R|CELL_GCM_CLEAR_G|CELL_GCM_CLEAR_B|CELL_GCM_CLEAR_A) : 0 )
		;
	if ( nFlags & D3DCLEAR_TARGET )
	{
		GCM_FUNC( cellGcmSetClearColor, nColor );
	}
	if ( nFlags & (D3DCLEAR_STENCIL|D3DCLEAR_ZBUFFER) )
	{
		uint32 nClearValue;

		if ( nDepthStencilBitDepth == 16 )
		{
			// NOTE: for SURFACE_Z16 depth is in lower 16 bits
			nClearValue = ( uint32 )( flZ * 0xFFFF );
		}
		else
		{
			nClearValue = ( ( ( uint32 )( flZ * 0xFFFFFF ) ) << 8 ) | ( nStencil & 0xFF );
		}

		// if(Z16) GCM_FUNC( cellGcmSetClearDepthStencil, (((uint32)( Z*0xFFFF ))<<8) );
		GCM_FUNC( cellGcmSetClearDepthStencil, nClearValue );
	}

	// Set scissor box to cover the intersection of viewport and scissor
	if ( !m_scissor.enabled )
	{
		GCM_FUNC( cellGcmSetScissor, m_viewportSize[0], m_viewportSize[1], m_viewportSize[2], m_viewportSize[3] );
	}
	else
	{
		uint16 uiScissorCoords[4] = {0};
		Helper_IntersectRectsXYWH( m_viewportSize, &m_scissor.x, uiScissorCoords );
		GCM_FUNC( cellGcmSetScissor, uiScissorCoords[0], uiScissorCoords[1], uiScissorCoords[2], uiScissorCoords[3] );
	}
	GCM_FUNC( cellGcmSetClearSurface, uiGcmClearMask );

	// Since we affected the scissor, mark it as dirty
	m_dirtyStatesMask |= kDirtyScissor;
}

inline void CGcmDrawState::ClearSurface( DWORD nFlags, D3DCOLOR nColor, float    flZ, uint32   nStencil,
											  uint32   nDepthStencilBitDepth )
{
	PackData(kDataClearSurface, nFlags, nColor, flZ, nStencil,	nDepthStencilBitDepth );
}


inline void CGcmDrawState::UnpackResetSurfaceToKnownDefaultState()
{
	// Reset to default state:
	GCM_FUNC( cellGcmSetCullFaceEnable, CELL_GCM_TRUE );
	GCM_FUNC( cellGcmSetCullFace, CELL_GCM_BACK );
	GCM_FUNC( cellGcmSetFrontFace, CELL_GCM_CW );

	GCM_FUNC( cellGcmSetBlendEnable, CELL_GCM_FALSE );
	GCM_FUNC( cellGcmSetAlphaTestEnable, CELL_GCM_FALSE );
	GCM_FUNC( cellGcmSetStencilTestEnable, CELL_GCM_FALSE );
	GCM_FUNC( cellGcmSetDepthTestEnable, CELL_GCM_FALSE );

	GCM_FUNC( cellGcmSetFrontPolygonMode, CELL_GCM_POLYGON_MODE_FILL );
	GCM_FUNC( cellGcmSetBackPolygonMode, CELL_GCM_POLYGON_MODE_FILL );
	GCM_FUNC( cellGcmSetPolygonOffset, 0, 0 );
	GCM_FUNC( cellGcmSetPolygonOffsetFillEnable, CELL_GCM_FALSE );

	// Force the viewport to match the current back buffer
	D3DVIEWPORT9 dForcedView =
	{
		0, 0,
		m_nBackBufferSize[0],
		m_nBackBufferSize[1],
		m_viewZ[0],
		m_viewZ[1]
	};
	SetViewport( &dForcedView );
	GCM_FUNC( cellGcmSetScissor, 0, 0, m_nBackBufferSize[0], m_nBackBufferSize[1] );

	// Reset some cached gcm state
	m_userClipPlanesState = 0;
	m_shaderVxConstants = 0;
	m_dirtyCachesMask |= ( kDirtyVxConstants | kDirtyVxShader | 
						   kDirtyClipPlanes | kDirtyPxShader | 
					       kDirtyPxConstants );
}

inline void CGcmDrawState::ResetSurfaceToKnownDefaultState()
{
	PackData(kDataResetSurface);
}


//--------------------------------------------------------------------------------------------------
// Blit
//--------------------------------------------------------------------------------------------------

inline void UnpackTransferImage(uint8 mode, uint32 dstOffset, uint32 dstPitch, uint32 dstX, uint32 dstY, uint32 srcOffset, 
								uint32 srcPitch, uint32 srcX, uint32 srcY, uint32 width, uint32 height, uint32 bytesPerPixel )
{
	GCM_FUNC(cellGcmSetTransferImage, mode, dstOffset, dstPitch, dstX, dstY, srcOffset, 
 		srcPitch, srcX, srcY, width, height, bytesPerPixel );

}

inline void  CGcmDrawState::SetTransferImage(uint8 mode, uint32 dstOffset, uint32 dstPitch, uint32 dstX, uint32 dstY, uint32 srcOffset, 
											 uint32 srcPitch, uint32 srcX, uint32 srcY, uint32 width, uint32 height, uint32 bytesPerPixel )
{

// 	return UnpackTransferImage( mode,  dstOffset,  dstPitch,  dstX,  dstY,  srcOffset, 
// 		 srcPitch,  srcX,  srcY,  width,  height,  bytesPerPixel);

	uint32	aValues[12];

	aValues[0] =	mode;
	aValues[1] =	dstOffset;
	aValues[2] =	dstPitch;
	aValues[3] =	dstX;
	aValues[4] =	dstY;
	aValues[5] =	srcOffset;
	aValues[6] =	srcPitch;
	aValues[7] =	srcX;
	aValues[8] =	srcY;
	aValues[9] =	width;
	aValues[10] =	height;
	aValues[11] =	bytesPerPixel;

	PackData(kDataTransferImage, 0, sizeof(aValues), (void*)aValues);
}

//--------------------------------------------------------------------------------------------------
// State Flushing and Pixel Shader Patching
//--------------------------------------------------------------------------------------------------

inline void CGcmDrawState::UnpackData()
{
	static uint32 highWater = 0;
	static float average = 0.0f;
	static uint32 count = 0;
#ifndef SPU
	static int display = 4000;
#endif 
	m_nNumECB = 0;

	int aSizes[64];

	memset(aSizes, 0, sizeof(aSizes));

	DrawData* pSrc = (DrawData*)m_pData;

	while ((uint8*)pSrc < m_pDataCursor)
	{
		uint32* pVals = (uint32*)(pSrc+1);
		float* pfVals = (float*)pVals;

		aSizes[pSrc->m_type] += pSrc->m_size;

		switch (pSrc->m_type)
		{
        case kDataEcbTexture:
            V_memcpy(&m_aBindTexture[pSrc->m_idx], pVals, pSrc->m_size);
            break;
		case kDataSetRenderState:
			UnpackSetRenderState((D3DRENDERSTATETYPE)pVals[0], pVals[1]);
			break;
		case kDataFpuConsts:
			V_memcpy(&g_aFPConst[pSrc->m_idx], pVals, pSrc->m_size);
			break;
		case kDataSetWorldSpaceCameraPosition:
			UnpackSetWorldSpaceCameraPosition(pfVals);
			break;
		case kDataStreamDesc:
			V_memcpy(&g_dxGcmVertexStreamSources[pSrc->m_idx], pVals, pSrc->m_size);
			break;
		case kDataVpuConsts:
			GCM_FUNC( cellGcmSetVertexProgramParameterBlock, pSrc->m_idx, pSrc->m_size/16, (float*)pVals );
			break;
		case kDataZcullStats:
			GCM_FUNC( cellGcmSetReport, CELL_GCM_ZCULL_STATS, GCM_REPORT_ZCULL_STATS_0 );
			GCM_FUNC( cellGcmSetReport, CELL_GCM_ZCULL_STATS1, GCM_REPORT_ZCULL_STATS_1 );
			break;
		case kDataZcullLimit:
			GCM_FUNC(cellGcmSetZcullLimit, pVals[0], pVals[2] );
			break;
		case kDataViewport:
			UnpackSetViewport((D3DVIEWPORT9*) pVals);
			break;
		case kDataScissor:
			UnpackSetScissorRect((DrawScissor_t*) pVals);
			break;
		case kDataSetZpassPixelCountEnable:
			UnpackSetZpassPixelCountEnable(pVals[0]);
			break;
		case kDataSetClearReport:
			UnpackSetClearReport(pVals[0]);
			break;
		case kDataSetReport:
			UnpackSetReport(pVals[0], pVals[1]);
			break;
		case kDataSetWriteBackEndLabel:
			UnpackSetWriteBackEndLabel(pVals[0], pVals[1]);
			break;
		case kDataUpdateSurface:
			UnpackUpdateSurface((CellGcmSurface*)pVals);
			break;
		case kDataResetSurface:
			UnpackResetSurfaceToKnownDefaultState();
			break;
		case kDataClearSurface:
			UnpackClearSurface(pVals[0], pVals[1], pfVals[2], pVals[3], pVals[4] );
			break;
		case kDataTransferImage:
			UnpackTransferImage(pVals[0], pVals[1], pVals[2], pVals[3],
								pVals[4], pVals[5], pVals[6], pVals[7],
								pVals[8], pVals[9], pVals[10], pVals[11] );
			break;
		case kDataTexture:
			UnpackSetTexture(pVals[0], pVals[1], pVals[2]);
			break;
		case kDataResetTexture:
			UnpackResetTexture(pVals[0]);
			break;
		case kDataUpdateVtxBufferOffset:
			UnpackUpdateVtxBufferOffset((IDirect3DVertexBuffer9*)pVals[0], pVals[1]);
			break;
		case kDataECB:
			UnpackExecuteCommandBuffer(m_aECB[m_nNumECB]);
			m_aECB[m_nNumECB] = 0;
			m_nNumECB++;
			break;
		case kDataBeginScene:
			m_nDisabledSamplers       = 0; 
			m_nSetTransformBranchBits = 0;
			break;

		}

		pSrc = (DrawData*)((uint8*)(pSrc+1)+pSrc->m_size);
	}

	m_nNumECB = 0;

	// Record High Water

	uint32 size = m_pDataCursor - m_pData;

	average *= count;
	count++;
	average += size;
	average /= count;

#ifndef SPU
	uint32 avgInt = uint32(average + 0.5f);
#endif

	if (size > highWater)
	{
		highWater = size;

 		Msg("\n>>>>>>>>>>>High Water %d (0x%x) : Average %d (0x%x) : Avg plus GcmDrawState = %d (0x%x) : This plus drawstate (%d (0x%x)) \n", highWater, highWater, 
 			avgInt, avgInt, avgInt + DRAWSTATE_SIZEOFDMA, avgInt + DRAWSTATE_SIZEOFDMA, size + DRAWSTATE_SIZEOFDMA, size + DRAWSTATE_SIZEOFDMA );

		for (int i = 1; i <= kDataTransferImage; i++ )
		{
			Msg( ">>>%d : %d\n", i, aSizes[i]);
		}
	}

// 	display--;
// 	if ( (display < 1) || ((size+sizeof(CGcmDrawState)) > 0x1800))
// 	{
// 		Msg("\n>>>>>>>>>>>High Water %d (0x%x) : Average %d (0x%x) : Avg plus GcmDrawState = %d (0x%x) : This (%d (0x%x)) \n", highWater, highWater, 
// 			avgInt, avgInt, avgInt + sizeof(CGcmDrawState), avgInt + sizeof(CGcmDrawState), size, size );
// 
// 		display = 10000;
// 	}

	// Reset cursor

	m_pDataCursor = m_pData;

}

inline void CGcmDrawState::CommitRenderStates()
{
	uint nMask = m_dirtyStatesMask;
	m_dirtyStatesMask = 0;

	if ( nMask & kDirtyDepthMask)
	{
		GCM_FUNC(cellGcmSetDepthMask, m_nSetDepthMask);
	}

	if ( nMask & kDirtyZEnable )
	{
		GCM_FUNC( cellGcmSetDepthTestEnable, m_ZEnable );
	}

	if ( nMask & kDirtyZFunc )
	{
		GCM_FUNC( cellGcmSetDepthFunc, m_ZFunc );
	}

	if ( nMask & kDirtyColorWriteEnable )
	{
		GCM_FUNC(  cellGcmSetColorMask, m_ColorWriteEnable);
	}

	if ( nMask & kDirtyCullMode )
	{
		switch(m_CullMode)
		{
		case	D3DCULL_NONE:
			GCM_FUNC( cellGcmSetCullFaceEnable, CELL_GCM_FALSE );
			break;

		case	D3DCULL_CW:
			GCM_FUNC( cellGcmSetCullFaceEnable, CELL_GCM_TRUE );
			GCM_FUNC( cellGcmSetFrontFace, CELL_GCM_CCW ); // opposite from D3D
			break;

		case	D3DCULL_CCW:
			GCM_FUNC( cellGcmSetCullFaceEnable, CELL_GCM_TRUE );
			GCM_FUNC( cellGcmSetFrontFace, CELL_GCM_CW ); // opposite from D3D
			break;
		}

	}

	if ( nMask & kDirtyAlphablendEnable	)
	{
		GCM_FUNC( cellGcmSetBlendEnable, m_AlphablendEnable );
	}

	if (nMask & kDirtyBlendOp)
	{
		uint32 Value = m_BlendOp;
		uint16	equation = dxtogl_blendop[ Value ];
		GCM_FUNC( cellGcmSetBlendEquation, equation, equation );
	}

	if ( nMask & kDirtySrgbWriteEnable )
	{
		uint32 Value = m_SrgbWriteEnable;
		GCM_FUNC( cellGcmSetFragmentProgramGammaEnable, !!Value ); 

	}

	if ( nMask & kDirtyAlphaTestEnable )
	{
		uint32 Value = m_AlphaTestEnable;
		GCM_FUNC( cellGcmSetAlphaTestEnable, !!Value );

	}

	if ( nMask & kDirtyStencilEnable )
	{
		uint32 Value = m_StencilEnable;
		GCM_FUNC( cellGcmSetStencilTestEnable, !!Value );
	}

	if ( nMask & kDirtyStencilWriteMask	)
	{
		uint32 Value = m_StencilWriteMask;
		GCM_FUNC( cellGcmSetStencilMask, Value );
	}

	if ( nMask & kDirtyFillMode	)
	{
		uint32 Value = m_FillMode;
		uint32 mode = CELL_GCM_POLYGON_MODE_POINT + ( Value - D3DFILL_POINT );
		GCM_FUNC( cellGcmSetFrontPolygonMode, mode );
		GCM_FUNC( cellGcmSetBackPolygonMode, mode );
	}

	if ( nMask & CGcmDrawState::kDirtyBlendFactor )
	{
		GCM_FUNC( cellGcmSetBlendFunc,
			m_blends[0], m_blends[1],
			m_blends[0], m_blends[1] );
	}

	if ( nMask & CGcmDrawState::kDirtyAlphaFunc )
	{
		GCM_FUNC( cellGcmSetAlphaFunc, m_alphaFunc.func, m_alphaFunc.ref );
	}

	if ( nMask & CGcmDrawState::kDirtyStencilOp )
	{
		GCM_FUNC( cellGcmSetStencilOp, m_stencilOp.fail, m_stencilOp.dfail, m_stencilOp.dpass );
		GCM_FUNC( cellGcmSetBackStencilOp, m_stencilOp.fail, m_stencilOp.dfail, m_stencilOp.dpass );
	}

	if ( nMask & CGcmDrawState::kDirtyStencilFunc )
	{
		GCM_FUNC( cellGcmSetStencilFunc, m_stencilFunc.func, m_stencilFunc.ref, m_stencilFunc.mask );
		GCM_FUNC( cellGcmSetBackStencilFunc, m_stencilFunc.func, m_stencilFunc.ref, m_stencilFunc.mask );
	}

	if ( nMask & CGcmDrawState::kDirtyScissor )
	{
		if( m_scissor.enabled )
		{
			GCM_FUNC( cellGcmSetScissor, m_scissor.x, m_scissor.y, m_scissor.w, m_scissor.h );
		}
		else
		{
			GCM_FUNC( cellGcmSetScissor, 0, 0, 4095, 4095 ); // disable scissor
		}
	}

	if ( nMask & CGcmDrawState::kDirtyDepthBias )
	{
		float units = *((float*)&m_depthBias.units);
		GCM_FUNC( cellGcmSetPolygonOffset, *((float*)&m_depthBias.factor), /* NEED 2x here:see PSGL! */ 2.0f * units );
		if ( ( m_depthBias.factor != 0.0f ) || ( m_depthBias.units != 0.0f ) )
		{
			GCM_FUNC( cellGcmSetPolygonOffsetFillEnable, CELL_GCM_TRUE );
		}
		else
		{
			GCM_FUNC( cellGcmSetPolygonOffsetFillEnable, CELL_GCM_FALSE );
		}
	}

}


inline void CGcmDrawState::CommitVertexBindings(IDirect3DVertexDeclaration9 * pDecl, uint32 baseVertexIndex)
{
	// push vertex buffer state for the current vertex decl

	uint uiVertexSlotMask = m_pVertexShaderData->m_attributeInputMask;

	if ( !uiVertexSlotMask) Error(">>>>Blank vertex shader attr\n");

	for( int nStreamIndex = 0; nStreamIndex < D3D_MAX_STREAMS; ++ nStreamIndex, uiVertexSlotMask >>= 1 )
	{
		SetVertexDataArrayCache_t *pOldCache = &g_cacheSetVertexDataArray[nStreamIndex];
		// Check if this attribute is unused by the shader program
		// and try to find the match in the decl.
		if ( int j = ( uiVertexSlotMask & 1 ) ? pDecl->m_cgAttrSlots[ nStreamIndex ] : 0 )
		{
			D3DVERTEXELEMENT9_GCM *elem = &pDecl->m_elements[ j - 1 ];
			int streamIndex = elem->m_dxdecl.Stream;
			Assert( streamIndex >= 0 && streamIndex < D3D_MAX_STREAMS );

			D3DStreamDesc &dsd = g_dxGcmVertexStreamSources[ streamIndex ];

			D3DVERTEXELEMENT9_GCM::GcmDecl_t const &gcmvad = elem->m_gcmdecl;

			const uint8_t stride     = dsd.m_stride;
			const uint8_t size       = gcmvad.m_datasize;
			const uint8_t type		 = gcmvad.m_datatype;

			SetVertexDataArrayCache_t newCache( dsd, gcmvad, baseVertexIndex );

			if( *pOldCache != newCache )
			{
//				Msg(">>>>>>>>>> Offset 0x%x <<<<<<<<<<\n\n", newCache.GetLocalOffset());

				GCM_FUNC( cellGcmSetVertexDataArray, nStreamIndex, 1, stride, size, type,
						  CELL_GCM_LOCATION_LOCAL, newCache.GetLocalOffset() ); // 

//				if (!newCache.GetLocalOffset()) Error (">>>>>>>>>>>>>>>>>address %x <<<<<<<<<<<<<<<<<<<<<<\n", newCache.GetLocalOffset());


				*pOldCache = newCache;
			}
			continue;
		}
		if( !pOldCache->IsNull() )
		{
			// Disable data slot if we failed to bind proper data stream
			GCM_FUNC( cellGcmSetVertexDataArray, nStreamIndex, 1, 0, 0, CELL_GCM_VERTEX_F, CELL_GCM_LOCATION_LOCAL, 0 );
			pOldCache->SetNull(); // disable
		}
	}

}

inline void CGcmDrawState::CommitSampler(uint32 nSampler)
{
	D3DSamplerDesc const & dxsamp = m_aSamplers[ nSampler ];

#ifdef SPU

	extern CPs3gcmTextureLayout	gaLayout[D3D_MAX_TEXTURES];

	CPs3gcmTextureLayout const & texlayout = gaLayout[nSampler];

#else

	CPs3gcmTextureLayout const & texlayout = *((CPs3gcmTextureLayout const *)m_textures[ nSampler ].m_eaLayout);

#endif


	uint nMips = texlayout.m_mipCount;

	Assert( nMips > 0 );

	CPs3gcmTextureLayout::Format_t & texlayoutFormat = g_ps3texFormats[texlayout.m_nFormat];

	// If bReadsRawDepth is true, a depth texture has been set but shadow filtering has NOT been enabled. In this case, the shader is expecting to read
	// the texture as A8R8G8B8 and manually recover depth (used for depth feathering).
	bool bReadsRawDepth = ( texlayoutFormat.m_gcmFormat == CELL_GCM_TEXTURE_DEPTH24_D8 ) && !dxsamp.m_shadowFilter;

//	GCM_FUNC( cellGcmReserveMethodSize, 11 );
	uint32_t *current = gpGcmContext->current;
	current[0] = CELL_GCM_METHOD_HEADER_TEXTURE_OFFSET( nSampler, 8 );
	current[1] = CELL_GCM_METHOD_DATA_TEXTURE_OFFSET( m_textures[ nSampler ].Offset() );

	uint locn;

	if (current[1] & 1) 
	{
		locn = CELL_GCM_LOCATION_LOCAL; 
		current[1] &= 0xFFFFFFFE;
	}
	else 
	{ 
		locn = CELL_GCM_LOCATION_MAIN; 
	}

	current[2] = CELL_GCM_METHOD_DATA_TEXTURE_FORMAT(
		locn,
		texlayout.IsCubeMap() ? CELL_GCM_TRUE : CELL_GCM_FALSE,
		texlayout.IsVolumeTex() ? CELL_GCM_TEXTURE_DIMENSION_3 : CELL_GCM_TEXTURE_DIMENSION_2,
		( bReadsRawDepth
		? CELL_GCM_TEXTURE_A8R8G8B8	// bind depth textures as ARGB and reassemble depth in shader
		: texlayoutFormat.m_gcmFormat
		) |
		( texlayout.IsSwizzled() ? CELL_GCM_TEXTURE_SZ : CELL_GCM_TEXTURE_LN ),
		nMips
		);
	current[3] = CELL_GCM_METHOD_DATA_TEXTURE_ADDRESS(
		dxtogl_addressMode[ dxsamp.m_addressModeU ],
		dxtogl_addressMode[ dxsamp.m_addressModeV ],
		dxtogl_addressMode[ dxsamp.m_addressModeW ],
		CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL,
		dxsamp.m_shadowFilter ? CELL_GCM_TEXTURE_ZFUNC_GEQUAL : CELL_GCM_TEXTURE_ZFUNC_NEVER,
		( ( texlayoutFormat.m_gcmCaps & CPs3gcmTextureLayout::Format_t::kCapSRGB ) && dxsamp.m_srgb )
		? CELL_GCM_TEXTURE_GAMMA_R | CELL_GCM_TEXTURE_GAMMA_G | CELL_GCM_TEXTURE_GAMMA_B : 0,
		0
		);
	current[4] = CELL_GCM_METHOD_DATA_TEXTURE_CONTROL0( CELL_GCM_TRUE,
		(uint16)( Max<uint>( Min<uint>( dxsamp.m_maxMipLevel, nMips - 1 ), 0u ) * 256.0f ),
		(uint16)( Max<uint>( nMips - 1, 0u ) * 256.0f ),
		texlayout.IsVolumeTex() || ( ( dxsamp.m_minFilter != D3DTEXF_ANISOTROPIC ) && ( dxsamp.m_magFilter != D3DTEXF_ANISOTROPIC ) )
		? CELL_GCM_TEXTURE_MAX_ANISO_1	// 3D textures cannot have anisotropic filtering!
		: CELL_GCM_TEXTURE_MAX_ANISO_4 // dxtogl_anisoIndexHalf[ ( dxsamp.m_maxAniso / 2 ) & ( ARRAYSIZE( dxtogl_anisoIndexHalf ) - 1 ) ]
	);
	current[5] = bReadsRawDepth ? 
		CELL_GCM_REMAP_MODE( CELL_GCM_TEXTURE_REMAP_ORDER_XYXY, CELL_GCM_TEXTURE_REMAP_FROM_B, CELL_GCM_TEXTURE_REMAP_FROM_A, CELL_GCM_TEXTURE_REMAP_FROM_R, CELL_GCM_TEXTURE_REMAP_FROM_G,
		CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_REMAP, CELL_GCM_TEXTURE_REMAP_REMAP ) 
		: texlayoutFormat.m_gcmRemap;

	if( bReadsRawDepth )	
		current[6] = CELL_GCM_METHOD_DATA_TEXTURE_FILTER( 0, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_NEAREST, CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX );
	else
		current[6] = CELL_GCM_METHOD_DATA_TEXTURE_FILTER(
		0, // 0x1FBE, // 0x1FC0, // corresponding to PSGL 0 mip bias, formula: [( bias - .26 )*256] & 0x1FFF
		dxtogl_minFilter[ dxsamp.m_minFilter ][ Min( (D3DTEXTUREFILTERTYPE)dxsamp.m_mipFilter, D3DTEXF_LINEAR ) ],
		dxtogl_magFilter[ dxsamp.m_magFilter ],
		CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX
		);

	current[7] = CELL_GCM_METHOD_DATA_TEXTURE_IMAGE_RECT(
		texlayout.m_key.m_size[1],
		texlayout.m_key.m_size[0]
	);
	current[8] = CELL_GCM_METHOD_DATA_TEXTURE_BORDER_COLOR(
		0 // Border color always 0 ... dxsamp.m_borderColor // R=>>16; G=>>8; B=>>0; A=>>24 (same thing as GCM, see JSGCM_CALC_COLOR_LE_ARGB8)
		);
	current[9] = CELL_GCM_METHOD_HEADER_TEXTURE_CONTROL3( nSampler, 1 );
	current[10] = CELL_GCM_METHOD_DATA_TEXTURE_CONTROL3(
		texlayout.DefaultPitch2( g_ps3texFormats ),
		texlayout.m_key.m_size[2]
	);

	gpGcmContext->current = &current[11];
}

inline void CGcmDrawState::CommitSamplers()
{
	// Unpack from Fixed data into m_aSamplers

	for (uint32 lp = 0; lp < D3D_MAX_SAMPLERS; lp++)
	{
		uint32 SamplerIdx = m_pFixed->m_aSamplerIdx[lp];
		if (SamplerIdx != 0xFF)
			m_aSamplers[lp] = m_pFixed->m_aSamplers[SamplerIdx];
	}



	// PS3 is binding textures here
	uint mask = m_dirtySamplersMask;
	m_dirtySamplersMask = 0;

	uint16 uiPixelShaderInputMask = m_pPixelShaderData ? m_pPixelShaderData->m_samplerInputMask : 0;
	uint16 uiRunningUpBitMask = 1;

	uint nDisabledSamplers = m_nDisabledSamplers;
	m_nDisabledSamplers = 0;

	for ( int nSampler = 0; nSampler < 16; ++ nSampler, mask >>= 1, uiPixelShaderInputMask >>= 1, uiRunningUpBitMask <<= 1 )
	{
		if ( ( uiPixelShaderInputMask & 1 ) == 0 )	// The texture will not be sampled by pixel shader, unset it
		{
			// optimization
			if( !( nDisabledSamplers & uiRunningUpBitMask ) )
			{
				GCM_FUNC( cellGcmSetTextureControl, nSampler, CELL_GCM_FALSE, 0, 0, 0 );
			}
			m_dirtySamplersMask |= uiRunningUpBitMask;	// Keep the sampler dirty because we might have textures previously set on it
			m_nDisabledSamplers |= uiRunningUpBitMask; // don't disable repeatedly
			continue;
		}

		if ( ( mask & 1 ) == 0 )	// If the sampler is not dirty then don't do anything
			continue;

		if ( m_textures[nSampler].IsNull() )		// The sampler is dirty, but no texture on it, disable the sampler
		{
			// optimization
			if( !( nDisabledSamplers & uiRunningUpBitMask ) )
			{
				GCM_FUNC( cellGcmSetTextureControl, nSampler, CELL_GCM_FALSE, 0, 0, 0 );
			}
			m_nDisabledSamplers |= uiRunningUpBitMask; // don't disable repeatedly
			continue;
		}

		CommitSampler(nSampler);

	}

	m_pFixed->m_nInstanced = 0;
}

static vector unsigned int g_swap16x32m1[5] =
{
	{0x02030001, 0x14151617, 0x18191A1B, 0x1C1D1E1F},
	{0x02030001, 0x06070405, 0x18191A1B, 0x1C1D1E1F},
	{0x02030001, 0x06070405, 0x0A0B0809, 0x1C1D1E1F},
	{0x02030001, 0x06070405, 0x0A0B0809, 0x0E0F0C0D}
};


static inline void PatchUcodeConstSwap( void * pDestination, const fltx4 f4Source, int nLengthMinus1 )
{
	*( fltx4* )pDestination = vec_perm( f4Source, *( fltx4* )pDestination, ( vector unsigned char )g_swap16x32m1[nLengthMinus1] );
}

inline void CGcmDrawState::PatchUcode(fltx4 * pUCode16, uint32 * pPatchTable, uint nPatchCount )
{

	for ( uint nPatchIndex = 0; nPatchIndex < nPatchCount; ++nPatchIndex )
	{
		uint nPatchWord = pPatchTable[ nPatchIndex ], nLengthMinus1 = nPatchWord >> 30;
		uint nUcodeOffsetQword = nPatchWord & 0xFFFF;
		uint nRegister = ( nPatchWord >> 16 ) & 0x3FF;
		fltx4 & reg = g_aFPConst[nRegister];
		PatchUcodeConstSwap( pUCode16 + nUcodeOffsetQword, reg, nLengthMinus1 );
	}

}

#ifndef SPU

inline void		CGcmDrawState::AllocateUcode(FpHeader_t* pFp)
{
	uint32 patchIdx = g_ps3gcmGlobalState.m_nPatchIdx;

	uint32 uCodeSize = pFp->m_nUcodeSize;

	uint32 patchSize = AlignValue(uCodeSize + 400, 128);

	uint32 nEndPos = patchIdx + patchSize;
	uint32 nEndSeg = nEndPos/GCM_PATCHSEGSIZE;

	uint32 writeSeg = patchIdx/GCM_PATCHSEGSIZE;

	// are we out of space and so need to move to the next segment ?

	if (nEndSeg != writeSeg)
	{
		// move to the next segment

		uint32 nextSeg = (writeSeg + 1) % (GCM_PATCHBUFFSIZE/GCM_PATCHSEGSIZE);

		// Wait for RSX not to be in this segment

		uint32 readSeg = g_ps3gcmGlobalState.m_nPatchReadSeg;

		if (nextSeg == readSeg) readSeg = *g_label_fppatch_ring_seg;

		gpGcmDrawState->CmdBufferFlush();

		uint32 spins = 0;

		while (nextSeg == readSeg)
		{
			spins++;
			sys_timer_usleep(60);						// Not on SPU..
			readSeg = *g_label_fppatch_ring_seg;
		}

		//		if (spins > 0) Msg("Patch Spins %d\n", spins);

		// Move to the next segment and record the new readSeg

		patchIdx = (nextSeg * GCM_PATCHSEGSIZE);
		writeSeg = nextSeg;

		g_ps3gcmGlobalState.m_nPatchReadSeg = readSeg;

		//		Msg("New Patch Segment 0x%x\n", patchIdx);
	}


	uint8* pDst = g_ps3gcmGlobalState.m_pPatchBuff + patchIdx;

	patchIdx += patchSize;

	g_ps3gcmGlobalState.m_nPatchIdx = patchIdx;

	m_eaOutputUCode = uintp(pDst);
}

#endif

inline fltx4*	CGcmDrawState::CopyUcode(FpHeader_t* pFp)
{
	uint8* pDst = (uint8*)m_eaOutputUCode;

	uint32 patchIdx = pDst - g_ps3gcmGlobalState.m_pPatchBuff;

	uint32 uCodeSize = pFp->m_nUcodeSize;

	uint32 writeSeg = patchIdx/GCM_PATCHSEGSIZE;

#ifndef SPU
	V_memcpy(pDst, (uint8*)(pFp+1), uCodeSize);
#endif
	
	// Set the label to say we're using shaders in this part of the ring buffer now

	GCM_FUNC(cellGcmSetWriteBackEndLabel, GCM_LABEL_FPPATCH_RING_SEG, writeSeg);

	return (fltx4*) pDst;
}

inline void CGcmDrawState::BindFragmentProgram(uint32 nVertexToFragmentProgramAttributeMask)
{
	FpHeader_t * fpHeader = m_pPixelShaderData->m_eaFp;

	// Copy and Patch Ucode

	uint32* pPatches     = (uint32*)((uint8*)(fpHeader + 1) + fpHeader->m_nUcodeSize);

	fltx4* pUcode = CopyUcode(fpHeader);

#ifndef SPU
	PatchUcode(pUcode, pPatches, fpHeader->m_nPatchCount );
#else

	fltx4* pUcodeSPU = (fltx4*) (fpHeader+1);
	PatchUcode(pUcodeSPU, pPatches, fpHeader->m_nPatchCount );

	gSpuMgr.DmaSync();

	gSpuMgr.DmaPut(m_eaOutputUCode, (void*)pUcodeSPU, fpHeader->m_nUcodeSize, SPU_DMAPUT_TAG);

#endif

	// Set Fragment Shader
	
	uint32 nFragmentProgramOffset = uintp(pUcode);
	nFragmentProgramOffset += g_ps3gcmGlobalState.m_nIoOffsetDelta;

	uint32* pTexControls = pPatches + fpHeader->m_nPatchCount;

	uint nTexControls = fpHeader->m_nTexControls;
//	GCM_FUNC( cellGcmReserveMethodSize, 6 + (2 * nTexControls) );

 	CELL_GCM_METHOD_SET_SHADER_CONTROL( gpGcmContext->current, fpHeader->m_nShaderControl0 ); // +2
 	CELL_GCM_METHOD_SET_SHADER_PROGRAM( gpGcmContext->current, CELL_GCM_LOCATION_MAIN + 1, ( nFragmentProgramOffset & 0x1fffffff ) ); // +2
 	CELL_GCM_METHOD_SET_VERTEX_ATTRIB_OUTPUT_MASK( gpGcmContext->current, nVertexToFragmentProgramAttributeMask /*psh->m_attributeInputMask | 0x20*/ );  // +2  - this gets overwritten later, so it's useless here , but GPAD says "unrecognized sequence" if I don't insert this command here
 	V_memcpy( gpGcmContext->current, pTexControls, fpHeader->m_nTexControls * sizeof( uint32 ) * 2 );
 	gpGcmContext->current += 2 * nTexControls;

}

void CGcmDrawState::CommitShaders()
{
	uint nMask = m_dirtyCachesMask;
	m_dirtyCachesMask = 0;

	if( nMask & kDirtyVxCache )
	{
		GCM_FUNC(cellGcmSetInvalidateVertexCache);
	}

	if( nMask & kDirtyTxCache )
	{
		GCM_FUNC( cellGcmSetInvalidateTextureCache, CELL_GCM_INVALIDATE_TEXTURE );
	}

	if ( nMask & kDirtyVxShader )
	{
		void* pVertexShaderCmdBuffer = (void*)(m_pVertexShaderData->m_pVertexShaderCmdBuffer );
		if( pVertexShaderCmdBuffer )
		{
			uint32 nVertexShaderCmdBufferWords = m_pVertexShaderData->m_nVertexShaderCmdBufferWords;

			//			GCM_FUNC( cellGcmReserveMethodSize, nVertexShaderCmdBufferWords );
			//			uint32_t *current = gpGcmContext->current;

			V_memcpy(gpGcmContext->current, pVertexShaderCmdBuffer, nVertexShaderCmdBufferWords * sizeof( uint32 ));

			gpGcmContext->current += nVertexShaderCmdBufferWords;
		}
	}

	if ( nMask & kDirtyVxConstants )
	{
		uint nBits = m_shaderVxConstants;
		// Disabling this check because it causes lots of per-vertex dynamic lighting problems in common_vs_fxc.h function DoLighting().
		if( m_nSetTransformBranchBits != nBits )
		{
			GCM_FUNC( cellGcmSetTransformBranchBits, nBits );
			m_nSetTransformBranchBits = nBits;
		}
	}

	if ( nMask & ( kDirtyVxShader | kDirtyClipPlanes ) )
	{
		// 		GCM_FUNC( cellGcmSetUserClipPlaneControl,
		// 			( ( m_pGcmState->vertAttrOutputMask & ( 1 << ( 6 + 0 ) ) ) != 0 ) ? CELL_GCM_USER_CLIP_PLANE_ENABLE_GE : 0,
		// 			( ( m_pGcmState->vertAttrOutputMask & ( 1 << ( 6 + 1 ) ) ) != 0 ) ? CELL_GCM_USER_CLIP_PLANE_ENABLE_GE : 0,
		// 			( ( m_pGcmState->vertAttrOutputMask & ( 1 << ( 6 + 2 ) ) ) != 0 ) ? CELL_GCM_USER_CLIP_PLANE_ENABLE_GE : 0,
		// 			( ( m_pGcmState->vertAttrOutputMask & ( 1 << ( 6 + 3 ) ) ) != 0 ) ? CELL_GCM_USER_CLIP_PLANE_ENABLE_GE : 0,
		// 			( ( m_pGcmState->vertAttrOutputMask & ( 1 << ( 6 + 4 ) ) ) != 0 ) ? CELL_GCM_USER_CLIP_PLANE_ENABLE_GE : 0,
		// 			( ( m_pGcmState->vertAttrOutputMask & ( 1 << ( 6 + 5 ) ) ) != 0 ) ? CELL_GCM_USER_CLIP_PLANE_ENABLE_GE : 0
		// 			); 
	}

	uint setVertexAttribOutputMask = ( nMask & ( kDirtyVxShader | kDirtyPxShader ) );
	uint nVertexToFragmentProgramAttributeMask = m_pVertexShaderData->m_attributeOutputMask;

	if ( m_pPixelShaderData )
	{
		nVertexToFragmentProgramAttributeMask = m_pPixelShaderData->m_attributeInputMask;

		nVertexToFragmentProgramAttributeMask |= 0x20;

		BindFragmentProgram( nVertexToFragmentProgramAttributeMask );
	}
	else
	{
		// we need to set the shader, but no shader specified, so set the default empty shader
		if ( nMask & ( kDirtyPxShader | kDirtyPxConstants ) )
		{
			CELL_GCM_METHOD_SET_SHADER_CONTROL( gpGcmContext->current, g_ps3gcmGlobalState.m_nPsEmptyShaderControl0 ); // +2
			CELL_GCM_METHOD_SET_SHADER_PROGRAM( gpGcmContext->current, CELL_GCM_LOCATION_LOCAL + 1, 
												( g_ps3gcmGlobalState.m_pShaderPsEmptyBuffer.Offset() & 0x1fffffff ) ); // +2
			CELL_GCM_METHOD_SET_VERTEX_ATTRIB_OUTPUT_MASK( gpGcmContext->current, g_ps3gcmGlobalState.m_nPsEmptyAttributeInputMask | 0x20 );
			
		}
	}

	if ( setVertexAttribOutputMask )
	{
		GCM_FUNC( cellGcmSetVertexAttribOutputMask, nVertexToFragmentProgramAttributeMask );
	}

}



inline void ZeroFPConsts()
{
	memset(g_aFPConst, 0, sizeof(g_aFPConst));
}

inline void ZeroVPConsts()
{
	GCM_FUNC( cellGcmSetVertexProgramParameterBlock, 0, GCM_DS_MAXVPCONST, (float*)g_aVPConst);
}

#ifndef SPU
inline void CGcmDrawState::EndFrame()
{
	m_cmd      = CmdEndFrame;
	SendToSpu();
}
#endif


#ifndef SPU
inline void CGcmDrawState::CommitStates()
{
	m_cmd      = CmdCommitStates;
	SendToSpu();
}
#else
inline void CGcmDrawState::CommitStates()
{
	if (m_nFreeLabel) UnpackSetWriteBackEndLabel(GCM_LABEL_MEMORY_FREE, m_nFreeLabel);
	
	if ( m_dirtyStatesMask & kDirtyResetRsx) UnpackResetRsxState();

	if (m_dirtyStatesMask & kDirtyZeroAllPSConsts) ZeroFPConsts();
	if (m_dirtyStatesMask & kDirtyZeroAllVSConsts) ZeroVPConsts();

	UnpackData();					// Pulls out pixel shader consts and sets vertex shader consts
	CommitRenderStates();
}
#endif

inline void CGcmDrawState::CommitAll(IDirect3DVertexDeclaration9 * pDecl, uint32 baseVertexIndex)
{
	if (m_nFreeLabel) UnpackSetWriteBackEndLabel(GCM_LABEL_MEMORY_FREE, m_nFreeLabel);
	
	if ( m_dirtyStatesMask & kDirtyResetRsx) UnpackResetRsxState();

	if (m_dirtyStatesMask & kDirtyZeroAllPSConsts) ZeroFPConsts();
	if (m_dirtyStatesMask & kDirtyZeroAllVSConsts) ZeroVPConsts();

	UnpackData();					// Pulls out pixel shader consts and sets vertex shader consts


#ifdef SPU

	extern void GetTextureLayouts();
	GetTextureLayouts();

#endif 

	CommitRenderStates();

	CommitVertexBindings(pDecl, baseVertexIndex);

	CommitSamplers();
	CommitShaders();
}

//--------------------------------------------------------------------------------------------------
// Draw Prim
//--------------------------------------------------------------------------------------------------

#ifndef SPU

inline void	CGcmDrawState::DrawPrimitiveUP( IDirect3DVertexDeclaration9 * pDecl, D3DPRIMITIVETYPE nPrimitiveType,UINT nPrimitiveCount,
											CONST void *pVertexStreamZeroData, UINT nVertexStreamZeroStride )
{
	// Put drawcall into call buffer

	uint32 callAddr = g_ps3gcmGlobalState.DrawPrimitiveUP(nPrimitiveType, nPrimitiveCount, pVertexStreamZeroData, nVertexStreamZeroStride);

	// Allocate space to patch frag prog

	if ( m_pPixelShaderData)
	{
		AllocateUcode((FpHeader_t*)m_pPixelShaderData->m_eaFp);
	}

//	if (m_param[0] > uint32(0xD0000000) )
//		Error("Decl on Stack\n");

	m_cmd      = CmdDrawPrimUP;
	m_param[0] = uintp(pDecl);
	m_param[1] = callAddr + g_ps3gcmGlobalState.m_nIoOffsetDelta;
	m_param[2] = nVertexStreamZeroStride;

	m_param[4] = (uint32)&g_ps3texFormats;

	SendToSpu();

}

inline void CGcmDrawState::DrawIndexedPrimitive( uint32 offset, IDirect3DVertexDeclaration9 * pDecl, D3DPRIMITIVETYPE Type,INT BaseVertexIndex,UINT MinVertexIndex,
												 UINT NumVertices,UINT startIndex,UINT nDrawPrimCount )
{
	uint8 uiGcmMode = GetGcmMode(Type);
	if( !uiGcmMode ) Error("PS3 : Unsupported prim type\n");

	uint32 nPartitionStartIndex = startIndex;
	uint nPartitionPrimCount = nDrawPrimCount;
	uint32 uiGcmCount = GetGcmCount( Type, nPartitionPrimCount );

	uint32 ioMemoryIndexBuffer = offset + nPartitionStartIndex * sizeof( uint16 ) ;

	if (uiGcmCount)
	{
		if ( m_pPixelShaderData)
		{
			AllocateUcode((FpHeader_t*)m_pPixelShaderData->m_eaFp);
		}

		m_param[0] = uintp(pDecl);
		m_param[1] = BaseVertexIndex;
		m_param[2] = uiGcmMode;
		m_param[3] = ioMemoryIndexBuffer;

		m_param[4] = (uint32)&g_ps3texFormats;
		m_param[5] = uiGcmCount;

		m_cmd = CmdDrawPrim;

		SendToSpu();
	}

}

#endif

//--------------------------------------------------------------------------------------------------
// Execute command shader buffers
//--------------------------------------------------------------------------------------------------

template<class T> FORCEINLINE T GetData( uint8 *pData )
{
	return * ( reinterpret_cast< T const *>( pData ) );
}


inline void	CGcmDrawState::BindTexture2( CPs3BindTexture_t bindTex)
{
	// On SPU, we need to pull in the lmblock to get the correct offset
#ifdef SPU
	extern CPs3gcmLocalMemoryBlock gLmBlock;
	gSpuMgr.DmaGetUNSAFE(&gLmBlock, uintp(bindTex.m_pLmBlock), sizeof(gLmBlock), SPU_DMAGET_TAG );
#endif 

	// Check for same texture ?


	// Check for NULL texture ?

	uint32 stage = bindTex.m_sampler;

	if(bindTex.m_nLayout)
	{
		//		Msg("New Bind Flags %d\n", bindTex.m_nBindFlags);

		//		if(gBind != bindTex.m_nBindFlags) DebuggerBreak();

		SetSamplerState( stage, D3DSAMP_SRGBTEXTURE, ( bindTex.m_nBindFlags & (TEXTURE_BINDFLAGS_SRGBREAD>>24) ) != 0 );
		SetSamplerState( stage, D3DSAMP_SHADOWFILTER, ( bindTex.m_nBindFlags & (TEXTURE_BINDFLAGS_SHADOWDEPTH>>24) ) ? 1 : 0 );
		SetSamplerState( stage, D3DSAMP_ADDRESSU,  bindTex.m_UWrap );
		SetSamplerState( stage, D3DSAMP_ADDRESSV,  bindTex.m_VWrap );
		SetSamplerState( stage, D3DSAMP_ADDRESSW,  bindTex.m_WWrap );
		SetSamplerState( stage, D3DSAMP_MINFILTER, bindTex.m_minFilter	);
		SetSamplerState( stage, D3DSAMP_MAGFILTER, bindTex.m_magFilter	);
		SetSamplerState( stage, D3DSAMP_MIPFILTER, bindTex.m_mipFilter	);

		// 		if (m_textures[stage].m_nLocalOffset != bindTex.m_pLmBlock->Offset()) DebuggerBreak();
		// 		if (m_textures[stage].m_eaLayout != bindTex.m_nLayout) DebuggerBreak();

#ifdef SPU
		gSpuMgr.DmaDone(SPU_DMAGET_TAG_WAIT);
		bindTex.m_pLmBlock = &gLmBlock;
#endif

		m_textures[stage].m_nLocalOffset = bindTex.m_pLmBlock->Offset();
		m_textures[stage].m_eaLayout = bindTex.m_nLayout;

		if (bindTex.m_pLmBlock->IsLocalMemory() )
		{
			m_textures[stage].m_nLocalOffset |= 1;
		}

		m_dirtySamplersMask |= ( 1 << stage );

		//PackData(kDataTexture, stage, m_textures[stage].m_nLocalOffset, m_textures[stage].m_eaLayout );

		UnpackSetTexture(stage, m_textures[stage].m_nLocalOffset, m_textures[stage].m_eaLayout );

	}
	else
	{
#ifdef SPU
		gSpuMgr.DmaDone(SPU_DMAGET_TAG_WAIT);
#endif
		UnpackResetTexture(stage);
	}

}


inline void		CGcmDrawState::SetVertexShaderConstantInternal( int var, float const* pVec, int numVecs, bool bForce)
{
	GCM_FUNC( cellGcmSetVertexProgramParameterBlock, var, numVecs, pVec );
}

inline void 	CGcmDrawState::SetPixelShaderConstantInternal( int var, float const* pValues, int nNumConsts, bool bForce)
{
	V_memcpy(&g_aFPConst[var], pValues, nNumConsts * 16);
}

#ifndef SPU
#include "shaderapifast.h"
#endif

void CGcmDrawState::ExecuteCommandBuffer( uint8 *pCmdBuf )
{
#ifndef SPU

	int* pOffset = (int*) (pCmdBuf + sizeof(int)  + (2*sizeof(int)));

	for ( int i = 0; i < CBCMD_MAX_PS3TEX; i++)
	{
		uint32 offset = pOffset[i];
		if (!offset) break;

		CPs3BindParams_t* pBindParams = (CPs3BindParams_t*)(offset + pCmdBuf);
        CPs3BindTexture_t tex;
        CPs3BindTexture_t* pTex = &tex;

        pTex->m_sampler      = pBindParams->m_sampler;
        pTex->m_nBindFlags   = pBindParams->m_nBindFlags;
        pTex->m_boundStd     = pBindParams->m_boundStd;
        pTex->m_hTexture     = pBindParams->m_hTexture;

		if (pTex->m_boundStd == -1)
		{
			ShaderApiFast( pShaderAPI )->GetPs3Texture(pTex, (ShaderAPITextureHandle_t)pTex->m_hTexture);
		}
		else
		{
			ShaderApiFast( pShaderAPI )->GetPs3Texture(pTex, (StandardTextureId_t)pTex->m_boundStd);
		}

        PackData(kDataEcbTexture, (uint8) i, sizeof(CPs3BindTexture_t), pTex);
	}
#endif

    m_aECB[m_nNumECB] = pCmdBuf;
    uint32 size = *((uint32*)(pCmdBuf+4));
    m_aSizeECB[m_nNumECB] = size;

    m_nNumECB++;

    PackData(kDataECB);

}


void CGcmDrawState::UnpackExecuteCommandBuffer( uint8 *pCmdBuf )
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
				UnpackExecuteCommandBuffer( GetData<uint8 *>(  pCmdBuf + sizeof( int ) ) );
				pCmdBuf = pCmdBuf + sizeof( int ) + sizeof( uint8 *);
				break;
			}

		case CBCMD_SET_PIXEL_SHADER_FLOAT_CONST:
			{
				int nStartConst = GetData<int>( pCmdBuf + sizeof( int ) );
				int nNumConsts = GetData<int>( pCmdBuf + 2 * sizeof( int ) );
				float const *pValues = reinterpret_cast< float const *> ( pCmdBuf + 3 * sizeof( int ) );
				pCmdBuf += nNumConsts * 4 * sizeof( float ) + 3 * sizeof( int );
				SetPixelShaderConstantInternal( nStartConst, pValues, nNumConsts, false );
				break;
			}


		case CBCMD_SETPIXELSHADERFOGPARAMS:
			{
				Error("Pixel Shader Fog params not supported\n");
				break;
			}

		case CBCMD_STORE_EYE_POS_IN_PSCONST:
			{
				int nReg = GetData<int>( pCmdBuf + sizeof( int ) );
				float flWValue = GetData<float>( pCmdBuf + 2 * sizeof( int ) );
				pCmdBuf += 2 * sizeof( int ) + sizeof( float );

				float vecValue[4];
				memcpy(vecValue, m_vecWorldSpaceCameraPosition, sizeof(vecValue));
				vecValue[3] = flWValue;
						   
				SetPixelShaderConstantInternal( nReg, vecValue, 1, false );
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
				SetVertexShaderConstantInternal( nStartConst, pValues, nNumConsts, false );
				break;
			}


		case CBCMD_BIND_PS3_TEXTURE:
			{
                CPs3BindParams_t params = GetData<CPs3BindParams_t> (pCmdBuf + sizeof( int ));
				CPs3BindTexture_t tex = m_aBindTexture[params.m_nBindTexIndex];
				gpGcmDrawState->BindTexture2(  tex );
				pCmdBuf += sizeof(int) + sizeof(params);
				break;
			}


		case CBCMD_BIND_PS3_STANDARD_TEXTURE:
			{
                CPs3BindParams_t params = GetData<CPs3BindParams_t> (pCmdBuf + sizeof( int ));
                CPs3BindTexture_t tex = m_aBindTexture[params.m_nBindTexIndex];
				
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

				// Bind texture

				gpGcmDrawState->BindTexture2(  tex );

				// Twice more for bumped...

				if ( (tex.m_boundStd == TEXTURE_LIGHTMAP_BUMPED) || (tex.m_boundStd == TEXTURE_LIGHTMAP_BUMPED))
				{
					tex.m_sampler++;
					gpGcmDrawState->BindTexture2(  tex );
					tex.m_sampler++;
					gpGcmDrawState->BindTexture2(  tex );			
				}

				pCmdBuf += sizeof(int) + sizeof(params);
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

inline void		CGcmDrawState::TextureReplace(uint32 id, CPs3BindTexture_t tex)
{
	switch (id)
	{
	case TEXTURE_LOCAL_ENV_CUBEMAP:
		m_pFixed->m_nInstanced |= GCM_DS_INST_ENVMAP;
		m_pFixed->m_instanceEnvCubemap = tex;
		break;
	case TEXTURE_LIGHTMAP:
		m_pFixed->m_nInstanced |= GCM_DS_INST_LIGHTMAP;
		m_pFixed->m_instanceLightmap = tex;
		break;
	case TEXTURE_PAINT:
		m_pFixed->m_nInstanced |= GCM_DS_INST_ENVMAP;
		m_pFixed->m_instancePaintmap =  tex;
		break;
	}
}

#endif // INCLUDED_GCMDRAWSTATE_H