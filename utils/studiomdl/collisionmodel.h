//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
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
//===========================================================================//

#ifndef COLLISIONMODEL_H
#define COLLISIONMODEL_H

#ifdef _WIN32
#pragma once
#endif

class CDmElement;
struct s_source_t;

void Cmd_CollisionText( void );
int DoCollisionModel( bool separateJoints );

#ifdef MDLCOMPILE
int DoCollisionModel( s_source_t *pSource, CDmElement *pInfo, bool bStaticProp );
void LoadCollisionText( const char *pszCollisionText );
#endif MDLCOMPILE

// execute after simplification, before writing
void CollisionModel_Build( void );
// execute during writing
extern void CollisionModel_Write( long checkSum );
extern void CollisionModel_SetName( const char *pName );

void CollisionModel_ExpandBBox( Vector &mins, Vector &maxs );

#endif // COLLISIONMODEL_H
