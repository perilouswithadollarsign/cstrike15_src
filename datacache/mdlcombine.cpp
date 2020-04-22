#include "datacache/imdlcache.h"
#include "optimize.h"
#include "mdlcombine.h"
#include "filesystem.h"
#include "studio.h"
#include "mathlib/mathlib.h"
#include <math.h>
#include "tier1/characterset.h"
#include "keyvalues.h"
#include "vtfcombine.h"

// beware: this does not do any type of byte swapping for handling of endian issues!


CCombinerMemoryWriter	g_CombinerWriter;
CModelCombine			g_ModelCombiner;
static CModelCombine	*s_pCurrentCombine = &g_ModelCombiner;
static characterset_t	s_BreakSet;
unsigned int CModelCombine::m_nNextAssetID = 0;


#ifdef DEBUG_COMBINE

#define DebugCombineMsg( ... ) Msg( __VA_ARGS__ )
//#define TEST_MDL_COMBINE	1
//#define TEST_VTX_COMBINE	1

#else

#define DebugCombineMsg( ... )

#endif



CCombinerMemoryWriter::CCombinerMemoryWriter( )
{
	m_pWorkBuffer = NULL;
}


CCombinerMemoryWriter::~CCombinerMemoryWriter( )
{
	free( m_pWorkBuffer );
}


void CCombinerMemoryWriter::Init( )
{
	if ( m_pWorkBuffer == NULL )
	{
		CharacterSetBuild( &s_BreakSet, "{}()':" );

		m_pWorkBuffer = ( char * )malloc( COMBINER_WORK_BUFFER_SIZE );
		m_pEndBuffer = m_pWorkBuffer + COMBINER_WORK_BUFFER_SIZE - 1;
	}

	memset( m_pWorkBuffer, 0, COMBINER_WORK_BUFFER_SIZE );

	m_pWriteArea = m_pWritePos = NULL;
#ifdef DEBUG_COMBINE
	m_pErrorPos = NULL;
#endif // DEBUG_COMBINE
	m_nWriteArea = -1;

	InitWriteArea( WRITE_AREA_VVD, m_pWorkBuffer );
	SetWriteArea( WRITE_AREA_VVD );
}


void CCombinerMemoryWriter::InitWriteArea( int nArea, char *pPosition )
{
	pPosition = ( char * )( ( intp )( pPosition + 15 ) & ( ~15 ) );
	m_pSaveWriteArea[ nArea ] = m_pSaveWritePos[ nArea ] = pPosition;
}


void CCombinerMemoryWriter::SetWriteArea( int nArea )
{
	if ( m_nWriteArea != -1 )
	{
		m_pSaveWriteArea[ m_nWriteArea ] = m_pWriteArea;
		m_pSaveWritePos[ m_nWriteArea ] = m_pWritePos;
	}

	m_nWriteArea = nArea;
	if ( m_nWriteArea != -1 )
	{
		m_pWriteArea = m_pSaveWriteArea[ m_nWriteArea ];
		m_pWritePos = m_pSaveWritePos[ m_nWriteArea ];
	}
}


char *CCombinerMemoryWriter::AllocWrite( int nSize )
{
	char *pOrigPos = m_pWritePos;

#ifdef DEBUG_COMBINE
	if ( m_pErrorPos >= m_pWritePos && m_pErrorPos < m_pWritePos + nSize )
	{
		Msg( "AllocFailure: %d offset %d\n", m_pErrorPos - m_pWriteArea, m_pErrorPos - m_pWritePos );
		Assert( 0 );
	}
#endif // DEBUG_COMBINE
	m_pWritePos += nSize;

	if ( m_pWritePos > m_pEndBuffer )
	{
		AssertMsg( false, "Internal model combiner buffer not large enough" );
		V_sprintf_safe( g_ModelCombiner.GetResults()->m_szErrorMessage, "Internal model combiner buffer not large enough" );
		throw( COMBINE_RESULT_FLAG_OUT_OF_MEMORY );
	}

	return pOrigPos;
}


char *CCombinerMemoryWriter::WriteOffset( int &nOffsetIndex )
{
	nOffsetIndex = ( m_pWritePos - m_pWriteArea );

	return m_pWritePos;
}


char *CCombinerMemoryWriter::WriteOffset( int &nOffsetIndex, void *pBasePtr )
{
	nOffsetIndex = ( ( byte * )m_pWritePos - ( byte * )pBasePtr );

	return m_pWritePos;
}


char *CCombinerMemoryWriter::WriteOffset( short &nOffsetIndex, void *pBasePtr )
{
	nOffsetIndex = ( ( byte * )m_pWritePos - ( byte * )pBasePtr );

	return m_pWritePos;
}


char *CCombinerMemoryWriter::WriteBuffer( const void *pData, int nSize )
{
	char *pOrigPos = AllocWrite( nSize );

	memcpy( pOrigPos, pData, nSize );

	return pOrigPos;
}


char *CCombinerMemoryWriter::WriteBufferWithOffset( const void *pData, int nSize, int &nOffsetIndex )
{
	WriteOffset( nOffsetIndex );

	return WriteBuffer( pData, nSize );
}


char *CCombinerMemoryWriter::WriteString( const char *pszString )
{
	return WriteBuffer( pszString, strlen( pszString ) + 1 );
}


char *CCombinerMemoryWriter::WriteText( const char *pszString )
{
	return WriteBuffer( pszString, strlen( pszString ) );
}


void CCombinerMemoryWriter::AlignWrite( int nAlignSize )
{
#ifdef DEBUG_COMBINE
	char *pPriorPos = m_pWritePos;
#endif	// DEBUG_COMBINE

	nAlignSize--;
	m_pWritePos = ( char * )( ( intp )( m_pWritePos + nAlignSize ) & ( ~nAlignSize ) );

#ifdef DEBUG_COMBINE
	if ( m_pErrorPos >= pPriorPos && m_pErrorPos < m_pWritePos )
	{
		DebugCombineMsg( "AlignFailure: %d offset %d\n", m_pErrorPos - m_pWriteArea, m_pErrorPos - m_pWritePos );
		Assert( 0 );
	}
#endif // DEBUG_COMBINE

	if ( m_pWritePos > m_pEndBuffer )
	{
		Assert( 0 );
		V_sprintf_safe( g_ModelCombiner.GetResults()->m_szErrorMessage, "Internal model combiner buffer not large enough" );
		throw( COMBINE_RESULT_FLAG_OUT_OF_MEMORY );
	}
}


CModelCombine::CModelCombine( )
{
}


CModelCombine::~CModelCombine( )
{
}


void CModelCombine::Init( TCombinedStudioData *pCombinedStudioData )
{
	m_pCombinedStudioData = pCombinedStudioData;

	memset( &m_pCombinedStudioData->m_Results, 0, sizeof( m_pCombinedStudioData->m_Results ) );
	for ( int nGroup = 0; nGroup < COMBINER_MAX_ATLAS_GROUPS; nGroup++ )
	{
		memset( &m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_pCombinedTextures, 0, sizeof( m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_pCombinedTextures ) );
		memset( &m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_nCombinedTextureSizes, 0, sizeof( m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_nCombinedTextureSizes ) );
		m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_pCombinedMaterial = NULL;
		memset( &m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_szCombinedMaterialName, 0, sizeof( m_pCombinedStudioData->m_AtlasGroups[ nGroup ].m_szCombinedMaterialName ) );
	}

	// we need to make sure we don't have any periods, as other routines may strip off those as an extension
	for( char *pszTestChar = m_pCombinedStudioData->m_szCombinedModelName; *pszTestChar; pszTestChar++ )
	{
		if ( *pszTestChar == '.' )
		{
			*pszTestChar = '_';
		}
	}

	g_CombinerWriter.Init();
}


void CModelCombine::BeginStringTable( )
{
	strings[ 0 ].base = NULL;
	strings[ 0 ].ptr = NULL;
	strings[ 0 ].string = "";
	strings[ 0 ].dupindex = -1;
	numStrings = 1;
}


//-----------------------------------------------------------------------------
// Purpose: add a string to the file-global string table.
//			Keep track of fixup locations
//-----------------------------------------------------------------------------
void CModelCombine::AddToStringTable( void *base, int *ptr, const char *string )
{
	if ( numStrings >= COMBINER_MAX_STRINGS )
	{
		Assert( 0 );
		V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "Too many strings for string table of size %d", COMBINER_MAX_STRINGS );
		throw( COMBINE_RESULT_FLAG_TOO_MANY_STRINGS );
	}

	for ( int i = 0; i < numStrings; i++ )
	{
		if ( !string || !strcmp( string, strings[ i ].string ) )
		{
			strings[ numStrings ].base = ( byte * )base;
			strings[ numStrings ].ptr = ptr;
			strings[ numStrings ].string = string;
			strings[ numStrings ].dupindex = i;
			numStrings++;
			return;
		}
	}

	strings[ numStrings ].base = ( byte * )base;
	strings[ numStrings ].ptr = ptr;
	strings[ numStrings ].string = string;
	strings[ numStrings ].dupindex = -1;
	numStrings++;
}


//-----------------------------------------------------------------------------
// Purpose: Write out stringtable
//			fixup local pointers
//-----------------------------------------------------------------------------
void CModelCombine::WriteStringTable( )
{
	// force null at first address
	strings[ 0 ].addr = ( byte * )g_CombinerWriter.GetWritePos();
	g_CombinerWriter.WriteBuffer( "", 1 ); // null string

	// save all the rest
	for ( int i = 1; i < numStrings; i++ )
	{
		if ( strings[ i ].dupindex == -2 )
		{
			// initial filler to match string tables up
			strings[ i ].addr = ( byte * )g_CombinerWriter.GetWritePos();
			g_CombinerWriter.WriteBuffer( strings[ i ].string, strlen( strings[ i ].string ) + 1 );
		}
		else if ( strings[ i ].dupindex == -1 )
		{
			// not in table yet
			// calc offset relative to local base
			*strings[ i ].ptr = ( byte * )g_CombinerWriter.GetWritePos() - strings[ i ].base;
			// keep track of address in case of duplication
			strings[ i ].addr = ( byte * )g_CombinerWriter.GetWritePos();
			// copy string data, add a terminating \0
			g_CombinerWriter.WriteBuffer( strings[ i ].string, strlen( strings[ i ].string ) + 1 );
		}
		else
		{
			// already in table, calc offset of existing string relative to local base
			*strings[ i ].ptr = strings[ strings[ i ].dupindex ].addr - strings[ i ].base;
		}
	}

	g_CombinerWriter.AlignWrite( 4 );
}


void CModelCombine::VerifyField( int nField, const char *pszDescription )
{
#ifdef DEBUG_COMBINE
	int *pOriginal = ( int * )( ( ( byte * )m_pStudioHdr[ 0 ] ) + nField );
	int *pCombined = ( int * )( ( ( byte * )m_pCombinedStudioHdr ) + nField );

	DebugCombineMsg( "Verify Field %s: %d / %d\n", pszDescription, *pOriginal, *pCombined );
#endif // DEBUG_COMBINE
}


void CModelCombine::VerifyField2( int nField, const char *pszDescription )
{
#ifdef DEBUG_COMBINE
	int *pOriginal = ( int * )( ( ( byte * )m_pStudioHdr2[ 0 ] ) + nField );
	int *pCombined = ( int * )( ( ( byte * )m_pCombinedStudioHdr2 ) + nField );

	DebugCombineMsg( "Verify Field2 %s: %d / %d\n", pszDescription, *pOriginal, *pCombined );
#endif // DEBUG_COMBINE
}


void CModelCombine::VerifyOffset( void *pPtr, const char *pszDescription, void *pWritePos )
{
#ifdef DEBUG_COMBINE
	int pV1 = ( ( char * )pPtr ) - ( ( char * ) m_pStudioHdr[ 0 ] );

	if ( pWritePos == 0 )
	{
		pWritePos = m_pWritePos;
	}
	int pV2 = ( char * )pWritePos - m_pWriteArea;

	DebugCombineMsg( "Verify Offset %s: %d / %d ( %d )\n", pszDescription, pV1, pV2, pV2 - pV1 );
#endif // DEBUG_COMBINE
}


bool CModelCombine::Resolve( )
{
	char szFileName[ MAX_PATH ];

	m_pCombinedStudioData->m_Results.m_nCombinedResults = COMBINE_RESULT_FLAG_OK;
	m_pCombinedStudioData->m_Results.m_szErrorMessage[ 0 ] = 0;
	m_pCombinedStudioData->m_Results.m_szErrorDetails[ 0 ] = 0;
	m_pCombinedStudioData->m_Results.m_nDetailedError = COMBINED_DETAIL_ERROR_NOT_SPECIFIED;

	memset( MDL_Data, 0, sizeof( MDL_Data ) );
	memset( VTX_Data, 0, sizeof( VTX_Data ) );
	memset( VVD_Data, 0, sizeof( VVD_Data ) );

	try
	{
		if ( STRING(m_pCombinedStudioData->m_ModelInputData[ 0 ].m_iszModelName)[ 0 ] == 0 )
		{
			Assert( 0 );
			V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "No primary model specified" );
			throw( COMBINE_RESULT_FLAG_MISSING_ASSET_FILE );
		}

		double flStartLoadTime = Plat_FloatTime();

		for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
		{
			bool bResult;

			MDL_Data[ nModel ] = new CUtlBuffer();
			V_strcpy_safe( szFileName, STRING(m_pCombinedStudioData->m_ModelInputData[ nModel ].m_iszModelName) );
			Q_SetExtension( szFileName, ".mdl", sizeof( szFileName ) );
			bResult = g_pFullFileSystem->ReadFile( szFileName, "GAME", *MDL_Data[ nModel ], 0 );
			if ( !bResult )
			{
				Assert( 0 );
				V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "Missing asset file: %s", szFileName );
				throw( COMBINE_RESULT_FLAG_MISSING_ASSET_FILE );
			}
			m_pStudioHdr[ nModel ] = ( studiohdr_t * )MDL_Data[ nModel ]->PeekGet();
			m_pStudioHdr2[ nModel ] = ( studiohdr2_t * )MDL_Data[ nModel ]->PeekGet( m_pStudioHdr[ nModel ]->studiohdr2index );

			VTX_Data[ nModel ] = new CUtlBuffer();
			V_strcpy_safe( szFileName, STRING(m_pCombinedStudioData->m_ModelInputData[ nModel ].m_iszModelName) );
			Q_SetExtension( szFileName, ".dx90.vtx", sizeof( szFileName ) );			// GetVTXExtension() private :(
			bResult = g_pFullFileSystem->ReadFile( szFileName, "GAME", *VTX_Data[ nModel ], 0 );
			if ( !bResult )
			{
				Assert( 0 );
				V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "Missing asset file: %s", szFileName );
				throw( COMBINE_RESULT_FLAG_MISSING_ASSET_FILE );
			}

			VVD_Data[ nModel ] = new CUtlBuffer();
			V_strcpy_safe( szFileName, STRING(m_pCombinedStudioData->m_ModelInputData[ nModel ].m_iszModelName) );
			Q_SetExtension( szFileName, ".vvd", sizeof( szFileName ) );
			bResult = g_pFullFileSystem->ReadFile( szFileName, "GAME", *VVD_Data[ nModel ], 0 );
			if ( !bResult )
			{
				Assert( 0 );
				V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "Missing asset file: %s", szFileName );
				throw( COMBINE_RESULT_FLAG_MISSING_ASSET_FILE );
			}
			m_pVertexFileHeader[ nModel ] = ( vertexFileHeader_t * )VVD_Data[ nModel ]->PeekGet();
		}

		m_pCombinedStudioData->m_Results.m_flModelLoadDuration += ( float )( Plat_FloatTime() - flStartLoadTime );

		CombineTextures();

		double flStartCombineTime = Plat_FloatTime();

		DetermineMasterBoneList();

		CombineVVD();

		CombineVTX();

#ifdef TEST_MDL_COMBINE
		CombineMDL( true );
		TestCombineMDL();
#else
		CombineMDL( false );
#endif

		g_CombinerWriter.SetWriteArea( -1 );
		
		m_pCombinedStudioData->m_Results.m_flModelCombineDuration += ( float )( Plat_FloatTime() - flStartCombineTime );
		m_pCombinedStudioData->m_Results.m_nCombinedResults = COMBINE_RESULT_FLAG_OK;
	}
	catch( ECombinedResult nFlags )
	{
		m_pCombinedStudioData->m_Results.m_nCombinedResults = nFlags;
	}

	GetTextureCombiner().Cleanup();

	for( int i = 0; i < m_pCombinedStudioData->m_nNumModels; i++ )
	{
		delete MDL_Data[ i ];
		delete VTX_Data[ i ];
		delete VVD_Data[ i ];
	}

	return ( m_pCombinedStudioData->m_Results.m_nCombinedResults == COMBINE_RESULT_FLAG_OK );
}


