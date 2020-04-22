//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#include "stdafx.h"
#include "hammer.h"
#include "mapview3d.h"
#include <direct.h>
#include "mapdoc.h"
#include "foundrytool.h"
#include "appframework/AppFramework.h"
#include "vphysics_interface.h"
#include "datacache/idatacache.h"
#include "toolutils/basetoolsystem.h"
#include "toolutils/recentfilelist.h"
#include "toolutils/toolmenubar.h"
#include "toolutils/toolswitchmenubutton.h"
#include "toolutils/tooleditmenubutton.h"
#include "toolutils/miniviewport.h"
#include "toolutils/toolfilemenubutton.h"
#include "toolutils/toolmenubutton.h"
#include "vgui_controls/Menu.h"
#include "tier1/KeyValues.h"
#include "toolutils/enginetools_int.h"
#include "toolframework/ienginetool.h"
#include "vgui/IInput.h"
#include "vgui/KeyCode.h"
#include "vgui_controls/FileOpenDialog.h"
#include "filesystem.h"
#include "vgui/ilocalize.h"
#include "dme_controls/elementpropertiestree.h"
#include "tier0/icommandline.h"
#include "materialsystem/imaterialsystem.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "toolutils/savewindowpositions.h"
#include "toolutils/toolwindowfactory.h"
#include "tier3/tier3.h"
#include "tier2/fileutils.h"
#include "vgui/ivgui.h"
#include "ihammer.h"
#include "mainfrm.h"
#include "vgui/keycode.h"
#include "saveinfo.h"
#include "foundry/iserverfoundry.h"
#include "mapworld.h"
#include "ToolManager.h"
#include "map_shared.h"
#include "scriplib.h"


using namespace vgui;


class CFoundryViewMenuButton;


//-----------------------------------------------------------------------------
// Singleton interfaces
//-----------------------------------------------------------------------------

const char *GetVGuiControlsModuleName()
{
	return "FoundryTool";
}

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
CreateInterfaceFn g_MainFactory = NULL;
bool ConnectTools( CreateInterfaceFn factory )
{
	g_MainFactory = factory;
	return (materials != NULL) && (g_pMatSystemSurface != NULL);
}

void DisconnectTools( )
{
}


//-----------------------------------------------------------------------------
// Implementation of the Foundry tool
//-----------------------------------------------------------------------------
class CFoundryTool : public CBaseToolSystem, public IServerFoundry, public IFileMenuCallbacks, public IFoundryTool
{
	DECLARE_CLASS_SIMPLE( CFoundryTool, CBaseToolSystem );

public:
	friend class CFoundryViewport;

	CFoundryTool();

	// Inherited from IToolSystem
	virtual const char *GetToolName() { return "Foundry"; }
	virtual const char *GetBindingsContextFile() { return "cfg/Foundry.kb"; }
	virtual bool	Init( );
    virtual void	Shutdown();
	virtual bool	CanQuit( const char *pExitMsg );
	virtual void	OnToolActivate();
	virtual void	OnToolDeactivate();
	virtual void*	QueryInterface( const char *pInterfaceName );
	virtual void	ClientLevelInitPostEntity();
	virtual void	ClientLevelShutdownPreEntity();

	virtual void	ClientPostRender();
	virtual bool	SetupEngineView( Vector &origin, QAngle &angles, float &fov );
	virtual bool	TrapKey( ButtonCode_t key, bool down );

	// Inherited from IServerFoundry.
	virtual bool	GetRestoredEntityReplacementData( int iHammerID, CUtlVector<char> &data );
	virtual void	OnFinishedRestoreSavegame();
	virtual void	MoveEntityTo( int nHammerID, const Vector &vPos, const QAngle &vAngles );
	virtual void	MoveHammerViewTo( const Vector &vPos, const QAngle &vAngles );
	virtual void	EngineGetMouseControl();
	virtual void	EngineReleaseMouseControl();
	virtual void	SelectEntities( int *pHammerIDs, int nIDs );
	virtual void	SelectionClickInCenterOfView( const Vector &vPos, const QAngle &vAngles );

	// Inherited from IFileMenuCallbacks
	virtual int		GetFileMenuItemsEnabled( );
	virtual void	AddRecentFilesToMenu( vgui::Menu *menu );
	virtual bool	GetPerforceFileName( char *pFileName, int nMaxLen );

	// Inherited from IFoundryDocCallback
	virtual void	OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags );
	virtual vgui::Panel *GetRootPanel() { return this; }

	// Inherited from CBaseToolSystem
	virtual vgui::HScheme GetToolScheme();
	virtual vgui::Menu *CreateActionMenu( vgui::Panel *pParent );
	virtual void OnCommand( const char *cmd );
	virtual const char *GetRegistryName() { return "FoundryTool"; }
	virtual vgui::MenuBar *CreateMenuBar( CBaseToolSystem *pParent );
	virtual void OnModeChanged();
	virtual CMiniViewport	*CreateMiniViewport( vgui::Panel *parent );

	// Inherited from IFoundryTool
	virtual void DestroyEntity( int iHammerID );
	virtual bool UpdateEntity( int iHammerID, CUtlVector<char*> &keys, CUtlVector<char*> &values );
	virtual void RespawnEntitiesWithEdits( CMapClass **ppEntities, int nEntities );
	virtual void MoveEngineViewTo( const Vector &vPos, const QAngle &vAngles );
	virtual void SwitchToEngine();
	virtual void ConsoleCommand( const char *pConCommand );
	virtual bool ShouldRender3DModels();
	virtual void OnMapDocDestroy( CMapDoc *pDoc );

public:
	MESSAGE_FUNC( OnNew, "OnNew" );
	MESSAGE_FUNC( OnOpen, "OnOpen" );
	MESSAGE_FUNC( OnSave, "OnSave" );
	MESSAGE_FUNC( OnSaveAs, "OnSaveAs" );
	MESSAGE_FUNC( OnClose, "OnClose" );
	MESSAGE_FUNC( OnCloseNoSave, "OnCloseNoSave" );
	MESSAGE_FUNC( OnMarkNotDirty, "OnMarkNotDirty" );
	MESSAGE_FUNC( OnExit, "OnExit" );

	// Commands related to the edit menu
	KEYBINDING_FUNC( undo, KEY_Z, vgui::MODIFIER_CONTROL, OnUndo, "#undo_help", 0 );
	KEYBINDING_FUNC( redo, KEY_Z, vgui::MODIFIER_CONTROL | vgui::MODIFIER_SHIFT, OnRedo, "#redo_help", 0 );
	void		OnDescribeUndo();

	// Methods related to the Foundry menu
	MESSAGE_FUNC( OnUpdateHammerEntity, "UpdateHammerEntity" );
	MESSAGE_FUNC( OnReload, "ReloadMap" );
	MESSAGE_FUNC( OnReloadFromSave, "ReloadFromSave" );
	MESSAGE_FUNC( OnReloadFromSaveSlamEnts, "ReloadFromSaveSlamEnts" );

	// Methods related to the view menu
	MESSAGE_FUNC( OnDefaultLayout, "OnDefaultLayout" );
	MESSAGE_FUNC( OnDrawHammerEntities, "OnDrawHammerEntities" );
	MESSAGE_FUNC( OnDrawHammerModels, "OnDrawHammerModels" );
	MESSAGE_FUNC( OnDrawEntityHighlights, "OnDrawEntityHighlights" );
	MESSAGE_FUNC( OnDrawGameEntities, "OnDrawGameEntities" );
	MESSAGE_FUNC( OnSyncHammerView, "OnSyncHammerView" );
	
	void		SetDefaultMiniViewportBounds( vgui::Panel *pMiniViewport );

	void		PerformNew();
	void		OpenFileFromHistory( int slot );
	void		OpenSpecificFile( const char *pFileName );
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual void OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues );

	// returns the document
	CMapDoc* GetDocument();

