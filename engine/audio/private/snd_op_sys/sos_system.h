//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// 
//
//===============================================================================

#ifndef SOS_SYSTEM_H
#define SOS_SYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/UtlStringMap.h"
#include "tier1/utlstring.h"
#include "tier1/utlsymbol.h"
#include "toolframework/itoolframework.h"
#include "snd_dma.h"
#include "snd_channels.h"
#include "sos_entry_match_system.h"



class CSosOperator;
class CSosOperatorStack;
// Externs the logging channel
DECLARE_LOGGING_CHANNEL( LOG_SND_OPERATORS );

extern bool IsSoundSourceViewEntity( int soundsource );


extern ConVar snd_sos_show_operator_updates;
extern ConVar snd_sos_show_operator_start;
extern ConVar snd_sos_show_operator_prestart;
extern ConVar snd_sos_show_operator_entry_filter;

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
#define SOS_INVALID_TRACK_NUMBER -1


struct track_data_t 
{
	void SetDefaults( )
	{
		m_pTrackName = NULL;
		m_pSyncTrackName = NULL;
		m_nTrackNumber = SOS_INVALID_TRACK_NUMBER;
		m_nTrackPriority = 0;
		m_bPriorityOverride = false;
		m_bBlockEqualPriority = false;
		m_nSyncTrackNumber = SOS_INVALID_TRACK_NUMBER;
		m_flStartPoint = 0.0;
		m_flEndPoint = 0.0;
	};

	const char *m_pTrackName;
	const char *m_pSyncTrackName;
	int m_nTrackNumber;
	int m_nTrackPriority;
	bool m_bPriorityOverride;
	bool m_bBlockEqualPriority;
	int m_nSyncTrackNumber;
	float m_flStartPoint;
	float m_flEndPoint;

};
bool S_TrackHasPriority( track_data_t &newTrackData, track_data_t &existingTrackData );

struct stack_data_t
{
	stack_data_t( )
	{
		m_nGuid = -1;
		m_flStartTime = -1.0;
		m_pOperatorsKV = NULL;
		m_nSoundScriptHash = SOUNDEMITTER_INVALID_HASH;
	};

	int m_nGuid;
	float m_flStartTime;
	KeyValues *m_pOperatorsKV;
	HSOUNDSCRIPTHASH m_nSoundScriptHash;

};
bool S_GetTrackData( KeyValues *pOperatorsKV, track_data_t &trackData );

extern CSosOperatorStackList *S_InitChannelOperators( stack_data_t &stackData );
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
#define SO_MAX_DATA_SIZE SO_MAX_SPEAKERS
#define SO_MAX_SPEAKERS ( CCHANVOLUMES / 2 )
#define SO_POSITION_ARRAY_SIZE 3
#define XPOSITION 0
#define YPOSITION 1
#define ZPOSITION 2
#define SO_ANGLE_ARRAY_SIZE 3
#define XANGLE 0
#define YANGLE 1
#define ZANGLE 2

// 
// 
// 
class CScratchPad
{

public:

