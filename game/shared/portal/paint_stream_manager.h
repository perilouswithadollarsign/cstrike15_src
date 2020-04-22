//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//
#ifndef PAINT_STREAM_MANAGER_H
#define PAINT_STREAM_MANAGER_H

#include "paint_color_manager.h"
#include "mempool.h"

#ifdef CLIENT_DLL
#include "c_baseentity.h"
#include "c_paintblob.h"
#else
#include "baseentity.h"
#include "cpaintblob.h"
#endif

#ifdef CLIENT_DLL
class C_PaintStream;
#else
class CPaintStream;
#endif

class CBasePaintBlob;

//The blob impact effects. (Only on client)
struct PaintBlobImpactEffect_t
{
	Vector vecPosition;
	float flTime;
};

enum PaintImpactEffect
{
	PAINT_STREAM_IMPACT_EFFECT,
	PAINT_DRIP_IMPACT_EFFECT,

	PAINT_IMPACT_EFFECT_COUNT
};


typedef CUtlVector<PaintBlobImpactEffect_t> PaintBlobImpactEffectVector_t;
typedef float TimeStamp;
typedef CUtlVector<TimeStamp> TimeStampVector;
typedef CUtlVectorFixedGrowable<Vector, 32> PaintImpactPositionVector;

class CPaintStreamManager : public CAutoGameSystemPerFrame
{
public:
	CPaintStreamManager( char const *name );
	~CPaintStreamManager();

	//CAutoGameSystem members
	virtual char const *Name() { return "PaintStreamManager"; }
	virtual void LevelInitPreEntity();
	virtual void LevelInitPostEntity();
	virtual void LevelShutdownPostEntity();

#ifdef CLIENT_DLL
	virtual void Update( float frametime );
#else
	virtual void PreClientUpdate( void );
#endif

	void RemoveAllPaintBlobs( void );

	const char *GetPaintMaterialName( int type );

	void CreatePaintImpactParticles( const Vector &vecPosition, const Vector &vecNormal, int paintType );
	float PlayPaintImpactSound( const Vector &vecPosition, PaintImpactEffect impactEffect );
	void PlayMultiplePaintImpactSounds( TimeStampVector& channelTimeStamps, int maxChannels, const PaintImpactPositionVector& positions, PaintImpactEffect impactEffect );

	void QueuePaintImpactEffect( const Vector &vecPosition, const Vector &vecNormal, int paintType, PaintImpactEffect impactEffect );

	void AllocatePaintBlobPool( int nMaxBlobs );
	CPaintBlob* AllocatePaintBlob( bool bSilent = false );
	void FreeBlob( CPaintBlob* pBlob );

private:

	void PaintStreamUpdate();

	void UpdatePaintImpactEffects( float flDeltaTime, PaintBlobImpactEffectVector_t &paintImpactEffects );
	bool ShouldPlayImpactEffect( const Vector& vecPosition, PaintBlobImpactEffectVector_t &paintImpactEffects, int minEffects, int maxEffects, float flDistanceThreshold );
	void PlayPaintImpactParticles( const Vector &vecPosition, const Vector &vecNormal, int paintType );
	PaintBlobImpactEffectVector_t m_PaintImpactParticles;

	static const char* const m_pPaintMaterialNames[PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER];

	float PlayPaintImpactSound( const EmitSound_t& emitParams );
	static const char* GetPaintSoundEffectName( unsigned int impactEffect );
	static const char* const s_SoundEffectNames[PAINT_IMPACT_EFFECT_COUNT];
	CClassMemoryPool< CPaintBlob > *m_pBlobPool;
};

extern CPaintStreamManager PaintStreamManager;

#endif //PAINT_STREAM_MANAGER_H
