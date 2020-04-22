//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMECLIP_H
#define DMECLIP_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"
#include "datamodel/dmehandle.h"
#include "avi/iavi.h"
#include "materialsystem/materialsystemutil.h"
#include "tier1/utlmap.h"
#include "videocache/iremotevideomaterial.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeClip;
class CDmeTimeFrame;
class CDmeBookmark;
class CDmeBookmarkSet;
class CDmeSound;
class CDmeChannel;
class CDmeCamera;
class CDmeDag;
class CDmeInput;
class CDmeOperator;
class CDmeMaterial;
class CDmeTrack;
class CDmeTrackGroup;
class IMaterial;
class CDmeChannelsClip;
class CDmeAnimationSet;
class CDmeMaterialOverlayFXClip;
class DmeLog_TimeSelection_t;
struct TimeSelection_t;
struct Rect_t;
enum ChannelMode_t;

enum DmeClipSkipFlag_t
{
	DMESKIP_NONE = 0,
	DMESKIP_MUTED = 1,
	DMESKIP_INVISIBLE = 2,
};
DEFINE_ENUM_BITWISE_OPERATORS( DmeClipSkipFlag_t )


//-----------------------------------------------------------------------------
// Clip types
//-----------------------------------------------------------------------------
enum DmeClipType_t
{
	DMECLIP_UNKNOWN = -1,

	DMECLIP_FIRST = 0,

	DMECLIP_CHANNEL = 0,
	DMECLIP_SOUND,
	DMECLIP_FX,
	DMECLIP_FILM,

	DMECLIP_LAST = DMECLIP_FILM,

	DMECLIP_TYPE_COUNT
};
DEFINE_ENUM_INCREMENT_OPERATORS( DmeClipType_t )


struct DmeClipStack_t
{
public:
	DmeClipStack_t() : m_bOptimized( false ) {}
	DmeClipStack_t( const CDmeClip *pRoot, CDmeClip *pShot, CDmeClip *pClip ) { BuildClipStack( pRoot, pShot, pClip ); }

	int GetClipCount() const { return m_clips.Count(); }
	const CDmeClip *GetClip( int i ) const { return m_clips[ i ]; }
	      CDmeClip *GetClip( int i )       { return m_clips[ i ]; }
	int FindClip( const CDmeClip *pClip ) const;
	int InvalidClipIndex() const { return m_clips.InvalidIndex(); }

	bool BuildClipStack( const CDmeClip *pRoot, const CDmeClip *pShot, const CDmeClip *pClip );

	int AddClipToHead( const CDmeClip *pClip );
	int AddClipToTail( const CDmeClip *pClip );
	void RemoveClip( int i ) { m_clips.Remove( i ); m_bOptimized = false; }
	void RemoveAll() { m_clips.RemoveAll(); m_bOptimized = false; }

	DmeTime_t ToChildMediaTime  ( DmeTime_t t, bool bClamp = true ) const;
	DmeTime_t FromChildMediaTime( DmeTime_t t, bool bClamp = true ) const;
	DmeTime_t ToChildMediaDuration ( DmeTime_t t ) const;
	DmeTime_t FromChildMediaDuration( DmeTime_t t ) const;

	void ToChildMediaTime( TimeSelection_t &params ) const;

protected:
	// Given a root clip and a child (or grandchild) clip, builds the stack 
	// from root on down to the destination clip. If shot is specified, then it
	// must build a clip stack that passes through the shot
	bool BuildClipStack_R( const CDmeClip *pMovie, const CDmeClip *pShot, const CDmeClip *pCurrent );


	CUtlVector< CDmeHandle< CDmeClip > > m_clips;
	mutable DmeTime_t m_tStart;
	mutable DmeTime_t m_tDuration;
	mutable DmeTime_t m_tOffset;
	mutable double m_flScale;
	mutable bool m_bOptimized;

	void Optimize() const;
};


//-----------------------------------------------------------------------------
// Is a particular clip type non-overlapping?
//-----------------------------------------------------------------------------
inline bool IsNonoverlapping( DmeClipType_t type )
{
	return ( type == DMECLIP_FILM );
}


//-----------------------------------------------------------------------------
// String to clip type + back
//-----------------------------------------------------------------------------
DmeClipType_t ClipTypeFromString( const char *pName );
const char *ClipTypeToString( DmeClipType_t type );


