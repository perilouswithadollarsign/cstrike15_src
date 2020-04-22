//===== Copyright © 1996-2007, Valve Corporation, All rights reserved. ======//
//
// $Header: $
// $NoKeywords: $
//
// SOA container
//===========================================================================//

#include "utlsoacontainer.h"
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include "mathlib/halton.h"
#include "vstdlib/jobthread.h"
#include "tier1/callqueue.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
static size_t s_DataTypeByteSize[]= 
{
	sizeof( float ),
	3 * sizeof( float ),
	sizeof( int ),
	sizeof( void * ),
};

static fltx4 s_ZeroFields[3];

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CSOAContainer::CSOAContainer( int nCols, int nRows, int nSlices, ... )
{
	COMPILE_TIME_ASSERT( ATTRDATATYPE_COUNT == ARRAYSIZE( s_DataTypeByteSize ) );

	Init();
	va_list args;
	va_start( args, nSlices );
	for(;;)
	{
		int nFieldNumber = va_arg( args, int );
		if ( nFieldNumber == -1 )
			break;
		EAttributeDataType nDataType = (EAttributeDataType)va_arg( args, int );
		SetAttributeType( nFieldNumber, nDataType );
	}
	va_end( args );
	AllocateData( nCols, nRows, nSlices );
}

CSOAContainer::~CSOAContainer( void )
{
	Purge();
}


//-----------------------------------------------------------------------------
// Purge
//-----------------------------------------------------------------------------
void CSOAContainer::Purge( void )
{
	PurgeData();
	Init();
}


//-----------------------------------------------------------------------------
// Allocate data, purge data
//-----------------------------------------------------------------------------
void CSOAContainer::AllocateData( int nNCols, int nNRows, int nSlices )
{
	m_nColumns = nNCols;
	m_nRows = nNRows;
	m_nSlices = nSlices;
	
	m_nPaddedColumns = ( 3 + nNCols ) & ~3;							// pad up for sse
	m_nNumQuadsPerRow = ( m_nPaddedColumns >> 2 );

	// Allocate data memory and constant memory
	AllocateDataMemory();
	AllocateConstantMemory();

	// now, fill in strides and pointers
	uint8 *pBasePtr = m_pDataMemory;
	uint8 *pConstantDataPtr = m_pConstantDataMemory;
	for( int i = 0; i < MAX_SOA_FIELDS; i++ )
	{
		if ( m_nDataType[i] == ATTRDATATYPE_NONE )
		{
			m_pAttributePtrs[i] = reinterpret_cast<uint8 *>( s_ZeroFields );
			m_nStrideInBytes[i] = 0;
			m_nRowStrideInBytes[i] = 0;
			m_nSliceStrideInBytes[i] = 0;
			continue;
		}

		if ( m_nFieldPresentMask & ( 1 << i ) )
		{
			m_pAttributePtrs[i] = pBasePtr;
			m_nStrideInBytes[i] = s_DataTypeByteSize[m_nDataType[i]];
			m_nRowStrideInBytes[i] = m_nPaddedColumns * m_nStrideInBytes[i];
			m_nSliceStrideInBytes[i] = m_nRowStrideInBytes[i] * m_nRows;
			pBasePtr += AttributeMemorySize( i );
		}
		else
		{
			m_pAttributePtrs[i] = pConstantDataPtr;
			m_nStrideInBytes[i] = 0;
			m_nRowStrideInBytes[i] = 0;
			m_nSliceStrideInBytes[i] = 0;
			pConstantDataPtr += AttributeMemorySize( i );
		}
	}
	SetThreadMode( SOATHREADMODE_AUTO );
}


void CSOAContainer::SetAttributeType( int nAttrIdx, EAttributeDataType nDataType, bool bAllocateMemory )
{
	Assert( nAttrIdx < MAX_SOA_FIELDS );
	if ( !m_pDataMemory )
	{
		// Attributes will be allocated/setup later, when AllocateData is called
		if ( ( nDataType != ATTRDATATYPE_NONE ) && bAllocateMemory )
			m_nFieldPresentMask |= ( 1 << nAttrIdx );
		else
			m_nFieldPresentMask &= ~( 1 << nAttrIdx );
		m_nDataType[nAttrIdx] = nDataType;
		return;
	}

	// Attributes have already been allocated/setup by AllocateData
	if ( m_nDataType[nAttrIdx] != ATTRDATATYPE_NONE )
	{
		// This attribute was already setup, can't change it now!
		if ( m_nDataType[nAttrIdx] != nDataType )
		{
			Warning( "CSOAContainer::SetAttributeType - ERROR, trying to change type of previously-defined attribute %d!\n", nAttrIdx );
			Assert( 0 );
		}
		return;
	}

	// Add a new attribute with a separate allocation
	m_nDataType[nAttrIdx] = nDataType;
	if ( bAllocateMemory )
	{
		m_nFieldPresentMask |= ( 1 << nAttrIdx );
		m_nStrideInBytes[nAttrIdx]      = s_DataTypeByteSize[nDataType];
		m_nRowStrideInBytes[nAttrIdx]   = m_nStrideInBytes[nAttrIdx] * m_nPaddedColumns;
		m_nSliceStrideInBytes[nAttrIdx] = m_nRowStrideInBytes[nAttrIdx] * m_nRows;
	}
	else
	{
		// New attribute is constant
		m_nStrideInBytes[nAttrIdx]      = 0;
		m_nRowStrideInBytes[nAttrIdx]   = 0;
		m_nSliceStrideInBytes[nAttrIdx] = 0;
	}
	m_pSeparateDataMemory[nAttrIdx] = reinterpret_cast<uint8 *>( MemAlloc_AllocAligned( AttributeMemorySize( nAttrIdx ), 16 ) );
	m_pAttributePtrs[nAttrIdx] = m_pSeparateDataMemory[nAttrIdx];
	if ( !bAllocateMemory )
	{
		// Set constant memory to zero as the default value
		memset( m_pSeparateDataMemory[nAttrIdx], 0, AttributeMemorySize( nAttrIdx ) );
	}
}


