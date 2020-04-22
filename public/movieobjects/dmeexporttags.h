//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
//=============================================================================

#ifndef DMEEXPORTTAGS_H
#define DMEEXPORTTAGS_H

#include "datamodel/dmelement.h"

//=============================================================================
//
//=============================================================================
class CDmeExportTags : public CDmElement
{
	DEFINE_ELEMENT( CDmeExportTags, CDmElement );

public:
	// Initializes all fields by calling each Set* function with no arguments
	void Init( const char *pszApp = NULL, const char *pszAppVersion = NULL );

	// Sets the app field, if NULL, app is the empty string
	void SetApp( const char *pszVal = NULL );
	const char *GetApp() const { return m_sApp.GetString(); }

	// Sets the app version, if NULL, appVersion is the empty string
	void SetAppVersion( const char *pszVal = NULL );
	const char *GetAppVersion() const { return m_sAppVersion.GetString(); }

	// Sets the date, if NULL, date is set to the current date
	void SetDate( const char *pszVal = NULL );
	const char *GetDate() const { return m_sDate.GetString(); }

	// Sets the time, if NULL, time is set to the current date
	void SetTime( const char *pszVal = NULL );
	const char *GetTime() const { return m_sTime.GetString(); }

	// Sets the user, if NULL, time is set to the current user
	void SetUser( const char *pszVal = NULL );
	const char *GetUser() const { return m_sUser.GetString(); }

	// Sets the machine, if NULL, machine is set to the current machine
	void SetMachine( const char *pszVal = NULL );
	const char *GetMachine() const { return m_sMachine.GetString(); }

protected:
	CDmaString m_sApp;
	CDmaString m_sAppVersion;
	CDmaString m_sDate;
	CDmaString m_sTime;
	CDmaString m_sUser;
	CDmaString m_sMachine;

};

#endif // DMEEXPORTTAGS_H