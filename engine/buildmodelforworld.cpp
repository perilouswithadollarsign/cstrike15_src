#include "render_pch.h"

#if !defined( DEDICATED ) && !defined( _GAMECONSOLE )

#include "icliententitylist.h"
#include "icliententity.h"
#include "imagepacker.h"
#include "bitmap/tgawriter.h"
#include "client.h"
#include "tier2/fileutils.h"
#include "vstdlib/iprocessutils.h"
#include "appframework/iappsystem.h"
#include "appframework/IAppSystemGroup.h"
#include "appframework/AppFramework.h"
#include "../utils/common/bsplib.h"
#include "ibsppack.h"
#include "disp.h"
// Set to 0 if you want to be able to see intermediate files in hlmv, etc.
#define DELETE_INTERMEDIATE_FILES 1

// THIS NEEDS TO BE THE SAME IN worldimposter_ps2x.fxc!!!!
#define BASE_TIMES_LIGHTMAP_LINEAR_TONEMAP_SCALE 4.0f
// THIS NEEDS TO BE THE SAME IN worldimposter_ps2x.fxc!!!!

#define MAX_ATLAS_TEXTURE_DIMENSION 1024 
static IBSPPack *s_pBSPPack = NULL;
static CSysModule *s_pBSPPackModule = NULL;

static void LoadBSPPackInterface( void )
{
	// load the bsppack dll
	s_pBSPPackModule = FileSystem_LoadModule( "bsppack" );
	if ( s_pBSPPackModule )
	{
		CreateInterfaceFn factory = Sys_GetFactory( s_pBSPPackModule );
		if ( factory )
		{
			s_pBSPPack = ( IBSPPack * )factory( IBSPPACK_VERSION_STRING, NULL );
		}
	}
	if( !s_pBSPPack )
	{
		Error( "can't get bsppack interface\n" );
	}
}

static void UnloadBSPPackInterface( void )
{
	FileSystem_UnloadModule( s_pBSPPackModule );
	s_pBSPPack = NULL;
	s_pBSPPackModule = NULL;
}

static void RandomColor( Vector& color )
{
	color[0] = ( ( float )rand() ) / ( float )VALVE_RAND_MAX;
	color[1] = ( ( float )rand() ) / ( float )VALVE_RAND_MAX;
	color[2] = ( ( float )rand() ) / ( float )VALVE_RAND_MAX;
	VectorNormalize( color );
}

//-----------------------------------------------------------------------------
// Compute a context necessary for creating vertex data
//-----------------------------------------------------------------------------
static void SurfSetupSurfaceContextAtlased( SurfaceCtx_t &ctx, SurfaceHandle_t surfID, int x, int y, int nAtlasedTextureWidth, int nAtlasedTextureHeight )
{
	ctx.m_LightmapPageSize[0] = nAtlasedTextureWidth;
	ctx.m_LightmapPageSize[1] = nAtlasedTextureHeight;
	ctx.m_LightmapSize[0] = ( MSurf_LightmapExtents( surfID )[0] ) + 1;
	ctx.m_LightmapSize[1] = ( MSurf_LightmapExtents( surfID )[1] ) + 1;

	ctx.m_Scale.x = 1.0f / ( float )ctx.m_LightmapPageSize[0];
	ctx.m_Scale.y = 1.0f / ( float )ctx.m_LightmapPageSize[1];

	ctx.m_Offset.x = ( float )x * ctx.m_Scale.x;
	ctx.m_Offset.y = ( float )y * ctx.m_Scale.y;

	ctx.m_BumpSTexCoordOffset = 0.0f;
}

static void SurfComputeAtlasedTextureCoordinate( SurfaceCtx_t const& ctx, SurfaceHandle_t surfID, Vector const& vec, Vector2D& uv )
{
	if ( (MSurf_Flags( surfID ) & SURFDRAW_NOLIGHT) )
	{
		uv.x = uv.y = 0.5f;
	}
	else if ( MSurf_LightmapExtents( surfID )[0] == 0 )
	{
		uv = (0.5f * ctx.m_Scale + ctx.m_Offset);
	}
	else
	{
		mtexinfo_t* pTexInfo = MSurf_TexInfo( surfID );

		uv.x = DotProduct (vec, pTexInfo->lightmapVecsLuxelsPerWorldUnits[0].AsVector3D()) + 
			pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][3];
		uv.x -= MSurf_LightmapMins( surfID )[0];
		uv.x += 0.5f;

		uv.y = DotProduct (vec, pTexInfo->lightmapVecsLuxelsPerWorldUnits[1].AsVector3D()) + 
			pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][3];
		uv.y -= MSurf_LightmapMins( surfID )[1];
		uv.y += 0.5f;

		uv *= ctx.m_Scale;
		uv += ctx.m_Offset;

		assert( uv.IsValid() );
	}
#if _DEBUG
	// This was here for check against displacements and they actually get calculated later correctly.
	//	CheckTexCoord( uv.x );
	//	CheckTexCoord( uv.y );
#endif
	uv.x = clamp(uv.x, 0.0f, 1.0f);
	uv.y = clamp(uv.y, 0.0f, 1.0f);
}

void WriteDisplacementSurfaceToSMD( SurfaceHandle_t surfID, CDispInfo *pDispInfo, const SurfaceCtx_t &ctxAtlased, FileHandle_t smdfp )
{
	int nLightmapPageSize[2];
	materials->GetLightmapPageSize( SortInfoToLightmapPage( MSurf_MaterialSortID( surfID ) ), &nLightmapPageSize[0], &nLightmapPageSize[1] );
	Vector2D vOffset;
	float flPageSizeU = nLightmapPageSize[0];
	float flPageSizeV = nLightmapPageSize[1];

	vOffset.x = ( float )MSurf_OffsetIntoLightmapPage( surfID )[0] / flPageSizeU;
	vOffset.y = ( float )MSurf_OffsetIntoLightmapPage( surfID )[1] / flPageSizeV;

	for ( int i = 0; i < pDispInfo->m_nIndices; i += 3 )
	{
		g_pFullFileSystem->FPrintf( smdfp, "simpleworldmodel.tga\n" );
		for ( int j = 2; j >= 0; --j )
		{
			int nVert = pDispInfo->m_Indices[i + j] - pDispInfo->m_iVertOffset;
			CDispRenderVert *pVert = &pDispInfo->m_Verts[nVert];
			Vector vPos = pVert->m_vPos;
			Vector vNormal = pVert->m_vNormal;

			Vector2D uv = pVert->m_LMCoords;
			uv -= vOffset;
			uv.x *= flPageSizeU;
			uv.y *= flPageSizeV;

			uv *= ctxAtlased.m_Scale;
			uv += ctxAtlased.m_Offset;

			g_pFullFileSystem->FPrintf( smdfp, "%d %f %f %f %f %f %f %f %f\n", 0, vPos.x, vPos.y, vPos.z, vNormal.x, vNormal.y, vNormal.z, uv.x, 1.0f - uv.y );

		}
	}
}

