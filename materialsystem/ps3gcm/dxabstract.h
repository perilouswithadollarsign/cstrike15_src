//================ Copyright (c) 1996-2009 Valve Corporation. All Rights Reserved. =================
//
// LibGcm implementation of DX
//
//==================================================================================================

#ifndef DXABSTRACT_H
#define DXABSTRACT_H

#include "tier0/platform.h"
#include "tier0/memalloc.h"

#include "utlvector.h"

#include <cell/gcm.h>
#include <cell/gcm/gcm_method_data.h>
#include <cell/gcm/gcm_methods.h>
#include <sysutil/sysutil_sysparam.h>

#include "gcmconfig.h"

#include "dxabstract_def.h"
#include "gcmtexture.h"
#include "gcmlabels.h"

#define GCM_ALLOW_TIMESTAMPS 1

#ifdef _CERT
#define Debugger() ((void)0)
#else
#define Debugger() DebuggerBreak()
#endif

#define PS3GCM_ARTIFICIAL_TEXTURE_HANDLE_INDEX_BACKBUFFER 0
#define PS3GCM_ARTIFICIAL_TEXTURE_HANDLE_INDEX_DEPTHBUFFER 1

//--------------------------------------------------------------------------------------------------
// Interfaces
//--------------------------------------------------------------------------------------------------

struct IDirect3DResource9 : public IUnknown
{
	IDirect3DDevice9	*m_device;		// parent device
	D3DRESOURCETYPE		m_restype;
	
	DWORD SetPriority(DWORD PriorityNew);
};

// for the moment, a "D3D surface" is modeled as a GLM tex, a face, and a mip.
struct IDirect3DSurface9 : public IDirect3DResource9
{
	// no Create method, these are filled in by the various create surface methods.

	HRESULT LockRect(D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags);
	HRESULT UnlockRect();
	HRESULT GetDesc(D3DSURFACE_DESC *pDesc);

	// only invoke this on depth/stencil surfaces please...
	// axed HRESULT	ResetDepthStencilSurfaceSize( int Width, int Height );

	D3DSURFACE_DESC	m_desc;	// Layout must be the same as IDirect3DBaseTexture9!
	CPs3gcmTexture	*m_tex;

	int				m_face;
	int				m_mip;
	bool			m_bOwnsTexture;

	~IDirect3DSurface9() { if ( m_bOwnsTexture && m_tex ) m_tex->Release(); }
};

struct IDirect3DBaseTexture9 : public IDirect3DResource9						// "A Texture.."
{	
	D3DSURFACE_DESC		m_descZero;			// desc of top level.
	CPs3gcmTexture		*m_tex;				// this object owns data

    D3DRESOURCETYPE GetType();
    DWORD GetLevelCount();
	HRESULT GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc);

	~IDirect3DBaseTexture9() { if ( m_tex ) m_tex->Release(); }
};

struct IDirect3DTexture9 : public IDirect3DBaseTexture9							// "Texture 2D"
{	
	//CUtlVector< IDirect3DSurface9* > m_surfs;
	
	IDirect3DSurface9	*m_surfZero;			// surf of top level.  YUK!!

    HRESULT LockRect(UINT Level,D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags);
    HRESULT UnlockRect(UINT Level);
    HRESULT GetSurfaceLevel(UINT Level,IDirect3DSurface9** ppSurfaceLevel);

	~IDirect3DTexture9() { if ( m_surfZero ) m_surfZero->Release(); }
};

struct IDirect3DCubeTexture9 : public IDirect3DBaseTexture9						// "Texture Cube Map"
{
	IDirect3DSurface9	*m_surfZero[6];			// surfs of top level.  YUK!!

    HRESULT GetCubeMapSurface(D3DCUBEMAP_FACES FaceType,UINT Level,IDirect3DSurface9** ppCubeMapSurface);
    HRESULT GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc);

	~IDirect3DCubeTexture9() { for ( int j = 0; j < 6; ++ j ) if ( m_surfZero[j] ) m_surfZero[j]->Release(); }
};

struct IDirect3DVolumeTexture9 : public IDirect3DBaseTexture9					// "Texture 3D"
{
	IDirect3DSurface9	*m_surfZero;			// surf of top level.  YUK!!

