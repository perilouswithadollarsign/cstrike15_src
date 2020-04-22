// svd.cpp : Defines the entry point for the console application.
//
#include "svd.h"

namespace SVD
{
	void Test()
	{
		SvdIterator<float> test;
		for ( int nTest = 0; nTest< 100; ++nTest )
		{
			Matrix3<float> a;
			for ( int i = 0; i < 3; ++i )
				for ( int j = 0; j < 3; ++j )
					a.m[ i ][ j ] = i * 3 + j;//float( rand() ) / RAND_MAX - 0.5f;

			test.Init( a );
			Msg( "%d", nTest );
			for ( int i = 0; i < 5; ++i )
			{
				Matrix3< float > v = test.ComputeV();
				// B = US = AV
				Matrix3< float > us = a * v;
				//float flOrtho = OrthogonalityError( us );
				Matrix3< float > reconstruction = MulT( us, v );
				//float flRec = ( reconstruction - a ).FrobeniusNorm();
				SymMatrix3< float > ata = AtA( a );
				float flOffDiagError = ata.OffDiagNorm() / ata.DiagNorm();

				// Msg( "\t%g", logf( flRec ) / logf( 10 ) );
				// Msg( "\t%g", logf( flOrtho ) / logf( 10 ) );
				Msg( "\t%g", logf( flOffDiagError ) / logf( 10 ) );

				test.Iterate( 1 );
			}
			Msg( "\n" );
		}
	}

}