void WriteSurfaceToSMD( worldbrushdata_t *pBrushData, SurfaceHandle_t surfID, int x, int y, FileHandle_t smdfp, int nAtlasedTextureWidth, int nAtlasedTextureHeight )
{
	SurfaceCtx_t ctxAtlased;
	SurfSetupSurfaceContextAtlased( ctxAtlased, surfID, x, y, nAtlasedTextureWidth, nAtlasedTextureHeight );

	if ( surfID->pDispInfo )
	{
		CDispInfo *pDispInfo = static_cast<CDispInfo *>(surfID->pDispInfo);
		WriteDisplacementSurfaceToSMD( surfID, pDispInfo, ctxAtlased, smdfp );
		return;
	}
	int vertCount = MSurf_VertCount( surfID );
	for ( int triID = 0; triID < vertCount - 2; triID++ )
	{
		// This really refers to simpleworldmodel.vmt
		g_pFullFileSystem->FPrintf( smdfp, "simpleworldmodel.tga\n" );

		for ( int triVertID = 2; triVertID >= 0; triVertID-- )
		{
			int i;
			switch( triVertID )
			{
			case 0:
				i = 0;
				break;
			case 1:
				i = triID + 1;
				break;
			case 2:
			default:
				i = triID + 2;
				break;
			}

			int vertIndex = pBrushData->vertindices[MSurf_FirstVertIndex( surfID ) + i];

			// world-space vertex
			Vector &vec = pBrushData->vertexes[vertIndex].position;

			// output to mesh
			Vector2D pos;
			SurfComputeAtlasedTextureCoordinate( ctxAtlased, surfID, vec, pos );

			Vector &normal = pBrushData->vertnormals[ pBrushData->vertnormalindices[MSurf_FirstVertNormal( surfID ) + i] ];
			g_pFullFileSystem->FPrintf( smdfp, "%d %f %f %f %f %f %f %f %f\n", 0, vec.x, vec.y, vec.z, normal.x, normal.y, normal.z, pos.x, 1.0f - pos.y );
		}
	}
}

static const float LUXEL_WORLD_SPACE_EPSILON = 1e-3;

// Based on code in lightmaptransfer.cpp in vbsp2lib.
static void CalculateLuxelToWorldTransform( const mtexinfo_t *pTexInfo, const Vector &vFaceNormal, float flFaceDistance, Vector *pLuxelOrigin, Vector *pS, Vector *pT )
{
	Vector vLuxelSpaceCross;

	vLuxelSpaceCross[0] = 
		pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][1] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][2] - 
		pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][2] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][1];
	vLuxelSpaceCross[1] = 
		pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][2] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][0] - 
		pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][0] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][2];
	vLuxelSpaceCross[2] = 
		pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][0] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][1] - 
		pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][1] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][0];

	float flDeterminant = -DotProduct( vFaceNormal, vLuxelSpaceCross );

	if ( fabs( flDeterminant ) < 1e-6 )
	{
//		Warning( "Warning - UV vectors are parallel to face normal, bad lighting will be produced.\n" );
		( *pLuxelOrigin ) = vec3_origin;
	}
	else
	{
		// invert the matrix
		( *pS )[0]	= (vFaceNormal[2] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][1] - vFaceNormal[1] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][2]) / flDeterminant;
		( *pT )[0]	= (vFaceNormal[1] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][2] - vFaceNormal[2] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][1]) / flDeterminant;
		( *pLuxelOrigin )[0]			= -(flFaceDistance * vLuxelSpaceCross[0]) / flDeterminant;
		( *pS )[1]  = (vFaceNormal[0] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][2] - vFaceNormal[2] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][0]) / flDeterminant; 
		( *pT )[1]  = (vFaceNormal[2] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][0] - vFaceNormal[0] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][2]) / flDeterminant; 
		( *pLuxelOrigin )[1]			= -(flFaceDistance * vLuxelSpaceCross[1]) / flDeterminant;
		( *pS )[2]  = (vFaceNormal[1] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][0] - vFaceNormal[0] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][1]) / flDeterminant; 
		( *pT )[2]  = (vFaceNormal[0] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][1] - vFaceNormal[1] * pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][0]) / flDeterminant; 
		( *pLuxelOrigin )[2]			= -(flFaceDistance * vLuxelSpaceCross[2]) / flDeterminant;

		// adjust for luxel offset
		VectorMA( ( *pLuxelOrigin ), -pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][3], ( *pS ), ( *pLuxelOrigin ) );
		VectorMA( ( *pLuxelOrigin ), -pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][3], ( *pT ), ( *pLuxelOrigin ) );

		Assert( fabsf( DOT_PRODUCT( *pLuxelOrigin, pTexInfo->lightmapVecsLuxelsPerWorldUnits[0] ) + pTexInfo->lightmapVecsLuxelsPerWorldUnits[0][3] ) < LUXEL_WORLD_SPACE_EPSILON );
		Assert( fabsf( DOT_PRODUCT( *pLuxelOrigin, pTexInfo->lightmapVecsLuxelsPerWorldUnits[1] ) + pTexInfo->lightmapVecsLuxelsPerWorldUnits[1][3] ) < LUXEL_WORLD_SPACE_EPSILON );
	}
}

static void R_ComputeSurfaceBasis( SurfaceHandle_t surfID, Vector &luxelBasePosition, Vector &sVect, Vector &tVect )
{
	CalculateLuxelToWorldTransform( MSurf_TexInfo( surfID ), MSurf_Plane( surfID ).normal, MSurf_Plane( surfID ).dist, &luxelBasePosition, &sVect, &tVect );
	luxelBasePosition += sVect * MSurf_LightmapMins( surfID )[0];
	luxelBasePosition += tVect * MSurf_LightmapMins( surfID )[1];
}

static void LuxelSpaceToWorld( SurfaceHandle_t surfID, Vector &worldPosition, float u, float v )
{
	Vector luxelBasePosition, sVect, tVect;
	R_ComputeSurfaceBasis( surfID, luxelBasePosition, sVect, tVect );
	worldPosition = luxelBasePosition;
	worldPosition += u * sVect * MSurf_LightmapExtents( surfID )[0];
	worldPosition += v * tVect * MSurf_LightmapExtents( surfID )[1];
}

static bool KeepSurface( SurfaceHandle_t surfID )
{
	IMaterial *pMaterial = materialSortInfoArray[MSurf_MaterialSortID( surfID )].material;

	if( pMaterial->IsTranslucent() )
	{
		static unsigned int nWorldImposterVarCache = 0;
		if ( IMaterialVar *pMaterialVar = pMaterial->FindVarFast( "$worldimposter", &nWorldImposterVarCache ) )
		{
			if ( pMaterialVar->GetIntValue() != 0 )
			{
				return true;
			}
		}
		return false;
	}

#if defined( CSTRIKE15 )
	if( MSurf_Flags( surfID ) & SURFDRAW_NODRAW )
	{
		return false;
	}

	if( MSurf_Flags(surfID) & SURFDRAW_SKY )
	{
		return false;
	}
#endif

	return true;
}

