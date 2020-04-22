//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#if !defined( BEAMSEGDRAW_H )
#define BEAMSEGDRAW_H
#ifdef _WIN32
#pragma once
#endif

#define NOISE_DIVISIONS		128


#include "mathlib/vector.h"
#include "materialsystem/imesh.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct BeamTrail_t;
class IMaterial;


//-----------------------------------------------------------------------------
// CBeamSegDraw is a simple interface to beam rendering.
//-----------------------------------------------------------------------------
struct BeamSeg_t
{
	VectorAligned	m_vPos;
	color32			m_color;
	float			m_flTexCoord;	// Y texture coordinate
	float			m_flWidth;

	void SetColor( float r, float g, float b, float a )
	{
		// Specify the points.
		Assert( IsFinite(r) && IsFinite(g) && IsFinite(b) && IsFinite(a) );
		Assert( (r >= 0.0) && (g >= 0.0) && (b >= 0.0) && (a >= 0.0) );
		Assert( (r <= 1.0) && (g <= 1.0) && (b <= 1.0) && (a <= 1.0) );

		m_color.r = FastFToC( r );
		m_color.g = FastFToC( g );
		m_color.b = FastFToC( b );
		m_color.a = FastFToC( a );
	}

	void SetColor( float r, float g, float b )
	{
		// Specify the points.
		Assert( IsFinite(r) && IsFinite(g) && IsFinite(b) );
		Assert( (r >= 0.0) && (g >= 0.0) && (b >= 0.0) );
		Assert( (r <= 1.0) && (g <= 1.0) && (b <= 1.0) );

		m_color.r = FastFToC( r );
		m_color.g = FastFToC( g );
		m_color.b = FastFToC( b );
	}

	void SetAlpha( float a )
	{
		// Specify the points.
		Assert( IsFinite(a) );
		Assert( (a >= 0.0) );
		Assert( (a <= 1.0) );

		m_color.a = FastFToC( a );
	}

	void SetColor( const Vector &vecColor, float a )
	{
		SetColor( vecColor.x, vecColor.y, vecColor.z, a );
	}

	void SetColor( const Vector4D &vecColor )
	{
		SetColor( vecColor.x, vecColor.y, vecColor.z, vecColor.w );
	}

	void SetColor( const Vector &vecColor )
	{
		SetColor( vecColor.x, vecColor.y, vecColor.z );
	}

	void GetColor( Vector4D *pColor )
	{
		pColor->x = m_color.r / 255.0f;
		pColor->y = m_color.g / 255.0f;
		pColor->z = m_color.b / 255.0f;
		pColor->w = m_color.a / 255.0f;
	}

	void GetColor( Vector *pColor )
	{
		pColor->x = m_color.r / 255.0f;
		pColor->y = m_color.g / 255.0f;
		pColor->z = m_color.b / 255.0f;
	}
};

struct BeamSegRenderInfo_t
{
	Vector	m_vecPoint1;
	Vector	m_vecPoint2;
	Vector	m_vecCenter;
	Vector	m_vecTangentS;
	Vector	m_vecTangentT;
	float	m_flTexCoord;
	color32	m_color;
};

class CBeamSegDraw
{
public:
	CBeamSegDraw() : m_pRenderContext( NULL ) {}
	// Pass null for pMaterial if you have already set the material you want.
	void			Start( IMatRenderContext *pRenderContext, int nSegs, IMaterial *pMaterial=0, CMeshBuilder *pMeshBuilder = NULL, int nMeshVertCount = 0 );

	void			ComputeRenderInfo( BeamSegRenderInfo_t *pRenderInfo, const Vector &vecCameraPos, int nSegCount, const BeamSeg_t *pSegs ) RESTRICT;
	virtual void	NextSeg( BeamSeg_t *pSeg );
	void			End();

protected:
	void			SpecifySeg( const Vector &vecCameraPos, const Vector &vNextPos );
	void			ComputeNormal( const Vector &vecCameraPos, const Vector &vStartPos, const Vector &vNextPos, Vector *pNormal );
	static void		LoadSIMDData( FourVectors *pV4StartPos, FourVectors *pV4EndPos, FourVectors *pV4HalfWidth, int nSegCount, const BeamSeg_t *pSegs );
	CMeshBuilder	*m_pMeshBuilder;
	int				m_nMeshVertCount;

