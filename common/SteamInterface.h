
/************ (C) Copyright 2004 Valve Corporation, All rights reserved. ***********
**
** The copyright to the contents herein is the property of Valve Corporation.
** The contents may be used and/or copied only with the written permission of
** Valve, or in accordance with the terms and conditions stipulated in
** the agreement/contract under which the contents have been supplied.
**
*******************************************************************************
**
** Contents:
**
**		This file provides an obfuscated interface to the Steam service.  This
**		interface is described in the SDK documentation.
**
******************************************************************************/


#ifndef INCLUDED_STEAM_INTERFACE_H
#define INCLUDED_STEAM_INTERFACE_H


#if defined(_MSC_VER) && (_MSC_VER > 1000)
#pragma once
#endif

#ifndef INCLUDED_STEAM2_USERID_STRUCTS
	#include "SteamCommon.h"
#endif

// IAppSystem interface. Returns an IAppSystem implementation; use QueryInterface on
// that to get the ISteamInterface (same as the older _f function).
#define STEAMDLL_APPSYSTEM_VERSION "SteamDLLAppsystem001"
// extern "C" STEAM_API void * STEAM_CALL CreateInterface( const char *pName, int *pReturncode );

// create interface
#define STEAM_INTERFACE_VERSION "Steam006"
extern "C" STEAM_API void * STEAM_CALL _f(const char *szInterfaceVersionRequested);

// current abstract interface
class ISteamInterface
{
public:
	virtual ~ISteamInterface() {};

