//====== Copyright c 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "stdafx.h"

#include "cmdsyncmesh.h"
#include "cmdhandlers.h"

#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <direct.h>

#include <atlenc.h>

#include "hammer.h"
#include "Box3D.h"				// For units
#include "History.h"
#include "MainFrm.h"
#include "GlobalFunctions.h"
#include "MapDoc.h"
#include "MapView.h"

#include "ToolManager.h"

#include "smartptr.h"

#include "utlhash.h"
#include "generichash.h"
#include "utlbuffer.h"
#include "valve_ipc_win32.h"

#include "threadtools.h"

#include "mapsolid.h"
#include "mapentity.h"

#include "chunkfile.h"

#include "texturebrowser.h"

#include "vmfentitysupport.h"
#include "vmfmeshdatasupport.h"


//
// Fwd declarations
//

class CToolHandler_SyncMesh;


//
// Friendly structures declarations
//

class CMapDoc_Friendly : public CMapDoc
{
	friend class CToolHandler_SyncMesh;
};
static CMapDoc_Friendly * _AsFriendlyDoc( CDocument *pDoc )
{
	if ( pDoc )
	{
		ASSERT_KINDOF( CMapDoc, pDoc );
	}
	return static_cast< CMapDoc_Friendly * >( pDoc );
}

//
// Settings
//

static bool s_opt_vmf_bStoreMdl = true;
static bool s_opt_vmf_bStoreDmx = true;
static bool s_opt_vmf_bStoreMa = true;

static bool s_opt_maya_bMeshAtOrigin = false;
static bool s_opt_maya_bReplaceSelection = true;

static HWND s_hMainWnd = NULL;

//
// Utility routines
//

BOOL PrepareEmptyTempHammerDir( CString &sPath )
{
	// Create the temp directory
	char const *pszVproject = getenv( "VPROJECT" );
	if ( !pszVproject )
		return FALSE;

	sPath = pszVproject;
	sPath.Replace( '\\', '/' );
	sPath.Append( "/models/.hammer.tmp" );

	// Remove the folder
	if ( !access( sPath, 00) )
	{
		SHFILEOPSTRUCT sfo;
		memset( &sfo, 0, sizeof( sfo ) );
		sfo.hwnd = AfxGetMainWnd()->GetSafeHwnd();
		sfo.wFunc = FO_DELETE;
		sfo.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
		CArrayAutoPtr< char > bufPathCopy( new char [ sPath.GetLength() + 10 ] );
		sprintf( bufPathCopy.Get(), "%s/*.*%c%c", sPath, 0, 0 );
		Q_FixSlashes( bufPathCopy.Get() );
		sfo.pFrom = bufPathCopy.Get();
		if ( SHFileOperation( &sfo ) )
			return FALSE;
	}

	// Create it empty
	BOOL bDir = !mkdir( sPath ) || ( errno == EEXIST );
	if ( !bDir )
		return FALSE;

	return TRUE;
}

BOOL GetMdlCacheDir( CString &sPath )
{
	// Create the temp directory
	char const *pszVproject = getenv( "VPROJECT" );
	if ( !pszVproject )
		return FALSE;

	sPath = pszVproject;
	sPath.Replace( '\\', '/' );
	sPath.Append( "/models/.hammer.mdlcache" );

	// Create it empty
	BOOL bDir = !mkdir( sPath ) || ( errno == EEXIST );
	if ( !bDir )
		return FALSE;

	return TRUE;
}

CString GetSystemTempDir()
{
	CString sPath;

	if ( char const *pszTemp = getenv("TEMP") )
	{
		sPath = pszTemp;
	}
	else if ( char const *pszTmp = getenv("TMP") )
	{
		sPath = pszTmp;
	}
	else
	{
		sPath = "c:";
	}

	sPath.Replace( '\\', '/' );
	return sPath;
}

int ComputeBufferHash( const void *lpvBuffer, size_t numBytes )
{
	return HashBlock( lpvBuffer, numBytes );
}

int ComputeFileHash( const char *pszFilename )
{
	CUtlBuffer bufFile;
	
	if ( g_pFileSystem->ReadFile( pszFilename, NULL, bufFile ) )
		return ComputeBufferHash( bufFile.Base(), bufFile.TellPut() );
	
	return 0;
}

BOOL IsFileReadable( const char *pszFilename )
{
	return 0 == access( pszFilename, 04 );
}

CString GetOtherFileName( CString &sFile, char const *szNewExt, int nCurExtLen )
{
	return sFile.Left( sFile.GetLength() - nCurExtLen ) + szNewExt;
}

