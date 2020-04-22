//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// NOTE: This is a cut-and-paste hack job to get animation set construction
// working from a commandline tool. It came from tools/ifm/createsfmanimation.cpp
// This file needs to die almost immediately + be replaced with a better solution
// that can be used both by the sfm + sfmgen.
//
//=============================================================================

#ifndef SFMANIMATIONSETUTILS_H
#define SFMANIMATIONSETUTILS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/UtlVector.h"
#include "tier1/UtlDict.h"
#include "tier1/UtlString.h"
#include "datamodel/dmattributevar.h"
#include "dme_controls/BaseAnimationSetEditorController.h"

//-----------------------------------------------------------------------------
// movieobjects
//-----------------------------------------------------------------------------
class CDmeCamera;
class CDmeFilmClip;
class CDmeGameModel;
class CDmeAnimationSet;
class CDmeGameModel;
class CDmeProjectedLight;
class CDmePresetGroup;
class CDmeChannelsClip;
class CDmeTransform;
class CDmeChannel;
class CDmeLog;
class CStudioHdr;

extern const char *g_pSuffix[];
extern DmAttributeType_t g_ChannelTypes[];

enum ControlType_t
{
	CONTROL_TYPE_POSITION	 = 0,
	CONTROL_TYPE_ORIENTATION = 1,
};

enum LogPreviewChannelType_t
{
	LOG_PREVIEW_VALUE = 0,
	LOG_PREVIEW_VALUE_RIGHT,
	LOG_PREVIEW_VALUE_LEFT,
	LOG_PREVIEW_FLEX_CHANNEL_COUNT,

	LOG_PREVIEW_POSITION = 0,
	LOG_PREVIEW_ORIENTATION,
	LOG_PREVIEW_TRANSFORM_CHANNEL_COUNT,

	LOG_PREVIEW_MAX_CHANNEL_COUNT = LOG_PREVIEW_FLEX_CHANNEL_COUNT,
};

struct LogPreview_t;
struct Context_t
{
	DECLARE_FIXEDSIZE_ALLOCATOR( Context_t );
public:
	Context_t() : 
		m_bHighlight( false ),
		m_bValid( false ),
		m_bHasAncestorsBeingManipulated( false )
	{
		SetIdentityMatrix( m_InitialBoneToWorld );
		SetIdentityMatrix( m_InitialWorldToBone );
		SetIdentityMatrix( m_InitialBoneMatrix );

		m_CurrentWorldPosition.Init();

		SetIdentityMatrix( m_InitialTranslationWorldToParent );
	}

	void						InitContext( const LogPreview_t *lp );
	void						UpdatePosition( bool bInitialAndCurrent );

	// from TxForm_t
	CUtlString					m_basename;
	bool						m_bHighlight : 1;

	bool						m_bValid : 1;
	bool						m_bHasAncestorsBeingManipulated : 1;

	CDmeHandle< CDmeDag >		m_hDrag;

	// those originally used by DragContextList
	matrix3x4_t					m_InitialBoneToWorld;
	matrix3x4_t					m_InitialWorldToBone;
	matrix3x4_t					m_InitialBoneMatrix;

	// those originally used by FullTransformList
	Vector						m_CurrentWorldPosition;
	
	matrix3x4_t					m_InitialTranslationWorldToParent;
};

struct LogPreview_t : public SelectionInfo_t
{
	DECLARE_FIXEDSIZE_ALLOCATOR( LogPreview_t );
public:

	LogPreview_t() : SelectionInfo_t(), m_pTransformContext( NULL ) {}
	LogPreview_t( CDmeAnimationSet *pAnimSet, CDmElement *pControl, TransformComponent_t nComponentFlags );

	bool IsEqual( const LogPreview_t& other ) const
	{
		if ( m_hControl != other.m_hControl )
			return false;
		for ( int i = 0; i < LOG_PREVIEW_MAX_CHANNEL_COUNT; ++i )
		{
			if ( m_hChannels[ i ] != other.m_hChannels[ i ] )
				return false;
		}
		if ( m_hOwner != other.m_hOwner )
			return false;
		if ( m_nComponentFlags != other.m_nComponentFlags )
			return false;

		return true;
	}

