//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
	   
#ifndef KEYS_H
#define KEYS_H

#ifdef _WIN32
#pragma once
#endif

#include "inputsystem/ButtonCode.h"
#include "cdll_int.h"

class CUtlBuffer;
struct InputEvent_t;


void		Key_Event( const InputEvent_t &event );

void		Key_Init( void );
void		Key_Shutdown( void );
void		Key_WriteBindings( CUtlBuffer &buf, const int iSplitscreenSlot = -1 );
int			Key_CountBindings( void );
void		Key_SetBinding( ButtonCode_t code, const char *pBinding );
const char	*Key_BindingForKey( ButtonCode_t code );
const char	*Key_NameForBinding( const char *pBinding , int userId = -1, int iStartCount = 0, BindingLookupOption_t nFlags = BINDINGLOOKUP_ALL );
int			Key_CodeForBinding( const char *pBinding , int userId = -1, int iStartCount = 0, BindingLookupOption_t nFlags = BINDINGLOOKUP_ALL );

void		Key_StartTrapMode( void );
bool		Key_CheckDoneTrapping( ButtonCode_t& key );


#endif // KEYS_H
