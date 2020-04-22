//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#if !defined( PROFILE_H )
#define PROFILE_H
#ifdef _WIN32
#pragma once
#endif

void Host_ResetGlobalConfiguration();
void Host_ResetConfiguration( const int iController );
void Host_ReadConfiguration( const int iController, const bool readDefault );
void Host_WriteConfiguration( const int iController, const char *filename );

void Host_SubscribeForProfileEvents( bool bSubscribe );

#endif // PROFILE_H
