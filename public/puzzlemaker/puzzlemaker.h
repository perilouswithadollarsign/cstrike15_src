//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: interface for the Portal2 puzzle maker within client.dll
//
//==========================================================================//
#ifndef PUZZLEMAKER_H
#define PUZZLEMAKER_H

typedef unsigned long long uint64;
// QT needs to think these are QStrings, and the engine needs to think these
//	are CUtlStrings, but we're going to define QStrings as a derived class in
//  puzzlemaker_internal later
#if defined (QT_DLL)
	#define CUtlString QString
#endif

#define SAVE_FILE_VERSION_NUMBER 14

struct PuzzleFilesInfo_t
{
	CUtlString	m_strPuzzleTitle;
	CUtlString	m_strDescription;
	CUtlString	m_strPuzzleFileName;
	CUtlString	m_strMapFileName;
	CUtlString	m_strScreenshotFileName;
	CUtlString	m_strSteamIDPath;
	uint64		m_uTimeStamp_Created;
	uint64		m_uTimeStamp_Modified;
	uint64		m_uFileID;
	bool		m_bIsCoop;
};

enum PuzzleCompileSteps
{
	PUZZLE_COMPILE_NONE = 0,
	PUZZLE_COMPILE_EXPORT,
	PUZZLE_COMPILE_VBSP,
	PUZZLE_COMPILE_VVIS,
	PUZZLE_COMPILE_VRAD
};

enum PuzzleMakerQuitReason_t
{
	PUZZLEMAKER_QUIT_TO_MAINMENU = 0,
	PUZZLEMAKER_QUIT_APPLICATION,
	PUZZLEMAKER_QUIT_TO_ACCEPT_COOP_INVITE
};

#if defined (QT_DLL)
	#undef CUtlString
#endif

#if !defined( QT_DLL ) && defined( PORTAL2_PUZZLEMAKER )

#include "vgui_controls/frame.h"
#include "materialsystem/materialsystemutil.h"

// forward declarations
enum	ButtonCode_t;
class	CBaseGameSystem;
struct	GLGlobalState;
class	CPuzzleMakerFrame;
class	CPuzzleTexture;
class	CPuzzleModel;
class	QString;
class	IQEvent;
class	QKeyEvent;
class	CQP2EditorMainWindow;
class	KeyValues;
class   IGameEventManager2;

#if defined( PUZZLEMAKER_DLL_IMPORT )
// The Client DLL imports g_pPuzzleMaker from the PuzzleMaker DLL
#define PUZZLEMAKER_EXPORT			DLL_IMPORT
#define PUZZLEMAKER_CLASS_EXPORT	DLL_CLASS_IMPORT
#elif defined( PUZZLEMAKER_DLL_EXPORT )
// The PuzzleMaker DLL exports g_pPuzzleMaker
#define PUZZLEMAKER_EXPORT			DLL_EXPORT
#define PUZZLEMAKER_CLASS_EXPORT	DLL_CLASS_EXPORT
#else
// g_pPuzzleMaker is linked directly via the PuzzleMaker static LIB
#define PUZZLEMAKER_EXPORT			extern
#define PUZZLEMAKER_CLASS_EXPORT
#endif

typedef void(*ScreenshotCallback_t)( const char * );


