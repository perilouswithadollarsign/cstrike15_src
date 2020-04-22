//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TABLET_H
#define TABLET_H

#ifdef _WIN32
#pragma once
#endif

bool	WinTab_Opened( );
bool	WinTab_Init( );
void	WinTab_Open( HWND hWnd );
void	WinTab_Packet( WPARAM wSerial, LPARAM hCtx );
float	WinTab_GetPressure( );

#endif // TABLET_H