//================ Copyright (c) 1996-2009 Valve Corporation. All Rights Reserved. =================
//
// Maps can have any number of cordons. They can be toggled independently, but
// CMapDoc::m_bIsCordoning dictates whether culling against the active cordons is enabled or not.
//
//==================================================================================================

#ifndef CORDON_H
#define CORDON_H
#ifdef _WIN32
#pragma once
#endif

#include <utlvector.h>
#include <utlstring.h>
#include "boundbox.h"


#define DEFAULT_CORDON_NAME "cordon"


//
// Each cordon is a named collection of bounding boxes.
//
struct Cordon_t
{
	inline Cordon_t()
	{
		m_bActive = false;
	}

	CUtlString m_szName;
	bool m_bActive;					// True means cull using this cordon when cordoning is enabled.
	CUtlVector<BoundBox> m_Boxes;
};


#endif // CORDON_H
