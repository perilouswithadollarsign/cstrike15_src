
//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*******************************************************************************
**
** Contents:
**
**		This file provides the public interface to the Steam service.  This
**		interface is described in the SDK documentation.
**
******************************************************************************/


#ifndef INCLUDED_STEAMUSERIDVALIDATION_H
#define INCLUDED_STEAMUSERIDVALIDATION_H


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

/*
** User ID
*/

/* Client calls this (see also ValidateNewValveCDKeyClient.h if appropriate) */
STEAM_API ESteamError	STEAM_CALL	SteamGetEncryptedUserIDTicket
									(
										const void *							pEncryptionKeyReceivedFromAppServer, 
										unsigned int							uEncryptionKeyLength, 
										void *									pOutputBuffer, 
										unsigned int							uSizeOfOutputBuffer, 
										unsigned int *							pReceiveSizeOfEncryptedTicket,
										TSteamError *							pReceiveErrorCode
									);


/* Game/Application server calls these */
STEAM_API ESteamError	STEAM_CALL	SteamInitializeUserIDTicketValidator
									(
										const char *							pszOptionalPublicEncryptionKeyFilename,
										const char *							pszOptionalPrivateDecryptionKeyFilename,
										unsigned int							ClientClockSkewToleranceInSeconds,
										unsigned int							ServerClockSkewToleranceInSeconds,
										unsigned int							MaxNumLoginsWithinClientClockSkewTolerancePerClient,
										unsigned int							HintPeakSimultaneousValidations,
										unsigned int							AbortValidationAfterStallingForNProcessSteps
									);

STEAM_API ESteamError	STEAM_CALL	SteamShutdownUserIDTicketValidator();

STEAM_API const char *	STEAM_CALL	SteamGetEncryptionKeyToSendToNewClient
									(
										unsigned int *							pReceiveSizeOfEncryptionKey
									);

STEAM_API ESteamError	STEAM_CALL	SteamStartValidatingUserIDTicket
									( 
										void *									pEncryptedUserIDTicketFromClient,
										unsigned int							uSizeOfEncryptedUserIDTicketFromClient,
										unsigned int							ObservedClientIPAddr,
										SteamUserIDTicketValidationHandle_t *	pReceiveHandle
									);

STEAM_API ESteamError	STEAM_CALL	SteamStartValidatingNewValveCDKey
									( 
										void *									pEncryptedNewValveCDKeyFromClient,
										unsigned int							uSizeOfEncryptedNewValveCDKeyFromClient,
										unsigned int							ObservedClientIPAddr,
										struct sockaddr *						pPrimaryValidateNewCDKeyServerSockAddr,
										struct sockaddr *						pSecondaryValidateNewCDKeyServerSockAddr,
										SteamUserIDTicketValidationHandle_t *	pReceiveHandle
									);


STEAM_API ESteamError	STEAM_CALL	SteamProcessOngoingUserIDTicketValidation
									( 
										SteamUserIDTicketValidationHandle_t		Handle,
										TSteamGlobalUserID *					pReceiveValidSteamGlobalUserID,
										unsigned int *							pReceiveClientLocalIPAddr,
										unsigned char *							pOptionalReceiveProofOfAuthenticationToken,
										size_t									SizeOfOptionalAreaToReceiveProofOfAuthenticationToken,
										size_t *								pOptionalReceiveSizeOfProofOfAuthenticationToken
									);

STEAM_API void			STEAM_CALL	SteamAbortOngoingUserIDTicketValidation
									(
										SteamUserIDTicketValidationHandle_t		Handle
									);

STEAM_API ESteamError	STEAM_CALL	SteamOptionalCleanUpAfterClientHasDisconnected
									( 
										unsigned int							ObservedClientIPAddr,
										unsigned int							ClientLocalIPAddr
									);


#ifdef __cplusplus
}
#endif

#endif /* #ifndef INCLUDED_STEAM2_USERID_STRUCTS */
