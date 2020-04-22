//====== Copyright 1996-2006, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#if defined( BINK_VIDEO )

#if !defined( _GAMECONSOLE ) || defined( BINK_ENABLED_FOR_CONSOLE )

#include "avi/ibik.h"
#if defined( _X360 )
#	include "bink_x360/bink.h"
#elif defined( _PS3 )
#	include "bink_ps3/bink.h"
#else
#	include "bink/bink.h"
#endif
#include "filesystem.h"
#include "tier1/strtools.h"
#include "tier1/utllinkedlist.h"
#include "tier1/keyvalues.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/itexture.h"
#include "callqueue.h"
#include "vtf/vtf.h"
#include "pixelwriter.h"
#include "tier3/tier3.h"
#if defined( _X360 )
#include "snd_dev_xaudio.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#pragma warning( disable : 4201 )

#define WIN32_LEAN_AND_MEAN

#pragma warning( default : 4201 )

//#define BINK_TRACK_71_ENABLED		// Current movies are in 5.1

#if defined( PLATFORM_X360 )
	// On 360, we specifically want a linear format so that we aren't swizzling on the CPU every frame.
	#define BINK_SHADER_IMAGE_FORMAT IMAGE_FORMAT_LINEAR_I8
	#define BINK_NUMBER_OF_CHANNELS	6		// Up to 5.1
#elif defined( PLATFORM_PS3 )
	#define BINK_SHADER_IMAGE_FORMAT IMAGE_FORMAT_I8
	#define BINK_NUMBER_OF_CHANNELS	8		// Up to 7.1
#else
	#define BINK_SHADER_IMAGE_FORMAT IMAGE_FORMAT_I8
	#define BINK_NUMBER_OF_CHANNELS	6		// Up to 5.1
#endif

ConVar bink_mat_queue_mode( "bink_mat_queue_mode", "1", 0, "Update bink on mat queue thread if mat_queue_mode is on (if turned off, always update bink on main thread; may cause stalls!)" );
ConVar bink_try_load_vmt( "bink_try_load_vmt", "0", 0, "Try and load a VMT with the same name as the BIK file to override settings" );
ConVar bink_use_preallocated_scratch_texture( "bink_use_preallocated_scratch_texture", "1", 0, "Use a pre-allocated VTF instead of creating a new one and deleting it for every texture update. Gameconsole only." );

// don't set volume when using the wave out device.  This will cause changes to the global volume
// mixer and we can't tell the difference between waveOut setting those and a user doing that.
// Also bink will set our volume to 25% instead of 100% in that case.
static bool g_bDisableVolumeChanges = false;

// We don't support the alpha channel in bink files due to dx8.  Can make it work if necessary.
//#define SUPPORT_BINK_ALPHA

class CBIKMaterial;

struct PrecachedMovie_t
{
	CUtlString	m_BaseName;
	CUtlBuffer	m_MemoryBuffer;
};

PathTypeFilter_t GetMoviePathFilter()
{
	static ConVarRef force_audio_english( "force_audio_english" );

#if defined( _GAMECONSOLE )
	if ( XBX_IsAudioLocalized() && force_audio_english.GetBool() )
	{
		// skip the localized search paths and fall through
		return FILTER_CULLLOCALIZED_ANY;
	}
#else
	if ( force_audio_english.GetBool() )
	{
		// skip the localized search paths and fall through
		return FILTER_CULLLOCALIZED_ANY;
	}
#endif

	// No movies exists inside of zips, all the movies are external
	return FILTER_CULLPACK;
}

class CBIKMaterialYTextureRegenerator : public ITextureRegenerator
{
public:
	void SetParentMaterial( CBIKMaterial *pBIKMaterial, int nWidth, int nHeight )
	{
		m_pBIKMaterial = pBIKMaterial;
		m_nSourceWidth = nWidth;
		m_nSourceHeight = nHeight;
	}

	// Inherited from ITextureRegenerator
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect );
	virtual void Release();

	virtual bool HasPreallocatedScratchTexture() const { return IsGameConsole() ? bink_use_preallocated_scratch_texture.GetBool() : false; }
	virtual IVTFTexture *GetPreallocatedScratchTexture();

private:
	CBIKMaterial	*m_pBIKMaterial;
	int				m_nSourceWidth;
	int				m_nSourceHeight;
};

#ifdef SUPPORT_BINK_ALPHA
class CBIKMaterialATextureRegenerator : public ITextureRegenerator
{
public:
	void SetParentMaterial( CBIKMaterial *pBIKMaterial, int nWidth, int nHeight )
	{
		m_pBIKMaterial = pBIKMaterial;
		m_nSourceWidth = nWidth;
		m_nSourceHeight = nHeight;
	}

	// Inherited from ITextureRegenerator
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect );
	virtual void Release();

	virtual bool HasPreallocatedScratchTexture() const { return IsGameConsole() ? bink_use_preallocated_scratch_texture.GetBool() : false; }
	virtual IVTFTexture *GetPreallocatedScratchTexture();

private:
	CBIKMaterial	*m_pBIKMaterial;
	int				m_nSourceWidth;
	int				m_nSourceHeight;
};
#endif

class CBIKMaterialCrTextureRegenerator : public ITextureRegenerator
{
public:
	void SetParentMaterial( CBIKMaterial *pBIKMaterial, int nWidth, int nHeight )
	{
		m_pBIKMaterial = pBIKMaterial;
		m_nSourceWidth = nWidth;
		m_nSourceHeight = nHeight;
	}

	// Inherited from ITextureRegenerator
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect );
	virtual void Release();

	virtual bool HasPreallocatedScratchTexture() const { return IsGameConsole() ? bink_use_preallocated_scratch_texture.GetBool() : false; }
	virtual IVTFTexture *GetPreallocatedScratchTexture();

private:
	CBIKMaterial	 *m_pBIKMaterial;
	int				m_nSourceWidth;
	int				m_nSourceHeight;
};

class CBIKMaterialCbTextureRegenerator : public ITextureRegenerator
{
public:
	void SetParentMaterial( CBIKMaterial *pBIKMaterial, int nWidth, int nHeight )
	{
		m_pBIKMaterial = pBIKMaterial;
		m_nSourceWidth = nWidth;
		m_nSourceHeight = nHeight;
	}

	// Inherited from ITextureRegenerator
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect );
	virtual void Release();

	virtual bool HasPreallocatedScratchTexture() const { return IsGameConsole() ? bink_use_preallocated_scratch_texture.GetBool() : false; }
	virtual IVTFTexture *GetPreallocatedScratchTexture();

private:
	CBIKMaterial	 *m_pBIKMaterial;
	int				m_nSourceWidth;
	int				m_nSourceHeight;
};

//-----------------------------------------------------------------------------
//
// Class used to associated BIK files with IMaterials
//
//-----------------------------------------------------------------------------
class CBIKMaterial
{
public:
	CBIKMaterial();

	// Initializes, shuts down the material
	bool Init( const char *pMaterialName, const char *pFileName, const char *pPathID, int flags );
	bool Shutdown();

	// Keeps the frames updated
	// Work is actually done by UpdateInternal, either on the main thread or on the mat queue thread
	bool Update( void );

	// Call this in a loop that does nothing or something minor right before calling SwapBuffers.
	bool ReadyForSwap();

	// Returns the material
	IMaterial *GetMaterial();

	// Returns the texcoord range
	void GetTexCoordRange( float *pMaxU, float *pMaxV );

	// Returns the frame size of the BIK (stored in a subrect of the material itself)
	void GetFrameSize( int *pWidth, int *pHeight );

	// Returns the frame rate/count of the BIK
	int GetFrame( void );
	int GetFrameRate( void );
	int GetFrameCount( void );

	// Sets the frame for an BIK material (use instead of SetTime)
	void SetFrame( float flFrame );
	void SetLooping( bool bLoops = true ) { m_bLoops = bLoops; }

	void Pause( void );
	void Unpause( void );

	// Access cached frame information (takes a mutex; safe to call any time, but may be a frame delayed)
	bool IsVideoFinished();

	IVTFTexture * GetScratchVTFTexture() { return m_pScratchTexture; }

	void UpdateVolume();

	bool IsMovieResidentInMemory();

private:

	friend class CBIKMaterialYTextureRegenerator;
#ifdef SUPPORT_BINK_ALPHA
	friend class CBIKMaterialATextureRegenerator;
#endif
	friend class CBIKMaterialCrTextureRegenerator;
	friend class CBIKMaterialCbTextureRegenerator;

	// Initializes, shuts down the procedural texture
	void CreateProceduralTextures( const char *pTextureName );
	void DestroyProceduralTexture( CTextureReference &texture );
	void DestroyProceduralTextures();

	// Initializes, shuts down the procedural material
	void CreateProceduralMaterial( const char *pMaterialName );
	void DestroyProceduralMaterial();

	// Initializes, shuts down the video stream
	void CreateVideoStream( );
	void DestroyVideoStream( );

	// Performs the actual bink texture update, either on the mat queue thread or on the main thread
	void UpdateInternal();

	void UpdateCurrentFrameIndex();
	
