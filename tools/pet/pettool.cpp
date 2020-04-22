//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Core Movie Maker UI API
//
//=============================================================================

#include "pettool.h"
#include "vgui_controls/Menu.h"
#include "tier1/KeyValues.h"
#include "vgui/IInput.h"
#include "vgui/KeyCode.h"
#include "vgui_controls/FileOpenDialog.h"
#include "filesystem.h"
#include "vgui/ilocalize.h"
#include "dme_controls/elementpropertiestree.h"
#include "tier0/icommandline.h"
#include "materialsystem/imaterialsystem.h"
#include "vguimatsurface/imatsystemsurface.h"
#include "petdoc.h"
#include "particlesystemdefinitionbrowser.h"
#include "particlesystempropertiescontainer.h"
#include "dme_controls/AttributeStringChoicePanel.h"
#include "dme_controls/ParticleSystemPanel.h"
#include "dme_controls/sheeteditorpanel.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "matsys_controls/picker.h"
#include "tier2/fileutils.h"
#include "tier3/tier3.h"
#include "tier3/mdlutils.h"
#include "particles/particles.h"
#include "dmserializers/idmserializers.h"
#include "dme_controls/dmepanel.h"
#include "vgui/ivgui.h"
#include "engine/IVDebugOverlay.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Interface to allow the particle system to call back into the game code
//-----------------------------------------------------------------------------
class CPetParticleSystemQuery : public CBaseAppSystem< IParticleSystemQuery >
{
public:
	CPetParticleSystemQuery() : m_pQuery( NULL ) {}

	virtual void LevelShutdown( ) { }

	virtual bool IsEditor( ) { return true; }

	// Inherited from IParticleSystemQuery
	virtual void GetLightingAtPoint( const Vector& vecOrigin, Color &cTint )
	{
		cTint.SetColor( 255, 255, 255, 255 );
	}

	virtual void TraceLine( const Vector& vecAbsStart,
							const Vector& vecAbsEnd, unsigned int mask, 
							const IHandleEntity *ignore,
							int collisionGroup, CBaseTrace *ptr )
	{
		ptr->startpos = vecAbsStart;
		ptr->endpos = vecAbsEnd;
		ptr->fraction = 1.0f;
		ptr->allsolid = ptr->startsolid = false;
	}

	virtual bool IsPointInSolid( const Vector& vecPos, const int nContentsMask )
	{
		return false;
	}

	virtual bool MovePointInsideControllingObject( CParticleCollection *pParticles, void *pObject, Vector *pPnt )
	{
		return true;
	}

	virtual void GetRandomPointsOnControllingObjectHitBox( CParticleCollection *pParticles,
															int nControlPointNumber, 
															int nNumPtsOut,
															float flBBoxScale,
															int nNumTrysToGetAPointInsideTheModel,
															Vector *pPntsOut,
															Vector vecDirectionalBias,
															Vector *pHitBoxRelativeCoordOut,
															int *pHitBoxIndexOut,
															int nDesiredHitbox, 
															const char *pszHitboxSetName )
	{
		for ( int i=0; i < nNumPtsOut; i++ )
		{
			pPntsOut[i] = pParticles->GetControlPointAtCurrentTime(nControlPointNumber); // fallback if anything goes wrong

			if ( pHitBoxIndexOut )
				pHitBoxIndexOut[i] = 0;

			if ( pHitBoxRelativeCoordOut )
				pHitBoxRelativeCoordOut[i].Init();
		}
	}

	virtual void GetClosestControllingObjectHitBox( CParticleCollection *pParticles,
		int nControlPointNumber, 
		int nNumPtsIn,
		float flBBoxScale,
		Vector *pPntsIn,
		Vector *pHitBoxRelativeCoordOut,
		int *pHitBoxIndexOut,
		int nDesiredHitbox, 
		const char *pszHitboxSetName )
	{
		for ( int i=0; i < nNumPtsIn; i++ )
		{
			if ( pHitBoxIndexOut )
				pHitBoxIndexOut[i] = 0;

			if ( pHitBoxRelativeCoordOut )
				pHitBoxRelativeCoordOut[i].Init();
		}
	}

	virtual void TraceAgainstRayTraceEnv( 
		int envnumber,  
		const FourRays &rays, fltx4 TMin, fltx4 TMax,
		RayTracingResult *rslt_out, int32 skip_id ) const
	{
		rslt_out->HitDistance = Four_Ones;
		rslt_out->surface_normal.DuplicateVector( vec3_origin );
	}

	virtual int GetRayTraceEnvironmentFromName( const char *pszRtEnvName )
	{
		if ( !m_pQuery )
			return 0;
		return m_pQuery->GetRayTraceEnvironmentFromName( pszRtEnvName );
	}

	virtual int GetCollisionGroupFromName( const char *pszCollisionGroupName )
	{
		if ( !m_pQuery )
			return 0;
		return m_pQuery->GetCollisionGroupFromName( pszCollisionGroupName );
	}

	virtual int GetControllingObjectHitBoxInfo( CParticleCollection *pParticles,
												int nControlPointNumber,
												int nBufSize, // # of output slots available
												ModelHitBoxInfo_t *pHitBoxOutputBuffer, 
												const char *pszHitboxSetName )
	{
		return 0;
	}

	virtual void GetControllingObjectOBBox( CParticleCollection *pParticles,
		int nControlPointNumber,
		Vector vecMin, Vector vecMax )
	{
		vecMin = vecMax = vec3_origin;
	}

