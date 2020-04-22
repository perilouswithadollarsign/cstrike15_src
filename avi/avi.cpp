//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
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


#pragma warning( disable : 4201 )

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vfw.h>

#pragma warning( default : 4201 )

DWORD g_dwLastValidCodec = 0;

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
	IAVIFile		*m_pAVIFile;
	WAVEFORMATEX	m_wFormat;
	int				m_nFrameRate;
	int				m_nFrameScale;
	IAVIStream		*m_pAudioStream;
	IAVIStream		*m_pVideoStream;
	IAVIStream		*m_pCompressedStream;
	int				m_nFrame;
	int				m_nSample;
	HDC				m_memdc;
	HBITMAP			m_DIBSection;
	BITMAPINFO		m_bi;
	BITMAPINFOHEADER *m_bih;
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
	Q_memset( &m_wFormat, 0, sizeof( m_wFormat ) );
	Q_memset( &m_bi, 0, sizeof( m_bi ) );

	m_bValid				= false;
	m_nWidth				= 0;
	m_nHeight				= 0;
	m_pAVIFile				= NULL;
	m_nFrameRate			= 0;
	m_nFrameScale			= 1;
	m_pAudioStream			= NULL;
	m_pVideoStream			= NULL;
	m_pCompressedStream		= NULL;
	m_nFrame				= 0;
	m_nSample				= 0;
	m_memdc					= ( HDC )0;
	m_DIBSection			= ( HBITMAP )0;

	m_bih					= &m_bi.bmiHeader;
	m_bih->biSize			= sizeof( *m_bih );
	//m_bih->biWidth		= xxx
	//m_bih->biHeight		= xxx
	m_bih->biPlanes			= 1;
	m_bih->biBitCount		= 24;
	m_bih->biCompression	= BI_RGB;
	//m_bih->biSizeImage	= ( ( m_bih->biWidth * m_bih->biBitCount/8 + 3 )& 0xFFFFFFFC ) * m_bih->biHeight;
	m_bih->biXPelsPerMeter	= 10000;
	m_bih->biYPelsPerMeter	= 10000;
	m_bih->biClrUsed		= 0;
	m_bih->biClrImportant	= 0;
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

	HRESULT hr = AVIFileOpen( &m_pAVIFile, fullavifilename, OF_WRITE | OF_CREATE, NULL );
	if ( hr != AVIERR_OK ) 
		return;

	m_wFormat.cbSize			= sizeof( m_wFormat );
	m_wFormat.wFormatTag		= WAVE_FORMAT_PCM;
	m_wFormat.nChannels			= params.m_nNumChannels;
	m_wFormat.nSamplesPerSec	= params.m_nSampleRate; 
	m_wFormat.nBlockAlign		= params.m_nNumChannels * ( params.m_nSampleBits == 8 ? 1 : 2 );
	m_wFormat.nAvgBytesPerSec	= m_wFormat.nBlockAlign * params.m_nSampleRate; 
	m_wFormat.wBitsPerSample	= params.m_nSampleBits; 

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
	if ( m_pAudioStream ) 
	{
		AVIStreamRelease( m_pAudioStream );
		m_pAudioStream = NULL;
	}
	if ( m_pVideoStream ) 
	{
		AVIStreamRelease( m_pVideoStream );
		m_pVideoStream = NULL;
	}
	if ( m_pCompressedStream ) 
	{
		AVIStreamRelease( m_pCompressedStream );
		m_pCompressedStream = NULL;
	}

	if ( m_pAVIFile ) 
	{
		AVIFileRelease( m_pAVIFile );
		m_pAVIFile = NULL;
	}

	if ( m_DIBSection != 0 )
	{
		DeleteObject( m_DIBSection );
	}

	if ( m_memdc != 0 )
	{
		// Release the compatible DC
		DeleteDC( m_memdc );
	}

	Reset();
}

