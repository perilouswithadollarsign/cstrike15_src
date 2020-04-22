//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MESSAGEBOXWITHCHECKBOX_H
#define MESSAGEBOXWITHCHECKBOX_H
#ifdef _WIN32
#pragma once
#endif

#include "basedialogparams.h"

struct CMessageBoxWithCheckBoxParams : public CBaseDialogParams
{
	CMessageBoxWithCheckBoxParams()
	{
		m_szPrompt[ 0 ] = 0;
		m_szCheckBoxText[ 0 ] = 0;
		m_bChecked = false;
	}

	char		m_szPrompt[ 256 ];
	char		m_szCheckBoxText[ 1024 ];
	bool		m_bChecked;
};

// Display/create dialog
int MessageBoxWithCheckBox( CMessageBoxWithCheckBoxParams *params );

#endif // MESSAGEBOXWITHCHECKBOX_H
