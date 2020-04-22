//===== Copyright (c), Valve Corporation, All rights reserved. ======//
#ifndef imaterialsystem_declarations_h
#define imaterialsystem_declarations_h

#ifndef IMPLEMENT_OPERATOR_EQUAL
// Define a reasonable operator=
#define IMPLEMENT_OPERATOR_EQUAL( _classname )			\
	public:												\
	_classname &operator=( const _classname &src )	\
		{												\
		memcpy( this, &src, sizeof(_classname) );	\
		return *this;								\
		}
#endif


// Sergiy: moved to _declarations.h
/** EAPS3 [ pWallace ]
* Moved to DmaPacketPcStubs.h:
*
*	- Standard vertex shader constants
*/

enum MaterialMatrixMode_t
{
	MATERIAL_VIEW = 0,
	MATERIAL_PROJECTION,

/*
	MATERIAL_MATRIX_UNUSED0,
	MATERIAL_MATRIX_UNUSED1,
	MATERIAL_MATRIX_UNUSED2,
	MATERIAL_MATRIX_UNUSED3,
	MATERIAL_MATRIX_UNUSED4,
	MATERIAL_MATRIX_UNUSED5,
	MATERIAL_MATRIX_UNUSED6,
	MATERIAL_MATRIX_UNUSED7,
*/
	// Texture matrices; used to be UNUSED
	MATERIAL_TEXTURE0,
	MATERIAL_TEXTURE1,
	MATERIAL_TEXTURE2,
	MATERIAL_TEXTURE3,
	MATERIAL_TEXTURE4,
	MATERIAL_TEXTURE5,
	MATERIAL_TEXTURE6,
	MATERIAL_TEXTURE7,

	MATERIAL_MODEL,

	// Total number of matrices
	NUM_MATRIX_MODES = MATERIAL_MODEL+1,

	// Number of texture transforms
	NUM_TEXTURE_TRANSFORMS = MATERIAL_TEXTURE7 - MATERIAL_TEXTURE0 + 1
};


/*******************************************************************************
* MaterialFogMode_t
*******************************************************************************/

enum MaterialFogMode_t
{
	MATERIAL_FOG_NONE,
	MATERIAL_FOG_LINEAR,
	MATERIAL_FOG_LINEAR_BELOW_FOG_Z,
};



//--------------------------------------------------------------------------------
// Uberlight parameters
//--------------------------------------------------------------------------------
struct UberlightState_t
{
	UberlightState_t()
	{
		m_fNearEdge 	= 2.0f;
		m_fFarEdge  	= 100.0f;
		m_fCutOn    	= 10.0f;
		m_fCutOff   	= 650.0f;
		m_fShearx   	= 0.0f;
		m_fSheary   	= 0.0f;
		m_fWidth    	= 0.3f;
		m_fWedge    	= 0.05f;
		m_fHeight		= 0.3f;
		m_fHedge		= 0.05f;
		m_fRoundness	= 0.8f;
	}

	float m_fNearEdge;
	float m_fFarEdge;
	float m_fCutOn;
	float m_fCutOff;
	float m_fShearx;
	float m_fSheary;
	float m_fWidth;
	float m_fWedge;
	float m_fHeight;
	float m_fHedge;
	float m_fRoundness;

	IMPLEMENT_OPERATOR_EQUAL( UberlightState_t );
};


class ITexture;
class IMaterial;

// fixme: should move this into something else.
struct FlashlightState_t
{
	FlashlightState_t()
	{
		m_bEnableShadows = false;						// Provide reasonable defaults for shadow depth mapping parameters
		m_bDrawShadowFrustum = false;
		m_flShadowMapResolution = 1024.0f;
		m_flShadowFilterSize = 3.0f;
		m_flShadowSlopeScaleDepthBias = 16.0f;
		m_flShadowDepthBias = 0.0005f;
		m_flShadowJitterSeed = 0.0f;
		m_flFlashlightTime = 0.0f;
		m_flShadowAtten = 0.0f;
		m_flAmbientOcclusion = 0.0f;
		m_bScissor = false; 
		m_nLeft = -1;
		m_nTop = -1;
		m_nRight = -1;
		m_nBottom = -1;
		m_nShadowQuality = 0;
		m_bShadowHighRes = false;
		m_bUberlight = false;
		m_bVolumetric = false;
		m_flNoiseStrength = 0.8f;
		m_nNumPlanes = 64;
		m_flPlaneOffset = 0.0f;
		m_flVolumetricIntensity = 1.0f;
		m_bOrtho = false;
		m_fOrthoLeft = -1.0f;
		m_fOrthoRight = 1.0f;
		m_fOrthoTop = -1.0f;
		m_fOrthoBottom = 1.0f;
		m_flProjectionSize = 500.0f;
		m_flProjectionRotation = 0.0f;
	}

	Vector m_vecLightOrigin;
	Quaternion m_quatOrientation;
	float m_NearZ;
	float m_FarZ;
	float m_fHorizontalFOVDegrees;
	float m_fVerticalFOVDegrees;
	bool  m_bOrtho;
	float m_fOrthoLeft;
	float m_fOrthoRight;
	float m_fOrthoTop;
	float m_fOrthoBottom;
	float m_fQuadraticAtten;
	float m_fLinearAtten;
	float m_fConstantAtten;
	float m_FarZAtten;
	float m_Color[4];
	ITexture *m_pSpotlightTexture;
	IMaterial *m_pProjectedMaterial;
	int m_nSpotlightTextureFrame;

	// Shadow depth mapping parameters
	bool  m_bEnableShadows;
	bool  m_bDrawShadowFrustum;
	float m_flShadowMapResolution;
	float m_flShadowFilterSize;
	float m_flShadowSlopeScaleDepthBias;
	float m_flShadowDepthBias;
	float m_flShadowJitterSeed;
	float m_flShadowAtten;
	float m_flAmbientOcclusion;
	int   m_nShadowQuality;
	bool  m_bShadowHighRes;

	// simple projection
	float m_flProjectionSize;
	float m_flProjectionRotation;

	// Uberlight parameters
	bool m_bUberlight;
	UberlightState_t m_uberlightState;

	bool m_bVolumetric;
	float m_flNoiseStrength;
	float m_flFlashlightTime;
	int m_nNumPlanes;
	float m_flPlaneOffset;
	float m_flVolumetricIntensity;

	// Getters for scissor members
	bool DoScissor() const { return m_bScissor; }
	int GetLeft()	 const { return m_nLeft; }
	int GetTop()	 const { return m_nTop; }
	int GetRight()	 const { return m_nRight; }
	int GetBottom()	 const { return m_nBottom; }

private:

	friend class CShadowMgr;

	bool m_bScissor; 
	int m_nLeft;
	int m_nTop;
	int m_nRight;
	int m_nBottom;
	IMPLEMENT_OPERATOR_EQUAL( FlashlightState_t ) ;
};


#endif