	virtual Vector GetLocalPlayerPos()
	{
		return vec3_origin;
	}

	virtual Vector GetCurrentViewOrigin()
	{
		//FIXME : This should get the petool's window location.
		return vec3_origin;
	}

	//Not Yet Implemented
	virtual int GetActivityCount() { return 0; }
	virtual const char *GetActivityNameFromIndex( int nActivityIndex ) { return 0; }
	virtual int GetActivityNumber( void *pModel, const char *m_pszActivityName ); 

	virtual float GetPixelVisibility( int *pQueryHandle, const Vector &vecOrigin, float flScale )
	{
		return 1.0f;
	}

	void SetChainQuery( IParticleSystemQuery *pQuery ) { m_pQuery = pQuery; }

	virtual void PreSimulate( ) { }

	virtual void PostSimulate( ) { }

	void DebugDrawLine( const Vector &origin, const Vector &target, int r, int g, int b, bool noDepthTest, float duration )
	{
		debugoverlay->AddLineOverlay( origin, target, r, g, b, noDepthTest, duration );
	}

	virtual void *GetModel( char const *pMdlName );

	virtual void DrawModel( void *pModel, const matrix3x4_t &DrawMatrix, CParticleCollection *pParticles, int nParticleNumber, int nBodyPart, int nSubModel,
		int nSkin, int nAnimationSequence = 0, float flAnimationRate = 30.0f, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f );

	virtual void UpdateProjectedTexture( const int nParticleID, IMaterial *pMaterial, Vector &vOrigin, float flRadius, float flRotation, float r, float g, float b, float a, void *&pUserVar ) { }

private:
	IParticleSystemQuery *m_pQuery;
};


static CPetParticleSystemQuery s_PetParticleSystemQuery;
IParticleSystemQuery *g_pPetParticleSystemQuery = &s_PetParticleSystemQuery;

 
static void SetBodygroup( studiohdr_t *pstudiohdr, int &body, int iGroup, int iValue )
{
	if ( !pstudiohdr )
	{
		return;
	}

	if (iGroup >= pstudiohdr->numbodyparts)
	{
		return;
	}

	mstudiobodyparts_t *pbodypart = pstudiohdr->pBodypart( iGroup );

	if ( iValue >= pbodypart->nummodels )
	{
		return;
	}

	int iCurrent = ( body / pbodypart->base ) % pbodypart->nummodels;

	body = ( body - ( iCurrent * pbodypart->base ) + ( iValue * pbodypart->base ) );
}



void CPetParticleSystemQuery::DrawModel( void *pModel, const matrix3x4_t &DrawMatrix, CParticleCollection *pParticles, int nParticleNumber, int nBodyPart, int nSubModel, 
										 int nSkin, int nAnimationSequence, float flAnimationRate, float r, float g, float b, float a )
{
	MDLHandle_t hMdl = ( MDLHandle_t ) pModel;
	CMDL myModel;
	myModel.SetMDL( hMdl );
	CMatRenderContextPtr pRenderContext( materials );
	// flashlights can't work in the model panel under queued mode (the state isn't ready yet, so causes a crash)
	pRenderContext->SetFlashlightMode( false );
	CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( hMdl ), g_pMDLCache );

	CMatRenderData< matrix3x4_t > rdBoneToWorld( pRenderContext, studioHdr.numbones() );

	myModel.m_Color = Color( FastFToC( r ), FastFToC( g ), FastFToC( b ), FastFToC( a ) );

	SetBodygroup( myModel.GetStudioHdr(), myModel.m_nBody, nBodyPart, nSubModel );
	
	myModel.m_nSkin = nSkin;
	myModel.m_nSequence = nAnimationSequence;
	myModel.m_flPlaybackRate = flAnimationRate;
	myModel.m_flTime = pParticles->m_flCurTime;

 	myModel.SetUpBones( DrawMatrix, studioHdr.numbones(), rdBoneToWorld.Base() );
	myModel.Draw( DrawMatrix, rdBoneToWorld.Base(), STUDIORENDER_DRAW_NO_SHADOWS );

	//debugging
	//Vector vecFwd, vecRight, vecUp, vecOrigin;
	//MatrixVectors( DrawMatrix, &vecFwd, &vecRight, &vecUp );
	//vecOrigin = Vector ( DrawMatrix[0][3], DrawMatrix[1][3], DrawMatrix[2][3] );
	//debugoverlay->AddLineOverlay( vecOrigin, vecOrigin + 36 * vecFwd, 255, 0, 0, true, 0.1 );
	//debugoverlay->AddLineOverlay( vecOrigin, vecOrigin + 36 * vecUp, 0, 0, 255, true, 0.1 );
	//debugoverlay->AddLineOverlay( vecOrigin, vecOrigin + 36 * vecRight, 0, 255, 0, true, 0.1 );
}


void *CPetParticleSystemQuery::GetModel( char const *pMdlName )
{
	CUtlString	ModelName = "models/";
	
	ModelName += pMdlName;

	MDLHandle_t mdlHandle = g_pMDLCache->FindMDL( ModelName );

	return ( void * ) mdlHandle;
}

