//===== Copyright (c) 2010, Valve Corporation, All rights reserved. ===========
//
// Purpose: 
//
//=============================================================================


#ifndef QUICKTIME_H
#define QUICKTIME_H

#ifdef _WIN32
#pragma once
#endif

#include <avi/iquicktime.h>

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IFileSystem;
class IMaterialSystem;
class CQuickTimeMaterial;

//-----------------------------------------------------------------------------
// Global interfaces - you already did the needed includes, right?
//-----------------------------------------------------------------------------
extern IFileSystem *g_pFileSystem;
extern IMaterialSystem *materials;


//-----------------------------------------------------------------------------
// Quicktime includes - conditional compilation on #define QUICKTIME in VPC
//   The intent is to have a default functionality fallback if not defined
//   which provides a dynamically generated texture (moving line on background)
//-----------------------------------------------------------------------------
#if defined( QUICKTIME_VIDEO )

  #if defined ( OSX )
	#include <quicktime/QTML.h>
	#include <quicktime/Movies.h>
  #elif defined ( WIN32 )
	#include <QTML.h>
	#include <Movies.h>
	#include <windows.h>
  #endif
	
#else
	typedef TimeValue long;
#endif



// -----------------------------------------------------------------------
// eVideoFrameFormat_t - bitformat for quicktime video frames
// -----------------------------------------------------------------------
enum eVideoFrameFormat_t
{
	cVFF_Undefined = 0,
	cVFF_R8G8B8A8_32Bit,
	cVFF_R8G8B8_24Bit,
	
	cVFF_Count,						// Auto list counter
	cVFF_ForceInt32	= 0x7FFFFFFF	// Make sure eNum is (at least) an int32
};



//-----------------------------------------------------------------------------
// texture regenerator - callback to get new movie pixels into the texture
//-----------------------------------------------------------------------------
class CQuicktimeMaterialRGBTextureRegenerator : public ITextureRegenerator
{
public:
	CQuicktimeMaterialRGBTextureRegenerator() :
		m_pQTMaterial( NULL ),
		m_nSourceWidth( 0 ),
		m_nSourceHeight( 0 )
	{
	}
	
	void SetParentMaterial( CQuickTimeMaterial *pQTMaterial, int nWidth, int nHeight )
	{
		m_pQTMaterial	= pQTMaterial;
		m_nSourceWidth	= nWidth;
		m_nSourceHeight = nHeight;
	}

	// Inherited from ITextureRegenerator
	virtual void RegenerateTextureBits( ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect );
	virtual void Release();

private:
	CQuickTimeMaterial *m_pQTMaterial;
	int					m_nSourceWidth;
	int					m_nSourceHeight;
};


//-----------------------------------------------------------------------------
//
// Class used to associated QuickTime video files with IMaterials
//
//-----------------------------------------------------------------------------
class CQuickTimeMaterial
{
	public:
		CQuickTimeMaterial();
		~CQuickTimeMaterial();

		// Initializes, shuts down the material
		bool Init( const char *pMaterialName, const char *pFileName, const char *pPathID );
		void Shutdown();

		// Keeps the frames updated
		bool Update( void );

		// Check if a new frame is available
		bool ReadyForSwap( void );

		// Returns the material
		IMaterial *GetMaterial();

		// Returns the texcoord range
		void GetTexCoordRange( float *pMaxU, float *pMaxV );

		// Returns the frame size of the QuickTime Video (stored in a subrect of the material itself)
		void GetFrameSize( int *pWidth, int *pHeight );

		// Sets the current time
		void SetTime( float flTime );

		// Returns the frame rate/count of the QuickTime Material
		int GetFrameRate( );
		int GetFrameCount( );

		// Sets the frame for an QuickTime material (use instead of SetTime) ??
		void SetFrame( float flFrame );
		
		void SetLooping( bool loop );

	private:
		friend class CQuicktimeMaterialRGBTextureRegenerator;

		void Reset();
		void OpenQTMovie( const char* theQTMovieFileName );
		void SetQTFileName( const char *theQTMovieFileName );
		void GetErrorFrame();

		void CloseQTFile();


		// Initializes, shuts down the procedural texture
		void CreateProceduralTexture( const char *pTextureName );
		void DestroyProceduralTexture();

		// Initializes, shuts down the procedural material
		void CreateProceduralMaterial( const char *pMaterialName );
		void DestroyProceduralMaterial();

