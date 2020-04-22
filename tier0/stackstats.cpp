//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $NoKeywords: $
//===========================================================================//

#include "pch_tier0.h"
#include "tier0/stackstats.h"
#include "tier0/threadtools.h"
#include "tier0/icommandline.h"

#include "tier0/valve_off.h"

#if defined( PLATFORM_WINDOWS_PC )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#endif

#if defined( PLATFORM_X360 )
#include <xbdm.h>
#include "xbox/xbox_console.h"
#include "xbox/xbox_vxconsole.h"
#include <map>
#include <set>
#endif

#include "tier0/valve_on.h"

#include "tier0/memdbgon.h"


#if !defined( ENABLE_RUNTIME_STACK_TRANSLATION )
bool _CCallStackStatsGatherer_Internal_DumpStatsToFile( const char *szFileName, const CCallStackStatsGatherer_Standardized_t &StatsGatherer, bool bAllowMemoryAllocations )
{
	return false;
}
#else //#if !defined( ENABLE_RUNTIME_STACK_TRANSLATION )

static void BufferedFwrite( FILE *pFile, uint8 *pBuffer, size_t iBufferSize, const void *pData, size_t iDataSize, size_t &iWriteMarker )
{
	//write to buffer until it's full, then write to file
	if( (iWriteMarker + iDataSize) > iBufferSize )
	{
		//too big for the buffer, flush the buffer and try again
		if( iWriteMarker != 0 )
		{
			fwrite( pBuffer, 1, iWriteMarker, pFile );
			iWriteMarker = 0;
		}

		if( iDataSize > iBufferSize )
		{
			//too big to ever hold in the buffer, write it now
			fwrite( pData, iDataSize, 1, pFile );
			return;
		}
	}

	memcpy( &pBuffer[iWriteMarker], pData, iDataSize );
	iWriteMarker += iDataSize;
}

struct CCallStackStatsGatherer_DumpHelperVars_t
{
	uint8 *pWriteBuffer;
	size_t iWriteBufferSize;
	size_t *iWriteMarker; 
	FILE *pFile;
	bool bAllowMemoryAllocations;
};