//-----------------------------------------------------------------------------
// Used to move clips in non-film track groups with film clips
//-----------------------------------------------------------------------------
struct ClipAssociation_t
{
	enum AssociationType_t
	{
		HAS_CLIP = 0,
		BEFORE_START,
		AFTER_END,
		NO_MOVEMENT,
	};

	AssociationType_t m_nType;
	CDmeHandle< CDmeClip > m_hClip;
	CDmeHandle< CDmeClip > m_hAssociation;
	DmeTime_t m_startTimeInAssociatedClip; // used for HAS_CLIP
	DmeTime_t m_offset; // used for BEFORE_START and AFTER_END
};


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
class CDmeClip : public CDmElement
{
	DEFINE_ELEMENT( CDmeClip, CDmElement );

public:
	// Returns the time frame
	CDmeTimeFrame *GetTimeFrame() const;
	DmeTime_t ToChildMediaTime  ( DmeTime_t t, bool bClamp = true ) const;
	DmeTime_t FromChildMediaTime( DmeTime_t t, bool bClamp = true ) const;
	DmeTime_t ToChildMediaDuration  ( DmeTime_t dt ) const;
	DmeTime_t FromChildMediaDuration( DmeTime_t dt ) const;
	DmeTime_t GetStartTime() const;
	DmeTime_t GetEndTime() const;
	DmeTime_t GetDuration() const;
	DmeTime_t GetTimeOffset() const;
	DmeTime_t GetStartInChildMediaTime() const;
	DmeTime_t GetEndInChildMediaTime() const;
	float GetTimeScale() const;
	void SetStartTime ( DmeTime_t t );
	void SetDuration  ( DmeTime_t t );
	void SetTimeOffset( DmeTime_t t );
	void SetTimeScale ( float s );

	virtual void BakeTimeScale( float scale = 1.0f );

	// Given a root clip and a child (or grandchild) clip, builds the stack 
	// from root on down to the destination clip. If shot is specified, then it
	// must build a clip stack that passes through the shot
	bool BuildClipStack( DmeClipStack_t* pStack, const CDmeClip *pRoot, CDmeClip *pShot = NULL );

	void SetClipColor( const Color& clr );
	Color GetClipColor() const;

	void SetClipText( const char *pText );
	const char* GetClipText() const;

	// Clip type
	virtual DmeClipType_t GetClipType() { return DMECLIP_UNKNOWN; }

	// Track group iteration methods
	int GetTrackGroupCount() const;
	CDmeTrackGroup *GetTrackGroup( int nIndex ) const;
	const CUtlVector< DmElementHandle_t > &GetTrackGroups( ) const;

	// Track group addition/removal
	void AddTrackGroup( CDmeTrackGroup *pTrackGroup );
	void AddTrackGroupBefore( CDmeTrackGroup *pTrackGroup, CDmeTrackGroup *pBefore );
	CDmeTrackGroup *AddTrackGroup( const char *pTrackGroupName );
	void RemoveTrackGroup( int nIndex );
	void RemoveTrackGroup( CDmeTrackGroup *pTrackGroup );
	void RemoveTrackGroup( const char *pTrackGroupName );

	// Track group finding
	CDmeTrackGroup *FindTrackGroup( const char *pTrackGroupName ) const;
	int GetTrackGroupIndex( CDmeTrackGroup *pTrack ) const;
	CDmeTrackGroup *FindOrAddTrackGroup( const char *pTrackGroupName );

	// Swap track groups
	void SwapOrder( CDmeTrackGroup *pTrackGroup1, CDmeTrackGroup *pTrackGroup2 );

	// Clip finding
	virtual CDmeTrack *FindTrackForClip( CDmeClip *pClip, CDmeTrackGroup **ppTrackGroup = NULL ) const;
	bool FindMultiTrackGroupForClip( CDmeClip *pClip, int *pTrackGroupIndex, int *pTrackIndex = NULL, int *pClipIndex = NULL ) const;