	D3DVOLUME_DESC		m_volDescZero;			// volume desc top level

    HRESULT LockBox(UINT Level,D3DLOCKED_BOX* pLockedVolume,CONST D3DBOX* pBox,DWORD Flags);
    HRESULT UnlockBox(UINT Level);
	HRESULT GetLevelDesc( UINT level, D3DVOLUME_DESC *pDesc );

	~IDirect3DVolumeTexture9() { if ( m_surfZero ) m_surfZero->Release(); }
};


struct IDirect3D9 : public IUnknown
{
public:
	UINT	GetAdapterCount();			//cheese: returns 1

    HRESULT GetDeviceCaps				(UINT Adapter,D3DDEVTYPE DeviceType,D3DCAPS9* pCaps);
    HRESULT GetAdapterIdentifier		(UINT Adapter,DWORD Flags,D3DADAPTER_IDENTIFIER9* pIdentifier);
    HRESULT CheckDeviceFormat			(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,DWORD Usage,D3DRESOURCETYPE RType,D3DFORMAT CheckFormat);
    UINT	GetAdapterModeCount			(UINT Adapter,D3DFORMAT Format);
    HRESULT EnumAdapterModes			(UINT Adapter,D3DFORMAT Format,UINT Mode,D3DDISPLAYMODE* pMode);
    HRESULT CheckDeviceType				(UINT Adapter,D3DDEVTYPE DevType,D3DFORMAT AdapterFormat,D3DFORMAT BackBufferFormat,BOOL bWindowed);
    HRESULT GetAdapterDisplayMode		(UINT Adapter,D3DDISPLAYMODE* pMode);
    HRESULT CheckDepthStencilMatch		(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,D3DFORMAT RenderTargetFormat,D3DFORMAT DepthStencilFormat);
    HRESULT CheckDeviceMultiSampleType	(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT SurfaceFormat,BOOL Windowed,D3DMULTISAMPLE_TYPE MultiSampleType,DWORD* pQualityLevels);

    HRESULT CreateDevice				(UINT Adapter,D3DDEVTYPE DeviceType,VD3DHWND hFocusWindow,DWORD BehaviorFlags,D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DDevice9** ppReturnedDeviceInterface);
};

struct IDirect3DSwapChain9 : public IUnknown
{
};


struct IDirect3DQuery9 : public IUnknown
{
public:
	D3DQUERYTYPE			m_type;		// D3DQUERYTYPE_OCCLUSION or D3DQUERYTYPE_EVENT
	uint32					m_queryIdx;

	enum Flags_t
	{
		kQueryValueMask		=	0x0000FFFF,	// Mask for query value index
		kQueryFinished		=	0x80000000,	// Query is completed
		kQueryUninitialized	=	0xFFFFFFFF,	// Query hasn't started
	};

	struct QueryGlobalStateOcclusion_t
	{
		enum { kMaxQueries = GCM_REPORT_QUERY_LAST + 1 - GCM_REPORT_QUERY_FIRST, kGcmQueryBase = GCM_REPORT_QUERY_FIRST };
		CellGcmReportData volatile *m_Values[kMaxQueries];
		uint32 m_queryIdx;
		uint32 PrepareForQuery();
	};
	static QueryGlobalStateOcclusion_t s_GlobalStateOcclusion;

	struct QueryGlobalStateFence_t
	{
		enum { kMaxQueries = GCM_LABEL_QUERY_LAST + 1 - GCM_LABEL_QUERY_FIRST, kGcmLabelBase = GCM_LABEL_QUERY_FIRST };
		uint32 volatile *m_Values[kMaxQueries];
		uint32 m_queryIdx;
		uint32 PrepareForQuery();
	};
	static QueryGlobalStateFence_t s_GlobalStateFence;
	
    HRESULT Issue(DWORD dwIssueFlags);
    HRESULT GetData(void* pData,DWORD dwSize,DWORD dwGetDataFlags);
};

struct IDirect3DGcmBufferBase : public IUnknown
{
public:
	CPs3gcmBuffer			*m_pBuffer;

	HRESULT Lock(UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags);
	HRESULT Unlock();

	~IDirect3DGcmBufferBase() { if ( m_pBuffer ) m_pBuffer->Release(); }
};