int CPetParticleSystemQuery::GetActivityNumber( void *pModel, const char *m_pszActivityName )
{
	/*MDLHandle_t hMdl = ( MDLHandle_t ) pModel;
	CStudioHdr *pStudioHdr = new CStudioHdr( g_pMDLCache->GetStudioHdr( hMdl ), g_pMDLCache );
	if ( pStudioHdr->IsValid() )
	{
		int nActivityNum = LookupActivity( pStudioHdr, m_pszActivityName );
		int nAnimationNum = pStudioHdr->SelectWeightedSequence( nActivityNum, -1 );
		return nAnimationNum;
	}*/
	return -1;
}


//-----------------------------------------------------------------------------
// Methods needed by scenedatabase. They have to live here instead of toolutils
// because this is a DLL but toolutils is only a static library
//-----------------------------------------------------------------------------
USING_DMEPANEL_FACTORY( CParticleSystemPreviewPanel, DmeParticleSystemDefinition );
USING_DMEPANEL_FACTORY( CParticleSystemDmePanel, DmeParticleSystemDefinition );


const char *GetVGuiControlsModuleName()
{
	return "PetTool";
}

//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool ConnectTools( CreateInterfaceFn factory )
{
	// Attach to the dmserializers instance of the particle system
	return (materials != NULL) && (g_pMatSystemSurface != NULL) && (g_pMDLCache != NULL) && (studiorender != NULL) && (g_pMaterialSystemHardwareConfig != NULL);
}

void DisconnectTools( )
{
}


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CPetTool	*g_pPetTool = NULL;


void CreateTools()
{
	g_pPetTool = new CPetTool();
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CPetTool::CPetTool()
{
	m_pMenuBar = NULL;
	m_pDoc = NULL;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CPetTool::Init( )
{
	m_hCurrentParticleSystem = NULL;
	m_pDoc = NULL;
	m_RecentFiles.LoadFromRegistry( GetRegistryName() );

	// NOTE: This has to happen before BaseClass::Init
	g_pVGuiLocalize->AddFile( "resource/toolpet_%language%.txt" );

	if ( !BaseClass::Init( ) )
		return false;

	CreateInterfaceFn factory;
	enginetools->GetClientFactory( factory );
	IParticleSystemQuery *pQuery = (IParticleSystemQuery*)factory( PARTICLE_SYSTEM_QUERY_INTERFACE_VERSION, NULL );
	s_PetParticleSystemQuery.SetChainQuery( pQuery );

	g_pParticleSystemMgr->Init( g_pPetParticleSystemQuery, true );
	// tell particle mgr to add the default simulation + rendering ops
	g_pParticleSystemMgr->AddBuiltinSimulationOperators();
	g_pParticleSystemMgr->AddBuiltinRenderingOperators();

	// Create a directory for particles if it doesn't exist
	char pStartingDir[ MAX_PATH ];
	GetModSubdirectory( "particles", pStartingDir, sizeof(pStartingDir) );
	g_pFullFileSystem->CreateDirHierarchy( pStartingDir );

	return true;
}

void CPetTool::Shutdown()
{
	m_RecentFiles.SaveToRegistry( GetRegistryName() );

	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
bool UTIL_IsDedicatedServer( void )
{
	return false;
}

//-----------------------------------------------------------------------------
// returns the document
//-----------------------------------------------------------------------------
CPetDoc *CPetTool::GetDocument()
{
	return m_pDoc;
}

	
//-----------------------------------------------------------------------------
// Tool activation/deactivation
//-----------------------------------------------------------------------------
void CPetTool::OnToolActivate()
{
	BaseClass::OnToolActivate();
}

void CPetTool::OnToolDeactivate()
{
	BaseClass::OnToolDeactivate();
}


//-----------------------------------------------------------------------------
// Used to hook DME VMF entities into the render lists
//-----------------------------------------------------------------------------
void CPetTool::Think( bool finalTick )
{
	BaseClass::Think( finalTick );

	if ( IsActiveTool() )
	{
		// Force resolve calls to happen
		// FIXME: Shouldn't this not have to happen here?
		CUtlVector< IDmeOperator* > operators;
		g_pDmElementFramework->SetOperators( operators );
		g_pDmElementFramework->Operate( true );
		g_pDmElementFramework->BeginEdit();
	}
}


//-----------------------------------------------------------------------------
// Derived classes can implement this to get a new scheme to be applied to this tool
//-----------------------------------------------------------------------------
vgui::HScheme CPetTool::GetToolScheme() 
{ 
	return vgui::scheme()->LoadSchemeFromFile( "Resource/BoxRocket.res", "BoxRocket" );
}


//-----------------------------------------------------------------------------
//
// The View menu
//
//-----------------------------------------------------------------------------
class CPetViewMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CPetViewMenuButton, CToolMenuButton );
public:
	CPetViewMenuButton( CPetTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu(vgui::Menu *menu);

private:
	CPetTool *m_pTool;
};

CPetViewMenuButton::CPetViewMenuButton( CPetTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	m_pTool = parent;

	AddCheckableMenuItem( "properties", "#PetProperties", new KeyValues( "OnToggleProperties" ), pActionSignalTarget );
	AddCheckableMenuItem( "browser", "#PetParticleSystemBrowser", new KeyValues( "OnToggleParticleSystemBrowser" ), pActionSignalTarget );
	AddCheckableMenuItem( "particlepreview", "#PetParticlePreview", new KeyValues( "OnToggleParticlePreview" ), pActionSignalTarget );
	// AddCheckableMenuItem( "sheeteditor", "#PetSheetEditor", new KeyValues( "OnToggleSheetEditor" ), pActionSignalTarget );

	AddSeparator();

	AddMenuItem( "defaultlayout", "#PetViewDefault", new KeyValues( "OnDefaultLayout" ), pActionSignalTarget );

	SetMenu(m_pMenu);
}


void CPetViewMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );

	// Update the menu
	int id;

	CPetDoc *pDoc = m_pTool->GetDocument();
	if ( pDoc )
	{
		id = m_Items.Find( "properties" );
		m_pMenu->SetItemEnabled( id, true );

		Panel *p;
		p = m_pTool->GetProperties();
		Assert( p );
		m_pMenu->SetMenuItemChecked( id, ( p && p->GetParent() ) ? true : false );

		id = m_Items.Find( "browser" );
		m_pMenu->SetItemEnabled( id, true );
		
		p = m_pTool->GetParticleSystemDefinitionBrowser();
		Assert( p );
		m_pMenu->SetMenuItemChecked( id, ( p && p->GetParent() ) ? true : false );

		id = m_Items.Find( "particlepreview" );
		m_pMenu->SetItemEnabled( id, true );

		p = m_pTool->GetParticlePreview();
		Assert( p );
		m_pMenu->SetMenuItemChecked( id, ( p && p->GetParent() ) ? true : false );

		/*
		id = m_Items.Find( "sheeteditor" );
		m_pMenu->SetItemEnabled( id, true );

		p = m_pTool->GetSheetEditor();
		Assert( p );
		m_pMenu->SetMenuItemChecked( id, ( p && p->GetParent() ) ? true : false );
		*/
	}
	else
	{
		id = m_Items.Find( "properties" );
		m_pMenu->SetItemEnabled( id, false );
		id = m_Items.Find( "browser" );
		m_pMenu->SetItemEnabled( id, false );
		id = m_Items.Find( "particlepreview" );
		m_pMenu->SetItemEnabled( id, false );
		/*
		id = m_Items.Find( "sheeteditor" );
		m_pMenu->SetItemEnabled( id, false );
		*/
	}
}


