//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//
#ifndef PAINT_SPRAYER_SHARED_H
#define PAINT_SPRAYER_SHARED_H

#include "paint_blobs_shared.h"

CPaintBlob* FirePaintBlob( const Vector& vecSourcePosition,
					  const Vector& vecOldSourcePosition,
					  const Vector& vecSourceVelocity,
					  const Vector& vecSprayDir,
					  int paintType,
					  float flBlobSpreadRadius,
					  float flBlobSpreadAngle,
					  float flMinSpeed,
					  float flMaxSpeed,
					  float flBlobStreakPercent,
					  float flMinStreakTime,
					  float flMaxStreakTime,
					  float flMinStreakSpeedDampen,
					  float flMaxStreakSpeedDampen,
					  bool bSilent,
					  bool bDrawOnly,
					  CBaseEntity *pOwner,
					  int nRandomSeed = 0 );

enum BlobRenderMode_t
{
	BLOB_RENDER_BLOBULATOR = 0,
	BLOB_RENDER_FAST_SPHERE,
	TOTAL_BLOB_RENDER_TYPE
};

#endif //PAINT_SPRAYER_SHARED_H
