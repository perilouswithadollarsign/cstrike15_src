//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VCOOPMODE_H__
#define __VCOOPMODE_H__

#include "basemodui.h"

namespace BaseModUI {

	class CCoopMode : public CBaseModFrame
	{
		DECLARE_CLASS_SIMPLE( CCoopMode, CBaseModFrame );

	public:
		CCoopMode( vgui::Panel *pParent, const char *pPanelName );
		~CCoopMode();

		static void ConfirmChallengeMode_Callback();

	protected:
		virtual void OnCommand( char const *szCommand );
		virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
		virtual void Activate();
		virtual void OnKeyCodePressed( vgui::KeyCode code );

	private:
		void	UpdateFooter();

	};

};

#endif
