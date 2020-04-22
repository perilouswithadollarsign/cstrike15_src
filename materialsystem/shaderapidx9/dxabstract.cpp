//================ Copyright (c) 1996-2009 Valve Corporation. All Rights Reserved. =================
//
//
//
//==================================================================================================

#include "dxabstract.h"
#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include <stdarg.h>
#include "tier1/strtools.h"
#include "tier1/utlbuffer.h"
//#include "dx9asmtogl.h"
#include "dx9asmtogl2.h"
#include "mathlib/vmatrix.h"

#ifdef OSX
#include "glmgr/glmgr.h"
#include "appframework/icocoamgr.h"
extern ICocoaMgr *g_extCocoaMgr;

#include <Carbon/Carbon.h>
#endif

#include "tier0/icommandline.h"
#include "tier0/memdbgon.h"

#ifdef USE_ACTUAL_DX

#pragma comment( lib, "../../dx9sdk/lib/d3d9.lib" )
#pragma comment( lib, "../../dx9sdk/lib/d3dx9.lib" )

#else

#ifdef POSIX
#define strcat_s( a, b, c) V_strcat( a, c, b )
#endif

// ------------------------------------------------------------------------------------------------------------------------------ //

bool				g_useASMTranslations = true;
//static D3DToGL_ASM	g_D3DToOpenGLTranslatorASM;	// old translator retired
static D3DToGL		g_D3DToOpenGLTranslatorASM;		// same class as the GLSL one, just invoked with different options

bool				g_useGLSLTranslations = true;
static D3DToGL		g_D3DToOpenGLTranslatorGLSL;

bool g_bUseControlFlow = false;
	
// ------------------------------------------------------------------------------------------------------------------------------ //

void GlobalMemoryStatus( MEMORYSTATUS *pOut )
{
	//cheese: return 2GB physical
	pOut->dwTotalPhys = (1<<31);
}

void Sleep( unsigned int ms )
{
	Debugger();
	ThreadSleep( ms );
}

bool IsIconic( VD3DHWND hWnd )
{
	// FIXME for now just act non-minimized all the time
	//Debugger();
	return false;
}

void GetClientRect( void *hWnd, RECT *destRect )
{
	// the only useful answer this call can offer, is the size of the canvas.
	// actually getting the window bounds is not useful.
	// so, see if a D3D device is up and running, and if so,
	// dig in and find out its backbuffer size and use that.

	uint width, height;		
	g_extCocoaMgr->RenderedSize( width, height, false );	// false = get them, don't set them
	Assert( width!=0 && height!=0 );
	
	destRect->left = 0;
	destRect->top = 0;
	destRect->right = width;
	destRect->bottom = height;		
	
	//GLMPRINTF(( "-D- GetClientRect returning rect of (0,0, %d,%d)",width,height ));
	
	return;	
}

BOOL ClientToScreen( VD3DHWND hWnd, LPPOINT pPoint )
{
	Debugger();
	return true;
}

void* GetCurrentThread()
{
	Debugger();
	return 0;
}

void SetThreadAffinityMask( void *hThread, int nMask )
{
	Debugger();
}

bool GUID::operator==( const struct _GUID &other ) const
{
	Debugger();
	return memcmp( this, &other, sizeof( GUID ) ) == 0;
}



// ------------------------------------------------------------------------------------------------------------------------------ //

#pragma mark ----- D3DXMATRIX operators

D3DXMATRIX D3DXMATRIX::operator*( const D3DXMATRIX &o ) const
{
	D3DXMATRIX result;
	
	D3DXMatrixMultiply( &result, this, &o );	// this = lhs    o = rhs    result = this * o

	return result;
}

D3DXMATRIX::operator FLOAT* ()
{
	return (float*)this;
}

float& D3DXMATRIX::operator()( int row, int column )
{
	return m[row][column];
}