bool _CCallStackStatsGatherer_Internal_DumpSubTree( const CCallStackStatsGatherer_Standardized_t &StatsGatherer, void *pDumpHelpers )
{
	size_t CapturedCallStackLength = 0;
	size_t iEntrySizeWithCallStack = 0;
	void *pEntries = NULL;
	size_t iEntryCount = 0;
	CCallStackStatsGatherer_Standardized_t *pSubTrees = NULL;
	size_t iSubTreeCount = 0;
	const char *szStructName = "";
	StatsGatherer.pFunctionTable->pfn_GetDumpInfo( StatsGatherer.pGatherer, szStructName, CapturedCallStackLength, iEntrySizeWithCallStack, pEntries, iEntryCount, pSubTrees, iSubTreeCount );

	if( iEntryCount == 0 )
		return false; //nothing to write

	CCallStackStatsGatherer_DumpHelperVars_t *pHelpers = (CCallStackStatsGatherer_DumpHelperVars_t *)pDumpHelpers;
	uint8 *pWriteBuffer = pHelpers->pWriteBuffer;
	size_t iWriteBufferSize = pHelpers->iWriteBufferSize;
	size_t &iWriteMarker = *pHelpers->iWriteMarker;
	FILE *pFile = pHelpers->pFile;

	//struct header
	{
		size_t iStructNameLength = strlen( szStructName ) + 1;
		BufferedFwrite( pFile, pWriteBuffer, iWriteBufferSize, szStructName, iStructNameLength, iWriteMarker );

		//number of sub-trees
		uint32 iSubTreeCount32 = (uint32)iSubTreeCount;
		BufferedFwrite( pFile, pWriteBuffer, iWriteBufferSize, &iSubTreeCount32, sizeof( uint32 ), iWriteMarker );

		//sub-tree translation addresses
		for( size_t i = 0; i != iSubTreeCount; ++i )
		{
			//sub-trees will be written out immediately after we finish dumping this gatherer, depth first recursion
			BufferedFwrite( pFile, pWriteBuffer, iWriteBufferSize, &pSubTrees[i].pGatherer, sizeof( void * ), iWriteMarker );
		}

		//size of call stack array
		BufferedFwrite( pFile, pWriteBuffer, iWriteBufferSize, &CapturedCallStackLength, sizeof( size_t ), iWriteMarker );

		//size of each entry, including call stack
		BufferedFwrite( pFile, pWriteBuffer, iWriteBufferSize, &iEntrySizeWithCallStack, sizeof( size_t ), iWriteMarker );

		//flush the write buffer
		if( iWriteMarker != 0 )
		{
			fwrite( pWriteBuffer, 1, iWriteMarker, pFile );
			iWriteMarker = 0;
		}

		//reserve space for writing out the description blob size since we don't know how big it is until after we write it.
		size_t iStructDescSizeWriteMarker = iWriteMarker;
		iWriteMarker += sizeof( uint32 );

		//describe how the structure should be interpreted on the receiving end
		iWriteMarker += StatsGatherer.pFunctionTable->pfn_DescribeCallStackStatStruct( pWriteBuffer + iWriteMarker, iWriteBufferSize - iWriteMarker );

		//write out the description size now
		*(uint32 *)(pWriteBuffer + iStructDescSizeWriteMarker) = (uint32)(iWriteMarker - (iStructDescSizeWriteMarker + sizeof( uint32 )));
	}

	size_t iInfoPos;		

	//stats entries
	{
		//number of entries
		BufferedFwrite( pFile, pWriteBuffer, iWriteBufferSize, &iEntryCount, sizeof( size_t ), iWriteMarker );

		iInfoPos = iWriteMarker + ftell( pFile ); //where in the file we wrote out the stats entries. Need this in case we need to repurpose the stats memory for sorting later

		//write them all out
		BufferedFwrite( pFile, pWriteBuffer, iWriteBufferSize, pEntries, iEntrySizeWithCallStack * iEntryCount, iWriteMarker );
	}

	//flush any remnants of the write buffer. We're going to repurpose it for a bit
	if( iWriteMarker != 0 )
	{
		fwrite( pWriteBuffer, 1, iWriteMarker, pFile );
		iWriteMarker = 0;
	}


	//now sort the individual addresses, throwing away duplicates. We need memory for this operation, but we might be in a state where we can't allocate any...
	//So we'll start with the stack memory used for buffered writing first, then overflow into a memory allocation that should be able to hold it all if we're allowed, 
	//but if that fails we're going to repurpose our stat tracking memory we already own and just wrote to file. Then read in what we overwrote after we're done.
	int iUniqueAddresses = 0;
	int iSortSize = (int)iWriteBufferSize / sizeof( void * );
	void **pSortedAddresses = (void **)pWriteBuffer;
	{
		uint8 *pEntryRead = (uint8 *)pEntries;
		for( uint32 i = 0; i != iEntryCount; ++i )
		{
			for( size_t j = 0; j != CapturedCallStackLength; ++j )
			{
				void *pInsertAddress = ((void **)pEntryRead)[j];
				if( pInsertAddress == NULL )
					break;

				//binary search
				int iHigh = iUniqueAddresses - 1;
				int iLow = 0;
				while( iHigh >= iLow )
				{
					int iMid = (iHigh + iLow) >> 1;
					if( pSortedAddresses[iMid] > pInsertAddress )
					{
						iHigh = iMid - 1;
					}
					else if( pSortedAddresses[iMid] < pInsertAddress )
					{
						iLow = iMid + 1;
					}
					else
					{
						//same address
						iLow = iMid;
						//iHigh = iLow - 1;
						break;
					}
				}
				if( iLow > iUniqueAddresses )
				{
					//tack it onto the end
					if( iUniqueAddresses >= iSortSize )
					{
						size_t maxSize = sizeof( void * ) * CapturedCallStackLength * iEntryCount;
						//crap, grew past the temp stack buffer, use real memory...
						void **pTemp = pHelpers->bAllowMemoryAllocations ? new void * [maxSize] : NULL;
						if( pTemp == NULL )
						{
							//double crap, memory wasn't available, overwrite our stat tracking data and read it back from file later...
							pTemp = (void **)pEntries;
						}

						memcpy( pTemp, pSortedAddresses, iUniqueAddresses * sizeof( void * ) );
						iSortSize = (int)maxSize;
						pSortedAddresses = pTemp;
					}
					pSortedAddresses[iLow] = pInsertAddress;
					++iUniqueAddresses;
				}
				else if( pSortedAddresses[iLow] != pInsertAddress )
				{
					//insert it here
					if( iUniqueAddresses >= iSortSize )
					{
						size_t maxSize = sizeof( void * ) * CapturedCallStackLength * iEntryCount;
						//crap, grew past the temp stack buffer, use real memory...
						void **pTemp = pHelpers->bAllowMemoryAllocations ? new void * [maxSize] : NULL;
						if( pTemp == NULL )
						{
							//double crap, memory wasn't available, overwrite our stat tracking data and read it back from file later...
							pTemp = (void **)pEntries;
						}

						memcpy( pTemp, pSortedAddresses, iUniqueAddresses * sizeof( void * ) );
						iSortSize = (int)maxSize;
						pSortedAddresses = pTemp;
					}
					for( int k = iUniqueAddresses; --k >= iLow; )
					{
						pSortedAddresses[k + 1] = pSortedAddresses[k];
					}
					++iUniqueAddresses;
					pSortedAddresses[iLow] = pInsertAddress;
				}
				//else do nothing, it's a duplicate
#ifdef DBGFLAG_ASSERT
				else
				{
					Assert( pSortedAddresses[iLow] == pInsertAddress );				
				}
#endif				
			}

			pEntryRead += iEntrySizeWithCallStack;
		}
	}

	//now that we have them sorted, see if we need to fudge with the memory we own a bit...
	if( (uint8 *)pSortedAddresses == pWriteBuffer )
	{
		size_t iUniqueAddressSpace = iUniqueAddresses * sizeof( void * );
		pWriteBuffer += iUniqueAddressSpace;
		iWriteBufferSize -= iUniqueAddressSpace;
	}

	//write the address to file/symbol/line translation table
	{
		Assert( iUniqueAddresses > 0 );
		BufferedFwrite( pFile, pWriteBuffer, iWriteBufferSize, &iUniqueAddresses, sizeof( int ), iWriteMarker );

		//now that they're sorted, we can assume that we have a basic grouping by symbol. And on top of that we can assume that a unique symbol is contained 
		//in one file. But we're not going to use that information just yet...

		//for first version, we'll store all the info for every address. It's excessive but stable. A future file protocol revision should try recognize file/symbol duplicates. The problem is finding them without using mallocs()
		for( int i = 0; i != iUniqueAddresses; ++i )
		{
			BufferedFwrite( pFile, pWriteBuffer, iWriteBufferSize, &pSortedAddresses[i], sizeof( void * ), iWriteMarker );

			char szBuff[1024];
			if( !GetModuleNameFromAddress( pSortedAddresses[i], szBuff, sizeof( szBuff ) ) )
			{
				szBuff[0] = '\0';
			}
			BufferedFwrite( pFile, pWriteBuffer, iWriteBufferSize, szBuff, strlen( szBuff ) + 1, iWriteMarker );

			if( !GetSymbolNameFromAddress( pSortedAddresses[i], szBuff, sizeof( szBuff ) ) )
			{
				szBuff[0] = '\0';
			}
			BufferedFwrite( pFile, pWriteBuffer, iWriteBufferSize, szBuff, strlen( szBuff ) + 1, iWriteMarker );

			uint32 iLine;
			if( !GetFileAndLineFromAddress( pSortedAddresses[i], szBuff, sizeof( szBuff ), iLine ) )
			{
				szBuff[0] = '\0';
				iLine = 0;
			}
			BufferedFwrite( pFile, pWriteBuffer, iWriteBufferSize, szBuff, strlen( szBuff ) + 1, iWriteMarker );
			BufferedFwrite( pFile, pWriteBuffer, iWriteBufferSize, &iLine, sizeof( uint32 ), iWriteMarker );
		}
	}

	uint32 iSubToolDataSize = 0; //there's generally a lot of data to process in this file, save some space for any tools that want to save data back to the file in a lump format
	BufferedFwrite( pFile, pWriteBuffer, iWriteBufferSize, &iSubToolDataSize, sizeof( uint32 ), iWriteMarker );

	//flush any remnants of the write buffer
	if( iWriteMarker != 0 )
	{
		fwrite( pWriteBuffer, 1, iWriteMarker, pFile );
		iWriteMarker = 0;
	}

	pWriteBuffer = pHelpers->pWriteBuffer;
	iWriteBufferSize = pHelpers->iWriteBufferSize;

	

	if( pSortedAddresses == (void **)pEntries )
	{
		// We trashed the data after we wrote it. We needed the memory for sorting unique addresses. Fix it by reading what we overwrote back into place
		fseek( pFile, (long)iInfoPos, SEEK_SET );
		fread( pEntries, sizeof( void * ) * iUniqueAddresses, 1, pFile ); //we overwrote iUniqueAddresses worth of void pointers, so we just need to fix that chunk
		fseek( pFile, 0, SEEK_END );
	}
	else if( (uint8 *)pSortedAddresses != pWriteBuffer )
	{
		//we overflowed the temp buffer, but got away with an allocation. Free it
		delete []pSortedAddresses;
	}

	//dump sub-trees, depth first
	for( size_t i = 0; i != iSubTreeCount; ++i )
	{
		bool bRet = _CCallStackStatsGatherer_Internal_DumpSubTree( pSubTrees[i], pDumpHelpers );
		if( bRet == false )
		{
			return false;
		}
	}

	return true;
}