static bool ComputeMapName( char *pMapName, size_t nMapNameSize )
{
	IClientEntity *world = entitylist->GetClientEntity( 0 );

	if( world && world->GetModel() )
	{
		const model_t *pModel = world->GetModel();
		const char *pModelName = modelloader->GetName( pModel );

		// This handles the case where you have a map in a directory under maps. 
		// We need to keep everything after "maps/" so it looks for the BSP file in the right place.
		if ( Q_stristr( pModelName, "maps/" ) == pModelName ||
			Q_stristr( pModelName, "maps\\" ) == pModelName )
		{
			Q_strncpy( pMapName, &pModelName[5], nMapNameSize );
			Q_StripExtension( pMapName, pMapName, nMapNameSize );
		}
		else
		{
			Q_FileBase( pModelName, pMapName, nMapNameSize );
		}
		return true;
	}
	else
	{
		return false;
	}
}

static void ComputeAndMakeDirectories( const char *pMapName, char *pMatDir, size_t nMatDirSize, char *pMaterialSrcDir, size_t nMaterialSrcDirSize, char *pModelDir, size_t nModelDirSize, char *pModelSrcDir, size_t nModelSrcDirSize )
{
	// materials dir
	Q_snprintf( pMatDir, nMatDirSize, "materials/models/maps/%s", pMapName );
	g_pFileSystem->CreateDirHierarchy( pMatDir, "DEFAULT_WRITE_PATH" );

	// materialsrc dir
	char pTemp[MAX_PATH];
	Q_snprintf( pTemp, sizeof( pTemp ), "materialsrc/models/maps/%s", pMapName );
	GetModContentSubdirectory( pTemp, pMaterialSrcDir, nMaterialSrcDirSize );
	g_pFileSystem->CreateDirHierarchy( pMaterialSrcDir, NULL );

	// model dir
	Q_snprintf( pModelDir, nModelDirSize, "models/maps//%s", pMapName );
	g_pFileSystem->CreateDirHierarchy( pModelDir, "DEFAULT_WRITE_PATH" );

	// model src dir
	Q_snprintf( pTemp, sizeof( pTemp ), "models/maps/%s", pMapName );
	GetModContentSubdirectory( pTemp, pModelSrcDir, nModelSrcDirSize );
	g_pFileSystem->CreateDirHierarchy( pModelSrcDir, NULL );
}

static bool CreateSimpleWorldModelVMT( const char *pMaterialDir, const char *mapName )
{
	char vmtPath[MAX_PATH];
	V_strncpy( vmtPath, pMaterialDir, MAX_PATH );
	V_strncat( vmtPath, "/simpleworldmodel.vmt", MAX_PATH );
	FileHandle_t fp = g_pFullFileSystem->Open( vmtPath, "w" );
	if ( fp == FILESYSTEM_INVALID_HANDLE )
	{
		Warning( "can't create %s\n", vmtPath );
		return false;
	}

	g_pFullFileSystem->FPrintf( fp, "\"patch\"\n" );
	g_pFullFileSystem->FPrintf( fp, "{\n" );
	g_pFullFileSystem->FPrintf( fp, "\t\"include\" \"materials/engine/simpleworldmodel.vmt\"\n" );
	g_pFullFileSystem->FPrintf( fp, "\t\"insert\"\n" );
	g_pFullFileSystem->FPrintf( fp, "\t{\n" );
	g_pFullFileSystem->FPrintf( fp, "\t\t\"$basetexture\" \"models/maps/%s/simpleworldmodel\"\n", mapName );
	g_pFullFileSystem->FPrintf( fp, "\t\t\"$albedo\" \"models/maps/%s/simpleworldmodel_albedo\"\n", mapName );
	g_pFullFileSystem->FPrintf( fp, "\t\t\"$lightmap\" \"models/maps/%s/simpleworldmodel_lightmap\"\n", mapName );
	g_pFullFileSystem->FPrintf( fp, "\t}\n" );
	g_pFullFileSystem->FPrintf( fp, "}\n" );
	g_pFullFileSystem->Close( fp );

	return true;
}

static void CompileQC( const char *pFileName )
{
	// Spawn studiomdl.exe process to generate .mdl and associated files
	char cmdline[ 2 * MAX_PATH + 256 ];
	V_snprintf( cmdline, sizeof( cmdline ), "studiomdl.exe -nop4 %s", pFileName );

	int nExitCode = g_pProcessUtils->SimpleRunProcess( cmdline );
	if ( nExitCode == -1 )
	{
		Msg( "Failed compiling %s\n", pFileName );
		return;
	}

	Msg( "Compilation of \"%s\" succeeded\n", pFileName );
}

static void CreateAndCompileQCFile( const char *pModelSrcDir, const char *mapName, bool bWater )
{
	/*
	$cdmaterials "models/maps/sp_a2_laster_over_goo"
	$scale 1.0
	$surfaceprop "default"
	$staticprop


	$modelname "test/test.mdl"

	// --- geometry
	$body "Body" "smd/test.smd"

	// --- animation
	$sequence "idle" "smd/test.smd" fps 30
	*/
	char qcPath[MAX_PATH];
	V_snprintf( qcPath, MAX_PATH, "%s/simpleworldmodel%s.qc", pModelSrcDir, bWater ? "_water" : "" );

	FileHandle_t fp = g_pFileSystem->Open( qcPath, "w" );
	if ( fp == FILESYSTEM_INVALID_HANDLE )
	{
		Warning( "can't create qc file %s\n", qcPath );
		return;
	}

	g_pFileSystem->FPrintf( fp, "// -- generated by buildmodelforworld --\n" );
	g_pFileSystem->FPrintf( fp, "$cdmaterials \"models/maps/%s\"\n", mapName );
	g_pFileSystem->FPrintf( fp, "$scale 1.0\n" );
	g_pFileSystem->FPrintf( fp, "$surfaceprop \"default\"\n" );
	g_pFileSystem->FPrintf( fp, "$staticprop\n" );
	g_pFileSystem->FPrintf( fp, "$modelname \"maps/%s/simpleworldmodel%s.mdl\"\n", mapName, bWater ? "_water" : "" );
	g_pFileSystem->FPrintf( fp, "$body \"Body\" \"simpleworldmodel%s.smd\"\n", bWater ? "_water" : "" );
	g_pFileSystem->FPrintf( fp, "$sequence \"idle\" \"simpleworldmodel%s.smd\" fps 30\n", bWater ? "_water" : "" );

	g_pFileSystem->Close( fp );

	CompileQC( qcPath );
}

static void CompileTGA( const char *pFileName )
{
	// Spawn vtex.exe process to generate .vtf file
	char cmdline[ 2 * MAX_PATH + 256 ];
	V_snprintf( cmdline, sizeof( cmdline ), "vtex.exe -nop4 %s", pFileName );

	int nExitCode = g_pProcessUtils->SimpleRunProcess( cmdline );
	if ( nExitCode == -1 )
	{
		Msg( "Failed compiling %s\n", pFileName );
		return;
	}

	Msg( "Compilation of \"%s\" succeeded\n", pFileName );
}

static void AddFileToPackAndDeleteFile( const char *mapName, const char *gameDir, const char *pFormatString )
{
	char relativePath[MAX_PATH];
	char fullPath[MAX_PATH];
	V_snprintf( relativePath, MAX_PATH, pFormatString, mapName );
	V_snprintf( fullPath, MAX_PATH, "%s/%s", gameDir, relativePath );
	s_pBSPPack->AddFileToPack( relativePath, fullPath );

	if ( DELETE_INTERMEDIATE_FILES )
	{
		g_pFileSystem->RemoveFile( fullPath );
	}
}

