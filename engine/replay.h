//--------- Copyright (c) 1996-2009, Valve Corporation, All rights reserved. -------------
//
//----------------------------------------------------------------------------------------

#ifndef REPLAY_H
#define REPLAY_H
#ifdef _WIN32
#pragma once
#endif

//----------------------------------------------------------------------------------------
// Is replay enabled?
//----------------------------------------------------------------------------------------
bool Replay_IsEnabled();

//----------------------------------------------------------------------------------------
// Called from CNetChan when a file has been completely sent down to the client.
//----------------------------------------------------------------------------------------
void Replay_OnFileSendComplete( const char *pFilename, int nSize );

#endif // REPLAY_H
