#include "datacache/imdlcache.h"
#include "vtfcombine.h"
#include "strtools.h"
#include "keyvalues.h"
#include "filesystem.h"
#include "vtf/vtf.h"
#include "tier0/cache_hints.h"
#include "materialsystem/imaterialsystem.h"


//#define DEBUG_VMT_COMBINE 1

#ifdef DEBUG_VMT_COMBINE

#define DebugCombineMsg( ... ) Msg( __VA_ARGS__ )

#else

#define DebugCombineMsg( ... )

#endif


CTextureCombine& GetTextureCombiner()
{
	// Allocate on demand to avoid consuming 50 MB of memory and address space
	// everywhere when this object isn't even used.
	// This is a memory leak but that is still better than having the memory
	// allocated regardless of whether it is used.
	static CTextureCombine* s_TextureCombiner = new CTextureCombine;
	return *s_TextureCombiner;
}


class CSimpleTexturePacker
{
public:
			CSimpleTexturePacker( );
	void	Init( int nWidth, int nHeight );
	void	AddTexture( int nID, int nWidth, int nHeight );
	void	Resolve( );
	void	GetTextureLocation( int nID, int &x, int &y );
	void	GetTextureSize( int nID, int &x, int &y );

private:
	static const int	m_nMaxSubdivisions = 32;
	static const int	m_nMaxTextures = 32;

	typedef struct SSubDivision
	{
		int x, y, width, height;
	} TSubDivisions;

	TSubDivisions		m_SubDivisions[ m_nMaxSubdivisions ];
	int					m_nNumSubDivisions;

	typedef struct STextureInfo
	{
		int				m_nID;
		TSubDivisions	m_Location;
	} TTextureInfo;

	TTextureInfo		m_Textures[ m_nMaxTextures ];
	TTextureInfo		*m_TextureOrder[ m_nMaxTextures ];
	int					m_nNumTextures;
	int					m_nWidth;
	int					m_nHeight;

	static int		TextureSizeCompare( const void *elem1, const void *elem2 );
	void			Reset( );
	bool			FindOpenSpace( TTextureInfo *pTexture );
	bool			ResolveBrute( );
	bool			IterateTextures( );
	bool			BruteIterate( );
};


CSimpleTexturePacker::CSimpleTexturePacker( )
{
	m_nNumSubDivisions = 0;
	m_nNumTextures = 0;
}


void CSimpleTexturePacker::Init( int nWidth, int nHeight )
{
	m_nWidth = nWidth;
	m_nHeight = nHeight;
	m_nNumTextures = 0;
}


void CSimpleTexturePacker::Reset( )
{
	m_SubDivisions[ 0 ].x = 0;
	m_SubDivisions[ 0 ].y = 0;
	m_SubDivisions[ 0 ].width = m_nWidth;
	m_SubDivisions[ 0 ].height = m_nHeight;
	m_nNumSubDivisions = 1;
}


void CSimpleTexturePacker::AddTexture( int nID, int nWidth, int nHeight )
{
	if ( m_nNumTextures >= m_nMaxTextures )
	{
		Assert( 0 );
		V_sprintf_safe( GetTextureCombiner().m_pCombinedStudioData->m_Results.m_szErrorMessage, "too many textures added to packer" );
		throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
	}

	m_Textures[ m_nNumTextures ].m_nID = nID;
	m_Textures[ m_nNumTextures ].m_Location.width = nWidth;
	m_Textures[ m_nNumTextures ].m_Location.height = nHeight;
	m_TextureOrder[ m_nNumTextures ] = &m_Textures[ m_nNumTextures ];

	m_nNumTextures++;
}


int CSimpleTexturePacker::TextureSizeCompare( const void *elem1, const void *elem2 )
{
	TTextureInfo *pTexture1 = ( TTextureInfo * )elem1;
	TTextureInfo *pTexture2 = ( TTextureInfo * )elem2;

	if ( pTexture1->m_Location.height > pTexture2->m_Location.height )
	{
		return -1;
	}
	else if ( pTexture1->m_Location.height < pTexture2->m_Location.height )
	{
		return 1;
	}
	else if ( pTexture1->m_Location.width > pTexture2->m_Location.width )
	{
		return -1;
	}
	else if ( pTexture1->m_Location.width < pTexture2->m_Location.width )
	{
		return 1;
	}

#if 0
	int index1 = *(byte *)elem1;
	int index2 = *(byte *)elem2;

	if ( SimpleTexturePacker.m_Textures[ index1 ].m_Location.height > SimpleTexturePacker.m_Textures[ index2 ].m_Location.height )
	{
		return -1;
	}
	else if ( SimpleTexturePacker.m_Textures[ index1 ].m_Location.height < SimpleTexturePacker.m_Textures[ index2 ].m_Location.height )
	{
		return 1;
	}
	else if ( SimpleTexturePacker.m_Textures[ index1 ].m_Location.width > SimpleTexturePacker.m_Textures[ index2 ].m_Location.width )
	{
		return -1;
	}
	else if ( SimpleTexturePacker.m_Textures[ index1 ].m_Location.width < SimpleTexturePacker.m_Textures[ index2 ].m_Location.width )
	{
		return 1;
	}
#endif

	return 0;
}