size_t _CCallStackStatsGatherer_Write_FieldDescriptions( CallStackStatStructDescFuncs *pFieldDescriptions, uint8 *pWriteBuffer, size_t iWriteBufferSize )
{
	size_t iWriteMarker = 0;

	//lump ID
	*(uint32 *)(pWriteBuffer + iWriteMarker) = (uint32)SSDLID_FIELDDESC;
	iWriteMarker += sizeof( uint32 );

	//lump size, currently unknown, so just save a spot to write it later
	size_t iLumpSizeMarker = iWriteMarker;
	iWriteMarker += sizeof( uint32 );

	//number of fields described, currently unknown, so just save a spot to write it later
	size_t iNumFieldsWriteMarker = iWriteMarker;
	iWriteMarker += sizeof( uint32 );
	uint32 iNumFields = 0;

	//describe the structure in the file
	CallStackStatStructDescFuncs *pDesc = pFieldDescriptions;
	while( pDesc != NULL )
	{
		size_t iFieldWrote = pDesc->DescribeField( pWriteBuffer + iWriteMarker, iWriteBufferSize - iWriteMarker );
		if( iFieldWrote != 0 )
		{
			++iNumFields;
		}
		iWriteMarker += iFieldWrote;
		pDesc = pDesc->m_pNext;
	}

	*(uint32 *)(pWriteBuffer + iNumFieldsWriteMarker) = iNumFields;
	*(uint32 *)(pWriteBuffer + iLumpSizeMarker) = (uint32)(iWriteMarker - (iLumpSizeMarker + sizeof( uint32 )));

	return iWriteMarker;
}

