//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "pch_serverbrowser.h"

using namespace vgui;

// HACK: Copy/paste from ugc_utils... not worth adding to project just for this
uint64 GetMapIDFromMapPath( const char *pMapPath )
{
	char tmp[ MAX_PATH ];
	V_strcpy_safe( tmp, pMapPath );
	V_FixSlashes( tmp, '/' ); // internal path strings use forward slashes, make sure we compare like that.
	if ( V_strstr( tmp, "workshop/" ) )
	{
		V_StripFilename( tmp );
		V_StripTrailingSlash( tmp );
		const char* szDirName = V_GetFileName( tmp );
		return V_atoui64( szDirName );
	}

	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CServerContextMenu::CServerContextMenu(Panel *parent) : Menu(parent, "ServerContextMenu")
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CServerContextMenu::~CServerContextMenu()
{
}

//-----------------------------------------------------------------------------
// Purpose: Activates the menu
//-----------------------------------------------------------------------------
void CServerContextMenu::ShowMenu(
	Panel *target, 
	unsigned int serverID, 
	bool showConnect, 
	bool showViewGameInfo,
	bool showRefresh, 
	bool showAddToFavorites )
{
	if (showConnect)
	{
		AddMenuItem("ConnectToServer", "#ServerBrowser_ConnectToServer", new KeyValues("ConnectToServer", "serverID", serverID), target);
	}

	if (showViewGameInfo)
	{
		AddMenuItem("ViewGameInfo", "#ServerBrowser_ViewServerInfo", new KeyValues("ViewGameInfo", "serverID", serverID), target);
	}

	if (showRefresh)
	{
		AddMenuItem("RefreshServer", "#ServerBrowser_RefreshServer", new KeyValues("RefreshServer", "serverID", serverID), target);
	}

	if (showAddToFavorites)
	{
		AddMenuItem("AddToFavorites", "#ServerBrowser_AddServerToFavorites", new KeyValues("AddToFavorites", "serverID", serverID), target);
		AddMenuItem("AddToBlacklist", "#ServerBrowser_AddServerToBlacklist", new KeyValues("AddToBlacklist", "serverID", serverID), target);
	}

	gameserveritem_t *pServer = ServerBrowserDialog().GetServer( serverID );
	if ( pServer && GetMapIDFromMapPath( pServer->m_szMap ) != 0 )
	{
		AddMenuItem( "ViewWorkshop", "#ServerBrowser_ViewInWorkshop", new KeyValues( "ViewInWorkshop", "serverID", serverID ), target );
	}

	int x, y, gx, gy;
	input()->GetCursorPos(x, y);
	ipanel()->GetPos(surface()->GetEmbeddedPanel(), gx, gy);
	SetPos(x - gx, y - gy);
	SetVisible(true);
}
