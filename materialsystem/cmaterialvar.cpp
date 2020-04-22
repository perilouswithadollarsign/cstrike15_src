//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#if defined( _WIN32 ) && !defined( _X360 )
#define WIN_32_LEAN_AND_MEAN
#include <windows.h>
#define VA_COMMIT_FLAGS MEM_COMMIT
#define VA_RESERVE_FLAGS MEM_RESERVE
#elif defined( _X360 )
#undef Verify
#define VA_COMMIT_FLAGS (MEM_COMMIT|MEM_NOZERO|MEM_LARGE_PAGES)
#define VA_RESERVE_FLAGS (MEM_RESERVE|MEM_LARGE_PAGES)
#endif

#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialproxyfactory.h"
#include <string.h>
#include "materialsystem_global.h"
#include <stdlib.h>
#include "shaderapi/ishaderapi.h"
#include "imaterialinternal.h"
#include "utlsymbol.h"
#include "mempool.h"
#include "itextureinternal.h"
#include "tier0/dbg.h"
#include "tier0/vprof.h"
#include "tier1/callqueue.h"
#include "mathlib/vmatrix.h"
#include "tier1/strtools.h"
#include "tier1/keyvalues.h"
#include "tier1/mempool.h"
#include "texturemanager.h"
#include "cdll_int.h"
#include "tier1/utlmap.h"

#define MATERIALVAR_CHAR_BUF_SIZE 512

#if !defined( _X360 )
#pragma pack (1)
#endif

//-----------------------------------------------------------------------------
// Material var custom allocator
//-----------------------------------------------------------------------------
#ifdef _X360
#define USE_MV_POOL
#endif

#ifdef USE_MV_POOL

#define SIZEOF_MATVAR 40
#define MAX_MATVARS ( 256*1024 )
#define MV_PAGE_SIZE (64*1024)
#define MV_NUM_PER_PAGE ( (64*1024) / SIZEOF_MATVAR )
#define MV_SIZE_BUFFER ((MAX_MATVARS/MV_NUM_PER_PAGE)*MV_PAGE_SIZE)
#define MV_NUM_PAGES ( MV_SIZE_BUFFER/MV_PAGE_SIZE )
#define MV_NOT_COMMITTED ( (MVFreeBlock_t *)~0 )

byte *g_pMVData = (byte *)VirtualAlloc( NULL, MV_NUM_PAGES * MV_PAGE_SIZE, VA_RESERVE_FLAGS, PAGE_NOACCESS );
CThreadFastMutex g_MVDataMutex; // we never normally alloc & free during levels, and there's not much contention, so a mutex is fine

struct MVFreeBlock_t
{
	MVFreeBlock_t *pNext;
	byte pad[SIZEOF_MATVAR - sizeof(MVFreeBlock_t *)];
};

struct MVFreeHeader_t
{
	MVFreeHeader_t() : pHead(MV_NOT_COMMITTED), nBlocks(MV_NUM_PER_PAGE) {}
	MVFreeBlock_t *pHead;
	int nBlocks;
};

MVFreeHeader_t g_MVFreeLists[MV_NUM_PAGES];
int g_nMVAllocated;

void DumpAllMaterialVars( void );

void MVDumpHeap()
{
	AUTO_LOCK( g_MVDataMutex );

	Msg( "MaterialVar heap:\n" );
	int sum = 0, pages = 0;
	for ( int i = 0; i < MV_NUM_PAGES; i++ )
	{
		sum += MV_NUM_PER_PAGE - g_MVFreeLists[i].nBlocks;
		Msg( "    Page %3d: %d vars%s\n", i, MV_NUM_PER_PAGE - g_MVFreeLists[i].nBlocks, ( g_MVFreeLists[i].pHead == MV_NOT_COMMITTED ) ? " (decommitted)" : ""  );
		if ( g_MVFreeLists[i].pHead != MV_NOT_COMMITTED )
			pages++;
	}
	if ( sum != g_nMVAllocated )
	{
		Msg( "\n************************** MaterialVar heap inconsistency! **************************\n\n" );
	}
	Msg( "Total of %d materialvars (%d logical bytes, %d actual bytes\n", g_nMVAllocated, g_nMVAllocated * SIZEOF_MATVAR, pages * MV_PAGE_SIZE );

	// Now dump individual materialvars, grouped by owning material:
	DumpAllMaterialVars();
}

bool MVInternallyAllocated( void * p )
{
	return ( p >= g_pMVData && p < g_pMVData + MV_NUM_PAGES * MV_PAGE_SIZE );
}

MVFreeBlock_t *MVCommitAlloc( int iPage )
{
	MVFreeBlock_t *pCur, *pLimit;
	MVFreeBlock_t *pBlock = (MVFreeBlock_t *)VirtualAlloc( g_pMVData + ( iPage * MV_PAGE_SIZE ), MV_PAGE_SIZE, VA_COMMIT_FLAGS, PAGE_READWRITE );
	pCur = g_MVFreeLists[iPage].pHead = pBlock + 1;
	pLimit = pBlock + ( MV_NUM_PER_PAGE - 1 );

	while ( pCur < pLimit )
	{
		pCur->pNext = pCur + 1;
		pCur++;
	}

	pLimit->pNext = NULL;
	g_MVFreeLists[iPage].nBlocks = ( MV_NUM_PER_PAGE - 1 );
	g_nMVAllocated++;
	return pBlock;
}

int MVFindBestPage()
{
	AUTO_LOCK( g_MVDataMutex );
	int i;
	int iFirstUncommitted = -1;
	int iBestPage = -1;
#ifdef MV_MOST_FULL
	// Most full page
	int nInMostFull = INT_MAX;
	for ( i = 0; i < MV_NUM_PAGES; i++ )
	{
		if ( g_MVFreeLists[i].pHead != MV_NOT_COMMITTED )
		{
			if ( g_MVFreeLists[i].nBlocks && g_MVFreeLists[i].nBlocks < nInMostFull )
			{
				iBestPage = i;
				nInMostFull = g_MVFreeLists[i].nBlocks;
			}
		}
		else if ( iFirstUncommitted == -1 )
		{
			iFirstUncommitted = i;
		}
	}
#else
	// Lowest page
	for ( i = 0; i < MV_NUM_PAGES; i++ )
	{
		if ( g_MVFreeLists[i].pHead != MV_NOT_COMMITTED )
		{
			if ( g_MVFreeLists[i].nBlocks )
			{
				iBestPage = i;
				break;
			}
		}
		else if ( iFirstUncommitted == -1 )
		{
			iFirstUncommitted = i;
		}
	}
#endif

	if ( iBestPage != -1 )
	{
		return iBestPage;
	}

	return iFirstUncommitted;
}

void *MVAlloc()
{
	AUTO_LOCK( g_MVDataMutex );
	int iBestPage = MVFindBestPage();

	if ( iBestPage != -1 )
	{
		MVFreeBlock_t *pBlock = NULL;
		if ( g_MVFreeLists[iBestPage].pHead != MV_NOT_COMMITTED )
		{
			pBlock = g_MVFreeLists[iBestPage].pHead;
			g_MVFreeLists[iBestPage].pHead = pBlock->pNext;
			g_MVFreeLists[iBestPage].nBlocks--;
			g_nMVAllocated++;
		}
		else
		{
			pBlock = MVCommitAlloc( iBestPage );
		}
		return pBlock;
	}
	return malloc( SIZEOF_MATVAR );
}

void MVFree( void *p )
{
	if ( MVInternallyAllocated( p ) )
	{
		AUTO_LOCK( g_MVDataMutex );

		MVFreeBlock_t *pBlock = (MVFreeBlock_t *)p;
		int iPage = ( ( (byte *)pBlock - g_pMVData ) / MV_PAGE_SIZE );

		pBlock->pNext = g_MVFreeLists[iPage].pHead;
		g_MVFreeLists[iPage].pHead = pBlock;
		g_MVFreeLists[iPage].nBlocks++;
		g_nMVAllocated--;
	}
	else
	{
		free( p );
	}
}