//-----------------------------------------------------------------------------
//
// The Tool menu
//
//-----------------------------------------------------------------------------
class CPetToolMenuButton : public CToolMenuButton
{
	DECLARE_CLASS_SIMPLE( CPetToolMenuButton, CToolMenuButton );
public:
	CPetToolMenuButton( CPetTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget );
	virtual void OnShowMenu(vgui::Menu *menu);

private:
	CPetTool *m_pTool;
};

CPetToolMenuButton::CPetToolMenuButton( CPetTool *parent, const char *panelName, const char *text, vgui::Panel *pActionSignalTarget )
	: BaseClass( parent, panelName, text, pActionSignalTarget )
{
	m_pTool = parent;

	SetMenu(m_pMenu);
}

void CPetToolMenuButton::OnShowMenu(vgui::Menu *menu)
{
	BaseClass::OnShowMenu( menu );
}


//-----------------------------------------------------------------------------
// Initializes the menu bar
//-----------------------------------------------------------------------------
vgui::MenuBar *CPetTool::CreateMenuBar( CBaseToolSystem *pParent ) 
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
	CPetToolMenuButton *pToolButton = new CPetToolMenuButton( this, "Pet", "&Pet", GetActionTarget() );
	CPetViewMenuButton *pViewButton = new CPetViewMenuButton( this, "View", "&View", GetActionTarget() );
	CToolMenuButton *pSwitchButton = CreateToolSwitchMenuButton( m_pMenuBar, "Switcher", "&Tools", GetActionTarget() );

	pEditButton->AddMenuItem( "copySystem", "Copy Systems", new KeyValues( "OnCopySystems" ), GetActionTarget(), NULL );
	pEditButton->AddMenuItem( "copyFunctions", "Copy Functions", new KeyValues( "OnCopyFunctions" ), GetActionTarget() );
	pEditButton->AddMenuItem( "paste", "#BxEditPaste", new KeyValues( "OnPaste" ), GetActionTarget(), NULL, "edit_paste" );

	pEditButton->MoveMenuItem( pEditButton->FindMenuItem( "paste" ), pEditButton->FindMenuItem( "editkeybindings" ) );
	pEditButton->MoveMenuItem( pEditButton->FindMenuItem( "copySystem" ), pEditButton->FindMenuItem( "paste" ) );
	pEditButton->MoveMenuItem( pEditButton->FindMenuItem( "copyFunctions" ), pEditButton->FindMenuItem( "paste" ) );
	pEditButton->AddSeparatorAfterItem( "paste" );

	m_pMenuBar->AddButton( pFileButton );
	m_pMenuBar->AddButton( pEditButton );
	m_pMenuBar->AddButton( pToolButton );
	m_pMenuBar->AddButton( pViewButton );
	m_pMenuBar->AddButton( pSwitchButton );

	return m_pMenuBar;
}


//-----------------------------------------------------------------------------
// Updates the menu bar based on the current file
//-----------------------------------------------------------------------------
void CPetTool::UpdateMenuBar( )
{
	if ( !m_pDoc )
	{
		m_pMenuBar->SetFileName( "#PetNoFile" );
		return;
	}

	const char *pFile = m_pDoc->GetFileName();
	if ( !pFile[0] )
	{
		m_pMenuBar->SetFileName( "#PetNoFile" );
		return;
	}

	if ( m_pDoc->IsDirty() )
	{
		char sz[ 512 ];
		Q_snprintf( sz, sizeof( sz ), "* %s", pFile );
		m_pMenuBar->SetFileName( sz );
	}
	else
	{
		m_pMenuBar->SetFileName( pFile );
	}
}