bool CSimpleTexturePacker::FindOpenSpace( TTextureInfo *pTexture )
{
	int			nSubDivision;
	const int	nWidth = pTexture->m_Location.width;
	const int	nHeight = pTexture->m_Location.height;

	for( nSubDivision = 0; nSubDivision < m_nNumSubDivisions; nSubDivision++ )
	{
		if ( m_SubDivisions[ nSubDivision ].width >= nWidth && m_SubDivisions[ nSubDivision ].height >= nHeight )
		{
			break;
		}
	}

	if ( nSubDivision >= m_nNumSubDivisions )
	{
		return false;
	}

	pTexture->m_Location.x = m_SubDivisions[ nSubDivision ].x;
	pTexture->m_Location.y = m_SubDivisions[ nSubDivision ].y;

	if ( m_SubDivisions[ nSubDivision ].width == nWidth && m_SubDivisions[ nSubDivision ].height == nHeight )
	{	// completely used up
		m_nNumSubDivisions--;
		if ( nSubDivision < m_nNumSubDivisions )
		{
			memmove( &m_SubDivisions[ nSubDivision ], &m_SubDivisions[ nSubDivision + 1 ], sizeof( m_SubDivisions[ nSubDivision ] ) * ( m_nNumSubDivisions - nSubDivision ) );
		}
	}
	else
	{
		if ( m_SubDivisions[ nSubDivision ].width == nWidth )
		{	// only one potential piece
			m_SubDivisions[ nSubDivision ].y += nHeight;
			m_SubDivisions[ nSubDivision ].height -= nHeight;
		}
		else if ( m_SubDivisions[ nSubDivision ].height == nHeight )
		{	// only one potential piece
			m_SubDivisions[ nSubDivision ].x += nWidth;
			m_SubDivisions[ nSubDivision ].width -= nWidth;
		}
		else
		{
			if ( m_nNumSubDivisions >= m_nMaxSubdivisions )
			{
				Assert( 0 );
				V_sprintf_safe( GetTextureCombiner().m_pCombinedStudioData->m_Results.m_szErrorMessage, "too many subdivision within texture packer" );
				throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
			}

			m_SubDivisions[ m_nNumSubDivisions ].x = m_SubDivisions[ nSubDivision ].x + nWidth;
			m_SubDivisions[ m_nNumSubDivisions ].y = m_SubDivisions[ nSubDivision ].y;
			m_SubDivisions[ m_nNumSubDivisions ].width = m_SubDivisions[ nSubDivision ].width - nWidth;
			m_SubDivisions[ m_nNumSubDivisions ].height = m_SubDivisions[ nSubDivision ].height;
			m_nNumSubDivisions++;

			m_SubDivisions[ nSubDivision ].y += nHeight;
			m_SubDivisions[ nSubDivision ].width = nWidth;
			m_SubDivisions[ nSubDivision ].height -= nHeight;
		}
	}

	return true;
}


void CSimpleTexturePacker::Resolve( )
{
	Reset();
	qsort( m_TextureOrder, m_nNumTextures, sizeof( m_TextureOrder[ 0 ] ), CSimpleTexturePacker::TextureSizeCompare );
	GetTextureCombiner().m_pCombinedStudioData->m_Results.m_nNumTexturePackIterations++;

	for( int nTexture = 0; nTexture < m_nNumTextures; nTexture++ )
	{
		if ( FindOpenSpace( m_TextureOrder[ nTexture ] ) == false )
		{
			if ( ResolveBrute() )
			{
#if 0
				for( int nTexture = 0; nTexture < m_nNumTextures; nTexture++ )
				{
					Msg( "ID %d: x=%d y=%d width=%d height=%d\n", m_TextureOrder[ nTexture ]->m_nID, m_TextureOrder[ nTexture ]->m_Location.x, m_TextureOrder[ nTexture ]->m_Location.y, 
						 m_TextureOrder[ nTexture ]->m_Location.width, m_TextureOrder[ nTexture ]->m_Location.height );
				}
#endif
				return;
			}

			// put them back in order
			qsort( m_TextureOrder, m_nNumTextures, sizeof( m_TextureOrder[ 0 ] ), CSimpleTexturePacker::TextureSizeCompare );

			AssertMsg( false, "Could not find open space for texture in packer" );

			V_sprintf_safe( GetTextureCombiner().m_pCombinedStudioData->m_Results.m_szErrorMessage, "could not find open space for texture in packer" );
			GetTextureCombiner().m_pCombinedStudioData->m_Results.m_nDetailedError = COMBINED_DETAIL_ERROR_TEXTURE_PACKER_NO_SPACE;

			char szTemp[ 256 ];

			for( int nTextureList = 0; nTextureList < m_nNumTextures; nTextureList++ )
			{
				V_sprintf_safe( szTemp, "ID %d%c: x=%d y=%d width=%d height=%d\n", m_TextureOrder[ nTextureList ]->m_nID, ( nTextureList == nTexture ? '*' : ' ' ), 
								m_TextureOrder[ nTextureList ]->m_Location.x, m_TextureOrder[ nTextureList ]->m_Location.y, m_TextureOrder[ nTextureList ]->m_Location.width, m_TextureOrder[ nTextureList ]->m_Location.height );
				V_strcat_safe( GetTextureCombiner().m_pCombinedStudioData->m_Results.m_szErrorDetails, szTemp );
			}

			throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
		}

		DebugCombineMsg( "ID %d: x=%d y=%d width=%d height=%d\n", m_TextureOrder[ nTexture ]->m_nID, m_TextureOrder[ nTexture ]->m_Location.x, m_TextureOrder[ nTexture ]->m_Location.y, 
						 m_TextureOrder[ nTexture ]->m_Location.width, m_TextureOrder[ nTexture ]->m_Location.height );
	}

	DebugCombineMsg( "Remaining Space:\n" );
	for( int nSubDivision = 0; nSubDivision < m_nNumSubDivisions; nSubDivision++ )
	{
		DebugCombineMsg( "   %d: x=%d y=%d width=%d height=%d\n", nSubDivision, m_SubDivisions[ nSubDivision ].x, m_SubDivisions[ nSubDivision ].y, m_SubDivisions[ nSubDivision ].width, m_SubDivisions[ nSubDivision ].height );
	}
}


bool CSimpleTexturePacker::BruteIterate( )
{
	Reset();

	GetTextureCombiner().m_pCombinedStudioData->m_Results.m_nNumTexturePackIterations++;

	DebugCombineMsg( "Trying: " );
	for( int nTexture = 0; nTexture < m_nNumTextures; nTexture++ )
	{
		DebugCombineMsg( "%d ", nTexture );

		if ( FindOpenSpace( m_TextureOrder[ nTexture ] ) == false )
		{
			DebugCombineMsg( "Failed\n" );
			return false;
		}
	}

	DebugCombineMsg( "Succeeded\n" );
	return true;
}


bool CSimpleTexturePacker::IterateTextures( )
{
	int nSwapIndex;
	TTextureInfo *pSaveSwap;
	int nCounters[ m_nMaxTextures ];

	memset( nCounters, 0, m_nNumTextures * sizeof( int )  );
 
	// Boothroyd method
	if ( BruteIterate() )
	{
		return true;
	}

	for ( int nIndex = 0; ; nCounters[ nIndex ]++ ) 
	{
		while ( nIndex > 1 )
		{
			nCounters[ --nIndex ] = 0;
		}

		while ( nCounters[ nIndex ] >= nIndex )
		{
			if ( ++nIndex >= m_nNumTextures )
			{
				return false;
			}
		}
 
		nSwapIndex = ( nIndex & 1 ) ? nCounters[ nIndex ] : 0;
		pSaveSwap = m_TextureOrder[ nSwapIndex ];
		m_TextureOrder[ nSwapIndex ] = m_TextureOrder[ nIndex ];
		m_TextureOrder[ nIndex ] = pSaveSwap;

		if ( BruteIterate() )
		{
			return true;
		}
	}

	return false;
}


