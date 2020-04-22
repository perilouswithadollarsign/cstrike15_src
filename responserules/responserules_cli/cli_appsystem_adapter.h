/// The local implementation of an AppSystem for this project

#ifndef CLI_APPSYSTEM_ADAPTER_H
#define CLI_APPSYSTEM_ADAPTER_H

#include "appframework/appframework.h"
#include "filesystem.h"
#include "vstdlib/random.h"
#include "icommandline.h"

// if you don't use the proper AppSystem to make a filesystem connection:
// #define TIER2_USE_INIT_DEFAULT_FILESYSTEM 1

/// A singleton class used to set up all the DLL interfaces.
/// EXTREMELY IMPORTANT: This class must  exist in unmanaged code.
//#pragma unmanaged
class IFileSystem;
class IUniformRandomStream;
class ICommandLine; 
class CCLIAppSystemAdapter : public CAppSystemGroup // , public ResponseRules_CLI::ICLI_AppSystem_Adapter
{
	//// UNMANAGED:
private:
	virtual bool Create();
	virtual bool PreInit();
	virtual int  Main() { return 0; } ///< never used, cannot be used
	virtual void PostShutdown() {Wipe(true);} ///< does it leak?
	virtual void Destroy() {};

	void Wipe( bool bPerformDelete );

	IUniformRandomStream *m_pLocalRandomStream;
#if TIER2_USE_INIT_DEFAULT_FILESYSTEM
#else
	IFileSystem *m_pFilesystem;
#endif

	// IUniformRandomStream *m_pRandomstream;
	// ICommandLine *m_pCommandline;

public:
	CCLIAppSystemAdapter();
	virtual ~CCLIAppSystemAdapter();
	void SetupFileSystem( ) ;
	/// Make the "LOCAL" filesystem directory point at the given path.
	void AddFileSystemRoot( const char *pPath ) ;

	IFileSystem * GetFilesytem();
	IUniformRandomStream * GetRandomStream();
	ICommandLine * GetCommandLine(); 

};


inline IUniformRandomStream * CCLIAppSystemAdapter::GetRandomStream() 
{ 
	return m_pLocalRandomStream; 
}
inline ICommandLine * CCLIAppSystemAdapter::GetCommandLine() 
{ 
	return CommandLine(); 
}


#endif