struct IDirect3DVertexBuffer9 : public IDirect3DGcmBufferBase
{
public:
	D3DVERTEXBUFFER_DESC	m_vtxDesc;		// to satisfy GetDesc
};

struct IDirect3DIndexBuffer9 : public IDirect3DGcmBufferBase
{
public:
	D3DINDEXBUFFER_DESC		m_idxDesc;		// to satisfy GetDesc
	
    HRESULT GetDesc(D3DINDEXBUFFER_DESC *pDesc);
};

struct IDirect3DGcmProgramBase : public IUnknown
{
public:
	CgBinaryProgram			*m_pProgram;
	inline CGprogram GetCgProgram() const { return reinterpret_cast< CGprogram >( m_pProgram ); }
	inline void * GetProgramUCode() const { return (((char*)m_pProgram) + m_pProgram->ucode); }

	~IDirect3DGcmProgramBase() { if ( m_pProgram ) free( m_pProgram ); }
};


// define this to find out how many times we reuse the same shader during a frame
//#define DEBUG_GCM_VERTEX_SHADER_USAGE

struct IDirect3DVertexShader9 : public IDirect3DGcmProgramBase
{
public:

	VertexShader9Data_t m_data;



	//uint32 m_nIoOffsetStart; // the start of subroutine (IO Offset on RSX) that sets this vertex program
	
	~IDirect3DVertexShader9();
};


struct IDirect3DPixelShader9 : public CAlignedNewDelete< 16, IUnknown >
{
public:
	PixelShader9Data_t m_data;
public:
	//inline CgBinaryFragmentProgram *GetFragmentProgram() const { return (CgBinaryFragmentProgram *)(((char*)m_pProgram) + m_pProgram->program); }
	//void ValidateAssumptions( const char * pShaderName );
	IDirect3DPixelShader9( CgBinaryProgram* prog );
	~IDirect3DPixelShader9();
	
	#ifdef _DEBUG
	CgBinaryProgram *m_pCgProg;
	#endif
};

struct ID3DXMatrixStack : public IUnknown
{
public:
	CUtlVector<D3DMATRIX>	m_stack;
	int						m_stackTop;	// top of stack is at the highest index, this is that index.  push increases, pop decreases.
	
	HRESULT	Create( void );
	
    D3DXMATRIX* GetTop();
	void Push();
	void Pop();
	void LoadIdentity();
	void LoadMatrix( const D3DXMATRIX *pMat );
	void MultMatrix( const D3DXMATRIX *pMat );
	void MultMatrixLocal( const D3DXMATRIX *pMat );
    HRESULT ScaleLocal(FLOAT x, FLOAT y, FLOAT z);

	// Left multiply the current matrix with the computed rotation
    // matrix, counterclockwise about the given axis with the given angle.
    // (rotation is about the local origin of the object)
    HRESULT RotateAxisLocal(CONST D3DXVECTOR3* pV, FLOAT Angle);

	// Left multiply the current matrix with the computed translation
    // matrix. (transformation is about the local origin of the object)
    HRESULT TranslateLocal(FLOAT x, FLOAT y, FLOAT z);
};
typedef ID3DXMatrixStack* LPD3DXMATRIXSTACK;

struct IDirect3DDevice9Params
{
	UINT					m_adapter;
	D3DDEVTYPE				m_deviceType;
	VD3DHWND				m_focusWindow;
	DWORD					m_behaviorFlags;
	D3DPRESENT_PARAMETERS	m_presentationParameters;
};


struct D3DIndexDesc
{
	IDirect3DIndexBuffer9	*m_idxBuffer;
};

struct IDirect3DDevice9 : public IUnknown
{
	// members
	
	IDirect3DDevice9Params	m_params;						// mirror of the creation inputs

	// D3D flavor stuff
	IDirect3DSurface9			*m_rtSurfaces[16];				// current color RT (Render Target) surfaces. [0] is initially == m_defaultColorSurface
	IDirect3DSurface9			*m_dsSurface;					// current Depth Stencil Render Target surface. can be changed!
	
	IDirect3DSurface9			*m_defaultColorSurface;			// default color surface.
	IDirect3DSurface9			*m_defaultDepthStencilSurface;	// queried by GetDepthStencilSurface.
	
