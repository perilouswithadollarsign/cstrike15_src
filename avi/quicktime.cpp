//====== Copyright 2010, Valve Corporation, All rights reserved. ==============
//
// Purpose: 
//
//=============================================================================

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
#include "platform.h"
#include "avi/iquicktime.h"
#include "quicktime.h"

#if defined ( WIN32 )
 #if defined ( QUICKTIME_VIDEO )
	#include <WinDef.h>
	#include <../dx9sdk/include/dsound.h>
 #endif
#endif

#include "tier0/memdbgon.h"


#define ZeroVar( var )  V_memset( &var, 0, sizeof( var) )
#define SAFE_DELETE( var )  if ( var != NULL ) { delete var; var = NULL; }
#define SAFE_DELETE_ARRAY( var )  if ( var != NULL ) { delete[] var; var = NULL; }


#ifdef DBGFLAG_ASSERT

#define AssertExit( _exp )		Assert( _exp )
#define AssertExitF( _exp )		Assert( _exp )

#else

#define AssertExit( _exp )		if ( !( _exp ) ) return;
#define AssertExitF( _exp )		if ( !( _exp ) ) return false;

#endif





//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CQuickTime g_QUICKTIME;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CQuickTime, IQuickTime, QUICKTIME_INTERFACE_VERSION, g_QUICKTIME );



//-----------------------------------------------------------------------------
// Inherited from ITextureRegenerator
//-----------------------------------------------------------------------------
void CQuicktimeMaterialRGBTextureRegenerator::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
{
	AssertExit( pVTFTexture != NULL );
	
	// Error condition
	if ( ( pVTFTexture->FrameCount() > 1 ) || ( pVTFTexture->FaceCount() > 1 ) || ( pVTFTexture->MipCount() > 1 ) || ( pVTFTexture->Depth() > 1 ) )
	{
		memset( pVTFTexture->ImageData(), 0xAA, pVTFTexture->ComputeTotalSize() );
		return;
	}

	// do we not have a video to install, or a video buffer that is too big?
	if ( m_pQTMaterial->m_BitMapData == NULL /* || m_VideoFrameBufferSize < pVTFTexture->ComputeMipSize( 0 ) */ )
	{
		memset( pVTFTexture->ImageData(), 0xCC, pVTFTexture->ComputeTotalSize() );
		return;
	}

	// Need to verify we have compatible formats
	Assert( pVTFTexture->Format() == IMAGE_FORMAT_BGRA8888 );
	Assert( pVTFTexture->RowSizeInBytes( 0 ) >= pVTFTexture->Width() * 4 );
	Assert( pVTFTexture->Width() >= m_nSourceWidth );
	Assert( pVTFTexture->Height() >= m_nSourceHeight );

	// simplest of image copies, one line at a time
	BYTE   *ImageData = pVTFTexture->ImageData();
	BYTE   *SrcData   = (BYTE*) m_pQTMaterial->m_BitMapData;
	int		dstStride = pVTFTexture->RowSizeInBytes( 0 );
	int		srcStride = m_nSourceWidth * 4;
	int		rowSize   = m_nSourceWidth * 4;
	
	// copy the rows of data
	for ( int y = 0; y < m_nSourceHeight; y++ )
	{
		memcpy( ImageData, SrcData, rowSize);
		ImageData+= dstStride;
		SrcData+= srcStride;
	}

}

void CQuicktimeMaterialRGBTextureRegenerator::Release()
{
	// we don't invoke the destructor here, we're not using the no-release extensions
}



//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CQuickTimeMaterial::CQuickTimeMaterial() :
	m_pFileName( NULL )
#if defined ( QUICKTIME_VIDEO )	
	,m_MovieGWorld( NULL ),
	m_QTMovie( NULL ),
	m_AudioContext( NULL ),
	m_BitMapData( NULL )
#endif	
{
	Reset();
}


//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CQuickTimeMaterial::~CQuickTimeMaterial()
{
	Reset();
}


void CQuickTimeMaterial::SetQTFileName( const char* theQTMovieFileName )
{
	SAFE_DELETE_ARRAY( m_pFileName );
	
	if ( theQTMovieFileName != NULL )
	{
		int sLen = V_strlen( theQTMovieFileName );
		AssertMsg( sLen > 0 && sLen <= cMaxQTFileNameLen, "Bad Movie FileName" );
		m_pFileName = new char[ sLen + 1 ];
		V_strcpy( m_pFileName, theQTMovieFileName );
	}
}