	// Performs the actual bink frame set operation
	void SetFrameInternal( int nFrame );

	void SetTracks();

	CMaterialReference m_Material;
	CTextureReference m_TextureY;
#ifdef SUPPORT_BINK_ALPHA
	CTextureReference m_TextureA;
#endif
	CTextureReference m_TextureCr;
	CTextureReference m_TextureCb;

	HBINK m_pHBINK;

	BINKFRAMEBUFFERS m_buffers;

	int m_nBinkFlags;

	int m_nBIKWidth;
	int m_nBIKHeight;

	int m_nFrameRate;
	int m_nFrameCount;

	int m_nCurrentFrame;

	bool m_bLoops;
	bool m_bShutdown;

	CBIKMaterialYTextureRegenerator m_YTextureRegenerator;
#ifdef SUPPORT_BINK_ALPHA
	CBIKMaterialATextureRegenerator m_ATextureRegenerator;
#endif
	CBIKMaterialCrTextureRegenerator m_CrTextureRegenerator;
	CBIKMaterialCbTextureRegenerator m_CbTextureRegenerator;

	// CTexture::Download() will make a temp copy in a scratch texture every time we update the Bink textures,
	// so use this instead of re-allocating the scratch texture on every update (4 times per frame per bink movie)	
	IVTFTexture *m_pScratchTexture;

	// Since bink can be updated on the mat queue thread, we need to protect internal class state
	CThreadFastMutex m_BinkUpdateMutex;
	CThreadFastMutex m_BinkFrameCountMutex;

	// The number of outstanding calls to UpdateInternal (gets incremented on Update() and decremented when UpdateInternal() is finished
	int m_nQueuedUpdateCount;

#if IsPlatformPS3()
	// Indicates if we have to resume the prefetches, and if yes, when.
	enum PrefetchResumeMode_t
	{
		PRM_NO_RESUME_NECESSARY,				// Used if the movie is in memory
		PRM_RESUME_AT_END_OF_FIRST_FRAME,		// Used if the movie is preloaded
		PRM_RESUME_AT_END_OF_MOVIE				// Used if the movie is streamed
	};
	PrefetchResumeMode_t m_ResumeMode;
#endif
};

//-----------------------------------------------------------------------------
// Inherited from ITextureRegenerator
//-----------------------------------------------------------------------------
void CBIKMaterialYTextureRegenerator::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
{
	int nWidth = m_nSourceWidth;
	int nHeight = m_nSourceHeight;
	unsigned char *pYData = NULL;
	int nBytes = 0;
	int y;
	CPixelWriter pixelWriter;
	int nBufferPitch = 0;

	// Error condition
	if ( (pVTFTexture->FrameCount() > 1) || (pVTFTexture->FaceCount() > 1) || (pVTFTexture->MipCount() > 1) || (pVTFTexture->Depth() > 1) )
	{
		goto BIKMaterialError;
	}

	pYData = (unsigned char *)m_pBIKMaterial->m_buffers.Frames[ m_pBIKMaterial->m_buffers.FrameNum ].YPlane.Buffer;
	nBufferPitch = m_pBIKMaterial->m_buffers.Frames[ m_pBIKMaterial->m_buffers.FrameNum ].YPlane.BufferPitch;

	Assert( pVTFTexture->Format() == BINK_SHADER_IMAGE_FORMAT );
	Assert( pVTFTexture->RowSizeInBytes( 0 ) == pVTFTexture->Width() );
	Assert( pVTFTexture->Width() >= m_nSourceWidth );
	Assert( pVTFTexture->Height() >= m_nSourceHeight );

	// Set up the pixel writer to write into the VTF texture
	pixelWriter.SetPixelMemory( pVTFTexture->Format(), pVTFTexture->ImageData(), pVTFTexture->RowSizeInBytes( 0 ) );

	for ( y = 0; y < nHeight; ++y )
	{
		pixelWriter.Seek( 0, y );
		memcpy( pixelWriter.GetCurrentPixel(), pYData, nWidth );
		pYData += nBufferPitch;
	}

	return;

BIKMaterialError:
	nBytes = pVTFTexture->ComputeTotalSize();
	memset( pVTFTexture->ImageData(), 0xFF, nBytes );
	return;
}


IVTFTexture *CBIKMaterialYTextureRegenerator::GetPreallocatedScratchTexture() 
{ 
	return m_pBIKMaterial->GetScratchVTFTexture(); 
}

void CBIKMaterialYTextureRegenerator::Release()
{
}

#ifdef SUPPORT_BINK_ALPHA
//-----------------------------------------------------------------------------
// Inherited from ITextureRegenerator
//-----------------------------------------------------------------------------
void CBIKMaterialATextureRegenerator::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
{
	CPixelWriter pixelWriter;
	int nBufferPitch = 0;

	// Error condition
	if ( (pVTFTexture->FrameCount() > 1) || (pVTFTexture->FaceCount() > 1) || (pVTFTexture->MipCount() > 1) || (pVTFTexture->Depth() > 1) )
	{
		goto BIKMaterialError;
	}

	unsigned char *pAData = (unsigned char *)m_pBIKMaterial->m_buffers.Frames[ m_pBIKMaterial->m_buffers.FrameNum ].APlane.Buffer;
	nBufferPitch = m_pBIKMaterial->m_buffers.Frames[ m_pBIKMaterial->m_buffers.FrameNum ].APlane.BufferPitch;

	Assert( pVTFTexture->Format() == BINK_SHADER_IMAGE_FORMAT );
	Assert( pVTFTexture->RowSizeInBytes( 0 ) == pVTFTexture->Width() );
	Assert( pVTFTexture->Width() >= m_nSourceWidth );
	Assert( pVTFTexture->Height() >= m_nSourceHeight );

	// Set up the pixel writer to write into the VTF texture
	pixelWriter.SetPixelMemory( pVTFTexture->Format(), pVTFTexture->ImageData(), pVTFTexture->RowSizeInBytes( 0 ) );

	int nWidth = m_nSourceWidth;
	int nHeight = m_nSourceHeight;
	int y;
	if( pAData )
	{
		for ( y = 0; y < nHeight; ++y )
		{
			pixelWriter.Seek( 0, y );
			memcpy( pixelWriter.GetCurrentPixel(), pAData, nWidth );
			pAData += nBufferPitch;
		}
	}
	else
	{
		for ( y = 0; y < nHeight; ++y )
		{
			pixelWriter.Seek( 0, y );
			memset( pixelWriter.GetCurrentPixel(), 255, nWidth );
		}
	}

	return;

BIKMaterialError:
	int nBytes = pVTFTexture->ComputeTotalSize();
	memset( pVTFTexture->ImageData(), 0xFF, nBytes );
	return;
}

IVTFTexture *CBIKMaterialATextureRegenerator::GetPreallocatedScratchTexture() 
{ 
	return m_pBIKMaterial->GetScratchVTFTexture(); 
}

void CBIKMaterialATextureRegenerator::Release()
{
}
#endif

//-----------------------------------------------------------------------------
// Inherited from ITextureRegenerator
//-----------------------------------------------------------------------------
void CBIKMaterialCrTextureRegenerator::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
{
	int nWidth = m_nSourceWidth;
	int nHeight = m_nSourceHeight;
	unsigned char *pCrData = NULL;
	CPixelWriter pixelWriter;
	int nBufferPitch = 0;

	// Error condition
	if ( (pVTFTexture->FrameCount() > 1) || (pVTFTexture->FaceCount() > 1) || (pVTFTexture->MipCount() > 1) || (pVTFTexture->Depth() > 1) )
	{
		goto BIKMaterialError;
	}

	pCrData = (unsigned char *)m_pBIKMaterial->m_buffers.Frames[ m_pBIKMaterial->m_buffers.FrameNum ].cRPlane.Buffer;
	nBufferPitch = m_pBIKMaterial->m_buffers.Frames[ m_pBIKMaterial->m_buffers.FrameNum ].cRPlane.BufferPitch;

	Assert( pVTFTexture->Format() == BINK_SHADER_IMAGE_FORMAT );
	Assert( pVTFTexture->RowSizeInBytes( 0 ) == pVTFTexture->Width() );
	Assert( pVTFTexture->Width() >= m_nSourceWidth );
	Assert( pVTFTexture->Height() >= m_nSourceHeight );

	// Set up the pixel writer to write into the VTF texture
	pixelWriter.SetPixelMemory( pVTFTexture->Format(), pVTFTexture->ImageData(), pVTFTexture->RowSizeInBytes( 0 ) );

	int y;
	for ( y = 0; y < nHeight; ++y )
	{
		pixelWriter.Seek( 0, y );
		memcpy( pixelWriter.GetCurrentPixel(), pCrData, nWidth );
		pCrData += nBufferPitch;
	}

	return;

BIKMaterialError:
	int nBytes = pVTFTexture->ComputeTotalSize();
	memset( pVTFTexture->ImageData(), 0xFF, nBytes );
	return;
}

IVTFTexture *CBIKMaterialCrTextureRegenerator::GetPreallocatedScratchTexture() 
{ 
	return m_pBIKMaterial->GetScratchVTFTexture(); 
}