void CSOAContainer::PurgeData( void )
{
	if ( m_pConstantDataMemory )
	{
		MemAlloc_FreeAligned( m_pConstantDataMemory );
		m_pConstantDataMemory = NULL;
	}

	if ( m_pDataMemory )
	{
		MemAlloc_FreeAligned( m_pDataMemory );
		m_pDataMemory = NULL;
	}

	for( int i = 0; i < ARRAYSIZE( m_pSeparateDataMemory ); i++ )
	{
		if ( m_pSeparateDataMemory[i] )
		{
			MemAlloc_FreeAligned( m_pSeparateDataMemory[i] );
			m_pSeparateDataMemory[i] = NULL;
		}
	}
}

size_t CSOAContainer::AttributeMemorySize( int nAttrIndex ) const
{
	EAttributeDataType nDataType = m_nDataType[ nAttrIndex ];
	if ( nDataType == ATTRDATATYPE_NONE )
		return 0;
	else if ( m_nFieldPresentMask & ( 1 << nAttrIndex ) )
		return ( s_DataTypeByteSize[ nDataType ] * m_nPaddedColumns * m_nRows * m_nSlices );
	else
		return ( 4 * s_DataTypeByteSize[ nDataType ] );
}

size_t CSOAContainer::DataMemorySize( void ) const
{
	size_t nDataMemorySize = 0;
	for( int i = 0; i < MAX_SOA_FIELDS; i++ )
	{
		if ( !( m_nFieldPresentMask & ( 1 << i ) ) )
			continue;
		nDataMemorySize += AttributeMemorySize( i );
	}
	return nDataMemorySize;
}

void CSOAContainer::AllocateDataMemory( void )
{
	Assert( !m_pDataMemory );
	size_t nMemorySize = DataMemorySize();
	if ( nMemorySize )
	{
		m_pDataMemory = reinterpret_cast<uint8 *> ( MemAlloc_AllocAligned( nMemorySize, 16 ) );
	}
}

size_t CSOAContainer::ConstantMemorySize( void ) const
{
	size_t nConstantDataSize = 0;
	for( int i = 0; i < MAX_SOA_FIELDS; i++ )
	{
		if ( ( m_nDataType[i] == ATTRDATATYPE_NONE ) || ( m_nFieldPresentMask & ( 1 << i ) ) )
			continue;
		nConstantDataSize += AttributeMemorySize( i );
	}
	return nConstantDataSize;
}

void CSOAContainer::AllocateConstantMemory( void )
{
	Assert( !m_pConstantDataMemory );
	size_t nConstantDataSize = ConstantMemorySize();
	if ( nConstantDataSize > 0 )
	{
		m_pConstantDataMemory = (uint8*)MemAlloc_AllocAligned( nConstantDataSize, 16 );
		memset( m_pConstantDataMemory, 0, nConstantDataSize );
	}
}


void CSOAContainer::SetThreadMode( SOAThreadMode_t eThreadMode )
{
	if ( eThreadMode == SOATHREADMODE_AUTO )
	{
		eThreadMode = SOATHREADMODE_NONE;
		if ( NumRows() * NumCols() > ( 16 * 16 ) )
		{
			eThreadMode = SOATHREADMODE_BYROWS;
		}
	}
	m_eThreadMode = eThreadMode;
}

#define THREAD_NJOBS 32

#define PARALLEL_DISPATCH( method, ... )								\
{																		\
	if ( m_eThreadMode == SOATHREADMODE_NONE )							\
	{																	\
		method( 0, NumRows(), 0, NumSlices(), __VA_ARGS__ );			\
	}																	\
	else																\
	{																	\
		CCallQueue workList;											\
		int nStep = MAX( 1, ( NumRows() / THREAD_NJOBS ) );				\
		int nY = 0;														\
		while( nY < NumRows() )											\
		{																\
			nStep = MIN( nStep, NumRows() - nY );						\
			workList.QueueCall( this, &CSOAContainer::method, nY, nStep, 0, NumSlices(), __VA_ARGS__ ); \
			nY += nStep;												\
		}																\
		workList.ParallelCallQueued();									\
	}																	\
}


void CSOAContainer::CopyAttrFromPartial( int nStartRow, int nNumRows, int nStartSlice, int nEndSlice, CSOAContainer const *pOther, int nDestAttributeIndex, int nSrcAttributeIndex )
{
	// copy a subregion in parallel
	for( int z = nStartSlice; z < nEndSlice; z++ )
	{
		size_t nCopySize = m_nRowStrideInBytes[nDestAttributeIndex] * nNumRows;
		memcpy( RowPtr<fltx4>( nDestAttributeIndex, nStartRow, z ), 
				pOther->ConstRowPtr( nSrcAttributeIndex, nStartRow, z ),
				nCopySize );
	}
}

void CSOAContainer::CopyAttrFrom( CSOAContainer const &other, int nDestAttributeIndex, int nSrcAttributeIndex )
{
	if ( nSrcAttributeIndex == -1 )
	{
		nSrcAttributeIndex = nDestAttributeIndex;
	}
	Assert( other.NumRows() == NumRows() );
	Assert( other.NumCols() == NumCols() );
	Assert( other.NumSlices() == NumSlices() );
	Assert( m_nDataType[nDestAttributeIndex] == other.m_nDataType[nSrcAttributeIndex] );
	if ( m_eThreadMode == SOATHREADMODE_NONE )
	{
		memcpy( m_pAttributePtrs[nDestAttributeIndex], other.m_pAttributePtrs[nSrcAttributeIndex], AttributeMemorySize( nDestAttributeIndex ) );
	}
	else
	{
		PARALLEL_DISPATCH( CopyAttrFromPartial, &other, nDestAttributeIndex, nSrcAttributeIndex );
	}
}


void CSOAContainer::CopyAttrToAttr( int nSrcAttributeIndex, int nDestAttributeIndex)
{
	Assert( m_nDataType[nSrcAttributeIndex] == m_nDataType[nDestAttributeIndex] );
	memcpy( m_pAttributePtrs[nDestAttributeIndex], m_pAttributePtrs[nSrcAttributeIndex], AttributeMemorySize( nSrcAttributeIndex ) );
}