void *MVRelocate( void *p )
{
	AUTO_LOCK( g_MVDataMutex );

	MVFreeBlock_t *pBlockOld = (MVFreeBlock_t *)p;
	MVFreeBlock_t *pBlockNew = pBlockOld;
	int iCurPage = ( ( (byte *)pBlockOld - g_pMVData ) / MV_PAGE_SIZE );

#ifdef MV_MOST_FULL
	if ( g_MVFreeLists[iCurPage].nBlocks == 0 )
	{
		return p;
	}
#else
	if ( iCurPage <= g_nMVAllocated/MV_NUM_PER_PAGE )
	{
		return p;
	}
#endif

	int iBestPage = MVFindBestPage();
	bool bMove;
#ifdef MV_MOST_FULL
	bMove = ( iBestPage != iCurPage );
#else
	bMove = ( iBestPage <= iCurPage  );
#endif
	if ( bMove )
	{
		if ( g_MVFreeLists[iBestPage].pHead != MV_NOT_COMMITTED )
		{
			pBlockNew = g_MVFreeLists[iBestPage].pHead;
			g_MVFreeLists[iBestPage].pHead = pBlockNew->pNext;
			g_MVFreeLists[iBestPage].nBlocks--;
			g_nMVAllocated++;
		}
		else
		{
			pBlockNew = MVCommitAlloc( iBestPage );
		}
	}

	if ( pBlockNew != pBlockOld )
	{
		memcpy( pBlockNew, pBlockOld, SIZEOF_MATVAR );
		MVFree( pBlockOld );
	}

	return pBlockNew;
}

void MVDecommitUnusedPages()
{
	AUTO_LOCK( g_MVDataMutex );

	for ( int i = 0; i < MV_NUM_PAGES; i++ )
	{
		if ( g_MVFreeLists[i].pHead != MV_NOT_COMMITTED && g_MVFreeLists[i].nBlocks == MV_NUM_PER_PAGE )
		{
			VirtualFree( g_pMVData + ( i * MV_PAGE_SIZE ), MV_PAGE_SIZE, MEM_DECOMMIT );
			g_MVFreeLists[i].pHead = MV_NOT_COMMITTED;
		}
	}
}

CON_COMMAND( mv_decommit, "" )
{
	MVDecommitUnusedPages();
}

CON_COMMAND( mv_status, "" )
{
	MVDumpHeap();
}


#else

void *MVRelocate( void *p )
{
	return p;
}

void MVDecommitUnusedPages()
{
}

bool MVInternallyAllocated( void * p )
{
	return false;
}

void MVDumpHeap()
{
}

#endif

//-----------------------------------------------------------------------------

struct MaterialVarMatrix_t
{
	VMatrix m_Matrix;
	bool m_bIsIdent;
};

class CMaterialVar : public IMaterialVar
{
public:
	// stuff from IMaterialVar
	virtual const char *		GetName( void ) const;
	virtual MaterialVarSym_t	GetNameAsSymbol() const;
	virtual void				SetFloatValue( float val );
	virtual void				SetIntValue( int val );
	virtual void				SetStringValue( const char *val );
	virtual const char *		GetStringValue( void ) const;
	virtual void				SetMatrixValue( VMatrix const& matrix );
	virtual VMatrix const&		GetMatrixValue( );
	virtual bool				MatrixIsIdentity( void ) const;
	virtual void				SetVecValue( const float* pVal, int numComps );
	virtual void				SetVecValue( float x, float y );
	virtual void				SetVecValue( float x, float y, float z );
	virtual void				SetVecValue( float x, float y, float z, float w );
	void						SetVecValueInternal( const Vector4D &vec, int nComps );
	virtual void				SetVecComponentValue( float fVal, int nComponent );
	virtual void				GetLinearVecValue( float *val, int numComps ) const;
	virtual void				SetFourCCValue( FourCC type, void *pData );
	virtual void				GetFourCCValue( FourCC *type, void **ppData );
	virtual int					GetIntValueInternal( void ) const;
	virtual float				GetFloatValueInternal( void ) const;
	virtual float const*		GetVecValueInternal( ) const;
	virtual void				GetVecValueInternal( float *val, int numcomps ) const;
	virtual int					VectorSizeInternal() const;

	// revisit: is this a good interface for textures?

	virtual ITexture *			GetTextureValue( void );
	virtual bool				IsTextureValueInternalEnvCubemap( void ) const;
	virtual void				SetTextureValue( ITexture * );
	virtual IMaterial *			GetMaterialValue( void );
	virtual void				SetMaterialValue( IMaterial * );

	virtual 					operator ITexture *() { return GetTextureValue(); }
	virtual bool				IsDefined() const;
	virtual void				SetUndefined();

	virtual void				CopyFrom( IMaterialVar *pMaterialVar );

	 void Init( void )
	{
		m_nNumVectorComps = 4;
		m_VecVal.Init();
		m_pStringVal = NULL;
		m_intVal = 0;
		m_nTempIndex = 0xFF;
		m_bFakeMaterialVar = false;
		m_Type = MATERIAL_VAR_TYPE_INT;
	}

	// stuff that is only visible inside of the material system
	CMaterialVar();
	CMaterialVar( IMaterial* pMaterial, const char *key, VMatrix const& matrix );
	CMaterialVar( IMaterial* pMaterial, const char *key, const char *val );
	CMaterialVar( IMaterial* pMaterial, const char *key, float* pVal, int numcomps );
	CMaterialVar( IMaterial* pMaterial, const char *key, float val );
	CMaterialVar( IMaterial* pMaterial, const char *key, int val );
	CMaterialVar( IMaterial* pMaterial, const char *key );
	virtual ~CMaterialVar();

	virtual void			SetValueAutodetectType( const char *val );

	virtual IMaterial *		GetOwningMaterial() { return m_pMaterial; }

#ifdef USE_MV_POOL
	// Fixed-size allocator
	inline void* operator new( size_t size ) { return MVAlloc(); } 
	inline void* operator new( size_t size, int nBlockUse, const char *pFileName, int nLine ) { return MVAlloc(); }
	inline void  operator delete( void* p ) { MVFree(p); }
	inline void  operator delete( void* p, int nBlockUse, const char *pFileName, int nLine ) { MVFree(p); }
#else
	DECLARE_FIXEDSIZE_ALLOCATOR( CMaterialVar );
#endif

private:
	// Cleans up material var data
	void CleanUpData();

	// NOTE: Dummy vars have no backlink so we have to check the pointer here
	void VarChanged();
#ifndef NO_TOOLFRAMEWORK
	void RecordToolMessage();
#endif

	// class data
	static char s_CharBuf[MATERIALVAR_CHAR_BUF_SIZE];
	static ITextureInternal *m_dummyTexture;

	// Owning material....
	IMaterialInternal* m_pMaterial;

	// Only using one of these at a time...
	struct FourCC_t
	{
		FourCC	m_FourCC;
		void	*m_pFourCCData;
	};

	FourCC_t *AllocFourCC();

	union
	{
		IMaterialInternal* m_pMaterialValue;
		ITextureInternal *m_pTexture;
		MaterialVarMatrix_t* m_pMatrix;
		FourCC_t *m_pFourCC;
	};
};

// Has to exist *after* fixed size allocator declaration
#include "tier0/memdbgon.h"

typedef CMaterialVar *CMaterialVarPtr;

#ifndef USE_MV_POOL
DEFINE_FIXEDSIZE_ALLOCATOR( CMaterialVar, 1024, true );
#endif

// Stores symbols for the material vars
static CUtlSymbolTableMT s_MaterialVarSymbols( 0, 32, true );

static bool g_bDeleteUnreferencedTexturesEnabled = false;


//-----------------------------------------------------------------------------
// Used to make GetIntValue thread safe from within proxy calls
//-----------------------------------------------------------------------------
static CMaterialVar s_pTempMaterialVar[254];
static MaterialVarMatrix_t s_pTempMatrix[254];
static bool s_bEnableThreadedAccess = false;
static int s_nTempVarsUsed = 0;


