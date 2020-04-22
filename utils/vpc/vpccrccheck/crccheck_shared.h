//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//
//
//==================================================================================================

#ifndef CRCCHECK_SHARED_H
#define CRCCHECK_SHARED_H
#ifdef _WIN32
#pragma once
#endif


#define VPCCRCCHECK_EXE_FILENAME	"vpc.exe"

// The file extension for the file that contains the CRCs that a vcproj depends on.
#define VPCCRCCHECK_FILE_EXTENSION		"vpc_crc"
#define VPCCRCCHECK_FILE_VERSION_STRING	"[vpc crc file version 3]"

void Sys_Error( PRINTF_FORMAT_STRING const char *format, ... );
int Sys_LoadTextFileWithIncludes( const char* filename, char** bufferptr, bool bInsertFileMacroExpansion );

bool VPC_CheckProjectDependencyCRCs( const char *szCRCFile, const char *pReferenceSupplementalString, char *pErrorString, int nErrorStringLength );

// Used by vpccrccheck.exe or by vpc.exe to do the CRC check that's initiated in the pre-build steps.
int VPC_CommandLineCRCChecks( int argc, char **argv );


#endif // CRCCHECK_SHARED_H