static unsigned int FormatAviMessage( HRESULT code, char *buf, unsigned int len)
{ 
	const char *msg="unknown avi result code";
	switch (code)
	{ 
	case S_OK: msg="Success"; break;
	case AVIERR_BADFORMAT: msg="AVIERR_BADFORMAT: corrupt file or unrecognized format"; break;
	case AVIERR_MEMORY: msg="AVIERR_MEMORY: insufficient memory"; break;
	case AVIERR_FILEREAD: msg="AVIERR_FILEREAD: disk error while reading file"; break;
	case AVIERR_FILEOPEN: msg="AVIERR_FILEOPEN: disk error while opening file"; break;
	case REGDB_E_CLASSNOTREG: msg="REGDB_E_CLASSNOTREG: file type not recognised"; break;
	case AVIERR_READONLY: msg="AVIERR_READONLY: file is read-only"; break;
	case AVIERR_NOCOMPRESSOR: msg="AVIERR_NOCOMPRESSOR: a suitable compressor could not be found"; break;
	case AVIERR_UNSUPPORTED: msg="AVIERR_UNSUPPORTED: compression is not supported for this type of data"; break;
	case AVIERR_INTERNAL: msg="AVIERR_INTERNAL: internal error"; break;
	case AVIERR_BADFLAGS: msg="AVIERR_BADFLAGS"; break;
	case AVIERR_BADPARAM: msg="AVIERR_BADPARAM"; break;
	case AVIERR_BADSIZE: msg="AVIERR_BADSIZE"; break;
	case AVIERR_BADHANDLE: msg="AVIERR_BADHANDLE"; break;
	case AVIERR_FILEWRITE: msg="AVIERR_FILEWRITE: disk error while writing file"; break;
	case AVIERR_COMPRESSOR: msg="AVIERR_COMPRESSOR"; break;
	case AVIERR_NODATA: msg="AVIERR_READONLY"; break;
	case AVIERR_BUFFERTOOSMALL: msg="AVIERR_BUFFERTOOSMALL"; break;
	case AVIERR_CANTCOMPRESS: msg="AVIERR_CANTCOMPRESS"; break;
	case AVIERR_USERABORT: msg="AVIERR_USERABORT"; break;
	case AVIERR_ERROR: msg="AVIERR_ERROR"; break;
	}
	unsigned int mlen = (unsigned int)Q_strlen( msg );
	if ( buf==0 || len==0 ) 
		return mlen;
	unsigned int n=mlen; 
	if (n+1>len)
	{
		n=len-1;
	}
	strncpy(buf,msg,n); 
	buf[n]=0;
	return mlen;
}

static void ReportError( HRESULT hr )
{
	char buf[ 512 ];
	FormatAviMessage( hr, buf, sizeof( buf ) );
	Warning( "%s\n", buf );
}

