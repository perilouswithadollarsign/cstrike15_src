//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose: XBox HAL - Game to hardware abstraction
//
//=============================================================================
#pragma once

#define XBX_BREAK(x)					if (x) {DebugBreak();}

//#define XBX_MAX_DPORTS					4
// PS3 supports up to 7 controllers!
#define XUSER_MAX_COUNT					7
#define XBX_MAX_EVENTS					32
#define XBX_MAX_BUTTONSAMPLE			32768
#define XBX_MAX_ANALOGSAMPLE			255
#define XBX_MAX_MESSAGE					1024
#define XBX_MAX_PATH					MAX_PATH
#define XBX_MAX_RCMDLENGTH				256
#define XBX_MAX_RCMDNAMELEN				32
#define XBX_HDD_CLUSTERSIZE				16384

#define XBX_INVALID_STORAGE_ID			((DWORD)(-1))
#define XBX_STORAGE_DECLINED			((DWORD)(-2))
#define XBX_INVALID_USER_ID				((DWORD)(-1))

#define XBX_USER_SETTINGS_CONTAINER_DRIVE	"CFG"
#define XBX_USER_SAVES_CONTAINER_DRIVE		"SAV"

typedef uint64_t XUID;
#define IsEqualXUID( a, b ) ( ( a ) == ( b ) )

typedef struct {
	BYTE        ab[8];                          // xbox to xbox key identifier
} XNKID;

typedef struct {
	BYTE        ab[16];                         // xbox to xbox key exchange key
} XNKEY;

#define XCONTENT_MAX_DISPLAYNAME_LENGTH 128
#define XCONTENT_MAX_FILENAME_LENGTH    42
#define XCONTENTDEVICE_MAX_NAME_LENGTH  27
typedef DWORD                           XCONTENTDEVICEID, *PXCONTENTDEVICEID;
typedef struct _XCONTENT_DATA
{
	XCONTENTDEVICEID                    DeviceID;
	DWORD                               dwContentType;
	wchar_t                             szDisplayName[XCONTENT_MAX_DISPLAYNAME_LENGTH];
	char                                szFileName[XCONTENT_MAX_FILENAME_LENGTH];
} XCONTENT_DATA, *PXCONTENT_DATA;

// could be dvd or hdd, actual device depends on source of xbe launch
#define XBX_DVD_DRIVE					"D:\\"
#define XBX_BOOT_DRIVE					"D:\\"
#define XBX_SWAP_DIRECTORY				"T:\\HL2\\HL2X\\SWAP\\"	
#define XBX_PERSISTENT_DIR				"T:\\HL2\\"
#define XBX_HDD_SAVE_DIRECTORY			"U:\\"

#define XBX_IOTHREAD_STACKSIZE			32768
#define XBX_IOTHREAD_PRIORITY			THREAD_PRIORITY_HIGHEST

#define XBX_SCREEN_WIDTH				640
#define XBX_SCREEN_HEIGHT				480
#define XBOX_MINBORDERSAFE				0.04f
#define XBOX_MAXBORDERSAFE				0.04f

#ifndef GPU_RESOLVE_ALIGNMENT
#define GPU_RESOLVE_ALIGNMENT 8
#endif

#define	XBX_CALCSIG_TYPE				XCALCSIG_FLAG_NON_ROAMABLE

#define XBX_VIRTUAL_BASEDIR				"r:\\hl2"
#define XBX_GAMEDIR						"hl2x"

#if defined( _DEBUG )
#define XBX_XBE_BASE_FILENAME			"hl2d_xbox.xbe"
#elif defined( _RELEASE )
#define XBX_XBE_BASE_FILENAME			"hl2r_xbox.xbe"
#else
#define XBX_XBE_BASE_FILENAME			"hl2_xbox.xbe"
#endif

// Path to our running executable
#define XBX_XBE_PATH					XBX_BOOT_DRIVE XBX_XBE_BASE_FILENAME

#define CLR_DEFAULT						0xFF000000
#define CLR_WARNING						0x0000FFFF
#define CLR_ERROR						0x000000FF

#define XBX_ALIGN(x,y)					(((x)+((y)-1))&~((y)-1))

// disk space requirements
#define HL2_SAVEIMAGE_BYTES				( 1024 * 70 )
#define	HL2_SAVEGAME_BYTES				( 1024 * 1024 * 10 )
#define	HL2_CONFIGFILE_BYTES			XBX_HDD_CLUSTERSIZE