void SwitchActiveWindow( HWND hWndActivate )
{
	HWND hWndFg = GetForegroundWindow();
	if ( hWndActivate == hWndFg )
		return;

	DWORD dwThreadIdFg, dwThreadIdActivate;
	dwThreadIdFg = GetWindowThreadProcessId( hWndFg, NULL );
	dwThreadIdActivate = GetWindowThreadProcessId( hWndActivate, NULL );
	
	if ( dwThreadIdActivate != dwThreadIdFg )
		AttachThreadInput( dwThreadIdFg, dwThreadIdActivate, TRUE );

	SetForegroundWindow( hWndActivate );
	BringWindowToTop( hWndActivate );

	if ( dwThreadIdActivate != dwThreadIdFg )
		AttachThreadInput( dwThreadIdFg, dwThreadIdActivate, FALSE );

	if ( IsIconic( hWndActivate ) )
		ShowWindow( hWndActivate, SW_RESTORE );
	else
		ShowWindow( hWndActivate, SW_SHOW );
}

//
// Command handler classes
//

class CToolHandler_SyncMesh : public IToolHandlerInfo
{
public:
	CToolHandler_SyncMesh();

public:
	virtual BOOL UpdateCmdUI( CCmdUI *pCmdUI );
	virtual BOOL Execute( UINT uMsg );

public:
	BOOL RequestDmxLoad( char const *pszCookie, char const *pszDmxFile );
	BOOL RequestTexture( char const *pszTextureString );
	void AppMainLoopIdle();

protected:
	BOOL Setup();
	BOOL CanExecute();

protected:
	BOOL CopySelToClipboard();
	BOOL CreateTempDoc();
	BOOL PasteSelToTempDoc();
	BOOL CreateTempFileName();
	BOOL ExportTempDoc();
	BOOL CloseTempDoc();
	BOOL NotifyMaya();

protected:
	BOOL ProcessDmxRequest();
	BOOL DmxPrepareModelFiles();
	BOOL DmxDeleteOrigObjects();
	BOOL DmxCreateStaticProp();

protected:
	CWinApp *m_pApp;
	CMapDoc_Friendly *m_pDoc;
	CMapView *m_pView;
	CSelection *m_pSelection;

	CSelection m_origSelection;
	Vector m_vecSelectionCenter;

	IHammerClipboard *m_pCopiedObjects;
	CMapDoc_Friendly *m_pTempDoc;
	CString m_strTempFileName;

	CString m_strMayaRequest;
	CString m_strCookie;

	CString m_strReqCookie;
	CString m_strReqDmxFileName;
	CDialog *m_pAsyncDialogRequest;
	CThreadFastMutex m_mtxRequest;

	CString m_strDmxFileName;
	CString m_strMdlFileName;
}
g_ToolHandlerSyncMesh;

IToolHandlerInfo *g_pToolHandlerSyncMesh = &g_ToolHandlerSyncMesh;

static void AppMainLoopIdle_Delegate()
{
	g_ToolHandlerSyncMesh.AppMainLoopIdle();
}

CToolHandler_SyncMesh::CToolHandler_SyncMesh()
{
	m_pApp = NULL;
	m_pDoc = NULL;
	m_pView = NULL;
	m_pSelection = NULL;
	
	m_pCopiedObjects = NULL;
	m_pTempDoc = NULL;

	m_pAsyncDialogRequest = NULL;

	AppRegisterMessageLoopFn( AppMainLoopIdle_Delegate );
}

//////////////////////////////////////////////////////////////////////////
//
// IPC Server class
//
//////////////////////////////////////////////////////////////////////////

class CHammerIpcServer : public CValveIpcServerUtl
{
public:
	CHammerIpcServer() : CValveIpcServerUtl( "HAMMER_IPC_SERVER" )
	{
		AppRegisterPostInitFn( AppInit );
		AppRegisterPreShutdownFn( AppShutdown );
	}

	static void AppInit() { g_HammerIpcServer.EnsureRegisteredAndRunning(); }
	static void AppShutdown() { g_HammerIpcServer.EnsureStoppedAndUnregistered(); }

public:
	virtual BOOL ExecuteCommand( CUtlBuffer &cmd, CUtlBuffer &res );
}
g_HammerIpcServer;


//////////////////////////////////////////////////////////////////////////
//
// Async texture browser interaction
//
//////////////////////////////////////////////////////////////////////////

class CTextureBrowser_Async : public CTextureBrowser
{
public:
	CTextureBrowser_Async();

protected:
	virtual INT_PTR DoModal();

protected:
	virtual void OnResult( INT_PTR nModalResult );
};


