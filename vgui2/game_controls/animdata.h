//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ANIMDATA_H
#define ANIMDATA_H

#ifdef _WIN32
#pragma once
#endif


#include "typedlog.h"
#include "tier1/utlstring.h"
#include "dmxloader/dmxelement.h"

class CAnimData
{
	DECLARE_DMXELEMENT_UNPACK()

public:
	CAnimData();
	~CAnimData();

	bool Unserialize( CDmxElement *pElement );

	bool IsDone( DmeTime_t time );

	CUtlString m_pStateName;
	CUtlString m_pAnimAlias;
	int m_TextureAnimSheetSeqNumber;
	float m_AnimationRate;

	CTypedLog< color32 > m_ColorAnim;
	CTypedLog< Vector2D > m_CenterPosAnim;
	CTypedLog< Vector2D > m_ScaleAnim;
	CTypedLog< float > m_RotationAnim;
	CTypedLog< CUtlString > m_FontAnim;
};



#endif // ANIMDATA_H
