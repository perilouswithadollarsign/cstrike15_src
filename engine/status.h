//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//=============================================================================//
#if !defined( STATUS_H )
#define STATUS_H
#ifdef _WIN32
#pragma once
#endif

void Status_Update();
void Status_CheckSendETWMark();
const char *Status_GetBuffer();

#endif // STATUS_H
