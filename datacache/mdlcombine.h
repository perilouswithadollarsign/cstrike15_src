//========= Copyright c 1996-2011, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//====================================================================================//

#ifndef __MDLCOMBINE_H
#define __MDLCOMBINE_H

#ifdef _WIN32
#pragma once
#endif


#include "utlbuffer.h"


struct studiohdr2_t;
struct studiodata_t;
struct mstudiobone_t;
struct mstudioanimdesc_t;
struct mstudiomodel_t;
class CModelCombine;
class KeyValues;


namespace OptimizedModel
{
	struct StripGroupHeader_t;
	struct StripHeader_t;
	struct MeshHeader_t;
	struct ModelLODHeader_t;
	struct ModelHeader_t;
	struct BodyPartHeader_t;
}


//#define DEBUG_COMBINE	1


#define COMBINER_MAX_STRINGS				2000
#define COMBINER_MAX_BONES					( 53 * 2 )
#define COMBINER_MAX_SUB_MODELS				20
#define COMBINER_WORK_BUFFER_SIZE			( 2 * 1024 * 1024 )
#define COMBINER_MAX_MATERIALS				( COMBINER_MAX_MATERIALS_PER_INPUT_MODEL * COMBINER_MAX_MODELS )
#define COMBINER_MAX_BODYPARTS_PER_MODEL	5

typedef struct SAtlasGroup
{
	unsigned char	*m_pCombinedTextures[ COMBINER_MAX_TEXTURES_PER_MATERIAL ];
	int				m_nCombinedTextureSizes[ COMBINER_MAX_TEXTURES_PER_MATERIAL ];
	KeyValues		*m_pCombinedMaterial;
	char			m_szCombinedMaterialName[ MAX_PATH ];
} TAtlasGroup;


typedef struct SCombinedStudioData
{
	studiodata_t						*m_pPlaceholderStudioData;
	MDLHandle_t							m_PlaceholderHandle;
	studiodata_t						*m_pFinalStudioData;
	MDLHandle_t							m_FinalHandle;
	void								*m_pCombinedUserData;
	unsigned int						m_nReferenceFlags;
	CombinedModelLoadedCallback			m_CallbackFunc;
	CModelCombine						*m_pCombineData;

	SCombinerModelInput_t				m_ModelInputData[ COMBINER_MAX_MODELS ];
	int									m_nNumModels;

	int									m_nModelMaterialCounts[ COMBINER_MAX_MODELS ];
	int									m_nModelMaterialIndices[ COMBINER_MAX_MODELS ][ COMBINER_MAX_MATERIALS ];

	int									m_MeshToMaterialMap[ COMBINER_MAX_MODELS ][ COMBINER_MAX_BODYPARTS_PER_MODEL ][ COMBINER_MAX_MATERIALS_PER_INPUT_MODEL ];
	
	TAtlasGroup							m_AtlasGroups[ COMBINER_MAX_ATLAS_GROUPS ];
	int									m_nNumAtlasGroups;

	char								m_szNonAtlasedMaterialPaths[ COMBINER_MAX_MODELS ][ MAX_PATH ];
	int									m_nNumNonAtlasedMaterialPaths;

	KeyValues							*m_pNonAtlasedMaterialKVs[ COMBINER_MAX_MODELS ];
	char								m_szNonAtlasedMaterialBaseName[ COMBINER_MAX_MODELS ][ MAX_PATH ];
	int									m_nNumNonAtlasedMaterialBaseNames;

	char								m_szCombinedModelName[ MAX_PATH ];

	// returned results
	TCombinedResults					m_Results;

} TCombinedStudioData;


enum
{
	WRITE_AREA_MDL = 0,
	WRITE_AREA_VTX,
	WRITE_AREA_VVD,
	WRITE_AREA_VTF,

	MAX_WRITE_AREAS
};


class CCombinerMemoryWriter
{
public:
				CCombinerMemoryWriter( );
				~CCombinerMemoryWriter( );

	void		Init( );

	void		InitWriteArea( int nArea, char *pPosition );
	void		SetWriteArea( int nArea );

	char		*AllocWrite( int nSize );
	char		*WriteOffset( int &nOffsetIndex );
	char		*WriteOffset( int &nOffsetIndex, void *pBasePtr );
	char		*WriteOffset( short &nOffsetIndex, void *pBasePtr );
	char		*WriteBuffer( const void *pData, int nSize );
	char		*WriteBufferWithOffset( const void *pData, int nSize, int &nOffsetIndex );
	char		*WriteString( const char *pszString );	// adds NULL terminator
	char		*WriteText( const char *pszString );	// does not add NULL terminator
	void		AlignWrite( int nAlignSize );