//-----------------------------------------------------------------------------
// Gets at tool windows
//-----------------------------------------------------------------------------
CParticleSystemPropertiesContainer *CPetTool::GetProperties()
{
	return m_hProperties.Get();
}

CParticleSystemDefinitionBrowser *CPetTool::GetParticleSystemDefinitionBrowser()
{
	return m_hParticleSystemDefinitionBrowser.Get();
}

CParticleSystemPreviewPanel *CPetTool::GetParticlePreview()
{
	return m_hParticlePreview.Get();
}

/*
CSheetEditorPanel *CPetTool::GetSheetEditor()
{
	return m_hSheetEditorPanel.Get();
}
*/

//-----------------------------------------------------------------------------
// paste
//-----------------------------------------------------------------------------
void CPetTool::OnCopySystems()
{
	Panel *pDefBrowser = FindChildByName( "ParticleSystemDefinitionBrowser", true );

	if ( pDefBrowser )
	{
		vgui::ipanel()->SendMessage( pDefBrowser->GetVPanel(), new KeyValues( "OnCopy" ), GetVPanel() );
	}
}

void CPetTool::OnCopyFunctions()
{
	Panel *pFuncBrowser = FindChildByName( "FunctionBrowser", true );

	if ( pFuncBrowser )
	{
		vgui::ipanel()->SendMessage( pFuncBrowser->GetVPanel(), new KeyValues( "OnCopy" ), GetVPanel() );
	}
}

void CPetTool::OnPaste()
{
	GetParticleSystemDefinitionBrowser()->PasteFromClipboard();
}

void CPetTool::OnRequestPaste()
{
	OnPaste();
}

//-----------------------------------------------------------------------------
// Sets/gets the current particle system
//-----------------------------------------------------------------------------
void CPetTool::SetCurrentParticleSystem( CDmeParticleSystemDefinition *pParticleSystem, bool bForceBrowserSelection )
{
	if ( !m_pDoc )
		return;

	if ( pParticleSystem && m_hCurrentParticleSystem.Get() == pParticleSystem )
		return;

	m_hCurrentParticleSystem = pParticleSystem;
	if ( bForceBrowserSelection && m_hParticleSystemDefinitionBrowser.Get() )
	{
		m_hParticleSystemDefinitionBrowser->UpdateParticleSystemList( false );
		m_hParticleSystemDefinitionBrowser->SelectParticleSystem( pParticleSystem );
	}
	if ( m_hParticlePreview.Get() )
	{
		m_hParticlePreview->SetParticleSystem( pParticleSystem, bForceBrowserSelection );
	}
	if ( m_hProperties.Get() )
	{
		m_hProperties->SetParticleSystem( m_hCurrentParticleSystem );
	}
	/*
	if ( m_hSheetEditorPanel.Get() )
	{
		m_hSheetEditorPanel->SetParticleSystem( m_hCurrentParticleSystem );
	}
	*/
}

CDmeParticleSystemDefinition* CPetTool::GetCurrentParticleSystem( void )
{
	return m_hCurrentParticleSystem;
}


//-----------------------------------------------------------------------------
// Destroys all tool windows
//-----------------------------------------------------------------------------
void CPetTool::DestroyToolContainers()
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
void CPetTool::OnDefaultLayout()
{
	int y = m_pMenuBar->GetTall();

	int usew, useh;
	GetSize( usew, useh );

	DestroyToolContainers();

	Assert( ToolWindow::GetToolWindowCount() == 0 );

	CParticleSystemPropertiesContainer *pProperties = GetProperties();
	CParticleSystemDefinitionBrowser *pParticleSystemBrowser = GetParticleSystemDefinitionBrowser();
	CParticleSystemPreviewPanel *pPreviewer = GetParticlePreview();
	//CSheetEditorPanel *pSheetEditor = GetSheetEditor();

	// Need three containers
	ToolWindow *pPropertyWindow = m_ToolWindowFactory.InstanceToolWindow( GetClientArea(), false, pProperties, "#PetProperties", false );
	ToolWindow *pBrowserWindow = m_ToolWindowFactory.InstanceToolWindow( GetClientArea(), false, pParticleSystemBrowser, "#PetParticleSystemBrowser", false );
	ToolWindow *pPreviewWindow = m_ToolWindowFactory.InstanceToolWindow( GetClientArea(), false, pPreviewer, "#PetPreviewer", false );

	//pPropertyWindow->AddPage( pSheetEditor, "#PetSheetEditor", false );

	int halfScreen = usew / 2;
	int bottom = useh - y;
	int sy = (bottom - y) / 2;
	SetMiniViewportBounds( halfScreen, y, halfScreen, sy - y );
	pPreviewWindow->SetBounds( halfScreen, sy, halfScreen, bottom - sy );
	pBrowserWindow->SetBounds( 0, y, halfScreen, sy - y );
	pPropertyWindow->SetBounds( 0, sy, halfScreen, bottom - sy );
}

void CPetTool::OnToggleProperties()
{
	if ( m_hProperties.Get() )
	{ 
		ToggleToolWindow( m_hProperties.Get(), "#PetProperties" );
	}
}

