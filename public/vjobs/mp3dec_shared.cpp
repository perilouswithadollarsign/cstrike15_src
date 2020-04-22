//========== Copyright © Valve Corporation, All rights reserved. ========
#include "ps3/spu_job_shared.h"
#include "vjobs/mp3dec_shared.h"

// layer 3, version {2, 1} ( indexed by LSB of m_nAudioVersionId)
uint8 s_mp3_bitrate_8000[2][16] =  // bitrate / 8000
{
	{ // version 2, version id = 10b
		0, // free
			8 / 8,
			16 / 8,
			24 / 8,
			32 / 8,
			40 / 8,
			48 / 8,
			56 / 8,
			64 / 8,
			80 / 8,
			96 / 8,
			112 / 8,
			128 / 8,
			144 / 8,
			160 / 8,
			0// bad
	},
	{  // version 1, version id = 11b
		0, // free
			32 / 8,
			40 / 8,	
			48 / 8,	
			56 / 8,	
			64 / 8,	
			80 / 8,
			96 / 8,	
			112 / 8,
			128 / 8,
			160 / 8,
			192 / 8,
			224 / 8,
			256 / 8,
			320 / 8,
			0 //bad
		}
};



uint16 s_mp3_samplingrate_div50[2][4]  = 
{
	{// version 2, version id = 10b
		22050/50,
			24000/50,
			16000/50,
			0 // reserved
	},
	{// version 1, version id = 11b
		44100/50,
			48000/50,
			32000/50,
			0
		}
};

void job_mp3dec::Context_t::Init()
{
	ZeroMemAligned( this, sizeof( *this ) );
}
