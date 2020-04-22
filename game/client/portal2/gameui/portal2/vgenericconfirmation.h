//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VGENERICCONFIRMATION_H__
#define __VGENERICCONFIRMATION_H__

#include "vgui_controls/CvarToggleCheckButton.h"
#include "gameui_util.h"

#include "basemodui.h"
#include "vfooterpanel.h"

namespace BaseModUI {

class GenericConfirmation : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( GenericConfirmation, CBaseModFrame );

public:
	GenericConfirmation(vgui::Panel *parent, const char *panelName);
	~GenericConfirmation();

	typedef void (*Callback_t)(void);

	struct Data_t
	{
		Data_t();

		const char		*pWindowTitle;
		const char		*pMessageText;
		const wchar_t	*pMessageTextW;
		
		bool			bOkButtonEnabled;
		Callback_t		pfnOkCallback;
		const char		*pOkButtonText;

		bool			bCancelButtonEnabled;
		Callback_t		pfnCancelCallback;
		const char		*pCancelButtonText;

		bool			bCheckBoxEnabled;
		const char		*pCheckBoxLabelText;
		const char		*pCheckBoxCvarName;
	};

	int  SetUsageData( const Data_t & data );     // returns the usageId, different number each time this is called
	int  GetUsageId() const { return m_usageId; }

	const Data_t & GetUsageData() const { return m_data; }

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnCommand(const char *command);
	virtual void OnKeyCodePressed(vgui::KeyCode code);
#ifndef _GAMECONSOLE
	virtual void OnKeyCodeTyped( vgui::KeyCode code );
#endif
	virtual void OnOpen();
	virtual void PaintBackground();

	vgui::CvarToggleCheckButton<CGameUIConVarRef> *m_pCheckBox;

private:
	void FixLayout();
	void UpdateFooter();

	static int		sm_currentUsageId;

	vgui::Label		*m_pLblMessage;
	vgui::Label		*m_pLblCheckBox;

	Data_t			m_data;
	int				m_usageId;

	bool			m_bNeedsMoveToFront;
	bool			m_bValid;

	vgui::HFont		m_hMessageFont;
	int				m_nTextOffsetX;
	int				m_nIconOffsetY;
};

};

#endif