	IDirect3DVertexDeclaration9	*m_vertDecl;					// Set by SetVertexDeclaration...
	//D3DStreamDesc				*m_pVertexStreamSources;	// Set by SetStreamSource..
	D3DIndexDesc				m_indices;						// Set by SetIndices..
	
	IDirect3DVertexShader9		*m_vertexShader;				// Set by SetVertexShader...
	IDirect3DPixelShader9		*m_pixelShader;					// Set by SetPixelShader...

	#ifdef _DEBUG
	uint m_nDrawIndexedPrimitives;
	#endif
	
	enum AntiAliasingStatusEnum_t
	{
		AA_STATUS_NORMAL,
		AA_STATUS_PREV_FRAME, // drawing into previous frame, aliased
		AA_STATUS_DEFERRED    // drawing into deferred queue
	};
	// this is used to draw UI into already-mlaa'd-surface (to avoid AA'ing the UI)
	// when this is on, the default surface to draw should be previous flip surface
	AntiAliasingStatusEnum_t                         m_nAntiAliasingStatus; 
	// is in logical zpass? logical zpass may have wider scope than spuGcm.zPass, because logical zpass does not abort for any reason. It begins and ends with API calls. Used to balance Perf Marker Push/Pop
	bool                         m_isZPass; //
	bool                         m_isDeferredDrawQueueSurfaceSet;

	// methods
	
	// Create call invoked from IDirect3D9
	HRESULT	Create( IDirect3DDevice9Params *params );
	
	//
	// Basics
	//
	HRESULT Reset(D3DPRESENT_PARAMETERS* pPresentationParameters);
	HRESULT SetViewport(CONST D3DVIEWPORT9* pViewport);
    HRESULT BeginScene();
	HRESULT Clear(DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil);
    HRESULT EndScene();
    HRESULT Present(CONST RECT* pSourceRect,CONST RECT* pDestRect,VD3DHWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion);

