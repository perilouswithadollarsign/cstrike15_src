//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
//=============================================================================

#include "tier0/platform.h"

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#endif // PLATFORM_WINDOWS

#include "datamodel/dmelementfactoryhelper.h"
#include "tier1/fmtstr.h"

#include "movieobjects/dmeexporttags.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeExportTags, CDmeExportTags );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeExportTags::OnConstruction()
{
	m_sDate.Init( this, "date" );
	m_sTime.Init( this, "time" );
	m_sUser.Init( this, "user" );
	m_sMachine.Init( this, "machine" );
	m_sApp.Init( this, "app" );
	m_sAppVersion.Init( this, "appVersion" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeExportTags::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeExportTags::Init( const char *pszApp /* = NULL */, const char *pszAppVersion /* = NULL */ )
{
	SetApp( pszApp );
	SetAppVersion( pszAppVersion );

	// Avoid the issue of inconsistent date/time if run just as midnight approaches, SetDate/SetTime without args
	// query time twice, query it once and use the value for both time and date
	struct tm localTime;
	Plat_GetLocalTime( &localTime );

	SetDate( CFmtStr( "%04d/%02d/%02d", localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday ).Get() );

	int nHour = localTime.tm_hour % 12;
	if ( nHour == 0 )
	{
		nHour = 12;
	}

	SetTime( CFmtStr( "%02d:%02d:%02d %2s", nHour, localTime.tm_min, localTime.tm_sec, localTime.tm_hour > 11 ? "pm" : "am" ).Get() );

	SetUser();
	SetMachine();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeExportTags::SetApp( const char *pszVal /*= NULL */ )
{
	m_sApp.Set( pszVal );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeExportTags::SetAppVersion( const char *pszVal /* = NULL */ )
{
	m_sAppVersion.Set( pszVal );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeExportTags::SetDate( const char *pszVal /* = NULL */ )
{
	if ( pszVal )
	{
		m_sDate.Set( pszVal );
	}
	else
	{
		struct tm localTime;
		Plat_GetLocalTime( &localTime );
		m_sDate.Set( CFmtStr( "%04d/%02d/%02d", localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday ).Get() );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeExportTags::SetTime( const char *pszVal /* = NULL */ )
{
	if ( pszVal )
	{
		m_sTime.Set( pszVal );
	}
	else
	{
		struct tm localTime;
		Plat_GetLocalTime( &localTime );

		int nHour = localTime.tm_hour % 12;
		if ( nHour == 0 )
		{
			nHour = 12;
		}

		m_sTime.Set( CFmtStr( "%02d:%02d:%02d %2s", nHour, localTime.tm_min, localTime.tm_sec, localTime.tm_hour > 11 ? "pm" : "am" ).Get() );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeExportTags::SetUser( const char *pszVal /* = NULL */ )
{
	if ( pszVal )
	{
		m_sUser.Set( pszVal );
	}
	else
	{
#ifdef PLATFORM_WINDOWS
		char szUserName[ 256 ];
		DWORD nLen = ARRAYSIZE( szUserName );
		GetUserName( szUserName, &nLen );

		m_sUser.Set( szUserName );
#else // PLATFORM_WINDOWS
		m_sMachine.Set( NULL );
#endif // PLATFORM_WINDOWS
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeExportTags::SetMachine( const char *pszVal /* = NULL */ )
{
	if ( pszVal )
	{
		m_sMachine.Set( pszVal );
	}
	else
	{
#ifdef PLATFORM_WINDOWS
		char szComputerName[ 256 ];
		DWORD nLen = ARRAYSIZE( szComputerName );
		GetComputerName( szComputerName, &nLen );

		m_sMachine.Set( szComputerName );
#else // PLATFORM_WINDOWS
		m_sMachine.Set( NULL );
#endif // PLATFORM_WINDOWS
	}
}