bool CSimpleTexturePacker::ResolveBrute( )
{
	for ( int i = 0; i < m_nMaxTextures; i++ ) 
	{
		m_TextureOrder[ i ] = &m_Textures[ i ];
	}

	return IterateTextures();
}



void CSimpleTexturePacker::GetTextureSize( int nID, int &x, int &y ) 
{ 
	x = m_Textures[ nID ].m_Location.width; 
	y = m_Textures[ nID ].m_Location.height; 
}


void CSimpleTexturePacker::GetTextureLocation( int nID, int &x, int &y ) 
{ 
	x = m_Textures[ nID ].m_Location.x; 
	y = m_Textures[ nID ].m_Location.y; 
}



typedef struct STextureEntry
{
	const char *m_pszTextureField;
	const char *m_pszFlatReplacement;
} TTextureEntry;


static TTextureEntry szCustomHeroTextures[] =
{
	{ "$basetexture", NULL },
	{ "$normalmap", "models\\development\\flatnormal" },
//	"$diffusewarp",
	{ "$maskmap1", "models\\development\\blankmasks1" },
	{ "$maskmap2", "models\\development\\blankmasks2" },

	{ NULL, NULL }
};

static TTextureEntry szVertexLitGenericTextures[] =
{
	{ "$basetexture", NULL },
	{ "$phongexponenttexture", NULL },

	{ NULL, NULL }
};

typedef struct SMaterialToTexture
{
	const char		*m_pszMaterialName;
	TTextureEntry	*m_pszTextureList;
} TMaterialToTexture;


static const TMaterialToTexture MaterialToTexture[] =
{
	{
		"customhero",
		szCustomHeroTextures
	},
	{
		"VertexLitGeneric",
		szVertexLitGenericTextures
	},
	{
		NULL,
		NULL
	}
};


static const char *pszFlatTextures[] =
{
	"models\\development\\flatnormal",
	"models\\development\\blankmasks1",
	"models\\development\\blankmasks2",

	NULL
};


CTextureCombine::CTextureCombine( )
{
	Init( NULL );
}
	

void CTextureCombine::Init( TCombinedStudioData *pCombinedStudioData )
{
	m_pCombinedStudioData = pCombinedStudioData;

	m_nNumMaterials = 0;

	memset( m_szMaterials, 0, sizeof( m_szMaterials ) );
	memset( m_nMaterialAtlasInfo, 0, sizeof( m_nMaterialAtlasInfo ) );
	m_nMaxAtlasGroup = 0;

	memset( m_pMaterialKVs, 0, sizeof( m_pMaterialKVs ) );

	for ( int nGroup = 0; nGroup < COMBINER_MAX_ATLAS_GROUPS; nGroup++ )
	{
		m_AtlasGroups[ nGroup ].m_nNumMaterials = 0;
		memset( m_AtlasGroups[ nGroup ].m_nMaterialIndices, 0xFF, sizeof( m_AtlasGroups[ nGroup ].m_nMaterialIndices ) ); // all set to -1
		
		memset( m_AtlasGroups[ nGroup ].m_pVTFData, 0, sizeof( m_AtlasGroups[ nGroup ].m_pVTFData ) );
		memset( m_AtlasGroups[ nGroup ].m_pVTFFileHeader, 0, sizeof( m_AtlasGroups[ nGroup ].m_pVTFFileHeader ) );
		memset( m_AtlasGroups[ nGroup ].m_pResources, 0, sizeof( m_AtlasGroups[ nGroup ].m_pResources ) );
		memset( m_AtlasGroups[ nGroup ].m_bIsFlat, 0, sizeof( m_AtlasGroups[ nGroup ].m_bIsFlat ) );

		m_AtlasGroups[ nGroup ].m_pCombinedMaterialKVs = NULL;

		memset( m_AtlasGroups[ nGroup ].m_CombinedTextureMemory, 0, sizeof( m_AtlasGroups[ nGroup ].m_CombinedTextureMemory ) );
		memset( m_AtlasGroups[ nGroup ].m_nCombinedTextureSize, 0, sizeof( m_AtlasGroups[ nGroup ].m_nCombinedTextureSize ) );
		memset( m_AtlasGroups[ nGroup ].m_CombinedHeaders, 0, sizeof( m_AtlasGroups[ nGroup ].m_CombinedHeaders ) );

		m_AtlasGroups[ nGroup ].m_pSimpleTexturePacker = NULL;
	}

	if ( pCombinedStudioData )
	{
		g_CombinerWriter.InitWriteArea( WRITE_AREA_VTF, g_CombinerWriter.GetWritePos() );
		g_CombinerWriter.SetWriteArea( WRITE_AREA_VTF );
	}
}


void CTextureCombine::Cleanup( )
{
	for( int nMaterial = 0; nMaterial < m_nNumMaterials; nMaterial++ )
	{
		if ( m_pMaterialKVs[ nMaterial ] )
		{
			m_pMaterialKVs[ nMaterial ]->deleteThis();
			m_pMaterialKVs[ nMaterial ] = NULL;
		}
	}

	for ( int nGroup = 0; nGroup <= m_nMaxAtlasGroup; nGroup++ )
	{
		for( int nMaterial = 0; nMaterial < m_nNumMaterials; nMaterial++ )
		{
			delete m_AtlasGroups[ nGroup ].m_pVTFData[ nMaterial ];
			m_AtlasGroups[ nGroup ].m_pVTFData[ nMaterial ] = NULL;
		}
	}
}


void CTextureCombine::FreeCombinedMaterials( )
{
	for ( int nGroup = 0; nGroup <= COMBINER_MAX_ATLAS_GROUPS; nGroup++ )
	{
		if ( m_AtlasGroups[ nGroup ].m_pCombinedMaterialKVs != NULL )
		{
			m_AtlasGroups[ nGroup ].m_pCombinedMaterialKVs->deleteThis();
			m_AtlasGroups[ nGroup ].m_pCombinedMaterialKVs = NULL;
		}

		delete m_AtlasGroups[ nGroup ].m_pSimpleTexturePacker;
		m_AtlasGroups[ nGroup ].m_pSimpleTexturePacker = NULL;
	}
}

int CTextureCombine::AddMaterial( const char *pszFileName )
{
	for( int nMaterial = 0; nMaterial < m_nNumMaterials; nMaterial++ )
	{
		if ( strcmpi( m_szMaterials[ nMaterial ], pszFileName ) == 0 )
		{
			return nMaterial;
		}
	}

	V_strcpy_safe( m_szMaterials[ m_nNumMaterials ], pszFileName );

	// intending to return m_nNumMaterials, and then increment it
	return m_nNumMaterials++;
}