void CBIKMaterialCrTextureRegenerator::Release()
{
}

//-----------------------------------------------------------------------------
// Inherited from ITextureRegenerator
//-----------------------------------------------------------------------------
void CBIKMaterialCbTextureRegenerator::RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect )
{
	int nWidth = m_nSourceWidth;
	int nHeight = m_nSourceHeight;
	unsigned char *pCbData = NULL;
	CPixelWriter pixelWriter;
	int nBufferPitch = 0;

	// Error condition
	if ( (pVTFTexture->FrameCount() > 1) || (pVTFTexture->FaceCount() > 1) || (pVTFTexture->MipCount() > 1) || (pVTFTexture->Depth() > 1) )
	{
		goto BIKMaterialError;
	}

	pCbData = (unsigned char *)m_pBIKMaterial->m_buffers.Frames[ m_pBIKMaterial->m_buffers.FrameNum ].cBPlane.Buffer;
	nBufferPitch = m_pBIKMaterial->m_buffers.Frames[ m_pBIKMaterial->m_buffers.FrameNum ].cBPlane.BufferPitch;

	Assert( pVTFTexture->Format() == BINK_SHADER_IMAGE_FORMAT );
	Assert( pVTFTexture->RowSizeInBytes( 0 ) == pVTFTexture->Width() );
	Assert( pVTFTexture->Width() >= m_nSourceWidth );
	Assert( pVTFTexture->Height() >= m_nSourceHeight );

	// Set up the pixel writer to write into the VTF texture
	pixelWriter.SetPixelMemory( pVTFTexture->Format(), pVTFTexture->ImageData(), pVTFTexture->RowSizeInBytes( 0 ) );

	int y;
	for ( y = 0; y < nHeight; ++y )
	{
		pixelWriter.Seek( 0, y );
		memcpy( pixelWriter.GetCurrentPixel(), pCbData, nWidth );
		pCbData += nBufferPitch;
	}

	return;

BIKMaterialError:
	int nBytes = pVTFTexture->ComputeTotalSize();
	memset( pVTFTexture->ImageData(), 0xFF, nBytes );
	return;
}

IVTFTexture *CBIKMaterialCbTextureRegenerator::GetPreallocatedScratchTexture() 
{ 
	return m_pBIKMaterial->GetScratchVTFTexture(); 
}