void CPetTool::OnToggleParticleSystemBrowser()
{
	if ( m_hParticleSystemDefinitionBrowser.Get() )
	{ 
		ToggleToolWindow( m_hParticleSystemDefinitionBrowser.Get(), "#PetParticleSystemBrowser" );
	}
}

void CPetTool::OnToggleParticlePreview()
{
	if ( m_hParticlePreview.Get() )
	{ 
		ToggleToolWindow( m_hParticlePreview.Get(), "#PetPreviewer" );
	}
}

/*
void CPetTool::OnToggleSheetEditor()
{
	if ( m_hSheetEditorPanel.Get() )
	{ 
		ToggleToolWindow( m_hSheetEditorPanel.Get(), "#PetSheetEditor" );
	}
}
*/

//-----------------------------------------------------------------------------
// Creates
//-----------------------------------------------------------------------------
void CPetTool::CreateTools( CPetDoc *doc )
{
	if ( !m_hProperties.Get() )
	{
		m_hProperties = new CParticleSystemPropertiesContainer( m_pDoc, this );
		m_hProperties->AddActionSignalTarget(this);
	}

	if ( !m_hParticleSystemDefinitionBrowser.Get() )
	{
		m_hParticleSystemDefinitionBrowser = new CParticleSystemDefinitionBrowser( m_pDoc, this, "ParticleSystemDefinitionBrowser" );
	}

	if ( !m_hParticlePreview.Get() )
	{
		m_hParticlePreview = new CParticleSystemPreviewPanel( NULL, "Particle System Preview" );
	}

	/*
	if ( !m_hSheetEditorPanel.Get() )
	{
		m_hSheetEditorPanel = new CSheetEditorPanel( NULL, "Sheet Editor" );
	}
	*/

	RegisterToolWindow( m_hProperties );
	RegisterToolWindow( m_hParticleSystemDefinitionBrowser );
	RegisterToolWindow( m_hParticlePreview );
	// RegisterToolWindow( m_hSheetEditorPanel );
}


//-----------------------------------------------------------------------------
// Initializes the tools
//-----------------------------------------------------------------------------
void CPetTool::InitTools()
{
	// FIXME: There are no tool windows here; how should this work?
	// These panels are saved
	windowposmgr->RegisterPanel( "properties", m_hProperties, false );
	windowposmgr->RegisterPanel( "particlesystemdefinitionbrowser", m_hParticleSystemDefinitionBrowser, false );
	windowposmgr->RegisterPanel( "previewpanel", m_hParticlePreview, false );
	// windowposmgr->RegisterPanel( "sheeteditor", m_hSheetEditorPanel, false );

	if ( !windowposmgr->LoadPositions( "cfg/pet.txt", this, &m_ToolWindowFactory, "Pet" ) )
	{
		OnDefaultLayout();
	}
}


void CPetTool::DestroyTools()
{	
	if ( m_hParticlePreview.Get() )
	{
		m_hParticlePreview->ClearParticleSystemLock();
	}

	SetCurrentParticleSystem( NULL );

	int c = ToolWindow::GetToolWindowCount();
	for ( int i = c - 1; i >= 0 ; --i )
	{
		ToolWindow *kill = ToolWindow::GetToolWindow( i );
		delete kill;
	}

	UnregisterAllToolWindows();

	if ( m_hProperties.Get() )
	{
		windowposmgr->UnregisterPanel( m_hProperties.Get() );
		delete m_hProperties.Get();
		m_hProperties = NULL;
	}

	if ( m_hParticleSystemDefinitionBrowser.Get() )
	{
		windowposmgr->UnregisterPanel( m_hParticleSystemDefinitionBrowser.Get() );
		delete m_hParticleSystemDefinitionBrowser.Get();
		m_hParticleSystemDefinitionBrowser = NULL;
	}

	if ( m_hParticlePreview.Get() )
	{
		windowposmgr->UnregisterPanel( m_hParticlePreview.Get() );
		delete m_hParticlePreview.Get();
		m_hParticlePreview = NULL;
	}

	/*
	if ( m_hSheetEditorPanel.Get() )
	{
		windowposmgr->UnregisterPanel( m_hSheetEditorPanel.Get() );
		delete m_hSheetEditorPanel.Get();
		m_hSheetEditorPanel = NULL;
	}
	*/
}


void CPetTool::ShowToolWindow( Panel *tool, char const *toolName, bool visible )
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

void CPetTool::ToggleToolWindow( Panel *tool, char const *toolName )
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
vgui::Menu *CPetTool::CreateActionMenu( vgui::Panel *pParent )
{
	vgui::Menu *pActionMenu = new Menu( pParent, "ActionMenu" );
	pActionMenu->AddMenuItem( "#ToolHide", new KeyValues( "Command", "command", "HideActionMenu" ), GetActionTarget() );
	return pActionMenu;
}


//-----------------------------------------------------------------------------
// Inherited from IFileMenuCallbacks
//-----------------------------------------------------------------------------
int	CPetTool::GetFileMenuItemsEnabled( )
{
	int nFlags = FILE_ALL;
	if ( m_RecentFiles.IsEmpty() )
	{
		nFlags &= ~FILE_RECENT;
	}
	return nFlags;
}