//////////////////////////////////////////////////////////////////////////
//
// Tool handler implementation
//
//////////////////////////////////////////////////////////////////////////

void CToolHandler_SyncMesh::AppMainLoopIdle()
{
	s_hMainWnd = AfxGetMainWnd()->GetSafeHwnd();

	CDialog *pAsyncDlg = NULL;
	// Fetch requests
	{
		AUTO_LOCK( m_mtxRequest );

		if ( !m_strReqDmxFileName.IsEmpty() )
		{
			m_strDmxFileName = m_strReqDmxFileName;
			m_strReqDmxFileName.Empty();
			m_strReqCookie.Empty();
		}

		if ( m_pAsyncDialogRequest )
		{
			pAsyncDlg = m_pAsyncDialogRequest;
		}
	}

	if ( !m_strDmxFileName.IsEmpty() )
	{
		ProcessDmxRequest();
		m_strDmxFileName.Empty();
		SwitchActiveWindow( s_hMainWnd );
	}

	if ( pAsyncDlg )
	{
		SwitchActiveWindow( s_hMainWnd );
		pAsyncDlg->DoModal();
		m_pAsyncDialogRequest = NULL;
	}
}

BOOL CToolHandler_SyncMesh::UpdateCmdUI( CCmdUI *pCmdUI )
{
	s_hMainWnd = AfxGetMainWnd()->GetSafeHwnd();

	BOOL bEnabled =
		Setup() &&
		CanExecute()
		;
	pCmdUI->Enable( bEnabled );

	return TRUE;
}

BOOL CToolHandler_SyncMesh::Execute( UINT uMsg )
{
	// Execute the command
	BOOL bSuccess =
		Setup() &&
		CanExecute();
	if ( !bSuccess )
		return FALSE;

	// Determine if we are processing brushed primitives or model selection
	// also remember the selection
	m_origSelection = *m_pSelection;
	m_pSelection->GetBoundsCenter( m_vecSelectionCenter );

	bSuccess =
		CopySelToClipboard() &&
		CreateTempDoc() &&
		PasteSelToTempDoc() &&
		CreateTempFileName() &&
		ExportTempDoc();

	CloseTempDoc();
	
	if ( bSuccess )
	{
		NotifyMaya();
	}
	else
	{
		AfxMessageBox( "Failed to prepare mesh for Maya", MB_ICONSTOP | MB_OK );
	}

	// Set new tool to pointer
	ToolManager()->SetTool( TOOL_POINTER );
	return TRUE;
}

//
// Stage by stage implementation
//

BOOL CToolHandler_SyncMesh::Setup()
{
	m_pApp = AfxGetApp();
	if ( !m_pApp )
		return FALSE;

	m_pDoc = _AsFriendlyDoc( CMapDoc::GetActiveMapDoc() );
	if ( !m_pDoc )
		return FALSE;

	m_pView = m_pDoc->GetActiveMapView();
	if ( !m_pView )
		return FALSE;

	m_pSelection = m_pDoc->GetSelection();
	if ( !m_pSelection )
		return FALSE;

	if ( m_pSelection->IsEmpty() ||
		 m_pSelection->GetCount() < 1 )
		 return FALSE;

	m_pTempDoc = NULL;
	m_pCopiedObjects = NULL;
	m_strTempFileName.Empty();

	return TRUE;
}

BOOL CToolHandler_SyncMesh::CanExecute()
{
	if ( GetMainWnd()->IsShellSessionActive() )
		return FALSE;

	if ( ToolManager()->GetActiveToolID() == TOOL_FACEEDIT_MATERIAL )
		return FALSE;

	// Check selection objects
	const CMapObjectList *pList = m_pSelection->GetList();
	for ( int k = 0; k < pList->Count(); ++ k )
	{
		CMapClass *pElem = (CUtlReference< CMapClass >)pList->Element( k );
		
		MAPCLASSTYPE eType = pElem->GetType();
		( void ) eType;

		if ( pElem->IsMapClass( MAPCLASS_TYPE( CMapEntity ) ) )
		{
			if ( !static_cast< CMapEntity * >( pElem )->IsClass( "prop_static" ) )
				return FALSE;
			else
				continue;
		}
		else if ( pElem->IsMapClass( MAPCLASS_TYPE( CMapSolid ) ) )
			 continue;
		else
			return FALSE;
	}

	return TRUE;
}

BOOL CToolHandler_SyncMesh::CopySelToClipboard()
{
	m_pCopiedObjects = IHammerClipboard::CreateInstance();
	m_pDoc->Copy( m_pCopiedObjects );

	return TRUE;
}

