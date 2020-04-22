//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================


#include "avi/iavi.h"
#include "avi.h"
#include "filesystem.h"
#include "tier1/strtools.h"
#include "tier1/utllinkedlist.h"
#include "tier1/keyvalues.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/materialsystemutil.h"
#include "materialsystem/itexture.h"
#include "vtf/vtf.h"
#include "pixelwriter.h"
#include "tier3/tier3.h"


//-----------------------------------------------------------------------------
//
// Class used to write out AVI files
//
//-----------------------------------------------------------------------------
class CAviFile
{
public:
	CAviFile();

	void Init( const AVIParams_t& params, void *hWnd );
	void Shutdown();
	void AppendMovieSound( short *buf, size_t bufsize );
	void AppendMovieFrame( const BGR888_t *pRGBData );

private:
	void Reset();
	void CreateVideoStreams( const AVIParams_t& params, void *hWnd );
	void CreateAudioStream();

	bool			m_bValid;
	int				m_nWidth;
	int				m_nHeight;
	int				m_nFrameRate;
	int				m_nFrameScale;
	int				m_nFrame;
	int				m_nSample;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAviFile::CAviFile()
{
	Reset();
}


//-----------------------------------------------------------------------------
// Reset the avi file
//-----------------------------------------------------------------------------
void CAviFile::Reset()
{
	m_bValid				= false;
	m_nWidth				= 0;
	m_nHeight				= 0;
	m_nFrameRate			= 0;
	m_nFrameScale			= 1;
	m_nFrame				= 0;
	m_nSample				= 0;
}

	
//-----------------------------------------------------------------------------
// Start recording an AVI
//-----------------------------------------------------------------------------
void CAviFile::Init( const AVIParams_t& params, void *hWnd )
{
	Reset();

	char avifilename[ 512 ];
	char fullavifilename[ 512 ];
	Q_snprintf( avifilename, sizeof( avifilename ), "%s", params.m_pFileName );
	Q_SetExtension( avifilename, ".avi", sizeof( avifilename ) );

	g_pFullFileSystem->RelativePathToFullPath( avifilename, params.m_pPathID, fullavifilename, sizeof( fullavifilename ) );
	if ( g_pFullFileSystem->FileExists( fullavifilename, params.m_pPathID ) )
	{
		g_pFullFileSystem->RemoveFile( fullavifilename, params.m_pPathID );
	}

	m_nFrameRate = params.m_nFrameRate;
	m_nFrameScale = params.m_nFrameScale;

	m_bValid = true;

	m_nHeight = params.m_nHeight;
	m_nWidth = params.m_nWidth;

	CreateVideoStreams( params, hWnd );
	CreateAudioStream();
}

void CAviFile::Shutdown()
{
	Reset();
}


void CAviFile::CreateVideoStreams( const AVIParams_t& params, void *hWnd )
{
}

void CAviFile::CreateAudioStream()
{
}

void CAviFile::AppendMovieSound( short *buf, size_t bufsize )
{
	if ( !m_bValid )
		return;

	unsigned long numsamps = bufsize / sizeof( short ); // numbytes*8 / au->wfx.wBitsPerSample;

	m_nSample += numsamps;
}


//-----------------------------------------------------------------------------
// Adds a frame of the movie to the AVI
//-----------------------------------------------------------------------------
void CAviFile::AppendMovieFrame( const BGR888_t *pRGBData )
{
	if ( !m_bValid )
		return;

	++m_nFrame;
}


//-----------------------------------------------------------------------------
//
// Class used to associated AVI files with IMaterials
//
//-----------------------------------------------------------------------------
class CAVIMaterial : public ITextureRegenerator
{
public:
	CAVIMaterial();

	// Initializes, shuts down the material
	bool Init( const char *pMaterialName, const char *pFileName, const char *pPathID );
	void Shutdown();

	// Inherited from ITextureRegenerator
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect );
	virtual void Release();

	// Returns the material
	IMaterial *GetMaterial();

	// Returns the texcoord range
	void GetTexCoordRange( float *pMaxU, float *pMaxV );

	// Returns the frame size of the AVI (stored in a subrect of the material itself)
	void GetFrameSize( int *pWidth, int *pHeight );

	// Sets the current time
	void SetTime( float flTime );

	// Returns the frame rate/count of the AVI
	int GetFrameRate( );
	int GetFrameCount( );

	// Sets the frame for an AVI material (use instead of SetTime)
	void SetFrame( float flFrame );

private:
	// Initializes, shuts down the procedural texture
	void CreateProceduralTexture( const char *pTextureName );
	void DestroyProceduralTexture();

