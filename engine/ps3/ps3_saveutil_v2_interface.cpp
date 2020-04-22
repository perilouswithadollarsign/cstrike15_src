//===== Copyright © 1996-2011, Valve Corporation, All rights reserved. ======//

#include "ps3_saveutil_v2.h"
#include "memdbgon.h"


// the interface class
class CPS3SaveRestoreToUI : public CTier2AppSystem< IPS3SaveRestoreToUI >
{
public: // IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

public: // IPS3SaveRestoreToUI
	/// You have to call this before doing any other save operation. In particular,
	/// there may be an error opening the container. Poll on Async, and when it's done,
	/// look in the return value to see if it succeeded, or if not, why not. 
	/// When bCreateIfMissing is set, it will create a new container where none exists. 
	virtual void Initialize( CPS3SaveRestoreAsyncStatus *pAsync, IPS3SaveSteamInfoProvider *pSteamInfoProvider, bool bCreateIfMissing, int nKBRequired )
	{
		SaveUtilV2_Initialize( pAsync, pSteamInfoProvider, nKBRequired );
	}

	// Save the given file into the container. (ie /dev_hdd1/tempsave/foo.ps3.sav will 
	// be written into the container as foo.ps3.sav ). You can optionally specify a second
	// file to be written at the same time, eg a screenshot, because batching two writes
	// to happen at once is far far faster than having two batches one after another.
	// ALL game progress files must be 
	// written as secure; for now, even the screenshots should be, as that puts the work
	// of CRC onto the operating system. 
	virtual void Write( CPS3SaveRestoreAsyncStatus *pAsync, const char *pSourcepath, const char *pScreenshotPath, const char *pComment, FileSecurity_t nSecurity = kSECURE )
	{
		SaveUtilV2_Write( pAsync, pSourcepath, pScreenshotPath, pComment );
	}

	// A more complicated and intelligent form of save writing, specifically for the case of autosaves.
	// The source filename given (as an absolute path) will be written to "autosave.ps3.sav". 
	// Meanwhile, the existing "autosave.ps3.sav" will be renamed "autosave01.ps3.sav",
	// any "autosave01.ps3.sav" will be renamed "autosave02.ps3.sav", and so on up to a maximum
	// number of autosaves N, plus the base autosave.ps3.sav. The highest "autosave%02d.ps3.sav"
	// will therefore have a number N. Excess autosaves with numbers >N will be deleted.
	// If you specify a ScreenshotExtension (such as "tga"), the same operation is performed
	// for every file above, where ."sav" is replaced with the given extension. 
	virtual void WriteAutosave(  CPS3SaveRestoreAsyncStatus *pAsync, 
		const char *pSourcePath, // eg "/dev_hdd1/tempsave/autosave.ps3.sav"
		const char *pComment, // the comment field for the new autosave.
		const unsigned int nMaxNumAutosaves ) // should be at least 1; the highest numbered autosave will be N-1.
	{
		SaveUtilV2_WriteAutosave( pAsync, pSourcePath, pComment, nMaxNumAutosaves );
	}

	// A way of writing clouded files into container, clouded files over
	// a certain age are purged from container
	virtual void WriteCloudFile( CPS3SaveRestoreAsyncStatus *pAsync,
		const char *pSourcePath,
		const unsigned int nMaxNumCloudFiles ) // should be at least 1; the highest numbered cloud file will be N-1.
	{
		SaveUtilV2_WriteCloudFile( pAsync, pSourcePath, nMaxNumCloudFiles );
	}

	// Load a file from the container into the given directory. 
	// give it a pointer to a CPS3SaveRestoreAsyncStatus struct that you have created and
	// intend to poll.
	virtual void Load( CPS3SaveRestoreAsyncStatus *pAsync, const char *pFilename, const char *pDestFullPath )
	{
		SaveUtilV2_Load( pAsync, pFilename, pDestFullPath );
	}

	// kill one or two files (eg, save and screenshot). 
	// async will respond when done.
	virtual void Delete( CPS3SaveRestoreAsyncStatus *pAsync, const char *pFilename, const char *pOtherFilename = NULL )
	{
		SaveUtilV2_Delete( pAsync, pFilename );
	}

