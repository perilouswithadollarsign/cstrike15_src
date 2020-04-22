//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           ViewerSettings.h
// last modified:  May 29 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
// version:        1.2
//
// email:          mete@swissquake.ch
// web:            http://www.swissquake.ch/chumbalum-soft/
//
#ifndef INCLUDED_VIEWERSETTINGS
#define INCLUDED_VIEWERSETTINGS

#include "mathlib/vector.h"
#include "rubikon/param_types.h"
#include "utlvector.h"

enum // render modes
{
	RM_WIREFRAME = 0,
//	RM_FLATSHADED,
	RM_SMOOTHSHADED,
	RM_TEXTURED,
	RM_BONEWEIGHTS,
	RM_SHOWBADVERTEXDATA,
	RM_TEXCOORDS,
	RM_SHOWCOLOCATED,
};

enum //draw model pass modes. Useful for rendering hitboxes on top of submodels, for example
{
	PASS_DEFAULT = 0,	// draw geometry, hitboxes, bones, etc. Any selected options
	PASS_MODELONLY,		// draw only geometry, skip rendering hitboxes and so on
	PASS_EXTRASONLY		// draw only extras
};

#define HLMV_MAX_MERGED_MODELS 12

enum HitboxEditMode
{
	HITBOX_EDIT_ROTATION = 0,
	HITBOX_EDIT_BBMIN,
	HITBOX_EDIT_BBMAX,
};

enum qccompilestatus
{
	QCSTATUS_UNKNOWN = 0,
	QCSTATUS_NOLOGFILE,
	QCSTATUS_COMPILING,
	QCSTATUS_ERROR,
	QCSTATUS_COMPLETE_WITH_WARNING,
	QCSTATUS_COMPLETE
};

struct qcpathrecord_t
{
	char szAbsPath[1024];
	char szPrettyPath[1024];
	char szLogFilePath[1024];
	char szCWDPath[1024];
	char szMostRecentWarningOrError[1024];
	char szModelPath[1024];
	
	qccompilestatus status;

	void InitFromAbsPath( const char* szInputAbsPath )
	{
		V_strcpy_safe( szAbsPath, szInputAbsPath );
		V_FileBase( szInputAbsPath, szPrettyPath, sizeof(szPrettyPath) );

		V_strcpy_safe( szLogFilePath, szInputAbsPath );
		V_SetExtension( szLogFilePath, ".log", sizeof(szLogFilePath) );

		V_strcpy_safe( szCWDPath, szInputAbsPath );
		V_StripFilename( szCWDPath );
	}

	bool DoesAbsPathMatch( const char* szOther )
	{
		return ( !V_strcmp( szAbsPath, szOther ) );
	}

	qcpathrecord_t()
	{
		szAbsPath[0] = '\0';
		szPrettyPath[0] = '\0';
		szLogFilePath[0] = '\0';
		szCWDPath[0] = '\0';
		szMostRecentWarningOrError[0] = '\0';
		szModelPath[0] = '\0';
		status = QCSTATUS_UNKNOWN;
	}
};

#define MAX_NUM_QCPATH_RECORDS 64

extern CUtlVector< qcpathrecord_t > g_QCPathRecords;

struct ViewerSettings
{
	char	registrysubkey[ 64 ];
	int		application_mode;	// 0 expression, 1 choreo

	bool showHitBoxes;
	int  showHitBoxSet;
	int  showHitBoxNumber;
	bool showBones;
	bool showAttachments;
	bool showPhysicsModel;
	bool showPhysicsPreview;
	bool showSequenceBoxes;
	bool enableIK;
	bool enableTargetIK;
	bool showNormals;
	bool showTangentFrame;
	bool overlayWireframe;
	bool enableNormalMapping;
	bool enableDisplacementMapping;
	bool enableParallaxMapping;
	bool enableSpecular;
	bool showIllumPosition;
	bool playSounds;

	// Current attachment we're editing. -1 if none.
	int m_iEditAttachment;
	bool showLightingCenter;
	int  highlightPhysicsBone;
	int  highlightHitbox;
	int  highlightBone;
	QAngle lightrot;	// light rotation
	float lColor[4];	// directional color
	float aColor[4];	// ambient color

	// external

	// model 
	float  fov;		// horizontal field of view

	// render
	int renderMode;
	bool showBackground;
	bool showGround;
	bool showTexture;
	bool showMovement;
	bool showShadow;
	bool showOrbitCircle;
	bool allowOrbitYaw;
	int texture;
	int skin;
	int materialIndex;
	bool showOriginAxis;
	float originAxisLength;

	// animation
	float speedScale;
	bool blendSequenceChanges;
	bool animateWeapons;

	// softbodies
	bool simulateSoftbodies;

	// bodyparts and bonecontrollers
	//int submodels[32];
	//float controllers[8];

	// fullscreen
	int xpos, ypos;
	int width, height;
	bool cds;

	// colors
	float bgColor[4];	// background color
	float gColor[4];

	// misc
	bool pause;
	bool rotating;
	bool mousedown;

	HitboxEditMode hitboxEditMode;

	bool showBoneNames;

	// only used for fullscreen mode
	// char modelFile[256];
	//char backgroundTexFile[256];
	//char groundTexFile[256];

	int lod;
	bool autoLOD;
	bool softwareSkin;
	bool overbright;

	int	thumbnailsize;
	int thumbnailsizeanim;

	int	speechapiindex;
	int cclanguageid; // Close captioning language id (see sentence.h enum)

	bool showHidden;
	bool showActivities;
	bool showSequenceIndices;
	bool sortSequences;

	bool dotaMode;

	bool faceposerToolsDriveMouth;

	char mergeModelFile[HLMV_MAX_MERGED_MODELS][256];

	bool secondaryLights;

	RnDebugDrawOptions_t softbodyDrawOptions;

	ViewerSettings();

};

extern ViewerSettings g_viewerSettings;
class StudioModel;

void InitViewerSettings ( const char *subkey );
bool LoadViewerSettings (const char *filename, StudioModel *pModel );
bool SaveViewerSettings (const char *filename, StudioModel *pModel );
bool LoadViewerRootSettings( void );
bool SaveViewerRootSettings( void );

bool SaveCompileQCPathSettings( void );
bool LoadCompileQCPathSettings( void );

// For saving/loading "global" settings
bool LoadViewerSettingsInt( char const *keyname, int *value );
bool SaveViewerSettingsInt ( const char *keyname, int value );



struct debug_vert_weight_t
{
	int index;
	float flweight;
};
extern int g_BoneWeightInspectVert;
extern debug_vert_weight_t g_BoneWeightInspectResults[3];


#endif // INCLUDED_VIEWERSETTINGS 