//-----------------------------------------------------------------------------
// Dump materialvars
//-----------------------------------------------------------------------------
#ifdef USE_MV_POOL
bool MaterialVarLessFunc( IMaterial *const &a, IMaterial *const &b )
{
	if ( a == b ) return 0;
	return ( V_stricmp( (a ? a->GetName() : "[UNKNOWN]"), (b ? b->GetName() : "[UNKNOWN]") ) < 0 );
}
void DumpAllMaterialVars( void )
{
	CUtlMap< IMaterial*, int > matcounts( MaterialVarLessFunc );
	int numvars = 0;
	for ( int page = 0; page < MV_NUM_PAGES; page++ )
	{
		if ( g_MVFreeLists[page].pHead == MV_NOT_COMMITTED )
			continue;

		MVFreeBlock_t *block = (MVFreeBlock_t *)(g_pMVData + ( page * MV_PAGE_SIZE ));
		for ( int i = 0; i < MV_NUM_PER_PAGE; i++ )
		{
			if ( block->pNext && !MVInternallyAllocated( block->pNext ) )\
			{
				// not in the freelist
				CMaterialVar *var = (CMaterialVar *)block;
				IMaterial *mat = var->GetOwningMaterial();
				//Msg( " -- %40s | %s\n", (mat?mat->GetName():"[UNKNOWN]"), var->GetName() );
				int idx = matcounts.Find( mat );
				if ( idx == matcounts.InvalidIndex() )
					matcounts.Insert( mat, 1 );
				else
					matcounts.Element(idx)++;
				numvars++;
			}
			block++;
		}
	}
	//Msg("\n ==== Total %d materialvars:\n", numvars );
	for ( int j = matcounts.FirstInorder(); j != matcounts.InvalidIndex(); j = matcounts.NextInorder(j) )
	{
		IMaterial *mat = matcounts.Key(j);
		Msg( " -- %4d %s\n", matcounts.Element(j), (mat?mat->GetName():"[UNKNOWN]") );
	}
	Msg("\n ==== Total %d materialvars in %d materials\n", numvars, matcounts.Count() );
}
#endif // USE_MV_POOL


//-----------------------------------------------------------------------------
// Global methods related to material vars
//-----------------------------------------------------------------------------
void EnableThreadedMaterialVarAccess( bool bEnable, IMaterialVar **ppParams, int nVarCount )
{
	if ( s_bEnableThreadedAccess == bEnable	)
		return;

	s_bEnableThreadedAccess = bEnable;
	if ( !s_bEnableThreadedAccess )
	{
		// Necessary to free up reference counts
		for ( int i = 0; i < s_nTempVarsUsed; ++i )
		{
			s_pTempMaterialVar[i].SetUndefined();
		}
		for ( int i = 0; i < nVarCount; ++i )
		{
			ppParams[i]->SetTempIndex( 0xFF );
		}
		s_nTempVarsUsed = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Static method
// Input  : enable - 
//-----------------------------------------------------------------------------
void IMaterialVar::DeleteUnreferencedTextures( bool enable )
{
	g_bDeleteUnreferencedTexturesEnabled = enable;
}

//-----------------------------------------------------------------------------
// class factory methods
//-----------------------------------------------------------------------------
IMaterialVar* IMaterialVar::Create( IMaterial* pMaterial, const char* pKey, VMatrix const& matrix )
{
	return new CMaterialVar( pMaterial, pKey, matrix );
}

IMaterialVar* IMaterialVar::Create( IMaterial* pMaterial, const char* pKey, const char* pVal )
{
	return new CMaterialVar( pMaterial, pKey, pVal );
}

IMaterialVar* IMaterialVar::Create( IMaterial* pMaterial, const char* pKey, float* pVal, int numComps )
{
	return new CMaterialVar( pMaterial, pKey, pVal, numComps );
}

IMaterialVar* IMaterialVar::Create( IMaterial* pMaterial, const char* pKey, float val )
{
	return new CMaterialVar( pMaterial, pKey, val );
}

IMaterialVar* IMaterialVar::Create( IMaterial* pMaterial, const char* pKey, int val )
{
	return new CMaterialVar( pMaterial, pKey, val );
}

IMaterialVar* IMaterialVar::Create( IMaterial* pMaterial, const char* pKey )
{
	return new CMaterialVar( pMaterial, pKey );
}

void IMaterialVar::Destroy( IMaterialVar* pVar )
{
	if (pVar)
	{
		CMaterialVar* pVarImp = static_cast<CMaterialVar*>(pVar);
		delete pVarImp;
	}
}

MaterialVarSym_t IMaterialVar::GetSymbol( const char* pName )
{
	if (!pName)
		return UTL_INVAL_SYMBOL;

	char temp[1024];
	Q_strncpy( temp, pName, sizeof( temp ) );
	Q_strlower( temp );
	return s_MaterialVarSymbols.AddString( temp );
}

MaterialVarSym_t IMaterialVar::FindSymbol( const char* pName )
{
	if (!pName)
		return UTL_INVAL_SYMBOL;

	return s_MaterialVarSymbols.Find( pName );
}

bool IMaterialVar::SymbolMatches( const char* pName, MaterialVarSym_t symbol )
{
	return !Q_stricmp( s_MaterialVarSymbols.String(symbol), pName );
}


//-----------------------------------------------------------------------------
// class globals
//-----------------------------------------------------------------------------
char CMaterialVar::s_CharBuf[MATERIALVAR_CHAR_BUF_SIZE];


//-----------------------------------------------------------------------------
// constructors
//-----------------------------------------------------------------------------
inline CMaterialVar::FourCC_t *CMaterialVar::AllocFourCC()
{
	return new FourCC_t;
}


//-----------------------------------------------------------------------------
// NOTE: This constructor is only used by the "fake" material vars
// used to get thread mode working
//-----------------------------------------------------------------------------
CMaterialVar::CMaterialVar()
{
	Init();
	m_pMaterial = NULL;
	m_bFakeMaterialVar = true;
}

//-------------------------------------

CMaterialVar::CMaterialVar( IMaterial* pMaterial, const char *pKey, VMatrix const& matrix )
{
#ifdef POOL_MATVARS
	COMPILE_TIME_ASSERT( sizeof(CMaterialVar) == SIZEOF_MATVAR );

	if ( sizeof(CMaterialVar) != SIZEOF_MATVAR )
	{
		Error( "( sizeof(CMaterialVar) != SIZEOF_MATVAR )\n" );
	}
#endif

	Init();
	Assert( pKey );

	m_pMaterial = static_cast<IMaterialInternal*>(pMaterial);
	m_Name = GetSymbol( pKey );
	Assert( m_Name != UTL_INVAL_SYMBOL );
	m_Type = MATERIAL_VAR_TYPE_MATRIX;
	m_pMatrix = new MaterialVarMatrix_t;
	Assert( m_pMatrix );
	MatrixCopy( matrix, m_pMatrix->m_Matrix );
	m_pMatrix->m_bIsIdent = matrix.IsIdentity();
	m_intVal = 0;
	m_VecVal.Init();

}

CMaterialVar::CMaterialVar( IMaterial* pMaterial, const char *pKey, const char *pVal )
{
	Init();
	Assert( pVal && pKey );

	m_pMaterial = static_cast<IMaterialInternal*>(pMaterial);
	m_Name = GetSymbol( pKey );
	Assert( m_Name != UTL_INVAL_SYMBOL );
	int len = Q_strlen( pVal ) + 1;
	m_pStringVal = new char[ len ];
	Q_strncpy( m_pStringVal, pVal, len );
	m_Type = MATERIAL_VAR_TYPE_STRING;
	m_VecVal[0] = m_VecVal[1] = m_VecVal[2] = m_VecVal[3] = atof( m_pStringVal );
	m_intVal = int( atof( m_pStringVal ) );
}

CMaterialVar::CMaterialVar( IMaterial* pMaterial, const char *pKey, float* pVal, int numComps )
{
	Init();
	Assert( pVal && pKey && (numComps <= 4) );

	m_pMaterial = static_cast<IMaterialInternal*>(pMaterial);;
	m_Name = GetSymbol( pKey );
	Assert( m_Name != UTL_INVAL_SYMBOL );
	m_Type = MATERIAL_VAR_TYPE_VECTOR;
	memcpy( m_VecVal.Base(), pVal, numComps * sizeof(float) );
	for (int i = numComps; i < 4; ++i)
		m_VecVal[i] = 0.0f;

	m_intVal = ( int ) m_VecVal[0];
	m_nNumVectorComps = numComps;
}

CMaterialVar::CMaterialVar( IMaterial* pMaterial, const char *pKey, float val )
{
	Init();
	m_pMaterial = static_cast<IMaterialInternal*>(pMaterial);
	m_Name = GetSymbol( pKey );
	Assert( m_Name != UTL_INVAL_SYMBOL );
	m_Type = MATERIAL_VAR_TYPE_FLOAT;
	m_VecVal[0] = m_VecVal[1] = m_VecVal[2] = m_VecVal[3] = val;
	m_intVal = (int) val;

}

CMaterialVar::CMaterialVar( IMaterial* pMaterial, const char *pKey, int val )
{
	Init();
	m_pMaterial = static_cast<IMaterialInternal*>(pMaterial);
	m_Name = GetSymbol( pKey );
	Assert( m_Name != UTL_INVAL_SYMBOL );
	m_Type = MATERIAL_VAR_TYPE_INT;
	m_VecVal[0] = m_VecVal[1] = m_VecVal[2] = m_VecVal[3] = (float) val;
	m_intVal = val;
}

CMaterialVar::CMaterialVar( IMaterial* pMaterial, const char *pKey )
{
	Init();
	m_pMaterial = static_cast<IMaterialInternal*>(pMaterial);
	m_Name = GetSymbol( pKey );
	Assert( m_Name != UTL_INVAL_SYMBOL );
	m_Type = MATERIAL_VAR_TYPE_UNDEFINED;
}


//-----------------------------------------------------------------------------
// destructor
//-----------------------------------------------------------------------------
CMaterialVar::~CMaterialVar()
{
	CleanUpData();
}


//-----------------------------------------------------------------------------
// Cleans up material var allocated data if necessary
//-----------------------------------------------------------------------------
void CMaterialVar::CleanUpData()
{
	switch ( m_Type )
	{
	case MATERIAL_VAR_TYPE_STRING:
		delete [] m_pStringVal;
		break;

	case MATERIAL_VAR_TYPE_TEXTURE:
		// garymcthack
		if( !IsTextureInternalEnvCubemap( m_pTexture ) )
		{
			m_pTexture->DecrementReferenceCount();
			if ( g_bDeleteUnreferencedTexturesEnabled )
			{
				m_pTexture->DeleteIfUnreferenced();
			}
		}
		break;

	case MATERIAL_VAR_TYPE_MATERIAL:
		if( m_pMaterialValue != NULL )
		{
			m_pMaterialValue->DecrementReferenceCount();
		}
		break;

	case MATERIAL_VAR_TYPE_MATRIX:
		delete m_pMatrix;
		break;

	case MATERIAL_VAR_TYPE_FOURCC:
		delete m_pFourCC;
		break;

	case MATERIAL_VAR_TYPE_VECTOR:
	case MATERIAL_VAR_TYPE_INT:
	case MATERIAL_VAR_TYPE_FLOAT:
	default:
		break;
	}
}


//-----------------------------------------------------------------------------
// name	+ type
//-----------------------------------------------------------------------------
MaterialVarSym_t CMaterialVar::GetNameAsSymbol() const
{
	return m_Name;
}

const char *CMaterialVar::GetName( ) const
{
	if( !m_Name.IsValid() )
	{
		Warning( "m_pName is NULL for CMaterialVar\n" );
		return "";
	}
	return s_MaterialVarSymbols.String( m_Name );
}


//-----------------------------------------------------------------------------
// Thread-safe versions
//-----------------------------------------------------------------------------
int	CMaterialVar::GetIntValueInternal( void ) const
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( pCallQueue && !m_bFakeMaterialVar )
	{
		if ( !s_bEnableThreadedAccess )
		{
			//DevMsg( 2, "Non-thread safe call to CMaterialVar %s!\n", GetName() );
		}

		if ( m_nTempIndex != 0xFF )
			return s_pTempMaterialVar[m_nTempIndex].GetIntValueInternal();
	}

	// Set methods for float and vector update this
	return m_intVal;
}

float CMaterialVar::GetFloatValueInternal( void ) const
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( pCallQueue && !m_bFakeMaterialVar )
	{
		if ( !s_bEnableThreadedAccess )
		{
			//DevMsg( 2, "Non-thread safe call to CMaterialVar %s!\n", GetName() );
		}

		if ( m_nTempIndex != 0xFF )
			return s_pTempMaterialVar[m_nTempIndex].GetFloatValueInternal();
	}

	return m_VecVal[0];
}

