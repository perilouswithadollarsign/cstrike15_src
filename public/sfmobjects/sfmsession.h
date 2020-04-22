//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// A class representing session state for the SFM
//
//=============================================================================

#ifndef SFMSESSION_H
#define SFMSESSION_H
#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmehandle.h"
#include "datamodel/dmelement.h"
#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class CDmeFilmClip;
class CDmeClip;
struct studiohdr_t;
class Vector;
class Quaternion;
class CDmeGameModel;
class CDmeCamera;
class CDmeDag;
class CDmeAnimationSet;


//-----------------------------------------------------------------------------
// Camera creation paramaters
//-----------------------------------------------------------------------------
struct DmeCameraParams_t
{
	DmeCameraParams_t() : fov( 90.0f )
	{
		name[ 0 ] = 0;
		origin.Init();
		angles.Init();
	}

	DmeCameraParams_t( const char *pszName ) : fov( 90.0f )
	{
		Q_strncpy( name, pszName ? pszName : "", sizeof( name ) );
		origin.Init();
		angles.Init();
	}

	DmeCameraParams_t( const char *pszName, const Vector &org, const QAngle &ang ) :
		fov( 90.0f ), origin( org ), angles( ang )
	{
		Q_strncpy( name, pszName ? pszName : "", sizeof( name ) );
	}

	char	name[ 128 ];

	Vector	origin;
	QAngle	angles;

	float	fov;
};


//-----------------------------------------------------------------------------
// A class representing the SFM Session
// FIXME: Should this be a dmelement? Maybe!
//-----------------------------------------------------------------------------
class CSFMSession
{
public:
	CSFMSession();

	// Creates a new (empty) session
	void Init();
	void Shutdown();

	// Creates session settings
	void CreateSessionSettings();

	// Gets/sets the root
	CDmElement *Root();
	const CDmElement *Root() const;
	void SetRoot( CDmElement *pRoot );

	// Methods to get at session settings
	CDmElement *					GetSettings() const;
	template< class T > const T&	GetSettings( const char *pSettingName ) const;
	CDmAttribute *					GetSettingsAttribute( const char *pSettingName, DmAttributeType_t type );
	template< class E > E*			GetSettingsElement( const char *pSettingName ) const;
	template< class T > void		SetSettings( const char *pSettingName, const T& value );

	// Creates a game model
	CDmeGameModel *CreateEditorGameModel( studiohdr_t *hdr, const Vector &vecOrigin, Quaternion &qOrientation );

	// Creates a camera
	CDmeCamera *CreateCamera( const DmeCameraParams_t& params );

private:
	void CreateRenderSettings( CDmElement *pSettings );
	void CreateProgressiveRefinementSettings( CDmElement *pRenderSettings );
	void CreatePosterSettings( CDmElement *pRenderSettings );
	void CreateMovieSettings( CDmElement *pRenderSettings );
	void CreateSharedPresetGroupSettings( CDmElement *pRenderSettings );

	CDmeHandle< CDmElement > m_hRoot;
//	CDmeHandle< CDmeFilmClip >				m_hCurrentMovie;
//	CUtlVector< CDmeHandle< CDmeClip > >	m_hClipStack;
//	CUtlVector< CDmeHandle< CDmeClip > >	m_hSelectedClip[ NUM_SELECTION_TYPES ];
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline CDmElement *CSFMSession::Root()
{
	return m_hRoot;
}

inline const CDmElement *CSFMSession::Root() const
{
	return m_hRoot;
}


//-----------------------------------------------------------------------------
// Method to get at various settings
//-----------------------------------------------------------------------------

inline CDmElement *CSFMSession::GetSettings() const
{
	return m_hRoot.Get() ? m_hRoot->GetValueElement< CDmElement >( "settings" ) : NULL;
}

template< class T >
inline const T& CSFMSession::GetSettings( const char *pSettingName ) const
{
	CDmElement *pSettings = GetSettings();
	if ( pSettings )
		return pSettings->GetValue< T >( pSettingName );

	static T defaultVal;
	CDmAttributeInfo<T>::SetDefaultValue( defaultVal );
	return defaultVal;
}

inline CDmAttribute *CSFMSession::GetSettingsAttribute( const char *pSettingName, DmAttributeType_t type )
{
	CDmElement *pSettings = GetSettings();
	if ( pSettings )
		return pSettings->GetAttribute( pSettingName, type );
	return NULL;
}

template< class T > 
inline void CSFMSession::SetSettings( const char *pSettingName, const T& value )
{
	CDmElement *pSettings = GetSettings();
	if ( pSettings )
	{
		pSettings->SetValue( pSettingName, value );
	}
}

template< class E >
inline E* CSFMSession::GetSettingsElement( const char *pSettingName ) const
{
	CDmElement *pSettings = GetSettings();
	return pSettings ? pSettings->GetValueElement< E >( pSettingName ) : NULL;
}


#endif // SFMSESSION_H
