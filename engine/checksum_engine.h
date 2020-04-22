//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Engine crc routines
//
// $NoKeywords: $
//=============================================================================//
#ifndef CHECKSUM_H
#define CHECKSUM_H
#ifdef _WIN32
#pragma once
#endif

#include "checksum_crc.h"
#include "checksum_md5.h"
#include "quakedef.h"

bool CRC_File(CRC32_t *crcvalue, const char *pszFileName);
byte COM_BlockSequenceCRCByte (byte *base, int length, int sequence);
bool CRC_MapFile(CRC32_t *crcvalue, const char *pszFileName);

bool MD5_Hash_File(unsigned char digest[16], const char *pszFileName, bool bSeed, unsigned int seed[4]);

#endif // CHECKSUM_H