static void RemoveFileFromPack( const char *mapName, const char *gameDir, const char *pFormatString )
{
	char relativePath[MAX_PATH];
	char fullPath[MAX_PATH];
	V_snprintf( relativePath, MAX_PATH, pFormatString, mapName );
	V_snprintf( fullPath, MAX_PATH, "%s/%s", gameDir, relativePath );
	s_pBSPPack->RemoveFileFromPack( relativePath );
}

static void RemoveContentFile( const char *mapName, const char *pFormatString )
{
	if ( DELETE_INTERMEDIATE_FILES )
	{
		char pTemp[MAX_PATH];
		char absolutePath[MAX_PATH];
		Q_snprintf( pTemp, sizeof( pTemp ), pFormatString, mapName );
		GetModContentSubdirectory( pTemp, absolutePath, sizeof( absolutePath ) );
		g_pFileSystem->RemoveFile( absolutePath );	
	}
}

class CPackedSurfaceInfo
{
public:
	SurfaceHandle_t m_SurfID;
	int m_nMins[2]; // texel offset in packed/atlased texture
	CPackedSurfaceInfo()
	{
		m_SurfID = SURFACE_HANDLE_INVALID;
		m_nMins[0] = -1;
		m_nMins[1] = -1;
	}
};

//
// calculate the packing for all of the world geometry in order to know what size render target to allocate for buliding textures.
//
static void PackSurfacesAndBuildSurfaceList( CUtlVector<CPackedSurfaceInfo> &packedSurfaces, int &nWidth, int &nHeight )
{
	worldbrushdata_t *pBrushData = host_state.worldbrush;
	int numSurfaces = pBrushData->nWorldFaceCount;

	CImagePacker imagePacker;
	imagePacker.Reset( 0, MAX_ATLAS_TEXTURE_DIMENSION, MAX_ATLAS_TEXTURE_DIMENSION );

	for( int surfaceIndex = 0; surfaceIndex < numSurfaces; surfaceIndex++ )
	{
		SurfaceHandle_t surfID = SurfaceHandleFromIndex( surfaceIndex );

		// Get the size of the lightmap page.
		int lightmapSize[2];
		lightmapSize[0] = ( MSurf_LightmapExtents( surfID )[0] ) + 1;
		lightmapSize[1] = ( MSurf_LightmapExtents( surfID )[1] ) + 1;

		if ( !KeepSurface( surfID ) )
		{
			continue;
		}
		CPackedSurfaceInfo &surfaceInfo = packedSurfaces[ packedSurfaces.AddToTail() ];
		surfaceInfo.m_SurfID = surfID;
		
		if ( !imagePacker.AddBlock( lightmapSize[0], lightmapSize[1], &surfaceInfo.m_nMins[0], &surfaceInfo.m_nMins[1] ) )
		{
			Warning( "failed allocating an atlased texture block in buildmodelforworld\n" );
		}
	}	

	int nMinWidth, nMinHeight;
	imagePacker.GetMinimumDimensions( &nMinWidth, &nMinHeight );
	nWidth = nMinWidth;
	nHeight = nMinHeight;
}

static void WriteSMDHeader( FileHandle_t smdfp )
{
	g_pFullFileSystem->FPrintf( smdfp, "version 1\n" );
	g_pFullFileSystem->FPrintf( smdfp, "nodes\n" );
	g_pFullFileSystem->FPrintf( smdfp, "0 \"polymsh_extracted2\" -1\n" );
	g_pFullFileSystem->FPrintf( smdfp, "end\n" );
	g_pFullFileSystem->FPrintf( smdfp, "skeleton\n" );
	g_pFullFileSystem->FPrintf( smdfp, "time 0\n" );
	g_pFullFileSystem->FPrintf( smdfp, "0 0.000000 0.000000 0.000000 1.570796 0.000000 0.000000\n" );
	g_pFullFileSystem->FPrintf( smdfp, "end\n" );
	g_pFullFileSystem->FPrintf( smdfp, "triangles\n" );
}

static void Push2DRenderingSetup( IMatRenderContext *pRenderContext, ITexture *pRenderTarget, int nAtlasedTextureWidth, int nAtlasedTextureHeight )
{
	pRenderContext->PushRenderTargetAndViewport( pRenderTarget );

	pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
	materials->ClearBuffers( true, false, false );

	float flPixelOffset = 0.5f;

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
	pRenderContext->Scale( 1, -1, 1 );
	pRenderContext->Ortho( 
		flPixelOffset * ( 1.0f / nAtlasedTextureWidth ), 
		flPixelOffset * ( 1.0f / nAtlasedTextureHeight ), 
		( flPixelOffset + nAtlasedTextureWidth ) * ( 1.0f / nAtlasedTextureWidth ), 
		( flPixelOffset + nAtlasedTextureHeight ) * ( 1.0f / nAtlasedTextureHeight ), -99999, 99999 );

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();
}

static void Pop2DRenderingSetup( IMatRenderContext *pRenderContext )
{
	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->PopMatrix();

	pRenderContext->PopRenderTargetAndViewport();
}

enum RenderToAtlasedTextureRenderMode_t
{
	RENDER_TO_ATLASED_TEXTURE_FULL_RENDERING = 0,
	RENDER_TO_ATLASED_TEXTURE_NO_LIGHTING,
	RENDER_TO_ATLASED_TEXTURE_LIGHTING_ONLY,
};

class CDisplacementData
{
public:

	CDisplacementData( HDISPINFOARRAY hDispInfo, int nDispInfo )
	{
		m_nDispStartVert.SetCount( nDispInfo );
		int nStartVert = 0;
		for ( int i = 0; i < nDispInfo; i++ )
		{
			CDispInfo *pDisp = static_cast<CDispInfo *> ( DispInfo_IndexArray( hDispInfo, i ) );
			m_nDispStartVert[i] = nStartVert;
			nStartVert += pDisp->NumVerts();
		}

		// total number of verts, size memory
		m_dispVerts.SetCount( nStartVert );
		// now load from disk
		CMapLoadHelper lh( LUMP_DISP_VERTS );
		lh.LoadLumpData( 0, nStartVert * sizeof(CDispVert), m_dispVerts.Base() );
	}
	CUtlVector<int> m_nDispStartVert;
	CUtlVector<CDispVert> m_dispVerts;
};

struct surfacerect_t
{
	Vector		m_vPos[4];
	Vector		m_vPosWorld[4];
	Vector2D	m_LMCoords[4];
	float		m_flBumpSTexCoordOffset;
	int			m_nTexCoordIndexOffset;
};