const float& D3DXMATRIX::operator()( int row, int column ) const
{
	return m[row][column];
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#pragma mark ----- D3DXPLANE operators

float& D3DXPLANE::operator[]( int i )
{
	return ((float*)this)[i];
}

bool D3DXPLANE::operator==( const D3DXPLANE &o )
{
	return a == o.a && b == o.b && c == o.c && d == o.d;
}

bool D3DXPLANE::operator!=( const D3DXPLANE &o )
{
	return !( *this == o );
}

D3DXPLANE::operator float*()
{
	return (float*)this;
}

D3DXPLANE::operator const float*() const
{
	return (const float*)this;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#pragma mark ----- D3DXVECTOR2 operators

D3DXVECTOR2::operator FLOAT* ()
{
	return (float*)this;
}

D3DXVECTOR2::operator CONST FLOAT* () const
{
	return (const float*)this;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#pragma mark ----- D3DXVECTOR3 operators

D3DXVECTOR3::D3DXVECTOR3( float a, float b, float c )
{
	x = a;
	y = b;
	z = c;
}

D3DXVECTOR3::operator FLOAT* ()
{
	return (float*)this;
}

D3DXVECTOR3::operator CONST FLOAT* () const
{
	return (const float*)this;
}

// ------------------------------------------------------------------------------------------------------------------------------ //


#pragma mark ----- D3DXVECTOR4 operators

D3DXVECTOR4::D3DXVECTOR4( float a, float b, float c, float d )
{
	x = a;
	y = b;
	z = c;
	w = d;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

DWORD IDirect3DResource9::SetPriority(DWORD PriorityNew)
{
//	Debugger();
//	GLMPRINTF(( "-X- SetPriority" ));
	// no-op city
	return 0;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#pragma mark ----- IDirect3DBaseTexture9

IDirect3DBaseTexture9::~IDirect3DBaseTexture9()
{
	GLMPRINTF(( ">-A- ~IDirect3DBaseTexture9" ));

	if (m_device)
	{
		GLMPRINTF(( "-A- ~IDirect3DBaseTexture9 taking normal delete path on %08x, device is %08x ", this, m_device ));
		m_device->ReleasedTexture( this );

		if (m_tex)
		{
			GLMPRINTF(("-A- ~IDirect3DBaseTexture9 deleted '%s' @ %08x (GLM %08x) %s",m_tex->m_layout->m_layoutSummary, this, m_tex, m_tex->m_debugLabel ? m_tex->m_debugLabel : "" ));

			m_tex->m_ctx->DelTex( m_tex );
			m_tex = NULL;
		}
		else
		{
			GLMPRINTF(( "-A- ~IDirect3DBaseTexture9 : whoops, no tex to delete here ?" ));
		}		
		m_device = NULL;	// ** THIS ** is the only place to scrub this.  Don't do it in the subclass destructors.
	}
	else
	{
		GLMPRINTF(( "-A- ~IDirect3DBaseTexture9 taking strange delete path on %08x, device is %08x ", this, m_device ));
	}

	GLMPRINTF(( "<-A- ~IDirect3DBaseTexture9" ));
}

D3DRESOURCETYPE IDirect3DBaseTexture9::GetType()
{
	return m_restype;	//D3DRTYPE_TEXTURE;
}

DWORD IDirect3DBaseTexture9::GetLevelCount()
{
	return m_tex->m_layout->m_mipCount;
}

HRESULT IDirect3DBaseTexture9::GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc)
{
	Assert (Level < m_tex->m_layout->m_mipCount);

	D3DSURFACE_DESC result = m_descZero;
	// then mutate it for the level of interest
	
	GLMTexLayoutSlice *slice = &m_tex->m_layout->m_slices[ m_tex->CalcSliceIndex( 0, Level ) ];

	result.Width = slice->m_xSize;
	result.Height = slice->m_ySize;
	
	*pDesc = result;

	return S_OK;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#pragma mark ----- IDirect3DTexture9

HRESULT IDirect3DDevice9::CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture9** ppTexture,VD3DHANDLE* pSharedHandle, char *pDebugLabel )
{
	GLMPRINTF((">-A-IDirect3DDevice9::CreateTexture"));
	IDirect3DTexture9	*dxtex = new IDirect3DTexture9;
	dxtex->m_restype = D3DRTYPE_TEXTURE;
	
	dxtex->m_device		= this;

	dxtex->m_descZero.Format	= Format;
	dxtex->m_descZero.Type		= D3DRTYPE_TEXTURE;
	dxtex->m_descZero.Usage		= Usage;
	dxtex->m_descZero.Pool		= Pool;

	dxtex->m_descZero.MultiSampleType		= D3DMULTISAMPLE_NONE;
	dxtex->m_descZero.MultiSampleQuality	= 0;
	dxtex->m_descZero.Width		= Width;
	dxtex->m_descZero.Height	= Height;
	
	GLMTexLayoutKey key;
	memset( &key, 0, sizeof(key) );
	
	key.m_texGLTarget	= GL_TEXTURE_2D;
	key.m_texFormat		= Format;

	if (Levels>1)
	{
		key.m_texFlags |= kGLMTexMipped;
	}

	// http://msdn.microsoft.com/en-us/library/bb172625(VS.85).aspx
	
	// complain if any usage bits come down that I don't know.
	uint knownUsageBits = (D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_RENDERTARGET | D3DUSAGE_DYNAMIC | D3DUSAGE_TEXTURE_SRGB | D3DUSAGE_DEPTHSTENCIL);
	if ( ( Usage & knownUsageBits ) != Usage )
	{
		GLMDebugger();
	}
	
	if ( Usage & D3DUSAGE_AUTOGENMIPMAP )
	{
		key.m_texFlags |= kGLMTexMipped | kGLMTexMippedAuto;
	}
	
	if ( Usage & D3DUSAGE_DYNAMIC )
	{
		// GLMPRINTF(("-X- DYNAMIC tex usage ignored.."));	//FIXME
	}
	
	if ( Usage & D3DUSAGE_TEXTURE_SRGB )
	{
		key.m_texFlags |= kGLMTexSRGB;
	}
	
	if ( Usage & D3DUSAGE_RENDERTARGET )
	{
		Assert( !(Usage & D3DUSAGE_DEPTHSTENCIL) );
		
		key.m_texFlags |= kGLMTexRenderable;
		key.m_texFlags |= kGLMTexSRGB;			// this catches callers of CreateTexture who set the "renderable" option - they get an SRGB tex
		
		if (m_ctx->Caps().m_cantAttachSRGB)
		{
			// this config can't support SRGB render targets.  quietly turn off the sRGB bit.
			key.m_texFlags &= ~kGLMTexSRGB;
		}
	}
	
	key.m_xSize = Width;
	key.m_ySize = Height;
	key.m_zSize = 1;
	
	CGLMTex *tex = m_ctx->NewTex( &key, pDebugLabel );
	if ( !tex )
	{
		GLMDebugger();
	}
	dxtex->m_tex = tex;

	dxtex->m_srgbFlipCount = 0;

	dxtex->m_surfZero = new IDirect3DSurface9;
	dxtex->m_surfZero->m_restype = (D3DRESOURCETYPE)0;	// this is a ref to a tex, not the owner... 
	
	// do not do an AddRef here.	
	
	dxtex->m_surfZero->m_device = this;

	dxtex->m_surfZero->m_desc	=	dxtex->m_descZero;
	dxtex->m_surfZero->m_tex	=	tex;
	dxtex->m_surfZero->m_face	=	0;
	dxtex->m_surfZero->m_mip	=	0;
	
	GLMPRINTF(("-A- IDirect3DDevice9::CreateTexture created '%s' @ %08x (GLM %08x) %s",tex->m_layout->m_layoutSummary, dxtex, tex, pDebugLabel ? pDebugLabel : "" ));
	
	*ppTexture = dxtex;
	
	GLMPRINTF(("<-A-IDirect3DDevice9::CreateTexture"));
	return S_OK;
}


IDirect3DTexture9::~IDirect3DTexture9()
{
	GLMPRINTF(( ">-A- IDirect3DTexture9" ));

	// IDirect3DBaseTexture9::~IDirect3DBaseTexture9 frees up m_tex
	// we take care of surfZero

	if (m_device)
	{
		m_device->ReleasedTexture( this );

		if (m_surfZero)
		{
			ULONG refc = m_surfZero->Release( 0, "~IDirect3DTexture9 public release (surfZero)" );
			Assert( !refc );
			m_surfZero = NULL;
		}
		// leave m_device alone!
	}

	GLMPRINTF(( "<-A- IDirect3DTexture9" ));
}

HRESULT IDirect3DTexture9::LockRect(UINT Level,D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
{
	Debugger();
	return S_OK;
}

HRESULT IDirect3DTexture9::UnlockRect(UINT Level)
{
	Debugger();
	return S_OK;
}

HRESULT IDirect3DTexture9::GetSurfaceLevel(UINT Level,IDirect3DSurface9** ppSurfaceLevel)
{
	// we create and pass back a surface, and the client is on the hook to release it.  tidy.

	IDirect3DSurface9 *surf = new IDirect3DSurface9;
	surf->m_restype = (D3DRESOURCETYPE)0;	// 0 is special and means this 'surface' does not own its m_tex

	// Dicey...higher level code seems to want this and not want this.  Are we missing some AddRef/Release behavior elsewhere?
//	surf->AddRef();
	
	surf->m_device = this->m_device;
	
	GLMTexLayoutSlice *slice = &m_tex->m_layout->m_slices[ m_tex->CalcSliceIndex( 0, Level ) ];
		
	surf->m_desc = m_descZero;
		surf->m_desc.Width = slice->m_xSize;
		surf->m_desc.Height = slice->m_ySize;
		
	surf->m_tex	= m_tex;
	surf->m_face = 0;
	surf->m_mip = Level;

	*ppSurfaceLevel = surf;

	return S_OK;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#pragma mark ----- IDirect3DCubeTexture9

HRESULT IDirect3DDevice9::CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture9** ppCubeTexture,VD3DHANDLE* pSharedHandle, char *pDebugLabel)
{
	GLMPRINTF((">-A-  IDirect3DDevice9::CreateCubeTexture"));

	IDirect3DCubeTexture9	*dxtex = new IDirect3DCubeTexture9;
	dxtex->m_restype = D3DRTYPE_CUBETEXTURE;
	
	dxtex->m_device			= this;

	dxtex->m_descZero.Format	= Format;
	dxtex->m_descZero.Type		= D3DRTYPE_CUBETEXTURE;
	dxtex->m_descZero.Usage		= Usage;
	dxtex->m_descZero.Pool		= Pool;

	dxtex->m_descZero.MultiSampleType		= D3DMULTISAMPLE_NONE;
	dxtex->m_descZero.MultiSampleQuality	= 0;
	dxtex->m_descZero.Width		= EdgeLength;
	dxtex->m_descZero.Height	= EdgeLength;
	
	GLMTexLayoutKey key;
	memset( &key, 0, sizeof(key) );
	
	key.m_texGLTarget	= GL_TEXTURE_CUBE_MAP;
	key.m_texFormat		= Format;

	if (Levels>1)
	{
		key.m_texFlags |= kGLMTexMipped;
	}

	// http://msdn.microsoft.com/en-us/library/bb172625(VS.85).aspx	
	// complain if any usage bits come down that I don't know.
	uint knownUsageBits = (D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_RENDERTARGET | D3DUSAGE_DYNAMIC | D3DUSAGE_TEXTURE_SRGB);
	if ( (Usage & knownUsageBits) != Usage )
	{
		GLMDebugger();
	}
	
	if (Usage & D3DUSAGE_AUTOGENMIPMAP)
	{
		key.m_texFlags |= kGLMTexMipped | kGLMTexMippedAuto;
	}
	
	if (Usage & D3DUSAGE_RENDERTARGET)
	{
		key.m_texFlags |= kGLMTexRenderable;
	}
	
	if (Usage & D3DUSAGE_DYNAMIC)
	{
		//GLMPRINTF(("-X- DYNAMIC tex usage ignored.."));	//FIXME
	}
	
	if (Usage & D3DUSAGE_TEXTURE_SRGB)
	{
		key.m_texFlags |= kGLMTexSRGB;
	}
	
	key.m_xSize = EdgeLength;
	key.m_ySize = EdgeLength;
	key.m_zSize = 1;
	
	CGLMTex *tex = m_ctx->NewTex( &key, pDebugLabel );
	if (!tex)
	{
		GLMDebugger();
	}
	dxtex->m_tex = tex;
	
	dxtex->m_srgbFlipCount = 0;

	for( int face = 0; face < 6; face ++)
	{
		dxtex->m_surfZero[face] = new IDirect3DSurface9;
		dxtex->m_surfZero[face]->m_restype = (D3DRESOURCETYPE)0;	// 0 is special and means this 'surface' does not own its m_tex
		// do not do an AddRef here.	
		
		dxtex->m_surfZero[face]->m_device = this;
		
		dxtex->m_surfZero[face]->m_desc	=	dxtex->m_descZero;
		dxtex->m_surfZero[face]->m_tex	=	tex;
		dxtex->m_surfZero[face]->m_face	=	face;
		dxtex->m_surfZero[face]->m_mip	=	0;
	}
	
	GLMPRINTF(("-A- IDirect3DDevice9::CreateCubeTexture created '%s' @ %08x (GLM %08x)",tex->m_layout->m_layoutSummary, dxtex, tex ));
	
	*ppCubeTexture = dxtex;
	
	GLMPRINTF(("<-A- IDirect3DDevice9::CreateCubeTexture"));

	return S_OK;
}

IDirect3DCubeTexture9::~IDirect3DCubeTexture9()
{
	GLMPRINTF(( ">-A- ~IDirect3DCubeTexture9" ));

	if (m_device)
	{
		GLMPRINTF(( "-A- ~IDirect3DCubeTexture9 taking normal delete path on %08x, device is %08x, surfzero[0] is %08x ", this, m_device, m_surfZero[0] ));
		m_device->ReleasedTexture( this );

		// let IDirect3DBaseTexture9::~IDirect3DBaseTexture9 free up m_tex
		// we handle the surfZero array for the faces
		
		for( int face = 0; face < 6; face ++)
		{
			if (m_surfZero[face])
			{
				Assert( m_surfZero[face]->m_device = m_device );
				ULONG refc = m_surfZero[face]->Release( 0, "~IDirect3DCubeTexture9 public release (surfZero)");
				if ( refc!=0 )
				{
					GLMPRINTF(( "-A- ~IDirect3DCubeTexture9 seeing non zero refcount on surfzero[%d] => %d ", face, refc ));
				}
				m_surfZero[face] = NULL;
			}
		}
		// leave m_device alone!
	}
	else
	{
		GLMPRINTF(( "-A- ~IDirect3DCubeTexture9 taking strange delete path on %08x, device is %08x, surfzero[0] is %08x ", this, m_device, m_surfZero[0] ));
	}

	GLMPRINTF(( "<-A- ~IDirect3DCubeTexture9" ));
}

HRESULT IDirect3DCubeTexture9::GetCubeMapSurface(D3DCUBEMAP_FACES FaceType,UINT Level,IDirect3DSurface9** ppCubeMapSurface)
{
	// we create and pass back a surface, and the client is on the hook to release it...

	IDirect3DSurface9 *surf = new IDirect3DSurface9;
	surf->m_restype = (D3DRESOURCETYPE)0;	// 0 is special and means this 'surface' does not own its m_tex
	
	GLMTexLayoutSlice *slice = &m_tex->m_layout->m_slices[ m_tex->CalcSliceIndex( FaceType, Level ) ];
	
	surf->m_device = this->m_device;
	
	surf->m_desc = m_descZero;
		surf->m_desc.Width = slice->m_xSize;
		surf->m_desc.Height = slice->m_ySize;
		
	surf->m_tex	= m_tex;
	surf->m_face = FaceType;
	surf->m_mip = Level;

	*ppCubeMapSurface = surf;

	return S_OK;
}

HRESULT IDirect3DCubeTexture9::GetLevelDesc(UINT Level,D3DSURFACE_DESC *pDesc)
{
	Assert (Level < m_tex->m_layout->m_mipCount);

	D3DSURFACE_DESC result = m_descZero;
	// then mutate it for the level of interest
	
	GLMTexLayoutSlice *slice = &m_tex->m_layout->m_slices[ m_tex->CalcSliceIndex( 0, Level ) ];

	result.Width = slice->m_xSize;
	result.Height = slice->m_ySize;

	*pDesc = result;

	return S_OK;
}


// ------------------------------------------------------------------------------------------------------------------------------ //

#pragma mark ----- IDirect3DVolumeTexture9

HRESULT IDirect3DDevice9::CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture9** ppVolumeTexture,VD3DHANDLE* pSharedHandle, char *pDebugLabel)
{
	GLMPRINTF((">-A-  IDirect3DDevice9::CreateVolumeTexture"));
	// set dxtex->m_restype to D3DRTYPE_VOLUMETEXTURE...

	IDirect3DVolumeTexture9	*dxtex = new IDirect3DVolumeTexture9;
	dxtex->m_restype = D3DRTYPE_VOLUMETEXTURE;
	
	dxtex->m_device			= this;

	dxtex->m_descZero.Format	= Format;
	dxtex->m_descZero.Type		= D3DRTYPE_VOLUMETEXTURE;
	dxtex->m_descZero.Usage		= Usage;
	dxtex->m_descZero.Pool		= Pool;

	dxtex->m_descZero.MultiSampleType		= D3DMULTISAMPLE_NONE;
	dxtex->m_descZero.MultiSampleQuality	= 0;
	dxtex->m_descZero.Width		= Width;
	dxtex->m_descZero.Height	= Height;

	// also a volume specific desc
	dxtex->m_volDescZero.Format		= Format;
	dxtex->m_volDescZero.Type		= D3DRTYPE_VOLUMETEXTURE;
	dxtex->m_volDescZero.Usage		= Usage;
	dxtex->m_volDescZero.Pool		= Pool;

	dxtex->m_volDescZero.Width		= Width;
	dxtex->m_volDescZero.Height		= Height;
	dxtex->m_volDescZero.Depth		= Depth;
	
	GLMTexLayoutKey key;
	memset( &key, 0, sizeof(key) );
	
	key.m_texGLTarget	= GL_TEXTURE_3D;
	key.m_texFormat		= Format;

	if (Levels>1)
	{
		key.m_texFlags |= kGLMTexMipped;
	}

	// http://msdn.microsoft.com/en-us/library/bb172625(VS.85).aspx	
	// complain if any usage bits come down that I don't know.
	uint knownUsageBits = (D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_RENDERTARGET | D3DUSAGE_DYNAMIC | D3DUSAGE_TEXTURE_SRGB);
	if ( (Usage & knownUsageBits) != Usage )
	{
		Debugger();
	}
	
	if (Usage & D3DUSAGE_AUTOGENMIPMAP)
	{
		key.m_texFlags |= kGLMTexMipped | kGLMTexMippedAuto;
	}
	
	if (Usage & D3DUSAGE_RENDERTARGET)
	{
		key.m_texFlags |= kGLMTexRenderable;
	}
	
	if (Usage & D3DUSAGE_DYNAMIC)
	{
		GLMPRINTF(("-X- DYNAMIC tex usage ignored.."));	//FIXME
	}
	
	if (Usage & D3DUSAGE_TEXTURE_SRGB)
	{
		key.m_texFlags |= kGLMTexSRGB;
	}
	
	key.m_xSize = Width;
	key.m_ySize = Height;
	key.m_zSize = Depth;
	
	CGLMTex *tex = m_ctx->NewTex( &key, pDebugLabel );
	if (!tex)
	{
		Debugger();
	}
	dxtex->m_tex = tex;
	
	dxtex->m_srgbFlipCount = 0;

	dxtex->m_surfZero = new IDirect3DSurface9;
	dxtex->m_surfZero->m_restype = (D3DRESOURCETYPE)0;	// this is a ref to a tex, not the owner... 
	// do not do an AddRef here.	
	
	dxtex->m_surfZero->m_device = this;
	
	dxtex->m_surfZero->m_desc	=	dxtex->m_descZero;
	dxtex->m_surfZero->m_tex	=	tex;
	dxtex->m_surfZero->m_face	=	0;
	dxtex->m_surfZero->m_mip	=	0;
	
	GLMPRINTF(("-A- IDirect3DDevice9::CreateVolumeTexture created '%s' @ %08x (GLM %08x)",tex->m_layout->m_layoutSummary, dxtex, tex ));
	
	*ppVolumeTexture = dxtex;

	GLMPRINTF(("<-A-  IDirect3DDevice9::CreateVolumeTexture"));

	return S_OK;
}

IDirect3DVolumeTexture9::~IDirect3DVolumeTexture9()
{
	GLMPRINTF((">-A-  ~IDirect3DVolumeTexture9"));

	if (m_device)
	{
		m_device->ReleasedTexture( this );

		// let IDirect3DBaseTexture9::~IDirect3DBaseTexture9 free up m_tex
		// we handle m_surfZero
		
		if (m_surfZero)
		{
			ULONG refc = m_surfZero->Release( 0, "~IDirect3DVolumeTexture9 public release (surfZero)" );
			Assert( !refc );
			m_surfZero = NULL;
		}
		// leave m_device alone!
	}

	GLMPRINTF(("<-A-  ~IDirect3DVolumeTexture9"));
}

HRESULT IDirect3DVolumeTexture9::LockBox(UINT Level,D3DLOCKED_BOX* pLockedVolume,CONST D3DBOX* pBox,DWORD Flags)
{
	GLMTexLockParams lockreq;
	memset( &lockreq, 0, sizeof(lockreq) );
	
	lockreq.m_tex		= this->m_tex;
	lockreq.m_face		= 0;
	lockreq.m_mip		= Level;

	lockreq.m_region.xmin = pBox->Left;
	lockreq.m_region.ymin = pBox->Top;
	lockreq.m_region.zmin = pBox->Front;
	lockreq.m_region.xmax = pBox->Right;
	lockreq.m_region.ymax = pBox->Bottom;
	lockreq.m_region.zmax = pBox->Back;
	
	char	*lockAddress;
	int		yStride;
	int		zStride;
	
	lockreq.m_tex->Lock( &lockreq, &lockAddress, &yStride, &zStride );

	pLockedVolume->RowPitch = yStride;
	pLockedVolume->SlicePitch = yStride;
	pLockedVolume->pBits = lockAddress;	
	
	return S_OK;
}

HRESULT IDirect3DVolumeTexture9::UnlockBox(UINT Level)
{
	GLMTexLockParams lockreq;
	memset( &lockreq, 0, sizeof(lockreq) );
	
	lockreq.m_tex		= this->m_tex;
	lockreq.m_face		= 0;
	lockreq.m_mip		= Level;

	this->m_tex->Unlock( &lockreq );
	
	return S_OK;
}

HRESULT IDirect3DVolumeTexture9::GetLevelDesc( UINT Level, D3DVOLUME_DESC *pDesc )
{
	if (Level > m_tex->m_layout->m_mipCount)
	{
		Debugger();
	}

	D3DVOLUME_DESC result = m_volDescZero;
	// then mutate it for the level of interest
	
	GLMTexLayoutSlice *slice = &m_tex->m_layout->m_slices[ m_tex->CalcSliceIndex( 0, Level ) ];

	result.Width = slice->m_xSize;
	result.Height = slice->m_ySize;
	result.Depth = slice->m_zSize;
	
	*pDesc = result;

	return S_OK;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#pragma mark ----- IDirect3DSurface9

IDirect3DSurface9::~IDirect3DSurface9()
{
	// not much to do here, but good to verify that these things are being freed (and they are)
	//GLMPRINTF(("-A-  ~IDirect3DSurface9 - signpost"));

	if (m_device)
	{
		GLMPRINTF(("-A-  ~IDirect3DSurface9 - taking real delete path on %08x device %08x", this, m_device));
		m_device->ReleasedSurface( this );

		memset( &m_desc, 0, sizeof(m_desc) );

		if (m_restype != 0)	// signal that we are a surface that owns this tex (render target)
		{
			if (m_tex)
			{
				GLMPRINTF(("-A- ~IDirect3DSurface9 deleted '%s' @ %08x (GLM %08x) %s",m_tex->m_layout->m_layoutSummary, this, m_tex, m_tex->m_debugLabel ? m_tex->m_debugLabel : "" ));

				m_tex->m_ctx->DelTex( m_tex );
				m_tex = NULL;
			}
			else
			{
				GLMPRINTF(( "-A- ~IDirect3DSurface9 : whoops, no tex to delete here ?" ));
			}		
		}
		else
		{
			m_tex = NULL;	// we are just a view on the tex, we don't own the tex, do not delete it
		}

		m_face = m_mip = 0;
		
		m_device = NULL;
	}
	else
	{
		GLMPRINTF(("-A-  ~IDirect3DSurface9 - taking strange delete path on %08x device %08x", this, m_device));
	}
}

HRESULT IDirect3DSurface9::LockRect(D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags)
{
	GLMTexLockParams lockreq;
	memset( &lockreq, 0, sizeof(lockreq) );
	
	lockreq.m_tex	= this->m_tex;
	lockreq.m_face	= this->m_face;
	lockreq.m_mip	= this->m_mip;

	lockreq.m_region.xmin = pRect->left;
	lockreq.m_region.ymin = pRect->top;
	lockreq.m_region.zmin = 0;
	lockreq.m_region.xmax = pRect->right;
	lockreq.m_region.ymax = pRect->bottom;
	lockreq.m_region.zmax = 1;
	
	if ((Flags & (D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK)) == (D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK) )
	{
		// smells like readback, force texel readout
		lockreq.m_readback = true;
	}
	
	char	*lockAddress;
	int		yStride;
	int		zStride;
	
	lockreq.m_tex->Lock( &lockreq, &lockAddress, &yStride, &zStride );

	pLockedRect->Pitch = yStride;
	pLockedRect->pBits = lockAddress;
	
	return S_OK;
}

HRESULT IDirect3DSurface9::UnlockRect()
{
	GLMTexLockParams lockreq;
	memset( &lockreq, 0, sizeof(lockreq) );
	
	lockreq.m_tex	= this->m_tex;
	lockreq.m_face	= this->m_face;
	lockreq.m_mip	= this->m_mip;

	lockreq.m_tex->Unlock( &lockreq );

	return S_OK;
}

HRESULT IDirect3DSurface9::GetDesc(D3DSURFACE_DESC *pDesc)
{
	*pDesc = m_desc;
	return S_OK;
}


// ------------------------------------------------------------------------------------------------------------------------------ //


#pragma mark ----- IDirect3D9 -------------------------------------------------------

IDirect3D9::~IDirect3D9()
{
	GLMPRINTF(("-A-  ~IDirect3D9 - signpost"));
}

UINT IDirect3D9::GetAdapterCount()
{	
	GLMgr::NewGLMgr();				// init GL manager

	GLMDisplayDB *db = g_extCocoaMgr->GetDisplayDB();
	int dxAdapterCount = db->GetFakeAdapterCount();

	return dxAdapterCount;
}

HRESULT IDirect3D9::GetDeviceCaps(UINT Adapter,D3DDEVTYPE DeviceType,D3DCAPS9* pCaps)
{
	// Generally called from "CShaderDeviceMgrDx8::ComputeCapsFromD3D" in ShaderDeviceDX8.cpp

	// "Adapter" is used to index amongst the set of fake-adapters maintained in the display DB
	GLMDisplayDB *db = g_extCocoaMgr->GetDisplayDB();
	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;
	
	GLMRendererInfoFields	glmRendererInfo;
	GLMDisplayInfoFields	glmDisplayInfo;
	
	bool result = db->GetFakeAdapterInfo( Adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo );
	Assert (!result);
	// just leave glmRendererInfo filled out for subsequent code to look at as needed.
	
	// fill in the pCaps record for adapter... we zero most of it and just fill in the fields that we think the caller wants.
	Q_memset( pCaps, 0, sizeof(*pCaps) );
	
    /* Device Info */
	pCaps->DeviceType					=	D3DDEVTYPE_HAL;

    /* Caps from DX7 Draw */
    pCaps->Caps							=	0;									// does anyone look at this ?
	
    pCaps->Caps2						=	D3DCAPS2_DYNAMICTEXTURES;    
    /* Cursor Caps */
    pCaps->CursorCaps					=	0;									// nobody looks at this

    /* 3D Device Caps */
    pCaps->DevCaps						=	D3DDEVCAPS_HWTRANSFORMANDLIGHT;

    pCaps->TextureCaps					=	D3DPTEXTURECAPS_CUBEMAP | D3DPTEXTURECAPS_MIPCUBEMAP | D3DPTEXTURECAPS_NONPOW2CONDITIONAL | D3DPTEXTURECAPS_PROJECTED;
											// D3DPTEXTURECAPS_NOPROJECTEDBUMPENV ?
											// D3DPTEXTURECAPS_POW2 ? 
	// caller looks at POT support like this:
	//		pCaps->m_SupportsNonPow2Textures = 
	//			( !( caps.TextureCaps & D3DPTEXTURECAPS_POW2 ) || 
	//			( caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL ) );
	// so we should set D3DPTEXTURECAPS_NONPOW2CONDITIONAL bit ?


    pCaps->PrimitiveMiscCaps			=	0;									//only the HDR setup looks at this for D3DPMISCCAPS_SEPARATEALPHABLEND.
		// ? D3DPMISCCAPS_SEPARATEALPHABLEND 
		// ? D3DPMISCCAPS_BLENDOP
		// ? D3DPMISCCAPS_CLIPPLANESCALEDPOINTS
		// ? D3DPMISCCAPS_CLIPTLVERTS D3DPMISCCAPS_COLORWRITEENABLE D3DPMISCCAPS_MASKZ D3DPMISCCAPS_TSSARGTEMP
		
		
    pCaps->RasterCaps					=	D3DPRASTERCAPS_SCISSORTEST
										|	D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS	// ref'd in CShaderDeviceMgrDx8::ComputeCapsFromD3D
										|	D3DPRASTERCAPS_DEPTHBIAS			// ref'd in CShaderDeviceMgrDx8::ComputeCapsFromD3D
										;
		
    pCaps->TextureFilterCaps			=	D3DPTFILTERCAPS_MINFANISOTROPIC | D3DPTFILTERCAPS_MAGFANISOTROPIC;
    
    pCaps->MaxTextureWidth				=	4096;
	pCaps->MaxTextureHeight				=	4096;
    pCaps->MaxVolumeExtent				=	1024;	//guesses

    pCaps->MaxTextureAspectRatio		=	0;		// imply no limit on AR

    pCaps->MaxAnisotropy				=	glmRendererInfo.m_maxAniso;
    
    pCaps->TextureOpCaps				=	D3DTEXOPCAPS_ADD | D3DTEXOPCAPS_MODULATE2X;	//guess
    DWORD   MaxTextureBlendStages;
    DWORD   MaxSimultaneousTextures;

	pCaps->VertexProcessingCaps			=	D3DVTXPCAPS_TEXGEN_SPHEREMAP;
	
    pCaps->MaxActiveLights				=	8;		// guess


	// MaxUserClipPlanes.  A bit complicated..
	// it's difficult to make this fluid without teaching the engine about a cap that could change during run.

	// start it out set to '2'.
	// turn it off, if we're in GLSL mode but do not have native clip plane capability.
    pCaps->MaxUserClipPlanes			=	2;		// assume good news
	
	// is user asking for it to be off ?
	if ( CommandLine()->CheckParm( "-nouserclip" ) )
	{
		pCaps->MaxUserClipPlanes		=	0;
	}
	
	g_bUseControlFlow = CommandLine()->CheckParm( "-glslcontrolflow" );
	
	// are we ARB mode and not forcing GLSL control flow mode?
	if ( CommandLine()->CheckParm( "-arbmode" ) && !g_bUseControlFlow )
	{
		pCaps->MaxUserClipPlanes		=	0;
	}
	
    
    pCaps->MaxVertexBlendMatrices		=	0;		// see if anyone cares
    pCaps->MaxVertexBlendMatrixIndex	=	0;		// see if anyone cares

    pCaps->MaxPrimitiveCount			=	32768;	// guess
    pCaps->MaxStreams					=	4;		// guess

    pCaps->VertexShaderVersion			=	0x200;	// model 2.0
    pCaps->MaxVertexShaderConst			=	DXABSTRACT_VS_PARAM_SLOTS;	// number of vertex shader constant registers

    pCaps->PixelShaderVersion			=	0x200;	// model 2.0

    // Here are the DX9 specific ones
    pCaps->DevCaps2						=	D3DDEVCAPS2_STREAMOFFSET;
	
    pCaps->PS20Caps.NumInstructionSlots	=	512;	// guess
	// only examined once:
	// pCaps->m_SupportsPixelShaders_2_b = ( ( caps.PixelShaderVersion & 0xffff ) >= 0x0200) && (caps.PS20Caps.NumInstructionSlots >= 512);
	//pCaps->m_SupportsPixelShaders_2_b = 1;
	
	pCaps->NumSimultaneousRTs					=	1;         // Will be at least 1
    pCaps->MaxVertexShader30InstructionSlots	=	0; 
    pCaps->MaxPixelShader30InstructionSlots		=	0;

	#if ( defined ( POSIX ) && !defined( _PS3 ) )
		pCaps->FakeSRGBWrite			=	!glmRendererInfo.m_hasGammaWrites;
		pCaps->CanDoSRGBReadFromRTs		=	!glmRendererInfo.m_cantAttachSRGB;
		pCaps->MixedSizeTargets			=	glmRendererInfo.m_hasMixedAttachmentSizes;
		pCaps->SRGBDecode				=	glmRendererInfo.m_hasSRGBDecode;
	#endif

	return S_OK;
}

HRESULT IDirect3D9::GetAdapterIdentifier( UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier )
{
	// Generally called from "CShaderDeviceMgrDx8::ComputeCapsFromD3D" in ShaderDeviceDX8.cpp

	Assert( Flags == D3DENUM_WHQL_LEVEL );	// we're not handling any other queries than this yet
	
	Q_memset( pIdentifier, 0, sizeof(*pIdentifier) );

	GLMDisplayDB *db = g_extCocoaMgr->GetDisplayDB();
	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;
	
	GLMRendererInfoFields	glmRendererInfo;
	GLMDisplayInfoFields	glmDisplayInfo;
	
	// the D3D "Adapter" number feeds the fake adapter index
	bool result = db->GetFakeAdapterInfo( Adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo );
	Assert (!result);

	Q_snprintf( pIdentifier->Driver, sizeof(pIdentifier->Driver), "OpenGL %s (%08x)",
		GLMDecode( eGL_RENDERER, glmRendererInfo.m_rendererID & 0x00FFFF00 ),
		glmRendererInfo.m_rendererID
	);
	
	Q_snprintf( pIdentifier->Description, sizeof(pIdentifier->Description), "%s - %dx%d - %dMB VRAM",
		GLMDecode( eGL_RENDERER, glmRendererInfo.m_rendererID & 0x00FFFF00 ),
		glmDisplayInfo.m_displayPixelWidth, glmDisplayInfo.m_displayPixelHeight,
		glmRendererInfo.m_vidMemory >> 20 );
		
	pIdentifier->VendorId				= glmRendererInfo.m_pciVendorID;	// 4318;
	pIdentifier->DeviceId				= glmRendererInfo.m_pciDeviceID;	// 401;
	pIdentifier->SubSysId				= 0;								// 3358668866;
	pIdentifier->Revision				= 0;								// 162;
	pIdentifier->VideoMemory			= glmRendererInfo.m_vidMemory;		// amount of video memory in bytes

	#if 0
		// this came from the shaderapigl effort	
		Q_strncpy( pIdentifier->Driver, "Fake-Video-Card", MAX_DEVICE_IDENTIFIER_STRING );
		Q_strncpy( pIdentifier->Description, "Fake-Video-Card", MAX_DEVICE_IDENTIFIER_STRING );
		pIdentifier->VendorId				= 4318;
		pIdentifier->DeviceId				= 401;
		pIdentifier->SubSysId				= 3358668866;
		pIdentifier->Revision				= 162;
	#endif
	
	return S_OK;
}

HRESULT IDirect3D9::CheckDeviceFormat(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,DWORD Usage,D3DRESOURCETYPE RType,D3DFORMAT CheckFormat)
{
	if (0)	// hush for now, less spew
	{
		GLMPRINTF(("-X- ** IDirect3D9::CheckDeviceFormat:  \n -- Adapter=%d || DeviceType=%4x:%s || AdapterFormat=%8x:%s\n -- RType       %8x: %s\n -- CheckFormat %8x: %s\n -- Usage       %8x: %s",
			Adapter,													
			DeviceType,		GLMDecode(eD3D_DEVTYPE, DeviceType),				
			AdapterFormat,	GLMDecode(eD3D_FORMAT, AdapterFormat),
			RType,			GLMDecode(eD3D_RTYPE, RType),							
			CheckFormat,	GLMDecode(eD3D_FORMAT, CheckFormat),
			Usage,			GLMDecodeMask( eD3D_USAGE, Usage ) ));			
	}

	HRESULT result = D3DERR_NOTAVAILABLE;	// failure
	
	DWORD	knownUsageMask =	D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL | D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP
							|	D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_FILTER | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING
							|	D3DUSAGE_QUERY_VERTEXTEXTURE;
	
	//	FramebufferSRGB stuff.
	//	basically a format is only allowed to have SRGB usage for writing, if you have the framebuffer SRGB extension.
	//	so, check for that capability with GLM adapter db, and if it's not there, don't mark that bit as usable in any of our formats.
	GLMDisplayDB *db = g_extCocoaMgr->GetDisplayDB();
	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;
	
	GLMRendererInfoFields	glmRendererInfo;
	GLMDisplayInfoFields	glmDisplayInfo;
	
	bool dbresult = db->GetFakeAdapterInfo( Adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo );
	Assert (!dbresult);

	Assert ((Usage & knownUsageMask) == Usage);

	DWORD	legalUsage = 0;
	switch( AdapterFormat )
	{
		case	D3DFMT_X8R8G8B8:
			switch( RType )
			{
				case	D3DRTYPE_TEXTURE:
					switch( CheckFormat )
					{
						case D3DFMT_DXT1:
						case D3DFMT_DXT3:
						case D3DFMT_DXT5:
													legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage	|=	D3DUSAGE_QUERY_SRGBREAD;
													
													//open question: is auto gen of mipmaps is allowed or attempted on any DXT textures.
						break;

						case D3DFMT_A8R8G8B8:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage |=	D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
						break;

						case D3DFMT_R32F:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage |=	D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
						break;

						case D3DFMT_A16B16G16R16:
													legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage |=	D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
						break;

						case D3DFMT_A16B16G16R16F:	legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE;

													if ( !glmRendererInfo.m_atiR5xx )
													{
														legalUsage |= D3DUSAGE_QUERY_FILTER | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
													}
						break;

						case D3DFMT_A32B32G32R32F:	legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE;

													if ( !glmRendererInfo.m_atiR5xx && !glmRendererInfo.m_nvG7x )
													{
														legalUsage |= D3DUSAGE_QUERY_FILTER | D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING;
													}
						break;

						case D3DFMT_R5G6B5:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						//-----------------------------------------------------------
						// these come in from TestTextureFormat in ColorFormatDX8.cpp which is being driven by InitializeColorInformation...
						// which is going to try all 8 combinations of (vertex texturable / render targetable / filterable ) on every image format it knows.

						case D3DFMT_R8G8B8:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage	|=	D3DUSAGE_QUERY_SRGBREAD;
						break;
												
						case D3DFMT_X8R8G8B8:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
													legalUsage	|=	D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_SRGBWRITE;
						break;

							// one and two channel textures... we'll have to fake these as four channel tex if we want to support them
						case D3DFMT_L8:				legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						case D3DFMT_A8L8:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						case D3DFMT_A8:				legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;
						
							// going to need to go back and double check all of these..
						case D3DFMT_X1R5G5B5:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;
						
						case D3DFMT_A4R4G4B4:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						case D3DFMT_A1R5G5B5:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;
					
						case D3DFMT_V8U8:			legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
						break;

						case D3DFMT_Q8W8V8U8:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
							// what the heck is QWVU8 ... ?
						break;

						case D3DFMT_X8L8V8U8:		legalUsage	=	D3DUSAGE_DYNAMIC | D3DUSAGE_AUTOGENMIPMAP | D3DUSAGE_QUERY_FILTER;
							// what the heck is XLVU8 ... ?
						break;

							// formats with depth...
							
						case	D3DFMT_D16:			legalUsage =	D3DUSAGE_DYNAMIC | D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL;
							// just a guess on the legal usages
						break;

						case	D3DFMT_D24S8:		legalUsage =	D3DUSAGE_DYNAMIC | D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL;
							// just a guess on the legal usages
						break;

							// vendor formats... try marking these all invalid for now
						case	D3DFMT_NV_INTZ:
						case	D3DFMT_NV_RAWZ:
						case	D3DFMT_NV_NULL:
						case	D3DFMT_ATI_D16:
						case	D3DFMT_ATI_D24S8:
						case	D3DFMT_ATI_2N:
						case	D3DFMT_ATI_1N:
							legalUsage = 0;
						break;

						//-----------------------------------------------------------
												
						default:
							Assert(!"Unknown check format");
							result = D3DERR_NOTAVAILABLE;
						break;
					}

					if ((Usage & legalUsage) == Usage)
					{
						result = S_OK;
					}
					else
					{
						DWORD unsatBits = Usage & (~legalUsage);	// clear the bits of the req that were legal, leaving the illegal ones
						GLMPRINTF(( "-X- --> NOT OK: flags %8x:%s", unsatBits,GLMDecodeMask( eD3D_USAGE, unsatBits ) ));
						result = D3DERR_NOTAVAILABLE;
					}
				break;				
				
				case	D3DRTYPE_SURFACE:
					switch( CheckFormat )
					{
						case	0x434f5441:
						case	0x41415353:
							result = D3DERR_NOTAVAILABLE;
						break;
							
						case	D3DFMT_D24S8:
							result = S_OK;
						break;
						//** IDirect3D9::CheckDeviceFormat  adapter=0, DeviceType=   1:D3DDEVTYPE_HAL, AdapterFormat=       5:D3DFMT_X8R8G8B8, RType=   1:D3DRTYPE_SURFACE, CheckFormat=434f5441:UNKNOWN
						//** IDirect3D9::CheckDeviceFormat  adapter=0, DeviceType=   1:D3DDEVTYPE_HAL, AdapterFormat=       5:D3DFMT_X8R8G8B8, RType=   1:D3DRTYPE_SURFACE, CheckFormat=41415353:UNKNOWN
						//** IDirect3D9::CheckDeviceFormat  adapter=0, DeviceType=   1:D3DDEVTYPE_HAL, AdapterFormat=       5:D3DFMT_X8R8G8B8, RType=   1:D3DRTYPE_SURFACE, CheckFormat=434f5441:UNKNOWN
						//** IDirect3D9::CheckDeviceFormat  adapter=0, DeviceType=   1:D3DDEVTYPE_HAL, AdapterFormat=       5:D3DFMT_X8R8G8B8, RType=   1:D3DRTYPE_SURFACE, CheckFormat=41415353:UNKNOWN
					}
				break;
				
				default:
					Assert(!"Unknown resource type");
					result = D3DERR_NOTAVAILABLE;
				break;
			}
		break;
		
		default:
			Assert(!"Unknown adapter format");
			result = D3DERR_NOTAVAILABLE;
		break;
	}
	
	return result;
}

UINT IDirect3D9::GetAdapterModeCount(UINT Adapter,D3DFORMAT Format)
{
	GLMPRINTF(( "-X- IDirect3D9::GetAdapterModeCount: Adapter=%d || Format=%8x:%s", Adapter, Format, GLMDecode(eD3D_FORMAT, Format) ));

	uint modeCount=0;
	
	GLMDisplayDB *db = g_extCocoaMgr->GetDisplayDB();
	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;
	
	GLMRendererInfoFields	glmRendererInfo;
	GLMDisplayInfoFields	glmDisplayInfo;
	
	// the D3D "Adapter" number feeds the fake adapter index
	bool result = db->GetFakeAdapterInfo( Adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo );
	Assert (!result);

	modeCount = db->GetModeCount( glmRendererIndex, glmDisplayIndex );
	GLMPRINTF(( "-X-   --> result is %d", modeCount ));		
	
	return modeCount + 1; // Add one on for 800 x 500, which we'll tack on as mode 0 below
}

HRESULT IDirect3D9::EnumAdapterModes( UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode )
{
	GLMPRINTF(( "-X- IDirect3D9::EnumAdapterModes:    Adapter=%d || Format=%8x:%s || Mode=%d", Adapter, Format, GLMDecode(eD3D_FORMAT, Format), Mode ));

	Assert(Format==D3DFMT_X8R8G8B8);

	GLMDisplayDB *db = g_extCocoaMgr->GetDisplayDB();
	
	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;
	
	GLMRendererInfoFields		glmRendererInfo;
	GLMDisplayInfoFields		glmDisplayInfo;
	GLMDisplayModeInfoFields	glmModeInfo;
	
	// The D3D "Adapter" number feeds the fake adapter index
	bool result = db->GetFakeAdapterInfo( Adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo );
	Assert ( !result );
	if ( result )
		return D3DERR_NOTAVAILABLE;

	bool result2 = db->GetModeInfo( glmRendererIndex, glmDisplayIndex, Mode == 0 ? 0 : Mode - 1, &glmModeInfo );  // End up asking Cocoa for mode zero twice
	Assert( !result2 );
	if ( result2 )
		return D3DERR_NOTAVAILABLE;

	pMode->Width		= Mode == 0 ? 800 : glmModeInfo.m_modePixelWidth;   // substitute in width of 800 on first "Mode zero"
	pMode->Height		= Mode == 0 ? 500 : glmModeInfo.m_modePixelHeight;  // substitute in height of 500 on first "Mode zero"
	pMode->RefreshRate	= glmModeInfo.m_modeRefreshHz;		// "adapter default"
	pMode->Format		= Format;							// whatever you asked for ?
	
	GLMPRINTF(( "-X- IDirect3D9::EnumAdapterModes returning mode size (%d,%d) and D3DFMT_X8R8G8B8", pMode->Width, pMode->Height ));
	return S_OK;	
}

HRESULT IDirect3D9::CheckDeviceType(UINT Adapter,D3DDEVTYPE DevType,D3DFORMAT AdapterFormat,D3DFORMAT BackBufferFormat,BOOL bWindowed)
{
	//FIXME: we just say "OK" on any query

	GLMPRINTF(( "-X- IDirect3D9::CheckDeviceType:    Adapter=%d || DevType=%d:%s || AdapterFormat=%d:%s || BackBufferFormat=%d:%s || bWindowed=%d",
		Adapter,
		DevType, GLMDecode(eD3D_DEVTYPE,DevType),
		AdapterFormat, GLMDecode(eD3D_FORMAT, AdapterFormat),
		BackBufferFormat, GLMDecode(eD3D_FORMAT, BackBufferFormat),
		(int) bWindowed ));
		
	return S_OK;
}

HRESULT IDirect3D9::GetAdapterDisplayMode(UINT Adapter,D3DDISPLAYMODE* pMode)
{
	// asking what the current mode is
	GLMPRINTF(("-X- IDirect3D9::GetAdapterDisplayMode: Adapter=%d", Adapter ));

	GLMDisplayDB *db = g_extCocoaMgr->GetDisplayDB();

	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;
	
	GLMRendererInfoFields		glmRendererInfo;
	GLMDisplayInfoFields		glmDisplayInfo;
	GLMDisplayModeInfoFields	glmModeInfo;
	
	// the D3D "Adapter" number feeds the fake adapter index
	bool result = db->GetFakeAdapterInfo( Adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo );
	Assert(!result);
		if (result)	return D3DERR_INVALIDCALL;

	int modeIndex = -1;	// pass -1 as a mode index to find out about whatever the current mode is on the selected display

	bool modeResult = db->GetModeInfo( glmRendererIndex, glmDisplayIndex, modeIndex, &glmModeInfo );
	Assert (!modeResult);
		if (modeResult)	return D3DERR_INVALIDCALL;

	pMode->Width		= glmModeInfo.m_modePixelWidth;
	pMode->Height		= glmModeInfo.m_modePixelHeight;
	pMode->RefreshRate	= glmModeInfo.m_modeRefreshHz;		// "adapter default"
	pMode->Format		= D3DFMT_X8R8G8B8;					//FIXME, this is a SWAG

	return S_OK;
}

HRESULT IDirect3D9::CheckDepthStencilMatch(UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT AdapterFormat,D3DFORMAT RenderTargetFormat,D3DFORMAT DepthStencilFormat)
{
	GLMPRINTF(("-X- IDirect3D9::CheckDepthStencilMatch:    Adapter=%d || DevType=%d:%s || AdapterFormat=%d:%s || RenderTargetFormat=%d:%s || DepthStencilFormat=%d:%s",
		Adapter,
		DeviceType, GLMDecode(eD3D_DEVTYPE,DeviceType),
		AdapterFormat, GLMDecode(eD3D_FORMAT, AdapterFormat),
		RenderTargetFormat, GLMDecode(eD3D_FORMAT, RenderTargetFormat),
		DepthStencilFormat, GLMDecode(eD3D_FORMAT, DepthStencilFormat) ));
	
	// one known request looks like this:
	// AdapterFormat=5:D3DFMT_X8R8G8B8 || RenderTargetFormat=3:D3DFMT_A8R8G8B8 || DepthStencilFormat=2:D3DFMT_D24S8
	
	// return S_OK for that one combo, Debugger() on anything else
	HRESULT result = D3DERR_NOTAVAILABLE;	// failure
	
	switch( AdapterFormat )
	{
		case	D3DFMT_X8R8G8B8:
		{
			if ( (RenderTargetFormat == D3DFMT_A8R8G8B8) && (DepthStencilFormat == D3DFMT_D24S8) )
			{
				result = S_OK;
			}
		}
		break;
	}
	
	Assert( result == S_OK );

	return result;
}

HRESULT IDirect3D9::CheckDeviceMultiSampleType( UINT Adapter,D3DDEVTYPE DeviceType,D3DFORMAT SurfaceFormat,BOOL Windowed,D3DMULTISAMPLE_TYPE MultiSampleType,DWORD* pQualityLevels )
{
	GLMDisplayDB *db = g_extCocoaMgr->GetDisplayDB();

	int glmRendererIndex = -1;
	int glmDisplayIndex = -1;
	
	GLMRendererInfoFields		glmRendererInfo;
	GLMDisplayInfoFields		glmDisplayInfo;
	GLMDisplayModeInfoFields	glmModeInfo;
	
	// the D3D "Adapter" number feeds the fake adapter index
	bool result = db->GetFakeAdapterInfo( Adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo );
	Assert( !result );
	if ( result )
		return D3DERR_INVALIDCALL;

	
	if ( !CommandLine()->FindParm("-glmenabletrustmsaa") )
	{
		// These ghetto drivers don't get MSAA
		if ( ( glmRendererInfo.m_nvG7x || glmRendererInfo.m_atiR5xx ) && ( MultiSampleType > D3DMULTISAMPLE_NONE ) )
		{
			if ( pQualityLevels )
			{
				*pQualityLevels = 0;
			}
			return D3DERR_NOTAVAILABLE;
		}
	}

	switch ( MultiSampleType )
	{
		case D3DMULTISAMPLE_NONE:		// always return true
			if ( pQualityLevels )
			{
				*pQualityLevels = 1;
			}
			return S_OK;
		break;

		case D3DMULTISAMPLE_2_SAMPLES:
		case D3DMULTISAMPLE_4_SAMPLES:
		case D3DMULTISAMPLE_6_SAMPLES:
		case D3DMULTISAMPLE_8_SAMPLES:
			// note the fact that the d3d enums for 2, 4, 6, 8 samples are equal to 2,4,6,8...
			if (glmRendererInfo.m_maxSamples >= (int)MultiSampleType )
			{
				if ( pQualityLevels )
				{
					*pQualityLevels = 1;
				}
				return S_OK;
			}
			else
			{
				return D3DERR_NOTAVAILABLE;
			}
		break;
		
		default:
			if ( pQualityLevels )
			{
				*pQualityLevels = 0;
			}
			return D3DERR_NOTAVAILABLE;
		break;
	}
	return D3DERR_NOTAVAILABLE;
}

HRESULT IDirect3D9::CreateDevice(UINT Adapter,D3DDEVTYPE DeviceType,VD3DHWND hFocusWindow,DWORD BehaviorFlags,D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DDevice9** ppReturnedDeviceInterface)
{
	// constrain these inputs for the time being
	// BackBufferFormat			-> A8R8G8B8
	// BackBufferCount			-> 1;
	// MultiSampleType			-> D3DMULTISAMPLE_NONE
	// AutoDepthStencilFormat	-> D3DFMT_D24S8
	
	// NULL out the return pointer so if we exit early it is not set
	*ppReturnedDeviceInterface = NULL;
	
	// assume success unless something is sour
	HRESULT result = S_OK;
	
	// relax this check for now
	//if (pPresentationParameters->BackBufferFormat != D3DFMT_A8R8G8B8)
	//{
	//	Debugger();
	//	result = -1;
	//}
	
	//rbarris 24Aug10 - relaxing this check - we don't care if the game asks for two backbuffers, it's moot
	//if ( pPresentationParameters->BackBufferCount != 1 )
	//{
	//	Debugger();
	//	result = D3DERR_NOTAVAILABLE;
	//}
		
	if ( pPresentationParameters->AutoDepthStencilFormat != D3DFMT_D24S8 )
	{
		Debugger();
		result = D3DERR_NOTAVAILABLE;
	}

	if ( result == S_OK )
	{
		// create an IDirect3DDevice9
		// it will make a GLMContext and set up some drawables

		IDirect3DDevice9Params	devparams;
		memset( &devparams, 0, sizeof(devparams) );
		
		devparams.m_adapter					= Adapter;
		devparams.m_deviceType				= DeviceType;
		devparams.m_focusWindow				= hFocusWindow;				// is this meaningful?  is this a WindowRef ?  follow it up the chain..
		devparams.m_behaviorFlags			= BehaviorFlags;
		devparams.m_presentationParameters	= *pPresentationParameters;

		IDirect3DDevice9 *dev = new IDirect3DDevice9;
		
		result = dev->Create( &devparams );
		
		if ( result == S_OK )
		{
			*ppReturnedDeviceInterface = dev;
		}
	}
	return result;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#pragma mark ----- IDirect3DQuery9

HRESULT IDirect3DQuery9::Issue(DWORD dwIssueFlags)
{
	// Flags field for Issue
	//	#define D3DISSUE_END (1 << 0) // Tells the runtime to issue the end of a query, changing it's state to "non-signaled".
	//	#define D3DISSUE_BEGIN (1 << 1) // Tells the runtime to issue the beginng of a query.
	
	if (dwIssueFlags & D3DISSUE_BEGIN)
	{
		switch( m_type )
		{
			case	D3DQUERYTYPE_OCCLUSION:
				m_query->Start();	// drop "start counter" call into stream
			break;

			default:
				Assert(!"Can't use D3DISSUE_BEGIN on this query");
			break;
		}
	}
	
	if (dwIssueFlags & D3DISSUE_END)
	{
		switch( m_type )
		{
			case	D3DQUERYTYPE_OCCLUSION:
				m_query->Stop();	// drop "end counter" call into stream
			break;
			
			case	D3DQUERYTYPE_EVENT:
				// End is very weird with respect to Events (fences).
				// DX9 docs say to use End to put the fence in the stream.  So we map End to GLM's Start.
				// http://msdn.microsoft.com/en-us/library/ee422167(VS.85).aspx
				m_query->Start();	// drop "set fence" into stream
			break;
		}
	}
	return S_OK;
}

HRESULT IDirect3DQuery9::GetData(void* pData,DWORD dwSize,DWORD dwGetDataFlags)
{
	HRESULT	result = -1;
	
	// GetData is not always called with the flush bit.
		
	// if an answer is not yet available - return S_FALSE.
	// if an answer is available - return S_OK and write the answer into *pData.
	bool done = false;
	bool flush = (dwGetDataFlags & D3DGETDATA_FLUSH) != 0;	// aka spin until done

	// hmmm both of these paths are the same, maybe we could fold them up
	if ( !m_query->IsStarted() )
	{
		Assert(!"Can't GetData before issue/start");
		printf("\n** IDirect3DQuery9::GetData: can't GetData before issue/start");
		result = -1;
	}
	else if ( !m_query->IsStopped() )
	{
		Assert(!"Can't GetData before issue-end/stop");
		printf("\n** IDirect3DQuery9::GetData: can't GetData before issue-end/stop");
		result = -1;
	}
	else
	{
		switch( m_type )
		{
			case	D3DQUERYTYPE_OCCLUSION:
			{
				// expectation - caller already did an issue begin (start) and an issue end (stop).
				// we can probe using IsDone.
				if (flush && (!m_ctx->Caps().m_hasPerfPackage1) )
				{
					glFlush();
				}
				do
				{
					done = m_query->IsDone();
					if (done)
					{
						uint oqValue = 0;				// or we could just pass pData directly to Complete...
						m_query->Complete(&oqValue);
						if (pData)
						{
							*(uint*)pData = oqValue;
						}					
						result = S_OK;
					}
					else
					{
						result = S_FALSE;
					}
				}	while( flush && (!done) );
			}
			break;

			case	D3DQUERYTYPE_EVENT:
			{
				// expectation - caller already did an issue end (for fence => start) but has not done anything that would call Stop.
				// that's ok because Stop is a no-op for fences.
				if (flush && (!m_ctx->Caps().m_hasPerfPackage1) )
				{
					glFlush();
				}

				done = m_query->IsDone();
				if (done)
				{
					m_query->Complete(NULL);	// this will block on pre-SLGU
					*(uint*)pData = 0;
					result = S_OK;
				}
				else
				{
					result = S_FALSE;
				}
			}
			break;
		}
	}

	return result;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

#pragma mark ----- IDirect3DVertexBuffer9

HRESULT IDirect3DDevice9::CreateVertexBuffer(UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer9** ppVertexBuffer,VD3DHANDLE* pSharedHandle)
{
	GLMPRINTF(( ">-A- IDirect3DDevice9::CreateVertexBuffer" ));
	
	IDirect3DVertexBuffer9 *newbuff = new IDirect3DVertexBuffer9;
	
	newbuff->m_device = this;
	
	newbuff->m_ctx = m_ctx;

		// FIXME need to find home or use for the Usage, FVF, Pool values passed in
	uint options = 0;
	
	if (Usage&D3DUSAGE_DYNAMIC)
	{
		options |= GLMBufferOptionDynamic;
	}
	
	newbuff->m_vtxBuffer = m_ctx->NewBuffer( kGLMVertexBuffer, Length, options  ) ;
	
	newbuff->m_vtxDesc.Type		= D3DRTYPE_VERTEXBUFFER;
	newbuff->m_vtxDesc.Usage	= Usage;
	newbuff->m_vtxDesc.Pool		= Pool;
	newbuff->m_vtxDesc.Size		= Length;

	*ppVertexBuffer = newbuff;
	
	GLMPRINTF(( "<-A- IDirect3DDevice9::CreateVertexBuffer" ));

	return S_OK;
}

IDirect3DVertexBuffer9::~IDirect3DVertexBuffer9()
{
	GLMPRINTF(( ">-A- ~IDirect3DVertexBuffer9" ));
	
	if (m_device)
	{
		m_device->ReleasedVertexBuffer( this );

		if (m_ctx && m_vtxBuffer)
		{
			GLMPRINTF(( ">-A- ~IDirect3DVertexBuffer9 deleting m_vtxBuffer" ));
			m_ctx->DelBuffer( m_vtxBuffer );
			m_vtxBuffer = NULL;
			GLMPRINTF(( "<-A- ~IDirect3DVertexBuffer9 deleting m_vtxBuffer - done" ));
		}
		m_device = NULL;
	}
	
	GLMPRINTF(( "<-A- ~IDirect3DVertexBuffer9" ));
}

HRESULT IDirect3DVertexBuffer9::Lock(UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags)
{
	// FIXME would be good to have "can't lock twice" logic

	Assert( !(Flags & D3DLOCK_READONLY) );	// not impl'd
//	Assert( !(Flags & D3DLOCK_NOSYSLOCK) );	// not impl'd - it triggers though
	
	GLMBuffLockParams lockreq;
	lockreq.m_offset		= OffsetToLock;
	lockreq.m_size			= SizeToLock;
	lockreq.m_discard		= (Flags & D3DLOCK_DISCARD) != 0;
	lockreq.m_nonblocking	= ( (Flags & D3DLOCK_NOOVERWRITE) != 0 ) || lockreq.m_discard;
		
	m_vtxBuffer->Lock( &lockreq, (char**)ppbData );

	GLMPRINTF(("-X- IDirect3DDevice9::Lock on D3D buf %p (GL name %d) offset %d, size %d => address %p", this, this->m_vtxBuffer->m_name, OffsetToLock, SizeToLock, *ppbData));
	return S_OK;
}

HRESULT IDirect3DVertexBuffer9::Unlock()
{
	m_vtxBuffer->Unlock();
	return S_OK;
}

// ------------------------------------------------------------------------------------------------------------------------------ //


#pragma mark ----- IDirect3DIndexBuffer9

HRESULT IDirect3DDevice9::CreateIndexBuffer(UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer9** ppIndexBuffer,VD3DHANDLE* pSharedHandle)
{
	GLMPRINTF(( ">-A- IDirect3DDevice9::CreateIndexBuffer" ));

	// it is important to save all the create info, since GetDesc could get called later to query it
	
	IDirect3DIndexBuffer9 *newbuff = new IDirect3DIndexBuffer9;

	newbuff->m_device = this;

	newbuff->m_restype = D3DRTYPE_INDEXBUFFER;		//	hmmmmmmm why are we not derived from d3dresource..
		
	newbuff->m_ctx = m_ctx;
	
		// FIXME need to find home or use for the Usage, Format, Pool values passed in
	uint options = 0;
	
	if (Usage&D3DUSAGE_DYNAMIC)
	{
		options |= GLMBufferOptionDynamic;
	}

	newbuff->m_idxBuffer = m_ctx->NewBuffer( kGLMIndexBuffer, Length, options ) ;
	
	newbuff->m_idxDesc.Format	= Format;
	newbuff->m_idxDesc.Type		= D3DRTYPE_INDEXBUFFER;
	newbuff->m_idxDesc.Usage	= Usage;
	newbuff->m_idxDesc.Pool		= Pool;
	newbuff->m_idxDesc.Size		= Length;

	*ppIndexBuffer = newbuff;

	GLMPRINTF(( "<-A- IDirect3DDevice9::CreateIndexBuffer" ));

	return S_OK;
}

IDirect3DIndexBuffer9::~IDirect3DIndexBuffer9()
{
	GLMPRINTF(( ">-A- ~IDirect3DIndexBuffer9" ));
	
	if (m_device)
	{
		m_device->ReleasedIndexBuffer( this );

		if (m_ctx && m_idxBuffer)
		{
			GLMPRINTF(( ">-A- ~IDirect3DIndexBuffer9 deleting m_idxBuffer" ));
			m_ctx->DelBuffer( m_idxBuffer );
			GLMPRINTF(( "<-A- ~IDirect3DIndexBuffer9 deleting m_idxBuffer - done" ));
		}
		m_device = NULL;
	}
	else
	{
	}
	
	GLMPRINTF(( "<-A- ~IDirect3DIndexBuffer9" ));
}


HRESULT IDirect3DIndexBuffer9::Lock(UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags)
{
	// FIXME would be good to have "can't lock twice" logic
	
	GLMBuffLockParams lockreq;
	lockreq.m_offset		= OffsetToLock;
	lockreq.m_size			= SizeToLock;
	lockreq.m_discard		= (Flags & D3DLOCK_DISCARD) != 0;
	lockreq.m_nonblocking	= ( (Flags & D3DLOCK_NOOVERWRITE) != 0 ) || lockreq.m_discard;

	m_idxBuffer->Lock( &lockreq, (char**)ppbData );

	return S_OK;
}

HRESULT IDirect3DIndexBuffer9::Unlock()
{
	m_idxBuffer->Unlock();

	return S_OK;
}

HRESULT IDirect3DIndexBuffer9::GetDesc(D3DINDEXBUFFER_DESC *pDesc)
{
	*pDesc = m_idxDesc;
	return S_OK;
}


// ------------------------------------------------------------------------------------------------------------------------------ //

#pragma mark ----- IDirect3DDevice9 -------------------------------------------------

void	ConvertPresentationParamsToGLMDisplayParams( D3DPRESENT_PARAMETERS *d3dp, GLMDisplayParams *gldp )
{
	memset( gldp, 0, sizeof(*gldp) );

	gldp->m_fsEnable					=	!d3dp->Windowed;

	// see http://msdn.microsoft.com/en-us/library/ee416515(VS.85).aspx
	// note that the values below are the only ones mentioned by Source engine; there are many others
	switch(d3dp->PresentationInterval)
	{
		case D3DPRESENT_INTERVAL_ONE:
			gldp->m_vsyncEnable					=	true;	// "The driver will wait for the vertical retrace period (the runtime will beam-follow to prevent tearing)."
		break;

		case D3DPRESENT_INTERVAL_IMMEDIATE:
			gldp->m_vsyncEnable					=	false;	// "The runtime updates the window client area immediately and might do so more than once during the adapter refresh period."
		break;
		
		default:
			gldp->m_vsyncEnable					=	true;	// if I don't know it, you're getting vsync enabled.
		break;
	}
	
	gldp->m_backBufferWidth				=	d3dp->BackBufferWidth;
	gldp->m_backBufferHeight			=	d3dp->BackBufferHeight;
	gldp->m_backBufferFormat			=	d3dp->BackBufferFormat;
	gldp->m_multiSampleCount			=	d3dp->MultiSampleType;	// it's a count really

	gldp->m_enableAutoDepthStencil		=	d3dp->EnableAutoDepthStencil;
	gldp->m_autoDepthStencilFormat		=	d3dp->AutoDepthStencilFormat;

	gldp->m_fsRefreshHz					=	d3dp->FullScreen_RefreshRateInHz;
	
	// some fields in d3d PB we're not acting on yet...
	//	UINT                BackBufferCount;
	//	DWORD               MultiSampleQuality;
	//	D3DSWAPEFFECT       SwapEffect;
	//	VD3DHWND            hDeviceWindow;
	//	DWORD               Flags;
}

HRESULT	IDirect3DDevice9::Create( IDirect3DDevice9Params *params )
{
	GLMPRINTF((">-X-IDirect3DDevice9::Create"));
	HRESULT result = S_OK;
	
	// create an IDirect3DDevice9
	// make a GLMContext and set up some drawables
	m_params		=	*params;
	
	m_ctx			=	NULL;
	m_drawableFBO	=	NULL;

	memset( m_rtSurfaces, 0, sizeof(m_rtSurfaces) );
	m_dsSurface = NULL;
	
	m_defaultColorSurface = NULL;
	m_defaultDepthStencilSurface = NULL;
	
	memset( m_streams, 0, sizeof(m_streams) );
	memset( m_textures, 0, sizeof(m_textures) );
	memset( m_samplers, 0, sizeof(m_samplers) );
	

	//============================================================================
	// param block for GLM context create
	GLMDisplayParams	glmParams;	
	ConvertPresentationParamsToGLMDisplayParams( &params->m_presentationParameters, &glmParams );

	glmParams.m_mtgl						=	true;	// forget this idea -> (params->m_behaviorFlags & D3DCREATE_MULTITHREADED) != 0;
	// the call above fills in a bunch of things, but doesn't know about anything outside of the presentation params.
	// those tend to be the things that do not change after create, so we do those here in Create.

	glmParams.m_focusWindow					=	params->m_focusWindow;	

		#if 0	//FIXME-HACK
			// map the D3D "adapter" to a renderer/display pair
			// (that GPU will have to stay set as-is for any subsequent mode changes)
		
			int glmRendererIndex = -1;
			int glmDisplayIndex = -1;
		
			GLMRendererInfoFields		glmRendererInfo;
			GLMDisplayInfoFields		glmDisplayInfo;
		
			// the D3D "Adapter" number feeds the fake adapter index
			bool adaptResult = GLMgr::aGLMgr()->GetDisplayDB()->GetFakeAdapterInfo( params->m_adapter, &glmRendererIndex, &glmDisplayIndex, &glmRendererInfo, &glmDisplayInfo );
			Assert(!adaptResult);

			glmParams.m_rendererIndex				=	glmRendererIndex;
			glmParams.m_displayIndex				=	glmDisplayIndex;
				// glmParams.m_modeIndex  hmmmmm, client doesn't give us a mode number, just a resolution..
		#endif
	
	m_ctx = GLMgr::aGLMgr()->NewContext( &glmParams );
	if (!m_ctx)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Create (error out)"));
		return (HRESULT) -1;
	}
	
	// make an FBO to draw into and activate it.
	m_drawableFBO = m_ctx->NewFBO();					

	m_ctx->SetDrawingFBO( m_drawableFBO );
	
	// bind it to context.  will receive attachments shortly.
	m_ctx->BindFBOToCtx( m_drawableFBO, GL_READ_FRAMEBUFFER_EXT );
	m_ctx->BindFBOToCtx( m_drawableFBO, GL_DRAW_FRAMEBUFFER_EXT );

	// we create two IDirect3DSurface9's.  These will be known as the internal render target 0 and the depthstencil.
	
	GLMPRINTF(("-X- IDirect3DDevice9::Create making color render target..."));
	// color surface
	result = this->CreateRenderTarget( 
		m_params.m_presentationParameters.BackBufferWidth,			// width
		m_params.m_presentationParameters.BackBufferHeight,			// height
		m_params.m_presentationParameters.BackBufferFormat,			// format
		m_params.m_presentationParameters.MultiSampleType,			// MSAA depth
		m_params.m_presentationParameters.MultiSampleQuality,		// MSAA quality
		true,														// lockable
		&m_defaultColorSurface,										// ppSurface
		NULL														// shared handle
		);

	if (result != S_OK)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Create (error out)"));
		return result;
	}
		// do not do an AddRef..

	GLMPRINTF(("-X- IDirect3DDevice9::Create making color render target complete -> %08x", m_defaultColorSurface ));

	GLMPRINTF(("-X- IDirect3DDevice9::Create setting color render target..."));
	result = this->SetRenderTarget(0, m_defaultColorSurface);
	if (result != S_OK)
	{
		GLMPRINTF(("< IDirect3DDevice9::Create (error out)"));
		return result;
	}
	GLMPRINTF(("-X- IDirect3DDevice9::Create setting color render target complete."));

	Assert (m_params.m_presentationParameters.EnableAutoDepthStencil);

	GLMPRINTF(("-X- IDirect3DDevice9::Create making depth-stencil..."));
    result = CreateDepthStencilSurface(
		m_params.m_presentationParameters.BackBufferWidth,			// width
		m_params.m_presentationParameters.BackBufferHeight,			// height
		m_params.m_presentationParameters.AutoDepthStencilFormat,	// format
		m_params.m_presentationParameters.MultiSampleType,			// MSAA depth
		m_params.m_presentationParameters.MultiSampleQuality,		// MSAA quality
		TRUE,														// enable z-buffer discard ????
		&m_defaultDepthStencilSurface,								// ppSurface
		NULL														// shared handle
		);
	if (result != S_OK)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Create (error out)"));
		return result;
	}
		// do not do an AddRef here..

	GLMPRINTF(("-X- IDirect3DDevice9::Create making depth-stencil complete -> %08x", m_defaultDepthStencilSurface));
	GLMPRINTF(("-X- Direct3DDevice9::Create setting depth-stencil render target..."));
	result = this->SetDepthStencilSurface(m_defaultDepthStencilSurface);
	if (result != S_OK)
	{
		GLMDebugger();
		GLMPRINTF(("<-X- IDirect3DDevice9::Create (error out)"));
		return result;
	}
	GLMPRINTF(("-X- IDirect3DDevice9::Create setting depth-stencil render target complete."));

	bool ready = m_drawableFBO->IsReady();
	if (!ready)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Create (error out)"));
		return (HRESULT)-1;
	}

	// this next part really needs to be inside GLMContext.. or replaced with D3D style viewport setup calls.
	m_ctx->GenDebugFontTex();
	
	// blast the gl state mirror...
	memset( &this->gl, 0, sizeof( this->gl ) );

	GLScissorEnable_t		defScissorEnable		= { true };
	GLScissorBox_t			defScissorBox			= { 0,0, m_params.m_presentationParameters.BackBufferWidth,m_params.m_presentationParameters.BackBufferHeight };
	GLViewportBox_t			defViewportBox			= { 0,0, m_params.m_presentationParameters.BackBufferWidth,m_params.m_presentationParameters.BackBufferHeight };
	GLViewportDepthRange_t	defViewportDepthRange	= { 0.1, 1000.0 };
	GLCullFaceEnable_t		defCullFaceEnable		= { true };
	GLCullFrontFace_t		defCullFrontFace		= { GL_CCW };
	
	gl.m_ScissorEnable		=	defScissorEnable;
	gl.m_ScissorBox			=	defScissorBox;
	gl.m_ViewportBox		=	defViewportBox;
	gl.m_ViewportDepthRange	=	defViewportDepthRange;
	gl.m_CullFaceEnable		=	defCullFaceEnable;
	gl.m_CullFrontFace		=	defCullFrontFace;

	gl.m_stateDirtyMask =	(1<<kGLScissorEnable) | (1<<kGLScissorBox) | (1<<kGLViewportBox) | (1<<kGLViewportDepthRange) | (1<<kGLCullFaceEnable) | (1<<kGLCullFrontFace);
	
	GLMPRINTF(("<-X- IDirect3DDevice9::Create complete"));

	// so GetClientRect can return sane answers
	uint width, height;		
	g_extCocoaMgr->RenderedSize( m_params.m_presentationParameters.BackBufferWidth, m_params.m_presentationParameters.BackBufferHeight, true );	// true = set
	
	return result;
}

IDirect3DDevice9::~IDirect3DDevice9()
{
	GLMPRINTF(( "-D- IDirect3DDevice9::~IDirect3DDevice9 signpost" ));	// want to know when this is called, if ever
}

#pragma mark ----- Basics - (IDirect3DDevice9)


HRESULT IDirect3DDevice9::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	HRESULT result = S_OK;

	// define the task of reset as:
	// provide new drawable RT's for the backbuffer (color and depthstencil).
	// fix up viewport / scissor..
	// then pass the new presentation parameters through to GLM.
	// (it will in turn notify appframework on the next present... which may be very soon, as mode changes are usually spotted inside Present() ).
	
	// so some of this looks a lot like Create - we're just a subset of what it does.
	// with a little work you could refactor this to be common code.

	//------------------------------------------------------------------------------- absorb new presentation params..
	
	m_params.m_presentationParameters = *pPresentationParameters;
	
	//------------------------------------------------------------------------------- color buffer..
	// release old color surface if it's there..
	if (m_defaultColorSurface)
	{
		ULONG refc = m_defaultColorSurface->Release( 0, "IDirect3DDevice9::Reset public release color surface" );
		Assert( !refc );
		m_defaultColorSurface = NULL;
	}
	
	GLMPRINTF(("-X- IDirect3DDevice9::Reset making new color render target..."));
	// color surface
	result = this->CreateRenderTarget( 
		m_params.m_presentationParameters.BackBufferWidth,			// width
		m_params.m_presentationParameters.BackBufferHeight,			// height
		m_params.m_presentationParameters.BackBufferFormat,			// format
		m_params.m_presentationParameters.MultiSampleType,			// MSAA depth
		m_params.m_presentationParameters.MultiSampleQuality,		// MSAA quality
		true,														// lockable
		&m_defaultColorSurface,										// ppSurface
		NULL														// shared handle
		);

	if (result != S_OK)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Reset (error out)"));
		return result;
	}
		// do not do an AddRef here..

	GLMPRINTF(("-X- IDirect3DDevice9::Reset making color render target complete -> %08x", m_defaultColorSurface ));

	GLMPRINTF(("-X- IDirect3DDevice9::Reset setting color render target..."));
	result = this->SetRenderTarget(0, m_defaultColorSurface);
	if (result != S_OK)
	{
		GLMPRINTF(("< IDirect3DDevice9::Reset (error out)"));
		return result;
	}
	GLMPRINTF(("-X- IDirect3DDevice9::Reset setting color render target complete."));


	//-------------------------------------------------------------------------------depth stencil buffer
	// release old depthstencil surface if it's there..
	if (m_defaultDepthStencilSurface)
	{
		ULONG refc = m_defaultDepthStencilSurface->Release( 0, "IDirect3DDevice9::Reset public release depthstencil surface" );
		Assert(!refc);
		m_defaultDepthStencilSurface = NULL;
	}
	
	Assert (m_params.m_presentationParameters.EnableAutoDepthStencil);

	GLMPRINTF(("-X- IDirect3DDevice9::Reset making depth-stencil..."));
    result = CreateDepthStencilSurface(
		m_params.m_presentationParameters.BackBufferWidth,			// width
		m_params.m_presentationParameters.BackBufferHeight,			// height
		m_params.m_presentationParameters.AutoDepthStencilFormat,	// format
		m_params.m_presentationParameters.MultiSampleType,			// MSAA depth
		m_params.m_presentationParameters.MultiSampleQuality,		// MSAA quality
		TRUE,														// enable z-buffer discard ????
		&m_defaultDepthStencilSurface,								// ppSurface
		NULL														// shared handle
		);
	if (result != S_OK)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Reset (error out)"));
		return result;
	}
		// do not do an AddRef here..

	GLMPRINTF(("-X- IDirect3DDevice9::Reset making depth-stencil complete -> %08x", m_defaultDepthStencilSurface));

	GLMPRINTF(("-X- IDirect3DDevice9::Reset setting depth-stencil render target..."));
	result = this->SetDepthStencilSurface(m_defaultDepthStencilSurface);
	if (result != S_OK)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Reset (error out)"));
		return result;
	}
	GLMPRINTF(("-X- IDirect3DDevice9::Reset setting depth-stencil render target complete."));

	bool ready = m_drawableFBO->IsReady();
	if (!ready)
	{
		GLMPRINTF(("<-X- IDirect3DDevice9::Reset (error out)"));
		return D3DERR_DEVICELOST;
	}

	//-------------------------------------------------------------------------------zap viewport and scissor to new backbuffer size

	GLScissorEnable_t		defScissorEnable		= { true };
	GLScissorBox_t			defScissorBox			= { 0,0, m_params.m_presentationParameters.BackBufferWidth,m_params.m_presentationParameters.BackBufferHeight };
	GLViewportBox_t			defViewportBox			= { 0,0, m_params.m_presentationParameters.BackBufferWidth,m_params.m_presentationParameters.BackBufferHeight };
	GLViewportDepthRange_t	defViewportDepthRange	= { 0.1, 1000.0 };
	GLCullFaceEnable_t		defCullFaceEnable		= { true };
	GLCullFrontFace_t		defCullFrontFace		= { GL_CCW };
	
	gl.m_ScissorEnable		=	defScissorEnable;
	gl.m_ScissorBox			=	defScissorBox;
	gl.m_ViewportBox		=	defViewportBox;
	gl.m_ViewportDepthRange	=	defViewportDepthRange;
	gl.m_CullFaceEnable		=	defCullFaceEnable;
	gl.m_CullFrontFace		=	defCullFrontFace;

	gl.m_stateDirtyMask		|=	(1<<kGLScissorEnable) | (1<<kGLScissorBox) | (1<<kGLViewportBox) | (1<<kGLViewportDepthRange) | (1<<kGLCullFaceEnable) | (1<<kGLCullFrontFace);

	//-------------------------------------------------------------------------------finally, propagate new display params to GLM context
	GLMDisplayParams	glmParams;	
	ConvertPresentationParamsToGLMDisplayParams( pPresentationParameters, &glmParams );

	// steal back previously sent focus window...
	glmParams.m_focusWindow = m_ctx->m_displayParams.m_focusWindow;
	Assert( glmParams.m_focusWindow != NULL );

	// so GetClientRect can return sane answers
	uint width, height;		
	g_extCocoaMgr->RenderedSize( pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight, true );	// true = set
		
	m_ctx->SetDisplayParams( &glmParams );
	
	return S_OK;
}

