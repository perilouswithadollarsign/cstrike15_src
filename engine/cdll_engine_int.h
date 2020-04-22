//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CDLL_ENGINE_INT_H
#define CDLL_ENGINE_INT_H
#ifdef _WIN32
#pragma once
#endif


#include "cdll_int.h"

class IVModelRender;
class IClientLeafSystemEngine;
class ClientClass;

bool ClientDLL_Load( void );
void ClientDLL_Unload ( void );
void ClientDLL_Init( void );
void ClientDLL_Connect( void );
void ClientDLL_Disconnect();
void ClientDLL_Shutdown( void );
void ClientDLL_GameInit();
void ClientDLL_GameShutdown();
void ClientDLL_HudVidInit( void );
void ClientDLL_ProcessInput( void );
void ClientDLL_Update( void );
void ClientDLL_VoiceStatus( int entindex, int iSsSlot, bool bTalking );
// returns false if the player can't hear the other client due to game rules (eg. the other team)
bool ClientDLL_IsPlayerAudible( int iPlayerIndex );
// Returns the index of the entity the local player is spectating, if any, otherwise returns -1
int  ClientDLL_GetSpectatorTarget( ClientDLLObserverMode_t *pObserverMode );
void ClientDLL_FrameStageNotify( ClientFrameStage_t frameStage );
ClientClass *ClientDLL_GetAllClasses( void );
CreateInterfaceFn ClientDLL_GetFactory( void );
void ClientDLL_OnActiveSplitscreenPlayerChanged( int slot );
void ClientDLL_OnSplitScreenStateChanged();
vgui::VPANEL ClientDLL_GetFullscreenClientDLLVPanel( void );

//-----------------------------------------------------------------------------
// slow routine to draw a physics model
//-----------------------------------------------------------------------------
void DebugDrawPhysCollide( const CPhysCollide *pCollide, IMaterial *pMaterial, const matrix3x4_t& transform, const color32 &color, bool drawAxes );

#ifndef DEDICATED
extern IBaseClientDLL *g_ClientDLL;
#endif

extern IVModelRender* modelrender;
extern ClientClass *g_pClientClasses;
extern IClientLeafSystemEngine* clientleafsystem;
extern bool scr_drawloading;

#endif // CDLL_ENGINE_INT_H
