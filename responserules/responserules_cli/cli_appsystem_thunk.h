/// Defines an interface which may be called from the MANAGED code's 
/// main function to start up (connect) and shut down (disconnect) 
/// the app system interface.

/// This class is a singleton. It gets manufactured explicitly 
/// from the Manufacture() call, which is a factory function that
/// your app must implement in a non-/CLR file it will presumably
/// return your custom type of CCLI_AppSystem_Adapter_Unmanaged). 
/// Calling Startup() will connect the app system and
/// Shutdown() will disconnect it. 
///
/// This shim is necessary to hide the CAppSystemGroup header from the 
/// CLI compiler, because it'll freak out if it has to include it.
///
/// 
/// Placed here so that can be instanced from the app's main loop
/// after the dll loads; this workaround obviates having to write a
/// DLLMain() which might cause a loader-lock. 
/// see: http://msdn.microsoft.com/en-us/library/ms173266(vs.80).aspx
#ifndef CLI_APPSYSTEM_THUNK_H
#define CLI_APPSYSTEM_THUNK_H
#pragma once

class AppSystemWrapper_Unmanaged;

namespace ManagedAppSystem
{
	public ref class AppSystemWrapper 
	{
	public:
	   // Allocate the native object on the C++ Heap via a constructor
	   AppSystemWrapper() ; // : m_Impl( new UnmanagedClass ) {}
	   
	   // Deallocate the native object on a destructor
	   ~AppSystemWrapper(); 
	   
	   /// Set the "LOCAL" search path for the file system.
	   void SetFileSystemSearchRoot( String ^path );

	protected:
	   // Deallocate the native object on the finalizer just in case no destructor is called
	   !AppSystemWrapper(); 

	private:
	   AppSystemWrapper_Unmanaged * m_Impl;
	};

}




#endif