HRESULT IDirect3DDevice9::SetViewport(CONST D3DVIEWPORT9* pViewport)
{
	GLMPRINTF(("-X- IDirect3DDevice9::SetViewport : minZ %f, maxZ %f",pViewport->MinZ, pViewport->MaxZ ));
	
	gl.m_ViewportBox.x		= pViewport->X;
	gl.m_ViewportBox.width	= pViewport->Width;

	gl.m_ViewportBox.y		= pViewport->Y;
	gl.m_ViewportBox.height	= pViewport->Height;	

	gl.m_stateDirtyMask |= (1<<kGLViewportBox);

	gl.m_ViewportDepthRange.near	=	pViewport->MinZ;
	gl.m_ViewportDepthRange.far		=	pViewport->MaxZ;

	gl.m_stateDirtyMask |= (1<<kGLViewportDepthRange);

	return S_OK;
}

HRESULT IDirect3DDevice9::BeginScene()
{
	m_ctx->BeginFrame();

	return S_OK;
}

HRESULT IDirect3DDevice9::EndScene()
{
	m_ctx->EndFrame();
	return S_OK;
}


// stolen from glmgrbasics.cpp

enum ECarbonModKeyIndex
{
  EcmdKeyBit                     = 8,    /* command key down?*/
  EshiftKeyBit                   = 9,    /* shift key down?*/
  EalphaLockBit                  = 10,   /* alpha lock down?*/
  EoptionKeyBit                  = 11,   /* option key down?*/
  EcontrolKeyBit                 = 12    /* control key down?*/
};