float const* CMaterialVar::GetVecValueInternal( ) const
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( pCallQueue && !m_bFakeMaterialVar )
	{
		if ( !s_bEnableThreadedAccess )
		{
			//DevMsg( 2, "Non-thread safe call to CMaterialVar %s!\n", GetName() );
		}

		if ( m_nTempIndex != 0xFF )
			return s_pTempMaterialVar[m_nTempIndex].GetVecValueInternal();
	}

	return m_VecVal.Base();
}

void CMaterialVar::GetVecValueInternal( float *val, int numcomps ) const
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( pCallQueue && !m_bFakeMaterialVar )
	{
		if ( !s_bEnableThreadedAccess )
		{
			//DevMsg( 2, "Non-thread safe call to CMaterialVar %s!\n", GetName() );
		}

		if ( m_nTempIndex != 0xFF )
		{
			s_pTempMaterialVar[m_nTempIndex].GetVecValueInternal( val, numcomps );
			return;
		}
	}

	Assert( ( numcomps >0 ) && ( numcomps <= 4 ) );
	for( int i=0 ; i < numcomps; i++ )
	{
		val[i] = m_VecVal[ i ];
	}
}

int	CMaterialVar::VectorSizeInternal() const
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( pCallQueue && !m_bFakeMaterialVar )
	{
		if ( !s_bEnableThreadedAccess )
		{
			//DevMsg( 2, "Non-thread safe call to CMaterialVar %s!\n", GetName() );
		}

		if ( m_nTempIndex != 0xFF )
			return s_pTempMaterialVar[m_nTempIndex].VectorSizeInternal( );
	}

	return m_nNumVectorComps;
}

// Don't want to be grabbing the dummy var and changing it's value.  That usually means badness.
#define ASSERT_NOT_DUMMY_VAR()	AssertMsg( m_bFakeMaterialVar || ( V_stricmp( GetName(), "$dummyvar" ) != 0 ), "TRYING TO MODIFY $dummyvar, WHICH IS BAD, MMMKAY!" )

//-----------------------------------------------------------------------------
// float
//-----------------------------------------------------------------------------
void CMaterialVar::SetFloatValue( float val )
{
	ASSERT_NOT_DUMMY_VAR();
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( !m_bFakeMaterialVar && pCallQueue )
	{
		if ( s_bEnableThreadedAccess )
		{
			if ( m_nTempIndex == 0xFF )
			{
				m_nTempIndex = s_nTempVarsUsed++;
			}
			s_pTempMaterialVar[m_nTempIndex].SetFloatValue( val );
		}
		pCallQueue->QueueCall( this, &CMaterialVar::SetFloatValue, val );
		return;
	}

	// Suppress all this if we're not actually changing anything
	if ((m_Type == MATERIAL_VAR_TYPE_FLOAT) && (m_VecVal[0] == val))
	{
#ifndef NO_TOOLFRAMEWORK
		RecordToolMessage();
#endif
		return;
	}

	CleanUpData();
	m_VecVal[0] = m_VecVal[1] = m_VecVal[2] = m_VecVal[3] = val;
	m_intVal = (int) val;
	m_Type = MATERIAL_VAR_TYPE_FLOAT;
	VarChanged();
}


//-----------------------------------------------------------------------------
// int
//-----------------------------------------------------------------------------
void CMaterialVar::SetIntValue( int val )
{
	ASSERT_NOT_DUMMY_VAR();
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( !m_bFakeMaterialVar && pCallQueue )
	{
		if ( s_bEnableThreadedAccess )
		{
			if ( m_nTempIndex == 0xFF )
			{
				m_nTempIndex = s_nTempVarsUsed++;
			}
			s_pTempMaterialVar[m_nTempIndex].SetIntValue( val );
		}
		pCallQueue->QueueCall( this, &CMaterialVar::SetIntValue, val );
		return;
	}

	// Suppress all this if we're not actually changing anything
	if ((m_Type == MATERIAL_VAR_TYPE_INT) && (m_intVal == val))
	{
#ifndef NO_TOOLFRAMEWORK
		RecordToolMessage();
#endif
		return;
	}

	CleanUpData();
	m_intVal = val;
	m_VecVal[0] = m_VecVal[1] = m_VecVal[2] = m_VecVal[3] = (float) val;
	m_Type = MATERIAL_VAR_TYPE_INT;
	VarChanged();
}


