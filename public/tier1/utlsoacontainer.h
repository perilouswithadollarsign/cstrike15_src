//====== Copyright © 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
// A Fixed-allocation class for maintaining a 1d or 2d or 3d array of data in a structure-of-arrays
// (SOA) sse-friendly manner.
// =============================================================================//

#ifndef UTLSOACONTAINER_H
#define UTLSOACONTAINER_H

#ifdef _WIN32
#pragma once
#endif


#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "tier1/utlmemory.h"
#include "tier1/utlblockmemory.h"
#include "mathlib/ssemath.h"



// strided pointers. gives you a class that acts like a pointer, but the ++ and += operators do the
// right thing
template<class T> class CStridedPtr
{
protected:
	T *m_pData;
	size_t m_nStride;
	
public:
	FORCEINLINE CStridedPtr<T>( void *pData, size_t nByteStride )
	{
		m_pData = reinterpret_cast<T *>( pData );
		m_nStride = nByteStride / sizeof( T );
	}

	FORCEINLINE CStridedPtr<T>( void ) {}
	T *operator->(void) const
	{
		return m_pData;
	}
	
	T & operator*(void) const
	{
		return *m_pData;
	}
	
	FORCEINLINE operator T *(void)
	{
		return m_pData;
	}

	FORCEINLINE CStridedPtr<T> & operator++(void)
	{
		m_pData += m_nStride;
		return *this;
	}

	FORCEINLINE void operator+=( size_t nNumElements )
	{
		m_pData += nNumElements * m_nStride;
	}

	FORCEINLINE size_t Stride( void ) const
	{
		return m_nStride;

	}

};

template<class T> class CStridedConstPtr
{
protected:
	const T *m_pData;
	size_t m_nStride;

public:
	FORCEINLINE CStridedConstPtr<T>( void const *pData, size_t nByteStride )
	{
		m_pData = reinterpret_cast<T const *>( pData );
		m_nStride = nByteStride / sizeof( T );
	}

	FORCEINLINE CStridedConstPtr<T>( void ) {}

	const T *operator->(void) const
	{
		return m_pData;
	}

	const T & operator*(void) const
	{
		return *m_pData;
	}

	FORCEINLINE operator const T *(void) const
	{
		return m_pData;
	}

	FORCEINLINE CStridedConstPtr<T> &operator++(void)
	{
		m_pData += m_nStride;
		return *this;
	}
	FORCEINLINE void operator+=( size_t nNumElements )
	{
		m_pData += nNumElements*m_nStride;
	}
	FORCEINLINE size_t Stride( void ) const
	{
		return m_nStride;

	}
};

// allowed field data types. if you change these values, you need to change the tables in the .cpp file
enum EAttributeDataType
{
	ATTRDATATYPE_NONE = -1,		// pad and varargs ender
	ATTRDATATYPE_FLOAT = 0,		// a float attribute
	ATTRDATATYPE_4V,			// vector data type, stored as class FourVectors
	ATTRDATATYPE_INT,			// integer. not especially sse-able on all architectures.
	ATTRDATATYPE_POINTER,		// a pointer.

	ATTRDATATYPE_COUNT,
};

#define MAX_SOA_FIELDS 32

class KMeansQuantizedValue;
class IKMeansErrorMetric;

typedef fltx4 (*UNARYSIMDFUNCTION)( fltx4 const & );
typedef fltx4 (*BINARYSIMDFUNCTION)( fltx4 const &, fltx4 const & );

class CSOAAttributeReference;



/// mode of threading for a container. Normalyy automatically set based upon dimensions, but
/// controllable via SetThreadMode.
enum SOAThreadMode_t
{
	SOATHREADMODE_NONE = 0,
	SOATHREADMODE_BYROWS = 1,
	SOATHREADMODE_BYSLICES = 2,
	SOATHREADMODE_BYROWS_AND_SLICES = 3,

	SOATHREADMODE_AUTO = -1,								// compute based upon dimensions
};


class CSOAContainer
{
	friend class CSOAAttributeReference;

public:
	// Constructor, destructor
	CSOAContainer( void );							// an empty one with no attributes
	CSOAContainer( int nCols, int nRows, int nSlices, ... );
	~CSOAContainer( void );

	// !!!!! UPDATE SERIALIZATION CODE WHENEVER THE STRUCTURE OF CSOAContainer CHANGES !!!!!
	// To avoid dependency on datamodel, serialization is implemented in utlsoacontainer_serialization.cpp, in dmxloader.lib
	//bool Serialize( CDmxElement *pRootElement );
	//bool Unserialize( const CDmxElement *pRootElement );