	// Initializes, shuts down the procedural material
	void CreateProceduralMaterial( const char *pMaterialName );
	void DestroyProceduralMaterial();

	// Initializes, shuts down the video stream
	void CreateVideoStream( );
	void DestroyVideoStream( );

	CMaterialReference m_Material;
	CTextureReference m_Texture;
	int m_nAVIWidth;
	int m_nAVIHeight;
	int m_nFrameRate;
	int m_nFrameCount;
	int m_nCurrentSample;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAVIMaterial::CAVIMaterial()
{
}


//-----------------------------------------------------------------------------
// Initializes the material
//-----------------------------------------------------------------------------
bool CAVIMaterial::Init( const char *pMaterialName, const char *pFileName, const char *pPathID )
{
	// Determine the full path name of the AVI
	char pAVIFileName[ 512 ];
	char pFullAVIFileName[ 512 ];
	Q_snprintf( pAVIFileName, sizeof( pAVIFileName ), "%s", pFileName );
	Q_DefaultExtension( pAVIFileName, ".avi", sizeof( pAVIFileName ) );
	g_pFullFileSystem->RelativePathToFullPath( pAVIFileName, pPathID, pFullAVIFileName, sizeof( pFullAVIFileName ) );

	CreateProceduralTexture( pMaterialName );
	CreateProceduralMaterial( pMaterialName );
	CreateVideoStream();

	m_Texture->Download();

	return true;
}

void CAVIMaterial::Shutdown()
{
	DestroyVideoStream();
	DestroyProceduralMaterial( );
	DestroyProceduralTexture( );
}


//-----------------------------------------------------------------------------
// Returns the material
//-----------------------------------------------------------------------------
IMaterial *CAVIMaterial::GetMaterial()
{
	return m_Material;
}							   


//-----------------------------------------------------------------------------
// Returns the texcoord range
//-----------------------------------------------------------------------------
void CAVIMaterial::GetTexCoordRange( float *pMaxU, float *pMaxV )
{
	if ( !m_Texture )
	{
		*pMaxU = *pMaxV = 1.0f;
		return;
	}

	int nTextureWidth = m_Texture->GetActualWidth();
	int nTextureHeight = m_Texture->GetActualHeight();
	*pMaxU = (float)m_nAVIWidth / (float)nTextureWidth;
	*pMaxV = (float)m_nAVIHeight / (float)nTextureHeight;
}


//-----------------------------------------------------------------------------
// Returns the frame size of the AVI (stored in a subrect of the material itself)
//-----------------------------------------------------------------------------
void CAVIMaterial::GetFrameSize( int *pWidth, int *pHeight )
{
	*pWidth = m_nAVIWidth;
	*pHeight = m_nAVIHeight;
}


//-----------------------------------------------------------------------------
// Computes a power of two at least as big as the passed-in number
//-----------------------------------------------------------------------------
static inline int ComputeGreaterPowerOfTwo( int n )
{
	int i = 1;
	while ( i < n )
	{
		i <<= 1;
	}
	return i;
}


//-----------------------------------------------------------------------------
// Initializes, shuts down the procedural texture
//-----------------------------------------------------------------------------
void CAVIMaterial::CreateProceduralTexture( const char *pTextureName )
{
	// Choose power-of-two textures which are at least as big as the AVI
	int nWidth = ComputeGreaterPowerOfTwo( m_nAVIWidth ); 
	int nHeight = ComputeGreaterPowerOfTwo( m_nAVIHeight ); 
	m_Texture.InitProceduralTexture( pTextureName, "avi", nWidth, nHeight, 
		IMAGE_FORMAT_RGBA8888, TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP |
		TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY );
	m_Texture->SetTextureRegenerator( this );
}

void CAVIMaterial::DestroyProceduralTexture()
{
	if (m_Texture)
	{
		m_Texture->SetTextureRegenerator( NULL );
		m_Texture.Shutdown();
	}
}


//-----------------------------------------------------------------------------
// Initializes, shuts down the procedural material
//-----------------------------------------------------------------------------
void CAVIMaterial::CreateProceduralMaterial( const char *pMaterialName )
{
	// FIXME: gak, this is backwards.  Why doesn't the material just see that it has a funky basetexture?
	char vmtfilename[ 512 ];
	Q_strcpy( vmtfilename, pMaterialName );
	Q_SetExtension( vmtfilename, ".vmt", sizeof( vmtfilename ) );

	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	if (!pVMTKeyValues->LoadFromFile( g_pFullFileSystem , vmtfilename, "GAME" ))
	{
		pVMTKeyValues->SetString( "$basetexture", m_Texture->GetName() );
		pVMTKeyValues->SetInt( "$nofog", 1 );
		pVMTKeyValues->SetInt( "$spriteorientation", 3 );
		pVMTKeyValues->SetInt( "$translucent", 1 );
	}

	m_Material.Init( pMaterialName, pVMTKeyValues );
	m_Material->Refresh();
}

void CAVIMaterial::DestroyProceduralMaterial()
{
	m_Material.Shutdown();
}


//-----------------------------------------------------------------------------
// Sets the current time
//-----------------------------------------------------------------------------
void CAVIMaterial::SetTime( float flTime )
{

}


//-----------------------------------------------------------------------------
// Returns the frame rate of the AVI
//-----------------------------------------------------------------------------
int CAVIMaterial::GetFrameRate( )
{
	return m_nFrameRate;
}

int CAVIMaterial::GetFrameCount( )
{
	return m_nFrameCount;
}


//-----------------------------------------------------------------------------
// Sets the frame for an AVI material (use instead of SetTime)
//-----------------------------------------------------------------------------
void CAVIMaterial::SetFrame( float flFrame )
{

}


//-----------------------------------------------------------------------------
// Initializes, shuts down the video stream
//-----------------------------------------------------------------------------
void CAVIMaterial::CreateVideoStream( )
{
}

void CAVIMaterial::DestroyVideoStream( )
{

}

	
//-----------------------------------------------------------------------------
// Inherited from ITextureRegenerator
//-----------------------------------------------------------------------------
void CAVIMaterial::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
{
}

void CAVIMaterial::Release()
{
}

	
//-----------------------------------------------------------------------------
//
// Implementation of IAvi
//
//-----------------------------------------------------------------------------
class CAvi : public CTier3AppSystem< IAvi >
{
	typedef CTier3AppSystem< IAvi > BaseClass;

public:
	CAvi();