	bool operator==( const LogPreview_t& other ) const
	{
		return IsEqual( other );
	}

	LogComponents_t GetLogComponentFlagsForChannel( int nChannel )
	{
		// If the control is a transform control convert the flags from transform component flags to log component flags
		if ( m_pTransformContext )
		{					
			return ConvertTransformFlagsToLogFlags( m_nComponentFlags, ( nChannel == LOG_PREVIEW_ORIENTATION ) );
		}

		// For all other controls consider all components selected
		return LOG_COMPONENTS_ALL;
	}

	CDmeChannel *GetSelectedChannel( int nChannel )
	{
		if ( nChannel >=  LOG_PREVIEW_MAX_CHANNEL_COUNT )
			return NULL;

		// If the control is a transform control check the flags to determine if the specific channel is selected
		if ( m_pTransformContext )
		{
			if ( ( nChannel == LOG_PREVIEW_POSITION ) && ( ( m_nComponentFlags & TRANSFORM_COMPONENT_POSITION ) == 0 ) )
				return NULL;

			if ( ( nChannel == LOG_PREVIEW_ORIENTATION ) && ( ( m_nComponentFlags & TRANSFORM_COMPONENT_ROTATION ) == 0 ) )
				return NULL;		
		}

		return m_hChannels[ nChannel ];
	}
	

	CDmeHandle< CDmeChannel >		m_hChannels[ LOG_PREVIEW_MAX_CHANNEL_COUNT ];
	CDmeHandle< CDmeChannelsClip >	m_hOwner;
	Context_t						*m_pTransformContext;
};
struct MDLSquenceLayer_t;


//-----------------------------------------------------------------------------
// Creates an animation set
//-----------------------------------------------------------------------------
CDmeAnimationSet *CreateEmptyAnimationSet( CDmeFilmClip *pClip, const char *pSetName, CDmeChannelsClip **ppChannelsClip = NULL, CDmeControlGroup**ppControlGroup = NULL );
CDmeAnimationSet *CreateAnimationSetForDag( CDmeFilmClip *pClip, const char *pSetName, CDmeDag *pDag );

CDmeAnimationSet *CreateAnimationSet( CDmeFilmClip *pClip, const char *pSetName, CDmeCamera *pCamera );
CDmeAnimationSet *CreateAnimationSet( CDmeFilmClip *pClip, const char *pSetName, CDmeProjectedLight *pLight );
CDmeAnimationSet *CreateAnimationSet( CDmeFilmClip *pMovie, CDmeFilmClip *pShot, CDmeGameModel *pGameModel, const char *pAnimationSetName, bool bAttachToGameRecording, CDmElement *pSharedPresetGroupSettings );

CDmeGameModel *CreateGameModel( CDmeFilmClip *pShot, const char *pRelativeName );



// building blocks for creating animationsets (don't contain undo scopes)
void AddFloatControlToAnimationSet( CDmeFilmClip *pFilmClip, CDmeAnimationSet *pAnimSet, CDmeChannelsClip *pDstChannelsClip, CDmeControlGroup *pControlGroup, CDmAttribute *pSrcAttr, float flMin = 0.0f, float flMax = 1.0f, float flDefault = -FLT_MAX );
void AddColorControlToAnimationSet( CDmeFilmClip *pFilmClip, CDmeAnimationSet *pAnimSet, CDmeChannelsClip *pDstChannelsClip, CDmeControlGroup *pControlGroup, CDmAttribute *pSrcAttr );
void AddTransformControlsToAnimationSet( CDmeFilmClip *pFilmClip, CDmeAnimationSet *pAnimSet, CDmeChannelsClip *pDstChannelsClip, CDmeControlGroup *pControlGroup, CDmeTransform *pTransform, const char *pControlName );
void AddLocalViewTargetControl( CDmeFilmClip *pFilmClip, CDmeAnimationSet *pAnimSet, CDmeGameModel *pGameModel, CDmeChannelsClip *pChannelsClip );
void AddEyeConvergenceControl( CDmeAnimationSet *pAnimationSet, CDmeGameModel *pGameModel, CDmeChannelsClip *pChannelsClip );
void AddViewTargetControl( CDmeFilmClip *pFilmClip, CDmeAnimationSet *pAnimationSet, CDmeGameModel *pGameModel, CDmeChannelsClip *pChannelsClip );


