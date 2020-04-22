
/************ (C) Copyright 2002 Valve, L.L.C. All rights reserved. ***********
**
** The copyright to the contents herein is the property of Valve, L.L.C.
** The contents may be used and/or copied only with the written permission of
** Valve, L.L.C., or in accordance with the terms and conditions stipulated in
** the agreement/contract under which the contents have been supplied.
**
*******************************************************************************
**
** Contents:
**
**		This file provides the public interface to the Steam service.  This
**		interface is described in the SDK documentation.
**
******************************************************************************/


#ifndef INCLUDED_STEAMWRITEMINIDUMP_H
#define INCLUDED_STEAMWRITEMINIDUMP_H

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

#ifdef _WIN32
/*
** Write minidump using result of GetExceptionInformation() in an __except block
*/
STEAM_API void STEAM_CALL	SteamWriteMiniDumpUsingExceptionInfo
	( 
	unsigned int			uStructuredExceptionCode, 
	struct _EXCEPTION_POINTERS * pExceptionInfo
	);

STEAM_API void STEAM_CALL	SteamWriteMiniDumpUsingExceptionInfoWithBuildId
	( 
	unsigned int			uStructuredExceptionCode, 
	struct _EXCEPTION_POINTERS * pExceptionInfo,
	unsigned int uBuildID
	);

STEAM_API void STEAM_CALL	SteamWriteMiniDumpSetComment
	(
	const char *cszComment
	);

#endif // _WIN32

#ifdef __cplusplus
}
#endif

#endif