private:
	// Called by the engine on exit.
	static bool StaticQuitHandler( void *pvUserData );

	bool GetEntityVMFText( CMapClass *pClass, CUtlVector<char> &data );

		// Loads up a new document
	bool LoadDocument( const char *pDocName );

	// Updates the menu bar based on the current file
	void UpdateMenuBar( );

	void MiniViewport_OnMousePressed( vgui::MouseCode code );

	virtual const char *GetLogoTextureName();

	// Creates, destroys tools
	void CreateTools();
	void DestroyTools();

	// Initializes the tools
	void InitTools();

	// Shows, toggles tool windows
	void ToggleToolWindow( Panel *tool, char const *toolName );
	void ShowToolWindow( Panel *tool, char const *toolName, bool visible );

	// Kills all tool windows
	void DestroyToolContainers();

	// Used to hook DME VMF entities into the render lists
	void DrawVMFEntitiesInEngine( bool bDrawInEngine );

	void AddOriginalEntities( CUtlBuffer &entityBuf, const char *pActualEntityData );
	void AddVMFEntities( CUtlBuffer &entityBuf, const char *pActualEntityData );
	const char* GenerateEntityData( const char *pActualEntityData );
	const char* GetVMFFileName();
	bool IsDocumentDirty();

private:
	// Document
	CMapDoc *m_pMapDoc;		// The REAL document from Hammer.

	char m_pBSPFileName[MAX_PATH];

	// Hammer
	IHammer *m_pHammer;

	Vector m_v3dViewOrigin;
	QAngle m_v3dViewAngles;
	float m_fl3dViewFOV;

	// The menu bar
	CToolFileMenuBar *m_pMenuBar;
	CFoundryViewMenuButton *m_pViewMenuButton;

	// Separate undo context for the act busy tool
	CToolWindowFactory< ToolWindow > m_ToolWindowFactory;

	CUtlVector< DmElementHandle_t > m_toolElements;

	CUtlVector<int> m_SavegameRestoredEnts;
};



//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CFoundryTool	*g_pFoundryToolImp = NULL;
IFoundryTool	*g_pFoundryTool = NULL;

void CreateTools()
{
	g_pFoundryTool = g_pFoundryToolImp = new CFoundryTool();
}


static ConVar foundry_draw_hammer_models( "foundry_draw_hammer_models", "0", 0 );
static ConVar foundry_draw_hammer_entities( "foundry_draw_hammer_entities", "1", 0 );

static ConVar foundry_auto_pause( "foundry_auto_pause", "2", 0, "If 1, Foundry pauses the game when the engine window loses focus. If 2, Foundry disables AI when the engine window loses focus." );

static bool GetDrawEntitiesCvar()
{
	static ConVarRef val( "r_drawentities" );
	return val.GetBool();
}


CON_COMMAND( foundry_sync_engine_view, "Move engine's 3D view to the same position as Hammer's 3D view." )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
		return;

	POSITION p = pDoc->GetFirstViewPosition();
	while (p != NULL)
	{
		CMapView3D *pView = dynamic_cast< CMapView3D* >( pDoc->GetNextView(p) );
		if ( pView )
		{
			CCamera *pCamera = pView->GetCamera();

			Vector vPos;
			pCamera->GetViewPoint( vPos );

			QAngle vEngineAngles = pCamera->GetAngles();
			vEngineAngles[YAW] = -vEngineAngles[YAW] + 90.0f; // translate from Hammer's 3D renderer angles to engine angles
			g_pFoundryTool->MoveEngineViewTo( vPos, vEngineAngles );
			return;
		}
	}
}

CON_COMMAND( foundry_send_ents_to_engine, "Send selected entities in Hammer into the engine." )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !g_pFoundryTool || !pDoc )
		return;

	CSelection *pSelection = pDoc->GetSelection();
	if ( !pSelection || pSelection->IsEmpty() )
		return;

	CUtlVector<CMapClass*> toRespawn;

	const CMapObjectList *pObjectList = pSelection->GetList();
	for ( int i=0; i < pObjectList->Count(); i++ )
	{
		CMapClass *pClass = (CUtlReference< CMapClass >)pObjectList->Element( i );
		toRespawn.AddToTail( pClass );
	}

	g_pFoundryTool->RespawnEntitiesWithEdits( toRespawn.Base(), toRespawn.Count() );
}

CON_COMMAND( foundry_move_focus_to_engine, "Send focus to the engine." )
{
	HWND hWnd = (HWND)enginetools->GetEngineHwnd();
	
	RECT rcWindow;
	::GetWindowRect( hWnd, &rcWindow );
	
	SetCursorPos( (rcWindow.left + rcWindow.right) / 2, (rcWindow.top + rcWindow.bottom) / 2 );	
	::SetFocus( hWnd );
}


class CVisGroupEntList
{
public:
	CUtlVector<CMapClass*> m_Entities;
	CVisGroup *m_pGroup;
};

BOOL FindEntitiesInVisGroupCallback( CMapEntity *pObject, CVisGroupEntList *pList )
{
	CMapClass *pClass = dynamic_cast< CMapClass* >( pObject );

	if ( pClass && pClass->IsInVisGroup( pList->m_pGroup ) )
		pList->m_Entities.AddToTail( pClass );

	return TRUE;
}


CON_COMMAND( foundry_spawn_visgroup, "Spawn all the entities in the specified visgroup." )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !g_pFoundryTool || !pDoc )
		return;

	if ( args.ArgC() < 2 )
	{
		Warning( "Visgroup name required.\n" );
		return;
	}

	const char *pVisGroupName = args.Arg( 1 );
	CVisGroup *pVisGroup = pDoc->VisGroups_GroupForName( pVisGroupName, false );
	if ( !pVisGroup )
		pVisGroup = pDoc->VisGroups_GroupForName( pVisGroupName, true );

	if ( !pVisGroup )
	{
		Warning( "Can't find visgroup '%s'\n", pVisGroupName );
		return;
	}

	// Now find all objects.
	CVisGroupEntList theList;
	theList.m_pGroup = pVisGroup;
	pDoc->GetMapWorld()->EnumChildren( (ENUMMAPCHILDRENPROC)FindEntitiesInVisGroupCallback, (DWORD)&theList, MAPCLASS_TYPE(CMapEntity) );

	// Recreate them all.
	g_pFoundryTool->RespawnEntitiesWithEdits( theList.m_Entities.Base(), theList.m_Entities.Count() );
}