		// Initializes, shuts down the video stream

		CQuicktimeMaterialRGBTextureRegenerator	m_TextureRegen;

		CMaterialReference			m_Material;						// Ref to Material used for rendering the video frame
		CTextureReference			m_Texture;						// Ref to the renderable texture which contains the most recent video frame (in a sub-rect)

		float						m_TexCordU;						// Max U texture coordinate of the texture sub-rect which holds the video frame
		float						m_TexCordV;						// Max V texture coordinate of the texture sub-rect which holds the video frame

		int							m_VideoFrameWidth;
		int							m_VideoFrameHeight;

		char					   *m_pFileName;

		char						m_TextureName[128];
		char						m_MaterialName[128];
		
		bool						m_bActive;
		bool						m_bLoopMovie;


		bool						m_bMoviePlaying;
		double						m_MovieBeganPlayingTime;
		double						m_MovieCurrentTime;
#if defined( QUICKTIME_VIDEO )
		
		// QuickTime Stuff
		Movie						m_QTMovie;
		TimeValue					m_QTMovieTimeScale;				// Units per second
		TimeValue					m_QTMovieDuration;				// movie duration is UPS
		float						m_QTMovieDurationinSec;			// movie duration in seconds
		long						m_QTMoveFrameRate;
		Rect						m_QTMovieRect;
		GWorldPtr					m_MovieGWorld;

		QTAudioContextRef			m_AudioContext;

		TimeValue					m_LastInterestingTimePlayed;
		TimeValue					m_NextInterestingTimeToPlay;

		// our Frame buffer stuff
  #if defined ( WIN32 )
		BITMAPINFO					m_BitmapInfo;
  #endif
	
#endif
		
		void					   *m_BitMapData;
		int							m_BitMapDataSize;
		
		bool						m_bIsErrorFrame;
		float						m_nLastFrameTime;

		static const int			cMaxQTFileNameLen = 255;

		static const int			cMinVideoFrameWidth = 16;
		static const int			cMinVideoFrameHeight = 16;
		static const int			cMaxVideoFrameWidth = 2048;
		static const int			cMaxVideoFrameHeight = 2048;
};




//-----------------------------------------------------------------------------
//
// Implementation of IQuickTime
//
//-----------------------------------------------------------------------------
class CQuickTime : public CBaseAppSystem<IQuickTime>
{
	public:
		CQuickTime();
		~CQuickTime();

		// Inherited from IAppSystem 
		virtual bool				Connect( CreateInterfaceFn factory );
		virtual void				Disconnect();
		virtual void			   *QueryInterface( const char *pInterfaceName );
		virtual InitReturnVal_t		Init();
		virtual void				Shutdown();

	// Inherited from IQuickTime
		virtual QUICKTIMEMaterial_t CreateMaterial( const char *pMaterialName, const char *pFileName, const char *pPathID, int flags = 0 );
		virtual void				DestroyMaterial( QUICKTIMEMaterial_t hMaterial );

		virtual bool				Update( QUICKTIMEMaterial_t hMaterial );
		virtual bool				ReadyForSwap(  QUICKTIMEMaterial_t hMaterial );
		
		virtual IMaterial		   *GetMaterial( QUICKTIMEMaterial_t hMaterial );
		virtual void				GetTexCoordRange( QUICKTIMEMaterial_t hMaterial, float *pMaxU, float *pMaxV );
		virtual void				GetFrameSize( QUICKTIMEMaterial_t hMaterial, int *pWidth, int *pHeight );
		virtual int					GetFrameRate( QUICKTIMEMaterial_t hMaterial );
		virtual void				SetFrame( QUICKTIMEMaterial_t hMaterial, float flFrame );
		virtual int					GetFrameCount( QUICKTIMEMaterial_t hMaterial );

		virtual bool				SetSoundDevice( void *pDevice );
		
	private:

		bool						SetupQuicktime();
		void						ShutdownQuicktime();
		
		// NOTE: Have to use pointers here since QuickTimeKMaterials inherit from ITextureRegenerator
		// The realloc screws up the pointers held to ITextureRegenerators in the material system.
		CUtlLinkedList< CQuickTimeMaterial*, QUICKTIMEMaterial_t > m_QTMaterials;
		
		bool						m_bQTInitialized;
};




#endif // QUICKTIME_H
