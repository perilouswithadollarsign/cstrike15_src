// ======= Copyright (c) 2009, Valve Corporation, All rights reserved. =========
//
// appinstance.h
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


#ifndef APPINSTANCE_H
#define APPINSTANCE_H
#ifdef _WIN32
#pragma once
#endif


// check if handle is defined rather than inlcude another header
#ifndef HANDLE
typedef void *HANDLE;
#endif

class CSingleAppInstance
{
	public:
		explicit CSingleAppInstance( tchar* InstanceName, bool exitOnNotUnique = false, bool displayMsgIfNotUnique = false );
				~CSingleAppInstance();

		bool	CheckForOtherRunningInstances( bool exitOnNotUnique = false, bool displayMsgIfNotUnique = true );
		
		static bool CheckForRunningInstance( tchar* InstanceName );

		bool	IsUniqueInstance()	{ return m_isUniqueInstance; }
		HANDLE	GetHandle()			{ return reinterpret_cast< HANDLE >( (intp) m_hMutex ); }

	private:
		CSingleAppInstance();		// Hidden for a reason.  You must specify the instance name

#ifdef OSX
		char m_szLockPath[ MAX_PATH ];
		int	m_hMutex;
#else
		HANDLE	m_hMutex;
#endif
		bool	m_isUniqueInstance;
};

#endif