CON_COMMAND( foundry_remove_selected, "Remove selected entities." )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !g_pFoundryTool || !pDoc )
		return;

	CSelection *pSelection = pDoc->GetSelection();
	if ( !pSelection || pSelection->IsEmpty() )
		return;

	const CMapObjectList *pObjectList = pSelection->GetList();
	for ( int i=0; i < pObjectList->Count(); i++ )
	{
		CMapClass *pClass = (CUtlReference< CMapClass >)pObjectList->Element( i );

		// Update in Foundry if we are running in that mode
		if ( g_pFoundryTool && pClass )
		{
			servertools->RemoveEntity( pClass->GetHammerID() );
		}
	}
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CFoundryTool::CFoundryTool()
{
	m_pBSPFileName[0] = 0;

	m_pMenuBar = NULL;
	m_pMapDoc = NULL;
	
	m_pHammer = NULL;
	m_v3dViewOrigin.Init();
	m_v3dViewAngles.Init();
	m_fl3dViewFOV = 90;

	m_pViewMenuButton = NULL;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------

bool CFoundryTool::Init( )
{
	m_pMapDoc = NULL;
	m_RecentFiles.LoadFromRegistry( GetRegistryName() );

	// NOTE: This has to happen before BaseClass::Init
	g_pVGuiLocalize->AddFile( "resource/toolfoundry_%language%.txt" );

	if ( !BaseClass::Init( ) )
		return false;

	enginetools->InstallQuitHandler( this, &CFoundryTool::StaticQuitHandler );

	// Startup Hammer.
	m_pHammer = (IHammer*)Sys_GetFactoryThis()( INTERFACEVERSION_HAMMER, NULL );
	if ( !m_pHammer )
		Error( "Unable to load hammer_dll.dll" );

	char gamedir[MAX_PATH];
	enginetools->GetGameDir( gamedir, sizeof( gamedir ) );

	m_pHammer->InitFoundryMode( g_MainFactory, NULL, gamedir );
	return true;
}

void CFoundryTool::Shutdown()
{
	m_RecentFiles.SaveToRegistry( GetRegistryName() );

	{
		CDisableUndoScopeGuard guard;
		int nElements = m_toolElements.Count();
		for ( int i = 0; i < nElements; ++i )
		{
			g_pDataModel->DestroyElement( m_toolElements[ i ] );
		}
	}

	m_pMapDoc = NULL;

	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// returns the document
//-----------------------------------------------------------------------------
inline CMapDoc *CFoundryTool::GetDocument()
{
	return m_pMapDoc;
}

	
//-----------------------------------------------------------------------------
// Tool activation/deactivation
//-----------------------------------------------------------------------------
void CFoundryTool::OnToolActivate()
{
	BaseClass::OnToolActivate();
}

void CFoundryTool::OnToolDeactivate()
{
	BaseClass::OnToolDeactivate();
}


//-----------------------------------------------------------------------------
// Used to hook DME VMF entities into the render lists
//-----------------------------------------------------------------------------
void CFoundryTool::DrawVMFEntitiesInEngine( bool bDrawInEngine )
{
}

void CFoundryTool::ClientLevelInitPostEntity()
{
	BaseClass::ClientLevelInitPostEntity();
	DrawVMFEntitiesInEngine( true );
}

void CFoundryTool::ClientLevelShutdownPreEntity()
{
	DrawVMFEntitiesInEngine( false );
	BaseClass::ClientLevelShutdownPreEntity();
}

	
//-----------------------------------------------------------------------------
// Derived classes can implement this to get a new scheme to be applied to this tool
//-----------------------------------------------------------------------------
vgui::HScheme CFoundryTool::GetToolScheme() 
{ 
	return vgui::scheme()->LoadSchemeFromFile( "Resource/BoxRocket.res", "BoxRocket" );
}


//-----------------------------------------------------------------------------
//
// The View menu
//
//-----------------------------------------------------------------------------
class CFoundryViewMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CFoundryViewMenuButton, CToolMenuButton );
public:
	CFoundryViewMenuButton( CFoundryTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu(vgui::Menu *menu);

public:
	int m_menuitemidDrawHammerEntities;
	int m_menuitemidDrawHammerModels;
	int m_menuitemidDrawEntityHighlights;
	int m_menuitemidDrawGameEntities;

private:
	CFoundryTool *m_pTool;
};

CFoundryViewMenuButton::CFoundryViewMenuButton( CFoundryTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	m_pTool = parent;

	AddMenuItem( "defaultlayout", "#FoundryViewDefault", new KeyValues( "OnDefaultLayout" ), pActionSignalTarget );

	m_menuitemidDrawHammerEntities = AddCheckableMenuItem( "drawhammerentities", "#FoundryViewDrawHammerEntities", new KeyValues( "OnDrawHammerEntities" ), pActionSignalTarget );
	m_pMenu->SetMenuItemChecked( m_menuitemidDrawHammerEntities, foundry_draw_hammer_entities.GetBool() );
	
	m_menuitemidDrawHammerModels = AddCheckableMenuItem( "drawhammermodels", "#FoundryViewDrawHammerModels", new KeyValues( "OnDrawHammerModels" ), pActionSignalTarget );
	m_pMenu->SetMenuItemChecked( m_menuitemidDrawHammerModels, foundry_draw_hammer_models.GetBool() );
	
	m_menuitemidDrawEntityHighlights = AddCheckableMenuItem( "DrawEntityHighlights", "#FoundryViewDrawEntityHighlights", new KeyValues( "OnDrawEntityHighlights" ), pActionSignalTarget );
	m_pMenu->SetMenuItemChecked( m_menuitemidDrawEntityHighlights, true );

	m_menuitemidDrawGameEntities = AddCheckableMenuItem( "drawgameentities", "#FoundryViewDrawGameEntities", new KeyValues( "OnDrawGameEntities" ), pActionSignalTarget );
	m_pMenu->SetMenuItemChecked( m_menuitemidDrawGameEntities, GetDrawEntitiesCvar() );

	AddMenuItem( "synchammerview", "#FoundrySyncHammerView", new KeyValues( "OnSyncHammerView" ), pActionSignalTarget );

	SetMenu(m_pMenu);
}

void CFoundryViewMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );
/*
	// Update the menu
	int id;

	if ( m_pTool->GetDocument() )
	{
		id = m_Items.Find( "properties" );
		m_pMenu->SetItemEnabled( id, true );

		Panel *p;
		p = m_pTool->GetProperties();
		Assert( p );
		m_pMenu->SetMenuItemChecked( id, ( p && p->GetParent() ) ? true : false );

		id = m_Items.Find( "entityreport" );
		m_pMenu->SetItemEnabled( id, true );
		
		p = m_pTool->GetEntityReport();
		Assert( p );
		m_pMenu->SetMenuItemChecked( id, ( p && p->GetParent() ) ? true : false );
	}
	else
	{
		id = m_Items.Find( "properties" );
		m_pMenu->SetItemEnabled( id, false );
		id = m_Items.Find( "entityreport" );
		m_pMenu->SetItemEnabled( id, false );
	}
*/
}


//-----------------------------------------------------------------------------
//
// The Tool menu
//
//-----------------------------------------------------------------------------
class CFoundryToolMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CFoundryToolMenuButton, CToolMenuButton );
public:
	CFoundryToolMenuButton( CFoundryTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu(vgui::Menu *menu);

private:
	CFoundryTool *m_pTool;
};

CFoundryToolMenuButton::CFoundryToolMenuButton( CFoundryTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	m_pTool = parent;

	AddMenuItem( "updateentity", "#FoundryUpdateHammerEntity", new KeyValues( "UpdateHammerEntity" ), pActionSignalTarget );
	AddMenuItem( "reload", "#FoundryReload", new KeyValues( "ReloadMap" ), pActionSignalTarget );
	AddMenuItem( "reloadsave", "#FoundryReloadFromSave", new KeyValues( "ReloadFromSave" ), pActionSignalTarget );
	AddMenuItem( "reloadsaveslaments", "#FoundryReloadReplaceEnts", new KeyValues( "ReloadFromSaveSlamEnts" ), pActionSignalTarget );

	SetMenu(m_pMenu);
}

void CFoundryToolMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );

	// Update the menu
	int id;

	CMapDoc *pDoc = m_pTool->GetDocument();
	
	id = m_Items.Find( "reload" );
	m_pMenu->SetItemEnabled( id, pDoc != NULL );
	
	id = m_Items.Find( "reloadsave" );
	m_pMenu->SetItemEnabled( id, pDoc != NULL  );

	id = m_Items.Find( "reloadsaveslaments" );
	m_pMenu->SetItemEnabled( id, pDoc != NULL  );
}