//-----------------------------------------------------------------------------
// IPuzzleMaker is the puzzlemaker interface for client.dll
//-----------------------------------------------------------------------------
abstract_class PUZZLEMAKER_CLASS_EXPORT IPuzzleMaker
{
public:
	// 'AppSystem lite' Connect function, to set up global interface pointers for the PuzzleMaker DLL:
	virtual void Connect(	IFileSystem *_g_pFullFileSystem, IVEngineClient *_engine, IMaterialSystem *_materials, CGlobalVarsBase *_gpGlobals,
							IMDLCache *_g_pMDLCache, IVModelInfoClient *_modelinfo, IVModelRender *_modelrender, IStudioRender *_g_pStudioRender,
							vgui::ISurface *_g_pVGuiSurface, vgui::IInput *_g_pVGuiInput, vgui::IVGUILocalize *_g_pVGuiLocalize,
							IProcessUtils *_g_pProcessUtils, ICvar *_g_pCVar, IGameEventManager2 *_g_pGameEventManager ) = 0;

	// Call this from the parent frame's 'ApplySchemeSettings' to set up the puzzlemaker's UI (fonts, etc)
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )					= 0;

	// Show/hide the puzzlemaker
	virtual void Show( bool bShow )												= 0;

	// Returns FALSE if the puzzlemaker is fully inactive (invisible), otherwise TRUE
	virtual bool IsVisible( void )												= 0;

	// Update state once per frame (only render the puzzlemaker if this returns TRUE)
	virtual bool FrameUpdate( bool bIgnore = false )							= 0;

	// Puzzlemaker render calls, in the order in which they should be called:
	//  1. Render the puzzlemaker's main 3D view (optionally with shadows)
	//  2. Render geometry to be highlighted with a screen-space glow outline
	//  3. Render the UI and localized UI text (call this from the parent frame's 'Paint')
	virtual void RenderPuzzleMaker( FlashlightState_t	*pFlashlight	= NULL,
									CTextureReference	*pDepthTexture	= NULL,
									CTextureReference	*pColorTexture	= NULL,
									VMatrix				*pWorldToShadow	= NULL) = 0;
	virtual void RenderPuzzleMakerGlow( void )									= 0;
	virtual void RenderPuzzleMakerUI( int xOrigin, int yOrigin )				= 0;

	// Should the main in-game world be rendered? (sometimes returns true even while the puzzlemaker is visible)
	virtual bool ShouldRenderWorld( void )										= 0;

	// Call this after rendering the main in-game world, so the puzzlemaker can update the full-frame texture
	virtual void UpdateSnapshot( ITexture *pTexture )							= 0;

	// Get the parameters for the shadow camera (returns FALSE if no shadows desired):
	virtual bool GetDepthShadowState( FlashlightState_t &flashlightState )		= 0;

	// Input-handling
	virtual void OnKeyCodeTyped(		ButtonCode_t code )						= 0;
	virtual void OnKeyCodeReleased(		ButtonCode_t code )						= 0;
	virtual void OnMousePressed(		ButtonCode_t code )						= 0;
	virtual void OnMouseReleased(		ButtonCode_t code )						= 0;
	virtual void OnMouseDoublePressed(	ButtonCode_t code )						= 0;
	virtual void OnMouseWheeled(		int delta )								= 0;
	virtual void OnCursorMoved(			int x, int y )							= 0;

	// UI communication
	virtual void NewPuzzle( bool bFromGameMenu )								= 0;
	virtual void LoadPuzzle( const char *name )									= 0;
	virtual void SavePuzzle( bool bGenerateFileName )							= 0;
	virtual void CompilePuzzle( void )											= 0;
	virtual void CancelCompile( void )											= 0;
	virtual bool IsCompiling( void )											= 0;
	virtual void SetActive( bool bActive )										= 0;
	virtual bool GetActive() const												= 0;
	virtual bool HasUnsavedChanges( void )										= 0;
	virtual bool HasUncompiledChanges( void )									= 0;
	virtual bool HasErrors( void )												= 0;
	virtual bool IsOverLimits( void )											= 0;
	virtual bool IsSaving( void )												= 0;
	virtual void TakeScreenshotAsync( void(*pt2Callback)( const char * ), bool bUsePuzzleName, bool bAutoSave = false )		= 0;
	virtual const PuzzleFilesInfo_t& GetPuzzleInfo() const						= 0;
	virtual void SetPuzzleInfo( const PuzzleFilesInfo_t &publishInfo )			= 0;
	virtual bool CanShowInGameMenu( void )										= 0;
	virtual void RestartSounds( void )											= 0;
	virtual void StopSounds( void )												= 0;
	virtual float GetCurrentCompileProgress( int *pnFailedErrorCode, CUtlString *pstrFailedProcess, PuzzleCompileSteps *peCompileStep )	= 0;
	virtual void GetTagsForCurrentPuzzle( CUtlVector< const char * > &vecTags ) = 0;
	virtual bool RequestQuitGame( PuzzleMakerQuitReason_t reason )				= 0;
	virtual bool CanQuitGame( void )											= 0;
	virtual void QuitGame( void )												= 0;

	// CBaseGameSystem hooks (should be called by a wrapper gamesystem)
	// NOTES: PostInit must happen after all other VGui initialization (i.e after InitGameSystems).
	//        Assets are loaded on entry to puzzlemaker mode and unloaded on exit to the main menu.
	virtual void PostInit( CPuzzleMakerFrame *pFrame )							= 0;
	virtual void LevelInitPreEntity( void )										= 0;
	virtual void LevelInitPostEntity( void )									= 0;
	virtual void LevelShutdownPostEntity( void )								= 0;
	virtual void Shutdown( void )												= 0;

	// Methods for development-only concommands (non-UI backdoors for Load/Save/Export/Compile/Publish):
	virtual void LoadPuzzle_Dev( const char *name )								= 0;
	virtual void SavePuzzle_Dev( const char *name )								= 0;
	virtual void ExportPuzzle_Dev( const char *name )							= 0;
	virtual void PublishPuzzle_Dev( void )										= 0;
	virtual void AutoSave_Dev( void )											= 0;

	virtual void UpdateSaveFileVersion( KeyValues *pKeyValues, const char* pFileName )	= 0;
	virtual bool IsInCoopSession( void )										= 0;
};