void CTextureCombine::AddNonAtlasedMaterial( int nMaterial )
{
	m_pCombinedStudioData->m_pNonAtlasedMaterialKVs[ m_pCombinedStudioData->m_nNumNonAtlasedMaterialBaseNames ] = ( m_pMaterialKVs[ nMaterial ] != NULL ) ? m_pMaterialKVs[ nMaterial ]->MakeCopy() : NULL;
	V_FileBase(  m_szMaterials[ nMaterial ], m_pCombinedStudioData->m_szNonAtlasedMaterialBaseName[ m_pCombinedStudioData->m_nNumNonAtlasedMaterialBaseNames ], MAX_PATH );
	m_pCombinedStudioData->m_nNumNonAtlasedMaterialBaseNames++;

	// de-dupe paths
	char szPath[ MAX_PATH ];
	V_strcpy( szPath, m_szMaterials[ nMaterial ] );
	V_StripFilename( szPath );
	bool bFound = false;
	for (int i = 0; i < m_pCombinedStudioData->m_nNumNonAtlasedMaterialPaths; i++ )
	{
		if ( V_strcmp( m_pCombinedStudioData->m_szNonAtlasedMaterialPaths[ i ], szPath ) == 0 )
		{
			bFound = true;
			break;
		}
	}

	if ( !bFound )
	{
		V_strcpy( m_pCombinedStudioData->m_szNonAtlasedMaterialPaths[ m_pCombinedStudioData->m_nNumNonAtlasedMaterialPaths ], szPath );
		m_pCombinedStudioData->m_nNumNonAtlasedMaterialPaths++;
	}
}

void CTextureCombine::Resolve( )
{
	if ( m_nNumMaterials <= 0 )
	{
		Assert( 0 );
		V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "no materials specified for texture combiner" );
		throw( COMBINE_RESULT_FLAG_MISSING_ASSET_FILE );
	}

	for( int nMaterial = 0; nMaterial < m_nNumMaterials; nMaterial++ )
	{
		m_pMaterialKVs[ nMaterial ] = new KeyValues( "vmt" );
		if ( !materials->LoadKeyValuesFromVMTFile( *(m_pMaterialKVs[ nMaterial ]), m_szMaterials[ nMaterial ], true ) )
		{
			AddNonAtlasedMaterial( nMaterial );

			// mark as not in an atlas
			m_nMaterialAtlasInfo[ nMaterial ][ ATLAS_INFO_GROUP_INDEX ] = -1;
			m_nMaterialAtlasInfo[ nMaterial ][ ATLAS_INFO_MATERIAL_INDEX ] = m_pCombinedStudioData->m_nNumNonAtlasedMaterialBaseNames - 1;

			m_pMaterialKVs[ nMaterial ]->deleteThis();
			m_pMaterialKVs[ nMaterial ] = NULL;
		}
	}

	GatherAtlasInfo();

	// assign a copy of the KVs from the first material in each atlas group to be the combined material KVs
	for ( int nAtlasGroup = 0; nAtlasGroup <= m_nMaxAtlasGroup; nAtlasGroup++ )
	{
		int nMaterial = m_AtlasGroups[ nAtlasGroup ].m_nMaterialIndices[ 0 ];

		if ( m_AtlasGroups[ nAtlasGroup ].m_nNumMaterials > 1 )
		{
			m_AtlasGroups[ nAtlasGroup ].m_pCombinedMaterialKVs = m_pMaterialKVs[ nMaterial ]->MakeCopy();
		}
		else
		{
			AddNonAtlasedMaterial( nMaterial );
			// mark as not in an atlas
			m_nMaterialAtlasInfo[ nMaterial ][ ATLAS_INFO_GROUP_INDEX ] = -1;
			m_nMaterialAtlasInfo[ nMaterial ][ ATLAS_INFO_MATERIAL_INDEX ] = m_pCombinedStudioData->m_nNumNonAtlasedMaterialBaseNames - 1;
		}
	}

	m_pCombinedStudioData->m_nNumAtlasGroups = m_nMaxAtlasGroup + 1;

	FindMaterialToTexture();

	Cleanup();
}


void CTextureCombine::GatherAtlasInfo( )
{
	for( int nMaterial = 0; nMaterial < m_nNumMaterials; nMaterial++ )
	{	
		if ( m_pMaterialKVs[ nMaterial ] != NULL )
		{
			// get the atlas index for each material (default it to just use index 0, one atlas for all)
			int nAtlasGroup = m_pMaterialKVs[ nMaterial ]->GetInt( "$atlas_group", 0 );

			// track the max used atlas group
			if ( nAtlasGroup > m_nMaxAtlasGroup )
			{
				m_nMaxAtlasGroup = nAtlasGroup;
			}

			// link the "global" material index to an atlas group
			m_nMaterialAtlasInfo[ nMaterial ][ ATLAS_INFO_GROUP_INDEX ] = nAtlasGroup;

			// link the "global" material index to the atlas group material index
			m_nMaterialAtlasInfo[ nMaterial ][ ATLAS_INFO_MATERIAL_INDEX ] = m_AtlasGroups[ nAtlasGroup ].m_nNumMaterials;

			// link atlas group material index to the "global" material index
			m_AtlasGroups[ nAtlasGroup ].m_nMaterialIndices[ m_AtlasGroups[ nAtlasGroup ].m_nNumMaterials ] = nMaterial;
		
			m_AtlasGroups[ nAtlasGroup ].m_nNumMaterials++;
		}
	}
}