	// Set the data type for an attribute. If you set the data type, but tell it not to allocate,
	// the data type will be set but writes will assert, and reads will give you back zeros.  if
	// AllocateData hasn't been called yet, this will set up for AllocateData to reserve space for
	// this attribute. If you have already called AllocateData, but wish to add an attribute, you
	// can also use this, which will result in separate memory being allocated for this attribute.
	void SetAttributeType( int nAttrIdx, EAttributeDataType nDataType, bool bAllocateMemory = true );
	EAttributeDataType GetAttributeType( int nAttrIdx ) const;


	// Set the attribute type for a field, if that field is not already present (potentially
	// allocating memory). You can use this, for instance, to make sure an already loaded image has
	// an alpha channel.
	void EnsureDataType( int nAttrIdx, EAttributeDataType nDataType );

	// set back to un-initted state, freeing memory
	void Purge( void );

	// Allocate, purge data
	void AllocateData( int nNCols, int nNRows, int nSlices = 1 ); // actually allocate the memory and set the pointers up
	void PurgeData( void );							

	// Did the container allocate memory for this attribute?
	bool HasAllocatedMemory( int nAttrIdx ) const;

	// easy constructor for 2d using varargs. call like
	// #define ATTR_RED 0
	// #define ATTR_GREEN 1
	// #define ATTR_BLUE 2
	// CSOAContainer myimage( 256, 256, ATTR_RED, ATTRDATATYPE_FLOAT, ATTR_GREEN, ATTRDATATYPE_FLOAT,
	//                        ATTR_BLUE, ATTRDATATYPE_FLOAT, -1 );

	int NumCols( void ) const;
	int NumRows( void ) const;
	int NumSlices( void ) const;
	void AssertDataType( int nAttrIdx, EAttributeDataType nDataType ) const;

	// # of groups of 4 elements per row
	int NumQuadsPerRow( void ) const;
	int Count( void ) const;					// for 1d data
	int NumElements( void ) const;

	// how much to step to go from the end of one row to the start of the next one. Basically, how
	// many bytes to add at the end of a row when iterating over the whole 2d array with ++
	size_t RowToRowStep( int nAttrIdx ) const;
	template<class T> T *RowPtr( int nAttributeIdx, int nRowNumber, int nSliceNumber = 0 ) const;
	void const *ConstRowPtr( int nAttributeIdx, int nRowNumber, int nSliceNumber = 0 ) const;
	template<class T> T *ElementPointer( int nAttributeIdx, int nX = 0, int nY = 0, int nZ = 0 ) const;
	FourVectors *ElementPointer4V( int nAttributeIdx, int nX = 0, int nY = 0, int nZ = 0 ) const;
	size_t ItemByteStride( int nAttributeIdx ) const;

	FORCEINLINE float &FloatValue( int nAttrIdx, int nX, int nY, int nZ ) const
	{
		AssertDataType( nAttrIdx, ATTRDATATYPE_FLOAT );
		return RowPtr<float>( nAttrIdx, nY, nZ )[nX];
	}

	// return a reference to an attribute, which can have operations performed on it. For instance,
	// this is valid code to zero out the red component of a whole image:
	//  myImage[FBM_ATTR_RED] = 0.;
	CSOAAttributeReference operator[]( int nAttrIdx );

	// this is just an alias for readbaility w/ ptrs. instead of (*p)[FBM_ATTR_RED], you can do p->Attr( FBM_ATTR_RED );
	FORCEINLINE CSOAAttributeReference Attr( int nAttrIdx );

	// copy the attribute data from another soacontainer. must be compatible geometry. 
	void CopyAttrFrom( CSOAContainer const &other, int nDestAttributeIdx, int nSrcAttributeIndex = -1 );

	// copy the attribute data from another attribute. must be compatible data format
	void CopyAttrToAttr( int nSrcAttributeIndex, int nDestAttributeIndex);

	// copy a subvolume of attribute data from one container to another.
	void CopyRegionFrom( CSOAContainer const &src, int nSrcAttr, int nDestAttr,
						 int nSrcMinX, int nSrcMaxX, int nSrcMinY, int nSrcMaxY, int nSrcMinZ, int nSrcMaxZ,
						 int nDestX, int nDestY, int nDestZ );

	// copy all fields from a region of src to this.
	void CopyRegionFrom( CSOAContainer const &src, 
						 int nSrcMinX, int nSrcMaxX, int nSrcMinY, int nSrcMaxY, int nSrcMinZ, int nSrcMaxZ,
						 int nDestX, int nDestY, int nDestZ );

	// move all the data from one csoacontainer to another, leaving the source empty.  this is just
	// a pointer copy.
	FORCEINLINE void MoveDataFrom( CSOAContainer other );

	// arithmetic and data filling functions. All SIMD and hopefully fast

	/// set all elements of a float attribute to random #s
	void RandomizeAttribute( int nAttr, float flMin, float flMax ) const;

