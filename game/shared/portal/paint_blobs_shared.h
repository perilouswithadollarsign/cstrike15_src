//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//==========================================================================//
#ifndef PAINT_BLOBS_SHARED_H
#define PAINT_BLOBS_SHARED_H

#include "paint_color_manager.h"
#include "trigger_tractorbeam_shared.h"

enum BlobTraceResult
{
	BLOB_TRACE_HIT_NOTHING = 0,
	BLOB_TRACE_HIT_PORTAL,
	BLOB_TRACE_HIT_WORLD,
	BLOB_TRACE_HIT_PAINT_CLEANSER,
	BLOB_TRACE_HIT_SOMETHING,
	BLOB_TRACE_HIT_PLAYER,
	BLOB_TRACE_HIT_TRACTORBEAM,
	BLOB_TRACE_HIT_PROP_PORTAL // this flag is for creating ghost blobs to make smooth blobs rendering when blobs go through portal
};

struct BlobCollisionRecord
{
	trace_t trace;
	Vector targetEndPos;
	BlobTraceResult traceResultType;
};

enum PaintBlobMoveState
{
	PAINT_BLOB_AIR_MOVE = 0,
	PAINT_BLOB_STREAK_MOVE,
	PAINT_BLOB_TRACTOR_BEAM_MOVE
};


class CBasePaintBlob
{
public:
	CBasePaintBlob( void );
	~CBasePaintBlob( void );

	void Init( const Vector &vecOrigin, const Vector &vecVelocity, int paintType, float flMaxStreakTime, float flStreakSpeedDampenRate, CBaseEntity* pOwner, bool bSilent, bool bDrawOnly );

	bool IsStreaking( void ) const;

	void SetTractorBeam( CTrigger_TractorBeam* pBeam );

	const Vector& GetTempEndPosition( void ) const;
	void SetTempEndPosition( const Vector &vecTempEndPosition );

	const Vector& GetTempEndVelocity( void ) const;
	void SetTempEndVelocity( const Vector &vecTempEndVelocity );

	const Vector& GetPosition( void ) const;
	void SetPosition( const Vector &vecPosition );

	const Vector& GetPrevPosition() const;
	void SetPrevPosition( const Vector& vPrevPosition );

	const Vector& GetVelocity( void ) const;
	void SetVelocity( const Vector &vecVelocity );

	const Vector& GetStreakDir() const;

	PaintPowerType GetPaintPowerType( void ) const;

	PaintBlobMoveState GetMoveState( void ) const;
	void SetMoveState( PaintBlobMoveState moveState );

	float GetAccumulatedTime() const { return m_flAccumulatedTime; }
	void SetAccumulatedTime( float flAccumulatedTime ) { m_flAccumulatedTime = flAccumulatedTime; }

	// kill this if we don't need fixed time step
	float GetLastUpdateTime() const { return m_flLastUpdateTime; }
	void SetLastUpdateTime( float flLastUpdateTime ) { m_flLastUpdateTime = flLastUpdateTime; }

	float GetVortexDirection() const;

	bool ShouldDeleteThis() const;
	void SetDeletionFlag( bool bDelete );

	float GetLifeTime() const;
	void UpdateLifeTime( float flUpdateTime );

	void UpdateBlobCollision( float flDeltaTime, const Vector& vecEndPos, Vector& vecEndVelocity );
	void UpdateBlobPostCollision( float flDeltaTime );

	const Vector& GetContactNormal() const;

	float GetStreakTime() const { return m_flStreakTimer; }
	float GetStreakSpeedDampenRate() const { return m_flStreakSpeedDampenRate; }

	void SetRadiusScale( float flRadiusScale );
	float GetRadiusScale( void ) const;

	bool ShouldPlayEffect() const;

	bool IsSilent() const { return m_bSilent; }

	bool IsGhosting() const { return ( m_hPortal != NULL ); }
	void GetGhostMatrix( VMatrix& matGhostTransform );
	void ResetGhostState() { m_hPortal = NULL; }

	CTrigger_TractorBeam* GetCurrentBeam() const;

	bool PaintBlobCheckShouldStreak( const trace_t &trace );

	void PlayEffect( const Vector& vPosition, const Vector& vNormal );

	bool PaintBlobStreakPaint( const Vector &vecBlobStartPos );

	virtual void PaintBlobPaint( const trace_t &tr ) = 0;

	void SetShouldPlaySound( bool shouldPlaySound );
	bool ShouldPlaySound() const;

	void SetBlobTeleportedThisFrame( bool bTeleported ) { m_bTeleportedThisFrame = bTeleported; }
	bool HasBlobTeleportedThisFrame() const { return m_bTeleportedThisFrame; }
	int GetTeleportationCount() const { return m_nTeleportationCount; }

	float m_flDestVortexRadius;
	float m_flCurrentVortexRadius;
	float m_flCurrentVortexSpeed;
	float m_flVortexDirection; // -1.f or 1.f

protected:
	void PaintBlobMoveThroughPortal( float flDeltaTime, CPortal_Base2D *pInPortal, const Vector &vecStartPos, const Vector &vecTransformedEndPos );