void DrawTexturedQuad( IMaterial *pMaterial, IMatRenderContext *pRenderContext, SurfaceHandle_t surfID, const surfacerect_t &rect, const Vector4D &vColor )
{
	IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );

	CMeshBuilder builder;
	builder.Begin( pMesh, MATERIAL_POLYGON, 4 );
	for ( int vertID = 0; vertID < 4; vertID++ )
	{
		const Vector &vecPageCoord = rect.m_vPos[vertID];

		Vector2D texCoord;
		SurfComputeTextureCoordinate( surfID, rect.m_vPosWorld[( vertID + rect.m_nTexCoordIndexOffset ) % 4], texCoord.Base() );

		// Need to figure out what the world position is of vecPageCoord.
		builder.Position3fv( vecPageCoord.Base() ); 		
		builder.Color4fv( vColor.Base() );
		builder.TexCoord2fv( 0, texCoord.Base() );
		builder.TexCoord2fv( 1, rect.m_LMCoords[vertID].Base() );
		if ( MSurf_Flags( surfID ) & SURFDRAW_BUMPLIGHT )
		{
			// bump maps appear left to right in lightmap page memory, calculate 
			// the offset for the width of a single map. The pixel shader will use 
			// this to compute the actual texture coordinates
			builder.TexCoord2f( 2, rect.m_flBumpSTexCoordOffset, 0.0f );
		}
		else
		{
			// PORTAL 2 FIX - paint shader assumes it can use 3 lightmapped coordinates in all cases, so set the offset to something reasonable
			builder.TexCoord2f( 2, 0.0f, 0.0f );
		}
		builder.Normal3f( 0.0f, 0.0f, 1.0f );

		builder.AdvanceVertex();  
	}

	builder.End( false, true );

}

// includes left and top pixel, excludes right and bottom pixels
// This draws the whole lightmap space for the surface including the half texel boards into the atlased texture.
void DrawSurfaceRectToAtlasedTexture( float left, float top, float right, float bottom, int nRenderTargetWidth, int nRenderTargetHeight, SurfaceHandle_t surfID, RenderToAtlasedTextureRenderMode_t renderMode, const CDisplacementData &dispData )
{
	float flOffset = 0.0f;

	// Could compute all this from the ctxAtlased instead.  Make sure they generate the same result.
	left = ( flOffset + left ) * ( 1.0f / ( float )nRenderTargetWidth );
	right = ( flOffset + right ) * ( 1.0f / ( float )nRenderTargetWidth );
	top = ( flOffset + top ) * ( 1.0f / ( float )nRenderTargetHeight );
	bottom = ( flOffset + bottom ) * ( 1.0f / ( float )nRenderTargetHeight );

	SurfaceCtx_t ctx;
	SurfSetupSurfaceContext( ctx, surfID );

	SurfaceCtx_t ctxAtlased;
	SurfSetupSurfaceContextAtlased( ctxAtlased, surfID, left, top, nRenderTargetWidth, nRenderTargetHeight );

	Vector tVect;
	bool negate = false;
	if ( MSurf_Flags( surfID ) & SURFDRAW_TANGENTSPACE )
	{
		negate = TangentSpaceSurfaceSetup( surfID, tVect );
	}

	CMatRenderContextPtr pRenderContext( materials );
	IMaterial *pMaterial = materialSortInfoArray[ MSurf_MaterialSortID( surfID ) ].material;
	pRenderContext->Bind( pMaterial, NULL );
	if ( renderMode == RENDER_TO_ATLASED_TEXTURE_NO_LIGHTING )
	{
		if ( MSurf_Flags( surfID ) & SURFDRAW_BUMPLIGHT )
		{
			pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE_BUMP );
		}
		else
		{
			pRenderContext->BindLightmapPage( MATERIAL_SYSTEM_LIGHTMAP_PAGE_WHITE );
		}
	}
	else
	{
		pRenderContext->BindLightmapPage( materialSortInfoArray[ MSurf_MaterialSortID( surfID ) ].lightmapPageID );
	}

	float lightmapLeft = ctx.m_Offset.x;
	float lightmapTop = ctx.m_Offset.y;
	float lightmapRight = ctx.m_LightmapSize[0] * ctx.m_Scale.x + ctx.m_Offset.x;
	float lightmapBottom = ctx.m_LightmapSize[1] * ctx.m_Scale.y + ctx.m_Offset.y;

	Vector worldPosition;
	Vector2D testUV;
	LuxelSpaceToWorld( surfID, worldPosition, 0.0f, 0.0f );
	SurfComputeLightmapCoordinate( ctx, surfID, worldPosition, testUV );
	// -0.5f to account for half-texel border
	float flLeft = testUV.x - 0.5f * ctx.m_Scale.x;
	float flTop = testUV.y - 0.5f * ctx.m_Scale.y;

	LuxelSpaceToWorld( surfID, worldPosition, 1.0f, 1.0f );
	SurfComputeLightmapCoordinate( ctx, surfID, worldPosition, testUV );
	// +0.5f to account for half-texel border
	float flRight = testUV.x + 0.5f * ctx.m_Scale.x;
	float flBottom = testUV.y + 0.5f * ctx.m_Scale.y;

	surfacerect_t surfaceRect;
	surfaceRect.m_flBumpSTexCoordOffset = ctx.m_BumpSTexCoordOffset;
	surfaceRect.m_nTexCoordIndexOffset = 0;
	surfaceRect.m_vPos[0].Init( left, bottom, 0.5f );
	surfaceRect.m_vPos[1].Init( left, top, 0.5f );
	surfaceRect.m_vPos[2].Init( right, top, 0.5f );
	surfaceRect.m_vPos[3].Init( right, bottom, 0.5f );

	Vector4D vColor;
	RandomColor( vColor.AsVector3D() );
	vColor.w = 0.0f;	// default alpha of 0.0f
	LuxelSpaceToWorld( surfID, surfaceRect.m_vPosWorld[0], 0.0f, 1.0f );
	LuxelSpaceToWorld( surfID, surfaceRect.m_vPosWorld[1], 0.0f, 0.0f );
	LuxelSpaceToWorld( surfID, surfaceRect.m_vPosWorld[2], 1.0f, 0.0f );
	LuxelSpaceToWorld( surfID, surfaceRect.m_vPosWorld[3], 1.0f, 1.0f );

	// round to the nearest integer in texels (+0.5f for rounding)
	float flLeftSnapped = ctx.m_Scale.x * ( float )( int )( ctx.m_LightmapPageSize[0] * flLeft + 0.5f );
	float flRightSnapped = ctx.m_Scale.x * ( float )( int )( ctx.m_LightmapPageSize[0] * flRight + 0.5f );
	float flTopSnapped = ctx.m_Scale.y * ( float )( int )( ctx.m_LightmapPageSize[1] * flTop + 0.5f );
	float flBottomSnapped = ctx.m_Scale.y * ( float )( int )( ctx.m_LightmapPageSize[1] * flBottom + 0.5f );

	Assert( flLeftSnapped == lightmapLeft );
	Assert( flRightSnapped == lightmapRight );
	Assert( flTopSnapped == lightmapTop );
	Assert( flBottomSnapped == lightmapBottom );

	surfaceRect.m_LMCoords[0].Init( flLeftSnapped, flBottomSnapped );
	surfaceRect.m_LMCoords[1].Init( flLeftSnapped, flTopSnapped );
	surfaceRect.m_LMCoords[2].Init( flRightSnapped, flTopSnapped );
	surfaceRect.m_LMCoords[3].Init( flRightSnapped, flBottomSnapped );

	int nTexCoordShift = 0;
	if ( surfID->pDispInfo )
	{
		CDispInfo *pDispInfo = static_cast<CDispInfo *>(surfID->pDispInfo);
		nTexCoordShift = pDispInfo->m_iPointStart;
	}
	DrawTexturedQuad( pMaterial, pRenderContext, surfID, surfaceRect, vColor );

	if ( surfID->pDispInfo && renderMode != RENDER_TO_ATLASED_TEXTURE_LIGHTING_ONLY )
	{
		CDispInfo *pDispInfo = static_cast<CDispInfo *>(surfID->pDispInfo);

		int nLightmapPageSize[2];
		materials->GetLightmapPageSize( SortInfoToLightmapPage( MSurf_MaterialSortID( surfID ) ), &nLightmapPageSize[0], &nLightmapPageSize[1] );
		Vector2D vOffset;
		float flPageSizeU = nLightmapPageSize[0];
		float flPageSizeV = nLightmapPageSize[1];

		vOffset.x = ( float )MSurf_OffsetIntoLightmapPage( surfID )[0] / flPageSizeU;
		vOffset.y = ( float )MSurf_OffsetIntoLightmapPage( surfID )[1] / flPageSizeV;

		pRenderContext->Bind( pMaterial, NULL );

		IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );

		float flLightmapSizeU = ( MSurf_LightmapExtents( surfID )[0] ) + 1;
		float flLightmapSizeV = ( MSurf_LightmapExtents( surfID )[1] ) + 1;

		int nTriangleCount = pDispInfo->m_nIndices / 3;
		CMeshBuilder builder;
		builder.Begin( pMesh, MATERIAL_TRIANGLES, nTriangleCount * 2 );

		int nDispIndex = DispInfo_ComputeIndex( host_state.worldbrush->hDispInfos, surfID->pDispInfo );
		int nStartVert = dispData.m_nDispStartVert[ nDispIndex ];
		Vector4D vColor(1,1,1,0);
		for ( int i = 0; i < pDispInfo->m_Verts.Count(); i++ )
		{
			// Need to figure out what the world position is of vecPageCoord.
			CDispRenderVert *pVert = &pDispInfo->m_Verts[i];

			const CDispVert *pDiskVert = &dispData.m_dispVerts[ nStartVert + i];
			vColor.w = pDiskVert->m_flAlpha * (1.0f / 255.0f);

			Vector2D uv = pVert->m_LMCoords;
			uv -= vOffset;
			uv.x *= flPageSizeU / flLightmapSizeU;
			uv.y *= flPageSizeV / flLightmapSizeV;

			// uv is now in [0,1] for lightmap space, use this to generate a position
			Vector vPos( left + uv.x * (right-left), top + uv.y * (bottom-top), 0.5f );
//			Vector2D vLuxel( flLeftSnapped + uv.x * (flRightSnapped-flLeftSnapped), flTopSnapped + uv.y * (flBottomSnapped-flTopSnapped) );
			Vector2D vLuxel( lightmapLeft + uv.x * (lightmapRight-lightmapLeft), lightmapTop + uv.y * (lightmapBottom-lightmapTop) );
			builder.Position3fv( vPos.Base() ); 		
			builder.Color4fv( vColor.Base() );

			builder.TexCoord2fv( 0, pVert->m_vTexCoord.Base() );
			builder.TexCoord2fv( 1, vLuxel.Base() );//pVert->m_LMCoords.Base() );
			builder.TangentS3fv( pVert->m_vSVector.Base() );
			builder.TangentT3fv( pVert->m_vTVector.Base() );
			if ( MSurf_Flags( surfID ) & SURFDRAW_BUMPLIGHT )
			{
				// bump maps appear left to right in lightmap page memory, calculate 
				// the offset for the width of a single map. The pixel shader will use 
				// this to compute the actual texture coordinates
				builder.TexCoord2f( 2, ctx.m_BumpSTexCoordOffset, 0.0f );
			}
			else
			{
				// PORTAL 2 FIX - paint shader assumes it can use 3 lightmapped coordinates in all cases, so set the offset to something reasonable
				builder.TexCoord2f( 2, 0.0f, 0.0f );
			}
			builder.Normal3f( 0.0f, 0.0f, 1.0f );

			builder.AdvanceVertex();  
		}
		for ( int i = 0; i < pDispInfo->m_nIndices; i += 3 )
		{
			builder.FastIndex( pDispInfo->m_Indices[i+0] - pDispInfo->m_iVertOffset );
			builder.FastIndex( pDispInfo->m_Indices[i+1] - pDispInfo->m_iVertOffset );
			builder.FastIndex( pDispInfo->m_Indices[i+2] - pDispInfo->m_iVertOffset );

			// draw all triangles twice in case they are backfacing due to the projection into texture space
			builder.FastIndex( pDispInfo->m_Indices[i+2] - pDispInfo->m_iVertOffset );
			builder.FastIndex( pDispInfo->m_Indices[i+1] - pDispInfo->m_iVertOffset );
			builder.FastIndex( pDispInfo->m_Indices[i+0] - pDispInfo->m_iVertOffset );
		}

		builder.End( false, true );
	}
}