	/// this.attr = vec
	void FillAttr( int nAttr, Vector const &vecValue );

	/// this.attr = float
	void FillAttr( int nAttr, float flValue );

	/// this.nDestAttr *= src.nSrcAttr
	void MulAttr( CSOAContainer const &src, int nSrcAttr, int nDestAttr );

	/// Returns the result of repeatedly combining attr values with the initial value using the specified function.
	/// For instance, SumAttributeValue is just ReduceAttr<AddSIMD>( attr, FOUR_ZEROS );
	template<BINARYSIMDFUNCTION fn> float ReduceAttr( int nSrcAttr, fltx4 const &fl4InitialValue ) const;

	template<BINARYSIMDFUNCTION fn> void ApplyBinaryFunctionToAttr( int nDestAttr, fltx4 const &flFnArg1 );

	/// this.attr = fn1( fn2( attr, arg2 ), arg1 )
	template<BINARYSIMDFUNCTION fn1, BINARYSIMDFUNCTION fn2> void ApplyTwoComposedBinaryFunctionsToAttr( int nDestAttr, fltx4 const &flFnArg1, fltx4 const &flFnArg2 );

	/// this.nDestAttr *= flValue
	void MulAttr( int nDestAttr, float flScale )
	{
		ApplyBinaryFunctionToAttr<MulSIMD>( nDestAttr, ReplicateX4( flScale ) );
	}

	void AddToAttr( int nDestAttr, float flAddend )
	{
		ApplyBinaryFunctionToAttr<AddSIMD>( nDestAttr, ReplicateX4( flAddend ) );
	}

	// this.attr = max( this.attr, flminvalue )
	void MaxAttr( int nDestAttr, float flMinValue )
	{
		ApplyBinaryFunctionToAttr<MaxSIMD>( nDestAttr, ReplicateX4( flMinValue ) );
	}

	/// this.attr = min( this.attr, flminvalue )
	void MinAttr( int nDestAttr, float flMaxValue )
	{
		ApplyBinaryFunctionToAttr<MinSIMD>( nDestAttr, ReplicateX4( flMaxValue ) );
	}

	void ClampAttr( int nDestAttr, float flMinValue, float flMaxValue )
	{
		ApplyTwoComposedBinaryFunctionsToAttr<MinSIMD, MaxSIMD>( nDestAttr, ReplicateX4( flMaxValue ), ReplicateX4( flMinValue ) );
	}

	/// this.attr = normalize( this.attr )
	void NormalizeAttr( int nAttr );

	/// fill 2d a rectangle with values interpolated from 4 corner values. 
	void FillAttrWithInterpolatedValues( int nAttr, float flValue00, float flValue10, float flValue01, float flValue11 ) const;
	void FillAttrWithInterpolatedValues( int nAttr, Vector flValue00, Vector flValue10,
										 Vector const &flValue01, Vector const &flValue11 ) const;

	/// grab 3 scalar attributes from one csoaa and fill in a fourvector attr in.
	void PackScalarAttributesToVectorAttribute( CSOAContainer *pInput,
												int nVecAttributeOut,
												int nScalarAttributeX,
												int nScalarAttributeY,
												int nScalarAttributeZ );

	/// grab the 3 components of a vector attribute and store in 3 scalar attributes.
	void UnPackVectorAttributeToScalarAttributes( CSOAContainer *pInput,
												  int nVecAttributeIn,
												  int nScalarAttributeX,
												  int nScalarAttributeY,
												  int nScalarAttributeZ );
	
	/// this.attrout = src.attrin * vec (component by component )
	void MultiplyVectorAttribute( CSOAContainer *pInput, int nAttributeIn, Vector const &vecScalar, int nAttributeOut );

	/// Given an soa container of a different dimension, resize one attribute from it to fit this
	/// table's geometry. point sampling only
	void ResampleAttribute( CSOAContainer &pInput, int nAttr );

	/// sum of all floats in an attribute
    float SumAttributeValue( int nAttr ) const;

	/// sum(attr) / ( w * h * d )
	float AverageFloatAttributeValue( int nAttr ) const;

    /// maximum float value in a float attr
	float MaxAttributeValue( int nAttr ) const;

    /// minimum float value in a float attr
    float MinAttributeValue( int nAttr ) const;


	/// scalartargetattribute += w*exp( vecdir dot ndirection)
	void AddGaussianSRBF( float flWeight, Vector vecDir, int nDirectionAttribute, int nScalarTargetAttribute );

	/// vec3targetattribute += w*exp( vecdir dot ndirection)
	void AddGaussianSRBF( Vector vecWeight, Vector vecDir, int nDirectionAttribute, 
						  int nVectorTargetAttribute );