enum ECarbonModKeyMask
{
  EcmdKey                        = 1 << EcmdKeyBit,
  EshiftKey                      = 1 << EshiftKeyBit,
  EalphaLock                     = 1 << EalphaLockBit,
  EoptionKey                     = 1 << EoptionKeyBit,
  EcontrolKey                    = 1 << EcontrolKeyBit
};

ConVar gl_blitmode( "gl_blitmode", "1" );
ConVar dxa_nullrefresh_capslock( "dxa_nullrefresh_capslock", "0" );

HRESULT IDirect3DDevice9::Present(CONST RECT* pSourceRect,CONST RECT* pDestRect,VD3DHWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion)
{
	// before attempting to present a tex, make sure it's been resolved if it was MSAA.
		// if we push that responsibility down to m_ctx->Present, it could probably do it without an extra copy.
		// i.e. anticipate the blit from the resolvedtex to GL_BACK, and just do that instead.

	if (dxa_nullrefresh_capslock.GetInt())
	{
		if (GetCurrentKeyModifiers() & EalphaLock )
		{
			return S_OK;
		}
	}

	// no explicit ResolveTex call first - that got pushed down into GLMContext::Present
	m_ctx->Present( m_defaultColorSurface->m_tex );

	return S_OK;
}

	// Sanity-chedk the results

/*
	// TODO, something like the following:

	// safe because of early exit on NULL above
	m_rtSurfaces[ RenderTargetIndex ]->AddRef();

	*ppRenderTarget = m_rtSurfaces[ RenderTargetIndex ];
*/

#pragma mark ----- Textures - (IDirect3DDevice9)
#pragma mark ( create functions for each texture are now adjacent to the rest of the methods for each texture class)


HRESULT IDirect3DDevice9::SetTexture(DWORD Stage,IDirect3DBaseTexture9* pTexture)
{
	// texture sets are sent through immediately to GLM
	// but we also latch the value so we know which TMU's are active.
	// whuch can help FlushSamplers do less work.
	
	// place new tex
	m_textures[Stage] = pTexture;
	if (!pTexture)
	{
		m_ctx->SetSamplerTex( Stage, NULL );
	}
	else
	{
		m_ctx->SetSamplerTex( Stage, pTexture->m_tex );
	}
	
	return S_OK;
}

HRESULT IDirect3DDevice9::GetTexture(DWORD Stage,IDirect3DBaseTexture9** ppTexture)
{
	// if implemented, should it increase the ref count ??
	GLMDebugger();
	return S_OK;
}


#pragma mark ----- RT's and Surfaces - (IDirect3DDevice9)

HRESULT IDirect3DDevice9::CreateRenderTarget(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable,IDirect3DSurface9** ppSurface,VD3DHANDLE* pSharedHandle, char *pDebugLabel)
{
	HRESULT result = S_OK;
	
	IDirect3DSurface9 *surf = new IDirect3DSurface9;
	surf->m_restype = D3DRTYPE_SURFACE;

	surf->m_device		= this;				// always set device on creations!
	
	GLMTexLayoutKey rtkey;
	memset( &rtkey, 0, sizeof(rtkey) );
	
	rtkey.m_texGLTarget	=	GL_TEXTURE_2D;
	rtkey.m_xSize		=	Width;
	rtkey.m_ySize		=	Height;
	rtkey.m_zSize		=	1;

	rtkey.m_texFormat	=	Format;
	rtkey.m_texFlags	=	kGLMTexRenderable;

	rtkey.m_texFlags |= kGLMTexSRGB;	// all render target tex are SRGB mode
	if (m_ctx->Caps().m_cantAttachSRGB)
	{
		// this config can't support SRGB render targets.  quietly turn off the sRGB bit.
		rtkey.m_texFlags &= ~kGLMTexSRGB;
	}
	
	if ( (MultiSample !=0) && (!m_ctx->Caps().m_nvG7x) )
	{
		rtkey.m_texFlags |= kGLMTexMultisampled;
		rtkey.m_texSamples = MultiSample;
		// FIXME no support for "MS quality" yet
	}

	surf->m_tex			= m_ctx->NewTex( &rtkey, pDebugLabel );
	surf->m_face		= 0;
	surf->m_mip			= 0;
	
	//desc
	surf->m_desc.Format				=	Format;
    surf->m_desc.Type				=	D3DRTYPE_SURFACE;
    surf->m_desc.Usage				=	0;					//FIXME ???????????
    surf->m_desc.Pool				=	D3DPOOL_DEFAULT;	//FIXME ???????????
	surf->m_desc.MultiSampleType	=	MultiSample;
    surf->m_desc.MultiSampleQuality	=	MultisampleQuality;
    surf->m_desc.Width				=	Width;
    surf->m_desc.Height				=	Height;

	*ppSurface = (result==S_OK) ? surf : NULL;

	#if IUNKNOWN_ALLOC_SPEW
		char scratch[1024];
		sprintf(scratch,"RT %s", surf->m_tex->m_layout->m_layoutSummary );
		surf->SetMark( true, scratch ); 
	#endif
	
	
	return result;
}

HRESULT IDirect3DDevice9::SetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9* pRenderTarget)
{
	HRESULT result = S_OK;

	GLMPRINTF(("-F- SetRenderTarget index=%d, surface=%8x (tex=%8x %s)",
		RenderTargetIndex,
		pRenderTarget,
		pRenderTarget ? pRenderTarget->m_tex : NULL,
		pRenderTarget ? pRenderTarget->m_tex->m_layout->m_layoutSummary : ""
	));

	// note that it is OK to pass NULL for pRenderTarget, it implies that you would like to detach any color buffer from that target index
	
	// behaviors...
	// if new surf is same as old surf, no change in refcount, in fact, it's early exit
	IDirect3DSurface9 *oldTarget = m_rtSurfaces[RenderTargetIndex];

	if (pRenderTarget == oldTarget)
	{
		GLMPRINTF(("-F-             --> no change",RenderTargetIndex));
		return S_OK;
	}
	
	// we now know that the new surf is not the same as the old surf.
	// you can't assume either one is non NULL here though.
	
	if (m_rtSurfaces[RenderTargetIndex])
	{
		m_rtSurfaces[RenderTargetIndex]->Release( 1, "-A  SetRenderTarget private release" );	// note this is the private refcount being lowered
	}

	if (pRenderTarget)
	{
		pRenderTarget->AddRef( 1, "+A  SetRenderTarget private addref"  );						// again, private refcount being raised
	}
	
	m_rtSurfaces[RenderTargetIndex] = pRenderTarget;	// emplace it whether NULL or not

	if (!pRenderTarget)
	{		
		GLMPRINTF(("-F-             --> Setting NULL render target on index=%d ",RenderTargetIndex));
	}
	else
	{
		GLMPRINTF(("-F-             --> attaching index=%d on drawing FBO (%8x)",RenderTargetIndex, m_drawableFBO));
		// attach color to FBO
		GLMFBOTexAttachParams	rtParams;
		memset( &rtParams, 0, sizeof(rtParams) );
		
		rtParams.m_tex		= pRenderTarget->m_tex;
		rtParams.m_face		= pRenderTarget->m_face;
		rtParams.m_mip		= pRenderTarget->m_mip;
		rtParams.m_zslice	= 0;	// FIXME if you ever want to be able to render to slices of a 3D tex..
		
		m_drawableFBO->TexAttach( &rtParams, (EGLMFBOAttachment)(kAttColor0 + RenderTargetIndex) );
	}

	return result;
}

HRESULT IDirect3DDevice9::GetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9** ppRenderTarget)
{
	if ( !m_rtSurfaces[ RenderTargetIndex ] )
		return D3DERR_NOTFOUND;
	
	if ( ( RenderTargetIndex < 0 ) || ( RenderTargetIndex > 4 ) || !ppRenderTarget )
		return D3DERR_INVALIDCALL;

	// safe because of early exit on NULL above
	m_rtSurfaces[ RenderTargetIndex ]->AddRef(0, "+B GetRenderTarget public addref");	// per http://msdn.microsoft.com/en-us/library/bb174404(VS.85).aspx
	
	*ppRenderTarget = m_rtSurfaces[ RenderTargetIndex ];
	
	return S_OK;
}

HRESULT IDirect3DDevice9::CreateOffscreenPlainSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DPOOL Pool,IDirect3DSurface9** ppSurface,VD3DHANDLE* pSharedHandle)
{
	// set surf->m_restype to D3DRTYPE_SURFACE...

	// this is almost identical to CreateRenderTarget..
	
	HRESULT result = S_OK;
	
	IDirect3DSurface9 *surf = new IDirect3DSurface9;
	surf->m_restype = D3DRTYPE_SURFACE;

	surf->m_device		= this;				// always set device on creations!

	GLMTexLayoutKey rtkey;
	memset( &rtkey, 0, sizeof(rtkey) );
	
	rtkey.m_texGLTarget	=	GL_TEXTURE_2D;
	rtkey.m_xSize		=	Width;
	rtkey.m_ySize		=	Height;
	rtkey.m_zSize		=	1;

	rtkey.m_texFormat	=	Format;
	rtkey.m_texFlags	=	kGLMTexRenderable;

	surf->m_tex			=	m_ctx->NewTex( &rtkey, "offscreen plain surface" );
	surf->m_face		=	0;
	surf->m_mip			=	0;
	
	//desc
	surf->m_desc.Format				=	Format;
    surf->m_desc.Type				=	D3DRTYPE_SURFACE;
    surf->m_desc.Usage				=	0;
    surf->m_desc.Pool				=	D3DPOOL_DEFAULT;
	surf->m_desc.MultiSampleType	=	D3DMULTISAMPLE_NONE;
    surf->m_desc.MultiSampleQuality	=	0;
    surf->m_desc.Width				=	Width;
    surf->m_desc.Height				=	Height;

	*ppSurface = (result==S_OK) ? surf : NULL;
	
	return result;
}

HRESULT IDirect3DDevice9::CreateDepthStencilSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Discard,IDirect3DSurface9** ppSurface,VD3DHANDLE* pSharedHandle)
{
	HRESULT result = S_OK;
	
	IDirect3DSurface9 *surf = new IDirect3DSurface9;
	surf->m_restype = D3DRTYPE_SURFACE;

	surf->m_device		= this;				// always set device on creations!
	
	GLMTexLayoutKey depthkey;
	memset( &depthkey, 0, sizeof(depthkey) );

	depthkey.m_texGLTarget	=	GL_TEXTURE_2D;
	depthkey.m_xSize		=	Width;
	depthkey.m_ySize		=	Height;
	depthkey.m_zSize		=	1;

	depthkey.m_texFormat	=	Format;
	depthkey.m_texFlags		=	kGLMTexRenderable | kGLMTexIsDepth | kGLMTexIsStencil;

	if ( (MultiSample !=0) && (!m_ctx->Caps().m_nvG7x) )
	{
		depthkey.m_texFlags |= kGLMTexMultisampled;
		depthkey.m_texSamples = MultiSample;
		// FIXME no support for "MS quality" yet
	}

	surf->m_tex				= m_ctx->NewTex( &depthkey, "depth-stencil surface" );
	surf->m_face			= 0;
	surf->m_mip				= 0;

	//desc

	surf->m_desc.Format				=	Format;
    surf->m_desc.Type				=	D3DRTYPE_SURFACE;
    surf->m_desc.Usage				=	0;					//FIXME ???????????
    surf->m_desc.Pool				=	D3DPOOL_DEFAULT;	//FIXME ???????????
	surf->m_desc.MultiSampleType	=	MultiSample;
    surf->m_desc.MultiSampleQuality	=	MultisampleQuality;
    surf->m_desc.Width				=	Width;
    surf->m_desc.Height				=	Height;

	*ppSurface = (result==S_OK) ? surf : NULL;
	
	return result;
}

HRESULT IDirect3DDevice9::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil)
{
	HRESULT	result = S_OK;

	GLMPRINTF(("-F- SetDepthStencilSurface, surface=%8x (tex=%8x %s)",
		pNewZStencil,
		pNewZStencil ? pNewZStencil->m_tex : NULL,
		pNewZStencil ? pNewZStencil->m_tex->m_layout->m_layoutSummary : ""
	));

	if (pNewZStencil)
	{
		pNewZStencil->AddRef(1, "+A  SetDepthStencilSurface private addref");
	}

	if (m_dsSurface)
	{
		m_dsSurface->Release(1, "-A  SetDepthStencilSurface private release");
		// do not do a Release here..
	}

	if (m_dsSurface != pNewZStencil)
	{
		GLMPRINTF(("-F-             --> attaching depthstencil %8x on drawing FBO (%8x)", pNewZStencil, m_drawableFBO));

		m_dsSurface = pNewZStencil;
		
		// aka FBO attach

		GLMFBOTexAttachParams	depthParams;
		memset( &depthParams, 0, sizeof(depthParams) );
		
			// NULL is OK - it means unbind the depth buffer
		depthParams.m_tex	= (pNewZStencil) ? pNewZStencil->m_tex : NULL;
		depthParams.m_face	= 0;
		depthParams.m_mip	= 0;
		depthParams.m_zslice= 0;

		// brute force baby
		// clear old attachments in all D/S categories
		m_drawableFBO->TexDetach( kAttStencil );
		m_drawableFBO->TexDetach( kAttDepth );
		m_drawableFBO->TexDetach( kAttDepthStencil );
		
		// select dest for new attachment
		
		if (depthParams.m_tex!=NULL)
		{
			EGLMFBOAttachment destAttach = (depthParams.m_tex->m_layout->m_format->m_glDataFormat != 34041) ? kAttDepth : kAttDepthStencil;
			m_drawableFBO->TexAttach( &depthParams, destAttach );	// attach(NULL) is allowed to mean "detach".
		}
	}
	else
	{
		GLMPRINTF(("-F-             --> no change"));
	}

	return result;
}

HRESULT IDirect3DDevice9::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface)
{
	if ( !ppZStencilSurface )
	{
		return D3DERR_INVALIDCALL;		
	}
	
	if ( !m_dsSurface )
	{
		*ppZStencilSurface = NULL;
		return D3DERR_NOTFOUND;
	}

	m_dsSurface->AddRef(0, "+B  GetDepthStencilSurface public addref");			// per http://msdn.microsoft.com/en-us/library/bb174384(VS.85).aspx

	*ppZStencilSurface = m_dsSurface;

	return S_OK;
}

HRESULT IDirect3DDevice9::GetRenderTargetData(IDirect3DSurface9* pRenderTarget,IDirect3DSurface9* pDestSurface)
{
	// is it just a blit ?

	this->StretchRect( pRenderTarget, NULL, pDestSurface, NULL, D3DTEXF_NONE ); // is this good enough ???

	return S_OK;
}

HRESULT IDirect3DDevice9::GetFrontBufferData(UINT iSwapChain,IDirect3DSurface9* pDestSurface)
{
	Debugger();
	return S_OK;
}

HRESULT IDirect3DDevice9::StretchRect(IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestSurface,CONST RECT* pDestRect,D3DTEXTUREFILTERTYPE Filter)
{
	// find relevant slices in GLM tex

	CGLMTex	*srcTex = pSourceSurface->m_tex;
	int srcSliceIndex = srcTex->CalcSliceIndex( pSourceSurface->m_face, pSourceSurface->m_mip );
	GLMTexLayoutSlice *srcSlice = &srcTex->m_layout->m_slices[ srcSliceIndex ];

	CGLMTex	*dstTex = pDestSurface->m_tex;
	int dstSliceIndex = dstTex->CalcSliceIndex( pDestSurface->m_face, pDestSurface->m_mip );
	GLMTexLayoutSlice *dstSlice = &dstTex->m_layout->m_slices[ dstSliceIndex ];

	if ( dstTex->m_rboName != 0 )
	{
		Assert(!"No path yet for blitting into an MSAA tex");
		return S_OK;
	}

	bool useFastBlit = (gl_blitmode.GetInt() != 0);
	
	if ( !useFastBlit && (srcTex->m_rboName !=0))		// old way, we do a resolve to scratch tex first (necessitating two step blit)
	{
		m_ctx->ResolveTex( srcTex, true );
	}

	// set up source/dest rect in GLM form
	GLMRect	srcRect, dstRect;

	// d3d nomenclature:
	// Y=0 is the visual top and also aligned with V=0.

	srcRect.xmin	=	pSourceRect		?	pSourceRect->left	:	0;
	srcRect.xmax	=	pSourceRect		?	pSourceRect->right	:	srcSlice->m_xSize;
	srcRect.ymin	=	pSourceRect		?	pSourceRect->top	:	0;
	srcRect.ymax	=	pSourceRect		?	pSourceRect->bottom	:	srcSlice->m_ySize;

	dstRect.xmin	=	pDestRect		?	pDestRect->left		:	0;
	dstRect.xmax	=	pDestRect		?	pDestRect->right	:	dstSlice->m_xSize;
	dstRect.ymin	=	pDestRect		?	pDestRect->top		:	0;
	dstRect.ymax	=	pDestRect		?	pDestRect->bottom	:	dstSlice->m_ySize;

	GLenum filterGL = 0;
	switch(Filter)
	{
		case	D3DTEXF_NONE:
		case	D3DTEXF_POINT:
			filterGL = GL_NEAREST;
		break;
		
		case	D3DTEXF_LINEAR:
			filterGL = GL_LINEAR;
		break;
		
		default:			// D3DTEXF_ANISOTROPIC
			Assert(!"Impl aniso stretch");
		break;
	}
	
	if (useFastBlit)
	{
		m_ctx->Blit2(		srcTex, &srcRect, pSourceSurface->m_face, pSourceSurface->m_mip, 
							dstTex, &dstRect, pDestSurface->m_face, pDestSurface->m_mip, 
							filterGL
					);
	}
	else
	{
		m_ctx->BlitTex(		srcTex, &srcRect, pSourceSurface->m_face, pSourceSurface->m_mip, 
							dstTex, &dstRect, pDestSurface->m_face, pDestSurface->m_mip, 
							filterGL
					);
	}
						
	return S_OK;
}


// This totally sucks, but this information can't be gleaned any
// other way when translating from D3D to GL at this level
//
// This returns a mask, since multiple GLSL "varyings" can be tagged with centroid
static uint32 CentroidMaskFromName( bool bPixelShader, const char *pName )
{
	if ( !pName )
		return 0;
	
	if ( bPixelShader )
	{
		if ( V_stristr( pName, "lightmappedpaint_ps" ) || V_stristr( pName, "lightmappedgeneric_ps" ) || V_stristr( pName, "flashlight_ps" ) )
		{
			return (0x01 << 1); // iterator 1
		}
		else if ( V_stristr( pName, "shadow_ps" ) )
		{
			return (0x01 << 1) | (0x01 << 2) | (0x01 << 3); // iterators 1, 2 and 3
		}
		else if ( V_stristr( pName, "water_ps" ) )
		{
			return (0x01 << 5) | (0x01 << 6) | (0x01 << 7); // iterators 5, 6 and 7
		}
	}
	else // vertex shader
	{
		// Vertex shaders also
		if ( V_stristr( pName, "lightmappedgeneric_vs" ) || V_stristr( pName, "flashlight_vs" ) )
		{
			return (0x01 << 1); // iterator 1
		}
		else if ( V_stristr( pName, "shadow_vs" ) )
		{
			return (0x01 << 1) | (0x01 << 2) | (0x01 << 3); // iterators 1, 2 and 3
		}
		else if ( V_stristr( pName, "water_vs" ) )
		{
			return (0x01 << 5) | (0x01 << 6) | (0x01 << 7); // iterators 5, 6 and 7
		}
	}
	
	// This shader doesn't have any centroid iterators
	return 0;
}


// This totally sucks, but this information can't be gleaned any
// other way when translating from D3D to GL at this level
static int ShadowDepthSamplerFromName( const char *pName )
{
	if ( !pName )
		return -1;	
	
	if ( V_stristr( pName, "water_ps" ) )
	{
		return 7;
	}

	else if ( V_stristr( pName, "phong_ps" ) )
	{
		return 4;
	}
	else if ( V_stristr( pName, "vertexlit_and_unlit_generic_bump_ps" ) )
	{
		return 8;
	}
	else if ( V_stristr( pName, "vertexlit_and_unlit_generic_ps" ) )
	{
		return 8;
	}
	else if ( V_stristr( pName, "eye_refract_ps" ) )
	{
		return 6;
	}
	else if ( V_stristr( pName, "eyes_flashlight_ps" ) )
	{
		return 4;
	}
	else if ( V_stristr( pName, "worldtwotextureblend_ps" ) ) 
	{
		return 7;
	}
	else if ( V_stristr( pName, "teeth_flashlight_ps" ) ) 
	{
		return 2;
	}
	else if ( V_stristr( pName, "flashlight_ps" ) ) // substring of above, make sure this comes last!!
	{
		return 7;
	}
	
	// This shader doesn't have a shadow depth map sampler
	return -1;
}


#pragma mark ----- Pixel Shaders - (IDirect3DDevice9)

HRESULT IDirect3DDevice9::CreatePixelShader(CONST DWORD* pFunction,IDirect3DPixelShader9** ppShader, const char *pShaderName, char *pDebugLabel)
{
	HRESULT	result = D3DERR_INVALIDCALL;
	*ppShader = NULL;
	
	int nShadowDepthSampler = ShadowDepthSamplerFromName( pShaderName );
	uint32 nCentroidMask = CentroidMaskFromName( true, pShaderName );
	
	if ( g_bUseControlFlow || !m_ctx->Caps().m_hasDualShaders )
	{
		// either having control-flow 'on' or -glmdualshaders 'off' disqualifies ARB assembler mode
		g_useASMTranslations = false;
	}

	if ( ! (g_useASMTranslations || g_useGLSLTranslations) )
	{
		Assert(!"Must set at least one translation option..");
		*ppShader = NULL;
		return -1;
	}
	else
	{
		int numTranslations = (g_useASMTranslations!=0) + (g_useGLSLTranslations!=0);
		
		bool bVertexShader = false;

		// we can do one or two translated forms. they go together in a single buffer with some markers to allow GLM to break it up.
		// this also lets us mirror each set of translations to disk with a single file making it easier to view and edit side by side.
		
		int maxTranslationSize = 50000;	// size of any one translation
		
		CUtlBuffer transbuf( 3000, numTranslations * maxTranslationSize, CUtlBuffer::TEXT_BUFFER );
		CUtlBuffer tempbuf( 3000, maxTranslationSize, CUtlBuffer::TEXT_BUFFER );

		if ( g_useASMTranslations )
		{
			// no extra tag needed for ARBfp, just use the !!ARBfp marker

			tempbuf.EnsureCapacity( maxTranslationSize );
			g_D3DToOpenGLTranslatorASM.TranslateShader( (uint32 *) pFunction, &tempbuf, &bVertexShader, D3DToGL_OptionUseEnvParams,	nShadowDepthSampler, 0, pDebugLabel );

			// grow to encompass...
			transbuf.PutString ( (char*)tempbuf.Base() );
			transbuf.PutString( "\n\n" );	// whitespace
		}

		if ( g_useGLSLTranslations )
		{
			transbuf.PutString( "//GLSLfp\n" );		// this is required so GLM can crack the text apart

			// note the GLSL translator wants its own buffer
			tempbuf.EnsureCapacity( maxTranslationSize );
			
			uint glslPixelShaderOptions = D3DToGL_OptionGLSL | D3DToGL_OptionUseEnvParams;
			

			// Fake SRGB mode - needed on R500, probably indefinitely.
			// Do this stuff if caps show m_needsFakeSRGB=true and the sRGBWrite state is true
			// (but not if it's engine_post which is special)

			if (!m_ctx->Caps().m_hasGammaWrites)
			{
				if ( pShaderName )
				{
					if ( !V_stristr( pShaderName, "engine_post" ) )
					{
						glslPixelShaderOptions |= D3DToGL_OptionSRGBWriteSuffix;
					}
				}
			}

			if (m_ctx->Caps().m_hasBindableUniforms)
			{
				glslPixelShaderOptions |= D3DToGL_OptionUseBindableUniforms;
			}
			g_D3DToOpenGLTranslatorGLSL.TranslateShader( (uint32 *) pFunction, &tempbuf, &bVertexShader, glslPixelShaderOptions, nShadowDepthSampler, nCentroidMask, pDebugLabel );
			
			transbuf.PutString( (char*)tempbuf.Base() );
			transbuf.PutString( "\n\n" );	// whitespace
		}
		
		if ( bVertexShader )
		{
			// don't cross the streams
			Assert(!"Can't accept vertex shader in CreatePixelShader");
			result = D3DERR_INVALIDCALL;
		}
		else
		{
			IDirect3DPixelShader9 *newprog = new IDirect3DPixelShader9;
					
			newprog->m_pixProgram = m_ctx->NewProgram( kGLMFragmentProgram, (char *)transbuf.Base() ) ;

			newprog->m_device = this;
			
			//------ find the frag program metadata and extract it..

			// find the highwater mark
			char *highWaterPrefix = "//HIGHWATER-";		// try to arrange this so it can work with pure GLSL if needed
			char *highWaterStr = strstr( (char *)transbuf.Base(), highWaterPrefix );
			if (highWaterStr)
			{
				char *highWaterActualData = highWaterStr + strlen( highWaterPrefix );
				
				int value = -1;
				sscanf( highWaterActualData, "%d", &value );

				newprog->m_pixHighWater = value;
				newprog->m_pixProgram->m_descs[kGLMGLSL].m_highWater = value;
			}
			else
			{
				Assert(!"couldn't find sampler map in pixel shader");
			}
			
			// find the sampler map
			char *samplerMaskPrefix = "//SAMPLERMASK-";		// try to arrange this so it can work with pure GLSL if needed
			
			char *samplerMaskStr = strstr( (char *)transbuf.Base(), samplerMaskPrefix );
			if (samplerMaskStr)
			{
				char *samplerMaskActualData = samplerMaskStr + strlen( samplerMaskPrefix );
				
				int value = -1;
				sscanf( samplerMaskActualData, "%04x", &value );

				newprog->m_pixSamplerMask = value;
				newprog->m_pixProgram->m_samplerMask = value;	// helps GLM maintain a better linked pair cache even when SRGB sampler state changes
			}
			else
			{
				Assert(!"couldn't find sampler map in pixel shader");
			}
			
			*ppShader = newprog;
			
			result = S_OK;
		}
	}


	return result;
}