static void RenderToAtlasedTexture( const CUtlVector<CPackedSurfaceInfo> &packedSurfaces, IMatRenderContext *pRenderContext, ITexture *pRenderTarget, int nAtlasedTextureWidth, int nAtlasedTextureHeight, const char *pMaterialSrcDir, const char *pTextureBaseName, RenderToAtlasedTextureRenderMode_t renderMode, const CDisplacementData &dispData, float flToneMapScale )
{
	// Disable specular for atlased texture generation.
	bool bSaveMatSpecular = mat_fastspecular.GetBool();
	mat_fastspecular.SetValue( "0" );
	if ( renderMode == RENDER_TO_ATLASED_TEXTURE_LIGHTING_ONLY )
	{
		mat_fullbright.SetValue( "2" );
	}
	else
	{
		mat_fullbright.SetValue( "0" );
	}
	UpdateMaterialSystemConfig();
	materials->EndFrame();
	materials->BeginFrame( host_frametime );

	pRenderContext->SetToneMappingScaleLinear( Vector( flToneMapScale, flToneMapScale, flToneMapScale ) );


	Push2DRenderingSetup( pRenderContext, pRenderTarget, nAtlasedTextureWidth, nAtlasedTextureHeight );

	// run through all of the faces and render them into the render target.
	FOR_EACH_VEC( packedSurfaces, packedSurfaceIndex )
	{
		const CPackedSurfaceInfo &packedSurface = packedSurfaces[packedSurfaceIndex];

		// Get the size of the lightmap subpage for this surface.
		int lightmapSize[2];
		lightmapSize[0] = ( MSurf_LightmapExtents( packedSurface.m_SurfID )[0] ) + 1;
		lightmapSize[1] = ( MSurf_LightmapExtents( packedSurface.m_SurfID )[1] ) + 1;

		DrawSurfaceRectToAtlasedTexture( packedSurface.m_nMins[0], packedSurface.m_nMins[1], 
			packedSurface.m_nMins[0] + lightmapSize[0], packedSurface.m_nMins[1] + lightmapSize[1], 
			nAtlasedTextureWidth, nAtlasedTextureHeight, packedSurface.m_SurfID, renderMode, dispData );
	}	

	// Read the resulting image.
	unsigned char *pImage = new unsigned char[nAtlasedTextureWidth * nAtlasedTextureHeight * 4];
	pRenderContext->ReadPixels( 0, 0, nAtlasedTextureWidth, nAtlasedTextureHeight, pImage, IMAGE_FORMAT_RGBA8888 );

	// Write the image to a TGA file.
	char tgaPath[MAX_PATH];
	V_snprintf( tgaPath, MAX_PATH, "%s/%s.tga", pMaterialSrcDir, pTextureBaseName );
	TGAWriter::WriteTGAFile( tgaPath, nAtlasedTextureWidth, nAtlasedTextureHeight, IMAGE_FORMAT_RGBA8888, pImage, nAtlasedTextureWidth * 4 );
	delete [] pImage;

	// Write a txt file with the vtex options for the TGA file.
	char txtPath[MAX_PATH];
	V_snprintf( txtPath, MAX_PATH, "%s/%s.txt", pMaterialSrcDir, pTextureBaseName );
	FileHandle_t txtFP = g_pFullFileSystem->Open( txtPath, "w", NULL );
	g_pFullFileSystem->FPrintf( txtFP, "\"nocompress\" \"1\"\n" );
	g_pFullFileSystem->FPrintf( txtFP, "\"nomip\" \"1\"\n" );
	g_pFullFileSystem->FPrintf( txtFP, "\"nolod\" \"1\"\n" );
	g_pFullFileSystem->Close( txtFP );

	// Compile the TGA file.
	CompileTGA( tgaPath );

	if ( DELETE_INTERMEDIATE_FILES )
	{
		g_pFullFileSystem->RemoveFile( txtPath );
		g_pFullFileSystem->RemoveFile( tgaPath );
	}

	Pop2DRenderingSetup( pRenderContext );

	// restore specular
	if( bSaveMatSpecular )
	{
		mat_fastspecular.SetValue( "1" );
	}
	else
	{
		mat_fastspecular.SetValue( "0" );
	}

	mat_fullbright.SetValue( "0" );
	UpdateMaterialSystemConfig();
	materials->EndFrame();
	materials->BeginFrame( host_frametime );
}

