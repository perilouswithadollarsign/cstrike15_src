// ======= Copyright (c) 2009, Valve Corporation, All rights reserved. =========
//
// appinstance.cpp
// 
// Purpose: Provide a simple way to enforce that only one instance of an
//          application is running on a machine at any one time.
//          
// Usage:  declare a global object of CSingleAppInstance type, with the unique name
//         you want to use wrapped in the TEXT( " " ) macro.
//
//		   upon entering main you can check the IsUniqueInstance() method to determine if another instance is running
//         or you can call the CheckForOtherRunningInstances() method to perform the check AND optinally 
//         pop up a message box to the user, and/or have the program terminate
//
// Example:
//
// CSingleAppInstance   g_ThisAppInstance( TEXT("this_source_app") );
//
// void main(int argc,char **argv)
// {
//     if ( g_ThisAppInstance.CheckForOtherRunningInstances() )  return;
//
//	   .. rest of code ..
//
// Notes:  Currently this object only works when IsPlatformWindows() is true
//         i.e. no Xbox 360, linux, or Mac yet..
//         (feel free to impliment)
//
// ===========================================================================




#include "tier0/platform.h"
#include "tier1/appinstance.h"
#include "tier1/strtools.h"
#include "tier0/dbg.h"

#include "tier0/memdbgon.h"

#ifdef PLATFORM_WINDOWS_PC 
#include <windows.h>
#endif
#ifdef PLATFORM_OSX
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "tier1/checksum_crc.h"
#endif



// ===========================================================================
//  Constructor - create a named mutex on the PC and see if someone else has
//     already done it.
// ===========================================================================
CSingleAppInstance::CSingleAppInstance( tchar* InstanceName, bool exitOnNotUnique, bool displayMsgIfNotUnique )
{
	// defaults for non-Windows builds
	m_hMutex = NULL;
	m_isUniqueInstance = true;
	
	if ( InstanceName == NULL || V_strlen( InstanceName ) == 0 || V_strlen( InstanceName ) >= MAX_PATH )
	{
		Assert( false );
		return;
	}

#ifndef _PS3
#ifdef WIN32
	if ( IsPlatformWindows() )
	{
		// don't allow more than one instance to run
		m_hMutex = ::CreateMutex( NULL, FALSE, InstanceName );

		unsigned int waitResult = ::WaitForSingleObject( m_hMutex, 0 );

		// Here, we have the mutex
		if ( waitResult == WAIT_OBJECT_0 || waitResult == WAIT_ABANDONED )
		{
			m_isUniqueInstance = true;		
			return;
		}

		// couldn't get the mutex, we must be running another instance
		::CloseHandle( m_hMutex );
		m_hMutex = NULL;
		

		// note that we are not unique, i.e. another instance of this app (or one using the same instance name) is running
		m_isUniqueInstance = false;
		
		CheckForOtherRunningInstances( exitOnNotUnique, displayMsgIfNotUnique );
	}
#elif defined(OSX)
	
	m_hMutex = -1; // 0 in theory is a valid fd, so set the sentinal to -1 for checking in the destructor
	
	// Under OSX use flock in /tmp/source_engine_<game>.lock, create the file if it doesn't exist
	CRC32_t gameCRC;
	CRC32_Init(&gameCRC);
	CRC32_ProcessBuffer( &gameCRC, (void *)InstanceName, Q_strlen( InstanceName ) );
	CRC32_Final( &gameCRC );
	
	V_snprintf( m_szLockPath, sizeof(m_szLockPath), "/tmp/source_engine_%lu.lock", gameCRC );
	m_hMutex = open( m_szLockPath, O_CREAT | O_WRONLY | O_EXLOCK | O_NONBLOCK | O_TRUNC, 0777 );
	if( m_hMutex >= 0 )
	{
		// make sure we give full perms to the file, we only one instance per machine
		fchmod( m_hMutex, 0777 );
		
		m_isUniqueInstance = true;		
		// we leave the file open, under unix rules when we die we'll automatically close and remove the locks
		return;
	}   		 
	
	// We were unable to open the file, it should be because we are unable to retain a lock
	if ( errno != EWOULDBLOCK)
	{
		fprintf( stderr, "unexpected error %d trying to exclusively lock %s\n", errno, m_szLockPath );
	}
	
	m_isUniqueInstance = false;		
#endif

#endif // _PS3
}