void CBIKMaterialCbTextureRegenerator::Release()
{
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CBIKMaterial::CBIKMaterial()
{
	m_pHBINK = NULL;
	Q_memset( &m_buffers, 0, sizeof( m_buffers ) );
	m_bLoops = false;
	m_nQueuedUpdateCount = 0;
	m_bShutdown = false;
#if IsPlatformPS3()
	m_ResumeMode = PRM_NO_RESUME_NECESSARY;
#endif
	m_pScratchTexture = NULL;
	m_nBinkFlags = 0;
}

//-----------------------------------------------------------------------------
// Initializes the material
//-----------------------------------------------------------------------------
bool CBIKMaterial::Init( const char *pMaterialName, const char *pFileName, const char *pPathID, int flags )
{
	// Determine the full path name of the BIK
	char pBIKFileName[ 512 ];
	char pFullBIKFileName[ 512 ];
	Q_snprintf( pBIKFileName, sizeof( pBIKFileName ), "%s", pFileName );
	Q_DefaultExtension( pBIKFileName, ".bik", sizeof( pBIKFileName ) );

	PathTypeQuery_t pathType;
	if ( !g_pFullFileSystem->RelativePathToFullPath( pBIKFileName, pPathID, pFullBIKFileName, sizeof( pFullBIKFileName ), GetMoviePathFilter(), &pathType ) )
	{
		// A file by that name was not found
		Assert( 0 );
		return false;
	}
	
	//Msg( "BinkOpen( %s )\n", pFullBIKFileName );

	U32 binkFlags = BINKNOFRAMEBUFFERS | BINKSNDTRACK;

	if ( flags & BIK_PRELOAD )
	{
		binkFlags |= BINKPRELOADALL;
	}

	if ( ( flags & BIK_NO_AUDIO ) != 0 )
	{
		// No audio
		BinkSetSoundTrack( 0, 0 );
	}
	else
	{
#if BINK_NUMBER_OF_CHANNELS == 8
		// Settings for 7.1
		//	Track ID 0 - A stereo track containing the front left and front right channels. 
		//	Track ID 1 - A mono track containing the center channel. 
		//	Track ID 2 - A mono track containing the sub-woofer channel. 
		//	Track ID 3 - A stereo track containing the back left and back right channels. 
		//	Track ID 4 - A stereo track containing the side left and side right channels.
		U32 TrackIDsToPlay[ 5 ] = { 0, 1, 2, 3, 4 };
		BinkSetSoundTrack( 5, TrackIDsToPlay );
#elif defined(OSX)
		U32 TrackIDsToPlay[ 2 ] = { 0, 1 };
		BinkSetSoundTrack( 2, TrackIDsToPlay );
#else
		// Setting 8 channels may not seem to make the X360 implementation of Bink happy. Use 4 tracks for the 6 channels.
		U32 TrackIDsToPlay[ 4 ] = { 0, 1, 2, 3 };
		BinkSetSoundTrack( 4, TrackIDsToPlay );
#endif
	}

	// perhaps already in memory
	void *pBinkInMemory = g_pBIK->GetPrecachedMovie( pFullBIKFileName );
	if ( pBinkInMemory )
	{
		binkFlags &= ~BINKPRELOADALL;
		binkFlags |= BINKFROMMEMORY;
	}

#if IsPlatformPS3()
	if ( ( binkFlags & BINKFROMMEMORY ) != 0 )
	{
		m_ResumeMode = PRM_NO_RESUME_NECESSARY;					// No issue with prefetching in this case
	}
	else if ( ( binkFlags & BINKPRELOADALL ) != 0 )
	{
		g_pFullFileSystem->SuspendPrefetches( "Bink movie is going to be preloaded." );	// At the end of the first frame, the movie should be loaded
		m_ResumeMode = PRM_RESUME_AT_END_OF_FIRST_FRAME;								// prefetches can restart after that. So we can continue prefetching during the movie.
	}
	else
	{
		// This is the streamed mode
		g_pFullFileSystem->SuspendPrefetches( "Bink movie is going to be streamed.");	// As it is streamed, suspend all prefetches until the end
		m_ResumeMode = PRM_RESUME_AT_END_OF_MOVIE;										// to avoid stuttering while playing the movie
	}
#endif

	m_nBinkFlags = binkFlags;

	m_pHBINK = BinkOpen( pBinkInMemory ? (const char *)pBinkInMemory : pFullBIKFileName, binkFlags );
	if ( !m_pHBINK )
	{
		// The file was unable to be opened
		Assert( 0 );

		m_nBIKWidth = 64;
		m_nBIKHeight = 64;
		m_nFrameRate = 1;
		m_nFrameCount = 1;
		m_Material.Init( "debug/debugempty", TEXTURE_GROUP_OTHER );
		return false;
	}

	SetTracks();

	// Get BIK size
	m_nBIKWidth = m_pHBINK->Width;
	m_nBIKHeight = m_pHBINK->Height;

	m_nFrameRate = (int)( (float)m_pHBINK->FrameRate / (float)m_pHBINK->FrameRateDiv );
	m_nFrameCount = m_pHBINK->Frames;
	m_nCurrentFrame = 0;
	CreateVideoStream();

	// Now we can properly setup out regenerators
	m_YTextureRegenerator.SetParentMaterial( this, m_nBIKWidth, m_nBIKHeight );
#ifdef SUPPORT_BINK_ALPHA
	m_ATextureRegenerator.SetParentMaterial( this, m_nBIKWidth, m_nBIKHeight);
#endif
	// The Cr and Cb display textures are always HALF the res of the Y/A textures.
	// However, the bink framebuffers which get decompressed have their own alignment requirements that may cause them to
	// be slightly larger.  In such a case, we only use this smaller sub-rect (the rest may be invalid)
	m_CrTextureRegenerator.SetParentMaterial( this, m_nBIKWidth >> 1, m_nBIKHeight >> 1 );
	m_CbTextureRegenerator.SetParentMaterial( this, m_nBIKWidth >> 1, m_nBIKHeight >> 1 );

	CreateProceduralTextures( pMaterialName );
	CreateProceduralMaterial( pMaterialName );

	SetLooping( ( flags & BIK_LOOP ) != 0 );

	UpdateVolume();

	return true;
}

#if PLATFORM_X360
// After trying without success to have a the same setup between PC, PS3 and X360,
// I ended up creating specific version for each to make Bink work in all cases.

// Talking with Bink support, they said that the model to follow should be the X360 model.
// I.e. each call is listing the number of channels in the track, instead of listing all channels.
// I did not test it though, and it still seems to be a flaw in their API.
// The other model (X360 following the PS3 model mixed with X360) does not work.
void CBIKMaterial::SetTracks()
{
	U32 bins[ 2 ];

	// front LR
	bins[ 0 ] = 0;			// 0 is front left on XAudio
	bins[ 1 ] = 1;			// 1 is front right on XAudio
	BinkSetMixBins( m_pHBINK, 0, bins, 2 );

	// center
	bins [ 0 ] = 2;			// 2 is center on XAudio
	BinkSetMixBins( m_pHBINK, 1, bins, 1 );

	// sub
	bins [ 0 ] = 3;			// 3 is sub on XAudio
	BinkSetMixBins( m_pHBINK, 2, bins, 1 );

	// back LR
	bins[ 0 ] = 4;			// 4 is back left on XAudio
	bins[ 1 ] = 5;			// 5 is back right on XAudio
	BinkSetMixBins( m_pHBINK, 3, bins, 2 );
}
#elif defined(PLATFORM_PS3)
void CBIKMaterial::SetTracks()
{
	int nMasterVolume = 32768;

	S32 nVolumes[ BINK_NUMBER_OF_CHANNELS ];	// Up to 8 tracks for 7.1

	// front LR
	memset( nVolumes, 0, sizeof( nVolumes ) );
	nVolumes[ 0 ] = nMasterVolume;
	nVolumes[ 1 ] = nMasterVolume;
	BinkSetMixBinVolumes( m_pHBINK, 0, NULL, nVolumes, BINK_NUMBER_OF_CHANNELS );

	// center
	memset( nVolumes, 0, sizeof( nVolumes ) );
	nVolumes[ 2 ] = nMasterVolume;
	BinkSetMixBinVolumes( m_pHBINK, 1, NULL, nVolumes, BINK_NUMBER_OF_CHANNELS );

	// sub
	memset( nVolumes, 0, sizeof( nVolumes ) );
	nVolumes[ 3 ] = nMasterVolume;
	BinkSetMixBinVolumes( m_pHBINK, 2, NULL, nVolumes, BINK_NUMBER_OF_CHANNELS );

#if BINK_TRACK_71_ENABLED
	// This is not enabled, we only play 5.1 right now (on Portal 2).

	// back LR
	memset( nVolumes, 0, sizeof( nVolumes ) );
	nVolumes[ 4 ] = nMasterVolume;
	nVolumes[ 5 ] = nMasterVolume;
	BinkSetMixBinVolumes( m_pHBINK, 3, NULL, nVolumes, BINK_NUMBER_OF_CHANNELS );

#if ( BINK_NUMBER_OF_CHANNELS == 8 )
	// side LR
	memset( nVolumes, 0, sizeof( nVolumes ) );
	nVolumes[ 6 ] = nMasterVolume;
	nVolumes[ 7 ] = nMasterVolume;
	BinkSetMixBinVolumes( m_pHBINK, 4, NULL, nVolumes, BINK_NUMBER_OF_CHANNELS );
#endif

#else

	// route rear bink track to both back and side speakers
	memset( nVolumes, 0, sizeof( nVolumes ) );
	nVolumes[ 4 ] = nMasterVolume;
	nVolumes[ 5 ] = nMasterVolume;

	// If we are in 7.1, we want to duplicate the side speakers to the speakers. snd_ps3_back_channel_multiplier will be equal to 1.0f.
	// If we are in 5.1, we may not want to do that to create issues with the downmixer. snd_ps3_back_channel_multiplier will be equal to 0.0f.
	extern ConVar snd_ps3_back_channel_multiplier;
	nMasterVolume = ( int )( ( ( float ) nMasterVolume ) * snd_ps3_back_channel_multiplier.GetFloat() );

#if ( BINK_NUMBER_OF_CHANNELS == 8 )
	nVolumes[ 6 ] = nMasterVolume;
	nVolumes[ 7 ] = nMasterVolume;
#endif

	BinkSetMixBinVolumes( m_pHBINK, 3, NULL, nVolumes, BINK_NUMBER_OF_CHANNELS );

#endif
}
#elif defined(PLATFORM_WINDOWS)
void CBIKMaterial::SetTracks()
{
	S32 volumes[ 6 ]; // 6 channels for 5.1
	U32 bins[ 6 ];

	// turn on the front left and right for the first Bink track
	memset( volumes, 0, sizeof( volumes ) );
	memset( bins, 0, sizeof( bins ) );
	volumes[ 0 ] = 32768;
	volumes[ 1 ] = 32768;
	bins[ 0 ] = 0;
	bins[ 1 ] = 1;
	BinkSetMixBinVolumes( m_pHBINK, 0, bins, volumes, 6 ); 

	// turn on the center for the second Bink track
	memset( volumes, 0, sizeof( volumes ) );
	memset( bins, 0, sizeof( bins ) );
	volumes[ 2 ] = 32768;
	bins[ 2 ] = 2;
	BinkSetMixBinVolumes( m_pHBINK, 1, bins, volumes, 6 ); 

	// turn on the sub woofer for the third Bink track
	memset( volumes, 0, sizeof( volumes ) );
	memset( bins, 0, sizeof( bins ) );
	volumes[ 3 ] = 32768;
	bins[ 3 ] = 3;
	BinkSetMixBinVolumes( m_pHBINK, 2, bins, volumes, 6 ); 

	// turn on the back left and right for the final Bink track
	memset( volumes, 0, sizeof( volumes ) );
	memset( bins, 0, sizeof( bins ) );
	volumes[ 4 ] = 32768;
	volumes[ 5 ] = 32768;
	bins[ 4 ] = 4;
	bins[ 5 ] = 5;
	BinkSetMixBinVolumes( m_pHBINK, 3, bins, volumes, 6 ); 
}
#elif defined(OSX)
void CBIKMaterial::SetTracks()
{
	S32 volumes[ 3 ]; // 2 channels for stero + mix in the center channel to left and right
	U32 bins[ 3 ];
	
	// turn on the front left and right for the first Bink track
	memset( volumes, 0, sizeof( volumes ) );
	memset( bins, 0, sizeof( bins ) );
	volumes[ 0 ] = 32768;
	volumes[ 1 ] = 32768;
	bins[ 0 ] = 0;
	bins[ 1 ] = 1;
	BinkSetMixBinVolumes( m_pHBINK, 0, bins, volumes, 3 ); 

	// turn on the center for the second Bink track
	memset( volumes, 0, sizeof( volumes ) );
	memset( bins, 0, sizeof( bins ) );
	volumes[ 0 ] = 16535;
	volumes[ 1 ] = 16535;
	bins[ 2 ] = 2;
	BinkSetMixBinVolumes( m_pHBINK, 1, bins, volumes, 3 ); 
}
#else
void CBIKMaterial::SetTracks()
{
	Assert( !"Need some code here please" );
	// Do nothing... Mac does not use Bink.
}
#endif

void CBIKMaterial::UpdateVolume()
{
	if ( !m_pHBINK || g_bDisableVolumeChanges )
		return;

	if ( !( m_nBinkFlags & BIK_NO_AUDIO ) )
	{
		// set the master volume
		static ConVarRef volumeConVar( "volume" );
		static ConVarRef movieVolumeScaleConVar( "movie_volume_scale" );
		float flVolume = volumeConVar.GetFloat() * movieVolumeScaleConVar.GetFloat() * 32768.0f;

		static ConVarRef snd_surroundSpeakersConVarRef( "snd_surround_speakers" );
		switch ( snd_surroundSpeakersConVarRef.GetInt() )
		{
		default:		// 5.1 or 7.1
		case -1:		// Not initialized yet, keep the same value
			// Keep it the way it is...
			break;
		case 2:
			// The output is in stereo, but the movie is 5.1
			// We reduce the volume so when downmixed we have the correct overall volume.
#if defined( PLATFORM_PS3 )
			// This value is coming from this formula:
			//   8 (7.1)ch -> 2ch [CELL_AUDIO_OUT_DOWNMIXER_TYPE_A]
			//	 L(mix) = 0.707 x L + 0.5 x C + 0.5 x Ls + 0.5 x Le
			//   R(mix) = 0.707 x R + 0.5 x C + 0.5 x Rs + 0.5 x Re
			// In this case Le and Re are 0.0, so the multiplying factor is 1.707
			// Thus we have to multiply it by 0.58585858 to re-normalize the volume.
			// flVolume *= 0.58585858f;
			// However after empirical testing on Portal 2, this value sounds better:
			flVolume *= 0.781144f;
#elif defined( PLATFORM_X360 )
			// Similar values for X360.
			// (potentially the downmixing on X360 is adding 0.25 of the center and 0.25 of the back surround)
			flVolume *= 0.66f;
#endif
			break;
		}

		S32 nVolume = (S32)( flVolume );
		for ( int i = 0; i != m_pHBINK->NumTracks; ++i )
		{
			BinkSetVolume( m_pHBINK, BinkGetTrackID( m_pHBINK, i ), nVolume );
		}
	}
}

bool CBIKMaterial::IsMovieResidentInMemory()
{
	if ( !m_pHBINK )
		return false;

	return ( ( m_nBinkFlags & ( BINKPRELOADALL | BINKFROMMEMORY ) ) != 0 );
}

static void ShutdownAndDeleteBinkMaterial( CBIKMaterial *pBIK )
{
	if ( !pBIK->Shutdown() )
	{
		// WILL CRASH HERE
		DebuggerBreakIfDebugging();
	}
	delete pBIK;
}

bool CBIKMaterial::Shutdown( void )
{
	m_bShutdown = true;
	AUTO_LOCK( m_BinkUpdateMutex );
	if ( m_nQueuedUpdateCount != 0 )
	{
		CMatRenderContextPtr pRenderContext( materials );
		if ( pRenderContext->GetCallQueue() )
		{
			pRenderContext->GetCallQueue()->QueueCall( &ShutdownAndDeleteBinkMaterial, this );
			return false;
		}
	}
	// If this isn't 0, then this zombie object is going to get called by the mat queue thread... badness!
	Assert( m_nQueuedUpdateCount == 0 );
	
	DestroyVideoStream();
	DestroyProceduralMaterial();
	DestroyProceduralTextures();

	if ( m_pHBINK )
	{
		if ( ENABLE_BIK_PERF_SPEW )
		{
			BINKSUMMARY summary;
			BinkGetSummary( m_pHBINK, &summary );

			Warning( "BINK PERF:\n" );
			Warning( "--------------------------\n" );
			Warning( "Height of frames: %u\n", summary.Height );
			Warning( "total time (ms): %u\n", summary.TotalTime );
			Warning( "file frame rate: %f\n", ( float )summary.FileFrameRate / ( float )summary.FileFrameRateDiv );
			Warning( "file frame rate: %u\n", summary.FileFrameRate );
			Warning( "file frame rate divisor: %u\n", summary.FileFrameRateDiv );       
			Warning( "frame rate: %f\n", ( float )summary.FrameRate / ( float )summary.FrameRateDiv );
			Warning( "frame rate: %u\n", summary.FrameRate );              
			Warning( "frame rate divisor: %u\n", summary.FrameRateDiv );           
			Warning( "Time to open and prepare for decompression: %u\n", summary.TotalOpenTime );          
			Warning( "Total Frames: %u\n", summary.TotalFrames );            
			Warning( "Total Frames played: %u\n", summary.TotalPlayedFrames );      
			Warning( "Total number of skipped frames: %u\n", summary.SkippedFrames );          
			Warning( "Total number of skipped blits: %u\n", summary.SkippedBlits );           
			Warning( "Total number of sound skips: %u\n", summary.SoundSkips );             
			Warning( "Total time spent blitting: %u\n", summary.TotalBlitTime );          
			Warning( "Total time spent reading: %u\n", summary.TotalReadTime );          
			Warning( "Total time spent decompressing video: %u\n", summary.TotalVideoDecompTime );   
			Warning( "Total time spent decompressing audio: %u\n", summary.TotalAudioDecompTime );   
			Warning( "Total time spent reading while idle: %u\n", summary.TotalIdleReadTime );      
			Warning( "Total time spent reading in background: %u\n", summary.TotalBackReadTime );      
			Warning( "Total io speed (bytes/second): %u\n", summary.TotalReadSpeed );         
			Warning( "Slowest single frame time (ms): %u\n", summary.SlowestFrameTime );       
			Warning( "Second slowest single frame time (ms): %u\n", summary.Slowest2FrameTime );      
			Warning( "Slowest single frame number: %u\n", summary.SlowestFrameNum );        
			Warning( "Second slowest single frame number: %u\n", summary.Slowest2FrameNum );       
			Warning( "Average data rate of the movie: %u\n", summary.AverageDataRate );        
			Warning( "Average size of the frame: %u\n", summary.AverageFrameSize );       
			Warning( "Highest amount of memory allocated: %u\n", summary.HighestMemAmount );       
			Warning( "Total extra memory allocated: %u\n", summary.TotalIOMemory );          
			Warning( "Highest extra memory actually used: %u\n", summary.HighestIOUsed );          
			Warning( "Highest 1 second rate: %u\n", summary.Highest1SecRate );        
			Warning( "Highest 1 second start frame: %u\n", summary.Highest1SecFrame );       
			Warning( "--------------------------\n" );
		}

		BinkClose( m_pHBINK );
		m_pHBINK = NULL;
	}

#if IsPlatformPS3()
	switch ( m_ResumeMode )
	{
	case PRM_NO_RESUME_NECESSARY:
		break;
	case PRM_RESUME_AT_END_OF_FIRST_FRAME:
		// This should not happen, as hopefully one frame at least occurred, but just in case let's do what is expected
		Assert( false );
		// Pass through...
	case PRM_RESUME_AT_END_OF_MOVIE:
		g_pFullFileSystem->ResumePrefetches( "Streaming of Bink movie is finished." );
		m_ResumeMode = PRM_NO_RESUME_NECESSARY;		// Reset the state just in case.
		break;
	}
#endif

	return true;
}

bool CBIKMaterial::IsVideoFinished()
{
	AUTO_LOCK( m_BinkFrameCountMutex );
	return m_bShutdown || ( ( m_nCurrentFrame == m_nFrameCount ) && ( m_bLoops == false ) );
}

// Unless you like to Dine with Philosophers, 
// I don't recommend calling this function unless you already have the m_BinkUpdateMutex
void CBIKMaterial::UpdateCurrentFrameIndex()
{
	AUTO_LOCK( m_BinkUpdateMutex );	// If you already have this mutex, this is re-entrant safe and will early-out
	AUTO_LOCK( m_BinkFrameCountMutex );	
	m_nCurrentFrame = m_pHBINK->FrameNum;
}


//-----------------------------------------------------------------------------
// Purpose: Updates our scene
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBIKMaterial::Update( void )
{
	// is the video done?
	if ( IsVideoFinished() )
		return false;

	// This gets decremented by UpdateInternal(), either right now (if not queueing) or when the work is done (if queueing)
	ThreadInterlockedIncrement( &m_nQueuedUpdateCount );

	CMatRenderContextPtr pRenderContext( materials );
	if ( pRenderContext->GetCallQueue() && bink_mat_queue_mode.GetBool() )
	{
		pRenderContext->GetCallQueue()->QueueCall( this, &CBIKMaterial::UpdateInternal );
	}
	else
	{
		UpdateInternal();

		if ( IsVideoFinished() )
			return false;
	}

	// When using mat_queue_mode with bink movies, the return value 'false' (which indicates that the movie is over) will be delayed by a frame
	return true;
}

void CBIKMaterial::UpdateInternal()
{
	// If you crash in this function when called from the mat queue thread and the member variables appear to be junk, 
	// it is likely that this bink material has been shutdown/deleted already.
	// That would be bad, because the call queue needs to be flushed before you can safely destroy this object.

	AUTO_LOCK( m_BinkUpdateMutex );

	ThreadInterlockedDecrement( &m_nQueuedUpdateCount );
	
	// If we're waiting, then go away
	if ( BinkWait( m_pHBINK ) )
		return;

	// Decompress this frame
	BinkDoFrame( m_pHBINK );
	
	// do we need to skip a frame?
	while ( BinkShouldSkip( m_pHBINK ) )
	{
		UpdateCurrentFrameIndex();

		// is the video done?
		if ( IsVideoFinished() )
			break;

		BinkNextFrame( m_pHBINK );
		
		BinkDoFrame( m_pHBINK );
	}

	// Regenerate our textures
	m_TextureY->Download();
#ifdef SUPPORT_BINK_ALPHA
	m_TextureA->Download();
#endif
	m_TextureCr->Download();
	m_TextureCb->Download();

	UpdateCurrentFrameIndex();
	
	// is the video done?
	if ( IsVideoFinished() )
		return;

	// Move on
	BinkNextFrame( m_pHBINK );
	
	UpdateCurrentFrameIndex();

#if IsPlatformPS3()
	if ( m_ResumeMode == PRM_RESUME_AT_END_OF_FIRST_FRAME )
	{
		g_pFullFileSystem->ResumePrefetches( "Bink movie has been preloaded." );
		m_ResumeMode = PRM_NO_RESUME_NECESSARY;
	}
#endif
}

// Call this in a loop that does nothing or something minor right before calling SwapBuffers.
bool CBIKMaterial::ReadyForSwap( void )
{
	AUTO_LOCK( m_BinkUpdateMutex );

	return !BinkWait( m_pHBINK );
}

//-----------------------------------------------------------------------------
// Returns the material
//-----------------------------------------------------------------------------
IMaterial *CBIKMaterial::GetMaterial()
{
	return m_Material;
}							   


//-----------------------------------------------------------------------------
// Returns the texcoord range
//-----------------------------------------------------------------------------
void CBIKMaterial::GetTexCoordRange( float *pMaxU, float *pMaxV )
{
	AUTO_LOCK( m_BinkUpdateMutex );

	// Must have a luminosity channel
	if ( m_TextureY == NULL )
	{
		*pMaxU = *pMaxV = 1.0f;
		return;
	}

	// YA texture is always larger than the CrCb texture, so always base our size on that
	int nTextureWidth = m_TextureY->GetActualWidth();
	int nTextureHeight = m_TextureY->GetActualHeight();
	if ( nTextureWidth )
		*pMaxU = (float)m_nBIKWidth / (float)nTextureWidth;
	else
		*pMaxU = 0.0f;

	if ( nTextureHeight )
		*pMaxV = (float)m_nBIKHeight / (float)nTextureHeight;
	else
		*pMaxV = 0.0f;
}


//-----------------------------------------------------------------------------
// Returns the frame size of the BIK (stored in a subrect of the material itself)
//-----------------------------------------------------------------------------
void CBIKMaterial::GetFrameSize( int *pWidth, int *pHeight )
{
	*pWidth = m_nBIKWidth;
	*pHeight = m_nBIKHeight;
}

//-----------------------------------------------------------------------------
// Initializes, shuts down the procedural texture
//-----------------------------------------------------------------------------
void CBIKMaterial::CreateProceduralTextures( const char *pTextureName )
{
	int nWidth, nHeight;

	char textureName[MAX_PATH];
	Q_strncpy( textureName, pTextureName, MAX_PATH-1 );
	Q_StripExtension( textureName, textureName, sizeof( textureName ) );
	Q_strncat( textureName, "Y", MAX_PATH );

	unsigned int nTextureFlags = ( TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_NOLOD );

	nWidth = m_nBIKWidth;
	nHeight = m_nBIKHeight;
	m_TextureY.InitProceduralTexture( textureName, "bik", nWidth, nHeight, BINK_SHADER_IMAGE_FORMAT, nTextureFlags );
	m_TextureY->SetTextureRegenerator( &m_YTextureRegenerator );

#ifdef SUPPORT_BINK_ALPHA
	Q_strncpy( textureName, pTextureName, MAX_PATH-1 );
	Q_StripExtension( textureName, textureName, sizeof( textureName ) );
	Q_strncat( textureName, "A", MAX_PATH );

	m_TextureA.InitProceduralTexture( textureName, "bik", nWidth, nHeight, BINK_SHADER_IMAGE_FORMAT, nTextureFlags );
	m_TextureA->SetTextureRegenerator( &m_ATextureRegenerator );
#endif

	Q_strncpy( textureName, pTextureName, MAX_PATH-1 );
	Q_StripExtension( textureName, textureName, sizeof( textureName ) );
	Q_strncat( textureName, "Cr", MAX_PATH );

	nWidth = m_nBIKWidth >> 1;
	nHeight = m_nBIKHeight >> 1;
	m_TextureCr.InitProceduralTexture( textureName, "bik", nWidth, nHeight, BINK_SHADER_IMAGE_FORMAT, nTextureFlags );
	m_TextureCr->SetTextureRegenerator( &m_CrTextureRegenerator );

	Q_strncpy( textureName, pTextureName, MAX_PATH-1 );
	Q_StripExtension( textureName, textureName, sizeof( textureName ) );
	Q_strncat( textureName, "Cb", MAX_PATH );

	m_TextureCb.InitProceduralTexture( textureName, "bik", nWidth, nHeight, BINK_SHADER_IMAGE_FORMAT, nTextureFlags );
	m_TextureCb->SetTextureRegenerator( &m_CbTextureRegenerator );

	// This pulls in way too many lib dependencies on the PC; on consoles it's in engine.dll
	// It's also just a perf improvement and not as useful on PC.
#ifdef _GAMECONSOLE
	Assert( !m_pScratchTexture );
	m_pScratchTexture = CreateVTFTexture();
#endif // _GAMECONSOLE
}

void CBIKMaterial::DestroyProceduralTexture( CTextureReference &texture )
{
	if( texture )
	{
		texture->SetTextureRegenerator( NULL );
		texture.Shutdown( true );
	}

}

void CBIKMaterial::DestroyProceduralTextures()
{	
#ifdef _GAMECONSOLE
	DestroyVTFTexture( m_pScratchTexture );
	m_pScratchTexture = NULL;
#endif // GAMECONSOLE

	DestroyProceduralTexture( m_TextureY );
#ifdef SUPPORT_BINK_ALPHA
	DestroyProceduralTexture( m_TextureA );
#endif
	DestroyProceduralTexture( m_TextureCr );
	DestroyProceduralTexture( m_TextureCb );
}

//-----------------------------------------------------------------------------
// Initializes, shuts down the procedural material
//-----------------------------------------------------------------------------
void CBIKMaterial::CreateProceduralMaterial( const char *pMaterialName )
{
	// FIXME: gak, this is backwards.  Why doesn't the material just see that it has a funky basetexture?
	char vmtfilename[ 512 ];
	Q_strcpy( vmtfilename, pMaterialName );
	Q_SetExtension( vmtfilename, ".vmt", sizeof( vmtfilename ) );

	KeyValues *pVMTKeyValues = new KeyValues( "Bik" );
	if ( bink_try_load_vmt.GetBool() && pVMTKeyValues->LoadFromFile( g_pFullFileSystem , vmtfilename, "GAME" ) )
	{
		// use VMT settings
	}
	else
	{
		pVMTKeyValues->SetString( "$ytexture", m_TextureY->GetName() );
#ifdef SUPPORT_BINK_ALPHA
		pVMTKeyValues->SetString( "$atexture", m_TextureA->GetName() );
#endif
		pVMTKeyValues->SetString( "$crtexture", m_TextureCr->GetName() );
		pVMTKeyValues->SetString( "$cbtexture", m_TextureCb->GetName() );
		pVMTKeyValues->SetInt( "$nofog", 1 );
		pVMTKeyValues->SetInt( "$spriteorientation", 3 );
		pVMTKeyValues->SetInt( "$translucent", 1 );
		pVMTKeyValues->SetInt( "$vertexcolor", 1 );
		pVMTKeyValues->SetInt( "$vertexalpha", 1 );
		pVMTKeyValues->SetInt( "$nolod", 1 );
		pVMTKeyValues->SetInt( "$nomip", 1 );
		pVMTKeyValues->SetInt( "$nobasetexture", 1 );
	}

	m_Material.Init( pMaterialName, pVMTKeyValues );

	m_Material->Refresh();
}

void CBIKMaterial::DestroyProceduralMaterial()
{
	m_Material.Shutdown( true );
}

//-----------------------------------------------------------------------------
// Returns the frame rate of the BIK
//-----------------------------------------------------------------------------
int CBIKMaterial::GetFrameRate( )
{
	return m_nFrameRate;
}

int CBIKMaterial::GetFrameCount( )
{
	return m_nFrameCount;
}


//-----------------------------------------------------------------------------
// Sets the frame for an BIK material (use instead of SetTime)
//-----------------------------------------------------------------------------
void CBIKMaterial::SetFrame( float flFrame )
{
	AUTO_LOCK( m_BinkUpdateMutex );

	U32 nFrame = (U32)flFrame + 1;

	CMatRenderContextPtr pRenderContext( materials );
	if ( pRenderContext->GetCallQueue() && bink_mat_queue_mode.GetBool() )
	{
		pRenderContext->GetCallQueue()->QueueCall( this, &CBIKMaterial::SetFrameInternal, nFrame );
	}
	else
	{
		SetFrameInternal( nFrame );
	}
}

void CBIKMaterial::SetFrameInternal( int nFrame )
{
	AUTO_LOCK( m_BinkUpdateMutex );

	if ( m_pHBINK->LastFrameNum != nFrame )
	{
		BinkGoto( m_pHBINK, nFrame, 0 );
		m_TextureY->Download();
#ifdef SUPPORT_BINK_ALPHA
		m_TextureA->Download();
#endif
		m_TextureCr->Download();
		m_TextureCb->Download();
	}

	UpdateCurrentFrameIndex();
}

//-----------------------------------------------------------------------------
// Initializes, shuts down the video stream
//-----------------------------------------------------------------------------
void CBIKMaterial::CreateVideoStream( )
{
	// get the frame buffers info
	BinkGetFrameBuffersInfo( m_pHBINK, &m_buffers );

	// fixme: these should point to local buffers that the material system can splat
	for ( int i = 0 ; i < m_buffers.TotalFrames ; i++ )
	{  
		if ( m_buffers.Frames[ i ].YPlane.Allocate )
		{    
			// calculate a good pitch    
			m_buffers.Frames[ i ].YPlane.BufferPitch = ( m_buffers.YABufferWidth + 15 ) & ~15;
			// now allocate the pointer
			m_buffers.Frames[ i ].YPlane.Buffer = MemAlloc_AllocAligned( m_buffers.Frames[ i ].YPlane.BufferPitch * m_buffers.YABufferHeight, 16 );
		}
        if ( m_buffers.Frames[ i ].cRPlane.Allocate )
		{    
			// calculate a good pitch    
			m_buffers.Frames[ i ].cRPlane.BufferPitch = ( m_buffers.cRcBBufferWidth + 15 ) & ~15;    
			// now allocate the pointer    
			m_buffers.Frames[ i ].cRPlane.Buffer = MemAlloc_AllocAligned( m_buffers.Frames[ i ].cRPlane.BufferPitch * m_buffers.cRcBBufferHeight, 16 );  
		}

		if ( m_buffers.Frames[ i ].cBPlane.Allocate )
		{    
			// calculate a good pitch    
			m_buffers.Frames[ i ].cBPlane.BufferPitch = ( m_buffers.cRcBBufferWidth + 15 ) & ~15;    
			// now allocate the pointer    
			m_buffers.Frames[ i ].cBPlane.Buffer = MemAlloc_AllocAligned( m_buffers.Frames[ i ].cBPlane.BufferPitch * m_buffers.cRcBBufferHeight, 16 );  
		}

#ifdef SUPPORT_BINK_ALPHA
		if ( m_buffers.Frames[ i ].APlane.Allocate )  
		{    // calculate a good pitch    
			m_buffers.Frames[ i ].APlane.BufferPitch = ( m_buffers.YABufferWidth + 15 ) & ~15;    
			// now allocate the pointer   
			m_buffers.Frames[ i ].APlane.Buffer = MemAlloc_AllocAligned( m_buffers.Frames[ i ].APlane.BufferPitch * m_buffers.YABufferHeight, 16 );  
		}
#endif
	}
	// Now tell Bink to use these new planes
	BinkRegisterFrameBuffers( m_pHBINK, &m_buffers );
}

//-----------------------------------------------------------------------------
// Purpose: Destroy the stream
//-----------------------------------------------------------------------------
void CBIKMaterial::DestroyVideoStream( )
{
	// who free's this?
	for ( int i = 0 ; i < m_buffers.TotalFrames ; i++ )
	{  
		if ( m_buffers.Frames[ i ].YPlane.Allocate && m_buffers.Frames[ i ].YPlane.Buffer )
		{    
			// now allocate the pointer
			MemAlloc_FreeAligned( m_buffers.Frames[ i ].YPlane.Buffer );
			m_buffers.Frames[ i ].YPlane.Buffer  = NULL;
		}
        if ( m_buffers.Frames[ i ].cRPlane.Allocate && m_buffers.Frames[ i ].cRPlane.Buffer )
		{    
			// now allocate the pointer    
			MemAlloc_FreeAligned( m_buffers.Frames[ i ].cRPlane.Buffer );
			m_buffers.Frames[ i ].cRPlane.Buffer = NULL;
		}

		if ( m_buffers.Frames[ i ].cBPlane.Allocate && m_buffers.Frames[ i ].cBPlane.Buffer )
		{    
			// now allocate the pointer    
			MemAlloc_FreeAligned( m_buffers.Frames[ i ].cBPlane.Buffer );
			m_buffers.Frames[ i ].cBPlane.Buffer = NULL;
		}

#ifdef SUPPORT_BINK_ALPHA
		if ( m_buffers.Frames[ i ].APlane.Allocate && m_buffers.Frames[ i ].APlane.Buffer )  
		{
			// now allocate the pointer   
			MemAlloc_FreeAligned( m_buffers.Frames[ i ].APlane.Buffer );
			m_buffers.Frames[ i ].APlane.Buffer = NULL;
		}
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the current frame number of the video
//-----------------------------------------------------------------------------
int CBIKMaterial::GetFrame( void )
{
	AUTO_LOCK( m_BinkFrameCountMutex );

	return m_nCurrentFrame;
}

//-----------------------------------------------------------------------------
// Purpose: Pause playback
//-----------------------------------------------------------------------------
void CBIKMaterial::Pause( void )
{
	AUTO_LOCK( m_BinkUpdateMutex );

	BinkPause( m_pHBINK, 1 );
}

//-----------------------------------------------------------------------------
// Purpose: Resume playback
//-----------------------------------------------------------------------------
void CBIKMaterial::Unpause( void )
{	
	AUTO_LOCK( m_BinkUpdateMutex );

	BinkPause( m_pHBINK, 0 );
}

//-----------------------------------------------------------------------------
//
// Implementation of IAvi
//
//-----------------------------------------------------------------------------
class CBik : public CBaseAppSystem< IBik >
{
public:
	CBik();

	// Inherited from IAppSystem 
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Inherited from IBik
	virtual BIKMaterial_t CreateMaterial( const char *pMaterialName, const char *pFileName, const char *pPathID, int flags );
	virtual void DestroyMaterial( BIKMaterial_t hMaterial );
	virtual bool Update( BIKMaterial_t hMaterial );
	virtual bool ReadyForSwap( BIKMaterial_t hMaterial );
	virtual IMaterial* GetMaterial( BIKMaterial_t hMaterial );
	virtual void GetTexCoordRange( BIKMaterial_t hMaterial, float *pMaxU, float *pMaxV );
	virtual void GetFrameSize( BIKMaterial_t hMaterial, int *pWidth, int *pHeight );
	virtual int GetFrameRate( BIKMaterial_t hMaterial );
	virtual int GetFrame( BIKMaterial_t hMaterial );
	virtual void SetFrame( BIKMaterial_t hMaterial, float flFrame );
	virtual int GetFrameCount( BIKMaterial_t hMaterial );
#ifdef WIN32
#if !defined( _X360 )
	virtual bool SetDirectSoundDevice( void *pDevice );
	virtual bool SetMilesSoundDevice( void *pDevice );
#else
	virtual bool HookXAudio( void );
#endif
#endif

#if defined( _PS3 )
	virtual bool SetPS3SoundDevice( int nChannelCount );
#endif

	virtual void Pause( BIKMaterial_t hMaterial );
	virtual void Unpause( BIKMaterial_t hMaterial );

	virtual int GetGlobalMaterialAllocationNumber( void ) { return s_nMaterialAllocation; }
	
	virtual bool PrecacheMovie( const char *pFileName, const char *pPathID );
	virtual void *GetPrecachedMovie( const char *pFileName );
	virtual void EvictPrecachedMovie( const char *pFileName );
	virtual void EvictAllPrecachedMovies();

	void DumpPrecachedMovieList();

	virtual void UpdateVolume( BIKMaterial_t hMaterial );

	virtual bool IsMovieResidentInMemory( BIKMaterial_t hMaterial );

private:
	static void * RADLINK BinkMemAlloc( U32 bytes ) { return malloc( bytes ); };
	static void RADLINK BinkMemFree( void PTR4* ptr ) { free( ptr ); };
	
	// NOTE: Have to use pointers here since BIKMaterials inherit from ITextureRegenerator
	// The realloc screws up the pointers held to ITextureRegenerators in the material system.
	CUtlLinkedList< CBIKMaterial*, BIKMaterial_t > m_BIKMaterials;
	static int s_nMaterialAllocation;

	CUtlVector< PrecachedMovie_t > m_PrecachedMovies;
};


//-----------------------------------------------------------------------------
// Static variables
//-----------------------------------------------------------------------------
int CBik::s_nMaterialAllocation = 0;

//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CBik g_BIK;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CBik, IBik, BIK_INTERFACE_VERSION, g_BIK );

//-----------------------------------------------------------------------------
// Constructor/destructor
//-----------------------------------------------------------------------------
CBik::CBik()
{
}

//-----------------------------------------------------------------------------
// Connect/disconnect
//-----------------------------------------------------------------------------
bool CBik::Connect( CreateInterfaceFn factory )
{
	if ( IsGameConsole() )
	{
		return true;
	}
	
	ConnectTier1Libraries( &factory, 1 );
	ConnectTier2Libraries( &factory, 1 );
	if ( !(  g_pFullFileSystem && materials ) )
	{
		Msg( "Bik failed to connect to a required system\n" );
	}
	return (  g_pFullFileSystem && materials );
}

//-----------------------------------------------------------------------------
// Connect/disconnect
//-----------------------------------------------------------------------------
void CBik::Disconnect( void )
{
}

//-----------------------------------------------------------------------------
// Query Interface
//-----------------------------------------------------------------------------
void *CBik::QueryInterface( const char *pInterfaceName )
{
	if (!Q_strncmp(	pInterfaceName, BIK_INTERFACE_VERSION, Q_strlen(BIK_INTERFACE_VERSION) + 1))
		return (IBik*)this;

	return NULL;
}

//-----------------------------------------------------------------------------
// Init/shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CBik::Init()
{	
	BinkSetMemory( BinkMemAlloc, BinkMemFree );

	return INIT_OK;
}

void CBik::Shutdown()
{
#ifdef _PS3
	BinkFreeGlobals();
#endif // _PS3
}

//-----------------------------------------------------------------------------
// Create/destroy an BIK material
//-----------------------------------------------------------------------------
BIKMaterial_t CBik::CreateMaterial( const char *pMaterialName, const char *pFileName, const char *pPathID, int flags )
{
	// material names aren't filenames, they are expected to be adherent to forward slashes
	char fixedMaterialName[MAX_PATH];
	V_strncpy( fixedMaterialName, pMaterialName, sizeof( fixedMaterialName ) );
	V_FixSlashes( fixedMaterialName, '/' );

	BIKMaterial_t h = m_BIKMaterials.AddToTail();
	m_BIKMaterials[h] = new CBIKMaterial;
	if ( m_BIKMaterials[h]->Init( fixedMaterialName, pFileName, pPathID, flags ) == false )
	{
		delete m_BIKMaterials[h];
		m_BIKMaterials.Remove( h );
		return BIKMATERIAL_INVALID;
	}

	s_nMaterialAllocation++;

	return h;
}

void CBik::DestroyMaterial( BIKMaterial_t h )
{
	if ( h != BIKMATERIAL_INVALID )
	{
		if ( m_BIKMaterials[h]->Shutdown() )
		{
			delete m_BIKMaterials[h];
		}
		m_BIKMaterials.Remove( h );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : hMaterial - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBik::Update( BIKMaterial_t hMaterial )
{
	if ( hMaterial == BIKMATERIAL_INVALID )
		return false;

	return m_BIKMaterials[hMaterial]->Update();
}

bool CBik::ReadyForSwap( BIKMaterial_t hMaterial )
{
	Assert( hMaterial != BIKMATERIAL_INVALID );
	if ( hMaterial == BIKMATERIAL_INVALID )
	{
		return true;
	}

	return m_BIKMaterials[hMaterial]->ReadyForSwap();
}

//-----------------------------------------------------------------------------
// Gets the IMaterial associated with an BIK material
//-----------------------------------------------------------------------------
IMaterial* CBik::GetMaterial( BIKMaterial_t h )
{
	if ( h != BIKMATERIAL_INVALID )
		return m_BIKMaterials[h]->GetMaterial();
	
	return NULL;
}

//-----------------------------------------------------------------------------
// Returns the max texture coordinate of the BIK
//-----------------------------------------------------------------------------
void CBik::GetTexCoordRange( BIKMaterial_t h, float *pMaxU, float *pMaxV )
{
	if ( h != BIKMATERIAL_INVALID )
	{
		m_BIKMaterials[h]->GetTexCoordRange( pMaxU, pMaxV );
	}
	else
	{
		*pMaxU = *pMaxV = 1.0f;
	}
}

//-----------------------------------------------------------------------------
// Returns the frame size of the BIK (is a subrect of the material itself)
//-----------------------------------------------------------------------------
void CBik::GetFrameSize( BIKMaterial_t h, int *pWidth, int *pHeight )
{
	if ( h != BIKMATERIAL_INVALID )
	{
		m_BIKMaterials[h]->GetFrameSize( pWidth, pHeight );
	}
	else
	{
		*pWidth = *pHeight = 1;
	}
}

//-----------------------------------------------------------------------------
// Returns the frame size of the BIK (is a subrect of the material itself)
//-----------------------------------------------------------------------------
int CBik::GetFrameRate( BIKMaterial_t h )
{
	if ( h == BIKMATERIAL_INVALID )
		return -1;

	return m_BIKMaterials[h]->GetFrameRate();
}

//-----------------------------------------------------------------------------
// Returns the frame rate of the BIK
//-----------------------------------------------------------------------------
int CBik::GetFrameCount( BIKMaterial_t h )
{
	if ( h == BIKMATERIAL_INVALID )
		return -1;

	return m_BIKMaterials[h]->GetFrameCount();
}

//-----------------------------------------------------------------------------
// Sets the frame for an BIK material (use instead of SetTime)
//-----------------------------------------------------------------------------
void CBik::SetFrame( BIKMaterial_t h, float flFrame )
{
	if ( h != BIKMATERIAL_INVALID )
	{
		m_BIKMaterials[h]->SetFrame( flFrame );
	}
}

#if defined( WIN32 ) 
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pDevice - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
#if !defined( _X360 )
bool CBik::SetDirectSoundDevice( void *pDevice )
{
	g_bDisableVolumeChanges = false;

	return ( BinkSoundUseDirectSound( pDevice ) != 0 );
}

bool CBik::SetMilesSoundDevice( void *pDevice )
{
	if ( !pDevice )
	{
		g_bDisableVolumeChanges = true;
		return BinkSoundUseWaveOut() != 0;

	}
	g_bDisableVolumeChanges = false;
	return ( BinkSoundUseMiles( pDevice ) != 0 );
}

#else
bool CBik::HookXAudio( void )
{
	IXAudio2 *pXAudio2 = Audio_GetXAudio2();
	if ( !pXAudio2 )
	{
		// it better be there, it was when th
		Warning( "Bink playback not supported, init sequence of audio has been regressed." );
		return false;
	}
	return ( BinkSoundUseXAudio2( pXAudio2 ) != 0 );
}
#endif
#endif // WIN32

#if defined( _PS3 )
bool CBik::SetPS3SoundDevice( int nChannelCount )
{
	return BinkSoundUseLibAudio( nChannelCount );
}
#endif // _PS3

//-----------------------------------------------------------------------------
// Purpose: Gets the current frame from the Bink movie (for playback purposes)
//-----------------------------------------------------------------------------
int CBik::GetFrame( BIKMaterial_t hMaterial )
{
	if ( hMaterial != BIKMATERIAL_INVALID )
	{
		return m_BIKMaterials[hMaterial]->GetFrame();
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Pause the movie playback
//-----------------------------------------------------------------------------
void CBik::Pause( BIKMaterial_t hMaterial )
{
	if ( hMaterial != BIKMATERIAL_INVALID )
	{
		m_BIKMaterials[hMaterial]->Pause();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Resume movie playback
//-----------------------------------------------------------------------------
void CBik::Unpause( BIKMaterial_t hMaterial )
{
	if ( hMaterial != BIKMATERIAL_INVALID )
	{
		m_BIKMaterials[hMaterial]->Unpause();
	}
}

bool CBik::PrecacheMovie( const char *pFileName, const char *pPathID )
{
	if ( GetPrecachedMovie( pFileName ) )
	{
		// alread precached
		return true;
	}

	MEM_ALLOC_CREDIT();

	// must deal in absolute paths to ensure zip cull (media files are external)
	char pBIKFileName[ 512 ]; 
	char pFullBIKFileName[ 512 ];
	Q_snprintf( pBIKFileName, sizeof( pBIKFileName ), "%s", pFileName );
	Q_DefaultExtension( pBIKFileName, ".bik", sizeof( pBIKFileName ) );
	
	PathTypeQuery_t pathType;
	if ( !g_pFullFileSystem->RelativePathToFullPath( pBIKFileName, pPathID, pFullBIKFileName, sizeof( pFullBIKFileName ), GetMoviePathFilter(), &pathType ) )
	{
		// A file by that name was not found
		return false;
	}

	const char *pBaseName = V_UnqualifiedFileName( pFullBIKFileName );
	int iIndex = m_PrecachedMovies.AddToTail();
	m_PrecachedMovies[iIndex].m_BaseName = pBaseName;

	if ( !g_pFullFileSystem->ReadFile( pFullBIKFileName, NULL, m_PrecachedMovies[iIndex].m_MemoryBuffer ) )
	{
		m_PrecachedMovies.Remove( iIndex );
		return false;
	}

#if defined( PLAT_BIG_ENDIAN )
	// per Bink Docs
	DWORD *pBase = (DWORD *)m_PrecachedMovies[iIndex].m_MemoryBuffer.Base();
	int numDWords = m_PrecachedMovies[iIndex].m_MemoryBuffer.TellPut() / sizeof( DWORD );
	for ( int i = 0; i < numDWords; i++ )
	{
		pBase[i] = DWordSwap( pBase[i] );
	}
#endif

	return true;
}

void *CBik::GetPrecachedMovie( const char *pFileName )
{
	const char *pBaseName = V_UnqualifiedFileName( pFileName );
	for ( int i = 0; i < m_PrecachedMovies.Count(); i++ )
	{
		if ( !V_stricmp( m_PrecachedMovies[i].m_BaseName.Get(), pBaseName ) )
		{
			return m_PrecachedMovies[i].m_MemoryBuffer.Base();
		}
	}

	// not found
	return NULL;
}

void CBik::EvictPrecachedMovie( const char *pFileName )
{
	const char *pBaseName = V_UnqualifiedFileName( pFileName );
	for ( int i = 0; i < m_PrecachedMovies.Count(); i++ )
	{
		if ( !V_stricmp( m_PrecachedMovies[i].m_BaseName.Get(), pBaseName ) )
		{
			m_PrecachedMovies.Remove( i );
			break;
		}
	}
}

void CBik::EvictAllPrecachedMovies()
{
	m_PrecachedMovies.Purge();
}

void CBik::UpdateVolume( BIKMaterial_t hMaterial )
{
	if ( hMaterial != BIKMATERIAL_INVALID )
	{
		m_BIKMaterials[hMaterial]->UpdateVolume();
	}
}

bool CBik::IsMovieResidentInMemory( BIKMaterial_t hMaterial )
{
	if ( hMaterial != BIKMATERIAL_INVALID )
	{
		return m_BIKMaterials[hMaterial]->IsMovieResidentInMemory();
	}

	return false;
}

void CBik::DumpPrecachedMovieList()
{
	Msg( "-- %d precached bink movies -- \n", m_PrecachedMovies.Count() );
	for ( int i = 0; i < m_PrecachedMovies.Count(); ++ i )
	{
		Msg( "%d: %s, %d bytes\n", i, m_PrecachedMovies[i].m_BaseName.String(), m_PrecachedMovies[i].m_MemoryBuffer.Size() );
	}
}

CON_COMMAND( bink_dump_precached_movies, "Dumps information about all precached Bink movies" )
{
	g_BIK.DumpPrecachedMovieList();	
}


#endif

#endif // BINK_VIDEO