void CModelCombine::DetermineMasterBoneList( )
{
	studiohdr_t *pPrimaryStudioHdr = m_pStudioHdr[ 0 ];

	if ( pPrimaryStudioHdr->numbones > COMBINER_MAX_BONES )
	{
		Assert( 0 );
		V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "Too many bones in primary model: %d > %d", pPrimaryStudioHdr->numbones, COMBINER_MAX_BONES );
		throw( COMBINE_RESULT_FLAG_TOO_MANY_BONES );
	}

	memset( m_nMasterToLocalBoneRemap, -1, sizeof( m_nMasterToLocalBoneRemap ) );

	m_nNumMasterBones = 0;
	for( int nBone = 0; nBone < pPrimaryStudioHdr->numbones; nBone++ )
	{
		const mstudiobone_t *pOrigBone = pPrimaryStudioHdr->pBone( nBone );

		m_pMasterBoneList[ m_nNumMasterBones ] = pOrigBone;
		m_nBoneModelOwner[ m_nNumMasterBones ] = 0;
		m_nBoneRemap[ 0 ][ nBone ] = m_nNumMasterBones;
		m_nMasterToLocalBoneRemap[ 0 ][ m_nNumMasterBones ] = nBone;

		m_nNumMasterBones++;
	}

	for( int nModel = 1; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
	{
		studiohdr_t *pSecondaryStudioHdr = m_pStudioHdr[ nModel ];

		for( int nBone = 0; nBone < pSecondaryStudioHdr->numbones; nBone++ )
		{
			const mstudiobone_t	*pOrigBone = pSecondaryStudioHdr->pBone( nBone );
			char			*pszName = pOrigBone->pszName();
			bool			bFound = false;

			for ( int nBoneSearch = 0 ; nBoneSearch < m_nNumMasterBones; nBoneSearch++ )
			{
				if ( strcmpi( m_pMasterBoneList[ nBoneSearch ]->pszName(), pszName ) == 0 )
				{
					m_nBoneRemap[ nModel ][ nBone ] = nBoneSearch;
					m_nMasterToLocalBoneRemap[ nModel ][ nBoneSearch ] = nBone;
					bFound = true;
					break;
				}
			}

			if ( bFound == false )
			{
				if ( m_nNumMasterBones >= COMBINER_MAX_BONES )
				{
					Assert( 0 );
					V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "Too many bones from secondary model %s", pSecondaryStudioHdr->pszName() );
					throw( COMBINE_RESULT_FLAG_TOO_MANY_BONES );
				}

				m_pMasterBoneList[ m_nNumMasterBones ] = pOrigBone;
				m_nBoneModelOwner[ m_nNumMasterBones ] = nModel;
				m_nBoneRemap[ nModel ][ nBone ] = m_nNumMasterBones;
				m_nMasterToLocalBoneRemap[ nModel ][ m_nNumMasterBones ] = nBone;

				m_nNumMasterBones++;
			}
		}
	}
}


void CModelCombine::CombineMDL_PreintStrings( )
{
	char *pStringData = m_pStudioHdr2[ 0 ]->pszName();		// this is the first entry in the string table;
	char *pEndData = ( ( char * ) m_pStudioHdr[ 0 ] ) + m_pStudioHdr[ 0 ]->length;

	while( pStringData < pEndData )
	{
		if ( numStrings >= COMBINER_MAX_STRINGS )
		{
			Assert( 0 );
			V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "Too many initial strings %d > %d", numStrings, COMBINER_MAX_STRINGS );
			throw( COMBINE_RESULT_FLAG_TOO_MANY_STRINGS );
		}

		strings[ numStrings ].base = NULL;
		strings[ numStrings ].ptr = NULL;
		strings[ numStrings ].string = pStringData;
		strings[ numStrings ].dupindex = -2;
		numStrings++;

		pStringData += strlen( pStringData ) + 1;
	}
}


void CModelCombine::WriteBoneProc( int nSize, int nType1, int nType2 )
{
	for( int nBone = 0; nBone < m_nNumMasterBones; nBone++ )
	{
		const mstudiobone_t *pOrigBone = m_pMasterBoneList[ nBone ];
		mstudiobone_t *pNewBone = (mstudiobone_t *)m_pCombinedStudioHdr->pBone( nBone );

		if ( pNewBone->proctype == nType1 || pNewBone->proctype == nType2 )
		{
			char *pPos = g_CombinerWriter.WriteBuffer( pOrigBone->pProcedure(), nSize );
			pNewBone->procindex = (byte *)pPos - (byte *)pNewBone;
		}
	}

	g_CombinerWriter.AlignWrite( 4 );
}


void CModelCombine::WriteBoneQuatInterp( )
{
	for( int nBone = 0; nBone < m_nNumMasterBones; nBone++ )
	{
		const mstudiobone_t *pOrigBone = m_pMasterBoneList[ nBone ];
		mstudiobone_t *pNewBone = (mstudiobone_t *)m_pCombinedStudioHdr->pBone( nBone );

		if ( pNewBone->proctype == STUDIO_PROC_QUATINTERP )
		{
			mstudioquatinterpbone_t *pOrigProc = ( mstudioquatinterpbone_t * )pOrigBone->pProcedure();
			mstudioquatinterpbone_t *pNewProc = ( mstudioquatinterpbone_t * )pNewBone->pProcedure();

			char *pPos = g_CombinerWriter.WriteBuffer( pOrigProc->pTrigger( 0 ), pOrigProc->numtriggers * sizeof( mstudioquatinterpinfo_t ) );
			pNewProc->triggerindex = (byte *)pPos - (byte *)pNewProc;
		}
	}

//	AlignWrite( 4 );
}


void CModelCombine::WriteBoneTwist( )
{
	for( int nBone = 0; nBone < m_nNumMasterBones; nBone++ )
	{
		const mstudiobone_t *pOrigBone = m_pMasterBoneList[ nBone ];
		mstudiobone_t *pNewBone = (mstudiobone_t *)m_pCombinedStudioHdr->pBone( nBone );

		if ( pNewBone->proctype == STUDIO_PROC_TWIST_MASTER || pNewBone->proctype == STUDIO_PROC_TWIST_SLAVE )
		{
			mstudiotwistbone_t *pOrigProc = ( mstudiotwistbone_t * )pOrigBone->pProcedure();
			mstudiotwistbone_t *pNewProc = ( mstudiotwistbone_t * )pNewBone->pProcedure();

			char *pPos = g_CombinerWriter.WriteBuffer( pOrigProc->pTarget( 0 ), pOrigProc->m_nTargetCount * sizeof( mstudiotwistbonetarget_t ) );
			pNewProc->m_nTargetIndex = (byte *)pPos - (byte *)pNewProc;

			g_CombinerWriter.AlignWrite( 4 );
		}
	}

	//	AlignWrite( 4 );
}


void CModelCombine::WriteBoneConstraints( )
{
	for( int nBone = 0; nBone < m_nNumMasterBones; nBone++ )
	{
		const mstudiobone_t *pOrigBone = m_pMasterBoneList[ nBone ];
		mstudiobone_t *pNewBone = (mstudiobone_t *)m_pCombinedStudioHdr->pBone( nBone );

		// make this not repeat the same code
		switch( pNewBone->proctype )
		{
			case STUDIO_PROC_POINT_CONSTRAINT:
				{
					char *pPos = g_CombinerWriter.WriteBuffer( pOrigBone->pProcedure(), sizeof( mstudiopointconstraint_t ) );
					pNewBone->procindex = (byte *)pPos - (byte *)pNewBone;
					g_CombinerWriter.AlignWrite( 4 );

					mstudiopointconstraint_t *pOrigProc = ( mstudiopointconstraint_t * )pOrigBone->pProcedure();
					mstudiopointconstraint_t *pNewProc = ( mstudiopointconstraint_t * )pNewBone->pProcedure();

					pPos = g_CombinerWriter.WriteBuffer( pOrigProc->pTarget( 0 ), pOrigProc->m_nTargetCount * sizeof( mstudioconstrainttarget_t ) );
					pNewProc->m_nTargetIndex = (byte *)pPos - (byte *)pNewProc;

					g_CombinerWriter.AlignWrite( 4 );
				}
				break;

			case STUDIO_PROC_ORIENT_CONSTRAINT:
				{
					char *pPos = g_CombinerWriter.WriteBuffer( pOrigBone->pProcedure(), sizeof( mstudioorientconstraint_t ) );
					pNewBone->procindex = (byte *)pPos - (byte *)pNewBone;
					g_CombinerWriter.AlignWrite( 4 );

					mstudioorientconstraint_t *pOrigProc = ( mstudioorientconstraint_t * )pOrigBone->pProcedure();
					mstudioorientconstraint_t *pNewProc = ( mstudioorientconstraint_t * )pNewBone->pProcedure();

					pPos = g_CombinerWriter.WriteBuffer( pOrigProc->pTarget( 0 ), pOrigProc->m_nTargetCount * sizeof( mstudioconstrainttarget_t ) );
					pNewProc->m_nTargetIndex = (byte *)pPos - (byte *)pNewProc;

					g_CombinerWriter.AlignWrite( 4 );
				}
				break;

			case STUDIO_PROC_AIM_CONSTRAINT:
				{
					char *pPos = g_CombinerWriter.WriteBuffer( pOrigBone->pProcedure(), sizeof( mstudioaimconstraint_t ) );
					pNewBone->procindex = (byte *)pPos - (byte *)pNewBone;
					g_CombinerWriter.AlignWrite( 4 );

					mstudioaimconstraint_t *pOrigProc = ( mstudioaimconstraint_t * )pOrigBone->pProcedure();
					mstudioaimconstraint_t *pNewProc = ( mstudioaimconstraint_t * )pNewBone->pProcedure();

					pPos = g_CombinerWriter.WriteBuffer( pOrigProc->pTarget( 0 ), pOrigProc->m_nTargetCount * sizeof( mstudioconstrainttarget_t ) );
					pNewProc->m_nTargetIndex = (byte *)pPos - (byte *)pNewProc;

					g_CombinerWriter.AlignWrite( 4 );
				}
				break;

			case STUDIO_PROC_PARENT_CONSTRAINT:
				{
					char *pPos = g_CombinerWriter.WriteBuffer( pOrigBone->pProcedure(), sizeof( mstudioparentconstraint_t ) );
					pNewBone->procindex = (byte *)pPos - (byte *)pNewBone;
					g_CombinerWriter.AlignWrite( 4 );

					mstudioparentconstraint_t *pOrigProc = ( mstudioparentconstraint_t * )pOrigBone->pProcedure();
					mstudioparentconstraint_t *pNewProc = ( mstudioparentconstraint_t * )pNewBone->pProcedure();

					pPos = g_CombinerWriter.WriteBuffer( pOrigProc->pTarget( 0 ), pOrigProc->m_nTargetCount * sizeof( mstudioconstrainttarget_t ) );
					pNewProc->m_nTargetIndex = (byte *)pPos - (byte *)pNewProc;

					g_CombinerWriter.AlignWrite( 4 );
				}
				break;
		}
	}
}


void CModelCombine::WriteBoneAttachments( )
{
	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->localattachmentindex );
	m_pCombinedStudioHdr->numlocalattachments = 0;

	for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
	{
		studiohdr_t *pStudioHdr = m_pStudioHdr[ nModel ];

		int nNumAttachments = pStudioHdr->numlocalattachments;

		for( int nAttachment = 0; nAttachment < nNumAttachments; nAttachment++ )
		{
			m_pCombinedStudioHdr->numlocalattachments++;

			mstudioattachment_t		*pOrigAttachment = pStudioHdr->pLocalAttachment( nAttachment );
			mstudioattachment_t		*pNewAttachment = m_pCombinedStudioHdr->pLocalAttachment( m_pCombinedStudioHdr->numlocalattachments - 1 );

			g_CombinerWriter.WriteBuffer( pOrigAttachment, sizeof( *pOrigAttachment ) );
			AddToStringTable( pNewAttachment, &pNewAttachment->sznameindex, pOrigAttachment->pszName() );
			RemapBone( nModel, pNewAttachment->localbone );

		}
	}

	g_CombinerWriter.AlignWrite( 4 );

	VerifyField( offsetof( studiohdr_t, localattachmentindex ), "BoneAttachments" );
}


void CModelCombine::WriteHitBoxes( )
{
	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->hitboxsetindex );
	m_pCombinedStudioHdr->numhitboxsets = 0;

	for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
	{
		if ( nModel > 0 )
		{	// for now, we are only interested in the hitbox sets of the primary model
			break;
		}

		studiohdr_t *pStudioHdr = m_pStudioHdr[ nModel ];

		int nNumHitBoxSets = pStudioHdr->numhitboxsets;

		for( int nHitBoxSet = 0; nHitBoxSet < nNumHitBoxSets; nHitBoxSet++ )
		{
			m_pCombinedStudioHdr->numhitboxsets++;
			mstudiohitboxset_t		*pOrigHitBoxSet = pStudioHdr->pHitboxSet( nHitBoxSet );
			mstudiohitboxset_t		*pNewHitBoxSet = m_pCombinedStudioHdr->pHitboxSet( m_pCombinedStudioHdr->numhitboxsets - 1 );

			g_CombinerWriter.WriteBuffer( pOrigHitBoxSet, sizeof( *pOrigHitBoxSet ) );
			AddToStringTable( pNewHitBoxSet, &pNewHitBoxSet->sznameindex, pOrigHitBoxSet->pszName() );
		}
	}

	g_CombinerWriter.AlignWrite( 4 );

	for( int nHitBoxSet = 0; nHitBoxSet < m_pCombinedStudioHdr->numhitboxsets; nHitBoxSet++ )
	{
		mstudiohitboxset_t		*pNewHitBoxSet = m_pCombinedStudioHdr->pHitboxSet( nHitBoxSet );

		pNewHitBoxSet->numhitboxes = 0;
		g_CombinerWriter.WriteOffset( pNewHitBoxSet->hitboxindex, pNewHitBoxSet );

		for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
		{
			studiohdr_t *pStudioHdr = m_pStudioHdr[ nModel ];

			if ( nModel > 0 )
			{
				break;
			}
			if ( nHitBoxSet < pStudioHdr->numhitboxsets )
			{
				mstudiohitboxset_t		*pOrigHitBoxSet = pStudioHdr->pHitboxSet( nHitBoxSet );

				for( int nHitBox = 0; nHitBox < pOrigHitBoxSet->numhitboxes; nHitBox++ )
				{
					pNewHitBoxSet->numhitboxes++;
					mstudiobbox_t		*pOrigHitBox = pOrigHitBoxSet->pHitbox( nHitBox );
					mstudiobbox_t		*pNewHitBox = pNewHitBoxSet->pHitbox( pNewHitBoxSet->numhitboxes - 1 );

					g_CombinerWriter.WriteBuffer( pOrigHitBox, sizeof( *pOrigHitBox ) );
					AddToStringTable( pNewHitBox, &pNewHitBox->szhitboxnameindex, pOrigHitBox->pszHitboxName() );
					RemapBone( nModel, pNewHitBox->bone );
				}
			}
		}

		g_CombinerWriter.AlignWrite( 4 );
	}

	VerifyField( offsetof( studiohdr_t, hitboxsetindex ), "HitBoxes" );
}


// compare function for qsort below
int CModelCombine::BoneNameCompare( const void *elem1, const void *elem2 )
{
	int index1 = *(byte *)elem1;
	int index2 = *(byte *)elem2;

	// compare bones by name
	return strcmpi( s_pCurrentCombine->m_pMasterBoneList[ index1 ]->pszName(), s_pCurrentCombine->m_pMasterBoneList[ index2 ]->pszName() );
}


void CModelCombine::WriteBoneTable( )
{
	byte *pBoneTable = ( byte * )g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->bonetablebynameindex );
	g_CombinerWriter.AllocWrite( sizeof( *pBoneTable ) * m_pCombinedStudioHdr->numbones );

	for ( int i = 0; i < m_pCombinedStudioHdr->numbones; i++ )
	{
		pBoneTable[ i ] = i;
	}

	qsort( pBoneTable, m_pCombinedStudioHdr->numbones, sizeof( byte ), BoneNameCompare );
}


void CModelCombine::CombineMDL_Bones( )
{
	studiohdr_t *pPrimaryStudioHdr = m_pStudioHdr[ 0 ];

	AddToStringTable( m_pCombinedStudioHdr, &m_pCombinedStudioHdr->surfacepropindex, pPrimaryStudioHdr->pszSurfaceProp() );

	int *nFlags = ( int * )stackalloc( sizeof( int ) * m_nNumMasterBones );
	memset( nFlags, 0, sizeof( int ) * m_nNumMasterBones );

	for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
	{
		OptimizedModel::FileHeader_t	*pOrigHeader = ( OptimizedModel::FileHeader_t * )VTX_Data[ nModel ]->PeekGet();
		int nBoneTest = 0;
		int nBoneProp = 0;

		if ( nModel > 0 )
		{
			nBoneTest = BONE_USED_BY_VERTEX_AT_LOD( pOrigHeader->numLODs - 1 );

			for( int nLOD = pOrigHeader->numLODs; nLOD < m_pCombinedHardwareHeader->numLODs; nLOD++ )
			{
				nBoneProp |= BONE_USED_BY_VERTEX_AT_LOD( nLOD );
			}
		}

		for( int nBone = 0; nBone < m_pStudioHdr[ nModel ]->numbones; nBone++ )
		{
			nFlags[ m_nBoneRemap[ nModel ][ nBone ] ] |= m_pStudioHdr[ nModel ]->pBone( nBone )->flags;

			if ( nModel > 0 && pOrigHeader->numLODs < m_pCombinedHardwareHeader->numLODs )
			{
				if ( ( m_pStudioHdr[ nModel ]->pBone( nBone )->flags & nBoneTest ) != 0 )
				{
					nFlags[ m_nBoneRemap[ nModel ][ nBone ] ] |= nBoneProp;
				}
			}
		}
	}

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->boneindex );
	m_pCombinedStudioHdr->numbones = 0;
	for( int nBone = 0; nBone < pPrimaryStudioHdr->numbones; nBone++ )
	{
		const mstudiobone_t *pOrigBone = pPrimaryStudioHdr->pBone( nBone );
		mstudiobone_t *pNewBone = ( mstudiobone_t * )g_CombinerWriter.WriteBuffer( pOrigBone, sizeof( mstudiobone_t ) );

		AddToStringTable( pNewBone, &pNewBone->sznameindex, pOrigBone->pszName() );
		AddToStringTable( pNewBone, &pNewBone->surfacepropidx, pOrigBone->pszSurfaceProp() );

		pNewBone->flags = nFlags[ nBone ];
		m_pCombinedStudioHdr->numbones++;
	}

	for( int nBone = m_pCombinedStudioHdr->numbones; nBone < m_nNumMasterBones; nBone++ )
	{
		const mstudiobone_t *pOrigBone = m_pMasterBoneList[ nBone ];
		mstudiobone_t *pNewBone = ( mstudiobone_t * )g_CombinerWriter.WriteBuffer( pOrigBone, sizeof( mstudiobone_t ) );

		AddToStringTable( pNewBone, &pNewBone->sznameindex, pOrigBone->pszName() );
		AddToStringTable( pNewBone, &pNewBone->surfacepropidx, pOrigBone->pszSurfaceProp() );

		if ( pNewBone->parent >= 0 )
		{
			pNewBone->parent = m_nBoneRemap[ m_nBoneModelOwner[ nBone ] ][ pNewBone->parent ];
		}

		pNewBone->flags = nFlags[ nBone ];
		m_pCombinedStudioHdr->numbones++;
	}

	g_CombinerWriter.AlignWrite( 4 );

	WriteBoneProc( sizeof( mstudioaxisinterpbone_t ), STUDIO_PROC_AXISINTERP );
	WriteBoneProc( sizeof( mstudioquatinterpbone_t ), STUDIO_PROC_QUATINTERP );
	WriteBoneQuatInterp();
	WriteBoneProc( sizeof( mstudiojigglebone_t ), STUDIO_PROC_JIGGLE );
	WriteBoneProc( sizeof( mstudioaimatbone_t ), STUDIO_PROC_AIMATBONE, STUDIO_PROC_AIMATATTACH );	// aimAttach needs fixup
	WriteBoneProc( sizeof( mstudiotwistbone_t ), STUDIO_PROC_TWIST_MASTER, STUDIO_PROC_TWIST_SLAVE );
	WriteBoneTwist();
	WriteBoneConstraints();

	if ( pPrimaryStudioHdr->numbonecontrollers > 0 )
	{
		Assert( 0 );
		V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "model has bone controllers: %s", pPrimaryStudioHdr->pszName() );
		throw( COMBINE_RESULT_FLAG_UNSUPPORTED_FEATURE );
	}
	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->bonecontrollerindex );
	g_CombinerWriter.AlignWrite( 4 );

	WriteBoneAttachments();

	WriteHitBoxes();

	WriteBoneTable();

	g_CombinerWriter.AlignWrite( 4 );
}