void CSOAContainer::PackScalarAttributesToVectorAttribute( CSOAContainer *pInput,
														   int nVecAttributeOut,
														   int nScalarAttributeX,
														   int nScalarAttributeY,
														   int nScalarAttributeZ )
{
	AssertDataType( nVecAttributeOut, ATTRDATATYPE_4V );
	pInput->AssertDataType( nScalarAttributeX, ATTRDATATYPE_FLOAT );
	pInput->AssertDataType( nScalarAttributeY, ATTRDATATYPE_FLOAT );
	pInput->AssertDataType( nScalarAttributeZ, ATTRDATATYPE_FLOAT );

	FourVectors *pOut = RowPtr<FourVectors>( nVecAttributeOut, 0 );
	fltx4 *pInX = pInput->RowPtr<fltx4>( nScalarAttributeX, 0 );
	fltx4 *pInY = pInput->RowPtr<fltx4>( nScalarAttributeY, 0 );
	fltx4 *pInZ = pInput->RowPtr<fltx4>( nScalarAttributeZ, 0 );
	size_t nRowToRowStride = RowToRowStep( nVecAttributeOut ) / sizeof( FourVectors );
	size_t nRowToRowStrideX = pInput->RowToRowStep( nScalarAttributeX ) / sizeof( fltx4 );
	size_t nRowToRowStrideY = pInput->RowToRowStep( nScalarAttributeY ) / sizeof( fltx4 );
	size_t nRowToRowStrideZ = pInput->RowToRowStep( nScalarAttributeZ ) / sizeof( fltx4 );
	int nRowCtr = NumRows() * NumSlices();
	do
	{
		int nColCtr = NumQuadsPerRow();
		do
		{
			pOut->x = *( pInX++ );
			pOut->y = *( pInY++ );
			pOut->z = *( pInZ++ );
			pOut++;
		} while ( --nColCtr );
		pOut += nRowToRowStride;
		pInX += nRowToRowStrideX;
		pInY += nRowToRowStrideY;
		pInZ += nRowToRowStrideZ;
	} while ( --nRowCtr );
}


void CSOAContainer::UnPackVectorAttributeToScalarAttributes( CSOAContainer *pInput,
															 int nVecAttributeIn,
															 int nScalarAttributeX,
															 int nScalarAttributeY,
															 int nScalarAttributeZ )
{
	pInput->AssertDataType( nVecAttributeIn, ATTRDATATYPE_4V );
	AssertDataType( nScalarAttributeX, ATTRDATATYPE_FLOAT );
	AssertDataType( nScalarAttributeY, ATTRDATATYPE_FLOAT );
	AssertDataType( nScalarAttributeZ, ATTRDATATYPE_FLOAT );
	Assert( pInput->NumCols() == NumCols() );
	Assert( pInput->NumRows() == NumRows() );
	Assert( pInput->NumSlices() == NumSlices() );

	FourVectors *pIn = pInput->RowPtr<FourVectors>( nVecAttributeIn, 0 );
	fltx4 *pX = RowPtr<fltx4>( nScalarAttributeX, 0 );
	fltx4 *pY = RowPtr<fltx4>( nScalarAttributeY, 0 );
	fltx4 *pZ = RowPtr<fltx4>( nScalarAttributeZ, 0 );
	size_t nRowToRowStride = pInput->RowToRowStep( nVecAttributeIn ) / sizeof( FourVectors );
	size_t nRowToRowStrideX = RowToRowStep( nScalarAttributeX ) / sizeof( fltx4 );
	size_t nRowToRowStrideY = RowToRowStep( nScalarAttributeY ) / sizeof( fltx4 );
	size_t nRowToRowStrideZ = RowToRowStep( nScalarAttributeZ ) / sizeof( fltx4 );
	int nRowCtr = NumRows() * NumSlices();
	do
	{
		int nColCtr = NumQuadsPerRow();
		do
		{
			*( pX++ ) = pIn->x;
			*( pY++ ) = pIn->y;
			*( pZ++ ) = pIn->z;
			pIn++;
		} while ( --nColCtr );
		pIn += nRowToRowStride;
		pX += nRowToRowStrideX;
		pY += nRowToRowStrideY;
		pZ += nRowToRowStrideZ;
	} while ( --nRowCtr );
}



void CSOAContainer::MultiplyVectorAttribute( CSOAContainer *pInput, int nAttributeIn,
											 const Vector &vecScalar, 
											 int nAttributeOut )
{
	Assert( pInput->NumCols() == NumCols() );
	Assert( pInput->NumRows() == NumRows() );
	FourVectors v4Scale;
	v4Scale.DuplicateVector( vecScalar );
	pInput->AssertDataType( nAttributeIn, ATTRDATATYPE_4V );
	AssertDataType( nAttributeOut, ATTRDATATYPE_4V );
	size_t nRowToRowStride = pInput->RowToRowStep( nAttributeIn ) / sizeof( FourVectors );
	size_t nRowToRowStrideOut = RowToRowStep( nAttributeOut ) / sizeof( FourVectors );
	int nRowCtr = NumRows() * NumSlices();
	FourVectors const *pIn = pInput->RowPtr<FourVectors>( nAttributeIn, 0 );
	FourVectors *pOut = RowPtr<FourVectors>( nAttributeOut, 0 );
	do
	{
		int nColCtr = NumQuadsPerRow();
		do
		{
			FourVectors v4In = *( pIn++ );
			v4In *= v4Scale;
			*(pOut++) = v4In;
		} while ( --nColCtr );
		pOut += nRowToRowStrideOut;
		pIn += nRowToRowStride;
	} while ( --nRowCtr );
}