void CTextureCombine::FindMaterialToTexture( )
{
	for ( int nAtlasGroup = 0; nAtlasGroup <= m_nMaxAtlasGroup; nAtlasGroup++ )
	{
		// use the first material of the atlas group as the "master" for KV values and image format
		const char *pszShaderName = m_pMaterialKVs[ m_AtlasGroups[ nAtlasGroup ].m_nMaterialIndices[ 0 ] ]->GetName();

		for( m_nMaterialToTexture = 0; MaterialToTexture[ m_nMaterialToTexture ].m_pszMaterialName != NULL; m_nMaterialToTexture++ )
		{
			if ( strcmpi( pszShaderName, MaterialToTexture[ m_nMaterialToTexture ].m_pszMaterialName ) == 0 )
			{
				break;
			}
		}

		if ( MaterialToTexture[ m_nMaterialToTexture ].m_pszMaterialName == NULL )
		{
			Assert( 0 );
			V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "unsupported shader for texture combiner: %s", pszShaderName );
			throw( COMBINE_RESULT_FLAG_UNSUPPORTED_SHADER );
		}

		for( int nTexture = 0; MaterialToTexture[ m_nMaterialToTexture ].m_pszTextureList[ nTexture ].m_pszTextureField != NULL; nTexture++ )
		{
			const char *pszTextureField = MaterialToTexture[ m_nMaterialToTexture ].m_pszTextureList[ nTexture ].m_pszTextureField;
			const char *pszFlatReplacement = MaterialToTexture[ m_nMaterialToTexture ].m_pszTextureList[ nTexture ].m_pszFlatReplacement;
			char NewFieldValue[ 128 ];

			bool bUsed = CombineTexture( nAtlasGroup, nTexture, pszTextureField, pszFlatReplacement );

			if ( bUsed )
			{
				m_pCombinedStudioData->m_AtlasGroups[ nAtlasGroup ].m_pCombinedTextures[ nTexture ] = ( unsigned char * )malloc( m_AtlasGroups[ nAtlasGroup ].m_nCombinedTextureSize[ nTexture ] );
				memcpy( m_pCombinedStudioData->m_AtlasGroups[ nAtlasGroup ].m_pCombinedTextures[ nTexture ], m_AtlasGroups[ nAtlasGroup ].m_CombinedTextureMemory[ nTexture ], m_AtlasGroups[ nAtlasGroup ].m_nCombinedTextureSize[ nTexture ] );
				m_pCombinedStudioData->m_AtlasGroups[ nAtlasGroup ].m_nCombinedTextureSizes[ nTexture ] = m_AtlasGroups[ nAtlasGroup ].m_nCombinedTextureSize[ nTexture ];

				V_sprintf_safe( NewFieldValue, "!%s|%d|%d|%hu|%d!", m_pCombinedStudioData->m_szCombinedModelName, nAtlasGroup, nTexture, m_pCombinedStudioData->m_FinalHandle, CModelCombine::GetNextAssetID() );
				m_AtlasGroups[ nAtlasGroup ].m_pCombinedMaterialKVs->SetString( pszTextureField, NewFieldValue );

				m_pCombinedStudioData->m_AtlasGroups[ nAtlasGroup ].m_pCombinedMaterial = m_AtlasGroups[ nAtlasGroup ].m_pCombinedMaterialKVs;
			}
		}
	}
}


#define IGNORE_TEXTURE_FLAGS ( TEXTUREFLAGS_HINT_DXT5 | TEXTUREFLAGS_ONEBITALPHA | TEXTUREFLAGS_EIGHTBITALPHA | TEXTUREFLAGS_SKIP_INITIAL_DOWNLOAD )

bool CTextureCombine::LoadVTFs( int nAtlasGroup, const char *pszTextureField, const char *pszFlatReplacement, char szTextureNames[ COMBINER_MAX_MATERIALS ][ MAX_PATH ] )
{
	for( int nMaterial = 0; nMaterial < m_AtlasGroups[ nAtlasGroup ].m_nNumMaterials; nMaterial++ )
	{
		const char *pszTexture = m_pMaterialKVs[ m_AtlasGroups[ nAtlasGroup ].m_nMaterialIndices[ nMaterial ] ]->GetString( pszTextureField, NULL );

		if ( pszTexture == NULL )
		{
			if ( nMaterial == 0 )
			{	// if not present on the primary material, then skip this texture option
				return false;
			}

			if ( pszFlatReplacement != NULL )
			{
				pszTexture = pszFlatReplacement;
			}
			else
			{
				Assert( 0 );
				V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "could not located required texture in material %s", pszTexture );
				throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
			}
		}

		V_strcpy_safe( szTextureNames[ nMaterial ], pszTexture );

		char szFinalPath[ MAX_PATH ];
		
		V_ComposeFileName( "materials/", pszTexture, szFinalPath, sizeof( szFinalPath ) );
		V_DefaultExtension( szFinalPath, ".vtf", sizeof( szFinalPath ) );

		m_AtlasGroups[ nAtlasGroup ].m_pVTFData[ nMaterial ] = new CUtlBuffer();
		if ( g_pFullFileSystem->ReadFile( szFinalPath, "GAME", *m_AtlasGroups[ nAtlasGroup ].m_pVTFData[ nMaterial ], 0 ) == false )
		{
			Assert( 0 );
			V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "could not read texture %s", szFinalPath );
			throw( COMBINE_RESULT_FLAG_MISSING_ASSET_FILE );
		}

		VTFFileBaseHeader_t	*pVTFFileBaseHeader = ( VTFFileBaseHeader_t * )m_AtlasGroups[ nAtlasGroup ].m_pVTFData[ nMaterial ]->PeekGet();
		if ( pVTFFileBaseHeader->version[ 0 ] != VTF_MAJOR_VERSION || pVTFFileBaseHeader->version[ 1 ] != VTF_MINOR_VERSION )
		{
			Assert( 0 );
			V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "texture is invalid version %s", szFinalPath );
			throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
		}

		m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ] = ( VTFFileHeader_t * )pVTFFileBaseHeader;

		if ( m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->numFrames != 1 || m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->startFrame != 0 )
		{
			Assert( 0 );
			V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "texture has frame information %s", szFinalPath );
			throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
		}

		m_AtlasGroups[ nAtlasGroup ].m_pResources[ nMaterial ] = ( ResourceEntryInfo * )(m_AtlasGroups[ nAtlasGroup ]. m_pVTFFileHeader[ nMaterial ] + 1 );

		if ( m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->numMipLevels > MAX_COMBINED_MIP_LEVELS )
		{
			Assert( 0 );
			V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "texture %s has too many mip levels: %d > %d", szFinalPath, m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->numMipLevels, MAX_COMBINED_MIP_LEVELS );
			throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
		}
	}

	return true;
}