void CModelCombine::WriteAnimation( mstudioanimdesc_t *pOrigAnim, void *pAnimData, int nFrameSize )
{
	if ( ( pOrigAnim->flags & STUDIO_FRAMEANIM ) != 0 )
	{
		Assert( 0 );
		V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "model has anim frames" );
		throw( COMBINE_RESULT_FLAG_UNSUPPORTED_FEATURE );
	}
	else
	{
		mstudio_rle_anim_t *pOrigFrame = ( mstudio_rle_anim_t * )( pAnimData );

		while( 1 )
		{
			int nSize = sizeof( mstudio_rle_anim_t );
			if ( pOrigFrame->flags & STUDIO_ANIM_RAWROT2 )
			{
				nSize += sizeof( Quaternion64 );
			}
			if ( pOrigFrame->flags & STUDIO_ANIM_RAWPOS )
			{
				nSize += sizeof( Vector48 );
			}
			if ( pOrigFrame->flags & STUDIO_ANIM_ANIMROT )
			{
				nSize += sizeof( mstudioanim_valueptr_t );
			}
			if ( pOrigFrame->flags & STUDIO_ANIM_ANIMPOS )
			{
				nSize += sizeof( mstudioanim_valueptr_t );
			}

			if ( pOrigFrame->flags & STUDIO_ANIM_ANIMROT )
			{
				mstudioanim_valueptr_t *rotvptr	= pOrigFrame->pRotV();
				for (int k = 0; k < 3; k++)
				{
					mstudioanimvalue_t	*pAnimValue = rotvptr->pAnimvalue( k );
					if ( pAnimValue != NULL )
					{
						int nCount = nFrameSize;
						while( nCount > 0 )
						{
							nSize += sizeof( *pAnimValue ) * ( pAnimValue->num.valid + 1 );
							nCount -= pAnimValue->num.total;
							pAnimValue += pAnimValue->num.valid + 1;
						}
					}
				}

			}
			if ( pOrigFrame->flags & STUDIO_ANIM_ANIMPOS )
			{
				mstudioanim_valueptr_t *posvptr	= pOrigFrame->pPosV();
				for (int k = 0; k < 3; k++)
				{
					mstudioanimvalue_t	*pAnimValue = posvptr->pAnimvalue( k );
					if ( pAnimValue != NULL )
					{
						int nCount = nFrameSize;
						while( nCount > 0 )
						{
							nSize += sizeof( *pAnimValue ) * ( pAnimValue->num.valid + 1 );
							nCount -= pAnimValue->num.total;
							pAnimValue += pAnimValue->num.valid + 1;
						}
					}
				}
			}

			// all of the indexes are relative, so a direct copy should be safe
			g_CombinerWriter.WriteBuffer( pOrigFrame, nSize );
//			DebugCombineMsg( "%d: %d\n", pOrigFrame->bone, pOrigFrame->nextoffset - nSize );

			if ( pOrigFrame->nextoffset == 0 )
			{
				break;
			}
			else
			{
				pOrigFrame = pOrigFrame->pNext();
				VerifyOffset( pOrigFrame, "Start rle_anim" );
			}
		} 

		g_CombinerWriter.AllocWrite( sizeof( mstudio_rle_anim_t ) );	// bug in studiomdl which adds an extra one of these
	}

	g_CombinerWriter.AlignWrite( 4 );
}


void CModelCombine::CombineMDL_Anims( )
{
	studiohdr_t *pStudioHdr = m_pStudioHdr[ 0 ];

	VerifyOffset( pStudioHdr->pLocalAnimdesc( 0 ), "Start Animations" );

	g_CombinerWriter.WriteBufferWithOffset( pStudioHdr->pLocalAnimdesc( 0 ), sizeof( mstudioanimdesc_t ) * pStudioHdr->numlocalanim, m_pCombinedStudioHdr->localanimindex );
	g_CombinerWriter.AlignWrite( 4 );
	m_pCombinedStudioHdr->numlocalanim = pStudioHdr->numlocalanim;

	for( int nAnim = 0; nAnim < m_pCombinedStudioHdr->numlocalanim; nAnim++ )
	{
		mstudioanimdesc_t *pOrigAnim = pStudioHdr->pLocalAnimdesc( nAnim );
		mstudioanimdesc_t *pNewAnim = m_pCombinedStudioHdr->pLocalAnimdesc( nAnim );

		pNewAnim->baseptr = ( byte * )g_CombinerWriter.GetWriteArea() - ( byte * )pNewAnim;
		AddToStringTable( pNewAnim, &pNewAnim->sznameindex, pOrigAnim->pszName() );

		int nNumSections = 0;

		if ( pOrigAnim->sectionframes > 1 )
		{
			nNumSections = ( pOrigAnim->numframes / pOrigAnim->sectionframes ) + 2;		// studio.cpp line 113
			pNewAnim->sectionindex = ( byte * )g_CombinerWriter.AllocWrite( nNumSections * sizeof( mstudioanimsections_t ) ) - (byte *)pNewAnim;
		}

		g_CombinerWriter.AlignWrite( 16 );
		pNewAnim->sectionframes = pOrigAnim->sectionframes;
		g_CombinerWriter.WriteOffset( pNewAnim->animindex, pNewAnim );

		DebugCombineMsg( "Anim %d: %d / %d ( %d )\n", nAnim, pOrigAnim->animindex, pNewAnim->animindex, pNewAnim->animindex - pOrigAnim->animindex );

		if ( nNumSections > 1 )
		{
			int nRemainingFrames = pOrigAnim->numframes;

			for( int nSection = 0; nSection < nNumSections; nSection++ )
			{
				mstudioanimsections_t	*pOrigSection = pOrigAnim->pSection( nSection );
				mstudioanimsections_t	*pNewSection = pNewAnim->pSection( nSection );

				VerifyOffset( pOrigSection, "Start mstudioanimsections_t", pNewSection );

				*pNewSection = *pOrigSection;
				g_CombinerWriter.WriteOffset( pNewSection->animindex, pNewAnim );

				int nFrames;

				if ( nRemainingFrames > pOrigAnim->sectionframes )
				{
					nRemainingFrames -= pOrigAnim->sectionframes;
					nFrames = pOrigAnim->sectionframes + 1;
				}
				else
				{
					nFrames = nRemainingFrames;
					nRemainingFrames = 0;
				}

				WriteAnimation( pOrigAnim, ( ( byte * )pOrigAnim ) + pOrigSection->animindex, nFrames );
			}
		}
		else
		{
			WriteAnimation( pOrigAnim, pOrigAnim->pAnimBlock( pOrigAnim->animblock, pOrigAnim->animindex ), pOrigAnim->numframes );
		}

		g_CombinerWriter.AlignWrite( 4 );
		if ( pOrigAnim->numikrules > 0 )
		{
			Assert( 0 );
			V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "model has ik rules: %s", pStudioHdr->pszName() );
			throw( COMBINE_RESULT_FLAG_UNSUPPORTED_FEATURE );
		}

		g_CombinerWriter.AlignWrite( 4 );
		if ( pOrigAnim->numlocalhierarchy > 0 )
		{
			g_CombinerWriter.WriteOffset( pNewAnim->localhierarchyindex, pNewAnim );
			g_CombinerWriter.WriteBuffer( pOrigAnim->pHierarchy( 0 ), pOrigAnim->numlocalhierarchy * sizeof( mstudiolocalhierarchy_t ) );
			for( int nLocalHierarchy = 0; nLocalHierarchy < pOrigAnim->numlocalhierarchy; nLocalHierarchy++ )
			{
				mstudiolocalhierarchy_t	*pOrigLocalHierarchy = pOrigAnim->pHierarchy( nLocalHierarchy );
				mstudiolocalhierarchy_t	*pNewLocalHierarchy = pNewAnim->pHierarchy( nLocalHierarchy );

				g_CombinerWriter.WriteOffset( pNewLocalHierarchy->localanimindex, pNewLocalHierarchy );

				mstudiocompressedikerror_t	*pOrigCompressedIKError = pOrigLocalHierarchy->pLocalAnim();
				mstudiocompressedikerror_t	*pNewCompressedIKError = pNewLocalHierarchy->pLocalAnim();
				
				g_CombinerWriter.WriteBuffer( pOrigCompressedIKError, sizeof( *pOrigCompressedIKError ) );

				for( int nAnim = 0; nAnim < 6; nAnim++ )
				{
					g_CombinerWriter.WriteOffset( pNewCompressedIKError->offset[ nAnim ], pNewCompressedIKError );

					int nSize = 0;
					mstudioanimvalue_t	*pAnimValue = pOrigCompressedIKError->pAnimvalue( nAnim );
					if ( pAnimValue != NULL )
					{
						int nCount = pOrigAnim->numframes;
						while( nCount > 0 )
						{
							nSize += sizeof( *pAnimValue ) * ( pAnimValue->num.valid + 1 );
							nCount -= pAnimValue->num.total;
							pAnimValue += pAnimValue->num.valid + 1;
						}
					}

					g_CombinerWriter.WriteBuffer( pOrigCompressedIKError->pAnimvalue( nAnim ), nSize );
				}
			}
		}

		g_CombinerWriter.AlignWrite( 4 );
	}

	for( int nAnim = 0; nAnim < m_pCombinedStudioHdr->numlocalanim; nAnim++ )
	{
		mstudioanimdesc_t *pOrigAnim = pStudioHdr->pLocalAnimdesc( nAnim );
		mstudioanimdesc_t *pNewAnim = m_pCombinedStudioHdr->pLocalAnimdesc( nAnim );

		if ( pOrigAnim->nummovements > 0 )
		{
			g_CombinerWriter.WriteOffset( pNewAnim->movementindex, pNewAnim );
			VerifyOffset( pOrigAnim->pMovement( 0 ), "Start Movement" );
			g_CombinerWriter.WriteBuffer( pOrigAnim->pMovement( 0 ), sizeof( mstudiomovement_t ) * pOrigAnim->nummovements );
			g_CombinerWriter.AlignWrite( 4 );
		}
	}

	g_CombinerWriter.AlignWrite( 4 );
}


void CModelCombine::CombineMDL_SequenceInfo( )
{
	studiohdr_t *pStudioHdr = m_pStudioHdr[ 0 ];

	VerifyField( offsetof( studiohdr_t, localseqindex ), "Sequence Info" );
	
	m_pCombinedStudioHdr->numlocalseq = pStudioHdr->numlocalseq;
	g_CombinerWriter.WriteBufferWithOffset( pStudioHdr->pLocalSeqdesc( 0 ), sizeof( mstudioseqdesc_t ) * pStudioHdr->numlocalseq, m_pCombinedStudioHdr->localseqindex );

	for( int nSequence = 0; nSequence < pStudioHdr->numlocalseq; nSequence++ )
	{
		mstudioseqdesc_t *pOrigSequence = pStudioHdr->pLocalSeqdesc( nSequence );
		mstudioseqdesc_t *pNewSequence = m_pCombinedStudioHdr->pLocalSeqdesc( nSequence );

		AddToStringTable( pNewSequence, &pNewSequence->szlabelindex, pOrigSequence->pszLabel() );
		AddToStringTable( pNewSequence, &pNewSequence->szactivitynameindex, pOrigSequence->pszActivityName() );

		pNewSequence->baseptr = ( byte * )g_CombinerWriter.GetWriteArea() - ( byte * )pNewSequence;

		if ( pOrigSequence->groupsize[ 0 ] > 1 || pOrigSequence->groupsize[ 1 ] > 1 )
		{
			g_CombinerWriter.WriteOffset( pNewSequence->posekeyindex, pNewSequence );
			int nSize = ( pOrigSequence->groupsize[ 0 ] + pOrigSequence->groupsize[ 1 ] ) * sizeof( float );
			g_CombinerWriter.WriteBuffer( pOrigSequence->pPoseKey( 0, 0 ), nSize );
		}

		g_CombinerWriter.WriteOffset( pNewSequence->eventindex, pNewSequence );
		if ( pOrigSequence->numevents > 0 )
		{
			VerifyOffset( pOrigSequence->pEvent( 0 ), "Sequence Event" );
			g_CombinerWriter.WriteBuffer( pOrigSequence->pEvent( 0 ), pOrigSequence->numevents * sizeof( mstudioevent_t ) );
			for ( int nEvent = 0; nEvent < pOrigSequence->numevents; nEvent++ )
			{
				mstudioevent_t *pOrigEvent = pOrigSequence->pEvent( nEvent );
				mstudioevent_t *pNewEvent = pNewSequence->pEvent( nEvent );

				if ( pOrigEvent->type == NEW_EVENT_STYLE )
				{
					AddToStringTable( pNewEvent, &pNewEvent->szeventindex, pOrigEvent->pszEventName() );
				}
			}
		}
		g_CombinerWriter.AlignWrite( 4 );

		g_CombinerWriter.WriteOffset( pNewSequence->autolayerindex, pNewSequence );
		if ( pOrigSequence->numautolayers > 0 )
		{
			VerifyOffset( pOrigSequence->pAutolayer( 0 ), "Auto Layer" );
			g_CombinerWriter.WriteBuffer( pOrigSequence->pAutolayer( 0 ), pOrigSequence->numautolayers * sizeof( mstudioautolayer_t ) );
		}

		if ( pOrigSequence->weightlistindex < pOrigSequence->eventindex )
		{	// we are using an existing weight
			int nFound = -1;

			for( int nPreviousSequence = 0; nPreviousSequence < nSequence; nPreviousSequence++ )
			{
				mstudioseqdesc_t *pOrigPreviousSequence = pStudioHdr->pLocalSeqdesc( nPreviousSequence );

				if ( memcmp( pOrigPreviousSequence->pBoneweight( 0 ), pOrigSequence->pBoneweight( 0 ), pStudioHdr->numbones * sizeof( float ) ) == 0 )
				{
					nFound = nPreviousSequence;
					break;
				}
			}

			if ( nFound != -1 )
			{
				mstudioseqdesc_t *pNewPreviousSequence = m_pCombinedStudioHdr->pLocalSeqdesc( nFound );
				pNewSequence->weightlistindex = ( ( byte * )pNewPreviousSequence->pBoneweight( 0 ) - ( byte * )pNewSequence);
			}
			else
			{
				Assert( 0 );
				V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "was not able to find existing weight in model: %s", pStudioHdr->pszName() );
				throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
			}
		}
		else
		{
			g_CombinerWriter.WriteOffset( pNewSequence->weightlistindex, pNewSequence );
//			Assert( m_nNumMasterBones == pStudioHdr->numbones );		// need to handle bone merge!
			g_CombinerWriter.WriteBuffer( pOrigSequence->pBoneweight( 0 ), pStudioHdr->numbones * sizeof( float ) );
			float flTempWeight = 1.0f;
			for( int nNewBones = pStudioHdr->numbones; nNewBones < m_nNumMasterBones; nNewBones++ )
			{	// more efficient ways to do this, but for now, not knowing if I can just give them all 1.0f, I'll leave it
				g_CombinerWriter.WriteBuffer( &flTempWeight, sizeof( flTempWeight ) );
			}
		}

		g_CombinerWriter.WriteOffset( pNewSequence->iklockindex, pNewSequence );
		if ( pOrigSequence->numiklocks > 0 )
		{
			VerifyOffset( pOrigSequence->pIKLock( 0 ), "IK Lock" );
			g_CombinerWriter.WriteBuffer( pOrigSequence->pIKLock( 0 ), pOrigSequence->numiklocks * sizeof( mstudioiklock_t ) );
		}
		g_CombinerWriter.AlignWrite( 4 );

		g_CombinerWriter.WriteOffset( pNewSequence->animindexindex, pNewSequence );
		int nSize = ( pOrigSequence->groupsize[ 0 ] * pOrigSequence->groupsize[ 1 ] ) * sizeof( short );
		if ( nSize > 0 )
		{
			VerifyOffset( ( ( ( byte * )pOrigSequence ) + pOrigSequence->animindexindex ), "AnimIndexIndex" );
			g_CombinerWriter.WriteBuffer( ( ( ( byte * )pOrigSequence ) + pOrigSequence->animindexindex ), nSize );
		}
		g_CombinerWriter.AlignWrite( 4 );

		g_CombinerWriter.WriteOffset( pNewSequence->keyvalueindex, pNewSequence );
		if( pOrigSequence->keyvaluesize > 0 )
		{
			VerifyOffset( ( void * )pOrigSequence->KeyValueText(), "Sequence KV" );
			g_CombinerWriter.WriteBuffer( ( void * )pOrigSequence->KeyValueText(), pOrigSequence->keyvaluesize * sizeof( char ) );
		}
		g_CombinerWriter.AlignWrite( 4 );

		g_CombinerWriter.WriteOffset( pNewSequence->activitymodifierindex, pNewSequence );
		if ( pOrigSequence->numactivitymodifiers > 0 )
		{
			g_CombinerWriter.WriteBuffer( pOrigSequence->pActivityModifier( 0 ), pOrigSequence->numactivitymodifiers * sizeof( mstudioactivitymodifier_t ) );
		}
		g_CombinerWriter.AlignWrite( 4 );
		for ( int nActivityModifier = 0; nActivityModifier < pOrigSequence->numactivitymodifiers; nActivityModifier++ )
		{
			mstudioactivitymodifier_t *pOrigActivityModifier = pOrigSequence->pActivityModifier( nActivityModifier );
			mstudioactivitymodifier_t *pNewActivityModifier = pNewSequence->pActivityModifier( nActivityModifier );

			AddToStringTable( pNewActivityModifier, &pNewActivityModifier->sznameindex, pOrigActivityModifier->pszName() );
		}

		nSize = pOrigSequence->numactivitymodifiers * sizeof( mstudioactivitymodifier_t );
		byte *pTestPtr = ( ( ( byte * )pOrigSequence ) + pOrigSequence->activitymodifierindex ) + nSize;
		VerifyOffset( pTestPtr, "Final Sequence Write" );
	}

	int	*pNodeName = ( int * )g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->localnodenameindex );
	g_CombinerWriter.AllocWrite( pStudioHdr->numlocalnodes * sizeof( *pNodeName ) );
	g_CombinerWriter.AlignWrite( 4 );
	for ( int nNodeIndex = 0; nNodeIndex < pStudioHdr->numlocalnodes; nNodeIndex++ )
	{
		AddToStringTable( m_pCombinedStudioHdr, pNodeName, pStudioHdr->pszLocalNodeName( nNodeIndex ) );
		pNodeName++;
	}

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->localnodeindex );
	if ( pStudioHdr->numlocalnodes > 0 )
	{
		g_CombinerWriter.WriteBuffer( pStudioHdr->pLocalTransition( 0 ), pStudioHdr->numlocalnodes * pStudioHdr->numlocalnodes * sizeof( byte ) );
	}

	g_CombinerWriter.AlignWrite( 4 );
}


