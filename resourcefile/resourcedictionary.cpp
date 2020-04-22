//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "resourcefile/resourcedictionary.h"
#include "tier1/generichash.h"
#include "tier0/dbg.h"
#include "tier2/fileutils.h"

// Must be last
#include "tier0/memdbgon.h"


#define RESOURCE_ID_HASH_SEED 0xEDABCDEF


//-----------------------------------------------------------------------------
// Computes a resource id given a resource name
//-----------------------------------------------------------------------------
ResourceId_t ComputeResourceIdHash( const char *pResourceName )
{
	if ( !pResourceName || !pResourceName[0] )
		return RESOURCE_ID_INVALID;
	int nLength = Q_strlen( pResourceName );
	return (ResourceId_t)MurmurHash2( pResourceName, nLength, RESOURCE_ID_HASH_SEED );
}
				
void GenerateResourceFileName( const char *pFileName, char *pResourceFileName, size_t nBufLen )
{
	char pContentName[MAX_PATH];
	char pContentNameNoExt[MAX_PATH];
	char pFixedContentName[MAX_PATH];
	if ( Q_IsAbsolutePath( pFileName ) )
	{
		ComputeModContentFilename( pFileName, pContentName, sizeof(pContentName) );
		Q_StripExtension( pContentName, pContentNameNoExt, sizeof(pContentNameNoExt) );
		Q_FixupPathName( pFixedContentName, sizeof(pFixedContentName), pContentNameNoExt );
		g_pFullFileSystem->FullPathToRelativePathEx( pFixedContentName, "CONTENT", pResourceFileName, nBufLen );
	}
	else
	{
		Q_StripExtension( pFileName, pContentNameNoExt, sizeof(pContentNameNoExt) );
		Q_FixupPathName( pResourceFileName, nBufLen, pContentNameNoExt );
	}
}

void GenerateResourceName( const char *pFileName, const char *pSubResourceName, char *pResourceName, size_t pBufLen )
{
	if ( !pSubResourceName )
	{
		pSubResourceName = "";
	}

	char pFixedFileName[MAX_PATH];
	char pFixedSubResourceName[MAX_PATH];
	GenerateResourceFileName( pFileName, pFixedFileName, sizeof(pFixedFileName) );
	Q_strncpy( pFixedSubResourceName, pSubResourceName, sizeof(pFixedSubResourceName) );
	Q_strlower( pFixedSubResourceName );
	Q_snprintf( pResourceName, pBufLen, "%s::%s", pFixedFileName, pFixedSubResourceName );
}

ResourceId_t ComputeResourceIdHash( const char *pFileName, const char *pSubResourceName )
{
	if ( !pFileName || !pFileName[0] )
		return RESOURCE_ID_INVALID;

	char pTemp[MAX_PATH+128];
	GenerateResourceName( pFileName, pSubResourceName, pTemp, sizeof(pTemp) );
	return ComputeResourceIdHash( pTemp );
}
