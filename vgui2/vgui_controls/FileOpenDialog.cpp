//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Implementation of vgui generic open file dialog
//
// $NoKeywords: $
//===========================================================================//


#define PROTECTED_THINGS_DISABLE

#if defined( WIN32 )
#if !defined( _GAMECONSOLE )
#include "winlite.h"
#include <shellapi.h>
#endif
#elif defined( POSIX )
#include <stdlib.h>
#define _stat stat
#define _wcsnicmp wcsncmp
#else
#error
#endif

#undef GetCurrentDirectory
#include "filesystem.h"
#include <sys/stat.h>

#include "tier1/utldict.h"
#include "tier1/utlstring.h"

#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <keyvalues.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <vgui/IInput.h>

#include <vgui_controls/FileOpenDialog.h>

#include <vgui_controls/Button.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/InputDialog.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/ImageList.h>
#include <vgui_controls/MenuItem.h>
#include <vgui_controls/Tooltip.h>

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#undef GetCurrentDirectory
#endif

#if defined( _PS3 )
#include "ps3/ps3_core.h"
#include "ps3/ps3_win32stubs.h"
#undef GetCurrentDirectory
#endif

#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

static int s_nLastSortColumn = 0;
static const int MAX_FILTER_LENGTH = 255;
static const int MAX_SEARCH_HISTORY = 8;

static int ListFileNameSortFunc(ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	NOTE_UNUSED( pPanel );

	bool dir1 = item1.kv->GetInt("directory") == 1;
	bool dir2 = item2.kv->GetInt("directory") == 1;

	// if they're both not directories of files, return if dir1 is a directory (before files)
	if (dir1 != dir2)
	{
		return dir1 ? -1 : 1;
	}

	const char *string1 = item1.kv->GetString("text");
	const char *string2 = item2.kv->GetString("text");

	// YWB:  Mimic windows behavior where filenames starting with numbers are sorted based on numeric part
	int num1 = Q_atoi( string1 );
	int num2 = Q_atoi( string2 );

	if ( num1 != 0 && 
		 num2 != 0 )
	{
		if ( num1 < num2 )
			return -1;
		else if ( num1 > num2 )
			return 1;
	}

	// Push numbers before everything else
	if ( num1 != 0 )
	{
		return -1;
	}
	
	// Push numbers before everything else
	if ( num2 != 0 )
	{
		return 1;
	}

	return Q_stricmp( string1, string2 );
}

static int ListBaseStringSortFunc(ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2, char const *fieldName )
{
	bool dir1 = item1.kv->GetInt("directory") == 1;
	bool dir2 = item2.kv->GetInt("directory") == 1;

	// if they're both not directories of files, return if dir1 is a directory (before files)
	if (dir1 != dir2)
	{
		return -1;
	}

	const char *string1 = item1.kv->GetString(fieldName);
	const char *string2 = item2.kv->GetString(fieldName);
	int cval = Q_stricmp(string1, string2);
	if ( cval == 0 )
	{
		// Use filename to break ties
		return ListFileNameSortFunc( pPanel, item1, item2 );
	}

	return cval;
}

static int ListBaseInteger64SortFunc(ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2, char const *fieldName )
{
	bool dir1 = item1.kv->GetInt("directory") == 1;
	bool dir2 = item2.kv->GetInt("directory") == 1;

	// if they're both not directories of files, return if dir1 is a directory (before files)
	if ( dir1 != dir2 )
	{
		return dir1 ? -1 : 1;
	}

	int64 n1 = (int64)item1.kv->GetUint64( fieldName );
	int64 n2 = (int64)item2.kv->GetUint64( fieldName );

	if ( n1 == n2 )
	{
		// Use filename to break ties
		return ListFileNameSortFunc( pPanel, item1, item2 );
	}

	return ( n1 < n2 ) ? -1 : 1;
}

static int ListFileSizeSortFunc(ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	return ListBaseInteger64SortFunc( pPanel, item1, item2, "filesizeint" );
}

static int ListFileModifiedSortFunc(ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	// NOTE: Backward order to get most recent files first
	return ListBaseInteger64SortFunc( pPanel, item2, item1, "modifiedint" );
}
static int ListFileCreatedSortFunc(ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	// NOTE: Backward order to get most recent files first
	return ListBaseInteger64SortFunc( pPanel, item2, item1, "createdint" );
}
static int ListFileAttributesSortFunc(ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	return ListBaseStringSortFunc( pPanel, item1, item2, "attributes" );
}
static int ListFileTypeSortFunc(ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	return ListBaseStringSortFunc( pPanel, item1, item2, "type" );
}



namespace vgui
{

class FileNameComboBox : public ComboBox
{
	DECLARE_CLASS_SIMPLE( FileNameComboBox, ComboBox );

public:
	FileNameComboBox(FileOpenDialog *parent, const char *panelName, int numLines, bool allowEdit) :
	   BaseClass( parent, panelName, numLines, allowEdit )
	{
	}

	virtual void OnKeyCodeTyped( KeyCode code )
	{
		if ( code == KEY_ENTER && !IsDropdownVisible() )
		{
			// Post to parent
			CallParentFunction(new KeyValues("KeyCodeTyped", "code", code));
			return;
		}

		BaseClass::OnKeyCodeTyped( code );
	}

	virtual void OnMenuItemSelected()
	{
		BaseClass::OnMenuItemSelected();
		PostMessage( GetVParent(), new KeyValues( "OnMatchStringSelected" ) );
	}
};

} // namespace vgui


//-----------------------------------------------------------------------------
// Dictionary of start dir contexts 
//-----------------------------------------------------------------------------
static CUtlDict< CUtlString, unsigned short > s_StartDirContexts;

struct ColumnInfo_t
{
	char const	*columnName;
	char const	*columnText;
	int			startingWidth;
	int			minWidth;
	int			maxWidth;
	int			flags;
	SortFunc	*pfnSort;
	Label::Alignment alignment;
};