	// Inherited from IAppSystem 
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Inherited from IAvi
	virtual void	SetMainWindow( void* hWnd );
	virtual AVIHandle_t	StartAVI( const AVIParams_t& params );
	virtual void	FinishAVI( AVIHandle_t h );
	virtual void	AppendMovieSound( AVIHandle_t h, short *buf, size_t bufsize );
	virtual void	AppendMovieFrame( AVIHandle_t h, const BGR888_t *pRGBData );
	virtual AVIMaterial_t CreateAVIMaterial( const char *pMaterialName, const char *pFileName, const char *pPathID );
	virtual void DestroyAVIMaterial( AVIMaterial_t hMaterial );
	virtual void SetTime( AVIMaterial_t hMaterial, float flTime );
	virtual IMaterial* GetMaterial( AVIMaterial_t hMaterial );
	virtual void GetTexCoordRange( AVIMaterial_t hMaterial, float *pMaxU, float *pMaxV );
	virtual void GetFrameSize( AVIMaterial_t hMaterial, int *pWidth, int *pHeight );
	virtual int GetFrameRate( AVIMaterial_t hMaterial );
	virtual void SetFrame( AVIMaterial_t hMaterial, float flFrame );
	virtual int GetFrameCount( AVIMaterial_t hMaterial );

private:
	CUtlLinkedList< CAviFile, AVIHandle_t > m_AVIFiles;

	// NOTE: Have to use pointers here since AVIMaterials inherit from ITextureRegenerator
	// The realloc screws up the pointers held to ITextureRegenerators in the material system.
	CUtlLinkedList< CAVIMaterial*, AVIMaterial_t > m_AVIMaterials;
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CAvi g_AVI;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CAvi, IAvi, AVI_INTERFACE_VERSION, g_AVI );


//-----------------------------------------------------------------------------
// Constructor/destructor
//-----------------------------------------------------------------------------
CAvi::CAvi()
{
}


//-----------------------------------------------------------------------------
// Connect/disconnect
//-----------------------------------------------------------------------------
bool CAvi::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;
	if ( !( g_pFullFileSystem && materials ) )
	{
		Msg( "Avi failed to connect to a required system\n" );
	}
	return ( g_pFullFileSystem && materials );
}


//-----------------------------------------------------------------------------
// Query Interface
//-----------------------------------------------------------------------------
void *CAvi::QueryInterface( const char *pInterfaceName )
{
	if (!Q_strncmp(	pInterfaceName, AVI_INTERFACE_VERSION, Q_strlen(AVI_INTERFACE_VERSION) + 1))
		return (IAvi*)this;

	return NULL;
}


//-----------------------------------------------------------------------------
// Init/shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CAvi::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	return INIT_OK;
}