void CSOAContainer::RandomizeAttribute( int nAttr, float flMin, float flMax ) const
{
	AssertDataType( nAttr, ATTRDATATYPE_FLOAT );
	fltx4 *pOut = RowPtr<fltx4>( nAttr, 0 );
	size_t nRowToRowStride = RowToRowStep( nAttr ) / sizeof( fltx4 );
	int nContext = GetSIMDRandContext();
	int nRowCtr = NumRows() * NumSlices();
	fltx4 fl4Min = ReplicateX4( flMin );
	fltx4 fl4Domain = ReplicateX4( flMin - flMin );
	do
	{
		int nColCtr = NumQuadsPerRow();
		do
		{
			*(pOut++) = AddSIMD( fl4Min, MulSIMD( fl4Domain, RandSIMD( nContext ) ) );
		} while ( --nColCtr );
		pOut += nRowToRowStride;
	} while ( --nRowCtr );
	ReleaseSIMDRandContext( nContext );

}


void CSOAContainer::FillAttrWithInterpolatedValues( int nAttr, float flValue00, float flValue10, float flValue01, float flValue11 ) const
{
	float ooWidth = 1.0 / ( NumCols() - 1 );
	float ooHeight = 1.0 / ( NumRows() - 1 );
	float flYDelta0 = ooHeight * ( flValue01 - flValue00 );
	float flYDelta1 = ooHeight * ( flValue11 - flValue10 );
	int nRowCtr = NumRows();
	fltx4 *pOut = RowPtr<fltx4>( nAttr, 0 );
	size_t nRowToRowStride = RowToRowStep( nAttr ) / sizeof( fltx4 );
	do
	{
		float flXDelta = ooWidth * ( flValue10 - flValue00 );
		fltx4 fl4Value;
		SubFloat( fl4Value, 0 ) = flValue00;
		SubFloat( fl4Value, 1 ) = flValue00 + flXDelta;
		SubFloat( fl4Value, 2 ) = flValue00 + flXDelta + flXDelta;
		SubFloat( fl4Value, 3 ) = flValue00 + flXDelta + flXDelta + flXDelta;
		fltx4 fl4XDelta = ReplicateX4( flXDelta * 4.0 );
		int nColCtr = NumQuadsPerRow();
		do
		{
			*( pOut++ ) = fl4Value;
			fl4Value = AddSIMD( fl4Value, fl4XDelta );
		} while( --nColCtr );
		pOut += nRowToRowStride;
		flValue00 += flYDelta0;
		flValue10 += flYDelta1;
	} while ( --nRowCtr );

}

void CSOAContainer::FillAttrWithInterpolatedValues( int nAttr, Vector vecValue00, Vector vecValue10, const Vector &vecValue01, const Vector &vecValue11 ) const
{
	float ooWidth = 1.0 / ( NumCols() - 1 );
	float ooHeight = 1.0 / ( NumRows() - 1 );
	Vector vecYDelta0 = ooHeight * ( vecValue01 - vecValue00 );
	Vector vecYDelta1 = ooHeight * ( vecValue11 - vecValue10 );
	int nRowCtr = NumRows();
	FourVectors *pOut = RowPtr<FourVectors>( nAttr, 0 );
	size_t nRowToRowStride = RowToRowStep( nAttr ) / sizeof( FourVectors );
	do
	{
		Vector vecXDelta = ooWidth * ( vecValue10 - vecValue00 );
		FourVectors v4Value;
		v4Value.LoadAndSwizzle( vecValue00, vecValue00 + vecXDelta, 
								vecValue00 + vecXDelta + vecXDelta, vecValue00 + vecXDelta + vecXDelta + vecXDelta );
		FourVectors v4XDelta;
		v4XDelta.DuplicateVector( vecXDelta * 4.0 );
		int nColCtr = NumQuadsPerRow();
		do
		{
			*( pOut++ ) = v4Value;
			v4Value += v4XDelta;
		} while( --nColCtr );
		pOut += nRowToRowStride;
		vecValue00 += vecYDelta0;
		vecValue10 += vecYDelta1;
	} while ( --nRowCtr );
	
}

void CSOAContainer::FillAttr( int nAttr, const Vector &vecValue )
{
	FourVectors v4Fill;
	v4Fill.DuplicateVector( vecValue );
	if ( !HasAllocatedMemory( nAttr ) )
	{
		FourVectors *pOut = (FourVectors*)m_pAttributePtrs[ nAttr ];
		*pOut = v4Fill;
		return;
	}

	AssertDataType( nAttr, ATTRDATATYPE_4V );
	FourVectors *pOut = RowPtr<FourVectors>( nAttr, 0 );
	size_t nRowToRowStride = RowToRowStep( nAttr ) / sizeof( FourVectors );
	int nRowCtr = NumRows() * NumSlices();
	do
	{
		int nColCtr = NumQuadsPerRow();
		do
		{
			*(pOut++) = v4Fill;
		} while ( --nColCtr );
		pOut += nRowToRowStride;
	} while ( --nRowCtr );
}

void CSOAContainer::FillAttrPartial( int nStartRow, int nNumRows, int nStartSlice, int nEndSlice, int nAttr, fltx4 fl4Value )
{
	for( int z = nStartSlice; z < nEndSlice; z++ )
	{
		fltx4 *pOut = RowPtr<fltx4>( nAttr, nStartRow, z );
		size_t nRowToRowStride = RowToRowStep( nAttr ) / sizeof( fltx4 );
		int nRowCtr = nNumRows;
		do
		{
			int nColCtr = NumQuadsPerRow();
			do
			{
				*(pOut++) = fl4Value;
			} while ( --nColCtr );
			pOut += nRowToRowStride;
		} while ( --nRowCtr );
	}
}


void CSOAContainer::FillAttr( int nAttr, float flValue )
{
	fltx4 fl4Fill = ReplicateX4( flValue );
	if ( !HasAllocatedMemory( nAttr ) )
	{
		fltx4 *pOut = (fltx4*)m_pAttributePtrs[ nAttr ];
		*pOut = fl4Fill;
		return;
	}

	AssertDataType( nAttr, ATTRDATATYPE_FLOAT );
	PARALLEL_DISPATCH( FillAttrPartial, nAttr, fl4Fill );
}