//-----------------------------------------------------------------------------
// string
//-----------------------------------------------------------------------------
const char *CMaterialVar::GetStringValue( void ) const
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( pCallQueue && !m_bFakeMaterialVar )
	{
		if ( !s_bEnableThreadedAccess )
		{
			//DevMsg( 2, "Non-thread safe call to CMaterialVar %s!\n", GetName() );
		}

		if ( m_nTempIndex != 0xFF )
			return s_pTempMaterialVar[m_nTempIndex].GetStringValue();
	}

	switch( m_Type )
	{
	case MATERIAL_VAR_TYPE_STRING:
		return m_pStringVal;

	case MATERIAL_VAR_TYPE_INT:
		Q_snprintf( s_CharBuf, sizeof( s_CharBuf ), "%d", m_intVal );
		return s_CharBuf;

	case MATERIAL_VAR_TYPE_FLOAT:
		Q_snprintf( s_CharBuf, sizeof( s_CharBuf ), "%f", m_VecVal[0] );
		return s_CharBuf;

	case MATERIAL_VAR_TYPE_VECTOR:
		{
			s_CharBuf[0] = '[';
			s_CharBuf[1] = ' ';
			int len = 2;
			for (int i = 0; i < m_nNumVectorComps; ++i)
			{
				if (len < sizeof( s_CharBuf ))
				{
					Q_snprintf( s_CharBuf + len, sizeof( s_CharBuf ) - len, "%f ", m_VecVal[i] );
					len += strlen( s_CharBuf + len );
				}
			}
			if (len < sizeof( s_CharBuf ) - 1)
			{
				s_CharBuf[len] = ']';
				s_CharBuf[len+1] = '\0';
			}
			else
			{
				s_CharBuf[sizeof( s_CharBuf )-1] = '\0';
			}
			return s_CharBuf;
		}

	case MATERIAL_VAR_TYPE_MATRIX:
 		{
			s_CharBuf[0] = '[';
			s_CharBuf[1] = ' ';
			int len = 2;
			for (int i = 0; i < 4; ++i)
			{
				for (int j = 0; j < 4; ++j)
				{
					if (len < sizeof( s_CharBuf ))
						len += Q_snprintf( s_CharBuf + len, sizeof( s_CharBuf ) - len, "%.3f ", m_pMatrix->m_Matrix[j][i] );
				}
			}
			if (len < sizeof( s_CharBuf ) - 1)
			{
				s_CharBuf[len] = ']';
				s_CharBuf[len+1] = '\0';
			}
			else
			{
				s_CharBuf[sizeof( s_CharBuf )-1] = '\0';
			}
			return s_CharBuf;
		}

	case MATERIAL_VAR_TYPE_TEXTURE:
		// check for env_cubemap
		if( IsTextureInternalEnvCubemap( m_pTexture ) )
		{
			return "env_cubemap";
		}
		else
		{
			Q_snprintf( s_CharBuf, sizeof( s_CharBuf ), "%s", m_pTexture->GetName() );
			return s_CharBuf;
		}
	case MATERIAL_VAR_TYPE_MATERIAL:
		Q_snprintf( s_CharBuf, sizeof( s_CharBuf ), "%s", m_pMaterialValue->GetName() );
		return s_CharBuf;

	case MATERIAL_VAR_TYPE_UNDEFINED:
		return "<UNDEFINED>";

	default:
		Warning( "CMaterialVar::GetStringValue: Unknown material var type\n" );
		return "";
	}
}

void CMaterialVar::SetStringValue( const char *val )
{
	ASSERT_NOT_DUMMY_VAR();
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( !m_bFakeMaterialVar && pCallQueue )
	{
		if ( s_bEnableThreadedAccess )
		{
			if ( m_nTempIndex == 0xFF )
			{
				m_nTempIndex = s_nTempVarsUsed++;
			}
			s_pTempMaterialVar[m_nTempIndex].SetStringValue( val );
		}
		pCallQueue->QueueCall( this, &CMaterialVar::SetStringValue, CUtlEnvelope<const char *>(val) );
		return;
	}

	CleanUpData();
	int len = Q_strlen( val ) + 1;
	m_pStringVal = new char[len];
	Q_strncpy( m_pStringVal, val, len );
	m_Type = MATERIAL_VAR_TYPE_STRING;
	m_intVal = atoi( val );
	m_VecVal[0] = m_VecVal[1] = m_VecVal[2] = m_VecVal[3] = atof( m_pStringVal );
	VarChanged();
}

void CMaterialVar::SetFourCCValue( FourCC type, void *pData )
{
	ASSERT_NOT_DUMMY_VAR();
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( !m_bFakeMaterialVar && pCallQueue )
	{
		if ( s_bEnableThreadedAccess )
		{
			if ( m_nTempIndex == 0xFF )
			{
				m_nTempIndex = s_nTempVarsUsed++;
			}
			s_pTempMaterialVar[m_nTempIndex].SetFourCCValue( type, pData );
		}
		pCallQueue->QueueCall( this, &CMaterialVar::SetFourCCValue, type, pData );
		return;
	}

	// Suppress all this if we're not actually changing anything
	if ((m_Type == MATERIAL_VAR_TYPE_FOURCC) && m_pFourCC->m_FourCC == type && m_pFourCC->m_pFourCCData == pData )
	{
#ifndef NO_TOOLFRAMEWORK
		RecordToolMessage();
#endif
		return;
	}

	CleanUpData();
	m_pFourCC = AllocFourCC();
	m_pFourCC->m_FourCC = type;
	m_pFourCC->m_pFourCCData = pData;
	m_Type = MATERIAL_VAR_TYPE_FOURCC;
	m_VecVal.Init();
	m_intVal = 0;
	VarChanged();
}

void CMaterialVar::GetFourCCValue( FourCC *type, void **ppData )
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( pCallQueue && !m_bFakeMaterialVar )
	{
		if ( !s_bEnableThreadedAccess )
		{
			//DevMsg( 2, "Non-thread safe call to CMaterialVar %s!\n", GetName() );
		}

		if ( m_nTempIndex != 0xFF )
			return s_pTempMaterialVar[m_nTempIndex].GetFourCCValue( type, ppData );
	}

	if( m_Type == MATERIAL_VAR_TYPE_FOURCC )
	{
		*type = m_pFourCC->m_FourCC;
		*ppData = m_pFourCC->m_pFourCCData;
	}
	else
	{
		*type = FOURCC_UNKNOWN;
		*ppData = 0;

		static int bitchCount;
		if( bitchCount < 10 )
		{
			Warning( "CMaterialVar::GetVecValue: trying to get a vec value for %s which is of type %d\n",
				GetName(), ( int )m_Type );
			bitchCount++;
		}
	}
}

bool CMaterialVar::IsTextureValueInternalEnvCubemap( void ) const
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( pCallQueue && !m_bFakeMaterialVar )
	{
		if ( !s_bEnableThreadedAccess )
		{
			//DevMsg( 2, "Non-thread safe call to CMaterialVar %s!\n", GetName() );
		}

		if ( m_nTempIndex != 0xFF )
			return s_pTempMaterialVar[m_nTempIndex].IsTextureValueInternalEnvCubemap( );
	}
	
	if( m_pMaterial )
	{
		m_pMaterial->Precache();
	}

	if( m_Type == MATERIAL_VAR_TYPE_TEXTURE )
	{
		if ( IsTextureInternalEnvCubemap( m_pTexture ) )
			return true;
	}

	return false;

}

//-----------------------------------------------------------------------------
// texture
//-----------------------------------------------------------------------------
ITexture *CMaterialVar::GetTextureValue( void )
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( pCallQueue && !m_bFakeMaterialVar )
	{
		if ( !s_bEnableThreadedAccess )
		{
			//DevMsg( 2, "Non-thread safe call to CMaterialVar %s!\n", GetName() );
		}

		if ( m_nTempIndex != 0xFF )
			return s_pTempMaterialVar[m_nTempIndex].GetTextureValue( );
	}

	ITexture *retVal = NULL;
	
	if( m_pMaterial )
	{
		m_pMaterial->Precache();
	}
	
	if( m_Type == MATERIAL_VAR_TYPE_TEXTURE )
	{
		if ( !IsTextureInternalEnvCubemap( m_pTexture ) )
		{
			retVal = static_cast<ITexture *>( m_pTexture );
		}
		else
		{
			retVal = MaterialSystem()->GetLocalCubemap();
		}
		if( !retVal )
		{
			static int bitchCount = 0;
			if( bitchCount < 10 )
			{
				Warning( "Invalid texture value in CMaterialVar::GetTextureValue\n" );
				bitchCount++;
			}
		}
	}
	else
	{
		static int bitchCount = 0;
		if( bitchCount < 10 )
		{
			Warning( "Requesting texture value from var \"%s\" which is "
					  "not a texture value (material: %s)\n", GetName(),
						m_pMaterial ? m_pMaterial->GetName() : "NULL material" );
			bitchCount++;
		}
	}

	if( !retVal )
	{
		retVal = TextureManager()->ErrorTexture();
	}
	return retVal;
}