#if 0 //embedded script handling not ready yet
PLATFORM_INTERFACE size_t _CCallStackStatsGatherer_Write_FieldMergeScript( CallStackStatStructDescFuncs *pFieldDescriptions, CallStackStatStructDescFuncs::MergeScript_Language scriptMergeLanguage, uint8 *pWriteBuffer, size_t iWriteBufferSize )
{
	Assert( scriptMergeLanguage == SSMSL_Squirrel ); //all we support so far

	size_t iWriteMarker = 0;

	*(uint32 *)(pWriteBuffer + iWriteMarker) = (uint32)SSDLID_EMBEDDEDSCRIPT; //at some point this should move to a more global location, for now we only support exporting the merge function and no other script code
	iWriteMarker += sizeof( uint32 );

	//lump size, currently unknown, so just save a spot to write it later
	size_t iLumpSizeMarker = iWriteMarker;
	iWriteMarker += sizeof( uint32 );

	*(uint8 *)(pWriteBuffer + iWriteMarker) = (uint8)scriptMergeLanguage;
	iWriteMarker += sizeof( uint8 );



	//write out squirrel script
	iWriteMarker += _snprintf( (char *)pWriteBuffer + iWriteMarker, iWriteBufferSize - iWriteMarker, "function MergeStructs( mergeTo, mergeFrom )\n{\n" );
	CallStackStatStructDescFuncs *pDesc = pFieldDescriptions;
	while( pDesc != NULL )
	{
		iWriteMarker += pDesc->DescribeMergeOperation( scriptMergeLanguage, pWriteBuffer + iWriteMarker, iWriteBufferSize - iWriteMarker );
		if( iWriteMarker < (iWriteBufferSize - 2) )
		{
			pWriteBuffer[iWriteMarker] = '\n';
			++iWriteMarker;
			pWriteBuffer[iWriteMarker] = '\0';
		}
		pDesc = pDesc->m_pNext;
	}
	iWriteMarker += _snprintf( (char *)pWriteBuffer + iWriteMarker, iWriteBufferSize - iWriteMarker, "}\n" ) + 1;


	*(uint32 *)(pWriteBuffer + iLumpSizeMarker) = (uint32)(iWriteMarker - (iLumpSizeMarker + sizeof( uint32 )));

	return iWriteMarker;
}
#endif