void CAvi::Shutdown()
{
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Sets the main window
//-----------------------------------------------------------------------------
void CAvi::SetMainWindow( void* hWnd )
{
	Assert( false );
}


//-----------------------------------------------------------------------------
// Start, finish recording an AVI
//-----------------------------------------------------------------------------
AVIHandle_t CAvi::StartAVI( const AVIParams_t& params )
{
	AVIHandle_t h = m_AVIFiles.AddToTail();
	m_AVIFiles[h].Init( params, NULL /*hwnd*/ );
	return h;
}

void CAvi::FinishAVI( AVIHandle_t h )
{
	if ( h != AVIHANDLE_INVALID )
	{
		m_AVIFiles[h].Shutdown();
		m_AVIFiles.Remove( h );
	}
}


//-----------------------------------------------------------------------------
// Add sound buffer
//-----------------------------------------------------------------------------
void CAvi::AppendMovieSound( AVIHandle_t h, short *buf, size_t bufsize )
{
	if ( h != AVIHANDLE_INVALID )
	{
		m_AVIFiles[h].AppendMovieSound( buf, bufsize );
	}
}


//-----------------------------------------------------------------------------
// Add movie frame
//-----------------------------------------------------------------------------
void CAvi::AppendMovieFrame( AVIHandle_t h, const BGR888_t *pRGBData )
{
	if ( h != AVIHANDLE_INVALID )
	{
		m_AVIFiles[h].AppendMovieFrame( pRGBData );
	}
}


//-----------------------------------------------------------------------------
// Create/destroy an AVI material
//-----------------------------------------------------------------------------
AVIMaterial_t CAvi::CreateAVIMaterial( const char *pMaterialName, const char *pFileName, const char *pPathID )
{
	AVIMaterial_t h = m_AVIMaterials.AddToTail();
	m_AVIMaterials[h] = new CAVIMaterial;
	m_AVIMaterials[h]->Init( pMaterialName, pFileName, pPathID );
	return h;
}

void CAvi::DestroyAVIMaterial( AVIMaterial_t h )
{
	if ( h != AVIMATERIAL_INVALID )
	{
		m_AVIMaterials[h]->Shutdown();
		delete m_AVIMaterials[h];
		m_AVIMaterials.Remove( h );
	}
}


//-----------------------------------------------------------------------------
// Sets the time for an AVI material
//-----------------------------------------------------------------------------
void CAvi::SetTime( AVIMaterial_t h, float flTime )
{
	if ( h != AVIMATERIAL_INVALID )
	{
		m_AVIMaterials[h]->SetTime( flTime );
	}
}


//-----------------------------------------------------------------------------
// Gets the IMaterial associated with an AVI material
//-----------------------------------------------------------------------------
IMaterial* CAvi::GetMaterial( AVIMaterial_t h )
{
	if ( h != AVIMATERIAL_INVALID )
		return m_AVIMaterials[h]->GetMaterial();
	return NULL;
}


//-----------------------------------------------------------------------------
// Returns the max texture coordinate of the AVI
//-----------------------------------------------------------------------------
void CAvi::GetTexCoordRange( AVIMaterial_t h, float *pMaxU, float *pMaxV )
{
	if ( h != AVIMATERIAL_INVALID )
	{
		m_AVIMaterials[h]->GetTexCoordRange( pMaxU, pMaxV );
	}
	else
	{
		*pMaxU = *pMaxV = 1.0f;
	}
}


//-----------------------------------------------------------------------------
// Returns the frame size of the AVI (is a subrect of the material itself)
//-----------------------------------------------------------------------------
void CAvi::GetFrameSize( AVIMaterial_t h, int *pWidth, int *pHeight )
{
	if ( h != AVIMATERIAL_INVALID )
	{
		m_AVIMaterials[h]->GetFrameSize( pWidth, pHeight );
	}
	else
	{
		*pWidth = *pHeight = 1;
	}
}


//-----------------------------------------------------------------------------
// Returns the frame rate of the AVI
//-----------------------------------------------------------------------------
int CAvi::GetFrameRate( AVIMaterial_t h )
{
	if ( h != AVIMATERIAL_INVALID )
		return m_AVIMaterials[h]->GetFrameRate();
	return 1;
}

int CAvi::GetFrameCount( AVIMaterial_t h )
{
	if ( h != AVIMATERIAL_INVALID )
		return m_AVIMaterials[h]->GetFrameCount();
	return 1;
}


//-----------------------------------------------------------------------------
// Sets the frame for an AVI material (use instead of SetTime)
//-----------------------------------------------------------------------------
void CAvi::SetFrame( AVIMaterial_t h, float flFrame )
{
	if ( h != AVIMATERIAL_INVALID )
	{
		m_AVIMaterials[h]->SetFrame( flFrame );
	}
}