	// synchronously retrieve information on the files in the container. Lacks some of the container-wide'
	// info of the above function, and may have slightly out of date information, but is a synchronous call
	// and returns precisely the structure needed by CBaseModPanel::GetSaveGameInfos(). 
	virtual void GetFileInfoSync( CUtlVector< PS3SaveGameInfo_t > &saveGameInfos, bool bFindAll )
	{
		SaveUtilV2_GetFileInfoSync( saveGameInfos, bFindAll );
	}

	// try to find Steam's schema file for the user and stuff it into the container.
	// returns false if it couldn't find the file locally (in which case you should
	// not wait for the async object to be "done", as the job wasn't initiated); 
	// true if it found it locally and queued up an async job to write it.
	virtual void WriteSteamInfo( CPS3SaveRestoreAsyncStatus *pAsync )
	{
		SaveUtilV2_WriteSteamInfo( pAsync );
	}

	// returns whether save thread is busy
	virtual bool IsSaveUtilBusy()
	{
		return !!g_pSaveUtilAsyncStatus;
	}

	// returns the m_nCurrentOperationTag field of the most recent async op to
	// have run, or kSAVE_TAG_UNKNOWN if none has been enqueued yet. This tag
	// changes the moment a job is made active and remains until the next job
	// starts.
	virtual uint32 GetCurrentOpTag()
	{
		CPS3SaveRestoreAsyncStatus *pAsync = g_pSaveUtilAsyncStatus;
		if ( pAsync )
			return pAsync->m_nCurrentOperationTag;
		else
			return kSAVE_TAG_UNKNOWN;
	}

	// returns the version of container, used to fire off events when container
	// contents changes.
	virtual uint32 GetContainerModificationVersion()
	{
		return g_SaveUtilV2TOCVersion;
	}

	// sets the cloud crypto key.
	virtual void SetCloudFileCryptoKey( uint64 uiCloudCryptoKey )
	{
		g_uiSteamCloudCryptoKey = uiCloudCryptoKey;
	}
};

//////////////////////////////////////////////////////////////////////////

CPS3SaveRestoreToUI g_PS3SaveToUI;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CPS3SaveRestoreToUI, IPS3SaveRestoreToUI,
								  IPS3SAVEUIAPI_VERSION_STRING, g_PS3SaveToUI );


static CreateInterfaceFn s_pfnDelegateFactory;
static void * InternalFactory( const char *pName, int *pReturnCode )
{
	if ( pReturnCode )
	{
		*pReturnCode = IFACE_OK;
	}

	// Try to get interface via delegate
	if ( void *pInterface = s_pfnDelegateFactory ? s_pfnDelegateFactory( pName, pReturnCode ) : NULL )
	{
		return pInterface;
	}

	// Try to get internal interface
	if ( void *pInterface = Sys_GetFactoryThis()( pName, pReturnCode ) )
	{
		return pInterface;
	}

	// Failed
	if ( pReturnCode )
	{
		*pReturnCode = IFACE_FAILED;
	}
	return NULL;	
}


bool CPS3SaveRestoreToUI::Connect( CreateInterfaceFn factory )
{
	Assert( !s_pfnDelegateFactory );

	s_pfnDelegateFactory = factory;

	CreateInterfaceFn ourFactory = InternalFactory;
	ConnectTier1Libraries( &ourFactory, 1 );
	ConnectTier2Libraries( &ourFactory, 1 );

	s_pfnDelegateFactory = NULL;

	return true;
}

void CPS3SaveRestoreToUI::Disconnect()
{
	DisconnectTier2Libraries();
	DisconnectTier1Libraries();
}

void * CPS3SaveRestoreToUI::QueryInterface( const char *pInterfaceName )
{
	if ( !Q_stricmp( pInterfaceName, IPS3SAVEUIAPI_VERSION_STRING ) )
		return static_cast< IPS3SaveRestoreToUI * >( this );

	return NULL;
}

InitReturnVal_t CPS3SaveRestoreToUI::Init()
{
	return INIT_OK;
}

void CPS3SaveRestoreToUI::Shutdown()
{
	return;
}


