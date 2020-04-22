//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ========//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef BLACKBOX_H
#define BLACKBOX_H 1

#include "engine/iblackbox.h"

extern IBlackBox *gBlackBox;

// helper function
void BlackBox_Record(const char *type, const char *pFormat, ...);

#endif