float CSOAContainer::SumAttributeValue( int nAttr ) const
{
	return ReduceAttr<AddSIMD>( nAttr, Four_Zeros );
}

float CSOAContainer::AverageFloatAttributeValue( int nAttr ) const
{
	if ( HasAllocatedMemory( nAttr ) )
	{
		return SumAttributeValue( nAttr ) / ( NumCols() * NumRows() * NumSlices() );
	}
	else
	{
		return FloatValue( nAttr, 0, 0, 0 );
	}
}

float CSOAContainer::MaxAttributeValue( int nAttr ) const
{
	return ReduceAttr<MaxSIMD>( nAttr, Four_Negative_FLT_MAX );
}

float CSOAContainer::MinAttributeValue( int nAttr ) const
{
	return ReduceAttr<MinSIMD>( nAttr, Four_FLT_MAX );
}

void CSOAContainer::NormalizeAttr( int nAttr )
{
	AssertDataType( nAttr, ATTRDATATYPE_4V );
	FourVectors *pOut = RowPtr<FourVectors>( nAttr, 0 );
	size_t nRowToRowStride = RowToRowStep( nAttr ) / sizeof( FourVectors );
	int nRowCtr = NumRows() * NumSlices();
	do
	{
		int nColCtr = NumQuadsPerRow();
		do
		{
			FourVectors v4Data = *pOut;
			v4Data.VectorNormalize();
			*( pOut++ ) = v4Data;
		} while ( --nColCtr );
		pOut += nRowToRowStride;
	} while ( --nRowCtr );
}

void CSOAContainer::MulAttr( CSOAContainer const &src, int nSrcAttr, int nDestAttr )
{
	AssertDataType( nDestAttr, ATTRDATATYPE_4V );
	src.AssertDataType( nSrcAttr, ATTRDATATYPE_4V );
	FourVectors *pOut = RowPtr<FourVectors>( nDestAttr, 0 );
	FourVectors *pIn = src.RowPtr<FourVectors>( nSrcAttr, 0 );
	size_t nSrcRowToRowStride = src.RowToRowStep( nSrcAttr ) / sizeof( FourVectors );
	size_t nRowToRowStride = RowToRowStep( nDestAttr ) / sizeof( FourVectors );
	int nRowCtr = NumRows() * NumSlices();
	do
	{
		int nColCtr = NumQuadsPerRow();
		do
		{
			FourVectors rslt = *( pIn++ );
			rslt *= *pOut;
			*(pOut++) = rslt;
		} while ( --nColCtr );
		pOut += nRowToRowStride;
		pIn += nSrcRowToRowStride;
	} while ( --nRowCtr );
}

void CSOAContainer::AddGaussianSRBF( float flWeight, Vector vecDir, int nDirectionAttribute, int nScalarTargetAttribute )
{
	AssertDataType( nDirectionAttribute, ATTRDATATYPE_4V );
	AssertDataType( nScalarTargetAttribute, ATTRDATATYPE_FLOAT );
	fltx4 fl4Weight = ReplicateX4( flWeight );
	FourVectors v4Dir;
	v4Dir.DuplicateVector( vecDir );

	FourVectors *pDirIn = RowPtr<FourVectors>( nDirectionAttribute, 0 );
	size_t nRowToRowStride = RowToRowStep( nDirectionAttribute ) / sizeof( FourVectors );
	fltx4 *pTarget = RowPtr<fltx4>( nScalarTargetAttribute, 0 );
	size_t nRowToRowStrideTarget = RowToRowStep( nScalarTargetAttribute ) / sizeof( fltx4 );
	int nRowCtr = NumRows() * NumSlices();
	do
	{
		int nColCtr = NumQuadsPerRow();
		do
		{
			FourVectors v4InDir = *( pDirIn++ );
			fltx4 fl4ExpDot = NatExpSIMD( v4Dir * v4InDir );
			fltx4 fl4Addend = MulSIMD( fl4Weight, fl4ExpDot );
			fl4Addend = AddSIMD( fl4Addend, *( pTarget ) );
			*( pTarget++ ) = fl4Addend;
		} while ( --nColCtr );
		pDirIn += nRowToRowStride;
		pTarget += nRowToRowStrideTarget;
	} while ( --nRowCtr );
}

void CSOAContainer::AddGaussianSRBF( Vector vecWeight, Vector vecDir, int nDirectionAttribute, 
									 int nVectorTargetAttribute )
{
	AssertDataType( nDirectionAttribute, ATTRDATATYPE_4V );
	AssertDataType( nVectorTargetAttribute, ATTRDATATYPE_4V );
	FourVectors v4Weight;
	v4Weight.DuplicateVector( vecWeight );
	FourVectors v4Dir;
	v4Dir.DuplicateVector( vecDir );

	FourVectors *pDirIn = RowPtr<FourVectors>( nDirectionAttribute, 0 );
	size_t nRowToRowStride = RowToRowStep( nDirectionAttribute ) / sizeof( FourVectors );
	FourVectors *pTarget = RowPtr<FourVectors>( nVectorTargetAttribute, 0 );
	int nRowCtr = NumRows() * NumSlices();
	do
	{
		int nColCtr = NumQuadsPerRow();
		do
		{
			fltx4 fl4ExpDot = NatExpSIMD( *( pDirIn++ ) * v4Dir );
			FourVectors v4Addend = v4Weight;
			v4Addend *= fl4ExpDot;
			*( pTarget++ ) += v4Addend;
		} while ( --nColCtr );
		pDirIn += nRowToRowStride;
		pTarget += nRowToRowStride;
	} while ( --nRowCtr );
}

enum EResampleHorzMode {
	HMODE_DOWNSAMPLE_4X,
	HMODE_DOWNSAMPLE_2X,
	HMODE_DOWNSAMPLE_1X,
};