void CQuickTimeMaterial::Reset()
{
	SetQTFileName( NULL );

	ZeroVar( m_TextureName );
	ZeroVar( m_MaterialName );
	
	DestroyProceduralTexture();
	DestroyProceduralMaterial();

	SAFE_DELETE_ARRAY( m_BitMapData );
	
	m_TexCordU = 0.0f;
	m_TexCordV = 0.0f;

	m_bActive = false;
	m_bLoopMovie = false;

	m_bMoviePlaying = false;
	m_MovieBeganPlayingTime = 0.0;
	m_MovieCurrentTime = 0.0;
	

#if defined ( QUICKTIME_VIDEO )	

	if ( m_AudioContext != NULL )
	{
		QTAudioContextRelease( m_AudioContext );
		m_AudioContext = NULL;
	}

	if ( m_MovieGWorld != NULL )
	{
		DisposeGWorld( m_MovieGWorld );
		m_MovieGWorld = NULL;
	}
 
	if ( m_QTMovie != NULL )
	{
		DisposeMovie( m_QTMovie );
		m_QTMovie = NULL;
	}
#endif
}


//-----------------------------------------------------------------------------
// Initializes the material
//-----------------------------------------------------------------------------
bool CQuickTimeMaterial::Init( const char *pMaterialName, const char *pFileName, const char *pPathID )
{
	// Determine the full path name of the video file
	char pQTFileName[ MAX_PATH ];
	char pFullQTFileName[ MAX_PATH ];
	Q_snprintf( pQTFileName, sizeof( pQTFileName ), "%s", pFileName );
	V_SetExtension( pQTFileName, ".mov", sizeof( pQTFileName ) );
	if ( !g_pFullFileSystem->RelativePathToFullPath( pQTFileName, pPathID, pFullQTFileName, sizeof( pFullQTFileName ) ) )
	{
		// A file by that name was not found
		Assert( 0 );
		return false;
	}

	OpenQTMovie( pFullQTFileName );
	if ( !m_bActive )
	{
		// The file was unable to be opened
		Assert( 0 );
		return false;
	}

	// Now we can properly setup out regenerators
	m_TextureRegen.SetParentMaterial( this, m_VideoFrameWidth, m_VideoFrameHeight);

	CreateProceduralTexture( pMaterialName );
	CreateProceduralMaterial( pMaterialName );

	return true;
}


void CQuickTimeMaterial::Shutdown( void )
{
	CloseQTFile();

	DestroyProceduralMaterial();
	DestroyProceduralTexture();
	
	Reset();

}