void CModelCombine::WriteModel( int nModel )
{
	mstudiomodel_t	*pOrigModel = m_pMasterModels[ 0 ][ nModel ];
	mstudiomodel_t	*pNewModel = &m_pCombinedModels[ nModel ];

	g_CombinerWriter.WriteOffset( pNewModel->meshindex, pNewModel );
	g_CombinerWriter.WriteBuffer( pOrigModel->pMesh( 0 ), pOrigModel->nummeshes * sizeof( mstudiomesh_t ) );
	g_CombinerWriter.AlignWrite( 4 );
	for ( int nMesh = 0; nMesh < pOrigModel->nummeshes; nMesh++ )
	{
//		mstudiomesh_t	*pOrigMesh = pOrigModel->pMesh( nMesh );
		mstudiomesh_t	*pNewMesh = pNewModel->pMesh( nMesh );

		pNewMesh->numvertices = m_pCombinedVertex->numLODVertexes[ 0 ];
		for( int nLOD = 0; nLOD < m_pCombinedVertex->numLODs; nLOD++ )
		{
			pNewMesh->vertexdata.numLODVertexes[ nLOD ] = m_pCombinedVertex->numLODVertexes[ nLOD ];
		}

		pNewMesh->modelindex = ( byte * )pNewModel - ( byte * )pNewMesh;
	}

//	pNewModel->vertexindex = pOrigModel->vertexindex;
//	externalVertexIndex += pmodel[i].numvertices * sizeof(mstudiovertex_t);

//	ALIGN4( externalTangentsIndex );
//	pmodel[i].tangentsindex = (int)externalTangentsIndex;
//	externalTangentsIndex += pmodel[i].numvertices * sizeof( Vector4D );

	g_CombinerWriter.WriteOffset( pNewModel->eyeballindex, pNewModel );
	if ( pOrigModel->numeyeballs > 0 )
	{
		g_CombinerWriter.WriteBuffer( pOrigModel->pEyeball( 0 ), pOrigModel->numeyeballs * sizeof( mstudioeyeball_t ) );
	}

	for ( int nMesh = 0; nMesh < pOrigModel->nummeshes; nMesh++ )
	{
//		mstudiomesh_t	*pOrigMesh = pOrigModel->pMesh( nMesh );
		mstudiomesh_t	*pNewMesh = pNewModel->pMesh( nMesh );

		pNewMesh->numflexes = 0;
		g_CombinerWriter.WriteOffset( pNewMesh->flexindex, pNewMesh );
		for( int nSubModel = 0; nSubModel < m_pCombinedStudioData->m_nNumModels; nSubModel++ )
		{
			if ( !m_pMasterModels[ nSubModel ][ nModel ] )
			{
				continue;
			}
			mstudiomesh_t	*pOrigFlexMesh = m_pMasterModels[ nSubModel ][ nModel ]->pMesh( nMesh );

			if ( pOrigFlexMesh->numflexes )
			{	// figure out the total number of flex entries for this mesh and reserve space
				pNewMesh->numflexes += pOrigFlexMesh->numflexes;
				g_CombinerWriter.WriteBuffer( pOrigFlexMesh->pFlex( 0 ), pOrigFlexMesh->numflexes * sizeof( mstudioflex_t ) );
			}
		}

		int nTotalFlexes = 0;
		if ( pNewMesh->numflexes > 0 )
		{
			g_CombinerWriter.AlignWrite( 4 );

			for( int nSubModel = 0; nSubModel < m_pCombinedStudioData->m_nNumModels; nSubModel++ )
			{
				if ( !m_pMasterModels[ nSubModel ][ nModel ] )
				{
					continue;
				}
				mstudiomesh_t	*pOrigFlexMesh = m_pMasterModels[ nSubModel ][ nModel ]->pMesh( nMesh );

				if ( pOrigFlexMesh->numflexes )
				{
					for( int nFlex = 0; nFlex < pOrigFlexMesh->numflexes; nFlex++, nTotalFlexes++ )
					{
						mstudioflex_t	*pOrigFlex = pOrigFlexMesh->pFlex( nFlex );
						mstudioflex_t	*pNewFlex = pNewMesh->pFlex( nTotalFlexes );

						bool bWrinkleVAnim = ( pOrigFlex->vertanimtype == STUDIO_VERT_ANIM_WRINKLE );
						int nVAnimDeltaSize = bWrinkleVAnim ? sizeof( mstudiovertanim_wrinkle_t ) : sizeof( mstudiovertanim_t );

						g_CombinerWriter.WriteOffset( pNewFlex->vertindex, pNewFlex );
						g_CombinerWriter.WriteBuffer( pOrigFlex->pBaseVertanim(), pOrigFlex->numverts * nVAnimDeltaSize );

						if ( bWrinkleVAnim )
						{
							Assert( 0 );
							V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "model has anim wrinkle" );
							throw( COMBINE_RESULT_FLAG_UNSUPPORTED_FEATURE );

#if 0
							for( int nVert = 0; nVert < pOrigFlex->numverts; nVert++ )
							{
								mstudiovertanim_wrinkle_t	*pNewVertAnimWrinkle = pNewFlex->pVertanimWrinkle( nVert );
							}
#endif
						}
						else
						{
							float flDestAnimFixPointScale = m_pCombinedStudioHdr->VertAnimFixedPointScale();
							float flSourceAnimFixPointScale = m_pStudioHdr[ nSubModel ]->VertAnimFixedPointScale();
						
							Vector vUpdate;

							for( int nVert = 0; nVert < pOrigFlex->numverts; nVert++ )
							{
								mstudiovertanim_t	*pOrigVertAnim = pOrigFlex->pVertanim( nVert );
								mstudiovertanim_t	*pNewVertAnim = pNewFlex->pVertanim( nVert );

								int nVertIndex = pOrigVertAnim->index;
								nVertIndex += pOrigFlexMesh->vertexoffset;

								vUpdate = pNewVertAnim->GetDeltaFloat();
								vUpdate /= flSourceAnimFixPointScale;
								vUpdate *= flDestAnimFixPointScale;
								pNewVertAnim->SetDeltaFloat( vUpdate );

								vUpdate = pNewVertAnim->GetNDeltaFloat();
								vUpdate /= flSourceAnimFixPointScale;
								vUpdate *= flDestAnimFixPointScale;
								pNewVertAnim->SetNDeltaFloat( vUpdate );

								pNewVertAnim->index = m_nVertexRemap[ nSubModel ][ nVertIndex ] - pNewMesh->vertexoffset;
							}
						}
					}
				}
			}
		}
	}
}


void CModelCombine::CombineMDL_Model( )
{
	studiohdr_t *pPrimaryStudioHdr = m_pStudioHdr[ 0 ];

	studiohdr_t *pFlexStudioHdr = NULL;

	for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
	{
		studiohdr_t *pStudioHdr = m_pStudioHdr[ nModel ];
		if ( pStudioHdr->numflexdesc )
		{
#if 0
			if ( pFlexStudioHdr )
			{
				Assert( 0 );
				V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "model %s and %s both have flex specifications", pFlexStudioHdr->pszName(), pStudioHdr->pszName() );
				throw( COMBINE_RESULT_FLAG_UNSUPPORTED_FEATURE );
			}
#endif
			pFlexStudioHdr = pStudioHdr;
			m_nFlexModelSource = nModel;
			break;
		}
	}

	if ( !pFlexStudioHdr )
	{
		pFlexStudioHdr = pPrimaryStudioHdr;
		m_nFlexModelSource = 0;
	}

	VerifyField( offsetof( studiohdr_t, bodypartindex ), "Body Part Sequence" );

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->bodypartindex );
	g_CombinerWriter.WriteBuffer( pPrimaryStudioHdr->pBodypart( 0 ), pPrimaryStudioHdr->numbodyparts * sizeof( mstudiobodyparts_t ) );

	m_pCombinedModels = ( mstudiomodel_t * )g_CombinerWriter.GetWritePos();

	int nTotalModels = 0;
	for ( int nBodyPart = 0; nBodyPart < pPrimaryStudioHdr->numbodyparts; nBodyPart++ )
	{
		mstudiobodyparts_t	*pOrigBodyPart = pPrimaryStudioHdr->pBodypart( nBodyPart );
		mstudiobodyparts_t	*pNewBodyPart = m_pCombinedStudioHdr->pBodypart( nBodyPart );

		AddToStringTable( pNewBodyPart, &pNewBodyPart->sznameindex, pOrigBodyPart->pszName() );
		g_CombinerWriter.WriteOffset( pNewBodyPart->modelindex, pNewBodyPart );

		for( int nModel = 0; nModel < pOrigBodyPart->nummodels; nModel++ )
		{	// build our list of master models
			m_pMasterModels[ 0 ][ nTotalModels ] = m_pMasterFlexModels[ nTotalModels ] = pOrigBodyPart->pModel( nModel );
			g_CombinerWriter.WriteBuffer( m_pMasterModels[ 0 ][ nTotalModels ], sizeof( mstudiomodel_t ) );

			for( int nSubModels = 1; nSubModels < m_pCombinedStudioData->m_nNumModels; nSubModels++ )
			{
				m_pMasterModels[ nSubModels ][ nTotalModels ] = NULL;

				studiohdr_t *pSubStudioHdr = m_pStudioHdr[ nSubModels ];
				if ( nBodyPart < pSubStudioHdr->numbodyparts )
				{
					mstudiobodyparts_t	*pSubBodyPart = pSubStudioHdr->pBodypart( nBodyPart );
					if ( nModel < pSubBodyPart->nummodels )
					{	// if a sub model matches up to the body part
						m_pCombinedModels[ nTotalModels ].numvertices += pSubBodyPart->pModel( nModel )->numvertices;
						if ( nSubModels == m_nFlexModelSource )
						{	// if it is part of the primary flex specification
							m_pMasterFlexModels[ nTotalModels ] = pSubBodyPart->pModel( nModel );
						}
						m_pMasterModels[ nSubModels ][ nTotalModels ] = pSubBodyPart->pModel( nModel );
					}
				}
			}
			nTotalModels++;
		}
	}
	g_CombinerWriter.AlignWrite( 4 );

	m_pCombinedStudioHdr->numflexdesc = pFlexStudioHdr->numflexdesc;
	m_pCombinedStudioHdr->numflexcontrollers = pFlexStudioHdr->numflexcontrollers;
	m_pCombinedStudioHdr->numflexrules = pFlexStudioHdr->numflexrules;
	m_pCombinedStudioHdr->numflexcontrollerui = pFlexStudioHdr->numflexcontrollerui;

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->flexdescindex );
	VerifyField( offsetof( studiohdr_t, flexdescindex ), "Flex Desc" );
	if ( pFlexStudioHdr->numflexdesc > 0 )
	{
		g_CombinerWriter.WriteBuffer( pFlexStudioHdr->pFlexdesc( 0 ), pFlexStudioHdr->numflexdesc * sizeof( mstudioflexdesc_t ) );
	}
	g_CombinerWriter.AlignWrite( 4 );
	for ( int nFlexDesc = 0; nFlexDesc < pFlexStudioHdr->numflexdesc; nFlexDesc++ )
	{
		mstudioflexdesc_t	*pOrigFlexDesc = pFlexStudioHdr->pFlexdesc( nFlexDesc );
		mstudioflexdesc_t	*pNewFlexDesc = m_pCombinedStudioHdr->pFlexdesc( nFlexDesc );
		AddToStringTable( pNewFlexDesc, &pNewFlexDesc->szFACSindex, pOrigFlexDesc->pszFACS() );
	}

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->flexcontrollerindex );
	VerifyField( offsetof( studiohdr_t, flexdescindex ), "Flex Controller" );
	if ( pFlexStudioHdr->numflexcontrollers > 0 )
	{
		g_CombinerWriter.WriteBuffer( pFlexStudioHdr->pFlexcontroller( ( LocalFlexController_t )0 ), pFlexStudioHdr->numflexcontrollers * sizeof( mstudioflexcontroller_t ) );
		for( int nFlexController = 0; nFlexController < pFlexStudioHdr->numflexcontrollers; nFlexController++ )
		{
			mstudioflexcontroller_t	*pOrigFlexController = pFlexStudioHdr->pFlexcontroller( ( LocalFlexController_t )nFlexController );
			mstudioflexcontroller_t	*pNewFlexController = m_pCombinedStudioHdr->pFlexcontroller( ( LocalFlexController_t )nFlexController );

			AddToStringTable( pNewFlexController, &pNewFlexController->sznameindex, pOrigFlexController->pszName() );
			AddToStringTable( pNewFlexController, &pNewFlexController->sztypeindex, pOrigFlexController->pszType() );
		}
	}

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->flexruleindex );
	VerifyField( offsetof( studiohdr_t, flexruleindex ), "Flex Rules" );
	if ( pFlexStudioHdr->numflexrules > 0 )
	{
		g_CombinerWriter.WriteBuffer( pFlexStudioHdr->pFlexRule( 0 ), pFlexStudioHdr->numflexrules * sizeof( mstudioflexrule_t ) );
		g_CombinerWriter.AlignWrite( 4 );
		for( int nFlexRule = 0; nFlexRule < pFlexStudioHdr->numflexrules; nFlexRule++ )
		{
			mstudioflexrule_t	*pOrigFlexRule = pFlexStudioHdr->pFlexRule( nFlexRule );
			mstudioflexrule_t	*pNewFlexRule = m_pCombinedStudioHdr->pFlexRule( nFlexRule );

			g_CombinerWriter.WriteOffset( pNewFlexRule->opindex, pNewFlexRule );
			g_CombinerWriter.WriteBuffer( pOrigFlexRule->iFlexOp( 0 ), pOrigFlexRule->numops * sizeof( mstudioflexop_t ) );

			g_CombinerWriter.AlignWrite( 4 );
		}
	}

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->flexcontrolleruiindex );
	VerifyField( offsetof( studiohdr_t, flexcontrolleruiindex ), "Flex Controller UI" );
	if ( pFlexStudioHdr->numflexcontrollerui > 0 )
	{
		g_CombinerWriter.WriteBuffer( pFlexStudioHdr->pFlexControllerUI( 0 ), pFlexStudioHdr->numflexcontrollerui * sizeof( mstudioflexcontrollerui_t ) );
		for( int nFlexControllerUI = 0; nFlexControllerUI < pFlexStudioHdr->numflexcontrollerui; nFlexControllerUI++ )
		{
			mstudioflexcontrollerui_t	*pOrigFlexControllerUI = pFlexStudioHdr->pFlexControllerUI( nFlexControllerUI );
			mstudioflexcontrollerui_t	*pNewFlexControllerUI = m_pCombinedStudioHdr->pFlexControllerUI( nFlexControllerUI );

			AddToStringTable( pNewFlexControllerUI, &pNewFlexControllerUI->sznameindex, pOrigFlexControllerUI->pszName() );

			// not worrying about remapping the controller indexes here yet
		}
		g_CombinerWriter.AlignWrite( 4 );
	}

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->ikchainindex );
	VerifyField( offsetof( studiohdr_t, ikchainindex ), "IK Chain" );
	if ( pPrimaryStudioHdr->numikchains > 0 )
	{
		g_CombinerWriter.WriteBuffer( pPrimaryStudioHdr->pIKChain( 0 ), pPrimaryStudioHdr->numikchains * sizeof( mstudioikchain_t ) );
		g_CombinerWriter.AlignWrite( 4 );

		for( int nIKChain = 0; nIKChain < pPrimaryStudioHdr->numikchains; nIKChain++ )
		{
			mstudioikchain_t	*pOrigIKChain = pPrimaryStudioHdr->pIKChain( nIKChain );
			mstudioikchain_t	*pNewIKChain = m_pCombinedStudioHdr->pIKChain( nIKChain );

			AddToStringTable( pNewIKChain, &pNewIKChain->sznameindex, pNewIKChain->pszName() );

			g_CombinerWriter.WriteOffset( pNewIKChain->linkindex, pNewIKChain );
			g_CombinerWriter.WriteBuffer( pOrigIKChain->pLink( 0 ), pOrigIKChain->numlinks * sizeof( mstudioiklink_t ) );
		}
	}

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->localikautoplaylockindex );
	VerifyField( offsetof( studiohdr_t, localikautoplaylockindex ), "IK Autoplay Lock" );
	if ( pPrimaryStudioHdr->numlocalikautoplaylocks > 0 )
	{
		g_CombinerWriter.WriteBuffer( pPrimaryStudioHdr->pLocalIKAutoplayLock( 0 ), pPrimaryStudioHdr->numlocalikautoplaylocks * sizeof( mstudioiklock_t ) );
		g_CombinerWriter.AlignWrite( 4 );
	}

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->mouthindex );
	VerifyField( offsetof( studiohdr_t, mouthindex ), "Mouth" );
	if ( pPrimaryStudioHdr->nummouths )
	{
		g_CombinerWriter.WriteBuffer( pPrimaryStudioHdr->pMouth( 0 ), pPrimaryStudioHdr->nummouths * sizeof( mstudiomouth_t ) );
		g_CombinerWriter.AlignWrite( 4 );
	}

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->localposeparamindex );
	VerifyField( offsetof( studiohdr_t, localposeparamindex ), "Pose Param" );
	if ( pPrimaryStudioHdr->numlocalposeparameters > 0 )
	{
		g_CombinerWriter.WriteBuffer( pPrimaryStudioHdr->pLocalPoseParameter( 0 ), pPrimaryStudioHdr->numlocalposeparameters * sizeof( mstudioposeparamdesc_t ) );
		g_CombinerWriter.AlignWrite( 4 );
		for( int nPoseParam = 0; nPoseParam < pPrimaryStudioHdr->numlocalposeparameters; nPoseParam++ )
		{
			mstudioposeparamdesc_t	*pOrigPoseParam = pPrimaryStudioHdr->pLocalPoseParameter( nPoseParam );
			mstudioposeparamdesc_t	*pNewPoseParam = m_pCombinedStudioHdr->pLocalPoseParameter( nPoseParam );

			AddToStringTable( pNewPoseParam, &pNewPoseParam->sznameindex, pOrigPoseParam->pszName() );
		}
	}

	for( int nModel = 0; nModel < nTotalModels; nModel++ )
	{
		WriteModel( nModel );		
	}

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->includemodelindex );
	VerifyField( offsetof( studiohdr_t, includemodelindex ), "Model Groups" );
	if ( pPrimaryStudioHdr->numincludemodels > 0 )
	{
		g_CombinerWriter.WriteBuffer( pPrimaryStudioHdr->pModelGroup( 0 ), pPrimaryStudioHdr->numincludemodels * sizeof( mstudiomodelgroup_t ) );
		for( int nModelGroup = 0; nModelGroup < pPrimaryStudioHdr->numincludemodels; nModelGroup++ )
		{
			mstudiomodelgroup_t	*pOrigModelGroup = pPrimaryStudioHdr->pModelGroup( nModelGroup );
			mstudiomodelgroup_t	*pNewModelGroup = m_pCombinedStudioHdr->pModelGroup( nModelGroup );

			AddToStringTable( pNewModelGroup, &pNewModelGroup->sznameindex, pOrigModelGroup->pszName() );
		}
	}

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->animblockindex );
	VerifyField( offsetof( studiohdr_t, animblockindex ), "Anim Blocks" );
	if ( pPrimaryStudioHdr->numanimblocks > 0 )
	{
		g_CombinerWriter.WriteBuffer( pPrimaryStudioHdr->pAnimBlock( 0 ), pPrimaryStudioHdr->numanimblocks * sizeof( mstudioanimblock_t ) );

		Assert( 0 );
		V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "model has anim blocks: %s", pPrimaryStudioHdr->pszName() );
		throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
	}

	g_CombinerWriter.AlignWrite( 4 );
	AddToStringTable( m_pCombinedStudioHdr, &m_pCombinedStudioHdr->szanimblocknameindex, pPrimaryStudioHdr->pszAnimBlockName() );
}


