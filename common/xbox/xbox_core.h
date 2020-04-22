//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose: XBox Core definitions
//
//=============================================================================
#pragma once

#define XBOX_DONTCARE					0		// for functions with don't care params

#define XBX_MAX_DPORTS					4
#define XBX_MAX_BUTTONSAMPLE			32768
#define XBX_MAX_ANALOGSAMPLE			255
#define XBX_MAX_MESSAGE					2048
#define XBX_MAX_PATH					MAX_PATH
#define XBX_MAX_RCMDLENGTH				256
#define XBX_MAX_RCMDNAMELEN				32
#define XBX_HDD_CLUSTERSIZE				16384

// could be dvd or hdd, actual device depends on source of xex launch
#define XBX_DVD_DRIVE					"D:\\"
#define XBX_BOOT_DRIVE					"D:\\"

#define XBX_IOTHREAD_STACKSIZE			32768
#define XBX_IOTHREAD_PRIORITY			THREAD_PRIORITY_HIGHEST

// scale by screen dimension to get an inset
#define XBOX_MINBORDERSAFE				0.05f
#define XBOX_MAXBORDERSAFE				0.075f

#define	XBX_CALCSIG_TYPE				XCALCSIG_FLAG_NON_ROAMABLE
#define XBX_INVALID_STORAGE_ID			((DWORD)-1)
#define XBX_STORAGE_DECLINED			((DWORD)-2)
#define XBX_INVALID_USER_ID				((DWORD)-1)

#define XBX_USER_SETTINGS_CONTAINER_DRIVE	"CFG"
#define XBX_USER_SAVES_CONTAINER_DRIVE		"SAV"

// Path to our running executable
#define XBX_XEX_BASE_FILENAME			"default.xex"
#define XBX_XEX_PATH					XBX_BOOT_DRIVE XBX_XEX_BASE_FILENAME

#define XBX_CLR_DEFAULT					0xFF000000
#define XBX_CLR_WARNING					0x0000FFFF
#define XBX_CLR_ERROR					0x000000FF

// disk space requirements
#define XBX_SAVEGAME_BYTES				( 1024 * 1024 * 2 )
#define XBX_CONFIGFILE_BYTES			( 1024 * 100 )
#define XBX_USER_STATS_BYTES			( 1024 * 28 )
#define XBX_USER_SETTINGS_BYTES			( XBX_CONFIGFILE_BYTES + XBX_USER_STATS_BYTES )

#define XBX_PERSISTENT_BYTES_NEEDED		( XBX_SAVEGAME_BYTES * 10 )	// 8 save games, 1 autosave, 1 autosavedangerous

#define XMAKECOLOR( r, g, b )			((unsigned int)(((unsigned char)(r)|((unsigned int)((unsigned char)(g))<<8))|(((unsigned int)(unsigned char)(b))<<16)))

#define MAKE_NON_SRGB_FMT(x)			((D3DFORMAT)( ((unsigned int)(x)) & ~(D3DFORMAT_SIGNX_MASK | D3DFORMAT_SIGNY_MASK | D3DFORMAT_SIGNZ_MASK)))
#define IS_D3DFORMAT_SRGB( x )			( MAKESRGBFMT(x) == (x) )

typedef enum
{
	XEV_NULL,
	XEV_REMOTECMD,
	XEV_QUIT,
	XEV_LISTENER_NOTIFICATION,
} xevent_e;

typedef struct xevent_s
{
	xevent_e	event;
	int			arg1;
	int			arg2;
	int			arg3;
} xevent_t;

typedef struct xevent_SYS_SIGNINCHANGED_s
{
	XUID xuid[ XUSER_MAX_COUNT ];
	XUSER_SIGNIN_STATE state[ XUSER_MAX_COUNT ];
	DWORD dwParam;
} xevent_SYS_SIGNINCHANGED_t;

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
	XK_BUTTON_INACTIVE_START, // Special key that is passed through on disabled controllers
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

typedef struct
{
	const char	*pName;
	const char	*pGroupName;
	const char	*pFormatName;
	int			size;
	int			width;
	int			height;
	int			depth;
	int			numLevels;
	int			binds;
	int			refCount;
	int			sRGB;
	int			edram;
	int			procedural;
	int			cacheableState;
	int			cacheableSize;
	int			final;
	int			failed;
	int			pwl;
	int			reduced;
} xTextureList_t;

typedef struct
{
	const char	*pName;
	const char	*pShaderName;
	int			refCount;
} xMaterialList_t;

typedef struct
{
	char		name[MAX_PATH];
	char		formatName[32];
	int			rate;
	int			bits;
	int			channels;
	int			looped;
	int			dataSize;
	int			numSamples;
	int			streamed;
	int			quality;
} xSoundList_t;

typedef struct
{
	float	position[3];
	float	angle[3];
	char	mapPath[256];
	char	savePath[256];
	int		build;
	int		skill;
	char	details[1024];
} xMapInfo_t;