	/// find the largest value of a vector attribute
	void FindLargestMagnitudeVector( int nAttr, int *nx, int *ny, int *nz );

	void KMeansQuantization( int const *pFieldIndices, int nNumFields,
							 KMeansQuantizedValue *pOutValues,
							 int nNumResultsDesired, IKMeansErrorMetric *pErrorCalculator,
							 int nFieldToStoreIndexInto, int nNumIterations,
							 int nChannelToReceiveErrorSignal = -1 );

	// Calculate the signed distance, in voxels, between all voxels and a surface boundary defined
	// by nSrcField being >0. Voxels with nSrcField <0 will end up with negative distances. Voxels
	// with nSrcField == 0 will get 0, and nSrcField >0 will yield positive distances.  Note the
	// min/max x/y/z fields don't reflect the range to be written, but rather represent the bounds
	// of updated voxels that you want your distance field modified to take into account. This
	// volume will be bloated based upon the nMaxDistance parameter and simd padding.  A
	// brute-force algorithm is used, but it is threaded and simd'd. Large "nMaxDistance" values
	// applied to large images can take a long time, as the execution time per output pixel is
	// proportional to maxdistance^2.  The rect argument, if passed, will be modified to be the
	// entire rectangle modified by the operation.
	void GenerateDistanceField( int nSrcField, int nDestField,
								int nMaxDistance,
								Rect3D_t *pRect = NULL );

	void SetThreadMode( SOAThreadMode_t eThreadMode );

protected:
	int m_nColumns;										// # of rows and columns created with
	int m_nRows;
	int m_nSlices;
	int m_nPaddedColumns;								// # of columns rounded up for sse
	int m_nNumQuadsPerRow;								// # of groups of 4 elements per row
	uint8 *m_pDataMemory;								// the actual data memory
	uint8 *m_pAttributePtrs[MAX_SOA_FIELDS];
	EAttributeDataType m_nDataType[MAX_SOA_FIELDS];
	size_t m_nStrideInBytes[MAX_SOA_FIELDS];			// stride from one field datum to another
	size_t m_nRowStrideInBytes[MAX_SOA_FIELDS];			// stride from one row datum to another per field
	size_t m_nSliceStrideInBytes[MAX_SOA_FIELDS];       // stride from one slice datum to another per field
	uint32 m_nFieldPresentMask;
	uint8 *m_pConstantDataMemory;
	uint8 *m_pSeparateDataMemory[MAX_SOA_FIELDS];			// for fields allocated separately from the main allocation
	SOAThreadMode_t m_eThreadMode;							// set thread mode

	FORCEINLINE void Init( void )
	{
		memset( m_nDataType, 0xff, sizeof( m_nDataType ) );
		memset( m_pSeparateDataMemory, 0, sizeof( m_pSeparateDataMemory ) );
		
#ifdef _DEBUG
		memset( m_pAttributePtrs, 0xFF, sizeof( m_pAttributePtrs ) );
		memset( m_nStrideInBytes, 0xFF, sizeof( m_nStrideInBytes ) );
		memset( m_nRowStrideInBytes, 0xFF, sizeof( m_nRowStrideInBytes ) );
		memset( m_nSliceStrideInBytes, 0xFF, sizeof( m_nSliceStrideInBytes ) );
#endif

		m_pConstantDataMemory = NULL;
		m_pDataMemory = 0;
		m_nNumQuadsPerRow = 0;
		m_nColumns = m_nPaddedColumns = m_nRows = m_nSlices = 0;
		m_nFieldPresentMask = 0;
		m_eThreadMode = SOATHREADMODE_NONE;
	}

	void UpdateDistanceRow( int nSearchRadius, int nMinX, int nMaxX, int nY, int nZ,
							int nSrcField, int nDestField );

	// parallel helper functions. These do the work, and all take a row/column range as their first arguments.
	void CopyAttrFromPartial( int nStartRow, int nNumRows, int nStartSlice, int nEndSlice, CSOAContainer const *pOther, int nDestAttributeIndex, int nSrcAttributeIndex );
	void FillAttrPartial( int nStartRow, int nNumRows, int nStartSlice, int nEndSlice, int nAttr, fltx4 fl4Value );

	// Allocation utility funcs (NOTE: all allocs are multiples of 16, and are aligned allocs)
	size_t DataMemorySize( void ) const;					// total bytes of data memory to allocate at m_pDataMemory (if all attributes were allocated in a single block)
	size_t ConstantMemorySize( void ) const;				// total bytes of constant memory to allocate at m_pConstantDataMemory (if all constant attributes were allocated in a single block)
	size_t AttributeMemorySize( int nAttrIndex ) const;		// total bytes of data memory allocated to a single attribute (constant or otherwise)
	void AllocateDataMemory( void );
	void AllocateConstantMemory( void );
};