//-----------------------------------------------------------------------------
// Purpose: Updates our scene
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CQuickTimeMaterial::Update( void )
{
	Assert( m_bActive );
#if defined ( QUICKTIME_VIDEO )	
	
	OSType	qTypes[1] = { VisualMediaCharacteristic };
	
	// is this our first frame?
	if ( !m_bMoviePlaying )
	{
		TimeValue startTime = -1;
		short   qFlags = nextTimeMediaSample + nextTimeEdgeOK;
		
		GetMovieNextInterestingTime( m_QTMovie, qFlags, 1, qTypes, (TimeValue) 0, fixed1, &startTime, NULL);
		Assert( GetMoviesError() == noErr );

		GetMovieNextInterestingTime( m_QTMovie, nextTimeMediaSample, 1, qTypes, startTime, fixed1, &m_NextInterestingTimeToPlay, NULL);
		Assert( GetMoviesError() == noErr );

		SetMovieTimeValue( m_QTMovie, startTime );
		Assert( GetMoviesError() == noErr );
	
		m_LastInterestingTimePlayed = startTime;
	
		m_MovieBeganPlayingTime = Plat_FloatTime();
		
		m_bMoviePlaying = true;
	}
	else	// we've drawn at least one frame before
	{
		// Get Current Time.. are we done playing the movie?
		double rightNow = Plat_FloatTime();
		m_MovieCurrentTime = rightNow - m_MovieBeganPlayingTime;

		// did we hit the end of the movie?
		if ( m_MovieCurrentTime >= m_QTMovieDurationinSec || m_NextInterestingTimeToPlay == -2 )
		{
			// If we're not looping, then report that we are done updating
			if ( m_bLoopMovie == false )
			{
				return false;
			}
			// ok we're looping the movie, so....
			
			// wrap around the current time
			while ( m_MovieCurrentTime >= m_QTMovieDurationinSec )
			{
				m_MovieBeganPlayingTime+= m_QTMovieDurationinSec;
				m_MovieCurrentTime = rightNow - m_MovieBeganPlayingTime;
			}

			// the next frame is set the frame 0, so it should trigger wrapping to the beginning
			long currentMovieTime = ( long ) ( m_MovieCurrentTime * m_QTMovieTimeScale );
			m_NextInterestingTimeToPlay = 0;
			
			// Reset the movie to the wrapped around time (probably should compute starttime instead of assuming 0)
			SetMovieTimeValue( m_QTMovie, currentMovieTime );
			Assert( GetMoviesError() == noErr );

		}
		
		// where are we in terms of QT media units?
		long currentMovieTime = ( long ) ( m_MovieCurrentTime * m_QTMovieTimeScale );
		
		// Enough time passed to get to next frame
		if ( currentMovieTime < m_NextInterestingTimeToPlay )
		{
			// nope.. use the previous frame
			return true;
		}
		
		TimeValue nextTimeAfter = -1;
		
		// do we need to skip any frames?
		while ( true )
		{
			// look at the sample time after the one we past
			GetMovieNextInterestingTime( m_QTMovie, nextTimeMediaSample, 1, qTypes, m_NextInterestingTimeToPlay, fixed1, &nextTimeAfter, NULL);
			OSErr lastErr = GetMoviesError();
			// hit the end of the movie?
			if ( lastErr == invalidTime )
			{
				nextTimeAfter = -2;
				break;
			}
			Assert( lastErr == noErr );
			
			// is there a later frame we should be showing?
			if ( nextTimeAfter <= currentMovieTime)		
			{
				m_NextInterestingTimeToPlay = nextTimeAfter;
				nextTimeAfter = -1;
			}
			else
			{
				break;
			}
		}
		
		// SetMovieTimeValue( m_QTMovie, m_NextInterestingTimeToPlay );
		Assert( GetMoviesError() == noErr );
	
		m_LastInterestingTimePlayed = m_NextInterestingTimeToPlay;
		m_NextInterestingTimeToPlay = nextTimeAfter;
			
	}	

	// move the movie along
	UpdateMovie( m_QTMovie );
	Assert( GetMoviesError() == noErr );

	MoviesTask( m_QTMovie, 10L );
	Assert( GetMoviesError() == noErr );

  #if defined (WIN32)

	HDC theHDC = (HDC) GetPortHDC( (GrafPtr) m_MovieGWorld );
	HBITMAP theHBITMAP = (HBITMAP) GetPortHBITMAP( (GrafPtr) m_MovieGWorld );

	// create the bitmapinfo header information
	BITMAP bmp; 
	if ( !GetObject( theHBITMAP, sizeof(BITMAP), (LPSTR)&bmp) )
	{
		Assert( false );
		return false;
	}

	Assert( bmp.bmWidth == m_QTMovieRect.right );
	Assert( bmp.bmBitsPixel == 32 );

	BITMAPINFO		tempInfo;
	V_memcpy( &tempInfo, &m_BitmapInfo, sizeof( tempInfo ) );

	// Retrieve the pixel bits (no color table) 
	if ( !GetDIBits( theHDC, theHBITMAP, 0, (WORD) bmp.bmHeight, m_BitMapData, &tempInfo, DIB_RGB_COLORS)) 
	{
		AssertMsg( false, "writeBMP::GetDIB error" );
		return false;
	}
	
  #elif defined ( OSX )	

	PixMapHandle thePixMap = GetGWorldPixMap( m_MovieGWorld );
	if ( LockPixels( thePixMap ) )
	{
		void *pPixels = GetPixBaseAddr( thePixMap );
		long rowStride = GetPixRowBytes( thePixMap );
		int rowBytes = m_VideoFrameWidth * 4;
		
		for (int y = 0; y < m_VideoFrameHeight; y++ )
		{
			BYTE *src = (BYTE*) pPixels + ( y * rowStride );
			BYTE *dst = (BYTE*) m_BitMapData + ( y * rowBytes );
			memcpy( dst, src, rowBytes );
		}
		
		UnlockPixels( thePixMap );
	}
	
  #endif

	// Regenerate our texture
	m_Texture->Download();

#endif

	return true;
}

//-----------------------------------------------------------------------------
// Checks to see if the video has a new frame ready to download into the
// texture
//-----------------------------------------------------------------------------
bool CQuickTimeMaterial::ReadyForSwap( void )
{
	AssertExitF( m_bActive );

	// Waiting to play the first frame?  Hell yes we are ready
	if ( !m_bMoviePlaying )	return true;

	// Get Current Time.. are we done playing the movie?
	double CurrentTime = Plat_FloatTime() - m_MovieBeganPlayingTime;

	// did we hit the end of the movie?
	if ( CurrentTime >= m_QTMovieDurationinSec || m_NextInterestingTimeToPlay == -2 )
	{
		// if we are looping, we have another frame, otherwise no
		return m_bLoopMovie;
	}
		
	// where are we in terms of QT media units?
	long currentMovieTime = ( long ) ( CurrentTime * m_QTMovieTimeScale );
		
	// Enough time passed to get to next frame??
	if ( currentMovieTime < m_NextInterestingTimeToPlay )
	{
		// nope.. use the previous frame
		return false;
	}

	// we have a new frame we want then..
	return true;


}


