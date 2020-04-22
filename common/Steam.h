
//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/**
** Contents:
**
**		This file provides the public interface to the Steam service.  This
**		interface is described in the SDK documentation.
**
******************************************************************************/


#ifndef INCLUDED_STEAM_H
#define INCLUDED_STEAM_H


#if defined(_MSC_VER) && (_MSC_VER > 1000)
#pragma once
#endif

#ifndef INCLUDED_STEAM2_USERID_STRUCTS
	#include "SteamCommon.h"
#endif


#ifdef __cplusplus
extern "C"
{
#endif


/******************************************************************************
**
** Exported function prototypes
**
******************************************************************************/

/*
** Engine control
**
** It is the responsibility of the Steam Bootstrapper (Steam.exe) to start
** and stop the Steam Engine at the appropriate times.
** 
** However, to properly handle some operating-system messages, the UI may need
** to call a Shutdown. For instance, WM_ENDSESSION will terminate the process after
** being handled -- so we should do as much cleanup as possible first.
*/

STEAM_API int			STEAM_CALL SteamStartEngine(TSteamError *pError);
STEAM_API int			STEAM_CALL SteamShutdownEngine(TSteamError *pError);


/*
** Initialization and misc
*/

STEAM_API int			STEAM_CALL	SteamStartup( unsigned int uUsingMask, TSteamError *pError );
STEAM_API int			STEAM_CALL	SteamCleanup( TSteamError *pError );
STEAM_API unsigned int	STEAM_CALL	SteamNumAppsRunning( TSteamError *pError );
STEAM_API void			STEAM_CALL	SteamClearError( TSteamError *pError );
STEAM_API int			STEAM_CALL	SteamGetVersion( char *szVersion, unsigned int uVersionBufSize );


/*
** Offline status
*/

STEAM_API int			STEAM_CALL	SteamGetOfflineStatus( TSteamOfflineStatus *pStatus, TSteamError *pError );
STEAM_API int			STEAM_CALL	SteamChangeOfflineStatus( TSteamOfflineStatus *pStatus, TSteamError *pError );


/*
** Asynchrounous call handling
*/

STEAM_API int				STEAM_CALL	SteamProcessCall( SteamCallHandle_t handle, TSteamProgress *pProgress, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamAbortCall( SteamCallHandle_t handle, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamBlockingCall( SteamCallHandle_t handle, unsigned int uiProcessTickMS, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamSetMaxStallCount( unsigned int uNumStalls, TSteamError *pError );
							
/*
** Filesystem
*/

STEAM_API int				STEAM_CALL	SteamMountAppFilesystem( TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamUnmountAppFilesystem( TSteamError *pError );
STEAM_API SteamHandle_t		STEAM_CALL	SteamMountFilesystem( unsigned int uAppId, const char *szMountPath, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamUnmountFilesystem( SteamHandle_t hFs, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamStat( const char *cszName, TSteamElemInfo *pInfo, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamSetvBuf( SteamHandle_t hFile, void* pBuf, ESteamBufferMethod eMethod, unsigned int uBytes, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamFlushFile( SteamHandle_t hFile, TSteamError *pError );
STEAM_API SteamHandle_t		STEAM_CALL	SteamOpenFile( const char *cszName, const char *cszMode, TSteamError *pError );
STEAM_API SteamHandle_t		STEAM_CALL	SteamOpenFileEx( const char *cszName, const char *cszMode, unsigned int *puFileSize, TSteamError *pError );
STEAM_API SteamHandle_t		STEAM_CALL	SteamOpenTmpFile( TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamCloseFile( SteamHandle_t hFile, TSteamError *pError );
STEAM_API unsigned int		STEAM_CALL	SteamReadFile( void *pBuf, unsigned int uSize, unsigned int uCount, SteamHandle_t hFile, TSteamError *pError );
STEAM_API unsigned int		STEAM_CALL	SteamWriteFile( const void *pBuf, unsigned int uSize, unsigned int uCount, SteamHandle_t hFile, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetc( SteamHandle_t hFile, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamPutc( int cChar, SteamHandle_t hFile, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamPrintFile( SteamHandle_t hFile, TSteamError *pError, const char *cszFormat, ... );
STEAM_API int				STEAM_CALL	SteamSeekFile( SteamHandle_t hFile, long lOffset, ESteamSeekMethod, TSteamError *pError );
STEAM_API long				STEAM_CALL	SteamTellFile( SteamHandle_t hFile, TSteamError *pError );
STEAM_API long				STEAM_CALL	SteamSizeFile( SteamHandle_t hFile, TSteamError *pError );
STEAM_API SteamHandle_t		STEAM_CALL	SteamFindFirst( const char *cszPattern, ESteamFindFilter eFilter, TSteamElemInfo *pFindInfo, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamFindNext( SteamHandle_t hDirectory, TSteamElemInfo *pFindInfo, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamFindClose( SteamHandle_t hDirectory, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetLocalFileCopy( const char *cszName, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamIsFileImmediatelyAvailable( const char *cszName, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamHintResourceNeed( const char *cszMasterList, int bForgetEverything, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamForgetAllHints( TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamPauseCachePreloading( TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamResumeCachePreloading( TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamWaitForResources( const char *cszMasterList, TSteamError *pError );

/*
** Logging
*/

STEAM_API SteamHandle_t		STEAM_CALL	SteamCreateLogContext( const char *cszName );
STEAM_API int				STEAM_CALL	SteamLog( SteamHandle_t hContext, const char *cszMsg );
STEAM_API void				STEAM_CALL	SteamLogResourceLoadStarted( const char *cszMsg );
STEAM_API void				STEAM_CALL	SteamLogResourceLoadFinished( const char *cszMsg );

/*
** Account
*/

STEAM_API SteamCallHandle_t	STEAM_CALL	SteamCreateAccount( const char *cszUser, const char *cszEmailAddress, const char *cszPassphrase, const char *cszCreationKey, const char *cszPersonalQuestion, const char *cszAnswerToQuestion, int *pbCreated, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamIsAccountNameInUse( const char *cszAccountName, int *pbIsUsed, TSteamError *pError);
STEAM_API SteamCallHandle_t STEAM_CALL	SteamGenerateSuggestedAccountNames( const char *cszAccountNameToSelectMasterAS, const char *cszGenerateNamesLikeAccountName, char *pSuggestedNamesBuf, unsigned int uBufSize, unsigned int *puNumSuggestedChars, TSteamError *pError);
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamDeleteAccount( TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamIsLoggedIn( int *pbIsLoggedIn, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamSetUser( const char *cszUser, int *pbUserSet, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetUser( char *szUser, unsigned int uBufSize, unsigned int *puUserChars, TSteamGlobalUserID *pOptionalReceiveUserID, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamLogin( const char *cszUser, const char *cszPassphrase, int bIsSecureComputer, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamLogout( TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamCreateCachePreloaders( TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamIsSecureComputer(  int *pbIsSecure, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamRefreshLogin( const char *cszPassphrase, int bIsSecureComputer, TSteamError * pError );
STEAM_API int				STEAM_CALL	SteamVerifyPassword( const char *cszPassphrase, int *pbCorrect, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetUserType( unsigned int *puUserTypeFlags, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetAccountStatus( unsigned int *puAccountStatusFlags, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamRefreshAccountInfo( TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamSubscribe( unsigned int uSubscriptionId, const TSteamSubscriptionBillingInfo *pSubscriptionBillingInfo, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamUnsubscribe( unsigned int uSubscriptionId, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetSubscriptionReceipt( unsigned int uSubscriptionId, TSteamSubscriptionReceipt *pSubscriptionReceipt, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamAckSubscriptionReceipt( unsigned int uSubscriptionId, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamIsSubscribed( unsigned int uSubscriptionId, int *pbIsSubscribed, int *pbIsSubscriptionPending, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamIsAppSubscribed( unsigned int uAppId, int *pbIsAppSubscribed, int *pbIsSubscriptionPending, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetSubscriptionStats( TSteamSubscriptionStats *pSubscriptionStats, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetSubscriptionIds( unsigned int *puIds, unsigned int uMaxIds, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamEnumerateSubscription( unsigned int uId, TSteamSubscription *pSubscription, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamEnumerateSubscriptionDiscount( unsigned int uSubscriptionId, unsigned int uDiscountIndex, TSteamSubscriptionDiscount *pDiscount, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamEnumerateSubscriptionDiscountQualifier( unsigned int uSubscriptionId, unsigned int uDiscountIndex, unsigned int uQualifierIndex, TSteamDiscountQualifier *pDiscountQualifier, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetAppStats( TSteamAppStats *pAppStats, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetAppIds( unsigned int *puIds, unsigned int uMaxIds, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamEnumerateApp( unsigned int uId, TSteamApp *pApp, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamEnumerateAppLaunchOption( unsigned int uAppId, unsigned int uLaunchOptionIndex, TSteamAppLaunchOption *pLaunchOption, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamEnumerateAppIcon( unsigned int uAppId, unsigned int uIconIndex, unsigned char *pIconData, unsigned int uIconDataBufSize, unsigned int *puSizeOfIconData, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamEnumerateAppVersion( unsigned int uAppId, unsigned int uVersionIndex, TSteamAppVersion *pAppVersion, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamEnumerateAppDependency( unsigned int uAppId, unsigned int uIndex, TSteamAppDependencyInfo *pDependencyInfo, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamInsertAppDependency( unsigned int uAppId, unsigned int uIndex, TSteamAppDependencyInfo *pDependencyInfo, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamRemoveAppDependency( unsigned int uAppId, unsigned int uIndex, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamFindApp( const char *cszAppName, unsigned int *puAppId, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetAppDependencies( unsigned int uAppId, unsigned int *puCacheIds, unsigned int uMaxIds, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetAppUserDefinedInfo( unsigned int uAppId, const char *cszKey, char *szValueBuf, unsigned int uValueBufLen, unsigned int *puValueLen, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamWaitForAppReadyToLaunch( unsigned int uAppId, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamLaunchApp( unsigned int uAppId, unsigned int uLaunchOption, const char *cszArgs, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamIsCacheLoadingEnabled( unsigned int uAppId, int *pbIsLoading, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetCacheFilePath( unsigned int uCacheId, char *szPathBuf, unsigned int uBufSize, unsigned int *puPathChars, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamStartLoadingCache( unsigned int uAppId, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamStopLoadingCache( unsigned int uAppId, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamFlushCache( unsigned int uCacheId, TSteamError *pError );
STEAM_API SteamCallHandle_t STEAM_CALL	SteamRepairOrDecryptCaches( unsigned int uAppId, int bForceValidation, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamLoadCacheFromDir( unsigned int uAppId, const char *szPath, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetCacheDefaultDirectory( char *szPath, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamSetCacheDefaultDirectory( const char *szPath, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetAppDir( unsigned int uAppId, char *szPath, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamMoveApp( unsigned int uAppId, const char *szPath, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamGetAppCacheSize( unsigned int uCacheId, unsigned int *pCacheSizeInMb, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamSetAppCacheSize( unsigned int uCacheId, unsigned int nCacheSizeInMb, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamSetAppVersion( unsigned int uAppId, unsigned int uAppVersionId, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamUninstall( TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamSetNotificationCallback( SteamNotificationCallback_t pCallbackFunction, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamChangeForgottenPassword( const char *cszUser, const char *cszAnswerToQuestion, const char *cszEmailVerificationKey, const char *cszNewPassphrase, int *pbChanged, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamRequestForgottenPasswordEmail( const char *cszUser, SteamPersonalQuestion_t ReceivePersonalQuestion, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamRequestAccountsByEmailAddressEmail( const char *cszEmailAddress, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamRequestAccountsByCdKeyEmail( const char *cszCdKey, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamGetNumAccountsWithEmailAddress( const char *cszEmailAddress, unsigned int *puNumAccounts, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamChangePassword( const char *cszCurrentPassphrase, const char *cszNewPassphrase, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamChangePersonalQA( const char *cszCurrentPassphrase, const char *cszNewPersonalQuestion, const char *cszNewAnswerToQuestion, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetCurrentEmailAddress( char *szEmailAddress, unsigned int uBufSize, unsigned int *puEmailChars, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamChangeEmailAddress( const char *cszNewEmailAddress, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamVerifyEmailAddress( const char *cszEmailVerificationKey, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamRequestEmailAddressVerificationEmail( TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamChangeAccountName( const char *cszCurrentPassphrase, const char *cszNewAccountName, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamUpdateAccountBillingInfo( const TSteamPaymentCardInfo *pPaymentCardInfo, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamUpdateSubscriptionBillingInfo( unsigned int uSubscriptionId, const TSteamSubscriptionBillingInfo *pSubscriptionBillingInfo, TSteamError *pError );
STEAM_API int				STEAM_CALL  SteamGetSponsorUrl( unsigned int uAppId, char *szUrl, unsigned int uBufSize, unsigned int *pUrlChars, TSteamError *pError );
STEAM_API int				STEAM_CALL  SteamGetContentServerInfo( unsigned int uAppId, unsigned int *puServerId, unsigned int *puServerIpAddress, TSteamError *pError );
STEAM_API SteamCallHandle_t	STEAM_CALL	SteamGetAppUpdateStats( unsigned int uAppOrCacheId, ESteamAppUpdateStatsQueryType eQueryType, TSteamUpdateStats *pUpdateStats, TSteamError *pError );
STEAM_API int				STEAM_CALL	SteamGetTotalUpdateStats( TSteamUpdateStats *pUpdateStats, TSteamError *pError );

/*
** User ID exported functions are in SteamUserIdValidation.h
*/


#ifdef __cplusplus
}
#endif

#endif /* #ifndef INCLUDED_STEAM_H */