	// textures
	HRESULT CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture9** ppTexture,VD3DHANDLE* pSharedHandle);
    HRESULT CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture9** ppCubeTexture,VD3DHANDLE* pSharedHandle);
    HRESULT CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture9** ppVolumeTexture,VD3DHANDLE* pSharedHandle);
	
	HRESULT SetTexture(DWORD Stage,IDirect3DBaseTexture9* pTexture);
    HRESULT GetTexture(DWORD Stage,IDirect3DBaseTexture9** ppTexture);
	
	// render targets, color and depthstencil, surfaces, blit
    HRESULT CreateRenderTarget(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable,IDirect3DSurface9** ppSurface,VD3DHANDLE* pSharedHandle);
    HRESULT SetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9* pRenderTarget);
    HRESULT GetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9** ppRenderTarget);

    HRESULT CreateOffscreenPlainSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DPOOL Pool,IDirect3DSurface9** ppSurface,VD3DHANDLE* pSharedHandle);

    HRESULT CreateDepthStencilSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Discard,IDirect3DSurface9** ppSurface,VD3DHANDLE* pSharedHandle);
    HRESULT SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil);
    HRESULT GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface);

	HRESULT GetRenderTargetData(IDirect3DSurface9* pRenderTarget,IDirect3DSurface9* pDestSurface);	// ? is anyone using this ?
    HRESULT GetFrontBufferData(UINT iSwapChain,IDirect3DSurface9* pDestSurface);
    HRESULT StretchRect(IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestSurface,CONST RECT* pDestRect,D3DTEXTUREFILTERTYPE Filter);
	
	// pixel shaders
    HRESULT CreatePixelShader(CONST DWORD* pFunction,IDirect3DPixelShader9** ppShader, const char *pShaderName = NULL, char *debugLabel = NULL);
	HRESULT SetPixelShader(IDirect3DPixelShader9* pShader);
    HRESULT SetPixelShaderConstantF(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount);
    HRESULT SetPixelShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount);
    HRESULT SetPixelShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount);

	// vertex shaders
    HRESULT CreateVertexShader(CONST DWORD* pFunction,IDirect3DVertexShader9** ppShader, char *debugLabel = NULL);
    HRESULT SetVertexShader(IDirect3DVertexShader9* pShader);
    HRESULT SetVertexShaderConstantF(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount);
    HRESULT SetVertexShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount);
    HRESULT SetVertexShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount);

	// vertex buffers
    HRESULT CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements,IDirect3DVertexDeclaration9** ppDecl);
	HRESULT SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl);

    HRESULT SetFVF(DWORD FVF);		// we might not be using these ?
	HRESULT GetFVF(DWORD* pFVF);

    HRESULT CreateVertexBuffer(UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer9** ppVertexBuffer,VD3DHANDLE* pSharedHandle);
    HRESULT SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride);
	HRESULT SetRawHardwareDataStreams( IDirect3DVertexBuffer9** ppRawHardwareDataStreams );

	// index buffers
    HRESULT CreateIndexBuffer(UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer9** ppIndexBuffer,VD3DHANDLE* pSharedHandle);
    HRESULT SetIndices(IDirect3DIndexBuffer9* pIndexData);

	// State management.
    HRESULT SetRenderState(D3DRENDERSTATETYPE State,DWORD Value);
    HRESULT SetSamplerState(DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD Value);

	// Draw.
	HRESULT	ValidateDrawPrimitiveStreams( D3DPRIMITIVETYPE Type, UINT baseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount );	// validate streams
    HRESULT DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount);
	void DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void *pVertexStreamZeroData,UINT VertexStreamZeroStride);
    HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount);
	HRESULT DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride);

	// misc
    BOOL ShowCursor(BOOL bShow);
    HRESULT ValidateDevice(DWORD* pNumPasses);
    HRESULT SetMaterial(CONST D3DMATERIAL9* pMaterial);
    HRESULT LightEnable(DWORD Index,BOOL Enable);
    HRESULT SetScissorRect(CONST RECT* pRect);
	HRESULT CreateQuery(D3DQUERYTYPE Type,IDirect3DQuery9** ppQuery);
    HRESULT GetDeviceCaps(D3DCAPS9* pCaps);
    HRESULT TestCooperativeLevel();
    HRESULT EvictManagedResources();
    HRESULT SetLight(DWORD Index,CONST D3DLIGHT9*);
    void SetGammaRamp(UINT iSwapChain,DWORD Flags,CONST D3DGAMMARAMP* pRamp);

	// Talk to JasonM about this one. It's tricky in GL.
    HRESULT SetClipPlane(DWORD Index,CONST float* pPlane);

	ULONG __stdcall Release();

	// Xbox ZPass analogue
	void BeginZPass( DWORD Flags );
	void SetPredication( DWORD PredicationMask );
	HRESULT EndZPass();
	
//	void ReloadZcullMemory( int nStencilRef );
	void StartRenderingIntoPreviousFramebuffer();
	void AntiAliasingHint( int nHint );

	//
	//
	// **** FIXED FUNCTION STUFF - None of this stuff needs support in GL.
	//
	//
    HRESULT SetTransform(D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix);
    HRESULT SetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value);	

#ifdef _PS3
	void GetGPUMemoryStats( GPUMemoryStats &stats ) { return ::GetGPUMemoryStats( stats ); }
	
	void FlushVertexCache();
	void FlushTextureCache();

	// Allocate storage for a texture's bits (if D3DUSAGE_TEXTURE_NOD3DMEMORY was used to defer allocation on creation)
	bool AllocateTextureStorage( IDirect3DBaseTexture9 *pTexture );

protected:
	// Flushing changes to GL
	void                        SetVertexStreamSource( uint i, IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride );
	void Ps3Helper_ResetSurfaceToKnownDefaultState();
	void Ps3Helper_UpdateSurface( int idx );
	friend void DxDeviceForceUpdateRenderTarget( );
#endif
};

struct ID3DXInclude
{
    virtual HRESULT Open(D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes) = 0;
    virtual HRESULT Close(LPCVOID pData) = 0;
};
typedef ID3DXInclude* LPD3DXINCLUDE;


struct ID3DXBuffer : public IUnknown
{
    void* GetBufferPointer();
    DWORD GetBufferSize();
};

typedef ID3DXBuffer* LPD3DXBUFFER;

class ID3DXConstantTable : public IUnknown
{
};
typedef ID3DXConstantTable* LPD3DXCONSTANTTABLE;