void CModelCombine::CombineMDL_Textures( )
{
	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->textureindex );

	m_pCombinedStudioHdr->numtextures = m_pCombinedStudioData->m_nNumAtlasGroups + m_pCombinedStudioData->m_nNumNonAtlasedMaterialBaseNames;
	
	// first allocate/write out "blank" texture entries (materials)
	mstudiotexture_t NewTexture;
	memset( &NewTexture, 0, sizeof( NewTexture ) );
	NewTexture.sznameindex = 0;
	NewTexture.used = 1;
	for ( int nAtlasGroup = 0; nAtlasGroup < m_pCombinedStudioHdr->numtextures; nAtlasGroup++ )
	{
		g_CombinerWriter.WriteBuffer( &NewTexture, sizeof( mstudiotexture_t ) );
	}
	g_CombinerWriter.AlignWrite( 4 );

	// then fix up those written entries with proper name index values from the string table
	for ( int nAtlasGroup = 0; nAtlasGroup < m_pCombinedStudioData->m_nNumAtlasGroups; nAtlasGroup++ )
	{
		mstudiotexture_t *pNewTexture = m_pCombinedStudioHdr->pTexture( nAtlasGroup );
		// we use a specialized name here that other systems won't understand 
		V_sprintf_safe( m_pCombinedStudioData->m_AtlasGroups[ nAtlasGroup ].m_szCombinedMaterialName, "!%s|%d|%hu|%d!", m_pCombinedStudioData->m_szCombinedModelName, nAtlasGroup, m_pCombinedStudioData->m_FinalHandle, GetNextAssetID() );
		AddToStringTable( pNewTexture, &pNewTexture->sznameindex, m_pCombinedStudioData->m_AtlasGroups[ nAtlasGroup ].m_szCombinedMaterialName );
	}
	for ( int nNonAtlasedMaterial = 0; nNonAtlasedMaterial < m_pCombinedStudioData->m_nNumNonAtlasedMaterialBaseNames; nNonAtlasedMaterial++ )
	{
		int nMaterial = m_pCombinedStudioData->m_nNumAtlasGroups + nNonAtlasedMaterial;
		mstudiotexture_t *pNewTexture = m_pCombinedStudioHdr->pTexture( nMaterial );
		AddToStringTable( pNewTexture, &pNewTexture->sznameindex, m_pCombinedStudioData->m_szNonAtlasedMaterialBaseName[ nNonAtlasedMaterial ] );
	}
	
	// write out material paths (used for non atlased materials)
	int *pNewCDTexture = ( int * )g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->cdtextureindex );
	m_pCombinedStudioHdr->numcdtextures = m_pCombinedStudioData->m_nNumNonAtlasedMaterialPaths;
	g_CombinerWriter.AllocWrite( m_pCombinedStudioHdr->numcdtextures * sizeof( int ) );
	g_CombinerWriter.AlignWrite( 4 );

	for ( int nMaterialPaths = 0; nMaterialPaths < m_pCombinedStudioData->m_nNumNonAtlasedMaterialPaths; nMaterialPaths++ )
	{
		AddToStringTable( m_pCombinedStudioHdr, &pNewCDTexture[ nMaterialPaths ], m_pCombinedStudioData->m_szNonAtlasedMaterialPaths[ nMaterialPaths ] );
	}

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->skinindex );
	m_pCombinedStudioHdr->numskinref = m_pCombinedStudioHdr->numtextures;
	m_pCombinedStudioHdr->numskinfamilies = 1;
	for ( short nTexture = 0; nTexture < static_cast< short >( m_pCombinedStudioHdr->numtextures ); nTexture++ )
	{
		g_CombinerWriter.WriteBuffer( &nTexture, sizeof( short ) );
	}
	g_CombinerWriter.AlignWrite( 4 );
}


void CModelCombine::CombineMDL_KeyValues( )
{
	studiohdr_t *pStudioHdr = m_pStudioHdr[ 0 ];

	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr->keyvalueindex );
	VerifyField( offsetof( studiohdr_t, keyvalueindex ), "KeyValues" );

	if ( pStudioHdr->keyvaluesize > 0 )
	{
		g_CombinerWriter.WriteBuffer( pStudioHdr->KeyValueText(), pStudioHdr->keyvaluesize );
	}

	g_CombinerWriter.AlignWrite( 4 );
}


#define WRITE_BONE_BLOCK( type, srcfield, dest, destindex )						\
	g_CombinerWriter.WriteOffset( pNewLinearBone->destindex, pNewLinearBone );					\
	type *dest = (type *)g_CombinerWriter.AllocWrite( m_nNumMasterBones * sizeof( type ) );		\
	g_CombinerWriter.AlignWrite( 4 );															\
	for ( int i = 0; i < m_nNumMasterBones; i++)								\
	{																			\
		dest[ i ] = m_pMasterBoneList[ i ]->srcfield;							\
	}


void CModelCombine::CombineMDL_BoneTransforms( )
{
	g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr2->srcbonetransformindex );
	VerifyField2( offsetof( studiohdr2_t, srcbonetransformindex ), "Bone Transforms" );
	// need to merge bone transforms from the sub models?
	if ( m_pStudioHdr2[ 0 ]->numsrcbonetransform > 0 )
	{
		g_CombinerWriter.WriteBuffer( m_pStudioHdr[ 0 ]->SrcBoneTransform( 0 ), m_pStudioHdr2[ 0 ]->numsrcbonetransform * sizeof( mstudiosrcbonetransform_t ) );
		for( int nBoneTransform = 0; nBoneTransform < m_pStudioHdr2[ 0 ]->numsrcbonetransform; nBoneTransform++ )
		{
			mstudiosrcbonetransform_t	*pOrigBoneTransform = ( mstudiosrcbonetransform_t * )m_pStudioHdr[ 0 ]->SrcBoneTransform( nBoneTransform );
			mstudiosrcbonetransform_t	*pNewBoneTransform = ( mstudiosrcbonetransform_t * )m_pCombinedStudioHdr->SrcBoneTransform( nBoneTransform );

			AddToStringTable( pNewBoneTransform, &pNewBoneTransform->sznameindex, pOrigBoneTransform->pszName() );
		}
	}
	g_CombinerWriter.AlignWrite( 4 );

	if ( m_pStudioHdr2[ 0 ]->pLinearBones() != NULL )
	{
		g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr2->linearboneindex, m_pCombinedStudioHdr2 );
		VerifyField2( offsetof( studiohdr2_t, linearboneindex ), "Linear Bone Index" );
		mstudiolinearbone_t *pNewLinearBone =  ( mstudiolinearbone_t * )g_CombinerWriter.WriteBuffer( m_pStudioHdr2[ 0 ]->pLinearBones(), sizeof( mstudiolinearbone_t ) );
		pNewLinearBone->numbones = m_nNumMasterBones;

		WRITE_BONE_BLOCK( int, flags, pFlags, flagsindex );
		WRITE_BONE_BLOCK( int, parent, pParent, parentindex );
		WRITE_BONE_BLOCK( Vector, pos, pPos, posindex );
		WRITE_BONE_BLOCK( Quaternion, quat, pQuat, quatindex );
		WRITE_BONE_BLOCK( RadianEuler, rot, pRot, rotindex );
		WRITE_BONE_BLOCK( matrix3x4_t, poseToBone, pPoseToBone, posetoboneindex );
		WRITE_BONE_BLOCK( Vector, posscale, pPoseScale, posscaleindex );
		WRITE_BONE_BLOCK( Vector, rotscale, pRotScale, rotscaleindex );
		WRITE_BONE_BLOCK( Quaternion, qAlignment, pQAlignment, qalignmentindex );

		for( int nBone = 0; nBone < m_nNumMasterBones; nBone++ )
		{
			mstudiobone_t *pNewBone = (mstudiobone_t *)m_pCombinedStudioHdr->pBone( nBone );

			*pNewLinearBone->pflags( nBone ) = pNewBone->flags;
		}
	}
}


void CModelCombine::CombineMDL_BoneFlexDrivers( )
{
	m_pCombinedStudioHdr2->m_nBoneFlexDriverCount = m_pStudioHdr2[ m_nFlexModelSource ]->m_nBoneFlexDriverCount;

	if ( m_pStudioHdr2[ m_nFlexModelSource ]->m_nBoneFlexDriverCount > 0 )
	{
		g_CombinerWriter.WriteOffset( m_pCombinedStudioHdr2->m_nBoneFlexDriverIndex, m_pCombinedStudioHdr2 );
		VerifyField2( offsetof( studiohdr2_t, m_nBoneFlexDriverIndex ), "Bone Flex Drivers" );
		g_CombinerWriter.WriteBuffer( m_pStudioHdr2[ m_nFlexModelSource ]->pBoneFlexDriver( 0 ), m_pStudioHdr2[ m_nFlexModelSource ]->m_nBoneFlexDriverCount * sizeof( mstudioboneflexdriver_t ) );
		g_CombinerWriter.AlignWrite( 4 );

		for( int nBoneFlexDriver = 0; nBoneFlexDriver < m_pStudioHdr2[ m_nFlexModelSource ]->m_nBoneFlexDriverCount; nBoneFlexDriver++ )
		{
			mstudioboneflexdriver_t	*pOrigBoneFlexDriver = m_pStudioHdr2[ m_nFlexModelSource ]->pBoneFlexDriver( nBoneFlexDriver );
			mstudioboneflexdriver_t	*pNewBoneFlexDriver = m_pCombinedStudioHdr2->pBoneFlexDriver( nBoneFlexDriver );

			g_CombinerWriter.WriteOffset( pNewBoneFlexDriver->m_nControlIndex, pNewBoneFlexDriver );
			g_CombinerWriter.WriteBuffer( pOrigBoneFlexDriver->pBoneFlexDriverControl( 0 ), pOrigBoneFlexDriver->m_nControlCount * sizeof( mstudioboneflexdrivercontrol_t ) );
/*			for( int nBoneFlexDriverControl = 0; nBoneFlexDriverControl < pOrigBoneFlexDriver->m_nControlCount; nBoneFlexDriverControl++ )
			{
				mstudioboneflexdrivercontrol_t	*pOrigBoneFlexDriverControl = pOrigBoneFlexDriver->pBoneFlexDriverControl( nBoneFlexDriverControl );
				mstudioboneflexdrivercontrol_t	*pNewBoneFlexDriverControl = pNewBoneFlexDriver->pBoneFlexDriverControl( nBoneFlexDriver );
			}*/
			g_CombinerWriter.AlignWrite( 4 );
		}
	}
}


void CModelCombine::CombineMDL_AssignMeshIDs( )
{
	int					i;
	int					j;
	int					m;
	int					numMeshes;
	mstudiobodyparts_t	*pStudioBodyPart;
	mstudiomodel_t		*pStudioModel;
	mstudiomesh_t		*pStudioMesh;

	numMeshes = 0;
	for (i=0; i<m_pCombinedStudioHdr->numbodyparts; i++)
	{
		pStudioBodyPart = m_pCombinedStudioHdr->pBodypart(i);
		for (j=0; j<pStudioBodyPart->nummodels; j++)
		{
			pStudioModel = pStudioBodyPart->pModel(j);
			for (m=0; m<pStudioModel->nummeshes; m++)
			{				
				// get each mesh
				pStudioMesh = pStudioModel->pMesh(m);
				pStudioMesh->meshid = numMeshes + m;
			}
			numMeshes += pStudioModel->nummeshes;
		}
	}
}


void CModelCombine::CombineMDL( bool bNoStringTable )
{
	g_CombinerWriter.AllocWrite( 256 );
	g_CombinerWriter.AlignWrite( 16 );
	g_CombinerWriter.InitWriteArea( WRITE_AREA_MDL, g_CombinerWriter.GetWritePos() );
	g_CombinerWriter.SetWriteArea( WRITE_AREA_MDL );

	m_nFlexModelSource = -1;

	studiohdr_t *pPrimaryStudioHdr = m_pStudioHdr[ 0 ];

	m_pCombinedStudioHdr = ( studiohdr_t * )g_CombinerWriter.WriteBuffer( pPrimaryStudioHdr, sizeof( *pPrimaryStudioHdr ) );
	m_pCombinedStudioHdr2 = ( studiohdr2_t * )g_CombinerWriter.WriteBufferWithOffset( pPrimaryStudioHdr->pStudioHdr2(), sizeof( studiohdr2_t ), m_pCombinedStudioHdr->studiohdr2index );

	m_pCombinedStudioHdr->flags |= STUDIOHDR_FLAGS_COMBINED;

	BeginStringTable();
	CombineMDL_PreintStrings();

	AddToStringTable( m_pCombinedStudioHdr2, &m_pCombinedStudioHdr2->sznameindex, m_pStudioHdr2[ 0 ]->pszName() );

	CombineMDL_Bones();
	CombineMDL_Anims();
	CombineMDL_SequenceInfo();
	CombineMDL_Model();
	CombineMDL_Textures();
	CombineMDL_KeyValues();
	CombineMDL_BoneTransforms();
	CombineMDL_BoneFlexDrivers();

//	int nStringOffset = m_pWritePos - m_pWriteArea;
//	const char *pszOrigStrings = ( ( char * ) m_pStudioHdr[ 0 ] ) + nStringOffset;
//	const char *pszNewStrings = ( char * )m_pWritePos;

	if ( !bNoStringTable )
	{
		WriteStringTable();
	}

	int nTotal = g_CombinerWriter.GetWritePos() - g_CombinerWriter.GetWriteArea();
	m_pCombinedStudioHdr->length = nTotal;

	m_pCombinedStudioHdr->checksum = 0;
	for ( int i = 0; i < nTotal; i += 4 )
	{
		// TODO: does this need something more than a simple shift left and add checksum?
		m_pCombinedStudioHdr->checksum = ( m_pCombinedStudioHdr->checksum << 1 ) + 
			( ( m_pCombinedStudioHdr->checksum & 0x8000000 ) ? 1 : 0 ) + *( ( long * )( g_CombinerWriter.GetWriteArea() + i ) );
	}

	m_pCombinedHardwareHeader->checkSum = m_pCombinedStudioHdr->checksum;
	m_pCombinedVertex->checksum = m_pCombinedStudioHdr->checksum;

	CombineMDL_AssignMeshIDs();
}


