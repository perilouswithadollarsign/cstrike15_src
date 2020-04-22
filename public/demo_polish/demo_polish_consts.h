//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef C_DEMO_POLISH_CONSTS_H
#define C_DEMO_POLISH_CONSTS_H
#ifdef _WIN32
#pragma once
#endif

//------------------------------------------------------------------------------------------------------------------------

#define kMaxPlayersSupported			4
#define MAX_STR							256
#define DEMO_POLISH_DIR					"demo_polish"
#define DEMO_POLISH_DIR_FILTERED		"filtered"
#define	DEMO_POLISH_DIR_RAW				"raw"
#define DEMO_POLISH_HEADER_NAME			"header.bin"
#define DEMO_POLISH_CFG_FILENAME		"demo_polish_settings.cfg"
#define SERIALIZE_POLISH_FILES_AS_TEXT	false

//------------------------------------------------------------------------------------------------------------------------

enum
{ 
	kMaxBones		= 128,	// Same as MAXSTUDIOBONES -- including "studio.h" here would be painful
	kMaxOverlays	= 16 ,
};

enum ESide
{
	kInvalidSide = -1,
	kLeft,
	kRight
};

//------------------------------------------------------------------------------------------------------------------------

inline ESide GetOppositeSide(ESide iSide)
{
	Assert( !iSide == (iSide == kLeft ? kRight : kLeft) );
	return (ESide)(!(int)iSide);
}

//------------------------------------------------------------------------------------------------------------------------

#endif