	char		*GetSaveWritePos( int nArea ) { return m_pSaveWritePos[ nArea ]; }
	char		*GetWritePos( ) { return m_pWritePos; }
	char		*GetWriteArea( ) { return m_pWriteArea; }

private:
	char		*m_pWorkBuffer;
	char		*m_pEndBuffer;
	char		*m_pWriteArea;
	char		*m_pSaveWriteArea[ MAX_WRITE_AREAS ];
	char		*m_pWritePos;
	char		*m_pSaveWritePos[ MAX_WRITE_AREAS ];
	int			m_nWriteArea;
#ifdef DEBUG_COMBINE
	char		*m_pErrorPos;
#endif

};


extern CCombinerMemoryWriter	g_CombinerWriter;


class CModelCombine
{
public:
			CModelCombine( );
			~CModelCombine( );

	void	Init( TCombinedStudioData *pCombinedStudioData );

	bool	Resolve( );

	void	*GetCombinedMDLPtr( ) { return ( void * )m_pCombinedStudioHdr; }
	int		GetCombinedMDLSize( ) { return g_CombinerWriter.GetSaveWritePos( WRITE_AREA_MDL ) - ( char *)m_pCombinedStudioHdr; }
	bool	GetCombinedMDLAvailability( ) { return GetCombinedMDLSize() != 0; }

	void	*GetCombinedVTXPtr( ) { return ( void * )m_pCombinedHardwareHeader; }
	int		GetCombinedVTXSize( ) { return g_CombinerWriter.GetSaveWritePos(  WRITE_AREA_VTX ) - ( char *)m_pCombinedHardwareHeader; }
	bool	GetCombinedVTXAvailability( ) { return GetCombinedVTXSize() != 0; }

	void	*GetCombinedVVDPtr( ) { return ( void * )m_pCombinedVertex; }
	int		GetCombinedVVDSize( ) { return g_CombinerWriter.GetSaveWritePos( WRITE_AREA_VVD ) - ( char *)m_pCombinedVertex; }
	bool	GetCombinedVVDAvailability( ) { return GetCombinedVVDSize() != 0; }

	TCombinedResults	*GetResults( ) { return &m_pCombinedStudioData->m_Results; }

	static int GetNextAssetID( ) { return ++m_nNextAssetID; }

private:
	static int	BoneNameCompare( const void *elem1, const void *elem2 );

	void		BeginStringTable( );
	void		AddToStringTable( void *base, int *ptr, const char *string );
	void		WriteStringTable( );

	void		VerifyField( int nField, const char *pszDescription );
	void		VerifyField2( int nField, const char *pszDescription );
	void		VerifyOffset( void *pPtr, const char *pszDescription, void *pWritePos = 0 );

	void		DetermineMasterBoneList( );

	//
	void		CombineMDL_PreintStrings( );

	void		RemapBone( int nModel, int &nBone ) { nBone = m_nBoneRemap[ nModel ][ nBone ]; }

	void		WriteBoneProc( int nSize, int nType1, int nType2 = -999 );
	void		WriteBoneQuatInterp( );
	void		WriteBoneTwist( );
	void		WriteBoneConstraints( );
	void		WriteBoneAttachments( );
	void		WriteHitBoxes( );
	void		WriteBoneTable( );
	void		CombineMDL_Bones( );
	
	void		WriteAnimation( mstudioanimdesc_t *pOrigAnim, void *pAnimData, int nFrameSize );
	void		CombineMDL_Anims( );

	void		CombineMDL_SequenceInfo( );

	void		WriteModel( int nModel );
	void		CombineMDL_Model( );

	void		CombineMDL_Textures( );

	void		CombineMDL_KeyValues( );

	void		CombineMDL_BoneTransforms( );

	void		CombineMDL_BoneFlexDrivers( );

	void		CombineMDL_AssignMeshIDs( );

	void		CombineMDL( bool bNoStringTable );
#ifdef DEBUG_COMBINE
	void		TestCombineMDL( );
#endif // DEBUG_COMBINE

	void		CalcVTXInfo();