#define HL2_PERSISTENT_BYTES_NEEDED		( HL2_CONFIGFILE_BYTES + HL2_SAVEGAME_BYTES * 2 )
#define HL2_USERSAVE_BYTES_NEEDED		( HL2_SAVEGAME_BYTES + HL2_SAVEIMAGE_BYTES + XBX_GetSaveGameOverhead() )

#define FILE_BEGIN           0
#define FILE_CURRENT         1
#define FILE_END             2

typedef enum
{
	XC_NULL,
	XC_NORMAL,
	XC_IBEAM,
	XC_WAIT,
	XC_CROSS,
	XC_UP,
	XC_SIZENWSE,
	XC_SIZENESW,
	XC_SIZEWE,
	XC_SIZENS,
	XC_SIZEALL,
	XC_NO,
	XC_HAND,
	XC_MAXCURSORS,
} xCursor_e;

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

typedef enum
{
	XVRB_NONE,		// off
	XVRB_ERROR,		// fatal error
	XVRB_ALWAYS,	// no matter what
	XVRB_WARNING,	// non-fatal warnings
	XVRB_STATUS,	// status reports
	XVRB_ALL,
} xverbose_e;

typedef enum
{
	XOF_READ     = 0x01,	// read access
	XOF_WRITE	 = 0x02,	// write access
	XOF_CREATE	 = 0x04,	// create if not exist
} xopenfile_e;

typedef enum
{
	XSF_SET = FILE_BEGIN,
	XSF_CUR = FILE_CURRENT,
	XSF_END = FILE_END,	
} xseekfile_e;

typedef enum
{
	XFA_LOCALONLY,
	XFA_REMOTEONLY,
	XFA_LOCALFIRST,
} xFileAccess_e;

typedef enum
{
	XEV_NULL,
	XEV_KEY,
	XEV_REMOTECMD,
	XEV_LISTENER_NOTIFICATION,
	XEV_QUIT,
	XEV_GAMEPAD_UNPLUGGED,
	XEV_GAMEPAD_INSERTED,
} xevent_e;

typedef struct xevent_s
{
	xevent_e	event;
	int			arg1;
	int			arg2;
	int			arg3;
	uint64		sysutil_status;
	uint64		sysutil_param;
} xevent_t;

typedef struct xevent_SYS_SIGNINCHANGED_s
{
	XUID xuid[ XUSER_MAX_COUNT ];
	int state[ XUSER_MAX_COUNT ];
	DWORD dwParam;
} xevent_SYS_SIGNINCHANGED_t;

typedef struct ps3syscbckeventhdlr_s
{
	int (*pfnHandler)( xevent_t const &ev );
} ps3syscbckeventhdlr_t;

typedef enum
{
	MDIR_NULL   = 0x00,
	MDIR_UP		= 0x01,
	MDIR_DOWN	= 0x02,
	MDIR_LEFT	= 0x04,
	MDIR_RIGHT	= 0x08,
} xMouseDir_e;

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

/******************************************************************************
	XBX_CONSOLE.CPP
******************************************************************************/
extern int XBX_rAddCommands(int numCommands, const char* commands[], const char* help[]);
extern int XBX_rTextureList(int nTextures, const xTextureList_t* pXTextureList);
extern int XBX_rTimeStampLog(float time, const char *pString);
#define    XBX_rMaterialList  if ( !g_pValvePS3Console ); else g_pValvePS3Console->MaterialList       //(int nMaterials, const xMaterialList_t* pXMaterialList);
// inline int XBX_rSoundList(int nSounds, const xSoundList_t* pXSoundList) { return g_pValvePS3Console ? g_pValvePS3Console->SoundList( nSounds, pXSoundList ) : -1 ; }
#define		XBX_rSoundList if ( !g_pValvePS3Console ); else g_pValvePS3Console->SoundList
extern int XBX_rMemDump( int nStatsID );
#define	   XBX_rMapInfo if ( !g_pValvePS3Console ) ; else g_pValvePS3Console->MapInfo  //( xMapInfo_t *pMapInfo ) { return g_pValvePS3Console ? g_pValvePS3Console->MapInfo( pMapInfo ) : -1 ; }