//-----------------------------------------------------------------------------
// Initializes the menu bar
//-----------------------------------------------------------------------------
vgui::MenuBar *CFoundryTool::CreateMenuBar( CBaseToolSystem *pParent ) 
{
	m_pMenuBar = new CToolFileMenuBar( pParent, "Main Menu Bar" );

	// Sets info in the menu bar
	char title[ 64 ];
	ComputeMenuBarTitle( title, sizeof( title ) );
	m_pMenuBar->SetInfo( title );
	m_pMenuBar->SetToolName( GetToolName() );

	// Add menu buttons
	CToolMenuButton *pFileButton = CreateToolFileMenuButton( m_pMenuBar, "File", "&File", GetActionTarget(), this );
	CToolMenuButton *pEditButton = CreateToolEditMenuButton( this, "Edit", "&Edit", GetActionTarget() );
	CFoundryToolMenuButton *pToolButton = new CFoundryToolMenuButton( this, "Foundry", "F&oundry", GetActionTarget() );
	m_pViewMenuButton = new CFoundryViewMenuButton( this, "View", "&View", GetActionTarget() );
	CToolMenuButton *pSwitchButton = CreateToolSwitchMenuButton( m_pMenuBar, "Switcher", "&Tools", GetActionTarget() );

	m_pMenuBar->AddButton( pFileButton );
	m_pMenuBar->AddButton( pEditButton );
	m_pMenuBar->AddButton( pToolButton );
	m_pMenuBar->AddButton( m_pViewMenuButton );
	m_pMenuBar->AddButton( pSwitchButton );

	return m_pMenuBar;
}


// Acts like a viewport but passes mouse input into Foundry.
class CFoundryViewport : public CMiniViewport
{
public:
	DECLARE_CLASS_SIMPLE( CFoundryViewport, CMiniViewport );

	CFoundryViewport( CFoundryTool *pFoundryTool, vgui::Panel *pParent ) :
		CMiniViewport( pParent, "MiniViewport" )
	{
		m_pFoundryTool = pFoundryTool;
	}

	virtual void	OnMousePressed( vgui::MouseCode code )
	{
		m_pFoundryTool->MiniViewport_OnMousePressed( code );
	}

private:
	CFoundryTool *m_pFoundryTool;
};


CMiniViewport* CFoundryTool::CreateMiniViewport( vgui::Panel *parent )
{
	int w, h;
	surface()->GetScreenSize( w, h );

	CMiniViewport *vp = new CFoundryViewport( this, parent );
	Assert( vp );
	vp->SetVisible( true );

	SetDefaultMiniViewportBounds( vp );
	return vp;
}


void CFoundryTool::MiniViewport_OnMousePressed( vgui::MouseCode code )
{
	if ( !m_pMapDoc )
		return;

	CMapView3D *pView = m_pMapDoc->GetFirst3DView();
	if ( !pView )
		return;

	if ( code == MOUSE_LEFT )
	{
		int x, y;
		input()->GetCursorPos( x, y );
		ScreenToLocal( x, y );
		pView->Foundry_OnLButtonDown( x, y );
	}
}


//-----------------------------------------------------------------------------
// Updates the menu bar based on the current file
//-----------------------------------------------------------------------------
void CFoundryTool::UpdateMenuBar( )
{
	if ( !m_pMapDoc )
	{
		m_pMenuBar->SetFileName( "#FoundryNoFile" );
		return;
	}

	const char *pVMFFile = GetVMFFileName();
	if ( !pVMFFile[0] )
	{
		m_pMenuBar->SetFileName( "#FoundryNoFile" );
		return;
	}

	if ( IsDocumentDirty() )
	{
		char sz[ 512 ];
		Q_snprintf( sz, sizeof( sz ), "* %s", pVMFFile );
		m_pMenuBar->SetFileName( sz );
	}
	else
	{
		m_pMenuBar->SetFileName( pVMFFile );
	}
}



//-----------------------------------------------------------------------------
// Destroys all tool windows
//-----------------------------------------------------------------------------
void CFoundryTool::DestroyToolContainers()
{
	int c = ToolWindow::GetToolWindowCount();
	for ( int i = c - 1; i >= 0 ; --i )
	{
		ToolWindow *kill = ToolWindow::GetToolWindow( i );
		delete kill;
	}
}


//-----------------------------------------------------------------------------
// Sets up the default layout
//-----------------------------------------------------------------------------
void CFoundryTool::OnDefaultLayout()
{
	DestroyToolContainers();
}


void CFoundryTool::OnDrawHammerEntities()
{
	foundry_draw_hammer_entities.SetValue( !foundry_draw_hammer_entities.GetInt() );
	if ( m_pViewMenuButton )
	{
		vgui::Menu *pMenu = m_pViewMenuButton->GetMenu();
		if ( pMenu )
			pMenu->SetMenuItemChecked( m_pViewMenuButton->m_menuitemidDrawHammerEntities, foundry_draw_hammer_entities.GetBool() );
	}
}

void CFoundryTool::OnDrawHammerModels()
{
	foundry_draw_hammer_models.SetValue( !foundry_draw_hammer_models.GetInt() );
	if ( m_pViewMenuButton )
	{
		vgui::Menu *pMenu = m_pViewMenuButton->GetMenu();
		if ( pMenu )
			pMenu->SetMenuItemChecked( m_pViewMenuButton->m_menuitemidDrawHammerModels, foundry_draw_hammer_models.GetBool() );
	}
}

void CFoundryTool::OnDrawGameEntities()
{
	bool bDrawGameEntities = !GetDrawEntitiesCvar();

	// Update convar
	char szConCommand[50];

	V_snprintf( szConCommand, 50, "r_drawentities %i\n", (int)bDrawGameEntities); 
	enginetools->Command( szConCommand );

	// Update checkmark in menu
	if ( m_pViewMenuButton )
	{
		vgui::Menu *pMenu = m_pViewMenuButton->GetMenu();
		if ( pMenu )
		{
			pMenu->SetMenuItemChecked( m_pViewMenuButton->m_menuitemidDrawGameEntities, bDrawGameEntities );
		}
	}
}

void CFoundryTool::OnSyncHammerView()
{
	enginetools->Command( "foundry_sync_hammer_view\n" );
}

void CFoundryTool::OnDrawEntityHighlights()
{
	ConVar *pCv = ( ConVar * )cvar->FindVar( "cl_foundry_ShowEntityHighlights" );
	if ( pCv )
	{
		pCv->SetValue( !pCv->GetInt() );

		if ( m_pViewMenuButton )
		{
			vgui::Menu *pMenu = m_pViewMenuButton->GetMenu();
			if ( pMenu )
				pMenu->SetMenuItemChecked( m_pViewMenuButton->m_menuitemidDrawEntityHighlights, pCv->GetBool() );
		}
	}
}


void CFoundryTool::SetDefaultMiniViewportBounds( vgui::Panel *pMiniViewport )
{
	int menuBarY = m_pMenuBar->GetTall();

	int mainPanelWidth, mainPanelHeight;
	GetSize( mainPanelWidth, mainPanelHeight );

	int padding = 3;
	
	int left = padding;
	int top = menuBarY + padding;
	int width = mainPanelWidth - padding*2;
	int height = (mainPanelHeight - menuBarY) - top;

	pMiniViewport->SetBounds( left, top, width, height );
}


//-----------------------------------------------------------------------------
// Creates
//-----------------------------------------------------------------------------
void CFoundryTool::CreateTools()
{
/*
	if ( !m_hProperties.Get() )
	{
		m_hProperties = new CBasePropertiesContainer( NULL, NULL, NULL );
	}

	if ( !m_hEntityReport.Get() )
	{
		m_hEntityReport = new CEntityReportPanel( NULL, this, "EntityReportPanel" );
	}

	RegisterToolWindow( m_hProperties );
	RegisterToolWindow( m_hEntityReport );
*/
}


