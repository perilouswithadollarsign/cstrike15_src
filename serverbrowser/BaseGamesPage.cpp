//========= Copyright 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"
#include "vstdlib/vstrtools.h"

// engine interface
#include "cdll_int.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif


using namespace vgui;

#define FILTER_ALLSERVERS			0
#define FILTER_SECURESERVERSONLY	1
#define FILTER_INSECURESERVERSONLY	2

#define FILTER_ALLMAPS				0
#define FILTER_SUBSCRIBEDMAPSONLY	1
#define FILTER_FEATUREDMAPSONLY		2

#define UNIVERSE_OFFICIAL			0
#define UNIVERSE_CUSTOMGAMES		1
#define GAMETYPES_FILE				"scripts/serverbrowsergametypes.txt"
#define QUICKLIST_FILTER_MIN_PING	0

// #define MAX_MAP_NAME	32

CUtlVector< gametypes_t > g_GameTypes;
void LoadGameTypes( void );

#undef wcscat


extern IVEngineClient *engine;

extern uint64 GetMapIDFromMapPath( const char *pMapPath );

//--------------------------------------------------------------------------------------------------------
inline char *CloneString( const char *str )
{
	char *cloneStr = new char [ strlen(str)+1 ];
	strcpy( cloneStr, str );
	return cloneStr;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGameListPanel::CGameListPanel( CBaseGamesPage *pOuter, const char *pName ) :
	BaseClass( pOuter, pName )
{
	m_pOuter = pOuter;
}

//-----------------------------------------------------------------------------
// Purpose: Forward KEY_ENTER to the CBaseGamesPage.
//-----------------------------------------------------------------------------
void CGameListPanel::OnKeyCodeTyped(vgui::KeyCode code)
{
	// Let the outer class handle it.
	if ( code == KEY_ENTER && m_pOuter->OnGameListEnterPressed() )
		return;
	
	BaseClass::OnKeyCodeTyped( code );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBaseGamesPage::CBaseGamesPage( vgui::Panel *parent, const char *name, EPageType eType, const char *pCustomResFilename) 
	: PropertyPage(parent, name), m_pCustomResFilename( pCustomResFilename ),
	m_CallbackFavoritesMsg( this, &CBaseGamesPage::OnFavoritesMsg ),
	m_hRequest( NULL )
{
	SetSize( 624, 278 );
	m_szGameFilter[0] = 0;
	m_szMapFilter[0] = 0;
	m_szComboAllText[0] = 0;
	m_iPingFilter = 0;
	m_iServerRefreshCount = 0;
	m_bFilterNoFullServers = false;
	m_bFilterNoEmptyServers = false;
	m_bFilterNoPasswordedServers = false;
	m_iSecureFilter = FILTER_ALLSERVERS;
	m_hFont = NULL;
	m_eMatchMakingType = eType;
	SetDefLessFunc( m_mapServers );
	SetDefLessFunc( m_mapServerIP );
	SetDefLessFunc( m_mapGamesFilterItem );
	
	// get the 'all' text
	wchar_t *all = g_pVGuiLocalize->Find("ServerBrowser_All");
	if ( all )
	{
		V_UnicodeToUTF8(all, m_szComboAllText, sizeof(m_szComboAllText));
	}

	// Init UI
	m_pConnect = new Button(this, "ConnectButton", "#ServerBrowser_Connect");
	m_pConnect->SetEnabled(false);
	m_pRefreshAll = new Button(this, "RefreshButton", "#ServerBrowser_Refresh");
	m_pRefreshQuick = new Button(this, "RefreshQuickButton", "#ServerBrowser_RefreshQuick");
	m_pAddServer = new Button(this, "AddServerButton", "#ServerBrowser_AddServer");
	m_pAddCurrentServer = new Button(this, "AddCurrentServerButton", "#ServerBrowser_AddCurrentServer");
	m_pGameList = new CGameListPanel(this, "gamelist");
	m_pGameList->SetAllowUserModificationOfColumns(true);

	m_pQuickList = new PanelListPanel(this, "quicklist");
	m_pQuickList->SetFirstColumnWidth( 0 );
		
	m_pAddToFavoritesButton = new vgui::Button( this, "AddToFavoritesButton", "" );
	m_pAddToFavoritesButton->SetEnabled( false );
	m_pAddToFavoritesButton->SetVisible( false );

	// Add the column headers
	m_pGameList->AddColumnHeader(0, "Password", "#ServerBrowser_Password", 16, ListPanel::COLUMN_FIXEDSIZE | ListPanel::COLUMN_IMAGE);
	m_pGameList->AddColumnHeader(1, "Bots", "#ServerBrowser_Bots", 16, ListPanel::COLUMN_FIXEDSIZE | ListPanel::COLUMN_HIDDEN);
	m_pGameList->AddColumnHeader(2, "Secure", "#ServerBrowser_Secure", 16, ListPanel::COLUMN_FIXEDSIZE | ListPanel::COLUMN_IMAGE);
	m_pGameList->AddColumnHeader(3, "Mode", "#ServerBrowser_Mode", 16, ListPanel::COLUMN_FIXEDSIZE | ListPanel::COLUMN_IMAGE);
	m_pGameList->AddColumnHeader(4, "Name", "#ServerBrowser_Servers", 50, ListPanel::COLUMN_RESIZEWITHWINDOW | ListPanel::COLUMN_UNHIDABLE);
	m_pGameList->AddColumnHeader(5, "IPAddr", "#ServerBrowser_IPAddress", 64, ListPanel::COLUMN_HIDDEN);
	m_pGameList->AddColumnHeader(6, "Players", "#ServerBrowser_Players", 55, ListPanel::COLUMN_FIXEDSIZE);
	m_pGameList->AddColumnHeader(7, "Workshop", "#ServerBrowser_Workshop", 16, ListPanel::COLUMN_FIXEDSIZE | ListPanel::COLUMN_IMAGE);
	m_pGameList->AddColumnHeader(8, "Map", "#ServerBrowser_Map", 90, 
		90,		// minwidth
		300,	// maxwidth
		0		// flags
		);
	//m_pGameList->AddColumnHeader(7, "WorkshopId", "#ServerBrowser_WorkshopId", 75, ListPanel::COLUMN_FIXEDSIZE );
	m_pGameList->AddColumnHeader(9, "Ping", "#ServerBrowser_Latency", 55, ListPanel::COLUMN_FIXEDSIZE);

	m_pGameList->SetColumnHeaderTooltip(0, "#ServerBrowser_PasswordColumn_Tooltip" );
	m_pGameList->SetColumnHeaderTooltip(1, "#ServerBrowser_BotColumn_Tooltip" );
	m_pGameList->SetColumnHeaderTooltip(2, "#ServerBrowser_SecureColumn_Tooltip" );
	m_pGameList->SetColumnHeaderTooltip(3, "#ServerBrowser_ModeColumn_Tooltip" );
	m_pGameList->SetColumnHeaderTooltip(7, "#ServerBrowser_WorkshopColumn_Tooltip" );

	// setup fast sort functions
	m_pGameList->SetSortFunc(0, PasswordCompare);
	m_pGameList->SetSortFunc(1, BotsCompare);
	m_pGameList->SetSortFunc(2, SecureCompare);
	m_pGameList->SetSortFunc(3, ModeCompare);
	m_pGameList->SetSortFunc(4, ServerNameCompare);
	m_pGameList->SetSortFunc(5, IPAddressCompare);
	m_pGameList->SetSortFunc(6, PlayersCompare);
	m_pGameList->SetSortFunc(7, WorkshopCompare);
	m_pGameList->SetSortFunc(8, MapCompare);
	m_pGameList->SetSortFunc(9, PingCompare);

	// Sort by ping time by default
	m_pGameList->SetSortColumn(9);

 	CreateFilters();
	LoadFilterSettings();
	LoadGameTypes();

	m_bAutoSelectFirstItemInGameList = false;

	m_iWorkshopIconIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CBaseGamesPage::~CBaseGamesPage()
{
	if ( m_hRequest )
	{
		steamapicontext->SteamMatchmakingServers()->ReleaseRequest( m_hRequest );
		m_hRequest = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void LoadGameTypes( void )
{
	if ( g_GameTypes.Count() > 0 )
		return;

	KeyValues * kv = new KeyValues( GAMETYPES_FILE );

	if  ( !kv->LoadFromFile( g_pFullFileSystem, GAMETYPES_FILE, "MOD" ) )
	{
		kv->deleteThis();
		return;
	}

	g_GameTypes.RemoveAll();

	for ( KeyValues *pData = kv->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
	{
		gametypes_t& gametype = g_GameTypes[ g_GameTypes.AddToTail() ];

		gametype.m_szPrefix = CloneString( pData->GetString( "prefix", "" ) );
		gametype.m_szGametypeName = CloneString( pData->GetString( "name", "" ) );
		gametype.m_szGametypeIcon = CloneString( pData->GetString( "icon", "" ) );
		gametype.m_iIconImageIndex = 0; // This gets set later when we register icons with vgui
	}
	

	kv->deleteThis();
}

const char *GetGameTypeFriendlyName( const char *pMapName )
{
	for ( int i = 0; i < g_GameTypes.Count(); i++ )
	{
		int iLength = strlen( g_GameTypes[i].m_szPrefix );

		if ( !Q_strncmp( pMapName, g_GameTypes[i].m_szPrefix, iLength ) )
		{
			return pMapName + iLength;
		}
	}

	return pMapName;
}

const char *GetGameTypeName( const char *pMapName )
{
	for ( int i = 0; i < g_GameTypes.Count(); i++ )
	{
		int iLength = strlen( g_GameTypes[i].m_szPrefix );

		if ( !Q_strncmp( pMapName, g_GameTypes[i].m_szPrefix, iLength ) )
		{
			return g_GameTypes[i].m_szGametypeName;
		}
	}

	return "";
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseGamesPage::GetInvalidServerListID()
{
	return m_pGameList->InvalidItemID();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseGamesPage::PerformLayout()
{
	BaseClass::PerformLayout();
	
	if ( GetSelectedServerID() == -1 )
	{
		m_pConnect->SetEnabled(false);
	}
	else
	{
		m_pConnect->SetEnabled(true);
	}

	if (SupportsItem(IGameList::GETNEWLIST))
	{
		m_pRefreshQuick->SetVisible(true);
		m_pRefreshAll->SetText("#ServerBrowser_RefreshAll");
	}
	else
	{
		m_pRefreshQuick->SetVisible(false);
		m_pRefreshAll->SetText("#ServerBrowser_Refresh");
	}

	if ( SupportsItem(IGameList::ADDSERVER) )
	{
// 		m_pFilterString->SetWide( 90 ); // shrink the filter label to fix the add current server button
		m_pAddServer->SetVisible(true);
	}
	else
	{
		m_pAddServer->SetVisible(false);
	}

	if ( SupportsItem(IGameList::ADDCURRENTSERVER) )
	{
		m_pAddCurrentServer->SetVisible(true);
	}
	else
	{
		m_pAddCurrentServer->SetVisible(false);
	}

	if ( IsRefreshing() )
	{
		m_pRefreshAll->SetText( "#ServerBrowser_StopRefreshingList" );
	}

	if (m_pGameList->GetItemCount() > 0)
	{
		m_pRefreshQuick->SetEnabled(true);
	}
	else
	{
		m_pRefreshQuick->SetEnabled(false);
	}

	if ( !steamapicontext->SteamMatchmakingServers() || !steamapicontext->SteamMatchmaking() )
	{
		m_pAddCurrentServer->SetVisible( false );
		m_pRefreshQuick->SetEnabled( false );
		m_pAddServer->SetEnabled( false );
		m_pConnect->SetEnabled( false );
		m_pRefreshAll->SetEnabled( false );
		m_pAddToFavoritesButton->SetEnabled( false );
		m_pGameList->SetEmptyListText( "#ServerBrowser_SteamRunning" );
	}

	Repaint();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseGamesPage::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	// load the password icon
	ImageList *imageList = new ImageList(false);
	imageList->AddImage(scheme()->GetImage("servers/icon_password", false));
	imageList->AddImage(scheme()->GetImage("servers/icon_bots", false));
	imageList->AddImage(scheme()->GetImage("servers/icon_robotron", false));
	imageList->AddImage(scheme()->GetImage("servers/icon_secure_deny", false));

	imageList->AddImage(scheme()->GetImage("servers/icon_secure_deny", false)); 

	// THE ABOVE INDICIES HAVE HARD CODED USES, ONLY ADD NEW IMAGES BELOW

	int passwordColumnImage = imageList->AddImage(scheme()->GetImage("servers/icon_password_column", false));
	int botColumnImage = imageList->AddImage(scheme()->GetImage("servers/icon_bots_column", false));
	int secureColumnImage = imageList->AddImage(scheme()->GetImage("servers/icon_robotron_column", false));
	int gamemodeColumnImage = imageList->AddImage(scheme()->GetImage("servers/icon_mod_header", false));
	int workshopColumnImage = imageList->AddImage(scheme()->GetImage("servers/icon_workshop_header", false));

	m_pGameList->SetImageList(imageList, true);
	m_hFont = pScheme->GetFont( "ListSmall", IsProportional() );
	if ( !m_hFont )
		m_hFont = pScheme->GetFont( "DefaultSmall", IsProportional() );

	m_pGameList->SetFont( m_hFont );
	m_pGameList->SetColumnHeaderImage( 0, passwordColumnImage );
	m_pGameList->SetColumnHeaderImage( 1, botColumnImage );
	m_pGameList->SetColumnHeaderImage( 2, secureColumnImage );
	m_pGameList->SetColumnHeaderImage( 3, gamemodeColumnImage );
	m_pGameList->SetColumnHeaderImage( 7, workshopColumnImage );

	m_iWorkshopIconIndex = imageList->AddImage( scheme()->GetImage( "servers/icon_workshop_column", false ) );

	// Load up all images from script file for different custom game modes
	FOR_EACH_VEC( g_GameTypes, i )
	{
		gametypes_t &type = g_GameTypes[ i ];
		if ( type.m_szGametypeIcon && type.m_szGametypeIcon[ 0 ] )
		{
			g_GameTypes[ i ].m_iIconImageIndex = imageList->AddImage( scheme()->GetImage( type.m_szGametypeIcon, false ) );
		}
	}

}

struct serverqualitysort_t
{
	int iIndex;
	int iPing;
	int iPlayerCount;
};

int ServerQualitySort( const serverqualitysort_t *pSQ1, const serverqualitysort_t *pSQ2 )
{
	if ( pSQ1->iPing <= 100 && pSQ2->iPing <= 100 && pSQ1->iPlayerCount != pSQ2->iPlayerCount  )
	{
		return pSQ2->iPlayerCount - pSQ1->iPlayerCount;
	}

	return pSQ1->iPing - pSQ2->iPing;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseGamesPage::SelectQuickListServers( void )
{
	int iIndex = m_pQuickList->FirstItem();
	
	while ( iIndex != m_pQuickList->InvalidItemID() )
	{
		CQuickListPanel *pQuickListPanel = dynamic_cast< CQuickListPanel *> ( m_pQuickList->GetItemPanel( iIndex ) );
		
		if ( pQuickListPanel )
		{
			CUtlVector< serverqualitysort_t > vecServerQuality;

			int iElement = m_quicklistserverlist.Find( pQuickListPanel->GetName() );

			if ( iElement != m_quicklistserverlist.InvalidIndex() )
			{
				CQuickListMapServerList *vecMapServers = &m_quicklistserverlist[iElement];

				if ( vecMapServers )
				{
					for ( int i =0; i < vecMapServers->Count(); i++ )
					{
						int iListID = vecMapServers->Element( i );

						serverqualitysort_t serverquality;

						serverquality.iIndex = iListID;

						KeyValues *kv = NULL;
						if ( m_pGameList->IsValidItemID( iListID ) )
						{
							kv = m_pGameList->GetItem( iListID );
						}

						if ( kv )
						{
							serverquality.iPing = kv->GetInt( "ping", 0 );
							serverquality.iPlayerCount = kv->GetInt( "PlayerCount", 0 );
						}

						vecServerQuality.AddToTail( serverquality );
					}

					vecServerQuality.Sort( ServerQualitySort );

					serverqualitysort_t bestserver = vecServerQuality.Head();

					if ( m_pGameList->IsValidItemID( bestserver.iIndex ) )
					{
						pQuickListPanel->SetServerInfo( m_pGameList->GetItem( bestserver.iIndex ), bestserver.iIndex );
					}
				}
			}
		}

		iIndex = m_pQuickList->NextItem( iIndex );
	}

	//Force the connect button to recalculate its state.
	OnItemSelected();
}

int ServerMapnameSortFunc( const servermaps_t *p1,  const servermaps_t *p2 )
{
	//If they're both on disc OR both missing then sort them alphabetically
	if ( (p1->bOnDisk && p2->bOnDisk) || (!p1->bOnDisk && !p2->bOnDisk ) )
		return Q_strcmp( p1->pFriendlyName, p2->pFriendlyName );

	//Otherwise maps you have show up first
	return p2->bOnDisk - p1->bOnDisk;
}

//-----------------------------------------------------------------------------
// Purpose: prepares all the QuickListPanel map panels...
//-----------------------------------------------------------------------------
void CBaseGamesPage::PrepareQuickListMap( const char *pMapName, int iListID )
{
	char szMapName[ 512 ];
	Q_snprintf( szMapName, sizeof( szMapName ), "%s",  pMapName );

	Q_strlower( szMapName );

	char path[ 512 ];
	Q_snprintf( path, sizeof( path ), "maps/%s.bsp", szMapName );
	
	int iIndex = m_quicklistserverlist.Find( szMapName );

	if ( m_quicklistserverlist.IsValidIndex( iIndex ) == false )
	{
		CQuickListMapServerList vecMapServers;
		iIndex = m_quicklistserverlist.Insert( szMapName, vecMapServers );
		
		char szFriendlyName[MAX_MAP_NAME];
		Q_strncpy( szFriendlyName, GetGameTypeFriendlyName( szMapName ), sizeof( szFriendlyName ) );

		char *pszFinal = Q_strstr( szFriendlyName, "_final" );
		if ( pszFinal )
		{
			// truncate the _final (or _final1) part of the filename if it's at the end of the name
			char *pszNextChar = pszFinal + Q_strlen( "_final" );
			if ( pszNextChar )
			{
				if ( ( *pszNextChar == '\0' ) ||
					( ( *pszNextChar == '1' ) && ( *(pszNextChar+1) == '\0' ) ) )
				{
					*pszFinal = '\0';
				}
			}
		}

		//Add the map to our list of panels.
		if ( m_pQuickList )
		{
			servermaps_t servermap;

			servermap.pFriendlyName = CloneString( szFriendlyName );
			servermap.pOriginalName = CloneString( szMapName );

			char path[ 512 ];
			Q_snprintf( path, sizeof( path ), "maps/%s.bsp", szMapName );

			servermap.bOnDisk = g_pFullFileSystem->FileExists( path, "MOD" );

			CQuickListPanel *pQuickListPanel = new CQuickListPanel( m_pQuickList, "QuickListPanel");

			if ( pQuickListPanel ) 
			{
				pQuickListPanel->InvalidateLayout();
				pQuickListPanel->SetName( servermap.pOriginalName );
				pQuickListPanel->SetMapName( servermap.pFriendlyName );
				pQuickListPanel->SetImage( servermap.pOriginalName );
				pQuickListPanel->SetGameType( GetGameTypeName( servermap.pOriginalName ) );
				pQuickListPanel->SetVisible( true );
				pQuickListPanel->SetRefreshing();

				servermap.iPanelIndex = m_pQuickList->AddItem( NULL,  pQuickListPanel );
			}

			m_vecMapNamesFound.AddToTail( servermap );
			m_vecMapNamesFound.Sort( ServerMapnameSortFunc );
		}

		//Now make sure that list is sorted.
		// jms: TODO
		CUtlVector<int> *pPanelSort = NULL; //m_pQuickList->GetSortedVector();

		if ( pPanelSort )
		{
			pPanelSort->RemoveAll();

			for ( int i = 0; i < m_vecMapNamesFound.Count(); i++ )
			{
				pPanelSort->AddToTail( m_vecMapNamesFound[i].iPanelIndex );
			}
		}
	}

	if ( iIndex != m_quicklistserverlist.InvalidIndex() )
	{
		CQuickListMapServerList *vecMapServers = &m_quicklistserverlist[iIndex];

		if ( vecMapServers )
		{
			if ( vecMapServers->Find( iListID ) == vecMapServers->InvalidIndex() )
			{
				 vecMapServers->AddToTail( iListID );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: gets information about specified server
//-----------------------------------------------------------------------------
gameserveritem_t *CBaseGamesPage::GetServer( unsigned int serverID )
{
	if ( !steamapicontext->SteamMatchmakingServers() )
		return NULL;

	if ( serverID >= 0 )
	{
		return steamapicontext->SteamMatchmakingServers()->GetServerDetails( m_hRequest, serverID );
	}
	else
	{
		Assert( !"Unable to return a useful entry" );
		return NULL; // bugbug Alfred: temp Favorites/History objects won't return a good value here...
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CBaseGamesPage::TagsExclude( void )
{
	if ( m_pTagsIncludeFilter == NULL )
		return false;

	return m_pTagsIncludeFilter->GetActiveItem() != NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseGamesPage::CreateFilters()
{
	m_pFilter = new ToggleButton(this, "Filter", "#ServerBrowser_Filter");
	m_pFilterString = new Label(this, "FilterString", "");

	m_pFilter->SetSelected( false );
	m_bFiltersVisible = false;

	// filter controls
	m_pGameFilter = new ComboBox(this, "GameFilter", 6, false);

	m_pLocationFilter = new ComboBox(this, "LocationFilter", 6, false);
	m_pLocationFilter->AddItem("", NULL);

	m_pMapFilter = new TextEntry(this, "MapFilter");

	m_pWorkshopFilter = new ComboBox(this, "WorkshopFilter", 3, false);
	m_pWorkshopFilter->AddItem("#ServerBrowser_All", NULL);
	m_pWorkshopFilter->AddItem("#ServerBrowser_SubscribedOnly", NULL);
	m_pWorkshopFilter->AddItem("#ServerBrowser_FeaturedOnly", NULL);

	m_pPingFilter = new ComboBox(this, "PingFilter", 6, false);
	m_pPingFilter->AddItem("#ServerBrowser_All", NULL);
	m_pPingFilter->AddItem("#ServerBrowser_LessThan50", NULL);
	m_pPingFilter->AddItem("#ServerBrowser_LessThan100", NULL);
	m_pPingFilter->AddItem("#ServerBrowser_LessThan150", NULL);
	m_pPingFilter->AddItem("#ServerBrowser_LessThan250", NULL);
	m_pPingFilter->AddItem("#ServerBrowser_LessThan350", NULL);
	m_pPingFilter->AddItem("#ServerBrowser_LessThan600", NULL);

	m_pSecureFilter = new ComboBox(this, "SecureFilter", 3, false);
	m_pSecureFilter->AddItem("#ServerBrowser_All", NULL);
	m_pSecureFilter->AddItem("#ServerBrowser_SecureOnly", NULL);
	m_pSecureFilter->AddItem("#ServerBrowser_InsecureOnly", NULL);

	m_pTagsIncludeFilter = new ComboBox(this, "TagsInclude", 2, false);
	m_pTagsIncludeFilter->AddItem("#ServerBrowser_TagsInclude", NULL);
	m_pTagsIncludeFilter->AddItem("#ServerBrowser_TagsDoNotInclude", NULL);
	m_pTagsIncludeFilter->SetVisible( false );

	m_pNoEmptyServersFilterCheck = new CheckButton(this, "ServerEmptyFilterCheck", "");
	m_pNoFullServersFilterCheck = new CheckButton(this, "ServerFullFilterCheck", "");
	m_pNoPasswordFilterCheck = new CheckButton(this, "NoPasswordFilterCheck", "");
//	m_pQuickListCheckButton = new CheckButton(this, "QuickListCheck", "");
	
	KeyValues *pkv = new KeyValues("mod", "gamedir", "", "appid", NULL );
	m_pGameFilter->AddItem("#ServerBrowser_All", pkv);

	for (int i = 0; i < ModList().ModCount(); i++)
	{
		pkv->SetString("gamedir", ModList().GetModDir(i));
		pkv->SetUint64("appid", ModList().GetAppID(i).ToUint64() );
		int iItemID = m_pGameFilter->AddItem(ModList().GetModName(i), pkv);
		m_mapGamesFilterItem.Insert( ModList().GetAppID(i).ToUint64(), iItemID );
	}
	pkv->deleteThis();

}


//-----------------------------------------------------------------------------
// Purpose: loads filter settings from the keyvalues
//-----------------------------------------------------------------------------
void CBaseGamesPage::LoadFilterSettings()
{
	bool bUsingDefaults = true;

	KeyValues *filter = ServerBrowserDialog().GetFilterSaveData(GetName());

	if (ServerBrowserDialog().GetActiveModName())
	{
		Q_strncpy(m_szGameFilter, ServerBrowserDialog().GetActiveModName(), sizeof(m_szGameFilter));
		m_iLimitToAppID = ServerBrowserDialog().GetActiveAppID();
	}
	else
	{
		Q_strncpy(m_szGameFilter, filter->GetString("game"), sizeof(m_szGameFilter));
		m_iLimitToAppID = CGameID( filter->GetUint64( "appid", 0 ) );
	}

	Q_strncpy(m_szMapFilter, filter->GetString("map"), sizeof(m_szMapFilter));

	int nWorkshopFilter = filter->GetInt("Workshop");
	m_pWorkshopFilter->ActivateItem(nWorkshopFilter);

	m_iPingFilter = filter->GetInt("ping");
	m_bFilterNoFullServers = filter->GetBool("NoFull");
	m_bFilterNoEmptyServers = filter->GetBool("NoEmpty");
	m_bFilterNoPasswordedServers = filter->GetBool("NoPassword");
	
	int secureFilter = filter->GetInt("Secure");
	m_pSecureFilter->ActivateItem(secureFilter);

	int tagsinclude = filter->GetInt("tagsinclude");
	m_pTagsIncludeFilter->ActivateItem( tagsinclude );

	// apply to the controls
	UpdateGameFilter();
	m_pMapFilter->SetText(m_szMapFilter);

	int nLocation = filter->GetInt("location");
	m_pLocationFilter->ActivateItem( nLocation );

	if ( /*nWorkshopFilter != 0 || */
		 m_iPingFilter != 0 ||
		 m_bFilterNoFullServers || 
		 m_bFilterNoEmptyServers || 
		 m_bFilterNoPasswordedServers || 
		 secureFilter != 0 || 
		 tagsinclude != 0 || 
		 nLocation != 0 )
	{
		bUsingDefaults = false;
	}

	if (m_iPingFilter)
	{
		char buf[32];
		Q_snprintf(buf, sizeof(buf), "< %d", m_iPingFilter);
		m_pPingFilter->SetText(buf);
	}

	m_pNoFullServersFilterCheck->SetSelected(m_bFilterNoFullServers);
	m_pNoEmptyServersFilterCheck->SetSelected(m_bFilterNoEmptyServers);
	m_pNoPasswordFilterCheck->SetSelected(m_bFilterNoPasswordedServers);

	OnLoadFilter( filter );
	UpdateFilterSettings();

	UpdateFilterAndQuickListVisibility();

	if ( !bUsingDefaults )
	{
		m_pFilter->SetSelected( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sets the game filter combo box to be the saved setting
//-----------------------------------------------------------------------------
void CBaseGamesPage::UpdateGameFilter()
{
	bool bFound = false;
	for (int i = 0; i < m_pGameFilter->GetItemCount(); i++)
	{
		KeyValues *kv = m_pGameFilter->GetItemUserData(i);
		CGameID gameID( kv->GetUint64( "appID", 0 ) );
		const char *pchGameDir = kv->GetString( "gamedir" );
		if ( ( gameID == m_iLimitToAppID || m_iLimitToAppID.AppID() == 0 ) && ( !m_szGameFilter[0] || 
				( pchGameDir && pchGameDir[0] && !Q_strncmp( pchGameDir, m_szGameFilter, Q_strlen( pchGameDir ) ) ) ) )
		{
			if ( i != m_pGameFilter->GetActiveItem() )
			{
				m_pGameFilter->ActivateItem(i);
			}
			bFound = true;
			break;
		}
	}
	if (!bFound)
	{
		// default to empty
		if ( 0 != m_pGameFilter->GetActiveItem() )
		{
			m_pGameFilter->ActivateItem(0);
		}
	}

	// only one mod is allowed in the game
	if ( ServerBrowserDialog().GetActiveModName() )
	{
		m_pGameFilter->SetEnabled( false );
		m_pGameFilter->SetText( ServerBrowserDialog().GetActiveGameName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handles incoming server refresh data
//			updates the server browser with the refreshed information from the server itself
//-----------------------------------------------------------------------------
void CBaseGamesPage::ServerResponded( gameserveritem_t &server )
{
	int nIndex = -1; // start at -1 and work backwards to find the next free slot for this adhoc query
	while ( m_mapServers.Find( nIndex ) != m_mapServers.InvalidIndex() )
		nIndex--;
	ServerResponded( nIndex, &server );
}


//-----------------------------------------------------------------------------
// Purpose: Callback for ISteamMatchmakingServerListResponse
//-----------------------------------------------------------------------------
void CBaseGamesPage::ServerResponded( HServerListRequest hReq, int iServer )
{
	gameserveritem_t *pServerItem = steamapicontext->SteamMatchmakingServers()->GetServerDetails( hReq, iServer );
	if ( !pServerItem )
	{
		Assert( !"Missing server response" );
		return;
	}
	ServerResponded( iServer, pServerItem );
}


//-----------------------------------------------------------------------------
// Purpose: Handles incoming server refresh data
//			updates the server browser with the refreshed information from the server itself
//-----------------------------------------------------------------------------
void CBaseGamesPage::ServerResponded( int iServer, gameserveritem_t *pServerItem )
{
	int iServerMap = m_mapServers.Find( iServer );
	if ( iServerMap == m_mapServers.InvalidIndex() )
	{
		netadr_t netAdr( pServerItem->m_NetAdr.GetIP(), pServerItem->m_NetAdr.GetConnectionPort() );
		int iServerIP = m_mapServerIP.Find( netAdr );
		if ( iServerIP != m_mapServerIP.InvalidIndex() )
		{
			// if we already had this entry under another index remove the old entry
			int iServerMap = m_mapServers.Find( m_mapServerIP[ iServerIP ] );
			if ( iServerMap != m_mapServers.InvalidIndex() )
			{
				serverdisplay_t &server = m_mapServers[ iServerMap ];
				if ( m_pGameList->IsValidItemID( server.m_iListID ) )
					m_pGameList->RemoveItem( server.m_iListID );
				m_mapServers.RemoveAt( iServerMap );
			}
			m_mapServerIP.RemoveAt( iServerIP );
		}

		serverdisplay_t serverFind;
		serverFind.m_iListID = -1;
		serverFind.m_bDoNotRefresh = false;
		iServerMap = m_mapServers.Insert( iServer, serverFind );
		m_mapServerIP.Insert( netAdr, iServer );
	}

	serverdisplay_t *pServer = &m_mapServers[ iServerMap ];
	pServer->m_iServerID = iServer;
	Assert( pServerItem->m_NetAdr.GetIP() != 0 );

	// check filters
	bool removeItem = false;
	if ( !CheckPrimaryFilters( *pServerItem ) )
	{
		// server has been filtered at a primary level
		// remove from lists
		pServer->m_bDoNotRefresh = true;

		// remove from UI list
		removeItem = true;

		if ( m_pGameList->IsValidItemID( pServer->m_iListID ) )
		{
			m_pGameList->RemoveItem( pServer->m_iListID );
			pServer->m_iListID = GetInvalidServerListID();
		}

		return;
	}
	else if (!CheckSecondaryFilters( *pServerItem ))
	{
		// we still ping this server in the future; however it is removed from UI list
		removeItem = true;
	}

	// update UI
	KeyValues *kv;
	if ( m_pGameList->IsValidItemID( pServer->m_iListID ) )
	{
		// we're updating an existing entry
		kv = m_pGameList->GetItem( pServer->m_iListID );
		m_pGameList->SetUserData( pServer->m_iListID, pServer->m_iServerID );
	}
	else
	{
		// new entry
		kv = new KeyValues("Server");
	}

	CUtlVector< char* > mapStrings;
	V_SplitString( pServerItem->m_szMap, "/", mapStrings );
	if ( mapStrings.Count() > 1 )
	{
		if ( V_strcmp( mapStrings[0], "workshop" ) == 0 )
		{
			kv->SetString("workshopId", mapStrings[1] );
		}
		else
		{
			kv->SetString("workshopId", mapStrings[0] );
		}
	}
	else
	{
		kv->SetString("workshopId", "" );
	}

	FOR_EACH_VEC( mapStrings, i )
	{
		delete[] mapStrings[i];
	}

	kv->SetString("name", pServerItem->GetName());
	kv->SetString("map", V_UnqualifiedFileName( pServerItem->m_szMap ) );
	kv->SetString("GameDir", pServerItem->m_szGameDir);
	kv->SetString("GameDesc", pServerItem->m_szGameDescription);
	kv->SetBool("password", pServerItem->m_bPassword);
	
	if ( pServerItem->m_nBotPlayers > 0 )
		kv->SetInt("bots", pServerItem->m_nBotPlayers);
	else
		kv->SetString("bots", "");
	
	if ( pServerItem->m_bSecure )
	{
		// show the denied icon if banned from secure servers, the secure icon otherwise
		kv->SetInt("secure", ServerBrowser().IsVACBannedFromGame( pServerItem->m_nAppID ) ?  4 : 3);
	}
	else
	{
		kv->SetInt("secure", 0);
	}

	kv->SetString( "IPAddr", pServerItem->m_NetAdr.GetConnectionAddressString() );

	// Set the game type icon if it matches a registered prefix
	FOR_EACH_VEC( g_GameTypes, i )
	{
		gametypes_t &type = g_GameTypes[ i ];
		if ( type.m_iIconImageIndex > 0 && StringHasPrefix( V_UnqualifiedFileName( pServerItem->m_szMap ), type.m_szPrefix ) )
		{
			kv->SetInt( "Mode", type.m_iIconImageIndex );
		}
	}

	// Mark this map as using workshop as appropriate
	if ( GetMapIDFromMapPath( pServerItem->m_szMap ) != 0 )
	{
		kv->SetInt( "Workshop", m_iWorkshopIconIndex );
	}

	int nAdjustedForBotsPlayers = MAX( 0, pServerItem->m_nPlayers - pServerItem->m_nBotPlayers );
	int nAdjustedForBotsMaxPlayers = MAX( 0, pServerItem->m_nMaxPlayers - pServerItem->m_nBotPlayers );

	char buf[32];
	Q_snprintf(buf, sizeof(buf), "%d / %d", nAdjustedForBotsPlayers, nAdjustedForBotsMaxPlayers);
	kv->SetString("Players", buf);

	kv->SetInt("PlayerCount", nAdjustedForBotsPlayers );
	
	kv->SetInt("Ping", pServerItem->m_nPing);

	kv->SetString("Tags", pServerItem->m_szGameTags );

	if ( pServerItem->m_ulTimeLastPlayed )
	{
		// construct a time string for last played time
		struct tm now;
		Plat_ConvertToLocalTime( pServerItem->m_ulTimeLastPlayed, &now );

		char buf[64];
		strftime(buf, sizeof(buf), "%a %d %b %I:%M%p", &now);
		Q_strlower(buf + strlen(buf) - 4);
		kv->SetString("LastPlayed", buf);
	}

	if ( pServer->m_bDoNotRefresh )
	{
		// clear out the vars
		kv->SetString("Ping", "");
		kv->SetWString("GameDesc", g_pVGuiLocalize->Find("#ServerBrowser_NotResponding"));
		kv->SetString("Players", "");
		kv->SetString("map", "");
	}

	if ( !m_pGameList->IsValidItemID( pServer->m_iListID ) )
	{
		// new server, add to list
		pServer->m_iListID = m_pGameList->AddItem(kv, pServer->m_iServerID, false, false);
		if ( m_bAutoSelectFirstItemInGameList && m_pGameList->GetItemCount() == 1 )
		{
			m_pGameList->AddSelectedItem( pServer->m_iListID );
		}

		m_pGameList->SetItemVisible( pServer->m_iListID, !removeItem );
		
		kv->deleteThis();
	}
	else
	{
		// tell the list that we've changed the data
		m_pGameList->ApplyItemChanges( pServer->m_iListID );
		m_pGameList->SetItemVisible( pServer->m_iListID, !removeItem );
	}

	PrepareQuickListMap( pServerItem->m_szMap, pServer->m_iListID );
	UpdateStatus();
	m_iServerRefreshCount++;
}

//=============================================================================
// HPE_BEGIN:
// [tj] New function that hide and shows the filters and map lists based on 
//		selected UI elements.
//=============================================================================
void CBaseGamesPage::UpdateFilterAndQuickListVisibility()
{
	bool showQuickList = false; //m_pQuickListCheckButton->IsSelected();
	bool showFilter = m_pFilter->IsSelected();

	m_bFiltersVisible = !showQuickList && !m_pCustomResFilename && showFilter;
	
	int wide, tall;
	GetSize( wide, tall );
	SetSize( 624, 278 );

	UpdateDerivedLayouts();		
	UpdateGameFilter();

	if ( m_hFont )
	{
		SETUP_PANEL( m_pGameList );
		m_pGameList->SetFont( m_hFont );
	}

	SetSize( wide, tall );

	
	m_pQuickList->SetVisible( showQuickList );
	m_pGameList->SetVisible( !showQuickList );
	m_pFilter->SetVisible( !showQuickList );
	m_pFilterString->SetVisible ( !showQuickList );		


	InvalidateLayout();

	UpdateFilterSettings();
	ApplyGameFilters();
	
}
//=============================================================================
// HPE_END
//=============================================================================

//-----------------------------------------------------------------------------
// Purpose: Handles filter dropdown being toggled
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnButtonToggled( Panel *panel, int state )
{
	//=============================================================================
	// HPE_BEGIN:
	// [tj] Replaced elaborqate logic with a call to the general function to take care of it
	//=============================================================================
	UpdateFilterAndQuickListVisibility();
	//=============================================================================
	// HPE_END
	//=============================================================================

	if (panel == m_pNoFullServersFilterCheck || panel == m_pNoEmptyServersFilterCheck || panel == m_pNoPasswordFilterCheck)
	{
		// treat changing these buttons like any other filter has changed
		OnTextChanged(panel, "");
	}

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseGamesPage::UpdateDerivedLayouts( void )
{
	char rgchControlSettings[MAX_PATH];
	if ( m_pCustomResFilename )
	{
		Q_snprintf( rgchControlSettings, sizeof( rgchControlSettings ), "%s", m_pCustomResFilename );
	}
	else
	{
		if ( m_pFilter->IsSelected() /* && !m_pQuickListCheckButton->IsSelected() */ )
		{
			// drop down
			V_strncpy( rgchControlSettings, "servers/InternetGamesPage_Filters.res", sizeof( rgchControlSettings ) );
		}
		else
		{
			// hide filter area
			V_strncpy( rgchControlSettings, "servers/InternetGamesPage.res", sizeof( rgchControlSettings ) );
		}
	}

	const char *pPathID = "PLATFORM";

	if ( g_pFullFileSystem->FileExists( rgchControlSettings, "MOD" ) )
	{
		pPathID = "MOD";
	}	

	LoadControlSettings( rgchControlSettings, pPathID );
}

//-----------------------------------------------------------------------------
// Purpose: Called when the game dir combo box is changed
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnTextChanged(Panel *panel, const char *text)
{
	if (!Q_stricmp(text, m_szComboAllText))
	{
		ComboBox *box = dynamic_cast<ComboBox *>(panel);
		if (box)
		{
			box->SetText("");
			text = "";
		}
	}

	// get filter settings from controls
	UpdateFilterSettings();

	// apply settings
	ApplyGameFilters();

	if ( m_bFiltersVisible && ( panel == m_pGameFilter || panel == m_pLocationFilter ) && ServerBrowserDialog().IsVisible() )
	{
		// if they changed games and/or region then cancel the refresh because the old list they are getting
		// will be for the wrong game, so stop and start a refresh
		StopRefresh(); 
		GetNewServerList(); 
	}
}

//-----------------------------------------------------------------------------
// Purpose: applies only the game filter to the current list
//-----------------------------------------------------------------------------
void CBaseGamesPage::ApplyGameFilters()
{
	if ( !steamapicontext->SteamMatchmakingServers() )
		return;

	m_iServersBlacklisted = 0;

	// loop through all the servers checking filters
	FOR_EACH_MAP_FAST( m_mapServers, i )
	{
		serverdisplay_t &server = m_mapServers[ i ];
		gameserveritem_t *pServer = steamapicontext->SteamMatchmakingServers()->GetServerDetails( m_hRequest, server.m_iServerID );
		if ( !pServer ) 
			continue;

		if (!CheckPrimaryFilters( *pServer ) || !CheckSecondaryFilters( *pServer ))
		{
			// server failed filtering, remove it
			server.m_bDoNotRefresh = true;
			if ( m_pGameList->IsValidItemID( server.m_iListID) )
			{
				// don't remove the server from list, just hide since this is a lot faster
				m_pGameList->SetItemVisible( server.m_iListID, false );
			}
		}
		else if ( BShowServer( server ) )
		{
			// server passed filters, so it can be refreshed again
			server.m_bDoNotRefresh = false;
			gameserveritem_t *pServer = steamapicontext->SteamMatchmakingServers()->GetServerDetails( m_hRequest, server.m_iServerID );

			// re-add item to list
			if ( !m_pGameList->IsValidItemID( server.m_iListID ) )
			{
				KeyValues *kv = new KeyValues("Server");
				kv->SetString("name", pServer->GetName());
				kv->SetString("map", pServer->m_szMap);
				kv->SetString("GameDir", pServer->m_szGameDir);
				kv->SetString( "GameTags", pServer->m_szGameTags );
				if ( pServer->m_szGameDescription[0] )
				{
					kv->SetString("GameDesc", pServer->m_szGameDescription );
				}
				else
				{
					kv->SetWString("GameDesc", g_pVGuiLocalize->Find("#ServerBrowser_PendingPing"));
				}

				int nAdjustedForBotsPlayers = MAX( 0, pServer->m_nPlayers - pServer->m_nBotPlayers );
				int nAdjustedForBotsMaxPlayers = MAX( 0, pServer->m_nMaxPlayers - pServer->m_nBotPlayers );

				char buf[256];
				Q_snprintf(buf, sizeof(buf), "%d / %d", nAdjustedForBotsPlayers, nAdjustedForBotsMaxPlayers);
				kv->SetString( "Players", buf);
				kv->SetInt( "Ping", pServer->m_nPing );
				kv->SetBool( "password", pServer->m_bPassword);
				if ( pServer->m_nBotPlayers > 0 )
					kv->SetInt("bots", pServer->m_nBotPlayers);
				else
					kv->SetString("bots", "");
				
				server.m_iListID = m_pGameList->AddItem(kv, server.m_iServerID, false, false);
				kv->deleteThis();
			}
			
			// make sure the server is visible
			m_pGameList->SetItemVisible( server.m_iListID, true );
		}
	}

	UpdateStatus();
	m_pGameList->SortList();
	InvalidateLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Resets UI server count
//-----------------------------------------------------------------------------
void CBaseGamesPage::UpdateStatus()
{
	if (m_pGameList->GetItemCount() > 1)
	{
		wchar_t header[256];
		wchar_t count[128];
		wchar_t blacklistcount[128];

		_snwprintf( count, Q_ARRAYSIZE(count), L"%d", m_pGameList->GetItemCount() );
		_snwprintf( blacklistcount, Q_ARRAYSIZE(blacklistcount), L"%d", m_iServersBlacklisted );
		g_pVGuiLocalize->ConstructString( header, sizeof( header ), g_pVGuiLocalize->Find( "#ServerBrowser_ServersCountWithBlacklist"), 2, count, blacklistcount );
		m_pGameList->SetColumnHeaderText(3, header);
	}
	else
	{
		m_pGameList->SetColumnHeaderText(3, g_pVGuiLocalize->Find("#ServerBrowser_Servers"));
	}
}

//-----------------------------------------------------------------------------
// Purpose: gets filter settings from controls
//-----------------------------------------------------------------------------
void CBaseGamesPage::UpdateFilterSettings()
{
	// game
	if ( ServerBrowserDialog().GetActiveModName() )
	{
		// overriding the game filter
		Q_strncpy(m_szGameFilter, ServerBrowserDialog().GetActiveModName(), sizeof(m_szGameFilter));
		m_iLimitToAppID = ServerBrowserDialog().GetActiveAppID();
		RecalculateFilterString();
		UpdateGameFilter();
	}
	else
	{
		KeyValues *data = m_pGameFilter->GetActiveItemUserData();
		if (data && Q_strlen( data->GetString( "gamedir" ) ) > 0 )
		{
			Q_strncpy( m_szGameFilter, data->GetString( "gamedir" ), sizeof( m_szGameFilter ) );
			if ( Q_strlen( m_szGameFilter ) > 0 ) // if there is a gamedir
			{
				m_iLimitToAppID = CGameID( data->GetUint64( "appid", 0 ) );
			}
			else
			{
				m_iLimitToAppID.Reset();
			}
		}
		else
		{
			m_iLimitToAppID.Reset();
			m_szGameFilter[0] = 0;
		}
		m_pGameFilter->SetEnabled(true);
	}
	Q_strlower(m_szGameFilter);

	// map
	m_pMapFilter->GetText(m_szMapFilter, sizeof(m_szMapFilter) - 1);
	Q_strlower(m_szMapFilter);

	m_iWorkshopFilter = m_pWorkshopFilter->GetActiveItem();

	// ping
	char buf[256];
	m_pPingFilter->GetText(buf, sizeof(buf));
	if (buf[0])
	{
		m_iPingFilter = atoi(buf + 2);
	}
	else
	{
		m_iPingFilter = 0;
	}

	// players
	m_bFilterNoFullServers = m_pNoFullServersFilterCheck->IsSelected();
	m_bFilterNoEmptyServers = m_pNoEmptyServersFilterCheck->IsSelected();
	m_bFilterNoPasswordedServers = m_pNoPasswordFilterCheck->IsSelected();
	m_iSecureFilter = m_pSecureFilter->GetActiveItem();

	m_vecServerFilters.RemoveAll();

	bool bFilterNoEmpty = m_bFilterNoEmptyServers;
	bool bFilterNoFull = m_bFilterNoFullServers;
	bool bFilterNoPassword = m_bFilterNoPasswordedServers;
	int iFilterSecure = m_iSecureFilter;

	if ( m_pQuickList->IsVisible() == true )
	{
		bFilterNoEmpty = true;
		bFilterNoFull = true;
		bFilterNoPassword = true;
		iFilterSecure = FILTER_SECURESERVERSONLY;
	}

	// update master filter string text
	if (m_szGameFilter[0] && m_iLimitToAppID.AppID() != 1002 ) // HACKHACK: Alfred - don't use a dir filter for RDKF
	{
		m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "gamedir", m_szGameFilter ) );
	}
	if (bFilterNoEmpty)
	{
		m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "empty", "1" ) );
	}
	if (bFilterNoFull)
	{
		m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "full", "1" ) );
	}
	if (iFilterSecure == FILTER_SECURESERVERSONLY)
	{
		m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "secure", "1" ) );
	}
	int regCode = GetRegionCodeToFilter();
	if ( regCode > 0 )
	{
		char szRegCode[ 32 ];
		Q_snprintf( szRegCode, sizeof(szRegCode), "%i", regCode );
		m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "region", szRegCode ) );		
	}

	m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "gametagsnor", "valve_ds" ) );	

	// copy filter settings into filter file
	KeyValues *filter = ServerBrowserDialog().GetFilterSaveData(GetName());

	// only save the game filter if we're not overriding it
	if (!ServerBrowserDialog().GetActiveModName())
	{
		filter->SetString("game", m_szGameFilter);
		filter->SetUint64( "appid", m_iLimitToAppID.ToUint64() );
	}

	filter->SetString("map", m_szMapFilter);
	filter->SetInt( "Workshop", m_iWorkshopFilter );

	filter->SetInt("ping", m_iPingFilter);

	if ( m_pLocationFilter->GetItemCount() > 1 )
	{ 
		// only save this if there are options to choose from
		filter->SetInt("location", m_pLocationFilter->GetActiveItem());
	}
	
	filter->SetBool("NoFull", m_bFilterNoFullServers);
	filter->SetBool("NoEmpty", m_bFilterNoEmptyServers);
	filter->SetBool("NoPassword", m_bFilterNoPasswordedServers);
	filter->SetInt("Secure", m_iSecureFilter);
//	filter->SetInt("QuickList", m_pQuickListCheckButton->IsSelected() );
	filter->SetInt("tagsinclude", m_pTagsIncludeFilter->GetActiveItem() );

	filter->SetString("gametype", "notags" );
	m_vecServerFilters.AddToTail( MatchMakingKeyValuePair_t( "gametype", "notags" ) );

	OnSaveFilter(filter);

	RecalculateFilterString();
}


//-----------------------------------------------------------------------------
// Purpose: allow derived classes access to the saved filter string
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnSaveFilter(KeyValues *filter)
{
}

//-----------------------------------------------------------------------------
// Purpose: allow derived classes access to the saved filter string
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnLoadFilter(KeyValues *filter)
{
}

//-----------------------------------------------------------------------------
// Purpose: reconstructs the filter description string from the current filter settings
//-----------------------------------------------------------------------------
void CBaseGamesPage::RecalculateFilterString()
{
	wchar_t unicode[2048], tempUnicode[128], spacerUnicode[8];
	unicode[0] = 0;
	int iTempUnicodeSize = sizeof( tempUnicode );

	Q_UTF8ToUnicode( "; ", spacerUnicode, sizeof( spacerUnicode ) );

	if (m_szGameFilter[0])
	{
		Q_UTF8ToUnicode( ModList().GetModNameForModDir( m_iLimitToAppID ), tempUnicode, iTempUnicodeSize );
		wcscat( unicode, tempUnicode );
		wcscat( unicode, spacerUnicode );
	}

	if (m_iSecureFilter == FILTER_SECURESERVERSONLY)
	{
		wcscat( unicode, g_pVGuiLocalize->Find( "#ServerBrowser_FilterDescSecureOnly" ) );
		wcscat( unicode, spacerUnicode );
	}
	else if (m_iSecureFilter == FILTER_INSECURESERVERSONLY)
	{
		wcscat( unicode, g_pVGuiLocalize->Find( "#ServerBrowser_FilterDescInsecureOnly" ) );
		wcscat( unicode, spacerUnicode );
	}

	if (m_pLocationFilter->GetActiveItem() > 0)
	{
		m_pLocationFilter->GetText(tempUnicode, sizeof(tempUnicode));
		wcscat( unicode, tempUnicode );
		wcscat( unicode, spacerUnicode );
	}

	if (m_iPingFilter)
	{
		char tmpBuf[16];
		_snprintf( tmpBuf, sizeof(tmpBuf), "%d", m_iPingFilter );

		wcscat( unicode, g_pVGuiLocalize->Find( "#ServerBrowser_FilterDescLatency" ) );
		Q_UTF8ToUnicode( " < ", tempUnicode, iTempUnicodeSize );
		wcscat( unicode, tempUnicode );
		Q_UTF8ToUnicode(tmpBuf, tempUnicode, iTempUnicodeSize );
		wcscat( unicode, tempUnicode );	
		wcscat( unicode, spacerUnicode );
	}

	if (m_bFilterNoFullServers)
	{
		wcscat( unicode, g_pVGuiLocalize->Find( "#ServerBrowser_FilterDescNotFull" ) );
		wcscat( unicode, spacerUnicode );
	}

	if (m_bFilterNoEmptyServers)
	{
		wcscat( unicode, g_pVGuiLocalize->Find( "#ServerBrowser_FilterDescNotEmpty" ) );
		wcscat( unicode, spacerUnicode );
	}

	if (m_bFilterNoPasswordedServers)
	{
		wcscat( unicode, g_pVGuiLocalize->Find( "#ServerBrowser_FilterDescNoPassword" ) );
		wcscat( unicode, spacerUnicode );
	}

	if (m_iWorkshopFilter == FILTER_SUBSCRIBEDMAPSONLY)
	{
		wcscat( unicode, g_pVGuiLocalize->Find( "#ServerBrowser_FilterDescSubscribedOnly" ) );
		wcscat( unicode, spacerUnicode );
	}
	else if (m_iWorkshopFilter == FILTER_FEATUREDMAPSONLY)
	{
		wcscat( unicode, g_pVGuiLocalize->Find( "#ServerBrowser_FilterDescFeaturedOnly" ) );
		wcscat( unicode, spacerUnicode );
	}

	if (m_szMapFilter[0])
	{
		Q_UTF8ToUnicode( m_szMapFilter, tempUnicode, iTempUnicodeSize );
		wcscat( unicode, tempUnicode );
	}

	m_pFilterString->SetText(unicode);
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if the server passes the primary filters
//			if the server fails the filters, it will not be refreshed again
//-----------------------------------------------------------------------------
bool CBaseGamesPage::CheckPrimaryFilters( gameserveritem_t &server )
{
	if (m_szGameFilter[0] && ( server.m_szGameDir[0] || server.m_nPing ) && Q_stricmp(m_szGameFilter, server.m_szGameDir ) && server.m_nAppID != 1002 ) // HACKHACK Alfred - don't apply gamedir filter to rdkf
	{
		return false;
	}
	
	// Mods on Steam app IDs are OK
	if ( ( server.m_nAppID >= 17500 && server.m_nAppID <= 17599 ) || ( server.m_nAppID >= 17700 && server.m_nAppID <= 17799 ) )
	{
		return true;
	}

	if ( server.m_nAppID < 200 || ( server.m_nAppID > 900 && server.m_nAppID != 4000 ) )
		return false;

	// If it's blacklisted, we ignore it too
	if ( ServerBrowserDialog().IsServerBlacklisted( server ) )
	{
		m_iServersBlacklisted++;
		return false;
	}

	// In CSGO we always want to filter out matchmaking servers from the server browser.
	if ( Q_strnistr( server.m_szGameTags, "valve_ds", MAX_TAG_CHARACTERS ) != NULL )
	{
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if a server passes the secondary set of filters
//			server will be continued to be pinged if it fails the filter, since
//			the relvent server data is dynamic
//-----------------------------------------------------------------------------
bool CBaseGamesPage::CheckSecondaryFilters( gameserveritem_t &server )
{
	bool bFilterNoEmpty = m_bFilterNoEmptyServers;
	bool bFilterNoFull = m_bFilterNoFullServers;
	int iFilterWorkshop = m_iWorkshopFilter;
	int	iFilterPing = m_iPingFilter;
	bool bFilterNoPassword = m_bFilterNoPasswordedServers;
	int iFilterSecure = m_iSecureFilter;

	if ( m_pQuickList->IsVisible() == true )
	{
		bFilterNoEmpty = true;
		bFilterNoFull = true;
		iFilterPing = QUICKLIST_FILTER_MIN_PING;
		bFilterNoPassword = true;
		iFilterSecure = FILTER_SECURESERVERSONLY;
		iFilterWorkshop = 0;	// all
	}

	if ( bFilterNoEmpty && (server.m_nPlayers - server.m_nBotPlayers) < 1 )
	{
		return false;
	}

	if ( bFilterNoFull && server.m_nPlayers >= server.m_nMaxPlayers )
	{
		return false;
	}

	if ( iFilterPing && server.m_nPing > iFilterPing )
	{
		return false;
	}

	if ( bFilterNoPassword && server.m_bPassword )
	{
		return false;
	}

	if ( iFilterSecure == FILTER_SECURESERVERSONLY && !server.m_bSecure )
	{
		return false;
	}

	if ( iFilterSecure == FILTER_INSECURESERVERSONLY && server.m_bSecure )
	{
		return false;
	}

	char szBaseFilename[ MAX_PATH ] = {0};
	V_FileBase( server.m_szMap, szBaseFilename, sizeof( szBaseFilename ) );

	if ( iFilterWorkshop == FILTER_SUBSCRIBEDMAPSONLY )
	{
		// Make sure its map is in the subscribed list!
		if ( !engine->IsSubscribedMap( szBaseFilename, false ) )
		{
			return false;
		}
	}
	else if ( iFilterWorkshop == FILTER_FEATUREDMAPSONLY )
	{
		// Make sure its map is in the featured list!
		if ( !engine->IsFeaturedMap( szBaseFilename, false ) )
		{
			return false;
			}
	}

	if ( m_pQuickList->IsVisible() == false )
	{
		// Map name uses substring
		// Can start with / to search by prefix in base filename
		// Can start with * to search for suffix
		// Can start with // to search by exact prefix
		switch ( m_szMapFilter[0] )
		{
		case 0:
			// empty string
			break;

		case '/':
			if ( m_szMapFilter[1] == '/' )
			{
				// double-slash -- exact prefix search
				if ( m_szMapFilter[2] && !StringHasPrefix( server.m_szMap, m_szMapFilter + 2 ) )
					return false;
			}
			else if ( m_szMapFilter[1] )
			{
				char szBaseFilename[ MAX_PATH ] = {0};
				V_FileBase( server.m_szMap, szBaseFilename, sizeof( szBaseFilename ) );
	
				// single-slash -- prefix in base filename
				if ( !StringHasPrefix( szBaseFilename, m_szMapFilter + 1 ) )
					return false;
			}
			break;

		case '*':
			{
				int nLenSuffix = Q_strlen( m_szMapFilter + 1 );
				int nLenServerMap = Q_strlen( server.m_szMap );
				if ( ( nLenSuffix > 0 ) && ( ( nLenServerMap < nLenSuffix ) ||
					Q_stricmp( server.m_szMap + nLenServerMap - nLenSuffix, m_szMapFilter + 1 ) ) )
					return false;
			}
			break;

		default:
			// substring search
			if ( !Q_stristr( server.m_szMap, m_szMapFilter ) )
				return false;
			break;
		}
	}
	
	return CheckTagFilter( server );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
uint32 CBaseGamesPage::GetServerFilters( MatchMakingKeyValuePair_t **pFilters )
{
	*pFilters = m_vecServerFilters.Base();
	return m_vecServerFilters.Count();
}


//-----------------------------------------------------------------------------
// Purpose: call to let the UI now whether the game list is currently refreshing
//-----------------------------------------------------------------------------
void CBaseGamesPage::SetRefreshing(bool state)
{
	if (state)
	{
		ServerBrowserDialog().UpdateStatusText("#ServerBrowser_RefreshingServerList");

		// clear message in panel
		m_pGameList->SetEmptyListText("");
		m_pRefreshAll->SetText("#ServerBrowser_StopRefreshingList");
		m_pRefreshAll->SetCommand("stoprefresh");
		m_pRefreshQuick->SetEnabled(false);
	}
	else
	{
		ServerBrowserDialog().UpdateStatusText("");
		if (SupportsItem(IGameList::GETNEWLIST))
		{
			m_pRefreshAll->SetText("#ServerBrowser_RefreshAll");
		}
		else
		{
			m_pRefreshAll->SetText("#ServerBrowser_Refresh");
		}
		m_pRefreshAll->SetCommand("GetNewList");

		// 'refresh quick' button is only enabled if there are servers in the list
		if (m_pGameList->GetItemCount() > 0)
		{
			m_pRefreshQuick->SetEnabled(true);
		}
		else
		{
			m_pRefreshQuick->SetEnabled(false);
		}
	}
}

#if !defined(NO_STEAM)
//-----------------------------------------------------------------------------
// Purpose: Pop up the overlay showing the requested community map page
//-----------------------------------------------------------------------------
bool CBaseGamesPage::ViewCommunityMapsInWorkshop( uint64 workshopID /*= 0*/ )
{
	if ( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamUtils() && steamapicontext->SteamFriends() && steamapicontext->SteamUtils() )
	{
		// Overlay is disabled
		if( !steamapicontext->SteamUtils()->IsOverlayEnabled() )
			return false;

		EUniverse eUniverse = steamapicontext && steamapicontext->SteamUtils()
			? steamapicontext->SteamUtils()->GetConnectedUniverse()
			: k_EUniverseInvalid;

		if ( eUniverse == k_EUniverseInvalid )
			return false;

		char szDestURL[MAX_PATH];
		const char *lpszDomanPrefix = ( eUniverse == k_EUniverseBeta ) ? "beta" : "www";

		if ( workshopID != 0 )
		{
			V_snprintf( szDestURL, ARRAYSIZE( szDestURL ), "http://%s.steamcommunity.com/sharedfiles/filedetails/?id=%llu", lpszDomanPrefix, workshopID );
		}
		else
		{
			V_snprintf( szDestURL, ARRAYSIZE( szDestURL ), "http://%s.steamcommunity.com/workshop/browse?appid=%lu", lpszDomanPrefix, steamapicontext->SteamUtils()->GetAppID() );
		}

		steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage( szDestURL );

		return true;
	}        

	return false;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnCommand(const char *command)
{
	if (!Q_stricmp(command, "Connect"))
	{
		OnBeginConnect();
	}
	else if (!Q_stricmp(command, "stoprefresh"))
	{
		// cancel the existing refresh
		StopRefresh();
	}
	else if ( !Q_stricmp(command, "refresh") )
	{
		if ( steamapicontext->SteamMatchmakingServers() )
			steamapicontext->SteamMatchmakingServers()->RefreshQuery( m_hRequest );
		SetRefreshing( true );
		m_iServerRefreshCount = 0;
		ClearQuickList();
	}
	else if (!Q_stricmp(command, "GetNewList"))
	{
		GetNewServerList();
	}
	else if (!Q_stricmp(command, "OpenWorkshop"))
	{
#if !defined(NO_STEAM)
		ViewCommunityMapsInWorkshop();
#endif
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when a row gets selected in the list
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnItemSelected()
{
	if ( GetSelectedServerID() == -1 )
	{
		m_pConnect->SetEnabled(false);
	}
	else
	{
		m_pConnect->SetEnabled(true);
	}
}

//-----------------------------------------------------------------------------
// Purpose: refreshes server list on F5
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnKeyCodePressed(vgui::KeyCode code)
{
	if (code == KEY_F5)
	{
		StartRefresh();
	}
	else
	{
		BaseClass::OnKeyCodePressed(code);
	}
}


//-----------------------------------------------------------------------------
// Purpose: Handle enter pressed in the games list page. Return true
// to intercept the message instead of passing it on through vgui.
//-----------------------------------------------------------------------------
bool CBaseGamesPage::OnGameListEnterPressed()
{
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Get the # items selected in the game list.
//-----------------------------------------------------------------------------
int CBaseGamesPage::GetSelectedItemsCount()
{
	return m_pGameList->GetSelectedItemsCount();
}


//-----------------------------------------------------------------------------
// Purpose: adds a server to the favorites
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnAddToFavorites()
{
	if ( !steamapicontext->SteamMatchmakingServers() )
		return;

	// loop through all the selected favorites
	for (int i = 0; i < m_pGameList->GetSelectedItemsCount(); i++)
	{
		int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(i));

		gameserveritem_t *pServer = steamapicontext->SteamMatchmakingServers()->GetServerDetails( m_hRequest, serverID );
		if ( pServer )
		{
			// add to favorites list
			ServerBrowserDialog().AddServerToFavorites(*pServer);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: adds a server to the blacklist
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnAddToBlacklist()
{
	if ( !steamapicontext->SteamMatchmakingServers() )
		return;

	// loop through all the selected favorites
	for (int i = 0; i < m_pGameList->GetSelectedItemsCount(); i++)
	{
		int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(i));

		gameserveritem_t *pServer = steamapicontext->SteamMatchmakingServers()->GetServerDetails( m_hRequest, serverID );
		if ( pServer )
		{
			ServerBrowserDialog().AddServerToBlacklist(*pServer);
		}
	}
	ServerBrowserDialog().BlacklistsChanged();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseGamesPage::ServerFailedToRespond( HServerListRequest hReq, int iServer )
{
	ServerResponded( hReq, iServer );
}


//-----------------------------------------------------------------------------
// Purpose: removes the server from the UI list
//-----------------------------------------------------------------------------
void CBaseGamesPage::RemoveServer( serverdisplay_t &server )
{
	if ( m_pGameList->IsValidItemID( server.m_iListID ) )
	{
		// don't remove the server from list, just hide since this is a lot faster
		m_pGameList->SetItemVisible( server.m_iListID, false );

		// find the row in the list and kill
		//	m_pGameList->RemoveItem(server.listEntryID);
		//	server.listEntryID = GetInvalidServerListID();
	}

	UpdateStatus();
}


//-----------------------------------------------------------------------------
// Purpose: refreshes a single server
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnRefreshServer( int serverID )
{
	if ( !steamapicontext->SteamMatchmakingServers() )
		return;

	// walk the list of selected servers refreshing them
	for (int i = 0; i < m_pGameList->GetSelectedItemsCount(); i++)
	{
		int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(i));

		// refresh this server
		steamapicontext->SteamMatchmakingServers()->RefreshServer( m_hRequest, serverID );
	}

	SetRefreshing(IsRefreshing());
}

//-----------------------------------------------------------------------------
// Purpose: view selected server in workshop
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnViewWorkshop( int serverID )
{
	gameserveritem_t *pServer = ServerBrowserDialog().GetServer( serverID );
	ViewCommunityMapsInWorkshop( pServer ? GetMapIDFromMapPath( pServer->m_szMap ) : 0 );
}

//-----------------------------------------------------------------------------
// Purpose: starts the servers refreshing
//-----------------------------------------------------------------------------
void CBaseGamesPage::StartRefresh()
{
	if ( !steamapicontext->SteamMatchmakingServers() )
		return;

	ClearServerList();
	MatchMakingKeyValuePair_t *pFilters;
	int nFilters = GetServerFilters( &pFilters );

	if ( m_hRequest )
	{
		steamapicontext->SteamMatchmakingServers()->ReleaseRequest( m_hRequest );
		m_hRequest = NULL;
	}

	switch ( m_eMatchMakingType )
	{
	case eFavoritesServer:
		m_hRequest = steamapicontext->SteamMatchmakingServers()->RequestFavoritesServerList( GetFilterAppID().AppID(), &pFilters, nFilters, this );
		break;
	case eHistoryServer:
		m_hRequest = steamapicontext->SteamMatchmakingServers()->RequestHistoryServerList( GetFilterAppID().AppID(), &pFilters, nFilters, this );
		break;
	case eInternetServer:
		m_hRequest = steamapicontext->SteamMatchmakingServers()->RequestInternetServerList( GetFilterAppID().AppID(), &pFilters, nFilters, this );
		break;
	case eSpectatorServer:
		m_hRequest = steamapicontext->SteamMatchmakingServers()->RequestSpectatorServerList( GetFilterAppID().AppID(), &pFilters, nFilters, this );
		break;
	case eFriendsServer:
		m_hRequest = steamapicontext->SteamMatchmakingServers()->RequestFriendsServerList( GetFilterAppID().AppID(), &pFilters, nFilters, this );
		break;
	case eLANServer:
		m_hRequest = steamapicontext->SteamMatchmakingServers()->RequestLANServerList( GetFilterAppID().AppID(), this );
		break;
	default:
		Assert( !"Unknown server type" );
		break;
	}

	SetRefreshing( true );

	m_iServerRefreshCount = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseGamesPage::ClearQuickList( void )
{
	m_pQuickList->DeleteAllItems();
	m_vecMapNamesFound.RemoveAll();

	int iIndex = m_quicklistserverlist.First();

	while ( iIndex != m_quicklistserverlist.InvalidIndex() )
	{
		CQuickListMapServerList *vecMapServers = &m_quicklistserverlist[iIndex];

		vecMapServers->RemoveAll();

		iIndex = m_quicklistserverlist.Next( iIndex );
	}

	m_quicklistserverlist.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Remove all the servers we currently have
//-----------------------------------------------------------------------------
void CBaseGamesPage::ClearServerList()
{ 
	m_mapServers.RemoveAll(); 
	m_mapServerIP.RemoveAll();
	m_pGameList->RemoveAll();
	m_iServersBlacklisted = 0;

	ClearQuickList();
}


//-----------------------------------------------------------------------------
// Purpose: get a new list of servers from the backend
//-----------------------------------------------------------------------------
void CBaseGamesPage::GetNewServerList()
{
	StartRefresh();
}


//-----------------------------------------------------------------------------
// Purpose: stops current refresh/GetNewServerList()
//-----------------------------------------------------------------------------
void CBaseGamesPage::StopRefresh()
{
	// clear update states
	m_iServerRefreshCount = 0;

	// Stop the server list refreshing
	if ( steamapicontext->SteamMatchmakingServers() )
		steamapicontext->SteamMatchmakingServers()->CancelQuery( m_hRequest );

	// update UI
	RefreshComplete( m_hRequest, eServerResponded );

	// apply settings
	ApplyGameFilters();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseGamesPage::RefreshComplete( HServerListRequest hReq, EMatchMakingServerResponse response )
{
	SelectQuickListServers();
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the list is currently refreshing servers
//-----------------------------------------------------------------------------
bool CBaseGamesPage::IsRefreshing()
{
	return steamapicontext->SteamMatchmakingServers() && steamapicontext->SteamMatchmakingServers()->IsRefreshing( m_hRequest );
}

//-----------------------------------------------------------------------------
// Purpose: Activates the page, starts refresh
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnPageShow()
{
	StartRefresh();
}

//-----------------------------------------------------------------------------
// Purpose: Called on page hide, stops any refresh
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnPageHide()
{
	StopRefresh();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
vgui::Panel *CBaseGamesPage::GetActiveList( void )
{
	if ( m_pQuickList->IsVisible() )
		return m_pQuickList;

	return m_pGameList;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBaseGamesPage::GetSelectedServerID( void )
{
	int serverID = -1;

	if ( m_pQuickList->IsVisible() == true )
	{
		if ( IsRefreshing() == true )
			return -1;

		if ( m_pQuickList->GetSelectedPanel() )
		{
			CQuickListPanel *pQuickPanel = dynamic_cast<CQuickListPanel*>( m_pQuickList->GetSelectedPanel() );

			if ( pQuickPanel )
			{
				serverID = m_pGameList->GetItemUserData( pQuickPanel->GetListID() );
			}
		}
	}
	else
	{
		if (!m_pGameList->GetSelectedItemsCount())
			return -1;

		// get the server
		serverID = m_pGameList->GetItemUserData( m_pGameList->GetSelectedItem(0) );
	}

	return serverID;
}

//-----------------------------------------------------------------------------
// Purpose: initiates server connection
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnBeginConnect()
{
	int serverID = GetSelectedServerID();
	
	if ( serverID == -1 )
		return;

	// Stop the current refresh
	StopRefresh();

	// join the game
	ServerBrowserDialog().JoinGame(this, serverID, PageTypeToString( m_eMatchMakingType ) );
}

//-----------------------------------------------------------------------------
// Purpose: Displays the current game info without connecting
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnViewGameInfo()
{
	int serverID = GetSelectedServerID();

	if ( serverID == -1 )
		return;

	// Stop the current refresh
	StopRefresh();

	// join the game
	ServerBrowserDialog().OpenGameInfoDialog(this, serverID);
}


//-----------------------------------------------------------------------------
// Purpose: Refresh if our favorites list changed
//-----------------------------------------------------------------------------
void CBaseGamesPage::OnFavoritesMsg( FavoritesListChanged_t *pFavListChanged )
{
	if ( !pFavListChanged->m_nIP ) // a zero for IP means the whole list was reloaded and we need to reload
	{
		switch ( m_eMatchMakingType )
		{
		case eInternetServer:
		case eLANServer:
		case eSpectatorServer:
		case eFriendsServer:
			return;
		case eFavoritesServer:
		case eHistoryServer:
			// check containing property sheet to see if the page is visible.
			// if not, don't bother initiating a server list grab right now -
			// it will happen when the dialog is activated later.
			if ( reinterpret_cast< PropertySheet* >( GetParent() )->GetActivePage() == this &&
				GetParent()->IsVisible() && ServerBrowserDialog().IsVisible()  )
			{
				GetNewServerList();
			}
			return;
		default:
			Assert( !"unknown matchmaking type" );
		}
		return;
	}

	switch ( m_eMatchMakingType )
	{
	case eInternetServer:
	case eLANServer:
	case eSpectatorServer:
	case eFriendsServer:
		break;
	case eFavoritesServer:
	case eHistoryServer:
		{
		int iIPServer = m_mapServerIP.Find( netadr_t( pFavListChanged->m_nIP, pFavListChanged->m_nConnPort ) );
		if ( iIPServer == m_mapServerIP.InvalidIndex() )
		{
			if ( pFavListChanged->m_bAdd )	
			{
				if ( steamapicontext->SteamMatchmakingServers() )
					steamapicontext->SteamMatchmakingServers()->PingServer( pFavListChanged->m_nIP, pFavListChanged->m_nQueryPort, this );
			}
			// ignore deletes of fav's we didn't have
		}
		else
		{
			if ( pFavListChanged->m_bAdd )	
			{
				if ( m_mapServerIP[ iIPServer ] > 0 )
					ServerResponded( m_hRequest, m_mapServerIP[ iIPServer ] );
			}
			else
			{
				int iServer = m_mapServers.Find( m_mapServerIP[ iIPServer ] );
				serverdisplay_t &server = m_mapServers[ iServer ];
				RemoveServer( server );
			}
		}
		}
		break;
	default:
		Assert( !"unknown matchmaking type" );
	};
}

const char* CBaseGamesPage::PageTypeToString( EPageType eType ) const
{
	switch ( eType )
	{
	case eFavoritesServer:
		return "ServerBrowserFavorites";
	case eHistoryServer:
		return "ServerBrowserHistory";
	case eInternetServer:
		return "ServerBrowserInternet";
	case eSpectatorServer:
		return "ServerBrowserSpectator";
	case eFriendsServer:
		return "ServerBrowserFriends";
	case eLANServer:
		return "ServerBrowserLAN";
	default:
		Assert( !"Unknown server type" );
		return "ServerBrowserUnknownPageType";
	}
}
