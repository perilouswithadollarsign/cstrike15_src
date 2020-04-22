//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef SND_WAVE_MIXER_PS3_MP3_H
#define SND_WAVE_MIXER_PS3_MP3_H
#pragma once

class CAudioMixer;
class IWaveData;

CAudioMixer *CreatePs3Mp3Mixer( IWaveData *data, int initialStreamPosition, int skipInitialSamples, bool bUpdateDelayForChoreo );

#endif // SND_WAVE_MIXER_PS3_MP3_H