#ifdef DEBUG_COMBINE

void CModelCombine::TestCombineMDL( )
{
	int		nLength = m_pCombinedStudioHdr->length;
	char	*pOrig = ( char * )m_pStudioHdr[ 0 ];
	char	*pNew = ( char * )m_pCombinedStudioHdr;
	char	*pStart = pNew;

	if ( nLength != m_pStudioHdr[ 0 ]->length )
	{
		Msg( "Test Failure: Original Length: %d, New Length: %d\n", m_pStudioHdr[ 0 ]->length, nLength );
		if ( nLength > m_pStudioHdr[ 0 ]->length )
		{
			nLength = m_pStudioHdr[ 0 ]->length;
		}
	}

	int nOffset = sizeof( studiohdr_t ) + sizeof( studiohdr2_t );
	nLength -= nOffset;
	pOrig += nOffset;
	pNew += nOffset;

	char *pErrorPos = NULL;

	for( ; nLength; pOrig++, pNew++, nLength-- )
	{
		if ( ( *pOrig ) != ( *pNew ) )
		{
			pErrorPos = pNew;
			break;
		}
	}

	if ( pErrorPos != NULL )
	{
		Msg( "Test Failure: Offset %d\n", pNew - pStart );
		Init( m_pCombinedStudioData );
		m_pErrorPos = pErrorPos;

		// need to do VVD and VTX to recreate memory
		CombineVVD();
		CombineVTX();

		CombineMDL( true );
	}
}

#endif // DEBUG_COMBINE


void CModelCombine::CalcVTXInfo()
{
	for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
	{
		OptimizedModel::FileHeader_t *pOrigHeader = ( OptimizedModel::FileHeader_t * )VTX_Data[ nModel ]->PeekGet();

		if ( pOrigHeader->numBodyParts > m_MaxHardwareData.m_nMaxBodyParts )
		{
			m_MaxHardwareData.m_nMaxBodyParts = pOrigHeader->numBodyParts;
		}

		for( int nBodyPart = 0; nBodyPart < pOrigHeader->numBodyParts; nBodyPart++ )
		{
			OptimizedModel::BodyPartHeader_t *pOrigBodyPart = pOrigHeader->pBodyPart( nBodyPart );
			int nSubModelToUse = m_pCombinedStudioData->m_ModelInputData[ nModel ].m_nBodyGroupSubModelSelection;
			for( int nBodyModel = 0; nBodyModel < pOrigBodyPart->numModels; nBodyModel++ )
			{
				// skip over sub models that are not the selected one (-1 no selection, so no skipping)
				if ( nSubModelToUse != -1 && nBodyModel != nSubModelToUse )
				{
					continue;
				}
				OptimizedModel::ModelHeader_t *pOrigModel = pOrigBodyPart->pModel( nBodyModel );
				for ( int nLOD = 0; nLOD < pOrigModel->numLODs; nLOD++ )
				{
					OptimizedModel::ModelLODHeader_t *pOrigLOD = pOrigModel->pLOD( nLOD );
					unsigned char nStripGroupFlags = OptimizedModel::STRIPGROUP_IS_HWSKINNED;
					for (int i = 0; i < 2; i++ )
					{
						if ( i > 0 )
						{
							nStripGroupFlags |= OptimizedModel::STRIPGROUP_IS_DELTA_FLEXED;
						}
						for( int nMesh = 0; nMesh < pOrigLOD->numMeshes; nMesh++ )
						{
							OptimizedModel::MeshHeader_t *pOrigMesh = pOrigLOD->pMesh( nMesh );
							bool bUsed = false;
							for( int nStripGroup = 0; nStripGroup < pOrigMesh->numStripGroups; nStripGroup++ )
							{
								OptimizedModel::StripGroupHeader_t	*pOrigStripGroup = pOrigMesh->pStripGroup( nStripGroup );
								if ( pOrigStripGroup->flags == nStripGroupFlags )
								{	// we are compatible
									for( int nStrip = 0; nStrip < pOrigStripGroup->numStrips; nStrip++ )
									{
										OptimizedModel::StripHeader_t *pOrigStrip = pOrigStripGroup->pStrip( nStrip );
								
										if ( pOrigStrip->numTopologyIndices > 0 )
										{
											Assert( 0 );
											V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "strip has topology indices" );
											throw( COMBINE_RESULT_FLAG_UNSUPPORTED_FEATURE );
										}
	
										m_MaxHardwareData.m_nBoneStateChanges += pOrigStrip->numBoneStateChanges;
										m_MaxHardwareData.m_nIndices += pOrigStrip->numIndices;
										m_MaxHardwareData.m_nVerts += pOrigStrip->numVerts;
										m_MaxHardwareData.m_nStrips++;
									}
									bUsed = true;
								}
							}
							if ( bUsed )
							{	// we put things into this strip group
								m_MaxHardwareData.m_nStripGroups++;
							}
						}
					}
					m_MaxHardwareData.m_nModelLODs++;
				}
			}
			m_MaxHardwareData.m_nBodyParts++;
		}
	}
}


void CModelCombine::WriteStrip( OptimizedModel::StripGroupHeader_t	*pNewStripGroup, 
								int nModel, OptimizedModel::StripGroupHeader_t *pOrigStripGroup, OptimizedModel::StripHeader_t *pOrigStrip )
{
	OptimizedModel::Vertex_t		*pNewVert = pNewStripGroup->pVertex( pNewStripGroup->numVerts );
//	OptimizedModel::Vertex_t		*pOrigVert = pOrigStripGroup->pVertex( pOrigStrip->vertOffset );
	unsigned short					*pNewIndex = pNewStripGroup->pIndex( pNewStripGroup->numIndices );
	unsigned short					*pOrigIndex = pOrigStripGroup->pIndex( pOrigStrip->indexOffset );
	OptimizedModel::StripHeader_t	*pNewStrip = pNewStripGroup->pStrip( pNewStripGroup->numStrips );

	if ( pOrigStrip->numTopologyIndices > 0 )
	{
		Assert( 0 );
		V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "strip has topology indices" );
		throw( COMBINE_RESULT_FLAG_UNSUPPORTED_FEATURE );
	}

	Assert( m_CurrentHardwareData.m_nStrips < m_MaxHardwareData.m_nStrips );
	
	pNewStrip->numIndices = pOrigStrip->numIndices;
	pNewStrip->numTopologyIndices = pOrigStrip->numTopologyIndices;
	pNewStrip->indexOffset = pNewStripGroup->numIndices;
	pNewStrip->topologyOffset = pNewStripGroup->numTopologyIndices;
	pNewStrip->numVerts = pOrigStrip->numVerts;
	pNewStrip->vertOffset = pNewStripGroup->numVerts;
	pNewStrip->numBoneStateChanges = pOrigStrip->numBoneStateChanges;
	pNewStrip->numBones = pOrigStrip->numBones;
	pNewStrip->flags = pOrigStrip->flags;
	int boneFileOffset = m_HardwareOffsets.m_nBoneStateChanges + m_CurrentHardwareData.m_nBoneStateChanges * sizeof( OptimizedModel::BoneStateChangeHeader_t );
	int stripFileOffset = m_HardwareOffsets.m_nStrips + m_CurrentHardwareData.m_nStrips * sizeof( OptimizedModel::StripHeader_t );
	pNewStrip->boneStateChangeOffset = boneFileOffset - stripFileOffset;

	int nOffset = pNewStripGroup->numVerts - pOrigStrip->vertOffset;
	for( int nIndex = 0; nIndex < pOrigStrip->numIndices; nIndex++ )
	{
		*pNewIndex = ( *pOrigIndex ) + nOffset;
		Assert( ( *pNewIndex ) >= pNewStrip->vertOffset );
		Assert( ( *pNewIndex ) < pNewStrip->vertOffset + pNewStrip->numVerts );

		pOrigIndex++;
		pNewIndex++;
	}

//	OptimizedModel::Vertex_t *pTestVert = pOrigStripGroup->pVertex( 0 );
//	OptimizedModel::FileHeader_t		*pPrimaryHeader = ( OptimizedModel::FileHeader_t * )VTX_Data[ 0 ]->PeekGet();
//	Msg( "%d / %d\n", ( char * )pNewVert - ( char * )m_pCombinedHardwareHeader, ( char * )pTestVert - ( char * )pPrimaryHeader );

	for( int nVert = 0; nVert < pOrigStrip->numVerts; nVert++ )
	{
		OptimizedModel::Vertex_t *pOrigVert = pOrigStripGroup->pVertex( nVert + pOrigStrip->vertOffset );

		*pNewVert = *pOrigVert;
		pNewVert->origMeshVertID = m_nVertexRemap[ nModel ][ pOrigVert->origMeshVertID ];

		pNewVert++;
	}

//	pTestVert = pOrigStripGroup->pVertex( pOrigStrip->numVerts );
//	Msg( "%d / %d\n", ( char * )pNewVert - ( char * )m_pCombinedHardwareHeader, ( char * )pTestVert - ( char * )pPrimaryHeader );

	for( int nBone = 0; nBone < pOrigStrip->numBoneStateChanges; nBone++ )
	{
		OptimizedModel::BoneStateChangeHeader_t	*pNewBone = pNewStrip->pBoneStateChange( nBone );
		OptimizedModel::BoneStateChangeHeader_t	*pOrigBone = pOrigStrip->pBoneStateChange( nBone );

		pNewBone->hardwareID = pOrigBone->hardwareID;
		pNewBone->newBoneID = m_nBoneRemap[ nModel ][ pOrigBone->newBoneID ];
	}

	m_CurrentHardwareData.m_nBoneStateChanges += pNewStrip->numBoneStateChanges;

	pNewStripGroup->numIndices += pOrigStrip->numIndices;
	m_CurrentHardwareData.m_nIndices += pOrigStrip->numIndices;

	pNewStripGroup->numVerts += pOrigStrip->numVerts;
	m_CurrentHardwareData.m_nVerts += pOrigStrip->numVerts;

	pNewStripGroup->numStrips++;
	m_CurrentHardwareData.m_nStrips++;
}


void CModelCombine::MergeStripGroup( int nLOD, OptimizedModel::StripGroupHeader_t	*pNewStripGroup, int nModel, OptimizedModel::StripGroupHeader_t	*pOrigStripGroup )
{
//	OptimizedModel::Vertex_t		*pNewVert = pNewStripGroup->pVertex( pNewStripGroup->numVerts );
//	unsigned short					*pNewIndex = pNewStripGroup->pIndex( pNewStripGroup->numIndices );
//	OptimizedModel::StripHeader_t	*pNewStrip = pNewStripGroup->pStrip( pNewStripGroup->numStrips );

	for( int nStrip = 0; nStrip < pOrigStripGroup->numStrips; nStrip++ )
	{
		OptimizedModel::StripHeader_t *pOrigStrip = pOrigStripGroup->pStrip( nStrip );
		WriteStrip( pNewStripGroup, nModel, pOrigStripGroup, pOrigStrip );

		m_pCombinedStudioData->m_Results.m_nNumIndexes[ nModel ][ nLOD ] += pOrigStrip->numIndices;
	}

	m_pCombinedStudioData->m_Results.m_nBatches[ nModel ][ nLOD ] += pOrigStripGroup->numStrips;
	m_pCombinedStudioData->m_Results.m_nCombinedBatches[ nLOD ] += pOrigStripGroup->numStrips;
}


void CModelCombine::WriteStripGroup( int nLOD, int nMaterialIndex, unsigned char nStripGroupFlags, OptimizedModel::MeshHeader_t *pNewMesh )
{
	OptimizedModel::StripGroupHeader_t	*pNewStripGroup = pNewMesh->pStripGroup( pNewMesh->numStripGroups );

	pNewStripGroup->numVerts = 0;
	pNewStripGroup->numIndices = 0;
	pNewStripGroup->numTopologyIndices = 0;
	pNewStripGroup->numStrips = 0;
	pNewStripGroup->flags = nStripGroupFlags;

	int stripGroupFileOffset = m_HardwareOffsets.m_nStripGroups + m_CurrentHardwareData.m_nStripGroups * sizeof( OptimizedModel::StripGroupHeader_t );
	int vertsFileOffset = m_HardwareOffsets.m_nVerts + m_CurrentHardwareData.m_nVerts * sizeof( OptimizedModel::Vertex_t );
	int indicesFileOffset = m_HardwareOffsets.m_nIndices + m_CurrentHardwareData.m_nIndices * sizeof( unsigned short );
	int topologyFileOffset = m_HardwareOffsets.m_nTopology + m_CurrentHardwareData.m_nTopology * sizeof( unsigned short );
	int stripsFileOffset = m_HardwareOffsets.m_nStrips + m_CurrentHardwareData.m_nStrips * sizeof( OptimizedModel::StripHeader_t );
	pNewStripGroup->vertOffset = vertsFileOffset - stripGroupFileOffset;
	pNewStripGroup->indexOffset = indicesFileOffset - stripGroupFileOffset;
	pNewStripGroup->topologyOffset = topologyFileOffset - stripGroupFileOffset;
	pNewStripGroup->stripOffset = stripsFileOffset - stripGroupFileOffset;

	for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
	{
		OptimizedModel::FileHeader_t	*pOrigHeader = ( OptimizedModel::FileHeader_t * )VTX_Data[ nModel ]->PeekGet();

		for( int nBodyPart = 0; nBodyPart < pOrigHeader->numBodyParts; nBodyPart++ )
		{
			OptimizedModel::BodyPartHeader_t	*pOrigBodyPart = pOrigHeader->pBodyPart( nBodyPart );

			int nSubModelToUse = m_pCombinedStudioData->m_ModelInputData[ nModel ].m_nBodyGroupSubModelSelection;

			for( int nBodyModel = 0; nBodyModel < pOrigBodyPart->numModels; nBodyModel++ )
			{
				// skip over sub models that are not the selected one (-1 no selection, so no skipping)
				if ( nSubModelToUse != -1 && nBodyModel != nSubModelToUse )
				{
					continue;
				}

				OptimizedModel::ModelHeader_t	*pOrigModel = pOrigBodyPart->pModel( nBodyModel );

				int nPickLOD = nLOD;
				if ( nLOD >= pOrigModel->numLODs )
				{
					nPickLOD = pOrigModel->numLODs - 1;
				}

				OptimizedModel::ModelLODHeader_t	*pOrigLOD = pOrigModel->pLOD( nPickLOD );

				for( int nMesh = 0; nMesh < pOrigLOD->numMeshes; nMesh++ )
				{
					OptimizedModel::MeshHeader_t	*pOrigMesh = pOrigLOD->pMesh( nMesh );

					// determine the new material index
					int nOriginalMaterialIndex = m_pCombinedStudioData->m_MeshToMaterialMap[ nModel ][ nBodyPart ][ nMesh ];
					int nNewMaterialIndex = GetTextureCombiner().GetAtlasGroupIndex( nOriginalMaterialIndex );
					if ( nNewMaterialIndex == -1 )
					{
						nNewMaterialIndex = m_pCombinedStudioData->m_nNumAtlasGroups + GetTextureCombiner().GetAtlasGroupMaterialIndex( nOriginalMaterialIndex );
					}

					if ( nMaterialIndex == nNewMaterialIndex )
					{
						for( int nStripGroup = 0; nStripGroup < pOrigMesh->numStripGroups; nStripGroup++ )
						{
							OptimizedModel::StripGroupHeader_t	*pOrigStripGroup = pOrigMesh->pStripGroup( nStripGroup );
							if ( pOrigStripGroup->flags == pNewStripGroup->flags )
							{	// we are compatible
								MergeStripGroup( nLOD, pNewStripGroup, nModel, pOrigStripGroup );
							}
						}
					}
				}
			}
		}
	}

	m_pCombinedStudioData->m_Results.m_nCombinedNumIndexes[ nLOD ] += pNewStripGroup->numIndices;

	if ( pNewStripGroup->numIndices )
	{	// we put things into this strip group
		pNewMesh->numStripGroups++;
		m_CurrentHardwareData.m_nStripGroups++;
	}
}


void CModelCombine::WriteMeshes( int nLOD, int nMaterialIndex, OptimizedModel::ModelLODHeader_t *pNewModelLOD, OptimizedModel::ModelLODHeader_t *pOrigModelLOD )
{
	OptimizedModel::MeshHeader_t	*pNewMesh = pNewModelLOD->pMesh( pNewModelLOD->numMeshes );
	OptimizedModel::MeshHeader_t	*pOrigMesh = pOrigModelLOD->pMesh( 0 );

	int meshFileOffset = m_HardwareOffsets.m_nMeshes + m_CurrentHardwareData.m_nMeshes * sizeof( OptimizedModel::MeshHeader_t );
	int stripGroupFileOffset = m_HardwareOffsets.m_nStripGroups + m_CurrentHardwareData.m_nStripGroups * sizeof( OptimizedModel::StripGroupHeader_t );
	pNewMesh->stripGroupHeaderOffset = stripGroupFileOffset - meshFileOffset;

	pNewMesh->flags = pOrigMesh->flags;

	WriteStripGroup( nLOD, nMaterialIndex, OptimizedModel::STRIPGROUP_IS_HWSKINNED, pNewMesh );
	WriteStripGroup( nLOD, nMaterialIndex, OptimizedModel::STRIPGROUP_IS_HWSKINNED | OptimizedModel::STRIPGROUP_IS_DELTA_FLEXED, pNewMesh );

	pNewModelLOD->numMeshes++;
	m_CurrentHardwareData.m_nMeshes++;
}


