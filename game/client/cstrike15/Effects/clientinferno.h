// ClientInferno.cpp
// Render client-side Inferno effects
// Author: Michael Booth, February 2005
// Copyright (c) 2005 Turtle Rock Studios, Inc. - All Rights Reserved


#include "cbase.h"
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------------------------
/**
 * The client-side Inferno effect
 */
class C_Inferno : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_Inferno, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_Inferno();
	virtual ~C_Inferno();

	virtual void Spawn( void );
	virtual void ClientThink();

	virtual int DrawModel( int flags, const RenderableInstance_t &instance );
	virtual bool ShouldDraw()				{ return true; }
	virtual RenderableTranslucencyType_t ComputeTranslucencyType() { return RENDERABLE_IS_TRANSLUCENT; }

	virtual void GetRenderBounds( Vector& mins, Vector& maxs );

	// returns the bounds as an AABB in worldspace
	virtual void GetRenderBoundsWorldspace( Vector& mins, Vector& maxs );

	virtual void OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect );
	virtual void OnParticleEffectDeleted( CNewParticleEffect *pParticleEffect );

	virtual const char* GetParticleEffectName();

private:
	void UpdateParticles( void );
	CUtlReference<CNewParticleEffect> m_burnParticleEffect;

	enum { MAX_INFERNO_FIRES = 100 };
	int m_fireXDelta[ MAX_INFERNO_FIRES ];
	int m_fireYDelta[ MAX_INFERNO_FIRES ];
	int m_fireZDelta[ MAX_INFERNO_FIRES ];
	bool m_bFireIsBurning[ MAX_INFERNO_FIRES ];
	Vector m_BurnNormal[ MAX_INFERNO_FIRES ];
	int m_fireUniqueID[ MAX_INFERNO_FIRES ];
	int m_fireCount;
	int m_nInfernoType;
	int m_lastFireCount;						///< used to detect changes
	
	// these are the actual visible fires
	enum FireState { STARTING, BURNING, GOING_OUT, FIRE_OUT, UNKNOWN };
	struct Drawable
	{
		Vector m_pos;							///< position of flame
		Vector m_normal;						///< normal of flame surface
		int m_frame;							///< current animation frame
		float m_framerate;						///< rate of animation
		bool m_mirror;							///< if true, flame is mirrored about vertical axis

		int m_dlightIndex;

		FireState m_state;						///< the state of this fire
		float m_stateTimestamp;					///< when the fire entered its current state
		void SetState( FireState state )
		{
			m_state = state;
			m_stateTimestamp = gpGlobals->realtime;
		}

		float m_size;							///< current flame size
		float m_maxSize;						///< maximum size of full-grown flame

		void Draw( void );						///< render this flame
	};
	Drawable m_drawable[ MAX_INFERNO_FIRES ];
	int m_drawableCount;

	void SynchronizeDrawables( void );					///< compare m_fireX etc to m_drawable and update states
	Drawable *GetDrawable( const Vector &pos );	///< given a position, return the fire there

	float m_maxFireHalfWidth;
	float m_maxFireHeight;
	Vector m_minBounds, m_maxBounds;

	void DrawFire( C_Inferno::Drawable *fire, IMesh *mesh );	///< render an individual flame
	void RecomputeBounds( void );
};

//---------------------------------------------------------
//---------------------------------------------------------
class C_FireCrackerBlast: public C_Inferno
{
public:
	DECLARE_CLASS( C_FireCrackerBlast, C_Inferno );
	DECLARE_CLIENTCLASS();

	virtual const char* GetParticleEffectName();
};
