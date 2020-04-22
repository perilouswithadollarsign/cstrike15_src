//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CL_ENTS_PARSE_H
#define CL_ENTS_PARSE_H
#ifdef _WIN32
#pragma once
#endif

class CEntityReadInfo;
class CSVCMsg_PacketEntities;

void CL_DeleteDLLEntity( int iEnt, char *reason, bool bOnRecreatingAllEntities = false );
void CL_CopyExistingEntity( CEntityReadInfo &u );
void CL_PreserveExistingEntity( int nOldEntity );
void CL_CopyNewEntity( CEntityReadInfo &u, int iClass, int iSerialNum );
void CL_PreprocessEntities( void );
bool CL_ProcessPacketEntities ( const CSVCMsg_PacketEntities &msg );


#endif // CL_ENTS_PARSE_H