static ColumnInfo_t g_ColInfo[] =
{
	{	"text",				"#FileOpenDialog_Col_Name",				175,	20, 10000, ListPanel::COLUMN_UNHIDABLE,		&ListFileNameSortFunc			, Label::a_west },
	{	"filesize",			"#FileOpenDialog_Col_Size",				100,	20, 10000, 0,								&ListFileSizeSortFunc			, Label::a_east },
	{	"type",				"#FileOpenDialog_Col_Type",				150,	20, 10000, 0,								&ListFileTypeSortFunc			, Label::a_west },
	{	"modified",			"#FileOpenDialog_Col_DateModified",		125,	20, 10000, 0,								&ListFileModifiedSortFunc		, Label::a_west },
//	{	"created",			"#FileOpenDialog_Col_DateCreated",		125,	20, 10000, ListPanel::COLUMN_HIDDEN,		&ListFileCreatedSortFunc		, Label::a_west },
	{	"attributes",		"#FileOpenDialog_Col_Attributes",		50,		20, 10000, ListPanel::COLUMN_HIDDEN,		&ListFileAttributesSortFunc		, Label::a_west },
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
FileOpenDialog::FileOpenDialog(Panel *parent, const char *title, bool bOpenOnly, KeyValues* pContextKeyValues ) : 
	Frame( parent, "FileOpenDialog" )
{
	m_DialogType = bOpenOnly ? FOD_OPEN : FOD_SAVE;
	Init( title, pContextKeyValues );
}


FileOpenDialog::FileOpenDialog( Panel *parent, const char *title, FileOpenDialogType_t type, KeyValues *pContextKeyValues ) : 
	Frame( parent, "FileOpenDialog" )
{
	m_DialogType = type;
	Init( title, pContextKeyValues );
}

void FileOpenDialog::Init( const char *title, KeyValues *pContextKeyValues )
{
	// By default, delete self on close
	SetDeleteSelfOnClose( true );

	m_bFileSelected = false;
	SetTitle(title, true);
	SetMinimizeButtonVisible(false);
#ifdef POSIX
	V_strncpy(m_szLastPath, "/", sizeof( m_szLastPath ) );	
#else
	V_strncpy(m_szLastPath, "c:\\", sizeof( m_szLastPath ) );
#endif
	m_pContextKeyValues = pContextKeyValues;

	// Get the list of available drives and put them in a menu here.
	// Start with the directory we are in.
	m_pFullPathEdit = new ComboBox(this, "FullPathEdit", 6, false);
	m_pFullPathEdit->GetTooltip()->SetTooltipFormatToSingleLine();

	// list panel
	m_pFileList = new ListPanel(this, "FileList");
	for ( int i = 0; i < ARRAYSIZE( g_ColInfo ); ++i )
	{
		const ColumnInfo_t& info = g_ColInfo[ i ];

		m_pFileList->AddColumnHeader( i, info.columnName, info.columnText, info.startingWidth, info.minWidth, info.maxWidth, info.flags );
		m_pFileList->SetSortFunc( i, info.pfnSort );
		m_pFileList->SetColumnTextAlignment( i, info.alignment );
	}

	m_pFileList->SetSortColumn( s_nLastSortColumn );
	m_pFileList->SetMultiselectEnabled( false );

	// file name edit box
	m_pFileNameCombo = new FileNameComboBox(this, "FileNameCombo", 6, true ); 
	m_pFileTypeCombo = new ComboBox( this, "FileTypeCombo", 6, false );

	switch ( m_DialogType )
	{
	case FOD_OPEN:
		m_pOpenButton = new Button( this, "OpenButton", "#FileOpenDialog_Open", this );
		break;
	case FOD_SAVE:
		m_pOpenButton = new Button( this, "OpenButton", "#FileOpenDialog_Save", this );
		break;
	case FOD_SELECT_DIRECTORY:
		m_pOpenButton = new Button( this, "OpenButton", "#FileOpenDialog_Select", this );
		m_pFileTypeCombo->SetVisible( false );
		break;
	}

	m_pCancelButton = new Button( this, "CancelButton", "#FileOpenDialog_Cancel", this );
	m_pFolderUpButton = new Button( this, "FolderUpButton", "", this );
	m_pFolderUpButton->GetTooltip()->SetText( "#FileOpenDialog_ToolTip_Up" );
	m_pNewFolderButton = new Button( this, "NewFolderButton", "", this );
	m_pNewFolderButton->GetTooltip()->SetText( "#FileOpenDialog_ToolTip_NewFolder" );
	m_pOpenInExplorerButton = new Button( this, "OpenInExplorerButton", "", this );
#if defined ( OSX )	
	m_pOpenInExplorerButton->GetTooltip()->SetText( "#FileOpenDialog_ToolTip_OpenInFinderButton" );
#elif defined ( POSIX )
	m_pOpenInExplorerButton->GetTooltip()->SetText( "#FileOpenDialog_ToolTip_OpenInDesktopManagerButton" );
#else // Assume Windows / Explorer
	m_pOpenInExplorerButton->GetTooltip()->SetText( "#FileOpenDialog_ToolTip_OpenInExplorerButton" );
#endif
	Label *lookIn  = new Label( this, "LookInLabel", "#FileOpenDialog_Look_in" );
	Label *fileName = new Label( this, "FileNameLabel", 
		( m_DialogType != FOD_SELECT_DIRECTORY ) ? "#FileOpenDialog_File_name" : "#FileOpenDialog_Directory_Name" );

	// set up the control's initial positions
	SetSize( 600, 260 );

	int nFileEditLeftSide = ( m_DialogType != FOD_SELECT_DIRECTORY ) ? 84 : 100;
	int nFileNameWidth = ( m_DialogType != FOD_SELECT_DIRECTORY ) ? 72 : 82;

	m_pFullPathEdit->SetBounds(67, 32, 310, 24);
	m_pFolderUpButton->SetBounds(362, 32, 24, 24);
	m_pNewFolderButton->SetBounds(392, 32, 24, 24);
	m_pOpenInExplorerButton->SetBounds(332, 32, 24, 24);
	m_pFileList->SetBounds(10, 60, 406, 130);
	m_pFileNameCombo->SetBounds( nFileEditLeftSide, 194, 238, 24);
	m_pFileTypeCombo->SetBounds( nFileEditLeftSide, 224, 238, 24);
	m_pOpenButton->SetBounds(336, 194, 74, 24);
	m_pCancelButton->SetBounds(336, 224, 74, 24);
	lookIn->SetBounds(10, 32, 55, 24);
	fileName->SetBounds(10, 194, nFileNameWidth, 24);

	// set autolayout parameters
	m_pFullPathEdit->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_RIGHT, 67, 32, -100, 0 );
	m_pFileNameCombo->SetAutoResize( Panel::PIN_BOTTOMLEFT, Panel::AUTORESIZE_RIGHT, nFileEditLeftSide, -42, -104, 0 );
	m_pFileTypeCombo->SetAutoResize( Panel::PIN_BOTTOMLEFT, Panel::AUTORESIZE_RIGHT, nFileEditLeftSide, -12, -104, 0 );
	m_pFileList->SetAutoResize( Panel::PIN_TOPLEFT, Panel::AUTORESIZE_DOWNANDRIGHT, 10, 60, -10, -70 );

	m_pFolderUpButton->SetPinCorner( Panel::PIN_TOPRIGHT, -40, 32 );
	m_pNewFolderButton->SetPinCorner( Panel::PIN_TOPRIGHT, -10, 32 );
	m_pOpenInExplorerButton->SetPinCorner( Panel::PIN_TOPRIGHT, -70, 32 );
	m_pOpenButton->SetPinCorner( Panel::PIN_BOTTOMRIGHT, -16, -42 );
	m_pCancelButton->SetPinCorner( Panel::PIN_BOTTOMRIGHT, -16, -12 );
	lookIn->SetPinCorner( Panel::PIN_TOPLEFT, 10, 32 );
	fileName->SetPinCorner( Panel::PIN_BOTTOMLEFT, 10, -42 );

	// label settings
	lookIn->SetContentAlignment(Label::a_west);
	fileName->SetContentAlignment(Label::a_west);

	lookIn->SetAssociatedControl(m_pFullPathEdit);
	fileName->SetAssociatedControl(m_pFileNameCombo);

	if ( m_DialogType != FOD_SELECT_DIRECTORY )
	{
		Label *fileType = new Label(this, "FileTypeLabel", "#FileOpenDialog_File_type");
		fileType->SetBounds(10, 224, 72, 24);
		fileType->SetPinCorner( Panel::PIN_BOTTOMLEFT, 10, -12 );
		fileType->SetContentAlignment(Label::a_west);
		fileType->SetAssociatedControl( m_pFileTypeCombo );
	}

	// set tab positions
	GetFocusNavGroup().SetDefaultButton(m_pOpenButton);

	m_pFileNameCombo->SetTabPosition(1);
	m_pFileTypeCombo->SetTabPosition(2);
	m_pOpenButton->SetTabPosition(3);
	m_pCancelButton->SetTabPosition(4);
	m_pFullPathEdit->SetTabPosition(5);
	m_pFileList->SetTabPosition(6);

	m_pOpenButton->SetCommand( ( m_DialogType != FOD_SELECT_DIRECTORY ) ? new KeyValues( "OnOpen" ) : new KeyValues( "SelectFolder" ) );
	m_pCancelButton->SetCommand( "CloseModal" );
	m_pFolderUpButton->SetCommand( new KeyValues( "OnFolderUp" ) );
	m_pNewFolderButton->SetCommand( new KeyValues( "OnNewFolder" ) );
	m_pOpenInExplorerButton->SetCommand( new KeyValues( "OpenInExplorer" ) );

	SetSize( 600, 384 );

	m_nStartDirContext = s_StartDirContexts.InvalidIndex();

	// Set our starting path to the current directory
	char pLocalPath[255];
	g_pFullFileSystem->GetCurrentDirectory( pLocalPath , 255 );
	SetStartDirectory( pLocalPath );

	// Because these call through virtual functions, we can't issue them in the constructor, so we post a message to ourselves instead!!
	PostMessage( GetVPanel(), new KeyValues( "PopulateFileList" ) );
	PostMessage( GetVPanel(), new KeyValues( "PopulateDriveList" ) );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
FileOpenDialog::~FileOpenDialog()
{
	s_nLastSortColumn = m_pFileList->GetSortColumn();
	if ( m_pContextKeyValues )
	{
		m_pContextKeyValues->deleteThis();
		m_pContextKeyValues = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Apply scheme settings
//-----------------------------------------------------------------------------
void FileOpenDialog::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_pFolderUpButton->SetImage( scheme()->GetImage("resource/icon_folderup", false), 0 );
	m_pFolderUpButton->SetContentAlignment( Label::a_center );
	m_pNewFolderButton->SetImage( scheme()->GetImage("resource/icon_newfolder", false), 0 );
	m_pNewFolderButton->SetContentAlignment( Label::a_center );
	m_pOpenInExplorerButton->SetImage( scheme()->GetImage("resource/icon_explore", false), 0 );
	m_pOpenInExplorerButton->SetContentAlignment( Label::a_center );

	ImageList *imageList = new ImageList(false);
	imageList->AddImage(scheme()->GetImage("resource/icon_file", false));
	imageList->AddImage(scheme()->GetImage("resource/icon_folder", false));
	imageList->AddImage(scheme()->GetImage("resource/icon_folder_selected", false));

	m_pFileList->SetImageList(imageList, true);
}


//-----------------------------------------------------------------------------
// Prevent default button ('select') from getting triggered
// when selecting directories. Instead, open the directory
//-----------------------------------------------------------------------------
void FileOpenDialog::OnKeyCodeTyped(KeyCode code)
{
	if ( m_DialogType == FOD_SELECT_DIRECTORY && code == KEY_ENTER )
	{
		OnOpen();
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void FileOpenDialog::PopulateDriveList()
{
	char fullpath[MAX_PATH * 4];
	char subDirPath[MAX_PATH * 4];
	GetCurrentDirectory(fullpath, sizeof(fullpath) - MAX_PATH);
	Q_strncpy(subDirPath, fullpath, sizeof( subDirPath ) );

	m_pFullPathEdit->DeleteAllItems();

	// populate the drive list
	char buf[512];
	int len = system()->GetAvailableDrives(buf, 512);
	char *pBuf = buf;
	for (int i=0; i < len / 4; i++)
	{
		m_pFullPathEdit->AddItem(pBuf, NULL);

		// is this our drive - add all subdirectories
		if (!_strnicmp(pBuf, fullpath, 2))
		{
			int indent = 0;
			char *pData = fullpath;
			while (*pData)
			{
				if (*pData == CORRECT_PATH_SEPARATOR )
				{
					if (indent > 0)
					{
						memset(subDirPath, ' ', indent);
						memcpy(subDirPath+indent, fullpath, pData-fullpath);
						subDirPath[indent+pData-fullpath] = 0;

						m_pFullPathEdit->AddItem(subDirPath, NULL);
					}
					indent += 2;
				}
				pData++;
			}
		}
		pBuf += 4;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Delete self on close
//-----------------------------------------------------------------------------
void FileOpenDialog::OnClose()
{
	s_nLastSortColumn = m_pFileList->GetSortColumn();
	if ( !m_bFileSelected )
	{
		KeyValues *pKeyValues = new KeyValues( "FileSelectionCancelled" );
		PostActionSignal( pKeyValues );
		m_bFileSelected = true;
	}

	m_pFileNameCombo->SetText("");
	m_pFileNameCombo->HideMenu();

	if ( vgui::input()->GetAppModalSurface() == GetVPanel() )
	{
		input()->SetAppModalSurface(NULL);
	}

	BaseClass::OnClose();
}

void FileOpenDialog::OnFolderUp()
{
	MoveUpFolder();
	OnOpen();
}

void FileOpenDialog::OnInputCompleted( KeyValues *data )
{
	if ( m_hInputDialog.Get() )
	{
		delete m_hInputDialog.Get();
	}

	input()->SetAppModalSurface( m_SaveModal );
	m_SaveModal = 0;

	NewFolder( data->GetString( "text" ) );
	OnOpen();
}

void FileOpenDialog::OnInputCanceled()
{
	input()->SetAppModalSurface( m_SaveModal );
	m_SaveModal = 0;
}

void FileOpenDialog::OnNewFolder()
{
	if ( m_hInputDialog.Get() )
		delete m_hInputDialog.Get();

	m_hInputDialog = new InputDialog( this, "#FileOpenDialog_NewFolder_InputTitle", "#FileOpenDialog_NewFolderPrompt", "#FileOpenDialog_NewFolder_DefaultName" );
	if ( m_hInputDialog.Get() )
	{
		m_SaveModal = input()->GetAppModalSurface();

		KeyValues *pContextKeyValues = new KeyValues( "NewFolder" );
		m_hInputDialog->SetSmallCaption( true );
		m_hInputDialog->SetMultiline( false );
		m_hInputDialog->DoModal( pContextKeyValues );
	}
}


//-----------------------------------------------------------------------------
// Opens the current file/folder in explorer
//-----------------------------------------------------------------------------
void FileOpenDialog::OnOpenInExplorer()
{
	char pCurrentDirectory[MAX_PATH];
	GetCurrentDirectory( pCurrentDirectory, sizeof(pCurrentDirectory) );
#if defined( WIN32 )
#if !defined( _GAMECONSOLE )
	ShellExecute( NULL, NULL, pCurrentDirectory, NULL, NULL, SW_SHOWNORMAL );
#endif
#elif defined( OSX )
	char szCmd[ MAX_PATH ];
	Q_snprintf( szCmd, sizeof(szCmd), "/usr/bin/open \"%s\"", pCurrentDirectory );
	::system( szCmd );
#elif defined( LINUX )
	DevMsg( "FileOpenDialog::OnOpenInExplorer unimplemented under LINUX\n" );
#endif
}

void FileOpenDialog::AddSearchHistoryString( char const *str )
{
	// See if it's already in list
	for ( int i = 0; i < m_SearchHistory.Count(); ++i )
	{
		if ( !Q_stricmp( str, m_SearchHistory[ i ].String() )  )
			return;
	}

	while ( m_SearchHistory.Count() > MAX_SEARCH_HISTORY )
	{
		m_SearchHistory.Remove( m_SearchHistory.Count() - 1 );
	}

	CUtlString string;
	string = str;
	m_SearchHistory.AddToTail( string );

	PopulateFileNameSearchHistory();
}


//-----------------------------------------------------------------------------
// Purpose: Handle for button commands
//-----------------------------------------------------------------------------
void FileOpenDialog::OnCommand(const char *command)
{
	if (!stricmp(command, "Cancel"))
	{
		Close();
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}


//-----------------------------------------------------------------------------
// Sets the start directory context (and resets the start directory in the process)
//-----------------------------------------------------------------------------
void FileOpenDialog::SetStartDirectoryContext( const char *pStartDirContext, const char *pDefaultDir )
{
	bool bUseCurrentDirectory = true;
	if ( pStartDirContext )
	{
		m_nStartDirContext = s_StartDirContexts.Find( pStartDirContext );
		if ( m_nStartDirContext == s_StartDirContexts.InvalidIndex() )
		{
			m_nStartDirContext = s_StartDirContexts.Insert( pStartDirContext, pDefaultDir );
			bUseCurrentDirectory = ( pDefaultDir == NULL );
		}
		else
		{
			bUseCurrentDirectory = false;
		}
	}
	else
	{
		m_nStartDirContext = s_StartDirContexts.InvalidIndex();
	}

	if ( !bUseCurrentDirectory )
	{
		SetStartDirectory( s_StartDirContexts[m_nStartDirContext].Get() );
	}
	else
	{
		// Set our starting path to the current directory
		char pLocalPath[255];
		g_pFullFileSystem->GetCurrentDirectory( pLocalPath, 255 );
		SetStartDirectory( pLocalPath );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Set the starting directory of the file search.
//-----------------------------------------------------------------------------
void FileOpenDialog::SetStartDirectory( const char *dir )
{
	m_pFullPathEdit->SetText(dir);

	// ensure it's validity
	ValidatePath();

	// Store this in the start directory list
	if ( m_nStartDirContext != s_StartDirContexts.InvalidIndex() )
	{
		char pDirBuf[MAX_PATH];
		GetCurrentDirectory( pDirBuf, sizeof(pDirBuf) );
		s_StartDirContexts[ m_nStartDirContext ] = pDirBuf;
	}

	PopulateDriveList();
}


//-----------------------------------------------------------------------------
// Purpose: Add filters for the drop down combo box
//-----------------------------------------------------------------------------
void FileOpenDialog::AddFilter( const char *filter, const char *filterName, bool bActive, const char *pFilterInfo  )
{
	KeyValues *kv = new KeyValues("item");
	kv->SetString( "filter", filter );
	kv->SetString( "filterinfo", pFilterInfo );
	int itemID = m_pFileTypeCombo->AddItem(filterName, kv);
	if ( bActive )
	{
		m_pFileTypeCombo->ActivateItem(itemID);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Activate the dialog
//-----------------------------------------------------------------------------
void FileOpenDialog::DoModal( bool bUnused )
{
	m_bFileSelected = false;
	m_pFileNameCombo->RequestFocus();
	BaseClass::DoModal();
}


//-----------------------------------------------------------------------------
// Purpose: Gets the directory this is currently in
//-----------------------------------------------------------------------------
void FileOpenDialog::GetCurrentDirectory(char *buf, int bufSize)
{
	// get the text from the text entry
	m_pFullPathEdit->GetText(buf, bufSize);
}


//-----------------------------------------------------------------------------
// Purpose: Get the last selected file name
//-----------------------------------------------------------------------------
void FileOpenDialog::GetSelectedFileName(char *buf, int bufSize)
{
	m_pFileNameCombo->GetText(buf, bufSize);
}


//-----------------------------------------------------------------------------
// Creates a new folder
//-----------------------------------------------------------------------------
void FileOpenDialog::NewFolder( char const *folderName )
{
	char pCurrentDirectory[MAX_PATH];
	GetCurrentDirectory( pCurrentDirectory, sizeof(pCurrentDirectory) );

	char pFullPath[MAX_PATH];
	char pNewFolderName[MAX_PATH];
	Q_strncpy( pNewFolderName, folderName, sizeof(pNewFolderName) );
	int i = 2;
	do
	{
		Q_MakeAbsolutePath( pFullPath, sizeof(pFullPath), pNewFolderName, pCurrentDirectory );
		if ( !g_pFullFileSystem->FileExists( pFullPath, NULL ) &&
			 !g_pFullFileSystem->IsDirectory( pFullPath, NULL ) )
		{
			g_pFullFileSystem->CreateDirHierarchy( pFullPath, NULL );
			m_pFileNameCombo->SetText( pNewFolderName );
			return;
		}

		Q_snprintf( pNewFolderName, sizeof(pNewFolderName), "%s%d", folderName, i );
		++i;
	} while ( i <= 999 );
}


//-----------------------------------------------------------------------------
// Purpose: Move the directory structure up
//-----------------------------------------------------------------------------
void FileOpenDialog::MoveUpFolder()
{
	char fullpath[MAX_PATH * 4];
	GetCurrentDirectory(fullpath, sizeof(fullpath) - MAX_PATH);

	// strip it back
	char *pos = strrchr(fullpath, CORRECT_PATH_SEPARATOR );
	if (pos)
	{
		*pos = 0;

		if (!pos[1])
		{
			pos = strrchr(fullpath, CORRECT_PATH_SEPARATOR );
			if (pos)
			{
				*pos = 0;
			}
		}
	}

	// append a trailing slash
	Q_strncat(fullpath, CORRECT_PATH_SEPARATOR_S, sizeof( fullpath ), COPY_ALL_CHARACTERS );

	SetStartDirectory(fullpath);
	PopulateFileList();
	InvalidateLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Validate that the current path is valid
//-----------------------------------------------------------------------------
void FileOpenDialog::ValidatePath()
{
	char fullpath[MAX_PATH * 4];
	GetCurrentDirectory(fullpath, sizeof(fullpath) - MAX_PATH);
	Q_RemoveDotSlashes( fullpath );

	// when statting a directory on Windows, you want to include
	// the terminal slash exactly when you are statting a root
	// directory. PKMN.
#ifdef _WIN32
	if ( Q_strlen( fullpath ) != 3 )
	{
		Q_StripTrailingSlash( fullpath );
	}
#endif
	
	struct _stat buf;
	if ( ( 0 == _stat( fullpath, &buf ) ) &&
		( 0 != ( buf.st_mode & S_IFDIR ) ) )
	{
		Q_AppendSlash( fullpath, sizeof( fullpath ) );
		Q_strncpy(m_szLastPath, fullpath, sizeof(m_szLastPath));
	}
	else
	{
		// failed to load file, use the previously successful path
	}	

	m_pFullPathEdit->SetText(m_szLastPath);
	m_pFullPathEdit->GetTooltip()->SetText(m_szLastPath);
}


static void InitFileData( bool bDirectory, char const *pszFileName, const char *pchDirectoryName, FileData_t &data )
{	
	data.m_FileName = V_UnqualifiedFileName( pszFileName );
	data.m_FullPath = CFmtStr( "%s%s", pchDirectoryName, pszFileName );
	Q_FixSlashes( data.m_FullPath.Get() );

	if ( !bDirectory )
	{
		g_pFullFileSystem->GetFileTypeForFullPath( data.m_FullPath, data.m_FileType, sizeof( data.m_FileType ) );
	}

	data.m_bDirectory = bDirectory;
	data.m_nFileSize = g_pFullFileSystem->Size( data.m_FullPath.Get() );

	if ( !g_pFullFileSystem->IsFileWritable( data.m_FullPath.Get() ) )
		data.m_FileAttributes = "R";

	long fileModified = g_pFullFileSystem->GetFileTime( data.m_FullPath.Get() );
	char pszFileModified[64];
	g_pFullFileSystem->FileTimeToString( pszFileModified, sizeof( pszFileModified ), fileModified );
	data.m_LastWriteTime = pszFileModified;
	data.m_nLastWriteTime = fileModified;
}


void FileData_t::PrepareKV( KeyValues *kv )
{
	// add the file to the list
	kv->SetString("text", m_FileName );

	kv->SetInt("directory", m_bDirectory ? 1 : 0 );
	kv->SetInt("image", m_bDirectory ? 2 : 1 );
	kv->SetInt("imageSelected", m_bDirectory ? 3 : 1 );

	kv->SetPtr( "iconImage", NULL );

	if ( !m_bDirectory )
	{
		IImage *image = surface()->GetIconImageForFullPath( m_FullPath.String() );
		if ( image )
		{
			kv->SetPtr( "iconImage", (void *)image );
		}
		kv->SetUint64( "filesizeint", (uint64)m_nFileSize );
		kv->SetString( "filesize", Q_pretifymem( (float)m_nFileSize, 0, true ) );
		kv->SetWString( "type", m_FileType );

	}
	else
	{
		kv->SetUint64( "filesizeint", (uint64)0 );
		kv->SetString( "filesize", "" );
		kv->SetString( "type", "#FileOpenDialog_FileType_Folder" );
	}
	
	kv->SetString( "attributes", m_FileAttributes );
	kv->SetString( "modified", m_LastWriteTime );
	kv->SetString( "created", m_CreationTime );
	kv->SetUint64( "modifiedint", m_nLastWriteTime );
	kv->SetUint64( "createdint", m_nCreationTime );
}

void FileOpenDialog::BuildFileList()
{
	m_Files.RemoveAll();
	m_Filtered.RemoveAll();

#ifndef _GAMECONSOLE

	// get the current directory
	char currentDir[MAX_PATH * 4];
	char dir[MAX_PATH * 4];
	char filterList[MAX_FILTER_LENGTH+1];
	GetCurrentDirectory(currentDir, sizeof(currentDir));

	FileFindHandle_t findHandle;

	KeyValues *combokv = m_pFileTypeCombo->GetActiveItemUserData();
	if (combokv)
	{
		Q_strncpy(filterList, combokv->GetString("filter", "*"), MAX_FILTER_LENGTH);
	}
	else
	{
		// add wildcard for search
		Q_strncpy(filterList, "*\0", MAX_FILTER_LENGTH);
	}

	char *filterPtr = filterList;
	//KeyValues *kv = new KeyValues("item");
	
	if ( m_DialogType != FOD_SELECT_DIRECTORY )
	{
		while ((filterPtr != NULL) && (*filterPtr != 0))
		{
			// parse the next filter in the list.
			char curFilter[MAX_FILTER_LENGTH];
			curFilter[0] = 0;
			int i = 0;
			while ((filterPtr != NULL) && ((*filterPtr == ',') || (*filterPtr == ';') || (*filterPtr <= ' ')))
			{
				++filterPtr;
			}
			while ((filterPtr != NULL) && (*filterPtr != ',') && (*filterPtr != ';') && (*filterPtr > ' '))
			{
				curFilter[i++] = *(filterPtr++);
			}
			curFilter[i] = 0;
			
			if (curFilter[0] == 0)
			{
				break;
			}
			
			Q_snprintf( dir, MAX_PATH*4, "%s%s", currentDir, curFilter );
			
			// Open the directory and walk it, loading files
			const char *pszFileName = g_pFullFileSystem->FindFirst( dir, &findHandle );
			while ( pszFileName )
			{
				if ( !g_pFullFileSystem->FindIsDirectory( findHandle ) )
				{
					FileData_t &fd = m_Files[ m_Files.AddToTail() ];
					InitFileData( false, pszFileName, currentDir, fd );
				}
				
				pszFileName = g_pFullFileSystem->FindNext( findHandle );
			}
			g_pFullFileSystem->FindClose( findHandle );
		}
	}

	
	// find all the directories
	GetCurrentDirectory(currentDir, sizeof(currentDir));
	Q_snprintf( dir, MAX_PATH*4, "%s*", currentDir );
	
	const char *pszFileName = g_pFullFileSystem->FindFirst( dir, &findHandle );
	while ( pszFileName )
	{
		if ( pszFileName[0] != '.' && g_pFullFileSystem->FindIsDirectory( findHandle ) )
		{
			FileData_t &fd = m_Files[ m_Files.AddToTail() ];
			InitFileData( true, pszFileName, currentDir, fd );
		}
		
		pszFileName = g_pFullFileSystem->FindNext( findHandle );
	}
	g_pFullFileSystem->FindClose( findHandle );
#endif
}

// Static method to do wildcard matching for *, ? and . characters
bool FileOpenDialog::FileNameWildCardMatch( char const *string, char const *pattern )
{
	for (;; ++string)
	{
		char stringc=toupper(*string);
		char patternc=toupper(*pattern++);
		switch (patternc)
		{
		case 0:
			return(stringc==0);
		case '?':
			if (stringc == 0)
				return(false);
			break;
		case '*':
			if (*pattern==0)
				return(true);
			if (*pattern=='.')
			{
				if (pattern[1]=='*' && pattern[2]==0)
					return(true);
				const char *dot=strchr(string,'.');
				if (pattern[1]==0)
					return (dot==NULL || dot[1]==0);
				if (dot!=NULL)
				{
					string=dot;
					if (strpbrk(pattern,"*?")==NULL && strchr(string+1,'.')==NULL)
						return(Q_stricmp(pattern+1,string+1)==0);
				}
			}

			while (*string)
				if (FileNameWildCardMatch(string++, pattern))
					return(true);
			return(false);
		default:
			if (patternc != stringc)
			{
				if (patternc=='.' && stringc==0)
					return(FileNameWildCardMatch(string, pattern ));
				else
					return(false);
			}
			break;
		}
	}
}

bool FileOpenDialog::PassesFilter( FileData_t *fd )
{
	// Do the substring filtering
	if ( fd->m_bDirectory )
		return true;

	// Never filter Save... dialogs
	if ( m_DialogType == FOD_SAVE )
		return true;

	if ( m_CurrentSubstringFilter.Length() <= 0 )
		return true;

	if ( Q_stristr( fd->m_FileName, m_CurrentSubstringFilter.String() ) )
		return true;

	if ( FileNameWildCardMatch( fd->m_FileName, m_CurrentSubstringFilter.String() ) )
		return true;

	return false;
}

void FileOpenDialog::FilterFileList()
{
	m_Filtered.RemoveAll();
	for ( int i = 0; i < m_Files.Count(); ++i )
	{
		// Apply filter
		FileData_t *pFD = &m_Files[ i ];
		if ( PassesFilter( pFD ) )
		{
			m_Filtered.AddToTail( &m_Files[ i ] );
		}
	}

	// clear the current list
	m_pFileList->DeleteAllItems();

	KeyValues *kv = new KeyValues("item");

	for ( int i = 0; i < m_Filtered.Count(); ++i )
	{
		FileData_t *fd = m_Filtered[ i ];
		fd->PrepareKV( kv );
		m_pFileList->AddItem(kv, 0, false, false);
	}

	kv->deleteThis();

	m_pFileList->SortList();
}

int FileOpenDialog::CountSubstringMatches()
{
	int nMatches = 0;

	for ( int i = 0; i < m_Files.Count(); ++i )
	{
		// Apply filter
		FileData_t *pFD = &m_Files[ i ];
		if ( PassesFilter( pFD ) )
		{
			if ( !pFD->m_bDirectory )
			{
				++nMatches;
			}
		}
	}
	return nMatches;
}

//-----------------------------------------------------------------------------
// Purpose: Fill the filelist with the names of all the files in the current directory
//-----------------------------------------------------------------------------
void FileOpenDialog::PopulateFileList()
{
	BuildFileList();
	FilterFileList();
}


//-----------------------------------------------------------------------------
// Does the specified extension match something in the filter list?
//-----------------------------------------------------------------------------
bool FileOpenDialog::ExtensionMatchesFilter( const char *pExt )
{
	KeyValues *combokv = m_pFileTypeCombo->GetActiveItemUserData();
	if ( !combokv )
		return true;

	char filterList[MAX_FILTER_LENGTH+1];
	Q_strncpy( filterList, combokv->GetString("filter", "*"), MAX_FILTER_LENGTH );

	char *filterPtr = filterList;
	while ((filterPtr != NULL) && (*filterPtr != 0))
	{
		// parse the next filter in the list.
		char curFilter[MAX_FILTER_LENGTH];
		curFilter[0] = 0;
		int i = 0;
		while ((filterPtr != NULL) && ((*filterPtr == ',') || (*filterPtr == ';') || (*filterPtr <= ' ')))
		{
			++filterPtr;
		}
		while ((filterPtr != NULL) && (*filterPtr != ',') && (*filterPtr != ';') && (*filterPtr > ' '))
		{
			curFilter[i++] = *(filterPtr++);
		}
		curFilter[i] = 0;

		if (curFilter[0] == 0)
			break;

		if ( !Q_stricmp( curFilter, "*" ) || !Q_stricmp( curFilter, "*.*" ) )
			return true;

		// FIXME: This isn't exactly right, but tough cookies;
		// it assumes the first two characters of the filter are *.
		Assert( curFilter[0] == '*' && curFilter[1] == '.' );
		if ( !Q_stricmp( &curFilter[2], pExt ) )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Choose the first non *.* filter in the filter list
//-----------------------------------------------------------------------------
void FileOpenDialog::ChooseExtension( char *pExt, int nBufLen )
{
	pExt[0] = 0;

	KeyValues *combokv = m_pFileTypeCombo->GetActiveItemUserData();
	if ( !combokv )
		return;

	char filterList[MAX_FILTER_LENGTH+1];
	Q_strncpy( filterList, combokv->GetString("filter", "*"), MAX_FILTER_LENGTH );

	char *filterPtr = filterList;
	while ((filterPtr != NULL) && (*filterPtr != 0))
	{
		// parse the next filter in the list.
		char curFilter[MAX_FILTER_LENGTH];
		curFilter[0] = 0;
		int i = 0;
		while ((filterPtr != NULL) && ((*filterPtr == ',') || (*filterPtr == ';') || (*filterPtr <= ' ')))
		{
			++filterPtr;
		}
		while ((filterPtr != NULL) && (*filterPtr != ',') && (*filterPtr != ';') && (*filterPtr > ' '))
		{
			curFilter[i++] = *(filterPtr++);
		}
		curFilter[i] = 0;

		if (curFilter[0] == 0)
			break;

		if ( !Q_stricmp( curFilter, "*" ) || !Q_stricmp( curFilter, "*.*" ) )
			continue;

		// FIXME: This isn't exactly right, but tough cookies;
		// it assumes the first two characters of the filter are *.
		Assert( curFilter[0] == '*' && curFilter[1] == '.' );
		Q_strncpy( pExt, &curFilter[1], nBufLen );
		break;
	}
}


//-----------------------------------------------------------------------------
// Saves the file to the start dir context
//-----------------------------------------------------------------------------
void FileOpenDialog::SaveFileToStartDirContext( const char *pFullPath )
{
	if ( m_nStartDirContext == s_StartDirContexts.InvalidIndex() )
		return;

	char pPath[MAX_PATH];
	pPath[0] = 0;
	Q_ExtractFilePath( pFullPath, pPath, sizeof(pPath) );
	s_StartDirContexts[ m_nStartDirContext ] = pPath;
}


//-----------------------------------------------------------------------------
// Posts a file selected message
//-----------------------------------------------------------------------------
void FileOpenDialog::PostFileSelectedMessage( const char *pFileName )
{
	m_bFileSelected = true;

	// open the file!
	KeyValues *pKeyValues = new KeyValues( "FileSelected", "fullpath", pFileName );
	KeyValues *pFilterKeys = m_pFileTypeCombo->GetActiveItemUserData();
	const char *pFilterInfo = pFilterKeys ? pFilterKeys->GetString( "filterinfo", NULL ) : NULL;
	if ( pFilterInfo )
	{
		pKeyValues->SetString( "filterinfo", pFilterInfo );
	}
	if ( m_pContextKeyValues )
	{
		pKeyValues->AddSubKey( m_pContextKeyValues );
		m_pContextKeyValues = NULL;
	}
	PostActionSignal( pKeyValues );
	CloseModal();
}


//-----------------------------------------------------------------------------
// Selects the current folder
//-----------------------------------------------------------------------------
void FileOpenDialog::OnSelectFolder()
{
	ValidatePath();

	// construct a file path
	char pFileName[MAX_PATH];
	GetSelectedFileName( pFileName, sizeof( pFileName ) );

	Q_StripTrailingSlash( pFileName );

	if ( !stricmp(pFileName, "..") )
	{
		MoveUpFolder();

		// clear the name text
		m_pFileNameCombo->SetText("");
		return;
	}

	if ( !stricmp(pFileName, ".") )
	{
		// clear the name text
		m_pFileNameCombo->SetText("");
		return;
	}

	// Compute the full path
	char pFullPath[MAX_PATH * 4];
	if ( !Q_IsAbsolutePath( pFileName ) )
	{
		GetCurrentDirectory(pFullPath, sizeof(pFullPath) - MAX_PATH);
		strcat( pFullPath, pFileName );
		if ( !pFileName[0] )
		{
			Q_StripTrailingSlash( pFullPath );
		}
	}
	else
	{
		Q_strncpy( pFullPath, pFileName, sizeof(pFullPath) );
	}

	if ( g_pFullFileSystem->FileExists( pFullPath ) )
	{
		// open the file!
		SaveFileToStartDirContext( pFullPath );
		PostFileSelectedMessage( pFullPath );
		return;
	}

	PopulateDriveList();
	PopulateFileList();
	InvalidateLayout();
}


//-----------------------------------------------------------------------------
// Purpose: Handle the open button being pressed
//			checks on what has changed and acts accordingly
//-----------------------------------------------------------------------------
void FileOpenDialog::OnOpen()
{
	ValidatePath();

	// construct a file path
	char pFileName[MAX_PATH];
	GetSelectedFileName( pFileName, sizeof( pFileName ) );

	int nLen = Q_strlen( pFileName );
	bool bSpecifiedDirectory = ( pFileName[nLen-1] == '/' || pFileName[nLen-1] == '\\' );
	Q_StripTrailingSlash( pFileName );

	if ( !stricmp(pFileName, "..") )
	{
		MoveUpFolder();
		
		// clear the name text
		m_pFileNameCombo->SetText("");
		return;
	}

	if ( !stricmp(pFileName, ".") )
	{
		// clear the name text
		m_pFileNameCombo->SetText("");
		return;
	}
	 
	// Compute the full path
	char pFullPath[MAX_PATH * 4];
	if ( !Q_IsAbsolutePath( pFileName ) )
	{
		GetCurrentDirectory(pFullPath, sizeof(pFullPath) - MAX_PATH);
		strcat(pFullPath, pFileName);
		if ( !pFileName[0] )
		{
			Q_StripTrailingSlash( pFullPath );
		}
	}
	else
	{
		Q_strncpy( pFullPath, pFileName, sizeof(pFullPath) );
	}

	// If the name specified is a directory, then change directory
	if ( g_pFullFileSystem->IsDirectory( pFullPath, NULL ) )
	{
		// it's a directory; change to the specified directory
		if ( !bSpecifiedDirectory )
		{
			strcat( pFullPath , CORRECT_PATH_SEPARATOR_S );
		}
		SetStartDirectory( pFullPath );

		// clear the name text
		m_pFileNameCombo->SetText("");
		m_pFileNameCombo->HideMenu();
		m_CurrentSubstringFilter = "";

		PopulateDriveList();
		PopulateFileList();
		InvalidateLayout();
		return;
	}
	else if ( bSpecifiedDirectory )
	{
		PopulateDriveList();
		PopulateFileList();
		InvalidateLayout();
		return;
	}

	m_CurrentSubstringFilter = pFileName;
	AddSearchHistoryString( pFileName );

	if ( m_DialogType != FOD_SAVE )
	{
		if ( m_CurrentSubstringFilter.Length() > 0 )
		{
			// It's ambiguous
			int nMatches = CountSubstringMatches();
			if ( nMatches >= 2 )
			{
				// Apply filter instead
				FilterFileList();
				return;
			}
		}
	}
	
	// Append suffix of the first filter that isn't *.*
	char extension[512];
	Q_ExtractFileExtension( pFullPath, extension, sizeof(extension) );
	if ( !ExtensionMatchesFilter( extension ) )
	{
		ChooseExtension( extension, sizeof(extension) );
		Q_SetExtension( pFullPath, extension, sizeof(pFullPath) );
	}

	if ( g_pFullFileSystem->FileExists( pFullPath ) )
	{
		// open the file!
		SaveFileToStartDirContext( pFullPath );
		PostFileSelectedMessage( pFullPath );
		return;
	}

	// file not found
	if ( ( m_DialogType == FOD_SAVE ) && pFileName[0] )
	{
		// open the file!
		SaveFileToStartDirContext( pFullPath );
		PostFileSelectedMessage( pFullPath );
		return;
	}

	PopulateDriveList();
	PopulateFileList();
	InvalidateLayout();
}


//-----------------------------------------------------------------------------
// Purpose: using the file edit box as a prefix, create a menu of all possible files 
//-----------------------------------------------------------------------------
void FileOpenDialog::PopulateFileNameSearchHistory()
{
	m_pFileNameCombo->RemoveAll();
	for ( int i = 0; i < m_SearchHistory.Count(); ++i )
	{
		m_pFileNameCombo->AddItem( m_SearchHistory[ i ].String(), 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle an item in the list being selected
//-----------------------------------------------------------------------------
void FileOpenDialog::OnItemSelected()
{
	// make sure only one item is selected
	if (m_pFileList->GetSelectedItemsCount() != 1)
	{
		m_pFileNameCombo->SetText("");
	}
	else
	{
		// put the file name into the text edit box
		KeyValues *data = m_pFileList->GetItem(m_pFileList->GetSelectedItem(0));
		m_pFileNameCombo->SetText(data->GetString("text"));
	}

	InvalidateLayout();
}

void FileOpenDialog::OnMatchStringSelected()
{
	char pFileName[MAX_PATH];
	GetSelectedFileName( pFileName, sizeof( pFileName ) );
	m_pFileNameCombo->GetText( pFileName, sizeof( pFileName ) );

	m_CurrentSubstringFilter = pFileName;

	// Redo filter
	FilterFileList();
}

//-----------------------------------------------------------------------------
// Purpose: Handle an item in the Drive combo box being selected
//-----------------------------------------------------------------------------
void FileOpenDialog::OnTextChanged(KeyValues *kv)
{
	Panel *pPanel = (Panel *) kv->GetPtr("panel", NULL);

	// first check which control had its text changed!
	if (pPanel == m_pFullPathEdit)
	{
		m_pFileNameCombo->HideMenu();
		m_pFileNameCombo->SetText("");
		OnOpen();
	}
	else if (pPanel == m_pFileNameCombo)
	{
		// Don't react to text being typed
	}
	else if (pPanel == m_pFileTypeCombo)
	{
		m_pFileNameCombo->HideMenu();
		PopulateFileList();
	}
}