void CPetTool::AddRecentFilesToMenu( vgui::Menu *pMenu )
{
	m_RecentFiles.AddToMenu( pMenu, GetActionTarget(), "OnRecent" );
}

bool CPetTool::GetPerforceFileName( char *pFileName, int nMaxLen )
{
	if ( !m_pDoc )
		return false;

	Q_strncpy( pFileName, m_pDoc->GetFileName(), nMaxLen );
	return pFileName[0] != 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
//-----------------------------------------------------------------------------
void CPetTool::OnExit()
{
	windowposmgr->SavePositions( "cfg/pet.txt", "Pet" );

	enginetools->Command( "quit\n" );
}

//-----------------------------------------------------------------------------
// Handle commands from the action menu and other menus
//-----------------------------------------------------------------------------
void CPetTool::OnCommand( const char *cmd )
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
void CPetTool::PerformNew()
{
	OnCloseNoSave();
	NewDocument();
}

void CPetTool::OnNew()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		SaveFile( m_pDoc->GetFileName(), PET_FILE_FORMAT, FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY,
			new KeyValues( "OnNew" ) );
		return;
	}

	PerformNew();
}


//-----------------------------------------------------------------------------
// Called when the File->Open menu is selected
//-----------------------------------------------------------------------------
void CPetTool::OnOpen( )
{
	int nFlags = 0;
	const char *pSaveFileName = NULL;
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
		pSaveFileName = m_pDoc->GetFileName();
	}

	OpenFile( PET_FILE_FORMAT, pSaveFileName, PET_FILE_FORMAT, nFlags );
}

bool CPetTool::OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	OnCloseNoSave();

	if ( !LoadDocument( pFileName ) )
		return false;

	m_RecentFiles.Add( pFileName, pFileFormat );
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	UpdateMenuBar();
	return true;
}

void CPetTool::Save()
{
	if ( m_pDoc )
	{
		SaveFile( m_pDoc->GetFileName(), PET_FILE_FORMAT, FOSM_SHOW_PERFORCE_DIALOGS );
	}
}

void CPetTool::OnSaveAs()
{
	if ( m_pDoc )
	{
		SaveFile( NULL, PET_FILE_FORMAT, FOSM_SHOW_PERFORCE_DIALOGS );
	}
}

void CPetTool::OnRestartLevel()
{
	enginetools->Command( "restart" );
	enginetools->Execute();
}

void CPetTool::SaveAndTest()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		SaveFile( m_pDoc->GetFileName(), PET_FILE_FORMAT, FOSM_SHOW_PERFORCE_DIALOGS, 
			new KeyValues( "RestartLevel" ) );
	}
	else
	{
		OnRestartLevel();
	}
}

bool CPetTool::OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	if ( !m_pDoc )
		return true;

	m_pDoc->SetFileName( pFileName );
	m_pDoc->SaveToFile( );

	m_RecentFiles.Add( pFileName, pFileFormat );
	m_RecentFiles.SaveToRegistry( GetRegistryName() );
	UpdateMenuBar();
	return true;
}

void CPetTool::OnClose()
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		SaveFile( m_pDoc->GetFileName(), PET_FILE_FORMAT, FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY, 
			new KeyValues( "OnClose" ) );
		return;
	}

	OnCloseNoSave();
}

void CPetTool::OnCloseNoSave()
{
	DestroyTools();

	if ( m_pDoc )
	{
		CAppNotifyScopeGuard sg( "CPetTool::OnCloseNoSave", NOTIFY_CHANGE_OTHER );

		delete m_pDoc;
		m_pDoc = NULL;

		if ( m_hProperties )
		{
			m_hProperties->SetParticleSystem( NULL );
		}
	}

	UpdateMenuBar( );
}

void CPetTool::OnMarkNotDirty()
{
	if ( m_pDoc )
	{
		m_pDoc->SetDirty( false );
	}
}


//-----------------------------------------------------------------------------
// Open a specific file
//-----------------------------------------------------------------------------
void CPetTool::OpenSpecificFile( const char *pFileName )
{
	int nFlags = 0;
	const char *pSaveFileName = NULL;
	if ( m_pDoc )
	{
		// File is already open
		if ( !Q_stricmp( m_pDoc->GetFileName(), pFileName ) )
			return;

		if ( m_pDoc->IsDirty() )
		{
			nFlags = FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY;
			pSaveFileName = m_pDoc->GetFileName();
		}
		else
		{
			OnCloseNoSave();
		}
	}

	OpenFile( pFileName, PET_FILE_FORMAT, pSaveFileName, PET_FILE_FORMAT, nFlags );
}


//-----------------------------------------------------------------------------
// Show the save document query dialog
//-----------------------------------------------------------------------------
void CPetTool::OpenFileFromHistory( int slot )
{
	const char *pFileName = m_RecentFiles.GetFile( slot );
	if ( !pFileName )
		return;
	OpenSpecificFile( pFileName );
}


//-----------------------------------------------------------------------------
// Derived classes can implement this to get notified when files are saved/loaded
//-----------------------------------------------------------------------------
void CPetTool::OnFileOperationCompleted( const char *pFileType, bool bWroteFile, vgui::FileOpenStateMachine::CompletionState_t state, KeyValues *pContextKeyValues )
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
		enginetools->Command( "toolunload pet -nosave\n" );
		return;
	}


	if ( !Q_stricmp( pContextKeyValues->GetName(), "RestartLevel" ) )
	{
		OnRestartLevel();
		return;
	}
}