void CAviFile::CreateVideoStreams( const AVIParams_t& params, void *hWnd )
{
	AVISTREAMINFO streaminfo; 
	Q_memset( &streaminfo, 0, sizeof( streaminfo ) ) ;
    streaminfo.fccType		= streamtypeVIDEO;
    streaminfo.fccHandler	= 0; 
    streaminfo.dwScale		= params.m_nFrameScale;
    streaminfo.dwRate		= params.m_nFrameRate;
    streaminfo.dwSuggestedBufferSize  = params.m_nWidth * params.m_nHeight * 3;
    
	SetRect( &streaminfo.rcFrame, 0, 0, params.m_nWidth, params.m_nHeight );
    
	HRESULT hr = AVIFileCreateStream( m_pAVIFile, &m_pVideoStream, &streaminfo );
	if ( hr != AVIERR_OK )
	{
		m_bValid = false;
		ReportError( hr );
		return;
	}

	AVICOMPRESSOPTIONS compression;
	Q_memset( &compression, 0, sizeof( compression ) );
	AVICOMPRESSOPTIONS *aopts[1];
	aopts[ 0 ] = &compression;

	// Choose DIVX compressor for now
	Warning( "FIXME:  DIVX only for now\n" );

	if ( params.m_bGetCodecFromUser )
	{
		// FIXME:  This won't work so well in full screen!!!
		if ( !AVISaveOptions( (HWND)hWnd, 0, 1, &m_pVideoStream, aopts ) )
		{
			m_bValid = false;
			return;
		}

		// Cache for next time
		g_dwLastValidCodec = compression.fccHandler;
	}
	else
	{
		compression.fccHandler = g_dwLastValidCodec ? g_dwLastValidCodec : mmioFOURCC( 'd', 'i', 'b', ' ' );
	}

    hr = AVIMakeCompressedStream( &m_pCompressedStream, m_pVideoStream, &compression, NULL );
    if ( hr != AVIERR_OK )
	{
		m_bValid = false;
		ReportError( hr );
		return;
	}

	// Create a compatible DC
	HDC hdcscreen = GetDC( GetDesktopWindow() );
	m_memdc = CreateCompatibleDC(hdcscreen); 
	ReleaseDC( GetDesktopWindow(), hdcscreen );

	// Set up a DIBSection for the screen
	m_bih->biWidth		= params.m_nWidth;
	m_bih->biHeight		= params.m_nHeight;
	m_bih->biSizeImage	= ( ( m_bih->biWidth * m_bih->biBitCount / 8 + 3 )& 0xFFFFFFFC ) * m_bih->biHeight;
	
	// Create the DIBSection
	void *bits;
	m_DIBSection = CreateDIBSection
	(
		m_memdc,
		( BITMAPINFO *)m_bih,
		DIB_RGB_COLORS,
		&bits,
		NULL,
		NULL
	);

	// Get at the DIBSection object
    DIBSECTION dibs; 
	GetObject( m_DIBSection, sizeof( dibs ), &dibs );

	// Set the stream format
    hr = AVIStreamSetFormat( 
		m_pCompressedStream, 
		0, 
		&dibs.dsBmih, 
		dibs.dsBmih.biSize + dibs.dsBmih.biClrUsed *sizeof( RGBQUAD )
		);

    if ( hr != AVIERR_OK )
	{ 
		m_bValid = false;
		ReportError( hr );
		return;
	}
}

void CAviFile::CreateAudioStream()
{
	AVISTREAMINFO audiostream; 
	Q_memset( &audiostream, 0, sizeof( audiostream ) );
	audiostream.fccType			= streamtypeAUDIO;
	audiostream.dwScale			= m_wFormat.nBlockAlign;
	audiostream.dwRate			= m_wFormat.nSamplesPerSec * m_wFormat.nBlockAlign; 
	audiostream.dwSampleSize	= m_wFormat.nBlockAlign;
	audiostream.dwQuality		= (DWORD)-1;

	HRESULT hr = AVIFileCreateStream( m_pAVIFile, &m_pAudioStream, &audiostream );
	if ( hr != AVIERR_OK )
	{
		m_bValid = false;
		ReportError( hr );
		return;
	}
	hr = AVIStreamSetFormat( m_pAudioStream, 0,	&m_wFormat, sizeof( m_wFormat ) );
	if ( hr != AVIERR_OK )
	{
		m_bValid = false;
		ReportError( hr );
		return;
	}
}

void CAviFile::AppendMovieSound( short *buf, size_t bufsize )
{
	if ( !m_bValid )
		return;

	unsigned long numsamps = bufsize / sizeof( short ); // numbytes*8 / au->wfx.wBitsPerSample;
	//
	// now we can write the data
	HRESULT hr = AVIStreamWrite
	(
		m_pAudioStream,
		m_nSample,
		numsamps,
		buf,
		bufsize,
        0,
		NULL,
		NULL
	);
	if ( hr != AVIERR_OK )
	{
		m_bValid = false;

		ReportError( hr );

		return;
	}

	m_nSample += numsamps;
}


