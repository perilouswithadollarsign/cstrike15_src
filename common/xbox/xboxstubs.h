//========= Copyright 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose: Win32 replacements for XBox.
//
//=============================================================================

#if !defined( XBOXSTUBS_H ) && !defined( _X360 )
#define XBOXSTUBS_H

#ifdef _WIN32
#pragma once
#endif

#ifdef _WIN32
typedef unsigned long DWORD;
//#include "winlite.h"
#endif

#include "tier0/platform.h"

//  Content creation/open flags
#define XCONTENTFLAG_NONE                           0x00
#define XCONTENTFLAG_CREATENEW                      0x00
#define XCONTENTFLAG_CREATEALWAYS                   0x00
#define XCONTENTFLAG_OPENEXISTING                   0x00
#define XCONTENTFLAG_OPENALWAYS                     0x00
#define XCONTENTFLAG_TRUNCATEEXISTING               0x00

//  Content attributes
#define XCONTENTFLAG_NOPROFILE_TRANSFER             0x00
#define XCONTENTFLAG_NODEVICE_TRANSFER              0x00
#define XCONTENTFLAG_STRONG_SIGNED                  0x00
#define XCONTENTFLAG_ALLOWPROFILE_TRANSFER          0x00
#define XCONTENTFLAG_MOVEONLY_TRANSFER              0x00

//	XNet flags
#define XNET_GET_XNADDR_PENDING             0x00000000 // Address acquisition is not yet complete
#define XNET_GET_XNADDR_NONE                0x00000001 // XNet is uninitialized or no debugger found
#define XNET_GET_XNADDR_ETHERNET            0x00000002 // Host has ethernet address (no IP address)
#define XNET_GET_XNADDR_STATIC              0x00000004 // Host has statically assigned IP address
#define XNET_GET_XNADDR_DHCP                0x00000008 // Host has DHCP assigned IP address
#define XNET_GET_XNADDR_PPPOE               0x00000010 // Host has PPPoE assigned IP address
#define XNET_GET_XNADDR_GATEWAY             0x00000020 // Host has one or more gateways configured
#define XNET_GET_XNADDR_DNS                 0x00000040 // Host has one or more DNS servers configured
#define XNET_GET_XNADDR_ONLINE              0x00000080 // Host is currently connected to online service
#define XNET_GET_XNADDR_TROUBLESHOOT        0x00008000 // Network configuration requires troubleshooting

// Console device ports
#define XDEVICE_PORT0               0
#define XDEVICE_PORT1               1
#define XDEVICE_PORT2               2
#define XDEVICE_PORT3               3
#ifndef _PS3 
#define XUSER_MAX_COUNT				4
#endif // !_PS3
#define XUSER_INDEX_NONE            0x000000FE

#define XBX_CLR_DEFAULT				0xFF000000
#define XBX_CLR_WARNING				0x0000FFFF
#define XBX_CLR_ERROR				0x000000FF

#ifndef _PS3 
#define XBOX_MINBORDERSAFE			0
#define XBOX_MAXBORDERSAFE			0
#endif // !_PS3

#if defined( PLATFORM_PS3 )
#include "ps3/ps3_core.h"
#endif

#ifndef _PS3
typedef enum
{
	XK_NULL,
	XK_BUTTON_UP,
	XK_BUTTON_DOWN,
	XK_BUTTON_LEFT,
	XK_BUTTON_RIGHT,
	XK_BUTTON_START,
	XK_BUTTON_BACK,
	XK_BUTTON_STICK1,
	XK_BUTTON_STICK2,
	XK_BUTTON_A,
	XK_BUTTON_B,
	XK_BUTTON_X,
	XK_BUTTON_Y,
	XK_BUTTON_LEFT_SHOULDER,
	XK_BUTTON_RIGHT_SHOULDER,
	XK_BUTTON_LTRIGGER,
	XK_BUTTON_RTRIGGER,
	XK_STICK1_UP,
	XK_STICK1_DOWN,
	XK_STICK1_LEFT,
	XK_STICK1_RIGHT,
	XK_STICK2_UP,
	XK_STICK2_DOWN,
	XK_STICK2_LEFT,
	XK_STICK2_RIGHT,
	XK_BUTTON_INACTIVE_START, 
	XK_BUTTON_FIREMODE_SELECTOR_1,
	XK_BUTTON_FIREMODE_SELECTOR_2,
	XK_BUTTON_FIREMODE_SELECTOR_3,
	XK_BUTTON_RELOAD,
	XK_BUTTON_TRIGGER,
	XK_BUTTON_PUMP_ACTION,
	XK_XBUTTON_ROLL_RIGHT,
	XK_XBUTTON_ROLL_LEFT,
	XK_MAX_KEYS,
} xKey_t;
#endif