typedef struct
{
	int		BSPSize;
} xBudgetInfo_t;

struct xModelList_t
{
	char		name[MAX_PATH];
	int			dataSize;
	int			numVertices;
	int			triCount;
	int			dataSizeLod0;
	int			numVerticesLod0;
	int			triCountLod0;
	int			numBones;
	int			numParts;
	int			numLODs;
	int			numMeshes;
};

struct xDataCacheItem_t
{
	char			name[MAX_PATH];
	char			section[64];
	int				size;
	int				lockCount;
	unsigned int	clientId;
	unsigned int	itemData;
	unsigned int	handle;
};

struct xVProfNodeItem_t
{
	const char		*pName;
	const char		*pBudgetGroupName;
	unsigned int	budgetGroupColor;
	unsigned int	totalCalls;
	double			inclusiveTime;
	double			exclusiveTime;
};

/******************************************************************************
	XBOX_SYSTEM.CPP
******************************************************************************/
#if defined( PLATFORM_H )

// redirect debugging output through xbox debug channel
#define OutputDebugStringA		XBX_OutputDebugStringA

// Messages
PLATFORM_INTERFACE	void		XBX_Error( const char* format, ... );
PLATFORM_INTERFACE	void		XBX_OutputDebugStringA( LPCSTR lpOutputString );

// Event handling
PLATFORM_INTERFACE  bool		XBX_NotifyCreateListener( ULONG64 categories );
PLATFORM_INTERFACE	void		XBX_QueueEvent( xevent_e event, int arg1, int arg2, int arg3 );
PLATFORM_INTERFACE	void		XBX_ProcessEvents( void );
PLATFORM_INTERFACE	void		XBX_DispatchEventsQueue( void );

// Accessors
PLATFORM_INTERFACE	const char* XBX_GetLanguageString( void );
PLATFORM_INTERFACE	bool		XBX_IsLocalized( void );
PLATFORM_INTERFACE	bool		XBX_IsAudioLocalized( void );
PLATFORM_INTERFACE	const char *XBX_GetNextSupportedLanguage( const char *pLanguage, bool *pbHasAudio );
PLATFORM_INTERFACE	bool		XBX_IsRestrictiveLanguage( void );

//
// Storage devices management
//
PLATFORM_INTERFACE	void		XBX_ResetStorageDeviceInfo();
PLATFORM_INTERFACE	DWORD		XBX_DescribeStorageDevice( DWORD nStorageID );
PLATFORM_INTERFACE  char const *XBX_MakeStorageContainerRoot( int iController, char const *szRootName, char *pBuffer, int numBufferBytes );

PLATFORM_INTERFACE	DWORD		XBX_GetStorageDeviceId( int iController );
PLATFORM_INTERFACE	void		XBX_SetStorageDeviceId( int iController, DWORD id );

//
// Information about game primary user
//
PLATFORM_INTERFACE	DWORD		XBX_GetPrimaryUserId( void );
PLATFORM_INTERFACE	void		XBX_SetPrimaryUserId( DWORD id );

PLATFORM_INTERFACE  DWORD		XBX_GetPrimaryUserIsGuest( void );
PLATFORM_INTERFACE	void		XBX_SetPrimaryUserIsGuest( DWORD bPrimaryUserIsGuest );

//
// Disabling and enabling input from controllers
//
PLATFORM_INTERFACE void			XBX_ResetUserIdSlots();
PLATFORM_INTERFACE void			XBX_ClearUserIdSlots();

//
// Mapping between game slots and controllers
//
PLATFORM_INTERFACE int			XBX_GetUserId( int nSlot );
PLATFORM_INTERFACE int 			XBX_GetSlotByUserId( int idx );
PLATFORM_INTERFACE void			XBX_SetUserId( int nSlot, int idx );
PLATFORM_INTERFACE void			XBX_ClearSlot( int nSlot );
PLATFORM_INTERFACE void			XBX_ClearUserId( int idx );

PLATFORM_INTERFACE DWORD		XBX_GetUserIsGuest( int nSlot );
PLATFORM_INTERFACE void			XBX_SetUserIsGuest( int nSlot, DWORD dwUserIsGuest );

//
// Number of game users
//
PLATFORM_INTERFACE  DWORD		XBX_GetNumGameUsers( void );
PLATFORM_INTERFACE  void		XBX_SetNumGameUsers( DWORD numGameUsers );

//
// Invite related functions
//
PLATFORM_INTERFACE  XNKID		XBX_GetInviteSessionId( void );
PLATFORM_INTERFACE	void		XBX_SetInviteSessionId( XNKID nSessionId );
PLATFORM_INTERFACE  XUID		XBX_GetInvitedUserXuid( void );
PLATFORM_INTERFACE	void		XBX_SetInvitedUserXuid( XUID xuid );
PLATFORM_INTERFACE  DWORD		XBX_GetInvitedUserId( void );
PLATFORM_INTERFACE	void		XBX_SetInvitedUserId( DWORD nUserId );


#endif