	virtual SteamCallHandle_t	ChangePassword( const char *cszCurrentPassphrase, const char *cszNewPassphrase, TSteamError *pError ) = 0;
	virtual int					GetCurrentEmailAddress( char *szEmailAddress, unsigned int uBufSize, unsigned int *puEmailChars, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	ChangePersonalQA( const char *cszCurrentPassphrase, const char *cszNewPersonalQuestion, const char *cszNewAnswerToQuestion, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	ChangeEmailAddress( const char *cszNewEmailAddress, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	VerifyEmailAddress( const char *cszEmailVerificationKey, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	RequestEmailAddressVerificationEmail( TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	ChangeAccountName( const char *cszCurrentPassphrase, const char *cszNewAccountName, TSteamError *pError ) = 0;
	
	virtual int					MountAppFilesystem( TSteamError *pError ) = 0;
	virtual int					UnmountAppFilesystem( TSteamError *pError ) = 0;
	virtual SteamHandle_t		MountFilesystem( unsigned int uAppId, const char *szMountPath, TSteamError *pError ) = 0;
	virtual int					UnmountFilesystem( SteamHandle_t hFs, TSteamError *pError ) = 0;
	virtual int					Stat( const char *cszName, TSteamElemInfo *pInfo, TSteamError *pError ) = 0;
	virtual int					SetvBuf( SteamHandle_t hFile, void* pBuf, ESteamBufferMethod eMethod, unsigned int uBytes, TSteamError *pError ) = 0;
	virtual int					FlushFile( SteamHandle_t hFile, TSteamError *pError ) = 0;
	virtual SteamHandle_t		OpenFile( const char *cszName, const char *cszMode, TSteamError *pError ) = 0;
	virtual SteamHandle_t		OpenFileEx( const char *cszName, const char *cszMode, int nFlags, unsigned int *puFileSize, int *pbLocal, TSteamError *pError ) = 0;
	virtual SteamHandle_t		OpenTmpFile( TSteamError *pError ) = 0;

	virtual void			ClearError( TSteamError *pError ) = 0;
	virtual int				GetVersion( char *szVersion, unsigned int uVersionBufSize ) = 0;

	virtual int				GetOfflineStatus( TSteamOfflineStatus *pStatus, TSteamError *pError ) = 0;
	virtual int				ChangeOfflineStatus( TSteamOfflineStatus *pStatus, TSteamError *pError ) = 0;

	virtual int				ProcessCall( SteamCallHandle_t handle, TSteamProgress *pProgress, TSteamError *pError ) = 0;
	virtual int				AbortCall( SteamCallHandle_t handle, TSteamError *pError ) = 0;
	virtual int				BlockingCall( SteamCallHandle_t handle, unsigned int uiProcessTickMS, TSteamError *pError ) = 0;
	virtual int				SetMaxStallCount( unsigned int uNumStalls, TSteamError *pError ) = 0;

	virtual int					CloseFile( SteamHandle_t hFile, TSteamError *pError ) = 0;
	virtual unsigned int		ReadFile( void *pBuf, unsigned int uSize, unsigned int uCount, SteamHandle_t hFile, TSteamError *pError ) = 0;
	virtual unsigned int		WriteFile( const void *pBuf, unsigned int uSize, unsigned int uCount, SteamHandle_t hFile, TSteamError *pError ) = 0;
	virtual int					Getc( SteamHandle_t hFile, TSteamError *pError ) = 0;
	virtual int					Putc( int cChar, SteamHandle_t hFile, TSteamError *pError ) = 0;
	//virtual int					PrintFile( SteamHandle_t hFile, TSteamError *pError, const char *cszFormat, ... ) = 0;
	virtual int					SeekFile( SteamHandle_t hFile, long lOffset, ESteamSeekMethod, TSteamError *pError ) = 0;
	virtual long				TellFile( SteamHandle_t hFile, TSteamError *pError ) = 0;
	virtual long				SizeFile( SteamHandle_t hFile, TSteamError *pError ) = 0;
	virtual SteamHandle_t		FindFirst( const char *cszPattern, ESteamFindFilter eFilter, TSteamElemInfo *pFindInfo, TSteamError *pError ) = 0;
	virtual int					FindNext( SteamHandle_t hDirectory, TSteamElemInfo *pFindInfo, TSteamError *pError ) = 0;
#if !defined( _X360 ) // X360TBD: Macro defined in winbase.h 
	virtual int					FindClose( SteamHandle_t hDirectory, TSteamError *pError ) = 0;
#endif
	virtual int					GetLocalFileCopy( const char *cszName, TSteamError *pError ) = 0;
	virtual int					IsFileImmediatelyAvailable( const char *cszName, TSteamError *pError ) = 0;
	virtual int					HintResourceNeed( const char *cszMasterList, int bForgetEverything, TSteamError *pError ) = 0;
	virtual int					ForgetAllHints( TSteamError *pError ) = 0;
	virtual int					PauseCachePreloading( TSteamError *pError ) = 0;
	virtual int					ResumeCachePreloading( TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	WaitForResources( const char *cszMasterList, TSteamError *pError ) = 0;

	virtual int				StartEngine(TSteamError *pError) = 0;
	virtual int				ShutdownEngine(TSteamError *pError) = 0;

	virtual int				Startup( unsigned int uUsingMask, TSteamError *pError ) = 0;
	virtual int				Cleanup( TSteamError *pError ) = 0;

	virtual unsigned int	NumAppsRunning( TSteamError *pError ) = 0;

	virtual SteamCallHandle_t	CreateAccount( const char *cszUser, const char *cszEmailAddress, const char *cszPassphrase, const char *cszCreationKey, const char *cszPersonalQuestion, const char *cszAnswerToQuestion, int *pbCreated, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t 	GenerateSuggestedAccountNames( const char *cszAccountNameToSelectMasterAS, const char *cszGenerateNamesLikeAccountName, char *pSuggestedNamesBuf, unsigned int uBufSize, unsigned int *puNumSuggestedChars, TSteamError *pError) = 0;
	virtual int					IsLoggedIn( int *pbIsLoggedIn, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	Logout( TSteamError *pError ) = 0;
	virtual int					IsSecureComputer(  int *pbIsSecure, TSteamError *pError ) = 0;

	virtual SteamHandle_t		CreateLogContext( const char *cszName ) = 0;
	virtual int					Log( SteamHandle_t hContext, const char *cszMsg ) = 0;
	virtual void				LogResourceLoadStarted( const char *cszMsg ) = 0;
	virtual void				LogResourceLoadFinished( const char *cszMsg ) = 0;

	virtual SteamCallHandle_t	RefreshLogin( const char *cszPassphrase, int bIsSecureComputer, TSteamError * pError ) = 0;
	virtual int					VerifyPassword( const char *cszPassphrase, int *pbCorrect, TSteamError *pError ) = 0;
	virtual int					GetUserType( unsigned int *puUserTypeFlags, TSteamError *pError ) = 0;
	virtual int					GetAppStats( TSteamAppStats *pAppStats, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	IsAccountNameInUse( const char *cszAccountName, int *pbIsUsed, TSteamError *pError) = 0;
	virtual int					GetAppIds( unsigned int *puIds, unsigned int uMaxIds, TSteamError *pError ) = 0;
	virtual int					GetSubscriptionStats( TSteamSubscriptionStats *pSubscriptionStats, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	RefreshAccountInfo( int bContentDescriptionOnly, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	Subscribe( unsigned int uSubscriptionId, const TSteamSubscriptionBillingInfo *pSubscriptionBillingInfo, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	Unsubscribe( unsigned int uSubscriptionId, TSteamError *pError ) = 0;
	virtual int					GetSubscriptionReceipt( unsigned int uSubscriptionId, TSteamSubscriptionReceipt *pSubscriptionReceipt, TSteamError *pError ) = 0;
	virtual int					GetAccountStatus( unsigned int *puAccountStatusFlags, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	SetUser( const char *cszUser, int *pbUserSet, TSteamError *pError ) = 0;
	virtual int					GetUser( char *szUser, unsigned int uBufSize, unsigned int *puUserChars, TSteamGlobalUserID *pOptionalReceiveUserID, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	Login( const char *cszUser, const char *cszPassphrase, int bIsSecureComputer, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	AckSubscriptionReceipt( unsigned int uSubscriptionId, TSteamError *pError ) = 0;
	virtual int					IsAppSubscribed( unsigned int uAppId, int *pbIsAppSubscribed, int *pbIsSubscriptionPending, TSteamError *pError ) = 0;
	virtual int					GetSubscriptionIds( unsigned int *puIds, unsigned int uMaxIds, TSteamError *pError ) = 0;
	virtual int					EnumerateSubscription( unsigned int uId, TSteamSubscription *pSubscription, TSteamError *pError ) = 0;
	virtual int					EnumerateSubscriptionDiscount( unsigned int uSubscriptionId, unsigned int uDiscountIndex, TSteamSubscriptionDiscount *pDiscount, TSteamError *pError ) = 0;
	virtual int					EnumerateSubscriptionDiscountQualifier( unsigned int uSubscriptionId, unsigned int uDiscountIndex, unsigned int uQualifierIndex, TSteamDiscountQualifier *pDiscountQualifier, TSteamError *pError ) = 0;
	virtual int					EnumerateApp( unsigned int uId, TSteamApp *pApp, TSteamError *pError ) = 0;
	virtual int					EnumerateAppLaunchOption( unsigned int uAppId, unsigned int uLaunchOptionIndex, TSteamAppLaunchOption *pLaunchOption, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	DeleteAccount( TSteamError *pError ) = 0;
	virtual int					EnumerateAppIcon( unsigned int uAppId, unsigned int uIconIndex, unsigned char *pIconData, unsigned int uIconDataBufSize, unsigned int *puSizeOfIconData, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	LaunchApp( unsigned int uAppId, unsigned int uLaunchOption, const char *cszArgs, TSteamError *pError ) = 0;
	virtual int					GetCacheFilePath( unsigned int uCacheId, char *szPathBuf, unsigned int uBufSize, unsigned int *puPathChars, TSteamError *pError ) = 0;
	virtual int					EnumerateAppVersion( unsigned int uAppId, unsigned int uVersionIndex, TSteamAppVersion *pAppVersion, TSteamError *pError ) = 0;
	virtual int					EnumerateAppDependency( unsigned int uAppId, unsigned int uIndex, TSteamAppDependencyInfo *pDependencyInfo, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	StartLoadingCache( unsigned int uAppId, TSteamError *pError ) = 0;
	virtual int					InsertAppDependency( unsigned int uAppId, unsigned int uIndex, TSteamAppDependencyInfo *pDependencyInfo, TSteamError *pError ) = 0;
	virtual int					RemoveAppDependency( unsigned int uAppId, unsigned int uIndex, TSteamError *pError ) = 0;
	virtual int					FindApp( const char *cszAppName, unsigned int *puAppId, TSteamError *pError ) = 0;
	virtual int					GetAppDependencies( unsigned int uAppId, unsigned int *puCacheIds, unsigned int uMaxIds, TSteamError *pError ) = 0;
	virtual int					IsSubscribed( unsigned int uSubscriptionId, int *pbIsSubscribed, int *pbIsSubscriptionPending, TSteamError *pError ) = 0;
	virtual int					GetAppUserDefinedInfo( unsigned int uAppId, const char *cszKey, char *szValueBuf, unsigned int uValueBufLen, unsigned int *puValueLen, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	WaitForAppReadyToLaunch( unsigned int uAppId, TSteamError *pError ) = 0;
	virtual int					IsCacheLoadingEnabled( unsigned int uAppId, int *pbIsLoading, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	StopLoadingCache( unsigned int uAppId, TSteamError *pError ) = 0;

	virtual ESteamError		GetEncryptedUserIDTicket
		(
		const void *							pEncryptionKey, 
		unsigned int							uKeyLength, 
		void *									pOutputBuffer, 
		unsigned int							uSizeOfOutputBuffer, 
		unsigned int *							pReceiveSizeOfEncryptedTicket,
		TSteamError *							pReceiveErrorCode
		) = 0;

	virtual SteamCallHandle_t	FlushCache( unsigned int uCacheId, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t 	RepairOrDecryptCaches( unsigned int uAppId, int bForceValidation, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	LoadCacheFromDir( unsigned int uAppId, const char *szPath, TSteamError *pError ) = 0;
	virtual int					GetCacheDefaultDirectory( char *szPath, TSteamError *pError ) = 0;
	virtual int					SetCacheDefaultDirectory( const char *szPath, TSteamError *pError ) = 0;
	virtual int					GetAppDir( unsigned int uAppId, char *szPath, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	MoveApp( unsigned int uAppId, const char *szPath, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	GetAppCacheSize( unsigned int uCacheId, unsigned int *pCacheSizeInMb, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	SetAppCacheSize( unsigned int uCacheId, unsigned int nCacheSizeInMb, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	SetAppVersion( unsigned int uAppId, unsigned int uAppVersionId, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	Uninstall( TSteamError *pError ) = 0;
	virtual int					SetNotificationCallback( SteamNotificationCallback_t pCallbackFunction, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	ChangeForgottenPassword( const char *cszUser, const char *cszAnswerToQuestion, const char *cszEmailVerificationKey, const char *cszNewPassphrase, int *pbChanged, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	RequestForgottenPasswordEmail( const char *cszUser, SteamPersonalQuestion_t ReceivePersonalQuestion, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	RequestAccountsByEmailAddressEmail( const char *cszEmailAddress, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	RequestAccountsByCdKeyEmail( const char *cszCdKey, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	GetNumAccountsWithEmailAddress( const char *cszEmailAddress, unsigned int *puNumAccounts, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	UpdateAccountBillingInfo( const TSteamPaymentCardInfo *pPaymentCardInfo, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	UpdateSubscriptionBillingInfo( unsigned int uSubscriptionId, const TSteamSubscriptionBillingInfo *pSubscriptionBillingInfo, TSteamError *pError ) = 0;
	virtual int					GetSponsorUrl( unsigned int uAppId, char *szUrl, unsigned int uBufSize, unsigned int *pUrlChars, TSteamError *pError ) = 0;
	virtual int					GetContentServerInfo( unsigned int uAppId, unsigned int *puServerId, unsigned int *puServerIpAddress, TSteamError *pError ) = 0;
	virtual SteamCallHandle_t	GetAppUpdateStats( unsigned int uAppOrCacheId, ESteamAppUpdateStatsQueryType eQueryType, TSteamUpdateStats *pUpdateStats, TSteamError *pError ) = 0;
	virtual int					GetTotalUpdateStats( TSteamUpdateStats *pUpdateStats, TSteamError *pError ) = 0;

	virtual SteamCallHandle_t	CreateCachePreloaders( TSteamError *pError ) = 0;

	virtual void				Win32SetMiniDumpComment( const char *comment ) = 0;
	virtual void				Win32SetMiniDumpSourceControlId( unsigned int SourcecontrolID ) = 0;
	virtual void				Win32SetMiniDumpEnableFullMemory() = 0;
	virtual void				Win32WriteMiniDump(	const char * szErrorOrAssertType, const char * szDescriptionOrAssertName, const char * szAssertExpr, const char * szAssertFilename, unsigned int uAssertLineNumber ) = 0;

	virtual int					GetCurrentAppId( unsigned int *puAppId, TSteamError *pError ) = 0;

	virtual int					GetAppPurchaseCountry( unsigned int uAppId, char *szCountry, unsigned int uBufSize, int * pPurchaseTime, TSteamError *pError ) = 0;

	virtual int					GetLocalClientVersion( unsigned int *puBootstrapperVersion, unsigned int *puClientVersion, TSteamError *pError ) = 0;

	virtual int					IsFileNeededByCache( unsigned int uCacheId, const char *pchFileName, unsigned int uFileSize, TSteamError *pError ) = 0;
	virtual int					LoadFileToCache( unsigned int uCacheId, const char *pchFileName, const void *pubDataChunk, unsigned int cubDataChunk, unsigned int cubDataOffset, TSteamError *pError ) = 0;
	virtual int					GetCacheDecryptionKey( unsigned int uCacheId, char *pchKeyBuffer, unsigned int cubBuff, unsigned int *pcubKey, TSteamError *pError ) = 0;

	virtual int					GetSubscriptionExtendedInfo( unsigned int uSubscriptionId, const char *cszKey, char *szValueBuf, unsigned int uValueBufLen, unsigned *puValueLen, TSteamError *pError ) = 0;

	virtual int					GetSubscriptionPurchaseCountry( unsigned int uSubscriptionId, char *szCountry, unsigned int uBufSize, int * pPurchaseTime, TSteamError *pError ) = 0;

	virtual int					GetAppUserDefinedRecord( unsigned int uAppid, KeyValueIteratorCallback_t pIterationCallback, void *pvParam, TSteamError *pError ) = 0;

	virtual int					FindServersNumServers(ESteamServerType eServerType) = 0;

	// Get nth ipaddr:port for this server type
	// buffer needs to be 22 chars long: aaa.bbb.ccc.ddd:12345 plus null
	//
	// returns 0 if succsessful, negative is error
	virtual int					FindServersIterateServer(ESteamServerType eServerType, unsigned int nServer, char *szIpAddrPort, int szIpAddrPortLen) = 0;

	virtual const char *		FindServersGetErrorString() = 0;
};


#endif