/******************************************************************************
	XBX_DEBUG.CPP
******************************************************************************/
// extern void XBX_SendRemoteCommand(const char* dbgCommand, bool async);
#define XBX_SendRemoteCommand			if ( !g_pValvePS3Console ) ; else g_pValvePS3Console->SendRemoteCommand
#define XBX_SendPrefixedMsg				if ( !g_pValvePS3Console ) ; else g_pValvePS3Console->SendPrefixedDECIMessage
// extern void XBX_DebugString(xverbose_e verbose, COLORREF color, const char* format, ...);
#define XBX_DebugString					if ( !g_pValvePS3Console ) ; else g_pValvePS3Console->DebugString
extern void XBX_SetVerbose(xverbose_e verbose);
extern void	XBX_InitDebug(void);
extern void XBX_Log( const char *pString );
extern bool g_xbx_bNoVXConsole;

/******************************************************************************
	XBX_DEVICES.CPP
******************************************************************************/
extern void XBX_InitDevices(void);
extern void XBX_SampleDevices(void);
extern void XBX_SetRumble( float fLeftMotor, float fRightMotor );
extern void XBX_StopRumble( void );
extern void XBX_SetActiveController( int port );
extern int	XBX_GetActiveController();
extern bool	XBX_IsControllerValid( int port );

/******************************************************************************
	XBX_DISPLAY.CPP
******************************************************************************/
extern u32_t					XBX_GetDisplayTime(void);
extern void						XBX_CreateDisplay(void);
extern void						XBX_BeginFrame(void);
extern void						XBX_EndFrame(void);
//EAPS3extern void						XBX_HookD3DDevice(IDirect3DDevice8* pD3DDevice);
//EAPS3extern IDirect3DDevice8			*g_xbx_pD3DDevice;
extern u32_t					g_xbx_numVBlanks;
extern u32_t					g_xbx_frameTime;
extern int						g_xbx_numFrames;

/******************************************************************************
	XBX_FILEIO.CPP
******************************************************************************/
enum xFileDevice_e
{
	XFD_NULL,
	XFD_LOCALHDD,
	XFD_REMOTEHDD,
	XFD_DVDROM,
	XFD_TITLE_PERSISTENT_HDD,
};

enum XFileMode_t
{
	XFM_BINARY,
	XFM_TEXT
};

//EAPS3 : XBox structure
struct WIN32_FIND_DATA
{

};

//EAPS3 : XBox structure
struct XGAME_FIND_DATA
{

};

extern void			XBX_FixupFilename(const char* input, char* output, xFileDevice_e& outFileDevice, bool bWarnInvalid = true);
extern void			XBX_SetRemotePath(const char* remotePath);
extern void			XBX_SetLocalPath(const char* localPath);
extern const char	*XBX_GetLocalPath();
extern FILE*		XBX_fopen(const char* filename, const char* options);
extern int 			XBX_setvbuf( FILE *fp, char *,int mode, size_t size );
extern int			XBX_fclose(FILE* fp);
extern int			XBX_fseek(FILE *fp, long pos, int seekType);
extern long			XBX_ftell(FILE *fp);
extern int			XBX_feof(FILE *fp);
extern size_t		XBX_fread(void *dest, size_t size, size_t count, FILE *fp);
extern size_t		XBX_fwrite(const void *src, size_t size, size_t count, FILE *fp);
extern bool			XBX_setmode( FILE *fp, int mode );
extern size_t		XBX_vfprintf(FILE *fp, const char *fmt, va_list list);
extern int			XBX_ferror(FILE *fp);
extern int			XBX_fflush(FILE *fp);
extern char*		XBX_fgets(char *dest, int destSize, FILE *fp);
extern int			XBX_stat(const char *path, struct _stat *buf);
//extern int			XBX_unlink(const char* filename);
extern int			XBX_rename(const char* pszFrom, const char* pszTo);
extern HANDLE		XBX_FindFirstFile(const char *findname, WIN32_FIND_DATA *dat);
extern BOOL			XBX_FindNextFile(HANDLE handle, WIN32_FIND_DATA *dat);
extern BOOL			XBX_FindClose(HANDLE handle);
extern void			XBX_SetFileAccess(xFileAccess_e mode);
extern void			XBX_EnableFileSync(bool bSync);
//extern int			XBX_mkdir( const char * );
extern void			XBX_rFileSync(const char *pFileName);
extern DWORD		XBX_GetSigSize( DWORD sigType );
extern void			XBX_CalculateSignature( BYTE *buff, void *pSig, DWORD buffSize, DWORD sigType );
extern bool			XBX_ValidateSignature( BYTE *pBuffer, DWORD size, DWORD sigType );
extern bool			g_xbx_bFileSync;
extern DWORD		XBX_GetSigSize( DWORD sigType );
extern void			XBX_CalculateSignature( BYTE *buff, void *pSig, DWORD buffSize, DWORD sigType );
extern bool			XBX_ValidateSignature( BYTE *pBuffer, DWORD size, DWORD sigType );
extern bool			XBX_SaveFileExists( const wchar_t *pName, XGAME_FIND_DATA *fileData );
extern bool			XBX_SaveNumberExists( const int number, XGAME_FIND_DATA *fileData );