#if 0 //embedded script handling not ready yet
size_t BasicStatStructFieldDesc::DescribeMergeOperation( MergeScript_Language scriptLanguage, uint8 *pDescribeWriteBuffer, size_t iDescribeMaxLength )
{
	Assert( scriptMergeLanguage == SSMSL_Squirrel ); //all we support so far

	switch( m_Combine )
	{
	case BSSFCM_ADD:
		{
			return _snprintf( (char *)pDescribeWriteBuffer, iDescribeMaxLength, "mergeTo.%s += mergeFrom.%s\n", m_szFieldName, m_szFieldName );
			break;
		}

	case BSSFCM_MAX:
		{
			return _snprintf( (char *)pDescribeWriteBuffer, iDescribeMaxLength,  "if( mergeTo.%s < mergeFrom.%s ) mergeTo.%s = mergeFrom.%s\n", m_szFieldName, m_szFieldName, m_szFieldName, m_szFieldName );
			break;
		}

	case BSSFCM_MIN:
		{
			return _snprintf( (char *)pDescribeWriteBuffer, iDescribeMaxLength, "if( mergeTo.%s > mergeFrom.%s ) mergeTo.%s = mergeFrom.%s\n", m_szFieldName, m_szFieldName, m_szFieldName, m_szFieldName );
			break;
		}

	/*case BSSFCM_AND:
		{

			break;
		}

	case BSSFCM_OR:
		{

			break;
		}

	case BSSFCM_XOR:
		{

			break;
		}

	case BSSFCM_LIST: //add the values
		{
			Error( "BSSFCM_LIST is currently unsupported" );
			break;
		}*/
	};

	return 0;
}
#endif





