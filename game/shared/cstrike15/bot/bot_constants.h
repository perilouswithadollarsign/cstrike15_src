//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

// Author: Matthew D. Campbell (matt@turtlerockstudios.com), 2003

#ifndef BOT_CONSTANTS_H
#define BOT_CONSTANTS_H

/// version number is MAJOR.MINOR
#define BOT_VERSION_MAJOR			1
#define BOT_VERSION_MINOR			50

//--------------------------------------------------------------------------------------------------------
/**
 * Difficulty levels
 */
enum BotDifficultyType
{
	BOT_EASY = 0,
	BOT_NORMAL = 1,
	BOT_HARD = 2,
	BOT_EXPERT = 3,

	NUM_DIFFICULTY_LEVELS
};

#ifdef DEFINE_DIFFICULTY_NAMES
	char *BotDifficultyName[] = 
	{
		"EASY", "NORMAL", "HARD", "EXPERT", NULL
	};
#else
	extern char *BotDifficultyName[];
#endif

namespace BotProfileInputDevice
{
	enum Device
	{
		GAMEPAD				= 0,
		KB_MOUSE			= 1,
		PS3_MOVE			= 2,
		HYDRA				= 3,
		SHARPSHOOTER		= 4,

		COUNT,								// Auto list counter
		FORCE_INT32  = 0x7FFFFFFF			// Force the typedef to be int32
	};
};
typedef BotProfileInputDevice::Device  BotProfileDevice_t;



#endif // BOT_CONSTANTS_H