template<EResampleHorzMode M, class T> void ResampleAttributeInternal( CSOAContainer &src, CSOAContainer &dst, int nAttr )
{
	// we'll just point sample in rows + slices. Within a row, we need do do simd expand/no-expand
	for( int s = 0; s < dst.NumSlices(); s++ )
	{
		int srcs = (int)RemapVal( s, 0, dst.NumSlices() - 1, 0, src.NumSlices() - 1 );
		for( int r = 0; r < dst.NumRows(); r++ )
		{
			int srcr = (int)RemapVal( r, 0, dst.NumRows() - 1, 0, src.NumRows() - 1 );
			T *pSrc = src.RowPtr<T>( nAttr, srcr, srcs );
			T *pDest = dst.RowPtr<T>( nAttr, r, s );
			int n = dst.NumQuadsPerRow();
			if ( M == HMODE_DOWNSAMPLE_4X )
			{
				do
				{
					*( pDest++ ) = Compress4SIMD( pSrc[0], pSrc[1], pSrc[2], pSrc[3] );
					pSrc += 4;
				} while( --n );
			}
			if ( M == HMODE_DOWNSAMPLE_2X )
			{
				do
				{
					*( pDest++ ) = CompressSIMD( pSrc[0], pSrc[1] );
					pSrc += 2;
				} while( --n );
			}
			if ( M == HMODE_DOWNSAMPLE_1X )
			{
				memcpy( pDest, pSrc, n * sizeof( T ) );
			}
		}
	}
}

template<class T> void ResampleAttributeInternalDType( CSOAContainer &src, CSOAContainer &dst, int nAttr )
{
	int nSrcW = src.NumCols();
	int nDstW = dst.NumCols();
	if ( nSrcW == nDstW )
	{
		ResampleAttributeInternal<HMODE_DOWNSAMPLE_1X, T>( src, dst, nAttr );
	}
	else
	{
		if ( nSrcW == ( nDstW << 2 ) )
		{
			ResampleAttributeInternal<HMODE_DOWNSAMPLE_4X, T>( src, dst, nAttr );
		}
		else
		{
			if ( nSrcW == ( nDstW << 1 ) )
			{
				ResampleAttributeInternal<HMODE_DOWNSAMPLE_2X, T>( src, dst, nAttr );
			}
		}
	}
}

void CSOAContainer::ResampleAttribute( CSOAContainer &src, int nAttr )
{
	if ( m_nDataType[nAttr] == ATTRDATATYPE_FLOAT )
	{
		ResampleAttributeInternalDType<fltx4>( src, *this, nAttr );
	}
	else
	{
		if ( m_nDataType[nAttr] == ATTRDATATYPE_4V )
		{
			ResampleAttributeInternalDType<FourVectors>( src, *this, nAttr );
		}
	}

}



struct KMeansQuantizationWorkUnit
{
	CSOAContainer *m_pContainer;
	int m_nRowIndex;
	int m_nNumResultsDesired;
	IKMeansErrorMetric *m_pErrorCalculator;
	int const *m_pFieldIndices;
	int m_nNumFields;
	int m_nFieldToStoreIndexInto;
	KMeansQuantizedValue *m_pOutValues;
	int m_nErrorChannel;
	void Process( void );
};



static void DoKMeansWork( KMeansQuantizationWorkUnit &jobDesc )
{
	jobDesc.Process();
}

void KMeansQuantizationWorkUnit::Process( void )
{
	FourVectors v4SamplePositions;
	for( int nZ = 0; nZ < m_pContainer->NumSlices(); nZ++ )
	{
		v4SamplePositions.z = ReplicateX4( nZ );
		for( int nY = m_nRowIndex; nY < m_pContainer->NumRows(); nY += QUANTIZER_NJOBS )
		{
			v4SamplePositions.y = ReplicateX4( nY );
			KMeansSampleDescriptor samples;
			for( int c = 0; c < m_nNumFields; c++ )
			{
				samples.m_pInputValues[c] = m_pContainer->RowPtr<fltx4>( m_pFieldIndices[c], nY, nZ );
			}
			fltx4 *pIndexOut = m_pContainer->RowPtr<fltx4>( m_nFieldToStoreIndexInto, nY, nZ );
			fltx4 *pErrOut = NULL;
			if ( m_nErrorChannel != -1 )
			{
				pErrOut = m_pContainer->RowPtr<fltx4>( m_nErrorChannel, nY, nZ );
			}
			v4SamplePositions.x = g_SIMD_0123;

			// simd closest match search
			int nXSize = m_pContainer->NumQuadsPerRow();
			do
			{
				fltx4 fl4SampleIdx = Four_Zeros;
				fltx4 fl4ClosestError = Four_FLT_MAX;
				fltx4 fl4BestSampleIdx = Four_Zeros;
				for( int n = 0; n < m_nNumResultsDesired; n++ )
				{
					fltx4 fl4TrialError;
					m_pErrorCalculator->CalculateError( samples, v4SamplePositions, m_pOutValues[n], &fl4TrialError );
					// find which samples got a closest match from this comparison
					bi32x4 fl4BetterMask = CmpLeSIMD( fl4TrialError, fl4ClosestError );
					fl4BestSampleIdx = MaskedAssign( fl4BetterMask, fl4SampleIdx, fl4BestSampleIdx );
					fl4ClosestError = MaskedAssign( fl4BetterMask, fl4TrialError, fl4ClosestError );
					fl4SampleIdx = AddSIMD( fl4SampleIdx, Four_Ones );
				}
				// now, we have found the best match for 4 sample values. Need to update output indices and statistics
				*( pIndexOut++ ) = fl4BestSampleIdx;
				if ( pErrOut )
				{
					*( pErrOut++ ) = fl4ClosestError;
				}

				// unfortunately, we can not quite simd this because of needing scatter
				for( int s = 0; s < 4; s++ )
				{
					int nIdx = ( int )SubFloat( fl4BestSampleIdx, s );
					for( int c = 0; c < m_nNumFields; c++ )
					{
						m_pOutValues[nIdx].m_flValueAccumulators[m_nRowIndex][c] += SubFloat( *samples.m_pInputValues[c], s );
					}
					m_pOutValues[nIdx].m_flWeightAccumulators[m_nRowIndex] += 1.0;
				}

				for( int c = 0; c < m_nNumFields; c++ )
				{
					samples.m_pInputValues[c]++;
				}
				fl4SampleIdx = AddSIMD( fl4SampleIdx, Four_Ones );
				v4SamplePositions.x = AddSIMD( v4SamplePositions.x, Four_Fours );

			} while( -- nXSize );
		}
	}
}