	BlobTraceResult BlobHitSolid( CBaseEntity* pHitEntity );
	int CheckCollision( BlobCollisionRecord *pCollisions, int maxCollisions, const Vector &vecEndPos );
	void CheckCollisionAgainstWorldAndStaticProps( BlobCollisionRecord& solidHitRecord, float& flHitFraction );
	int CheckCollisionThroughPortal( BlobCollisionRecord *pCollisions, int maxCollisions, const Vector &vecEndPos );
	void ResolveCollision( bool& bDeleted, const BlobCollisionRecord& collision, Vector& targetVelocity, float deltaTime );

	void DecayVortexSpeed( float flDeltaTime );

	Vector m_vecTempEndPosition;
	Vector m_vecTempEndVelocity;

	Vector m_vecPosition;
	Vector m_vecPrevPosition;
	Vector m_vecVelocity;

	// this is used for playing paint effect on client and painting surface on server
	Vector m_vContactNormal;

	PaintPowerType m_paintType;
	EHANDLE m_hOwner;
	PaintBlobMoveState m_MoveState;

	//Timers for the blob
	float m_flLifeTime;

	//Streaking
	Vector m_vecStreakDir;
	bool m_bStreakDirChanged;
	float m_flStreakTimer;
	float m_flStreakSpeedDampenRate;

	bool m_bDeleteFlag;

	float m_flAccumulatedTime;
	float m_flLastUpdateTime;

	//The radius scale of the blob for the isosurface rendering
	// this is shared so the listen server doesn't have to create blobs on the client dll
	float m_flRadiusScale;

	bool m_bShouldPlayEffect;

	// blob needs to know if hitting beam is the same as the current beam, so it doesn't need to init beam data again
	EntityBeamHistory_t m_beamHistory;
	bool m_bInTractorBeam;

	// HACK: remove this when awesome paint box/sphere feature is done (Bank)
	bool m_bSilent;

	// portal handle (for ghosting)
	EHANDLE m_hPortal;

	// optimize trace
	bool CheckCollisionBoxAgainstWorldAndStaticProps();
	Vector m_vCollisionBoxCenter;
	bool m_bCollisionBoxHitSolid;
	bool m_bShouldPlaySound;

	// if this flag is true, blob won't do effect/sound/paint
	bool m_bDrawOnly;

	// teleportation counter
	bool m_bTeleportedThisFrame;
	int m_nTeleportationCount;
};

#ifdef GAME_DLL
class CPaintBlob;
#else
class C_PaintBlob;
typedef C_PaintBlob CPaintBlob;
#endif

typedef CUtlVector<CPaintBlob*> PaintBlobVector_t;


//////////////////////////////////////////////////////////////////////////
//	Listen Server Shared Data
//////////////////////////////////////////////////////////////////////////
struct BlobTeleportationHistory_t
{
	BlobTeleportationHistory_t() : m_vEnterPosition( vec3_origin ), m_vExitPosition( vec3_origin ), m_flTeleportTime( 0.f )
	{
	}

	BlobTeleportationHistory_t( const VMatrix& matSourceToLinked, const VMatrix& matLinkedToSource, const Vector& vEnter, const Vector& vExit, float flTeleportTime )
	{
		m_matSourceToLinked = matSourceToLinked;
		m_matLinkedToSource = matLinkedToSource;

		m_vEnterPosition = vEnter;
		m_vExitPosition = vExit;
		m_flTeleportTime = flTeleportTime;
	}

	VMatrix m_matSourceToLinked;
	VMatrix m_matLinkedToSource;

	Vector m_vEnterPosition;
	Vector m_vExitPosition;
	float m_flTeleportTime;
};

typedef CUtlVector< BlobTeleportationHistory_t > BlobTeleportationHistoryVector_t;

struct BlobData_t
{
	BlobData_t()
	{
		m_blobID = -1;
		m_vPosition = vec3_origin;
		m_flScale = 0.f;
		m_bTeleportedThisFrame = false;
		m_bGhosting = false;
		MatrixSetIdentity( m_matGhostTransform );
	}

	BlobData_t( const BlobData_t& blobData )
	{
		m_blobID = blobData.m_blobID;
		m_vPosition = blobData.m_vPosition;
		m_flScale = blobData.m_flScale;
		m_bTeleportedThisFrame = blobData.m_bTeleportedThisFrame;
		m_bGhosting = blobData.m_bGhosting;
		m_matGhostTransform = blobData.m_matGhostTransform;
	}

	~BlobData_t()
	{
		m_teleportationHistory.Purge();
	}

	int m_blobID;
	Vector m_vPosition;
	float m_flScale;

	// teleportation data
	bool m_bTeleportedThisFrame;
	BlobTeleportationHistoryVector_t m_teleportationHistory;

	// ghosting data
	bool m_bGhosting;
	VMatrix m_matGhostTransform;
};

typedef CUtlVector< BlobData_t > BlobDataVector_t;

struct BlobInterpolationData_t
{
	BlobInterpolationData_t()
	{
		m_flUpdateTime = 0.f;
	}

	~BlobInterpolationData_t()
	{
		m_blobData.Purge();
	}

	float m_flUpdateTime;
	BlobDataVector_t m_blobData;
};

typedef CUtlVector< BlobInterpolationData_t > BlobInterpolationDataVector_t;

void PaintBlobUpdate( const PaintBlobVector_t& blobList );

#endif //PAINT_BLOBS_SHARED_H
