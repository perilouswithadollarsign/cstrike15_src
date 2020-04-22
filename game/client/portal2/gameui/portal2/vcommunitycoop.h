//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VCOMMUNITYCOOP_H__
#define __VCOMMUNITYCOOP_H__

#include "basemodui.h"

#if defined( PORTAL2_PUZZLEMAKER )

using namespace vgui;
using namespace BaseModUI;


namespace BaseModUI
{
	class CCommunityCoop : public CBaseModFrame
	{
		DECLARE_CLASS_SIMPLE( CCommunityCoop, CBaseModFrame );

	public:
		CCommunityCoop( vgui::Panel *pParent, const char *pPanelName );
		~CCommunityCoop( void );

	protected:
		virtual void Activate( void ) OVERRIDE;
		virtual void ApplySchemeSettings( vgui::IScheme* pScheme ) OVERRIDE;
		virtual void OnCommand( char const *pszCommand ) OVERRIDE;
		virtual void OnKeyCodePressed( vgui::KeyCode code ) OVERRIDE;

	private:
		void UpdateFooter( void );
		void CloseUI( void );
		void StartPlayingCoop( void );

		void CheckHasMapMsgReceived( PublishedFileId_t fileID );
	};
}

#endif // PORTAL2_PUZZLEMAKER

#endif // __VCOMMUNITYCOOP_H__