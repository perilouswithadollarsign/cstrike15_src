//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MM_TITLE_RICHPRESENCE_H
#define MM_TITLE_RICHPRESENCE_H
#ifdef _WIN32
#pragma once
#endif

#include "mm_title.h"

void MM_Title_RichPresence_Update( KeyValues *pFullSettings, KeyValues *pUpdatedSettings );
void MM_Title_RichPresence_PlayersChanged( KeyValues *pFullSettings );

KeyValues * MM_Title_RichPresence_PrepareForSessionCreate( KeyValues *pSettings );

extern ContextValue_t g_pcv_CONTEXT_ACCESS[];
extern ContextValue_t g_pcv_CONTEXT_GAME_MODE[];
extern ContextValue_t g_pcv_CONTEXT_STATE[];
extern ContextValue_t g_pcv_CONTEXT_DIFFICULTY[];

#endif // MM_TITLE_H
