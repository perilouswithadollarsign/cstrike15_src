//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: TF2 specific input handling
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "kbutton.h"
#include "input.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: TF Input interface
//-----------------------------------------------------------------------------
class CCSInput : public CInput
{
public:
};

static CCSInput g_Input;

// Expose this interface
IInput *input = ( IInput * )&g_Input;