	CMeshBuilder	m_Mesh;
	BeamSeg_t		m_Seg;	

	int				m_nTotalSegs;
	int				m_nSegsDrawn;

	Vector			m_vNormalLast;
	IMatRenderContext *m_pRenderContext;

	Vector			m_vecCameraPos;
};

class CBeamSegDrawArbitrary : public CBeamSegDraw
{
public:
	void			SetNormal( const Vector &normal );
	void			NextSeg( BeamSeg_t *pSeg );

protected:
	void			SpecifySeg( const Vector &vNextPos );

	BeamSeg_t		m_PrevSeg;
};

#if 0
int ScreenTransform( const Vector& point, Vector& screen );

void DrawSegs( int noise_divisions, float *prgNoise, const model_t* spritemodel,
				float frame, int rendermode, const Vector& source, const Vector& delta, 
				float startWidth, float endWidth, float scale, float freq, float speed, int segments,
				int flags, float* color, float fadeLength, float flHDRColorScale = 1.0f );
void DrawTeslaSegs( int noise_divisions, float *prgNoise, const model_t* spritemodel,
				float frame, int rendermode, const Vector& source, const Vector& delta, 
				float startWidth, float endWidth, float scale, float freq, float speed, int segments,
				int flags, float* color, float fadeLength, float flHDRColorScale = 1.0f );
void DrawSplineSegs( int noise_divisions, float *prgNoise, 
				const model_t* beammodel, const model_t* halomodel, float flHaloScale,
				float frame, int rendermode, int numAttachments, Vector* attachment, 
				float startWidth, float endWidth, float scale, float freq, float speed, int segments,
				int flags, float* color, float fadeLength, float flHDRColorScale = 1.0f );
void DrawHalo(IMaterial* pMaterial, const Vector& source, float scale, float const* color, float flHDRColorScale = 1.0f );
void BeamDrawHalo( const model_t* spritemodel, float frame, int rendermode, const Vector& source, 
				  float scale, float* color, float flHDRColorScale = 1.0f );
void DrawDisk( int noise_divisions, float *prgNoise, const model_t* spritemodel,
			  float frame, int rendermode, const Vector& source, const Vector& delta, 
			  float width, float scale, float freq, float speed, 
			  int segments, float* color, float flHDRColorScale = 1.0f );
void DrawCylinder( int noise_divisions, float *prgNoise, const model_t* spritemodel, 
				  float frame, int rendermode, const Vector& source, 
				  const Vector&  delta, float width, float scale, float freq, 
				  float speed, int segments, float* color, float flHDRColorScale = 1.0f );
void DrawRing( int noise_divisions, float *prgNoise, void (*pfnNoise)( float *noise, int divs, float scale ), 
			  const model_t* spritemodel, float frame, int rendermode, 
			  const Vector& source, const Vector& delta, float width, float amplitude, 
			  float freq, float speed, int segments, float* color, float flHDRColorScale = 1.0f );
void DrawBeamFollow( const model_t* spritemodel, BeamTrail_t* pHead, int frame, int rendermode, Vector& delta, 
					Vector& screen, Vector& screenLast, float die, const Vector& source, 
					int flags, float width, float amplitude, float freq, float* color, float flHDRColorScale = 1.0f );

void DrawBeamQuadratic( const Vector &start, const Vector &control, const Vector &end, float width, const Vector &color, float scrollOffset, float flHDRColorScale = 1.0f );
#endif

//-----------------------------------------------------------------------------
// Assumes the material has already been bound
//-----------------------------------------------------------------------------
void DrawSprite( const Vector &vecOrigin, float flWidth, float flHeight, color32 color );

#endif // BEAMDRAW_H