//typedef enum
//{
//	XVRB_NONE,		// off
//	XVRB_ERROR,		// fatal error
//	XVRB_ALWAYS,	// no matter what
//	XVRB_WARNING,	// non-fatal warnings
//	XVRB_STATUS,	// status reports
//	XVRB_ALL,
//} xverbose_e;

#ifndef WORD
typedef unsigned short WORD;
#endif
#if ( !defined ( DWORD ) && !defined( WIN32 ) && !defined( _PS3 ))
typedef unsigned int DWORD;
#endif

#ifndef POSIX
typedef void* HANDLE;
# if defined(_PS3) || defined(POSIX)
typedef unsigned long long ULONGLONG
# else
typedef unsigned __int64 ULONGLONG;
# endif
#endif

#if defined( OSX )
typedef DWORD COLORREF;
#elif defined( POSIX ) && !defined( _PS3 )
typedef DWORD COLORREF;
#endif

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#endif

/*
* Internet address (old style... should be updated)
*/
#ifdef PLATFORM_PS3
#include <netinet/in.h>
typedef struct in_addr IN_ADDR;
#elif defined( POSIX )
struct ip4_addr {
	union {
		struct { unsigned char s_b1,s_b2,s_b3,s_b4; } S_un_b;
		struct { unsigned short s_w1,s_w2; } S_un_w;
		unsigned long S_addr;
	} S_un;
};
typedef struct ip4_addr IN_ADDR;
#else
#ifndef s_addr
/*
* Internet address (old style... should be updated)
*/
struct in_addr {
	union {
		struct { unsigned char s_b1,s_b2,s_b3,s_b4; } S_un_b;
		struct { unsigned short s_w1,s_w2; } S_un_w;
		unsigned long S_addr;
	} S_un;
#define s_addr  S_un.S_addr
	/* can be used for most tcp & ip code */
#define s_host  S_un.S_un_b.s_b2
	/* host on imp */
#define s_net   S_un.S_un_b.s_b1
	/* network */
#define s_imp   S_un.S_un_w.s_w2
	/* imp */
#define s_impno S_un.S_un_b.s_b4
	/* imp # */
#define s_lh    S_un.S_un_b.s_b3
	/* logical host */
};
typedef struct in_addr IN_ADDR;
#endif
#endif

typedef unsigned int XIN_ADDR;

typedef struct {
 	IN_ADDR    ina;                            // IP address (zero if not static/DHCP)
 	IN_ADDR    inaOnline;                      // Online IP address (zero if not online)
 	WORD        wPortOnline;                    // Online port
 	BYTE        abEnet[6];                      // Ethernet MAC address
	BYTE        abOnline[20];                   // Online identification
} XNADDR;

typedef uint64 XUID;

#ifndef INVALID_XUID
#define INVALID_XUID ((XUID) 0)
#endif

#ifndef _PS3
typedef struct {
	BYTE        ab[8];                          // xbox to xbox key identifier
} XNKID;

typedef struct {
	BYTE        ab[16];                         // xbox to xbox key exchange key
} XNKEY;
#endif

typedef struct _XSESSION_INFO
{
	XNKID sessionID;                // 8 bytes
	XNADDR hostAddress;             // 36 bytes
	XNKEY keyExchangeKey;           // 16 bytes
} XSESSION_INFO, *PXSESSION_INFO;

typedef struct _XUSER_DATA
{
	BYTE                                type;

	union
	{
		int                            nData;     // XUSER_DATA_TYPE_INT32
		int64                        i64Data;   // XUSER_DATA_TYPE_INT64
		double                          dblData;   // XUSER_DATA_TYPE_DOUBLE
		struct                                     // XUSER_DATA_TYPE_UNICODE
		{
			uint                       cbData;    // Includes null-terminator
			char *                      pwszData;
		} string;
		float                           fData;     // XUSER_DATA_TYPE_FLOAT
		struct                                     // XUSER_DATA_TYPE_BINARY
		{
			uint                       cbData;
			char *                       pbData;
		} binary;
	};
} XUSER_DATA, *PXUSER_DATA;

typedef struct _XUSER_PROPERTY
{
	DWORD                               dwPropertyId;
	XUSER_DATA                          value;
} XUSER_PROPERTY, *PXUSER_PROPERTY;

typedef struct _XUSER_CONTEXT
{
	DWORD                               dwContextId;
	DWORD                               dwValue;
} XUSER_CONTEXT, *PXUSER_CONTEXT;

typedef struct _XSESSION_SEARCHRESULT
{
	XSESSION_INFO   info;
	DWORD           dwOpenPublicSlots;
	DWORD           dwOpenPrivateSlots;
	DWORD           dwFilledPublicSlots;
	DWORD           dwFilledPrivateSlots;
	DWORD           cProperties;
	DWORD           cContexts;
	PXUSER_PROPERTY pProperties;
	PXUSER_CONTEXT  pContexts;
} XSESSION_SEARCHRESULT, *PXSESSION_SEARCHRESULT;

