//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#if defined( INCLUDE_SCALEFORM )

#if !defined( __OPTIONS_VIDEO_SCALEFORM_H__ )
#define __OPTIONS_VIDEO_SCALEFORM_H__
#ifdef _WIN32
#pragma once
#endif

#include "messagebox_scaleform.h"
#include "GameEventListener.h"
#include "options_scaleform.h"


class COptionsVideoScaleform : public COptionsScaleform
{
public:
	struct AAMode_t
	{
		int m_nNumSamples;
		int m_nQualityLevel;

		AAMode_t()
		{
			m_nNumSamples = 0;
			m_nQualityLevel = 0;
		}

		inline bool operator==( const AAMode_t& in ) const
		{
			return ( m_nNumSamples == in.m_nNumSamples ) && ( m_nQualityLevel == in.m_nQualityLevel );
		}
	};

	struct ResolutionModes_t
	{
		int m_nWidth;
		int m_nHeight;

		ResolutionModes_t()
		{
			m_nWidth = 0;
			m_nHeight = 0;
		}

		inline bool operator==( const ResolutionModes_t& in ) const
		{
			return ( m_nWidth == in.m_nWidth ) && ( m_nHeight == in.m_nHeight );
		}
	};

	struct AspectModes_t
	{
		CUtlVector<ResolutionModes_t>	m_vecResolutionModes; // The resolution modes compatible with this aspect ratio
	};

	COptionsVideoScaleform( );
	virtual ~COptionsVideoScaleform( );


	// IMessageBoxEventCallback implementation
	virtual bool OnMessageBoxEvent( MessageBoxFlags_t buttonPressed );

protected:
	virtual bool HandleUpdateChoice( OptionChoice_t * pChoice, int nCurrentChoice );

	// Sets the option to whatever the current ConVar value
	// bForceDefaultValue signals that unique algorithms should be used to select the value
	virtual void SetChoiceWithConVar( OptionChoice_t * pOption, bool bForceDefaultValue = false );

	// Resets all options and binds to their default values
	virtual void ResetToDefaults( void );

	// Fills m_vecAAModes with supported AA modes
	void BuildAAModes( CUtlVector<OptionChoiceData_t> &choices );

	// Returns sys_antialiasing matching provided values
	int  FindAAMode( int nAASamples, int nAAQuality );

	// Sets mat_antialias and mat_aaquality
	void SetAAMode( int nIndex );
	
	// Returns sys_refldetail  r_waterforceexpensive and r_waterforcereflectentities 
	int  FindReflection( void );

	// Sets r_waterforceexpensive and r_waterforcereflectentities 
	void SetReflection( int nIndex );

	// Returns sys_vsync assigned to provided values
	int FindVSync( bool bVsync, bool bTrippleBuffered );

	// Sets mat_vsync and mat_triplebuffered
	void SetVSync( int nIndex );

	// fills m_vecAspectModes with compatible resolutions for each aspect mode
	void GenerateCompatibleResolutions( void );

	// fills m_vecAspectModes with compatible windowed resolutions for each aspect mode
	void GenerateWindowedModes( CUtlVector< vmode_t > &windowedModes, int nCount, vmode_t *pFullscreenModes );

	// fills a choice vector with resolution options for the currently selected aspect mode (m_nSelectedAspect)
	void BuildResolutionModes( CUtlVector<OptionChoiceData_t> &choices );	

	// converts the resolution value to a string
	void GetResolutionName( int nWidth, int nHeight, char *pOutBuffer, int nOutBufferLength );	

	// returns appropriate aspect code for supplied resolution
	int GetScreenAspectRatio( int width, int height );													

	// returns an aspect code for the current resolution
	int FindCurrentAspectRatio();	

	// returns the current resolution
	const ResolutionModes_t FindCurrentResolution( void );

	// returns true if currently in full screen windowed mode
	bool FullScreenWindowMode( void );

	// changes the aspect ratio widget selection and fills m_pResolutionWidget with resolutions
	// appropriate for the selected aspect ratio
	void SelectAspectRatio( int nSelection );

	void SelectResolution( void );

	// if true then a warning dialog should be displayed before allowing this option to be changed
	bool ShowWarning( const char * szConVar );

	virtual void PreSaveChanges( void );

	// Disable widgets based on current state (brightness not available in windowed mode, etc)
	virtual void HandleDisableConditionalWidgets( Option_t * pOption, int & nWidgetIDOut, bool & bDisableOut );

	virtual bool InitUniqueWidget( const char * szWidgetID, OptionChoice_t * pOptionChoice  );

protected:
	CUtlVector<AAMode_t>			m_vecAAModes;			// List of AA modes supported by device
	CUtlVector<AspectModes_t>		m_vecAspectModes;		// List of aspect modes and their compatible resolutions
	
	OptionChoice_t *				m_pResolutionWidget;	// Pointer to the resolution widget
	int								m_nSelectedAspect;		// ID of currently selected aspect ratio
};

#endif // __OPTIONS_VIDEO_SCALEFORM_H__

#endif // INCLUDE_SCALEFORM