IDirect3DPixelShader9::~IDirect3DPixelShader9()
{
	GLMPRINTF(( ">-A- ~IDirect3DPixelShader9" ));

	if (m_device)
	{
		m_device->ReleasedPixelShader( this );

		if (m_pixProgram)
		{
			m_pixProgram->m_ctx->DelProgram( m_pixProgram );
			m_pixProgram = NULL;
		}
		m_device = NULL;
	}
	
	GLMPRINTF(( "<-A- ~IDirect3DPixelShader9" ));
}


HRESULT IDirect3DDevice9::SetPixelShader(IDirect3DPixelShader9* pShader)
{
	if (pShader)
	{
		m_ctx->SetDrawingProgram( kGLMFragmentProgram, pShader->m_pixProgram );
	}
	else
	{
		m_ctx->SetDrawingProgram( kGLMFragmentProgram, NULL );
	}
	m_pixelShader = pShader;

	return S_OK;
}

HRESULT IDirect3DDevice9::SetPixelShaderConstantF(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount)
{
	m_ctx->SetProgramParametersF( kGLMFragmentProgram, StartRegister, (float *)pConstantData, Vector4fCount );

	return S_OK;
}

HRESULT IDirect3DDevice9::SetPixelShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
{
	GLMPRINTF(("-X- Ignoring IDirect3DDevice9::SetPixelShaderConstantB call, count was %d", BoolCount ));
// actually no way to do this yet.
//	m_ctx->SetProgramParametersB( kGLMFragmentProgram, StartRegister, pConstantData, BoolCount );

	return S_OK;
}

HRESULT IDirect3DDevice9::SetPixelShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
{
	GLMPRINTF(("-X- Ignoring IDirect3DDevice9::SetPixelShaderConstantI call, count was %d", Vector4iCount ));
//	m_ctx->SetProgramParametersI( kGLMFragmentProgram, StartRegister, pConstantData, Vector4iCount );
	return S_OK;
}


#pragma mark ----- Vertex Shaders - (IDirect3DDevice9)

HRESULT IDirect3DDevice9::CreateVertexShader(CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader, const char *pShaderName, char *pDebugLabel)
{
	HRESULT	result = D3DERR_INVALIDCALL;
	*ppShader = NULL;

	uint32 nCentroidMask = CentroidMaskFromName( false, pShaderName );

	if ( g_bUseControlFlow || !m_ctx->Caps().m_hasDualShaders )
	{
		// either having control-flow 'on' or -glmdualshaders 'off' disqualifies ARB assembler mode
		g_useASMTranslations = false;
	}
	
	if ( ! (g_useASMTranslations || g_useGLSLTranslations) )
	{
		Assert(!"Must set at least one translation option..");
		*ppShader = NULL;
		return -1;
	}
	else
	{
		int numTranslations = (g_useASMTranslations!=0) + (g_useGLSLTranslations!=0);
		
		bool bVertexShader = false;

		// we can do one or two translated forms. they go together in a single buffer with some markers to allow GLM to break it up.
		// this also lets us mirror each set of translations to disk with a single file making it easier to view and edit side by side.
		
		int maxTranslationSize = 500000;	// size of any one translation

		CUtlBuffer transbuf( 1000, numTranslations * maxTranslationSize, CUtlBuffer::TEXT_BUFFER );
		CUtlBuffer tempbuf( 1000, maxTranslationSize, CUtlBuffer::TEXT_BUFFER );

		if ( g_useASMTranslations )
		{
			// no extra tag needed for ARBvp, just use the !!ARBvp marker

			tempbuf.EnsureCapacity( maxTranslationSize );

			uint asmTransOptions = D3DToGL_OptionUseEnvParams | D3DToGL_OptionDoFixupZ | D3DToGL_OptionDoFixupY;
			
			// D3DToGL_OptionDoUserClipPlanes not being set for asm yet, it generates NV VP 2..
			g_D3DToOpenGLTranslatorASM.TranslateShader( (uint32 *) pFunction, &tempbuf, &bVertexShader,  asmTransOptions, -1, 0, pDebugLabel );

			// grow to encompass...
			transbuf.PutString ( (char*)tempbuf.Base() );
			transbuf.PutString( "\n\n" );	// whitespace
		}

		if ( g_useGLSLTranslations )
		{
			transbuf.PutString( "//GLSLvp\n" );		// this is required so GLM can crack the text apart

			// note the GLSL translator wants its own buffer
			tempbuf.EnsureCapacity( maxTranslationSize );
			
			uint glslVertexShaderOptions = D3DToGL_OptionGLSL | D3DToGL_OptionUseEnvParams | D3DToGL_OptionDoFixupZ | D3DToGL_OptionDoFixupY;

			if ( g_bUseControlFlow )
			{
				glslVertexShaderOptions |= D3DToGL_OptionAllowStaticControlFlow; 
			}

			if ( m_ctx->Caps().m_hasNativeClipVertexMode )
			{
				// note the matched trickery over in IDirect3DDevice9::FlushStates - 
				// if on a chipset that does no have native gl_ClipVertex support, then
				// omit writes to gl_ClipVertex, and instead submit plane equations that have been altered,
				// and clipping will take place in GL space using gl_Position instead of gl_ClipVertex.
				
				// note that this is very much a hack to mate up with ATI R5xx hardware constraints, and with older
				// drivers even for later ATI parts like r6xx/r7xx.   And it doesn't work on NV parts, so you really
				// do have to choose the right way to go.
				
				glslVertexShaderOptions |= D3DToGL_OptionDoUserClipPlanes; 
			}
			
			if (m_ctx->Caps().m_hasBindableUniforms)
			{
				glslVertexShaderOptions |= D3DToGL_OptionUseBindableUniforms;
			}
			
			g_D3DToOpenGLTranslatorGLSL.TranslateShader( (uint32 *) pFunction, &tempbuf, &bVertexShader, glslVertexShaderOptions, -1, nCentroidMask, pDebugLabel );
			
			transbuf.PutString( (char*)tempbuf.Base() );
			transbuf.PutString( "\n\n" );	// whitespace
		}
		
		if ( !bVertexShader )
		{
			// don't cross the streams
			Assert(!"Can't accept pixel shader in CreateVertexShader");
			result = D3DERR_INVALIDCALL;
		}
		else
		{
			IDirect3DVertexShader9 *newprog = new IDirect3DVertexShader9;

			newprog->m_device = this;
					
			newprog->m_vtxProgram = m_ctx->NewProgram( kGLMVertexProgram, (char *)transbuf.Base() ) ;

			// find the highwater mark..

			char *highWaterPrefix = "//HIGHWATER-";		// try to arrange this so it can work with pure GLSL if needed
			char *highWaterStr = strstr( (char *)transbuf.Base(), highWaterPrefix );
			if (highWaterStr)
			{
				char *highWaterActualData = highWaterStr + strlen( highWaterPrefix );
				
				int value = -1;
				sscanf( highWaterActualData, "%d", &value );

				newprog->m_vtxHighWater = value;
				newprog->m_vtxProgram->m_descs[kGLMGLSL].m_highWater = value;
			}
			else
			{
				Assert(!"couldn't find highwater mark in vertex shader");
			}
			
			// find the attrib map..
			char *attribMapPrefix = "//ATTRIBMAP-";		// try to arrange this so it can work with pure GLSL if needed
			char *attribMapStr = strstr( (char *)transbuf.Base(), attribMapPrefix );
			if (attribMapStr)
			{
				char *attribMapActualData = attribMapStr + strlen( attribMapPrefix );
				for( int i=0; i<16; i++)
				{
					int value = -1;
					char *dataItem = attribMapActualData + (i*3);
					sscanf( dataItem, "%02x", &value );
					if (value >=0)
					{
						// make sure it's not a terminator
						if (value == 0xBB)
						{
							Debugger();
						}
					}
					else
					{
						// probably an 'xx'... check
						if ( (dataItem[0] != 'x') || (dataItem[1] != 'x') )
						{
							Debugger();	// bad news
						}
						else
						{
							value = 0xBB;		// not likely to see one of these... "fog with usage index 11"
						}
					}
					newprog->m_vtxAttribMap[i] = value;
				}
			}
			else
			{
				Debugger();	// that's bad...
			}
			
			*ppShader = newprog;
			
			result = S_OK;
		}
	}

	return result;
}

IDirect3DVertexShader9::~IDirect3DVertexShader9()
{
	GLMPRINTF(( ">-A- ~IDirect3DVertexShader9" ));

	if (m_device)
	{
		m_device->ReleasedVertexShader( this );

		if (m_vtxProgram)
		{
			m_vtxProgram->m_ctx->DelProgram( m_vtxProgram );
			m_vtxProgram = NULL;
		}
		m_device = NULL;
	}
	else
	{
	}

	
	GLMPRINTF(( "<-A- ~IDirect3DVertexShader9" ));
}

HRESULT IDirect3DDevice9::SetVertexShader(IDirect3DVertexShader9* pShader)
{
	if (pShader)
	{
		m_ctx->SetDrawingProgram( kGLMVertexProgram, pShader->m_vtxProgram );
	}
	else
	{
		m_ctx->SetDrawingProgram( kGLMVertexProgram, NULL );
	}
	m_vertexShader = pShader;

	return S_OK;
}

HRESULT IDirect3DDevice9::SetVertexShaderConstantF(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount)	// groups of 4 floats!
{
	m_ctx->SetProgramParametersF( kGLMVertexProgram, StartRegister, (float *)pConstantData, Vector4fCount );
	return S_OK;
}

HRESULT IDirect3DDevice9::SetVertexShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)		// individual bool count!
{
	m_ctx->SetProgramParametersB( kGLMVertexProgram, StartRegister, (int *)pConstantData, BoolCount );
	return S_OK;
}

HRESULT IDirect3DDevice9::SetVertexShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)		// groups of 4 ints!
{
	m_ctx->SetProgramParametersI( kGLMVertexProgram, StartRegister, (int *)pConstantData, Vector4iCount );
	return S_OK;
}


#pragma mark ----- Shader Pairs - (IDirect3DDevice9)

// callers need to ifdef POSIX this, because this method does not exist on real DX9
HRESULT IDirect3DDevice9::LinkShaderPair( IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps )
{
	// these are really GLSL "shaders" not "programs" but the old reference to "program" persists due to the assembler heritage
	if (vs->m_vtxProgram && ps->m_pixProgram)
	{
		m_ctx->LinkShaderPair( vs->m_vtxProgram, ps->m_pixProgram );
	}
	return S_OK;
}

// callers need to ifdef POSIX this, because this method does not exist on real DX9
// 
HRESULT IDirect3DDevice9::QueryShaderPair( int index, GLMShaderPairInfo *infoOut )
{
	// these are really GLSL "shaders" not "programs" ...

	m_ctx->QueryShaderPair( index, infoOut );
	
	return S_OK;
}


#pragma mark ----- Vertex Buffers and Vertex Declarations - (IDirect3DDevice9)

HRESULT IDirect3DDevice9::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements,IDirect3DVertexDeclaration9** ppDecl)
{
	*ppDecl = NULL;
	
	// the goal here is to arrive at something which lets us quickly generate GLMVertexSetups.

	// the information we don't have, that must be inferred from the decls, is:
	// -> how many unique streams (buffers) are used - pure curiosity
	// -> what the stride and offset is for each decl.  Size you can figure out on the spot, stride requires surveying all the components in each stream first.
	//	so init an array of per-stream offsets to 0.
	//	each one is a cursor that gets bumped by decls.
	uint	streamOffsets[ D3D_MAX_STREAMS ];
	uint	streamCount = 0;
	
	uint	attribMap[16];
	uint	attribMapIndex = 0;
	memset( attribMap, 0xFF, sizeof( attribMap ) );
	
	memset( streamOffsets, 0, sizeof( streamOffsets ) );

	IDirect3DVertexDeclaration9 *decl9 = new IDirect3DVertexDeclaration9;
	
	decl9->m_elemCount = 0;
	
	for (const D3DVERTEXELEMENT9 *src = pVertexElements; (src->Stream != 0xFF); src++)
	{
		// element
		D3DVERTEXELEMENT9_GL *elem = &decl9->m_elements[ decl9->m_elemCount++ ];

		// copy the D3D decl wholesale.
		elem->m_dxdecl = *src;
		
		// latch current offset in this stream.
		elem->m_gldecl.m_offset = streamOffsets[ elem->m_dxdecl.Stream ];
		
		// figure out size of this attr and move the cursor
		// if cursor was on zero, bump the active stream count
		
		if (!streamOffsets[ elem->m_dxdecl.Stream ])
			streamCount++;
		
		int bytes = 0;
		switch( elem->m_dxdecl.Type )
		{
			case D3DDECLTYPE_FLOAT1:	elem->m_gldecl.m_datasize = 1; elem->m_gldecl.m_datatype = GL_FLOAT; elem->m_gldecl.m_normalized=0; bytes = 4; break;
			case D3DDECLTYPE_FLOAT2:	elem->m_gldecl.m_datasize = 2; elem->m_gldecl.m_datatype = GL_FLOAT; elem->m_gldecl.m_normalized=0; bytes = 8; break;

			//case D3DVSDT_FLOAT3:
			case D3DDECLTYPE_FLOAT3:	elem->m_gldecl.m_datasize = 3; elem->m_gldecl.m_datatype = GL_FLOAT; elem->m_gldecl.m_normalized=0; bytes = 12; break;
			
			//case D3DVSDT_FLOAT4:
			case D3DDECLTYPE_FLOAT4:	elem->m_gldecl.m_datasize = 4; elem->m_gldecl.m_datatype = GL_FLOAT; elem->m_gldecl.m_normalized=0; bytes = 16; break;
			
			// case D3DVSDT_UBYTE4:		
			case D3DDECLTYPE_D3DCOLOR:
			case D3DDECLTYPE_UBYTE4:
				
				// Force this path since we're on 10.6.2 and can't rely on EXT_vertex_array_bgra
				if ( 1 )
				{
					// pass 4 UB's but we know this is out of order compared to D3DCOLOR data
					elem->m_gldecl.m_datasize = 4; elem->m_gldecl.m_datatype = GL_UNSIGNED_BYTE;
				}
				else
				{
					// pass a GL BGRA color courtesy of http://www.opengl.org/registry/specs/ARB/vertex_array_bgra.txt
					elem->m_gldecl.m_datasize = GL_BGRA; elem->m_gldecl.m_datatype = GL_UNSIGNED_BYTE;
				}

				elem->m_gldecl.m_normalized = (elem->m_dxdecl.Type == D3DDECLTYPE_D3DCOLOR);
				
				bytes = 4;
			break;
			
			case D3DDECLTYPE_SHORT2:
				// pass 2 US's but we know this is out of order compared to D3DCOLOR data
				elem->m_gldecl.m_datasize = 2; elem->m_gldecl.m_datatype = GL_UNSIGNED_SHORT;

				elem->m_gldecl.m_normalized = 0;
				
				bytes = 4;
			break;
			
			default:	Debugger(); return D3DERR_INVALIDCALL; break;

			/*
				typedef enum _D3DDECLTYPE
				{
					D3DDECLTYPE_FLOAT1    =  0,  // 1D float expanded to (value, 0., 0., 1.)
					D3DDECLTYPE_FLOAT2    =  1,  // 2D float expanded to (value, value, 0., 1.)
					D3DDECLTYPE_FLOAT3    =  2,  // 3D float expanded to (value, value, value, 1.)
					D3DDECLTYPE_FLOAT4    =  3,  // 4D float
					D3DDECLTYPE_D3DCOLOR  =  4,  // 4D packed unsigned bytes mapped to 0. to 1. range
												 // Input is in D3DCOLOR format (ARGB) expanded to (R, G, B, A)
					D3DDECLTYPE_UBYTE4    =  5,  // 4D unsigned byte
					D3DDECLTYPE_SHORT2    =  6,  // 2D signed short expanded to (value, value, 0., 1.)
					D3DDECLTYPE_SHORT4    =  7,  // 4D signed short

				// The following types are valid only with vertex shaders >= 2.0


					D3DDECLTYPE_UBYTE4N   =  8,  // Each of 4 bytes is normalized by dividing to 255.0
					D3DDECLTYPE_SHORT2N   =  9,  // 2D signed short normalized (v[0]/32767.0,v[1]/32767.0,0,1)
					D3DDECLTYPE_SHORT4N   = 10,  // 4D signed short normalized (v[0]/32767.0,v[1]/32767.0,v[2]/32767.0,v[3]/32767.0)
					D3DDECLTYPE_USHORT2N  = 11,  // 2D unsigned short normalized (v[0]/65535.0,v[1]/65535.0,0,1)
					D3DDECLTYPE_USHORT4N  = 12,  // 4D unsigned short normalized (v[0]/65535.0,v[1]/65535.0,v[2]/65535.0,v[3]/65535.0)
					D3DDECLTYPE_UDEC3     = 13,  // 3D unsigned 10 10 10 format expanded to (value, value, value, 1)
					D3DDECLTYPE_DEC3N     = 14,  // 3D signed 10 10 10 format normalized and expanded to (v[0]/511.0, v[1]/511.0, v[2]/511.0, 1)
					D3DDECLTYPE_FLOAT16_2 = 15,  // Two 16-bit floating point values, expanded to (value, value, 0, 1)
					D3DDECLTYPE_FLOAT16_4 = 16,  // Four 16-bit floating point values
					D3DDECLTYPE_UNUSED    = 17,  // When the type field in a decl is unused.
				} D3DDECLTYPE;
			*/
		}
		
		// write the offset and move the cursor
		elem->m_gldecl.m_offset = streamOffsets[elem->m_dxdecl.Stream];
		streamOffsets[ elem->m_dxdecl.Stream ] += bytes;
		
		// cannot write m_stride yet, so zero it
		elem->m_gldecl.m_stride = 0;
		
		elem->m_gldecl.m_buffer = NULL;	// must be filled in at draw time..
		
		// elem count was already bumped.
		
		// update attrib map
		attribMap[ attribMapIndex++ ] = (elem->m_dxdecl.Usage << 4) | (elem->m_dxdecl.UsageIndex);
	}
	// the loop is done, we now know how many active streams there are, how many atribs are active in the declaration,
	// and how big each one is in terms of stride.

	// all that is left is to go back and write the strides - the stride comes from the stream offset cursors accumulated earlier.
	for( int j=0; j< decl9->m_elemCount; j++)
	{
		D3DVERTEXELEMENT9_GL	*elem = &decl9->m_elements[ j ];
		
		elem->m_gldecl.m_stride = streamOffsets[ elem->m_dxdecl.Stream ];
	}
	
	*ppDecl = decl9;
	
	return S_OK;
}

IDirect3DVertexDeclaration9::~IDirect3DVertexDeclaration9()
{
	GLMPRINTF(("-A- ~IDirect3DVertexDeclaration9 signpost"));
}

HRESULT IDirect3DDevice9::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl)
{
	// we just latch it.  At draw time we combine the current vertex decl with the current stream set and generate a vertex setup for GLM.
	// GLM can see what the differences are and act accordingly to adjust vert attrib bindings.

	m_vertDecl = pDecl;
	
	return S_OK;
}

HRESULT IDirect3DDevice9::SetFVF(DWORD FVF)
{
	Debugger();
}

HRESULT IDirect3DDevice9::GetFVF(DWORD* pFVF)
{
	Debugger();
}


#pragma mark ----- Vertex Buffers and Streams - (IDirect3DDevice9)

#pragma mark ----- Create function moved to be adjacent to other buffer methods

HRESULT IDirect3DDevice9::SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride)
{
	// perfectly legal to see a vertex buffer of NULL get passed in here.
	// so we need an array to track these.
	// OK, we are being given the stride, we don't need to calc it..
	
	GLMPRINTF(("-X- IDirect3DDevice9::SetStreamSource setting stream #%d to D3D buf %p (GL name %d); offset %d, stride %d", StreamNumber, pStreamData, (pStreamData) ? pStreamData->m_vtxBuffer->m_name: -1, OffsetInBytes, Stride));
	
	if (pStreamData)
	{
		m_streams[ StreamNumber ].m_vtxBuffer	= pStreamData;
		m_streams[ StreamNumber ].m_offset	= OffsetInBytes;
		m_streams[ StreamNumber ].m_stride	= Stride;
	}
	else
	{
		m_streams[ StreamNumber ].m_vtxBuffer	= NULL;
		m_streams[ StreamNumber ].m_offset	= 0;
		m_streams[ StreamNumber ].m_stride	= 0;
	}
}

#pragma mark ----- Index Buffers - (IDirect3DDevice9)
#pragma mark ----- Creatue function relocated to be adjacent to the rest of the index buffer methods

HRESULT IDirect3DDevice9::SetIndices(IDirect3DIndexBuffer9* pIndexData)
{
	// just latch it.
	m_indices.m_idxBuffer = pIndexData;
}


#pragma mark ----- Release Handlers - (IDirect3DDevice9)
void	IDirect3DDevice9::ReleasedTexture( IDirect3DBaseTexture9 *baseTex )
{
	// see if this texture is referenced in any of the texture units and scrub it if so.
	for( int i=0; i<16; i++)
	{
		if (m_textures[i] == baseTex)
		{
			m_textures[i] = NULL;
			m_ctx->SetSamplerTex( i, NULL );	// texture sets go straight through to GLM, no dirty bit
		}
	}
}

void	IDirect3DDevice9::ReleasedSurface( IDirect3DSurface9 *surface )
{
	for( int i=0; i<16; i++)
	{
		if (m_rtSurfaces[i]==surface)
		{
			// this was a surprise release... scrub it
			m_rtSurfaces[i] = NULL;
			GLMPRINTF(( "-A- Scrubbed surface %08x from m_rtSurfaces[%d]", surface, i ));
		}
	}
	if( m_dsSurface == surface )
	{
		m_dsSurface = NULL;
		GLMPRINTF(( "-A- Scrubbed surface %08x from m_dsSurface", surface ));
	}
	
	if ( m_defaultColorSurface == surface )
	{
		m_defaultColorSurface = NULL;
		GLMPRINTF(( "-A- Scrubbed surface %08x from m_defaultColorSurface", surface ));
	}
	
	if ( m_defaultDepthStencilSurface == surface )
	{
		m_defaultDepthStencilSurface = NULL;
		GLMPRINTF(( "-A- Scrubbed surface %08x from m_defaultDepthStencilSurface", surface ));
	}
}

void	IDirect3DDevice9::ReleasedPixelShader( IDirect3DPixelShader9 *pixelShader )
{
	if ( m_pixelShader == pixelShader )
	{
		m_pixelShader = NULL;
		GLMPRINTF(( "-A- Scrubbed pixel shader %08x from m_pixelShader", pixelShader ));
	}
}

void	IDirect3DDevice9::ReleasedVertexShader( IDirect3DVertexShader9 *vertexShader )
{
	if ( m_vertexShader == vertexShader )
	{
		m_vertexShader = NULL;
		GLMPRINTF(( "-A- Scrubbed vertex shader %08x from m_vertexShader", vertexShader ));
	}
}

void	IDirect3DDevice9::ReleasedVertexBuffer( IDirect3DVertexBuffer9 *vertexBuffer )
{
	for (int i=0; i< D3D_MAX_STREAMS; i++)
	{
		if ( m_streams[i].m_vtxBuffer == vertexBuffer )
		{
			m_streams[i].m_vtxBuffer = NULL;
			GLMPRINTF(( "-A- Scrubbed vertex buffer %08x from m_streams[%d]", vertexBuffer, i ));
		}
	}
}

void	IDirect3DDevice9::ReleasedIndexBuffer( IDirect3DIndexBuffer9 *indexBuffer )
{
	if ( m_indices.m_idxBuffer == indexBuffer )
	{
		m_indices.m_idxBuffer = NULL;
		GLMPRINTF(( "-A- Scrubbed index buffer %08x from m_indices", indexBuffer ));
	}
}


void	IDirect3DDevice9::ReleasedQuery( IDirect3DQuery9 *query )
{
	// nothing to do yet..
}




#pragma mark ----- Queries - (IDirect3DDevice9)

// note that detection of whether queries are supported is done by trying to create one.
// so for GL, be observant here of whether we have that capability or not.
// pretty much have this everywhere but i950.

HRESULT IDirect3DDevice9::CreateQuery(D3DQUERYTYPE Type,IDirect3DQuery9** ppQuery)
{
	if (m_ctx->Caps().m_hasOcclusionQuery)
	{
		IDirect3DQuery9	*newquery = new IDirect3DQuery9;
		
		newquery->m_device = this;
		
		newquery->m_type = Type;
		newquery->m_ctx = m_ctx;

		GLMQueryParams	params;
		memset( &params, 0, sizeof(params) );
		
		bool known = false;
		switch(newquery->m_type)
		{
			case	D3DQUERYTYPE_OCCLUSION:				/* D3DISSUE_BEGIN, D3DISSUE_END */
				// create an occlusion query
				params.m_type = EOcclusion;
			break;
			
			case	D3DQUERYTYPE_EVENT:					/* D3DISSUE_END */
				params.m_type = EFence;
			break;
			
			case	D3DQUERYTYPE_RESOURCEMANAGER:		/* D3DISSUE_END */
			case	D3DQUERYTYPE_TIMESTAMP:				/* D3DISSUE_END */
			case	D3DQUERYTYPE_TIMESTAMPFREQ:			/* D3DISSUE_END */
			case	D3DQUERYTYPE_INTERFACETIMINGS:		/* D3DISSUE_BEGIN, D3DISSUE_END */
			case	D3DQUERYTYPE_PIXELTIMINGS:			/* D3DISSUE_BEGIN, D3DISSUE_END */
			case	D3DQUERYTYPE_CACHEUTILIZATION:		/* D3DISSUE_BEGIN, D3DISSUE_END */
				Assert( !"Un-implemented query type" );
			break;
			
			default:
				Assert( !"Unknown query type" );
			break;
		}
		newquery->m_query = m_ctx->NewQuery( &params );
		
		*ppQuery = newquery;
		return S_OK;
	}
	else
	{
		*ppQuery = NULL;
		return -1;	// failed
	}

}

IDirect3DQuery9::~IDirect3DQuery9()
{
	GLMPRINTF((">-A- ~IDirect3DQuery9"));

	if (m_device)
	{
		m_device->ReleasedQuery( this );

		if (m_query)
		{
			GLMPRINTF((">-A- ~IDirect3DQuery9 freeing m_query"));
			
			m_query->m_ctx->DelQuery( m_query );
			m_query = NULL;

			GLMPRINTF(("<-A- ~IDirect3DQuery9 freeing m_query done"));
		}
		m_device = NULL;
	}
	
	GLMPRINTF(("<-A- ~IDirect3DQuery9"));
}

#pragma mark ----- Render States - (IDirect3DDevice9)

struct	D3D_RSINFO
{
	int					m_class;
	D3DRENDERSTATETYPE	m_state;
	DWORD				m_defval;
		// m_class runs 0-3.
		// 3 = must implement - fully general - "obey"
		// 2 = implement setup to the default value (it has a GL effect but does not change later) "obey once"
		// 1 = "fake implement" setup to the default value no GL effect, debug break if anything but default value comes through - "ignore"
		// 0 = game never ever sets this one, break if someone even tries. "complain"
};