//-----------------------------------------------------------------------------
// Initializes the tools
//-----------------------------------------------------------------------------
void CFoundryTool::InitTools()
{
	// FIXME: There are no tool windows here; how should this work?
	// These panels are saved
	//windowposmgr->RegisterPanel( "properties", m_hProperties, false );
	//windowposmgr->RegisterPanel( "entityreport", m_hEntityReport, false );

	OnDefaultLayout();
	windowposmgr->LoadPositions( "cfg/foundry.txt", this, &m_ToolWindowFactory, "Foundry" );
}


void CFoundryTool::DestroyTools()
{
	int c = ToolWindow::GetToolWindowCount();
	for ( int i = c - 1; i >= 0 ; --i )
	{
		ToolWindow *kill = ToolWindow::GetToolWindow( i );
		delete kill;
	}

	UnregisterAllToolWindows();
}


void CFoundryTool::ShowToolWindow( Panel *tool, char const *toolName, bool visible )
{
	Assert( tool );

	if ( tool->GetParent() == NULL && visible )
	{
		m_ToolWindowFactory.InstanceToolWindow( this, false, tool, toolName, false );
	}
	else if ( !visible )
	{
		ToolWindow *tw = dynamic_cast< ToolWindow * >( tool->GetParent()->GetParent() );
		Assert( tw );
		tw->RemovePage( tool );
	}
}

void CFoundryTool::ToggleToolWindow( Panel *tool, char const *toolName )
{
	Assert( tool );

	if ( tool->GetParent() == NULL )
	{
		ShowToolWindow( tool, toolName, true );
	}
	else
	{
		ShowToolWindow( tool, toolName, false );
	}
}


//-----------------------------------------------------------------------------
// Creates the action menu
//-----------------------------------------------------------------------------
vgui::Menu *CFoundryTool::CreateActionMenu( vgui::Panel *pParent )
{
	vgui::Menu *pActionMenu = new Menu( pParent, "ActionMenu" );
	pActionMenu->AddMenuItem( "#ToolHide", new KeyValues( "Command", "command", "HideActionMenu" ), GetActionTarget() );
	return pActionMenu;
}

//-----------------------------------------------------------------------------
// Inherited from IFileMenuCallbacks
//-----------------------------------------------------------------------------
int	CFoundryTool::GetFileMenuItemsEnabled( )
{
	int nFlags = FILE_ALL & (~FILE_NEW);
	if ( m_RecentFiles.IsEmpty() )
	{
		nFlags &= ~FILE_RECENT;
	}
	return nFlags;
}

void CFoundryTool::AddRecentFilesToMenu( vgui::Menu *pMenu )
{
	m_RecentFiles.AddToMenu( pMenu, GetActionTarget(), "OnRecent" );
}

bool CFoundryTool::GetPerforceFileName( char *pFileName, int nMaxLen )
{
	if ( !m_pMapDoc )
		return false;

	Q_strncpy( pFileName, GetVMFFileName(), nMaxLen );
	return pFileName[0] != 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CFoundryTool::OnExit()
{
	windowposmgr->SavePositions( "cfg/foundry.txt", "Foundry" );

	enginetools->Command( "quit\n" );
}

//-----------------------------------------------------------------------------
// Handle commands from the action menu and other menus
//-----------------------------------------------------------------------------
void CFoundryTool::OnCommand( const char *cmd )
{
	if ( !V_stricmp( cmd, "HideActionMenu" ) )
	{
		if ( GetActionMenu() )
		{
			GetActionMenu()->SetVisible( false );
		}
	}
	else if ( const char *pSuffix = StringAfterPrefix( cmd, "OnRecent" ) )
	{
		int idx = Q_atoi( pSuffix );
		OpenFileFromHistory( idx );
	}
	else if ( const char *pSuffix = StringAfterPrefix( cmd, "OnTool" ) )
	{
		int idx = Q_atoi( pSuffix );
		enginetools->SwitchToTool( idx );
	}
	else if ( !V_stricmp( cmd, "OnUndo" ) )
	{
		OnUndo();
	}
	else if ( !V_stricmp( cmd, "OnRedo" ) )
	{
		OnRedo();
	}
	else if ( !V_stricmp( cmd, "OnDescribeUndo" ) )
	{
		OnDescribeUndo();
	}
	else
	{
		BaseClass::OnCommand( cmd );
	}
}


//-----------------------------------------------------------------------------
// Command handlers
//-----------------------------------------------------------------------------
void CFoundryTool::PerformNew()
{
	// Can never do new
	Assert( 0 );
}

void CFoundryTool::OnNew()
{
	if ( m_pMapDoc )
	{
		if ( IsDocumentDirty() )
		{
			SaveFile( GetVMFFileName(), "vmf", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY,
				new KeyValues( "OnNew" ) );
			return;
		}
	}
	PerformNew();
}

void CFoundryTool::OnOpen( )
{
	int nFlags = 0;
	const char *pSaveFileName = NULL;
	if ( IsDocumentDirty() )
	{
		nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
		pSaveFileName = GetVMFFileName();
	}

	OpenFile( "bsp", pSaveFileName, "vmf", nFlags );
}

bool CFoundryTool::OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	OnCloseNoSave();
	if ( !LoadDocument( pFileName ) )
		return false;
	m_RecentFiles.Add( pFileName, pFileFormat );
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	UpdateMenuBar();
	return true;
}

void CFoundryTool::OnSave()
{
	if ( m_pMapDoc )
	{
		SaveFile( NULL, "vmf", FOSM_SHOW_PERFORCE_DIALOGS );
	}
}

void CFoundryTool::OnSaveAs()
{
	if ( m_pMapDoc )
	{
		SaveFile( GetVMFFileName(), "vmf", FOSM_SHOW_PERFORCE_DIALOGS );
	}
}

bool CFoundryTool::OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	if ( !m_pMapDoc )
		return true;

	m_pMapDoc->SetPathName( pFileName );
	m_pMapDoc->SaveVMF( pFileName, 0 );

	m_RecentFiles.Add( pFileName, pFileFormat );
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	UpdateMenuBar();
	return true;
}

void CFoundryTool::OnClose()
{
	if ( IsDocumentDirty() )
	{
		SaveFile( GetVMFFileName(), "vmf", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY, 
			new KeyValues( "OnClose" ) );
		return;
	}

	OnCloseNoSave();
}

void CFoundryTool::OnCloseNoSave()
{
	
}

void CFoundryTool::OnMarkNotDirty()
{
	// FIXME: Implement
}


//-----------------------------------------------------------------------------
// Open a specific file
//-----------------------------------------------------------------------------
void CFoundryTool::OpenSpecificFile( const char *pFileName )
{
	if ( m_pMapDoc )
	{
		// TODO: close the MFC document here.
		m_pMapDoc = NULL;
	}

	int nFlags = 0;
	const char *pSaveFileName = NULL;

	OpenFile( pFileName, "bsp", pSaveFileName, "vmf", nFlags );
}


//-----------------------------------------------------------------------------
// Show the save document query dialog
//-----------------------------------------------------------------------------
void CFoundryTool::OpenFileFromHistory( int slot )
{
	const char *pFileName = m_RecentFiles.GetFile( slot );
	if ( !pFileName )
		return;
	OpenSpecificFile( pFileName );
}