bool CTextureCombine::CombineTexture( int nAtlasGroup, int nTexture, const char *pszTextureField, const char *pszFlatReplacement )
{
	Assert( nAtlasGroup >= 0 && nAtlasGroup <= m_nMaxAtlasGroup );
	Assert( nTexture >= 0 && nTexture < COMBINER_MAX_TEXTURES_PER_MATERIAL );

	double	flStartLoadTime = Plat_FloatTime();
	char	szTextureNames[ COMBINER_MAX_MATERIALS ][ MAX_PATH ];

	V_strcpy_safe( szTextureNames[ 0 ], "Unknown Texture" );

	if ( !LoadVTFs( nAtlasGroup, pszTextureField, pszFlatReplacement, szTextureNames ) )
	{
		return false;
	}

	double flStartCombineTime = Plat_FloatTime();
	m_pCombinedStudioData->m_Results.m_flTextureLoadDuration += ( float )( flStartCombineTime - flStartLoadTime );

	byte	*pMipOffset[ COMBINER_MAX_MATERIALS ][ MAX_COMBINED_MIP_LEVELS ];
	int		nMipWidth[ COMBINER_MAX_MATERIALS ][ MAX_COMBINED_MIP_LEVELS ];
	int		nMipHeight[ COMBINER_MAX_MATERIALS ][ MAX_COMBINED_MIP_LEVELS ];
	int		nBlockSize;

	memset( pMipOffset, 0, sizeof( pMipOffset ) );

	switch ( m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ 0 ]->imageFormat )
	{
		case IMAGE_FORMAT_DXT1:
		case IMAGE_FORMAT_DXT1_RUNTIME:
		case IMAGE_FORMAT_LINEAR_DXT1:
		case IMAGE_FORMAT_ATI1N:
			nBlockSize = 8;
			break;

		case IMAGE_FORMAT_DXT3:
		case IMAGE_FORMAT_DXT5:
		case IMAGE_FORMAT_DXT5_RUNTIME:
		case IMAGE_FORMAT_LINEAR_DXT3:
		case IMAGE_FORMAT_LINEAR_DXT5:
		case IMAGE_FORMAT_ATI2N:
			nBlockSize = 16;
			break;

		default:
			Assert( 0 );
			V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "texture '%s' has unsupported format: %d", szTextureNames[ 0 ], m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ 0 ]->imageFormat );
			throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
			break;
	}

	for( int nMaterial = 0; nMaterial < m_AtlasGroups[ nAtlasGroup ].m_nNumMaterials; nMaterial++ )
	{
		//if ( m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->imageFormat != m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ 0 ]->imageFormat )
		//{
		//	Assert( 0 );
		//	V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "texture '%s' has different format ( number of channels, compression, etc. ) than base: %d != %d", szTextureNames[ nMaterial ], 
		//					m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->imageFormat, m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ 0 ]->imageFormat );
		//	throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
		//}

		if ( ( m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->flags & ~( IGNORE_TEXTURE_FLAGS ) ) != ( m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ 0 ]->flags & ~( IGNORE_TEXTURE_FLAGS ) ) )
		{
			Assert( 0 );
			V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "texture '%s' has different flags than base: %d != %d", szTextureNames[ nMaterial ],
							m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->flags, m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ 0 ]->flags );
			throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
		}

		ResourceEntryInfo	*pImageResource = NULL;
		for( unsigned int nResource = 0; nResource < m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->numResources; nResource++ )
		{
			if ( m_AtlasGroups[ nAtlasGroup ].m_pResources[ nMaterial ][ nResource ].eType == VTF_LEGACY_RSRC_IMAGE )
			{
				pImageResource = &m_AtlasGroups[ nAtlasGroup ].m_pResources[ nMaterial ][ nResource ];
				break;
			}
		}

		if ( pImageResource == NULL )
		{
			Assert( 0 );
			V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "could not locate image resource for texture '%s'", szTextureNames[ nMaterial ] );
			throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
		}

		byte *pPtr = ( ( byte * )m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ] ) + ( pImageResource->resData );
		DebugCombineMsg( "Material %d: Size = %d\n", nMaterial, m_pVTFData[ nMaterial ]->TellMaxPut() );

		char		szCheckFileName[ MAX_PATH ];
		V_FixupPathName( szCheckFileName, sizeof( szCheckFileName ), szTextureNames[ nMaterial ] );

		m_AtlasGroups[ nAtlasGroup ].m_bIsFlat[ nMaterial ] = false;
		for( int nTestIndex = 0; pszFlatTextures[ nTestIndex ] != NULL; nTestIndex++ )
		{
			if ( strcmpi( pszFlatTextures[ nTestIndex ], szCheckFileName ) == 0 )
			{
				m_AtlasGroups[ nAtlasGroup ].m_bIsFlat[ nMaterial ] = true;
				break;
			}
		}

		if ( nTexture != 0 && !m_AtlasGroups[ nAtlasGroup ].m_bIsFlat[ nMaterial ] )
		{
			int nWidth, nHeight;

			m_AtlasGroups[ nAtlasGroup ].m_pSimpleTexturePacker->GetTextureSize( nMaterial, nWidth, nHeight );

			if ( nWidth != m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->width || nHeight != m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->height )
			{
				DebugCombineMsg( "   '%s' has inconsistent texture size %d/%d: width %d->%d, height %d->%d\n", szTextureNames[ nMaterial ], nTexture, nMaterial, nWidth,
								 m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->width, nHeight, m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->height );
			}
		}

		for ( int nMip = m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->numMipLevels - 1; nMip >= 0; nMip-- )
		{
			int nWidth = m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->width >> nMip;
			int nHeight = m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->height >> nMip;

			if ( nWidth < 4 )
			{
				nWidth = 4;
			}
			if ( nHeight < 4 )
			{
				nHeight = 4;
			}

			nWidth >>= 2;
			nHeight >>= 2;

			int nNumBlocks = ( nWidth * nHeight );
			int nMipSize = nNumBlocks * nBlockSize;

			pMipOffset[ nMaterial ][ nMip ] = pPtr;
			nMipWidth[ nMaterial ][ nMip ] = nWidth;
			nMipHeight[ nMaterial ][ nMip ] = nHeight;

			DebugCombineMsg( "   Mip %d: Width=%d, Height=%d, Offset = %d\n", nMip, nWidth, nHeight, pPtr - ( byte * ) m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ] );
			pPtr += nMipSize;
		}
		DebugCombineMsg( "   END OF FILE = %d\n", pPtr - ( byte * ) m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ] );
	}

	int nWidthShift = 0;
	int nHeightShift = 0;

	if ( nTexture == 0 )
	{
		m_AtlasGroups[ nAtlasGroup ].m_pSimpleTexturePacker = new CSimpleTexturePacker();
		m_AtlasGroups[ nAtlasGroup ].m_pSimpleTexturePacker->Init( MAX_COMBINED_WIDTH, MAX_COMBINED_HEIGHT );
		for( int nMaterial = 0; nMaterial < m_AtlasGroups[ nAtlasGroup ].m_nNumMaterials; nMaterial++ )
		{
#ifdef DEBUG_VTF_COMBINE
			const char *pszTexture = m_pMaterialKVs[ nMaterial ]->GetString( pszTextureField, NULL );
#endif
			DebugCombineMsg( "Material: %d ( %s ) Width=%d, Height=%d\n", nMaterial, szTextureNames[ nMaterial ], m_pVTFFileHeader[ nMaterial ]->width, m_pVTFFileHeader[ nMaterial ]->height );
			m_AtlasGroups[ nAtlasGroup ].m_pSimpleTexturePacker->AddTexture( nMaterial, m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->width, m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->height );
		}
		m_AtlasGroups[ nAtlasGroup ].m_pSimpleTexturePacker->Resolve();
	}
	else
	{
		// need to validate that the all the "other" textures in the material are the same ratio of the base
		// they don't have to be the same size as the base, just all 1/2 the base size or 1/4, or twice, or the same

		bool bGotShifts = false;

		for( int nMaterial = 0; nMaterial < m_AtlasGroups[ nAtlasGroup ].m_nNumMaterials; nMaterial++ )
		{
			if ( m_AtlasGroups[ nAtlasGroup ].m_bIsFlat[ nMaterial ] )
			{
				continue;
			}

			int nWriteWidth, nWriteHeight;

			m_AtlasGroups[ nAtlasGroup ].m_pSimpleTexturePacker->GetTextureSize( nMaterial, nWriteWidth, nWriteHeight );
			nWriteWidth >>= 2;	// 2 accounts for the DDS block encoding
			nWriteHeight >>= 2;	// 2 accounts for the DDS block encoding

			if ( !bGotShifts )
			{
				int nWidth = nWriteWidth;
				int nHeight = nWriteHeight;

				while ( nMipWidth[ nMaterial ][ 0 ] < nWidth )
				{
					nWidthShift++;
					nWidth >>= 1;
				}

				while ( nMipWidth[ nMaterial ][ 0 ] > nWidth )
				{
					nWidthShift--;
					nWidth <<= 1;
				}

				while ( nMipHeight[ nMaterial ][ 0 ] < nHeight )
				{
					nHeightShift++;
					nHeight >>= 1;
				}

				while ( nMipHeight[ nMaterial ][ 0 ] > nHeight )
				{
					nHeightShift--;
					nHeight <<= 1;
				}

				bGotShifts = true;
			}

			nWriteWidth = (nWidthShift > 0) ? nWriteWidth >> nWidthShift : nWriteWidth << (-nWidthShift);
			nWriteHeight = (nHeightShift > 0) ? nWriteHeight >> nHeightShift : nWriteHeight << (-nHeightShift);

			if ( nMipWidth[ nMaterial ][ 0 ] != nWriteWidth || nMipHeight[ nMaterial ][ 0 ] != nWriteHeight )
			{
				Assert( 0 );
				V_sprintf_safe( m_pCombinedStudioData->m_Results.m_szErrorMessage, "texture '%s' size ( %d, %d ) differs expected size ( %d, %d )", 
					szTextureNames[ nMaterial ], nMipWidth[ nMaterial ][ 0 ] << 2, nMipHeight[ nMaterial ][ 0 ] << 2, nWriteWidth << 2, nWriteHeight << 2 );
				throw( COMBINE_RESULT_FLAG_UNHANDLED_ISSUE );
			}
		}
	}

	byte *pPtr = m_AtlasGroups[ nAtlasGroup ].m_CombinedTextureMemory[ nTexture ];

	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ] = ( VTFFileHeader_t * )pPtr;
	pPtr += sizeof( VTFFileHeader_t );
	memset( m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ], 0, sizeof( *m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ] ) );

	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->numResources = 1;
	ResourceEntryInfo	*pResource = ( ResourceEntryInfo * )pPtr;
	pPtr += sizeof( ResourceEntryInfo );

	// align the dds data to a 16 byte boundary
	pPtr = ( byte * )( ( ( uintp )( pPtr + 15 ) ) & ( ~16 ) );

	pResource->eType = VTF_LEGACY_RSRC_IMAGE;
	pResource->resData = pPtr - ( byte * )m_AtlasGroups[ nAtlasGroup ].m_CombinedTextureMemory[ nTexture ];
	

	int nMaxSize = ( MAX_COMBINED_WIDTH > MAX_COMBINED_HEIGHT ? MAX_COMBINED_WIDTH : MAX_COMBINED_HEIGHT );
	int nNumMips = 0;
	while( nMaxSize > 0 )
	{
		nNumMips++;
		nMaxSize >>= 1;
	}

	Q_strncpy( m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->fileTypeString, "VTF", 4 );
	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->version[ 0 ] = VTF_MAJOR_VERSION;
	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->version[ 1 ] = VTF_MINOR_VERSION;
	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->headerSize = pPtr - ( ( byte * )m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ] );

	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->imageFormat = m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ 0 ]->imageFormat;
	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->width = MAX_COMBINED_WIDTH;
	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->height = MAX_COMBINED_HEIGHT;
	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->numMipLevels = nNumMips;
	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->flags = m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ 0 ]->flags | TEXTUREFLAGS_COMBINED;
	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->numFrames = 1;
	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->startFrame = 0;
	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->depth = 1;
	// potential alignment issues 
	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->reflectivity.x = m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ 0 ]->reflectivity.x;
	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->reflectivity.y = m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ 0 ]->reflectivity.y;
	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->reflectivity.z = m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ 0 ]->reflectivity.z;
	m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->bumpScale = m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ 0 ]->bumpScale;

	byte	*pCombinedMipOffset[ MAX_COMBINED_MIP_LEVELS ];
	int		nCombinedMipWidth[ MAX_COMBINED_MIP_LEVELS ];
	int		nCombinedMipHeight[ MAX_COMBINED_MIP_LEVELS ];
	int		nCombinedMipSize[ MAX_COMBINED_MIP_LEVELS ];

	for( int nMip = nNumMips - 1; nMip >= 0; nMip-- )
	{
		pCombinedMipOffset[ nMip ] = pPtr;
		
		int nWidth = m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->width >> nMip;
		int nHeight = m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ]->height >> nMip;

		if ( nWidth < 4 )
		{
			nWidth = 4;
		}
		if ( nHeight < 4 )
		{
			nHeight = 4;
		}

		nWidth >>= 2;
		nHeight >>= 2;

		int nNumBlocks = ( nWidth * nHeight );
		nCombinedMipWidth[ nMip ] = nWidth;
		nCombinedMipHeight[ nMip ] = nHeight;
		nCombinedMipSize[ nMip ] = nNumBlocks * nBlockSize;

		pPtr += nCombinedMipSize[ nMip ];
	}

	for( int nMip = 0; nMip < nNumMips; nMip++ )
	{
		int nCombinedLineSize = ( nCombinedMipWidth[ nMip ] ) * nBlockSize;

		for( int nMaterial = 0; nMaterial < m_AtlasGroups[ nAtlasGroup ].m_nNumMaterials; nMaterial++ )
		{
			if ( !m_AtlasGroups[ nAtlasGroup ].m_bIsFlat[ nMaterial ] && nMip >= m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ]->numMipLevels )
			{
				continue;
			}

			int nNewX, nNewY, nWriteWidth, nWriteHeight;

			m_AtlasGroups[ nAtlasGroup ].m_pSimpleTexturePacker->GetTextureSize( nMaterial, nWriteWidth, nWriteHeight );
			m_AtlasGroups[ nAtlasGroup ].m_pSimpleTexturePacker->GetTextureLocation( nMaterial, nNewX, nNewY );

			// adjust nWriteWidth & nWriteHeight by the ratio for this texture
			nWriteWidth = (nWidthShift > 0) ? nWriteWidth >> nWidthShift : nWriteWidth << (-nWidthShift);
			nWriteHeight = (nHeightShift > 0) ? nWriteHeight >> nHeightShift : nWriteHeight << (-nHeightShift);

			nWriteWidth >>= ( 2 + nMip );	// 2 accounts for the DDS block encoding
			if ( nWriteWidth <= 0 )
			{
				if ( m_AtlasGroups[ nAtlasGroup ].m_bIsFlat[ nMaterial ] )
				{	// we don't need to replicate down any further
					continue;
				}
				nWriteWidth = 1;
			}
			nWriteHeight >>= ( 2 + nMip );	// 2 accounts for the DDS block encoding
			if ( nWriteHeight <= 0 )
			{
				if ( m_AtlasGroups[ nAtlasGroup ].m_bIsFlat[ nMaterial ] )
				{	// we don't need to replicate down any further
					continue;
				}
				nWriteHeight = 1;
			}
			nNewX >>= nMip;
			nNewY >>= nMip;

			byte *pWriteOffset = pCombinedMipOffset[ nMip ];
			pWriteOffset += ( nNewX >> 2 ) * nBlockSize;
			pWriteOffset += ( nNewY >> 2 ) * nCombinedLineSize;

			if ( m_AtlasGroups[ nAtlasGroup ].m_bIsFlat[ nMaterial ] )
			{
/*
				optimize below to use
				_mm_stream_ps
*/
				int nCombinedLineSizeDelta = nCombinedLineSize - ( nWriteWidth * nBlockSize );

				byte *pReadOffset = pMipOffset[ nMaterial ][ 0 ];
				for( int y = 0; y < nWriteHeight; y++ )
				{
					for( int x = 0; x < nWriteWidth; x++, pWriteOffset += nBlockSize )
					{
						memcpy( pWriteOffset, pReadOffset, nBlockSize );
					}
					pWriteOffset += nCombinedLineSizeDelta;
				}
			}
			else
			{
				int nReadWidth = nMipWidth[ nMaterial ][ nMip ];

				Assert( nWriteWidth == nReadWidth );
				Assert( nWriteHeight == nMipHeight[ nMaterial ][ nMip ] );

				int nReadSize = nReadWidth * nBlockSize;

				byte *pReadOffset = pMipOffset[ nMaterial ][ nMip ];
				for( int y = 0; y < nWriteHeight; y++ )
				{
/*					byte	*pReadCache = pReadOffset;
					int		nReadCacheSize = nReadSize;
					while ( nReadCacheSize >= 0 )
					{
						PREFETCH_128( pReadCache, nReadCacheSize );
						nReadCacheSize -= CACHE_LINE_SIZE;
					}
					*/
					memcpy( pWriteOffset, pReadOffset, nReadSize );
					pReadOffset += nReadSize;
					pWriteOffset += nCombinedLineSize;
				}
			}
		}
	}

	m_AtlasGroups[ nAtlasGroup ].m_nCombinedTextureSize[ nTexture ] = pPtr - ( ( byte * )m_AtlasGroups[ nAtlasGroup ].m_CombinedHeaders[ nTexture ] );