// ------------------------------------------------------------------------------------------------------------------------------ //
// D3DX stuff.
// ------------------------------------------------------------------------------------------------------------------------------ //

const char* D3DXGetPixelShaderProfile( IDirect3DDevice9 *pDevice );


D3DXMATRIX* D3DXMatrixMultiply( D3DXMATRIX *pOut, CONST D3DXMATRIX *pM1, CONST D3DXMATRIX *pM2 );
D3DXVECTOR3* D3DXVec3TransformCoord( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV, CONST D3DXMATRIX *pM );

HRESULT D3DXCreateMatrixStack( DWORD Flags, LPD3DXMATRIXSTACK* ppStack);
void D3DXMatrixIdentity( D3DXMATRIX * );

D3DXINLINE D3DXVECTOR3* D3DXVec3Subtract( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV1, CONST D3DXVECTOR3 *pV2 )
{
    pOut->x = pV1->x - pV2->x;
    pOut->y = pV1->y - pV2->y;
    pOut->z = pV1->z - pV2->z;
    return pOut;
}

D3DXINLINE D3DXVECTOR3* D3DXVec3Cross( D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV1, CONST D3DXVECTOR3 *pV2 )
{
    D3DXVECTOR3 v;

    v.x = pV1->y * pV2->z - pV1->z * pV2->y;
    v.y = pV1->z * pV2->x - pV1->x * pV2->z;
    v.z = pV1->x * pV2->y - pV1->y * pV2->x;

    *pOut = v;
    return pOut;
}

D3DXINLINE FLOAT D3DXVec3Dot( CONST D3DXVECTOR3 *pV1, CONST D3DXVECTOR3 *pV2 )
{
    return pV1->x * pV2->x + pV1->y * pV2->y + pV1->z * pV2->z;
}

D3DXMATRIX* D3DXMatrixInverse( D3DXMATRIX *pOut, FLOAT *pDeterminant, CONST D3DXMATRIX *pM );

D3DXMATRIX* D3DXMatrixTranspose( D3DXMATRIX *pOut, CONST D3DXMATRIX *pM );

D3DXPLANE* D3DXPlaneNormalize( D3DXPLANE *pOut, CONST D3DXPLANE *pP);

D3DXVECTOR4* D3DXVec4Transform( D3DXVECTOR4 *pOut, CONST D3DXVECTOR4 *pV, CONST D3DXMATRIX *pM );


D3DXVECTOR4* D3DXVec4Normalize( D3DXVECTOR4 *pOut, CONST D3DXVECTOR4 *pV );

D3DXMATRIX* D3DXMatrixTranslation( D3DXMATRIX *pOut, FLOAT x, FLOAT y, FLOAT z );

// Build an ortho projection matrix. (right-handed)
D3DXMATRIX* D3DXMatrixOrthoOffCenterRH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn,FLOAT zf );

D3DXMATRIX* D3DXMatrixPerspectiveRH( D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf );

D3DXMATRIX* D3DXMatrixPerspectiveOffCenterRH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf );

// Transform a plane by a matrix.  The vector (a,b,c) must be normal.
// M should be the inverse transpose of the transformation desired.
D3DXPLANE* D3DXPlaneTransform( D3DXPLANE *pOut, CONST D3DXPLANE *pP, CONST D3DXMATRIX *pM );

IDirect3D9 *Direct3DCreate9(UINT SDKVersion);

void D3DPERF_SetOptions( DWORD dwOptions );

HRESULT D3DXCompileShader(
        LPCSTR                          pSrcData,
        UINT                            SrcDataLen,
        CONST D3DXMACRO*                pDefines,
        LPD3DXINCLUDE                   pInclude,
        LPCSTR                          pFunctionName,
        LPCSTR                          pProfile,
        DWORD                           Flags,
        LPD3DXBUFFER*                   ppShader,
        LPD3DXBUFFER*                   ppErrorMsgs,
        LPD3DXCONSTANTTABLE*            ppConstantTable);


// fake D3D usage constant for SRGB tex creation
#define D3DUSAGE_TEXTURE_SRGB			(0x80000000L)
// fake D3D usage constant for deferred tex bits allocation
#define D3DUSAGE_TEXTURE_NOD3DMEMORY	(0x40000000L)

extern bool g_bDxMicroProfile;

#endif // DXABSTRACT_H

