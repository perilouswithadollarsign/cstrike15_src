//========== Copyright © Valve Corporation, All rights reserved. ========
#if !defined( VJOBS_JOBPARAMS_SHARED_HDR ) && defined( _PS3 )
#define VJOBS_JOBPARAMS_SHARED_HDR


#include "ps3/spu_job_shared.h"





// these structure belong in its own headers in public/vjobs, but they're small and I don't want to pollute public with such trivialities
namespace job_ctxflush
{
	struct JobParams_t
	{
		uint32 m_nUsefulCmdBytes;
		uint32 m_nNewPcbringEnd;
	};
	inline JobParams_t * GetJobParams( void *pJob )
	{
		return VjobGetJobParams< JobParams_t, CellSpursJob128 >( pJob );
	}	
}

namespace job_gcmstateflush
{
	typedef CellSpursJob128 JobDescriptor_t;
	struct JobParams_t
	{
		uint32 m_nSpuDrawQueueSignal;
		uint16 m_nSizeofDrawQueueUploadWords; // this may be unaligned, and it counts bytes from the unaligned start
		uint16 m_nSkipDrawQueueWords;
	};
	inline JobParams_t * GetJobParams( void *pJob )
	{
		return VjobGetJobParams< JobParams_t, JobDescriptor_t >( pJob );
	}	
}



namespace job_hello
{
	struct ALIGN128 Exchange_t
	{
		uint32 m_numSpusJoined;
		uint32 m_nStage;
		uint64 m_nIncrementer[2];
	} ALIGN128_POST;
}

namespace job_zpass
{
	typedef CellSpursJob128 JobDescriptor_t;
	
	enum ConstEnum_t
	{
		PHASE_ZPREPASS,
		PHASE_RENDER,
		PHASE_END
	};
	
	struct JobParams_t
	{
		uint8 m_nPhase;
		uint8 m_nDebuggerBreak;
	};

	inline JobParams_t * GetJobParams( void *pJob )
	{
		return VjobGetJobParams< JobParams_t, JobDescriptor_t >( pJob );
	}	
}

struct CellMP3Context;


// Edge Zlib compression job
namespace job_zlibdeflate
{
	typedef CellSpursJob128 JobDescriptor_t;

	struct JobParams_t
	{
		uint16 IsDone()const { return *(volatile uint16*)&m_nStatus; }

		void * m_eaOutputCompressedData;
		uint32 m_nMaxCompressedOutputSize;
		void * m_eaInputUncompressedData;
		uint32 m_nUncompressedSize;

		uint32 m_nError;

		//  0 : compressed data was larger than uncompressed or compression error, store uncompressed
		// the MSB is set when data is compressed
		uint32  m_nCompressedSizeOut; 

		uint16 m_nStatus; // will be non-0 when the job is done
		uint16 m_nDebuggerBreak;
	};


	inline JobParams_t * GetJobParams( void *pJob )
	{
		return VjobGetJobParams< JobParams_t, JobDescriptor_t >( pJob );
	}	
}


// Edge Zlib decompression job
namespace job_zlibinflate
{
	typedef CellSpursJob128 JobDescriptor_t;

	struct JobParams_t
	{
		uint16 IsDone()const { return *(volatile uint16*)&m_nStatus; }

		void *m_eaUncompressedOutput;
		uint32 m_nExpectedUncompressedSize;
		void *m_eaCompressed;
		uint32 m_nCompressedSize;

		// 0 : decompressed without error
		uint32 m_nError;

		uint16 m_nStatus; // will be non-0 when the job is done
		uint16 m_nDebuggerBreak;
	};

	inline JobParams_t * GetJobParams( void *pJob )
	{
		return VjobGetJobParams< JobParams_t, JobDescriptor_t >( pJob );
	}	
}

namespace job_edgemlaa
{
	typedef CellSpursJob128 JobDescriptor_t;

	struct JobParams_t
	{
		uint32 m_nDebuggerBreakMask;
		uint32 *m_eaJts; // patch this with RETURN
	};

	inline JobParams_t * GetJobParams( void *pJob )
	{
		return VjobGetJobParams< JobParams_t, JobDescriptor_t >( pJob );
	}	
}


namespace job_buildindices
{
	typedef CellSpursJob128 JobDescriptor_t;

	struct JobParams_t
	{
		int				m_testInt_IN;
		int				m_testInt_OUT;
	};