#define	D3DRS_VALUE_LIMIT	210
bool		g_D3DRS_INFO_unpacked_ready = false;	// set to true after unpack
D3D_RSINFO	g_D3DRS_INFO_unpacked[ D3DRS_VALUE_LIMIT+1 ];

#ifdef D3D_RSI
	#error macro collision... rename this
#else
	#define D3D_RSI(nclass,nstate,ndefval)	{ nclass, nstate, ndefval }
#endif

// FP conversions to hex courtesy of http://babbage.cs.qc.cuny.edu/IEEE-754/Decimal.html
#define	CONST_DZERO		0x00000000
#define	CONST_DONE		0x3F800000
#define	CONST_D64		0x42800000
#define	DONT_KNOW_YET	0x31415926


// see http://www.toymaker.info/Games/html/render_states.html

D3D_RSINFO	g_D3DRS_INFO_packed[] = 
{
	// these do not have to be in any particular order.  they get unpacked into the empty array above for direct indexing.

	D3D_RSI(	3,	D3DRS_ZENABLE,						DONT_KNOW_YET			),	// enable Z test (or W buffering)
	D3D_RSI(	3,	D3DRS_ZWRITEENABLE,					DONT_KNOW_YET			),	// enable Z write
	D3D_RSI(	3,	D3DRS_ZFUNC,						DONT_KNOW_YET			),	// select Z func

	D3D_RSI(	3,	D3DRS_COLORWRITEENABLE,				TRUE					),	// see transitiontable.cpp "APPLY_RENDER_STATE_FUNC( D3DRS_COLORWRITEENABLE, ColorWriteEnable )"

	D3D_RSI(	3,	D3DRS_CULLMODE,						D3DCULL_CCW				),	// backface cull control

	D3D_RSI(	3,	D3DRS_ALPHABLENDENABLE,				DONT_KNOW_YET			),	// ->CTransitionTable::ApplySeparateAlphaBlend and ApplyAlphaBlend
	D3D_RSI(	3,	D3DRS_BLENDOP,						D3DBLENDOP_ADD			),
	D3D_RSI(	3,	D3DRS_SRCBLEND,						DONT_KNOW_YET			),
	D3D_RSI(	3,	D3DRS_DESTBLEND,					DONT_KNOW_YET			),

	D3D_RSI(	1,	D3DRS_SEPARATEALPHABLENDENABLE,		FALSE					),	// hit in CTransitionTable::ApplySeparateAlphaBlend
	D3D_RSI(	1,	D3DRS_SRCBLENDALPHA,				D3DBLEND_ONE			),	// going to demote these to class 1 until I figure out if they are implementable
	D3D_RSI(	1,	D3DRS_DESTBLENDALPHA,				D3DBLEND_ZERO			),
	D3D_RSI(	1,	D3DRS_BLENDOPALPHA,					D3DBLENDOP_ADD			),

	// what is the deal with alpha test... looks like it is inited to off.
	D3D_RSI(	3,	D3DRS_ALPHATESTENABLE,				0						),
	D3D_RSI(	3,	D3DRS_ALPHAREF,						0						),
	D3D_RSI(	3,	D3DRS_ALPHAFUNC,					D3DCMP_GREATEREQUAL		),

	D3D_RSI(	3,	D3DRS_STENCILENABLE,				FALSE					),
	D3D_RSI(	3,	D3DRS_STENCILFAIL,					D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_STENCILZFAIL,					D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_STENCILPASS,					D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_STENCILFUNC,					D3DCMP_ALWAYS			),
	D3D_RSI(	3,	D3DRS_STENCILREF,					0						),
	D3D_RSI(	3,	D3DRS_STENCILMASK,					0xFFFFFFFF				),
	D3D_RSI(	3,	D3DRS_STENCILWRITEMASK,				0xFFFFFFFF				),

	D3D_RSI(	3,	D3DRS_TWOSIDEDSTENCILMODE,			FALSE					),
	D3D_RSI(	3,	D3DRS_CCW_STENCILFAIL,				D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_CCW_STENCILZFAIL,				D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_CCW_STENCILPASS,				D3DSTENCILOP_KEEP		),
	D3D_RSI(	3,	D3DRS_CCW_STENCILFUNC,				D3DCMP_ALWAYS 			),

	D3D_RSI(	3,	D3DRS_FOGENABLE,					FALSE					),	// see CShaderAPIDx8::FogMode and friends - be ready to do the ARB fog linear option madness
	D3D_RSI(	3,	D3DRS_FOGCOLOR,						0						),
	D3D_RSI(	3,	D3DRS_FOGTABLEMODE,					D3DFOG_NONE				),
	D3D_RSI(	3,	D3DRS_FOGSTART,						CONST_DZERO				),
	D3D_RSI(	3,	D3DRS_FOGEND,						CONST_DONE				),
	D3D_RSI(	3,	D3DRS_FOGDENSITY,					CONST_DZERO				),
	D3D_RSI(	3,	D3DRS_RANGEFOGENABLE,				FALSE					),
	D3D_RSI(	3,	D3DRS_FOGVERTEXMODE,				D3DFOG_NONE				),	// watch out for CShaderAPIDx8::CommitPerPassFogMode....

	D3D_RSI(	3,	D3DRS_MULTISAMPLEANTIALIAS,			TRUE					),
	D3D_RSI(	3,	D3DRS_MULTISAMPLEMASK,				0xFFFFFFFF				),

	D3D_RSI(	3,	D3DRS_SCISSORTESTENABLE,			FALSE					),	// heed IDirect3DDevice9::SetScissorRect

	D3D_RSI(	3,	D3DRS_DEPTHBIAS,					CONST_DZERO				),
	D3D_RSI(	3,	D3DRS_SLOPESCALEDEPTHBIAS,			CONST_DZERO				),

	D3D_RSI(	3,	D3DRS_COLORWRITEENABLE1,			0x0000000f				),
	D3D_RSI(	3,	D3DRS_COLORWRITEENABLE2,			0x0000000f				),
	D3D_RSI(	3,	D3DRS_COLORWRITEENABLE3,			0x0000000f				),

	D3D_RSI(	3,	D3DRS_SRGBWRITEENABLE,				0						),	// heeded but ignored..

	D3D_RSI(	2,	D3DRS_CLIPPING,						TRUE					),	// um, yeah, clipping is enabled (?)
	D3D_RSI(	3,	D3DRS_CLIPPLANEENABLE,				0						),	// mask 1<<n of active user clip planes.

	D3D_RSI(	0,	D3DRS_LIGHTING,						0						),	// strange, someone turns it on then off again. move to class 0 and just ignore it (lie)?

	D3D_RSI(	3,	D3DRS_FILLMODE,						D3DFILL_SOLID			),

	D3D_RSI(	1,	D3DRS_SHADEMODE,					D3DSHADE_GOURAUD		),
	D3D_RSI(	1,	D3DRS_LASTPIXEL,					TRUE					),
	D3D_RSI(	1,	D3DRS_DITHERENABLE,					0						),	//set to false by game, no one sets it to true
	D3D_RSI(	1,	D3DRS_SPECULARENABLE,				FALSE					),
	D3D_RSI(	1,	D3DRS_TEXTUREFACTOR,				0xFFFFFFFF				),	// watch out for CShaderAPIDx8::Color3f et al.
	D3D_RSI(	1,	D3DRS_WRAP0,						0						),
	D3D_RSI(	1,	D3DRS_WRAP1,						0						),
	D3D_RSI(	1,	D3DRS_WRAP2,						0						),
	D3D_RSI(	1,	D3DRS_WRAP3,						0						),
	D3D_RSI(	1,	D3DRS_WRAP4,						0						),
	D3D_RSI(	1,	D3DRS_WRAP5,						0						),
	D3D_RSI(	1,	D3DRS_WRAP6,						0						),
	D3D_RSI(	1,	D3DRS_WRAP7,						0						),
	D3D_RSI(	1,	D3DRS_AMBIENT,						0						),	// FF lighting, no
	D3D_RSI(	1,	D3DRS_COLORVERTEX,					TRUE					),	// FF lighing again
	D3D_RSI(	1,	D3DRS_LOCALVIEWER,					TRUE					),	// FF lighting
	D3D_RSI(	1,	D3DRS_NORMALIZENORMALS,				FALSE					),	// FF mode I think.  CShaderAPIDx8::SetVertexBlendState says it might switch this on when skinning is in play
	D3D_RSI(	1,	D3DRS_DIFFUSEMATERIALSOURCE,		D3DMCS_MATERIAL			),	// hit only in CShaderAPIDx8::ResetRenderState
	D3D_RSI(	1,	D3DRS_SPECULARMATERIALSOURCE,		D3DMCS_COLOR2			),
	D3D_RSI(	1,	D3DRS_AMBIENTMATERIALSOURCE,		D3DMCS_MATERIAL			),
	D3D_RSI(	1,	D3DRS_EMISSIVEMATERIALSOURCE,		D3DMCS_MATERIAL			),
	D3D_RSI(	1,	D3DRS_VERTEXBLEND,					D3DVBF_DISABLE			),	// also being set by CShaderAPIDx8::SetVertexBlendState, so might be FF
	D3D_RSI(	1,	D3DRS_POINTSIZE,					CONST_DONE				),
	D3D_RSI(	1,	D3DRS_POINTSIZE_MIN,				CONST_DONE				),
	D3D_RSI(	1,	D3DRS_POINTSPRITEENABLE,			FALSE					),
	D3D_RSI(	1,	D3DRS_POINTSCALEENABLE,				FALSE					),
	D3D_RSI(	1,	D3DRS_POINTSCALE_A,					CONST_DONE				),
	D3D_RSI(	1,	D3DRS_POINTSCALE_B,					CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_POINTSCALE_C,					CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_PATCHEDGESTYLE,				D3DPATCHEDGE_DISCRETE	),
	D3D_RSI(	1,	D3DRS_DEBUGMONITORTOKEN,			D3DDMT_ENABLE			),
	D3D_RSI(	1,	D3DRS_POINTSIZE_MAX,				CONST_D64				),
	D3D_RSI(	1,	D3DRS_INDEXEDVERTEXBLENDENABLE,		FALSE					),
	D3D_RSI(	1,	D3DRS_TWEENFACTOR,					CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_POSITIONDEGREE,				D3DDEGREE_CUBIC			),
	D3D_RSI(	1,	D3DRS_NORMALDEGREE,					D3DDEGREE_LINEAR		),
	D3D_RSI(	1,	D3DRS_ANTIALIASEDLINEENABLE,		FALSE					),	// just ignore it
	D3D_RSI(	1,	D3DRS_MINTESSELLATIONLEVEL,			CONST_DONE				),
	D3D_RSI(	1,	D3DRS_MAXTESSELLATIONLEVEL,			CONST_DONE				),
	D3D_RSI(	1,	D3DRS_ADAPTIVETESS_X,				CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_ADAPTIVETESS_Y,				CONST_DZERO				), // Overridden as Alpha-to-coverage contrl
	D3D_RSI(	1,	D3DRS_ADAPTIVETESS_Z,				CONST_DONE				),
	D3D_RSI(	1,	D3DRS_ADAPTIVETESS_W,				CONST_DZERO				),
	D3D_RSI(	1,	D3DRS_ENABLEADAPTIVETESSELLATION,	FALSE					),
	D3D_RSI(	1,	D3DRS_BLENDFACTOR,					0xffffffff				),
	D3D_RSI(	1,	D3DRS_WRAP8,						0						),
	D3D_RSI(	1,	D3DRS_WRAP9,						0						),
	D3D_RSI(	1,	D3DRS_WRAP10,						0						),
	D3D_RSI(	1,	D3DRS_WRAP11,						0						),
	D3D_RSI(	1,	D3DRS_WRAP12,						0						),
	D3D_RSI(	1,	D3DRS_WRAP13,						0						),
	D3D_RSI(	1,	D3DRS_WRAP14,						0						),
	D3D_RSI(	1,	D3DRS_WRAP15,						0						),
	D3D_RSI(	-1,	(D3DRENDERSTATETYPE)0,				0						)	// terminator
};

void	UnpackD3DRSITable( void )
{
	memset (g_D3DRS_INFO_unpacked, 0, sizeof(g_D3DRS_INFO_unpacked) );
	
	for( D3D_RSINFO *packed = g_D3DRS_INFO_packed; packed->m_class >= 0; packed++ )
	{
		if ( (packed->m_state <0) || (packed->m_state >= D3DRS_VALUE_LIMIT) )
		{
			// bad
			Debugger();
		}
		else
		{
			// dispatch it to the unpacked array
			g_D3DRS_INFO_unpacked[ packed->m_state ] = *packed;
		}
	}
}

// convenience functions

GLenum	D3DCompareFuncToGL( DWORD function )
{
	switch ( function )
	{
		case D3DCMP_NEVER		: return GL_NEVER;				// Always fail the test.
		case D3DCMP_LESS		: return GL_LESS;				// Accept the new pixel if its value is less than the value of the current pixel.
		case D3DCMP_EQUAL		: return GL_EQUAL;				// Accept the new pixel if its value equals the value of the current pixel.
		case D3DCMP_LESSEQUAL	: return GL_LEQUAL;				// Accept the new pixel if its value is less than or equal to the value of the current pixel. **
		case D3DCMP_GREATER		: return GL_GREATER;			// Accept the new pixel if its value is greater than the value of the current pixel.
		case D3DCMP_NOTEQUAL	: return GL_NOTEQUAL;			// Accept the new pixel if its value does not equal the value of the current pixel.
		case D3DCMP_GREATEREQUAL: return GL_GEQUAL;				// Accept the new pixel if its value is greater than or equal to the value of the current pixel.
		case D3DCMP_ALWAYS		: return GL_ALWAYS;				// Always pass the test.
		default					: Debugger(); return 0xFFFFFFFF;
	}
}

static GLenum D3DStencilOpToGL( DWORD operation )
{
	switch( operation )
	{
		case D3DSTENCILOP_KEEP		: return GL_KEEP;
		case D3DSTENCILOP_ZERO		: return GL_ZERO;
		case D3DSTENCILOP_REPLACE	: return GL_REPLACE;
		case D3DSTENCILOP_INCRSAT	: return GL_INCR;
		case D3DSTENCILOP_DECRSAT	: return GL_DECR;
		case D3DSTENCILOP_INVERT	: return GL_INVERT;
		case D3DSTENCILOP_INCR		: return GL_INCR_WRAP_EXT;
		case D3DSTENCILOP_DECR		: return GL_DECR_WRAP_EXT;
		default						: Debugger(); return 0xFFFFFFFF;
	}
}

static GLenum D3DBlendFactorToGL( DWORD equation )
{
	switch (equation)
	{
		case	D3DBLEND_ZERO			: return GL_ZERO;					// Blend factor is (0, 0, 0, 0).
		case	D3DBLEND_ONE			: return GL_ONE;					// Blend factor is (1, 1, 1, 1).
		case	D3DBLEND_SRCCOLOR		: return GL_SRC_COLOR;				// Blend factor is (Rs, Gs, Bs, As).
		case	D3DBLEND_INVSRCCOLOR	: return GL_ONE_MINUS_SRC_COLOR;	// Blend factor is (1 - Rs, 1 - Gs, 1 - Bs, 1 - As).
		case	D3DBLEND_SRCALPHA		: return GL_SRC_ALPHA;				// Blend factor is (As, As, As, As).
		case	D3DBLEND_INVSRCALPHA	: return GL_ONE_MINUS_SRC_ALPHA;	// Blend factor is ( 1 - As, 1 - As, 1 - As, 1 - As).
		case	D3DBLEND_DESTALPHA		: return GL_DST_ALPHA;				// Blend factor is (Ad Ad Ad Ad).
		case	D3DBLEND_INVDESTALPHA	: return GL_ONE_MINUS_DST_ALPHA;	// Blend factor is (1 - Ad 1 - Ad 1 - Ad 1 - Ad).
		case	D3DBLEND_DESTCOLOR		: return GL_DST_COLOR;				// Blend factor is (Rd, Gd, Bd, Ad).
		case	D3DBLEND_INVDESTCOLOR	: return GL_ONE_MINUS_DST_COLOR;	// Blend factor is (1 - Rd, 1 - Gd, 1 - Bd, 1 - Ad).
		case	D3DBLEND_SRCALPHASAT	: return GL_SRC_ALPHA_SATURATE;		// Blend factor is (f, f, f, 1); where f = min(As, 1 - Ad).

		/*
			// these are weird.... break if we hit them
			case	D3DBLEND_BOTHSRCALPHA	: Assert(0); return GL_ZERO;		// Obsolete. Starting with DirectX 6, you can achieve the same effect by setting the source and destination blend factors to D3DBLEND_SRCALPHA and D3DBLEND_INVSRCALPHA in separate calls.
			case	D3DBLEND_BOTHINVSRCALPHA: Assert(0); return GL_ZERO;		// Source blend factor is (1 - As, 1 - As, 1 - As, 1 - As), and destination blend factor is (As, As, As, As); the destination blend selection is overridden. This blend mode is supported only for the D3DRS_SRCBLEND render state.
			case	D3DBLEND_BLENDFACTOR	: Assert(0); return GL_ZERO;		// Constant color blending factor used by the frame-buffer blender. This blend mode is supported only if D3DPBLENDCAPS_BLENDFACTOR is set in the SrcBlendCaps or DestBlendCaps members of D3DCAPS9.
		
		dxabstract.h has not heard of these, so let them hit the debugger if they come through
			case	D3DBLEND_INVBLENDFACTOR:	//Inverted constant color-blending factor used by the frame-buffer blender. This blend mode is supported only if the D3DPBLENDCAPS_BLENDFACTOR bit is set in the SrcBlendCaps or DestBlendCaps members of D3DCAPS9.
			case	D3DBLEND_SRCCOLOR2:		// Blend factor is (PSOutColor[1]r, PSOutColor[1]g, PSOutColor[1]b, not used).	This flag is available in Direct3D 9Ex only.
			case	D3DBLEND_INVSRCCOLOR2:	// Blend factor is (1 - PSOutColor[1]r, 1 - PSOutColor[1]g, 1 - PSOutColor[1]b, not used)). This flag is available in Direct3D 9Ex only.
		*/
		default:
			Debugger();
			return 0xFFFFFFFF;
		break;
	}
}

static GLenum D3DBlendOperationToGL( DWORD operation )
{
	switch (operation)
	{
		case	D3DBLENDOP_ADD			: return GL_FUNC_ADD;				// The result is the destination added to the source. Result = Source + Destination

		/* not covered by dxabstract.h..
			case	D3DBLENDOP_SUBTRACT		: return GL_FUNC_SUBTRACT;			// The result is the destination subtracted from to the source. Result = Source - Destination
			case	D3DBLENDOP_REVSUBTRACT	: return GL_FUNC_REVERSE_SUBTRACT;	// The result is the source subtracted from the destination. Result = Destination - Source
			case	D3DBLENDOP_MIN			: return GL_MIN;					// The result is the minimum of the source and destination. Result = MIN(Source, Destination)
			case	D3DBLENDOP_MAX			: return GL_MAX;					// The result is the maximum of the source and destination. Result = MAX(Source, Destination)
		*/
		
		default:
			Debugger();
			return 0xFFFFFFFF;
		break;
		
	}
}

HRESULT IDirect3DDevice9::SetRenderState( D3DRENDERSTATETYPE State, DWORD Value )
{
	char	rsSpew = 1;
	char	ignored = 0;
	
	if (!g_D3DRS_INFO_unpacked_ready)
	{
		UnpackD3DRSITable();
		g_D3DRS_INFO_unpacked_ready = true;
	}
	
	if (State >= D3DRS_VALUE_LIMIT)
	{
		Debugger();		// bad
	}
	else
	{
		D3D_RSINFO *info = &g_D3DRS_INFO_unpacked[ State ];
	
		if (info->m_state != State)
		{
			Debugger();	// bad - we never set up that state in our list
		}

		if (rsSpew)
		{
			GLMPRINTF(("-X- IDirect3DDevice9::SetRenderState: set %s(%d) to %d(0x%08x) ( class %d, defval is %d(0x%08x) )", GLMDecode( eD3D_RSTATE,State),State, Value,Value, info->m_class, info->m_defval,info->m_defval ));
		}
		
		switch( info->m_class )
		{
			case	0:		// just ignore quietly. example: D3DRS_LIGHTING
				ignored = 1;
			break;
			
			case	1:
			{
				// no GL response - and no error as long as the write value matches the default
				if (Value != info->m_defval)
				{
					static char stop_here_1 = 0;
					if (stop_here_1)
						Debugger();
				}
			}
			break;
			
			case	2:

				// provide GL response, but only support known default value
				if (Value != info->m_defval)
				{
					static char stop_here_2 = 0;
					if (stop_here_2)
						Debugger();
				}
				// fall through to mode 3
				
			case	3:

				// full GL response, support any legal value
				// note we're handling the class-2's as well.
				switch(State)
				{
					case	D3DRS_ZENABLE:				// kGLDepthTestEnable
						gl.m_DepthTestEnable.enable = Value;
						gl.m_stateDirtyMask |= (1<<kGLDepthTestEnable);
					break;
					
					case	D3DRS_ZWRITEENABLE:			// kGLDepthMask
						gl.m_DepthMask.mask = Value;
						gl.m_stateDirtyMask |= (1<<kGLDepthMask);
					break;
					
					case	D3DRS_ZFUNC:	
					{
						// kGLDepthFunc
						GLenum func = D3DCompareFuncToGL( Value );
						gl.m_DepthFunc.func = func;
						gl.m_stateDirtyMask |= (1<<kGLDepthFunc);
					}
					break;

					case	D3DRS_COLORWRITEENABLE:		// kGLColorMaskSingle
					{
						gl.m_ColorMaskSingle.r	=	((Value & D3DCOLORWRITEENABLE_RED)  != 0) ? 0xFF : 0x00;
						gl.m_ColorMaskSingle.g	=	((Value & D3DCOLORWRITEENABLE_GREEN)!= 0) ? 0xFF : 0x00;	
						gl.m_ColorMaskSingle.b	=	((Value & D3DCOLORWRITEENABLE_BLUE) != 0) ? 0xFF : 0x00;
						gl.m_ColorMaskSingle.a	=	((Value & D3DCOLORWRITEENABLE_ALPHA)!= 0) ? 0xFF : 0x00;

						gl.m_stateDirtyMask |= (1<<kGLColorMaskSingle);
					}
					break;
					
					case	D3DRS_COLORWRITEENABLE1:	// kGLColorMaskMultiple
					case	D3DRS_COLORWRITEENABLE2:	// kGLColorMaskMultiple
					case	D3DRS_COLORWRITEENABLE3:	// kGLColorMaskMultiple
						ignored = 1;
					break;

					case	D3DRS_CULLMODE:				// kGLCullFaceEnable / kGLCullFrontFace
					{
						switch(Value)
						{
							case	D3DCULL_NONE:
								gl.m_CullFaceEnable.enable = false;
								gl.m_CullFrontFace.value = GL_CCW;	//doesn't matter
								
								gl.m_stateDirtyMask |= (1<<kGLCullFaceEnable) | (1<<kGLCullFrontFace);
							break;
							
							case	D3DCULL_CW:
								gl.m_CullFaceEnable.enable = true;
								gl.m_CullFrontFace.value = GL_CW;	//origGL_CCW;
								
								gl.m_stateDirtyMask |= (1<<kGLCullFaceEnable) | (1<<kGLCullFrontFace);
							break;
							
							case	D3DCULL_CCW:
								gl.m_CullFaceEnable.enable = true;
								gl.m_CullFrontFace.value = GL_CCW;	//origGL_CW;
								
								gl.m_stateDirtyMask |= (1<<kGLCullFaceEnable) | (1<<kGLCullFrontFace);
							break;

							default:	Debugger();	break;
						}
					}
					break;


					//-------------------------------------------------------------------------------------------- alphablend stuff

					case	D3DRS_ALPHABLENDENABLE:		// kGLBlendEnable
						gl.m_BlendEnable.enable = Value;
						gl.m_stateDirtyMask |= (1<<kGLBlendEnable);
					break;
					
					case	D3DRS_BLENDOP:				// kGLBlendEquation				// D3D blend-op ==> GL blend equation
					{
						GLenum	equation = D3DBlendOperationToGL( Value );
						gl.m_BlendEquation.equation = equation;
						gl.m_stateDirtyMask |= (1<<kGLBlendEquation);
					}
					break;
					
					case	D3DRS_SRCBLEND:				// kGLBlendFactor				// D3D blend-factor ==> GL blend factor
					case	D3DRS_DESTBLEND:			// kGLBlendFactor
					{
						GLenum	factor = D3DBlendFactorToGL( Value );
												
						if (State==D3DRS_SRCBLEND)
						{
							gl.m_BlendFactor.srcfactor = factor;
						}
						else
						{
							gl.m_BlendFactor.dstfactor = factor;
						}
						gl.m_stateDirtyMask |= (1<<kGLBlendFactor);
					}
					break;

					case	D3DRS_SEPARATEALPHABLENDENABLE:
					case	D3DRS_BLENDOPALPHA:
					case	D3DRS_SRCBLENDALPHA:
					case	D3DRS_DESTBLENDALPHA:
						ignored = 1;
					break;

					case	D3DRS_SRGBWRITEENABLE:			// kGLBlendEnableSRGB
						gl.m_BlendEnableSRGB.enable = Value;
						gl.m_stateDirtyMask |= (1<<kGLBlendEnableSRGB);
					break;					

					//-------------------------------------------------------------------------------------------- alphatest stuff

					case	D3DRS_ALPHATESTENABLE:
						gl.m_AlphaTestEnable.enable = Value;
						gl.m_stateDirtyMask |= (1<<kGLAlphaTestEnable);
					break;
						
					case	D3DRS_ALPHAREF:
						gl.m_AlphaTestFunc.ref = Value / 255.0f;
						gl.m_stateDirtyMask |= (1<<kGLAlphaTestFunc);
					break;
					
					case	D3DRS_ALPHAFUNC:
					{
						GLenum func = D3DCompareFuncToGL( Value );;
						gl.m_AlphaTestFunc.func = func;
						gl.m_stateDirtyMask |= (1<<kGLAlphaTestFunc);
					}
					break;


					//-------------------------------------------------------------------------------------------- stencil stuff

					case	D3DRS_STENCILENABLE:		// GLStencilTestEnable_t
					{
						gl.m_StencilTestEnable.enable = Value;

						gl.m_stateDirtyMask |= (1<<kGLStencilTestEnable);
					}
					break;

					case	D3DRS_STENCILFAIL:			// GLStencilOp_t		"what do you do if stencil test fails"
					{
						GLenum stencilop = D3DStencilOpToGL( Value );
						gl.m_StencilOp.sfail = stencilop;

						gl.m_stateDirtyMask |= (1<<kGLStencilOp);
					}
					break;
					
					case	D3DRS_STENCILZFAIL:			// GLStencilOp_t		"what do you do if stencil test passes *but* depth test fails, if depth test happened"
					{
						GLenum stencilop = D3DStencilOpToGL( Value );
						gl.m_StencilOp.dpfail = stencilop;

						gl.m_stateDirtyMask |= (1<<kGLStencilOp);
					}
					break;
					
					case	D3DRS_STENCILPASS:			// GLStencilOp_t		"what do you do if stencil test and depth test both pass"
					{
						GLenum stencilop = D3DStencilOpToGL( Value );
						gl.m_StencilOp.dppass = stencilop;

						gl.m_stateDirtyMask |= (1<<kGLStencilOp);
					}
					break;
					
					case	D3DRS_STENCILFUNC:			// GLStencilFunc_t
					{
						GLenum stencilfunc = D3DCompareFuncToGL( Value );
						gl.m_StencilFunc.frontfunc = gl.m_StencilFunc.backfunc = stencilfunc;

						gl.m_stateDirtyMask |= (1<<kGLStencilFunc);
					}
					break;

					case	D3DRS_STENCILREF:			// GLStencilFunc_t
					{
						gl.m_StencilFunc.ref = Value;
						gl.m_stateDirtyMask |= (1<<kGLStencilFunc);
					}
					break;
					
					case	D3DRS_STENCILMASK:			// GLStencilFunc_t
					{
						//if (Value==255)
						//{
						//	Value = 0xFFFFFFFF;	// mask blast
						//}
						
						gl.m_StencilFunc.mask = Value;
						gl.m_stateDirtyMask |= (1<<kGLStencilFunc);
					}
					break;

					case D3DRS_STENCILWRITEMASK:		// GLStencilWriteMask_t
					{
						//if (Value==255)
						//{
						//	Value = 0xFFFFFFFF;	// mask blast
						//}
						
						gl.m_StencilWriteMask.mask = Value;
						gl.m_stateDirtyMask |= (1<<kGLStencilWriteMask);
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
						gl.m_FogEnable = (Value != 0);
						GLMPRINTF(("-D- fogenable = %d",Value ));
						//ignored = 1;
					break;

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
						gl.m_ScissorEnable.enable = Value;
						
						gl.m_stateDirtyMask |= (1<<kGLScissorEnable);
					}
					break;

					case	D3DRS_DEPTHBIAS:			// kGLDepthBias
					{
						// the value in the dword is actually a float
						float	fvalue = *(float*)&Value;
						gl.m_DepthBias.units = fvalue;

						gl.m_stateDirtyMask |= (1<<kGLDepthBias);
					}
					break;
						
					// good ref on these: http://aras-p.info/blog/2008/06/12/depth-bias-and-the-power-of-deceiving-yourself/
					case	D3DRS_SLOPESCALEDEPTHBIAS:
					{
						// the value in the dword is actually a float
						float	fvalue = *(float*)&Value;
						gl.m_DepthBias.factor = fvalue;

						gl.m_stateDirtyMask |= (1<<kGLDepthBias);
					}
					break;

					// Alpha to coverage

					case	D3DRS_CLIPPING:				// ???? is clipping ever turned off ??
						ignored = 1;
					break;
					
					case	D3DRS_CLIPPLANEENABLE:		// kGLClipPlaneEnable
						// d3d packs all the enables into one word.
						// we break that out so we don't do N glEnable calls to sync - 
						// GLM is tracking one unique enable per plane.
						for( int i=0; i<kGLMUserClipPlanes; i++)
						{
							gl.m_ClipPlaneEnable[i].enable = (Value & (1<<i)) != 0;
						}
						gl.m_stateDirtyMask |= (1<<kGLClipPlaneEnable);
					break;

					//-------------------------------------------------------------------------------------------- polygon/fill mode
					
					case D3DRS_FILLMODE:
						GLuint mode = 0;
						switch(Value)
						{
							case D3DFILL_POINT:			mode = GL_POINT; break;
							case D3DFILL_WIREFRAME:		mode = GL_LINE; break;
							case D3DFILL_SOLID:			mode = GL_FILL; break;
							
							default:
								Assert(!"unknown fill mode");
						}
						gl.m_PolygonMode.values[0] = gl.m_PolygonMode.values[1] = mode;						
						gl.m_stateDirtyMask |= (1<<kGLPolygonMode);
					break;
				}
			break;
		}
	}

	if (rsSpew && ignored)
	{
		GLMPRINTF(("-X-  (ignored)"));
	}
	
	//Debugger();
	return S_OK;
}