CSingleAppInstance::~CSingleAppInstance()
{
#ifndef _PS3
#ifdef WIN32
	if ( IsPlatformWindows() && m_hMutex )
	{
		::ReleaseMutex( m_hMutex );
		::CloseHandle( m_hMutex );
		m_hMutex = NULL;
	}
#elif defined(OSX)
	if ( m_hMutex != -1 )
	{
		close( m_hMutex );
		m_hMutex = -1;
		unlink( m_szLockPath ); 
	}
#endif
#endif // _PS3
}


bool CSingleAppInstance::CheckForOtherRunningInstances( bool exitOnNotUnique, bool displayMsgIfNotUnique )
{

	if ( IsPlatformWindows() || IsOSX() )
	{
		// are we the only running instance of this app?  Then we are Unique (aren't we SPECIAL?)
		if ( m_isUniqueInstance )
		{
			return false;
		}
	
		// should we display a message to the user?	
		if ( displayMsgIfNotUnique )
		{
			Plat_MessageBox( "Alert", "Another copy of this program is already running on this machine.  Only one instance at a time is allowed" );
		}
		
		// should we attempt normal program termination?
		if ( exitOnNotUnique )
		{
			exit( 0 );
		}
		
		return true;
	}

	// We fell through, so act like there are no other instances
	return false;
}



// ===========================================================================
// Static Function - this is used by *other* programs to query the system for 
//     a running program that grabs the named mutex/has a CSingleAppInstance
//     object of the specified name.  
//     It's a way to check if a tool is running or not, (and then presumably
//     lauch it if it is not there).
// ===========================================================================
bool CSingleAppInstance::CheckForRunningInstance( tchar* InstanceName )
{
#ifndef _PS3
	// validate input		
	Assert( InstanceName != NULL && V_strlen( InstanceName ) > 0 && V_strlen( InstanceName ) < MAX_PATH );

#ifdef WIN32
	if ( IsPlatformWindows() )
	{
		// don't allow more than one instance to run
		HANDLE hMutex = ::CreateMutex( NULL, FALSE, InstanceName );

		unsigned int waitResult = ::WaitForSingleObject( hMutex, 0 );

		::CloseHandle( hMutex );

		// Did we grab the mutex successfully?  nope...
		if ( waitResult == WAIT_OBJECT_0 || waitResult == WAIT_ABANDONED )
		{
			return false;
		}

		// couldn't get the mutex, must be another instance running somewhere that has it
		return true;
		
	}
#elif defined(OSX)
	// Under OSX use flock in /tmp/source_engine_<game>.lock, create the file if it doesn't exist
	CRC32_t gameCRC;
	CRC32_Init(&gameCRC);
	CRC32_ProcessBuffer( &gameCRC, (void *)InstanceName, Q_strlen( InstanceName ) );
	CRC32_Final( &gameCRC );
	
	char szLockPath[ MAX_PATH ];
	V_snprintf( szLockPath, sizeof(szLockPath), "/tmp/source_engine_%lu.lock", gameCRC );
	int lockFD = open( szLockPath, O_CREAT | O_WRONLY | O_EXLOCK | O_NONBLOCK | O_TRUNC, 0777 );
	if( lockFD >= 0 )
	{
		close( lockFD );
		unlink( szLockPath ); 
		return false;
	}   		 
	
	// We were unable to open the file, it should be because we are unable to retain a lock
	if ( errno != EWOULDBLOCK)
	{
		fprintf( stderr, "unexpected error %d trying to exclusively lock %s\n", errno, szLockPath );
	}
	
	// couldn't get the mutex, must be another instance running somewhere that has it
	return true;
#endif
	
#endif // _PS3

	return false;
}