typedef struct _XSESSION_SEARCHRESULT_HEADER
{
	DWORD dwSearchResults;
	XSESSION_SEARCHRESULT *pResults;
} XSESSION_SEARCHRESULT_HEADER, *PXSESSION_SEARCHRESULT_HEADER;

typedef struct _XSESSION_REGISTRANT
{
	uint64 qwMachineID;
	DWORD bTrustworthiness;
	DWORD bNumUsers;
	XUID *rgUsers;

} XSESSION_REGISTRANT;

typedef struct _XSESSION_REGISTRATION_RESULTS
{
	DWORD wNumRegistrants;
	XSESSION_REGISTRANT *rgRegistrants;
} XSESSION_REGISTRATION_RESULTS, *PXSESSION_REGISTRATION_RESULTS;

typedef struct {
	BYTE        bFlags;                         
	BYTE        bReserved;                    
	WORD        cProbesXmit;                   
	WORD        cProbesRecv;                   
	WORD        cbData;                        
	BYTE *      pbData;                        
	WORD        wRttMinInMsecs;                
	WORD        wRttMedInMsecs;                
	DWORD       dwUpBitsPerSec;                
	DWORD       dwDnBitsPerSec;                
} XNQOSINFO;

typedef struct {
	uint        cxnqos;                        
	uint        cxnqosPending;                 
	XNQOSINFO   axnqosinfo[1];                 
} XNQOS;

#define XSESSION_CREATE_HOST				0
#define XSESSION_CREATE_USES_ARBITRATION	0
#define XNET_QOS_LISTEN_ENABLE				0
#define XNET_QOS_LISTEN_DISABLE				0
#define XNET_QOS_LISTEN_SET_DATA			0

#define XUSER_DATA_TYPE_CONTEXT     ((BYTE)0)
#define XUSER_DATA_TYPE_INT32       ((BYTE)1)
#define XUSER_DATA_TYPE_INT64       ((BYTE)2)
#define XUSER_DATA_TYPE_DOUBLE      ((BYTE)3)
#define XUSER_DATA_TYPE_UNICODE     ((BYTE)4)
#define XUSER_DATA_TYPE_FLOAT       ((BYTE)5)
#define XUSER_DATA_TYPE_BINARY      ((BYTE)6)
#define XUSER_DATA_TYPE_DATETIME    ((BYTE)7)
#define XUSER_DATA_TYPE_NULL        ((BYTE)0xFF)

#define XPROFILE_TITLE_SPECIFIC1    0x3FFF
#define XPROFILE_TITLE_SPECIFIC2    0x3FFE
#define XPROFILE_TITLE_SPECIFIC3    0x3FFD
#define XPROFILE_SETTING_MAX_SIZE 1000

FORCEINLINE unsigned int	XBX_GetSystemTime() { return 0; }

#ifndef PLATFORM_PS3
FORCEINLINE DWORD			XBX_GetNumGameUsers() { return 1; }
FORCEINLINE void			XBX_ProcessEvents() {}
FORCEINLINE void			XBX_DispatchEventsQueue() {}
FORCEINLINE	DWORD			XBX_GetPrimaryUserId() { return 0; }
FORCEINLINE	void			XBX_SetPrimaryUserId( DWORD idx ) {}
FORCEINLINE	void			XBX_ResetStorageDeviceInfo() {}
FORCEINLINE	DWORD			XBX_DescribeStorageDevice( DWORD nStorageID ) { return 1; }
FORCEINLINE	DWORD			XBX_GetStorageDeviceId(int) { return 0; }
FORCEINLINE	void			XBX_SetStorageDeviceId( int, DWORD ) {}
FORCEINLINE const char		*XBX_GetLanguageString() { return ""; }
FORCEINLINE bool			XBX_IsLocalized() { return false; }
FORCEINLINE bool			XBX_IsAudioLocalized() { return false; }
FORCEINLINE const char		*XBX_GetNextSupportedLanguage( const char *pLanguage, bool *pbHasAudio ) { return NULL; }
FORCEINLINE bool			XBX_IsRestrictiveLanguage() { return false; }
FORCEINLINE int				XBX_GetUserId( int nSlot ) { return nSlot; }
FORCEINLINE int				XBX_GetSlotByUserId( int idx ) { return idx; }
FORCEINLINE void			XBX_SetUserId( int nSlot, int idx ) {}
#endif


#define XCONTENT_MAX_DISPLAYNAME_LENGTH	128
#define XCONTENT_MAX_FILENAME_LENGTH	42