#pragma mark ----- Sampler States - (IDirect3DDevice9)

HRESULT IDirect3DDevice9::SetSamplerState( DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value )
{
	Assert(Sampler<16);
	
	// the D3D-to-GL translation has been moved to FlushSamplers since we want to do it at draw time
	// so this call just stuffs values in slots.
	
	D3DSamplerDesc	*samp = &m_samplers[ Sampler ];
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
		case	D3DSAMP_ADDRESSV:
		case	D3DSAMP_ADDRESSW:
			samp->m_addressModes[ Type - (int)D3DSAMP_ADDRESSU ] = (D3DTEXTUREADDRESS)Value;
		break;
		
		case	D3DSAMP_BORDERCOLOR:
			samp->m_borderColor = Value;
		break;
		
		case	D3DSAMP_MAGFILTER:	samp->m_magFilter = (D3DTEXTUREFILTERTYPE)Value; break;
		case	D3DSAMP_MINFILTER:	samp->m_minFilter = (D3DTEXTUREFILTERTYPE)Value; break;
		case	D3DSAMP_MIPFILTER:	samp->m_mipFilter = (D3DTEXTUREFILTERTYPE)Value; break;
		case	D3DSAMP_MIPMAPLODBIAS: samp->m_mipmapBias = Value; break;		// float in sheep's clothing - check this one out
		case	D3DSAMP_MAXMIPLEVEL: samp->m_maxMipLevel = Value; break;
		case	D3DSAMP_MAXANISOTROPY: samp->m_maxAniso = Value; break;
		case	D3DSAMP_SRGBTEXTURE: samp->m_srgb = Value; break;
		case	D3DSAMP_SHADOWFILTER: samp->m_shadowFilter = Value; break;
			
		default:
			Assert(!"Unknown sampler parameter");
		break;

	}
	gl.m_samplerDirtyMask |= (1<<Sampler);				// at draw time, push the dirty samplers down to GLM
	
	return S_OK;
}

HRESULT IDirect3DDevice9::FlushStates( uint mask )
{
	uint	stateHitMask = gl.m_stateDirtyMask & mask;

	// note that we will turn off all the bits that are set in the hit mask, once the work is done
	// so no need to individually clear.
	
	if ( stateHitMask & (1<<kGLAlphaTestEnable) )
		m_ctx->WriteAlphaTestEnable( &gl.m_AlphaTestEnable );

	if ( stateHitMask & (1<<kGLAlphaTestFunc) )
		m_ctx->WriteAlphaTestFunc( &gl.m_AlphaTestFunc );

	if ( stateHitMask & (1<<kGLCullFaceEnable) )
		m_ctx->WriteCullFaceEnable( &gl.m_CullFaceEnable );

	if ( stateHitMask & (1<<kGLCullFrontFace) )
		m_ctx->WriteCullFrontFace( &gl.m_CullFrontFace );

	if ( stateHitMask & (1<<kGLPolygonMode) )
		m_ctx->WritePolygonMode( &gl.m_PolygonMode );

	if ( stateHitMask & (1<<kGLDepthBias) )
		m_ctx->WriteDepthBias( &gl.m_DepthBias );

	if ( stateHitMask & (1<<kGLScissorEnable) )
		m_ctx->WriteScissorEnable( &gl.m_ScissorEnable );

	if ( stateHitMask & (1<<kGLScissorBox) )
		m_ctx->WriteScissorBox( &gl.m_ScissorBox );

	if ( stateHitMask & (1<<kGLViewportBox) )
		m_ctx->WriteViewportBox( &gl.m_ViewportBox );

	if ( stateHitMask & (1<<kGLViewportDepthRange) )
		m_ctx->WriteViewportDepthRange( &gl.m_ViewportDepthRange );

	if ( stateHitMask & (1<<kGLClipPlaneEnable) )
	{
		for( int x=0; x<kGLMUserClipPlanes; x++)
		{
			m_ctx->WriteClipPlaneEnable( &gl.m_ClipPlaneEnable[x], x );
		}
	}

	if ( stateHitMask & (1<<kGLClipPlaneEquation) )
	{
		for( int x=0; x<kGLMUserClipPlanes; x++)
		{
			GLClipPlaneEquation_t temp1;	// Antonio's way
			GLClipPlaneEquation_t temp2;	// our way

			// if we don't have native clip vertex support. then munge the plane coeffs
			// this should engage on ALL ATI PARTS < 10.6.4
			// and should continue to engage on R5xx forever.
			
			if ( !m_ctx->Caps().m_hasNativeClipVertexMode )
			{
				// hacked coeffs = { src->x, -src->y, 0.5f * src->z, src->w + (0.5f * src->z) };
				// Antonio's trick - so we can use gl_Position as the clippee, not gl_ClipVertex.

				GLClipPlaneEquation_t *equ = &gl.m_ClipPlaneEquation[x];

				///////////////// temp1
				temp1.x	=	equ->x;
				temp1.y	=	equ->y * -1.0;
				temp1.z	=	equ->z * 0.5;
				temp1.w	=	equ->w + (equ->z * 0.5);

				
				//////////////// temp2
				VMatrix mat1(	1,	0,	0,	0,
								0,	-1,	0,	0,
								0,	0,	2,	-1,
								0,	0,	0,	1
								);
				//mat1 = mat1.Transpose();
								
				VMatrix mat2;
				bool success = mat1.InverseGeneral( mat2 );
				
				if (success)
				{
					VMatrix mat3;
					mat3 = mat2.Transpose();

					VPlane origPlane( Vector( equ->x, equ->y, equ->z ), equ->w );
					VPlane newPlane;
					
					newPlane = mat3 * origPlane /* * mat3 */;
					
					VPlane finalPlane = newPlane;
					
					temp2.x = newPlane.m_Normal.x;
					temp2.y = newPlane.m_Normal.y;
					temp2.z = newPlane.m_Normal.z;
					temp2.w = newPlane.m_Dist;
				}
				else
				{
					temp2.x = 0;
					temp2.y = 0;
					temp2.z = 0;
					temp2.w = 0;
				}
			}
			else
			{
				temp1 = temp2 = gl.m_ClipPlaneEquation[x];
			}

			if (1)	//GLMKnob("caps-key",NULL)==0.0)
			{
				m_ctx->WriteClipPlaneEquation( &temp1, x );		// no caps lock = Antonio or classic
				
				/*
				if (x<1)
				{
					GLMPRINTF(( " plane %d  √vers1[ %5.2f %5.2f %5.2f %5.2f ]    vers2[ %5.2f %5.2f %5.2f %5.2f ]",
						x,
						temp1.x,temp1.y,temp1.z,temp1.w,
						temp2.x,temp2.y,temp2.z,temp2.w
					));
				}
				*/
			}
			else
			{
				m_ctx->WriteClipPlaneEquation( &temp2, x );		// caps = our way or classic

				/*
				if (x<1)
				{
					GLMPRINTF(( " plane %d   vers1[ %5.2f %5.2f %5.2f %5.2f ]    √vers2[ %5.2f %5.2f %5.2f %5.2f ]",
						x,
						temp1.x,temp1.y,temp1.z,temp1.w,
						temp2.x,temp2.y,temp2.z,temp2.w
					));
				}
				*/
			}
		}
	}

	if ( stateHitMask & (1<<kGLColorMaskSingle) )
		m_ctx->WriteColorMaskSingle( &gl.m_ColorMaskSingle );

//	if ( stateHitMask & (1<<kGLColorMaskMultiple) )
//		m_ctx->WriteColorMaskMultiple( &gl.m_ColorMaskMultiple );	// ???????????? hmmmmmmmm

	if ( stateHitMask & (1<<kGLBlendEnable) )
		m_ctx->WriteBlendEnable( &gl.m_BlendEnable );

	if ( stateHitMask & (1<<kGLBlendFactor) )
		m_ctx->WriteBlendFactor( &gl.m_BlendFactor );

	if ( stateHitMask & (1<<kGLBlendEquation) )
		m_ctx->WriteBlendEquation( &gl.m_BlendEquation );

	if ( stateHitMask & (1<<kGLBlendColor) )
		m_ctx->WriteBlendColor( &gl.m_BlendColor );

	if ( stateHitMask & (1<<kGLBlendEnableSRGB) )
		m_ctx->WriteBlendEnableSRGB( &gl.m_BlendEnableSRGB );

	if ( stateHitMask & (1<<kGLDepthTestEnable) )
		m_ctx->WriteDepthTestEnable( &gl.m_DepthTestEnable );

	if ( stateHitMask & (1<<kGLDepthFunc) )
		m_ctx->WriteDepthFunc( &gl.m_DepthFunc );

	if ( stateHitMask & (1<<kGLDepthMask) )
		m_ctx->WriteDepthMask( &gl.m_DepthMask );

	if ( stateHitMask & (1<<kGLStencilTestEnable) )
		m_ctx->WriteStencilTestEnable( &gl.m_StencilTestEnable );

	if ( stateHitMask & (1<<kGLStencilFunc) )
		m_ctx->WriteStencilFunc( &gl.m_StencilFunc );

	if ( stateHitMask & (1<<kGLStencilOp) )
	{
		m_ctx->WriteStencilOp( &gl.m_StencilOp,0 );
		m_ctx->WriteStencilOp( &gl.m_StencilOp,1 );		// ********* need to recheck this
	}

	if ( stateHitMask & (1<<kGLStencilWriteMask) )
		m_ctx->WriteStencilWriteMask( &gl.m_StencilWriteMask );

	if ( stateHitMask & (1<<kGLClearColor) )
		m_ctx->WriteClearColor( &gl.m_ClearColor );

	if ( stateHitMask & (1<<kGLClearDepth) )
		m_ctx->WriteClearDepth( &gl.m_ClearDepth );

	if ( stateHitMask & (1<<kGLClearStencil) )
		m_ctx->WriteClearStencil( &gl.m_ClearStencil );
		
	gl.m_stateDirtyMask &= (~stateHitMask);
}


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
static GLenum dxtogl_addressMode[] = 
{
	(GLenum)-1,			// no zero entry
	GL_REPEAT,			// from D3DTADDRESS_WRAP
	(GLenum)-1,			// no   D3DTADDRESS_MIRROR support
	GL_CLAMP_TO_EDGE,	// from D3DTADDRESS_CLAMP
	GL_CLAMP,			// from D3DTADDRESS_BORDER
	(GLenum)-1,			// no D3DTADDRESS_MIRRORONCE support
};

/*
	_D3DTEXTUREFILTERTYPE:
		D3DTEXF_NONE            = 0,    // filtering disabled (valid for mip filter only)
		D3DTEXF_POINT           = 1,    // nearest
		D3DTEXF_LINEAR          = 2,    // linear interpolation
		D3DTEXF_ANISOTROPIC     = 3,    // anisotropic
*/

static GLenum dxtogl_magFilter[4] =		// indexed by _D3DTEXTUREFILTERTYPE
{
	GL_NEAREST,				// D3DTEXF_NONE not applicable to mag filter but we handle it like POINT (mat_showmiplevels hits this)
	GL_NEAREST,				// D3DTEXF_POINT
	GL_LINEAR,				// D3DTEXF_LINEAR
	GL_LINEAR,				// D3DTEXF_ANISOTROPIC (aniso will be driven by setting maxAniso, not by a GL filter mode)
};

static GLenum dxtogl_minFilter[4][4] =		// indexed by _D3DTEXTUREFILTERTYPE on both axes: [row is min filter][col is mip filter]. 
{
		/*  mip filter ---------------> D3DTEXF_NONE	D3DTEXF_POINT				D3DTEXF_LINEAR				(D3DTEXF_ANISOTROPIC not applicable to mip filter)
		/* min = D3DTEXF_NONE */		{	GL_NEAREST,		GL_NEAREST_MIPMAP_NEAREST,	GL_NEAREST_MIPMAP_LINEAR,	(GLenum)-1	},		// D3DTEXF_NONE we just treat like POINT
		/* min = D3DTEXF_POINT */		{	GL_NEAREST,		GL_NEAREST_MIPMAP_NEAREST,	GL_NEAREST_MIPMAP_LINEAR,	(GLenum)-1	},
		/* min = D3DTEXF_LINEAR */		{	GL_LINEAR,		GL_LINEAR_MIPMAP_NEAREST,	GL_LINEAR_MIPMAP_LINEAR,	(GLenum)-1	},
		/* min = D3DTEXF_ANISOTROPIC */	{	GL_LINEAR,		GL_LINEAR_MIPMAP_NEAREST,	GL_LINEAR_MIPMAP_LINEAR,	(GLenum)-1	},		// no diff from prior row, set maxAniso to effect the sampling
};

HRESULT IDirect3DDevice9::FlushSamplers()
{
	// a minor optimization we could do here would be to only write sampler state for
	// TMU's that are active (i.e. consult m_textures)
	uint activeSamplerMask = m_pixelShader ? m_pixelShader->m_pixSamplerMask : 0;	// if no pixel shader bound at time of draw, act like it references no samplers
																					// (and avoid an access violation while yer at it)

	// ho, we're not clearing the dirty mask for samplers as we go... need to do that...
	uint samplerHitMask = gl.m_samplerDirtyMask;
	for( int index = 0; (index < 16) && (samplerHitMask !=0); index++)
	{
		uint nCurrentSamplerMask = 1<<index;
		
		// only push a sampler to GLM if the sampler is dirty *and* there is a live texture on that TMU
		// else the values will sit quietly in the d3d sampler side until conditions permit pushing them
		if ( ( samplerHitMask & nCurrentSamplerMask ) && ( m_textures[index] != NULL ) )
		{
			// Clear that dirty bit before you forget...
			gl.m_samplerDirtyMask &= (~nCurrentSamplerMask);
			
			// Translate from D3D sampler desc
			D3DSamplerDesc			*dxsamp = &m_samplers[ index ];
			GLMTexSamplingParams	*glsamp = &gl.m_samplers[ index ];

			// Address modes
			glsamp->m_addressModes[0] = dxtogl_addressMode[ dxsamp->m_addressModes[0] ];
			glsamp->m_addressModes[1] = dxtogl_addressMode[ dxsamp->m_addressModes[1] ];
			glsamp->m_addressModes[2] = dxtogl_addressMode[ dxsamp->m_addressModes[2] ];

			// Border color
			uint dxcolor = dxsamp->m_borderColor;
			glsamp->m_borderColor[0] =	((dxcolor >> 16) & 0xFF) / 255.0f;	//R
			glsamp->m_borderColor[1] =	((dxcolor >>  8) & 0xFF) / 255.0f;	//G
			glsamp->m_borderColor[2] =	((dxcolor      ) & 0xFF) / 255.0f;	//B
			glsamp->m_borderColor[3] =	((dxcolor >> 24) & 0xFF) / 255.0f;	//A
			
			// mag filter - pretty easy
			Assert( dxsamp->m_magFilter <= D3DTEXF_ANISOTROPIC );
			Assert( dxsamp->m_magFilter >= D3DTEXF_POINT );

			glsamp->m_magFilter = dxtogl_magFilter[ dxsamp->m_magFilter ];
			
			// min filter - more involved
			Assert( dxsamp->m_minFilter <= D3DTEXF_ANISOTROPIC );
			Assert( dxsamp->m_minFilter >= D3DTEXF_POINT );
			Assert( dxsamp->m_mipFilter <= D3DTEXF_LINEAR );
			Assert( dxsamp->m_mipFilter >= D3DTEXF_NONE );

			D3DTEXTUREFILTERTYPE mipFilterLimit = D3DTEXF_LINEAR;
			
			/*
				if (GLMKnob("caps-key",NULL) > 0.0)
				{
					if (dxsamp->m_mipFilter > D3DTEXF_NONE)
					{
						// evil hack
						glsamp->m_magFilter = GL_LINEAR_MIPMAP_NEAREST;
					}
				}

				if (GLMKnob("option-key",NULL) > 0.0)
				{
					// limit to point
					mipFilterLimit = D3DTEXF_POINT;
				}
				
				if (GLMKnob("control-key",NULL) > 0.0)
				{
					// limit to none
					mipFilterLimit = D3DTEXF_NONE;
				}
			*/

			D3DTEXTUREFILTERTYPE mipFilterChoice = MIN( dxsamp->m_mipFilter, mipFilterLimit );
			glsamp->m_minFilter = dxtogl_minFilter[ dxsamp->m_minFilter ][ mipFilterChoice ];
			
			// should we check for mip filtering being requested on unmipped textures ? does it matter ?

			// mipmap bias
			glsamp->m_mipmapBias = dxsamp->m_mipmapBias;

			// d3d "MAX MIP LEVEL" means the *largest size* MIP that will be selected. (max size)
			// this is the same as GL's "MIN LOD level" which means the GL_TEXTURE_MIN_LOD level. (min index)
			
			int texMipCount = m_textures[index]->m_tex->m_layout->m_mipCount;
			Assert( texMipCount >=1 );
			
			glsamp->m_minMipLevel = dxsamp->m_maxMipLevel;		// it says gl_minMipLevel because we're setting GL's "GL_TEXTURE_MIN_LOD" aka d3d's "maximum mip size index".
			if (glsamp->m_minMipLevel >= texMipCount)
			{
				// clamp - you can't have the GL base tex level be higher than the index of the last mip
				glsamp->m_minMipLevel = texMipCount - 1;
			}

			// d3d has no idea of a "MIN MIP LEVEL" i.e. smallest size allowed.
			// this would be expressed in GL by setting the GL_TEXTURE_MIN_LOD meaning largest index to select.
			// for now, just set it to the index of the last mip.
			glsamp->m_maxMipLevel = texMipCount-1;				// d3d has no value for constraining how small we can sample.
																// however we may need to set this more intelligently if textures are not being fully submitted.

			// On OpenGL, GL_TEXTURE_MAX_ANISOTROPY_EXT needs to be 1 if we don't want aniso
			glsamp->m_maxAniso = (dxsamp->m_minFilter != D3DTEXF_ANISOTROPIC) ? 1 : dxsamp->m_maxAniso;

			// SRGB
			glsamp->m_srgb = dxsamp->m_srgb != 0;

			// Shadow compare mode
			glsamp->m_compareMode = dxsamp->m_shadowFilter ? GL_COMPARE_R_TO_TEXTURE_ARB : GL_NONE;
			
			// write that sampler.
			m_ctx->SetSamplerParams( index, glsamp );
			samplerHitMask ^= nCurrentSamplerMask;	//turn bit off
			
			// finally, if the SRGB state of the sampler does not match the SRGB format of the underlying texture...
			// ... and the tex is not a renderable...
			// ... and it is possible to re-submit the tex in an sRGB format...

			// ******** AND THE TEX IS ACTUALLY REFERENCED BY THE ACTIVE PIXEL SHADER *******

			//  fix it.
			//	else complain ?
			
			if ( nCurrentSamplerMask & activeSamplerMask )	// don't do SRGB check on unreferenced textures.
			{
				bool bTexSRGB = (m_textures[index]->m_tex->m_layout->m_key.m_texFlags & kGLMTexSRGB) != 0;
				bool bMismatch = (bTexSRGB != glsamp->m_srgb);
				bool bSRGBCapableTex = false; // not yet known
				bool bRenderableTex = false; // not yet known.
				
				if ( bMismatch )
				{
					bSRGBCapableTex	= m_textures[index]->m_tex->m_layout->m_format->m_glIntFormatSRGB != 0;
					bRenderableTex	= (m_textures[index]->m_tex->m_layout->m_key.m_texFlags & kGLMTexRenderable) != 0;
					// we can fix it if it's not a renderable, and an sRGB enabled format variation is available.
					
					if ( bSRGBCapableTex && !bRenderableTex )
					{
						char *pTexName = m_textures[index]->m_tex->m_debugLabel;
						if (!pTexName)
							pTexName = "-";
										
						m_textures[index]->m_srgbFlipCount++;
						
						static bool bCheckedCommandline = false;
						static bool bPrintAllflips = false;
						static bool bPrintFirstflips = false;
						static bool bPrintFreqflips = false;
						static bool bPrintCrawls = false;
						static bool bPrintMaxCrawls = false;
						if ( !bCheckedCommandline )
						{
							bPrintAllflips		= CommandLine()->FindParm( "-glmspewallsrgbflips" );
							bPrintFirstflips	= CommandLine()->FindParm( "-glmspewfirstsrgbflips" );
							bPrintFreqflips		= CommandLine()->FindParm( "-glmspewfreqsrgbflips" );
							bPrintCrawls		= CommandLine()->FindParm( "-glmspewsrgbcrawls" );
							bPrintMaxCrawls		= CommandLine()->FindParm( "-glmspewsrgbmaxcrawls" );
							bCheckedCommandline = true;
						}
						
						bool bPrintIt = bPrintAllflips;
						
						if ( bPrintFirstflips )		// report on first flip
						{
							bPrintIt = bPrintIt || m_textures[index]->m_srgbFlipCount==1;
						}
						if ( bPrintFreqflips )	// report on 50th flip
						{
							bPrintIt = bPrintIt || m_textures[index]->m_srgbFlipCount==50;
						}
						
						if ( bPrintIt )
						{
							char	*pFormatStr;
							pFormatStr = "srgb change (samp=%d): tex '%-30s' %08x %s (srgb=%d, %d times)";
							
							if ( strlen( pTexName ) >= 30 )
							{
								pFormatStr = "srgb change (samp=%d): tex '%s' %08x %s (srgb=%d, %d times)";
							}
							
							printf( "\n" );
							printf( pFormatStr, index, pTexName, m_textures[index], m_textures[index]->m_tex->m_layout->m_layoutSummary, (int)glsamp->m_srgb, m_textures[index]->m_srgbFlipCount );
							
							if ( bPrintCrawls )
							{
								static char *interesting_crawl_substrs[] = { "CShader::OnDrawElements", NULL };		// add more as needed
								
								CStackCrawlParams	cp;
								memset( &cp, 0, sizeof(cp) );
								cp.m_frameLimit = 20;
								
								g_extCocoaMgr->GetStackCrawl(&cp);
								
								for( int i=0; i< cp.m_frameCount; i++)
								{
									// for each row of crawl, decide if name is interesting
									bool bHit = bPrintMaxCrawls;
									
									for( char **match = interesting_crawl_substrs; (!bHit) && (*match != NULL); match++)
									{
										if (strstr(cp.m_crawlNames[i], *match))
										{
											bHit = true;
										}
									}
									
									if ( bHit )
									{
										printf( "\n\t%s", cp.m_crawlNames[i] );
									}
								}
								printf( "\n");
							}
						}

						#if GLMDEBUG && 0
							//"toi" = texture of interest
							static char s_toi[256] = "colorcorrection";
							if (strstr( pTexName, s_toi ))
							{
								// breakpoint on this if you like
								GLMPRINTF(( "srgb change %d for %s", m_textures[index]->m_srgbFlipCount, pTexName ));
							}
						#endif
						
						// re-submit the tex unless we're stifling it
						if (!CommandLine()->FindParm( "-glmnosrgbflips" ))
						{
							m_textures[index]->m_tex->ResetSRGB( glsamp->m_srgb, false );
						}
					}
					else
					{
						//GLMPRINTF(("-Z- srgb sampling conflict: NOT fixing tex %08x [%s] (srgb req: %d) because (tex-srgb-capable=%d  tex-renderable=%d)", m_textures[index], m_textures[index]->m_tex->m_layout->m_layoutSummary, (int)glsamp->m_srgb, (int)bSRGBCapableTex, (int)bRenderableTex ));
						// we just leave the sampler state where it is, and that's life
					}
				}
			}
		}		
	}
}

HRESULT IDirect3DDevice9::FlushIndexBindings( void )
{
	// push index buffer state
	m_ctx->SetIndexBuffer( m_indices.m_idxBuffer->m_idxBuffer );
}