static int WriteSMD( const CUtlVector<CPackedSurfaceInfo> &packedSurfaces, int nAtlasedTextureWidth, int nAtlasedTextureHeight, const char *pModelSrcDir, bool bWater )
{
	char smdPath[MAX_PATH];
	V_snprintf( smdPath, MAX_PATH, "%s/simpleworldmodel%s.smd", pModelSrcDir, bWater ? "_water" : "" );

	FileHandle_t smdfp = g_pFullFileSystem->Open( smdPath, "w", NULL );
	WriteSMDHeader( smdfp );

	int nSurfaces = 0;

	// run through all of the faces and render them into the render target.
	worldbrushdata_t *pBrushData = host_state.worldbrush;
	FOR_EACH_VEC( packedSurfaces, packedSurfaceIndex )
	{
		const CPackedSurfaceInfo &packedSurface = packedSurfaces[packedSurfaceIndex];

		IMaterial *pMaterial = materialSortInfoArray[MSurf_MaterialSortID( packedSurface.m_SurfID )].material;
		bool bIsWater = ( V_stristr( pMaterial->GetShaderName(), "water" ) != 0 );

		if ( bIsWater != bWater )
		{
			continue;
		}

		nSurfaces++;

		// Get the size of the lightmap subpage for this surface.
		int lightmapSize[2];
		lightmapSize[0] = ( MSurf_LightmapExtents( packedSurface.m_SurfID )[0] ) + 1;
		lightmapSize[1] = ( MSurf_LightmapExtents( packedSurface.m_SurfID )[1] ) + 1;

		// Draw the face with both facings since we don't know which way it's going to face in the texture page.
		WriteSurfaceToSMD( pBrushData, packedSurface.m_SurfID, packedSurface.m_nMins[0], packedSurface.m_nMins[1], smdfp, nAtlasedTextureWidth, nAtlasedTextureHeight );
	}	

	g_pFullFileSystem->FPrintf( smdfp, "end\n" );
	g_pFullFileSystem->Close( smdfp );
	return nSurfaces;
}

ConVar r_buildingmapforworld( "r_buildingmapforworld", "0" );