#ifndef _PS3
#define XBX_INVALID_STORAGE_ID ((DWORD) -1)
#define XBX_STORAGE_DECLINED ((DWORD) -2)
#endif // !_PS3

enum XUSER_SIGNIN_STATE
{
	eXUserSigninState_NotSignedIn,
	eXUserSigninState_SignedInLocally,
	eXUserSigninState_SignedInToLive,
};

#if defined( _PS3 )
#elif defined( POSIX )
typedef size_t ULONG_PTR;
#elif defined( _M_X64 )
typedef _W64 unsigned __int64 ULONG_PTR;
#else
typedef _W64 unsigned long ULONG_PTR;
#endif
typedef ULONG_PTR DWORD_PTR, *PDWORD_PTR;


typedef void * PXOVERLAPPED_COMPLETION_ROUTINE;


#if defined( _PS3 ) || !defined( POSIX )
typedef struct _XOVERLAPPED {
    ULONG_PTR                           InternalLow;
    ULONG_PTR                           InternalHigh;
    ULONG_PTR                           InternalContext;
    HANDLE                              hEvent;
    PXOVERLAPPED_COMPLETION_ROUTINE     pCompletionRoutine;
    DWORD_PTR                           dwCompletionContext;
    DWORD                               dwExtendedError;
} XOVERLAPPED, *PXOVERLAPPED;
#endif

#ifndef MAX_RICHPRESENCE_SIZE
#define MAX_RICHPRESENCE_SIZE 64
#endif

#ifndef XUSER_NAME_SIZE
#define XUSER_NAME_SIZE 16
#endif

#ifndef GPU_RESOLVE_ALIGNMENT
#define GPU_RESOLVE_ALIGNMENT 8
#endif

#define XCONTENT_MAX_DISPLAYNAME_LENGTH 128
#define XCONTENT_MAX_FILENAME_LENGTH    42
#define XCONTENTDEVICE_MAX_NAME_LENGTH  27
typedef DWORD                           XCONTENTDEVICEID, *PXCONTENTDEVICEID;
#ifndef _PS3
typedef struct _XCONTENT_DATA
{
	XCONTENTDEVICEID                    DeviceID;
	DWORD                               dwContentType;
	wchar_t                             szDisplayName[XCONTENT_MAX_DISPLAYNAME_LENGTH];
	char                                szFileName[XCONTENT_MAX_FILENAME_LENGTH];
} XCONTENT_DATA, *PXCONTENT_DATA;
#endif

#define X_CONTEXT_PRESENCE              0x00010001
#define X_CONTEXT_GAME_TYPE             0x0001000A
#define X_CONTEXT_GAME_MODE             0x0001000B

#define X_PROPERTY_RANK                 0x00011001
#define X_PROPERTY_GAMERNAME            0x00011002
#define X_PROPERTY_SESSION_ID           0x00011003

// System attributes used in matchmaking queries
#define X_PROPERTY_GAMER_ZONE           0x00011101
#define X_PROPERTY_GAMER_COUNTRY        0x00011102
#define X_PROPERTY_GAMER_LANGUAGE       0x00011103
#define X_PROPERTY_GAMER_RATING         0x00011104
#define X_PROPERTY_GAMER_MU             0x00011105
#define X_PROPERTY_GAMER_SIGMA          0x00011106
#define X_PROPERTY_GAMER_PUID           0x00011107
#define X_PROPERTY_AFFILIATE_SCORE      0x00011108
#define X_PROPERTY_GAMER_HOSTNAME       0x00011109

// Properties used to write to skill leaderboards
#define X_PROPERTY_RELATIVE_SCORE                   0x0001100A
#define X_PROPERTY_SESSION_TEAM                     0x0001100B

// Properties written at the session level to override TrueSkill parameters
#define X_PROPERTY_PLAYER_PARTIAL_PLAY_PERCENTAGE   0x0001100C
#define X_PROPERTY_PLAYER_SKILL_UPDATE_WEIGHTING_FACTOR 0x0001100D
#define X_PROPERTY_SESSION_SKILL_BETA               0x0001100E
#define X_PROPERTY_SESSION_SKILL_TAU                0x0001100F
#define X_PROPERTY_SESSION_SKILL_DRAW_PROBABILITY   0x00011010

// Attachment size is written to a leaderboard when the entry qualifies for
// a gamerclip.  The rating can be retrieved via XUserEstimateRankForRating.
#define X_PROPERTY_ATTACHMENT_SIZE                  0x00011011

// Values for X_CONTEXT_GAME_TYPE
#define X_CONTEXT_GAME_TYPE_RANKED      0
#define X_CONTEXT_GAME_TYPE_STANDARD    1

#endif // XBOXSTUBS_H