// define binary op class to allow this construct without temps:
// dest( FBM_ATTR_RED ) = src( FBM_ATTR_BLUE ) + src( FBM_ATTR_GREEN )
template<BINARYSIMDFUNCTION fn> class CSOAAttributeReferenceBinaryOp;

class CSOAAttributeReference
{
	friend class CSOAContainer;

	class CSOAContainer *m_pContainer;
	int m_nAttributeID;

public:
	FORCEINLINE void operator *=( float flScale ) const
	{
		m_pContainer->MulAttr( m_nAttributeID, flScale );
	}
	FORCEINLINE void operator +=( float flAddend ) const
	{
		m_pContainer->AddToAttr( m_nAttributeID, flAddend );
	}
	FORCEINLINE void operator -=( float flAddend ) const
	{
		m_pContainer->AddToAttr( m_nAttributeID, -flAddend );
	}
	FORCEINLINE void operator =( float flValue ) const
	{
		m_pContainer->FillAttr( m_nAttributeID, flValue );
	}

	FORCEINLINE void operator =( CSOAAttributeReference const &other ) const
	{
		m_pContainer->CopyAttrFrom( *other.m_pContainer, m_nAttributeID, other.m_nAttributeID );
	}

	template<BINARYSIMDFUNCTION fn> FORCEINLINE void operator =( CSOAAttributeReferenceBinaryOp<fn> const &op );

	FORCEINLINE void CopyTo( CSOAAttributeReference &other ) const; // since operator= is over-ridden
};


// define binary op class to allow this construct without temps:
// dest( FBM_ATTR_RED ) = src( FBM_ATTR_BLUE ) + src( FBM_ATTR_GREEN )
template<BINARYSIMDFUNCTION fn> class CSOAAttributeReferenceBinaryOp
{
public:
	CSOAAttributeReference m_opA;
	CSOAAttributeReference m_opB;

	CSOAAttributeReferenceBinaryOp( CSOAAttributeReference const &a, CSOAAttributeReference const & b )
	{
		a.CopyTo( m_opA );
		b.CopyTo( m_opB );
	}

};

#define DEFINE_OP( opname, fnname )										\
	FORCEINLINE CSOAAttributeReferenceBinaryOp<fnname> operator opname( CSOAAttributeReference const &left, CSOAAttributeReference const &right )  \
{																		\
	return CSOAAttributeReferenceBinaryOp<fnname>( left, right );		\
}

// these operator overloads let you do
// dst[ATT1] = src1[ATT] + src2[ATT] with no temporaries generated
DEFINE_OP( +, AddSIMD );
DEFINE_OP( *, MulSIMD );
DEFINE_OP( -, SubSIMD );
DEFINE_OP( /, DivSIMD );


template<BINARYSIMDFUNCTION fn> FORCEINLINE void CSOAAttributeReference::operator =( CSOAAttributeReferenceBinaryOp<fn> const &op )
{
	m_pContainer->AssertDataType( m_nAttributeID, ATTRDATATYPE_FLOAT );
	fltx4 *pOut = m_pContainer->RowPtr<fltx4>( m_nAttributeID, 0 );
	
	// GCC on PS3 gets confused by this code, so we literally have to break it into multiple statements
	CSOAContainer *pContainerA = op.m_opA.m_pContainer;
	CSOAContainer *pContainerB = op.m_opB.m_pContainer;
	
	fltx4 *pInA = pContainerA->RowPtr< fltx4 >( op.m_opA.m_nAttributeID, 0 );
	fltx4 *pInB = pContainerB->RowPtr< fltx4 >( op.m_opB.m_nAttributeID, 0 );

	size_t nRowToRowStride = m_pContainer->RowToRowStep( m_nAttributeID ) / sizeof( fltx4 );
	int nRowCtr = m_pContainer->NumRows() * m_pContainer->NumSlices();
	do
	{
		int nColCtr = m_pContainer->NumQuadsPerRow();
		do
		{
			*(pOut++) = fn( *( pInA++ ), *( pInB++ ) );
		} while ( --nColCtr );
		pOut += nRowToRowStride;
		pInA += nRowToRowStride;
		pInB += nRowToRowStride;
	} while ( --nRowCtr );
}

FORCEINLINE void CSOAAttributeReference::CopyTo( CSOAAttributeReference &other ) const
{
	other.m_pContainer = m_pContainer;
	other.m_nAttributeID = m_nAttributeID;
}



FORCEINLINE CSOAAttributeReference CSOAContainer::operator[]( int nAttrIdx )
{
	CSOAAttributeReference ret;
	ret.m_pContainer = this;
	ret.m_nAttributeID = nAttrIdx;
	return ret;
}