void AddIllumPositionAttribute( CDmeGameModel *pGameModel );
void BuildGroupMappings( CDmeAnimationSet *pAnimationSet );
CDmeTrack *GetAnimationSetTrack( CDmeFilmClip *pFilmClip, bool bCreateIfMissing );
CDmeChannelsClip* CreateChannelsClip( CDmeAnimationSet *pAnimationSet, CDmeFilmClip *pOwnerClip );
CDmeChannelsClip *FindChannelsClip( CDmeDag *pDag );
CDmeChannelsClip *FindChannelsClip( CDmeAnimationSet *pAnimSet );
CDmeDag *GetAnimSetTargetDag( CDmeAnimationSet *pAnimSet );
CDmeChannel *FindChannelTargetingTransform( CDmeChannelsClip *pChannelsClip, CDmeTransform *pTransform, ControlType_t controlType );
void CreateTransformChannels( CDmeTransform *pTransform, const char *pBaseName, CDmeChannelsClip *pChannelsClip );
void CreateAnimationLogs( CDmeChannelsClip *channelsClip, CDmeGameModel *pModel, const CStudioHdr &hdr, int sequence, float flStartTime, float flDuration, float flTimeStep );
void ImportAnimationLogs( CUtlVector< KeyValues* > &importData, DmeTime_t &duration, DmeTime_t startTime, DmeFramerate_t framerate, CDmeFilmClip *pMovie, CDmeFilmClip *pShot, CUtlLinkedList< LogPreview_t* > &controls, CDmeGameModel *pModel, int sequence, const CUtlVector< float > *pPoseParameters = NULL, const CUtlVector< MDLSquenceLayer_t > *pLayers = NULL, bool bRootMotion = true );
void DestroyLayerData( CUtlVector< KeyValues* > &layerData );
void RetimeLogData( CDmeChannelsClip *pSrcChannelsClip, CDmeChannelsClip *pDstChannelsClip, CDmeLog *pLog );
void TransferRemainingChannels( CDmeFilmClip *shot, CDmeChannelsClip *destClip, CDmeChannelsClip *srcClip );
template < class T >
CDmeChannel *CreateConstantValuedLog( CDmeChannelsClip *channelsClip, const char *pName, CDmElement *pToElement, const char *pToAttr, const T &value );


CDmeTransformControl *CreateTransformControlAndChannels( const char *pName, CDmeTransform *pTransform, CDmeChannelsClip *pSrcChannelsClip, CDmeChannelsClip *pDstChannelsClip, CDmeAnimationSet *pAnimationSet,  bool bUseExistingLogData );
void SetupBoneTransform( CDmeChannelsClip *pSrcChannelsClip, CDmeChannelsClip *pDstChannelsClip, CDmeAnimationSet *pAnimationSet, CDmeGameModel *pGameModel, const CStudioHdr &hdr, int bonenum, bool bUseExistingLogData );

// Finds a channel in the animation set to overwrite with import data
CDmeChannel* FindImportChannel( CDmeChannel *pChannel, CDmeChannelsClip *pChannelsClip );

// Transforms an imported channel, if necessary
void TransformBoneChannel( CDmeChannel *pChannel, bool bImport );

bool ImportChannel( CDmeChannel *pChannel, CDmeChannelsClip *pChannelsClip );

void GetChannelsForControl( const CDmElement *control, CDmeChannel *channels[LOG_PREVIEW_MAX_CHANNEL_COUNT] );

void GetChannelsForControl( const CDmElement *pControl, CUtlVector< CDmeChannel* > &channels );
void GetControlChannelsForAnimSet( CDmeAnimationSet *pAnimSet, CUtlVector< CDmeChannel* > &channels );

void ImportAnimation( CDmeChannelsClip *pImportedChannelsClip, CDmeAnimationSet *pAnimSet, int *pNumMatchingChannels, int *pNumMissingChannels, int *pNumSkippedChannels );


#endif // SFMANIMATIONSETUTILS_H