void CMaterialVar::SetTextureValue( ITexture *texture )
{
	ASSERT_NOT_DUMMY_VAR();
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( !m_bFakeMaterialVar && pCallQueue )
	{
		// FIXME (toml): deal with reference count
		if ( s_bEnableThreadedAccess )
		{
			if ( m_nTempIndex == 0xFF )
			{
				m_nTempIndex = s_nTempVarsUsed++;
			}
			s_pTempMaterialVar[m_nTempIndex].SetTextureValue( texture );
		}
		pCallQueue->QueueCall( this, &CMaterialVar::SetTextureValue, texture );
		return;
	}

	ITextureInternal* pTexImp = static_cast<ITextureInternal *>( texture );

	// Suppress all this if we're not actually changing anything
	if ((m_Type == MATERIAL_VAR_TYPE_TEXTURE) && (m_pTexture == pTexImp))
	{
#ifndef NO_TOOLFRAMEWORK
		RecordToolMessage();
#endif
		return;
	}

	if( !IsTextureInternalEnvCubemap( pTexImp ) )
	{
		pTexImp->IncrementReferenceCount();
	}

	CleanUpData();
	m_pTexture = pTexImp;
	m_Type = MATERIAL_VAR_TYPE_TEXTURE;
	m_intVal = 0;
	m_VecVal.Init();
	VarChanged();
}


//-----------------------------------------------------------------------------
// material
//-----------------------------------------------------------------------------
IMaterial *CMaterialVar::GetMaterialValue( void )
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( pCallQueue && !m_bFakeMaterialVar )
	{
		if ( !s_bEnableThreadedAccess )
		{
			//DevMsg( 2, "Non-thread safe call to CMaterialVar %s!\n", GetName() );
		}

		if ( m_nTempIndex != 0xFF )
			return s_pTempMaterialVar[m_nTempIndex].GetMaterialValue( );
	}

	IMaterial *retVal = NULL;
	
	if( m_pMaterial )
	{
		m_pMaterial->Precache();
	}
	
	if( m_Type == MATERIAL_VAR_TYPE_MATERIAL )
	{
		retVal = static_cast<IMaterial *>( m_pMaterialValue );
	}
	else
	{
		static int bitchCount = 0;
		if( bitchCount < 10 )
		{
			Warning( "Requesting material value from var \"%s\" which is "
					  "not a material value (material: %s)\n", GetName(),
						m_pMaterial ? m_pMaterial->GetName() : "NULL material" );
			bitchCount++;
		}
	}
	return retVal;
}

void CMaterialVar::SetMaterialValue( IMaterial *pMaterial )
{
	ASSERT_NOT_DUMMY_VAR();
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( !m_bFakeMaterialVar && pCallQueue )
	{
		// FIXME (toml): deal with reference count
		if ( s_bEnableThreadedAccess )
		{
			if ( m_nTempIndex == 0xFF )
			{
				m_nTempIndex = s_nTempVarsUsed++;
			}
			s_pTempMaterialVar[m_nTempIndex].SetMaterialValue( pMaterial );
		}
		pCallQueue->QueueCall( this, &CMaterialVar::SetMaterialValue, pMaterial );
		return;
	}

	//HACKHACK: Only use the realtime material as the material value since converting it every time it's loaded could be forgotten, and chance of game code usage is low
	if( pMaterial )
		pMaterial = ((IMaterialInternal *)pMaterial)->GetRealTimeVersion();

	IMaterialInternal* pMaterialImp = static_cast<IMaterialInternal *>( pMaterial );

	// Suppress all this if we're not actually changing anything
	if ((m_Type == MATERIAL_VAR_TYPE_MATERIAL) && (m_pMaterialValue == pMaterialImp))
	{
#ifndef NO_TOOLFRAMEWORK
		RecordToolMessage();
#endif
		return;
	}

	if( pMaterialImp != NULL )
	{
		pMaterialImp->IncrementReferenceCount();
	}

	CleanUpData();
	m_pMaterialValue = pMaterialImp;
	m_Type = MATERIAL_VAR_TYPE_MATERIAL;
	m_intVal = 0;
	m_VecVal.Init();
	VarChanged();
}


//-----------------------------------------------------------------------------
// Vector
//-----------------------------------------------------------------------------
void CMaterialVar::GetLinearVecValue( float *pVal, int numComps ) const
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( pCallQueue && !m_bFakeMaterialVar )
	{
		if ( !s_bEnableThreadedAccess )
		{
			//DevMsg( 2, "Non-thread safe call to CMaterialVar %s!\n", GetName() );
		}

		if ( m_nTempIndex != 0xFF )
			return s_pTempMaterialVar[m_nTempIndex].GetLinearVecValue( pVal, numComps );
	}

	Assert( numComps <= 4 );

	switch( m_Type )
	{
	case MATERIAL_VAR_TYPE_VECTOR:
		{
			for ( int i = 0; i < numComps; ++i )
			{
				pVal[i] = GammaToLinear( m_VecVal[i] );
			}
		}
		break;

	case MATERIAL_VAR_TYPE_INT:
		{
			for ( int i = 0; i < numComps; ++i )
			{
				pVal[i] = GammaToLinear( m_intVal );
			}
		}
		break;

	case MATERIAL_VAR_TYPE_FLOAT:
		{
			for ( int i = 0; i < numComps; ++i )
			{
				pVal[i] = GammaToLinear( m_VecVal[0] );
			}
		}
		break;

	case MATERIAL_VAR_TYPE_MATRIX:
	case MATERIAL_VAR_TYPE_UNDEFINED:
		{
			for ( int i = 0; i < numComps; ++i )
			{
				pVal[i] = 0.0f;
			}
		}
		break;

	default:
		Warning( "CMaterialVar::GetVecValue: trying to get a vec value for %s which is of type %d\n",
			GetName(), ( int )m_Type );
		break;
	}
}

void CMaterialVar::SetVecValueInternal( const Vector4D &vec, int nComps )
{
	ASSERT_NOT_DUMMY_VAR();
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( !m_bFakeMaterialVar && pCallQueue )
	{
		if ( s_bEnableThreadedAccess )
		{
			if ( m_nTempIndex == 0xFF )
			{
				m_nTempIndex = s_nTempVarsUsed++;
			}
			s_pTempMaterialVar[m_nTempIndex].SetVecValueInternal( vec, nComps );
		}
		pCallQueue->QueueCall( this, &CMaterialVar::SetVecValueInternal, RefToVal( vec ), nComps );
		return;
	}
	// Suppress all this if we're not actually changing anything
	if ((m_Type == MATERIAL_VAR_TYPE_VECTOR ) && (m_VecVal == vec ) )
	{
#ifndef NO_TOOLFRAMEWORK
		RecordToolMessage();
#endif
		return;
	}

	if ( m_Type != MATERIAL_VAR_TYPE_VECTOR )
	{
		CleanUpData();
		m_Type = MATERIAL_VAR_TYPE_VECTOR;
	}
	Assert( nComps <= 4 );
	m_nNumVectorComps = nComps;
	memcpy( m_VecVal.Base(), vec.Base(), 4 * sizeof(float) );
	m_intVal = ( int ) m_VecVal[0];

#ifdef _DEBUG
	for (int i = m_nNumVectorComps; i < 4; ++i )
		Assert( m_VecVal[i] == 0.0f );
#endif
	VarChanged();
}

void CMaterialVar::SetVecValue( const float* pVal, int numComps )
{
	Vector4D vec;
	memcpy( vec.Base(), pVal, numComps * sizeof(float) );
	for (int i = numComps; i < 4; ++i )
	{
		vec[i] = 0.0f;
	}
	SetVecValueInternal( vec, numComps);
}

void CMaterialVar::SetVecValue( float x, float y )
{
	SetVecValueInternal( Vector4D( x, y, 0.0f, 0.0f ), 2 );
}

void CMaterialVar::SetVecValue( float x, float y, float z )
{
	SetVecValueInternal( Vector4D( x, y, z, 0.0f ), 3 );
}

void CMaterialVar::SetVecValue( float x, float y, float z, float w )
{
	SetVecValueInternal( Vector4D( x, y, z, w ), 4 );
}