// kmeans quantization
void CSOAContainer:: KMeansQuantization( int const *pFieldIndices, int nNumFields, 
										 KMeansQuantizedValue *pOutValues,
										 int nNumResultsDesired, IKMeansErrorMetric *pErrorCalculator,
										 int nFieldToStoreIndexInto, int nNumIterations,
										 int nChannelToReceiveErrorSignal )
{
	// first, initialize trial samples randomly
	HaltonSequenceGenerator_t xSequence( 13 );
	HaltonSequenceGenerator_t ySequence( 17 );
	HaltonSequenceGenerator_t zSequence( 23 );
	for( int i = 0; i < nNumResultsDesired; i++ )
	{
		int nX = ( int )( ( NumCols() - 1 ) * xSequence.NextValue() );
		int nY = ( int )( ( NumRows() - 1 ) * ySequence.NextValue() );
		int nZ = ( int )( ( NumSlices() - 1 ) * zSequence.NextValue() );
		pOutValues[i].m_vecValuePosition.DuplicateVector( Vector( nX, nY, nZ ) );
		for( int c = 0; c < nNumFields; c++ )
		{
			pOutValues[i].m_fl4Values[c] = ReplicateX4( FloatValue( pFieldIndices[c], nX, nY, nZ ) );
		}
	}

	// now,. run iterations
	while( nNumIterations-- )
	{
		for( int i = 0; i < nNumResultsDesired; i++ )
		{
			memset( pOutValues[i].m_flValueAccumulators, 0, sizeof( pOutValues[i].m_flValueAccumulators ) );
			memset( pOutValues[i].m_flWeightAccumulators, 0, sizeof( pOutValues[i].m_flWeightAccumulators ) );
		}
		// now, find the closest matches for all data samples, in parallel
		KMeansQuantizationWorkUnit jobs[QUANTIZER_NJOBS];
		for( int i = 0; i < QUANTIZER_NJOBS; i++ )
		{
			jobs[i].m_pContainer = this;
			jobs[i].m_nRowIndex = i;
			jobs[i].m_nNumResultsDesired = nNumResultsDesired;
			jobs[i].m_pErrorCalculator = pErrorCalculator;
			jobs[i].m_pFieldIndices = pFieldIndices;
			jobs[i].m_nNumFields = nNumFields;
			jobs[i].m_nFieldToStoreIndexInto = nFieldToStoreIndexInto;
			jobs[i].m_pOutValues = pOutValues;
			jobs[i].m_nErrorChannel = nChannelToReceiveErrorSignal;
		}
		ParallelProcess( jobs, ARRAYSIZE( jobs ), DoKMeansWork );
		if ( nNumIterations )						// don't refine the results after the last pass
		{
			for( int n = 0; n < nNumResultsDesired; n++ )
			{
				// accumulate over all threads
				for( int j = 1; j < QUANTIZER_NJOBS; j++ )
				{
					pOutValues[n].m_flWeightAccumulators[0] += pOutValues[n].m_flWeightAccumulators[j];
					for( int c = 0; c < nNumFields; c++ )
					{
						pOutValues[n].m_flValueAccumulators[0][c] += pOutValues[n].m_flValueAccumulators[j][c];
					}
				}
				// re-adjust quantized values
				float flOOWeight = 1.0 / MAX( FLT_EPSILON, pOutValues[n].m_flWeightAccumulators[0] );
				for( int c = 0; c < nNumFields; c++ )
				{
					pOutValues[n].m_fl4Values[c] = ReplicateX4( pOutValues[n].m_flValueAccumulators[0][c] * flOOWeight );
				}				
				pErrorCalculator->PostAdjustQuantizedValue( pOutValues[n] );
			}
			pErrorCalculator->PostStep( pFieldIndices, nNumFields, pOutValues, nNumResultsDesired, nFieldToStoreIndexInto, *this );
		}
	}
}

#define THRESH 0.9