BOOL CToolHandler_SyncMesh::CreateTempDoc()
{
	CDocTemplate *pTemplate = NULL;
	if ( POSITION pos = m_pApp->GetFirstDocTemplatePosition() )
	{
		pTemplate = m_pApp->GetNextDocTemplate( pos );
	}

	if ( !pTemplate )
		return FALSE;

	// Force a new document file created
	m_pTempDoc = _AsFriendlyDoc( pTemplate->OpenDocumentFile( NULL ) );
	if ( !m_pTempDoc )
		return FALSE;

	return TRUE;
}

BOOL CToolHandler_SyncMesh::PasteSelToTempDoc()
{
	// Force snapping off in the doc
	bool bSnapping = m_pTempDoc->IsSnapEnabled();
	if ( bSnapping )
		m_pTempDoc->OnMapSnaptogrid();

	// Perform the paste
	Vector vecPasteOffset( 0, 0, 0 ), vecSelCenter;
	m_pSelection->GetBoundsCenter( vecSelCenter );
	vecPasteOffset -= vecSelCenter;

	if ( !s_opt_maya_bMeshAtOrigin )
		vecPasteOffset = Vector( 0, 0, 0 );
	
	m_pTempDoc->Paste( m_pCopiedObjects, m_pTempDoc->GetMapWorld(),
		vecPasteOffset, QAngle(0, 0, 0), NULL, false, NULL);
	
	// Restore snapping mode
	if ( bSnapping )
		m_pTempDoc->OnMapSnaptogrid();

	return TRUE;
}

BOOL CToolHandler_SyncMesh::CreateTempFileName()
{
	m_strTempFileName = GetSystemTempDir() + "/hammer_geom.vmf";

	ATLTRACE( "[SyncMesh] Temp file name is '%s'\n", (LPCTSTR) m_strTempFileName );
	return TRUE;
}

BOOL CToolHandler_SyncMesh::ExportTempDoc()
{
	extern BOOL bSaveVisiblesOnly;
	CAutoPushPop< BOOL > _auto_bSaveVisiblesOnly( bSaveVisiblesOnly, FALSE );

	BOOL bSaved = m_pTempDoc->OnSaveDocument( m_strTempFileName );
	if ( !bSaved )
		return FALSE;

	m_strMayaRequest = "hammerBrush";

	return TRUE;
}

BOOL CToolHandler_SyncMesh::CloseTempDoc()
{
	if ( m_pCopiedObjects )
	{
		m_pCopiedObjects->Destroy();
		m_pCopiedObjects = NULL;
	}

	if ( m_pTempDoc )
	{
		m_pTempDoc->SetModifiedFlag( FALSE );
		m_pTempDoc->OnFileClose();
	}

	// Activate the view that used to be active
	if ( m_pView )
	{
		m_pDoc->SetActiveView( m_pView );
	}

	return TRUE;
}

BOOL CToolHandler_SyncMesh::NotifyMaya()
{
	CValveIpcClientUtl ipc( "MAYA_VST_UIHOOK_IPC_SERVER" );
	while ( !ipc.Connect() )
	{
		int iResponse = AfxMessageBox(
			"Cannot connect to Maya.\n"
			"Make sure you have Maya running and proper plug-ins loaded and try again.",
			MB_ICONWARNING | MB_RETRYCANCEL );
		if ( iResponse != IDRETRY )
			return FALSE;
	}

	// Update the cookie
	m_strCookie.Format( "%d", 1 + atoi( ( LPCTSTR ) m_strCookie ) );

	CUtlBuffer cmd;

	cmd.PutString( m_strMayaRequest );
	cmd.PutString( m_strCookie );
	cmd.PutString( m_strTempFileName );

	CUtlBuffer res;
	res.EnsureCapacity( 2 * MAX_PATH );
	
	if ( !ipc.ExecuteCommand( cmd, res ) )
		goto comm_error;

	int uCode = res.GetInt();
	switch ( uCode )
	{
	case 0: // Error
		{
			char chErrorString[ MAX_PATH ] = {0};
			sprintf( chErrorString, "Generic Error" );
			res.GetString( chErrorString, sizeof( chErrorString ) - 1 );
			AfxMessageBox( chErrorString, MB_ICONSTOP );
			return FALSE;
		}
	
	case 1: // OK
		return TRUE;
	
	default:
		goto comm_error;
	}

comm_error:
	AfxMessageBox(
		"Cannot communicate with Maya.\n"
		"Make sure you have Maya running and proper plug-ins loaded and try again.",
		MB_ICONSTOP | MB_OK );
	return FALSE;
}