// Global puzzlemaker singleton (exported from the PuzzleMaker DLL/LIB to the client DLL):
PUZZLEMAKER_EXPORT IPuzzleMaker * g_pPuzzleMaker;

// Global puzzlemaker gamesystem singleton (implemented in vpuzzlemaker.cpp in the client dll)
extern CBaseGameSystem* g_pPuzzleMakerGameSystem;



//-----------------------------------------------------------------------------
// CPuzzleMakerFrame - the fullscreen UI frame which wraps the puzzlemaker
//                     (implemented in vpuzzlemaker.cpp in the client dll)
//-----------------------------------------------------------------------------
class CPuzzleMakerFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CPuzzleMakerFrame, vgui::Frame );

public:
	CPuzzleMakerFrame( void );
	virtual ~CPuzzleMakerFrame();

	virtual void Paint() OVERRIDE;

	virtual void PrecacheSound( const char *pszSoundName );
	virtual int PlaySoundEffect( const char *pszSoundName );
	virtual void StopSoundByGUID( int nGUID );

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme )	OVERRIDE;
	virtual void PerformLayout( void ) OVERRIDE;
	virtual void OnThink( void ) OVERRIDE;

	// Forward input events to g_pPuzzleMaker (from our parent panel):
	// [NOTE: MOUSE_WHEEL_UP and MOUSE_WHEEL_DOWN are received by OnMousePressed (with no associated 'release' event), OnMouseWheeled is not called]
	virtual void OnKeyCodeTyped( vgui::KeyCode code )			OVERRIDE { g_pPuzzleMaker->OnKeyCodeTyped(		code  );	}
	virtual void OnKeyCodeReleased( vgui::KeyCode code )		OVERRIDE { g_pPuzzleMaker->OnKeyCodeReleased(	code  );	}
	virtual void OnMousePressed( vgui::MouseCode code )			OVERRIDE { g_pPuzzleMaker->OnMousePressed(		code  );	}
	virtual void OnMouseReleased( vgui::MouseCode code )		OVERRIDE { g_pPuzzleMaker->OnMouseReleased(		code  );	}
	virtual void OnMouseDoublePressed( vgui::MouseCode code )	OVERRIDE { g_pPuzzleMaker->OnMouseDoublePressed(code  );	}
	virtual void OnMouseWheeled( int delta )					OVERRIDE { g_pPuzzleMaker->OnMouseWheeled(		delta );	}
	virtual void OnCursorMoved( int x, int y );
	// Forward the 'show' message to g_pPuzzleMaker (from our parent panel):
	MESSAGE_FUNC_INT( OnShow, "Show", show )							 { g_pPuzzleMaker->Show( !!show ); }

private:
	void AdjustBounds( void );
	bool ShadowMapPreRender( FlashlightState_t &flashlightState, CTextureReference &shadowDepthTexture, CTextureReference &shadowColorTexture, VMatrix &worldToShadow );

	vgui::VPANEL m_hLastFocusPanel;
};


#endif // PORTAL2_PUZZLEMAKER

#endif // PUZZLEMAKER_H
