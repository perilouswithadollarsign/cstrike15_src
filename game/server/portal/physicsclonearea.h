//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PHYSICSCLONEAREA_H
#define PHYSICSCLONEAREA_H

#ifdef _WIN32
#pragma once
#endif

#include "baseentity.h"

class CPortal_Base2D;
class CPortalSimulator;


class CPhysicsCloneArea : public CBaseEntity
{
public:
	DECLARE_CLASS( CPhysicsCloneArea, CBaseEntity );

	//static const Vector		vLocalMins;
	//static const Vector		vLocalMaxs;

	virtual void			StartTouch( CBaseEntity *pOther );
	virtual void			Touch( CBaseEntity *pOther ); 
	virtual void			EndTouch( CBaseEntity *pOther );

	virtual void			Spawn( void );
	virtual void			Activate( void );

	virtual int				ObjectCaps( void );
	void					UpdatePosition( void );

	void					CloneTouchingEntities( void );
	void					CloneNearbyEntities( void );
	static CPhysicsCloneArea *CreatePhysicsCloneArea( CPortal_Base2D *pFollowPortal );	

	inline Vector			GetLocalMins( void ) const { return Vector( 3.0f, -m_fHalfWidth, -m_fHalfHeight ); }
	inline Vector			GetLocalMaxs( void ) const { return Vector( m_fHalfDepth, m_fHalfWidth, m_fHalfHeight ); }

	void					Resize( float fPortalHalfWidth, float fPortalHalfHeight );
private:
	
	CPortal_Base2D			*m_pAttachedPortal;
	CPortalSimulator		*m_pAttachedSimulator;
	bool					m_bActive;

	float					m_fHalfWidth, m_fHalfHeight, m_fHalfDepth;
	static const float		s_fPhysicsCloneAreaScale;


};

#endif //#ifndef PHYSICSCLONEAREA_H