//	FileHandle_t fh = g_pFullFileSystem->Open( "rjtest.vtf", "wb" );
//	g_pFullFileSystem->Write( m_CombinedHeaders[ nTexture ], m_nCombinedTextureSize[ nTexture ], fh );
//	g_pFullFileSystem->Close( fh );

	for( int nMaterial = 0; nMaterial < m_AtlasGroups[ nAtlasGroup ].m_nNumMaterials; nMaterial++ )
	{
		delete m_AtlasGroups[ nAtlasGroup ].m_pVTFData[ nMaterial ];
		m_AtlasGroups[ nAtlasGroup ].m_pVTFData[ nMaterial ] = NULL;

		m_AtlasGroups[ nAtlasGroup ].m_pVTFFileHeader[ nMaterial ] = NULL;
	}

	m_pCombinedStudioData->m_Results.m_flTextureCombineDuration += ( float )( Plat_FloatTime() - flStartCombineTime );

	return true;
}


void CTextureCombine::GetTextureInfo( int nIndex, Vector2D &vStartST, Vector2D &vSizeST, Vector2D &vPixelSize )
{
	int nAtlasGroup = m_nMaterialAtlasInfo[ nIndex ][ ATLAS_INFO_GROUP_INDEX ];
	int nMaterial = m_nMaterialAtlasInfo[ nIndex ][ ATLAS_INFO_MATERIAL_INDEX ];

	int nStartS, nStartT;
	int nWidth, nHeight;

	if ( m_AtlasGroups[ nAtlasGroup ].m_pSimpleTexturePacker )
	{
		m_AtlasGroups[ nAtlasGroup ].m_pSimpleTexturePacker->GetTextureSize( nMaterial, nWidth, nHeight );
		m_AtlasGroups[ nAtlasGroup ].m_pSimpleTexturePacker->GetTextureLocation( nMaterial, nStartS, nStartT );

		vStartST.x = ( float )nStartS / ( float )MAX_COMBINED_WIDTH;
		vStartST.y = ( float )nStartT / ( float )MAX_COMBINED_HEIGHT;
		vSizeST.x = ( float )nWidth / ( float )MAX_COMBINED_WIDTH;
		vSizeST.y = ( float )nHeight / ( float )MAX_COMBINED_HEIGHT;
		vPixelSize.x = 1.0f / ( float )nWidth;
		vPixelSize.y = 1.0f / ( float )nHeight;
	}
	else
	{
		vStartST.x = 0.0f;
		vStartST.y = 0.0f;
		vSizeST.x = 1.0f;
		vSizeST.y = 1.0f;
		vPixelSize.x = 0.0f;
		vPixelSize.y = 0.0f;
	}
}