BOOL CToolHandler_SyncMesh::RequestDmxLoad( char const *pszCookie, char const *pszDmxFile )
{
	if ( stricmp( pszCookie, m_strCookie ) )
		return FALSE;

	AUTO_LOCK( m_mtxRequest );
	m_strReqDmxFileName = pszDmxFile;
	m_strReqCookie = pszCookie;
	
	return TRUE;
}

BOOL CToolHandler_SyncMesh::RequestTexture( char const *pszTextureString )
{
	AFX_MANAGE_STATE( AfxGetStaticModuleState() );

	if ( ::GetLastActivePopup( s_hMainWnd ) != s_hMainWnd )
		return FALSE;

	{
		AUTO_LOCK( m_mtxRequest );
		if ( m_pAsyncDialogRequest )
			return FALSE;
		
		CTextureBrowser_Async *pBrowser = new CTextureBrowser_Async;
		if ( !pBrowser )
			return FALSE;
		m_pAsyncDialogRequest = pBrowser;
		
		pBrowser->SetTextureFormat( tfVMT );
		if ( pszTextureString )
		{
			pBrowser->SetInitialTexture( pszTextureString );
		}
	}

	return TRUE;
}

BOOL CToolHandler_SyncMesh::ProcessDmxRequest()
{
	CWaitCursor curWait;
	CUtlInplaceBuffer bufIndex( 0, 0, CUtlInplaceBuffer::TEXT_BUFFER );

	if ( !DmxDeleteOrigObjects() )
		goto failed;

	if ( !g_pFileSystem->ReadFile( m_strDmxFileName, NULL, bufIndex ) )
		goto failed;

	while ( char *pszEntry = bufIndex.InplaceGetLinePtr() )
	{
		if ( !*pszEntry )
			continue;

		m_strDmxFileName = pszEntry;
		m_strDmxFileName += ".dmx";

		if ( DmxPrepareModelFiles() )
			DmxCreateStaticProp();

	}

	m_strCookie.Format( "%d", 1 + atoi( ( LPCTSTR ) m_strCookie ) );	// TODO: for now we just bump the cookie and prevent subsequent import
	return TRUE;

failed:
	AfxMessageBox( "Failed to apply Maya-edited geometry", MB_ICONSTOP | MB_OK );
	return TRUE;
}

BOOL CToolHandler_SyncMesh::DmxPrepareModelFiles()
{
	if ( !s_opt_vmf_bStoreMdl )
		return TRUE;

	// Create the temp directory
	char const *pszVproject = getenv( "VPROJECT" );
	CString sPath;
	if ( !PrepareEmptyTempHammerDir( sPath ) )
		return FALSE;

	// Copy the dmx
	CString sDmx = sPath + "/mayamesh.dmx";
	if ( !CopyFile( m_strDmxFileName, sDmx, FALSE ) )
		return FALSE;

	// Create a sample .qc
	CString sQc = sPath + "/mayamesh.qc";
	if ( FILE *fQc = fopen( sQc, "wt" ) )
	{
		fprintf( fQc,
			" $modelname .hammer.tmp/studiomdl.mdl \n"
			" $scale 1.0 \n"
			" $body \"Body\" \"mayamesh.dmx\" \n"
			" $staticprop \n"
			" $upaxis y \n"
			" $sequence  \"idle\" \"mayamesh\" fps 30 \n"
			" $collisionmodel \"mayamesh.dmx\" { $automass $concave }\n"
			);
		fclose( fQc );
	}
	else
		return FALSE;

	// Compile the mdl
	CString sExeName;
		sExeName.Format( "%s/../bin/studiomdl.exe", pszVproject );
	CString sMdlCmdLine;
		sMdlCmdLine.Format( "%s/../bin/studiomdl.exe -nop4 -fastbuild \"%s\"",
		pszVproject, ( LPCTSTR ) sQc );
	
	STARTUPINFO si;
	memset( &si, 0, sizeof( si ) );
	si.cb = sizeof( si );
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;

	PROCESS_INFORMATION pi;
	memset( &pi, 0, sizeof( pi ) );
	
	// system( sMdlCmdLine );
	BOOL bCreatedProcess = CreateProcess(
		sExeName.GetBuffer(), sMdlCmdLine.GetBuffer(),
		NULL, NULL, FALSE, 0, NULL, NULL,
		&si, &pi );
	bCreatedProcess;
	
	if ( pi.hThread )
	{
		CloseHandle( pi.hThread );
		pi.hThread = NULL;
	}

	if ( pi.hProcess )
	{
		WaitForSingleObject( pi.hProcess, INFINITE );
		CloseHandle( pi.hProcess );
		pi.hProcess = NULL;
	}

	// We should have the model now
	CString sMdl = sPath + "/studiomdl.mdl";
	if ( !IsFileReadable( sMdl ) )
		return FALSE;

	m_strMdlFileName = sMdl;
	return TRUE;
}

