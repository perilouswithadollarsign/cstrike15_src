//========= Copyright ©, Valve LLC, All rights reserved. ============
//
// Purpose: declares a variety of constants
//
// $NoKeywords: $
//=============================================================================

#ifndef GCCONSTANTS_H
#define GCCONSTANTS_H
#ifdef _WIN32
#pragma once
#endif

#include "steam/steamtypes.h"
#include "tier0/t0constants.h"

namespace GCSDK
{

//-----------------------------------------------------------------------------
// Timing constants
//-----------------------------------------------------------------------------

// How long each frame should last
const int k_cMicroSecPerShellFrame = 50 * k_nThousand;

// How many frames per second should we have
const uint32 k_cFramesPerSec = k_nMillion / k_cMicroSecPerShellFrame;

// Task granularity -- the longest time in microseconds you should spend without yielding
const int k_cMicroSecTaskGranularity = 5 * k_nThousand;

// If a frame runs longer than it's supposed to (which they always will by at least a little),
// we subtract the overage from the next frame.  This is the maximum amount to correct each frame.
const int k_cMicroSecMaxFrameCorrection = 25 * k_nThousand;

// if server time is too far behind, we will start skipping frames to re-synchronize it with real time
const int k_cMicroSecBehindToStartFrameSkipping = (k_nMillion * 2);

// Default Max time to allow a job to be blocked on I/O
const int k_cMicroSecJobPausedTimeout = 20 * k_nMillion;

// How much time a job should run before heartbeating
const int k_cMicroSecJobHeartbeat = k_cMicroSecJobPausedTimeout / 4;

// Default Max number of job heartbeat intervals to allow a job to be blocked on I/O
const int k_cJobHeartbeatsBeforeTimeoutDefault = k_cMicroSecJobPausedTimeout / k_cMicroSecJobHeartbeat;

// Number of seconds to take to cycle through all active sessions
const int k_nUserSessionRunInterval = 5 * k_nMillion;
const int k_nGSSessionRunInterval = 5 * k_nMillion;
const int k_nAccountDetailsRunInterval = 30 * k_nMillion;
const int k_nLocksRunInterval = 120 * k_nMillion;

//-----------------------------------------------------------------------------
// Spew / EmitEvent constants
//-----------------------------------------------------------------------------

#define SPEW_ALWAYS 1
#define SPEW_NEVER 5

#define LOG_ALWAYS 1
#define LOG_NEVER 5


//-----------------------------------------------------------------------------
// string constants
//-----------------------------------------------------------------------------

const char k_rgchUnknown[] = "Unknown";

#ifdef GC
//-----------------------------------------------------------------------------
// SQL constants
//-----------------------------------------------------------------------------

const int k_cSQLObjectNameMax = 128;			// max length of a SQL identifier (column name, index name, table name ... )
const int k_cchSQLStatementTextMax = 8192;		// nominal max length of a SQL statement
const uint32 k_cubRecordMax = 4 * k_nMillion;				// Max size of a single record
const uint32 k_cubVarFieldMax = 1 * k_nMillion;			// Max size of a variable-length field
const int k_cMaxCol = 50;
const uint32 k_cMaxBindParams = 500;				// the largest number of bind parameters allowed in a single query

// iTable constants
const int k_iTableNil = -1;		// No table at all
const int k_iFieldNil = -1;		// No field at all


enum EForeignKeyAction
{
	k_EForeignKeyActionNoAction	= 0,
	k_EForeignKeyActionCascade	= 1,
	k_EForeignKeyActionSetNULL	= 2,
};
const char *PchNameFromEForeignKeyAction( EForeignKeyAction eForeignKeyAction );
EForeignKeyAction EForeignKeyActionFromName( const char *pchName );

const char *PchNameFromEGCSQLType( EGCSQLType eForeignKeyAction );

#ifndef SQLRETURN
typedef short SQLRETURN;
#endif // SQLRETURN
#endif // GC


//-----------------------------------------------------------------------------
// WebAPI constants
//-----------------------------------------------------------------------------

const uint32 k_cubWebAPIKey = 16;									// size of the web api key
const uint32 k_cchWebAPIKeyStringMax = (k_cubWebAPIKey * 2) + 1;	// size of string representing web API key (hex encoded so twice the size + NUL)
typedef	unsigned char WebAPIKey_t[ k_cubWebAPIKey ];

// used to revoke WebAPI keys.  Stored in the database... do not reorder.
enum EWebAPIKeyStatus
{
	k_EWebAPIKeyValid = 0,
	k_EWebAPIKeyRevoked = 1,

	k_EWebAPIKeyInvalid = 255,
};


//-----------------------------------------------------------------------------
// Game Server constants
//-----------------------------------------------------------------------------

const uint32 k_cubGameServerToken = 16;									// size of the game server identity token key
const uint32 k_cchGameServerTokenStringMax = (k_cubGameServerToken * 2) + 1;	// size of string representing game server identity token (hex encoded so twice the size + NUL)
typedef	unsigned char GameServerIdentityToken_t[ k_cubGameServerToken ];


//-----------------------------------------------------------------------------
// Account linkage (constants from Steam)
//-----------------------------------------------------------------------------

const uint32 k_unInvalidPerfectWorldID = 0;


//-----------------------------------------------------------------------------
// Other constants
//-----------------------------------------------------------------------------

const int k_cSmallBuff = 255;					// smallish buffer 
const int k_cMedBuff = 1024;					// medium buffer 

const int k_cubMsgSizeSmall = 512;				// small sized packet

const int k_cInitialNetworkBuffers = 10;		// # of network buffers to see the system with

const int k_cubMaxExpectedMsgDataSize = 5 * k_nMegabyte;// the maximum application data that we EXPECT to be sent in a single message

#ifndef STRINGIFY
#define STRINGIFY(x) #x
#endif // CS:GO already defines STRINGIFY appropriately, no reason to redefine here
#define TOSTRING(x) STRINGIFY(x)
#define FILE_AND_LINE __FILE__ ":" TOSTRING(__LINE__)	

// Default capacity for session hash tables
const int k_cGCUserSessionInit = 10 * k_nThousand;		// Can grow indefinitely by this increment
const int k_cBucketGCUserSession = 100 * k_nThousand;			// Fixed size
const int k_cGCGSSessionInit = 5 * k_nThousand;		// Can grow indefinitely by this increment
const int k_cBucketGCGSSession = 50 * k_nThousand;			// Fixed size
const int k_cAccountDetailsInit = 10 * k_nThousand;		// Can grow indefinitely by this increment
const int k_cBucketAccountDetails = 100 * k_nThousand;			// Fixed size
const int k_cPersonaNamesInit = 50 * k_nThousand;		// Can grow indefinitely by this increment
const int k_cBucketPersonaNames = 250 * k_nThousand;			// Fixed size
const int k_cGCLocksInit = 50 * k_nThousand;		// Can grow indefinitely by this increment
const int k_cBucketGCLocks = 500 * k_nThousand;			// Fixed size

const char *PchNameFromEUniverse( EUniverse eUniverse );
EUniverse EUniverseFromName( const char *pchName );

} // namespace GCSDK

#endif // GCCONSTANTS_H