	void		WriteStrip( OptimizedModel::StripGroupHeader_t *pNewStripGroup, int nModel, OptimizedModel::StripGroupHeader_t *pOrigStripGroup, OptimizedModel::StripHeader_t *pOrigStrip );
	void		MergeStripGroup( int nLOD, OptimizedModel::StripGroupHeader_t *pNewStripGroup, int nModel, OptimizedModel::StripGroupHeader_t	*pOrigStripGroup );
	void		WriteStripGroup( int nLOD, int nMaterialIndex, unsigned char nStripGroupFlags, OptimizedModel::MeshHeader_t *pNewMesh );
	void		WriteMeshes( int nLOD, int nMaterialIndex, OptimizedModel::ModelLODHeader_t *pNewModelLOD, OptimizedModel::ModelLODHeader_t *pOrigModelLOD );
	void		WriteModelLOD( int nLOD, OptimizedModel::ModelHeader_t *pNewModel, OptimizedModel::ModelHeader_t *pOrigModel );
	void		WriteModel( int nModel, OptimizedModel::BodyPartHeader_t *pNewBodyPart, OptimizedModel::BodyPartHeader_t *pOrigBodyPart );
	void		WriteBodyPart( int nBodyPart );
	void		CombineVTX( );
#ifdef DEBUG_COMBINE
	void		TestCombineVTX( );
#endif // DEBUG_COMBINE

	//
	void		CombineVVD_OffsetVerts( );
	void		CombineVVD( );

	// 
	void		CombineTextures( );
	int			AddMaterialToTextureCombiner( int nTextureIndex, int nModel, int nModelMaterialIndex );

	void		Test( );

	typedef struct SHardwareData
	{
		int m_nBodyParts;
		int m_nMaxBodyParts;
		int m_nModels;
		int m_nModelLODs;
		int m_nMeshes;
		int m_nStripGroups;
		int m_nStrips;
		int m_nVerts;
		int m_nIndices;
		int m_nBoneStateChanges;
		int m_nStringTable;
		int m_nTopology;
		int m_nMaterialReplacements;
	} THardwareData;

	TCombinedStudioData	*m_pCombinedStudioData;

	CUtlBuffer	*MDL_Data[ COMBINER_MAX_MODELS ];
	CUtlBuffer	*VTX_Data[ COMBINER_MAX_MODELS ];
	CUtlBuffer	*VVD_Data[ COMBINER_MAX_MODELS ];

	studiohdr_t			*m_pStudioHdr[ COMBINER_MAX_MODELS ];
	studiohdr2_t		*m_pStudioHdr2[ COMBINER_MAX_MODELS ];
	vertexFileHeader_t	*m_pVertexFileHeader[ COMBINER_MAX_MODELS ];

	const mstudiobone_t	*m_pMasterBoneList[ COMBINER_MAX_BONES ];
	int				m_nBoneModelOwner[ COMBINER_MAX_BONES ];
	int				m_nNumMasterBones;
	int				m_nBoneRemap[ COMBINER_MAX_MODELS ][ COMBINER_MAX_BONES ];
	int				m_nMasterToLocalBoneRemap[ COMBINER_MAX_MODELS ][ COMBINER_MAX_BONES ];
	mstudiomodel_t	*m_pMasterModels[ COMBINER_MAX_MODELS ][ COMBINER_MAX_SUB_MODELS ];
	mstudiomodel_t	*m_pMasterFlexModels[ COMBINER_MAX_SUB_MODELS ];
	int				*m_nVertexRemap[ COMBINER_MAX_MODELS ];
	int				m_nFlexModelSource;
	THardwareData	m_MaxHardwareData;
	THardwareData	m_CurrentHardwareData;
	THardwareData	m_HardwareOffsets;

	studiohdr_t						*m_pCombinedStudioHdr;
	studiohdr2_t					*m_pCombinedStudioHdr2;
	OptimizedModel::FileHeader_t	*m_pCombinedHardwareHeader;
	mstudiomodel_t					*m_pCombinedModels;
	vertexFileHeader_t				*m_pCombinedVertex;

	struct stringtable_t
	{
		byte		*base;
		int			*ptr;
		const char	*string;
		int			dupindex;
		byte		*addr;
	};
	int				numStrings;
	stringtable_t	strings[ COMBINER_MAX_STRINGS ];

	static unsigned int m_nNextAssetID;
};


extern CModelCombine			g_ModelCombiner;


#endif // __MDLCOMBINE_H