	inline JobParams_t * GetJobParams( void *pJob )
	{
		return VjobGetJobParams< JobParams_t, JobDescriptor_t >( pJob );
	}	
}


namespace job_buildrenderables
{
	typedef CellSpursJob128 JobDescriptor_t;

	struct JobParams_t
	{
		int				m_testInt_IN;
		int				m_testInt_OUT;
	};

	inline JobParams_t * GetJobParams( void *pJob )
	{
		return VjobGetJobParams< JobParams_t, JobDescriptor_t >( pJob );
	}	
}


namespace job_buildworldlists
{
	typedef CellSpursJob256 JobDescriptor_t;

	struct JobParams_t
	{
		uint32				m_nDebugBreak;

		uint32				m_eaWorldNodes;
		int					m_visframecount;

		uint32				m_pSurfaces2;
		uint32				m_pmarksurfaces;
		uint32 				m_pLeafs;

		float				m_ModelOrg[4];
		bool				m_bViewerInSolidSpace;

		uint32				m_Disp_ParentSurfID_offset;
		uint32				m_Disp_BB_offset;
		uint32				m_Disp_Info_Size;

		uint32				m_pDispInfos;

		uint32				m_eaFrustum;
		uint32				m_nAreaFrustum;
		uint32				m_eaAreaFrustum;
		uint32				m_eaRenderAreaBits;

		uint32				m_eaDispInfoReferences;

		uint32				m_nMaxVisitSurfaces;
		uint32				m_nNumSortID;

		bool				m_bShadowDepth;

		bool				m_bDrawTopView;
		bool				m_bTopViewNoBackfaceCulling;
		bool				m_bTopViewNoVisCheck;
		uint32				m_eaVolumeCuller;
		float				m_orthoCenter[2];
		float				m_orthoHalfDi[2];

		int					m_DrawFlags;
		int					m_buildViewID;

		// inout
		uint32				m_eaInfo;
		uint32				m_eaRenderListLeaves;

		// addr of output DMA structure
		uint32				m_eaDMAOut;

		// offset for CUtlVector Count
		uint32				m_nUtlCountOffset;
	};

	struct ALIGN128 buildWorldListsDMAOut
	{
		// m_SortList
		uint32				m_pSortList_m_list;
		uint32				m_pSortList_m_groupsShared;
		uint32				m_pSortList_m_groupIndices;
		uint32				m_pSortList_m_sortGroupLists[4];
		uint32				m_pSortList_m_listUtlPtr;
		uint32				m_pSortList_m_groupsSharedUtlPtr;
		uint32				m_pSortList_m_groupIndicesUtlPtr;
		uint32				m_pSortList_m_sortGroupListsUtlPtr[4];

		// m_DispSortList
		uint32				m_pDispSortList_m_list;
		uint32				m_pDispSortList_m_groupsShared;
		uint32				m_pDispSortList_m_groupIndices;
		uint32				m_pDispSortList_m_sortGroupLists[4];
		uint32				m_pDispSortList_m_listUtlPtr;
		uint32				m_pDispSortList_m_groupsSharedUtlPtr;
		uint32				m_pDispSortList_m_groupIndicesUtlPtr;
		uint32				m_pDispSortList_m_sortGroupListsUtlPtr[4];

		// m_AlphaSurfaces
		uint32				m_pAlphaSurfaces;
		uint32				m_pAlphaSurfacesUtlPtr;

		// m_DlightSurfaces
		uint32				m_pDlightSurfaces[4];
		uint32				m_pDlightSurfacesUtlPtr[4];

		// m_PaintedSurfaces
		uint32				m_pPaintedSurfaces[4];
		uint32				m_pPaintedSurfacesUtlPtr[4];

		// m_leaves
		uint32				m_pLeaves;
		uint32				m_pLeavesUtlPtr;

		// m_VisitedSurfs
		uint32				m_pVisitedSurfs;

		// decal surf list
		uint32				m_pDecalSurfsToAdd;
		uint32				m_pDecalSurfsToAddUtlPtr;

		// m_bSkyVisible
		uint32				m_pSkyVisible;

		// m_bWaterVisible
		uint32				m_pWaterVisible;

	} ALIGN128_POST;

	struct decalSurfPair
	{
		uint32				m_surfID;
		int					m_renderGroup;
	};

	inline JobParams_t * GetJobParams( void *pJob )
	{
		return VjobGetJobParams< JobParams_t, JobDescriptor_t >( pJob );
	}	
}


#endif
