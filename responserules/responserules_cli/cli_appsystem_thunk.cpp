/// These functions define the MANAGED interface. This file has /CLR.
///
/// Defines an interface which may be called from the MANAGED code's 
/// main function to start up (connect) and shut down (disconnect) 
/// the app system interface.

/// This class is a singleton. It gets manufactured explicitly 
/// from the Manufacture() call, which is a factory function that
/// your app must implement in a non-/CLR file it will presumably
/// return your custom type of CCLI_AppSystem_Adapter_Unmanaged). 
/// Calling Startup() will connect the app system and
/// Shutdown() will disconnect it. 

#include "stdafx.h"

#pragma unmanaged
#include "cli_appsystem_unmanaged_wrapper.h"
#pragma managed


// Allocate the native object on the C++ Heap via a constructor
ManagedAppSystem::AppSystemWrapper::AppSystemWrapper() 
{
	m_Impl = new AppSystemWrapper_Unmanaged( StrToAnsi(Environment::CommandLine) );
}

ManagedAppSystem::AppSystemWrapper::~AppSystemWrapper()
{
  delete m_Impl;
  m_Impl = NULL;
}

// Deallocate the native object on the finalizer just in case no destructor is called
ManagedAppSystem::AppSystemWrapper::!AppSystemWrapper() 
{
  delete m_Impl;
  m_Impl = NULL;
}

#pragma unmanaged
#include "cli_appsystem_adapter.h"
#pragma managed

void ManagedAppSystem::AppSystemWrapper::SetFileSystemSearchRoot( String ^path )
{
	m_Impl->Get()->AddFileSystemRoot( StrToAnsi(path) );
}