//-----------------------------------------------------------------------------
// Derived classes can implement this to get notified when files are saved/loaded
//-----------------------------------------------------------------------------
void CFoundryTool::OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues )
{
	if ( bWroteFile )
	{
		OnMarkNotDirty();
	}

	if ( !pContextKeyValues )
		return;

	if ( state != FileOpenStateMachine::SUCCESSFUL )
		return;

	if ( !Q_stricmp( pContextKeyValues->GetName(), "OnNew" ) )
	{
		PerformNew();
		return;
	}

	if ( !Q_stricmp( pContextKeyValues->GetName(), "OnClose" ) )
	{
		OnCloseNoSave();
		return;
	}

	if ( !Q_stricmp( pContextKeyValues->GetName(), "OnQuit" ) )
	{
		OnCloseNoSave();
		vgui::ivgui()->PostMessage( GetVPanel(), new KeyValues( "OnExit" ), 0 );
		return;
	}

	if ( !Q_stricmp( pContextKeyValues->GetName(), "OnUnload" ) )
	{
		enginetools->Command( "toolunload foundry -nosave\n" );
		return;
	}
}


//-----------------------------------------------------------------------------
// Show the File browser dialog
//-----------------------------------------------------------------------------
void CFoundryTool::SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	char pStartingDir[ MAX_PATH ];

	// We open BSPs, but save-as VMFs
	if ( bOpenFile )
	{
		GetModSubdirectory( "maps", pStartingDir, sizeof(pStartingDir) );
		pDialog->SetTitle( "Choose Valve BSP File", true );
		pDialog->SetStartDirectoryContext( "foundry_bsp_session", pStartingDir );
		pDialog->AddFilter( "*.bsp", "Valve BSP File (*.bsp)", true );
	}
	else
	{
		GetModContentSubdirectory( "maps", pStartingDir, sizeof(pStartingDir) );
		pDialog->SetTitle( "Choose Valve VMF File", true );
		pDialog->SetStartDirectoryContext( "foundry_vmf_session", pStartingDir );
		pDialog->AddFilter( "*.vmf", "Valve VMF File (*.vmf)", true );
	}
}