FORCEINLINE CSOAAttributeReference CSOAContainer::Attr( int nAttrIdx )
{
	return (*this)[nAttrIdx];
}

template<BINARYSIMDFUNCTION fn1, BINARYSIMDFUNCTION fn2> void CSOAContainer::ApplyTwoComposedBinaryFunctionsToAttr( int nDestAttr, fltx4 const &fl4FnArg1, fltx4 const &fl4FnArg2 )
{
	if (  m_nDataType[nDestAttr] == ATTRDATATYPE_4V )
	{
		FourVectors *pOut = RowPtr<FourVectors>( nDestAttr, 0 );
		size_t nRowToRowStride = RowToRowStep( nDestAttr ) / sizeof( FourVectors );
		int nRowCtr = NumRows() * NumSlices();
		do
		{
			int nColCtr = NumQuadsPerRow();
			do
			{
				pOut->x = fn1( fn2( pOut->x, fl4FnArg2 ), fl4FnArg1 );
				pOut->y = fn1( fn2( pOut->y, fl4FnArg2 ), fl4FnArg1 );
				pOut->z = fn1( fn2( pOut->z, fl4FnArg2 ), fl4FnArg1 );
			} while ( --nColCtr );
			pOut += nRowToRowStride;
		} while ( --nRowCtr );
	}
	else
	{
		AssertDataType( nDestAttr, ATTRDATATYPE_FLOAT );
		fltx4 *pOut = RowPtr<fltx4>( nDestAttr, 0 );
		size_t nRowToRowStride = RowToRowStep( nDestAttr ) / sizeof( fltx4 );
		int nRowCtr = NumRows() * NumSlices();
		do
		{
			int nColCtr = NumQuadsPerRow();
			do
			{
				*pOut = fn1( fn2( *pOut, fl4FnArg2 ), fl4FnArg1 );
                pOut++;
			} while ( --nColCtr );
			pOut += nRowToRowStride;
		} while ( --nRowCtr );
	}
}

template<BINARYSIMDFUNCTION fn> void CSOAContainer::ApplyBinaryFunctionToAttr( int nDestAttr, fltx4 const &fl4FnArg1 )
{
	if (  m_nDataType[nDestAttr] == ATTRDATATYPE_4V )
	{
		FourVectors *pOut = RowPtr<FourVectors>( nDestAttr, 0 );
		size_t nRowToRowStride = RowToRowStep( nDestAttr ) / sizeof( FourVectors );
		int nRowCtr = NumRows() * NumSlices();
		do
		{
			int nColCtr = NumQuadsPerRow();
			do
			{
				pOut->x = fn( pOut->x, fl4FnArg1 );
				pOut->y = fn( pOut->y, fl4FnArg1 );
				pOut->z = fn( pOut->z, fl4FnArg1 );
			} while ( --nColCtr );
			pOut += nRowToRowStride;
		} while ( --nRowCtr );
	}
	else
	{
		AssertDataType( nDestAttr, ATTRDATATYPE_FLOAT );
		fltx4 *pOut = RowPtr<fltx4>( nDestAttr, 0 );
		size_t nRowToRowStride = RowToRowStep( nDestAttr ) / sizeof( fltx4 );
		int nRowCtr = NumRows() * NumSlices();
		do
		{
			int nColCtr = NumQuadsPerRow();
			do
			{
				*pOut = fn( *pOut, fl4FnArg1 );
                pOut++;
			} while ( --nColCtr );
			pOut += nRowToRowStride;
		} while ( --nRowCtr );
	}
}

template<BINARYSIMDFUNCTION fn> float CSOAContainer::ReduceAttr( int nSrcAttr, fltx4 const &fl4InitialValue ) const
{
	AssertDataType( nSrcAttr, ATTRDATATYPE_FLOAT );
	fltx4 fl4Result = fl4InitialValue;
	fltx4 const *pIn = RowPtr<fltx4>( nSrcAttr, 0 );
	size_t nRowToRowStride = RowToRowStep( nSrcAttr ) / sizeof( fltx4 );
	int nRowCtr = NumRows() * NumSlices();
	bi32x4 fl4LastColumnMask = (bi32x4)LoadAlignedSIMD( g_SIMD_SkipTailMask[NumCols() & 3 ] );
	do
	{
		for( int i = 0; i < NumQuadsPerRow() - 1; i++ )
		{
			fl4Result = fn( fl4Result, *( pIn++ ) );
		}
		// handle the last column in case its not a multiple of 4 wide
		fl4Result = MaskedAssign( fl4LastColumnMask, fn( fl4Result, *( pIn++ ) ), fl4Result );
		pIn += nRowToRowStride;
	} while ( --nRowCtr );
	// now, combine the subfields
	fl4Result = fn(
		fn( fl4Result, SplatYSIMD( fl4Result ) ),
		fn( SplatZSIMD( fl4Result ), SplatWSIMD( fl4Result ) ) );
	return SubFloat( fl4Result, 0 );
}