//-----------------------------------------------------------------------------
// Adds a frame of the movie to the AVI
//-----------------------------------------------------------------------------
void CAviFile::AppendMovieFrame( const BGR888_t *pRGBData )
{
	if ( !m_bValid )
		return;

	DIBSECTION dibs; 

	HGDIOBJ hOldObject = SelectObject( m_memdc, m_DIBSection );

	// Update the DIBSection bits
	// FIXME: Have to invert this vertically since passing in negative
	// biHeights in the m_bih field doesn't make the system know it's a top-down AVI
	int scanlines = 0;
	for ( int i = 0; i < m_nHeight; ++i )
	{
		scanlines += SetDIBits( m_memdc, m_DIBSection, m_nHeight - i - 1, 1, pRGBData,
			( CONST BITMAPINFO * )m_bih, DIB_RGB_COLORS );
		pRGBData += m_nWidth;
	}

	int objectSize = GetObject( m_DIBSection, sizeof( dibs ), &dibs );
	if ( scanlines != m_nHeight || objectSize != sizeof( DIBSECTION ))
	{
		SelectObject( m_memdc, hOldObject );
		m_bValid = false;
		return;
	}

	// Now we can add the frame
	HRESULT hr = AVIStreamWrite(
		m_pCompressedStream, 
		m_nFrame, 
		1, 
		dibs.dsBm.bmBits, 
		dibs.dsBmih.biSizeImage, 
		AVIIF_KEYFRAME, 
		NULL, 
		NULL );

	SelectObject( m_memdc, hOldObject );

	if ( hr != AVIERR_OK )
	{
		m_bValid = false;
		ReportError( hr );
		return;
	}

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
	IAVIFile *m_pAVIFile;
	IAVIStream *m_pAVIStream;
	IGetFrame *m_pGetFrame;
	int m_nAVIWidth;
	int m_nAVIHeight;
	int m_nFrameRate;
	int m_nFrameCount;
	int m_nCurrentSample;
	HDC				m_memdc;
	HBITMAP			m_DIBSection;
	BITMAPINFO		m_bi;
	BITMAPINFOHEADER *m_bih;
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAVIMaterial::CAVIMaterial()
{
	Q_memset( &m_bi, 0, sizeof( m_bi ) );
	m_memdc = ( HDC )0;
	m_DIBSection = ( HBITMAP )0;
	m_pAVIStream = NULL;
	m_pAVIFile = NULL;
	m_pGetFrame = NULL;
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

	HRESULT hr = AVIFileOpen( &m_pAVIFile, pFullAVIFileName, OF_READ, NULL );
	if ( hr != AVIERR_OK ) 
	{
		Warning( "AVI '%s' not found\n", pFullAVIFileName );
		m_nAVIWidth = 64;
		m_nAVIHeight = 64;
		m_nFrameRate = 1;
		m_nFrameCount = 1;
		m_Material.Init( "debug/debugempty", TEXTURE_GROUP_OTHER );
		return false;
	}

	// Get AVI size
	AVIFILEINFO info;
	AVIFileInfo( m_pAVIFile, &info, sizeof(info) );
	m_nAVIWidth = info.dwWidth;
	m_nAVIHeight = info.dwHeight;
	m_nFrameRate = (int)( (float)info.dwRate / (float)info.dwScale + 0.5f );

	CreateProceduralTexture( pMaterialName );
	CreateProceduralMaterial( pMaterialName );
	CreateVideoStream();

	// Get frame count
	m_nFrameCount = MAX( 0, AVIStreamLength( m_pAVIStream ) );

	m_Texture->Download();

	return true;
}

void CAVIMaterial::Shutdown()
{
	DestroyVideoStream();
	DestroyProceduralMaterial( );
	DestroyProceduralTexture( );
	if ( m_pAVIFile )
	{
		AVIFileRelease( m_pAVIFile );
		m_pAVIFile = NULL;
	}
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
	if ( nTextureWidth )
		*pMaxU = (float)m_nAVIWidth / (float)nTextureWidth;
	else
		*pMaxU = 0.0f;

	if ( nTextureHeight )
		*pMaxV = (float)m_nAVIHeight / (float)nTextureHeight;
	else
		*pMaxV = 0.0f;
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
	if ( m_pAVIStream )
	{
		// Round to the nearest frame
		// FIXME: Strangely, AVIStreamTimeToSample gets off by several frames if you're a ways down the stream
//		int nCurrentSample = AVIStreamTimeToSample( m_pAVIStream, ( flTime + 0.5f / m_nFrameRate )* 1000.0f );
		int nCurrentSample = (int)( flTime * m_nFrameRate + 0.5f );
		if ( m_nCurrentSample != nCurrentSample )
		{
			m_nCurrentSample = nCurrentSample;
			m_Texture->Download();
		}
	}
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
	if ( m_pAVIStream )
	{
		int nCurrentSample = (int)( flFrame + 0.5f );
		if ( m_nCurrentSample != nCurrentSample )
		{
			m_nCurrentSample = nCurrentSample;
			m_Texture->Download();
		}
	}
}


//-----------------------------------------------------------------------------
// Initializes, shuts down the video stream
//-----------------------------------------------------------------------------
void CAVIMaterial::CreateVideoStream( )
{
	HRESULT hr = AVIFileGetStream( m_pAVIFile, &m_pAVIStream, streamtypeVIDEO, 0 );
	if ( hr != AVIERR_OK )
	{ 
		ReportError( hr );
		return;
	}

	m_nCurrentSample = AVIStreamStart( m_pAVIStream );

	// Create a compatible DC
	HDC hdcscreen = GetDC( GetDesktopWindow() );
	m_memdc = CreateCompatibleDC( hdcscreen ); 
	ReleaseDC( GetDesktopWindow(), hdcscreen );

	// Set up a DIBSection for the screen
	m_bih					= &m_bi.bmiHeader;
	m_bih->biSize			= sizeof( *m_bih );
	m_bih->biWidth			= m_nAVIWidth;
	m_bih->biHeight			= m_nAVIHeight;
	m_bih->biPlanes			= 1;
	m_bih->biBitCount		= 32;
	m_bih->biCompression	= BI_RGB;
	m_bih->biSizeImage		= ( ( m_bih->biWidth * m_bih->biBitCount / 8 + 3 )& 0xFFFFFFFC ) * m_bih->biHeight;
	m_bih->biXPelsPerMeter	= 10000;
	m_bih->biYPelsPerMeter	= 10000;
	m_bih->biClrUsed		= 0;
	m_bih->biClrImportant	= 0;
	
	// Create the DIBSection
	void *bits;
	m_DIBSection = CreateDIBSection( m_memdc, ( BITMAPINFO *)m_bih, DIB_RGB_COLORS, &bits, NULL, NULL );

	// Get at the DIBSection object
    DIBSECTION dibs; 
	GetObject( m_DIBSection, sizeof( dibs ), &dibs );

	m_pGetFrame = AVIStreamGetFrameOpen( m_pAVIStream, &dibs.dsBmih );
}

void CAVIMaterial::DestroyVideoStream( )
{
	if ( m_pGetFrame )
	{
		AVIStreamGetFrameClose( m_pGetFrame );
		m_pGetFrame = NULL;
	}

	if ( m_DIBSection != 0 )
	{
		DeleteObject( m_DIBSection );
		m_DIBSection = (HBITMAP)0;
	}

	if ( m_memdc != 0 )
	{
		// Release the compatible DC
		DeleteDC( m_memdc );
		m_memdc = (HDC)0;
	}

	if ( m_pAVIStream )
	{
		AVIStreamRelease( m_pAVIStream );
		m_pAVIStream = NULL;
	}
}

	
//-----------------------------------------------------------------------------
// Inherited from ITextureRegenerator
//-----------------------------------------------------------------------------
void CAVIMaterial::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
{
	CPixelWriter pixelWriter;
	LPBITMAPINFOHEADER lpbih;
	unsigned char *pData;
	int i, y, nIncY;

	// Error condition
	if ( !m_pAVIStream || !m_pGetFrame || (pVTFTexture->FrameCount() > 1) || 
		(pVTFTexture->FaceCount() > 1) || (pVTFTexture->MipCount() > 1) || (pVTFTexture->Depth() > 1) )
	{
		goto AVIMaterialError;
	}

	lpbih = (LPBITMAPINFOHEADER)AVIStreamGetFrame( m_pGetFrame, m_nCurrentSample );
	if ( !lpbih )
		goto AVIMaterialError;

	// Set up the pixel writer to write into the VTF texture
	pixelWriter.SetPixelMemory( pVTFTexture->Format(), 
		pVTFTexture->ImageData( ), pVTFTexture->RowSizeInBytes( 0 ) );
	
	int nWidth = pVTFTexture->Width();
	int nHeight = pVTFTexture->Height();
	int nBihHeight = abs( lpbih->biHeight );
	if ( lpbih->biWidth > nWidth || nBihHeight > nHeight )
		goto AVIMaterialError;

	pData = (unsigned char *)lpbih + lpbih->biSize;
	if ( lpbih->biBitCount == 8 )
	{
		// This is the palette
		pData += 256 * sizeof(RGBQUAD);
	}

	if ( (( lpbih->biBitCount == 16 ) || ( lpbih->biBitCount == 32 )) && ( lpbih->biCompression == BI_BITFIELDS ) )
	{
		pData += 3 * sizeof(DWORD);

		// MASKS NOT IMPLEMENTED YET
		Assert( 0 );
	}

	int nStride = ( lpbih->biWidth * lpbih->biBitCount / 8 + 3 ) & 0xFFFFFFFC;
	if ( lpbih->biHeight > 0 )
	{
		y = nBihHeight - 1;
		nIncY = -1;
	}
	else
	{
		y = 0;
		nIncY = 1;
	}

	if ( lpbih->biBitCount == 24)
	{
		for ( i = 0; i < nBihHeight; ++i, pData += nStride, y += nIncY )
		{
			pixelWriter.Seek( 0, y );
			BGR888_t *pAVIPixel = (BGR888_t*)pData;
			for (int x = 0; x < lpbih->biWidth; ++x, ++pAVIPixel)
			{
				pixelWriter.WritePixel( pAVIPixel->r, pAVIPixel->g, pAVIPixel->b, 255 );
			}
		}
	}
	else if (lpbih->biBitCount == 32)
	{
		for ( i = 0; i < nBihHeight; ++i, pData += nStride, y += nIncY )
		{
			pixelWriter.Seek( 0, y );
			BGRA8888_t *pAVIPixel = (BGRA8888_t*)pData;
			for (int x = 0; x < lpbih->biWidth; ++x, ++pAVIPixel)
			{
				pixelWriter.WritePixel( pAVIPixel->r, pAVIPixel->g, pAVIPixel->b, pAVIPixel->a );
			}
		}
	}
	return;

AVIMaterialError:
	int nBytes = pVTFTexture->ComputeTotalSize();
	memset( pVTFTexture->ImageData(), 0xFF, nBytes );
	return;
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
	HWND			m_hWnd;
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
	m_hWnd = NULL;
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

	AVIFileInit();
	return INIT_OK;
}

void CAvi::Shutdown()
{
	AVIFileExit();
	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Sets the main window
//-----------------------------------------------------------------------------
void CAvi::SetMainWindow( void* hWnd )
{
	m_hWnd = (HWND)hWnd;
}


//-----------------------------------------------------------------------------
// Start, finish recording an AVI
//-----------------------------------------------------------------------------
AVIHandle_t CAvi::StartAVI( const AVIParams_t& params )
{
	AVIHandle_t h = m_AVIFiles.AddToTail();
	m_AVIFiles[h].Init( params, m_hWnd );
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
