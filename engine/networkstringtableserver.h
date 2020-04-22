//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef NETWORKSTRINGTABLESERVER_H
#define NETWORKSTRINGTABLESERVER_H
#ifdef _WIN32
#pragma once
#endif

void SV_CreateNetworkStringTables( char const *pchMapName );

void SV_PrintStringTables( void );
void SV_CreateDictionary( char const *pchMapName );

#endif // NETWORKSTRINGTABLESERVER_H