/******************************************************************************
	XBX_MEMORY.CPP
******************************************************************************/
extern void			XBX_InitMemory(void);
extern void			XBX_EnableMemoryTrace(bool enable);
extern unsigned int	g_xbx_memoryD3DCost;

/******************************************************************************
	XBX_PROFILE.CPP
******************************************************************************/
class xVProfNodeItem_t;
extern int	XBX_rSetProfileAttributes(const char *pProfileName, int numCounters, const char *names[], COLORREF colors[]);
extern void XBX_rSetProfileData( const char *pProfileName, int numCounters, unsigned int *counters );
extern void XBX_rVProfNodeList( int nItems, const xVProfNodeItem_t *pItems );

/******************************************************************************
	XBX_SYSTEM.CPP
******************************************************************************/
extern void			XBX_StringCopyToWide( WCHAR *pDst, const char *pSrc );
extern u32_t		XBX_GetSystemTime(void);
ULARGE_INTEGER		XBX_GetFreeBytes( const char *drive );
extern unsigned int XBX_GetBlocksNeeded( const char *drive, DWORD bytesRequested );
extern DWORD		XBX_GetSaveGameOverhead( void );
extern void			XBX_Error(const char* format, ...);
extern void			XBX_Init();
extern bool			XBX_IsRetailMode();
extern void			XBX_RelaunchHL2( unsigned int contextCode = 0, const char *pszArgs = "", void *pRelaunchData = NULL, unsigned nBytesRelaunchData = 0 );
extern bool			XBX_GetRelaunchContext( unsigned int *pContextCode, __int64 *pStartTime );
extern bool			XBX_GetRelaunchData( void *pRelaunchData, unsigned maxBytes );
extern void			XBX_LaunchInstaller( unsigned int contextCode );
extern void			XBX_LaunchDashboard( char chDrive, int spaceNeeded );
extern LPSTR		g_xbx_pCmdLine;
extern const char*	g_xbx_version;
extern bool			XBX_NoXBDM();

typedef HRESULT (STDCALL *DmSendNotificationStringFunc_t)(LPCSTR sz);
typedef HRESULT (STDCALL *DmRegisterCommandProcessorFunc_t)(LPCSTR szProcessor, PVOID /*HACK:/needsport/  WAS:PDM_CMDPROC /Vitaliy/ */ pfn);
typedef HRESULT (STDCALL *DmCaptureStackBackTraceFunc_t)(ULONG FramesToCapture, PVOID *BackTrace);
typedef BOOL (STDCALL *DmIsDebuggerPresentFunc_t)(void);

extern DmSendNotificationStringFunc_t CallDmSendNotificationString;
extern DmRegisterCommandProcessorFunc_t CallDmRegisterCommandProcessor;
extern DmCaptureStackBackTraceFunc_t CallDmCaptureStackBackTrace;
extern DmIsDebuggerPresentFunc_t CallDmIsDebuggerPresent;


/******************************************************************************
	XBX_EVENTS.CPP
******************************************************************************/

// Event handling
PLATFORM_INTERFACE  bool		XBX_NotifyCreateListener( uint64_t categories );
PLATFORM_INTERFACE	void		XBX_QueueEvent( xevent_e event, int arg1, int arg2, int arg3 );
PLATFORM_INTERFACE	void		XBX_ProcessEvents( void );
PLATFORM_INTERFACE	void		XBX_DispatchEventsQueue( void );

// Accessors
PLATFORM_INTERFACE	const char* XBX_GetLanguageString( void );
PLATFORM_INTERFACE	bool		XBX_IsLocalized( void );
PLATFORM_INTERFACE	bool		XBX_IsAudioLocalized( void );
PLATFORM_INTERFACE	const char *XBX_GetNextSupportedLanguage( const char *pLanguage, bool *pbHasAudio );
PLATFORM_INTERFACE	bool		XBX_IsRestrictiveLanguage( void );
PLATFORM_INTERFACE	u32_t		XBX_GetImageChangelist( void );

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

