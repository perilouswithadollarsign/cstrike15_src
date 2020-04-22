//--------------------------------------------------------------------------------------------------
// qhMath.cpp
//
// Copyright(C) 2011 by D. Gregorius. All rights reserved.
//--------------------------------------------------------------------------------------------------
#include "qhMath.h"


//--------------------------------------------------------------------------------------------------
// Local utilities
//--------------------------------------------------------------------------------------------------
static inline qhBounds3 qhComputeBounds( const qhArray< qhVector3 >& Vertices, const qhTransform& Transform ) 
{
	qhBounds3 Bounds = QH_BOUNDS3_EMPTY;
	for ( int Index = 0; Index < Vertices.Size(); ++Index )
	{
		Bounds += qhTMul( Transform, Vertices[ Index ] );
	}

	return Bounds;
}


//--------------------------------------------------------------------------------------------------
// qhBox
//--------------------------------------------------------------------------------------------------
void qhBox::GetVertices( qhVector3 Vertices[ 8 ] ) const
{
	Vertices[ 0 ] = Orientation * qhVector3(  Extent.X, -Extent.Y,  Extent.Z ) + Center;
	Vertices[ 1 ] = Orientation * qhVector3( -Extent.X, -Extent.Y,  Extent.Z ) + Center;
	Vertices[ 2 ] = Orientation * qhVector3( -Extent.X, -Extent.Y, -Extent.Z ) + Center;
	Vertices[ 3 ] = Orientation * qhVector3(  Extent.X, -Extent.Y, -Extent.Z ) + Center;
	Vertices[ 4 ] = Orientation * qhVector3(  Extent.X,  Extent.Y,  Extent.Z ) + Center;
	Vertices[ 5 ] = Orientation * qhVector3( -Extent.X,  Extent.Y,  Extent.Z ) + Center;
	Vertices[ 6 ] = Orientation * qhVector3( -Extent.X,  Extent.Y, -Extent.Z ) + Center;
	Vertices[ 7 ] = Orientation * qhVector3(  Extent.X,  Extent.Y, -Extent.Z ) + Center;
}


//--------------------------------------------------------------------------------------------------
qhBox qhBestFit( const qhArray< qhVector3 >& Vertices, qhReal Threshold )
{
	qhVector3 Center( 0, 0, 0 );
	for ( int Index = 0; Index < Vertices.Size(); ++Index )
	{
		Center += Vertices[ Index ];
	}
	Center = Center / float( Vertices.Size() );

	qhBox BestBox;
	qhReal BestVolume = QH_REAL_MAX;

	qhReal Sweep = qhReal( 45 );
	qhVector3 SweepCenter( 0, 0, 0 );

	while ( Sweep >= Threshold )
	{
		bool Improved = false;
		qhVector3 BestAngles;

		static const qhReal kSteps = 7.0;
		qhReal StepSize = Sweep / kSteps;

		for ( qhReal X = SweepCenter.X - Sweep; X <= SweepCenter.X + Sweep; X += StepSize )
		{
			for ( qhReal Y = SweepCenter.Y - Sweep; Y <= SweepCenter.Y + Sweep; Y += StepSize )
			{
				for ( qhReal Z = SweepCenter.Z - Sweep; Z <= SweepCenter.Z + Sweep; Z += StepSize )
				{
					qhQuaternion Rx = qhRotationX( X * QH_DEG2RAD );
					qhQuaternion Ry = qhRotationY( Y * QH_DEG2RAD );
					qhQuaternion Rz = qhRotationZ( Z * QH_DEG2RAD );
					qhQuaternion R = Rz * Ry * Rx;

					qhTransform Transform;
					Transform.Rotation = qhConvert( R );
					Transform.Translation = Center;

					qhBounds3 Bounds = qhComputeBounds( Vertices, Transform );

					float Volume = Bounds.GetVolume();
					if ( Volume < BestVolume )
					{
						BestBox.Center = Transform * Bounds.GetCenter();
						BestBox.Orientation = R;
						BestBox.Extent = Bounds.GetExtent();

						BestVolume = Volume;
						BestAngles = qhVector3( X, Y, Z );

						Improved = true;
					}
				}
			}
		}

		if ( Improved )
		{
			SweepCenter = BestAngles;
			Sweep *= 0.5f;
		}
		else
		{
			break;
		}
	}

	return BestBox;
}


//--------------------------------------------------------------------------------------------------
qhBox qhBestFit( int VertexCount, const void* VertexBase, int VertexStride, qhReal Threshold )
{
	qhArray< qhVector3 > Vertices;
	Vertices.Resize( VertexCount );

	for ( int Index = 0; Index < VertexCount; ++Index )
	{
		Vertices[ Index ] = qhVector3( reinterpret_cast< const qhReal* >( VertexBase ) );
		VertexBase = qhAddByteOffset( VertexBase, VertexStride );
	}

	return qhBestFit( Vertices, Threshold );
}