	// Finding clips in tracks by time
	virtual void FindClipsAtTime( DmeClipType_t clipType, DmeTime_t time, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const;
	virtual void FindClipsIntersectingTime( DmeClipType_t clipType, DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const;
	virtual void FindClipsWithinTime      ( DmeClipType_t clipType, DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const;

	// Is a particular clip typed able to be added?
	bool IsSubClipTypeAllowed( DmeClipType_t type ) const;

	// Returns the special film track group
	virtual CDmeTrackGroup *GetFilmTrackGroup() const { return NULL; }
	virtual CDmeTrack *GetFilmTrack() const { return NULL; }

	// Checks for muteness
	void SetMute( bool state );
	bool IsMute( ) const;

	// Clip vertical display size
	void SetDisplayScale( float flDisplayScale );
	float GetDisplayScale() const;

protected:
	virtual int AllowedClipTypes() const { return 1 << DMECLIP_CHANNEL; }

	// Is a track group valid to add?
	bool IsTrackGroupValid( CDmeTrackGroup *pTrackGroup );

	CDmaElementArray< CDmeTrackGroup > m_TrackGroups;

	CDmaElement< CDmeTimeFrame > m_TimeFrame;
	CDmaVar< Color > m_ClipColor;
	CDmaVar< bool > m_bMute;
	CDmaVar< float > m_flDisplayScale;

	CDmaString m_ClipText;
};

inline bool CDmeClip::IsSubClipTypeAllowed( DmeClipType_t type ) const
{
	return ( AllowedClipTypes() & ( 1 << type ) ) != 0;
}

inline void CDmeClip::SetMute( bool state )
{
	m_bMute = state;
}

inline bool CDmeClip::IsMute( ) const
{
	return m_bMute;
}

//-----------------------------------------------------------------------------
// Clip vertical display size
//-----------------------------------------------------------------------------
inline void CDmeClip::SetDisplayScale( float flDisplayScale )
{
	m_flDisplayScale = flDisplayScale;
}

inline float CDmeClip::GetDisplayScale() const
{
	return m_flDisplayScale;
}

//-----------------------------------------------------------------------------
// Sound clip
//-----------------------------------------------------------------------------
class CDmeSoundClip : public CDmeClip
{
	DEFINE_ELEMENT( CDmeSoundClip, CDmeClip );

public:
	virtual DmeClipType_t GetClipType() { return DMECLIP_SOUND; }

	void SetShowWave( bool state );
	bool ShouldShowWave( ) const;

	void SetFadeTimes( DmeTime_t fadeIn, DmeTime_t fadeOut ) { m_fadeInDuration = fadeIn; m_fadeOutDuration = fadeOut; }
	void SetFadeInTime( DmeTime_t t ) { m_fadeInDuration = t; }
	void SetFadeOutTime( DmeTime_t t ) { m_fadeOutDuration = t; }
	DmeTime_t GetFadeInTime() const { return m_fadeInDuration; }
	DmeTime_t GetFadeOutTime() const { return m_fadeOutDuration; }

	float GetVolumeFade( DmeTime_t tParent );

	virtual void BakeTimeScale( float scale = 1.0f );

	CDmaElement< CDmeSound >	m_Sound;
	CDmaVar< bool >				m_bShowWave;
	CDmaTime					m_fadeInDuration;
	CDmaTime					m_fadeOutDuration;
};


//-----------------------------------------------------------------------------
// Clip containing recorded data from the game
//-----------------------------------------------------------------------------
class CDmeChannelsClip : public CDmeClip
{
	DEFINE_ELEMENT( CDmeChannelsClip, CDmeClip );

public:
	virtual DmeClipType_t GetClipType() { return DMECLIP_CHANNEL; }

	virtual void BakeTimeScale( float scale = 1.0f );

	CDmeChannel *CreatePassThruConnection
	( 
		char const *passThruName,
		CDmElement *pFrom, 
		char const *pFromAttribute, 
		CDmElement *pTo, 
		char const *pToAttribute,
		int index = 0 
	);

	void RemoveChannel( CDmeChannel *pChannel );
	
	// Set the mode of all of the channels in the clip
	void SetChannelMode( const ChannelMode_t &mode );

	// Operate all of the channels in the clip
	void OperateChannels();

	// Play all of the channels in the clip
	void PlayChannels(); 

	CDmaElementArray< CDmeChannel > m_Channels;
};


//-----------------------------------------------------------------------------
// An effect clip
//-----------------------------------------------------------------------------
class CDmeFXClip : public CDmeClip
{
	DEFINE_ELEMENT( CDmeFXClip, CDmeClip );

public:
	virtual DmeClipType_t GetClipType() { return DMECLIP_FX; }

	enum
	{
		MAX_FX_INPUT_TEXTURES = 3
	};

	// All effects must be able to apply their effect
	virtual void ApplyEffect( DmeTime_t time, Rect_t &currentRect, Rect_t &totalRect, ITexture *pTextures[MAX_FX_INPUT_TEXTURES] ) {}

	// Global list of FX clip types
	static void InstallFXClipType( const char *pElementType, const char *pDescription );
	static int FXClipTypeCount();
	static const char *FXClipType( int nIndex );
	static const char *FXClipDescription( int nIndex );

private:
	enum
	{
		MAX_FXCLIP_TYPES = 16
	};
	static const char *s_pFXClipTypes[MAX_FXCLIP_TYPES];
	static const char *s_pFXClipDescriptions[MAX_FXCLIP_TYPES];
	static int s_nFXClipTypeCount;
};


//-----------------------------------------------------------------------------
// Helper Template factory for simple creation of factories
//-----------------------------------------------------------------------------
template <class T>
class CDmFXClipFactory : public CDmElementFactory<T>
{
public:
	CDmFXClipFactory( const char *pLookupName, const char *pDescription ) : CDmElementFactory<T>( pLookupName ) 
	{
		CDmeFXClip::InstallFXClipType( pLookupName, pDescription );
	}
};


//-----------------------------------------------------------------------------
// All effects must use IMPLEMENT_FX_CLIP_ELEMENT_FACTORY instead of IMPLEMENT_ELEMENT_FACTORY
//-----------------------------------------------------------------------------
#if defined( MOVIEOBJECTS_LIB ) || defined ( DATAMODEL_LIB ) || defined ( DMECONTROLS_LIB )

#define IMPLEMENT_FX_CLIP_ELEMENT_FACTORY( lookupName, className, description )	\
	IMPLEMENT_ELEMENT( className )																\
	CDmFXClipFactory< className > g_##className##_Factory( #lookupName, description );			\
	CDmElementFactoryHelper g_##className##_Helper( #lookupName, &g_##className##_Factory, true );	\
	className *g_##className##LinkerHack = NULL;

#else

#define IMPLEMENT_FX_CLIP_ELEMENT_FACTORY( lookupName, className, description )	\
	IMPLEMENT_ELEMENT( className )																\
	CDmFXClipFactory< className > g_##className##_Factory( #lookupName, description );			\
	CDmElementFactoryHelper g_##className##_Helper( #lookupName, &g_##className##_Factory, false );	\
	className *g_##className##LinkerHack = NULL;

#endif


//-----------------------------------------------------------------------------
// Film clip
//-----------------------------------------------------------------------------
class CDmeFilmClip : public CDmeClip
{
	DEFINE_ELEMENT( CDmeFilmClip, CDmeClip );

public:
	virtual DmeClipType_t GetClipType() { return DMECLIP_FILM; }

	virtual void BakeTimeScale( float scale = 1.0f );

	// Attribute changed
	virtual void OnElementUnserialized( );
	virtual void OnAttributeChanged( CDmAttribute *pAttribute );

	// Resolve
	virtual void Resolve();

	// Returns the special film track group
	virtual CDmeTrackGroup *GetFilmTrackGroup() const;
	virtual CDmeTrack *GetFilmTrack() const;

	CDmeTrackGroup *FindOrCreateFilmTrackGroup();
	CDmeTrack *FindOrCreateFilmTrack();

	// Clip finding
	virtual CDmeTrack *FindTrackForClip( CDmeClip *pClip, CDmeTrackGroup **ppTrackGroup = NULL ) const;

	// Finding clips in tracks by time
	virtual void FindClipsAtTime( DmeClipType_t clipType, DmeTime_t time, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const;
	virtual void FindClipsIntersectingTime( DmeClipType_t clipType, DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const;
	virtual void FindClipsWithinTime      ( DmeClipType_t clipType, DmeTime_t startTime, DmeTime_t endTime, DmeClipSkipFlag_t flags, CUtlVector< CDmeClip * >& clips ) const;

	// mapname helper methods
	const char *GetMapName();
	void SetMapName( const char *pMapName );

	// Returns the camera associated with the clip
	CDmeCamera *GetCamera();
	void SetCamera( CDmeCamera *pCamera );

	// Audio volume
	void			SetVolume( float state );
	float			GetVolume() const;

	int	GetConCommandCount() const;
	const char *GetConCommand( int i ) const;

	int	GetConVarCount() const;
	const char *GetConVar( int i ) const;

	// Returns the monitor camera associated with the clip (for now, only 1 supported)
	CDmeCamera *GetMonitorCamera();
	void AddMonitorCamera( CDmeCamera *pCamera );
	void RemoveMonitorCamera( CDmeCamera *pCamera );
	void SelectMonitorCamera( CDmeCamera *pCamera );
	int FindMonitorCamera( CDmeCamera *pCamera );

	// Scene / Dag helper methods
	void SetScene( CDmeDag *pDag );
	CDmeDag *GetScene( bool bCreateIfNull = false );

	// helper for inputs and operators
	int GetInputCount();
	CDmeInput *GetInput( int nIndex );
	void AddInput( CDmeInput *pInput );
	void RemoveAllInputs();
	void AddOperator( CDmeOperator *pOperator );
	void RemoveOperator( CDmeOperator *pOperator );
	void CollectOperators( CUtlVector< DmElementHandle_t > &operators );
	CDmaElementArray< CDmeOperator > &GetOperators();

	// Helper for overlays
	// FIXME: Change this to use CDmeMaterials
	IMaterial *GetOverlayMaterial();
	void SetOverlay( const char *pMaterialName );
	float GetOverlayAlpha();
	void SetOverlayAlpha( float alpha );
	void DrawOverlay( DmeTime_t time, Rect_t &currentRect, Rect_t &totalRect );
	bool HasOpaqueOverlay();

	// AVI tape out
	void UseCachedVersion( bool bUseCachedVersion );
	bool IsUsingCachedVersion() const;
	AVIMaterial_t GetCachedAVI();
	void SetCachedAVI( const char *pAVIFile );

	void AssignRemoteVideoMaterial( IRemoteVideoMaterial *theMaterial );
	void UpdateRemoteVideoMaterialStatus();
	bool HasRemoteVideo();
	bool GetCachedQTVideoFrameAt( float timeInSec );
	IMaterial *GetRemoteVideoMaterial();
	void GetRemoteVideoMaterialTexCoordRange( float *u, float *v );

	CDmaElementArray< CDmeAnimationSet > &GetAnimationSets(); // raw access to the array
	const CDmaElementArray< CDmeAnimationSet > &GetAnimationSets() const;
	CDmeAnimationSet *FindAnimationSet( const char *pAnimSetName ) const;

	const CDmaElementArray< CDmeBookmarkSet > &GetBookmarkSets() const;
	CDmaElementArray< CDmeBookmarkSet > &GetBookmarkSets();
	int GetActiveBookmarkSetIndex() const;
	void SetActiveBookmarkSetIndex( int nActiveBookmarkSet );
	CDmeBookmarkSet *GetActiveBookmarkSet();
	CDmeBookmarkSet *CreateBookmarkSet( const char *pName = "default set" );

	void SetFadeTimes( DmeTime_t fadeIn, DmeTime_t fadeOut ) { m_fadeInDuration = fadeIn; m_fadeOutDuration = fadeOut; }
	void SetFadeInTime( DmeTime_t t ) { m_fadeInDuration = t; }
	void SetFadeOutTime( DmeTime_t t ) { m_fadeOutDuration = t; }
	DmeTime_t GetFadeInTime() const { return m_fadeInDuration; }
	DmeTime_t GetFadeOutTime() const { return m_fadeOutDuration; }

	// Used to move clips in non-film track groups with film clips
	// Call BuildClipAssociations before modifying the film track,
	// then UpdateAssociatedClips after modifying it.
	void BuildClipAssociations( CUtlVector< ClipAssociation_t > &association, bool bHandleGaps = true );
	void UpdateAssociatedClips( CUtlVector< ClipAssociation_t > &association );

	void LatchWorkCamera( CDmeCamera *pCamera );
	void UpdateWorkCamera( CDmeCamera *pCamera );

	void PreviousWorkCamera();
	void NextWorkCamera();
	CDmeCamera *GetCurrentCameraStackEntry();
	void	PurgeCameraStack();

private:
	virtual int AllowedClipTypes() const { return (1 << DMECLIP_CHANNEL) | (1 << DMECLIP_SOUND) | (1 << DMECLIP_FX) | (1 << DMECLIP_FILM); }

	CDmaElement< CDmeTrackGroup >			m_FilmTrackGroup;

	CDmaString								m_MapName;
	CDmaElement     < CDmeCamera >			m_Camera;
	CDmaElementArray< CDmeCamera >			m_MonitorCameras;
	CDmaVar< int >							m_nActiveMonitor;
	CDmaElement     < CDmeDag >				m_Scene;

	CDmaElementArray< CDmeInput >			m_Inputs;
	CDmaElementArray< CDmeOperator >		m_Operators;

	CDmaString								m_AVIFile;

	CDmaTime								m_fadeInDuration;
	CDmaTime								m_fadeOutDuration;

	CDmaElement< CDmeMaterialOverlayFXClip >m_MaterialOverlayEffect;
	CDmaVar< bool >							m_bIsUsingCachedVersion;

	CDmaElementArray< CDmeAnimationSet >	m_AnimationSets;	// "animationSets"
	CDmaElementArray< CDmeBookmarkSet >		m_BookmarkSets;				// "bookmarkSets"
	CDmaVar< int >							m_nActiveBookmarkSet;		// "activeBookmarkSet"

	CDmaVar< float >						m_Volume;

	CDmaStringArray							m_ConCommands;
	CDmaStringArray							m_ConVars;

	AVIMaterial_t							m_hCachedVersion;
	bool									m_bReloadCachedVersion;
	
	CMaterialReference						m_FadeMaterial;

	CDmaElementArray< CDmeCamera >			m_CameraStack;
	int										m_nCurrentStackCamera;
	
	IRemoteVideoMaterial				   *m_pRemoteVideoMaterial;
	
};


//-----------------------------------------------------------------------------
// Fast type conversions
//-----------------------------------------------------------------------------
inline bool IsFilmClip( CDmeClip *pClip )
{
	return pClip && pClip->IsA( CDmeFilmClip::GetStaticTypeSymbol() );
}


//-----------------------------------------------------------------------------
// Creates a slug clip
//-----------------------------------------------------------------------------
CDmeFilmClip *CreateSlugClip( const char *pClipName, DmeTime_t startTime, DmeTime_t endTime, DmFileId_t fileid );


//-----------------------------------------------------------------------------
// For use in template functions
//-----------------------------------------------------------------------------
template <class T>
class CDmeClipInfo
{
public:
	static DmeClipType_t ClipType( ) { return DMECLIP_UNKNOWN; }
};

#define DECLARE_DMECLIP_TYPE( _className, _dmeClipType )			\
	template< > class CDmeClipInfo< _className >					\
	{																\
	public:															\
		static DmeClipType_t ClipType() { return _dmeClipType; }	\
	};

DECLARE_DMECLIP_TYPE( CDmeSoundClip,	DMECLIP_SOUND )
DECLARE_DMECLIP_TYPE( CDmeChannelsClip,	DMECLIP_CHANNEL )
DECLARE_DMECLIP_TYPE( CDmeFXClip,		DMECLIP_FX )
DECLARE_DMECLIP_TYPE( CDmeFilmClip,		DMECLIP_FILM )

#define DMECLIP_TYPE( _className )	CDmeClipInfo<T>::ClipType()


//-----------------------------------------------------------------------------
// helper methods
//-----------------------------------------------------------------------------
CDmeTrack *GetParentTrack( CDmeClip *pClip );
CDmeChannel *FindChannelTargetingElement( CDmElement *pElement, const char *pAttributeName = NULL );
CDmeChannel *FindChannelTargetingElement( CDmeChannelsClip *pChannelsClip, CDmElement *pElement, const char *pAttributeName = NULL );
CDmeChannel *FindChannelTargetingElement( CDmeFilmClip *pClip, CDmElement *pElement, const char *pAttributeName, CDmeChannelsClip **ppChannelsClip, CDmeTrack **ppTrack = NULL, CDmeTrackGroup **ppTrackGroup = NULL );
CDmeFilmClip *FindFilmClipContainingDag( CDmeDag *pDag );
void BuildClipStackList( const CUtlVector< CDmeChannel* > &channelList, CUtlVector< DmeClipStack_t > &clipStackList, CUtlVector< DmeTime_t > &orginalTimeList, const CDmeClip *pMovie, CDmeClip *pShot );
void PlayChannelsAtTime( DmeTime_t time, const CUtlVector< CDmeChannel* > &channelList, const CUtlVector< CDmeOperator* > &operatorList, const CUtlVector< DmeClipStack_t > &clipStackList, bool forcePlay = true );
void PlayChannelsAtLocalTimes( const CUtlVector< DmeTime_t > &timeList, const CUtlVector< CDmeChannel* > &channelList, const CUtlVector< CDmeOperator* > &operatorList, bool forcePlay = true );



#endif // DMECLIP_H