void CModelCombine::WriteModelLOD( int nLOD, OptimizedModel::ModelHeader_t *pNewModel, OptimizedModel::ModelHeader_t *pOrigModel )
{
	OptimizedModel::ModelLODHeader_t	*pNewModelLOD = pNewModel->pLOD( m_CurrentHardwareData.m_nModelLODs );
	OptimizedModel::ModelLODHeader_t	*pOrigModelLOD = pOrigModel->pLOD( nLOD );

	int lodFileOffset = m_HardwareOffsets.m_nModelLODs + m_CurrentHardwareData.m_nModelLODs * sizeof( OptimizedModel::ModelLODHeader_t );
	int meshFileOffset = m_HardwareOffsets.m_nMeshes + m_CurrentHardwareData.m_nMeshes * sizeof( OptimizedModel::MeshHeader_t );
	pNewModelLOD->meshOffset = meshFileOffset - lodFileOffset;

	int nTotalMaterials = m_pCombinedStudioData->m_nNumAtlasGroups + m_pCombinedStudioData->m_nNumNonAtlasedMaterialBaseNames;
	for ( int nMaterialIndex = 0; nMaterialIndex < nTotalMaterials; nMaterialIndex++ )
	{
		WriteMeshes( nLOD, nMaterialIndex, pNewModelLOD, pOrigModelLOD );
	}

	m_CurrentHardwareData.m_nModelLODs++;
}


void CModelCombine::WriteModel( int nBodyModel, OptimizedModel::BodyPartHeader_t *pNewBodyPart, OptimizedModel::BodyPartHeader_t *pOrigBodyPart )
{
	OptimizedModel::ModelHeader_t	*pNewModel = pNewBodyPart->pModel( m_CurrentHardwareData.m_nModels );
	OptimizedModel::ModelHeader_t	*pOrigModel = pOrigBodyPart->pModel( nBodyModel );

	pNewModel->numLODs = pOrigModel->numLODs;
	int modelFileOffset = m_HardwareOffsets.m_nModels + m_CurrentHardwareData.m_nModels * sizeof( OptimizedModel::ModelHeader_t );
	int lodFileOffset = m_HardwareOffsets.m_nModelLODs + m_CurrentHardwareData.m_nModelLODs * sizeof( OptimizedModel::ModelLODHeader_t );
	pNewModel->lodOffset = lodFileOffset - modelFileOffset;

	for( int nLOD = 0; nLOD < pOrigModel->numLODs; nLOD++ )
	{
		WriteModelLOD( nLOD, pNewModel, pOrigModel );
	}

	m_CurrentHardwareData.m_nModels++;
	pNewBodyPart->numModels++;
}


void CModelCombine::WriteBodyPart( int nBodyPart )
{
	for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
	{
		OptimizedModel::FileHeader_t		*pOrigHeader = ( OptimizedModel::FileHeader_t * )VTX_Data[ nModel ]->PeekGet();
		if ( pOrigHeader->numBodyParts > nBodyPart )
		{
			OptimizedModel::BodyPartHeader_t	*pOrigBodyPart = pOrigHeader->pBodyPart( nBodyPart );
			OptimizedModel::BodyPartHeader_t	*pNewBodyPart = m_pCombinedHardwareHeader->pBodyPart( nBodyPart );
			pNewBodyPart->numModels = 0;
			int bodyPartOffset = m_HardwareOffsets.m_nBodyParts + m_CurrentHardwareData.m_nBodyParts * sizeof( OptimizedModel::BodyPartHeader_t );
			int modelFileOffset = m_HardwareOffsets.m_nModels + m_CurrentHardwareData.m_nModels * sizeof( OptimizedModel::ModelHeader_t );
			pNewBodyPart->modelOffset = modelFileOffset - bodyPartOffset;

			int nBodyModel = ( m_pCombinedStudioData->m_ModelInputData[ nModel ].m_nBodyGroupSubModelSelection != -1 ) ? m_pCombinedStudioData->m_ModelInputData[ nModel ].m_nBodyGroupSubModelSelection : 0;
			WriteModel( nBodyModel, pNewBodyPart, pOrigBodyPart );
			break;
		}
	}

	m_CurrentHardwareData.m_nBodyParts++;

}


void CModelCombine::CombineVTX( )
{
	int nMaxSize = 0;

	for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
	{
		nMaxSize += VTX_Data[ nModel ]->TellMaxPut();
	}

	g_CombinerWriter.AllocWrite( 256 );
	g_CombinerWriter.AlignWrite( 16 );
	g_CombinerWriter.InitWriteArea( WRITE_AREA_VTX, g_CombinerWriter.GetWritePos() );
	g_CombinerWriter.SetWriteArea( WRITE_AREA_VTX );

	OptimizedModel::FileHeader_t	*pPrimaryHeader = ( OptimizedModel::FileHeader_t * )VTX_Data[ 0 ]->PeekGet();

//	m_pCombinedHardwareHeader = ( OptimizedModel::FileHeader_t * )g_CombinerWriter.WriteBuffer( pPrimaryHeader, VTX_Data[ 0 ]->TellMaxPut() );
//	return;


	m_pCombinedHardwareHeader = ( OptimizedModel::FileHeader_t * )g_CombinerWriter.AllocWrite( sizeof( *m_pCombinedHardwareHeader ) );
	m_pCombinedHardwareHeader->version = pPrimaryHeader->version;
	m_pCombinedHardwareHeader->vertCacheSize = pPrimaryHeader->vertCacheSize;
	m_pCombinedHardwareHeader->maxBonesPerStrip = pPrimaryHeader->maxBonesPerStrip;
	m_pCombinedHardwareHeader->maxBonesPerFace = pPrimaryHeader->maxBonesPerFace;
	m_pCombinedHardwareHeader->maxBonesPerVert = pPrimaryHeader->maxBonesPerVert;
	m_pCombinedHardwareHeader->numLODs = pPrimaryHeader->numLODs;

	// figure out the worst case scenario - we'll have some blank spots in the file, but should be minimal
	memset( &m_MaxHardwareData, 0, sizeof( m_MaxHardwareData ) );
	memset( &m_CurrentHardwareData, 0, sizeof( m_CurrentHardwareData ) );
	memset( &m_HardwareOffsets, 0, sizeof( m_HardwareOffsets ) );


	CalcVTXInfo();

	m_pCombinedHardwareHeader->numBodyParts = m_MaxHardwareData.m_nMaxBodyParts;

	m_MaxHardwareData.m_nStripGroups++;	// we need to reserve space for temp writing

	m_HardwareOffsets.m_nBodyParts = sizeof( OptimizedModel::FileHeader_t );
	m_HardwareOffsets.m_nModels = m_HardwareOffsets.m_nBodyParts + sizeof( OptimizedModel::BodyPartHeader_t ) * m_MaxHardwareData.m_nBodyParts;
	m_HardwareOffsets.m_nModelLODs = m_HardwareOffsets.m_nModels + sizeof( OptimizedModel::ModelHeader_t ) * m_MaxHardwareData.m_nModels;
	m_HardwareOffsets.m_nMeshes = m_HardwareOffsets.m_nModelLODs + sizeof( OptimizedModel::ModelLODHeader_t ) * m_MaxHardwareData.m_nModelLODs;
	m_HardwareOffsets.m_nStripGroups = m_HardwareOffsets.m_nMeshes + sizeof( OptimizedModel::MeshHeader_t ) * m_MaxHardwareData.m_nMeshes;
	m_HardwareOffsets.m_nStrips = m_HardwareOffsets.m_nStripGroups + sizeof( OptimizedModel::StripGroupHeader_t ) * m_MaxHardwareData.m_nStripGroups;
	m_HardwareOffsets.m_nVerts = m_HardwareOffsets.m_nStrips + sizeof( OptimizedModel::StripHeader_t ) * m_MaxHardwareData.m_nStrips;
	m_HardwareOffsets.m_nIndices = m_HardwareOffsets.m_nVerts + sizeof( OptimizedModel::Vertex_t ) * m_MaxHardwareData.m_nVerts;
	m_HardwareOffsets.m_nBoneStateChanges = m_HardwareOffsets.m_nIndices + sizeof( unsigned short ) * m_MaxHardwareData.m_nIndices;
	m_HardwareOffsets.m_nStringTable = m_HardwareOffsets.m_nBoneStateChanges + sizeof( OptimizedModel::BoneStateChangeHeader_t ) * m_MaxHardwareData.m_nBoneStateChanges;
	m_HardwareOffsets.m_nMaterialReplacements = m_HardwareOffsets.m_nStringTable + 0;

	g_CombinerWriter.AllocWrite( m_HardwareOffsets.m_nMaterialReplacements - m_HardwareOffsets.m_nModels );

	m_pCombinedHardwareHeader->bodyPartOffset = m_HardwareOffsets.m_nBodyParts;

	for( int nBodyPart = 0; nBodyPart < m_MaxHardwareData.m_nMaxBodyParts; nBodyPart++ )
	{
		WriteBodyPart( nBodyPart );
	}

	for( int nLOD = 1; nLOD < m_pCombinedVertex->numLODs; nLOD++ )
	{
		for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
		{
			if ( m_pCombinedStudioData->m_Results.m_nNumIndexes[ nModel ][ nLOD ] > m_pCombinedStudioData->m_Results.m_nNumIndexes[ nModel ][ nLOD - 1 ] )
			{
				Assert( 0 );
				V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "%s has lower LOD which has higher triangle count\n", m_pStudioHdr[ nModel ]->pszName() );
				m_pCombinedStudioData->m_Results.m_nDetailedError = COMBINED_DETAIL_ERROR_MODEL_LOWER_LOD_HIGHER_TRI_COUNT;
				V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorDetails, "Model %s\nLOD %d Tris %d\nLOD %d Tris %d\n", m_pStudioHdr[ nModel ]->pszName(), nLOD - 1, m_pCombinedStudioData->m_Results.m_nNumIndexes[ nModel ][ nLOD - 1 ] / 3, 
					nLOD, m_pCombinedStudioData->m_Results.m_nNumIndexes[ nModel ][ nLOD ] / 3 );

				throw( COMBINE_RESULT_FLAG_FAILED_GOOD_PRACTICE );
			}
		}
	}

	m_pCombinedHardwareHeader->materialReplacementListOffset = m_HardwareOffsets.m_nMaterialReplacements;
	g_CombinerWriter.AllocWrite( pPrimaryHeader->numLODs * sizeof( OptimizedModel::MaterialReplacementListHeader_t ) );
	for( int nLOD = 0; nLOD < pPrimaryHeader->numLODs; nLOD++ )
	{
		OptimizedModel::MaterialReplacementListHeader_t	*pNewMaterialReplacementList = m_pCombinedHardwareHeader->pMaterialReplacementList( nLOD );
		OptimizedModel::MaterialReplacementListHeader_t	*pOrigMaterialReplacementList = pPrimaryHeader->pMaterialReplacementList( nLOD );

		pNewMaterialReplacementList->numReplacements = pOrigMaterialReplacementList->numReplacements;
		g_CombinerWriter.WriteOffset( pNewMaterialReplacementList->replacementOffset, pNewMaterialReplacementList );

		if ( pOrigMaterialReplacementList->numReplacements > 0 )
		{
			Assert( 0 );
			V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "material has replacements" );
			throw( COMBINE_RESULT_FLAG_UNSUPPORTED_FEATURE );
		}
	}

#ifdef TEST_VTX_COMBINE

	TestCombineVTX();

#endif // TEST_VTX_COMBINE

//	int nSize = Min( 48916, VTX_Data[ 0 ]->TellMaxPut() );
//	memcpy( m_pCombinedHardwareHeader, VTX_Data[ 0 ]->PeekGet(), nSize );
//	memcpy( ( char * )m_pCombinedHardwareHeader + 48916, ( char * )VTX_Data[ 0 ]->PeekGet() + 48916, 688 );
//	g_CombinerWriter.AlignWrite( 128 );
//	m_pCombinedHardwareHeader = ( OptimizedModel::FileHeader_t * )g_CombinerWriter.WriteBuffer( VTX_Data[ 0 ]->PeekGet(), VTX_Data[ 0 ]->TellMaxPut() );
}


#ifdef DEBUG_COMBINE

void CModelCombine::TestCombineVTX( )
{
	OptimizedModel::FileHeader_t	*pPrimaryHeader = ( OptimizedModel::FileHeader_t * )VTX_Data[ 0 ]->PeekGet();

	Assert( pPrimaryHeader->version == m_pCombinedHardwareHeader->version );
	Assert( pPrimaryHeader->vertCacheSize == m_pCombinedHardwareHeader->vertCacheSize );
	Assert( pPrimaryHeader->maxBonesPerStrip == m_pCombinedHardwareHeader->maxBonesPerStrip );
	Assert( pPrimaryHeader->maxBonesPerFace == m_pCombinedHardwareHeader->maxBonesPerFace );
	Assert( pPrimaryHeader->maxBonesPerVert == m_pCombinedHardwareHeader->maxBonesPerVert );
	Assert( pPrimaryHeader->numLODs == m_pCombinedHardwareHeader->numLODs );
	Assert( pPrimaryHeader->numBodyParts == m_pCombinedHardwareHeader->numBodyParts );

	for( int nBodyPart = 0; nBodyPart < pPrimaryHeader->numBodyParts; nBodyPart++ )
	{
		OptimizedModel::BodyPartHeader_t	*pOrigBodyPart = pPrimaryHeader->pBodyPart( nBodyPart );
		OptimizedModel::BodyPartHeader_t	*pNewBodyPart = m_pCombinedHardwareHeader->pBodyPart( nBodyPart );

		Assert( pOrigBodyPart->numModels == pNewBodyPart->numModels );

		for( int nModel = 0; nModel < pOrigBodyPart->numModels; nModel++ )
		{
			OptimizedModel::ModelHeader_t	*pOrigModel = pOrigBodyPart->pModel( nModel );
			OptimizedModel::ModelHeader_t	*pNewModel = pNewBodyPart->pModel( nModel );

			Assert( pOrigModel->numLODs == pNewModel->numLODs );

			for( int nLOD = 0; nLOD < pOrigModel->numLODs; nLOD++ )
			{
				OptimizedModel::ModelLODHeader_t	*pOrigLOD = pOrigModel->pLOD( nLOD );
				OptimizedModel::ModelLODHeader_t	*pNewLOD = pNewModel->pLOD( nLOD );

				Assert( pOrigLOD->numMeshes == pNewLOD->numMeshes );
				Assert( pOrigLOD->switchPoint == pNewLOD->switchPoint );

				for( int nMesh = 0; nMesh < pOrigLOD->numMeshes; nMesh++ )
				{
					OptimizedModel::MeshHeader_t	*pOrigMesh = pOrigLOD->pMesh( nMesh );
					OptimizedModel::MeshHeader_t	*pNewMesh = pNewLOD->pMesh( nMesh );

					Assert( pOrigMesh->numStripGroups == pNewMesh->numStripGroups );
					Assert( pOrigMesh->flags == pNewMesh->flags );

					for( int nStripGroup = 0; nStripGroup < pOrigMesh->numStripGroups; nStripGroup++ )
					{
						OptimizedModel::StripGroupHeader_t	*pOrigStripGroup = pOrigMesh->pStripGroup( nStripGroup );
						OptimizedModel::StripGroupHeader_t	*pNewStripGroup = pNewMesh->pStripGroup( nStripGroup );

						Assert( pOrigStripGroup->numVerts == pNewStripGroup->numVerts );
						Assert( pOrigStripGroup->numIndices == pNewStripGroup->numIndices );
						Assert( pOrigStripGroup->numStrips == pNewStripGroup->numStrips );
						Assert( pOrigStripGroup->flags == pNewStripGroup->flags );
						Assert( pOrigStripGroup->numTopologyIndices == pNewStripGroup->numTopologyIndices );

						for( int nNumVerts = 0; nNumVerts < pOrigStripGroup->numVerts; nNumVerts++ )
						{
							OptimizedModel::Vertex_t	*pOrigVertex = pOrigStripGroup->pVertex( nNumVerts );
							OptimizedModel::Vertex_t	*pNewVertex = pNewStripGroup->pVertex( nNumVerts );

							Assert( memcmp( pOrigVertex, pNewVertex, sizeof( *pOrigVertex ) ) == 0 );
						}

						for( int nStrip = 0; nStrip < pOrigStripGroup->numStrips; nStrip++ )
						{
							OptimizedModel::StripHeader_t	*pOrigStrip = pOrigStripGroup->pStrip( nStrip );
							OptimizedModel::StripHeader_t	*pNewStrip = pNewStripGroup->pStrip( nStrip );

							Assert( pOrigStrip->numIndices == pNewStrip->numIndices );
							Assert( pOrigStrip->numVerts == pNewStrip->numVerts );
							Assert( pOrigStrip->numBones == pNewStrip->numBones );
							Assert( pOrigStrip->flags == pNewStrip->flags );
							Assert( pOrigStrip->numBoneStateChanges == pNewStrip->numBoneStateChanges );
							Assert( pOrigStrip->numTopologyIndices == pNewStrip->numTopologyIndices );


							for( int nBoneStateChange = 0; nBoneStateChange < pOrigStrip->numBoneStateChanges; nBoneStateChange++ )
							{
								OptimizedModel::BoneStateChangeHeader_t	*pOrigBoneStateChange = pOrigStrip->pBoneStateChange( nBoneStateChange );
								OptimizedModel::BoneStateChangeHeader_t	*pNewBoneStateChange = pNewStrip->pBoneStateChange( nBoneStateChange );

								Assert( pOrigBoneStateChange->hardwareID == pNewBoneStateChange->hardwareID );
								Assert( pOrigBoneStateChange->newBoneID == pNewBoneStateChange->newBoneID );
							}
						}
					}
				}
			}
		}
	}
}

#endif // DEBUG_COMBINE