void CSOAContainer::UpdateDistanceRow( int nSearchRadius, int nMinX, int nMaxX, int nY, int nZ,
									   int nSrcField, int nDestField )
{
	float const *pDataIn = RowPtr<float>( nSrcField, nY, nZ ) + nMinX;
	float *pDataOut = RowPtr<float>( nDestField, nY, nZ ) + nMinX;

	int nStartY = MAX( 0, nY - nSearchRadius );
	int nEndY = MIN( NumRows() - 1, nY + nSearchRadius );

	int nStartZ = MAX( 0, nZ - nSearchRadius );
	int nEndZ = MIN( NumSlices() - 1, nZ + nSearchRadius );

	fltx4 fl4Thresh = ReplicateX4( THRESH );
	for( int x = nMinX; x <= nMaxX; x++ )
	{
		float flReferenceValue = *( pDataIn++ );
		// map it to 0 or 1
		fltx4 fl4ReferenceValue = ( flReferenceValue > THRESH ) ? Four_Ones: Four_Zeros;
		fltx4 fl4ClosestDistance = ReplicateX4( nSearchRadius );
		// now, we need to walk over a (3d) window around the sample
		int nStartX = MAX( 0, x - nSearchRadius );
		int nEndX = MIN( NumCols() - 1, x + nSearchRadius );
		// pad to simd values
		nStartX = nStartX & ~3;
		nEndX = nEndX & ~3;
		int nCount = 1 + ( ( nEndX - nStartX ) / 4 );
		for( int z1 = nStartZ; z1 <= nEndZ; z1++ )
		{
			for( int y1 = nStartY; y1 <= nEndY; y1++ )
			{
				fltx4 fl4YZDist = ReplicateX4( ( y1 - nY ) * ( y1 - nY ) + ( z1 - nZ ) * ( z1 - nZ ) );
				fltx4 fl4SrcXDiff = AddSIMD( ReplicateX4( nStartX - x ), g_SIMD_0123 );
				fltx4 *pfl4SrcData = RowPtr<fltx4>( nSrcField, y1, z1 ) + ( nStartX / 4 );
				for( int x1 = 0; x1 < nCount; x1++ )
				{
					// fetch the source data, mapping it to 1 or 0.
					fltx4 fl4SrcData = *( pfl4SrcData++ );
					fl4SrcData = MaskedAssign( CmpGtSIMD( fl4SrcData, fl4Thresh ), Four_Ones, Four_Zeros );
					fltx4 fl4Distance = SqrtSIMD( AddSIMD( MulSIMD( fl4SrcXDiff, fl4SrcXDiff ), fl4YZDist ) );
					fl4ClosestDistance = MaskedAssign( 
						AndNotSIMD( CmpEqSIMD( fl4SrcData, fl4ReferenceValue ), CmpLtSIMD( fl4Distance, fl4ClosestDistance ) ),
						fl4Distance, fl4ClosestDistance );
					fl4SrcXDiff = AddSIMD( fl4SrcXDiff, Four_Fours );
				}
			}
		}
		// we have found the closest different voxel. store it
		float flClosestDistance = MIN( MIN( SubFloat( fl4ClosestDistance, 0 ), SubFloat( fl4ClosestDistance, 1 ) ),
									   MIN( SubFloat( fl4ClosestDistance, 2 ), SubFloat( fl4ClosestDistance, 3 ) ) );
		flClosestDistance = MIN( flClosestDistance, nSearchRadius );
		if ( flReferenceValue <= THRESH )
		{
			flClosestDistance = -flClosestDistance;
		}
		*( pDataOut++ ) = flClosestDistance;
	}
}



void CSOAContainer::GenerateDistanceField( int nSrcField, int nDestField,
										   int nMaxDistance,
										   Rect3D_t *pRect )
{
	int nMinX, nMaxX, nMinY, nMaxY, nMinZ, nMaxZ;

	if ( pRect )
	{
		nMinX = pRect->x;
		nMinY = pRect->y;
		nMinZ = pRect->z;
		nMaxX = nMinX + pRect->width - 1;
		nMaxY = nMinY + pRect->height - 1;
		nMaxZ = nMinZ + pRect->depth;
	}
	else
	{
		nMinX = nMinY = nMinZ = 0;
		nMaxX = NumCols() - 1;
		nMaxY = NumRows() - 1;
		nMaxZ = NumSlices() - 1;
	}
	nMinX -= nMaxDistance;
	nMinZ -= nMaxDistance;
	nMinY -= nMaxDistance;
	nMinX = MAX( 0, nMinX );
	nMinY = MAX( 0, nMinY );
	nMinZ = MAX( 0, nMinZ );

	nMaxX += nMaxDistance;
	nMaxY += nMaxDistance;
	nMaxZ += nMaxDistance;
	nMaxX = MIN( NumCols() - 1, nMaxX );
	nMaxY = MIN( NumRows() - 1, nMaxY );
	nMaxZ = MIN( NumSlices() - 1, nMaxZ );

	if ( pRect )											// update rect?
	{
		pRect->x = nMinX;
		pRect->y = nMinY;
		pRect->z = nMaxZ;
		pRect->width = 1 + nMaxX - nMinX;
		pRect->height = 1 + nMaxY - nMinY;
		pRect->depth = 1 + nMaxZ - nMinZ;
	}

	CCallQueue workList;
	for( int z = nMinZ; z <= nMaxZ; z++ )
	{
		for( int y = nMinY; y <= nMaxY; y++ )
		{
			workList.QueueCall( this, &CSOAContainer::UpdateDistanceRow,
								nMaxDistance, nMinX, nMaxX, y, z, nSrcField, nDestField );
		}
	}
	workList.ParallelCallQueued();
}

void CSOAContainer::CopyRegionFrom( CSOAContainer const &src, int nSrcAttr, int nDestAttr,
									int nSrcMinX, int nSrcMaxX, int nSrcMinY, int nSrcMaxY, int nSrcMinZ, int nSrcMaxZ,
									int nDestX, int nDestY, int nDestZ )
{
	Assert( HasAllocatedMemory( nDestAttr ) );
	Assert( src.HasAllocatedMemory( nSrcAttr ) );
	Assert( ItemByteStride( nDestAttr ) == src.ItemByteStride( nSrcAttr ) );
	size_t nRowSize = ( 1 + nSrcMaxX - nSrcMinX ) * ItemByteStride( nDestAttr );
	for( int z = nSrcMinZ; z <= nSrcMaxZ; z++ )
	{
		for( int y = nSrcMinY; y <= nSrcMaxY; y++ )
		{
			uint8 const *pSrc = src.RowPtr<uint8>( nSrcAttr, y,z ) + nSrcMinX * ItemByteStride( nDestAttr );
			uint8 *pDest = RowPtr<uint8>( nDestAttr, y + nDestY - nSrcMinY, z + nDestZ - nSrcMinZ ) + nDestX * ItemByteStride( nDestAttr );
			memcpy( pDest, pSrc, nRowSize );
		}
	}
}

void CSOAContainer::CopyRegionFrom( CSOAContainer const &src, 
									int nSrcMinX, int nSrcMaxX, int nSrcMinY, int nSrcMaxY, int nSrcMinZ, int nSrcMaxZ,
									int nDestX, int nDestY, int nDestZ )
{
	for( int i = 0; i < MAX_SOA_FIELDS; i++ )
	{
		if ( src.HasAllocatedMemory( i ) && ( HasAllocatedMemory( i ) ) && ( ItemByteStride( i ) == src.ItemByteStride( i ) ) )
		{
			CopyRegionFrom( src, i, i, nSrcMinX, nSrcMaxX, nSrcMinY, nSrcMaxY, nSrcMinZ, nSrcMaxZ, nDestX, nDestY, nDestZ );
		}
	}
}


