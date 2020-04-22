//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef SFMRIGUTILS_H
#define SFMRIGUTILS_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/dmerigconstraintoperators.h"
#include "movieobjects/dmechannel.h"
#include "dmeutils/dmanimutils.h"

class CDmeAnimationSet;
class CDmeFilmClip;
class CDmeClip;
class CDmeChannelsClip;
class CDmeRigHandle;
class CDmeRig;
class CDmeTransformControl;
class CSFMAnimSetScriptContext;
class CStudioHdr;


void SpewChannel( CDmeChannel *ch );
int FindBoneIndex( CStudioHdr const* pStudioHdr, const char* pName );


class CSFMRigUtils
{

public:

	// Create a rig handle with the specified name with the specified position and orientation
	static CDmeRigHandle *CreateRigHandle( const char *pchName, const Vector &position, const Quaternion &orientation, CDmeFilmClip *pShot );

	// Destroy the specified rig handle and remove it from the animation set.
	static void DestroyRigHandle( CDmeRigHandle *pRigHandle, CDmeFilmClip *pShot, CSFMAnimSetScriptContext *pAnimSetContext = NULL );

	// Generate logs of all dag nodes controlled by the rig and then destroy all elements of the rig.
	static void DetachRig( CDmeRig *pRig, CDmeFilmClip *pShot );

	// Detach all rigs and constraints from the animation set
	static void DetachAllRigs( CDmeAnimationSet *pAnimSet );

	// Determine if the specified animation set has any rig components (rig handles, constraints, display sets)
	static bool HasRigComponents( CDmeAnimationSet *pAnimSet );

};

#endif // SFMRIGUTILS_H