void CMaterialVar::SetVecComponentValue( float fVal, int nComponent )
{
	ASSERT_NOT_DUMMY_VAR();
	
#ifndef _CERT
	// DIAF
	if ( nComponent < 0 || nComponent > 3 )
	{
		Error( "Invalid vector component (%d) of variable %s referenced in material %s", nComponent, GetName(), GetOwningMaterial()->GetName() );
		return;
	}
#endif

	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( !m_bFakeMaterialVar && pCallQueue )
	{
		if ( s_bEnableThreadedAccess )
		{
			if ( m_nTempIndex == 0xFF )
			{
				m_nTempIndex = s_nTempVarsUsed++;
				s_pTempMaterialVar[m_nTempIndex].SetVecValue( m_VecVal.Base(), m_nNumVectorComps );
			}
			s_pTempMaterialVar[m_nTempIndex].SetVecComponentValue( fVal, nComponent );
		}
		pCallQueue->QueueCall( this, &CMaterialVar::SetVecComponentValue, fVal, nComponent );
		return;
	}

	// Suppress all this if we're not actually changing anything
	if ((m_Type == MATERIAL_VAR_TYPE_VECTOR ) && (m_VecVal[nComponent] == fVal ) )
	{
#ifndef NO_TOOLFRAMEWORK
		RecordToolMessage();
#endif
		return;
	}

	if ( m_Type != MATERIAL_VAR_TYPE_VECTOR )
	{
		CleanUpData();
		m_Type = MATERIAL_VAR_TYPE_VECTOR;
	}

	if( m_nNumVectorComps < nComponent )
	{
		//reset all undefined components to 0
		for( int i = m_nNumVectorComps; i != nComponent; ++i )
			m_VecVal[i] = 0.0f; 

		m_nNumVectorComps = nComponent;
	}

	m_VecVal[nComponent] = fVal;

#ifdef _DEBUG
	for (int i = m_nNumVectorComps; i < 4; ++i )
		Assert( m_VecVal[i] == 0.0f );
#endif
	VarChanged();
}


//-----------------------------------------------------------------------------
// Matrix 
//-----------------------------------------------------------------------------
VMatrix const& CMaterialVar::GetMatrixValue( )
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( pCallQueue && !m_bFakeMaterialVar )
	{
		if ( !s_bEnableThreadedAccess )
		{
			//DevMsg( 2, "Non-thread safe call to CMaterialVar %s!\n", GetName() );
		}

		if ( m_nTempIndex != 0xFF )
			return s_pTempMaterialVar[m_nTempIndex].GetMatrixValue();
	}

	if (m_Type == MATERIAL_VAR_TYPE_MATRIX)
		return m_pMatrix->m_Matrix;

	static VMatrix identity( 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 );
	return identity;
}

void CMaterialVar::SetMatrixValue( VMatrix const& matrix )
{
	ASSERT_NOT_DUMMY_VAR();
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( !m_bFakeMaterialVar && pCallQueue )
	{
		if ( s_bEnableThreadedAccess )
		{
			if ( m_nTempIndex == 0xFF )
			{
				m_nTempIndex = s_nTempVarsUsed++;
			}
			s_pTempMaterialVar[m_nTempIndex].SetMatrixValue( matrix );
		}
		pCallQueue->QueueCall( this, &CMaterialVar::SetMatrixValue, RefToVal( matrix ) );
		return;
	}

	CleanUpData();

	// NOTE: This is necessary because the mempool MaterialVarMatrix_t uses is not threadsafe
	m_pMatrix = new MaterialVarMatrix_t;

	MatrixCopy( matrix, m_pMatrix->m_Matrix );
	m_Type = MATERIAL_VAR_TYPE_MATRIX;
	m_pMatrix->m_bIsIdent = matrix.IsIdentity();
	m_VecVal.Init();
	m_intVal = ( int ) m_VecVal[0];
	VarChanged();
}

bool CMaterialVar::MatrixIsIdentity( void ) const
{
	if( m_Type != MATERIAL_VAR_TYPE_MATRIX ) 
	{
		return true;
	}
	return m_pMatrix->m_bIsIdent;
}

//-----------------------------------------------------------------------------
// Undefined 
//-----------------------------------------------------------------------------
bool CMaterialVar::IsDefined() const
{
	return m_Type != MATERIAL_VAR_TYPE_UNDEFINED;
}

void CMaterialVar::SetUndefined()
{
	ASSERT_NOT_DUMMY_VAR();
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( !m_bFakeMaterialVar && pCallQueue )
	{
		if ( s_bEnableThreadedAccess )
		{		
			if ( m_nTempIndex == 0xFF )
			{
				m_nTempIndex = s_nTempVarsUsed++;
			}
			s_pTempMaterialVar[m_nTempIndex].SetUndefined( );
		}
		pCallQueue->QueueCall( this, &CMaterialVar::SetUndefined );
		return;
	}

	if (m_Type == MATERIAL_VAR_TYPE_UNDEFINED)
		return;

	CleanUpData();
	m_Type = MATERIAL_VAR_TYPE_UNDEFINED;
	VarChanged();
}


//-----------------------------------------------------------------------------
// Copy from another material var 
//-----------------------------------------------------------------------------
void CMaterialVar::CopyFrom( IMaterialVar *pMaterialVar )
{
	CMatCallQueue *pCallQueue = MaterialSystem()->GetRenderCallQueue();
	if ( !m_bFakeMaterialVar && pCallQueue )
	{
		if ( s_bEnableThreadedAccess )
		{
			if ( m_nTempIndex == 0xFF )
			{
				m_nTempIndex = s_nTempVarsUsed++;
			}
			s_pTempMaterialVar[m_nTempIndex].CopyFrom( pMaterialVar );
		}
		pCallQueue->QueueCall( this, &CMaterialVar::CopyFrom, pMaterialVar );
		return;
	}

	switch( pMaterialVar->GetType() )
	{
	case MATERIAL_VAR_TYPE_FLOAT:
		SetFloatValue( pMaterialVar->GetFloatValue() );
		break;

	case MATERIAL_VAR_TYPE_STRING:
		SetStringValue( pMaterialVar->GetStringValue() );
		break;

	case MATERIAL_VAR_TYPE_VECTOR:
		SetVecValue( pMaterialVar->GetVecValue(), pMaterialVar->VectorSize() );
		break;

	case MATERIAL_VAR_TYPE_TEXTURE:
		SetTextureValue( pMaterialVar->GetTextureValue() );
		break;

	case MATERIAL_VAR_TYPE_INT:
		SetIntValue( pMaterialVar->GetIntValue() );
		break;

	case MATERIAL_VAR_TYPE_FOURCC:
		{
			FourCC fourCC;
			void *pData;
			pMaterialVar->GetFourCCValue( &fourCC, &pData );
			SetFourCCValue( fourCC, pData );
		}
		break;

	case MATERIAL_VAR_TYPE_UNDEFINED:
		SetUndefined();
		break;

	case MATERIAL_VAR_TYPE_MATRIX:
		SetMatrixValue( pMaterialVar->GetMatrixValue() );
		break;

	case MATERIAL_VAR_TYPE_MATERIAL:
		SetMaterialValue( pMaterialVar->GetMaterialValue() );
		break;

	default:
		Assert(0);
	}
}

#ifndef NO_TOOLFRAMEWORK