//-----------------------------------------------------------------------------
// Can we quit?
//-----------------------------------------------------------------------------
bool CFoundryTool::CanQuit( const char *pExitMsg )
{
	if ( IsDocumentDirty() )
	{
		// Show Save changes Yes/No/Cancel and re-quit if hit yes/no
		SaveFile( GetVMFFileName(), "vmf", FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY, new KeyValues( pExitMsg ) );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Various command handlers related to the Edit menu
//-----------------------------------------------------------------------------
void CFoundryTool::OnUndo()
{
	CDisableUndoScopeGuard guard;
	g_pDataModel->Undo();
}

void CFoundryTool::OnRedo()
{
	CDisableUndoScopeGuard guard;
	g_pDataModel->Redo();
}

void CFoundryTool::OnDescribeUndo()
{
	CUtlVector< UndoInfo_t > list;
	g_pDataModel->GetUndoInfo( list );

	Msg( "%i operations in stack\n", list.Count() );

	for ( int i = list.Count() - 1; i >= 0; --i )
	{
		UndoInfo_t& entry = list[ i ];
		if ( entry.terminator )
		{
			Msg( "[ '%s' ] - %i operations\n", entry.undo, entry.numoperations );
		}

		Msg( "   +%s\n", entry.desc );
	}
}


//-----------------------------------------------------------------------------
// Foundry menu items
//-----------------------------------------------------------------------------
void CFoundryTool::OnReload()
{
	// Reloads the map, entities only, will reload every entity 
	enginetools->Command( "respawn_entities\n" );
}

void CFoundryTool::OnReloadFromSave()
{
	// Reloads the map from a save point, overrides selected entities
	// for now, this is hardcoded to be info_targets
	enginetools->Command( "load quick\n" );
}

void CFoundryTool::OnUpdateHammerEntity()
{
	enginetools->Command( "foundry_update_entity" );
}

void CFoundryTool::OnReloadFromSaveSlamEnts()
{
	m_SavegameRestoredEnts.Purge();
	enginetools->Command( "load quick * LetToolsOverrideLoadGameEnts\n" );
	enginetools->Execute();
}


//-----------------------------------------------------------------------------
// Background
//-----------------------------------------------------------------------------
const char *CFoundryTool::GetLogoTextureName()
{
	//return "vgui/tools/sampletool/sampletool_logo";
	return NULL;
}


//-----------------------------------------------------------------------------
// Inherited from IFoundryDocCallback
//-----------------------------------------------------------------------------
void CFoundryTool::OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	UpdateMenuBar();

	/*
	if ( bRefreshUI && m_hProperties.Get() )
	{
		m_hProperties->Refresh();
	}
	*/
}


//-----------------------------------------------------------------------------
// List of all entity classnames to copy over from the original block
//-----------------------------------------------------------------------------
static const char *s_pUseOriginalClasses[] =
{
	"worldspawn",
	"func_occluder",
	NULL
};


//-----------------------------------------------------------------------------
// Always copy the worldspawn and other entities that had data built into them by VBSP out
//-----------------------------------------------------------------------------
void CFoundryTool::AddOriginalEntities( CUtlBuffer &entityBuf, const char *pActualEntityData )
{
	while ( *pActualEntityData )
	{
		pActualEntityData = strchr( pActualEntityData, '{' );
		if ( !pActualEntityData )
			break;

		const char *pBlockStart = pActualEntityData;

		pActualEntityData = strstr( pActualEntityData, "\"classname\"" );
		if ( !pActualEntityData )
			break;

		// Skip "classname"
		pActualEntityData += 11;

		pActualEntityData = strchr( pActualEntityData, '\"' );
		if ( !pActualEntityData )
			break;

		// Skip "
		++pActualEntityData;

		char pClassName[512];
		int j = 0;
		while (*pActualEntityData != 0 && *pActualEntityData != '\"' )
		{
			pClassName[j++] = *pActualEntityData++;	
		}
		pClassName[j] = 0;

		pActualEntityData = strchr( pActualEntityData, '}' );
		if ( !pActualEntityData )
			break;

		// Skip }
		++pActualEntityData;

		for ( int i = 0; s_pUseOriginalClasses[i]; ++i )
		{
			if ( !Q_stricmp( pClassName, s_pUseOriginalClasses[i] ) )
			{
				// Found one we need to keep, add it to the buffer
				int nBytes = (int)( (size_t)pActualEntityData - (size_t)pBlockStart );
				entityBuf.Put( pBlockStart, nBytes );
				entityBuf.PutChar( '\n' );
				break;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Copy in other entities from the editable VMF
//-----------------------------------------------------------------------------
void CFoundryTool::AddVMFEntities( CUtlBuffer &entityBuf, const char *pActualEntityData )
{
}

bool CFoundryTool::IsDocumentDirty()
{
	return m_pMapDoc && m_pMapDoc->IsModified();
}

//-----------------------------------------------------------------------------
// Create a text block the engine can parse containing the entity data to spawn
//-----------------------------------------------------------------------------
const char* CFoundryTool::GenerateEntityData( const char *pActualEntityData )
{
	return pActualEntityData;
}


const char* CFoundryTool::GetVMFFileName()
{
	if ( m_pMapDoc )
		return m_pMapDoc->GetPathName();
	else
		return "";
}


//-----------------------------------------------------------------------------
// Loads up a new document
//-----------------------------------------------------------------------------
bool CFoundryTool::LoadDocument( const char *pFileName )
{
	Assert( !m_pMapDoc );

	DestroyTools();

	// Store the BSP file name
	Q_strncpy( m_pBSPFileName, pFileName, sizeof( m_pBSPFileName ) );

	// Construct VMF file name from the BSP
	const char *pGame = Q_stristr( pFileName, "\\game\\" );
	if ( !pGame )
		return false;

	// Compute the map name
	char mapname[ 256 ];
	const char *pMaps = Q_stristr( pFileName, "\\maps\\" );
	if ( !pMaps )
		return false;

	Q_strncpy( mapname, pMaps + 6, sizeof( mapname ) );

	int nLen = (int)( (size_t)pGame - (size_t)pFileName ) + 1;
	char vmfFilename[MAX_PATH];
	Q_strncpy( vmfFilename, pFileName, nLen );
	Q_strncat( vmfFilename, "\\content\\", sizeof(vmfFilename) );
	Q_strncat( vmfFilename, pGame + 6, sizeof(vmfFilename) );
	Q_SetExtension( vmfFilename, ".vmf", sizeof(vmfFilename) );

	// Have Hammer load this VMF.
	CHammer *pApp = (CHammer*) AfxGetApp();
	m_pMapDoc = (CMapDoc*)pApp->pMapDocTemplate->OpenDocumentFile( vmfFilename );	


	// Now have the engine load the map.
	char cmd[ 256 ];
	Q_snprintf( cmd, sizeof( cmd ), "disconnect; map %s\n", mapname );
	enginetools->Command( cmd );
	enginetools->Execute( );


	ShowMiniViewport( true );

	CreateTools();
	InitTools();
	return true;
}


//-----------------------------------------------------------------------------
// Create the entities that are in our VMF file
//-----------------------------------------------------------------------------

void* CFoundryTool::QueryInterface( const char *pInterfaceName )
{
	if ( V_stricmp( pInterfaceName, VSERVERFOUNDRY_INTERFACE_VERSION ) == 0 )
		return (IServerFoundry*)this;
	
	return NULL;
}


// This simulates passing entity data through VBSP. We should share the code with VBSP here if the format changes
// and this gets complicated, but it would involve lots of ugly #ifdef HAMMER's chopping up map.cpp.
static bool TransformEntityVMFToBSPFormat( char *pIn, CUtlVector<char> &data )
{
	ParseFromMemory( pIn, V_strlen( pIn ) );

	// Ignore the leading "entity {"
	// Also ignore everything inside "editor"
	if ( !GetToken(true) || V_stricmp(token, "entity") != 0 || !GetToken(true) || V_stricmp(token,"{") != 0 )
	{
		Warning( "Unknown entity format.\n" );
		return false;
	}

	bool bInsideEditorBlock = false;
	bool bValidFile = false;
	int braceLevel = 1;
	while ( 1 )
	{
		char firstToken[MAXTOKEN];
		GetToken( true );
		V_strncpy( firstToken, token, sizeof( firstToken ) );
		if ( firstToken[0] == '}' )
		{
			--braceLevel;
			if ( braceLevel == 0 )
			{
				// If we end the file and we're not somehow in the editor block, then the file is good.
				bValidFile = !bInsideEditorBlock;
				break;
			}
			else
			{
				bInsideEditorBlock = false;
			}
			continue;
		}

		if ( !GetToken( true ) )
			break;

		if ( token[0] == '{' )
		{
			++braceLevel;
			if ( V_stricmp( firstToken, "editor" ) == 0 )
				bInsideEditorBlock = true;

			continue;
		}

		if ( bInsideEditorBlock )
			continue;

		char outStr[MAXTOKEN*2+16];
		V_snprintf( outStr, sizeof( outStr ), "\"%s\" \"%s\"\n", firstToken, token );
		data.AddMultipleToTail( V_strlen( outStr ), outStr );
	}

	if ( !bValidFile )
	{
		Warning( "Invalid entity format.\n" );
		return false;
	}

	data.AddToTail( '}' );
	data.AddToTail( 0 ); // Null-terminate our string.
	return true;
}


static bool LoadFileDataIntoBuffer( const char *pFilename, const char *pFormat, CUtlVector<char> &tempData )
{
	FILE *fp = fopen( pFilename, pFormat );
	if ( !fp )
	{
		return false;
	}
	fseek( fp, 0, SEEK_END );
	tempData.SetSize( ftell( fp ) + 1 );
	fseek( fp, 0, SEEK_SET );
	fread( tempData.Base(), 1, tempData.Count()-1, fp );
	fclose( fp );
	
	return true;
}


bool CFoundryTool::GetEntityVMFText( CMapClass *pClass, CUtlVector<char> &data )
{
	char baseDir[MAX_PATH], cheesyFilename[MAX_PATH];
	_getcwd( baseDir, sizeof( baseDir ) );
	V_ComposeFileName( baseDir, "__foundry_tempchunk.txt", cheesyFilename, sizeof( cheesyFilename ) );

	// Save this entity's data into a temporary file.
	CSaveInfo saveInfo;
	saveInfo.SetVisiblesOnly( false );

	CChunkFile chunkFile;
	chunkFile.Open( cheesyFilename, ChunkFile_Write );
	ChunkFileResult_t result = pClass->SaveVMF( &chunkFile, &saveInfo );
	chunkFile.Close();
	if ( result != ChunkFile_Ok )
	{
		return false;
	}

	// Load the temporary file into memory.
	CUtlVector<char> tempData;
	if ( !LoadFileDataIntoBuffer( cheesyFilename, "rt", tempData ) )
	{
		Warning( "Internal error in GetEntityVMFText\n" );
		return false;
	}

	DeleteFile( cheesyFilename );

	// Convert from the VMF-format entity to the BSP-format entity.
	return TransformEntityVMFToBSPFormat( tempData.Base(), data );
}

bool CFoundryTool::GetRestoredEntityReplacementData( int iHammerID, CUtlVector<char> &data )
{
	CMapDoc *pDoc = g_pFoundryToolImp->GetDocument();
	CSelection *pSelection = pDoc->GetSelection();
	if ( !pSelection || pSelection->IsEmpty() )
		return false;

	const CMapObjectList *pObjectList = pSelection->GetList();
	for ( int i=0; i < pObjectList->Count(); i++ )
	{
		CMapClass *pClass = (CUtlReference< CMapClass >)pObjectList->Element( i );

		if ( pClass->GetID() == iHammerID )
		{
			if ( GetEntityVMFText( pClass, data ) )
			{
				m_SavegameRestoredEnts.AddToTail( pClass->GetID() );
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	
	return false;
}


void CFoundryTool::OnFinishedRestoreSavegame()
{
	CUtlVector<CMapClass*> toRespawn;

	// We're restoring a savegame and slamming any ents that are selected.
	// The savegame is finished, so now slam entities that are selected but weren't in the savegame.
	CMapDoc *pDoc = g_pFoundryToolImp->GetDocument();
	CSelection *pSelection = pDoc->GetSelection();
	if ( pSelection && !pSelection->IsEmpty() )
	{
		const CMapObjectList *pObjectList = pSelection->GetList();
		for ( int i=0; i < pObjectList->Count(); i++ )
		{
			CMapClass *pClass = (CUtlReference< CMapClass >)pObjectList->Element( i );

			if ( m_SavegameRestoredEnts.Find( pClass->GetID() ) == -1 )
			{
				toRespawn.AddToTail( pClass );
			}
		}
	}

	RespawnEntitiesWithEdits( toRespawn.Base(), toRespawn.Count() );

	m_SavegameRestoredEnts.Purge();
}


void CFoundryTool::MoveEntityTo( int nHammerID, const Vector &vPos, const QAngle &vAngles )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
	{
		Warning( "CFoundryTool::MoveEntityTo - no active CMapDoc\n" );
		return;
	}

	// 
	CMapEntity *pEnt = pDoc->FindEntityByHammerID( nHammerID );
	if ( !pEnt )
	{
		Warning( "CFoundryTool::MoveEntityTo - no entity with HammerID %d\n", nHammerID );
		return;
	}

	Vector vTempPos = vPos;
	pEnt->SetOrigin( vTempPos );

	CEditGameClass *pClass = dynamic_cast< CEditGameClass * >( pEnt );
	if ( pClass )
		pClass->SetAngles( vAngles );
}


void CFoundryTool::ClientPostRender()
{
	if ( !m_pMapDoc || !foundry_draw_hammer_entities.GetBool() )
		return;

	CMapView3D *pView = m_pMapDoc->GetFirst3DView();
	if ( !pView )
		return;


	// Store the old camera view parameters.
	CCamera *pCamera = pView->GetCamera();
	float flOldPitch = pCamera->GetPitch();
	float flOldYaw = pCamera->GetYaw();
	Vector vOldViewPoint;
	pCamera->GetViewPoint( vOldViewPoint );
	float flOldFOV = pCamera->GetFOV();
	float flOldNearClip = pCamera->GetNearClip();
	float flOldFarClip = pCamera->GetFarClip();

	// Move the Hammer camera to the engine's position for rendering.
	Vector vForward;
	AngleVectors( m_v3dViewAngles, &vForward );
	pView->SetCamera( m_v3dViewOrigin, m_v3dViewOrigin + vForward * 100 );
	pView->RenderView2( true );

	// Restore the old camera view parameters.
	pCamera->SetViewPoint( vOldViewPoint );
	pCamera->SetPerspective( flOldFOV, flOldNearClip, flOldFarClip );
	pCamera->SetPitch( flOldPitch );
	pCamera->SetYaw( flOldYaw );
}


bool CFoundryTool::SetupEngineView( Vector &origin, QAngle &angles, float &fov )
{
	m_v3dViewOrigin = origin;
	m_v3dViewAngles = angles;
	m_fl3dViewFOV = fov;

	return BaseClass::SetupEngineView( origin, angles, fov );
}


bool CFoundryTool::TrapKey( ButtonCode_t key, bool down )
{
	return BaseClass::TrapKey( key, down );
}

bool CFoundryTool::UpdateEntity( int iHammerID, CUtlVector<char*> &keys, CUtlVector<char*> &values )
{
	// Find the entity to be updated 
	void *pServerEntity = servertools->FindEntityByHammerID( iHammerID );
	if ( pServerEntity != NULL)
	{
		// Set updated properties
		for ( int i = 0; i < keys.Count(); i++ )
		{
			servertools->SetKeyValue( pServerEntity, keys[i], values[i] );
		}
		return true;
	}

	// Entity not found in running game
	return false;
}

void CFoundryTool::RespawnEntitiesWithEdits( CMapClass **ppEntities, int nEntities )
{
	CUtlVector<char> *pDatas = new CUtlVector<char>[nEntities];
	CUtlVector<CEntityRespawnInfo> respawnInfos;
	respawnInfos.SetSize( nEntities );

	int nValid = 0;
	for ( int i=0; i < nEntities; i++ )
	{
		if ( GetEntityVMFText( ppEntities[i], pDatas[nValid] ) )
		{
			respawnInfos[nValid].m_nHammerID = ppEntities[i]->GetHammerID();
			respawnInfos[nValid].m_pEntText = pDatas[nValid].Base();
			++nValid;
		}

	}

	servertools->RespawnEntitiesWithEdits( respawnInfos.Base(), nValid );

	delete [] pDatas;
}

void CFoundryTool::DestroyEntity( int iHammerID )
{
	servertools->DestroyEntityByHammerId( iHammerID );
}


void CFoundryTool::OnModeChanged()
{
	BaseClass::OnModeChanged();

	// We can get here during shutdown.
	if ( !GetMainWnd() )
		return;

	if ( IsGameInputEnabled() )
	{
		GetMainWnd()->EnableWindow( false );
		
		// Unpause the game.
		if ( foundry_auto_pause.GetInt() == 1 )
			enginetools->Command( "unpause" );
		else if ( foundry_auto_pause.GetInt() == 2 )
			enginetools->Command( "ai_setenabled 1" );
	}
	else
	{
		GetMainWnd()->EnableWindow( true );

		// Pause the game.
		if ( foundry_auto_pause.GetInt() == 1 )
			enginetools->Command( "setpause" );
		else if ( foundry_auto_pause.GetInt() == 2 )
			enginetools->Command( "ai_setenabled 0" );
	}
}	
	
void CFoundryTool::SwitchToEngine()
{
	EngineGetMouseControl();
}


void CFoundryTool::MoveEngineViewTo( const Vector &vPos, const QAngle &vAngles )
{
	servertools->MoveEngineViewTo( vPos, vAngles );
}


void CFoundryTool::MoveHammerViewTo( const Vector &vPos, const QAngle &vAngles )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
		return;

	POSITION p = pDoc->GetFirstViewPosition();
	while (p != NULL)
	{
		CMapView3D *pView = dynamic_cast< CMapView3D* >( pDoc->GetNextView(p) );
		if ( pView )
		{
			CCamera *pCamera = pView->GetCamera();
			pCamera->SetViewPoint( vPos );
			pCamera->SetPitch( vAngles[PITCH] );
			pCamera->SetYaw( -vAngles[YAW] + 90.0f );
			pCamera->SetRoll( 0 );
			return;
		}
	}
}


void CFoundryTool::EngineGetMouseControl()
{
	// Set focus on the engine window.
	::SetFocus( (HWND)enginetools->GetEngineHwnd() );
	SetMode( true, false );
}


void CFoundryTool::EngineReleaseMouseControl()
{
	SetMode( false, false );
}

void CFoundryTool::SelectEntities( int *pHammerIDs, int nIDs )
{
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
		return;

	pDoc->ClearEntitySelection();

	for ( int i=0; i < nIDs; i++ )
	{
		CMapEntity *pEnt = pDoc->FindEntityByHammerID( pHammerIDs[i] );
		pDoc->SelectObject( pEnt, scSelect );
	}
}

void CFoundryTool::ConsoleCommand( const char *pConCommand )
{
	enginetools->Command( pConCommand );
}


bool CFoundryTool::ShouldRender3DModels()
{
	return foundry_draw_hammer_models.GetBool();
}

void CFoundryTool::SelectionClickInCenterOfView( const Vector &vPos, const QAngle &vAngles )
{
	// First move the camera
	CMapDoc *pDoc = CMapDoc::GetActiveMapDoc();
	if ( !pDoc )
		return;

	CMapView3D *pView = NULL;
	POSITION p = pDoc->GetFirstViewPosition();
	while (p != NULL)
	{
		pView = dynamic_cast< CMapView3D* >( pDoc->GetNextView(p) );
		if ( pView )
			break;
	}

	if ( !pView )
		return;

	CToolManager *pToolManager = pDoc->GetTools();
	pToolManager->SetTool( TOOL_POINTER );
	CBaseTool *pTool = pToolManager->GetActiveTool();
	if ( !pTool )
		return;

	// Setup the camera position.
	CCamera *pCamera = pView->GetCamera();
	CCamera cameraBackup = *pCamera;

	pCamera->SetViewPoint( vPos );
	pCamera->SetPitch( vAngles[PITCH] );
	pCamera->SetYaw( -vAngles[YAW] + 90.0f );
	pCamera->SetRoll( 0 );


	RECT rc;
	pView->GetClientRect( &rc );

	// Simulate a mouse click.
	Vector2D vPoint( (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 );
	pTool->OnLMouseDown3D( pView, 0, vPoint );
	pTool->OnLMouseUp3D( pView, 0, vPoint );
	
	// Restore the camera.
	*pCamera = cameraBackup;	
}


// Called by the engine when exiting.
bool CFoundryTool::StaticQuitHandler( void *pvUserData )
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())
	
	CMainFrame *pFrame = GetMainWnd();
	if ( pFrame )
		pFrame->PostMessageA( WM_CLOSE, 0, 0 );

	// Return false to tell the engine to ignore the quit request. If Hammer finishes up the WM_QUIT, then everything will shutdown properly.
	return false;
}


void CFoundryTool::OnMapDocDestroy( CMapDoc *pDoc )
{
	if ( m_pMapDoc == pDoc )
		m_pMapDoc = NULL;
}