//-----------------------------------------------------------------------------
// Returns the material
//-----------------------------------------------------------------------------
IMaterial *CQuickTimeMaterial::GetMaterial()
{
	return m_Material;
}							   



//-----------------------------------------------------------------------------
// Returns the texcoord range
//-----------------------------------------------------------------------------
void CQuickTimeMaterial::GetTexCoordRange( float *pMaxU, float *pMaxV )
{
	// no texture?
	if ( m_Texture == NULL )
	{
		*pMaxU = *pMaxV = 1.0f;
		return;
	}

	int nTextureWidth = m_Texture->GetActualWidth();
	int nTextureHeight = m_Texture->GetActualHeight();
	*pMaxU = (float) m_VideoFrameWidth  / (float) nTextureWidth;
	*pMaxV = (float) m_VideoFrameHeight / (float) nTextureHeight;
}

//-----------------------------------------------------------------------------
// Returns the frame size of the QuickTime Video (stored in a subrect of the material itself)
//-----------------------------------------------------------------------------
void CQuickTimeMaterial::GetFrameSize( int *pWidth, int *pHeight )
{
	*pWidth  = m_VideoFrameWidth;
	*pHeight = m_VideoFrameHeight;
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
// Returns the frame rate of the Quicktime Video
//-----------------------------------------------------------------------------
int CQuickTimeMaterial::GetFrameRate( )
{
#if defined ( QUICKTIME_VIDEO )	
	return m_QTMoveFrameRate;
#else
	return 1;
#endif
}

int CQuickTimeMaterial::GetFrameCount( )
{
#if defined ( QUICKTIME_VIDEO )	
	return (int) ( m_QTMovieDurationinSec * m_QTMoveFrameRate );
#else
	return 1;
#endif
}


//-----------------------------------------------------------------------------
// Sets the frame for an QuickTime  Material (use instead of SetTime)
//-----------------------------------------------------------------------------
void CQuickTimeMaterial::SetFrame( float flFrame )
{
	flFrame;
	AssertMsg( false, "method not implemented " );
}


//-----------------------------------------------------------------------------
// Sets the movie to loop continously instead of end, or not
//-----------------------------------------------------------------------------
void CQuickTimeMaterial::SetLooping( bool loop )
{
	m_bLoopMovie = loop;
}


//-----------------------------------------------------------------------------
// Initializes, shuts down the procedural texture
//-----------------------------------------------------------------------------
void CQuickTimeMaterial::CreateProceduralTexture( const char *pTextureName )
{

	Assert( m_VideoFrameWidth >= cMinVideoFrameWidth && m_VideoFrameHeight >= cMinVideoFrameHeight &&
			m_VideoFrameWidth <= cMaxVideoFrameWidth && m_VideoFrameHeight <= cMaxVideoFrameHeight);
	Assert( pTextureName );

	// Choose power-of-two textures which are at least as big as the AVI
	int nWidth  = ComputeGreaterPowerOfTwo( m_VideoFrameWidth ); 
	int nHeight = ComputeGreaterPowerOfTwo( m_VideoFrameHeight ); 
	
	// initialize the procedural texture as 32-it RGBA, w/o mipmaps
	m_Texture.InitProceduralTexture( pTextureName, "VideoCacheTextures", nWidth, nHeight, 
		IMAGE_FORMAT_BGRA8888, TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP |
		TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_NOLOD );
		
	// Use this to get the updated frame from the remote connection		
	m_Texture->SetTextureRegenerator( &m_TextureRegen /* , false */ );
	
	// compute the texcoords
	int nTextureWidth = m_Texture->GetActualWidth();
	int nTextureHeight = m_Texture->GetActualHeight();
	
	m_TexCordU = ( nTextureWidth > 0 ) ? (float) m_VideoFrameWidth / (float) nTextureWidth : 0.0f;
	m_TexCordV = ( nTextureHeight > 0 ) ? (float) m_VideoFrameHeight / (float) nTextureHeight : 0.0f;


}

void CQuickTimeMaterial::DestroyProceduralTexture()
{
	if ( m_Texture != NULL )
	{
		// DO NOT Call release on the Texture Regenerator, as it will destroy this object!  bad bad bad
		// instead we tell it to assign a NULL regenerator and flag it to not call release
		m_Texture->SetTextureRegenerator( NULL /*, false */ );
		// Texture, texture go away...
		m_Texture.Shutdown( true );
	}

}



//-----------------------------------------------------------------------------
// Initializes, shuts down the procedural material
//-----------------------------------------------------------------------------
void CQuickTimeMaterial::CreateProceduralMaterial( const char *pMaterialName )
{
	// create keyvalues if necessary
	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	{
		pVMTKeyValues->SetString( "$basetexture", m_Texture->GetName() );
		pVMTKeyValues->SetInt( "$nofog", 1 );
		pVMTKeyValues->SetInt( "$spriteorientation", 3 );
		pVMTKeyValues->SetInt( "$translucent", 1 );
		pVMTKeyValues->SetInt( "$nolod", 1 );
		pVMTKeyValues->SetInt( "$nomip", 1 );
		pVMTKeyValues->SetInt( "$gammacolorread", 0 );
	}

	// FIXME: gak, this is backwards.  Why doesn't the material just see that it has a funky basetexture?
	m_Material.Init( pMaterialName, pVMTKeyValues );
	m_Material->Refresh();
}

void CQuickTimeMaterial::DestroyProceduralMaterial()
{
	// Store the internal material pointer for later use
	IMaterial *pMaterial = m_Material;
	m_Material.Shutdown();
	materials->UncacheUnusedMaterials();

	// Now be sure to free that material because we don't want to reference it again later, we'll recreate it!
	if ( pMaterial != NULL )
	{
		pMaterial->DeleteIfUnreferenced();
	}
}



//-----------------------------------------------------------------------------
// Opens a movie file using quicktime
//-----------------------------------------------------------------------------
void CQuickTimeMaterial::OpenQTMovie( const char* theQTMovieFileName )
{

	AssertExit( theQTMovieFileName != NULL );

#if defined ( QUICKTIME_VIDEO )	
    short	theFile = 0; 
    FSSpec	sfFile; 
    char	fullPath[256];
	OSErr	status = 0;
     
    // Set graphics port 
  #if defined ( WIN32 )
	SetGWorld ( (CGrafPtr) GetNativeWindowPort( nil ), nil ); 
  #elif defined ( OSX		)
	SetGWorld( nil, nil );
  #endif
	
	SetQTFileName( theQTMovieFileName );

  #if defined ( OSX )

	FSRef dirRef;
	Boolean isDir;

	status = FSPathMakeRef( (UInt8 *)theQTMovieFileName, &dirRef, &isDir );
	if ( status == noErr )
	{
		status = FSGetCatalogInfo( &dirRef, kFSCatInfoNone, NULL, NULL, &sfFile, NULL );
		Assert( status == noErr );
	}
	
	if ( status != noErr )
	{
		Reset();
		return;
	}
		
  #elif defined ( WIN32 ) 
    strcpy ( fullPath, theQTMovieFileName);         // Copy full pathname  
    c2pstr ( fullPath );                            // Convert to Pascal string  
	
    status = FSMakeFSSpec( 0, 0L, (const unsigned char *)&fullPath[0], &sfFile );      // Make file-system specification record
    AssertExit( status == noErr );

  #endif 
    status = OpenMovieFile( &sfFile, &theFile, fsRdPerm) ;  // Open movie file 
    Assert( status == noErr );
    if ( status != noErr )
    {
		CloseMovieFile( theFile );
		Reset();
		return;
    }
    
    status = NewMovieFromFile ( &m_QTMovie, theFile, nil, nil, newMovieActive, nil);   // Get movie from file 
    Assert( status == noErr );
    if ( status != noErr )
    {
		CloseMovieFile( theFile );
		Reset();
		return;
    }
     
    status = CloseMovieFile (theFile);                     // Close movie file  
    AssertExit( status == noErr );

	// Now we need to extract the time info from the QT Movie 
	// Duration scale = 600 per second...
	m_QTMovieTimeScale = GetMovieTimeScale( m_QTMovie );
	m_QTMovieDuration = GetMovieDuration( m_QTMovie );
	m_QTMovieDurationinSec = float ( m_QTMovieDuration ) / float ( m_QTMovieTimeScale );

	Fixed movieRate = GetMoviePreferredRate( m_QTMovie );
	m_QTMoveFrameRate = Fix2Long( movieRate );

	// what size do we set the output rect to?
	GetMovieNaturalBoundsRect(m_QTMovie, &m_QTMovieRect);
	
	m_VideoFrameWidth = m_QTMovieRect.right;
	m_VideoFrameHeight = m_QTMovieRect.bottom;
	
	// Sanity check...
	AssertExit( m_QTMovieRect.top == 0 && m_QTMovieRect.left == 0 && m_QTMovieRect.right >= 64 && m_QTMovieRect.right <= 1920 && 
	    		m_QTMovieRect.bottom >= 48 && m_QTMovieRect.bottom <= 1200 && m_QTMovieRect.right % 4 == 0 );

	// Setup a bitmap to store frames...
	
	// compute image buffer size	
	m_BitMapDataSize = 4 *  m_QTMovieRect.right * m_QTMovieRect.bottom;


  #if defined ( WIN32 )	
	// Initialize bitmap info
	ZeroVar( m_BitmapInfo );
	m_BitmapInfo.bmiHeader.biSize = sizeof( m_BitmapInfo.bmiHeader );
	m_BitmapInfo.bmiHeader.biWidth = (LONG) m_QTMovieRect.right;
	m_BitmapInfo.bmiHeader.biHeight = -1 * (LONG) m_QTMovieRect.bottom;
	m_BitmapInfo.bmiHeader.biPlanes = 1;
	m_BitmapInfo.bmiHeader.biBitCount = 32;
	m_BitmapInfo.bmiHeader.biCompression = 0; /* BI_RGB */
	m_BitmapInfo.bmiHeader.biSizeImage = m_BitMapDataSize; 
	// the rest of the fields should be 0

  #endif

	// create buffer to hold a single frame
	m_BitMapData = new byte[ m_BitMapDataSize ];
	
	// Setup the QuiuckTime Graphics World for the Movie
	status = QTNewGWorld( &m_MovieGWorld, k32BGRAPixelFormat, &m_QTMovieRect, nil, nil, 0 );
	AssertExit( status == noErr );
	
	// perform any needed gamma correction
	//   kQTUsePlatformDefaultGammaLevel = 0,  /* When decompressing into this PixMap, gamma-correct to the platform's standard gamma. */
	//   kQTUseSourceGammaLevel        = -1L,  /* When decompressing into this PixMap, don't perform gamma-correction. */
	//   kQTCCIR601VideoGammaLevel     = 0x00023333 /* 2.2, standard television video gamma.*/
	//   Fixed cGamma1_8 = 0x0001CCCC;		// Gamma 1.8
	//   Fixed cGamma2_5 = 0x00028000;      // Gamma 2.5
	//
	//  On OSX it appears we need to set a gamma of 1.0 or 0x0001000 - the values are interpreted differently?

  #if defined ( OSX )	
	Fixed decodeGamma = 0x00012000;
  #elif defined ( WIN32 )
	Fixed decodeGamma = 0x00023333;
  #endif

	// Get the pix map for the GWorld and adjust the gamma correction on it
	PixMapHandle thePixMap = GetGWorldPixMap( m_MovieGWorld );

	OSErr Status = QTSetPixMapHandleGammaLevel( thePixMap, decodeGamma );
	AssertExit( Status == noErr );

	Status = QTSetPixMapHandleRequestedGammaLevel( thePixMap, decodeGamma );
	AssertExit( Status == noErr );

	SetMovieGWorld( m_QTMovie, m_MovieGWorld, nil );		


  #if defined ( WIN32 )

	WCHAR strGUID[39];
	int numBytes = StringFromGUID2( DSDEVID_DefaultPlayback, (LPOLESTR) strGUID, 39);			// CLSID_DirectSound is not what you want here

	// create the audio context
	CFStringRef deviceNameStrRef = NULL;
	deviceNameStrRef = CFStringCreateWithCharacters(kCFAllocatorDefault, 
                                                    (const UniChar*) strGUID, 
                                                    (CFIndex) (numBytes -1) );

    OSStatus result = QTAudioContextCreateForAudioDevice( NULL, deviceNameStrRef, NULL, &m_AudioContext );
  #elif defined ( OSX )
	
    OSStatus result = QTAudioContextCreateForAudioDevice( NULL, NULL, NULL, &m_AudioContext );
	
  #endif
	
	AssertExit( result == noErr );

	// Set the audio context

    result = SetMovieAudioContext( m_QTMovie, m_AudioContext );
    AssertExit( result == noErr );

	// Set the volume
	ConVarRef volumeConVar( "volume" );
	float sysVolume = 1.0f;
	if ( volumeConVar.IsValid() )
		sysVolume = volumeConVar.GetFloat();
	clamp( sysVolume, 0.0f, 1.0f);
	
	short  movieVolume = (short) ( sysVolume * 256.0 );
	
	SetMovieVolume( m_QTMovie, movieVolume );

	// Start movie playback (get the sound rolling)
    
	StartMovie( m_QTMovie );

	m_bActive = true;

  #if defined( WIN32 )
	if ( deviceNameStrRef )
	{
		CFRelease( deviceNameStrRef );
	}
  #endif
	
#endif	

}


void CQuickTimeMaterial::CloseQTFile()
{
#if defined ( QUICKTIME_VIDEO )	

	StopMovie( m_QTMovie );
	
	SAFE_DELETE_ARRAY( m_BitMapData );
	
	if ( m_AudioContext != NULL )
	{
		QTAudioContextRelease( m_AudioContext );
		m_AudioContext = NULL;
	}

	if ( m_MovieGWorld != NULL )
	{
		DisposeGWorld( m_MovieGWorld );
		m_MovieGWorld = NULL;
	}
 
	if ( m_QTMovie )
	{
		DisposeMovie( m_QTMovie );
		m_QTMovie = NULL;
	
	}

	SetQTFileName( NULL );
#endif
}



//-----------------------------------------------------------------------------
// Constructor/destructor
//-----------------------------------------------------------------------------
CQuickTime::CQuickTime()
{
	m_bQTInitialized = false;
}

// ----------------------------------------------------------------------------
CQuickTime::~CQuickTime()
{
	// Make sure we shut down quicktime
	ShutdownQuicktime();
}

//-----------------------------------------------------------------------------
// Connect/disconnect
//-----------------------------------------------------------------------------
bool CQuickTime::Connect( CreateInterfaceFn factory )
{
	ConnectTier1Libraries( &factory, 1 );
	ConnectTier2Libraries( &factory, 1 );

	if ( !(  g_pFullFileSystem && materials ) )
	{
		Msg( "Quicktime failed to connect to a required system\n" );
	}
	return ( g_pFullFileSystem && materials );
}

//-----------------------------------------------------------------------------
// Connect/disconnect
//-----------------------------------------------------------------------------
void CQuickTime::Disconnect( void )
{
	// Make sure we shut down quicktime
	ShutdownQuicktime();
}


//-----------------------------------------------------------------------------
// Query Interface
//-----------------------------------------------------------------------------
void *CQuickTime::QueryInterface( const char *pInterfaceName )
{
	if ( Q_strncmp(	pInterfaceName, QUICKTIME_INTERFACE_VERSION, Q_strlen(QUICKTIME_INTERFACE_VERSION) + 1) == 0 )
	{
		return (IQuickTime*)this;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Init/shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CQuickTime::Init()
{	
	return SetupQuicktime() ? INIT_OK : INIT_FAILED;
}

//-----------------------------------------------------------------------------
void CQuickTime::Shutdown()
{
	ShutdownQuicktime();
}


//-----------------------------------------------------------------------------
// Create/destroy an QuickTime material
//-----------------------------------------------------------------------------
QUICKTIMEMaterial_t CQuickTime::CreateMaterial( const char *pMaterialName, const char *pFileName, const char *pPathID, int flags )
{
	if ( ! m_bQTInitialized )
	{
		return QUICKTIMEMATERIAL_INVALID;
	}

	QUICKTIMEMaterial_t h = m_QTMaterials.AddToTail();
	m_QTMaterials[h] = new CQuickTimeMaterial;
	if ( m_QTMaterials[h]->Init( pMaterialName, pFileName, pPathID ) == false )
	{
		delete m_QTMaterials[h];
		m_QTMaterials.Remove( h );
		return QUICKTIMEMATERIAL_INVALID;
	}
	
	m_QTMaterials[h]->SetLooping( ( flags & QUICKTIME_LOOP_MOVIE ) == QUICKTIME_LOOP_MOVIE );

	return h;
}

// ----------------------------------------------------------------------------
void CQuickTime::DestroyMaterial( QUICKTIMEMaterial_t h )
{
	if ( h != QUICKTIMEMATERIAL_INVALID )
	{
		m_QTMaterials[h]->Shutdown();
		delete m_QTMaterials[h];
		m_QTMaterials.Remove( h );
	}
}


//-----------------------------------------------------------------------------
// Update the QuickTime Video Material
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CQuickTime::Update( QUICKTIMEMaterial_t hMaterial )
{
	return ( hMaterial == QUICKTIMEMATERIAL_INVALID ) ? false :  m_QTMaterials[hMaterial]->Update();
}

//-----------------------------------------------------------------------------
// Determine if a new frame of the movie is ready for display
//-----------------------------------------------------------------------------
bool CQuickTime::ReadyForSwap( QUICKTIMEMaterial_t hMaterial )
{
	return ( hMaterial == QUICKTIMEMATERIAL_INVALID ) ? false :  m_QTMaterials[hMaterial]->ReadyForSwap();
}

//-----------------------------------------------------------------------------
// Gets the IMaterial associated with an QuickTime video material
//-----------------------------------------------------------------------------
IMaterial *CQuickTime::GetMaterial( QUICKTIMEMaterial_t h )
{
	return ( h != QUICKTIMEMATERIAL_INVALID ) ? m_QTMaterials[h]->GetMaterial() : NULL;
}


//-----------------------------------------------------------------------------
// Returns the max texture coordinate of the QuickTime Video on the texture
//-----------------------------------------------------------------------------
void CQuickTime::GetTexCoordRange( QUICKTIMEMaterial_t h, float *pMaxU, float *pMaxV )
{
	if ( h != QUICKTIMEMATERIAL_INVALID )
	{
		m_QTMaterials[h]->GetTexCoordRange( pMaxU, pMaxV );
	}
	else
	{
		*pMaxU = *pMaxV = 1.0f;
	}
}


//-----------------------------------------------------------------------------
// Returns the frame size of the Quicktime Video (is a subrect of the material itself)
//-----------------------------------------------------------------------------
void CQuickTime::GetFrameSize( QUICKTIMEMaterial_t h, int *pWidth, int *pHeight )
{
	if ( h != QUICKTIMEMATERIAL_INVALID )
	{
		m_QTMaterials[h]->GetFrameSize( pWidth, pHeight );
	}
	else
	{
		*pWidth = *pHeight = 1;
	}
}

//-----------------------------------------------------------------------------
// Returns the frame size of the QuickTime Video (is a subrect of the material itself)
//-----------------------------------------------------------------------------
int CQuickTime::GetFrameRate( QUICKTIMEMaterial_t h )
{
	return ( h == QUICKTIMEMATERIAL_INVALID ) ? -1 : m_QTMaterials[h]->GetFrameRate();
}

//-----------------------------------------------------------------------------
// Sets the frame for an Quicktime Video material (use instead of SetTime)
//-----------------------------------------------------------------------------
void CQuickTime::SetFrame( QUICKTIMEMaterial_t h, float flFrame )
{
	if ( h != QUICKTIMEMATERIAL_INVALID )
	{
		m_QTMaterials[h]->SetFrame( flFrame );
	}
}

//-----------------------------------------------------------------------------
// Returns the frame rate of the Quicktime Video
//-----------------------------------------------------------------------------
int CQuickTime::GetFrameCount( QUICKTIMEMaterial_t h )
{
	return ( h == QUICKTIMEMATERIAL_INVALID ) ? -1 : m_QTMaterials[h]->GetFrameCount();
}


//-----------------------------------------------------------------------------
// Hooks up the houtput sound device
//-----------------------------------------------------------------------------
bool CQuickTime::SetSoundDevice( void *pDevice )
{
	pDevice;
	return true;
}



// ----------------------------------------------------------------------------
// functions to initialize and shut down QuickTime services
// ----------------------------------------------------------------------------
bool CQuickTime::SetupQuicktime()
{
	m_bQTInitialized = false;

#if defined ( QUICKTIME_VIDEO )	
  #if defined ( WIN32 )
	OSErr status = InitializeQTML( 0 ); 
    
    // if -2903 then quicktime not installed on this system
    if ( status != noErr )
    {
		if  ( status == qtmlDllLoadErr )
		{
			//Plat_MessageBox( "VideoCache ERROR", "ERROR: QuickTime is not installed on this system.  It is needed in order for the SFM Video Cache service to run" );
			Assert( 0 );
		}			
		return false;
    }
    
	// Make sure we have version 7.04 or greater of quicktime 
    long version = 0;
    status = Gestalt(gestaltQuickTime, &version);
    if ( (status != noErr) || ( version < 0x07048000) )
    {
		TerminateQTML();
        return false;
    }
    
  #endif    
	
	OSErr status2 = EnterMovies();           // Initialize QuickTime Movie Toolbox
	if ( status2 != noErr )
	{
		Assert( 0 );
  #if defined ( WIN32 )	
		TerminateQTML();
  #endif
		return false;
	}
    
    m_bQTInitialized = true;
#endif
    
	return m_bQTInitialized;
}


// ----------------------------------------------------------------------------
void CQuickTime::ShutdownQuicktime()
{
	if ( m_bQTInitialized )
	{
#if defined ( QUICKTIME_VIDEO )	
       ExitMovies();                               // Terminate QuickTime  
  #if defined ( WIN32 )
       TerminateQTML();                            // Terminate QTML  
  #endif
#endif       
       m_bQTInitialized = false;
    }
    
}