// this will move a vert belonging to a secondary model from pose space of that secondary model
// into that secondary model's bone space, then transform the vert into pose space of the primary model.
void CModelCombine::CombineVVD_OffsetVerts( )
{
	matrix3x4_t	VertToPrimaryPose[ COMBINER_MAX_BONES ];

	for( int nBone = 0; nBone < m_nNumMasterBones; nBone++ )
	{
		matrix3x4_t	VertToPrimaryBone;
		VertToPrimaryBone = m_pMasterBoneList[ nBone ]->poseToBone;

		MatrixInvert( VertToPrimaryBone, VertToPrimaryPose[ nBone ] );
	}

	for( int nModel = 1; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
	{
		int	nNumVerts = m_pVertexFileHeader[ nModel ]->numLODVertexes[ 0 ];

		for( int nVert = 0; nVert < nNumVerts; nVert++ )
		{
			int				nRealVert = m_nVertexRemap[ nModel ][ nVert ];
			mstudiovertex_t	*pVert = ( mstudiovertex_t * )m_pCombinedVertex->GetVertexData() + nRealVert;
			Vector			vPosition, vPos, vFinalVert, vFinalNormal, vFinalTangent;
			bool			bHasTangent = m_pVertexFileHeader[ nModel ]->tangentDataStart != NULL;

			vFinalVert.Init();
			vFinalNormal.Init();
			vFinalTangent.Init();
			for( int nBone = 0; nBone < pVert->m_BoneWeights.numbones; nBone++ )
			{
				int nBoneIndex = pVert->m_BoneWeights.bone[ nBone ];

				VectorTransform( pVert->m_vecPosition, m_pStudioHdr[ nModel ]->pBone( m_nMasterToLocalBoneRemap[ nModel ][ nBoneIndex ] )->poseToBone, vPosition );
				VectorTransform( vPosition, VertToPrimaryPose[ nBoneIndex ], vPos );
				vFinalVert += vPos * pVert->m_BoneWeights.weight[ nBone ];

				VectorRotate( pVert->m_vecNormal, m_pStudioHdr[ nModel ]->pBone( m_nMasterToLocalBoneRemap[ nModel ][ nBoneIndex ] )->poseToBone, vPosition );
				VectorRotate( vPosition, VertToPrimaryPose[ nBoneIndex ], vPos );
				vFinalNormal += vPos * pVert->m_BoneWeights.weight[ nBone ];

				if ( bHasTangent )
				{
					Vector4D vTangent = *( m_pCombinedVertex->GetTangentData() + nRealVert );
					VectorRotate( vTangent.AsVector3D(), m_pStudioHdr[ nModel ]->pBone( m_nMasterToLocalBoneRemap[ nModel ][ nBoneIndex ] )->poseToBone, vPosition );
					VectorRotate( vPosition, VertToPrimaryPose[ nBoneIndex ], vPos );
					vFinalTangent += vPos * pVert->m_BoneWeights.weight[ nBone ];
				}
			}
			pVert->m_vecPosition = vFinalVert;
			pVert->m_vecNormal = vFinalNormal;
			if ( bHasTangent )
			{
				Vector *pTangent = ( Vector * )( m_pCombinedVertex->GetTangentData() + nRealVert );
				*pTangent = vFinalTangent;
			}
		}
	}
}


void CModelCombine::CombineVVD( )
{
	int nVertsWritten[ COMBINER_MAX_MODELS ];
	for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
	{
		int	nNumVerts = m_pVertexFileHeader[ nModel ]->numLODVertexes[ 0 ];

		m_nVertexRemap[ nModel ] = ( int * )g_CombinerWriter.AllocWrite( sizeof( int ) * nNumVerts );
		nVertsWritten[ nModel ] = 0;
	}

	g_CombinerWriter.AlignWrite( 16 );

	g_CombinerWriter.InitWriteArea( WRITE_AREA_VVD, g_CombinerWriter.GetWritePos() );
	g_CombinerWriter.SetWriteArea( WRITE_AREA_VVD );

	m_pCombinedVertex = ( vertexFileHeader_t * )g_CombinerWriter.AllocWrite( sizeof( *m_pCombinedVertex ) );

	m_pCombinedVertex->id = MODEL_VERTEX_FILE_ID;
	m_pCombinedVertex->version = MODEL_VERTEX_FILE_VERSION;
	m_pCombinedVertex->numLODs = m_pVertexFileHeader[ 0 ]->numLODs;

	g_CombinerWriter.WriteOffset( m_pCombinedVertex->vertexDataStart, m_pCombinedVertex );
	int nVertOffset = 0;
	for( int nLOD = m_pCombinedVertex->numLODs - 1; nLOD >= 0; nLOD-- )
	{
		for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
		{
			int nNumVerts = m_pVertexFileHeader[ nModel ]->numLODVertexes[ nLOD ];

			m_pCombinedVertex->numLODVertexes[ nLOD ] += m_pVertexFileHeader[ nModel ]->numLODVertexes[ nLOD ];

			nNumVerts -= nVertsWritten[ nModel ];

			mstudiovertex_t	*pNewVertex = ( mstudiovertex_t * )g_CombinerWriter.GetWritePos();
			if ( m_pVertexFileHeader[ nModel ]->numFixups > 0 )
			{
				int						nCount = nNumVerts;
				int						nOffset = nVertsWritten[ nModel ];
				int						nNumWritten = 0;
				const mstudiovertex_t	*pOrigVertex = m_pVertexFileHeader[ nModel ]->GetVertexData();

				for( int nFixup = 0; nFixup < m_pVertexFileHeader[ nModel ]->numFixups; nFixup++ )
				{
					vertexFileFixup_t	*pOrigFixup = ( vertexFileFixup_t * )( ( byte * )m_pVertexFileHeader[ nModel ] + m_pVertexFileHeader[ nModel ]->fixupTableStart) + nFixup;

					if ( pOrigFixup->numVertexes < nOffset )
					{
						nOffset -= pOrigFixup->numVertexes;
					}
					else
					{
						int nSize = pOrigFixup->numVertexes - nOffset;
						if ( nSize > nCount )
						{
							nSize = nCount;
						}

						g_CombinerWriter.WriteBuffer( pOrigVertex + nOffset, nSize * sizeof( mstudiovertex_t ) );

						nOffset = 0;
						nCount -= nSize;
						nNumWritten += nSize;

						if ( nCount == 0 )
						{
							break;
						}
					}
				}

				Assert( nNumWritten == nNumVerts );
			}
			else
			{
				g_CombinerWriter.WriteBuffer( m_pVertexFileHeader[ nModel ]->GetVertexData() + nVertsWritten[ nModel ], nNumVerts * sizeof( mstudiovertex_t ) );
			}

			if ( nModel > 0 )
			{	// primary one doesn't need bone remap
				mstudiovertex_t	*pBoneVertex = pNewVertex;

				for( int nVert = 0, nVertIndex = nVertsWritten[ nModel ]; nVert < nNumVerts; nVert++, pBoneVertex++, nVertIndex++ )
				{
					for( int nBone = 0; nBone < pBoneVertex->m_BoneWeights.numbones; nBone++ )
					{
						pBoneVertex->m_BoneWeights.bone[ nBone ] = m_nBoneRemap[ nModel ][ pBoneVertex->m_BoneWeights.bone[ nBone ] ];
					}
				}
			}

			for( int nBodyBart = 0; nBodyBart < m_pStudioHdr[ nModel ]->numbodyparts; nBodyBart++ )
			{
				mstudiobodyparts_t	*pOrigBodyPart = m_pStudioHdr[ nModel ]->pBodypart( nBodyBart );

				int nSubModelToUse = m_pCombinedStudioData->m_ModelInputData[ nModel ].m_nBodyGroupSubModelSelection;

				for( int nBodyPartModel = 0; nBodyPartModel < pOrigBodyPart->nummodels; nBodyPartModel++ )
				{
					// skip over sub models that are not the selected one (-1 no selection, so no skipping)
					if ( nSubModelToUse != -1 && nBodyPartModel != nSubModelToUse )
					{
						continue;
					}

					mstudiomodel_t *pOrigModel = pOrigBodyPart->pModel( nBodyPartModel );
					
					for( int nMesh = 0; nMesh < pOrigModel->nummeshes; nMesh++ )
					{
						mstudiomesh_t *pOrigMesh = pOrigModel->pMesh( nMesh );

						Vector2D vStartST, vSizeST, vPixelSize;
						int nTextureIndex = m_pCombinedStudioData->m_nModelMaterialIndices[ nModel ][ nMesh ];

						GetTextureCombiner().GetTextureInfo( nTextureIndex, vStartST, vSizeST, vPixelSize );

						for( int nVert = 0, nVertIndex = nVertsWritten[ nModel ] + pOrigMesh->vertexoffset; nVert < pOrigMesh->numvertices; nVert++, pNewVertex++, nVertIndex++ )
						{
							double flInteger;

							pNewVertex->m_vecTexCoord.x = ( float )modf( ( double )pNewVertex->m_vecTexCoord.x, &flInteger );
							if ( pNewVertex->m_vecTexCoord.x < 0.0f )
							{
								pNewVertex->m_vecTexCoord.x += 1.0f;
							}

							pNewVertex->m_vecTexCoord.y = ( float )modf( ( double )pNewVertex->m_vecTexCoord.y, &flInteger );
							if ( pNewVertex->m_vecTexCoord.y < 0.0f )
							{
								pNewVertex->m_vecTexCoord.y += 1.0f;
							}

							pNewVertex->m_vecTexCoord = ( pNewVertex->m_vecTexCoord * vSizeST );
							pNewVertex->m_vecTexCoord += vStartST;

							m_nVertexRemap[ nModel ][ nVertIndex ] = nVertOffset + nVert; 
						}
					}
				}
			}

			nVertsWritten[ nModel ] += nNumVerts;
			nVertOffset += nNumVerts;

			m_pCombinedStudioData->m_Results.m_nNumLODs[ nModel ] = m_pVertexFileHeader[ nModel ]->numLODs;
			if ( nLOD < m_pCombinedVertex->numLODs - 1 )
			{
				m_pCombinedStudioData->m_Results.m_nNumVerts[ nModel ][ nLOD ] = m_pCombinedStudioData->m_Results.m_nNumVerts[ nModel ][ nLOD + 1 ] + nNumVerts;
			}
			else
			{	
				m_pCombinedStudioData->m_Results.m_nNumVerts[ nModel ][ nLOD ] = nNumVerts;
			}
			m_pCombinedStudioData->m_Results.m_nCombinedNumVerts[ nLOD ] += nNumVerts;
		}
	}

	m_pCombinedStudioData->m_Results.m_nCombinedNumLODs = m_pCombinedVertex->numLODs;

	int nLastLODSize = m_pCombinedVertex->numLODVertexes[ m_pCombinedVertex->numLODs - 1 ];
	for( int nLOD = m_pCombinedVertex->numLODs; nLOD < MAX_NUM_LODS; nLOD++ )
	{
		m_pCombinedVertex->numLODVertexes[ nLOD ] = nLastLODSize;
	}

	Vector4D	*pData = ( Vector4D * )g_CombinerWriter.WriteOffset( m_pCombinedVertex->tangentDataStart, m_pCombinedVertex );
	g_CombinerWriter.AllocWrite( m_pCombinedVertex->numLODVertexes[ 0 ] * sizeof( Vector4D ) );
	
	for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
	{
		if ( m_pVertexFileHeader[ nModel ]->tangentDataStart == 0 )
		{
			Assert( 0 );
			V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "vertex file has tangent data" );
			throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
		}

		const Vector4D *vData = m_pVertexFileHeader[ nModel ]->GetTangentData();
		for( int nIndex = 0; nIndex < m_pVertexFileHeader[ nModel ]->numLODVertexes[ 0 ]; nIndex++ )
		{
			pData[ m_nVertexRemap[ nModel ][ nIndex ] ] = *vData;
			vData++;
		}
	}

	CombineVVD_OffsetVerts();
}

#if 0
void CModelCombine::Test( )
{
	studiohdr_t			*pStudioHdr = m_pStudioHdr[ 0 ];
	vertexFileHeader_t	*pVertexHeader = ( vertexFileHeader_t * )VVD_Data[ 0 ]->PeekGet();
	const mstudiovertex_t		*pVertex = pVertexHeader->GetVertexData();

	int nTotalModels = 0;
	for ( int nBodyPart = 0; nBodyPart < pStudioHdr->numbodyparts; nBodyPart++ )
	{
		mstudiobodyparts_t	*pOrigBodyPart = pStudioHdr->pBodypart( nBodyPart );

		for( int nModel = 0; nModel < pOrigBodyPart->nummodels; nModel++ )
		{
			mstudiomodel_t	*pOrigModel = pOrigBodyPart->pModel( nModel );

			for( int nMesh = 0; nMesh < pOrigModel->nummeshes; nMesh++ )
			{
				mstudiomesh_t	*pOrigMesh = pOrigModel->pMesh( nMesh );

				for( int nVert = 0; nVert < pOrigMesh->numvertices; nVert++ )
				{
					const mstudiovertex_t	*pOrigVert = &pVertex[ nVert + pOrigMesh->vertexoffset ];

					Msg("");
				}
			}
		}
	}
}
#endif


void CModelCombine::CombineTextures( )
{
	GetTextureCombiner().Init( m_pCombinedStudioData );

	for( int nModel = 0; nModel < m_pCombinedStudioData->m_nNumModels; nModel++ )
	{
#if 0
		for( int nSkin = 0; nSkin < m_pStudioHdr[ nModel ]->numskinfamilies; nSkin++ )
		{
			short *pSkinRef = m_pStudioHdr[ nModel ]->pSkinref( 0 );
			pSkinRef += ( nSkin * m_pStudioHdr[ nModel ]->numskinref );

			for( int nReference = 0; nReference < m_pStudioHdr[ nModel ]->numskinref; nReference++, pSkinRef++ )
			{
				int			nTextureIndex = *pSkinRef;
				const char *pszTextureName = m_pStudioHdr[ nModel ]->pTexture( nTextureIndex )->pszName();

				Msg( "SKin %d, Reference %d: %s\n", nSkin, nReference, pszTextureName );
			}
		}
#endif

		int nSkin = m_pCombinedStudioData->m_ModelInputData[ nModel ].m_nSkinFamily;

		short *pSkinRef = m_pStudioHdr[ nModel ]->pSkinref( 0 );
		if ( nSkin > 0 && nSkin < m_pStudioHdr[ nModel ]->numskinfamilies )
		{
			pSkinRef += ( nSkin * m_pStudioHdr[ nModel ]->numskinref );
		}

		for( int nBodyPart = 0; nBodyPart < m_pStudioHdr[ nModel ]->numbodyparts; nBodyPart++ )
		{
			mstudiobodyparts_t	*pOrigBodyPart = m_pStudioHdr[ nModel ]->pBodypart( nBodyPart );

			int nSubModelToUse = m_pCombinedStudioData->m_ModelInputData[ nModel ].m_nBodyGroupSubModelSelection;

			for( int nBodyPartModel = 0; nBodyPartModel < pOrigBodyPart->nummodels; nBodyPartModel++ )
			{
				// skip over sub models that are not the selected one (-1 no selection, so no skipping)
				if ( nSubModelToUse != -1 && nBodyPartModel != nSubModelToUse )
				{
					continue;
				}

				mstudiomodel_t *pOrigModel = pOrigBodyPart->pModel( nBodyPartModel );
				m_pCombinedStudioData->m_nModelMaterialCounts[ nModel ] = pOrigModel->nummeshes;
				for( int nMesh = 0; nMesh < pOrigModel->nummeshes; nMesh++ )
				{
					mstudiomesh_t *pOrigMesh = pOrigModel->pMesh( nMesh );

					int nTextureIndex = pSkinRef[ pOrigMesh->material ];

					m_pCombinedStudioData->m_MeshToMaterialMap[ nModel ][ nBodyPart ][ nMesh ] = nTextureIndex;

					m_pCombinedStudioData->m_nModelMaterialIndices[ nModel ][ nMesh ] = AddMaterialToTextureCombiner( nTextureIndex, nModel, pOrigMesh->material );
				}
			}
		}
	}

	GetTextureCombiner().Resolve();
}

int CModelCombine::AddMaterialToTextureCombiner( int nTextureIndex, int nModel, int nModelMaterialIndex )
{
	char	szPath[ MAX_PATH ];
	char	szFinalPath[ MAX_PATH ];
	bool	bFound = false;

	// If we don't do this, we get filenames like "materials\\blah.vmt".
	const char *pszTextureName = m_pStudioHdr[ nModel ]->pTexture( nTextureIndex )->pszName();
	if ( pszTextureName[ 0 ] == CORRECT_PATH_SEPARATOR || pszTextureName[ 0 ] == INCORRECT_PATH_SEPARATOR )
	{
		pszTextureName++;
	}

	// search through all specified directories until a valid material is found
	for ( int nSearch = 0; nSearch < m_pStudioHdr[ nModel ]->numcdtextures; nSearch++ )
	{
		// This prevents filenames like /models/blah.vmt.
		const char *pszCDTexture = m_pStudioHdr[ nModel ]->pCdtexture( nSearch );
		if ( pszCDTexture[ 0 ] == CORRECT_PATH_SEPARATOR || pszCDTexture[ 0 ] == INCORRECT_PATH_SEPARATOR )
		{
			pszCDTexture++;
		}

		V_ComposeFileName( pszCDTexture, pszTextureName, szPath, sizeof( szPath ) );
		V_ComposeFileName( "materials/", szPath, szFinalPath, sizeof( szFinalPath ) );

		char szCheckPath[ MAX_PATH ];
		V_strcpy_safe( szCheckPath, szFinalPath );
		V_strcat_safe( szCheckPath, ".vmt" );

		if ( g_pFullFileSystem->FileExists( szCheckPath ) )
		{
			bFound = true;
			break;
		}
	}

	if ( bFound == false )
	{
		Assert( 0 );
		V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "model %s has missing material %s", m_pStudioHdr[ nModel ]->pszName(), pszTextureName );
		throw( COMBINE_RESULT_FLAG_MISSING_ASSET_FILE );
	}

	return GetTextureCombiner().AddMaterial( szFinalPath );
}