BOOL CToolHandler_SyncMesh::DmxDeleteOrigObjects()
{
	// Make sure we are still in the same document and same selection
	if ( m_pDoc != m_origSelection.GetMapDoc() )
		 return FALSE;

	if ( s_opt_maya_bReplaceSelection )
	{
		m_pDoc->OnEditDelete();
	}
	else
	{
		GetHistory()->MarkUndoPosition( m_pSelection->GetList(), "Delete" );

		// Delete objects in selection
		const CMapObjectList &lst = *m_origSelection.GetList();
		for ( int k = 0; k < lst.Count(); ++ k )
		{
			CMapClass *pObj = (CUtlReference< CMapClass >)lst.Element( k );
			m_pDoc->DeleteObject( pObj );
		}
		m_pSelection->RemoveAll();
		m_pDoc->SetModifiedFlag();
	}

	return TRUE;
}

BOOL CToolHandler_SyncMesh::DmxCreateStaticProp()
{
	// Make sure we are still in the same document and same selection
	if ( m_pDoc != m_origSelection.GetMapDoc() )
		 return FALSE;

	// Compute model hash
	int iMdlHash = ComputeFileHash( m_strDmxFileName );
	if ( !iMdlHash )
		return FALSE;

	// Crack the DMX file coordinates
	float vecDmxOffset[3] = {0};
	if ( m_strDmxFileName.GetLength() > 37 &&
		 m_strDmxFileName[ m_strDmxFileName.GetLength() -  37 ] == '@' )
	{
		int arrV[4] = {0};
		for ( int k = 0; k < 4; ++ k )
		{
			CString sParse = m_strDmxFileName.Mid( m_strDmxFileName.GetLength() - 37 + 1 + 8 * k, 8 );
			arrV[k] = strtoul( sParse, NULL, 16 );
		}
		for ( int k = 1; k < 4; ++ k )
		{
			vecDmxOffset[ k - 1 ] = reinterpret_cast< float & >( arrV[ k ] );
		}
	}

	// Move the model to the mdl cache section
	CString sMdlPath;
	if ( !GetMdlCacheDir( sMdlPath ) )
		return FALSE;
	
	CString sMdlCacheFile;
	sMdlCacheFile.Format( "%s/%08X.mdl", ( LPCTSTR ) sMdlPath, iMdlHash );
	CString sMdlCacheFileRel = sMdlCacheFile.Mid( strlen( getenv( "VPROJECT" ) ) + 1 );

	if ( s_opt_vmf_bStoreMdl )
	{
		char const * arrFiles[] = { ".mdl", ".vvd", ".dx90.vtx", ".phy", ".ss2" };
		bool arrRequired[] =      {  true,   true,   true,        false,  false };
		for ( int j = 0; j < ARRAYSIZE( arrFiles ); ++ j )
		{
			CString sSrc = GetOtherFileName( m_strMdlFileName, arrFiles[ j ], 4 );
			CString sDest = GetOtherFileName( sMdlCacheFile, arrFiles[j], 4 );
			CopyFile( sSrc, sDest, FALSE );
			if ( !IsFileReadable( sDest ) && arrRequired[ j ] )
				return FALSE;
		}

		// Patch the MDL file
		if ( FILE *fp = fopen( sMdlCacheFile, "r+b" ) )
		{
			fseek( fp, 12, SEEK_SET );
			fprintf( fp, "%s%c%c", ( (LPCTSTR) sMdlCacheFileRel ) + strlen( "models/" ), 0, 0 );
			fclose( fp );
		}
		else
			return FALSE;
	}

	if ( s_opt_vmf_bStoreDmx )
	{
		// Copy the dmx file as well
		CString sDmxCacheFile;
		sDmxCacheFile.Format( "%s/%08X.dmx", ( LPCTSTR ) sMdlPath, iMdlHash );
		CopyFile( m_strDmxFileName, sDmxCacheFile, FALSE );
		if ( !IsFileReadable( sDmxCacheFile ) )
			 return FALSE;
	}

	if ( s_opt_vmf_bStoreMa )
	{
		// Copy the maya file as well
		CString sMayaCacheFile;
		sMayaCacheFile.Format( "%s/%08X.ma", ( LPCTSTR ) sMdlPath, iMdlHash );
		CopyFile( GetOtherFileName( m_strDmxFileName, ".ma", 4 ), sMayaCacheFile, FALSE );
		if ( !IsFileReadable( sMayaCacheFile ) )
			return FALSE;
	}

	// Create the static prop
	Vector vecEntityPos( vecDmxOffset[0], vecDmxOffset[1], vecDmxOffset[2] );
	if ( s_opt_maya_bMeshAtOrigin )
		vecEntityPos += m_vecSelectionCenter;

	CMapEntity *pEnt = m_pDoc->CreateEntity( "prop_static",
		vecEntityPos.x, vecEntityPos.y, vecEntityPos.z );
	pEnt->SetKeyValue( "model", sMdlCacheFileRel ); // TODO: proper DMX/MDL encoding

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
//
// Async texture browser implementation
//
//////////////////////////////////////////////////////////////////////////

CTextureBrowser_Async::CTextureBrowser_Async() :
	CTextureBrowser( AfxGetMainWnd() )
{
	NULL;
}

INT_PTR CTextureBrowser_Async::DoModal()
{
	INT_PTR nResult = CTextureBrowser::DoModal();
	OnResult( nResult );
	delete this;
	return nResult;
}

void CTextureBrowser_Async::OnResult( INT_PTR nModalResult )
{
	CValveIpcClientUtl ipc( "MAYA_VST_UIHOOK_IPC_SERVER" );
	if ( !ipc.Connect() )
		return;

	CUtlBuffer cmd;

	cmd.PutString( "textureSelected" );
	if ( nModalResult == IDOK )
	{
		cmd.PutString( m_cTextureWindow.szCurTexture );
	}
	else
	{
		cmd.PutString( "" );
	}

	CUtlBuffer res;
	res.EnsureCapacity( 2 * MAX_PATH );

	ipc.ExecuteCommand( cmd, res );
}


//////////////////////////////////////////////////////////////////////////
//
// Handling commands from Maya
//
//////////////////////////////////////////////////////////////////////////

BOOL CHammerIpcServer::ExecuteCommand( CUtlBuffer &cmd, CUtlBuffer &res )
{
	char szCmd[ MAX_PATH ] = {0};
	cmd.GetString( szCmd, sizeof( szCmd ) - 1 );

	if ( !stricmp( szCmd, "mayaDmx" ) )
	{
		cmd.GetString( szCmd, sizeof( szCmd ) - 1 );
		CString sCookie = szCmd;

		cmd.GetString( szCmd, sizeof( szCmd ) - 1 );
		char const *szDmxName = szCmd;

		if ( !g_ToolHandlerSyncMesh.RequestDmxLoad( sCookie, szDmxName ) )
		{
			res.PutInt( 0 );
			res.PutString( "Invalid mesh synchronization request!" );
			return TRUE;
		}
		
		res.PutInt( 1 );
		return TRUE;
	}

	if ( !stricmp( szCmd, "textureBrowse" ) )
	{
		cmd.GetString( szCmd, sizeof( szCmd ) - 1 );
		char const *szTextureName = szCmd;

		if ( !g_ToolHandlerSyncMesh.RequestTexture( szTextureName ) )
		{
			res.PutInt( 0 );
			res.PutString( "Cannot request texture!" );
			return TRUE;
		}

		res.PutInt( 1 );
		return TRUE;
	}

	return FALSE;
}


//////////////////////////////////////////////////////////////////////////
//
// Special implementation of custom load/save chunks for entities
//
//////////////////////////////////////////////////////////////////////////

class CSyncMesh_SaveLoadHandler : public CVmfMeshDataSupport_SaveLoadHandler
{
public:
	CSyncMesh_SaveLoadHandler();
	~CSyncMesh_SaveLoadHandler();

public:
	virtual char const *GetCustomSectionName() { return "meshdata"; }

public:
	virtual ChunkFileResult_t SaveVMF( CChunkFile *pFile, IMapEntity_SaveInfo_t *pSaveInfo );

protected:
	virtual ChunkFileResult_t OnFileDataLoaded( CUtlBuffer &bufData );
	virtual ChunkFileResult_t OnFileDataWriting( CChunkFile *pFile, char const *szHash );
};
static CSyncMesh_SaveLoadHandler g_syncmesh_saveloadhandler;

CSyncMesh_SaveLoadHandler::CSyncMesh_SaveLoadHandler()
{
	VmfInstallMapEntitySaveLoadHandler( this );
}

CSyncMesh_SaveLoadHandler::~CSyncMesh_SaveLoadHandler()
{
	NULL;
}

ChunkFileResult_t CSyncMesh_SaveLoadHandler::SaveVMF( CChunkFile *pFile, IMapEntity_SaveInfo_t *pSaveInfo )
{
	if ( !m_pEntity->IsClass( "prop_static" ) )
		return ChunkFile_Ok;

	LPCTSTR szModelName = m_pEntity->GetKeyValue( "model" );
	if ( !szModelName || !*szModelName )
		return ChunkFile_Ok;

	CString sMdlPath;
	if ( !GetMdlCacheDir( sMdlPath ) )
		return ChunkFile_Ok;

	CString sMdlRelPath = sMdlPath.Mid( strlen( getenv( "VPROJECT" ) ) + 1 ) + "/";
	if ( !StringHasPrefix( szModelName, sMdlRelPath ) )
		return ChunkFile_Ok;

	// Model is under our special cache path
	char szModelHash[ 16 ] = {0};
	sprintf( szModelHash, "%.8s", szModelName + sMdlRelPath.GetLength() );
	return WriteDataChunk( pFile, szModelHash );
}

ChunkFileResult_t CSyncMesh_SaveLoadHandler::OnFileDataWriting( CChunkFile *pFile, char const *szHash )
{
	ChunkFileResult_t eResult;

	LPCTSTR szModelName = m_pEntity->GetKeyValue( "model" );
	
	CString sMdlPath;
	GetMdlCacheDir( sMdlPath );

	CString sMdlRelPath = sMdlPath.Mid( strlen( getenv( "VPROJECT" ) ) + 1 ) + "/";

	char szModelHash[ 16 ] = {0};
	sprintf( szModelHash, "%.8s", szModelName + sMdlRelPath.GetLength() );

	// Write files
	char const * arrFiles[] = { ".ma", ".dmx", ".mdl", ".vvd", ".dx90.vtx", ".phy", ".ss2" };
	char const * arrNames[] = { "maa", "dmx", "mdl", "vvd", "vtx", "phy", "ss2" };
	bool bOptNames[] = { s_opt_vmf_bStoreMa, s_opt_vmf_bStoreDmx, s_opt_vmf_bStoreMdl, s_opt_vmf_bStoreMdl,
		s_opt_vmf_bStoreMdl, s_opt_vmf_bStoreMdl, s_opt_vmf_bStoreMdl };
	bool bOptRequired[] = { true, true, true, true, true, false };
	for ( int j = 0; j < ARRAYSIZE( arrFiles ); ++ j )
	{
		if ( !bOptNames[j] )
			continue;

		CString sSrc = sMdlPath + CString( "/" ) + CString( szModelHash ) + arrFiles[j];

		CUtlBuffer bufFile;
		if ( !g_pFileSystem->ReadFile( sSrc, NULL, bufFile ) )
		{
			if ( !bOptRequired[ j ] )
				continue;
			else
				return ChunkFile_OpenFail;
		}

		eResult = WriteBufferData( pFile, bufFile, arrNames[j] );
		if ( eResult != ChunkFile_Ok )
			return eResult;
	}

	return ChunkFile_Ok;
}

ChunkFileResult_t CSyncMesh_SaveLoadHandler::OnFileDataLoaded( CUtlBuffer &bufData )
{
	char const * arrFiles[] = { ".ma", ".dmx", ".mdl", ".vvd", ".dx90.vtx", ".phy", ".ss2" };
	char const * arrNames[] = { "maa", "dmx", "mdl", "vvd", "vtx", "phy", "ss2" };

	char const *pFileExt = NULL;

	// Determine the file name to save
	for ( int j = 0; j < ARRAYSIZE( arrFiles ); ++ j )
	{
		if ( !stricmp( m_hLoadHeader.sPrefix, arrNames[j] ) )
		{
			pFileExt = arrFiles[j];
			break;
		}
	}
	if ( !pFileExt )
		return ChunkFile_Fail;

	// The filename
	CString sSaveFileName;
	if ( !GetMdlCacheDir( sSaveFileName ) )
		return ChunkFile_Fail;
	sSaveFileName += "/";
	sSaveFileName += m_hLoadHeader.sHash;
	sSaveFileName += pFileExt;
	
	// We have file data, save it
	if ( FILE *f = fopen( sSaveFileName, "wb" ) )
	{
		fwrite( bufData.Base(), 1, bufData.TellPut(), f );
		fclose( f );
	}
	else
		return ChunkFile_Fail;

	return ChunkFile_Ok;
}