CON_COMMAND( buildmodelforworld, "buildmodelforworld" )
{
	r_buildingmapforworld.SetValue( 1 );
	extern void V_RenderVGuiOnly();

	if ( g_LostVideoMemory )
	{
		r_buildingmapforworld.SetValue( 0 );
		return;
	}

	// Make sure that the file is writable before building cubemaps.
	Assert( g_pFileSystem->FileExists( GetBaseLocalClient().m_szLevelName, "GAME" ) );
	if( !g_pFileSystem->IsFileWritable( GetBaseLocalClient().m_szLevelName, "GAME" ) )
	{
		Warning( "%s is not writable!!!  Check it out before running buildmodelforworld.\n", GetBaseLocalClient().m_szLevelName );
		r_buildingmapforworld.SetValue( 0 );
		return;
	}

	char mapName[MAX_PATH];
	if ( !ComputeMapName( mapName, sizeof( mapName ) ) )
	{
		Warning( "can't buildmodelforworld.  Map not loaded.\n" );
		r_buildingmapforworld.SetValue( 0 );
		return;
	}

	char matDir[MAX_PATH];
	char materialSrcDir[MAX_PATH];
	char modelDir[MAX_PATH];
	char modelSrcDir[MAX_PATH];
	ComputeAndMakeDirectories( mapName, matDir, MAX_PATH, materialSrcDir, MAX_PATH, modelDir, MAX_PATH, modelSrcDir, MAX_PATH );

	char gameDir[MAX_PATH];
	COM_GetGameDir( gameDir, sizeof( gameDir ) );

	if ( !CreateSimpleWorldModelVMT( matDir, mapName ) ) 
	{
		r_buildingmapforworld.SetValue( 0 );
		return;
	}

	// turn off queued material system while we are doing this work.
	bool bAllow = Host_AllowQueuedMaterialSystem( false );

	// do this to force a frame to render so the material system syncs up to single thread mode
	V_RenderVGuiOnly();

	// we need to load some data from disk again to make displacement alpha work
	CMapLoadHelper::Init( host_state.worldmodel, host_state.worldmodel->szPathName );
	CDisplacementData dispData( host_state.worldbrush->hDispInfos, host_state.worldbrush->numDispInfos );
	CMapLoadHelper::Shutdown();


	CMatRenderContextPtr pRenderContext( materials );

	CUtlVector<CPackedSurfaceInfo> packedSurfaces;
	int nAtlasedTextureWidth, nAtlasedTextureHeight;
	PackSurfacesAndBuildSurfaceList( packedSurfaces, nAtlasedTextureWidth, nAtlasedTextureHeight );

	// Allocate a render target to render into.  THIS WILL LEAK!!!!!  There isn't a good way in source 1 to delete the rendertarget, 
	// so I'm going to spew this when we are done so that the user knows that they are going to leak.
	materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly();
	materials->BeginRenderTargetAllocation();
	ITexture *pRenderTarget = materials->CreateRenderTargetTexture( nAtlasedTextureWidth, nAtlasedTextureHeight, RT_SIZE_NO_CHANGE, IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_NONE );
	materials->EndRenderTargetAllocation();
	Assert( pRenderTarget );

	RenderToAtlasedTexture( packedSurfaces, pRenderContext, pRenderTarget, nAtlasedTextureWidth, nAtlasedTextureHeight, materialSrcDir, "simpleworldmodel", RENDER_TO_ATLASED_TEXTURE_FULL_RENDERING, dispData, BASE_TIMES_LIGHTMAP_LINEAR_TONEMAP_SCALE );
	RenderToAtlasedTexture( packedSurfaces, pRenderContext, pRenderTarget, nAtlasedTextureWidth, nAtlasedTextureHeight, materialSrcDir, "simpleworldmodel_lightmap", RENDER_TO_ATLASED_TEXTURE_LIGHTING_ONLY, dispData, 1.0f );
	RenderToAtlasedTexture( packedSurfaces, pRenderContext, pRenderTarget, nAtlasedTextureWidth, nAtlasedTextureHeight, materialSrcDir, "simpleworldmodel_albedo", RENDER_TO_ATLASED_TEXTURE_NO_LIGHTING, dispData, 1.0f );
	int nWaterSurfaces = WriteSMD( packedSurfaces, nAtlasedTextureWidth, nAtlasedTextureHeight, modelSrcDir, true /* bWater */ );
	int nNonWaterSurfaces = WriteSMD( packedSurfaces, nAtlasedTextureWidth, nAtlasedTextureHeight, modelSrcDir, false /* bWater */ );

	Host_AllowQueuedMaterialSystem( bAllow );

	if ( nWaterSurfaces > 0 )
	{
		CreateAndCompileQCFile( modelSrcDir, mapName, true /* bWater */ );
	}
	if ( nNonWaterSurfaces > 0 )
	{
		CreateAndCompileQCFile( modelSrcDir, mapName, false /* bWater */ );
	}

	LoadBSPPackInterface();

	char mapPath[MAX_PATH];
	Q_snprintf( mapPath, sizeof( mapPath ), "maps/%s.bsp", mapName );
	s_pBSPPack->LoadBSPFile( g_pFileSystem, mapPath );

	// add files to bsp zip file and nuke local copies (fixme. .move these closer to the code that creates and compiles the assets)
	if ( nNonWaterSurfaces > 0 )
	{
		AddFileToPackAndDeleteFile( mapName, gameDir, "models/maps/%s/simpleworldmodel.mdl" );
		AddFileToPackAndDeleteFile( mapName, gameDir, "models/maps/%s/simpleworldmodel.dx90.vtx" );
		AddFileToPackAndDeleteFile( mapName, gameDir, "models/maps/%s/simpleworldmodel.vvd" );
	}
	else
	{
		RemoveFileFromPack( mapName, gameDir, "models/maps/%s/simpleworldmodel.mdl" );
		RemoveFileFromPack( mapName, gameDir, "models/maps/%s/simpleworldmodel.dx90.vtx" );
		RemoveFileFromPack( mapName, gameDir, "models/maps/%s/simpleworldmodel.vvd" );
	}
	if ( nWaterSurfaces > 0 )
	{
		AddFileToPackAndDeleteFile( mapName, gameDir, "models/maps/%s/simpleworldmodel_water.mdl" );
		AddFileToPackAndDeleteFile( mapName, gameDir, "models/maps/%s/simpleworldmodel_water.dx90.vtx" );
		AddFileToPackAndDeleteFile( mapName, gameDir, "models/maps/%s/simpleworldmodel_water.vvd" );
	}
	else
	{
		RemoveFileFromPack( mapName, gameDir, "models/maps/%s/simpleworldmodel_water.mdl" );
		RemoveFileFromPack( mapName, gameDir, "models/maps/%s/simpleworldmodel_water.dx90.vtx" );
		RemoveFileFromPack( mapName, gameDir, "models/maps/%s/simpleworldmodel_water.vvd" );
	}
	if ( nWaterSurfaces > 0 || nNonWaterSurfaces > 0 )
	{
		AddFileToPackAndDeleteFile( mapName, gameDir, "materials/models/maps/%s/simpleworldmodel.vmt" );
		AddFileToPackAndDeleteFile( mapName, gameDir, "materials/models/maps/%s/simpleworldmodel.pwl.vtf" );
		AddFileToPackAndDeleteFile( mapName, gameDir, "materials/models/maps/%s/simpleworldmodel.vtf" );
		AddFileToPackAndDeleteFile( mapName, gameDir, "materials/models/maps/%s/simpleworldmodel_albedo.pwl.vtf" );
		AddFileToPackAndDeleteFile( mapName, gameDir, "materials/models/maps/%s/simpleworldmodel_albedo.vtf" );
		AddFileToPackAndDeleteFile( mapName, gameDir, "materials/models/maps/%s/simpleworldmodel_lightmap.pwl.vtf" );
		AddFileToPackAndDeleteFile( mapName, gameDir, "materials/models/maps/%s/simpleworldmodel_lightmap.vtf" );
	}
	else
	{
		RemoveFileFromPack( mapName, gameDir, "materials/models/maps/%s/simpleworldmodel.vmt" );
		RemoveFileFromPack( mapName, gameDir, "materials/models/maps/%s/simpleworldmodel.pwl.vtf" );
		RemoveFileFromPack( mapName, gameDir, "materials/models/maps/%s/simpleworldmodel.vtf" );
		RemoveFileFromPack( mapName, gameDir, "materials/models/maps/%s/simpleworldmodel_albedo.pwl.vtf" );
		RemoveFileFromPack( mapName, gameDir, "materials/models/maps/%s/simpleworldmodel_albedo.vtf" );
		RemoveFileFromPack( mapName, gameDir, "materials/models/maps/%s/simpleworldmodel_lightmap.pwl.vtf" );
		RemoveFileFromPack( mapName, gameDir, "materials/models/maps/%s/simpleworldmodel_lightmap.vtf" );
	}

	s_pBSPPack->WriteBSPFile( mapPath );

	// nuke files generated in the content directory
	RemoveContentFile( mapName, "models/maps/%s/simpleworldmodel.qc" );
	RemoveContentFile( mapName, "models/maps/%s/simpleworldmodel.smd" );
	RemoveContentFile( mapName, "models/maps/%s/simpleworldmodel_water.qc" );
	RemoveContentFile( mapName, "models/maps/%s/simpleworldmodel_water.smd" );

	UnloadBSPPackInterface();

	Warning( "*****************It is recommended to quit the game after running buildmodelforworld!  Leaks rendertargets!****************\n" );
	r_buildingmapforworld.SetValue( 0 );
}


#endif // !defined( DEDICATED ) && !defined( _GAMECONSOLE )