#define QUANTIZER_NJOBS 1			   // # of simultaneous subjobs to execute for kmeans quantizer

// kmeans quantization classes
// the array of quantized values returned by quantization
class KMeansQuantizedValue
{
public:
	FourVectors m_vecValuePosition;							// replicated
	fltx4 m_fl4Values[MAX_SOA_FIELDS];						// replicated
	
	float m_flValueAccumulators[QUANTIZER_NJOBS][MAX_SOA_FIELDS];
	float m_flWeightAccumulators[QUANTIZER_NJOBS];

	FORCEINLINE float operator()( int n )
	{
		return SubFloat( m_fl4Values[n], 0 );
	}

};

class KMeansSampleDescriptor
{
public:
	fltx4 *m_pInputValues[MAX_SOA_FIELDS];

	FORCEINLINE fltx4 const & operator()( int nField ) const
	{
		return *m_pInputValues[nField];
	}

};

class IKMeansErrorMetric
{
public:
	virtual void CalculateError( KMeansSampleDescriptor const &sampleAddresses,
								 FourVectors const &v4SamplePositions,
								 KMeansQuantizedValue const &valueToCompareAgainst,
								 fltx4 *pfl4ErrOut ) =0;

	// for things like normalization, etc
	virtual void PostAdjustQuantizedValue( KMeansQuantizedValue &valueToAdjust )
	{
	}

	// for global fixup after each adjustment step
	virtual void PostStep( int const *pFieldIndices, int nNumFields,
						   KMeansQuantizedValue *pValues, int nNumQuantizedValues,
						   int nIndexField, CSOAContainer &data )
	{
	}

};




FORCEINLINE CSOAContainer::CSOAContainer( void )
{
	Init();
}


//-----------------------------------------------------------------------------
// Did the container allocate memory for this attribute?
//-----------------------------------------------------------------------------
FORCEINLINE bool CSOAContainer::HasAllocatedMemory( int nAttrIdx ) const
{
	return ( m_nFieldPresentMask & ( 1 << nAttrIdx ) ) != 0;
}



FORCEINLINE EAttributeDataType CSOAContainer::GetAttributeType( int nAttrIdx ) const
{
	Assert( ( nAttrIdx >= 0 ) && ( nAttrIdx < MAX_SOA_FIELDS ) );
	return m_nDataType[nAttrIdx];
}

FORCEINLINE void CSOAContainer::EnsureDataType( int nAttrIdx, EAttributeDataType nDataType )
{
	if ( !HasAllocatedMemory( nAttrIdx ) )
	{
		SetAttributeType( nAttrIdx, nDataType );
	}
}

FORCEINLINE int CSOAContainer::NumRows( void ) const
{
	return m_nRows;
}

FORCEINLINE int CSOAContainer::NumCols( void ) const
{
	return m_nColumns;
}
FORCEINLINE int CSOAContainer::NumSlices( void ) const
{
	return m_nSlices;
}


FORCEINLINE void CSOAContainer::AssertDataType( int nAttrIdx, EAttributeDataType nDataType ) const
{
	Assert( nAttrIdx >= 0 );
	Assert( nAttrIdx < MAX_SOA_FIELDS );
	Assert( m_nDataType[ nAttrIdx ] == nDataType );
}


// # of groups of 4 elements per row
FORCEINLINE int CSOAContainer::NumQuadsPerRow( void ) const
{
	return m_nNumQuadsPerRow;
}

FORCEINLINE int CSOAContainer::Count( void ) const						// for 1d data
{
	return NumCols();
}

FORCEINLINE int CSOAContainer::NumElements( void ) const
{
	return NumCols() * NumRows() * NumSlices();
}


// how much to step to go from the end of one row to the start of the next one. Basically, how
// many bytes to add at the end of a row when iterating over the whole 2d array with ++
FORCEINLINE size_t CSOAContainer::RowToRowStep( int nAttrIdx ) const
{
	return 0;
}

template<class T> FORCEINLINE T *CSOAContainer::RowPtr( int nAttributeIdx, int nRowNumber, int nSliceNumber ) const
{
	Assert( nRowNumber < m_nRows );
	Assert( nAttributeIdx < MAX_SOA_FIELDS );
	Assert( m_nDataType[nAttributeIdx] != ATTRDATATYPE_NONE );
	Assert( ( m_nFieldPresentMask & ( 1 << nAttributeIdx ) ) || ( ( nRowNumber == 0 ) && ( nSliceNumber == 0 ) ) );
	return reinterpret_cast<T *>(
		m_pAttributePtrs[nAttributeIdx] + 
		+ nRowNumber * m_nRowStrideInBytes[nAttributeIdx]
	+ nSliceNumber * m_nSliceStrideInBytes[nAttributeIdx] );
}