	void Clear()
	{
		m_bIsPlayerSound = false;
		m_bIsLooping = false;

		// cleared before update loop, needs to go away anyway
		// 		m_vBlendedListenerOrigin.Init( 0, 0, 0 );
		m_UtlVecMultiOrigins.RemoveAll( );
		m_nChannel = 6;
		m_nSoundSource = 0;
		m_flDelay = 0.0;
		m_bBlockStart = false;
		m_nSoundScriptHash = SOUNDEMITTER_INVALID_HASH;
		m_flDelayToQueue = 0.0;
	}
	
// 	void SetPerUpdate();
	void SetPerExecution( channel_t *pChannel, StartSoundParams_t *pStartParams )
	{
		Clear();

		if( pChannel )
		{
			//////////////////////////////////////////////////////////////////////////
			// channel params
			//////////////////////////////////////////////////////////////////////////

			VectorCopy( pChannel->origin, m_vEmitterInfoOrigin );
			VectorCopy( pChannel->direction, m_vEmitterInfoDirection );
			m_nSoundSource = pChannel->soundsource;
			m_nChannel = pChannel->entchannel;
			pChannel->sfx ? pChannel->sfx->getname( m_emitterInfoSoundName, sizeof( m_emitterInfoSoundName ) ) : "";
			m_flEmitterInfoMasterVolume = ( (float) pChannel->master_vol ) / 255.0f;
			m_vEmitterInfoSoundLevel = DIST_MULT_TO_SNDLVL( pChannel->dist_mult );
			m_flEmitterInfoPitch = pChannel->basePitch * 0.01f;
			m_nEmitterInfoSpeakerEntity = pChannel->speakerentity;
			m_nEmitterInfoRadius = pChannel->radius;
			
			CAudioSource *pSource = pChannel->sfx ? pChannel->sfx->pSource : NULL;
			if ( pSource )
			{
				m_bIsLooping = pSource->IsLooped();
			}

			m_nSoundScriptHash = pChannel->m_nSoundScriptHash;
		}
		else if ( pStartParams )
		{
			char m_emitterInfoSoundName[MAX_PATH];	
			pStartParams->pSfx ? pStartParams->pSfx->getname( m_emitterInfoSoundName, sizeof( m_emitterInfoSoundName ) ) : "";
			// MORASKY: handle emitter angles throughout scratchpad
			//VectorCopy( pStartParams.direction, g_scratchpad.m_vEmitterInfoAngle );
			VectorCopy( pStartParams->origin, m_vEmitterInfoOrigin );
			VectorCopy( pStartParams->direction, m_vEmitterInfoDirection );
			m_nSoundSource = pStartParams->soundsource;
			m_nChannel = pStartParams->entchannel;
			m_vEmitterInfoSoundLevel = pStartParams->soundlevel;
			m_flEmitterInfoPitch = pStartParams->pitch * 0.01f;
			m_nEmitterInfoSpeakerEntity = pStartParams->speakerentity;
			m_flEmitterInfoMasterVolume = pStartParams->fvol;
			m_nEmitterInfoRadius = dB_To_Radius( m_vEmitterInfoSoundLevel );

			CAudioSource *pSource = pStartParams->pSfx ? pStartParams->pSfx->pSource : NULL;
			if ( pSource )
			{
				m_bIsLooping = pSource->IsLooped();
			}

			if( pStartParams->m_bIsScriptHandle )
			{
				m_nSoundScriptHash = pStartParams->m_nSoundScriptHash;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// is this a player originated sound and not a tool sound?
		//////////////////////////////////////////////////////////////////////////
		if ( IsSoundSourceViewEntity( m_nSoundSource ) && !toolframework->InToolMode() )
		{
			// sounds coming from listener actually come from a short distance directly in front of listener
			// in tool mode however, the view entity is meaningless, since we're viewing from arbitrary locations in space
			m_bIsPlayerSound = true;
		}
	}
	
	HSOUNDSCRIPTHASH	m_nSoundScriptHash;

	// listener(s)
	// NOTE: should be made 1 listener per updated and use iterators
	Vector		m_vPlayerOrigin[ MAX_SPLITSCREEN_CLIENTS ];
	Vector		m_vPlayerForward[ MAX_SPLITSCREEN_CLIENTS ];
	Vector		m_vPlayerRight[ MAX_SPLITSCREEN_CLIENTS ];
	Vector		m_vPlayerUp[ MAX_SPLITSCREEN_CLIENTS ];
	Vector m_vClientSourceVectors[ MAX_SPLITSCREEN_CLIENTS ];
	Vector m_vBlendedListenerOrigin;

	// source info
	bool m_bIsLooping;
	bool m_bIsPlayerSound;

	// emitter info
	int m_nChannel;
	int m_nSoundSource;
	char m_emitterInfoSoundName[MAX_PATH];	
	Vector m_vEmitterInfoOrigin;
	Vector m_vEmitterInfoDirection;
	soundlevel_t m_vEmitterInfoSoundLevel;
	float m_flEmitterInfoPitch;
	int m_nEmitterInfoSpeakerEntity;
	float m_flEmitterInfoMasterVolume; // channel actually? ie: it's been altered by operators?
	float m_nEmitterInfoRadius;

	// not passed to scratchpad from StartSoundParams_t
// 	CSfxTable		*pSfx;
// 	int				flags;
// 	float			delay;
// 	int				initialStreamPosition;
// 	HSOUNDSCRIPTHANDLE m_nSoundScriptHandle;
// 	const char		*m_pSoundEntryName;
// 
// 	bool			staticsound : 1;
// 	bool			bUpdatePositions : 1;
// 	bool			fromserver : 1;
// 	bool			bToolSound : 1;
// 	bool			m_bIsScriptHandle : 1;

	// entity info
	QAngle m_vEntityInfoAngle;
	Vector m_vEntityInfoOrigin;
	float m_flEntityInfoRadius;
	CUtlVector< Vector > m_UtlVecMultiOrigins;


	// outputs
	float m_flDelay;
	bool m_bBlockStart;
	float m_flDelayToQueue;

private:

};


enum SODataType_t
{
	SO_FLOAT = 0,
};

#define SO_SINGLE 1
#define SO_VEC3 3
#define SO_SPEAKERS SO_MAX_SPEAKERS
#define SO_VEC3_X_8 3 * 8


// enum SODataCount_t
// {
// 	SO_FLOAT_1 = 1,
// 	SO_VEC3 = 3,
// 	SO_SPEAKERS = SO_MAX_SPEAKERS,
// 	SO_VEC3_X_8 = 3 * 8,
// 
// };


// SIZES MUST MATCH ABOVE INDICES
// short SODataSizes[] = { sizeof( float ), sizeof( float ) * 3, sizeof( float ) * SO_MAX_SPEAKERS, sizeof( float ) * 3 * 8 };
// 
// short SODataIndices[] = { 1, 3, SO_MAX_SPEAKERS, 3 * 8 };

enum SOSStopType_t
{
	SOS_STOP_NONE = 0,
	SOS_STOP_NORM,
	SOS_STOP_FORCE,
	SOS_STOP_HOLD,
	SOS_STOP_QUEUE
};

//-----------------------------------------------------------------------------
// The CSosOperatorStack class:
//
// Manages a "stack" of operators.
//
//-----------------------------------------------------------------------------
class CSosOperatorStack
{
public:

	enum SosStackType_t
	{
		SOS_NONE = 0,
		SOS_UPDATE,
		SOS_CUE,
		SOS_START,
		SOS_STOP,

	};

	CSosOperatorStack( SosStackType_t SosType, stack_data_t &stackData );
	~CSosOperatorStack();

	SosStackType_t GetType() { return m_SOSType; }

	void			SetName( const char *pName ) { V_strcpy_safe( m_nName, pName ); }
	const char		*GetName() { return m_nName; }
	void			SetChannelGuid( int nGuid ) { m_nChannelGuid = nGuid; }
	int				GetChannelGuid( ) const { return m_nChannelGuid; }
	void			SetStartTime( float flStartTime ) { m_flStartTime = flStartTime; }
	float			GetStartTime( ) const { return m_flStartTime; }
	float			GetElapsedTime( ) const { return g_pSoundServices->GetHostTime() - m_flStartTime; }
	void			SetStopTime( float flStopTime ) { m_flStopTime = flStopTime; }
	float			GetStopTime( ) const { return m_flStopTime; }
	float			GetElapsedStopTime( ) const { return m_flStopTime < 0.0 ? -1.0 : g_pSoundServices->GetHostTime() - m_flStopTime; }
	void			SetScriptHash( HSOUNDSCRIPTHASH nHash );
	HSOUNDSCRIPTHASH GetScriptHash( ) const { return m_nScriptHash; }
	void			SetOperatorsKV( KeyValues *pOpKV ) { m_pOperatorsKV = pOpKV; }
	KeyValues		*GetOperatorsKV( ) const { return m_pOperatorsKV; }
	void			SetStopType( SOSStopType_t stopType );
	SOSStopType_t	GetStopType( ) const { return m_stopType; }

	KeyValues		*GetSyncPointsKV( KeyValues *pOperatorsKV, const char *pListName );
	KeyValues		*GetSyncPointsKV( const char *pListName );
	
	void			GetTrackData( track_data_t &trackData ) const;

	void			AddToTail( CSosOperator *pOperator, const char *pName );

	int				FindOperatorViaOffset( size_t nOffset );
	CSosOperator	*FindOperator( const char *pName, void **pStructHandle );
	int				FindOperator( const char *pName );
	const char*     GetOperatorName( int nOpIndex ) { return m_vOperatorMap.GetElementName( nOpIndex ); }
	int				GetOperatorOffset( int nIndex );
	int				GetOperatorOutputOffset( const char *pOperatorName, const char *pOutputName );
	void			*GetMemPtr() { return m_pMemPool; }
	size_t			GetSize() { return m_nMemSize; }
	void			Execute( channel_t *pChannel, CScratchPad *pScratchPad );
	void			ExecuteIterator( channel_t *pChannel, CScratchPad *pScratchPad, const void *pVoidMem, const char *pOperatorName, int * pnOperatorIndex );
	void			Init( );
	void			Shutdown( );

	inline bool		IsType( SosStackType_t SosType ) { return ( SosType == m_SOSType ); }

	void			Print( int nLevel );
	void			Copy( CSosOperatorStack *pDstStack, size_t nMemOffset );
	void			ParseKV( KeyValues *pParamKV );
	bool			ShouldPrintOperators( ) const;
	bool			ShouldPrintOperatorUpdates( ) const;
// 	void			OverParseKV( KeyValues *pOperatorsKV );


private:

	char m_nName[64];
	SosStackType_t m_SOSType;
	void *m_pMemPool;
	size_t m_nMemSize;
	CUtlVector< CSosOperator * > m_vStack;
	CUtlDict < int, int > m_vOperatorMap;

	int m_nChannelGuid;
	float m_flStartTime;
	HSOUNDSCRIPTHASH m_nScriptHash;
	KeyValues *m_pOperatorsKV;
	SOSStopType_t m_stopType;
	float m_flStopTime;

public:
	const char *m_pCurrentOperatorName;

};

extern CSosOperatorStack *S_GetStack( CSosOperatorStack::SosStackType_t stackType, stack_data_t &stackData );

//-----------------------------------------------------------------------------
// The CSosOperatorStackList class:
//
// Manages a "stack" of operators.
//
//-----------------------------------------------------------------------------
class CSosOperatorStackList
{

public:
	CSosOperatorStackList();
	~CSosOperatorStackList();

	void			SetStopType( SOSStopType_t stopType );
	SOSStopType_t	GetStopType( ) const { return m_stopType; }
	void			SetStopTime( float flStopTime );
	float			GetStartTime( ) const { return m_flStopTime; }
	float			GetElapsedStopTime( ) const { return m_flStopTime < 0.0 ? 0.0 : g_pSoundServices->GetHostTime() - m_flStopTime; }

	bool HasStack( CSosOperatorStack::SosStackType_t SosType );

	void Execute( CSosOperatorStack::SosStackType_t SosType, channel_t *pChannel, CScratchPad *pScratchPad );

	void SetChannelGuid( int nGuid );
	void SetStartTime( float flStartTime );
	void SetScriptHash( HSOUNDSCRIPTHASH nHash );
	void SetStack( CSosOperatorStack *pStack );
	CSosOperatorStack *GetStack( CSosOperatorStack::SosStackType_t SosType );
	void StopStacks( SOSStopType_t stopType );
	bool IsStopped( ) const { return ( m_stopType != SOS_STOP_NONE && m_stopType!= SOS_STOP_HOLD && m_stopType!=  SOS_STOP_QUEUE ); }
	bool IsStopping( ) const { return ( m_stopType != SOS_STOP_NONE ); }
	void ParseKV( stack_data_t &stackData );
	void Print(  );

private:

	CSosOperatorStack *m_vUpdateStack;
	CSosOperatorStack *m_vStartStack;
	CSosOperatorStack *m_vStopStack;
	CSosOperatorStack *m_vCueStack;
	SOSStopType_t m_stopType;
	float m_flStopTime;	

};

//-----------------------------------------------------------------------------
// The CSosOperatorStackCollection class:
//
// Manages a "stack" of operators.
//
//-----------------------------------------------------------------------------
class CSosOperatorStackCollection
{

public:
	~CSosOperatorStackCollection();
	void Clear();

	CSosOperatorStack *GetStack( const char *pStackName );
	CSosOperatorStack *GetStack( CSosOperatorStack::SosStackType_t SosType,  const char *pStackName );

	void Print();
	void ParseKV( CSosOperatorStack::SosStackType_t SosType, KeyValues *pParamKV );

private:

	CUtlDict <CSosOperatorStack *, int> m_vAllStacks;
	CUtlDict <CSosOperatorStack *, int> m_vUpdateStacks;
	CUtlDict <CSosOperatorStack *, int> m_vStartStacks;
	CUtlDict <CSosOperatorStack *, int> m_vStopStacks;
	CUtlDict <CSosOperatorStack *, int> m_vCueStacks;

};

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class SosStopQueueData_t
{
public:
	int m_nChannelGuid;
	float m_nStopTime;
};
class SosStartQueueData_t
{
public:
	StartSoundParams_t m_startSoundParams;
	float m_nStartTime;
	bool m_bFromPrestart;
};

class CSosOperatorSystem
{
public:
	CSosOperatorSystem();
	~CSosOperatorSystem();

	void Init();
	void Update();
	void Shutdown();
	void Flush();
	void PrintOperatorList();

	void ClearSubSystems();

	void QueueStartEntry( StartSoundParams_t &startParams, float flDelay = 0.0, bool bFromPrestart = false );
	void StartQueuedEntries();

	void QueueStopChannel( int nChannelGuid, float flDelay = 0.0 );
	void StopQueuedChannels();
	bool IsInStopQueue( int nChannelGuid );

	void StopChannelOnTrack( const char *pTrackName, bool bStopAll = false, float flStopDelay = 0.0 );
	void SetChannelOnTrack( const char *pTrackName, channel_t *pChannel );
	void RemoveChannelFromTracks( int nGuid );
	void RemoveChannelFromTrack( const char *pTrackName, int nGuid );
	void DEBUG_ShowTrackList( void );

	channel_t *GetChannelOnTrack( const char *pTrackName );
	HSOUNDSCRIPTHASH GetSoundEntryOnTrack( const char *pTrackName );
	bool GetTrackDataOnTrack( const char *pTrackName, track_data_t &trackData );

	bool SetOpVarFloat( const char *pString, float flVariable );
	bool GetOpVarFloat( const char *pString, float &flVariable );
	void DEBUG_ShowOpvarList( void );


public:
	CSosOperatorStackCollection m_MasterStackCollection;
	CUtlDict <CSosOperator*, int> m_vOperatorCollection;

 	CSosEntryMatchList m_sosEntryBlockList;

	CUtlDict <channel_t *, int> m_vTrackDict;


	// NOT THREAD PROTECTED!
	CUtlVector < SosStopQueueData_t * > m_sosStopChannelQueue;
	CUtlVector< SosStartQueueData_t * >	m_sosStartEntryQueue;

//	CTSQueue< StartSoundParams_t >	m_sosStartEntryQueue;
//	CTSQueue< sos_stop_queue_t >	m_sosStopChannelQueue;

	static CSosOperatorSystem *GetSoundOperatorSystem();

	// Used to generate random numbers
	CUniformRandomStream m_operatorRandomStream;

private: 
	bool	m_bHasInitialized;
//	CUtlStringMap < float > m_sosFloatOpvarMap;
	CUtlDict < float, int > m_sosOpVarFloatMap;
};

#define g_pSoundOperatorSystem CSosOperatorSystem::GetSoundOperatorSystem()

#endif // SOS_SYSTEM_H