bool _CCallStackStatsGatherer_Internal_DumpStatsToFile( const char *szFileName, const CCallStackStatsGatherer_Standardized_t &StatsGatherer, bool bAllowMemoryAllocations )
{
	if( !StatsGatherer.pGatherer )
		return false;

	FILE *pFile = fopen(szFileName, "wb");
	if (!pFile)
		return false;

	uint8 tempBuffer[256 * 1024]; //256kb
	size_t iWriteMarker = 0;

	//File Header
	{
		//file format version
		uint8 version = 3;
		BufferedFwrite( pFile, tempBuffer, sizeof( tempBuffer ), &version, sizeof( uint8 ), iWriteMarker );

		uint32 iEndian = 0x12345678;
		BufferedFwrite( pFile, tempBuffer, sizeof( tempBuffer ), &iEndian, sizeof( uint32 ), iWriteMarker );
	}

	CCallStackStatsGatherer_DumpHelperVars_t helperVars;
	helperVars.pWriteBuffer = tempBuffer;
	helperVars.iWriteBufferSize = sizeof( tempBuffer );
	helperVars.iWriteMarker = &iWriteMarker;
	helperVars.pFile = pFile;
	helperVars.bAllowMemoryAllocations = bAllowMemoryAllocations;

	bool bRetVal = _CCallStackStatsGatherer_Internal_DumpSubTree( StatsGatherer, &helperVars );

	//flush any remnants of the write buffer
	if( iWriteMarker != 0 )
	{
		fwrite( tempBuffer, 1, iWriteMarker, pFile );
		iWriteMarker = 0;
	}

	fclose( pFile );

	return bRetVal;
}
#endif //#if !defined( ENABLE_RUNTIME_STACK_TRANSLATION )



size_t BasicStatStructFieldDesc::DescribeField( uint8 *pDescribeWriteBuffer, size_t iDescribeMaxLength )
{
#if defined( ENABLE_RUNTIME_STACK_TRANSLATION )
	//description file format
	//1 byte version
	//1 byte field type
	//1 byte combine method
	//4 byte field offset
	//null terminated string field name
	size_t iFieldNameStrLen = strlen( m_szFieldName ) + 1;
	const size_t iOutputLength = sizeof( uint8 ) + sizeof( uint8 ) + sizeof( uint8 ) + sizeof( uint32 ) + iFieldNameStrLen;
	if( iDescribeMaxLength < iOutputLength )
		return 0;

	size_t iWriteOffset = 0;

	//entry version
	*reinterpret_cast<uint8 *>(pDescribeWriteBuffer + iWriteOffset) = static_cast<uint8>(DFV_BasicStatStructFieldTypes_t);
	iWriteOffset += sizeof( uint8 );

	//field type
	*reinterpret_cast<uint8 *>(pDescribeWriteBuffer + iWriteOffset) = static_cast<uint8>(m_Type);
	iWriteOffset += sizeof( uint8 );

	//combine method
	*reinterpret_cast<uint8 *>(pDescribeWriteBuffer + iWriteOffset) = static_cast<uint8>(m_Combine);
	iWriteOffset += sizeof( uint8 );

	//field offset
	*reinterpret_cast<uint32 *>(pDescribeWriteBuffer + iWriteOffset) = static_cast<uint32>(m_iFieldOffset);
	iWriteOffset += sizeof( uint32 );

	//field name
	memcpy( pDescribeWriteBuffer + iWriteOffset, m_szFieldName, iFieldNameStrLen );
	iWriteOffset += iFieldNameStrLen;

	Assert( iWriteOffset == iOutputLength );
	return iWriteOffset;
#else
	return 0;
#endif
}

