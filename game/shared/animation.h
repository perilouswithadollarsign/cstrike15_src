//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#ifndef ANIMATION_H
#define ANIMATION_H

#ifdef _WIN32
#pragma once
#endif


#define ACTIVITY_NOT_AVAILABLE		-1

struct animevent_t;
struct studiohdr_t;
class CStudioHdr;
struct mstudioseqdesc_t;

int ExtractBbox( CStudioHdr *pstudiohdr, int sequence, Vector& mins, Vector& maxs );

void IndexModelSequences( CStudioHdr *pstudiohdr );
void ResetActivityIndexes( CStudioHdr *pstudiohdr );
void VerifySequenceIndex( CStudioHdr *pstudiohdr );
int SelectWeightedSequence( CStudioHdr *pstudiohdr, int activity, int curSequence = -1 );
int SelectHeaviestSequence( CStudioHdr *pstudiohdr, int activity );
void SetEventIndexForSequence( mstudioseqdesc_t &seqdesc );
void BuildAllAnimationEventIndexes( CStudioHdr *pstudiohdr );
void ResetEventIndexes( CStudioHdr *pstudiohdr );
float GetSequenceLinearMotionAndDuration( CStudioHdr *pstudiohdr, int iSequence, const float poseParameter[], Vector *pVec );

void GetEyePosition( CStudioHdr *pstudiohdr, Vector &vecEyePosition );

int LookupActivity( CStudioHdr *pstudiohdr, const char *label );
int LookupSequence( CStudioHdr *pstudiohdr, const char *label );

#define NOMOTION 99999
void GetSequenceLinearMotion( CStudioHdr *pstudiohdr, int iSequence, const float poseParameter[], Vector *pVec );

const char *GetSequenceName( CStudioHdr *pstudiohdr, int sequence );
const char *GetSequenceActivityName( CStudioHdr *pstudiohdr, int iSequence );

int GetSequenceFlags( CStudioHdr *pstudiohdr, int sequence );
int GetAnimationEvent( CStudioHdr *pstudiohdr, int sequence, animevent_t *pNPCEvent, float flStart, float flEnd, int index );
bool HasAnimationEventOfType( CStudioHdr *pstudiohdr, int sequence, int type );

int FindTransitionSequence( CStudioHdr *pstudiohdr, int iCurrentSequence, int iGoalSequence, int *piDir );
bool GotoSequence( CStudioHdr *pstudiohdr, int iCurrentSequence, float flCurrentCycle, float flCurrentRate, int iGoalSequence, int &nNextSequence, float &flNextCycle, int &iNextDir );

void SetBodygroup( CStudioHdr *pstudiohdr, int& body, int iGroup, int iValue );
int GetBodygroup( CStudioHdr *pstudiohdr, int body, int iGroup );
void SetBodygroupPreset( CStudioHdr *pstudiohdr, int& body, char const *szName );

const char *GetBodygroupName( CStudioHdr *pstudiohdr, int iGroup );
int FindBodygroupByName( CStudioHdr *pstudiohdr, const char *name );
const char *GetBodygroupPartName( CStudioHdr *pstudiohdr, int iGroup, int iPart );
int GetBodygroupCount( CStudioHdr *pstudiohdr, int iGroup );
int GetNumBodyGroups( CStudioHdr *pstudiohdr );

int GetSequenceActivity( CStudioHdr *pstudiohdr, int sequence, int *pweight = NULL );

void GetAttachmentLocalSpace( CStudioHdr *pstudiohdr, int attachIndex, matrix3x4_t &pLocalToWorld );

float SetBlending( CStudioHdr *pstudiohdr, int sequence, int *pblendings, int iBlender, float flValue );

int FindHitboxSetByName( CStudioHdr *pstudiohdr, const char *name );
const char *GetHitboxSetName( CStudioHdr *pstudiohdr, int setnumber );
int GetHitboxSetCount( CStudioHdr *pstudiohdr );

enum animtag_indices
{
	ANIMTAG_INVALID = -1,
	ANIMTAG_UNINITIALIZED = 0,
	ANIMTAG_STARTCYCLE_N,
	ANIMTAG_STARTCYCLE_NE,
	ANIMTAG_STARTCYCLE_E,
	ANIMTAG_STARTCYCLE_SE,
	ANIMTAG_STARTCYCLE_S,
	ANIMTAG_STARTCYCLE_SW,
	ANIMTAG_STARTCYCLE_W,
	ANIMTAG_STARTCYCLE_NW,
	ANIMTAG_AIMLIMIT_YAWMIN_IDLE,
	ANIMTAG_AIMLIMIT_YAWMAX_IDLE,
	ANIMTAG_AIMLIMIT_YAWMIN_WALK,
	ANIMTAG_AIMLIMIT_YAWMAX_WALK,
	ANIMTAG_AIMLIMIT_YAWMIN_RUN,
	ANIMTAG_AIMLIMIT_YAWMAX_RUN,
	ANIMTAG_AIMLIMIT_YAWMIN_CROUCHIDLE,
	ANIMTAG_AIMLIMIT_YAWMAX_CROUCHIDLE,
	ANIMTAG_AIMLIMIT_YAWMIN_CROUCHWALK,
	ANIMTAG_AIMLIMIT_YAWMAX_CROUCHWALK,
	ANIMTAG_AIMLIMIT_PITCHMIN_IDLE,
	ANIMTAG_AIMLIMIT_PITCHMAX_IDLE,
	ANIMTAG_AIMLIMIT_PITCHMIN_WALKRUN,
	ANIMTAG_AIMLIMIT_PITCHMAX_WALKRUN,
	ANIMTAG_AIMLIMIT_PITCHMIN_CROUCH,
	ANIMTAG_AIMLIMIT_PITCHMAX_CROUCH,
	ANIMTAG_AIMLIMIT_PITCHMIN_CROUCHWALK,
	ANIMTAG_AIMLIMIT_PITCHMAX_CROUCHWALK,
	ANIMTAG_WEAPON_POSTLAYER,
	ANIMTAG_FLASHBANG_PASSABLE,
	ANIMTAG_COUNT
};

float GetFirstSequenceAnimTag( CStudioHdr *pstudiohdr, int sequence, int nDesiredTag, float flStart = 0, float flEnd = 1 );

float GetAnySequenceAnimTag( CStudioHdr *pstudiohdr, int sequence, int nDesiredTag, float flDefault );

#endif	//ANIMATION_H