#if 0
HRESULT IDirect3DDevice9::FlushVertexBindings( void )
{
	// push vertex buffer state for the current vertex decl
	
	GLMVertexSetup	setup;
	IDirect3DVertexDeclaration9 *vxdecl = m_vertDecl;

	memset( &setup, 0, sizeof( setup ) );

	// see if the elems in the vertex decl match the attrib map of the shader we're about to draw with.
	// can we do this in a simple style that handles both matched and unmatched orderings?
	// just pick up each elem from the decl.
	// visit the same slot in the shader attrib map.
	// if the usage/usageindex matches, you're good.
	// if not, hunt through the shader attrib map and find it.
	// if you can't find it, then the shader is not consuming that attribute - odd but not fatal ?
	// the serious one is shader trying to consume an attrib that isn't being sourced.
	// we can check for that though with a little more work (copy the shader attrib map and mark the attribs as each one gets satisfied)
	
	unsigned char	vshAttribMap[ 16 ];
	uint			activeAttribCount = 0;
	for( int i=0; i<16; i++)
	{
		vshAttribMap[i] = m_vertexShader->m_vtxAttribMap[i];
		if (vshAttribMap[i] != 0xBB)
		{
			activeAttribCount++;			// this counting could be done at shader creation time, or changed to a mask
		}
	}
	
	for( int elemIndex=0; elemIndex<vxdecl->m_elemCount; elemIndex++)
	{
		D3DVERTEXELEMENT9_GL *srcelem = &vxdecl->m_elements[elemIndex];
		
		int matchIndex = elemIndex;		// initial guess - will iterate if this does not match
		int tries = 0;					// >16 means done
		bool matched = false;
		
		do
		{
			if ( ((vshAttribMap[matchIndex] >>4) == srcelem->m_dxdecl.Usage) && ((vshAttribMap[matchIndex] & 0x0F) == srcelem->m_dxdecl.UsageIndex) )
			{
				// hit
				int attribIndex = matchIndex;
				int streamIndex = srcelem->m_dxdecl.Stream;
			
				GLMVertexAttributeDesc *dstAttr = &setup.m_attrs[ matchIndex ];

				// copy whole thing
				*dstAttr = srcelem->m_gldecl;
				
				// then fix buffer, stride, offset
				dstAttr->m_buffer = m_streams[ streamIndex ].m_vtxBuffer->m_vtxBuffer;
				dstAttr->m_stride = m_streams[ streamIndex ].m_stride;
				dstAttr->m_offset += m_streams[ streamIndex ].m_offset; 

				// set mask
				setup.m_attrMask |= (1<<attribIndex);

				vshAttribMap[matchIndex] = 0xBB;	// can't match it again...
				activeAttribCount--;
				matched = true;		// confirm we found it
				tries = 999;		// end the loop
			}
			else
			{
				// miss.
				// just skip ahead one slot and wrap.  Increment the try count. Top of loop can try to match on it.
				// if we run out of tries, it just means the vert decl is sourcing an attrib that the VS is not reading.

				matchIndex = (matchIndex+1) & 15;
				tries++;
			}
		} while (tries<=16);

		if ( !matched )
		{
			// this one is somewhat innocuous so we just do the AssertOnce
			if ( !CommandLine()->FindParm("-hushasserts") )
			{
				AssertOnce( !"Vertex shader not consuming attribs that are sourced by decl");
			}
		}
	}

	if (activeAttribCount >0)
	{
		// this one is more serious
		if ( !CommandLine()->FindParm("-hushasserts") )
		{
			Assert( !"Vertex shader consuming attribs not sourced by decl");
		}
	}
	
	// pass the whole shebang to GLM
	m_ctx->SetVertexAttributes( &setup );
}
#endif


HRESULT IDirect3DDevice9::FlushVertexBindings( uint baseVertexIndex )
{
	// push vertex buffer state for the current vertex decl
	// in this variant we just walk the attrib map in the VS and do a pull for each one.
	// if we can't find a match in the vertex decl, we may fall back to the secret 'dummy' VBO that GLM maintains
	
	GLMVertexSetup	setup;
	memset( &setup, 0, sizeof( setup ) );

	IDirect3DVertexDeclaration9 *vxdecl = m_vertDecl;
	unsigned char *vshAttribMap = m_vertexShader->m_vtxAttribMap;
	
	// this loop could be tightened if we knew the number of live entries in the shader attrib map.
	// which of course would be easy to do in the create shader function or even in the translator.
	
	GLMVertexAttributeDesc *dstAttr = setup.m_attrs;
	for( int i=0; i<16; i++,dstAttr++ )
	{
		unsigned char vshattrib = vshAttribMap[ i ];
		if (vshattrib != 0xBB)
		{
			// try to find the match in the decl.
			// idea: put some inverse table in the decl which could accelerate this search.
			
			D3DVERTEXELEMENT9_GL *elem = m_vertDecl->m_elements;
			for( int j=0; j< m_vertDecl->m_elemCount; j++,elem++)
			{
				// if it matches, install it, change vshattrib so the code below does not trigger, then end the loop
				if ( ((vshattrib>>4) == elem->m_dxdecl.Usage) && ((vshattrib & 0x0F) == elem->m_dxdecl.UsageIndex) )
				{
					// targeting attribute #i in the setup with element data #j from the decl
				
					*dstAttr = elem->m_gldecl;
					
					// then fix buffer, stride, offset - note that we honor the base vertex index here by fiddling the offset
					int streamIndex = elem->m_dxdecl.Stream;
					dstAttr->m_buffer = m_streams[ streamIndex ].m_vtxBuffer->m_vtxBuffer;
					dstAttr->m_stride = m_streams[ streamIndex ].m_stride;
					dstAttr->m_offset += m_streams[ streamIndex ].m_offset + (baseVertexIndex * dstAttr->m_stride); 

					// set mask
					setup.m_attrMask |= (1<<i);
					
					// end loop
					vshattrib = 0xBB;
					j = 999;
				}
			}
			
			// if vshattrib is not 0xBB here, that means we could not find a source in the decl for it
			if (vshattrib != 0xBB)
			{
				// fill out attr the same way as usual, we just pass NULL for the buffer and ask GLM to have mercy on us

				dstAttr->m_buffer = NULL;
				dstAttr->m_stride = 0;
				dstAttr->m_offset = 0;

				// only implement certain usages... if we haven't seen it before, stop.
				switch( vshattrib >>4 )	// aka usage
				{
					case	D3DDECLUSAGE_POSITION:
					case	D3DDECLUSAGE_BLENDWEIGHT:
					case	D3DDECLUSAGE_BLENDINDICES:
						Debugger();
					break;
					
					case	D3DDECLUSAGE_NORMAL:
						dstAttr->m_datasize = 3;
						dstAttr->m_datatype = GL_FLOAT;
						dstAttr->m_normalized = false;
					break;
						
					case	D3DDECLUSAGE_PSIZE:
						Debugger();
					break;

					case	D3DDECLUSAGE_TEXCOORD:
						dstAttr->m_datasize = 3;
						dstAttr->m_datatype = GL_FLOAT;
						dstAttr->m_normalized = false;
					break;
					
					case	D3DDECLUSAGE_TANGENT:
					case	D3DDECLUSAGE_BINORMAL:
					case	D3DDECLUSAGE_TESSFACTOR:
					case	D3DDECLUSAGE_PLUGH:
						Debugger();
					break;
					
					case	D3DDECLUSAGE_COLOR:
						dstAttr->m_datasize = 4;
						dstAttr->m_datatype = GL_UNSIGNED_BYTE;
						dstAttr->m_normalized = true;
					break;
					
					case	D3DDECLUSAGE_FOG:
					case	D3DDECLUSAGE_DEPTH:
					case	D3DDECLUSAGE_SAMPLE:
						Debugger();
					break;
				}
			}
		}
	}

	// copy active program's vertex attrib map into the vert setup info
	memcpy( &setup.m_vtxAttribMap, m_vertexShader->m_vtxAttribMap, sizeof( m_vertexShader->m_vtxAttribMap ) );	
	
	m_ctx->SetVertexAttributes( &setup );
}



HRESULT	IDirect3DDevice9::FlushGLM( void )
{
	Debugger();// old routine not used now
}

HRESULT IDirect3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
{
	this->FlushStates( 0xFFFFFFFF );

	Debugger();
	return S_OK;
}

//	Type
//	[in] Member of the D3DPRIMITIVETYPE enumerated type, describing the type of primitive to render. D3DPT_POINTLIST is not supported with this method. See Remarks.

//	BaseVertexIndex
//	[in] Offset from the start of the vertex buffer to the first vertex. See Scenario 4.

//	MinIndex
//	[in] Minimum vertex index for vertices used during this call. This is a zero based index relative to BaseVertexIndex.

//	NumVertices
//	[in] Number of vertices used during this call. The first vertex is located at index: BaseVertexIndex + MinIndex.

//	StartIndex
//	[in] Index of the first index to use when accesssing the vertex buffer. Beginning at StartIndex to index vertices from the vertex buffer.

//	PrimitiveCount
//	[in] Number of primitives to render. The number of vertices used is a function of the primitive count and the primitive type. The maximum number of primitives allowed is determined by checking the MaxPrimitiveCount member of the D3DCAPS9 structure.


HRESULT IDirect3DDevice9::DrawIndexedPrimitive( D3DPRIMITIVETYPE Type,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount )
{
	this->FlushStates( 0xFFFFFFFF );

	this->FlushSamplers();

	this->FlushIndexBindings( );
	this->FlushVertexBindings( BaseVertexIndex );
	m_ctx->FlushDrawStates( true );

	if (gl.m_FogEnable)
	{
		GLMPRINTF(("-D- IDirect3DDevice9::DrawIndexedPrimitive is seeing enabled fog..."));
	}
	
	switch(Type)
	{
		case	D3DPT_POINTLIST:
			Debugger();
		break;

		case	D3DPT_LINELIST:
			GLMPRINTF(("-X- IDirect3DDevice9::DrawIndexedPrimitive( D3DPT_LINELIST ) - ignored."));
//			Debugger();
			m_ctx->DrawRangeElements( (GLenum)GL_LINES, (GLuint)MinVertexIndex, (GLuint)(MinVertexIndex + NumVertices), (GLsizei)primCount*2, (GLenum)GL_UNSIGNED_SHORT, (const GLvoid *)(startIndex * sizeof(short)) );
		break;
		
		case	D3DPT_TRIANGLELIST:
			m_ctx->DrawRangeElements(GL_TRIANGLES, (GLuint)MinVertexIndex, (GLuint)(MinVertexIndex + NumVertices), (GLsizei)primCount*3, (GLenum)GL_UNSIGNED_SHORT, (const GLvoid *)(startIndex * sizeof(short)) );
		break;
		
		case D3DPT_TRIANGLESTRIP:
			// enabled... Debugger();
			m_ctx->DrawRangeElements(GL_TRIANGLE_STRIP, (GLuint)MinVertexIndex, (GLuint)(MinVertexIndex + NumVertices), (GLsizei)(2+primCount), (GLenum)GL_UNSIGNED_SHORT, (const GLvoid *)(startIndex * sizeof(short)) );
		break;
	}
	
	return S_OK;
}

HRESULT IDirect3DDevice9::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
	this->FlushStates( 0xFFFFFFFF );

	Debugger();
	return S_OK;
}




BOOL IDirect3DDevice9::ShowCursor(BOOL bShow)
{
	// FIXME NOP
	//Debugger();
	return TRUE;
}

void	d3drect_to_glmbox( D3DRECT *src, GLScissorBox_t *dst )
{
	// to convert from a d3d rect to a GL rect you have to fix up the vertical axis, since D3D Y=0 is the top, but GL Y=0 is the bottom.
	// you can't fix it without knowing the height.

	dst->width	= src->x2 - src->x1;
	dst->x		= src->x1;				// left edge

	dst->height	= src->y2 - src->y1;
	dst->y		= src->y1;				// bottom edge - take large Y from d3d and subtract from surf height.
}

HRESULT IDirect3DDevice9::Clear(DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil)
{
	
	this->FlushStates( (1<<kGLViewportBox) | (1<<kGLViewportDepthRange) );	// i.e. viewport changes..
	m_ctx->FlushDrawStates( false );

	
	//debugging Color = (rand() | 0xFF0000FF) & 0xFF3F3FFF;
	if (!Count)
	{
		// run clear with no added rectangle
		m_ctx->Clear(	(Flags&D3DCLEAR_TARGET)!=0, Color,
						(Flags&D3DCLEAR_ZBUFFER)!=0, Z,
						(Flags&D3DCLEAR_STENCIL)!=0, Stencil,
						NULL
					);
	}
	else
	{
		GLScissorBox_t	tempbox;
		
		// do the rects one by one and convert each one to GL form
		for( int i=0; i<Count; i++)
		{
			D3DRECT d3dtempbox = pRects[i];
			d3drect_to_glmbox( &d3dtempbox, &tempbox );

			m_ctx->Clear(	(Flags&D3DCLEAR_TARGET)!=0, Color,
							(Flags&D3DCLEAR_ZBUFFER)!=0, Z,
							(Flags&D3DCLEAR_STENCIL)!=0, Stencil,
							&tempbox
						);
		}
	}

	return S_OK;
}

HRESULT IDirect3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix)
{
	Debugger();
	return S_OK;
}

HRESULT IDirect3DDevice9::SetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value)
{
	Debugger();
	return S_OK;
}

HRESULT IDirect3DDevice9::ValidateDevice(DWORD* pNumPasses)
{
	Debugger();
	return S_OK;
}

HRESULT IDirect3DDevice9::SetMaterial(CONST D3DMATERIAL9* pMaterial)
{
	GLMPRINTF(("-X- IDirect3DDevice9::SetMaterial - ignored."));
//	Debugger();
	return S_OK;
}


HRESULT IDirect3DDevice9::LightEnable(DWORD Index,BOOL Enable)
{
	Debugger();
	return S_OK;
}

HRESULT IDirect3DDevice9::SetScissorRect(CONST RECT* pRect)
{
	int nSurfaceHeight = m_drawableFBO->m_attach[ kAttColor0 ].m_tex->m_layout->m_key.m_ySize;
	
	GLScissorBox_t newScissorBox = { pRect->left, pRect->top, pRect->right - pRect->left, pRect->bottom - pRect->top };
	gl.m_ScissorBox	= newScissorBox;
	gl.m_stateDirtyMask |= (1<<kGLScissorBox);
	return S_OK;
}

HRESULT IDirect3DDevice9::GetDeviceCaps(D3DCAPS9* pCaps)
{
	Debugger();
	return S_OK;
}


HRESULT IDirect3DDevice9::TestCooperativeLevel()
{
	// game calls this to see if device was lost.
	// last I checked the device was still attached to the computer.
	// so, return OK.

	return S_OK;
}

HRESULT IDirect3DDevice9::SetClipPlane(DWORD Index,CONST float* pPlane)
{
	Assert(Index<2);

	// We actually push the clip plane coeffs to two places
	// - into a shader param for ARB mode
	// - and into the API defined clip plane slots for GLSL mode.
	
	// if ARB mode... THIS NEEDS TO GO... it's messing up the dirty ranges..
	{
	//	this->SetVertexShaderConstantF( DXABSTRACT_VS_CLIP_PLANE_BASE+Index, pPlane, 1 );	// stash the clip plane values into shader param - translator knows where to look
	}
	
	// if GLSL mode... latch it and let FlushStates push it out
	{
		GLClipPlaneEquation_t	peq;
		peq.x = pPlane[0];
		peq.y = pPlane[1];
		peq.z = pPlane[2];
		peq.w = pPlane[3];

		gl.m_ClipPlaneEquation[ Index ] = peq;
		gl.m_stateDirtyMask |= (1<<kGLClipPlaneEquation);

		// m_ctx->WriteClipPlaneEquation( &peq, Index );
	}

	return S_OK;
}

HRESULT IDirect3DDevice9::EvictManagedResources()
{
	GLMPRINTF(("-X- IDirect3DDevice9::EvictManagedResources --> IGNORED"));
	return S_OK;
}

HRESULT IDirect3DDevice9::SetLight(DWORD Index,CONST D3DLIGHT9*)
{
	Debugger();
	return S_OK;
}

void IDirect3DDevice9::SetGammaRamp(UINT iSwapChain,DWORD Flags,CONST D3DGAMMARAMP* pRamp)
{
	// just slam it directly for the time being
	// this code is OS X specific

    CGTableCount sampleCount;
    CGDisplayErr cgErr;

	CGGammaValue	redt[256];
	CGGammaValue	grnt[256];
	CGGammaValue	blut[256];
	for( int i=0; i<256; i++)
	{
		redt[i] = ((float)pRamp->red[i]) / 65535.0f;
		grnt[i] = ((float)pRamp->green[i]) / 65535.0f;
		blut[i] = ((float)pRamp->blue[i]) / 65535.0f;
	}
	cgErr = CGSetDisplayTransferByTable( 0, 256, redt, grnt, blut );
	
}

// ------------------------------------------------------------------------------------------------------------------------------ //

void* ID3DXBuffer::GetBufferPointer()
{
	Debugger();
	return NULL;
}

DWORD ID3DXBuffer::GetBufferSize()
{
	Debugger();
	return 0;
}




#pragma mark ----- More D3DX stuff

// ------------------------------------------------------------------------------------------------------------------------------ //
// D3DX stuff.
// ------------------------------------------------------------------------------------------------------------------------------ //

// matrix stack...

HRESULT D3DXCreateMatrixStack( DWORD Flags, LPD3DXMATRIXSTACK* ppStack)
{
	
	*ppStack = new ID3DXMatrixStack;
	
	(*ppStack)->Create();
	
	return S_OK;
}

HRESULT	ID3DXMatrixStack::Create()
{
	m_stack.EnsureCapacity( 16 );	// 1KB ish
	m_stack.AddToTail();
	m_stackTop = 0;				// top of stack is at index 0 currently
	
	LoadIdentity();
	
	return S_OK;
}

D3DXMATRIX* ID3DXMatrixStack::GetTop()
{
	return (D3DXMATRIX*)&m_stack[ m_stackTop ];
}

void ID3DXMatrixStack::Push()
{
	D3DMATRIX temp = m_stack[ m_stackTop ];
	m_stack.AddToTail( temp );
	m_stackTop ++;
}

void ID3DXMatrixStack::Pop()
{
	int elem = m_stackTop--;
	m_stack.Remove( elem );
}

void ID3DXMatrixStack::LoadIdentity()
{
	D3DXMATRIX *mat = GetTop();

	D3DXMatrixIdentity( mat );
}

void ID3DXMatrixStack::LoadMatrix( const D3DXMATRIX *pMat )
{
	*(GetTop()) = *pMat;
}


void ID3DXMatrixStack::MultMatrix( const D3DXMATRIX *pMat )
{

	// http://msdn.microsoft.com/en-us/library/bb174057(VS.85).aspx
	//	This method right-multiplies the given matrix to the current matrix
	//	(transformation is about the current world origin).
	//		m_pstack[m_currentPos] = m_pstack[m_currentPos] * (*pMat);
	//	This method does not add an item to the stack, it replaces the current
	//  matrix with the product of the current matrix and the given matrix.


	Debugger();
}

void ID3DXMatrixStack::MultMatrixLocal( const D3DXMATRIX *pMat )
{
	//	http://msdn.microsoft.com/en-us/library/bb174058(VS.85).aspx
	//	This method left-multiplies the given matrix to the current matrix
	//	(transformation is about the local origin of the object).
	//		m_pstack[m_currentPos] = (*pMat) * m_pstack[m_currentPos];
	//	This method does not add an item to the stack, it replaces the current
	//	matrix with the product of the given matrix and the current matrix.


	Debugger();
}

HRESULT ID3DXMatrixStack::ScaleLocal(FLOAT x, FLOAT y, FLOAT z)
{
	//	http://msdn.microsoft.com/en-us/library/bb174066(VS.85).aspx
	//	Scale the current matrix about the object origin.
	//	This method left-multiplies the current matrix with the computed
	//	scale matrix. The transformation is about the local origin of the object.
	//
	//	D3DXMATRIX tmp;
	//	D3DXMatrixScaling(&tmp, x, y, z);
	//	m_stack[m_currentPos] = tmp * m_stack[m_currentPos];

	Debugger();
}


HRESULT ID3DXMatrixStack::RotateAxisLocal(CONST D3DXVECTOR3* pV, FLOAT Angle)
{
	//	http://msdn.microsoft.com/en-us/library/bb174062(VS.85).aspx
	//	Left multiply the current matrix with the computed rotation
	//	matrix, counterclockwise about the given axis with the given angle.
	//	(rotation is about the local origin of the object)

	//	D3DXMATRIX tmp;
	//	D3DXMatrixRotationAxis( &tmp, pV, angle );
	//	m_stack[m_currentPos] = tmp * m_stack[m_currentPos];
	//	Because the rotation is left-multiplied to the matrix stack, the rotation
	//	is relative to the object's local coordinate space.
	
	Debugger();
}

HRESULT ID3DXMatrixStack::TranslateLocal(FLOAT x, FLOAT y, FLOAT z)
{
	//	http://msdn.microsoft.com/en-us/library/bb174068(VS.85).aspx
	//	Left multiply the current matrix with the computed translation
	//	matrix. (transformation is about the local origin of the object)

	//	D3DXMATRIX tmp;
	//	D3DXMatrixTranslation( &tmp, x, y, z );
	//	m_stack[m_currentPos] = tmp * m_stack[m_currentPos];

	Debugger();
}




const char* D3DXGetPixelShaderProfile( IDirect3DDevice9 *pDevice )
{
	Debugger();
	return "";
}


D3DXMATRIX* D3DXMatrixMultiply( D3DXMATRIX *pOut, CONST D3DXMATRIX *pM1, CONST D3DXMATRIX *pM2 )
{
	D3DXMATRIX temp;
	
	for( int i=0; i<4; i++)
	{
		for( int j=0; j<4; j++)
		{
			temp.m[i][j]	=	(pM1->m[ i ][ 0 ] * pM2->m[ 0 ][ j ])
							+	(pM1->m[ i ][ 1 ] * pM2->m[ 1 ][ j ])
							+	(pM1->m[ i ][ 2 ] * pM2->m[ 2 ][ j ])
							+	(pM1->m[ i ][ 3 ] * pM2->m[ 3 ][ j ]);
		}
	}
	*pOut = temp;
	return pOut;
}

// Transform a 3D vector by a given matrix, projecting the result back into w = 1
// http://msdn.microsoft.com/en-us/library/ee417622(VS.85).aspx
D3DXVECTOR3* D3DXVec3TransformCoord(D3DXVECTOR3 *pOut, CONST D3DXVECTOR3 *pV, CONST D3DXMATRIX *pM)
{
    D3DXVECTOR3 vOut;
    
	vOut.x = vOut.y = vOut.z = 0.0f;
    float norm = (pM->m[0][3] * pV->x) + (pM->m[1][3] * pV->y) + (pM->m[2][3] *pV->z) + pM->m[3][3];
	if ( norm )
	{
		vOut.x = (pM->m[0][0] * pV->x + pM->m[1][0] * pV->y + pM->m[2][0] * pV->z + pM->m[3][0]) / norm;
		vOut.y = (pM->m[0][1] * pV->x + pM->m[1][1] * pV->y + pM->m[2][1] * pV->z + pM->m[3][1]) / norm;
		vOut.z = (pM->m[0][2] * pV->x + pM->m[1][2] * pV->y + pM->m[2][2] * pV->z + pM->m[3][2]) / norm;
	}
    
    *pOut = vOut;

	return pOut;
}

void D3DXMatrixIdentity( D3DXMATRIX *mat )
{
	for( int i=0; i<4; i++)
	{
		for( int j=0; j<4; j++)
		{
			mat->m[i][j] = (i==j) ? 1.0f : 0.0f;	// 1's on the diagonal.
		}
	}
}

D3DXMATRIX* D3DXMatrixTranslation( D3DXMATRIX *pOut, FLOAT x, FLOAT y, FLOAT z )
{
	D3DXMatrixIdentity( pOut );
	pOut->m[3][0] = x;
	pOut->m[3][1] = y;
	pOut->m[3][2] = z;
	return pOut;
}

D3DXMATRIX* D3DXMatrixInverse( D3DXMATRIX *pOut, FLOAT *pDeterminant, CONST D3DXMATRIX *pM )
{
	Assert( sizeof( D3DXMATRIX ) == (16 * sizeof(float) ) );
	Assert( sizeof( VMatrix ) == (16 * sizeof(float) ) );
	Assert( pDeterminant == NULL );	// homey don't play that
	
	VMatrix *origM = (VMatrix*)pM;
	VMatrix *destM = (VMatrix*)pOut;
	
	bool success = MatrixInverseGeneral( *origM, *destM );
	Assert( success );
	
	return pOut;
}


D3DXMATRIX* D3DXMatrixTranspose( D3DXMATRIX *pOut, CONST D3DXMATRIX *pM )
{
	if (pOut != pM)
	{
		for( int i=0; i<4; i++)
		{
			for( int j=0; j<4; j++)
			{
				pOut->m[i][j] = pM->m[j][i];
			}
		}
	}
	else
	{
		D3DXMATRIX temp = *pM;
		D3DXMatrixTranspose( pOut, &temp );
	}

	return NULL;
}


D3DXPLANE* D3DXPlaneNormalize( D3DXPLANE *pOut, CONST D3DXPLANE *pP)
{
	// not very different from normalizing a vector.
	// figure out the square root of the sum-of-squares of the x,y,z components
	// make sure that's non zero
	// then divide all four components by that value
	// or return some dummy plane like 0,0,1,0 if it fails
	
	float	len = sqrt( (pP->a * pP->a) + (pP->b * pP->b) + (pP->c * pP->c) );
	if (len > 1e-10)	//FIXME need a real epsilon here ?
	{
		pOut->a = pP->a / len;		pOut->b = pP->b / len;		pOut->c = pP->c / len;		pOut->d = pP->d / len;
	}
	else
	{
		pOut->a = 0.0f;				pOut->b = 0.0f;				pOut->c = 1.0f;				pOut->d = 0.0f;
	}
	return pOut;
}


D3DXVECTOR4* D3DXVec4Transform( D3DXVECTOR4 *pOut, CONST D3DXVECTOR4 *pV, CONST D3DXMATRIX *pM )
{
	VMatrix *mat = (VMatrix*)pM;
	Vector4D *vIn = (Vector4D*)pV;
	Vector4D *vOut = (Vector4D*)pOut;

	Vector4DMultiplyTranspose( *mat, *vIn, *vOut );

	return pOut;
}



D3DXVECTOR4* D3DXVec4Normalize( D3DXVECTOR4 *pOut, CONST D3DXVECTOR4 *pV )
{
	Vector4D *vIn = (Vector4D*) pV;
	Vector4D *vOut = (Vector4D*) pOut;

	*vOut = *vIn;
	Vector4DNormalize( *vOut );
	
	return pOut;
}


D3DXMATRIX* D3DXMatrixOrthoOffCenterRH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn,FLOAT zf )
{
	Debugger();
	return NULL;
}


D3DXMATRIX* D3DXMatrixPerspectiveRH( D3DXMATRIX *pOut, FLOAT w, FLOAT h, FLOAT zn, FLOAT zf )
{
	Debugger();
	return NULL;
}


D3DXMATRIX* D3DXMatrixPerspectiveOffCenterRH( D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf )
{
	Debugger();
	return NULL;
}


D3DXPLANE* D3DXPlaneTransform( D3DXPLANE *pOut, CONST D3DXPLANE *pP, CONST D3DXMATRIX *pM )
{
	float *out = &pOut->a;

	// dot dot dot
	for( int x=0; x<4; x++ )
	{
		out[x] =	(pM->m[0][x] * pP->a)
				+	(pM->m[1][x] * pP->b)
				+	(pM->m[2][x] * pP->c)
				+	(pM->m[3][x] * pP->d);
	}
	
	return pOut;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

IDirect3D9 *Direct3DCreate9(UINT SDKVersion)
{
	GLMPRINTF(( "-X- Direct3DCreate9: %d", SDKVersion ));

	return new IDirect3D9;
}

// ------------------------------------------------------------------------------------------------------------------------------ //

void D3DPERF_SetOptions( DWORD dwOptions )
{
}


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
        LPD3DXCONSTANTTABLE*            ppConstantTable)
{
	Debugger();	// is anyone calling this ?
	return S_OK;
}


#endif

