//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef BONELIST_H
#define BONELIST_H
#ifdef _WIN32
#pragma once
#endif

#include "studio.h"

class CBoneList
{
public:

	CBoneList();

	void Release();

	static CBoneList *Alloc();

	unsigned int GetWriteSize() const
	{
		return 2 + m_nBones * ( sizeof( Vector ) + sizeof( Quaternion ) );
	}

	// The order of these data members must be maintained in order for the server
	// demo system.  ServerDemoPacket_BaseAnimating::GetSize() depends on this.

private:
	bool		m_bShouldDelete : 1;

public:
	uint16		m_nBones : 15;
	Vector		m_vecPos[ MAXSTUDIOBONES ];
	Quaternion	m_quatRot[ MAXSTUDIOBONES ];
};

class CFlexList
{
public:

	CFlexList();

	void Release();

	static CFlexList *Alloc();

public:

	int			m_nNumFlexes;
	float		m_flexWeights[ MAXSTUDIOFLEXCTRL ];

private:
	bool		m_bShouldDelete;
};

#endif // BONELIST_H