// record tool message
void CMaterialVar::RecordToolMessage()
{
	if ( !m_pMaterial )
		return;

	IClientMaterialSystem *pClientMaterialSystem = MaterialSystem()->GetClientMaterialSystemInterface();
	if ( !pClientMaterialSystem )
		return;

	HTOOLHANDLE hEntity = pClientMaterialSystem->GetCurrentRecordingEntity();
	if ( hEntity == HTOOLHANDLE_INVALID )
		return;

	KeyValues *msg = new KeyValues( "material_proxy_state" );
	msg->SetString( "mtlName", m_pMaterial->GetName() );
	msg->SetString( "groupName", m_pMaterial->GetTextureGroupName() );

	switch ( GetType() )
	{
	case MATERIAL_VAR_TYPE_FLOAT:
		msg->SetFloat( GetName(), GetFloatValue() );
		break;

	case MATERIAL_VAR_TYPE_INT:
		msg->SetInt( GetName(), GetIntValue() );
		break;

	case MATERIAL_VAR_TYPE_STRING:
		msg->SetString( GetName(), GetStringValue() );
		break;

	case MATERIAL_VAR_TYPE_VECTOR:
		{
			char str[ 256 ];
			const float *pVal = GetVecValue();
			int dim = VectorSize();
			switch ( dim )
			{
			case 2:
				V_snprintf( str, sizeof( str ), "vector2d: %f %f", pVal[ 0 ], pVal[ 1 ] );
				break;
			case 3:
				V_snprintf( str, sizeof( str ), "vector3d: %f %f %f", pVal[ 0 ], pVal[ 1 ], pVal[ 2 ] );
				break;
			case 4:
				V_snprintf( str, sizeof( str ), "vector4d: %f %f %f %f", pVal[ 0 ], pVal[ 1 ], pVal[ 2 ], pVal[ 3 ] );
				break;
			default:
				Assert( 0 );
				*str = 0;
			}
			msg->SetString( GetName(), str );
		}
		break;

	case MATERIAL_VAR_TYPE_MATRIX:
		{
			char str[ 256 ];
			const VMatrix &matrix = GetMatrixValue();
			const float *pVal = matrix.Base();
			V_snprintf( str, sizeof( str ),
				"matrix: %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f",
				pVal[ 0 ],  pVal[ 1 ],  pVal[ 2 ],  pVal[ 3 ],
				pVal[ 4 ],  pVal[ 5 ],  pVal[ 6 ],  pVal[ 7 ],
				pVal[ 8 ],  pVal[ 9 ],  pVal[ 10 ], pVal[ 11 ],
				pVal[ 12 ], pVal[ 13 ], pVal[ 14 ], pVal[ 15 ] );
			msg->SetString( GetName(), str );
		}
		break;

	case MATERIAL_VAR_TYPE_FOURCC:
		Assert( 0 ); // JDTODO
		// fall-through intentional (until this case is implemented)

	case MATERIAL_VAR_TYPE_TEXTURE:
		//			V_snprintf( str, sizeof( str ), "texture: %x", pVar->GetTextureValue() );
		//			msg->SetString( pVarName, str );
		// fall-through intentional (until this case is implemented)

	case MATERIAL_VAR_TYPE_MATERIAL:
		//			V_snprintf( str, sizeof( str ), "material: %x", pVar->GetMaterialValue() );
		//			msg->SetString( pVarName, str );
		// fall-through intentional (until this case is implemented)

	case MATERIAL_VAR_TYPE_UNDEFINED:
		//			Assert( 0 ); // these appear to be (mostly? all?) textures, although I don't know why they're not caught by the texture case above...
		// fall-through intentional (until this case is implemented)

	default:
		Assert( 0 );
		msg->deleteThis();
		return;
	}

	pClientMaterialSystem->PostToolMessage( hEntity, msg );
	msg->deleteThis();
}

#endif

void CMaterialVar::VarChanged()
{
	if ( !m_pMaterial )
		return;

	m_pMaterial->ReportVarChanged( this );

#ifndef NO_TOOLFRAMEWORK
	RecordToolMessage();
#endif
}

//-----------------------------------------------------------------------------
// Parser utilities
//-----------------------------------------------------------------------------
static inline bool IsWhitespace( char c )
{
	return c == ' ' || c == '\t';
}

static inline bool IsEndline( char c )
{
	return c == '\n' || c == '\0';
}

static inline bool IsVector( const char* v )
{
	while (IsWhitespace(*v))
	{
		++v;
		if (IsEndline(*v))
			return false;
	}
	return *v == '[' || *v == '{';
}

//-----------------------------------------------------------------------------
// Creates a vector material var
//-----------------------------------------------------------------------------
static int ParseVectorFromKeyValueString( const char *pString, float vecVal[4] )
{
	const char* pScan = pString;
	bool divideBy255 = false;

	// skip whitespace
	while( IsWhitespace(*pScan) )
	{
		++pScan;
	}

	if( *pScan == '{' )
	{
		divideBy255 = true;
	}
	else
	{
		Assert( *pScan == '[' );
	}
	
	// skip the '['
	++pScan;
	int i;
	for( i = 0; i < 4; i++ )
	{
		// skip whitespace
		while( IsWhitespace(*pScan) )
		{
			++pScan;
		}

		if( IsEndline(*pScan) || *pScan == ']' || *pScan == '}' )
		{
			if (*pScan != ']' && *pScan != '}')
			{
				Warning( "no ']' or '}' found in vector key in ParseVectorFromKeyValueString\n" );
			}

			// allow for vec2's, etc.
			vecVal[i] = 0.0f;
			break;
		}

		char* pEnd;

		vecVal[i] = strtod( pScan, &pEnd );
		if (pScan == pEnd)
		{
			Warning( "error parsing vector element in ParseVectorFromKeyValueString\n" );
			return 0;
		}

		pScan = pEnd;
	}

	if( divideBy255 )
	{
		vecVal[0] *= ( 1.0f / 255.0f );
		vecVal[1] *= ( 1.0f / 255.0f );
		vecVal[2] *= ( 1.0f / 255.0f );
		vecVal[3] *= ( 1.0f / 255.0f );
	}

	return i;
}

void CMaterialVar::SetValueAutodetectType( const char *val )
{
	ASSERT_NOT_DUMMY_VAR();
	int len = Q_strlen( val );

	// Here, let's determine if we got a float or an int....
	char* pIEnd;	// pos where int scan ended
	char* pFEnd;	// pos where float scan ended
	const char* pSEnd = val + len ; // pos where token ends

	int ival = strtol( val, &pIEnd, 10 );
	float fval = (float)strtod( val, &pFEnd );

	if ( ( pFEnd > pIEnd ) && ( pFEnd == pSEnd ) )
	{
		SetFloatValue( fval );
		return;
	}
	
	if ( pIEnd == pSEnd )
	{
		SetIntValue( ival );
		return;
	}

	// Isn't an int or a float.

	// Is it a matrix?

	VMatrix mat;
	int count = sscanf( val, " [ %f %f %f %f  %f %f %f %f  %f %f %f %f  %f %f %f %f ]",
		&mat.m[0][0], &mat.m[0][1], &mat.m[0][2], &mat.m[0][3],
		&mat.m[1][0], &mat.m[1][1], &mat.m[1][2], &mat.m[1][3],
		&mat.m[2][0], &mat.m[2][1], &mat.m[2][2], &mat.m[2][3],
		&mat.m[3][0], &mat.m[3][1], &mat.m[3][2], &mat.m[3][3] );
	if (count == 16)
	{
		SetMatrixValue( mat );
		return;
	}

	Vector2D scale, center;
	float angle;
	Vector2D translation;

	//scan for pre-rotation scale and translation with assumed center syntax
	count = sscanf( val, " scale %f %f translate %f %f rotate %f",
		&scale.x, &scale.y, &translation.x, &translation.y, &angle );
	if (count == 5)
	{
		VMatrix temp;

		MatrixBuildTranslation( mat, translation.x - 0.5, translation.y - 0.5, 0.0f );
		MatrixBuildScale( temp, scale.x, scale.y, 1.0f );
		MatrixMultiply( mat, temp, mat );
		MatrixBuildRotateZ( temp, angle );
		MatrixMultiply( mat, temp, mat );

		Vector2D vOffset;
		vOffset.Init( 0.5f / ( scale.x != 0 ? scale.x : 1.0 ), 0.5f / ( scale.y != 0 ? scale.y : 1.0 ) );
		Vector2DRotate( vOffset, -angle, vOffset );

		MatrixBuildTranslation( temp, vOffset.x, vOffset.y, 0.0f );
		MatrixMultiply( mat, temp, mat );

		SetMatrixValue( mat );
		return;
	}

	count = sscanf( val, " center %f %f scale %f %f rotate %f translate %f %f",
		&center.x, &center.y, &scale.x, &scale.y, &angle, &translation.x, &translation.y );
	if (count == 7)
	{
		VMatrix temp;
		MatrixBuildTranslation( mat, -center.x, -center.y, 0.0f );
		MatrixBuildScale( temp, scale.x, scale.y, 1.0f );
		MatrixMultiply( temp, mat, mat );
		MatrixBuildRotateZ( temp, angle );
		MatrixMultiply( temp, mat, mat );
		MatrixBuildTranslation( temp, center.x + translation.x, center.y + translation.y, 0.0f );
		MatrixMultiply( temp, mat, mat );
		SetMatrixValue( mat );
		return;
	}

	if( IsVector( val ) )
	{
		float vecVal[4];
		int nDim = ParseVectorFromKeyValueString( val, vecVal );
		if ( nDim > 0 )
		{
			SetVecValue( vecVal, nDim );
			return;
		}
	}

	SetStringValue( val );
}

void CompactMaterialVars( IMaterialVar **ppMaterialVars, int nVars )
{
	for ( int i = 0; i < nVars; i++ )
	{
		if ( MVInternallyAllocated( ppMaterialVars[i] ) )
		{
			ppMaterialVars[i] = (IMaterialVar *)MVRelocate( ppMaterialVars[i] );
		}
	}
}

void CompactMaterialVarHeap()
{
	MVDecommitUnusedPages();
}