//-----------------------------------------------------------------------------
// Show the File browser dialog
//-----------------------------------------------------------------------------
void CPetTool::SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	char pStartingDir[ MAX_PATH ];

	GetModSubdirectory( "particles", pStartingDir, sizeof(pStartingDir) );

	// Open a bsp file to create a new commentary file
	pDialog->SetTitle( "Choose Particle Configuration File", true );
	pDialog->SetStartDirectoryContext( "pet_session", pStartingDir );
	pDialog->AddFilter( "*.pcf", "Particle Configuration File (*.pcf)", true );
}


//-----------------------------------------------------------------------------
// Can we quit?
//-----------------------------------------------------------------------------
bool CPetTool::CanQuit( const char *pExitMsg )
{
	if ( m_pDoc && m_pDoc->IsDirty() )
	{
		// Show Save changes Yes/No/Cancel and re-quit if hit yes/no
		SaveFile( m_pDoc->GetFileName(), PET_FILE_FORMAT, FOSM_SHOW_PERFORCE_DIALOGS | FOSM_SHOW_SAVE_QUERY, new KeyValues( pExitMsg ) );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Various command handlers related to the Edit menu
//-----------------------------------------------------------------------------
void CPetTool::OnUndo()
{
	CDisableUndoScopeGuard guard;
	g_pDataModel->Undo();
}

void CPetTool::OnRedo()
{
	CDisableUndoScopeGuard guard;
	g_pDataModel->Redo();
}

void CPetTool::OnDescribeUndo()
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
// Background
//-----------------------------------------------------------------------------
const char *CPetTool::GetLogoTextureName()
{
	return NULL;
}


//-----------------------------------------------------------------------------
// Inherited from IPetDocCallback
//-----------------------------------------------------------------------------
void CPetTool::OnDocChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	if ( nNotifyFlags & NOTIFY_CHANGE_MASK )
	{
		// force a resolve so the local particle manager stays in sync
		g_pDmElementFramework->Operate( true );
		g_pDmElementFramework->BeginEdit();
	}

	CDmeParticleSystemDefinition *pParticleSystem = GetCurrentParticleSystem();
	if ( m_pDoc && GetParticlePreview() && pParticleSystem )
	{
		m_pDoc->UpdateParticleDefinition( pParticleSystem );
	}

	if ( nNotifyFlags & NOTIFY_CHANGE_TOPOLOGICAL )
	{
		if ( GetParticleSystemDefinitionBrowser() )
		{
			// only retain selection if we didn't create/delete a system
			bool bRetainSelection = !(nNotifyFlags & NOTIFY_FLAG_PARTICLESYS_ADDED_OR_REMOVED);

			GetParticleSystemDefinitionBrowser()->UpdateParticleSystemList( bRetainSelection );
		}
	}

	bool bRefreshProperties = ( nNotifySource != NOTIFY_SOURCE_PROPERTIES_TREE ) &&
		( ( nNotifyFlags & ( NOTIFY_CHANGE_TOPOLOGICAL | NOTIFY_CHANGE_ATTRIBUTE_ARRAY_SIZE ) ) != 0 );
	bool bRefreshPropertyValues = ( nNotifySource != NOTIFY_SOURCE_PROPERTIES_TREE ) &&
		( nNotifyFlags & NOTIFY_CHANGE_ATTRIBUTE_VALUE ) != 0;
	
	if ( bRefreshProperties || bRefreshPropertyValues )
	{
		if ( m_hProperties.Get() )
		{
			m_hProperties->Refresh( !bRefreshProperties );
		}
	}

	UpdateMenuBar();
}


//-----------------------------------------------------------------------------
// Creates a new document
//-----------------------------------------------------------------------------
void CPetTool::NewDocument( )
{
	Assert( !m_pDoc );

	DestroyTools();

	m_pDoc = new CPetDoc( this );
	m_pDoc->CreateNew( );

	ShowMiniViewport( true );
	CreateTools( m_pDoc );
	UpdateMenuBar( );
	InitTools();
}


//-----------------------------------------------------------------------------
// Loads up a new document
//-----------------------------------------------------------------------------
bool CPetTool::LoadDocument( const char *pDocName )
{
	Assert( !m_pDoc );

	DestroyTools();

	m_pDoc = new CPetDoc( this );
	if ( !m_pDoc->LoadFromFile( pDocName ) )
	{
		delete m_pDoc;
		m_pDoc = NULL;
		Warning( "Fatal error loading '%s'\n", pDocName );
		return false;
	}

	ShowMiniViewport( true );

	CreateTools( m_pDoc );
	UpdateMenuBar( );
	InitTools();

	// Let the other tools know	we've loaded + therefore modified particle systems
	CUtlBuffer buf;
	g_pDataModel->Serialize( buf, "binary", PET_FILE_FORMAT, m_pDoc->GetRootObject()->GetHandle() );

	KeyValues *pMessage = new KeyValues( "ParticleSystemUpdated" );
	pMessage->SetPtr( "definitionBits", buf.Base() );
	pMessage->SetInt( "definitionSize", buf.TellMaxPut() );
	PostMessageToAllTools( pMessage );
	pMessage->deleteThis();
	return true;
}

void CPetTool::PreOperatorsPaste()
{
	if ( m_hProperties.Get() )
	{
		m_hProperties->DeleteSelectedFunctions( );
	}
}