FORCEINLINE void const *CSOAContainer::ConstRowPtr( int nAttributeIdx, int nRowNumber, int nSliceNumber ) const
{
	Assert( nRowNumber < m_nRows );
	Assert( nAttributeIdx < MAX_SOA_FIELDS );
	Assert( m_nDataType[nAttributeIdx] != ATTRDATATYPE_NONE );
	return m_pAttributePtrs[nAttributeIdx] 
	+ nRowNumber * m_nRowStrideInBytes[nAttributeIdx]
	+ nSliceNumber * m_nSliceStrideInBytes[nAttributeIdx];
}


template<class T> FORCEINLINE T *CSOAContainer::ElementPointer( int nAttributeIdx, int nX, int nY, int nZ ) const
{
	Assert( nAttributeIdx < MAX_SOA_FIELDS );
	Assert( nX < m_nColumns );
	Assert( nY < m_nRows );
	Assert( nZ < m_nSlices );
	Assert( m_nDataType[nAttributeIdx] != ATTRDATATYPE_NONE );
	Assert( m_nDataType[nAttributeIdx] != ATTRDATATYPE_4V );
	return reinterpret_cast<T *>( m_pAttributePtrs[nAttributeIdx] 
		+ nX * m_nStrideInBytes[nAttributeIdx]
		+ nY * m_nRowStrideInBytes[nAttributeIdx]
		+ nZ * m_nSliceStrideInBytes[nAttributeIdx]
	);
}

FORCEINLINE FourVectors *CSOAContainer::ElementPointer4V( int nAttributeIdx, int nX, int nY, int nZ ) const
{
	Assert( nAttributeIdx < MAX_SOA_FIELDS );
	Assert( nX < m_nColumns );
	Assert( nY < m_nRows );
	Assert( nZ < m_nSlices );
	Assert( m_nDataType[nAttributeIdx] == ATTRDATATYPE_4V );
	int nXIdx = nX / 4;
	uint8 *pRet =  m_pAttributePtrs[nAttributeIdx] 
		+ nXIdx * 4 * m_nStrideInBytes[nAttributeIdx]
		+ nY * m_nRowStrideInBytes[nAttributeIdx]
		+ nZ * m_nSliceStrideInBytes[nAttributeIdx];
	pRet += 4 * ( nX & 3 );
	return reinterpret_cast<FourVectors *>( pRet );
}
FORCEINLINE size_t CSOAContainer::ItemByteStride( int nAttributeIdx ) const
{
	Assert( nAttributeIdx < MAX_SOA_FIELDS );
	Assert( m_nDataType[nAttributeIdx] != ATTRDATATYPE_NONE );
	return m_nStrideInBytes[ nAttributeIdx ];
}

// move all the data from one csoacontainer to another, leaving the source empty.
// this is just a pointer copy.
FORCEINLINE void CSOAContainer::MoveDataFrom( CSOAContainer other )
{
	(*this) = other;
	other.Init();
}




class CFltX4AttributeIterator : public CStridedConstPtr<fltx4>
{
	FORCEINLINE CFltX4AttributeIterator( CSOAContainer const *pContainer, int nAttribute, int nRowNumber = 0 )
		: CStridedConstPtr<fltx4>( pContainer->ConstRowPtr( nAttribute, nRowNumber), 
								   pContainer->ItemByteStride( nAttribute ) )
	{
	}
};

class CFltX4AttributeWriteIterator : public CStridedPtr<fltx4>
{
	FORCEINLINE CFltX4AttributeWriteIterator( CSOAContainer const *pContainer, int nAttribute, int nRowNumber = 0 )
		: CStridedPtr<fltx4>( pContainer->RowPtr<uint8>( nAttribute, nRowNumber), 
							  pContainer->ItemByteStride( nAttribute ) )
	{
	}
	
};

FORCEINLINE FourVectors CompressSIMD( FourVectors const &a, FourVectors const &b )
{
	FourVectors ret;
	ret.x = CompressSIMD( a.x, b.x );
	ret.y = CompressSIMD( a.y, b.y );
	ret.z = CompressSIMD( a.z, b.z );
	return ret;
}

FORCEINLINE FourVectors Compress4SIMD( FourVectors const &a, FourVectors const &b,
									   FourVectors const &c, FourVectors const &d )
{
	FourVectors ret;
	ret.x = Compress4SIMD( a.x, b.x, c.x, d.x );
	ret.y = Compress4SIMD( a.y, b.y, c.y, d.y );
	ret.z = Compress4SIMD( a.z, b.z, c.z, d.z );
	return ret;
}



#endif
