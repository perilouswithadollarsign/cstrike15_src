//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =====//
//
// Dme version of a hitbox
//
//===========================================================================//

#ifndef DMEHITBOX_H
#define DMEHITBOX_H

#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "mdlobjects/dmebbox.h"


//-----------------------------------------------------------------------------
// A class representing an attachment point
//-----------------------------------------------------------------------------
class CDmeHitbox : public CDmeBBox
{
	DEFINE_ELEMENT( CDmeHitbox, CDmeBBox );

public:
	virtual void Draw( const matrix3x4_t &shapeToWorld, CDmeDrawSettings *pDrawSettings = NULL );

	CDmaString m_sSurfaceProperty;
	CDmaVar< int > m_nGroupId;
	CDmaString m_sBoneName;
	CDmaColor m_cRenderColor;	// used for visualization

};


#endif // DMEHITBOX_H
