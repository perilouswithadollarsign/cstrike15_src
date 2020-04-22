//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SND_MIXGROUP_MANAGER_H
#define SND_MIXGROUP_MANAGER_H
#ifdef _WIN32
#pragma once
#endif

enum MXRMixGroupFields_t
{
	MXR_MIXGROUP_NONE = 0,
	MXR_MIXGROUP_VOL,
	MXR_MIXGROUP_LEVEL,
	MXR_MIXGROUP_DSP,
	MXR_MIXGROUP_SOLO,
	MXR_MIXGROUP_MUTE
};

struct mixervalues_t
{
//public:
	float volume;
	float level;
	float dsp;
	mixervalues_t(void)
	{
		volume = 1.0;
		level = 1.0;
		dsp = 1.0;
	}
};

void MXR_SetSoloActive(void);
bool MXR_LoadAllSoundMixers( void );
void MXR_ReleaseMemory( void );
int MXR_GetMixGroupListFromDirName( const char *pDirname, byte *pList, int listMax );
bool MXR_IsMusicGroup( int ruleIndex );
void MXR_GetMixGroupFromSoundsource( channel_t *pchan );
void MXR_GetValuesFromMixGroupIndex( mixervalues_t *mixValues, int imixgroup );
void MXR_GetVolFromMixGroup( channel_t *ch, mixervalues_t *mixValues, int *plast_mixgroupid );
char *MXR_GetGroupnameFromId( int mixgroupid );
int MXR_GetMixgroupFromName( const char *pszgroupname );
int MXR_GetFirstValidMixGroup( channel_t *pChannel );

void MXR_DebugShowMixVolumes( void );
#ifdef _DEBUG
void MXR_DebugSetMixGroupVolume( const CCommand &args );
#endif //_DEBUG

static void MXR_SetSoundMixer( const CCommand &args );
static void MXR_ListMixGroups( const CCommand &args );

void MXR_UpdateAllDuckerVolumes( void );

// // sound mixers
// int g_csoundmixers	= 0;					// total number of soundmixers found
// extern int g_cgrouprules;					// total number of group rules found
// int g_cgroupclass	= 0;

int MXR_GetMixLayerIndexFromName( const char *szmixlayername );
void S_SetMixGroupOfMixLayer( int nMixGroupIndex, int nMixLayerIndex, MXRMixGroupFields_t nMixGroupField, float flValue );



#endif // SND_MIXGROUP_MANAGER_H
